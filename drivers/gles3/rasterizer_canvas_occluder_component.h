#pragma once

#include "rasterizer_gl_unique_handle.h"
#include "core/math/vector2.h"
#include "core/vector.h"

struct RasterizerCanvasOccluderComponent  {

    GLVAOHandle array_id; // 0 means, unconfigured
    GLBufferHandle vertex_id; // 0 means, unconfigured
    GLBufferHandle index_id; // 0 means, unconfigured
    Vector<Vector2> lines;
    int len;

    RasterizerCanvasOccluderComponent(const RasterizerCanvasOccluderComponent &)=delete;
    RasterizerCanvasOccluderComponent &operator=(const RasterizerCanvasOccluderComponent &)=delete;

    RasterizerCanvasOccluderComponent(RasterizerCanvasOccluderComponent &&from)=default;
    RasterizerCanvasOccluderComponent &operator=(RasterizerCanvasOccluderComponent &&from)=default;

    RasterizerCanvasOccluderComponent() = default;
};
