#pragma once

#include "rasterizer_gl_unique_handle.h"
#include "servers/rendering/render_entity_helpers.h"
#include "core/engine_entities.h"
#include "core/hash_map.h"
#include "core/vector.h"

#include "EASTL/array.h"

struct RasterizerShadowAtlasComponent {

    enum {
        QUADRANT_SHIFT = 27,
        SHADOW_INDEX_MASK = (1 << QUADRANT_SHIFT) - 1,
        SHADOW_INVALID = 0xFFFFFFFF
    };

    struct Quadrant {

        struct Shadow {
            uint64_t version = 0;
            uint64_t alloc_tick = 0;
            MoveOnlyEntityHandle owner;
        };
        Vector<Shadow> shadows;
        uint32_t subdivision = 0;  //not in use
    };

    eastl::array<Quadrant,4> quadrants;
    HashMap<RenderingEntity, uint32_t> shadow_owners;

    eastl::array<int,4> size_order;
    uint32_t smallest_subdiv;

    int size;

    GLFBOHandle fbo;
    GLTextureHandle depth;
    MoveOnlyEntityHandle self=entt::null;

    void unregister_from_lights();
    void set_size(RenderingEntity self, int p_size);

    RasterizerShadowAtlasComponent(const RasterizerShadowAtlasComponent &) = delete;
    RasterizerShadowAtlasComponent &operator=(const RasterizerShadowAtlasComponent &) = delete;

    RasterizerShadowAtlasComponent(RasterizerShadowAtlasComponent&&) = default;
    RasterizerShadowAtlasComponent &operator=(RasterizerShadowAtlasComponent &&from) noexcept {
        unregister_from_lights();
        if(this==&from) {
            quadrants = {}; //.fill({{0,0,entt::null}});
            size_order.fill(0);
        } else {
            quadrants = eastl::move(from.quadrants);
            size_order = from.size_order;
        }
        shadow_owners = eastl::move(from.shadow_owners);
        smallest_subdiv = from.smallest_subdiv;
        size=from.size;
        fbo = eastl::move(from.fbo);
        depth = eastl::move(from.depth);
        self = eastl::move(from.self);
        return *this;
    }
    RasterizerShadowAtlasComponent() {
    }
    ~RasterizerShadowAtlasComponent();
};
