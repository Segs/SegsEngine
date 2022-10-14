/*************************************************************************/
/*  editor_vcs_interface.cpp                                             */
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

#include "editor_vcs_interface.h"
#include "core/dictionary.h"
#include "core/method_bind.h"
#include "core/class_db.h"

EditorVCSInterface *EditorVCSInterface::singleton = nullptr;

IMPL_GDCLASS(EditorVCSInterface)

void EditorVCSInterface::_bind_methods() {

    // Proxy end points that act as fallbacks to unavailability of a function in the VCS addon
    BIND_METHOD(EditorVCSInterface,_initialize);
    BIND_METHOD(EditorVCSInterface,_is_vcs_initialized);
    BIND_METHOD(EditorVCSInterface,_get_vcs_name);
    BIND_METHOD(EditorVCSInterface,_shut_down);
    BIND_METHOD(EditorVCSInterface,_get_project_name);
    BIND_METHOD(EditorVCSInterface,_get_modified_files_data);
    BIND_METHOD(EditorVCSInterface,_commit);
    BIND_METHOD(EditorVCSInterface,_get_file_diff);
    BIND_METHOD(EditorVCSInterface,_stage_file);
    BIND_METHOD(EditorVCSInterface,_unstage_file);

    BIND_METHOD(EditorVCSInterface,is_addon_ready);

    // API methods that redirect calls to the proxy end points
    BIND_METHOD(EditorVCSInterface,initialize);
    BIND_METHOD(EditorVCSInterface,is_vcs_initialized);
    BIND_METHOD(EditorVCSInterface,get_modified_files_data);
    BIND_METHOD(EditorVCSInterface,stage_file);
    BIND_METHOD(EditorVCSInterface,unstage_file);
    BIND_METHOD(EditorVCSInterface,commit);
    BIND_METHOD(EditorVCSInterface,get_file_diff);
    BIND_METHOD(EditorVCSInterface,shut_down);
    BIND_METHOD(EditorVCSInterface,get_project_name);
    BIND_METHOD(EditorVCSInterface,get_vcs_name);
}

bool EditorVCSInterface::_initialize(StringView p_project_root_path) {

    WARN_PRINT("Selected VCS addon does not implement an initialization function. This warning will be suppressed.");
    return true;
}

bool EditorVCSInterface::_is_vcs_initialized() {

    return false;
}

Dictionary EditorVCSInterface::_get_modified_files_data() {

    return Dictionary();
}

void EditorVCSInterface::_stage_file(StringView p_file_path) {
}

void EditorVCSInterface::_unstage_file(StringView p_file_path) {
}

void EditorVCSInterface::_commit(StringView p_msg) {
}

Array EditorVCSInterface::_get_file_diff(StringView p_file_path) {

    return Array();
}

bool EditorVCSInterface::_shut_down() {

    return false;
}

String EditorVCSInterface::_get_project_name() {

    return String();
}

String EditorVCSInterface::_get_vcs_name() {

    return String();
}

bool EditorVCSInterface::initialize(StringView p_project_root_path) {

    is_initialized = call_va("_initialize", p_project_root_path).as<bool>();
    return is_initialized;
}

bool EditorVCSInterface::is_vcs_initialized() {

    return call_va("_is_vcs_initialized").as<bool>();
}

Dictionary EditorVCSInterface::get_modified_files_data() {

    return call_va("_get_modified_files_data").as<Dictionary>();
}

void EditorVCSInterface::stage_file(StringView p_file_path) {

    if (is_addon_ready()) {

        call_va("_stage_file", p_file_path);
    }
}

void EditorVCSInterface::unstage_file(StringView p_file_path) {

    if (is_addon_ready()) {

        call_va("_unstage_file", p_file_path);
    }
}

bool EditorVCSInterface::is_addon_ready() {

    return is_initialized;
}

void EditorVCSInterface::commit(StringView p_msg) {

    if (is_addon_ready()) {

        call_va("_commit", p_msg);
    }
}

Array EditorVCSInterface::get_file_diff(StringView p_file_path) {

    if (is_addon_ready()) {

        return call_va("_get_file_diff", p_file_path).as<Array>();
    }
    return Array();
}

bool EditorVCSInterface::shut_down() {

    return call_va("_shut_down").as<bool>();
}

String EditorVCSInterface::get_project_name() {

    return call_va("_get_project_name").as<String>();
}

String EditorVCSInterface::get_vcs_name() {

    return call_va("_get_vcs_name").as<String>();
}

EditorVCSInterface::EditorVCSInterface() {

    is_initialized = false;
}

EditorVCSInterface::~EditorVCSInterface() {
}

EditorVCSInterface *EditorVCSInterface::get_singleton() {

    return singleton;
}

void EditorVCSInterface::set_singleton(EditorVCSInterface *p_singleton) {

    singleton = p_singleton;
}
