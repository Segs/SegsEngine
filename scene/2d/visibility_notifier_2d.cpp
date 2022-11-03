/*************************************************************************/
/*  visibility_notifier_2d.cpp                                           */
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

#include "visibility_notifier_2d.h"

#include "gpu_particles_2d.h"

#include "core/engine.h"
#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/translation_helpers.h"
#include "scene/2d/animated_sprite_2d.h"
#include "scene/2d/physics_body_2d.h"
#include "scene/animation/animation_player.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/scene_string_names.h"

IMPL_GDCLASS(VisibilityNotifier2D)
IMPL_GDCLASS(VisibilityEnabler2D)
VARIANT_ENUM_CAST(VisibilityEnabler2D::Enabler);

void VisibilityNotifier2D::_enter_viewport(Viewport *p_viewport) {

    ERR_FAIL_COND(viewports.contains(p_viewport));
    viewports.insert(p_viewport);

    if (is_inside_tree() && Engine::get_singleton()->is_editor_hint())
        return;

    if (viewports.size() == 1) {
        emit_signal(SceneStringNames::screen_entered);

        _screen_enter();
    }
    emit_signal(SceneStringNames::viewport_entered, Variant(p_viewport));
}

void VisibilityNotifier2D::_exit_viewport(Viewport *p_viewport) {

    ERR_FAIL_COND(!viewports.contains(p_viewport));
    viewports.erase(p_viewport);

    if (is_inside_tree() && Engine::get_singleton()->is_editor_hint())
        return;

    emit_signal(SceneStringNames::viewport_exited, Variant(p_viewport));
    if (viewports.empty()) {
        emit_signal(SceneStringNames::screen_exited);

        _screen_exit();
    }
}

void VisibilityNotifier2D::set_rect(const Rect2 &p_rect) {

    rect = p_rect;
    if (is_inside_tree()) {
        get_world_2d()->_update_notifier(this, get_global_transform().xform(rect));
        if (Engine::get_singleton()->is_editor_hint()) {
            update();
            item_rect_changed();
        }
    }

    Object_change_notify(this,"rect");
}
#ifdef TOOLS_ENABLED
Rect2 VisibilityNotifier2D::_edit_get_rect() const {

    return rect;
}

bool VisibilityNotifier2D::_edit_use_rect() const {
    return true;
}
#endif

Rect2 VisibilityNotifier2D::get_rect() const {

    return rect;
}

void VisibilityNotifier2D::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {

            //get_world_2d()->
            get_world_2d()->_register_notifier(this, get_global_transform().xform(rect));
        } break;
        case NOTIFICATION_TRANSFORM_CHANGED: {

            //get_world_2d()->
            get_world_2d()->_update_notifier(this, get_global_transform().xform(rect));
        } break;
        case NOTIFICATION_DRAW: {

            if (Engine::get_singleton()->is_editor_hint()) {

                draw_rect_filled(rect, Color(1, 0.5, 1, 0.2));
            }
        } break;
        case NOTIFICATION_EXIT_TREE: {

            get_world_2d()->_remove_notifier(this);
        } break;
    }
}

bool VisibilityNotifier2D::is_on_screen() const {

    return !viewports.empty();
}

void VisibilityNotifier2D::_bind_methods() {

    SE_BIND_METHOD(VisibilityNotifier2D,set_rect);
    SE_BIND_METHOD(VisibilityNotifier2D,get_rect);
    SE_BIND_METHOD(VisibilityNotifier2D,is_on_screen);

    ADD_PROPERTY(PropertyInfo(VariantType::RECT2, "rect"), "set_rect", "get_rect");

    ADD_SIGNAL(MethodInfo("viewport_entered", PropertyInfo(VariantType::OBJECT, "viewport", PropertyHint::ResourceType, "Viewport")));
    ADD_SIGNAL(MethodInfo("viewport_exited", PropertyInfo(VariantType::OBJECT, "viewport", PropertyHint::ResourceType, "Viewport")));
    ADD_SIGNAL(MethodInfo("screen_entered"));
    ADD_SIGNAL(MethodInfo("screen_exited"));
}

VisibilityNotifier2D::VisibilityNotifier2D() {

    rect = Rect2(-10, -10, 20, 20);
    set_notify_transform(true);
}

//////////////////////////////////////

void VisibilityEnabler2D::_screen_enter() {

    for (eastl::pair<Node *const,Variant> &E : nodes) {

        _change_node_state(E.first, true);
    }

    if (enabler[ENABLER_PARENT_PHYSICS_PROCESS] && get_parent())
        get_parent()->set_physics_process(true);
    if (enabler[ENABLER_PARENT_PROCESS] && get_parent())
        get_parent()->set_process(true);

    visible = true;
}

void VisibilityEnabler2D::_screen_exit() {

    for (eastl::pair<Node *const,Variant> &E : nodes) {

        _change_node_state(E.first, false);
    }

    if (enabler[ENABLER_PARENT_PHYSICS_PROCESS] && get_parent())
        get_parent()->set_physics_process(false);
    if (enabler[ENABLER_PARENT_PROCESS] && get_parent())
        get_parent()->set_process(false);

    visible = false;
}

void VisibilityEnabler2D::_find_nodes(Node *p_node) {

    bool add = false;
    Variant meta;

    {

        RigidBody2D *rb2d = object_cast<RigidBody2D>(p_node);
        if (rb2d && ((rb2d->get_mode() == RigidBody2D::MODE_CHARACTER || rb2d->get_mode() == RigidBody2D::MODE_RIGID))) {

            add = true;
            meta = rb2d->get_mode();
        }
    }

    {

        AnimationPlayer *ap = object_cast<AnimationPlayer>(p_node);
        if (ap) {
            add = true;
        }
    }

    {

        AnimatedSprite2D *as = object_cast<AnimatedSprite2D>(p_node);
        if (as) {
            add = true;
        }
    }

    {

        GPUParticles2D *ps = object_cast<GPUParticles2D>(p_node);
        if (ps) {
            add = true;
        }
    }

    if (add) {

        p_node->connect(SceneStringNames::tree_exiting, callable_gen(this, [=]() { _node_removed(p_node);}), ObjectNS::CONNECT_ONESHOT);
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

void VisibilityEnabler2D::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {

        if (Engine::get_singleton()->is_editor_hint())
            return;

        Node *from = this;
        //find where current scene starts
        while (from->get_parent() && from->get_filename().empty())
            from = from->get_parent();

        _find_nodes(from);

        if (enabler[ENABLER_PARENT_PHYSICS_PROCESS] && get_parent())
            get_parent()->set_physics_process(false);
        if (enabler[ENABLER_PARENT_PROCESS] && get_parent())
            get_parent()->set_process(false);

        // We need to defer the call of set_process and set_physics_process,
        // otherwise they are overwritten inside NOTIFICATION_READY.
        // We can't use call_deferred, because it happens after a physics frame.
        // The ready signal works as it's emitted immediately after NOTIFICATION_READY.

        if (enabler[ENABLER_PARENT_PHYSICS_PROCESS] && get_parent()) {
            get_parent()->connect(SceneStringNames::ready,
                    callable_gen(get_parent(),[par=get_parent()]() {par->set_physics_process(false);}), ObjectNS::CONNECT_REFERENCE_COUNTED);
        }
        if (enabler[ENABLER_PARENT_PROCESS] && get_parent()) {
            get_parent()->connect(SceneStringNames::ready,
                    callable_gen(get_parent(),[par=get_parent()]() {par->set_process(false);}), ObjectNS::CONNECT_REFERENCE_COUNTED);
        }

    }

    if (p_what == NOTIFICATION_EXIT_TREE) {

        if (Engine::get_singleton()->is_editor_hint())
            return;

        for (eastl::pair<Node *const,Variant> &E : nodes) {

            if (!visible)
                _change_node_state(E.first, true);
            E.first->disconnect(SceneStringNames::tree_exiting, callable_mp(this, &VisibilityEnabler2D::_node_removed));
        }

        nodes.clear();
    }
}

void VisibilityEnabler2D::_change_node_state(Node *p_node, bool p_enabled) {

    ERR_FAIL_COND(!nodes.contains(p_node));

    if (enabler[ENABLER_FREEZE_BODIES]) {
        RigidBody2D *rb = object_cast<RigidBody2D>(p_node);
        if (rb) {

            rb->set_sleeping(!p_enabled);
        }
    }

    if (enabler[ENABLER_PAUSE_ANIMATIONS]) {
        AnimationPlayer *ap = object_cast<AnimationPlayer>(p_node);

        if (ap) {

            ap->set_active(p_enabled);
        }
    }
    if (enabler[ENABLER_PAUSE_ANIMATED_SPRITES]) {
        AnimatedSprite2D *as = object_cast<AnimatedSprite2D>(p_node);

        if (as) {

            if (p_enabled)
                as->play();
            else
                as->stop();
        }
    }

    if (enabler[ENABLER_PAUSE_PARTICLES]) {
        GPUParticles2D *ps = object_cast<GPUParticles2D>(p_node);

        if (ps) {

            ps->set_emitting(p_enabled);
        }
    }
}

void VisibilityEnabler2D::_node_removed(Node *p_node) {

    if (!visible)
        _change_node_state(p_node, true);

    nodes.erase(p_node);
}

String VisibilityEnabler2D::get_configuration_warning() const {
    String warning = BaseClassName::get_configuration_warning();

#ifdef TOOLS_ENABLED
    if (is_inside_tree() && get_parent() && (get_parent()->get_filename().empty() && get_parent() != get_tree()->get_edited_scene_root())) {
        if(!warning.empty())
            warning.append("\n\n");
        return warning+TTRS("VisibilityEnabler2D works best when used with the edited scene root directly as parent.");
    }
#endif
    return warning;
}

void VisibilityEnabler2D::_bind_methods() {

    SE_BIND_METHOD(VisibilityEnabler2D,set_enabler);
    SE_BIND_METHOD(VisibilityEnabler2D,is_enabler_enabled);

    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "pause_animations"), "set_enabler", "is_enabler_enabled", ENABLER_PAUSE_ANIMATIONS);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "freeze_bodies"), "set_enabler", "is_enabler_enabled", ENABLER_FREEZE_BODIES);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "pause_particles"), "set_enabler", "is_enabler_enabled", ENABLER_PAUSE_PARTICLES);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "pause_animated_sprites"), "set_enabler", "is_enabler_enabled", ENABLER_PAUSE_ANIMATED_SPRITES);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "process_parent"), "set_enabler", "is_enabler_enabled", ENABLER_PARENT_PROCESS);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "physics_process_parent"), "set_enabler", "is_enabler_enabled", ENABLER_PARENT_PHYSICS_PROCESS);

    BIND_ENUM_CONSTANT(ENABLER_PAUSE_ANIMATIONS);
    BIND_ENUM_CONSTANT(ENABLER_FREEZE_BODIES);
    BIND_ENUM_CONSTANT(ENABLER_PAUSE_PARTICLES);
    BIND_ENUM_CONSTANT(ENABLER_PARENT_PROCESS);
    BIND_ENUM_CONSTANT(ENABLER_PARENT_PHYSICS_PROCESS);
    BIND_ENUM_CONSTANT(ENABLER_PAUSE_ANIMATED_SPRITES);
    BIND_ENUM_CONSTANT(ENABLER_MAX);
}

void VisibilityEnabler2D::set_enabler(Enabler p_enabler, bool p_enable) {

    ERR_FAIL_INDEX(p_enabler, ENABLER_MAX);
    enabler[p_enabler] = p_enable;
}
bool VisibilityEnabler2D::is_enabler_enabled(Enabler p_enabler) const {

    ERR_FAIL_INDEX_V(p_enabler, ENABLER_MAX, false);
    return enabler[p_enabler];
}

VisibilityEnabler2D::VisibilityEnabler2D() {

    for (int i = 0; i < ENABLER_MAX; i++)
        enabler[i] = true;
    enabler[ENABLER_PARENT_PROCESS] = false;
    enabler[ENABLER_PARENT_PHYSICS_PROCESS] = false;

    visible = false;
}
