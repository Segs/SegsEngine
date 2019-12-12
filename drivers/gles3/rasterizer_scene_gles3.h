/*************************************************************************/
/*  rasterizer_scene_gles3.h                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#ifndef RASTERIZERSCENEGLES3_H
#define RASTERIZERSCENEGLES3_H

/* Must come before shaders or the Windows build fails... */
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

    RID default_material;
    RID default_material_twosided;
    RID default_shader;
    RID default_shader_twosided;

    RID default_worldcoord_material;
    RID default_worldcoord_material_twosided;
    RID default_worldcoord_shader;
    RID default_worldcoord_shader_twosided;

    RID default_overdraw_material;
    RID default_overdraw_shader;

    RasterizerStorageGLES3 *storage;

    Vector<RasterizerStorageGLES3::RenderTarget::Exposure> exposure_shrink;
    int exposure_shrink_size;

    struct State {

        bool texscreen_copied;
        int current_blend_mode;
        float current_line_width;
        int current_depth_draw;
        bool current_depth_test;
        GLuint current_main_tex;

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
            // make sure this struct is padded to be a multiple of 16 bytes for webgl
            float pad[2];

        } ubo_data;

        GLuint scene_ubo;

        struct EnvironmentRadianceUBO {

            float transform[16];
            float ambient_contribution;
            uint8_t padding[12];

        } env_radiance_data;

        GLuint env_radiance_ubo;

        GLuint sky_verts;
        GLuint sky_array;

        GLuint directional_ubo;

        GLuint spot_array_ubo;
        GLuint omni_array_ubo;
        GLuint reflection_array_ubo;

        GLuint immediate_buffer;
        GLuint immediate_array;

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

        VS::ViewportDebugDraw debug_draw;
    } state;

    /* SHADOW ATLAS API */

    struct ShadowAtlas : public RID_Data {

        enum {
            QUADRANT_SHIFT = 27,
            SHADOW_INDEX_MASK = (1 << QUADRANT_SHIFT) - 1,
            SHADOW_INVALID = 0xFFFFFFFF
        };

        struct Quadrant {

            uint32_t subdivision;

            struct Shadow {
                RID owner;
                uint64_t version;
                uint64_t alloc_tick;

                Shadow() {
                    version = 0;
                    alloc_tick = 0;
                }
            };

            Vector<Shadow> shadows;

            Quadrant() {
                subdivision = 0; //not in use
            }

        } quadrants[4];

        int size_order[4];
        uint32_t smallest_subdiv;

        int size;

        GLuint fbo;
        GLuint depth;

        Map<RID, uint32_t> shadow_owners;
    };

    struct ShadowCubeMap {

        GLuint fbo_id[6];
        GLuint cubemap;
        uint32_t size;
    };

    Vector<ShadowCubeMap> shadow_cubemaps;

    RID_Owner<ShadowAtlas> shadow_atlas_owner;

    RID shadow_atlas_create() override;
    void shadow_atlas_set_size(RID p_atlas, int p_size) override;
    void shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) override;
    bool _shadow_atlas_find_shadow(ShadowAtlas *shadow_atlas, int *p_in_quadrants, int p_quadrant_count, int p_current_subdiv, uint64_t p_tick, int &r_quadrant, int &r_shadow);
    bool shadow_atlas_update_light(RID p_atlas, RID p_light_intance, float p_coverage, uint64_t p_light_version) override;

    struct DirectionalShadow {
        GLuint fbo;
        GLuint depth;
        int light_count;
        int size;
        int current_light;
    } directional_shadow;

    int get_directional_light_shadow_size(RID p_light_intance) override;
    void set_directional_shadow_count(int p_count) override;

    /* REFLECTION PROBE ATLAS API */

    struct ReflectionAtlas : public RID_Data {

        int subdiv;
        int size;

        struct Reflection {
            RID owner;
            uint64_t last_frame;
        };

        GLuint fbo[6];
        GLuint color;

        Vector<Reflection> reflections;
    };

    mutable RID_Owner<ReflectionAtlas> reflection_atlas_owner;

    RID reflection_atlas_create() override;
    void reflection_atlas_set_size(RID p_ref_atlas, int p_size) override;
    void reflection_atlas_set_subdivision(RID p_ref_atlas, int p_subdiv) override;

    /* REFLECTION CUBEMAPS */

    struct ReflectionCubeMap {

        GLuint fbo_id[6];
        GLuint cubemap;
        GLuint depth;
        int size;
    };

    Vector<ReflectionCubeMap> reflection_cubemaps;

    /* REFLECTION PROBE INSTANCE */

    struct ReflectionProbeInstance : public RID_Data {

        RasterizerStorageGLES3::ReflectionProbe *probe_ptr;
        RID probe;
        RID self;
        RID atlas;

        int reflection_atlas_index;

        int render_step;

        uint64_t last_pass;
        int reflection_index;

        Transform transform;
    };

    struct ReflectionProbeDataUBO {

        float box_extents[4];
        float box_ofs[4];
        float params[4]; // intensity, 0, 0, boxproject
        float ambient[4]; //color, probe contrib
        float atlas_clamp[4];
        float local_matrix[16]; //up to here for spot and omni, rest is for directional
        //notes: for ambientblend, use distance to edge to blend between already existing global environment
    };

    mutable RID_Owner<ReflectionProbeInstance> reflection_probe_instance_owner;

    RID reflection_probe_instance_create(RID p_probe) override;
    void reflection_probe_instance_set_transform(RID p_instance, const Transform &p_transform) override;
    void reflection_probe_release_atlas_index(RID p_instance) override;
    bool reflection_probe_instance_needs_redraw(RID p_instance) override;
    bool reflection_probe_instance_has_reflection(RID p_instance) override;
    bool reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas) override;
    bool reflection_probe_instance_postprocess_step(RID p_instance) override;

    /* ENVIRONMENT API */

    struct Environment : public RID_Data {

        VS::EnvironmentBG bg_mode = VS::ENV_BG_CLEAR_COLOR;

        RID sky;
        float sky_custom_fov = 0.0;
        Basis sky_orientation;

        Color bg_color;
        float bg_energy = 1.0;
        float sky_ambient = 0;

        int camera_feed_id = 0;

        Color ambient_color;
        float ambient_energy = 1.0;
        float ambient_sky_contribution = 0.0;

        int canvas_max_layer = 0;

        bool ssr_enabled = false;
        int ssr_max_steps = 64;
        float ssr_fade_in = 0.15f;
        float ssr_fade_out = 2.0f;
        float ssr_depth_tolerance = 0.2f;
        bool ssr_roughness = true;

        bool ssao_enabled = false;
        float ssao_intensity = 1.0f;
        float ssao_radius = 1.0f;
        float ssao_intensity2 = 1.0f;
        float ssao_radius2 = 0.0;
        float ssao_bias = 0.01f;
        float ssao_light_affect = 0;
        float ssao_ao_channel_affect = 0;
        Color ssao_color;
        VS::EnvironmentSSAOQuality ssao_quality = VS::ENV_SSAO_QUALITY_LOW;
        float ssao_bilateral_sharpness = 4;
        VS::EnvironmentSSAOBlur ssao_filter = VS::ENV_SSAO_BLUR_3x3;

        bool glow_enabled = false;
        int glow_levels = (1 << 2) | (1 << 4);
        float glow_intensity = 0.8f;
        float glow_strength = 1.0f;
        float glow_bloom = 0.0;
        VS::EnvironmentGlowBlendMode glow_blend_mode = VS::GLOW_BLEND_MODE_SOFTLIGHT;
        float glow_hdr_bleed_threshold = 1.0f;
        float glow_hdr_bleed_scale = 2.0f;
        float glow_hdr_luminance_cap = 12.0f;
        bool glow_bicubic_upscale = false;

        VS::EnvironmentToneMapper tone_mapper = VS::ENV_TONE_MAPPER_LINEAR;
        float tone_mapper_exposure = 1.0f;
        float tone_mapper_exposure_white = 1.0f;
        bool auto_exposure = false;
        float auto_exposure_speed = 0.5f;
        float auto_exposure_min = 0.05f;
        float auto_exposure_max = 8;
        float auto_exposure_grey = 0.4f;

        bool dof_blur_far_enabled = false;
        float dof_blur_far_distance = 10;
        float dof_blur_far_transition = 5;
        float dof_blur_far_amount = 0.1f;
        VS::EnvironmentDOFBlurQuality dof_blur_far_quality = VS::ENV_DOF_BLUR_QUALITY_MEDIUM;

        bool dof_blur_near_enabled = false;
        float dof_blur_near_distance = 2;
        float dof_blur_near_transition = 1;
        float dof_blur_near_amount = 0.1f;
        VS::EnvironmentDOFBlurQuality dof_blur_near_quality = VS::ENV_DOF_BLUR_QUALITY_MEDIUM;

        bool adjustments_enabled = false;
        float adjustments_brightness = 1.0f;
        float adjustments_contrast = 1.0f;
        float adjustments_saturation = 1.0f;
        RID color_correction;

        bool fog_enabled = false;
        Color fog_color = Color(0.5f, 0.5f, 0.5f);
        Color fog_sun_color = Color(0.8f, 0.8f, 0.0);
        float fog_sun_amount = 0;

        bool fog_depth_enabled = true;
        float fog_depth_begin = 10;
        float fog_depth_end = 0;
        float fog_depth_curve = 1;
        bool fog_transmit_enabled = true;
        float fog_transmit_curve = 1;
        bool fog_height_enabled = false;
        float fog_height_min = 10;
        float fog_height_max = 0;
        float fog_height_curve = 1;
    };

    RID_Owner<Environment> environment_owner;

    RID environment_create() override;

    void environment_set_background(RID p_env, VS::EnvironmentBG p_bg) override;
    void environment_set_sky(RID p_env, RID p_sky) override;
    void environment_set_sky_custom_fov(RID p_env, float p_scale) override;
    void environment_set_sky_orientation(RID p_env, const Basis &p_orientation) override;
    void environment_set_bg_color(RID p_env, const Color &p_color) override;
    void environment_set_bg_energy(RID p_env, float p_energy) override;
    void environment_set_canvas_max_layer(RID p_env, int p_max_layer) override;
    void environment_set_ambient_light(RID p_env, const Color &p_color, float p_energy = 1.0, float p_sky_contribution = 0.0) override;
    void environment_set_camera_feed_id(RID p_env, int p_camera_feed_id) override;

    void environment_set_dof_blur_near(RID p_env, bool p_enable, float p_distance, float p_transition, float p_amount, VS::EnvironmentDOFBlurQuality p_quality) override;
    void environment_set_dof_blur_far(RID p_env, bool p_enable, float p_distance, float p_transition, float p_amount, VS::EnvironmentDOFBlurQuality p_quality) override;
    void environment_set_glow(RID p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, VS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale) override;
    void environment_set_fog(RID p_env, bool p_enable, float p_begin, float p_end, RID p_gradient_texture) override;

    void environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_in, float p_fade_out, float p_depth_tolerance, bool p_roughness) override;
    void environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, VS::EnvironmentSSAOQuality p_quality, VS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) override;

    void environment_set_tonemap(RID p_env, VS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale) override;

    void environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RID p_ramp) override;

    void environment_set_fog(RID p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) override;
    void environment_set_fog_depth(RID p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) override;
    void environment_set_fog_height(RID p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) override;

    bool is_environment(RID p_env) override;

    VS::EnvironmentBG environment_get_background(RID p_env) override;
    int environment_get_canvas_max_layer(RID p_env) override;

    /* LIGHT INSTANCE */

    struct LightDataUBO {

        float light_pos_inv_radius[4];
        float light_direction_attenuation[4];
        float light_color_energy[4];
        float light_params[4]; //spot attenuation, spot angle, specular, shadow enabled
        float light_clamp[4];
        float light_shadow_color_contact[4];
        union {
            struct {
                float matrix1[16]; //up to here for spot and omni, rest is for directional
                float matrix2[16];
                float matrix3[16];
                float matrix4[16];
            };
            float matrix[4 * 16];
        } shadow;
        float shadow_split_offsets[4];
    };

    struct LightInstance : public RID_Data {

        struct ShadowTransform {

            CameraMatrix camera;
            Transform transform;
            float farplane;
            float split;
            float bias_scale;
        };

        ShadowTransform shadow_transform[4];

        RID self;
        RID light;
        RasterizerStorageGLES3::Light *light_ptr;
        Transform transform;

        Vector3 light_vector;
        Vector3 spot_vector;
        float linear_att;

        uint64_t shadow_pass;
        uint64_t last_scene_pass;
        uint64_t last_scene_shadow_pass;
        uint64_t last_pass;
        uint16_t light_index;
        uint16_t light_directional_index;

        uint32_t current_shadow_atlas_key;

        Vector2 dp;

        Rect2 directional_rect;

        Set<RID> shadow_atlases; //shadow atlases where this light is registered

        LightInstance() {}
    };

    mutable RID_Owner<LightInstance> light_instance_owner;

    RID light_instance_create(RID p_light) override;
    void light_instance_set_transform(RID p_light_instance, const Transform &p_transform) override;
    void light_instance_set_shadow_transform(RID p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale = 1.0) override;
    void light_instance_mark_visible(RID p_light_instance) override;

    /* REFLECTION INSTANCE */

    struct GIProbeInstance : public RID_Data {
        RID data;
        RasterizerStorageGLES3::GIProbe *probe;
        GLuint tex_cache;
        Vector3 cell_size_cache;
        Vector3 bounds;
        Transform transform_to_data;

        GIProbeInstance() :
                probe(nullptr),
                tex_cache(0) {
        }
    };

    mutable RID_Owner<GIProbeInstance> gi_probe_instance_owner;

    RID gi_probe_instance_create() override;
    void gi_probe_instance_set_light_data(RID p_probe, RID p_base, RID p_data) override;
    void gi_probe_instance_set_transform_to_data(RID p_probe, const Transform &p_xform) override;
    void gi_probe_instance_set_bounds(RID p_probe, const Vector3 &p_bounds) override;

    /* RENDER LIST */

    struct RenderList {

        enum {
            DEFAULT_MAX_ELEMENTS = 65536,
            SORT_FLAG_SKELETON = 1,
            SORT_FLAG_INSTANCING = 2,
            MAX_DIRECTIONAL_LIGHTS = 16,
            DEFAULT_MAX_LIGHTS = 4096,
            DEFAULT_MAX_REFLECTIONS = 1024,

            SORT_KEY_PRIORITY_SHIFT = 56,
            SORT_KEY_PRIORITY_MASK = 0xFF,
            //depth layer for opaque (56-52)
            SORT_KEY_OPAQUE_DEPTH_LAYER_SHIFT = 52,
            SORT_KEY_OPAQUE_DEPTH_LAYER_MASK = 0xF,
//64 bits unsupported in MSVC
#define SORT_KEY_UNSHADED_FLAG (uint64_t(1) << 49)
#define SORT_KEY_NO_DIRECTIONAL_FLAG (uint64_t(1) << 48)
#define SORT_KEY_LIGHTMAP_CAPTURE_FLAG (uint64_t(1) << 47)
#define SORT_KEY_LIGHTMAP_FLAG (uint64_t(1) << 46)
#define SORT_KEY_GI_PROBES_FLAG (uint64_t(1) << 45)
#define SORT_KEY_VERTEX_LIT_FLAG (uint64_t(1) << 44)
            SORT_KEY_SHADING_SHIFT = 44,
            SORT_KEY_SHADING_MASK = 63,
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

        int max_elements;
        int max_lights;
        int max_reflections;

        struct Element {

            RasterizerScene::InstanceBase *instance;
            RasterizerStorageGLES3::Geometry *geometry;
            RasterizerStorageGLES3::Material *material;
            RasterizerStorageGLES3::GeometryOwner *owner;
            uint64_t sort_key;
        };

        Element *base_elements;
        Element **elements;

        int element_count;
        int alpha_element_count;

        void clear() {

            element_count = 0;
            alpha_element_count = 0;
        }

        //should eventually be replaced by radix

        struct SortByKey {

            _FORCE_INLINE_ bool operator()(const Element *A, const Element *B) const {
                return A->sort_key < B->sort_key;
            }
        };

        void sort_by_key(bool p_alpha) {

            SortArray<Element *, SortByKey> sorter;
            if (p_alpha) {
                sorter.sort(&elements[max_elements - alpha_element_count], alpha_element_count);
            } else {
                sorter.sort(elements, element_count);
            }
        }

        struct SortByDepth {

            _FORCE_INLINE_ bool operator()(const Element *A, const Element *B) const {
                return A->instance->depth < B->instance->depth;
            }
        };

        void sort_by_depth(bool p_alpha) { //used for shadows

            SortArray<Element *, SortByDepth> sorter;
            if (p_alpha) {
                sorter.sort(&elements[max_elements - alpha_element_count], alpha_element_count);
            } else {
                sorter.sort(elements, element_count);
            }
        }

        struct SortByReverseDepthAndPriority {

            _FORCE_INLINE_ bool operator()(const Element *A, const Element *B) const {
                uint32_t layer_A = uint32_t(A->sort_key >> SORT_KEY_PRIORITY_SHIFT);
                uint32_t layer_B = uint32_t(B->sort_key >> SORT_KEY_PRIORITY_SHIFT);
                if (layer_A == layer_B) {
                    return A->instance->depth > B->instance->depth;
                } else {
                    return layer_A < layer_B;
                }
            }
        };

        void sort_by_reverse_depth_and_priority(bool p_alpha) { //used for alpha

            SortArray<Element *, SortByReverseDepthAndPriority> sorter;
            if (p_alpha) {
                sorter.sort(&elements[max_elements - alpha_element_count], alpha_element_count);
            } else {
                sorter.sort(elements, element_count);
            }
        }

        _FORCE_INLINE_ Element *add_element() {

            if (element_count + alpha_element_count >= max_elements)
                return nullptr;
            elements[element_count] = &base_elements[element_count];
            return elements[element_count++];
        }

        _FORCE_INLINE_ Element *add_alpha_element() {

            if (element_count + alpha_element_count >= max_elements)
                return nullptr;
            int idx = max_elements - alpha_element_count - 1;
            elements[idx] = &base_elements[idx];
            alpha_element_count++;
            return elements[idx];
        }

        void init() {

            element_count = 0;
            alpha_element_count = 0;
            elements = memnew_arr(Element *, max_elements);
            base_elements = memnew_arr(Element, max_elements);
            for (int i = 0; i < max_elements; i++)
                elements[i] = &base_elements[i]; // assign elements
        }

        RenderList() {

            max_elements = DEFAULT_MAX_ELEMENTS;
            max_lights = DEFAULT_MAX_LIGHTS;
            max_reflections = DEFAULT_MAX_REFLECTIONS;
        }

        ~RenderList() {
            memdelete_arr(elements);
            memdelete_arr(base_elements);
        }
    };

    LightInstance *directional_light;
    LightInstance *directional_lights[RenderList::MAX_DIRECTIONAL_LIGHTS];

    RenderList render_list;

    _FORCE_INLINE_ void _set_cull(bool p_front, bool p_disabled, bool p_reverse_cull);

    _FORCE_INLINE_ bool _setup_material(RasterizerStorageGLES3::Material *p_material, bool p_depth_pass, bool p_alpha_pass);
    _FORCE_INLINE_ void _setup_geometry(RenderList::Element *e, const Transform &p_view_transform);
    _FORCE_INLINE_ void _render_geometry(RenderList::Element *e);
    _FORCE_INLINE_ void _setup_light(RenderList::Element *e, const Transform &p_view_transform);

    void _render_list(RenderList::Element **p_elements, int p_element_count, const Transform &p_view_transform, const CameraMatrix &p_projection, RasterizerStorageGLES3::Sky *p_sky, bool p_reverse_cull, bool p_alpha_pass, bool p_shadow, bool p_directional_add, bool p_directional_shadows);

    _FORCE_INLINE_ void _add_geometry(RasterizerStorageGLES3::Geometry *p_geometry, InstanceBase *p_instance, RasterizerStorageGLES3::GeometryOwner *p_owner, int p_material, bool p_depth_pass, bool p_shadow_pass);

    _FORCE_INLINE_ void _add_geometry_with_material(RasterizerStorageGLES3::Geometry *p_geometry, InstanceBase *p_instance, RasterizerStorageGLES3::GeometryOwner *p_owner, RasterizerStorageGLES3::Material *p_material, bool p_depth_pass, bool p_shadow_pass);

    void _draw_sky(RasterizerStorageGLES3::Sky *p_sky, const CameraMatrix &p_projection, const Transform &p_transform, bool p_vflip, float p_custom_fov, float p_energy, const Basis &p_sky_orientation);

    void _setup_environment(Environment *env, const CameraMatrix &p_cam_projection, const Transform &p_cam_transform, bool p_no_fog = false);
    void _setup_directional_light(int p_index, const Transform &p_camera_inverse_transform, bool p_use_shadows);
    void _setup_lights(RID *p_light_cull_result, int p_light_cull_count, const Transform &p_camera_inverse_transform, const CameraMatrix &p_camera_projection, RID p_shadow_atlas);
    void _setup_reflections(RID *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, const Transform &p_camera_inverse_transform, const CameraMatrix &p_camera_projection, RID p_reflection_atlas, Environment *p_env);

    void _copy_screen(bool p_invalidate_color = false, bool p_invalidate_depth = false);
    void _copy_texture_to_front_buffer(GLuint p_texture); //used for debug

    void _fill_render_list(InstanceBase **p_cull_result, int p_cull_count, bool p_depth_pass, bool p_shadow_pass);

    void _blur_effect_buffer();
    void _render_mrts(Environment *env, const CameraMatrix &p_cam_projection);
    void _post_process(Environment *env, const CameraMatrix &p_cam_projection);

    void _prepare_depth_texture();
    void _bind_depth_texture();

    void render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_ortogonal, InstanceBase **p_cull_result, int p_cull_count, RID *p_light_cull_result, int p_light_cull_count, RID *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RID p_environment, RID p_shadow_atlas, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass) override;
    void render_shadow(RID p_light, RID p_shadow_atlas, int p_pass, InstanceBase **p_cull_result, int p_cull_count) override;
    bool free(RID p_rid) override;

    void set_scene_pass(uint64_t p_pass) override;
    void set_debug_draw_mode(VS::ViewportDebugDraw p_debug_draw) override;

    void iteration();
    void initialize();
    void finalize();
    RasterizerSceneGLES3();
    ~RasterizerSceneGLES3() override;
};

#endif // RASTERIZERSCENEGLES3_H
