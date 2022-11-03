/*************************************************************************/
/*  networked_multiplayer_enet.h                                         */
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

#include "core/io/ip_address.h"
#include "core/io/compression.h"
#include "core/io/networked_multiplayer_peer.h"
#include "core/map.h"
#include "core/list.h"

class GODOT_EXPORT NetworkedMultiplayerENet : public NetworkedMultiplayerPeer {

    GDCLASS(NetworkedMultiplayerENet,NetworkedMultiplayerPeer)

public:
    enum CompressionMode : int8_t {
        COMPRESS_NONE,
        COMPRESS_RANGE_CODER,
        COMPRESS_FASTLZ,
        COMPRESS_ZLIB,
        COMPRESS_ZSTD
    };

private:
    enum {
        SYSMSG_ADD_PEER,
        SYSMSG_REMOVE_PEER
    };

    enum {
        SYSCH_CONFIG,
        SYSCH_RELIABLE,
        SYSCH_UNRELIABLE,
        SYSCH_MAX
    };
    void *private_data;

    IP_Address bind_ip;
    uint32_t unique_id;
    int target_peer;
    int transfer_channel;
    int channel_count;
    TransferMode transfer_mode;

    ConnectionStatus connection_status;
    bool active;
    bool server;
    bool always_ordered;
    bool refuse_connections;
    bool server_relay;


    uint32_t _gen_unique_id() const;
    void _pop_current_packet();


protected:
    static void _bind_methods();

public:
    void set_transfer_mode(TransferMode p_mode) override;
    TransferMode get_transfer_mode() const override;
    void set_target_peer(int p_peer) override;

    int get_packet_peer() const override;

    virtual IP_Address get_peer_address(int p_peer_id) const;
    virtual int get_peer_port(int p_peer_id) const;
    void set_peer_timeout(int p_peer_id, int p_timeout_limit, int p_timeout_min, int p_timeout_max);

    Error create_server(int p_port, int p_max_clients = 32, int p_in_bandwidth = 0, int p_out_bandwidth = 0);
    Error create_client(StringView p_address, int p_port, int p_in_bandwidth = 0, int p_out_bandwidth = 0, int p_client_port = 0);

    void close_connection(uint32_t wait_usec = 100);

    void disconnect_peer(int p_peer, bool now = false);

    void poll() override;

    bool is_server() const override;

    int get_available_packet_count() const override;
    Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override; ///< buffer is GONE after next get_packet
    Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;

    int get_max_packet_size() const override;

    ConnectionStatus get_connection_status() const override;

    void set_refuse_new_connections(bool p_enable) override;
    bool is_refusing_new_connections() const override;

    int get_unique_id() const override;

    void set_compression_mode(CompressionMode p_mode);
    CompressionMode get_compression_mode() const;

    int get_packet_channel() const;
    int get_last_packet_channel() const;
    void set_transfer_channel(int p_channel);
    int get_transfer_channel() const;
    void set_channel_count(int p_channel);
    int get_channel_count() const;
    void set_always_ordered(bool p_ordered);
    bool is_always_ordered() const;
    void set_server_relay_enabled(bool p_enabled);
    bool is_server_relay_enabled() const;

    NetworkedMultiplayerENet();
    ~NetworkedMultiplayerENet() override;

    void set_bind_ip(const IP_Address &p_ip);
    void set_bind_ip(StringView p_ip) { set_bind_ip(IP_Address(p_ip));}
};
