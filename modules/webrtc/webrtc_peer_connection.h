/*************************************************************************/
/*  webrtc_peer_connection.h                                             */
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

#include "core/io/packet_peer.h"
#include "modules/webrtc/webrtc_data_channel.h"

class GODOT_EXPORT WebRTCPeerConnection : public RefCounted {
    GDCLASS(WebRTCPeerConnection,RefCounted)


public:
    enum ConnectionState {
        STATE_NEW,
        STATE_CONNECTING,
        STATE_CONNECTED,
        STATE_DISCONNECTED,
        STATE_FAILED,
        STATE_CLOSED
    };

protected:
    static void _bind_methods();
    static WebRTCPeerConnection *(*_create)();

public:
    virtual ConnectionState get_connection_state() const = 0;

    virtual Error initialize(Dictionary p_config) = 0;
    virtual Ref<WebRTCDataChannel> create_data_channel(StringView p_label, Dictionary p_options) = 0;
    virtual Error create_offer() = 0;
    virtual Error set_remote_description(StringView type, StringView sdp) = 0;
    virtual Error set_local_description(StringView type, StringView sdp) = 0;
    virtual Error add_ice_candidate(StringView sdpMidName, int sdpMlineIndexName, StringView sdpName) = 0;
    virtual Error poll() = 0;
    virtual void close() = 0;

    static Ref<WebRTCPeerConnection> create_ref();
    static WebRTCPeerConnection *create();

    WebRTCPeerConnection();
    ~WebRTCPeerConnection() override;
};
