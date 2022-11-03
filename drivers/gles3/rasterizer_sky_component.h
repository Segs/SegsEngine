#pragma once

#include "core/engine_entities.h"
#include "servers/rendering/render_entity_helpers.h"
#include "rasterizer_gl_unique_handle.h"

struct RasterizerSkyComponent {
    MoveOnlyEntityHandle panorama;
    GLTextureHandle radiance;
    GLTextureHandle irradiance;
    int radiance_size;

    RasterizerSkyComponent(const RasterizerSkyComponent &) = delete;
    RasterizerSkyComponent &operator=(const RasterizerSkyComponent &) = delete;

    RasterizerSkyComponent(RasterizerSkyComponent &&) = default;
    RasterizerSkyComponent &operator=(RasterizerSkyComponent &&) = default;
};
