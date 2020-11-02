/*************************************************************************/
/*  webrtc_peer_connection.cpp                                           */
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

#include "webrtc_peer_connection.h"

#include "core/dictionary.h"
#include "core/method_bind.h"

IMPL_GDCLASS(WebRTCPeerConnection)
VARIANT_ENUM_CAST(WebRTCPeerConnection::ConnectionState);

WebRTCPeerConnection *(*WebRTCPeerConnection::_create)() = nullptr;

Ref<WebRTCPeerConnection> WebRTCPeerConnection::create_ref() {

    return Ref<WebRTCPeerConnection>(create());
}

WebRTCPeerConnection *WebRTCPeerConnection::create() {

    if (!_create)
        return nullptr;
    return _create();
}

void WebRTCPeerConnection::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("initialize", {"configuration"}), &WebRTCPeerConnection::initialize);
    MethodBinder::bind_method(D_METHOD("create_data_channel", {"label", "options"}), &WebRTCPeerConnection::create_data_channel);
    MethodBinder::bind_method(D_METHOD("create_offer"), &WebRTCPeerConnection::create_offer);
    MethodBinder::bind_method(D_METHOD("set_local_description", {"type", "sdp"}), &WebRTCPeerConnection::set_local_description);
    MethodBinder::bind_method(D_METHOD("set_remote_description", {"type", "sdp"}), &WebRTCPeerConnection::set_remote_description);
    MethodBinder::bind_method(D_METHOD("add_ice_candidate", {"media", "index", "name"}), &WebRTCPeerConnection::add_ice_candidate);
    MethodBinder::bind_method(D_METHOD("poll"), &WebRTCPeerConnection::poll);
    MethodBinder::bind_method(D_METHOD("close"), &WebRTCPeerConnection::close);

    MethodBinder::bind_method(D_METHOD("get_connection_state"), &WebRTCPeerConnection::get_connection_state);

    ADD_SIGNAL(MethodInfo("session_description_created", PropertyInfo(VariantType::STRING, "type"), PropertyInfo(VariantType::STRING, "sdp")));
    ADD_SIGNAL(MethodInfo("ice_candidate_created", PropertyInfo(VariantType::STRING, "media"), PropertyInfo(VariantType::INT, "index"), PropertyInfo(VariantType::STRING, "name")));
    ADD_SIGNAL(MethodInfo("data_channel_received", PropertyInfo(VariantType::OBJECT, "channel")));

    BIND_ENUM_CONSTANT(STATE_NEW);
    BIND_ENUM_CONSTANT(STATE_CONNECTING);
    BIND_ENUM_CONSTANT(STATE_CONNECTED);
    BIND_ENUM_CONSTANT(STATE_DISCONNECTED);
    BIND_ENUM_CONSTANT(STATE_FAILED);
    BIND_ENUM_CONSTANT(STATE_CLOSED);
}

WebRTCPeerConnection::WebRTCPeerConnection() {
}

WebRTCPeerConnection::~WebRTCPeerConnection() {
}
