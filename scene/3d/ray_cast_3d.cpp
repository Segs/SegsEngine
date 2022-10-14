/*************************************************************************/
/*  ray_cast_3d.cpp                                                      */
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

#include "ray_cast_3d.h"

#include "core/engine.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "mesh_instance_3d.h"
#include "scene/3d/collision_object_3d.h"
#include "scene/main/scene_tree.h"
#include "servers/physics_server_3d.h"
#include "scene/resources/material.h"

IMPL_GDCLASS(RayCast3D)

void RayCast3D::set_cast_to(const Vector3 &p_point) {

    cast_to = p_point;
        update_gizmo();
    if (Engine::get_singleton()->is_editor_hint()) {
        if (is_inside_tree()) {
            _update_debug_shape_vertices();
        }
    } else if (debug_shape) {
        _update_debug_shape();
    }
}

Vector3 RayCast3D::get_cast_to() const {

    return cast_to;
}

void RayCast3D::set_collision_mask(uint32_t p_mask) {

    collision_mask = p_mask;
}

uint32_t RayCast3D::get_collision_mask() const {

    return collision_mask;
}

void RayCast3D::set_collision_mask_bit(int p_bit, bool p_value) {
    ERR_FAIL_INDEX_MSG(p_bit, 32, "Collision mask bit must be between 0 and 31 inclusive.");

    uint32_t mask = get_collision_mask();
    if (p_value)
        mask |= 1 << p_bit;
    else
        mask &= ~(1 << p_bit);
    set_collision_mask(mask);
}

bool RayCast3D::get_collision_mask_bit(int p_bit) const {
    ERR_FAIL_INDEX_V_MSG(p_bit, 32, false, "Collision mask bit must be between 0 and 31 inclusive.");

    return get_collision_mask() & (1 << p_bit);
}

bool RayCast3D::is_colliding() const {

    return collided;
}
Object *RayCast3D::get_collider() const {

    if (against==entt::null)
        return nullptr;

    return object_for_entity(against);
}

int RayCast3D::get_collider_shape() const {

    return against_shape;
}
Vector3 RayCast3D::get_collision_point() const {

    return collision_point;
}
Vector3 RayCast3D::get_collision_normal() const {

    return collision_normal;
}

void RayCast3D::set_enabled(bool p_enabled) {

    enabled = p_enabled;
    update_gizmo();

    if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint())
        set_physics_process_internal(p_enabled);
    if (!p_enabled)
        collided = false;

    if (is_inside_tree() && get_tree()->is_debugging_collisions_hint()) {
        if (p_enabled)
            _update_debug_shape();
        else
            _clear_debug_shape();
    }
}

bool RayCast3D::is_enabled() const {

    return enabled;
}

void RayCast3D::set_exclude_parent_body(bool p_exclude_parent_body) {

    if (exclude_parent_body == p_exclude_parent_body)
        return;

    exclude_parent_body = p_exclude_parent_body;

    if (!is_inside_tree())
        return;

    if (object_cast<CollisionObject3D>(get_parent())) {
        if (exclude_parent_body)
            exclude.insert(object_cast<CollisionObject3D>(get_parent())->get_rid());
        else
            exclude.erase(object_cast<CollisionObject3D>(get_parent())->get_rid());
    }
}

bool RayCast3D::get_exclude_parent_body() const {

    return exclude_parent_body;
}

void RayCast3D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {

            if (Engine::get_singleton()->is_editor_hint()) {
                _update_debug_shape_vertices();
            }
            if (enabled && !Engine::get_singleton()->is_editor_hint()) {
                set_physics_process_internal(true);

                if (get_tree()->is_debugging_collisions_hint())
                    _update_debug_shape();
            } else
                set_physics_process_internal(false);

            if (object_cast<CollisionObject3D>(get_parent())) {
                if (exclude_parent_body)
                    exclude.insert(object_cast<CollisionObject3D>(get_parent())->get_rid());
                else
                    exclude.erase(object_cast<CollisionObject3D>(get_parent())->get_rid());
            }

        } break;
        case NOTIFICATION_EXIT_TREE: {

            if (enabled) {
                set_physics_process_internal(false);
            }

            if (debug_shape)
                _clear_debug_shape();

        } break;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {

            if (!enabled) {
                break;
            }

            bool prev_collision_state = collided;
            _update_raycast_state();
            if (prev_collision_state != collided && get_tree()->is_debugging_collisions_hint()) {
                _update_debug_shape_material(true);
                }

        } break;
    }
}

void RayCast3D::_update_raycast_state() {
    Ref<World3D> w3d = get_world_3d();
    ERR_FAIL_COND(not w3d);

    PhysicsDirectSpaceState3D *dss = PhysicsServer3D::get_singleton()->space_get_direct_state(w3d->get_space());
    ERR_FAIL_COND(!dss);

    Transform gt = get_global_transform();

    Vector3 to = cast_to;
    if (to == Vector3())
        to = Vector3(0, 0.01f, 0);

    PhysicsDirectSpaceState3D::RayResult rr;

    if (dss->intersect_ray(
                gt.get_origin(), gt.xform(to), rr, exclude, collision_mask, collide_with_bodies, collide_with_areas)) {

        collided = true;
        against = rr.collider_id;
        collision_point = rr.position;
        collision_normal = rr.normal;
        against_shape = rr.shape;
    } else {
        collided = false;
        against = entt::null;
        against_shape = 0;
    }
}

void RayCast3D::force_raycast_update() {
    _update_raycast_state();
}

void RayCast3D::add_exception_rid(const RID &p_rid) {

    exclude.insert(p_rid);
}

void RayCast3D::add_exception(const Object *p_object) {

    ERR_FAIL_NULL(p_object);
    const CollisionObject3D *co = object_cast<CollisionObject3D>(p_object);
    if (!co)
        return;
    add_exception_rid(co->get_rid());
}

void RayCast3D::remove_exception_rid(const RID &p_rid) {

    exclude.erase(p_rid);
}

void RayCast3D::remove_exception(const Object *p_object) {

    ERR_FAIL_NULL(p_object);
    const CollisionObject3D *co = object_cast<CollisionObject3D>(p_object);
    if (!co)
        return;
    remove_exception_rid(co->get_rid());
}

void RayCast3D::clear_exceptions() {

    exclude.clear();
    if (exclude_parent_body && is_inside_tree()) {
        CollisionObject3D *parent = object_cast<CollisionObject3D>(get_parent());
        if (parent) {
            exclude.insert(parent->get_rid());
        }
    }
}

void RayCast3D::set_collide_with_areas(bool p_clip) {

    collide_with_areas = p_clip;
}

bool RayCast3D::is_collide_with_areas_enabled() const {

    return collide_with_areas;
}

void RayCast3D::set_collide_with_bodies(bool p_clip) {

    collide_with_bodies = p_clip;
}

bool RayCast3D::is_collide_with_bodies_enabled() const {

    return collide_with_bodies;
}

void RayCast3D::_bind_methods() {

    BIND_METHOD(RayCast3D,set_enabled);
    BIND_METHOD(RayCast3D,is_enabled);

    BIND_METHOD(RayCast3D,set_cast_to);
    BIND_METHOD(RayCast3D,get_cast_to);

    BIND_METHOD(RayCast3D,is_colliding);
    BIND_METHOD(RayCast3D,force_raycast_update);

    BIND_METHOD(RayCast3D,get_collider);
    BIND_METHOD(RayCast3D,get_collider_shape);
    BIND_METHOD(RayCast3D,get_collision_point);
    BIND_METHOD(RayCast3D,get_collision_normal);

    BIND_METHOD(RayCast3D,add_exception_rid);
    BIND_METHOD(RayCast3D,add_exception);

    BIND_METHOD(RayCast3D,remove_exception_rid);
    BIND_METHOD(RayCast3D,remove_exception);

    BIND_METHOD(RayCast3D,clear_exceptions);

    BIND_METHOD(RayCast3D,set_collision_mask);
    BIND_METHOD(RayCast3D,get_collision_mask);

    MethodBinder::bind_method(
            D_METHOD("set_collision_mask_bit", { "bit", "value" }), &RayCast3D::set_collision_mask_bit);
    BIND_METHOD(RayCast3D,get_collision_mask_bit);

    BIND_METHOD(RayCast3D,set_exclude_parent_body);
    BIND_METHOD(RayCast3D,get_exclude_parent_body);

    BIND_METHOD(RayCast3D,set_collide_with_areas);
    BIND_METHOD(RayCast3D,is_collide_with_areas_enabled);

    BIND_METHOD(RayCast3D,set_collide_with_bodies);
    BIND_METHOD(RayCast3D,is_collide_with_bodies_enabled);
    BIND_METHOD(RayCast3D,set_debug_shape_custom_color);
    BIND_METHOD(RayCast3D,get_debug_shape_custom_color);

    BIND_METHOD(RayCast3D,set_debug_shape_thickness);
    BIND_METHOD(RayCast3D,get_debug_shape_thickness);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "enabled"), "set_enabled", "is_enabled");
    ADD_PROPERTY(
            PropertyInfo(VariantType::BOOL, "exclude_parent"), "set_exclude_parent_body", "get_exclude_parent_body");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "cast_to"), "set_cast_to", "get_cast_to");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_mask", PropertyHint::Layers3DPhysics), "set_collision_mask",
            "get_collision_mask");

    ADD_GROUP("Collide With", "collide_with");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "collide_with_areas", PropertyHint::Layers3DPhysics),
            "set_collide_with_areas", "is_collide_with_areas_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "collide_with_bodies", PropertyHint::Layers3DPhysics),
            "set_collide_with_bodies", "is_collide_with_bodies_enabled");

    ADD_GROUP("Debug Shape", "debug_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "debug_shape_custom_color"), "set_debug_shape_custom_color", "get_debug_shape_custom_color");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "debug_shape_thickness", PropertyHint::Range, "1,5"), "set_debug_shape_thickness", "get_debug_shape_thickness");
}


int RayCast3D::get_debug_shape_thickness() const {
    return debug_shape_thickness;
}

void RayCast3D::_update_debug_shape_vertices() {
    debug_shape_vertices.clear();
    debug_line_vertices.clear();

    if (cast_to == Vector3()) {
        return;
    }
    debug_line_vertices.push_back(Vector3());
    debug_line_vertices.push_back(cast_to);

    if (debug_shape_thickness > 1) {
        float scale_factor = 100.0;
        Vector3 dir = Vector3(cast_to).normalized();
        // Draw truncated pyramid
        Vector3 normal = (fabs(dir.x) + fabs(dir.y) > CMP_EPSILON) ? Vector3(-dir.y, dir.x, 0).normalized() : Vector3(0, -dir.z, dir.y).normalized();
        normal *= debug_shape_thickness / scale_factor;
        int vertices_strip_order[14] = { 4, 5, 0, 1, 2, 5, 6, 4, 7, 0, 3, 2, 7, 6 };
        for (int v = 0; v < 14; v++) {
            Vector3 vertex = vertices_strip_order[v] < 4 ? normal : normal / 3.0 + cast_to;
            debug_shape_vertices.push_back(vertex.rotated(dir, Math_PI * (0.5 * (vertices_strip_order[v] % 4) + 0.25)));
        }
    }
}
void RayCast3D::set_debug_shape_thickness(int p_debug_shape_thickness) {
    debug_shape_thickness = p_debug_shape_thickness;
    update_gizmo();

    if (Engine::get_singleton()->is_editor_hint()) {
        if (is_inside_tree()) {
            _update_debug_shape_vertices();
        }
    } else if (debug_shape) {
        _update_debug_shape();
    }
}

const Vector<Vector3> &RayCast3D::get_debug_shape_vertices() const {
    return debug_shape_vertices;
}

const Vector<Vector3> &RayCast3D::get_debug_line_vertices() const {
    return debug_line_vertices;
}

void RayCast3D::set_debug_shape_custom_color(const Color &p_color) {
    debug_shape_custom_color = p_color;
    if (debug_material) {
        _update_debug_shape_material();
    }
}

Ref<SpatialMaterial> RayCast3D::get_debug_material() {
    _update_debug_shape_material();
    return dynamic_ref_cast<SpatialMaterial>(debug_material);
}

const Color &RayCast3D::get_debug_shape_custom_color() const {
    return debug_shape_custom_color;
}

void RayCast3D::_create_debug_shape() {
    _update_debug_shape_material();

    Ref<ArrayMesh> mesh(make_ref_counted<ArrayMesh>());

    MeshInstance3D *mi = memnew(MeshInstance3D);
    mi->set_mesh(mesh);

    add_child(mi);
    debug_shape = mi;
}

void RayCast3D::_update_debug_shape_material(bool p_check_collision) {
    if (!debug_material) {
        Ref<SpatialMaterial> material = make_ref_counted<SpatialMaterial>();
        debug_material = material;

        material->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
        material->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
        // Use double-sided rendering so that the RayCast can be seen if the camera is inside.
        material->set_cull_mode(SpatialMaterial::CULL_DISABLED);
    }

    Color color = debug_shape_custom_color;
    if (color == Color(0.0, 0.0, 0.0)) {
        // Use the default debug shape color defined in the Project Settings.
        color = get_tree()->get_debug_collisions_color();
    }

    if (p_check_collision && collided) {
        if ((color.get_h() < 0.055 || color.get_h() > 0.945) && color.get_s() > 0.5 && color.get_v() > 0.5) {
            // If base color is already quite reddish, highlight collision with green color
            color = Color(0.0, 1.0, 0.0, color.a);
        } else {
            // Else, highlight collision with red color
            color = Color(1.0, 0, 0, color.a);
        }
    }

    Ref<SpatialMaterial> material = static_ref_cast<SpatialMaterial>(debug_material);
    material->set_albedo(color);
}

void RayCast3D::_update_debug_shape() {

    if (!enabled)
        return;

    if (!debug_shape)
        _create_debug_shape();

    MeshInstance3D *mi = static_cast<MeshInstance3D *>(debug_shape);
    if (not mi->get_mesh())
        return;

    _update_debug_shape_vertices();
    Ref<ArrayMesh> mesh = dynamic_ref_cast<ArrayMesh>(mi->get_mesh());
    mesh->clear_surfaces();

    Vector<Vector3> verts = {Vector3(0,0,0),cast_to};
    SurfaceArrays a(eastl::move(verts));

    uint32_t flags = 0;
    int surface_count = 0;

    if (!debug_line_vertices.empty()) {
        Vector<Vector3> verts(debug_line_vertices);
        SurfaceArrays a(eastl::move(verts));
        mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, eastl::move(a), {}, flags);
        mesh->surface_set_material(surface_count, debug_material);
        ++surface_count;
    }

    if (!debug_shape_vertices.empty()) {
        Vector<Vector3> verts(debug_shape_vertices);
        SurfaceArrays a(eastl::move(verts));
        mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLE_STRIP, eastl::move(a), {}, flags);
        mesh->surface_set_material(surface_count, debug_material);
        ++surface_count;
    }
}

void RayCast3D::_clear_debug_shape() {

    if (!debug_shape)
        return;

    MeshInstance3D *mi = static_cast<MeshInstance3D *>(debug_shape);
    if (mi->is_inside_tree()) {
        mi->queue_delete();
    } else {
        memdelete(mi);
    }

    debug_shape = nullptr;
}

RayCast3D::RayCast3D() = default;

