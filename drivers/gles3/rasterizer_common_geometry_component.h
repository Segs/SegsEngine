#pragma once

#include "core/engine_entities.h"
#include "servers/rendering/render_entity_helpers.h"

struct RasterizerCommonGeometryComponent {
    enum Type {
        GEOMETRY_INVALID,
        GEOMETRY_SURFACE,
        GEOMETRY_IMMEDIATE,
        GEOMETRY_MULTISURFACE,
        };
    MoveOnlyEntityHandle material=entt::null;
    uint64_t last_pass = 0;
    uint32_t index = 0;
    Type type;

    RasterizerCommonGeometryComponent(const RasterizerCommonGeometryComponent &) = delete;
    RasterizerCommonGeometryComponent &operator=(const RasterizerCommonGeometryComponent &)=delete;

    RasterizerCommonGeometryComponent(RasterizerCommonGeometryComponent &&) = default;
    RasterizerCommonGeometryComponent &operator=(RasterizerCommonGeometryComponent &&) = default;

    RasterizerCommonGeometryComponent(Type mt) : type(mt) {}
    ~RasterizerCommonGeometryComponent();
};
