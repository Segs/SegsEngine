/*************************************************************************/
/*  editor_run.cpp                                                       */
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

#include "editor_run.h"

#include "core/project_settings.h"
#include "core/string_utils.inl"
#include "editor_settings.h"
#include "plugins/script_editor_plugin.h"
#include "script_editor_debugger.h"

#include <QDebug>
#include <QStringRef>
#include <QVector>

EditorRun::Status EditorRun::get_status() const {

    return status;
}

String EditorRun::get_running_scene() const {
    return running_scene;
}

Error EditorRun::run(StringView p_scene, StringView p_custom_args, const Vector<String> &p_breakpoints, const bool &p_skip_breakpoints) {

    Vector<String> args;

    String resource_path(ProjectSettings::get_singleton()->get_resource_path());

    if (!resource_path.empty()) {
        args.push_back("--path");
        args.push_back(StringUtils::replace(resource_path," ", "%20"));
    }

    args.push_back("--remote-debug");
    const String conn_string = ScriptEditor::get_singleton()->get_debugger()->get_connection_string();
    if (!conn_string.empty()) {
        args.push_back(ScriptEditor::get_singleton()->get_debugger()->get_connection_string());
    } else { // Try anyway with default settings
        const String remote_host = EditorSettings::get_singleton()->getT<String>("network/debug/remote_host");
        const int remote_port = (int)EditorSettings::get_singleton()->getT<int>("network/debug/remote_port");
        args.push_back(remote_host + ":" + itos(remote_port));
    }

    args.push_back("--allow_focus_steal_pid");
    args.push_back(::to_string(OS::get_singleton()->get_process_id()));

    if (debug_collisions) {
        args.push_back("--debug-collisions");
    }

    if (debug_navigation) {
        args.push_back("--debug_navigation");
    }

    if (debug_shader_fallbacks) {
        args.push_back("--debug-shader-fallbacks");
    }

    int screen = EditorSettings::get_singleton()->getT<int>("run/window_placement/screen");
    if (screen == 0) {
        // Same as editor
        screen = OS::get_singleton()->get_current_screen();
    } else if (screen == 1) {
        // Previous monitor (wrap to the other end if needed)
        screen = Math::wrapi(
                OS::get_singleton()->get_current_screen() - 1,
                0,
                OS::get_singleton()->get_screen_count());
    } else if (screen == 2) {
        // Next monitor (wrap to the other end if needed)
        screen = Math::wrapi(
                OS::get_singleton()->get_current_screen() + 1,
                0,
                OS::get_singleton()->get_screen_count());
    } else {
        // Fixed monitor ID
        // There are 3 special options, so decrement the option ID by 3 to get the monitor ID
        screen -= 3;
    }

    if (OS::get_singleton()->is_disable_crash_handler()) {
        args.push_back("--disable-crash-handler");
    }

    Rect2 screen_rect;
    screen_rect.position = OS::get_singleton()->get_screen_position(screen);
    screen_rect.size = OS::get_singleton()->get_screen_size(screen);

    Size2 desired_size;
    desired_size.x = ProjectSettings::get_singleton()->getT<float>("display/window/size/width");
    desired_size.y = ProjectSettings::get_singleton()->getT<float>("display/window/size/height");

    Size2 test_size;
    test_size.x = ProjectSettings::get_singleton()->getT<float>("display/window/size/test_width");
    test_size.y = ProjectSettings::get_singleton()->getT<float>("display/window/size/test_height");
    if (test_size.x > 0 && test_size.y > 0) {

        desired_size = test_size;
    }

    int window_placement = EditorSettings::get_singleton()->getT<int>("run/window_placement/rect");
    bool hidpi_proj = ProjectSettings::get_singleton()->getT<bool>("display/window/dpi/allow_hidpi");
    int display_scale = 1;
    if (OS::get_singleton()->is_hidpi_allowed()) {
        if (hidpi_proj) {
            display_scale = 1; // Both editor and project runs in hiDPI mode, do not scale.
        } else {
            display_scale = OS::get_singleton()->get_screen_max_scale(); // Editor is in hiDPI mode, project is not, scale down.
        }
    } else {
        if (hidpi_proj) {
            display_scale = (1.f / OS::get_singleton()->get_screen_max_scale()); // Editor is not in hiDPI mode, project is, scale up.
        } else {
            display_scale = 1; // Both editor and project runs in lowDPI mode, do not scale.
        }
    }
    screen_rect.position /= display_scale;
    screen_rect.size /= display_scale;

    switch (window_placement) {
        case 0: { // top left

            args.push_back("--position");
            args.push_back(::to_string(screen_rect.position.x) + "," + ::to_string(screen_rect.position.y));
        } break;
        case 1: { // centered
            Vector2 pos = screen_rect.position + ((screen_rect.size - desired_size) / 2).floor();
            args.push_back("--position");
            args.push_back(::to_string(pos.x) + "," + ::to_string(pos.y));
        } break;
        case 2: { // custom pos
            Vector2 pos = EditorSettings::get_singleton()->getT<Vector2>("run/window_placement/rect_custom_position");
            pos += screen_rect.position;
            args.push_back("--position");
            args.push_back(::to_string(pos.x) + "," + ::to_string(pos.y));
        } break;
        case 3: { // force maximized
            Vector2 pos = screen_rect.position;
            args.push_back("--position");
            args.push_back(::to_string(pos.x) + "," + ::to_string(pos.y));
            args.push_back("--maximized");

        } break;
        case 4: { // force fullscreen

            Vector2 pos = screen_rect.position;
            args.push_back("--position");
            args.push_back(::to_string(pos.x) + "," + ::to_string(pos.y));
            args.push_back("--fullscreen");
        } break;
    }

    if (!p_breakpoints.empty()) {

        args.push_back("--breakpoints");
        String bpoints = String::joined(p_breakpoints,",").replaced(" ","%20");

        args.emplace_back(eastl::move(bpoints));
    }

    if (p_skip_breakpoints) {
        args.push_back("--skip-breakpoints");
    }

    if (!p_scene.empty()) {
        args.emplace_back(p_scene);
    }


#ifdef RUN_DEBUGEE_THROUGH_VALGRIND
    String exec = "/usr/bin/valgrind";
    args.push_front("--track-origins=yes");
    args.push_front(OS::get_singleton()->get_executable_path());
#else
    String exec = OS::get_singleton()->get_executable_path();
#endif
    if (!p_custom_args.empty()) {
        // Allow the user to specify a command to run, similar to Steam's launch options.
        // In this case, Godot will no longer be run directly; it's up to the underlying command
        // to run it. For instance, this can be used on Linux to force a running project
        // to use Optimus using `prime-run` or similar.
        // Example: `prime-run %command% --time_scale 0.5`
        const int placeholder_pos = p_custom_args.find("%command%");

        Vector<String> custom_args;

        if (placeholder_pos != -1) {
            // Prepend executable-specific custom arguments.
            // If nothing is placed before `%command%`, behave as if no placeholder was specified.
            auto epart = p_custom_args.substr(0,placeholder_pos);
            auto exec_args = StringUtils::split(epart," ", false);
            if (exec_args.size() >= 1) {
                exec = exec_args[0];
                exec_args.pop_front();

                // Append the Godot executable name before we append executable arguments
                // (since the order is reversed when using `push_front()`).
                args.push_front(OS::get_singleton()->get_executable_path());
            }

            for (int i = exec_args.size() - 1; i >= 0; i--) {
                // Iterate backwards as we're pushing items in the reverse order.
                args.push_front(String(exec_args[i]).replaced(" ", "%20"));
            }

            // Append Godot-specific custom arguments.
            auto part = p_custom_args.substr(placeholder_pos + String("%command%").size());
            auto cargs = StringUtils::split(part," ", false);
            for (int i = 0; i < cargs.size(); i++) {
                args.push_back(String(cargs[i]).replaced(" ", "%20"));
            }
        } else {
            // Append Godot-specific custom arguments.
            auto cargs = StringUtils::split(p_custom_args," ", false);
            for (int i = 0; i < cargs.size(); i++) {
                args.push_back(String(cargs[i]).replaced(" ", "%20"));
            }
        }
    }
    {
        QDebug msg_log(qDebug());
        msg_log<<"Running: "<<exec.c_str();
        for (const String &E : args) {

            msg_log<<" "<<E.data();
        }
    }

    pid = 0;
    Error err = OS::get_singleton()->execute(exec, args, false, &pid);
    ERR_FAIL_COND_V(err, err);

    status = STATUS_PLAY;
    if (!p_scene.empty()) {
        running_scene = p_scene;
    }
    return OK;
}

void EditorRun::stop() {

    if (status != STATUS_STOP && pid != 0) {

        OS::get_singleton()->kill(pid);
    }

    status = STATUS_STOP;
    running_scene.clear();
}

void EditorRun::set_debug_collisions(bool p_debug) {

    debug_collisions = p_debug;
}

bool EditorRun::get_debug_collisions() const {

    return debug_collisions;
}

void EditorRun::set_debug_navigation(bool p_debug) {

    debug_navigation = p_debug;
}

bool EditorRun::get_debug_navigation() const {

    return debug_navigation;
}

void EditorRun::set_debug_shader_fallbacks(bool p_debug) {

    debug_shader_fallbacks = p_debug;
}

bool EditorRun::get_debug_shader_fallbacks() const {

    return debug_shader_fallbacks;
}

EditorRun::EditorRun() {
}
