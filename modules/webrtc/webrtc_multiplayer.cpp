/*************************************************************************/
/*  webrtc_multiplayer.cpp                                               */
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

#include "webrtc_multiplayer.h"

#include "core/io/marshalls.h"
#include "core/method_bind.h"
#include "core/os/os.h"

IMPL_GDCLASS(WebRTCMultiplayer)

void WebRTCMultiplayer::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("initialize", {"peer_id", "server_compatibility"}), &WebRTCMultiplayer::initialize, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("add_peer", {"peer", "peer_id", "unreliable_lifetime"}), &WebRTCMultiplayer::add_peer, {DEFVAL(1)});
    MethodBinder::bind_method(D_METHOD("remove_peer", {"peer_id"}), &WebRTCMultiplayer::remove_peer);
    MethodBinder::bind_method(D_METHOD("has_peer", {"peer_id"}), &WebRTCMultiplayer::has_peer);
    MethodBinder::bind_method(D_METHOD("get_peer", {"peer_id"}), &WebRTCMultiplayer::get_peer);
    MethodBinder::bind_method(D_METHOD("get_peers"), &WebRTCMultiplayer::get_peers);
    MethodBinder::bind_method(D_METHOD("close"), &WebRTCMultiplayer::close);
}

void WebRTCMultiplayer::set_transfer_mode(TransferMode p_mode) {
    transfer_mode = p_mode;
}

NetworkedMultiplayerPeer::TransferMode WebRTCMultiplayer::get_transfer_mode() const {
    return transfer_mode;
}

void WebRTCMultiplayer::set_target_peer(int p_peer_id) {
    target_peer = p_peer_id;
}

/* Returns the ID of the NetworkedMultiplayerPeer who sent the most recent packet: */
int WebRTCMultiplayer::get_packet_peer() const {
    return next_packet_peer;
}

bool WebRTCMultiplayer::is_server() const {
    return unique_id == TARGET_PEER_SERVER;
}

void WebRTCMultiplayer::poll() {
    if (peer_map.empty())
        return;

    Dequeue<int> remove;
    Dequeue<int> add;
    for (eastl::pair<const int,Ref<ConnectedPeer> > &E : peer_map) {
        Ref<ConnectedPeer> peer = E.second;
        peer->connection->poll();
        // Check peer state
        switch (peer->connection->get_connection_state()) {
            case WebRTCPeerConnection::STATE_NEW:
            case WebRTCPeerConnection::STATE_CONNECTING:
                // Go to next peer, not ready yet.
                continue;
            case WebRTCPeerConnection::STATE_CONNECTED:
                // Good to go, go ahead and check channel state.
                break;
            default:
                // Peer is closed or in error state. Got to next peer.
                remove.push_back(E.first);
                continue;
        }
        // Check channels state
        int ready = 0;
        for (const Ref<WebRTCDataChannel> &ch : peer->channels) {
            if(!ch)
                break;
            switch (ch->get_ready_state()) {
                case WebRTCDataChannel::STATE_CONNECTING:
                    continue;
                case WebRTCDataChannel::STATE_OPEN:
                    ready++;
                    continue;
                default:
                    // Channel was closed or in error state, remove peer id.
                    remove.push_back(E.first);
            }
            // We got a closed channel break out, the peer will be removed.
            break;
        }
        // This peer has newly connected, and all channels are now open.
        if (ready == peer->channels.size() && !peer->connected) {
            peer->connected = true;
            add.push_back(E.first);
        }
    }
    // Remove disconnected peers
    for (int E : remove) {
        remove_peer(E);
        if (next_packet_peer == E)
            next_packet_peer = 0;
    }
    // Signal newly connected peers
    for (int E : add) {
        // Already connected to server: simply notify new peer.
        // NOTE: Mesh is always connected.
        if (connection_status == CONNECTION_CONNECTED)
            emit_signal("peer_connected", E);

        // Server emulation mode suppresses peer_conencted until server connects.
        if (server_compat && E == TARGET_PEER_SERVER) {
            // Server connected.
            connection_status = CONNECTION_CONNECTED;
            emit_signal("peer_connected", TARGET_PEER_SERVER);
            emit_signal("connection_succeeded");
            // Notify of all previously connected peers
            for (eastl::pair<const int,Ref<ConnectedPeer> > &F : peer_map) {
                if (F.first != 1 && F.second->connected)
                    emit_signal("peer_connected", F.first);
            }
            break; // Because we already notified of all newly added peers.
        }
    }
    // Fetch next packet
    if (next_packet_peer == 0)
        _find_next_peer();
}

void WebRTCMultiplayer::_find_next_peer() {
    Map<int, Ref<ConnectedPeer> >::iterator E = peer_map.find(next_packet_peer);
    if (E!=peer_map.end())
        ++E;
    // After last.
    for( ;E!=peer_map.end(); ++E) {
        for (const Ref<WebRTCDataChannel> &F : E->second->channels) {
            if (F->get_available_packet_count()) {
                next_packet_peer = E->first;
                return;
            }
        }
    }
    // Before last
    for(E = peer_map.begin(); E!=peer_map.end(); ++E) {
        for (const Ref<WebRTCDataChannel> &F : E->second->channels) {
            if (F->get_available_packet_count()) {
                next_packet_peer = E->first;
                return;
            }
        }
        if (E->first == (int)next_packet_peer)
            break;
    }
    // No packet found
    next_packet_peer = 0;
}

void WebRTCMultiplayer::set_refuse_new_connections(bool p_enable) {
    refuse_connections = p_enable;
}

bool WebRTCMultiplayer::is_refusing_new_connections() const {
    return refuse_connections;
}

NetworkedMultiplayerPeer::ConnectionStatus WebRTCMultiplayer::get_connection_status() const {
    return connection_status;
}

Error WebRTCMultiplayer::initialize(int p_self_id, bool p_server_compat) {
    ERR_FAIL_COND_V(p_self_id < 0 || p_self_id > ~(1 << 31), ERR_INVALID_PARAMETER);
    unique_id = p_self_id;
    server_compat = p_server_compat;

    // Mesh and server are always connected
    if (!server_compat || p_self_id == 1)
        connection_status = CONNECTION_CONNECTED;
    else
        connection_status = CONNECTION_CONNECTING;
    return OK;
}

int WebRTCMultiplayer::get_unique_id() const {
    ERR_FAIL_COND_V(connection_status == CONNECTION_DISCONNECTED, 1);
    return unique_id;
}

void WebRTCMultiplayer::_peer_to_dict(Ref<ConnectedPeer> p_connected_peer, Dictionary &r_dict) {
    Array channels;
    for (const Ref<WebRTCDataChannel> &F : p_connected_peer->channels) {
        channels.push_back(F);
    }
    r_dict["connection"] = p_connected_peer->connection;
    r_dict["connected"] = p_connected_peer->connected;
    r_dict["channels"] = channels;
}

bool WebRTCMultiplayer::has_peer(int p_peer_id) {
    return peer_map.contains(p_peer_id);
}

Dictionary WebRTCMultiplayer::get_peer(int p_peer_id) {
    ERR_FAIL_COND_V(!peer_map.contains(p_peer_id), Dictionary());
    Dictionary out;
    _peer_to_dict(peer_map[p_peer_id], out);
    return out;
}

Dictionary WebRTCMultiplayer::get_peers() {
    Dictionary out;
    for (eastl::pair<const int,Ref<ConnectedPeer> > &E : peer_map) {
        Dictionary d;
        _peer_to_dict(E.second, d);
        out[E.first] = d;
    }
    return out;
}

Error WebRTCMultiplayer::add_peer(Ref<WebRTCPeerConnection> p_peer, int p_peer_id, int p_unreliable_lifetime) {
    ERR_FAIL_COND_V(p_peer_id < 0 || p_peer_id > ~(1 << 31), ERR_INVALID_PARAMETER);
    ERR_FAIL_COND_V(p_unreliable_lifetime < 0, ERR_INVALID_PARAMETER);
    ERR_FAIL_COND_V(refuse_connections, ERR_UNAUTHORIZED);
    // Peer must be valid, and in new state (to create data channels)
    ERR_FAIL_COND_V(not p_peer, ERR_INVALID_PARAMETER);
    ERR_FAIL_COND_V(p_peer->get_connection_state() != WebRTCPeerConnection::STATE_NEW, ERR_INVALID_PARAMETER);

    Ref<ConnectedPeer> peer(make_ref_counted<ConnectedPeer>());
    peer->connection = p_peer;

    // Initialize data channels
    Dictionary cfg;
    cfg["negotiated"] = true;
    cfg["ordered"] = true;

    cfg["id"] = 1;
    peer->channels[CH_RELIABLE] = p_peer->create_data_channel("reliable", cfg);
    ERR_FAIL_COND_V(not peer->channels[CH_RELIABLE], FAILED);

    cfg["id"] = 2;
    cfg["maxPacketLifetime"] = p_unreliable_lifetime;
    peer->channels[CH_ORDERED] = p_peer->create_data_channel("ordered", cfg);
    ERR_FAIL_COND_V(not peer->channels[CH_ORDERED], FAILED);

    cfg["id"] = 3;
    cfg["ordered"] = false;
    peer->channels[CH_UNRELIABLE] = p_peer->create_data_channel("unreliable", cfg);
    ERR_FAIL_COND_V(not peer->channels[CH_UNRELIABLE], FAILED);

    peer_map[p_peer_id] = peer; // add the new peer connection to the peer_map

    return OK;
}

void WebRTCMultiplayer::remove_peer(int p_peer_id) {
    ERR_FAIL_COND(!peer_map.contains(p_peer_id));
    Ref<ConnectedPeer> peer = peer_map[p_peer_id];
    peer_map.erase(p_peer_id);
    if (peer->connected) {
        peer->connected = false;
        emit_signal("peer_disconnected", p_peer_id);
        if (server_compat && p_peer_id == TARGET_PEER_SERVER) {
            emit_signal("server_disconnected");
            connection_status = CONNECTION_DISCONNECTED;
        }
    }
}

Error WebRTCMultiplayer::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
    // Peer not available
    if (next_packet_peer == 0 || !peer_map.contains(next_packet_peer)) {
        _find_next_peer();
        ERR_FAIL_V(ERR_UNAVAILABLE);
    }
    for (const Ref<WebRTCDataChannel> &E : peer_map[next_packet_peer]->channels) {
        if (E->get_available_packet_count()) {
            Error err = E->get_packet(r_buffer, r_buffer_size);
            _find_next_peer();
            return err;
        }
    }
    // Channels for that peer were empty. Bug?
    _find_next_peer();
    ERR_FAIL_V(ERR_BUG);
}

Error WebRTCMultiplayer::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
    ERR_FAIL_COND_V(connection_status == CONNECTION_DISCONNECTED, ERR_UNCONFIGURED);

    int ch = CH_RELIABLE;
    switch (transfer_mode) {
        case TRANSFER_MODE_RELIABLE:
            ch = CH_RELIABLE;
            break;
        case TRANSFER_MODE_UNRELIABLE_ORDERED:
            ch = CH_ORDERED;
            break;
        case TRANSFER_MODE_UNRELIABLE:
            ch = CH_UNRELIABLE;
            break;
    }

    Map<int, Ref<ConnectedPeer> >::iterator E = peer_map.end();

    if (target_peer > 0) {

        E = peer_map.find(target_peer);
        ERR_FAIL_COND_V_MSG(E==peer_map.end(), ERR_INVALID_PARAMETER, "Invalid target peer: " + itos(target_peer) + ".");

        ERR_FAIL_COND_V(E->second->channels.size() <= ch, ERR_BUG);
        ERR_FAIL_COND_V(not E->second->channels[ch], ERR_BUG);
        return E->second->channels[ch]->put_packet(p_buffer, p_buffer_size);

    } else {
        int exclude = -target_peer;

        for (eastl::pair<const int,Ref<ConnectedPeer> > &F : peer_map) {

            // Exclude packet. If target_peer == 0 then don't exclude any packets
            if (target_peer != 0 && F.first == exclude)
                continue;

            ERR_CONTINUE(F.second->channels.size() <= ch || not F.second->channels[ch]);
            F.second->channels[ch]->put_packet(p_buffer, p_buffer_size);
        }
    }
    return OK;
}

int WebRTCMultiplayer::get_available_packet_count() const {
    if (next_packet_peer == 0)
        return 0; // To be sure next call to get_packet works if size > 0 .
    int size = 0;
    for (const eastl::pair<const int,Ref<ConnectedPeer> > &E : peer_map) {
        for (const Ref<WebRTCDataChannel> &F : E.second->channels) {
            size += F->get_available_packet_count();
        }
    }
    return size;
}

int WebRTCMultiplayer::get_max_packet_size() const {
    return 1200;
}

void WebRTCMultiplayer::close() {
    peer_map.clear();
    unique_id = 0;
    next_packet_peer = 0;
    target_peer = 0;
    connection_status = CONNECTION_DISCONNECTED;
}

WebRTCMultiplayer::WebRTCMultiplayer() {
    unique_id = 0;
    next_packet_peer = 0;
    target_peer = 0;
    transfer_mode = TRANSFER_MODE_RELIABLE;
    refuse_connections = false;
    connection_status = CONNECTION_DISCONNECTED;
    server_compat = false;
}

WebRTCMultiplayer::~WebRTCMultiplayer() {
    close();
}
