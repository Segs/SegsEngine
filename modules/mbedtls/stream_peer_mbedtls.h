/*************************************************************************/
/*  stream_peer_mbedtls.h                                                */
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

#include "core/io/stream_peer_ssl.h"
#include "ssl_context_mbedtls.h"

class StreamPeerMbedTLS : public StreamPeerSSL {
private:
    Status status;
    se_string hostname;

    Ref<StreamPeer> base;

    static StreamPeerSSL *_create_func();

    static int bio_recv(void *ctx, unsigned char *buf, size_t len);
    static int bio_send(void *ctx, const unsigned char *buf, size_t len);
    void _cleanup();

protected:
    Ref<SSLContextMbedTLS> ssl_ctx;

    static void _bind_methods();

    Error _do_handshake();

public:
    void poll() override;
    Error accept_stream(Ref<StreamPeer> p_base, Ref<CryptoKey> p_key, Ref<X509Certificate> p_cert, Ref<X509Certificate> p_ca_chain = Ref<X509Certificate>()) override;
    Error connect_to_stream(Ref<StreamPeer> p_base, bool p_validate_certs = false, se_string_view p_for_hostname = {}, Ref<X509Certificate> p_valid_cert = Ref<X509Certificate>()) override;
    Status get_status() const override;

    void disconnect_from_stream() override;

    Error put_data(const uint8_t *p_data, int p_bytes) override;
    Error put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent) override;

    Error get_data(uint8_t *p_buffer, int p_bytes) override;
    Error get_partial_data(uint8_t *p_buffer, int p_bytes, int &r_received) override;

    int get_available_bytes() const override;

    static void initialize_ssl();
    static void finalize_ssl();

    StreamPeerMbedTLS();
    ~StreamPeerMbedTLS() override;
};
