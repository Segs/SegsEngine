/*************************************************************************/
/*  collision_polygon_2d.cpp                                             */
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

#include "collision_polygon_2d.h"

#include "collision_object_2d.h"
#include "core/engine.h"
#include "core/math/geometry.h"
#include "core/method_bind.h"
#include "core/translation_helpers.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/concave_polygon_shape_2d.h"
#include "scene/resources/convex_polygon_shape_2d.h"

#include "thirdparty/misc/triangulator.h"

IMPL_GDCLASS(CollisionPolygon2D)
VARIANT_ENUM_CAST(CollisionPolygon2D::BuildMode);

void CollisionPolygon2D::_build_polygon() {

    parent->shape_owner_clear_shapes(owner_id);


    bool solids = build_mode == BUILD_SOLIDS;

    if (solids) {

        if (polygon.size() < 3) {
            return;
        }
        //here comes the sun, lalalala
        //decompose concave into multiple convex polygons and add them
        Vector<Vector<Vector2> > decomp = _decompose_in_convex();
        for (size_t i = 0; i < decomp.size(); i++) {
            Ref<ConvexPolygonShape2D> convex(make_ref_counted<ConvexPolygonShape2D>());
            convex->set_points(decomp[i]);
            parent->shape_owner_add_shape(owner_id, convex);
        }

    } else {
        if (polygon.size() < 2) {
            return;
        }

        Ref<ConcavePolygonShape2D> concave(make_ref_counted<ConcavePolygonShape2D>());

        PoolVector<Vector2> segments;
        segments.resize(polygon.size() * 2);
        PoolVector<Vector2>::Write w = segments.write();

        for (size_t i = 0; i < polygon.size(); i++) {
            w[(i << 1) + 0] = polygon[i];
            w[(i << 1) + 1] = polygon[(i + 1) % polygon.size()];
        }

        w.release();
        concave->set_segments(segments);

        parent->shape_owner_add_shape(owner_id, concave);
    }
}

Vector<Vector<Vector2> > CollisionPolygon2D::_decompose_in_convex() {
    Vector<Vector<Vector2> > decomp = Geometry::decompose_polygon_in_convex(polygon);
    return decomp;
}

void CollisionPolygon2D::_update_in_shape_owner(bool p_xform_only) {

    parent->shape_owner_set_transform(owner_id, get_transform());
    if (p_xform_only)
        return;
    parent->shape_owner_set_disabled(owner_id, disabled);
    parent->shape_owner_set_one_way_collision(owner_id, one_way_collision);
    parent->shape_owner_set_one_way_collision_margin(owner_id, one_way_collision_margin);
}

void CollisionPolygon2D::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_PARENTED: {

            parent = object_cast<CollisionObject2D>(get_parent());
            if (parent) {
                owner_id = parent->create_shape_owner(this);
                _build_polygon();
                _update_in_shape_owner();
            }

            /*if (Engine::get_singleton()->is_editor_hint()) {
                //display above all else
                set_z_as_relative(false);
                set_z_index(RS::CANVAS_ITEM_Z_MAX - 1);
            }*/

        } break;
        case NOTIFICATION_ENTER_TREE: {

            if (parent) {
                _update_in_shape_owner();
            }

        } break;
        case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {

            if (parent) {
                _update_in_shape_owner(true);
            }

        } break;
        case NOTIFICATION_UNPARENTED: {
            if (parent) {
                parent->remove_shape_owner(owner_id);
            }
            owner_id = 0;
            parent = nullptr;
        } break;

        case NOTIFICATION_DRAW: {
            ERR_FAIL_COND(!is_inside_tree());

            if (!Engine::get_singleton()->is_editor_hint() && !get_tree()->is_debugging_collisions_hint()) {
                break;
            }

            for (int i = 0; i < polygon.size(); i++) {

                Vector2 p = polygon[i];
                Vector2 n = polygon[(i + 1) % polygon.size()];
                // draw line with width <= 1, so it does not scale with zoom and break pixel exact editing
                draw_line(p, n, Color(0.9f, 0.2f, 0.0, 0.8f), 1);
            }
            if (polygon.size() > 2) {
#define DEBUG_DECOMPOSE
#if defined(TOOLS_ENABLED) && defined(DEBUG_DECOMPOSE)

            Vector<Vector<Vector2> > decomp = _decompose_in_convex();

            Color c(0.4f, 0.9f, 0.1f);
            for (int i = 0; i < decomp.size(); i++) {

                c.set_hsv(Math::fmod(c.get_h() + 0.738f, 1), c.get_s(), c.get_v(), 0.5);
                draw_colored_polygon(decomp[i], c);
            }
#else
            draw_colored_polygon(polygon, get_tree()->get_debug_collisions_color());
#endif

            }
            if (one_way_collision) {
                Color dcol = get_tree()->get_debug_collisions_color(); //0.9,0.2,0.2,0.4);
                dcol.a = 1.0;
                Vector2 line_to(0, 20);
                draw_line(Vector2(), line_to, dcol, 3);
                Vector<Vector2> pts;
                float tsize = 8;
                pts.push_back(line_to + (Vector2(0, tsize)));
                pts.push_back(line_to + (Vector2(0.707f * tsize, 0)));
                pts.push_back(line_to + (Vector2(-0.707f * tsize, 0)));
                const Color cols[3] = {dcol,dcol,dcol};

                draw_primitive(pts, cols, PoolVector<Vector2>()); //small arrow
            }
        } break;
    }
}

void CollisionPolygon2D::set_polygon(const Vector<Point2> &p_polygon) {

    polygon = p_polygon;

    {
        for (size_t i = 0; i < polygon.size(); i++) {
            if (i == 0)
                aabb = Rect2(polygon[i], Size2());
            else
                aabb.expand_to(polygon[i]);
        }
        if (aabb == Rect2()) {

            aabb = Rect2(-10, -10, 20, 20);
        } else {
            aabb.position -= aabb.size * 0.3f;
            aabb.size += aabb.size * 0.6f;
        }
    }

    if (parent) {
        _build_polygon();
        _update_in_shape_owner();
    }
    update();
    update_configuration_warning();
}



void CollisionPolygon2D::set_build_mode(BuildMode p_mode) {

    ERR_FAIL_INDEX((int)p_mode, 2);
    build_mode = p_mode;
    if (parent) {
        _build_polygon();
        _update_in_shape_owner();
    }
    update();
    update_configuration_warning();
}

CollisionPolygon2D::BuildMode CollisionPolygon2D::get_build_mode() const {

    return build_mode;
}

#ifdef TOOLS_ENABLED
Rect2 CollisionPolygon2D::_edit_get_rect() const {

    return aabb;
}

bool CollisionPolygon2D::_edit_use_rect() const {
    return true;
}

bool CollisionPolygon2D::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {

    return Geometry::is_point_in_polygon(p_point, polygon);
}
#endif

String CollisionPolygon2D::get_configuration_warning() const {

    String warning = BaseClassName::get_configuration_warning();

    if (!object_cast<CollisionObject2D>(get_parent())) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("CollisionPolygon2D only serves to provide a collision shape to a CollisionObject2D derived node. Please only use it as a child of Area2D, StaticBody2D, RigidBody2D, KinematicBody2D, etc. to give them a shape.");
    }

    int polygon_count = polygon.size();
    if (polygon_count == 0) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("An empty CollisionPolygon2D has no effect on collision.");
    } else {
        bool solids = build_mode == BUILD_SOLIDS;
        if (solids) {
            if (polygon_count < 3) {
                if (!warning.empty()) {
                    warning += "\n\n";
                }
                warning += TTR("Invalid polygon. At least 3 points are needed in 'Solids' build mode.");
            }
        } else if (polygon_count < 2) {
            if (!warning.empty()) {
                warning += "\n\n";
            }
            warning += TTR("Invalid polygon. At least 2 points are needed in 'Segments' build mode.");
        }
    }

    return warning;
}

void CollisionPolygon2D::set_disabled(bool p_disabled) {
    disabled = p_disabled;
    update();
    if (parent) {
        parent->shape_owner_set_disabled(owner_id, p_disabled);
    }
}

bool CollisionPolygon2D::is_disabled() const {
    return disabled;
}

void CollisionPolygon2D::set_one_way_collision(bool p_enable) {
    one_way_collision = p_enable;
    update();
    if (parent) {
        parent->shape_owner_set_one_way_collision(owner_id, p_enable);
    }
}

bool CollisionPolygon2D::is_one_way_collision_enabled() const {

    return one_way_collision;
}

void CollisionPolygon2D::set_one_way_collision_margin(float p_margin) {
    one_way_collision_margin = p_margin;
    if (parent) {
        parent->shape_owner_set_one_way_collision_margin(owner_id, one_way_collision_margin);
    }
}

float CollisionPolygon2D::get_one_way_collision_margin() const {
    return one_way_collision_margin;
}
void CollisionPolygon2D::_bind_methods() {

    SE_BIND_METHOD(CollisionPolygon2D,set_polygon);
    SE_BIND_METHOD(CollisionPolygon2D,get_polygon);

    SE_BIND_METHOD(CollisionPolygon2D,set_build_mode);
    SE_BIND_METHOD(CollisionPolygon2D,get_build_mode);
    SE_BIND_METHOD(CollisionPolygon2D,set_disabled);
    SE_BIND_METHOD(CollisionPolygon2D,is_disabled);
    SE_BIND_METHOD(CollisionPolygon2D,set_one_way_collision);
    SE_BIND_METHOD(CollisionPolygon2D,is_one_way_collision_enabled);
    SE_BIND_METHOD(CollisionPolygon2D,set_one_way_collision_margin);
    SE_BIND_METHOD(CollisionPolygon2D,get_one_way_collision_margin);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "build_mode", PropertyHint::Enum, "Solids,Segments"), "set_build_mode", "get_build_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR2_ARRAY, "polygon"), "set_polygon", "get_polygon");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "disabled"), "set_disabled", "is_disabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "one_way_collision"), "set_one_way_collision", "is_one_way_collision_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "one_way_collision_margin", PropertyHint::Range, "0,128,0.1"), "set_one_way_collision_margin", "get_one_way_collision_margin");

    BIND_ENUM_CONSTANT(BUILD_SOLIDS);
    BIND_ENUM_CONSTANT(BUILD_SEGMENTS);
}

CollisionPolygon2D::CollisionPolygon2D() {

    aabb = Rect2(-10, -10, 20, 20);
    build_mode = BUILD_SOLIDS;
    set_notify_local_transform(true);
    parent = nullptr;
    owner_id = 0;
    disabled = false;
    one_way_collision = false;
    one_way_collision_margin = 1.0;
}
