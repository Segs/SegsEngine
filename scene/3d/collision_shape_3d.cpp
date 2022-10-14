/*************************************************************************/
/*  collision_shape.cpp                                                  */
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

#include "collision_shape_3d.h"
#include "core/math/quick_hull.h"
#include "mesh_instance_3d.h"
#include "physics_body_3d.h"

#include "core/method_bind.h"
#include "core/callable_method_pointer.h"
#include "core/translation_helpers.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/box_shape_3d.h"
#include "scene/resources/capsule_shape_3d.h"
#include "scene/resources/concave_polygon_shape_3d.h"
#include "scene/resources/convex_polygon_shape_3d.h"
#include "scene/resources/plane_shape.h"
#include "scene/resources/ray_shape_3d.h"
#include "scene/resources/sphere_shape_3d.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(CollisionShape3D)

void CollisionShape3D::make_convex_from_brothers() {

    Node *p = get_parent();
    if (!p) {
        return;
    }

    for (int i = 0; i < p->get_child_count(); i++) {

        Node *n = p->get_child(i);
        MeshInstance3D *mi = object_cast<MeshInstance3D>(n);
        if (mi) {

            Ref<Mesh> m = mi->get_mesh();
            if (m) {

                Ref<Shape> s = m->create_convex_shape();
                set_shape(s);
            }
        }
    }
}

void CollisionShape3D::_update_in_shape_owner(bool p_xform_only) {
    parent->shape_owner_set_transform(owner_id, get_transform());
    if (p_xform_only) {
        return;
    }
    parent->shape_owner_set_disabled(owner_id, disabled);
}

void CollisionShape3D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_PARENTED: {
            parent = object_cast<CollisionObject3D>(get_parent());
            if (parent) {
                owner_id = parent->create_shape_owner(this);
                if (shape) {
                    parent->shape_owner_add_shape(owner_id, shape);
                }
            }
            _update_in_shape_owner();
        } break;
        case NOTIFICATION_ENTER_TREE: {
            if (parent) {
                _update_in_shape_owner();
            }
        } break;
        case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
            if (parent) {
                _update_in_shape_owner(true);
            }
        } break;
        case NOTIFICATION_UNPARENTED: {
            if (parent) {
                parent->remove_shape_owner(owner_id);
            }
            owner_id = 0;
            parent = nullptr;
        } break;
    }
}

void CollisionShape3D::resource_changed(const RES& res) {

    update_gizmo();
}

String CollisionShape3D::get_configuration_warning() const {

    String warning = BaseClassName::get_configuration_warning();

    if (!object_cast<CollisionObject3D>(get_parent())) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("CollisionShape only serves to provide a collision shape to a CollisionObject derived node. Please only use it as a child of Area, StaticBody, RigidBody, KinematicBody, etc. to give them a shape.");
    }

    if (!shape) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("A shape must be provided for CollisionShape to function. Please create a shape resource for it.");
    } else {
        if (shape->is_class("PlaneShape")) {
            if (!warning.empty()) {
                warning += "\n\n";
            }
            warning += TTR("Plane shapes don't work well and will be removed in future versions. Please don't use them.");
        }

        if (object_cast<RigidBody>(get_parent()) &&
                dynamic_ref_cast<ConcavePolygonShape3D>(shape) &&
                object_cast<RigidBody>(get_parent())->get_mode() != RigidBody::MODE_STATIC) {
            if (!warning.empty()) {
                warning += "\n\n";
            }
            warning += TTR("ConcavePolygonShape doesn't support RigidBody in another mode than static.");
        }
    }

    return warning;
}

void CollisionShape3D::_bind_methods() {

    //not sure if this should do anything
    BIND_METHOD(CollisionShape3D,resource_changed);
    BIND_METHOD(CollisionShape3D,set_shape);
    BIND_METHOD(CollisionShape3D,get_shape);
    BIND_METHOD(CollisionShape3D,set_disabled);
    BIND_METHOD(CollisionShape3D,is_disabled);
    MethodBinder::bind_method(D_METHOD("make_convex_from_brothers"), &CollisionShape3D::make_convex_from_brothers,METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);


    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "shape", PropertyHint::ResourceType, "Shape"), "set_shape", "get_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "disabled"), "set_disabled", "is_disabled");
}

void CollisionShape3D::set_shape(const Ref<Shape> &p_shape) {
    if (p_shape == shape) {
        return;
    }
    if (shape) {
        shape->unregister_owner(this);
    }
    shape = p_shape;
    if (shape) {
        shape->register_owner(this);
    }
    update_gizmo();
    if (parent) {
        parent->shape_owner_clear_shapes(owner_id);
        if (shape) {
            parent->shape_owner_add_shape(owner_id, shape);
        }
    }

    if (is_inside_tree() && parent) {
        // If this is a heightfield shape our center may have changed
        _update_in_shape_owner(true);
    }
    update_configuration_warning();
}

void CollisionShape3D::set_disabled(bool p_disabled) {

    disabled = p_disabled;
    update_gizmo();
    if (parent) {
        parent->shape_owner_set_disabled(owner_id, p_disabled);
    }
}

CollisionShape3D::CollisionShape3D() {

    //indicator = RenderingServer::get_singleton()->mesh_create();
    disabled = false;
    parent = nullptr;
    owner_id = 0;
    set_notify_local_transform(true);
}

CollisionShape3D::~CollisionShape3D() {
    if (shape)
        shape->unregister_owner(this);
    //RenderingServer::get_singleton()->free(indicator);
}
