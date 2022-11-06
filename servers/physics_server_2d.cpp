/*************************************************************************/
/*  physics_server_2d.cpp                                                */
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

#include "physics_server_2d.h"

#include "core/dictionary.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/method_enum_caster.h"
#include "core/pool_vector.h"

IMPL_GDCLASS(PhysicsDirectBodyState2D)
IMPL_GDCLASS(PhysicsShapeQueryParameters2D)
IMPL_GDCLASS(PhysicsDirectSpaceState2D)
IMPL_GDCLASS(PhysicsServer2D)
IMPL_GDCLASS(Physics2DTestMotionResult)

VARIANT_ENUM_CAST(PhysicsServer2D::ShapeType);
VARIANT_ENUM_CAST(PhysicsServer2D::SpaceParameter);
VARIANT_ENUM_CAST(PhysicsServer2D::AreaParameter);
VARIANT_ENUM_CAST(PhysicsServer2D::AreaSpaceOverrideMode);
VARIANT_ENUM_CAST(PhysicsServer2D::BodyMode);
VARIANT_ENUM_CAST(PhysicsServer2D::BodyParameter);
VARIANT_ENUM_CAST(PhysicsServer2D::BodyState);
VARIANT_ENUM_CAST(PhysicsServer2D::CCDMode);
VARIANT_ENUM_CAST(PhysicsServer2D::JointParam);
VARIANT_ENUM_CAST(PhysicsServer2D::JointType);
VARIANT_ENUM_CAST(PhysicsServer2D::DampedStringParam);
//VARIANT_ENUM_CAST( PhysicsServer2D::ObjectType );
VARIANT_ENUM_CAST(PhysicsServer2D::AreaBodyStatus);
VARIANT_ENUM_CAST(PhysicsServer2D::ProcessInfo);

PhysicsServer2D *PhysicsServer2D::queueing_thread_singleton = nullptr;
PhysicsServer2D* PhysicsServer2D::submission_thread_singleton = nullptr;
Thread::ID PhysicsServer2D::server_thread;

namespace {
struct ClassInfo {
    StringName name;
    CreatePhysics2DServerCallback create_callback=nullptr;

    ClassInfo() = default;

    ClassInfo(StringName p_name, CreatePhysics2DServerCallback p_create_callback) :
            name(eastl::move(p_name)),
            create_callback(p_create_callback) {}

    ClassInfo(const ClassInfo &p_ci) = default;

    ClassInfo &operator=(const ClassInfo &p_ci) {
        name = p_ci.name;
        create_callback = p_ci.create_callback;
        return *this;
    }
};

static Vector<ClassInfo> physics_2d_servers;
}
void PhysicsDirectBodyState2D::integrate_forces() {

    real_t step = get_step();
    Vector2 lv = get_linear_velocity();
    lv += get_total_gravity() * step;

    real_t av = get_angular_velocity();

    float damp = 1.0f - step * get_total_linear_damp();

    if (damp < 0) // reached zero in the given time
        damp = 0;

    lv *= damp;

    damp = 1.0f - step * get_total_angular_damp();

    if (damp < 0) // reached zero in the given time
        damp = 0;

    av *= damp;

    set_linear_velocity(lv);
    set_angular_velocity(av);
}

Object *PhysicsDirectBodyState2D::get_contact_collider_object(int p_contact_idx) const {

    GameEntity objid = get_contact_collider_id(p_contact_idx);
    Object *obj = object_for_entity(objid);
    return obj;
}

void PhysicsDirectBodyState2D::_bind_methods() {

    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_total_gravity);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_total_linear_damp);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_total_angular_damp);

    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_inverse_mass);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_inverse_inertia);

    SE_BIND_METHOD(PhysicsDirectBodyState2D,set_linear_velocity);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_linear_velocity);

    SE_BIND_METHOD(PhysicsDirectBodyState2D,set_angular_velocity);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_angular_velocity);

    SE_BIND_METHOD(PhysicsDirectBodyState2D,set_transform);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_transform);

    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_velocity_at_local_position);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,add_central_force);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,add_force);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,add_torque);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,apply_central_impulse);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,apply_torque_impulse);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,apply_impulse);

    SE_BIND_METHOD(PhysicsDirectBodyState2D,set_sleep_state);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,is_sleeping);

    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_count);

    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_local_position);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_local_normal);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_local_shape);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_collider);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_collider_position);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_collider_id);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_collider_object);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_collider_shape);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_collider_shape_metadata);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_contact_collider_velocity_at_position);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_step);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,integrate_forces);
    SE_BIND_METHOD(PhysicsDirectBodyState2D,get_space_state);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "step"), "", "get_step");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "inverse_mass"), "", "get_inverse_mass");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "inverse_inertia"), "", "get_inverse_inertia");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "total_angular_damp"), "", "get_total_angular_damp");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "total_linear_damp"), "", "get_total_linear_damp");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "total_gravity"), "", "get_total_gravity");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "angular_velocity"), "set_angular_velocity", "get_angular_velocity");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "linear_velocity"), "set_linear_velocity", "get_linear_velocity");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "sleeping"), "set_sleep_state", "is_sleeping");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM2D, "transform"), "set_transform", "get_transform");
}

PhysicsDirectBodyState2D::PhysicsDirectBodyState2D() {}

///////////////////////////////////////////////////////

void PhysicsShapeQueryParameters2D::set_shape(const RES &p_shape) {

    ERR_FAIL_COND(not p_shape);
    shape = p_shape->get_phys_rid();
}

void PhysicsShapeQueryParameters2D::set_shape_rid(const RID &p_shape) {

    shape = p_shape;
}

RID PhysicsShapeQueryParameters2D::get_shape_rid() const {

    return shape;
}

void PhysicsShapeQueryParameters2D::set_transform(const Transform2D &p_transform) {

    transform = p_transform;
}
Transform2D PhysicsShapeQueryParameters2D::get_transform() const {

    return transform;
}

void PhysicsShapeQueryParameters2D::set_motion(const Vector2 &p_motion) {

    motion = p_motion;
}
Vector2 PhysicsShapeQueryParameters2D::get_motion() const {

    return motion;
}

void PhysicsShapeQueryParameters2D::set_margin(float p_margin) {

    margin = p_margin;
}
float PhysicsShapeQueryParameters2D::get_margin() const {

    return margin;
}

void PhysicsShapeQueryParameters2D::set_collision_mask(uint32_t p_collision_mask) {

    collision_mask = p_collision_mask;
}
uint32_t PhysicsShapeQueryParameters2D::get_collision_mask() const {

    return collision_mask;
}

void PhysicsShapeQueryParameters2D::set_exclude(const PoolVector<RID> &p_exclude) {

    exclude.clear();
    for (int i = 0; i < p_exclude.size(); i++)
        exclude.insert(p_exclude[i]);
}

PoolVector<RID> PhysicsShapeQueryParameters2D::get_exclude() const {

    PoolVector<RID> ret;
    ret.resize(exclude.size());
    int idx = 0;
    auto wr(ret.write());
    for (const RID &E : exclude) {
        wr[idx++] = E;
    }
    return ret;
}

void PhysicsShapeQueryParameters2D::set_collide_with_bodies(bool p_enable) {
    collide_with_bodies = p_enable;
}

bool PhysicsShapeQueryParameters2D::is_collide_with_bodies_enabled() const {
    return collide_with_bodies;
}

void PhysicsShapeQueryParameters2D::set_collide_with_areas(bool p_enable) {
    collide_with_areas = p_enable;
}

bool PhysicsShapeQueryParameters2D::is_collide_with_areas_enabled() const {
    return collide_with_areas;
}

void PhysicsShapeQueryParameters2D::_bind_methods() {

    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,set_shape);
    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,set_shape_rid);
    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,get_shape_rid);

    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,set_transform);
    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,get_transform);

    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,set_motion);
    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,get_motion);

    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,set_margin);
    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,get_margin);

    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,set_collision_mask);
    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,get_collision_mask);

    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,set_exclude);
    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,get_exclude);

    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,set_collide_with_bodies);
    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,is_collide_with_bodies_enabled);

    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,set_collide_with_areas);
    SE_BIND_METHOD(PhysicsShapeQueryParameters2D,is_collide_with_areas_enabled);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_mask", PropertyHint::Layers2DPhysics), "set_collision_mask", "get_collision_mask");
    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "exclude", PropertyHint::None, (itos(int8_t(VariantType::_RID)) + ":")), "set_exclude", "get_exclude");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "margin", PropertyHint::Range, "0,100,0.01"), "set_margin", "get_margin");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "motion"), "set_motion", "get_motion");
    //ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "shape", PropertyHint::ResourceType, "Shape2D"), "set_shape", ""); // FIXME: Lacks a getter
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "shape_rid"), "set_shape_rid", "get_shape_rid");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM2D, "transform"), "set_transform", "get_transform");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "collide_with_bodies"), "set_collide_with_bodies", "is_collide_with_bodies_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "collide_with_areas"), "set_collide_with_areas", "is_collide_with_areas_enabled");
}

PhysicsShapeQueryParameters2D::PhysicsShapeQueryParameters2D() {

    margin = 0;
    collision_mask = 0x7FFFFFFF;
    collide_with_bodies = true;
    collide_with_areas = false;
}

Dictionary PhysicsDirectSpaceState2D::_intersect_ray(const Vector2 &p_from, const Vector2 &p_to, const Array &p_exclude, uint32_t p_layers, bool p_collide_with_bodies, bool p_collide_with_areas) {

    RayResult inters;
    HashSet<RID> exclude;
    exclude.reserve(p_exclude.size());

    for(const Variant & v : p_exclude.vals())
        exclude.insert(v.as<RID>());

    bool res = intersect_ray(p_from, p_to, inters, exclude, p_layers, p_collide_with_bodies, p_collide_with_areas);

    if (!res)
        return Dictionary();

    Dictionary d;
    d["position"] = inters.position;
    d["normal"] = inters.normal;
    d["collider_id"] = Variant::from(inters.collider_id);
    d["collider"] = Variant(inters.collider);
    d["shape"] = inters.shape;
    d["rid"] = inters.rid;
    d["metadata"] = inters.metadata;

    return d;
}

Array PhysicsDirectSpaceState2D::_intersect_shape(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query, int p_max_results) {

    ERR_FAIL_COND_V(not p_shape_query, Array());

    Vector<ShapeResult> sr;
    sr.resize(p_max_results);
    int rc = intersect_shape(p_shape_query->shape, p_shape_query->transform, p_shape_query->motion, p_shape_query->margin, sr.data(), sr.size(), p_shape_query->exclude, p_shape_query->collision_mask, p_shape_query->collide_with_bodies, p_shape_query->collide_with_areas);
    Array ret;
    ret.resize(rc);
    for (int i = 0; i < rc; i++) {

        Dictionary d;
        d["rid"] = sr[i].rid;
        d["collider_id"] = Variant::from(sr[i].collider_id);
        d["collider"] = Variant(sr[i].collider);
        d["shape"] = sr[i].shape;
        d["metadata"] = sr[i].metadata;
        ret[i] = d;
    }

    return ret;
}

Array PhysicsDirectSpaceState2D::_cast_motion(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query) {

    ERR_FAIL_COND_V(not p_shape_query, Array());

    float closest_safe, closest_unsafe;
    bool res = cast_motion(p_shape_query->shape, p_shape_query->transform, p_shape_query->motion, p_shape_query->margin, closest_safe, closest_unsafe, p_shape_query->exclude, p_shape_query->collision_mask, p_shape_query->collide_with_bodies, p_shape_query->collide_with_areas);
    if (!res)
        return Array();
    Array ret;
    ret.resize(2);
    ret[0] = closest_safe;
    ret[1] = closest_unsafe;
    return ret;
}

Array PhysicsDirectSpaceState2D::_intersect_point_impl(const Vector2 &p_point, int p_max_results,
        const Array &p_exclude, uint32_t p_layers, bool p_collide_with_bodies, bool p_collide_with_areas,
        bool p_filter_by_canvas, GameEntity p_canvas_instance_id) {

    HashSet<RID> exclude;

    exclude.reserve(p_exclude.size());

    for(const Variant & v : p_exclude.vals())
        exclude.insert(v.as<RID>());

    Vector<ShapeResult> ret;
    ret.resize(p_max_results);

    int rc;
    if (p_filter_by_canvas)
        rc = intersect_point(p_point, ret.data(), ret.size(), exclude, p_layers, p_collide_with_bodies, p_collide_with_areas);
    else
        rc = intersect_point_on_canvas(p_point, p_canvas_instance_id, ret.data(), ret.size(), exclude, p_layers, p_collide_with_bodies, p_collide_with_areas);

    if (rc == 0)
        return Array();

    Array r;
    r.resize(rc);
    for (int i = 0; i < rc; i++) {

        Dictionary d;
        d["rid"] = ret[i].rid;
        d["collider_id"] = Variant::from(ret[i].collider_id);
        d["collider"] = Variant(ret[i].collider);
        d["shape"] = ret[i].shape;
        d["metadata"] = ret[i].metadata;
        r[i] = d;
    }
    return r;
}

Array PhysicsDirectSpaceState2D::_intersect_point(const Vector2 &p_point, int p_max_results, const Array &p_exclude, uint32_t p_layers, bool p_collide_with_bodies, bool p_collide_with_areas) {

    return _intersect_point_impl(p_point, p_max_results, p_exclude, p_layers, p_collide_with_bodies, p_collide_with_areas);
}

Array PhysicsDirectSpaceState2D::_intersect_point_on_canvas(const Vector2 &p_point, GameEntity p_canvas_intance_id, int p_max_results, const Array &p_exclude, uint32_t p_layers, bool p_collide_with_bodies, bool p_collide_with_areas) {

    return _intersect_point_impl(p_point, p_max_results, p_exclude, p_layers, p_collide_with_bodies, p_collide_with_areas, true, p_canvas_intance_id);
}

Array PhysicsDirectSpaceState2D::_collide_shape(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query, int p_max_results) {

    ERR_FAIL_COND_V(not p_shape_query, Array());

    Vector<Vector2> ret;
    ret.resize(p_max_results * 2);
    int rc = 0;
    bool res = collide_shape(p_shape_query->shape, p_shape_query->transform, p_shape_query->motion, p_shape_query->margin, ret.data(), p_max_results, rc, p_shape_query->exclude, p_shape_query->collision_mask, p_shape_query->collide_with_bodies, p_shape_query->collide_with_areas);
    if (!res)
        return Array();

    Vector<Variant> r;
    r.reserve(rc * 2);
    for (int i = 0; i < rc * 2; i++)
        ret.emplace_back(ret[i]);
    return Array(eastl::move(r));
}
Dictionary PhysicsDirectSpaceState2D::_get_rest_info(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query) {

    ERR_FAIL_COND_V(not p_shape_query, Dictionary());

    ShapeRestInfo sri;

    bool res = rest_info(p_shape_query->shape, p_shape_query->transform, p_shape_query->motion, p_shape_query->margin, &sri, p_shape_query->exclude, p_shape_query->collision_mask, p_shape_query->collide_with_bodies, p_shape_query->collide_with_areas);
    Dictionary r;
    if (!res)
        return r;

    r["point"] = sri.point;
    r["normal"] = sri.normal;
    r["rid"] = sri.rid;
    r["collider_id"] = Variant::from(sri.collider_id);
    r["shape"] = sri.shape;
    r["linear_velocity"] = sri.linear_velocity;
    r["metadata"] = sri.metadata;

    return r;
}

PhysicsDirectSpaceState2D::PhysicsDirectSpaceState2D() {
}

void PhysicsDirectSpaceState2D::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("intersect_point", {"point", "max_results", "exclude", "collision_layer", "collide_with_bodies", "collide_with_areas"}), &PhysicsDirectSpaceState2D::_intersect_point, {DEFVAL(32), DEFVAL(Array()), DEFVAL(0x7FFFFFFF), DEFVAL(true), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("intersect_point_on_canvas", {"point", "canvas_instance_id", "max_results", "exclude", "collision_layer", "collide_with_bodies", "collide_with_areas"}), &PhysicsDirectSpaceState2D::_intersect_point_on_canvas, {DEFVAL(32), DEFVAL(Array()), DEFVAL(0x7FFFFFFF), DEFVAL(true), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("intersect_ray", {"from", "to", "exclude", "collision_layer", "collide_with_bodies", "collide_with_areas"}), &PhysicsDirectSpaceState2D::_intersect_ray, {DEFVAL(Array()), DEFVAL(0x7FFFFFFF), DEFVAL(true), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("intersect_shape", {"shape", "max_results"}), &PhysicsDirectSpaceState2D::_intersect_shape, {DEFVAL(32)});
    MethodBinder::bind_method(D_METHOD("cast_motion", {"shape"}), &PhysicsDirectSpaceState2D::_cast_motion);
    MethodBinder::bind_method(D_METHOD("collide_shape", {"shape", "max_results"}), &PhysicsDirectSpaceState2D::_collide_shape, {DEFVAL(32)});
    MethodBinder::bind_method(D_METHOD("get_rest_info", {"shape"}), &PhysicsDirectSpaceState2D::_get_rest_info);
}

///////////////////////////////

Vector2 Physics2DTestMotionResult::get_motion() const {

    return result.motion;
}
Vector2 Physics2DTestMotionResult::get_motion_remainder() const {

    return result.remainder;
}

Vector2 Physics2DTestMotionResult::get_collision_point() const {

    return result.collision_point;
}
Vector2 Physics2DTestMotionResult::get_collision_normal() const {

    return result.collision_normal;
}
Vector2 Physics2DTestMotionResult::get_collider_velocity() const {

    return result.collider_velocity;
}
GameEntity Physics2DTestMotionResult::get_collider_id() const {

    return result.collider_id;
}
RID Physics2DTestMotionResult::get_collider_rid() const {

    return result.collider;
}

Object *Physics2DTestMotionResult::get_collider() const {
    return object_for_entity(result.collider_id);
}

int Physics2DTestMotionResult::get_collider_shape() const {

    return result.collider_shape;
}

real_t Physics2DTestMotionResult::get_collision_depth() const {
    return result.collision_depth;
}

real_t Physics2DTestMotionResult::get_collision_safe_fraction() const {
    return result.collision_safe_fraction;
}

real_t Physics2DTestMotionResult::get_collision_unsafe_fraction() const {
    return result.collision_unsafe_fraction;
}

void Physics2DTestMotionResult::_bind_methods() {

    SE_BIND_METHOD(Physics2DTestMotionResult,get_motion);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_motion_remainder);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_collision_point);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_collision_normal);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_collider_velocity);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_collider_id);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_collider_rid);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_collider);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_collider_shape);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_collision_depth);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_collision_safe_fraction);
    SE_BIND_METHOD(Physics2DTestMotionResult,get_collision_unsafe_fraction);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "motion"), "", "get_motion");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "motion_remainder"), "", "get_motion_remainder");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "collision_point"), "", "get_collision_point");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "collision_normal"), "", "get_collision_normal");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "collider_velocity"), "", "get_collider_velocity");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collider_id", PropertyHint::ObjectID), "", "get_collider_id");
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "collider_rid"), "", "get_collider_rid");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "collider"), "", "get_collider");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collider_shape"), "", "get_collider_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "collision_depth"), "", "get_collision_depth");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "collision_safe_fraction"), "", "get_collision_safe_fraction");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "collision_unsafe_fraction"), "", "get_collision_unsafe_fraction");

}

///////////////////////////////////////

bool PhysicsServer2D::_body_test_motion(RID p_body, const Transform2D &p_from, const Vector2 &p_motion, bool p_infinite_inertia, float p_margin, const Ref<Physics2DTestMotionResult> &p_result, bool p_exclude_raycast_shapes, const Vector<RID> &p_exclude) {

    MotionResult *r = nullptr;
    if (p_result)
        r = p_result->get_result_ptr();
    Set<RID> exclude(p_exclude.begin(),p_exclude.end());
    return body_test_motion(p_body, p_from, p_motion, p_infinite_inertia, p_margin, r, p_exclude_raycast_shapes, exclude);
}

void PhysicsServer2D::_bind_methods() {

    SE_BIND_METHOD(PhysicsServer2D,line_shape_create);
    SE_BIND_METHOD(PhysicsServer2D,ray_shape_create);
    SE_BIND_METHOD(PhysicsServer2D,segment_shape_create);
    SE_BIND_METHOD(PhysicsServer2D,circle_shape_create);
    SE_BIND_METHOD(PhysicsServer2D,rectangle_shape_create);
    SE_BIND_METHOD(PhysicsServer2D,capsule_shape_create);
    SE_BIND_METHOD(PhysicsServer2D,convex_polygon_shape_create);
    SE_BIND_METHOD(PhysicsServer2D,concave_polygon_shape_create);

    SE_BIND_METHOD(PhysicsServer2D,shape_set_data);

    SE_BIND_METHOD(PhysicsServer2D,shape_get_type);
    SE_BIND_METHOD(PhysicsServer2D,shape_get_data);

    SE_BIND_METHOD(PhysicsServer2D,space_create);
    SE_BIND_METHOD(PhysicsServer2D,space_set_active);
    SE_BIND_METHOD(PhysicsServer2D,space_is_active);
    SE_BIND_METHOD(PhysicsServer2D,space_set_param);
    SE_BIND_METHOD(PhysicsServer2D,space_get_param);
    SE_BIND_METHOD(PhysicsServer2D,space_get_direct_state);

    SE_BIND_METHOD(PhysicsServer2D,area_create);
    SE_BIND_METHOD(PhysicsServer2D,area_set_space);
    SE_BIND_METHOD(PhysicsServer2D,area_get_space);

    SE_BIND_METHOD(PhysicsServer2D,area_set_space_override_mode);
    SE_BIND_METHOD(PhysicsServer2D,area_get_space_override_mode);

    MethodBinder::bind_method(D_METHOD("area_add_shape", {"area", "shape", "transform", "disabled"}), &PhysicsServer2D::area_add_shape, {DEFVAL(Transform2D()), DEFVAL(false)});
    SE_BIND_METHOD(PhysicsServer2D,area_set_shape);
    SE_BIND_METHOD(PhysicsServer2D,area_set_shape_transform);
    SE_BIND_METHOD(PhysicsServer2D,area_set_shape_disabled);

    SE_BIND_METHOD(PhysicsServer2D,area_get_shape_count);
    SE_BIND_METHOD(PhysicsServer2D,area_get_shape);
    SE_BIND_METHOD(PhysicsServer2D,area_get_shape_transform);

    SE_BIND_METHOD(PhysicsServer2D,area_remove_shape);
    SE_BIND_METHOD(PhysicsServer2D,area_clear_shapes);

    SE_BIND_METHOD(PhysicsServer2D,area_set_collision_layer);
    SE_BIND_METHOD(PhysicsServer2D,area_set_collision_mask);

    SE_BIND_METHOD(PhysicsServer2D,area_set_param);
    SE_BIND_METHOD(PhysicsServer2D,area_set_transform);

    SE_BIND_METHOD(PhysicsServer2D,area_get_param);
    SE_BIND_METHOD(PhysicsServer2D,area_get_transform);

    SE_BIND_METHOD(PhysicsServer2D,area_attach_object_instance_id);
    SE_BIND_METHOD(PhysicsServer2D,area_get_object_instance_id);

    SE_BIND_METHOD(PhysicsServer2D,area_attach_canvas_instance_id);
    SE_BIND_METHOD(PhysicsServer2D,area_get_canvas_instance_id);

    SE_BIND_METHOD(PhysicsServer2D,area_set_monitor_callback);
    SE_BIND_METHOD(PhysicsServer2D,area_set_area_monitor_callback);
    SE_BIND_METHOD(PhysicsServer2D,area_set_monitorable);

    SE_BIND_METHOD(PhysicsServer2D,body_create);

    SE_BIND_METHOD(PhysicsServer2D,body_set_space);
    SE_BIND_METHOD(PhysicsServer2D,body_get_space);

    SE_BIND_METHOD(PhysicsServer2D,body_set_mode);
    SE_BIND_METHOD(PhysicsServer2D,body_get_mode);

    MethodBinder::bind_method(D_METHOD("body_add_shape", {"body", "shape", "transform", "disabled"}), &PhysicsServer2D::body_add_shape, {DEFVAL(Transform2D()), DEFVAL(false)});
    SE_BIND_METHOD(PhysicsServer2D,body_set_shape);
    SE_BIND_METHOD(PhysicsServer2D,body_set_shape_transform);
    SE_BIND_METHOD(PhysicsServer2D,body_set_shape_metadata);

    SE_BIND_METHOD(PhysicsServer2D,body_get_shape_count);
    SE_BIND_METHOD(PhysicsServer2D,body_get_shape);
    SE_BIND_METHOD(PhysicsServer2D,body_get_shape_transform);
    SE_BIND_METHOD(PhysicsServer2D,body_get_shape_metadata);

    SE_BIND_METHOD(PhysicsServer2D,body_remove_shape);
    SE_BIND_METHOD(PhysicsServer2D,body_clear_shapes);

    SE_BIND_METHOD(PhysicsServer2D,body_set_shape_disabled);
    SE_BIND_METHOD(PhysicsServer2D,body_set_shape_as_one_way_collision);

    SE_BIND_METHOD(PhysicsServer2D,body_attach_object_instance_id);
    SE_BIND_METHOD(PhysicsServer2D,body_get_object_instance_id);

    SE_BIND_METHOD(PhysicsServer2D,body_attach_canvas_instance_id);
    SE_BIND_METHOD(PhysicsServer2D,body_get_canvas_instance_id);

    SE_BIND_METHOD(PhysicsServer2D,body_set_continuous_collision_detection_mode);
    SE_BIND_METHOD(PhysicsServer2D,body_get_continuous_collision_detection_mode);

    SE_BIND_METHOD(PhysicsServer2D,body_set_collision_layer);
    SE_BIND_METHOD(PhysicsServer2D,body_get_collision_layer);

    SE_BIND_METHOD(PhysicsServer2D,body_set_collision_mask);
    SE_BIND_METHOD(PhysicsServer2D,body_get_collision_mask);

    SE_BIND_METHOD(PhysicsServer2D,body_set_param);
    SE_BIND_METHOD(PhysicsServer2D,body_get_param);

    SE_BIND_METHOD(PhysicsServer2D,body_set_state);
    SE_BIND_METHOD(PhysicsServer2D,body_get_state);

    SE_BIND_METHOD(PhysicsServer2D,body_apply_central_impulse);
    SE_BIND_METHOD(PhysicsServer2D,body_apply_torque_impulse);
    SE_BIND_METHOD(PhysicsServer2D,body_apply_impulse);
    SE_BIND_METHOD(PhysicsServer2D,body_add_central_force);
    SE_BIND_METHOD(PhysicsServer2D,body_add_force);
    SE_BIND_METHOD(PhysicsServer2D,body_add_torque);
    SE_BIND_METHOD(PhysicsServer2D,body_set_axis_velocity);

    SE_BIND_METHOD(PhysicsServer2D,body_add_collision_exception);
    SE_BIND_METHOD(PhysicsServer2D,body_remove_collision_exception);

    SE_BIND_METHOD(PhysicsServer2D,body_set_max_contacts_reported);
    SE_BIND_METHOD(PhysicsServer2D,body_get_max_contacts_reported);

    SE_BIND_METHOD(PhysicsServer2D,body_set_omit_force_integration);
    SE_BIND_METHOD(PhysicsServer2D,body_is_omitting_force_integration);

    SE_BIND_METHOD(PhysicsServer2D,body_set_force_integration_callback);

    MethodBinder::bind_method(D_METHOD("body_test_motion", { "body", "from", "motion", "infinite_inertia", "margin",
                                                                   "result", "exclude_raycast_shapes", "exclude" }),
            &PhysicsServer2D::_body_test_motion, { DEFVAL(0.08f), DEFVAL(Variant()),DEFVAL(true), DEFVAL(Array()) });

    SE_BIND_METHOD(PhysicsServer2D,body_get_direct_state);

    /* JOINT API */

    SE_BIND_METHOD(PhysicsServer2D,joint_set_param);
    SE_BIND_METHOD(PhysicsServer2D,joint_get_param);

    MethodBinder::bind_method(D_METHOD("pin_joint_create", {"anchor", "body_a", "body_b"}), &PhysicsServer2D::pin_joint_create, {DEFVAL(RID())});
    MethodBinder::bind_method(D_METHOD("groove_joint_create", {"groove1_a", "groove2_a", "anchor_b", "body_a", "body_b"}), &PhysicsServer2D::groove_joint_create, {DEFVAL(RID()), DEFVAL(RID())});
    MethodBinder::bind_method(D_METHOD("damped_spring_joint_create", {"anchor_a", "anchor_b", "body_a", "body_b"}), &PhysicsServer2D::damped_spring_joint_create, {DEFVAL(RID())});

    SE_BIND_METHOD(PhysicsServer2D,damped_string_joint_set_param);
    SE_BIND_METHOD(PhysicsServer2D,damped_string_joint_get_param);

    SE_BIND_METHOD(PhysicsServer2D,joint_get_type);

    SE_BIND_METHOD(PhysicsServer2D,free_rid);

    SE_BIND_METHOD(PhysicsServer2D,set_active);
    SE_BIND_METHOD(PhysicsServer2D,set_collision_iterations);

    SE_BIND_METHOD(PhysicsServer2D,get_process_info);

    BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_RECYCLE_RADIUS);
    BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_MAX_SEPARATION);
    BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_MAX_ALLOWED_PENETRATION);
    BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_THRESHOLD);
    BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_ANGULAR_VELOCITY_SLEEP_THRESHOLD);
    BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_TIME_TO_SLEEP);
    BIND_ENUM_CONSTANT(SPACE_PARAM_CONSTRAINT_DEFAULT_BIAS);

    BIND_ENUM_CONSTANT(SHAPE_LINE);
    BIND_ENUM_CONSTANT(SHAPE_RAY);
    BIND_ENUM_CONSTANT(SHAPE_SEGMENT);
    BIND_ENUM_CONSTANT(SHAPE_CIRCLE);
    BIND_ENUM_CONSTANT(SHAPE_RECTANGLE);
    BIND_ENUM_CONSTANT(SHAPE_CAPSULE);
    BIND_ENUM_CONSTANT(SHAPE_CONVEX_POLYGON);
    BIND_ENUM_CONSTANT(SHAPE_CONCAVE_POLYGON);
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
    BIND_ENUM_CONSTANT(BODY_PARAM_INERTIA);
    BIND_ENUM_CONSTANT(BODY_PARAM_GRAVITY_SCALE);
    BIND_ENUM_CONSTANT(BODY_PARAM_LINEAR_DAMP);
    BIND_ENUM_CONSTANT(BODY_PARAM_ANGULAR_DAMP);
    BIND_ENUM_CONSTANT(BODY_PARAM_MAX);

    BIND_ENUM_CONSTANT(BODY_STATE_TRANSFORM);
    BIND_ENUM_CONSTANT(BODY_STATE_LINEAR_VELOCITY);
    BIND_ENUM_CONSTANT(BODY_STATE_ANGULAR_VELOCITY);
    BIND_ENUM_CONSTANT(BODY_STATE_SLEEPING);
    BIND_ENUM_CONSTANT(BODY_STATE_CAN_SLEEP);

    BIND_ENUM_CONSTANT(JOINT_PIN);
    BIND_ENUM_CONSTANT(JOINT_GROOVE);
    BIND_ENUM_CONSTANT(JOINT_DAMPED_SPRING);

    BIND_ENUM_CONSTANT(JOINT_PARAM_BIAS);
    BIND_ENUM_CONSTANT(JOINT_PARAM_MAX_BIAS);
    BIND_ENUM_CONSTANT(JOINT_PARAM_MAX_FORCE);

    BIND_ENUM_CONSTANT(DAMPED_STRING_REST_LENGTH);
    BIND_ENUM_CONSTANT(DAMPED_STRING_STIFFNESS);
    BIND_ENUM_CONSTANT(DAMPED_STRING_DAMPING);

    BIND_ENUM_CONSTANT(CCD_MODE_DISABLED);
    BIND_ENUM_CONSTANT(CCD_MODE_CAST_RAY);
    BIND_ENUM_CONSTANT(CCD_MODE_CAST_SHAPE);

    BIND_ENUM_CONSTANT(AREA_BODY_ADDED);
    BIND_ENUM_CONSTANT(AREA_BODY_REMOVED);

    BIND_ENUM_CONSTANT(INFO_ACTIVE_OBJECTS);
    BIND_ENUM_CONSTANT(INFO_COLLISION_PAIRS);
    BIND_ENUM_CONSTANT(INFO_ISLAND_COUNT);
}

PhysicsServer2D::PhysicsServer2D() {
}

PhysicsServer2D::~PhysicsServer2D() {
}

int Physics2DServerManager::default_server_id = -1;
int Physics2DServerManager::default_server_priority = -1;
const StaticCString Physics2DServerManager::setting_property_name("physics/2d/physics_engine");

void Physics2DServerManager::on_servers_changed() {

    String physics_servers("DEFAULT");
    for (int i = get_servers_count() - 1; 0 <= i; --i) {
        physics_servers+= ',' + String(get_server_name(i));
    }
    ProjectSettings::get_singleton()->set_custom_property_info(setting_property_name,
            PropertyInfo(VariantType::STRING, StringName(setting_property_name), PropertyHint::Enum, physics_servers));
}

void Physics2DServerManager::register_server(const StringName &p_name, CreatePhysics2DServerCallback p_creat_callback) {

    ERR_FAIL_COND(!p_creat_callback);
    ERR_FAIL_COND(find_server_id(p_name) != -1);
    physics_2d_servers.push_back(ClassInfo(p_name, p_creat_callback));
    on_servers_changed();
}

void Physics2DServerManager::set_default_server(const StringName &p_name, int p_priority) {

    const int id = find_server_id(p_name);
    ERR_FAIL_COND(id == -1); // Not found
    if (default_server_priority < p_priority) {
        default_server_id = id;
        default_server_priority = p_priority;
    }
}

int Physics2DServerManager::find_server_id(const StringName &p_name) {

    for (int i = physics_2d_servers.size() - 1; 0 <= i; --i) {
        if (p_name == physics_2d_servers[i].name) {
            return i;
        }
    }
    return -1;
}

int Physics2DServerManager::get_servers_count() {
    return physics_2d_servers.size();
}

StringName Physics2DServerManager::get_server_name(int p_id) {
    ERR_FAIL_INDEX_V(p_id, get_servers_count(), StringName());
    return physics_2d_servers[p_id].name;
}

PhysicsServer2D *Physics2DServerManager::new_default_server() {
    ERR_FAIL_COND_V(default_server_id == -1, nullptr);
    return physics_2d_servers[default_server_id].create_callback();
}

PhysicsServer2D *Physics2DServerManager::new_server(const StringName &p_name) {
    int id = find_server_id(p_name);
    if (id == -1) {
        return nullptr;
    } else {
        return physics_2d_servers[id].create_callback();
    }
}

void Physics2DServerManager::cleanup()
{
    physics_2d_servers.clear();
    default_server_id=-1;
    default_server_priority=-1;

}
PhysicsServer2D *initialize_2d_physics() {
    PhysicsServer2D *physics_server_2d = Physics2DServerManager::new_server(ProjectSettings::get_singleton()->getT<StringName>(Physics2DServerManager::setting_property_name));
    if (!physics_server_2d) {
        // Physics server not found, Use the default physics
        physics_server_2d = Physics2DServerManager::new_default_server();
    }
    ERR_FAIL_COND_V(!physics_server_2d,nullptr);
    physics_server_2d->init();
    return physics_server_2d;
}
