/*************************************************************************/
/*  script_language.h                                                    */
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


#include "core/map.h"
#include "core/hash_set.h"
#include "core/hash_map.h"
#include "core/pair.h"
#include "core/resource.h"
#include "core/variant.h"
#include "core/script_language.h"
#include "core/string_name.h"
#include "core/property_info.h"

class ScriptLanguage;
class MultiplayerAPI;
enum MultiplayerAPI_RPCMode : int8_t;

using ScriptEditRequestFunction = void (*)(StringView);

class GODOT_EXPORT ScriptDebugger {

    using StackInfo = ScriptLanguage::StackInfo ;
    int lines_left;
    int depth;

    static ScriptDebugger *singleton;
    Map<int, HashSet<StringName> > breakpoints;

    ScriptLanguage *break_lang;

public:

    _FORCE_INLINE_ static ScriptDebugger *get_singleton() { return singleton; }
    void set_lines_left(int p_left);
    int get_lines_left() const;

    void set_depth(int p_depth);
    int get_depth() const;

    String breakpoint_find_source(StringView p_source) const;
    void insert_breakpoint(int p_line, const StringName &p_source);
    void remove_breakpoint(int p_line, const StringName &p_source);
    bool is_breakpoint(int p_line, const StringName &p_source) const;
    bool is_breakpoint_line(int p_line) const;
    void clear_breakpoints();
    const Map<int, HashSet<StringName> > &get_breakpoints() const { return breakpoints; }

    virtual void debug(ScriptLanguage *p_script, bool p_can_continue = true, bool p_is_error_breakpoint = false) = 0;
    virtual void idle_poll();
    virtual void line_poll();

    void set_break_language(ScriptLanguage *p_lang);
    ScriptLanguage *get_break_language() const;

    virtual void send_message(const String &p_message, const Array &p_args) = 0;
    virtual void send_error(StringView p_func, StringView p_file, int p_line, StringView p_err, StringView p_descr, ErrorHandlerType p_type, const Vector<StackInfo> &p_stack_info) = 0;

    virtual bool is_remote() const { return false; }
    virtual void request_quit() {}

    virtual void set_multiplayer(const Ref<MultiplayerAPI> & /*p_multiplayer*/) {}

    virtual bool is_profiling() const = 0;
    virtual void add_profiling_frame_data(const StringName &p_name, const Array &p_data) = 0;
    virtual void profiling_start() = 0;
    virtual void profiling_end() = 0;
    virtual void profiling_set_frame_times(float p_frame_time, float p_idle_time, float p_physics_time, float p_physics_frame_time) = 0;

    ScriptDebugger();
    virtual ~ScriptDebugger() { singleton = nullptr; }
};
