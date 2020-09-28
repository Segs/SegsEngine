/*************************************************************************/
/*  doc_data.cpp                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "reflection_data.h"


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
#include <QCryptographicHash>

namespace  {

template<class T>
T valFromJson(const QJsonValue& v);

template<>
String valFromJson<String>(const QJsonValue& v) {
    return v.toString().toUtf8().data();
}
template<>
bool valFromJson<bool>(const QJsonValue& v) {
    return v.toBool();
}
template<>
TypeRefKind valFromJson<TypeRefKind>(const QJsonValue& v) {
    return TypeRefKind(v.toInt());
}
template<class T>
static void setJsonIfNonDefault(QJsonObject& obj, const char* field, const T& v) {
    if (v != T())
        obj[field] = v;
}

template<>
void setJsonIfNonDefault<String>(QJsonObject& obj, const char* field, const String& v) {
    if (!v.empty())
        obj[field] = v.c_str();
}

template<class T>
void getJsonOrDefault(const QJsonObject& obj, const char* field, T& v) {
    if (obj.contains(field))
        v = valFromJson<T>(obj[field]);
    else
        v = T();
}

template<>
void getJsonOrDefault<String>(const QJsonObject& obj, const char* field, String& v) {
    if (obj.contains(field))
        v = valFromJson<String>(obj[field]);
    else
        v = String();
}

template<class T>
void toJson(QJsonObject& tgt, const char* name, const Vector<T>& src) {
    if(src.empty())
        return;
    QJsonArray entries;
    for (const T& c : src) {
        QJsonObject field;
        c.toJson(field);
        entries.push_back(field);
    }
    tgt[name] = entries;

}
template<class T>
void fromJson(const QJsonObject& src, const char* name, Vector<T>& tgt) {
    if(!src.contains(name)) {
        tgt.clear();
        return;
    }
    assert(src[name].isArray());
    QJsonArray arr = src[name].toArray();
    tgt.reserve(arr.size());
    for (int i = 0; i < arr.size(); ++i) {
        T ci;
        ci.fromJson(arr[i].toObject());
        tgt.emplace_back(ci);
    }

}

}
void ConstantInterface::toJson(QJsonObject& obj) const {
    obj["name"] = name.c_str();
    obj["value"] = value;
}
void ConstantInterface::fromJson(const QJsonObject& obj) {
    name = obj["name"].toString().toUtf8().data();
    value = obj["value"].toInt();
}

void EnumInterface::toJson(QJsonObject& obj) const {
    obj["cname"] = cname.c_str();
    if(!underlying_type.empty() && underlying_type!="int32_t") {
        obj["underlying_type"] = underlying_type.c_str();
    }
    ::toJson(obj, "constants", constants);
}
void EnumInterface::fromJson(const QJsonObject& obj) {

    cname = obj["cname"].toString().toUtf8().data();
    if(obj.contains("underlying_type")) {
        underlying_type = obj["underlying_type"].toString().toUtf8().data();
    }
    else
        underlying_type = "int32_t";
    ::fromJson(obj, "constants", constants);
}


void PropertyInterface::toJson(QJsonObject &obj) const {
    obj["cname"] = cname.c_str();
    if(!hint_str.empty())
        obj["hint_string"] = hint_str.c_str();
    obj["max_property_index"] = max_property_index;
    QJsonArray prop_infos;
    if(max_property_index!=-1) {
        for(const auto & entry: indexed_entries) {
            QJsonObject entry_enc;
            entry_enc["name"] = entry.subfield_name.c_str();
            if(max_property_index==-2) // enum based properties -> BlendMode(val) ->  set((PropKind)1,val)
            {
                if(entry.index!=-1)
                    entry_enc["index"] = entry.index;
            }
            QJsonObject enc_type;
            entry.entry_type.toJson(enc_type);
            entry_enc["type"] = enc_type;
            setJsonIfNonDefault(entry_enc, "setter", entry.setter);
            setJsonIfNonDefault(entry_enc, "getter", entry.getter);

            prop_infos.push_back(entry_enc);
        }
    }
    else {
        QJsonObject entry_enc;
        QJsonObject enc_type;
        indexed_entries.front().entry_type.toJson(enc_type);
        entry_enc["type"] = enc_type;
        setJsonIfNonDefault(entry_enc, "setter", indexed_entries.front().setter);
        setJsonIfNonDefault(entry_enc, "getter", indexed_entries.front().getter);
        prop_infos.push_back(entry_enc);
    }
    obj["subfields"] = prop_infos;

}

void PropertyInterface::fromJson(const QJsonObject &obj) {
    cname = obj["cname"].toString().toUtf8().data();
    hint_str = obj["hint_string"].toString("").toUtf8().data();
    max_property_index = -1;

    max_property_index = obj["max_property_index"].toInt();

    QJsonArray subfields = obj["subfields"].toArray();
    indexed_entries.resize(subfields.size());
    int idx=0;
    for (auto& entry : indexed_entries) {
        QJsonObject entry_enc = subfields[idx++].toObject();
        entry.subfield_name = entry_enc["name"].toString().toUtf8().data();
        entry.index = -1;
        if (max_property_index == -2) // enum based properties -> BlendMode(val) ->  set((PropKind)1,val)
        {
            if(entry_enc.contains("index"))
                entry.index = entry_enc["index"].toInt();
        }

        entry.entry_type.fromJson(entry_enc["type"].toObject());

        getJsonOrDefault(entry_enc, "setter", entry.setter);
        getJsonOrDefault(entry_enc, "getter", entry.getter);
    }
}

void TypeReference::toJson(QJsonObject &obj) const {
    obj["cname"] = cname.c_str();
    setJsonIfNonDefault(obj, "is_enum", (int8_t)is_enum);
    if (pass_by != TypePassBy::Value)
        obj["pass_by"] = (int8_t)pass_by;
}

void TypeReference::fromJson(const QJsonObject &obj) {
    cname = obj["cname"].toString().toUtf8().data();

    getJsonOrDefault(obj, "is_enum", is_enum);

    if (obj.contains("pass_by"))
        pass_by = (TypePassBy)obj["pass_by"].toInt();
    else
        pass_by = TypePassBy::Value;
}

void ArgumentInterface::toJson(QJsonObject &obj) const {
    QJsonObject sertype;
    type.toJson(sertype);

    obj["type"] = sertype;
    obj["name"] = name.c_str();
    if(!default_argument.empty())
        obj["default_argument"] = default_argument.c_str();
    if (def_param_mode != CONSTANT)
        obj["def_param_mode"] = def_param_mode;
}

void ArgumentInterface::fromJson(const QJsonObject &obj) {
    type.fromJson(obj["type"].toObject());
    name = obj["name"].toString().toUtf8().data();

    getJsonOrDefault(obj, "default_argument", default_argument);

    if (obj.contains("def_param_mode"))
        def_param_mode = (DefaultParamMode)obj["def_param_mode"].toInt();
    else
        def_param_mode = CONSTANT;

}
void SignalInterface::fromJson(const QJsonObject &obj) {
    name = obj["name"].toString().toUtf8().data();
    ::fromJson(obj, "arguments", arguments);
    getJsonOrDefault(obj, "is_deprecated", is_deprecated);
    getJsonOrDefault(obj, "deprecation_message", deprecation_message);
}

void SignalInterface::toJson(QJsonObject &obj) const {
    obj["name"] = name.c_str();
    ::toJson(obj, "arguments", arguments);
    setJsonIfNonDefault(obj, "is_deprecated", is_deprecated);
    setJsonIfNonDefault(obj, "deprecation_message", deprecation_message);
}

void MethodInterface::toJson(QJsonObject &obj) const {
    QJsonObject sertype;
    return_type.toJson(sertype);

    obj["name"] = name.c_str();
    obj["return_type"] = sertype;

    setJsonIfNonDefault(obj,"is_vararg", is_vararg);
    setJsonIfNonDefault(obj, "is_virtual", is_virtual);
    setJsonIfNonDefault(obj, "requires_object_call", requires_object_call);
    setJsonIfNonDefault(obj, "is_internal", is_internal);

    ::toJson(obj, "arguments", arguments);

    setJsonIfNonDefault(obj, "is_deprecated", is_deprecated);
    setJsonIfNonDefault(obj, "implements_property", implements_property);
    setJsonIfNonDefault(obj, "deprecation_message", deprecation_message);

}

void MethodInterface::fromJson(const QJsonObject &obj) {

    name = obj["name"].toString().toUtf8().data();
    return_type.fromJson(obj["return_type"].toObject());

    getJsonOrDefault(obj,"is_vararg", is_vararg);
    getJsonOrDefault(obj, "is_virtual", is_virtual);
    getJsonOrDefault(obj, "requires_object_call", requires_object_call);
    getJsonOrDefault(obj, "is_internal", is_internal);


    ::fromJson(obj, "arguments", arguments);

    getJsonOrDefault(obj, "is_deprecated", is_deprecated);
    getJsonOrDefault(obj, "implements_property", implements_property);
    getJsonOrDefault(obj, "deprecation_message", deprecation_message);

}
TypeInterface TypeInterface::create_value_type(const String &p_name) {
    TypeInterface itype;
    itype.name = p_name;
    //itype.cname = (p_name);
    //_init_value_type(itype);
    return itype;
}

TypeInterface TypeInterface::create_object_type(const String&p_cname, APIType p_api_type) {
    TypeInterface itype;

    itype.name = p_cname;
    //itype.cname = p_cname;
    //itype.proxy_name = itype.name.startsWith("_") ? itype.name.mid(1) : itype.name;
    itype.api_type = p_api_type;
    itype.is_object_type = true;
    //itype.class_doc = &g_doc_data->class_list[itype.proxy_name];

    return itype;
}

TypeInterface::TypeInterface() {

    api_type = APIType::Common;

    is_enum = false;
    is_object_type = false;
    is_singleton = false;
    is_reference = false;
    is_instantiable = false;

    memory_own = false;

    ret_as_byref_arg = false;

    //c_arg_in = "%s";
}

void TypeInterface::toJson(QJsonObject &obj) const {
    obj["name"] = name.c_str();
    obj["base_name"] = base_name.c_str();
    obj["header_path"] = header_path.c_str();
    obj["api_type"] = (int)api_type;
    if(is_enum)
        obj["is_enum"] = is_enum;
    if(is_object_type)
        obj["is_object_type"] = is_object_type;
    if(is_singleton)
        obj["is_singleton"] = is_singleton;
    if(is_reference)
        obj["is_reference"] = is_reference;
    if(is_namespace)
        obj["is_namespace"] = true;
    if(is_opaque_type)
        obj["is_opaque_type"] = true;
    if(is_instantiable)
        obj["is_instantiable"] = is_instantiable;
    if(memory_own)
        obj["memory_own"] = memory_own;
    if(ret_as_byref_arg)
        obj["ret_as_byref_arg"] = ret_as_byref_arg;

    if(!constants.empty())
        ::toJson(obj, "constants", constants);
    if(!enums.empty())
        ::toJson(obj, "enums", enums);
    if(!properties.empty())
        ::toJson(obj, "properties", properties);
    if(!methods.empty())
        ::toJson(obj, "methods", methods);
    if(!signals_.empty())
        ::toJson(obj, "signals", signals_);

}

void TypeInterface::fromJson(const QJsonObject &obj) {
    name = obj["name"].toString().toUtf8().data();
    base_name = obj["base_name"].toString().toUtf8().data();
    header_path = obj["header_path"].toString().toUtf8().data();

    api_type = (APIType)obj["api_type"].toInt();
    is_enum = obj["is_enum"].toBool(false);
    is_object_type = obj["is_object_type"].toBool(false);
    is_singleton = obj["is_singleton"].toBool(false);
    is_reference = obj["is_reference"].toBool(false);
    is_namespace = obj["is_namespace"].toBool(false);
    is_instantiable = obj["is_instantiable"].toBool(false);
    is_opaque_type = obj["is_opaque_type"].toBool(false);
    memory_own = obj["memory_own"].toBool(false);
    ret_as_byref_arg = obj["ret_as_byref_arg"].toBool(false);
    if(obj.contains("constants"))
        ::fromJson(obj,"constants",constants);
    if(obj.contains("enums"))
        ::fromJson(obj, "enums", enums);
    if(obj.contains("properties"))
        ::fromJson(obj, "properties", properties);
    if(obj.contains("methods"))
        ::fromJson(obj, "methods", methods);
    if(obj.contains("signals"))
        ::fromJson(obj, "signals", signals_);
}

const TypeInterface * NamespaceInterface::_get_type_or_null(const TypeReference &p_typeref) const {

    if(p_typeref.cname.empty())
        return nullptr;
    const auto obj_type_match = obj_types.find(p_typeref.cname);

    if (obj_type_match != obj_types.end())
        return &obj_type_match->second;

    if (p_typeref.is_enum==TypeRefKind::Enum) {
        auto enum_match = enum_types.find(p_typeref.cname);

        if (enum_match != enum_types.end())
            return &enum_match->second;
        enum_match = enum_types.find(p_typeref.cname + "Enum");

        if (enum_match != enum_types.end())
            return &enum_match->second;
    }

    assert(false);
    return nullptr;
}

void NamespaceInterface::toJson(QJsonObject &obj) const {

    QJsonObject root_obj;

    QJsonArray enum_arr;
    for (const auto& v : global_enums) {
        QJsonObject entry;
        v.toJson(entry);
        enum_arr.push_back(entry);
    }
    root_obj["global_enums"] = enum_arr;

    QJsonArray globals_arr;
    for (const auto& v : global_constants) {
        QJsonObject entry;
        v.toJson(entry);
        globals_arr.push_back(entry);
    }
    root_obj["global_constants"] = globals_arr;

    QJsonArray global_funcs_arr;
    for (const auto& v : global_functions) {
        QJsonObject entry;
        v.toJson(entry);
        global_funcs_arr.push_back(entry);
    }
    root_obj["global_functions"] = global_funcs_arr;

    QJsonArray types_arr;
    for (const auto& type : obj_types) {

        QJsonObject entry;
        type.second.toJson(entry);
        types_arr.push_back(entry);
    }
    root_obj["obj_types"] = types_arr;

    obj["name"] = namespace_name.c_str();
    obj["required_header"] = required_header.c_str();
    obj["namespace_contents"] = root_obj;
}

void NamespaceInterface::fromJson(const QJsonObject &obj)
{
    namespace_name = obj["name"].toString().toUtf8().data();
    required_header = obj["required_header"].toString().toUtf8().data();
    QJsonObject root_obj = obj["namespace_contents"].toObject();

    QJsonArray enum_arr = root_obj["global_enums"].toArray();
    global_enums.reserve(enum_arr.size());

    for (const auto& v : enum_arr) {
        EnumInterface entry;
        entry.fromJson(v.toObject());
        global_enums.emplace_back(eastl::move(entry));
    }

    QJsonArray globals_arr = root_obj["global_constants"].toArray();
    global_constants.reserve(globals_arr.size());
    for (const auto& v : globals_arr) {
        ConstantInterface global;
        global.fromJson(v.toObject());
        global_constants.emplace_back(eastl::move(global));
    }

    QJsonArray global_funcs_arr = root_obj["global_functions"].toArray();
    global_constants.reserve(global_funcs_arr.size());
    for (const auto& v : global_funcs_arr) {
        MethodInterface global;
        global.fromJson(v.toObject());
        global_functions.emplace_back(eastl::move(global));
    }


    QJsonArray types_arr = root_obj["obj_types"].toArray();
    obj_types.reserve(types_arr.size());

    for (const auto& type : types_arr) {

        QJsonObject entry;
        TypeInterface type_iface;
        type_iface.fromJson(type.toObject());
        obj_types[type_iface.name] = eastl::move(type_iface);
    }

}

bool ReflectionData::load_from_file(StringView os_path) {
    QFile inFile(QLatin1String(os_path.data(), os_path.size()));
    if(!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QByteArray data = inFile.readAll();
    inFile.close();

    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }
    QJsonObject root = doc.object();
    module_name = qPrintable(root.value("module_name").toString());
    api_version = qPrintable(root.value("api_version").toString());
    version = qPrintable(root.value("version").toString());
    if(version.empty())
        version = api_version;

    QByteArray normalized(data);
    normalized.replace('\r',"");
    normalized.replace('\n',"");
    api_hash = qPrintable(root.value("api_hash").toString());
    //api_hash = QCryptographicHash::hash(normalized, QCryptographicHash::Sha256).toHex().data();

    QJsonArray dependencies_arr(root["dependencies"].toArray());
    for (const auto &v : dependencies_arr) {
        QJsonObject val(v.toObject());
        ImportedData id;
        id.module_name = qPrintable(val.value("name").toString());
        id.api_version = qPrintable(val.value("api_version").toString());
        imports.emplace_back(eastl::move(id));
    }

    QJsonArray namespaces_arr(root["namespaces"].toArray());
    for (const auto &v : namespaces_arr) {
        NamespaceInterface ni;
        ni.fromJson(v.toObject());
        namespaces.emplace_back(eastl::move(ni));
    }
    return true;
}

bool ReflectionData::save_to_file(StringView os_path) {

    QFile inFile(QLatin1String(os_path.data(), os_path.size()));
    if(!inFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QJsonObject root;
    root["module_name"] = module_name.c_str();
    root["api_version"] = api_version.c_str();
    root["api_hash"] = api_hash.c_str();
    root["version"] = version.c_str();

    QJsonArray dependencies;
    for(const auto & v : imports) {
        QJsonObject dependency;
        dependency["name"] = v.module_name.c_str();
        dependency["api_version"] = v.api_version.c_str();
        dependencies.push_back(dependency);
    }
    root["dependencies"] = dependencies;

    QJsonArray j_namespaces;
    for(const auto & v : namespaces) {
        QJsonObject ns_data;
        v.toJson(ns_data);
        j_namespaces.push_back(ns_data);
    }
    root["namespaces"] = j_namespaces;

    QByteArray content=QJsonDocument(root).toJson();
    inFile.write(content);
    return true;
}

