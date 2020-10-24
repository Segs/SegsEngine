/*************************************************************************/
/*  websocket_peer.cpp                                                   */
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

#include "websocket_peer.h"
#include "core/method_bind.h"
#include "core/io/ip_address.h"

GDCINULL(WebSocketPeer)

IMPL_GDCLASS(WebSocketPeer)
VARIANT_ENUM_CAST(WebSocketPeer::WriteMode);

WebSocketPeer::WebSocketPeer() {
}

WebSocketPeer::~WebSocketPeer() {
}

void WebSocketPeer::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("get_write_mode"), &WebSocketPeer::get_write_mode);
    MethodBinder::bind_method(D_METHOD("set_write_mode", {"mode"}), &WebSocketPeer::set_write_mode);
    MethodBinder::bind_method(D_METHOD("is_connected_to_host"), &WebSocketPeer::is_connected_to_host);
    MethodBinder::bind_method(D_METHOD("was_string_packet"), &WebSocketPeer::was_string_packet);
    MethodBinder::bind_method(D_METHOD("close", {"code", "reason"}), &WebSocketPeer::close, {DEFVAL(1000), DEFVAL("")});
    MethodBinder::bind_method(D_METHOD("get_connected_host"), &WebSocketPeer::get_connected_host);
    MethodBinder::bind_method(D_METHOD("get_connected_port"), &WebSocketPeer::get_connected_port);
    MethodBinder::bind_method(D_METHOD("set_no_delay", {"enabled"}), &WebSocketPeer::set_no_delay);

    BIND_ENUM_CONSTANT(WRITE_MODE_TEXT);
    BIND_ENUM_CONSTANT(WRITE_MODE_BINARY);
}
