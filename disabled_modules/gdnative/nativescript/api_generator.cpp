/*************************************************************************/
/*  api_generator.cpp                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "api_generator.h"

#ifdef TOOLS_ENABLED

#include "core/class_db.h"
#include "core/engine.h"
#include "core/global_constants.h"
#include "core/os/file_access.h"
#include "core/property_info.h"
#include "core/method_info.h"
#include "core/method_bind_interface.h"
#include "core/pair.h"
#include "core/ustring.h"
#include "core/se_string.h"

#include "EASTL/sort.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
// helper stuff
static const char *indent(int n) {
    static char buf[16];
    static int prev=-1;
    if(n==prev)
        return buf;
    memset(buf,'\t',std::min(16,n));
    buf[15]=0;
    buf[n] = 0;
    prev = n;
    return buf;
}
static Error save_file(const String &p_path, se_string &p_content) {

    FileAccessRef file = FileAccess::open(p_path, FileAccess::WRITE);

    ERR_FAIL_COND_V(!file, ERR_FILE_CANT_WRITE)

    file->store_string(String(p_content));

    file->close();

    return OK;
}

// helper stuff end

struct MethodAPI {
    se_string method_name;
    se_string return_type;

    List<se_string> argument_types;
    List<se_string> argument_names;

    Map<int, Variant> default_arguments;

    int argument_count;
    bool has_varargs;
    bool is_editor;
    bool is_noscript;
    bool is_const;
    bool is_reverse;
    bool is_virtual;
    bool is_from_script;
};

struct PropertyAPI {
    se_string name;
    se_string getter;
    se_string setter;
    se_string type;
    int index;
};

struct ConstantAPI {
    se_string constant_name;
    int constant_value;
};

struct SignalAPI {
    se_string name;
    PODVector<se_string> argument_types;
    PODVector<se_string> argument_names;
    Map<int, Variant> default_arguments;
};

struct EnumAPI {
    se_string_view name;
    PODVector<Pair<int, se_string_view> > values;
};

struct ClassAPI {
    se_string class_name;
    se_string super_class_name;

    ClassDB::APIType api_type;

    bool is_singleton;
    bool is_instanciable;
    // @Unclear
    bool is_reference;

    PODVector<MethodAPI> methods;
    PODVector<PropertyAPI> properties;
    PODVector<ConstantAPI> constants;
    PODVector<SignalAPI> signals_;
    PODVector<EnumAPI> enums;
};

static se_string get_type_name(const PropertyInfo &info) {
    if (info.type == VariantType::INT && (info.usage & PROPERTY_USAGE_CLASS_IS_ENUM)) {
        return "enum." + se_string(info.class_name.asCString()).replaced(".", "::");
    }
    if (info.class_name != StringName()) {
        return info.class_name.asCString();
    }
    if (info.hint == PROPERTY_HINT_RESOURCE_TYPE) {
        return StringUtils::to_utf8(info.hint_string).data();
    }
    if (info.type == VariantType::NIL && (info.usage & PROPERTY_USAGE_NIL_IS_VARIANT)) {
        return "Variant";
    }
    if (info.type == VariantType::NIL) {
        return "void";
    }
    return Variant::get_type_name(info.type);
}

/*
 * Some comparison helper functions we need
 */

struct MethodInfoComparator {
    bool operator()(const MethodInfo &p_a, const MethodInfo &p_b) const {

        return StringName::AlphCompare(p_a.name, p_b.name);
    }
};

struct PropertyInfoComparator {
    bool operator()(const PropertyInfo &p_a, const PropertyInfo &p_b) const {

        return StringName::AlphCompare(p_a.name, p_b.name);
    }
};

struct ConstantAPIComparator {
//    NoCaseComparator compare;
    bool operator()(const ConstantAPI &p_a, const ConstantAPI &p_b) const {

        return p_a.constant_name.comparei(p_b.constant_name)<0;
    }
};

/*
 * Reads the entire Godot API to a list
 */
List<ClassAPI> generate_c_api_classes() {

    List<ClassAPI> api;

    Vector<StringName> classes;
    ClassDB::get_class_list(&classes);

    std::sort(classes.ptrw(),classes.ptrw()+classes.size(),StringName::AlphCompare);

    // Register global constants as a fake GlobalConstants singleton class
    {
        ClassAPI global_constants_api;
        global_constants_api.class_name = "GlobalConstants";
        global_constants_api.api_type = ClassDB::API_CORE;
        global_constants_api.is_singleton = true;
        global_constants_api.is_instanciable = false;
        const int constants_count = GlobalConstants::get_global_constant_count();
        for (int i = 0; i < constants_count; ++i) {
            ConstantAPI constant_api;
            constant_api.constant_name = GlobalConstants::get_global_constant_name(i);
            constant_api.constant_value = GlobalConstants::get_global_constant_value(i);
            global_constants_api.constants.push_back(constant_api);
        }
        eastl::sort(global_constants_api.constants.begin(),global_constants_api.constants.end(),ConstantAPIComparator());

        api.push_back(global_constants_api);
    }

    for (int i=0,fin=classes.size(); i<fin; ++i) {
        StringName class_name = classes[i];

        ClassAPI class_api;
        class_api.api_type = ClassDB::get_api_type(classes[i]);
        class_api.class_name = class_name.asCString();
        class_api.super_class_name = ClassDB::get_parent_class(class_name).asCString();
        {
            String name = class_name;
            if (StringUtils::begins_with(name,"_")) {
                StringUtils::erase(name,0,1);
            }
            class_api.is_singleton = Engine::get_singleton()->has_singleton(name);
        }
        class_api.is_instanciable = !class_api.is_singleton && ClassDB::can_instance(class_name);

        {
            ListPOD<StringName> inheriters;
            ClassDB::get_inheriters_from_class("Reference", &inheriters);
            bool is_reference = inheriters.contains(class_name);
            // @Unclear
            class_api.is_reference = !class_api.is_singleton && is_reference;
        }

        // constants
        {
            ListPOD<se_string> constant;
            ClassDB::get_integer_constant_list(class_name, &constant, true);
            constant.sort(NoCaseComparator());
            for (const se_string &c : constant) {
                ConstantAPI constant_api;
                constant_api.constant_name = StringUtils::to_utf8(c).data();
                constant_api.constant_value = ClassDB::get_integer_constant(class_name, StringName(c));

                class_api.constants.push_back(constant_api);
            }
        }

        // signals
        {
            ListPOD<MethodInfo> signals_;
            ClassDB::get_signal_list(class_name, &signals_, true);
            signals_.sort(MethodInfoComparator());

            for (const MethodInfo &method_info : signals_) {
                SignalAPI signal;

                signal.name = method_info.name.asCString();

                for (const PropertyInfo &argument : method_info.arguments) {
                    se_string type;
                    se_string name = argument.name.asCString();

                    if (StringUtils::contains(argument.name,':')) {
                        type = StringUtils::get_slice(argument.name.asCString(),':', 1);
                        name = StringUtils::get_slice(argument.name.asCString(),':', 0);
                    } else {
                        type = get_type_name(argument);
                    }

                    signal.argument_names.push_back(name);
                    signal.argument_types.push_back(type);
                }

                const PODVector<Variant> &default_arguments(method_info.default_arguments);

                int default_start = signal.argument_names.size() - default_arguments.size();

                for (size_t j = 0; j < default_arguments.size(); j++) {
                    signal.default_arguments[default_start + j] = default_arguments[j];
                }

                class_api.signals_.push_back(signal);
            }
        }

        //properties
        {
            ListPOD<PropertyInfo> properties;
            ClassDB::get_property_list(class_name, &properties, true);
            properties.sort(PropertyInfoComparator());

            for (const PropertyInfo &p : properties) {
                PropertyAPI property_api;

                property_api.name = p.name.asCString();
                property_api.getter = ClassDB::get_property_getter(class_name, p.name).asCString();
                property_api.setter = ClassDB::get_property_setter(class_name, p.name).asCString();

                if( StringUtils::contains(p.name,':') ) {
                    property_api.type = StringUtils::get_slice(p.name.asCString(),':', 1);
                    property_api.name = StringUtils::get_slice(p.name.asCString(),':', 0);
                } else {
                    property_api.type = get_type_name(p);
                }

                property_api.index = ClassDB::get_property_index(class_name, p.name);

                if (!property_api.setter.empty() || !property_api.getter.empty()) {
                    class_api.properties.push_back(property_api);
                }
            }
        }

        //methods
        {
            PODVector<MethodInfo> methods;
            ClassDB::get_method_list(class_name, &methods, true);
            eastl::sort(methods.begin(),methods.end(),MethodInfoComparator());

            for (MethodInfo &method_info : methods) {
                MethodAPI method_api;
                MethodBind *method_bind = ClassDB::get_method(class_name, method_info.name);

                //method name
                method_api.method_name = method_info.name.asCString();
                //method return type
                if( StringUtils::contains(method_api.method_name,':') ) {
                    method_api.return_type = StringUtils::get_slice(method_api.method_name,':', 1);
                    method_api.method_name = StringUtils::get_slice(method_api.method_name,':', 0);
                } else {
                    method_api.return_type = get_type_name(method_info.return_val);
                }

                method_api.argument_count = method_info.arguments.size();
                method_api.has_varargs = method_bind && method_bind->is_vararg();

                // Method flags
                method_api.is_virtual = false;
                if (method_info.flags) {
                    const uint32_t flags = method_info.flags;
                    method_api.is_editor = flags & METHOD_FLAG_EDITOR;
                    method_api.is_noscript = flags & METHOD_FLAG_NOSCRIPT;
                    method_api.is_const = flags & METHOD_FLAG_CONST;
                    method_api.is_reverse = flags & METHOD_FLAG_REVERSE;
                    method_api.is_virtual = flags & METHOD_FLAG_VIRTUAL;
                    method_api.is_from_script = flags & METHOD_FLAG_FROM_SCRIPT;
                }

                method_api.is_virtual = method_api.is_virtual || method_api.method_name[0] == '_';

                // method argument name and type

                for (int i = 0; i < method_api.argument_count; i++) {
                    se_string arg_name;
                    se_string arg_type;
                    PropertyInfo arg_info = method_info.arguments[i];

                    arg_name = arg_info.name.asCString();

                    if (StringUtils::contains(arg_info.name,':') ) {
                        arg_type = StringUtils::get_slice(arg_info.name.asCString(),':', 1);
                        arg_name = StringUtils::get_slice(arg_info.name.asCString(),':', 0);
                    } else if (arg_info.hint == PROPERTY_HINT_RESOURCE_TYPE) {
                        arg_type = StringUtils::to_utf8(arg_info.hint_string).data();
                    } else if (arg_info.type == VariantType::NIL) {
                        arg_type = "Variant";
                    } else if (arg_info.type == VariantType::OBJECT) {
                        arg_type = arg_info.class_name.asCString();
                        if (arg_type.empty()) {
                            arg_type = Variant::get_type_name(arg_info.type);
                        }
                    } else {
                        arg_type = Variant::get_type_name(arg_info.type);
                    }

                    method_api.argument_names.push_back(arg_name);
                    method_api.argument_types.push_back(arg_type);

                    if (method_bind && method_bind->has_default_argument(i)) {
                        method_api.default_arguments[i] = method_bind->get_default_argument(i);
                    }
                }

                class_api.methods.push_back(method_api);
            }
        }

        // enums
        {
            List<EnumAPI> enums;
            ListPOD<StringName> enum_names;
            ClassDB::get_enum_list(class_name, &enum_names, true);
            for(const StringName & E : enum_names) {
                ListPOD<StringName> value_names;
                EnumAPI enum_api;
                enum_api.name = E.asCString();
                ClassDB::get_enum_constants(class_name, E, &value_names, true);
                for(const StringName & val_e : value_names) {
                    int int_val = ClassDB::get_integer_constant(class_name, val_e, nullptr);
                    enum_api.values.push_back(Pair<int, se_string>(int_val, val_e.asCString()));
                }
                eastl::sort(enum_api.values.begin(),enum_api.values.end(),
                            [](const Pair<int, se_string_view> &A, const Pair<int, se_string_view> &B)->bool { return A.first<B.first;});
                class_api.enums.push_back(enum_api);
            }
        }

        api.push_back(class_api);
    }

    return api;
}

/*
 * Generates the JSON source from the API in p_api
 */
static se_string generate_c_api_json(const List<ClassAPI> &p_api) {

    using namespace StringUtils;
    QJsonDocument doc;
    QJsonArray array;
    for (const List<ClassAPI>::Element *c = p_api.front(); c != nullptr; c = c->next()) {
        QJsonObject sb;
        ClassAPI api = c->deref();
        sb["name"] = api.class_name.c_str();
        sb["base_class"] = api.super_class_name.c_str();
        sb["api_type"] = api.api_type == ClassDB::API_CORE ? "core" : (api.api_type == ClassDB::API_EDITOR ? "tools" : "none");
        sb["singleton"] = api.is_singleton;
        sb["instanciable"] = api.is_instanciable;
        sb["is_reference"] = api.is_reference;
        QJsonObject constants;
        for (const ConstantAPI &e : api.constants) {
            constants[e.constant_name.data()] = e.constant_value;
        }
        sb["constants"] = constants;

        QJsonArray properties;
        for (const PropertyAPI &e : api.properties) {
            QJsonObject prop;
            prop["name"] = e.name.c_str();
            prop["type"] = e.type.c_str();
            prop["getter"] = e.getter.c_str();
            prop["setter"] = e.setter.c_str();
            prop["index"] = e.index;
            properties.append(prop);
        }
        sb["properties"] = properties;

        QJsonArray def_signals;
        for (const SignalAPI &e : api.signals_) {
            QJsonObject sig;
            sig["name"] = e.name.c_str();
            QJsonArray args;
            for (int i = 0; i < e.argument_names.size(); i++) {
                QJsonObject argval;
                argval["name"] = e.argument_names[i].c_str();
                argval["type"] = e.argument_types[i].c_str();
                argval["default_value"] = e.default_arguments.contains(i) ? to_utf8((String)e.default_arguments.at(i)).data() : "";
                args.append(argval);
            }
            sig["arguments"] = args;
            def_signals.append(sig);
        }
        sb["signals"] = def_signals;

        QJsonArray methods;
        for (const MethodAPI &e : api.methods) {
            QJsonObject method;
            method["name"] = e.method_name.data();
            method["return_type"] = e.return_type.data();
            method["is_editor"] = e.is_editor;
            method["is_noscript"] = e.is_noscript;
            method["is_const"] = e.is_const;
            method["is_reverse"] = e.is_reverse;
            method["is_virtual"] = e.is_virtual;
            method["has_varargs"] = e.has_varargs;
            method["is_from_script"] = e.is_from_script;
            QJsonArray args;
            for (int i = 0; i < e.argument_names.size(); i++) {
                QJsonObject arg;
                arg["name"] = e.argument_names[i].data();
                arg["type"] = e.argument_types[i].data();
                arg["has_default_value"] = e.default_arguments.contains(i);
                arg["default_value"] = e.default_arguments.contains(i) ? to_utf8((String)e.default_arguments.at(i)).data() : "";
                args.append(arg);
            }
            method["arguments"] = args;
        }
        sb["methods"] = methods;

        QJsonArray enums;
        for (const EnumAPI &e : api.enums) {
            QJsonObject edef;
            edef["name"] = e.name.data();
            QJsonObject evals;
            for (const Pair<int, se_string_view> &val_e : e.values) {
                evals[val_e.second.data()] = val_e.first;
            }
            edef["values"] = evals;
        }
        sb["enums"] = enums;

        array.append(sb);
    }
    doc.setArray(array);
    return doc.toJson(QJsonDocument::Indented).data();
}

#endif

/*
 * Saves the whole Godot API to a JSON file located at
 *  p_path
 */
Error generate_c_api(se_string_view p_path) {

#ifndef TOOLS_ENABLED
    return ERR_BUG;
#else

    List<ClassAPI> api = generate_c_api_classes();

    se_string json_source = generate_c_api_json(api);

    return save_file(p_path, json_source);
#endif
}
