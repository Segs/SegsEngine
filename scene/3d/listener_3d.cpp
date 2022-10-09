/*************************************************************************/
/*  listener.cpp                                                         */
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

#include "listener_3d.h"

#include "core/method_bind.h"
#include "scene/resources/mesh.h"
#include "scene/main/scene_tree.h"

IMPL_GDCLASS(Listener3D)

void Listener3D::_update_audio_listener_state() {}

void Listener3D::_request_listener_update() {

    _update_listener();
}

bool Listener3D::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "current") {
        if (p_value.as<bool>()) {
            make_current();
        } else {
            clear_current();
        }
    } else
        return false;

    return true;
}
bool Listener3D::_get(const StringName &p_name, Variant &r_ret) const {

    if (p_name == "current") {
        if (is_inside_tree() && get_tree()->is_node_being_edited(this)) {
            r_ret = current;
        } else {
            r_ret = is_current();
        }
    } else
        return false;

    return true;
}

void Listener3D::_get_property_list(Vector<PropertyInfo> *p_list) const {

    p_list->push_back(PropertyInfo(VariantType::BOOL, "current"));
}

void Listener3D::_update_listener() {

    if (is_inside_tree() && is_current()) {
        get_viewport()->_listener_transform_changed_notify();
    }
}

void Listener3D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_WORLD: {
            bool first_listener = get_viewport()->_listener_add(this);
            if (!get_tree()->is_node_being_edited(this) && (current || first_listener))
                make_current();
        } break;
        case NOTIFICATION_TRANSFORM_CHANGED: {
            _request_listener_update();
        } break;
        case NOTIFICATION_EXIT_WORLD: {

            if (!get_tree()->is_node_being_edited(this)) {
                if (is_current()) {
                    clear_current();
                    current = true; //keep it true

                } else {
                    current = false;
                }
            }

            get_viewport()->_listener_remove(this);

        } break;
    }
}

Transform Listener3D::get_listener_transform() const {

    return get_global_transform().orthonormalized();
}

void Listener3D::make_current() {

    current = true;

    if (!is_inside_tree())
        return;

    get_viewport()->_listener_set(this);
}

void Listener3D::clear_current() {

    current = false;
    if (!is_inside_tree())
        return;

    if (get_viewport()->get_listener() == this) {
        get_viewport()->_listener_set(nullptr);
        get_viewport()->_listener_make_next_current(this);
    }
}

bool Listener3D::is_current() const {

    if (is_inside_tree() && !get_tree()->is_node_being_edited(this)) {

        return get_viewport()->get_listener() == this;
    } else
        return current;

    return false;
}


void Listener3D::_bind_methods() {

    BIND_METHOD(Listener3D,make_current);
    BIND_METHOD(Listener3D,clear_current);
    BIND_METHOD(Listener3D,is_current);
    BIND_METHOD(Listener3D,get_listener_transform);
}

Listener3D::Listener3D() {

    current = false;
    force_change = false;
    set_notify_transform(true);
}

Listener3D::~Listener3D() {
}
