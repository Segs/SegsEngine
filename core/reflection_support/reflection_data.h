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
#include "core/map.h"
#include "core/doc_support/doc_data.h"
#include "core/ustring.h"

#include <QString>
#include <QHash>
#include <QByteArray>

class QJsonObject;

enum class APIType {
    None,
    Core,
    Editor,
    User

};
struct ConstantInterface {
    QString name;
    //QString proxy_name;
    int value;

    ConstantInterface() {}

    ConstantInterface(const QString& p_name, int p_value) {
        name = p_name;
        //proxy_name = p_proxy_name;
        value = p_value;
    }
    void toJson(QJsonObject& obj) const;
    void fromJson(const QJsonObject& obj);
};

struct EnumInterface {
    QString cname;
    Vector<ConstantInterface> constants;

    _FORCE_INLINE_ bool operator==(const EnumInterface& p_ienum) const {
        return p_ienum.cname == cname;
    }

    EnumInterface() {}

    EnumInterface(const QString& p_cname) {
        cname = p_cname;
    }

    void toJson(QJsonObject& obj) const;
    void fromJson(const QJsonObject& obj);
};
struct TypeReference {
    QString cname;
    bool is_enum = false;
    TypePassBy pass_by = TypePassBy::Value;

    void toJson(QJsonObject& obj) const;

    void fromJson(const QJsonObject& obj);
};
struct PropertyInterface {
    QString cname;
    int max_property_index; // -1 for plain properties, -2 for indexed properties, >0 for arrays of multiple properties it's the maximum number.
    struct TypedEntry {
        QString subfield_name;
        TypeReference entry_type;
        int index;
        QString setter;
        QString getter;
    };
    QVector<TypedEntry> indexed_entries;


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

    QString name;
    QString default_argument;
    DefaultParamMode def_param_mode = CONSTANT;

    void toJson(QJsonObject &obj) const;

    void fromJson(const QJsonObject &obj);
};

struct MethodInterface {
    QString name;

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
    QString deprecation_message;

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
    QString name;
    /**
     * Identifier name of the base class.
     */
    QString base_name;

    /**
     * Name of the C# class
     */
    //QString proxy_name;

    APIType api_type;

    bool is_enum;
    bool is_object_type;
    bool is_singleton;
    bool is_reference;
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

    // !! The comments of the following fields make reference to other fields via square brackets, e.g.: [field_name]
    // !! When renaming those fields, make sure to rename their references in the comments

    // --- C INTERFACE ---

    static constexpr const char* DEFAULT_VARARG_C_IN = "\t%0 %1_in = Variant::from(%1);\n";

    /**
     * One or more statements that manipulate the parameter before being passed as argument of a ptrcall.
     * If the statement adds a local that must be passed as the argument instead of the parameter,
     * the name of that local must be specified with [c_arg_in].
     * For variadic methods, this field is required and, if empty, [DEFAULT_VARARG_C_IN] is used instead.
     * Formatting elements:
     * %0: [c_type] of the parameter
     * %1: name of the parameter
     */
    //QString c_in;

    /**
     * Determines the expression that will be passed as argument to ptrcall.
     * By default the value equals the name of the parameter,
     * this varies for types that require special manipulation via [c_in].
     * Formatting elements:
     * %0 or %s: name of the parameter
     */
    //QString c_arg_in;

    /**
     * One or more statements that determine how a variable of this type is returned from a function.
     * It must contain the return statement(s).
     * Formatting elements:
     * %0: [c_type_out] of the return type
     * %1: name of the variable to be returned
     * %2: [name] of the return type
     * ---------------------------------------
     * If [ret_as_byref_arg] is true, the format is different. Instead of using a return statement,
     * the value must be assigned to a parameter. This type of this parameter is a pointer to [c_type_out].
     * Formatting elements:
     * %0: [c_type_out] of the return type
     * %1: name of the variable to be returned
     * %2: [name] of the return type
     * %3: name of the parameter that must be assigned the return value
     */
    //QString c_out;

    /**
     * The actual expected type, as seen (in most cases) in Variant copy constructors
     * Used for the type of the return variable and to format [c_in].
     * The value must be the following depending of the type:
     * Object-derived types: Object*
     * Other types: [name]
     * -- Exceptions --
     * VarArg (fictitious type to represent variable arguments): Array
     * float: double (because ptrcall only supports double)
     * int: int64_t (because ptrcall only supports int64_t and uint64_t)
     * Reference types override this for the type of the return variable: Ref<RefCounted>
     */
    //QString c_type;

    /**
     * Determines the type used for parameters in function signatures.
     */
    //QString c_type_in;

    /**
     * Determines the return type used for function signatures.
     * Also used to construct a default value to return in case of errors,
     * and to format [c_out].
     */
    //QString c_type_out;

    // --- C# INTERFACE ---

    /**
     * An expression that overrides the way the parameter is passed to the internal call.
     * If empty, the parameter is passed as is.
     * Formatting elements:
     * %0 or %s: name of the parameter
     */
    //QString cs_in;

    /**
     * One or more statements that determine how a variable of this type is returned from a method.
     * It must contain the return statement(s).
     * Formatting elements:
     * %0: internal method name
     * %1: internal method call arguments without surrounding parenthesis
     * %2: [cs_type] of the return type
     * %3: [im_type_out] of the return type
     */
    //QString cs_out;

    /**
     * Type used for method signatures, both for parameters and the return type.
     * Same as [proxy_name] except for variable arguments (VarArg) and collections (which include the namespace).
     */
    //QString cs_type;

    /**
     * Type used for parameters of internal call methods.
     */
    //QString im_type_in;

    /**
     * Type used for the return type of internal call methods.
     */
    //QString im_type_out;

    Vector<ConstantInterface> constants;
    Vector<EnumInterface> enums;
    Vector<PropertyInterface> properties;
    Vector<MethodInterface> methods;

    const MethodInterface* find_method_by_name(const QString& p_cname) const {
        for (const MethodInterface& E : methods) {
            if (E.name == p_cname)
                return &E;
        }

        return nullptr;
    }

    const PropertyInterface* find_property_by_name(const QString& p_cname) const {
        for (const PropertyInterface& E : properties) {
            if (E.cname == p_cname)
                return &E;
        }

        return nullptr;
    }

    /*const PropertyInterface* find_property_by_proxy_name(const QString& p_proxy_name) const {
        for (const PropertyInterface& E : properties) {
            if (E.proxy_name == p_proxy_name)
                return &E;
        }

        return nullptr;
    }*/

private:
    static void _init_value_type(TypeInterface &itype);

public:
    static TypeInterface create_value_type(const QString&p_name);

    static TypeInterface create_object_type(const QString&p_cname, APIType p_api_type);

    static void create_placeholder_type(TypeInterface &r_itype, const QString&p_cname);

    static void postsetup_enum_type(TypeInterface &r_enum_itype);

    TypeInterface();

    void toJson(QJsonObject &obj) const;

    void fromJson(const QJsonObject &obj);
};
struct NamespaceInterface {
    QString namespace_name;
    QString required_header;

    HashMap<QString, TypeInterface> obj_types;
    Vector<QString> obj_type_insert_order;
    Vector<EnumInterface> global_enums;
    Vector<ConstantInterface> global_constants;

    Map<QString, TypeInterface> placeholder_types;
    Map<QString, TypeInterface> builtin_types;
    Map<QString, TypeInterface> enum_types;



    const TypeInterface* _get_type_or_null(const TypeReference& p_typeref);
    const TypeInterface* _get_type_or_placeholder(const TypeReference& p_typeref);

    void toJson(QJsonObject& obj) const;
    void fromJson(const QJsonObject& obj);
};
struct ReflectionData {
    class DocData* doc;

    struct ClassLookupHelper {
        HashMap<QString, const DocContents::MethodDoc*> methods;
        HashMap<QString, const DocContents::MethodDoc*> defined_signals;
        HashMap<QString, const DocContents::PropertyDoc*> properties;
        HashMap<QString, const DocContents::PropertyDoc*> theme_properties;

        HashMap<QString, const DocContents::ConstantDoc*> constantsz;
    };
    HashMap<QString, ClassLookupHelper> doc_lookup_helpers;
    void build_doc_lookup_helper();

    Vector<NamespaceInterface> namespaces;
    /*const DocContents::ConstantDoc* constant_doc(QString classname, QString enum_name, QString const_name) {
        return doc_lookup_helpers[classname].constantsz.at(enum_name + "::" + const_name, nullptr);
    }*/

    const TypeInterface *_get_type_or_null(const TypeReference &p_typeref);

    const ConstantInterface* find_constant_by_name(const QString &p_name, const Vector<ConstantInterface>& p_constants) const {
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