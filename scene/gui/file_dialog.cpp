/*************************************************************************/
/*  file_dialog.cpp                                                      */
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

#include "file_dialog.h"

#include "core/callable_method_pointer.h"
#include "core/dictionary.h"
#include "core/method_bind.h"
#include "core/os/keyboard.h"
#include "core/print_string.h"
#include "core/translation_helpers.h"
#include "scene/gui/label.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/texture.h"

#include "EASTL/sort.h"

IMPL_GDCLASS(FileDialog)
IMPL_GDCLASS(LineEditFileChooser)
VARIANT_ENUM_CAST(FileDialog::Mode);
VARIANT_ENUM_CAST(FileDialog::Access);

FileDialog::GetIconFunc FileDialog::get_icon_func = nullptr;
FileDialog::GetIconFunc FileDialog::get_large_icon_func = nullptr;

FileDialog::RegisterFunc FileDialog::register_func = nullptr;
FileDialog::RegisterFunc FileDialog::unregister_func = nullptr;

VBoxContainer *FileDialog::get_vbox() {
    return vbox;
}

void FileDialog::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {

        if (p_what == NOTIFICATION_ENTER_TREE) {
            dir_up->set_button_icon(get_theme_icon("parent_folder"));
            refresh->set_button_icon(get_theme_icon("reload"));
            show_hidden->set_button_icon(get_theme_icon("toggle_hidden"));
        }

        Color font_color = get_theme_color("font_color", "ToolButton");
        Color font_color_hover = get_theme_color("font_color_hover", "ToolButton");
        Color font_color_focus = get_theme_color("font_color_focus", "ToolButton");
        Color font_color_pressed = get_theme_color("font_color_pressed", "ToolButton");

        dir_up->add_theme_color_override("icon_color_normal", font_color);
        dir_up->add_theme_color_override("icon_color_hover", font_color_hover);
        dir_up->add_theme_color_override("font_color_focus", font_color_focus);
        dir_up->add_theme_color_override("icon_color_pressed", font_color_pressed);

        refresh->add_theme_color_override("icon_color_normal", font_color);
        refresh->add_theme_color_override("icon_color_hover", font_color_hover);
        refresh->add_theme_color_override("font_color_focus", font_color_focus);
        refresh->add_theme_color_override("icon_color_pressed", font_color_pressed);

        show_hidden->add_theme_color_override("icon_color_normal", font_color);
        show_hidden->add_theme_color_override("icon_color_hover", font_color_hover);
        show_hidden->add_theme_color_override("font_color_focus", font_color_focus);
        show_hidden->add_theme_color_override("icon_color_pressed", font_color_pressed);

    } else if (p_what == NOTIFICATION_POPUP_HIDE) {

        set_process_unhandled_input(false);
    }
}
void FileDialog::_unhandled_input(const Ref<InputEvent> &p_event) {

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_event);

    if (!k || !is_window_modal_on_top() || !k->is_pressed())
        return;

    bool handled = true;

    switch (k->get_keycode()) {

        case KEY_H: {

            if (k->get_command()) {
                set_show_hidden_files(!show_hidden_files);
            } else {
                handled = false;
            }

        } break;
        case KEY_F5: {

            invalidate();
        } break;
        case KEY_BACKSPACE: {

            _dir_entered("..");
        } break;
        default: {
            handled = false;
        }
    }

    if (handled)
        accept_event();
}

void FileDialog::set_enable_multiple_selection(bool p_enable) {

    tree->set_select_mode(p_enable ? Tree::SELECT_MULTI : Tree::SELECT_SINGLE);
}

Vector<String> FileDialog::get_selected_files() const {

    Vector<String> list;

    TreeItem *item = tree->get_root();
    while ((item = tree->get_next_selected(item))) {

        list.emplace_back(PathUtils::plus_file(dir_access->get_current_dir(),item->get_text(0)));
    }

    return list;
}

void FileDialog::update_dir() {

    dir->set_text(dir_access->get_current_dir_without_drive());
    if (drives->is_visible()) {
        drives->select(dir_access->get_current_drive());
    }

    // Deselect any item, to make "Select Current Folder" button text by default.
    deselect_items();
}

void FileDialog::_dir_entered(StringView p_dir) {

    dir_access->change_dir(p_dir);
    file->set_text("");
    invalidate();
    update_dir();
}

void FileDialog::_file_entered(StringView p_file) {

    _action_pressed();
}

void FileDialog::_save_confirm_pressed() {
    String f = PathUtils::plus_file(dir_access->get_current_dir(),file->get_text());
    emit_signal("file_selected", f);
    hide();
}

void FileDialog::_post_popup() {

    ConfirmationDialog::_post_popup();
    if (invalidated) {
        update_file_list();
        invalidated = false;
    }
    if (mode == MODE_SAVE_FILE)
        file->grab_focus();
    else
        tree->grab_focus();

    set_process_unhandled_input(true);

    // For open dir mode, deselect all items on file dialog open.
    if (mode == MODE_OPEN_DIR) {
        deselect_items();
        file_box->set_visible(false);
    } else {
        file_box->set_visible(true);
    }
}

void FileDialog::_action_pressed() {

    if (mode == MODE_OPEN_FILES) {

        TreeItem *ti = tree->get_next_selected(nullptr);
        String fbase = dir_access->get_current_dir();

        PoolVector<String> files;
        while (ti) {

            files.push_back(PathUtils::plus_file(fbase,ti->get_text(0)));
            ti = tree->get_next_selected(ti);
        }

        if (!files.empty()) {
            emit_signal("files_selected", files);
            hide();
        }

        return;
    }

    String file_text = file->get_text();
    String f = PathUtils::is_abs_path(file_text) ?
                       file_text :
                       PathUtils::plus_file(dir_access->get_current_dir(), file->get_text());

    if ((mode == MODE_OPEN_ANY || mode == MODE_OPEN_FILE) && dir_access->file_exists(f)) {
        emit_signal("file_selected", f);
        hide();
    } else if (mode == MODE_OPEN_ANY || mode == MODE_OPEN_DIR) {

        String path = dir_access->get_current_dir();

        path =StringUtils::replace(path,"\\", "/");
        TreeItem *item = tree->get_selected();
        if (item) {
            Dictionary d = item->get_metadata(0).as<Dictionary>();
            if (d["dir"].as<bool>() && d["name"].as<String>() != "..") {
                path = PathUtils::plus_file(path,d["name"].as<String>());
            }
        }

        emit_signal("dir_selected", path);
        hide();
    }

    if (mode == MODE_SAVE_FILE) {

        bool valid = false;

        if (filter->get_selected() == filter->get_item_count() - 1) {
            valid = true; // match none
        } else if (filters.size() > 1 && filter->get_selected() == 0) {
            // match all filters
            for (const String &flt_src : filters) {

                StringView flt = StringUtils::get_slice(flt_src,';', 0);
                for (int j = 0; j < StringUtils::get_slice_count(flt,','); j++) {

                    StringView str = StringUtils::strip_edges(StringUtils::get_slice(flt,',', j));
                    if (StringUtils::match(f,str)) {
                        valid = true;
                        break;
                    }
                }
                if (valid)
                    break;
            }
        } else {
            int idx = filter->get_selected();
            if (filters.size() > 1)
                idx--;
            if (idx >= 0 && idx < filters.size()) {

                StringView flt = StringUtils::get_slice(filters[idx],';', 0);
                int filterSliceCount = StringUtils::get_slice_count(flt,',');
                for (int j = 0; j < filterSliceCount; j++) {

                    StringView str = StringUtils::strip_edges(StringUtils::get_slice(flt,',', j));
                    if (StringUtils::match(f,str)) {
                        valid = true;
                        break;
                    }
                }

                if (!valid && filterSliceCount > 0) {
                    StringView str = StringUtils::strip_edges(StringUtils::get_slice(flt,',', 0));
                    f += StringUtils::substr(str,1, str.length() - 1);
                    file->set_text(PathUtils::get_file(f));
                    valid = true;
                }
            } else {
                valid = true;
            }
        }

        if (!valid) {

            exterr->popup_centered_minsize(Size2(250, 80));
            return;
        }

        if (dir_access->file_exists(f)) {
            confirm_save->set_text(RTR("File exists, overwrite?"));
            confirm_save->popup_centered(Size2(200, 80));
        } else {

            emit_signal("file_selected", f);
            hide();
        }
    }
}

void FileDialog::_cancel_pressed() {

    file->set_text("");
    invalidate();
    hide();
}

bool FileDialog::_is_open_should_be_disabled() {

    if (mode == MODE_OPEN_ANY || mode == MODE_SAVE_FILE) {
        return false;
    }

    TreeItem *ti = tree->get_next_selected(tree->get_root());
    while (ti) {
        TreeItem *prev_ti = ti;
        ti = tree->get_next_selected(tree->get_root());
        if (ti == prev_ti) {
            break;
        }
    }
    // We have something that we can't select?
    if (!ti) {
        return mode != MODE_OPEN_DIR; // In "Open folder" mode, having nothing selected picks the current folder.
    }

    Dictionary d = ti->get_metadata(0).as<Dictionary>();

    // Opening a file, but selected a folder? Forbidden.
    return ((mode == MODE_OPEN_FILE || mode == MODE_OPEN_FILES) &&
                   d["dir"].as<bool>()) || // Flipped case, also forbidden.
           (mode == MODE_OPEN_DIR && !d["dir"].as<bool>());
}

void FileDialog::_go_up() {

    dir_access->change_dir("..");
    update_file_list();
    update_dir();
}

void FileDialog::deselect_items() {

    // Clear currently selected items in file manager.
    tree->deselect_all();

    // And change get_ok title.
    if (!tree->is_anything_selected()) {
        get_ok()->set_disabled(_is_open_should_be_disabled());

        switch (mode) {

            case MODE_OPEN_FILE:
            case MODE_OPEN_FILES:
                get_ok()->set_text(RTR("Open"));
                break;
            case MODE_OPEN_DIR:
                get_ok()->set_text(RTR("Select Current Folder"));
                break;
            case MODE_OPEN_ANY:
            case MODE_SAVE_FILE:
                // FIXME: Implement, or refactor to avoid duplication with set_mode
                break;
        }
    }
}

void FileDialog::_tree_multi_selected(Object *p_object, int p_cell, bool p_selected) {
    _tree_selected();
}

void FileDialog::_tree_selected() {

    TreeItem *ti = tree->get_selected();
    if (!ti)
        return;
    Dictionary d = ti->get_metadata(0).as<Dictionary>();

    if (!d["dir"].as<bool>()) {

        file->set_text(d["name"].as<String>());
    } else if (mode == MODE_OPEN_DIR) {
        get_ok()->set_text(RTR("Select This Folder"));
    }

    get_ok()->set_disabled(_is_open_should_be_disabled());
}

void FileDialog::_tree_item_activated() {

    TreeItem *ti = tree->get_selected();
    if (!ti)
        return;

    Dictionary d = ti->get_metadata(0).as<Dictionary>();

    if (d["dir"].as<bool>()) {

        dir_access->change_dir(d["name"].as<String>());
        if (mode == MODE_OPEN_FILE || mode == MODE_OPEN_FILES || mode == MODE_OPEN_DIR || mode == MODE_OPEN_ANY)
            file->set_text("");
        call_deferred([this]() {
            update_file_list();
            update_dir();
        });
    } else {

        _action_pressed();
    }
}

void FileDialog::update_file_name() {
    int idx = filter->get_selected() - 1;
    if ((idx == -1 && filter->get_item_count() == 2) ||
            (filter->get_item_count() > 2 && idx >= 0 && idx < filter->get_item_count() - 2)) {
        if (idx == -1) {
            idx += 1;
        }
        String filter_str = filters[idx];
        String file_str = file->get_text();
        String base_name(PathUtils::get_basename(file_str));
        file_str = base_name + "." + StringUtils::to_lower(StringUtils::strip_edges(filter_str));
        file->set_text(file_str);
    }
}

void FileDialog::update_file_list() {

    tree->clear();
    // Scroll back to the top after opening a directory
    tree->get_vscroll_bar()->set_value(0);

    dir_access->list_dir_begin();

    TreeItem *root = tree->create_item();
    Ref<Texture> folder = get_theme_icon("folder");
    Ref<Texture> file_icon = get_theme_icon("file");
    const Color folder_color = get_theme_color("folder_icon_modulate");
    const Color file_color = get_theme_color("file_icon_modulate");

    Vector<String> files;
    Vector<String> dirs;

    String item;

    while (!(item = dir_access->get_next()).empty()) {

        if (item == "." || item == "..")
            continue;

        bool is_hidden = dir_access->current_is_hidden();

        if (show_hidden_files || !is_hidden) {
            if (!dir_access->current_is_dir())
                files.push_back(item);
            else
                dirs.push_back(item);
        }
    }
    eastl::sort(dirs.begin(), dirs.end(), NaturalNoCaseComparator());
    eastl::sort(files.begin(), files.end(), NaturalNoCaseComparator());

    for(const String& dir_name : dirs) {
        TreeItem *ti = tree->create_item(root);
        ti->set_text_utf8(0, dir_name);
        ti->set_icon(0, folder);
        ti->set_icon_modulate(0, folder_color);

        Dictionary d;
        d["name"] = dir_name;
        d["dir"] = true;

        ti->set_metadata(0, d);
    }
    dirs.clear();

    Vector<String> patterns;
    // build filter
    if (filter->get_selected() == filter->get_item_count() - 1) {

        // match all
    } else if (filters.size() > 1 && filter->get_selected() == 0) {
        // match all filters
        for (int i = 0; i < filters.size(); i++) {

            StringView f = StringUtils::get_slice(filters[i],";", 0);
            for (int j = 0; j < StringUtils::get_slice_count(f,','); j++) {

                patterns.emplace_back(StringUtils::strip_edges(StringUtils::get_slice(f,",", j)));
            }
        }
    } else {
        int idx = filter->get_selected();
        if (filters.size() > 1)
            idx--;

        if (idx >= 0 && idx < filters.size()) {

            StringView f = StringUtils::get_slice(filters[idx],";", 0);
            for (int j = 0; j < StringUtils::get_slice_count(f,','); j++) {

                patterns.emplace_back(StringUtils::strip_edges(StringUtils::get_slice(f,',', j)));
            }
        }
    }

    String base_dir = dir_access->get_current_dir();

    for(const String &filename : files) {

        bool match = patterns.empty();
        String match_str;

        for (const String & E : patterns) {

            if (StringUtils::matchn(filename,E)) {
                match_str = E;
                match = true;
                break;
            }
        }

        if (match) {
            TreeItem *ti = tree->create_item(root);
            ti->set_text_utf8(0, filename);

            if (get_icon_func) {

                Ref<Texture> icon = get_icon_func(PathUtils::plus_file(base_dir,filename));
                ti->set_icon(0, icon);
            } else {
                ti->set_icon(0, file_icon);
            }
            ti->set_icon_modulate(0, file_color);

            if (mode == MODE_OPEN_DIR) {
                ti->set_custom_color(0, get_theme_color("files_disabled"));
                ti->set_selectable(0, false);
            }
            Dictionary d;
            d["name"] = filename;
            d["dir"] = false;
            ti->set_metadata(0, d);

            if (file->get_text() == filename || match_str == filename)
                ti->select(0);
        }
    }

    if (tree->get_root() && tree->get_root()->get_children() && tree->get_selected() == nullptr)
        tree->get_root()->get_children()->select(0);
}

void FileDialog::_filter_selected(int) {

    update_file_name();
    update_file_list();
}

void FileDialog::update_filters() {

    filter->clear();

    if (filters.size() > 1) {
        String all_filters;

        const int max_filters = 5;

        for (int i = 0; i < MIN(max_filters, filters.size()); i++) {
            StringView flt = StringUtils::strip_edges(StringUtils::get_slice(filters[i],";", 0));
            if (i > 0)
                all_filters += ',';
            all_filters += flt;
        }

        if (max_filters < filters.size())
            all_filters += (", ...");

        filter->add_item(RTR("All Recognized") + " ( " + all_filters + " )");
    }
    for (int i = 0; i < filters.size(); i++) {

        StringView flt = StringUtils::strip_edges(StringUtils::get_slice(filters[i],";", 0));
        StringView desc = StringUtils::strip_edges(StringUtils::get_slice(filters[i],";", 1));
        if (desc.length())
            filter->add_item(tr(StringName(desc)) + " ( " + flt + " )");
        else
            filter->add_item(StringName("( ") + flt + " )");
    }

    filter->add_item(RTR("All Files (*)"));
}

void FileDialog::clear_filters() {

    filters.clear();
    update_filters();
    invalidate();
}
void FileDialog::add_filter(StringView p_filter) {
    ERR_FAIL_COND_MSG(p_filter.starts_with("."), "Filter must be \"filename.extension\", can't start with dot.");

    filters.emplace_back(p_filter);
    update_filters();
    invalidate();
}

void FileDialog::set_filters(const Vector<String> &p_filters) {
    filters = p_filters;
    update_filters();
    invalidate();
}

const Vector<String> &FileDialog::get_filters() const {
    return filters;
}

String FileDialog::get_current_dir() const {

    return dir->get_text();
}
String FileDialog::get_current_file() const {

    return file->get_text();
}
String FileDialog::get_current_path() const {

    return PathUtils::plus_file(dir->get_text(),file->get_text());
}
void FileDialog::set_current_dir(StringView p_dir) {

    dir_access->change_dir(p_dir);
    update_dir();
    invalidate();
}
void FileDialog::set_current_file(StringView p_file) {

    file->set_text(p_file);
    update_dir();
    invalidate();
    auto lp = p_file.rfind('.');
    if (lp != StringView::npos) {
        file->select(0, lp);
        if (file->is_inside_tree() && !get_tree()->is_node_being_edited(file))
            file->grab_focus();
    }
}
void FileDialog::set_current_path(StringView p_path) {

    if (p_path.empty())
        return;
    auto base_p(PathUtils::path(p_path));
    if (base_p==StringView(".")) {
        set_current_file(p_path);
    } else {

        StringView dir = base_p;
        StringView file = PathUtils::get_file(base_p);
        set_current_dir(dir);
        set_current_file(file);
    }
}

void FileDialog::set_mode_overrides_title(bool p_override) {
    mode_overrides_title = p_override;
}

bool FileDialog::is_mode_overriding_title() const {
    return mode_overrides_title;
}

void FileDialog::set_mode(Mode p_mode) {

    ERR_FAIL_INDEX((int)p_mode, int(MODE_MAX));

    mode = p_mode;
    switch (mode) {

        case MODE_OPEN_FILE:
            get_ok()->set_text(RTR("Open"));
            if (mode_overrides_title)
                set_title(RTR("Open a File"));
            makedir->hide();
            break;
        case MODE_OPEN_FILES:
            get_ok()->set_text(RTR("Open"));
            if (mode_overrides_title)
                set_title(RTR("Open File(s)"));
            makedir->hide();
            break;
        case MODE_OPEN_DIR:
            get_ok()->set_text(RTR("Select Current Folder"));
            if (mode_overrides_title) {
                set_title(RTR("Open a Directory"));
            }
            makedir->show();
            break;
        case MODE_OPEN_ANY:
            get_ok()->set_text(RTR("Open"));
            if (mode_overrides_title) {
                set_title(RTR("Open a File or Directory"));
            }
            makedir->show();
            break;
        case MODE_SAVE_FILE:
            get_ok()->set_text(RTR("Save"));
            if (mode_overrides_title) {
                set_title(RTR("Save a File"));
            }
            makedir->show();
            break;
    }

    if (mode == MODE_OPEN_FILES) {
        tree->set_select_mode(Tree::SELECT_MULTI);
    } else {
        tree->set_select_mode(Tree::SELECT_SINGLE);
    }
}

FileDialog::Mode FileDialog::get_mode() const {

    return mode;
}

void FileDialog::set_access(Access p_access) {

    ERR_FAIL_INDEX(p_access, 3);
    if (access == p_access)
        return;
    memdelete(dir_access);
    switch (p_access) {
        case ACCESS_FILESYSTEM: {

            dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
        } break;
        case ACCESS_RESOURCES: {

            dir_access = DirAccess::create(DirAccess::ACCESS_RESOURCES);
        } break;
        case ACCESS_USERDATA: {

            dir_access = DirAccess::create(DirAccess::ACCESS_USERDATA);
        } break;
    }
    access = p_access;
    _update_drives();
    invalidate();
    update_filters();
    update_dir();
}

void FileDialog::invalidate() {

    if (is_visible_in_tree()) {
        update_file_list();
        invalidated = false;
    } else {
        invalidated = true;
    }
}

FileDialog::Access FileDialog::get_access() const {

    return access;
}

void FileDialog::_make_dir_confirm() {

    Error err = dir_access->make_dir(StringUtils::strip_edges(makedirname->get_text()));
    if (err == OK) {
        dir_access->change_dir(StringUtils::strip_edges(makedirname->get_text()));
        invalidate();
        update_filters();
        update_dir();
    } else {
        mkdirerr->popup_centered_minsize(Size2(250, 50));
    }
    makedirname->set_text(""); // reset label
}

void FileDialog::_make_dir() {

    makedialog->popup_centered_minsize(Size2(250, 80));
    makedirname->grab_focus();
}

void FileDialog::_select_drive(int p_idx) {

    String d = drives->get_item_text(p_idx);
    dir_access->change_dir(d);
    file->set_text("");
    invalidate();
    update_dir();
}

void FileDialog::_update_drives() {

    int dc = dir_access->get_drive_count();
    if (dc == 0 || access != ACCESS_FILESYSTEM) {
        drives->hide();
    } else {
        drives->clear();
        Node *dp = drives->get_parent();
        if (dp) {
            dp->remove_child(drives);
        }
        dp = dir_access->drives_are_shortcuts() ? shortcuts_container : drives_container;
        dp->add_child(drives);
        drives->show();

        for (int i = 0; i < dir_access->get_drive_count(); i++) {
            drives->add_item(StringName(dir_access->get_drive(i)));
        }

        drives->select(dir_access->get_current_drive());
    }
}

bool FileDialog::default_show_hidden_files = false;

void FileDialog::_bind_methods() {

    SE_BIND_METHOD(FileDialog,_unhandled_input);

    SE_BIND_METHOD(FileDialog,clear_filters);
    SE_BIND_METHOD(FileDialog,add_filter);
    SE_BIND_METHOD(FileDialog,set_filters);
    SE_BIND_METHOD(FileDialog,get_filters);
    SE_BIND_METHOD(FileDialog,get_current_dir);
    SE_BIND_METHOD(FileDialog,get_current_file);
    SE_BIND_METHOD(FileDialog,get_current_path);
    SE_BIND_METHOD(FileDialog,set_current_dir);
    SE_BIND_METHOD(FileDialog,set_current_file);
    SE_BIND_METHOD(FileDialog,set_current_path);
    MethodBinder::bind_method(
            D_METHOD("set_mode_overrides_title", { "override" }), &FileDialog::set_mode_overrides_title);
    SE_BIND_METHOD(FileDialog,is_mode_overriding_title);
    SE_BIND_METHOD(FileDialog,set_mode);
    SE_BIND_METHOD(FileDialog,get_mode);
    SE_BIND_METHOD(FileDialog,get_vbox);
    SE_BIND_METHOD(FileDialog,get_line_edit);
    SE_BIND_METHOD(FileDialog,set_access);
    SE_BIND_METHOD(FileDialog,get_access);
    SE_BIND_METHOD(FileDialog,set_show_hidden_files);
    SE_BIND_METHOD(FileDialog,is_showing_hidden_files);
    SE_BIND_METHOD(FileDialog,deselect_items);

    SE_BIND_METHOD(FileDialog,invalidate);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "mode_overrides_title"), "set_mode_overrides_title",
            "is_mode_overriding_title");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "mode", PropertyHint::Enum,
                         "Open File,Open Files,Open Folder,Open Any,Save"),
            "set_mode", "get_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "access", PropertyHint::Enum, "Resources,User data,File system"),
            "set_access", "get_access");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_STRING_ARRAY, "filters"), "set_filters", "get_filters");
    ADD_PROPERTY(
            PropertyInfo(VariantType::BOOL, "show_hidden_files"), "set_show_hidden_files", "is_showing_hidden_files");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "current_dir", PropertyHint::Dir, "",0), "set_current_dir",
            "get_current_dir");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "current_file", PropertyHint::File, "", 0), "set_current_file",
            "get_current_file");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "current_path", PropertyHint::None, "", 0), "set_current_path",
            "get_current_path");

    ADD_SIGNAL(MethodInfo("file_selected", PropertyInfo(VariantType::STRING, "path")));
    ADD_SIGNAL(MethodInfo("files_selected", PropertyInfo(VariantType::POOL_STRING_ARRAY, "paths")));
    ADD_SIGNAL(MethodInfo("dir_selected", PropertyInfo(VariantType::STRING, "dir")));

    BIND_ENUM_CONSTANT(MODE_OPEN_FILE);
    BIND_ENUM_CONSTANT(MODE_OPEN_FILES);
    BIND_ENUM_CONSTANT(MODE_OPEN_DIR);
    BIND_ENUM_CONSTANT(MODE_OPEN_ANY);
    BIND_ENUM_CONSTANT(MODE_SAVE_FILE);

    BIND_ENUM_CONSTANT(ACCESS_RESOURCES);
    BIND_ENUM_CONSTANT(ACCESS_USERDATA);
    BIND_ENUM_CONSTANT(ACCESS_FILESYSTEM);
}

void FileDialog::set_show_hidden_files(bool p_show) {
    show_hidden_files = p_show;
    invalidate();
}

bool FileDialog::is_showing_hidden_files() const {
    return show_hidden_files;
}

void FileDialog::set_default_show_hidden_files(bool p_show) {
    default_show_hidden_files = p_show;
}

FileDialog::FileDialog() {

    show_hidden_files = default_show_hidden_files;

    mode_overrides_title = true;

    VBoxContainer *vbc = memnew(VBoxContainer);
    add_child(vbc);

    mode = MODE_SAVE_FILE;
    set_title(RTR("Save a File"));

    HBoxContainer *hbc = memnew(HBoxContainer);

    dir_up = memnew(ToolButton);
    dir_up->set_tooltip(RTR("Go to parent folder."));
    hbc->add_child(dir_up);
    dir_up->connect("pressed",callable_mp(this, &ClassName::_go_up));

    hbc->add_child(memnew(Label(RTR("Path:"))));

    drives_container = memnew(HBoxContainer);
    hbc->add_child(drives_container);

    drives = memnew(OptionButton);
    drives->connect("item_selected",callable_mp(this, &ClassName::_select_drive));
    hbc->add_child(drives);

    dir = memnew(LineEdit);
    hbc->add_child(dir);
    dir->set_h_size_flags(SIZE_EXPAND_FILL);

    refresh = memnew(ToolButton);
    refresh->set_tooltip(RTR("Refresh files."));
    refresh->connect("pressed",callable_mp(this, &ClassName::update_file_list));
    hbc->add_child(refresh);

    show_hidden = memnew(ToolButton);
    show_hidden->set_toggle_mode(true);
    show_hidden->set_pressed(is_showing_hidden_files());
    show_hidden->set_tooltip(RTR("Toggle the visibility of hidden files."));
    show_hidden->connect("toggled",callable_mp(this, &ClassName::set_show_hidden_files));
    hbc->add_child(show_hidden);

    shortcuts_container = memnew(HBoxContainer);
    hbc->add_child(shortcuts_container);

    makedir = memnew(Button);
    makedir->set_text(RTR("Create Folder"));
    makedir->connect("pressed",callable_mp(this, &ClassName::_make_dir));
    hbc->add_child(makedir);
    vbc->add_child(hbc);

    tree = memnew(Tree);
    tree->set_hide_root(true);
    vbc->add_margin_child(RTR("Directories & Files:"), tree, true);

    file_box = memnew(HBoxContainer);
    file_box->add_child(memnew(Label(RTR("File:"))));
    file = memnew(LineEdit);
    file->set_stretch_ratio(4);
    file->set_h_size_flags(SIZE_EXPAND_FILL);
    file_box->add_child(file);
    filter = memnew(OptionButton);
    filter->set_stretch_ratio(3);
    filter->set_h_size_flags(SIZE_EXPAND_FILL);
    filter->set_clip_text(true); // too many extensions overflows it
    file_box->add_child(filter);
    vbc->add_child(file_box);

    dir_access = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    access = ACCESS_RESOURCES;
    _update_drives();

    connect("confirmed",callable_mp(this, &ClassName::_action_pressed));
    tree->connect("multi_selected",callable_mp(this, &ClassName::_tree_multi_selected), ObjectNS::CONNECT_QUEUED);
    tree->connect("cell_selected",callable_mp(this, &ClassName::_tree_selected), ObjectNS::CONNECT_QUEUED);
    tree->connect("item_activated",callable_mp(this, &ClassName::_tree_item_activated));
    tree->connect("nothing_selected",callable_mp(this, &ClassName::deselect_items));
    dir->connect("text_entered",callable_mp(this, &ClassName::_dir_entered));
    file->connect("text_entered",callable_mp(this, &ClassName::_file_entered));
    filter->connect("item_selected",callable_mp(this, &ClassName::_filter_selected));

    confirm_save = memnew(ConfirmationDialog);
    confirm_save->set_as_top_level(true);
    add_child(confirm_save);

    confirm_save->connect("confirmed",callable_mp(this, &ClassName::_save_confirm_pressed));

    makedialog = memnew(ConfirmationDialog);
    makedialog->set_title(RTR("Create Folder"));
    VBoxContainer *makevb = memnew(VBoxContainer);
    makedialog->add_child(makevb);

    makedirname = memnew(LineEdit);
    makevb->add_margin_child(RTR("Name:"), makedirname);
    add_child(makedialog);
    makedialog->register_text_enter(makedirname);
    makedialog->connect("confirmed",callable_mp(this, &ClassName::_make_dir_confirm));
    mkdirerr = memnew(AcceptDialog);
    mkdirerr->set_text(RTR("Could not create folder."));
    add_child(mkdirerr);

    exterr = memnew(AcceptDialog);
    exterr->set_text(RTR("Must use a valid extension."));
    add_child(exterr);

    update_filters();
    update_dir();

    set_hide_on_ok(false);
    vbox = vbc;

    invalidated = true;
    if (register_func)
        register_func(this);
}

FileDialog::~FileDialog() {

    if (unregister_func)
        unregister_func(this);
    memdelete(dir_access);
}

void LineEditFileChooser::_bind_methods() {

    SE_BIND_METHOD(LineEditFileChooser,get_button);
    SE_BIND_METHOD(LineEditFileChooser,get_line_edit);
    SE_BIND_METHOD(LineEditFileChooser,get_file_dialog);
}

void LineEditFileChooser::_chosen(StringView p_text) {

    line_edit->set_text(p_text);
    line_edit->emit_signal("text_entered", p_text);
}

void LineEditFileChooser::_browse() {

    dialog->popup_centered_ratio();
}

LineEditFileChooser::LineEditFileChooser() {

    line_edit = memnew(LineEdit);
    add_child(line_edit);
    line_edit->set_h_size_flags(SIZE_EXPAND_FILL);
    button = memnew(Button);
    button->set_text(" .. ");
    add_child(button);
    button->connect("pressed",callable_mp(this, &ClassName::_browse));
    dialog = memnew(FileDialog);
    add_child(dialog);
    dialog->connect("file_selected",callable_mp(this, &ClassName::_chosen));
    dialog->connect("dir_selected",callable_mp(this, &ClassName::_chosen));
    dialog->connect("files_selected",callable_mp(this, &ClassName::_chosen));
}
