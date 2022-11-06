/*************************************************************************/
/*  physics_body.cpp                                                     */
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

#include "physics_body_3d.h"

#include "core/callable_method_pointer.h"
#include "core/core_string_names.h"
#include "core/engine.h"
#include "core/list.h"
#include "core/method_bind.h"
#include "core/object.h"
#include "core/object_db.h"
#include "core/object_tooling.h"
#include "core/project_settings.h"
#include "core/rid.h"
#include "core/script_language.h"
#include "core/translation_helpers.h"
#include "scene/scene_string_names.h"

#ifdef TOOLS_ENABLED
#include "editor/plugins/node_3d_editor_plugin.h"
#endif

IMPL_GDCLASS(PhysicsBody3D)
IMPL_GDCLASS(RigidBody)
IMPL_GDCLASS(KinematicBody3D)
IMPL_GDCLASS(KinematicCollision)
IMPL_GDCLASS(PhysicalBone3D)
IMPL_GDCLASS(StaticBody3D)

//TODO: SEGS: this is duplicating instantiation in physics_server_3d.cpp
VARIANT_ENUM_CAST(PhysicsServer3D::BodyAxis);
VARIANT_ENUM_CAST(RigidBody::Mode);
VARIANT_ENUM_CAST(PhysicalBone3D::JointType);
VARIANT_ENUM_CAST(KinematicBody3D::MovingPlatformApplyVelocityOnLeave);


void PhysicsBody3D::_notification(int p_what) {}

Vector3 PhysicsBody3D::get_linear_velocity() const {

    return Vector3();
}
Vector3 PhysicsBody3D::get_angular_velocity() const {

    return Vector3();
}

float PhysicsBody3D::get_inverse_mass() const {

    return 0;
}

Array PhysicsBody3D::get_collision_exceptions() {
    Vector<RID> exceptions;
    PhysicsServer3D::get_singleton()->body_get_collision_exceptions(get_rid(), &exceptions);
    Array ret;
    for (RID body : exceptions) {
        GameEntity instance_id = PhysicsServer3D::get_singleton()->body_get_object_instance_id(body);
        Object *obj = object_for_entity(instance_id);
        PhysicsBody3D *physics_body = object_cast<PhysicsBody3D>(obj);
        ret.append(Variant(physics_body));
    }
    return ret;
}

void PhysicsBody3D::add_collision_exception_with(Node *p_node) {

    ERR_FAIL_NULL(p_node);
    CollisionObject3D *collision_object = object_cast<CollisionObject3D>(p_node);
    ERR_FAIL_COND_MSG(!collision_object, "Collision exception only works between two CollisionObject3D.");
    PhysicsServer3D::get_singleton()->body_add_collision_exception(get_rid(), collision_object->get_rid());
}

void PhysicsBody3D::remove_collision_exception_with(Node *p_node) {

    ERR_FAIL_NULL(p_node);
    CollisionObject3D *collision_object = object_cast<CollisionObject3D>(p_node);
    ERR_FAIL_COND_MSG(!collision_object, "Collision exception only works between two CollisionObject3D.");
    PhysicsServer3D::get_singleton()->body_remove_collision_exception(get_rid(), collision_object->get_rid());
}

void PhysicsBody3D::_set_layers(uint32_t p_mask) {
    set_collision_layer(p_mask);
    set_collision_mask(p_mask);
}

uint32_t PhysicsBody3D::_get_layers() const {

    return get_collision_layer();
}

void PhysicsBody3D::_bind_methods() {

    SE_BIND_METHOD(PhysicsBody3D,_set_layers);
    SE_BIND_METHOD(PhysicsBody3D,_get_layers);

}

PhysicsBody3D::PhysicsBody3D(PhysicsServer3D::BodyMode p_mode) :
        CollisionObject3D(PhysicsServer3D::get_singleton()->body_create(p_mode), false) {

    collision_layer = 1;
    collision_mask = 1;
}

// WARN_DEPRECATED_MSG("The method set_friction has been deprecated and will be removed in the future, use physics
// material instead."); WARN_DEPRECATED_MSG("The method get_friction has been deprecated and will be removed in the
// future, use physics material instead."); WARN_DEPRECATED_MSG("The method set_bounce has been deprecated and will be
// removed in the future, use physics material instead."); WARN_DEPRECATED_MSG("The method get_bounce has been deprecated
// and will be removed in the future, use physics material instead.");

void StaticBody3D::set_physics_material_override(const Ref<PhysicsMaterial> &p_physics_material_override) {
    if (physics_material_override) {
        if (physics_material_override->is_connected(CoreStringNames::get_singleton()->changed,
                    callable_mp(this, &StaticBody3D::_reload_physics_characteristics))) {
            physics_material_override->disconnect(CoreStringNames::get_singleton()->changed,
                    callable_mp(this, &StaticBody3D::_reload_physics_characteristics));
        }
    }

    physics_material_override = p_physics_material_override;

    if (physics_material_override) {
        physics_material_override->connect(CoreStringNames::get_singleton()->changed,
                callable_mp(this, &StaticBody3D::_reload_physics_characteristics));
    }
    _reload_physics_characteristics();

}

Ref<PhysicsMaterial> StaticBody3D::get_physics_material_override() const {
    return physics_material_override;
}

void StaticBody3D::set_constant_linear_velocity(const Vector3 &p_vel) {

    constant_linear_velocity = p_vel;
    PhysicsServer3D::get_singleton()->body_set_state(
            get_rid(), PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY, constant_linear_velocity);
}

void StaticBody3D::set_constant_angular_velocity(const Vector3 &p_vel) {

    constant_angular_velocity = p_vel;
    PhysicsServer3D::get_singleton()->body_set_state(
            get_rid(), PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY, constant_angular_velocity);
}

Vector3 StaticBody3D::get_constant_linear_velocity() const {

    return constant_linear_velocity;
}
Vector3 StaticBody3D::get_constant_angular_velocity() const {

    return constant_angular_velocity;
}

void StaticBody3D::_bind_methods() {

    MethodBinder::bind_method(
            D_METHOD("set_constant_linear_velocity", { "vel" }), &StaticBody3D::set_constant_linear_velocity);
    MethodBinder::bind_method(
            D_METHOD("set_constant_angular_velocity", { "vel" }), &StaticBody3D::set_constant_angular_velocity);
    SE_BIND_METHOD(StaticBody3D,get_constant_linear_velocity);
    SE_BIND_METHOD(StaticBody3D,get_constant_angular_velocity);

    MethodBinder::bind_method(D_METHOD("set_physics_material_override", { "physics_material_override" }),
            &StaticBody3D::set_physics_material_override);
    SE_BIND_METHOD(StaticBody3D,get_physics_material_override);

    SE_BIND_METHOD(PhysicsBody3D,get_collision_exceptions);
    MethodBinder::bind_method(
            D_METHOD("add_collision_exception_with", { "body" }), &PhysicsBody3D::add_collision_exception_with);
    MethodBinder::bind_method(
            D_METHOD("remove_collision_exception_with", { "body" }), &PhysicsBody3D::remove_collision_exception_with);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "physics_material_override", PropertyHint::ResourceType,
                         "PhysicsMaterial"),
            "set_physics_material_override", "get_physics_material_override");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "constant_linear_velocity"), "set_constant_linear_velocity",
            "get_constant_linear_velocity");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "constant_angular_velocity"), "set_constant_angular_velocity",
            "get_constant_angular_velocity");
}

StaticBody3D::StaticBody3D() : PhysicsBody3D(PhysicsServer3D::BODY_MODE_STATIC) {}

StaticBody3D::~StaticBody3D() {}

void StaticBody3D::_reload_physics_characteristics() {
    if (not physics_material_override) {
        PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_BOUNCE, 0);
        PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_FRICTION, 1);
    } else {
        PhysicsServer3D::get_singleton()->body_set_param(
                get_rid(), PhysicsServer3D::BODY_PARAM_BOUNCE, physics_material_override->computed_bounce());
        PhysicsServer3D::get_singleton()->body_set_param(
                get_rid(), PhysicsServer3D::BODY_PARAM_FRICTION, physics_material_override->computed_friction());
    }
}

void RigidBody::_body_enter_tree(GameEntity p_id) {

    Object *obj = object_for_entity(p_id);
    Node *node = object_cast<Node>(obj);
    ERR_FAIL_COND(!node);

    ERR_FAIL_COND(!contact_monitor);
    auto E = contact_monitor->body_map.find(p_id);
    ERR_FAIL_COND(E==contact_monitor->body_map.end());
    ERR_FAIL_COND(E->second.in_tree);

    E->second.in_tree = true;

    contact_monitor->locked = true;

    emit_signal(SceneStringNames::body_entered, Variant(node));

    for (size_t i = 0; i < E->second.shapes.size(); i++) {

        emit_signal(SceneStringNames::body_shape_entered, Variant::from(E->second.rid), Variant(node),
                E->second.shapes[i].body_shape, E->second.shapes[i].local_shape);
    }

    contact_monitor->locked = false;
}

void RigidBody::_body_exit_tree(GameEntity p_id) {

    Object *obj = object_for_entity(p_id);
    Node *node = object_cast<Node>(obj);
    ERR_FAIL_COND(!node);
    ERR_FAIL_COND(!contact_monitor);
    auto E = contact_monitor->body_map.find(p_id);
    ERR_FAIL_COND(E==contact_monitor->body_map.end());
    ERR_FAIL_COND(!E->second.in_tree);
    E->second.in_tree = false;

    contact_monitor->locked = true;

    emit_signal(SceneStringNames::body_exited, Variant(node));

    for (size_t i = 0; i < E->second.shapes.size(); i++) {

        emit_signal(SceneStringNames::body_shape_exited, Variant::from(E->second.rid), Variant(node),
                E->second.shapes[i].body_shape, E->second.shapes[i].local_shape);
    }

    contact_monitor->locked = false;
}

void RigidBody::_body_inout(int p_status, const RID &p_body, GameEntity p_instance, int p_body_shape, int p_local_shape) {

    bool body_in = p_status == 1;
    GameEntity objid = p_instance;

    Object *obj = object_for_entity(objid);
    Node *node = object_cast<Node>(obj);

    ERR_FAIL_COND(!contact_monitor);
    auto E = contact_monitor->body_map.find(objid);

    ERR_FAIL_COND(!body_in && E==contact_monitor->body_map.end());

    if (body_in) {
        if (E==contact_monitor->body_map.end()) {

            E = contact_monitor->body_map.emplace(objid, BodyState()).first;
            E->second.rid=p_body;
            E->second.in_tree = node && node->is_inside_tree();
            if (node) {
                node->connect(SceneStringNames::tree_entered, callable_gen(this, [=]() { _body_enter_tree(objid); }));
                node->connect(SceneStringNames::tree_exiting, callable_gen(this, [=]() { _body_exit_tree(objid); }));

                if (E->second.in_tree) {
                    emit_signal(SceneStringNames::body_entered, Variant(node));
                }
            }
        }
        //E->second.rc++;
        if (node)
            E->second.shapes.insert(ShapePair(p_body_shape, p_local_shape));

        if (E->second.in_tree) {
            emit_signal(SceneStringNames::body_shape_entered, Variant::from(p_body), Variant(node), p_body_shape,
                    p_local_shape);
        }

    } else {

        //E.second.rc--;

        if (node)
            E->second.shapes.erase(ShapePair(p_body_shape, p_local_shape));

        bool in_tree = E->second.in_tree;

        if (E->second.shapes.empty()) {

            if (node) {
                node->disconnect_all(SceneStringNames::tree_entered, get_instance_id());
                node->disconnect_all(SceneStringNames::tree_exiting, get_instance_id());
                if (in_tree)
                    emit_signal(SceneStringNames::body_exited, Variant(node));
            }

            contact_monitor->body_map.erase(E);
        }
        if (node && in_tree) {
            emit_signal(SceneStringNames::body_shape_exited, Variant::from(p_body), Variant(obj), p_body_shape,
                    p_local_shape);
        }
    }
}

struct _RigidBodyInOut {

    RID rid;
    GameEntity id;
    int shape;
    int local_shape;
};

void RigidBody::_direct_state_changed(Object *p_state) {

    state = object_cast<PhysicsDirectBodyState3D>(p_state);
    ERR_FAIL_COND_MSG(
            !state, "Method '_direct_state_changed' must receive a valid PhysicsDirectBodyState object as argument");

    set_ignore_transform_notification(true);
    set_global_transform(state->get_transform());
    linear_velocity = state->get_linear_velocity();
    angular_velocity = state->get_angular_velocity();
    inverse_inertia_tensor = state->get_inverse_inertia_tensor();
    if (sleeping != state->is_sleeping()) {
        sleeping = state->is_sleeping();
        emit_signal(SceneStringNames::sleeping_state_changed);
    }
    if (get_script_instance())
        get_script_instance()->call("_integrate_forces", Variant(state));
    set_ignore_transform_notification(false);
    _on_transform_changed();

    if (contact_monitor) {

        contact_monitor->locked = true;

        //untag all
        int rc = 0;
        for (eastl::pair<const GameEntity,BodyState> &E : contact_monitor->body_map) {

            for (size_t i = 0; i < E.second.shapes.size(); i++) {

                E.second.shapes[i].tagged = false;
                rc++;
            }
        }

        _RigidBodyInOut *toadd = (_RigidBodyInOut *)alloca(state->get_contact_count() * sizeof(_RigidBodyInOut));
        int toadd_count = 0; //state->get_contact_count();
        RigidBody_RemoveAction *toremove = (RigidBody_RemoveAction *)alloca(rc * sizeof(RigidBody_RemoveAction));
        int toremove_count = 0;

        //put the ones to add

        for (int i = 0; i < state->get_contact_count(); i++) {

            RID rid = state->get_contact_collider(i);
            GameEntity obj = state->get_contact_collider_id(i);
            int local_shape = state->get_contact_local_shape(i);
            int shape = state->get_contact_collider_shape(i);

            //bool found=false;

            HashMap<GameEntity, BodyState>::iterator E = contact_monitor->body_map.find(obj);
            if (E==contact_monitor->body_map.end()) {
                toadd[toadd_count].rid = rid;
                toadd[toadd_count].local_shape = local_shape;
                toadd[toadd_count].id = obj;
                toadd[toadd_count].shape = shape;
                toadd_count++;
                continue;
            }

            ShapePair sp(shape, local_shape);
            auto idx = E->second.shapes.find(sp);
            if (idx==E->second.shapes.end()) {

                toadd[toadd_count].rid = rid;
                toadd[toadd_count].local_shape = local_shape;
                toadd[toadd_count].id = obj;
                toadd[toadd_count].shape = shape;
                toadd_count++;
                continue;
            }

            idx->tagged = true;
        }

        //put the ones to remove

        for (eastl::pair<const GameEntity,BodyState> &E : contact_monitor->body_map) {

            for (auto &i : E.second.shapes) {

                if (!i.tagged) {

                    toremove[toremove_count].rid = E.second.rid;
                    toremove[toremove_count].body_id = E.first;
                    toremove[toremove_count].pair = i;
                    toremove_count++;
                }
            }
        }

        //process remotions

        for (int i = 0; i < toremove_count; i++) {

            _body_inout(0, toremove[i].rid, toremove[i].body_id, toremove[i].pair.body_shape, toremove[i].pair.local_shape);
        }

        //process aditions

        for (int i = 0; i < toadd_count; i++) {

            _body_inout(1, toremove[i].rid, toadd[i].id, toadd[i].shape, toadd[i].local_shape);
        }

        contact_monitor->locked = false;
    }

    state = nullptr;
}

void RigidBody::_notification(int p_what) {

#ifdef TOOLS_ENABLED
    if (p_what == NOTIFICATION_ENTER_TREE) {
        if (Engine::get_singleton()->is_editor_hint()) {
            set_notify_local_transform(true); //used for warnings and only in editor
        }
    }

    if (p_what == NOTIFICATION_LOCAL_TRANSFORM_CHANGED) {
        if (Engine::get_singleton()->is_editor_hint()) {
            update_configuration_warning();
        }
    }

#endif
}

void RigidBody::set_mode(Mode p_mode) {

    mode = p_mode;
    switch (p_mode) {

        case MODE_RIGID: {

            PhysicsServer3D::get_singleton()->body_set_mode(get_rid(), PhysicsServer3D::BODY_MODE_RIGID);
        } break;
        case MODE_STATIC: {

            PhysicsServer3D::get_singleton()->body_set_mode(get_rid(), PhysicsServer3D::BODY_MODE_STATIC);

        } break;
        case MODE_CHARACTER: {
            PhysicsServer3D::get_singleton()->body_set_mode(get_rid(), PhysicsServer3D::BODY_MODE_CHARACTER);

        } break;
        case MODE_KINEMATIC: {

            PhysicsServer3D::get_singleton()->body_set_mode(get_rid(), PhysicsServer3D::BODY_MODE_KINEMATIC);
        } break;
    }
    update_configuration_warning();
}

RigidBody::Mode RigidBody::get_mode() const {

    return mode;
}

void RigidBody::set_mass(real_t p_mass) {

    ERR_FAIL_COND(p_mass <= 0);
    mass = p_mass;
    Object_change_notify(this,"mass");
    Object_change_notify(this,"weight");
    PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_MASS, mass);
}
real_t RigidBody::get_mass() const {

    return mass;
}

void RigidBody::set_weight(real_t p_weight) {

    set_mass(p_weight / real_t(T_GLOBAL_DEF<float>("physics/3d/default_gravity", 9.8f)));
}
real_t RigidBody::get_weight() const {

    return mass * real_t(T_GLOBAL_DEF<float>("physics/3d/default_gravity", 9.8f));
}

// WARN_DEPRECATED_MSG("The method set_friction has been deprecated and will be removed in the future, use physics
// material instead."); WARN_DEPRECATED_MSG("The method get_friction has been deprecated and will be removed in the
// future, use physics material instead."); WARN_DEPRECATED_MSG("The method set_bounce has been deprecated and will be
// removed in the future, use physics material instead."); WARN_DEPRECATED_MSG("The method get_bounce has been deprecated
// and will be removed in the future, use physics material instead.");

void RigidBody::set_physics_material_override(const Ref<PhysicsMaterial> &p_physics_material_override) {
    if (physics_material_override) {
        if (physics_material_override->is_connected(CoreStringNames::get_singleton()->changed,
                    callable_mp(this, &RigidBody::_reload_physics_characteristics))) {
            physics_material_override->disconnect(CoreStringNames::get_singleton()->changed,
                    callable_mp(this, &RigidBody::_reload_physics_characteristics));
        }
    }

    physics_material_override = p_physics_material_override;

    if (physics_material_override) {
        physics_material_override->connect(CoreStringNames::get_singleton()->changed,
                callable_mp(this, &RigidBody::_reload_physics_characteristics));
    }
    _reload_physics_characteristics();
}

Ref<PhysicsMaterial> RigidBody::get_physics_material_override() const {
    return physics_material_override;
}

void RigidBody::set_gravity_scale(real_t p_gravity_scale) {

    gravity_scale = p_gravity_scale;
    PhysicsServer3D::get_singleton()->body_set_param(
            get_rid(), PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE, gravity_scale);
}
real_t RigidBody::get_gravity_scale() const {

    return gravity_scale;
}

void RigidBody::set_linear_damp(real_t p_linear_damp) {

    ERR_FAIL_COND(p_linear_damp < -1);
    linear_damp = p_linear_damp;
    PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_LINEAR_DAMP, linear_damp);
}
real_t RigidBody::get_linear_damp() const {

    return linear_damp;
}

void RigidBody::set_angular_damp(real_t p_angular_damp) {

    ERR_FAIL_COND(p_angular_damp < -1);
    angular_damp = p_angular_damp;
    PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP, angular_damp);
}
real_t RigidBody::get_angular_damp() const {

    return angular_damp;
}

void RigidBody::set_axis_velocity(const Vector3 &p_axis) {

    Vector3 v = state ? state->get_linear_velocity() : linear_velocity;
    Vector3 axis = p_axis.normalized();
    v -= axis * axis.dot(v);
    v += p_axis;
    if (state) {
        set_linear_velocity(v);
    } else {
        PhysicsServer3D::get_singleton()->body_set_axis_velocity(get_rid(), p_axis);
        linear_velocity = v;
    }
}

void RigidBody::set_linear_velocity(const Vector3 &p_velocity) {

    linear_velocity = p_velocity;
    if (state)
        state->set_linear_velocity(linear_velocity);
    else
        PhysicsServer3D::get_singleton()->body_set_state(
                get_rid(), PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY, linear_velocity);
}

Vector3 RigidBody::get_linear_velocity() const {

    return linear_velocity;
}

void RigidBody::set_angular_velocity(const Vector3 &p_velocity) {

    angular_velocity = p_velocity;
    if (state)
        state->set_angular_velocity(angular_velocity);
    else
        PhysicsServer3D::get_singleton()->body_set_state(
                get_rid(), PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY, angular_velocity);
}
Vector3 RigidBody::get_angular_velocity() const {

    return angular_velocity;
}

Basis RigidBody::get_inverse_inertia_tensor() {
    return inverse_inertia_tensor;
}

void RigidBody::set_use_custom_integrator(bool p_enable) {

    if (custom_integrator == p_enable)
        return;

    custom_integrator = p_enable;
    PhysicsServer3D::get_singleton()->body_set_omit_force_integration(get_rid(), p_enable);
}
bool RigidBody::is_using_custom_integrator() {

    return custom_integrator;
}

void RigidBody::set_sleeping(bool p_sleeping) {

    sleeping = p_sleeping;
    PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_SLEEPING, sleeping);
}

void RigidBody::set_can_sleep(bool p_active) {

    can_sleep = p_active;
    PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_CAN_SLEEP, p_active);
}

bool RigidBody::is_able_to_sleep() const {

    return can_sleep;
}

bool RigidBody::is_sleeping() const {

    return sleeping;
}

void RigidBody::set_max_contacts_reported(int p_amount) {

    max_contacts_reported = p_amount;
    PhysicsServer3D::get_singleton()->body_set_max_contacts_reported(get_rid(), p_amount);
}

int RigidBody::get_max_contacts_reported() const {

    return max_contacts_reported;
}

void RigidBody::add_central_force(const Vector3 &p_force) {
    PhysicsServer3D::get_singleton()->body_add_central_force(get_rid(), p_force);
}

void RigidBody::add_force(const Vector3 &p_force, const Vector3 &p_pos) {
    PhysicsServer3D::get_singleton()->body_add_force(get_rid(), p_force, p_pos);
}

void RigidBody::add_torque(const Vector3 &p_torque) {
    PhysicsServer3D::get_singleton()->body_add_torque(get_rid(), p_torque);
}

void RigidBody::apply_central_impulse(const Vector3 &p_impulse) {
    PhysicsServer3D::get_singleton()->body_apply_central_impulse(get_rid(), p_impulse);
}

void RigidBody::apply_impulse(const Vector3 &p_pos, const Vector3 &p_impulse) {

    PhysicsServer3D::get_singleton()->body_apply_impulse(get_rid(), p_pos, p_impulse);
}

void RigidBody::apply_torque_impulse(const Vector3 &p_impulse) {
    PhysicsServer3D::get_singleton()->body_apply_torque_impulse(get_rid(), p_impulse);
}

void RigidBody::set_use_continuous_collision_detection(bool p_enable) {

    ccd = p_enable;
    PhysicsServer3D::get_singleton()->body_set_enable_continuous_collision_detection(get_rid(), p_enable);
}

bool RigidBody::is_using_continuous_collision_detection() const {

    return ccd;
}

void RigidBody::set_contact_monitor(bool p_enabled) {

    if (p_enabled == is_contact_monitor_enabled())
        return;

    if (!p_enabled) {

        ERR_FAIL_COND_MSG(contact_monitor->locked, "Can't disable contact monitoring during in/out callback. Use "
                                                   "call_deferred(\"set_contact_monitor\", false) instead.");

        for (eastl::pair<const GameEntity,BodyState> &E : contact_monitor->body_map) {

            //clean up mess
            Object *obj = object_for_entity(E.first);
            Node *node = object_cast<Node>(obj);

            if (node) {
                node->disconnect_all(SceneStringNames::tree_entered, get_instance_id());
                node->disconnect_all(SceneStringNames::tree_exiting, get_instance_id());
            }
        }

        memdelete(contact_monitor);
        contact_monitor = nullptr;
    } else {

        contact_monitor = memnew(ContactMonitor);
        contact_monitor->locked = false;
    }
}

bool RigidBody::is_contact_monitor_enabled() const {

    return contact_monitor != nullptr;
}

void RigidBody::set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_lock) {
    PhysicsServer3D::get_singleton()->body_set_axis_lock(get_rid(), p_axis, p_lock);
}

bool RigidBody::get_axis_lock(PhysicsServer3D::BodyAxis p_axis) const {
    return PhysicsServer3D::get_singleton()->body_is_axis_locked(get_rid(), p_axis);
}

Array RigidBody::get_colliding_bodies() const {

    ERR_FAIL_COND_V(!contact_monitor, Array());

    Array ret;
    ret.resize(contact_monitor->body_map.size());
    int idx = 0;
    for (const eastl::pair<const GameEntity,BodyState> &E : contact_monitor->body_map) {
        Object *obj = object_for_entity(E.first);
        if (!obj) {
            ret.resize(ret.size() - 1); //ops
        } else {
            ret[idx++] = Variant(obj);
        }
    }

    return ret;
}

String RigidBody::get_configuration_warning() const {

    Transform t = get_transform();

    String warning(CollisionObject3D::get_configuration_warning());

    if ((get_mode() == MODE_RIGID || get_mode() == MODE_CHARACTER) &&
            (ABS(t.basis.get_axis(0).length() - 1.0) > 0.05 || ABS(t.basis.get_axis(1).length() - 1.0) > 0.05 ||
                    ABS(t.basis.get_axis(2).length() - 1.0) > 0.05)) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("Size changes to RigidBody (in character or rigid modes) will be overridden by the physics "
                       "engine when running.\nChange the size in children collision shapes instead.");
    }

    return warning;
}

void RigidBody::_bind_methods() {

    SE_BIND_METHOD(RigidBody,set_mode);
    SE_BIND_METHOD(RigidBody,get_mode);

    SE_BIND_METHOD(RigidBody,set_mass);
    SE_BIND_METHOD(RigidBody,get_mass);

    SE_BIND_METHOD(RigidBody,set_weight);
    SE_BIND_METHOD(RigidBody,get_weight);

    MethodBinder::bind_method(D_METHOD("set_physics_material_override", { "physics_material_override" }),
            &RigidBody::set_physics_material_override);
    SE_BIND_METHOD(RigidBody,get_physics_material_override);

    SE_BIND_METHOD(RigidBody,set_linear_velocity);
    SE_BIND_METHOD(RigidBody,get_linear_velocity);

    MethodBinder::bind_method(
            D_METHOD("set_angular_velocity", { "angular_velocity" }), &RigidBody::set_angular_velocity);
    SE_BIND_METHOD(RigidBody,get_angular_velocity);

    SE_BIND_METHOD(RigidBody,get_inverse_inertia_tensor);

    SE_BIND_METHOD(RigidBody,set_gravity_scale);
    SE_BIND_METHOD(RigidBody,get_gravity_scale);

    SE_BIND_METHOD(RigidBody,set_linear_damp);
    SE_BIND_METHOD(RigidBody,get_linear_damp);

    SE_BIND_METHOD(RigidBody,set_angular_damp);
    SE_BIND_METHOD(RigidBody,get_angular_damp);

    MethodBinder::bind_method(
            D_METHOD("set_max_contacts_reported", { "amount" }), &RigidBody::set_max_contacts_reported);
    SE_BIND_METHOD(RigidBody,get_max_contacts_reported);

    MethodBinder::bind_method(
            D_METHOD("set_use_custom_integrator", { "enable" }), &RigidBody::set_use_custom_integrator);
    SE_BIND_METHOD(RigidBody,is_using_custom_integrator);

    SE_BIND_METHOD(RigidBody,set_contact_monitor);
    SE_BIND_METHOD(RigidBody,is_contact_monitor_enabled);

    MethodBinder::bind_method(D_METHOD("set_use_continuous_collision_detection", { "enable" }),
            &RigidBody::set_use_continuous_collision_detection);
    MethodBinder::bind_method(
            D_METHOD("is_using_continuous_collision_detection"), &RigidBody::is_using_continuous_collision_detection);

    SE_BIND_METHOD(RigidBody,set_axis_velocity);

    SE_BIND_METHOD(RigidBody,add_central_force);
    SE_BIND_METHOD(RigidBody,add_force);
    SE_BIND_METHOD(RigidBody,add_torque);

    SE_BIND_METHOD(RigidBody,apply_central_impulse);
    SE_BIND_METHOD(RigidBody,apply_impulse);
    SE_BIND_METHOD(RigidBody,apply_torque_impulse);

    SE_BIND_METHOD(RigidBody,set_sleeping);
    SE_BIND_METHOD(RigidBody,is_sleeping);

    SE_BIND_METHOD(RigidBody,set_can_sleep);
    SE_BIND_METHOD(RigidBody,is_able_to_sleep);

    SE_BIND_METHOD(RigidBody,_direct_state_changed);
    SE_BIND_METHOD(RigidBody,_body_enter_tree);

    SE_BIND_METHOD(RigidBody,set_axis_lock);
    SE_BIND_METHOD(RigidBody,get_axis_lock);

    SE_BIND_METHOD(RigidBody,get_colliding_bodies);

    BIND_VMETHOD(MethodInfo("_integrate_forces",
            PropertyInfo(VariantType::OBJECT, "state", PropertyHint::ResourceType, "PhysicsDirectBodyState3D")));

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "mode", PropertyHint::Enum, "Rigid,Static,Character,Kinematic"),
            "set_mode", "get_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "mass", PropertyHint::ExpRange, "0.01,65535,0.01,or_greater"), "set_mass",
            "get_mass");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "weight", PropertyHint::ExpRange, "0.01,65535,0.01,or_greater",
                         PROPERTY_USAGE_EDITOR),
            "set_weight", "get_weight");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "physics_material_override", PropertyHint::ResourceType,
                         "PhysicsMaterial"),
            "set_physics_material_override", "get_physics_material_override");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "gravity_scale", PropertyHint::Range, "-128,128,0.01"),
            "set_gravity_scale", "get_gravity_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "custom_integrator"), "set_use_custom_integrator",
            "is_using_custom_integrator");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "continuous_cd"), "set_use_continuous_collision_detection",
            "is_using_continuous_collision_detection");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "contacts_reported", PropertyHint::Range, "0,64,1,or_greater"),
            "set_max_contacts_reported", "get_max_contacts_reported");

    ADD_PROPERTY(
            PropertyInfo(VariantType::BOOL, "contact_monitor"), "set_contact_monitor", "is_contact_monitor_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "sleeping"), "set_sleeping", "is_sleeping");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "can_sleep"), "set_can_sleep", "is_able_to_sleep");
    ADD_GROUP("Axis Lock", "axis_lock_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "axis_lock_linear_x"), "set_axis_lock", "get_axis_lock",
            PhysicsServer3D::BODY_AXIS_LINEAR_X);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "axis_lock_linear_y"), "set_axis_lock", "get_axis_lock",
            PhysicsServer3D::BODY_AXIS_LINEAR_Y);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "axis_lock_linear_z"), "set_axis_lock", "get_axis_lock",
            PhysicsServer3D::BODY_AXIS_LINEAR_Z);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "axis_lock_angular_x"), "set_axis_lock", "get_axis_lock",
            PhysicsServer3D::BODY_AXIS_ANGULAR_X);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "axis_lock_angular_y"), "set_axis_lock", "get_axis_lock",
            PhysicsServer3D::BODY_AXIS_ANGULAR_Y);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "axis_lock_angular_z"), "set_axis_lock", "get_axis_lock",
            PhysicsServer3D::BODY_AXIS_ANGULAR_Z);
    ADD_GROUP("Linear", "linear_");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "linear_velocity"), "set_linear_velocity", "get_linear_velocity");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "linear_damp", PropertyHint::Range, "-1,100,0.001,or_greater"),
            "set_linear_damp", "get_linear_damp");
    ADD_GROUP("Angular", "angular_");
    ADD_PROPERTY(
            PropertyInfo(VariantType::VECTOR3, "angular_velocity"), "set_angular_velocity", "get_angular_velocity");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "angular_damp", PropertyHint::Range, "-1,100,0.001,or_greater"),
            "set_angular_damp", "get_angular_damp");

    ADD_SIGNAL(MethodInfo("body_shape_entered", PropertyInfo(VariantType::_RID, "body_rid"),
            PropertyInfo(VariantType::OBJECT, "body", PropertyHint::ResourceType, "Node"),
            PropertyInfo(VariantType::INT, "body_shape_index"), PropertyInfo(VariantType::INT, "local_shape_index")));
    ADD_SIGNAL(MethodInfo("body_shape_exited", PropertyInfo(VariantType::_RID, "body_rid"),
            PropertyInfo(VariantType::OBJECT, "body", PropertyHint::ResourceType, "Node"),
            PropertyInfo(VariantType::INT, "body_shape_index"), PropertyInfo(VariantType::INT, "local_shape_index")));
    ADD_SIGNAL(
            MethodInfo("body_entered", PropertyInfo(VariantType::OBJECT, "body", PropertyHint::ResourceType, "Node")));
    ADD_SIGNAL(
            MethodInfo("body_exited", PropertyInfo(VariantType::OBJECT, "body", PropertyHint::ResourceType, "Node")));
    ADD_SIGNAL(MethodInfo("sleeping_state_changed"));

    BIND_ENUM_CONSTANT(MODE_RIGID);
    BIND_ENUM_CONSTANT(MODE_STATIC);
    BIND_ENUM_CONSTANT(MODE_CHARACTER);
    BIND_ENUM_CONSTANT(MODE_KINEMATIC);
}

RigidBody::RigidBody() : PhysicsBody3D(PhysicsServer3D::BODY_MODE_RIGID) {

    mode = MODE_RIGID;

    mass = 1;
    max_contacts_reported = 0;
    state = nullptr;

    gravity_scale = 1;
    linear_damp = -1;
    angular_damp = -1;

    //angular_velocity=0;
    sleeping = false;
    ccd = false;

    custom_integrator = false;
    contact_monitor = nullptr;
    can_sleep = true;

    PhysicsServer3D::get_singleton()->body_set_force_integration_callback(
            get_rid(), callable_mp(this, &RigidBody::_direct_state_changed));
}

RigidBody::~RigidBody() {
    memdelete(contact_monitor);
}

void RigidBody::_reload_physics_characteristics() {
    if (not physics_material_override) {
        PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_BOUNCE, 0);
        PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_FRICTION, 1);
    } else {
        PhysicsServer3D::get_singleton()->body_set_param(
                get_rid(), PhysicsServer3D::BODY_PARAM_BOUNCE, physics_material_override->computed_bounce());
        PhysicsServer3D::get_singleton()->body_set_param(
                get_rid(), PhysicsServer3D::BODY_PARAM_FRICTION, physics_material_override->computed_friction());
    }
}

//////////////////////////////////////////////////////
//////////////////////////

Ref<KinematicCollision> KinematicBody3D::_move(
        const Vector3 &p_motion, bool p_infinite_inertia, bool p_exclude_raycast_shapes, bool p_test_only) {

    Collision col;
    if (move_and_collide(p_motion, p_infinite_inertia, col, p_exclude_raycast_shapes, p_test_only)) {
        // Create a new instance when the cached reference is invalid or still in use in script.
        if (not motion_cache || motion_cache->reference_get_count() > 1) {
            motion_cache = make_ref_counted<KinematicCollision>();
            motion_cache->owner = this;
        }

        motion_cache->collision = col;

        return motion_cache;
    }

    return Ref<KinematicCollision>();
}

bool KinematicBody3D::move_and_collide(const Vector3 &p_motion, bool p_infinite_inertia, Collision &r_collision, bool p_exclude_raycast_shapes, bool p_test_only, bool p_cancel_sliding, const Set<RID> &p_exclude) {
    if (sync_to_physics) {
        ERR_PRINT("Functions move_and_slide and move_and_collide do not work together with 'sync to physics' option. Please read the documentation.");
    }

    Transform gt = get_global_transform();
    PhysicsServer3D::MotionResult result;
    bool colliding = PhysicsServer3D::get_singleton()->body_test_motion(get_rid(), gt, p_motion, p_infinite_inertia, &result, p_exclude_raycast_shapes, p_exclude);

    // Restore direction of motion to be along original motion,
    // in order to avoid sliding due to recovery,
    // but only if collision depth is low enough to avoid tunneling.
    if (p_cancel_sliding) {
        real_t motion_length = p_motion.length();
        real_t precision = 0.001;

        if (colliding) {
            // Can't just use margin as a threshold because collision depth is calculated on unsafe motion,
            // so even in normal resting cases the depth can be a bit more than the margin.
            precision += motion_length * (result.collision_unsafe_fraction - result.collision_safe_fraction);

            if (result.collision_depth > (real_t)margin + precision) {
                p_cancel_sliding = false;
            }
        }

        if (p_cancel_sliding) {
            // When motion is null, recovery is the resulting motion.
            Vector3 motion_normal;
            if (motion_length > CMP_EPSILON) {
                motion_normal = p_motion / motion_length;
            }

            // Check depth of recovery.
            real_t projected_length = result.motion.dot(motion_normal);
            Vector3 recovery = result.motion - motion_normal * projected_length;
            real_t recovery_length = recovery.length();
            // Fixes cases where canceling slide causes the motion to go too deep into the ground,
            // because we're only taking rest information into account and not general recovery.
            if (recovery_length < (real_t)margin + precision) {
                // Apply adjustment to motion.
                result.motion = motion_normal * projected_length;
                result.remainder = p_motion - result.motion;
            }
        }
    }
    if (colliding) {
        r_collision.collider_metadata = result.collider_metadata;
        r_collision.collider_shape = result.collider_shape;
        r_collision.collider_vel = result.collider_velocity;
        r_collision.collision = result.collision_point;
        r_collision.normal = result.collision_normal;
        r_collision.collider = result.collider_id;
        r_collision.collider_rid = result.collider;
        r_collision.travel = result.motion;
        r_collision.remainder = result.remainder;
        r_collision.local_shape = result.collision_local_shape;
    }

    for (int i = 0; i < 3; i++) {
        if (locked_axis & (1 << i)) {
            result.motion[i] = 0;
        }
    }

    if (!p_test_only) {
        gt.origin += result.motion;
        set_global_transform(gt);
    }

    return colliding;
}

//so, if you pass 45 as limit, avoid numerical precision errors when angle is 45.
static constexpr float FLOOR_ANGLE_THRESHOLD = 0.01f;

Vector3 KinematicBody3D::_move_and_slide_internal(const Vector3 &p_linear_velocity, const Vector3 &p_snap, const Vector3 &p_up_direction, bool p_stop_on_slope, int p_max_slides, float p_floor_max_angle, bool p_infinite_inertia) {

    Vector3 body_velocity = p_linear_velocity;
    Vector3 body_velocity_normal = body_velocity.normalized();
    Vector3 up_direction = p_up_direction.normalized();
    bool was_on_floor = on_floor;

    for (int i = 0; i < 3; i++) {
        if (locked_axis & (1 << i)) {
            body_velocity[i] = 0;
        }
    }

    // Hack in order to work with calling from _process as well as from _physics_process; calling from thread is risky
    float delta = Engine::get_singleton()->is_in_physics_frame() ? get_physics_process_delta_time() : get_process_delta_time();

    Vector3 current_floor_velocity = floor_velocity;
    if (on_floor && on_floor_body.is_valid()) {
        // This approach makes sure there is less delay between the actual body velocity and the one we saved.
        PhysicsDirectBodyState3D *bs = PhysicsServer3D::get_singleton()->body_get_direct_state(on_floor_body);
        if (bs) {
            Transform gt = get_global_transform();
            Vector3 local_position = gt.origin - bs->get_transform().origin;
            current_floor_velocity = bs->get_velocity_at_local_position(local_position);
        } else {
            // Body is removed or destroyed, invalidate floor.
            current_floor_velocity = Vector3();
            on_floor_body = RID();
        }
    }

    colliders.clear();
    on_floor = false;
    on_ceiling = false;
    on_wall = false;
    floor_normal = Vector3();
    floor_velocity = Vector3();

    if (current_floor_velocity != Vector3() && on_floor_body.is_valid()) {
        Collision floor_collision;
        Set<RID> exclude;
        exclude.insert(on_floor_body);
        if (move_and_collide(current_floor_velocity * delta, p_infinite_inertia, floor_collision, true, false, false, exclude)) {
            colliders.push_back(floor_collision);
            _set_collision_direction(floor_collision, up_direction, p_floor_max_angle);
        }
    }

    on_floor_body = RID();
    Vector3 motion = body_velocity * delta;

    // No sliding on first attempt to keep floor motion stable when possible,
    // when stop on slope is enabled.
    bool sliding_enabled = !p_stop_on_slope;
    for (int iteration = 0; iteration < p_max_slides; ++iteration) {
        Collision collision;
        bool found_collision = false;

        for (int i = 0; i < 2; ++i) {
            bool collided;
            if (i == 0) { //collide
                collided = move_and_collide(motion, p_infinite_inertia, collision, true, false, !sliding_enabled);
                if (!collided) {
                    motion = Vector3(); //clear because no collision happened and motion completed
                }
            } else { //separate raycasts (if any)
                collided = separate_raycast_shapes(p_infinite_inertia, collision);
                if (collided) {
                    collision.remainder = motion; //keep
                    collision.travel = Vector3();
                }
            }

            if (collided) {
                found_collision = true;

                colliders.push_back(collision);

                _set_collision_direction(collision, up_direction, p_floor_max_angle);

                if (on_floor && p_stop_on_slope) {
                    if ((body_velocity_normal + up_direction).length() < 0.01) {
                                Transform gt = get_global_transform();
                        if (collision.travel.length() > margin) {
                            gt.origin -= collision.travel.slide(up_direction);
                        } else {
                            gt.origin -= collision.travel;
                        }
                                set_global_transform(gt);
                                return Vector3();
                    }
                }

                if (sliding_enabled || !on_floor) {
                    motion = collision.remainder.slide(collision.normal);
                body_velocity = body_velocity.slide(collision.normal);

                for (int j = 0; j < 3; j++) {
                    if (locked_axis & (1 << j)) {
                        body_velocity[j] = 0;
                    }
                }
                } else {
                    motion = collision.remainder;
            }
        }

            sliding_enabled = true;
    }

        if (!found_collision || motion == Vector3()) {
            break;
}

    }

    if (was_on_floor && p_snap != Vector3() && !on_floor) {
        // Apply snap.
    Collision col;
    Transform gt = get_global_transform();

        if (move_and_collide(p_snap, p_infinite_inertia, col, false, true, false)) {

        bool apply = true;
            if (up_direction != Vector3()) {
                if (Math::acos(col.normal.dot(up_direction)) <= p_floor_max_angle + FLOOR_ANGLE_THRESHOLD) {
                on_floor = true;
                floor_normal = col.normal;
                on_floor_body = col.collider_rid;
                floor_velocity = col.collider_vel;
                if (p_stop_on_slope) {
                    // move and collide may stray the object a bit because of pre un-stucking,
                    // so only ensure that motion happens on floor direction in this case.
                        if (col.travel.length() > margin) {
                            col.travel = col.travel.project(up_direction);
                        } else {
                            col.travel = Vector3();
                        }
                }
            } else {
                apply = false; //snapped with floor direction, but did not snap to a floor, do not snap.
            }
        }
        if (apply) {
            gt.origin += col.travel;
            set_global_transform(gt);
            }
        }
    }

	if (moving_platform_apply_velocity_on_leave != PLATFORM_VEL_ON_LEAVE_NEVER) {
        // Add last platform velocity when just left a moving platform.
        if (!on_floor) {
            if (moving_platform_apply_velocity_on_leave == PLATFORM_VEL_ON_LEAVE_UPWARD_ONLY &&
                    current_floor_velocity.dot(up_direction) < 0) {
                current_floor_velocity = current_floor_velocity.slide(up_direction);
            }
            return body_velocity + current_floor_velocity;
        }
    }

    return body_velocity;
}

Vector3 KinematicBody3D::move_and_slide(const Vector3 &p_linear_velocity, const Vector3 &p_up_direction, bool p_stop_on_slope, int p_max_slides, float p_floor_max_angle, bool p_infinite_inertia) {
    return _move_and_slide_internal(p_linear_velocity, Vector3(), p_up_direction, p_stop_on_slope, p_max_slides, p_floor_max_angle, p_infinite_inertia);
}

Vector3 KinematicBody3D::move_and_slide_with_snap(const Vector3 &p_linear_velocity, const Vector3 &p_snap, const Vector3 &p_up_direction, bool p_stop_on_slope, int p_max_slides, float p_floor_max_angle, bool p_infinite_inertia) {
    return _move_and_slide_internal(p_linear_velocity, p_snap, p_up_direction, p_stop_on_slope, p_max_slides, p_floor_max_angle, p_infinite_inertia);
}

void KinematicBody3D::_set_collision_direction(const Collision &p_collision, const Vector3 &p_up_direction, float p_floor_max_angle) {
    if (p_up_direction == Vector3()) {
        //all is a wall
        on_wall = true;
    } else {
        if (Math::acos(p_collision.normal.dot(p_up_direction)) <= p_floor_max_angle + FLOOR_ANGLE_THRESHOLD) { //floor
            on_floor = true;
            floor_normal = p_collision.normal;
            on_floor_body = p_collision.collider_rid;
            floor_velocity = p_collision.collider_vel;
        } else if (Math::acos(p_collision.normal.dot(-p_up_direction)) <= p_floor_max_angle + FLOOR_ANGLE_THRESHOLD) { //ceiling
            on_ceiling = true;
        } else {
            on_wall = true;
        }
    }
}

bool KinematicBody3D::is_on_floor() const {

    return on_floor;
}

bool KinematicBody3D::is_on_wall() const {

    return on_wall;
}
bool KinematicBody3D::is_on_ceiling() const {

    return on_ceiling;
}
Vector3 KinematicBody3D::get_floor_normal() const {

    return floor_normal;
}

real_t KinematicBody3D::get_floor_angle(const Vector3 &p_up_direction) const {
    ERR_FAIL_COND_V(p_up_direction == Vector3(), 0);
    return Math::acos(floor_normal.dot(p_up_direction));
}

void KinematicBody3D::set_moving_platform_apply_velocity_on_leave(
        MovingPlatformApplyVelocityOnLeave p_on_leave_apply_velocity) {
    moving_platform_apply_velocity_on_leave = p_on_leave_apply_velocity;
}

KinematicBody3D::MovingPlatformApplyVelocityOnLeave
KinematicBody3D::get_moving_platform_apply_velocity_on_leave() const {
    return moving_platform_apply_velocity_on_leave;
}
bool KinematicBody3D::test_move(const Transform &p_from, const Vector3 &p_motion, bool p_infinite_inertia) {

    ERR_FAIL_COND_V(!is_inside_tree(), false);
    PhysicsServer3D::MotionResult result;
    bool colliding = PhysicsServer3D::get_singleton()->body_test_motion(
            get_rid(), p_from, p_motion, p_infinite_inertia, &result);

    if (!colliding) {
        return false;
    }
    // Don't report collision when the whole motion is done.
    return (result.collision_safe_fraction < 1.0f);
}

bool KinematicBody3D::separate_raycast_shapes(bool p_infinite_inertia, Collision &r_collision) {

    PhysicsServer3D::SeparationResult sep_res[8]; //max 8 rays

    Transform gt = get_global_transform();

    Vector3 recover;
    int hits = PhysicsServer3D::get_singleton()->body_test_ray_separation(
            get_rid(), gt, p_infinite_inertia, recover, sep_res, 8, margin);
    int deepest = -1;
    float deepest_depth;
    for (int i = 0; i < hits; i++) {
        if (deepest == -1 || sep_res[i].collision_depth > deepest_depth) {
            deepest = i;
            deepest_depth = sep_res[i].collision_depth;
        }
    }

    gt.origin += recover;
    set_global_transform(gt);

    if (deepest != -1) {
        r_collision.collider = sep_res[deepest].collider_id;
        r_collision.collider_rid = sep_res[deepest].collider;
        r_collision.collider_metadata = sep_res[deepest].collider_metadata;
        r_collision.collider_shape = sep_res[deepest].collider_shape;
        r_collision.collider_vel = sep_res[deepest].collider_velocity;
        r_collision.collision = sep_res[deepest].collision_point;
        r_collision.normal = sep_res[deepest].collision_normal;
        r_collision.local_shape = sep_res[deepest].collision_local_shape;
        r_collision.travel = recover;
        r_collision.remainder = Vector3();

        return true;
    } else {
        return false;
    }
}

void KinematicBody3D::set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_lock) {
    if (p_lock) {
        locked_axis |= p_axis;
    } else {
        locked_axis &= (~p_axis);
    }
    PhysicsServer3D::get_singleton()->body_set_axis_lock(get_rid(), p_axis, p_lock);
}

bool KinematicBody3D::get_axis_lock(PhysicsServer3D::BodyAxis p_axis) const {
    return PhysicsServer3D::get_singleton()->body_is_axis_locked(get_rid(), p_axis);
}

void KinematicBody3D::set_safe_margin(float p_margin) {

    margin = p_margin;
    PhysicsServer3D::get_singleton()->body_set_kinematic_safe_margin(get_rid(), margin);
}

float KinematicBody3D::get_safe_margin() const {

    return margin;
}
int KinematicBody3D::get_slide_count() const {

    return colliders.size();
}

KinematicBody3D::Collision KinematicBody3D::get_slide_collision(int p_bounce) const {
    ERR_FAIL_INDEX_V(p_bounce, colliders.size(), Collision());
    return colliders[p_bounce];
}

Ref<KinematicCollision> KinematicBody3D::_get_slide_collision(int p_bounce) {

    ERR_FAIL_INDEX_V(p_bounce, colliders.size(), Ref<KinematicCollision>());
    if (p_bounce >= slide_colliders.size()) {
        slide_colliders.resize(p_bounce + 1);
    }

    // Create a new instance when the cached reference is invalid or still in use in script.
    if (not slide_colliders[p_bounce] || slide_colliders[p_bounce]->reference_get_count() > 1) {
        slide_colliders[p_bounce] = make_ref_counted<KinematicCollision>();
        slide_colliders[p_bounce]->owner = this;
    }

    slide_colliders[p_bounce]->collision = colliders[p_bounce];
    return slide_colliders[p_bounce];
}

Ref<KinematicCollision> KinematicBody3D::_get_last_slide_collision() {
    if (colliders.empty()) {
        return Ref<KinematicCollision>();
    }
    return _get_slide_collision(colliders.size() - 1);
}

void KinematicBody3D::set_sync_to_physics(bool p_enable) {
    if (sync_to_physics == p_enable) {
        return;
    }
    sync_to_physics = p_enable;

    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    if (p_enable) {
        PhysicsServer3D::get_singleton()->body_set_force_integration_callback(get_rid(), callable_gen(this,[this](Object *p_state) { _direct_state_changed(p_state); }));
        set_only_update_transform_changes(true);
        set_notify_local_transform(true);
    } else {
        PhysicsServer3D::get_singleton()->body_set_force_integration_callback(get_rid(), {});
        set_only_update_transform_changes(false);
        set_notify_local_transform(false);
    }
}

bool KinematicBody3D::is_sync_to_physics_enabled() const {
    return sync_to_physics;
}

void KinematicBody3D::_direct_state_changed(Object *p_state) {
    if (!sync_to_physics) {
        return;
    }

    PhysicsDirectBodyState3D *state = object_cast<PhysicsDirectBodyState3D>(p_state);
    ERR_FAIL_COND_MSG(!state, "Method '_direct_state_changed' must receive a valid PhysicsDirectBodyState object as argument");

    last_valid_transform = state->get_transform();
    set_notify_local_transform(false);
    set_global_transform(last_valid_transform);
    set_notify_local_transform(true);
    _on_transform_changed();
}

void KinematicBody3D::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE) {
        last_valid_transform = get_global_transform();
        // Reset move_and_slide() data.
        on_floor = false;
        on_floor_body = RID();
        on_ceiling = false;
        on_wall = false;
        colliders.clear();
        floor_velocity = Vector3();
    }
    if (p_what == NOTIFICATION_LOCAL_TRANSFORM_CHANGED) {
        //used by sync to physics, send the new transform to the physics
        Transform new_transform = get_global_transform();
        PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_TRANSFORM, new_transform);
        //but then revert changes
        set_notify_local_transform(false);
        set_global_transform(last_valid_transform);
        set_notify_local_transform(true);
        _on_transform_changed();
    }
}

void KinematicBody3D::_bind_methods() {
    MethodBinder::bind_method(
            D_METHOD("move_and_collide", { "rel_vec", "infinite_inertia", "exclude_raycast_shapes", "test_only" }),
            &KinematicBody3D::_move, { DEFVAL(true), DEFVAL(true), DEFVAL(false) });
    MethodBinder::bind_method(D_METHOD("move_and_slide", { "linear_velocity", "up_direction", "stop_on_slope",
                                                                 "max_slides", "floor_max_angle", "infinite_inertia" }),
            &KinematicBody3D::move_and_slide,
            { DEFVAL(Vector3(0, 0, 0)), DEFVAL(false), DEFVAL(4), DEFVAL(Math::deg2rad((float)45)), DEFVAL(true) });
    MethodBinder::bind_method(
            D_METHOD("move_and_slide_with_snap", { "linear_velocity", "snap", "up_direction", "stop_on_slope",
                                                         "max_slides", "floor_max_angle", "infinite_inertia" }),
            &KinematicBody3D::move_and_slide_with_snap,
            { DEFVAL(Vector3(0, 0, 0)), DEFVAL(false), DEFVAL(4), DEFVAL(Math::deg2rad((float)45)), DEFVAL(true) });

    MethodBinder::bind_method(D_METHOD("test_move", { "from", "rel_vec", "infinite_inertia" }),
            &KinematicBody3D::test_move, { DEFVAL(true) });

    SE_BIND_METHOD(KinematicBody3D,is_on_floor);
    SE_BIND_METHOD(KinematicBody3D,is_on_ceiling);
    SE_BIND_METHOD(KinematicBody3D,is_on_wall);
    SE_BIND_METHOD(KinematicBody3D,get_floor_normal);
    MethodBinder::bind_method(D_METHOD("get_floor_angle", {"up_direction"}), &KinematicBody3D::get_floor_angle, {DEFVAL(Vector3(0.0, 1.0, 0.0))});
    SE_BIND_METHOD(KinematicBody3D,get_floor_velocity);

    SE_BIND_METHOD(KinematicBody3D,set_axis_lock);
    SE_BIND_METHOD(KinematicBody3D,get_axis_lock);

    SE_BIND_METHOD(KinematicBody3D,set_safe_margin);
    SE_BIND_METHOD(KinematicBody3D,get_safe_margin);
    MethodBinder::bind_method(D_METHOD("set_moving_platform_apply_velocity_on_leave", {"on_leave_apply_velocity"}),
            &KinematicBody3D::set_moving_platform_apply_velocity_on_leave);
    MethodBinder::bind_method(D_METHOD("get_moving_platform_apply_velocity_on_leave"),
            &KinematicBody3D::get_moving_platform_apply_velocity_on_leave);

    SE_BIND_METHOD(KinematicBody3D,get_slide_count);
    MethodBinder::bind_method(D_METHOD("get_slide_collision", {"slide_idx"}), &KinematicBody3D::_get_slide_collision);
    MethodBinder::bind_method(D_METHOD("get_last_slide_collision"), &KinematicBody3D::_get_last_slide_collision);


    SE_BIND_METHOD(KinematicBody3D,set_sync_to_physics);
    SE_BIND_METHOD(KinematicBody3D,is_sync_to_physics_enabled);

    SE_BIND_METHOD(KinematicBody3D,_direct_state_changed);

    ADD_GROUP("Axis Lock", "axis_lock_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "axis_lock_motion_x"), "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_LINEAR_X);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "axis_lock_motion_y"), "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_LINEAR_Y);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "axis_lock_motion_z"), "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_LINEAR_Z);


    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "move_lock_x", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR),
            "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_LINEAR_X);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "move_lock_y", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR),
            "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_LINEAR_Y);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "move_lock_z", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR),
            "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_LINEAR_Z);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "collision/safe_margin", PropertyHint::Range, "0.001,256,0.001"),
            "set_safe_margin", "get_safe_margin");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "motion/sync_to_physics"), "set_sync_to_physics", "is_sync_to_physics_enabled");

    ADD_GROUP("Moving Platform", "moving_platform");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "moving_platform_apply_velocity_on_leave", PropertyHint::Enum,
                         "Always,Upward Only,Never", PROPERTY_USAGE_DEFAULT),
            "set_moving_platform_apply_velocity_on_leave", "get_moving_platform_apply_velocity_on_leave");

    BIND_ENUM_CONSTANT(PLATFORM_VEL_ON_LEAVE_ALWAYS);
    BIND_ENUM_CONSTANT(PLATFORM_VEL_ON_LEAVE_UPWARD_ONLY);
    BIND_ENUM_CONSTANT(PLATFORM_VEL_ON_LEAVE_NEVER);
}

KinematicBody3D::KinematicBody3D() : PhysicsBody3D(PhysicsServer3D::BODY_MODE_KINEMATIC) {
    set_safe_margin(0.001f);
}
KinematicBody3D::~KinematicBody3D() {

    if (motion_cache) {
        motion_cache->owner = nullptr;
    }

    for (Ref<KinematicCollision> &slide_collider : slide_colliders) {
        if (slide_collider) {
            slide_collider->owner = nullptr;
        }
    }
}
///////////////////////////////////////

Vector3 KinematicCollision::get_position() const {

    return collision.collision;
}
Vector3 KinematicCollision::get_normal() const {
    return collision.normal;
}
Vector3 KinematicCollision::get_travel() const {
    return collision.travel;
}
Vector3 KinematicCollision::get_remainder() const {
    return collision.remainder;
}
real_t KinematicCollision::get_angle(const Vector3 &p_up_direction) const {
    ERR_FAIL_COND_V(p_up_direction == Vector3(), 0);
    return collision.get_angle(p_up_direction);
}
Object *KinematicCollision::get_local_shape() const {
    if (!owner) {
        return nullptr;
    }
    uint32_t ownerid = owner->shape_find_owner(collision.local_shape);
    return owner->shape_owner_get_owner(ownerid);
}

Object *KinematicCollision::get_collider() const {

    if (collision.collider != entt::null) {
        return object_for_entity(collision.collider);
}

    return nullptr;
}
GameEntity KinematicCollision::get_collider_id() const {

    return collision.collider;
}

RID KinematicCollision::get_collider_rid() const {
    return collision.collider_rid;
}

Object *KinematicCollision::get_collider_shape() const {

    Object *collider = get_collider();
    if (collider) {
        CollisionObject3D *obj2d = object_cast<CollisionObject3D>(collider);
        if (obj2d) {
            uint32_t ownerid = obj2d->shape_find_owner(collision.collider_shape);
            return obj2d->shape_owner_get_owner(ownerid);
        }
    }

    return nullptr;
}
int KinematicCollision::get_collider_shape_index() const {

    return collision.collider_shape;
}
Vector3 KinematicCollision::get_collider_velocity() const {

    return collision.collider_vel;
}
Variant KinematicCollision::get_collider_metadata() const {

    return Variant();
}

void KinematicCollision::_bind_methods() {

    SE_BIND_METHOD(KinematicCollision,get_position);
    SE_BIND_METHOD(KinematicCollision,get_normal);
    SE_BIND_METHOD(KinematicCollision,get_travel);
    SE_BIND_METHOD(KinematicCollision,get_remainder);
    MethodBinder::bind_method(D_METHOD("get_angle", {"up_direction"}), &KinematicCollision::get_angle, {DEFVAL(Vector3(0.0, 1.0, 0.0))});
    SE_BIND_METHOD(KinematicCollision,get_local_shape);
    SE_BIND_METHOD(KinematicCollision,get_collider);
    SE_BIND_METHOD(KinematicCollision,get_collider_id);
    SE_BIND_METHOD(KinematicCollision,get_collider_rid);
    SE_BIND_METHOD(KinematicCollision,get_collider_shape);
    SE_BIND_METHOD(KinematicCollision,get_collider_shape_index);
    SE_BIND_METHOD(KinematicCollision,get_collider_velocity);
    SE_BIND_METHOD(KinematicCollision,get_collider_metadata);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "position"), "", "get_position");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "normal"), "", "get_normal");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "travel"), "", "get_travel");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "remainder"), "", "get_remainder");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "local_shape"), "", "get_local_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "collider"), "", "get_collider");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collider_id"), "", "get_collider_id");
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "collider_rid"), "", "get_collider_rid");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "collider_shape"), "", "get_collider_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collider_shape_index"), "", "get_collider_shape_index");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "collider_velocity"), "", "get_collider_velocity");
    ADD_PROPERTY(
            PropertyInfo(VariantType::NIL, "collider_metadata", PropertyHint::None, "", PROPERTY_USAGE_NIL_IS_VARIANT),
            "", "get_collider_metadata");
}

KinematicCollision::KinematicCollision() : owner(nullptr) {
    collision.collider = entt::null;
    collision.collider_shape = 0;
    collision.local_shape = 0;
}

///////////////////////////////////////

bool PhysicalBone3D::JointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
    return false;
}

bool PhysicalBone3D::JointData::_get(const StringName &p_name, Variant &r_ret) const {
    return false;
}

void PhysicalBone3D::JointData::_get_property_list(Vector<PropertyInfo> *p_list) const {}

void PhysicalBone3D::apply_central_impulse(const Vector3 &p_impulse) {
    PhysicsServer3D::get_singleton()->body_apply_central_impulse(get_rid(), p_impulse);
}

void PhysicalBone3D::apply_impulse(const Vector3 &p_pos, const Vector3 &p_impulse) {
    PhysicsServer3D::get_singleton()->body_apply_impulse(get_rid(), p_pos, p_impulse);
}
bool PhysicalBone3D::PinJointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
    using namespace eastl;
    if (JointData::_set(p_name, p_value, j)) {
        return true;
    }

    if ("joint_constraints/bias"_sv == StringView(p_name)) {
        bias = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->pin_joint_set_param(j, PhysicsServer3D::PIN_JOINT_BIAS, bias);

    } else if ("joint_constraints/damping"_sv == StringView(p_name)) {
        damping = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->pin_joint_set_param(j, PhysicsServer3D::PIN_JOINT_DAMPING, damping);

    } else if ("joint_constraints/impulse_clamp"_sv == StringView(p_name)) {
        impulse_clamp = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->pin_joint_set_param(
                    j, PhysicsServer3D::PIN_JOINT_IMPULSE_CLAMP, impulse_clamp);

    } else {
        return false;
    }

    return true;
}

bool PhysicalBone3D::PinJointData::_get(const StringName &p_name, Variant &r_ret) const {
    using namespace eastl;
    if (JointData::_get(p_name, r_ret)) {
        return true;
    }

    if ("joint_constraints/bias"_sv == StringView(p_name)) {
        r_ret = bias;
    } else if ("joint_constraints/damping"_sv == StringView(p_name)) {
        r_ret = damping;
    } else if ("joint_constraints/impulse_clamp"_sv == StringView(p_name)) {
        r_ret = impulse_clamp;
    } else {
        return false;
    }

    return true;
}

void PhysicalBone3D::PinJointData::_get_property_list(Vector<PropertyInfo> *p_list) const {
    JointData::_get_property_list(p_list);

    p_list->push_back(
            PropertyInfo(VariantType::FLOAT, "joint_constraints/bias", PropertyHint::Range, "0.01,0.99,0.01"));
    p_list->push_back(
            PropertyInfo(VariantType::FLOAT, "joint_constraints/damping", PropertyHint::Range, "0.01,8.0,0.01"));
    p_list->push_back(
            PropertyInfo(VariantType::FLOAT, "joint_constraints/impulse_clamp", PropertyHint::Range, "0.0,64.0,0.01"));
}

bool PhysicalBone3D::ConeJointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
    if (JointData::_set(p_name, p_value, j)) {
        return true;
    }

    if (p_name == "joint_constraints/swing_span") {
        swing_span = Math::deg2rad(real_t(p_value.as<float>()));
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(
                    j, PhysicsServer3D::CONE_TWIST_JOINT_SWING_SPAN, swing_span);

    } else if (p_name == "joint_constraints/twist_span") {
        twist_span = Math::deg2rad(real_t(p_value.as<float>()));
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(
                    j, PhysicsServer3D::CONE_TWIST_JOINT_TWIST_SPAN, twist_span);

    } else if (p_name == "joint_constraints/bias") {
        bias = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(
                    j, PhysicsServer3D::CONE_TWIST_JOINT_BIAS, bias);

    } else if (p_name == "joint_constraints/softness") {
        softness = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(
                    j, PhysicsServer3D::CONE_TWIST_JOINT_SOFTNESS, softness);

    } else if (p_name == "joint_constraints/relaxation") {
        relaxation = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(
                    j, PhysicsServer3D::CONE_TWIST_JOINT_RELAXATION, relaxation);

    } else {
        return false;
    }

    return true;
}

bool PhysicalBone3D::ConeJointData::_get(const StringName &p_name, Variant &r_ret) const {

    if (JointData::_get(p_name, r_ret)) {
        return true;
    }

    if (p_name == "joint_constraints/swing_span") {
        r_ret = Math::rad2deg(swing_span);
    } else if (p_name == "joint_constraints/twist_span") {
        r_ret = Math::rad2deg(twist_span);
    } else if (p_name == "joint_constraints/bias") {
        r_ret = bias;
    } else if (p_name == "joint_constraints/softness") {
        r_ret = softness;
    } else if (p_name == "joint_constraints/relaxation") {
        r_ret = relaxation;
    } else {
        return false;
    }

    return true;
}

void PhysicalBone3D::ConeJointData::_get_property_list(Vector<PropertyInfo> *p_list) const {
    JointData::_get_property_list(p_list);

    p_list->push_back(
            PropertyInfo(VariantType::FLOAT, "joint_constraints/swing_span", PropertyHint::Range, "-180,180,0.01"));
    p_list->push_back(PropertyInfo(VariantType::FLOAT, "joint_constraints/twist_span", PropertyHint::Range,
            "-40000,40000,0.1,or_lesser,or_greater"));
    p_list->push_back(
            PropertyInfo(VariantType::FLOAT, "joint_constraints/bias", PropertyHint::Range, "0.01,16.0,0.01"));
    p_list->push_back(
            PropertyInfo(VariantType::FLOAT, "joint_constraints/softness", PropertyHint::Range, "0.01,16.0,0.01"));
    p_list->push_back(
            PropertyInfo(VariantType::FLOAT, "joint_constraints/relaxation", PropertyHint::Range, "0.01,16.0,0.01"));
}

bool PhysicalBone3D::HingeJointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
    if (JointData::_set(p_name, p_value, j)) {
        return true;
    }

    if (p_name == "joint_constraints/angular_limit_enabled") {
        angular_limit_enabled = p_value.as<bool>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->hinge_joint_set_flag(
                    j, PhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT, angular_limit_enabled);

    } else if (p_name == "joint_constraints/angular_limit_upper") {
        angular_limit_upper = Math::deg2rad(real_t(p_value.as<float>()));
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->hinge_joint_set_param(
                    j, PhysicsServer3D::HINGE_JOINT_LIMIT_UPPER, angular_limit_upper);

    } else if (p_name == "joint_constraints/angular_limit_lower") {
        angular_limit_lower = Math::deg2rad(real_t(p_value.as<float>()));
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->hinge_joint_set_param(
                    j, PhysicsServer3D::HINGE_JOINT_LIMIT_LOWER, angular_limit_lower);

    } else if (p_name == "joint_constraints/angular_limit_bias") {
        angular_limit_bias = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->hinge_joint_set_param(
                    j, PhysicsServer3D::HINGE_JOINT_LIMIT_BIAS, angular_limit_bias);

    } else if (p_name == "joint_constraints/angular_limit_softness") {
        angular_limit_softness = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->hinge_joint_set_param(
                    j, PhysicsServer3D::HINGE_JOINT_LIMIT_SOFTNESS, angular_limit_softness);

    } else if (p_name == "joint_constraints/angular_limit_relaxation") {
        angular_limit_relaxation = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->hinge_joint_set_param(
                    j, PhysicsServer3D::HINGE_JOINT_LIMIT_RELAXATION, angular_limit_relaxation);

    } else {
        return false;
    }

    return true;
}

bool PhysicalBone3D::HingeJointData::_get(const StringName &p_name, Variant &r_ret) const {
    if (JointData::_get(p_name, r_ret)) {
        return true;
    }
    if (p_name == "joint_constraints/angular_limit_enabled") {
        r_ret = angular_limit_enabled;
    } else if (p_name == "joint_constraints/angular_limit_upper") {
        r_ret = Math::rad2deg(angular_limit_upper);
    } else if (p_name == "joint_constraints/angular_limit_lower") {
        r_ret = Math::rad2deg(angular_limit_lower);
    } else if (p_name == "joint_constraints/angular_limit_bias") {
        r_ret = angular_limit_bias;
    } else if (p_name == "joint_constraints/angular_limit_softness") {
        r_ret = angular_limit_softness;
    } else if (p_name == "joint_constraints/angular_limit_relaxation") {
        r_ret = angular_limit_relaxation;
    } else {
        return false;
    }

    return true;
}

void PhysicalBone3D::HingeJointData::_get_property_list(Vector<PropertyInfo> *p_list) const {
    JointData::_get_property_list(p_list);

    p_list->push_back(PropertyInfo(VariantType::BOOL, "joint_constraints/angular_limit_enabled"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/angular_limit_upper", PropertyHint::Range, "-180,180,0.01"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/angular_limit_lower", PropertyHint::Range, "-180,180,0.01"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/angular_limit_bias", PropertyHint::Range, "0.01,0.99,0.01"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/angular_limit_softness", PropertyHint::Range, "0.01,16,0.01"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/angular_limit_relaxation", PropertyHint::Range, "0.01,16,0.01"));
}

bool PhysicalBone3D::SliderJointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
    if (JointData::_set(p_name, p_value, j)) {
        return true;
    }

    if (p_name == "joint_constraints/linear_limit_upper") {
        linear_limit_upper = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    j, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_UPPER, linear_limit_upper);

    } else if (p_name == "joint_constraints/linear_limit_lower") {
        linear_limit_lower = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    j, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_LOWER, linear_limit_lower);

    } else if (p_name == "joint_constraints/linear_limit_softness") {
        linear_limit_softness = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    j, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS, linear_limit_softness);

    } else if (p_name == "joint_constraints/linear_limit_restitution") {
        linear_limit_restitution = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    j, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION, linear_limit_restitution);

    } else if (p_name == "joint_constraints/linear_limit_damping") {
        linear_limit_damping = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    j, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_DAMPING, linear_limit_restitution);

    } else if (p_name == "joint_constraints/angular_limit_upper") {
        angular_limit_upper = Math::deg2rad((p_value.as<float>()));
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    j, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_UPPER, angular_limit_upper);

    } else if (p_name == "joint_constraints/angular_limit_lower") {
        angular_limit_lower = Math::deg2rad((p_value.as<float>()));
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    j, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_LOWER, angular_limit_lower);

    } else if (p_name == "joint_constraints/angular_limit_softness") {
        angular_limit_softness = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    j, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS, angular_limit_softness);

    } else if (p_name == "joint_constraints/angular_limit_restitution") {
        angular_limit_restitution = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    j, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS, angular_limit_softness);

    } else if (p_name == "joint_constraints/angular_limit_damping") {
        angular_limit_damping = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    j, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_DAMPING, angular_limit_damping);

    } else {
        return false;
    }

    return true;
}

bool PhysicalBone3D::SliderJointData::_get(const StringName &p_name, Variant &r_ret) const {
    if (JointData::_get(p_name, r_ret)) {
        return true;
    }
    if (p_name == "joint_constraints/linear_limit_upper") {
        r_ret = linear_limit_upper;
    } else if (p_name == "joint_constraints/linear_limit_lower") {
        r_ret = linear_limit_lower;
    } else if (p_name == "joint_constraints/linear_limit_softness") {
        r_ret = linear_limit_softness;
    } else if (p_name == "joint_constraints/linear_limit_restitution") {
        r_ret = linear_limit_restitution;
    } else if (p_name == "joint_constraints/linear_limit_damping") {
        r_ret = linear_limit_damping;
    } else if (p_name == "joint_constraints/angular_limit_upper") {
        r_ret = Math::rad2deg(angular_limit_upper);
    } else if (p_name == "joint_constraints/angular_limit_lower") {
        r_ret = Math::rad2deg(angular_limit_lower);
    } else if (p_name == "joint_constraints/angular_limit_softness") {
        r_ret = angular_limit_softness;
    } else if (p_name == "joint_constraints/angular_limit_restitution") {
        r_ret = angular_limit_restitution;
    } else if (p_name == "joint_constraints/angular_limit_damping") {
        r_ret = angular_limit_damping;
    } else {
        return false;
    }

    return true;
}

void PhysicalBone3D::SliderJointData::_get_property_list(Vector<PropertyInfo> *p_list) const {
    JointData::_get_property_list(p_list);

    p_list->push_back(PropertyInfo(VariantType::FLOAT, "joint_constraints/linear_limit_upper"));
    p_list->push_back(PropertyInfo(VariantType::FLOAT, "joint_constraints/linear_limit_lower"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/linear_limit_softness", PropertyHint::Range, "0.01,16.0,0.01"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/linear_limit_restitution", PropertyHint::Range, "0.01,16.0,0.01"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/linear_limit_damping", PropertyHint::Range, "0,16.0,0.01"));

    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/angular_limit_upper", PropertyHint::Range, "-180,180,0.01"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/angular_limit_lower", PropertyHint::Range, "-180,180,0.01"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/angular_limit_softness", PropertyHint::Range, "0.01,16.0,0.01"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/angular_limit_restitution", PropertyHint::Range, "0.01,16.0,0.01"));
    p_list->push_back(PropertyInfo(
            VariantType::FLOAT, "joint_constraints/angular_limit_damping", PropertyHint::Range, "0,16.0,0.01"));
}

bool PhysicalBone3D::SixDOFJointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
    using namespace eastl;

    if (JointData::_set(p_name, p_value, j)) {
        return true;
    }

    String path(p_name);

    if (!path.starts_with("joint_constraints/")) {
        return false;
    }

    Vector3::Axis axis;
    {
        const StringView axis_s = StringUtils::get_slice(path,'/', 1);
        if (StringView("x") == axis_s) {
            axis = Vector3::AXIS_X;
        } else if (StringView("y") == axis_s) {
            axis = Vector3::AXIS_Y;
        } else if (StringView("z") == axis_s) {
            axis = Vector3::AXIS_Z;
        } else {
            return false;
        }
    }

    StringView var_name = StringUtils::get_slice(path,'/', 2);

    if ("linear_limit_enabled"_sv == var_name) {
        axis_data[axis].linear_limit_enabled = p_value.as<bool>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(j, axis,
                    PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT, axis_data[axis].linear_limit_enabled);

    } else if ("linear_limit_upper"_sv == var_name) {
        axis_data[axis].linear_limit_upper = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_UPPER_LIMIT, axis_data[axis].linear_limit_upper);

    } else if ("linear_limit_lower"_sv == var_name) {
        axis_data[axis].linear_limit_lower = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_LOWER_LIMIT, axis_data[axis].linear_limit_lower);

    } else if ("linear_limit_softness"_sv == var_name) {
        axis_data[axis].linear_limit_softness = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS, axis_data[axis].linear_limit_softness);

    } else if ("linear_spring_enabled"_sv == var_name) {
        axis_data[axis].linear_spring_enabled = p_value.as<bool>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(j, axis,
                    PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING, axis_data[axis].linear_spring_enabled);

    } else if ("linear_spring_stiffness"_sv == var_name) {
        axis_data[axis].linear_spring_stiffness = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis,
                    PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_STIFFNESS, axis_data[axis].linear_spring_stiffness);

    } else if ("linear_spring_damping"_sv == var_name) {
        axis_data[axis].linear_spring_damping = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_DAMPING, axis_data[axis].linear_spring_damping);

    } else if ("linear_equilibrium_point"_sv == var_name) {
        axis_data[axis].linear_equilibrium_point = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis,
                    PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT,
                    axis_data[axis].linear_equilibrium_point);

    } else if ("linear_restitution"_sv == var_name) {
        axis_data[axis].linear_restitution = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_RESTITUTION, axis_data[axis].linear_restitution);

    } else if ("linear_damping"_sv == var_name) {
        axis_data[axis].linear_damping = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_DAMPING, axis_data[axis].linear_damping);

    } else if ("angular_limit_enabled"_sv == var_name) {
        axis_data[axis].angular_limit_enabled = p_value.as<bool>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(j, axis,
                    PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT, axis_data[axis].angular_limit_enabled);

    } else if ("angular_limit_upper"_sv == var_name) {
        axis_data[axis].angular_limit_upper = Math::deg2rad(p_value.as<float>());
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_UPPER_LIMIT, axis_data[axis].angular_limit_upper);

    } else if ("angular_limit_lower"_sv == var_name) {
        axis_data[axis].angular_limit_lower = Math::deg2rad((p_value.as<float>()));
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_LOWER_LIMIT, axis_data[axis].angular_limit_lower);

    } else if ("angular_limit_softness"_sv == var_name) {
        axis_data[axis].angular_limit_softness = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis,
                    PhysicsServer3D::G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS, axis_data[axis].angular_limit_softness);

    } else if ("angular_restitution"_sv == var_name) {
        axis_data[axis].angular_restitution = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_RESTITUTION, axis_data[axis].angular_restitution);

    } else if ("angular_damping"_sv == var_name) {
        axis_data[axis].angular_damping = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_DAMPING, axis_data[axis].angular_damping);

    } else if ("erp"_sv == var_name) {
        axis_data[axis].erp = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(
                    j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_ERP, axis_data[axis].erp);

    } else if ("angular_spring_enabled"_sv == var_name) {
        axis_data[axis].angular_spring_enabled = p_value.as<bool>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(j, axis,
                    PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING, axis_data[axis].angular_spring_enabled);

    } else if ("angular_spring_stiffness"_sv == var_name) {
        axis_data[axis].angular_spring_stiffness = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis,
                    PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS, axis_data[axis].angular_spring_stiffness);

    } else if ("angular_spring_damping"_sv == var_name) {
        axis_data[axis].angular_spring_damping = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis,
                    PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_DAMPING, axis_data[axis].angular_spring_damping);

    } else if ("angular_equilibrium_point"_sv == var_name) {
        axis_data[axis].angular_equilibrium_point = p_value.as<float>();
        if (j.is_valid())
            PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis,
                    PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT,
                    axis_data[axis].angular_equilibrium_point);

    } else {
        return false;
    }

    return true;
}

bool PhysicalBone3D::SixDOFJointData::_get(const StringName &p_name, Variant &r_ret) const {

    using namespace eastl;

    if (JointData::_get(p_name, r_ret)) {
        return true;
    }

    StringView path(p_name);

    if (!path.starts_with("joint_constraints/")) {
        return false;
    }

    int axis;
    {
        const StringView axis_s = StringUtils::get_slice(path,'/', 1);
        if ("x"_sv == axis_s) {
            axis = 0;
        } else if ("y"_sv == axis_s) {
            axis = 1;
        } else if ("z"_sv == axis_s) {
            axis = 2;
        } else {
            return false;
        }
    }

    StringView var_name = StringUtils::get_slice(path,'/', 2);

    if ("linear_limit_enabled"_sv == var_name) {
        r_ret = axis_data[axis].linear_limit_enabled;
    } else if ("linear_limit_upper"_sv == var_name) {
        r_ret = axis_data[axis].linear_limit_upper;
    } else if ("linear_limit_lower"_sv == var_name) {
        r_ret = axis_data[axis].linear_limit_lower;
    } else if ("linear_limit_softness"_sv == var_name) {
        r_ret = axis_data[axis].linear_limit_softness;
    } else if ("linear_spring_enabled"_sv == var_name) {
        r_ret = axis_data[axis].linear_spring_enabled;
    } else if ("linear_spring_stiffness"_sv == var_name) {
        r_ret = axis_data[axis].linear_spring_stiffness;
    } else if ("linear_spring_damping"_sv == var_name) {
        r_ret = axis_data[axis].linear_spring_damping;
    } else if ("linear_equilibrium_point"_sv == var_name) {
        r_ret = axis_data[axis].linear_equilibrium_point;
    } else if ("linear_restitution"_sv == var_name) {
        r_ret = axis_data[axis].linear_restitution;
    } else if ("linear_damping"_sv == var_name) {
        r_ret = axis_data[axis].linear_damping;
    } else if ("angular_limit_enabled"_sv == var_name) {
        r_ret = axis_data[axis].angular_limit_enabled;
    } else if ("angular_limit_upper"_sv == var_name) {
        r_ret = Math::rad2deg(axis_data[axis].angular_limit_upper);
    } else if ("angular_limit_lower"_sv == var_name) {
        r_ret = Math::rad2deg(axis_data[axis].angular_limit_lower);
    } else if ("angular_limit_softness"_sv == var_name) {
        r_ret = axis_data[axis].angular_limit_softness;
    } else if ("angular_restitution"_sv == var_name) {
        r_ret = axis_data[axis].angular_restitution;
    } else if ("angular_damping"_sv == var_name) {
        r_ret = axis_data[axis].angular_damping;
    } else if ("erp"_sv == var_name) {
        r_ret = axis_data[axis].erp;
    } else if ("angular_spring_enabled"_sv == var_name) {
        r_ret = axis_data[axis].angular_spring_enabled;
    } else if ("angular_spring_stiffness"_sv == var_name) {
        r_ret = axis_data[axis].angular_spring_stiffness;
    } else if ("angular_spring_damping"_sv == var_name) {
        r_ret = axis_data[axis].angular_spring_damping;
    } else if ("angular_equilibrium_point"_sv == var_name) {
        r_ret = axis_data[axis].angular_equilibrium_point;
    } else {
        return false;
    }

    return true;
}

void PhysicalBone3D::SixDOFJointData::_get_property_list(Vector<PropertyInfo> *p_list) const {
    const char * axis_names[] = { "x", "y", "z" };
    for (int i = 0; i < 3; ++i) {
        String prefix(String("joint_constraints/") + axis_names[i]);
        p_list->push_back(PropertyInfo(VariantType::BOOL, StringName(prefix + "/linear_limit_enabled")));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/linear_limit_upper")));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/linear_limit_lower")));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/linear_limit_softness"),
                PropertyHint::Range, "0.01,16,0.01"));
        p_list->push_back(PropertyInfo(VariantType::BOOL, StringName(prefix + "/linear_spring_enabled")));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/linear_spring_stiffness")));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/linear_spring_damping")));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/linear_equilibrium_point")));
        p_list->push_back(PropertyInfo(
                VariantType::FLOAT, StringName(prefix + "/linear_restitution"), PropertyHint::Range, "0.01,16,0.01"));
        p_list->push_back(PropertyInfo(
                VariantType::FLOAT, StringName(prefix + "/linear_damping"), PropertyHint::Range, "0.01,16,0.01"));
        p_list->push_back(PropertyInfo(VariantType::BOOL, StringName(prefix + "/angular_limit_enabled")));
        p_list->push_back(PropertyInfo(
                VariantType::FLOAT, StringName(prefix + "/angular_limit_upper"), PropertyHint::Range, "-180,180,0.01"));
        p_list->push_back(PropertyInfo(
                VariantType::FLOAT, StringName(prefix + "/angular_limit_lower"), PropertyHint::Range, "-180,180,0.01"));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/angular_limit_softness"),
                PropertyHint::Range, "0.01,16,0.01"));
        p_list->push_back(PropertyInfo(
                VariantType::FLOAT, StringName(prefix + "/angular_restitution"), PropertyHint::Range, "0.01,16,0.01"));
        p_list->push_back(PropertyInfo(
                VariantType::FLOAT, StringName(prefix + "/angular_damping"), PropertyHint::Range, "0.01,16,0.01"));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/erp")));
        p_list->push_back(PropertyInfo(VariantType::BOOL, StringName(prefix + "/angular_spring_enabled")));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/angular_spring_stiffness")));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/angular_spring_damping")));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName(prefix + "/angular_equilibrium_point")));
    }
}

bool PhysicalBone3D::_set(const StringName &p_name, const Variant &p_value) {
    if (p_name == "bone_name") {
        set_bone_name(p_value.as<String>());
        return true;
    }

    if (joint_data) {
        if (joint_data->_set(p_name, p_value, joint)) {
#ifdef TOOLS_ENABLED
            if (get_gizmo())
                get_gizmo()->redraw();
#endif
            return true;
        }
    }

    return false;
}

bool PhysicalBone3D::_get(const StringName &p_name, Variant &r_ret) const {
    if (p_name == "bone_name") {
        r_ret = get_bone_name();
        return true;
    }

    if (joint_data) {
        return joint_data->_get(p_name, r_ret);
    }

    return false;
}

void PhysicalBone3D::_get_property_list(Vector<PropertyInfo> *p_list) const {

    Skeleton *parent = find_skeleton_parent(get_parent());

    if (parent) {

        String names;
        for (int i = 0; i < parent->get_bone_count(); i++) {
            if (i > 0)
                names += ',';
            names += parent->get_bone_name(i);
        }

        p_list->push_back(PropertyInfo(VariantType::STRING_NAME, "bone_name", PropertyHint::Enum, names));
    } else {

        p_list->push_back(PropertyInfo(VariantType::STRING_NAME, "bone_name"));
    }

    if (joint_data) {
        joint_data->_get_property_list(p_list);
    }
}

void PhysicalBone3D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE:
            parent_skeleton = find_skeleton_parent(get_parent());
            update_bone_id();
            reset_to_rest_position();
            _reset_physics_simulation_state();
            if (!joint.is_valid() && joint_data) {
                _reload_joint();
            }
            break;
        case NOTIFICATION_EXIT_TREE:
            if (parent_skeleton) {
                if (-1 != bone_id) {
                    parent_skeleton->unbind_physical_bone_from_bone(bone_id);
                    parent_skeleton->unbind_child_node_from_bone(bone_id, this);
                    bone_id = -1;
                }
            }
            parent_skeleton = nullptr;
            if (joint.is_valid()) {
                PhysicsServer3D::get_singleton()->free_rid(joint);
                joint = RID();
            }
            break;
        case NOTIFICATION_TRANSFORM_CHANGED:
            if (Engine::get_singleton()->is_editor_hint()) {

                update_offset();
            }
            break;
    }
}

void PhysicalBone3D::_direct_state_changed(Object *p_state) {

    if (!simulate_physics || !_internal_simulate_physics) {
        return;
    }

    /// Update bone transform

    PhysicsDirectBodyState3D *state;

    state = object_cast<PhysicsDirectBodyState3D>(p_state);
    ERR_FAIL_COND_MSG(
            !state, "Method '_direct_state_changed' must receive a valid PhysicsDirectBodyState object as argument");

    Transform global_transform(state->get_transform());

    set_ignore_transform_notification(true);
    set_global_transform(global_transform);
    set_ignore_transform_notification(false);
    _on_transform_changed();

    // Update skeleton
    if (parent_skeleton) {
        if (-1 != bone_id) {
            parent_skeleton->set_bone_global_pose_override(bone_id,
                    parent_skeleton->get_global_transform().affine_inverse() * (global_transform * body_offset_inverse),
                    1.0, true);
        }
    }
}

void PhysicalBone3D::_bind_methods() {
    SE_BIND_METHOD(PhysicalBone3D,apply_central_impulse);
    SE_BIND_METHOD(PhysicalBone3D,apply_impulse);

    SE_BIND_METHOD(PhysicalBone3D,_direct_state_changed);

    SE_BIND_METHOD(PhysicalBone3D,set_joint_type);
    SE_BIND_METHOD(PhysicalBone3D,get_joint_type);

    SE_BIND_METHOD(PhysicalBone3D,set_joint_offset);
    SE_BIND_METHOD(PhysicalBone3D,get_joint_offset);

    SE_BIND_METHOD(PhysicalBone3D,set_body_offset);
    SE_BIND_METHOD(PhysicalBone3D,get_body_offset);

    SE_BIND_METHOD(PhysicalBone3D,is_static_body);

    SE_BIND_METHOD(PhysicalBone3D,get_simulate_physics);

    SE_BIND_METHOD(PhysicalBone3D,is_simulating_physics);

    SE_BIND_METHOD(PhysicalBone3D,get_bone_id);

    SE_BIND_METHOD(PhysicalBone3D,set_mass);
    SE_BIND_METHOD(PhysicalBone3D,get_mass);

    SE_BIND_METHOD(PhysicalBone3D,set_weight);
    SE_BIND_METHOD(PhysicalBone3D,get_weight);

    SE_BIND_METHOD(PhysicalBone3D,set_friction);
    SE_BIND_METHOD(PhysicalBone3D,get_friction);

    SE_BIND_METHOD(PhysicalBone3D,set_bounce);
    SE_BIND_METHOD(PhysicalBone3D,get_bounce);

    SE_BIND_METHOD(PhysicalBone3D,set_gravity_scale);
    SE_BIND_METHOD(PhysicalBone3D,get_gravity_scale);

    ADD_GROUP("Joint3D", "joint_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "joint_type", PropertyHint::Enum,
                         "None,PinJoint3D,ConeJoint,HingeJoint3D,SliderJoint3D,6DOFJoint"),
            "set_joint_type", "get_joint_type");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM, "joint_offset"), "set_joint_offset", "get_joint_offset");

    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM, "body_offset"), "set_body_offset", "get_body_offset");

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "mass", PropertyHint::ExpRange, "0.01,65535,0.01,or_greater"), "set_mass",
            "get_mass");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "weight", PropertyHint::ExpRange, "0.01,65535,0.01,or_greater"), "set_weight",
            "get_weight");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "friction", PropertyHint::Range, "0,1,0.01,or_greater"),
            "set_friction", "get_friction");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "bounce", PropertyHint::Range, "0,1,0.01,or_greater"), "set_bounce",
            "get_bounce");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "gravity_scale", PropertyHint::Range, "-10,10,0.01"),
            "set_gravity_scale", "get_gravity_scale");

    BIND_ENUM_CONSTANT(JOINT_TYPE_NONE);
    BIND_ENUM_CONSTANT(JOINT_TYPE_PIN);
    BIND_ENUM_CONSTANT(JOINT_TYPE_CONE);
    BIND_ENUM_CONSTANT(JOINT_TYPE_HINGE);
    BIND_ENUM_CONSTANT(JOINT_TYPE_SLIDER);
    BIND_ENUM_CONSTANT(JOINT_TYPE_6DOF);
}

Skeleton *PhysicalBone3D::find_skeleton_parent(Node *p_parent) {
    if (!p_parent) {
        return nullptr;
    }
    Skeleton *s = object_cast<Skeleton>(p_parent);
    return s ? s : find_skeleton_parent(p_parent->get_parent());
}

void PhysicalBone3D::_fix_joint_offset() {
    // Clamp joint origin to bone origin
    if (parent_skeleton) {
        joint_offset.origin = body_offset.affine_inverse().origin;
    }
}

void PhysicalBone3D::_reload_joint() {

    if (joint.is_valid()) {
        PhysicsServer3D::get_singleton()->free_rid(joint);
        joint = RID();
    }

    if (!parent_skeleton) {
        return;
    }

    PhysicalBone3D *body_a = parent_skeleton->get_physical_bone_parent(bone_id);
    if (!body_a) {
        return;
    }

    Transform joint_transf = get_global_transform() * joint_offset;
    Transform local_a = body_a->get_global_transform().affine_inverse() * joint_transf;
    local_a.orthonormalize();

    switch (get_joint_type()) {
        case JOINT_TYPE_PIN: {

            joint = PhysicsServer3D::get_singleton()->joint_create_pin(
                    body_a->get_rid(), local_a.origin, get_rid(), joint_offset.origin);
            const PinJointData *pjd(static_cast<const PinJointData *>(joint_data));
            PhysicsServer3D::get_singleton()->pin_joint_set_param(joint, PhysicsServer3D::PIN_JOINT_BIAS, pjd->bias);
            PhysicsServer3D::get_singleton()->pin_joint_set_param(
                    joint, PhysicsServer3D::PIN_JOINT_DAMPING, pjd->damping);
            PhysicsServer3D::get_singleton()->pin_joint_set_param(
                    joint, PhysicsServer3D::PIN_JOINT_IMPULSE_CLAMP, pjd->impulse_clamp);

        } break;
        case JOINT_TYPE_CONE: {

            joint = PhysicsServer3D::get_singleton()->joint_create_cone_twist(
                    body_a->get_rid(), local_a, get_rid(), joint_offset);
            const ConeJointData *cjd(static_cast<const ConeJointData *>(joint_data));
            PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(
                    joint, PhysicsServer3D::CONE_TWIST_JOINT_SWING_SPAN, cjd->swing_span);
            PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(
                    joint, PhysicsServer3D::CONE_TWIST_JOINT_TWIST_SPAN, cjd->twist_span);
            PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(
                    joint, PhysicsServer3D::CONE_TWIST_JOINT_BIAS, cjd->bias);
            PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(
                    joint, PhysicsServer3D::CONE_TWIST_JOINT_SOFTNESS, cjd->softness);
            PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(
                    joint, PhysicsServer3D::CONE_TWIST_JOINT_RELAXATION, cjd->relaxation);

        } break;
        case JOINT_TYPE_HINGE: {

            joint = PhysicsServer3D::get_singleton()->joint_create_hinge(
                    body_a->get_rid(), local_a, get_rid(), joint_offset);
            const HingeJointData *hjd(static_cast<const HingeJointData *>(joint_data));
            PhysicsServer3D::get_singleton()->hinge_joint_set_flag(
                    joint, PhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT, hjd->angular_limit_enabled);
            PhysicsServer3D::get_singleton()->hinge_joint_set_param(
                    joint, PhysicsServer3D::HINGE_JOINT_LIMIT_UPPER, hjd->angular_limit_upper);
            PhysicsServer3D::get_singleton()->hinge_joint_set_param(
                    joint, PhysicsServer3D::HINGE_JOINT_LIMIT_LOWER, hjd->angular_limit_lower);
            PhysicsServer3D::get_singleton()->hinge_joint_set_param(
                    joint, PhysicsServer3D::HINGE_JOINT_LIMIT_BIAS, hjd->angular_limit_bias);
            PhysicsServer3D::get_singleton()->hinge_joint_set_param(
                    joint, PhysicsServer3D::HINGE_JOINT_LIMIT_SOFTNESS, hjd->angular_limit_softness);
            PhysicsServer3D::get_singleton()->hinge_joint_set_param(
                    joint, PhysicsServer3D::HINGE_JOINT_LIMIT_RELAXATION, hjd->angular_limit_relaxation);

        } break;
        case JOINT_TYPE_SLIDER: {

            joint = PhysicsServer3D::get_singleton()->joint_create_slider(
                    body_a->get_rid(), local_a, get_rid(), joint_offset);
            const SliderJointData *sjd(static_cast<const SliderJointData *>(joint_data));
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    joint, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_UPPER, sjd->linear_limit_upper);
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    joint, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_LOWER, sjd->linear_limit_lower);
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    joint, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS, sjd->linear_limit_softness);
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    joint, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION, sjd->linear_limit_restitution);
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    joint, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_DAMPING, sjd->linear_limit_restitution);
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    joint, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_UPPER, sjd->angular_limit_upper);
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    joint, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_LOWER, sjd->angular_limit_lower);
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    joint, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS, sjd->angular_limit_softness);
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    joint, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS, sjd->angular_limit_softness);
            PhysicsServer3D::get_singleton()->slider_joint_set_param(
                    joint, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_DAMPING, sjd->angular_limit_damping);

        } break;
        case JOINT_TYPE_6DOF: {

            joint = PhysicsServer3D::get_singleton()->joint_create_generic_6dof(
                    body_a->get_rid(), local_a, get_rid(), joint_offset);
            const SixDOFJointData *g6dofjd(static_cast<const SixDOFJointData *>(joint_data));
            for (int axis = 0; axis < 3; ++axis) {
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT,
                        g6dofjd->axis_data[axis].linear_limit_enabled);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_LINEAR_UPPER_LIMIT, g6dofjd->axis_data[axis].linear_limit_upper);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_LINEAR_LOWER_LIMIT, g6dofjd->axis_data[axis].linear_limit_lower);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS,
                        g6dofjd->axis_data[axis].linear_limit_softness);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING,
                        g6dofjd->axis_data[axis].linear_spring_enabled);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_STIFFNESS,
                        g6dofjd->axis_data[axis].linear_spring_stiffness);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_DAMPING,
                        g6dofjd->axis_data[axis].linear_spring_damping);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT,
                        g6dofjd->axis_data[axis].linear_equilibrium_point);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_LINEAR_RESTITUTION, g6dofjd->axis_data[axis].linear_restitution);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_LINEAR_DAMPING, g6dofjd->axis_data[axis].linear_damping);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT,
                        g6dofjd->axis_data[axis].angular_limit_enabled);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_ANGULAR_UPPER_LIMIT, g6dofjd->axis_data[axis].angular_limit_upper);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_ANGULAR_LOWER_LIMIT, g6dofjd->axis_data[axis].angular_limit_lower);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS,
                        g6dofjd->axis_data[axis].angular_limit_softness);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_ANGULAR_RESTITUTION, g6dofjd->axis_data[axis].angular_restitution);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_ANGULAR_DAMPING, g6dofjd->axis_data[axis].angular_damping);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_ANGULAR_ERP, g6dofjd->axis_data[axis].erp);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING,
                        g6dofjd->axis_data[axis].angular_spring_enabled);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS,
                        g6dofjd->axis_data[axis].angular_spring_stiffness);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_DAMPING,
                        g6dofjd->axis_data[axis].angular_spring_damping);
                PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis),
                        PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT,
                        g6dofjd->axis_data[axis].angular_equilibrium_point);
            }

        } break;
        case JOINT_TYPE_NONE: {
        } break;
    }
}

void PhysicalBone3D::_on_bone_parent_changed() {
    _reload_joint();
}

void PhysicalBone3D::_set_gizmo_move_joint(bool p_move_joint) {
#ifdef TOOLS_ENABLED
    gizmo_move_joint = p_move_joint;
    Node3DEditor::get_singleton()->update_transform_gizmo();
#endif
}

#ifdef TOOLS_ENABLED
Transform PhysicalBone3D::get_global_gizmo_transform() const {
    return gizmo_move_joint ? get_global_transform() * joint_offset : get_global_transform();
}

Transform PhysicalBone3D::get_local_gizmo_transform() const {
    return gizmo_move_joint ? get_transform() * joint_offset : get_transform();
}
#endif

const PhysicalBone3D::JointData *PhysicalBone3D::get_joint_data() const {
    return joint_data;
}

Skeleton *PhysicalBone3D::find_skeleton_parent() {
    return find_skeleton_parent(this);
}

void PhysicalBone3D::set_joint_type(JointType p_joint_type) {

    if (p_joint_type == get_joint_type())
        return;

    memdelete(joint_data);
    joint_data = nullptr;

    switch (p_joint_type) {
        case JOINT_TYPE_PIN:
            joint_data = memnew(PinJointData);
            break;
        case JOINT_TYPE_CONE:
            joint_data = memnew(ConeJointData);
            break;
        case JOINT_TYPE_HINGE:
            joint_data = memnew(HingeJointData);
            break;
        case JOINT_TYPE_SLIDER:
            joint_data = memnew(SliderJointData);
            break;
        case JOINT_TYPE_6DOF:
            joint_data = memnew(SixDOFJointData);
            break;
        case JOINT_TYPE_NONE:
            break;
    }

    _reload_joint();

    Object_change_notify(this);
#ifdef TOOLS_ENABLED
    if (get_gizmo())
        get_gizmo()->redraw();
#endif
}

PhysicalBone3D::JointType PhysicalBone3D::get_joint_type() const {
    return joint_data ? joint_data->get_joint_type() : JOINT_TYPE_NONE;
}

void PhysicalBone3D::set_joint_offset(const Transform &p_offset) {
    joint_offset = p_offset;

    _fix_joint_offset();

    set_ignore_transform_notification(true);
    reset_to_rest_position();
    set_ignore_transform_notification(false);

#ifdef TOOLS_ENABLED
    if (get_gizmo())
        get_gizmo()->redraw();
#endif
}

const Transform &PhysicalBone3D::get_body_offset() const {
    return body_offset;
}

void PhysicalBone3D::set_body_offset(const Transform &p_offset) {
    body_offset = p_offset;
    body_offset_inverse = body_offset.affine_inverse();

    _fix_joint_offset();

    set_ignore_transform_notification(true);
    reset_to_rest_position();
    set_ignore_transform_notification(false);

#ifdef TOOLS_ENABLED
    if (get_gizmo())
        get_gizmo()->redraw();
#endif
}

const Transform &PhysicalBone3D::get_joint_offset() const {
    return joint_offset;
}

void PhysicalBone3D::set_static_body(bool p_static) {

    static_body = p_static;

    set_as_top_level(!static_body);

    _reset_physics_simulation_state();
}

bool PhysicalBone3D::is_static_body() {
    return static_body;
}

void PhysicalBone3D::set_simulate_physics(bool p_simulate) {
    if (simulate_physics == p_simulate) {
        return;
    }

    simulate_physics = p_simulate;
    _reset_physics_simulation_state();
}

bool PhysicalBone3D::get_simulate_physics() {
    return simulate_physics;
}

bool PhysicalBone3D::is_simulating_physics() {
    return _internal_simulate_physics && !_internal_static_body;
}

void PhysicalBone3D::set_bone_name(StringView p_name) {

    bone_name = StringName(p_name);
    bone_id = -1;

    update_bone_id();
    reset_to_rest_position();
}

const StringName &PhysicalBone3D::get_bone_name() const {

    return bone_name;
}

void PhysicalBone3D::set_mass(real_t p_mass) {

    ERR_FAIL_COND(p_mass <= 0);
    mass = p_mass;
    PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_MASS, mass);
}

real_t PhysicalBone3D::get_mass() const {

    return mass;
}

void PhysicalBone3D::set_weight(real_t p_weight) {

    set_mass(p_weight / real_t(T_GLOBAL_DEF<float>("physics/3d/default_gravity", 9.8f)));
}

real_t PhysicalBone3D::get_weight() const {

    return mass * real_t(T_GLOBAL_DEF<float>("physics/3d/default_gravity", 9.8f));
}

void PhysicalBone3D::set_friction(real_t p_friction) {

    ERR_FAIL_COND(p_friction < 0 || p_friction > 1);

    friction = p_friction;
    PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_FRICTION, friction);
}

real_t PhysicalBone3D::get_friction() const {

    return friction;
}

void PhysicalBone3D::set_bounce(real_t p_bounce) {

    ERR_FAIL_COND(p_bounce < 0 || p_bounce > 1);

    bounce = p_bounce;
    PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_BOUNCE, bounce);
}

real_t PhysicalBone3D::get_bounce() const {

    return bounce;
}

void PhysicalBone3D::set_gravity_scale(real_t p_gravity_scale) {

    gravity_scale = p_gravity_scale;
    PhysicsServer3D::get_singleton()->body_set_param(
            get_rid(), PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE, gravity_scale);
}

real_t PhysicalBone3D::get_gravity_scale() const {

    return gravity_scale;
}

PhysicalBone3D::PhysicalBone3D() :
        PhysicsBody3D(PhysicsServer3D::BODY_MODE_STATIC),
#ifdef TOOLS_ENABLED
        gizmo_move_joint(false),
#endif
        joint_data(nullptr),
        parent_skeleton(nullptr),
        static_body(false),
        _internal_static_body(false),
        simulate_physics(false),
        _internal_simulate_physics(false),
        bone_id(-1),
        bone_name(""),
        bounce(0),
        mass(1),
        friction(1),
        gravity_scale(1) {

    set_static_body(static_body);
    _reset_physics_simulation_state();
}

PhysicalBone3D::~PhysicalBone3D() {
    memdelete(joint_data);
}

void PhysicalBone3D::update_bone_id() {
    if (!parent_skeleton) {
        return;
    }

    const int new_bone_id = parent_skeleton->find_bone(bone_name);

    if (new_bone_id != bone_id) {
        if (-1 != bone_id) {
            // Assert the unbind from old node
            parent_skeleton->unbind_physical_bone_from_bone(bone_id);
            parent_skeleton->unbind_child_node_from_bone(bone_id, this);
        }

        bone_id = new_bone_id;

        parent_skeleton->bind_physical_bone_to_bone(bone_id, this);

        _fix_joint_offset();
        _internal_static_body = !static_body; // Force staticness reset
        _reset_staticness_state();
    }
}

void PhysicalBone3D::update_offset() {
#ifdef TOOLS_ENABLED
    if (parent_skeleton) {

        Transform bone_transform(parent_skeleton->get_global_transform());
        if (-1 != bone_id)
            bone_transform *= parent_skeleton->get_bone_global_pose(bone_id);

        if (gizmo_move_joint) {
            bone_transform *= body_offset;
            set_joint_offset(bone_transform.affine_inverse() * get_global_transform());
        } else {
            set_body_offset(bone_transform.affine_inverse() * get_global_transform());
        }
    }
#endif
}

void PhysicalBone3D::reset_to_rest_position() {
    if (parent_skeleton) {
        if (-1 == bone_id) {
            set_global_transform(parent_skeleton->get_global_transform() * body_offset);
        } else {
            set_global_transform(parent_skeleton->get_global_transform() *
                                 parent_skeleton->get_bone_global_pose(bone_id) * body_offset);
        }
    }
}

void PhysicalBone3D::_reset_physics_simulation_state() {
    if (simulate_physics && !static_body) {
        _start_physics_simulation();
    } else {
        _stop_physics_simulation();
    }

    _reset_staticness_state();
}

void PhysicalBone3D::_reset_staticness_state() {

    if (parent_skeleton && -1 != bone_id) {
        if (static_body && simulate_physics) { // With this check I'm sure the position of this body is updated only
                                               // when it's necessary

            if (_internal_static_body) {
                return;
            }

            parent_skeleton->bind_child_node_to_bone(bone_id, this);
            _internal_static_body = true;
        } else {

            if (!_internal_static_body) {
                return;
            }

            parent_skeleton->unbind_child_node_from_bone(bone_id, this);
            _internal_static_body = false;
        }
    }
}

void PhysicalBone3D::_start_physics_simulation() {
    if (_internal_simulate_physics || !parent_skeleton) {
        return;
    }
    reset_to_rest_position();
    PhysicsServer3D::get_singleton()->body_set_mode(get_rid(), PhysicsServer3D::BODY_MODE_RIGID);
    PhysicsServer3D::get_singleton()->body_set_collision_layer(get_rid(), get_collision_layer());
    PhysicsServer3D::get_singleton()->body_set_collision_mask(get_rid(), get_collision_mask());
    PhysicsServer3D::get_singleton()->body_set_force_integration_callback(
            get_rid(), callable_mp(this, &PhysicalBone3D::_direct_state_changed));
    _internal_simulate_physics = true;
}

void PhysicalBone3D::_stop_physics_simulation() {
    if (!_internal_simulate_physics || !parent_skeleton) {
        return;
    }
    PhysicsServer3D::get_singleton()->body_set_mode(get_rid(), PhysicsServer3D::BODY_MODE_STATIC);
    PhysicsServer3D::get_singleton()->body_set_collision_layer(get_rid(), 0);
    PhysicsServer3D::get_singleton()->body_set_collision_mask(get_rid(), 0);
    PhysicsServer3D::get_singleton()->body_set_force_integration_callback(get_rid(), {});
    parent_skeleton->set_bone_global_pose_override(bone_id, Transform(), 0.0, false);
    _internal_simulate_physics = false;
}
