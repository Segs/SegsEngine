/*************************************************************************/
/*  scene_create_dialog.cpp                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "scene_create_dialog.h"

#include "core/callable_method_pointer.h"
#include "core/class_db.h"
#include "core/method_bind_interface.h"
#include "core/os/dir_access.h"
#include "core/resource/resource_manager.h"
#include "editor/create_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/node_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/style_box.h"

IMPL_GDCLASS(SceneCreateDialog)

void SceneCreateDialog::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE:
        case NOTIFICATION_THEME_CHANGED: {
            select_node_button->set_button_icon(get_theme_icon("ClassList", "EditorIcons"));
            node_type_2d->set_button_icon(get_theme_icon("Node2D", "EditorIcons"));
            node_type_3d->set_button_icon(get_theme_icon("Spatial", "EditorIcons"));
            node_type_gui->set_button_icon(get_theme_icon("Control", "EditorIcons"));
            node_type_other->add_icon_override("icon", get_theme_icon("Node", "EditorIcons"));
            status_panel->add_theme_style_override("panel", get_theme_stylebox("bg", "Tree"));
        } break;
    }
}

void SceneCreateDialog::config(const String &p_dir) {
    directory = p_dir;
    root_name_edit->set_text("");
    scene_name_edit->set_text("");
    scene_name_edit->call_deferred("grab_focus");
    update_dialog();
}

void SceneCreateDialog::accept_create(Variant p_discard) {
    if (!get_ok()->is_disabled()) {
        hide();
        emit_signal("confirmed");
    }
}

void SceneCreateDialog::browse_types() {
    select_node_dialog->popup_create(true);
    select_node_dialog->set_title(TTR("Pick Root Node Type"));
    select_node_dialog->get_ok()->set_text(TTR("Pick"));
}

void SceneCreateDialog::on_type_picked() {
    other_type_display->set_text(StringUtils::get_slice(select_node_dialog->get_selected_type()," ", 0));
    if (node_type_other->is_pressed()) {
        update_dialog();
    } else {
        node_type_other->set_pressed(true); // Calls update_dialog() via group.
    }
}

void SceneCreateDialog::update_dialog(Variant p_discard) {
    scene_name = StringUtils::strip_edges(scene_name_edit->get_text());
    update_error(file_error_label, MSG_OK, TTR("Scene name is valid.").asCString());

    bool is_valid = true;
    if (scene_name.empty()) {
        update_error(file_error_label, MSG_ERROR, TTR("Scene name is empty.").asCString());
        is_valid = false;
    }

    if (is_valid) {
        if (!scene_name.ends_with(".")) {
            scene_name += ".";
        }
        scene_name += scene_extension_picker->get_selected_metadata().operator String();
    }

    if (is_valid && !StringUtils::is_valid_filename(scene_name)) {
        update_error(file_error_label, MSG_ERROR, TTR("File name invalid.").asCString());
        is_valid = false;
    }

    if (is_valid) {
        scene_name = PathUtils::plus_file(directory,scene_name);
        DirAccessRef da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
        if (da->file_exists(scene_name)) {
            update_error(file_error_label, MSG_ERROR, TTR("File already exists.").asCString());
            is_valid = false;
        }
    }

    const StringName root_type_name = StringName(other_type_display->get_text());
    if (has_icon(root_type_name, "EditorIcons")) {
        node_type_other->set_button_icon(get_theme_icon(root_type_name, "EditorIcons"));
    } else {
        node_type_other->set_button_icon({});
    }

    update_error(node_error_label, MSG_OK, "Root node valid.");

    root_name = StringUtils::strip_edges(root_name_edit->get_text());
    if (root_name.empty()) {
        root_name = PathUtils::get_basename(scene_name);
    }

    if (!StringUtils::is_valid_identifier(root_name)) {
        update_error(node_error_label, MSG_ERROR, TTR("Invalid root node name.").asCString());
        is_valid = false;
    }

    get_ok()->set_disabled(!is_valid);
}

void SceneCreateDialog::update_error(Label *p_label, MsgType p_type, const String &p_msg) {
    p_label->set_text(String("•  ") + p_msg);
    switch (p_type) {
        case MSG_OK:
            p_label->add_theme_color_override("font_color", get_theme_color("success_color", "Editor"));
            break;
        case MSG_ERROR:
            p_label->add_theme_color_override("font_color", get_theme_color("error_color", "Editor"));
            break;
    }
}

String SceneCreateDialog::get_scene_path() const {
    return scene_name;
}

Node *SceneCreateDialog::create_scene_root() {
    ERR_FAIL_NULL_V(node_type_group->get_pressed_button(), nullptr);
    RootType type = (RootType)node_type_group->get_pressed_button()->get_meta(type_meta).operator int();

    Node *root = nullptr;
    switch (type) {
        case ROOT_2D_SCENE:
            root = memnew(Node2D);
            break;
        case ROOT_3D_SCENE:
            root = memnew(Node3D);
            break;
        case ROOT_USER_INTERFACE: {
            Control *gui = memnew(Control);
            gui->set_anchors_and_margins_preset(Control::PRESET_WIDE);
            root = gui;
        } break;
        case ROOT_OTHER:
            root = object_cast<Node>(select_node_dialog->instance_selected());
            break;
    }

    ERR_FAIL_NULL_V(root, nullptr);
    root->set_name(root_name);
    return root;
}

SceneCreateDialog::SceneCreateDialog() {
    select_node_dialog = memnew(CreateDialog);
    add_child(select_node_dialog);
    select_node_dialog->set_base_type("Node");
    select_node_dialog->select_base();
    select_node_dialog->connect("create", callable_mp(this,&SceneCreateDialog::on_type_picked));

    VBoxContainer *main_vb = memnew(VBoxContainer);
    add_child(main_vb);

    GridContainer *gc = memnew(GridContainer);
    main_vb->add_child(gc);
    gc->set_columns(2);

    {
        Label *label = memnew(Label(TTR("Root Type:")));
        gc->add_child(label);
        label->set_v_size_flags(0);

        VBoxContainer *vb = memnew(VBoxContainer);
        gc->add_child(vb);

        node_type_group = make_ref_counted<ButtonGroup>();

        node_type_2d = memnew(CheckBox);
        vb->add_child(node_type_2d);
        node_type_2d->set_text(TTR("2D Scene"));
        node_type_2d->set_button_group(node_type_group);
        node_type_2d->set_meta(type_meta, ROOT_2D_SCENE);
        node_type_2d->set_pressed(true);

        node_type_3d = memnew(CheckBox);
        vb->add_child(node_type_3d);
        node_type_3d->set_text(TTR("3D Scene"));
        node_type_3d->set_button_group(node_type_group);
        node_type_3d->set_meta(type_meta, ROOT_3D_SCENE);

        node_type_gui = memnew(CheckBox);
        vb->add_child(node_type_gui);
        node_type_gui->set_text(TTR("User Interface"));
        node_type_gui->set_button_group(node_type_group);
        node_type_gui->set_meta(type_meta, ROOT_USER_INTERFACE);

        HBoxContainer *hb = memnew(HBoxContainer);
        vb->add_child(hb);

        node_type_other = memnew(CheckBox);
        hb->add_child(node_type_other);
        node_type_other->set_button_group(node_type_group);
        node_type_other->set_meta(type_meta, ROOT_OTHER);

        Control *spacing = memnew(Control);
        hb->add_child(spacing);
        spacing->set_custom_minimum_size(Size2(4 * EDSCALE, 0));

        other_type_display = memnew(LineEdit);
        hb->add_child(other_type_display);
        other_type_display->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        other_type_display->set_editable(false);
        other_type_display->set_text("Node");

        select_node_button = memnew(Button);
        hb->add_child(select_node_button);
        select_node_button->connect("pressed", callable_mp(this,&SceneCreateDialog::browse_types));

        node_type_group->connect("pressed", callable_mp(this,&SceneCreateDialog::update_dialog));
    }

    {
        Label *label = memnew(Label(TTR("Scene Name:")));
        gc->add_child(label);

        HBoxContainer *hb = memnew(HBoxContainer);
        gc->add_child(hb);

        scene_name_edit = memnew(LineEdit);
        hb->add_child(scene_name_edit);
        scene_name_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        scene_name_edit->connect("text_changed", callable_mp(this,&SceneCreateDialog::update_dialog));
        scene_name_edit->connect("text_entered", callable_mp(this,&SceneCreateDialog::accept_create));

        Vector<String> extensions;
        Ref<PackedScene> sd = make_ref_counted<PackedScene>();
        gResourceManager().get_recognized_extensions(sd, extensions);

        scene_extension_picker = memnew(OptionButton);
        hb->add_child(scene_extension_picker);
        for (const String & s : extensions) {
            scene_extension_picker->add_item("." + s);
            scene_extension_picker->set_item_metadata(scene_extension_picker->get_item_count() - 1, s);
        }
    }

    {
        Label *label = memnew(Label(TTR("Root Name:")));
        gc->add_child(label);

        root_name_edit = memnew(LineEdit);
        gc->add_child(root_name_edit);
        root_name_edit->set_placeholder(TTR("Leave empty to use scene name"));
        root_name_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        root_name_edit->connect("text_changed", callable_mp(this,&SceneCreateDialog::update_dialog));
        root_name_edit->connect("text_entered", callable_mp(this,&SceneCreateDialog::accept_create));
    }

    Control *spacing = memnew(Control);
    main_vb->add_child(spacing);
    spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

    status_panel = memnew(PanelContainer);
    main_vb->add_child(status_panel);
    status_panel->set_h_size_flags(Control::SIZE_FILL);
    status_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);

    VBoxContainer *status_vb = memnew(VBoxContainer);
    status_panel->add_child(status_vb);

    file_error_label = memnew(Label);
    status_vb->add_child(file_error_label);

    node_error_label = memnew(Label);
    status_vb->add_child(node_error_label);

    set_title(TTR("Create New Scene"));
    set_custom_minimum_size(Size2i(400 * EDSCALE, 0));
}
