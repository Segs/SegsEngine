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

#include "doc_builder.h"
#include "core/doc_support/doc_data.h"

#include "core/engine.h"
#include "core/global_constants.h"
#include "core/io/compression.h"
#include "core/io/marshalls.h"
#include "core/method_bind_interface.h"
#include "core/print_string.h"
#include "core/os/dir_access.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "core/version.h"
#include "core/string_utils.h"
#include "core/string_formatter.h"
#include "scene/resources/theme.h"
#include "core/io/xml_parser.h"

#include "EASTL/sort.h"

//NOTE: this function is also used in doc_dump.cpp

static void return_doc_from_retinfo(DocContents::MethodDoc &p_method, const PropertyInfo &p_retinfo) {

    if (p_retinfo.type == VariantType::INT && p_retinfo.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
        p_method.return_enum = p_retinfo.class_name.asCString();
        if (StringUtils::begins_with(p_method.return_enum,"_")) //proxy class
            p_method.return_enum = StringUtils::substr(p_method.return_enum,1, p_method.return_enum.length());
        p_method.return_type = "int";
    } else if (p_retinfo.class_name != StringName()) {
        p_method.return_type = p_retinfo.class_name.asCString();
    } else if (p_retinfo.hint == PropertyHint::ResourceType) {
        p_method.return_type = p_retinfo.hint_string.c_str();
    } else if (p_retinfo.type == VariantType::NIL && p_retinfo.usage & PROPERTY_USAGE_NIL_IS_VARIANT) {
        p_method.return_type = "Variant";
    } else if (p_retinfo.type == VariantType::NIL) {
        p_method.return_type = "void";
    } else {
        p_method.return_type = Variant::get_type_name(p_retinfo.type);
    }
}

static void argument_doc_from_arginfo(DocContents::ArgumentDoc &p_argument, const PropertyInfo &p_arginfo) {

    p_argument.name = p_arginfo.name.asCString();

    if (p_arginfo.type == VariantType::INT && p_arginfo.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
        p_argument.enumeration = p_arginfo.class_name.asCString();
        if (StringUtils::begins_with(p_argument.enumeration,"_")) //proxy class
            p_argument.enumeration = StringUtils::substr(p_argument.enumeration,1, p_argument.enumeration.length());
        p_argument.type = "int";
    } else if (p_arginfo.class_name != StringName()) {
        p_argument.type = p_arginfo.class_name.asCString();
    } else if (p_arginfo.hint == PropertyHint::ResourceType) {
        p_argument.type = p_arginfo.hint_string.c_str();
    } else if (p_arginfo.type == VariantType::NIL) {
        // Parameters cannot be void, so PROPERTY_USAGE_NIL_IS_VARIANT is not necessary
        p_argument.type = "Variant";
    } else {
        p_argument.type = Variant::get_type_name(p_arginfo.type);
    }
}

static Variant get_documentation_default_value(const StringName &p_class_name, const StringName &p_property_name, bool &r_default_value_valid) {

    Variant default_value = Variant();
    r_default_value_valid = false;

    if (ClassDB::can_instance(p_class_name)) {
        default_value = ClassDB::class_get_default_property_value(p_class_name, p_property_name, &r_default_value_valid);
    } else {
        // Cannot get default value of classes that can't be instanced
        Vector<StringName> inheriting_classes;
        ClassDB::get_direct_inheriters_from_class(p_class_name, &inheriting_classes);
        for (const StringName &E2 : inheriting_classes) {
            if (ClassDB::can_instance(E2)) {
                default_value = ClassDB::class_get_default_property_value(E2, p_property_name, &r_default_value_valid);
                if (r_default_value_valid)
                    break;
            }
        }
    }

    return default_value;
}

void generate_docs_from_running_program(DocData &tgt,bool p_basic_types) {

    Vector<StringName> classes;
    ClassDB::get_class_list(&classes);
    eastl::sort(classes.begin(),classes.end(),WrapAlphaCompare());
    // Move ProjectSettings, so that other classes can register properties there.
    auto it=classes.find("ProjectSettings");
    classes.erase_unsorted(it);
    classes.push_back(StringName("ProjectSettings"));

    bool skip_setter_getter_methods = true;

    for(int i=0,fin=classes.size(); i<fin; ++i) {

        HashSet<StringName> setters_getters;

        StringName name = classes[i];
        if (!ClassDB::is_class_exposed(name)) {
            print_verbose(FormatVE("Class '%s' is not exposed, skipping.", name.asCString()));
            continue;
        }

        String cname(name);
        if (cname.starts_with('_')) //proxy class
            cname = cname.substr(1);

        tgt.class_list[cname] = DocContents::ClassDoc();
        DocContents::ClassDoc &c = tgt.class_list[cname];
        c.name = cname;
        c.inherits = ClassDB::get_parent_class(name).asCString();

        Vector<PropertyInfo> properties;
        Vector<PropertyInfo> own_properties;
        if (name == "ProjectSettings") {
            //special case for project settings, so settings can be documented
            ProjectSettings::get_singleton()->get_property_list(&properties);
            own_properties = properties;
        } else {
            ClassDB::get_property_list(name, &properties);
            ClassDB::get_property_list(name, &own_properties, true);
        }
        auto iter=own_properties.begin();
        for (const PropertyInfo & E : properties) {
            bool inherited = iter == own_properties.end();
            if (!inherited && *iter == E) {
                ++iter;
            }
            if (E.usage & PROPERTY_USAGE_GROUP || E.usage & PROPERTY_USAGE_CATEGORY || E.usage & PROPERTY_USAGE_INTERNAL)
                continue;

            DocContents::PropertyDoc prop;

            prop.name = E.name.asCString();
            prop.overridden = inherited;

            bool default_value_valid = false;
            Variant default_value;

            if (name == "ProjectSettings") {
                // Special case for project settings, so that settings are not taken from the current project's settings
                if (E.name == "script" ||
                        ProjectSettings::get_singleton()->get_order(E.name) >= ProjectSettings::NO_BUILTIN_ORDER_BASE) {
                    continue;
                }
                if (E.usage & PROPERTY_USAGE_EDITOR) {
                    default_value = ProjectSettings::get_singleton()->property_get_revert(E.name);
                    default_value_valid = true;
                }
            } else {
                default_value = get_documentation_default_value(name, E.name, default_value_valid);

                if (inherited) {
                    bool base_default_value_valid = false;
                    Variant base_default_value = get_documentation_default_value(ClassDB::get_parent_class(name), E.name, base_default_value_valid);
                    if (!default_value_valid || !base_default_value_valid || default_value == base_default_value)
                        continue;
                }
            }

            if (default_value_valid && default_value.get_type() != VariantType::OBJECT) {
                prop.default_value = StringUtils::replace(default_value.get_construct_string(),"\n", "").c_str();
            }

            StringName setter = ClassDB::get_property_setter(name, E.name);
            StringName getter = ClassDB::get_property_getter(name, E.name);

            prop.setter = setter.asCString();
            prop.getter = getter.asCString();

            bool found_type = false;
            if (getter != StringName()) {
                MethodBind *mb = ClassDB::get_method(name, getter);
                if (mb) {
                    #ifdef DEBUG_METHODS_ENABLED
                    PropertyInfo retinfo = mb->get_return_info();

                    found_type = true;
                    if (retinfo.type == VariantType::INT && retinfo.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
                        prop.enumeration = retinfo.class_name.asCString();
                        prop.type = "int";
                    } else if (retinfo.class_name != StringName()) {
                        prop.type = retinfo.class_name.asCString();
                    } else if (retinfo.hint == PropertyHint::ResourceType) {

                        prop.type = StringName(retinfo.hint_string).asCString();
                    } else if (retinfo.type == VariantType::NIL && retinfo.usage & PROPERTY_USAGE_NIL_IS_VARIANT) {

                        prop.type = "Variant";
                    } else if (retinfo.type == VariantType::NIL) {
                        prop.type = "void";
                    } else {
                        prop.type = Variant::interned_type_name(retinfo.type).asCString();
                    }
                    #endif
                }

                setters_getters.insert(getter);
            }

            if (setter != StringName()) {

                setters_getters.insert(setter);
            }

            if (!found_type) {

                if (E.type == VariantType::OBJECT && E.hint == PropertyHint::ResourceType)
                    prop.type = StringName(E.hint_string).asCString();
                else
                    prop.type = Variant::interned_type_name(E.type).asCString();
            }

            c.properties.push_back(prop);
        }

        Vector<MethodInfo> method_list;
        ClassDB::get_method_list(name, &method_list, true);
        eastl::sort(method_list.begin(),method_list.end());

        for (const MethodInfo &E : method_list) {

            if (E.name.empty() || E.name.asCString()[0] == '_' && !(E.flags & METHOD_FLAG_VIRTUAL))
                continue; //hidden, don't count

            if (skip_setter_getter_methods && setters_getters.contains(E.name)) {
                // Don't skip parametric setters and getters, i.e. method which require
                // one or more parameters to define what property should be set or retrieved.
                // E.g. CPUParticles3D::set_param(Parameter param, float value).
                if (E.arguments.empty() /* getter */ || E.arguments.size() == 1 && E.return_val.type == VariantType::NIL /* setter */) {
                    continue;
                }
            }

            DocContents::MethodDoc method;

            method.name = E.name.asCString();

            if (E.flags & METHOD_FLAG_VIRTUAL)
                method.qualifiers = "virtual";

            if (E.flags & METHOD_FLAG_CONST) {
                if (!method.qualifiers.empty())
                    method.qualifiers += ' ';
                method.qualifiers += "const";
            } else if (E.flags & METHOD_FLAG_VARARG) {
                if (!method.qualifiers.empty())
                    method.qualifiers += ' ';
                method.qualifiers += "vararg";
            }

#ifdef DEBUG_METHODS_ENABLED
            return_doc_from_retinfo(method, E.return_val);
#endif
            for (size_t i = 0; i < E.arguments.size(); i++) {

                const PropertyInfo &arginfo = E.arguments[i];

                DocContents::ArgumentDoc argument;

                argument_doc_from_arginfo(argument, arginfo);

                int darg_idx = int(i) - int(E.arguments.size() - E.default_arguments.size());

                if (darg_idx >= 0) {
                    Variant default_arg = E.default_arguments[darg_idx];
                    argument.default_value = default_arg.get_construct_string().c_str();
                }

                method.arguments.push_back(argument);
            }

            c.methods.push_back(method);
        }

        Vector<MethodInfo> signal_list;
        ClassDB::get_signal_list(name, &signal_list, true);

        if (!signal_list.empty()) {

            for (const MethodInfo &EV : signal_list) {

                DocContents::MethodDoc signal;
                signal.name = EV.name.asCString();
                for (const PropertyInfo &arginfo : EV.arguments) {

                    DocContents::ArgumentDoc argument;
                    argument_doc_from_arginfo(argument, arginfo);

                    signal.arguments.push_back(argument);
                }

                c.defined_signals.push_back(signal);
            }
        }

        List<String> constant_list;
        ClassDB::get_integer_constant_list(name, &constant_list, true);

        for (const String & E : constant_list) {

            DocContents::ConstantDoc constant;
            constant.name = E.c_str();
            constant.value = itos(ClassDB::get_integer_constant(name, StringName(E))).c_str();
            constant.enumeration = ClassDB::get_integer_constant_enum(name, StringName(E)).asCString();
            c.constants.push_back(constant);
        }

        //theme stuff
        StringName scname((cname));
        {
            Vector<StringName> l;
            Theme::get_default()->get_constant_list(scname, &l);
            for (const StringName &E : l) {

                DocContents::PropertyDoc pd;
                pd.name = E.asCString();
                pd.type = "int";
                pd.default_value = itos(Theme::get_default()->get_constant(E, scname)).c_str();
                c.theme_properties.push_back(pd);
            }

            l.clear();
            Theme::get_default()->get_color_list(scname, &l);
            for (const StringName &E : l) {

                DocContents::PropertyDoc pd;
                pd.name = E.asCString();
                pd.type = "Color";
                pd.default_value = Variant(Theme::get_default()->get_color(E, scname)).get_construct_string().c_str();
                c.theme_properties.push_back(pd);
            }

            l.clear();
            Theme::get_default()->get_icon_list(scname, &l);
            for (const StringName &E : l) {

                DocContents::PropertyDoc pd;
                pd.name = E.asCString();
                pd.type = "Texture";
                c.theme_properties.push_back(pd);
            }
            l.clear();
            Theme::get_default()->get_font_list(scname, &l);
            for (const StringName &E : l) {

                DocContents::PropertyDoc pd;
                pd.name = E.asCString();
                pd.type = "Font";
                c.theme_properties.push_back(pd);
            }
            l = Theme::get_default()->get_stylebox_list(scname);
            for (const StringName &E : l) {

                DocContents::PropertyDoc pd;
                pd.name = E.asCString();
                pd.type = "StyleBox";
                c.theme_properties.push_back(pd);
            }
        }
    }

    {
        // So we can document the concept of Variant even if it's not a usable class per se.
        tgt.class_list["Variant"] = DocContents::ClassDoc();
        tgt.class_list["Variant"].name = "Variant";
    }

    if (!p_basic_types)
        return;
    // Add Variant types.
    for (int i = 0; i < int(VariantType::VARIANT_MAX); i++) {

        if (VariantType(i) == VariantType::NIL)
            continue; // Not exposed outside of 'null', should not be in class list.

        if (VariantType(i) == VariantType::OBJECT)
            continue; // Use the core type instead.

        StringName cname(Variant::get_type_name(VariantType(i)));

        tgt.class_list[cname.asCString()] = DocContents::ClassDoc();
        DocContents::ClassDoc &c = tgt.class_list[cname.asCString()];
        c.name = cname.asCString();

        Callable::CallError cerror;
        Variant v = Variant::construct(VariantType(i), nullptr, 0, cerror);

        Vector<MethodInfo> method_list;
        //v.get_method_list(&method_list);
        //eastl::sort(method_list.begin(),method_list.end());
        Variant::get_constructor_list(VariantType(i), &method_list);

        for (MethodInfo &mi : method_list) {

            DocContents::MethodDoc method;

            method.name = mi.name.asCString();

            for (size_t j = 0; j < mi.arguments.size(); j++) {

                PropertyInfo arginfo(mi.arguments[j]);

                DocContents::ArgumentDoc ad;
                argument_doc_from_arginfo(ad, mi.arguments[j]);
                ad.name = arginfo.name.asCString();

                int darg_idx = mi.default_arguments.size() - mi.arguments.size() + j;
                if (darg_idx >= 0) {
                    Variant default_arg = mi.default_arguments[darg_idx];
                    ad.default_value = default_arg.get_construct_string().c_str();
                }

                method.arguments.push_back(ad);
            }

            if (mi.return_val.type == VariantType::NIL) {
                if (!mi.return_val.name.empty())
                    method.return_type = "Variant";
            } else {
                method.return_type = Variant::get_type_name(mi.return_val.type);
            }

            c.methods.push_back(method);
        }

        Vector<PropertyInfo> properties;
        v.get_property_list(&properties);
        for (const PropertyInfo &pi : properties) {

            DocContents::PropertyDoc property;
            property.name = pi.name.asCString();
            property.type = Variant::interned_type_name(pi.type).asCString();
            property.default_value = v.get(pi.name).get_construct_string().c_str();

            c.properties.push_back(property);
        }

        Vector<StringName> constants;
        Variant::get_constants_for_type(VariantType(i), &constants);

        for (const StringName &E : constants) {

            DocContents::ConstantDoc constant;
            constant.name = E.asCString();
            Variant value = Variant::get_constant_value(VariantType(i), E);
            constant.value = value.get_type() == VariantType::INT ? itos(value.as<int>()).c_str() : value.get_construct_string().c_str();
            c.constants.push_back(constant);
        }
    }

    //built in constants and functions

    {

        StringName cname("@GlobalScope");
        tgt.class_list[cname.asCString()] = DocContents::ClassDoc();
        DocContents::ClassDoc &c = tgt.class_list[cname.asCString()];
        c.name = cname.asCString();

        for (int i = 0; i < GlobalConstants::get_global_constant_count(); i++) {

            DocContents::ConstantDoc cd;
            cd.name = GlobalConstants::get_global_constant_name(i);
            cd.value = itos(GlobalConstants::get_global_constant_value(i)).c_str();
            cd.enumeration = GlobalConstants::get_global_constant_enum(i).asCString();
            c.constants.push_back(cd);
        }

        auto &singletons(Engine::get_singleton()->get_singletons());

        //servers (this is kind of hackish)
        for (const Engine::Singleton &s : singletons) {

            DocContents::PropertyDoc pd;
            if (!s.ptr) {
                continue;
            }
            pd.name = s.name.asCString();
            pd.type = StringName(s.ptr->get_class()).asCString();
            while (ClassDB::get_parent_class(StringName(pd.type)) != StringView("Object"))
                pd.type = ClassDB::get_parent_class(StringName(pd.type)).asCString();
            if (pd.type.starts_with('_'))
                pd.type = pd.type.substr(1);
            c.properties.push_back(pd);
        }
    }

    //built in script reference

    {

        for (int i = 0; i < ScriptServer::get_language_count(); i++) {

            ScriptLanguage *lang = ScriptServer::get_language(i);
            StringName cname = StringName(String("@") + lang->get_name());
            tgt.class_list[cname.asCString()] = DocContents::ClassDoc();
            DocContents::ClassDoc &c = tgt.class_list[cname.asCString()];
            c.name = cname.asCString();

            Vector<MethodInfo> minfo;

            lang->get_public_functions(&minfo);

            for (MethodInfo& mi : minfo) {

                DocContents::MethodDoc md;
                md.name = mi.name.asCString();

                if (mi.flags & METHOD_FLAG_VARARG) {
                    if (!md.qualifiers.empty())
                        md.qualifiers += ' ';
                    md.qualifiers += "vararg";
                }

                return_doc_from_retinfo(md, mi.return_val);

                for (size_t j = 0; j < mi.arguments.size(); j++) {

                    DocContents::ArgumentDoc ad;
                    argument_doc_from_arginfo(ad, mi.arguments[j]);

                    int darg_idx = j - (mi.arguments.size() - mi.default_arguments.size());

                    if (darg_idx >= 0) {
                        Variant default_arg = mi.default_arguments[darg_idx];
                        ad.default_value = default_arg.get_construct_string().c_str();
                    }

                    md.arguments.push_back(ad);
                }

                c.methods.push_back(md);
            }

            Vector<Pair<StringView, Variant> > cinfo;
            lang->get_public_constants(&cinfo);

            for (const Pair<StringView, Variant> & E : cinfo) {

                DocContents::ConstantDoc cd;
                cd.name = E.first;
                cd.value = E.second.as<String>();
                c.constants.push_back(cd);
            }
        }
    }
}
