/*************************************************************************/
/*  body_2d_sw.h                                                         */
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

#pragma once

#include "area_2d_sw.h"
#include "collision_object_2d_sw.h"

#include "core/rid.h"
#include "core/vset.h"

class Constraint2DSW;
class Physics2DDirectBodyStateSW;

class Body2DSW : public CollisionObject2DSW {

    struct AreaCMP {

        Area2DSW *area;
        int refCount;
        bool operator==(const AreaCMP &p_cmp) const { return area->get_self() == p_cmp.area->get_self(); }
        bool operator<(const AreaCMP &p_cmp) const { return area->get_priority() < p_cmp.area->get_priority(); }
        AreaCMP() {}
        AreaCMP(Area2DSW *p_area) {
            area = p_area;
            refCount = 1;
        }
    };
    struct Contact {

        Vector2 local_pos;
        Vector2 local_normal;
        float depth;
        int local_shape;
        Vector2 collider_pos;
        RID collider;
        int collider_shape;
        GameEntity collider_instance_id;
        Vector2 collider_velocity_at_pos;
    };
    using ForceIntegrationCallback = Callable;


    PhysicsServer2D::BodyMode mode = PhysicsServer2D::BODY_MODE_RIGID;

    Vector2 biased_linear_velocity;
    float biased_angular_velocity = 0;

    Vector2 linear_velocity;
    float angular_velocity = 0;

    float linear_damp = -1;
    float angular_damp = -1;
    float gravity_scale = 1.0f;

    float mass=1;
    float inertia=0;
    float bounce=0;
    float friction=1;

    float _inv_mass=1;
    float _inv_inertia=0;

    Vector2 gravity;
    float area_linear_damp=0;
    float area_angular_damp=0;

    float still_time = 0;

    Vector2 applied_force;
    float applied_torque = 0;

    //IntrusiveListNode<Body2DSW> active_list;
    IntrusiveListNode<Body2DSW> inertia_update_list;
    IntrusiveListNode<Body2DSW> direct_state_query_list;

    VSet<RID> exceptions;
    PhysicsServer2D::CCDMode continuous_cd_mode = PhysicsServer2D::CCD_MODE_DISABLED;
    uint8_t user_inertia : 1;
    uint8_t omit_force_integration : 1;
    uint8_t active : 1;
    uint8_t in_active_list : 1;
    uint8_t can_sleep : 1;
    uint8_t first_time_kinematic : 1;
    uint8_t first_integration : 1;

    Transform2D new_transform;
    HashMap<Constraint2DSW *, int> constraint_map;


    eastl::vector_set<AreaCMP,eastl::less<AreaCMP>,wrap_allocator> areas;

    Vector<Contact> contacts; //no contacts by default
    int contact_count = 0;

    ForceIntegrationCallback fi_callback;

    uint64_t island_step=0;
    Body2DSW *island_next=nullptr;
    Body2DSW *island_list_next=nullptr;


    void _update_inertia();
    void _shapes_changed() override;


    _FORCE_INLINE_ void _compute_area_gravity_and_dampenings(const Area2DSW *p_area);

    Physics2DDirectBodyStateSW *direct_access = nullptr;
    friend class Physics2DDirectBodyStateSW; // i give up, too many functions to expose

public:
    void set_force_integration_callback(Callable &&cb);

    _FORCE_INLINE_ void add_area(Area2DSW *p_area) {
        auto index = areas.find(AreaCMP(p_area));
        if (index != areas.end()) {
            index->refCount += 1;
        } else {
            areas.emplace(p_area);
        }
    }

    _FORCE_INLINE_ void remove_area(Area2DSW *p_area) {
        auto index = areas.find(AreaCMP(p_area));
        if (index != areas.end()) {
            index->refCount -= 1;
            if (index->refCount < 1)
                areas.erase(index);
        }
    }

    _FORCE_INLINE_ void set_max_contacts_reported(int p_size) {
        contacts.resize(p_size);
        contact_count = 0;
        if (mode == PhysicsServer2D::BODY_MODE_KINEMATIC && p_size)
            set_active(true);
    }

    int get_max_contacts_reported() const { return contacts.size(); }

    bool can_report_contacts() const { return !contacts.empty(); }
    _FORCE_INLINE_ void add_contact(const Vector2 &p_local_pos, const Vector2 &p_local_normal, float p_depth,
            int p_local_shape, const Vector2 &p_collider_pos, int p_collider_shape, GameEntity p_collider_instance_id,
            const RID &p_collider, const Vector2 &p_collider_velocity_at_pos);

    _FORCE_INLINE_ void add_exception(const RID &p_exception) { exceptions.insert(p_exception); }
    _FORCE_INLINE_ void remove_exception(const RID &p_exception) { exceptions.erase(p_exception); }
    _FORCE_INLINE_ bool has_exception(const RID &p_exception) const { return exceptions.contains(p_exception); }
    _FORCE_INLINE_ const VSet<RID> &get_exceptions() const { return exceptions; }

    _FORCE_INLINE_ uint64_t get_island_step() const { return island_step; }
    _FORCE_INLINE_ void set_island_step(uint64_t p_step) { island_step = p_step; }

    _FORCE_INLINE_ Body2DSW *get_island_next() const { return island_next; }
    _FORCE_INLINE_ void set_island_next(Body2DSW *p_next) { island_next = p_next; }

    _FORCE_INLINE_ Body2DSW *get_island_list_next() const { return island_list_next; }
    _FORCE_INLINE_ void set_island_list_next(Body2DSW *p_next) { island_list_next = p_next; }

    _FORCE_INLINE_ void add_constraint(Constraint2DSW *p_constraint, int p_pos) {
        constraint_map[p_constraint] = p_pos;
    }
    _FORCE_INLINE_ void remove_constraint(Constraint2DSW *p_constraint) { constraint_map.erase(p_constraint); }
    const HashMap<Constraint2DSW *, int> &get_constraint_map() const { return constraint_map; }
    _FORCE_INLINE_ void clear_constraint_map() { constraint_map.clear(); }

    _FORCE_INLINE_ void set_omit_force_integration(bool p_omit_force_integration) {
        omit_force_integration = p_omit_force_integration;
    }
    _FORCE_INLINE_ bool get_omit_force_integration() const { return omit_force_integration; }

    _FORCE_INLINE_ void set_linear_velocity(const Vector2 &p_velocity) { linear_velocity = p_velocity; }
    _FORCE_INLINE_ Vector2 get_linear_velocity() const { return linear_velocity; }

    _FORCE_INLINE_ void set_angular_velocity(float p_velocity) { angular_velocity = p_velocity; }
    _FORCE_INLINE_ float get_angular_velocity() const { return angular_velocity; }

    _FORCE_INLINE_ void set_biased_linear_velocity(const Vector2 &p_velocity) { biased_linear_velocity = p_velocity; }
    _FORCE_INLINE_ Vector2 get_biased_linear_velocity() const { return biased_linear_velocity; }

    _FORCE_INLINE_ void set_biased_angular_velocity(float p_velocity) { biased_angular_velocity = p_velocity; }
    _FORCE_INLINE_ float get_biased_angular_velocity() const { return biased_angular_velocity; }

    _FORCE_INLINE_ void apply_central_impulse(const Vector2 &p_impulse) { linear_velocity += p_impulse * _inv_mass; }

    _FORCE_INLINE_ void apply_impulse(const Vector2 &p_offset, const Vector2 &p_impulse) {

        linear_velocity += p_impulse * _inv_mass;
        angular_velocity += _inv_inertia * p_offset.cross(p_impulse);
    }

    _FORCE_INLINE_ void apply_torque_impulse(float p_torque) { angular_velocity += _inv_inertia * p_torque; }

    _FORCE_INLINE_ void apply_bias_impulse(const Vector2 &p_pos, const Vector2 &p_j) {

        biased_linear_velocity += p_j * _inv_mass;
        biased_angular_velocity += _inv_inertia * p_pos.cross(p_j);
    }

    void set_active(bool p_active);
    _FORCE_INLINE_ bool is_active() const { return active; }

    _FORCE_INLINE_ void wakeup() {
        if ((!get_space()) || mode == PhysicsServer2D::BODY_MODE_STATIC || mode == PhysicsServer2D::BODY_MODE_KINEMATIC)
            return;
        set_active(true);
    }

    void set_param(PhysicsServer2D::BodyParameter p_param, float);
    float get_param(PhysicsServer2D::BodyParameter p_param) const;

    void set_mode(PhysicsServer2D::BodyMode p_mode);
    PhysicsServer2D::BodyMode get_mode() const;

    void set_state(PhysicsServer2D::BodyState p_state, const Variant &p_variant);
    Variant get_state(PhysicsServer2D::BodyState p_state) const;

    void set_applied_force(const Vector2 &p_force) { applied_force = p_force; }
    Vector2 get_applied_force() const { return applied_force; }

    void set_applied_torque(float p_torque) { applied_torque = p_torque; }
    float get_applied_torque() const { return applied_torque; }

    _FORCE_INLINE_ void add_central_force(const Vector2 &p_force) { applied_force += p_force; }

    _FORCE_INLINE_ void add_force(const Vector2 &p_offset, const Vector2 &p_force) {

        applied_force += p_force;
        applied_torque += p_offset.cross(p_force);
    }

    _FORCE_INLINE_ void add_torque(float p_torque) { applied_torque += p_torque; }

    _FORCE_INLINE_ void set_continuous_collision_detection_mode(PhysicsServer2D::CCDMode p_mode) {
        continuous_cd_mode = p_mode;
    }

    _FORCE_INLINE_ PhysicsServer2D::CCDMode get_continuous_collision_detection_mode() const {
        return continuous_cd_mode;
    }

    void set_space(Space2DSW *p_space) override;

    void update_inertias();

    float get_inv_mass() const { return _inv_mass; }
    float get_inv_inertia() const { return _inv_inertia; }
    float get_friction() const { return friction; }
    Vector2 get_gravity() const { return gravity; }
    float get_bounce() const { return bounce; }
    float get_linear_damp() const { return linear_damp; }
    float get_angular_damp() const { return angular_damp; }

    void integrate_forces(float p_step);
    void integrate_velocities(float p_step);
    Vector2 get_velocity_in_local_point(const Vector2 &rel_pos) const {
        return linear_velocity + Vector2(-angular_velocity * rel_pos.y, angular_velocity * rel_pos.x);
    }

    Vector2 get_motion() const {
        if (mode > PhysicsServer2D::BODY_MODE_KINEMATIC) {
            return new_transform.get_origin() - get_transform().get_origin();
        } else if (mode == PhysicsServer2D::BODY_MODE_KINEMATIC) {
            return get_transform().get_origin() - new_transform.get_origin(); //kinematic simulates forward
        }
        return Vector2();
    }

    void call_queries();
    void wakeup_neighbours();

    bool sleep_test(float p_step);
    Physics2DDirectBodyStateSW *get_direct_state() const { return direct_access; }

    Body2DSW();
    ~Body2DSW() override;
};

//add contact inline

void Body2DSW::add_contact(const Vector2 &p_local_pos, const Vector2 &p_local_normal, float p_depth, int p_local_shape,
        const Vector2 &p_collider_pos, int p_collider_shape, GameEntity p_collider_instance_id, const RID &p_collider,
        const Vector2 &p_collider_velocity_at_pos) {

    int c_max = contacts.size();

    if (c_max == 0)
        return;

    Contact *c = contacts.data();

    int idx = -1;

    if (contact_count < c_max) {
        idx = contact_count++;
    } else {

        float least_depth = 1e20f;
        int least_deep = -1;
        for (int i = 0; i < c_max; i++) {

            if (i == 0 || c[i].depth < least_depth) {
                least_deep = i;
                least_depth = c[i].depth;
            }
        }

        if (least_deep >= 0 && least_depth < p_depth) {

            idx = least_deep;
        }
        if (idx == -1)
            return; //none least deepe than this
    }

    c[idx].local_pos = p_local_pos;
    c[idx].local_normal = p_local_normal;
    c[idx].depth = p_depth;
    c[idx].local_shape = p_local_shape;
    c[idx].collider_pos = p_collider_pos;
    c[idx].collider_shape = p_collider_shape;
    c[idx].collider_instance_id = p_collider_instance_id;
    c[idx].collider = p_collider;
    c[idx].collider_velocity_at_pos = p_collider_velocity_at_pos;
}

class Physics2DDirectBodyStateSW : public PhysicsDirectBodyState2D {

    GDCLASS(Physics2DDirectBodyStateSW,PhysicsDirectBodyState2D)

public:
    Body2DSW *body = nullptr;

    Vector2 get_total_gravity() const override {
        return body->gravity;
    } // get gravity vector working on this body space/area
    float get_total_angular_damp() const override {
        return body->area_angular_damp;
    } // get density of this body space/area
    float get_total_linear_damp() const override {
        return body->area_linear_damp;
    } // get density of this body space/area

    float get_inverse_mass() const override { return body->get_inv_mass(); } // get the mass
    float get_inverse_inertia() const override { return body->get_inv_inertia(); } // get density of this body space

    void set_linear_velocity(const Vector2 &p_velocity) override {
        body->wakeup();
        body->set_linear_velocity(p_velocity);
    }
    Vector2 get_linear_velocity() const override { return body->get_linear_velocity(); }

    void set_angular_velocity(float p_velocity) override {
        body->wakeup();
        body->set_angular_velocity(p_velocity);
    }
    float get_angular_velocity() const override { return body->get_angular_velocity(); }

    void set_transform(const Transform2D &p_transform) override {
        body->set_state(PhysicsServer2D::BODY_STATE_TRANSFORM, p_transform);
    }

    Vector2 get_velocity_at_local_position(const Vector2 &p_position) const override {
        return body->get_velocity_in_local_point(p_position);
    }
    Transform2D get_transform() const override { return body->get_transform(); }

    void add_central_force(const Vector2 &p_force) override {
        body->wakeup();
        body->add_central_force(p_force);
    }
    void add_force(const Vector2 &p_offset, const Vector2 &p_force) override {
        body->wakeup();
        body->add_force(p_offset, p_force);
    }
    void add_torque(float p_torque) override {
        body->wakeup();
        body->add_torque(p_torque);
    }
    void apply_central_impulse(const Vector2 &p_impulse) override {
        body->wakeup();
        body->apply_central_impulse(p_impulse);
    }
    void apply_impulse(const Vector2 &p_offset, const Vector2 &p_force) override {
        body->wakeup();
        body->apply_impulse(p_offset, p_force);
    }
    void apply_torque_impulse(float p_torque) override {
        body->wakeup();
        body->apply_torque_impulse(p_torque);
    }

    void set_sleep_state(bool p_enable) override { body->set_active(!p_enable); }
    bool is_sleeping() const override { return !body->is_active(); }

    int get_contact_count() const override { return body->contact_count; }

    Vector2 get_contact_local_position(int p_contact_idx) const override {
        ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, Vector2());
        return body->contacts[p_contact_idx].local_pos;
    }
    Vector2 get_contact_local_normal(int p_contact_idx) const override {
        ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, Vector2());
        return body->contacts[p_contact_idx].local_normal;
    }
    int get_contact_local_shape(int p_contact_idx) const override {
        ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, -1);
        return body->contacts[p_contact_idx].local_shape;
    }

    RID get_contact_collider(int p_contact_idx) const override {
        ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, RID());
        return body->contacts[p_contact_idx].collider;
    }
    Vector2 get_contact_collider_position(int p_contact_idx) const override {
        ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, Vector2());
        return body->contacts[p_contact_idx].collider_pos;
    }
    GameEntity get_contact_collider_id(int p_contact_idx) const override {
        ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, entt::null);
        return body->contacts[p_contact_idx].collider_instance_id;
    }
    int get_contact_collider_shape(int p_contact_idx) const override {
        ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, 0);
        return body->contacts[p_contact_idx].collider_shape;
    }
    Variant get_contact_collider_shape_metadata(int p_contact_idx) const override;

    Vector2 get_contact_collider_velocity_at_position(int p_contact_idx) const override {
        ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, Vector2());
        return body->contacts[p_contact_idx].collider_velocity_at_pos;
    }

    PhysicsDirectSpaceState2D *get_space_state() override;

    float get_step() const override;

    Physics2DDirectBodyStateSW() {}
};
