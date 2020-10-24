/*************************************************************************/
/*  websocket_server.h                                                   */
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

#pragma once

#include "core/reference.h"
#include "core/io/ip_address.h"
#include "websocket_multiplayer_peer.h"
#include "websocket_peer.h"

class CryptoKey;
class X509Certificate;

class GODOT_EXPORT WebSocketServer : public WebSocketMultiplayerPeer {

    GDCLASS(WebSocketServer,WebSocketMultiplayerPeer)
    GDCICLASS(WebSocketServer)

    IP_Address bind_ip;
protected:
    static void _bind_methods();

    Ref<CryptoKey> private_key;
    Ref<X509Certificate> ssl_cert;
    Ref<X509Certificate> ca_chain;
public:
    void poll() override = 0;
    virtual Error listen(int p_port, const PoolVector<String> &p_protocols = PoolVector<String>(), bool gd_mp_api = false) = 0;
    virtual void stop() = 0;
    virtual bool is_listening() const = 0;
    virtual bool has_peer(int p_id) const = 0;
    Ref<WebSocketPeer> get_peer(int p_id) const override = 0;
    bool is_server() const override;
    ConnectionStatus get_connection_status() const override;

    virtual IP_Address get_peer_address(int p_peer_id) const = 0;
    virtual int get_peer_port(int p_peer_id) const = 0;
    virtual void disconnect_peer(int p_peer_id, int p_code = 1000, StringView p_reason = {}) = 0;

    void _on_peer_packet(int32_t p_peer_id);
    void _on_connect(int32_t p_peer_id, StringView p_protocol);
    void _on_disconnect(int32_t p_peer_id, bool p_was_clean);
    void _on_close_request(int32_t p_peer_id, int p_code, StringView p_reason);

    IP_Address get_bind_ip() const;
    void set_bind_ip(const IP_Address &p_bind_ip);
    void set_bind_ip(StringView p_bind_ip) {
        set_bind_ip(IP_Address(p_bind_ip));
    }

    Ref<CryptoKey> get_private_key() const;
    void set_private_key(Ref<CryptoKey> p_key);

    Ref<X509Certificate> get_ssl_certificate() const;
    void set_ssl_certificate(Ref<X509Certificate> p_cert);

    Ref<X509Certificate> get_ca_chain() const;
    void set_ca_chain(Ref<X509Certificate> p_ca_chain);

    Error set_buffers(int p_in_buffer, int p_in_packets, int p_out_buffer, int p_out_packets) override = 0;

    WebSocketServer();
    ~WebSocketServer() override;
};
