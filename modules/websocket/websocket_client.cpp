/*************************************************************************/
/*  websocket_client.cpp                                                 */
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

#include "websocket_client.h"
#include "core/method_bind.h"
#include "core/io/ip_address.h"

GDCINULL(WebSocketClient)
IMPL_GDCLASS(WebSocketClient)

WebSocketClient::WebSocketClient() {

    verify_ssl = true;
}

WebSocketClient::~WebSocketClient() {
}

Error WebSocketClient::connect_to_url(StringView p_url, const PoolStringArray &p_protocols, bool gd_mp_api, const PoolVector<String> &p_custom_headers) {
    _is_multiplayer = gd_mp_api;

    String host(p_url);
    String path("/");
    auto p_len = String::npos;
    int port = 80;
    bool ssl = false;
    if (StringUtils::begins_with(host,"wss://")) {
        ssl = true; // we should implement this
        host = StringUtils::substr(host,6, host.length() - 6);
        port = 443;
    } else {
        ssl = false;
        if (StringUtils::begins_with(host,"ws://"))
            host = StringUtils::substr(host,5, host.length() - 5);
    }

    // Path
    p_len = StringUtils::find(host,"/");
    if (p_len != String::npos) {
        path = StringUtils::substr(host,p_len, host.length() - p_len);
        host = StringUtils::substr(host,0, p_len);
    }

    // Port
    p_len = StringUtils::find_last(host,":");
    if (p_len != String::npos && p_len == StringUtils::find(host,":")) {
        port = StringUtils::to_int(StringUtils::substr(host,p_len, host.length() - p_len));
        host = StringUtils::substr(host,0, p_len);
    }

    return connect_to_host(host, path, port, ssl, p_protocols, p_custom_headers);
}

void WebSocketClient::set_verify_ssl_enabled(bool p_verify_ssl) {

    verify_ssl = p_verify_ssl;
}

bool WebSocketClient::is_verify_ssl_enabled() const {

    return verify_ssl;
}

Ref<X509Certificate> WebSocketClient::get_trusted_ssl_certificate() const {

    return ssl_cert;
}

void WebSocketClient::set_trusted_ssl_certificate(Ref<X509Certificate> p_cert) {

    ERR_FAIL_COND(get_connection_status() != CONNECTION_DISCONNECTED);
    ssl_cert = p_cert;
}

bool WebSocketClient::is_server() const {

    return false;
}

void WebSocketClient::_on_peer_packet() {

    if (_is_multiplayer) {
        _process_multiplayer(get_peer(1), 1);
    } else {
        emit_signal("data_received");
    }
}

void WebSocketClient::_on_connect(String p_protocol) {

    if (_is_multiplayer) {
        // need to wait for ID confirmation...
    } else {
        emit_signal("connection_established", StringName(p_protocol));
    }
}

void WebSocketClient::_on_close_request(int p_code, StringView p_reason) {

    emit_signal("server_close_request", p_code, p_reason);
}

void WebSocketClient::_on_disconnect(bool p_was_clean) {

    if (_is_multiplayer) {
        emit_signal("connection_failed");
    } else {
        emit_signal("connection_closed", p_was_clean);
    }
}

void WebSocketClient::_on_error() {

    if (_is_multiplayer) {
        emit_signal("connection_failed");
    } else {
        emit_signal("connection_error");
    }
}

void WebSocketClient::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("connect_to_url", {"url", "protocols", "gd_mp_api", "custom_headers"}), &WebSocketClient::connect_to_url, {DEFVAL(PoolStringArray()), DEFVAL(false), DEFVAL(PoolStringArray())});
    MethodBinder::bind_method(D_METHOD("disconnect_from_host", {"code", "reason"}), &WebSocketClient::disconnect_from_host, {DEFVAL(1000), DEFVAL("")});
    MethodBinder::bind_method(D_METHOD("get_connected_host"), &WebSocketClient::get_connected_host);
    MethodBinder::bind_method(D_METHOD("get_connected_port"), &WebSocketClient::get_connected_port);
    MethodBinder::bind_method(D_METHOD("set_verify_ssl_enabled", {"enabled"}), &WebSocketClient::set_verify_ssl_enabled);
    MethodBinder::bind_method(D_METHOD("is_verify_ssl_enabled"), &WebSocketClient::is_verify_ssl_enabled);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "verify_ssl", PropertyHint::None, "", 0), "set_verify_ssl_enabled", "is_verify_ssl_enabled");

    ADD_SIGNAL(MethodInfo("data_received"));
    ADD_SIGNAL(MethodInfo("connection_established", PropertyInfo(VariantType::STRING, "protocol")));
    ADD_SIGNAL(MethodInfo("server_close_request", PropertyInfo(VariantType::INT, "code"), PropertyInfo(VariantType::STRING, "reason")));
    ADD_SIGNAL(MethodInfo("connection_closed", PropertyInfo(VariantType::BOOL, "was_clean_close")));
    ADD_SIGNAL(MethodInfo("connection_error"));
}
