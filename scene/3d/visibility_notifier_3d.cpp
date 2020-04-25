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

#include "core/engine.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/physics_body_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/scene_string_names.h"
#include "scene/resources/world.h"

IMPL_GDCLASS(VisibilityNotifier3D)
IMPL_GDCLASS(VisibilityEnabler3D)
VARIANT_ENUM_CAST(VisibilityEnabler3D::Enabler);

void VisibilityNotifier3D::_enter_camera(Camera3D *p_camera) {

    ERR_FAIL_COND(cameras.contains(p_camera));
    cameras.insert(p_camera);
    if (cameras.size() == 1) {
        emit_signal(SceneStringNames::get_singleton()->screen_entered);
        _screen_enter();
    }

    emit_signal(SceneStringNames::get_singleton()->camera_entered, Variant(p_camera));
}

void VisibilityNotifier3D::_exit_camera(Camera3D *p_camera) {

    ERR_FAIL_COND(!cameras.contains(p_camera));
    cameras.erase(p_camera);

    emit_signal(SceneStringNames::get_singleton()->camera_exited, Variant(p_camera));
    if (cameras.empty()) {
        emit_signal(SceneStringNames::get_singleton()->screen_exited);

        _screen_exit();
    }
}

void VisibilityNotifier3D::set_aabb(const AABB &p_aabb) {

    if (aabb == p_aabb)
        return;
    aabb = p_aabb;

    if (is_inside_world()) {
        get_world()->_update_notifier(this, get_global_transform().xform(aabb));
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

            get_world()->_register_notifier(this, get_global_transform().xform(aabb));
        } break;
        case NOTIFICATION_TRANSFORM_CHANGED: {

            get_world()->_update_notifier(this, get_global_transform().xform(aabb));
        } break;
        case NOTIFICATION_EXIT_WORLD: {

            get_world()->_remove_notifier(this);
        } break;
    }
}

bool VisibilityNotifier3D::is_on_screen() const {

    return !cameras.empty();
}

void VisibilityNotifier3D::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_aabb", {"rect"}), &VisibilityNotifier3D::set_aabb);
    MethodBinder::bind_method(D_METHOD("get_aabb"), &VisibilityNotifier3D::get_aabb);
    MethodBinder::bind_method(D_METHOD("is_on_screen"), &VisibilityNotifier3D::is_on_screen);

    ADD_PROPERTY(PropertyInfo(VariantType::AABB, "aabb"), "set_aabb", "get_aabb");

    ADD_SIGNAL(MethodInfo("camera_entered", PropertyInfo(VariantType::OBJECT, "camera", PropertyHint::ResourceType, "Camera3D")));
    ADD_SIGNAL(MethodInfo("camera_exited", PropertyInfo(VariantType::OBJECT, "camera", PropertyHint::ResourceType, "Camera3D")));
    ADD_SIGNAL(MethodInfo("screen_entered"));
    ADD_SIGNAL(MethodInfo("screen_exited"));
}

VisibilityNotifier3D::VisibilityNotifier3D() {

    aabb = AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));
    set_notify_transform(true);
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

    if (enabler[ENABLER_FREEZE_BODIES]) {

        RigidBody *rb = object_cast<RigidBody>(p_node);
        if (rb && ((rb->get_mode() == RigidBody::MODE_CHARACTER || rb->get_mode() == RigidBody::MODE_RIGID))) {

            add = true;
            meta = rb->get_mode();
        }
    }

    if (enabler[ENABLER_PAUSE_ANIMATIONS]) {

        AnimationPlayer *ap = object_cast<AnimationPlayer>(p_node);
        if (ap) {
            add = true;
        }
    }

    if (add) {

        p_node->connect(SceneStringNames::get_singleton()->tree_exiting, this, "_node_removed", varray(Variant(p_node)), ObjectNS::CONNECT_ONESHOT);
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
            E.first->disconnect(SceneStringNames::get_singleton()->tree_exiting, this, "_node_removed");
        }

        nodes.clear();
    }
}

void VisibilityEnabler3D::_change_node_state(Node *p_node, bool p_enabled) {

    ERR_FAIL_COND(!nodes.contains(p_node));

    {
        RigidBody *rb = object_cast<RigidBody>(p_node);
        if (rb)

            rb->set_sleeping(!p_enabled);
    }

    {
        AnimationPlayer *ap = object_cast<AnimationPlayer>(p_node);

        if (ap) {

            ap->set_active(p_enabled);
        }
    }
}

void VisibilityEnabler3D::_node_removed(Node *p_node) {

    if (!visible)
        _change_node_state(p_node, true);
    nodes.erase(p_node);
}

void VisibilityEnabler3D::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_enabler", {"enabler", "enabled"}), &VisibilityEnabler3D::set_enabler);
    MethodBinder::bind_method(D_METHOD("is_enabler_enabled", {"enabler"}), &VisibilityEnabler3D::is_enabler_enabled);
    MethodBinder::bind_method(D_METHOD("_node_removed"), &VisibilityEnabler3D::_node_removed);

    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "pause_animations"), "set_enabler", "is_enabler_enabled", ENABLER_PAUSE_ANIMATIONS);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "freeze_bodies"), "set_enabler", "is_enabler_enabled", ENABLER_FREEZE_BODIES);

    BIND_ENUM_CONSTANT(ENABLER_PAUSE_ANIMATIONS)
    BIND_ENUM_CONSTANT(ENABLER_FREEZE_BODIES)
    BIND_ENUM_CONSTANT(ENABLER_MAX)
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
