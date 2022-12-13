/*************************************************************************/
/*  rasterizer_dummy.h                                                   */
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

#include "core/math/camera_matrix.h"
#include "core/self_list.h"
#include "scene/resources/mesh.h"
#include "servers/rendering/rasterizer.h"
#include "servers/rendering_server.h"

class RasterizerSceneDummy : public RasterizerScene {
public:
    /* SHADOW ATLAS API */

    RenderingEntity shadow_atlas_create() { return entt::null; }
    void shadow_atlas_set_size(RenderingEntity p_atlas, int p_size) {}
    void shadow_atlas_set_quadrant_subdivision(RenderingEntity p_atlas, int p_quadrant, int p_subdivision) {}
    bool shadow_atlas_update_light(RenderingEntity p_atlas, RenderingEntity p_light_intance, float p_coverage, uint64_t p_light_version) { return false; }

    int get_directional_light_shadow_size(RenderingEntity p_light_intance) { return 0; }
    void set_directional_shadow_count(int p_count) {}

    /* ENVIRONMENT API */

    RenderingEntity environment_create() { return entt::null; }

    void environment_set_background(RenderingEntity p_env, RS::EnvironmentBG p_bg) {}
    void environment_set_sky(RenderingEntity p_env, RenderingEntity p_sky) {}
    void environment_set_sky_custom_fov(RenderingEntity p_env, float p_scale) {}
    void environment_set_sky_orientation(RenderingEntity p_env, const Basis &p_orientation) {}
    void environment_set_bg_color(RenderingEntity p_env, const Color &p_color) {}
    void environment_set_bg_energy(RenderingEntity p_env, float p_energy) {}
    void environment_set_canvas_max_layer(RenderingEntity p_env, int p_max_layer) {}
    void environment_set_ambient_light(RenderingEntity p_env, const Color &p_color, float p_energy = 1.0, float p_sky_contribution = 0.0) {}
    void environment_set_camera_feed_id(RenderingEntity p_env, int p_camera_feed_id){};

    void environment_set_dof_blur_near(RenderingEntity p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, RS::EnvironmentDOFBlurQuality p_quality) {}
    void environment_set_dof_blur_far(RenderingEntity p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, RS::EnvironmentDOFBlurQuality p_quality) {}
    void environment_set_glow(RenderingEntity p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, RS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale) {}

    void environment_set_fog(RenderingEntity p_env, bool p_enable, float p_begin, float p_end, RenderingEntity p_gradient_texture) {}

    void environment_set_ssr(RenderingEntity p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance, bool p_roughness) {}
    void environment_set_ssao(RenderingEntity p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, RS::EnvironmentSSAOQuality p_quality, RS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) {}

    void environment_set_tonemap(RenderingEntity p_env, RS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale) {}

    void environment_set_adjustment(RenderingEntity p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RenderingEntity p_ramp) {}

    void environment_set_fog(RenderingEntity p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) {}
    void environment_set_fog_depth(RenderingEntity p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) {}
    void environment_set_fog_height(RenderingEntity p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) {}

    bool is_environment(RenderingEntity p_env) { return false; }
    RS::EnvironmentBG environment_get_background(RenderingEntity p_env) { return RS::ENV_BG_KEEP; }
    int environment_get_canvas_max_layer(RenderingEntity p_env) { return 0; }

    RenderingEntity light_instance_create(RenderingEntity p_light) { return entt::null; }
    void light_instance_set_transform(RenderingEntity p_light_instance, const Transform &p_transform) {}
    void light_instance_set_shadow_transform(RenderingEntity p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale = 1.0) {}
    void light_instance_mark_visible(RenderingEntity p_light_instance) {}

    RenderingEntity reflection_atlas_create() { return entt::null; }
    void reflection_atlas_set_size(RenderingEntity p_ref_atlas, int p_size) {}
    void reflection_atlas_set_subdivision(RenderingEntity p_ref_atlas, int p_subdiv) {}

    RenderingEntity reflection_probe_instance_create(RenderingEntity p_probe) { return entt::null; }
    void reflection_probe_instance_set_transform(RenderingEntity p_instance, const Transform &p_transform) {}
    void reflection_probe_release_atlas_index(RenderingEntity p_instance) {}
    bool reflection_probe_instance_needs_redraw(RenderingEntity p_instance) { return false; }
    bool reflection_probe_instance_has_reflection(RenderingEntity p_instance) { return false; }
    bool reflection_probe_instance_begin_render(RenderingEntity p_instance, RenderingEntity p_reflection_atlas) { return false; }
    bool reflection_probe_instance_postprocess_step(RenderingEntity p_instance) { return true; }

    RenderingEntity gi_probe_instance_create() { return entt::null; }
    void gi_probe_instance_set_light_data(RenderingEntity p_probe, RenderingEntity p_base, RenderingEntity p_data) {}
    void gi_probe_instance_set_transform_to_data(RenderingEntity p_probe, const Transform &p_xform) {}
    void gi_probe_instance_set_bounds(RenderingEntity p_probe, const Vector3 &p_bounds) {}

    void render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_ortogonal, InstanceBase **p_cull_result, int p_cull_count, RenderingEntity *p_light_cull_result, int p_light_cull_count, RenderingEntity *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RenderingEntity p_environment, RenderingEntity p_shadow_atlas, RenderingEntity p_reflection_atlas, RenderingEntity p_reflection_probe, int p_reflection_probe_pass) {}
    void render_shadow(RenderingEntity p_light, RenderingEntity p_shadow_atlas, int p_pass, InstanceBase **p_cull_result, int p_cull_count) {}

    void set_scene_pass(uint64_t p_pass) {}
    void set_debug_draw_mode(RS::ViewportDebugDraw p_debug_draw) {}

    bool free(RenderingEntity p_rid) { return true; }

    RasterizerSceneDummy() {}
    ~RasterizerSceneDummy() {}
};

class RasterizerStorageDummy : public RasterizerStorage {
public:
    /* TEXTURE API */
    struct DummyTexture : public RID_Data {
        int width;
        int height;
        uint32_t flags;
        Image::Format format;
        Ref<Image> image;
        String path;
    };

    struct DummySurface {
        uint32_t format;
        RS::PrimitiveType primitive;
        PoolVector<uint8_t> array;
        int vertex_count;
        PoolVector<uint8_t> index_array;
        int index_count;
        AABB aabb;
        Vector<PoolVector<uint8_t> > blend_shapes;
        Vector<AABB> bone_aabbs;
    };

    struct DummyMesh : public RID_Data {
        Vector<DummySurface> surfaces;
        int blend_shape_count;
        RS::BlendShapeMode blend_shape_mode;
    };

    mutable RID_Owner<DummyTexture> texture_owner;
    mutable RID_Owner<DummyMesh> mesh_owner;

    RenderingEntity texture_create() {

        DummyTexture *texture = memnew(DummyTexture);
        ERR_FAIL_COND_V(!texture, entt::null)
        return texture_owner.make_rid(texture);
    }

    void texture_allocate(RenderingEntity p_texture, int p_width, int p_height, int p_depth_3d, Image::Format p_format, RenderingServerEnums::TextureType p_type = RS::TEXTURE_TYPE_2D, uint32_t p_flags = RS::TEXTURE_FLAGS_DEFAULT) {
        DummyTexture *t = texture_owner.getornull(p_texture);
        ERR_FAIL_COND(!t);
        t->width = p_width;
        t->height = p_height;
        t->flags = p_flags;
        t->format = p_format;
        t->image = make_ref_counted<Image>();
        t->image->create(p_width, p_height, false, p_format);
    }
    void texture_set_data(RenderingEntity p_texture, const Ref<Image> &p_image, int p_level) {
        DummyTexture *t = texture_owner.getornull(p_texture);
        ERR_FAIL_COND(!t);
        t->width = p_image->get_width();
        t->height = p_image->get_height();
        t->format = p_image->get_format();
        t->image->create(t->width, t->height, false, t->format, p_image->get_data());
    }

    void texture_set_data_partial(RenderingEntity p_texture, const Ref<Image> &p_image, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, int p_dst_mip, int p_level) {
        DummyTexture *t = texture_owner.get(p_texture);

        ERR_FAIL_COND(!t);
        ERR_FAIL_COND_MSG(not p_image, "It's not a reference to a valid Image object.");
        ERR_FAIL_COND(t->format != p_image->get_format());
        ERR_FAIL_COND(src_w <= 0 || src_h <= 0);
        ERR_FAIL_COND(src_x < 0 || src_y < 0 || src_x + src_w > p_image->get_width() || src_y + src_h > p_image->get_height());
        ERR_FAIL_COND(dst_x < 0 || dst_y < 0 || dst_x + src_w > t->width || dst_y + src_h > t->height);

        t->image->blit_rect(p_image, Rect2(src_x, src_y, src_w, src_h), Vector2(dst_x, dst_y));
    }

    Ref<Image> texture_get_data(RenderingEntity p_texture, int p_level) const {
        DummyTexture *t = texture_owner.getornull(p_texture);
        ERR_FAIL_COND_V(!t, Ref<Image>())
        return t->image;
    }
    void texture_set_flags(RenderingEntity p_texture, uint32_t p_flags) {
        DummyTexture *t = texture_owner.getornull(p_texture);
        ERR_FAIL_COND(!t);
        t->flags = p_flags;
    }
    uint32_t texture_get_flags(RenderingEntity p_texture) const {
        DummyTexture *t = texture_owner.getornull(p_texture);
        ERR_FAIL_COND_V(!t, 0)
        return t->flags;
    }
    Image::Format texture_get_format(RenderingEntity p_texture) const {
        DummyTexture *t = texture_owner.getornull(p_texture);
        ERR_FAIL_COND_V(!t, ImageData::FORMAT_RGB8)
        return t->format;
    }

    RenderingServer::TextureType texture_get_type(RenderingEntity p_texture) const { return RS::TEXTURE_TYPE_2D; }
    uint32_t texture_get_texid(RenderingEntity p_texture) const { return 0; }
    uint32_t texture_get_width(RenderingEntity p_texture) const { return 0; }
    uint32_t texture_get_height(RenderingEntity p_texture) const { return 0; }
    uint32_t texture_get_depth(RenderingEntity p_texture) const { return 0; }
    void texture_set_size_override(RenderingEntity p_texture, int p_width, int p_height, int p_depth_3d) {}
    void texture_bind(RenderingEntity p_texture, uint32_t p_texture_no) {}

    void texture_set_path(RenderingEntity p_texture, StringView p_path) {
        DummyTexture *t = texture_owner.getornull(p_texture);
        ERR_FAIL_COND(!t);
        t->path = p_path;
    }
    const String &texture_get_path(RenderingEntity p_texture) const {
        DummyTexture *t = texture_owner.getornull(p_texture);
        ERR_FAIL_COND_V(!t, null_string)
        return t->path;
    }

    void texture_set_shrink_all_x2_on_set_data(bool p_enable) {}

    void texture_debug_usage(List<RS::TextureInfo> *r_info) {}

    RenderingEntity texture_create_radiance_cubemap(RenderingEntity p_source, int p_resolution = -1) const { return entt::null; }

    void texture_set_detect_3d_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) {}
    void texture_set_detect_srgb_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) {}
    void texture_set_detect_normal_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) {}

    void textures_keep_original(bool p_enable) {}

    void texture_set_proxy(RenderingEntity p_proxy, RenderingEntity p_base) {}
    virtual Size2 texture_size_with_proxy(RenderingEntity p_texture) const { return Size2(); }
    void texture_set_force_redraw_if_visible(RenderingEntity p_texture, bool p_enable) {}

    /* SKY API */

    RenderingEntity sky_create() { return entt::null; }
    void sky_set_texture(RenderingEntity p_sky, RenderingEntity p_cube_map, int p_radiance_size) {}

    /* SHADER API */

    RenderingEntity shader_create() { return entt::null; }

    void shader_set_code(RenderingEntity p_shader, const String &p_code) {}
    String shader_get_code(RenderingEntity p_shader) const { return String(); }
    void shader_get_param_list(RenderingEntity p_shader, Vector<PropertyInfo> *p_param_list) const {}

    void shader_set_default_texture_param(RenderingEntity p_shader, const StringName &p_name, RenderingEntity p_texture) {}
    RenderingEntity shader_get_default_texture_param(RenderingEntity p_shader, const StringName &p_name) const { return entt::null; }

    void shader_add_custom_define(RenderingEntity p_shader, StringView p_define) {}
    void shader_get_custom_defines(RenderingEntity p_shader, Span<const StringView> p_defines) const {}
    void shader_remove_custom_define(RenderingEntity p_shader, StringView p_define) {}

    /* COMMON MATERIAL API */

    RenderingEntity material_create() { return entt::null; }

    void material_set_render_priority(RenderingEntity p_material, int priority) {}
    void material_set_shader(RenderingEntity p_shader_material, RenderingEntity p_shader) {}
    RenderingEntity material_get_shader(RenderingEntity p_shader_material) const { return entt::null; }

    void material_set_param(RenderingEntity p_material, const StringName &p_param, const Variant &p_value) {}
    Variant material_get_param(RenderingEntity p_material, const StringName &p_param) const { return Variant(); }
    Variant material_get_param_default(RenderingEntity p_material, const StringName &p_param) const { return Variant(); }

    void material_set_line_width(RenderingEntity p_material, float p_width) {}

    void material_set_next_pass(RenderingEntity p_material, RenderingEntity p_next_material) {}

    bool material_is_animated(RenderingEntity p_material) { return false; }
    bool material_casts_shadows(RenderingEntity p_material) { return false; }

    void material_add_instance_owner(RenderingEntity p_material, RenderingEntity p_instance) {}
    void material_remove_instance_owner(RenderingEntity p_material, RenderingEntity p_instance) {}

    /* MESH API */

    RenderingEntity mesh_create() {
        DummyMesh *mesh = memnew(DummyMesh);
        ERR_FAIL_COND_V(!mesh, entt::null)
        mesh->blend_shape_count = 0;
        mesh->blend_shape_mode = RS::BLEND_SHAPE_MODE_NORMALIZED;
        return mesh_owner.make_rid(mesh);
    }

    void mesh_add_surface(RenderingEntity p_mesh, uint32_t p_format, RS::PrimitiveType p_primitive, const Vector<uint8_t> &p_array, int p_vertex_count, const Vector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<Vector<uint8_t> > &p_blend_shapes = Vector<Vector<uint8_t> >(), const Vector<AABB> &p_bone_aabbs = Vector<AABB>()) {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND(!m);

        m->surfaces.push_back(DummySurface());
        DummySurface *s = &m->surfaces.write[m->surfaces.size() - 1];
        s->format = p_format;
        s->primitive = p_primitive;
        s->array = p_array;
        s->vertex_count = p_vertex_count;
        s->index_array = p_index_array;
        s->index_count = p_index_count;
        s->aabb = p_aabb;
        s->blend_shapes = p_blend_shapes;
        s->bone_aabbs = p_bone_aabbs;
    }

    void mesh_set_blend_shape_count(RenderingEntity p_mesh, int p_amount) {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND(!m);
        m->blend_shape_count = p_amount;
    }
    int mesh_get_blend_shape_count(RenderingEntity p_mesh) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, 0)
        return m->blend_shape_count;
    }

    void mesh_set_blend_shape_mode(RenderingEntity p_mesh, RS::BlendShapeMode p_mode) {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND(!m);
        m->blend_shape_mode = p_mode;
    }
    RS::BlendShapeMode mesh_get_blend_shape_mode(RenderingEntity p_mesh) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, RS::BLEND_SHAPE_MODE_NORMALIZED)
        return m->blend_shape_mode;
    }

    void mesh_surface_update_region(RenderingEntity p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data) {}

    void mesh_surface_set_material(RenderingEntity p_mesh, int p_surface, RenderingEntity p_material) {}
    RenderingEntity mesh_surface_get_material(RenderingEntity p_mesh, int p_surface) const { return entt::null; }

    int mesh_surface_get_array_len(RenderingEntity p_mesh, int p_surface) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, 0)

        return m->surfaces[p_surface].vertex_count;
    }
    int mesh_surface_get_array_index_len(RenderingEntity p_mesh, int p_surface) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, 0)

        return m->surfaces[p_surface].index_count;
    }

    PoolVector<uint8_t> mesh_surface_get_array(RenderingEntity p_mesh, int p_surface) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, PoolVector<uint8_t>())

        return m->surfaces[p_surface].array;
    }
    PoolVector<uint8_t> mesh_surface_get_index_array(RenderingEntity p_mesh, int p_surface) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, PoolVector<uint8_t>())

        return m->surfaces[p_surface].index_array;
    }

    uint32_t mesh_surface_get_format(RenderingEntity p_mesh, int p_surface) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, 0)

        return m->surfaces[p_surface].format;
    }
    RS::PrimitiveType mesh_surface_get_primitive_type(RenderingEntity p_mesh, int p_surface) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, RS::PRIMITIVE_POINTS)

        return m->surfaces[p_surface].primitive;
    }

    AABB mesh_surface_get_aabb(RenderingEntity p_mesh, int p_surface) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, AABB())

        return m->surfaces[p_surface].aabb;
    }
    Vector<PoolVector<uint8_t> > mesh_surface_get_blend_shapes(RenderingEntity p_mesh, int p_surface) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, Vector<PoolVector<uint8_t> >())

        return m->surfaces[p_surface].blend_shapes;
    }
    Vector<AABB> mesh_surface_get_skeleton_aabb(RenderingEntity p_mesh, int p_surface) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, Vector<AABB>())

        return m->surfaces[p_surface].bone_aabbs;
    }

    void mesh_remove_surface(RenderingEntity p_mesh, int p_index) {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND(!m);
        ERR_FAIL_COND(p_index >= m->surfaces.size());

        m->surfaces.remove(p_index);
    }
    int mesh_get_surface_count(RenderingEntity p_mesh) const {
        DummyMesh *m = mesh_owner.getornull(p_mesh);
        ERR_FAIL_COND_V(!m, 0)
        return m->surfaces.size();
    }

    void mesh_set_custom_aabb(RenderingEntity p_mesh, const AABB &p_aabb) {}
    AABB mesh_get_custom_aabb(RenderingEntity p_mesh) const { return AABB(); }

    AABB mesh_get_aabb(RenderingEntity p_mesh, RenderingEntity p_skeleton) const { return AABB(); }
    void mesh_clear(RenderingEntity p_mesh) {}

    /* MULTIMESH API */

    virtual RenderingEntity multimesh_create() { return entt::null; }

    void multimesh_allocate(RenderingEntity p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, RS::MultimeshColorFormat p_color_format, RS::MultimeshCustomDataFormat p_data = RS::MULTIMESH_CUSTOM_DATA_NONE) {}
    int multimesh_get_instance_count(RenderingEntity p_multimesh) const { return 0; }

    void multimesh_set_mesh(RenderingEntity p_multimesh, RenderingEntity p_mesh) {}
    void multimesh_instance_set_transform(RenderingEntity p_multimesh, int p_index, const Transform &p_transform) {}
    void multimesh_instance_set_transform_2d(RenderingEntity p_multimesh, int p_index, const Transform2D &p_transform) {}
    void multimesh_instance_set_color(RenderingEntity p_multimesh, int p_index, const Color &p_color) {}
    void multimesh_instance_set_custom_data(RenderingEntity p_multimesh, int p_index, const Color &p_color) {}

    RenderingEntity multimesh_get_mesh(RenderingEntity p_multimesh) const { return entt::null; }

    Transform multimesh_instance_get_transform(RenderingEntity p_multimesh, int p_index) const { return Transform(); }
    Transform2D multimesh_instance_get_transform_2d(RenderingEntity p_multimesh, int p_index) const { return Transform2D(); }
    Color multimesh_instance_get_color(RenderingEntity p_multimesh, int p_index) const { return Color(); }
    Color multimesh_instance_get_custom_data(RenderingEntity p_multimesh, int p_index) const { return Color(); }

    void multimesh_set_as_bulk_array(RenderingEntity p_multimesh, const PoolVector<float> &p_array) {}

    void multimesh_set_visible_instances(RenderingEntity p_multimesh, int p_visible) {}
    int multimesh_get_visible_instances(RenderingEntity p_multimesh) const { return 0; }

    AABB multimesh_get_aabb(RenderingEntity p_multimesh) const { return AABB(); }

    /* IMMEDIATE API */

    RenderingEntity immediate_create() { return entt::null; }
    void immediate_begin(RenderingEntity p_immediate, RS::PrimitiveType p_rimitive, RenderingEntity p_texture = entt::null) {}
    void immediate_vertex(RenderingEntity p_immediate, const Vector3 &p_vertex) {}
    void immediate_normal(RenderingEntity p_immediate, const Vector3 &p_normal) {}
    void immediate_tangent(RenderingEntity p_immediate, const Plane &p_tangent) {}
    void immediate_color(RenderingEntity p_immediate, const Color &p_color) {}
    void immediate_uv(RenderingEntity p_immediate, const Vector2 &tex_uv) {}
    void immediate_uv2(RenderingEntity p_immediate, const Vector2 &tex_uv) {}
    void immediate_end(RenderingEntity p_immediate) {}
    void immediate_clear(RenderingEntity p_immediate) {}
    void immediate_set_material(RenderingEntity p_immediate, RenderingEntity p_material) {}
    RenderingEntity immediate_get_material(RenderingEntity p_immediate) const { return entt::null; }
    AABB immediate_get_aabb(RenderingEntity p_immediate) const { return AABB(); }

    /* SKELETON API */

    RenderingEntity skeleton_create() { return entt::null; }
    void skeleton_allocate(RenderingEntity p_skeleton, int p_bones, bool p_2d_skeleton = false) {}
    void skeleton_set_base_transform_2d(RenderingEntity p_skeleton, const Transform2D &p_base_transform) {}
    void skeleton_set_world_transform(RenderingEntity p_skeleton, bool p_enable, const Transform &p_world_transform) {}
    int skeleton_get_bone_count(RenderingEntity p_skeleton) const { return 0; }
    void skeleton_bone_set_transform(RenderingEntity p_skeleton, int p_bone, const Transform &p_transform) {}
    Transform skeleton_bone_get_transform(RenderingEntity p_skeleton, int p_bone) const { return Transform(); }
    void skeleton_bone_set_transform_2d(RenderingEntity p_skeleton, int p_bone, const Transform2D &p_transform) {}
    Transform2D skeleton_bone_get_transform_2d(RenderingEntity p_skeleton, int p_bone) const { return Transform2D(); }

    /* Light3D API */

    RenderingEntity light_create(RS::LightType p_type) { return entt::null; }

    RenderingEntity directional_light_create() { return light_create(RS::LIGHT_DIRECTIONAL); }
    RenderingEntity omni_light_create() { return light_create(RS::LIGHT_OMNI); }
    RenderingEntity spot_light_create() { return light_create(RS::LIGHT_SPOT); }

    void light_set_color(RenderingEntity p_light, const Color &p_color) {}
    void light_set_param(RenderingEntity p_light, RS::LightParam p_param, float p_value) {}
    void light_set_shadow(RenderingEntity p_light, bool p_enabled) {}
    void light_set_shadow_color(RenderingEntity p_light, const Color &p_color) {}
    void light_set_projector(RenderingEntity p_light, RenderingEntity p_texture) {}
    void light_set_negative(RenderingEntity p_light, bool p_enable) {}
    void light_set_cull_mask(RenderingEntity p_light, uint32_t p_mask) {}
    void light_set_reverse_cull_face_mode(RenderingEntity p_light, bool p_enabled) {}
    void light_set_use_gi(RenderingEntity p_light, bool p_enabled) override {}
    void light_set_bake_mode(RenderingEntity p_light, RS::LightBakeMode p_bake_mode) override {}

    void light_omni_set_shadow_mode(RenderingEntity p_light, RS::LightOmniShadowMode p_mode) {}
    void light_omni_set_shadow_detail(RenderingEntity p_light, RS::LightOmniShadowDetail p_detail) {}

    void light_directional_set_shadow_mode(RenderingEntity p_light, RS::LightDirectionalShadowMode p_mode) {}
    void light_directional_set_blend_splits(RenderingEntity p_light, bool p_enable) {}
    bool light_directional_get_blend_splits(RenderingEntity p_light) const { return false; }
    void light_directional_set_shadow_depth_range_mode(RenderingEntity p_light, RS::LightDirectionalShadowDepthRangeMode p_range_mode) {}
    RS::LightDirectionalShadowDepthRangeMode light_directional_get_shadow_depth_range_mode(RenderingEntity p_light) const { return RS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE; }

    RS::LightDirectionalShadowMode light_directional_get_shadow_mode(RenderingEntity p_light) { return RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL; }
    RS::LightOmniShadowMode light_omni_get_shadow_mode(RenderingEntity p_light) { return RS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID; }

    bool light_has_shadow(RenderingEntity p_light) const { return false; }

    RS::LightType light_get_type(RenderingEntity p_light) const { return RS::LIGHT_OMNI; }
    AABB light_get_aabb(RenderingEntity p_light) const { return AABB(); }
    float light_get_param(RenderingEntity p_light, RS::LightParam p_param) { return 0.0; }
    Color light_get_color(RenderingEntity p_light) { return Color(); }
    bool light_get_use_gi(RenderingEntity p_light) { return false; }
    RS::LightBakeMode light_get_bake_mode(RenderingEntity p_light) override { return RS::LightBakeMode::LIGHT_BAKE_DISABLED; }
    uint64_t light_get_version(RenderingEntity p_light) const { return 0; }

    /* PROBE API */

    RenderingEntity reflection_probe_create() { return entt::null; }

    void reflection_probe_set_update_mode(RenderingEntity p_probe, RS::ReflectionProbeUpdateMode p_mode) {}
    void reflection_probe_set_intensity(RenderingEntity p_probe, float p_intensity) {}
    void reflection_probe_set_interior_ambient(RenderingEntity p_probe, const Color &p_ambient) {}
    void reflection_probe_set_interior_ambient_energy(RenderingEntity p_probe, float p_energy) {}
    void reflection_probe_set_interior_ambient_probe_contribution(RenderingEntity p_probe, float p_contrib) {}
    void reflection_probe_set_max_distance(RenderingEntity p_probe, float p_distance) {}
    void reflection_probe_set_extents(RenderingEntity p_probe, const Vector3 &p_extents) {}
    void reflection_probe_set_origin_offset(RenderingEntity p_probe, const Vector3 &p_offset) {}
    void reflection_probe_set_as_interior(RenderingEntity p_probe, bool p_enable) {}
    void reflection_probe_set_enable_box_projection(RenderingEntity p_probe, bool p_enable) {}
    void reflection_probe_set_enable_shadows(RenderingEntity p_probe, bool p_enable) {}
    void reflection_probe_set_cull_mask(RenderingEntity p_probe, uint32_t p_layers) {}
    void reflection_probe_set_resolution(RenderingEntity p_probe, int p_resolution) {}

    AABB reflection_probe_get_aabb(RenderingEntity p_probe) const { return AABB(); }
    RS::ReflectionProbeUpdateMode reflection_probe_get_update_mode(RenderingEntity p_probe) const { return RenderingServer::REFLECTION_PROBE_UPDATE_ONCE; }
    uint32_t reflection_probe_get_cull_mask(RenderingEntity p_probe) const { return 0; }
    Vector3 reflection_probe_get_extents(RenderingEntity p_probe) const { return Vector3(); }
    Vector3 reflection_probe_get_origin_offset(RenderingEntity p_probe) const { return Vector3(); }
    float reflection_probe_get_origin_max_distance(RenderingEntity p_probe) const { return 0.0; }
    bool reflection_probe_renders_shadows(RenderingEntity p_probe) const { return false; }

    void instance_add_skeleton(RenderingEntity p_skeleton, RenderingEntity p_instance) {}
    void instance_remove_skeleton(RenderingEntity p_skeleton, RenderingEntity p_instance) {}

    void instance_add_dependency(RenderingEntity p_base, RenderingEntity p_instance) {}
    void instance_remove_dependency(RenderingEntity p_base, RenderingEntity p_instance) {}

    /* GI PROBE API */

    RenderingEntity gi_probe_create() { return entt::null; }

    void gi_probe_set_bounds(RenderingEntity p_probe, const AABB &p_bounds) {}
    AABB gi_probe_get_bounds(RenderingEntity p_probe) const { return AABB(); }

    void gi_probe_set_cell_size(RenderingEntity p_probe, float p_range) {}
    float gi_probe_get_cell_size(RenderingEntity p_probe) const { return 0.0; }

    void gi_probe_set_to_cell_xform(RenderingEntity p_probe, const Transform &p_xform) {}
    Transform gi_probe_get_to_cell_xform(RenderingEntity p_probe) const { return Transform(); }

    void gi_probe_set_dynamic_data(RenderingEntity p_probe, const PoolVector<int> &p_data) {}
    PoolVector<int> gi_probe_get_dynamic_data(RenderingEntity p_probe) const {
        PoolVector<int> p;
        return p;
    }

    void gi_probe_set_dynamic_range(RenderingEntity p_probe, int p_range) {}
    int gi_probe_get_dynamic_range(RenderingEntity p_probe) const { return 0; }

    void gi_probe_set_energy(RenderingEntity p_probe, float p_range) {}
    float gi_probe_get_energy(RenderingEntity p_probe) const { return 0.0; }

    void gi_probe_set_bias(RenderingEntity p_probe, float p_range) {}
    float gi_probe_get_bias(RenderingEntity p_probe) const { return 0.0; }

    void gi_probe_set_normal_bias(RenderingEntity p_probe, float p_range) {}
    float gi_probe_get_normal_bias(RenderingEntity p_probe) const { return 0.0; }

    void gi_probe_set_propagation(RenderingEntity p_probe, float p_range) {}
    float gi_probe_get_propagation(RenderingEntity p_probe) const { return 0.0; }

    void gi_probe_set_interior(RenderingEntity p_probe, bool p_enable) {}
    bool gi_probe_is_interior(RenderingEntity p_probe) const { return false; }


    uint32_t gi_probe_get_version(RenderingEntity p_probe) { return 0; }

    RenderingEntity gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth) { return entt::null; }
    void gi_probe_dynamic_data_update(RenderingEntity p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data) {}

    /* LIGHTMAP CAPTURE */
    struct Instantiable : public RID_Data {

        InList<RasterizerScene::InstanceBase> instance_list;

        _FORCE_INLINE_ void instance_change_notify(bool p_aabb = true, bool p_materials = true) {

            IntrusiveListNode<RasterizerScene::InstanceBase> *instances = instance_list.first();
            while (instances) {

                instances->self()->base_changed(p_aabb, p_materials);
                instances = instances->next();
            }
        }

        void instance_remove_deps() {
            IntrusiveListNode<RasterizerScene::InstanceBase> *instances = instance_list.first();
            while (instances) {

                IntrusiveListNode<RasterizerScene::InstanceBase> *next = instances->next();
                instances->self()->base_removed();
                instances = next;
            }
        }

        Instantiable() {}
        virtual ~Instantiable() {
        }
    };

    struct LightmapCapture : public Instantiable {

        PoolVector<LightmapCaptureOctree> octree;
        AABB bounds;
        Transform cell_xform;
        int cell_subdiv;
        float energy;
        LightmapCapture() {
            energy = 1.0;
            cell_subdiv = 1;
        }
    };

    mutable RID_Owner<LightmapCapture> lightmap_capture_data_owner;
    void lightmap_capture_set_bounds(RenderingEntity p_capture, const AABB &p_bounds) {}
    AABB lightmap_capture_get_bounds(RenderingEntity p_capture) const { return AABB(); }
    void lightmap_capture_set_octree(RenderingEntity p_capture, const PoolVector<uint8_t> &p_octree) {}
    RenderingEntity lightmap_capture_create() {
        LightmapCapture *capture = memnew(LightmapCapture);
        return lightmap_capture_data_owner.make_rid(capture);
    }
    PoolVector<uint8_t> lightmap_capture_get_octree(RenderingEntity p_capture) const {
        const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
        ERR_FAIL_COND_V(!capture, PoolVector<uint8_t>())
        return PoolVector<uint8_t>();
    }
    void lightmap_capture_set_octree_cell_transform(RenderingEntity p_capture, const Transform &p_xform) {}
    Transform lightmap_capture_get_octree_cell_transform(RenderingEntity p_capture) const { return Transform(); }
    void lightmap_capture_set_octree_cell_subdiv(RenderingEntity p_capture, int p_subdiv) {}
    int lightmap_capture_get_octree_cell_subdiv(RenderingEntity p_capture) const { return 0; }
    void lightmap_capture_set_energy(RenderingEntity p_capture, float p_energy) {}
    float lightmap_capture_get_energy(RenderingEntity p_capture) const { return 0.0; }
    const PoolVector<LightmapCaptureOctree> *lightmap_capture_get_octree_ptr(RenderingEntity p_capture) const {
        const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
        ERR_FAIL_COND_V(!capture, NULL)
        return &capture->octree;
    }

    /* PARTICLES */

    RenderingEntity particles_create() { return entt::null; }

    void particles_set_emitting(RenderingEntity p_particles, bool p_emitting) {}
    void particles_set_amount(RenderingEntity p_particles, int p_amount) {}
    void particles_set_lifetime(RenderingEntity p_particles, float p_lifetime) {}
    void particles_set_one_shot(RenderingEntity p_particles, bool p_one_shot) {}
    void particles_set_pre_process_time(RenderingEntity p_particles, float p_time) {}
    void particles_set_explosiveness_ratio(RenderingEntity p_particles, float p_ratio) {}
    void particles_set_randomness_ratio(RenderingEntity p_particles, float p_ratio) {}
    void particles_set_custom_aabb(RenderingEntity p_particles, const AABB &p_aabb) {}
    void particles_set_speed_scale(RenderingEntity p_particles, float p_scale) {}
    void particles_set_use_local_coordinates(RenderingEntity p_particles, bool p_enable) {}
    void particles_set_process_material(RenderingEntity p_particles, RenderingEntity p_material) {}
    void particles_set_fixed_fps(RenderingEntity p_particles, int p_fps) {}
    void particles_set_fractional_delta(RenderingEntity p_particles, bool p_enable) {}
    void particles_restart(RenderingEntity p_particles) {}

    void particles_set_draw_order(RenderingEntity p_particles, RS::ParticlesDrawOrder p_order) {}

    void particles_set_draw_passes(RenderingEntity p_particles, int p_count) {}
    void particles_set_draw_pass_mesh(RenderingEntity p_particles, int p_pass, RenderingEntity p_mesh) {}

    void particles_request_process(RenderingEntity p_particles) {}
    AABB particles_get_current_aabb(RenderingEntity p_particles) { return AABB(); }
    AABB particles_get_aabb(RenderingEntity p_particles) const { return AABB(); }

    void particles_set_emission_transform(RenderingEntity p_particles, const Transform &p_transform) {}

    bool particles_get_emitting(RenderingEntity p_particles) { return false; }
    int particles_get_draw_passes(RenderingEntity p_particles) const { return 0; }
    RenderingEntity particles_get_draw_pass_mesh(RenderingEntity p_particles, int p_pass) const { return entt::null; }

    virtual bool particles_is_inactive(RenderingEntity p_particles) const { return false; }

    /* RENDER TARGET */

    RenderingEntity render_target_create() { return entt::null; }
    void render_target_set_position(RenderingEntity p_render_target, int p_x, int p_y) {}
    void render_target_set_size(RenderingEntity p_render_target, int p_width, int p_height) {}
    RenderingEntity render_target_get_texture(RenderingEntity p_render_target) const { return entt::null; }
    void render_target_set_external_texture(RenderingEntity p_render_target, unsigned int p_texture_id) {}
    void render_target_set_flag(RenderingEntity p_render_target, RenderTargetFlags p_flag, bool p_value) {}
    bool render_target_was_used(RenderingEntity p_render_target) { return false; }
    void render_target_clear_used(RenderingEntity p_render_target) {}
    void render_target_set_msaa(RenderingEntity p_render_target, RS::ViewportMSAA p_msaa) {}
    void render_target_set_use_fxaa(RenderingEntity p_render_target, bool p_fxaa) override {}
    void render_target_set_use_debanding(RenderingEntity p_render_target, bool p_debanding) override {}

    /* CANVAS SHADOW */

    RenderingEntity canvas_light_shadow_buffer_create(int p_width) { return entt::null; }

    /* LIGHT SHADOW MAPPING */

    RenderingEntity canvas_light_occluder_create() { return entt::null; }
    void canvas_light_occluder_set_polylines(RenderingEntity p_occluder, Span<const Vector2> p_lines) override {}

    RS::InstanceType get_base_type(RenderingEntity p_rid) const {
        if (mesh_owner.owns(p_rid)) {
            return RS::INSTANCE_MESH;
        } else if (lightmap_capture_data_owner.owns(p_rid)) {
            return RS::INSTANCE_LIGHTMAP_CAPTURE;
        }
        return RS::INSTANCE_NONE;
    }

    bool free(RenderingEntity p_rid) {

        if (texture_owner.owns(p_rid)) {
            // delete the texture
            DummyTexture *texture = texture_owner.get(p_rid);
            texture_owner.free(p_rid);
            memdelete(texture);
        }
        return true;
    }

    bool has_os_feature(const String &p_feature) const { return false; }

    void update_dirty_resources() {}

    void set_debug_generate_wireframes(bool p_generate) {}

    void render_info_begin_capture() {}
    void render_info_end_capture() {}
    int get_captured_render_info(RS::RenderInfo p_info) { return 0; }

    int get_render_info(RS::RenderInfo p_info) { return 0; }
    const char *get_video_adapter_name() const { return ""; }
    const char *get_video_adapter_vendor() const { return ""; }

    static RasterizerStorage *base_singleton;

    RasterizerStorageDummy(){};
    ~RasterizerStorageDummy() {}
};

class RasterizerCanvasDummy : public RasterizerCanvas {
public:
    RenderingEntity light_internal_create() { return entt::null; }
    void light_internal_update(RenderingEntity p_rid, Span<RasterizerCanvasLight3DComponent *> p_light) {}
    void light_internal_free(RenderingEntity p_rid) {}

    void canvas_begin(){};
    void canvas_end(){};

    void canvas_render_items(Dequeue<Item *> &p_item_list, int p_z, const Color &p_modulate, Span<RasterizerCanvasLight3DComponent *> p_light, const Transform2D &p_transform){};
    void canvas_debug_viewport_shadows(Span<RasterizerCanvasLight3DComponent *> p_lights_with_shadow){};

    void canvas_light_shadow_buffer_update(RenderingEntity p_buffer, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, CameraMatrix *p_xform_cache) {}

    void reset_canvas() {}

    void draw_window_margins(int *p_margins, RenderingEntity *p_margin_textures) {}

    RasterizerCanvasDummy() {}
    ~RasterizerCanvasDummy() {}
};

class RasterizerDummy : public Rasterizer {
protected:
    RasterizerCanvasDummy canvas;
    RasterizerStorageDummy storage;
    RasterizerSceneDummy scene;

public:
    RasterizerStorage *get_storage() { return &storage; }
    RasterizerCanvas *get_canvas() { return &canvas; }
    RasterizerScene *get_scene() { return &scene; }

    void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) {}

    void initialize() {}
    void begin_frame(double frame_step) {}
    void set_current_render_target(RenderingEntity p_render_target) {}
    void restore_render_target(bool p_3d_was_drawn) {}
    void clear_render_target(const Color &p_color) {}
    void blit_render_target_to_screen(RenderingEntity p_render_target, const Rect2 &p_screen_rect, int p_screen = 0) {}
    void output_lens_distorted_to_screen(RenderingEntity p_render_target, const Rect2 &p_screen_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample) {}
    void end_frame(bool p_swap_buffers) {}
    void finalize() {}

    static Error is_viable() {
        return OK;
    }

    static Rasterizer *_create_current() {
        return memnew(RasterizerDummy);
    }

    static void make_current() {
        _create_func = _create_current;
    }

    RasterizerDummy() {}
    ~RasterizerDummy() {}
};

#endif // RASTERIZER_DUMMY_H
