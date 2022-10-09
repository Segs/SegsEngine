#pragma once
#include "core/engine_entities.h"
#include "rasterizer_gl_unique_handle.h"

#include "core/math/transform.h"
#include "core/pool_vector.h"

struct RasterizerGIProbeComponent {

    AABB bounds;
    Transform to_cell;
    PoolVector<int> dynamic_data;
    float cell_size;

    int dynamic_range;
    float energy;
    float bias;
    float normal_bias;
    float propagation;
    uint32_t version;

    bool interior;
    bool compress;

};

struct RasterizerGIProbeDataComponent {

    int width;
    int height;
    int depth;
    int levels;
    GLTextureHandle tex_id;
    RasterizerGIProbeDataComponent(const RasterizerGIProbeDataComponent &) = delete;
    RasterizerGIProbeDataComponent &operator=(const RasterizerGIProbeDataComponent &) = delete;

    RasterizerGIProbeDataComponent(RasterizerGIProbeDataComponent &&f) = default;
    RasterizerGIProbeDataComponent &operator=(RasterizerGIProbeDataComponent &&f) = default;

    RasterizerGIProbeDataComponent() { }
};

struct RasterizerGIProbeInstance {
    RenderingEntity data;
    RenderingEntity probe = entt::null; //RasterizerGIProbeComponent *
    GLNonOwningHandle tex_cache;
    Vector3 cell_size_cache;
    Vector3 bounds;
    Transform transform_to_data;
};
RenderingEntity gi_probe_instance_create();
void gi_probe_instance_set_light_data(RenderingEntity p_probe, RenderingEntity p_base, RenderingEntity p_data);
void gi_probe_instance_set_transform_to_data(RenderingEntity p_probe, const Transform &p_xform);
void gi_probe_instance_set_bounds(RenderingEntity p_probe, Vector3 p_bounds);
bool setup_probes(RenderingEntity inst, struct SceneShaderGLES3 &, int max_texture_image_units, const Transform &p_view_transform, bool p_async_in_use);
