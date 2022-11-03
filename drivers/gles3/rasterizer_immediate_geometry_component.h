#pragma once

#include "core/math/aabb.h"
#include "servers/rendering_server_enums.h"
#include "core/color.h"
#include "core/vector.h"
#include "core/engine_entities.h"
#include "core/math/plane.h"
#include "core/math/vector3.h"

struct RasterizerImmediateGeometryComponent {

    struct Chunk {
        Vector<Vector3> vertices;
        RenderingEntity texture;
        Vector<Vector3> normals;
        Vector<Plane> tangents;
        Vector<Color> colors;
        Vector<Vector2> uvs;
        Vector<Vector2> uvs2;
        RS::PrimitiveType primitive;
    };

    Vector<Chunk> chunks;
    AABB aabb;
    int mask;
    bool building=false;

    RasterizerImmediateGeometryComponent(const RasterizerImmediateGeometryComponent &) = delete;
    RasterizerImmediateGeometryComponent &operator=(const RasterizerImmediateGeometryComponent &) = delete;

    RasterizerImmediateGeometryComponent(RasterizerImmediateGeometryComponent &&) noexcept = default;
    RasterizerImmediateGeometryComponent &operator=(RasterizerImmediateGeometryComponent &&) noexcept = default;
    RasterizerImmediateGeometryComponent() = default;
};
