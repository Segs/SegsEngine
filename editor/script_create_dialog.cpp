/*************************************************************************/
/*  script_create_dialog.cpp                                             */
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

#include "script_create_dialog.h"

#include "core/callable_method_pointer.h"
#include "core/io/resource_saver.h"
#include "core/method_bind.h"
#include "core/os/file_access.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"
#include "core/string_formatter.h"
#include "core/script_language.h"
#include "core/string_builder.h"
#include "editor/create_dialog.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "scene/resources/style_box.h"

IMPL_GDCLASS(ScriptCreateDialog)

void ScriptCreateDialog::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_THEME_CHANGED:
        case NOTIFICATION_ENTER_TREE: {
            for (int i = 0; i < ScriptServer::get_language_count(); i++) {
                StringName lang(ScriptServer::get_language(i)->get_type());
                Ref<Texture> lang_icon = get_theme_icon(lang, "EditorIcons");
                if (lang_icon) {
                    language_menu->set_item_icon(i, lang_icon);
                }
            }
            String last_lang = EditorSettings::get_singleton()->get_project_metadataT<String>("script_setup", "last_selected_language", "");
            if (!last_lang.empty()) {
                for (int i = 0; i < language_menu->get_item_count(); i++) {
                    if (language_menu->get_item_text(i) == last_lang) {
                        language_menu->select(i);
                        current_language = i;
                        break;
                    }
                }
            } else {
                if(language_menu->get_item_count()>0)
                    language_menu->select(default_language);
            }

            path_button->set_button_icon(get_theme_icon("Folder", "EditorIcons"));
            parent_browse_button->set_button_icon(get_theme_icon("Folder", "EditorIcons"));
            parent_search_button->set_button_icon(get_theme_icon("ClassList", "EditorIcons"));
            status_panel->add_theme_style_override("panel", get_theme_stylebox("bg", "Tree"));
        } break;
    }
}

void ScriptCreateDialog::_path_hbox_sorted() {
    if (is_visible()) {
        int filename_start_pos = StringUtils::rfind(initial_bp,'/') + 1;
        int filename_end_pos = initial_bp.length();

        if (!is_built_in) {
            file_path->select(filename_start_pos, filename_end_pos);
        }

        // First set cursor to the end of line to scroll LineEdit view
        // to the right and then set the actual cursor position.
        file_path->set_cursor_position(file_path->get_text_ui().length());
        file_path->set_cursor_position(filename_start_pos);

        file_path->grab_focus();
    }
}

bool ScriptCreateDialog::_can_be_built_in() {
    return supports_built_in && built_in_enabled;
}

void ScriptCreateDialog::config(StringView p_base_name, StringView p_base_path, bool p_built_in_enabled, bool p_load_enabled) {

    class_name->set_text("");
    class_name->deselect();
    parent_name->set_text(p_base_name);
    parent_name->deselect();

    if (!p_base_path.empty()) {
        initial_bp = PathUtils::get_basename(p_base_path);
        file_path->set_text(initial_bp + "." + ScriptServer::get_language(language_menu->get_selected())->get_extension());
        current_language = language_menu->get_selected();
    } else {
        initial_bp = "";
        file_path->set_text("");
    }
    file_path->deselect();

    built_in_enabled = p_built_in_enabled;
    load_enabled = p_load_enabled;

    _lang_changed(current_language);
    _class_name_changed(StringView());
    _path_changed(file_path->get_text());
}

void ScriptCreateDialog::set_inheritance_base_type(const StringName &p_base) {

    base_type = p_base;
}

bool ScriptCreateDialog::_validate_parent(StringView p_string) {

    if (p_string.length() == 0)
        return false;

    if (can_inherit_from_file && StringUtils::is_quoted(p_string)) {
        StringView p = StringUtils::substr(p_string,1, p_string.length() - 2);
        if (_validate_path(p, true).empty())
            return true;
    }

    return ClassDB::class_exists(StringName(p_string)) || ScriptServer::is_global_class(StringName(p_string));
}

bool ScriptCreateDialog::_validate_class(const UIString &p_string) {

    if (p_string.length() == 0)
        return false;

    for (int i = 0; i < p_string.length(); i++) {

        if (i == 0) {
            if (p_string[0] >= '0' && p_string[0] <= '9')
                return false; // no start with number plz
        }

        bool valid_char = p_string[i].isDigit() || p_string[i].isLetter() ||
                          p_string[i] == '_' || p_string[i] == '.';

        if (!valid_char)
            return false;
    }

    return true;
}

StringName ScriptCreateDialog::_validate_path(StringView p_path, bool p_file_must_exist) {

    String p(StringUtils::strip_edges( p_path));

    if (p.empty()) return TTR("Path is empty.");
    if (PathUtils::get_basename(PathUtils::get_file(p)).empty()) return TTR("Filename is empty.");

    p = ProjectSettings::get_singleton()->localize_path(p);
    if (!StringUtils::begins_with(p,"res://")) return TTR("Path is not local.");

    DirAccess *d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    if (d->change_dir(PathUtils::get_base_dir(p)) != OK) {
        memdelete(d);
        return TTR("Invalid base path.");
    }
    memdelete(d);

    /* Does file already exist */
    DirAccess *f = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    if (f->dir_exists(p)) {
        memdelete(f);
        return TTR("A directory with the same name exists.");
    } else if (p_file_must_exist && !f->file_exists(p)) {
        memdelete(f);
        return TTR("File does not exist.");
    }
    memdelete(f);

    /* Check file extension */
    StringView extension = PathUtils::get_extension(p);
    Vector<String> extensions;

    // get all possible extensions for script
    for (int l = 0; l < language_menu->get_item_count(); l++) {
        ScriptServer::get_language(l)->get_recognized_extensions(&extensions);
    }

    bool found = false;
    bool match = false;
    int index = 0;
    for (const String &E : extensions) {
        if (StringUtils::compare(E,extension,StringUtils::CaseInsensitive) == 0) {
            //FIXME (?) - changing language this way doesn't update controls, needs rework
            //language_menu->select(index); // change Language option by extension
            found = true;
            if (E == ScriptServer::get_language(language_menu->get_selected())->get_extension()) {
                match = true;
            }
            break;
        }
        index++;
    }

    if (!found) return TTR("Invalid extension.");
    if (!match) return TTR("Wrong extension chosen.");

    /* Let ScriptLanguage do custom validation */
    String path_error = ScriptServer::get_language(language_menu->get_selected())->validate_path(p);
    if (!path_error.empty()) return StringName(path_error);

    /* All checks passed */
    return StringName();
}

String ScriptCreateDialog::_get_class_name() const {
    using namespace PathUtils;
    if (has_named_classes) {
        return class_name->get_text();
    } else {
        return String(get_basename(get_file(ProjectSettings::get_singleton()->localize_path(file_path->get_text()))));
    }
}
void ScriptCreateDialog::_class_name_changed(StringView p_name) {

    is_class_name_valid = _validate_class(class_name->get_text_ui());
    _update_dialog();
}

void ScriptCreateDialog::_parent_name_changed(StringView p_parent) {

    if (_validate_parent(parent_name->get_text())) {
        is_parent_name_valid = true;
    } else {
        is_parent_name_valid = false;
    }
    _update_dialog();
}

void ScriptCreateDialog::_template_changed(int p_template) {

    StringName selected_template = p_template == 0 ? StringName() : StringName(template_menu->get_item_text(p_template));
    EditorSettings::get_singleton()->set_project_metadata("script_setup", "last_selected_template", selected_template);
    if (p_template == 0) {
        //default
        script_template = "";
        return;
    }
    int selected_id = template_menu->get_selected_id();

    for (int i = 0; i < template_list.size(); i++) {
        const ScriptTemplateInfo &sinfo = template_list[i];
        if (sinfo.id == selected_id) {
            script_template = PathUtils::plus_file(sinfo.dir,sinfo.name + "." + sinfo.extension);
            break;
        }
    }
}

void ScriptCreateDialog::ok_pressed() {

    if (is_new_script_created) {
        _create_new();
    } else {
        _load_exist();
    }

    is_new_script_created = true;
    _update_dialog();
}

void ScriptCreateDialog::_create_new() {

    String cname_param = _get_class_name();

    Ref<Script> scr;
    if (!script_template.empty()) {
        scr = dynamic_ref_cast<Script>(gResourceManager().load(script_template));
        if (not scr) {
            alert->set_text(FormatSN(TTR("Error loading template '%s'").asCString(), script_template.c_str()));
            alert->popup_centered();
            return;
        }
        scr = dynamic_ref_cast<Script>(scr->duplicate());
        ScriptServer::get_language(language_menu->get_selected())->make_template(cname_param, parent_name->get_text(), scr);
    } else {
        scr = ScriptServer::get_language(language_menu->get_selected())->get_template(cname_param, parent_name->get_text());
    }

    if (has_named_classes) {
        String cname = class_name->get_text();
        if (cname.length())
            scr->set_name(cname);
    }

    if (is_built_in) {
        scr->set_name(internal_name->get_text());
    } else {
        String lpath = ProjectSettings::get_singleton()->localize_path(file_path->get_text());
        scr->set_path(lpath);
        Error err = gResourceManager().save(lpath, scr, ResourceManager::FLAG_CHANGE_PATH);
        if (err != OK) {
            alert->set_text(TTR("Error - Could not create script in filesystem."));
            alert->popup_centered();
            return;
        }
    }

    emit_signal("script_created", scr);
    hide();
}

void ScriptCreateDialog::_load_exist() {

    String path = file_path->get_text();
    RES p_script(gResourceManager().load(path, "Script"));
    if (not p_script) {
        alert->set_text(FormatSN(TTR("Error loading script from %s").asCString(), path.c_str()));
        alert->popup_centered();
        return;
    }

    emit_signal("script_created", Variant(p_script));
    hide();
}

void ScriptCreateDialog::_lang_changed(int l) {

    ScriptLanguage *language = ScriptServer::get_language(l);

    has_named_classes = language->has_named_classes();
    can_inherit_from_file = language->can_inherit_from_file();
    supports_built_in = language->supports_builtin_mode();
    if (!supports_built_in)
        is_built_in = false;

    String selected_ext = "." + language->get_extension();
    String path = file_path->get_text();
    String extension;
    if (!path.empty()) {
        if (StringUtils::contains(path,'.')) {
            extension = PathUtils::get_extension(path);
        }

        if (extension.length() == 0) {
            // add extension if none
            path += selected_ext;
            _path_changed(path);
        } else {
            // change extension by selected language
            Vector<String> extensions;
            // get all possible extensions for script
            for (int m = 0; m < language_menu->get_item_count(); m++) {
                ScriptServer::get_language(m)->get_recognized_extensions(&extensions);
            }

            for (const String &E : extensions) {
                if (StringUtils::compare(E,extension,StringUtils::CaseInsensitive) == 0) {
                    path = String(PathUtils::get_basename(path)) + selected_ext;
                    _path_changed(path);
                    break;
                }
            }
        }
    } else {
        path = "class" + selected_ext;
        _path_changed(path);
    }
    file_path->set_text(path);

    bool use_templates = language->is_using_templates();
    template_menu->set_disabled(!use_templates);
    template_menu->clear();

    if (use_templates) {
        _update_script_templates(language->get_extension());

        StringName last_lang = EditorSettings::get_singleton()->get_project_metadata("script_setup", "last_selected_language", StringName()).as<StringName>();
        StringName last_template = EditorSettings::get_singleton()->get_project_metadata("script_setup", "last_selected_template", StringName()).as<StringName>();

        template_menu->add_item(TTR("Default"));
        ScriptTemplateInfo *templates = template_list.data();

        const StringName origin_names[2] = {
            TTR("Project"),
            TTR("Editor")
        };
        int cur_origin = -1;

        // Populate script template items previously sorted and now grouped by origin
        for (int i = 0; i < template_list.size(); i++) {
            if (int(templates[i].origin) != cur_origin) {
                template_menu->add_separator();

                StringName origin_name = origin_names[templates[i].origin];

                int last_index = template_menu->get_item_count() - 1;
                template_menu->set_item_text(last_index, origin_name);

                cur_origin = templates[i].origin;
            }
            StringName item_name(StringUtils::capitalize(templates[i].name));
            template_menu->add_item(item_name);

            int new_id = template_menu->get_item_count() - 1;
            templates[i].id = new_id;
        }
        // Disable overridden
        for (eastl::pair<const String,Vector<int> > &E : template_overrides) {
            const Vector<int> &overrides = E.second;

            if (overrides.size() == 1) {
                continue; // doesn't override anything
            }
            const ScriptTemplateInfo &extended = template_list[overrides[0]];

            StringBuilder override_info;
            override_info += TTR("Overrides").asCString();
            override_info += ": ";

            for (int i = 1; i < overrides.size(); i++) {
                const ScriptTemplateInfo &overridden = template_list[overrides[i]];

                int disable_index = template_menu->get_item_index(overridden.id);
                template_menu->set_item_disabled(disable_index, true);

                override_info += origin_names[overridden.origin].asCString();
                if (i < overrides.size() - 1) {
                    override_info += ", ";
                }
            }
            template_menu->set_item_icon(extended.id, get_theme_icon("Override", "EditorIcons"));
            template_menu->get_popup()->set_item_tooltip(extended.id, StringName(override_info.as_string()));
        }
        // Reselect last selected template
        for (int i = 0; i < template_menu->get_item_count(); i++) {
            StringName ti(template_menu->get_item_text(i));
            if (language_menu->get_item_text(language_menu->get_selected()) == last_lang && last_template == ti) {
                template_menu->select(i);
                break;
            }
        }
    } else {

        template_menu->add_item(TTR("N/A"));
        script_template = "";
    }

    _template_changed(template_menu->get_selected());
    EditorSettings::get_singleton()->set_project_metadata("script_setup", "last_selected_language", language_menu->get_item_text(language_menu->get_selected()));

    _parent_name_changed(parent_name->get_text());
    _update_dialog();
}

void ScriptCreateDialog::_update_script_templates(const String &p_extension) {

    template_list.clear();
    template_overrides.clear();

    Vector<String> dirs;

    // Ordered from local to global for correct override mechanism
    dirs.emplace_back(EditorSettings::get_singleton()->get_project_script_templates_dir());
    dirs.emplace_back(EditorSettings::get_singleton()->get_script_templates_dir());

    for (size_t i = 0; i < dirs.size(); i++) {

        Vector<String> list(EditorSettings::get_singleton()->get_script_templates(p_extension, dirs[i]));

        for (const String & entry : list) {
            ScriptTemplateInfo sinfo;
            sinfo.origin = ScriptOrigin(i);
            sinfo.dir = dirs[i];
            sinfo.name = entry;
            sinfo.extension = p_extension;
            template_list.push_back(sinfo);

            if (!template_overrides.contains(sinfo.name)) {
                Vector<int> overrides;
                overrides.push_back(template_list.size() - 1); // first one
                template_overrides.emplace(sinfo.name, overrides);
            } else {
                Vector<int> &overrides = template_overrides[sinfo.name];
                overrides.push_back(template_list.size() - 1);
            }
        }
    }
}

void ScriptCreateDialog::_built_in_pressed() {

    if (internal->is_pressed()) {
        is_built_in = true;
        is_new_script_created = true;
    } else {
        is_built_in = false;
        _path_changed(file_path->get_text());
    }
    _update_dialog();
}

void ScriptCreateDialog::_browse_path(bool browse_parent, bool p_save) {

    is_browsing_parent = browse_parent;

    if (p_save) {
        file_browse->set_mode(EditorFileDialog::MODE_SAVE_FILE);
        file_browse->set_title(TTR("Open Script / Choose Location"));
        file_browse->get_ok()->set_text(TTR("Open"));
    } else {
        file_browse->set_mode(EditorFileDialog::MODE_OPEN_FILE);
        file_browse->set_title(TTR("Open Script"));
    }

    file_browse->set_disable_overwrite_warning(true);
    file_browse->clear_filters();
    Vector<String> extensions;

    int lang = language_menu->get_selected();
    ScriptServer::get_language(lang)->get_recognized_extensions(&extensions);

    for (const String &E : extensions) {
        file_browse->add_filter("*." + E);
    }

    file_browse->set_current_path(file_path->get_text());
    file_browse->popup_centered_ratio();
}

void ScriptCreateDialog::_file_selected(const String &p_file) {

    String p = ProjectSettings::get_singleton()->localize_path(p_file);
    if (is_browsing_parent) {
        parent_name->set_text("\"" + p + "\"");
        _parent_name_changed(parent_name->get_text());
    } else {
        file_path->set_text(p);
        _path_changed(p);

        String filename(PathUtils::get_basename(PathUtils::get_file(p)));
        auto select_start = StringUtils::rfind(p,filename);
        file_path->select(select_start, select_start + filename.length());
        file_path->set_cursor_position(select_start + filename.length());
        file_path->grab_focus();
    }
}

void ScriptCreateDialog::_create() {

    parent_name->set_text(StringUtils::split(select_class->get_selected_type(),' ')[0]);
    _parent_name_changed(parent_name->get_text());
}

void ScriptCreateDialog::_browse_class_in_tree() {

    select_class->set_base_type(StringName(base_type));
    select_class->popup_create(true);
}

void ScriptCreateDialog::_path_changed(StringView p_path) {
    if (is_built_in) {
        return;
    }

    is_path_valid = false;
    is_new_script_created = true;

    StringName path_error = _validate_path(p_path, false);
    if (!path_error.empty()) {
        _msg_path_valid(false, path_error);
        _update_dialog();
        return;
    }

    /* Does file already exist */
    DirAccess *f = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    String p = ProjectSettings::get_singleton()->localize_path(StringUtils::strip_edges(p_path));
    if (f->file_exists(p)) {
        is_new_script_created = false;
        _msg_path_valid(true, TTR("File exists, it will be reused."));
    }
    memdelete(f);

    is_path_valid = true;
    _update_dialog();
}

void ScriptCreateDialog::_path_entered(StringView p_path) {
    ok_pressed();
}

void ScriptCreateDialog::_msg_script_valid(bool valid, const StringName &p_msg) {

    error_label->set_text("- " + p_msg);
    if (valid) {
        error_label->add_theme_color_override("font_color", get_theme_color("success_color", "Editor"));
    } else {
        error_label->add_theme_color_override("font_color", get_theme_color("error_color", "Editor"));
    }
}

void ScriptCreateDialog::_msg_path_valid(bool valid, const StringName &p_msg) {

    path_error_label->set_text("- " + p_msg);
    if (valid) {
        path_error_label->add_theme_color_override("font_color", get_theme_color("success_color", "Editor"));
    } else {
        path_error_label->add_theme_color_override("font_color", get_theme_color("error_color", "Editor"));
    }
}

void ScriptCreateDialog::_update_dialog() {
    /* "Add Script Dialog" gui logic and script checks */

    bool script_ok = true;
    // Is script path/name valid (order from top to bottom)?

    if (!is_built_in && !is_path_valid) {
        _msg_script_valid(false, TTR("Invalid path."));
        script_ok = false;
    }
    if (has_named_classes && (is_new_script_created && !is_class_name_valid)) {
        _msg_script_valid(false, TTR("Invalid class name."));
        script_ok = false;
    }
    if (!is_parent_name_valid && is_new_script_created) {
        _msg_script_valid(false, TTR("Invalid inherited parent name or path."));
        script_ok = false;
    }
    if (script_ok) {
        _msg_script_valid(true, TTR("Script path/name is valid."));
    }

    // Does script have named classes?

    if (has_named_classes) {
        if (is_new_script_created) {
            class_name->set_editable(true);
            class_name->set_placeholder(TTR("Allowed: a-z, A-Z, 0-9, _ and ."));
            class_name->set_placeholder_alpha(0.3f);
        } else {
            class_name->set_editable(false);
        }
    } else {
        class_name->set_editable(false);
        class_name->set_placeholder(TTR("N/A"));
        class_name->set_placeholder_alpha(1);
        class_name->set_text("");
    }

    /* Is script Built-in */

    if (is_built_in) {
        file_path->set_editable(false);
        path_button->set_disabled(true);
        re_check_path = true;
    } else {
        file_path->set_editable(true);
        path_button->set_disabled(false);
        if (re_check_path) {
            re_check_path = false;
            _path_changed(file_path->get_text());
        }
    }

    if (!_can_be_built_in()) {
        internal->set_pressed(false);
    }
    internal->set_disabled(!_can_be_built_in());

    // Is Script created or loaded from existing file?

    builtin_warning_label->set_visible(is_built_in);
    path_controls[0]->set_visible(!is_built_in);
    path_controls[1]->set_visible(!is_built_in);
    name_controls[0]->set_visible(is_built_in);
    name_controls[1]->set_visible(is_built_in);

    // Check if the script name is the same as the parent class.
    // This warning isn't relevant if the script is built-in.
    script_name_warning_label->set_visible(!is_built_in && _get_class_name() == parent_name->get_text());

    if (is_built_in) {
        get_ok()->set_text(TTR("Create"));
        parent_name->set_editable(true);
        parent_search_button->set_disabled(false);
        parent_browse_button->set_disabled(!can_inherit_from_file);
        _msg_path_valid(true, TTR("Built-in script (into scene file)."));
    } else if (is_new_script_created) {
        // New script created.
        get_ok()->set_text(TTR("Create"));
        parent_name->set_editable(true);
        parent_search_button->set_disabled(false);
        parent_browse_button->set_disabled(!can_inherit_from_file);
        if (is_path_valid) {
            _msg_path_valid(true, TTR("Will create a new script file."));
        }
    } else if (load_enabled) {
        // Script loaded.
        get_ok()->set_text(TTR("Load"));
        parent_name->set_editable(false);
        parent_search_button->set_disabled(true);
        parent_browse_button->set_disabled(true);
        if (is_path_valid) {
            _msg_path_valid(true, TTR("Will load an existing script file."));
        }
    } else {
        get_ok()->set_text(TTR("Create"));
        parent_name->set_editable(true);
        parent_search_button->set_disabled(false);
        parent_browse_button->set_disabled(!can_inherit_from_file);
        _msg_path_valid(false, TTR("Script file already exists."));

        script_ok = false;
    }

    get_ok()->set_disabled(!script_ok);
    set_size(Vector2());
    minimum_size_changed();
}

void ScriptCreateDialog::_bind_methods() {

    MethodBinder::bind_method("_create", &ScriptCreateDialog::_create);

    MethodBinder::bind_method(D_METHOD("config", {"inherits", "path", "built_in_enabled","load_enabled"}), &ScriptCreateDialog::config, {DEFVAL(true),DEFVAL(true)});

    ADD_SIGNAL(MethodInfo("script_created", PropertyInfo(VariantType::OBJECT, "script", PropertyHint::ResourceType, "Script")));
}

ScriptCreateDialog::ScriptCreateDialog() {

    /* DIALOG */

    /* Main Controls */

    GridContainer *gc = memnew(GridContainer);
    gc->set_columns(2);

    /* Error Messages Field */

    VBoxContainer *vb = memnew(VBoxContainer);
    vb->set_custom_minimum_size(Size2(340, 30) * EDSCALE);

    error_label = memnew(Label);
    vb->add_child(error_label);

    path_error_label = memnew(Label);
    vb->add_child(path_error_label);

    builtin_warning_label = memnew(Label);
    builtin_warning_label->set_custom_minimum_size(Size2(340, 10) * EDSCALE);
    builtin_warning_label->set_text(
            TTR("Note: Built-in scripts have some limitations and can't be edited using an external editor."));
    vb->add_child(builtin_warning_label);
    builtin_warning_label->set_autowrap(true);
    builtin_warning_label->hide();

    script_name_warning_label = memnew(Label);
    script_name_warning_label->set_custom_minimum_size(Size2(340, 10) * EDSCALE);
    script_name_warning_label->set_text(
            TTR("Warning: Having the script name be the same as a built-in type is usually not desired."));
    vb->add_child(script_name_warning_label);
    script_name_warning_label->add_theme_color_override("font_color", Color(1, 0.85, 0.4));
    script_name_warning_label->set_autowrap(true);
    script_name_warning_label->hide();
    status_panel = memnew(PanelContainer);
    status_panel->set_custom_minimum_size(Size2(350, 40) * EDSCALE);
    status_panel->set_h_size_flags(Control::SIZE_FILL);
    status_panel->add_child(vb);

    /* Spacing */

    Control *spacing = memnew(Control);
    spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

    vb = memnew(VBoxContainer);
    vb->add_child(gc);
    vb->add_child(spacing);
    vb->add_child(status_panel);
    HBoxContainer *hb = memnew(HBoxContainer);
    hb->add_child(vb);

    add_child(hb);

    /* Language */

    language_menu = memnew(OptionButton);
    language_menu->set_custom_minimum_size(Size2(250, 0) * EDSCALE);
    language_menu->set_h_size_flags(SIZE_EXPAND_FILL);
    gc->add_child(memnew(Label(TTR("Language:"))));
    gc->add_child(language_menu);

    default_language = 0;
    for (int i = 0; i < ScriptServer::get_language_count(); i++) {

        StringName lang(ScriptServer::get_language(i)->get_name());
        language_menu->add_item(lang);
        if (lang == "GDScript") {
            default_language = i;
        }
    }
    if(ScriptServer::get_language_count()>0) {
        language_menu->select(default_language);
        current_language = default_language;
    }

    language_menu->connect("item_selected",callable_mp(this, &ClassName::_lang_changed));

    /* Inherits */

    base_type = "Object";

    hb = memnew(HBoxContainer);
    hb->set_h_size_flags(SIZE_EXPAND_FILL);
    parent_name = memnew(LineEdit);
    parent_name->connect("text_changed",callable_mp(this, &ClassName::_parent_name_changed));
    parent_name->set_h_size_flags(SIZE_EXPAND_FILL);
    hb->add_child(parent_name);
    parent_search_button = memnew(Button);
    parent_search_button->set_flat(true);
    parent_search_button->connect("pressed",callable_mp(this, &ClassName::_browse_class_in_tree));
    hb->add_child(parent_search_button);
    parent_browse_button = memnew(Button);
    parent_browse_button->set_flat(true);
    parent_browse_button->connectF("pressed",this,[=]() { _browse_path(true, false); });
    hb->add_child(parent_browse_button);
    gc->add_child(memnew(Label(TTR("Inherits:"))));
    gc->add_child(hb);
    is_browsing_parent = false;

    /* Class Name */

    class_name = memnew(LineEdit);
    class_name->connect("text_changed",callable_mp(this, &ClassName::_class_name_changed));
    class_name->set_h_size_flags(SIZE_EXPAND_FILL);
    gc->add_child(memnew(Label(TTR("Class Name:"))));
    gc->add_child(class_name);

    /* Templates */

    template_menu = memnew(OptionButton);
    gc->add_child(memnew(Label(TTR("Template:"))));
    gc->add_child(template_menu);
    template_menu->connect("item_selected",callable_mp(this, &ClassName::_template_changed));

    /* Built-in Script */

    internal = memnew(CheckBox);
    internal->set_text(TTR("On"));
    internal->connect("pressed",callable_mp(this, &ClassName::_built_in_pressed));
    gc->add_child(memnew(Label(TTR("Built-in Script:"))));
    gc->add_child(internal);

    /* Path */

    hb = memnew(HBoxContainer);
    hb->connect("sort_children",callable_mp(this, &ClassName::_path_hbox_sorted));
    file_path = memnew(LineEdit);
    file_path->connect("text_changed",callable_mp(this, &ClassName::_path_changed));
    file_path->connect("text_entered",callable_mp(this, &ClassName::_path_entered));
    file_path->set_h_size_flags(SIZE_EXPAND_FILL);
    hb->add_child(file_path);
    path_button = memnew(Button);
    path_button->set_flat(true);
    path_button->connectF("pressed",this,[=]() { _browse_path(false, true); });
    hb->add_child(path_button);
    Label *label = memnew(Label(TTR("Path:")));
    gc->add_child(label);
    gc->add_child(hb);
    re_check_path = false;
    path_controls[0] = label;
    path_controls[1] = hb;

    /* Name */

    internal_name = memnew(LineEdit);
    internal_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    label = memnew(Label(TTR("Name:")));
    gc->add_child(label);
    gc->add_child(internal_name);
    name_controls[0] = label;
    name_controls[1] = internal_name;
    label->hide();
    internal_name->hide();

    /* Dialog Setup */

    select_class = memnew(CreateDialog);
    select_class->connect("create",callable_mp(this, &ClassName::_create));
    add_child(select_class);

    file_browse = memnew(EditorFileDialog);
    file_browse->connect("file_selected",callable_mp(this, &ClassName::_file_selected));
    file_browse->set_mode(EditorFileDialog::MODE_OPEN_FILE);
    add_child(file_browse);
    get_ok()->set_text(TTR("Create"));
    alert = memnew(AcceptDialog);
    alert->set_as_minsize();
    alert->get_label()->set_autowrap(true);
    alert->get_label()->set_align(Label::ALIGN_CENTER);
    alert->get_label()->set_valign(Label::VALIGN_CENTER);
    alert->get_label()->set_custom_minimum_size(Size2(325, 60) * EDSCALE);
    add_child(alert);

    set_as_minsize();
    set_hide_on_ok(false);
    set_title(TTR("Attach Node Script"));

    is_parent_name_valid = false;
    is_class_name_valid = false;
    is_path_valid = false;

    has_named_classes = false;
    supports_built_in = false;
    can_inherit_from_file = false;
    is_built_in = false;
    built_in_enabled = true;
    load_enabled = true;

    is_new_script_created = true;
}
