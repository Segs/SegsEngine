/*************************************************************************/
/*  line_2d.cpp                                                          */
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

#include "line_2d.h"
#include "line_builder.h"

#include "core/method_bind.h"
#include "core/core_string_names.h"
#include "scene/resources/curve.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(Line2D)

// Needed so we can bind functions
VARIANT_ENUM_CAST(Line2D::LineJointMode)
VARIANT_ENUM_CAST(Line2D::LineCapMode)
VARIANT_ENUM_CAST(Line2D::LineTextureMode)

#ifdef TOOLS_ENABLED
Rect2 Line2D::_edit_get_rect() const {

    if (_points.size() == 0)
        return Rect2(0, 0, 0, 0);
    Vector2 d = Vector2(_width, _width);
    Rect2 aabb = Rect2(_points[0] - d, 2 * d);
    for (int i = 1; i < _points.size(); i++) {
        aabb.expand_to(_points[i] - d);
        aabb.expand_to(_points[i] + d);
    }
    return aabb;
}

bool Line2D::_edit_use_rect() const {
    return true;
}

bool Line2D::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {

    const real_t d = _width / 2 + p_tolerance;
    PoolVector<Vector2>::Read points = _points.read();
    for (int i = 0; i < _points.size() - 1; i++) {
        Vector2 p = Geometry::get_closest_point_to_segment_2d(p_point, &points[i]);
        if (p.distance_to(p_point) <= d)
            return true;
    }

    return false;
}
#endif

Line2D::Line2D() {
    _joint_mode = LINE_JOINT_SHARP;
    _begin_cap_mode = LINE_CAP_NONE;
    _end_cap_mode = LINE_CAP_NONE;
    _width = 10;
    _default_color = Color(0.4f, 0.5f, 1);
    _texture_mode = LINE_TEXTURE_NONE;
    _sharp_limit = 2.f;
    _round_precision = 8;
    _antialiased = false;

}

void Line2D::set_points(const PoolVector<Vector2> &p_points) {
    _points = p_points;
    update();
}

void Line2D::set_width(float p_width) {
    if (p_width < 0.0f)
        p_width = 0.0;
    _width = p_width;
    update();
}

float Line2D::get_width() const {
    return _width;
}

void Line2D::set_curve(const Ref<Curve> &p_curve) {
    // Cleanup previous connection if any
    if (_curve) {
        _curve->disconnect(CoreStringNames::get_singleton()->changed, this, "_curve_changed");
    }

    _curve = p_curve;

    // Connect to the curve so the line will update when it is changed
    if (_curve) {
        _curve->connect(CoreStringNames::get_singleton()->changed, this, "_curve_changed");
    }

    update();
}

Ref<Curve> Line2D::get_curve() const {
    return _curve;
}

PoolVector<Vector2> Line2D::get_points() const {
    return _points;
}

void Line2D::set_point_position(int i, Vector2 p_pos) {
    _points.set(i, p_pos);
    update();
}

Vector2 Line2D::get_point_position(int i) const {
    ERR_FAIL_INDEX_V(i, _points.size(), Vector2());
    return _points.get(i);
}

int Line2D::get_point_count() const {
    return _points.size();
}

void Line2D::clear_points() {
    int count = _points.size();
    if (count > 0) {
        _points.resize(0);
        update();
    }
}

void Line2D::add_point(Vector2 p_pos, int p_atpos) {
    if (p_atpos < 0 || _points.size() < p_atpos) {
        _points.append(p_pos);
    } else {
        _points.insert(p_atpos, p_pos);
    }
    update();
}

void Line2D::remove_point(int i) {
    _points.remove(i);
    update();
}

void Line2D::set_default_color(Color p_color) {
    _default_color = p_color;
    update();
}

Color Line2D::get_default_color() const {
    return _default_color;
}

void Line2D::set_gradient(const Ref<Gradient> &p_gradient) {

    // Cleanup previous connection if any
    if (_gradient) {
        _gradient->disconnect(CoreStringNames::get_singleton()->changed, this, "_gradient_changed");
    }

    _gradient = p_gradient;

    // Connect to the gradient so the line will update when the ColorRamp is changed
    if (_gradient) {
        _gradient->connect(CoreStringNames::get_singleton()->changed, this, "_gradient_changed");
    }

    update();
}

Ref<Gradient> Line2D::get_gradient() const {
    return _gradient;
}

void Line2D::set_texture(const Ref<Texture> &p_texture) {
    _texture = p_texture;
    update();
}

Ref<Texture> Line2D::get_texture() const {
    return _texture;
}

void Line2D::set_texture_mode(const LineTextureMode p_mode) {
    _texture_mode = p_mode;
    update();
}

Line2D::LineTextureMode Line2D::get_texture_mode() const {
    return _texture_mode;
}

void Line2D::set_joint_mode(LineJointMode p_mode) {
    _joint_mode = p_mode;
    update();
}

Line2D::LineJointMode Line2D::get_joint_mode() const {
    return _joint_mode;
}

void Line2D::set_begin_cap_mode(LineCapMode p_mode) {
    _begin_cap_mode = p_mode;
    update();
}

Line2D::LineCapMode Line2D::get_begin_cap_mode() const {
    return _begin_cap_mode;
}

void Line2D::set_end_cap_mode(LineCapMode p_mode) {
    _end_cap_mode = p_mode;
    update();
}

Line2D::LineCapMode Line2D::get_end_cap_mode() const {
    return _end_cap_mode;
}

void Line2D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_DRAW:
            _draw();
            break;
    }
}

void Line2D::set_sharp_limit(float p_limit) {
    if (p_limit < 0.f)
        p_limit = 0.f;
    _sharp_limit = p_limit;
    update();
}

float Line2D::get_sharp_limit() const {
    return _sharp_limit;
}

void Line2D::set_round_precision(int p_precision) {
    if (p_precision < 1)
        p_precision = 1;
    _round_precision = p_precision;
    update();
}

int Line2D::get_round_precision() const {
    return _round_precision;
}

void Line2D::set_antialiased(bool p_antialiased) {
    _antialiased = p_antialiased;
    update();
}

bool Line2D::get_antialiased() const {
    return _antialiased;
}

void Line2D::_draw() {
    if (_points.size() <= 1 || _width == 0.f)
        return;

    // TODO Is this really needed?
    // Copy points for faster access
    Vector<Vector2> points;
    points.resize(_points.size());
    int len = points.size();
    {
        PoolVector<Vector2>::Read points_read = _points.read();
        for (int i = 0; i < len; ++i) {
            points[i] = points_read[i];
        }
    }

    // TODO Maybe have it as member rather than copying parameters and allocating memory?
    LineBuilder lb;
    lb.points = eastl::move(points);
    lb.default_color = _default_color;
    lb.gradient = _gradient.get();
    lb.texture_mode = _texture_mode;
    lb.joint_mode = _joint_mode;
    lb.begin_cap_mode = _begin_cap_mode;
    lb.end_cap_mode = _end_cap_mode;
    lb.round_precision = _round_precision;
    lb.sharp_limit = _sharp_limit;
    lb.width = _width;
    lb.curve = _curve.get();

    RID texture_rid;
    if (_texture) {
        texture_rid = _texture->get_rid();

        lb.tile_aspect = _texture->get_size().aspect();
    }

    lb.build();

    RenderingServer::get_singleton()->canvas_item_add_triangle_array(
                get_canvas_item(),
                lb.indices,
                lb.vertices,
                lb.colors,
                lb.uvs, {}, {},
                texture_rid, -1, RID(),
                _antialiased,true);

    // DEBUG
    // Draw wireframe
    //	if(lb.indices.size() % 3 == 0) {
    //		Color col(0,0,0);
    //		for(int i = 0; i < lb.indices.size(); i += 3) {
    //			int vi = lb.indices[i];
    //			int lbvsize = lb.vertices.size();
    //			Vector2 a = lb.vertices[lb.indices[i]];
    //			Vector2 b = lb.vertices[lb.indices[i+1]];
    //			Vector2 c = lb.vertices[lb.indices[i+2]];
    //			draw_line(a, b, col);
    //			draw_line(b, c, col);
    //			draw_line(c, a, col);
    //		}
    //		for(int i = 0; i < lb.vertices.size(); ++i) {
    //			Vector2 p = lb.vertices[i];
    //			draw_rect(Rect2(p.x-1, p.y-1, 2, 2), Color(0,0,0,0.5));
    //		}
    //	}
}

void Line2D::_gradient_changed() {
    update();
}

void Line2D::_curve_changed() {
    update();
}

// static
void Line2D::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_points", {"points"}), &Line2D::set_points);
    MethodBinder::bind_method(D_METHOD("get_points"), &Line2D::get_points);

    MethodBinder::bind_method(D_METHOD("set_point_position", {"i", "position"}), &Line2D::set_point_position);
    MethodBinder::bind_method(D_METHOD("get_point_position", {"i"}), &Line2D::get_point_position);

    MethodBinder::bind_method(D_METHOD("get_point_count"), &Line2D::get_point_count);

    MethodBinder::bind_method(D_METHOD("add_point", {"position", "at_position"}), &Line2D::add_point, {DEFVAL(-1)});
    MethodBinder::bind_method(D_METHOD("remove_point", {"i"}), &Line2D::remove_point);

    MethodBinder::bind_method(D_METHOD("clear_points"), &Line2D::clear_points);

    MethodBinder::bind_method(D_METHOD("set_width", {"width"}), &Line2D::set_width);
    MethodBinder::bind_method(D_METHOD("get_width"), &Line2D::get_width);

    MethodBinder::bind_method(D_METHOD("set_curve", {"curve"}), &Line2D::set_curve);
    MethodBinder::bind_method(D_METHOD("get_curve"), &Line2D::get_curve);

    MethodBinder::bind_method(D_METHOD("set_default_color", {"color"}), &Line2D::set_default_color);
    MethodBinder::bind_method(D_METHOD("get_default_color"), &Line2D::get_default_color);

    MethodBinder::bind_method(D_METHOD("set_gradient", {"color"}), &Line2D::set_gradient);
    MethodBinder::bind_method(D_METHOD("get_gradient"), &Line2D::get_gradient);

    MethodBinder::bind_method(D_METHOD("set_texture", {"texture"}), &Line2D::set_texture);
    MethodBinder::bind_method(D_METHOD("get_texture"), &Line2D::get_texture);

    MethodBinder::bind_method(D_METHOD("set_texture_mode", {"mode"}), &Line2D::set_texture_mode);
    MethodBinder::bind_method(D_METHOD("get_texture_mode"), &Line2D::get_texture_mode);

    MethodBinder::bind_method(D_METHOD("set_joint_mode", {"mode"}), &Line2D::set_joint_mode);
    MethodBinder::bind_method(D_METHOD("get_joint_mode"), &Line2D::get_joint_mode);

    MethodBinder::bind_method(D_METHOD("set_begin_cap_mode", {"mode"}), &Line2D::set_begin_cap_mode);
    MethodBinder::bind_method(D_METHOD("get_begin_cap_mode"), &Line2D::get_begin_cap_mode);

    MethodBinder::bind_method(D_METHOD("set_end_cap_mode", {"mode"}), &Line2D::set_end_cap_mode);
    MethodBinder::bind_method(D_METHOD("get_end_cap_mode"), &Line2D::get_end_cap_mode);

    MethodBinder::bind_method(D_METHOD("set_sharp_limit", {"limit"}), &Line2D::set_sharp_limit);
    MethodBinder::bind_method(D_METHOD("get_sharp_limit"), &Line2D::get_sharp_limit);

    MethodBinder::bind_method(D_METHOD("set_round_precision", {"precision"}), &Line2D::set_round_precision);
    MethodBinder::bind_method(D_METHOD("get_round_precision"), &Line2D::get_round_precision);

    MethodBinder::bind_method(D_METHOD("set_antialiased", {"antialiased"}), &Line2D::set_antialiased);
    MethodBinder::bind_method(D_METHOD("get_antialiased"), &Line2D::get_antialiased);

    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR2_ARRAY, "points"), "set_points", "get_points");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "width"), "set_width", "get_width");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "width_curve", PropertyHint::ResourceType, "Curve"), "set_curve", "get_curve");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "default_color"), "set_default_color", "get_default_color");
    ADD_GROUP("Fill", "");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "gradient", PropertyHint::ResourceType, "Gradient"), "set_gradient", "get_gradient");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "texture_mode", PropertyHint::Enum, "None,Tile,Stretch"), "set_texture_mode", "get_texture_mode");
    ADD_GROUP("Capping", "");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "joint_mode", PropertyHint::Enum, "Sharp,Bevel,Round"), "set_joint_mode", "get_joint_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "begin_cap_mode", PropertyHint::Enum, "None,Box,Round"), "set_begin_cap_mode", "get_begin_cap_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "end_cap_mode", PropertyHint::Enum, "None,Box,Round"), "set_end_cap_mode", "get_end_cap_mode");
    ADD_GROUP("Border", "");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "sharp_limit"), "set_sharp_limit", "get_sharp_limit");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "round_precision"), "set_round_precision", "get_round_precision");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "antialiased"), "set_antialiased", "get_antialiased");

    BIND_ENUM_CONSTANT(LINE_JOINT_SHARP)
    BIND_ENUM_CONSTANT(LINE_JOINT_BEVEL)
    BIND_ENUM_CONSTANT(LINE_JOINT_ROUND)

    BIND_ENUM_CONSTANT(LINE_CAP_NONE)
    BIND_ENUM_CONSTANT(LINE_CAP_BOX)
    BIND_ENUM_CONSTANT(LINE_CAP_ROUND)

    BIND_ENUM_CONSTANT(LINE_TEXTURE_NONE)
    BIND_ENUM_CONSTANT(LINE_TEXTURE_TILE)
    BIND_ENUM_CONSTANT(LINE_TEXTURE_STRETCH)

    MethodBinder::bind_method(D_METHOD("_gradient_changed"), &Line2D::_gradient_changed);
    MethodBinder::bind_method(D_METHOD("_curve_changed"), &Line2D::_curve_changed);
}
