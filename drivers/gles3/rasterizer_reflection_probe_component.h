#pragma once

#include "core/color.h"
#include "core/math/vector3.h"
#include "core/math/transform.h"
#include "servers/rendering_server_enums.h"
#include "servers/rendering/render_entity_helpers.h"
#include "core/forward_decls.h"

struct RasterizerReflectionCubeMap;
class CubeToDpShaderGLES3;
class RasterizerStorageGLES3;
class RasterizerSceneGLES3;


struct RasterizerReflectionProbeComponent {

    Color interior_ambient = {};
    Vector3 extents {1, 1, 1};
    Vector3 origin_offset {0, 0, 0};
    float intensity=1.0f;
    float interior_ambient_energy=1.0f;
    float interior_ambient_probe_contrib=0.0f;
    float max_distance=0.0f;
    uint32_t cull_mask = (1 << 20) - 1;
    RS::ReflectionProbeUpdateMode update_mode=RS::REFLECTION_PROBE_UPDATE_ONCE;
    bool interior : 1;
    bool box_projection: 1;
    bool enable_shadows: 1;
};

struct RasterizerReflectionProbeInstanceComponent {

    MoveOnlyEntityHandle probe;
    MoveOnlyEntityHandle self;
    MoveOnlyEntityHandle atlas;

    int reflection_atlas_index=-1;

    int render_step=-1;

    uint64_t last_pass=0;
    int reflection_index;

    Transform transform;
    void release_atlas_index();
    RasterizerReflectionProbeInstanceComponent(const RasterizerReflectionProbeInstanceComponent&) = delete;
    RasterizerReflectionProbeInstanceComponent &operator=(const RasterizerReflectionProbeInstanceComponent&) = delete;

    RasterizerReflectionProbeInstanceComponent(RasterizerReflectionProbeInstanceComponent &&) = default;
    RasterizerReflectionProbeInstanceComponent &operator=(RasterizerReflectionProbeInstanceComponent&&from) {
        release_atlas_index();
        probe = eastl::move(from.probe);
        self = eastl::move(from.self);
        atlas = eastl::move(from.atlas);

        reflection_atlas_index=from.reflection_atlas_index;
        render_step=from.render_step;
        last_pass=from.last_pass;
        reflection_index=from.reflection_index;
        transform = from.transform;

        from.render_step=-1;
        return *this;
    }

    ~RasterizerReflectionProbeInstanceComponent();
};
