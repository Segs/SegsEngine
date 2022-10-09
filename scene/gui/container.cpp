/*************************************************************************/
/*  container.cpp                                                        */
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

#include "container.h"

#include "core/callable_method_pointer.h"
#include "core/message_queue.h"
#include "core/method_bind.h"
#include "core/translation_helpers.h"
#include "scene/scene_string_names.h"

IMPL_GDCLASS(Container)

void Container::_child_minsize_changed() {
    // Size2 ms = get_combined_minimum_size();
    // if (ms.width > get_size().width || ms.height > get_size().height) {
    minimum_size_changed();
    queue_sort();
}

void Container::add_child_notify(Node *p_child) {
    Control::add_child_notify(p_child);

    Control *control = object_cast<Control>(p_child);
    if (!control) {
        return;
    }

    control->connect("size_flags_changed", callable_mp(this, &ClassName::queue_sort));
    control->connect("minimum_size_changed", callable_mp(this, &ClassName::_child_minsize_changed));
    control->connect("visibility_changed", callable_mp(this, &ClassName::_child_minsize_changed));

    minimum_size_changed();
    queue_sort();
}

void Container::move_child_notify(Node *p_child) {

    Control::move_child_notify(p_child);

    if (!object_cast<Control>(p_child)) {
        return;
    }

    minimum_size_changed();
    queue_sort();
}

void Container::remove_child_notify(Node *p_child) {
    Control::remove_child_notify(p_child);

    Control *control = object_cast<Control>(p_child);
    if (!control)
        return;

    control->disconnect("size_flags_changed", callable_mp(this, &ClassName::queue_sort));
    control->disconnect("minimum_size_changed", callable_mp(this, &ClassName::_child_minsize_changed));
    control->disconnect("visibility_changed", callable_mp(this, &ClassName::_child_minsize_changed));

    minimum_size_changed();
    queue_sort();
}

void Container::_sort_children() {
    if (!is_inside_tree())
        return;

    notification(NOTIFICATION_SORT_CHILDREN);
    emit_signal(SceneStringNames::sort_children);
    pending_sort = false;
}

void Container::fit_child_in_rect(Control *p_child, const Rect2 &p_rect) {
    ERR_FAIL_COND(!p_child);
    ERR_FAIL_COND(p_child->get_parent() != this);

    Size2 minsize = p_child->get_combined_minimum_size();
    Rect2 r = p_rect;

    if (!(p_child->get_h_size_flags() & SIZE_FILL)) {
        r.size.x = minsize.width;
        float pos_x_diff = 0.0f;
        if (p_child->get_h_size_flags() & SIZE_SHRINK_END) {
            pos_x_diff = p_rect.size.width - minsize.width;
        } else if (p_child->get_h_size_flags() & SIZE_SHRINK_CENTER) {
            pos_x_diff = Math::floor((p_rect.size.x - minsize.width) / 2);
        }
        r.position.x += pos_x_diff;
    }

    if (!(p_child->get_v_size_flags() & SIZE_FILL)) {
        r.size.height = minsize.height;
        float pos_y_diff = 0.0f;
        if (p_child->get_v_size_flags() & SIZE_SHRINK_END) {
            pos_y_diff = p_rect.size.height - minsize.height;
        } else if (p_child->get_v_size_flags() & SIZE_SHRINK_CENTER) {
            pos_y_diff = Math::floor((p_rect.size.height - minsize.height) / 2);
        }
        r.position.y += pos_y_diff;
    }

    for (int i = 0; i < 4; i++) {
        p_child->set_anchor(Margin(i), ANCHOR_BEGIN);
    }

    p_child->set_position(r.position);
    p_child->set_size(r.size);
    p_child->set_rotation(0);
    p_child->set_scale(Vector2(1, 1));
}

void Container::queue_sort() {

    if (!is_inside_tree() || pending_sort) {
        return;
    }

    MessageQueue::get_singleton()->push_call(get_instance_id(), [this]() { _sort_children(); });
    pending_sort = true;
}

void Container::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {
            pending_sort = false;
            queue_sort();
        } break;
        case NOTIFICATION_RESIZED: {

            queue_sort();
        } break;
        case NOTIFICATION_THEME_CHANGED: {

            queue_sort();
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {

            if (is_visible_in_tree()) {
                queue_sort();
            }
        } break;
    }
}

String Container::get_configuration_warning() const {

    String warning(Control::get_configuration_warning());

    if (strcmp(get_class(),"Container")==0 && get_script().is_null()) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("Container by itself serves no purpose unless a script configures its children placement behavior.\nIf "
                       "you don't intend to add a script, use a plain Control node instead.");
    }
    return warning;
}

void Container::_bind_methods() {

    BIND_METHOD(Container,_sort_children);
    BIND_METHOD(Container,_child_minsize_changed);

    BIND_METHOD(Container,queue_sort);
    BIND_METHOD(Container,fit_child_in_rect);

    BIND_CONSTANT(NOTIFICATION_SORT_CHILDREN)
    ADD_SIGNAL(MethodInfo("sort_children"));
}

Container::Container() : pending_sort(false) {
    // All containers should let mouse events pass by default.
    set_mouse_filter(MOUSE_FILTER_PASS);
}
