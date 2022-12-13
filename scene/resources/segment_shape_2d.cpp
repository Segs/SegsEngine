/*************************************************************************/
/*  segment_shape_2d.cpp                                                 */
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

#include "segment_shape_2d.h"

#include "core/dictionary.h"
#include "core/math/geometry.h"
#include "core/method_bind.h"
#include "servers/physics_server_2d.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(SegmentShape2D)
IMPL_GDCLASS(RayShape2D)

#ifdef TOOLS_ENABLED
bool SegmentShape2D::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {

    const Vector2 l[2] = { a, b };
    const Vector2 closest = Geometry::get_closest_point_to_segment_2d(p_point, l);
    return p_point.distance_to(closest) < p_tolerance;
}
#endif

void SegmentShape2D::_update_shape() {

    Rect2 r;
    r.position = a;
    r.size = b;
    PhysicsServer2D::get_singleton()->shape_set_data(get_phys_rid(), r);
    emit_changed();
}

void SegmentShape2D::set_a(const Vector2 &p_a) {

    a = p_a;
    _update_shape();
}
Vector2 SegmentShape2D::get_a() const {

    return a;
}

void SegmentShape2D::set_b(const Vector2 &p_b) {

    b = p_b;
    _update_shape();
}
Vector2 SegmentShape2D::get_b() const {

    return b;
}

void SegmentShape2D::draw(RenderingEntity p_to_rid, const Color &p_color) {

    RenderingServer::get_singleton()->canvas_item_add_line(p_to_rid, a, b, p_color, 3);
}

Rect2 SegmentShape2D::get_rect() const {

    Rect2 rect;
    rect.position = a;
    rect.expand_to(b);
    return rect;
}

void SegmentShape2D::_bind_methods() {

    SE_BIND_METHOD(SegmentShape2D,set_a);
    SE_BIND_METHOD(SegmentShape2D,get_a);

    SE_BIND_METHOD(SegmentShape2D,set_b);
    SE_BIND_METHOD(SegmentShape2D,get_b);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "a"), "set_a", "get_a");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "b"), "set_b", "get_b");
}

SegmentShape2D::SegmentShape2D() :
        Shape2D(PhysicsServer2D::get_singleton()->segment_shape_create()) {

    a = Vector2();
    b = Vector2(0, 10);
    _update_shape();
}

////////////////////////////////////////////////////////////

void RayShape2D::_update_shape() {

    Dictionary d;
    d["length"] = length;
    d["slips_on_slope"] = slips_on_slope;
    PhysicsServer2D::get_singleton()->shape_set_data(get_phys_rid(), d);
    emit_changed();
}

void RayShape2D::draw(RenderingEntity p_to_rid, const Color &p_color) {

    Vector2 tip = Vector2(0, get_length());
    RenderingServer::get_singleton()->canvas_item_add_line(p_to_rid, Vector2(), tip, p_color, 3);
    Vector<Vector2> pts;
    float tsize = 4;
    pts.push_back(tip + Vector2(0, tsize));
    pts.push_back(tip + Vector2(0.707f * tsize, 0));
    pts.push_back(tip + Vector2(-0.707f * tsize, 0));
    const Color cols[3] {p_color,p_color,p_color};

    RenderingServer::get_singleton()->canvas_item_add_primitive(p_to_rid, pts, cols, {}, entt::null);
}

Rect2 RayShape2D::get_rect() const {

    Rect2 rect;
    rect.position = Vector2();
    rect.expand_to(Vector2(0, length));
    rect.grow_by(0.707 * 4);
    return rect;
}

void RayShape2D::_bind_methods() {

    SE_BIND_METHOD(RayShape2D,set_length);
    SE_BIND_METHOD(RayShape2D,get_length);

    SE_BIND_METHOD(RayShape2D,set_slips_on_slope);
    SE_BIND_METHOD(RayShape2D,get_slips_on_slope);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "length", PropertyHint::Range, "0.01,1024,0.01,or_greater"),
            "set_length", "get_length");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "slips_on_slope"), "set_slips_on_slope", "get_slips_on_slope");
}

void RayShape2D::set_length(real_t p_length) {

    length = p_length;
    _update_shape();
}
real_t RayShape2D::get_length() const {

    return length;
}

void RayShape2D::set_slips_on_slope(bool p_active) {

    slips_on_slope = p_active;
    _update_shape();
}

bool RayShape2D::get_slips_on_slope() const {
    return slips_on_slope;
}

RayShape2D::RayShape2D() :
        Shape2D(PhysicsServer2D::get_singleton()->ray_shape_create()) {

    length = 20;
    slips_on_slope = false;
    _update_shape();
}
