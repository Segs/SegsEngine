/*************************************************************************/
/*  collision_object.cpp                                                 */
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

#include "collision_object_3d.h"

#include "mesh_instance_3d.h"
#include "scene/scene_string_names.h"
#include "servers/physics_server_3d.h"
#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/script_language.h"
#include "core/input/input_event.h"
#include "core/translation_helpers.h"
#include "core/engine.h"
#include "scene/main/scene_tree.h"

IMPL_GDCLASS(CollisionObject3D)

void CollisionObject3D::_notification(int p_what) {

    switch (p_what) {

    case NOTIFICATION_ENTER_TREE: {
            if (_are_collision_shapes_visible()) {
                debug_shape_old_transform = get_global_transform();
                for (const Pair<const uint32_t, ShapeData> &iter : shapes) {
                    debug_shapes_to_update.insert(iter.first);
                }
                _update_debug_shapes();
            }
        } break;
        case NOTIFICATION_EXIT_TREE: {
            if (debug_shapes_count > 0) {
                _clear_debug_shapes();
            }
        } break;
        case NOTIFICATION_ENTER_WORLD: {

            if (area) {
                PhysicsServer3D::get_singleton()->area_set_transform(rid, get_global_transform());
            } else {
                PhysicsServer3D::get_singleton()->body_set_state(rid, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());
            }

            RID space = get_world_3d()->get_space();
            if (area) {
                PhysicsServer3D::get_singleton()->area_set_space(rid, space);
            } else {
                PhysicsServer3D::get_singleton()->body_set_space(rid, space);
            }

            _update_pickable();
            //get space
        } break;

        case NOTIFICATION_TRANSFORM_CHANGED: {
            if (only_update_transform_changes) {
                return;
            }

            if (area) {
                PhysicsServer3D::get_singleton()->area_set_transform(rid, get_global_transform());
            } else {
                PhysicsServer3D::get_singleton()->body_set_state(rid, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());

            }
            _on_transform_changed();
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {

            _update_pickable();

        } break;
        case NOTIFICATION_EXIT_WORLD: {

            if (area) {
                PhysicsServer3D::get_singleton()->area_set_space(rid, RID());
            } else {
                PhysicsServer3D::get_singleton()->body_set_space(rid, RID());
            }

        } break;
    }
}

void CollisionObject3D::set_collision_layer(uint32_t p_layer) {
    collision_layer = p_layer;
    if (area) {
        PhysicsServer3D::get_singleton()->area_set_collision_layer(get_rid(), p_layer);
    } else {
        PhysicsServer3D::get_singleton()->body_set_collision_layer(get_rid(), p_layer);
    }
}

uint32_t CollisionObject3D::get_collision_layer() const {
    return collision_layer;
}

void CollisionObject3D::set_collision_mask(uint32_t p_mask) {
    collision_mask = p_mask;
    if (area) {
        PhysicsServer3D::get_singleton()->area_set_collision_mask(get_rid(), p_mask);
    } else {
        PhysicsServer3D::get_singleton()->body_set_collision_mask(get_rid(), p_mask);
    }
}

uint32_t CollisionObject3D::get_collision_mask() const {
    return collision_mask;
}

void CollisionObject3D::set_collision_layer_bit(int p_bit, bool p_value) {
    ERR_FAIL_INDEX_MSG(p_bit, 32, "Collision layer bit must be between 0 and 31 inclusive.");
    uint32_t collision_layer = get_collision_layer();
    if (p_value) {
        collision_layer |= 1 << p_bit;
    } else {
        collision_layer &= ~(1 << p_bit);
    }
    set_collision_layer(collision_layer);
}

bool CollisionObject3D::get_collision_layer_bit(int p_bit) const {
    ERR_FAIL_INDEX_V_MSG(p_bit, 32, false, "Collision layer bit must be between 0 and 31 inclusive.");
    return get_collision_layer() & (1 << p_bit);
}

void CollisionObject3D::set_collision_mask_bit(int p_bit, bool p_value) {
    ERR_FAIL_INDEX_MSG(p_bit, 32, "Collision mask bit must be between 0 and 31 inclusive.");
    uint32_t mask = get_collision_mask();
    if (p_value) {
        mask |= 1 << p_bit;
    } else {
        mask &= ~(1 << p_bit);
    }
    set_collision_mask(mask);
}

bool CollisionObject3D::get_collision_mask_bit(int p_bit) const {
    ERR_FAIL_INDEX_V_MSG(p_bit, 32, false, "Collision mask bit must be between 0 and 31 inclusive.");
    return get_collision_mask() & (1 << p_bit);
}

void CollisionObject3D::_input_event(Node *p_camera, const Ref<InputEvent> &p_input_event, const Vector3 &p_pos, const Vector3 &p_normal, int p_shape) {

    if (get_script_instance()) {
        get_script_instance()->call(SceneStringNames::_input_event, Variant(p_camera), p_input_event, p_pos, p_normal, p_shape);
    }
    emit_signal(SceneStringNames::input_event, Variant(p_camera), p_input_event, p_pos, p_normal, p_shape);
}

void CollisionObject3D::_mouse_enter() {

    if (get_script_instance()) {
        get_script_instance()->call(SceneStringNames::_mouse_enter);
    }
    emit_signal(SceneStringNames::mouse_entered);
}

void CollisionObject3D::_mouse_exit() {

    if (get_script_instance()) {
        get_script_instance()->call(SceneStringNames::_mouse_exit);
    }
    emit_signal(SceneStringNames::mouse_exited);
}

void CollisionObject3D::set_only_update_transform_changes(bool p_enable) {
    only_update_transform_changes = p_enable;
}
void CollisionObject3D::_update_pickable() {
    if (!is_inside_tree()) {
        return;
    }
    bool pickable = ray_pickable && is_visible_in_tree();
    if (area) {
        PhysicsServer3D::get_singleton()->area_set_ray_pickable(rid, pickable);
    } else {
        PhysicsServer3D::get_singleton()->body_set_ray_pickable(rid, pickable);
    }
}

bool CollisionObject3D::_are_collision_shapes_visible() {
    return is_inside_tree() && get_tree()->is_debugging_collisions_hint() && !Engine::get_singleton()->is_editor_hint();
}

void CollisionObject3D::_update_shape_data(uint32_t p_owner) {
    if (_are_collision_shapes_visible()) {
        if (debug_shapes_to_update.empty()) {
            call_deferred("_update_debug_shapes");
        }
        debug_shapes_to_update.insert(p_owner);
    }
}

void CollisionObject3D::_shape_changed(const Ref<Shape> &p_shape) {
    for (auto & E : shapes) {
        ShapeData &shapedata = E.second;
        ShapeData::ShapeBase *shapes = shapedata.shapes.data();
        for (int i = 0; i < shapedata.shapes.size(); i++) {
            ShapeData::ShapeBase &s = shapes[i];
            if (s.shape == p_shape && s.debug_shape!=entt::null) {
                Ref<Mesh> mesh = s.shape->get_debug_mesh();
                RenderingServer::get_singleton()->instance_set_base(s.debug_shape, mesh->get_rid());
            }
        }
    }
}


void CollisionObject3D::_update_debug_shapes() {
    if (!is_inside_tree()) {
        debug_shapes_to_update.clear();
        return;
    }

    for (uint32_t shapedata_idx : debug_shapes_to_update) {
        if (shapes.contains(shapedata_idx)) {
            ShapeData &shapedata = shapes[shapedata_idx];
            for (ShapeData::ShapeBase &s : shapedata.shapes) {
                if (!s.shape || shapedata.disabled) {
                    if (s.debug_shape!=entt::null) {
                        RenderingServer::get_singleton()->free_rid(s.debug_shape);
                        s.debug_shape = entt::null;
                        --debug_shapes_count;
                    }
                    continue;
                }
                if (s.debug_shape==entt::null) {
                    s.debug_shape = RenderingServer::get_singleton()->instance_create();
                    RenderingServer::get_singleton()->instance_set_scenario(s.debug_shape, get_world_3d()->get_scenario());


                    if (!s.shape->is_connected_any(StringName("changed"), get_instance_id())) {
                        auto lambda= [this,shape=s.shape]() { _shape_changed(shape); };
                        s.shape->connect("changed", callable_gen(this,lambda), ObjectNS::CONNECT_QUEUED);
                    }

                    ++debug_shapes_count;

                }
                Ref<Mesh> mesh = s.shape->get_debug_mesh();
                RenderingServer::get_singleton()->instance_set_base(s.debug_shape, mesh->get_rid());
                RenderingServer::get_singleton()->instance_set_transform(s.debug_shape, get_global_transform() * shapedata.xform);
                RenderingServer::get_singleton()->instance_set_portal_mode(s.debug_shape, RS::INSTANCE_PORTAL_MODE_GLOBAL);

            }
        }
    }
    debug_shapes_to_update.clear();
}

void CollisionObject3D::_clear_debug_shapes() {
    for (auto &E : shapes) {
        ShapeData &shapedata = E.second;
        ShapeData::ShapeBase *shapes = shapedata.shapes.data();
        for (int i = 0; i < shapedata.shapes.size(); i++) {
            ShapeData::ShapeBase &s = shapes[i];
            if (s.debug_shape!=entt::null) {
                RenderingServer::get_singleton()->free_rid(s.debug_shape);
                s.debug_shape = entt::null;
                if (s.shape && s.shape->is_connected_any("changed", get_instance_id())) {
                    s.shape->disconnect_all("changed", this->get_instance_id());
                }
            }
        }
    }

    debug_shapes_count = 0;
}

void CollisionObject3D::_on_transform_changed() {
    if (debug_shapes_count > 0 && !debug_shape_old_transform.is_equal_approx(get_global_transform())) {
        debug_shape_old_transform = get_global_transform();
        for (auto &E : shapes) {
            ShapeData &shapedata = E.second;
            for (const ShapeData::ShapeBase &shape : shapedata.shapes) {
                RenderingServer::get_singleton()->instance_set_transform(shape.debug_shape, debug_shape_old_transform * shapedata.xform);
            }
        }
    }
}

void CollisionObject3D::set_ray_pickable(bool p_ray_pickable) {

    ray_pickable = p_ray_pickable;
    _update_pickable();
}

bool CollisionObject3D::is_ray_pickable() const {

    return ray_pickable;
}

void CollisionObject3D::_bind_methods() {

    SE_BIND_METHOD(CollisionObject3D,set_collision_layer);
    SE_BIND_METHOD(CollisionObject3D,get_collision_layer);
    SE_BIND_METHOD(CollisionObject3D,set_collision_mask);
    SE_BIND_METHOD(CollisionObject3D,get_collision_mask);
    SE_BIND_METHOD(CollisionObject3D,set_collision_layer_bit);
    SE_BIND_METHOD(CollisionObject3D,get_collision_layer_bit);
    SE_BIND_METHOD(CollisionObject3D,set_collision_mask_bit);
    SE_BIND_METHOD(CollisionObject3D,get_collision_mask_bit);
    SE_BIND_METHOD(CollisionObject3D,set_ray_pickable);
    SE_BIND_METHOD(CollisionObject3D,is_ray_pickable);
    SE_BIND_METHOD(CollisionObject3D,set_capture_input_on_drag);
    SE_BIND_METHOD(CollisionObject3D,get_capture_input_on_drag);
    SE_BIND_METHOD(CollisionObject3D,get_rid);
    SE_BIND_METHOD(CollisionObject3D,create_shape_owner);
    SE_BIND_METHOD(CollisionObject3D,remove_shape_owner);
    MethodBinder::bind_method(D_METHOD("get_shape_owners"), &CollisionObject3D::_get_shape_owners);
    SE_BIND_METHOD(CollisionObject3D,shape_owner_set_transform);
    SE_BIND_METHOD(CollisionObject3D,shape_owner_get_transform);
    SE_BIND_METHOD(CollisionObject3D,shape_owner_get_owner);
    SE_BIND_METHOD(CollisionObject3D,shape_owner_set_disabled);
    SE_BIND_METHOD(CollisionObject3D,is_shape_owner_disabled);
    SE_BIND_METHOD(CollisionObject3D,shape_owner_add_shape);
    SE_BIND_METHOD(CollisionObject3D,shape_owner_get_shape_count);
    SE_BIND_METHOD(CollisionObject3D,shape_owner_get_shape);
    SE_BIND_METHOD(CollisionObject3D,shape_owner_get_shape_index);
    SE_BIND_METHOD(CollisionObject3D,shape_owner_remove_shape);
    SE_BIND_METHOD(CollisionObject3D,shape_owner_clear_shapes);
    SE_BIND_METHOD(CollisionObject3D,shape_find_owner);

    BIND_VMETHOD(MethodInfo("_input_event", PropertyInfo(VariantType::OBJECT, "camera"),
            PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEvent"),
            PropertyInfo(VariantType::VECTOR3, "click_position"), PropertyInfo(VariantType::VECTOR3, "click_normal"),
            PropertyInfo(VariantType::INT, "shape_idx")));

    ADD_SIGNAL(MethodInfo("input_event", PropertyInfo(VariantType::OBJECT, "camera", PropertyHint::ResourceType, "Node"),
            PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEvent"),
            PropertyInfo(VariantType::VECTOR3, "click_position"), PropertyInfo(VariantType::VECTOR3, "click_normal"),
            PropertyInfo(VariantType::INT, "shape_idx")));
    ADD_SIGNAL(MethodInfo("mouse_entered"));
    ADD_SIGNAL(MethodInfo("mouse_exited"));

    ADD_GROUP("Collision", "collision_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_layer", PropertyHint::Layers3DPhysics), "set_collision_layer", "get_collision_layer");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_mask", PropertyHint::Layers3DPhysics), "set_collision_mask", "get_collision_mask");

    ADD_GROUP("Input", "input_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "input_ray_pickable"), "set_ray_pickable", "is_ray_pickable");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "input_capture_on_drag"), "set_capture_input_on_drag", "get_capture_input_on_drag");
}

uint32_t CollisionObject3D::create_shape_owner(Object *p_owner) {

    ShapeData sd;
    uint32_t id;

    if (shapes.empty()) {
        id = 0;
    } else {
        id = shapes.rbegin()->first + 1;
    }

    sd.owner_id = p_owner ? p_owner->get_instance_id() : entt::null;

    shapes[id] = sd;

    return id;
}

void CollisionObject3D::remove_shape_owner(uint32_t owner) {

    ERR_FAIL_COND(!shapes.contains(owner));

    shape_owner_clear_shapes(owner);

    shapes.erase(owner);
}

void CollisionObject3D::shape_owner_set_disabled(uint32_t p_owner, bool p_disabled) {
    ERR_FAIL_COND(!shapes.contains(p_owner));

    ShapeData &sd = shapes[p_owner];
    if (sd.disabled == p_disabled) {
        return;
    }
    sd.disabled = p_disabled;
    for (int i = 0; i < sd.shapes.size(); i++) {
        if (area) {
            PhysicsServer3D::get_singleton()->area_set_shape_disabled(rid, sd.shapes[i].index, p_disabled);
        } else {
            PhysicsServer3D::get_singleton()->body_set_shape_disabled(rid, sd.shapes[i].index, p_disabled);
        }
    }
    _update_shape_data(p_owner);
}

bool CollisionObject3D::is_shape_owner_disabled(uint32_t p_owner) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), false);

    return shapes.at(p_owner).disabled;
}

void CollisionObject3D::get_shape_owners(Vector<uint32_t> *r_owners) {

    for (eastl::pair<const uint32_t,ShapeData> &E : shapes) {
        r_owners->push_back(E.first);
    }
}

Array CollisionObject3D::_get_shape_owners() {

    Array ret;
    for (eastl::pair<const uint32_t,ShapeData> &E : shapes) {
        ret.push_back(E.first);
    }

    return ret;
}

void CollisionObject3D::shape_owner_set_transform(uint32_t p_owner, const Transform &p_transform) {

    ERR_FAIL_COND(!shapes.contains(p_owner));

    ShapeData &sd = shapes[p_owner];
    sd.xform = p_transform;
    for (int i = 0; i < sd.shapes.size(); i++) {
        if (area) {
            PhysicsServer3D::get_singleton()->area_set_shape_transform(rid, sd.shapes[i].index, p_transform);
        } else {
            PhysicsServer3D::get_singleton()->body_set_shape_transform(rid, sd.shapes[i].index, p_transform);
        }
    }
    _update_shape_data(p_owner);
}
Transform CollisionObject3D::shape_owner_get_transform(uint32_t p_owner) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), Transform());

    return shapes.at(p_owner).xform;
}

Object *CollisionObject3D::shape_owner_get_owner(uint32_t p_owner) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), nullptr);

    return object_for_entity(shapes.at(p_owner).owner_id);
}

void CollisionObject3D::shape_owner_add_shape(uint32_t p_owner, const Ref<Shape> &p_shape) {

    ERR_FAIL_COND(!shapes.contains(p_owner));
    ERR_FAIL_COND(not p_shape);

    ShapeData &sd = shapes[p_owner];
    ShapeData::ShapeBase s;
    s.index = total_subshapes;
    s.shape = p_shape;
    if (area) {
        PhysicsServer3D::get_singleton()->area_add_shape(rid, p_shape->get_phys_rid(), sd.xform, sd.disabled);
    } else {
        PhysicsServer3D::get_singleton()->body_add_shape(rid, p_shape->get_phys_rid(), sd.xform, sd.disabled);
    }
    sd.shapes.push_back(s);

    total_subshapes++;
    _update_shape_data(p_owner);
}
int CollisionObject3D::shape_owner_get_shape_count(uint32_t p_owner) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), 0);

    return shapes.at(p_owner).shapes.size();
}
Ref<Shape> CollisionObject3D::shape_owner_get_shape(uint32_t p_owner, int p_shape) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), Ref<Shape>());
    ERR_FAIL_INDEX_V(p_shape, shapes.at(p_owner).shapes.size(), Ref<Shape>());

    return shapes.at(p_owner).shapes[p_shape].shape;
}
int CollisionObject3D::shape_owner_get_shape_index(uint32_t p_owner, int p_shape) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), -1);
    ERR_FAIL_INDEX_V(p_shape, shapes.at(p_owner).shapes.size(), -1);

    return shapes.at(p_owner).shapes[p_shape].index;
}

void CollisionObject3D::shape_owner_remove_shape(uint32_t p_owner, int p_shape) {

    ERR_FAIL_COND(!shapes.contains(p_owner));
    ERR_FAIL_INDEX(p_shape, shapes[p_owner].shapes.size());

    const ShapeData::ShapeBase &s = shapes[p_owner].shapes[p_shape];
    int index_to_remove = s.index;
    if (area) {
        PhysicsServer3D::get_singleton()->area_remove_shape(rid, index_to_remove);
    } else {
        PhysicsServer3D::get_singleton()->body_remove_shape(rid, index_to_remove);
    }

    if (s.debug_shape!=entt::null) {
        RenderingServer::get_singleton()->free_rid(s.debug_shape);
        if (s.shape && s.shape->is_connected_any("changed", get_instance_id())) {
            s.shape->disconnect_all("changed", get_instance_id());
        }
        --debug_shapes_count;
    }

    shapes[p_owner].shapes.erase_at(p_shape);

    for (eastl::pair<const uint32_t,ShapeData> &E : shapes) {
        for (auto & shape : E.second.shapes) {
            if (shape.index > index_to_remove) {
                shape.index -= 1;
            }
        }
    }

    total_subshapes--;
}

void CollisionObject3D::shape_owner_clear_shapes(uint32_t p_owner) {

    ERR_FAIL_COND(!shapes.contains(p_owner));

    while (shape_owner_get_shape_count(p_owner) > 0) {
        shape_owner_remove_shape(p_owner, 0);
    }
}

uint32_t CollisionObject3D::shape_find_owner(int p_shape_index) const {

    ERR_FAIL_INDEX_V(p_shape_index, total_subshapes, UINT32_MAX);

    for (const eastl::pair<const uint32_t,ShapeData> &E : shapes) {
        for (int i = 0; i < E.second.shapes.size(); i++) {
            if (E.second.shapes[i].index == p_shape_index) {
                return E.first;
            }
        }
    }

    //in theory it should be unreachable
    ERR_FAIL_V_MSG(UINT32_MAX, "Can't find owner for shape index " + itos(p_shape_index) + ".");
}

CollisionObject3D::CollisionObject3D(RID p_rid, bool p_area) {

    rid = p_rid;
    area = p_area;
    capture_input_on_drag = false;
    ray_pickable = true;
    set_notify_transform(true);
    total_subshapes = 0;

    if (p_area) {
        PhysicsServer3D::get_singleton()->area_attach_object_instance_id(rid, get_instance_id());
    } else {
        PhysicsServer3D::get_singleton()->body_attach_object_instance_id(rid, get_instance_id());
    }
    //set_transform_notify(true);
}

void CollisionObject3D::set_capture_input_on_drag(bool p_capture) {

    capture_input_on_drag = p_capture;
}

bool CollisionObject3D::get_capture_input_on_drag() const {

    return capture_input_on_drag;
}

String CollisionObject3D::get_configuration_warning() const {

    String warning(Node3D::get_configuration_warning());

    if (shapes.empty()) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("This node has no shape, so it can't collide or interact with other objects.\nConsider adding a CollisionShape3D or CollisionPolygon3D as a child to define its shape.");
    }

    return warning;
}

CollisionObject3D::CollisionObject3D() {

    capture_input_on_drag = false;
    ray_pickable = true;
    set_notify_transform(true);
    //owner=

    //set_transform_notify(true);
}

CollisionObject3D::~CollisionObject3D() {

    PhysicsServer3D::get_singleton()->free_rid(rid);
}
