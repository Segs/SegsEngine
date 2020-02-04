/*************************************************************************/
/*  animation_tree_editor_plugin.h                                       */
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

#pragma once

#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "editor/property_editor.h"
#include "scene/animation/animation_tree.h"
#include "scene/gui/button.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/popup.h"
#include "scene/gui/tree.h"

class AnimationTreeNodeEditorPlugin : public VBoxContainer {
    GDCLASS(AnimationTreeNodeEditorPlugin,VBoxContainer)

public:
    virtual bool can_edit(const Ref<AnimationNode> &p_node) = 0;
    virtual void edit(const Ref<AnimationNode> &p_node) = 0;
};

class AnimationTreeEditor : public VBoxContainer {

    GDCLASS(AnimationTreeEditor,VBoxContainer)

    ScrollContainer *path_edit;
    HBoxContainer *path_hb;

    AnimationTree *tree;
    PanelContainer *editor_base;

    PODVector<String> button_path;
    PODVector<String> edited_path;
    PODVector<AnimationTreeNodeEditorPlugin *> editors;

    void _update_path();
    void _about_to_show_root();
    ObjectID current_root;

    void _path_button_pressed(int p_path);

    static PODVector<String> get_animation_list();

protected:
    void _notification(int p_what);
    static void _bind_methods();

    static AnimationTreeEditor *singleton;

public:
    AnimationTree *get_tree() { return tree; }
    void add_plugin(AnimationTreeNodeEditorPlugin *p_editor);
    void remove_plugin(AnimationTreeNodeEditorPlugin *p_editor);

    String get_base_path();

    bool can_edit(const Ref<AnimationNode> &p_node) const;

    void edit_path(const PODVector<String> &p_path);
    const PODVector<String> &get_edited_path() const;

    void enter_editor(se_string_view p_path = {});
    static AnimationTreeEditor *get_singleton() { return singleton; }
    void edit(AnimationTree *p_tree);
    AnimationTreeEditor();
};

class AnimationTreeEditorPlugin : public EditorPlugin {

    GDCLASS(AnimationTreeEditorPlugin,EditorPlugin)

    AnimationTreeEditor *anim_tree_editor;
    EditorNode *editor;
    Button *button;

public:
    se_string_view get_name() const override { return "AnimationTree"; }
    bool has_main_screen() const override { return false; }
    void edit(Object *p_object) override;
    bool handles(Object *p_object) const override;
    void make_visible(bool p_visible) override;

    AnimationTreeEditorPlugin(EditorNode *p_node);
    ~AnimationTreeEditorPlugin() override;
};
