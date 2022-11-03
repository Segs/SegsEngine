/*************************************************************************/
/*  slider.cpp                                                           */
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

#include "slider.h"

#include "core/method_bind.h"
#include "core/input/input_event.h"
#include "core/os/keyboard.h"
#include "scene/resources/style_box.h"

IMPL_GDCLASS(Slider)
IMPL_GDCLASS(HSlider)
IMPL_GDCLASS(VSlider)

Size2 Slider::get_minimum_size() const {

    Ref<StyleBox> style = get_theme_stylebox("slider");
    Size2i ss = style->get_minimum_size() + style->get_center_size();

    Ref<Texture> grabber = get_theme_icon("grabber");
    Size2i rs = grabber->get_size();

    if (orientation == HORIZONTAL)
        return Size2i(ss.width, M_MAX(ss.height, rs.height));
    else
        return Size2i(M_MAX(ss.width, rs.width), ss.height);
}

void Slider::_gui_input(Ref<InputEvent> p_event) {

    if (!editable) {
        return;
    }

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);

    if (mb) {
        if (!mb->is_pressed()) {
        if (mb->get_button_index() == BUTTON_LEFT) {
                // left mouse button released.
                grab.active = false;
                const bool value_changed = !Math::is_equal_approx(grab.uvalue, get_as_ratio());
                emit_signal("drag_ended", value_changed);
            }
            return;
        }

        if (mb->get_button_index() != BUTTON_LEFT) {
            if (scrollable) {
                if (mb->get_button_index() == BUTTON_WHEEL_UP) {
                    grab_focus();
                    set_value(get_value() + get_step());
                } else if (mb->get_button_index() == BUTTON_WHEEL_DOWN) {
                    grab_focus();
                    set_value(get_value() - get_step());
                }
            }
            return;
        }
        Ref<Texture> grabber = get_theme_icon(mouse_inside || has_focus() ?
                                                  StringName("grabber_highlight") :
                                                  StringName("grabber"));
                grab.pos = orientation == VERTICAL ? mb->get_position().y : mb->get_position().x;

                double grab_width = (double)grabber->get_size().width;
                double grab_height = (double)grabber->get_size().height;
                double max = orientation == VERTICAL ? get_size().height - grab_height : get_size().width - grab_width;
                if (orientation == VERTICAL)
                    set_as_ratio(1 - (((double)grab.pos - (grab_height / 2.0)) / max));
                else
                    set_as_ratio(((double)grab.pos - (grab_width / 2.0)) / max);
                grab.active = true;
                grab.uvalue = get_as_ratio();
        emit_signal("drag_started");
        return;
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);

    if (mm) {
        if (grab.active) {

            Size2i size = get_size();
            Ref<Texture> grabber = get_theme_icon("grabber");
            float motion = (orientation == VERTICAL ? mm->get_position().y : mm->get_position().x) - grab.pos;
            if (orientation == VERTICAL)
                motion = -motion;
            float areasize = orientation == VERTICAL ? size.height - grabber->get_size().height : size.width - grabber->get_size().width;
            if (areasize <= 0)
                return;
            float umotion = motion / float(areasize);
            set_as_ratio(grab.uvalue + umotion);
        }
    }

    if (not mm && not mb) {

        if (p_event->is_action_pressed("ui_left", true)) {

            if (orientation != HORIZONTAL)
                return;
            set_value(get_value() - (custom_step >= 0 ? custom_step : get_step()));
            accept_event();
        } else if (p_event->is_action_pressed("ui_right", true)) {

            if (orientation != HORIZONTAL)
                return;
            set_value(get_value() + (custom_step >= 0 ? custom_step : get_step()));
            accept_event();
        } else if (p_event->is_action_pressed("ui_up", true)) {

            if (orientation != VERTICAL)
                return;

            set_value(get_value() + (custom_step >= 0 ? custom_step : get_step()));
            accept_event();
        } else if (p_event->is_action_pressed("ui_down", true)) {

            if (orientation != VERTICAL)
                return;
            set_value(get_value() - (custom_step >= 0 ? custom_step : get_step()));
            accept_event();
        } else if (p_event->is_action("ui_home") && p_event->is_pressed()) {

            set_value(get_min());
            accept_event();
        } else if (p_event->is_action("ui_end") && p_event->is_pressed()) {

            set_value(get_max());
            accept_event();
        }
    }
}

void Slider::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_THEME_CHANGED: {

            minimum_size_changed();
            update();
        } break;
        case NOTIFICATION_MOUSE_ENTER: {

            mouse_inside = true;
            update();
        } break;
        case NOTIFICATION_MOUSE_EXIT: {

            mouse_inside = false;
            update();
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: // fallthrough
        case NOTIFICATION_EXIT_TREE: {

            mouse_inside = false;
            grab.active = false;
        } break;
        case NOTIFICATION_DRAW: {
            RenderingEntity ci = get_canvas_item();
            Size2i size = get_size();
            Ref<StyleBox> style = get_theme_stylebox("slider");
            bool highlighted = mouse_inside || has_focus();
            Ref<StyleBox> grabber_area = get_theme_stylebox(highlighted ? StringName("grabber_area_highlight") : "grabber_area");
            Ref<Texture> grabber = get_theme_icon(editable ? (highlighted ? StringName("grabber_highlight") : "grabber") : "grabber_disabled");
            Ref<Texture> tick = get_theme_icon("tick");
            double ratio = Math::is_nan(get_as_ratio()) ? 0 : get_as_ratio();

            if (orientation == VERTICAL) {

                int widget_width = style->get_minimum_size().width + style->get_center_size().width;
                float areasize = size.height - grabber->get_size().height;
                style->draw(ci, Rect2i(Point2i(size.width / 2 - widget_width / 2, 0), Size2i(widget_width, size.height)));

                grabber_area->draw(ci, Rect2i(Point2i((size.width - widget_width) / 2,
                                                      size.height - areasize * ratio - grabber->get_size().height / 2),
                                               Size2i(widget_width, areasize * ratio + grabber->get_size().height / 2)));

                if (ticks > 1) {
                    int grabber_offset = (grabber->get_size().height / 2 - tick->get_height() / 2);
                    for (int i = 0; i < ticks; i++) {
                        if (!ticks_on_borders && (i == 0 || i + 1 == ticks)) continue;
                        int ofs = (i * areasize / (ticks - 1)) + grabber_offset;
                        tick->draw(ci, Point2i((size.width - widget_width) / 2, ofs));
                    }
                }
                grabber->draw(ci, Point2i(size.width / 2 - grabber->get_size().width / 2, size.height - ratio * areasize - grabber->get_size().height));
            } else {

                int widget_height = style->get_minimum_size().height + style->get_center_size().height;
                float areasize = size.width - grabber->get_size().width;

                style->draw(ci, Rect2i(Point2i(0, (size.height - widget_height) / 2), Size2i(size.width, widget_height)));
                grabber_area->draw(ci, Rect2i(Point2i(0, (size.height - widget_height) / 2), Size2i(areasize * ratio + grabber->get_size().width / 2, widget_height)));

                if (ticks > 1) {
                    int grabber_offset = (grabber->get_size().width / 2 - tick->get_width() / 2);
                    for (int i = 0; i < ticks; i++) {
                        if ((!ticks_on_borders) && ((i == 0) || ((i + 1) == ticks))) continue;
                        int ofs = (i * areasize / (ticks - 1)) + grabber_offset;
                        tick->draw(ci, Point2i(ofs, (size.height - widget_height) / 2));
                    }
                }
                grabber->draw(ci, Point2i(ratio * areasize, size.height / 2 - grabber->get_size().height / 2));
            }

        } break;
    }
}

void Slider::set_custom_step(float p_custom_step) {

    custom_step = p_custom_step;
}

float Slider::get_custom_step() const {

    return custom_step;
}

void Slider::set_ticks(int p_count) {

    ticks = p_count;
    update();
}

int Slider::get_ticks() const {

    return ticks;
}

bool Slider::get_ticks_on_borders() const {
    return ticks_on_borders;
}

void Slider::set_ticks_on_borders(bool _tob) {
    ticks_on_borders = _tob;
    update();
}

void Slider::set_editable(bool p_editable) {

    editable = p_editable;
    update();
}

bool Slider::is_editable() const {

    return editable;
}

void Slider::set_scrollable(bool p_scrollable) {

    scrollable = p_scrollable;
}

bool Slider::is_scrollable() const {

    return scrollable;
}

void Slider::_bind_methods() {

    SE_BIND_METHOD(Slider,_gui_input);
    SE_BIND_METHOD(Slider,set_ticks);
    SE_BIND_METHOD(Slider,get_ticks);

    SE_BIND_METHOD(Slider,get_ticks_on_borders);
    SE_BIND_METHOD(Slider,set_ticks_on_borders);

    SE_BIND_METHOD(Slider,set_editable);
    SE_BIND_METHOD(Slider,is_editable);
    SE_BIND_METHOD(Slider,set_scrollable);
    SE_BIND_METHOD(Slider,is_scrollable);
    ADD_SIGNAL(MethodInfo("drag_started"));
    ADD_SIGNAL(MethodInfo("drag_ended", PropertyInfo(VariantType::BOOL, "value_changed")));

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "editable"), "set_editable", "is_editable");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "scrollable"), "set_scrollable", "is_scrollable");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "tick_count", PropertyHint::Range, "0,4096,1"), "set_ticks", "get_ticks");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "ticks_on_borders"), "set_ticks_on_borders", "get_ticks_on_borders");
}

Slider::Slider(Orientation p_orientation) {
    orientation = p_orientation;
    mouse_inside = false;
    grab.active = false;
    ticks = 0;
    ticks_on_borders = false;
    custom_step = -1;
    editable = true;
    scrollable = true;
    set_focus_mode(FOCUS_ALL);
}
