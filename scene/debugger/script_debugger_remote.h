/*************************************************************************/
/*  script_debugger_remote.h                                             */
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

#include "core/debugger/script_debugger.h"
#include "core/io/multiplayer_api.h"
#include "core/io/packet_peer.h"
#include "core/io/stream_peer_tcp.h"
#include "core/list.h"
#include "core/os/os.h"
#include "core/os/mutex.h"
#include "core/print_string.h"
#include "core/rid.h"
#include "core/script_language.h"

class SceneTree;

class GODOT_EXPORT ScriptDebuggerRemote : public ScriptDebugger {

    struct Message {

        String message;
        Array data;
    };

    struct ProfileInfoSort {

        bool operator()(ScriptLanguage::ProfilingInfo *A, ScriptLanguage::ProfilingInfo *B) const {
            return A->total_time < B->total_time;
        }
    };

    Vector<ScriptLanguage::ProfilingInfo> profile_info;
    Vector<ScriptLanguage::ProfilingInfo *> profile_info_ptrs;
    Vector<MultiplayerAPI::ProfilingInfo> network_profile_info;

    Map<StringName, int> profiler_function_signature_map;
    float frame_time, process_time, physics_time, physics_frame_time;

    bool profiling;
    bool profiling_network;
    int max_frame_functions;
    bool skip_profile_frame;
    bool reload_all_scripts;

    Ref<StreamPeerTCP> tcp_client;
    Ref<PacketPeerStream> packet_peer_stream;

    uint64_t last_perf_time;
    uint64_t last_net_prof_time;
    uint64_t last_net_bandwidth_time;
    Object *performance;
    bool requested_quit;
    Mutex mutex;

    struct OutputError {

        int hr;
        int min;
        int sec;
        int msec;
        String source_file;
        String source_func;
        int source_line;
        String error;
        String error_descr;
        bool warning;
        Array callstack;
    };
    struct OutputString {
        String message;
        int type;
    };

    List<OutputString> output_strings;
    List<Message> messages;
    int max_messages_per_frame;
    int n_messages_dropped;
    List<OutputError> errors;
    int max_errors_per_second;
    int max_warnings_per_second;
    int n_errors_dropped;
    int n_warnings_dropped=0;

    int max_cps;
    int char_count;
    int err_count;
    int warn_count;
    uint64_t last_msec;
    uint64_t msec_count;

    OS::ProcessID allow_focus_steal_pid;

    bool locking; //hack to avoid a deadloop
    static void _print_handler(void *p_this, const String &p_string, bool p_error);

    PrintHandlerList phl;

    void _get_output();
    void _poll_events();
    uint32_t poll_every;

    SceneTree *scene_tree;

    bool _parse_live_edit(const Array &p_command);

    void _set_object_property(GameEntity p_id, const String &p_property, const Variant &p_value);

    void _send_object_id(GameEntity p_id);
    void _send_video_memory();

    Ref<MultiplayerAPI> multiplayer;

    ErrorHandlerList eh;
    static void _err_handler(void *, StringView, StringView, int p_line, StringView, StringView, ErrorHandlerType p_type);

    void _send_profiling_data(bool p_for_frame);
    void _send_network_profiling_data();
    void _send_network_bandwidth_usage();

    struct FrameData {

        StringName name;
        Array data;
    };

    Vector<FrameData> profile_frame_data;

    void _put_variable(StringView p_name, const Variant &p_variable);

    void _save_node(GameEntity id, StringView p_path);

    bool skip_breakpoints;
public:
    enum MessageType {
        MESSAGE_TYPE_LOG,
        MESSAGE_TYPE_ERROR,
    };

    struct ResourceUsage {

        String path;
        String format;
        String type;
        RenderingEntity id=entt::null;
        int vram;
        bool operator<(const ResourceUsage &p_img) const { return vram == p_img.vram ? entt::to_integral(id) < entt::to_integral(p_img.id) : vram > p_img.vram; }
    };

    using ResourceUsageFunc = void (*)(List<ResourceUsage> *);

    static ResourceUsageFunc resource_usage_func;

    Error connect_to_host(StringView p_host, uint16_t p_port);
    void debug(ScriptLanguage *p_script, bool p_can_continue = true, bool p_is_error_breakpoint = false) override;
    void idle_poll() override;
    void line_poll() override;

    bool is_remote() const override { return true; }
    void request_quit() override;

    void send_message(const String &p_message, const Array &p_args) override;
    void send_error(StringView p_func, StringView p_file, int p_line, StringView p_err, StringView p_descr, ErrorHandlerType p_type, const Vector<ScriptLanguage::StackInfo> &p_stack_info) override;

    void set_multiplayer(const Ref<MultiplayerAPI> &p_multiplayer) override;

    bool is_profiling() const override;
    void add_profiling_frame_data(const StringName &p_name, const Array &p_data) override;

    void profiling_start() override;
    void profiling_end() override;
    void profiling_set_frame_times(float p_frame_time, float p_process_time, float p_physics_time, float p_physics_frame_time) override;

    void set_skip_breakpoints(bool p_skip_breakpoints);

    void set_scene_tree(SceneTree *p_scene_tree) { scene_tree = p_scene_tree; }
    void set_allow_focus_steal_pid(OS::ProcessID p_pid);

    ScriptDebuggerRemote();
    ~ScriptDebuggerRemote() override;
};
