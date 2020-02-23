/*************************************************************************/
/*  editor_settings.cpp                                                  */
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

#include "editor_settings.h"

#include "core/method_bind.h"
#include "core/container_tools.h"
#include "core/io/compression.h"
#include "core/io/config_file.h"
#include "core/io/file_access_memory.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/translation_loader_po.h"
#include "core/io/ip.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/keyboard.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/se_string.h"
#include "core/version.h"
#include "editor/editor_node.h"
#include "editor/translations.gen.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"

#include "EASTL/sort.h"
#include <QtCore/QResource>

#define _SYSTEM_CERTS_PATH ""

IMPL_GDCLASS(EditorSettings)

// PRIVATE METHODS

Ref<EditorSettings> EditorSettings::singleton = Ref<EditorSettings>();

// Properties

bool EditorSettings::_set(const StringName &p_name, const Variant &p_value) {

    _THREAD_SAFE_METHOD_

    bool changed = _set_only(p_name, p_value);
    if (changed) {
        emit_signal("settings_changed");
    }
    return true;
}

bool EditorSettings::_set_only(const StringName &p_name, const Variant &p_value) {

    _THREAD_SAFE_METHOD_

    if (se_string_view(p_name) == se_string_view("shortcuts")) {

        Array arr = p_value;
        ERR_FAIL_COND_V(!arr.empty() && arr.size() & 1, true);
        for (int i = 0; i < arr.size(); i += 2) {

            String name = arr[i];
            Ref<InputEvent> shortcut(arr[i + 1]);

            Ref<ShortCut> sc(make_ref_counted<ShortCut>());
            sc->set_shortcut(shortcut);
            add_shortcut(name, sc);
        }

        return false;
    }

    bool changed = false;

    if (p_value.get_type() == VariantType::NIL) {
        if (props.contains(p_name)) {
            props.erase(p_name);
            changed = true;
        }
    } else {
        if (props.contains(p_name)) {
            if (p_value != props[p_name].variant) {
                props[p_name].variant = p_value;
                changed = true;
            }
        } else {
            props[p_name] = VariantContainer(p_value, last_order++);
            changed = true;
        }

        if (save_changed_setting) {
            if (!props[p_name].save) {
                props[p_name].save = true;
                changed = true;
            }
        }
    }

    return changed;
}

bool EditorSettings::_get(const StringName &p_name, Variant &r_ret) const {

    _THREAD_SAFE_METHOD_

    if (p_name == StringName("shortcuts")) {

        Array arr;
        for (const eastl::pair<const String,Ref<ShortCut> > &E : shortcuts) {

            Ref<ShortCut> sc = E.second;

            if (optimize_save) {
                if (!sc->has_meta("original")) {
                    continue; //this came from settings but is not any longer used
                }

                Ref<InputEvent> original(sc->get_meta("original"));
                if ((not original && not sc->get_shortcut()) || sc->is_shortcut(original))
                    continue; //not changed from default, don't save
            }

            arr.push_back(E.first);
            arr.push_back(sc->get_shortcut());
        }
        r_ret = arr;
        return true;
    }

    auto v = props.find(p_name);
    if (v==props.end()) {
        WARN_PRINT("EditorSettings::_get - Property not found: " + String(p_name));
        return false;
    }
    r_ret = v->second.variant;
    return true;
}

void EditorSettings::_initial_set(const StringName &p_name, const Variant &p_value) {
    set(p_name, p_value);
    props[p_name].initial = p_value;
    props[p_name].has_default_value = true;
}

struct _EVCSort {

    StringName name;
    VariantType type;
    int order;
    bool save;
    bool restart_if_changed;

    bool operator<(const _EVCSort &p_vcs) const { return order < p_vcs.order; }
};

void EditorSettings::_get_property_list(Vector<PropertyInfo> *p_list) const {

    _THREAD_SAFE_METHOD_

    Set<_EVCSort> vclist;

    for(const auto & prop : props) {

        const VariantContainer &v = prop.second;

        if (v.hide_from_editor)
            continue;

        _EVCSort vc;
        vc.name = prop.first;
        vc.order = v.order;
        vc.type = v.variant.get_type();
        vc.save = v.save;
        /*if (vc.save) { this should be implemented, but lets do after 3.1 is out.
            if (v->initial.get_type() != VariantType::NIL && v->initial == v->variant) {
                vc.save = false;
            }
        }*/
        vc.restart_if_changed = v.restart_if_changed;

        vclist.insert(vc);
    }

    for (const _EVCSort &E : vclist) {

        int pinfo = 0;
        if (E.save || !optimize_save) {
            pinfo |= PROPERTY_USAGE_STORAGE;
        }

        if (!StringUtils::begins_with(E.name,"_") && !StringUtils::begins_with(E.name,"projects/")) {
            pinfo |= PROPERTY_USAGE_EDITOR;
        } else {
            pinfo |= PROPERTY_USAGE_STORAGE; //hiddens must always be saved
        }

        PropertyInfo pi(E.type, E.name);
        pi.usage = pinfo;
        if (hints.contains(E.name))
            pi = hints.at(E.name);

        if (E.restart_if_changed) {
            pi.usage |= PROPERTY_USAGE_RESTART_IF_CHANGED;
        }
        p_list->push_back(pi);
    }

    p_list->push_back(PropertyInfo(VariantType::ARRAY, "shortcuts", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL)); //do not edit
}

void EditorSettings::_add_property_info_bind(const Dictionary &p_info) {

    ERR_FAIL_COND(!p_info.has("name"));
    ERR_FAIL_COND(!p_info.has("type"));

    PropertyInfo pinfo;
    pinfo.name = p_info["name"];
    ERR_FAIL_COND(!props.contains(pinfo.name));
    pinfo.type = VariantType(p_info["type"].operator int());
    ERR_FAIL_INDEX((int)pinfo.type, (int)VariantType::VARIANT_MAX);

    if (p_info.has("hint"))
        pinfo.hint = PropertyHint(p_info["hint"].operator int());
    if (p_info.has("hint_string"))
        pinfo.hint_string = p_info["hint_string"].as<String>();

    add_property_hint(pinfo);
}

// Default configs
bool EditorSettings::has_default_value(const StringName &p_setting) const {

    _THREAD_SAFE_METHOD_

    if (!props.contains(p_setting))
        return false;
    return const_cast<EditorSettings *>(this)->props[p_setting].has_default_value;
}

void EditorSettings::_load_defaults(const Ref<ConfigFile> &p_extra_config) {

    _THREAD_SAFE_METHOD_
    /* Languages */

    {
        String lang_hint("en");
        String host_lang = TranslationServer::standardize_locale(OS::get_singleton()->get_locale());
        // Some locales are not properly supported currently in Godot due to lack of font shaping
        // (e.g. Arabic or Hindi), so even though we have work in progress translations for them,
        // we skip them as they don't render properly. (GH-28577)
        const se_string_view locales_to_skip[10] = {"ar","bn","fa","he","hi","ml","si","ta","te","ur"};

        se_string_view best;

        EditorTranslationList *etl = _editor_translations;

        while (etl->data) {

            const se_string_view &locale = etl->lang;
            // Skip locales which we can't render properly (see above comment).
            // Test against language code without regional variants (e.g. ur_PK).
            auto lang_code = StringUtils::get_slice(locale,'_', 0);
            if (ContainerUtils::contains(locales_to_skip,lang_code)) {
                etl++;
                continue;
            }
            lang_hint += ",";
            lang_hint += locale;

            if (0==locale.compare(host_lang)) {
                best = locale;
            }

            if (best.empty() && StringUtils::begins_with(host_lang,locale)) {
                best = locale;
            }

            etl++;
        }

        if (best.empty()) {
            best = "en";
        }

        _initial_set("interface/editor/editor_language", best);
        set_restart_if_changed("interface/editor/editor_language", true);
        hints[StringName("interface/editor/editor_language")] = PropertyInfo(VariantType::STRING, "interface/editor/editor_language", PropertyHint::Enum, lang_hint, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
    }

    /* Interface */

    // Editor
    _initial_set("interface/editor/display_scale", 0);
    hints["interface/editor/display_scale"] = PropertyInfo(VariantType::INT, "interface/editor/display_scale", PropertyHint::Enum, "Auto,75%,100%,125%,150%,175%,200%,Custom", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
    _initial_set("interface/editor/custom_display_scale", 1.0f);
    hints["interface/editor/custom_display_scale"] = PropertyInfo(VariantType::REAL, "interface/editor/custom_display_scale", PropertyHint::Range, "0.5,3,0.01", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
    _initial_set("interface/editor/main_font_size", 14);
    hints["interface/editor/main_font_size"] = PropertyInfo(VariantType::INT, "interface/editor/main_font_size", PropertyHint::Range, "8,48,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
    _initial_set("interface/editor/code_font_size", 14);
    hints["interface/editor/code_font_size"] = PropertyInfo(VariantType::INT, "interface/editor/code_font_size", PropertyHint::Range, "8,48,1", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/editor/font_antialiased", true);
    _initial_set("interface/editor/font_hinting", 0);
    hints["interface/editor/font_hinting"] = PropertyInfo(VariantType::INT, "interface/editor/font_hinting", PropertyHint::Enum, "Auto,None,Light,Normal", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/editor/main_font", "");
    hints["interface/editor/main_font"] = PropertyInfo(VariantType::STRING, "interface/editor/main_font", PropertyHint::GlobalFile, "*.ttf,*.otf", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/editor/main_font_bold", "");
    hints["interface/editor/main_font_bold"] = PropertyInfo(VariantType::STRING, "interface/editor/main_font_bold", PropertyHint::GlobalFile, "*.ttf,*.otf", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/editor/code_font", "");
    hints["interface/editor/code_font"] = PropertyInfo(VariantType::STRING, "interface/editor/code_font", PropertyHint::GlobalFile, "*.ttf,*.otf", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/editor/dim_editor_on_dialog_popup", true);
    _initial_set("interface/editor/low_processor_mode_sleep_usec", 6900); // ~144 FPS
    hints["interface/editor/low_processor_mode_sleep_usec"] = PropertyInfo(VariantType::REAL, "interface/editor/low_processor_mode_sleep_usec", PropertyHint::Range, "1,100000,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
    _initial_set("interface/editor/unfocused_low_processor_mode_sleep_usec", 50000); // 20 FPS
    hints["interface/editor/unfocused_low_processor_mode_sleep_usec"] = PropertyInfo(VariantType::REAL, "interface/editor/unfocused_low_processor_mode_sleep_usec", PropertyHint::Range, "1,100000,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);

    _initial_set("interface/editor/separate_distraction_mode", false);

    _initial_set("interface/editor/automatically_open_screenshots", true);
    _initial_set("interface/editor/hide_console_window", false);
    _initial_set("interface/editor/save_each_scene_on_quit", true); // Regression
    _initial_set("interface/editor/quit_confirmation", true);

    // Theme
    _initial_set("interface/theme/preset", "Default");
    hints["interface/theme/preset"] = PropertyInfo(VariantType::STRING, "interface/theme/preset", PropertyHint::Enum, "Default,Alien,Arc,Godot 2,Grey,Light,Solarized (Dark),Solarized (Light),Custom", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/theme/icon_and_font_color", 0);
    hints["interface/theme/icon_and_font_color"] = PropertyInfo(VariantType::INT, "interface/theme/icon_and_font_color", PropertyHint::Enum, "Auto,Dark,Light", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/theme/base_color", Color(0.2f, 0.23f, 0.31f));
    hints["interface/theme/base_color"] = PropertyInfo(VariantType::COLOR, "interface/theme/base_color", PropertyHint::None, "", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/theme/accent_color", Color(0.41f, 0.61f, 0.91f));
    hints["interface/theme/accent_color"] = PropertyInfo(VariantType::COLOR, "interface/theme/accent_color", PropertyHint::None, "", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/theme/contrast", 0.25);
    hints["interface/theme/contrast"] = PropertyInfo(VariantType::REAL, "interface/theme/contrast", PropertyHint::Range, "0.01, 1, 0.01");
    _initial_set("interface/theme/relationship_line_opacity", 0.1);
    hints["interface/theme/relationship_line_opacity"] = PropertyInfo(VariantType::REAL, "interface/theme/relationship_line_opacity", PropertyHint::Range, "0.00, 1, 0.01");
    _initial_set("interface/theme/highlight_tabs", false);
    _initial_set("interface/theme/border_size", 1);
    _initial_set("interface/theme/use_graph_node_headers", false);
    hints["interface/theme/border_size"] = PropertyInfo(VariantType::INT, "interface/theme/border_size", PropertyHint::Range, "0,2,1", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/theme/additional_spacing", 0);
    hints["interface/theme/additional_spacing"] = PropertyInfo(VariantType::REAL, "interface/theme/additional_spacing", PropertyHint::Range, "0,5,0.1", PROPERTY_USAGE_DEFAULT);
    _initial_set("interface/theme/custom_theme", "");
    hints["interface/theme/custom_theme"] = PropertyInfo(VariantType::STRING, "interface/theme/custom_theme", PropertyHint::GlobalFile, "*.res,*.tres,*.theme", PROPERTY_USAGE_DEFAULT);

    // Scene tabs
    _initial_set("interface/scene_tabs/show_extension", false);
    _initial_set("interface/scene_tabs/show_thumbnail_on_hover", true);
    _initial_set("interface/scene_tabs/resize_if_many_tabs", true);
    _initial_set("interface/scene_tabs/minimum_width", 50);
    hints["interface/scene_tabs/minimum_width"] = PropertyInfo(VariantType::INT, "interface/scene_tabs/minimum_width", PropertyHint::Range, "50,500,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
    _initial_set("interface/scene_tabs/show_script_button", false);

    /* Filesystem */

    // Directories
    _initial_set("filesystem/directories/autoscan_project_path", "");
    hints["filesystem/directories/autoscan_project_path"] = PropertyInfo(VariantType::STRING, "filesystem/directories/autoscan_project_path", PropertyHint::GlobalDir);
    _initial_set("filesystem/directories/default_project_path", OS::get_singleton()->has_environment("HOME") ? OS::get_singleton()->get_environment("HOME") : OS::get_system_dir(OS::SYSTEM_DIR_DOCUMENTS));
    hints["filesystem/directories/default_project_path"] = PropertyInfo(VariantType::STRING, "filesystem/directories/default_project_path", PropertyHint::GlobalDir);

    // On save
    _initial_set("filesystem/on_save/compress_binary_resources", true);
    _initial_set("filesystem/on_save/safe_save_on_backup_then_rename", true);

    // File dialog
    _initial_set("filesystem/file_dialog/show_hidden_files", false);
    _initial_set("filesystem/file_dialog/display_mode", 0);
    hints["filesystem/file_dialog/display_mode"] = PropertyInfo(VariantType::INT, "filesystem/file_dialog/display_mode", PropertyHint::Enum, "Thumbnails,List");
    _initial_set("filesystem/file_dialog/thumbnail_size", 64);
    hints["filesystem/file_dialog/thumbnail_size"] = PropertyInfo(VariantType::INT, "filesystem/file_dialog/thumbnail_size", PropertyHint::Range, "32,128,16");

    // Import
    _initial_set("filesystem/import/pvrtc_texture_tool", "");
#ifdef WINDOWS_ENABLED
    hints["filesystem/import/pvrtc_texture_tool"] = PropertyInfo(VariantType::STRING, "filesystem/import/pvrtc_texture_tool", PropertyHint::GlobalFile, "*.exe");
#else
    hints["filesystem/import/pvrtc_texture_tool"] = PropertyInfo(VariantType::STRING, "filesystem/import/pvrtc_texture_tool", PropertyHint::GlobalFile, "");
#endif
    _initial_set("filesystem/import/pvrtc_fast_conversion", false);

    /* Docks */

    // SceneTree
    _initial_set("docks/scene_tree/start_create_dialog_fully_expanded", false);

    // FileSystem
    _initial_set("docks/filesystem/thumbnail_size", 64);
    hints["docks/filesystem/thumbnail_size"] = PropertyInfo(VariantType::INT, "docks/filesystem/thumbnail_size", PropertyHint::Range, "32,128,16");
    _initial_set("docks/filesystem/always_show_folders", true);

    // Property editor
    _initial_set("docks/property_editor/auto_refresh_interval", 0.3);

    /* Text editor */

    // Theme
    _initial_set("text_editor/theme/color_theme", "Adaptive");
    hints["text_editor/theme/color_theme"] = PropertyInfo(VariantType::STRING, "text_editor/theme/color_theme", PropertyHint::Enum, "Adaptive,Default,Custom");

    _initial_set("text_editor/theme/line_spacing", 6);
    hints["text_editor/theme/line_spacing"] = PropertyInfo(VariantType::INT, "text_editor/theme/line_spacing", PropertyHint::Range, "0,50,1");

    _load_default_text_editor_theme();

    // Highlighting
    _initial_set("text_editor/highlighting/syntax_highlighting", true);

    _initial_set("text_editor/highlighting/highlight_all_occurrences", true);
    _initial_set("text_editor/highlighting/highlight_current_line", true);
    _initial_set("text_editor/highlighting/highlight_type_safe_lines", true);

    // Indent
    _initial_set("text_editor/indent/type", 0);
    hints["text_editor/indent/type"] = PropertyInfo(VariantType::INT, "text_editor/indent/type", PropertyHint::Enum, "Tabs,Spaces");
    _initial_set("text_editor/indent/size", 4);
    hints["text_editor/indent/size"] = PropertyInfo(VariantType::INT, "text_editor/indent/size", PropertyHint::Range, "1, 64, 1"); // size of 0 crashes.
    _initial_set("text_editor/indent/auto_indent", true);
    _initial_set("text_editor/indent/convert_indent_on_save", true);
    _initial_set("text_editor/indent/draw_tabs", true);
    _initial_set("text_editor/indent/draw_spaces", false);

    // Navigation
    _initial_set("text_editor/navigation/smooth_scrolling", true);
    _initial_set("text_editor/navigation/v_scroll_speed", 80);
    _initial_set("text_editor/navigation/show_minimap", true);
    _initial_set("text_editor/navigation/minimap_width", 80);
    hints["text_editor/navigation/minimap_width"] = PropertyInfo(VariantType::INT, "text_editor/navigation/minimap_width", PropertyHint::Range, "50,250,1");

    // Appearance
    _initial_set("text_editor/appearance/show_line_numbers", true);
    _initial_set("text_editor/appearance/line_numbers_zero_padded", false);
    _initial_set("text_editor/appearance/show_bookmark_gutter", true);
    _initial_set("text_editor/appearance/show_breakpoint_gutter", true);
    _initial_set("text_editor/appearance/show_info_gutter", true);
    _initial_set("text_editor/appearance/code_folding", true);
    _initial_set("text_editor/appearance/word_wrap", false);
    _initial_set("text_editor/appearance/show_line_length_guideline", true);
    _initial_set("text_editor/appearance/line_length_guideline_column", 80);
    hints["text_editor/appearance/line_length_guideline_column"] = PropertyInfo(VariantType::INT, "text_editor/appearance/line_length_guideline_column", PropertyHint::Range, "20, 160, 1");

    // Script list
    _initial_set("text_editor/script_list/show_members_overview", true);

    // Files
    _initial_set("text_editor/files/trim_trailing_whitespace_on_save", false);
    _initial_set("text_editor/files/autosave_interval_secs", 0);
    _initial_set("text_editor/files/restore_scripts_on_load", true);

    // Tools
    _initial_set("text_editor/tools/create_signal_callbacks", true);
    _initial_set("text_editor/tools/sort_members_outline_alphabetically", false);

    // Cursor
    _initial_set("text_editor/cursor/scroll_past_end_of_file", false);
    _initial_set("text_editor/cursor/block_caret", false);
    _initial_set("text_editor/cursor/caret_blink", true);
    _initial_set("text_editor/cursor/caret_blink_speed", 0.5);
    hints["text_editor/cursor/caret_blink_speed"] = PropertyInfo(VariantType::REAL, "text_editor/cursor/caret_blink_speed", PropertyHint::Range, "0.1, 10, 0.01");
    _initial_set("text_editor/cursor/right_click_moves_caret", true);

    // Completion
    _initial_set("text_editor/completion/idle_parse_delay", 2.0);
    hints["text_editor/completion/idle_parse_delay"] = PropertyInfo(VariantType::REAL, "text_editor/completion/idle_parse_delay", PropertyHint::Range, "0.1, 10, 0.01");
    _initial_set("text_editor/completion/auto_brace_complete", true);
    _initial_set("text_editor/completion/code_complete_delay", 0.3);
    hints["text_editor/completion/code_complete_delay"] = PropertyInfo(VariantType::REAL, "text_editor/completion/code_complete_delay", PropertyHint::Range, "0.01, 5, 0.01");
    _initial_set("text_editor/completion/put_callhint_tooltip_below_current_line", true);
    _initial_set("text_editor/completion/callhint_tooltip_offset", Vector2());
    _initial_set("text_editor/completion/complete_file_paths", true);
    _initial_set("text_editor/completion/add_type_hints", false);
    _initial_set("text_editor/completion/use_single_quotes", false);

    // Help
    _initial_set("text_editor/help/show_help_index", true);
    _initial_set("text_editor/help/help_font_size", 15);
    hints["text_editor/help/help_font_size"] = PropertyInfo(VariantType::INT, "text_editor/help/help_font_size", PropertyHint::Range, "8,48,1");
    _initial_set("text_editor/help/help_source_font_size", 14);
    hints["text_editor/help/help_source_font_size"] = PropertyInfo(VariantType::INT, "text_editor/help/help_source_font_size", PropertyHint::Range, "8,48,1");
    _initial_set("text_editor/help/help_title_font_size", 23);
    hints["text_editor/help/help_title_font_size"] = PropertyInfo(VariantType::INT, "text_editor/help/help_title_font_size", PropertyHint::Range, "8,48,1");

    /* Editors */

    // GridMap
    _initial_set("editors/grid_map/pick_distance", 5000.0);

    // 3D
    _initial_set("editors/3d/primary_grid_color", Color(0.56f, 0.56f, 0.56f));
    hints["editors/3d/primary_grid_color"] = PropertyInfo(VariantType::COLOR, "editors/3d/primary_grid_color", PropertyHint::ColorNoAlpha, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);

    _initial_set("editors/3d/secondary_grid_color", Color(0.38f, 0.38f, 0.38f));
    hints["editors/3d/secondary_grid_color"] = PropertyInfo(VariantType::COLOR, "editors/3d/secondary_grid_color", PropertyHint::ColorNoAlpha, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);

    _initial_set("editors/3d/grid_size", 50);
    hints["editors/3d/grid_size"] = PropertyInfo(VariantType::INT, "editors/3d/grid_size", PropertyHint::Range, "1,500,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);

    _initial_set("editors/3d/primary_grid_steps", 10);
    hints["editors/3d/primary_grid_steps"] = PropertyInfo(VariantType::INT, "editors/3d/primary_grid_steps", PropertyHint::Range, "1,100,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);

    _initial_set("editors/3d/default_fov", 70.0);
    _initial_set("editors/3d/default_z_near", 0.05);
    _initial_set("editors/3d/default_z_far", 500.0);

    // 3D: Navigation
    _initial_set("editors/3d/navigation/navigation_scheme", 0);
    _initial_set("editors/3d/navigation/invert_y_axis", false);
    hints["editors/3d/navigation/navigation_scheme"] = PropertyInfo(VariantType::INT, "editors/3d/navigation/navigation_scheme", PropertyHint::Enum, "Godot,Maya,Modo");
    _initial_set("editors/3d/navigation/zoom_style", 0);
    hints["editors/3d/navigation/zoom_style"] = PropertyInfo(VariantType::INT, "editors/3d/navigation/zoom_style", PropertyHint::Enum, "Vertical, Horizontal");

    _initial_set("editors/3d/navigation/emulate_3_button_mouse", false);
    _initial_set("editors/3d/navigation/orbit_modifier", 0);
    hints["editors/3d/navigation/orbit_modifier"] = PropertyInfo(VariantType::INT, "editors/3d/navigation/orbit_modifier", PropertyHint::Enum, "None,Shift,Alt,Meta,Ctrl");
    _initial_set("editors/3d/navigation/pan_modifier", 1);
    hints["editors/3d/navigation/pan_modifier"] = PropertyInfo(VariantType::INT, "editors/3d/navigation/pan_modifier", PropertyHint::Enum, "None,Shift,Alt,Meta,Ctrl");
    _initial_set("editors/3d/navigation/zoom_modifier", 4);
    hints["editors/3d/navigation/zoom_modifier"] = PropertyInfo(VariantType::INT, "editors/3d/navigation/zoom_modifier", PropertyHint::Enum, "None,Shift,Alt,Meta,Ctrl");

    _initial_set("editors/3d/navigation/warped_mouse_panning", true);

    // 3D: Navigation feel
    _initial_set("editors/3d/navigation_feel/orbit_sensitivity", 0.4);
    hints["editors/3d/navigation_feel/orbit_sensitivity"] = PropertyInfo(VariantType::REAL, "editors/3d/navigation_feel/orbit_sensitivity", PropertyHint::Range, "0.0, 2, 0.01");

    _initial_set("editors/3d/navigation_feel/orbit_inertia", 0.05);
    hints["editors/3d/navigation_feel/orbit_inertia"] = PropertyInfo(VariantType::REAL, "editors/3d/navigation_feel/orbit_inertia", PropertyHint::Range, "0.0, 1, 0.01");
    _initial_set("editors/3d/navigation_feel/translation_inertia", 0.15);
    hints["editors/3d/navigation_feel/translation_inertia"] = PropertyInfo(VariantType::REAL, "editors/3d/navigation_feel/translation_inertia", PropertyHint::Range, "0.0, 1, 0.01");
    _initial_set("editors/3d/navigation_feel/zoom_inertia", 0.075);
    hints["editors/3d/navigation_feel/zoom_inertia"] = PropertyInfo(VariantType::REAL, "editors/3d/navigation_feel/zoom_inertia", PropertyHint::Range, "0.0, 1, 0.01");
    _initial_set("editors/3d/navigation_feel/manipulation_orbit_inertia", 0.075);
    hints["editors/3d/navigation_feel/manipulation_orbit_inertia"] = PropertyInfo(VariantType::REAL, "editors/3d/navigation_feel/manipulation_orbit_inertia", PropertyHint::Range, "0.0, 1, 0.01");
    _initial_set("editors/3d/navigation_feel/manipulation_translation_inertia", 0.075);
    hints["editors/3d/navigation_feel/manipulation_translation_inertia"] = PropertyInfo(VariantType::REAL, "editors/3d/navigation_feel/manipulation_translation_inertia", PropertyHint::Range, "0.0, 1, 0.01");

    // 3D: Freelook
    _initial_set("editors/3d/freelook/freelook_inertia", 0.1f);
    hints["editors/3d/freelook/freelook_inertia"] = PropertyInfo(VariantType::REAL, "editors/3d/freelook/freelook_inertia", PropertyHint::Range, "0.0, 1, 0.01");
    _initial_set("editors/3d/freelook/freelook_base_speed", 5.0f);
    hints["editors/3d/freelook/freelook_base_speed"] = PropertyInfo(VariantType::REAL, "editors/3d/freelook/freelook_base_speed", PropertyHint::Range, "0.0, 10, 0.01");
    _initial_set("editors/3d/freelook/freelook_activation_modifier", 0);
    hints["editors/3d/freelook/freelook_activation_modifier"] = PropertyInfo(VariantType::INT, "editors/3d/freelook/freelook_activation_modifier", PropertyHint::Enum, "None,Shift,Alt,Meta,Ctrl");
    _initial_set("editors/3d/freelook/freelook_speed_zoom_link", false);

    // 2D
    _initial_set("editors/2d/grid_color", Color(1.0f, 1.0f, 1.0f, 0.07f));
    _initial_set("editors/2d/guides_color", Color(0.6f, 0.0f, 0.8f));
    _initial_set("editors/2d/smart_snapping_line_color", Color(0.9f, 0.1f, 0.1f));
    _initial_set("editors/2d/bone_width", 5);
    _initial_set("editors/2d/bone_color1", Color(1.0f, 1.0f, 1.0f, 0.9f));
    _initial_set("editors/2d/bone_color2", Color(0.6f, 0.6f, 0.6f, 0.9f));
    _initial_set("editors/2d/bone_selected_color", Color(0.9f, 0.45f, 0.45f, 0.9f));
    _initial_set("editors/2d/bone_ik_color", Color(0.9f, 0.9f, 0.45f, 0.9f));
    _initial_set("editors/2d/bone_outline_color", Color(0.35f, 0.35f, 0.35f));
    _initial_set("editors/2d/bone_outline_size", 2);
    _initial_set("editors/2d/viewport_border_color", Color(0.4f, 0.4f, 1.0f, 0.4f));
    _initial_set("editors/2d/constrain_editor_view", true);
    _initial_set("editors/2d/warped_mouse_panning", true);
    _initial_set("editors/2d/simple_panning", false);
    _initial_set("editors/2d/scroll_to_pan", false);
    _initial_set("editors/2d/pan_speed", 20);

    // Polygon editor
    _initial_set("editors/poly_editor/point_grab_radius", 8);
    _initial_set("editors/poly_editor/show_previous_outline", true);

    // Animation
    _initial_set("editors/animation/autorename_animation_tracks", true);
    _initial_set("editors/animation/confirm_insert_track", true);
    _initial_set("editors/animation/onion_layers_past_color", Color(1, 0, 0));
    _initial_set("editors/animation/onion_layers_future_color", Color(0, 1, 0));

    /* Run */

    // Window placement
    _initial_set("run/window_placement/rect", 1);
    hints["run/window_placement/rect"] = PropertyInfo(VariantType::INT, "run/window_placement/rect", PropertyHint::Enum, "Top Left,Centered,Custom Position,Force Maximized,Force Fullscreen");
    String screen_hints("Same as Editor,Previous Monitor,Next Monitor");
    for (int i = 0; i < OS::get_singleton()->get_screen_count(); i++) {
        screen_hints += ",Monitor " + itos(i + 1);
    }
    _initial_set("run/window_placement/rect_custom_position", Vector2());
    _initial_set("run/window_placement/screen", 0);
    hints["run/window_placement/screen"] = PropertyInfo(VariantType::INT, StringName("run/window_placement/screen"), PropertyHint::Enum, screen_hints);

    // Auto save
    _initial_set("run/auto_save/save_before_running", true);

    // Output
    _initial_set("run/output/font_size", 13);
    hints["run/output/font_size"] = PropertyInfo(VariantType::INT, "run/output/font_size", PropertyHint::Range, "8,48,1");
    _initial_set("run/output/always_clear_output_on_play", true);
    _initial_set("run/output/always_open_output_on_play", true);
    _initial_set("run/output/always_close_output_on_stop", false);

    /* Network */

    // Debug
    _initial_set("network/debug/remote_host", "127.0.0.1"); // Hints provided in setup_network

    _initial_set("network/debug/remote_port", 6007);
    hints["network/debug/remote_port"] = PropertyInfo(VariantType::INT, "network/debug/remote_port", PropertyHint::Range, "1,65535,1");

    // SSL
    _initial_set("network/ssl/editor_ssl_certificates", _SYSTEM_CERTS_PATH);
    hints["network/ssl/editor_ssl_certificates"] = PropertyInfo(VariantType::STRING, "network/ssl/editor_ssl_certificates", PropertyHint::GlobalFile, "*.crt,*.pem");

    /* Extra config */

    _initial_set("project_manager/sorting_order", 0);
    hints["project_manager/sorting_order"] = PropertyInfo(VariantType::INT, "project_manager/sorting_order", PropertyHint::Enum, "Name,Path,Last Modified");

    if (!p_extra_config)
        return;

    if (p_extra_config->has_section("init_projects") && p_extra_config->has_section_key("init_projects", "list")) {

        PoolVector<String> list = p_extra_config->get_value("init_projects", "list").as<PoolVector<String>>();
        for (int i = 0; i < list.size(); i++) {

            const String &name = list[i];
            set(StringName("projects/" + StringUtils::replace(name,"/", "::")), list[i]);
        }
    }

    if (p_extra_config->has_section("presets")) {

        Vector<String> keys = p_extra_config->get_section_keys("presets");

        for (const String &key : keys) {

            Variant val = p_extra_config->get_value("presets", key);
            set(StringName(key), val);
        }
    }
}

void EditorSettings::_load_default_text_editor_theme() {
    bool dark_theme = is_dark_theme();

    _initial_set("text_editor/highlighting/symbol_color", Color(0.73f, 0.87f, 1.0));
    _initial_set("text_editor/highlighting/keyword_color", Color(1.0, 1.0, 0.7f));
    _initial_set("text_editor/highlighting/base_type_color", Color(0.64f, 1.0, 0.83f));
    _initial_set("text_editor/highlighting/engine_type_color", Color(0.51f, 0.83f, 1.0));
    _initial_set("text_editor/highlighting/user_type_color", Color(0.42f, 0.67f, 0.93f));
    _initial_set("text_editor/highlighting/comment_color", Color(0.4f, 0.4f, 0.4f));
    _initial_set("text_editor/highlighting/string_color", Color(0.94f, 0.43f, 0.75));
    _initial_set("text_editor/highlighting/background_color", dark_theme ? Color(0.0f, 0.0f, 0.0f, 0.23f) : Color(0.2f, 0.23f, 0.31f));
    _initial_set("text_editor/highlighting/completion_background_color", Color(0.17f, 0.16f, 0.2f));
    _initial_set("text_editor/highlighting/completion_selected_color", Color(0.26f, 0.26f, 0.27f));
    _initial_set("text_editor/highlighting/completion_existing_color", Color(0.13f, 0.87f, 0.87f, 0.87f));
    _initial_set("text_editor/highlighting/completion_scroll_color", Color(1, 1, 1));
    _initial_set("text_editor/highlighting/completion_font_color", Color(0.67f, 0.67f, 0.67f));
    _initial_set("text_editor/highlighting/text_color", Color(0.67f, 0.67f, 0.67f));
    _initial_set("text_editor/highlighting/line_number_color", Color(0.67f, 0.67f, 0.67f, 0.4f));
    _initial_set("text_editor/highlighting/safe_line_number_color", Color(0.67f, 0.78f, 0.67f, 0.6f));
    _initial_set("text_editor/highlighting/caret_color", Color(0.67f, 0.67f, 0.67f));
    _initial_set("text_editor/highlighting/caret_background_color", Color(0, 0, 0));
    _initial_set("text_editor/highlighting/text_selected_color", Color(0, 0, 0));
    _initial_set("text_editor/highlighting/selection_color", Color(0.41f, 0.61f, 0.91f, 0.35f));
    _initial_set("text_editor/highlighting/brace_mismatch_color", Color(1, 0.2f, 0.2f));
    _initial_set("text_editor/highlighting/current_line_color", Color(0.3f, 0.5f, 0.8f, 0.15f));
    _initial_set("text_editor/highlighting/line_length_guideline_color", Color(0.3f, 0.5f, 0.8f, 0.1f));
    _initial_set("text_editor/highlighting/word_highlighted_color", Color(0.8f, 0.9f, 0.9f, 0.15f));
    _initial_set("text_editor/highlighting/number_color", Color(0.92f, 0.58f, 0.2f));
    _initial_set("text_editor/highlighting/function_color", Color(0.4f, 0.64f, 0.81f));
    _initial_set("text_editor/highlighting/member_variable_color", Color(0.9f, 0.31f, 0.35f));
    _initial_set("text_editor/highlighting/mark_color", Color(1.0, 0.4f, 0.4f, 0.4f));
    _initial_set("text_editor/highlighting/bookmark_color", Color(0.08f, 0.49f, 0.98f));
    _initial_set("text_editor/highlighting/breakpoint_color", Color(0.8f, 0.8f, 0.4f, 0.2f));
    _initial_set("text_editor/highlighting/executing_line_color", Color(0.2f, 0.8f, 0.2f, 0.4f));
    _initial_set("text_editor/highlighting/code_folding_color", Color(0.8f, 0.8f, 0.8f, 0.8f));
    _initial_set("text_editor/highlighting/search_result_color", Color(0.05f, 0.25f, 0.05f, 1));
    _initial_set("text_editor/highlighting/search_result_border_color", Color(0.41f, 0.61f, 0.91f, 0.38f));

}

bool EditorSettings::_save_text_editor_theme(se_string_view p_file) {
    se_string_view theme_section("color_theme");
    Ref<ConfigFile> cf(make_ref_counted<ConfigFile>()); // hex is better?

    Vector<StringName> keys;
    props.keys_into(keys);
    //NOTE: original code was sorting by pointers her
    eastl::sort(keys.begin(),keys.end(),WrapAlphaCompare());

    for (const StringName &key : keys) {
        if (StringUtils::begins_with(key, "text_editor/highlighting/") && StringUtils::contains(key, "color")) {
            cf->set_value(theme_section, StringUtils::replace(key, "text_editor/highlighting/", ""),
                    ((Color)props[key].variant).to_html());
        }
    }

    Error err = cf->save(p_file);

    return err == OK;
}
bool EditorSettings::_is_default_text_editor_theme(se_string_view p_theme_name) {
    return p_theme_name == se_string_view("default") || p_theme_name == se_string_view("adaptive") || p_theme_name == se_string_view("custom");
}

static Dictionary _get_builtin_script_templates() {
    Dictionary templates;

    //No Comments
    templates["no_comments.gd"] =
            "extends %BASE%\n"
            "\n"
            "\n"
            "func _ready()%VOID_RETURN%:\n"
            "%TS%pass\n";

    //Empty
    templates["empty.gd"] =
            "extends %BASE%"
            "\n"
            "\n";

    return templates;
}

static void _create_script_templates(se_string_view p_path) {

    Dictionary templates = _get_builtin_script_templates();
    Vector<Variant> keys(templates.get_key_list());
    FileAccess *file = FileAccess::create(FileAccess::ACCESS_FILESYSTEM);

    DirAccess *dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    dir->change_dir(p_path);
    for (const Variant & k : keys) {
        if (!dir->file_exists(k.as<String>())) {
            Error err = file->reopen(PathUtils::plus_file(p_path,k.as<String>()), FileAccess::WRITE);
            ERR_FAIL_COND(err != OK);
            file->store_string(templates[k].as<String>());
            file->close();
        }
    }

    memdelete(dir);
    memdelete(file);
}

// PUBLIC METHODS

EditorSettings *EditorSettings::get_singleton() {

    return singleton.get();
}

void EditorSettings::create() {
    Q_INIT_RESOURCE(editor);

    if (singleton.get())
        return; //pointless

    String data_path;
    String data_dir;
    String config_path;
    String config_dir;
    String cache_path;
    String cache_dir;
    OS * os_ptr = OS::get_singleton();
    Ref<ConfigFile> extra_config(make_ref_counted<ConfigFile>());

    String exe_path = PathUtils::get_base_dir(os_ptr->get_executable_path());
    DirAccess *d = DirAccess::create_for_path(exe_path);
    bool self_contained = false;

    if (d->file_exists(exe_path + "/._sc_")) {
        self_contained = true;
        Error err = extra_config->load(exe_path + "/._sc_");
        if (err != OK) {
            ERR_PRINT("Can't load config from path '" + exe_path + "/._sc_'.");
        }
    } else if (d->file_exists(exe_path + "/_sc_")) {
        self_contained = true;
        Error err = extra_config->load(exe_path + "/_sc_");
        if (err != OK) {
            ERR_PRINT("Can't load config from path '" + exe_path + "/_sc_'.");
        }
    }
    memdelete(d);

    if (self_contained) {

        // editor is self contained, all in same folder
        data_path = exe_path;
        data_dir = PathUtils::plus_file(data_path,"editor_data");
        config_path = exe_path;
        config_dir = data_dir;
        cache_path = exe_path;
        cache_dir = PathUtils::plus_file(data_dir,"cache");
    } else {

        // Typically XDG_DATA_HOME or %APPDATA%
        data_path = os_ptr->get_data_path();
        data_dir = data_path;
        // Can be different from data_path e.g. on Linux or macOS
        config_path = os_ptr->get_config_path();
        config_dir = config_path;
        // Can be different from above paths, otherwise a subfolder of data_dir
        cache_path = os_ptr->get_cache_path();
        cache_dir = cache_path;
        if (cache_path == data_path) {
            cache_dir = PathUtils::plus_file(data_dir,"cache");
        }
        // Old code was making sure that cache was app specific, since transitioning to QStandardPaths, it should always be the case
    }

    ClassDB::register_class<EditorSettings>(); //otherwise it can't be unserialized

    String config_file_path;

    if (!data_path.empty() && !config_path.empty() && !cache_path.empty()) {

        // Validate/create data dir and subdirectories

        DirAccess *dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

        if (dir->change_dir(data_dir) != OK) {
            dir->make_dir_recursive(data_dir);
            if (dir->change_dir(data_dir) != OK) {
                ERR_PRINT("Cannot create data directory!");
                memdelete(dir);
                goto fail;
            }
        }

        if (dir->change_dir_utf8("templates") != OK) {
            dir->make_dir_utf8("templates");
        } else {
            dir->change_dir_utf8("..");
        }

        // Validate/create cache dir


        if (dir->change_dir(cache_dir) != OK) {
            dir->make_dir_recursive(cache_dir);
            if (dir->change_dir(cache_dir) != OK) {
                ERR_PRINT("Cannot create cache directory!");
                memdelete(dir);
                goto fail;
            }
        }

        // Validate/create config dir and subdirectories


        if (dir->change_dir(config_dir) != OK) {
            dir->make_dir_recursive(config_dir);
            if (dir->change_dir(config_dir) != OK) {
                ERR_PRINT("Cannot create config directory!");
                memdelete(dir);
                goto fail;
            }
        }

        if (dir->change_dir_utf8("text_editor_themes") != OK) {
            dir->make_dir_utf8("text_editor_themes");
        } else {
            dir->change_dir_utf8("..");
        }

        if (dir->change_dir_utf8("script_templates") != OK) {
            dir->make_dir_utf8("script_templates");
        } else {
            dir->change_dir_utf8("..");
        }
        if (dir->change_dir_utf8("feature_profiles") != OK) {
            dir->make_dir_utf8("feature_profiles");
        } else {
            dir->change_dir_utf8("..");
        }
        _create_script_templates(PathUtils::plus_file(dir->get_current_dir(),"script_templates"));

        if (dir->change_dir_utf8("projects") != OK) {
            dir->make_dir_utf8("projects");
        } else {
            dir->change_dir_utf8("..");
        }

        // Validate/create project-specific config dir

        dir->change_dir_utf8("projects");
        String project_config_dir = ProjectSettings::get_singleton()->get_resource_path();
        if (StringUtils::ends_with(project_config_dir,"/"))
            project_config_dir = StringUtils::substr(config_path,0, project_config_dir.size() - 1);
        project_config_dir = String(PathUtils::get_file(project_config_dir)) + "-" + StringUtils::md5_text(project_config_dir);

        if (dir->change_dir(project_config_dir) != OK) {
            dir->make_dir(project_config_dir);
        } else {
            dir->change_dir_utf8("..");
        }
        dir->change_dir_utf8("..");

        // Validate editor config file

        String config_file_name = "editor_settings-" + ::to_string(VERSION_MAJOR) + ".tres";
        config_file_path = PathUtils::plus_file(config_dir,config_file_name);
        if (!dir->file_exists(config_file_name)) {
            goto fail;
        }

        memdelete(dir);

        singleton = dynamic_ref_cast<EditorSettings>(ResourceLoader::load(config_file_path, "EditorSettings"));

        if (not singleton) {
            WARN_PRINT("Could not open config file.");
            goto fail;
        }

        singleton->save_changed_setting = true;
        singleton->config_file_path = config_file_path;
        singleton->project_config_dir = project_config_dir;
        singleton->settings_dir = config_dir;
        singleton->data_dir = data_dir;
        singleton->cache_dir = cache_dir;

        print_verbose("EditorSettings: Load OK!");

        singleton->setup_language();
        singleton->setup_network();
        singleton->load_favorites();
        singleton->list_text_editor_themes();

        return;
    }

fail:

    // patch init projects
    if (extra_config->has_section("init_projects")) {
        PoolVector<String> list = extra_config->get_value("init_projects", "list").as<PoolVector<String>>();
        auto w = list.write();
        for (int i = 0; i < list.size(); i++) {

            w[i] = PathUtils::plus_file(exe_path,list[i]);
        }
        extra_config->set_value("init_projects", "list", list);
    }

    singleton = make_ref_counted<EditorSettings>();
    singleton->save_changed_setting = true;
    singleton->config_file_path = config_file_path;
    singleton->settings_dir = config_dir;
    singleton->data_dir = data_dir;
    singleton->cache_dir = cache_dir;
    singleton->_load_defaults(extra_config);
    singleton->setup_language();
    singleton->setup_network();
    singleton->list_text_editor_themes();
}

void EditorSettings::setup_language() {

    UIString lang = get("interface/editor/editor_language");
    if (lang == "en")
        return; //none to do

    EditorTranslationList *etl = _editor_translations;

    while (etl->data) {

        if (etl->lang == lang) {

            Vector<uint8_t> data;
            data.resize(etl->uncomp_size);
            Compression::decompress(data.data(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);

            FileAccessMemory *fa = memnew(FileAccessMemory);
            fa->open_custom(data.data(), data.size());

            Ref<Translation> tr = dynamic_ref_cast<Translation>(TranslationLoaderPO::load_translation(fa, nullptr, "translation_" + String(etl->lang)));

            if (tr) {
                tr->set_locale(etl->lang);
                TranslationServer::get_singleton()->set_tool_translation(tr);
                break;
            }
        }

        etl++;
    }
}

void EditorSettings::setup_network() {

    Vector<IP_Address> local_ip;
    IP::get_singleton()->get_local_addresses(&local_ip);
    String hint;
    const StringName remotehost("network/debug/remote_host");
    String current = has_setting(remotehost) ? get(remotehost).as<String>() : "";
    String selected("127.0.0.1");

    // Check that current remote_host is a valid interface address and populate hints.
    for (const IP_Address &E : local_ip) {

        String ip(E);

        // link-local IPv6 addresses don't work, skipping them
        if (StringUtils::begins_with(ip, "fe80:0:0:0:")) // fe80::/64
            continue;
        // Same goes for IPv4 link-local (APIPA) addresses.
        if (StringUtils::begins_with(ip, "169.254.")) // 169.254.0.0/16
            continue;
        // Select current IP (found)
        if (ip == current)
            selected = ip;
        if (!hint.empty())
            hint += ',';
        hint += ip;
    }

    // Add hints with valid IP addresses to remote_host property.
    add_property_hint(PropertyInfo(VariantType::STRING, "network/debug/remote_host", PropertyHint::Enum, hint));

    // Fix potentially invalid remote_host due to network change.
    set("network/debug/remote_host", selected);
}

void EditorSettings::save() {

    //_THREAD_SAFE_METHOD_

    if (!singleton.get())
        return;

    if (singleton->config_file_path.empty()) {
        ERR_PRINT("Cannot save EditorSettings config, no valid path");
        return;
    }
    assert(singleton->reference_get_count()>=1);
    Error err = ResourceSaver::save(singleton->config_file_path, singleton);

    if (err != OK) {
        ERR_PRINT("Error saving editor settings to " + singleton->config_file_path);
    } else {
        print_verbose("EditorSettings: Save OK!");
    }
}

void EditorSettings::destroy() {

    if (!singleton.get())
        return;
    save();
    singleton = Ref<EditorSettings>();
}

void EditorSettings::set_optimize_save(bool p_optimize) {

    optimize_save = p_optimize;
}

// Properties

void EditorSettings::set_setting(const StringName &p_setting, const Variant &p_value) {
    _THREAD_SAFE_METHOD_
    set(p_setting, p_value);
}

Variant EditorSettings::get_setting(const StringName &p_setting) const {
    _THREAD_SAFE_METHOD_
    return get(p_setting);
}
bool EditorSettings::has_setting(const StringName &p_setting) const {

    _THREAD_SAFE_METHOD_

    return props.contains(p_setting);
}

void EditorSettings::erase(const StringName &p_setting) {

    _THREAD_SAFE_METHOD_

    props.erase(p_setting);
}

void EditorSettings::raise_order(const StringName &p_setting) {
    _THREAD_SAFE_METHOD_

    ERR_FAIL_COND(!props.contains(p_setting));
    props[p_setting].order = ++last_order;
}

void EditorSettings::set_restart_if_changed(const StringName &p_setting, bool p_restart) {
    _THREAD_SAFE_METHOD_

    if (!props.contains(p_setting))
        return;
    props[p_setting].restart_if_changed = p_restart;
}

void EditorSettings::set_initial_value(const StringName &p_setting, const Variant &p_value, bool p_update_current) {

    _THREAD_SAFE_METHOD_

    if (!props.contains(p_setting))
        return;
    props[p_setting].initial = p_value;
    props[p_setting].has_default_value = true;
    if (p_update_current) {
        set(p_setting, p_value);
    }
}

Variant _EDITOR_DEF(const StringName &p_setting, const Variant &p_default, bool p_restart_if_changed) {

    Variant ret = p_default;
    if (EditorSettings::get_singleton()->has_setting(p_setting)) {
        ret = EditorSettings::get_singleton()->get(p_setting);
    } else {
        EditorSettings::get_singleton()->set_manually(p_setting, p_default);
        EditorSettings::get_singleton()->set_restart_if_changed(p_setting, p_restart_if_changed);
    }

    if (!EditorSettings::get_singleton()->has_default_value(p_setting)) {
        EditorSettings::get_singleton()->set_initial_value(p_setting, p_default);
    }

    return ret;
}

Variant _EDITOR_GET(const StringName &p_setting) {

    ERR_FAIL_COND_V(!EditorSettings::get_singleton()->has_setting(p_setting), Variant());
    return EditorSettings::get_singleton()->get(p_setting);
}

bool EditorSettings::property_can_revert(const StringName &p_setting) {

    if (!props.contains(p_setting))
        return false;

    if (!props[p_setting].has_default_value)
        return false;

    return props[p_setting].initial != props[p_setting].variant;
}

Variant EditorSettings::property_get_revert(const StringName &p_setting) {

    if (!props.contains(p_setting) || !props[p_setting].has_default_value)
        return Variant();

    return props[p_setting].initial;
}

void EditorSettings::add_property_hint(const PropertyInfo &p_hint) {

    _THREAD_SAFE_METHOD_

    hints[p_hint.name] = p_hint;
}

// Data directories

const String &EditorSettings::get_data_dir() const {

    return data_dir;
}

String EditorSettings::get_templates_dir() const {

    return PathUtils::plus_file(get_data_dir(),"templates");
}

// Config directories

const String &EditorSettings::get_settings_dir() const {

    return settings_dir;
}

String EditorSettings::get_project_settings_dir() const {

    return PathUtils::plus_file(PathUtils::plus_file(get_settings_dir(),"projects"),project_config_dir);
}

String EditorSettings::get_text_editor_themes_dir() const {

    return PathUtils::plus_file(get_settings_dir(),"text_editor_themes");
}

String EditorSettings::get_script_templates_dir() const {

    return PathUtils::plus_file(get_settings_dir(),"script_templates");
}

String EditorSettings::get_project_script_templates_dir() const {

    return ProjectSettings::get_singleton()->get("editor/script_templates_search_path");
}

// Cache directory

const String &EditorSettings::get_cache_dir() const {

    return cache_dir;
}

String EditorSettings::get_feature_profiles_dir() const {

    return PathUtils::plus_file(get_settings_dir(),"feature_profiles");
}

// Metadata

void EditorSettings::set_project_metadata(se_string_view p_section, se_string_view p_key, const Variant& p_data) {
    Ref<ConfigFile> cf(make_ref_counted<ConfigFile>());
    String path = PathUtils::plus_file(get_project_settings_dir(),"project_metadata.cfg");
    Error err = cf->load(path);

    ERR_FAIL_COND_MSG(err != OK && err != ERR_FILE_NOT_FOUND, "Cannot load editor settings from file '" + path + "'.");
    cf->set_value(p_section, p_key, p_data);
    err = cf->save(path);
    ERR_FAIL_COND_MSG(err != OK, "Cannot save editor settings to file '" + path + "'.");
}

Variant EditorSettings::get_project_metadata(se_string_view p_section, se_string_view p_key, const Variant& p_default) const {
    Ref<ConfigFile> cf(make_ref_counted<ConfigFile>());
    String path = PathUtils::plus_file(get_project_settings_dir(),"project_metadata.cfg");
    Error err = cf->load(path);
    if (err != OK) {
        return p_default;
    }
    return cf->get_value(p_section, p_key, p_default);
}

void EditorSettings::set_favorites(const Vector<String> &p_favorites) {

    favorites = p_favorites;
    FileAccess *f = FileAccess::open(PathUtils::plus_file(get_project_settings_dir(),"favorites"), FileAccess::WRITE);
    if (f) {
        for (int i = 0; i < favorites.size(); i++)
            f->store_line(favorites[i]);
        memdelete(f);
    }
}

const Vector<String> &EditorSettings::get_favorites() const {

    return favorites;
}

void EditorSettings::set_recent_dirs(const Vector<String> &p_recent_dirs) {

    recent_dirs = p_recent_dirs;
    FileAccess *f = FileAccess::open(PathUtils::plus_file(get_project_settings_dir(),"recent_dirs"), FileAccess::WRITE);
    if (f) {
        for (int i = 0; i < recent_dirs.size(); i++)
            f->store_line(recent_dirs[i]);
        memdelete(f);
    }
}

const Vector<String> &EditorSettings::get_recent_dirs() const {

    return recent_dirs;
}

void EditorSettings::load_favorites() {

    FileAccess *f = FileAccess::open(PathUtils::plus_file(get_project_settings_dir(),"favorites"), FileAccess::READ);
    if (f) {
        String line(StringUtils::strip_edges(f->get_line()));
        while (!line.empty()) {
            favorites.push_back(line);
            line = StringUtils::strip_edges(f->get_line());
        }
        memdelete(f);
    }

    f = FileAccess::open(PathUtils::plus_file(get_project_settings_dir(),"recent_dirs"), FileAccess::READ);
    if (f) {
        String line(StringUtils::strip_edges(f->get_line()));
        while (!line.empty()) {
            recent_dirs.push_back(line);
            line = StringUtils::strip_edges(f->get_line());
        }
        memdelete(f);
    }
}

bool EditorSettings::is_dark_theme() {
    int AUTO_COLOR = 0;
    int LIGHT_COLOR = 2;
    Color base_color = get("interface/theme/base_color");
    int icon_font_color_setting = get("interface/theme/icon_and_font_color");
    return (icon_font_color_setting == AUTO_COLOR && (base_color.r + base_color.g + base_color.b) / 3.0f < 0.5f) ||
           icon_font_color_setting == LIGHT_COLOR;
}

void EditorSettings::list_text_editor_themes() {
    String themes("Adaptive,Default,Custom");
    DirAccess *d = DirAccess::open(get_text_editor_themes_dir());
    if (d) {
        List<String> custom_themes;
        d->list_dir_begin();
        String file = d->get_next();
        while (!file.empty()) {
            if (PathUtils::get_extension(file) == se_string_view("tet") &&
                    !_is_default_text_editor_theme(StringUtils::to_lower(PathUtils::get_basename(file)))) {
                custom_themes.emplace_back(PathUtils::get_basename(file));
            }
            file = d->get_next();
        }
        d->list_dir_end();
        memdelete(d);
        custom_themes.sort();
        for (const auto & E : custom_themes) {
            themes += "," + E;
        }
    }
    add_property_hint(PropertyInfo(VariantType::STRING, "text_editor/theme/color_theme", PropertyHint::Enum, themes));
}

void EditorSettings::load_text_editor_theme() {
    String p_file = get("text_editor/theme/color_theme");

    if (_is_default_text_editor_theme(StringUtils::to_lower(PathUtils::get_file(p_file)))) {
        if (p_file == "Default") {
            _load_default_text_editor_theme();
        }
        return; // sorry for "Settings changed" console spam
    }

    String theme_path = PathUtils::plus_file(get_text_editor_themes_dir(),p_file + ".tet");

    Ref<ConfigFile> cf(make_ref_counted<ConfigFile>());
    Error err = cf->load(theme_path);

    if (err != OK) {
        return;
    }

    Vector<String> keys = cf->get_section_keys("color_theme");

    for (const String & key : keys) {
        String val = cf->get_value("color_theme", key);

        // don't load if it's not already there!
        if (has_setting(StringName("text_editor/highlighting/" + key))) {

            // make sure it is actually a color
            if (StringUtils::is_valid_html_color(val) && StringUtils::contains(key,"color")) {
                props[StringName("text_editor/highlighting/" + key)].variant = Color::html(val); // change manually to prevent "Settings changed" console spam
            }
        }
    }
    emit_signal("settings_changed");
    // if it doesn't load just use what is currently loaded
}

bool EditorSettings::import_text_editor_theme(se_string_view p_file) {

    if (!StringUtils::ends_with(p_file,".tet")) {
        return false;
    } else {
        if (StringUtils::to_lower(PathUtils::get_file(p_file)) == "default.tet") {
            return false;
        }

        DirAccess *d = DirAccess::open(get_text_editor_themes_dir());
        if (d) {
            d->copy(p_file, PathUtils::plus_file(get_text_editor_themes_dir(),PathUtils::get_file(p_file)));
            memdelete(d);
            return true;
        }
    }
    return false;
}

bool EditorSettings::save_text_editor_theme() {

    String p_file = get("text_editor/theme/color_theme");

    if (_is_default_text_editor_theme(StringUtils::to_lower(PathUtils::get_file(p_file)))) {
        return false;
    }
    String theme_path =PathUtils::plus_file( get_text_editor_themes_dir(),p_file + ".tet");
    return _save_text_editor_theme(theme_path);
}

bool EditorSettings::save_text_editor_theme_as(se_string_view _file) {
    String p_file(_file);
    if (!StringUtils::ends_with(p_file,".tet")) {
        p_file += ".tet";
    }

    if (_is_default_text_editor_theme(StringUtils::trim_suffix(StringUtils::to_lower(PathUtils::get_file(p_file)),".tet"))) {
        return false;
    }
    if (_save_text_editor_theme(p_file)) {

        // switch to theme is saved in the theme directory
        list_text_editor_themes();
        se_string_view theme_name = PathUtils::get_file(StringUtils::substr(p_file,0, p_file.length() - 4));

        if (PathUtils::get_base_dir(p_file) == get_text_editor_themes_dir()) {
            _initial_set("text_editor/theme/color_theme", theme_name);
            load_text_editor_theme();
        }
        return true;
    }
    return false;
}

bool EditorSettings::is_default_text_editor_theme() {
    String p_file = get("text_editor/theme/color_theme");
    return _is_default_text_editor_theme(StringUtils::to_lower(PathUtils::get_file(p_file)));
}
Vector<String> EditorSettings::get_script_templates(se_string_view p_extension, se_string_view p_custom_path) {

    Vector<String> templates;
    String template_dir = get_script_templates_dir();
    if (!p_custom_path.empty()) {
        template_dir = p_custom_path;
    }
    DirAccess *d = DirAccess::open(template_dir);
    if (d) {
        d->list_dir_begin();
        String file = d->get_next();
        while (!file.empty()) {
            if (PathUtils::get_extension(file) == p_extension) {
                templates.emplace_back(PathUtils::get_basename(file));
            }
            file = d->get_next();
        }
        d->list_dir_end();
        memdelete(d);
    }
    return templates;
}

String EditorSettings::get_editor_layouts_config() const {

    return PathUtils::plus_file(get_settings_dir(),"editor_layouts.cfg");
}

// Shortcuts

void EditorSettings::add_shortcut(se_string_view p_name, Ref<ShortCut> &p_shortcut) {

    shortcuts[String(p_name)] = p_shortcut;
}

bool EditorSettings::is_shortcut(se_string_view p_name, const Ref<InputEvent> &p_event) const {

    const Map<String, Ref<ShortCut> >::const_iterator E = shortcuts.find_as(p_name);
    ERR_FAIL_COND_V_MSG(E==shortcuts.end(), false, "Unknown Shortcut: " + String(p_name) + ".");

    return E->second->is_shortcut(p_event);
}

Ref<ShortCut> EditorSettings::get_shortcut(se_string_view p_name) const {

    const Map<String, Ref<ShortCut> >::const_iterator E = shortcuts.find_as(p_name);
    if (E==shortcuts.end())
        return Ref<ShortCut>();

    return E->second;
}

void EditorSettings::get_shortcut_list(Vector<String> *r_shortcuts) {

    for (const eastl::pair<const String,Ref<ShortCut> > &E : shortcuts) {

        r_shortcuts->push_back(E.first);
    }
}

Ref<ShortCut> ED_GET_SHORTCUT(se_string_view p_path) {

    if (!EditorSettings::get_singleton()) {
        return Ref<ShortCut>();
    }

    Ref<ShortCut> sc = EditorSettings::get_singleton()->get_shortcut(p_path);

    ERR_FAIL_COND_V_MSG(not sc, sc, "Used ED_GET_SHORTCUT with invalid shortcut: " + String(p_path) + ".");
    return sc;
}

struct ShortCutMapping {
    const char *path;
    uint32_t keycode;
};
Ref<ShortCut> ED_SHORTCUT(se_string_view p_path, const StringName &p_name, uint32_t p_keycode) {

#ifdef OSX_ENABLED
    // Use Cmd+Backspace as a general replacement for Delete shortcuts on macOS

    if (p_keycode == KEY_DELETE) {
        p_keycode = KEY_MASK_CMD | KEY_BACKSPACE;
    }
#endif

    Ref<InputEventKey> ie;
    auto name(p_name);
    if (p_keycode) {
        ie = make_ref_counted<InputEventKey>();

        ie->set_unicode(p_keycode & KEY_CODE_MASK);
        ie->set_scancode(p_keycode & KEY_CODE_MASK);
        ie->set_shift(bool(p_keycode & KEY_MASK_SHIFT));
        ie->set_alt(bool(p_keycode & KEY_MASK_ALT));
        ie->set_control(bool(p_keycode & KEY_MASK_CTRL));
        ie->set_metakey(bool(p_keycode & KEY_MASK_META));
    }

    if (!EditorSettings::get_singleton()) {
        Ref<ShortCut> sc(make_ref_counted<ShortCut>());
        sc->set_name(name);
        sc->set_shortcut(ie);
        sc->set_meta("original", ie);
        return sc;
    }
    Ref<ShortCut> sc = EditorSettings::get_singleton()->get_shortcut(p_path);
    if (sc) {

        sc->set_name(name); //keep name (the ones that come from disk have no name)
        sc->set_meta("original", ie); //to compare against changes
        return sc;
    }

    sc = make_ref_counted<ShortCut>();
    sc->set_name(name);
    sc->set_shortcut(ie);
    sc->set_meta("original", ie); //to compare against changes
    EditorSettings::get_singleton()->add_shortcut(p_path, sc);

    return sc;
}

void EditorSettings::notify_changes() {

    _THREAD_SAFE_METHOD_

    SceneTree *sml = object_cast<SceneTree>(OS::get_singleton()->get_main_loop());

    if (!sml) {
        return;
    }

    Node *root = sml->get_root()->get_child(0);

    if (!root) {
        return;
    }
    root->propagate_notification(NOTIFICATION_EDITOR_SETTINGS_CHANGED);
}

void EditorSettings::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("has_setting", {"name"}), &EditorSettings::has_setting);
    MethodBinder::bind_method(D_METHOD("set_setting", {"name", "value"}), &EditorSettings::set_setting);
    MethodBinder::bind_method(D_METHOD("get_setting", {"name"}), &EditorSettings::get_setting);
    MethodBinder::bind_method(D_METHOD("erase", {"property"}), &EditorSettings::erase);
    MethodBinder::bind_method(D_METHOD("set_initial_value", {"name", "value", "update_current"}), &EditorSettings::set_initial_value);
    MethodBinder::bind_method(D_METHOD("property_can_revert", {"name"}), &EditorSettings::property_can_revert);
    MethodBinder::bind_method(D_METHOD("property_get_revert", {"name"}), &EditorSettings::property_get_revert);
    MethodBinder::bind_method(D_METHOD("add_property_info", {"info"}), &EditorSettings::_add_property_info_bind);

    MethodBinder::bind_method(D_METHOD("get_settings_dir"), &EditorSettings::get_settings_dir);
    MethodBinder::bind_method(D_METHOD("get_project_settings_dir"), &EditorSettings::get_project_settings_dir);

    MethodBinder::bind_method(D_METHOD("set_project_metadata", {"section", "key", "data"}), &EditorSettings::set_project_metadata);
    MethodBinder::bind_method(D_METHOD("get_project_metadata", {"section", "key", "default"}), &EditorSettings::get_project_metadata, {DEFVAL(Variant())});

    MethodBinder::bind_method(D_METHOD("set_favorites", {"dirs"}), &EditorSettings::set_favorites);
    MethodBinder::bind_method(D_METHOD("get_favorites"), &EditorSettings::get_favorites);
    MethodBinder::bind_method(D_METHOD("set_recent_dirs", {"dirs"}), &EditorSettings::set_recent_dirs);
    MethodBinder::bind_method(D_METHOD("get_recent_dirs"), &EditorSettings::get_recent_dirs);

    ADD_SIGNAL(MethodInfo("settings_changed"));
    BIND_CONSTANT(NOTIFICATION_EDITOR_SETTINGS_CHANGED)
}

EditorSettings::EditorSettings() {
    __thread__safe__.reset(new Mutex);
    last_order = 0;
    optimize_save = true;
    save_changed_setting = true;

    _load_defaults();
}

EditorSettings::~EditorSettings() = default;
