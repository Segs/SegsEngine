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

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/core_string_names.h"
#include "scene/resources/curve.h"
#include "servers/rendering_server.h"

#include "core/ecs_registry.h"
IMPL_GDCLASS(Line2D)

// Needed so we can bind functions
VARIANT_ENUM_CAST(Line2DJointMode);
VARIANT_ENUM_CAST(Line2DCapMode);
VARIANT_ENUM_CAST(Line2DTextureMode);

#ifdef TOOLS_ENABLED
Rect2 Line2D::_edit_get_rect() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));

    if (data._points.empty())
        return Rect2(0, 0, 0, 0);
    Vector2 d = Vector2(data._width, data._width);
    Rect2 aabb = Rect2(data._points[0] - d, 2 * d);
    for (int i = 1; i < data._points.size(); i++) {
        aabb.expand_to(data._points[i] - d);
        aabb.expand_to(data._points[i] + d);
    }
    return aabb;
}

bool Line2D::_edit_use_rect() const {
    return true;
}

bool Line2D::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));

    const real_t d = data._width / 2 + p_tolerance;
    auto & points = data._points;
    for (int i = 0; i < data._points.size() - 1; i++) {
        Vector2 p = Geometry::get_closest_point_to_segment_2d(p_point, &points[i]);
        if (p.distance_to(p_point) <= d)
            return true;
    }

    return false;
}
#endif

Line2D::Line2D() {
    auto &data(game_object_registry.registry.emplace<Line2DDrawableComponent>(get_instance_id()));


    data._joint_mode = Line2DJointMode::LINE_JOINT_SHARP;
    data._begin_cap_mode = Line2DCapMode::LINE_CAP_NONE;
    data._end_cap_mode = Line2DCapMode::LINE_CAP_NONE;
    data._width = 10;
    data._curve = nullptr;
    data._gradient = nullptr;
    data._default_color = Color(0.4f, 0.5f, 1);
    data._texture_mode = Line2DTextureMode::LINE_TEXTURE_NONE;
    data._sharp_limit = 2.f;
    data._round_precision = 8;
    data._antialiased = false;

}

void Line2D::set_points(Span<const Vector2> p_points) {
    game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id())._points.assign(p_points.begin(),p_points.end());
    update();
}

void Line2D::set_width(float p_width) {
    if (p_width < 0.0f)
        p_width = 0.0;
    game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id())._width = p_width;
    update();
}

float Line2D::get_width() const {
    return game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id())._width;
}

void Line2D::set_curve(const Ref<Curve> &p_curve) {
    // Cleanup previous connection if any
    if (game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id())._curve) {
        game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id())._curve->disconnect(CoreStringNames::get_singleton()->changed, callable_mp(this, &Line2D::_curve_changed));
    }

    game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id())._curve = p_curve;

    // Connect to the curve so the line will update when it is changed
    if (game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id())._curve) {
        game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id())._curve->connect(CoreStringNames::get_singleton()->changed, callable_mp(this, &Line2D::_curve_changed));
    }

    update();
}

Ref<Curve> Line2D::get_curve() const {
    return game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id())._curve;
}

Span<const Vector2> Line2D::get_points() const {
    return game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id())._points;
}

void Line2D::set_point_position(int i, Vector2 p_pos) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    ERR_FAIL_INDEX(i, data._points.size());
    data._points[i] = p_pos;
    update();
}

Vector2 Line2D::get_point_position(int i) const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    ERR_FAIL_INDEX_V(i, data._points.size(), Vector2());
    return data._points[i];
}

int Line2D::get_point_count() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._points.size();
}

void Line2D::clear_points() {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    int count = data._points.size();
    if (count > 0) {
        data._points.resize(0);
        update();
    }
}

void Line2D::add_point(Vector2 p_pos, int p_atpos) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    if (p_atpos < 0 || data._points.size() < p_atpos) {
        data._points.push_back(p_pos);
    } else {
        data._points.insert_at(p_atpos, p_pos);
    }
    update();
}

void Line2D::remove_point(int i) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    data._points.erase_at(i);
    update();
}

void Line2D::set_default_color(Color p_color) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    data._default_color = p_color;
    update();
}

Color Line2D::get_default_color() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._default_color;
}

void Line2D::set_gradient(const Ref<Gradient> &p_gradient) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));

    // Cleanup previous connection if any
    if (data._gradient) {
        data._gradient->disconnect(CoreStringNames::get_singleton()->changed, callable_mp(this, &Line2D::_gradient_changed));
    }

    data._gradient = p_gradient;

    // Connect to the gradient so the line will update when the ColorRamp is changed
    if (data._gradient) {
        data._gradient->connect(CoreStringNames::get_singleton()->changed, callable_mp(this, &Line2D::_gradient_changed));
    }

    update();
}

Ref<Gradient> Line2D::get_gradient() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._gradient;
}

void Line2D::set_texture(const Ref<Texture> &p_texture) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    data._texture = p_texture;
    update();
}

Ref<Texture> Line2D::get_texture() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._texture;
}

void Line2D::set_texture_mode(const Line2DTextureMode p_mode) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    data._texture_mode = p_mode;
    update();
}

Line2DTextureMode Line2D::get_texture_mode() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._texture_mode;
}

void Line2D::set_joint_mode(Line2DJointMode p_mode) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    data._joint_mode = p_mode;
    update();
}

Line2DJointMode Line2D::get_joint_mode() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._joint_mode;
}

void Line2D::set_begin_cap_mode(Line2DCapMode p_mode) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    data._begin_cap_mode = p_mode;
    update();
}

Line2DCapMode Line2D::get_begin_cap_mode() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._begin_cap_mode;
}

void Line2D::set_end_cap_mode(Line2DCapMode p_mode) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    data._end_cap_mode = p_mode;
    update();
}

Line2DCapMode Line2D::get_end_cap_mode() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._end_cap_mode;
}

void Line2D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_DRAW:
            _draw();
            break;
    }
}

void Line2D::set_sharp_limit(float p_limit) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    if (p_limit < 0.f)
        p_limit = 0.f;
    data._sharp_limit = p_limit;
    update();
}

float Line2D::get_sharp_limit() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._sharp_limit;
}

void Line2D::set_round_precision(int p_precision) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    data._round_precision = M_MAX(1, p_precision);
    update();
}

int Line2D::get_round_precision() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._round_precision;
}

void Line2D::set_antialiased(bool p_antialiased) {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    data._antialiased = p_antialiased;
    update();
}

bool Line2D::get_antialiased() const {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    return data._antialiased;
}

void Line2D::_draw() {
    auto &data(game_object_registry.registry.get<Line2DDrawableComponent>(get_instance_id()));
    if (data._points.size() <= 1 || data._width == 0.f)
        return;


    RenderingEntity texture_rid = entt::null;
    if (data._texture) {
        texture_rid = data._texture->get_rid();
    }

    LineBuildOutput output;

    build_2d_line_buffers({&data,1},{&output,1});
    RenderingServer::get_singleton()->canvas_item_add_triangle_array(
                get_canvas_item(),
                output.indices,
                output.vertices,
                output.colors,
                output.uvs, {}, {},
                texture_rid, -1, entt::null,
                data._antialiased,true);
}

void Line2D::_gradient_changed() {
    update();
}

void Line2D::_curve_changed() {
    update();
}

// static
void Line2D::_bind_methods() {

    SE_BIND_METHOD(Line2D,set_points);
    SE_BIND_METHOD(Line2D,get_points);

    SE_BIND_METHOD(Line2D,set_point_position);
    SE_BIND_METHOD(Line2D,get_point_position);

    SE_BIND_METHOD(Line2D,get_point_count);

    MethodBinder::bind_method(D_METHOD("add_point", {"position", "at_position"}), &Line2D::add_point, {DEFVAL(-1)});
    SE_BIND_METHOD(Line2D,remove_point);

    SE_BIND_METHOD(Line2D,clear_points);

    SE_BIND_METHOD(Line2D,set_width);
    SE_BIND_METHOD(Line2D,get_width);

    SE_BIND_METHOD(Line2D,set_curve);
    SE_BIND_METHOD(Line2D,get_curve);

    SE_BIND_METHOD(Line2D,set_default_color);
    SE_BIND_METHOD(Line2D,get_default_color);

    SE_BIND_METHOD(Line2D,set_gradient);
    SE_BIND_METHOD(Line2D,get_gradient);

    SE_BIND_METHOD(Line2D,set_texture);
    SE_BIND_METHOD(Line2D,get_texture);

    SE_BIND_METHOD(Line2D,set_texture_mode);
    SE_BIND_METHOD(Line2D,get_texture_mode);

    SE_BIND_METHOD(Line2D,set_joint_mode);
    SE_BIND_METHOD(Line2D,get_joint_mode);

    SE_BIND_METHOD(Line2D,set_begin_cap_mode);
    SE_BIND_METHOD(Line2D,get_begin_cap_mode);

    SE_BIND_METHOD(Line2D,set_end_cap_mode);
    SE_BIND_METHOD(Line2D,get_end_cap_mode);

    SE_BIND_METHOD(Line2D,set_sharp_limit);
    SE_BIND_METHOD(Line2D,get_sharp_limit);

    SE_BIND_METHOD(Line2D,set_round_precision);
    SE_BIND_METHOD(Line2D,get_round_precision);

    SE_BIND_METHOD(Line2D,set_antialiased);
    SE_BIND_METHOD(Line2D,get_antialiased);

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
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "round_precision", PropertyHint::Range, "1,32,1"), "set_round_precision", "get_round_precision");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "antialiased"), "set_antialiased", "get_antialiased");

    BIND_GLOBAL_ENUM_CONSTANT(Line2DJointMode::LINE_JOINT_SHARP);
    BIND_GLOBAL_ENUM_CONSTANT(Line2DJointMode::LINE_JOINT_BEVEL);
    BIND_GLOBAL_ENUM_CONSTANT(Line2DJointMode::LINE_JOINT_ROUND);


    BIND_GLOBAL_ENUM_CONSTANT(Line2DCapMode::LINE_CAP_NONE);
    BIND_GLOBAL_ENUM_CONSTANT(Line2DCapMode::LINE_CAP_BOX);
    BIND_GLOBAL_ENUM_CONSTANT(Line2DCapMode::LINE_CAP_ROUND);


    BIND_GLOBAL_ENUM_CONSTANT(Line2DTextureMode::LINE_TEXTURE_NONE);
    BIND_GLOBAL_ENUM_CONSTANT(Line2DTextureMode::LINE_TEXTURE_TILE);
    BIND_GLOBAL_ENUM_CONSTANT(Line2DTextureMode::LINE_TEXTURE_STRETCH);
}
