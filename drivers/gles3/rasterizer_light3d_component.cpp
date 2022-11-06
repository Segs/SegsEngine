#include "rasterizer_light3d_component.h"

#include "drivers/gles3/rasterizer_dependent_entities_component.h"
#include "rasterizer_storage_gles3.h"
#include "servers/rendering/render_entity_getter.h"

/* Light3D API */

RenderingEntity RasterizerStorageGLES3::light_create(RS::LightType p_type) {
    auto res = VSG::ecs->create();
    RasterizerLight3DComponent &light(VSG::ecs->registry.emplace<RasterizerLight3DComponent>(res));
    VSG::ecs->registry.emplace<RasterizerInstantiableComponent>(res);

    light.type = p_type;

    light.param[RS::LIGHT_PARAM_ENERGY] = 1.0f;
    light.param[RS::LIGHT_PARAM_INDIRECT_ENERGY] = 1.0f;
    light.param[RS::LIGHT_PARAM_SIZE] = 0.0;
    light.param[RS::LIGHT_PARAM_SPECULAR] = 0.5f;
    light.param[RS::LIGHT_PARAM_RANGE] = 1.0f;
    light.param[RS::LIGHT_PARAM_SPOT_ANGLE] = 45;
    light.param[RS::LIGHT_PARAM_CONTACT_SHADOW_SIZE] = 45;
    light.param[RS::LIGHT_PARAM_SHADOW_MAX_DISTANCE] = 0;
    light.param[RS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET] = 0.1f;
    light.param[RS::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET] = 0.3f;
    light.param[RS::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET] = 0.6f;
    light.param[RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS] = 0.1f;
    light.param[RS::LIGHT_PARAM_SHADOW_BIAS_SPLIT_SCALE] = 0.1f;

    light.color = Color(1, 1, 1, 1);
    light.shadow = false;
    light.negative = false;
    light.cull_mask = 0xFFFFFFFF;
    light.directional_shadow_mode = RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL;
    light.omni_shadow_mode = RS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID;
    light.omni_shadow_detail = RS::LIGHT_OMNI_SHADOW_DETAIL_VERTICAL;
    light.directional_blend_splits = false;
    light.directional_range_mode = RS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE;
    light.reverse_cull = false;
    light.bake_mode = RS::LIGHT_BAKE_INDIRECT;
    light.version = 0;

    return res;
}

void RasterizerStorageGLES3::light_set_color(RenderingEntity p_light, const Color &p_color) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);

    light->color = p_color;
}
void RasterizerStorageGLES3::light_set_param(RenderingEntity p_light, RS::LightParam p_param, float p_value) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);
    ERR_FAIL_INDEX(p_param, RS::LIGHT_PARAM_MAX);
    auto *inst = getUnchecked<RasterizerInstantiableComponent>(p_light);
    switch (p_param) {
        case RS::LIGHT_PARAM_RANGE:
        case RS::LIGHT_PARAM_SPOT_ANGLE:
        case RS::LIGHT_PARAM_SHADOW_MAX_DISTANCE:
        case RS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET:
        case RS::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET:
        case RS::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET:
        case RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS:
        case RS::LIGHT_PARAM_SHADOW_BIAS: {

            light->version++;
            inst->instance_change_notify(true, false);
        } break;
        default: {
        }
    }

    light->param[p_param] = p_value;
}
void RasterizerStorageGLES3::light_set_shadow(RenderingEntity p_light, bool p_enabled) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);
    light->shadow = p_enabled;
    light->version++;

    auto *inst = &VSG::ecs->registry.get<RasterizerInstantiableComponent>(p_light);
    inst->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_set_shadow_color(RenderingEntity p_light, const Color &p_color) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);
    light->shadow_color = p_color;
}

void RasterizerStorageGLES3::light_set_projector(RenderingEntity p_light, RenderingEntity p_texture) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);

    light->projector = p_texture;
}

void RasterizerStorageGLES3::light_set_negative(RenderingEntity p_light, bool p_enable) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);

    light->negative = p_enable;
}
void RasterizerStorageGLES3::light_set_cull_mask(RenderingEntity p_light, uint32_t p_mask) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);

    ERR_FAIL_COND(!light);

    light->cull_mask = p_mask;

    light->version++;
    auto *inst = &VSG::ecs->registry.get<RasterizerInstantiableComponent>(p_light);
    inst->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_set_reverse_cull_face_mode(RenderingEntity p_light, bool p_enabled) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);

    light->reverse_cull = p_enabled;

    light->version++;
    auto *inst = &VSG::ecs->registry.get<RasterizerInstantiableComponent>(p_light);
    inst->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_set_use_gi(RenderingEntity p_light, bool p_enabled) {
    WARN_DEPRECATED_MSG("'VisualServer.light_set_use_gi' is deprecated and will be removed in a future version. Use 'VisualServer.light_set_bake_mode' instead.");
    light_set_bake_mode(p_light, p_enabled ? RS::LightBakeMode::LIGHT_BAKE_INDIRECT : RS::LightBakeMode::LIGHT_BAKE_DISABLED);
}

void RasterizerStorageGLES3::light_set_bake_mode(RenderingEntity p_light, RS::LightBakeMode p_bake_mode) {
    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);

    light->bake_mode = p_bake_mode;

    light->version++;
    auto *inst = &VSG::ecs->registry.get<RasterizerInstantiableComponent>(p_light);
    inst->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_omni_set_shadow_mode(RenderingEntity p_light, RS::LightOmniShadowMode p_mode) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);

    light->omni_shadow_mode = p_mode;

    light->version++;
    auto *inst = &VSG::ecs->registry.get<RasterizerInstantiableComponent>(p_light);
    inst->instance_change_notify(true, false);
}

RS::LightOmniShadowMode RasterizerStorageGLES3::light_omni_get_shadow_mode(RenderingEntity p_light) {

    const RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, RS::LIGHT_OMNI_SHADOW_CUBE);

    return light->omni_shadow_mode;
}

void RasterizerStorageGLES3::light_omni_set_shadow_detail(RenderingEntity p_light, RS::LightOmniShadowDetail p_detail) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);

    light->omni_shadow_detail = p_detail;
    light->version++;
    auto *inst = &VSG::ecs->registry.get<RasterizerInstantiableComponent>(p_light);
    inst->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_directional_set_shadow_mode(RenderingEntity p_light, RS::LightDirectionalShadowMode p_mode) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);

    light->directional_shadow_mode = p_mode;
    light->version++;
    auto *inst = &VSG::ecs->registry.get<RasterizerInstantiableComponent>(p_light);
    inst->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_directional_set_blend_splits(RenderingEntity p_light, bool p_enable) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);

    light->directional_blend_splits = p_enable;
    light->version++;
    auto *inst = &VSG::ecs->registry.get<RasterizerInstantiableComponent>(p_light);
    inst->instance_change_notify(true, false);
}

bool RasterizerStorageGLES3::light_directional_get_blend_splits(RenderingEntity p_light) const {

    const RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, false);

    return light->directional_blend_splits;
}

RS::LightDirectionalShadowMode RasterizerStorageGLES3::light_directional_get_shadow_mode(RenderingEntity p_light) {

    const RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL);

    return light->directional_shadow_mode;
}

void RasterizerStorageGLES3::light_directional_set_shadow_depth_range_mode(RenderingEntity p_light, RS::LightDirectionalShadowDepthRangeMode p_range_mode) {

    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND(!light);

    light->directional_range_mode = p_range_mode;
}

RS::LightDirectionalShadowDepthRangeMode RasterizerStorageGLES3::light_directional_get_shadow_depth_range_mode(RenderingEntity p_light) const {

    const RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, RS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE);

    return light->directional_range_mode;
}

RS::LightType RasterizerStorageGLES3::light_get_type(RenderingEntity p_light) const {

    const RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, RS::LIGHT_DIRECTIONAL);

    return light->type;
}

float RasterizerStorageGLES3::light_get_param(RenderingEntity p_light, RS::LightParam p_param) {

    const RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, RS::LIGHT_DIRECTIONAL);

    return light->param[p_param];
}

Color RasterizerStorageGLES3::light_get_color(RenderingEntity p_light) {

    const RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, Color());

    return light->color;
}

bool RasterizerStorageGLES3::light_get_use_gi(RenderingEntity p_light) {
    return light_get_bake_mode(p_light) != RS::LightBakeMode::LIGHT_BAKE_DISABLED;
}

RS::LightBakeMode RasterizerStorageGLES3::light_get_bake_mode(RenderingEntity p_light) {
    RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, RS::LightBakeMode::LIGHT_BAKE_DISABLED);

    return light->bake_mode;
}

bool RasterizerStorageGLES3::light_has_shadow(RenderingEntity p_light) const {

    const RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, RS::LIGHT_DIRECTIONAL);

    return light->shadow;
}

uint64_t RasterizerStorageGLES3::light_get_version(RenderingEntity p_light) const {

    const RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, 0);

    return light->version;
}

AABB RasterizerStorageGLES3::light_get_aabb(RenderingEntity p_light) const {

    const RasterizerLight3DComponent *light = get<RasterizerLight3DComponent>(p_light);
    ERR_FAIL_COND_V(!light, AABB());

    switch (light->type) {

        case RS::LIGHT_SPOT: {

            float len = light->param[RS::LIGHT_PARAM_RANGE];
            float size = Math::tan(Math::deg2rad(light->param[RS::LIGHT_PARAM_SPOT_ANGLE])) * len;
            return AABB(Vector3(-size, -size, -len), Vector3(size * 2, size * 2, len));
        }
        case RS::LIGHT_OMNI: {

            float r = light->param[RS::LIGHT_PARAM_RANGE];
            return AABB(-Vector3(r, r, r), Vector3(r, r, r) * 2);
        }
        case RS::LIGHT_DIRECTIONAL: {

            return AABB();
        }
    }

    ERR_FAIL_V(AABB());
}
