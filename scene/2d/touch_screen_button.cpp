/*************************************************************************/
/*  touch_screen_button.cpp                                              */
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

#include "touch_screen_button.h"

#include "core/callable_method_pointer.h"
#include "scene/main/scene_tree.h"
#include "core/method_bind.h"
#include "core/input/input_map.h"
#include "core/os/input.h"
#include "core/os/os.h"

IMPL_GDCLASS(TouchScreenButton)
VARIANT_ENUM_CAST(TouchScreenButton::VisibilityMode);

#ifdef TOOLS_ENABLED
Rect2 TouchScreenButton::_edit_get_rect() const {
    if (not texture) {
        return CanvasItem::_edit_get_rect();
    }

    return Rect2(Size2(), texture->get_size());
}

bool TouchScreenButton::_edit_use_rect() const {
    return texture;
}
#endif

void TouchScreenButton::set_texture(const Ref<Texture> &p_texture) {

    texture = p_texture;
    update();
}

Ref<Texture> TouchScreenButton::get_texture() const {

    return texture;
}

void TouchScreenButton::set_texture_pressed(const Ref<Texture> &p_texture_pressed) {

    texture_pressed = p_texture_pressed;
    update();
}

Ref<Texture> TouchScreenButton::get_texture_pressed() const {

    return texture_pressed;
}

void TouchScreenButton::set_bitmask(const Ref<BitMap> &p_bitmask) {

    bitmask = p_bitmask;
}

Ref<BitMap> TouchScreenButton::get_bitmask() const {

    return bitmask;
}

void TouchScreenButton::set_shape(const Ref<Shape2D> &p_shape) {

    if (shape)
        shape->disconnect("changed",callable_mp((CanvasItem *)this, &CanvasItem::update));

    shape = p_shape;

    if (shape)
        shape->connect("changed",callable_mp((CanvasItem *)this, &CanvasItem::update));

    update();
}

Ref<Shape2D> TouchScreenButton::get_shape() const {

    return shape;
}

void TouchScreenButton::set_shape_centered(bool p_shape_centered) {

    shape_centered = p_shape_centered;
    update();
}

bool TouchScreenButton::is_shape_visible() const {

    return shape_visible;
}

void TouchScreenButton::set_shape_visible(bool p_shape_visible) {

    shape_visible = p_shape_visible;
    update();
}

bool TouchScreenButton::is_shape_centered() const {

    return shape_centered;
}

void TouchScreenButton::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_DRAW: {

            if (!is_inside_tree())
                return;
            if (!Engine::get_singleton()->is_editor_hint() && !OS::get_singleton()->has_touchscreen_ui_hint() && visibility == VISIBILITY_TOUCHSCREEN_ONLY)
                return;

            if (finger_pressed != -1) {

                if (texture_pressed)
                    draw_texture(texture_pressed, Point2());
                else if (texture)
                    draw_texture(texture, Point2());

            } else {
                if (texture)
                    draw_texture(texture, Point2());
            }

            if (!shape_visible)
                return;
            if (!Engine::get_singleton()->is_editor_hint() && !get_tree()->is_debugging_collisions_hint())
                return;
            if (shape) {
                Color draw_col = get_tree()->get_debug_collisions_color();
                Vector2 size = not texture ? shape->get_rect().size : texture->get_size();
                Vector2 pos = shape_centered ? size * 0.5f : Vector2();
                draw_set_transform_matrix(get_canvas_transform().translated(pos));
                shape->draw(get_canvas_item(), draw_col);
            }

        } break;
        case NOTIFICATION_ENTER_TREE: {

            if (!Engine::get_singleton()->is_editor_hint() && !OS::get_singleton()->has_touchscreen_ui_hint() && visibility == VISIBILITY_TOUCHSCREEN_ONLY)
                return;
            update();

            if (!Engine::get_singleton()->is_editor_hint())
                set_process_input(is_visible_in_tree());

        } break;
        case NOTIFICATION_EXIT_TREE: {
            if (is_pressed())
                _release(true);
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {
            if (Engine::get_singleton()->is_editor_hint())
                break;
            if (is_visible_in_tree()) {
                set_process_input(true);
            } else {
                set_process_input(false);
                if (is_pressed())
                    _release();
            }
        } break;
        case NOTIFICATION_PAUSED: {
            if (is_pressed())
                _release();
        } break;
    }
}

bool TouchScreenButton::is_pressed() const {

    return finger_pressed != -1;
}

void TouchScreenButton::set_action(const StringName &p_action) {

    action = p_action;
}

StringName TouchScreenButton::get_action() const {

    return action;
}

void TouchScreenButton::_input(const Ref<InputEvent> &p_event) {
    ERR_FAIL_COND(!p_event);

    if (!is_visible_in_tree())
        return;
    ERR_FAIL_COND(nullptr==p_event);

    if (p_event->get_device() != 0)
        return;

    const InputEventScreenTouch *st = object_cast<InputEventScreenTouch>(p_event.get());

    if (passby_press) {

        const InputEventScreenDrag *sd = object_cast<InputEventScreenDrag>(p_event.get());

        if (st && !st->is_pressed() && finger_pressed == st->get_index()) {

            _release();
        }

        if ((st && st->is_pressed()) || sd) {

            int index = st ? st->get_index() : sd->get_index();
            Point2 coord = st ? st->get_position() : sd->get_position();

            if (finger_pressed == -1 || index == finger_pressed) {

                if (_is_point_inside(coord)) {
                    if (finger_pressed == -1) {
                        _press(index);
                    }
                } else {
                    if (finger_pressed != -1) {
                        _release();
                    }
                }
            }
        }

    } else {

        if (st) {

            if (st->is_pressed()) {

                const bool can_press = finger_pressed == -1;
                if (!can_press)
                    return; //already fingering

                if (_is_point_inside(st->get_position())) {
                    _press(st->get_index());
                }
            } else {
                if (st->get_index() == finger_pressed) {
                    _release();
                }
            }
        }
    }
}

bool TouchScreenButton::_is_point_inside(const Point2 &p_point) {

    Point2 coord = (get_global_transform_with_canvas()).affine_inverse().xform(p_point);

    bool touched = false;
    bool check_rect = true;

    if (shape) {

        check_rect = false;

        Vector2 size = !texture ? shape->get_rect().size : texture->get_size();
        Transform2D xform = shape_centered ? Transform2D().translated(size * 0.5f) : Transform2D();
        touched = shape->collide(xform, unit_rect, Transform2D(0, coord + Vector2(0.5, 0.5)));
    }

    if (bitmask) {

        check_rect = false;
        if (!touched && Rect2(Point2(), bitmask->get_size()).has_point(coord)) {

            if (bitmask->get_bit(coord))
                touched = true;
        }
    }

    if (!touched && check_rect) {
        if (texture)
            touched = Rect2(Size2(), texture->get_size()).has_point(coord);
    }

    return touched;
}

void TouchScreenButton::_press(int p_finger_pressed) {

    finger_pressed = p_finger_pressed;

    if (action != StringName()) {

        Input::get_singleton()->action_press(action);
        Ref<InputEventAction> iea(make_ref_counted<InputEventAction>());
        iea->set_action(action);
        iea->set_pressed(true);
        get_tree()->input_event(iea);
    }

    emit_signal("pressed");
    update();
}

void TouchScreenButton::_release(bool p_exiting_tree) {

    finger_pressed = -1;

    if (action != StringName()) {

        Input::get_singleton()->action_release(action);
        if (!p_exiting_tree) {

            Ref<InputEventAction> iea(make_ref_counted<InputEventAction>());
            iea->set_action(action);
            iea->set_pressed(false);
            get_tree()->input_event(iea);
        }
    }

    if (!p_exiting_tree) {
        emit_signal("released");
        update();
    }
}

Rect2 TouchScreenButton::get_anchorable_rect() const {
    if (not texture)
        return CanvasItem::get_anchorable_rect();

    return Rect2(Size2(), texture->get_size());
}

void TouchScreenButton::set_visibility_mode(VisibilityMode p_mode) {
    visibility = p_mode;
    update();
}

TouchScreenButton::VisibilityMode TouchScreenButton::get_visibility_mode() const {

    return visibility;
}

void TouchScreenButton::set_passby_press(bool p_enable) {

    passby_press = p_enable;
}

bool TouchScreenButton::is_passby_press_enabled() const {

    return passby_press;
}

void TouchScreenButton::_bind_methods() {

    SE_BIND_METHOD(TouchScreenButton,set_texture);
    SE_BIND_METHOD(TouchScreenButton,get_texture);

    SE_BIND_METHOD(TouchScreenButton,set_texture_pressed);
    SE_BIND_METHOD(TouchScreenButton,get_texture_pressed);

    SE_BIND_METHOD(TouchScreenButton,set_bitmask);
    SE_BIND_METHOD(TouchScreenButton,get_bitmask);

    SE_BIND_METHOD(TouchScreenButton,set_shape);
    SE_BIND_METHOD(TouchScreenButton,get_shape);

    SE_BIND_METHOD(TouchScreenButton,set_shape_centered);
    SE_BIND_METHOD(TouchScreenButton,is_shape_centered);

    SE_BIND_METHOD(TouchScreenButton,set_shape_visible);
    SE_BIND_METHOD(TouchScreenButton,is_shape_visible);

    SE_BIND_METHOD(TouchScreenButton,set_action);
    SE_BIND_METHOD(TouchScreenButton,get_action);

    SE_BIND_METHOD(TouchScreenButton,set_visibility_mode);
    SE_BIND_METHOD(TouchScreenButton,get_visibility_mode);

    SE_BIND_METHOD(TouchScreenButton,set_passby_press);
    SE_BIND_METHOD(TouchScreenButton,is_passby_press_enabled);

    SE_BIND_METHOD(TouchScreenButton,is_pressed);

    SE_BIND_METHOD(TouchScreenButton,_input);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "normal", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "pressed", PropertyHint::ResourceType, "Texture"), "set_texture_pressed", "get_texture_pressed");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "bitmask", PropertyHint::ResourceType, "BitMap"), "set_bitmask", "get_bitmask");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "shape", PropertyHint::ResourceType, "Shape2D"), "set_shape", "get_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "shape_centered"), "set_shape_centered", "is_shape_centered");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "shape_visible"), "set_shape_visible", "is_shape_visible");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "passby_press"), "set_passby_press", "is_passby_press_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "action"), "set_action", "get_action");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "visibility_mode", PropertyHint::Enum, "Always,TouchScreen Only"), "set_visibility_mode", "get_visibility_mode");

    ADD_SIGNAL(MethodInfo("pressed"));
    ADD_SIGNAL(MethodInfo("released"));

    BIND_ENUM_CONSTANT(VISIBILITY_ALWAYS);
    BIND_ENUM_CONSTANT(VISIBILITY_TOUCHSCREEN_ONLY);
}

TouchScreenButton::TouchScreenButton() {

    finger_pressed = -1;
    passby_press = false;
    visibility = VISIBILITY_ALWAYS;
    shape_centered = true;
    shape_visible = true;
    unit_rect = make_ref_counted<RectangleShape2D>();
    unit_rect->set_extents(Vector2(0.5, 0.5));
}
