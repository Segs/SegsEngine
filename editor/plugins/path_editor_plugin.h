/*************************************************************************/
/*  path_editor_plugin.h                                                 */
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

#include "editor/spatial_editor_gizmos.h"
#include "scene/3d/path.h"

class Separator;

class PathSpatialGizmo : public EditorSpatialGizmo {

    GDCLASS(PathSpatialGizmo,EditorSpatialGizmo)

    Path *path;
    mutable Vector3 original;
    mutable float orig_in_length;
    mutable float orig_out_length;

public:
    StringName get_handle_name(int p_idx) const override;
    Variant get_handle_value(int p_idx) override;
    void set_handle(int p_idx, Camera *p_camera, const Point2 &p_point) override;
    void commit_handle(int p_idx, const Variant &p_restore, bool p_cancel = false) override;

    void redraw() override;
    PathSpatialGizmo(Path *p_path = nullptr);
};

class PathSpatialGizmoPlugin : public EditorSpatialGizmoPlugin {

    GDCLASS(PathSpatialGizmoPlugin,EditorSpatialGizmoPlugin)

protected:
    Ref<EditorSpatialGizmo> create_gizmo(Spatial *p_spatial) override;

public:
    StringView get_name() const override;
    int get_priority() const override;
    PathSpatialGizmoPlugin();
};

class PathEditorPlugin : public EditorPlugin {

    GDCLASS(PathEditorPlugin,EditorPlugin)

    Separator *sep;
    ToolButton *curve_create;
    ToolButton *curve_edit;
    ToolButton *curve_del;
    ToolButton *curve_close;
    MenuButton *handle_menu;

    EditorNode *editor;

    Path *path;

    void _mode_changed(int p_idx);
    void _close_curve();
    void _handle_option_pressed(int p_option);
    bool handle_clicked;
    bool mirror_handle_angle;
    bool mirror_handle_length;

    enum HandleOption {
        HANDLE_OPTION_ANGLE,
        HANDLE_OPTION_LENGTH
    };

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    Path *get_edited_path() { return path; }

    static PathEditorPlugin *singleton;
    bool forward_spatial_gui_input(Camera *p_camera, const Ref<InputEvent> &p_event) override;

    //virtual bool forward_gui_input(const InputEvent& p_event) { return collision_polygon_editor->forward_gui_input(p_event); }
    //virtual Ref<SpatialEditorGizmo> create_spatial_gizmo(Spatial *p_spatial);
    StringView get_name() const override { return "Path"; }
    bool has_main_screen() const override { return false; }
    void edit(Object *p_object) override;
    bool handles(Object *p_object) const override;
    void make_visible(bool p_visible) override;

    bool mirror_angle_enabled() { return mirror_handle_angle; }
    bool mirror_length_enabled() { return mirror_handle_length; }
    bool is_handle_clicked() { return handle_clicked; }
    void set_handle_clicked(bool clicked) { handle_clicked = clicked; }

    PathEditorPlugin(EditorNode *p_node);
    ~PathEditorPlugin() override;
};
