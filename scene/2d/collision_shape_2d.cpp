/*************************************************************************/
/*  collision_shape_2d.cpp                                               */
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

#include "collision_shape_2d.h"

#include "collision_object_2d.h"
#include "core/callable_method_pointer.h"
#include "core/engine.h"
#include "core/method_bind.h"
#include "core/pool_vector.h"
#include "core/translation_helpers.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/capsule_shape_2d.h"
#include "scene/resources/circle_shape_2d.h"
#include "scene/resources/concave_polygon_shape_2d.h"
#include "scene/resources/convex_polygon_shape_2d.h"
#include "scene/resources/line_shape_2d.h"
#include "scene/resources/rectangle_shape_2d.h"
#include "scene/resources/segment_shape_2d.h"


IMPL_GDCLASS(CollisionShape2D)

void CollisionShape2D::_shape_changed() {

    update();
}

void CollisionShape2D::_update_in_shape_owner(bool p_xform_only) {

    parent->shape_owner_set_transform(owner_id, get_transform());
    if (p_xform_only)
        return;
    parent->shape_owner_set_disabled(owner_id, disabled);
    parent->shape_owner_set_one_way_collision(owner_id, one_way_collision);
    parent->shape_owner_set_one_way_collision_margin(owner_id, one_way_collision_margin);
}

void CollisionShape2D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_PARENTED: {

            parent = object_cast<CollisionObject2D>(get_parent());
            if (parent) {
                owner_id = parent->create_shape_owner(this);
                if (shape) {
                    parent->shape_owner_add_shape(owner_id, shape);
                }
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

            if (not shape) {
                break;
            }

            rect = Rect2();

            Color draw_col = get_tree()->get_debug_collisions_color();
            if (disabled) {
                float g = draw_col.get_v();
                draw_col.r = g;
                draw_col.g = g;
                draw_col.b = g;
                draw_col.a *= 0.5f;
            }
            shape->draw(get_canvas_item(), draw_col);

            rect = shape->get_rect();
            rect.grow_by(3);

            if (one_way_collision) {
                // Draw an arrow indicating the one-way collision direction
                draw_col = get_tree()->get_debug_collisions_color().inverted();
                if (disabled) {
                    draw_col = draw_col.darkened(0.25);
                }
                Vector2 line_to(0, 20);
                draw_line(Vector2(), line_to, draw_col, 2, true);
                float tsize = 8;
                const Vector2 pts[3] = {
                    line_to + (Vector2(0, tsize)),
                    line_to + (Vector2(0.707f * tsize, 0)),
                    line_to + (Vector2(-0.707f * tsize, 0)),
                };
                const Color cols[3] = {draw_col,draw_col,draw_col};

                draw_primitive(pts, cols, PoolVector<Vector2>());
            }
        } break;
    }
}

void CollisionShape2D::set_shape(const Ref<Shape2D> &p_shape) {
    if (p_shape == shape) {
        return;
    }

    if (shape)
        shape->disconnect("changed",callable_mp(this, &ClassName::_shape_changed));
    shape = p_shape;
    update();
    if (parent) {
        parent->shape_owner_clear_shapes(owner_id);
        if (shape) {
            parent->shape_owner_add_shape(owner_id, shape);
        }
        _update_in_shape_owner();
    }

    if (shape)
        shape->connect("changed",callable_mp(this, &ClassName::_shape_changed));

    update_configuration_warning();
}

Ref<Shape2D> CollisionShape2D::get_shape() const {

    return shape;
}
#ifdef TOOLS_ENABLED

bool CollisionShape2D::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {

    if (not shape)
        return false;

    return shape->_edit_is_selected_on_click(p_point, p_tolerance);
}
#endif
String CollisionShape2D::get_configuration_warning() const {

    String warning = Node2D::get_configuration_warning();

    if (!object_cast<CollisionObject2D>(get_parent())) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("CollisionShape2D only serves to provide a collision shape to a CollisionObject2D derived node. Please only use it as a child of Area2D, StaticBody2D, RigidBody2D, KinematicBody2D, etc. to give them a shape.");
    }

    if (!shape) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("A shape must be provided for CollisionShape2D to function. Please create a shape resource for it!");
    } else {
        Ref<ConvexPolygonShape2D> convex {dynamic_ref_cast<ConvexPolygonShape2D>(shape)};
        Ref<ConcavePolygonShape2D> concave {dynamic_ref_cast<ConcavePolygonShape2D>(shape)};
        if (convex || concave) {
            if (!warning.empty()) {
                warning += "\n\n";
            }
            warning += TTR("Polygon-based shapes are not meant be used nor edited directly through the CollisionShape2D node. Please use the CollisionPolygon2D node instead.");
        }
    }

    return warning;

}

void CollisionShape2D::set_disabled(bool p_disabled) {
    disabled = p_disabled;
    update();
    if (parent) {
        parent->shape_owner_set_disabled(owner_id, p_disabled);
    }
}

bool CollisionShape2D::is_disabled() const {
    return disabled;
}

void CollisionShape2D::set_one_way_collision(bool p_enable) {
    one_way_collision = p_enable;
    update();
    if (parent) {
        parent->shape_owner_set_one_way_collision(owner_id, p_enable);
    }
}

bool CollisionShape2D::is_one_way_collision_enabled() const {

    return one_way_collision;
}

void CollisionShape2D::set_one_way_collision_margin(float p_margin) {
    one_way_collision_margin = p_margin;
    if (parent) {
        parent->shape_owner_set_one_way_collision_margin(owner_id, one_way_collision_margin);
    }
}

float CollisionShape2D::get_one_way_collision_margin() const {
    return one_way_collision_margin;
}

void CollisionShape2D::_bind_methods() {

    SE_BIND_METHOD(CollisionShape2D,set_shape);
    SE_BIND_METHOD(CollisionShape2D,get_shape);
    SE_BIND_METHOD(CollisionShape2D,set_disabled);
    SE_BIND_METHOD(CollisionShape2D,is_disabled);
    SE_BIND_METHOD(CollisionShape2D,set_one_way_collision);
    SE_BIND_METHOD(CollisionShape2D,is_one_way_collision_enabled);
    SE_BIND_METHOD(CollisionShape2D,set_one_way_collision_margin);
    SE_BIND_METHOD(CollisionShape2D,get_one_way_collision_margin);
    SE_BIND_METHOD(CollisionShape2D,_shape_changed);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "shape", PropertyHint::ResourceType, "Shape2D"), "set_shape", "get_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "disabled"), "set_disabled", "is_disabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "one_way_collision"), "set_one_way_collision", "is_one_way_collision_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "one_way_collision_margin", PropertyHint::Range, "0,128,0.1"), "set_one_way_collision_margin", "get_one_way_collision_margin");
}

CollisionShape2D::CollisionShape2D() {

    rect = Rect2(-Point2(10, 10), Point2(20, 20));
    set_notify_local_transform(true);
    owner_id = 0;
    parent = nullptr;
    disabled = false;
    one_way_collision = false;
    one_way_collision_margin = 1.0;
}
