/*************************************************************************/
/*  stream_peer_ssl.cpp                                                  */
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

#include "stream_peer_ssl.h"

#include "core/engine.h"
#include "core/method_bind.h"

IMPL_GDCLASS(StreamPeerSSL)
VARIANT_ENUM_CAST(StreamPeerSSL::Status);

StreamPeerSSL *(*StreamPeerSSL::_create)() = nullptr;

StreamPeerSSL *StreamPeerSSL::create() {

    if (_create)
        return _create();
    return nullptr;
}

bool StreamPeerSSL::available = false;


bool StreamPeerSSL::is_available() {
    return available;
}

void StreamPeerSSL::set_blocking_handshake_enabled(bool p_enabled) {
    blocking_handshake = p_enabled;
}

bool StreamPeerSSL::is_blocking_handshake_enabled() const {
    return blocking_handshake;
}

void StreamPeerSSL::_bind_methods() {

    BIND_METHOD(StreamPeerSSL,poll);
    MethodBinder::bind_method(D_METHOD("accept_stream", {"stream", "private_key", "certificate", "chain"}), &StreamPeerSSL::accept_stream, {DEFVAL(Ref<X509Certificate>())});
    MethodBinder::bind_method(D_METHOD("connect_to_stream", {"stream", "validate_certs", "for_hostname", "valid_certificate"}), &StreamPeerSSL::connect_to_stream, {DEFVAL(false), DEFVAL(String()), DEFVAL(Ref<X509Certificate>())});
    BIND_METHOD(StreamPeerSSL,get_status);
    BIND_METHOD(StreamPeerSSL,disconnect_from_stream);
    BIND_METHOD(StreamPeerSSL,set_blocking_handshake_enabled);
    BIND_METHOD(StreamPeerSSL,is_blocking_handshake_enabled);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "blocking_handshake"), "set_blocking_handshake_enabled", "is_blocking_handshake_enabled");

    BIND_ENUM_CONSTANT(STATUS_DISCONNECTED);
    BIND_ENUM_CONSTANT(STATUS_HANDSHAKING);
    BIND_ENUM_CONSTANT(STATUS_CONNECTED);
    BIND_ENUM_CONSTANT(STATUS_ERROR);
    BIND_ENUM_CONSTANT(STATUS_ERROR_HOSTNAME_MISMATCH);
}

StreamPeerSSL::StreamPeerSSL() {
    blocking_handshake = true;
}
