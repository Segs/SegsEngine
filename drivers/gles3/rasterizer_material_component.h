#pragma once

#include "core/engine_entities.h"
#include "core/string_name.h"
#include "core/hash_map.h"
#include "core/variant.h"
#include "servers/rendering/render_entity_helpers.h"

#include "rasterizer_gl_unique_handle.h"

struct RasterizerShaderComponent;

struct RasterizerMaterialComponent  {

    GLBufferHandle ubo_id;
    MoveOnlyEntityHandle shader;
    HashMap<StringName, Variant> params;
    // reference count for geometries referencing this material
    HashMap<RenderingEntity, int> geometry_owners;
    // reference count for instances using this material
    HashMap<RenderingEntity, int> instance_owners;

    Vector<bool> texture_is_3d; //TODO: SEGS: consider using dynamic_bitvector here.
    Vector<RenderingEntity> textures;
    MoveOnlyEntityHandle next_pass; // next material pass
    MoveOnlyEntityHandle self;
    float line_width=1.0f;
    uint32_t ubo_size=0;
    int render_priority=0;
    uint32_t index;
    uint64_t last_pass=0;
    bool can_cast_shadow_cache=false;
    bool is_animated_cache = false;

    void release_resources();

    RasterizerMaterialComponent(const RasterizerMaterialComponent&) = delete;
    RasterizerMaterialComponent& operator=(const RasterizerMaterialComponent&) = delete;

    RasterizerMaterialComponent(RasterizerMaterialComponent&& f) {
        *this = eastl::move(f);
    }
    RasterizerMaterialComponent& operator=(RasterizerMaterialComponent&&);

    RasterizerMaterialComponent() { }
    ~RasterizerMaterialComponent();

};

void _material_make_dirty(const RasterizerMaterialComponent *p_material);
void _material_add_geometry(RenderingEntity p_material, RenderingEntity p_geometry);
void material_remove_geometry(RenderingEntity material, RenderingEntity p_geometry);
