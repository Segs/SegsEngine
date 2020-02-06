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

#include "doc_data.h"

#include "core/engine.h"
#include "core/global_constants.h"
#include "core/io/compression.h"
#include "core/io/marshalls.h"
#include "core/method_bind_interface.h"
#include "core/os/dir_access.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "core/version.h"
#include "core/string_utils.h"
#include "scene/resources/theme.h"

#include "EASTL/sort.h"

//NOTE: this function is also used in doc_dump.cpp
void _write_string(FileAccess *f, int p_tablevel, se_string_view p_string) {

    if (p_string.empty())
        return;
    String tab;
    for (int i = 0; i < p_tablevel; i++)
        tab += "\t";
    f->store_string(tab + p_string + "\n");
}

void DocData::merge_from(const DocData &p_data) {

    for (eastl::pair<const StringName,ClassDoc> &E : class_list) {

        ClassDoc &c(E.second);
        auto iter = p_data.class_list.find(StringName(c.name));
        if (iter==p_data.class_list.end())
            continue;

        const ClassDoc &cf = iter->second;

        c.description = cf.description;
        c.brief_description = cf.brief_description;
        c.tutorials = cf.tutorials;

        for (MethodDoc &m : c.methods) {

            for (int j = 0; j < cf.methods.size(); j++) {

                if (cf.methods[j].name != m.name)
                    continue;
                if (cf.methods[j].arguments.size() != m.arguments.size())
                    continue;
                // since polymorphic functions are allowed we need to check the type of
                // the arguments so we make sure they are different.
                int arg_count = cf.methods[j].arguments.size();
                PODVector<bool> arg_used;
                arg_used.resize(arg_count);
                for (int l = 0; l < arg_count; ++l)
                    arg_used[l] = false;
                // also there is no guarantee that argument ordering will match, so we
                // have to check one by one so we make sure we have an exact match
                for (int k = 0; k < arg_count; ++k) {
                    for (int l = 0; l < arg_count; ++l)
                        if (cf.methods[j].arguments[k].type == m.arguments[l].type && !arg_used[l]) {
                            arg_used[l] = true;
                            break;
                        }
                }
                bool not_the_same = false;
                for (int l = 0; l < arg_count; ++l)
                    if (!arg_used[l]) // at least one of the arguments was different
                        not_the_same = true;
                if (not_the_same)
                    continue;

                const MethodDoc &mf = cf.methods[j];

                m.description = mf.description;
                break;
            }
        }

        for (int i = 0; i < c.defined_signals.size(); i++) {

            MethodDoc &m = c.defined_signals[i];

            for (int j = 0; j < cf.defined_signals.size(); j++) {

                if (cf.defined_signals[j].name != m.name)
                    continue;
                const MethodDoc &mf = cf.defined_signals[j];

                m.description = mf.description;
                break;
            }
        }

        for (ConstantDoc &m : c.constants) {

            for (int j = 0; j < cf.constants.size(); j++) {

                if (cf.constants[j].name != m.name)
                    continue;
                const ConstantDoc &mf = cf.constants[j];

                m.description = mf.description;
                break;
            }
        }

        for (PropertyDoc &p : c.properties) {

            for (int j = 0; j < cf.properties.size(); j++) {

                if (cf.properties[j].name != p.name)
                    continue;
                const PropertyDoc &pf = cf.properties[j];

                p.description = pf.description;
                break;
            }
        }

        for (PropertyDoc &p : c.theme_properties) {

            for (int j = 0; j < cf.theme_properties.size(); j++) {

                if (cf.theme_properties[j].name != p.name)
                    continue;
                const PropertyDoc &pf = cf.theme_properties[j];

                p.description = pf.description;
                break;
            }
        }
    }
}

void DocData::remove_from(const DocData &p_data) {
    for (const eastl::pair<const StringName,ClassDoc> &E : p_data.class_list) {
        if (class_list.contains(E.first))
            class_list.erase(E.first);
    }
}

static void return_doc_from_retinfo(DocData::MethodDoc &p_method, const PropertyInfo &p_retinfo) {

    if (p_retinfo.type == VariantType::INT && p_retinfo.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
        p_method.return_enum = p_retinfo.class_name;
        if (StringUtils::begins_with(p_method.return_enum,"_")) //proxy class
            p_method.return_enum = StringUtils::substr(p_method.return_enum,1, p_method.return_enum.length());
        p_method.return_type = "int";
    } else if (p_retinfo.class_name != StringName()) {
        p_method.return_type = p_retinfo.class_name;
    } else if (p_retinfo.hint == PropertyHint::ResourceType) {
        p_method.return_type = p_retinfo.hint_string;
    } else if (p_retinfo.type == VariantType::NIL && p_retinfo.usage & PROPERTY_USAGE_NIL_IS_VARIANT) {
        p_method.return_type = "Variant";
    } else if (p_retinfo.type == VariantType::NIL) {
        p_method.return_type = "void";
    } else {
        p_method.return_type = Variant::get_type_name(p_retinfo.type);
    }
}

static void argument_doc_from_arginfo(DocData::ArgumentDoc &p_argument, const PropertyInfo &p_arginfo) {

    p_argument.name = p_arginfo.name;

    if (p_arginfo.type == VariantType::INT && p_arginfo.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
        p_argument.enumeration = p_arginfo.class_name;
        if (StringUtils::begins_with(p_argument.enumeration,"_")) //proxy class
            p_argument.enumeration = StringUtils::substr(p_argument.enumeration,1, p_argument.enumeration.length());
        p_argument.type = "int";
    } else if (p_arginfo.class_name != StringName()) {
        p_argument.type = p_arginfo.class_name;
    } else if (p_arginfo.hint == PropertyHint::ResourceType) {
        p_argument.type = p_arginfo.hint_string;
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
        PODVector<StringName> inheriting_classes;
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

void DocData::generate(bool p_basic_types) {

    PODVector<StringName> classes;
    ClassDB::get_class_list(&classes);
    eastl::sort(classes.begin(),classes.end(),WrapAlphaCompare());
    // Move ProjectSettings, so that other classes can register properties there.
    auto it=classes.find("ProjectSettings");
    classes.erase_unsorted(it);
    classes.push_back(StringName("ProjectSettings"));

    bool skip_setter_getter_methods = true;

    for(int i=0,fin=classes.size(); i<fin; ++i) {

        Set<StringName> setters_getters;

        StringName name = classes[i];
        StringName cname(name);
        if (StringUtils::begins_with(cname,"_")) //proxy class
            cname = StringName(StringUtils::substr(cname,1, strlen(name.asCString())));

        class_list[cname] = ClassDoc();
        ClassDoc &c = class_list[cname];
        c.name = cname;
        c.inherits = ClassDB::get_parent_class(name);

        PODVector<PropertyInfo> properties;
        PODVector<PropertyInfo> own_properties;
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

            PropertyDoc prop;

            prop.name = E.name;
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
                prop.default_value = StringUtils::replace(default_value.get_construct_string(),"\n", "");
            }

            StringName setter = ClassDB::get_property_setter(name, E.name);
            StringName getter = ClassDB::get_property_getter(name, E.name);

            prop.setter = setter;
            prop.getter = getter;

            bool found_type = false;
            if (getter != StringName()) {
                MethodBind *mb = ClassDB::get_method(name, getter);
                if (mb) {
                    #ifdef DEBUG_METHODS_ENABLED
                    PropertyInfo retinfo = mb->get_return_info();

                    found_type = true;
                    if (retinfo.type == VariantType::INT && retinfo.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
                        prop.enumeration = retinfo.class_name;
                        prop.type = "int";
                    } else if (retinfo.class_name != StringName()) {
                        prop.type = retinfo.class_name;
                    } else if (retinfo.hint == PropertyHint::ResourceType) {

                        prop.type = StringName(retinfo.hint_string);
                    } else if (retinfo.type == VariantType::NIL && retinfo.usage & PROPERTY_USAGE_NIL_IS_VARIANT) {

                        prop.type = "Variant";
                    } else if (retinfo.type == VariantType::NIL) {
                        prop.type = "void";
                    } else {
                        prop.type = Variant::interned_type_name(retinfo.type);
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
                    prop.type = StringName(E.hint_string);
                else
                    prop.type = Variant::interned_type_name(E.type);
            }

            c.properties.push_back(prop);
        }

        PODVector<MethodInfo> method_list;
        ClassDB::get_method_list(name, &method_list, true);
        eastl::sort(method_list.begin(),method_list.end());

        for (const MethodInfo &E : method_list) {

            if (E.name.empty() || E.name.asCString()[0] == '_' && !(E.flags & METHOD_FLAG_VIRTUAL))
                continue; //hidden, don't count

            if (skip_setter_getter_methods && setters_getters.contains(E.name)) {
                // Don't skip parametric setters and getters, i.e. method which require
                // one or more parameters to define what property should be set or retrieved.
                // E.g. CPUParticles::set_param(Parameter param, float value).
                if (E.arguments.empty() /* getter */ || E.arguments.size() == 1 && E.return_val.type == VariantType::NIL /* setter */) {
                    continue;
                }
            }

            MethodDoc method;

            method.name = E.name;

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

                ArgumentDoc argument;

                argument_doc_from_arginfo(argument, arginfo);

                int darg_idx = int(i) - int(E.arguments.size() - E.default_arguments.size());

                if (darg_idx >= 0) {
                    Variant default_arg = E.default_arguments[darg_idx];
                    argument.default_value = default_arg.get_construct_string();
                }

                method.arguments.push_back(argument);
            }

            c.methods.push_back(method);
        }

        PODVector<MethodInfo> signal_list;
        ClassDB::get_signal_list(name, &signal_list, true);

        if (!signal_list.empty()) {

            for (const MethodInfo &EV : signal_list) {

                MethodDoc signal;
                signal.name = EV.name;
                for (const PropertyInfo &arginfo : EV.arguments) {

                    ArgumentDoc argument;
                    argument_doc_from_arginfo(argument, arginfo);

                    signal.arguments.push_back(argument);
                }

                c.defined_signals.push_back(signal);
            }
        }

        ListPOD<String> constant_list;
        ClassDB::get_integer_constant_list(name, &constant_list, true);

        for (const String & E : constant_list) {

            ConstantDoc constant;
            constant.name = E;
            constant.value = itos(ClassDB::get_integer_constant(name, StringName(E)));
            constant.enumeration = ClassDB::get_integer_constant_enum(name, StringName(E));
            c.constants.push_back(constant);
        }

        //theme stuff

        {
            PODVector<StringName> l;
            Theme::get_default()->get_constant_list(cname, &l);
            for (const StringName &E : l) {

                PropertyDoc pd;
                pd.name = E;
                pd.type = "int";
                pd.default_value = itos(Theme::get_default()->get_constant(E, cname));
                c.theme_properties.push_back(pd);
            }

            l.clear();
            Theme::get_default()->get_color_list(cname, &l);
            for (const StringName &E : l) {

                PropertyDoc pd;
                pd.name = E;
                pd.type = "Color";
                pd.default_value = Variant(Theme::get_default()->get_color(E, cname)).get_construct_string();
                c.theme_properties.push_back(pd);
            }

            l.clear();
            Theme::get_default()->get_icon_list(cname, &l);
            for (const StringName &E : l) {

                PropertyDoc pd;
                pd.name = E;
                pd.type = "Texture";
                c.theme_properties.push_back(pd);
            }
            l.clear();
            Theme::get_default()->get_font_list(cname, &l);
            for (const StringName &E : l) {

                PropertyDoc pd;
                pd.name = E;
                pd.type = "Font";
                c.theme_properties.push_back(pd);
            }
            l = Theme::get_default()->get_stylebox_list(cname);
            for (const StringName &E : l) {

                PropertyDoc pd;
                pd.name = E;
                pd.type = "StyleBox";
                c.theme_properties.push_back(pd);
            }
        }
    }

    {
        // So we can document the concept of Variant even if it's not a usable class per se.
        class_list["Variant"] = ClassDoc();
        class_list["Variant"].name = "Variant";
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

        class_list[cname] = ClassDoc();
        ClassDoc &c = class_list[cname];
        c.name = cname;

        Variant::CallError cerror;
        Variant v = Variant::construct(VariantType(i), nullptr, 0, cerror);

        PODVector<MethodInfo> method_list;
        v.get_method_list(&method_list);
        eastl::sort(method_list.begin(),method_list.end());
        Variant::get_constructor_list(VariantType(i), &method_list);

        for (MethodInfo &mi : method_list) {

            MethodDoc method;

            method.name = mi.name;

            for (size_t j = 0; j < mi.arguments.size(); j++) {

                PropertyInfo arginfo(mi.arguments[j]);

                ArgumentDoc ad;
                argument_doc_from_arginfo(ad, mi.arguments[j]);
                ad.name = arginfo.name;

                int defarg = mi.default_arguments.size() - mi.arguments.size() + j;
                if (defarg >= 0)
                    ad.default_value = mi.default_arguments[defarg].as<String>();

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

        PODVector<PropertyInfo> properties;
        v.get_property_list(&properties);
        for (const PropertyInfo &pi : properties) {

            PropertyDoc property;
            property.name = pi.name;
            property.type = Variant::interned_type_name(pi.type);
            property.default_value = v.get(pi.name).get_construct_string();

            c.properties.push_back(property);
        }

        PODVector<StringName> constants;
        Variant::get_constants_for_type(VariantType(i), &constants);

        for (const StringName &E : constants) {

            ConstantDoc constant;
            constant.name = E;
            Variant value = Variant::get_constant_value(VariantType(i), E);
            constant.value = value.get_type() == VariantType::INT ? itos(value) : value.get_construct_string();
            c.constants.push_back(constant);
        }
    }

    //built in constants and functions

    {

        StringName cname("@GlobalScope");
        class_list[cname] = ClassDoc();
        ClassDoc &c = class_list[cname];
        c.name = cname;

        for (int i = 0; i < GlobalConstants::get_global_constant_count(); i++) {

            ConstantDoc cd;
            cd.name = GlobalConstants::get_global_constant_name(i);
            cd.value = itos(GlobalConstants::get_global_constant_value(i));
            cd.enumeration = GlobalConstants::get_global_constant_enum(i);
            c.constants.push_back(cd);
        }

        auto &singletons(Engine::get_singleton()->get_singletons());

        //servers (this is kind of hackish)
        for (const Engine::Singleton &s : singletons) {

            PropertyDoc pd;
            if (!s.ptr) {
                continue;
            }
            pd.name = s.name;
            pd.type = StringName(s.ptr->get_class());
            while (ClassDB::get_parent_class(pd.type) != se_string_view("Object"))
                pd.type = ClassDB::get_parent_class(pd.type);
            if (StringUtils::begins_with(pd.type,"_"))
                pd.type = StringName(StringUtils::substr(pd.type,1, se_string_view(pd.type).size()));
            c.properties.push_back(pd);
        }
    }

    //built in script reference

    {

        for (int i = 0; i < ScriptServer::get_language_count(); i++) {

            ScriptLanguage *lang = ScriptServer::get_language(i);
            StringName cname = StringName(String("@") + lang->get_name());
            class_list[cname] = ClassDoc();
            ClassDoc &c = class_list[cname];
            c.name = cname;

            PODVector<MethodInfo> minfo;

            lang->get_public_functions(&minfo);

            for (MethodInfo& mi : minfo) {

                MethodDoc md;
                md.name = mi.name;

                if (mi.flags & METHOD_FLAG_VARARG) {
                    if (!md.qualifiers.empty())
                        md.qualifiers += ' ';
                    md.qualifiers += "vararg";
                }

                return_doc_from_retinfo(md, mi.return_val);

                for (size_t j = 0; j < mi.arguments.size(); j++) {

                    ArgumentDoc ad;
                    argument_doc_from_arginfo(ad, mi.arguments[j]);

                    int darg_idx = j - (mi.arguments.size() - mi.default_arguments.size());

                    if (darg_idx >= 0) {
                        Variant default_arg = mi.default_arguments[darg_idx];
                        ad.default_value = default_arg.get_construct_string();
                    }

                    md.arguments.push_back(ad);
                }

                c.methods.push_back(md);
            }

            PODVector<Pair<se_string_view, Variant> > cinfo;
            lang->get_public_constants(&cinfo);

            for (const Pair<se_string_view, Variant> & E : cinfo) {

                ConstantDoc cd;
                cd.name = E.first;
                cd.value = E.second.as<String>();
                c.constants.push_back(cd);
            }
        }
    }
}

static Error _parse_methods(Ref<XMLParser> &parser, PODVector<DocData::MethodDoc> &methods) {

    const String section(parser->get_node_name());
    se_string_view element = StringUtils::substr(section,0, section.length() - 1);

    while (parser->read() == OK) {

        if (parser->get_node_type() == XMLParser::NODE_ELEMENT) {

            if (parser->get_node_name() == element) {

                DocData::MethodDoc method;
                ERR_FAIL_COND_V(!parser->has_attribute("name"), ERR_FILE_CORRUPT);
                method.name = parser->get_attribute_value("name");
                if (parser->has_attribute("qualifiers"))
                    method.qualifiers = parser->get_attribute_value("qualifiers");

                while (parser->read() == OK) {

                    if (parser->get_node_type() == XMLParser::NODE_ELEMENT) {

                        const String & name(parser->get_node_name());
                        if (name == "return") {

                            ERR_FAIL_COND_V(!parser->has_attribute("type"), ERR_FILE_CORRUPT);
                            method.return_type = parser->get_attribute_value("type");
                            if (parser->has_attribute("enum")) {
                                method.return_enum = parser->get_attribute_value("enum");
                            }
                        } else if (name == "argument") {

                            DocData::ArgumentDoc argument;
                            ERR_FAIL_COND_V(!parser->has_attribute("name"), ERR_FILE_CORRUPT);
                            argument.name = parser->get_attribute_value("name");
                            ERR_FAIL_COND_V(!parser->has_attribute("type"), ERR_FILE_CORRUPT);
                            argument.type = parser->get_attribute_value("type");
                            if (parser->has_attribute("enum")) {
                                argument.enumeration = parser->get_attribute_value("enum");
                            }

                            method.arguments.push_back(argument);

                        } else if (name == "description") {

                            parser->read();
                            if (parser->get_node_type() == XMLParser::NODE_TEXT)
                                method.description = parser->get_node_data();
                        }

                    } else if (parser->get_node_type() == XMLParser::NODE_ELEMENT_END && parser->get_node_name() == element)
                        break;
                }

                methods.push_back(method);

            } else {
                ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Invalid tag in doc file: " + parser->get_node_name() + ".");
            }

        } else if (parser->get_node_type() == XMLParser::NODE_ELEMENT_END && parser->get_node_name() == section)
            break;
    }

    return OK;
}

Error DocData::load_classes(se_string_view p_dir) {

    Error err;
    DirAccessRef da = DirAccess::open(p_dir, &err);
    if (!da) {
        return err;
    }

    da->list_dir_begin();
    String path(da->get_next());
    while (!path.empty()) {
        if (!da->current_is_dir() && StringUtils::ends_with(path,"xml")) {
            Ref<XMLParser> parser(make_ref_counted<XMLParser>());
            Error err2 = parser->open(PathUtils::plus_file(p_dir,path));
            if (err2)
                return err2;

            _load(parser);
        }
        path = da->get_next();
    }

    da->list_dir_end();

    return OK;
}
Error DocData::erase_classes(se_string_view p_dir) {

    Error err;
    DirAccessRef da = DirAccess::open(p_dir, &err);
    if (!da) {
        return err;
    }

    List<String> to_erase;

    da->list_dir_begin();
    String path(da->get_next());
    while (!path.empty()) {
        if (!da->current_is_dir() && StringUtils::ends_with(path,"xml")) {
            to_erase.push_back(path);
        }
        path = da->get_next();
    }
    da->list_dir_end();

    while (!to_erase.empty()) {
        da->remove(to_erase.front()->deref());
        to_erase.pop_front();
    }

    return OK;
}
Error DocData::_load(Ref<XMLParser> parser) {

    Error err = OK;

    while ((err = parser->read()) == OK) {

        if (parser->get_node_type() == XMLParser::NODE_ELEMENT && parser->get_node_name() == "?xml") {
            parser->skip_section();
        }

        if (parser->get_node_type() != XMLParser::NODE_ELEMENT)
            continue; //no idea what this may be, but skipping anyway

        ERR_FAIL_COND_V(parser->get_node_name() != "class", ERR_FILE_CORRUPT);

        ERR_FAIL_COND_V(!parser->has_attribute("name"), ERR_FILE_CORRUPT);
        StringName name(parser->get_attribute_value("name"));
        class_list[name] = ClassDoc();
        ClassDoc &c = class_list[name];

        c.name = name;
        if (parser->has_attribute("inherits"))
            c.inherits = StringName(parser->get_attribute_value("inherits"));

        while (parser->read() == OK) {

            if (parser->get_node_type() == XMLParser::NODE_ELEMENT) {

                const String &name2 = parser->get_node_name();

                if (name2 == "brief_description") {

                    parser->read();
                    if (parser->get_node_type() == XMLParser::NODE_TEXT)
                        c.brief_description = parser->get_node_data();

                } else if (name2 == "description") {
                    parser->read();
                    if (parser->get_node_type() == XMLParser::NODE_TEXT)
                        c.description = parser->get_node_data();
                } else if (name2 == "tutorials") {
                    while (parser->read() == OK) {

                        if (parser->get_node_type() == XMLParser::NODE_ELEMENT) {

                            const String &name3 = parser->get_node_name();

                            if (name3 == "link") {

                                parser->read();
                                if (parser->get_node_type() == XMLParser::NODE_TEXT)
                                    c.tutorials.emplace_back(StringUtils::strip_edges(parser->get_node_data()));
                            } else {
                                ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Invalid tag in doc file: " + name3 + ".");
                            }
                        } else if (parser->get_node_type() == XMLParser::NODE_ELEMENT_END && parser->get_node_name() == "tutorials")
                            break; // End of <tutorials>.
                    }
                } else if (name2 == "methods") {

                    Error err2 = _parse_methods(parser, c.methods);
                    ERR_FAIL_COND_V(err2, err2);

                } else if (name2 == "signals") {

                    Error err2 = _parse_methods(parser, c.defined_signals);
                    ERR_FAIL_COND_V(err2, err2);
                } else if (name2 == "members") {

                    while (parser->read() == OK) {

                        if (parser->get_node_type() == XMLParser::NODE_ELEMENT) {

                            const String &name3 = parser->get_node_name();

                            if (name3 == "member") {

                                PropertyDoc prop2;

                                ERR_FAIL_COND_V(!parser->has_attribute("name"), ERR_FILE_CORRUPT);
                                prop2.name = parser->get_attribute_value("name");
                                ERR_FAIL_COND_V(!parser->has_attribute("type"), ERR_FILE_CORRUPT);
                                prop2.type = StringName(parser->get_attribute_value("type"));
                                if (parser->has_attribute("setter"))
                                    prop2.setter = parser->get_attribute_value("setter");
                                if (parser->has_attribute("getter"))
                                    prop2.getter = parser->get_attribute_value("getter");
                                if (parser->has_attribute("enum"))
                                    prop2.enumeration = parser->get_attribute_value("enum");
                                if (!parser->is_empty()) {
                                    parser->read();
                                    if (parser->get_node_type() == XMLParser::NODE_TEXT)
                                        prop2.description = parser->get_node_data();
                                }
                                c.properties.push_back(prop2);
                            } else {
                                ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Invalid tag in doc file: " + name3 + ".");
                            }

                        } else if (parser->get_node_type() == XMLParser::NODE_ELEMENT_END && parser->get_node_name() == "members")
                            break; // End of <members>.
                    }

                } else if (name2 == "theme_items") {

                    while (parser->read() == OK) {

                        if (parser->get_node_type() == XMLParser::NODE_ELEMENT) {

                            const String & name3 = parser->get_node_name();

                            if (name3 == "theme_item") {

                                PropertyDoc prop2;

                                ERR_FAIL_COND_V(!parser->has_attribute("name"), ERR_FILE_CORRUPT);
                                prop2.name = parser->get_attribute_value("name");
                                ERR_FAIL_COND_V(!parser->has_attribute("type"), ERR_FILE_CORRUPT);
                                prop2.type = StringName(parser->get_attribute_value("type"));
                                if (!parser->is_empty()) {
                                    parser->read();
                                    if (parser->get_node_type() == XMLParser::NODE_TEXT)
                                        prop2.description = parser->get_node_data();
                                }
                                c.theme_properties.push_back(prop2);
                            } else {
                                ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Invalid tag in doc file: " + name3 + ".");
                            }

                        } else if (parser->get_node_type() == XMLParser::NODE_ELEMENT_END && parser->get_node_name() == "theme_items")
                            break; // End of <theme_items>.
                    }

                } else if (name2 == "constants") {

                    while (parser->read() == OK) {

                        if (parser->get_node_type() == XMLParser::NODE_ELEMENT) {

                            const String & name3 = parser->get_node_name();

                            if (name3 == "constant") {

                                ConstantDoc constant2;
                                ERR_FAIL_COND_V(!parser->has_attribute("name"), ERR_FILE_CORRUPT);
                                constant2.name = parser->get_attribute_value("name");
                                ERR_FAIL_COND_V(!parser->has_attribute("value"), ERR_FILE_CORRUPT);
                                constant2.value = parser->get_attribute_value("value");
                                if (parser->has_attribute("enum")) {
                                    constant2.enumeration = parser->get_attribute_value("enum");
                                }
                                if (!parser->is_empty()) {
                                    parser->read();
                                    if (parser->get_node_type() == XMLParser::NODE_TEXT)
                                        constant2.description = parser->get_node_data();
                                }
                                c.constants.push_back(constant2);
                            } else {
                                ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Invalid tag in doc file: " + name3 + ".");
                            }

                        } else if (parser->get_node_type() == XMLParser::NODE_ELEMENT_END && parser->get_node_name() == "constants")
                            break; // End of <constants>.
                    }

                } else {

                    ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Invalid tag in doc file: " + name2 + ".");
                }

            } else if (parser->get_node_type() == XMLParser::NODE_ELEMENT_END && parser->get_node_name() == "class")
                break; // End of <class>.
        }
    }

    return OK;
}

Error DocData::save_classes(se_string_view p_default_path, const Map<StringName, String> &p_class_path) {

    for (eastl::pair<const StringName,ClassDoc> &E : class_list) {

        ClassDoc &c(E.second);

        String save_path;
        if (p_class_path.contains(c.name)) {
            save_path = p_class_path.at(c.name);
        } else {
            save_path = p_default_path;
        }

        Error err;
        String save_file = PathUtils::plus_file(save_path,String(c.name) + ".xml");
        FileAccessRef f = FileAccess::open(save_file, FileAccess::WRITE, &err);

        ERR_CONTINUE_MSG(err != OK, "Can't write doc file: " + save_file + "."); 

        _write_string(f, 0, R"(<?xml version="1.0" encoding="UTF-8" ?>)");

        String header = String("<class name=\"") + c.name + "\"";
        if (!c.inherits.empty())
            header += String(" inherits=\"") + c.inherits + "\"";

        header += String(" version=\"") + VERSION_NUMBER + "\"";
        header += '>';
        _write_string(f, 0, header);
        _write_string(f, 1, "<brief_description>");
        _write_string(f, 2, StringUtils::xml_escape(StringUtils::strip_edges(c.brief_description)));
        _write_string(f, 1, "</brief_description>");
        _write_string(f, 1, "<description>");
        _write_string(f, 2, StringUtils::xml_escape(StringUtils::strip_edges(c.description)));
        _write_string(f, 1, "</description>");
        _write_string(f, 1, "<tutorials>");
        for (int i = 0; i < c.tutorials.size(); i++) {
            _write_string(f, 2, "<link>" + StringUtils::xml_escape(c.tutorials[i]) + "</link>");
        }
        _write_string(f, 1, "</tutorials>");
        _write_string(f, 1, "<methods>");

        eastl::sort(c.methods.begin(),c.methods.end());

        for (int i = 0; i < c.methods.size(); i++) {

            const MethodDoc &m = c.methods[i];

            String qualifiers;
            if (!m.qualifiers.empty())
                qualifiers += " qualifiers=\"" + StringUtils::xml_escape(m.qualifiers) + "\"";

            _write_string(f, 2, "<method name=\"" + m.name + "\"" + qualifiers + ">");

            if (!m.return_type.empty()) {

                String enum_text;
                if (!m.return_enum.empty()) {
                    enum_text = " enum=\"" + m.return_enum + "\"";
                }
                _write_string(f, 3, "<return type=\"" + m.return_type + "\"" + enum_text + ">");
                _write_string(f, 3, "</return>");
            }

            for (int j = 0; j < m.arguments.size(); j++) {

                const ArgumentDoc &a = m.arguments[j];

                String enum_text;
                if (!a.enumeration.empty()) {
                    enum_text = " enum=\"" + a.enumeration + "\"";
                }

                if (!a.default_value.empty())
                    _write_string(f, 3,
                            "<argument index=\"" + itos(j) + "\" name=\"" + StringUtils::xml_escape(a.name) +
                                    "\" type=\"" + StringUtils::xml_escape(a.type) + "\"" + enum_text + " default=\"" +
                                    StringUtils::xml_escape(a.default_value, true) + "\">");
                else
                    _write_string(f, 3,
                            "<argument index=\"" + itos(j) + "\" name=\"" + StringUtils::xml_escape(a.name) +
                                    "\" type=\"" + StringUtils::xml_escape(a.type) + "\"" + enum_text + ">");

                _write_string(f, 3, "</argument>");
            }

            _write_string(f, 3, "<description>");
            _write_string(f, 4, StringUtils::xml_escape(StringUtils::strip_edges(m.description)));
            _write_string(f, 3, "</description>");

            _write_string(f, 2, "</method>");
        }

        _write_string(f, 1, "</methods>");

        if (!c.properties.empty()) {
            _write_string(f, 1, "<members>");
            eastl::sort(c.properties.begin(),c.properties.end());

            for (int i = 0; i < c.properties.size(); i++) {

                String additional_attributes;
                if (!c.properties[i].enumeration.empty()) {
                    additional_attributes += " enum=\"" + c.properties[i].enumeration + "\"";
                }
                if (!c.properties[i].default_value.empty()) {
                    additional_attributes += " default=\"" + StringUtils::xml_escape(c.properties[i].default_value,true) + "\"";
                }
                const PropertyDoc &p = c.properties[i];
                if (c.properties[i].overridden) {
                    _write_string(f, 2, "<member name=\"" + p.name + "\" type=\"" + p.type + "\" setter=\"" + p.setter + "\" getter=\"" + p.getter + "\" override=\"true\"" + additional_attributes + " />");
                } else {
                    _write_string(f, 2, "<member name=\"" + p.name + "\" type=\"" + p.type + "\" setter=\"" + p.setter + "\" getter=\"" + p.getter + "\"" + additional_attributes + ">");
                    _write_string(f, 3, StringUtils::xml_escape(StringUtils::strip_edges(p.description)));
                    _write_string(f, 2, "</member>");
                }
            }
            _write_string(f, 1, "</members>");
        }

        if (!c.defined_signals.empty()) {

            eastl::sort(c.defined_signals.begin(),c.defined_signals.end());

            _write_string(f, 1, "<signals>");
            for (int i = 0; i < c.defined_signals.size(); i++) {

                const MethodDoc &m = c.defined_signals[i];
                _write_string(f, 2, "<signal name=\"" + m.name + "\">");
                for (int j = 0; j < m.arguments.size(); j++) {

                    const ArgumentDoc &a = m.arguments[j];
                    _write_string(f, 3, "<argument index=\"" + itos(j) + "\" name=\"" + StringUtils::xml_escape(a.name) + "\" type=\"" + StringUtils::xml_escape(a.type) + "\">");
                    _write_string(f, 3, "</argument>");
                }

                _write_string(f, 3, "<description>");
                _write_string(f, 4, StringUtils::xml_escape(StringUtils::strip_edges(m.description)));
                _write_string(f, 3, "</description>");

                _write_string(f, 2, "</signal>");
            }

            _write_string(f, 1, "</signals>");
        }

        _write_string(f, 1, "<constants>");

        for (int i = 0; i < c.constants.size(); i++) {

            const ConstantDoc &k = c.constants[i];
            if (!k.enumeration.empty()) {
                _write_string(f, 2, "<constant name=\"" + k.name + "\" value=\"" + k.value + "\" enum=\"" + k.enumeration + "\">");
            } else {
                _write_string(f, 2, "<constant name=\"" + k.name + "\" value=\"" + k.value + "\">");
            }
            _write_string(f, 3, StringUtils::xml_escape(StringUtils::strip_edges(k.description)));
            _write_string(f, 2, "</constant>");
        }

        _write_string(f, 1, "</constants>");

        if (!c.theme_properties.empty()) {

            eastl::sort(c.theme_properties.begin(),c.theme_properties.end());

            _write_string(f, 1, "<theme_items>");
            for (int i = 0; i < c.theme_properties.size(); i++) {

                const PropertyDoc &p = c.theme_properties[i];

                if (!p.default_value.empty())
                    _write_string(f, 2, "<theme_item name=\"" + p.name + "\" type=\"" + p.type + "\" default=\"" + StringUtils::xml_escape(p.default_value,true) + "\">");
                else
                    _write_string(f, 2, "<theme_item name=\"" + p.name + "\" type=\"" + p.type + "\">");

                _write_string(f, 3, StringUtils::xml_escape(StringUtils::strip_edges(p.description)));

                _write_string(f, 2, "</theme_item>");
            }
            _write_string(f, 1, "</theme_items>");
        }

        _write_string(f, 0, "</class>");
    }

    return OK;
}

Error DocData::load_compressed(const uint8_t *p_data, int p_compressed_size, int p_uncompressed_size) {

    PoolVector<uint8_t> data;
    data.resize(p_uncompressed_size);
    Compression::decompress(data.write().ptr(), p_uncompressed_size, p_data, p_compressed_size, Compression::MODE_DEFLATE);
    class_list.clear();

    Ref<XMLParser> parser(make_ref_counted<XMLParser>());
    Error err = parser->open_buffer(data);
    if (err)
        return err;

    _load(parser);

    return OK;
}
