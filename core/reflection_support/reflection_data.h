/*************************************************************************/
/*  doc_data.h                                                           */
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

#pragma once

#include "core/hash_map.h"
#include "core/string.h"
#include "core/typesystem_decls.h"
#include "core/doc_support/doc_data.h"

class QJsonObject;
class VisitorInterface;
#define DEFAULT_NS "Godot"
// TODO: consider switching to flag enum for APIType
enum class APIType {
    Invalid = -1,
    Common=0,
    Editor,
    Client,
    Server,
};

enum class TypeRefKind : int8_t {
    Simple, //
    Enum,
    Array,
};

struct TypeReference {
    String cname;
    String template_argument = {};
    TypeRefKind is_enum = TypeRefKind::Simple;
    TypePassBy pass_by = TypePassBy::Value;

    void toJson(QJsonObject& obj) const;

    void fromJson(const QJsonObject& obj);
};

struct ConstantInterface {
    String name;
    TypeReference const_type = {"int"};
    int value;
    String str_value;

    ConstantInterface() = default;

    ConstantInterface(const String& p_name, int p_value) : name(p_name),value(p_value) {
    }
    ConstantInterface(const String& p_name, StringView p_value) : name(p_name),value(0), str_value(p_value) {
        const_type = {"String"};
    }
    void toJson(QJsonObject& obj) const;
    void fromJson(const QJsonObject& obj);
};

struct EnumInterface {
    String cname;
    String underlying_type;
    Vector<ConstantInterface> constants;

    _FORCE_INLINE_ bool operator==(const EnumInterface& p_ienum) const {
        return p_ienum.cname == cname;
    }

    EnumInterface() {}

    EnumInterface(const String& p_cname) : cname(p_cname) {
    }

    void toJson(QJsonObject& obj) const;
    void fromJson(const QJsonObject& obj);
};



struct PropertyInterface {
    String cname;
    String hint_str;
    int max_property_index; // -1 for plain properties, -2 for indexed properties, >0 for arrays of multiple properties it's the maximum number.
    struct TypedEntry {
        String subfield_name;
        TypeReference entry_type;
        int index;
        String setter;
        String getter;
    };
    Vector<TypedEntry> indexed_entries;


    void toJson(QJsonObject &obj) const;

    void fromJson(const QJsonObject &obj);

};

struct ArgumentInterface {
    enum DefaultParamMode {
        CONSTANT,
        NULLABLE_VAL,
        NULLABLE_REF
    };

    TypeReference type;

    String name;
    String default_argument;
    DefaultParamMode def_param_mode = CONSTANT;

    void toJson(QJsonObject &obj) const;

    void fromJson(const QJsonObject &obj);
};

struct SignalInterface {
    String name;

    Vector<ArgumentInterface> arguments;

    bool is_deprecated = false;
    String deprecation_message;

    void add_argument(const ArgumentInterface &argument) {
        arguments.push_back(argument);
    }

    void toJson(QJsonObject &obj) const;

    void fromJson(const QJsonObject &obj);
};

struct MethodInterface {
    String name;

    /**
     * Name of the C# method
     */
    //QString proxy_name;

    /**
     * [TypeInterface::name] of the return type
     */
    TypeReference return_type;

    /**
     * Determines if the method has a variable number of arguments (VarArg)
     */
    bool is_vararg = false;

    /**
     * Virtual methods ("virtual" as defined by the Godot API) are methods that by default do nothing,
     * but can be overridden by the user to add custom functionality.
     * e.g.: _ready, _process, etc.
     */
    bool is_virtual = false;

    /**
     * Determines if the call should fallback to Godot's object.Call(string, params) in C#.
     */
    bool requires_object_call = false;

    /**
     * Determines if the method visibility is 'internal' (visible only to files in the same assembly).
     * Currently, we only use this for methods that are not meant to be exposed,
     * but are required by properties as getters or setters.
     * Methods that are not meant to be exposed are those that begin with underscore and are not virtual.
     */
    bool is_internal = false;

    Vector<ArgumentInterface> arguments;

    bool is_deprecated = false;
    bool implements_property = false; // Set true on functions implementing a property.
    String deprecation_message;

    void add_argument(const ArgumentInterface& argument) {
        arguments.push_back(argument);
    }

    void toJson(QJsonObject &obj) const;

    void fromJson(const QJsonObject &obj);
};
struct TypeInterface {
    /**
     * Identifier name for this type.
     * Also used to format [c_out].
     */
    String name;
    String base_name;   //!< Identifier name of the base class.
    String header_path; //!< Relative path to header defining this type.

    APIType api_type;

    bool is_enum = false;
    bool is_object_type = false;
    bool is_singleton = false;
    bool is_reference = false;
    bool is_namespace = false;

    /**
     * Used only by Object-derived types.
     * Determines if this type is not abstract (incomplete).
     * e.g.: CanvasItem cannot be instantiated.
     */
    bool is_instantiable;

    /**
     * Used only by Object-derived types.
     * Determines if the C# class owns the native handle and must free it somehow when disposed.
     * e.g.: Reference types must notify when the C# instance is disposed, for proper refcounting.
     */
    bool memory_own;

    /**
     * This must be set to true for any struct bigger than 32-bits. Those cannot be passed/returned by value
     * with internal calls, so we must use pointers instead. Returns must be replace with out parameters.
     * In this case, [c_out] and [cs_out] must have a different format, explained below.
     * The Mono IL interpreter icall trampolines don't support passing structs bigger than 32-bits by value (at least not on WASM).
     */
    bool ret_as_byref_arg;
    /**
     * Marks this type as an opaque one, for Godot those are classes like Variant,NodePath,RID etc.
     * Some of the opaque types can contain helpers for the script side ( additional enums, constants etc. )
     */
    bool is_opaque_type = false;

    Vector<ConstantInterface> constants;
    Vector<EnumInterface> enums;
    Vector<PropertyInterface> properties;
    Vector<MethodInterface> methods;
    Vector<SignalInterface> signals_;

    const MethodInterface* find_method_by_name(const String& p_cname) const {
        for (const MethodInterface& E : methods) {
            if (E.name == p_cname)
                return &E;
        }

        return nullptr;
    }

    const PropertyInterface* find_property_by_name(const String& p_cname) const {
        for (const PropertyInterface& E : properties) {
            if (E.cname == p_cname)
                return &E;
        }

        return nullptr;
    }

public:
    static TypeInterface create_value_type(const String&p_name);

    static TypeInterface create_object_type(const String&p_cname, APIType p_api_type);

    TypeInterface();
    TypeInterface(String n) : TypeInterface() { name = n; }

    void toJson(QJsonObject &obj) const;

    void fromJson(const QJsonObject &obj);
};
struct NamespaceInterface {
    String name;
    String required_header;

    HashMap<String, TypeInterface> obj_types;
    Vector<EnumInterface> global_enums;
    Vector<ConstantInterface> global_constants;
    Vector<MethodInterface> global_functions; // functions exposed directly by this namespace

    HashMap<String, TypeInterface> placeholder_types;
    HashMap<String, TypeInterface> enum_types;

    const TypeInterface* _get_type_or_null(const TypeReference& p_typeref) const;

    void toJson(QJsonObject& obj) const;
    void fromJson(const QJsonObject& obj);
};
struct ReflectionData {
    //TODO: doc class is for a singular namespace!
    class DocData* doc=nullptr;
    String module_name;
    //! full reflection data version, should be >= api_version
    String version;
    //! supported api version.
    String api_version;
    //! Hash of the sourced reflection data.
    String api_hash;
    struct ImportedData {
        String module_name;
        String api_version;
        ReflectionData *resolved=nullptr;
    };
    // Contains imports required to process this ReflectionData.
    Vector<ImportedData> imports;
    Vector<NamespaceInterface> namespaces;

    const ConstantInterface* find_constant_by_name(const String &p_name, const Vector<ConstantInterface>& p_constants) const {
        for (const ConstantInterface& E : p_constants) {
            if (E.name == p_name)
                return &E;
        }

        return nullptr;
    }
    const TypeInterface *_get_type_or_placeholder(const TypeReference &p_typeref);

    [[nodiscard]] bool load_from_file(StringView os_path);
    [[nodiscard]] bool save_to_file(StringView os_path);
};
