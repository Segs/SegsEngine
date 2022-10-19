/*************************************************************************/
/*  input.cpp                                                            */
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

#include "input.h"

#include "core/input/input_map.h"
#include "core/list.h"
#include "core/method_bind.h"
#include "core/method_enum_caster.h"
#include "core/os/os.h"
#include "core/os/thread_safe.h"
#include "core/project_settings.h"


using namespace eastl;

IMPL_GDCLASS(Input)

VARIANT_ENUM_CAST(Input::MouseMode);
VARIANT_ENUM_CAST(Input::CursorShape);

Input *Input::singleton = nullptr;

Input *Input::get_singleton() {

    return singleton;
}

void Input::set_mouse_mode(MouseMode p_mode) {
    ERR_FAIL_INDEX((int)p_mode, (int)MOUSE_MODE_MAX);
    OS::get_singleton()->set_mouse_mode((OS::MouseMode)p_mode);
}

Input::MouseMode Input::get_mouse_mode() const {

    return (MouseMode)OS::get_singleton()->get_mouse_mode();
}

void Input::_bind_methods() {

    SE_BIND_METHOD(Input,is_key_pressed);
    SE_BIND_METHOD(Input,is_mouse_button_pressed);
    SE_BIND_METHOD(Input,is_joy_button_pressed);
    SE_BIND_METHOD_WITH_DEFAULTS(Input, is_action_pressed,DEFVAL(false));
    SE_BIND_METHOD_WITH_DEFAULTS(Input, is_action_just_pressed,DEFVAL(false));
    SE_BIND_METHOD_WITH_DEFAULTS(Input, is_action_just_released,DEFVAL(false));
    SE_BIND_METHOD_WITH_DEFAULTS(Input, get_action_strength,DEFVAL(false));
    SE_BIND_METHOD_WITH_DEFAULTS(Input, get_action_raw_strength,DEFVAL(false));
    SE_BIND_METHOD(Input,get_axis);
    SE_BIND_METHOD_WITH_DEFAULTS(Input, get_vector,DEFVAL(-1.0f));
    SE_BIND_METHOD_WITH_DEFAULTS(Input, add_joy_mapping,DEFVAL(false));
    SE_BIND_METHOD(Input,remove_joy_mapping);
    SE_BIND_METHOD(Input,joy_connection_changed);
    SE_BIND_METHOD(Input,is_joy_known);
    SE_BIND_METHOD(Input,get_joy_axis);
    SE_BIND_METHOD(Input,get_joy_name);
    SE_BIND_METHOD(Input,get_joy_guid);
    SE_BIND_METHOD(Input,get_connected_joypads);
    SE_BIND_METHOD(Input,get_joy_vibration_strength);
    SE_BIND_METHOD(Input,get_joy_vibration_duration);
    SE_BIND_METHOD(Input,get_joy_button_string);
    SE_BIND_METHOD(Input,get_joy_button_index_from_string);
    SE_BIND_METHOD(Input,get_joy_axis_string);
    SE_BIND_METHOD(Input,get_joy_axis_index_from_string);
    SE_BIND_METHOD_WITH_DEFAULTS(Input, start_joy_vibration,DEFVAL(0));
    SE_BIND_METHOD(Input,stop_joy_vibration);
    SE_BIND_METHOD_WITH_DEFAULTS(Input, vibrate_handheld,DEFVAL(500));
    SE_BIND_METHOD(Input,get_gravity);
    SE_BIND_METHOD(Input,get_accelerometer);
    SE_BIND_METHOD(Input,get_magnetometer);
    SE_BIND_METHOD(Input,get_gyroscope);
    SE_BIND_METHOD(Input,set_gravity);
    SE_BIND_METHOD(Input,set_accelerometer);
    SE_BIND_METHOD(Input,set_magnetometer);
    SE_BIND_METHOD(Input,set_gyroscope);
    //BIND_METHOD(Input,get_mouse_position); - this is not the function you want
    SE_BIND_METHOD(Input,get_last_mouse_speed);
    SE_BIND_METHOD(Input,get_mouse_button_mask);
    SE_BIND_METHOD(Input,set_mouse_mode);
    SE_BIND_METHOD(Input,get_mouse_mode);
    SE_BIND_METHOD(Input,warp_mouse_position);
    SE_BIND_METHOD_WITH_DEFAULTS(Input, action_press,DEFVAL(1.f));
    SE_BIND_METHOD(Input,action_release);
    SE_BIND_METHOD_WITH_DEFAULTS(Input, set_default_cursor_shape,DEFVAL(CURSOR_ARROW));
    SE_BIND_METHOD(Input,get_current_cursor_shape);
    SE_BIND_METHOD_WITH_DEFAULTS(Input, set_custom_mouse_cursor,DEFVAL(CURSOR_ARROW), DEFVAL(Vector2()));
    SE_BIND_METHOD(Input,parse_input_event);
    SE_BIND_METHOD(Input,set_use_accumulated_input);
    SE_BIND_METHOD(Input, is_using_accumulated_input);
    SE_BIND_METHOD(Input,flush_buffered_events);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "mouse_mode"), "set_mouse_mode", "get_mouse_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_accumulated_input"), "set_use_accumulated_input", "is_using_accumulated_input");

    BIND_ENUM_CONSTANT(MOUSE_MODE_VISIBLE);
    BIND_ENUM_CONSTANT(MOUSE_MODE_HIDDEN);
    BIND_ENUM_CONSTANT(MOUSE_MODE_CAPTURED);
    BIND_ENUM_CONSTANT(MOUSE_MODE_CONFINED);
    BIND_ENUM_CONSTANT(MOUSE_MODE_CONFINED_HIDDEN);

    BIND_ENUM_CONSTANT(CURSOR_ARROW);
    BIND_ENUM_CONSTANT(CURSOR_IBEAM);
    BIND_ENUM_CONSTANT(CURSOR_POINTING_HAND);
    BIND_ENUM_CONSTANT(CURSOR_CROSS);
    BIND_ENUM_CONSTANT(CURSOR_WAIT);
    BIND_ENUM_CONSTANT(CURSOR_BUSY);
    BIND_ENUM_CONSTANT(CURSOR_DRAG);
    BIND_ENUM_CONSTANT(CURSOR_CAN_DROP);
    BIND_ENUM_CONSTANT(CURSOR_FORBIDDEN);
    BIND_ENUM_CONSTANT(CURSOR_VSIZE);
    BIND_ENUM_CONSTANT(CURSOR_HSIZE);
    BIND_ENUM_CONSTANT(CURSOR_BDIAGSIZE);
    BIND_ENUM_CONSTANT(CURSOR_FDIAGSIZE);
    BIND_ENUM_CONSTANT(CURSOR_MOVE);
    BIND_ENUM_CONSTANT(CURSOR_VSPLIT);
    BIND_ENUM_CONSTANT(CURSOR_HSPLIT);
    BIND_ENUM_CONSTANT(CURSOR_HELP);

    ADD_SIGNAL(MethodInfo("joy_connection_changed", PropertyInfo(VariantType::INT, "device"), PropertyInfo(VariantType::BOOL, "connected")));
}

Input::Input() {

    singleton = this;
}

//////////////////////////////////////////////////////////
