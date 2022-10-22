#include "type_system.h"

#include "generator_helpers.h"
#include "type_mapper.h"
#include "property_generator.h"

#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "core/string_builder.h"
#include "core/deque.h"
#include "core/hash_map.h"

#include "EASTL/fixed_hash_set.h"
#include "EASTL/deque.h"
#include "EASTL/algorithm.h"
#include "EASTL/sort.h"

#include <QDebug>
#include <QFile>

const TS_TypeLike *TS_TypeLike::common_base(const TS_TypeLike *with) const {

    const TS_TypeLike *lh = this;
    const TS_TypeLike *rh = with;
    if(!lh||!rh)
        return nullptr;

    //NOTE: this assumes that no type path will be longer than 16, should be enough though ?
    FixedVector<const TS_TypeLike *,16,false> lh_path;
    FixedVector<const TS_TypeLike *,16,false> rh_path;

    // collect paths to root for both types

    while(lh->nested_in) {
        lh_path.push_back(lh);
        lh=lh->nested_in;
    }
    while(rh->nested_in) {
        rh_path.push_back(rh);
        rh=rh->nested_in;
    }
    if(lh!=rh)
        return nullptr; // no common base

    auto rb_lh = lh_path.rbegin();
    auto rb_rh = rh_path.rbegin();

    // walk backwards on both paths
    while(rb_lh!=lh_path.rend() && rb_rh!=rh_path.rend()) {

        if(*rb_lh!=*rb_rh) {
            // encountered non-common type, take a step back and return
            --rb_lh;
            return *rb_lh;
        }
        ++rb_lh;
        ++rb_rh;
    }
    return nullptr;
}

TS_TypeLike *TS_TypeLike::find_by(eastl::function<bool (const TS_TypeLike *)> func) const {
    // check self first
    if(func(this))
        return const_cast<TS_TypeLike *>(this);

    // search through our children, then go to parent.
    auto iter = eastl::find_if(m_children.begin(),m_children.end(),func);
    if(iter!=m_children.end())
        return *iter;
    if(nested_in)
        return nested_in->find_by(func);
    return nullptr;
}

void TS_TypeLike::visit_kind(TS_TypeLike::TypeKind to_visit, eastl::function<void (const TS_TypeLike *)> visitor) const{
    for(const TS_TypeLike *tl : m_children) {
        if(tl->kind()==to_visit) {
            visitor(tl);
        }
    }
}

TS_TypeLike *TS_TypeLike::find_typelike_by_cpp_name(StringView name) const {
    return find_by([name](const TS_TypeLike* entry)->bool {
        return entry->c_name() == name;
    });
}

TS_Enum *TS_TypeLike::find_enum_by_cpp_name(StringView name) const {
    return (TS_Enum *)find_by([name](const TS_TypeLike *entry)->bool {
        if(entry->kind()!=ENUM) {
            return false;
        }
        return entry->c_name()==name;
    });
}

TS_Constant *TS_TypeLike::find_constant_by_cpp_name(StringView name) const
{
    TS_TypeLike * is_in=find_by([name](const TS_TypeLike *entry)->bool {
        if(entry->kind()==NAMESPACE)
            return false;
        for(auto prop : entry->m_constants) {
            if(prop->c_name()==name)
                return true;
        }
        return entry->c_name()==name;
    });
    if(!is_in)
        return nullptr;
    for(auto prop : is_in->m_constants) {
        if(prop->c_name()==name)
            return prop;
    }
    return nullptr;
}

TS_Type *TS_TypeLike::find_by_cs_name(const String &name) const {
    return (TS_Type *)find_by([&name](const TS_TypeLike *entry)->bool {
        if(entry->kind()!=CLASS) {
            return false;
        }
        return entry->cs_name()==name;
    });
}

TS_Type *TS_TypeLike::find_type_by_cpp_name(StringView name) const {
    return (TS_Type *)find_by([name](const TS_TypeLike *entry)->bool {
        if(entry->kind()!=CLASS) {
            return false;
        }
        return entry->c_name()==name;
    });
}

TS_Constant *TS_TypeLike::add_constant(const ConstantInterface *ci) {

    bool already_have_it = eastl::find_if(m_constants.begin(), m_constants.end(),
                                          [ci](const TS_Constant *a)
    {
        return a->m_rd_data == ci;
    }) != m_constants.end();
    assert(!already_have_it);

    TS_Constant *to_add = TS_Constant::get_instance_for(this, ci);
    to_add->enclosing_type = this;
    m_constants.emplace_back(to_add);
    return m_constants.back();
}
//!
//! \brief Returns the type access path relative to \a rel_to,
//! if rel_to is nullptr this will return full access path
//! \param tgt
//! \param rel_to
//! \return the string representing the type path
//!
String TS_TypeLike::relative_path(TargetCode tgt, const TS_TypeLike *rel_to) const {
    Dequeue<String> parts;
    eastl::fixed_hash_set<const TS_TypeLike*,32> rel_path;
    const TS_TypeLike* rel_iter=rel_to;
    while(rel_iter) {
        rel_path.insert(rel_iter);
        rel_iter = rel_iter->nested_in;
    }

    const TS_TypeLike* ns_iter = this;
    while (ns_iter && !rel_path.contains(ns_iter)) {
        parts.push_front(tgt==CPP_IMPL ? String(ns_iter->c_name()) : ns_iter->cs_name());
        //FIXME: this is a hack to handle Variant.Operator correctly.
        if(kind()==ENUM && ns_iter->c_name()=="Variant" && parts[0]!="Variant") {
            parts[0]="Variant";
        }
        ns_iter = ns_iter->nested_in;
    }
    return String::joined(parts, tgt==CPP_IMPL ? "::" : ".");
}

TS_Function *TS_TypeLike::find_method_by_name(TargetCode tgt, StringView name, bool try_parent) const {
    auto iter = eastl::find_if(m_functions.begin(), m_functions.end(), [name,tgt](const TS_Function* p) {
        if(tgt==CPP_IMPL)
            return p->source_type->name==name;
        return p->cs_name == name;
    });
    if(iter!=m_functions.end())
        return *iter;
    if(!try_parent)
        return nullptr;

    // retry in enclosing container
    return nested_in ? nested_in->find_method_by_name(tgt,name,try_parent) : nullptr;
}


HashMap<String,TS_Constant *> TS_Constant::constants;

String TS_Constant::fix_cs_name(StringView cpp_ns_name) {
    if (allUpperCase(cpp_ns_name))
        return snake_to_pascal_case(cpp_ns_name, true);
    return String(cpp_ns_name);
}

String TS_Constant::convert_name(StringView cpp_ns_name) {
    StringView to_convert = cpp_ns_name;
    FixedVector<StringView, 4, true> parts;
    FixedVector<StringView, 10, true> access_path_parts;
    String::split_ref(parts, cpp_ns_name, "::");
    return fix_cs_name(to_convert);
}

TS_Constant *TS_Constant::get_instance_for(const TS_TypeLike *tl, const ConstantInterface *src) {
    auto iter = constants.find(tl->relative_path(TargetCode::CS_INTERFACE)+"."+src->name);
    if(iter!=constants.end())
        return iter->second;
    auto res = new TS_Constant;
    res->m_rd_data = src;
    res->m_resolved_doc = tl->m_docs ? tl->m_docs->const_by_name(src->name.c_str()) : nullptr;

    res->cs_name = convert_name(src->name);
    if(!src->str_value.empty()) {
        res->value = src->str_value;
        res->const_type = {"String","",TypeRefKind::Simple};
    }
    else {
        char buf[32]={0};
        snprintf(buf,31,"%d",src->value);
        res->value = buf;
    }
    res->enclosing_type = tl;
    constants.emplace(tl->relative_path(TargetCode::CS_INTERFACE)+"."+res->cs_name,res);
    //assert(false);
    return res;
}

String TS_Constant::relative_path(TargetCode tgt, const TS_TypeLike *rel_to) const {
    const TS_TypeLike *common_base=enclosing_type->common_base(rel_to);
    return enclosing_type->relative_path(tgt,common_base) + (tgt==TargetCode::CPP_IMPL ? "::" : ".") + (tgt==TargetCode::CPP_IMPL ? m_rd_data->name : cs_name);
}

HashMap<String, TS_Module * >  TS_Module::s_modules;
String TS_Module::convert_ns_name(StringView cpp_ns_name) {
    return String(cpp_ns_name);
}
TS_Namespace *TS_Module::find_ns(StringView full_ns_name) {
    auto iter = m_namespaces.find_as(full_ns_name);
    if(iter!=m_namespaces.end())
        return iter->second;
    // search through our imports.
    for(auto imp : m_imports) {
        auto res = imp->find_ns(full_ns_name);
        if(res)
            return res;
    }
    return nullptr;
}

TS_Namespace *TS_Module::create_ns(const String & access_path, const NamespaceInterface &src)
{
    TS_Namespace *parent = nullptr;

    TS_Namespace * res = find_ns(access_path+src.name);
    if(res)
        return res;

    if(!access_path.empty()) {
        parent = find_ns(access_path.substr(0, access_path.size() - 2));
    }
    res=new TS_Namespace();
    res->m_source = &src;
    res->set_cs_name(convert_ns_name(src.name));
    m_namespaces[access_path+src.name] = res;
    res->nested_in = parent;
    if(parent)
        parent->m_children.push_back(res);
    return res;
}

TS_Module *TS_Module::find_module(StringView name)
{
    auto iter = s_modules.find_as(name);
    if(iter!=s_modules.end())
        return iter->second;
    return nullptr;
}

TS_Module *TS_Module::create_instance(const ReflectionData &src, bool is_imported)
{
    assert(nullptr==find_module(src.module_name));
    auto res=new TS_Module;
    res->m_source = &src;
    res->m_imported = is_imported;
    res->m_name = src.module_name;
    s_modules[src.module_name] = res;

    for(const auto &imp : src.imports) {
        TS_Module *import_ts = find_module(imp.module_name);
        if(import_ts==nullptr) {
            qCritical() << "One of module's imports has not been translated before we tried creating TS_Module:"
                        << imp.module_name.c_str();
            delete res;
            return nullptr;
        }
        res->m_imports.push_back(import_ts);
    }
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// TS_Namespace implementation.
///

TS_Type *TS_Namespace::find_or_create_by_cpp_name(const String &name)  {
    auto res = find_type_by_cpp_name(name);
    if(res)
        return res;
    TS_Namespace* ns_iter = this;
    const TypeInterface* target_itype;
    while(ns_iter) {
        target_itype = ns_iter->m_source->_get_type_or_null(TypeReference{ name });
        if(target_itype)
            break;
        ns_iter = (TS_Namespace*)ns_iter->nested_in;
    }
    if(target_itype==nullptr)
        return nullptr;
    m_children.push_back(TS_Type::create_type(this, target_itype));
    m_children.back()->m_imported = m_imported;
    return (TS_Type *)m_children.back();
}

Vector<StringView> TS_Namespace::cs_path_components() const {
    Dequeue<StringView> parts;
    const TS_TypeLike* ns_iter = this;
    while (ns_iter) {
        parts.push_front(ns_iter->cs_name());
        ns_iter = ns_iter->nested_in;
    }
    Vector<StringView> continous(parts.begin(),parts.end());
    return continous;
}

//TS_Namespace *TS_Namespace::get_instance_for(const String & access_path, const NamespaceInterface &src) {
//    auto iter = namespaces.find(access_path+src.namespace_name);
//    if(iter!=namespaces.end())
//        return iter->second;
//    TS_Namespace *parent = nullptr;
//    if(!access_path.empty()) {
//        auto parent_iter = namespaces.find(access_path.substr(0, access_path.size() - 2));
//        parent = parent_iter->second;
//    }
//    auto res=new TS_Namespace();
//    res->m_source = &src;
//    res->set_cs_name(convert_ns_name(src.namespace_name));
//    namespaces[access_path+src.namespace_name] = res;
//    res->parent = parent;
//    if(parent)
//        parent->m_children.push_back(res);
//    return res;
//}


//TS_Namespace *TS_Namespace::from_path(Span<const StringView> path) {
//    String to_test = String::joined(path,"::");
//    auto iter = namespaces.find(to_test);
//    assert(iter!=namespaces.end());
//    return iter->second;
//}

HashMap<String,TS_Enum *> TS_Enum::enums;
TS_Enum *TS_Enum::get_instance_for(const TS_TypeLike *enclosing, const String &access_path, const EnumInterface *src) {
    auto iter = enums.find(access_path+src->cname);
    if(iter!=enums.end())
        return iter->second;
    auto res = new TS_Enum;
    res->m_rd_data = src;
    res->nested_in = enclosing;
    String cs_name = convert_name(access_path, src->cname);
    if(enclosing->enum_name_would_clash_with_property(cs_name)) {
        cs_name += "Enum";
    }
    res->underlying_val_type = TS_TypeResolver::get().resolveType(src->underlying_type,{});

    res->set_cs_name(cs_name);
    enums.emplace(access_path+res->cs_name(),res);
    //assert(false);
    return res;
}
String TS_Enum::convert_name(const String &access_path, StringView cpp_ns_name) {
    FixedVector<StringView, 4, true> parts;
    FixedVector<StringView, 10, true> access_path_parts;
    String::split_ref(parts, cpp_ns_name, "::");
    if (parts.size() == 1)
        return String(cpp_ns_name);
    String::split_ref(access_path_parts, cpp_ns_name, "::");
    if (parts.size() > 1 && parts.front() == access_path_parts.back()) {
        return String(parts[1]);
    }
    if (parts.size() == 2) {
        // NOTE: this assumes that handling of nested enum names is done outside.
        return String(parts[1]);
    }
    return String(cpp_ns_name);
}

HashMap< const TypeInterface*, TS_Type*> TS_Type::s_ptr_cache;
TS_Type *TS_Type::create_type(const TS_TypeLike *owning_type, const TypeInterface *type_interface) {
    TS_Type* res = s_ptr_cache[type_interface];
    if (res) {
        assert(res->nested_in==owning_type);
        return res;
    }

    res=new TS_Type;
    res->set_cs_name(convert_name(type_interface->name));
    //TODO: remove this special processing of StringView and StringNames.
    if(type_interface->name=="StringView") {
        res->set_cs_name("string");
    } else if(type_interface->name=="StringName") {
        res->set_cs_name("string");
    }
    res->nested_in = owning_type;
    res->source_type = type_interface;
    s_ptr_cache[type_interface] = res;
    return res;
}

TS_Property *TS_Type::find_property_by_name(StringView name) const {
    String csname(TS_TypeMapper::get().mapPropertyName(name));
    auto iter = eastl::find_if(m_properties.begin(), m_properties.end(),[csname](const TS_Property *p) {
        return p->cs_name==csname;
    });
    if(iter!=m_properties.end())
        return *iter;
    if(iter==m_properties.end()) { // try to search for non-converted name in indexed parts.
        for(auto prop : m_properties) {
            for(const auto &sub : prop->indexed_entries) {
                //TODO: this might fail!
                // property name in docs might be prefixed by a groupname
                if(name.ends_with(sub.subfield_name))
                    return prop;
            }
        }

    }
    return nullptr;
}
TS_Property *TS_Type::find_property_by_exact_name(StringView name) const {
    String csname(TS_TypeMapper::get().mapPropertyName(name));
    auto iter = eastl::find_if(m_properties.begin(), m_properties.end(),[csname](const TS_Property *p) {
        return p->cs_name==csname;
    });

    if(iter!=m_properties.end()) {
        return *iter;
    }

    if(iter==m_properties.end()) { // try to search for non-converted name in indexed parts.
        for(auto prop : m_properties) {
            for(const auto &sub : prop->indexed_entries) {
                //TODO: this might fail!
                // property name in docs might be prefixed by a groupname
                if(name==sub.subfield_name)
                    return prop;
            }
        }

    }
    return nullptr;
}
String TS_Type::get_property_path_by_func(const TS_Function *f) const
{
    String res;
    for(auto prop : m_properties) {
        for(const auto &sub : prop->indexed_entries) {
            //TODO: this might fail!
            // property name in docs might be prefixed by a groupname
            if(sub.getter==f || sub.setter==f) {
                String res = get_property_typename(*prop);
                if(!sub.subfield_name.empty())
                    res +=  "." + c_property_name_to_cs(sub.subfield_name);
                return res;
            }
        }
    }
    return "";
}

TS_TypeLike *TS_Type::find_by(eastl::function<bool (const TS_TypeLike *)> func) const {
    if(base_type) {
        auto res=base_type->find_by(func);
        if(res)
            return res;
    }
    // use base-class version:
    return TS_TypeLike::find_by(func);
}

TS_Function *TS_Type::find_method_by_name(TargetCode tgt, StringView name, bool try_parent) const {
    //follows the C++ logic first try this class, then the base classes, then enclosing namespace route

    //Try to find in 'self' and base classes
    const TS_TypeLike *current=this;
    while(current) {
        TS_Function *res = current->TS_TypeLike::find_method_by_name(tgt,name,false); // call base version on current.
        if(res)
            return res;
        current = current->base_type;
    }

    return nested_in ? nested_in->TS_TypeLike::find_method_by_name(tgt,name,true) : nullptr;
}

bool TS_Type::enum_name_would_clash_with_property(StringView cs_enum_name) const {
    for (const PropertyInterface& prop : this->source_type->properties) {
        String conv_name = escape_csharp_keyword(snake_to_pascal_case(prop.cname));
        if(conv_name==cs_enum_name)
            return true;
    }
    return false;
}
HashMap< const SignalInterface*, TS_Signal*> TS_Signal::s_ptr_cache;
TS_Signal *TS_Signal::from_rd(const TS_Type *inside, const SignalInterface *method_interface) {

    TS_Signal* res = s_ptr_cache[method_interface];
    if(res)
        return res;

    res = new TS_Signal;
    res->cs_name = TS_Function::mapMethodName(method_interface->name,inside ? inside->cs_name() : "");
    if(inside->find_property_by_exact_name(res->cs_name) || inside->find_method_by_name(TargetCode::CS_INTERFACE,res->cs_name,true)) {
        res->cs_name += "Signal";
    }
    res->source_type = method_interface;
    res->enclosing_type = inside;

    for(const ArgumentInterface & ai : method_interface->arguments) {
        res->arg_types.emplace_back(TS_TypeResolver::get().resolveType(ai.type));
        res->arg_values.emplace_back(escape_csharp_keyword(ai.name));
        res->nullable_ref.emplace_back(ai.def_param_mode!= ArgumentInterface::CONSTANT);
        if(!ai.default_argument.empty()) {
            res->arg_defaults[res->arg_values.size()-1] = ai.default_argument;
        }
    }
    s_ptr_cache[method_interface] = res;
    return res;
}


HashMap< const MethodInterface*, TS_Function*> TS_Function::s_ptr_cache;
String TS_Function::mapMethodName(StringView method_name, StringView class_name, StringView namespace_name) {
    String proxy_name(escape_csharp_keyword(snake_to_pascal_case(method_name)));
    String mapped_class_name = TS_Type::convert_name(class_name);

    // Prevent the method and its enclosing type from sharing the same name
    if ((!class_name.empty() && proxy_name == mapped_class_name) || (!namespace_name.empty() && proxy_name==namespace_name)) {
        qWarning("Name of method '%s' is ambiguous with the name of its enclosing class '%s'. Renaming method to '%s_'\n",
                 proxy_name.c_str(), mapped_class_name.c_str(), String(method_name).c_str());

        proxy_name += "_";
    }
    return proxy_name;
}

TS_Function *TS_Function::from_rd(const TS_TypeLike *inside, const MethodInterface *method_interface) {

    TS_Function* res = s_ptr_cache[method_interface];
    if(res)
        return res;

    res = new TS_Function;
    res->cs_name = mapMethodName(method_interface->name,inside ? inside->cs_name() : "");
    res->source_type = method_interface;
    res->enclosing_type = inside;

    res->return_type = TS_TypeResolver::get().resolveType(method_interface->return_type);
    int arg_idx=0;
    for(const ArgumentInterface & ai : method_interface->arguments) {
        res->arg_types.emplace_back(TS_TypeResolver::get().resolveType(ai.type));
        auto arg_name = ai.name;
        if(arg_name.empty()) {
            arg_name = String(String::CtorSprintf(),"arg%d",arg_idx);
        }
        res->arg_values.emplace_back(escape_csharp_keyword(arg_name));
        res->nullable_ref.emplace_back(ai.def_param_mode!= ArgumentInterface::CONSTANT);
        if(!ai.default_argument.empty()) {
            res->arg_defaults[res->arg_values.size()-1] = ai.default_argument;
        }
        arg_idx++;
    }
    s_ptr_cache[method_interface] = res;
    return res;
}

HashMap< const PropertyInterface*, TS_Property *> TS_Property::s_ptr_cache;
TS_Property *TS_Property::from_rd(const TS_Type *owner, const PropertyInterface *type_interface) {
    TS_Property *res = s_ptr_cache[type_interface];
    if (res)
        return res;

    res = new TS_Property;
    assert(owner && owner->nested_in);
    res->m_owner = owner;

    if(owner) {
        res->cs_name = TS_TypeMapper::get().mapPropertyName(type_interface->cname,owner->cs_name(),owner->nested_in->cs_name());
    }
    else {
        res->cs_name = TS_TypeMapper::get().mapPropertyName(type_interface->cname,"","");
    }
    if(owner->find_method_by_name(TargetCode::CS_INTERFACE,res->cs_name,true)) {
        res->cs_name = res->cs_name+"_";
    }
    res->source_type = type_interface;
    s_ptr_cache[type_interface] = res;
    return res;
}

String ResolvedTypeReference::to_c_type(const TS_TypeLike *base_ns) const {
    if(!type)
        return "null_t";
    String fulltypepath(type->relative_path(TargetCode::CPP_IMPL,base_ns));
    switch(pass_by) {
        case TypePassBy::Value:
            return fulltypepath;
            break;
    case TypePassBy::Pointer:
        //FIXME: this is a hackaround the fact that we register `Object *` as a primitive type
        if(type->c_name().ends_with('*'))
            return fulltypepath;
        return fulltypepath + " *";
        break;
    case TypePassBy::ConstPointer:
        return "const " +fulltypepath + " *";
        break;
    case TypePassBy::Move:
        return fulltypepath + " &&";
        break;
    case TypePassBy::Reference:
        return fulltypepath + " &";
        break;
    case TypePassBy::ConstReference:
        return "const "+fulltypepath + " &";
        break;
    case TypePassBy::RefValue:
        return "Ref<"+fulltypepath + ">";
        break;
    case TypePassBy::ConstRefReference:
        return "const Ref<"+ fulltypepath + "> &";
        break;
    }
    return "UnknC";
}

