#pragma once

#include "core/engine_entities.h"
#include "core/color.h"
#include "drivers/gles3/rasterizer_gl_unique_handle.h"
#include "servers/rendering_server_enums.h"

//: Instantiable
struct RasterizerLight3DComponent  {
    float param[RS::LIGHT_PARAM_MAX];
    Color color;
    Color shadow_color;
    RenderingEntity projector;
    uint64_t version;
    uint32_t cull_mask;
    RS::LightType type;
    RS::LightOmniShadowMode omni_shadow_mode;
    RS::LightOmniShadowDetail omni_shadow_detail;
    RS::LightDirectionalShadowMode directional_shadow_mode;
    RS::LightDirectionalShadowDepthRangeMode directional_range_mode;
    RS::LightBakeMode bake_mode;
    bool shadow : 1;
    bool negative : 1;
    bool reverse_cull : 1;
    bool directional_blend_splits : 1;
};

struct RasterizerLightInternalComponent {

    struct UBOData {

        float light_matrix[16];
        float local_matrix[16];
        float shadow_matrix[16];
        float color[4];
        float shadow_color[4];
        float light_pos[2];
        float shadowpixel_size;
        float shadow_gradient;
        float light_height;
        float light_outside_alpha;
        float shadow_distance_mult;
        uint8_t padding[4];
    } ubo_data;

    GLBufferHandle ubo;
};
