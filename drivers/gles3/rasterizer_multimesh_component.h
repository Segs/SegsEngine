#pragma once

#include "rasterizer_gl_unique_handle.h"
#include "servers/rendering/render_entity_helpers.h"
#include "servers/rendering_server_enums.h"
#include "core/engine_entities.h"
#include "core/pool_vector.h"
#include "core/math/aabb.h"

struct RasterizerMultiMeshComponent {
    MoveOnlyEntityHandle mesh;
    MoveOnlyEntityHandle self;
    int size=0;
    PoolVector<float> data;
    AABB aabb;
    GLBufferHandle buffer;
    int visible_instances=-1;
    int xform_floats=0;
    int color_floats=0;
    int custom_data_floats=0;

    RS::MultimeshTransformFormat transform_format = RS::MULTIMESH_TRANSFORM_2D;
    RS::MultimeshColorFormat color_format = RS::MULTIMESH_COLOR_NONE;
    RS::MultimeshCustomDataFormat custom_data_format = RS::MULTIMESH_CUSTOM_DATA_NONE;

    void unregister_from_mesh();

    RasterizerMultiMeshComponent &operator=(const RasterizerMultiMeshComponent&)=delete;
    RasterizerMultiMeshComponent(const RasterizerMultiMeshComponent&)=delete;

    RasterizerMultiMeshComponent(RasterizerMultiMeshComponent &&oth) {
        *this = eastl::move(oth);
    }
    RasterizerMultiMeshComponent &operator=(RasterizerMultiMeshComponent &&);

    RasterizerMultiMeshComponent(RenderingEntity s);

    ~RasterizerMultiMeshComponent();

};

void multimesh_remove_base_mesh(RenderingEntity mmesh);
void mark_multimeshes_dirty(Span<RenderingEntity> meshes);
void update_dirty_multimeshes();
