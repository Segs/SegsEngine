/*************************************************************************/
/*  project_export.cpp                                                   */
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

#include "project_export.h"

#include "editor_data.h"
#include "editor_node.h"
#include "editor_settings.h"

#include "core/callable_method_pointer.h"
#include "core/compressed_translation.h"
#include "core/io/image_loader.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/method_bind.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/string_formatter.h"
#include "core/version.h"
#include "editor/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/item_list.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/tab_container.h"
#include "scene/resources/style_box.h"

IMPL_GDCLASS(ProjectExportDialog)

void ProjectExportDialog::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE:
        case NOTIFICATION_THEME_CHANGED: {
            duplicate_preset->set_button_icon(get_theme_icon("Duplicate", "EditorIcons"));
            delete_preset->set_button_icon(get_theme_icon("Remove", "EditorIcons"));
        } break;
        case NOTIFICATION_READY: {
            connect("confirmed",callable_mp(this, &ClassName::_export_pck_zip));
            _update_export_all();
        } break;
        case NOTIFICATION_POPUP_HIDE: {
            EditorSettings::get_singleton()->set_project_metadata("dialog_bounds", "export", get_rect());
        } break;
        case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
            parameters->set_property_name_style(EditorPropertyNameProcessor::get_settings_style());
        } break;
    }
}

void ProjectExportDialog::popup_export() {

    add_preset->get_popup()->clear();
    for (int i = 0; i < EditorExport::get_singleton()->get_export_platform_count(); i++) {

        Ref<EditorExportPlatform> plat = EditorExport::get_singleton()->get_export_platform(i);

        add_preset->get_popup()->add_icon_item(plat->get_logo(), StringName(plat->get_name()));
    }

    _update_presets();
    if (presets->get_current() >= 0) {
        _update_current_preset(); // triggers rescan for templates if newly installed
    }

    // Restore valid window bounds or pop up at default size.
    Rect2 saved_size = EditorSettings::get_singleton()->get_project_metadataT("dialog_bounds", "export", Rect2());
    if (saved_size != Rect2()) {
        popup(saved_size);
    } else {
        popup_centered_clamped(Size2(900, 700) * EDSCALE, 0.8f);
    }
}

void ProjectExportDialog::_add_preset(int p_platform) {

    Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_platform(p_platform)->create_preset();
    ERR_FAIL_COND(not preset);

    String name(EditorExport::get_singleton()->get_export_platform(p_platform)->get_name());
    bool make_runnable = true;
    int attempt = 1;
    while (true) {

        bool valid = true;

        for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
            Ref<EditorExportPreset> p = EditorExport::get_singleton()->get_export_preset(i);
            if (p->get_platform() == preset->get_platform() && p->is_runnable()) {
                make_runnable = false;
            }
            if (p->get_name() == name) {
                valid = false;
                break;
            }
        }

        if (valid)
            break;

        attempt++;
        name = EditorExport::get_singleton()->get_export_platform(p_platform)->get_name() + " " + itos(attempt);
    }

    preset->set_name(name);
    if (make_runnable)
        preset->set_runnable(make_runnable);
    EditorExport::get_singleton()->add_export_preset(preset);
    _update_presets();
    _edit_preset(EditorExport::get_singleton()->get_export_preset_count() - 1);
}

void ProjectExportDialog::_force_update_current_preset_parameters() {
    // Force the parameters section to refresh its UI.
    parameters->edit(nullptr);
    _update_current_preset();
}

void ProjectExportDialog::_update_current_preset() {

    _edit_preset(presets->get_current());
}

void ProjectExportDialog::_update_presets() {

    updating = true;

    Ref<EditorExportPreset> current;
    if (presets->get_current() >= 0 && presets->get_current() < presets->get_item_count())
        current = get_current_preset();

    int current_idx = -1;
    presets->clear();
    for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
        Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(i);
        if (preset == current) {
            current_idx = i;
        }

        String name = preset->get_name();
        if (preset->is_runnable())
            name += " (" + (TTR("Runnable")) + ")";
        presets->add_item(StringName(name), preset->get_platform()->get_logo());
    }

    if (current_idx != -1) {
        presets->select(current_idx);
    }

    updating = false;
}

void ProjectExportDialog::_update_export_all() {

    bool can_export = EditorExport::get_singleton()->get_export_preset_count() > 0;

    for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
        Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(i);
        bool needs_templates;
        String error;
        if (preset->get_export_path().empty() || !preset->get_platform()->can_export(preset, error, needs_templates)) {
            can_export = false;
            break;
        }
    }

    export_all_button->set_disabled(!can_export);

    if (can_export) {
        export_all_button->set_tooltip(TTR("Export the project for all the presets defined."));
    }
    else {
        export_all_button->set_tooltip(TTR("All presets must have an export path defined for Export All to work."));
    }
}

void ProjectExportDialog::_edit_preset(int p_index) {

    if (p_index < 0 || p_index >= presets->get_item_count()) {
        name->set_text("");
        name->set_editable(false);
        export_path->hide();
        runnable->set_disabled(true);
        parameters->edit(nullptr);
        presets->unselect_all();
        duplicate_preset->set_disabled(true);
        delete_preset->set_disabled(true);
        sections->hide();
        patches->clear();
        export_error->hide();
        export_templates_error->hide();
        return;
    }

    Ref<EditorExportPreset> current = EditorExport::get_singleton()->get_export_preset(p_index);
    ERR_FAIL_COND(not current);

    updating = true;

    presets->select(p_index);
    sections->show();

    name->set_editable(true);
    export_path->show();
    duplicate_preset->set_disabled(false);
    delete_preset->set_disabled(false);
    get_ok()->set_disabled(false);
    name->set_text(current->get_name());

    Vector<String> extension_list = current->get_platform()->get_binary_extensions(current);
    Vector<StringView> extension_vector;
    for (int i = 0; i < extension_list.size(); i++) {
        extension_vector.push_back("*." + extension_list[i]);
    }

    export_path->setup(extension_vector, false, true);
    export_path->update_property();
    runnable->set_disabled(false);
    runnable->set_pressed(current->is_runnable());
    parameters->edit(current.get());

    export_filter->select(current->get_export_filter());
    include_filters->set_text(current->get_include_filter());
    exclude_filters->set_text(current->get_exclude_filter());

    patches->clear();
    TreeItem *patch_root = patches->create_item();
    const Vector<String> &patchlist = current->get_patches();
    for (int i = 0; i < patchlist.size(); i++) {
        TreeItem *patch = patches->create_item(patch_root);
        patch->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
        String file(PathUtils::get_file(patchlist[i]));
        patch->set_editable(0, true);
        patch->set_text_utf8(0, StringUtils::replace(PathUtils::get_file(file),"*", ""));
        if (StringUtils::ends_with(file,'*'))
            patch->set_checked(0, true);
        patch->set_tooltip(0, StringName(patchlist[i]));
        patch->set_metadata(0, i);
        patch->add_button(0, get_theme_icon("Remove", "EditorIcons"), 0);
        patch->add_button(0, get_theme_icon("folder", "FileDialog"), 1);
    }

    TreeItem *patch_add = patches->create_item(patch_root);
    patch_add->set_metadata(0, patchlist.size());

    if (patchlist.empty())
        patch_add->set_text(0, TTR("Add initial export..."));
    else
        patch_add->set_text(0, TTR("Add previous patches..."));

    patch_add->add_button(0, get_theme_icon("folder", "FileDialog"), 1);

    _fill_resource_tree();

    bool needs_templates;
    String error;
    if (!current->get_platform()->can_export(current, error, needs_templates)) {

        if (!error.empty()) {

            Vector<StringView> items = StringUtils::split(error,'\n', false);
            error = "";
            for (size_t i = 0; i < items.size(); i++) {
                if (i > 0)
                    error += '\n';
                error += " - " + items[i];
            }

            export_error->set_text(StringName(error));
            export_error->show();
        } else {
            export_error->hide();
        }
        export_warning->hide();
        if (needs_templates)
            export_templates_error->show();
        else
            export_templates_error->hide();

        export_button->set_disabled(true);
    } else {
        if (!error.empty()) {
            FixedVector<StringView,16,true> items;
            String::split_ref(items,error,'\n',false);
            error = "";
            for (int i = 0; i < items.size(); i++) {
                if (i > 0) {
                    error += "\n";
                }
                error += " - " + items[i];
            }
            export_warning->set_text(error);
            export_warning->show();
        } else {
            export_warning->hide();
        }
        export_error->hide();
        export_templates_error->hide();
        export_button->set_disabled(false);
    }

    custom_features->set_text(current->get_custom_features());
    _update_feature_list();
    _update_export_all();
    minimum_size_changed();

    int script_export_mode = current->get_script_export_mode();
    script_mode->select(script_export_mode);

    String key = current->get_script_encryption_key();
    if (!updating_script_key) {
        script_key->set_text(key);
    }
    if (script_export_mode == EditorExportPreset::MODE_SCRIPT_ENCRYPTED) {
        script_key->set_editable(true);

        bool key_valid = _validate_script_encryption_key(key);
        if (key_valid) {
            script_key_error->hide();
        } else {
            script_key_error->show();
        }
    } else {
        script_key->set_editable(false);
        script_key_error->hide();
    }

    updating = false;
}

void ProjectExportDialog::_update_feature_list() {

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    Set<String> fset;
    Vector<String> features;

    current->get_platform()->get_platform_features(&features);
    current->get_platform()->get_preset_features(current, &features);

    String custom = current->get_custom_features();
    Vector<StringView> custom_list = StringUtils::split(custom,',');
    for (size_t i = 0; i < custom_list.size(); i++) {
        StringView f =StringUtils::strip_edges( custom_list[i]);
        if (!f.empty()) {
            features.push_back(String(f));
        }
    }
    fset.insert(features.begin(), features.end());

    custom_feature_display->clear();
    for (Set<String>::iterator E = fset.begin(); E!=fset.end(); ) {
        String f = *E;
        if (++E != fset.end()) {
            f += (", ");
        }
        custom_feature_display->add_text(f);
    }
}

void ProjectExportDialog::_custom_features_changed(StringView p_text) {

    if (updating)
        return;

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    current->set_custom_features(p_text);
    _update_feature_list();
}

void ProjectExportDialog::_tab_changed(int) {
    _update_feature_list();
}

void ProjectExportDialog::_patch_button_pressed(Object *p_item, int p_column, int p_id) {

    TreeItem *ti = (TreeItem *)p_item;

    patch_index = ti->get_metadata(0).as<int>();;

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    if (p_id == 0) {
        const Vector<String> &patches = current->get_patches();
        ERR_FAIL_INDEX(patch_index, patches.size());
        StringView file_name(PathUtils::get_file(patches[patch_index]));
        patch_erase->set_text(FormatSN(TTR("Delete patch '%.*s' from list?").asCString(), file_name.length(),file_name.data() ));
        patch_erase->popup_centered_minsize();
    } else {
        patch_dialog->popup_centered_ratio();
    }
}

void ProjectExportDialog::_patch_edited() {

    TreeItem *item = patches->get_edited();
    if (!item)
        return;
    int index = item->get_metadata(0).as<int>();

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    const Vector<String> &patches = current->get_patches();

    ERR_FAIL_INDEX(index, patches.size());

    String patch = patches[index];
    patch.replace("*", "");

    if (item->is_checked(0)) {
        patch += '*';
    }

    current->set_patch(index, patch);
}

void ProjectExportDialog::_patch_selected(StringView p_path) {

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    const Vector<String> &patches = current->get_patches();

    if (patch_index >= patches.size()) {

        current->add_patch(PathUtils::path_to(ProjectSettings::get_singleton()->get_resource_path(),p_path) + "*");
    } else {
        String enabled =StringUtils::ends_with( patches[patch_index],"*") ? String("*") : String();
        current->set_patch(patch_index, PathUtils::path_to(ProjectSettings::get_singleton()->get_resource_path(),p_path) + enabled);
    }

    _update_current_preset();
}

void ProjectExportDialog::_patch_deleted() {

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    const Vector<String> &patches = current->get_patches();
    if (patch_index < patches.size()) {

        current->remove_patch(patch_index);
        _update_current_preset();
    }
}

void ProjectExportDialog::_update_parameters(StringView p_edited_property) {

    _update_current_preset();
}

void ProjectExportDialog::_runnable_pressed() {

    if (updating)
        return;

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    if (runnable->is_pressed()) {

        for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
            Ref<EditorExportPreset> p = EditorExport::get_singleton()->get_export_preset(i);
            if (p->get_platform() == current->get_platform()) {
                p->set_runnable(current == p);
            }
        }
    } else {

        current->set_runnable(false);
    }

    _update_presets();
}

void ProjectExportDialog::_name_changed(StringView p_string) {

    if (updating)
        return;

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    current->set_name(p_string);
    _update_presets();
}

void ProjectExportDialog::set_export_path(StringView p_value) {
    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    current->set_export_path(p_value);
}

String ProjectExportDialog::get_export_path() {
    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND_V(not current, String(""));

    return current->get_export_path();
}

Ref<EditorExportPreset> ProjectExportDialog::get_current_preset() const {

    return EditorExport::get_singleton()->get_export_preset(presets->get_current());
}

void ProjectExportDialog::_export_path_changed(const StringName &p_property, const Variant &p_value, StringView p_field, bool p_changing) {

    if (updating)
        return;

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    current->set_export_path(p_value.as<String>());
    _update_presets();
    _update_export_all();

}

void ProjectExportDialog::_open_key_help_link() {
    OS::get_singleton()->shell_open(FormatVE("%s/development/compiling/compiling_with_script_encryption_key.html", VERSION_DOCS_URL));
}
void ProjectExportDialog::_script_export_mode_changed(int p_mode) {

    if (updating)
        return;

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    current->set_script_export_mode(p_mode);

    _update_current_preset();
}

void ProjectExportDialog::_script_encryption_key_changed(const String &p_key) {

    if (updating)
        return;

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);

    current->set_script_encryption_key(p_key);

    updating_script_key = true;
    _update_current_preset();
    updating_script_key = false;
}

bool ProjectExportDialog::_validate_script_encryption_key(StringView p_key) {

    bool is_valid = false;

    if (!p_key.empty() && StringUtils::is_valid_hex_number(p_key,false) && p_key.length() == 64) {
        is_valid = true;
    }
    return is_valid;
}

void ProjectExportDialog::_duplicate_preset() {

    Ref<EditorExportPreset> current = get_current_preset();
    if (not current)
        return;

    Ref<EditorExportPreset> preset = current->get_platform()->create_preset();
    ERR_FAIL_COND(not preset);

    String name = current->get_name() + " (copy)";
    bool make_runnable = true;
    while (true) {

        bool valid = true;

        for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
            Ref<EditorExportPreset> p = EditorExport::get_singleton()->get_export_preset(i);
            if (p->get_platform() == preset->get_platform() && p->is_runnable()) {
                make_runnable = false;
            }
            if (p->get_name() == name) {
                valid = false;
                break;
            }
        }

        if (valid)
            break;

        name += (" (copy)");
    }

    preset->set_name(name);
    if (make_runnable)
        preset->set_runnable(make_runnable);
    preset->set_export_filter(current->get_export_filter());
    preset->set_include_filter(current->get_include_filter());
    preset->set_exclude_filter(current->get_exclude_filter());
    const Vector<String> &list = current->get_patches();
    for (int i = 0; i < list.size(); i++) {
        preset->add_patch(list[i]);
    }
    preset->set_custom_features(current->get_custom_features());

    for (const PropertyInfo &E : current->get_properties()) {
        preset->set(E.name, current->get(E.name));
    }

    EditorExport::get_singleton()->add_export_preset(preset);
    _update_presets();
    _edit_preset(EditorExport::get_singleton()->get_export_preset_count() - 1);
}

void ProjectExportDialog::_delete_preset() {

    Ref<EditorExportPreset> current = get_current_preset();
    if (not current)
        return;

    delete_confirm->set_text(FormatSN(TTR("Delete preset '%s'?").asCString(), current->get_name().c_str()));
    delete_confirm->popup_centered_minsize();
}

void ProjectExportDialog::_delete_preset_confirm() {

    int idx = presets->get_current();
    _edit_preset(-1);
    export_button->set_disabled(true);
    get_ok()->set_disabled(true);
    EditorExport::get_singleton()->remove_export_preset(idx);
    _update_presets();
    // The Export All button might become enabled (if all other presets have an export path defined),
    // or it could be disabled (if there are no presets anymore).
    _update_export_all();
}

Variant ProjectExportDialog::get_drag_data_fw(const Point2 &p_point, Control *p_from) {

    if (p_from == presets) {
        int pos = presets->get_item_at_position(p_point, true);

        if (pos >= 0) {
            Dictionary d;
            d["type"] = "export_preset";
            d["preset"] = pos;

            HBoxContainer *drag = memnew(HBoxContainer);
            TextureRect *tr = memnew(TextureRect);
            tr->set_texture(presets->get_item_icon(pos));
            drag->add_child(tr);
            Label *label = memnew(Label);
            label->set_text(presets->get_item_text(pos));
            drag->add_child(label);

            set_drag_preview(drag);

            return d;
        }
    } else if (p_from == patches) {

        TreeItem *item = patches->get_item_at_position(p_point);

        if (item && item->get_cell_mode(0) == TreeItem::CELL_MODE_CHECK) {

            int metadata = item->get_metadata(0).as<int>();;
            Dictionary d;
            d["type"] = "export_patch";
            d["patch"] = metadata;

            Label *label = memnew(Label);
            label->set_text(StringName(item->get_text(0)));
            set_drag_preview(label);

            return d;
        }
    }

    return Variant();
}

bool ProjectExportDialog::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {

    if (p_from == presets) {
        Dictionary d = p_data.as<Dictionary>();
        if (!d.has("type") || d["type"].as<String>() != "export_preset")
            return false;

        if (presets->get_item_at_position(p_point, true) < 0 && !presets->is_pos_at_end_of_items(p_point))
            return false;
    } else if (p_from == patches) {

        Dictionary d = p_data.as<Dictionary>();
        if (!d.has("type") || d["type"].as<String>() != "export_patch")
            return false;

        patches->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);

        TreeItem *item = patches->get_item_at_position(p_point);

        if (!item) {

            return false;
        }
    }

    return true;
}

void ProjectExportDialog::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {

    if (p_from == presets) {
        Dictionary d = p_data.as<Dictionary>();
        int from_pos = d["preset"].as<int>();

        int to_pos = -1;

        if (presets->get_item_at_position(p_point, true) >= 0) {
            to_pos = presets->get_item_at_position(p_point, true);
        }

        if (to_pos == -1 && !presets->is_pos_at_end_of_items(p_point))
            return;

        if (to_pos == from_pos)
            return;
        else if (to_pos > from_pos) {
            to_pos--;
        }

        Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(from_pos);
        EditorExport::get_singleton()->remove_export_preset(from_pos);
        EditorExport::get_singleton()->add_export_preset(preset, to_pos);

        _update_presets();
        if (to_pos >= 0)
            _edit_preset(to_pos);
        else
            _edit_preset(presets->get_item_count() - 1);
    } else if (p_from == patches) {

        Dictionary d = p_data.as<Dictionary>();
        if (!d.has("type") || d["type"].as<String>() != "export_patch")
            return;

        int from_pos = d["patch"].as<int>();

        TreeItem *item = patches->get_item_at_position(p_point);
        if (!item)
            return;

        int to_pos = item->get_cell_mode(0) == TreeItem::CELL_MODE_CHECK ? item->get_metadata(0).as<int>() : -1;

        if (to_pos == from_pos)
            return;
        else if (to_pos > from_pos) {
            to_pos--;
        }

        Ref<EditorExportPreset> preset = get_current_preset();
        String patch = preset->get_patch(from_pos);
        preset->remove_patch(from_pos);
        preset->add_patch(patch, to_pos);

        _update_current_preset();
    }
}

void ProjectExportDialog::_export_type_changed(int p_which) {

    if (updating)
        return;

    Ref<EditorExportPreset> current = get_current_preset();
    if (not current)
        return;

    current->set_export_filter(EditorExportPreset::ExportFilter(p_which));
    updating = true;
    _fill_resource_tree();
    updating = false;
}

void ProjectExportDialog::_filter_changed(StringView p_filter) {

    if (updating)
        return;

    Ref<EditorExportPreset> current = get_current_preset();
    if (not current)
        return;

    current->set_include_filter(include_filters->get_text());
    current->set_exclude_filter(exclude_filters->get_text());
}

void ProjectExportDialog::_fill_resource_tree() {

    include_files->clear();
    include_label->hide();
    include_margin->hide();

    Ref<EditorExportPreset> current = get_current_preset();
    if (not current)
        return;

    EditorExportPreset::ExportFilter f = current->get_export_filter();

    if (f == EditorExportPreset::EXPORT_ALL_RESOURCES) {
        return;
    }

    include_label->show();
    include_margin->show();

    TreeItem *root = include_files->create_item();

    _fill_tree(EditorFileSystem::get_singleton()->get_filesystem(), root, current, f == EditorExportPreset::EXPORT_SELECTED_SCENES);
}

bool ProjectExportDialog::_fill_tree(EditorFileSystemDirectory *p_dir, TreeItem *p_item, Ref<EditorExportPreset> &current, bool p_only_scenes) {

    p_item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
    p_item->set_icon(0, get_theme_icon("folder", "FileDialog"));
    p_item->set_text_utf8(0, p_dir->get_name() + "/");
    p_item->set_editable(0, true);
    p_item->set_metadata(0, p_dir->get_path());

    bool used = false;
    bool checked = true;
    for (int i = 0; i < p_dir->get_subdir_count(); i++) {

        TreeItem *subdir = include_files->create_item(p_item);
        if (_fill_tree(p_dir->get_subdir(i), subdir, current, p_only_scenes)) {
            used = true;
            checked = checked && subdir->is_checked(0);
        } else {
            memdelete(subdir);
        }
    }

    for (int i = 0; i < p_dir->get_file_count(); i++) {

        StringName type = p_dir->get_file_type(i);
        if (p_only_scenes && type != StringView("PackedScene"))
            continue;

        TreeItem *file = include_files->create_item(p_item);
        file->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
        file->set_text_utf8(0, p_dir->get_file(i));

        String path = p_dir->get_file_path(i);

        file->set_icon(0, EditorNode::get_singleton()->get_class_icon(type));
        file->set_editable(0, true);
        file->set_checked(0, current->has_export_file(path));
        file->set_metadata(0, path);
        checked = checked && file->is_checked(0);

        used = true;
    }

    p_item->set_checked(0, checked);
    return used;
}
static void _check_dir_recursive(ProjectExportDialog *self,TreeItem *p_dir, bool p_checked) {
    for (TreeItem *child = p_dir->get_children(); child; child = child->get_next()) {
        String path = child->get_metadata(0).as<String>();

        child->set_checked(0, p_checked);
        if (path.ends_with("/")) {
            _check_dir_recursive(self,child, p_checked);
        } else {
            if (p_checked) {
                self->get_current_preset()->add_export_file(path);
            } else {
                self->get_current_preset()->remove_export_file(path);
            }
        }
    }
}
static void _refresh_parent_checks(TreeItem *p_item) {
    TreeItem *parent = p_item->get_parent();
    if (!parent) {
        return;
    }

    bool checked = true;
    for (TreeItem *child = parent->get_children(); child; child = child->get_next()) {
        checked = checked && child->is_checked(0);
        if (!checked) {
            break;
        }
    }
    parent->set_checked(0, checked);

    _refresh_parent_checks(parent);
}

void ProjectExportDialog::_tree_changed() {

    if (updating)
        return;

    Ref<EditorExportPreset> current = get_current_preset();
    if (not current)
        return;

    TreeItem *item = include_files->get_edited();
    if (!item)
        return;

    String path = item->get_metadata(0).as<String>();
    bool added = item->is_checked(0);

    if (path.ends_with("/")) {
        _check_dir_recursive(this,item, added);
    } else {
        if (added) {
            current->add_export_file(path);
        } else {
            current->remove_export_file(path);
        }
    }
    _refresh_parent_checks(item); // Makes parent folder checked if all files/folders are checked.
}

void ProjectExportDialog::_export_pck_zip() {
    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(!current);

    String dir = PathUtils::get_base_dir(current->get_export_path());
    export_pck_zip->set_current_dir(dir);

    export_pck_zip->popup_centered_ratio();
}

void ProjectExportDialog::_export_pck_zip_selected(StringView p_path) {

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);
    Ref<EditorExportPlatform> platform = current->get_platform();
    ERR_FAIL_COND(not platform);

    if (StringUtils::ends_with(p_path,".zip")) {
        platform->export_zip(current, export_pck_zip_debug->is_pressed(), p_path);
    } else if (StringUtils::ends_with(p_path,".pck")) {
        platform->export_pack(current, export_pck_zip_debug->is_pressed(), p_path);
    }
}

void ProjectExportDialog::_open_export_template_manager() {

    EditorNode::get_singleton()->open_export_template_manager();
    hide();
}

void ProjectExportDialog::_validate_export_path(StringView p_path) {
    // Disable export via OK button or Enter key if LineEdit has an empty filename
    bool invalid_path = (PathUtils::get_basename(PathUtils::get_file(p_path)).empty());

    // Check if state change before needlessly messing with signals
    if (invalid_path && export_project->get_ok()->is_disabled())
        return;
    if (!invalid_path && !export_project->get_ok()->is_disabled())
        return;

    if (invalid_path) {
        export_project->get_ok()->set_disabled(true);
        export_project->get_line_edit()->disconnect("text_entered", callable_mp(export_project, &EditorFileDialog::_file_entered));
    } else {
        export_project->get_ok()->set_disabled(false);
        export_project->get_line_edit()->connect("text_entered", callable_mp(export_project, &EditorFileDialog::_file_entered));
    }
}

void ProjectExportDialog::_export_project() {

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);
    Ref<EditorExportPlatform> platform = current->get_platform();
    ERR_FAIL_COND(not platform);

    export_project->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
    export_project->clear_filters();

    Vector<String> extension_list = platform->get_binary_extensions(current);
    for (const auto &ext : extension_list) {
        export_project->add_filter("*." + ext + " ; " + platform->get_name() + " Export");
    }

    if (!current->get_export_path().empty()) {
        export_project->set_current_path(current->get_export_path());
    } else {
        if (!extension_list.empty()) {
            export_project->set_current_file(default_filename + "." + extension_list[0]);
        } else {
            export_project->set_current_file(default_filename);
        }
    }

    // Ensure that signal is connected if previous attempt left it disconnected with _validate_export_path
    if (!export_project->get_line_edit()->is_connected("text_entered", callable_mp(export_project, &EditorFileDialog::_file_entered))) {
        export_project->get_ok()->set_disabled(false);
        export_project->get_line_edit()->connect("text_entered", callable_mp(export_project, &EditorFileDialog::_file_entered));
    }

    export_project->set_mode(EditorFileDialog::MODE_SAVE_FILE);
    export_project->popup_centered_ratio();
}

void ProjectExportDialog::_export_project_to_path(StringView p_path) {
    // Save this name for use in future exports (but drop the file extension)
    default_filename = PathUtils::get_basename(PathUtils::get_file(p_path));
    EditorSettings::get_singleton()->set_project_metadata("export_options", "default_filename", default_filename);

    Ref<EditorExportPreset> current = get_current_preset();
    ERR_FAIL_COND(not current);
    Ref<EditorExportPlatform> platform = current->get_platform();
    ERR_FAIL_COND(not platform);
    current->set_export_path(p_path);

    platform->clear_messages();
    Error err = platform->export_project(current, export_debug->is_pressed(), p_path, 0);
    result_dialog_log->clear();

    if (err != ERR_SKIP) {
        if (platform->fill_log_messages(result_dialog_log, err)) {
            result_dialog->popup_centered_ratio(0.5);
        }
    }
}

void ProjectExportDialog::_export_all_dialog() {

    export_all_dialog->show();
    export_all_dialog->popup_centered_minsize(Size2(300, 80));
}

void ProjectExportDialog::_export_all_dialog_action(StringView p_str) {

    export_all_dialog->hide();

    _export_all(p_str != StringView("release"));
}

void ProjectExportDialog::_export_all(bool p_debug) {

    StringName mode = p_debug ? TTR("Debug") : TTR("Release");
    EditorProgress ep(("exportall"), TTR("Exporting All") + " " + mode, EditorExport::get_singleton()->get_export_preset_count(), true);

    bool show_dialog = false;
    result_dialog_log->clear();
    for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
        Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(i);
        ERR_FAIL_COND(!preset);
        Ref<EditorExportPlatform> platform = preset->get_platform();
        ERR_FAIL_COND(!platform);

        ep.step(preset->get_name(), i);

        platform->clear_messages();
        Error err = platform->export_project(preset, p_debug, preset->get_export_path(), 0);
        if (err == ERR_SKIP) {
            return;
        }
        bool has_messages = platform->fill_log_messages(result_dialog_log, err);
        show_dialog = show_dialog || has_messages;
    }
    if (show_dialog) {
        result_dialog->popup_centered_ratio(0.5);
    }
}

void ProjectExportDialog::_bind_methods() {

    MethodBinder::bind_method("get_drag_data_fw", &ProjectExportDialog::get_drag_data_fw);
    MethodBinder::bind_method("can_drop_data_fw", &ProjectExportDialog::can_drop_data_fw);
    MethodBinder::bind_method("drop_data_fw", &ProjectExportDialog::drop_data_fw);
    MethodBinder::bind_method("set_export_path", &ProjectExportDialog::set_export_path);
    MethodBinder::bind_method("get_export_path", &ProjectExportDialog::get_export_path);
    MethodBinder::bind_method("get_current_preset", &ProjectExportDialog::get_current_preset);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "export_path"), "set_export_path", "get_export_path");
}

ProjectExportDialog::ProjectExportDialog() {

    set_title(TTR("Export"));
    set_resizable(true);

    VBoxContainer *main_vb = memnew(VBoxContainer);
    add_child(main_vb);
    HSplitContainer *hbox = memnew(HSplitContainer);
    main_vb->add_child(hbox);
    hbox->set_v_size_flags(SIZE_EXPAND_FILL);

    // Presets list.

    VBoxContainer *preset_vb = memnew(VBoxContainer);
    preset_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    hbox->add_child(preset_vb);

    HBoxContainer *preset_hb = memnew(HBoxContainer);
    preset_hb->add_child(memnew(Label(TTR("Presets"))));
    preset_hb->add_spacer();
    preset_vb->add_child(preset_hb);

    add_preset = memnew(MenuButton);
    add_preset->set_text(TTR("Add..."));
    add_preset->get_popup()->connect("index_pressed",callable_mp(this, &ClassName::_add_preset));
    preset_hb->add_child(add_preset);
    MarginContainer *mc = memnew(MarginContainer);
    preset_vb->add_child(mc);
    mc->set_v_size_flags(SIZE_EXPAND_FILL);
    presets = memnew(ItemList);
    presets->set_drag_forwarding(this);
    mc->add_child(presets);
    presets->connect("item_selected",callable_mp(this, &ClassName::_edit_preset));
    duplicate_preset = memnew(ToolButton);
    duplicate_preset->set_tooltip(TTR("Duplicate"));
    preset_hb->add_child(duplicate_preset);
    duplicate_preset->connect("pressed",callable_mp(this, &ClassName::_duplicate_preset));
    delete_preset = memnew(ToolButton);
    delete_preset->set_tooltip(TTR("Delete"));
    preset_hb->add_child(delete_preset);
    delete_preset->connect("pressed",callable_mp(this, &ClassName::_delete_preset));

    // Preset settings.

    VBoxContainer *settings_vb = memnew(VBoxContainer);
    settings_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    hbox->add_child(settings_vb);

    name = memnew(LineEdit);
    settings_vb->add_margin_child(TTR("Name:"), name);
    name->connect("text_changed",callable_mp(this, &ClassName::_name_changed));
    runnable = memnew(CheckButton);
    runnable->set_text(TTR("Runnable"));
    runnable->set_tooltip(TTR("If checked, the preset will be available for use in one-click deploy.\nOnly one preset per platform may be marked as runnable."));
    runnable->connect("pressed",callable_mp(this, &ClassName::_runnable_pressed));
    settings_vb->add_child(runnable);

    export_path = memnew(EditorPropertyPath);
    settings_vb->add_child(export_path);
    export_path->set_label(TTR("Export Path"));
    export_path->set_object_and_property(this, "export_path");
    export_path->set_save_mode();
    export_path->connect("property_changed",callable_mp(this, &ClassName::_export_path_changed));

    // Subsections.

    sections = memnew(TabContainer);
    sections->set_tab_align(TabContainer::ALIGN_LEFT);
    sections->set_use_hidden_tabs_for_min_size(true);
    settings_vb->add_child(sections);
    sections->set_v_size_flags(SIZE_EXPAND_FILL);

    // Main preset parameters.

    parameters = memnew(EditorInspector);
    sections->add_child(parameters);
    parameters->set_name((TTR("Options")));
    parameters->set_v_size_flags(SIZE_EXPAND_FILL);
    parameters->set_property_name_style(EditorPropertyNameProcessor::get_settings_style());
    parameters->connect("property_edited",callable_mp(this, &ClassName::_update_parameters));
    EditorExport::get_singleton()->connect("export_presets_updated",callable_mp(this, &ClassName::_force_update_current_preset_parameters));

    // Resources export parameters.

    VBoxContainer *resources_vb = memnew(VBoxContainer);
    sections->add_child(resources_vb);
    resources_vb->set_name(TTR("Resources"));

    export_filter = memnew(OptionButton);
    export_filter->add_item(TTR("Export all resources in the project"));
    export_filter->add_item(TTR("Export selected scenes (and dependencies)"));
    export_filter->add_item(TTR("Export selected resources (and dependencies)"));
    resources_vb->add_margin_child(TTR("Export Mode:"), export_filter);
    export_filter->connect("item_selected",callable_mp(this, &ClassName::_export_type_changed));

    include_label = memnew(Label);
    include_label->set_text(TTR("Resources to export:"));
    resources_vb->add_child(include_label);
    include_margin = memnew(MarginContainer);
    include_margin->set_v_size_flags(SIZE_EXPAND_FILL);
    resources_vb->add_child(include_margin);

    include_files = memnew(Tree);
    include_margin->add_child(include_files);
    include_files->connect("item_edited",callable_mp(this, &ClassName::_tree_changed));

    include_filters = memnew(LineEdit);
    resources_vb->add_margin_child(
            TTR("Filters to export non-resource files/folders\n(comma-separated, e.g: *.json, *.txt, docs/*)"),
            include_filters);
    include_filters->connect("text_changed",callable_mp(this, &ClassName::_filter_changed));

    exclude_filters = memnew(LineEdit);
    resources_vb->add_margin_child(
            TTR("Filters to exclude files/folders from project\n(comma-separated, e.g: *.json, *.txt, docs/*)"),
            exclude_filters);
    exclude_filters->connect("text_changed",callable_mp(this, &ClassName::_filter_changed));

    // Patch packages.

    VBoxContainer *patch_vb = memnew(VBoxContainer);
    sections->add_child(patch_vb);
    patch_vb->set_name(TTR("Patches"));
    // FIXME: Patching support doesn't seem properly implemented yet, so we hide it.
    // The rest of the code is still kept for now, in the hope that it will be made
    // functional and reactivated.
    patch_vb->hide();

    patches = memnew(Tree);
    patch_vb->add_child(patches);
    patches->set_v_size_flags(SIZE_EXPAND_FILL);
    patches->set_hide_root(true);
    patches->connect("button_pressed",callable_mp(this, &ClassName::_patch_button_pressed));
    patches->connect("item_edited",callable_mp(this, &ClassName::_patch_edited));
    patches->set_drag_forwarding(this);
    patches->set_edit_checkbox_cell_only_when_checkbox_is_pressed(true);

    HBoxContainer *patches_hb = memnew(HBoxContainer);
    patch_vb->add_child(patches_hb);
    patches_hb->add_spacer();
    patch_export = memnew(Button);
    patch_export->set_text(TTR("Make Patch"));
    patches_hb->add_child(patch_export);
    patches_hb->add_spacer();

    patch_dialog = memnew(EditorFileDialog);
    patch_dialog->add_filter(("*.pck ; Pack File"));
    patch_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
    patch_dialog->connect("file_selected",callable_mp(this, &ClassName::_patch_selected));
    add_child(patch_dialog);

    patch_erase = memnew(ConfirmationDialog);
    patch_erase->get_ok()->set_text(TTR("Delete"));
    patch_erase->connect("confirmed",callable_mp(this, &ClassName::_patch_deleted));
    add_child(patch_erase);

    VBoxContainer *feature_vb = memnew(VBoxContainer);
    feature_vb->set_name((TTR("Features")));
    custom_features = memnew(LineEdit);
    custom_features->connect("text_changed",callable_mp(this, &ClassName::_custom_features_changed));
    feature_vb->add_margin_child(TTR("Custom (comma-separated):"), custom_features);
    custom_feature_display = memnew(RichTextLabel);
    custom_feature_display->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    feature_vb->add_margin_child(TTR("Feature List:"), custom_feature_display, true);
    sections->add_child(feature_vb);

    updating_script_key = false;

    VBoxContainer *script_vb = memnew(VBoxContainer);
    script_vb->set_name((TTR("Script")));
    script_mode = memnew(OptionButton);
    script_vb->add_margin_child(TTR("Script Export Mode:"), script_mode);
    script_mode->add_item(TTR("Text"), (int)EditorExportPreset::MODE_SCRIPT_TEXT);
    script_mode->add_item(TTR("Compiled"), (int)EditorExportPreset::MODE_SCRIPT_COMPILED);
    script_mode->add_item(TTR("Encrypted (Provide Key Below)"), (int)EditorExportPreset::MODE_SCRIPT_ENCRYPTED);
    script_mode->connect("item_selected",callable_mp(this, &ClassName::_script_export_mode_changed));
    script_key = memnew(LineEdit);
    script_key->connect("text_changed",callable_mp(this, &ClassName::_script_encryption_key_changed));
    script_key_error = memnew(Label);
    script_key_error->set_text("- " + TTR("Invalid Encryption Key (must be 64 characters long)"));
    script_key_error->add_theme_color_override("font_color", EditorNode::get_singleton()->get_gui_base()->get_theme_color("error_color", "Editor"));
    script_vb->add_margin_child(TTR("Script Encryption Key (256-bits as hex):"), script_key);
    script_vb->add_child(script_key_error);
    sections->add_child(script_vb);

    Label *sec_info = memnew(Label);
    sec_info->set_text(TTR("Note: Encryption key needs to be stored in the binary,\nyou need to build the export templates from source."));
    script_vb->add_child(sec_info);

    LinkButton *sec_more_info = memnew(LinkButton);
    sec_more_info->set_text(TTR("More Info..."));
    sec_more_info->connect("pressed", callable_mp(this, &ClassName::_open_key_help_link));
    script_vb->add_child(sec_more_info);
    sections->connect("tab_changed",callable_mp(this, &ClassName::_tab_changed));

    // Disable by default.
    name->set_editable(false);
    export_path->hide();
    runnable->set_disabled(true);
    duplicate_preset->set_disabled(true);
    delete_preset->set_disabled(true);
    script_key_error->hide();
    sections->hide();
    parameters->edit(nullptr);

    // Deletion dialog.
    delete_confirm = memnew(ConfirmationDialog);
    add_child(delete_confirm);
    delete_confirm->get_ok()->set_text(TTR("Delete"));
    delete_confirm->connect("confirmed",callable_mp(this, &ClassName::_delete_preset_confirm));

    // Export buttons, dialogs and errors.

    updating = false;

    get_cancel()->set_text(TTR("Close"));
    get_ok()->set_text(TTR("Export PCK/Zip..."));
    get_ok()->set_disabled(true);
    export_button = add_button(TTR("Export Project..."), !OS::get_singleton()->get_swap_ok_cancel(), "export");
    export_button->connect("pressed",callable_mp(this, &ClassName::_export_project));
    // Disable initially before we select a valid preset
    export_button->set_disabled(true);

    export_all_dialog = memnew(ConfirmationDialog);
    add_child(export_all_dialog);
    export_all_dialog->set_title(TTR("Export All"));
    export_all_dialog->set_text(TTR("Choose an export mode:"));
    export_all_dialog->get_ok()->hide();
    export_all_dialog->add_button(TTR("Debug"), true, "debug");
    export_all_dialog->add_button(TTR("Release"), true, "release");
    export_all_dialog->connect("custom_action",callable_mp(this, &ClassName::_export_all_dialog_action));

    export_all_button = add_button(TTR("Export All..."), !OS::get_singleton()->get_swap_ok_cancel(), "export");
    export_all_button->connect("pressed",callable_mp(this, &ClassName::_export_all_dialog));
    export_all_button->set_disabled(true);

    export_pck_zip = memnew(EditorFileDialog);
    export_pck_zip->add_filter("*.zip ; " + TTR("ZIP File"));
    export_pck_zip->add_filter("*.pck ; " + TTR("Godot Project Pack"));
    export_pck_zip->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
    export_pck_zip->set_mode(EditorFileDialog::MODE_SAVE_FILE);
    add_child(export_pck_zip);
    export_pck_zip->connect("file_selected",callable_mp(this, &ClassName::_export_pck_zip_selected));

    export_error = memnew(Label);
    main_vb->add_child(export_error);
    export_error->hide();
    export_error->add_theme_color_override("font_color", EditorNode::get_singleton()->get_gui_base()->get_theme_color("error_color", "Editor"));
    export_warning = memnew(Label);
    main_vb->add_child(export_warning);
    export_warning->hide();
    export_warning->add_theme_color_override("font_color", EditorNode::get_singleton()->get_gui_base()->get_theme_color("warning_color", "Editor"));

    export_templates_error = memnew(HBoxContainer);
    main_vb->add_child(export_templates_error);
    export_templates_error->hide();

    Label *export_error2 = memnew(Label);
    export_templates_error->add_child(export_error2);
    export_error2->add_theme_color_override("font_color", EditorNode::get_singleton()->get_gui_base()->get_theme_color("error_color", "Editor"));
    export_error2->set_text(" - " + TTR("Export templates for this platform are missing:") + " ");

    result_dialog = memnew(AcceptDialog);
    result_dialog->set_title(TTR("Project Export"));
    result_dialog_log = memnew(RichTextLabel);
    result_dialog_log->set_custom_minimum_size(Size2(300, 80)* EDSCALE);
    result_dialog->add_child(result_dialog_log);

    main_vb->add_child(result_dialog);
    result_dialog->hide();

    LinkButton *download_templates = memnew(LinkButton);
    download_templates->set_text(TTR("Manage Export Templates"));
    download_templates->set_v_size_flags(SIZE_SHRINK_CENTER);
    export_templates_error->add_child(download_templates);
    download_templates->connect("pressed",callable_mp(this, &ClassName::_open_export_template_manager));

    export_project = memnew(EditorFileDialog);
    export_project->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
    add_child(export_project);
    export_project->connect("file_selected",callable_mp(this, &ClassName::_export_project_to_path));
    export_project->get_line_edit()->connect("text_changed",callable_mp(this, &ClassName::_validate_export_path));

    export_debug = memnew(CheckBox);
    export_debug->set_text(TTR("Export With Debug"));
    export_debug->set_pressed(true);
    export_debug->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
    export_project->get_vbox()->add_child(export_debug);

    export_pck_zip_debug = memnew(CheckBox);
    export_pck_zip_debug->set_text(TTR("Export With Debug"));
    export_pck_zip_debug->set_pressed(true);
    export_pck_zip_debug->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
    export_pck_zip->get_vbox()->add_child(export_pck_zip_debug);

    set_hide_on_ok(false);

    editor_icons = "EditorIcons";

    default_filename = EditorSettings::get_singleton()->get_project_metadataT<String>("export_options", "default_filename", "");
    // If no default set, use project name
    if (default_filename.empty()) {
        // If no project name defined, use a sane default
        default_filename = ProjectSettings::get_singleton()->get("application/config/name").as<String>();
        if (default_filename.empty()) {
            default_filename = "UnnamedProject";
        }
    }
}

ProjectExportDialog::~ProjectExportDialog() = default;
