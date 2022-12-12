/*************************************************************************/
/*  rendering_server_raster.cpp                                             */
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

#include "rendering_server_raster.h"

#include "renderer_instance_component.h"
#include "core/external_profiler.h"
#include "core/os/os.h"
#include "core/ecs_registry.h"
#include "core/project_settings.h"
#include "rendering_server_canvas.h"
#include "rendering_server_globals.h"
#include "rendering_server_scene.h"

// careful, these may run in different threads than the visual server

int RenderingServerRaster::changes[2] = {0};

/* BLACK BARS */

void RenderingServerRaster::black_bars_set_margins(int p_left, int p_top, int p_right, int p_bottom) {

    black_margin[(int8_t)Margin::Left] = p_left;
    black_margin[(int8_t)Margin::Top] = p_top;
    black_margin[(int8_t)Margin::Right] = p_right;
    black_margin[(int8_t)Margin::Bottom] = p_bottom;
}

void RenderingServerRaster::black_bars_set_images(RenderingEntity p_left, RenderingEntity p_top, RenderingEntity p_right, RenderingEntity p_bottom) {

    black_image[(int8_t)Margin::Left] = p_left;
    black_image[(int8_t)Margin::Top] = p_top;
    black_image[(int8_t)Margin::Right] = p_right;
    black_image[(int8_t)Margin::Bottom] = p_bottom;
}

void RenderingServerRaster::_draw_margins() {

    VSG::canvas_render->draw_window_margins(black_margin, black_image);
}

void RenderingServerRaster::set_ent_debug_name(RenderingEntity p1, StringView p2) const
{
    if(p1==entt::null) {
        return;
    }
    if(p2.empty()) {
        VSG::ecs->registry.remove<RenderingEntityName>(p1);
    } else {
        strncpy(VSG::ecs->registry.emplace<RenderingEntityName>(p1).name,p2.data(),std::min<int>(63,p2.size()));
    }
}

/* FREE */

void RenderingServerRaster::free_rid(RenderingEntity p_rid) {
    if(p_rid!=entt::null) {
        bool needs_update  = VSG::ecs->registry.any_of<RenderingScenarioComponent,RenderingInstanceComponent>(p_rid);
        VSG::storage->free(p_rid);
        if(needs_update) {
            //update_dirty_instances(); //in case something changed this
        }
    }
}

/* EVENT QUEUING */

void RenderingServerRaster::request_frame_drawn_callback(FrameDrawnCallback&& cb) {
    frame_drawn_callbacks.emplace_back(eastl::move(cb));
}

void RenderingServerRaster::draw(bool p_swap_buffers, double frame_step) {
    SCOPE_AUTONAMED;
    VSG::bvh_nodes_created = 0;
    VSG::bvh_nodes_destroyed = 0;

    //needs to be done before changes is reset to 0, to not force the editor to redraw
    RenderingServer::get_singleton()->emit_signal("frame_pre_draw");

    changes[0] = 0;
    changes[1] = 0;

    VSG::rasterizer->begin_frame(frame_step);
    PROFILER_STARTFRAME("viewport");

    VSG::scene->update_dirty_instances(); //update scene stuff

    VSG::viewport->draw_viewports();
    VSG::scene->render_probes();
    _draw_margins();
    VSG::rasterizer->end_frame(p_swap_buffers);
    PROFILER_ENDFRAME("viewport");

    {
        SCOPE_PROFILE("frame_drawn_callbacks");
        while (!frame_drawn_callbacks.empty()) {

            frame_drawn_callbacks[0]();
            frame_drawn_callbacks.pop_front();
        }
    }
    {
        SCOPE_PROFILE("frame_post_draw");
        RenderingServer::get_singleton()->emit_signal("frame_post_draw");
    }
    PROFILE_VALUE("BVH_Created",VSG::bvh_nodes_created);
    PROFILE_VALUE("BVH_Destroyed",VSG::bvh_nodes_destroyed);
}

bool RenderingServerRaster::has_changed(RenderingServerEnums::ChangedPriority p_priority) const {
    switch (p_priority) {
        default: {
            return (changes[0] > 0) || (changes[1] > 0);
        } break;
        case RS::CHANGED_PRIORITY_LOW: {
            return changes[0] > 0;
        } break;
        case RS::CHANGED_PRIORITY_HIGH: {
            return changes[1] > 0;
        } break;
    }
}
void RenderingServerRaster::init() {

    VSG::rasterizer->initialize();
}
void RenderingServerRaster::finish() {

    VSG::rasterizer->finalize();
}

/* STATUS INFORMATION */

uint64_t RenderingServerRaster::get_render_info(RS::RenderInfo p_info) {

    return VSG::storage->get_render_info(p_info);
}
const char *RenderingServerRaster::get_video_adapter_name() const {

    return VSG::storage->get_video_adapter_name();
}

const char *RenderingServerRaster::get_video_adapter_vendor() const {

    return VSG::storage->get_video_adapter_vendor();
}
/* TESTING */

void RenderingServerRaster::set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter) {

    redraw_request();
    VSG::rasterizer->set_boot_image(p_image, p_color, p_scale, p_use_filter);
}
void RenderingServerRaster::set_default_clear_color(const Color &p_color) {
    VSG::viewport->set_default_clear_color(p_color);
}

void RenderingServerRaster::set_shader_time_scale(float p_scale) {
    VSG::rasterizer->set_shader_time_scale(p_scale);
}

bool RenderingServerRaster::has_feature(RS::Features p_feature) const {

    return false;
}

bool RenderingServerRaster::has_os_feature(const StringName &p_feature) const {

    return VSG::storage->has_os_feature(p_feature);
}

void RenderingServerRaster::set_debug_generate_wireframes(bool p_generate) {

    VSG::storage->set_debug_generate_wireframes(p_generate);
}

void RenderingServerRaster::call_set_use_vsync(bool p_enable) {
    OS::get_singleton()->_set_use_vsync(p_enable);
}

// bool VisualServerRaster::is_low_end() const {
//     return VSG::rasterizer->is_low_end();
// }
RenderingServerRaster::RenderingServerRaster() {
    submission_thread_singleton = this;
    VSG::ecs = new("ECS_Registry<RenderingEntity,true>") ECS_Registry<RenderingEntity,true>();
    VSG::ecs->initialize();
    VSG::canvas = memnew(RenderingServerCanvas);
    VSG::viewport = memnew(VisualServerViewport);
    VSG::scene = memnew(VisualServerScene);
    VSG::rasterizer = Rasterizer::create();
    VSG::storage = VSG::rasterizer->get_storage();
    VSG::canvas_render = VSG::rasterizer->get_canvas();
    VSG::scene_render = VSG::rasterizer->get_scene();

    for (int i = 0; i < 4; i++) {
        black_margin[i] = 0;
        black_image[i] = entt::null;
    }
}

#ifdef DEBUG_ENABLED
static void check_rendering_entity_leaks() {
    if (VSG::ecs->registry.empty()) {
        return; // nothing to report.
    }
    WARN_PRINT("Rendering instances still exist!");
    if (OS::get_singleton()->is_stdout_verbose()) {
        VSG::ecs->registry.each([](const RenderingEntity ent) {
            if(!VSG::ecs->registry.orphan(ent)) {
                printf("Leaked Rendering instance: %x", entt::to_integral(ent));
            }
            else {
                printf("Orphaned Rendering entity: %x", entt::to_integral(ent));
            }
        });
    }
}
#endif
RenderingServerRaster::~RenderingServerRaster() {
    submission_thread_singleton = nullptr;

    memdelete(VSG::canvas);
    memdelete(VSG::viewport);
    memdelete(VSG::rasterizer);
    memdelete(VSG::scene);
#ifdef DEBUG_ENABLED
    check_rendering_entity_leaks();
#endif
    memdelete(VSG::ecs);
}
