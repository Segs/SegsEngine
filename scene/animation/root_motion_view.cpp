/*************************************************************************/
/*  root_motion_view.cpp                                                 */
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

#include "root_motion_view.h"

#include "core/method_bind.h"
#include "scene/animation/animation_tree.h"
#include "scene/resources/material.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(RootMotionView)

void RootMotionView::set_animation_path(const NodePath &p_path) {
    path = p_path;
    first = true;
}

NodePath RootMotionView::get_animation_path() const {
    return path;
}

void RootMotionView::set_color(const Color &p_color) {
    color = p_color;
    first = true;
}

Color RootMotionView::get_color() const {
    return color;
}

void RootMotionView::set_cell_size(float p_size) {
    cell_size = p_size;
    first = true;
}

float RootMotionView::get_cell_size() const {
    return cell_size;
}

void RootMotionView::set_radius(float p_radius) {
    radius = p_radius;
    first = true;
}

float RootMotionView::get_radius() const {
    return radius;
}

void RootMotionView::set_zero_y(bool p_zero_y) {
    zero_y = p_zero_y;
}

bool RootMotionView::get_zero_y() const {
    return zero_y;
}

void RootMotionView::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {

        RenderingServer::get_singleton()->immediate_set_material(immediate, SpatialMaterial::get_material_rid_for_2d(false, true, false, false, false));
        first = true;
    }

    if (p_what == NOTIFICATION_INTERNAL_PROCESS || p_what == NOTIFICATION_INTERNAL_PHYSICS_PROCESS) {
        Transform transform;

        if (has_node(path)) {

            Node *node = get_node(path);

            AnimationTree *tree = object_cast<AnimationTree>(node);
            if (tree && tree->is_active() && tree->get_root_motion_track() != NodePath()) {
                if (is_processing_internal() && tree->get_process_mode() == AnimationTree::ANIMATION_PROCESS_PHYSICS) {
                    set_process_internal(false);
                    set_physics_process_internal(true);
                }

                if (is_physics_processing_internal() && tree->get_process_mode() == AnimationTree::ANIMATION_PROCESS_IDLE) {
                    set_process_internal(true);
                    set_physics_process_internal(false);
                }

                transform = tree->get_root_motion_transform();
            }
        }

        if (!first && transform == Transform()) {
            return;
        }

        first = false;

        transform.orthonormalize(); //don't want scale, too imprecise
        transform.affine_invert();

        accumulated = transform * accumulated;
        accumulated.origin.x = Math::fposmod(accumulated.origin.x, cell_size);
        if (zero_y) {
            accumulated.origin.y = 0;
        }
        accumulated.origin.z = Math::fposmod(accumulated.origin.z, cell_size);

        RenderingServer::get_singleton()->immediate_clear(immediate);

        int cells_in_radius = int((radius / cell_size) + 1.0);

        RenderingServer::get_singleton()->immediate_begin(immediate, RS::PRIMITIVE_LINES,entt::null);
        for (int i = -cells_in_radius; i < cells_in_radius; i++) {
            for (int j = -cells_in_radius; j < cells_in_radius; j++) {

                Vector3 from(i * cell_size, 0, j * cell_size);
                Vector3 from_i((i + 1) * cell_size, 0, j * cell_size);
                Vector3 from_j(i * cell_size, 0, (j + 1) * cell_size);
                from = accumulated.xform(from);
                from_i = accumulated.xform(from_i);
                from_j = accumulated.xform(from_j);

                Color c = color, c_i = color, c_j = color;
                c.a *= M_MAX(0, 1.0 - from.length() / radius);
                c_i.a *= M_MAX(0, 1.0 - from_i.length() / radius);
                c_j.a *= M_MAX(0, 1.0 - from_j.length() / radius);

                RenderingServer::get_singleton()->immediate_color(immediate, c);
                RenderingServer::get_singleton()->immediate_vertex(immediate, from);

                RenderingServer::get_singleton()->immediate_color(immediate, c_i);
                RenderingServer::get_singleton()->immediate_vertex(immediate, from_i);

                RenderingServer::get_singleton()->immediate_color(immediate, c);
                RenderingServer::get_singleton()->immediate_vertex(immediate, from);

                RenderingServer::get_singleton()->immediate_color(immediate, c_j);
                RenderingServer::get_singleton()->immediate_vertex(immediate, from_j);
            }
        }

        RenderingServer::get_singleton()->immediate_end(immediate);
    }
}

AABB RootMotionView::get_aabb() const {

    return AABB(Vector3(-radius, 0, -radius), Vector3(radius * 2, 0.001f, radius * 2));
}
Vector<Face3> RootMotionView::get_faces(uint32_t p_usage_flags) const {
    return Vector<Face3>();
}

void RootMotionView::_bind_methods() {

    SE_BIND_METHOD(RootMotionView,set_animation_path);
    SE_BIND_METHOD(RootMotionView,get_animation_path);

    SE_BIND_METHOD(RootMotionView,set_color);
    SE_BIND_METHOD(RootMotionView,get_color);

    SE_BIND_METHOD(RootMotionView,set_cell_size);
    SE_BIND_METHOD(RootMotionView,get_cell_size);

    SE_BIND_METHOD(RootMotionView,set_radius);
    SE_BIND_METHOD(RootMotionView,get_radius);

    SE_BIND_METHOD(RootMotionView,set_zero_y);
    SE_BIND_METHOD(RootMotionView,get_zero_y);

    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "animation_path", PropertyHint::NodePathValidTypes, "AnimationTree"), "set_animation_path", "get_animation_path");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "color"), "set_color", "get_color");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "cell_size", PropertyHint::Range, "0.1,16,0.01,or_greater"), "set_cell_size", "get_cell_size");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "radius", PropertyHint::Range, "0.1,16,0.01,or_greater"), "set_radius", "get_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "zero_y"), "set_zero_y", "get_zero_y");
}

RootMotionView::RootMotionView() {
    set_process_internal(true);
    immediate = RenderingServer::get_singleton()->immediate_create();
    set_base(immediate);
}

RootMotionView::~RootMotionView() {
    set_base(entt::null);
    RenderingServer::get_singleton()->free_rid(immediate);
}
