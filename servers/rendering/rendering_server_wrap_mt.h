/*************************************************************************/
/*  rendering_server_wrap_mt.h                                              */
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

#include "core/command_queue_mt.h"
#include "core/os/thread.h"
#include "core/safe_refcount.h"
#include "servers/rendering_server.h"

class  RenderingServerWrapMT : public RenderingServer {

    // the real visual server
    mutable CommandQueueMT command_queue;
    Mutex alloc_mutex;
    Thread thread;
    int pool_max_size;
    SafeFlag exit;
    SafeFlag draw_thread_up;
    bool create_thread;

    //#define DEBUG_SYNC

    static void _thread_callback(void *_instance);
    void thread_loop();
    void thread_draw(bool p_swap_buffers, double frame_step);
    void thread_flush();
    void thread_exit();
#ifdef DEBUG_SYNC
#define SYNC_DEBUG print_line("sync on: " + String(__FUNCTION__));
#else
#define SYNC_DEBUG
#endif

public:
//#define ServerName RenderingServer
#define ServerNameWrapMT RenderingServerWrapMT
#include "servers/server_wrap_mt_common.h"
    void set_ent_debug_name(RenderingEntity p1, StringView p2) const override {
        assert(Thread::get_caller_id() != server_thread);
        command_queue.push([p1,p2]() {submission_thread_singleton->set_ent_debug_name(p1, p2);});
    }

    /* EVENT QUEUING */
    FUNCENT(texture)
    FUNC7(texture_allocate, RenderingEntity, int, int, int, Image::Format, RS::TextureType, uint32_t)
    FUNC3(texture_set_data, RenderingEntity, const Ref<Image> &, int)
    FUNC10(texture_set_data_partial, RenderingEntity, const Ref<Image> &, int, int, int, int, int, int, int, int)
    FUNC2RC(Ref<Image>, texture_get_data, RenderingEntity, int)
    FUNC2(texture_set_flags, RenderingEntity, uint32_t)
    FUNC1RC(uint32_t, texture_get_flags, RenderingEntity)
    FUNC1RC(Image::Format, texture_get_format, RenderingEntity)
    FUNC1RC(RS::TextureType, texture_get_type, RenderingEntity)
    FUNC1RC(uint32_t, texture_get_texid, RenderingEntity)
    FUNC1RC(uint32_t, texture_get_width, RenderingEntity)
    FUNC1RC(uint32_t, texture_get_height, RenderingEntity)
    FUNC1RC(uint32_t, texture_get_depth, RenderingEntity)
    FUNC4(texture_set_size_override, RenderingEntity, int, int, int)
    FUNC2(texture_bind, RenderingEntity, uint32_t)

    FUNC3(texture_set_detect_3d_callback, RenderingEntity, TextureDetectCallback, void *)
    FUNC3(texture_set_detect_srgb_callback, RenderingEntity, TextureDetectCallback, void *)
    FUNC3(texture_set_detect_normal_callback, RenderingEntity, TextureDetectCallback, void *)

    void texture_set_path(RenderingEntity p1, StringView p2) override {
        assert(Thread::get_caller_id() != server_thread);
        String by_val(p2);
        command_queue.push( [p1,by_val]() { submission_thread_singleton->texture_set_path(p1, by_val);});
    }

    const String &texture_get_path(RenderingEntity p1) const override {
        assert(Thread::get_caller_id() != server_thread);
        const String *ret;
        command_queue.push_and_sync( [p1,&ret]() { ret = &submission_thread_singleton->texture_get_path(p1);});
        SYNC_DEBUG
        return *ret;
    }
    FUNC1(texture_set_shrink_all_x2_on_set_data, bool)
    FUNC1S(texture_debug_usage, Vector<TextureInfo> *)

    FUNC1(textures_keep_original, bool)

    FUNC2(texture_set_proxy, RenderingEntity, RenderingEntity)

    FUNC2(texture_set_force_redraw_if_visible, RenderingEntity, bool)

    /* SKY API */

    FUNCENT(sky)
    FUNC3(sky_set_texture, RenderingEntity, RenderingEntity, int)

    /* SHADER API */

    FUNCENT(shader)

    FUNC2(shader_set_code, RenderingEntity, const String &)
    FUNC1RC(String, shader_get_code, RenderingEntity)

    FUNC2SC(shader_get_param_list, RenderingEntity, Vector<PropertyInfo> *)

    FUNC3(shader_set_default_texture_param, RenderingEntity, const StringName &, RenderingEntity)
    FUNC2RC(RenderingEntity, shader_get_default_texture_param, RenderingEntity, const StringName &)

    FUNC2(shader_add_custom_define, RenderingEntity, StringView)
    FUNC2SC(shader_get_custom_defines, RenderingEntity, Vector<StringView> *)
    FUNC2(shader_remove_custom_define, RenderingEntity, StringView)

    FUNC1(set_shader_async_hidden_forbidden, bool)
    /* COMMON MATERIAL API */

    FUNCENT(material)

    FUNC2(material_set_shader, RenderingEntity, RenderingEntity)
    FUNC1RC(RenderingEntity, material_get_shader, RenderingEntity)

    FUNC3(material_set_param, RenderingEntity, const StringName &, const Variant &)
    FUNC2RC(Variant, material_get_param, RenderingEntity, const StringName &)
    FUNC2RC(Variant, material_get_param_default, RenderingEntity, const StringName &)

    FUNC2(material_set_render_priority, RenderingEntity, int)
    FUNC2(material_set_line_width, RenderingEntity, float)
    FUNC2(material_set_next_pass, RenderingEntity, RenderingEntity)

    /* MESH API */

    FUNCENT(mesh)

    FUNC10(mesh_add_surface, RenderingEntity, uint32_t, RS::PrimitiveType, const PoolVector<uint8_t> &, int, const PoolVector<uint8_t> &, int, const AABB &, const Vector<PoolVector<uint8_t> > &, const PoolVector<AABB> &)

    FUNC2(mesh_set_blend_shape_count, RenderingEntity, int)
    FUNC1RC(int, mesh_get_blend_shape_count, RenderingEntity)

    FUNC2(mesh_set_blend_shape_mode, RenderingEntity, RS::BlendShapeMode)
    FUNC1RC(RS::BlendShapeMode, mesh_get_blend_shape_mode, RenderingEntity)

    FUNC4(mesh_surface_update_region, RenderingEntity, int, int, const PoolVector<uint8_t> &)

    FUNC3(mesh_surface_set_material, RenderingEntity, int, RenderingEntity)
    FUNC2RC(RenderingEntity, mesh_surface_get_material, RenderingEntity, int)

    FUNC2RC(int, mesh_surface_get_array_len, RenderingEntity, int)
    FUNC2RC(int, mesh_surface_get_array_index_len, RenderingEntity, int)

    FUNC2RC(PoolVector<uint8_t>, mesh_surface_get_array, RenderingEntity, int)
    FUNC2RC(PoolVector<uint8_t>, mesh_surface_get_index_array, RenderingEntity, int)

    FUNC2RC(uint32_t, mesh_surface_get_format, RenderingEntity, int)
    FUNC2RC(RS::PrimitiveType, mesh_surface_get_primitive_type, RenderingEntity, int)

    FUNC2RC(AABB, mesh_surface_get_aabb, RenderingEntity, int)
    FUNC2RC(Vector<Vector<uint8_t> >, mesh_surface_get_blend_shapes, RenderingEntity, int)
    const Vector<AABB> & mesh_surface_get_skeleton_aabb(RenderingEntity p1, int p2) const override {
        assert(Thread::get_caller_id() != server_thread);
        using RetType = const Vector<AABB> *;

        RetType ret;
        command_queue.push_and_sync( [p1,p2,&ret]() { ret = &(submission_thread_singleton->mesh_surface_get_skeleton_aabb(p1, p2));});
        SYNC_DEBUG
        return *ret;
    }

    FUNC2(mesh_remove_surface, RenderingEntity, int)
    FUNC1RC(int, mesh_get_surface_count, RenderingEntity)

    FUNC2(mesh_set_custom_aabb, RenderingEntity, const AABB &)
    FUNC1RC(AABB, mesh_get_custom_aabb, RenderingEntity)

    FUNC1(mesh_clear, RenderingEntity)

    /* MULTIMESH API */

    FUNCENT(multimesh)

    FUNC5(multimesh_allocate, RenderingEntity, int, RS::MultimeshTransformFormat, RS::MultimeshColorFormat, RS::MultimeshCustomDataFormat)
    FUNC1RC(int, multimesh_get_instance_count, RenderingEntity)

    FUNC2(multimesh_set_mesh, RenderingEntity, RenderingEntity)
    FUNC3(multimesh_instance_set_transform, RenderingEntity, int, const Transform &)
    FUNC3(multimesh_instance_set_transform_2d, RenderingEntity, int, const Transform2D &)
    FUNC3(multimesh_instance_set_color, RenderingEntity, int, const Color &)
    FUNC3(multimesh_instance_set_custom_data, RenderingEntity, int, const Color &)

    FUNC1RC(RenderingEntity, multimesh_get_mesh, RenderingEntity)
    FUNC1RC(AABB, multimesh_get_aabb, RenderingEntity)

    FUNC2RC(Transform, multimesh_instance_get_transform, RenderingEntity, int)
    FUNC2RC(Transform2D, multimesh_instance_get_transform_2d, RenderingEntity, int)
    FUNC2RC(Color, multimesh_instance_get_color, RenderingEntity, int)
    FUNC2RC(Color, multimesh_instance_get_custom_data, RenderingEntity, int)

    FUNC2(multimesh_set_as_bulk_array, RenderingEntity, Span<const float>)

    FUNC2(multimesh_set_visible_instances, RenderingEntity, int)
    FUNC1RC(int, multimesh_get_visible_instances, RenderingEntity)

    /* IMMEDIATE API */

    FUNCENT(immediate)
    FUNC3(immediate_begin, RenderingEntity, RS::PrimitiveType, RenderingEntity)
    FUNC2(immediate_vertex, RenderingEntity, const Vector3 &)
    FUNC2(immediate_normal, RenderingEntity, const Vector3 &)
    FUNC2(immediate_tangent, RenderingEntity, const Plane &)
    FUNC2(immediate_color, RenderingEntity, const Color &)
    FUNC2(immediate_uv, RenderingEntity, const Vector2 &)
    FUNC2(immediate_uv2, RenderingEntity, const Vector2 &)
    FUNC1(immediate_end, RenderingEntity)
    FUNC1(immediate_clear, RenderingEntity)
    FUNC2(immediate_set_material, RenderingEntity, RenderingEntity)
    FUNC1RC(RenderingEntity, immediate_get_material, RenderingEntity)

    /* SKELETON API */

    FUNCENT(skeleton)
    FUNC3(skeleton_allocate, RenderingEntity, int, bool)
    FUNC1RC(int, skeleton_get_bone_count, RenderingEntity)
    FUNC3(skeleton_bone_set_transform, RenderingEntity, int, const Transform &)
    FUNC2RC(Transform, skeleton_bone_get_transform, RenderingEntity, int)
    FUNC3(skeleton_bone_set_transform_2d, RenderingEntity, int, const Transform2D &)
    FUNC2RC(Transform2D, skeleton_bone_get_transform_2d, RenderingEntity, int)
    FUNC2(skeleton_set_base_transform_2d, RenderingEntity, const Transform2D &)

    /* Light API */

    FUNCENT(directional_light)
    FUNCENT(omni_light)
    FUNCENT(spot_light)

    FUNC2(light_set_color, RenderingEntity, const Color &)
    FUNC3(light_set_param, RenderingEntity, RS::LightParam, float)
    FUNC2(light_set_shadow, RenderingEntity, bool)
    FUNC2(light_set_shadow_color, RenderingEntity, const Color &)
    FUNC2(light_set_projector, RenderingEntity, RenderingEntity)
    FUNC2(light_set_negative, RenderingEntity, bool)
    FUNC2(light_set_cull_mask, RenderingEntity, uint32_t)
    FUNC2(light_set_reverse_cull_face_mode, RenderingEntity, bool)
    FUNC2(light_set_use_gi, RenderingEntity, bool)
    FUNC2(light_set_bake_mode, RenderingEntity, RS::LightBakeMode)

    FUNC2(light_omni_set_shadow_mode, RenderingEntity, RS::LightOmniShadowMode)
    FUNC2(light_omni_set_shadow_detail, RenderingEntity, RS::LightOmniShadowDetail)

    FUNC2(light_directional_set_shadow_mode, RenderingEntity, RS::LightDirectionalShadowMode)
    FUNC2(light_directional_set_blend_splits, RenderingEntity, bool)
    FUNC2(light_directional_set_shadow_depth_range_mode, RenderingEntity, RS::LightDirectionalShadowDepthRangeMode)

    /* PROBE API */

    FUNCENT(reflection_probe)

    FUNC2(reflection_probe_set_update_mode, RenderingEntity, RS::ReflectionProbeUpdateMode)
    FUNC2(reflection_probe_set_intensity, RenderingEntity, float)
    FUNC2(reflection_probe_set_interior_ambient, RenderingEntity, const Color &)
    FUNC2(reflection_probe_set_interior_ambient_energy, RenderingEntity, float)
    FUNC2(reflection_probe_set_interior_ambient_probe_contribution, RenderingEntity, float)
    FUNC2(reflection_probe_set_max_distance, RenderingEntity, float)
    FUNC2(reflection_probe_set_extents, RenderingEntity, const Vector3 &)
    FUNC2(reflection_probe_set_origin_offset, RenderingEntity, const Vector3 &)
    FUNC2(reflection_probe_set_as_interior, RenderingEntity, bool)
    FUNC2(reflection_probe_set_enable_box_projection, RenderingEntity, bool)
    FUNC2(reflection_probe_set_enable_shadows, RenderingEntity, bool)
    FUNC2(reflection_probe_set_cull_mask, RenderingEntity, uint32_t)
    FUNC2(reflection_probe_set_resolution, RenderingEntity, int)

    /* BAKED LIGHT API */

    FUNCENT(gi_probe)

    FUNC2(gi_probe_set_bounds, RenderingEntity, const AABB &)
    FUNC1RC(AABB, gi_probe_get_bounds, RenderingEntity)

    FUNC2(gi_probe_set_cell_size, RenderingEntity, float)
    FUNC1RC(float, gi_probe_get_cell_size, RenderingEntity)

    FUNC2(gi_probe_set_to_cell_xform, RenderingEntity, const Transform &)
    FUNC1RC(Transform, gi_probe_get_to_cell_xform, RenderingEntity)

    FUNC2(gi_probe_set_dynamic_range, RenderingEntity, int)
    FUNC1RC(int, gi_probe_get_dynamic_range, RenderingEntity)

    FUNC2(gi_probe_set_energy, RenderingEntity, float)
    FUNC1RC(float, gi_probe_get_energy, RenderingEntity)

    FUNC2(gi_probe_set_bias, RenderingEntity, float)
    FUNC1RC(float, gi_probe_get_bias, RenderingEntity)

    FUNC2(gi_probe_set_normal_bias, RenderingEntity, float)
    FUNC1RC(float, gi_probe_get_normal_bias, RenderingEntity)

    FUNC2(gi_probe_set_propagation, RenderingEntity, float)
    FUNC1RC(float, gi_probe_get_propagation, RenderingEntity)

    FUNC2(gi_probe_set_interior, RenderingEntity, bool)
    FUNC1RC(bool, gi_probe_is_interior, RenderingEntity)

    FUNC2(gi_probe_set_dynamic_data, RenderingEntity, const PoolVector<int> &)
    FUNC1RC(PoolVector<int>, gi_probe_get_dynamic_data, RenderingEntity)

    /* LIGHTMAP CAPTURE */

    FUNCENT(lightmap_capture)

    FUNC2(lightmap_capture_set_bounds, RenderingEntity, const AABB &)
    FUNC1RC(AABB, lightmap_capture_get_bounds, RenderingEntity)

    FUNC2(lightmap_capture_set_octree, RenderingEntity, const PoolVector<uint8_t> &)
    FUNC1RC(PoolVector<uint8_t>, lightmap_capture_get_octree, RenderingEntity)
    FUNC2(lightmap_capture_set_octree_cell_transform, RenderingEntity, const Transform &)
    FUNC1RC(Transform, lightmap_capture_get_octree_cell_transform, RenderingEntity)
    FUNC2(lightmap_capture_set_octree_cell_subdiv, RenderingEntity, int)
    FUNC1RC(int, lightmap_capture_get_octree_cell_subdiv, RenderingEntity)
    FUNC2(lightmap_capture_set_energy, RenderingEntity, float)
    FUNC1RC(float, lightmap_capture_get_energy, RenderingEntity)
    FUNC2(lightmap_capture_set_interior, RenderingEntity, bool)
    FUNC1RC(bool, lightmap_capture_is_interior, RenderingEntity)

    /* PARTICLES */

    FUNCENT(particles)

    FUNC2(particles_set_emitting, RenderingEntity, bool)
    FUNC1R(bool, particles_get_emitting, RenderingEntity)
    FUNC2(particles_set_amount, RenderingEntity, int)
    FUNC2(particles_set_lifetime, RenderingEntity, float)
    FUNC2(particles_set_one_shot, RenderingEntity, bool)
    FUNC2(particles_set_pre_process_time, RenderingEntity, float)
    FUNC2(particles_set_explosiveness_ratio, RenderingEntity, float)
    FUNC2(particles_set_randomness_ratio, RenderingEntity, float)
    FUNC2(particles_set_custom_aabb, RenderingEntity, const AABB &)
    FUNC2(particles_set_speed_scale, RenderingEntity, float)
    FUNC2(particles_set_use_local_coordinates, RenderingEntity, bool)
    FUNC2(particles_set_process_material, RenderingEntity, RenderingEntity)
    FUNC2(particles_set_fixed_fps, RenderingEntity, int)
    FUNC2(particles_set_fractional_delta, RenderingEntity, bool)
    FUNC1R(bool, particles_is_inactive, RenderingEntity)
    FUNC1(particles_request_process, RenderingEntity)
    FUNC1(particles_restart, RenderingEntity)

    FUNC2(particles_set_draw_order, RenderingEntity, RS::ParticlesDrawOrder)

    FUNC2(particles_set_draw_passes, RenderingEntity, int)
    FUNC3(particles_set_draw_pass_mesh, RenderingEntity, int, RenderingEntity)
    FUNC2(particles_set_emission_transform, RenderingEntity, const Transform &)

    FUNC1R(AABB, particles_get_current_aabb, RenderingEntity)

    /* CAMERA API */

    FUNCENT(camera)
    FUNC4(camera_set_perspective, RenderingEntity, float, float, float)
    FUNC4(camera_set_orthogonal, RenderingEntity, float, float, float)
    FUNC5(camera_set_frustum, RenderingEntity, float, Vector2, float, float)
    FUNC2(camera_set_transform, RenderingEntity, const Transform &)
    FUNC2(camera_set_cull_mask, RenderingEntity, uint32_t)
    FUNC2(camera_set_environment, RenderingEntity, RenderingEntity)
    FUNC2(camera_set_use_vertical_aspect, RenderingEntity, bool)

    /* VIEWPORT TARGET API */

    FUNCENT(viewport)

    FUNC2(viewport_set_use_arvr, RenderingEntity, bool)

    FUNC3(viewport_set_size, RenderingEntity, int, int)

    FUNC2(viewport_set_active, RenderingEntity, bool)
    FUNC2(viewport_set_parent_viewport, RenderingEntity, RenderingEntity)

    FUNC2(viewport_set_clear_mode, RenderingEntity, RS::ViewportClearMode)

    FUNC3(viewport_attach_to_screen, RenderingEntity, const Rect2 &, int)
    FUNC1(viewport_detach, RenderingEntity)

    FUNC2(viewport_set_update_mode, RenderingEntity, RS::ViewportUpdateMode)
    FUNC2(viewport_set_vflip, RenderingEntity, bool)

    FUNC1RC(RenderingEntity, viewport_get_texture, RenderingEntity)

    FUNC2(viewport_set_hide_scenario, RenderingEntity, bool)
    FUNC2(viewport_set_hide_canvas, RenderingEntity, bool)
    FUNC2(viewport_set_disable_environment, RenderingEntity, bool)
    FUNC2(viewport_set_disable_3d, RenderingEntity, bool)
    FUNC2(viewport_set_keep_3d_linear, RenderingEntity, bool)

    FUNC2(viewport_attach_camera, RenderingEntity, RenderingEntity)
    FUNC2(viewport_set_scenario, RenderingEntity, RenderingEntity)
    FUNC2(viewport_attach_canvas, RenderingEntity, RenderingEntity)

    FUNC2(viewport_remove_canvas, RenderingEntity, RenderingEntity)
    FUNC3(viewport_set_canvas_transform, RenderingEntity, RenderingEntity, const Transform2D &)
    FUNC2(viewport_set_transparent_background, RenderingEntity, bool)

    FUNC2(viewport_set_global_canvas_transform, RenderingEntity, const Transform2D &)
    FUNC4(viewport_set_canvas_stacking, RenderingEntity, RenderingEntity, int, int)
    FUNC2(viewport_set_shadow_atlas_size, RenderingEntity, int)
    FUNC3(viewport_set_shadow_atlas_quadrant_subdivision, RenderingEntity, int, int)
    FUNC2(viewport_set_msaa, RenderingEntity, RS::ViewportMSAA)
    FUNC2(viewport_set_use_fxaa, RenderingEntity, bool)
    FUNC2(viewport_set_use_debanding, RenderingEntity, bool)
    FUNC2(viewport_set_sharpen_intensity, RenderingEntity, float)
    FUNC2(viewport_set_hdr, RenderingEntity, bool)
    FUNC2(viewport_set_use_32_bpc_depth, RenderingEntity, bool)
    FUNC2(viewport_set_usage, RenderingEntity, RS::ViewportUsage)

    //this passes directly to avoid stalling, but it's pretty dangerous, so don't call after freeing a viewport
    uint64_t viewport_get_render_info(RenderingEntity p_viewport, RS::ViewportRenderInfo p_info) override {
        return submission_thread_singleton->viewport_get_render_info(p_viewport, p_info);
    }

    FUNC2(viewport_set_debug_draw, RenderingEntity, RS::ViewportDebugDraw)

    /* ENVIRONMENT API */

    FUNCENT(environment)

    FUNC2(environment_set_background, RenderingEntity, RS::EnvironmentBG)
    FUNC2(environment_set_sky, RenderingEntity, RenderingEntity)
    FUNC2(environment_set_sky_custom_fov, RenderingEntity, float)
    FUNC2(environment_set_sky_orientation, RenderingEntity, const Basis &)
    FUNC2(environment_set_bg_color, RenderingEntity, const Color &)
    FUNC2(environment_set_bg_energy, RenderingEntity, float)
    FUNC2(environment_set_canvas_max_layer, RenderingEntity, int)
    FUNC4(environment_set_ambient_light, RenderingEntity, const Color &, float, float)
    FUNC2(environment_set_camera_feed_id, RenderingEntity, int)
    FUNC7(environment_set_ssr, RenderingEntity, bool, int, float, float, float, bool)
    FUNC13(environment_set_ssao, RenderingEntity, bool, float, float, float, float, float, float, float, const Color &, RS::EnvironmentSSAOQuality, RS::EnvironmentSSAOBlur, float)

    FUNC6(environment_set_dof_blur_near, RenderingEntity, bool, float, float, float, RS::EnvironmentDOFBlurQuality)
    FUNC6(environment_set_dof_blur_far, RenderingEntity, bool, float, float, float, RS::EnvironmentDOFBlurQuality)
    FUNC12(environment_set_glow, RenderingEntity, bool, int, float, float, float, RS::EnvironmentGlowBlendMode, float, float, float, bool, bool)

    FUNC9(environment_set_tonemap, RenderingEntity, RS::EnvironmentToneMapper, float, float, bool, float, float, float, float)

    FUNC6(environment_set_adjustment, RenderingEntity, bool, float, float, float, RenderingEntity)

    FUNC5(environment_set_fog, RenderingEntity, bool, const Color &, const Color &, float)
    FUNC7(environment_set_fog_depth, RenderingEntity, bool, float, float, float, bool, float)
    FUNC5(environment_set_fog_height, RenderingEntity, bool, float, float, float)

    FUNCENT(scenario)

    FUNC2(scenario_set_debug, RenderingEntity, RS::ScenarioDebugMode)
    FUNC2(scenario_set_environment, RenderingEntity, RenderingEntity)
    FUNC3(scenario_set_reflection_atlas_size, RenderingEntity, int, int)
    FUNC2(scenario_set_fallback_environment, RenderingEntity, RenderingEntity)

    /* INSTANCING API */

    FUNCENT(instance)

    FUNC2(instance_set_base, RenderingEntity, RenderingEntity) // from can be mesh, light, poly, area and portal so far.
    FUNC2(instance_set_scenario, RenderingEntity, RenderingEntity) // from can be mesh, light, poly, area and portal so far.
    FUNC2(instance_set_layer_mask, RenderingEntity, uint32_t)
    FUNC2(instance_set_transform, RenderingEntity, const Transform &)
    FUNC2(instance_attach_object_instance_id, RenderingEntity, GameEntity)
    FUNC3(instance_set_blend_shape_weight, RenderingEntity, int, float)
    FUNC3(instance_set_surface_material, RenderingEntity, int, RenderingEntity)
    FUNC2(instance_set_visible, RenderingEntity, bool)
    FUNC5(instance_set_use_lightmap, RenderingEntity, RenderingEntity, RenderingEntity, int, const Rect2 &)

    FUNC2(instance_set_custom_aabb, RenderingEntity, AABB)

    FUNC2(instance_attach_skeleton, RenderingEntity, RenderingEntity)

    FUNC2(instance_set_extra_visibility_margin, RenderingEntity, real_t)
    /* PORTALS API */

    FUNC2(instance_set_portal_mode, RenderingEntity, RS::InstancePortalMode)
    /* OCCLUDERS API */
    FUNCENT(occluder_instance)
    FUNC2(occluder_instance_set_scenario, RenderingEntity, RenderingEntity)
    FUNC2(occluder_instance_link_resource, RenderingEntity, RenderingEntity)
    FUNC2(occluder_instance_set_transform, RenderingEntity, const Transform &)
    FUNC2(occluder_instance_set_active, RenderingEntity, bool)

    FUNCENT(occluder_resource)
    FUNC2(occluder_resource_prepare, RenderingEntity, RS::OccluderType)
    FUNC2(occluder_resource_spheres_update, RenderingEntity, const Vector<Plane> &)
    FUNC2(occluder_resource_mesh_update, RenderingEntity, const OccluderMeshData &)

    FUNC1(set_use_occlusion_culling, bool)
    FUNC1RC(Geometry::MeshData, occlusion_debug_get_current_polys, RenderingEntity)

    // Callbacks
    FUNC1(callbacks_register, RenderingServerCallbacks *)
    // don't use these in a game!
    FUNC2RC(Vector<GameEntity>, instances_cull_aabb, const AABB &, RenderingEntity)
    FUNC3RC(Vector<GameEntity>, instances_cull_ray, const Vector3 &, const Vector3 &, RenderingEntity)
    FUNC2RC(Vector<GameEntity>, instances_cull_convex, Span<const Plane>, RenderingEntity)

    FUNC3(instance_geometry_set_flag, RenderingEntity, RS::InstanceFlags, bool)
    FUNC2(instance_geometry_set_cast_shadows_setting, RenderingEntity, RS::ShadowCastingSetting)
    FUNC2(instance_geometry_set_material_override, RenderingEntity, RenderingEntity)
    FUNC2(instance_geometry_set_material_overlay, RenderingEntity, RenderingEntity)

    FUNC5(instance_geometry_set_draw_range, RenderingEntity, float, float, float, float)
    FUNC2(instance_geometry_set_as_instance_lod, RenderingEntity, RenderingEntity)

    /* CANVAS (2D) */

    FUNCENT(canvas)
    FUNC3(canvas_set_item_mirroring, RenderingEntity, RenderingEntity, const Point2 &)
    FUNC2(canvas_set_modulate, RenderingEntity, const Color &)
    FUNC3(canvas_set_parent, RenderingEntity, RenderingEntity, float)
    FUNC1(canvas_set_disable_scale, bool)

    FUNCENT(canvas_item)
    FUNC2(canvas_item_set_parent, RenderingEntity, RenderingEntity)

    FUNC2(canvas_item_set_visible, RenderingEntity, bool)
    FUNC2(canvas_item_set_light_mask, RenderingEntity, int)

    FUNC2(canvas_item_set_update_when_visible, RenderingEntity, bool)

    FUNC2(canvas_item_set_transform, RenderingEntity, const Transform2D &)
    FUNC2(canvas_item_set_clip, RenderingEntity, bool)
    FUNC2(canvas_item_set_distance_field_mode, RenderingEntity, bool)
    FUNC3(canvas_item_set_custom_rect, RenderingEntity, bool, const Rect2 &)
    FUNC2(canvas_item_set_modulate, RenderingEntity, const Color &)
    FUNC2(canvas_item_set_self_modulate, RenderingEntity, const Color &)

    FUNC2(canvas_item_set_draw_behind_parent, RenderingEntity, bool)

    FUNC6(canvas_item_add_line, RenderingEntity, const Point2 &, const Point2 &, const Color &, float, bool)
    FUNC5(canvas_item_add_polyline, RenderingEntity, Span<const Vector2>, Span<const Color>, float, bool)
    FUNC5(canvas_item_add_multiline, RenderingEntity, Span<const Vector2>, Span<const Color> , float, bool)
    FUNC3(canvas_item_add_rect, RenderingEntity, const Rect2 &, const Color &)
    FUNC4(canvas_item_add_circle, RenderingEntity, const Point2 &, float, const Color &)
    FUNC7(canvas_item_add_texture_rect, RenderingEntity, const Rect2 &, RenderingEntity, bool, const Color &, bool, RenderingEntity)
    FUNC8(canvas_item_add_texture_rect_region, RenderingEntity, const Rect2 &, RenderingEntity, const Rect2 &, const Color &, bool, RenderingEntity, bool)
    FUNC11(canvas_item_add_nine_patch, RenderingEntity, const Rect2 &, const Rect2 &, RenderingEntity, const Vector2 &, const Vector2 &, RS::NinePatchAxisMode, RS::NinePatchAxisMode, bool, const Color &, RenderingEntity)
    FUNC7(canvas_item_add_primitive, RenderingEntity, Span<const Vector2>, Span<const Color>, const PoolVector<Point2> &, RenderingEntity, float, RenderingEntity)
    FUNC7(canvas_item_add_polygon, RenderingEntity, Span<const Point2>, Span<const Color>, Span<const Point2>, RenderingEntity, RenderingEntity, bool)
    FUNC12(canvas_item_add_triangle_array, RenderingEntity, Span<const int>, Span<const Point2>, Span<const Color>, Span<const Point2>, const PoolVector<int> &, const PoolVector<float> &, RenderingEntity, int, RenderingEntity, bool,bool)
    FUNC6(canvas_item_add_mesh, RenderingEntity, RenderingEntity, const Transform2D &, const Color &, RenderingEntity, RenderingEntity)
    FUNC4(canvas_item_add_multimesh, RenderingEntity, RenderingEntity, RenderingEntity, RenderingEntity)
    FUNC4(canvas_item_add_particles, RenderingEntity, RenderingEntity, RenderingEntity, RenderingEntity)
    FUNC2(canvas_item_add_set_transform, RenderingEntity, const Transform2D &)
    FUNC2(canvas_item_add_clip_ignore, RenderingEntity, bool)
    FUNC2(canvas_item_set_sort_children_by_y, RenderingEntity, bool)
    FUNC2(canvas_item_set_z_index, RenderingEntity, int)
    FUNC2(canvas_item_set_z_as_relative_to_parent, RenderingEntity, bool)
    FUNC3(canvas_item_set_copy_to_backbuffer, RenderingEntity, bool, const Rect2 &)
    FUNC2(canvas_item_attach_skeleton, RenderingEntity, RenderingEntity)

    FUNC1(canvas_item_clear, RenderingEntity)
    FUNC2(canvas_item_set_draw_index, RenderingEntity, int)

    FUNC2(canvas_item_set_material, RenderingEntity, RenderingEntity)

    FUNC2(canvas_item_set_use_parent_material, RenderingEntity, bool)

    FUNC0R(RenderingEntity, canvas_light_create)
    FUNC2(canvas_light_attach_to_canvas, RenderingEntity, RenderingEntity)
    FUNC2(canvas_light_set_enabled, RenderingEntity, bool)
    FUNC2(canvas_light_set_scale, RenderingEntity, float)
    FUNC2(canvas_light_set_transform, RenderingEntity, const Transform2D &)
    FUNC2(canvas_light_set_texture, RenderingEntity, RenderingEntity)
    FUNC2(canvas_light_set_texture_offset, RenderingEntity, const Vector2 &)
    FUNC2(canvas_light_set_color, RenderingEntity, const Color &)
    FUNC2(canvas_light_set_height, RenderingEntity, float)
    FUNC2(canvas_light_set_energy, RenderingEntity, float)
    FUNC3(canvas_light_set_z_range, RenderingEntity, int, int)
    FUNC3(canvas_light_set_layer_range, RenderingEntity, int, int)
    FUNC2(canvas_light_set_item_cull_mask, RenderingEntity, int)
    FUNC2(canvas_light_set_item_shadow_cull_mask, RenderingEntity, int)

    FUNC2(canvas_light_set_mode, RenderingEntity, RS::CanvasLightMode)

    FUNC2(canvas_light_set_shadow_enabled, RenderingEntity, bool)
    FUNC2(canvas_light_set_shadow_buffer_size, RenderingEntity, int)
    FUNC2(canvas_light_set_shadow_gradient_length, RenderingEntity, float)
    FUNC2(canvas_light_set_shadow_filter, RenderingEntity, RS::CanvasLightShadowFilter)
    FUNC2(canvas_light_set_shadow_color, RenderingEntity, const Color &)
    FUNC2(canvas_light_set_shadow_smooth, RenderingEntity, float)

    FUNCENT(canvas_light_occluder)
    FUNC2(canvas_light_occluder_attach_to_canvas, RenderingEntity, RenderingEntity)
    FUNC2(canvas_light_occluder_set_enabled, RenderingEntity, bool)
    FUNC2(canvas_light_occluder_set_polygon, RenderingEntity, RenderingEntity)
    FUNC2(canvas_light_occluder_set_transform, RenderingEntity, const Transform2D &)
    FUNC2(canvas_light_occluder_set_light_mask, RenderingEntity, int)

    FUNCENT(canvas_occluder_polygon)
    FUNC3(canvas_occluder_polygon_set_shape, RenderingEntity, Span<const Vector2>, bool)
    FUNC2(canvas_occluder_polygon_set_shape_as_lines, RenderingEntity, Span<const Vector2>)

    FUNC2(canvas_occluder_polygon_set_cull_mode, RenderingEntity, RS::CanvasOccluderPolygonCullMode)

    /* BLACK BARS */

    FUNC4(black_bars_set_margins, int, int, int, int)
    FUNC4(black_bars_set_images, RenderingEntity, RenderingEntity, RenderingEntity, RenderingEntity)

    /* FREE */

    FUNC1(free_rid, RenderingEntity)

    /* EVENT QUEUING */

    void request_frame_drawn_callback(Callable &&p1) override {
        assert (Thread::get_caller_id() != server_thread);
        command_queue.push([p1 = eastl::move(p1)]() mutable {
            submission_thread_singleton->request_frame_drawn_callback(eastl::move(p1));
        });
    }

    void init() override;
    void finish() override;
    void draw(bool p_swap_buffers, double frame_step) override;
    void sync();
    FUNC0(tick)
    FUNC1(pre_draw, bool)
    FUNC1RC(bool, has_changed, RS::ChangedPriority)

    /* RENDER INFO */

    //this passes directly to avoid stalling
    uint64_t get_render_info(RS::RenderInfo p_info) override {
        return submission_thread_singleton->get_render_info(p_info);
    }
    const char * get_video_adapter_name() const override{
        return submission_thread_singleton->get_video_adapter_name();
    }

    const char * get_video_adapter_vendor() const override {
        return submission_thread_singleton->get_video_adapter_vendor();
    }

    FUNC4(set_boot_image, const Ref<Image> &, const Color &, bool, bool)
    FUNC1(set_default_clear_color, const Color &)
	FUNC1(set_shader_time_scale, float)

    FUNC1(set_debug_generate_wireframes, bool)

    bool has_feature(RS::Features p_feature) const override { return submission_thread_singleton->has_feature(p_feature); }
    bool has_os_feature(const StringName &p_feature) const override { return submission_thread_singleton->has_os_feature(p_feature); }

    FUNC1(call_set_use_vsync, bool)

    static void set_use_vsync_callback(bool p_enable);

//    bool is_low_end() const override {
//        return rendering_server->is_low_end();
//    }

    GODOT_EXPORT RenderingServerWrapMT(bool p_create_thread);
    ~RenderingServerWrapMT() override;
    static RenderingServerWrapMT *get() { return (RenderingServerWrapMT*)queueing_thread_singleton; }

    static void queue_operation(eastl::function<void()> func)
    {
        get()->command_queue.push(func);
    }
    static void queue_synced_operation(eastl::function<void()> func)
    {
        get()->command_queue.push_and_sync(func);
    }

//#undef ServerName
#undef ServerNameWrapMT
#undef server_name
};

#ifdef DEBUG_SYNC
#undef DEBUG_SYNC
#endif
#undef SYNC_DEBUG
