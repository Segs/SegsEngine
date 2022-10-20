/*************************************************************************/
/*  resource_preloader_editor_plugin.cpp                                 */
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

#include "resource_preloader_editor_plugin.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"
#include "core/translation_helpers.h"
#include "editor/editor_file_dialog.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "EASTL/sort.h"

IMPL_GDCLASS(ResourcePreloaderEditor)
IMPL_GDCLASS(ResourcePreloaderEditorPlugin)

void ResourcePreloaderEditor::_gui_input(const Ref<InputEvent>& p_event) {
}

void ResourcePreloaderEditor::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE:
        case NOTIFICATION_THEME_CHANGED: {
        load->set_button_icon(get_theme_icon("Folder", "EditorIcons"));
        } break;
    }
}

void ResourcePreloaderEditor::_files_load_request(const Vector<String> &p_paths) {

    for (int i = 0; i < p_paths.size(); i++) {

        StringView path = p_paths[i];

        RES resource;
        resource = gResourceManager().load(path);

        if (not resource) {
            dialog->set_text(TTR("ERROR: Couldn't load resource!"));
            dialog->set_title(TTR("Error!"));
            //dialog->get_cancel()->set_text("Close");
            dialog->get_ok()->set_text(TTR("Close"));
            dialog->popup_centered_minsize();
            return; ///beh should show an error i guess
        }

        StringView basename = PathUtils::get_basename(PathUtils::get_file(path));
        String name(basename);
        int counter = 1;
        while (preloader->has_resource(StringName(name))) {
            counter++;
            name = String(basename) + " " + itos(counter);
        }

        undo_redo->create_action(TTR("Add Resource"));
        undo_redo->add_do_method(preloader, "add_resource", name, resource);
        undo_redo->add_undo_method(preloader, "remove_resource", name);
        undo_redo->add_do_method(this, "_update_library");
        undo_redo->add_undo_method(this, "_update_library");
        undo_redo->commit_action();
    }
}

void ResourcePreloaderEditor::_load_pressed() {

    loading_scene = false;

    file->clear_filters();
    Vector<String> extensions;
    gResourceManager().get_recognized_extensions_for_type("", extensions);
    for (const String & ext : extensions)
        file->add_filter("*." + ext);

    file->set_mode(EditorFileDialog::MODE_OPEN_FILES);

    file->popup_centered_ratio();
}

void ResourcePreloaderEditor::_item_edited() {

    if (!tree->get_selected())
        return;

    TreeItem *s = tree->get_selected();

    if (tree->get_selected_column() == 0) {
        // renamed
        StringName old_name = s->get_metadata(0).as<StringName>();
        StringName new_name(s->get_text(0));
        if (old_name == new_name)
            return;

        if (new_name.empty() || StringUtils::contains(new_name,'\\') || StringUtils::contains(new_name,'/') || preloader->has_resource(new_name)) {

            s->set_text(0, old_name);
            return;
        }

        RES samp(preloader->get_resource(StringName(old_name)));
        undo_redo->create_action(TTR("Rename Resource"));
        undo_redo->add_do_method(preloader, "remove_resource", old_name);
        undo_redo->add_do_method(preloader, "add_resource", new_name, samp);
        undo_redo->add_undo_method(preloader, "remove_resource", new_name);
        undo_redo->add_undo_method(preloader, "add_resource", old_name, samp);
        undo_redo->add_do_method(this, "_update_library");
        undo_redo->add_undo_method(this, "_update_library");
        undo_redo->commit_action();
    }
}

void ResourcePreloaderEditor::_remove_resource(const StringName &p_to_remove) {

    undo_redo->create_action(TTR("Delete Resource"));
    undo_redo->add_do_method(preloader, "remove_resource", p_to_remove);
    undo_redo->add_undo_method(preloader, "add_resource", p_to_remove, preloader->get_resource(StringName(p_to_remove)));
    undo_redo->add_do_method(this, "_update_library");
    undo_redo->add_undo_method(this, "_update_library");
    undo_redo->commit_action();
}

void ResourcePreloaderEditor::_paste_pressed() {

    RES r(EditorSettings::get_singleton()->get_resource_clipboard());
    if (not r) {
        dialog->set_text(TTR("Resource clipboard is empty!"));
        dialog->set_title(TTR("Error!"));
        dialog->get_ok()->set_text(TTR("Close"));
        dialog->popup_centered_minsize();
        return; ///beh should show an error i guess
    }

    String name(r->get_name());
    if (name.empty())
        name = PathUtils::get_file(r->get_path());
    if (name.empty())
        name = r->get_class();

    String basename = name;
    int counter = 1;
    while (preloader->has_resource(StringName(name))) {
        counter++;
        name = basename + " " + itos(counter);
    }

    undo_redo->create_action(TTR("Paste Resource"));
    undo_redo->add_do_method(preloader, "add_resource", name, r);
    undo_redo->add_undo_method(preloader, "remove_resource", name);
    undo_redo->add_do_method(this, "_update_library");
    undo_redo->add_undo_method(this, "_update_library");
    undo_redo->commit_action();
}

void ResourcePreloaderEditor::_update_library() {

    tree->clear();
    tree->set_hide_root(true);
    TreeItem *root = tree->create_item(nullptr);

    List<StringName> rnames;
    preloader->get_resource_list(&rnames);

    Vector<StringName> names(rnames.begin(),rnames.end());

    eastl::sort(names.begin(),names.end());

    for (const StringName &E : names) {

        TreeItem *ti = tree->create_item(root);
        ti->set_cell_mode(0, TreeItem::CELL_MODE_STRING);
        ti->set_editable(0, true);
        ti->set_selectable(0, true);
        ti->set_text_utf8(0, E);
        ti->set_metadata(0, E);

        RES r(preloader->get_resource(E));

        ERR_CONTINUE(not r);

        StringName type(r->get_class());
        ti->set_icon(0, EditorNode::get_singleton()->get_class_icon(type));
        ti->set_tooltip(0, TTR("Instance:") + " " + r->get_path() + "\n" + TTR("Type:") + " " + type);

        ti->set_text_utf8(1, r->get_path());
        ti->set_editable(1, false);
        ti->set_selectable(1, false);

        if (type == "PackedScene") {
            ti->add_button(1, get_theme_icon("InstanceOptions", "EditorIcons"), BUTTON_OPEN_SCENE, false, TTR("Open in Editor"));
        } else {
            ti->add_button(1, get_theme_icon("Load", "EditorIcons"), BUTTON_EDIT_RESOURCE, false, TTR("Open in Editor"));
        }
        ti->add_button(1, get_theme_icon("Remove", "EditorIcons"), BUTTON_REMOVE, false, TTR("Remove"));
    }

    //player->add_resource("default",resource);
}

void ResourcePreloaderEditor::_cell_button_pressed(Object *p_item, int p_column, int p_id) {

    TreeItem *item = object_cast<TreeItem>(p_item);
    ERR_FAIL_COND(!item);

    if (p_id == BUTTON_OPEN_SCENE) {
        String rpath(item->get_text(p_column));
        EditorInterface::get_singleton()->open_scene_from_path(rpath);

    } else if (p_id == BUTTON_EDIT_RESOURCE) {
        RES r(preloader->get_resource(StringName(item->get_text(0))));
        EditorInterface::get_singleton()->edit_resource(r);

    } else if (p_id == BUTTON_REMOVE) {
        _remove_resource(StringName(item->get_text(0)));
    }
}

void ResourcePreloaderEditor::edit(ResourcePreloader *p_preloader) {

    preloader = p_preloader;

    if (p_preloader) {
        _update_library();
    } else {

        hide();
        set_physics_process(false);
    }
}

Variant ResourcePreloaderEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {

    TreeItem *ti = tree->get_item_at_position(p_point);
    if (!ti)
        return Variant();

    StringName name(ti->get_metadata(0).as<StringName>());

    RES res(preloader->get_resource(name));
    if (not res)
        return Variant();

    return EditorNode::get_singleton()->drag_resource(res, p_from);
}

bool ResourcePreloaderEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {

    Dictionary d = p_data.as<Dictionary>();

    if (!d.has("type"))
        return false;

    if (d.has("from") && d["from"].as<Object *>() == tree)
        return false;

    if (d["type"].as<String>() == "resource" && d.has("resource")) {
        RES r(d["resource"]);

        return r;
    }

    if (d["type"].as<String>() == "files") {

        PoolVector<String> files(d["files"].as<PoolVector<String>>());

        return !files.empty();
    }
    return false;
}

void ResourcePreloaderEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {

    if (!can_drop_data_fw(p_point, p_data, p_from))
        return;

    Dictionary d = p_data.as<Dictionary>();

    if (!d.has("type"))
        return;

    if (d["type"].as<String>() == "resource" && d.has("resource")) {
        RES r(d["resource"]);

        if (r) {

            String basename;
            if (!r->get_name().empty()) {
                basename = r->get_name();
            } else if (PathUtils::is_resource_file(r->get_path())) {
                basename = PathUtils::get_basename(r->get_path());
            } else {
                basename = "Resource";
            }

            String name = basename;
            int counter = 0;
            while (preloader->has_resource(StringName(name))) {
                counter++;
                name = basename + "_" + itos(counter);
            }

            undo_redo->create_action(TTR("Add Resource"));
            undo_redo->add_do_method(preloader, "add_resource", name, r);
            undo_redo->add_undo_method(preloader, "remove_resource", name);
            undo_redo->add_do_method(this, "_update_library");
            undo_redo->add_undo_method(this, "_update_library");
            undo_redo->commit_action();
        }
    }

    if (d["type"].as<String>() == "files") {

        Vector<String> files(d["files"].as<Vector<String>>());

        _files_load_request(files);
    }
}

void ResourcePreloaderEditor::_bind_methods() {

    SE_BIND_METHOD(ResourcePreloaderEditor,_gui_input);
    SE_BIND_METHOD(ResourcePreloaderEditor,_update_library);

    SE_BIND_METHOD(ResourcePreloaderEditor,get_drag_data_fw);
    SE_BIND_METHOD(ResourcePreloaderEditor,can_drop_data_fw);
    SE_BIND_METHOD(ResourcePreloaderEditor,drop_data_fw);
}

ResourcePreloaderEditor::ResourcePreloaderEditor() {

    //add_theme_style_override("panel", EditorNode::get_singleton()->get_gui_base()->get_stylebox("panel","Panel"));

    VBoxContainer *vbc = memnew(VBoxContainer);
    add_child(vbc);

    HBoxContainer *hbc = memnew(HBoxContainer);
    vbc->add_child(hbc);

    load = memnew(Button);
    load->set_tooltip(TTR("Load Resource"));
    hbc->add_child(load);

    paste = memnew(Button);
    paste->set_text(TTR("Paste"));
    hbc->add_child(paste);

    file = memnew(EditorFileDialog);
    add_child(file);

    tree = memnew(Tree);
    tree->connect("button_pressed",callable_mp(this, &ClassName::_cell_button_pressed));
    tree->set_columns(2);
    tree->set_column_min_width(0, 2);
    tree->set_column_min_width(1, 3);
    tree->set_column_expand(0, true);
    tree->set_column_expand(1, true);
    tree->set_v_size_flags(SIZE_EXPAND_FILL);

    tree->set_drag_forwarding(this);
    vbc->add_child(tree);

    dialog = memnew(AcceptDialog);
    add_child(dialog);

    load->connect("pressed",callable_mp(this, &ClassName::_load_pressed));
    paste->connect("pressed",callable_mp(this, &ClassName::_paste_pressed));
    file->connect("files_selected",callable_mp(this, &ClassName::_files_load_request));
    tree->connect("item_edited",callable_mp(this, &ClassName::_item_edited));
    loading_scene = false;
}

void ResourcePreloaderEditorPlugin::edit(Object *p_object) {

    preloader_editor->set_undo_redo(get_undo_redo());
    ResourcePreloader *s = object_cast<ResourcePreloader>(p_object);
    if (!s)
        return;

    preloader_editor->edit(s);
}

bool ResourcePreloaderEditorPlugin::handles(Object *p_object) const {

    return p_object->is_class("ResourcePreloader");
}

void ResourcePreloaderEditorPlugin::make_visible(bool p_visible) {

    if (p_visible) {
        //preloader_editor->show();
        button->show();
        editor->make_bottom_panel_item_visible(preloader_editor);
        //preloader_editor->set_process(true);
    } else {

        if (preloader_editor->is_visible_in_tree())
            editor->hide_bottom_panel();
        button->hide();
        //preloader_editor->hide();
        //preloader_editor->set_process(false);
    }
}

ResourcePreloaderEditorPlugin::ResourcePreloaderEditorPlugin(EditorNode *p_node) {

    editor = p_node;
    preloader_editor = memnew(ResourcePreloaderEditor);
    preloader_editor->set_custom_minimum_size(Size2(0, 250) * EDSCALE);

    button = editor->add_bottom_panel_item(TTR("ResourcePreloader"), preloader_editor);
    button->hide();

    //preloader_editor->set_anchor( Margin::TOP, Control::ANCHOR_END);
    //preloader_editor->set_margin( Margin::TOP, 120 );
}

ResourcePreloaderEditorPlugin::~ResourcePreloaderEditorPlugin() = default;
