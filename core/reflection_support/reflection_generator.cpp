#include "reflection_generator.h"

#if defined(DEBUG_METHODS_ENABLED) && defined(TOOLS_ENABLED)

#include "reflection_data.h"
#include "core/variant.h"
#include "core/engine.h"
#include "core/string_utils.inl"
#include "core/math/quat.h"
#include "core/math/transform_2d.h"
#include "core/math/transform.h"
#include "core/class_db.h"
#include "core/global_constants.h"
#include "core/os/os.h"
#include "core/method_bind.h"
#include "core/script_language.h"
#include "core/string_formatter.h"
#include "core/version.h"
#include "core/class_db.h"

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

namespace {
bool s_log_print_enabled = true;

void _log(const char *p_format, ...) {
    if (s_log_print_enabled) {
        va_list list;

        va_start(list, p_format);
        OS::get_singleton()->print(String().append_sprintf_va_list(p_format, list).c_str());
        va_end(list);
    }
}

String toInitializer(Vector3 v) {
    return FormatVE("(%ff, %ff, %ff)", v.x, v.y, v.z);
}

String toInitializer(Vector2 v) {
    return FormatVE("(%ff, %ff)", v.x, v.y);
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
    case VariantType::BOOL:
        r_iarg.default_argument = p_val.as<bool>() ? "true" : "false";
        break;
    case VariantType::INT:
        if (r_iarg.type.cname != "int32_t") {
            r_iarg.default_argument = "(%s)" + r_iarg.default_argument;
        }
        break;
    case VariantType::REN_ENT:
        // the only one that makes sense. Disabled, until c# side is ready.
        // r_iarg.default_argument = "RenderingEntity.Null";
        assert(false);
        break;
    case VariantType::FLOAT:
        r_iarg.default_argument += "f";
        break;
    case VariantType::STRING_NAME:
    case VariantType::STRING:
    case VariantType::NODE_PATH:
        r_iarg.default_argument = "\"" + r_iarg.default_argument + "\"";
        break;
        case VariantType::TRANSFORM: {
            Transform transform = p_val.as<Transform>();
            if (transform == Transform()) {
                r_iarg.default_argument = "Transform.Identity";
            } else {
                Basis basis = transform.basis;
                r_iarg.default_argument = "new Transform(new Vector3" + toInitializer(basis.get_column(0)) +
                                          ", new Vector3" + toInitializer(basis.get_column(1)) + ", new Vector3" +
                                          toInitializer(basis.get_column(2)) + ", new Vector3" +
                                          toInitializer(transform.origin) + ")";
            }
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        } break;

        case VariantType::PLANE: {
            Plane plane = p_val.operator Plane();
            r_iarg.default_argument = "new Plane(new Vector3(" + (String)plane.normal + "), " + rtos(plane.d) + ")";
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        } break;
        case VariantType::AABB: {
            AABB aabb = p_val.as<::AABB>();
            r_iarg.default_argument = "new AABB(new Vector3(" + aabb.position.operator String() + "), new Vector3(" +
                                      aabb.size.operator String() + "))";
            r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        } break;
        case VariantType::COLOR: {
            if (r_iarg.default_argument == "1,1,1,1") {
                r_iarg.default_argument = "1, 1, 1, 1";
            }
            auto parts = r_iarg.default_argument.split(',');
            for (auto &str : parts) {
                str += "f";
            }
            auto clr = String::joined(parts, ", ");
            r_iarg.default_argument = "new Color(" + clr + ")";
            r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        } break;
        case VariantType::RECT2: {
            Rect2 rect = p_val.as<Rect2>();
            r_iarg.default_argument = "new Rect2(new Vector2(" + rect.position.operator String() + "), new Vector2(" +
                                      rect.size.operator String() + "))";
            r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        } break;
    case VariantType::VECTOR2:
        case VariantType::VECTOR3:
            r_iarg.default_argument = "new %s" + r_iarg.default_argument;
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        break;
    case VariantType::OBJECT:
        ERR_FAIL_COND_V_MSG(!p_val.is_zero(), false,
                    "Parameter of type '" + r_iarg.type.cname + "' can only have null/zero as the default value.");

        r_iarg.default_argument = "null";
        break;
        case VariantType::DICTIONARY:
            r_iarg.default_argument = "new %s()";
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_REF;
        break;
    case VariantType::_RID:
        ERR_FAIL_COND_V_MSG(r_iarg.type.cname != "RID", false,
            "Parameter of type '" + (r_iarg.type.cname) + "' cannot have a default value of type 'RID'.");

        ERR_FAIL_COND_V_MSG(!p_val.is_zero(), false,
                    "Parameter of type '" + (r_iarg.type.cname) + "' can only have null/zero as the default value.");

        r_iarg.default_argument = "null";
        break;
    case VariantType::ARRAY:
    case VariantType::POOL_BYTE_ARRAY:
    case VariantType::POOL_INT_ARRAY:
    case VariantType::POOL_FLOAT32_ARRAY:
    case VariantType::POOL_STRING_ARRAY:
    case VariantType::POOL_VECTOR2_ARRAY:
    case VariantType::POOL_VECTOR3_ARRAY:
    case VariantType::POOL_COLOR_ARRAY:
        r_iarg.default_argument = "new %s {}";
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_REF;
        break;
        case VariantType::TRANSFORM2D: {
            Transform2D transform = p_val.as<Transform2D>();
            if (transform == Transform2D()) {
                r_iarg.default_argument = "Transform2D.Identity";
            } else {
                r_iarg.default_argument = "new Transform2D(new Vector2" + transform.elements[0].operator String() +
                                          ", new Vector2" + transform.elements[1].operator String() + ", new Vector2" +
                                          transform.elements[2].operator String() + ")";
            }
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        } break;
        case VariantType::BASIS: {
            Basis basis = p_val.as<Basis>();
            if (basis == Basis()) {
                r_iarg.default_argument = "Basis.Identity";
            } else {
                r_iarg.default_argument = "new Basis(new Vector3" + basis.get_column(0).operator String() +
                                          ", new Vector3" + basis.get_column(1).operator String() + ", new Vector3" +
                                          basis.get_column(2).operator String() + ")";
            }
            r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        } break;
        case VariantType::QUAT: {
            Quat quat = p_val.as<Quat>();
            if (quat == Quat()) {
                r_iarg.default_argument = "Quat.Identity";
            } else {
                r_iarg.default_argument = "new Quat" + quat.operator String();
            }
            r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
        } break;
        default: {
            CRASH_NOW_MSG("Unexpected Variant type: " + StringUtils::num_uint64((int)p_val.get_type()));
        break;
    }
    }

    if (r_iarg.def_param_mode == ArgumentInterface::CONSTANT && r_iarg.type.cname == "Variant" &&
            r_iarg.default_argument != "null")
        r_iarg.def_param_mode = ArgumentInterface::NULLABLE_REF;

    return true;
}
APIType convertApiType(ClassDB_APIType ap) {
    if(ap==ClassDB_APIType::API_NONE)
        return APIType::Invalid;
    if (ap == ClassDB_APIType::API_CORE)
        return APIType::Common;
    if (ap == ClassDB_APIType::API_EDITOR)
        return APIType::Editor;
    if (ap == ClassDB_APIType::API_SERVER)
        return APIType::Server;
    return APIType::Invalid;

}

StringView _get_int_type_name_from_meta(GodotTypeInfo::Metadata p_meta) {
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
        case GodotTypeInfo::METADATA_IS_ENTITY_ID:
            return "RenderingEntity";
    default:
        // Assume INT32
        return "int32_t";
    }
}

StringView _get_float_type_name_from_meta(GodotTypeInfo::Metadata p_meta) {
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

StringView _get_string_type_name_from_meta(GodotTypeInfo::Metadata p_meta) {
    switch (p_meta) {
    case GodotTypeInfo::METADATA_STRING_VIEW:
        return "StringView";
    default:
        // Assume default String type
        return "String";
    }
}

StringName _get_variant_type_name_from_meta(VariantType tp, GodotTypeInfo::Metadata p_meta) {
    if (GodotTypeInfo::METADATA_NON_COW_CONTAINER == p_meta) {
        switch (tp) {

        case VariantType::POOL_BYTE_ARRAY:
            return StringName("PoolByteArray");

        case VariantType::POOL_INT_ARRAY:
            return StringName("PoolIntArray");
        case VariantType::POOL_FLOAT32_ARRAY:
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
void fill_type_info(const PropertyInfo &arginfo, TypeReference &tgt) {
    if (arginfo.type == VariantType::INT && arginfo.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
        tgt.cname = arginfo.class_name;
        tgt.is_enum = TypeRefKind::Enum;
        tgt.pass_by = TypePassBy::Value;
    } else if (arginfo.hint == PropertyHint::ResourceType) {
        if(arginfo.type==VariantType::ARRAY || arginfo.hint_string.contains(","))
            tgt.cname = "PH:"+arginfo.hint_string;
        else
            tgt.cname = arginfo.hint_string;
        tgt.is_enum = arginfo.type!=VariantType::ARRAY ? TypeRefKind::Simple : TypeRefKind::Array;
        tgt.pass_by = TypePassBy::Reference;
    } else if (!arginfo.class_name.empty()) {
        tgt.cname = arginfo.class_name;
        tgt.pass_by = TypePassBy::Reference;
    } else if (arginfo.type == VariantType::NIL) {
        tgt.cname = "Variant";
        tgt.pass_by = TypePassBy::Value;
    } else {
        if (arginfo.type == VariantType::INT) {
            tgt.cname = "int";
        } else if (arginfo.type == VariantType::FLOAT) {
            tgt.cname = "float";
        } else if (arginfo.type == VariantType::STRING) {
            tgt.cname = "String";
        } else {

            tgt.cname = _get_variant_type_name_from_meta(arginfo.type, GodotTypeInfo::METADATA_NONE).asCString();
        }
        tgt.pass_by = TypePassBy::Value;
    }
    if (tgt.cname == "Object" && tgt.pass_by == TypePassBy::Value) {
        // Fixup for virtual methods, since passing Object by value makes no sense.
        tgt.pass_by = TypePassBy::Pointer;
    }
}

}
enum GroupPropStatus {
    NO_GROUP,
    STARTED_GROUP,
    CONTINUE_GROUP,
    FINISHED_GROUP,
};

static void add_opaque_types(ReflectionData &rd,ReflectionSource src) {
    if(src!=ReflectionSource::Core)
        return;
    NamespaceInterface *core_ns=nullptr;
    for(auto & ns : rd.namespaces) {
        if(ns.name=="Godot") {
            core_ns = &ns;
            break;
        }
    }
    assert(core_ns);
    struct {
        const char *name;
        const char *header;
    } entries[] = {
        {"Variant","core/variant.h"},
        {"String","core/string.h"},
        {"StringView","core/string.h"},
        {"StringName","core/string_name.h"},
        {"NodePath","core/node_path.h"},
        {"RID","core/rid.h"},
        {"VarArg",""}, // synthetic type
        {"Dictionary",""},
        {"Array",""},

        {"Vector2","core/math/vector2.h"},
        {"Vector3","core/math/vector3.h"},
        {"Rect2","core/math/rect2.h"},
        {"Transform2D","core/math/transform_2d.h"},
        {"Basis","core/math/basis.h"},
        {"Quat","core/math/quat.h"},
        {"Transform","core/math/transform.h"},
        {"AABB","core/math/aabb.h"},
        {"Color","core/color.h"},
        {"Callable","core/callable.h"},
        {"Signal","core/callable.h"},
        {"Plane","core/math/plane.h"},
        {"PoolIntArray","core/vector.h"},
        {"VecInt","core/vector.h"},
        {"VecByte","core/vector.h"},
        {"VecFloat","core/vector.h"},
        {"VecString","core/vector.h"},
        {"VecVector2","core/vector.h"},
        {"VecVector3","core/vector.h"},
        {"VecColor","core/vector.h"},
        {"PoolByteArray","core/pool_vector.h"},
        {"PoolRealArray","core/vector.h"},
        {"PoolStringArray","core/vector.h"},
        {"PoolColorArray","core/pool_vector.h"},
        {"PoolVector2Array","core/pool_vector.h"},
        {"PoolVector3Array","core/pool_vector.h"},

    };
    for(const auto &v : entries) {
        auto ti = TypeInterface::create_object_type(v.name, APIType::Common);
        ti.header_path = v.header;
        ti.is_opaque_type = true;
        core_ns->obj_types[ti.name] = eastl::move(ti);
    }
    // Force-add Vector3 axis enum.
    auto &tgt_vec(core_ns->obj_types["Vector3"]);
    static EnumInterface axis_iface("Axis");
    axis_iface.underlying_type="int32_t";
    axis_iface.constants.emplace_back("X",0);
    axis_iface.constants.emplace_back("Y",1);
    axis_iface.constants.emplace_back("Z",2);
    tgt_vec.enums.emplace_back(eastl::move(axis_iface));
}

static String fixup_group_name(String grp) {
    // group names are used as grouped property names but contain spaces, this function fixes that.
    if(!grp.contains(' '))
        return grp;
    return grp.replaced(" ","").replaced("-","");
}
void fillArgInfoFromProperty(ArgumentInterface &iarg,const PropertyInfo& arginfo,const GodotTypeInfo::Metadata &arg_meta,const TypePassBy arg_pass)
{
    StringName orig_arg_name = arginfo.name;

    iarg.name = orig_arg_name;

    if (arginfo.type == VariantType::INT && arginfo.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
        iarg.type.cname = arginfo.class_name.asCString();
        iarg.type.is_enum = TypeRefKind::Enum;
        iarg.type.pass_by = TypePassBy::Value;
    }
    else if (arginfo.type == VariantType::INT && arg_meta == GodotTypeInfo::METADATA_IS_ENTITY_ID) {
        iarg.type.cname = arginfo.class_name.asCString();
        iarg.type.pass_by = TypePassBy::Value;
    }
    else if (!arginfo.class_name.empty()) {
        iarg.type.cname = arginfo.class_name.asCString();
        iarg.type.pass_by = arg_pass;
    }
    else if (arginfo.hint == PropertyHint::ResourceType) {
        iarg.type.cname = "PH:"+arginfo.hint_string;
        iarg.type.is_enum = arginfo.type!=VariantType::ARRAY ? TypeRefKind::Simple : TypeRefKind::Array;
        iarg.type.pass_by = TypePassBy::Reference;
    }
    else if (arginfo.type == VariantType::NIL) {
        iarg.type.cname = "Variant";
        iarg.type.pass_by = arg_pass;
    }
    else {
        if (arginfo.type == VariantType::INT) {
            if(arginfo.hint==PropertyHint::IntIsObjectID) {
                iarg.type.cname = arginfo.class_name;
            }
            else
                iarg.type.cname = _get_int_type_name_from_meta(arg_meta).data();
        }
        else if (arginfo.type == VariantType::FLOAT) {
            iarg.type.cname = _get_float_type_name_from_meta(arg_meta).data();
        }
        else if (arginfo.type == VariantType::STRING) {
            iarg.type.cname = _get_string_type_name_from_meta(arg_meta).data();
        }
        else {

            iarg.type.cname = _get_variant_type_name_from_meta(arginfo.type, arg_meta).asCString();
        }
        iarg.type.pass_by = arg_pass;
    }
    if (iarg.type.cname == "Object" && iarg.type.pass_by == TypePassBy::Value) {
        // Fixup for virtual methods, since passing Object by value makes no sense.
        iarg.type.pass_by = TypePassBy::Pointer;
    }
}

static bool _populate_object_type_interfaces(ReflectionData &rd,ReflectionSource src) {
    auto& current_namespace = rd.namespaces.back();
    current_namespace.obj_types.clear();
    Vector<StringName> class_list;
    ClassDB::get_class_list(&class_list);
    eastl::sort(class_list.begin(), class_list.end(), WrapAlphaCompare());
    if(src==ReflectionSource::Core)
        add_opaque_types(rd,src);

    while (!class_list.empty()) {
        StringName type_cname = class_list.front();
        if (type_cname == "@") {
            class_list.pop_front();
            continue;
        }
        ClassDB_APIType api_type = ClassDB::get_api_type(type_cname);

        if (api_type == ClassDB_APIType::API_NONE) {
            class_list.pop_front();
            continue;
        }
        bool editor_only = src==ReflectionSource::Editor;
        if(editor_only && api_type != ClassDB_APIType::API_EDITOR) {
            class_list.pop_front();
            continue;
        }
        if(!editor_only && api_type==ClassDB_APIType::API_EDITOR) {
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
        String proxy_name = itype.name.starts_with('_') ? itype.name.substr(1) : itype.name;
        itype.base_name = ClassDB::get_parent_class(type_cname).asCString();
        itype.header_path = class_iter->second.usage_header;
        itype.is_singleton = Engine::get_singleton()->has_singleton(StringName(proxy_name));
        itype.is_instantiable = class_iter->second.creation_func && !itype.is_singleton;
        itype.is_reference = ClassDB::is_parent_class(type_cname, "RefCounted");
        itype.memory_own = itype.is_reference;

        itype.is_namespace = class_iter->second.is_namespace;

        // Populate properties
        Vector<PropertyInfo> property_list;
        ClassDB::get_property_list(type_cname, &property_list, true);

        Map<String, String> accessor_methods;
        int last_prop_idx = -1;
        int this_prop_idx = -1;
        //String indexed_group_prefix;
        PropertyInterface indexed_property;
        String current_array_prefix;
        int current_array_max_size=0;
        String current_group;
        String current_group_prefix;
        String current_category;
        for (const PropertyInfo& property : property_list) {
            if (property.usage & PROPERTY_USAGE_GROUP || property.usage & PROPERTY_USAGE_CATEGORY) {
                if(property.usage & PROPERTY_USAGE_GROUP) {
                    current_group = property.name;

                    if(!indexed_property.cname.empty()) {
                        current_group_prefix.clear();
                        current_array_prefix.clear();
                        if(current_array_max_size!=0)
                            indexed_property.max_property_index = current_array_max_size;
                        else
                            indexed_property.max_property_index = -2;
                        itype.properties.emplace_back(eastl::move(indexed_property));
                        indexed_property = {};
                        current_array_max_size = 0;
                        current_array_prefix.clear();
                    }
                    current_group_prefix = property.hint_string;
                    indexed_property.cname = fixup_group_name(current_group);
                }
                if (property.usage & PROPERTY_USAGE_CATEGORY) {
                    current_category = property.name;
                }
                continue;
            }
            if(property.usage & PROPERTY_USAGE_ARRAY) {
                if(!indexed_property.cname.empty()) {
                    current_group_prefix.clear();
                    current_array_prefix.clear();
                    if(current_array_max_size!=0)
                        indexed_property.max_property_index = current_array_max_size;
                    else
                        indexed_property.max_property_index = -2;
                    itype.properties.emplace_back(eastl::move(indexed_property));
                    indexed_property = {};
                    current_array_max_size = 0;
                    current_array_prefix.clear();
                }

               current_array_prefix = property.hint_string;
               current_array_max_size = property.element_count;
               //FIXME: Close previous array/group here??
               continue;
            }

            bool valid = false;
            this_prop_idx = ClassDB::get_property_index(type_cname, property.name, &valid);
            ERR_FAIL_COND_V(!valid, false);

            if(!current_array_prefix.empty()) { // we are in an array definition block.
                if(StringUtils::begins_with(property.name,current_array_prefix)) {
                    // a new field or indexed field.
                    Vector<StringView> parts;
                    String::split_ref(parts,property.name,"/");
                    if(this_prop_idx==0) { // build the array description only from index 0 property definitions.
                        if(indexed_property.cname.empty())
                           indexed_property.cname = fixup_group_name(String(parts[0]));

                        PropertyInterface::TypedEntry e;
                        e.index = -2;
                        e.subfield_name = parts[2];
                        fill_type_info(property, e.entry_type);
                        e.setter = ClassDB::get_property_setter(type_cname, property.name);
                        e.getter = ClassDB::get_property_getter(type_cname, property.name);
                        accessor_methods[e.setter] = property.name;
                        accessor_methods[e.getter] = property.name;
                        indexed_property.indexed_entries.emplace_back(eastl::move(e));
                    }
                    continue;
                } else {
                    indexed_property.max_property_index = current_array_max_size;
                    itype.properties.emplace_back(eastl::move(indexed_property));
                    indexed_property = {};
                    current_array_max_size = 0;
                    current_array_prefix.clear();
                }
            }
            const bool auto_group=StringView(property.name).contains('/');
            if(auto_group) { // automatic group?
                StringView new_prefix = StringView(property.name).substr(0,StringView(property.name).find('/'));
                if(!indexed_property.cname.empty() && new_prefix!=StringView(current_group_prefix)) {
                    current_group_prefix.clear();
                    indexed_property.max_property_index = -2;
                    itype.properties.emplace_back(eastl::move(indexed_property));
                    indexed_property = {};
                }
                current_group_prefix = new_prefix;
                indexed_property.cname = fixup_group_name(String(new_prefix));

            }

            if(!indexed_property.cname.empty()) {
                if(StringUtils::begins_with(property.name,current_group_prefix)) {
                    // 2 cases ->
                    //      true group, defined by ADD_GROUP macro
                    //      automatic group, defined by common_name/field_name

                    // a new field or indexed field.

                    StringView field_name = StringView(property.name).substr(current_group_prefix.size());
                    if(auto_group) {
                        field_name = field_name.substr(1); // skip the leading '/'
                    }
                    PropertyInterface::TypedEntry e;
                    e.index = this_prop_idx;
                    e.subfield_name = field_name;
                    fill_type_info(property, e.entry_type);
                    e.setter = ClassDB::get_property_setter(type_cname, property.name);
                    e.getter = ClassDB::get_property_getter(type_cname, property.name);
                    accessor_methods[e.setter] = property.name;
                    accessor_methods[e.getter] = property.name;
                    indexed_property.indexed_entries.emplace_back(eastl::move(e));

                    continue;
                } else {
                    current_group_prefix.clear();
                    indexed_property.max_property_index = -2;
                    itype.properties.emplace_back(eastl::move(indexed_property));
                    indexed_property = {};
                }
            }

            last_prop_idx = this_prop_idx;
            this_prop_idx = ClassDB::get_property_index(type_cname, property.name, &valid);
            ERR_FAIL_COND_V(!valid, false);

            PropertyInterface iprop;
            iprop.cname = property.name.asCString();
            iprop.hint_str = property.hint_string;
            {
                PropertyInterface::TypedEntry e;
                e.setter = ClassDB::get_property_setter(type_cname, property.name);
                e.getter = ClassDB::get_property_getter(type_cname, property.name);
                e.index = this_prop_idx;
                fill_type_info(property, e.entry_type);
                iprop.indexed_entries.emplace_back(eastl::move(e));
            }
            iprop.max_property_index = this_prop_idx==-1 ? -1 : -2;

            if (!iprop.indexed_entries.back().setter.empty())
                accessor_methods[iprop.indexed_entries.back().setter] = iprop.cname;
            if (!iprop.indexed_entries.back().getter.empty())
                accessor_methods[iprop.indexed_entries.back().getter] = iprop.cname;
            //iprop.proxy_name = mapper->mapPropertyName(iprop.cname.toUtf8().data()).c_str();
            //iprop.proxy_name.replace("/", "__"); // Some members have a slash...
            itype.properties.push_back(iprop);
        }
        if(!current_array_prefix.empty()) { // we are in an array definition block. close it.
            indexed_property.max_property_index = current_array_max_size;
            itype.properties.emplace_back(eastl::move(indexed_property));
            indexed_property = {};
            current_array_max_size = 0;
            current_array_prefix.clear();
        }
        else if(!indexed_property.cname.empty()) { // we are in group definition block. close it.
            current_group_prefix.clear();
            indexed_property.max_property_index = -2;
            itype.properties.emplace_back(eastl::move(indexed_property));
            indexed_property = {};
        }
        // Populate methods

        Vector<MethodInfo> virtual_method_list;
        ClassDB::get_virtual_methods(type_cname, &virtual_method_list);

        Vector<MethodInfo> method_list;
        ClassDB::get_method_list(type_cname, &method_list, true);
        eastl::sort(method_list.begin(), method_list.end());
        for (const MethodInfo& method_info : method_list) {
            size_t argc = method_info.arguments.size();

            if (method_info.name.empty()) {
                continue;
            }
            auto cname = method_info.name;

            MethodInterface imethod { method_info.name.asCString() , {cname.asCString()} };

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
                imethod.return_type.is_enum = TypeRefKind::Enum;
            }
            else if (return_info.type != VariantType::INT && !return_info.class_name.empty()) {
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
                imethod.return_type.is_enum = return_info.type!=VariantType::ARRAY ? TypeRefKind::Simple : TypeRefKind::Array;
                imethod.return_type.cname = "PH:"+return_info.hint_string;
            }
            else if (return_info.type == VariantType::NIL) {
                if(return_info.usage & PROPERTY_USAGE_NIL_IS_VARIANT)
                imethod.return_type.cname = "Variant";
                else
                imethod.return_type.cname = "void";
            }
            else {
                if (return_info.type == VariantType::INT) {
                    if(return_info.hint==PropertyHint::IntIsObjectID || (m&&arg_meta[0]==GodotTypeInfo::METADATA_IS_ENTITY_ID)) {
                        imethod.return_type.cname = return_info.class_name;
                    }
                    else
                        imethod.return_type.cname = _get_int_type_name_from_meta(!arg_meta.empty() ? arg_meta[0] : GodotTypeInfo::METADATA_NONE).data();
                }
                else if (return_info.type == VariantType::FLOAT) {
                    imethod.return_type.cname = _get_float_type_name_from_meta(!arg_meta.empty() ? arg_meta[0] : GodotTypeInfo::METADATA_NONE).data();
                }
                else {
                    imethod.return_type.cname = Variant::interned_type_name(return_info.type).asCString();
                }
            }

            for (size_t i = 0; i < argc; i++) {
                const PropertyInfo& arginfo = method_info.arguments[i];

                StringName orig_arg_name = arginfo.name;

                ArgumentInterface iarg;
                fillArgInfoFromProperty(iarg,arginfo,
                                        arg_meta.size() > (i + 1) ? arg_meta[i + 1] : GodotTypeInfo::METADATA_NONE,
                                        arg_pass.size() > (i + 1) ? arg_pass[i + 1] : TypePassBy::Value);
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
                ivararg.name = "var_args";
                imethod.add_argument(ivararg);
            }

            //imethod.proxy_name = mapper->mapMethodName(qPrintable(imethod.name),qPrintable(itype.cname)).c_str();

            Map<String, String>::iterator accessor = accessor_methods.find(imethod.name);
            if (accessor != accessor_methods.end()) {
                //const PropertyInterface* accessor_property = itype.find_property_by_name(accessor->second);

                // We only deprecate an accessor method if it's in the same class as the property. It's easier this way, but also
                // we don't know if an accessor method in a different class could have other purposes, so better leave those untouched.
                imethod.implements_property = true;
            }

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

        // Populate signals
        static const HashMap<StringName, MethodInfo> dummy_signals;
        const HashMap<StringName, MethodInfo> * signal_map = ClassDB::get_signal_list(type_cname);
        if(!signal_map) {
            signal_map = &dummy_signals;
        }

        for(auto &e : *signal_map) {
            SignalInterface isignal;

            const MethodInfo &method_info = e.second;

            isignal.name = method_info.name;

            int argc = method_info.arguments.size();
            eastl::array<GodotTypeInfo::Metadata,20> fake_metadata;
            fake_metadata.fill(GodotTypeInfo::METADATA_NONE);
            for (const PropertyInfo &arginfo : method_info.arguments) {

                StringName orig_arg_name = arginfo.name;

                ArgumentInterface iarg;
                fillArgInfoFromProperty(iarg,arginfo, GodotTypeInfo::METADATA_NONE, TypePassBy::Value);
                isignal.add_argument(iarg);
            }

            itype.signals_.emplace_back(eastl::move(isignal));
        }

        // Populate enums and constants

        Vector<String> constants;
        ClassDB::get_integer_constant_list(type_cname, &constants, true);

        const HashMap<StringName, ClassDB_EnumDescriptor>& enum_map = class_iter->second.enum_map;
        const HashMap<StringName, int> &const_map(class_iter->second.constant_map);
        for (const auto& F : enum_map) {
            auto parts = StringUtils::split(F.first, "::");
            if (parts.size() > 1 && itype.name == parts[0]) {
                parts.pop_front(); // Skip leading type name, this will be fixed below
            }
            String enum_proxy_cname(parts.front().data(), parts.front().size());

            EnumInterface ienum(enum_proxy_cname);
            ienum.underlying_type = F.second.underlying_type;
            const Vector<StringName>& enum_constants = F.second.enumerators;
            for (const StringName& constant_cname : enum_constants) {
                String constant_name(constant_cname.asCString());
                auto value = const_map.find(constant_cname);
                ERR_FAIL_COND_V(value == const_map.end(), false);
                constants.erase_first_unsorted(constant_cname.asCString());

                ConstantInterface iconstant(constant_name, value->second);

                ienum.constants.push_back(iconstant);
            }

            itype.enums.push_back(ienum);

            TypeInterface enum_itype;
            enum_itype.is_enum = true;
            enum_itype.name = itype.name + "." + enum_proxy_cname;
            //enum_itype.cname = enum_itype.name;
            //enum_itype.proxy_name = itype.proxy_name + "." + enum_proxy_name;
            current_namespace.enum_types.emplace(enum_itype.name, enum_itype);
        }

        for (const String& constant_name : constants) {
            auto value = const_map.find(StringName(constant_name));
            ERR_FAIL_COND_V(value == const_map.end(), false);

            ConstantInterface iconstant(constant_name.c_str(), value->second);

            itype.constants.push_back(iconstant);
        }

        current_namespace.obj_types.emplace(itype.name, itype);

        class_list.pop_front();
    }

    return true;
}

void _populate_global_constants(ReflectionData &rd,ReflectionSource src) {
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

            current_namespace.enum_types.emplace(enum_itype.name, enum_itype);
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
        //assert(!StringView(enum_itype.name).contains("::"));
        current_namespace.enum_types.emplace(enum_itype.name, enum_itype);
    }
}
void _initialize_reflection_data(ReflectionData &rd, ReflectionSource src) {
    rd.doc = nullptr;

    rd.namespaces.clear();
    rd.namespaces.emplace_back();

    auto& current_namespace = rd.namespaces.back();
    current_namespace.name = "Godot";
    if(src==ReflectionSource::Editor) {
        rd.imports.emplace_back(ReflectionData::ImportedData {"GodotCore",VERSION_NUMBER});
        rd.module_name = "GodotEditor";
    }
    else
        rd.module_name = "GodotCore";
    rd.api_version = VERSION_NUMBER;
    rd.version = VERSION_NUMBER;
    ClassDB_APIType api_kind = src == ReflectionSource::Editor ? ClassDB_APIType::API_EDITOR : ClassDB_APIType::API_CORE;
    rd.api_hash = StringUtils::num_uint64(ClassDB::get_api_hash(api_kind),16);
    bool obj_type_ok = _populate_object_type_interfaces(rd,src);
    ERR_FAIL_COND_MSG(!obj_type_ok, "Failed to generate object type interfaces");

    if(src==ReflectionSource::Core) { // Only core registers the constants?
        _populate_global_constants(rd,src);
    }

}
#endif
