/*************************************************************************/
/*  input_default.cpp                                                    */
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

#include "input_default.h"

#include "core/class_db.h"
#include "core/input/input_map.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/input/default_controller_mappings.h"
#include "scene/resources/texture.h"
#include "servers/rendering_server.h"
#include "core/property_info.h"

IMPL_GDCLASS(InputDefault)

void InputDefault::SpeedTrack::update(const Vector2 &p_delta_p) {

    uint64_t tick = OS::get_singleton()->get_ticks_usec();
    uint32_t tdiff = tick - last_tick;
    float delta_t = tdiff / 1000000.0f;
    last_tick = tick;

    accum += p_delta_p;
    accum_t += delta_t;

    if (accum_t > max_ref_frame * 10)
        accum_t = max_ref_frame * 10;

    while (accum_t >= min_ref_frame) {

        float slice_t = min_ref_frame / accum_t;
        Vector2 slice = accum * slice_t;
        accum = accum - slice;
        accum_t -= min_ref_frame;

        speed = (slice / min_ref_frame).linear_interpolate(speed, min_ref_frame / max_ref_frame);
    }
}

void InputDefault::SpeedTrack::reset() {
    last_tick = OS::get_singleton()->get_ticks_usec();
    speed = Vector2();
    accum_t = 0;
}

InputDefault::SpeedTrack::SpeedTrack() {

    min_ref_frame = 0.1f;
    max_ref_frame = 0.3f;
    reset();
}

bool InputDefault::is_key_pressed(int p_scancode) const {

    _THREAD_SAFE_METHOD_
    return keys_pressed.contains(p_scancode);
}

bool InputDefault::is_mouse_button_pressed(int p_button) const {

    _THREAD_SAFE_METHOD_
    return (mouse_button_mask & (1 << (p_button - 1))) != 0;
}

static int _combine_device(int p_value, int p_device) {

    return p_value | (p_device << 20);
}

bool InputDefault::is_joy_button_pressed(int p_device, int p_button) const {

    _THREAD_SAFE_METHOD_
    return joy_buttons_pressed.contains(_combine_device(p_button, p_device));
}

bool InputDefault::is_action_pressed(const StringName &p_action) const {

    return action_state.contains(p_action) && action_state.at(p_action).pressed;
}

bool InputDefault::is_action_just_pressed(const StringName &p_action) const {

    const HashMap<StringName, Action>::const_iterator E = action_state.find(p_action);
    if (E==action_state.end())
        return false;

    if (Engine::get_singleton()->is_in_physics_frame()) {
        return E->second.pressed && E->second.physics_frame == Engine::get_singleton()->get_physics_frames();
    } else {
        return E->second.pressed && E->second.idle_frame == Engine::get_singleton()->get_idle_frames();
    }
}

bool InputDefault::is_action_just_released(const StringName &p_action) const {

    const HashMap<StringName, Action>::const_iterator E = action_state.find(p_action);
    if (E==action_state.end())
        return false;

    if (Engine::get_singleton()->is_in_physics_frame()) {
        return !E->second.pressed && E->second.physics_frame == Engine::get_singleton()->get_physics_frames();
    } else {
        return !E->second.pressed && E->second.idle_frame == Engine::get_singleton()->get_idle_frames();
    }
}

float InputDefault::get_action_strength(const StringName &p_action) const {
    const HashMap<StringName, Action>::const_iterator E = action_state.find(p_action);
    if (E==action_state.end())
        return 0.0f;

    return E->second.strength;
}

float InputDefault::get_joy_axis(int p_device, int p_axis) const {

    _THREAD_SAFE_METHOD_
    int c = _combine_device(p_axis, p_device);
    return _joy_axis.at(c,0);
}

StringName InputDefault::get_joy_name(int p_idx) {

    _THREAD_SAFE_METHOD_
    return joy_names[p_idx].name;
};

Vector2 InputDefault::get_joy_vibration_strength(int p_device) {
    if (joy_vibration.contains(p_device)) {
        return Vector2(joy_vibration[p_device].weak_magnitude, joy_vibration[p_device].strong_magnitude);
    } else {
        return Vector2(0, 0);
    }
}

uint64_t InputDefault::get_joy_vibration_timestamp(int p_device) {
    if (joy_vibration.contains(p_device)) {
        return joy_vibration[p_device].timestamp;
    } else {
        return 0;
    }
}

float InputDefault::get_joy_vibration_duration(int p_device) {
    if (joy_vibration.contains(p_device)) {
        return joy_vibration[p_device].duration;
    } else {
        return 0.f;
    }
}

static const char *_hex_str(uint8_t p_byte) {

    static const char *dict = "0123456789abcdef";
    thread_local char ret[3];
    ret[0] = dict[p_byte >> 4];
    ret[1] = dict[p_byte & 0xf];
    ret[2] = 0;

    return ret;
};

void InputDefault::joy_connection_changed(int p_idx, bool p_connected, StringName p_name, StringName p_guid) {

    _THREAD_SAFE_METHOD_
    Joypad js;
    js.name = p_connected ? p_name : StringName();
    js.uid = p_connected ? p_guid : StringName();

    if (p_connected) {

        String uidname(p_guid);
        if (p_guid.empty()) {
            int uidlen = MIN(StringView(p_name).length(), 16);
            for (int i = 0; i < uidlen; i++) {
                uidname += _hex_str(StringView(p_name)[i]);
            }
        }
        js.uid = StringName(uidname);
        js.connected = true;
        int mapping = fallback_mapping;
        for (int i = 0; i < map_db.size(); i++) {
            if (js.uid == map_db[i].uid) {
                mapping = i;
                js.name = map_db[i].name;
            }
        }
        js.mapping = mapping;
    } else {
        js.connected = false;
        for (int i = 0; i < JOY_BUTTON_MAX; i++) {

            if (i < JOY_AXIS_MAX)
                set_joy_axis(p_idx, i, 0.0f);

            int c = _combine_device(i, p_idx);
            joy_buttons_pressed.erase(c);
        }
    }
    joy_names[p_idx] = js;

    emit_signal("joy_connection_changed", p_idx, p_connected);
};

Vector3 InputDefault::get_gravity() const {

    _THREAD_SAFE_METHOD_
    return gravity;
}

Vector3 InputDefault::get_accelerometer() const {

    _THREAD_SAFE_METHOD_
    return accelerometer;
}

Vector3 InputDefault::get_magnetometer() const {

    _THREAD_SAFE_METHOD_
    return magnetometer;
}

Vector3 InputDefault::get_gyroscope() const {

    _THREAD_SAFE_METHOD_
    return gyroscope;
}

void InputDefault::parse_input_event(const Ref<InputEvent> &p_event) {

    _parse_input_event_impl(p_event, false);
}

void InputDefault::_parse_input_event_impl(const Ref<InputEvent> &p_event, bool p_is_emulated) {

    // Notes on mouse-touch emulation:
    // - Emulated mouse events are parsed, that is, re-routed to this method, so they make the same effects
    //   as true mouse events. The only difference is the situation is flagged as emulated so they are not
    //   emulated back to touch events in an endless loop.
    // - Emulated touch events are handed right to the main loop (i.e., the SceneTree) because they don't
    //   require additional handling by this class.

    _THREAD_SAFE_METHOD_

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_event);
    if (k && !k->is_echo() && k->get_keycode() != 0) {
        if (k->is_pressed())
            keys_pressed.insert(k->get_keycode());
        else
            keys_pressed.erase(k->get_keycode());
    }

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);

    if (mb) {

        if (mb->is_pressed()) {
            mouse_button_mask |= (1 << (mb->get_button_index() - 1));
        } else {
            mouse_button_mask &= ~(1 << (mb->get_button_index() - 1));
        }

        Point2 pos = mb->get_global_position();
        if (mouse_pos != pos) {
            set_mouse_position(pos);
        }

        if (main_loop && emulate_touch_from_mouse && !p_is_emulated && mb->get_button_index() == 1) {
            Ref<InputEventScreenTouch> touch_event(make_ref_counted<InputEventScreenTouch>());
            touch_event->set_pressed(mb->is_pressed());
            touch_event->set_position(mb->get_position());
            main_loop->input_event(touch_event);
        }
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);

    if (mm) {

        Point2 pos = mm->get_global_position();
        if (mouse_pos != pos) {
            set_mouse_position(pos);
        }

        if (main_loop && emulate_touch_from_mouse && !p_is_emulated && mm->get_button_mask() & 1) {
            Ref<InputEventScreenDrag> drag_event(make_ref_counted<InputEventScreenDrag>());

            drag_event->set_position(mm->get_position());
            drag_event->set_relative(mm->get_relative());
            drag_event->set_speed(mm->get_speed());

            main_loop->input_event(drag_event);
        }
    }

    Ref<InputEventScreenTouch> st = dynamic_ref_cast<InputEventScreenTouch>(p_event);

    if (st) {

        if (st->is_pressed()) {
            SpeedTrack &track = touch_speed_track[st->get_index()];
            track.reset();
        } else {
            // Since a pointer index may not occur again (OSs may or may not reuse them),
            // imperatively remove it from the map to keep no fossil entries in it
            touch_speed_track.erase(st->get_index());
        }

        if (emulate_mouse_from_touch) {

            bool translate = false;
            if (st->is_pressed()) {
                if (mouse_from_touch_index == -1) {
                    translate = true;
                    mouse_from_touch_index = st->get_index();
                }
            } else {
                if (st->get_index() == mouse_from_touch_index) {
                    translate = true;
                    mouse_from_touch_index = -1;
                }
            }

            if (translate) {
                Ref<InputEventMouseButton> button_event(make_ref_counted<InputEventMouseButton>());

                button_event->set_device(InputEvent::DEVICE_ID_TOUCH_MOUSE);
                button_event->set_position(st->get_position());
                button_event->set_global_position(st->get_position());
                button_event->set_pressed(st->is_pressed());
                button_event->set_button_index(BUTTON_LEFT);
                if (st->is_pressed()) {
                    button_event->set_button_mask(mouse_button_mask | (1 << (BUTTON_LEFT - 1)));
                } else {
                    button_event->set_button_mask(mouse_button_mask & ~(1 << (BUTTON_LEFT - 1)));
                }

                _parse_input_event_impl(button_event, true);
            }
        }
    }

    Ref<InputEventScreenDrag> sd = dynamic_ref_cast<InputEventScreenDrag>(p_event);

    if (sd) {

        SpeedTrack &track = touch_speed_track[sd->get_index()];
        track.update(sd->get_relative());
        sd->set_speed(track.speed);

        if (emulate_mouse_from_touch && sd->get_index() == mouse_from_touch_index) {

            Ref<InputEventMouseMotion> motion_event(make_ref_counted<InputEventMouseMotion>());

            motion_event->set_device(InputEvent::DEVICE_ID_TOUCH_MOUSE);
            motion_event->set_position(sd->get_position());
            motion_event->set_global_position(sd->get_position());
            motion_event->set_relative(sd->get_relative());
            motion_event->set_speed(sd->get_speed());
            motion_event->set_button_mask(mouse_button_mask);

            _parse_input_event_impl(motion_event, true);
        }
    }

    Ref<InputEventJoypadButton> jb = dynamic_ref_cast<InputEventJoypadButton>(p_event);

    if (jb) {

        int c = _combine_device(jb->get_button_index(), jb->get_device());

        if (jb->is_pressed())
            joy_buttons_pressed.insert(c);
        else
            joy_buttons_pressed.erase(c);
    }

    Ref<InputEventJoypadMotion> jm = dynamic_ref_cast<InputEventJoypadMotion>(p_event);

    if (jm) {
        set_joy_axis(jm->get_device(), jm->get_axis(), jm->get_axis_value());
    }

    Ref<InputEventGesture> ge = dynamic_ref_cast<InputEventGesture>(p_event);

    if (ge) {

        if (main_loop) {
            main_loop->input_event(ge);
        }
    }

    for (auto &E :InputMap::get_singleton()->get_action_map()) {
        if (InputMap::get_singleton()->event_is_action(p_event, E.first)) {

            // Save the action's state
            if (!p_event->is_echo() && is_action_pressed(E.first) != p_event->is_action_pressed(E.first)) {
                Action action;
                action.physics_frame = Engine::get_singleton()->get_physics_frames();
                action.idle_frame = Engine::get_singleton()->get_idle_frames();
                action.pressed = p_event->is_action_pressed(E.first);
                action.strength = 0.f;
                action_state[E.first] = action;
            }
            action_state[E.first].strength = p_event->get_action_strength(E.first);
        }
    }

    if (main_loop)
        main_loop->input_event(p_event);
}

void InputDefault::set_joy_axis(int p_device, int p_axis, float p_value) {

    _THREAD_SAFE_METHOD_
    int c = _combine_device(p_axis, p_device);
    _joy_axis[c] = p_value;
}

void InputDefault::start_joy_vibration(int p_device, float p_weak_magnitude, float p_strong_magnitude, float p_duration) {
    _THREAD_SAFE_METHOD_
    if (p_weak_magnitude < 0.f || p_weak_magnitude > 1.f || p_strong_magnitude < 0.f || p_strong_magnitude > 1.f) {
        return;
    }
    VibrationInfo vibration;
    vibration.weak_magnitude = p_weak_magnitude;
    vibration.strong_magnitude = p_strong_magnitude;
    vibration.duration = p_duration;
    vibration.timestamp = OS::get_singleton()->get_ticks_usec();
    joy_vibration[p_device] = vibration;
}

void InputDefault::stop_joy_vibration(int p_device) {
    _THREAD_SAFE_METHOD_
    VibrationInfo vibration;
    vibration.weak_magnitude = 0;
    vibration.strong_magnitude = 0;
    vibration.duration = 0;
    vibration.timestamp = OS::get_singleton()->get_ticks_usec();
    joy_vibration[p_device] = vibration;
}

void InputDefault::vibrate_handheld(int p_duration_ms) {
    OS::get_singleton()->vibrate_handheld(p_duration_ms);
}

void InputDefault::set_gravity(const Vector3 &p_gravity) {

    _THREAD_SAFE_METHOD_

    gravity = p_gravity;
}

void InputDefault::set_accelerometer(const Vector3 &p_accel) {

    _THREAD_SAFE_METHOD_

    accelerometer = p_accel;
}

void InputDefault::set_magnetometer(const Vector3 &p_magnetometer) {

    _THREAD_SAFE_METHOD_

    magnetometer = p_magnetometer;
}

void InputDefault::set_gyroscope(const Vector3 &p_gyroscope) {

    _THREAD_SAFE_METHOD_

    gyroscope = p_gyroscope;
}

void InputDefault::set_main_loop(MainLoop *p_main_loop) {
    main_loop = p_main_loop;
}

void InputDefault::set_mouse_position(const Point2 &p_posf) {

    mouse_speed_track.update(p_posf - mouse_pos);
    mouse_pos = p_posf;
}

Point2 InputDefault::get_mouse_position() const {

    return mouse_pos;
}
Point2 InputDefault::get_last_mouse_speed() const {

    return mouse_speed_track.speed;
}

int InputDefault::get_mouse_button_mask() const {

    return mouse_button_mask; // do not trust OS implementation, should remove it - OS::get_singleton()->get_mouse_button_state();
}

void InputDefault::warp_mouse_position(const Vector2 &p_to) {

    OS::get_singleton()->warp_mouse_position(p_to);
}

Point2i InputDefault::warp_mouse_motion(const Ref<InputEventMouseMotion> &p_motion, const Rect2 &p_rect) {

    // The relative distance reported for the next event after a warp is in the boundaries of the
    // size of the rect on that axis, but it may be greater, in which case there's not problem as fmod()
    // will warp it, but if the pointer has moved in the opposite direction between the pointer relocation
    // and the subsequent event, the reported relative distance will be less than the size of the rect
    // and thus fmod() will be disabled for handling the situation.
    // And due to this mouse warping mechanism being stateless, we need to apply some heuristics to
    // detect the warp: if the relative distance is greater than the half of the size of the relevant rect
    // (checked per each axis), it will be considered as the consequence of a former pointer warp.

    const Point2i rel_sgn(p_motion->get_relative().x >= 0.0f ? 1 : -1, p_motion->get_relative().y >= 0.0 ? 1 : -1);
    const Size2i warp_margin = p_rect.size * 0.5f;
    const Point2i rel_warped(
            Math::fmod(p_motion->get_relative().x + rel_sgn.x * warp_margin.x, p_rect.size.x) - rel_sgn.x * warp_margin.x,
            Math::fmod(p_motion->get_relative().y + rel_sgn.y * warp_margin.y, p_rect.size.y) - rel_sgn.y * warp_margin.y);

    const Point2i pos_local = p_motion->get_global_position() - p_rect.position;
    const Point2i pos_warped(Math::fposmod(pos_local.x, p_rect.size.x), Math::fposmod(pos_local.y, p_rect.size.y));
    if (pos_warped != pos_local) {
        OS::get_singleton()->warp_mouse_position(pos_warped + p_rect.position);
    }

    return rel_warped;
}

void InputDefault::iteration(float p_step) {
}

void InputDefault::action_press(const StringName &p_action, float p_strength) {

    Action action;

    action.physics_frame = Engine::get_singleton()->get_physics_frames();
    action.idle_frame = Engine::get_singleton()->get_idle_frames();
    action.pressed = true;
    action.strength = p_strength;

    action_state[p_action] = action;
}

void InputDefault::action_release(const StringName &p_action) {

    Action action;

    action.physics_frame = Engine::get_singleton()->get_physics_frames();
    action.idle_frame = Engine::get_singleton()->get_idle_frames();
    action.pressed = false;
    action.strength = 0.f;

    action_state[p_action] = action;
}

void InputDefault::set_emulate_touch_from_mouse(bool p_emulate) {

    emulate_touch_from_mouse = p_emulate;
}

bool InputDefault::is_emulating_touch_from_mouse() const {

    return emulate_touch_from_mouse;
}

// Calling this whenever the game window is focused helps unstucking the "touch mouse"
// if the OS or its abstraction class hasn't properly reported that touch pointers raised
void InputDefault::ensure_touch_mouse_raised() {

    if (mouse_from_touch_index != -1) {
        mouse_from_touch_index = -1;

        Ref<InputEventMouseButton> button_event(make_ref_counted<InputEventMouseButton>());

        button_event->set_device(InputEvent::DEVICE_ID_TOUCH_MOUSE);
        button_event->set_position(mouse_pos);
        button_event->set_global_position(mouse_pos);
        button_event->set_pressed(false);
        button_event->set_button_index(BUTTON_LEFT);
        button_event->set_button_mask(mouse_button_mask & ~(1 << (BUTTON_LEFT - 1)));

        _parse_input_event_impl(button_event, true);
    }
}

void InputDefault::set_emulate_mouse_from_touch(bool p_emulate) {

    emulate_mouse_from_touch = p_emulate;
}

bool InputDefault::is_emulating_mouse_from_touch() const {

    return emulate_mouse_from_touch;
}

Input::CursorShape InputDefault::get_default_cursor_shape() const {

    return default_shape;
}

void InputDefault::set_default_cursor_shape(CursorShape p_shape) {

    if (default_shape == p_shape)
        return;

    default_shape = p_shape;
    // The default shape is set in Viewport::_gui_input_event. To instantly
    // see the shape in the viewport we need to trigger a mouse motion event.
    Ref<InputEventMouseMotion> mm(make_ref_counted<InputEventMouseMotion>());
    mm->set_position(mouse_pos);
    mm->set_global_position(mouse_pos);
    parse_input_event(mm);
}

Input::CursorShape InputDefault::get_current_cursor_shape() const {

    return (Input::CursorShape)OS::get_singleton()->get_cursor_shape();
}

void InputDefault::set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
    if (Engine::get_singleton()->is_editor_hint())
        return;

    OS::get_singleton()->set_custom_mouse_cursor(p_cursor, (OS::CursorShape)p_shape, p_hotspot);
}

void InputDefault::accumulate_input_event(const Ref<InputEvent> &p_event) {
    ERR_FAIL_COND(not p_event);

    if (!use_accumulated_input) {
        parse_input_event(p_event);
        return;
    }
    if (!accumulated_events.empty() && accumulated_events.back()->accumulate(p_event)) {
        return; //event was accumulated, exit
    }

    accumulated_events.push_back(p_event);
}
void InputDefault::flush_accumulated_events() {

    while (accumulated_events.front()) {
        parse_input_event(accumulated_events.front());
        accumulated_events.pop_front();
    }
}

void InputDefault::set_use_accumulated_input(bool p_enable) {

    use_accumulated_input = p_enable;
}

void InputDefault::release_pressed_events() {

    flush_accumulated_events(); // this is needed to release actions strengths

    keys_pressed.clear();
    joy_buttons_pressed.clear();
    _joy_axis.clear();

    for (eastl::pair<const StringName,InputDefault::Action> &E : action_state) {
        if (E.second.pressed)
            action_release(E.first);
    }
}

InputDefault::InputDefault() {
    __thread__safe__.reset(new Mutex);

    use_accumulated_input = true;
    mouse_button_mask = 0;
    emulate_touch_from_mouse = false;
    emulate_mouse_from_touch = false;
    mouse_from_touch_index = -1;
    main_loop = nullptr;
    default_shape = CURSOR_ARROW;

    hat_map_default[HAT_UP].type = TYPE_BUTTON;
    hat_map_default[HAT_UP].index = JOY_DPAD_UP;
    hat_map_default[HAT_UP].value = 0;

    hat_map_default[HAT_RIGHT].type = TYPE_BUTTON;
    hat_map_default[HAT_RIGHT].index = JOY_DPAD_RIGHT;
    hat_map_default[HAT_RIGHT].value = 0;

    hat_map_default[HAT_DOWN].type = TYPE_BUTTON;
    hat_map_default[HAT_DOWN].index = JOY_DPAD_DOWN;
    hat_map_default[HAT_DOWN].value = 0;

    hat_map_default[HAT_LEFT].type = TYPE_BUTTON;
    hat_map_default[HAT_LEFT].index = JOY_DPAD_LEFT;
    hat_map_default[HAT_LEFT].value = 0;

    fallback_mapping = -1;
    // Parse default mappings.
    {
        int i = 0;
        while (DefaultControllerMappings::mappings[i]) {
            parse_mapping(DefaultControllerMappings::mappings[i++]);
        }
    }

    // If defined, parse SDL_GAMECONTROLLERCONFIG for possible new mappings/overrides.
    String env_var = OS::get_singleton()->get_environment("SDL_GAMECONTROLLERCONFIG");
    String env_mapping(env_var);
    if (!env_mapping.empty()) {

        Vector<StringView> entries = StringUtils::split(env_mapping,'\n');
        for (size_t i = 0; i < entries.size(); i++) {
            if (entries[i].empty())
                continue;
            parse_mapping(entries[i]);
        }
    }
}

InputDefault::~InputDefault() = default;

void InputDefault::joy_button(int p_device, int p_button, bool p_pressed) {

    _THREAD_SAFE_METHOD_;
    Joypad &joy = joy_names[p_device];
    //printf("got button %i, mapping is %i\n", p_button, joy.mapping);
    if (joy.last_buttons[p_button] == p_pressed) {
        return;
    }
    joy.last_buttons[p_button] = p_pressed;
    if (joy.mapping == -1) {
        _button_event(p_device, p_button, p_pressed);
        return;
    }
    const auto &buttons(map_db[joy.mapping].buttons);
    const Map<int, JoyEvent>::const_iterator el = buttons.find(p_button);
    if (el==buttons.end()) {
        //don't process un-mapped events for now, it could mess things up badly for devices with additional buttons/axis
        //return _button_event(p_last_id, p_device, p_button, p_pressed);
        return;
    }

    JoyEvent map = el->second;
    if (map.type == TYPE_BUTTON) {
        //fake additional axis event for triggers
        if (map.index == JOY_L2 || map.index == JOY_R2) {
            float value = p_pressed ? 1.0f : 0.0f;
            int axis = map.index == JOY_L2 ? JOY_ANALOG_L2 : JOY_ANALOG_R2;
            _axis_event(p_device, axis, value);
        }
        _button_event(p_device, map.index, p_pressed);
        return;
    }

    if (map.type == TYPE_AXIS) {
        _axis_event(p_device, map.index, p_pressed ? 1.0 : 0.0);
    }
    // no event?
}

void InputDefault::joy_axis(int p_device, int p_axis, const JoyAxis &p_value) {

    _THREAD_SAFE_METHOD_;

    ERR_FAIL_INDEX(p_axis, JOY_AXIS_MAX);

    Joypad &joy = joy_names[p_device];

    if (joy.last_axis[p_axis] == p_value.value) {
        return;
    }

    if (p_value.value > joy.last_axis[p_axis]) {

        if (p_value.value < joy.last_axis[p_axis] + joy.filter) {

            return;
        }
    } else if (p_value.value > joy.last_axis[p_axis] - joy.filter) {

        return;
    }

    //when changing direction quickly, insert fake event to release pending inputmap actions
    float last = joy.last_axis[p_axis];
    if (p_value.min == 0 && (last < 0.25f || last > 0.75f) && (last - 0.5f) * (p_value.value - 0.5f) < 0) {
        JoyAxis jx;
        jx.min = p_value.min;
        jx.value = p_value.value < 0.5f ? 0.6f : 0.4f;
        joy_axis(p_device, p_axis, jx);
    } else if (ABS(last) > 0.5f && last * p_value.value < 0) {
        JoyAxis jx;
        jx.min = p_value.min;
        jx.value = p_value.value < 0 ? 0.1f : -0.1f;
        joy_axis(p_device, p_axis, jx);
    }

    joy.last_axis[p_axis] = p_value.value;
    float val = p_value.min == 0 ? -1.0f + 2.0f * p_value.value : p_value.value;

    if (joy.mapping == -1) {
        _axis_event(p_device, p_axis, val);
        return;
    }
    const auto &axes(map_db[joy.mapping].axis);
    const Map<int, JoyEvent>::const_iterator el = axes.find(p_axis);
    if (el==axes.end()) {
        //return _axis_event(p_last_id, p_device, p_axis, p_value);
        return;
    }

    JoyEvent map = el->second;

    if (map.type == TYPE_BUTTON) {
        //send axis event for triggers
        if (map.index == JOY_L2 || map.index == JOY_R2) {
            float value = p_value.min == 0 ? p_value.value : 0.5f + p_value.value / 2.0f;
            int axis = map.index == JOY_L2 ? JOY_ANALOG_L2 : JOY_ANALOG_R2;
            _axis_event(p_device, axis, value);
        }

        if (map.index == JOY_DPAD_UP || map.index == JOY_DPAD_DOWN) {
            bool pressed = p_value.value != 0.0f;
            int button = p_value.value < 0 ? JOY_DPAD_UP : JOY_DPAD_DOWN;

            if (!pressed) {
                if (joy_buttons_pressed.contains(_combine_device(JOY_DPAD_UP, p_device))) {
                    _button_event(p_device, JOY_DPAD_UP, false);
                }
                if (joy_buttons_pressed.contains(_combine_device(JOY_DPAD_DOWN, p_device))) {
                    _button_event(p_device, JOY_DPAD_DOWN, false);
                }
            }
            if (pressed == joy_buttons_pressed.contains(_combine_device(button, p_device))) {
                return;
            }
            _button_event(p_device, button, true);
            return;
        }
        if (map.index == JOY_DPAD_LEFT || map.index == JOY_DPAD_RIGHT) {
            bool pressed = p_value.value != 0.0f;
            int button = p_value.value < 0 ? JOY_DPAD_LEFT : JOY_DPAD_RIGHT;

            if (!pressed) {
                if (joy_buttons_pressed.contains(_combine_device(JOY_DPAD_LEFT, p_device))) {
                    _button_event(p_device, JOY_DPAD_LEFT, false);
                }
                if (joy_buttons_pressed.contains(_combine_device(JOY_DPAD_RIGHT, p_device))) {
                    _button_event(p_device, JOY_DPAD_RIGHT, false);
                }
            }
            if (pressed == joy_buttons_pressed.contains(_combine_device(button, p_device))) {
                return;
            }
            _button_event(p_device, button, true);
            return;
        }
        float deadzone = p_value.min == 0 ? 0.5f : 0.0f;
        bool pressed = p_value.value > deadzone;
        if (pressed == joy_buttons_pressed.contains(_combine_device(map.index, p_device))) {
            // button already pressed or released, this is an axis bounce value
            return;
        }
        _button_event(p_device, map.index, pressed);
        return;
    }

    if (map.type == TYPE_AXIS) {

        _axis_event(p_device, map.index, val);
        return;
    }
    //printf("invalid mapping\n");
}

void InputDefault::joy_hat(int p_device, int p_val) {

    _THREAD_SAFE_METHOD_
    const Joypad &joy = joy_names[p_device];

    const JoyEvent *map;

    if (joy.mapping == -1) {
        map = hat_map_default;
    } else {
        map = map_db[joy.mapping].hat;
    }

    int cur_val = joy_names[p_device].hat_current;

    if ((p_val & HAT_MASK_UP) != (cur_val & HAT_MASK_UP)) {
        _button_event(p_device, map[HAT_UP].index, p_val & HAT_MASK_UP);
    }

    if ((p_val & HAT_MASK_RIGHT) != (cur_val & HAT_MASK_RIGHT)) {
        _button_event(p_device, map[HAT_RIGHT].index, p_val & HAT_MASK_RIGHT);
    }
    if ((p_val & HAT_MASK_DOWN) != (cur_val & HAT_MASK_DOWN)) {
        _button_event(p_device, map[HAT_DOWN].index, p_val & HAT_MASK_DOWN);
    }
    if ((p_val & HAT_MASK_LEFT) != (cur_val & HAT_MASK_LEFT)) {
        _button_event(p_device, map[HAT_LEFT].index, p_val & HAT_MASK_LEFT);
    }

    joy_names[p_device].hat_current = p_val;
}

void InputDefault::_button_event(int p_device, int p_index, bool p_pressed) {

    Ref<InputEventJoypadButton> ievent(make_ref_counted<InputEventJoypadButton>());
    ievent->set_device(p_device);
    ievent->set_button_index(p_index);
    ievent->set_pressed(p_pressed);

    parse_input_event(ievent);
}

void InputDefault::_axis_event(int p_device, int p_axis, float p_value) {

    Ref<InputEventJoypadMotion> ievent(make_ref_counted<InputEventJoypadMotion>());
    ievent->set_device(p_device);
    ievent->set_axis(p_axis);
    ievent->set_axis_value(p_value);

    parse_input_event(ievent);
};

InputDefault::JoyEvent InputDefault::_find_to_event(StringName p_to) {

    // string names of the SDL buttons in the same order as input_event.h godot buttons
    static const char *buttons[] = { "a", "b", "x", "y", "leftshoulder", "rightshoulder", "lefttrigger", "righttrigger", "leftstick", "rightstick", "back", "start", "dpup", "dpdown", "dpleft", "dpright", "guide", nullptr };

    static const char *axis[] = { "leftx", "lefty", "rightx", "righty", nullptr };

    JoyEvent ret;
    ret.type = -1;
    ret.index = 0;

    int i = 0;
    while (buttons[i]) {

        if (p_to == buttons[i]) {
            ret.type = TYPE_BUTTON;
            ret.index = i;
            ret.value = 0;
            return ret;
        }
        ++i;
    }

    i = 0;
    while (axis[i]) {

        if (p_to == axis[i]) {
            ret.type = TYPE_AXIS;
            ret.index = i;
            ret.value = 0;
            return ret;
        }
        ++i;
    }

    return ret;
};

void InputDefault::parse_mapping(StringView p_mapping) {

    _THREAD_SAFE_METHOD_;
    JoyDeviceMapping mapping;
    for (int i = 0; i < HAT_MAX; ++i)
        mapping.hat[i].index = 1024 + i;

    Vector<StringView> entry = StringUtils::split(p_mapping,',');
    if (entry.size() < 2) {
        return;
    }

    CharString uid;
    uid.resize(17);

    mapping.uid = StringName(entry[0]);
    mapping.name = StringName(entry[1]);

    size_t idx = 1;
    while (++idx < entry.size()) {

        if (entry[idx].empty())
            continue;

        String from(StringUtils::replace(StringUtils::get_slice(entry[idx],":", 1)," ", ""));
        StringName to(StringUtils::replace(StringUtils::get_slice(entry[idx],":", 0)," ", ""));

        JoyEvent to_event = _find_to_event(to);
        if (to_event.type == -1)
            continue;

        StringName etype(StringUtils::substr(from,0, 1));
        if (etype == "a") {

            int aid = StringUtils::to_int(StringUtils::substr(from,1, from.length() - 1));
            mapping.axis[aid] = to_event;

        } else if (etype == "b") {

            int bid = StringUtils::to_int(StringUtils::substr(from,1, from.length() - 1));
            mapping.buttons[bid] = to_event;

        } else if (etype == "h") {

            int hat_value = StringUtils::to_int(StringUtils::get_slice(from,".", 1));
            switch (hat_value) {
                case 1:
                    mapping.hat[HAT_UP] = to_event;
                    break;
                case 2:
                    mapping.hat[HAT_RIGHT] = to_event;
                    break;
                case 4:
                    mapping.hat[HAT_DOWN] = to_event;
                    break;
                case 8:
                    mapping.hat[HAT_LEFT] = to_event;
                    break;
            }
        }
    }
    map_db.push_back(mapping);
    //printf("added mapping with uuid %ls\n", mapping.uid.c_str());
};

void InputDefault::add_joy_mapping(StringView p_mapping, bool p_update_existing) {
    parse_mapping(p_mapping);
    if (!p_update_existing)
        return;
    assert(StringUtils::get_slice_count(p_mapping,',')>0);
    StringName uid(StringUtils::get_slice(p_mapping,',',0));

    for (eastl::pair<const int, Joypad> & e : joy_names) {
        if (uid == e.second.uid) {
            e.second.mapping = map_db.size() - 1;
        }
    }
}

void InputDefault::remove_joy_mapping(StringName p_guid) {
    for (int i = map_db.size() - 1; i >= 0; i--) {
        if (p_guid == map_db[i].uid) {
            map_db.erase_at(i);
        }
    }
    for (eastl::pair<const int, Joypad> & e : joy_names) {
        if (e.second.uid == p_guid) {
            e.second.mapping = -1;
        }
    }
}

void InputDefault::set_fallback_mapping(StringName p_guid) {

    for (int i = 0; i < map_db.size(); i++) {
        if (map_db[i].uid == p_guid) {
            fallback_mapping = i;
            return;
        }
    }
}

//Defaults to simple implementation for platforms with a fixed gamepad layout, like consoles.
bool InputDefault::is_joy_known(int p_device) {

    return OS::get_singleton()->is_joy_known(p_device);
}

StringName InputDefault::get_joy_guid(int p_device) const {
    return OS::get_singleton()->get_joy_guid(p_device);
}

//platforms that use the remapping system can override and call to these ones
bool InputDefault::is_joy_mapped(int p_device) {
    int mapping = joy_names[p_device].mapping;
    return mapping != -1 ? (mapping != fallback_mapping) : false;
}

StringName InputDefault::get_joy_guid_remapped(int p_device) const {
    ERR_FAIL_COND_V(!joy_names.contains(p_device), StringName());
    return joy_names.at(p_device).uid;
}

Array InputDefault::get_connected_joypads() {
    Array ret;
    for(auto & elem : joy_names)
    {
        if (elem.second.connected) {
            ret.push_back(elem.first);
        }
    }
    return ret;
}

static const char *_buttons[JOY_BUTTON_MAX] = {
    "Face Button Bottom",
    "Face Button Right",
    "Face Button Left",
    "Face Button Top",
    "L",
    "R",
    "L2",
    "R2",
    "L3",
    "R3",
    "Select",
    "Start",
    "DPAD Up",
    "DPAD Down",
    "DPAD Left",
    "DPAD Right"
};

static const char *_axes[JOY_AXIS_MAX] = {
    "Left Stick X",
    "Left Stick Y",
    "Right Stick X",
    "Right Stick Y",
    "",
    "",
    "L2",
    "R2",
    "",
    ""
};

StringName InputDefault::get_joy_button_string(int p_button) {
    ERR_FAIL_INDEX_V(p_button, JOY_BUTTON_MAX, StringName());

    return StringName(_buttons[p_button]);
}

int InputDefault::get_joy_button_index_from_string(StringView p_button) {
    for (int i = 0; i < JOY_BUTTON_MAX; i++) {
        if (StringView(_buttons[i]) == p_button) {
            return i;
        }
    }
    ERR_FAIL_V(-1);
}

int InputDefault::get_unused_joy_id() {
    for (int i = 0; i < JOYPADS_MAX; i++) {
        if (!joy_names.contains(i) || !joy_names[i].connected) {
            return i;
        }
    }
    return -1;
}

StringName InputDefault::get_joy_axis_string(int p_axis) {
    ERR_FAIL_INDEX_V(p_axis, JOY_AXIS_MAX, StringName());

    return StringName(_axes[p_axis]);
}

int InputDefault::get_joy_axis_index_from_string(StringView p_axis) {
    for (int i = 0; i < JOY_AXIS_MAX; i++) {
        if (StringView(_axes[i]) == p_axis) {
            return i;
        }
    }
    ERR_FAIL_V(-1);
}