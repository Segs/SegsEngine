/*************************************************************************/
/*  script_debugger_local.cpp                                            */
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

#include "script_debugger_local.h"

#include "core/os/os.h"
#include "core/print_string.h"
#include "core/string_formatter.h"
#include "core/string_utils.h"
//TODO: SEGS consider removing 'scene/main/scene_tree.h' include from core module, or moving script_debugger_local to it's own module depending on core and scene modules.
#include "scene/main/scene_tree.h"

void ScriptDebuggerLocal::debug(ScriptLanguage *p_script, bool p_can_continue, bool p_is_error_breakpoint) {

    if (!target_function.empty()) {
        String current_function = p_script->debug_get_stack_level_function(0);
        if (current_function != target_function) {
            set_depth(0);
            set_lines_left(1);
            return;
        }
        target_function = "";
    }

    print_line("\nDebugger Break, Reason: '" + p_script->debug_get_error() + "'");
    print_line("*Frame 0 - " + p_script->debug_get_stack_level_source(0) + ":" + ::to_string(p_script->debug_get_stack_level_line(0)) + " in function '" + p_script->debug_get_stack_level_function(0) + "'");
    print_line("Enter \"help\" for assistance.");
    int current_frame = 0;
    int total_frames = p_script->debug_get_stack_level_count();
    while (true) {

        OS::get_singleton()->print("debug> ");
        String line(StringUtils::strip_edges(OS::get_singleton()->get_stdin_string()));

        // Cache options
        String variable_prefix = options["variable_prefix"];

        if (line.empty()) {
            print_line("\nDebugger Break, Reason: '" + p_script->debug_get_error() + "'");
            print_line("*Frame " + ::to_string(current_frame) + " - " + p_script->debug_get_stack_level_source(current_frame) + ":" + ::to_string(p_script->debug_get_stack_level_line(current_frame)) + " in function '" + p_script->debug_get_stack_level_function(current_frame) + "'");
            print_line("Enter \"help\" for assistance.");
        } else if (line == "c" || line == "continue")
            break;
        else if (line == "bt" || line == "breakpoint") {

            for (int i = 0; i < total_frames; i++) {

                String cfi((current_frame == i) ? "*" : " "); //current frame indicator
                print_line(cfi + "Frame " + ::to_string(i) + " - " + p_script->debug_get_stack_level_source(i) + ":" + ::to_string(p_script->debug_get_stack_level_line(i)) + " in function '" + p_script->debug_get_stack_level_function(i) + "'");
            }

        } else if (StringUtils::begins_with(line,"fr") || StringUtils::begins_with(line,"frame")) {

            if (StringUtils::get_slice_count(line,' ') == 1) {
                print_line("*Frame " + ::to_string(current_frame) + " - " + p_script->debug_get_stack_level_source(current_frame) + ":" + ::to_string(p_script->debug_get_stack_level_line(current_frame)) + " in function '" + p_script->debug_get_stack_level_function(current_frame) + "'");
            } else {
                int frame = StringUtils::to_int(StringUtils::get_slice(line,' ', 1));
                if (frame < 0 || frame >= total_frames) {
                    print_line("Error: Invalid frame.");
                } else {
                    current_frame = frame;
                    print_line("*Frame " + ::to_string(frame) + " - " + p_script->debug_get_stack_level_source(frame) + ":" + ::to_string(p_script->debug_get_stack_level_line(frame)) + " in function '" + p_script->debug_get_stack_level_function(frame) + "'");
                }
            }

        } else if (StringUtils::begins_with(line,"set")) {

            if (StringUtils::get_slice_count(line,' ') == 1) {

                for (const eastl::pair<const String,String> &E : options) {
                    print_line("\t" + E.first + "=" + E.second);
                }

            } else {
                StringView key_value = StringUtils::get_slice(line,' ', 1);
                int value_pos = StringUtils::find(key_value,"=");

                if (value_pos < 0) {
                    print_line("Error: Invalid set format. Use: set key=value");
                } else {

                    StringView key = StringUtils::left(key_value,value_pos);

                    if (!options.contains_as(key)) {
                        print_line("Error: Unknown option " + String(key));
                    } else {

                        // Allow explicit tab character
                        String value =  StringUtils::replace(StringUtils::right(key_value,value_pos + 1),"\\t", "\t");

                        options[String(key)] = value;
                    }
                }
            }

        } else if (line == "lv" || line == "locals") {

            Vector<String> locals;
            Vector<Variant> values;
            p_script->debug_get_stack_level_locals(current_frame, &locals, &values);
            print_variables(locals, values, variable_prefix);

        } else if (line == "gv" || line == "globals") {

            Vector<String> globals;
            Vector<Variant> values;
            p_script->debug_get_globals(&globals, &values);
            print_variables(globals, values, variable_prefix);

        } else if (line == "mv" || line == "members") {

            Vector<String> members;
            Vector<Variant> values;
            p_script->debug_get_stack_level_members(current_frame, &members, &values);
            print_variables(members, values, variable_prefix);

        } else if (StringUtils::begins_with(line,"p") || StringUtils::begins_with(line,"print")) {

            if (StringUtils::get_slice_count(line,' ') <= 1) {
                print_line("Usage: print <expre>");
            } else {

                StringView expr = StringUtils::get_slice(line,' ', 2);
                String res = p_script->debug_parse_stack_level_expression(current_frame, expr);
                print_line(res);
            }

        } else if (line == "s" || line == "step") {

            set_depth(-1);
            set_lines_left(1);
            break;
        } else if (line == "n" || line == "next") {

            set_depth(0);
            set_lines_left(1);
            break;
        } else if (line == "fin" || line == "finish") {

            String current_function = p_script->debug_get_stack_level_function(0);

            for (int i = 0; i < total_frames; i++) {
                target_function = p_script->debug_get_stack_level_function(i);
                if (target_function != current_function) {
                    set_depth(0);
                    set_lines_left(1);
                    return;
                }
            }

            print_line("Error: Reached last frame.");
            target_function = "";

        } else if (StringUtils::begins_with(line,"br") || StringUtils::begins_with(line,"break")) {

            if (StringUtils::get_slice_count(line,' ') <= 1) {

                const Map<int, HashSet<StringName> > &breakpoints = get_breakpoints();
                if (breakpoints.empty()) {
                    print_line("No Breakpoints.");
                    continue;
                }

                print_line("Breakpoint(s): " + itos(breakpoints.size()));
                for (const eastl::pair<const int,HashSet<StringName> > &E : breakpoints) {
                    print_line("\t" + String(*E.second.begin()) + ":" + itos(E.first));
                }

            } else {

                Pair<String, int> breakpoint = to_breakpoint(line);

                String source = breakpoint.first;
                int linenr = breakpoint.second;

                if (source.empty())
                    continue;

                insert_breakpoint(linenr, StringName(source));

                print_line("Added breakpoint at " + source + ":" + itos(linenr));
            }

        } else if (line == "q" || line == "quit") {

            // Do not stop again on quit
            clear_breakpoints();
            ScriptDebugger::get_singleton()->set_depth(-1);
            ScriptDebugger::get_singleton()->set_lines_left(-1);

            SceneTree::get_singleton()->quit();
            break;
        } else if (StringUtils::begins_with(line,"delete")) {

            if (StringUtils::get_slice_count(line,' ') <= 1) {
                clear_breakpoints();
            } else {

                Pair<String, int> breakpoint = to_breakpoint(line);

                String source = breakpoint.first;
                int linenr = breakpoint.second;

                if (source.empty())
                    continue;

                remove_breakpoint(linenr, StringName(source));

                print_line("Removed breakpoint at " + source + ":" + itos(linenr));
            }

        } else if (line == "h" || line == "help") {

            print_line("Built-In Debugger command list:\n");
            print_line("\tc,continue\t\t Continue execution.");
            print_line("\tbt,backtrace\t\t Show stack trace (frames).");
            print_line("\tfr,frame <frame>:\t Change current frame.");
            print_line("\tlv,locals\t\t Show local variables for current frame.");
            print_line("\tmv,members\t\t Show member variables for \"this\" in frame.");
            print_line("\tgv,globals\t\t Show global variables.");
            print_line("\tp,print <expr>\t\t Execute and print variable in expression.");
            print_line("\ts,step\t\t\t Step to next line.");
            print_line("\tn,next\t\t\t Next line.");
            print_line("\tfin,finish\t\t Step out of current frame.");
            print_line("\tbr,break [source:line]\t List all breakpoints or place a breakpoint.");
            print_line("\tdelete [source:line]:\t Delete one/all breakpoints.");
            print_line("\tset [key=value]:\t List all options, or set one.");
            print_line("\tq,quit\t\t\t Quit application.");
        } else {
            print_line("Error: Invalid command, enter \"help\" for assistance.");
        }
    }
}

void ScriptDebuggerLocal::print_variables(const Vector<String> &names, const Vector<Variant> &values, StringView variable_prefix) {

    //ERR_FAIL_COND(names.size()!=values.size());

    Vector<StringView> value_lines;

    for(size_t idx=0,fin=values.size(); idx<fin; ++idx) {
        const String &E(names[idx]);
        const String value(values[idx].as<String>());
        if (variable_prefix.empty()) {
            print_line(E + ": " + value);
        } else {
            print_line(E + ":");
            value_lines = StringUtils::split(value,'\n');
            for (size_t i = 0; i < value_lines.size(); ++i) {
                print_line(String(variable_prefix) + value_lines[i]);
            }
        }

    }
}

Pair<String, int> ScriptDebuggerLocal::to_breakpoint(const String &p_line) {

    StringView breakpoint_part = StringUtils::get_slice(p_line,' ', 1);
    Pair<String, int> breakpoint;

    auto last_colon = StringUtils::rfind(breakpoint_part,":");
    if (last_colon == String::npos) {
        print_line("Error: Invalid breakpoint format. Expected [source:line]");
        return breakpoint;
    }

    breakpoint.first = breakpoint_find_source(StringUtils::strip_edges(StringUtils::left(breakpoint_part,last_colon)));
    breakpoint.second = StringUtils::to_int(StringUtils::strip_edges(StringUtils::right(breakpoint_part,last_colon)));

    return breakpoint;
}

struct _ScriptDebuggerLocalProfileInfoSort {

    bool operator()(const ScriptLanguage::ProfilingInfo &A, const ScriptLanguage::ProfilingInfo &B) const {
        return A.total_time > B.total_time;
    }
};

void ScriptDebuggerLocal::profiling_set_frame_times(float p_frame_time, float p_process_time, float p_physics_time, float p_physics_frame_time) {

    frame_time = p_frame_time;
    process_time = p_process_time;
    physics_time = p_physics_time;
    physics_frame_time = p_physics_frame_time;
}

void ScriptDebuggerLocal::idle_poll() {

    if (!profiling)
        return;

    uint64_t diff = OS::get_singleton()->get_ticks_usec() - idle_accum;

    if (diff < 1000000) //show every one second
        return;

    idle_accum = OS::get_singleton()->get_ticks_usec();

    int ofs = 0;
    for (int i = 0; i < ScriptServer::get_language_count(); i++) {
        ofs += ScriptServer::get_language(i)->profiling_get_frame_data(&pinfo[ofs], pinfo.size() - ofs);
    }

    SortArray<ScriptLanguage::ProfilingInfo, _ScriptDebuggerLocalProfileInfoSort> sort;
    sort.sort(pinfo.data(), ofs);

    //falta el frame time

    uint64_t script_time_us = 0;

    for (int i = 0; i < ofs; i++) {

        script_time_us += pinfo[i].self_time;
    }

    float script_time = USEC_TO_SEC(script_time_us);

    float total_time = frame_time;

    //print script total

    print_line("FRAME: total: " + rtos(frame_time) + " script: " + rtos(script_time) + "/" + itos(script_time * 100 / total_time) + " %");

    for (int i = 0; i < ofs; i++) {

        print_line(FormatVE("%d:%s",i,pinfo[i].signature.asCString()));
        float tt = USEC_TO_SEC(pinfo[i].total_time);
        float st = USEC_TO_SEC(pinfo[i].self_time);
        print_line("\ttotal: " + rtos(tt) + "/" + itos(tt * 100 / total_time) + " % \tself: " + rtos(st) + "/" + itos(st * 100 / total_time) + " % tcalls: " + itos(pinfo[i].call_count));
    }
}

void ScriptDebuggerLocal::profiling_start() {

    for (int i = 0; i < ScriptServer::get_language_count(); i++) {
        ScriptServer::get_language(i)->profiling_start();
    }

    print_line("BEGIN PROFILING");
    profiling = true;
    pinfo.resize(32768);
    frame_time = 0;
    physics_time = 0;
    process_time = 0;
    physics_frame_time = 0;
}

void ScriptDebuggerLocal::profiling_end() {

    int ofs = 0;

    for (int i = 0; i < ScriptServer::get_language_count(); i++) {
        ofs += ScriptServer::get_language(i)->profiling_get_accumulated_data(&pinfo[ofs], pinfo.size() - ofs);
    }

    SortArray<ScriptLanguage::ProfilingInfo, _ScriptDebuggerLocalProfileInfoSort> sort;
    sort.sort(pinfo.data(), ofs);

    uint64_t total_us = 0;
    for (int i = 0; i < ofs; i++) {
        total_us += pinfo[i].self_time;
    }

    float total_time = total_us / 1000000.0f;

    for (int i = 0; i < ofs; i++) {
        print_line(FormatVE("%d:%s",i,pinfo[i].signature.asCString()));
        float tt = USEC_TO_SEC(pinfo[i].total_time);
        float st = USEC_TO_SEC(pinfo[i].self_time);
        print_line("\ttotal_ms: " + rtos(tt) + "\tself_ms: " + rtos(st) + "total%: " + itos(tt * 100 / total_time) + "\tself%: " + itos(st * 100 / total_time) + "\tcalls: " + itos(pinfo[i].call_count));
    }

    for (int i = 0; i < ScriptServer::get_language_count(); i++) {
        ScriptServer::get_language(i)->profiling_stop();
    }

    profiling = false;
}

void ScriptDebuggerLocal::send_message(const String &p_message, const Array &p_args) {

    // This needs to be cleaned up entirely.
    // print_line("MESSAGE: '" + p_message + "' - " + String(Variant(p_args)));
}

void ScriptDebuggerLocal::send_error(StringView p_func, StringView p_file, int p_line, StringView p_err, StringView p_descr, ErrorHandlerType p_type, const Vector<ScriptLanguage::StackInfo> &p_stack_info) {

    print_line(String("ERROR: '") + (p_descr.empty() ? p_err : p_descr) + "'");
}

ScriptDebuggerLocal::ScriptDebuggerLocal() {

    profiling = false;
    idle_accum = OS::get_singleton()->get_ticks_usec();
    options["variable_prefix"] = "";
}
