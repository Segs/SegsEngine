/*************************************************************************/
/*  rendering_server_wrap_mt.cpp                                            */
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

#include "rendering_server_wrap_mt.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "servers/rendering/rendering_server_raster.h"

void RenderingServerWrapMT::thread_exit() {

    exit = true;
}

void RenderingServerWrapMT::thread_draw(bool p_swap_buffers, double frame_step) {

    if (!atomic_decrement(&draw_pending)) {

        submission_thread_singleton->draw(p_swap_buffers, frame_step);
    }
}

void RenderingServerWrapMT::thread_flush() {

    atomic_decrement(&draw_pending);
}

void RenderingServerWrapMT::_thread_callback(void *_instance) {

    RenderingServerWrapMT *vsmt = reinterpret_cast<RenderingServerWrapMT *>(_instance);

    vsmt->thread_loop();
}

void RenderingServerWrapMT::thread_loop() {

    server_thread = Thread::get_caller_id();

    OS::get_singleton()->make_rendering_thread();

    submission_thread_singleton->init();

    exit = false;
    draw_thread_up = true;
    while (!exit) {
        // flush commands one by one, until exit is requested
        command_queue.wait_and_flush_one();
    }

    command_queue.flush_all(); // flush all

    submission_thread_singleton->finish();
}

/* EVENT QUEUING */

void RenderingServerWrapMT::sync() {

    if (create_thread) {

        atomic_increment(&draw_pending);
        command_queue.push_and_sync([this]() {thread_flush(); });
    } else {

        command_queue.flush_all(); //flush all pending from other threads
    }
}

void RenderingServerWrapMT::draw(bool p_swap_buffers, double frame_step) {

    if (create_thread) {

        atomic_increment(&draw_pending);
        command_queue.push([this,p_swap_buffers,frame_step]() {thread_draw(p_swap_buffers,frame_step); });
    } else {

        submission_thread_singleton->draw(p_swap_buffers, frame_step);
    }
}

void RenderingServerWrapMT::init() {

    if (!create_thread) {
        submission_thread_singleton->init();
        return;
    }

    print_verbose("VisualServerWrapMT: Creating render thread");
    OS::get_singleton()->release_rendering_thread();
    if (create_thread) {
        thread = Thread::create(_thread_callback, this);
        print_verbose("VisualServerWrapMT: Starting render thread");
    }
    while (!draw_thread_up) {
        OS::get_singleton()->delay_usec(1000);
    }
    print_verbose("VisualServerWrapMT: Finished render thread");
}


void RenderingServerWrapMT::finish() {


    if (thread) {
        texture_free_cached_ids();
        sky_free_cached_ids();
        shader_free_cached_ids();
        material_free_cached_ids();
        mesh_free_cached_ids();
        multimesh_free_cached_ids();
        immediate_free_cached_ids();
        skeleton_free_cached_ids();
        directional_light_free_cached_ids();
        omni_light_free_cached_ids();
        spot_light_free_cached_ids();
        reflection_probe_free_cached_ids();
        gi_probe_free_cached_ids();
        lightmap_capture_free_cached_ids();
        particles_free_cached_ids();
        camera_free_cached_ids();
        viewport_free_cached_ids();
        environment_free_cached_ids();
        scenario_free_cached_ids();
        instance_free_cached_ids();
        canvas_free_cached_ids();
        canvas_item_free_cached_ids();
        canvas_light_occluder_free_cached_ids();
        canvas_occluder_polygon_free_cached_ids();

        command_queue.push([this]() {thread_exit(); });
        Thread::wait_to_finish(thread);
        memdelete(thread);

        thread = nullptr;
    } else {
        assert(Thread::get_caller_id() == server_thread);
        for (auto v : texture_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        texture_id_pool.clear();

        for (auto v : sky_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        sky_id_pool.clear();

        for (auto v : shader_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        shader_id_pool.clear();

        for (auto v : material_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        material_id_pool.clear();

        for (auto v : mesh_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        mesh_id_pool.clear();

        for (auto v : multimesh_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        multimesh_id_pool.clear();

        for (auto v : immediate_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        immediate_id_pool.clear();

        for (auto v : skeleton_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        skeleton_id_pool.clear();

        for (auto v : directional_light_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        directional_light_id_pool.clear();

        for (auto v : omni_light_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        omni_light_id_pool.clear();

        for (auto v : spot_light_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        spot_light_id_pool.clear();

        for (auto v : reflection_probe_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        reflection_probe_id_pool.clear();

        for (auto v : gi_probe_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        gi_probe_id_pool.clear();

        for (auto v : lightmap_capture_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        lightmap_capture_id_pool.clear();

        for (auto v : particles_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        particles_id_pool.clear();

        for (auto v : camera_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        camera_id_pool.clear();

        for (auto v : viewport_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        viewport_id_pool.clear();

        for (auto v : environment_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        environment_id_pool.clear();

        for (auto v : scenario_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        scenario_id_pool.clear();

        for (auto v : instance_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        instance_id_pool.clear();

        for (auto v : canvas_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        canvas_id_pool.clear();

        for (auto v : canvas_item_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        canvas_item_id_pool.clear();

        for (auto v : canvas_light_occluder_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        canvas_light_occluder_id_pool.clear();

        for (auto v : canvas_occluder_polygon_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        canvas_occluder_polygon_id_pool.clear();

        submission_thread_singleton->finish();
    }

}

void RenderingServerWrapMT::set_use_vsync_callback(bool p_enable) {

    queueing_thread_singleton->call_set_use_vsync(p_enable);
}

RenderingServerWrapMT::RenderingServerWrapMT(bool p_create_thread) :
        command_queue(p_create_thread) {
    queueing_thread_singleton = this;

    OS::switch_vsync_function = set_use_vsync_callback; //as this goes to another thread, make sure it goes properly

    memnew(RenderingServerRaster);
    create_thread = p_create_thread;
    thread = nullptr;
    draw_pending = 0;
    draw_thread_up = false;
    pool_max_size = T_GLOBAL_GET<int>("memory/limits/multithreaded_server/rid_pool_prealloc");

    if (!p_create_thread) {
        server_thread = Thread::get_caller_id();
    } else {
        server_thread = 0;
    }
}

RenderingServerWrapMT::~RenderingServerWrapMT() {
    queueing_thread_singleton = nullptr;
    memdelete(submission_thread_singleton);
    //finish();
}
