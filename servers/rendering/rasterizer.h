/*************************************************************************/
/*  rasterizer.h                                                         */
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

#include "render_entity_helpers.h"
#include "core/math/camera_matrix.h"
#include "servers/rendering_server.h"

#include "core/self_list.h"

struct RenderingInstanceComponent;

class RasterizerScene {
public:
    /* SHADOW ATLAS API */

    virtual RenderingEntity shadow_atlas_create() = 0;
    virtual void shadow_atlas_set_size(RenderingEntity p_atlas, int p_size) = 0;
    virtual void shadow_atlas_set_quadrant_subdivision(RenderingEntity p_atlas, int p_quadrant, int p_subdivision) = 0;
    virtual bool shadow_atlas_update_light(RenderingEntity p_atlas, RenderingEntity p_light_intance, float p_coverage, uint64_t p_light_version) = 0;

    virtual int get_directional_light_shadow_size(RenderingEntity p_light_intance) = 0;
    virtual void set_directional_shadow_count(int p_count) = 0;

    /* ENVIRONMENT API */

    virtual RenderingEntity environment_create() = 0;

    virtual void environment_set_background(RenderingEntity p_env, RS::EnvironmentBG p_bg) = 0;
    virtual void environment_set_sky(RenderingEntity p_env, RenderingEntity p_sky) = 0;
    virtual void environment_set_sky_custom_fov(RenderingEntity p_env, float p_scale) = 0;
    virtual void environment_set_sky_orientation(RenderingEntity p_env, const Basis &p_orientation) = 0;
    virtual void environment_set_bg_color(RenderingEntity p_env, const Color &p_color) = 0;
    virtual void environment_set_bg_energy(RenderingEntity p_env, float p_energy) = 0;
    virtual void environment_set_canvas_max_layer(RenderingEntity p_env, int p_max_layer) = 0;
    virtual void environment_set_ambient_light(RenderingEntity p_env, const Color &p_color, float p_energy = 1.0, float p_sky_contribution = 0.0) = 0;
    virtual void environment_set_camera_feed_id(RenderingEntity p_env, int p_camera_feed_id) = 0;

    virtual void environment_set_dof_blur_near(RenderingEntity p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, RS::EnvironmentDOFBlurQuality p_quality) = 0;
    virtual void environment_set_dof_blur_far(RenderingEntity p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, RS::EnvironmentDOFBlurQuality p_quality) = 0;
    virtual void environment_set_glow(RenderingEntity p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, RS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale, bool p_high_quality) = 0;
    virtual void environment_set_fog(RenderingEntity p_env, bool p_enable, float p_begin, float p_end, RenderingEntity p_gradient_texture) = 0;

    virtual void environment_set_ssr(RenderingEntity p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance, bool p_roughness) = 0;
    virtual void environment_set_ssao(RenderingEntity p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, RS::EnvironmentSSAOQuality p_quality, RS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) = 0;

    virtual void environment_set_tonemap(RenderingEntity p_env, RS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale) = 0;

    virtual void environment_set_adjustment(RenderingEntity p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RenderingEntity p_ramp) = 0;

    virtual void environment_set_fog(RenderingEntity p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) = 0;
    virtual void environment_set_fog_depth(RenderingEntity p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) = 0;
    virtual void environment_set_fog_height(RenderingEntity p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) = 0;

    virtual bool is_environment(RenderingEntity p_env) = 0;
    virtual RS::EnvironmentBG environment_get_background(RenderingEntity p_env) = 0;
    virtual int environment_get_canvas_max_layer(RenderingEntity p_env) = 0;

    virtual RenderingEntity light_instance_create(RenderingEntity p_light) = 0;
    virtual void light_instance_set_transform(RenderingEntity p_light_instance, const Transform &p_transform) = 0;
    virtual void light_instance_set_shadow_transform(RenderingEntity p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale = 1.0) = 0;
    virtual void light_instance_mark_visible(RenderingEntity p_light_instance) = 0;
    virtual bool light_instances_can_render_shadow_cube() const { return true; }

    virtual RenderingEntity reflection_atlas_create() = 0;
    virtual void reflection_atlas_set_size(RenderingEntity p_ref_atlas, int p_size) = 0;
    virtual void reflection_atlas_set_subdivision(RenderingEntity p_ref_atlas, int p_subdiv) = 0;

    virtual RenderingEntity reflection_probe_instance_create(RenderingEntity p_probe) = 0;
    virtual void reflection_probe_instance_set_transform(RenderingEntity p_instance, const Transform &p_transform) = 0;
    virtual void reflection_probe_release_atlas_index(RenderingEntity p_instance) = 0;
    virtual bool reflection_probe_instance_needs_redraw(RenderingEntity p_instance) = 0;
    virtual bool reflection_probe_instance_has_reflection(RenderingEntity p_instance) = 0;
    virtual bool reflection_probe_instance_begin_render(RenderingEntity p_instance, RenderingEntity p_reflection_atlas) = 0;
    virtual bool reflection_probe_instance_postprocess_step(RenderingEntity p_instance) = 0;

    virtual RenderingEntity gi_probe_instance_create() = 0;
    virtual void gi_probe_instance_set_light_data(RenderingEntity p_probe, RenderingEntity p_base, RenderingEntity p_data) = 0;
    virtual void gi_probe_instance_set_transform_to_data(RenderingEntity p_probe, const Transform &p_xform) = 0;
    virtual void gi_probe_instance_set_bounds(RenderingEntity p_probe, const Vector3 &p_bounds) = 0;

    virtual void render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, const int p_eye, bool p_cam_ortogonal, Span<RenderingEntity> p_cull_result, RenderingEntity *p_light_cull_result, int p_light_cull_count, RenderingEntity *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RenderingEntity p_environment, RenderingEntity p_shadow_atlas, RenderingEntity p_reflection_atlas, RenderingEntity p_reflection_probe, int p_reflection_probe_pass) = 0;
    virtual void render_shadow(RenderingEntity p_light, RenderingEntity p_shadow_atlas, int p_pass, Span<RenderingEntity> p_cull_result) = 0;

    virtual void set_scene_pass(uint64_t p_pass) = 0;
    virtual void set_debug_draw_mode(RS::ViewportDebugDraw p_debug_draw) = 0;


    virtual ~RasterizerScene();
};

class RasterizerStorage {
public:
    /* TEXTURE API */

    virtual RenderingEntity texture_create() = 0;
    virtual void texture_allocate(RenderingEntity p_texture,
            int p_width,
            int p_height,
            int p_depth_3d,
            Image::Format p_format,
            RS::TextureType p_type,
            uint32_t p_flags = RS::TEXTURE_FLAGS_DEFAULT) = 0;

    virtual void texture_set_data(RenderingEntity p_texture, const Ref<Image> &p_image, int p_level = 0) = 0;

    virtual void texture_set_data_partial(RenderingEntity p_texture,
            const Ref<Image> &p_image,
            int src_x, int src_y,
            int src_w, int src_h,
            int dst_x, int dst_y,
            int p_dst_mip,
            int p_level = 0) = 0;

    virtual Ref<Image> texture_get_data(RenderingEntity p_texture, int p_level = 0) const = 0;
    virtual void texture_set_flags(RenderingEntity p_texture, uint32_t p_flags) = 0;
    virtual uint32_t texture_get_flags(RenderingEntity p_texture) const = 0;
    virtual Image::Format texture_get_format(RenderingEntity p_texture) const = 0;
    virtual RS::TextureType texture_get_type(RenderingEntity p_texture) const = 0;
    virtual uint32_t texture_get_texid(RenderingEntity p_texture) const = 0;
    virtual uint32_t texture_get_width(RenderingEntity p_texture) const = 0;
    virtual uint32_t texture_get_height(RenderingEntity p_texture) const = 0;
    virtual uint32_t texture_get_depth(RenderingEntity p_texture) const = 0;
    virtual void texture_set_size_override(RenderingEntity p_texture, int p_width, int p_height, int p_depth_3d) = 0;
    virtual void texture_bind(RenderingEntity p_texture, uint32_t p_texture_no) = 0;

    virtual void texture_set_path(RenderingEntity p_texture, StringView p_path) = 0;
    virtual const String &texture_get_path(RenderingEntity p_texture) const = 0;

    virtual void texture_set_shrink_all_x2_on_set_data(bool p_enable) = 0;

    virtual void texture_debug_usage(Vector<RenderingServer::TextureInfo> *r_info) = 0;

    virtual RenderingEntity texture_create_radiance_cubemap(RenderingEntity p_source, int p_resolution = -1) const = 0;

    virtual void texture_set_detect_3d_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) = 0;
    virtual void texture_set_detect_srgb_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) = 0;
    virtual void texture_set_detect_normal_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) = 0;

    virtual void textures_keep_original(bool p_enable) = 0;

    virtual void texture_set_proxy(RenderingEntity p_proxy, RenderingEntity p_base) = 0;
    virtual Size2 texture_size_with_proxy(RenderingEntity p_texture) const = 0;
    virtual void texture_set_force_redraw_if_visible(RenderingEntity p_texture, bool p_enable) = 0;

    /* SKY API */

    virtual RenderingEntity sky_create() = 0;
    virtual void sky_set_texture(RenderingEntity p_sky, RenderingEntity p_cube_map, int p_radiance_size) = 0;

    /* SHADER API */

    virtual RenderingEntity shader_create() = 0;

    virtual void shader_set_code(RenderingEntity p_shader, const String &p_code) = 0;
    virtual String shader_get_code(RenderingEntity p_shader) const = 0;
    virtual void shader_get_param_list(RenderingEntity p_shader, Vector<PropertyInfo> *p_param_list) const = 0;

    virtual void shader_set_default_texture_param(RenderingEntity p_shader, const StringName &p_name, RenderingEntity p_texture) = 0;
    virtual RenderingEntity shader_get_default_texture_param(RenderingEntity p_shader, const StringName &p_name) const = 0;

    virtual void shader_add_custom_define(RenderingEntity p_shader, StringView p_define) = 0;
    virtual void shader_get_custom_defines(RenderingEntity p_shader, Vector<StringView> *p_defines) const = 0;
    virtual void shader_remove_custom_define(RenderingEntity p_shader, StringView p_define) = 0;

    virtual void set_shader_async_hidden_forbidden(bool p_forbidden) = 0;
    virtual bool is_shader_async_hidden_forbidden() = 0;
    /* COMMON MATERIAL API */

    virtual RenderingEntity material_create() = 0;

    virtual void material_set_render_priority(RenderingEntity p_material, int priority) = 0;
    virtual void material_set_shader(RenderingEntity p_shader_material, RenderingEntity p_shader) = 0;
    virtual RenderingEntity material_get_shader(RenderingEntity p_shader_material) const = 0;

    virtual void material_set_param(RenderingEntity p_material, const StringName &p_param, const Variant &p_value) = 0;
    virtual Variant material_get_param(RenderingEntity p_material, const StringName &p_param) const = 0;
    virtual Variant material_get_param_default(RenderingEntity p_material, const StringName &p_param) const = 0;

    virtual void material_set_line_width(RenderingEntity p_material, float p_width) = 0;

    virtual void material_set_next_pass(RenderingEntity p_material, RenderingEntity p_next_material) = 0;

    virtual bool material_is_animated(RenderingEntity p_material) = 0;
    virtual bool material_casts_shadows(RenderingEntity p_material) = 0;
    virtual bool material_uses_tangents(RenderingEntity /*p_material*/) { return false; }
    virtual bool material_uses_ensure_correct_normals(RenderingEntity /*p_material*/) { return false; }

    virtual void material_add_instance_owner(RenderingEntity p_material, RenderingEntity p_instance) = 0;
    virtual void material_remove_instance_owner(RenderingEntity p_material, RenderingEntity p_instance) = 0;

    /* MESH API */

    virtual RenderingEntity mesh_create() = 0;

    virtual void mesh_add_surface(RenderingEntity p_mesh, uint32_t p_format, RS::PrimitiveType p_primitive, Span<const uint8_t> p_array, int p_vertex_count, Span<const uint8_t> p_index_array, int p_index_count, const AABB &p_aabb, const
                                  Vector<PoolVector<uint8_t>> &p_blend_shapes = Vector<PoolVector<uint8_t>>(), Span<const AABB> p_bone_aabbs = {}) = 0;

    virtual void mesh_set_blend_shape_count(RenderingEntity p_mesh, int p_amount) = 0;
    virtual int mesh_get_blend_shape_count(RenderingEntity p_mesh) const = 0;

    virtual void mesh_set_blend_shape_mode(RenderingEntity p_mesh, RS::BlendShapeMode p_mode) = 0;
    virtual RS::BlendShapeMode mesh_get_blend_shape_mode(RenderingEntity p_mesh) const = 0;

    virtual void mesh_set_blend_shape_values(RenderingEntity p_mesh, Span<const float> p_values) = 0;
    virtual Vector<float> mesh_get_blend_shape_values(RenderingEntity p_mesh) const = 0;

    virtual void mesh_surface_update_region(RenderingEntity p_mesh, int p_surface, int p_offset, Span<const uint8_t> p_data) = 0;

    virtual void mesh_surface_set_material(RenderingEntity p_mesh, int p_surface, RenderingEntity p_material) = 0;
    virtual RenderingEntity mesh_surface_get_material(RenderingEntity p_mesh, int p_surface) const = 0;

    virtual int mesh_surface_get_array_len(RenderingEntity p_mesh, int p_surface) const = 0;
    virtual int mesh_surface_get_array_index_len(RenderingEntity p_mesh, int p_surface) const = 0;

    virtual PoolVector<uint8_t> mesh_surface_get_array(RenderingEntity p_mesh, int p_surface) const = 0;
    virtual PoolVector<uint8_t> mesh_surface_get_index_array(RenderingEntity p_mesh, int p_surface) const = 0;

    virtual uint32_t mesh_surface_get_format(RenderingEntity p_mesh, int p_surface) const = 0;
    virtual RS::PrimitiveType mesh_surface_get_primitive_type(RenderingEntity p_mesh, int p_surface) const = 0;

    virtual AABB mesh_surface_get_aabb(RenderingEntity p_mesh, int p_surface) const = 0;
    virtual Vector<Vector<uint8_t>> mesh_surface_get_blend_shapes(RenderingEntity p_mesh, int p_surface) const = 0;
    virtual const Vector<AABB> &mesh_surface_get_skeleton_aabb(RenderingEntity p_mesh, int p_surface) const = 0;

    virtual void mesh_remove_surface(RenderingEntity p_mesh, int p_index) = 0;
    virtual int mesh_get_surface_count(RenderingEntity p_mesh) const = 0;

    virtual void mesh_set_custom_aabb(RenderingEntity p_mesh, const AABB &p_aabb) = 0;
    virtual AABB mesh_get_custom_aabb(RenderingEntity p_mesh) const = 0;

    virtual AABB mesh_get_aabb(RenderingEntity p_mesh, RenderingEntity p_skeleton) const = 0;

    virtual void mesh_clear(RenderingEntity p_mesh) = 0;

    /* MULTIMESH API */

    virtual RenderingEntity multimesh_create() = 0;

    virtual void multimesh_allocate(RenderingEntity p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, RS::MultimeshColorFormat p_color_format, RS::MultimeshCustomDataFormat p_data = RS::MULTIMESH_CUSTOM_DATA_NONE) = 0;
    virtual int multimesh_get_instance_count(RenderingEntity p_multimesh) const = 0;

    virtual void multimesh_set_mesh(RenderingEntity p_multimesh, RenderingEntity p_mesh) = 0;
    virtual void multimesh_instance_set_transform(RenderingEntity p_multimesh, int p_index, const Transform &p_transform) = 0;
    virtual void multimesh_instance_set_transform_2d(RenderingEntity p_multimesh, int p_index, const Transform2D &p_transform) = 0;
    virtual void multimesh_instance_set_color(RenderingEntity p_multimesh, int p_index, const Color &p_color) = 0;
    virtual void multimesh_instance_set_custom_data(RenderingEntity p_multimesh, int p_index, const Color &p_color) = 0;

    virtual RenderingEntity multimesh_get_mesh(RenderingEntity p_multimesh) const = 0;

    virtual Transform multimesh_instance_get_transform(RenderingEntity p_multimesh, int p_index) const = 0;
    virtual Transform2D multimesh_instance_get_transform_2d(RenderingEntity p_multimesh, int p_index) const = 0;
    virtual Color multimesh_instance_get_color(RenderingEntity p_multimesh, int p_index) const = 0;
    virtual Color multimesh_instance_get_custom_data(RenderingEntity p_multimesh, int p_index) const = 0;

    virtual void multimesh_set_as_bulk_array(RenderingEntity p_multimesh, Span<const float> p_array) = 0;

    virtual void multimesh_set_visible_instances(RenderingEntity p_multimesh, int p_visible) = 0;
    virtual int multimesh_get_visible_instances(RenderingEntity p_multimesh) const = 0;

    virtual AABB multimesh_get_aabb(RenderingEntity p_multimesh) const = 0;

    /* IMMEDIATE API */

    virtual RenderingEntity immediate_create() = 0;
    virtual void immediate_begin(RenderingEntity p_immediate, RS::PrimitiveType p_rimitive, RenderingEntity p_texture = entt::null) = 0;
    virtual void immediate_vertex(RenderingEntity p_immediate, const Vector3 &p_vertex) = 0;
    virtual void immediate_normal(RenderingEntity p_immediate, const Vector3 &p_normal) = 0;
    virtual void immediate_tangent(RenderingEntity p_immediate, const Plane &p_tangent) = 0;
    virtual void immediate_color(RenderingEntity p_immediate, const Color &p_color) = 0;
    virtual void immediate_uv(RenderingEntity p_immediate, const Vector2 &tex_uv) = 0;
    virtual void immediate_uv2(RenderingEntity p_immediate, const Vector2 &tex_uv) = 0;
    virtual void immediate_end(RenderingEntity p_immediate) = 0;
    virtual void immediate_clear(RenderingEntity p_immediate) = 0;
    virtual void immediate_set_material(RenderingEntity p_immediate, RenderingEntity p_material) = 0;
    virtual RenderingEntity immediate_get_material(RenderingEntity p_immediate) const = 0;
    virtual AABB immediate_get_aabb(RenderingEntity p_immediate) const = 0;

    /* SKELETON API */

    virtual RenderingEntity skeleton_create() = 0;
    virtual void skeleton_allocate(RenderingEntity p_skeleton, int p_bones, bool p_2d_skeleton = false) = 0;
    virtual int skeleton_get_bone_count(RenderingEntity p_skeleton) const = 0;
    virtual void skeleton_bone_set_transform(RenderingEntity p_skeleton, int p_bone, const Transform &p_transform) = 0;
    virtual Transform skeleton_bone_get_transform(RenderingEntity p_skeleton, int p_bone) const = 0;
    virtual void skeleton_bone_set_transform_2d(RenderingEntity p_skeleton, int p_bone, const Transform2D &p_transform) = 0;
    virtual Transform2D skeleton_bone_get_transform_2d(RenderingEntity p_skeleton, int p_bone) const = 0;
    virtual void skeleton_set_base_transform_2d(RenderingEntity p_skeleton, const Transform2D &p_base_transform) = 0;
    virtual uint32_t skeleton_get_revision(RenderingEntity p_skeleton) const = 0;

    /* Light API */

    virtual RenderingEntity light_create(RS::LightType p_type) = 0;

    RenderingEntity directional_light_create() { return light_create(RS::LIGHT_DIRECTIONAL); }
    RenderingEntity omni_light_create() { return light_create(RS::LIGHT_OMNI); }
    RenderingEntity spot_light_create() { return light_create(RS::LIGHT_SPOT); }

    virtual void light_set_color(RenderingEntity p_light, const Color &p_color) = 0;
    virtual void light_set_param(RenderingEntity p_light, RS::LightParam p_param, float p_value) = 0;
    virtual void light_set_shadow(RenderingEntity p_light, bool p_enabled) = 0;
    virtual void light_set_shadow_color(RenderingEntity p_light, const Color &p_color) = 0;
    virtual void light_set_projector(RenderingEntity p_light, RenderingEntity p_texture) = 0;
    virtual void light_set_negative(RenderingEntity p_light, bool p_enable) = 0;
    virtual void light_set_cull_mask(RenderingEntity p_light, uint32_t p_mask) = 0;
    virtual void light_set_reverse_cull_face_mode(RenderingEntity p_light, bool p_enabled) = 0;
    virtual void light_set_use_gi(RenderingEntity p_light, bool p_enable) = 0;
    virtual void light_set_bake_mode(RenderingEntity p_light, RS::LightBakeMode p_bake_mode) = 0;

    virtual void light_omni_set_shadow_mode(RenderingEntity p_light, RS::LightOmniShadowMode p_mode) = 0;
    virtual void light_omni_set_shadow_detail(RenderingEntity p_light, RS::LightOmniShadowDetail p_detail) = 0;

    virtual void light_directional_set_shadow_mode(RenderingEntity p_light, RS::LightDirectionalShadowMode p_mode) = 0;
    virtual void light_directional_set_blend_splits(RenderingEntity p_light, bool p_enable) = 0;
    virtual bool light_directional_get_blend_splits(RenderingEntity p_light) const = 0;
    virtual void light_directional_set_shadow_depth_range_mode(RenderingEntity p_light, RS::LightDirectionalShadowDepthRangeMode p_range_mode) = 0;
    virtual RS::LightDirectionalShadowDepthRangeMode light_directional_get_shadow_depth_range_mode(RenderingEntity p_light) const = 0;

    virtual RS::LightDirectionalShadowMode light_directional_get_shadow_mode(RenderingEntity p_light) = 0;
    virtual RS::LightOmniShadowMode light_omni_get_shadow_mode(RenderingEntity p_light) = 0;

    virtual bool light_has_shadow(RenderingEntity p_light) const = 0;

    virtual RS::LightType light_get_type(RenderingEntity p_light) const = 0;
    virtual AABB light_get_aabb(RenderingEntity p_light) const = 0;
    virtual float light_get_param(RenderingEntity p_light, RS::LightParam p_param) = 0;
    virtual Color light_get_color(RenderingEntity p_light) = 0;
    virtual bool light_get_use_gi(RenderingEntity p_light) = 0;
    virtual RS::LightBakeMode light_get_bake_mode(RenderingEntity p_light) = 0;
    virtual uint64_t light_get_version(RenderingEntity p_light) const = 0;

    /* PROBE API */

    virtual RenderingEntity reflection_probe_create() = 0;

    virtual void reflection_probe_set_update_mode(RenderingEntity p_probe, RS::ReflectionProbeUpdateMode p_mode) = 0;
    virtual void reflection_probe_set_resolution(RenderingEntity p_probe, int p_resolution) = 0;
    virtual void reflection_probe_set_intensity(RenderingEntity p_probe, float p_intensity) = 0;
    virtual void reflection_probe_set_interior_ambient(RenderingEntity p_probe, const Color &p_ambient) = 0;
    virtual void reflection_probe_set_interior_ambient_energy(RenderingEntity p_probe, float p_energy) = 0;
    virtual void reflection_probe_set_interior_ambient_probe_contribution(RenderingEntity p_probe, float p_contrib) = 0;
    virtual void reflection_probe_set_max_distance(RenderingEntity p_probe, float p_distance) = 0;
    virtual void reflection_probe_set_extents(RenderingEntity p_probe, const Vector3 &p_extents) = 0;
    virtual void reflection_probe_set_origin_offset(RenderingEntity p_probe, const Vector3 &p_offset) = 0;
    virtual void reflection_probe_set_as_interior(RenderingEntity p_probe, bool p_enable) = 0;
    virtual void reflection_probe_set_enable_box_projection(RenderingEntity p_probe, bool p_enable) = 0;
    virtual void reflection_probe_set_enable_shadows(RenderingEntity p_probe, bool p_enable) = 0;
    virtual void reflection_probe_set_cull_mask(RenderingEntity p_probe, uint32_t p_layers) = 0;

    virtual AABB reflection_probe_get_aabb(RenderingEntity p_probe) const = 0;
    virtual RS::ReflectionProbeUpdateMode reflection_probe_get_update_mode(RenderingEntity p_probe) const = 0;
    virtual uint32_t reflection_probe_get_cull_mask(RenderingEntity p_probe) const = 0;
    virtual Vector3 reflection_probe_get_extents(RenderingEntity p_probe) const = 0;
    virtual Vector3 reflection_probe_get_origin_offset(RenderingEntity p_probe) const = 0;
    virtual float reflection_probe_get_origin_max_distance(RenderingEntity p_probe) const = 0;
    virtual bool reflection_probe_renders_shadows(RenderingEntity p_probe) const = 0;

    virtual void instance_add_skeleton(RenderingEntity p_skeleton, RenderingEntity p_instance) = 0;
    virtual void instance_remove_skeleton(RenderingEntity p_skeleton, RenderingEntity p_instance) = 0;

    virtual void instance_add_dependency(RenderingEntity p_base, RenderingEntity p_instance) = 0;
    virtual void instance_remove_dependency(RenderingEntity p_base, RenderingEntity p_instance) = 0;

    /* GI PROBE API */

    virtual RenderingEntity gi_probe_create() = 0;

    virtual void gi_probe_set_bounds(RenderingEntity p_probe, const AABB &p_bounds) = 0;
    virtual AABB gi_probe_get_bounds(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_cell_size(RenderingEntity p_probe, float p_range) = 0;
    virtual float gi_probe_get_cell_size(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_to_cell_xform(RenderingEntity p_probe, const Transform &p_xform) = 0;
    virtual Transform gi_probe_get_to_cell_xform(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_dynamic_data(RenderingEntity p_probe, const PoolVector<int> &p_data) = 0;
    virtual PoolVector<int> gi_probe_get_dynamic_data(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_dynamic_range(RenderingEntity p_probe, int p_range) = 0;
    virtual int gi_probe_get_dynamic_range(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_energy(RenderingEntity p_probe, float p_range) = 0;
    virtual float gi_probe_get_energy(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_bias(RenderingEntity p_probe, float p_range) = 0;
    virtual float gi_probe_get_bias(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_normal_bias(RenderingEntity p_probe, float p_range) = 0;
    virtual float gi_probe_get_normal_bias(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_propagation(RenderingEntity p_probe, float p_range) = 0;
    virtual float gi_probe_get_propagation(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_interior(RenderingEntity p_probe, bool p_enable) = 0;
    virtual bool gi_probe_is_interior(RenderingEntity p_probe) const = 0;

    virtual uint32_t gi_probe_get_version(RenderingEntity p_probe) = 0;

    virtual RenderingEntity gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth) = 0;
    virtual void gi_probe_dynamic_data_update(RenderingEntity p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data) = 0;

    /* LIGHTMAP CAPTURE */

    struct LightmapCaptureOctree {

        enum {
            CHILD_EMPTY = 0xFFFFFFFF
        };

        uint16_t light[6][3]; //anisotropic light
        float alpha;
        uint32_t children[8];
    };

    virtual RenderingEntity lightmap_capture_create() = 0;
    virtual void lightmap_capture_set_bounds(RenderingEntity p_capture, const AABB &p_bounds) = 0;
    virtual AABB lightmap_capture_get_bounds(RenderingEntity p_capture) const = 0;
    virtual void lightmap_capture_set_octree(RenderingEntity p_capture, const PoolVector<uint8_t> &p_octree) = 0;
    virtual PoolVector<uint8_t> lightmap_capture_get_octree(RenderingEntity p_capture) const = 0;
    virtual void lightmap_capture_set_octree_cell_transform(RenderingEntity p_capture, const Transform &p_xform) = 0;
    virtual Transform lightmap_capture_get_octree_cell_transform(RenderingEntity p_capture) const = 0;
    virtual void lightmap_capture_set_octree_cell_subdiv(RenderingEntity p_capture, int p_subdiv) = 0;
    virtual int lightmap_capture_get_octree_cell_subdiv(RenderingEntity p_capture) const = 0;
    virtual void lightmap_capture_set_energy(RenderingEntity p_capture, float p_energy) = 0;
    virtual float lightmap_capture_get_energy(RenderingEntity p_capture) const = 0;
    virtual void lightmap_capture_set_interior(RenderingEntity p_capture, bool p_interior) = 0;
    virtual bool lightmap_capture_is_interior(RenderingEntity p_capture) const = 0;
    virtual const PoolVector<LightmapCaptureOctree> *lightmap_capture_get_octree_ptr(RenderingEntity p_capture) const = 0;

    /* PARTICLES */

    virtual RenderingEntity particles_create() = 0;

    virtual void particles_set_emitting(RenderingEntity p_particles, bool p_emitting) = 0;
    virtual bool particles_get_emitting(RenderingEntity p_particles) = 0;

    virtual void particles_set_amount(RenderingEntity p_particles, int p_amount) = 0;
    virtual void particles_set_lifetime(RenderingEntity p_particles, float p_lifetime) = 0;
    virtual void particles_set_one_shot(RenderingEntity p_particles, bool p_one_shot) = 0;
    virtual void particles_set_pre_process_time(RenderingEntity p_particles, float p_time) = 0;
    virtual void particles_set_explosiveness_ratio(RenderingEntity p_particles, float p_ratio) = 0;
    virtual void particles_set_randomness_ratio(RenderingEntity p_particles, float p_ratio) = 0;
    virtual void particles_set_custom_aabb(RenderingEntity p_particles, const AABB &p_aabb) = 0;
    virtual void particles_set_speed_scale(RenderingEntity p_particles, float p_scale) = 0;
    virtual void particles_set_use_local_coordinates(RenderingEntity p_particles, bool p_enable) = 0;
    virtual void particles_set_process_material(RenderingEntity p_particles, RenderingEntity p_material) = 0;
    virtual void particles_set_fixed_fps(RenderingEntity p_particles, int p_fps) = 0;
    virtual void particles_set_fractional_delta(RenderingEntity p_particles, bool p_enable) = 0;
    virtual void particles_restart(RenderingEntity p_particles) = 0;

    virtual bool particles_is_inactive(RenderingEntity p_particles) const = 0;

    virtual void particles_set_draw_order(RenderingEntity p_particles, RS::ParticlesDrawOrder p_order) = 0;

    virtual void particles_set_draw_passes(RenderingEntity p_particles, int p_count) = 0;
    virtual void particles_set_draw_pass_mesh(RenderingEntity p_particles, int p_pass, RenderingEntity p_mesh) = 0;

    virtual void particles_request_process(RenderingEntity p_particles) = 0;
    virtual AABB particles_get_current_aabb(RenderingEntity p_particles) = 0;
    virtual AABB particles_get_aabb(RenderingEntity p_particles) const = 0;

    virtual void particles_set_emission_transform(RenderingEntity p_particles, const Transform &p_transform) = 0;

    virtual int particles_get_draw_passes(RenderingEntity p_particles) const = 0;
    virtual RenderingEntity particles_get_draw_pass_mesh(RenderingEntity p_particles, int p_pass) const = 0;

    /* RENDER TARGET */

    virtual RenderingEntity render_target_create() = 0;
    virtual void render_target_set_size(RenderingEntity p_render_target, int p_width, int p_height) = 0;
    virtual RenderingEntity render_target_get_texture(RenderingEntity p_render_target) const = 0;
    virtual uint32_t render_target_get_depth_texture_id(RenderingEntity p_render_target) const = 0;
    virtual void render_target_set_external_texture(RenderingEntity p_render_target, unsigned int p_texture_id, unsigned int p_depth_id) = 0;
    virtual void render_target_set_flag(RenderingEntity p_render_target, RS::RenderTargetFlags p_flag, bool p_value) = 0;
    virtual bool render_target_was_used(RenderingEntity p_render_target) = 0;
    virtual void render_target_clear_used(RenderingEntity p_render_target) = 0;
    virtual void render_target_set_msaa(RenderingEntity p_render_target, RS::ViewportMSAA p_msaa) = 0;
    virtual void render_target_set_use_fxaa(RenderingEntity p_render_target, bool p_fxaa) = 0;
    virtual void render_target_set_use_debanding(RenderingEntity p_render_target, bool p_debanding) = 0;
    virtual void render_target_set_sharpen_intensity(RenderingEntity p_render_target, float p_intensity) = 0;

    /* CANVAS SHADOW */

    virtual RenderingEntity canvas_light_shadow_buffer_create(int p_width) = 0;

    /* LIGHT SHADOW MAPPING */

    virtual RenderingEntity canvas_light_occluder_create() = 0;
    virtual void canvas_light_occluder_set_polylines(RenderingEntity p_occluder, Span<const Vector2> p_lines) = 0;

    virtual RS::InstanceType get_base_type(RenderingEntity p_rid) const = 0;
    virtual bool free(RenderingEntity p_rid) = 0;

    virtual bool has_os_feature(const StringName &p_feature) const = 0;

    virtual void update_dirty_resources() = 0;

    virtual void set_debug_generate_wireframes(bool p_generate) = 0;

    virtual void render_info_begin_capture() = 0;
    virtual void render_info_end_capture() = 0;
    virtual int get_captured_render_info(RS::RenderInfo p_info) = 0;

    virtual uint64_t get_render_info(RS::RenderInfo p_info) = 0;
    virtual const char *get_video_adapter_name() const = 0;
    virtual const char *get_video_adapter_vendor() const = 0;


    static RasterizerStorage *base_singleton;
    RasterizerStorage();
    virtual ~RasterizerStorage() {}
};

struct RasterizerCanvasLight3DComponent {
    bool enabled=true;
    Color color {1,1,1};
    Transform2D xform;
    float height=0;
    float energy=1.0f;
    float scale=1.0f;
    int z_min=-1024;
    int z_max=1024;
    int layer_min=0;
    int layer_max=0;
    int item_mask=1;
    int item_shadow_mask=1;
    Vector2 texture_offset;
    MoveOnlyEntityHandle texture;
    MoveOnlyEntityHandle self;
    MoveOnlyEntityHandle canvas;
    MoveOnlyEntityHandle shadow_buffer;
    Color shadow_color {0,0,0,0};
    float shadow_gradient_length = 0.0f;
    float shadow_smooth = 0.0f;
    int shadow_buffer_size = 2048;

    CameraMatrix shadow_matrix_cache;
    Rect2 rect_cache;
    Transform2D xform_cache;
    void *texture_cache = nullptr; // implementation dependent
    float radius_cache; //used for shadow far plane

    Transform2D light_shader_xform;
    Vector2 light_shader_pos;

    MoveOnlyEntityHandle light_internal;
    RS::CanvasLightMode mode = RS::CANVAS_LIGHT_MODE_ADD;
    RS::CanvasLightShadowFilter shadow_filter = RS::CANVAS_LIGHT_FILTER_NONE;

   void release_resources();

    RasterizerCanvasLight3DComponent &operator=(const RasterizerCanvasLight3DComponent &) = delete;
    RasterizerCanvasLight3DComponent(const RasterizerCanvasLight3DComponent &) = delete;

    RasterizerCanvasLight3DComponent &operator=(RasterizerCanvasLight3DComponent &&from);
    RasterizerCanvasLight3DComponent(RasterizerCanvasLight3DComponent &&) = default;

    RasterizerCanvasLight3DComponent()  = default;
    ~RasterizerCanvasLight3DComponent() {
        release_resources();
    }
};
struct RasterizerCanvasLightOccluderInstanceComponent {

    Rect2 aabb_cache;
    Transform2D xform;
    Transform2D xform_cache;
    RenderingEntity next = entt::null;
    MoveOnlyEntityHandle self;
    MoveOnlyEntityHandle canvas;
    MoveOnlyEntityHandle polygon;
    RenderingEntity polygon_buffer=entt::null; // not used in destructor
    int light_mask = 1;
    RS::CanvasOccluderPolygonCullMode cull_cache = RS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED;
    bool enabled = true;

    void release_resources();

    RasterizerCanvasLightOccluderInstanceComponent & operator=(const RasterizerCanvasLightOccluderInstanceComponent &) = delete;
    RasterizerCanvasLightOccluderInstanceComponent(const RasterizerCanvasLightOccluderInstanceComponent &) = delete;

    RasterizerCanvasLightOccluderInstanceComponent & operator=(RasterizerCanvasLightOccluderInstanceComponent &&from);
    RasterizerCanvasLightOccluderInstanceComponent(RasterizerCanvasLightOccluderInstanceComponent &&f) {
        *this = eastl::move(f);
    }

    RasterizerCanvasLightOccluderInstanceComponent() = default;
    ~RasterizerCanvasLightOccluderInstanceComponent() {
        release_resources();
    }
};

class GODOT_EXPORT RasterizerCanvas {
public:
    enum CanvasRectFlags {

        CANVAS_RECT_REGION = 1,
        CANVAS_RECT_TILE = 2,
        CANVAS_RECT_FLIP_H = 4,
        CANVAS_RECT_FLIP_V = 8,
        CANVAS_RECT_TRANSPOSE = 16,
        CANVAS_RECT_CLIP_UV = 32
    };


    virtual RenderingEntity light_internal_create() = 0;
    virtual void light_internal_update(RenderingEntity p_rid, RasterizerCanvasLight3DComponent *p_light) = 0;
    virtual void light_internal_free(RenderingEntity p_rid) = 0;

    struct Item  {

#pragma pack(push,1)
        struct Command {

            enum Type : uint8_t {

                TYPE_LINE,
                TYPE_POLYLINE,
                TYPE_RECT,
                TYPE_NINEPATCH,
                TYPE_PRIMITIVE,
                TYPE_POLYGON,
                TYPE_MESH,
                TYPE_MULTIMESH,
                TYPE_PARTICLES,
                TYPE_CIRCLE,
                TYPE_TRANSFORM,
                TYPE_CLIP_IGNORE,
            };

            Type type;
            virtual ~Command() {}
        };
#pragma pack(pop)

        struct CommandLine : public Command {

            Point2 from, to;
            Color color;
            float width;
            bool antialiased;
            CommandLine() { type = TYPE_LINE; }
        };
        struct CommandPolyLine : public Command {

            Vector<Point2> triangles;
            Vector<Color> triangle_colors;
            Vector<Point2> lines;
            Vector<Color> line_colors;
            bool antialiased=false;
            bool multiline=false;
            CommandPolyLine() {
                type = TYPE_POLYLINE;
            }
        };

        struct CommandRect : public Command {

            Rect2 rect;
            RenderingEntity texture=entt::null;
            RenderingEntity normal_map=entt::null;
            Color modulate;
            Rect2 source;
            uint8_t flags=0;

            CommandRect() {
                type = TYPE_RECT;
            }
        };

        struct CommandNinePatch : public Command {

            Rect2 rect;
            Rect2 source;
            Color color;
            RenderingEntity texture=entt::null;
            RenderingEntity normal_map=entt::null;
            float margin[4];
            bool draw_center;
            RS::NinePatchAxisMode axis_x;
            RS::NinePatchAxisMode axis_y;
            CommandNinePatch() {
                draw_center = true;
                type = TYPE_NINEPATCH;
            }
        };

        struct CommandPrimitive : public Command {

            Vector<Point2> points;
            PoolVector<Point2> uvs;
            Vector<Color> colors;
            RenderingEntity texture=entt::null;
            RenderingEntity normal_map=entt::null;
            float width;

            CommandPrimitive() {
                type = TYPE_PRIMITIVE;
                width = 1;
            }
        };

        struct CommandPolygon : public Command {

            Vector<int> indices;
            Vector<Point2> points;
            Vector<Point2> uvs;
            Vector<Color> colors;
            PoolVector<int> bones;
            PoolVector<float> weights;
            RenderingEntity texture=entt::null;
            RenderingEntity normal_map=entt::null;
            int count;
            bool antialiased;
            bool antialiasing_use_indices;

            CommandPolygon() {
                type = TYPE_POLYGON;
                count = 0;
            }
        };

        struct CommandMesh : public Command {

            RenderingEntity mesh=entt::null;
            RenderingEntity texture=entt::null;
            RenderingEntity normal_map=entt::null;
            Transform2D transform;
            Color modulate;
            CommandMesh() { type = TYPE_MESH; }
        };

        struct CommandMultiMesh : public Command {

            RenderingEntity multimesh=entt::null;
            RenderingEntity texture=entt::null;
            RenderingEntity normal_map=entt::null;
            CommandMultiMesh() { type = TYPE_MULTIMESH; }
        };

        struct CommandParticles : public Command {

            RenderingEntity particles=entt::null;
            RenderingEntity texture=entt::null;
            RenderingEntity normal_map=entt::null;
            CommandParticles() { type = TYPE_PARTICLES; }
        };

        struct CommandCircle : public Command {

            Point2 pos;
            float radius;
            Color color;
            CommandCircle() { type = TYPE_CIRCLE; }
        };

        struct CommandTransform : public Command {

            Transform2D xform;
            CommandTransform() { type = TYPE_TRANSFORM; }
        };

        struct CommandClipIgnore : public Command {

            bool ignore=false;
            CommandClipIgnore() {
                type = TYPE_CLIP_IGNORE;
            }
        };

        struct ViewportRender {
            Rect2 rect;
            RenderingServer *owner;
            void *udata;
        };
        struct CopyBackBuffer {
            Rect2 rect;
            Rect2 screen_rect;
            bool full;
        };
        Transform2D xform;
        Transform2D final_transform;
        Rect2 final_clip_rect;
        Rect2 global_rect_cache;
        Color final_modulate=Color(1, 1, 1, 1);
        Vector<Command *> commands;
        mutable Rect2 rect;
        MoveOnlyEntityHandle material;
        MoveOnlyEntityHandle skeleton;
        MoveOnlyPointer<Item> final_clip_owner;
        MoveOnlyPointer<Item> material_owner;
        MoveOnlyPointer<ViewportRender> vp_render;
        MoveOnlyPointer<CopyBackBuffer> copy_back_buffer;
        int light_mask=1;
        mutable uint32_t skeleton_revision=0;
        bool clip=false;
        bool visible=true;
        bool behind=false;
        bool update_when_visible=false;
        bool distance_field = false;
        bool light_masked = false;
        mutable bool custom_rect=true;
        mutable bool rect_dirty=true;

        const Rect2 &get_rect() const;

        void clear() {
            for (auto *ptr : commands)
                memdelete(ptr);
            commands.clear();
            clip = false;
            rect_dirty = true;
            final_clip_owner = nullptr;
            material_owner = nullptr;
            light_masked = false;
            skeleton_revision = 0;
        }
        Item(const Item &) = delete;
        Item &operator=(const Item &) = delete;

        Item(Item &&) = default;
        Item &operator=(Item &&oth) {
            clear();
            xform = eastl::move(oth.xform);
            final_transform = eastl::move(oth.final_transform);
            final_clip_rect = eastl::move(oth.final_clip_rect);
            global_rect_cache = eastl::move(oth.global_rect_cache);
            final_modulate = eastl::move(oth.final_modulate);
            commands = eastl::move(oth.commands);
            rect = eastl::move(oth.rect);
            material = eastl::move(oth.material);
            skeleton = eastl::move(oth.skeleton);
            final_clip_owner = eastl::move(oth.final_clip_owner);
            material_owner = eastl::move(oth.material_owner);
            vp_render = eastl::move(oth.vp_render);
            copy_back_buffer = eastl::move(oth.copy_back_buffer);
            light_mask = eastl::move(oth.light_mask);
            skeleton_revision = eastl::move(oth.skeleton_revision);
            clip = eastl::move(oth.clip);
            visible = eastl::move(oth.visible);
            behind = eastl::move(oth.behind);
            update_when_visible = eastl::move(oth.update_when_visible);
            distance_field = eastl::move(oth.distance_field);
            light_masked = eastl::move(oth.light_masked);
            custom_rect = eastl::move(oth.custom_rect);
            rect_dirty = eastl::move(oth.rect_dirty);
            return *this;
        }

        Item() = default;
        ~Item() {
            clear();
            memdelete(copy_back_buffer.value);
        }
    };

    virtual void canvas_begin() = 0;
    virtual void canvas_end() = 0;

    virtual void canvas_render_items_begin(const Color &p_modulate, Span<RasterizerCanvasLight3DComponent *> p_light, const Transform2D &p_base_transform) {}
    virtual void canvas_render_items_end() {}

    virtual void canvas_render_items(Dequeue<Item *> &p_item_list, int p_z, const Color &p_modulate,
            Span<RasterizerCanvasLight3DComponent *> p_light, const Transform2D &p_base_transform) = 0;
    virtual void canvas_debug_viewport_shadows(Span<RasterizerCanvasLight3DComponent *> p_lights_with_shadow) = 0;

    virtual void canvas_light_shadow_buffer_update(RenderingEntity p_buffer, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, RenderingEntity p_occluders, CameraMatrix *p_xform_cache) = 0;

    virtual void reset_canvas() = 0;

    virtual void draw_window_margins(int *p_margins, RenderingEntity *p_margin_textures) = 0;

    virtual ~RasterizerCanvas() {}
};

class Rasterizer {
protected:
    static Rasterizer *(*_create_func)();

public:
    static Rasterizer *create();

    virtual RasterizerStorage *get_storage() = 0;
    virtual RasterizerCanvas *get_canvas() = 0;
    virtual RasterizerScene *get_scene() = 0;

    virtual void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) = 0;
    virtual void set_shader_time_scale(float p_scale) = 0;

    virtual void initialize() = 0;
    virtual void begin_frame(double frame_step) = 0;
    virtual void set_current_render_target(RenderingEntity p_render_target) = 0;
    virtual void restore_render_target(bool p_3d) = 0;
    virtual void clear_render_target(const Color &p_color) = 0;
    virtual void blit_render_target_to_screen(RenderingEntity p_render_target, const Rect2 &p_screen_rect, int p_screen = 0) = 0;
    virtual void output_lens_distorted_to_screen(RenderingEntity p_render_target, const Rect2 &p_screen_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample) = 0;
    virtual void end_frame(bool p_swap_buffers) = 0;
    virtual void finalize() = 0;

    virtual ~Rasterizer() {}
};
