/*************************************************************************/
/*  rendering_server_viewport.cpp                                           */
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

#include "rendering_server_viewport.h"

#include "rendering_server_canvas.h"
#include "rendering_server_globals.h"
#include "rendering_server_scene.h"
#include "render_entity_getter.h"

#include "core/external_profiler.h"
#include "core/map.h"
#include "core/project_settings.h"

#include "servers/rendering/rendering_server_globals.h"
#include "EASTL/sort.h"

namespace {
struct CanvasKey {
    int64_t stacking;
    RenderingEntity canvas=entt::null;
    bool operator<(const CanvasKey &p_canvas) const {
        if (stacking == p_canvas.stacking)
            return entt::to_integral(canvas) < entt::to_integral(p_canvas.canvas);
        return stacking < p_canvas.stacking;
    }
    CanvasKey() { stacking = 0; }
    CanvasKey(RenderingEntity p_canvas, int p_layer, int p_sublayer) {
        canvas = p_canvas;
        int64_t sign = p_layer < 0 ? -1 : 1;
        stacking = sign * (((int64_t)ABS(p_layer)) << 32) + p_sublayer;
    }
    [[nodiscard]] int get_layer() const { return stacking >> 32; }
};
struct ViewportSort {
    _FORCE_INLINE_ bool operator()(RenderingEntity p_left, RenderingEntity p_right) const {
        auto *left=get<RenderingViewportComponent>(p_left);
        auto *right=get<RenderingViewportComponent>(p_right);
        bool left_to_screen = left->viewport_to_screen_rect.size != Size2();
        bool right_to_screen = right->viewport_to_screen_rect.size != Size2();

        if (left_to_screen == right_to_screen) {
            return left->parent == right->self;
        }
        return right_to_screen;
    }
};
} // namespace

static Transform2D _canvas_get_transform(RenderingViewportCanvasComponent *p_viewport, const Transform2D &view_global_transform, RenderingCanvasComponent *p_canvas, RenderingViewportCanvasComponent::CanvasData *p_canvas_data, const Vector2 &p_vp_size) {

    Transform2D xf = view_global_transform;

    float scale = 1.0f;


    if (p_viewport->canvas_map.contains(p_canvas->parent)) {

        Transform2D c_xform = p_viewport->canvas_map[p_canvas->parent].transform;
        xf = xf * c_xform;
        scale = p_canvas->parent_scale;
    }

    Transform2D c_xform = p_canvas_data->transform;
    xf = xf * c_xform;

    if (scale != 1.0f && !VSG::canvas->disable_scale) {
        Vector2 pivot = p_vp_size * 0.5;
        Transform2D xfpivot;
        xfpivot.set_origin(pivot);
        Transform2D xfscale;
        xfscale.scale(Vector2(scale, scale));

        xf = xfpivot.affine_inverse() * xf;
        xf = xfscale * xf;
        xf = xfpivot * xf;
    }

    return xf;
}

void VisualServerViewport::_draw_3d(RenderingViewportComponent *p_viewport, ARVREyes p_eye) {
    Ref<ARVRInterface> arvr_interface;
    if (ARVRServer::get_singleton() != nullptr) {
        arvr_interface = ARVRServer::get_singleton()->get_primary_interface();
    }

    if (p_viewport->use_arvr && arvr_interface) {
        VSG::scene->render_camera(arvr_interface, p_eye, p_viewport->camera, p_viewport->scenario, p_viewport->size, p_viewport->shadow_atlas);
    } else {
        VSG::scene->render_camera(p_viewport->camera, p_viewport->scenario, p_viewport->size, p_viewport->shadow_atlas);
    }
}

void VisualServerViewport::_draw_viewport(RenderingViewportComponent *p_viewport,RenderingViewportCanvasComponent *p_vp_canvas, ARVREyes p_eye) {

    /* Camera3D should always be BEFORE any other 3D */

    bool scenario_draw_canvas_bg = false; //draw canvas, or some layer of it, as BG for 3D instead of in front
    int scenario_canvas_max_layer = 0;

    if (!p_viewport->hide_canvas && !p_viewport->disable_environment && p_viewport->scenario!=entt::null) {

        RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_viewport->scenario);
        if (VSG::scene_render->is_environment(scenario->environment)) {
            scenario_draw_canvas_bg = VSG::scene_render->environment_get_background(scenario->environment) == RS::ENV_BG_CANVAS;

            scenario_canvas_max_layer = VSG::scene_render->environment_get_canvas_max_layer(scenario->environment);
        }
    }

    bool can_draw_3d = !p_viewport->disable_3d && !p_viewport->disable_3d_by_usage && VisualServerScene::owns_camera(p_viewport->camera);

    if (p_viewport->clear_mode != RS::VIEWPORT_CLEAR_NEVER) {
        VSG::rasterizer->clear_render_target(p_viewport->transparent_bg ? Color(0, 0, 0, 0) : clear_color);
        if (p_viewport->clear_mode == RS::VIEWPORT_CLEAR_ONLY_NEXT_FRAME) {
            p_viewport->clear_mode = RS::VIEWPORT_CLEAR_NEVER;
        }
    }

    if (!scenario_draw_canvas_bg && can_draw_3d) {
        _draw_3d(p_viewport, p_eye);
    }

    if (p_viewport->hide_canvas) {
        return;
    }
    int i = 0;

    Map<CanvasKey, RenderingViewportCanvasComponent::CanvasData *> canvas_map;

    Rect2 clip_rect(0, 0, p_viewport->size.x, p_viewport->size.y);
    FixedVector<RasterizerCanvasLight3DComponent *,32> lights_with_shadow;
    FixedVector<RasterizerCanvasLight3DComponent *,32> lights_with_mask;
    FixedVector<RasterizerCanvasLight3DComponent *,32> lights_filtered;
    Rect2 shadow_rect;

    for (eastl::pair<const RenderingEntity ,RenderingViewportCanvasComponent::CanvasData> &E : p_vp_canvas->canvas_map) {

        RenderingCanvasComponent *canvas = get<RenderingCanvasComponent>(E.second.canvas);

        Transform2D xf = _canvas_get_transform(p_vp_canvas, p_viewport->global_transform,canvas, &E.second, clip_rect.size);

        //find lights in canvas

        auto lights_view(VSG::ecs->registry.view<RasterizerCanvasLight3DComponent>());
        for (auto light_ent : canvas->lights) {

            auto * cl = &lights_view.get<RasterizerCanvasLight3DComponent>(light_ent);
            if (!cl->enabled || cl->texture==entt::null)
                continue;
            //not super efficient..
            Size2 tsize = VSG::storage->texture_size_with_proxy(cl->texture);
            tsize *= cl->scale;
            // Skip using lights with texture of 0 size
            if (!tsize.x || !tsize.y) {
                continue;
            }

            Vector2 offset = tsize / 2.0;
            cl->rect_cache = Rect2(-offset + cl->texture_offset, tsize);
            cl->xform_cache = xf * cl->xform;

            if (clip_rect.intersects_transformed(cl->xform_cache, cl->rect_cache)) {

                lights_filtered.emplace_back(cl);
                cl->texture_cache = nullptr;
                Transform2D scale;
                scale.scale(cl->rect_cache.size);
                scale.elements[2] = cl->rect_cache.position;
                cl->light_shader_xform = (cl->xform_cache * scale).affine_inverse();
                cl->light_shader_pos = cl->xform_cache[2];
                if (cl->shadow_buffer!=entt::null) {
                    if (lights_with_shadow.empty()) {
                        shadow_rect = cl->xform_cache.xform(cl->rect_cache);
                    } else {
                        shadow_rect = shadow_rect.merge(cl->xform_cache.xform(cl->rect_cache));
                    }
                    lights_with_shadow.emplace_back(cl);
                    cl->radius_cache = cl->rect_cache.size.length();
                }
                if (cl->mode == RS::CANVAS_LIGHT_MODE_MASK) {
                    lights_with_mask.emplace_back(cl);
                }

            }

            VSG::canvas_render->light_internal_update(cl->light_internal, cl);
        }

        canvas_map[CanvasKey(E.first, E.second.layer, E.second.sublayer)] = &E.second;
    }

    if (!lights_with_shadow.empty()) {
        //update shadows if any

        RenderingEntity occluders = entt::null;

        //make list of occluders
        auto occluders_view(VSG::ecs->registry.view<RasterizerCanvasLightOccluderInstanceComponent>());

        for (auto &E : p_vp_canvas->canvas_map) {

            RenderingCanvasComponent *canvas = get<RenderingCanvasComponent>(E.second.canvas);
            Transform2D xf = _canvas_get_transform(p_vp_canvas, p_viewport->global_transform,canvas, &E.second, clip_rect.size);
            for (auto occluder_ent : canvas->occluders) {
                auto * F = &occluders_view.get<RasterizerCanvasLightOccluderInstanceComponent>(occluder_ent);

                if (!F->enabled) {
                    continue;
                }
                F->xform_cache = xf * F->xform;
                if (shadow_rect.intersects_transformed(F->xform_cache, F->aabb_cache)) {

                    F->next = occluders;
                    occluders = occluder_ent;
                }
            }
        }
        //update the light shadowmaps with them
        for(RasterizerCanvasLight3DComponent *light : lights_with_shadow) {
            VSG::canvas_render->canvas_light_shadow_buffer_update(light->shadow_buffer, light->xform_cache.affine_inverse(), light->item_shadow_mask, light->radius_cache / 1000.0, light->radius_cache * 1.1, occluders, &light->shadow_matrix_cache);
        }

        //VSG::canvas_render->reset_canvas();
    }

    VSG::rasterizer->restore_render_target(!scenario_draw_canvas_bg && can_draw_3d);

    if (scenario_draw_canvas_bg && !canvas_map.empty() && canvas_map.begin()->first.get_layer() > scenario_canvas_max_layer) {
        if (!can_draw_3d) {
            VSG::scene->render_empty_scene(p_viewport->scenario, p_viewport->shadow_atlas);
        } else {
            _draw_3d(p_viewport, p_eye);
        }
        scenario_draw_canvas_bg = false;
    }

    for (auto &E : canvas_map) {
        auto *canvas = get<RenderingCanvasComponent>(E.second->canvas);

        Transform2D xform = _canvas_get_transform(p_vp_canvas,p_viewport->global_transform, canvas, E.second, clip_rect.size);

        FixedVector<RasterizerCanvasLight3DComponent *,32> canvas_lights;

        for(RasterizerCanvasLight3DComponent *ptr : lights_with_mask) {
            if (E.second->layer >= ptr->layer_min && E.second->layer <= ptr->layer_max) {
                canvas_lights.emplace_back(ptr);
            }
        }

        VSG::canvas->render_canvas(canvas, xform, canvas_lights, lights_with_mask, clip_rect);
        i++;

        if (scenario_draw_canvas_bg && E.first.get_layer() >= scenario_canvas_max_layer) {
            if (!can_draw_3d) {
                VSG::scene->render_empty_scene(p_viewport->scenario, p_viewport->shadow_atlas);
            } else {
                _draw_3d(p_viewport, p_eye);
            }

            scenario_draw_canvas_bg = false;
        }
    }

    if (scenario_draw_canvas_bg) {
        if (!can_draw_3d) {
            VSG::scene->render_empty_scene(p_viewport->scenario, p_viewport->shadow_atlas);
        } else {
            _draw_3d(p_viewport, p_eye);
        }
    }

    //VSG::canvas_render->canvas_debug_viewport_shadows(lights_with_shadow);
}

void VisualServerViewport::draw_viewports() {

    // get our arvr interface in case we need it
    Ref<ARVRInterface> arvr_interface;

    if (ARVRServer::get_singleton() != nullptr) {
        arvr_interface = ARVRServer::get_singleton()->get_primary_interface();

        // process all our active interfaces
        ARVRServer::get_singleton()->_process();
    }

    if (Engine::get_singleton()->is_editor_hint()) {
        clear_color = T_GLOBAL_GET<Color>("rendering/environment/default_clear_color");
    }

    //sort viewports
    eastl::sort(active_viewports.begin(),active_viewports.end(),ViewportSort());

    //draw viewports
    for (RenderingEntity viewport_ent : active_viewports) {
#ifdef TRACY_ENABLE
        char buf[32]="ActiveVP:";
        snprintf(buf+9,5,"_%x",entt::to_integral(viewport_ent));
        ZoneScoped("frame_drawn_callbacks");
        ZoneText(buf,strlen(buf));
#endif
        RenderingViewportComponent *vp = get<RenderingViewportComponent>(viewport_ent);

        if (vp->update_mode == RS::VIEWPORT_UPDATE_DISABLED) {
            continue;
        }

        ERR_CONTINUE(vp->render_target==entt::null);

        auto vp_canvas = &VSG::ecs->registry.get<RenderingViewportCanvasComponent>(vp->self);

        if (vp->use_arvr) {
            // In ARVR mode it is our interface that controls our size
            if (arvr_interface) {
                // override our size, make sure it matches our required size
                vp->size = arvr_interface->get_render_targetsize();
            } else {
                // reset this, we can't render the output without a valid interface (this will likely be so when we're in the editor)
                vp->size = Vector2(0, 0);
            }
        }
        bool visible = vp->viewport_to_screen_rect != Rect2() || vp->update_mode == RS::VIEWPORT_UPDATE_ALWAYS || vp->update_mode == RS::VIEWPORT_UPDATE_ONCE || (vp->update_mode == RS::VIEWPORT_UPDATE_WHEN_VISIBLE && VSG::storage->render_target_was_used(vp->render_target));
        visible = visible && vp->size.x > 1 && vp->size.y > 1;

        if (!visible) {
            continue;
        }

        VSG::storage->render_target_clear_used(vp->render_target);

        if (vp->use_arvr && arvr_interface) {
            VSG::storage->render_target_set_size(vp->render_target, vp->size.x, vp->size.y);

            // render mono or left eye first
            ARVREyes leftOrMono = arvr_interface->is_stereo() ? ARVREyes::EYE_LEFT : ARVREyes::EYE_MONO;

            // check for an external texture destination for our left eye/mono
            VSG::storage->render_target_set_external_texture(vp->render_target,
                    arvr_interface->get_external_texture_for_eye(leftOrMono),
                    arvr_interface->get_external_depth_for_eye(leftOrMono));

            // set our render target as current
            VSG::rasterizer->set_current_render_target(vp->render_target);

            // and draw left eye/mono
            _draw_viewport(vp,vp_canvas, leftOrMono);
            arvr_interface->commit_for_eye(leftOrMono, vp->render_target, vp->viewport_to_screen_rect);

            // render right eye
            if (leftOrMono == ARVREyes::EYE_LEFT) {
                // check for an external texture destination for our right eye
                VSG::storage->render_target_set_external_texture(vp->render_target,
                        arvr_interface->get_external_texture_for_eye(ARVREyes::EYE_RIGHT),
                        arvr_interface->get_external_depth_for_eye(ARVREyes::EYE_RIGHT));

                // commit for eye may have changed the render target
                VSG::rasterizer->set_current_render_target(vp->render_target);

                _draw_viewport(vp,vp_canvas, ARVREyes::EYE_RIGHT);
                arvr_interface->commit_for_eye(ARVREyes::EYE_RIGHT, vp->render_target, vp->viewport_to_screen_rect);
            }

            // and for our frame timing, mark when we've finished committing our eyes
            ARVRServer::get_singleton()->_mark_commit();
        } else {
            VSG::storage->render_target_set_external_texture(vp->render_target, 0, 0);
            VSG::rasterizer->set_current_render_target(vp->render_target);

            VSG::scene_render->set_debug_draw_mode(vp->debug_draw);
            VSG::storage->render_info_begin_capture();

            // render standard mono camera
            _draw_viewport(vp,vp_canvas);

            VSG::storage->render_info_end_capture();
            vp->render_info[RS::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] = VSG::storage->get_captured_render_info(RS::INFO_OBJECTS_IN_FRAME);
            vp->render_info[RS::VIEWPORT_RENDER_INFO_VERTICES_IN_FRAME] = VSG::storage->get_captured_render_info(RS::INFO_VERTICES_IN_FRAME);
            vp->render_info[RS::VIEWPORT_RENDER_INFO_MATERIAL_CHANGES_IN_FRAME] = VSG::storage->get_captured_render_info(RS::INFO_MATERIAL_CHANGES_IN_FRAME);
            vp->render_info[RS::VIEWPORT_RENDER_INFO_SHADER_CHANGES_IN_FRAME] = VSG::storage->get_captured_render_info(RS::INFO_SHADER_CHANGES_IN_FRAME);
            vp->render_info[RS::VIEWPORT_RENDER_INFO_SURFACE_CHANGES_IN_FRAME] = VSG::storage->get_captured_render_info(RS::INFO_SURFACE_CHANGES_IN_FRAME);
            vp->render_info[RS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] = VSG::storage->get_captured_render_info(RS::INFO_DRAW_CALLS_IN_FRAME);
            vp->render_info[RS::VIEWPORT_RENDER_INFO_2D_ITEMS_IN_FRAME] = VSG::storage->get_captured_render_info(RS::INFO_2D_ITEMS_IN_FRAME);
            vp->render_info[RS::VIEWPORT_RENDER_INFO_2D_DRAW_CALLS_IN_FRAME] = VSG::storage->get_captured_render_info(RS::INFO_2D_DRAW_CALLS_IN_FRAME);

            if (vp->viewport_to_screen_rect != Rect2()) {
                //copy to screen if set as such
                VSG::rasterizer->set_current_render_target(entt::null);
                VSG::rasterizer->blit_render_target_to_screen(vp->render_target, vp->viewport_to_screen_rect, vp->viewport_to_screen);
            }
        }

        if (vp->update_mode == RS::VIEWPORT_UPDATE_ONCE) {
            vp->update_mode = RS::VIEWPORT_UPDATE_DISABLED;
        }
        VSG::scene_render->set_debug_draw_mode(RS::VIEWPORT_DEBUG_DRAW_DISABLED);
    }
}

RenderingEntity VisualServerViewport::viewport_create() {
    RenderingEntity res = VSG::ecs->create();
    auto &viewport(VSG::ecs->registry.emplace<RenderingViewportComponent>(res));
    VSG::ecs->registry.emplace<RenderingViewportCanvasComponent>(res).self=res;

    viewport.self = res;
    viewport.hide_scenario = false;
    viewport.hide_canvas = false;
    viewport.render_target = VSG::storage->render_target_create();
    viewport.shadow_atlas = VSG::scene_render->shadow_atlas_create();

    return res;
}

void VisualServerViewport::viewport_set_use_arvr(RenderingEntity p_viewport, bool p_use_arvr) {
    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    if (viewport->use_arvr == p_use_arvr) {
        return;
    }
    viewport->use_arvr = p_use_arvr;
    if (!viewport->use_arvr && viewport->size.width > 0 && viewport->size.height > 0) {
        // No longer controlled by our XR server, make sure we reset it
        VSG::storage->render_target_set_size(viewport->render_target, viewport->size.width, viewport->size.height);
    }
}

void VisualServerViewport::viewport_set_size(RenderingEntity p_viewport, int p_width, int p_height) {

    ERR_FAIL_COND(p_width < 0 && p_height < 0);

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->size = Size2(p_width, p_height);
    if (!viewport->use_arvr) {
        // Only update if this is not controlled by our XR server
        VSG::storage->render_target_set_size(viewport->render_target, p_width, p_height);
    }
}

void VisualServerViewport::viewport_set_active(RenderingEntity p_viewport, bool p_active) {

    ERR_FAIL_COND(!VSG::ecs->registry.any_of<RenderingViewportComponent>(p_viewport));

    if (p_active) {
        ERR_FAIL_COND_MSG(active_viewports.contains(p_viewport), "Can't make active a Viewport that is already active.");
        active_viewports.push_back(p_viewport);
    } else {
        active_viewports.erase_first(p_viewport);
    }
}

void VisualServerViewport::viewport_set_parent_viewport(RenderingEntity p_viewport, RenderingEntity p_parent_viewport) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->parent = p_parent_viewport;
}

void VisualServerViewport::viewport_set_clear_mode(RenderingEntity p_viewport, RS::ViewportClearMode p_clear_mode) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->clear_mode = p_clear_mode;
}

void VisualServerViewport::viewport_attach_to_screen(RenderingEntity p_viewport, const Rect2 &p_rect, int p_screen) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->viewport_to_screen_rect = p_rect;
    viewport->viewport_to_screen = p_screen;
}

void VisualServerViewport::viewport_detach(RenderingEntity p_viewport) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->viewport_to_screen_rect = Rect2();
    viewport->viewport_to_screen = 0;
}

void VisualServerViewport::viewport_set_update_mode(RenderingEntity p_viewport, RS::ViewportUpdateMode p_mode) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->update_mode = p_mode;
}
void VisualServerViewport::viewport_set_vflip(RenderingEntity p_viewport, bool p_enable) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_VFLIP, p_enable);
}

RenderingEntity VisualServerViewport::viewport_get_texture(RenderingEntity p_viewport) const {

    const auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND_V(!viewport, entt::null);

    return VSG::storage->render_target_get_texture(viewport->render_target);
}

void VisualServerViewport::viewport_set_hide_scenario(RenderingEntity p_viewport, bool p_hide) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->hide_scenario = p_hide;
}
void VisualServerViewport::viewport_set_hide_canvas(RenderingEntity p_viewport, bool p_hide) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->hide_canvas = p_hide;
}
void VisualServerViewport::viewport_set_disable_environment(RenderingEntity p_viewport, bool p_disable) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->disable_environment = p_disable;
}

void VisualServerViewport::viewport_set_disable_3d(RenderingEntity p_viewport, bool p_disable) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->disable_3d = p_disable;
    //VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_3D, p_disable);
    //this should be just for disabling rendering of 3D, to actually disable it, set usage
}

void VisualServerViewport::viewport_set_keep_3d_linear(RenderingEntity p_viewport, bool p_keep_3d_linear) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->keep_3d_linear = p_keep_3d_linear;
    VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_KEEP_3D_LINEAR, p_keep_3d_linear);
}

void VisualServerViewport::viewport_attach_camera(RenderingEntity p_viewport, RenderingEntity p_camera) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->camera = p_camera;
}
void VisualServerViewport::viewport_set_scenario(RenderingEntity p_viewport, RenderingEntity p_scenario) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->scenario = p_scenario;
}
void VisualServerViewport::viewport_attach_canvas(RenderingEntity p_viewport, RenderingEntity p_canvas) {

    auto *viewport_canvas = get<RenderingViewportCanvasComponent>(p_viewport);
    ERR_FAIL_COND(!viewport_canvas);

    ERR_FAIL_COND(viewport_canvas->canvas_map.contains(p_canvas));
    auto *canvas = get<RenderingCanvasComponent>(p_canvas);
    ERR_FAIL_COND(!canvas);

    canvas->viewports.insert(p_viewport);
    viewport_canvas->canvas_map[p_canvas] = {p_canvas,{},0,0};
}

void viewport_remove_canvas(RenderingEntity  p_viewport, RenderingEntity  p_canvas) {

    auto *viewport_canvas = get<RenderingViewportCanvasComponent>(p_viewport);
    ERR_FAIL_COND(!viewport_canvas);

    auto *canvas = get<RenderingCanvasComponent>(p_canvas);
    ERR_FAIL_COND(!canvas);

    viewport_canvas->canvas_map.erase(p_canvas);
    canvas->viewports.erase(p_viewport);
}


void VisualServerViewport::viewport_remove_canvas(RenderingEntity  p_viewport, RenderingEntity  p_canvas) {
    ::viewport_remove_canvas(p_viewport,p_canvas);
}
void VisualServerViewport::viewport_set_canvas_transform(RenderingEntity  p_viewport, RenderingEntity  p_canvas, const Transform2D &p_offset) {
    auto *viewport_canvas = get<RenderingViewportCanvasComponent>(p_viewport);
    ERR_FAIL_COND(!viewport_canvas);

    ERR_FAIL_COND(!viewport_canvas->canvas_map.contains(p_canvas));
    viewport_canvas->canvas_map[p_canvas].transform = p_offset;
}
void VisualServerViewport::viewport_set_transparent_background(RenderingEntity p_viewport, bool p_enabled) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_TRANSPARENT, p_enabled);
    viewport->transparent_bg = p_enabled;
}

void VisualServerViewport::viewport_set_global_canvas_transform(RenderingEntity p_viewport, const Transform2D &p_transform) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->global_transform = p_transform;
}
void VisualServerViewport::viewport_set_canvas_stacking(RenderingEntity p_viewport, RenderingEntity p_canvas, int p_layer, int p_sublayer) {

    auto *viewport_canvas = get<RenderingViewportCanvasComponent>(p_viewport);
    ERR_FAIL_COND(!viewport_canvas);

    ERR_FAIL_COND(!viewport_canvas->canvas_map.contains(p_canvas));
    auto &entry(viewport_canvas->canvas_map[p_canvas]);
    entry.layer = p_layer;
    entry.sublayer = p_sublayer;
}

void VisualServerViewport::viewport_set_shadow_atlas_size(RenderingEntity p_viewport, int p_size) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->shadow_atlas_size = p_size;

    VSG::scene_render->shadow_atlas_set_size(viewport->shadow_atlas, viewport->shadow_atlas_size);
}

void VisualServerViewport::viewport_set_shadow_atlas_quadrant_subdivision(RenderingEntity p_viewport, int p_quadrant, int p_subdiv) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    VSG::scene_render->shadow_atlas_set_quadrant_subdivision(viewport->shadow_atlas, p_quadrant, p_subdiv);
}

void VisualServerViewport::viewport_set_msaa(RenderingEntity p_viewport, RS::ViewportMSAA p_msaa) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    VSG::storage->render_target_set_msaa(viewport->render_target, p_msaa);
}

void VisualServerViewport::viewport_set_use_fxaa(RenderingEntity p_viewport, bool p_fxaa) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    VSG::storage->render_target_set_use_fxaa(viewport->render_target, p_fxaa);
}

void VisualServerViewport::viewport_set_use_debanding(RenderingEntity p_viewport, bool p_debanding) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    VSG::storage->render_target_set_use_debanding(viewport->render_target, p_debanding);
}

void VisualServerViewport::viewport_set_sharpen_intensity(RenderingEntity p_viewport, float p_intensity) {
    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    VSG::storage->render_target_set_sharpen_intensity(viewport->render_target, p_intensity);
}
void VisualServerViewport::viewport_set_hdr(RenderingEntity p_viewport, bool p_enabled) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_HDR, p_enabled);
}

void VisualServerViewport::viewport_set_use_32_bpc_depth(RenderingEntity p_viewport, bool p_enabled) {
    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_USE_32_BPC_DEPTH, p_enabled);
}
void VisualServerViewport::viewport_set_usage(RenderingEntity p_viewport, RS::ViewportUsage p_usage) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    switch (p_usage) {
        case RS::VIEWPORT_USAGE_2D: {

            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_3D, true);
            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_3D_EFFECTS, true);
            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_SAMPLING, false);

            viewport->disable_3d_by_usage = true;
        } break;
        case RS::VIEWPORT_USAGE_2D_NO_SAMPLING: {

            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_3D, true);
            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_3D_EFFECTS, true);
            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_SAMPLING, true);
            viewport->disable_3d_by_usage = true;
        } break;
        case RS::VIEWPORT_USAGE_3D: {

            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_3D, false);
            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_3D_EFFECTS, false);
            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_SAMPLING, false);
            viewport->disable_3d_by_usage = false;
        } break;
        case RS::VIEWPORT_USAGE_3D_NO_EFFECTS: {

            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_3D, false);
            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_3D_EFFECTS, true);
            VSG::storage->render_target_set_flag(viewport->render_target, RS::RENDER_TARGET_NO_SAMPLING, false);
            viewport->disable_3d_by_usage = false;
        } break;
    }
}

uint64_t VisualServerViewport::viewport_get_render_info(RenderingEntity p_viewport, RS::ViewportRenderInfo p_info) {

    ERR_FAIL_INDEX_V(p_info, RS::VIEWPORT_RENDER_INFO_MAX, -1);

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    if (!viewport) {
        return 0; //there should be a lock here..

    }
    return viewport->render_info[p_info];
}

void VisualServerViewport::viewport_set_debug_draw(RenderingEntity p_viewport, RS::ViewportDebugDraw p_draw) {

    auto *viewport = get<RenderingViewportComponent>(p_viewport);
    ERR_FAIL_COND(!viewport);

    viewport->debug_draw = p_draw;
}

void VisualServerViewport::set_default_clear_color(const Color &p_color) {
    clear_color = p_color;
}

void RenderingViewportComponent::unregister_from_active_viewports()
{
    if(self!=entt::null)
        VSG::viewport->active_viewports.erase_first(self);

}

RenderingViewportComponent &RenderingViewportComponent::operator=(RenderingViewportComponent &&f) {
    if(this==&f) {
        render_info.fill(0);
    }
    if(render_target!=entt::null) {
        VSG::storage->free(render_target);
        render_target = entt::null;
    }
    if(shadow_atlas!=entt::null) {
        VSG::storage->free(shadow_atlas);
        shadow_atlas = entt::null;
    }

    unregister_from_active_viewports();

    render_target = eastl::move(f.render_target);
    shadow_atlas = eastl::move(f.shadow_atlas);
    scenario = eastl::move(f.scenario);
    self = eastl::move(f.self);

    global_transform = eastl::move(f.global_transform);
    parent = eastl::move(f.parent);
    camera = eastl::move(f.camera);
    //render_target_texture = eastl::move(f.render_target_texture);
    size = eastl::move(f.size);
    viewport_to_screen_rect = eastl::move(f.viewport_to_screen_rect);

    render_info = eastl::move(f.render_info);

    viewport_to_screen = eastl::move(f.viewport_to_screen);
    shadow_atlas_size = eastl::move(f.shadow_atlas_size);
    update_mode = f.update_mode;
    debug_draw = f.debug_draw;
    clear_mode = f.clear_mode;

    hide_scenario = f.hide_scenario;
    hide_canvas = f.hide_canvas;
    disable_environment = f.disable_environment;
    disable_3d = f.disable_3d;
    disable_3d_by_usage = f.disable_3d_by_usage;
    keep_3d_linear = f.keep_3d_linear;
    use_arvr = f.use_arvr; /* use arvr interface to override camera positioning and projection matrices and control output */
    transparent_bg = f.transparent_bg;

    return *this;
}

RenderingViewportComponent::~RenderingViewportComponent()
{
    if(render_target!=entt::null)
        VSG::storage->free(render_target);
    if(shadow_atlas!=entt::null)
        VSG::storage->free(shadow_atlas);
    scenario = entt::null;
    unregister_from_active_viewports();
}

void RenderingViewportCanvasComponent::unregister_from_canvas() {
    for(const eastl::pair<const RenderingEntity , CanvasData> &cav_map_entry : canvas_map) {
        auto p_canvas = cav_map_entry.first;
        auto *canvas  = get<RenderingCanvasComponent>(p_canvas);
        ERR_FAIL_COND(!canvas);

        canvas->viewports.erase(self);
    }
    canvas_map.clear();
}

RenderingViewportCanvasComponent::~RenderingViewportCanvasComponent()
{
    unregister_from_canvas();
}
