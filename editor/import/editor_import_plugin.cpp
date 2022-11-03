/*************************************************************************/
/*  editor_import_plugin.cpp                                             */
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

#include "editor_import_plugin.h"

#include "core/dictionary.h"
#include "core/list.h"
#include "core/script_language.h"
#include "core/method_info.h"
#include "core/class_db.h"

IMPL_GDCLASS(EditorImportPlugin)

EditorImportPlugin::EditorImportPlugin() {
}
const char *EditorImportPlugin::get_importer_name() const {
    ERR_FAIL_COND_V(!(get_script_instance() && get_script_instance()->has_method("get_importer_name")), {});
    static String s_last_returned = get_script_instance()->call("get_importer_name").as<String>();
    return s_last_returned.c_str();
}

const char *EditorImportPlugin::get_visible_name() const {
    ERR_FAIL_COND_V(!(get_script_instance() && get_script_instance()->has_method("get_visible_name")), {});
    static String s_last_returned = get_script_instance()->call("get_visible_name").as<String>();
    return s_last_returned.c_str();
}

void EditorImportPlugin::get_recognized_extensions(Vector<String> &p_extensions) const {
    ERR_FAIL_COND(!(get_script_instance() && get_script_instance()->has_method("get_recognized_extensions")));
    Array extensions = get_script_instance()->call("get_recognized_extensions").as<Array>();
    p_extensions.reserve(p_extensions.size()+extensions.size());
    for (int i = 0; i < extensions.size(); i++) {
        p_extensions.emplace_back(extensions[i].as<String>());
    }
}

bool EditorImportPlugin::can_import(StringView path) const
{
    ERR_FAIL_COND_V(!(get_script_instance() && get_script_instance()->has_method("can_import")), true);
    return (bool)get_script_instance()->call("can_import", path);

}

StringName EditorImportPlugin::get_preset_name(int p_idx) const {
    ERR_FAIL_COND_V(!(get_script_instance() && get_script_instance()->has_method("get_preset_name")), {});
    return get_script_instance()->call("get_preset_name", p_idx).as<StringName>();
}

int EditorImportPlugin::get_preset_count() const {
    ERR_FAIL_COND_V(!(get_script_instance() && get_script_instance()->has_method("get_preset_count")), 0);
    return get_script_instance()->call("get_preset_count").as<int>();
}

StringName EditorImportPlugin::get_save_extension() const {
    ERR_FAIL_COND_V(!(get_script_instance() && get_script_instance()->has_method("get_save_extension")), {});
    return get_script_instance()->call("get_save_extension").as<StringName>();
}

StringName EditorImportPlugin::get_resource_type() const {
    ERR_FAIL_COND_V(!(get_script_instance() && get_script_instance()->has_method("get_resource_type")), {});
    return get_script_instance()->call("get_resource_type").as<StringName>();
}

float EditorImportPlugin::get_priority() const {
    if (!(get_script_instance() && get_script_instance()->has_method("get_priority"))) {
        return ResourceImporter::get_priority();
    }
    return get_script_instance()->call("get_priority").as<float>();
}

int EditorImportPlugin::get_import_order() const {
    if (!(get_script_instance() && get_script_instance()->has_method("get_import_order"))) {
        return ResourceImporter::get_import_order();
    }
    return get_script_instance()->call("get_import_order").as<int>();
}

void EditorImportPlugin::get_import_options(Vector<ResourceImporterInterface::ImportOption> *r_options, int p_preset) const {

    ERR_FAIL_COND(!(get_script_instance() && get_script_instance()->has_method("get_import_options")));
    Array needed;
    needed.push_back("name");
    needed.push_back("default_value");
    Array options = get_script_instance()->call("get_import_options", p_preset).as<Array>();
    for (int i = 0; i < options.size(); i++) {
        Dictionary d = options[i].as<Dictionary>();
        ERR_FAIL_COND(!d.has_all(needed));
        String name = d["name"].as<String>();
        Variant default_value = d["default_value"];

        PropertyHint hint = PropertyHint::None;
        if (d.has("property_hint")) {
            hint = (PropertyHint)d["property_hint"].as<int64_t>();
        }

        String hint_string;
        if (d.has("hint_string")) {
            hint_string = d["hint_string"].as<String>();
        }

        uint32_t usage = PROPERTY_USAGE_DEFAULT;
        if (d.has("usage")) {
            usage = d["usage"].as<uint32_t>();
        }

        ImportOption option(PropertyInfo(default_value.get_type(), StringName(name), hint, StringName(hint_string), usage), default_value);
        r_options->push_back(option);
    }
}

bool EditorImportPlugin::get_option_visibility(const StringName &p_option, const HashMap<StringName, Variant> &p_options) const {
    ERR_FAIL_COND_V(!(get_script_instance() && get_script_instance()->has_method("get_option_visibility")), true);
    Dictionary d;
    for(const auto &E:p_options) {
        d[E.first] = E.second;
    }
    return get_script_instance()->call("get_option_visibility", p_option, d).as<bool>();
}

Error EditorImportPlugin::import(StringView p_source_file, StringView p_save_path, const HashMap<StringName, Variant> &p_options, Vector<String> &r_missing_deps,
        Vector<String> *r_platform_variants, Vector<String> *r_gen_files, Variant *r_metadata) {

    ERR_FAIL_COND_V(!(get_script_instance() && get_script_instance()->has_method("import")), ERR_UNAVAILABLE);
    Dictionary options;
    Array platform_variants, gen_files;

    for(const auto &E : p_options) {
        options[E.first] = E.second;
    }
    Error err = (Error)get_script_instance()->call("import", p_source_file, p_save_path, options, platform_variants, gen_files).as<int64_t>();

    for (int i = 0; i < platform_variants.size(); i++) {
        r_platform_variants->push_back(platform_variants[i].as<String>());
    }
    for (int i = 0; i < gen_files.size(); i++) {
        r_gen_files->push_back(gen_files[i].as<String>());
    }
    return err;
}

void EditorImportPlugin::_bind_methods() {

    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::STRING, "get_importer_name"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::STRING, "get_visible_name"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::INT, "get_preset_count"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::STRING, "get_preset_name", PropertyInfo(VariantType::INT, "preset")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::BOOL, "can_import"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::ARRAY, "get_recognized_extensions"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::ARRAY, "get_import_options", PropertyInfo(VariantType::INT, "preset")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::STRING, "get_save_extension"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::STRING, "get_resource_type"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::FLOAT, "get_priority"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::INT, "get_import_order"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::BOOL, "get_option_visibility", PropertyInfo(VariantType::STRING, "option"), PropertyInfo(VariantType::DICTIONARY, "options")));
    ClassDB::add_virtual_method(get_class_static_name(),
            MethodInfo(VariantType::INT, "import", PropertyInfo(VariantType::STRING, "source_file"),
                    PropertyInfo(VariantType::STRING, "save_path"), PropertyInfo(VariantType::DICTIONARY, "options"),
                    PropertyInfo(VariantType::ARRAY, "platform_variants"), PropertyInfo(VariantType::ARRAY, "gen_files")));
}
