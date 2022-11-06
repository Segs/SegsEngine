/*************************************************************************/
/*  input_event.cpp                                                      */
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

#include "input_event.h"

#include "core/input/input_map.h"
#include "core/method_bind.h"
#include "core/os/keyboard.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"

IMPL_GDCLASS(InputEvent)
IMPL_GDCLASS(InputEventWithModifiers)
IMPL_GDCLASS(InputEventKey)
IMPL_GDCLASS(InputEventMouse)
IMPL_GDCLASS(InputEventMouseButton)
IMPL_GDCLASS(InputEventMouseMotion)
IMPL_GDCLASS(InputEventJoypadMotion)
IMPL_GDCLASS(InputEventJoypadButton)
IMPL_GDCLASS(InputEventScreenTouch)
IMPL_GDCLASS(InputEventScreenDrag)
IMPL_GDCLASS(InputEventAction)
IMPL_GDCLASS(InputEventGesture)
IMPL_GDCLASS(InputEventMagnifyGesture)
IMPL_GDCLASS(InputEventPanGesture)
IMPL_GDCLASS(InputEventMIDI)

const int InputEvent::DEVICE_ID_TOUCH_MOUSE = -1;
const int InputEvent::DEVICE_ID_INTERNAL = -2;

void InputEvent::set_device(int p_device) {
    device = p_device;
}

int InputEvent::get_device() const {
    return device;
}

bool InputEvent::is_action(const StringName &p_action, bool p_exact_match) const {
    return InputMap::get_singleton()->event_is_action(
            Ref<InputEvent>(const_cast<InputEvent *>(this)), p_action, p_exact_match);
}

bool InputEvent::is_action_pressed(const StringName &p_action, bool p_allow_echo, bool p_exact_match) const {
    bool pressed = false;
    bool valid = InputMap::get_singleton()->event_get_action_status(
            Ref<InputEvent>(const_cast<InputEvent *>(this)), p_action, p_exact_match, &pressed);
    return valid && pressed && (p_allow_echo || !is_echo());
}

bool InputEvent::is_action_released(const StringName &p_action, bool p_exact_match) const {
    bool pressed = false;
    bool valid =
            InputMap::get_singleton()->event_get_action_status(
            Ref<InputEvent>(const_cast<InputEvent *>(this)), p_action, p_exact_match, &pressed);
    return valid && !pressed;
}

float InputEvent::get_action_strength(const StringName &p_action, bool p_exact_match) const {
    bool pressed = false;
    float strength = 0.0f;
    bool valid = InputMap::get_singleton()->event_get_action_status(
            Ref<InputEvent>(const_cast<InputEvent *>(this)), p_action, p_exact_match, &pressed, &strength);
    return valid ? strength : 0.0f;
}

float InputEvent::get_action_raw_strength(const StringName &p_action, bool p_exact_match) const {
    float raw_strength=0.0f;
    bool valid = InputMap::get_singleton()->event_get_action_status(
            Ref<InputEvent>((InputEvent *)this), p_action, p_exact_match, nullptr, nullptr, &raw_strength);
    return valid ? raw_strength : 0.0f;
}

bool InputEvent::is_pressed() const {
    return false;
}

bool InputEvent::is_echo() const {
    return false;
}

Ref<InputEvent> InputEvent::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
    return Ref<InputEvent>((InputEvent *)this);
}

String InputEvent::as_text() const {
    return String();
}

bool InputEvent::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, bool *p_pressed, float *p_strength,
        float *p_raw_strength,
        float p_deadzone) const {
    return false;
}

bool InputEvent::shortcut_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
    return false;
}

bool InputEvent::is_action_type() const {
    return false;
}

void InputEvent::_bind_methods() {
    SE_BIND_METHOD(InputEvent, set_device);
    SE_BIND_METHOD(InputEvent, get_device);

    SE_BIND_METHOD(InputEvent, is_action);
    SE_BIND_METHOD_WITH_DEFAULTS(InputEvent, is_action_pressed, DEFVAL(false) );
    SE_BIND_METHOD(InputEvent, is_action_released);
    SE_BIND_METHOD(InputEvent, get_action_strength);

    SE_BIND_METHOD(InputEvent, is_pressed);
    SE_BIND_METHOD(InputEvent, is_echo);

    SE_BIND_METHOD(InputEvent, as_text);

    SE_BIND_METHOD(InputEvent, shortcut_match);

    SE_BIND_METHOD(InputEvent, is_action_type);

    SE_BIND_METHOD(InputEvent, accumulate);

    SE_BIND_METHOD_WITH_DEFAULTS(InputEvent, xformed_by, DEFVAL(Vector2()) );

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "device"), "set_device", "get_device");
}

InputEvent::InputEvent() {
    device = 0;
}

//////////////////

void InputEventWithModifiers::set_shift(bool p_enabled) {
    shift = p_enabled;
}

bool InputEventWithModifiers::get_shift() const {
    return shift;
}

void InputEventWithModifiers::set_alt(bool p_enabled) {
    alt = p_enabled;
}
bool InputEventWithModifiers::get_alt() const {
    return alt;
}

void InputEventWithModifiers::set_control(bool p_enabled) {
    control = p_enabled;
}
bool InputEventWithModifiers::get_control() const {
    return control;
}

void InputEventWithModifiers::set_metakey(bool p_enabled) {
    meta = p_enabled;
}
bool InputEventWithModifiers::get_metakey() const {
    return meta;
}

void InputEventWithModifiers::set_command(bool p_enabled) {
    command = p_enabled;
}
bool InputEventWithModifiers::get_command() const {
    return command;
}

void InputEventWithModifiers::set_modifiers_from_event(const InputEventWithModifiers *event) {
    set_alt(event->get_alt());
    set_shift(event->get_shift());
    set_control(event->get_control());
    set_metakey(event->get_metakey());
}

uint32_t InputEventWithModifiers::get_modifiers_mask() const {
    uint32_t mask = 0;
    if (get_control()) {
        mask |= KEY_MASK_CTRL;
    }
    if (get_shift()) {
        mask |= KEY_MASK_SHIFT;
    }
    if (get_alt()) {
        mask |= KEY_MASK_ALT;
    }
    if (get_metakey()) {
        mask |= KEY_MASK_META;
    }
    return mask;
}

void InputEventWithModifiers::_bind_methods() {
    SE_BIND_METHOD(InputEventWithModifiers, set_alt);
    SE_BIND_METHOD(InputEventWithModifiers, get_alt);

    SE_BIND_METHOD(InputEventWithModifiers, set_shift);
    SE_BIND_METHOD(InputEventWithModifiers, get_shift);

    SE_BIND_METHOD(InputEventWithModifiers, set_control);
    SE_BIND_METHOD(InputEventWithModifiers, get_control);

    SE_BIND_METHOD(InputEventWithModifiers, set_metakey);
    SE_BIND_METHOD(InputEventWithModifiers, get_metakey);

    SE_BIND_METHOD(InputEventWithModifiers, set_command);
    SE_BIND_METHOD(InputEventWithModifiers, get_command);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "alt"), "set_alt", "get_alt");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "shift"), "set_shift", "get_shift");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "control"), "set_control", "get_control");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "meta"), "set_metakey", "get_metakey");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "command"), "set_command", "get_command");
}

InputEventWithModifiers::InputEventWithModifiers() {
    alt = false;
    shift = false;
    control = false;
    meta = false;
}

//////////////////////////////////

void InputEventKey::set_pressed(bool p_pressed) {
    pressed = p_pressed;
}

bool InputEventKey::is_pressed() const {
    return pressed;
}

void InputEventKey::set_keycode(uint32_t p_scancode) {
    keycode = p_scancode;
}
uint32_t InputEventKey::get_keycode() const {
    return keycode;
}

void InputEventKey::set_physical_scancode(uint32_t p_scancode) {
    physical_scancode = p_scancode;
}
uint32_t InputEventKey::get_physical_scancode() const {
    return physical_scancode;
}

void InputEventKey::set_unicode(uint32_t p_unicode) {
    unicode = p_unicode;
}
uint32_t InputEventKey::get_unicode() const {
    return unicode;
}

void InputEventKey::set_echo(bool p_enable) {
    echo = p_enable;
}
bool InputEventKey::is_echo() const {
    return echo;
}

uint32_t InputEventKey::get_keycode_with_modifiers() const {
    return keycode | get_modifiers_mask();
}

uint32_t InputEventKey::get_physical_scancode_with_modifiers() const {
    return physical_scancode | get_modifiers_mask();
}

String InputEventKey::as_text() const {
    String kc;

    if (keycode == 0) {
        kc = keycode_get_string(physical_scancode) + " (" + RTR("Physical") + ")";
    } else {
        kc = keycode_get_string(keycode);
    }
    if (kc.empty()) {
        return kc;
    }

    if (get_metakey()) {
        kc = find_keycode_name(KEY_META) + ("+" + kc);
    }
    if (get_alt()) {
        kc = find_keycode_name(KEY_ALT) + ("+" + kc);
    }
    if (get_shift()) {
        kc = find_keycode_name(KEY_SHIFT) + ("+" + kc);
    }
    if (get_control()) {
        kc = find_keycode_name(KEY_CONTROL) + ("+" + kc);
    }
    return kc;
}

bool InputEventKey::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, bool *p_pressed, float *p_strength,
        float *p_raw_strength, float p_deadzone) const {
    Ref<InputEventKey> key = dynamic_ref_cast<InputEventKey>(p_event);
    if (not key) {
        return false;
    }

	bool match;
    if (keycode != 0) {
        match = keycode == key->keycode;
    } else {
        match = physical_scancode == key->physical_scancode;
    }
    uint32_t action_mask = get_modifiers_mask();
    uint32_t key_mask = key->get_modifiers_mask();
    if (key->is_pressed()) {
        match &= (action_mask & key_mask) == action_mask;
    }
    if (p_exact_match) {
        match &= action_mask == key_mask;
    }

    if (match) {
        bool pressed = key->is_pressed();
        if (p_pressed != nullptr) {
            *p_pressed = pressed;
        }
        float strength = pressed ? 1.0f : 0.0f;
        if (p_strength != nullptr) {
            *p_strength = strength;
        }
        if (p_raw_strength != nullptr) {
            *p_raw_strength = strength;
        }
    }
    return match;
}

bool InputEventKey::shortcut_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
    Ref<InputEventKey> key = dynamic_ref_cast<InputEventKey>(p_event);
    if (not key) {
        return false;
    }

    if (keycode == 0) {
        return physical_scancode == key->physical_scancode &&
               (!p_exact_match || get_modifiers_mask() == key->get_modifiers_mask());
    } else {
        return keycode == key->keycode && (!p_exact_match || get_modifiers_mask() == key->get_modifiers_mask());
    }
}

void InputEventKey::_bind_methods() {
    SE_BIND_METHOD(InputEventKey, set_pressed);

    SE_BIND_METHOD(InputEventKey, set_keycode);
    SE_BIND_METHOD(InputEventKey, get_keycode);

    SE_BIND_METHOD(InputEventKey, set_physical_scancode);
    SE_BIND_METHOD(InputEventKey, get_physical_scancode);

    SE_BIND_METHOD(InputEventKey, set_unicode);
    SE_BIND_METHOD(InputEventKey, get_unicode);

    SE_BIND_METHOD(InputEventKey, set_echo);

    SE_BIND_METHOD(InputEventKey, get_keycode_with_modifiers);
    MethodBinder::bind_method(
            D_METHOD("get_physical_scancode_with_modifiers"), &InputEventKey::get_physical_scancode_with_modifiers);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "pressed"), "set_pressed", "is_pressed");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "keycode"), "set_keycode", "get_keycode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "physical_scancode"), "set_physical_scancode", "get_physical_scancode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "unicode"), "set_unicode", "get_unicode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "echo"), "set_echo", "is_echo");
}

InputEventKey::InputEventKey() {
    pressed = false;
    keycode = 0;
    physical_scancode = 0;
    unicode = 0; // unicode
    echo = false;
}

////////////////////////////////////////

void InputEventMouse::set_button_mask(int p_mask) {
    button_mask = p_mask;
}
int InputEventMouse::get_button_mask() const {
    return button_mask;
}

void InputEventMouse::set_position(const Vector2 &p_pos) {
    pos = p_pos;
}
Vector2 InputEventMouse::get_position() const {
    return pos;
}

void InputEventMouse::set_global_position(const Vector2 &p_global_pos) {
    global_pos = p_global_pos;
}
Vector2 InputEventMouse::get_global_position() const {
    return global_pos;
}

void InputEventMouse::_bind_methods() {
    SE_BIND_METHOD(InputEventMouse, set_button_mask);
    SE_BIND_METHOD(InputEventMouse, get_button_mask);

    SE_BIND_METHOD(InputEventMouse, set_position);
    SE_BIND_METHOD(InputEventMouse, get_position);

    MethodBinder::bind_method(
            D_METHOD("set_global_position", { "global_position" }), &InputEventMouse::set_global_position);
    SE_BIND_METHOD(InputEventMouse, get_global_position);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "button_mask"), "set_button_mask", "get_button_mask");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "position"), "set_position", "get_position");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "global_position"), "set_global_position", "get_global_position");
}

InputEventMouse::InputEventMouse() {
    button_mask = 0;
}

///////////////////////////////////////

void InputEventMouseButton::set_factor(float p_factor) {
    factor = p_factor;
}

float InputEventMouseButton::get_factor() const {
    return factor;
}

void InputEventMouseButton::set_button_index(int p_index) {
    button_index = p_index;
}
int InputEventMouseButton::get_button_index() const {
    return button_index;
}

void InputEventMouseButton::set_pressed(bool p_pressed) {
    pressed = p_pressed;
}
bool InputEventMouseButton::is_pressed() const {
    return pressed;
}

void InputEventMouseButton::set_doubleclick(bool p_doubleclick) {
    doubleclick = p_doubleclick;
}
bool InputEventMouseButton::is_doubleclick() const {
    return doubleclick;
}

Ref<InputEvent> InputEventMouseButton::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
    Vector2 g = get_global_position();
    Vector2 l = p_xform.xform(get_position() + p_local_ofs);

    Ref<InputEventMouseButton> mb(make_ref_counted<InputEventMouseButton>());

    mb->set_device(get_device());

    mb->set_modifiers_from_event(this);

    mb->set_position(l);
    mb->set_global_position(g);

    mb->set_button_mask(get_button_mask());
    mb->set_pressed(pressed);
    mb->set_doubleclick(doubleclick);
    mb->set_factor(factor);
    mb->set_button_index(button_index);

    return mb;
}

bool InputEventMouseButton::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, bool *p_pressed,
        float *p_strength,
        float *p_raw_strength, float /*p_deadzone*/) const {
    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);
    if (not mb) {
        return false;
    }

    bool match = mb->button_index == button_index;
    uint32_t action_mask = get_modifiers_mask();
    uint32_t button_mask = mb->get_modifiers_mask();
    if (mb->is_pressed()) {
        match &= (action_mask & button_mask) == action_mask;
    }
    if (p_exact_match) {
        match &= action_mask == button_mask;
    }
    if (match) {
        bool pressed = mb->is_pressed();
        if (p_pressed != nullptr) {
            *p_pressed = pressed;
        }
        float strength = pressed ? 1.0f : 0.0f;
        if (p_strength != nullptr) {
            *p_strength = strength;
        }
        if (p_raw_strength != nullptr) {
            *p_raw_strength = strength;
        }
    }

    return match;
}

bool InputEventMouseButton::shortcut_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);
    if (not mb) {
        return false;
    }

    return button_index == mb->button_index &&
            (!p_exact_match || get_modifiers_mask() == mb->get_modifiers_mask());
}

String InputEventMouseButton::as_text() const {
    String button_index_string;
    switch (get_button_index()) {
        case BUTTON_LEFT:
            button_index_string = "BUTTON_LEFT";
            break;
        case BUTTON_RIGHT:
            button_index_string = "BUTTON_RIGHT";
            break;
        case BUTTON_MIDDLE:
            button_index_string = "BUTTON_MIDDLE";
            break;
        case BUTTON_WHEEL_UP:
            button_index_string = "BUTTON_WHEEL_UP";
            break;
        case BUTTON_WHEEL_DOWN:
            button_index_string = "BUTTON_WHEEL_DOWN";
            break;
        case BUTTON_WHEEL_LEFT:
            button_index_string = "BUTTON_WHEEL_LEFT";
            break;
        case BUTTON_WHEEL_RIGHT:
            button_index_string = "BUTTON_WHEEL_RIGHT";
            break;
        case BUTTON_XBUTTON1:
            button_index_string = "BUTTON_XBUTTON1";
            break;
        case BUTTON_XBUTTON2:
            button_index_string = "BUTTON_XBUTTON2";
            break;
        default:
            button_index_string = ::to_string(get_button_index());
            break;
    }
    return "InputEventMouseButton : button_index=" + button_index_string + ", pressed=" + (pressed ? "true" : "false") +
           ", position=(" + String(get_position()) + "), button_mask=" + ::to_string(get_button_mask()) +
           ", doubleclick=" + (doubleclick ? "true" : "false");
}

void InputEventMouseButton::_bind_methods() {
    SE_BIND_METHOD(InputEventMouseButton, set_factor);
    SE_BIND_METHOD(InputEventMouseButton, get_factor);

    MethodBinder::bind_method(
            D_METHOD("set_button_index", { "button_index" }), &InputEventMouseButton::set_button_index);
    SE_BIND_METHOD(InputEventMouseButton, get_button_index);

    SE_BIND_METHOD(InputEventMouseButton, set_pressed);
    //    BIND_METHOD(InputEventMouseButton, is_pressed);

    SE_BIND_METHOD(InputEventMouseButton, set_doubleclick);
    SE_BIND_METHOD(InputEventMouseButton, is_doubleclick);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "factor"), "set_factor", "get_factor");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "button_index"), "set_button_index", "get_button_index");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "pressed"), "set_pressed", "is_pressed");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "doubleclick"), "set_doubleclick", "is_doubleclick");
}

InputEventMouseButton::InputEventMouseButton() {
    factor = 1;
    button_index = 0;
    pressed = false;
    doubleclick = false;
}

////////////////////////////////////////////

void InputEventMouseMotion::set_tilt(const Vector2 &p_tilt) {
    tilt = p_tilt;
}

Vector2 InputEventMouseMotion::get_tilt() const {
    return tilt;
}

void InputEventMouseMotion::set_pressure(float p_pressure) {
    pressure = p_pressure;
}

void InputEventMouseMotion::set_relative(const Vector2 &p_relative) {
    relative = p_relative;
}
Vector2 InputEventMouseMotion::get_relative() const {
    return relative;
}

void InputEventMouseMotion::set_speed(const Vector2 &p_speed) {
    speed = p_speed;
}
Vector2 InputEventMouseMotion::get_speed() const {
    return speed;
}

Ref<InputEvent> InputEventMouseMotion::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
    Vector2 g = get_global_position();
    Vector2 l = p_xform.xform(get_position() + p_local_ofs);
    Vector2 r = p_xform.basis_xform(get_relative());
    Vector2 s = p_xform.basis_xform(get_speed());

    Ref<InputEventMouseMotion> mm(make_ref_counted<InputEventMouseMotion>());

    mm->set_device(get_device());

    mm->set_modifiers_from_event(this);

    mm->set_position(l);
    mm->set_pressure(get_pressure());
    mm->set_tilt(get_tilt());
    mm->set_global_position(g);

    mm->set_button_mask(get_button_mask());
    mm->set_relative(r);
    mm->set_speed(s);

    return mm;
}

String InputEventMouseMotion::as_text() const {
    String button_mask_string;
    switch (get_button_mask()) {
        case BUTTON_MASK_LEFT:
            button_mask_string = "BUTTON_MASK_LEFT";
            break;
        case BUTTON_MASK_MIDDLE:
            button_mask_string = "BUTTON_MASK_MIDDLE";
            break;
        case BUTTON_MASK_RIGHT:
            button_mask_string = "BUTTON_MASK_RIGHT";
            break;
        case BUTTON_MASK_XBUTTON1:
            button_mask_string = "BUTTON_MASK_XBUTTON1";
            break;
        case BUTTON_MASK_XBUTTON2:
            button_mask_string = "BUTTON_MASK_XBUTTON2";
            break;
        default:
            button_mask_string = ::to_string(get_button_mask());
            break;
    }
    return "InputEventMouseMotion : button_mask=" + button_mask_string + ", position=(" + String(get_position()) +
           "), relative=(" + String(get_relative()) + "), speed=(" + String(get_speed()) + "), pressure=(" + rtos(get_pressure()) + "), tilt=(" + String(get_tilt()) +
           ")";
}

bool InputEventMouseMotion::accumulate(const Ref<InputEvent> &p_event) {
    Ref<InputEventMouseMotion> motion = dynamic_ref_cast<InputEventMouseMotion>(p_event);
    if (not motion) {
        return false;
    }

    if (is_pressed() != motion->is_pressed()) {
        return false;
    }

    if (get_button_mask() != motion->get_button_mask()) {
        return false;
    }

    if (get_shift() != motion->get_shift()) {
        return false;
    }

    if (get_control() != motion->get_control()) {
        return false;
    }

    if (get_alt() != motion->get_alt()) {
        return false;
    }

    if (get_metakey() != motion->get_metakey()) {
        return false;
    }

    set_position(motion->get_position());
    set_global_position(motion->get_global_position());
    set_speed(motion->get_speed());
    relative += motion->get_relative();

    return true;
}

void InputEventMouseMotion::_bind_methods() {
    SE_BIND_METHOD(InputEventMouseMotion, set_tilt);
    SE_BIND_METHOD(InputEventMouseMotion, get_tilt);

    SE_BIND_METHOD(InputEventMouseMotion, set_pressure);
    SE_BIND_METHOD(InputEventMouseMotion, get_pressure);

    SE_BIND_METHOD(InputEventMouseMotion, set_relative);
    SE_BIND_METHOD(InputEventMouseMotion, get_relative);

    SE_BIND_METHOD(InputEventMouseMotion, set_speed);
    SE_BIND_METHOD(InputEventMouseMotion, get_speed);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "tilt"), "set_tilt", "get_tilt");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "pressure"), "set_pressure", "get_pressure");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "relative"), "set_relative", "get_relative");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "speed"), "set_speed", "get_speed");
}

////////////////////////////////////////

void InputEventJoypadMotion::set_axis(int p_axis) {
    axis = p_axis;
}

int InputEventJoypadMotion::get_axis() const {
    return axis;
}

void InputEventJoypadMotion::set_axis_value(float p_value) {
    axis_value = p_value;
}

float InputEventJoypadMotion::get_axis_value() const {
    return axis_value;
}

bool InputEventJoypadMotion::is_pressed() const {
    return Math::abs(axis_value) >= 0.5f;
}

bool InputEventJoypadMotion::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, bool *p_pressed,
        float *p_strength,
        float *p_raw_strength, float p_deadzone) const {
    Ref<InputEventJoypadMotion> jm = dynamic_ref_cast<InputEventJoypadMotion>(p_event);
    if (not jm) {
        return false;
    }

	// Matches even if not in the same direction, but returns a "not pressed" event.
    bool match = (axis == jm->axis);
    if (p_exact_match) {
        match &= (axis_value < 0) == (jm->axis_value < 0);
    }
    if (match) {
        float jm_abs_axis_value = Math::abs(jm->get_axis_value());
        bool same_direction = (((axis_value < 0) == (jm->axis_value < 0)) || jm->axis_value == 0.0f);
        bool pressed = same_direction ? jm_abs_axis_value >= p_deadzone : false;
        if (p_pressed != nullptr) {
            *p_pressed = pressed;
        }
        if (p_strength != nullptr) {
            if (pressed) {
                if (p_deadzone == 1.0f) {
                    *p_strength = 1.0f;
                } else {
                    *p_strength = CLAMP(Math::inverse_lerp(p_deadzone, 1.0f, jm_abs_axis_value), 0.0f, 1.0f);
                }
            } else {
                *p_strength = 0.0f;
            }
        }
        if (p_raw_strength != nullptr) {
            if (same_direction) { // NOT pressed, because we want to ignore the deadzone.
                *p_raw_strength = jm_abs_axis_value;
            } else {
                *p_raw_strength = 0.0f;
            }
        }
    }
    return match;
}

bool InputEventJoypadMotion::shortcut_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
    Ref<InputEventJoypadMotion> jm = dynamic_ref_cast<InputEventJoypadMotion>(p_event);
    if (not jm) {
        return false;
    }

    return axis == jm->axis &&
            (!p_exact_match || ((axis_value < 0) == (jm->axis_value < 0)));
}

String InputEventJoypadMotion::as_text() const {
    return "InputEventJoypadMotion : axis=" + ::to_string(axis) + ", axis_value=" + StringUtils::num(axis_value);
}

void InputEventJoypadMotion::_bind_methods() {
    SE_BIND_METHOD(InputEventJoypadMotion, set_axis);
    SE_BIND_METHOD(InputEventJoypadMotion, get_axis);

    SE_BIND_METHOD(InputEventJoypadMotion, set_axis_value);
    SE_BIND_METHOD(InputEventJoypadMotion, get_axis_value);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "axis"), "set_axis", "get_axis");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "axis_value"), "set_axis_value", "get_axis_value");
}

InputEventJoypadMotion::InputEventJoypadMotion() {
    axis = 0;
    axis_value = 0;
}
/////////////////////////////////

void InputEventJoypadButton::set_button_index(int p_index) {
    button_index = p_index;
}

int InputEventJoypadButton::get_button_index() const {
    return button_index;
}

void InputEventJoypadButton::set_pressed(bool p_pressed) {
    pressed = p_pressed;
}
bool InputEventJoypadButton::is_pressed() const {
    return pressed;
}

void InputEventJoypadButton::set_pressure(float p_pressure) {
    pressure = p_pressure;
}
float InputEventJoypadButton::get_pressure() const {
    return pressure;
}

bool InputEventJoypadButton::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, bool *p_pressed,
        float *p_strength,
        float *p_raw_strength, float p_deadzone) const {
    Ref<InputEventJoypadButton> jb = dynamic_ref_cast<InputEventJoypadButton>(p_event);
    if (not jb) {
        return false;
    }

    bool match = button_index == jb->button_index;
    if (match) {
        bool pressed = jb->is_pressed();
        if (p_pressed != nullptr) {
            *p_pressed = pressed;
        }
        float strength = pressed ? 1.0f : 0.0f;
        if (p_strength != nullptr) {
            *p_strength = strength;
        }
        if (p_raw_strength != nullptr) {
            *p_raw_strength = strength;
        }
    }

    return match;
}

bool InputEventJoypadButton::shortcut_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
    Ref<InputEventJoypadButton> button = dynamic_ref_cast<InputEventJoypadButton>(p_event);
    if (not button) {
        return false;
    }

    return button_index == button->button_index;
}

String InputEventJoypadButton::as_text() const {
    return FormatVE("InputEventJoypadButton : button_index=%d , pressed=%s, pressure=%f", button_index,
            pressed ? "true" : "false", pressure);
}

void InputEventJoypadButton::_bind_methods() {
    MethodBinder::bind_method(
            D_METHOD("set_button_index", { "button_index" }), &InputEventJoypadButton::set_button_index);
    SE_BIND_METHOD(InputEventJoypadButton, get_button_index);

    SE_BIND_METHOD(InputEventJoypadButton, set_pressure);
    SE_BIND_METHOD(InputEventJoypadButton, get_pressure);

    SE_BIND_METHOD(InputEventJoypadButton, set_pressed);
    //    BIND_METHOD(InputEventJoypadButton, is_pressed);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "button_index"), "set_button_index", "get_button_index");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "pressure"), "set_pressure", "get_pressure");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "pressed"), "set_pressed", "is_pressed");
}

InputEventJoypadButton::InputEventJoypadButton() {
    button_index = 0;
    pressure = 0;
    pressed = false;
}

//////////////////////////////////////////////

void InputEventScreenTouch::set_index(int p_index) {
    index = p_index;
}
int InputEventScreenTouch::get_index() const {
    return index;
}

void InputEventScreenTouch::set_position(const Vector2 &p_pos) {
    pos = p_pos;
}
Vector2 InputEventScreenTouch::get_position() const {
    return pos;
}

void InputEventScreenTouch::set_pressed(bool p_pressed) {
    pressed = p_pressed;
}
bool InputEventScreenTouch::is_pressed() const {
    return pressed;
}

Ref<InputEvent> InputEventScreenTouch::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
    Ref<InputEventScreenTouch> st(make_ref_counted<InputEventScreenTouch>());

    st->set_device(get_device());
    st->set_index(index);
    st->set_position(p_xform.xform(pos + p_local_ofs));
    st->set_pressed(pressed);

    return st;
}

String InputEventScreenTouch::as_text() const {
    return "InputEventScreenTouch : index=" + ::to_string(index) + ", pressed=" + (pressed ? "true" : "false") +
           ", position=(" + String(get_position()) + ")";
}

void InputEventScreenTouch::_bind_methods() {
    SE_BIND_METHOD(InputEventScreenTouch, set_index);
    SE_BIND_METHOD(InputEventScreenTouch, get_index);

    SE_BIND_METHOD(InputEventScreenTouch, set_position);
    SE_BIND_METHOD(InputEventScreenTouch, get_position);

    SE_BIND_METHOD(InputEventScreenTouch, set_pressed);
    // BIND_METHOD(InputEventScreenTouch, is_pressed);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "index"), "set_index", "get_index");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "position"), "set_position", "get_position");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "pressed"), "set_pressed", "is_pressed");
}

InputEventScreenTouch::InputEventScreenTouch() {
    index = 0;
    pressed = false;
}

/////////////////////////////

void InputEventScreenDrag::set_index(int p_index) {
    index = p_index;
}

int InputEventScreenDrag::get_index() const {
    return index;
}

void InputEventScreenDrag::set_position(const Vector2 &p_pos) {
    pos = p_pos;
}
Vector2 InputEventScreenDrag::get_position() const {
    return pos;
}

void InputEventScreenDrag::set_relative(const Vector2 &p_relative) {
    relative = p_relative;
}
Vector2 InputEventScreenDrag::get_relative() const {
    return relative;
}

void InputEventScreenDrag::set_speed(const Vector2 &p_speed) {
    speed = p_speed;
}
Vector2 InputEventScreenDrag::get_speed() const {
    return speed;
}

Ref<InputEvent> InputEventScreenDrag::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
    Ref<InputEventScreenDrag> sd(make_ref_counted<InputEventScreenDrag>());

    sd->set_device(get_device());

    sd->set_index(index);
    sd->set_position(p_xform.xform(pos + p_local_ofs));
    sd->set_relative(p_xform.basis_xform(relative));
    sd->set_speed(p_xform.basis_xform(speed));

    return sd;
}

String InputEventScreenDrag::as_text() const {
    return "InputEventScreenDrag : index=" + ::to_string(index) + ", position=(" + String(get_position()) +
           "), relative=(" + String(get_relative()) + "), speed=(" + String(get_speed()) + ")";
}

bool InputEventScreenDrag::accumulate(const Ref<InputEvent> &p_event) {
    Ref<InputEventScreenDrag> drag = dynamic_ref_cast<InputEventScreenDrag>(p_event);
    if (not drag)
        return false;

    if (get_index() != drag->get_index()) {
        return false;
    }

    set_position(drag->get_position());
    set_speed(drag->get_speed());
    relative += drag->get_relative();

    return true;
}

void InputEventScreenDrag::_bind_methods() {
    SE_BIND_METHOD(InputEventScreenDrag, set_index);
    SE_BIND_METHOD(InputEventScreenDrag, get_index);

    SE_BIND_METHOD(InputEventScreenDrag, set_position);
    SE_BIND_METHOD(InputEventScreenDrag, get_position);

    SE_BIND_METHOD(InputEventScreenDrag, set_relative);
    SE_BIND_METHOD(InputEventScreenDrag, get_relative);

    SE_BIND_METHOD(InputEventScreenDrag, set_speed);
    SE_BIND_METHOD(InputEventScreenDrag, get_speed);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "index"), "set_index", "get_index");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "position"), "set_position", "get_position");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "relative"), "set_relative", "get_relative");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "speed"), "set_speed", "get_speed");
}

InputEventScreenDrag::InputEventScreenDrag() {
    index = 0;
}
/////////////////////////////

void InputEventAction::set_action(const StringName &p_action) {
    action = p_action;
}
StringName InputEventAction::get_action() const {
    return action;
}

void InputEventAction::set_pressed(bool p_pressed) {
    pressed = p_pressed;
}
bool InputEventAction::is_pressed() const {
    return pressed;
}

void InputEventAction::set_strength(float p_strength) {
    strength = CLAMP(p_strength, 0.0f, 1.0f);
}

float InputEventAction::get_strength() const {
    return strength;
}

bool InputEventAction::shortcut_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
    if (not p_event) {
        return false;
    }

    return p_event->is_action(action);
}

bool InputEventAction::is_action(const StringName &p_action) const {
    return action == p_action;
}

bool InputEventAction::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, bool *p_pressed,
        float *p_strength,
        float *p_raw_strength, float p_deadzone) const {
    Ref<InputEventAction> act = dynamic_ref_cast<InputEventAction>(p_event);
    if (not act) {
        return false;
    }

    bool match = action == act->action;
    if (match) {
        bool pressed = act->pressed;
        if (p_pressed != nullptr) {
            *p_pressed = pressed;
        }
        float strength = pressed ? 1.0f : 0.0f;
        if (p_strength != nullptr) {
            *p_strength = strength;
        }
        if (p_raw_strength != nullptr) {
            *p_raw_strength = strength;
        }
    }
    return match;
}

String InputEventAction::as_text() const {
    return "InputEventAction : action=" + String(action) + ", pressed=(" + (pressed ? "true" : "false");
}

void InputEventAction::_bind_methods() {
    SE_BIND_METHOD(InputEventAction, set_action);
    SE_BIND_METHOD(InputEventAction, get_action);

    SE_BIND_METHOD(InputEventAction, set_pressed);
    // BIND_METHOD(InputEventAction, is_pressed);

    SE_BIND_METHOD(InputEventAction, set_strength);
    SE_BIND_METHOD(InputEventAction, get_strength);

    //    BIND_METHOD(InputEventAction, is_action);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "action"), "set_action", "get_action");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "pressed"), "set_pressed", "is_pressed");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "strength", PropertyHint::Range, "0,1,0.01"), "set_strength",
            "get_strength");
}

/////////////////////////////

void InputEventGesture::set_position(const Vector2 &p_pos) {
    pos = p_pos;
}

void InputEventGesture::_bind_methods() {
    SE_BIND_METHOD(InputEventGesture, set_position);
    SE_BIND_METHOD(InputEventGesture, get_position);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "position"), "set_position", "get_position");
}

Vector2 InputEventGesture::get_position() const {
    return pos;
}
/////////////////////////////

void InputEventMagnifyGesture::set_factor(real_t p_factor) {
    factor = p_factor;
}

real_t InputEventMagnifyGesture::get_factor() const {
    return factor;
}

Ref<InputEvent> InputEventMagnifyGesture::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
    Ref<InputEventMagnifyGesture> ev(make_ref_counted<InputEventMagnifyGesture>());

    ev->set_device(get_device());
    ev->set_modifiers_from_event(this);

    ev->set_position(p_xform.xform(get_position() + p_local_ofs));
    ev->set_factor(get_factor());

    return ev;
}

String InputEventMagnifyGesture::as_text() const {
    return "InputEventMagnifyGesture : factor=" + StringUtils::num(get_factor()) + ", position=(" +
           String(get_position()) + ")";
}

void InputEventMagnifyGesture::_bind_methods() {
    SE_BIND_METHOD(InputEventMagnifyGesture, set_factor);
    SE_BIND_METHOD(InputEventMagnifyGesture, get_factor);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "factor"), "set_factor", "get_factor");
}

InputEventMagnifyGesture::InputEventMagnifyGesture() {
    factor = 1.0f;
}
/////////////////////////////

void InputEventPanGesture::set_delta(const Vector2 &p_delta) {
    delta = p_delta;
}

Vector2 InputEventPanGesture::get_delta() const {
    return delta;
}

Ref<InputEvent> InputEventPanGesture::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
    Ref<InputEventPanGesture> ev(make_ref_counted<InputEventPanGesture>());

    ev->set_device(get_device());
    ev->set_modifiers_from_event(this);

    ev->set_position(p_xform.xform(get_position() + p_local_ofs));
    ev->set_delta(get_delta());

    return ev;
}

String InputEventPanGesture::as_text() const {
    return "InputEventPanGesture : delta=(" + String(get_delta()) + "), position=(" + String(get_position()) + ")";
}

void InputEventPanGesture::_bind_methods() {
    SE_BIND_METHOD(InputEventPanGesture, set_delta);
    SE_BIND_METHOD(InputEventPanGesture, get_delta);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "delta"), "set_delta", "get_delta");
}

InputEventPanGesture::InputEventPanGesture() {
    delta = Vector2(0, 0);
}
/////////////////////////////

void InputEventMIDI::set_channel(const int p_channel) {
    channel = p_channel;
}

int InputEventMIDI::get_channel() const {
    return channel;
}

void InputEventMIDI::set_message(const int p_message) {
    message = p_message;
}

int InputEventMIDI::get_message() const {
    return message;
}

void InputEventMIDI::set_pitch(const int p_pitch) {
    pitch = p_pitch;
}

int InputEventMIDI::get_pitch() const {
    return pitch;
}

void InputEventMIDI::set_velocity(const int p_velocity) {
    velocity = p_velocity;
}

int InputEventMIDI::get_velocity() const {
    return velocity;
}

void InputEventMIDI::set_instrument(const int p_instrument) {
    instrument = p_instrument;
}

int InputEventMIDI::get_instrument() const {
    return instrument;
}

void InputEventMIDI::set_pressure(const int p_pressure) {
    pressure = p_pressure;
}

int InputEventMIDI::get_pressure() const {
    return pressure;
}

void InputEventMIDI::set_controller_number(const int p_controller_number) {
    controller_number = p_controller_number;
}

int InputEventMIDI::get_controller_number() const {
    return controller_number;
}

void InputEventMIDI::set_controller_value(const int p_controller_value) {
    controller_value = p_controller_value;
}

int InputEventMIDI::get_controller_value() const {
    return controller_value;
}

String InputEventMIDI::as_text() const {
    return "InputEventMIDI : channel=(" + ::to_string(get_channel()) + "), message=(" + ::to_string(get_message()) +
           ")";
}

void InputEventMIDI::_bind_methods() {
    SE_BIND_METHOD(InputEventMIDI, set_channel);
    SE_BIND_METHOD(InputEventMIDI, get_channel);
    SE_BIND_METHOD(InputEventMIDI, set_message);
    SE_BIND_METHOD(InputEventMIDI, get_message);
    SE_BIND_METHOD(InputEventMIDI, set_pitch);
    SE_BIND_METHOD(InputEventMIDI, get_pitch);
    SE_BIND_METHOD(InputEventMIDI, set_velocity);
    SE_BIND_METHOD(InputEventMIDI, get_velocity);
    SE_BIND_METHOD(InputEventMIDI, set_instrument);
    SE_BIND_METHOD(InputEventMIDI, get_instrument);
    SE_BIND_METHOD(InputEventMIDI, set_pressure);
    SE_BIND_METHOD(InputEventMIDI, get_pressure);
    MethodBinder::bind_method(
            D_METHOD("set_controller_number", { "controller_number" }), &InputEventMIDI::set_controller_number);
    SE_BIND_METHOD(InputEventMIDI, get_controller_number);
    MethodBinder::bind_method(
            D_METHOD("set_controller_value", { "controller_value" }), &InputEventMIDI::set_controller_value);
    SE_BIND_METHOD(InputEventMIDI, get_controller_value);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "channel"), "set_channel", "get_channel");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "message"), "set_message", "get_message");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "pitch"), "set_pitch", "get_pitch");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "velocity"), "set_velocity", "get_velocity");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "instrument"), "set_instrument", "get_instrument");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "pressure"), "set_pressure", "get_pressure");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "controller_number"), "set_controller_number", "get_controller_number");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "controller_value"), "set_controller_value", "get_controller_value");
}

InputEventMIDI::InputEventMIDI() {
    channel = 0;
    message = 0;
    pitch = 0;
    velocity = 0;
    instrument = 0;
    pressure = 0;
    controller_number = 0;
    controller_value = 0;
}
