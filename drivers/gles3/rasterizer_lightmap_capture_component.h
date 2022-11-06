#pragma once

#include "core/pool_vector.h"
#include "servers/rendering/rasterizer.h"


struct RasterizerLightmapCaptureComponent {

    PoolVector<RasterizerStorage::LightmapCaptureOctree> octree;
    AABB bounds;
    Transform cell_xform;
    int cell_subdiv=1;
    float energy=1.0f;
    bool interior = false;
};
