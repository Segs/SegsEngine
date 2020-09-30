/*************************************************************************/
/*  animation_tree_editor_plugin.cpp                                     */
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

#include "animation_tree_editor_plugin.h"

#include "animation_blend_space_1d_editor.h"
#include "animation_blend_space_2d_editor.h"
#include "animation_blend_tree_editor_plugin.h"
#include "animation_state_machine_editor.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/io/resource_loader.h"
#include "core/math/delaunay.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/project_settings.h"
#include "core/translation_helpers.h"
#include "editor/editor_scale.h"
#include "scene/animation/animation_blend_tree.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/margin_container.h"
#include "scene/main/viewport.h"
#include "scene/scene_string_names.h"

IMPL_GDCLASS(AnimationTreeNodeEditorPlugin)
IMPL_GDCLASS(AnimationTreeEditor)
IMPL_GDCLASS(AnimationTreeEditorPlugin)

void AnimationTreeEditor::edit(AnimationTree *p_tree) {

    if (tree == p_tree)
        return;

    tree = p_tree;

    if (!tree->has_meta("_tree_edit_path")) {
        current_root = ObjectID(0ULL);
        return;
    }

    Vector<String> path(tree->get_meta("_tree_edit_path").as<Vector<String>>());
    edit_path(path);
}

void AnimationTreeEditor::_path_button_pressed(int p_path) {

    edited_path.clear();
    for (int i = 0; i <= p_path; i++) {
        edited_path.push_back(button_path[i]);
    }
}

void AnimationTreeEditor::_update_path() {
    while (path_hb->get_child_count() > 1) {
        memdelete(path_hb->get_child(1));
    }

    Ref<ButtonGroup> group(make_ref_counted<ButtonGroup>());

    Button *b = memnew(Button);
    b->set_text("root");
    b->set_toggle_mode(true);
    b->set_button_group(group);
    b->set_pressed(true);
    b->set_focus_mode(FOCUS_NONE);
    b->connect("pressed",callable_mp(this, &ClassName::_path_button_pressed), varray(-1));
    path_hb->add_child(b);
    for (int i = 0; i < button_path.size(); i++) {
        b = memnew(Button);
        b->set_text_utf8(button_path[i]);
        b->set_toggle_mode(true);
        b->set_button_group(group);
        path_hb->add_child(b);
        b->set_pressed(true);
        b->set_focus_mode(FOCUS_NONE);
        b->connect("pressed",callable_mp(this, &ClassName::_path_button_pressed), varray(i));
    }
}

void AnimationTreeEditor::edit_path(const Vector<String> &p_path) {

    button_path.clear();

    Ref<AnimationNode> node = tree->get_tree_root();

    if (!node) {
        current_root = ObjectID(0ULL);
        edited_path = button_path;
        _update_path();
        return;
    }

    current_root = node->get_instance_id();

    for (int i = 0; i < p_path.size(); i++) {

        Ref<AnimationNode> child = node->get_child_by_name(StringName(p_path[i]));
        ERR_BREAK(not child);
        node = child;
        button_path.push_back(p_path[i]);
    }

    edited_path = button_path;

    for (int i = 0; i < editors.size(); i++) {
        if (editors[i]->can_edit(node)) {
            editors[i]->edit(node);
            editors[i]->show();
        } else {
            editors[i]->edit(Ref<AnimationNode>());
            editors[i]->hide();
        }
    }

    _update_path();
}

const Vector<String>& AnimationTreeEditor::get_edited_path() const {
    return button_path;
}

void AnimationTreeEditor::enter_editor(StringView p_path) {

    Vector<String> path(eastl::move(edited_path));
    path.emplace_back(p_path);
    edit_path(path);
}

void AnimationTreeEditor::_about_to_show_root() {
}

void AnimationTreeEditor::_notification(int p_what) {
    if (p_what == NOTIFICATION_PROCESS) {
        ObjectID root(0ULL);
        if (tree && tree->get_tree_root()) {
            root = tree->get_tree_root()->get_instance_id();
        }

        if (root != current_root) {
            edit_path({});
        }

        if (button_path.size() != edited_path.size()) {
            edit_path(edited_path);
        }
    }
}

void AnimationTreeEditor::_bind_methods() {
    MethodBinder::bind_method("_path_button_pressed", &AnimationTreeEditor::_path_button_pressed);
}

AnimationTreeEditor *AnimationTreeEditor::singleton = nullptr;

void AnimationTreeEditor::add_plugin(AnimationTreeNodeEditorPlugin *p_editor) {
    ERR_FAIL_COND(p_editor->get_parent());
    editor_base->add_child(p_editor);
    editors.push_back(p_editor);
    p_editor->set_h_size_flags(SIZE_EXPAND_FILL);
    p_editor->set_v_size_flags(SIZE_EXPAND_FILL);
    p_editor->hide();
}

void AnimationTreeEditor::remove_plugin(AnimationTreeNodeEditorPlugin *p_editor) {
    ERR_FAIL_COND(p_editor->get_parent() != editor_base);
    editor_base->remove_child(p_editor);
    editors.erase_first(p_editor);
}

String AnimationTreeEditor::get_base_path() {
    String path(SceneStringNames::get_singleton()->parameters_base_path.asCString());
    for (int i = 0; i < edited_path.size(); i++) {
        path += edited_path[i] + "/";
    }
    return path;
}

bool AnimationTreeEditor::can_edit(const Ref<AnimationNode> &p_node) const {
    for (int i = 0; i < editors.size(); i++) {
        if (editors[i]->can_edit(p_node)) {
            return true;
        }
    }
    return false;
}

Vector<String> AnimationTreeEditor::get_animation_list() {

    if (!singleton->is_visible()) {
        return {};
    }

    AnimationTree *tree = singleton->tree;
    if (!tree || !tree->has_node(tree->get_animation_player()))
        return {};

    AnimationPlayer *ap = object_cast<AnimationPlayer>(tree->get_node(tree->get_animation_player()));

    if (!ap)
        return {};

    Vector<StringName> anims(ap->get_animation_list());
    Vector<String> ret;
    for (const StringName &E : anims) {
        ret.emplace_back(E);
    }

    return ret;
}

AnimationTreeEditor::AnimationTreeEditor() {

    AnimationNodeAnimation::get_editable_animation_list = get_animation_list;
    path_edit = memnew(ScrollContainer);
    add_child(path_edit);
    path_edit->set_enable_h_scroll(true);
    path_edit->set_enable_v_scroll(false);
    path_hb = memnew(HBoxContainer);
    path_edit->add_child(path_hb);
    path_hb->add_child(memnew(Label(TTR("Path:"))));

    add_child(memnew(HSeparator));

    current_root = ObjectID(0ULL);
    singleton = this;
    editor_base = memnew(MarginContainer);
    editor_base->set_v_size_flags(SIZE_EXPAND_FILL);
    add_child(editor_base);

    add_plugin(memnew(AnimationNodeBlendTreeEditor));
    add_plugin(memnew(AnimationNodeBlendSpace1DEditor));
    add_plugin(memnew(AnimationNodeBlendSpace2DEditor));
    add_plugin(memnew(AnimationNodeStateMachineEditor));
}

void AnimationTreeEditorPlugin::edit(Object *p_object) {

    anim_tree_editor->edit(object_cast<AnimationTree>(p_object));
}

bool AnimationTreeEditorPlugin::handles(Object *p_object) const {

    return p_object->is_class("AnimationTree");
}

void AnimationTreeEditorPlugin::make_visible(bool p_visible) {

    if (p_visible) {
        //editor->hide_animation_player_editors();
        //editor->animation_panel_make_visible(true);
        button->show();
        editor->make_bottom_panel_item_visible(anim_tree_editor);
        anim_tree_editor->set_process(true);
    } else {

        if (anim_tree_editor->is_visible_in_tree())
            editor->hide_bottom_panel();
        button->hide();
        anim_tree_editor->set_process(false);
    }
}

AnimationTreeEditorPlugin::AnimationTreeEditorPlugin(EditorNode *p_node) {

    editor = p_node;
    anim_tree_editor = memnew(AnimationTreeEditor);
    anim_tree_editor->set_custom_minimum_size(Size2(0, 300) * EDSCALE);

    button = editor->add_bottom_panel_item(TTR("AnimationTree"), anim_tree_editor);
    button->hide();
}

AnimationTreeEditorPlugin::~AnimationTreeEditorPlugin() {
}
