#include "type_registration_pass.h"

#include "type_system.h"
#include "type_mapper.h"
#include "generator_helpers.h"

#include "core/string.h"
#include "core/string_builder.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"

#include <QDebug>

using namespace eastl::string_literals;

static int _determine_enum_prefix(const TS_Enum &p_ienum) {
    assert(!p_ienum.m_constants.empty());

    const TS_Constant *front_iconstant = p_ienum.m_constants.front();
    auto front_parts = front_iconstant->m_rd_data->name.split('_', /* p_allow_empty: */ true);
    size_t candidate_len = front_parts.size() - 1;

    if (candidate_len == 0) {
        return 0;
    }

    for (const TS_Constant *iconstant : p_ienum.m_constants) {
        auto parts = iconstant->m_rd_data->name.split('_', /* p_allow_empty: */ true);

        size_t i;
        for (i = 0; i < candidate_len && i < parts.size(); i++) {
            if (front_parts[i] != parts[i]) {
                // HARDCODED: Some Flag enums have the prefix 'FLAG_' for everything except 'FLAGS_DEFAULT' (same
                // for 'METHOD_FLAG_' and'METHOD_FLAGS_DEFAULT').
                bool hardcoded_exc =
                        (i == candidate_len - 1 && ((front_parts[i] == "FLAGS" && parts[i] == "FLAG") ||
                                                           (front_parts[i] == "FLAG" && parts[i] == "FLAGS")));
                if (!hardcoded_exc) {
                    break;
                }
            }
        }
        candidate_len = i;

        if (candidate_len == 0) {
            return 0;
        }
    }

    return candidate_len;
}

bool TypeRegistrationPass::covariantSetterGetterTypes(StringView getter, StringView setter) {
    using namespace eastl;
    if (getter == setter) {
        return true;
    }
    bool getter_stringy_type =
            (getter == "String"_sv) || (getter == "StringName"_sv) || (getter == "StringView"_sv);
    bool setter_stringy_type =
            (setter == "String"_sv) || (setter == "StringName"_sv) || (setter == "StringView"_sv);
    return getter_stringy_type == setter_stringy_type;
}


static void _apply_prefix_to_enum_constants(const TS_Enum &p_ienum, int p_prefix_length) {

    if (p_prefix_length <= 0) {
        return;
    }

    for (TS_Constant *curr_const : p_ienum.m_constants) {
        int curr_prefix_length = p_prefix_length;

        String constant_name = curr_const->m_rd_data->name;

        auto parts = constant_name.split('_', /* p_allow_empty: */ true);

        if (parts.size() <= curr_prefix_length) {
            continue;
        }

        if (parts[curr_prefix_length][0] >= '0' && parts[curr_prefix_length][0] <= '9') {
            // The name of enum constants may begin with a numeric digit when strip from the enum prefix,
            // so we make the prefix for this constant one word shorter in those cases.
            for (curr_prefix_length = curr_prefix_length - 1; curr_prefix_length > 0; curr_prefix_length--) {
                if (parts[curr_prefix_length][0] < '0' || parts[curr_prefix_length][0] > '9') break;
            }
        }

        constant_name = "";
        for (int i = curr_prefix_length; i < parts.size(); i++) {
            if (i > curr_prefix_length) constant_name += "_";
            constant_name += parts[i];
        }

        curr_const->cs_name = snake_to_pascal_case(constant_name, true);
    }
}

void TypeRegistrationPass::visitConstant(const ConstantInterface *ci) {
    // A few cases:
    // We're in namespace, create Constants class, add entry for the constant

    // In class add entry for the constants
    // In enum add entry for the constant
    if (m_current_enum) {
        m_current_enum->add_constant(ci)->m_imported = m_currently_visiting_imported;
    } else if (m_type_stack.empty()) {
        assert(!m_namespace_stack.empty());
        m_namespace_stack.back()->add_constant(ci)->m_imported = m_currently_visiting_imported;
    } else {
        m_type_stack.back()->add_constant(ci)->m_imported = m_currently_visiting_imported;
    }
}

void TypeRegistrationPass::visitEnum(const EnumInterface *ei) {
    // Two cases, in namespace, in class
    TS_TypeLike *parent = m_type_stack.empty() ? nullptr : m_type_stack.back();
    assert(!m_namespace_stack.empty());
    parent = parent ? parent : m_namespace_stack.back();

    StringView enum_c_name(ei->cname);
    StringView static_wrapper_class;
    bool add_to_ns_too = false;
    if (enum_c_name.contains("::")) {
        Vector<StringView> parts;
        String::split_ref(parts, enum_c_name, "::");
        static_wrapper_class = parts.front();
        enum_c_name = parts[1];
        assert(static_wrapper_class == StringView("Variant")); // Hard-coded...
        // make sure the static_wrapper_class is visited beforehand!
        // an enum that belongs to a synthetic type
        auto enum_parent = TS_TypeResolver::get().resolveType(
                    TypeReference{ String(static_wrapper_class),"",TypeRefKind::Simple, TypePassBy::Value },
                    parent);

        qDebug("Declaring global enum '%.*s' inside static class '%.*s'\n", int(enum_c_name.size()),
               enum_c_name.data(), int(static_wrapper_class.size()), static_wrapper_class.data());
        parent = const_cast<TS_TypeLike *>(enum_parent.type);
        assert(parent);
        add_to_ns_too = true;
    }

    TS_Enum *en = TS_Enum::get_instance_for(parent, current_access_path(), ei);
    en->static_wrapper_class = static_wrapper_class;
    en->m_imported = m_currently_visiting_imported;

    register_enum(m_namespace_stack.back(), parent, en);
    m_current_enum = en;
    parent->add_enum(en);
    if (add_to_ns_too) // need this hack to actually output the enum in cs file if the enum is bound to
        // static_wrapper_class
        m_namespace_stack.back()->add_enum(en);

    for (const auto &ci : ei->constants) {
        visitConstant(&ci);
    }

    m_current_enum = nullptr;

    int prefix_length=1;
    if(StringView("Error")!=en->c_name()) {
        prefix_length = _determine_enum_prefix(*en);
    }

    _apply_prefix_to_enum_constants(*en, prefix_length);
}

void TypeRegistrationPass::visitMethodInterface(const MethodInterface *fi) {
    TS_TypeLike *tgt;
    if (m_type_stack.empty()) {
        assert(!m_namespace_stack.empty());
        tgt = m_namespace_stack.back();
    } else {
        tgt = m_type_stack.back();
    }
    TS_Function *func = TS_Function::from_rd(tgt, fi);
    if(func->m_imported) // available in imported partial class
        return;
    func->m_imported = m_currently_visiting_imported;

    tgt->m_functions.emplace_back(func);

    //  assert(false);
}

void TypeRegistrationPass::visitSignalInterface(const SignalInterface *fi) {
    TS_TypeLike *tgt;
    if (m_type_stack.empty()) {
        assert(!m_namespace_stack.empty());
        tgt = m_namespace_stack.back();
    } else {
        tgt = m_type_stack.back();
    }
    assert(tgt->kind()==TS_TypeLike::CLASS);
    TS_Signal *sig = TS_Signal::from_rd((TS_Type *)tgt, fi);
    if(sig->m_imported) // available in imported partial class
        return;
    sig->m_imported = m_currently_visiting_imported;

    tgt->m_signals.emplace_back(sig);

    //  assert(false);
}

void TypeRegistrationPass::visitTypeProperty(const PropertyInterface *pi) {
    // TODO: mark both setter and getter as do-not-generate

    TS_Type *curr_type = m_type_stack.back();

    TS_Property *prop = TS_Property::from_rd(m_type_stack.back(), pi);

    prop->m_imported = m_currently_visiting_imported;

    curr_type->m_properties.emplace_back(prop);

    for (const auto &val : pi->indexed_entries) {
        TS_Property::ResolvedPropertyEntry conv;
        TypeReference set_get_type;
        if (!val.setter.empty()) {
            String mapped_setter_name = TS_Function::mapMethodName(val.setter, curr_type->cs_name());
            conv.setter = curr_type->find_method_by_name(CS_INTERFACE, mapped_setter_name, true);
            if (conv.setter) {
                set_get_type = conv.setter->source_type->arguments.back().type;
            }
        }
        if (!val.getter.empty()) {
            String mapped_getter_name = TS_Function::mapMethodName(val.getter, curr_type->cs_name());
            conv.getter = curr_type->find_method_by_name(CS_INTERFACE, mapped_getter_name, true);
            assert(conv.getter);
            if (conv.getter && set_get_type.cname.empty())
                set_get_type = conv.getter->source_type->return_type;
        }
        if (!conv.setter && !conv.getter) {
            qCritical() << "Failed to get setter or getter for property" << prop->cs_name.c_str() << " in class "
                        << curr_type->cs_name().c_str();
            return;
        }
        if (conv.setter) {
            size_t setter_argc = (pi->max_property_index > 0 || val.index == -2 || val.index >= 0) ? 2 : 1;
            if (conv.setter->source_type->arguments.size() != setter_argc) {
                qCritical() << "Setter function " << conv.setter->cs_name.c_str()
                            << "has incorrect number of arguments in class " << curr_type->cs_name().c_str();
                return;
            }
        }
        if (conv.getter) {
            size_t getter_argc = (pi->max_property_index > 0 || val.index == -2 || val.index >= 0) ? 1 : 0;
            if (conv.getter->source_type->arguments.size() != getter_argc) {
                qCritical() << "Getter function " << conv.getter->cs_name.c_str()
                            << "has incorrect number of arguments in class " << curr_type->cs_name().c_str();
                return;
            }
        }
        if (conv.getter && conv.setter) {
            if (unlikely(!covariantSetterGetterTypes(conv.getter->source_type->return_type.cname,
                                                     conv.setter->source_type->arguments.back().type.cname))) {
                qCritical() << "Getter and setter types are not covariant for property" << prop->cs_name.c_str()
                            << " in class " << curr_type->cs_name().c_str();
                return;
            }
        }

        conv.index = val.index;
        conv.subfield_name = val.subfield_name;
        Vector<StringView> allowed_types;
        StringView hint_string(val.entry_type.cname);
        if (val.entry_type.cname.starts_with("PH:")) {
            hint_string = hint_string.substr(3);
            auto base_entry = val.entry_type;
            // TODO: this is only used in one single property in the engine ( RichTextLabel custom_effects ),
            // eliminate it?
            if (hint_string.contains('/')) // Number/Number:Type  for semi-generic Arrays
            {
                // the encoded hint string is "PropertyHint/VariantType:subtype_hint_string"
                hint_string = "Array";
            }
            // can contain
            String::split_ref(allowed_types, hint_string, ",");
            for (StringView sub : allowed_types) {
                auto copy_entry = base_entry;
                copy_entry.cname = sub;
                ResolvedTypeReference entry_type = TS_TypeResolver::get().resolveType(copy_entry);
                conv.entry_type.emplace_back(eastl::move(entry_type));
            }
        } else {
            String::split_ref(allowed_types, hint_string, ",");
            if(allowed_types.size()==1) {
                ResolvedTypeReference entry_type = TS_TypeResolver::get().resolveType(set_get_type);
                if(!entry_type.type) {
                    auto copy_entry = val.entry_type;
                    copy_entry.cname = allowed_types.front();
                    entry_type = TS_TypeResolver::get().resolveType(copy_entry);
                }
                conv.entry_type.emplace_back(eastl::move(entry_type));
            }
            else {
                for (StringView sub : allowed_types) {
                    auto copy_entry = val.entry_type;
                    copy_entry.cname = sub;
                    ResolvedTypeReference entry_type = TS_TypeResolver::get().resolveType(copy_entry);
                    conv.entry_type.emplace_back(eastl::move(entry_type));
                }
            }
        }
        prop->indexed_entries.emplace_back(eastl::move(conv));
    }
}

void TypeRegistrationPass::registerTypesPass(const TypeInterface *ti) {
    TS_Type *type = TS_Type::by_rd(ti);

    if (type->pass > 0)
        return;

    type->base_type = m_namespace_stack.back()->find_or_create_by_cpp_name(ti->base_name);
    if (type->base_type && ((const TS_Type *)type->base_type)->pass == 0) {
        // process base type first.
        registerTypesPass(((const TS_Type *)type->base_type)->source_type);
    }

    m_type_stack.push_back(type);
    type->m_imported = m_currently_visiting_imported;

    if(type->m_imported && !m_currently_visiting_imported) {
        type->m_skip_special_functions = true;
    }

    TS_TypeMapper::get().register_complex_type(type);
    type->pass = 1;

    for (const auto &ci : ti->constants) {
        visitConstant(&ci);
    }

    for (const auto &ei : ti->enums) {
        visitEnum(&ei);
    }

    // TODO: handle nested classes.

    m_type_stack.pop_back();
}

void TypeRegistrationPass::registerTypeDetails(const TS_Type *type) {
    assert(type->pass > 0);
    if(type->pass==2) {
        return;
    }
    if (type->base_type && ((const TS_Type *)type->base_type)->pass == 1) {
        // process base type first.
        registerTypeDetails((const TS_Type *)type->base_type);
    }

    m_type_stack.push_back((TS_Type *)type);

    type->pass=2;
    // Properties use class methods for setters/getters, so we visit methods first.
    for (const MethodInterface &mi : type->source_type->methods) {
        visitMethodInterface(&mi);
    }
    for (const PropertyInterface &pi : type->source_type->properties) {
        visitTypeProperty(&pi);
    }

    for (const SignalInterface &mi : type->source_type->signals_) {
        visitSignalInterface(&mi);
    }
    m_type_stack.pop_back();
}

void TypeRegistrationPass::visitModule(const ReflectionData *rd, bool imported) {
    auto module = TS_Module::find_module(rd->module_name);
    if(module) {
        return; // module was visited already, nothing to do.
    }
    m_current_module = TS_Module::create_instance(*rd,imported);
    m_currently_visiting_imported = imported;
    for(const auto &ns : rd->namespaces) {
        visitNamespace(ns);
    }
    auto mod_ns = m_current_module->find_ns(rd->module_name+"MetaData");
    if(!mod_ns) {
        auto metadata_ns = new NamespaceInterface;
        metadata_ns->name = rd->module_name+"MetaData";
        metadata_ns->global_constants.emplace_back("api_hash",rd->api_hash);
        metadata_ns->global_constants.emplace_back("api_version",rd->api_version);
        metadata_ns->global_constants.emplace_back("version",rd->version);
        m_current_module->create_ns("",*metadata_ns);
        visitNamespace(*metadata_ns);
    }

}

void TypeRegistrationPass::visitNamespace(const NamespaceInterface &iface) {
    auto ns = m_current_module->find_ns(current_access_path()+iface.name);
    if(!ns) { // namespace is not available yet, so it must be a new one.
        ns = m_current_module->create_ns(current_access_path(), iface);
    }
    else {
        // namespace is available in another module ?
        if(m_current_module->m_namespaces.find_as(ns->c_name())==m_current_module->m_namespaces.end()) {
            // make it available in this module as well.
            m_current_module->m_namespaces.emplace(String(ns->c_name()),ns);
        }
    }
    // Overwrite the imported flag, if we're in non-imported module,
    // marks the given namespace as 'interesting' to generators etc.

    ns->m_imported = m_currently_visiting_imported;
    m_namespace_stack.push_back(ns);
    // current module can override the docs
    if(m_current_module->m_source->doc)
        ns->m_docs = &m_current_module->m_source->doc->class_doc("@GlobalScope");

    //Vector<TS_Type *> registered_obj_types;

    // Register all types in CSType lookup hash
    for (const auto &ci : iface.obj_types) {
        TS_Type *type = TS_Type::create_type(ns, &ci.second);
        ns->m_children.emplace_back(type);
    }

    for (const auto &ci : iface.obj_types) {
        registerTypesPass(&ci.second);
    }

    // TODO: handle namespace docs stuff in the future
    for (const ConstantInterface &ci : iface.global_constants) {
        visitConstant(&ci);
    }
    for (const EnumInterface &ci : iface.global_enums) {
        visitEnum(&ci);
    }

    for (auto ci : ns->m_children) {
        if (ci->kind() == TS_TypeLike::CLASS) {
            registerTypeDetails((TS_Type *)ci);
        }
    }
    for (const auto &ci : iface.global_functions) {
        visitMethodInterface(&ci);
    }
    leaveNamespace();
}

void TypeRegistrationPass::finalize() {
    auto obj_type = TS_TypeResolver::get().resolveType("Object", "Godot");
    obj_type.pass_by = TypePassBy::Pointer;
    TS_TypeMapper::get().registerTypeMaps(
                obj_type, {
                    { TS_TypeMapper::CPP_TO_WRAP_TYPE, "Object *" },
                    { TS_TypeMapper::CPP_TO_WRAP_TYPE_OUT, "MonoObject *" },
                    { TS_TypeMapper::WRAP_TO_CPP_IN_ARG, "AutoRef(%input%)" },
                    { TS_TypeMapper::SCRIPT_TO_WRAP_IN, "Object.GetPtr(%input%)" },
                });
}

void TypeRegistrationPass::visit(const ReflectionData *refl) {
    do_visit_recursive(refl,false,&m_ctx.m_rd!=refl);
}
