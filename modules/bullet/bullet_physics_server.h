/*************************************************************************/
/*  bullet_physics_server.h                                              */
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

#include "core/rid.h"
#include "servers/physics_server.h"

/**
    @author AndreaCatania
*/
class SpaceBullet;
class AreaBullet;
class ShapeBullet;
class RigidBodyBullet;
class SoftBodyBullet;
class JointBullet;
class CollisionObjectBullet;
class RigidCollisionObjectBullet;

class BulletPhysicsServer : public PhysicsServer {
    GDCLASS(BulletPhysicsServer,PhysicsServer)

    friend class BulletPhysicsDirectSpaceState;

    bool active;
    char active_spaces_count;
    PODVector<SpaceBullet *> active_spaces;

    mutable RID_Owner<SpaceBullet> space_owner;
    mutable RID_Owner<ShapeBullet> shape_owner;
    mutable RID_Owner<AreaBullet> area_owner;
    mutable RID_Owner<RigidBodyBullet> rigid_body_owner;
    mutable RID_Owner<SoftBodyBullet> soft_body_owner;
    mutable RID_Owner<JointBullet> joint_owner;

protected:
    static void _bind_methods();

public:
    BulletPhysicsServer();
    ~BulletPhysicsServer() override;

    _FORCE_INLINE_ RID_Owner<SpaceBullet> *get_space_owner() {
        return &space_owner;
    }
    _FORCE_INLINE_ RID_Owner<ShapeBullet> *get_shape_owner() {
        return &shape_owner;
    }
    _FORCE_INLINE_ RID_Owner<AreaBullet> *get_area_owner() {
        return &area_owner;
    }
    _FORCE_INLINE_ RID_Owner<RigidBodyBullet> *get_rigid_body_owner() {
        return &rigid_body_owner;
    }
    _FORCE_INLINE_ RID_Owner<SoftBodyBullet> *get_soft_body_owner() {
        return &soft_body_owner;
    }
    _FORCE_INLINE_ RID_Owner<JointBullet> *get_joint_owner() {
        return &joint_owner;
    }

    /* SHAPE API */
    RID shape_create(ShapeType p_shape) override;
    void shape_set_data(RID p_shape, const Variant &p_data) override;
    ShapeType shape_get_type(RID p_shape) const override;
    Variant shape_get_data(RID p_shape) const override;

    void shape_set_margin(RID p_shape, real_t p_margin) override;
    real_t shape_get_margin(RID p_shape) const override;

    /// Not supported
    void shape_set_custom_solver_bias(RID p_shape, real_t p_bias) override;
    /// Not supported
    real_t shape_get_custom_solver_bias(RID p_shape) const override;

    /* SPACE API */

    RID space_create() override;
    void space_set_active(RID p_space, bool p_active) override;
    bool space_is_active(RID p_space) const override;

    /// Not supported
    void space_set_param(RID p_space, SpaceParameter p_param, real_t p_value) override;
    /// Not supported
    real_t space_get_param(RID p_space, SpaceParameter p_param) const override;

    PhysicsDirectSpaceState *space_get_direct_state(RID p_space) override;

    void space_set_debug_contacts(RID p_space, int p_max_contacts) override;
    const PODVector<Vector3> &space_get_contacts(RID p_space) const override;
    int space_get_contact_count(RID p_space) const override;

    /* AREA API */

    /// Bullet Physics Engine not support "Area", this must be handled by the game developer in another way.
    /// Since godot Physics use the concept of area even to define the main world, the API area_set_param is used to set initial physics world information.
    /// The API area_set_param is a bit hacky, and allow Godot to set some parameters on Bullet's world, a different use print a warning to console.
    /// All other APIs returns a warning message if used

    RID area_create() override;

    void area_set_space(RID p_area, RID p_space) override;

    RID area_get_space(RID p_area) const override;

    void area_set_space_override_mode(RID p_area, AreaSpaceOverrideMode p_mode) override;
    AreaSpaceOverrideMode area_get_space_override_mode(RID p_area) const override;

    void area_add_shape(RID p_area, RID p_shape, const Transform &p_transform = Transform(), bool p_disabled = false) override;
    void area_set_shape(RID p_area, int p_shape_idx, RID p_shape) override;
    void area_set_shape_transform(RID p_area, int p_shape_idx, const Transform &p_transform) override;
    int area_get_shape_count(RID p_area) const override;
    RID area_get_shape(RID p_area, int p_shape_idx) const override;
    Transform area_get_shape_transform(RID p_area, int p_shape_idx) const override;
    void area_remove_shape(RID p_area, int p_shape_idx) override;
    void area_clear_shapes(RID p_area) override;
    void area_set_shape_disabled(RID p_area, int p_shape_idx, bool p_disabled) override;
    void area_attach_object_instance_id(RID p_area, ObjectID p_id) override;
    ObjectID area_get_object_instance_id(RID p_area) const override;

    /// If you pass as p_area the SpaceBullet you can set some parameters as specified below
    /// AREA_PARAM_GRAVITY
    /// AREA_PARAM_GRAVITY_VECTOR
    /// Otherwise you can set area parameters
    void area_set_param(RID p_area, AreaParameter p_param, const Variant &p_value) override;
    Variant area_get_param(RID p_area, AreaParameter p_param) const override;

    void area_set_transform(RID p_area, const Transform &p_transform) override;
    Transform area_get_transform(RID p_area) const override;

    void area_set_collision_mask(RID p_area, uint32_t p_mask) override;
    void area_set_collision_layer(RID p_area, uint32_t p_layer) override;

    void area_set_monitorable(RID p_area, bool p_monitorable) override;
    void area_set_monitor_callback(RID p_area, Object *p_receiver, const StringName &p_method) override;
    void area_set_area_monitor_callback(RID p_area, Object *p_receiver, const StringName &p_method) override;
    void area_set_ray_pickable(RID p_area, bool p_enable) override;
    bool area_is_ray_pickable(RID p_area) const override;

    /* RIGID BODY API */

    RID body_create(BodyMode p_mode = BODY_MODE_RIGID, bool p_init_sleeping = false) override;

    void body_set_space(RID p_body, RID p_space) override;
    RID body_get_space(RID p_body) const override;

    void body_set_mode(RID p_body, BodyMode p_mode) override;
    BodyMode body_get_mode(RID p_body) const override;

    void body_add_shape(RID p_body, RID p_shape, const Transform &p_transform = Transform(), bool p_disabled = false) override;
    // Not supported, Please remove and add new shape
    void body_set_shape(RID p_body, int p_shape_idx, RID p_shape) override;
    void body_set_shape_transform(RID p_body, int p_shape_idx, const Transform &p_transform) override;

    int body_get_shape_count(RID p_body) const override;
    RID body_get_shape(RID p_body, int p_shape_idx) const override;
    Transform body_get_shape_transform(RID p_body, int p_shape_idx) const override;

    void body_set_shape_disabled(RID p_body, int p_shape_idx, bool p_disabled) override;

    void body_remove_shape(RID p_body, int p_shape_idx) override;
    void body_clear_shapes(RID p_body) override;

    // Used for Rigid and Soft Bodies
    void body_attach_object_instance_id(RID p_body, uint32_t p_id) override;
    uint32_t body_get_object_instance_id(RID p_body) const override;

    void body_set_enable_continuous_collision_detection(RID p_body, bool p_enable) override;
    bool body_is_continuous_collision_detection_enabled(RID p_body) const override;

    void body_set_collision_layer(RID p_body, uint32_t p_layer) override;
    uint32_t body_get_collision_layer(RID p_body) const override;

    void body_set_collision_mask(RID p_body, uint32_t p_mask) override;
    uint32_t body_get_collision_mask(RID p_body) const override;

    /// This is not supported by physics server
    void body_set_user_flags(RID p_body, uint32_t p_flags) override;
    /// This is not supported by physics server
    uint32_t body_get_user_flags(RID p_body) const override;

    void body_set_param(RID p_body, BodyParameter p_param, float p_value) override;
    float body_get_param(RID p_body, BodyParameter p_param) const override;

    void body_set_kinematic_safe_margin(RID p_body, real_t p_margin) override;
    real_t body_get_kinematic_safe_margin(RID p_body) const override;

    void body_set_state(RID p_body, BodyState p_state, const Variant &p_variant) override;
    Variant body_get_state(RID p_body, BodyState p_state) const override;

    void body_set_applied_force(RID p_body, const Vector3 &p_force) override;
    Vector3 body_get_applied_force(RID p_body) const override;

    void body_set_applied_torque(RID p_body, const Vector3 &p_torque) override;
    Vector3 body_get_applied_torque(RID p_body) const override;

    void body_add_central_force(RID p_body, const Vector3 &p_force) override;
    void body_add_force(RID p_body, const Vector3 &p_force, const Vector3 &p_pos) override;
    void body_add_torque(RID p_body, const Vector3 &p_torque) override;

    void body_apply_central_impulse(RID p_body, const Vector3 &p_impulse) override;
    void body_apply_impulse(RID p_body, const Vector3 &p_pos, const Vector3 &p_impulse) override;
    void body_apply_torque_impulse(RID p_body, const Vector3 &p_impulse) override;
    void body_set_axis_velocity(RID p_body, const Vector3 &p_axis_velocity) override;

    void body_set_axis_lock(RID p_body, BodyAxis p_axis, bool p_lock) override;
    bool body_is_axis_locked(RID p_body, BodyAxis p_axis) const override;

    void body_add_collision_exception(RID p_body, RID p_body_b) override;
    void body_remove_collision_exception(RID p_body, RID p_body_b) override;
    void body_get_collision_exceptions(RID p_body, DefList<RID> *p_exceptions) override;

    void body_set_max_contacts_reported(RID p_body, int p_contacts) override;
    int body_get_max_contacts_reported(RID p_body) const override;

    void body_set_contacts_reported_depth_threshold(RID p_body, float p_threshold) override;
    float body_get_contacts_reported_depth_threshold(RID p_body) const override;

    void body_set_omit_force_integration(RID p_body, bool p_omit) override;
    bool body_is_omitting_force_integration(RID p_body) const override;

    void body_set_force_integration_callback(RID p_body, Object *p_receiver, const StringName &p_method, const Variant &p_udata = Variant()) override;

    void body_set_ray_pickable(RID p_body, bool p_enable) override;
    bool body_is_ray_pickable(RID p_body) const override;

    // this function only works on physics process, errors and returns null otherwise
    PhysicsDirectBodyState *body_get_direct_state(RID p_body) override;

    bool body_test_motion(RID p_body, const Transform &p_from, const Vector3 &p_motion, bool p_infinite_inertia, MotionResult *r_result = nullptr, bool p_exclude_raycast_shapes = true) override;
    int body_test_ray_separation(RID p_body, const Transform &p_transform, bool p_infinite_inertia, Vector3 &r_recover_motion, SeparationResult *r_results, int p_result_max, float p_margin = 0.001) override;

    /* SOFT BODY API */

    RID soft_body_create(bool p_init_sleeping = false) override;

    void soft_body_update_visual_server(RID p_body, class SoftBodyVisualServerHandler *p_visual_server_handler) override;

    void soft_body_set_space(RID p_body, RID p_space) override;
    RID soft_body_get_space(RID p_body) const override;

    void soft_body_set_mesh(RID p_body, const REF &p_mesh) override;

    void soft_body_set_collision_layer(RID p_body, uint32_t p_layer) override;
    uint32_t soft_body_get_collision_layer(RID p_body) const override;

    void soft_body_set_collision_mask(RID p_body, uint32_t p_mask) override;
    uint32_t soft_body_get_collision_mask(RID p_body) const override;

    void soft_body_add_collision_exception(RID p_body, RID p_body_b) override;
    void soft_body_remove_collision_exception(RID p_body, RID p_body_b) override;
    void soft_body_get_collision_exceptions(RID p_body, DefList<RID> *p_exceptions) override;

    void soft_body_set_state(RID p_body, BodyState p_state, const Variant &p_variant) override;
    Variant soft_body_get_state(RID p_body, BodyState p_state) const override;

    /// Special function. This function has bad performance
    void soft_body_set_transform(RID p_body, const Transform &p_transform) override;
    Vector3 soft_body_get_vertex_position(RID p_body, int vertex_index) const override;

    void soft_body_set_ray_pickable(RID p_body, bool p_enable) override;
    bool soft_body_is_ray_pickable(RID p_body) const override;

    void soft_body_set_simulation_precision(RID p_body, int p_simulation_precision) override;
    int soft_body_get_simulation_precision(RID p_body) override;

    void soft_body_set_total_mass(RID p_body, real_t p_total_mass) override;
    real_t soft_body_get_total_mass(RID p_body) override;

    void soft_body_set_linear_stiffness(RID p_body, real_t p_stiffness) override;
    real_t soft_body_get_linear_stiffness(RID p_body) override;

    void soft_body_set_areaAngular_stiffness(RID p_body, real_t p_stiffness) override;
    real_t soft_body_get_areaAngular_stiffness(RID p_body) override;

    void soft_body_set_volume_stiffness(RID p_body, real_t p_stiffness) override;
    real_t soft_body_get_volume_stiffness(RID p_body) override;

    void soft_body_set_pressure_coefficient(RID p_body, real_t p_pressure_coefficient) override;
    real_t soft_body_get_pressure_coefficient(RID p_body) override;

    void soft_body_set_pose_matching_coefficient(RID p_body, real_t p_pose_matching_coefficient) override;
    real_t soft_body_get_pose_matching_coefficient(RID p_body) override;

    void soft_body_set_damping_coefficient(RID p_body, real_t p_damping_coefficient) override;
    real_t soft_body_get_damping_coefficient(RID p_body) override;

    void soft_body_set_drag_coefficient(RID p_body, real_t p_drag_coefficient) override;
    real_t soft_body_get_drag_coefficient(RID p_body) override;

    void soft_body_move_point(RID p_body, int p_point_index, const Vector3 &p_global_position) override;
    Vector3 soft_body_get_point_global_position(RID p_body, int p_point_index) override;

    Vector3 soft_body_get_point_offset(RID p_body, int p_point_index) const override;

    void soft_body_remove_all_pinned_points(RID p_body) override;
    void soft_body_pin_point(RID p_body, int p_point_index, bool p_pin) override;
    bool soft_body_is_point_pinned(RID p_body, int p_point_index) override;

    /* JOINT API */

    JointType joint_get_type(RID p_joint) const override;

    void joint_set_solver_priority(RID p_joint, int p_priority) override;
    int joint_get_solver_priority(RID p_joint) const override;

    void joint_disable_collisions_between_bodies(RID p_joint, const bool p_disable) override;
    bool joint_is_disabled_collisions_between_bodies(RID p_joint) const override;

    RID joint_create_pin(RID p_body_A, const Vector3 &p_local_A, RID p_body_B, const Vector3 &p_local_B) override;

    void pin_joint_set_param(RID p_joint, PinJointParam p_param, float p_value) override;
    float pin_joint_get_param(RID p_joint, PinJointParam p_param) const override;

    void pin_joint_set_local_a(RID p_joint, const Vector3 &p_A) override;
    Vector3 pin_joint_get_local_a(RID p_joint) const override;

    void pin_joint_set_local_b(RID p_joint, const Vector3 &p_B) override;
    Vector3 pin_joint_get_local_b(RID p_joint) const override;

    RID joint_create_hinge(RID p_body_A, const Transform &p_hinge_A, RID p_body_B, const Transform &p_hinge_B) override;
    RID joint_create_hinge_simple(RID p_body_A, const Vector3 &p_pivot_A, const Vector3 &p_axis_A, RID p_body_B, const Vector3 &p_pivot_B, const Vector3 &p_axis_B) override;

    void hinge_joint_set_param(RID p_joint, HingeJointParam p_param, float p_value) override;
    float hinge_joint_get_param(RID p_joint, HingeJointParam p_param) const override;

    void hinge_joint_set_flag(RID p_joint, HingeJointFlag p_flag, bool p_value) override;
    bool hinge_joint_get_flag(RID p_joint, HingeJointFlag p_flag) const override;

    /// Reference frame is A
    RID joint_create_slider(RID p_body_A, const Transform &p_local_frame_A, RID p_body_B, const Transform &p_local_frame_B) override;

    void slider_joint_set_param(RID p_joint, SliderJointParam p_param, float p_value) override;
    float slider_joint_get_param(RID p_joint, SliderJointParam p_param) const override;

    /// Reference frame is A
    RID joint_create_cone_twist(RID p_body_A, const Transform &p_local_frame_A, RID p_body_B, const Transform &p_local_frame_B) override;

    void cone_twist_joint_set_param(RID p_joint, ConeTwistJointParam p_param, float p_value) override;
    float cone_twist_joint_get_param(RID p_joint, ConeTwistJointParam p_param) const override;

    /// Reference frame is A
    RID joint_create_generic_6dof(RID p_body_A, const Transform &p_local_frame_A, RID p_body_B, const Transform &p_local_frame_B) override;

    void generic_6dof_joint_set_param(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisParam p_param, float p_value) override;
    float generic_6dof_joint_get_param(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisParam p_param) override;

    void generic_6dof_joint_set_flag(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisFlag p_flag, bool p_enable) override;
    bool generic_6dof_joint_get_flag(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisFlag p_flag) override;

    void generic_6dof_joint_set_precision(RID p_joint, int precision) override;
    int generic_6dof_joint_get_precision(RID p_joint) override;

    /* MISC */

    void free_rid(RID p_rid) override;

    void set_active(bool p_active) override {
        active = p_active;
    }

    static bool singleton_isActive() {
        return static_cast<BulletPhysicsServer *>(get_singleton())->active;
    }

    bool isActive() {
        return active;
    }

    void init() override;
    void step(float p_deltaTime) override;
    void sync() override;
    void flush_queries() override;
    void finish() override;

    bool is_flushing_queries() const override { return false; }

    int get_process_info(ProcessInfo p_info) override;

    CollisionObjectBullet *get_collisin_object(RID p_object) const;
    RigidCollisionObjectBullet *get_rigid_collisin_object(RID p_object) const;

    /// Internal APIs
public:
};
