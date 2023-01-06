/*************************************************************************/
/*  camera_2d.cpp                                                        */
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

#include "camera_2d.h"

#include "core/engine.h"
#include "core/object_db.h"
#include "core/method_bind.h"
#include "core/callable_method_pointer.h"
#include "scene/scene_string_names.h"
#include "servers/rendering_server.h"
#include "scene/main/scene_tree.h"

IMPL_GDCLASS(Camera2D)
VARIANT_ENUM_CAST(Camera2D::AnchorMode);
VARIANT_ENUM_CAST(Camera2D::Camera2DProcessMode);

void Camera2D::_update_scroll() {

    if (!is_inside_tree()) {
        return;
    }

    if (Engine::get_singleton()->is_editor_hint()) {
        update(); //will just be drawn
        return;
    }

    if (!viewport || !current) {
        return;
    }

    ERR_FAIL_COND(custom_viewport && !object_for_entity(custom_viewport_id));

    Transform2D xform = get_camera_transform();

    viewport->set_canvas_transform(xform);

    Size2 screen_size = viewport->get_visible_rect().size;
    Point2 screen_offset = (anchor_mode == ANCHOR_MODE_DRAG_CENTER ? (screen_size * 0.5) : Point2());

    get_tree()->call_group_flags(SceneTree::GROUP_CALL_REALTIME, group_name, "_camera_moved", xform, screen_offset);
}

void Camera2D::_update_process_mode() {

    // smoothing can be enabled in the editor but will never be active
    if (process_mode == CAMERA2D_PROCESS_IDLE) {
        set_process_internal(smoothing_active);
        set_physics_process_internal(false);
    } else {
        set_process_internal(false);
        set_physics_process_internal(smoothing_active);
    }
}

void Camera2D::_setup_viewport() {
    const auto callable_update = callable_mp(this,&Camera2D::_update_scroll);
    // Disconnect signal on previous viewport if there's one.
    if (viewport && viewport->is_connected("size_changed", callable_update)) {
        viewport->disconnect("size_changed", callable_update);
    }

    if (custom_viewport && object_for_entity(custom_viewport_id)) {
        viewport = custom_viewport;
    } else {
        viewport = get_viewport();
    }

    RenderingEntity vp = viewport->get_viewport_rid();
    group_name = StringName("__cameras_" + itos(entt::to_integral(vp)));
    canvas_group_name = StringName("__cameras_c" + itos(entt::to_integral(canvas)));
    add_to_group(group_name);
    add_to_group(canvas_group_name);

    viewport->connect("size_changed", callable_update);
}

void Camera2D::set_zoom(const Vector2 &p_zoom) {
    // Setting zoom to zero causes 'affine_invert' issues
    ERR_FAIL_COND_MSG(Math::is_zero_approx(p_zoom.x) || Math::is_zero_approx(p_zoom.y), "Zoom level must be different from 0 (can be negative).");

    zoom = p_zoom;
    Point2 old_smoothed_camera_pos = smoothed_camera_pos;
    _update_scroll();
    smoothed_camera_pos = old_smoothed_camera_pos;
}

Vector2 Camera2D::get_zoom() const {

    return zoom;
}

Transform2D Camera2D::get_camera_transform() {

    if (!get_tree() || !viewport) {
        return Transform2D();
    }

    ERR_FAIL_COND_V(custom_viewport && !object_for_entity(custom_viewport_id), Transform2D());

    Size2 screen_size = viewport->get_visible_rect().size;

    Point2 new_camera_pos = get_global_transform().get_origin();
    Point2 ret_camera_pos;

    if (!first) {

        if (anchor_mode == ANCHOR_MODE_DRAG_CENTER) {

            if (h_drag_enabled && !Engine::get_singleton()->is_editor_hint() && !h_offset_changed) {
                camera_pos.x = MIN(camera_pos.x, (new_camera_pos.x + screen_size.x * 0.5f * zoom.x * drag_margin[(int8_t)Margin::Left]));
                camera_pos.x = M_MAX(camera_pos.x, (new_camera_pos.x - screen_size.x * 0.5f * zoom.x * drag_margin[(int8_t)Margin::Right]));
            } else {

                if (h_ofs < 0) {
                    camera_pos.x = new_camera_pos.x + screen_size.x * 0.5f * drag_margin[(int8_t)Margin::Right] * h_ofs;
                } else {
                    camera_pos.x = new_camera_pos.x + screen_size.x * 0.5f * drag_margin[(int8_t)Margin::Left] * h_ofs;
                }

                h_offset_changed = false;
            }

            if (v_drag_enabled && !Engine::get_singleton()->is_editor_hint() && !v_offset_changed) {

                camera_pos.y = MIN(camera_pos.y, (new_camera_pos.y + screen_size.y * 0.5f * zoom.y * drag_margin[(int8_t)Margin::Top]));
                camera_pos.y = M_MAX(camera_pos.y, (new_camera_pos.y - screen_size.y * 0.5f * zoom.y * drag_margin[(int8_t)Margin::Bottom]));

            } else {

                if (v_ofs < 0) {
                    camera_pos.y = new_camera_pos.y + screen_size.y * 0.5f * drag_margin[(int8_t)Margin::Bottom] * v_ofs;
                } else {
                    camera_pos.y = new_camera_pos.y + screen_size.y * 0.5f * drag_margin[(int8_t)Margin::Top] * v_ofs;
                }

                v_offset_changed = false;
            }

        } else if (anchor_mode == ANCHOR_MODE_FIXED_TOP_LEFT) {

            camera_pos = new_camera_pos;
        }

        Point2 screen_offset = (anchor_mode == ANCHOR_MODE_DRAG_CENTER ? (screen_size * 0.5 * zoom) : Point2());
        Rect2 screen_rect(-screen_offset + camera_pos, screen_size * zoom);

        if (limit_smoothing_enabled) {
            if (screen_rect.position.x < limit[(int8_t)Margin::Left])
                camera_pos.x -= screen_rect.position.x - limit[(int8_t)Margin::Left];

            if (screen_rect.position.x + screen_rect.size.x > limit[(int8_t)Margin::Right])
                camera_pos.x -= screen_rect.position.x + screen_rect.size.x - limit[(int8_t)Margin::Right];

            if (screen_rect.position.y + screen_rect.size.y > limit[(int8_t)Margin::Bottom])
                camera_pos.y -= screen_rect.position.y + screen_rect.size.y - limit[(int8_t)Margin::Bottom];

            if (screen_rect.position.y < limit[(int8_t)Margin::Top])
                camera_pos.y -= screen_rect.position.y - limit[(int8_t)Margin::Top];
        }

        if (smoothing_active) {

            float c = smoothing * (process_mode == CAMERA2D_PROCESS_PHYSICS ? get_physics_process_delta_time() : get_process_delta_time());
            smoothed_camera_pos = ((camera_pos - smoothed_camera_pos) * c) + smoothed_camera_pos;
            ret_camera_pos = smoothed_camera_pos;
        } else {

            ret_camera_pos = smoothed_camera_pos = camera_pos;
        }

    } else {
        ret_camera_pos = smoothed_camera_pos = camera_pos = new_camera_pos;
        first = false;
    }

    Point2 screen_offset = (anchor_mode == ANCHOR_MODE_DRAG_CENTER ? (screen_size * 0.5 * zoom) : Point2());

    float angle = get_global_transform().get_rotation();
    if (rotating) {
        screen_offset = screen_offset.rotated(angle);
    }

    Rect2 screen_rect(-screen_offset + ret_camera_pos, screen_size * zoom);
    if (!smoothing_enabled || !limit_smoothing_enabled) {
        if (screen_rect.position.x < limit[(int8_t)Margin::Left]) {
        screen_rect.position.x = limit[(int8_t)Margin::Left];

        }
        if (screen_rect.position.x + screen_rect.size.x > limit[(int8_t)Margin::Right]) {
        screen_rect.position.x = limit[(int8_t)Margin::Right] - screen_rect.size.x;

        }
        if (screen_rect.position.y + screen_rect.size.y > limit[(int8_t)Margin::Bottom]) {
        screen_rect.position.y = limit[(int8_t)Margin::Bottom] - screen_rect.size.y;

        }
        if (screen_rect.position.y < limit[(int8_t)Margin::Top]) {
        screen_rect.position.y = limit[(int8_t)Margin::Top];

        }
    }
    if (offset != Vector2())
        screen_rect.position += offset;

    camera_screen_center = screen_rect.position + screen_rect.size * 0.5;

    Transform2D xform;
    xform.scale_basis(zoom);
    if (rotating) {
        xform.set_rotation(angle);
    }
    xform.set_origin(screen_rect.position);

    return (xform).affine_inverse();
}

void Camera2D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_INTERNAL_PROCESS:
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {

        if (!smoothing_active) {
            _update_scroll();
        }

        } break;
        case NOTIFICATION_TRANSFORM_CHANGED: {

            if (!is_processing_internal() && !is_physics_processing_internal())
                _update_scroll();

        } break;
        case NOTIFICATION_ENTER_TREE: {

            ERR_FAIL_COND(!is_inside_tree());

            canvas = get_canvas();

            _setup_viewport();

            _update_process_mode();
            // if a camera enters the tree that is set to current,
            // it should take over as the current camera, and mark
            // all other cameras as non current
            _set_current(current);

            first = true;

        } break;
        case NOTIFICATION_EXIT_TREE: {
            const bool viewport_valid = !custom_viewport || object_for_entity(custom_viewport_id);

            if (is_current()) {
                if (viewport && viewport_valid) {
                    viewport->set_canvas_transform(Transform2D());
                }
            }
            if (viewport && viewport_valid) {
                viewport->disconnect("size_changed", callable_mp(this, &Camera2D::_update_scroll));
            }
            remove_from_group(group_name);
            remove_from_group(canvas_group_name);
            viewport = nullptr;

        } break;
#ifdef TOOLS_ENABLED
        case NOTIFICATION_DRAW: {

            if (!is_inside_tree() || !Engine::get_singleton()->is_editor_hint())
                break;

            if (screen_drawing_enabled) {
                Color area_axis_color(1, 0.4, 1, 0.63);
                float area_axis_width = 1;
                if (is_current()) {
                    area_axis_width = 3;
                }

                Transform2D inv_camera_transform = get_camera_transform().affine_inverse();
                Size2 screen_size = get_viewport_rect().size;

                Vector2 screen_endpoints[4] = {
                    inv_camera_transform.xform(Vector2(0, 0)),
                    inv_camera_transform.xform(Vector2(screen_size.width, 0)),
                    inv_camera_transform.xform(Vector2(screen_size.width, screen_size.height)),
                    inv_camera_transform.xform(Vector2(0, screen_size.height))
                };

                Transform2D inv_transform = get_global_transform().affine_inverse(); // undo global space

                for (int i = 0; i < 4; i++) {
                    draw_line(inv_transform.xform(screen_endpoints[i]), inv_transform.xform(screen_endpoints[(i + 1) % 4]), area_axis_color, area_axis_width);
                }
            }

            if (limit_drawing_enabled) {
                Color limit_drawing_color(1, 1, 0.25, 0.63);
                float limit_drawing_width = 1;
                if (is_current()) {
                    limit_drawing_width = 3;
                }

                Vector2 camera_origin = get_global_transform().get_origin();
                Vector2 camera_scale = get_global_transform().get_scale().abs();
                Vector2 limit_points[4] = {
                    (Vector2(limit[(int8_t)Margin::Left], limit[(int8_t)Margin::Top]) - camera_origin) / camera_scale,
                    (Vector2(limit[(int8_t)Margin::Right], limit[(int8_t)Margin::Top]) - camera_origin) / camera_scale,
                    (Vector2(limit[(int8_t)Margin::Right], limit[(int8_t)Margin::Bottom]) - camera_origin) / camera_scale,
                    (Vector2(limit[(int8_t)Margin::Left], limit[(int8_t)Margin::Bottom]) - camera_origin) / camera_scale
                };

                for (int i = 0; i < 4; i++) {
                    draw_line(limit_points[i], limit_points[(i + 1) % 4], limit_drawing_color, limit_drawing_width);
                }
            }

            if (margin_drawing_enabled) {
                Color margin_drawing_color(0.25, 1, 1, 0.63);
                float margin_drawing_width = 1;
                if (is_current()) {
                    margin_drawing_width = 3;
                }

                Transform2D inv_camera_transform = get_camera_transform().affine_inverse();
                Size2 screen_size = get_viewport_rect().size;

                Vector2 margin_endpoints[4] = {
                    inv_camera_transform.xform(Vector2((screen_size.width / 2) - ((screen_size.width / 2) * drag_margin[(int8_t)Margin::Left]), (screen_size.height / 2) - ((screen_size.height / 2) * drag_margin[(int8_t)Margin::Top]))),
                    inv_camera_transform.xform(Vector2((screen_size.width / 2) + ((screen_size.width / 2) * drag_margin[(int8_t)Margin::Right]), (screen_size.height / 2) - ((screen_size.height / 2) * drag_margin[(int8_t)Margin::Top]))),
                    inv_camera_transform.xform(Vector2((screen_size.width / 2) + ((screen_size.width / 2) * drag_margin[(int8_t)Margin::Right]), (screen_size.height / 2) + ((screen_size.height / 2) * drag_margin[(int8_t)Margin::Bottom]))),
                    inv_camera_transform.xform(Vector2((screen_size.width / 2) - ((screen_size.width / 2) * drag_margin[(int8_t)Margin::Left]), (screen_size.height / 2) + ((screen_size.height / 2) * drag_margin[(int8_t)Margin::Bottom])))
                };

                Transform2D inv_transform = get_global_transform().affine_inverse(); // undo global space

                for (int i = 0; i < 4; i++) {
                    draw_line(inv_transform.xform(margin_endpoints[i]), inv_transform.xform(margin_endpoints[(i + 1) % 4]), margin_drawing_color, margin_drawing_width);
                }
            }

        } break;
#endif
    }
}

void Camera2D::set_offset(const Vector2 &p_offset) {

    offset = p_offset;
    _update_scroll();
}

Vector2 Camera2D::get_offset() const {

    return offset;
}

void Camera2D::set_anchor_mode(AnchorMode p_anchor_mode) {

    anchor_mode = p_anchor_mode;
    _update_scroll();
}

Camera2D::AnchorMode Camera2D::get_anchor_mode() const {

    return anchor_mode;
}

void Camera2D::set_rotating(bool p_rotating) {

    rotating = p_rotating;
    _update_scroll();
}

bool Camera2D::is_rotating() const {

    return rotating;
}

void Camera2D::set_process_mode(Camera2DProcessMode p_mode) {

    if (process_mode == p_mode)
        return;

    process_mode = p_mode;
    _update_process_mode();
}

Camera2D::Camera2DProcessMode Camera2D::get_process_mode() const {

    return process_mode;
}

void Camera2D::_make_current(Object *p_which) {

    if (p_which == this) {

        current = true;
    } else {
        current = false;
    }
}

void Camera2D::_set_current(bool p_current) {

    if (p_current)
        make_current();

    current = p_current;
    update();
}

bool Camera2D::is_current() const {

    return current;
}

void Camera2D::make_current() {

    if (!is_inside_tree()) {
        current = true;
    } else {
        get_tree()->call_group_flags(SceneTree::GROUP_CALL_REALTIME, group_name, "_make_current", Variant(this));
    }
    _update_scroll();
}

void Camera2D::clear_current() {

    current = false;
    if (is_inside_tree()) {
        get_tree()->call_group_flags(SceneTree::GROUP_CALL_REALTIME, group_name, "_make_current", Variant((Object *)nullptr));
    }
}

void Camera2D::set_limit(Margin p_margin, int p_limit) {

    ERR_FAIL_INDEX((int)p_margin, 4);
    limit[(int)p_margin] = p_limit;
    update();
}

int Camera2D::get_limit(Margin p_margin) const {

    ERR_FAIL_INDEX_V((int)p_margin, 4, 0);
    return limit[(int)p_margin];
}

void Camera2D::set_limit_smoothing_enabled(bool enable) {

    limit_smoothing_enabled = enable;
    _update_scroll();
}

bool Camera2D::is_limit_smoothing_enabled() const {

    return limit_smoothing_enabled;
}

void Camera2D::set_drag_margin(Margin p_margin, float p_drag_margin) {

    ERR_FAIL_INDEX((int)p_margin, 4);
    drag_margin[(int)p_margin] = p_drag_margin;
    update();
}

float Camera2D::get_drag_margin(Margin p_margin) const {

    ERR_FAIL_INDEX_V((int)p_margin, 4, 0);
    return drag_margin[(int)p_margin];
}

Vector2 Camera2D::get_camera_position() const {

    return camera_pos;
}

void Camera2D::force_update_scroll() {

    _update_scroll();
}

void Camera2D::reset_smoothing() {

    _update_scroll();
    smoothed_camera_pos = camera_pos;
}

void Camera2D::align() {

    ERR_FAIL_COND(!is_inside_tree() || !viewport);
    ERR_FAIL_COND(custom_viewport && !object_for_entity(custom_viewport_id));

    Size2 screen_size = viewport->get_visible_rect().size;

    Point2 current_camera_pos = get_global_transform().get_origin();
    if (anchor_mode == ANCHOR_MODE_DRAG_CENTER) {
        if (h_ofs < 0) {
            camera_pos.x = current_camera_pos.x + screen_size.x * 0.5f * drag_margin[(int8_t)Margin::Right] * h_ofs;
        } else {
            camera_pos.x = current_camera_pos.x + screen_size.x * 0.5f * drag_margin[(int8_t)Margin::Left] * h_ofs;
        }
        if (v_ofs < 0) {
            camera_pos.y = current_camera_pos.y + screen_size.y * 0.5f * drag_margin[(int8_t)Margin::Top] * v_ofs;
        } else {
            camera_pos.y = current_camera_pos.y + screen_size.y * 0.5f * drag_margin[(int8_t)Margin::Bottom] * v_ofs;
        }
    } else if (anchor_mode == ANCHOR_MODE_FIXED_TOP_LEFT) {

        camera_pos = current_camera_pos;
    }

    _update_scroll();
}

void Camera2D::set_follow_smoothing(float p_speed) {

    smoothing = p_speed;
}

float Camera2D::get_follow_smoothing() const {

    return smoothing;
}

Point2 Camera2D::get_camera_screen_center() const {

    return camera_screen_center;
}

void Camera2D::set_h_drag_enabled(bool p_enabled) {

    h_drag_enabled = p_enabled;
}

bool Camera2D::is_h_drag_enabled() const {

    return h_drag_enabled;
}

void Camera2D::set_v_drag_enabled(bool p_enabled) {

    v_drag_enabled = p_enabled;
}

bool Camera2D::is_v_drag_enabled() const {

    return v_drag_enabled;
}

void Camera2D::set_v_offset(float p_offset) {

    v_ofs = p_offset;
    v_offset_changed = true;
    _update_scroll();
}

float Camera2D::get_v_offset() const {

    return v_ofs;
}

void Camera2D::set_h_offset(float p_offset) {

    h_ofs = p_offset;
    h_offset_changed = true;
    _update_scroll();
}
float Camera2D::get_h_offset() const {

    return h_ofs;
}


void Camera2D::set_enable_follow_smoothing(bool p_enabled) {
    // watch for situation where an pre-enabled camera is added to the tree
    // processing must be resumed and bypass this noop check
    // (this currently works but is a possible future bug)
    if (smoothing_enabled == p_enabled) {
        return;
    }

    // Separate the logic between enabled and active, because the smoothing
    // cannot be active in the editor. This can be done without a separate flag
    // but is bug prone so this approach is easier to follow.
    smoothing_enabled = p_enabled;
    smoothing_active = smoothing_enabled && !Engine::get_singleton()->is_editor_hint();

    // keep the processing up to date after each change
    _update_process_mode();
}

bool Camera2D::is_follow_smoothing_enabled() const {

    return smoothing_enabled;
}

void Camera2D::set_custom_viewport(Node *p_viewport) {
    ERR_FAIL_NULL(p_viewport);
    if (is_inside_tree()) {
        remove_from_group(group_name);
        remove_from_group(canvas_group_name);
    }

    if (custom_viewport && !object_for_entity(custom_viewport_id)) {
        viewport = nullptr;
    }
    custom_viewport = object_cast<Viewport>(p_viewport);

    if (custom_viewport) {
        custom_viewport_id = custom_viewport->get_instance_id();
    } else {
        custom_viewport_id = entt::null;
    }

    if (is_inside_tree()) {

        _setup_viewport();
    }
}

Node *Camera2D::get_custom_viewport() const {

    return custom_viewport;
}

void Camera2D::set_screen_drawing_enabled(bool enable) {
    screen_drawing_enabled = enable;
#ifdef TOOLS_ENABLED
    update();
#endif
}

bool Camera2D::is_screen_drawing_enabled() const {
    return screen_drawing_enabled;
}

void Camera2D::set_limit_drawing_enabled(bool enable) {
    limit_drawing_enabled = enable;
#ifdef TOOLS_ENABLED
    update();
#endif
}

bool Camera2D::is_limit_drawing_enabled() const {
    return limit_drawing_enabled;
}

void Camera2D::set_margin_drawing_enabled(bool enable) {
    margin_drawing_enabled = enable;
#ifdef TOOLS_ENABLED
    update();
#endif
}

bool Camera2D::is_margin_drawing_enabled() const {
    return margin_drawing_enabled;
}

void Camera2D::_bind_methods() {

    SE_BIND_METHOD(Camera2D,set_offset);
    SE_BIND_METHOD(Camera2D,get_offset);

    SE_BIND_METHOD(Camera2D,set_anchor_mode);
    SE_BIND_METHOD(Camera2D,get_anchor_mode);

    SE_BIND_METHOD(Camera2D,set_rotating);
    SE_BIND_METHOD(Camera2D,is_rotating);

    SE_BIND_METHOD(Camera2D,make_current);
    SE_BIND_METHOD(Camera2D,clear_current);
    SE_BIND_METHOD(Camera2D,_make_current);

    SE_BIND_METHOD(Camera2D,set_process_mode);
    SE_BIND_METHOD(Camera2D,get_process_mode);

    SE_BIND_METHOD(Camera2D,_set_current);
    SE_BIND_METHOD(Camera2D,is_current);

    SE_BIND_METHOD(Camera2D,set_limit);
    SE_BIND_METHOD(Camera2D,get_limit);

    SE_BIND_METHOD(Camera2D,set_limit_smoothing_enabled);
    SE_BIND_METHOD(Camera2D,is_limit_smoothing_enabled);

    SE_BIND_METHOD(Camera2D,set_v_drag_enabled);
    SE_BIND_METHOD(Camera2D,is_v_drag_enabled);

    SE_BIND_METHOD(Camera2D,set_h_drag_enabled);
    SE_BIND_METHOD(Camera2D,is_h_drag_enabled);

    SE_BIND_METHOD(Camera2D,set_v_offset);
    SE_BIND_METHOD(Camera2D,get_v_offset);

    SE_BIND_METHOD(Camera2D,set_h_offset);
    SE_BIND_METHOD(Camera2D,get_h_offset);

    SE_BIND_METHOD(Camera2D,set_drag_margin);
    SE_BIND_METHOD(Camera2D,get_drag_margin);

    SE_BIND_METHOD(Camera2D,get_camera_position);
    SE_BIND_METHOD(Camera2D,get_camera_screen_center);

    SE_BIND_METHOD(Camera2D,set_zoom);
    SE_BIND_METHOD(Camera2D,get_zoom);

    SE_BIND_METHOD(Camera2D,set_custom_viewport);
    SE_BIND_METHOD(Camera2D,get_custom_viewport);

    SE_BIND_METHOD(Camera2D,set_follow_smoothing);
    SE_BIND_METHOD(Camera2D,get_follow_smoothing);

    SE_BIND_METHOD(Camera2D,set_enable_follow_smoothing);
    SE_BIND_METHOD(Camera2D,is_follow_smoothing_enabled);

    SE_BIND_METHOD(Camera2D,force_update_scroll);
    SE_BIND_METHOD(Camera2D,reset_smoothing);
    SE_BIND_METHOD(Camera2D,align);


    SE_BIND_METHOD(Camera2D,set_screen_drawing_enabled);
    SE_BIND_METHOD(Camera2D,is_screen_drawing_enabled);

    SE_BIND_METHOD(Camera2D,set_limit_drawing_enabled);
    SE_BIND_METHOD(Camera2D,is_limit_drawing_enabled);

    SE_BIND_METHOD(Camera2D,set_margin_drawing_enabled);
    SE_BIND_METHOD(Camera2D,is_margin_drawing_enabled);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "offset"), "set_offset", "get_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "anchor_mode", PropertyHint::Enum, "Fixed TopLeft,Drag Center"), "set_anchor_mode", "get_anchor_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "rotating"), "set_rotating", "is_rotating");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "current"), "_set_current", "is_current");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "zoom"), "set_zoom", "get_zoom");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "custom_viewport", PropertyHint::ResourceType, "Viewport", 0), "set_custom_viewport", "get_custom_viewport");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "process_mode", PropertyHint::Enum, "Physics,Idle"), "set_process_mode", "get_process_mode");

    ADD_GROUP("Limit", "limit_");
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "limit_left"), "set_limit", "get_limit", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "limit_top"), "set_limit", "get_limit", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "limit_right"), "set_limit", "get_limit", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "limit_bottom"), "set_limit", "get_limit", (int)Margin::Bottom);
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "limit_smoothed"), "set_limit_smoothing_enabled", "is_limit_smoothing_enabled");

    ADD_GROUP("Draw Margin", "draw_margin_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "drag_margin_h_enabled"), "set_h_drag_enabled", "is_h_drag_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "drag_margin_v_enabled"), "set_v_drag_enabled", "is_v_drag_enabled");

    ADD_GROUP("Smoothing", "smoothing_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "smoothing_enabled"), "set_enable_follow_smoothing", "is_follow_smoothing_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "smoothing_speed"), "set_follow_smoothing", "get_follow_smoothing");

    ADD_GROUP("Drag Offset", "offset_");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "offset_h", PropertyHint::Range, "-1,1,0.01"), "set_h_offset", "get_h_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "offset_v", PropertyHint::Range, "-1,1,0.01"), "set_v_offset", "get_v_offset");

    ADD_GROUP("Drag Margin", "drag_margin_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "drag_margin_left", PropertyHint::Range, "0,1,0.01"), "set_drag_margin", "get_drag_margin", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "drag_margin_top", PropertyHint::Range, "0,1,0.01"), "set_drag_margin", "get_drag_margin", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "drag_margin_right", PropertyHint::Range, "0,1,0.01"), "set_drag_margin", "get_drag_margin", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "drag_margin_bottom", PropertyHint::Range, "0,1,0.01"), "set_drag_margin", "get_drag_margin", (int)Margin::Bottom);

    ADD_GROUP("Editor", "editor_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "editor_draw_screen"), "set_screen_drawing_enabled", "is_screen_drawing_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "editor_draw_limits"), "set_limit_drawing_enabled", "is_limit_drawing_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "editor_draw_drag_margin"), "set_margin_drawing_enabled", "is_margin_drawing_enabled");

    BIND_ENUM_CONSTANT(ANCHOR_MODE_FIXED_TOP_LEFT);
    BIND_ENUM_CONSTANT(ANCHOR_MODE_DRAG_CENTER);
    BIND_ENUM_CONSTANT(CAMERA2D_PROCESS_PHYSICS);
    BIND_ENUM_CONSTANT(CAMERA2D_PROCESS_IDLE);
}

Camera2D::Camera2D() {

    anchor_mode = ANCHOR_MODE_DRAG_CENTER;
    rotating = false;
    current = false;
    limit[(int8_t)Margin::Left] = -10000000;
    limit[(int8_t)Margin::Top] = -10000000;
    limit[(int8_t)Margin::Right] = 10000000;
    limit[(int8_t)Margin::Bottom] = 10000000;

    drag_margin[(int8_t)Margin::Left] = 0.2f;
    drag_margin[(int8_t)Margin::Top] = 0.2f;
    drag_margin[(int8_t)Margin::Right] = 0.2f;
    drag_margin[(int8_t)Margin::Bottom] = 0.2f;
    camera_pos = Vector2();
    first = true;
    smoothing_enabled = false;
    smoothing_active = false;
    limit_smoothing_enabled = false;
    viewport = nullptr;
    custom_viewport = nullptr;
    custom_viewport_id = entt::null;
    process_mode = CAMERA2D_PROCESS_IDLE;

    smoothing = 5.0f;
    zoom = Vector2(1, 1);

    screen_drawing_enabled = true;
    limit_drawing_enabled = false;
    margin_drawing_enabled = false;

    h_drag_enabled = false;
    v_drag_enabled = false;
    h_ofs = 0;
    v_ofs = 0;
    h_offset_changed = false;
    v_offset_changed = false;

    set_notify_transform(true);
}
