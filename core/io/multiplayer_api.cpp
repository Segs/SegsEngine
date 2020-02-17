/*************************************************************************/
/*  multiplayer_api.cpp                                                  */
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

#include "multiplayer_api.h"

#include "core/io/marshalls.h"
#include "core/method_bind.h"
#include "scene/main/node.h"
#include "core/script_language.h"
#include "core/io/networked_multiplayer_peer_enum_casters.h"
#ifdef DEBUG_ENABLED
#include "core/object_db.h"
#include "core/os/os.h"
#endif



IMPL_GDCLASS(MultiplayerAPI)

VARIANT_ENUM_CAST(MultiplayerAPI_RPCMode);


//! @warning MultiplayerAPI::DebugData methods are called with nullptr `this` in builds without defined `DEBUG_ENABLED`
//! So in those cases, they MUST behave like static methods ( not accessing any member fields of the class )
class MultiplayerAPI::DebugData {
public:
#ifdef DEBUG_ENABLED
    struct BandwidthFrame {
        uint32_t timestamp;
        int packet_size;
    };

    int bandwidth_incoming_pointer;
    Vector<BandwidthFrame> bandwidth_incoming_data;
    int bandwidth_outgoing_pointer;
    Vector<BandwidthFrame> bandwidth_outgoing_data;
    Map<ObjectID, ProfilingInfo> profiler_frame_data;
    bool profiling=false;
#endif
    enum Mode {
        Incoming=0,
        Outgoing=1,
    };
    int _get_bandwidth_usage(Mode m) {
        int total_bandwidth = 0;
#ifdef DEBUG_ENABLED
        const Vector<BandwidthFrame> &p_buffer = (m==Incoming) ? bandwidth_incoming_data : bandwidth_outgoing_data;
        int p_pointer = (m==Incoming) ? bandwidth_incoming_pointer : bandwidth_outgoing_pointer;
        uint32_t timestamp = OS::get_singleton()->get_ticks_msec();
        uint32_t final_timestamp = timestamp - 1000;

        int i = (p_pointer + p_buffer.size() - 1) % p_buffer.size();

        while (i != p_pointer && p_buffer[i].packet_size > 0) {
            if (p_buffer[i].timestamp < final_timestamp) {
                return total_bandwidth;
            }
            total_bandwidth += p_buffer[i].packet_size;
            i = (i + p_buffer.size() - 1) % p_buffer.size();
        }

        ERR_FAIL_COND_V_MSG(i == p_pointer, total_bandwidth, "Reached the end of the bandwidth profiler buffer, values might be inaccurate.");
#endif
        return total_bandwidth;
    }
    void _init_node_profile(ObjectID p_node) {
#ifdef DEBUG_ENABLED
        if (profiler_frame_data.contains(p_node))
            return;
        profiler_frame_data.emplace(p_node, ProfilingInfo());
        profiler_frame_data[p_node].node = p_node;
        profiler_frame_data[p_node].node_path = (String)object_cast<Node>(ObjectDB::get_instance(p_node))->get_path();
        profiler_frame_data[p_node].incoming_rpc = 0;
        profiler_frame_data[p_node].incoming_rset = 0;
        profiler_frame_data[p_node].outgoing_rpc = 0;
        profiler_frame_data[p_node].outgoing_rset = 0;
#endif
    }
    void record_packet(int p_packet_len)
    {
#ifdef DEBUG_ENABLED
        if (profiling) {
            bandwidth_incoming_data[bandwidth_incoming_pointer].timestamp = OS::get_singleton()->get_ticks_msec();
            bandwidth_incoming_data[bandwidth_incoming_pointer].packet_size = p_packet_len;
            bandwidth_incoming_pointer = (bandwidth_incoming_pointer + 1) % bandwidth_incoming_data.size();
        }
#endif
    }
    void record_rpc(Node *p_node)
    {
#ifdef DEBUG_ENABLED
        if (profiling) {
            ObjectID id = p_node->get_instance_id();
            _init_node_profile(id);
            profiler_frame_data[id].incoming_rpc += 1;
        }
#else
        (void)p_node;
#endif
    }
    void record_outgoing_rpc(Node *p_node)
    {
#ifdef DEBUG_ENABLED
        if (profiling) {
            ObjectID id = p_node->get_instance_id();
            _init_node_profile(id);
            profiler_frame_data[id].outgoing_rpc += 1;
        }
#else
        (void)p_node;
#endif
    }
    void record_rpc_call(int ofs)
    {
#ifdef DEBUG_ENABLED
        if (profiling) {
            bandwidth_outgoing_data[bandwidth_outgoing_pointer].timestamp = OS::get_singleton()->get_ticks_msec();
            bandwidth_outgoing_data[bandwidth_outgoing_pointer].packet_size = ofs;
            bandwidth_outgoing_pointer = (bandwidth_outgoing_pointer + 1) % bandwidth_outgoing_data.size();
        }
#else
        (void)ofs;
#endif
    }
    void record_outgoing_rset(Node *p_node)
    {
#ifdef DEBUG_ENABLED
    if (profiling) {
        ObjectID id = p_node->get_instance_id();
        _init_node_profile(id);
        profiler_frame_data[id].outgoing_rset += 1;
    }
#else
        (void)p_node;
#endif
    }
    void profiling_start() {
#ifdef DEBUG_ENABLED
        profiling = true;
        profiler_frame_data.clear();

        bandwidth_incoming_pointer = 0;
        bandwidth_incoming_data.resize(16384); // ~128kB
        for (BandwidthFrame & frm : bandwidth_incoming_data) {
            frm.packet_size = -1;
        }

        bandwidth_outgoing_pointer = 0;
        bandwidth_outgoing_data.resize(16384); // ~128kB
        for (BandwidthFrame & frm : bandwidth_outgoing_data) {
            frm.packet_size = -1;
        }
#endif
    }
    void profiling_end()
    {
#ifdef DEBUG_ENABLED
        profiling = false;
        bandwidth_incoming_data.clear();
        bandwidth_outgoing_data.clear();
#endif
    }
    int profiling_frame(ProfilingInfo *r_info) {
        int i = 0;
    #ifdef DEBUG_ENABLED
        for (eastl::pair<const ObjectID,ProfilingInfo> &E : profiler_frame_data) {
            r_info[i] = E.second;
            ++i;
        }
        profiler_frame_data.clear();
    #endif
        return i;
    }
};

namespace {
_FORCE_INLINE_ bool _should_call_local(MultiplayerAPI_RPCMode mode, bool is_master, bool &r_skip_rpc) {

    switch (mode) {

        case MultiplayerAPI_RPCMode::RPC_MODE_DISABLED: {
            // Do nothing.
        } break;
        case MultiplayerAPI_RPCMode::RPC_MODE_REMOTE: {
            // Do nothing also. Remote cannot produce a local call.
        } break;
        case MultiplayerAPI_RPCMode::RPC_MODE_MASTERSYNC: {
            if (is_master)
                r_skip_rpc = true; // I am the master, so skip remote call.
            FALLTHROUGH;
        }
        case MultiplayerAPI_RPCMode::RPC_MODE_REMOTESYNC:
        case MultiplayerAPI_RPCMode::RPC_MODE_PUPPETSYNC: {
            // Call it, sync always results in a local call.
            return true;
        }
        case MultiplayerAPI_RPCMode::RPC_MODE_MASTER: {
            if (is_master)
                r_skip_rpc = true; // I am the master, so skip remote call.
            return is_master;
        }
        case MultiplayerAPI_RPCMode::RPC_MODE_PUPPET: {
            return !is_master;
        }
    }
    return false;
}

_FORCE_INLINE_ bool _can_call_mode(Node *p_node, MultiplayerAPI_RPCMode mode, int p_remote_id) {
    switch (mode) {

        case MultiplayerAPI_RPCMode::RPC_MODE_DISABLED: {
            return false;
        }
        case MultiplayerAPI_RPCMode::RPC_MODE_REMOTE:
        case MultiplayerAPI_RPCMode::RPC_MODE_REMOTESYNC: {
            return true;
        }
        case MultiplayerAPI_RPCMode::RPC_MODE_MASTERSYNC:
        case MultiplayerAPI_RPCMode::RPC_MODE_MASTER: {
            return p_node->is_network_master();
        }
        case MultiplayerAPI_RPCMode::RPC_MODE_PUPPETSYNC:
        case MultiplayerAPI_RPCMode::RPC_MODE_PUPPET: {
            return !p_node->is_network_master() && p_remote_id == p_node->get_network_master();
        }
    }

    return false;
}
} // end of anonymous namespace

void MultiplayerAPI::poll() {

    if (not network_peer || network_peer->get_connection_status() == NetworkedMultiplayerPeer::CONNECTION_DISCONNECTED)
        return;

    network_peer->poll();

    if (not network_peer) // It's possible that polling might have resulted in a disconnection, so check here.
        return;

    while (network_peer->get_available_packet_count()) {

        int sender = network_peer->get_packet_peer();
        const uint8_t *packet;
        int len;

        Error err = network_peer->get_packet(&packet, len);
        if (err != OK) {
            ERR_PRINT("Error getting packet!");
            break; // Something is wrong!
        }

        rpc_sender_id = sender;
        _process_packet(sender, packet, len);
        rpc_sender_id = 0;

        if (not network_peer) {
            break; // It's also possible that a packet or RPC caused a disconnection, so also check here.
        }
    }
}

void MultiplayerAPI::clear() {
    connected_peers.clear();
    path_get_cache.clear();
    path_send_cache.clear();
    packet_cache.clear();
    last_send_cache_id = 1;
}

void MultiplayerAPI::set_root_node(Node *p_node) {
    root_node = p_node;
}

void MultiplayerAPI::set_network_peer(const Ref<NetworkedMultiplayerPeer> &p_peer) {

    if (p_peer == network_peer) return; // Nothing to do
    ERR_FAIL_COND_MSG(p_peer && p_peer->get_connection_status() == NetworkedMultiplayerPeer::CONNECTION_DISCONNECTED,
            "Supplied NetworkedMultiplayerPeer must be connecting or connected.");

    if (network_peer) {
        network_peer->disconnect("peer_connected", this, "_add_peer");
        network_peer->disconnect("peer_disconnected", this, "_del_peer");
        network_peer->disconnect("connection_succeeded", this, "_connected_to_server");
        network_peer->disconnect("connection_failed", this, "_connection_failed");
        network_peer->disconnect("server_disconnected", this, "_server_disconnected");
        clear();
    }

    network_peer = p_peer;

    if (network_peer) {
        network_peer->connect("peer_connected", this, "_add_peer");
        network_peer->connect("peer_disconnected", this, "_del_peer");
        network_peer->connect("connection_succeeded", this, "_connected_to_server");
        network_peer->connect("connection_failed", this, "_connection_failed");
        network_peer->connect("server_disconnected", this, "_server_disconnected");
    }
}

Ref<NetworkedMultiplayerPeer> MultiplayerAPI::get_network_peer() const {
    return network_peer;
}

void MultiplayerAPI::_process_packet(int p_from, const uint8_t *p_packet, int p_packet_len) {

    ERR_FAIL_COND_MSG(root_node == nullptr, "Multiplayer root node was not initialized. If you are using custom multiplayer, remember to set the root node via MultiplayerAPI.set_root_node before using it.");
    ERR_FAIL_COND_MSG(p_packet_len < 1, "Invalid packet received. Size too small.");


#ifdef DEBUG_ENABLED
    m_debug_data->record_packet(p_packet_len);
#endif
    uint8_t packet_type = p_packet[0];

    switch (packet_type) {

        case NETWORK_COMMAND_SIMPLIFY_PATH: {

            _process_simplify_path(p_from, p_packet, p_packet_len);
        } break;

        case NETWORK_COMMAND_CONFIRM_PATH: {

            _process_confirm_path(p_from, p_packet, p_packet_len);
        } break;

        case NETWORK_COMMAND_REMOTE_CALL:
        case NETWORK_COMMAND_REMOTE_SET: {

            ERR_FAIL_COND_MSG(p_packet_len < 6, "Invalid packet received. Size too small.");

            Node *node = _process_get_node(p_from, p_packet, p_packet_len);

            ERR_FAIL_COND_MSG(node == nullptr, "Invalid packet received. Requested node was not found.");

            // Detect cstring end.
            int len_end = 5;
            for (; len_end < p_packet_len; len_end++) {
                if (p_packet[len_end] == 0) {
                    break;
                }
            }

            ERR_FAIL_COND_MSG(len_end >= p_packet_len, "Invalid packet received. Size too small.");

            StringName name(((const char *)&p_packet[5]));

            if (packet_type == NETWORK_COMMAND_REMOTE_CALL) {

                _process_rpc(node, name, p_from, p_packet, p_packet_len, len_end + 1);

            } else {

                _process_rset(node, name, p_from, p_packet, p_packet_len, len_end + 1);
            }

        } break;

        case NETWORK_COMMAND_RAW: {

            _process_raw(p_from, p_packet, p_packet_len);
        } break;
    }
}

Node *MultiplayerAPI::_process_get_node(int p_from, const uint8_t *p_packet, int p_packet_len) {

    uint32_t target = decode_uint32(&p_packet[1]);
    Node *node = nullptr;

    if (target & 0x80000000) {
        // Use full path (not cached yet).

        int ofs = target & 0x7FFFFFFF;

        ERR_FAIL_COND_V_MSG(ofs >= p_packet_len, nullptr, "Invalid packet received. Size smaller than declared.");

        se_string_view paths((const char *)&p_packet[ofs], p_packet_len - ofs);

        NodePath np = (NodePath)paths;

        node = root_node->get_node(np);

        if (!node) {
            ERR_PRINT("Failed to get path from RPC: " + String(np) + ".");
        }
    } else {
        // Use cached path.
        uint32_t id = target;

        Map<int, PathGetCache>::iterator E = path_get_cache.find(p_from);
        ERR_FAIL_COND_V_MSG(E==path_get_cache.end(), nullptr, "Invalid packet received. Requests invalid peer cache.");

        Map<int, PathGetCache::NodeInfo>::iterator F = E->second.nodes.find(id);
        ERR_FAIL_COND_V_MSG(F==E->second.nodes.end(), nullptr, "Invalid packet received. Unabled to find requested cached node.");

        PathGetCache::NodeInfo *ni = &F->second;
        // Do proper caching later.

        node = root_node->get_node(ni->path);
        if (!node) {
            ERR_PRINT("Failed to get cached path from RPC: " + String(ni->path) + ".");
        }
    }
    return node;
}

void MultiplayerAPI::_process_rpc(Node *p_node, const StringName &p_name, int p_from, const uint8_t *p_packet, int p_packet_len, int p_offset) {

    ERR_FAIL_COND_MSG(p_offset >= p_packet_len, "Invalid packet received. Size too small.");

    // Check that remote can call the RPC on this node.
    MultiplayerAPI_RPCMode rpc_mode = RPC_MODE_DISABLED;
    const MultiplayerAPI_RPCMode *E = p_node->get_node_rpc_mode(p_name);
    if (E) {
        rpc_mode = *E;
    } else if (p_node->get_script_instance()) {
        rpc_mode = p_node->get_script_instance()->get_rpc_mode(p_name);
    }

    bool can_call = _can_call_mode(p_node, rpc_mode, p_from);
    ERR_FAIL_COND_MSG(!can_call, "RPC '" + String(p_name) + "' is not allowed on node " +
                                         (String)p_node->get_path() + " from: " + ::to_string(p_from) +
                                         ". Mode is " + ::to_string((int)rpc_mode) + ", master is " +
                                         ::to_string(p_node->get_network_master()) + ".");

    int argc = p_packet[p_offset];
    FixedVector<Variant,32,true> args;
    FixedVector<const Variant *,32,true> argp;
    args.resize(argc);
    argp.resize(argc);

    p_offset++;

    m_debug_data->record_rpc(p_node);

    for (int i = 0; i < argc; i++) {

        ERR_FAIL_COND_MSG(p_offset >= p_packet_len, "Invalid packet received. Size too small.");

        int vlen;
        Error err = decode_variant(args[i], &p_packet[p_offset], p_packet_len - p_offset, &vlen, allow_object_decoding || network_peer->is_object_decoding_allowed());
        ERR_FAIL_COND_MSG(err != OK, "Invalid packet received. Unable to decode RPC argument.");

        argp[i] = &args[i];
        p_offset += vlen;
    }

    Variant::CallError ce;

    p_node->call(p_name, (const Variant **)argp.data(), argc, ce);
    if (ce.error != Variant::CallError::CALL_OK) {
        String error = Variant::get_call_error_text(p_node, p_name, (const Variant **)argp.data(), argc, ce);
        error = "RPC - " + error;
        ERR_PRINT(error);
    }
}

void MultiplayerAPI::_process_rset(Node *p_node, const StringName &p_name, int p_from, const uint8_t *p_packet, int p_packet_len, int p_offset) {

    ERR_FAIL_COND_MSG(p_offset >= p_packet_len, "Invalid packet received. Size too small.");

    // Check that remote can call the RSET on this node.
    MultiplayerAPI_RPCMode rset_mode = RPC_MODE_DISABLED;
    const MultiplayerAPI_RPCMode *E = p_node->get_node_rset_mode(p_name);
    if (E) {
        rset_mode = *E;
    } else if (p_node->get_script_instance()) {
        rset_mode = p_node->get_script_instance()->get_rset_mode(p_name);
    }

    bool can_call = _can_call_mode(p_node, rset_mode, p_from);
    ERR_FAIL_COND_MSG(!can_call, "RSET '" + String(p_name) + "' is not allowed on node " + (String)p_node->get_path() +
                                         " from: " + ::to_string(p_from) + ". Mode is " + ::to_string((int)rset_mode) +
                                         ", master is " + ::to_string(p_node->get_network_master()) + ".");

    Variant value;
    Error err = decode_variant(value, &p_packet[p_offset], p_packet_len - p_offset, nullptr, allow_object_decoding || network_peer->is_object_decoding_allowed());

    ERR_FAIL_COND_MSG(err != OK, "Invalid packet received. Unable to decode RSET value.");

    bool valid;

    p_node->set(p_name, value, &valid);
    if (!valid) {
        String error = "Error setting remote property '" + String(p_name) + "', not found in object of type " + p_node->get_class() + ".";
        ERR_PRINT(error);
    }
}

void MultiplayerAPI::_process_simplify_path(int p_from, const uint8_t *p_packet, int p_packet_len) {

    ERR_FAIL_COND_MSG(p_packet_len < 5, "Invalid packet received. Size too small.");
    int id = decode_uint32(&p_packet[1]);

    se_string_view paths((const char *)&p_packet[5], p_packet_len - 5);

    NodePath path(paths);

    if (!path_get_cache.contains(p_from)) {
        path_get_cache[p_from] = PathGetCache();
    }

    PathGetCache::NodeInfo ni;
    ni.path = path;
    ni.instance = 0;

    path_get_cache[p_from].nodes[id] = ni;

    // Encode path to send ack.
    String pname(path);
    int len = encode_cstring(pname.data(), nullptr);

    Vector<uint8_t> packet;

    packet.resize(1 + len);
    packet[0] = NETWORK_COMMAND_CONFIRM_PATH;
    encode_cstring(pname.data(), &packet[1]);

    network_peer->set_transfer_mode(NetworkedMultiplayerPeer::TRANSFER_MODE_RELIABLE);
    network_peer->set_target_peer(p_from);
    network_peer->put_packet(packet.data(), packet.size());
}

void MultiplayerAPI::_process_confirm_path(int p_from, const uint8_t *p_packet, int p_packet_len) {

    ERR_FAIL_COND_MSG(p_packet_len < 2, "Invalid packet received. Size too small.");

    se_string_view paths((const char *)&p_packet[1], p_packet_len - 1);

    NodePath path(paths);

    auto psc = path_send_cache.find(path);
    ERR_FAIL_COND_MSG(path_send_cache.end()==psc, "Invalid packet received. Tries to confirm a path which was not found in cache.");

    Map<int, bool>:: iterator E = psc->second.confirmed_peers.find(p_from);
    ERR_FAIL_COND_MSG(E==psc->second.confirmed_peers.end(), "Invalid packet received. Source peer was not found in cache for the given path.");
    E->second = true;
}

bool MultiplayerAPI::_send_confirm_path(const NodePath& p_path, PathSentCache *psc, int p_target) {
    bool has_all_peers = true;
    Vector<int> peers_to_add; // If one is missing, take note to add it.

    for (int E : connected_peers) {

        if (p_target < 0 && E == -p_target)
            continue; // Continue, excluded.

        if (p_target > 0 && E != p_target)
            continue; // Continue, not for this peer.

        Map<int, bool>::iterator F = psc->confirmed_peers.find(E);

        if (F == psc->confirmed_peers.end() || !F->second) {
            // Path was not cached, or was cached but is unconfirmed.
            if (F== psc->confirmed_peers.end()) {
                // Not cached at all, take note.
                peers_to_add.push_back(E);
            }

            has_all_peers = false;
        }
    }

    // Those that need to be added, send a message for this.

    for (int peer : peers_to_add) {

        // Encode function name.
        String pname(p_path);
        int len = encode_cstring(pname.data(), nullptr);

        Vector<uint8_t> packet;

        packet.resize(1 + 4 + len);
        packet[0] = NETWORK_COMMAND_SIMPLIFY_PATH;
        encode_uint32(psc->id, &packet[1]);
        encode_cstring(pname.data(), &packet[5]);

        network_peer->set_target_peer(peer); // To all of you.
        network_peer->set_transfer_mode(NetworkedMultiplayerPeer::TRANSFER_MODE_RELIABLE);
        network_peer->put_packet(packet.data(), packet.size());

        psc->confirmed_peers.emplace(peer, false); // Insert into confirmed, but as false since it was not confirmed.
    }

    return has_all_peers;
}

void MultiplayerAPI::_send_rpc(Node *p_from, int p_to, bool p_unreliable, bool p_set, const StringName &p_name, const Variant **p_arg, int p_argcount) {

    ERR_FAIL_COND_MSG(not network_peer, "Attempt to remote call/set when networking is not active in SceneTree.");

    ERR_FAIL_COND_MSG(network_peer->get_connection_status() == NetworkedMultiplayerPeer::CONNECTION_CONNECTING, "Attempt to remote call/set when networking is not connected yet in SceneTree.");

    ERR_FAIL_COND_MSG(network_peer->get_connection_status() == NetworkedMultiplayerPeer::CONNECTION_DISCONNECTED, "Attempt to remote call/set when networking is disconnected.");

    ERR_FAIL_COND_MSG(p_argcount > 255, "Too many arguments >255.");

    if (p_to != 0 && !connected_peers.contains(ABS(p_to))) {
        ERR_FAIL_COND_MSG(p_to == network_peer->get_unique_id(), "Attempt to remote call/set yourself! unique ID: " + itos(network_peer->get_unique_id()) + ".");

        ERR_FAIL_MSG("Attempt to remote call unexisting ID: " + itos(p_to) + ".");
    }

    NodePath from_path = (root_node->get_path()).rel_path_to(p_from->get_path());
    ERR_FAIL_COND_MSG(from_path.is_empty(), "Unable to send RPC. Relative path is empty. THIS IS LIKELY A BUG IN THE ENGINE!");

    // See if the path is cached.
    auto psc = path_send_cache.find(from_path);
    if (path_send_cache.end()==psc) {
        // Path is not cached, create.
        psc = path_send_cache.emplace(eastl::make_pair(from_path, PathSentCache{{},last_send_cache_id++ })).first;
    }

    // Create base packet, lots of hardcode because it must be tight.

    size_t ofs = 0;

#define MAKE_ROOM(m_amount) \
    if (packet_cache.size() < m_amount) packet_cache.resize(m_amount);

    // Encode type.
    MAKE_ROOM(1)
    packet_cache[0] = p_set ? NETWORK_COMMAND_REMOTE_SET : NETWORK_COMMAND_REMOTE_CALL;
    ofs += 1;

    // Encode ID.
    MAKE_ROOM(ofs + 4)
    encode_uint32(psc->second.id, &packet_cache[ofs]);
    ofs += 4;

    // Encode function name.
    String name(p_name);
    int len = encode_cstring(name.data(), nullptr);
    MAKE_ROOM(ofs + len)
    encode_cstring(name.data(), &packet_cache[ofs]);
    ofs += len;

    if (p_set) {
        // Set argument.
        Error err = encode_variant(*p_arg[0], nullptr, len, allow_object_decoding || network_peer->is_object_decoding_allowed());
        ERR_FAIL_COND_MSG(err != OK, "Unable to encode RSET value. THIS IS LIKELY A BUG IN THE ENGINE!");
        MAKE_ROOM(ofs + len)
        encode_variant(*p_arg[0], &packet_cache[ofs], len, allow_object_decoding || network_peer->is_object_decoding_allowed());
        ofs += len;

    } else {
        // Call arguments.
        MAKE_ROOM(ofs + 1)
        packet_cache[ofs] = p_argcount;
        ofs += 1;
        for (int i = 0; i < p_argcount; i++) {
            Error err = encode_variant(*p_arg[i], nullptr, len, allow_object_decoding || network_peer->is_object_decoding_allowed());
            ERR_FAIL_COND_MSG(err != OK, "Unable to encode RPC argument. THIS IS LIKELY A BUG IN THE ENGINE!");
            MAKE_ROOM(ofs + len)
            encode_variant(*p_arg[i], &packet_cache[ofs], len, allow_object_decoding || network_peer->is_object_decoding_allowed());
            ofs += len;
        }
    }

    m_debug_data->record_rpc_call(ofs);

    // See if all peers have cached path (is so, call can be fast).
    bool has_all_peers = _send_confirm_path(from_path, &psc->second, p_to);

    // Take chance and set transfer mode, since all send methods will use it.
    network_peer->set_transfer_mode(p_unreliable ? NetworkedMultiplayerPeer::TRANSFER_MODE_UNRELIABLE : NetworkedMultiplayerPeer::TRANSFER_MODE_RELIABLE);

    if (has_all_peers) {

        // They all have verified paths, so send fast.
        network_peer->set_target_peer(p_to); // To all of you.
        network_peer->put_packet(packet_cache.data(), ofs); // A message with love.
    } else {
        // Not all verified path, so send one by one.

        // Append path at the end, since we will need it for some packets.
        String pname(from_path);
        int path_len = encode_cstring(pname.data(), nullptr);
        MAKE_ROOM(ofs + path_len)
        encode_cstring(pname.data(), &(packet_cache[ofs]));

        for (int E : connected_peers) {

            if (p_to < 0 && E == -p_to)
                continue; // Continue, excluded.

            if (p_to > 0 && E != p_to)
                continue; // Continue, not for this peer.

            Map<int, bool>::iterator F = psc->second.confirmed_peers.find(E);
            ERR_CONTINUE(F==psc->second.confirmed_peers.end()); // Should never happen.

            network_peer->set_target_peer(E); // To this one specifically.

            if (F->second) {
                // This one confirmed path, so use id.
                encode_uint32(psc->second.id, &packet_cache[1]);
                network_peer->put_packet(packet_cache.data(), ofs);
            } else {
                // This one did not confirm path yet, so use entire path (sorry!).
                encode_uint32(0x80000000 | ofs, &packet_cache[1]); // Offset to path and flag.
                network_peer->put_packet(packet_cache.data(), ofs + path_len);
            }
        }
    }
}

void MultiplayerAPI::_add_peer(int p_id) {
    connected_peers.insert(p_id);
    path_get_cache.emplace(p_id, PathGetCache());
    emit_signal("network_peer_connected", p_id);
}


void MultiplayerAPI::_del_peer(int p_id) {
    connected_peers.erase(p_id);
    // Cleanup get cache.
    path_get_cache.erase(p_id);
    // Cleanup sent cache.
    // Some refactoring is needed to make this faster and do paths GC.
    Vector<NodePath> keys;
    path_send_cache.keys_into(keys);
    for (const NodePath &E : keys) {
        auto psc = path_send_cache.find(E);
        psc->second.confirmed_peers.erase(p_id);
    }
    emit_signal("network_peer_disconnected", p_id);
}
void MultiplayerAPI::_connected_to_server() {

    emit_signal("connected_to_server");
}

void MultiplayerAPI::_connection_failed() {

    emit_signal("connection_failed");
}

void MultiplayerAPI::_server_disconnected() {

    emit_signal("server_disconnected");
}

void MultiplayerAPI::rpcp(Node *p_node, int p_peer_id, bool p_unreliable, const StringName &p_method, const Variant **p_arg, int p_argcount) {

    ERR_FAIL_COND_MSG(not network_peer, "Trying to call an RPC while no network peer is active.");
    ERR_FAIL_COND_MSG(!p_node->is_inside_tree(), "Trying to call an RPC on a node which is not inside SceneTree.");
    ERR_FAIL_COND_MSG(network_peer->get_connection_status() != NetworkedMultiplayerPeer::CONNECTION_CONNECTED, "Trying to call an RPC via a network peer which is not connected.");

    int node_id = network_peer->get_unique_id();
    bool skip_rpc = node_id == p_peer_id;
    bool call_local_native = false;
    bool call_local_script = false;
    bool is_master = p_node->is_network_master();

    if (p_peer_id == 0 || p_peer_id == node_id || (p_peer_id < 0 && p_peer_id != -node_id)) {
        // Check that send mode can use local call.

        const MultiplayerAPI_RPCMode *E = p_node->get_node_rpc_mode(p_method);
        if (E) {
            call_local_native = _should_call_local(*E, is_master, skip_rpc);
        }

        if (call_local_native) {
            // Done below.
        } else if (p_node->get_script_instance()) {
            // Attempt with script.
            MultiplayerAPI_RPCMode rpc_mode = p_node->get_script_instance()->get_rpc_mode(p_method);
            call_local_script = _should_call_local(rpc_mode, is_master, skip_rpc);
        }
    }

    if (!skip_rpc) {
        m_debug_data->record_outgoing_rpc(p_node);
        _send_rpc(p_node, p_peer_id, p_unreliable, false, p_method, p_arg, p_argcount);
    }

    if (call_local_native) {
        int temp_id = rpc_sender_id;
        rpc_sender_id = get_network_unique_id();
        Variant::CallError ce;
        p_node->call(p_method, p_arg, p_argcount, ce);
        rpc_sender_id = temp_id;
        if (ce.error != Variant::CallError::CALL_OK) {
            String error = Variant::get_call_error_text(p_node, p_method, p_arg, p_argcount, ce);
            error = "rpc() aborted in local call:  - " + error + ".";
            ERR_PRINT(error);
            return;
        }
    }

    if (call_local_script) {
        int temp_id = rpc_sender_id;
        rpc_sender_id = get_network_unique_id();
        Variant::CallError ce;
        ce.error = Variant::CallError::CALL_OK;
        p_node->get_script_instance()->call(p_method, p_arg, p_argcount, ce);
        rpc_sender_id = temp_id;
        if (ce.error != Variant::CallError::CALL_OK) {
            String error = Variant::get_call_error_text(p_node, p_method, p_arg, p_argcount, ce);
            error = "rpc() aborted in script local call:  - " + error + ".";
            ERR_PRINT(error);
            return;
        }
    }

    ERR_FAIL_COND_MSG(skip_rpc && !(call_local_native || call_local_script), "RPC '" + String(p_method) + "' on yourself is not allowed by selected mode.");
}

void MultiplayerAPI::rsetp(Node *p_node, int p_peer_id, bool p_unreliable, const StringName &p_property, const Variant &p_value) {

    ERR_FAIL_COND_MSG(not network_peer, "Trying to RSET while no network peer is active.");
    ERR_FAIL_COND_MSG(!p_node->is_inside_tree(), "Trying to RSET on a node which is not inside SceneTree.");
    ERR_FAIL_COND_MSG(network_peer->get_connection_status() != NetworkedMultiplayerPeer::CONNECTION_CONNECTED, "Trying to send an RSET via a network peer which is not connected.");

    int node_id = network_peer->get_unique_id();
    bool is_master = p_node->is_network_master();
    bool skip_rset = node_id == p_peer_id;
    bool set_local = false;

    if (p_peer_id == 0 || p_peer_id == node_id || (p_peer_id < 0 && p_peer_id != -node_id)) {
        // Check that send mode can use local call.
        const MultiplayerAPI_RPCMode *E = p_node->get_node_rset_mode(p_property);
        if (E) {

            set_local = _should_call_local(*E, is_master, skip_rset);
        }

        if (set_local) {
            bool valid;
            int temp_id = rpc_sender_id;

            rpc_sender_id = get_network_unique_id();
            p_node->set(p_property, p_value, &valid);
            rpc_sender_id = temp_id;

            if (!valid) {
                String error = "rset() aborted in local set, property not found:  - " + String(p_property) + ".";
                ERR_PRINT(error);
                return;
            }
        } else if (p_node->get_script_instance()) {
            // Attempt with script.
            MultiplayerAPI_RPCMode rpc_mode = p_node->get_script_instance()->get_rset_mode(p_property);

            set_local = _should_call_local(rpc_mode, is_master, skip_rset);

            if (set_local) {
                int temp_id = rpc_sender_id;

                rpc_sender_id = get_network_unique_id();
                bool valid = p_node->get_script_instance()->set(p_property, p_value);
                rpc_sender_id = temp_id;

                if (!valid) {
                    String error = "rset() aborted in local script set, property not found:  - " + String(p_property) + ".";
                    ERR_PRINT(error);
                    return;
                }
            }
        }
    }

    if (skip_rset) {
        ERR_FAIL_COND_MSG(!set_local, "RSET for '" + String(p_property) + "' on yourself is not allowed by selected mode.");
        return;
    }
    m_debug_data->record_outgoing_rset(p_node);
    const Variant *vptr = &p_value;

    _send_rpc(p_node, p_peer_id, p_unreliable, true, p_property, &vptr, 1);
}

Error MultiplayerAPI::send_bytes(const PoolVector<uint8_t>& p_data, int p_to, NetworkedMultiplayerPeer::TransferMode p_mode) {

    ERR_FAIL_COND_V_MSG(p_data.size() < 1, ERR_INVALID_DATA, "Trying to send an empty raw packet.");
    ERR_FAIL_COND_V_MSG(not network_peer, ERR_UNCONFIGURED, "Trying to send a raw packet while no network peer is active.");
    ERR_FAIL_COND_V_MSG(network_peer->get_connection_status() != NetworkedMultiplayerPeer::CONNECTION_CONNECTED, ERR_UNCONFIGURED, "Trying to send a raw packet via a network peer which is not connected.");

    MAKE_ROOM(p_data.size() + 1)
    PoolVector<uint8_t>::Read r = p_data.read();
    packet_cache[0] = NETWORK_COMMAND_RAW;
    memcpy(&packet_cache[1], &r[0], p_data.size());

    network_peer->set_target_peer(p_to);
    network_peer->set_transfer_mode(p_mode);

    return network_peer->put_packet(packet_cache.data(), p_data.size() + 1);
}

void MultiplayerAPI::_process_raw(int p_from, const uint8_t *p_packet, int p_packet_len) {

    ERR_FAIL_COND_MSG(p_packet_len < 2, "Invalid packet received. Size too small.");

    PoolVector<uint8_t> out;
    int len = p_packet_len - 1;
    out.resize(len);
    {
        PoolVector<uint8_t>::Write w = out.write();
        memcpy(&w[0], &p_packet[1], len);
    }
    emit_signal("network_peer_packet", p_from, out);
}

int MultiplayerAPI::get_network_unique_id() const {

    ERR_FAIL_COND_V_MSG(not network_peer, 0, "No network peer is assigned. Unable to get unique network ID.");
    return network_peer->get_unique_id();
}

bool MultiplayerAPI::is_network_server() const {

    // XXX Maybe fail silently? Maybe should actually return true to make development of both local and online multiplayer easier?
    ERR_FAIL_COND_V_MSG(not network_peer, false, "No network peer is assigned. I can't be a server.");
    return network_peer->is_server();
}

void MultiplayerAPI::set_refuse_new_network_connections(bool p_refuse) {

    ERR_FAIL_COND_MSG(not network_peer, "No network peer is assigned. Unable to set 'refuse_new_connections'.");
    network_peer->set_refuse_new_connections(p_refuse);
}

bool MultiplayerAPI::is_refusing_new_network_connections() const {

    ERR_FAIL_COND_V_MSG(not network_peer, false, "No network peer is assigned. Unable to get 'refuse_new_connections'.");
    return network_peer->is_refusing_new_connections();
}

Vector<int> MultiplayerAPI::get_network_connected_peers() const {

    ERR_FAIL_COND_V_MSG(not network_peer, Vector<int>(), "No network peer is assigned. Assume no peers are connected.");

    Vector<int> ret;
    ret.reserve(connected_peers.size());
    for (int E : connected_peers) {
        ret.push_back(E);
    }

    return ret;
}

void MultiplayerAPI::set_allow_object_decoding(bool p_enable) {

    allow_object_decoding = p_enable;
}

bool MultiplayerAPI::is_object_decoding_allowed() const {

    return allow_object_decoding;
}

void MultiplayerAPI::profiling_start() {
    m_debug_data->profiling_start();
}

void MultiplayerAPI::profiling_end() {
    m_debug_data->profiling_end();
}

int MultiplayerAPI::get_profiling_frame(ProfilingInfo *r_info) {
    return m_debug_data->profiling_frame(r_info);
}

int MultiplayerAPI::get_incoming_bandwidth_usage() {
    return m_debug_data->_get_bandwidth_usage(DebugData::Incoming);
}

int MultiplayerAPI::get_outgoing_bandwidth_usage() {
    return m_debug_data->_get_bandwidth_usage(DebugData::Outgoing);
}

void MultiplayerAPI::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_root_node", {"node"}), &MultiplayerAPI::set_root_node);
    MethodBinder::bind_method(D_METHOD("send_bytes", {"bytes", "id", "mode"}), &MultiplayerAPI::send_bytes, {DEFVAL(NetworkedMultiplayerPeer::TARGET_PEER_BROADCAST), DEFVAL(NetworkedMultiplayerPeer::TRANSFER_MODE_RELIABLE)});
    MethodBinder::bind_method(D_METHOD("has_network_peer"), &MultiplayerAPI::has_network_peer);
    MethodBinder::bind_method(D_METHOD("get_network_peer"), &MultiplayerAPI::get_network_peer);
    MethodBinder::bind_method(D_METHOD("get_network_unique_id"), &MultiplayerAPI::get_network_unique_id);
    MethodBinder::bind_method(D_METHOD("is_network_server"), &MultiplayerAPI::is_network_server);
    MethodBinder::bind_method(D_METHOD("get_rpc_sender_id"), &MultiplayerAPI::get_rpc_sender_id);
    MethodBinder::bind_method(D_METHOD("_add_peer", {"id"}), &MultiplayerAPI::_add_peer);
    MethodBinder::bind_method(D_METHOD("_del_peer", {"id"}), &MultiplayerAPI::_del_peer);
    MethodBinder::bind_method(D_METHOD("set_network_peer", {"peer"}), &MultiplayerAPI::set_network_peer);
    MethodBinder::bind_method(D_METHOD("poll"), &MultiplayerAPI::poll);
    MethodBinder::bind_method(D_METHOD("clear"), &MultiplayerAPI::clear);

    MethodBinder::bind_method(D_METHOD("_connected_to_server"), &MultiplayerAPI::_connected_to_server);
    MethodBinder::bind_method(D_METHOD("_connection_failed"), &MultiplayerAPI::_connection_failed);
    MethodBinder::bind_method(D_METHOD("_server_disconnected"), &MultiplayerAPI::_server_disconnected);
    MethodBinder::bind_method(D_METHOD("get_network_connected_peers"), &MultiplayerAPI::get_network_connected_peers);
    MethodBinder::bind_method(D_METHOD("set_refuse_new_network_connections", {"refuse"}), &MultiplayerAPI::set_refuse_new_network_connections);
    MethodBinder::bind_method(D_METHOD("is_refusing_new_network_connections"), &MultiplayerAPI::is_refusing_new_network_connections);
    MethodBinder::bind_method(D_METHOD("set_allow_object_decoding", {"enable"}), &MultiplayerAPI::set_allow_object_decoding);
    MethodBinder::bind_method(D_METHOD("is_object_decoding_allowed"), &MultiplayerAPI::is_object_decoding_allowed);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "allow_object_decoding"), "set_allow_object_decoding", "is_object_decoding_allowed");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "refuse_new_network_connections"), "set_refuse_new_network_connections", "is_refusing_new_network_connections");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "network_peer", PropertyHint::ResourceType, "NetworkedMultiplayerPeer", 0), "set_network_peer", "get_network_peer");
    ADD_PROPERTY_DEFAULT("refuse_new_network_connections", false);

    ADD_SIGNAL(MethodInfo("network_peer_connected", PropertyInfo(VariantType::INT, "id")));
    ADD_SIGNAL(MethodInfo("network_peer_disconnected", PropertyInfo(VariantType::INT, "id")));
    ADD_SIGNAL(MethodInfo("network_peer_packet", PropertyInfo(VariantType::INT, "id"), PropertyInfo(VariantType::POOL_BYTE_ARRAY, "packet")));
    ADD_SIGNAL(MethodInfo("connected_to_server"));
    ADD_SIGNAL(MethodInfo("connection_failed"));
    ADD_SIGNAL(MethodInfo("server_disconnected"));

    BIND_ENUM_CONSTANT(RPC_MODE_DISABLED)
    BIND_ENUM_CONSTANT(RPC_MODE_REMOTE)
    BIND_ENUM_CONSTANT(RPC_MODE_MASTER)
    BIND_ENUM_CONSTANT(RPC_MODE_PUPPET)
    BIND_ENUM_CONSTANT(RPC_MODE_SLAVE) // Deprecated.
    BIND_ENUM_CONSTANT(RPC_MODE_REMOTESYNC)
    BIND_ENUM_CONSTANT(RPC_MODE_SYNC) // Deprecated.
    BIND_ENUM_CONSTANT(RPC_MODE_MASTERSYNC)
    BIND_ENUM_CONSTANT(RPC_MODE_PUPPETSYNC)
}

MultiplayerAPI::MultiplayerAPI() {
    rpc_sender_id = 0;
    root_node = nullptr;

    //! @note m_debug_data can be a nullptr in non DEBUG_ENABLED builds, all calls on the pointer will be no-ops
    //! in such case.
#ifdef DEBUG_ENABLED
    m_debug_data = new DebugData;
#endif
    clear();
}

MultiplayerAPI::~MultiplayerAPI() {
    delete m_debug_data;
    clear();
}
