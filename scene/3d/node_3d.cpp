/*************************************************************************/
/*  node_3d.cpp                                                          */
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

#include "node_3d.h"

#include "core/engine.h"
#include "core/ecs_registry.h"
#include "core/message_queue.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/script_language.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/scene_string_names.h"
#include "servers/rendering_server_callbacks.h"

IMPL_GDCLASS(Node3DGizmo)
IMPL_GDCLASS(Node3D)

/*

 possible algorithms:

 Algorithm 1: (current)

 definition of invalidation: global is invalid

 1) If a node sets a LOCAL, it produces an invalidation of everything above
    a) If above is invalid, don't keep invalidating upwards
 2) If a node sets a GLOBAL, it is converted to LOCAL (and forces validation of everything pending below)

 drawback: setting/reading globals is useful and used very very often, and using affine inverses is slow

---

 Algorithm 2: (no longer current)

 definition of invalidation: NONE dirty, LOCAL dirty, GLOBAL dirty

 1) If a node sets a LOCAL, it must climb the tree and set it as GLOBAL dirty
    a) marking GLOBALs as dirty up all the tree must be done always
 2) If a node sets a GLOBAL, it marks local as dirty, and that's all?

 //is clearing the dirty state correct in this case?

 drawback: setting a local down the tree forces many tree walks often

--

future: no idea

 */

Node3DGizmo::Node3DGizmo() {
}

void Node3D::_notify_dirty() {

#ifdef TOOLS_ENABLED
    if ((data.gizmo || data.notify_transform) && !data.ignore_notification && !is_dirty_xfrom(get_instance_id())) {
#else
    if (data.notify_transform && !data.ignore_notification && !xform_change.in_list()) {

#endif
        mark_dirty_xform(get_instance_id());
    }
}

void Node3D::_update_local_transform() const {
    data.local_transform.basis.set_euler_scale(data.rotation, data.scale);

    data.dirty &= ~DIRTY_LOCAL;
}
void Node3D::_propagate_transform_changed(Node3D *p_origin) {

    if (!is_inside_tree()) {
        return;
    }

    /*
    if (data.dirty&DIRTY_GLOBAL)
        return; //already dirty
    */

    data.children_lock++;

    for (Node3D * E : data.children) {

        if (E->data.toplevel_active) {
            continue; //don't propagate to a toplevel
        }
        E->_propagate_transform_changed(p_origin);
    }
#ifdef TOOLS_ENABLED
    if ((data.gizmo || data.notify_transform) && !data.ignore_notification && !is_dirty_xfrom(get_instance_id())) {
#else
    if (data.notify_transform && !data.ignore_notification && !is_dirty_xfrom(get_instance_id())) {
#endif
        mark_dirty_xform(get_instance_id());
    }
    data.dirty |= DIRTY_GLOBAL;

    data.children_lock--;
}

void Node3D::notification_callback(int p_message_type) {
    switch (p_message_type) {
        default:
            break;
        case RenderingServerCallbacks::CALLBACK_NOTIFICATION_ENTER_GAMEPLAY: {
            notification(NOTIFICATION_ENTER_GAMEPLAY);
        } break;
        case RenderingServerCallbacks::CALLBACK_NOTIFICATION_EXIT_GAMEPLAY: {
            notification(NOTIFICATION_EXIT_GAMEPLAY);
        } break;
        case RenderingServerCallbacks::CALLBACK_SIGNAL_ENTER_GAMEPLAY: {
            emit_signal("gameplay_entered");
        } break;
        case RenderingServerCallbacks::CALLBACK_SIGNAL_EXIT_GAMEPLAY: {
            emit_signal("gameplay_exited");
        } break;
    }
}

void Node3D::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            ERR_FAIL_COND(!get_tree());

            Node *p = get_parent();
            if (p)
                data.parent = object_cast<Node3D>(p);

            if (data.parent)
                data.parent->data.children.emplace_back(this);

            if (data.toplevel && !Engine::get_singleton()->is_editor_hint()) {

                if (data.parent) {
                    data.local_transform = data.parent->get_global_transform() * get_transform();
                    data.dirty = DIRTY_VECTORS; //global is always dirty upon entering a scene
                }
                data.toplevel_active = true;
            }

            data.dirty |= DIRTY_GLOBAL; //global is always dirty upon entering a scene
            _notify_dirty();

            notification(NOTIFICATION_ENTER_WORLD);

        } break;
        case NOTIFICATION_EXIT_TREE: {

            notification(NOTIFICATION_EXIT_WORLD, true);
            mark_clean_xform(get_instance_id());

            if (data.parent)
                data.parent->data.children.erase_first(this);
            data.parent = nullptr;
            data.toplevel_active = false;
        } break;
        case NOTIFICATION_ENTER_WORLD: {

            data.inside_world = true;
            data.viewport = nullptr;
            Node *parent = get_parent();
            while (parent && !data.viewport) {
                data.viewport = object_cast<Viewport>(parent);
                parent = parent->get_parent();
            }

            ERR_FAIL_COND(!data.viewport);

            if (get_script_instance()) {
                get_script_instance()->call(StringName("_enter_world"));
            }
#ifdef TOOLS_ENABLED
            if (Engine::get_singleton()->is_editor_hint() && get_tree()->is_node_being_edited(this)) {

                //get_scene()->call_group(SceneMainLoop::GROUP_CALL_REALTIME,SceneStringNames::_spatial_editor_group,SceneStringNames::_request_gizmo,this);
                get_tree()->call_group_flags(0, "_spatial_editor_group", "_request_gizmo", Variant(this));
                if (!data.gizmo_disabled) {

                    if (data.gizmo) {
                        data.gizmo->create();
                        if (is_visible_in_tree()) {
                            data.gizmo->redraw();
                        }
                        data.gizmo->transform();
                    }
                }
            }
#endif

        } break;
        case NOTIFICATION_EXIT_WORLD: {

#ifdef TOOLS_ENABLED
            if (data.gizmo) {
                data.gizmo->free_gizmo();
                data.gizmo.unref();
            }
#endif
            if (get_script_instance()) {
                get_script_instance()->call(StringName("_exit_world"));
            }

            data.viewport = nullptr;
            data.inside_world = false;

        } break;

        case NOTIFICATION_TRANSFORM_CHANGED: {

#ifdef TOOLS_ENABLED
            if (data.gizmo) {
                data.gizmo->transform();
            }
#endif
        } break;

        default: {
        }
    }
}

void Node3D::set_transform(const Transform &p_transform) {

    data.local_transform = p_transform;
    data.dirty |= DIRTY_VECTORS;
    Object_change_notify(this,"translation");
    Object_change_notify(this,"rotation");
    Object_change_notify(this,"rotation_degrees");
    Object_change_notify(this,"scale");
    _propagate_transform_changed(this);
    if (data.notify_local_transform) {
        notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
    }
}

void Node3D::set_global_transform(const Transform &p_transform) {

    Transform xform = (data.parent && !data.toplevel_active) ?
                    data.parent->get_global_transform().affine_inverse() * p_transform :
                    p_transform;

    set_transform(xform);
}

const Transform &Node3D::get_transform() const {

    if (data.dirty & DIRTY_LOCAL) {

        _update_local_transform();
    }

    return data.local_transform;
}
Transform Node3D::get_global_transform() const {

    ERR_FAIL_COND_V(!is_inside_tree(), Transform());

    if (!(data.dirty & DIRTY_GLOBAL))
        return data.global_transform;

    if (data.dirty & DIRTY_LOCAL) {

        _update_local_transform();
    }

    if (data.parent && !data.toplevel_active) {

        data.global_transform = data.parent->get_global_transform() * data.local_transform;
    } else {

        data.global_transform = data.local_transform;
    }

    if (data.disable_scale) {
        data.global_transform.basis.orthonormalize();
    }

    data.dirty &= ~DIRTY_GLOBAL;

    return data.global_transform;
}

#ifdef TOOLS_ENABLED
Transform Node3D::get_global_gizmo_transform() const {
    return get_global_transform();
}

Transform Node3D::get_local_gizmo_transform() const {
    return get_transform();
}
// If not a VisualInstance, use this AABB for the orange box in the editor
AABB Node3D::get_fallback_gizmo_aabb() const {
    return AABB(Vector3(-0.2f, -0.2f, -0.2f), Vector3(0.4f, 0.4f, 0.4f));
}
#endif

Node3D *Node3D::get_parent_spatial() const {

    return data.parent;
}

void Node3D::_set_vi_visible(bool p_visible) {
    data.vi_visible = p_visible;
}

Transform Node3D::get_relative_transform(const Node *p_parent) const {

    if (p_parent == this) {
        return Transform();
    }

    ERR_FAIL_COND_V(!data.parent, Transform());

    if (p_parent == data.parent)
        return get_transform();
    else
        return data.parent->get_relative_transform(p_parent) * get_transform();
}

void Node3D::set_translation(const Vector3 &p_translation) {

    data.local_transform.origin = p_translation;
    Object_change_notify(this,"transform");
    _propagate_transform_changed(this);
    if (data.notify_local_transform) {
        notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
    }
}

void Node3D::set_rotation(const Vector3 &p_euler_rad) {

    if (data.dirty & DIRTY_VECTORS) {
        data.scale = data.local_transform.basis.get_scale();
        data.dirty &= ~DIRTY_VECTORS;
    }

    data.rotation = p_euler_rad;
    data.dirty |= DIRTY_LOCAL;
    Object_change_notify(this,"transform");
    _propagate_transform_changed(this);
    if (data.notify_local_transform) {
        notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
    }
}

void Node3D::set_rotation_degrees(const Vector3 &p_euler_deg) {

    set_rotation(p_euler_deg * Math_PI / 180.0);
}

void Node3D::set_scale(const Vector3 &p_scale) {

    if (data.dirty & DIRTY_VECTORS) {
        data.rotation = data.local_transform.basis.get_rotation();
        data.dirty &= ~DIRTY_VECTORS;
    }

    data.scale = p_scale;
    data.dirty |= DIRTY_LOCAL;
    Object_change_notify(this,"transform");
    _propagate_transform_changed(this);
    if (data.notify_local_transform) {
        notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
    }
}

Vector3 Node3D::get_translation() const {

    return data.local_transform.origin;
}

Vector3 Node3D::get_rotation() const {

    if (data.dirty & DIRTY_VECTORS) {
        data.scale = data.local_transform.basis.get_scale();
        data.rotation = data.local_transform.basis.get_rotation();

        data.dirty &= ~DIRTY_VECTORS;
    }

    return data.rotation;
}

Vector3 Node3D::get_rotation_degrees() const {

    return get_rotation() * 180.0 / Math_PI;
}

Vector3 Node3D::get_scale() const {

    if (data.dirty & DIRTY_VECTORS) {
        data.scale = data.local_transform.basis.get_scale();
        data.rotation = data.local_transform.basis.get_rotation();

        data.dirty &= ~DIRTY_VECTORS;
    }

    return data.scale;
}

void Node3D::update_gizmo() {

#ifdef TOOLS_ENABLED
    if (!is_inside_world())
        return;
    if (not data.gizmo)
        get_tree()->call_group_flags(SceneTree::GROUP_CALL_REALTIME, "_spatial_editor_group", "_request_gizmo", Variant(this));
    if (not data.gizmo)
        return;
    if (data.gizmo_dirty)
        return;
    data.gizmo_dirty = true;
    MessageQueue::get_singleton()->push_call(this, "_update_gizmo");
#endif
}

void Node3D::set_gizmo(const Ref<Node3DGizmo> &p_gizmo) {

#ifdef TOOLS_ENABLED

    if (data.gizmo_disabled)
        return;
    if (data.gizmo && is_inside_world())
        data.gizmo->free_gizmo();
    data.gizmo = p_gizmo;
    if (data.gizmo && is_inside_world()) {

        data.gizmo->create();
        if (is_visible_in_tree()) {
            data.gizmo->redraw();
        }
        data.gizmo->transform();
    }

#endif
}

Ref<Node3DGizmo> Node3D::get_gizmo() const {

#ifdef TOOLS_ENABLED

    return data.gizmo;
#else

    return Ref<Node3DGizmo>();
#endif
}

void Node3D::_update_gizmo() {

#ifdef TOOLS_ENABLED
    if (!is_inside_world())
        return;
    data.gizmo_dirty = false;
    if (data.gizmo) {
        if (is_visible_in_tree())
            data.gizmo->redraw();
        else
            data.gizmo->clear();
    }
#endif
}

void Node3D::set_disable_gizmo(bool p_enabled) {
#ifdef TOOLS_ENABLED

    data.gizmo_disabled = p_enabled;
    if (!p_enabled && data.gizmo)
        data.gizmo = Ref<Node3DGizmo>();
#endif
}

void Node3D::set_disable_scale(bool p_enabled) {

    data.disable_scale = p_enabled;
}

bool Node3D::is_scale_disabled() const {
    return data.disable_scale;
}

void Node3D::set_as_top_level(bool p_enabled) {

    if (data.toplevel == p_enabled)
        return;
    if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {

        if (p_enabled)
            set_transform(get_global_transform());
        else if (data.parent)
            set_transform(data.parent->get_global_transform().affine_inverse() * get_global_transform());

        data.toplevel = p_enabled;
        data.toplevel_active = p_enabled;

    } else {
        data.toplevel = p_enabled;
    }
}

bool Node3D::is_set_as_top_level() const {

    return data.toplevel;
}

Ref<World3D> Node3D::get_world_3d() const {

    ERR_FAIL_COND_V(!is_inside_world(), Ref<World3D>());
    ERR_FAIL_COND_V(!data.viewport, Ref<World3D>());

    return data.viewport->find_world_3d();
}

void Node3D::_propagate_visibility_changed() {

    notification(NOTIFICATION_VISIBILITY_CHANGED);
    emit_signal(SceneStringNames::visibility_changed);
    Object_change_notify(this,"visible");
#ifdef TOOLS_ENABLED
    if (data.gizmo) {
        _update_gizmo();
    }
#endif

    for (Node3D * c : data.children) {

        if (!c || !c->data.visible) {
            continue;
        }
        c->_propagate_visibility_changed();
    }
}

void Node3D::show() {

    if (data.visible) {
        return;
    }

    data.visible = true;

    if (!is_inside_tree()) {
        return;
    }

    _propagate_visibility_changed();
}

void Node3D::hide() {

    if (!data.visible) {
        return;
    }

    data.visible = false;

    if (!is_inside_tree()) {
        return;
    }

    _propagate_visibility_changed();
}

bool Node3D::is_visible_in_tree() const {

    const Node3D *s = this;

    while (s) {
        if (!s->data.visible) {
            return false;
        }
        s = s->data.parent;
    }

    return true;
}

void Node3D::set_visible(bool p_visible) {

    if (p_visible) {
        show();
    }
    else {
        hide();
    }
}

bool Node3D::is_visible() const {
    return data.visible;
}

void Node3D::rotate_object_local(const Vector3 &p_axis, float p_angle) {
    Transform t = get_transform();
    t.basis.rotate_local(p_axis, p_angle);
    set_transform(t);
}

void Node3D::rotate(const Vector3 &p_axis, float p_angle) {

    Transform t = get_transform();
    t.basis.rotate(p_axis, p_angle);
    set_transform(t);
}

void Node3D::rotate_x(float p_angle) {

    Transform t = get_transform();
    t.basis.rotate(Vector3(1, 0, 0), p_angle);
    set_transform(t);
}

void Node3D::rotate_y(float p_angle) {

    Transform t = get_transform();
    t.basis.rotate(Vector3(0, 1, 0), p_angle);
    set_transform(t);
}
void Node3D::rotate_z(float p_angle) {

    Transform t = get_transform();
    t.basis.rotate(Vector3(0, 0, 1), p_angle);
    set_transform(t);
}

void Node3D::translate(const Vector3 &p_offset) {

    Transform t = get_transform();
    t.translate(p_offset);
    set_transform(t);
}

void Node3D::translate_object_local(const Vector3 &p_offset) {
    Transform t = get_transform();

    Transform s;
    s.translate(p_offset);
    set_transform(t * s);
}

void Node3D::scale(const Vector3 &p_ratio) {

    Transform t = get_transform();
    t.basis.scale(p_ratio);
    set_transform(t);
}

void Node3D::scale_object_local(const Vector3 &p_scale) {
    Transform t = get_transform();
    t.basis.scale_local(p_scale);
    set_transform(t);
}

void Node3D::global_rotate(const Vector3 &p_axis, float p_angle) {

    Transform t = get_global_transform();
    t.basis.rotate(p_axis, p_angle);
    set_global_transform(t);
}

void Node3D::global_scale(const Vector3 &p_scale) {

    Transform t = get_global_transform();
    t.basis.scale(p_scale);
    set_global_transform(t);
}

void Node3D::global_translate(const Vector3 &p_offset) {
    Transform t = get_global_transform();
    t.origin += p_offset;
    set_global_transform(t);
}

void Node3D::orthonormalize() {

    Transform t = get_transform();
    t.orthonormalize();
    set_transform(t);
}

void Node3D::set_identity() {

    set_transform(Transform());
}

void Node3D::look_at(const Vector3 &p_target, const Vector3 &p_up) {

    Vector3 origin(get_global_transform().origin);
    look_at_from_position(origin, p_target, p_up);
}

void Node3D::look_at_from_position(const Vector3 &p_pos, const Vector3 &p_target, const Vector3 &p_up) {

    ERR_FAIL_COND_MSG(p_pos == p_target, "Node origin and target are in the same position, look_at() failed.");
    ERR_FAIL_COND_MSG(p_up == Vector3(), "The up vector can't be zero, look_at() failed.");
    ERR_FAIL_COND_MSG(p_up.cross(p_target - p_pos) == Vector3(), "Up vector and direction between node origin and target are aligned, look_at() failed.");

    Transform lookat;
    lookat.origin = p_pos;

    Vector3 original_scale(get_scale());
    lookat = lookat.looking_at(p_target, p_up);
    set_global_transform(lookat);
    set_scale(original_scale);
}

Vector3 Node3D::to_local(Vector3 p_global) const {

    return get_global_transform().affine_inverse().xform(p_global);
}

Vector3 Node3D::to_global(Vector3 p_local) const {

    return get_global_transform().xform(p_local);
}

void Node3D::set_notify_transform(bool p_enable) {
    data.notify_transform = p_enable;
}

bool Node3D::is_transform_notification_enabled() const {
    return data.notify_transform;
}

void Node3D::set_notify_local_transform(bool p_enable) {
    data.notify_local_transform = p_enable;
}

bool Node3D::is_local_transform_notification_enabled() const {
    return data.notify_local_transform;
}

void Node3D::force_update_transform() {
    ERR_FAIL_COND(!is_inside_tree());
    if (!is_dirty_xfrom(get_instance_id())) {
        return; //nothing to update
    }
    mark_clean_xform(get_instance_id());

    notification(NOTIFICATION_TRANSFORM_CHANGED);
}

void Node3D::_bind_methods() {

    SE_BIND_METHOD(Node3D,set_transform);
    SE_BIND_METHOD(Node3D,get_transform);
    SE_BIND_METHOD(Node3D,set_translation);
    SE_BIND_METHOD(Node3D,get_translation);
    SE_BIND_METHOD(Node3D,set_rotation);
    SE_BIND_METHOD(Node3D,get_rotation);
    SE_BIND_METHOD(Node3D,set_rotation_degrees);
    SE_BIND_METHOD(Node3D,get_rotation_degrees);
    SE_BIND_METHOD(Node3D,set_scale);
    SE_BIND_METHOD(Node3D,get_scale);
    SE_BIND_METHOD(Node3D,set_global_transform);
    SE_BIND_METHOD(Node3D,get_global_transform);
    SE_BIND_METHOD(Node3D,get_parent_spatial);
    SE_BIND_METHOD(Node3D,set_ignore_transform_notification);
    SE_BIND_METHOD(Node3D,set_as_top_level);
    SE_BIND_METHOD(Node3D,is_set_as_top_level);
    SE_BIND_METHOD(Node3D,set_disable_scale);
    SE_BIND_METHOD(Node3D,is_scale_disabled);
    SE_BIND_METHOD(Node3D,get_world_3d);

    SE_BIND_METHOD(Node3D,force_update_transform);

    SE_BIND_METHOD(Node3D,_update_gizmo);

    SE_BIND_METHOD(Node3D,update_gizmo);
    SE_BIND_METHOD(Node3D,set_gizmo);
    SE_BIND_METHOD(Node3D,get_gizmo);

    SE_BIND_METHOD(Node3D,set_visible);
    SE_BIND_METHOD(Node3D,is_visible);
    SE_BIND_METHOD(Node3D,is_visible_in_tree);
    SE_BIND_METHOD(Node3D,show);
    SE_BIND_METHOD(Node3D,hide);

    SE_BIND_METHOD(Node3D,set_notify_local_transform);
    SE_BIND_METHOD(Node3D,is_local_transform_notification_enabled);

    SE_BIND_METHOD(Node3D,set_notify_transform);
    SE_BIND_METHOD(Node3D,is_transform_notification_enabled);

    SE_BIND_METHOD(Node3D,rotate);
    SE_BIND_METHOD(Node3D,global_rotate);
    SE_BIND_METHOD(Node3D,global_scale);
    SE_BIND_METHOD(Node3D,global_translate);
    SE_BIND_METHOD(Node3D,rotate_object_local);
    SE_BIND_METHOD(Node3D,scale_object_local);
    SE_BIND_METHOD(Node3D,translate_object_local);
    SE_BIND_METHOD(Node3D,rotate_x);
    SE_BIND_METHOD(Node3D,rotate_y);
    SE_BIND_METHOD(Node3D,rotate_z);
    SE_BIND_METHOD(Node3D,translate);
    SE_BIND_METHOD(Node3D,orthonormalize);
    SE_BIND_METHOD(Node3D,set_identity);

    SE_BIND_METHOD(Node3D,look_at);
    SE_BIND_METHOD(Node3D,look_at_from_position);

    SE_BIND_METHOD(Node3D,to_local);
    SE_BIND_METHOD(Node3D,to_global);

    BIND_CONSTANT(NOTIFICATION_TRANSFORM_CHANGED);
    BIND_CONSTANT(NOTIFICATION_ENTER_WORLD);
    BIND_CONSTANT(NOTIFICATION_EXIT_WORLD);
    BIND_CONSTANT(NOTIFICATION_VISIBILITY_CHANGED);
    BIND_CONSTANT(NOTIFICATION_ENTER_GAMEPLAY);
    BIND_CONSTANT(NOTIFICATION_EXIT_GAMEPLAY);

    //ADD_PROPERTY( PropertyInfo(VariantType::TRANSFORM,"transform/global",PropertyHint::None, "", PROPERTY_USAGE_EDITOR ), "set_global_transform", "get_global_transform") ;
    ADD_GROUP("Transform", "");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM, "global_transform", PropertyHint::None, "", 0), "set_global_transform", "get_global_transform");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "translation", PropertyHint::None, "", PROPERTY_USAGE_EDITOR), "set_translation", "get_translation");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "rotation_degrees", PropertyHint::None, "", PROPERTY_USAGE_EDITOR), "set_rotation_degrees", "get_rotation_degrees");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "rotation", PropertyHint::None, "", 0), "set_rotation", "get_rotation");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "scale", PropertyHint::None, "", PROPERTY_USAGE_EDITOR), "set_scale", "get_scale");

    ADD_GROUP("Matrix", "");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM, "transform", PropertyHint::None, ""), "set_transform", "get_transform");
    ADD_GROUP("Visibility", "");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "visible"), "set_visible", "is_visible");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "gizmo", PropertyHint::ResourceType, "Node3DGizmo", 0), "set_gizmo", "get_gizmo");

    ADD_SIGNAL(MethodInfo("visibility_changed"));
    ADD_SIGNAL(MethodInfo("gameplay_entered"));
    ADD_SIGNAL(MethodInfo("gameplay_exited"));
}

Node3D::Node3D() {

    data.dirty = DIRTY_NONE;
    data.children_lock = 0;

    data.ignore_notification = false;
    data.toplevel = false;
    data.toplevel_active = false;
    data.scale = Vector3(1, 1, 1);
    data.viewport = nullptr;
    data.inside_world = false;
    data.visible = true;
    data.disable_scale = false;
    data.vi_visible = true;

#ifdef TOOLS_ENABLED
    data.gizmo_disabled = false;
    data.gizmo_dirty = false;
#endif
    data.notify_local_transform = false;
    data.notify_transform = false;
    data.parent = nullptr;
}

Node3D::~Node3D() {
}
