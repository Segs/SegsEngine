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

SceneStringNames *SceneStringNames::singleton = nullptr;

SceneStringNames::SceneStringNames() {

    _estimate_cost = StringName("_estimate_cost");
    _compute_cost = StringName("_compute_cost");

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
    item_rect_changed = StringName("item_rect_changed");
    size_flags_changed = StringName("size_flags_changed");
    minimum_size_changed = StringName("minimum_size_changed");
    sleeping_state_changed = StringName("sleeping_state_changed");

    finished = StringName("finished");
    emission_finished = StringName("emission_finished");
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

    _body_inout = StringName("_body_inout");
    _area_inout = StringName("_area_inout");

    idle = StringName("idle");
    iteration = StringName("iteration");
    update = StringName("update");
    updated = StringName("updated");

    _get_gizmo_geometry = StringName("_get_gizmo_geometry");
    _can_gizmo_scale = StringName("_can_gizmo_scale");

    _physics_process = StringName("_physics_process");
    _process = StringName("_process");

    _enter_tree = StringName("_enter_tree");
    _exit_tree = StringName("_exit_tree");
    _enter_world = StringName("_enter_world");
    _exit_world = StringName("_exit_world");
    _ready = StringName("_ready");

    _update_scroll = StringName("_update_scroll");
    _update_xform = StringName("_update_xform");

    _clips_input = StringName("_clips_input");

    _proxgroup_add = StringName("_proxgroup_add");
    _proxgroup_remove = StringName("_proxgroup_remove");

    grouped = StringName("grouped");
    ungrouped = StringName("ungrouped");

    screen_entered = StringName("screen_entered");
    screen_exited = StringName("screen_exited");

    viewport_entered = StringName("viewport_entered");
    viewport_exited = StringName("viewport_exited");

    camera_entered = StringName("camera_entered");
    camera_exited = StringName("camera_exited");

    _body_enter_tree = StringName("_body_enter_tree");
    _body_exit_tree = StringName("_body_exit_tree");

    _area_enter_tree = StringName("_area_enter_tree");
    _area_exit_tree = StringName("_area_exit_tree");

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

    line_separation = StringName("line_separation");

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

    path_pp = NodePath("..");

    _default = StringName("default");

    for (int i = 0; i < MAX_MATERIALS; i++) {

        mesh_materials[i] = StringName("material/" + itos(i));
    }

    _mesh_changed = StringName("_mesh_changed");

    parameters_base_path = "parameters/";

    tracks_changed = "tracks_changed";
}
