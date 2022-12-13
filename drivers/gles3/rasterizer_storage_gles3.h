/*************************************************************************/
/*  rasterizer_storage_gles3.h                                           */
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

#include "core/self_list.h"
#include "rasterizer_asserts.h"
#include "rasterizer_gl_unique_handle.h"
#include "servers/rendering/rasterizer.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/renderer_instance_component.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering/render_entity_helpers.h"
#include "shader_compiler_gles3.h"
#include "shader_gles3.h"

#include "gles3/shaders/blend_shape.glsl.gen.h"
#include "gles3/shaders/canvas.glsl.gen.h"
#include "gles3/shaders/copy.glsl.gen.h"
#include "gles3/shaders/cubemap_filter.glsl.gen.h"
#include "gles3/shaders/particles.glsl.gen.h"

#include "EASTL/deque.h"
#include "servers/rendering/rendering_server_scene.h"

class RasterizerCanvasGLES3;
class RasterizerSceneGLES3;
struct RasterizerRenderTargetComponent;


#define _TEXTURE_SRGB_DECODE_EXT 0x8A48
#define _DECODE_EXT 0x8A49
#define _SKIP_DECODE_EXT 0x8A4A

void glTexStorage2DCustom(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type);

struct RasterizerMeshComponent;
struct RasterizerSurfaceComponent;
struct RasterizerMeshComponent;
struct RasterizerMultiMeshComponent;


struct RasterizerShaderComponent;

struct RasterizerGLES3ShadersStorage {

    CopyShaderGLES3 copy;
    ShaderCompilerGLES3 compiler;
    ShaderCacheGLES3 *cache;
    ThreadedCallableQueue<GLuint> *cache_write_queue;
    ThreadedCallableQueue<GLuint> *compile_queue;
    CubemapFilterShaderGLES3 cubemap_filter;
    BlendShapeShaderGLES3 blend_shapes;
    ParticlesShaderGLES3 particles;
    ShaderCompilerGLES3::IdentifierActions actions_canvas;
    ShaderCompilerGLES3::IdentifierActions actions_scene;
    ShaderCompilerGLES3::IdentifierActions actions_particles;
};

struct RasterizerStorageInfo {
    uint64_t texture_mem;
    uint64_t vertex_mem;

    struct Render {
        uint32_t object_count;
        uint32_t draw_call_count;
        uint32_t material_switch_count;
        uint32_t surface_switch_count;
        uint32_t shader_rebind_count;
        uint32_t shader_compiles_started_count;
        uint32_t shader_compiles_in_progress_count;
        uint32_t vertices_count;
        uint32_t _2d_item_count;
        uint32_t _2d_draw_call_count;

        void reset() {
            static_assert(eastl::is_POD<Render>());
            memset(this,0,sizeof(Render));
        }
    };
    Render render;
    Render render_final;
    Render snap;

    void end_capture() {
        snap.object_count = render.object_count - snap.object_count;
        snap.draw_call_count = render.draw_call_count - snap.draw_call_count;
        snap.material_switch_count = render.material_switch_count - snap.material_switch_count;
        snap.surface_switch_count = render.surface_switch_count - snap.surface_switch_count;
        snap.shader_rebind_count = render.shader_rebind_count - snap.shader_rebind_count;
        snap.shader_compiles_started_count = render.shader_compiles_started_count - snap.shader_compiles_started_count;
        snap.shader_compiles_in_progress_count = render.shader_compiles_in_progress_count - snap.shader_compiles_in_progress_count;
        snap.vertices_count = render.vertices_count - snap.vertices_count;
        snap._2d_item_count = render._2d_item_count - snap._2d_item_count;
        snap._2d_draw_call_count = render._2d_draw_call_count - snap._2d_draw_call_count;
    }

    RasterizerStorageInfo() {
        texture_mem = 0;
        vertex_mem = 0;
        render.reset();
        render_final.reset();
    }
};
RasterizerStorageInfo &get_rasterizer_storage_info();
struct RasterizerMaterialComponent;

class RasterizerStorageGLES3 : public RasterizerStorage {
public:
    RasterizerCanvasGLES3 *canvas;
    RasterizerSceneGLES3 *scene;
    static GLuint system_fbo; // on some devices, such as apple, screen is rendered to yet another fbo.

    enum RenderArchitecture {
        RENDER_ARCH_MOBILE,
        RENDER_ARCH_DESKTOP,
    };

    struct Config {
        Set<String> extensions;
        float anisotropic_level;

        int max_texture_image_units;
        int max_texture_size;
        int max_cubemap_texture_size;

        bool shrink_textures_x2;
        bool use_fast_texture_filter;
        bool use_anisotropic_filter;
        bool use_lightmap_filter_bicubic;
        bool use_physical_light_attenuation;
        bool s3tc_supported;
        bool latc_supported;
        bool rgtc_supported;
        bool bptc_supported;
        bool srgb_decode_supported;
        bool support_npot_repeat_mipmap;
        bool texture_float_linear_supported;
        bool framebuffer_float_supported;
        bool framebuffer_half_float_supported;
        bool use_rgba_2d_shadows;
        bool generate_wireframes;
        bool use_texture_array_environment;
        bool keep_original_textures;
        bool use_depth_prepass;
        bool force_vertex_shading;
        // in some cases the legacy render didn't orphan. We will mark these
        // so the user can switch orphaning off for them.
        bool should_orphan;
        bool program_binary_supported;
        bool parallel_shader_compile_supported;
        bool async_compilation_enabled;
        bool shader_cache_enabled;
    };


    struct Resources {

        GLTextureHandle white_tex;
        GLTextureHandle black_tex;
        GLTextureHandle transparent_tex;
        GLTextureHandle normal_tex;
        GLTextureHandle aniso_tex;
        GLTextureHandle depth_tex;

        GLTextureHandle white_tex_3d;
        GLTextureHandle white_tex_array;

        GLBufferHandle quadie;
        GLVAOHandle quadie_array;

        GLMultiBufferHandle<2> transform_feedback_buffers;
        GLVAOHandle transform_feedback_array;

    };
    Config config;
    mutable RasterizerGLES3ShadersStorage shaders;
    Resources resources;

    struct Frame {

        RenderingEntity current_rt;

        bool clear_request;
        Color clear_request_color;
        float time[4];
        float delta;
        uint64_t count;
    };

    /////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////API////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////

    /* TEXTURE API */

    RenderingEntity texture_create() override;
    void texture_allocate(RenderingEntity p_texture, int p_width, int p_height, int p_depth_3d, Image::Format p_format, RS::TextureType p_type, uint32_t p_flags = RS::TEXTURE_FLAGS_DEFAULT) override;
    void texture_set_data(RenderingEntity p_texture, const Ref<Image> &p_image, int p_layer = 0) override;
    void texture_set_data_partial(RenderingEntity p_texture, const Ref<Image> &p_image, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, int p_dst_mip, int p_layer = 0) override;
    Ref<Image> texture_get_data(RenderingEntity p_texture, int p_layer = 0) const override;
    void texture_set_flags(RenderingEntity p_texture, uint32_t p_flags) override;
    uint32_t texture_get_flags(RenderingEntity p_texture) const override;
    Image::Format texture_get_format(RenderingEntity p_texture) const override;
    RS::TextureType texture_get_type(RenderingEntity p_texture) const override;
    uint32_t texture_get_texid(RenderingEntity p_texture) const override;
    uint32_t texture_get_width(RenderingEntity p_texture) const override;
    uint32_t texture_get_height(RenderingEntity p_texture) const override;
    uint32_t texture_get_depth(RenderingEntity p_texture) const override;
    void texture_set_size_override(RenderingEntity p_texture, int p_width, int p_height, int p_depth) override;
    void texture_bind(RenderingEntity p_texture, uint32_t p_texture_no) override;

    void texture_set_path(RenderingEntity p_texture, StringView p_path) override;
    const String &texture_get_path(RenderingEntity p_texture) const override;

    void texture_set_shrink_all_x2_on_set_data(bool p_enable) override;

    void texture_debug_usage(Vector<RenderingServer::TextureInfo> *r_info) override;

    RenderingEntity texture_create_radiance_cubemap(RenderingEntity p_source, int p_resolution = -1) const override;

    void textures_keep_original(bool p_enable) override;

    void texture_set_detect_3d_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) override;
    void texture_set_detect_srgb_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) override;
    void texture_set_detect_normal_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) override;

    void texture_set_proxy(RenderingEntity p_texture, RenderingEntity p_proxy) override;
    Size2 texture_size_with_proxy(RenderingEntity p_texture) const override;

    void texture_set_force_redraw_if_visible(RenderingEntity p_texture, bool p_enable) override;

    /* SKY API */
    RenderingEntity sky_create() override;
    void sky_set_texture(RenderingEntity p_sky, RenderingEntity p_panorama, int p_radiance_size) override;

    /* SHADER API */

    RenderingEntity shader_create() override;

    void shader_set_code(RenderingEntity p_shader, const String &p_code) override;
    String shader_get_code(RenderingEntity p_shader) const override;
    void shader_get_param_list(RenderingEntity p_shader, Vector<PropertyInfo> *p_param_list) const override;

    void shader_set_default_texture_param(RenderingEntity p_shader, const StringName &p_name, RenderingEntity p_texture) override;
    RenderingEntity shader_get_default_texture_param(RenderingEntity p_shader, const StringName &p_name) const override;

    void shader_add_custom_define(RenderingEntity p_shader, StringView p_define) override;
    void shader_get_custom_defines(RenderingEntity p_shader, Vector<StringView> *p_defines) const override;
    void shader_remove_custom_define(RenderingEntity p_shader, StringView p_define) override;

    void set_shader_async_hidden_forbidden(bool p_forbidden) override;
    bool is_shader_async_hidden_forbidden() override;

    void update_dirty_shaders();

    /* COMMON MATERIAL API */

    RenderingEntity material_create() override;

    void material_set_shader(RenderingEntity p_material, RenderingEntity p_shader) override;
    RenderingEntity material_get_shader(RenderingEntity p_material) const override;

    void material_set_param(RenderingEntity p_material, const StringName &p_param, const Variant &p_value) override;
    Variant material_get_param(RenderingEntity p_material, const StringName &p_param) const override;
    Variant material_get_param_default(RenderingEntity p_material, const StringName &p_param) const override;

    void material_set_line_width(RenderingEntity p_material, float p_width) override;
    void material_set_next_pass(RenderingEntity p_material, RenderingEntity p_next_material) override;

    bool material_is_animated(RenderingEntity p_material) override;
    bool material_casts_shadows(RenderingEntity p_material) override;
    bool material_uses_tangents(RenderingEntity p_material) override;
    bool material_uses_ensure_correct_normals(RenderingEntity p_material) override;


    void material_add_instance_owner(RenderingEntity p_material, RenderingEntity p_instance) override;
    void material_remove_instance_owner(RenderingEntity p_material, RenderingEntity p_instance) override;

    void material_set_render_priority(RenderingEntity p_material, int priority) override;

    void update_dirty_materials();

    /* MESH API */

    RenderingEntity mesh_create() override;

    void mesh_add_surface(RenderingEntity p_mesh, uint32_t p_format, RS::PrimitiveType p_primitive, Span<const uint8_t> p_array, int p_vertex_count, Span<const uint8_t> p_index_array, int p_index_count, const AABB &p_aabb, const
                          Vector<PoolVector<uint8_t>> &p_blend_shapes = Vector<PoolVector<uint8_t>>(), Span<const AABB> p_bone_aabbs = {}) override;

    void mesh_set_blend_shape_count(RenderingEntity p_mesh, int p_amount) override;
    int mesh_get_blend_shape_count(RenderingEntity p_mesh) const override;

    void mesh_set_blend_shape_mode(RenderingEntity p_mesh, RS::BlendShapeMode p_mode) override;
    RS::BlendShapeMode mesh_get_blend_shape_mode(RenderingEntity p_mesh) const override;

    void mesh_set_blend_shape_values(RenderingEntity p_mesh, Span<const float> p_values) override;
    Vector<float> mesh_get_blend_shape_values(RenderingEntity p_mesh) const override;

    void mesh_surface_update_region(RenderingEntity p_mesh, int p_surface, int p_offset, Span<const uint8_t> p_data) override;

    void mesh_surface_set_material(RenderingEntity p_mesh, int p_surface, RenderingEntity p_material) override;
    RenderingEntity mesh_surface_get_material(RenderingEntity p_mesh, int p_surface) const override;

    int mesh_surface_get_array_len(RenderingEntity p_mesh, int p_surface) const override;
    int mesh_surface_get_array_index_len(RenderingEntity p_mesh, int p_surface) const override;

    PoolVector<uint8_t> mesh_surface_get_array(RenderingEntity p_mesh, int p_surface) const override;
    PoolVector<uint8_t> mesh_surface_get_index_array(RenderingEntity p_mesh, int p_surface) const override;

    uint32_t mesh_surface_get_format(RenderingEntity p_mesh, int p_surface) const override;
    RS::PrimitiveType mesh_surface_get_primitive_type(RenderingEntity p_mesh, int p_surface) const override;

    AABB mesh_surface_get_aabb(RenderingEntity p_mesh, int p_surface) const override;
    Vector<Vector<uint8_t>> mesh_surface_get_blend_shapes(RenderingEntity p_mesh, int p_surface) const override;
    const Vector<AABB> &mesh_surface_get_skeleton_aabb(RenderingEntity p_mesh, int p_surface) const override;

    void mesh_remove_surface(RenderingEntity p_mesh, int p_surface) override;
    int mesh_get_surface_count(RenderingEntity p_mesh) const override;

    void mesh_set_custom_aabb(RenderingEntity p_mesh, const AABB &p_aabb) override;
    AABB mesh_get_custom_aabb(RenderingEntity p_mesh) const override;

    AABB mesh_get_aabb(RenderingEntity p_mesh, RenderingEntity p_skeleton) const override;
    void mesh_clear(RenderingEntity p_mesh) override;

    void mesh_render_blend_shapes(RasterizerSurfaceComponent *s, const float *p_weights);

    /* MULTIMESH API */
    RenderingEntity multimesh_create() override;

    void multimesh_allocate(RenderingEntity p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, RS::MultimeshColorFormat p_color_format, RS::MultimeshCustomDataFormat p_data_format = RS::MULTIMESH_CUSTOM_DATA_NONE) override;
    int multimesh_get_instance_count(RenderingEntity p_multimesh) const override;

    void multimesh_set_mesh(RenderingEntity p_multimesh, RenderingEntity p_mesh) override;
    void multimesh_instance_set_transform(RenderingEntity p_multimesh, int p_index, const Transform &p_transform) override;
    void multimesh_instance_set_transform_2d(RenderingEntity p_multimesh, int p_index, const Transform2D &p_transform) override;
    void multimesh_instance_set_color(RenderingEntity p_multimesh, int p_index, const Color &p_color) override;
    void multimesh_instance_set_custom_data(RenderingEntity p_multimesh, int p_index, const Color &p_custom_data) override;

    RenderingEntity multimesh_get_mesh(RenderingEntity p_multimesh) const override;

    Transform multimesh_instance_get_transform(RenderingEntity p_multimesh, int p_index) const override;
    Transform2D multimesh_instance_get_transform_2d(RenderingEntity p_multimesh, int p_index) const override;
    Color multimesh_instance_get_color(RenderingEntity p_multimesh, int p_index) const override;
    Color multimesh_instance_get_custom_data(RenderingEntity p_multimesh, int p_index) const override;

    void multimesh_set_as_bulk_array(RenderingEntity p_multimesh, Span<const float> p_array) override;

    void multimesh_set_visible_instances(RenderingEntity p_multimesh, int p_visible) override;
    int multimesh_get_visible_instances(RenderingEntity p_multimesh) const override;

    AABB multimesh_get_aabb(RenderingEntity p_multimesh) const override;

    /* IMMEDIATE API */

    Vector3 chunk_vertex;
    Vector3 chunk_normal;
    Plane chunk_tangent;
    Color chunk_color;
    Vector2 chunk_uv;
    Vector2 chunk_uv2;

    RenderingEntity immediate_create() override;
    void immediate_begin(RenderingEntity p_immediate, RS::PrimitiveType p_rimitive, RenderingEntity p_texture = entt::null) override;
    void immediate_vertex(RenderingEntity p_immediate, const Vector3 &p_vertex) override;
    void immediate_normal(RenderingEntity p_immediate, const Vector3 &p_normal) override;
    void immediate_tangent(RenderingEntity p_immediate, const Plane &p_tangent) override;
    void immediate_color(RenderingEntity p_immediate, const Color &p_color) override;
    void immediate_uv(RenderingEntity p_immediate, const Vector2 &tex_uv) override;
    void immediate_uv2(RenderingEntity p_immediate, const Vector2 &tex_uv) override;
    void immediate_end(RenderingEntity p_immediate) override;
    void immediate_clear(RenderingEntity p_immediate) override;
    void immediate_set_material(RenderingEntity p_immediate, RenderingEntity p_material) override;
    RenderingEntity immediate_get_material(RenderingEntity p_immediate) const override;
    AABB immediate_get_aabb(RenderingEntity p_immediate) const override;

    /* SKELETON API */

    void update_dirty_skeletons();

    RenderingEntity skeleton_create() override;
    void skeleton_allocate(RenderingEntity p_skeleton, int p_bones, bool p_2d_skeleton = false) override;
    int skeleton_get_bone_count(RenderingEntity p_skeleton) const override;
    void skeleton_bone_set_transform(RenderingEntity p_skeleton, int p_bone, const Transform &p_transform) override;
    Transform skeleton_bone_get_transform(RenderingEntity p_skeleton, int p_bone) const override;
    void skeleton_bone_set_transform_2d(RenderingEntity p_skeleton, int p_bone, const Transform2D &p_transform) override;
    Transform2D skeleton_bone_get_transform_2d(RenderingEntity p_skeleton, int p_bone) const override;
    void skeleton_set_base_transform_2d(RenderingEntity p_skeleton, const Transform2D &p_base_transform) override;
    uint32_t skeleton_get_revision(RenderingEntity p_skeleton) const override;

    /* Light3D API */
    RenderingEntity light_create(RS::LightType p_type) override;

    void light_set_color(RenderingEntity p_light, const Color &p_color) override;
    void light_set_param(RenderingEntity p_light, RS::LightParam p_param, float p_value) override;
    void light_set_shadow(RenderingEntity p_light, bool p_enabled) override;
    void light_set_shadow_color(RenderingEntity p_light, const Color &p_color) override;
    void light_set_projector(RenderingEntity p_light, RenderingEntity p_texture) override;
    void light_set_negative(RenderingEntity p_light, bool p_enable) override;
    void light_set_cull_mask(RenderingEntity p_light, uint32_t p_mask) override;
    void light_set_reverse_cull_face_mode(RenderingEntity p_light, bool p_enabled) override;
    void light_set_use_gi(RenderingEntity p_light, bool p_enabled) override;
    void light_set_bake_mode(RenderingEntity p_light, RS::LightBakeMode p_bake_mode) override;

    void light_omni_set_shadow_mode(RenderingEntity p_light, RS::LightOmniShadowMode p_mode) override;
    void light_omni_set_shadow_detail(RenderingEntity p_light, RS::LightOmniShadowDetail p_detail) override;

    void light_directional_set_shadow_mode(RenderingEntity p_light, RS::LightDirectionalShadowMode p_mode) override;
    void light_directional_set_blend_splits(RenderingEntity p_light, bool p_enable) override;
    bool light_directional_get_blend_splits(RenderingEntity p_light) const override;

    RS::LightDirectionalShadowMode light_directional_get_shadow_mode(RenderingEntity p_light) override;
    RS::LightOmniShadowMode light_omni_get_shadow_mode(RenderingEntity p_light) override;

    void light_directional_set_shadow_depth_range_mode(RenderingEntity p_light, RS::LightDirectionalShadowDepthRangeMode p_range_mode) override;
    RS::LightDirectionalShadowDepthRangeMode light_directional_get_shadow_depth_range_mode(RenderingEntity p_light) const override;

    bool light_has_shadow(RenderingEntity p_light) const override;

    RS::LightType light_get_type(RenderingEntity p_light) const override;
    float light_get_param(RenderingEntity p_light, RS::LightParam p_param) override;
    Color light_get_color(RenderingEntity p_light) override;
    bool light_get_use_gi(RenderingEntity p_light) override;
    RS::LightBakeMode light_get_bake_mode(RenderingEntity p_light) override;

    AABB light_get_aabb(RenderingEntity p_light) const override;
    uint64_t light_get_version(RenderingEntity p_light) const override;

    /* PROBE API */

    RenderingEntity reflection_probe_create() override;

    void reflection_probe_set_update_mode(RenderingEntity p_probe, RS::ReflectionProbeUpdateMode p_mode) override;
    void reflection_probe_set_intensity(RenderingEntity p_probe, float p_intensity) override;
    void reflection_probe_set_interior_ambient(RenderingEntity p_probe, const Color &p_ambient) override;
    void reflection_probe_set_interior_ambient_energy(RenderingEntity p_probe, float p_energy) override;
    void reflection_probe_set_interior_ambient_probe_contribution(RenderingEntity p_probe, float p_contrib) override;
    void reflection_probe_set_max_distance(RenderingEntity p_probe, float p_distance) override;
    void reflection_probe_set_extents(RenderingEntity p_probe, const Vector3 &p_extents) override;
    void reflection_probe_set_origin_offset(RenderingEntity p_probe, const Vector3 &p_offset) override;
    void reflection_probe_set_as_interior(RenderingEntity p_probe, bool p_enable) override;
    void reflection_probe_set_enable_box_projection(RenderingEntity p_probe, bool p_enable) override;
    void reflection_probe_set_enable_shadows(RenderingEntity p_probe, bool p_enable) override;
    void reflection_probe_set_cull_mask(RenderingEntity p_probe, uint32_t p_layers) override;
    void reflection_probe_set_resolution(RenderingEntity p_probe, int p_resolution) override;

    AABB reflection_probe_get_aabb(RenderingEntity p_probe) const override;
    RS::ReflectionProbeUpdateMode reflection_probe_get_update_mode(RenderingEntity p_probe) const override;
    uint32_t reflection_probe_get_cull_mask(RenderingEntity p_probe) const override;

    Vector3 reflection_probe_get_extents(RenderingEntity p_probe) const override;
    Vector3 reflection_probe_get_origin_offset(RenderingEntity p_probe) const override;
    float reflection_probe_get_origin_max_distance(RenderingEntity p_probe) const override;
    bool reflection_probe_renders_shadows(RenderingEntity p_probe) const override;

    /* GI PROBE API */


    RenderingEntity gi_probe_create() override;

    void gi_probe_set_bounds(RenderingEntity p_probe, const AABB &p_bounds) override;
    AABB gi_probe_get_bounds(RenderingEntity p_probe) const override;

    void gi_probe_set_cell_size(RenderingEntity p_probe, float p_size) override;
    float gi_probe_get_cell_size(RenderingEntity p_probe) const override;

    void gi_probe_set_to_cell_xform(RenderingEntity p_probe, const Transform &p_xform) override;
    Transform gi_probe_get_to_cell_xform(RenderingEntity p_probe) const override;

    void gi_probe_set_dynamic_data(RenderingEntity p_probe, const PoolVector<int> &p_data) override;
    PoolVector<int> gi_probe_get_dynamic_data(RenderingEntity p_probe) const override;

    void gi_probe_set_dynamic_range(RenderingEntity p_probe, int p_range) override;
    int gi_probe_get_dynamic_range(RenderingEntity p_probe) const override;

    void gi_probe_set_energy(RenderingEntity p_probe, float p_range) override;
    float gi_probe_get_energy(RenderingEntity p_probe) const override;

    void gi_probe_set_bias(RenderingEntity p_probe, float p_range) override;
    float gi_probe_get_bias(RenderingEntity p_probe) const override;

    void gi_probe_set_normal_bias(RenderingEntity p_probe, float p_range) override;
    float gi_probe_get_normal_bias(RenderingEntity p_probe) const override;

    void gi_probe_set_propagation(RenderingEntity p_probe, float p_range) override;
    float gi_probe_get_propagation(RenderingEntity p_probe) const override;

    void gi_probe_set_interior(RenderingEntity p_probe, bool p_enable) override;
    bool gi_probe_is_interior(RenderingEntity p_probe) const override;

    uint32_t gi_probe_get_version(RenderingEntity p_probe) override;

    RenderingEntity gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth) override;
    void gi_probe_dynamic_data_update(RenderingEntity p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data) override;

    /* LIGHTMAP CAPTURE API */

    RenderingEntity lightmap_capture_create() override;
    void lightmap_capture_set_bounds(RenderingEntity p_capture, const AABB &p_bounds) override;
    AABB lightmap_capture_get_bounds(RenderingEntity p_capture) const override;
    void lightmap_capture_set_octree(RenderingEntity p_capture, const PoolVector<uint8_t> &p_octree) override;
    PoolVector<uint8_t> lightmap_capture_get_octree(RenderingEntity p_capture) const override;
    void lightmap_capture_set_octree_cell_transform(RenderingEntity p_capture, const Transform &p_xform) override;
    Transform lightmap_capture_get_octree_cell_transform(RenderingEntity p_capture) const override;
    void lightmap_capture_set_octree_cell_subdiv(RenderingEntity p_capture, int p_subdiv) override;
    int lightmap_capture_get_octree_cell_subdiv(RenderingEntity p_capture) const override;

    void lightmap_capture_set_energy(RenderingEntity p_capture, float p_energy) override;
    float lightmap_capture_get_energy(RenderingEntity p_capture) const override;
    void lightmap_capture_set_interior(RenderingEntity p_capture, bool p_interior) override;
    bool lightmap_capture_is_interior(RenderingEntity p_capture) const override;

    const PoolVector<LightmapCaptureOctree> *lightmap_capture_get_octree_ptr(RenderingEntity p_capture) const override;

    void update_dirty_captures();

    /* PARTICLES API */

    void update_particles();

    RenderingEntity particles_create() override;

    void particles_set_emitting(RenderingEntity p_particles, bool p_emitting) override;
    bool particles_get_emitting(RenderingEntity p_particles) override;
    void particles_set_amount(RenderingEntity p_particles, int p_amount) override;
    void particles_set_lifetime(RenderingEntity p_particles, float p_lifetime) override;
    void particles_set_one_shot(RenderingEntity p_particles, bool p_one_shot) override;
    void particles_set_pre_process_time(RenderingEntity p_particles, float p_time) override;
    void particles_set_explosiveness_ratio(RenderingEntity p_particles, float p_ratio) override;
    void particles_set_randomness_ratio(RenderingEntity p_particles, float p_ratio) override;
    void particles_set_custom_aabb(RenderingEntity p_particles, const AABB &p_aabb) override;
    void particles_set_speed_scale(RenderingEntity p_particles, float p_scale) override;
    void particles_set_use_local_coordinates(RenderingEntity p_particles, bool p_enable) override;
    void particles_set_process_material(RenderingEntity p_particles, RenderingEntity p_material) override;
    void particles_set_fixed_fps(RenderingEntity p_particles, int p_fps) override;
    void particles_set_fractional_delta(RenderingEntity p_particles, bool p_enable) override;
    void particles_restart(RenderingEntity p_particles) override;

    void particles_set_draw_order(RenderingEntity p_particles, RS::ParticlesDrawOrder p_order) override;

    void particles_set_draw_passes(RenderingEntity p_particles, int p_passes) override;
    void particles_set_draw_pass_mesh(RenderingEntity p_particles, int p_pass, RenderingEntity p_mesh) override;

    void particles_request_process(RenderingEntity p_particles) override;
    AABB particles_get_current_aabb(RenderingEntity p_particles) override;
    AABB particles_get_aabb(RenderingEntity p_particles) const override;

    void particles_set_emission_transform(RenderingEntity p_particles, const Transform &p_transform) override;

    int particles_get_draw_passes(RenderingEntity p_particles) const override;
    RenderingEntity particles_get_draw_pass_mesh(RenderingEntity p_particles, int p_pass) const override;

    bool particles_is_inactive(RenderingEntity p_particles) const override;

    /* INSTANCE API */

    void instance_add_skeleton(RenderingEntity p_skeleton, RenderingEntity p_instance) override;
    void instance_remove_skeleton(RenderingEntity p_skeleton, RenderingEntity p_instance) override;

    void instance_add_dependency(RenderingEntity p_base, RenderingEntity p_instance) override;
    void instance_remove_dependency(RenderingEntity p_base, RenderingEntity p_instance) override;

    /* RENDER TARGET API */

    RenderingEntity render_target_create() override;
    void render_target_set_size(RenderingEntity p_render_target, int p_width, int p_height) override;
    RenderingEntity render_target_get_texture(RenderingEntity p_render_target) const override;
    uint32_t render_target_get_depth_texture_id(RenderingEntity p_render_target) const override;
    void render_target_set_external_texture(RenderingEntity p_render_target, unsigned int p_texture_id, unsigned int p_depth_id) override;

    void render_target_set_flag(RenderingEntity p_render_target, RS::RenderTargetFlags p_flag, bool p_value) override;
    bool render_target_was_used(RenderingEntity p_render_target) override;
    void render_target_clear_used(RenderingEntity p_render_target) override;
    void render_target_set_msaa(RenderingEntity p_render_target, RS::ViewportMSAA p_msaa) override;
    void render_target_set_use_fxaa(RenderingEntity p_render_target, bool p_fxaa) override;
    void render_target_set_use_debanding(RenderingEntity p_render_target, bool p_debanding) override;
    void render_target_set_sharpen_intensity(RenderingEntity p_render_target, float p_intensity) override;
    /* CANVAS SHADOW API */

    RenderingEntity canvas_light_shadow_buffer_create(int p_width) override;

    /* LIGHT SHADOW MAPPING API */

    RenderingEntity canvas_light_occluder_create() override;
    void canvas_light_occluder_set_polylines(RenderingEntity p_occluder, Span<const Vector2> p_lines) override;


    RS::InstanceType get_base_type(RenderingEntity p_rid) const override;

    bool free(RenderingEntity p_rid) override;

    Frame frame;

    void initialize();
    void finalize();

    bool has_os_feature(const StringName &p_feature) const override;

    void update_dirty_resources() override;

    void set_debug_generate_wireframes(bool p_generate) override;

    void render_info_begin_capture() override;
    void render_info_end_capture() override;
    int get_captured_render_info(RS::RenderInfo p_info) override;

    uint64_t get_render_info(RS::RenderInfo p_info) override;
    const char *get_video_adapter_name() const override;
    const char *get_video_adapter_vendor() const override;

    void buffer_orphan_and_upload(unsigned int p_buffer_size, unsigned int p_offset, unsigned int p_data_size, const void *p_data, GLenum p_target = GL_ARRAY_BUFFER, GLenum p_usage = GL_DYNAMIC_DRAW, bool p_optional_orphan = false) const;
    bool safe_buffer_sub_data(unsigned int p_total_buffer_size, GLenum p_target, unsigned int p_offset, unsigned int p_data_size, const void *p_data, unsigned int &r_offset_after) const;

    RasterizerStorageGLES3();
    ~RasterizerStorageGLES3();
};

inline bool RasterizerStorageGLES3::safe_buffer_sub_data(unsigned int p_total_buffer_size, GLenum p_target, unsigned int p_offset, unsigned int p_data_size, const void *p_data, unsigned int &r_offset_after) const {
    r_offset_after = p_offset + p_data_size;
#ifdef DEBUG_ENABLED
    // we are trying to write across the edge of the buffer
    if (r_offset_after > p_total_buffer_size)
        return false;
#endif
    glBufferSubData(p_target, p_offset, p_data_size, p_data);
    return true;
}

// standardize the orphan / upload in one place so it can be changed per platform as necessary, and avoid future
// bugs causing pipeline stalls
inline void RasterizerStorageGLES3::buffer_orphan_and_upload(unsigned int p_buffer_size, unsigned int p_offset, unsigned int p_data_size, const void *p_data, GLenum p_target, GLenum p_usage, bool p_optional_orphan) const {
    // Orphan the buffer to avoid CPU/GPU sync points caused by glBufferSubData
    // Was previously #ifndef GLES_OVER_GL however this causes stalls on desktop mac also (and possibly other)
    glBufferData(p_target, p_buffer_size, nullptr, p_usage);
#ifdef RASTERIZER_EXTRA_CHECKS
    // fill with garbage off the end of the array
    if (p_buffer_size) {
        unsigned int start = p_offset + p_data_size;
        unsigned int end = start + 1024;
        if (end < p_buffer_size) {
            uint8_t *garbage = (uint8_t *)alloca(1024);
            for (int n = 0; n < 1024; n++) {
                garbage[n] = Math::random(0, 255);
            }
            glBufferSubData(p_target, start, 1024, garbage);
        }
    }
#endif
    RAST_DEV_DEBUG_ASSERT((p_offset + p_data_size) <= p_buffer_size);
    glBufferSubData(p_target, p_offset, p_data_size, p_data);
}
