/*************************************************************************/
/*  scene_string_names.cpp                                               */
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

#include "scene_string_names.h"
#include "core/string.h"
#include "core/string_utils.h"
#include "core/vector.h"

StringName SceneStringNames::_baked_light_changed;
StringName SceneStringNames::_clips_input;
StringName SceneStringNames::_compute_cost;
StringName SceneStringNames::_default;
StringName SceneStringNames::_draw;
StringName SceneStringNames::_enter_tree;
StringName SceneStringNames::_estimate_cost;
StringName SceneStringNames::_exit_tree;
StringName SceneStringNames::_get_minimum_size;
StringName SceneStringNames::_gui_input;
StringName SceneStringNames::_im_update;
StringName SceneStringNames::_input;
StringName SceneStringNames::_input_event;
StringName SceneStringNames::_mouse_enter;
StringName SceneStringNames::_mouse_exit;
StringName SceneStringNames::_physics_process;
StringName SceneStringNames::_pressed;
StringName SceneStringNames::_process;
StringName SceneStringNames::_proxgroup_remove;
StringName SceneStringNames::_queue_update;
StringName SceneStringNames::_ready;
StringName SceneStringNames::_toggled;
StringName SceneStringNames::_unhandled_input;
StringName SceneStringNames::_unhandled_key_input;
StringName SceneStringNames::_update_pairs;
StringName SceneStringNames::_update_remote;
StringName SceneStringNames::_update_scroll;
StringName SceneStringNames::animation_changed;
StringName SceneStringNames::animation_finished;
StringName SceneStringNames::animation_started;
StringName SceneStringNames::area_entered;
StringName SceneStringNames::area_exited;
StringName SceneStringNames::area_shape_entered;
StringName SceneStringNames::area_shape_exited;
StringName SceneStringNames::autoplay;
StringName SceneStringNames::baked_light_changed;
StringName SceneStringNames::blend_times;
StringName SceneStringNames::body_entered;
StringName SceneStringNames::body_exited;
StringName SceneStringNames::body_shape_entered;
StringName SceneStringNames::body_shape_exited;
StringName SceneStringNames::camera_entered;
StringName SceneStringNames::camera_exited;
StringName SceneStringNames::can_drop_data;
StringName SceneStringNames::changed;
StringName SceneStringNames::dot;
StringName SceneStringNames::doubledot;
StringName SceneStringNames::draw;
StringName SceneStringNames::drop_data;
StringName SceneStringNames::finished;
StringName SceneStringNames::loop_finished;
StringName SceneStringNames::step_finished;
StringName SceneStringNames::focus_entered;
StringName SceneStringNames::focus_exited;
StringName SceneStringNames::frame_changed;
StringName SceneStringNames::get_drag_data;
StringName SceneStringNames::grouped;
StringName SceneStringNames::gui_input;
StringName SceneStringNames::h_offset;
StringName SceneStringNames::has_point;
StringName SceneStringNames::hide;
StringName SceneStringNames::input_event;
StringName SceneStringNames::item_rect_changed;
StringName SceneStringNames::line_separation;
StringName SceneStringNames::minimum_size_changed;
StringName SceneStringNames::mouse_entered;
StringName SceneStringNames::mouse_exited;
StringName SceneStringNames::node_configuration_warning_changed;
StringName SceneStringNames::offset;
StringName SceneStringNames::output;
StringName SceneStringNames::parameters_base_path;
StringName SceneStringNames::playback_active;
StringName SceneStringNames::playback_speed;
StringName SceneStringNames::ready;
StringName SceneStringNames::child_entered_tree;
StringName SceneStringNames::child_exiting_tree;
StringName SceneStringNames::resized;
StringName SceneStringNames::rotate;
StringName SceneStringNames::rotation_mode;
StringName SceneStringNames::screen_entered;
StringName SceneStringNames::screen_exited;
StringName SceneStringNames::shader;
StringName SceneStringNames::shader_unshaded;
StringName SceneStringNames::shading_mode;
StringName SceneStringNames::size_flags_changed;
StringName SceneStringNames::sleeping_state_changed;
StringName SceneStringNames::sort_children;
StringName SceneStringNames::speed;
StringName SceneStringNames::tracks_changed;
StringName SceneStringNames::transform_pos;
StringName SceneStringNames::transform_rot;
StringName SceneStringNames::transform_scale;
StringName SceneStringNames::tree_entered;
StringName SceneStringNames::tree_exited;
StringName SceneStringNames::tree_exiting;
StringName SceneStringNames::ungrouped;
StringName SceneStringNames::unit_offset;
StringName SceneStringNames::v_offset;
StringName SceneStringNames::viewport_entered;
StringName SceneStringNames::viewport_exited;
StringName SceneStringNames::visibility_changed;
StringName SceneStringNames::physics_process_internal;
StringName SceneStringNames::physics_process;

NodePath SceneStringNames::path_pp;

void SceneStringNames::free() {
    // unref string names.
    _estimate_cost = {};
    _compute_cost = {};
    line_separation = {};
    tracks_changed = {};
    tree_exited = {};

    resized = {};
    dot = {};
    doubledot = {};
    draw = {};
    hide = {};
    visibility_changed = {};
    input_event = {};
    _input_event = {};
    gui_input = {};
    _gui_input = {};
    item_rect_changed = {};
    shader = {};
    shader_unshaded = {};
    shading_mode = {};
    tree_entered = {};
    tree_exiting = {};
    ready = {};
    child_entered_tree = {};
    child_exiting_tree = {};
    size_flags_changed = {};
    minimum_size_changed = {};
    sleeping_state_changed = {};
    mouse_entered = {};
    mouse_exited = {};
    focus_entered = {};
    focus_exited = {};
    sort_children = {};
    finished = {};
    loop_finished = {};
    step_finished = {};
    animation_finished = {};
    animation_changed = {};
    animation_started = {};
    body_shape_entered = {};
    body_entered = {};
    body_shape_exited = {};
    body_exited = {};
    area_shape_entered = {};
    area_shape_exited = {};
    _physics_process = {};
    _process = {};
    _enter_tree = {};
    _exit_tree = {};
    _draw = {};
    _input = {};
    _ready = {};
    _unhandled_input = {};
    _unhandled_key_input = {};
    _pressed = {};
    _toggled = {};
    _update_scroll = {};
    _clips_input = {};
    _proxgroup_remove = {};
    grouped = {};
    ungrouped = {};
    has_point = {};
    get_drag_data = {};
    can_drop_data = {};
    drop_data = {};
    screen_entered = {};
    screen_exited = {};
    viewport_entered = {};
    viewport_exited = {};
    camera_entered = {};
    camera_exited = {};
    changed = {};
    offset = {};
    unit_offset = {};
    rotation_mode = {};
    rotate = {};
    v_offset = {};
    h_offset = {};
    transform_pos = {};
    transform_rot = {};
    transform_scale = {};
    _update_remote = {};
    _update_pairs = {};
    area_entered = {};
    area_exited = {};
    _get_minimum_size = {};
    _im_update = {};
    _queue_update = {};
    baked_light_changed = {};
    _baked_light_changed = {};
    _mouse_enter = {};
    _mouse_exit = {};
    frame_changed = {};
    playback_speed = {};
    playback_active = {};
    autoplay = {};
    blend_times = {};
    speed = {};
    _default = {};
    node_configuration_warning_changed = {};
    output = {};
    parameters_base_path = {};
    physics_process_internal = {};
    physics_process = {};

    path_pp = {};
}

void SceneStringNames::create() {

    _estimate_cost = StringName("_estimate_cost");
    _compute_cost = StringName("_compute_cost");
    line_separation = StringName("line_separation");

    resized = StringName("resized");
    dot = StringName(".");
    doubledot = StringName("..");
    draw = StringName("draw");
    _draw = StringName("_draw");
    hide = StringName("hide");
    visibility_changed = StringName("visibility_changed");
    input_event = StringName("input_event");
    shader = StringName("shader");
    shader_unshaded = StringName("shader/unshaded");
    shading_mode = StringName("shader/shading_mode");
    tree_entered = StringName("tree_entered");
    tree_exiting = StringName("tree_exiting");
    tree_exited = StringName("tree_exited");
    ready = StringName("ready");
    child_entered_tree = StringName("child_entered_tree");
    child_exiting_tree = StringName("child_exiting_tree");
    item_rect_changed = StringName("item_rect_changed");
    size_flags_changed = StringName("size_flags_changed");
    minimum_size_changed = StringName("minimum_size_changed");
    sleeping_state_changed = StringName("sleeping_state_changed");

    finished = StringName("finished");
    loop_finished = StringName("loop_finished");
    step_finished = StringName("step_finished");

    animation_finished = StringName("animation_finished");
    animation_changed = StringName("animation_changed");
    animation_started = StringName("animation_started");

    mouse_entered = StringName("mouse_entered");
    mouse_exited = StringName("mouse_exited");

    focus_entered = StringName("focus_entered");
    focus_exited = StringName("focus_exited");

    sort_children = StringName("sort_children");

    body_shape_entered = StringName("body_shape_entered");
    body_entered = StringName("body_entered");
    body_shape_exited = StringName("body_shape_exited");
    body_exited = StringName("body_exited");

    area_shape_entered = StringName("area_shape_entered");
    area_shape_exited = StringName("area_shape_exited");

    _physics_process = StringName("_physics_process");
    _process = StringName("_process");

    _enter_tree = StringName("_enter_tree");
    _exit_tree = StringName("_exit_tree");
    _ready = StringName("_ready");

    _clips_input = StringName("_clips_input");

    grouped = StringName("grouped");
    ungrouped = StringName("ungrouped");

    screen_entered = StringName("screen_entered");
    screen_exited = StringName("screen_exited");

    viewport_entered = StringName("viewport_entered");
    viewport_exited = StringName("viewport_exited");

    camera_entered = StringName("camera_entered");
    camera_exited = StringName("camera_exited");

    _input = StringName("_input");
    _input_event = StringName("_input_event");

    gui_input = StringName("gui_input");
    _gui_input = StringName("_gui_input");

    _unhandled_input = StringName("_unhandled_input");
    _unhandled_key_input = StringName("_unhandled_key_input");

    changed = StringName("changed");

    offset = StringName("offset");
    unit_offset = StringName("unit_offset");
    rotation_mode = StringName("rotation_mode");
    rotate = StringName("rotate");
    h_offset = StringName("h_offset");
    v_offset = StringName("v_offset");

    transform_pos = StringName("position");
    transform_rot = StringName("rotation_degrees");
    transform_scale = StringName("scale");

    _update_remote = StringName("_update_remote");
    _update_pairs = StringName("_update_pairs");

    _get_minimum_size = StringName("_get_minimum_size");

    area_entered = StringName("area_entered");
    area_exited = StringName("area_exited");

    has_point = StringName("has_point");

    get_drag_data = StringName("get_drag_data");
    drop_data = StringName("drop_data");
    can_drop_data = StringName("can_drop_data");

    _im_update = StringName("_im_update");
    _queue_update = StringName("_queue_update");

    baked_light_changed = StringName("baked_light_changed");
    _baked_light_changed = StringName("_baked_light_changed");

    _mouse_enter = StringName("_mouse_enter");
    _mouse_exit = StringName("_mouse_exit");

    _pressed = StringName("_pressed");
    _toggled = StringName("_toggled");

    frame_changed = StringName("frame_changed");

    playback_speed = StringName("playback/speed");
    playback_active = StringName("playback/active");
    autoplay = StringName("autoplay");
    blend_times = StringName("blend_times");
    speed = StringName("speed");

    node_configuration_warning_changed = StringName("node_configuration_warning_changed");

    output = StringName("output");
    const StringName dotdot_arr[1] = {doubledot};
    path_pp = NodePath(dotdot_arr,false);

    _default = StringName("default");

    parameters_base_path = "parameters/";

    tracks_changed = "tracks_changed";
    physics_process_internal = "physics_process_internal";
    physics_process = "physics_process";
}
