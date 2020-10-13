/*
 * SEGS - Super Entity Game Server Engine
 * http://www.segs.dev/
 * Copyright (c) 2006 - 2020 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License. See LICENSE_SEGS.md for details.
 */
#pragma once

#include "editor/editor_plugin.h"
#include "editor/pane_drag.h"
#include "scene_map.h"


class Node3DEditorPlugin;
class SpinBox;
class MenuButton;
class HSlider;
class ConfirmationDialog;
class ItemList;
class Tree;

class SceneMapEditor : public VBoxContainer {
    GDCLASS(SceneMapEditor, VBoxContainer)

    SceneMap* m_node = nullptr;
    EditorNode *m_editor = nullptr;
    Tree* scene_library_palette;
    Label *info_message;

    void _notification(int p_what);
public:
    void edit(SceneMap* p_scenemap);
    SceneMapEditor() = default;
    explicit SceneMapEditor(EditorNode* p_editor);
    ~SceneMapEditor() override;

};

class SceneMapEditorPlugin : public EditorPlugin {

    GDCLASS(SceneMapEditorPlugin, EditorPlugin)
    SceneMapEditor * scene_map_editor;

public:

    StringView get_name() const override { return "SceneMap"; }
    bool has_main_screen() const override { return false; }

    bool handles(Object* p_object) const override;
    void make_visible(bool p_visible) override;

    void edit(Object* p_object) override {
        scene_map_editor->edit(object_cast<SceneMap>(p_object));
    }

    explicit SceneMapEditorPlugin(EditorNode *p_node);

    ~SceneMapEditorPlugin() override;

};

