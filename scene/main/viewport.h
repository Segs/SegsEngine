/*************************************************************************/
/*  viewport.h                                                           */
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

#include "core/math/transform.h"
#include "core/math/transform_2d.h"
#include "core/node_path.h"
#include "scene/main/node.h"
#include "scene/resources/texture.h"
#include "scene/resources/world_2d.h"

#include "core/deque.h"

class Camera3D;
class Camera2D;
class Listener3D;
class Control;
class CanvasItem;
class CanvasLayer;
class Panel;
class World3D;
class Label;
class Timer;
class Viewport;
class CollisionObject3D;
class SceneTreeTimer;

class GODOT_EXPORT ViewportTexture : public Texture {

    GDCLASS(ViewportTexture,Texture)

    NodePath path;

    friend class Viewport;
    Viewport *vp = nullptr;
    uint32_t flags = 0;

    RenderingEntity proxy;

protected:
    static void _bind_methods();

public:
    void set_viewport_path_in_scene(const NodePath &p_path);
    NodePath get_viewport_path_in_scene() const;

    void setup_local_to_scene() override;

    int get_width() const override;
    int get_height() const override;
    Size2 get_size() const override;
    RenderingEntity get_rid() const override;

    bool has_alpha() const override;

    void set_flags(uint32_t p_flags) override;
    uint32_t get_flags() const override;

    Ref<Image> get_data() const override;

    ViewportTexture();
    ~ViewportTexture() override;
};

class GODOT_EXPORT Viewport : public Node {

    GDCLASS(Viewport,Node)

public:
    enum UpdateMode : uint8_t {
        UPDATE_DISABLED,
        UPDATE_ONCE, //then goes to disabled
        UPDATE_WHEN_VISIBLE, // default
        UPDATE_ALWAYS
    };

    enum ShadowAtlasQuadrantSubdiv : uint8_t {
        SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED,
        SHADOW_ATLAS_QUADRANT_SUBDIV_1,
        SHADOW_ATLAS_QUADRANT_SUBDIV_4,
        SHADOW_ATLAS_QUADRANT_SUBDIV_16,
        SHADOW_ATLAS_QUADRANT_SUBDIV_64,
        SHADOW_ATLAS_QUADRANT_SUBDIV_256,
        SHADOW_ATLAS_QUADRANT_SUBDIV_1024,
        SHADOW_ATLAS_QUADRANT_SUBDIV_MAX,

    };

    enum MSAA : uint8_t {
        MSAA_DISABLED=0,
        MSAA_2X,
        MSAA_4X,
        MSAA_8X,
        MSAA_16X,
        MSAA_COUNT
    };

    enum Usage: uint8_t {
        USAGE_2D,
        USAGE_2D_NO_SAMPLING,
        USAGE_3D,
        USAGE_3D_NO_EFFECTS,
    };

    enum RenderInfo: uint8_t {

        RENDER_INFO_OBJECTS_IN_FRAME,
        RENDER_INFO_VERTICES_IN_FRAME,
        RENDER_INFO_MATERIAL_CHANGES_IN_FRAME,
        RENDER_INFO_SHADER_CHANGES_IN_FRAME,
        RENDER_INFO_SURFACE_CHANGES_IN_FRAME,
        RENDER_INFO_DRAW_CALLS_IN_FRAME,
        RENDER_INFO_2D_ITEMS_IN_FRAME,
        RENDER_INFO_2D_DRAW_CALLS_IN_FRAME,
        RENDER_INFO_MAX
    };

    enum DebugDraw: uint8_t {
        DEBUG_DRAW_DISABLED,
        DEBUG_DRAW_UNSHADED,
        DEBUG_DRAW_OVERDRAW,
        DEBUG_DRAW_WIREFRAME,
    };

    enum ClearMode: uint8_t {

        CLEAR_MODE_ALWAYS,
        CLEAR_MODE_NEVER,
        CLEAR_MODE_ONLY_NEXT_FRAME
    };

private:
    friend class ViewportTexture;

    Viewport *parent = nullptr;
    Listener3D *listener = nullptr;
    HashSet<Listener3D *> listeners;

    bool arvr = false;
    struct CameraOverrideData {
        enum Projection : uint8_t {
            PROJECTION_PERSPECTIVE,
            PROJECTION_ORTHOGONAL
        };
        Transform transform;
        RenderingEntity rid = entt::null;
        float fov;
        float size;
        float z_near;
        float z_far;
        Projection projection;

        operator bool() const {
            return rid != entt::null;
        }
    } camera_override;

    Camera3D *camera = nullptr;
    HashSet<Camera3D *> cameras;
    HashSet<CanvasLayer *> canvas_layers;

    RenderingEntity viewport = entt::null;
    RenderingEntity current_canvas = entt::null;
    RID internal_listener;

    RID internal_listener_2d;

    bool audio_listener = false;
    bool audio_listener_2d = false;
    bool override_canvas_transform = false;

    Transform2D canvas_transform_override;
    Transform2D canvas_transform;
    Transform2D global_canvas_transform;
    Transform2D stretch_transform;

    Size2 size;
    Rect2 to_screen_rect;

    RenderingEntity contact_2d_debug = entt::null;
    RenderingEntity contact_3d_debug_multimesh = entt::null;
    RenderingEntity contact_3d_debug_instance = entt::null;

    Size2 size_override_size;
    Size2 size_override_margin;

    Rect2 last_vp_rect;

    ClearMode clear_mode;
    Dequeue<Ref<InputEvent> > physics_picking_events;
    GameEntity physics_object_capture = entt::null;
    GameEntity physics_object_over = entt::null;
    Transform physics_last_object_transform;
    Transform physics_last_camera_transform;
    GameEntity physics_last_id;
    Vector2 physics_last_mousepos;
    bool size_override = false;
    bool size_override_stretch = false;
    bool transparent_bg = false;
    bool vflip = false;
    bool filter;
    bool gen_mipmaps = false;
    bool snap_controls_to_pixels;
    bool physics_object_picking = false;
    bool physics_has_last_mousepos = false;
    struct {

        int mouse_mask;
        uint8_t alt : 1;
        uint8_t control : 1;
        uint8_t shift : 1;
        uint8_t meta : 1;

    } physics_last_mouse_state;

    void _collision_object_input_event(CollisionObject3D *p_object, Camera3D *p_camera, const Ref<InputEvent> &p_input_event, const Vector3 &p_pos, const Vector3 &p_normal, int p_shape);

    bool handle_input_locally;
    bool local_input_handled;

    HashMap<GameEntity, uint64_t> physics_2d_mouseover;

    Ref<World2D> world_2d;
    Ref<World3D> world;
    Ref<World3D> own_world;

    StringName input_group;
    StringName gui_input_group;
    StringName unhandled_input_group;
    StringName unhandled_key_input_group;

    void _update_listener();
    void _update_listener_2d();

    void _propagate_enter_world(Node *p_node);
    void _propagate_exit_world(Node *p_node);
    void _propagate_viewport_notification(Node *p_node, int p_what);

    void _update_stretch_transform();
    void _update_global_transform();

    bool disable_3d;
    bool keep_3d_linear;
    UpdateMode update_mode = UPDATE_WHEN_VISIBLE;
    RenderingEntity texture_rid = entt::null;
    uint32_t texture_flags = 0;

    DebugDraw debug_draw;

    Usage usage;

    float sharpen_intensity = 0.0f;
    int shadow_atlas_size = 0;
    ShadowAtlasQuadrantSubdiv shadow_atlas_quadrant_subdiv[4];

    MSAA msaa;
    bool use_fxaa = false;
    bool use_debanding = false;
    bool hdr = true;
    bool use_32_bpc_depth = false;

    Ref<ViewportTexture> default_texture;
    HashSet<ViewportTexture *> viewport_textures;

    struct GUI {
        // info used when this is a window
        Dequeue<Control *> roots;
        Dequeue<Control *> modal_stack;
        Dequeue<Control *> subwindows; // visible subwindows
        Dequeue<Control *> all_known_subwindows;
        Ref<SceneTreeTimer> tooltip_timer;
        Point2 tooltip_pos;
        Point2 last_mouse_pos;
        Point2 drag_accum;
        Control *mouse_focus = nullptr;
        Control *last_mouse_focus;
        Control *mouse_click_grabber = nullptr;
        Control *key_focus = nullptr;
        Control *mouse_over = nullptr;
        Control *tooltip_control = nullptr;
        Control *tooltip_popup = nullptr;
        Label *tooltip_label = nullptr;
        Variant drag_data;
        GameEntity drag_preview_id;
        Transform2D focus_inv_xform;
        int mouse_focus_mask = 0;
        float tooltip_delay;
        int canvas_sort_index; //for sorting items with canvas as root
        bool key_event_accepted;
        bool drag_attempted;
        bool subwindow_order_dirty = false;
        bool subwindow_visibility_dirty = false;
        bool roots_order_dirty;
        bool dragging = false;
        bool drag_successful=false;

        GUI();
    } gui;

    bool disable_input;

    void _gui_call_input(Control *p_control, const Ref<InputEvent> &p_input);
    void _gui_call_notification(Control *p_control, int p_what);

    void _gui_prepare_subwindows();
    void _gui_sort_subwindows();
    void _gui_sort_roots();
    void _gui_sort_modal_stack();
    Control *_gui_find_control(const Point2 &p_global);
    Control *_gui_find_control_at_pos(CanvasItem *p_node, const Point2 &p_global, const Transform2D &p_xform, Transform2D &r_inv_xform);

    void _gui_input_event(Ref<InputEvent> p_event);
    void _gui_cleanup_internal_state(Ref<InputEvent> p_event);
public:
    void update_worlds();
protected:
    _FORCE_INLINE_ Transform2D _get_input_pre_xform() const;

    void _vp_input(const Ref<InputEvent> &p_ev);
    void _vp_input_text(StringView p_text);
    void _vp_unhandled_input(const Ref<InputEvent> &p_ev);
    Ref<InputEvent> _make_input_local(const Ref<InputEvent> &ev);

    friend class Control;

    void _gui_add_root_control(Control *p_control);
    void _gui_add_subwindow_control(Control *p_control);

    void _gui_set_subwindow_order_dirty();
    void _gui_set_root_order_dirty();

    void _gui_remove_modal_control(Control* MI);
    void _gui_remove_from_modal_stack(Control *MI, GameEntity p_prev_focus_owner);
    void _gui_remove_root_control(Control *RI);
    void _gui_remove_subwindow_control(Control *SI);

    String _gui_get_tooltip(Control *p_control, const Vector2 &p_pos, Control **r_tooltip_owner = nullptr);
    void _gui_cancel_tooltip();
    void _gui_show_tooltip();

    void _gui_remove_control(Control *p_control);
    void _gui_hid_control(Control *p_control);

    void _gui_force_drag(Control *p_base, const Variant &p_data, Control *p_control);
    void _gui_set_drag_preview(Control *p_base, Control *p_control);
    Control *_gui_get_drag_preview();

    bool _gui_is_modal_on_top(const Control *p_control);
    void _gui_show_modal(Control *p_control);

    void _gui_remove_focus();
    void _gui_unfocus_control(Control *p_control);
    bool _gui_control_has_focus(const Control *p_control);
    void _gui_control_grab_focus(Control *p_control);
    void _gui_grab_click_focus(Control *p_control);
    void _post_gui_grab_click_focus();
    void _gui_accept_event();

    Control *_gui_get_focus_owner();

    Vector2 _get_window_offset() const;

    bool _gui_drop(Control *p_at_control, Point2 p_at_pos, bool p_just_check);

    friend class Listener3D;
    void _listener_transform_changed_notify();
    void _listener_set(Listener3D *p_listener);
    bool _listener_add(Listener3D *p_listener); //true if first
    void _listener_remove(Listener3D *p_listener);
    void _listener_make_next_current(Listener3D *p_exclude);

    friend class Camera3D;
    void _camera_transform_changed_notify();
    void _camera_set(Camera3D *p_camera);
    bool _camera_add(Camera3D *p_camera); //true if first
    void _camera_remove(Camera3D *p_camera);
    void _camera_make_next_current(Camera3D *p_exclude);

    friend class CanvasLayer;
    void _canvas_layer_add(CanvasLayer *p_canvas_layer);
    void _canvas_layer_remove(CanvasLayer *p_canvas_layer);

    void _drop_mouse_focus();
    void _drop_mouse_over();
    void _drop_physics_mouseover(bool p_paused_only = false);

    void _update_canvas_items(Node *p_node);

    void _own_world_changed();
protected:
    void _notification(int p_what);
    void _process_picking(bool p_ignore_paused);
    static void _bind_methods();
    void _validate_property(PropertyInfo &property) const override;

public:
    Listener3D *get_listener() const;
    Camera3D *get_camera() const;

    void enable_camera_override(bool p_enable);
    bool is_camera_override_enabled() const;

    void set_camera_override_transform(const Transform &p_transform);
    Transform get_camera_override_transform() const;

    void set_camera_override_perspective(float p_fovy_degrees, float p_z_near, float p_z_far);
    void set_camera_override_orthogonal(float p_size, float p_z_near, float p_z_far);

    void set_use_arvr(bool p_use_arvr);
    bool use_arvr();

    void set_as_audio_listener(bool p_enable);
    bool is_audio_listener() const;

    void set_as_audio_listener_2d(bool p_enable);
    bool is_audio_listener_2d() const;

    void set_size(const Size2 &p_size);
    void update_canvas_items();

    Size2 get_size() const;
    Rect2 get_visible_rect() const;
    RenderingEntity get_viewport_rid() const;

    void set_world_3d(const Ref<World3D> &p_world);
    void set_world_2d(const Ref<World2D> &p_world_2d);
    Ref<World3D> get_world_3d() const;
    Ref<World3D> find_world_3d() const;

    Ref<World2D> get_world_2d() const;
    Ref<World2D> find_world_2d() const;

    void enable_canvas_transform_override(bool p_enable);
    bool is_canvas_transform_override_enabled() const;

    void set_canvas_transform_override(const Transform2D &p_transform);
    Transform2D get_canvas_transform_override() const;

    void set_canvas_transform(const Transform2D &p_transform);
    Transform2D get_canvas_transform() const;

    void set_global_canvas_transform(const Transform2D &p_transform);
    Transform2D get_global_canvas_transform() const;

    Transform2D get_final_transform() const;

    void set_transparent_background(bool p_enable);
    bool has_transparent_background() const;

    void set_size_override(bool p_enable, const Size2 &p_size = Size2(-1, -1), const Vector2 &p_margin = Vector2());
    Size2 get_size_override() const;

    bool is_size_override_enabled() const;
    void set_size_override_stretch(bool p_enable);
    bool is_size_override_stretch_enabled() const;

    void set_vflip(bool p_enable);
    bool get_vflip() const;

    void set_clear_mode(ClearMode p_mode);
    ClearMode get_clear_mode() const;

    void set_update_mode(UpdateMode p_mode);
    UpdateMode get_update_mode() const;
    Ref<ViewportTexture> get_texture() const;

    void set_shadow_atlas_size(int p_size);
    int get_shadow_atlas_size() const;

    void set_shadow_atlas_quadrant_subdiv(int p_quadrant, ShadowAtlasQuadrantSubdiv p_subdiv);
    ShadowAtlasQuadrantSubdiv get_shadow_atlas_quadrant_subdiv(int p_quadrant) const;

    void set_msaa(MSAA p_msaa);
    MSAA get_msaa() const;

    void set_use_fxaa(bool p_fxaa);
    bool get_use_fxaa() const;

    void set_use_debanding(bool p_debanding);
    bool get_use_debanding() const;

    void set_sharpen_intensity(float p_intensity);
    float get_sharpen_intensity() const;
    void set_hdr(bool p_hdr);
    bool get_hdr() const;
    void set_use_32_bpc_depth(bool p_enable);
    bool get_use_32_bpc_depth() const;

    Vector2 get_camera_coords(const Vector2 &p_viewport_coords) const;
    Vector2 get_camera_rect_size() const;

    void set_use_own_world(bool p_world);
    bool is_using_own_world() const;

    void input(const Ref<InputEvent> &p_event);
    void unhandled_input(const Ref<InputEvent> &p_event);

    void set_disable_input(bool p_disable);
    bool is_input_disabled() const;

    void set_disable_3d(bool p_disable);
    bool is_3d_disabled() const;

    void set_keep_3d_linear(bool p_keep_3d_linear);
    bool get_keep_3d_linear() const;

    void set_attach_to_screen_rect(const Rect2 &p_rect);
    Rect2 get_attach_to_screen_rect() const;

    Vector2 get_mouse_position() const;
    void warp_mouse(const Vector2 &p_pos);

    void set_physics_object_picking(bool p_enable);
    bool get_physics_object_picking();

    bool gui_has_modal_stack() const;

    Variant gui_get_drag_data() const;
    Control *get_modal_stack_top() const;

    void gui_reset_canvas_sort_index();
    int gui_get_canvas_sort_index();

    String get_configuration_warning() const override;

    void set_usage(Usage p_usage);
    Usage get_usage() const;

    void set_debug_draw(DebugDraw p_debug_draw);
    DebugDraw get_debug_draw() const;

    int get_render_info(RenderInfo p_info);

    void set_snap_controls_to_pixels(bool p_enable);
    bool is_snap_controls_to_pixels_enabled() const;

    void _subwindow_visibility_changed();

    void set_input_as_handled();
    bool is_input_handled() const;

    void set_handle_input_locally(bool p_enable);
    bool is_handling_input_locally() const;

    bool gui_is_dragging() const;
    bool gui_is_drag_successful() const;

    Viewport();
    ~Viewport() override;
};

void register_viewport_local_classes();

