/*************************************************************************/
/*  scene_string_names.h                                                 */
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

#include "core/node_path.h"
#include "core/string_name.h"
#include "core/os/memory.h"

class SceneStringNames {

    friend void register_scene_types();
    friend void unregister_scene_types();

    static void create();
    static void free();
public:
    static StringName _estimate_cost;
    static StringName _compute_cost;
    static StringName line_separation;
    static StringName tree_exited;
    static StringName resized;
    static StringName dot;
    static StringName doubledot;
    static StringName draw;
    static StringName hide;
    static StringName visibility_changed;
    static StringName input_event;
    static StringName _input_event;
    static StringName gui_input;
    static StringName _gui_input;
    static StringName item_rect_changed;
    static StringName shader;
    static StringName shader_unshaded;
    static StringName shading_mode;
    static StringName tree_entered;
    static StringName tree_exiting;
    static StringName ready;
    static StringName child_entered_tree;
    static StringName child_exiting_tree;
    static StringName size_flags_changed;
    static StringName minimum_size_changed;
    static StringName sleeping_state_changed;
    static StringName mouse_entered;
    static StringName mouse_exited;
    static StringName focus_entered;
    static StringName focus_exited;
    static StringName sort_children;
    static StringName finished;
    static StringName loop_finished;
    static StringName step_finished;
    static StringName animation_finished;
    static StringName animation_changed;
    static StringName animation_started;
    static StringName body_shape_entered;
    static StringName body_entered;
    static StringName body_shape_exited;
    static StringName body_exited;
    static StringName area_shape_entered;
    static StringName area_shape_exited;
    static StringName _physics_process;
    static StringName _process;
    static StringName _enter_tree;
    static StringName _exit_tree;
    static StringName _draw;
    static StringName _input;
    static StringName _ready;
    static StringName _unhandled_input;
    static StringName _unhandled_key_input;
    static StringName _pressed;
    static StringName _toggled;
    static StringName _update_scroll;
    static StringName _clips_input;
    static StringName _proxgroup_remove;
    static StringName grouped;
    static StringName ungrouped;
    static StringName has_point;
    static StringName get_drag_data;
    static StringName can_drop_data;
    static StringName drop_data;
    static StringName screen_entered;
    static StringName screen_exited;
    static StringName viewport_entered;
    static StringName viewport_exited;
    static StringName camera_entered;
    static StringName camera_exited;
    static StringName changed;
    static StringName offset;
    static StringName unit_offset;
    static StringName rotation_mode;
    static StringName rotate;
    static StringName v_offset;
    static StringName h_offset;
    static StringName transform_pos;
    static StringName transform_rot;
    static StringName transform_scale;
    static StringName _update_remote;
    static StringName _update_pairs;
    static StringName area_entered;
    static StringName area_exited;
    static StringName _get_minimum_size;
    static StringName _im_update;
    static StringName _queue_update;
    static StringName baked_light_changed;
    static StringName _baked_light_changed;
    static StringName _mouse_enter;
    static StringName _mouse_exit;
    static StringName frame_changed;
    static StringName playback_speed;
    static StringName playback_active;
    static StringName autoplay;
    static StringName blend_times;
    static StringName speed;
    static StringName _default;
    static StringName node_configuration_warning_changed;
    static StringName output;
    static StringName parameters_base_path;
    static StringName tracks_changed;

    static StringName physics_process_internal;
    static StringName physics_process;

    static NodePath path_pp;

};
