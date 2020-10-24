/*************************************************************************/
/*  script_language.cpp                                                  */
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

#include "script_debugger.h"

#include "core/core_string_names.h"
#include "core/project_settings.h"
#include "core/object_tooling.h"
#include "core/method_bind.h"

#include "EASTL/sort.h"


ScriptDebugger *ScriptDebugger::singleton = nullptr;

void ScriptDebugger::set_lines_left(int p_left) {

    lines_left = p_left;
}

int ScriptDebugger::get_lines_left() const {

    return lines_left;
}

void ScriptDebugger::set_depth(int p_depth) {

    depth = p_depth;
}

int ScriptDebugger::get_depth() const {

    return depth;
}

void ScriptDebugger::insert_breakpoint(int p_line, const StringName &p_source) {

    if (!breakpoints.contains(p_line)) {
        breakpoints[p_line] = HashSet<StringName>();
    }
    breakpoints[p_line].insert(p_source);
}

void ScriptDebugger::remove_breakpoint(int p_line, const StringName &p_source) {

    if (!breakpoints.contains(p_line)) {
        return;
    }

    breakpoints[p_line].erase(p_source);
    if (breakpoints[p_line].empty()) {
        breakpoints.erase(p_line);
    }
}
bool ScriptDebugger::is_breakpoint(int p_line, const StringName &p_source) const {

    if (!breakpoints.contains(p_line)) {
        return false;
    }
    return breakpoints.at(p_line).contains(p_source);
}
bool ScriptDebugger::is_breakpoint_line(int p_line) const {

    return breakpoints.contains(p_line);
}

String ScriptDebugger::breakpoint_find_source(StringView p_source) const {

    return String(p_source);
}

void ScriptDebugger::clear_breakpoints() {

    breakpoints.clear();
}

void ScriptDebugger::idle_poll() {
}

void ScriptDebugger::line_poll() {
}

void ScriptDebugger::set_break_language(ScriptLanguage *p_lang) {

    break_lang = p_lang;
}

ScriptLanguage *ScriptDebugger::get_break_language() const {

    return break_lang;
}

ScriptDebugger::ScriptDebugger() {

    singleton = this;
    lines_left = -1;
    depth = -1;
    break_lang = nullptr;
}
