/*************************************************************************/
/*  editor_feature_profile.cpp                                           */
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

#include "editor_feature_profile.h"

#include "core/method_bind.h"
#include "core/callable_method_pointer.h"
#include "core/io/json.h"
#include "core/os/dir_access.h"
#include "core/string_formatter.h"
#include "editor/editor_settings.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "core/translation_helpers.h"
#include "EASTL/sort.h"

IMPL_GDCLASS(EditorFeatureProfile)
IMPL_GDCLASS(EditorFeatureProfileManager)
VARIANT_ENUM_CAST(EditorFeatureProfile::Feature)

const char *EditorFeatureProfile::feature_names[FEATURE_MAX] = {
    TTRC("3D Editor"),
    TTRC("Script Editor"),
    TTRC("Asset Library"),
    TTRC("Scene Tree Editing"),
    TTRC("Import Dock"),
    TTRC("Node Dock"),
    TTRC("FileSystem and Import Docks")
};

const char *EditorFeatureProfile::feature_identifiers[FEATURE_MAX] = {
    "3d",
    "script",
    "asset_lib",
    "scene_tree",
    "import_dock",
    "node_dock",
    "filesystem_dock"
};

void EditorFeatureProfile::set_disable_class(const StringName &p_class, bool p_disabled) {
    if (p_disabled) {
        disabled_classes.insert(p_class);
    } else {
        disabled_classes.erase(p_class);
    }
}

bool EditorFeatureProfile::is_class_disabled(const StringName &p_class) const {
    if(p_class.empty())
        return false;

    return disabled_classes.contains(p_class) || is_class_disabled(ClassDB::get_parent_class_nocheck(p_class));
}

void EditorFeatureProfile::set_disable_class_editor(const StringName &p_class, bool p_disabled) {
    if (p_disabled) {
        disabled_editors.insert(p_class);
    } else {
        disabled_editors.erase(p_class);
    }
}

bool EditorFeatureProfile::is_class_editor_disabled(const StringName &p_class) const {
    if (p_class.empty()) {
        return false;
    }
    return disabled_editors.contains(p_class) || is_class_editor_disabled(ClassDB::get_parent_class_nocheck(p_class));
}

void EditorFeatureProfile::set_disable_class_property(const StringName &p_class, const StringName &p_property, bool p_disabled) {

    if (p_disabled) {
        if (!disabled_properties.contains(p_class)) {
            disabled_properties[p_class] = HashSet<StringName>();
        }

        disabled_properties[p_class].insert(p_property);
    } else {
        ERR_FAIL_COND(!disabled_properties.contains(p_class));
        disabled_properties[p_class].erase(p_property);
        if (disabled_properties[p_class].empty()) {
            disabled_properties.erase(p_class);
        }
    }
}
bool EditorFeatureProfile::is_class_property_disabled(const StringName &p_class, const StringName &p_property) const {

    if (!disabled_properties.contains(p_class)) {
        return false;
    }

    if (!disabled_properties.at(p_class).contains(p_property)) {
        return false;
    }

    return true;
}

bool EditorFeatureProfile::has_class_properties_disabled(const StringName &p_class) const {
    return disabled_properties.contains(p_class);
}

void EditorFeatureProfile::set_disable_feature(Feature p_feature, bool p_disable) {

    ERR_FAIL_INDEX(p_feature, FEATURE_MAX);
    features_disabled[p_feature] = p_disable;
}
bool EditorFeatureProfile::is_feature_disabled(Feature p_feature) const {
    ERR_FAIL_INDEX_V(p_feature, FEATURE_MAX, false);
    return features_disabled[p_feature];
}

StringView EditorFeatureProfile::get_feature_name(Feature p_feature) {
    ERR_FAIL_INDEX_V(p_feature, FEATURE_MAX, {});
    return feature_names[p_feature];
}

Error EditorFeatureProfile::save_to_file(StringView p_path) {

    Dictionary json;
    json["type"] = "feature_profile";
    Array dis_classes;
    for (const StringName &E : disabled_classes) {
        dis_classes.push_back(E);
    }
    dis_classes.sort();
    json["disabled_classes"] = dis_classes;

    Array dis_editors;
    for (const StringName &E : disabled_editors) {
        dis_editors.push_back(E);
    }
    dis_editors.sort();
    json["disabled_editors"] = dis_editors;

    Array dis_props;

    for (eastl::pair<const StringName,HashSet<StringName> > &E : disabled_properties) {
        for (const StringName &F : E.second) {
            dis_props.push_back(String(E.first) + ":" + F);
        }
    }

    json["disabled_properties"] = dis_props;

    Array dis_features;
    for (int i = 0; i < FEATURE_MAX; i++) {
        if (features_disabled[i]) {
            dis_features.push_back(feature_identifiers[i]);
        }
    }

    json["disabled_features"] = dis_features;

    FileAccessRef f = FileAccess::open(p_path, FileAccess::WRITE);
    ERR_FAIL_COND_V_MSG(!f, ERR_CANT_CREATE, String("Cannot create file '") + p_path + "'.");

    String text = JSON::print(json, "\t");
    f->store_string(text);
    f->close();
    return OK;
}

Error EditorFeatureProfile::load_from_file(StringView p_path) {

    Error err;
    String text = FileAccess::get_file_as_string(p_path, &err);
    if (err != OK) {
        return err;
    }

    String err_str;
    int err_line;
    Variant v;
    err = JSON::parse(text, v, err_str, err_line);
    if (err != OK) {
        ERR_PRINT("Error parsing '" + String(p_path) + "' on line " + itos(err_line) + ": " + err_str);
        return ERR_PARSE_ERROR;
    }

    Dictionary json = v.as<Dictionary>();

    if (!json.has("type") || json["type"].as<String>() != "feature_profile") {
        ERR_PRINT("Error parsing '" + String(p_path) + "', it's not a feature profile.");
        return ERR_PARSE_ERROR;
    }

    disabled_classes.clear();

    if (json.has("disabled_classes")) {
        Array disabled_classes_arr = json["disabled_classes"].as<Array>();
        for (int i = 0; i < disabled_classes_arr.size(); i++) {
            disabled_classes.insert(disabled_classes_arr[i].as<StringName>());
        }
    }

    disabled_editors.clear();

    if (json.has("disabled_editors")) {
        Array disabled_editors_arr = json["disabled_editors"].as<Array>();
        for (int i = 0; i < disabled_editors_arr.size(); i++) {
            disabled_editors.insert(disabled_editors_arr[i].as<StringName>());
        }
    }

    disabled_properties.clear();

    if (json.has("disabled_properties")) {
        Array disabled_properties_arr = json["disabled_properties"].as<Array>();
        FixedVector<StringView,4,true> parts;
        for (int i = 0; i < disabled_properties_arr.size(); i++) {
            String s = disabled_properties_arr[i].as<String>();
            String::split_ref(parts,s,':');

            set_disable_class_property(StringName(parts[0]), StringName(parts[1]), true);
        }
    }

    if (json.has("disabled_features")) {

        Array disabled_features_arr = json["disabled_features"].as<Array>();
        for (int i = 0; i < FEATURE_MAX; i++) {
            bool found = false;
            const char * f(feature_identifiers[i]);
            for (int j = 0; j < disabled_features_arr.size(); j++) {
                String fd = disabled_features_arr[j].as<String>();
                if (fd == f) {
                    found = true;
                    break;
                }
            }

            features_disabled[i] = found;
        }
    }

    return OK;
}

void EditorFeatureProfile::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_disable_class", {"class_name", "disable"}), &EditorFeatureProfile::set_disable_class);
    MethodBinder::bind_method(D_METHOD("is_class_disabled", {"class_name"}), &EditorFeatureProfile::is_class_disabled);

    MethodBinder::bind_method(D_METHOD("set_disable_class_editor", {"class_name", "disable"}), &EditorFeatureProfile::set_disable_class_editor);
    MethodBinder::bind_method(D_METHOD("is_class_editor_disabled", {"class_name"}), &EditorFeatureProfile::is_class_editor_disabled);

    MethodBinder::bind_method(D_METHOD("set_disable_class_property", {"class_name", "property", "disable"}), &EditorFeatureProfile::set_disable_class_property);
    MethodBinder::bind_method(D_METHOD("is_class_property_disabled", {"class_name", "disable"}), &EditorFeatureProfile::is_class_property_disabled);

    MethodBinder::bind_method(D_METHOD("set_disable_feature", {"feature", "disable"}), &EditorFeatureProfile::set_disable_feature);
    MethodBinder::bind_method(D_METHOD("is_feature_disabled", {"feature"}), &EditorFeatureProfile::is_feature_disabled);

    MethodBinder::bind_method(D_METHOD("get_feature_name", {"feature"}), &EditorFeatureProfile::_get_feature_name);

    MethodBinder::bind_method(D_METHOD("save_to_file", {"path"}), &EditorFeatureProfile::save_to_file);
    MethodBinder::bind_method(D_METHOD("load_from_file", {"path"}), &EditorFeatureProfile::load_from_file);

    BIND_ENUM_CONSTANT(FEATURE_3D)
    BIND_ENUM_CONSTANT(FEATURE_SCRIPT)
    BIND_ENUM_CONSTANT(FEATURE_ASSET_LIB)
    BIND_ENUM_CONSTANT(FEATURE_SCENE_TREE)
    BIND_ENUM_CONSTANT(FEATURE_IMPORT_DOCK)
    BIND_ENUM_CONSTANT(FEATURE_NODE_DOCK)
    BIND_ENUM_CONSTANT(FEATURE_FILESYSTEM_DOCK)
    BIND_ENUM_CONSTANT(FEATURE_MAX)
}

EditorFeatureProfile::EditorFeatureProfile() {

    for (int i = 0; i < FEATURE_MAX; i++) {
        features_disabled[i] = false;
    }
}

//////////////////////////

void EditorFeatureProfileManager::_notification(int p_what) {
    if (p_what == NOTIFICATION_READY) {

        current_profile = EDITOR_GET("_default_feature_profile").as<String>();
        if (!current_profile.empty()) {
            current = make_ref_counted<EditorFeatureProfile>();
            Error err = current->load_from_file(PathUtils::plus_file(EditorSettings::get_singleton()->get_feature_profiles_dir(),current_profile + ".profile"));
            if (err != OK) {
                ERR_PRINT("Error loading default feature profile: " + current_profile);
                current_profile.clear();
                current.unref();
            }
        }
        _update_profile_list(current_profile);
    }
}

String EditorFeatureProfileManager::_get_selected_profile() {
    int idx = profile_list->get_selected();
    if (idx < 0) {
        return String();
    }

    return profile_list->get_item_metadata(idx).as<String>();
}

void EditorFeatureProfileManager::_update_profile_list(StringView p_select_profile) {

    String selected_profile;
    if (p_select_profile.empty()) { //default, keep
        if (profile_list->get_selected() >= 0) {
            selected_profile = profile_list->get_item_metadata(profile_list->get_selected()).as<String>();
            if (!FileAccess::exists(PathUtils::plus_file(EditorSettings::get_singleton()->get_feature_profiles_dir(),selected_profile + ".profile"))) {
                selected_profile.clear(); //does not exist
            }
        }
    } else {
        selected_profile = p_select_profile;
    }

    Vector<String> profiles;
    DirAccessRef d = DirAccess::open(EditorSettings::get_singleton()->get_feature_profiles_dir());
    ERR_FAIL_COND_MSG(!d, "Cannot open directory '" + EditorSettings::get_singleton()->get_feature_profiles_dir() + "'.");
    d->list_dir_begin();
    while (true) {
        String f = d->get_next();
        if (f.empty()) {
            break;
        }

        if (!d->current_is_dir()) {
            auto last_pos = StringUtils::find_last(f,".profile");
            if (last_pos != String::npos) {
                profiles.emplace_back(StringUtils::substr(f,0, last_pos));
            }
        }
    }

    eastl::sort(profiles.begin(), profiles.end());

    profile_list->clear();

    for (int i = 0; i < profiles.size(); i++) {
        String name = profiles[i];

        if (i == 0 && selected_profile.empty()) {
            selected_profile = name;
        }

        if (name == current_profile) {
            name += " (current)";
        }
        profile_list->add_item(StringName(name));
        int index = profile_list->get_item_count() - 1;
        profile_list->set_item_metadata(index, profiles[i]);
        if (profiles[i] == selected_profile) {
            profile_list->select(index);
        }
    }

    profile_actions[PROFILE_CLEAR]->set_disabled(current_profile.empty());
    profile_actions[PROFILE_ERASE]->set_disabled(selected_profile.empty());
    profile_actions[PROFILE_EXPORT]->set_disabled(selected_profile.empty());
    profile_actions[PROFILE_SET]->set_disabled(selected_profile.empty());

    current_profile_name->set_text(current_profile);

    _update_selected_profile();
}

void EditorFeatureProfileManager::_profile_action(int p_action) {

    switch (p_action) {
        case PROFILE_CLEAR: {

            EditorSettings::get_singleton()->set("_default_feature_profile", "");
            EditorSettings::get_singleton()->save();
            current_profile = "";
            current.unref();

            _update_profile_list();
            _emit_current_profile_changed();
        } break;
        case PROFILE_SET: {

            String selected = _get_selected_profile();
            ERR_FAIL_COND(selected.empty());
            if (selected == current_profile) {
                return; // Nothing to do here.
            }
            EditorSettings::get_singleton()->set("_default_feature_profile", selected);
            EditorSettings::get_singleton()->save();
            current_profile = selected;
            current = edited;

            _update_profile_list();
            _emit_current_profile_changed();
        } break;
        case PROFILE_IMPORT: {

            import_profiles->popup_centered_ratio();
        } break;
        case PROFILE_EXPORT: {

            export_profile->popup_centered_ratio();
            export_profile->set_current_file(_get_selected_profile() + ".profile");
        } break;
        case PROFILE_NEW: {

            new_profile_dialog->popup_centered_minsize();
            new_profile_name->clear();
            new_profile_name->grab_focus();
        } break;
        case PROFILE_ERASE: {

            String selected = _get_selected_profile();
            ERR_FAIL_COND(selected.empty());

            erase_profile_dialog->set_text(FormatSN(TTR("Erase profile '%s'? (no undo)").asCString(), selected.c_str()));
            erase_profile_dialog->popup_centered_minsize();
        } break;
    }
}

void EditorFeatureProfileManager::_erase_selected_profile() {

    String selected = _get_selected_profile();
    ERR_FAIL_COND(selected.empty());
    DirAccessRef da = DirAccess::open(EditorSettings::get_singleton()->get_feature_profiles_dir());
    ERR_FAIL_COND_MSG(!da, "Cannot open directory '" + EditorSettings::get_singleton()->get_feature_profiles_dir() + "'.");
    da->remove(selected + ".profile");
    if (selected == current_profile) {
        _profile_action(PROFILE_CLEAR);
    } else {
        _update_profile_list();
    }
}

void EditorFeatureProfileManager::_create_new_profile() {
    String name(StringUtils::strip_edges(new_profile_name->get_text()));
    if (!StringUtils::is_valid_filename(name) || StringUtils::find(name,".") != String::npos) {
        EditorNode::get_singleton()->show_warning(TTR("Profile must be a valid filename and must not contain '.'"));
        return;
    }
    String file = PathUtils::plus_file(EditorSettings::get_singleton()->get_feature_profiles_dir(),name + ".profile");
    if (FileAccess::exists(file)) {
        EditorNode::get_singleton()->show_warning(TTR("Profile with this name already exists."));
        return;
    }

    Ref<EditorFeatureProfile> new_profile(make_ref_counted<EditorFeatureProfile>());
    new_profile->save_to_file(file);

    _update_profile_list(name);
}

void EditorFeatureProfileManager::_profile_selected(int p_what) {

    _update_selected_profile();
}

void EditorFeatureProfileManager::_fill_classes_from(TreeItem *p_parent, const StringName &p_class, StringView p_selected) {

    TreeItem *class_item = class_list->create_item(p_parent);
    class_item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
    class_item->set_icon(0, EditorNode::get_singleton()->get_class_icon(p_class, "Node"));
    StringName text(p_class);

    bool disabled = edited->is_class_disabled(p_class);
    bool disabled_editor = edited->is_class_editor_disabled(p_class);
    bool disabled_properties = edited->has_class_properties_disabled(p_class);
    if (disabled) {
        class_item->set_custom_color(0, get_color("disabled_font_color", "Editor"));
    } else if (disabled_editor && disabled_properties) {
        text = text + " " + TTR("(Editor Disabled, Properties Disabled)");
    } else if (disabled_properties) {
        text = text + " " + TTR("(Properties Disabled)");
    } else if (disabled_editor) {
        text = text + " " + TTR("(Editor Disabled)");
    }
    class_item->set_text(0, text);
    class_item->set_editable(0, true);
    class_item->set_selectable(0, true);
    class_item->set_metadata(0, p_class);

    if (p_class == p_selected) {
        class_item->select(0);
    }
    if (disabled) {
        //class disabled, do nothing else (do not show further)
        return;
    }

    class_item->set_checked(0, true); // if its not disabled, its checked

    Vector<StringName> child_classes;
    ClassDB::get_direct_inheriters_from_class(p_class, &child_classes);
    eastl::sort(child_classes.begin(), child_classes.end(), WrapAlphaCompare());

    for (const StringName &E : child_classes) {
        if (StringUtils::begins_with(E,"Editor") || ClassDB::get_api_type(E) != ClassDB::API_CORE) {
            continue;
        }
        _fill_classes_from(class_item, E, p_selected);
    }
}

void EditorFeatureProfileManager::_class_list_item_selected() {

    if (updating_features)
        return;

    property_list->clear();

    TreeItem *item = class_list->get_selected();
    if (!item) {
        return;
    }

    Variant md = item->get_metadata(0);
    if (md.get_type() != VariantType::STRING && md.get_type() != VariantType::STRING_NAME) {
        return;
    }

    StringName class_name(md.as<StringName>());

    if (edited->is_class_disabled(class_name)) {
        return;
    }

    updating_features = true;
    TreeItem *root = property_list->create_item();
    TreeItem *options = property_list->create_item(root);
    options->set_text(0, TTR("Class Options:"));

    {
        TreeItem *option = property_list->create_item(options);
        option->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
        option->set_editable(0, true);
        option->set_selectable(0, true);
        option->set_checked(0, !edited->is_class_editor_disabled(class_name));
        option->set_text(0, TTR("Enable Contextual Editor"));
        option->set_metadata(0, CLASS_OPTION_DISABLE_EDITOR);
    }

    TreeItem *properties = property_list->create_item(root);
    properties->set_text(0, TTR("Enabled Properties:"));

    Vector<PropertyInfo> props;

    ClassDB::get_property_list(class_name, &props, true);

    for (const PropertyInfo &E : props) {

        StringName name = E.name;
        if (!(E.usage & PROPERTY_USAGE_EDITOR))
            continue;
        TreeItem *property = property_list->create_item(properties);
        property->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
        property->set_editable(0, true);
        property->set_selectable(0, true);
        property->set_checked(0, !edited->is_class_property_disabled(class_name, name));
        property->set_text_utf8(0, StringUtils::capitalize(name));
        property->set_metadata(0, name);
        StringName icon_type(Variant::interned_type_name(E.type));
        property->set_icon(0, EditorNode::get_singleton()->get_class_icon(icon_type,"Object"));
    }

    updating_features = false;
}

void EditorFeatureProfileManager::_class_list_item_edited() {

    if (updating_features)
        return;

    TreeItem *item = class_list->get_edited();
    if (!item) {
        return;
    }

    bool checked = item->is_checked(0);

    Variant md = item->get_metadata(0);
    if (md.get_type() == VariantType::STRING || md.get_type() == VariantType::STRING_NAME) {
        StringName class_selected = md.as<StringName>();
        edited->set_disable_class(class_selected, !checked);
        _save_and_update();
        _update_selected_profile();
    } else if (md.get_type() == VariantType::INT) {
        int feature_selected = md.as<int>();
        edited->set_disable_feature(EditorFeatureProfile::Feature(feature_selected), !checked);
        _save_and_update();
    }
}

void EditorFeatureProfileManager::_property_item_edited() {
    if (updating_features)
        return;

    TreeItem *class_item = class_list->get_selected();
    if (!class_item) {
        return;
    }

    Variant md = class_item->get_metadata(0);
    if (md.get_type() != VariantType::STRING && md.get_type() != VariantType::STRING_NAME) {
        return;
    }

    StringName class_name = md.as<StringName>();

    TreeItem *item = property_list->get_edited();
    if (!item) {
        return;
    }
    bool checked = item->is_checked(0);

    md = item->get_metadata(0);
    if (md.get_type() == VariantType::STRING || md.get_type() == VariantType::STRING_NAME) {
        StringName property_selected = md.as<StringName>();
        edited->set_disable_class_property(class_name, property_selected, !checked);
        _save_and_update();
        _update_selected_profile();
    } else if (md.get_type() == VariantType::INT) {
        int feature_selected = md.as<int>();
        switch (feature_selected) {
            case CLASS_OPTION_DISABLE_EDITOR: {
                edited->set_disable_class_editor(class_name, !checked);
                _save_and_update();
                _update_selected_profile();
            } break;
        }
    }
}

void EditorFeatureProfileManager::_update_selected_profile() {

    StringName class_selected;
    int feature_selected = -1;

    if (class_list->get_selected()) {
        Variant md = class_list->get_selected()->get_metadata(0);
        if (md.get_type() == VariantType::STRING||md.get_type() == VariantType::STRING_NAME) {
            class_selected = md.as<StringName>();
        } else if (md.get_type() == VariantType::INT) {
            feature_selected = md.as<int>();
        }
    }

    class_list->clear();

    String profile(_get_selected_profile());
    if (profile.empty()) { //nothing selected, nothing edited
        property_list->clear();
        edited.unref();
        return;
    }

    if (profile == current_profile) {
        edited = current; //reuse current profile (which is what editor uses)
        ERR_FAIL_COND(not current); //nothing selected, current should never be null
    } else {
        //reload edited, if different from current
        edited = make_ref_counted<EditorFeatureProfile>();
        Error err = edited->load_from_file(PathUtils::plus_file(EditorSettings::get_singleton()->get_feature_profiles_dir(),profile + ".profile"));
        ERR_FAIL_COND_MSG(err != OK, "Error when loading EditorSettings from file '" + PathUtils::plus_file(EditorSettings::get_singleton()->get_feature_profiles_dir(),profile + ".profile") + "'.");
    }

    updating_features = true;

    TreeItem *root = class_list->create_item();

    TreeItem *features = class_list->create_item(root);
    features->set_text(0, TTR("Enabled Features:"));
    for (int i = 0; i < EditorFeatureProfile::FEATURE_MAX; i++) {

        TreeItem *feature = class_list->create_item(features);
        feature->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
        feature->set_text(0, TTRGET(EditorFeatureProfile::get_feature_name(EditorFeatureProfile::Feature(i))));
        feature->set_selectable(0, true);
        feature->set_editable(0, true);
        feature->set_metadata(0, i);
        if (!edited->is_feature_disabled(EditorFeatureProfile::Feature(i))) {
            feature->set_checked(0, true);
        }

        if (i == feature_selected) {
            feature->select(0);
        }
    }

    TreeItem *classes = class_list->create_item(root);
    classes->set_text(0, TTR("Enabled Classes:"));

    _fill_classes_from(classes, "Node", class_selected);
    _fill_classes_from(classes, "Resource", class_selected);

    updating_features = false;

    _class_list_item_selected();
}

void EditorFeatureProfileManager::_import_profiles(const Vector<String> &p_paths) {

    //test it first
    for (const String &path : p_paths) {
        Ref<EditorFeatureProfile> profile(make_ref_counted<EditorFeatureProfile>());
        Error err = profile->load_from_file(path);
        String basefile(PathUtils::get_file(path));
        if (err != OK) {
            EditorNode::get_singleton()->show_warning(FormatSN(TTR("File '%s' format is invalid, import aborted.").asCString(), basefile.c_str()));
            return;
        }

        String dst_file = PathUtils::plus_file(EditorSettings::get_singleton()->get_feature_profiles_dir(),basefile);
        StringView basename(PathUtils::get_basename(basefile));
        if (FileAccess::exists(dst_file)) {
            EditorNode::get_singleton()->show_warning(FormatSN(
                    TTR("Profile '%.*s' already exists. Remove it first before importing, import aborted.").asCString(),
                    basename.length(),basename.data()));
            return;
        }
    }

    //do it second
    for (int i = 0; i < p_paths.size(); i++) {
        Ref<EditorFeatureProfile> profile(make_ref_counted<EditorFeatureProfile>());
        Error err = profile->load_from_file(p_paths[i]);
        ERR_CONTINUE(err != OK);
        String basefile(PathUtils::get_file(p_paths[i]));
        String dst_file = PathUtils::plus_file(EditorSettings::get_singleton()->get_feature_profiles_dir(),basefile);
        profile->save_to_file(dst_file);
    }

    _update_profile_list();
}

void EditorFeatureProfileManager::_export_profile(StringView p_path) {

    ERR_FAIL_COND(not edited);
    Error err = edited->save_to_file(p_path);
    if (err != OK) {
        EditorNode::get_singleton()->show_warning(FormatSN(TTR("Error saving profile to path: '%.*s'.").asCString(), p_path.length(),p_path.data()));
    }
}

void EditorFeatureProfileManager::_save_and_update() {

    String edited_path = _get_selected_profile();
    ERR_FAIL_COND(edited_path.empty());
    ERR_FAIL_COND(not edited);

    edited->save_to_file(PathUtils::plus_file(EditorSettings::get_singleton()->get_feature_profiles_dir(),edited_path + ".profile"));

    if (edited == current) {
        update_timer->start();
    }
}

void EditorFeatureProfileManager::_emit_current_profile_changed() {

    emit_signal("current_feature_profile_changed");
}

void EditorFeatureProfileManager::notify_changed() {
    _emit_current_profile_changed();
}

Ref<EditorFeatureProfile> EditorFeatureProfileManager::get_current_profile() {
    return current;
}

EditorFeatureProfileManager *EditorFeatureProfileManager::singleton = nullptr;

void EditorFeatureProfileManager::_bind_methods() {

    MethodBinder::bind_method("_update_selected_profile", &EditorFeatureProfileManager::_update_selected_profile);
    MethodBinder::bind_method("_profile_action", &EditorFeatureProfileManager::_profile_action);
    MethodBinder::bind_method("_create_new_profile", &EditorFeatureProfileManager::_create_new_profile);
    MethodBinder::bind_method("_profile_selected", &EditorFeatureProfileManager::_profile_selected);
    MethodBinder::bind_method("_erase_selected_profile", &EditorFeatureProfileManager::_erase_selected_profile);
    MethodBinder::bind_method("_import_profiles", &EditorFeatureProfileManager::_import_profiles);
    MethodBinder::bind_method("_export_profile", &EditorFeatureProfileManager::_export_profile);
    MethodBinder::bind_method("_class_list_item_selected", &EditorFeatureProfileManager::_class_list_item_selected);
    MethodBinder::bind_method("_class_list_item_edited", &EditorFeatureProfileManager::_class_list_item_edited);
    MethodBinder::bind_method("_property_item_edited", &EditorFeatureProfileManager::_property_item_edited);
    MethodBinder::bind_method("_emit_current_profile_changed", &EditorFeatureProfileManager::_emit_current_profile_changed);

    ADD_SIGNAL(MethodInfo("current_feature_profile_changed"));
}

EditorFeatureProfileManager::EditorFeatureProfileManager() {

    VBoxContainer *main_vbc = memnew(VBoxContainer);
    add_child(main_vbc);

    HBoxContainer *name_hbc = memnew(HBoxContainer);
    current_profile_name = memnew(LineEdit);
    name_hbc->add_child(current_profile_name);
    current_profile_name->set_editable(false);
    current_profile_name->set_h_size_flags(SIZE_EXPAND_FILL);
    profile_actions[PROFILE_CLEAR] = memnew(Button(TTR("Unset")));
    name_hbc->add_child(profile_actions[PROFILE_CLEAR]);
    profile_actions[PROFILE_CLEAR]->set_disabled(true);
    profile_actions[PROFILE_CLEAR]->connect("pressed",callable_mp(this, &ClassName::_profile_action), varray(PROFILE_CLEAR));

    main_vbc->add_margin_child(TTR("Current Profile:"), name_hbc);

    HBoxContainer *profiles_hbc = memnew(HBoxContainer);
    profile_list = memnew(OptionButton);
    profile_list->set_h_size_flags(SIZE_EXPAND_FILL);
    profiles_hbc->add_child(profile_list);
    profile_list->connect("item_selected",callable_mp(this, &ClassName::_profile_selected));

    profile_actions[PROFILE_SET] = memnew(Button(TTR("Make Current")));
    profiles_hbc->add_child(profile_actions[PROFILE_SET]);
    profile_actions[PROFILE_SET]->set_disabled(true);
    profile_actions[PROFILE_SET]->connect("pressed",callable_mp(this, &ClassName::_profile_action), varray(PROFILE_SET));

    profile_actions[PROFILE_ERASE] = memnew(Button(TTR("Remove")));
    profiles_hbc->add_child(profile_actions[PROFILE_ERASE]);
    profile_actions[PROFILE_ERASE]->set_disabled(true);
    profile_actions[PROFILE_ERASE]->connect("pressed",callable_mp(this, &ClassName::_profile_action), varray(PROFILE_ERASE));

    profiles_hbc->add_child(memnew(VSeparator));

    profile_actions[PROFILE_NEW] = memnew(Button(TTR("New")));
    profiles_hbc->add_child(profile_actions[PROFILE_NEW]);
    profile_actions[PROFILE_NEW]->connect("pressed",callable_mp(this, &ClassName::_profile_action), varray(PROFILE_NEW));

    profiles_hbc->add_child(memnew(VSeparator));

    profile_actions[PROFILE_IMPORT] = memnew(Button(TTR("Import")));
    profiles_hbc->add_child(profile_actions[PROFILE_IMPORT]);
    profile_actions[PROFILE_IMPORT]->connect("pressed",callable_mp(this, &ClassName::_profile_action), varray(PROFILE_IMPORT));

    profile_actions[PROFILE_EXPORT] = memnew(Button(TTR("Export")));
    profiles_hbc->add_child(profile_actions[PROFILE_EXPORT]);
    profile_actions[PROFILE_EXPORT]->set_disabled(true);
    profile_actions[PROFILE_EXPORT]->connect("pressed",callable_mp(this, &ClassName::_profile_action), varray(PROFILE_EXPORT));

    main_vbc->add_margin_child(TTR("Available Profiles:"), profiles_hbc);

    h_split = memnew(HSplitContainer);
    h_split->set_v_size_flags(SIZE_EXPAND_FILL);
    main_vbc->add_child(h_split);

    VBoxContainer *class_list_vbc = memnew(VBoxContainer);
    h_split->add_child(class_list_vbc);
    class_list_vbc->set_h_size_flags(SIZE_EXPAND_FILL);

    class_list = memnew(Tree);
    class_list_vbc->add_margin_child(TTR("Enabled Classes:"), class_list, true);
    class_list->set_hide_root(true);
    class_list->set_edit_checkbox_cell_only_when_checkbox_is_pressed(true);
    class_list->connect("cell_selected",callable_mp(this, &ClassName::_class_list_item_selected));
    class_list->connect("item_edited",callable_mp(this, &ClassName::_class_list_item_edited), varray(), ObjectNS::CONNECT_QUEUED);

    VBoxContainer *property_list_vbc = memnew(VBoxContainer);
    h_split->add_child(property_list_vbc);
    property_list_vbc->set_h_size_flags(SIZE_EXPAND_FILL);

    property_list = memnew(Tree);
    property_list_vbc->add_margin_child(TTR("Class Options"), property_list, true);
    property_list->set_hide_root(true);
    property_list->set_hide_folding(true);
    property_list->set_edit_checkbox_cell_only_when_checkbox_is_pressed(true);
    property_list->connect("item_edited",callable_mp(this, &ClassName::_property_item_edited), varray(), ObjectNS::CONNECT_QUEUED);

    new_profile_dialog = memnew(ConfirmationDialog);
    new_profile_dialog->set_title(TTR("New profile name:"));
    new_profile_name = memnew(LineEdit);
    new_profile_dialog->add_child(new_profile_name);
    new_profile_name->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
    add_child(new_profile_dialog);
    new_profile_dialog->connect("confirmed",callable_mp(this, &ClassName::_create_new_profile));
    new_profile_dialog->register_text_enter(new_profile_name);
    new_profile_dialog->get_ok()->set_text(TTR("Create"));

    erase_profile_dialog = memnew(ConfirmationDialog);
    add_child(erase_profile_dialog);
    erase_profile_dialog->set_title(TTR("Erase Profile"));
    erase_profile_dialog->connect("confirmed",callable_mp(this, &ClassName::_erase_selected_profile));

    import_profiles = memnew(EditorFileDialog);
    add_child(import_profiles);
    import_profiles->set_mode(EditorFileDialog::MODE_OPEN_FILES);
    import_profiles->add_filter("*.profile; " + TTR("Godot Feature Profile"));
    import_profiles->connect("files_selected",callable_mp(this, &ClassName::_import_profiles));
    import_profiles->set_title(TTR("Import Profile(s)"));
    import_profiles->set_access(EditorFileDialog::ACCESS_FILESYSTEM);

    export_profile = memnew(EditorFileDialog);
    add_child(export_profile);
    export_profile->set_mode(EditorFileDialog::MODE_SAVE_FILE);
    export_profile->add_filter("*.profile; " + TTR("Godot Feature Profile"));
    export_profile->connect("file_selected",callable_mp(this, &ClassName::_export_profile));
    export_profile->set_title(TTR("Export Profile"));
    export_profile->set_access(EditorFileDialog::ACCESS_FILESYSTEM);

    set_title(TTR("Manage Editor Feature Profiles"));
    EDITOR_DEF("_default_feature_profile", "");

    update_timer = memnew(Timer);
    update_timer->set_wait_time(1); //wait a second before updating editor
    add_child(update_timer);
    update_timer->connect("timeout",callable_mp(this, &ClassName::_emit_current_profile_changed));
    update_timer->set_one_shot(true);

    updating_features = false;

    singleton = this;
}
