/*************************************************************************/
/*  rasterizer_scene_gles3.h                                             */
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

/* Must come before shaders or the Windows build fails... */
#include "rasterizer_render_target_component.h"
#include "rasterizer_storage_gles3.h"

#include "drivers/gles3/shaders/cube_to_dp.glsl.gen.h"
#include "drivers/gles3/shaders/effect_blur.glsl.gen.h"
#include "drivers/gles3/shaders/exposure.glsl.gen.h"
#include "drivers/gles3/shaders/resolve.glsl.gen.h"
#include "drivers/gles3/shaders/scene.glsl.gen.h"
#include "drivers/gles3/shaders/screen_space_reflection.glsl.gen.h"
#include "drivers/gles3/shaders/ssao.glsl.gen.h"
#include "drivers/gles3/shaders/ssao_blur.glsl.gen.h"
#include "drivers/gles3/shaders/ssao_minify.glsl.gen.h"
#include "drivers/gles3/shaders/subsurf_scattering.glsl.gen.h"
#include "drivers/gles3/shaders/tonemap.glsl.gen.h"
#include "servers/rendering/rendering_server_globals.h"

#include "EASTL/sort.h"
struct RasterizerCommonGeometryComponent;
struct RasterizerLight3DComponent;
struct RasterizerSkyComponent;
struct RasterizerLightInstanceComponent;
struct RasterizerEnvironmentComponent;

struct RasterizerReflectionCubeMap {
    GLMultiFBOHandle<6> fbo_id;
    GLTextureHandle cubemap;
    GLTextureHandle depth;
    int size;
};

enum RenderListConstants : uint64_t {
    DEFAULT_MAX_ELEMENTS = 65536,
    SORT_FLAG_SKELETON = 1,
    SORT_FLAG_INSTANCING = 2,
    MAX_DIRECTIONAL_LIGHTS = 16,
    DEFAULT_MAX_LIGHTS = 4096,
    DEFAULT_MAX_REFLECTIONS = 1024,
    DEFAULT_MAX_LIGHTS_PER_OBJECT = 32,

    SORT_KEY_PRIORITY_SHIFT = 56,
    SORT_KEY_PRIORITY_MASK = 0xFF,
    //depth layer for opaque (56-52)
    SORT_KEY_OPAQUE_DEPTH_LAYER_SHIFT = 52,
    SORT_KEY_OPAQUE_DEPTH_LAYER_MASK = 0xF,
    SORT_KEY_UNSHADED_FLAG = (uint64_t(1) << 50),
    SORT_KEY_NO_DIRECTIONAL_FLAG = (uint64_t(1) << 49),
    SORT_KEY_LIGHTMAP_CAPTURE_FLAG = (uint64_t(1) << 48),
    SORT_KEY_LIGHTMAP_LAYERED_FLAG = (uint64_t(1) << 47),
    SORT_KEY_LIGHTMAP_FLAG = (uint64_t(1) << 46),
    SORT_KEY_GI_PROBES_FLAG = (uint64_t(1) << 45),
    SORT_KEY_VERTEX_LIT_FLAG = (uint64_t(1) << 44),
    SORT_KEY_SHADING_SHIFT = 44,
    SORT_KEY_SHADING_MASK = 127,
    //44-28 material index
    SORT_KEY_MATERIAL_INDEX_SHIFT = 28,
    //28-8 geometry index
    SORT_KEY_GEOMETRY_INDEX_SHIFT = 8,
    //bits 5-7 geometry type
    SORT_KEY_GEOMETRY_TYPE_SHIFT = 5,
    //bits 0-5 for flags
    SORT_KEY_OPAQUE_PRE_PASS = 8,
    SORT_KEY_CULL_DISABLED_FLAG = 4,
    SORT_KEY_SKELETON_FLAG = 2,
    SORT_KEY_MIRROR_FLAG = 1
};
struct RenderListElement {

    RenderingEntity instance; //RendererInstanceComponent *
    RenderingEntity geometry; //RasterizerStorageGLES3::Geometry *
    RenderingEntity material; //RasterizerMaterialComponent *
    RenderingEntity owner;
    uint64_t sort_key;
    float sort_depth; // filled as needed.
};
struct RenderList {


    int max_elements;
    int max_lights;
    int max_reflections;
    int max_lights_per_object;

    // Storage is using Dequeue to ensure pointer stability on push
    Dequeue<RenderListElement> base_elements;
    Dequeue<RenderListElement> base_alpha_elements;

    Vector<RenderListElement *> elements;
    Vector<RenderListElement *> alpha_elements;

    void clear() {
        elements.clear();
        alpha_elements.clear();
    }

    //should eventually be replaced by radix

    static bool compare_by_key(const RenderListElement *A, const RenderListElement *B) {
        return A->sort_key < B->sort_key;
    };
    static bool compare_by_depth(const RenderListElement *A, const RenderListElement *B) {
        return A->sort_depth < B->sort_depth;
    }
    static bool compare_by_reverse_depth_and_priority(const RenderListElement *A, const RenderListElement *B) {
        uint32_t layer_A = uint32_t(A->sort_key >> RenderListConstants::SORT_KEY_PRIORITY_SHIFT);
        uint32_t layer_B = uint32_t(B->sort_key >> RenderListConstants::SORT_KEY_PRIORITY_SHIFT);
        if (layer_A == layer_B) {
            return A->sort_depth > B->sort_depth;
        } else {
            return layer_A < layer_B;
        }
    }
    void sort_by(bool p_alpha, bool(*sorter)(const RenderListElement *A, const RenderListElement *B)) {
        if (p_alpha) {
            eastl::sort(alpha_elements.begin(),alpha_elements.end(),sorter);
        } else {
            eastl::sort(elements.begin(),elements.end(),sorter);
        }
    }


    void sort_by_key(bool p_alpha) {
        sort_by(p_alpha,compare_by_key);
    }
    void sort_by_depth(bool p_alpha) { //used for shadows
        sort_by(p_alpha,compare_by_depth);
    }

    void sort_by_reverse_depth_and_priority(bool p_alpha) { //used for alpha
        sort_by(p_alpha,compare_by_reverse_depth_and_priority);
    }

    _FORCE_INLINE_ RenderListElement *add_element(float depth) {

        if (elements.size() >= max_elements)
            return nullptr;
        base_elements.emplace_back();
        base_elements.back().sort_depth = depth;
        elements.emplace_back(&base_elements.back());
        return elements.back();
    }

    _FORCE_INLINE_ RenderListElement *add_alpha_element(float depth) {

        if (alpha_elements.size() >= max_elements)
            return nullptr;
        base_alpha_elements.emplace_back();
        base_alpha_elements.back().sort_depth = depth;
        alpha_elements.emplace_back(&base_alpha_elements.back());
        return alpha_elements.back();
    }

    void init() {
        clear();
    }

    RenderList() {

        max_elements = RenderListConstants::DEFAULT_MAX_ELEMENTS;
        max_lights = RenderListConstants::DEFAULT_MAX_LIGHTS;
        max_reflections = RenderListConstants::DEFAULT_MAX_REFLECTIONS;
    }

    ~RenderList() {
    }
};

class RasterizerSceneGLES3 : public RasterizerScene {
public:
    enum ShadowFilterMode {
        SHADOW_FILTER_NEAREST,
        SHADOW_FILTER_PCF5,
        SHADOW_FILTER_PCF13,
    };

    ShadowFilterMode shadow_filter_mode;

    uint64_t shadow_atlas_realloc_tolerance_msec;

    enum SubSurfaceScatterQuality {
        SSS_QUALITY_LOW,
        SSS_QUALITY_MEDIUM,
        SSS_QUALITY_HIGH,
    };

    SubSurfaceScatterQuality subsurface_scatter_quality;
    float subsurface_scatter_size;
    bool subsurface_scatter_follow_surface;
    bool subsurface_scatter_weight_samples;

    uint64_t render_pass;
    uint64_t scene_pass;
    uint32_t current_material_index;
    uint32_t current_geometry_index;

    RenderingEntity default_material;
    RenderingEntity default_material_twosided;
    RenderingEntity default_shader;
    RenderingEntity default_shader_twosided;

    RenderingEntity default_worldcoord_material;
    RenderingEntity default_worldcoord_material_twosided;
    RenderingEntity default_worldcoord_shader;
    RenderingEntity default_worldcoord_shader_twosided;

    RenderingEntity default_overdraw_material;
    RenderingEntity default_overdraw_shader;

    RasterizerStorageGLES3 *storage;

    Vector<RasterizerRenderTargetComponent::Exposure> exposure_shrink;
    int exposure_shrink_size;

    struct State {

        bool texscreen_copied;
        int current_blend_mode;
        float current_line_width;
        int current_depth_draw;
        bool current_depth_test;
        GLNonOwningHandle current_main_tex;

        SceneShaderGLES3 scene_shader;
        CubeToDpShaderGLES3 cube_to_dp_shader;
        ResolveShaderGLES3 resolve_shader;
        ScreenSpaceReflectionShaderGLES3 ssr_shader;
        EffectBlurShaderGLES3 effect_blur_shader;
        SubsurfScatteringShaderGLES3 sss_shader;
        SsaoMinifyShaderGLES3 ssao_minify_shader;
        SsaoShaderGLES3 ssao_shader;
        SsaoBlurShaderGLES3 ssao_blur_shader;
        ExposureShaderGLES3 exposure_shader;
        TonemapShaderGLES3 tonemap_shader;

        struct SceneDataUBO {
            //this is a std140 compatible struct. Please read the OpenGL 3.3 Specification spec before doing any changes
            float projection_matrix[16];
            float inv_projection_matrix[16];
            float camera_inverse_matrix[16];
            float camera_matrix[16];
            float ambient_light_color[4];
            float bg_color[4];
            float fog_color_enabled[4];
            float fog_sun_color_amount[4];

            float ambient_energy;
            float bg_energy;
            float z_offset;
            float z_slope_scale;
            float shadow_dual_paraboloid_render_zfar;
            float shadow_dual_paraboloid_render_side;
            float viewport_size[2];
            float screen_pixel_size[2];
            float shadow_atlas_pixel_size[2];
            float shadow_directional_pixel_size[2];

            float time;
            float z_far;
            float reflection_multiplier;
            float subsurface_scatter_width;
            float ambient_occlusion_affect_light;
            float ambient_occlusion_affect_ssao;
            float opaque_prepass_threshold;

            uint32_t fog_depth_enabled;
            float fog_depth_begin;
            float fog_depth_end;
            float fog_density;
            float fog_depth_curve;
            uint32_t fog_transmit_enabled;
            float fog_transmit_curve;
            uint32_t fog_height_enabled;
            float fog_height_min;
            float fog_height_max;
            float fog_height_curve;
            uint32_t view_index;
            // make sure this struct is padded to be a multiple of 16 bytes for webgl
            float pad[3];

        } ubo_data;
        static_assert(sizeof(SceneDataUBO) % 16 == 0, "SceneDataUBO size must be a multiple of 16 bytes");

        GLBufferHandle scene_ubo;

        struct EnvironmentRadianceUBO {

            float transform[16];
            float ambient_contribution;
            uint8_t padding[12];

        } env_radiance_data;

        GLBufferHandle env_radiance_ubo;

        GLBufferHandle sky_verts;
        GLVAOHandle sky_array;

        GLBufferHandle directional_ubo;

        GLBufferHandle spot_array_ubo;
        GLBufferHandle omni_array_ubo;
        GLBufferHandle reflection_array_ubo;

        GLBufferHandle immediate_buffer;
        GLVAOHandle immediate_array;

        uint32_t ubo_light_size;
        uint8_t *spot_array_tmp;
        uint8_t *omni_array_tmp;
        uint8_t *reflection_array_tmp;

        int max_ubo_lights;
        int max_forward_lights_per_object;
        int max_ubo_reflections;
        int max_skeleton_bones;

        bool used_contact_shadows;

        int spot_light_count;
        int omni_light_count;
        int directional_light_count;
        int reflection_probe_count;

        bool cull_front;
        bool cull_disabled;
        bool used_sss;
        bool used_screen_texture;

        bool used_depth_prepass;

        bool used_depth_texture;
        bool prepared_depth_texture;
        bool bound_depth_texture;

        RS::ViewportDebugDraw debug_draw;
    } state;

    /* SHADOW ATLAS API */



    struct ShadowCubeMap {

        GLMultiFBOHandle<6> fbo_id;
        GLTextureHandle cubemap;
        uint32_t size;
        ShadowCubeMap(ShadowCubeMap &&) = default;
        ShadowCubeMap &operator=(ShadowCubeMap &&) = default;
        ShadowCubeMap() = default;
    };

    Vector<ShadowCubeMap> shadow_cubemaps;
    int directional_shadow_size;

    //RenderingEntity_Owner<> shadow_atlas_owner;

    void directional_shadow_create();
    RenderingEntity shadow_atlas_create() override;
    void shadow_atlas_set_size(RenderingEntity p_atlas, int p_size) override;
    void shadow_atlas_set_quadrant_subdivision(RenderingEntity p_atlas, int p_quadrant, int p_subdivision) override;
    bool shadow_atlas_update_light(RenderingEntity p_atlas, RenderingEntity p_light_intance, float p_coverage, uint64_t p_light_version) override;

    struct DirectionalShadow {
        GLFBOHandle fbo;
        GLTextureHandle depth;
        int light_count=0;
        int size=0;
        int current_light=0;
    } directional_shadow;

    int get_directional_light_shadow_size(RenderingEntity p_light_intance) override;
    void set_directional_shadow_count(int p_count) override;

    /* REFLECTION PROBE ATLAS API */

    RenderingEntity reflection_atlas_create() override;
    void reflection_atlas_set_size(RenderingEntity p_ref_atlas, int p_size) override;
    void reflection_atlas_set_subdivision(RenderingEntity p_ref_atlas, int p_subdiv) override;

    /* REFLECTION CUBEMAPS */


    Vector<RasterizerReflectionCubeMap> reflection_cubemaps;

    /* REFLECTION PROBE INSTANCE */

    struct ReflectionProbeDataUBO {

        float box_extents[4];
        float box_ofs[4];
        float params[4]; // intensity, 0, 0, boxproject
        float ambient[4]; //color, probe contrib
        float atlas_clamp[4];
        float local_matrix[16]; //up to here for spot and omni, rest is for directional
        //notes: for ambientblend, use distance to edge to blend between already existing global environment
    };

    RenderingEntity reflection_probe_instance_create(RenderingEntity p_probe) override;
    void reflection_probe_instance_set_transform(RenderingEntity p_instance, const Transform &p_transform) override;
    void reflection_probe_release_atlas_index(RenderingEntity p_instance) override;
    bool reflection_probe_instance_needs_redraw(RenderingEntity p_instance) override;
    bool reflection_probe_instance_has_reflection(RenderingEntity p_instance) override;
    bool reflection_probe_instance_begin_render(RenderingEntity p_instance, RenderingEntity p_reflection_atlas) override;
    bool reflection_probe_instance_postprocess_step(RenderingEntity p_instance) override;

    /* ENVIRONMENT API */

    RenderingEntity environment_create() override;

    void environment_set_background(RenderingEntity p_env, RS::EnvironmentBG p_bg) override;
    void environment_set_sky(RenderingEntity p_env, RenderingEntity p_sky) override;
    void environment_set_sky_custom_fov(RenderingEntity p_env, float p_scale) override;
    void environment_set_sky_orientation(RenderingEntity p_env, const Basis &p_orientation) override;
    void environment_set_bg_color(RenderingEntity p_env, const Color &p_color) override;
    void environment_set_bg_energy(RenderingEntity p_env, float p_energy) override;
    void environment_set_canvas_max_layer(RenderingEntity p_env, int p_max_layer) override;
    void environment_set_ambient_light(RenderingEntity p_env, const Color &p_color, float p_energy = 1.0, float p_sky_contribution = 0.0) override;
    void environment_set_camera_feed_id(RenderingEntity p_env, int p_camera_feed_id) override;

    void environment_set_dof_blur_near(RenderingEntity p_env, bool p_enable, float p_distance, float p_transition, float p_amount, RS::EnvironmentDOFBlurQuality p_quality) override;
    void environment_set_dof_blur_far(RenderingEntity p_env, bool p_enable, float p_distance, float p_transition, float p_amount, RS::EnvironmentDOFBlurQuality p_quality) override;
    void environment_set_glow(RenderingEntity p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, RS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale, bool p_high_quality) override;
    void environment_set_fog(RenderingEntity p_env, bool p_enable, float p_begin, float p_end, RenderingEntity p_gradient_texture) override;

    void environment_set_ssr(RenderingEntity p_env, bool p_enable, int p_max_steps, float p_fade_in, float p_fade_out, float p_depth_tolerance, bool p_roughness) override;
    void environment_set_ssao(RenderingEntity p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, RS::EnvironmentSSAOQuality p_quality, RS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) override;

    void environment_set_tonemap(RenderingEntity p_env, RS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale) override;

    void environment_set_adjustment(RenderingEntity p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RenderingEntity p_ramp) override;

    void environment_set_fog(RenderingEntity p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) override;
    void environment_set_fog_depth(RenderingEntity p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) override;
    void environment_set_fog_height(RenderingEntity p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) override;

    bool is_environment(RenderingEntity p_env) override;

    RS::EnvironmentBG environment_get_background(RenderingEntity p_env) override;
    int environment_get_canvas_max_layer(RenderingEntity p_env) override;

    /* LIGHT INSTANCE */

    RenderingEntity light_instance_create(RenderingEntity p_light) override;
    void light_instance_set_transform(RenderingEntity p_light_instance, const Transform &p_transform) override;
    void light_instance_set_shadow_transform(RenderingEntity p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale = 1.0) override;
    void light_instance_mark_visible(RenderingEntity p_light_instance) override;

    /* REFLECTION INSTANCE */

    RenderingEntity gi_probe_instance_create() override;
    void gi_probe_instance_set_light_data(RenderingEntity p_probe, RenderingEntity p_base, RenderingEntity p_data) override;
    void gi_probe_instance_set_transform_to_data(RenderingEntity p_probe, const Transform &p_xform) override;
    void gi_probe_instance_set_bounds(RenderingEntity p_probe, const Vector3 &p_bounds) override;

    /* RENDER LIST */


    RasterizerLightInstanceComponent *directional_light;
    RasterizerLightInstanceComponent *directional_lights[RenderListConstants::MAX_DIRECTIONAL_LIGHTS];
    RenderingEntity first_directional_light = entt::null;

    RenderList render_list;

    _FORCE_INLINE_ void _set_cull(bool p_front, bool p_disabled, bool p_reverse_cull);

    bool _setup_material(RasterizerMaterialComponent *p_material, bool p_depth_pass, bool p_alpha_pass);
    void _setup_geometry(RenderListElement *e, const Transform &p_view_transform);
    void _render_geometry(RenderListElement *e);
    void _setup_light(RenderListElement *e, const Transform &p_view_transform);

    void _render_list(Span<RenderListElement *> p_elements, const Transform &p_view_transform, const CameraMatrix &p_projection, RasterizerSkyComponent *p_sky, bool p_reverse_cull, bool p_alpha_pass, bool p_shadow, bool p_directional_add, bool p_directional_shadows);

    void _draw_sky(RasterizerSkyComponent *p_sky, const CameraMatrix &p_projection, const Transform &p_transform, bool p_vflip, float p_custom_fov, float p_energy, const Basis &p_sky_orientation);

    void _setup_reflections(RenderingEntity *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, const Transform &p_camera_inverse_transform, const CameraMatrix &p_camera_projection, RenderingEntity p_reflection_atlas, RasterizerEnvironmentComponent *p_env);

    void _copy_screen(bool p_invalidate_color = false, bool p_invalidate_depth = false);
    void _copy_texture_to_front_buffer(GLuint p_texture); //used for debug

    void _fill_render_list(Span<RenderingEntity> p_cull_results, bool p_depth_pass, bool p_shadow_pass);

    void _blur_effect_buffer();
    void _render_mrts(RasterizerEnvironmentComponent *env, const CameraMatrix &p_cam_projection);
    void _post_process(RasterizerEnvironmentComponent *env, const CameraMatrix &p_cam_projection);

    void _prepare_depth_texture();
    void _bind_depth_texture();

    void render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, const int p_eye, bool p_cam_ortogonal, Span<RenderingEntity> p_cull_result, RenderingEntity *p_light_cull_result, int p_light_cull_count, RenderingEntity *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RenderingEntity p_environment, RenderingEntity p_shadow_atlas, RenderingEntity p_reflection_atlas, RenderingEntity p_reflection_probe, int p_reflection_probe_pass) override;
    void render_shadow(RenderingEntity p_light, RenderingEntity p_shadow_atlas, int p_pass, Span<RenderingEntity> p_cull_result) override;

    void set_scene_pass(uint64_t p_pass) override;
    void set_debug_draw_mode(RS::ViewportDebugDraw p_debug_draw) override;

    void iteration();
    void initialize();
    void finalize();
    RasterizerSceneGLES3();
    ~RasterizerSceneGLES3() override;
};
