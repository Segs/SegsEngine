/*************************************************************************/
/*  collision_object_2d.cpp                                              */
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

#include "collision_object_2d.h"

#include "scene/scene_string_names.h"
#include "servers/physics_server_2d.h"
#include "core/method_bind.h"
#include "core/class_db.h"
#include "core/object_tooling.h"
#include "core/script_language.h"
#include "core/input/input_event.h"
#include "core/translation_helpers.h"
#include "scene/resources/world_2d.h"

IMPL_GDCLASS(CollisionObject2D)

void CollisionObject2D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {

            Transform2D global_transform = get_global_transform();

            if (area)
                PhysicsServer2D::get_singleton()->area_set_transform(rid, global_transform);
            else
                PhysicsServer2D::get_singleton()->body_set_state(rid, PhysicsServer2D::BODY_STATE_TRANSFORM, global_transform);

            Ref<World2D> world_ref = get_world_2d();
            ERR_FAIL_COND(!world_ref);
            RID space = world_ref->get_space();
            if (area) {
                PhysicsServer2D::get_singleton()->area_set_space(rid, space);
            } else
                PhysicsServer2D::get_singleton()->body_set_space(rid, space);

            _update_pickable();

            //get space
        } break;

        case NOTIFICATION_ENTER_CANVAS: {

            if (area)
                PhysicsServer2D::get_singleton()->area_attach_canvas_instance_id(rid, get_canvas_layer_instance_id());
            else
                PhysicsServer2D::get_singleton()->body_attach_canvas_instance_id(rid, get_canvas_layer_instance_id());
        } break;

        case NOTIFICATION_VISIBILITY_CHANGED: {

            _update_pickable();
        } break;
        case NOTIFICATION_TRANSFORM_CHANGED: {

            if (only_update_transform_changes) {
                return;
            }

            Transform2D global_transform = get_global_transform();

            if (area)
                PhysicsServer2D::get_singleton()->area_set_transform(rid, global_transform);
            else
                PhysicsServer2D::get_singleton()->body_set_state(rid, PhysicsServer2D::BODY_STATE_TRANSFORM, global_transform);

        } break;
        case NOTIFICATION_EXIT_TREE: {

            if (area) {
                PhysicsServer2D::get_singleton()->area_set_space(rid, RID());
            } else
                PhysicsServer2D::get_singleton()->body_set_space(rid, RID());

        } break;

        case NOTIFICATION_EXIT_CANVAS: {

            if (area)
                PhysicsServer2D::get_singleton()->area_attach_canvas_instance_id(rid, entt::null);
            else
                PhysicsServer2D::get_singleton()->body_attach_canvas_instance_id(rid, entt::null);
        } break;
    }
}

uint32_t CollisionObject2D::create_shape_owner(Object *p_owner) {

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

void CollisionObject2D::remove_shape_owner(uint32_t owner) {

    ERR_FAIL_COND(!shapes.contains(owner));

    shape_owner_clear_shapes(owner);

    shapes.erase(owner);
}

void CollisionObject2D::shape_owner_set_disabled(uint32_t p_owner, bool p_disabled) {
    ERR_FAIL_COND(!shapes.contains(p_owner));

    ShapeData &sd = shapes[p_owner];
    sd.disabled = p_disabled;
    for (int i = 0; i < sd.shapes.size(); i++) {
        if (area) {
            PhysicsServer2D::get_singleton()->area_set_shape_disabled(rid, sd.shapes[i].index, p_disabled);
        } else {
            PhysicsServer2D::get_singleton()->body_set_shape_disabled(rid, sd.shapes[i].index, p_disabled);
        }
    }
}

bool CollisionObject2D::is_shape_owner_disabled(uint32_t p_owner) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), false);

    return shapes.at(p_owner).disabled;
}

void CollisionObject2D::shape_owner_set_one_way_collision(uint32_t p_owner, bool p_enable) {

    if (area)
        return; //not for areas

    ERR_FAIL_COND(!shapes.contains(p_owner));

    ShapeData &sd = shapes[p_owner];
    sd.one_way_collision = p_enable;
    for (int i = 0; i < sd.shapes.size(); i++) {
        PhysicsServer2D::get_singleton()->body_set_shape_as_one_way_collision(rid, sd.shapes[i].index, sd.one_way_collision, sd.one_way_collision_margin);
    }
}

bool CollisionObject2D::is_shape_owner_one_way_collision_enabled(uint32_t p_owner) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), false);

    return shapes.at(p_owner).one_way_collision;
}

void CollisionObject2D::shape_owner_set_one_way_collision_margin(uint32_t p_owner, float p_margin) {

    if (area)
        return; //not for areas

    ERR_FAIL_COND(!shapes.contains(p_owner));

    ShapeData &sd = shapes[p_owner];
    sd.one_way_collision_margin = p_margin;
    for (int i = 0; i < sd.shapes.size(); i++) {
        PhysicsServer2D::get_singleton()->body_set_shape_as_one_way_collision(rid, sd.shapes[i].index, sd.one_way_collision, sd.one_way_collision_margin);
    }
}

float CollisionObject2D::get_shape_owner_one_way_collision_margin(uint32_t p_owner) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), 0);

    return shapes.at(p_owner).one_way_collision_margin;
}

void CollisionObject2D::get_shape_owners(Vector<uint32_t> *r_owners) {

    for (eastl::pair<const uint32_t,ShapeData> &E : shapes) {
        r_owners->push_back(E.first);
    }
}

Array CollisionObject2D::_get_shape_owners() {

    Array ret;
    for (eastl::pair<const uint32_t,ShapeData> &E : shapes) {
        ret.push_back(E.first);
    }

    return ret;
}

void CollisionObject2D::shape_owner_set_transform(uint32_t p_owner, const Transform2D &p_transform) {

    ERR_FAIL_COND(!shapes.contains(p_owner));

    ShapeData &sd = shapes[p_owner];

    sd.xform = p_transform;
    for (int i = 0; i < sd.shapes.size(); i++) {
        if (area) {
            PhysicsServer2D::get_singleton()->area_set_shape_transform(rid, sd.shapes[i].index, sd.xform);
        } else {
            PhysicsServer2D::get_singleton()->body_set_shape_transform(rid, sd.shapes[i].index, sd.xform);
        }
    }
}
Transform2D CollisionObject2D::shape_owner_get_transform(uint32_t p_owner) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), Transform2D());

    return shapes.at(p_owner).xform;
}

Object *CollisionObject2D::shape_owner_get_owner(uint32_t p_owner) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), nullptr);

    return object_for_entity(shapes.at(p_owner).owner_id);
}

void CollisionObject2D::shape_owner_add_shape(uint32_t p_owner, const Ref<Shape2D> &p_shape) {

    ERR_FAIL_COND(!shapes.contains(p_owner));
    ERR_FAIL_COND(not p_shape);

    ShapeData &sd = shapes[p_owner];
    ShapeData::Shape s;
    s.index = total_subshapes;
    s.shape = p_shape;
    if (area) {
        PhysicsServer2D::get_singleton()->area_add_shape(rid, p_shape->get_phys_rid(), sd.xform, sd.disabled);
    } else {
        PhysicsServer2D::get_singleton()->body_add_shape(rid, p_shape->get_phys_rid(), sd.xform, sd.disabled);
    }
    sd.shapes.push_back(s);

    total_subshapes++;
}

int CollisionObject2D::shape_owner_get_shape_count(uint32_t p_owner) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), 0);

    return shapes.at(p_owner).shapes.size();
}
Ref<Shape2D> CollisionObject2D::shape_owner_get_shape(uint32_t p_owner, int p_shape) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), Ref<Shape2D>());
    ERR_FAIL_INDEX_V(p_shape, shapes.at(p_owner).shapes.size(), Ref<Shape2D>());
;
    return shapes.at(p_owner).shapes[p_shape].shape;
}
int CollisionObject2D::shape_owner_get_shape_index(uint32_t p_owner, int p_shape) const {

    ERR_FAIL_COND_V(!shapes.contains(p_owner), -1);
    ERR_FAIL_INDEX_V(p_shape, shapes.at(p_owner).shapes.size(), -1);

    return shapes.at(p_owner).shapes[p_shape].index;
}

void CollisionObject2D::shape_owner_remove_shape(uint32_t p_owner, int p_shape) {

    ERR_FAIL_COND(!shapes.contains(p_owner));
    ERR_FAIL_INDEX(p_shape, shapes[p_owner].shapes.size());

    int index_to_remove = shapes[p_owner].shapes[p_shape].index;
    if (area) {
        PhysicsServer2D::get_singleton()->area_remove_shape(rid, index_to_remove);
    } else {
        PhysicsServer2D::get_singleton()->body_remove_shape(rid, index_to_remove);
    }

    shapes[p_owner].shapes.erase_at(p_shape);

    for (eastl::pair<const uint32_t,ShapeData> &E : shapes) {
        for (int i = 0; i < E.second.shapes.size(); i++) {
            if (E.second.shapes[i].index > index_to_remove) {
                E.second.shapes[i].index -= 1;
            }
        }
    }

    total_subshapes--;
}

void CollisionObject2D::shape_owner_clear_shapes(uint32_t p_owner) {

    ERR_FAIL_COND(!shapes.contains(p_owner));

    while (shape_owner_get_shape_count(p_owner) > 0) {
        shape_owner_remove_shape(p_owner, 0);
    }
}

uint32_t CollisionObject2D::shape_find_owner(int p_shape_index) const {

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

void CollisionObject2D::set_pickable(bool p_enabled) {

    if (pickable == p_enabled)
        return;

    pickable = p_enabled;
    _update_pickable();
}

bool CollisionObject2D::is_pickable() const {

    return pickable;
}

void CollisionObject2D::_input_event(Node *p_viewport, const Ref<InputEvent> &p_input_event, int p_shape) {

    if (get_script_instance()) {
        get_script_instance()->call(SceneStringNames::_input_event, Variant(p_viewport), p_input_event, p_shape);
    }
    emit_signal(SceneStringNames::input_event, Variant(p_viewport), p_input_event, p_shape);
}

void CollisionObject2D::_mouse_enter() {

    if (get_script_instance()) {
        get_script_instance()->call(SceneStringNames::_mouse_enter);
    }
    emit_signal(SceneStringNames::mouse_entered);
}

void CollisionObject2D::_mouse_exit() {

    if (get_script_instance()) {
        get_script_instance()->call(SceneStringNames::_mouse_exit);
    }
    emit_signal(SceneStringNames::mouse_exited);
}

void CollisionObject2D::set_only_update_transform_changes(bool p_enable) {
    only_update_transform_changes = p_enable;
}

void CollisionObject2D::_update_pickable() {
    if (!is_inside_tree())
        return;
    bool is_pickable = pickable && is_visible_in_tree();
    if (area)
        PhysicsServer2D::get_singleton()->area_set_pickable(rid, is_pickable);
    else
        PhysicsServer2D::get_singleton()->body_set_pickable(rid, is_pickable);
}

String CollisionObject2D::get_configuration_warning() const {

    String warning(Node2D::get_configuration_warning());

    if (shapes.empty()) {
        if (!warning.empty()) {
            warning = ("\n\n");
        }
        warning += TTR("This node has no shape, so it can't collide or interact with other objects.\nConsider adding a CollisionShape2D or CollisionPolygon2D as a child to define its shape.");
    }

    return warning;
}

void CollisionObject2D::set_collision_layer(uint32_t p_layer) {
    collision_layer = p_layer;
    if (area) {
        PhysicsServer2D::get_singleton()->area_set_collision_layer(get_rid(), p_layer);
    } else {
        PhysicsServer2D::get_singleton()->body_set_collision_layer(get_rid(), p_layer);
    }
}

uint32_t CollisionObject2D::get_collision_layer() const {
    return collision_layer;
}

void CollisionObject2D::set_collision_mask(uint32_t p_mask) {
    collision_mask = p_mask;
    if (area) {
        PhysicsServer2D::get_singleton()->area_set_collision_mask(get_rid(), p_mask);
    } else {
        PhysicsServer2D::get_singleton()->body_set_collision_mask(get_rid(), p_mask);
    }
}

uint32_t CollisionObject2D::get_collision_mask() const {
    return collision_mask;
}

void CollisionObject2D::set_collision_layer_bit(int p_bit, bool p_value) {
    ERR_FAIL_INDEX_MSG(p_bit, 32, "Collision layer bit must be between 0 and 31 inclusive.");
    uint32_t collision_layer = get_collision_layer();
    if (p_value) {
        collision_layer |= 1 << p_bit;
    } else {
        collision_layer &= ~(1 << p_bit);
    }
    set_collision_layer(collision_layer);
}

bool CollisionObject2D::get_collision_layer_bit(int p_bit) const {
    ERR_FAIL_INDEX_V_MSG(p_bit, 32, false, "Collision layer bit must be between 0 and 31 inclusive.");
    return get_collision_layer() & (1 << p_bit);
}

void CollisionObject2D::set_collision_mask_bit(int p_bit, bool p_value) {
    ERR_FAIL_INDEX_MSG(p_bit, 32, "Collision mask bit must be between 0 and 31 inclusive.");
    uint32_t mask = get_collision_mask();
    if (p_value) {
        mask |= 1 << p_bit;
    } else {
        mask &= ~(1 << p_bit);
    }
    set_collision_mask(mask);
}

bool CollisionObject2D::get_collision_mask_bit(int p_bit) const {
    ERR_FAIL_INDEX_V_MSG(p_bit, 32, false, "Collision mask bit must be between 0 and 31 inclusive.");
    return get_collision_mask() & (1 << p_bit);
}

void CollisionObject2D::_bind_methods() {

    SE_BIND_METHOD(CollisionObject2D,get_rid);

    SE_BIND_METHOD(CollisionObject2D,set_collision_layer);
    SE_BIND_METHOD(CollisionObject2D,get_collision_layer);
    SE_BIND_METHOD(CollisionObject2D,set_collision_mask);
    SE_BIND_METHOD(CollisionObject2D,get_collision_mask);
    SE_BIND_METHOD(CollisionObject2D,set_collision_layer_bit);
    SE_BIND_METHOD(CollisionObject2D,get_collision_layer_bit);
    SE_BIND_METHOD(CollisionObject2D,set_collision_mask_bit);
    SE_BIND_METHOD(CollisionObject2D,get_collision_mask_bit);
    SE_BIND_METHOD(CollisionObject2D,set_pickable);
    SE_BIND_METHOD(CollisionObject2D,is_pickable);
    SE_BIND_METHOD(CollisionObject2D,create_shape_owner);
    SE_BIND_METHOD(CollisionObject2D,remove_shape_owner);
    MethodBinder::bind_method(D_METHOD("get_shape_owners"), &CollisionObject2D::_get_shape_owners);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_set_transform);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_get_transform);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_get_owner);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_set_disabled);
    SE_BIND_METHOD(CollisionObject2D,is_shape_owner_disabled);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_set_one_way_collision);
    SE_BIND_METHOD(CollisionObject2D,is_shape_owner_one_way_collision_enabled);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_set_one_way_collision_margin);
    SE_BIND_METHOD(CollisionObject2D,get_shape_owner_one_way_collision_margin);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_add_shape);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_get_shape_count);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_get_shape);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_get_shape_index);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_remove_shape);
    SE_BIND_METHOD(CollisionObject2D,shape_owner_clear_shapes);
    SE_BIND_METHOD(CollisionObject2D,shape_find_owner);

    BIND_VMETHOD(MethodInfo("_input_event", PropertyInfo(VariantType::OBJECT, "viewport"), PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEvent"), PropertyInfo(VariantType::INT, "shape_idx")));

    ADD_SIGNAL(MethodInfo("input_event", PropertyInfo(VariantType::OBJECT, "viewport", PropertyHint::ResourceType, "Node"), PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEvent"), PropertyInfo(VariantType::INT, "shape_idx")));
    ADD_SIGNAL(MethodInfo("mouse_entered"));
    ADD_SIGNAL(MethodInfo("mouse_exited"));

    ADD_GROUP("Collision", "collision_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_layer", PropertyHint::Layers2DPhysics), "set_collision_layer", "get_collision_layer");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_mask", PropertyHint::Layers2DPhysics), "set_collision_mask", "get_collision_mask");

    ADD_GROUP("AreaInput", "area_input_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "area_input_pickable"), "set_pickable", "is_pickable");
}

CollisionObject2D::CollisionObject2D(RID p_rid, bool p_area) {

    rid = p_rid;
    area = p_area;
    pickable = true;
    set_notify_transform(true);
    total_subshapes = 0;
    only_update_transform_changes = false;

    if (p_area) {

        PhysicsServer2D::get_singleton()->area_attach_object_instance_id(rid, get_instance_id());
    } else {
        PhysicsServer2D::get_singleton()->body_attach_object_instance_id(rid, get_instance_id());
    }
}

CollisionObject2D::CollisionObject2D() {

    //owner=

    set_notify_transform(true);
}

CollisionObject2D::~CollisionObject2D() {

    PhysicsServer2D::get_singleton()->free_rid(rid);
}
