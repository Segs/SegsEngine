/*************************************************************************/
/*  node_2d.cpp                                                          */
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

#include "node_2d.h"

#include "core/dictionary.h"
#include "core/message_queue.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "scene/gui/control.h"
#include "scene/main/viewport.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(Node2D)

#ifdef TOOLS_ENABLED
Dictionary Node2D::_edit_get_state() const {

    Dictionary state;
    state["position"] = get_position();
    state["rotation"] = get_rotation();
    state["scale"] = get_scale();
    state["skew"] = get_skew();

    return state;
}
void Node2D::_edit_set_state(const Dictionary &p_state) {

    pos = p_state["position"].as<Vector2>();
    angle = p_state["rotation"].as<float>();
    _scale = p_state["scale"].as<Vector2>();
    skew = p_state["skew"].as<float>();

    _update_transform();
    Object_change_notify(this,"rotation");
    Object_change_notify(this,"rotation_degrees");
    Object_change_notify(this,"scale");
    Object_change_notify(this,"skew");
    Object_change_notify(this,"skew_degrees");
    Object_change_notify(this,"position");
}

void Node2D::_edit_set_position(const Point2 &p_position) {
    set_position(p_position);
}

Point2 Node2D::_edit_get_position() const {
    return pos;
}

void Node2D::_edit_set_scale(const Size2 &p_scale) {
    set_scale(p_scale);
}

Size2 Node2D::_edit_get_scale() const {
    return _scale;
}

void Node2D::_edit_set_rotation(float p_rotation) {
    angle = p_rotation;
    _update_transform();
    Object_change_notify(this,"rotation");
    Object_change_notify(this,"rotation_degrees");
}

float Node2D::_edit_get_rotation() const {
    return angle;
}

bool Node2D::_edit_use_rotation() const {
    return true;
}

void Node2D::_edit_set_rect(const Rect2 &p_edit_rect) {
    ERR_FAIL_COND(!_edit_use_rect());

    Rect2 r = _edit_get_rect();

    Vector2 zero_offset;
    if (r.size.x != 0.0f)
        zero_offset.x = -r.position.x / r.size.x;
    if (r.size.y != 0.0f)
        zero_offset.y = -r.position.y / r.size.y;

    Size2 new_scale(1, 1);

    if (r.size.x != 0.0f)
        new_scale.x = p_edit_rect.size.x / r.size.x;
    if (r.size.y != 0.0f)
        new_scale.y = p_edit_rect.size.y / r.size.y;

    Point2 new_pos = p_edit_rect.position + p_edit_rect.size * zero_offset;

    Transform2D postxf;
    postxf.set_rotation_scale_and_skew(angle, _scale, skew);
    new_pos = postxf.xform(new_pos);

    pos += new_pos;
    _scale *= new_scale;

    _update_transform();
    Object_change_notify(this,"scale");
    Object_change_notify(this,"position");
}
#endif

void Node2D::_update_xform_values() {

    pos = _mat.elements[2];
    angle = _mat.get_rotation();
    _scale = _mat.get_scale();
    skew = _mat.get_skew();
    _xform_dirty = false;
}

void Node2D::_update_transform() {

    _mat.set_rotation_scale_and_skew(angle, _scale, skew);
    _mat.elements[2] = pos;

    RenderingServer::get_singleton()->canvas_item_set_transform(get_canvas_item(), _mat);

    if (!is_inside_tree())
        return;

    _notify_transform();
}

void Node2D::set_position(const Point2 &p_pos) {

    if (_xform_dirty)
        ((Node2D *)this)->_update_xform_values();
    pos = p_pos;
    _update_transform();
    Object_change_notify(this,"position");
}

void Node2D::set_rotation(float p_radians) {

    if (_xform_dirty)
        ((Node2D *)this)->_update_xform_values();
    angle = p_radians;
    _update_transform();
    Object_change_notify(this,"rotation");
    Object_change_notify(this,"rotation_degrees");
}

void Node2D::set_skew(float p_radians) {

    if (_xform_dirty)
        ((Node2D *)this)->_update_xform_values();
    skew = p_radians;
    _update_transform();
    Object_change_notify(this,"skew");
    Object_change_notify(this,"skew_degrees");
}

void Node2D::set_rotation_degrees(float p_degrees) {

    set_rotation(Math::deg2rad(p_degrees));
}

void Node2D::set_skew_degrees(float p_degrees) {

    set_skew(Math::deg2rad(p_degrees));
}

void Node2D::set_scale(const Size2 &p_scale) {

    if (_xform_dirty)
        ((Node2D *)this)->_update_xform_values();
    _scale = p_scale;
    // Avoid having 0 scale values, can lead to errors in physics and rendering.
    if (Math::is_zero_approx(_scale.x)) {
        _scale.x = CMP_EPSILON;
    }
    if (Math::is_zero_approx(_scale.y)) {
        _scale.y = CMP_EPSILON;
    }
    _update_transform();
    Object_change_notify(this,"scale");
}

Point2 Node2D::get_position() const {

    if (_xform_dirty)
        ((Node2D *)this)->_update_xform_values();
    return pos;
}

float Node2D::get_rotation() const {
    if (_xform_dirty)
        ((Node2D *)this)->_update_xform_values();

    return angle;
}

float Node2D::get_skew() const {
    if (_xform_dirty)
        ((Node2D *)this)->_update_xform_values();

    return skew;
}

float Node2D::get_rotation_degrees() const {

    return Math::rad2deg(get_rotation());
}

float Node2D::get_skew_degrees() const {

    return Math::rad2deg(get_skew());
}

Size2 Node2D::get_scale() const {
    if (_xform_dirty)
        ((Node2D *)this)->_update_xform_values();

    return _scale;
}

Transform2D Node2D::get_transform() const {

    return _mat;
}

void Node2D::rotate(float p_radians) {

    set_rotation(get_rotation() + p_radians);
}

void Node2D::translate(const Vector2 &p_amount) {

    set_position(get_position() + p_amount);
}

void Node2D::global_translate(const Vector2 &p_amount) {

    set_global_position(get_global_position() + p_amount);
}

void Node2D::apply_scale(const Size2 &p_amount) {

    set_scale(get_scale() * p_amount);
}

void Node2D::move_x(float p_delta, bool p_scaled) {

    Transform2D t = get_transform();
    Vector2 m = t[0];
    if (!p_scaled)
        m.normalize();
    set_position(t[2] + m * p_delta);
}

void Node2D::move_y(float p_delta, bool p_scaled) {

    Transform2D t = get_transform();
    Vector2 m = t[1];
    if (!p_scaled)
        m.normalize();
    set_position(t[2] + m * p_delta);
}

Point2 Node2D::get_global_position() const {

    return get_global_transform().get_origin();
}

void Node2D::set_global_position(const Point2 &p_pos) {

    Transform2D inv;
    CanvasItem *pi = get_parent_item();
    if (pi) {
        inv = pi->get_global_transform().affine_inverse();
        set_position(inv.xform(p_pos));
    } else {
        set_position(p_pos);
    }
}

float Node2D::get_global_rotation() const {

    return get_global_transform().get_rotation();
}

void Node2D::set_global_rotation(float p_radians) {

    CanvasItem *pi = get_parent_item();
    if (pi) {
        const float parent_global_rot = pi->get_global_transform().get_rotation();
        set_rotation(p_radians - parent_global_rot);
    } else {
        set_rotation(p_radians);
    }
}

float Node2D::get_global_rotation_degrees() const {

    return Math::rad2deg(get_global_rotation());
}

void Node2D::set_global_rotation_degrees(float p_degrees) {

    set_global_rotation(Math::deg2rad(p_degrees));
}

Size2 Node2D::get_global_scale() const {

    return get_global_transform().get_scale();
}

void Node2D::set_global_scale(const Size2 &p_scale) {

    CanvasItem *pi = get_parent_item();
    if (pi) {
        const Size2 parent_global_scale = pi->get_global_transform().get_scale();
        set_scale(p_scale / parent_global_scale);
    } else {
        set_scale(p_scale);
    }
}

void Node2D::set_transform(const Transform2D &p_transform) {

    _mat = p_transform;
    _xform_dirty = true;

    RenderingServer::get_singleton()->canvas_item_set_transform(get_canvas_item(), _mat);

    if (!is_inside_tree())
        return;

    _notify_transform();
}

void Node2D::set_global_transform(const Transform2D &p_transform) {

    CanvasItem *pi = get_parent_item();
    if (pi)
        set_transform(pi->get_global_transform().affine_inverse() * p_transform);
    else
        set_transform(p_transform);
}

void Node2D::set_z_index(int p_z) {

    ERR_FAIL_COND(p_z < RS::CANVAS_ITEM_Z_MIN);
    ERR_FAIL_COND(p_z > RS::CANVAS_ITEM_Z_MAX);
    z_index = p_z;
    RenderingServer::get_singleton()->canvas_item_set_z_index(get_canvas_item(), z_index);
    Object_change_notify(this,"z_index");
}

void Node2D::set_z_as_relative(bool p_enabled) {

    if (z_relative == p_enabled)
        return;
    z_relative = p_enabled;
    RenderingServer::get_singleton()->canvas_item_set_z_as_relative_to_parent(get_canvas_item(), p_enabled);
}

bool Node2D::is_z_relative() const {

    return z_relative;
}

int Node2D::get_z_index() const {

    return z_index;
}

Transform2D Node2D::get_relative_transform_to_parent(const Node *p_parent) const {

    if (p_parent == this)
        return Transform2D();

    Node2D *parent_2d = object_cast<Node2D>(get_parent());

    ERR_FAIL_COND_V(!parent_2d, Transform2D());
    if (p_parent == parent_2d)
        return get_transform();
    else
        return parent_2d->get_relative_transform_to_parent(p_parent) * get_transform();
}

void Node2D::look_at(const Vector2 &p_pos) {

    rotate(get_angle_to(p_pos));
}

float Node2D::get_angle_to(const Vector2 &p_pos) const {

    return (to_local(p_pos) * get_scale()).angle();
}

Point2 Node2D::to_local(Point2 p_global) const {

    return get_global_transform().affine_inverse().xform(p_global);
}

Point2 Node2D::to_global(Point2 p_local) const {

    return get_global_transform().xform(p_local);
}

void Node2D::_bind_methods() {

    SE_BIND_METHOD(Node2D,set_position);
    SE_BIND_METHOD(Node2D,set_rotation);
    SE_BIND_METHOD(Node2D,set_rotation_degrees);
    SE_BIND_METHOD(Node2D,set_skew);
    SE_BIND_METHOD(Node2D,set_skew_degrees);

    SE_BIND_METHOD(Node2D,set_scale);

    SE_BIND_METHOD(Node2D,get_position);
    SE_BIND_METHOD(Node2D,get_rotation);
    SE_BIND_METHOD(Node2D,get_rotation_degrees);
    SE_BIND_METHOD(Node2D,get_skew);
    SE_BIND_METHOD(Node2D,get_skew_degrees);

    SE_BIND_METHOD(Node2D,get_scale);

    SE_BIND_METHOD(Node2D,rotate);
    MethodBinder::bind_method(D_METHOD("move_local_x", {"delta", "scaled"}), &Node2D::move_x, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("move_local_y", {"delta", "scaled"}), &Node2D::move_y, {DEFVAL(false)});
    SE_BIND_METHOD(Node2D,translate);
    SE_BIND_METHOD(Node2D,global_translate);
    SE_BIND_METHOD(Node2D,apply_scale);

    SE_BIND_METHOD(Node2D,set_global_position);
    SE_BIND_METHOD(Node2D,get_global_position);
    SE_BIND_METHOD(Node2D,set_global_rotation);
    SE_BIND_METHOD(Node2D,get_global_rotation);
    SE_BIND_METHOD(Node2D,set_global_rotation_degrees);
    SE_BIND_METHOD(Node2D,get_global_rotation_degrees);
    SE_BIND_METHOD(Node2D,set_global_scale);
    SE_BIND_METHOD(Node2D,get_global_scale);

    SE_BIND_METHOD(Node2D,set_transform);
    SE_BIND_METHOD(Node2D,set_global_transform);

    SE_BIND_METHOD(Node2D,look_at);
    SE_BIND_METHOD(Node2D,get_angle_to);

    SE_BIND_METHOD(Node2D,to_local);
    SE_BIND_METHOD(Node2D,to_global);

    SE_BIND_METHOD(Node2D,set_z_index);
    SE_BIND_METHOD(Node2D,get_z_index);

    SE_BIND_METHOD(Node2D,set_z_as_relative);
    SE_BIND_METHOD(Node2D,is_z_relative);

    SE_BIND_METHOD(Node2D,get_relative_transform_to_parent);

    ADD_GROUP("Transform", "");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "position"), "set_position", "get_position");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "rotation", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_rotation", "get_rotation");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "rotation_degrees", PropertyHint::Range, "-360,360,0.1,or_lesser,or_greater", PROPERTY_USAGE_EDITOR), "set_rotation_degrees", "get_rotation_degrees");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "scale"), "set_scale", "get_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "skew", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_skew", "get_skew");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "skew_degrees", PropertyHint::Range, "-89.9,89.9,0.1", PROPERTY_USAGE_EDITOR), "set_skew_degrees", "get_skew_degrees");

    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM2D, "transform", PropertyHint::None, "", 0), "set_transform", "get_transform");

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "global_position", PropertyHint::None, "", 0), "set_global_position", "get_global_position");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "global_rotation", PropertyHint::None, "", 0), "set_global_rotation", "get_global_rotation");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "global_rotation_degrees", PropertyHint::None, "", 0), "set_global_rotation_degrees", "get_global_rotation_degrees");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "global_scale", PropertyHint::None, "", 0), "set_global_scale", "get_global_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM2D, "global_transform", PropertyHint::None, "", 0), "set_global_transform", "get_global_transform");

    ADD_GROUP("Z Index", "");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "z_index", PropertyHint::Range, itos(RS::CANVAS_ITEM_Z_MIN) + "," + itos(RS::CANVAS_ITEM_Z_MAX) + ",1"), "set_z_index", "get_z_index");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "z_as_relative"), "set_z_as_relative", "is_z_relative");
}

#ifdef TOOLS_ENABLED
StringName Node2D::get_property_store_alias(const StringName &p_property) const {
    if (p_property == "rotation_degrees") {
        return "rotation";
    } else {
        return Node::get_property_store_alias(p_property);
    }
}
#endif
Node2D::Node2D() {

}
