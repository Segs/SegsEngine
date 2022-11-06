/*************************************************************************/
/*  physics_body_3d.h                                                       */
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

#include "core/vset.h"
#include "scene/3d/collision_object_3d.h"
#include "scene/resources/physics_material.h"
#include "servers/physics_server_3d.h"
#include "skeleton_3d.h"

class GODOT_EXPORT PhysicsBody3D : public CollisionObject3D {

    GDCLASS(PhysicsBody3D,CollisionObject3D)

    uint32_t collision_layer;
    uint32_t collision_mask;

    void _set_layers(uint32_t p_mask);
    uint32_t _get_layers() const;

protected:
    static void _bind_methods();
    void _notification(int p_what);
    PhysicsBody3D(PhysicsServer3D::BodyMode p_mode);

public:
    virtual Vector3 get_linear_velocity() const;
    virtual Vector3 get_angular_velocity() const;
    virtual float get_inverse_mass() const;


    Array get_collision_exceptions();
    void add_collision_exception_with(Node *p_node); //must be physicsbody
    void remove_collision_exception_with(Node *p_node);

};

class GODOT_EXPORT StaticBody3D : public PhysicsBody3D {

    GDCLASS(StaticBody3D,PhysicsBody3D)

    Vector3 constant_linear_velocity;
    Vector3 constant_angular_velocity;

    Ref<PhysicsMaterial> physics_material_override;

protected:
    static void _bind_methods();

public:

    void set_physics_material_override(const Ref<PhysicsMaterial> &p_physics_material_override);
    Ref<PhysicsMaterial> get_physics_material_override() const;

    void set_constant_linear_velocity(const Vector3 &p_vel);
    void set_constant_angular_velocity(const Vector3 &p_vel);

    Vector3 get_constant_linear_velocity() const;
    Vector3 get_constant_angular_velocity() const;

    StaticBody3D();
    ~StaticBody3D() override;

private:
    void _reload_physics_characteristics();
};

class GODOT_EXPORT RigidBody : public PhysicsBody3D {

    GDCLASS(RigidBody,PhysicsBody3D)

public:
    enum Mode {
        MODE_RIGID,
        MODE_STATIC,
        MODE_CHARACTER,
        MODE_KINEMATIC,
    };

protected:
    bool can_sleep;
    PhysicsDirectBodyState3D *state;
    Mode mode;

    real_t mass;
    Ref<PhysicsMaterial> physics_material_override;

    Vector3 linear_velocity;
    Vector3 angular_velocity;
    Basis inverse_inertia_tensor;
    real_t gravity_scale;
    real_t linear_damp;
    real_t angular_damp;

    bool sleeping;
    bool ccd;

    int max_contacts_reported;

    bool custom_integrator;

    struct ShapePair {

        int body_shape;
        int local_shape;
        bool tagged;
        bool operator<(const ShapePair &p_sp) const {
            if (body_shape == p_sp.body_shape)
                return local_shape < p_sp.local_shape;
            else
                return body_shape < p_sp.body_shape;
        }

        ShapePair() = default;
        ShapePair(int p_bs, int p_ls) {
            body_shape = p_bs;
            local_shape = p_ls;
            tagged = false;
        }
    };
    struct RigidBody_RemoveAction {

        RID rid;
        ShapePair pair;
        GameEntity body_id;
    };
    struct BodyState {

        RID rid;
        bool in_tree;
        VSet<ShapePair> shapes;
    };

    struct ContactMonitor {

        bool locked;
        HashMap<GameEntity, BodyState> body_map;
    };

    ContactMonitor *contact_monitor;
    void _body_enter_tree(GameEntity p_id);
    void _body_exit_tree(GameEntity p_id);

    void _body_inout(int p_status, const RID &p_body, GameEntity p_instance, int p_body_shape, int p_local_shape);
    virtual void _direct_state_changed(Object *p_state);

    void _notification(int p_what);
    static void _bind_methods();

public:
    void set_mode(Mode p_mode);
    Mode get_mode() const;

    void set_mass(real_t p_mass);
    real_t get_mass() const;

    float get_inverse_mass() const override { return 1.0 / mass; }

    void set_weight(real_t p_weight);
    real_t get_weight() const;

    void set_physics_material_override(const Ref<PhysicsMaterial> &p_physics_material_override);
    Ref<PhysicsMaterial> get_physics_material_override() const;

    void set_linear_velocity(const Vector3 &p_velocity);
    Vector3 get_linear_velocity() const override;

    void set_axis_velocity(const Vector3 &p_axis);

    void set_angular_velocity(const Vector3 &p_velocity);
    Vector3 get_angular_velocity() const override;

    Basis get_inverse_inertia_tensor();

    void set_gravity_scale(real_t p_gravity_scale);
    real_t get_gravity_scale() const;

    void set_linear_damp(real_t p_linear_damp);
    real_t get_linear_damp() const;

    void set_angular_damp(real_t p_angular_damp);
    real_t get_angular_damp() const;

    void set_use_custom_integrator(bool p_enable);
    bool is_using_custom_integrator();

    void set_sleeping(bool p_sleeping);
    bool is_sleeping() const;

    void set_can_sleep(bool p_active);
    bool is_able_to_sleep() const;

    void set_contact_monitor(bool p_enabled);
    bool is_contact_monitor_enabled() const;

    void set_max_contacts_reported(int p_amount);
    int get_max_contacts_reported() const;

    void set_use_continuous_collision_detection(bool p_enable);
    bool is_using_continuous_collision_detection() const;

    void set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_lock);
    bool get_axis_lock(PhysicsServer3D::BodyAxis p_axis) const;

    Array get_colliding_bodies() const;

    void add_central_force(const Vector3 &p_force);
    void add_force(const Vector3 &p_force, const Vector3 &p_pos);
    void add_torque(const Vector3 &p_torque);

    void apply_central_impulse(const Vector3 &p_impulse);
    void apply_impulse(const Vector3 &p_pos, const Vector3 &p_impulse);
    void apply_torque_impulse(const Vector3 &p_impulse);

    String get_configuration_warning() const override;

    RigidBody();
    ~RigidBody() override;

private:
    void _reload_physics_characteristics();
};


class KinematicCollision;

class GODOT_EXPORT KinematicBody3D : public PhysicsBody3D {

    GDCLASS(KinematicBody3D,PhysicsBody3D)

public:
    enum MovingPlatformApplyVelocityOnLeave : int8_t {
        PLATFORM_VEL_ON_LEAVE_ALWAYS,
        PLATFORM_VEL_ON_LEAVE_UPWARD_ONLY,
        PLATFORM_VEL_ON_LEAVE_NEVER,
    };
    struct Collision {
        Vector3 collision;
        Vector3 normal;
        Vector3 collider_vel;
        Vector3 remainder;
        Vector3 travel;
        RID collider_rid;
        Variant collider_metadata;
        GameEntity collider;
        int collider_shape;
        int local_shape;
        real_t get_angle(const Vector3 &p_up_direction) const {
            return Math::acos(normal.dot(p_up_direction));
        }
    };

private:
    uint16_t locked_axis = 0;

    float margin;

    Vector3 floor_normal;
    Vector3 floor_velocity;
    RID on_floor_body;
    MovingPlatformApplyVelocityOnLeave moving_platform_apply_velocity_on_leave = PLATFORM_VEL_ON_LEAVE_ALWAYS;
    bool on_floor = false;
    bool on_ceiling = false;
    bool on_wall = false;
    bool sync_to_physics = false;
    Vector<Collision> colliders;
    Vector<Ref<KinematicCollision> > slide_colliders;
    Ref<KinematicCollision> motion_cache;
public:

    Ref<KinematicCollision> _move(const Vector3 &p_motion, bool p_infinite_inertia = true, bool p_exclude_raycast_shapes = true, bool p_test_only = false);
    Ref<KinematicCollision> _get_slide_collision(int p_bounce);
    Ref<KinematicCollision> _get_last_slide_collision();

    Transform last_valid_transform;
    void _direct_state_changed(Object *p_state);

    Vector3 _move_and_slide_internal(const Vector3 &p_linear_velocity, const Vector3 &p_snap, const Vector3 &p_up_direction = Vector3(0, 0, 0), bool p_stop_on_slope = false, int p_max_slides = 4, float p_floor_max_angle = Math::deg2rad((float)45), bool p_infinite_inertia = true);
    void _set_collision_direction(const Collision &p_collision, const Vector3 &p_up_direction, float p_floor_max_angle);
    void set_moving_platform_apply_velocity_on_leave(MovingPlatformApplyVelocityOnLeave p_on_leave_velocity);
    MovingPlatformApplyVelocityOnLeave get_moving_platform_apply_velocity_on_leave() const;

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    bool move_and_collide(const Vector3 &p_motion, bool p_infinite_inertia, Collision &r_collision, bool p_exclude_raycast_shapes = true, bool p_test_only = false, bool p_cancel_sliding = true, const Set<RID> &p_exclude = Set<RID>());
    bool test_move(const Transform &p_from, const Vector3 &p_motion, bool p_infinite_inertia);

    bool separate_raycast_shapes(bool p_infinite_inertia, Collision &r_collision);

    void set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_lock);
    bool get_axis_lock(PhysicsServer3D::BodyAxis p_axis) const;

    void set_safe_margin(float p_margin);
    float get_safe_margin() const;

    Vector3 move_and_slide(const Vector3 &p_linear_velocity, const Vector3 &p_up_direction = Vector3(0, 0, 0), bool p_stop_on_slope = false, int p_max_slides = 4, float p_floor_max_angle = Math::deg2rad((float)45), bool p_infinite_inertia = true);
    Vector3 move_and_slide_with_snap(const Vector3 &p_linear_velocity, const Vector3 &p_snap, const Vector3 &p_up_direction = Vector3(0, 0, 0), bool p_stop_on_slope = false, int p_max_slides = 4, float p_floor_max_angle = Math::deg2rad((float)45), bool p_infinite_inertia = true);
    bool is_on_floor() const;
    bool is_on_wall() const;
    bool is_on_ceiling() const;
    Vector3 get_floor_normal() const;
    real_t get_floor_angle(const Vector3 &p_up_direction = Vector3(0.0, 1.0, 0.0)) const;
    Vector3 get_floor_velocity() const { return floor_velocity; }

    int get_slide_count() const;
    Collision get_slide_collision(int p_bounce) const;

    void set_sync_to_physics(bool p_enable);
    bool is_sync_to_physics_enabled() const;
    KinematicBody3D();
    ~KinematicBody3D() override;
};

class GODOT_EXPORT KinematicCollision : public RefCounted {

    GDCLASS(KinematicCollision,RefCounted)

    KinematicBody3D *owner;
    friend class KinematicBody3D;
    KinematicBody3D::Collision collision;

protected:
    static void _bind_methods();

public:
    Vector3 get_position() const;
    Vector3 get_normal() const;
    Vector3 get_travel() const;
    Vector3 get_remainder() const;
    real_t get_angle(const Vector3 &p_up_direction = Vector3(0.0, 1.0, 0.0)) const;
    Object *get_local_shape() const;
    Object *get_collider() const;
    GameEntity get_collider_id() const;
    RID get_collider_rid() const;
    Object *get_collider_shape() const;
    int get_collider_shape_index() const;
    Vector3 get_collider_velocity() const;
    Variant get_collider_metadata() const;

    KinematicCollision();
};

class GODOT_EXPORT PhysicalBone3D : public PhysicsBody3D {

    GDCLASS(PhysicalBone3D,PhysicsBody3D)

public:
    enum JointType {
        JOINT_TYPE_NONE,
        JOINT_TYPE_PIN,
        JOINT_TYPE_CONE,
        JOINT_TYPE_HINGE,
        JOINT_TYPE_SLIDER,
        JOINT_TYPE_6DOF
    };

    struct JointData {
        virtual JointType get_joint_type() { return JOINT_TYPE_NONE; }

        /// "j" is used to set the parameter inside the PhysicsServer3D
        virtual bool _set(const StringName &p_name, const Variant &p_value, RID j);
        virtual bool _get(const StringName &p_name, Variant &r_ret) const;
        virtual void _get_property_list(Vector<PropertyInfo> *p_list) const;

        virtual ~JointData() {}
    };

    struct PinJointData : public JointData {
        JointType get_joint_type() override { return JOINT_TYPE_PIN; }

        bool _set(const StringName &p_name, const Variant &p_value, RID j) override;
        bool _get(const StringName &p_name, Variant &r_ret) const override;
        void _get_property_list(Vector<PropertyInfo> *p_list) const override;

        real_t bias;
        real_t damping;
        real_t impulse_clamp;

        PinJointData() :
                bias(0.3f),
                damping(1.f),
                impulse_clamp(0) {}
    };

    struct ConeJointData : public JointData {
        JointType get_joint_type() override { return JOINT_TYPE_CONE; }

        bool _set(const StringName &p_name, const Variant &p_value, RID j) override;
        bool _get(const StringName &p_name, Variant &r_ret) const override;
        void _get_property_list(Vector<PropertyInfo> *p_list) const override;

        real_t swing_span;
        real_t twist_span;
        real_t bias;
        real_t softness;
        real_t relaxation;

        ConeJointData() :
                swing_span(Math_PI * 0.25f),
                twist_span(Math_PI),
                bias(0.3f),
                softness(0.8f),
                relaxation(1.f) {}
    };

    struct HingeJointData : public JointData {
        JointType get_joint_type() override { return JOINT_TYPE_HINGE; }

        bool _set(const StringName &p_name, const Variant &p_value, RID j) override;
        bool _get(const StringName &p_name, Variant &r_ret) const override;
        void _get_property_list(Vector<PropertyInfo> *p_list) const override;

        bool angular_limit_enabled;
        real_t angular_limit_upper;
        real_t angular_limit_lower;
        real_t angular_limit_bias;
        real_t angular_limit_softness;
        real_t angular_limit_relaxation;

        HingeJointData() :
                angular_limit_enabled(false),
                angular_limit_upper(Math_PI * 0.5f),
                angular_limit_lower(-Math_PI * 0.5f),
                angular_limit_bias(0.3f),
                angular_limit_softness(0.9f),
                angular_limit_relaxation(1.f) {}
    };

    struct SliderJointData : public JointData {
        JointType get_joint_type() override { return JOINT_TYPE_SLIDER; }

        bool _set(const StringName &p_name, const Variant &p_value, RID j) override;
        bool _get(const StringName &p_name, Variant &r_ret) const override;
        void _get_property_list(Vector<PropertyInfo> *p_list) const override;

        real_t linear_limit_upper;
        real_t linear_limit_lower;
        real_t linear_limit_softness;
        real_t linear_limit_restitution;
        real_t linear_limit_damping;
        real_t angular_limit_upper;
        real_t angular_limit_lower;
        real_t angular_limit_softness;
        real_t angular_limit_restitution;
        real_t angular_limit_damping;

        SliderJointData() :
                linear_limit_upper(1.f),
                linear_limit_lower(-1.f),
                linear_limit_softness(1.f),
                linear_limit_restitution(0.7f),
                linear_limit_damping(1.f),
                angular_limit_upper(0),
                angular_limit_lower(0),
                angular_limit_softness(1.f),
                angular_limit_restitution(0.7f),
                angular_limit_damping(1.f) {}
    };

    struct SixDOFJointData : public JointData {
        struct SixDOFAxisData {
            bool linear_limit_enabled = true;
            real_t linear_limit_upper = 0;
            real_t linear_limit_lower = 0;
            real_t linear_limit_softness = 0.7f;
            real_t linear_restitution = 0.5f;
            real_t linear_damping = 1.f;
            bool linear_spring_enabled = false;
            real_t linear_spring_stiffness = 0;
            real_t linear_spring_damping = 0;
            real_t linear_equilibrium_point = 0;
            bool angular_limit_enabled = true;
            real_t angular_limit_upper = 0;
            real_t angular_limit_lower = 0;
            real_t angular_limit_softness = 0.5f;
            real_t angular_restitution = 0;
            real_t angular_damping = 1.f;
            real_t erp = 0.5f;
            bool angular_spring_enabled = false;
            real_t angular_spring_stiffness = 0;
            real_t angular_spring_damping = 0.;
            real_t angular_equilibrium_point = 0;
        };

        JointType get_joint_type() override { return JOINT_TYPE_6DOF; }

        bool _set(const StringName &p_name, const Variant &p_value, RID j) override;
        bool _get(const StringName &p_name, Variant &r_ret) const override;
        void _get_property_list(Vector<PropertyInfo> *p_list) const override;

        SixDOFAxisData axis_data[3];

        SixDOFJointData() {}
    };

private:
#ifdef TOOLS_ENABLED
    // if false gizmo move body
    bool gizmo_move_joint;
#endif

    JointData *joint_data;
    Transform joint_offset;
    RID joint;

    Skeleton *parent_skeleton;
    Transform body_offset;
    Transform body_offset_inverse;
    bool static_body;
    bool _internal_static_body;
    bool simulate_physics;
    bool _internal_simulate_physics;
    int bone_id;

    StringName bone_name;
    real_t bounce;
    real_t mass;
    real_t friction;
    real_t gravity_scale;

protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(Vector<PropertyInfo> *p_list) const;
    void _notification(int p_what);
    void _direct_state_changed(Object *p_state);

    static void _bind_methods();

private:
    static Skeleton *find_skeleton_parent(Node *p_parent);

    void _fix_joint_offset();
    void _reload_joint();

public:
    void _on_bone_parent_changed();
    void _set_gizmo_move_joint(bool p_move_joint);

public:
#ifdef TOOLS_ENABLED
    Transform get_global_gizmo_transform() const override;
    Transform get_local_gizmo_transform() const override;
#endif

    const JointData *get_joint_data() const;
    Skeleton *find_skeleton_parent();

    int get_bone_id() const { return bone_id; }

    void set_joint_type(JointType p_joint_type);
    JointType get_joint_type() const;

    void set_joint_offset(const Transform &p_offset);
    const Transform &get_joint_offset() const;

    void set_body_offset(const Transform &p_offset);
    const Transform &get_body_offset() const;

    void set_static_body(bool p_static);
    bool is_static_body();

    void set_simulate_physics(bool p_simulate);
    bool get_simulate_physics();
    bool is_simulating_physics();

    void set_bone_name(StringView p_name);
    const StringName &get_bone_name() const;

    void set_mass(real_t p_mass);
    real_t get_mass() const;

    void set_weight(real_t p_weight);
    real_t get_weight() const;

    void set_friction(real_t p_friction);
    real_t get_friction() const;

    void set_bounce(real_t p_bounce);
    real_t get_bounce() const;

    void set_gravity_scale(real_t p_gravity_scale);
    real_t get_gravity_scale() const;

    void apply_central_impulse(const Vector3 &p_impulse);
    void apply_impulse(const Vector3 &p_pos, const Vector3 &p_impulse);

    PhysicalBone3D();
    ~PhysicalBone3D() override;

private:
    void update_bone_id();
    void update_offset();
    void reset_to_rest_position();

    void _reset_physics_simulation_state();
    void _reset_staticness_state();

    void _start_physics_simulation();
    void _stop_physics_simulation();
};
