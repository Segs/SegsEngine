/*************************************************************************/
/*  webrtc_data_channel.cpp                                              */
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

#include "webrtc_data_channel.h"
#include "core/project_settings.h"
#include "core/method_bind.h"

IMPL_GDCLASS(WebRTCDataChannel)
VARIANT_ENUM_CAST(WebRTCDataChannel::WriteMode);
VARIANT_ENUM_CAST(WebRTCDataChannel::ChannelState);

void WebRTCDataChannel::_bind_methods() {
    SE_BIND_METHOD(WebRTCDataChannel,poll);
    SE_BIND_METHOD(WebRTCDataChannel,close);

    SE_BIND_METHOD(WebRTCDataChannel,was_string_packet);
    SE_BIND_METHOD(WebRTCDataChannel,set_write_mode);
    SE_BIND_METHOD(WebRTCDataChannel,get_write_mode);
    SE_BIND_METHOD(WebRTCDataChannel,get_ready_state);
    SE_BIND_METHOD(WebRTCDataChannel,get_label);
    SE_BIND_METHOD(WebRTCDataChannel,is_ordered);
    SE_BIND_METHOD(WebRTCDataChannel,get_id);
    SE_BIND_METHOD(WebRTCDataChannel,get_max_packet_life_time);
    SE_BIND_METHOD(WebRTCDataChannel,get_max_retransmits);
    SE_BIND_METHOD(WebRTCDataChannel,get_protocol);
    SE_BIND_METHOD(WebRTCDataChannel,is_negotiated);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "write_mode", PropertyHint::Enum), "set_write_mode", "get_write_mode");

    BIND_ENUM_CONSTANT(WRITE_MODE_TEXT);
    BIND_ENUM_CONSTANT(WRITE_MODE_BINARY);

    BIND_ENUM_CONSTANT(STATE_CONNECTING);
    BIND_ENUM_CONSTANT(STATE_OPEN);
    BIND_ENUM_CONSTANT(STATE_CLOSING);
    BIND_ENUM_CONSTANT(STATE_CLOSED);
}

WebRTCDataChannel::WebRTCDataChannel() {
    _in_buffer_shift = nearest_shift(GLOBAL_GET(WRTC_IN_BUF).as<int>() - 1) + 10;
}

WebRTCDataChannel::~WebRTCDataChannel() {
}
