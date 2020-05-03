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

IMPL_GDCLASS(RayCast3D)

void RayCast3D::set_cast_to(const Vector3 &p_point) {

    cast_to = p_point;
    if (is_inside_tree() && (Engine::get_singleton()->is_editor_hint() || get_tree()->is_debugging_collisions_hint()))
        update_gizmo();
    if (is_inside_tree() && get_tree()->is_debugging_collisions_hint())
        _update_debug_shape();
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

    uint32_t mask = get_collision_mask();
    if (p_value)
        mask |= 1 << p_bit;
    else
        mask &= ~(1 << p_bit);
    set_collision_mask(mask);
}

bool RayCast3D::get_collision_mask_bit(int p_bit) const {

    return get_collision_mask() & (1 << p_bit);
}

bool RayCast3D::is_colliding() const {

    return collided;
}
Object *RayCast3D::get_collider() const {

    if (against == 0)
        return nullptr;

    return gObjectDB().get_instance(against);
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

            if (!enabled)
                break;

            bool prev_collision_state = collided;
            _update_raycast_state();
            if (prev_collision_state != collided && get_tree()->is_debugging_collisions_hint()) {
                if (debug_material) {
                    Ref<SpatialMaterial> line_material = dynamic_ref_cast<SpatialMaterial>(debug_material);
                    line_material->set_albedo(collided ? Color(1.0, 0, 0) : Color(1.0, 0.8f, 0.6f));
                }
            }

        } break;
    }
}

void RayCast3D::_update_raycast_state() {
    Ref<World3D> w3d = get_world();
    ERR_FAIL_COND(not w3d);

    PhysicsDirectSpaceState3D *dss = PhysicsServer3D::get_singleton()->space_get_direct_state(w3d->get_space());
    ERR_FAIL_COND(!dss);

    Transform gt = get_global_transform();

    Vector3 to = cast_to;
    if (to == Vector3())
        to = Vector3(0, 0.01f, 0);

    PhysicsDirectSpaceState3D::RayResult rr;

    if (dss->intersect_ray(gt.get_origin(), gt.xform(to), rr, exclude, collision_mask, collide_with_bodies, collide_with_areas)) {

        collided = true;
        against = rr.collider_id;
        collision_point = rr.position;
        collision_normal = rr.normal;
        against_shape = rr.shape;
    } else {
        collided = false;
        against = 0;
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

    MethodBinder::bind_method(D_METHOD("set_enabled", {"enabled"}), &RayCast3D::set_enabled);
    MethodBinder::bind_method(D_METHOD("is_enabled"), &RayCast3D::is_enabled);

    MethodBinder::bind_method(D_METHOD("set_cast_to", {"local_point"}), &RayCast3D::set_cast_to);
    MethodBinder::bind_method(D_METHOD("get_cast_to"), &RayCast3D::get_cast_to);

    MethodBinder::bind_method(D_METHOD("is_colliding"), &RayCast3D::is_colliding);
    MethodBinder::bind_method(D_METHOD("force_raycast_update"), &RayCast3D::force_raycast_update);

    MethodBinder::bind_method(D_METHOD("get_collider"), &RayCast3D::get_collider);
    MethodBinder::bind_method(D_METHOD("get_collider_shape"), &RayCast3D::get_collider_shape);
    MethodBinder::bind_method(D_METHOD("get_collision_point"), &RayCast3D::get_collision_point);
    MethodBinder::bind_method(D_METHOD("get_collision_normal"), &RayCast3D::get_collision_normal);

    MethodBinder::bind_method(D_METHOD("add_exception_rid", {"rid"}), &RayCast3D::add_exception_rid);
    MethodBinder::bind_method(D_METHOD("add_exception", {"node"}), &RayCast3D::add_exception);

    MethodBinder::bind_method(D_METHOD("remove_exception_rid", {"rid"}), &RayCast3D::remove_exception_rid);
    MethodBinder::bind_method(D_METHOD("remove_exception", {"node"}), &RayCast3D::remove_exception);

    MethodBinder::bind_method(D_METHOD("clear_exceptions"), &RayCast3D::clear_exceptions);

    MethodBinder::bind_method(D_METHOD("set_collision_mask", {"mask"}), &RayCast3D::set_collision_mask);
    MethodBinder::bind_method(D_METHOD("get_collision_mask"), &RayCast3D::get_collision_mask);

    MethodBinder::bind_method(D_METHOD("set_collision_mask_bit", {"bit", "value"}), &RayCast3D::set_collision_mask_bit);
    MethodBinder::bind_method(D_METHOD("get_collision_mask_bit", {"bit"}), &RayCast3D::get_collision_mask_bit);

    MethodBinder::bind_method(D_METHOD("set_exclude_parent_body", {"mask"}), &RayCast3D::set_exclude_parent_body);
    MethodBinder::bind_method(D_METHOD("get_exclude_parent_body"), &RayCast3D::get_exclude_parent_body);

    MethodBinder::bind_method(D_METHOD("set_collide_with_areas", {"enable"}), &RayCast3D::set_collide_with_areas);
    MethodBinder::bind_method(D_METHOD("is_collide_with_areas_enabled"), &RayCast3D::is_collide_with_areas_enabled);

    MethodBinder::bind_method(D_METHOD("set_collide_with_bodies", {"enable"}), &RayCast3D::set_collide_with_bodies);
    MethodBinder::bind_method(D_METHOD("is_collide_with_bodies_enabled"), &RayCast3D::is_collide_with_bodies_enabled);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "enabled"), "set_enabled", "is_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "exclude_parent"), "set_exclude_parent_body", "get_exclude_parent_body");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "cast_to"), "set_cast_to", "get_cast_to");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_mask", PropertyHint::Layers3DPhysics), "set_collision_mask", "get_collision_mask");

    ADD_GROUP("Collide With", "collide_with");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "collide_with_areas", PropertyHint::Layers3DPhysics), "set_collide_with_areas", "is_collide_with_areas_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "collide_with_bodies", PropertyHint::Layers3DPhysics), "set_collide_with_bodies", "is_collide_with_bodies_enabled");
}

void RayCast3D::_create_debug_shape() {

    if (not debug_material) {
        Ref<SpatialMaterial> line_material = make_ref_counted<SpatialMaterial>();
        debug_material = line_material;

        line_material->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
        line_material->set_line_width(3.0);
        line_material->set_albedo(Color(1.0, 0.8f, 0.6f));
    }

    Ref<ArrayMesh> mesh(make_ref_counted<ArrayMesh>());

    MeshInstance3D *mi = memnew(MeshInstance3D);
    mi->set_mesh(mesh);

    add_child(mi);
    debug_shape = mi;
}

void RayCast3D::_update_debug_shape() {

    if (!enabled)
        return;

    if (!debug_shape)
        _create_debug_shape();

    MeshInstance3D *mi = static_cast<MeshInstance3D *>(debug_shape);
    if (not mi->get_mesh())
        return;

    Ref<ArrayMesh> mesh = dynamic_ref_cast<ArrayMesh>(mi->get_mesh());
    if (mesh->get_surface_count() > 0)
        mesh->surface_remove(0);

    Vector<Vector3> verts = {Vector3(0,0,0),cast_to};
    SurfaceArrays a(eastl::move(verts));

    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, eastl::move(a));
    mesh->surface_set_material(0, debug_material);
}

void RayCast3D::_clear_debug_shape() {

    if (!debug_shape)
        return;

    MeshInstance3D *mi = static_cast<MeshInstance3D *>(debug_shape);
    if (mi->is_inside_tree())
        mi->queue_delete();
    else
        memdelete(mi);

    debug_shape = nullptr;
}

RayCast3D::RayCast3D() : against(0), cast_to(Vector3(0, -1, 0)) {

    enabled = false;
    collided = false;
    against_shape = 0;
    collision_mask = 1;
    debug_shape = nullptr;
    exclude_parent_body = true;
    collide_with_areas = false;
    collide_with_bodies = true;
}
