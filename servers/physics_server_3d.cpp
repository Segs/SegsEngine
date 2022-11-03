/*************************************************************************/
/*  physics_server_3d.cpp                                                */
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

#include "physics_server_3d.h"

#include "core/dictionary.h"
#include "core/object_db.h"
#include "core/method_bind.h"
#include "core/project_settings.h"
#include "core/pool_vector.h"

IMPL_GDCLASS(PhysicsDirectBodyState3D)
IMPL_GDCLASS(PhysicsTestMotionResult)
IMPL_GDCLASS(PhysicsShapeQueryParameters3D)
IMPL_GDCLASS(PhysicsDirectSpaceState3D)
IMPL_GDCLASS(PhysicsServer3D)

VARIANT_ENUM_CAST(PhysicsServer3D::ShapeType);
VARIANT_ENUM_CAST(PhysicsServer3D::SpaceParameter);
VARIANT_ENUM_CAST(PhysicsServer3D::AreaParameter);
VARIANT_ENUM_CAST(PhysicsServer3D::AreaSpaceOverrideMode);
VARIANT_ENUM_CAST(PhysicsServer3D::BodyMode);
VARIANT_ENUM_CAST(PhysicsServer3D::BodyParameter);
VARIANT_ENUM_CAST(PhysicsServer3D::BodyState);
VARIANT_ENUM_CAST(PhysicsServer3D::BodyAxis);
VARIANT_ENUM_CAST(PhysicsServer3D::PinJointParam);
VARIANT_ENUM_CAST(PhysicsServer3D::JointType);
VARIANT_ENUM_CAST(PhysicsServer3D::HingeJointParam);
VARIANT_ENUM_CAST(PhysicsServer3D::HingeJointFlag);
VARIANT_ENUM_CAST(PhysicsServer3D::SliderJointParam);
VARIANT_ENUM_CAST(PhysicsServer3D::ConeTwistJointParam);
VARIANT_ENUM_CAST(PhysicsServer3D::G6DOFJointAxisParam);
VARIANT_ENUM_CAST(PhysicsServer3D::G6DOFJointAxisFlag);
VARIANT_ENUM_CAST(PhysicsServer3D::AreaBodyStatus);
VARIANT_ENUM_CAST(PhysicsServer3D::ProcessInfo);

PhysicsServer3D *PhysicsServer3D::singleton = nullptr;

void PhysicsDirectBodyState3D::integrate_forces() {

    real_t step = get_step();
    Vector3 lv = get_linear_velocity();
    lv += get_total_gravity() * step;

    Vector3 av = get_angular_velocity();

    float linear_damp = 1.0f - step * get_total_linear_damp();

    if (linear_damp < 0) // reached zero in the given time
        linear_damp = 0;

    float angular_damp = 1.0f - step * get_total_angular_damp();

    if (angular_damp < 0) // reached zero in the given time
        angular_damp = 0;

    lv *= linear_damp;
    av *= angular_damp;

    set_linear_velocity(lv);
    set_angular_velocity(av);
}

Object *PhysicsDirectBodyState3D::get_contact_collider_object(int p_contact_idx) const {

    GameEntity objid = get_contact_collider_id(p_contact_idx);
    Object *obj = object_for_entity(objid);
    return obj;
}

PhysicsServer3D *PhysicsServer3D::get_singleton() {

    return singleton;
}

void PhysicsDirectBodyState3D::_bind_methods() {

    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_total_gravity);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_total_linear_damp);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_total_angular_damp);

    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_center_of_mass);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_principal_inertia_axes);

    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_inverse_mass);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_inverse_inertia);

    SE_BIND_METHOD(PhysicsDirectBodyState3D,set_linear_velocity);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_linear_velocity);

    SE_BIND_METHOD(PhysicsDirectBodyState3D,set_angular_velocity);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_angular_velocity);

    SE_BIND_METHOD(PhysicsDirectBodyState3D,set_transform);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_transform);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_velocity_at_local_position);

    SE_BIND_METHOD(PhysicsDirectBodyState3D,add_central_force);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,add_force);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,add_torque);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,apply_central_impulse);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,apply_impulse);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,apply_torque_impulse);

    SE_BIND_METHOD(PhysicsDirectBodyState3D,set_sleep_state);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,is_sleeping);

    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_count);

    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_local_position);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_local_normal);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_impulse);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_local_shape);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_collider);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_collider_position);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_collider_id);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_collider_object);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_collider_shape);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_contact_collider_velocity_at_position);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_step);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,integrate_forces);
    SE_BIND_METHOD(PhysicsDirectBodyState3D,get_space_state);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "step"), "", "get_step");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "inverse_mass"), "", "get_inverse_mass");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "total_angular_damp"), "", "get_total_angular_damp");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "total_linear_damp"), "", "get_total_linear_damp");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "inverse_inertia"), "", "get_inverse_inertia");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "total_gravity"), "", "get_total_gravity");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "center_of_mass"), "", "get_center_of_mass");
    ADD_PROPERTY(PropertyInfo(VariantType::BASIS, "principal_inertia_axes"), "", "get_principal_inertia_axes");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "angular_velocity"), "set_angular_velocity", "get_angular_velocity");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "linear_velocity"), "set_linear_velocity", "get_linear_velocity");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "sleeping"), "set_sleep_state", "is_sleeping");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM2D, "transform"), "set_transform", "get_transform");
}

PhysicsDirectBodyState3D::PhysicsDirectBodyState3D() {}

///////////////////////////////////////////////////////

void PhysicsShapeQueryParameters3D::set_shape(const RES &p_shape) {

    ERR_FAIL_COND(not p_shape);
    shape = p_shape->get_phys_rid();
}

void PhysicsShapeQueryParameters3D::set_shape_rid(const RID &p_shape) {

    shape = p_shape;
}

RID PhysicsShapeQueryParameters3D::get_shape_rid() const {

    return shape;
}

void PhysicsShapeQueryParameters3D::set_transform(const Transform &p_transform) {

    transform = p_transform;
}
Transform PhysicsShapeQueryParameters3D::get_transform() const {

    return transform;
}

void PhysicsShapeQueryParameters3D::set_margin(float p_margin) {

    margin = p_margin;
}

float PhysicsShapeQueryParameters3D::get_margin() const {

    return margin;
}

void PhysicsShapeQueryParameters3D::set_collision_mask(uint32_t p_collision_mask) {

    collision_mask = p_collision_mask;
}
uint32_t PhysicsShapeQueryParameters3D::get_collision_mask() const {

    return collision_mask;
}
void PhysicsShapeQueryParameters3D::set_exclude(const PoolVector<RID> &p_exclude) {

    exclude.clear();
    for (int i = 0; i < p_exclude.size(); i++)
        exclude.insert(p_exclude[i]);
}

PoolVector<RID> PhysicsShapeQueryParameters3D::get_exclude() const {

    PoolVector<RID> ret;
    ret.resize(exclude.size());
    int idx = 0;
    auto wr(ret.write());
    for (const RID &E : exclude) {
        wr[idx++] = E;
    }
    return ret;
}

void PhysicsShapeQueryParameters3D::set_collide_with_bodies(bool p_enable) {
    collide_with_bodies = p_enable;
}

bool PhysicsShapeQueryParameters3D::is_collide_with_bodies_enabled() const {
    return collide_with_bodies;
}

void PhysicsShapeQueryParameters3D::set_collide_with_areas(bool p_enable) {
    collide_with_areas = p_enable;
}

bool PhysicsShapeQueryParameters3D::is_collide_with_areas_enabled() const {
    return collide_with_areas;
}

void PhysicsShapeQueryParameters3D::_bind_methods() {

    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,set_shape);
    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,set_shape_rid);
    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,get_shape_rid);

    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,set_transform);
    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,get_transform);

    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,set_margin);
    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,get_margin);

    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,set_collision_mask);
    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,get_collision_mask);

    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,set_exclude);
    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,get_exclude);

    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,set_collide_with_bodies);
    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,is_collide_with_bodies_enabled);

    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,set_collide_with_areas);
    SE_BIND_METHOD(PhysicsShapeQueryParameters3D,is_collide_with_areas_enabled);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_mask", PropertyHint::Layers3DPhysics), "set_collision_mask", "get_collision_mask");
    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "exclude", PropertyHint::None, itos(int8_t(VariantType::_RID)) + ":"), "set_exclude", "get_exclude");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "margin", PropertyHint::Range, "0,100,0.01"), "set_margin", "get_margin");
    //ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "shape", PropertyHint::ResourceType, "Shape2D"), "set_shape", ""); // FIXME: Lacks a getter
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "shape_rid"), "set_shape_rid", "get_shape_rid");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM, "transform"), "set_transform", "get_transform");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "collide_with_bodies"), "set_collide_with_bodies", "is_collide_with_bodies_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "collide_with_areas"), "set_collide_with_areas", "is_collide_with_areas_enabled");
}

PhysicsShapeQueryParameters3D::PhysicsShapeQueryParameters3D() {

    margin = 0;
    collision_mask = 0x7FFFFFFF;
    collide_with_bodies = true;
    collide_with_areas = false;
}

/////////////////////////////////////

Dictionary PhysicsDirectSpaceState3D::_intersect_ray(const Vector3 &p_from, const Vector3 &p_to, const Array &p_exclude, uint32_t p_collision_mask, bool p_collide_with_bodies, bool p_collide_with_areas) {

    RayResult inters;
    HashSet<RID> exclude;
    exclude.reserve(p_exclude.size());

    for(const Variant & v : p_exclude.vals())
        exclude.insert(v.as<RID>());

    bool res = intersect_ray(p_from, p_to, inters, exclude, p_collision_mask, p_collide_with_bodies, p_collide_with_areas);

    if (!res)
        return Dictionary();

    Dictionary d;
    d["position"] = inters.position;
    d["normal"] = inters.normal;
    d["collider_id"] = Variant::from(inters.collider_id);
    d["collider"] = Variant(inters.collider);
    d["shape"] = inters.shape;
    d["rid"] = inters.rid;

    return d;
}

Array PhysicsDirectSpaceState3D::_intersect_point(const Vector3 &p_point, int p_max_results,
        const Vector<RID> &p_exclude,
        uint32_t p_layers, bool p_collide_with_bodies, bool p_collide_with_areas) {
    Set<RID> exclude;
    for (int i = 0; i < p_exclude.size(); i++) {
        exclude.insert(p_exclude[i]);
    }

    Vector<ShapeResult> ret;
    ret.resize(p_max_results);
    HashSet<RID> excl_set;
    excl_set.insert(p_exclude.begin(), p_exclude.end());
    int rc = intersect_point(
            p_point, ret.data(), ret.size(), excl_set, p_layers, p_collide_with_bodies, p_collide_with_areas);

    if (rc == 0) {
        return Array();
    }

    Array r;
    r.resize(rc);
    for (int i = 0; i < rc; i++) {
        Dictionary d;
        d["rid"] = ret[i].rid;
        d["collider_id"] = Variant::from(ret[i].collider_id);
        d["collider"] = Variant(ret[i].collider);
        d["shape"] = ret[i].shape;
        r[i] = d;
    }
    return r;
}

Array PhysicsDirectSpaceState3D::_intersect_shape(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query, int p_max_results) {

    ERR_FAIL_COND_V(not p_shape_query, Array());

    Vector<ShapeResult> sr;
    sr.resize(p_max_results);
    int rc = intersect_shape(p_shape_query->shape, p_shape_query->transform, p_shape_query->margin, sr.data(), sr.size(), p_shape_query->exclude, p_shape_query->collision_mask, p_shape_query->collide_with_bodies, p_shape_query->collide_with_areas);
    Vector<Variant> ret;
    ret.reserve(rc);
    for (int i = 0; i < rc; i++) {

        Dictionary d;
        d["rid"] = sr[i].rid;
        d["collider_id"] = Variant::from(sr[i].collider_id);
        d["collider"] = Variant(sr[i].collider);
        d["shape"] = sr[i].shape;
        ret.emplace_back(eastl::move(d));
    }

    return Array(eastl::move(ret));
}

Array PhysicsDirectSpaceState3D::_cast_motion(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query, const Vector3 &p_motion) {

    ERR_FAIL_COND_V(not p_shape_query, Array());

    float closest_safe, closest_unsafe;
    bool res = cast_motion(p_shape_query->shape, p_shape_query->transform, p_motion, p_shape_query->margin, closest_safe, closest_unsafe, p_shape_query->exclude, p_shape_query->collision_mask, p_shape_query->collide_with_bodies, p_shape_query->collide_with_areas);
    if (!res)
        return Array();
    Array ret;
    ret.resize(2);
    ret[0] = closest_safe;
    ret[1] = closest_unsafe;
    return ret;
}
Array PhysicsDirectSpaceState3D::_collide_shape(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query, int p_max_results) {

    ERR_FAIL_COND_V(not p_shape_query, Array());

    Vector<Vector3> ret;
    ret.resize(p_max_results * 2);
    int rc = 0;
    bool res = collide_shape(p_shape_query->shape, p_shape_query->transform, p_shape_query->margin, ret.data(), p_max_results, rc, p_shape_query->exclude, p_shape_query->collision_mask, p_shape_query->collide_with_bodies, p_shape_query->collide_with_areas);
    if (!res) {
        return Array();
    }
    Vector<Variant> r;
    r.resize(rc * 2);
    for (int i = 0; i < rc * 2; i++) {
        r[i] = ret[i];
    }
    return Array(eastl::move(r));
}
Dictionary PhysicsDirectSpaceState3D::_get_rest_info(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query) {

    ERR_FAIL_COND_V(not p_shape_query, Dictionary());

    ShapeRestInfo sri;

    bool res = rest_info(p_shape_query->shape, p_shape_query->transform, p_shape_query->margin, &sri, p_shape_query->exclude, p_shape_query->collision_mask, p_shape_query->collide_with_bodies, p_shape_query->collide_with_areas);
    Dictionary r;
    if (!res)
        return r;

    r["point"] = sri.point;
    r["normal"] = sri.normal;
    r["rid"] = sri.rid;
    r["collider_id"] = Variant::from(sri.collider_id);
    r["shape"] = sri.shape;
    r["linear_velocity"] = sri.linear_velocity;

    return r;
}

PhysicsDirectSpaceState3D::PhysicsDirectSpaceState3D() {
}

void PhysicsDirectSpaceState3D::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("intersect_point", { "point", "max_results", "exclude", "collision_layer",
                                                                  "collide_with_bodies", "collide_with_areas" }),
            &PhysicsDirectSpaceState3D::_intersect_point,
            { DEFVAL(32), DEFVAL(Array()), DEFVAL(0x7FFFFFFF), DEFVAL(true), DEFVAL(false) });

    MethodBinder::bind_method(D_METHOD("intersect_ray", { "from", "to", "exclude", "collision_mask",
                                                                "collide_with_bodies", "collide_with_areas" }),
            &PhysicsDirectSpaceState3D::_intersect_ray,
            { DEFVAL(Array()), DEFVAL(0x7FFFFFFF), DEFVAL(true), DEFVAL(false) });
    MethodBinder::bind_method(D_METHOD("intersect_shape", {"shape", "max_results"}), &PhysicsDirectSpaceState3D::_intersect_shape, {DEFVAL(32)});
    MethodBinder::bind_method(D_METHOD("cast_motion", {"shape", "motion"}), &PhysicsDirectSpaceState3D::_cast_motion);
    MethodBinder::bind_method(D_METHOD("collide_shape", {"shape", "max_results"}), &PhysicsDirectSpaceState3D::_collide_shape, {DEFVAL(32)});
    MethodBinder::bind_method(D_METHOD("get_rest_info", {"shape"}), &PhysicsDirectSpaceState3D::_get_rest_info);
}

///////////////////////////////

Vector3 PhysicsTestMotionResult::get_motion() const {
    return result.motion;
}

Vector3 PhysicsTestMotionResult::get_motion_remainder() const {
    return result.remainder;
}

Vector3 PhysicsTestMotionResult::get_collision_point() const {
    return result.collision_point;
}

Vector3 PhysicsTestMotionResult::get_collision_normal() const {
    return result.collision_normal;
}

Vector3 PhysicsTestMotionResult::get_collider_velocity() const {
    return result.collider_velocity;
}

GameEntity PhysicsTestMotionResult::get_collider_id() const {
    return result.collider_id;
}

RID PhysicsTestMotionResult::get_collider_rid() const {
    return result.collider;
}

Object *PhysicsTestMotionResult::get_collider() const {
    return object_for_entity(result.collider_id);
}

int PhysicsTestMotionResult::get_collider_shape() const {
    return result.collider_shape;
}

real_t PhysicsTestMotionResult::get_collision_depth() const {
    return result.collision_depth;
}

real_t PhysicsTestMotionResult::get_collision_safe_fraction() const {
    return result.collision_safe_fraction;
}

real_t PhysicsTestMotionResult::get_collision_unsafe_fraction() const {
    return result.collision_unsafe_fraction;
}

PhysicsTestMotionResult::PhysicsTestMotionResult() {
}

void PhysicsTestMotionResult::_bind_methods() {
    SE_BIND_METHOD(PhysicsTestMotionResult,get_motion);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_motion_remainder);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_collision_point);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_collision_normal);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_collider_velocity);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_collider_id);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_collider_rid);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_collider);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_collider_shape);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_collision_depth);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_collision_safe_fraction);
    SE_BIND_METHOD(PhysicsTestMotionResult,get_collision_unsafe_fraction);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "motion"), "", "get_motion");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "motion_remainder"), "", "get_motion_remainder");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "collision_point"), "", "get_collision_point");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "collision_normal"), "", "get_collision_normal");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "collider_velocity"), "", "get_collider_velocity");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collider_id", PropertyHint::ObjectID), "", "get_collider_id");
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "collider_rid"), "", "get_collider_rid");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "collider"), "", "get_collider");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collider_shape"), "", "get_collider_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "collision_depth"), "", "get_collision_depth");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "collision_safe_fraction"), "", "get_collision_safe_fraction");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "collision_unsafe_fraction"), "", "get_collision_unsafe_fraction");
}
///////////////////////////////////////

bool PhysicsServer3D::_body_test_motion(RID p_body, const Transform &p_from, const Vector3 &p_motion, bool p_infinite_inertia, const Ref<PhysicsTestMotionResult> &p_result, bool p_exclude_raycast_shapes, const Vector<RID> &p_exclude) {
    MotionResult *r = nullptr;
    if (p_result) {
        r = p_result->get_result_ptr();
    }
    Set<RID> exclude;
    for (int i = 0; i < p_exclude.size(); i++) {
        exclude.insert(p_exclude[i]);
    }
    return body_test_motion(p_body, p_from, p_motion, p_infinite_inertia, r, p_exclude_raycast_shapes, exclude);
}
void PhysicsServer3D::_bind_methods() {

#ifndef _3D_DISABLED

    SE_BIND_METHOD(PhysicsServer3D,shape_create);
    SE_BIND_METHOD(PhysicsServer3D,shape_set_data);

    SE_BIND_METHOD(PhysicsServer3D,shape_get_type);
    SE_BIND_METHOD(PhysicsServer3D,shape_get_data);

    SE_BIND_METHOD(PhysicsServer3D,space_create);
    SE_BIND_METHOD(PhysicsServer3D,space_set_active);
    SE_BIND_METHOD(PhysicsServer3D,space_is_active);
    SE_BIND_METHOD(PhysicsServer3D,space_set_param);
    SE_BIND_METHOD(PhysicsServer3D,space_get_param);
    SE_BIND_METHOD(PhysicsServer3D,space_get_direct_state);

    SE_BIND_METHOD(PhysicsServer3D,area_create);
    SE_BIND_METHOD(PhysicsServer3D,area_set_space);
    SE_BIND_METHOD(PhysicsServer3D,area_get_space);

    SE_BIND_METHOD(PhysicsServer3D,area_set_space_override_mode);
    SE_BIND_METHOD(PhysicsServer3D,area_get_space_override_mode);

    MethodBinder::bind_method(D_METHOD("area_add_shape", {"area", "shape", "transform", "disabled"}), &PhysicsServer3D::area_add_shape, {DEFVAL(Transform()), DEFVAL(false)});
    SE_BIND_METHOD(PhysicsServer3D,area_set_shape);
    SE_BIND_METHOD(PhysicsServer3D,area_set_shape_transform);
    SE_BIND_METHOD(PhysicsServer3D,area_set_shape_disabled);

    SE_BIND_METHOD(PhysicsServer3D,area_get_shape_count);
    SE_BIND_METHOD(PhysicsServer3D,area_get_shape);
    SE_BIND_METHOD(PhysicsServer3D,area_get_shape_transform);

    SE_BIND_METHOD(PhysicsServer3D,area_remove_shape);
    SE_BIND_METHOD(PhysicsServer3D,area_clear_shapes);

    SE_BIND_METHOD(PhysicsServer3D,area_set_collision_layer);
    SE_BIND_METHOD(PhysicsServer3D,area_set_collision_mask);

    SE_BIND_METHOD(PhysicsServer3D,area_set_param);
    SE_BIND_METHOD(PhysicsServer3D,area_set_transform);

    SE_BIND_METHOD(PhysicsServer3D,area_get_param);
    SE_BIND_METHOD(PhysicsServer3D,area_get_transform);

    SE_BIND_METHOD(PhysicsServer3D,area_attach_object_instance_id);
    SE_BIND_METHOD(PhysicsServer3D,area_get_object_instance_id);

    SE_BIND_METHOD(PhysicsServer3D,area_set_monitor_callback);
    SE_BIND_METHOD(PhysicsServer3D,area_set_area_monitor_callback);
    SE_BIND_METHOD(PhysicsServer3D,area_set_monitorable);

    SE_BIND_METHOD(PhysicsServer3D,area_set_ray_pickable);
    SE_BIND_METHOD(PhysicsServer3D,area_is_ray_pickable);

    MethodBinder::bind_method(D_METHOD("body_create", {"mode", "init_sleeping"}), &PhysicsServer3D::body_create, {DEFVAL(BODY_MODE_RIGID), DEFVAL(false)});

    SE_BIND_METHOD(PhysicsServer3D,body_set_space);
    SE_BIND_METHOD(PhysicsServer3D,body_get_space);

    SE_BIND_METHOD(PhysicsServer3D,body_set_mode);
    SE_BIND_METHOD(PhysicsServer3D,body_get_mode);

    SE_BIND_METHOD(PhysicsServer3D,body_set_collision_layer);
    SE_BIND_METHOD(PhysicsServer3D,body_get_collision_layer);

    SE_BIND_METHOD(PhysicsServer3D,body_set_collision_mask);
    SE_BIND_METHOD(PhysicsServer3D,body_get_collision_mask);

    MethodBinder::bind_method(D_METHOD("body_add_shape", {"body", "shape", "transform", "disabled"}), &PhysicsServer3D::body_add_shape, {DEFVAL(Transform()), DEFVAL(false)});
    SE_BIND_METHOD(PhysicsServer3D,body_set_shape);
    SE_BIND_METHOD(PhysicsServer3D,body_set_shape_transform);
    SE_BIND_METHOD(PhysicsServer3D,body_set_shape_disabled);

    SE_BIND_METHOD(PhysicsServer3D,body_get_shape_count);
    SE_BIND_METHOD(PhysicsServer3D,body_get_shape);
    SE_BIND_METHOD(PhysicsServer3D,body_get_shape_transform);

    SE_BIND_METHOD(PhysicsServer3D,body_remove_shape);
    SE_BIND_METHOD(PhysicsServer3D,body_clear_shapes);

    SE_BIND_METHOD(PhysicsServer3D,body_attach_object_instance_id);
    SE_BIND_METHOD(PhysicsServer3D,body_get_object_instance_id);

    SE_BIND_METHOD(PhysicsServer3D,body_set_enable_continuous_collision_detection);
    SE_BIND_METHOD(PhysicsServer3D,body_is_continuous_collision_detection_enabled);

    SE_BIND_METHOD(PhysicsServer3D,body_set_param);
    SE_BIND_METHOD(PhysicsServer3D,body_get_param);

    SE_BIND_METHOD(PhysicsServer3D,body_set_kinematic_safe_margin);
    SE_BIND_METHOD(PhysicsServer3D,body_get_kinematic_safe_margin);

    SE_BIND_METHOD(PhysicsServer3D,body_set_state);
    SE_BIND_METHOD(PhysicsServer3D,body_get_state);

    SE_BIND_METHOD(PhysicsServer3D,body_add_central_force);
    SE_BIND_METHOD(PhysicsServer3D,body_add_force);
    SE_BIND_METHOD(PhysicsServer3D,body_add_torque);

    SE_BIND_METHOD(PhysicsServer3D,body_apply_central_impulse);
    SE_BIND_METHOD(PhysicsServer3D,body_apply_impulse);
    SE_BIND_METHOD(PhysicsServer3D,body_apply_torque_impulse);
    SE_BIND_METHOD(PhysicsServer3D,body_set_axis_velocity);

    SE_BIND_METHOD(PhysicsServer3D,body_set_axis_lock);
    SE_BIND_METHOD(PhysicsServer3D,body_is_axis_locked);

    SE_BIND_METHOD(PhysicsServer3D,body_add_collision_exception);
    SE_BIND_METHOD(PhysicsServer3D,body_remove_collision_exception);

    SE_BIND_METHOD(PhysicsServer3D,body_set_max_contacts_reported);
    SE_BIND_METHOD(PhysicsServer3D,body_get_max_contacts_reported);

    SE_BIND_METHOD(PhysicsServer3D,body_set_omit_force_integration);
    SE_BIND_METHOD(PhysicsServer3D,body_is_omitting_force_integration);

    SE_BIND_METHOD(PhysicsServer3D,body_set_force_integration_callback);

    SE_BIND_METHOD(PhysicsServer3D,body_set_ray_pickable);
    SE_BIND_METHOD(PhysicsServer3D,body_is_ray_pickable);

    MethodBinder::bind_method(D_METHOD("body_test_motion", {"body", "from", "motion", "infinite_inertia", "result", "exclude_raycast_shapes", "exclude"}), &PhysicsServer3D::_body_test_motion, {DEFVAL(Variant()), DEFVAL(true), DEFVAL(Array())});

    SE_BIND_METHOD(PhysicsServer3D,body_get_direct_state);

    /* JOINT API */

    BIND_ENUM_CONSTANT(JOINT_PIN);
    BIND_ENUM_CONSTANT(JOINT_HINGE);
    BIND_ENUM_CONSTANT(JOINT_SLIDER);
    BIND_ENUM_CONSTANT(JOINT_CONE_TWIST);
    BIND_ENUM_CONSTANT(JOINT_6DOF);

    SE_BIND_METHOD(PhysicsServer3D,joint_create_pin);
    SE_BIND_METHOD(PhysicsServer3D,pin_joint_set_param);
    SE_BIND_METHOD(PhysicsServer3D,pin_joint_get_param);

    SE_BIND_METHOD(PhysicsServer3D,pin_joint_set_local_a);
    SE_BIND_METHOD(PhysicsServer3D,pin_joint_get_local_a);

    SE_BIND_METHOD(PhysicsServer3D,pin_joint_set_local_b);
    SE_BIND_METHOD(PhysicsServer3D,pin_joint_get_local_b);

    BIND_ENUM_CONSTANT(PIN_JOINT_BIAS);
    BIND_ENUM_CONSTANT(PIN_JOINT_DAMPING);
    BIND_ENUM_CONSTANT(PIN_JOINT_IMPULSE_CLAMP);

    BIND_ENUM_CONSTANT(HINGE_JOINT_BIAS);
    BIND_ENUM_CONSTANT(HINGE_JOINT_LIMIT_UPPER);
    BIND_ENUM_CONSTANT(HINGE_JOINT_LIMIT_LOWER);
    BIND_ENUM_CONSTANT(HINGE_JOINT_LIMIT_BIAS);
    BIND_ENUM_CONSTANT(HINGE_JOINT_LIMIT_SOFTNESS);
    BIND_ENUM_CONSTANT(HINGE_JOINT_LIMIT_RELAXATION);
    BIND_ENUM_CONSTANT(HINGE_JOINT_MOTOR_TARGET_VELOCITY);
    BIND_ENUM_CONSTANT(HINGE_JOINT_MOTOR_MAX_IMPULSE);

    BIND_ENUM_CONSTANT(HINGE_JOINT_FLAG_USE_LIMIT);
    BIND_ENUM_CONSTANT(HINGE_JOINT_FLAG_ENABLE_MOTOR);

    SE_BIND_METHOD(PhysicsServer3D,joint_create_hinge);

    SE_BIND_METHOD(PhysicsServer3D,hinge_joint_set_param);
    SE_BIND_METHOD(PhysicsServer3D,hinge_joint_get_param);

    SE_BIND_METHOD(PhysicsServer3D,hinge_joint_set_flag);
    SE_BIND_METHOD(PhysicsServer3D,hinge_joint_get_flag);

    SE_BIND_METHOD(PhysicsServer3D,joint_create_slider);

    SE_BIND_METHOD(PhysicsServer3D,slider_joint_set_param);
    SE_BIND_METHOD(PhysicsServer3D,slider_joint_get_param);

    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_UPPER);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_LOWER);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_DAMPING);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_MOTION_SOFTNESS);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_MOTION_RESTITUTION);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_MOTION_DAMPING);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_ORTHOGONAL_SOFTNESS);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_ORTHOGONAL_RESTITUTION);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_ORTHOGONAL_DAMPING);

    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_UPPER);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_LOWER);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_RESTITUTION);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_DAMPING);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_MOTION_SOFTNESS);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_MOTION_RESTITUTION);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_MOTION_DAMPING);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_ORTHOGONAL_SOFTNESS);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_ORTHOGONAL_RESTITUTION);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_ORTHOGONAL_DAMPING);
    BIND_ENUM_CONSTANT(SLIDER_JOINT_MAX);

    SE_BIND_METHOD(PhysicsServer3D,joint_create_cone_twist);

    SE_BIND_METHOD(PhysicsServer3D,cone_twist_joint_set_param);
    SE_BIND_METHOD(PhysicsServer3D,cone_twist_joint_get_param);

    BIND_ENUM_CONSTANT(CONE_TWIST_JOINT_SWING_SPAN);
    BIND_ENUM_CONSTANT(CONE_TWIST_JOINT_TWIST_SPAN);
    BIND_ENUM_CONSTANT(CONE_TWIST_JOINT_BIAS);
    BIND_ENUM_CONSTANT(CONE_TWIST_JOINT_SOFTNESS);
    BIND_ENUM_CONSTANT(CONE_TWIST_JOINT_RELAXATION);

    BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_LOWER_LIMIT);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_UPPER_LIMIT);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_RESTITUTION);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_DAMPING);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_MOTOR_TARGET_VELOCITY);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_MOTOR_FORCE_LIMIT);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_LOWER_LIMIT);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_UPPER_LIMIT);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_DAMPING);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_RESTITUTION);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_FORCE_LIMIT);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_ERP);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_MOTOR_TARGET_VELOCITY);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_MOTOR_FORCE_LIMIT);

    BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_MOTOR);
    BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_LINEAR_MOTOR);

    SE_BIND_METHOD(PhysicsServer3D,joint_get_type);

    SE_BIND_METHOD(PhysicsServer3D,joint_set_solver_priority);
    SE_BIND_METHOD(PhysicsServer3D,joint_get_solver_priority);

    SE_BIND_METHOD(PhysicsServer3D,joint_create_generic_6dof);

    SE_BIND_METHOD(PhysicsServer3D,generic_6dof_joint_set_param);
    SE_BIND_METHOD(PhysicsServer3D,generic_6dof_joint_get_param);

    SE_BIND_METHOD(PhysicsServer3D,generic_6dof_joint_set_flag);
    SE_BIND_METHOD(PhysicsServer3D,generic_6dof_joint_get_flag);

    SE_BIND_METHOD(PhysicsServer3D,free_rid);

    SE_BIND_METHOD(PhysicsServer3D,set_active);
  
    SE_BIND_METHOD(PhysicsServer3D,set_collision_iterations);

    SE_BIND_METHOD(PhysicsServer3D,get_process_info);

    BIND_ENUM_CONSTANT(SHAPE_PLANE);
    BIND_ENUM_CONSTANT(SHAPE_RAY);
    BIND_ENUM_CONSTANT(SHAPE_SPHERE);
    BIND_ENUM_CONSTANT(SHAPE_BOX);
    BIND_ENUM_CONSTANT(SHAPE_CAPSULE);
    BIND_ENUM_CONSTANT(SHAPE_CYLINDER);
    BIND_ENUM_CONSTANT(SHAPE_CONVEX_POLYGON);
    BIND_ENUM_CONSTANT(SHAPE_CONCAVE_POLYGON);
    BIND_ENUM_CONSTANT(SHAPE_HEIGHTMAP);
    BIND_ENUM_CONSTANT(SHAPE_CUSTOM);

    BIND_ENUM_CONSTANT(AREA_PARAM_GRAVITY);
    BIND_ENUM_CONSTANT(AREA_PARAM_GRAVITY_VECTOR);
    BIND_ENUM_CONSTANT(AREA_PARAM_GRAVITY_IS_POINT);
    BIND_ENUM_CONSTANT(AREA_PARAM_GRAVITY_DISTANCE_SCALE);
    BIND_ENUM_CONSTANT(AREA_PARAM_GRAVITY_POINT_ATTENUATION);
    BIND_ENUM_CONSTANT(AREA_PARAM_LINEAR_DAMP);
    BIND_ENUM_CONSTANT(AREA_PARAM_ANGULAR_DAMP);
    BIND_ENUM_CONSTANT(AREA_PARAM_PRIORITY);

    BIND_ENUM_CONSTANT(AREA_SPACE_OVERRIDE_DISABLED);
    BIND_ENUM_CONSTANT(AREA_SPACE_OVERRIDE_COMBINE);
    BIND_ENUM_CONSTANT(AREA_SPACE_OVERRIDE_COMBINE_REPLACE);
    BIND_ENUM_CONSTANT(AREA_SPACE_OVERRIDE_REPLACE);
    BIND_ENUM_CONSTANT(AREA_SPACE_OVERRIDE_REPLACE_COMBINE);

    BIND_ENUM_CONSTANT(BODY_MODE_STATIC);
    BIND_ENUM_CONSTANT(BODY_MODE_KINEMATIC);
    BIND_ENUM_CONSTANT(BODY_MODE_RIGID);
    BIND_ENUM_CONSTANT(BODY_MODE_CHARACTER);

    BIND_ENUM_CONSTANT(BODY_PARAM_BOUNCE);
    BIND_ENUM_CONSTANT(BODY_PARAM_FRICTION);
    BIND_ENUM_CONSTANT(BODY_PARAM_MASS);
    BIND_ENUM_CONSTANT(BODY_PARAM_GRAVITY_SCALE);
    BIND_ENUM_CONSTANT(BODY_PARAM_LINEAR_DAMP);
    BIND_ENUM_CONSTANT(BODY_PARAM_ANGULAR_DAMP);
    BIND_ENUM_CONSTANT(BODY_PARAM_MAX);

    BIND_ENUM_CONSTANT(BODY_STATE_TRANSFORM);
    BIND_ENUM_CONSTANT(BODY_STATE_LINEAR_VELOCITY);
    BIND_ENUM_CONSTANT(BODY_STATE_ANGULAR_VELOCITY);
    BIND_ENUM_CONSTANT(BODY_STATE_SLEEPING);
    BIND_ENUM_CONSTANT(BODY_STATE_CAN_SLEEP);

    BIND_ENUM_CONSTANT(AREA_BODY_ADDED);
    BIND_ENUM_CONSTANT(AREA_BODY_REMOVED);

    BIND_ENUM_CONSTANT(INFO_ACTIVE_OBJECTS);
    BIND_ENUM_CONSTANT(INFO_COLLISION_PAIRS);
    BIND_ENUM_CONSTANT(INFO_ISLAND_COUNT);

    BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_RECYCLE_RADIUS);
    BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_MAX_SEPARATION);
    BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_MAX_ALLOWED_PENETRATION);
    BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_THRESHOLD);
    BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_ANGULAR_VELOCITY_SLEEP_THRESHOLD);
    BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_TIME_TO_SLEEP);
    BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_ANGULAR_VELOCITY_DAMP_RATIO);
    BIND_ENUM_CONSTANT(SPACE_PARAM_CONSTRAINT_DEFAULT_BIAS);

    BIND_ENUM_CONSTANT(BODY_AXIS_LINEAR_X);
    BIND_ENUM_CONSTANT(BODY_AXIS_LINEAR_Y);
    BIND_ENUM_CONSTANT(BODY_AXIS_LINEAR_Z);
    BIND_ENUM_CONSTANT(BODY_AXIS_ANGULAR_X);
    BIND_ENUM_CONSTANT(BODY_AXIS_ANGULAR_Y);
    BIND_ENUM_CONSTANT(BODY_AXIS_ANGULAR_Z);

#endif
}

PhysicsServer3D::PhysicsServer3D() {

    ERR_FAIL_COND(singleton != nullptr);
    singleton = this;
}

PhysicsServer3D::~PhysicsServer3D() {

    singleton = nullptr;
}

Vector<PhysicsServerManager::ClassInfo> PhysicsServerManager::physics_servers;
int PhysicsServerManager::default_server_id = -1;
int PhysicsServerManager::current_server_id = -1;
int PhysicsServerManager::default_server_priority = -1;
const StaticCString PhysicsServerManager::setting_property_name("physics/3d/physics_engine");

void PhysicsServerManager::on_servers_changed() {

    String physics_servers2("DEFAULT");
    for (int i = get_servers_count() - 1; 0 <= i; --i) {
        physics_servers2 += "," + String(get_server_name(i));
    }
    ProjectSettings::get_singleton()->set_custom_property_info(setting_property_name,
            PropertyInfo(VariantType::STRING, StringName(setting_property_name), PropertyHint::Enum, physics_servers2));
}

void PhysicsServerManager::register_server(const StringName &p_name, CreatePhysicsServerCallback p_creat_callback) {

    ERR_FAIL_COND(!p_creat_callback);
    ERR_FAIL_COND(find_server_id(p_name) != -1);
    physics_servers.push_back(ClassInfo(p_name, p_creat_callback));
    on_servers_changed();
}

void PhysicsServerManager::set_default_server(const StringName &p_name, int p_priority) {

    const int id = find_server_id(p_name);
    ERR_FAIL_COND(id == -1 ); // Not found
    if (default_server_priority < p_priority) {
        default_server_id = id;
        default_server_priority = p_priority;
    }
}

int PhysicsServerManager::find_server_id(const StringName &p_name) {

    for (int i = physics_servers.size() - 1; 0 <= i; --i) {
        if (p_name == physics_servers[i].name) {
            return i;
        }
    }
    return -1;
}

int PhysicsServerManager::get_servers_count() {
    return physics_servers.size();
}

StringName PhysicsServerManager::get_server_name(int p_id) {
    ERR_FAIL_INDEX_V(p_id, get_servers_count(), StringName());
    return physics_servers[p_id].name;
}

PhysicsServer3D *PhysicsServerManager::new_default_server() {
    ERR_FAIL_COND_V(default_server_id == -1, nullptr);
    current_server_id = default_server_id;
    return physics_servers[default_server_id].create_callback();
}

PhysicsServer3D *PhysicsServerManager::new_server(const StringName &p_name) {
    int id = find_server_id(p_name);
    if (id == -1) {
        return nullptr;
    } else {
        current_server_id = id;
        return physics_servers[id].create_callback();
    }
}

void PhysicsServerManager::cleanup()
{
    physics_servers.clear();
    default_server_id = -1;
    default_server_priority = -1;
}
PhysicsServer3D * initialize_3d_physics() {
    PhysicsServer3D *physics_server_3d = PhysicsServerManager::new_server(ProjectSettings::get_singleton()->getT<StringName>(PhysicsServerManager::setting_property_name));
    if (!physics_server_3d) {
        // Physics server not found, Use the default physics
        physics_server_3d = PhysicsServerManager::new_default_server();
    }
    ERR_FAIL_COND_V(!physics_server_3d,nullptr);
    physics_server_3d->init();
    return physics_server_3d;
}
