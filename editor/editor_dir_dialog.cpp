/*************************************************************************/
/*  editor_dir_dialog.cpp                                                */
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

#include "editor_dir_dialog.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/translation_helpers.h"
#include "editor/editor_file_system.h"
#include "editor/editor_settings.h"
#include "editor_scale.h"

IMPL_GDCLASS(EditorDirDialog)

void EditorDirDialog::_update_dir(TreeItem *p_item, EditorFileSystemDirectory *p_dir, StringView p_select_path) {

    updating = true;

    String path = p_dir->get_path();

    p_item->set_metadata(0, p_dir->get_path());
    p_item->set_icon(0, get_theme_icon("Folder", "EditorIcons"));
    p_item->set_icon_modulate(0, get_theme_color("folder_icon_modulate", "FileDialog"));

    if (!p_item->get_parent()) {
        p_item->set_text(0, "res://");
    } else {

        if (!opened_paths.contains(path) && (p_select_path.empty() || !StringUtils::begins_with(p_select_path,path))) {
            p_item->set_collapsed(true);
        }

        p_item->set_text_utf8(0, p_dir->get_name());
    }

    //this should be handled by EditorFileSystem already
    //bool show_hidden = EditorSettings::get_singleton()->get("filesystem/file_dialog/show_hidden_files");
    updating = false;
    for (int i = 0; i < p_dir->get_subdir_count(); i++) {

        TreeItem *ti = tree->create_item(p_item);
        _update_dir(ti, p_dir->get_subdir(i));
    }
}

void EditorDirDialog::reload(StringView p_path) {

    if (!is_visible_in_tree()) {
        must_reload = true;
        return;
    }

    tree->clear();
    TreeItem *root = tree->create_item();
    _update_dir(root, EditorFileSystem::get_singleton()->get_filesystem(), p_path);
    _item_collapsed(root);
    must_reload = false;
}

void EditorDirDialog::_notification(int p_what) {

    const auto reload_lambda = callable_gen(this, [this](){ reload();});
    if (p_what == NOTIFICATION_ENTER_TREE) {
        EditorFileSystem::get_singleton()->connect("filesystem_changed",reload_lambda);
        reload();

        if (!tree->is_connected("item_collapsed",callable_mp(this, &EditorDirDialog::_item_collapsed))) {
            tree->connect("item_collapsed",callable_mp(this, &EditorDirDialog::_item_collapsed), ObjectNS::CONNECT_QUEUED);
        }

        if (!EditorFileSystem::get_singleton()->is_connected("filesystem_changed",reload_lambda)) {
            EditorFileSystem::get_singleton()->connect("filesystem_changed",reload_lambda);
        }
    }

    if (p_what == NOTIFICATION_EXIT_TREE) {
        if (EditorFileSystem::get_singleton()->is_connected("filesystem_changed",reload_lambda)) {
            EditorFileSystem::get_singleton()->disconnect_all("filesystem_changed",get_instance_id());
        }
    }

    if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
        if (must_reload && is_visible_in_tree()) {
            reload();
        }
    }
}

void EditorDirDialog::_item_collapsed(Object *p_item) {

    TreeItem *item = object_cast<TreeItem>(p_item);

    if (updating)
        return;

    if (item->is_collapsed())
        opened_paths.erase(item->get_metadata(0).as<String>());
    else
        opened_paths.insert(item->get_metadata(0).as<String>());
}

void EditorDirDialog::ok_pressed() {

    TreeItem *ti = tree->get_selected();
    if (!ti)
        return;

    String dir = ti->get_metadata(0).as<String>();
    emit_signal("dir_selected", dir);
    hide();
}

void EditorDirDialog::_make_dir() {

    TreeItem *ti = tree->get_selected();
    if (!ti) {
        mkdirerr->set_text(TTR("Please select a base directory first."));
        mkdirerr->popup_centered_minsize();
        return;
    }

    makedialog->popup_centered_minsize(Size2(250, 80));
    makedirname->grab_focus();
}

void EditorDirDialog::_make_dir_confirm() {

    TreeItem *ti = tree->get_selected();
    if (!ti)
        return;

    String dir = ti->get_metadata(0).as<String>();

    DirAccessRef d = DirAccess::open(dir);
    ERR_FAIL_COND_MSG(!d, "Cannot open directory '" + dir + "'.");
    Error err = d->make_dir(makedirname->get_text());

    if (err != OK) {
        mkdirerr->popup_centered_minsize(Size2(250, 80) * EDSCALE);
    } else {
        opened_paths.insert(dir);
        //reload(PathUtils::plus_file(dir,makedirname->get_text()));
        EditorFileSystem::get_singleton()->scan_changes(); //we created a dir, so rescan changes
    }
    makedirname->set_text(""); // reset label
}

void EditorDirDialog::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("reload"), &EditorDirDialog::reload, {DEFVAL("")});

    ADD_SIGNAL(MethodInfo("dir_selected", PropertyInfo(VariantType::STRING, "dir")));
}

EditorDirDialog::EditorDirDialog() {

    updating = false;

    set_title(TTR("Choose a Directory"));
    set_hide_on_ok(false);

    tree = memnew(Tree);
    add_child(tree);

    tree->connect("item_activated",callable_mp((AcceptDialog *)this, &AcceptDialog::_ok_pressed));

    makedir = add_button(TTR("Create Folder"), OS::get_singleton()->get_swap_ok_cancel(), "makedir");
    makedir->connect("pressed",callable_mp(this, &EditorDirDialog::_make_dir));

    makedialog = memnew(ConfirmationDialog);
    makedialog->set_title(TTR("Create Folder"));
    add_child(makedialog);

    VBoxContainer *makevb = memnew(VBoxContainer);
    makedialog->add_child(makevb);
    //makedialog->set_child_rect(makevb);

    makedirname = memnew(LineEdit);
    makevb->add_margin_child(TTR("Name:"), makedirname);
    makedialog->register_text_enter(makedirname);
    makedialog->connect("confirmed",callable_mp(this, &EditorDirDialog::_make_dir_confirm));

    mkdirerr = memnew(AcceptDialog);
    mkdirerr->set_text(TTR("Could not create folder."));
    add_child(mkdirerr);

    get_ok()->set_text(TTR("Choose"));

    must_reload = false;
}
