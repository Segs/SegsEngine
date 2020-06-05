#include "reflection_generator.h"


#include "reflection_data.h"
#include "core/variant.h"
#include "core/engine.h"
#include "core/string_utils.inl"
#include "core/math/transform.h"
#include "core/class_db.h"
#include "core/global_constants.h"
#include "core/os/os.h"
#include "core/method_bind.h"
#include "core/script_language.h"

#include "EASTL/sort.h"
#include <QString>
#include <QtCore/QXmlStreamReader>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

static bool s_log_print_enabled = true;
static void _log(const char* p_format, ...) {

    if (s_log_print_enabled) {
        va_list list;

        va_start(list, p_format);
        OS::get_singleton()->print(String().append_sprintf_va_list(p_format, list).c_str());
        va_end(list);
    }
}


//Take the running program state, and generate the reflection json file.
bool _arg_default_value_from_variant(const Variant& p_val, ArgumentInterface& r_iarg) {

    r_iarg.default_argument = p_val.as<String>().data();

    switch (p_val.get_type()) {
    case VariantType::NIL:
        // Either Object type or Variant
        r_iarg.default_argument = "null";
        break;
        // Atomic types
    case VariantType::BOOL: r_iarg.default_argument = bool(p_val) ? "true" : "false";
        break;
    case VariantType::INT: if (r_iarg.type.cname != "int") {
        r_iarg.default_argument = "(%s)" + r_iarg.default_argument;
    }
                         break;
    case VariantType::FLOAT:
#ifndef REAL_T_IS_DOUBLE
        r_iarg.default_argument += "f";
#endif
        break;
    case VariantType::STRING:
    case VariantType::NODE_PATH:
        r_iarg.default_argument = "\"" + r_iarg.default_argument + "\"";
        break;
    case VariantType::TRANSFORM:
        if (p_val.as<Transform>() == Transform())
            r_iarg.default_argument.clear();
        r_iarg.default_argument = "new %s(" + r_iarg.default_argument + ")";
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        break;
    case VariantType::PLANE:
    case VariantType::AABB:
    case VariantType::COLOR: r_iarg.default_argument = "new Color(1, 1, 1, 1)";
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        break;
    case VariantType::VECTOR2:
    case VariantType::RECT2:
    case VariantType::VECTOR3: r_iarg.default_argument = "new %s" + r_iarg.default_argument;
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        break;
    case VariantType::OBJECT:
        ERR_FAIL_COND_V_MSG(!p_val.is_zero(), false,
            "Parameter of type '" + r_iarg.type.cname +
            "' can only have null/zero as the default value.");

        r_iarg.default_argument = "null";
        break;
    case VariantType::DICTIONARY: r_iarg.default_argument = "new %s()";
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_REF;
        break;
    case VariantType::_RID:
        ERR_FAIL_COND_V_MSG(r_iarg.type.cname != "RID", false,
            "Parameter of type '" + (r_iarg.type.cname) + "' cannot have a default value of type 'RID'.");

        ERR_FAIL_COND_V_MSG(!p_val.is_zero(), false,
            "Parameter of type '" + (r_iarg.type.cname) +
            "' can only have null/zero as the default value.");

        r_iarg.default_argument = "null";
        break;
    case VariantType::ARRAY:
    case VariantType::POOL_BYTE_ARRAY:
    case VariantType::POOL_INT_ARRAY:
    case VariantType::POOL_REAL_ARRAY:
    case VariantType::POOL_STRING_ARRAY:
    case VariantType::POOL_VECTOR2_ARRAY:
    case VariantType::POOL_VECTOR3_ARRAY:
    case VariantType::POOL_COLOR_ARRAY: r_iarg.default_argument = "new %s {}";
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_REF;
        break;
    case VariantType::TRANSFORM2D:
    case VariantType::BASIS:
    case VariantType::QUAT:
        r_iarg.default_argument = String(Variant::get_type_name(p_val.get_type())) + ".Identity";
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        break;
    default: {
    }
    }

    if (r_iarg.def_param_mode == ArgumentInterface::CONSTANT && r_iarg.type.cname == "Variant" && r_iarg.default_argument != "null")
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_REF;

    return true;
}
static APIType convertApiType(ClassDB::APIType ap) {
    if(ap==ClassDB::API_NONE)
        return APIType::Invalid;
    if (ap == ClassDB::API_CORE)
        return APIType::Common;
    if (ap == ClassDB::API_EDITOR)
        return APIType::Editor;
    if (ap == ClassDB::API_SERVER)
        return APIType::Server;
    return APIType::Invalid;

}
static StringView _get_int_type_name_from_meta(GodotTypeInfo::Metadata p_meta) {

    switch (p_meta) {
    case GodotTypeInfo::METADATA_INT_IS_INT8:
        return "int8_t";
    case GodotTypeInfo::METADATA_INT_IS_INT16:
        return "int16_t";
    case GodotTypeInfo::METADATA_INT_IS_INT32:
        return "int32_t";
    case GodotTypeInfo::METADATA_INT_IS_INT64:
        return "int64_t";
    case GodotTypeInfo::METADATA_INT_IS_UINT8:
        return "uint8_t";
    case GodotTypeInfo::METADATA_INT_IS_UINT16:
        return "uint16_t";
    case GodotTypeInfo::METADATA_INT_IS_UINT32:
        return "uint32_t";
    case GodotTypeInfo::METADATA_INT_IS_UINT64:
        return "uint64_t";
    default:
        // Assume INT32
        return "int32_t";
    }
}
static StringView _get_float_type_name_from_meta(GodotTypeInfo::Metadata p_meta) {

    switch (p_meta) {
    case GodotTypeInfo::METADATA_REAL_IS_FLOAT:
        return "float";
    case GodotTypeInfo::METADATA_REAL_IS_DOUBLE:
        return "double";
    default:
        // Assume real_t (float or double depending of REAL_T_IS_DOUBLE)
#ifdef REAL_T_IS_DOUBLE
        return "double";
#else
        return "float";
#endif
    }
}
static StringView _get_string_type_name_from_meta(GodotTypeInfo::Metadata p_meta) {

    switch (p_meta) {
    case GodotTypeInfo::METADATA_STRING_NAME:
        return "StringName";
    case GodotTypeInfo::METADATA_STRING_VIEW:
        return "StringView";
    default:
        // Assume default String type
        return "String";
    }
}

static StringView _get_variant_type_name_from_meta(VariantType tp, GodotTypeInfo::Metadata p_meta) {
    if (GodotTypeInfo::METADATA_NON_COW_CONTAINER == p_meta) {
        switch (tp) {

        case VariantType::POOL_BYTE_ARRAY:
            return StringName("PoolByteArray");

        case VariantType::POOL_INT_ARRAY:
            return StringName("PoolIntArray");
        case VariantType::POOL_REAL_ARRAY:
            return StringName("PoolRealArray");
        case VariantType::POOL_STRING_ARRAY:
            return StringName("PoolStringArray");
        case VariantType::POOL_VECTOR2_ARRAY:
            return StringName("PoolVector2Array");
        case VariantType::POOL_VECTOR3_ARRAY:
            return StringName("PoolVector3Array");
        case VariantType::POOL_COLOR_ARRAY:
            return StringName("PoolColorArray");
        default:;
        }
    }
    return Variant::interned_type_name(tp);
}
static void fill_type_info(const PropertyInfo &arginfo,TypeReference &tgt) {
    if (arginfo.type == VariantType::INT && arginfo.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
        tgt.cname = arginfo.class_name.asCString();
        tgt.is_enum = true;
        tgt.pass_by = TypePassBy::Value;
    }
    else if (!arginfo.class_name.empty()) {
        tgt.cname = arginfo.class_name.asCString();
        tgt.pass_by = TypePassBy::Reference;
    }
    else if (arginfo.hint == PropertyHint::ResourceType) {
        tgt.cname = arginfo.hint_string.c_str();
        tgt.pass_by = TypePassBy::Reference;
    }
    else if (arginfo.type == VariantType::NIL) {
        tgt.cname = "Variant";
        tgt.pass_by = TypePassBy::Value;
    }
    else {
        if (arginfo.type == VariantType::INT) {
            tgt.cname = "int";
        }
        else if (arginfo.type == VariantType::FLOAT) {
            tgt.cname = "float";
        }
        else if (arginfo.type == VariantType::STRING) {
            tgt.cname = "String";
        }
        else {

            tgt.cname = _get_variant_type_name_from_meta(arginfo.type, GodotTypeInfo::METADATA_NONE).data();
        }
        tgt.pass_by = TypePassBy::Value;
    }
    if (tgt.cname == "Object" && tgt.pass_by == TypePassBy::Value) {
        // Fixup for virtual methods, since passing Object by value makes no sense.
        tgt.pass_by = TypePassBy::Pointer;
    }
}
static bool _is_array_path(StringView prop_name) {
    FixedVector<StringView,8> parts;
    String::split_ref(parts,prop_name,'/');
    if(parts.size()!=3)
        return false;
    FixedVector<StringView, 32, false> parts_2;
    for(auto c : parts[1])
        if(!isdigit(c))
            return false;
    return true;
}
enum GroupPropStatus {
    NO_GROUP,
    STARTED_GROUP,
    CONTINUE_GROUP,
    FINISHED_GROUP,
};
static int new_group_prop_status(int curr_idx,int prev_idx) {
    if (prev_idx == -1 && curr_idx == -1)
        return NO_GROUP;

    if(prev_idx==-1 && curr_idx!=-1)
        return STARTED_GROUP;

    if(curr_idx>=prev_idx) {
        return CONTINUE_GROUP;
    }
    if(curr_idx==-1 && prev_idx!=-1)
        return FINISHED_GROUP;
    // else (curr_idx<prev_idx)
    return STARTED_GROUP;
}

static bool _populate_object_type_interfaces(ReflectionData &rd) {
    auto& current_namespace = rd.namespaces.back();
    current_namespace.obj_types.clear();
    current_namespace.obj_type_insert_order.clear();
    Vector<StringName> class_list;
    ClassDB::get_class_list(&class_list);
    eastl::sort(class_list.begin(), class_list.end(), WrapAlphaCompare());

    while (!class_list.empty()) {
        StringName type_cname = class_list.front();
        if (type_cname == "@") {
            class_list.pop_front();
            continue;
        }
        ClassDB::APIType api_type = ClassDB::get_api_type(type_cname);

        if (api_type == ClassDB::API_NONE) {
            class_list.pop_front();
            continue;
        }

        if (!ClassDB::is_class_exposed(type_cname)) {
            _log("Ignoring type '%s' because it's not exposed\n", String(type_cname).c_str());
            class_list.pop_front();
            continue;
        }

        if (!ClassDB::is_class_enabled(type_cname)) {
            _log("Ignoring type '%s' because it's not enabled\n", String(type_cname).c_str());
            class_list.pop_front();
            continue;
        }

        auto class_iter = ClassDB::classes.find(type_cname);

        TypeInterface itype = TypeInterface::create_object_type(type_cname.asCString(), convertApiType(api_type));

        itype.base_name = ClassDB::get_parent_class(type_cname).asCString();
        itype.is_singleton = Engine::get_singleton()->has_singleton(StringName(itype.name));
        itype.is_instantiable = class_iter->second.creation_func && !itype.is_singleton;
        itype.is_reference = ClassDB::is_parent_class(type_cname, "RefCounted");
        itype.memory_own = itype.is_reference;
        itype.is_namespace = class_iter->second.is_namespace;

        //itype.c_out = "\treturn ";
        //itype.c_out += C_METHOD_UNMANAGED_GET_MANAGED;
        //itype.c_out += itype.is_reference ? "((Object *)%1.get());\n" : "((Object *)%1);\n";

        //itype.cs_in = itype.is_singleton ? BINDINGS_PTR_FIELD : "Object." CS_SMETHOD_GETINSTANCE "(%0)";

        //itype.c_type = "Object";
        //itype.c_type_in = "Object *";
        //itype.c_type_out = "MonoObject*";
        //itype.cs_type = itype.proxy_name;
        //itype.im_type_in = "IntPtr";
        //itype.im_type_out = itype.proxy_name;

        // Populate properties
        Vector<PropertyInfo> property_list;
        ClassDB::get_property_list(type_cname, &property_list, true);

        Map<QString, QString> accessor_methods;
        int last_prop_idx = -1;
        int this_prop_idx = -1;
        String indexed_group_prefix;
        PropertyInterface indexed_property;
        bool in_array = false;
        for (const PropertyInfo& property : property_list) {

            if (property.usage & PROPERTY_USAGE_GROUP || property.usage & PROPERTY_USAGE_CATEGORY)
                continue;

            bool valid = false;
            if(last_prop_idx>250)
                printf("1");
            last_prop_idx = this_prop_idx;
            this_prop_idx = ClassDB::get_property_index(type_cname, property.name, &valid);
            ERR_FAIL_COND_V(!valid, false);
            auto status = new_group_prop_status(this_prop_idx,last_prop_idx);
            if(status!=NO_GROUP && status!=FINISHED_GROUP) {
                Vector<StringView> parts;
                String::split_ref(parts,property.name,"/");
                if(status==STARTED_GROUP || (parts.size()>1 && indexed_property.cname != String(parts[0]).c_str())) {
                    in_array = _is_array_path(property.name);
                    indexed_property = {};
                    PropertyInterface::TypedEntry e;
                    if (in_array) { //
                        indexed_property.cname = String(parts[0]).c_str();
                        e.subfield_name = String(parts[2]).c_str();
                        e.index = -2;
                    }
                    else {
                        if(parts.size()>1) {
                            indexed_property.cname = String(parts.front()).c_str();
                            parts.pop_front();
                        }
                        e.subfield_name = String(parts.front()).c_str();
                        e.index = this_prop_idx;
                    }

                    fill_type_info(property, e.entry_type);
                    e.setter = ClassDB::get_property_setter(type_cname, property.name).asCString();
                    e.getter = ClassDB::get_property_getter(type_cname, property.name).asCString();
                    indexed_property.indexed_entries.push_back(e);

                }
                else {
                    PropertyInterface::TypedEntry e;
                    if (in_array) {
                        if(this_prop_idx == 0) { // array like, scanning fields only at index 0
                            e.subfield_name = String(parts[2]).c_str();
                            e.index = -2;
                            e.setter = ClassDB::get_property_setter(type_cname, property.name).asCString();
                            e.getter = ClassDB::get_property_getter(type_cname, property.name).asCString();
                            fill_type_info(property, e.entry_type);
                            indexed_property.indexed_entries.push_back(e);
                        }
                    }
                    else {
                        if (parts.size() > 1) {
                            assert(indexed_property.cname == String(parts.front()).c_str());
                            parts.pop_front();
                        }
                        e.subfield_name = String(parts.front()).c_str();
                        e.index = this_prop_idx;
                        e.setter = ClassDB::get_property_setter(type_cname, property.name).asCString();
                        e.getter = ClassDB::get_property_getter(type_cname, property.name).asCString();
                        fill_type_info(property, e.entry_type);
                        indexed_property.indexed_entries.push_back(e);
                    }
                }
                continue;
            }
            else {
                if(last_prop_idx != -1) {
                    indexed_property.max_property_index = last_prop_idx;
                    itype.properties.push_back(indexed_property);
                }
                else {
                }
            }
            in_array = false;
            PropertyInterface iprop;
            iprop.cname = property.name.asCString();
            PropertyInterface::TypedEntry e;
            e.setter = ClassDB::get_property_setter(type_cname, property.name).asCString();
            e.getter = ClassDB::get_property_getter(type_cname, property.name).asCString();
            fill_type_info(property, e.entry_type);
            iprop.indexed_entries.push_back(e);
            iprop.max_property_index = -1;

            /*if (!iprop.setter.isEmpty())
                accessor_methods[iprop.setter] = iprop.cname;
            if (!iprop.getter.isEmpty())
                accessor_methods[iprop.getter] = iprop.cname;*/
            //iprop.proxy_name = mapper->mapPropertyName(iprop.cname.toUtf8().data()).c_str();
            //iprop.proxy_name.replace("/", "__"); // Some members have a slash...
            itype.properties.push_back(iprop);
        }
        if (in_array) {
            indexed_property.max_property_index = last_prop_idx;
            itype.properties.push_back(indexed_property);
        }
        // Populate methods

        Vector<MethodInfo> virtual_method_list;
        ClassDB::get_virtual_methods(type_cname, &virtual_method_list, true);

        Vector<MethodInfo> method_list;
        ClassDB::get_method_list(type_cname, &method_list, true);
        eastl::sort(method_list.begin(), method_list.end());
        for (const MethodInfo& method_info : method_list) {
            int argc = method_info.arguments.size();

            if (method_info.name.empty())
                continue;

            auto cname = method_info.name;

            //if(mapper->shouldSkipMethod(qPrintable(itype.cname),cname))
            //    continue;

            MethodInterface imethod{ method_info.name.asCString() ,cname.asCString() };

            if (method_info.flags & METHOD_FLAG_VIRTUAL)
                imethod.is_virtual = true;

            PropertyInfo return_info = method_info.return_val;

            MethodBind* m = imethod.is_virtual ? nullptr : ClassDB::get_method(type_cname, method_info.name);

            const Span<const GodotTypeInfo::Metadata> arg_meta(m ? m->get_arguments_meta() : Span<const GodotTypeInfo::Metadata>());
            const Span<const TypePassBy> arg_pass(m ? m->get_arguments_passing() : Span<const TypePassBy>());
            imethod.is_vararg = m && m->is_vararg();

            if (!m && !imethod.is_virtual) {
                ERR_FAIL_COND_V_MSG(!virtual_method_list.find(method_info), false, ("Missing MethodBind for non-virtual method: '" + itype.name + "." + imethod.name + "'."));

                // A virtual method without the virtual flag. This is a special case.

                // There is no method bind, so let's fallback to Godot's object.Call(string, params)
                imethod.requires_object_call = true;

                // The method Object.free is registered as a virtual method, but without the virtual flag.
                // This is because this method is not supposed to be overridden, but called.
                // We assume the return type is void.
                imethod.return_type.cname = "void";

                // Actually, more methods like this may be added in the future,
                // which could actually will return something different.
                // Let's put this to notify us if that ever happens.
                if (itype.name != "Object" || imethod.name != "free") {
                    WARN_PRINT("Notification: New unexpected virtual non-overridable method found."
                        " We only expected Object.free, but found '" + itype.name + "." + imethod.name + "'.");
                }
            }
            else if (return_info.type == VariantType::INT && return_info.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
                imethod.return_type.cname = return_info.class_name.asCString();
                imethod.return_type.is_enum = true;
            }
            else if (!return_info.class_name.empty()) {
                imethod.return_type.cname = return_info.class_name;
                if(return_info.hint == PropertyHint::ResourceType) // assumption -> resource types all return by ref
                    imethod.return_type.pass_by = TypePassBy::RefValue;
                if (!imethod.is_virtual && ClassDB::is_parent_class(return_info.class_name, StringName("Reference")) && return_info.hint != PropertyHint::ResourceType) {
                    /* clang-format off */
                    ERR_PRINT("Return type is reference but hint is not 'PropertyHint::ResourceType'."
                        " Are you returning a reference type by pointer? Method: '" + itype.name + "." + imethod.name + "'.");
                    /* clang-format on */
                    ERR_FAIL_V(false);
                }
            }
            else if (return_info.hint == PropertyHint::ResourceType) {
                imethod.return_type.cname = return_info.hint_string.c_str();
            }
            else if (return_info.type == VariantType::NIL && return_info.usage & PROPERTY_USAGE_NIL_IS_VARIANT) {
                imethod.return_type.cname = "Variant";
            }
            else if (return_info.type == VariantType::NIL) {
                imethod.return_type.cname = "void";
            }
            else {
                if (return_info.type == VariantType::INT) {
                    imethod.return_type.cname = _get_int_type_name_from_meta(arg_meta.size() > 0 ? arg_meta[0] : GodotTypeInfo::METADATA_NONE).data();
                }
                else if (return_info.type == VariantType::FLOAT) {
                    imethod.return_type.cname = _get_float_type_name_from_meta(arg_meta.size() > 0 ? arg_meta[0] : GodotTypeInfo::METADATA_NONE).data();
                }
                else {
                    imethod.return_type.cname = Variant::interned_type_name(return_info.type).asCString();
                }
            }

            for (int i = 0; i < argc; i++) {
                const PropertyInfo& arginfo = method_info.arguments[i];

                StringName orig_arg_name = arginfo.name;

                ArgumentInterface iarg;
                iarg.name = orig_arg_name.asCString();

                if (arginfo.type == VariantType::INT && arginfo.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
                    iarg.type.cname = arginfo.class_name.asCString();
                    iarg.type.is_enum = true;
                    iarg.type.pass_by = TypePassBy::Value;
                }
                else if (!arginfo.class_name.empty()) {
                    iarg.type.cname = arginfo.class_name.asCString();
                    iarg.type.pass_by = arg_pass.size() > (i + 1) ? arg_pass[i + 1] : TypePassBy::Reference;
                }
                else if (arginfo.hint == PropertyHint::ResourceType) {
                    iarg.type.cname = arginfo.hint_string.c_str();
                    iarg.type.pass_by = TypePassBy::Reference;
                }
                else if (arginfo.type == VariantType::NIL) {
                    iarg.type.cname = "Variant";
                    iarg.type.pass_by = arg_pass.size() > (i + 1) ? arg_pass[i + 1] : TypePassBy::Value;
                }
                else {
                    if (arginfo.type == VariantType::INT) {
                        iarg.type.cname = _get_int_type_name_from_meta(arg_meta.size() > (i + 1) ? arg_meta[i + 1] : GodotTypeInfo::METADATA_NONE).data();
                    }
                    else if (arginfo.type == VariantType::FLOAT) {
                        iarg.type.cname = _get_float_type_name_from_meta(arg_meta.size() > (i + 1) ? arg_meta[i + 1] : GodotTypeInfo::METADATA_NONE).data();
                    }
                    else if (arginfo.type == VariantType::STRING) {
                        iarg.type.cname = _get_string_type_name_from_meta(arg_meta.size() > (i + 1) ? arg_meta[i + 1] : GodotTypeInfo::METADATA_NONE).data();
                    }
                    else {

                        iarg.type.cname = _get_variant_type_name_from_meta(arginfo.type, arg_meta.size() > (i + 1) ? arg_meta[i + 1] : GodotTypeInfo::METADATA_NONE).data();
                    }
                    iarg.type.pass_by = arg_pass.size() > (i + 1) ? arg_pass[i + 1] : TypePassBy::Value;
                }
                if (iarg.type.cname == "Object" && iarg.type.pass_by == TypePassBy::Value) {
                    // Fixup for virtual methods, since passing Object by value makes no sense.
                    iarg.type.pass_by = TypePassBy::Pointer;
                }
                //iarg.name = mapper->mapArgumentName(qPrintable(iarg.name)).data();

                if (m && m->has_default_argument(i)) {
                    bool defval_ok = _arg_default_value_from_variant(m->get_default_argument(i), iarg);
                    ERR_FAIL_COND_V_MSG(!defval_ok, false,
                        String("Cannot determine default value for argument '") + orig_arg_name + "' of method '" + itype.name + "." + imethod.name + "'.");
                }

                imethod.add_argument(iarg);
            }

            if (imethod.is_vararg) {
                ArgumentInterface ivararg;
                ivararg.type.cname = "VarArg";
                ivararg.name = "@args";
                imethod.add_argument(ivararg);
            }

            //imethod.proxy_name = mapper->mapMethodName(qPrintable(imethod.name),qPrintable(itype.cname)).c_str();

            //Map<QString, QString>::iterator accessor = accessor_methods.find(imethod.cname);
            //if (accessor != accessor_methods.end()) {
            //    const PropertyInterface* accessor_property = itype.find_property_by_name(accessor->second);

            //    // We only deprecate an accessor method if it's in the same class as the property. It's easier this way, but also
            //    // we don't know if an accessor method in a different class could have other purposes, so better leave those untouched.
            //    imethod.is_deprecated = true;
            //    imethod.deprecation_message = imethod.proxy_name + " is deprecated. Use the " + accessor_property->proxy_name + " property instead.";
            //}

            if (!imethod.is_virtual && imethod.name[0] == '_') {
                bool found=false;
                for (const PropertyInterface& iprop : itype.properties) {
                    if(found)
                        break;
                    for(const auto &idx : iprop.indexed_entries) {
                        if (idx.setter == imethod.name || idx.getter == imethod.name) {
                            imethod.is_internal = true;
                            itype.methods.push_back(imethod);
                            found = true;
                            break;
                        }
                    }
                }
            }
            else {
                itype.methods.push_back(imethod);
            }
        }

        // Populate enums and constants

        List<String> constants;
        ClassDB::get_integer_constant_list(type_cname, &constants, true);

        const HashMap<StringName, ClassDB::EnumDescriptor>& enum_map = class_iter->second.enum_map;
        for (const auto& F : enum_map) {
            auto parts = StringUtils::split(F.first, "::");
            if (parts.size() > 1 && itype.name == parts[0]) {
                parts.pop_front(); // Skip leading type name, this will be fixed below
            }
            String enum_proxy_cname(parts.front().data(), parts.front().size());
            String enum_proxy_name(enum_proxy_cname);
            /*if (itype.find_property_by_proxy_name(enum_proxy_name)) {
                // We have several conflicts between enums and PascalCase properties,
                // so we append 'Enum' to the enum name in those cases.
                enum_proxy_name += "Enum";
                enum_proxy_cname = enum_proxy_name;
            }*/
            EnumInterface ienum(enum_proxy_cname);
            ienum.underlying_type = F.second.underlying_type;
            const Vector<StringName>& enum_constants = F.second.enumerators;
            for (const StringName& constant_cname : enum_constants) {
                String constant_name(constant_cname.asCString());
                auto value = class_iter->second.constant_map.find(constant_cname);
                ERR_FAIL_COND_V(value == class_iter->second.constant_map.end(), false);
                constants.remove(constant_cname.asCString());

                ConstantInterface iconstant(constant_name, value->second);

                ienum.constants.push_back(iconstant);
            }

            itype.enums.push_back(ienum);

            TypeInterface enum_itype;
            enum_itype.is_enum = true;
            enum_itype.name = itype.name + "." + enum_proxy_cname;
            //enum_itype.cname = enum_itype.name;
            //enum_itype.proxy_name = itype.proxy_name + "." + enum_proxy_name;
            TypeInterface::postsetup_enum_type(enum_itype);
            current_namespace.enum_types.emplace(enum_itype.name, enum_itype);
        }

        for (const String& constant_name : constants) {
            auto value = class_iter->second.constant_map.find(StringName(constant_name));
            ERR_FAIL_COND_V(value == class_iter->second.constant_map.end(), false);

            ConstantInterface iconstant(constant_name.c_str(), value->second);

            itype.constants.push_back(iconstant);
        }

        auto insert_res = current_namespace.obj_types.emplace(itype.name, itype);
        if (insert_res.second) //was inserted, record it in order container
            current_namespace.obj_type_insert_order.emplace_back(itype.name);

        class_list.pop_front();
    }

    return true;
}
void _populate_builtin_type_interfaces(ReflectionData &rd) {
    auto& current_namespace = rd.namespaces.back();

    current_namespace.builtin_types.clear();

    TypeInterface itype;

#define INSERT_STRUCT_TYPE(m_type)                             \
    {                                                          \
        itype = TypeInterface::create_value_type(#m_type);     \
        itype.ret_as_byref_arg = true;                         \
        current_namespace.builtin_types.emplace(itype.name, itype);           \
    }

    INSERT_STRUCT_TYPE(Vector2)
    INSERT_STRUCT_TYPE(Rect2)
    INSERT_STRUCT_TYPE(Transform2D)
    INSERT_STRUCT_TYPE(Vector3)
    INSERT_STRUCT_TYPE(Basis)
    INSERT_STRUCT_TYPE(Quat)
    INSERT_STRUCT_TYPE(Transform)
    INSERT_STRUCT_TYPE(AABB)
    INSERT_STRUCT_TYPE(Color)
    INSERT_STRUCT_TYPE(Plane)

#undef INSERT_STRUCT_TYPE

    // bool
    itype = TypeInterface::create_value_type("bool");
    current_namespace.builtin_types.emplace(itype.name, itype);

    // Integer types
    {
        // C interface for 'uint32_t' is the same as that of enums. Remember to apply
        // any of the changes done here to 'TypeInterface::postsetup_enum_type' as well.
#define INSERT_INT_TYPE(m_name, m_c_type_in_out, m_c_type)        \
    {                                                             \
        itype = TypeInterface::create_value_type(m_name); \
        current_namespace.builtin_types.emplace(itype.name, itype);                 \
    }

        INSERT_INT_TYPE("sbyte", int8_t, int8_t)
        INSERT_INT_TYPE("short", int16_t, int16_t)
        INSERT_INT_TYPE("int", int32_t, int32_t)
        INSERT_INT_TYPE("byte", uint8_t, uint8_t)
        INSERT_INT_TYPE("ushort", uint16_t, uint16_t)
        INSERT_INT_TYPE("uint", uint32_t, uint32_t)

        itype = TypeInterface::create_value_type("int64_t");
        itype.ret_as_byref_arg = true;
        current_namespace.builtin_types.emplace(itype.name, itype);

        itype = TypeInterface::create_value_type("uint64_t");
        itype.ret_as_byref_arg = true;
        current_namespace.builtin_types.emplace(itype.name, itype);
    }

    // Floating point types
    {
        // float
        itype = TypeInterface();
        itype.name = "float";
        itype.ret_as_byref_arg = true;
        current_namespace.builtin_types.emplace(itype.name, itype);

        // double
        itype = TypeInterface();
        itype.name = "double";
        itype.ret_as_byref_arg = true;
        current_namespace.builtin_types.emplace(itype.name, itype);
    }

    // String
    itype = TypeInterface();
    itype.name = "String";
    current_namespace.builtin_types.emplace(itype.name, itype);

    // StringView
    itype = TypeInterface();
    itype.name = "String";
    current_namespace.builtin_types.emplace(itype.name, itype);
    // StringName
    itype = TypeInterface();
    itype.name = "String";
    current_namespace.builtin_types.emplace(itype.name, itype);

    // NodePath
    itype = TypeInterface();
    itype.name = "NodePath";
    current_namespace.builtin_types.emplace(itype.name, itype);

    // RID
    itype = TypeInterface();
    itype.name = "RID";
    current_namespace.builtin_types.emplace(itype.name, itype);

    // Variant
    itype = TypeInterface();
    itype.name = "Variant";
    current_namespace.builtin_types.emplace(itype.name, itype);

    // VarArg (fictitious type to represent variable arguments)
    itype = TypeInterface();
    itype.name = "VarArg";
    current_namespace.builtin_types.emplace(itype.name, itype);

#define INSERT_ARRAY_FULL(m_name, m_type, m_proxy_t)                          \
    {                                                                         \
        itype = TypeInterface();                                              \
        itype.name = #m_name;                                                 \
        current_namespace.builtin_types.emplace(itype.name, itype);                 \
    }

#define INSERT_ARRAY_NC_FULL(m_name, m_type, m_proxy_t)                          \
    {                                                                         \
        itype = TypeInterface();                                              \
        itype.name = #m_name;                                                 \
        current_namespace.builtin_types.emplace(itype.name, itype);                 \
    }
#define INSERT_ARRAY_TPL_FULL(m_name, m_type, m_proxy_t)                      \
    {                                                                         \
        itype = TypeInterface();                                              \
        itype.name = #m_name;                                                 \
        current_namespace.builtin_types.emplace(StringName(itype.name), itype);                 \
    }
#define INSERT_ARRAY(m_type, m_proxy_t) INSERT_ARRAY_FULL(m_type, m_type, m_proxy_t)

    INSERT_ARRAY(PoolIntArray, int)
        INSERT_ARRAY_NC_FULL(VecInt, VecInt, int)
        INSERT_ARRAY_NC_FULL(VecByte, VecByte, byte)
        INSERT_ARRAY_NC_FULL(VecFloat, VecFloat, float)
        INSERT_ARRAY_NC_FULL(VecString, VecString, string)
        INSERT_ARRAY_NC_FULL(VecVector2, VecVector2, Vector2)
        INSERT_ARRAY_NC_FULL(VecVector3, VecVector3, Vector3)
        INSERT_ARRAY_NC_FULL(VecColor, VecColor, Color)

        INSERT_ARRAY_FULL(PoolByteArray, PoolByteArray, byte)


#ifdef REAL_T_IS_DOUBLE
        INSERT_ARRAY(PoolRealArray, double)
#else
        INSERT_ARRAY(PoolRealArray, float)
#endif

        INSERT_ARRAY(PoolStringArray, string)

        INSERT_ARRAY(PoolColorArray, Color)
        INSERT_ARRAY(PoolVector2Array, Vector2)
        INSERT_ARRAY(PoolVector3Array, Vector3)

#undef INSERT_ARRAY

        // Array
        itype = TypeInterface();
    itype.name = "Array";
    current_namespace.builtin_types.emplace(itype.name, itype);

    // Dictionary
    itype = TypeInterface();
    itype.name = "Dictionary";
    current_namespace.builtin_types.emplace(itype.name, itype);

    // void (fictitious type to represent the return type of methods that do not return anything)
    itype = TypeInterface();
    itype.name = "void";
    current_namespace.builtin_types.emplace(itype.name, itype);
}
static bool allUpperCase(StringView s) {
    for (char c : s) {
        if (StringUtils::char_uppercase(c) != c)
            return false;
    }
    return true;
}
void _populate_global_constants(ReflectionData &rd) {
    auto& current_namespace = rd.namespaces.back();

    int global_constants_count = GlobalConstants::get_global_constant_count();

    auto synth_global_iter = ClassDB::classes.find("@");
    if (synth_global_iter != ClassDB::classes.end()) {
        for (const auto& e : synth_global_iter->second.enum_map) {
            EnumInterface ienum(String(e.first).c_str());
            ienum.underlying_type = e.second.underlying_type.asCString();
            for (const auto& valname : e.second.enumerators) {
                int constant_value = synth_global_iter->second.constant_map[valname];
                ienum.constants.emplace_back(valname.asCString(),constant_value);
            }
            current_namespace.global_enums.emplace_back(eastl::move(ienum));
        }
    }
    if (global_constants_count > 0) {
        QHash<QString, DocContents::ClassDoc>::iterator match = rd.doc->class_list.find("@GlobalScope");

        CRASH_COND_MSG(match == rd.doc->class_list.end(), "Could not find '@GlobalScope' in DocData.");

        const DocContents::ClassDoc& global_scope_doc = *match;

        for (int i = 0; i < global_constants_count; i++) {

            String constant_name = GlobalConstants::get_global_constant_name(i);

            int constant_value = GlobalConstants::get_global_constant_value(i);
            StringName enum_name = GlobalConstants::get_global_constant_enum(i);
            ConstantInterface iconstant;
            iconstant = ConstantInterface(constant_name.c_str(), constant_value);

            if (enum_name.empty()) {
                current_namespace.global_constants.push_back(iconstant);
            }
            else {
                EnumInterface ienum(String(enum_name).c_str());
                auto enum_match = current_namespace.global_enums.find(ienum);
                if (enum_match != current_namespace.global_enums.end()) {
                    enum_match->constants.push_back(iconstant);
                }
                else {
                    ienum.constants.push_back(iconstant);
                    current_namespace.global_enums.push_back(ienum);
                }
            }
        }

        for (EnumInterface& ienum : current_namespace.global_enums) {

            TypeInterface enum_itype;
            enum_itype.is_enum = true;
            enum_itype.name = ienum.cname;
            TypeInterface::postsetup_enum_type(enum_itype);

            current_namespace.enum_types.emplace(enum_itype.name, enum_itype);

            /*int prefix_length = _determine_enum_prefix(ienum);

            // HARDCODED: The Error enum have the prefix 'ERR_' for everything except 'OK' and 'FAILED'.
            if (ienum.cname == name_cache->enum_Error) {
                if (prefix_length > 0) { // Just in case it ever changes
                    ERR_PRINT("Prefix for enum 'Error' is not empty.");
                }

                prefix_length = 1; // 'ERR_'
            }

            _apply_prefix_to_enum_constants(ienum, prefix_length);*/
        }
    }

    // HARDCODED
    Vector<StringName> hardcoded_enums;
    hardcoded_enums.push_back("Vector3::Axis");
    for (const StringName& E : hardcoded_enums) {
        // These enums are not generated and must be written manually (e.g.: Vector3.Axis)
        // Here, we assume core types do not begin with underscore
        TypeInterface enum_itype;
        enum_itype.is_enum = true;
        enum_itype.name = E.asCString();
        TypeInterface::postsetup_enum_type(enum_itype);
        //assert(!StringView(enum_itype.name).contains("::"));
        current_namespace.enum_types.emplace(enum_itype.name, enum_itype);
    }
}
void _initialize_reflection_data(ReflectionData &rd,DocData* docs) {

    rd.doc = docs;

    rd.namespaces.clear();
    rd.namespaces.emplace_back();

    auto& current_namespace = rd.namespaces.back();
    current_namespace.namespace_name = "Godot";

    rd.build_doc_lookup_helper();

//    _initialize_blacklisted_methods();

    bool obj_type_ok = _populate_object_type_interfaces(rd);
    ERR_FAIL_COND_MSG(!obj_type_ok, "Failed to generate object type interfaces");

    _populate_builtin_type_interfaces(rd);

    _populate_global_constants(rd);

    // Generate internal calls (after populating type interfaces and global constants)

    //core_custom_icalls.clear();
    //editor_custom_icalls.clear();


    for (const auto & E : current_namespace.obj_type_insert_order) {
        const TypeInterface& itype = current_namespace.obj_types[E];
        //_generate_method_icalls(itype);
    }
}
