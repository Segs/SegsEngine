/*************************************************************************/
/*  visibility_notifier_3d.cpp                                              */
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

#include "visibility_notifier_3d.h"

#include "cull_instance_component.h"
#include "core/ecs_registry.h"
#include "core/engine.h"
#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/physics_body_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/animation/animation_tree.h"
#include "scene/animation/animation_tree_player.h"
#include "scene/scene_string_names.h"
#include "scene/resources/world_3d.h"
#include "servers/rendering/rendering_server_scene.h"

IMPL_GDCLASS(VisibilityNotifier3D)
IMPL_GDCLASS(VisibilityEnabler3D)
VARIANT_ENUM_CAST(VisibilityEnabler3D::Enabler);

void VisibilityNotifier3D::_enter_camera(Camera3D *p_camera) {

    ERR_FAIL_COND(cameras.contains(p_camera));
    cameras.insert(p_camera);
    bool in_gameplay = _in_gameplay;
    if (!Engine::get_singleton()->are_portals_active()) {
        in_gameplay = true;
    }
    if (in_gameplay && (cameras.size() == 1)) {
        emit_signal(SceneStringNames::screen_entered);
        _screen_enter();
    }

    emit_signal(SceneStringNames::camera_entered, Variant(p_camera));
}

void VisibilityNotifier3D::_exit_camera(Camera3D *p_camera) {

    ERR_FAIL_COND(!cameras.contains(p_camera));
    cameras.erase(p_camera);
    bool in_gameplay = _in_gameplay;
    if (!Engine::get_singleton()->are_portals_active()) {
        in_gameplay = true;
    }

    emit_signal(SceneStringNames::camera_exited, Variant(p_camera));
    if (in_gameplay && cameras.empty()) {
        emit_signal(SceneStringNames::screen_exited);

        _screen_exit();
    }
}

void VisibilityNotifier3D::set_aabb(const AABB &p_aabb) {

    if (aabb == p_aabb)
        return;
    aabb = p_aabb;

    if (is_inside_world()) {
        AABB world_aabb = get_global_transform().xform(aabb);
        get_world_3d()->_update_notifier(this, world_aabb);
        _world_aabb_center = world_aabb.get_center();
    }

    Object_change_notify(this,"aabb");
    update_gizmo();
}

AABB VisibilityNotifier3D::get_aabb() const {

    return aabb;
}

void VisibilityNotifier3D::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_WORLD: {

            world = get_world_3d();
            ERR_FAIL_COND(!world);
            AABB world_aabb = get_global_transform().xform(aabb);
            world->_register_notifier(this, world_aabb);
            _world_aabb_center = world_aabb.get_center();
            game_object_registry.registry.emplace_or_replace<CullInstancePortalModeDirty>(get_instance_id());
            //_refresh_portal_mode();
        } break;
        case NOTIFICATION_TRANSFORM_CHANGED: {
            AABB world_aabb = get_global_transform().xform(aabb);

            world->_update_notifier(this, get_global_transform().xform(aabb));
            if (_max_distance_active) {
                _world_aabb_center = world_aabb.get_center();
            }
            if (_cull_instance_rid != entt::null) {
                RoomAPI::ghost_update(_cull_instance_rid, world_aabb);
            }
        } break;
        case NOTIFICATION_EXIT_WORLD: {

            ERR_FAIL_COND(!world);
            world->_remove_notifier(this);
        } break;
        case NOTIFICATION_ENTER_GAMEPLAY: {
            _in_gameplay = true;
            if (cameras.size() && Engine::get_singleton()->are_portals_active()) {
                emit_signal(SceneStringNames::screen_entered);
                _screen_enter();
            }
        } break;
        case NOTIFICATION_EXIT_GAMEPLAY: {
            _in_gameplay = false;
            if (cameras.size() && Engine::get_singleton()->are_portals_active()) {
                emit_signal(SceneStringNames::screen_exited);
                _screen_exit();
            }
        } break;
    }
}

bool VisibilityNotifier3D::is_on_screen() const {

    return !cameras.empty();
}

void VisibilityNotifier3D::set_max_distance(real_t p_max_distance) {
    if (p_max_distance > CMP_EPSILON) {
        _max_distance = p_max_distance;
        _max_distance_squared = _max_distance * _max_distance;
        _max_distance_active = true;

        // make sure world aabb centre is up to date
        if (is_inside_world()) {
            AABB world_aabb = get_global_transform().xform(aabb);
            _world_aabb_center = world_aabb.get_center();
        }
    } else {
        _max_distance = 0.0;
        _max_distance_squared = 0.0;
        _max_distance_active = false;
    }
}

void VisibilityNotifier3D::_bind_methods() {

    SE_BIND_METHOD(VisibilityNotifier3D,set_aabb);
    SE_BIND_METHOD(VisibilityNotifier3D,get_aabb);
    SE_BIND_METHOD(VisibilityNotifier3D,is_on_screen);

    ADD_PROPERTY(PropertyInfo(VariantType::AABB, "aabb"), "set_aabb", "get_aabb");

    ADD_SIGNAL(MethodInfo("camera_entered", PropertyInfo(VariantType::OBJECT, "camera", PropertyHint::ResourceType, "Camera3D")));
    ADD_SIGNAL(MethodInfo("camera_exited", PropertyInfo(VariantType::OBJECT, "camera", PropertyHint::ResourceType, "Camera3D")));
    ADD_SIGNAL(MethodInfo("screen_entered"));
    ADD_SIGNAL(MethodInfo("screen_exited"));
}

VisibilityNotifier3D::VisibilityNotifier3D() {
    game_object_registry.registry.emplace<CullInstanceComponent>(get_instance_id());

    set_notify_transform(true);
}

VisibilityNotifier3D::~VisibilityNotifier3D() {
    game_object_registry.registry.remove<CullInstanceComponent>(get_instance_id());
    if (_cull_instance_rid != entt::null) {
        RenderingServer::get_singleton()->free_rid(_cull_instance_rid);
    }
}

//////////////////////////////////////

void VisibilityEnabler3D::_screen_enter() {

    for (eastl::pair< Node *const,Variant> &E : nodes) {

        _change_node_state(E.first, true);
    }

    visible = true;
}

void VisibilityEnabler3D::_screen_exit() {

    for (eastl::pair<Node *const,Variant> &E : nodes) {

        _change_node_state(E.first, false);
    }

    visible = false;
}

void VisibilityEnabler3D::_find_nodes(Node *p_node) {

    bool add = false;
    Variant meta;

    {

        RigidBody *rb = object_cast<RigidBody>(p_node);
        if (rb && ((rb->get_mode() == RigidBody::MODE_CHARACTER || rb->get_mode() == RigidBody::MODE_RIGID))) {

            add = true;
            meta = rb->get_mode();
        }
    }

    if (object_cast<AnimationPlayer>(p_node) || object_cast<AnimationTree>(p_node) || object_cast<AnimationTreePlayer>(p_node)) {
            add = true;
    }

    if (add) {
        p_node->connect(SceneStringNames::tree_exiting, callable_gen(this, [=]() { _node_removed(p_node); }),
                ObjectNS::CONNECT_ONESHOT);
        nodes[p_node] = meta;
        _change_node_state(p_node, false);
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {
        Node *c = p_node->get_child(i);
        if (!c->get_filename().empty())
            continue; //skip, instance

        _find_nodes(c);
    }
}

void VisibilityEnabler3D::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {

        if (Engine::get_singleton()->is_editor_hint())
            return;

        Node *from = this;
        //find where current scene starts
        while (from->get_parent() && from->get_filename().empty())
            from = from->get_parent();

        _find_nodes(from);
    }

    if (p_what == NOTIFICATION_EXIT_TREE) {

        if (Engine::get_singleton()->is_editor_hint())
            return;

        for (eastl::pair< Node * const,Variant> &E : nodes) {

            if (!visible)
                _change_node_state(E.first, true);
            E.first->disconnect_all(SceneStringNames::tree_exiting, get_instance_id());
        }

        nodes.clear();
    }
}

void VisibilityEnabler3D::_change_node_state(Node *p_node, bool p_enabled) {

    ERR_FAIL_COND(!nodes.contains(p_node));

    if (enabler[ENABLER_FREEZE_BODIES]) {
        RigidBody *rb = object_cast<RigidBody>(p_node);
        if (rb)

            rb->set_sleeping(!p_enabled);
    }

    if (enabler[ENABLER_PAUSE_ANIMATIONS]) {
        if (AnimationPlayer *ap = object_cast<AnimationPlayer>(p_node)) {

            ap->set_active(p_enabled);
        } else if (AnimationTree *at = object_cast<AnimationTree>(p_node)) {
            at->set_active(p_enabled);
        } else if (AnimationTreePlayer *atp = object_cast<AnimationTreePlayer>(p_node)) {
            atp->set_active(p_enabled);
        }
    }
}

void VisibilityEnabler3D::_node_removed(Node *p_node) {

    if (!visible)
        _change_node_state(p_node, true);
    nodes.erase(p_node);
}

void VisibilityEnabler3D::_bind_methods() {

    SE_BIND_METHOD(VisibilityEnabler3D,set_enabler);
    SE_BIND_METHOD(VisibilityEnabler3D,is_enabler_enabled);

    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "pause_animations"), "set_enabler", "is_enabler_enabled", ENABLER_PAUSE_ANIMATIONS);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "freeze_bodies"), "set_enabler", "is_enabler_enabled", ENABLER_FREEZE_BODIES);

    BIND_ENUM_CONSTANT(ENABLER_PAUSE_ANIMATIONS);
    BIND_ENUM_CONSTANT(ENABLER_FREEZE_BODIES);
    BIND_ENUM_CONSTANT(ENABLER_MAX);
}

void VisibilityEnabler3D::set_enabler(Enabler p_enabler, bool p_enable) {

    ERR_FAIL_INDEX(p_enabler, ENABLER_MAX);
    enabler[p_enabler] = p_enable;
}
bool VisibilityEnabler3D::is_enabler_enabled(Enabler p_enabler) const {

    ERR_FAIL_INDEX_V(p_enabler, ENABLER_MAX, false);
    return enabler[p_enabler];
}

VisibilityEnabler3D::VisibilityEnabler3D() {

    for (int i = 0; i < ENABLER_MAX; i++)
        enabler[i] = true;

    visible = false;
}
