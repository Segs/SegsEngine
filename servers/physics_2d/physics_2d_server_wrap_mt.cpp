/*************************************************************************/
/*  physics_2d_server_wrap_mt.cpp                                        */
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

#include "physics_2d_server_wrap_mt.h"

#include "core/os/os.h"

void Physics2DServerWrapMT::thread_exit() {

    exit.set();
}

void Physics2DServerWrapMT::thread_step(real_t p_delta) {

    physics_server_2d->step(p_delta);
    step_sem.post();
}

void Physics2DServerWrapMT::_thread_callback(void *_instance) {

    Physics2DServerWrapMT *vsmt = reinterpret_cast<Physics2DServerWrapMT *>(_instance);

    vsmt->thread_loop();
}

void Physics2DServerWrapMT::thread_loop() {

    server_thread = Thread::get_caller_id();

    physics_server_2d->init();

    exit.clear();
    step_thread_up.set();
    while (!exit.is_set()) {
        // flush commands one by one, until exit is requested
        command_queue.wait_and_flush_one();
    }

    command_queue.flush_all(); // flush all

    physics_server_2d->finish();
}

/* EVENT QUEUING */

void Physics2DServerWrapMT::step(real_t p_step) {

    if (create_thread) {

        command_queue.push([this,p_step]() { thread_step(p_step); });
    } else {

        command_queue.flush_all(); //flush all pending from other threads
        physics_server_2d->step(p_step);
    }
}

void Physics2DServerWrapMT::sync() {

    if (create_thread) {
        if (first_frame)
            first_frame = false;
        else
            step_sem.wait(); //must not wait if a step was not issued
    }
    physics_server_2d->sync();
}

void Physics2DServerWrapMT::flush_queries() {

    physics_server_2d->flush_queries();
}

void Physics2DServerWrapMT::end_sync() {

    physics_server_2d->end_sync();
}

void Physics2DServerWrapMT::init() {

    if (create_thread) {
        //OS::get_singleton()->release_rendering_thread();
        thread.start(_thread_callback, this);
        while (!step_thread_up.is_set()) {
            OS::get_singleton()->delay_usec(1000);
        }
    } else {

        physics_server_2d->init();
    }
}

void Physics2DServerWrapMT::finish() {

    if (thread.is_started()) {
        line_shape_free_cached_ids();
        ray_shape_free_cached_ids();
        segment_shape_free_cached_ids();
        circle_shape_free_cached_ids();
        rectangle_shape_free_cached_ids();
        capsule_shape_free_cached_ids();
        convex_polygon_shape_free_cached_ids();
        concave_polygon_shape_free_cached_ids();

        space_free_cached_ids();
        area_free_cached_ids();
        body_free_cached_ids();

        command_queue.push([this]() {thread_exit(); });
        thread.wait_to_finish();
    } else {
        for (auto v : line_shape_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        line_shape_id_pool.clear();

        for (auto v : ray_shape_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        ray_shape_id_pool.clear();

        for (auto v : segment_shape_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        segment_shape_id_pool.clear();

        for (auto v : circle_shape_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        circle_shape_id_pool.clear();

        for (auto v : rectangle_shape_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        rectangle_shape_id_pool.clear();

        for (auto v : capsule_shape_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        capsule_shape_id_pool.clear();

        for (auto v : convex_polygon_shape_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        convex_polygon_shape_id_pool.clear();

        for (auto v : concave_polygon_shape_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        concave_polygon_shape_id_pool.clear();


        for (auto v : space_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        space_id_pool.clear();

        for (auto v : area_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        area_id_pool.clear();

        for (auto v : body_id_pool) {
            submission_thread_singleton->free_rid(v);
        }
        body_id_pool.clear();


        physics_server_2d->finish();
    }
}

Physics2DServerWrapMT::Physics2DServerWrapMT(PhysicsServer2D *p_contained, bool p_create_thread) :
        command_queue(p_create_thread) {
    queueing_thread_singleton = this;
    physics_server_2d = p_contained;
    create_thread = p_create_thread;


    pool_max_size = T_GLOBAL_GET<int>("memory/limits/multithreaded_server/rid_pool_prealloc");

    if (!p_create_thread) {
        server_thread = Thread::get_caller_id();
    } else {
        server_thread = {};
    }

    main_thread = Thread::get_caller_id();
    first_frame = true;
}

Physics2DServerWrapMT::~Physics2DServerWrapMT() {

    memdelete(physics_server_2d);
    queueing_thread_singleton = nullptr;
    //finish();
}
