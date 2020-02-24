/*************************************************************************/
/*  packet_peer_udp.h                                                    */
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

#include "core/io/net_socket.h"
#include "core/io/packet_peer.h"

class PacketPeerUDP : public PacketPeer {
    GDCLASS(PacketPeerUDP, PacketPeer)

protected:
    enum {
        PACKET_BUFFER_SIZE = 65536
    };

    RingBuffer<uint8_t> rb;
    uint8_t recv_buffer[PACKET_BUFFER_SIZE];
    uint8_t packet_buffer[PACKET_BUFFER_SIZE];
    IP_Address packet_ip;
    int packet_port=0;
    int queue_count=0;

    IP_Address peer_addr;
    int peer_port=0;
    bool blocking=true;
    bool broadcast=false;
    Ref<NetSocket> _sock;

    static void _bind_methods();
public:
    String _get_packet_ip() const;

    Error _set_dest_address(const String &p_address, int p_port);
    Error _poll();

public:
    void set_blocking_mode(bool p_enable);

    Error listen(int p_port, const IP_Address &p_bind_address = IP_Address("*"), int p_recv_buffer_size = 65536);
    void close();
    Error wait();
    bool is_listening() const;
    IP_Address get_packet_address() const;
    int get_packet_port() const;
    void set_dest_address(const IP_Address &p_address, int p_port);

    Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;
    Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override;
    int get_available_packet_count() const override;
    int get_max_packet_size() const override;
    void set_broadcast_enabled(bool p_enabled);
    Error join_multicast_group(IP_Address p_multi_address, StringView p_if_name);
    Error leave_multicast_group(IP_Address p_multi_address, StringView p_if_name);

    PacketPeerUDP();
    ~PacketPeerUDP() override;
};
