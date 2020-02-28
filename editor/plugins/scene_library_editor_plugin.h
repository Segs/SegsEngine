/* http://www.segs.dev/
 * Copyright (c) 2006 - 2020 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License.
 * See LICENSE.md for details.
*/
#pragma once

#include "editor/editor_node.h"
#include "scene/resources/scene_library.h"

class SceneLibraryEditor : public Control {

    GDCLASS(SceneLibraryEditor,Control)

    Ref<SceneLibrary> m_scene_library;

    EditorNode *editor;
    MenuButton *menu;
    ConfirmationDialog *cd;
    EditorFileDialog *file;
    Viewport *m_preview_target;
    int to_erase;

    enum {

        MENU_OPTION_ADD_ITEM,
        MENU_OPTION_REMOVE_ITEM,
        MENU_OPTION_UPDATE_FROM_SCENE,
        MENU_OPTION_IMPORT_FROM_SCENE
    };

    int option;
    void _import_scene_cbk(StringView p_str);
    void _menu_cbk(int p_option);
    void _menu_confirm();
    void _preview_ready();

    static void _import_scene(Node *p_scene, const Ref<SceneLibrary> &p_library, bool p_merge);

protected:
    static void _bind_methods();

public:
    MenuButton *get_menu_button() const { return menu; }

    void edit(const Ref<SceneLibrary> &p_mesh_library);
    static Error update_library_file(Node *p_base_scene, const Ref<SceneLibrary>& ml, bool p_merge = true);

    SceneLibraryEditor(EditorNode *p_editor);
};

class SceneLibraryEditorPlugin : public EditorPlugin {

    GDCLASS(SceneLibraryEditorPlugin,EditorPlugin)

    SceneLibraryEditor *mesh_library_editor;
    EditorNode *editor;

public:
    StringView get_name() const override { return "SceneLibrary"; }
    bool has_main_screen() const override { return false; }
    void edit(Object *p_node) override;
    bool handles(Object *p_node) const override;
    void make_visible(bool p_visible) override;

    SceneLibraryEditorPlugin(EditorNode *p_node);
};
