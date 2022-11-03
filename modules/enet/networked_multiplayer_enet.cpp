/*************************************************************************/
/*  networked_multiplayer_enet.cpp                                       */
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

#include "networked_multiplayer_enet.h"
#include "core/io/ip.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/method_bind.h"
#include "core/string_utils.inl"
#include "core/string_formatter.h"
#include "core/crypto/crypto.h"
#include "EASTL/deque.h"

#include <enet/enet.h>

IMPL_GDCLASS(NetworkedMultiplayerENet)

VARIANT_ENUM_CAST(NetworkedMultiplayerENet::CompressionMode);

struct NetworkedMultiplayerENet_Priv {
    ENetEvent event;
    ENetPeer *peer;
    ENetHost *host;
    Map<int, ENetPeer *> peer_map;
    NetworkedMultiplayerENet::CompressionMode compression_mode = NetworkedMultiplayerENet::COMPRESS_NONE;

    struct Packet {

        ENetPacket *packet;
        int from;
        int channel;
    };
    Dequeue<Packet> incoming_packets;
    Vector<uint8_t> src_compressor_mem;
    Vector<uint8_t> dst_compressor_mem;

    Ref<CryptoKey> dtls_key;
    Ref<X509Certificate> dtls_cert;
    bool dtls_enabled;
    bool dtls_verify;
    String dtls_hostname;

    Packet current_packet;
    ENetCompressor enet_compressor;

public:

    NetworkedMultiplayerENet_Priv() {
        current_packet.packet = nullptr;
        enet_compressor.context = this;
        enet_compressor.compress = enet_compress;
        enet_compressor.decompress = enet_decompress;
        enet_compressor.destroy = enet_compressor_destroy;

    }
    static size_t enet_compress(void *context, const ENetBuffer *inBuffers, size_t inBufferCount, size_t inLimit, enet_uint8 *outData, size_t outLimit);
    static size_t enet_decompress(void *context, const enet_uint8 *inData, size_t inLimit, enet_uint8 *outData, size_t outLimit);
    static void enet_compressor_destroy(void *context);
    void close_connection(uint32_t wait_usec,uint32_t unique_id) {
        bool peers_disconnected = false;
        for (eastl::pair<const int,ENetPeer *> &E : peer_map) {
            if (E.second) {
                enet_peer_disconnect_now(E.second, unique_id);
                int *id = (int *)(E.second->data);
                memdelete(id);
                peers_disconnected = true;
            }
        }

        if (peers_disconnected) {
            enet_host_flush(host);

            if (wait_usec > 0) {
                OS::get_singleton()->delay_usec(wait_usec); // Wait for disconnection packets to send
            }
        }

        enet_host_destroy(host);
        incoming_packets.clear();
        peer_map.clear();
    }
    void _pop_current_packet() {
        if (current_packet.packet) {
            enet_packet_destroy(current_packet.packet);
            current_packet.packet = nullptr;
            current_packet.from = 0;
            current_packet.channel = -1;
        }
    }
    void _setup_compressor();
};
#define D() ((NetworkedMultiplayerENet_Priv *)(private_data))

void NetworkedMultiplayerENet::set_transfer_mode(TransferMode p_mode) {

    transfer_mode = p_mode;
}
NetworkedMultiplayerPeer::TransferMode NetworkedMultiplayerENet::get_transfer_mode() const {

    return transfer_mode;
}

void NetworkedMultiplayerENet::set_target_peer(int p_peer) {

    target_peer = p_peer;
}

int NetworkedMultiplayerENet::get_packet_peer() const {

    ERR_FAIL_COND_V_MSG(!active, 1, "The multiplayer instance isn't currently active.");
    ERR_FAIL_COND_V(D()->incoming_packets.empty(), 1);

    return D()->incoming_packets.front().from;
}

int NetworkedMultiplayerENet::get_packet_channel() const {

    ERR_FAIL_COND_V_MSG(!active, -1, "The multiplayer instance isn't currently active.");
    ERR_FAIL_COND_V(D()->incoming_packets.empty(), -1);

    return D()->incoming_packets.front().channel;
}

int NetworkedMultiplayerENet::get_last_packet_channel() const {

    ERR_FAIL_COND_V_MSG(!active, -1, "The multiplayer instance isn't currently active.");
    ERR_FAIL_COND_V(!D()->current_packet.packet, -1);

    return D()->current_packet.channel;
}

Error NetworkedMultiplayerENet::create_server(int p_port, int p_max_clients, int p_in_bandwidth, int p_out_bandwidth) {

    ERR_FAIL_COND_V_MSG(active, ERR_ALREADY_IN_USE, "The multiplayer instance is already active.");
    ERR_FAIL_COND_V_MSG(p_port < 0 || p_port > 65535, ERR_INVALID_PARAMETER, "The port number must be set between 0 and 65535 (inclusive).");
    ERR_FAIL_COND_V_MSG(p_max_clients < 1 || p_max_clients > 4095, ERR_INVALID_PARAMETER, "The number of clients must be set between 1 and 4095 (inclusive).");
    ERR_FAIL_COND_V_MSG(p_in_bandwidth < 0, ERR_INVALID_PARAMETER, "The incoming bandwidth limit must be greater than or equal to 0 (0 disables the limit).");
    ERR_FAIL_COND_V_MSG(p_out_bandwidth < 0, ERR_INVALID_PARAMETER, "The outgoing bandwidth limit must be greater than or equal to 0 (0 disables the limit).");

    ENetAddress address;
    memset(&address, 0, sizeof(address));

    if (bind_ip.is_wildcard()) {
        address.wildcard = 1;
    } else {
        enet_address_set_ip(&address, bind_ip.get_ipv6(), 16);
    }
    address.port = p_port;

    D()->host = enet_host_create(&address /* the address to bind the server host to */,
            p_max_clients /* allow up to 32 clients and/or outgoing connections */,
            channel_count /* allow up to channel_count to be used */,
            p_in_bandwidth /* limit incoming bandwidth if > 0 */,
            p_out_bandwidth /* limit outgoing bandwidth if > 0 */);

    ERR_FAIL_COND_V_MSG(!D()->host, ERR_CANT_CREATE, "Couldn't create an ENet multiplayer server.");
#ifdef GODOT_ENET
    if (dtls_enabled) {
        enet_host_dtls_server_setup(host, dtls_key.ptr(), dtls_cert.ptr());
    }
    enet_host_refuse_new_connections(host, refuse_connections);
#endif
    D()->_setup_compressor();
    active = true;
    server = true;
    refuse_connections = false;
    unique_id = 1;
    connection_status = CONNECTION_CONNECTED;
    return OK;
}
Error NetworkedMultiplayerENet::create_client(StringView p_address, int p_port, int p_in_bandwidth, int p_out_bandwidth, int p_client_port) {

    ERR_FAIL_COND_V_MSG(active, ERR_ALREADY_IN_USE, "The multiplayer instance is already active.");
    ERR_FAIL_COND_V_MSG(p_port < 0 || p_port > 65535, ERR_INVALID_PARAMETER, "The server port number must be set between 0 and 65535 (inclusive).");
    ERR_FAIL_COND_V_MSG(p_client_port < 0 || p_client_port > 65535, ERR_INVALID_PARAMETER, "The client port number must be set between 0 and 65535 (inclusive).");
    ERR_FAIL_COND_V_MSG(p_in_bandwidth < 0, ERR_INVALID_PARAMETER, "The incoming bandwidth limit must be greater than or equal to 0 (0 disables the limit).");
    ERR_FAIL_COND_V_MSG(p_out_bandwidth < 0, ERR_INVALID_PARAMETER, "The outgoing bandwidth limit must be greater than or equal to 0 (0 disables the limit).");

    if (p_client_port != 0) {
        ENetAddress c_client;

        if (bind_ip.is_wildcard()) {
            c_client.wildcard = 1;
        } else {
            enet_address_set_ip(&c_client, bind_ip.get_ipv6(), 16);
        }

        c_client.port = p_client_port;

        D()->host = enet_host_create(&c_client /* create a client host */,
                1 /* only allow 1 outgoing connection */,
                channel_count /* allow up to channel_count to be used */,
                p_in_bandwidth /* limit incoming bandwidth if > 0 */,
                p_out_bandwidth /* limit outgoing bandwidth if > 0 */);
    } else {
        D()->host = enet_host_create(nullptr /* create a client host */,
                1 /* only allow 1 outgoing connection */,
                channel_count /* allow up to channel_count to be used */,
                p_in_bandwidth /* limit incoming bandwidth if > 0 */,
                p_out_bandwidth /* limit outgoing bandwidth if > 0 */);
    }

    ERR_FAIL_COND_V_MSG(!D()->host, ERR_CANT_CREATE, "Couldn't create the ENet client host.");

    D()->_setup_compressor();

    IP_Address ip;
    if (StringUtils::is_valid_ip_address(p_address)) {
        ip = p_address;
    } else {
        ip = IP::get_singleton()->resolve_hostname(p_address);
        ERR_FAIL_COND_V_MSG(!ip.is_valid(), ERR_CANT_RESOLVE, "Couldn't resolve the server IP address or domain name.");
    }

    ENetAddress address;
    enet_address_set_ip(&address, ip.get_ipv6(), 16);
    address.port = p_port;

    unique_id = _gen_unique_id();

    // Initiate connection, allocating enough channels
    ENetPeer *peer = enet_host_connect(D()->host, &address, channel_count, unique_id);

    if (peer == nullptr) {
        enet_host_destroy(D()->host);
        ERR_FAIL_COND_V_MSG(!peer, ERR_CANT_CREATE, "Couldn't connect to the ENet multiplayer server.");
    }

    // Technically safe to ignore the peer or anything else.

    connection_status = CONNECTION_CONNECTING;
    active = true;
    server = false;
    refuse_connections = false;

    return OK;
}

void NetworkedMultiplayerENet::poll() {

    ERR_FAIL_COND_MSG(!active, "The multiplayer instance isn't currently active.");

    _pop_current_packet();

    if (!D()->host || !active) { // Might be disconnected
            return;
    }

    ENetEvent event;
        int ret = enet_host_service(D()->host, &event, 0);

        if (ret < 0) {
        ERR_FAIL_MSG("Enet host service error");
        } else if (ret == 0) {
        return; // No events
    }

    /* Keep servicing until there are no available events left in the queue. */
    do {
        if (!D()->host || !active) { // Check again after every event
            return;
        }

        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                // Store any relevant client information here.

                if (server && refuse_connections) {
                    enet_peer_reset(event.peer);
                    break;
                }

                // A client joined with an invalid ID (negative values, 0, and 1 are reserved).
                // Probably trying to exploit us.
                if (server && ((int)event.data < 2 || D()->peer_map.contains((int)event.data))) {
                    enet_peer_reset(event.peer);
                    ERR_CONTINUE(true);
                }

                int *new_id = memnew(int);
                *new_id = event.data;

                if (*new_id == 0) { // Data zero is sent by server (enet won't let you configure this). Server is always 1.
                    *new_id = 1;
                }

                event.peer->data = new_id;

                D()->peer_map[*new_id] = event.peer;

                connection_status = CONNECTION_CONNECTED; // If connecting, this means it connected to something!

                emit_signal("peer_connected", *new_id);

                if (server) {
                    // Do not notify other peers when server_relay is disabled.
                    if (!server_relay)
                        break;
                    // Someone connected, notify all the peers available
                    for (eastl::pair<const int,ENetPeer *> &E : D()->peer_map) {

                        if (E.first == *new_id)
                            continue;
                        // Send existing peers to new peer
                        ENetPacket *packet = enet_packet_create(nullptr, 8, ENET_PACKET_FLAG_RELIABLE);
                        encode_uint32(SYSMSG_ADD_PEER, &packet->data[0]);
                        encode_uint32(E.first, &packet->data[4]);
                        enet_peer_send(event.peer, SYSCH_CONFIG, packet);
                        // Send the new peer to existing peers
                        packet = enet_packet_create(nullptr, 8, ENET_PACKET_FLAG_RELIABLE);
                        encode_uint32(SYSMSG_ADD_PEER, &packet->data[0]);
                        encode_uint32(*new_id, &packet->data[4]);
                        enet_peer_send(E.second, SYSCH_CONFIG, packet);
                    }
                } else {

                    emit_signal("connection_succeeded");
                }

            } break;
            case ENET_EVENT_TYPE_DISCONNECT: {

                // Reset the peer's client information.

                int *id = (int *)event.peer->data;

                if (!id) {
                    if (!server) {
                        emit_signal("connection_failed");
                    }
                    // Never fully connected.
                    break;
                }

                if (!server) {

                    // Client just disconnected from server.
                    emit_signal("server_disconnected");
                    close_connection();
                    return;
                } else if (server_relay) {

                    // Server just received a client disconnect and is in relay mode, notify everyone else.
                    for (const eastl::pair<const int, ENetPeer *> &E : D()->peer_map) {

                        if (E.first == *id)
                            continue;

                        ENetPacket *packet = enet_packet_create(nullptr, 8, ENET_PACKET_FLAG_RELIABLE);
                        encode_uint32(SYSMSG_REMOVE_PEER, &packet->data[0]);
                        encode_uint32(*id, &packet->data[4]);
                        enet_peer_send(E.second, SYSCH_CONFIG, packet);
                    }
                }

                emit_signal("peer_disconnected", *id);
                D()->peer_map.erase(*id);
                memdelete(id);
            } break;
            case ENET_EVENT_TYPE_RECEIVE: {

                if (event.channelID == SYSCH_CONFIG) {
                    // Some config message
                    ERR_CONTINUE(event.packet->dataLength < 8);

                    // Only server can send config messages
                    ERR_CONTINUE(server);

                    int msg = decode_uint32(&event.packet->data[0]);
                    int id = decode_uint32(&event.packet->data[4]);

                    switch (msg) {
                        case SYSMSG_ADD_PEER: {

                            D()->peer_map[id] = nullptr;
                            emit_signal("peer_connected", id);

                        } break;
                        case SYSMSG_REMOVE_PEER: {

                            D()->peer_map.erase(id);
                            emit_signal("peer_disconnected", id);
                        } break;
                    }

                    enet_packet_destroy(event.packet);
                } else if (event.channelID < channel_count) {

                    NetworkedMultiplayerENet_Priv::Packet packet;
                    packet.packet = event.packet;

                    uint32_t *id = (uint32_t *)event.peer->data;

                    ERR_CONTINUE(event.packet->dataLength < 8);

                    uint32_t source = decode_uint32(&event.packet->data[0]);
                    int target = decode_uint32(&event.packet->data[4]);

                    packet.from = source;
                    packet.channel = event.channelID;

                    if (server) {
                        // Someone is cheating and trying to fake the source!
                        ERR_CONTINUE(source != *id);

                        packet.from = *id;

                        if (target == 1) {
                            // To myself and only myself
                            D()->incoming_packets.push_back(packet);
                        } else if (!server_relay) {
                            // No other destination is allowed when server is not relaying
                            continue;
                        } else if (target == 0) {
                            // Re-send to everyone but sender :|

                            D()->incoming_packets.push_back(packet);
                            // And make copies for sending
                            for (eastl::pair<const int,ENetPeer *> &E : D()->peer_map) {

                                if (uint32_t(E.first) == source) // Do not resend to self
                                    continue;

                                ENetPacket *packet2 = enet_packet_create(packet.packet->data, packet.packet->dataLength, packet.packet->flags);

                                enet_peer_send(E.second, event.channelID, packet2);
                            }

                        } else if (target < 0) {
                            // To all but one

                            // And make copies for sending
                            for (eastl::pair<const int,ENetPeer *> &E : D()->peer_map) {

                                if (uint32_t(E.first) == source || E.first == -target) // Do not resend to self, also do not send to excluded
                                    continue;

                                ENetPacket *packet2 = enet_packet_create(packet.packet->data, packet.packet->dataLength, packet.packet->flags);

                                enet_peer_send(E.second, event.channelID, packet2);
                            }

                            if (-target != 1) {
                                // Server is not excluded
                                D()->incoming_packets.push_back(packet);
                            } else {
                                // Server is excluded, erase packet
                                enet_packet_destroy(packet.packet);
                            }
                        } else {
                            // To someone else, specifically
                            ERR_CONTINUE(!D()->peer_map.contains(target));
                            enet_peer_send(D()->peer_map[target], event.channelID, packet.packet);
                        }
                    } else {

                        D()->incoming_packets.push_back(packet);
                    }

                    // Destroy packet later
                } else {
                    ERR_CONTINUE(true);
                }

            } break;
            case ENET_EVENT_TYPE_NONE: {
                // Do nothing
            } break;
        }
    } while (enet_host_check_events(D()->host, &event) > 0);
}

bool NetworkedMultiplayerENet::is_server() const {
    ERR_FAIL_COND_V_MSG(!active, false, "The multiplayer instance isn't currently active.");

    return server;
}

void NetworkedMultiplayerENet::close_connection(uint32_t wait_usec) {
    if (!active) {
        return;
    }

    ERR_FAIL_COND_MSG(!active, "The multiplayer instance isn't currently active.");

    _pop_current_packet();
    D()->close_connection(wait_usec,unique_id);

    active = false;
    unique_id = 1; // Server is 1
    connection_status = CONNECTION_DISCONNECTED;
}

void NetworkedMultiplayerENet::disconnect_peer(int p_peer, bool now) {

    ERR_FAIL_COND_MSG(!active, "The multiplayer instance isn't currently active.");
    ERR_FAIL_COND_MSG(!is_server(), "Can't disconnect a peer when not acting as a server.");
    ERR_FAIL_COND_MSG(!D()->peer_map.contains(p_peer), FormatVE("Peer ID %d not found in the list of peers.", p_peer));

    if (now) {
        int *id = (int *)D()->peer_map[p_peer]->data;
        enet_peer_disconnect_now(D()->peer_map[p_peer], 0);

        // enet_peer_disconnect_now doesn't generate ENET_EVENT_TYPE_DISCONNECT,
        // notify everyone else, send disconnect signal & remove from peer_map like in poll()
        if (server_relay) {
            for (const auto & peer_pair : D()->peer_map) {

                if (peer_pair.first == p_peer) {
                    continue;
                }

                ENetPacket *packet = enet_packet_create(nullptr, 8, ENET_PACKET_FLAG_RELIABLE);
                encode_uint32(SYSMSG_REMOVE_PEER, &packet->data[0]);
                encode_uint32(p_peer, &packet->data[4]);
                enet_peer_send(peer_pair.second, SYSCH_CONFIG, packet);
            }
        }
        memdelete(id);

        emit_signal("peer_disconnected", p_peer);
        D()->peer_map.erase(p_peer);
    } else {
        enet_peer_disconnect_later(D()->peer_map[p_peer], 0);
    }
}

int NetworkedMultiplayerENet::get_available_packet_count() const {

    return D()->incoming_packets.size();
}

Error NetworkedMultiplayerENet::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {

    ERR_FAIL_COND_V_MSG(D()->incoming_packets.empty(), ERR_UNAVAILABLE, "No incoming packets available.");

    _pop_current_packet();

    D()->current_packet = D()->incoming_packets.front();
    D()->incoming_packets.pop_front();

    *r_buffer = (const uint8_t *)(&D()->current_packet.packet->data[8]);
    r_buffer_size = D()->current_packet.packet->dataLength - 8;

    return OK;
}

Error NetworkedMultiplayerENet::put_packet(const uint8_t *p_buffer, int p_buffer_size) {

    ERR_FAIL_COND_V_MSG(!active, ERR_UNCONFIGURED, "The multiplayer instance isn't currently active.");
    ERR_FAIL_COND_V_MSG(connection_status != CONNECTION_CONNECTED, ERR_UNCONFIGURED, "The multiplayer instance isn't currently connected to any server or client.");

    int packet_flags = 0;
    int channel = SYSCH_RELIABLE;

    switch (transfer_mode) {
        case TRANSFER_MODE_UNRELIABLE: {
            if (always_ordered)
                packet_flags = 0;
            else
                packet_flags = ENET_PACKET_FLAG_UNSEQUENCED;
            channel = SYSCH_UNRELIABLE;
            packet_flags |= ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT;
        } break;
        case TRANSFER_MODE_UNRELIABLE_ORDERED: {
            packet_flags = ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT;
            channel = SYSCH_UNRELIABLE;
        } break;
        case TRANSFER_MODE_RELIABLE: {
            packet_flags = ENET_PACKET_FLAG_RELIABLE;
            channel = SYSCH_RELIABLE;
        } break;
    }

    if (transfer_channel > SYSCH_CONFIG) {
        channel = transfer_channel;

    }
#ifdef DEBUG_ENABLED
    if ((packet_flags & ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT) && p_buffer_size + 8 > ENET_HOST_DEFAULT_MTU) {
        WARN_PRINT_ONCE(FormatVE(
                "Sending %d bytes unrealiably which is above the MTU (%d), this will result in higher packet loss",
                p_buffer_size + 8, D()->host->mtu));
    }
#endif
    Map<int, ENetPeer *>::iterator E=D()->peer_map.end();

    if (target_peer != 0) {

        E = D()->peer_map.find(ABS(target_peer));
        ERR_FAIL_COND_V_MSG(E==D()->peer_map.end(), ERR_INVALID_PARAMETER, FormatVE("Invalid target peer '%d'.",target_peer));
    }

    ENetPacket *packet = enet_packet_create(nullptr, p_buffer_size + 8, packet_flags);
    encode_uint32(unique_id, &packet->data[0]); // Source ID
    encode_uint32(target_peer, &packet->data[4]); // Dest ID
    memcpy(&packet->data[8], p_buffer, p_buffer_size);

    if (server) {

        if (target_peer == 0) {
            enet_host_broadcast(D()->host, channel, packet);
        } else if (target_peer < 0) {
            // Send to all but one
            // and make copies for sending

            int exclude = -target_peer;

            for (eastl::pair<const int,ENetPeer *> &F : D()->peer_map) {

                if (F.first == exclude) // Exclude packet
                    continue;

                ENetPacket *packet2 = enet_packet_create(packet->data, packet->dataLength, packet_flags);

                enet_peer_send(F.second, channel, packet2);
            }

            enet_packet_destroy(packet); // Original packet no longer needed
        } else {
            enet_peer_send(E->second, channel, packet);
        }
    } else {

        ERR_FAIL_COND_V(!D()->peer_map.contains(1), ERR_BUG);
        enet_peer_send(D()->peer_map[1], channel, packet); // Send to server for broadcast
    }

    enet_host_flush(D()->host);

    return OK;
}

int NetworkedMultiplayerENet::get_max_packet_size() const {

    return 1 << 24; // Anything is good
}

void NetworkedMultiplayerENet::_pop_current_packet() {

    D()->_pop_current_packet();
}

NetworkedMultiplayerPeer::ConnectionStatus NetworkedMultiplayerENet::get_connection_status() const {

    return connection_status;
}

uint32_t NetworkedMultiplayerENet::_gen_unique_id() const {

    uint32_t hash = 0;

    while (hash == 0 || hash == 1) {

        hash = hash_djb2_one_32(
                (uint32_t)OS::get_singleton()->get_ticks_usec());
        hash = hash_djb2_one_32(
                (uint32_t)OS::get_singleton()->get_unix_time(), hash);
        hash = hash_djb2_one_32(
                (uint32_t)StringUtils::hash64(OS::get_singleton()->get_user_data_dir()), hash);
        hash = hash_djb2_one_32(
                (uint32_t)((uint64_t)this), hash); // Rely on ASLR heap
        hash = hash_djb2_one_32(
                (uint32_t)((uint64_t)&hash), hash); // Rely on ASLR stack

        hash = hash & 0x7FFFFFFF; // Make it compatible with unsigned, since negative ID is used for exclusion
    }

    return hash;
}

int NetworkedMultiplayerENet::get_unique_id() const {

    ERR_FAIL_COND_V_MSG(!active, 0, "The multiplayer instance isn't currently active.");
    return unique_id;
}

void NetworkedMultiplayerENet::set_refuse_new_connections(bool p_enable) {

    refuse_connections = p_enable;
}

bool NetworkedMultiplayerENet::is_refusing_new_connections() const {

    return refuse_connections;
}

void NetworkedMultiplayerENet::set_compression_mode(CompressionMode p_mode) {

    D()->compression_mode = p_mode;
}

NetworkedMultiplayerENet::CompressionMode NetworkedMultiplayerENet::get_compression_mode() const {

    return D()->compression_mode;
}

size_t NetworkedMultiplayerENet_Priv::enet_compress(void *context, const ENetBuffer *inBuffers, size_t inBufferCount, size_t inLimit, enet_uint8 *outData, size_t outLimit) {

    NetworkedMultiplayerENet_Priv *enet = (NetworkedMultiplayerENet_Priv *)(context);

    if (size_t(enet->src_compressor_mem.size()) < inLimit) {
        enet->src_compressor_mem.resize(inLimit);
    }

    int total = inLimit;
    int ofs = 0;
    while (total) {
        for (size_t i = 0; i < inBufferCount; i++) {
            int to_copy = MIN(total, int(inBuffers[i].dataLength));
            memcpy(&enet->src_compressor_mem[ofs], inBuffers[i].data, to_copy);
            ofs += to_copy;
            total -= to_copy;
        }
    }

    Compression::Mode mode;

    switch (enet->compression_mode) {
        case NetworkedMultiplayerENet::COMPRESS_FASTLZ: {
            mode = Compression::MODE_FASTLZ;
        } break;
        case NetworkedMultiplayerENet::COMPRESS_ZLIB: {
            mode = Compression::MODE_DEFLATE;
        } break;
        case NetworkedMultiplayerENet::COMPRESS_ZSTD: {
            mode = Compression::MODE_ZSTD;
        } break;
        default: {
            ERR_FAIL_V_MSG(0, FormatVE("Invalid ENet compression mode: %d", enet->compression_mode));
        }
    }

    int req_size = Compression::get_max_compressed_buffer_size(ofs, mode);
    if (enet->dst_compressor_mem.size() < req_size) {
        enet->dst_compressor_mem.resize(req_size);
    }
    int ret = Compression::compress(enet->dst_compressor_mem.data(), enet->src_compressor_mem.data(), ofs, mode);

    if (ret < 0)
        return 0;

    if (ret > int(outLimit))
        return 0; // Do not bother

    memcpy(outData, enet->dst_compressor_mem.data(), ret);

    return ret;
}

size_t NetworkedMultiplayerENet_Priv::enet_decompress(void *context, const enet_uint8 *inData, size_t inLimit, enet_uint8 *outData, size_t outLimit) {

    NetworkedMultiplayerENet_Priv *enet = (NetworkedMultiplayerENet_Priv *)(context);
    int ret = -1;
    switch (enet->compression_mode) {
        case NetworkedMultiplayerENet::COMPRESS_FASTLZ: {

            ret = Compression::decompress(outData, outLimit, inData, inLimit, Compression::MODE_FASTLZ);
        } break;
        case NetworkedMultiplayerENet::COMPRESS_ZLIB: {

            ret = Compression::decompress(outData, outLimit, inData, inLimit, Compression::MODE_DEFLATE);
        } break;
        case NetworkedMultiplayerENet::COMPRESS_ZSTD: {

            ret = Compression::decompress(outData, outLimit, inData, inLimit, Compression::MODE_ZSTD);
        } break;
        default: {
        }
    }
    if (ret < 0) {
        return 0;
    } else {
        return ret;
    }
}

void NetworkedMultiplayerENet_Priv::_setup_compressor() {

    switch (compression_mode) {

        case NetworkedMultiplayerENet::COMPRESS_NONE: {

            enet_host_compress(host, nullptr);
        } break;
        case NetworkedMultiplayerENet::COMPRESS_RANGE_CODER: {
            enet_host_compress_with_range_coder(host);
        } break;
        case NetworkedMultiplayerENet::COMPRESS_FASTLZ:
        case NetworkedMultiplayerENet::COMPRESS_ZLIB:
        case NetworkedMultiplayerENet::COMPRESS_ZSTD: {

            enet_host_compress(host, &enet_compressor);
        } break;
    }
}

void NetworkedMultiplayerENet_Priv::enet_compressor_destroy(void *context) {

    // Nothing to do
}

IP_Address NetworkedMultiplayerENet::get_peer_address(int p_peer_id) const {

    ERR_FAIL_COND_V_MSG(!D()->peer_map.contains(p_peer_id), IP_Address(), FormatVE("Peer ID %d not found in the list of peers.", p_peer_id));
    ERR_FAIL_COND_V_MSG(!is_server() && p_peer_id != 1, IP_Address(), "Can't get the address of peers other than the server (ID -1) when acting as a client.");
    ERR_FAIL_COND_V_MSG(D()->peer_map[p_peer_id] == NULL, IP_Address(), FormatVE("Peer ID %d found in the list of peers, but is null.", p_peer_id));

    IP_Address out;
    out.set_ipv6((uint8_t *)&(D()->peer_map.at(p_peer_id)->address.host));

    return out;
}

int NetworkedMultiplayerENet::get_peer_port(int p_peer_id) const {
    ERR_FAIL_COND_V_MSG(!D()->peer_map.contains(p_peer_id), 0, FormatVE("Peer ID %d not found in the list of peers.", p_peer_id));
    ERR_FAIL_COND_V_MSG(!is_server() && p_peer_id != 1, 0, "Can't get the address of peers other than the server (ID -1) when acting as a client.");
    ERR_FAIL_COND_V_MSG(D()->peer_map[p_peer_id] == NULL, 0, FormatVE("Peer ID %d found in the list of peers, but is null.", p_peer_id));

    return D()->peer_map.at(p_peer_id)->address.port;
}

void NetworkedMultiplayerENet::set_peer_timeout(int p_peer_id, int p_timeout_limit, int p_timeout_min, int p_timeout_max)
{
    ERR_FAIL_COND_MSG(!D()->peer_map.contains(p_peer_id), FormatVE("Peer ID %d not found in the list of peers.", p_peer_id));
    ERR_FAIL_COND_MSG(!is_server() && p_peer_id != 1, "Can't change the timeout of peers other then the server when acting as a client.");
    ERR_FAIL_COND_MSG(D()->peer_map[p_peer_id] == nullptr, FormatVE("Peer ID %d found in the list of peers, but is null.", p_peer_id));
    ERR_FAIL_COND_MSG(p_timeout_limit > p_timeout_min || p_timeout_min > p_timeout_max, "Timeout limit must be less than minimum timeout, which itself must be less then maximum timeout");
    enet_peer_timeout(D()->peer_map[p_peer_id], p_timeout_limit, p_timeout_min, p_timeout_max);
}

void NetworkedMultiplayerENet::set_transfer_channel(int p_channel) {

    ERR_FAIL_COND_MSG(p_channel < -1 || p_channel >= channel_count, FormatVE("The transfer channel must be set between 0 and %d, inclusive (got %d).", channel_count - 1, p_channel));
    ERR_FAIL_COND_MSG(p_channel == SYSCH_CONFIG, FormatVE("The channel %d is reserved.", SYSCH_CONFIG));
    transfer_channel = p_channel;
}

int NetworkedMultiplayerENet::get_transfer_channel() const {
    return transfer_channel;
}

void NetworkedMultiplayerENet::set_channel_count(int p_channel) {

    ERR_FAIL_COND_MSG(active, "The channel count can't be set while the multiplayer instance is active.");
    ERR_FAIL_COND_MSG(p_channel < SYSCH_MAX, FormatVE("The channel count must be greater than or equal to %d to account for reserved channels (got %d).", SYSCH_MAX, p_channel));
    channel_count = p_channel;
}

int NetworkedMultiplayerENet::get_channel_count() const {
    return channel_count;
}

void NetworkedMultiplayerENet::set_always_ordered(bool p_ordered) {
    always_ordered = p_ordered;
}

bool NetworkedMultiplayerENet::is_always_ordered() const {
    return always_ordered;
}
void NetworkedMultiplayerENet::set_server_relay_enabled(bool p_enabled) {
    ERR_FAIL_COND_MSG(active, "Server relaying can't be toggled while the multiplayer instance is active.");

    server_relay = p_enabled;
}

bool NetworkedMultiplayerENet::is_server_relay_enabled() const {
    return server_relay;
}
void NetworkedMultiplayerENet::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("create_server", {"port", "max_clients", "in_bandwidth", "out_bandwidth"}), &NetworkedMultiplayerENet::create_server, {DEFVAL(32), DEFVAL(0), DEFVAL(0)});
    MethodBinder::bind_method(D_METHOD("create_client", {"address", "port", "in_bandwidth", "out_bandwidth", "client_port"}), &NetworkedMultiplayerENet::create_client, {DEFVAL(0), DEFVAL(0), DEFVAL(0)});
    MethodBinder::bind_method(D_METHOD("close_connection", {"wait_usec"}), &NetworkedMultiplayerENet::close_connection, {DEFVAL(100)});
    MethodBinder::bind_method(D_METHOD("disconnect_peer", {"id", "now"}), &NetworkedMultiplayerENet::disconnect_peer, {DEFVAL(false)});
    SE_BIND_METHOD(NetworkedMultiplayerENet,set_compression_mode);
    SE_BIND_METHOD(NetworkedMultiplayerENet,get_compression_mode);
    MethodBinder::bind_method(D_METHOD("set_bind_ip", {"ip"}), (void (NetworkedMultiplayerENet::*)(StringView))&NetworkedMultiplayerENet::set_bind_ip);
    SE_BIND_METHOD(NetworkedMultiplayerENet,get_peer_address);
    SE_BIND_METHOD(NetworkedMultiplayerENet,get_peer_port);
    SE_BIND_METHOD(NetworkedMultiplayerENet,set_peer_timeout);

    SE_BIND_METHOD(NetworkedMultiplayerENet,get_packet_channel);
    SE_BIND_METHOD(NetworkedMultiplayerENet,get_last_packet_channel);
    SE_BIND_METHOD(NetworkedMultiplayerENet,set_transfer_channel);
    SE_BIND_METHOD(NetworkedMultiplayerENet,get_transfer_channel);
    SE_BIND_METHOD(NetworkedMultiplayerENet,set_channel_count);
    SE_BIND_METHOD(NetworkedMultiplayerENet,get_channel_count);
    SE_BIND_METHOD(NetworkedMultiplayerENet,set_always_ordered);
    SE_BIND_METHOD(NetworkedMultiplayerENet,is_always_ordered);
    SE_BIND_METHOD(NetworkedMultiplayerENet,set_server_relay_enabled);
    SE_BIND_METHOD(NetworkedMultiplayerENet,is_server_relay_enabled);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "compression_mode", PropertyHint::Enum, "None,Range Coder,FastLZ,ZLib,ZStd"), "set_compression_mode", "get_compression_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "transfer_channel"), "set_transfer_channel", "get_transfer_channel");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "channel_count"), "set_channel_count", "get_channel_count");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "always_ordered"), "set_always_ordered", "is_always_ordered");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "server_relay"), "set_server_relay_enabled", "is_server_relay_enabled");

    BIND_ENUM_CONSTANT(COMPRESS_NONE);
    BIND_ENUM_CONSTANT(COMPRESS_RANGE_CODER);
    BIND_ENUM_CONSTANT(COMPRESS_FASTLZ);
    BIND_ENUM_CONSTANT(COMPRESS_ZLIB);
    BIND_ENUM_CONSTANT(COMPRESS_ZSTD);
}

NetworkedMultiplayerENet::NetworkedMultiplayerENet() {
    private_data = memnew(NetworkedMultiplayerENet_Priv);
    active = false;
    server = false;
    refuse_connections = false;
    server_relay = true;
    unique_id = 0;
    target_peer = 0;
    transfer_mode = TRANSFER_MODE_RELIABLE;
    channel_count = SYSCH_MAX;
    transfer_channel = -1;
    always_ordered = false;
    connection_status = CONNECTION_DISCONNECTED;
    bind_ip = IP_Address("*");
}

NetworkedMultiplayerENet::~NetworkedMultiplayerENet() {

    if (active) {
        close_connection();
    }
    memdelete(D());
    private_data=nullptr;
}

// Sets IP for ENet to bind when using create_server or create_client
// if no IP is set, then ENet bind to ENET_HOST_ANY
void NetworkedMultiplayerENet::set_bind_ip(const IP_Address &p_ip) {
    ERR_FAIL_COND_MSG(!p_ip.is_valid() && !p_ip.is_wildcard(), FormatVE("Invalid bind IP address: %s", String(p_ip).c_str()));

    bind_ip = p_ip;
}
#undef D
