/*************************************************************************/
/*  scroll_container.cpp                                                 */
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

#include "scroll_container.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/input/input_event.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/translation_helpers.h"
#include "scene/main/viewport.h"
#include "scene/resources/style_box.h"

IMPL_GDCLASS(ScrollContainer)

bool ScrollContainer::clips_input() const {

    return true;
}

Size2 ScrollContainer::get_minimum_size() const {

    Ref<StyleBox> sb = get_theme_stylebox("bg");
    Size2 min_size;

    for (int i = 0; i < get_child_count(); i++) {

        Control *c = object_cast<Control>(get_child(i));
        if (!c)
            continue;
        if (c->is_set_as_top_level())
            continue;
        if (c == h_scroll || c == v_scroll)
            continue;
        Size2 minsize = c->get_combined_minimum_size();

        if (!scroll_h) {
            min_size.x = M_MAX(min_size.x, minsize.x);
        }
        if (!scroll_v) {
            min_size.y = M_MAX(min_size.y, minsize.y);
        }
    }

    if (h_scroll->is_visible_in_tree()) {
        min_size.y += h_scroll->get_minimum_size().y;
    }
    if (v_scroll->is_visible_in_tree()) {
        min_size.x += v_scroll->get_minimum_size().x;
    }
    min_size += sb->get_minimum_size();
    return min_size;
}

void ScrollContainer::_cancel_drag() {
    set_physics_process_internal(false);
    drag_touching_deaccel = false;
    drag_touching = false;
    drag_speed = Vector2();
    drag_accum = Vector2();
    last_drag_accum = Vector2();
    drag_from = Vector2();

    if (beyond_deadzone) {
        emit_signal("scroll_ended");
        propagate_notification(NOTIFICATION_SCROLL_END);
        beyond_deadzone = false;
    }
}

void ScrollContainer::_gui_input(const Ref<InputEvent> &p_gui_input) {

    double prev_v_scroll = v_scroll->get_value();
    double prev_h_scroll = h_scroll->get_value();

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_gui_input);

    if (mb) {

        if (mb->get_button_index() == BUTTON_WHEEL_UP && mb->is_pressed()) {
            // only horizontal is enabled, scroll horizontally
            if (h_scroll->is_visible() && (!v_scroll->is_visible() || mb->get_shift())) {
                h_scroll->set_value(h_scroll->get_value() - h_scroll->get_page() / 8 * mb->get_factor());
            } else if (v_scroll->is_visible_in_tree()) {
                v_scroll->set_value(v_scroll->get_value() - v_scroll->get_page() / 8 * mb->get_factor());
            }
        }

        if (mb->get_button_index() == BUTTON_WHEEL_DOWN && mb->is_pressed()) {
            // only horizontal is enabled, scroll horizontally
            if (h_scroll->is_visible() && (!v_scroll->is_visible() || mb->get_shift())) {
                h_scroll->set_value(h_scroll->get_value() + h_scroll->get_page() / 8 * mb->get_factor());
            } else if (v_scroll->is_visible()) {
                v_scroll->set_value(v_scroll->get_value() + v_scroll->get_page() / 8 * mb->get_factor());
            }
        }

        if (mb->get_button_index() == BUTTON_WHEEL_LEFT && mb->is_pressed()) {
            if (h_scroll->is_visible_in_tree()) {
                h_scroll->set_value(h_scroll->get_value() - h_scroll->get_page() * mb->get_factor() / 8);
            }
        }

        if (mb->get_button_index() == BUTTON_WHEEL_RIGHT && mb->is_pressed()) {
            if (h_scroll->is_visible_in_tree()) {
                h_scroll->set_value(h_scroll->get_value() + h_scroll->get_page() * mb->get_factor() / 8);
            }
        }

        if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll)
            accept_event(); //accept event if scroll changed

        if (!OS::get_singleton()->has_touchscreen_ui_hint())
            return;

        if (mb->get_button_index() != BUTTON_LEFT)
            return;

        if (mb->is_pressed()) {

            if (drag_touching) {
                _cancel_drag();
            }

            drag_speed = Vector2();
            drag_accum = Vector2();
            last_drag_accum = Vector2();
            drag_from = Vector2(h_scroll->get_value(), v_scroll->get_value());
            drag_touching = OS::get_singleton()->has_touchscreen_ui_hint();
            drag_touching_deaccel = false;
            beyond_deadzone = false;
            time_since_motion = 0;
            if (drag_touching) {
                set_physics_process_internal(true);
                time_since_motion = 0;
            }

        } else {
            if (drag_touching) {

                if (drag_speed == Vector2()) {
                    _cancel_drag();
                } else {

                    drag_touching_deaccel = true;
                }
            }
        }
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_gui_input);

    if (mm) {

        if (drag_touching && !drag_touching_deaccel) {

            Vector2 motion = Vector2(mm->get_relative().x, mm->get_relative().y);
            drag_accum -= motion;

            if (beyond_deadzone || (scroll_h && Math::abs(drag_accum.x) > deadzone) || (scroll_v && Math::abs(drag_accum.y) > deadzone)) {
                if (!beyond_deadzone) {
                    propagate_notification(NOTIFICATION_SCROLL_BEGIN);
                    emit_signal("scroll_started");

                    beyond_deadzone = true;
                    // resetting drag_accum here ensures smooth scrolling after reaching deadzone
                    drag_accum = -motion;
                }
                Vector2 diff = drag_from + drag_accum;
                if (scroll_h)
                    h_scroll->set_value(diff.x);
                else
                    drag_accum.x = 0;
                if (scroll_v)
                    v_scroll->set_value(diff.y);
                else
                    drag_accum.y = 0;
                time_since_motion = 0;
            }
        }
    }

    Ref<InputEventPanGesture> pan_gesture = dynamic_ref_cast<InputEventPanGesture>(p_gui_input);
    if (pan_gesture) {

        if (h_scroll->is_visible_in_tree()) {
            h_scroll->set_value(h_scroll->get_value() + h_scroll->get_page() * pan_gesture->get_delta().x / 8);
        }
        if (v_scroll->is_visible_in_tree()) {
            v_scroll->set_value(v_scroll->get_value() + v_scroll->get_page() * pan_gesture->get_delta().y / 8);
        }
    }

    if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll)
        accept_event(); //accept event if scroll changed
}

void ScrollContainer::_update_scrollbar_position() {

    Size2 hmin = h_scroll->get_combined_minimum_size();
    Size2 vmin = v_scroll->get_combined_minimum_size();

    h_scroll->set_anchor_and_margin(Margin::Left, ANCHOR_BEGIN, 0);
    h_scroll->set_anchor_and_margin(Margin::Right, ANCHOR_END, 0);
    h_scroll->set_anchor_and_margin(Margin::Top, ANCHOR_END, -hmin.height);
    h_scroll->set_anchor_and_margin(Margin::Bottom, ANCHOR_END, 0);

    v_scroll->set_anchor_and_margin(Margin::Left, ANCHOR_END, -vmin.width);
    v_scroll->set_anchor_and_margin(Margin::Right, ANCHOR_END, 0);
    v_scroll->set_anchor_and_margin(Margin::Top, ANCHOR_BEGIN, 0);
    v_scroll->set_anchor_and_margin(Margin::Bottom, ANCHOR_END, 0);

    h_scroll->raise();
    v_scroll->raise();
}

void ScrollContainer::_gui_focus_changed(Control *p_control) {
    if (follow_focus && is_a_parent_of(p_control)) {
        ensure_control_visible(p_control);
    }
    }

void ScrollContainer::ensure_control_visible(Control *p_control) {
    ERR_FAIL_COND_MSG(!is_a_parent_of(p_control), "Must be a parent of the control.");
        Rect2 global_rect = get_global_rect();
        Rect2 other_rect = p_control->get_global_rect();
    float right_margin = v_scroll->is_visible() ? v_scroll->get_size().x : 0.0f;
    float bottom_margin = h_scroll->is_visible() ? h_scroll->get_size().y : 0.0f;

    Vector2 diff = Vector2(M_MAX(MIN(other_rect.position.x, global_rect.position.x), other_rect.position.x + other_rect.size.x - global_rect.size.x + right_margin),
            M_MAX(MIN(other_rect.position.y, global_rect.position.y), other_rect.position.y + other_rect.size.y - global_rect.size.y + bottom_margin));

    set_h_scroll(get_h_scroll() + (diff.x - global_rect.position.x));
    set_v_scroll(get_v_scroll() + (diff.y - global_rect.position.y));
}


void ScrollContainer::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        call_deferred([this]() { _update_scrollbar_position(); });
    }

    if (p_what == NOTIFICATION_READY) {
        Viewport *viewport = get_viewport();
        ERR_FAIL_COND(!viewport);
        viewport->connect("gui_focus_changed", callable_mp(this, &ClassName::_gui_focus_changed));
    }

    if (p_what == NOTIFICATION_SORT_CHILDREN) {

        child_max_size = Size2(0, 0);
        Size2 size = get_size();
        Point2 ofs;

        Ref<StyleBox> sb = get_theme_stylebox("bg");
        size -= sb->get_minimum_size();
        ofs += sb->get_offset();

        if (h_scroll->is_visible_in_tree() && h_scroll->get_parent() == this) //scrolls may have been moved out for reasons
            size.y -= h_scroll->get_minimum_size().y;

        if (v_scroll->is_visible_in_tree() && v_scroll->get_parent() == this) //scrolls may have been moved out for reasons
            size.x -= v_scroll->get_minimum_size().x;

        for (int i = 0; i < get_child_count(); i++) {

            Control *c = object_cast<Control>(get_child(i));
            if (!c)
                continue;
            if (c->is_set_as_top_level())
                continue;
            if (c == h_scroll || c == v_scroll)
                continue;
            Size2 minsize = c->get_combined_minimum_size();
            child_max_size.x = M_MAX(child_max_size.x, minsize.x);
            child_max_size.y = M_MAX(child_max_size.y, minsize.y);

            Rect2 r = Rect2(-scroll, minsize);
            if (!scroll_h || (!h_scroll->is_visible_in_tree() && c->get_h_size_flags() & SIZE_EXPAND)) {
                r.position.x = 0;
                if (c->get_h_size_flags() & SIZE_EXPAND)
                    r.size.width = M_MAX(size.width, minsize.width);
                else
                    r.size.width = minsize.width;
            }
            if (!scroll_v || (!v_scroll->is_visible_in_tree() && c->get_v_size_flags() & SIZE_EXPAND)) {
                r.position.y = 0;
                if (c->get_v_size_flags() & SIZE_EXPAND)
                    r.size.height = M_MAX(size.height, minsize.height);
                else
                    r.size.height = minsize.height;
            }
            r.position += ofs;
            fit_child_in_rect(c, r);
        }
        update();
    };

    if (p_what == NOTIFICATION_DRAW) {

        Ref<StyleBox> sb = get_theme_stylebox("bg");
        draw_style_box(sb, Rect2(Vector2(), get_size()));

        update_scrollbars();
    }

    if (p_what == NOTIFICATION_INTERNAL_PHYSICS_PROCESS) {

        if (drag_touching) {

            if (drag_touching_deaccel) {

                Vector2 pos = Vector2(h_scroll->get_value(), v_scroll->get_value());
                pos += drag_speed * get_physics_process_delta_time();

                bool turnoff_h = false;
                bool turnoff_v = false;

                if (pos.x < 0) {
                    pos.x = 0;
                    turnoff_h = true;
                }
                if (pos.x > (h_scroll->get_max() - h_scroll->get_page())) {
                    pos.x = h_scroll->get_max() - h_scroll->get_page();
                    turnoff_h = true;
                }

                if (pos.y < 0) {
                    pos.y = 0;
                    turnoff_v = true;
                }
                if (pos.y > (v_scroll->get_max() - v_scroll->get_page())) {
                    pos.y = v_scroll->get_max() - v_scroll->get_page();
                    turnoff_v = true;
                }

                if (scroll_h)
                    h_scroll->set_value(pos.x);
                if (scroll_v)
                    v_scroll->set_value(pos.y);

                float sgn_x = drag_speed.x < 0 ? -1 : 1;
                float val_x = Math::abs(drag_speed.x);
                val_x -= 1000 * get_physics_process_delta_time();

                if (val_x < 0) {
                    turnoff_h = true;
                }

                float sgn_y = drag_speed.y < 0 ? -1 : 1;
                float val_y = Math::abs(drag_speed.y);
                val_y -= 1000 * get_physics_process_delta_time();

                if (val_y < 0) {
                    turnoff_v = true;
                }

                drag_speed = Vector2(sgn_x * val_x, sgn_y * val_y);

                if (turnoff_h && turnoff_v) {
                    _cancel_drag();
                }

            } else {

                if (time_since_motion == 0 || time_since_motion > 0.1) {

                    Vector2 diff = drag_accum - last_drag_accum;
                    last_drag_accum = drag_accum;
                    drag_speed = diff / get_physics_process_delta_time();
                }

                time_since_motion += get_physics_process_delta_time();
            }
        }
    }
}

void ScrollContainer::update_scrollbars() {

    Size2 size = get_size();
    Ref<StyleBox> sb = get_theme_stylebox("bg");
    size -= sb->get_minimum_size();

    Size2 hmin;
    Size2 vmin;
    if (scroll_h) {
        hmin = h_scroll->get_combined_minimum_size();
    }
    if (scroll_v) {
        vmin = v_scroll->get_combined_minimum_size();
    }

    Size2 min = child_max_size;

    bool hide_scroll_v = !scroll_v || min.height <= size.height;
    bool hide_scroll_h = !scroll_h || min.width <= size.width;

    v_scroll->set_max(min.height);
    if (hide_scroll_v) {
        v_scroll->set_page(size.height);
        v_scroll->hide();
        scroll.y = 0;
    } else {

        v_scroll->show();
        if (hide_scroll_h) {
            v_scroll->set_page(size.height);
        } else {
        v_scroll->set_page(size.height - hmin.height);
        }
        scroll.y = v_scroll->get_value();
    }

    h_scroll->set_max(min.width);
    if (hide_scroll_h) {
        h_scroll->set_page(size.width);
        h_scroll->hide();
        scroll.x = 0;
    } else {

        h_scroll->show();
        if (hide_scroll_v) {
            h_scroll->set_page(size.width);
        } else {
        h_scroll->set_page(size.width - vmin.width);
        }
        scroll.x = h_scroll->get_value();
    }
    // Avoid scrollbar overlapping.
    h_scroll->set_anchor_and_margin(Margin::Right, ANCHOR_END, hide_scroll_v ? 0 : -vmin.width);
    v_scroll->set_anchor_and_margin(Margin::Bottom, ANCHOR_END, hide_scroll_h ? 0 : -hmin.height);
}

void ScrollContainer::_scroll_moved(float) {

    scroll.x = h_scroll->get_value();
    scroll.y = v_scroll->get_value();
    queue_sort();

    update();
}

void ScrollContainer::set_enable_h_scroll(bool p_enable) {
    if (scroll_h == p_enable) {
        return;
    }

    scroll_h = p_enable;
    minimum_size_changed();
    queue_sort();
}

bool ScrollContainer::is_h_scroll_enabled() const {

    return scroll_h;
}

void ScrollContainer::set_enable_v_scroll(bool p_enable) {
    if (scroll_v == p_enable) {
        return;
    }

    scroll_v = p_enable;
    minimum_size_changed();
    queue_sort();
}

bool ScrollContainer::is_v_scroll_enabled() const {

    return scroll_v;
}

int ScrollContainer::get_v_scroll() const {

    return v_scroll->get_value();
}
void ScrollContainer::set_v_scroll(int p_pos) {

    v_scroll->set_value(p_pos);
    _cancel_drag();
}

int ScrollContainer::get_h_scroll() const {

    return h_scroll->get_value();
}
void ScrollContainer::set_h_scroll(int p_pos) {

    h_scroll->set_value(p_pos);
    _cancel_drag();
}

int ScrollContainer::get_deadzone() const {
    return deadzone;
}

void ScrollContainer::set_deadzone(int p_deadzone) {
    deadzone = p_deadzone;
}

bool ScrollContainer::is_following_focus() const {
    return follow_focus;
}

void ScrollContainer::set_follow_focus(bool p_follow) {
    follow_focus = p_follow;
}

String ScrollContainer::get_configuration_warning() const {

    String warning = Control::get_configuration_warning();

    int found = 0;

    for (int i = 0; i < get_child_count(); i++) {

        Control *c = object_cast<Control>(get_child(i));
        if (!c)
            continue;
        if (c->is_set_as_top_level())
            continue;
        if (c == h_scroll || c == v_scroll)
            continue;

        found++;
    }

    if (found != 1) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTRS("ScrollContainer is intended to work with a single child control.\nUse a container as child (VBox, HBox, etc.), or a Control and set the custom minimum size manually.");
    }

    return warning;
}

HScrollBar *ScrollContainer::get_h_scrollbar() {

    return h_scroll;
}

VScrollBar *ScrollContainer::get_v_scrollbar() {

    return v_scroll;
}

void ScrollContainer::_bind_methods() {

    SE_BIND_METHOD(ScrollContainer,_gui_input);
    SE_BIND_METHOD(ScrollContainer,set_enable_h_scroll);
    SE_BIND_METHOD(ScrollContainer,is_h_scroll_enabled);
    SE_BIND_METHOD(ScrollContainer,set_enable_v_scroll);
    SE_BIND_METHOD(ScrollContainer,is_v_scroll_enabled);

    SE_BIND_METHOD(ScrollContainer,set_h_scroll);
    SE_BIND_METHOD(ScrollContainer,get_h_scroll);
    SE_BIND_METHOD(ScrollContainer,set_v_scroll);
    SE_BIND_METHOD(ScrollContainer,get_v_scroll);
    SE_BIND_METHOD(ScrollContainer,set_deadzone);
    SE_BIND_METHOD(ScrollContainer,get_deadzone);
    SE_BIND_METHOD(ScrollContainer,set_follow_focus);
    SE_BIND_METHOD(ScrollContainer,is_following_focus);


    SE_BIND_METHOD(ScrollContainer,get_h_scrollbar);
    SE_BIND_METHOD(ScrollContainer,get_v_scrollbar);
    SE_BIND_METHOD(ScrollContainer,ensure_control_visible);

    ADD_SIGNAL(MethodInfo("scroll_started"));
    ADD_SIGNAL(MethodInfo("scroll_ended"));

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "follow_focus"), "set_follow_focus", "is_following_focus");

    ADD_GROUP("Scroll", "scroll_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "scroll_horizontal_enabled"), "set_enable_h_scroll", "is_h_scroll_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "scroll_horizontal"), "set_h_scroll", "get_h_scroll");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "scroll_vertical_enabled"), "set_enable_v_scroll", "is_v_scroll_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "scroll_vertical"), "set_v_scroll", "get_v_scroll");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "scroll_deadzone"), "set_deadzone", "get_deadzone");

    GLOBAL_DEF("gui/common/default_scroll_deadzone", 0);
}

ScrollContainer::ScrollContainer() {

    h_scroll = memnew(HScrollBar);
    h_scroll->set_name("_h_scroll");
    add_child(h_scroll);
    h_scroll->connect("value_changed",callable_mp(this, &ClassName::_scroll_moved));

    v_scroll = memnew(VScrollBar);
    v_scroll->set_name("_v_scroll");
    add_child(v_scroll);
    v_scroll->connect("value_changed",callable_mp(this, &ClassName::_scroll_moved));


    drag_speed = Vector2();
    drag_touching = false;
    drag_touching_deaccel = false;
    beyond_deadzone = false;
    scroll_h = true;
    scroll_v = true;

    deadzone = T_GLOBAL_GET<int>("gui/common/default_scroll_deadzone");
    follow_focus = false;

    set_clip_contents(true);
}
