/*************************************************************************/
/*  node_3d_editor_plugin.h                                              */
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

#include "editor/editor_plugin.h"
#include "editor/editor_scale.h"
#include "scene/3d/immediate_geometry_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/spin_box.h"
#include "scene/3d/skeleton_3d.h"
#include "core/string.h"


class Camera3D;
class Node3DEditor;
class EditorSpatialGizmoPlugin;
class ViewportContainer;
class Environment;
class AcceptDialog;
class CheckBox;
class Button;
class OptionButton;
class ConfirmationDialog;
class VSplitContainer;
class HSplitContainer;
class TextureRect;
class Node3DEditorViewport;

class GODOT_EXPORT EditorNode3DGizmo : public Node3DGizmo {

    GDCLASS(EditorNode3DGizmo,Node3DGizmo)

    bool selected;
    bool instanced;

public:
    void set_selected(bool p_selected) { selected = p_selected; }
    bool is_selected() const { return selected; }

    struct Instance {

        RenderingEntity instance = entt::null;
        Ref<Mesh> mesh;
        Ref<Material> material;
        Ref<SkinReference> skin_reference;
        RenderingEntity skeleton = entt::null;
        bool billboard=false;
        bool unscaled=false;
        bool can_intersect=false;
        bool extra_margin=false;
        void create_instance(Node3D *p_base, bool p_hidden = false);
    };
    struct Handle {
        Vector3 pos;
        bool billboard;
    };

    Vector<Vector3> collision_segments;
    Ref<TriangleMesh> collision_mesh;
    Vector<Vector3> handles;
    Vector<Vector3> secondary_handles;
    Vector<Instance> instances;
    Node3D *base;
    Node3D *spatial_node;
    EditorSpatialGizmoPlugin *gizmo_plugin;
    float selectable_icon_size;
    bool billboard_handle;
    bool valid;
    bool hidden;

protected:
    static void _bind_methods();

public:
    void add_lines(const Vector<Vector3> &p_lines, const Ref<Material> &p_material, bool p_billboard = false, const Color &p_modulate = Color(1, 1, 1));
    void add_vertices(Vector<Vector3> &&p_vertices, const Ref<Material> &p_material, Mesh::PrimitiveType p_primitive_type, bool p_billboard = false, const Color &p_modulate = Color(1, 1, 1));
    void add_mesh(const Ref<Mesh> &p_mesh, bool p_billboard = false, const Ref<SkinReference> &p_skin_reference = Ref<SkinReference>(), const Ref<Material> &p_material = Ref<Material>());
    void add_collision_segments(const Vector<Vector3> &p_lines);
    void add_collision_triangles(const Ref<TriangleMesh> &p_tmesh);
    void add_unscaled_billboard(const Ref<Material> &p_material, float p_scale = 1, const Color &p_modulate = Color(1, 1, 1));
    void add_handles(Vector<Vector3> &&p_handles, const Ref<Material> &p_material, bool p_billboard = false, bool p_secondary = false);
    void add_solid_box(Ref<Material> &p_material, Vector3 p_size, Vector3 p_position = Vector3());

    virtual bool is_handle_highlighted(int p_idx) const;
    virtual StringName get_handle_name(int p_idx) const;
    virtual Variant get_handle_value(int p_idx);
    virtual void set_handle(int p_idx, Camera3D *p_camera, const Point2 &p_point);
    virtual void commit_handle(int p_idx, const Variant &p_restore, bool p_cancel = false);

    void set_spatial_node(Node3D *p_node);
    Node3D *get_spatial_node() const { return spatial_node; }
    Ref<EditorSpatialGizmoPlugin> get_plugin() const { return Ref<EditorSpatialGizmoPlugin>(gizmo_plugin); }
    Vector3 get_handle_pos(int p_idx) const;
    bool intersect_frustum(const Camera3D *p_camera, Span<const Plane, 6> p_frustum);
    bool intersect_ray(Camera3D *p_camera, const Point2 &p_point, Vector3 &r_pos, Vector3 &r_normal, int *r_gizmo_handle = nullptr, bool p_sec_first = false);

    void clear() override;
    void create() override;
    void transform() override;
    void redraw() override;
    void free_gizmo() override;

    virtual bool is_editable() const;

    void set_hidden(bool p_hidden);
    void set_plugin(EditorSpatialGizmoPlugin *p_plugin);

    EditorNode3DGizmo();
    ~EditorNode3DGizmo() override;
};

class GODOT_EXPORT ViewportRotationControl : public Control {
    GDCLASS(ViewportRotationControl, Control)

    struct Axis2D {
        Vector2 screen_point;
        float z_axis = -99.0;
        int axis = -1;
    };

    struct Axis2DCompare {
        _FORCE_INLINE_ bool operator()(const Axis2D &l, const Axis2D &r) const {
            return l.z_axis < r.z_axis;
        }
    };

    Node3DEditorViewport *viewport = nullptr;
    Vector<Color> axis_colors;
    Vector<int> axis_menu_options;
    Vector2i orbiting_mouse_start;
    bool orbiting = false;
    int focused_axis = -2;

    //TODO: this fails if the editor scale can change at runtime.
    const float AXIS_CIRCLE_RADIUS = 8.0f * EDSCALE;

protected:
    static void _bind_methods();
    void _notification(int p_what);
    void _gui_input(Ref<InputEvent> p_event);
    void _draw();
    void _draw_axis(const Axis2D &p_axis);
    void _get_sorted_axis(Vector<Axis2D> &r_axis);
    void _update_focus();
    void _on_mouse_exited();

public:
    void set_viewport(Node3DEditorViewport *p_viewport);
};

class GODOT_EXPORT Node3DEditorViewport : public Control {

    GDCLASS(Node3DEditorViewport,Control)

    friend class Node3DEditor;
    friend class ViewportRotationControl;
    enum {

        VIEW_TOP,
        VIEW_BOTTOM,
        VIEW_LEFT,
        VIEW_RIGHT,
        VIEW_FRONT,
        VIEW_REAR,
        VIEW_CENTER_TO_ORIGIN,
        VIEW_CENTER_TO_SELECTION,
        VIEW_ALIGN_TRANSFORM_WITH_VIEW,
        VIEW_ALIGN_ROTATION_WITH_VIEW,
        VIEW_PERSPECTIVE,
        VIEW_ENVIRONMENT,
        VIEW_ORTHOGONAL,
        VIEW_HALF_RESOLUTION,
        VIEW_AUDIO_LISTENER,
        VIEW_AUDIO_DOPPLER,
        VIEW_GIZMOS,
        VIEW_INFORMATION,
        VIEW_FPS,
        VIEW_DISPLAY_NORMAL,
        VIEW_DISPLAY_WIREFRAME,
        VIEW_DISPLAY_OVERDRAW,
        VIEW_DISPLAY_SHADELESS,
        VIEW_DISPLAY_LIGHTING,
        VIEW_DISPLAY_NORMAL_BUFFER,
        VIEW_DISPLAY_DEBUG_SHADOW_ATLAS,
        VIEW_DISPLAY_DEBUG_DIRECTIONAL_SHADOW_ATLAS,
        VIEW_DISPLAY_DEBUG_GIPROBE_ALBEDO,
        VIEW_DISPLAY_DEBUG_GIPROBE_LIGHTING,
        VIEW_DISPLAY_DEBUG_GIPROBE_EMISSION,
        VIEW_DISPLAY_DEBUG_SCENE_LUMINANCE,
        VIEW_DISPLAY_DEBUG_SSAO,
        VIEW_DISPLAY_DEBUG_ROUGHNESS_LIMITER,
        VIEW_LOCK_ROTATION,
        VIEW_CINEMATIC_PREVIEW,
        VIEW_AUTO_ORTHOGONAL,
        VIEW_PORTAL_CULLING,
        VIEW_OCCLUSION_CULLING,
        VIEW_MAX
    };

    enum ViewType {
        VIEW_TYPE_USER,
        VIEW_TYPE_TOP,
        VIEW_TYPE_BOTTOM,
        VIEW_TYPE_LEFT,
        VIEW_TYPE_RIGHT,
        VIEW_TYPE_FRONT,
        VIEW_TYPE_REAR,
    };
public:
    enum {
        GIZMO_BASE_LAYER = 27,
        GIZMO_EDIT_LAYER = 26,
        GIZMO_GRID_LAYER = 25,
        MISC_TOOL_LAYER = 24
    };

    enum NavigationScheme {
        NAVIGATION_GODOT,
        NAVIGATION_MAYA,
        NAVIGATION_MODO,
    };

    enum FreelookNavigationScheme {
        FREELOOK_DEFAULT,
        FREELOOK_PARTIALLY_AXIS_LOCKED,
        FREELOOK_FULLY_AXIS_LOCKED,
    };
private:
    int index;
    bool _project_settings_change_pending;
    ViewType view_type;
    void _menu_option(int p_option);
    void _set_auto_orthogonal();
    Node3D *preview_node;
    AABB *preview_bounds;
    Vector<String> selected_files;
    AcceptDialog *accept;

    Node *target_node;
    Point2 drop_pos;

    EditorNode *editor;
    class EditorData *editor_data;
    EditorSelection *editor_selection;
    UndoRedo *undo_redo;

    CheckBox *preview_camera;
    ViewportContainer *subviewport_container;

    class MenuButton *view_menu;
    PopupMenu *display_submenu;

    Control *surface;
    Viewport *viewport;
    Camera3D *camera;
    bool transforming;
    bool orthogonal;
    bool auto_orthogonal;
    bool lock_rotation;
    float gizmo_scale;

    bool freelook_active;
    real_t freelook_speed;
    Vector2 previous_mouse_position;

    Label *info_label;
    Label *fps_label;
    Label *cinema_label;
    Label *locked_label;
    Label *zoom_limit_label;
    VBoxContainer *top_right_vbox;
    ViewportRotationControl *rotation_control;
    Gradient *frame_time_gradient;

    struct _RayResult {

        Node3D *item;
        float depth;
        int handle;
        _FORCE_INLINE_ bool operator<(const _RayResult &p_rr) const { return depth < p_rr.depth; }
    };

    void _update_name();
    void _compute_edit(const Point2 &p_point);
    void _clear_selected();
    void _select_clicked(bool p_append, bool p_single, bool p_allow_locked = false);
    void _select(Node *p_node, bool p_append, bool p_single);
    GameEntity _select_ray(const Point2 &p_pos, bool p_append, bool &r_includes_current, int *r_gizmo_handle = nullptr, bool p_alt_select = false);
    void _find_items_at_pos(const Point2 &p_pos, bool &r_includes_current, Vector<_RayResult> &results, bool p_alt_select = false);
    Vector3 _get_ray_pos(const Vector2 &p_pos) const;
    Vector3 _get_ray(const Vector2 &p_pos) const;
    Point2 _point_to_screen(const Vector3 &p_point);
    Transform _get_camera_transform() const;
    int get_selected_count() const;

    Vector3 _get_camera_position() const;
    Vector3 _get_camera_normal() const;
    Vector3 _get_screen_to_space(const Vector3 &p_vector3);

    void _select_region();
    bool _gizmo_select(const Vector2 &p_screenpos, bool p_highlight_only = false);

    void _nav_pan(Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative);
    void _nav_zoom(Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative);
    void _nav_orbit(const Ref<InputEventWithModifiers>& p_event, const Vector2 &p_relative);
    void _nav_look(const Ref<InputEventWithModifiers>& p_event, const Vector2 &p_relative);

    float get_znear() const;
    float get_zfar() const;
    float get_fov() const;

    GameEntity clicked;
    Vector<_RayResult> selection_results;
    bool clicked_includes_current;
    bool clicked_wants_append;
    bool selection_in_progress = false;

    PopupMenu *selection_menu;

    enum NavigationZoomStyle {
        NAVIGATION_ZOOM_VERTICAL,
        NAVIGATION_ZOOM_HORIZONTAL
    };

    enum NavigationMode {
        NAVIGATION_NONE,
        NAVIGATION_PAN,
        NAVIGATION_ZOOM,
        NAVIGATION_ORBIT,
        NAVIGATION_LOOK
    };
    enum TransformMode {
        TRANSFORM_NONE,
        TRANSFORM_ROTATE,
        TRANSFORM_TRANSLATE,
        TRANSFORM_SCALE

    };
    enum TransformPlane {
        TRANSFORM_VIEW,
        TRANSFORM_X_AXIS,
        TRANSFORM_Y_AXIS,
        TRANSFORM_Z_AXIS,
        TRANSFORM_YZ,
        TRANSFORM_XZ,
        TRANSFORM_XY,
    };

    struct EditData {
        TransformMode mode;
        TransformPlane plane;
        Transform original;
        Vector3 click_ray;
        Vector3 click_ray_pos;
        Vector3 center;
        Vector3 orig_gizmo_pos;
        int edited_gizmo;
        Point2 mouse_pos;
        Point2 original_mouse_pos;
        bool snap;
        Ref<EditorNode3DGizmo> gizmo;
        int gizmo_handle;
        Variant gizmo_initial_value;
        Vector3 gizmo_initial_pos;
    } _edit;

    struct Cursor {

        Vector3 pos;
        float x_rot, y_rot, distance;
        float fov_scale=1.0f;
        Vector3 eye_pos; // Used in freelook mode
        bool region_select;
        Point2 region_begin, region_end;

        Cursor() {
            x_rot = 0.5;
            y_rot = -0.5;
            distance = 4;
            region_select = false;
        }
    };
    // Viewport camera supports movement smoothing,
    // so one cursor is the real cursor, while the other can be an interpolated version.
    Cursor cursor; // Immediate cursor
    Cursor camera_cursor; // That one may be interpolated (don't modify this one except for smoothing purposes)

    void scale_fov(real_t p_fov_offset);
    void reset_fov();
    void scale_cursor_distance(real_t scale);

    void set_freelook_active(bool active_now);
    void scale_freelook_speed(real_t scale);

    real_t zoom_indicator_delay;
    int zoom_failed_attempts_count = 0;

    RenderingEntity move_gizmo_instance[3],
            move_plane_gizmo_instance[3],
            rotate_gizmo_instance[4],
            scale_gizmo_instance[3],
            scale_plane_gizmo_instance[3];

    StringName last_message;
    StringName message;
    float message_time;

    void set_message(StringName p_message, float p_time = 5);

    void _view_settings_confirmed(float p_interp_delta);
    void _update_camera(float p_interp_delta);
    Transform to_camera_transform(const Cursor &p_cursor) const;
    void _draw();

    void _surface_mouse_enter();
    void _surface_mouse_exit();
    void _surface_focus_enter();
    void _surface_focus_exit();

    void _sinput(const Ref<InputEvent> &p_event);
    void _update_freelook(real_t delta);
    Node3DEditor *spatial_editor;

    Camera3D *previewing;
    Camera3D *preview;

    bool previewing_cinema;
    bool _is_node_locked(const Node *p_node);
    void _preview_exited_scene();
    void _toggle_camera_preview(bool);
    void _toggle_cinema_preview(bool);
    void _init_gizmo_instance(int p_idx);
    void _finish_gizmo_instances();
    void _selection_result_pressed(int);
    void _selection_menu_hide();
    void _list_select(Ref<InputEventMouseButton> b);
    bool handle_mouse_button(Ref<InputEventMouseButton> b);
    bool handle_mouse_motion(Ref<InputEventMouseMotion> m);
    bool handle_key_input(const Ref<InputEvent> &p_event, Ref<InputEventKey> k);
    Point2i _get_warped_mouse_motion(const Ref<InputEventMouseMotion> &p_ev_mouse_motion) const;

    Vector3 _get_instance_position(const Point2 &p_pos) const;
    static AABB _calculate_spatial_bounds(const Node3D *p_parent, bool p_exclude_toplevel_transform = true);
    Node *_sanitize_preview_node(Node *p_node) const;
    void _create_preview(const Vector<String> &files) const;
    void _remove_preview();
    bool _cyclical_dependency_exists(StringView p_target_scene_path, Node *p_desired_node);
    bool _create_instance(Node *parent, StringView path, const Point2 &p_point);
    void _perform_drop_data();

    bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
    void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
    void _project_settings_changed();

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    void update_surface() { surface->update(); }
    void update_transform_gizmo_view();

    void set_can_preview(Camera3D *p_preview);
    void set_state(const Dictionary &p_state);
    Dictionary get_state() const;
    void reset();
    bool is_freelook_active() const { return freelook_active; }

    void focus_selection();

    void assign_pending_data_pointers(
            Node3D *p_preview_node,
            AABB *p_preview_bounds,
            AcceptDialog *p_accept);

    Viewport *get_viewport_node() { return viewport; }
    Camera3D *get_camera() { return camera; } // return the default camera object.

    Node3DEditorViewport(Node3DEditor *p_spatial_editor, EditorNode *p_editor, int p_index);
    ~Node3DEditorViewport();
};

class Node3DEditorSelectedItem : public Object {

    GDCLASS(Node3DEditorSelectedItem,Object)

public:
    AABB aabb;
    Transform original; // original location when moving
    Transform original_local;
    Transform last_xform; // last transform
    Node3D *sp = nullptr;
    RenderingEntity sbox_instance=entt::null;
    RenderingEntity sbox_instance_offset=entt::null;
    RenderingEntity sbox_instance_xray=entt::null;
    RenderingEntity sbox_instance_xray_offset=entt::null;

    bool last_xform_dirty =  true;

    Node3DEditorSelectedItem() = default;
    ~Node3DEditorSelectedItem() override;
};

class GODOT_EXPORT SpatialEditorViewportContainer : public Container {

    GDCLASS(SpatialEditorViewportContainer,Container)

public:
    enum View {
        VIEW_USE_1_VIEWPORT,
        VIEW_USE_2_VIEWPORTS,
        VIEW_USE_2_VIEWPORTS_ALT,
        VIEW_USE_3_VIEWPORTS,
        VIEW_USE_3_VIEWPORTS_ALT,
        VIEW_USE_4_VIEWPORTS,
    };

private:
    View view;
    bool mouseover;
    float ratio_h;
    float ratio_v;

    bool hovering_v;
    bool hovering_h;

    bool dragging_v;
    bool dragging_h;
    Vector2 drag_begin_pos;
    Vector2 drag_begin_ratio;

    void _gui_input(const Ref<InputEvent> &p_event);

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    void set_view(View p_view);
    View get_view();

    SpatialEditorViewportContainer();
};

class GODOT_EXPORT Node3DEditor : public VBoxContainer {

    GDCLASS(Node3DEditor,VBoxContainer)

public:
    static const unsigned int VIEWPORTS_COUNT = 4;

    enum ToolMode {

        TOOL_MODE_SELECT,
        TOOL_MODE_MOVE,
        TOOL_MODE_ROTATE,
        TOOL_MODE_SCALE,
        TOOL_MODE_LIST_SELECT,
        TOOL_LOCK_SELECTED,
        TOOL_UNLOCK_SELECTED,
        TOOL_GROUP_SELECTED,
        TOOL_UNGROUP_SELECTED,
        TOOL_CONVERT_ROOMS,
        TOOL_MAX
    };

    enum ToolOptions {

        TOOL_OPT_LOCAL_COORDS,
        TOOL_OPT_USE_SNAP,
        TOOL_OPT_OVERRIDE_CAMERA,
        TOOL_OPT_MAX

    };

private:
    EditorNode *editor;
    EditorSelection *editor_selection;

    SpatialEditorViewportContainer *viewport_base;
    Node3DEditorViewport *viewports[VIEWPORTS_COUNT];
    VSplitContainer *shader_split;
    HSplitContainer *left_panel_split;
    HSplitContainer *right_panel_split;

    /////

    ToolMode tool_mode;

    RS::ScenarioDebugMode scenario_debug;

    RenderingEntity origin;
    RenderingEntity origin_instance;
    RenderingEntity grid[3];
    RenderingEntity grid_instance[3];
    bool grid_visible[3]; //currently visible
    bool grid_enable[3]; //should be always visible if true
    bool grid_enabled;
    bool origin_enabled;

    Ref<ArrayMesh> move_gizmo[3], move_plane_gizmo[3], rotate_gizmo[4], scale_gizmo[3], scale_plane_gizmo[3];
    Ref<SpatialMaterial> gizmo_color[3];
    Ref<SpatialMaterial> plane_gizmo_color[3];
    Ref<SpatialMaterial> gizmo_color_hl[3];
    Ref<SpatialMaterial> plane_gizmo_color_hl[3];
    Ref<ShaderMaterial> rotate_gizmo_color[3];
    Ref<ShaderMaterial> rotate_gizmo_color_hl[3];

    int over_gizmo_handle;
    float snap_translate_value;
    float snap_rotate_value;
    float snap_scale_value;

    Ref<ArrayMesh> selection_box_xray;
    Ref<ArrayMesh> selection_box;
    RenderingEntity indicators;
    RenderingEntity indicators_instance;
    RenderingEntity cursor_mesh;
    RenderingEntity cursor_instance;
    Ref<SpatialMaterial> indicator_mat;
    Ref<ShaderMaterial> grid_mat[3];
    Ref<SpatialMaterial> cursor_material;

    // Scene drag and drop support
    Node3D *preview_node;
    AABB preview_bounds;

    struct Gizmo {

        bool visible;
        float scale;
        Transform transform;
    } gizmo;

    enum MenuOption {

        MENU_TOOL_SELECT,
        MENU_TOOL_MOVE,
        MENU_TOOL_ROTATE,
        MENU_TOOL_SCALE,
        MENU_TOOL_LIST_SELECT,
        MENU_TOOL_LOCAL_COORDS,
        MENU_TOOL_USE_SNAP,
        MENU_TOOL_OVERRIDE_CAMERA,
        MENU_TOOL_CONVERT_ROOMS,
        MENU_TRANSFORM_CONFIGURE_SNAP,
        MENU_TRANSFORM_DIALOG,
        MENU_VIEW_USE_1_VIEWPORT,
        MENU_VIEW_USE_2_VIEWPORTS,
        MENU_VIEW_USE_2_VIEWPORTS_ALT,
        MENU_VIEW_USE_3_VIEWPORTS,
        MENU_VIEW_USE_3_VIEWPORTS_ALT,
        MENU_VIEW_USE_4_VIEWPORTS,
        MENU_VIEW_ORIGIN,
        MENU_VIEW_GRID,
        MENU_VIEW_PORTAL_CULLING,
        MENU_VIEW_OCCLUSION_CULLING,
        MENU_VIEW_GIZMOS_3D_ICONS,
        MENU_VIEW_CAMERA_SETTINGS,
        MENU_LOCK_SELECTED,
        MENU_UNLOCK_SELECTED,
        MENU_GROUP_SELECTED,
        MENU_UNGROUP_SELECTED,
        MENU_SNAP_TO_FLOOR,
        MENU_OPT_MAX
    };

    Button *tool_button[TOOL_MAX];
    Button *tool_option_button[TOOL_OPT_MAX];

    MenuButton *transform_menu;
    PopupMenu *gizmos_menu;
    MenuButton *view_menu = nullptr;

    AcceptDialog *accept;

    ConfirmationDialog *snap_dialog;
    ConfirmationDialog *xform_dialog;
    ConfirmationDialog *settings_dialog;

    bool snap_enabled;
    bool snap_key_enabled;
    LineEdit *snap_translate;
    LineEdit *snap_rotate;
    LineEdit *snap_scale;
    PanelContainer *menu_panel;

    LineEdit *xform_translate[3];
    LineEdit *xform_rotate[3];
    LineEdit *xform_scale[3];
    OptionButton *xform_type;

    VBoxContainer *settings_vbc;
    SpinBox *settings_fov;
    SpinBox *settings_znear;
    SpinBox *settings_zfar;

    void _snap_changed();
    void _snap_update();
    void _xform_dialog_action();
    void _menu_item_pressed(int p_option);
    void _menu_item_toggled(bool pressed, int p_option);
    void _menu_gizmo_toggled(int p_option);
    void _update_camera_override_button(bool p_game_running);
    void _update_camera_override_viewport(Object *p_viewport);

    HBoxContainer *hbc_menu;
    // Used for secondary menu items which are displayed depending on the currently selected node
    // (such as MeshInstance's "Mesh" menu).
    PanelContainer *context_menu_container;
    HBoxContainer *hbc_context_menu;

    void _generate_selection_boxes();
    UndoRedo *undo_redo;

    int camera_override_viewport_id;

    void _init_indicators();
    void _update_context_menu_stylebox();
    void _update_gizmos_menu();
    void _update_gizmos_menu_theme();
    void _init_grid();
    void _finish_indicators();
    void _finish_grid();

    void _toggle_maximize_view(Object *p_viewport);

    Node *custom_camera;

    Object *_get_editor_data(Object *p_what);
    //Color _get_axis_color(int axis);

    Ref<Environment> viewport_environment;

    Node3D *selected;

    void _request_gizmo(Object *p_obj);

    static Node3DEditor *singleton;

    void _node_removed(Node *p_node);
    Vector<Ref<EditorSpatialGizmoPlugin> > gizmo_plugins_by_priority;
    Vector<Ref<EditorSpatialGizmoPlugin> > gizmo_plugins_by_name;

    void _register_all_gizmos();

    Node3DEditor()=delete;

    bool is_any_freelook_active() const;

    void _refresh_menu_icons();

protected:
    void _notification(int p_what);
    //void _gui_input(InputEvent p_event);
    void _unhandled_key_input(const Ref<InputEvent>& p_event);

    static void _bind_methods();

public:
    static Node3DEditor *get_singleton() { return singleton; }
    void snap_cursor_to_plane(const Plane &p_plane);

    Vector3 snap_point(Vector3 p_target, Vector3 p_start = Vector3(0, 0, 0)) const;

    float get_znear() const { return float(settings_znear->get_value()); }
    float get_zfar() const { return float(settings_zfar->get_value()); }
    float get_fov() const { return float(settings_fov->get_value()); }

    Transform get_gizmo_transform() const { return gizmo.transform; }
    bool is_gizmo_visible() const { return gizmo.visible; }

    ToolMode get_tool_mode() const { return tool_mode; }
    bool are_local_coords_enabled() const;
    bool is_snap_enabled() const { return snap_enabled ^ snap_key_enabled; }
    float get_translate_snap() const;
    float get_rotate_snap() const;
    float get_scale_snap() const;

    Ref<ArrayMesh> get_move_gizmo(int idx) const { return move_gizmo[idx]; }
    Ref<ArrayMesh> get_move_plane_gizmo(int idx) const { return move_plane_gizmo[idx]; }
    Ref<ArrayMesh> get_rotate_gizmo(int idx) const { return rotate_gizmo[idx]; }
    Ref<ArrayMesh> get_scale_gizmo(int idx) const { return scale_gizmo[idx]; }
    Ref<ArrayMesh> get_scale_plane_gizmo(int idx) const { return scale_plane_gizmo[idx]; }

    void update_grid();
    void update_transform_gizmo();
    void update_portal_tools();
    void show_advanced_portal_tools(bool p_show);
    void update_all_gizmos(Node *p_node = nullptr);
    void snap_selected_nodes_to_floor();
    void select_gizmo_highlight_axis(int p_axis);
    void set_custom_camera(Node *p_camera) { custom_camera = p_camera; }

    void set_undo_redo(UndoRedo *p_undo_redo) { undo_redo = p_undo_redo; }
    Dictionary get_state() const;
    void set_state(const Dictionary &p_state);

    const Ref<Environment> &get_viewport_environment() { return viewport_environment; }

    UndoRedo *get_undo_redo() { return undo_redo; }

    void add_control_to_menu_panel(Control *p_control);
    void remove_control_from_menu_panel(Control *p_control);

    void add_control_to_left_panel(Control *p_control);
    void remove_control_from_left_panel(Control *p_control);

    void add_control_to_right_panel(Control *p_control);
    void remove_control_from_right_panel(Control *p_control);

    void move_control_to_left_panel(Control *p_control);
    void move_control_to_right_panel(Control *p_control);
    VSplitContainer *get_shader_split();

    Node3D *get_selected() { return selected; }

    int get_over_gizmo_handle() const { return over_gizmo_handle; }
    void set_over_gizmo_handle(int idx) { over_gizmo_handle = idx; }

    void set_can_preview(Camera3D *p_preview);
    void set_message(StringView p_message, float p_time = 5);

    Node3DEditorViewport *get_editor_viewport(int p_idx) {
        ERR_FAIL_INDEX_V(p_idx, static_cast<int>(VIEWPORTS_COUNT), nullptr);
        return viewports[p_idx];
    }

    void add_gizmo_plugin(Ref<EditorSpatialGizmoPlugin> p_plugin);
    void remove_gizmo_plugin(const Ref<EditorSpatialGizmoPlugin>& p_plugin);

    void edit(Node3D *p_spatial);
    void clear();

    Node3DEditor(EditorNode *p_editor);
    ~Node3DEditor() override;
};

class GODOT_EXPORT Node3DEditorPlugin : public EditorPlugin {

    GDCLASS(Node3DEditorPlugin,EditorPlugin)

    Node3DEditor *spatial_editor;
    EditorNode *editor;

protected:
    static void _bind_methods();

public:
    void snap_cursor_to_plane(const Plane &p_plane);

    Node3DEditor *get_spatial_editor() { return spatial_editor; }
    StringView get_name() const override { return "3D"; }
    bool has_main_screen() const override { return true; }
    void make_visible(bool p_visible) override;
    void edit(Object *p_object) override;
    bool handles(Object *p_object) const override;

    Dictionary get_state() const override;
    void set_state(const Dictionary &p_state) override;
    void clear() override { spatial_editor->clear(); }

    void edited_scene_changed() override;

    Node3DEditorPlugin(EditorNode *p_node);
    ~Node3DEditorPlugin() override;
};

class GODOT_EXPORT EditorSpatialGizmoPlugin : public Resource {

    GDCLASS(EditorSpatialGizmoPlugin,Resource)

public:
    static const int VISIBLE = 0;
    static const int HIDDEN = 1;
    static const int ON_TOP = 2;

protected:
    int current_state;
    Vector<EditorNode3DGizmo *> current_gizmos;
    HashMap<String, Vector<Ref<SpatialMaterial> > > materials;

    static void _bind_methods();
    virtual bool has_gizmo(Node3D *p_spatial);
    virtual Ref<EditorNode3DGizmo> create_gizmo(Node3D *p_spatial);

public:
    void create_material(StringView p_name, const Color &p_color, bool p_billboard = false, bool p_on_top = false, bool p_use_vertex_color = false);
    void create_icon_material(const String &p_name, const Ref<Texture> &p_texture, bool p_on_top = false, const Color &p_albedo = Color(1, 1, 1, 1));
    void create_handle_material(const String &p_name, bool p_billboard = false, const Ref<Texture> &p_icon = {});
    void add_material(const String &p_name, const Ref<SpatialMaterial>& p_material);

    Ref<SpatialMaterial> get_material(const String &p_name, EditorNode3DGizmo *p_gizmo = nullptr);

    virtual StringView get_name() const;
    virtual int get_priority() const;
    virtual bool can_be_hidden() const;
    virtual bool is_selectable_when_hidden() const;

    virtual void redraw(EditorNode3DGizmo *p_gizmo);
    virtual StringName get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_idx) const;
    virtual Variant get_handle_value(EditorNode3DGizmo *p_gizmo, int p_idx) const;
    virtual void set_handle(EditorNode3DGizmo *p_gizmo, int p_idx, Camera3D *p_camera, const Point2 &p_point);
    virtual void commit_handle(EditorNode3DGizmo *p_gizmo, int p_idx, const Variant &p_restore, bool p_cancel = false);
    virtual bool is_handle_highlighted(const EditorNode3DGizmo *p_gizmo, int p_idx) const;

    Ref<EditorNode3DGizmo> get_gizmo(Node3D *p_spatial);
    void set_state(int p_state);
    int get_state() const;
    void unregister_gizmo(EditorNode3DGizmo *p_gizmo);

    EditorSpatialGizmoPlugin();
    ~EditorSpatialGizmoPlugin() override;
};
