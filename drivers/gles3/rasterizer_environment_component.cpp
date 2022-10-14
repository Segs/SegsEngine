#include "rasterizer_environment_component.h"

#include "rasterizer_scene_gles3.h"

/* ENVIRONMENT API */

RenderingEntity RasterizerSceneGLES3::environment_create() {
    return VSG::ecs->create<RasterizerEnvironmentComponent>();
}

void RasterizerSceneGLES3::environment_set_background(RenderingEntity p_env, RS::EnvironmentBG p_bg) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);
    env->bg_mode = p_bg;
}

void RasterizerSceneGLES3::environment_set_sky(RenderingEntity p_env, RenderingEntity p_sky) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->sky = p_sky;
}

void RasterizerSceneGLES3::environment_set_sky_custom_fov(RenderingEntity p_env, float p_scale) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->sky_custom_fov = p_scale;
}

void RasterizerSceneGLES3::environment_set_sky_orientation(RenderingEntity p_env, const Basis &p_orientation) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->sky_orientation = p_orientation;
}

void RasterizerSceneGLES3::environment_set_bg_color(RenderingEntity p_env, const Color &p_color) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->bg_color = p_color;
}
void RasterizerSceneGLES3::environment_set_bg_energy(RenderingEntity p_env, float p_energy) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->bg_energy = p_energy;
}

void RasterizerSceneGLES3::environment_set_canvas_max_layer(RenderingEntity p_env, int p_max_layer) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->canvas_max_layer = p_max_layer;
}
void RasterizerSceneGLES3::environment_set_ambient_light(RenderingEntity p_env, const Color &p_color, float p_energy, float p_sky_contribution) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->ambient_color = p_color;
    env->ambient_energy = p_energy;
    env->ambient_sky_contribution = p_sky_contribution;
}
void RasterizerSceneGLES3::environment_set_camera_feed_id(RenderingEntity p_env, int p_camera_feed_id) {
    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->camera_feed_id = p_camera_feed_id;
}

void RasterizerSceneGLES3::environment_set_dof_blur_far(RenderingEntity p_env, bool p_enable, float p_distance, float p_transition, float p_amount, RS::EnvironmentDOFBlurQuality p_quality) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->dof_blur_far_enabled = p_enable;
    env->dof_blur_far_distance = p_distance;
    env->dof_blur_far_transition = p_transition;
    env->dof_blur_far_amount = p_amount;
    env->dof_blur_far_quality = p_quality;
}

void RasterizerSceneGLES3::environment_set_dof_blur_near(RenderingEntity p_env, bool p_enable, float p_distance, float p_transition, float p_amount, RS::EnvironmentDOFBlurQuality p_quality) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->dof_blur_near_enabled = p_enable;
    env->dof_blur_near_distance = p_distance;
    env->dof_blur_near_transition = p_transition;
    env->dof_blur_near_amount = p_amount;
    env->dof_blur_near_quality = p_quality;
}
void RasterizerSceneGLES3::environment_set_glow(RenderingEntity p_env, bool p_enable, int p_level_flags,
        float p_intensity, float p_strength, float p_bloom_threshold, RS::EnvironmentGlowBlendMode p_blend_mode,
        float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale,
        bool p_high_quality) {
    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->glow_enabled = p_enable;
    env->glow_levels = p_level_flags;
    env->glow_intensity = p_intensity;
    env->glow_strength = p_strength;
    env->glow_bloom = p_bloom_threshold;
    env->glow_blend_mode = p_blend_mode;
    env->glow_hdr_bleed_threshold = p_hdr_bleed_threshold;
    env->glow_hdr_bleed_scale = p_hdr_bleed_scale;
    env->glow_hdr_luminance_cap = p_hdr_luminance_cap;
    env->glow_bicubic_upscale = p_bicubic_upscale;
    env->glow_high_quality = p_high_quality;
}

void RasterizerSceneGLES3::environment_set_fog(RenderingEntity p_env, bool p_enable, float p_begin, float p_end, RenderingEntity p_gradient_texture) {
}

void RasterizerSceneGLES3::environment_set_ssr(RenderingEntity p_env, bool p_enable, int p_max_steps, float p_fade_in, float p_fade_out, float p_depth_tolerance, bool p_roughness) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->ssr_enabled = p_enable;
    env->ssr_max_steps = p_max_steps;
    env->ssr_fade_in = p_fade_in;
    env->ssr_fade_out = p_fade_out;
    env->ssr_depth_tolerance = p_depth_tolerance;
    env->ssr_roughness = p_roughness;
}

void RasterizerSceneGLES3::environment_set_ssao(RenderingEntity p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, RS::EnvironmentSSAOQuality p_quality, RS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->ssao_enabled = p_enable;
    env->ssao_radius = p_radius;
    env->ssao_intensity = p_intensity;
    env->ssao_radius2 = p_radius2;
    env->ssao_intensity2 = p_intensity2;
    env->ssao_bias = p_bias;
    env->ssao_light_affect = p_light_affect;
    env->ssao_ao_channel_affect = p_ao_channel_affect;
    env->ssao_color = p_color;
    env->ssao_filter = p_blur;
    env->ssao_quality = p_quality;
    env->ssao_bilateral_sharpness = p_bilateral_sharpness;
}

void RasterizerSceneGLES3::environment_set_tonemap(RenderingEntity p_env, RS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->tone_mapper = p_tone_mapper;
    env->tone_mapper_exposure = p_exposure;
    env->tone_mapper_exposure_white = p_white;
    env->auto_exposure = p_auto_exposure;
    env->auto_exposure_speed = p_auto_exp_speed;
    env->auto_exposure_min = p_min_luminance;
    env->auto_exposure_max = p_max_luminance;
    env->auto_exposure_grey = p_auto_exp_scale;
}

void RasterizerSceneGLES3::environment_set_adjustment(RenderingEntity p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RenderingEntity p_ramp) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->adjustments_enabled = p_enable;
    env->adjustments_brightness = p_brightness;
    env->adjustments_contrast = p_contrast;
    env->adjustments_saturation = p_saturation;
    env->color_correction = p_ramp;
}

void RasterizerSceneGLES3::environment_set_fog(RenderingEntity p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->fog_enabled = p_enable;
    env->fog_color = p_color;
    env->fog_sun_color = p_sun_color;
    env->fog_sun_amount = p_sun_amount;
}

void RasterizerSceneGLES3::environment_set_fog_depth(RenderingEntity p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->fog_depth_enabled = p_enable;
    env->fog_depth_begin = p_depth_begin;
    env->fog_depth_end = p_depth_end;
    env->fog_depth_curve = p_depth_curve;
    env->fog_transmit_enabled = p_transmit;
    env->fog_transmit_curve = p_transmit_curve;
}

void RasterizerSceneGLES3::environment_set_fog_height(RenderingEntity p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) {

    auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND(!env);

    env->fog_height_enabled = p_enable;
    env->fog_height_min = p_min_height;
    env->fog_height_max = p_max_height;
    env->fog_height_curve = p_height_curve;
}

bool is_environment(RenderingEntity p_env) {

    return VSG::ecs->registry.any_of<RasterizerEnvironmentComponent>(p_env);
}

RS::EnvironmentBG RasterizerSceneGLES3::environment_get_background(RenderingEntity p_env) {

    const auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND_V(!env, RS::ENV_BG_MAX);

    return env->bg_mode;
}

int RasterizerSceneGLES3::environment_get_canvas_max_layer(RenderingEntity p_env) {

    const auto *env = VSG::ecs->registry.try_get<RasterizerEnvironmentComponent>(p_env);
    ERR_FAIL_COND_V(!env, -1);

    return env->canvas_max_layer;
}
