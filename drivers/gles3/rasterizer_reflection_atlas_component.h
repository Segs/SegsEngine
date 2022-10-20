#pragma once

#include "core/engine_entities.h"
#include "core/vector.h"
#include "rasterizer_gl_unique_handle.h"

struct RasterizerReflectionAtlasComponent  {

    int subdiv=0;
    int size=0;

    struct Reflection {
        RenderingEntity owner = entt::null;
        uint64_t last_frame;
    };

    GLMultiFBOHandle<6> fbo;
    GLTextureHandle color;

    Vector<Reflection> reflections;
    void unregister_from_reflection_probes();
    void set_size(int p_size);

    RasterizerReflectionAtlasComponent &operator=(const RasterizerReflectionAtlasComponent &) = delete;
    RasterizerReflectionAtlasComponent(const RasterizerReflectionAtlasComponent &) = delete;

    RasterizerReflectionAtlasComponent &operator=(RasterizerReflectionAtlasComponent &&from) {
        unregister_from_reflection_probes();
        subdiv = from.subdiv;
        size= from.size;
        fbo = eastl::move(from.fbo);
        color = eastl::move(from.color);
        reflections = eastl::move(from.reflections);
        return *this;
    }
    RasterizerReflectionAtlasComponent(RasterizerReflectionAtlasComponent &&) = default;

    ~RasterizerReflectionAtlasComponent();
};

/* REFLECTION PROBE ATLAS API */

RenderingEntity reflection_atlas_create();
void reflection_atlas_set_size(RenderingEntity p_ref_atlas, int p_size);
void reflection_atlas_set_subdivision(RenderingEntity p_ref_atlas, int p_subdiv);
