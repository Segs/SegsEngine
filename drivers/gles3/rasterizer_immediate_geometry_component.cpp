#include "rasterizer_immediate_geometry_component.h"

#include "rasterizer_dependent_entities_component.h"
#include "rasterizer_shader_component.h"
#include "rasterizer_common_geometry_component.h"
#include "rasterizer_storage_gles3.h"
#include "servers/rendering/render_entity_getter.h"



/* IMMEDIATE API */

RenderingEntity RasterizerStorageGLES3::immediate_create() {
    auto res = VSG::ecs->create();
    VSG::ecs->registry.emplace<RasterizerInstantiableComponent>(res);
    VSG::ecs->registry.emplace<RasterizerCommonGeometryComponent>(res,RasterizerCommonGeometryComponent::GEOMETRY_IMMEDIATE);
    VSG::ecs->registry.emplace<RasterizerImmediateGeometryComponent>(res);
    return res;
}

void RasterizerStorageGLES3::immediate_begin(RenderingEntity p_immediate, RS::PrimitiveType p_primitive, RenderingEntity p_texture) {
    ERR_FAIL_INDEX(p_primitive, (int)RS::PRIMITIVE_MAX);

    auto *im = getUnchecked<RasterizerImmediateGeometryComponent>(p_immediate);
    ERR_FAIL_COND(!im);
    ERR_FAIL_COND(im->building);

    RasterizerImmediateGeometryComponent::Chunk ic;
    ic.texture = p_texture;
    ic.primitive = p_primitive;
    im->chunks.emplace_back(eastl::move(ic));
    im->mask = 0;
    im->building = true;
}
void RasterizerStorageGLES3::immediate_vertex(RenderingEntity p_immediate, const Vector3 &p_vertex) {

    auto *im = getUnchecked<RasterizerImmediateGeometryComponent>(p_immediate);
    ERR_FAIL_COND(!im);
    ERR_FAIL_COND(!im->building);

    RasterizerImmediateGeometryComponent::Chunk *c = &im->chunks.back();

    if (c->vertices.empty() && im->chunks.size() == 1) {

        im->aabb.position = p_vertex;
        im->aabb.size = Vector3();
    } else {
        im->aabb.expand_to(p_vertex);
    }

    if (im->mask & RS::ARRAY_FORMAT_NORMAL)
        c->normals.push_back(chunk_normal);
    if (im->mask & RS::ARRAY_FORMAT_TANGENT)
        c->tangents.push_back(chunk_tangent);
    if (im->mask & RS::ARRAY_FORMAT_COLOR)
        c->colors.push_back(chunk_color);
    if (im->mask & RS::ARRAY_FORMAT_TEX_UV)
        c->uvs.push_back(chunk_uv);
    if (im->mask & RS::ARRAY_FORMAT_TEX_UV2)
        c->uvs2.push_back(chunk_uv2);
    im->mask |= RS::ARRAY_FORMAT_VERTEX;
    c->vertices.push_back(p_vertex);
}

void RasterizerStorageGLES3::immediate_normal(RenderingEntity p_immediate, const Vector3 &p_normal) {

    auto *im = getUnchecked<RasterizerImmediateGeometryComponent>(p_immediate);
    ERR_FAIL_COND(!im);
    ERR_FAIL_COND(!im->building);

    im->mask |= RS::ARRAY_FORMAT_NORMAL;
    chunk_normal = p_normal;
}
void RasterizerStorageGLES3::immediate_tangent(RenderingEntity p_immediate, const Plane &p_tangent) {

    auto *im = getUnchecked<RasterizerImmediateGeometryComponent>(p_immediate);
    ERR_FAIL_COND(!im);
    ERR_FAIL_COND(!im->building);

    im->mask |= RS::ARRAY_FORMAT_TANGENT;
    chunk_tangent = p_tangent;
}
void RasterizerStorageGLES3::immediate_color(RenderingEntity p_immediate, const Color &p_color) {

    auto *im = getUnchecked<RasterizerImmediateGeometryComponent>(p_immediate);
    ERR_FAIL_COND(!im);
    ERR_FAIL_COND(!im->building);

    im->mask |= RS::ARRAY_FORMAT_COLOR;
    chunk_color = p_color;
}
void RasterizerStorageGLES3::immediate_uv(RenderingEntity p_immediate, const Vector2 &tex_uv) {

    auto *im = getUnchecked<RasterizerImmediateGeometryComponent>(p_immediate);
    ERR_FAIL_COND(!im);
    ERR_FAIL_COND(!im->building);

    im->mask |= RS::ARRAY_FORMAT_TEX_UV;
    chunk_uv = tex_uv;
}
void RasterizerStorageGLES3::immediate_uv2(RenderingEntity p_immediate, const Vector2 &tex_uv) {

    auto *im = getUnchecked<RasterizerImmediateGeometryComponent>(p_immediate);
    ERR_FAIL_COND(!im);
    ERR_FAIL_COND(!im->building);

    im->mask |= RS::ARRAY_FORMAT_TEX_UV2;
    chunk_uv2 = tex_uv;
}

void RasterizerStorageGLES3::immediate_end(RenderingEntity p_immediate) {

    RasterizerImmediateGeometryComponent *im = getUnchecked<RasterizerImmediateGeometryComponent>(p_immediate);
    ERR_FAIL_COND(!im);
    ERR_FAIL_COND(!im->building);

    im->building = false;

    RasterizerInstantiableComponent *inst( get<RasterizerInstantiableComponent>(p_immediate));
    ERR_FAIL_COND(!inst);
    inst->instance_change_notify(true, false);
}
void RasterizerStorageGLES3::immediate_clear(RenderingEntity p_immediate) {

    auto *im = VSG::ecs->try_get<RasterizerImmediateGeometryComponent>(p_immediate);
    auto *deps = VSG::ecs->registry.try_get<RasterizerInstantiableComponent>(p_immediate);
    ERR_FAIL_COND(!im || !deps);
    ERR_FAIL_COND(im->building);

    im->chunks.clear();
    deps->instance_change_notify(true, false);
}

AABB RasterizerStorageGLES3::immediate_get_aabb(RenderingEntity p_immediate) const {

    auto *im = VSG::ecs->try_get<RasterizerImmediateGeometryComponent>(p_immediate);
    ERR_FAIL_COND_V(!im, AABB());
    return im->aabb;
}

void RasterizerStorageGLES3::immediate_set_material(RenderingEntity p_immediate, RenderingEntity p_material) {

    auto *im = VSG::ecs->try_get<RasterizerCommonGeometryComponent>(p_immediate);
    auto *deps = VSG::ecs->registry.try_get<RasterizerInstantiableComponent>(p_immediate);
    ERR_FAIL_COND(!im || !deps);

    im->material = p_material;
    deps->instance_change_notify(false, true);
}

RenderingEntity RasterizerStorageGLES3::immediate_get_material(RenderingEntity p_immediate) const {

    const auto *im = VSG::ecs->try_get<RasterizerCommonGeometryComponent>(p_immediate);
    ERR_FAIL_COND_V(!im, entt::null);
    return im->material;
}
