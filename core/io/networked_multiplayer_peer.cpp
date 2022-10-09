/*************************************************************************/
/*  networked_multiplayer_peer.cpp                                       */
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

#include "networked_multiplayer_peer.h"
#include "core/method_bind.h"
#include "core/io/networked_multiplayer_peer_enum_casters.h"

IMPL_GDCLASS(NetworkedMultiplayerPeer)

void NetworkedMultiplayerPeer::_bind_methods() {

    BIND_METHOD(NetworkedMultiplayerPeer,set_transfer_mode);
    BIND_METHOD(NetworkedMultiplayerPeer,get_transfer_mode);
    BIND_METHOD(NetworkedMultiplayerPeer,set_target_peer);

    BIND_METHOD(NetworkedMultiplayerPeer,get_packet_peer);

    BIND_METHOD(NetworkedMultiplayerPeer,poll);

    BIND_METHOD(NetworkedMultiplayerPeer,get_connection_status);
    BIND_METHOD(NetworkedMultiplayerPeer,get_unique_id);

    BIND_METHOD(NetworkedMultiplayerPeer,set_refuse_new_connections);
    BIND_METHOD(NetworkedMultiplayerPeer,is_refusing_new_connections);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "refuse_new_connections"), "set_refuse_new_connections", "is_refusing_new_connections");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "transfer_mode", PropertyHint::Enum, "Unreliable,Unreliable Ordered,Reliable"), "set_transfer_mode", "get_transfer_mode");

    BIND_ENUM_CONSTANT(TRANSFER_MODE_UNRELIABLE);
    BIND_ENUM_CONSTANT(TRANSFER_MODE_UNRELIABLE_ORDERED);
    BIND_ENUM_CONSTANT(TRANSFER_MODE_RELIABLE);

    BIND_ENUM_CONSTANT(CONNECTION_DISCONNECTED);
    BIND_ENUM_CONSTANT(CONNECTION_CONNECTING);
    BIND_ENUM_CONSTANT(CONNECTION_CONNECTED);

    BIND_CONSTANT(TARGET_PEER_BROADCAST)
    BIND_CONSTANT(TARGET_PEER_SERVER)

    ADD_SIGNAL(MethodInfo("peer_connected", PropertyInfo(VariantType::INT, "id")));
    ADD_SIGNAL(MethodInfo("peer_disconnected", PropertyInfo(VariantType::INT, "id")));
    ADD_SIGNAL(MethodInfo("server_disconnected"));
    ADD_SIGNAL(MethodInfo("connection_succeeded"));
    ADD_SIGNAL(MethodInfo("connection_failed"));
}


