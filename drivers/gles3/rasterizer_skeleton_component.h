#pragma once

#include "core/hash_set.h"
#include "core/math/transform_2d.h"
#include "core/pool_vector.h"
#include "servers/rendering/render_entity_helpers.h"
#include "rasterizer_gl_unique_handle.h"


struct RasterizerSkeletonComponent {
    HashSet<RenderingEntity> instances; //instances using skeleton
    Vector<float> skel_texture;
    Transform2D base_transform_2d;
    GLTextureHandle texture;
    uint32_t revision=1;
    int size=0;
    bool use_2d=false;

    void unregister_from_instances();

    RasterizerSkeletonComponent(const RasterizerSkeletonComponent&) = delete;
    RasterizerSkeletonComponent& operator=(const RasterizerSkeletonComponent&) = delete;

    RasterizerSkeletonComponent(RasterizerSkeletonComponent &&) noexcept = default;
    RasterizerSkeletonComponent &operator=(RasterizerSkeletonComponent &&from) noexcept {
        unregister_from_instances();
        skel_texture.clear();
        instances = eastl::move(from.instances);
        skel_texture = eastl::move(from.skel_texture);
        base_transform_2d = eastl::move(from.base_transform_2d);
        texture = eastl::move(from.texture);

        size = from.size;
        use_2d = from.use_2d;
        return *this;
    }

    RasterizerSkeletonComponent();
    ~RasterizerSkeletonComponent();
};

void mark_skeleton_dirty(RenderingEntity e);
void update_dirty_skeletons();
