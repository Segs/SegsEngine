#pragma once

#include "core/math/aabb.h"
#include "servers/rendering/render_entity_helpers.h"
#include "servers/rendering_server_enums.h"

struct RasterizerMultiMeshComponent;

struct RasterizerMeshComponent {
    bool active=false;
    Vector<RenderingEntity> surfaces;
    AABB custom_aabb;
    //! a container that records all entities with a multimesh that uses this.
    Vector<RenderingEntity> multimeshes;
    Vector<float> blend_shape_values;
    mutable uint64_t last_pass=0;
    int blend_shape_count=0;
    RS::BlendShapeMode blend_shape_mode=RS::BLEND_SHAPE_MODE_NORMALIZED;
    MoveOnlyEntityHandle self;

    void update_multimeshes();

    RasterizerMeshComponent(const RasterizerMeshComponent&)=delete;
    RasterizerMeshComponent &operator=(const RasterizerMeshComponent&)=delete;

    RasterizerMeshComponent(RasterizerMeshComponent&& f) noexcept {
        *this = eastl::move(f);
    }
    RasterizerMeshComponent &operator=(RasterizerMeshComponent &&from) noexcept;

    RasterizerMeshComponent() = default;
    ~RasterizerMeshComponent();

};

AABB mesh_get_aabb(const RasterizerMeshComponent *mesh, RenderingEntity p_skeleton);
