/*************************************************************************/
/*  remote_transform_2d.cpp                                              */
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

#include "remote_transform_2d.h"
#include "scene/scene_string_names.h"
#include "core/object_db.h"
#include "core/method_bind.h"
#include "core/translation_helpers.h"

IMPL_GDCLASS(RemoteTransform2D)

void RemoteTransform2D::_update_cache() {

    cache = entt::null;
    if (has_node(remote_node)) {
        Node *node = get_node(remote_node);
        if (!node || this == node || node->is_a_parent_of(this) || this->is_a_parent_of(node)) {
            return;
        }

        cache = node->get_instance_id();
    }
}

void RemoteTransform2D::_update_remote() {

    if (!is_inside_tree())
        return;

    if (cache==entt::null)
        return;

    Node2D *n = object_cast<Node2D>(object_for_entity(cache));
    if (!n)
        return;

    if (!n->is_inside_tree())
        return;

    //todo make faster
    if (use_global_coordinates) {

        if (update_remote_position && update_remote_rotation && update_remote_scale) {
            n->set_global_transform(get_global_transform());
        } else {
            Transform2D n_trans = n->get_global_transform();
            Transform2D our_trans = get_global_transform();
            Vector2 n_scale = n->get_scale();

            if (!update_remote_position)
                our_trans.set_origin(n_trans.get_origin());
            if (!update_remote_rotation)
                our_trans.set_rotation(n_trans.get_rotation());

            n->set_global_transform(our_trans);

            if (update_remote_scale)
                n->set_scale(get_global_scale());
            else
                n->set_scale(n_scale);
        }

    } else {

        if (update_remote_position && update_remote_rotation && update_remote_scale) {
            n->set_transform(get_transform());
        } else {
            Transform2D n_trans = n->get_transform();
            Transform2D our_trans = get_transform();
            Vector2 n_scale = n->get_scale();

            if (!update_remote_position)
                our_trans.set_origin(n_trans.get_origin());
            if (!update_remote_rotation)
                our_trans.set_rotation(n_trans.get_rotation());

            n->set_transform(our_trans);

            if (update_remote_scale)
                n->set_scale(get_scale());
            else
                n->set_scale(n_scale);
        }
    }
}

void RemoteTransform2D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {

            _update_cache();

        } break;
        case NOTIFICATION_TRANSFORM_CHANGED: {
            if (!is_inside_tree())
                break;

            if (cache!=entt::null) {

                _update_remote();
            }

        } break;
    }
}

void RemoteTransform2D::set_remote_node(const NodePath &p_remote_node) {

    remote_node = p_remote_node;
    if (is_inside_tree()) {
        _update_cache();
        _update_remote();
    }

    update_configuration_warning();
}

NodePath RemoteTransform2D::get_remote_node() const {

    return remote_node;
}

void RemoteTransform2D::set_use_global_coordinates(const bool p_enable) {
    use_global_coordinates = p_enable;
    _update_remote();
}

bool RemoteTransform2D::get_use_global_coordinates() const {
    return use_global_coordinates;
}

void RemoteTransform2D::set_update_position(const bool p_update) {
    update_remote_position = p_update;
    _update_remote();
}

bool RemoteTransform2D::get_update_position() const {
    return update_remote_position;
}

void RemoteTransform2D::set_update_rotation(const bool p_update) {
    update_remote_rotation = p_update;
    _update_remote();
}

bool RemoteTransform2D::get_update_rotation() const {
    return update_remote_rotation;
}

void RemoteTransform2D::set_update_scale(const bool p_update) {
    update_remote_scale = p_update;
    _update_remote();
}

bool RemoteTransform2D::get_update_scale() const {
    return update_remote_scale;
}

void RemoteTransform2D::force_update_cache() {
    _update_cache();
}

String RemoteTransform2D::get_configuration_warning() const {

    String warning = BaseClassName::get_configuration_warning();
    if (!has_node(remote_node) || !object_cast<Node2D>(get_node(remote_node))) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("Path property must point to a valid Node2D node to work.");
    }

    return warning;
}

void RemoteTransform2D::_bind_methods() {

    BIND_METHOD(RemoteTransform2D,set_remote_node);
    BIND_METHOD(RemoteTransform2D,get_remote_node);
    BIND_METHOD(RemoteTransform2D,force_update_cache);

    BIND_METHOD(RemoteTransform2D,set_use_global_coordinates);
    BIND_METHOD(RemoteTransform2D,get_use_global_coordinates);

    BIND_METHOD(RemoteTransform2D,set_update_position);
    BIND_METHOD(RemoteTransform2D,get_update_position);
    BIND_METHOD(RemoteTransform2D,set_update_rotation);
    BIND_METHOD(RemoteTransform2D,get_update_rotation);
    BIND_METHOD(RemoteTransform2D,set_update_scale);
    BIND_METHOD(RemoteTransform2D,get_update_scale);

    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "remote_path", PropertyHint::NodePathValidTypes, "Node2D"), "set_remote_node", "get_remote_node");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_global_coordinates"), "set_use_global_coordinates", "get_use_global_coordinates");

    ADD_GROUP("Update", "update_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "update_position"), "set_update_position", "get_update_position");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "update_rotation"), "set_update_rotation", "get_update_rotation");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "update_scale"), "set_update_scale", "get_update_scale");
}

RemoteTransform2D::RemoteTransform2D() {

    use_global_coordinates = true;
    update_remote_position = true;
    update_remote_rotation = true;
    update_remote_scale = true;

    cache = entt::null;
    set_notify_transform(true);
}
