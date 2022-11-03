/*************************************************************************/
/*  canvas_layer.cpp                                                     */
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

#include "canvas_layer.h"
#include "scene_tree.h"
#include "scene/2d/canvas_item.h"
#include "viewport.h"
#include "core/object_db.h"
#include "core/method_bind.h"
#include "core/method_bind.h"

IMPL_GDCLASS(CanvasLayer)


void CanvasLayer::set_layer(int p_xform) {

    layer = p_xform;
    if (viewport != entt::null)
        RenderingServer::get_singleton()->viewport_set_canvas_stacking(viewport, canvas, layer, get_position_in_parent());
}

int CanvasLayer::get_layer() const {

    return layer;
}
void CanvasLayer::set_visible(bool p_visible) {
    if (p_visible == visible) {
        return;
    }

    visible = p_visible;
    emit_signal("visibility_changed");

    // For CanvasItems that is explicitly top level or has non-CanvasItem parents.
    if (is_inside_tree()) {
        const StringName group("root_canvas" + itos(entt::to_integral(canvas)));
        get_tree()->call_group_flags(SceneTree::GROUP_CALL_UNIQUE, group, "_toplevel_visibility_changed", p_visible);
    }
}
void CanvasLayer::show() {
    set_visible(true);
}

void CanvasLayer::hide() {
    set_visible(false);
}

bool CanvasLayer::is_visible() const {
    return visible;
}

void CanvasLayer::set_transform(const Transform2D &p_xform) {

    transform = p_xform;
    locrotscale_dirty = true;
    if (viewport!=entt::null)
        RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, canvas, transform);
}

Transform2D CanvasLayer::get_transform() const {

    return transform;
}

void CanvasLayer::_update_xform() {

    transform.set_rotation_and_scale(rot, scale);
    transform.set_origin(ofs);
    if (viewport!=entt::null)
        RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, canvas, transform);
}

void CanvasLayer::_update_locrotscale() {

    ofs = transform.elements[2];
    rot = transform.get_rotation();
    scale = transform.get_scale();
    locrotscale_dirty = false;
}

void CanvasLayer::set_offset(const Vector2 &p_offset) {

    if (locrotscale_dirty)
        _update_locrotscale();

    ofs = p_offset;
    _update_xform();
}

Vector2 CanvasLayer::get_offset() const {

    if (locrotscale_dirty)
        const_cast<CanvasLayer *>(this)->_update_locrotscale();

    return ofs;
}

void CanvasLayer::set_rotation(real_t p_radians) {

    if (locrotscale_dirty)
        _update_locrotscale();

    rot = p_radians;
    _update_xform();
}

real_t CanvasLayer::get_rotation() const {

    if (locrotscale_dirty)
        const_cast<CanvasLayer *>(this)->_update_locrotscale();

    return rot;
}

void CanvasLayer::set_rotation_degrees(real_t p_degrees) {

    set_rotation(Math::deg2rad(p_degrees));
}

real_t CanvasLayer::get_rotation_degrees() const {

    return Math::rad2deg(get_rotation());
}

void CanvasLayer::set_scale(const Vector2 &p_scale) {

    if (locrotscale_dirty)
        _update_locrotscale();

    scale = p_scale;
    _update_xform();
}

Vector2 CanvasLayer::get_scale() const {

    if (locrotscale_dirty)
        const_cast<CanvasLayer *>(this)->_update_locrotscale();

    return scale;
}

void CanvasLayer::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {

            if (custom_viewport && object_for_entity(custom_viewport_id)) {

                vp = custom_viewport;
            } else {
                vp = Node::get_viewport();
            }
            ERR_FAIL_NULL_MSG(vp, "Viewport is not initialized.");

            vp->_canvas_layer_add(this);
            viewport = vp->get_viewport_rid();

            RenderingServer::get_singleton()->viewport_attach_canvas(viewport, canvas);
            RenderingServer::get_singleton()->viewport_set_canvas_stacking(viewport, canvas, layer, get_position_in_parent());
            RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, canvas, transform);
            _update_follow_viewport();

        } break;
        case NOTIFICATION_EXIT_TREE: {

            ERR_FAIL_NULL_MSG(vp, "Viewport is not initialized.");
            vp->_canvas_layer_remove(this);
            RenderingServer::get_singleton()->viewport_remove_canvas(viewport, canvas);
            viewport = entt::null;
            _update_follow_viewport(false);

        } break;
        case NOTIFICATION_MOVED_IN_PARENT: {

            if (is_inside_tree())
                RenderingServer::get_singleton()->viewport_set_canvas_stacking(viewport, canvas, layer, get_position_in_parent());

        } break;
    }
}

Size2 CanvasLayer::get_viewport_size() const {

    if (!is_inside_tree()) {
        return Size2(1, 1);
    }
    ERR_FAIL_NULL_V_MSG(vp, Size2(1, 1), "Viewport is not initialized.");

    Rect2 r = vp->get_visible_rect();
    return r.size;
}

RenderingEntity CanvasLayer::get_viewport() const {

    return viewport;
}

void CanvasLayer::set_custom_viewport(Node *p_viewport) {
    ERR_FAIL_NULL_MSG(p_viewport, "Cannot set viewport to nullptr.");
    auto *rs = RenderingServer::get_singleton();
    ERR_FAIL_NULL(p_viewport);
    if (is_inside_tree()) {
        vp->_canvas_layer_remove(this);
        rs->viewport_remove_canvas(viewport, canvas);
        viewport = entt::null;
    }

    custom_viewport = object_cast<Viewport>(p_viewport);

    if (custom_viewport) {
        custom_viewport_id = custom_viewport->get_instance_id();
    } else {
        custom_viewport_id = entt::null;
    }

    if (!is_inside_tree()) {
        return;
    }

    if (custom_viewport) {
        vp = custom_viewport;
    } else {
        vp = Node::get_viewport();
    }

    vp->_canvas_layer_add(this);
    viewport = vp->get_viewport_rid();

    rs->viewport_attach_canvas(viewport, canvas);
    rs->viewport_set_canvas_stacking(viewport, canvas, layer, get_position_in_parent());
    rs->viewport_set_canvas_transform(viewport, canvas, transform);
}

Node *CanvasLayer::get_custom_viewport() const {

    return custom_viewport;
}

void CanvasLayer::reset_sort_index() {
    sort_index = 0;
}

int CanvasLayer::get_sort_index() {

    return sort_index++;
}

RenderingEntity CanvasLayer::get_canvas() const {

    return canvas;
}

void CanvasLayer::set_follow_viewport(bool p_enable) {
    if (follow_viewport == p_enable) {
        return;
    }

    follow_viewport = p_enable;
    _update_follow_viewport();
}

bool CanvasLayer::is_following_viewport() const {
    return follow_viewport;
}

void CanvasLayer::set_follow_viewport_scale(float p_ratio) {
    follow_viewport_scale = p_ratio;
    _update_follow_viewport();
}

float CanvasLayer::get_follow_viewport_scale() const {
    return follow_viewport_scale;
}

void CanvasLayer::_update_follow_viewport(bool p_force_exit) {

    if (!is_inside_tree()) {
        return;
    }
    if (p_force_exit || !follow_viewport) {
        RenderingServer::get_singleton()->canvas_set_parent(canvas, entt::null, 1.0);
    } else {
        RenderingServer::get_singleton()->canvas_set_parent(canvas, vp->get_world_2d()->get_canvas(), follow_viewport_scale);
    }
}

void CanvasLayer::_bind_methods() {

    SE_BIND_METHOD(CanvasLayer,set_layer);
    SE_BIND_METHOD(CanvasLayer,get_layer);

    SE_BIND_METHOD(CanvasLayer,set_visible);
    SE_BIND_METHOD(CanvasLayer,is_visible);
    SE_BIND_METHOD(CanvasLayer,show);
    SE_BIND_METHOD(CanvasLayer,hide);
    SE_BIND_METHOD(CanvasLayer,set_transform);
    SE_BIND_METHOD(CanvasLayer,get_transform);

    SE_BIND_METHOD(CanvasLayer,set_offset);
    SE_BIND_METHOD(CanvasLayer,get_offset);

    SE_BIND_METHOD(CanvasLayer,set_rotation);
    SE_BIND_METHOD(CanvasLayer,get_rotation);

    SE_BIND_METHOD(CanvasLayer,set_rotation_degrees);
    SE_BIND_METHOD(CanvasLayer,get_rotation_degrees);

    SE_BIND_METHOD(CanvasLayer,set_scale);
    SE_BIND_METHOD(CanvasLayer,get_scale);

    SE_BIND_METHOD(CanvasLayer,set_follow_viewport);
    SE_BIND_METHOD(CanvasLayer,is_following_viewport);

    SE_BIND_METHOD(CanvasLayer,set_follow_viewport_scale);
    SE_BIND_METHOD(CanvasLayer,get_follow_viewport_scale);

    SE_BIND_METHOD(CanvasLayer,set_custom_viewport);
    SE_BIND_METHOD(CanvasLayer,get_custom_viewport);

    SE_BIND_METHOD(CanvasLayer,get_canvas);

    ADD_GROUP("Layer", "layer_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "layer_index", PropertyHint::Range, "-128,128,1"), "set_layer", "get_layer");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "layer_visible"), "set_visible", "is_visible");
    ADD_GROUP("Transform", "xfm_");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "xfm_offset"), "set_offset", "get_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "xfm_rotation_degrees", PropertyHint::Range, "-1080,1080,0.1,or_lesser,or_greater", PROPERTY_USAGE_EDITOR), "set_rotation_degrees", "get_rotation_degrees");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "xfm_rotation", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_rotation", "get_rotation");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "xfm_scale"), "set_scale", "get_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM2D, "xfm_transform"), "set_transform", "get_transform");
    ADD_GROUP("", "");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "custom_viewport", PropertyHint::ResourceType, "Viewport", 0), "set_custom_viewport", "get_custom_viewport");
    ADD_GROUP("Follow Viewport", "follow_viewport_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "follow_viewport_enable"), "set_follow_viewport", "is_following_viewport");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "follow_viewport_scale", PropertyHint::Range, "0.001,1000,0.001,or_greater,or_lesser"), "set_follow_viewport_scale", "get_follow_viewport_scale");
    ADD_SIGNAL(MethodInfo("visibility_changed"));
}

#ifdef TOOLS_ENABLED
StringName CanvasLayer::get_property_store_alias(const StringName &p_property) const {
    if (p_property == "rotation_degrees") {
        return "rotation";
    } else {
        return Node::get_property_store_alias(p_property);
    }
}
#endif
CanvasLayer::CanvasLayer() {

    canvas = RenderingServer::get_singleton()->canvas_create();
}

CanvasLayer::~CanvasLayer() {

    RenderingServer::get_singleton()->free_rid(canvas);
}
