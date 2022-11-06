#include "rasterizer_reflection_probe_component.h"

#include "rasterizer_reflection_atlas_component.h"
#include "rasterizer_dependent_entities_component.h"
#include "rasterizer_storage_gles3.h"
#include "rasterizer_scene_gles3.h"

#include "servers/rendering/render_entity_getter.h"
#include "core/engine_entities.h"


/* PROBE API */
RenderingEntity RasterizerSceneGLES3::reflection_probe_instance_create(RenderingEntity p_probe) {


    RasterizerReflectionProbeComponent *probe = VSG::ecs->get_or_null<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!probe, entt::null);

    auto res = VSG::ecs->create();
    auto &rpi(VSG::ecs->registry.emplace<RasterizerReflectionProbeInstanceComponent>(res));

    rpi.self = res;
    rpi.probe = p_probe;
    rpi.reflection_atlas_index = -1;
    rpi.render_step = -1;
    rpi.last_pass = 0;

    return res;
}

void RasterizerSceneGLES3::reflection_probe_instance_set_transform(RenderingEntity p_instance, const Transform &p_transform) {

    RasterizerReflectionProbeInstanceComponent *rpi = VSG::ecs->get_or_null<RasterizerReflectionProbeInstanceComponent>(p_instance);
    ERR_FAIL_COND(!rpi);
    rpi->transform = p_transform;
}

void RasterizerSceneGLES3::reflection_probe_release_atlas_index(RenderingEntity p_instance) {

    RasterizerReflectionProbeInstanceComponent *rpi = VSG::ecs->get_or_null<RasterizerReflectionProbeInstanceComponent>(p_instance);
    ERR_FAIL_COND(!rpi);
    if (rpi->reflection_atlas_index == -1) {
        return;
    }

    auto *reflection_atlas = VSG::ecs->get_or_null<RasterizerReflectionAtlasComponent>(rpi->atlas);
    ERR_FAIL_COND(!reflection_atlas);

    ERR_FAIL_INDEX(rpi->reflection_atlas_index, reflection_atlas->reflections.size());

    ERR_FAIL_COND(reflection_atlas->reflections[rpi->reflection_atlas_index].owner != rpi->self);

    reflection_atlas->reflections[rpi->reflection_atlas_index].owner = entt::null;

    rpi->reflection_atlas_index = -1;
    rpi->atlas = entt::null;
    rpi->render_step = -1;
}

bool RasterizerSceneGLES3::reflection_probe_instance_needs_redraw(RenderingEntity p_instance) {

    RasterizerReflectionProbeInstanceComponent *rpi = VSG::ecs->get_or_null<RasterizerReflectionProbeInstanceComponent>(p_instance);
    ERR_FAIL_COND_V(!rpi, false);
    auto *probe_ptr = getUnchecked<RasterizerReflectionProbeComponent>(rpi->probe);

    return rpi->reflection_atlas_index == -1 || probe_ptr->update_mode == RS::REFLECTION_PROBE_UPDATE_ALWAYS;
}

bool RasterizerSceneGLES3::reflection_probe_instance_has_reflection(RenderingEntity p_instance) {

    RasterizerReflectionProbeInstanceComponent *rpi = VSG::ecs->get_or_null<RasterizerReflectionProbeInstanceComponent>(p_instance);
    ERR_FAIL_COND_V(!rpi, false);

    return rpi->reflection_atlas_index != -1;
}

bool RasterizerSceneGLES3::reflection_probe_instance_begin_render(RenderingEntity p_instance, RenderingEntity p_reflection_atlas) {

    auto *rpi = get<RasterizerReflectionProbeInstanceComponent>(p_instance);
    ERR_FAIL_COND_V(!rpi, false);

    rpi->render_step = 0;

    if (rpi->reflection_atlas_index != -1) {
        return true; //got one already
    }

    auto *reflection_atlas = get<RasterizerReflectionAtlasComponent>(p_reflection_atlas);
    ERR_FAIL_COND_V(!reflection_atlas, false);

    if (reflection_atlas->size == 0 || reflection_atlas->subdiv == 0) {
        return false;
    }

    int best_free = -1;
    int best_used = -1;
    uint64_t best_used_frame = 0;

    for (int i = 0; i < reflection_atlas->reflections.size(); i++) {
        if (reflection_atlas->reflections[i].owner == entt::null) {
            best_free = i;
            break;
        }

        if (rpi->render_step < 0 && reflection_atlas->reflections[i].last_frame < storage->frame.count &&
                (best_used == -1 || reflection_atlas->reflections[i].last_frame < best_used_frame)) {
            best_used = i;
            best_used_frame = reflection_atlas->reflections[i].last_frame;
        }
    }

    if (best_free == -1 && best_used == -1) {
        return false; // sorry, can not do. Try again next frame.
    }

    if (best_free == -1) {
        //find best from what is used
        best_free = best_used;

        auto *victim_rpi = get<RasterizerReflectionProbeInstanceComponent>(reflection_atlas->reflections[best_free].owner);
        ERR_FAIL_COND_V(!victim_rpi, false);
        victim_rpi->atlas = entt::null;
        victim_rpi->reflection_atlas_index = -1;
    }

    reflection_atlas->reflections[best_free].owner = p_instance;
    reflection_atlas->reflections[best_free].last_frame = storage->frame.count;

    rpi->reflection_atlas_index = best_free;
    rpi->atlas = p_reflection_atlas;
    rpi->render_step = 0;

    return true;
}

bool RasterizerSceneGLES3::reflection_probe_instance_postprocess_step(RenderingEntity p_instance) {

    RasterizerReflectionProbeInstanceComponent *rpi = VSG::ecs->get_or_null<RasterizerReflectionProbeInstanceComponent>(p_instance);
    ERR_FAIL_COND_V(!rpi, true);

    auto *reflection_atlas = VSG::ecs->get_or_null<RasterizerReflectionAtlasComponent>(rpi->atlas);
    ERR_FAIL_COND_V(!reflection_atlas, false);

    ERR_FAIL_COND_V(rpi->render_step >= 6, true);

    glBindFramebuffer(GL_FRAMEBUFFER, reflection_atlas->fbo[rpi->render_step]);
    state.cube_to_dp_shader.bind();

    int target_size = reflection_atlas->size / reflection_atlas->subdiv;

    int cubemap_index = reflection_cubemaps.size() - 1;

    for (int i = reflection_cubemaps.size() - 1; i >= 0; i--) {
        //find appropriate cubemap to render to
        if (reflection_cubemaps[i].size > target_size * 2) {
            break;

        }
        cubemap_index = i;
    }

    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, reflection_cubemaps[cubemap_index].cubemap);
    glDisable(GL_CULL_FACE);

    storage->shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
    storage->shaders.cubemap_filter.bind();

    int cell_size = reflection_atlas->size / reflection_atlas->subdiv;
    for (int i = 0; i < rpi->render_step; i++) {
        cell_size >>= 1; //mipmaps!
    }
    int x = (rpi->reflection_atlas_index % reflection_atlas->subdiv) * cell_size;
    int y = (rpi->reflection_atlas_index / reflection_atlas->subdiv) * cell_size;
    int width = cell_size;
    int height = cell_size;
    auto *probe_ptr = getUnchecked<RasterizerReflectionProbeComponent>(rpi->probe);

    storage->shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DIRECT_WRITE, rpi->render_step == 0);
    storage->shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::LOW_QUALITY, probe_ptr->update_mode == RS::REFLECTION_PROBE_UPDATE_ALWAYS);
    for (int i = 0; i < 2; i++) {

        storage->shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::Z_FLIP, i == 0);
        storage->shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::ROUGHNESS, rpi->render_step / 5.0);

        uint32_t local_width = width, local_height = height;
        uint32_t local_x = x, local_y = y;

        local_height /= 2;
        local_y += i * local_height;

        glViewport(local_x, local_y, local_width, local_height);

        _copy_screen();
    }
    storage->shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DIRECT_WRITE, false);
    storage->shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::LOW_QUALITY, false);

    rpi->render_step++;

    return rpi->render_step == 6;
}


void RasterizerReflectionProbeInstanceComponent::release_atlas_index()
{
    if (reflection_atlas_index == -1)
        return;

    auto *reflection_atlas = get<RasterizerReflectionAtlasComponent>(atlas);

    ERR_FAIL_COND(!reflection_atlas);

    ERR_FAIL_INDEX(reflection_atlas_index, reflection_atlas->reflections.size());

    ERR_FAIL_COND(reflection_atlas->reflections[reflection_atlas_index].owner != self);

    reflection_atlas->reflections[reflection_atlas_index].owner = entt::null;

    reflection_atlas_index = -1;
    atlas = entt::null;
    render_step = -1;
}

RasterizerReflectionProbeInstanceComponent::~RasterizerReflectionProbeInstanceComponent()
{
    release_atlas_index();
}

/* PROBE API */

RenderingEntity RasterizerStorageGLES3::reflection_probe_create() {
    auto res = VSG::ecs->create();
    auto &reflection_probe(VSG::ecs->registry.emplace<RasterizerReflectionProbeComponent>(res));
    VSG::ecs->registry.emplace<RasterizerInstantiableComponent>(res);

    reflection_probe.interior = false;
    reflection_probe.box_projection = false;
    reflection_probe.enable_shadows = false;

    return res;
}

void RasterizerStorageGLES3::reflection_probe_set_update_mode(RenderingEntity p_probe, RS::ReflectionProbeUpdateMode p_mode) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->update_mode = p_mode;
    getUnchecked<RasterizerInstantiableComponent>(p_probe)->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::reflection_probe_set_intensity(RenderingEntity p_probe, float p_intensity) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->intensity = p_intensity;
}

void RasterizerStorageGLES3::reflection_probe_set_interior_ambient(RenderingEntity p_probe, const Color &p_ambient) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->interior_ambient = p_ambient;
}

void RasterizerStorageGLES3::reflection_probe_set_interior_ambient_energy(RenderingEntity p_probe, float p_energy) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->interior_ambient_energy = p_energy;
}

void RasterizerStorageGLES3::reflection_probe_set_interior_ambient_probe_contribution(RenderingEntity p_probe, float p_contrib) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->interior_ambient_probe_contrib = p_contrib;
}

void RasterizerStorageGLES3::reflection_probe_set_max_distance(RenderingEntity p_probe, float p_distance) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->max_distance = p_distance;
    getUnchecked<RasterizerInstantiableComponent>(p_probe)->instance_change_notify(true, false);
}
void RasterizerStorageGLES3::reflection_probe_set_extents(RenderingEntity p_probe, const Vector3 &p_extents) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->extents = p_extents;
    get<RasterizerInstantiableComponent>(p_probe)->instance_change_notify(true, false);
}
void RasterizerStorageGLES3::reflection_probe_set_origin_offset(RenderingEntity p_probe, const Vector3 &p_offset) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->origin_offset = p_offset;
    getUnchecked<RasterizerInstantiableComponent>(p_probe)->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::reflection_probe_set_as_interior(RenderingEntity p_probe, bool p_enable) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->interior = p_enable;
    getUnchecked<RasterizerInstantiableComponent>(p_probe)->instance_change_notify(true, false);
}
void RasterizerStorageGLES3::reflection_probe_set_enable_box_projection(RenderingEntity p_probe, bool p_enable) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->box_projection = p_enable;
}

void RasterizerStorageGLES3::reflection_probe_set_enable_shadows(RenderingEntity p_probe, bool p_enable) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->enable_shadows = p_enable;
    getUnchecked<RasterizerInstantiableComponent>(p_probe)->instance_change_notify(true, false);
}
void RasterizerStorageGLES3::reflection_probe_set_cull_mask(RenderingEntity p_probe, uint32_t p_layers) {

    auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND(!reflection_probe);

    reflection_probe->cull_mask = p_layers;
    getUnchecked<RasterizerInstantiableComponent>(p_probe)->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::reflection_probe_set_resolution(RenderingEntity p_probe, int p_resolution) {
}

AABB RasterizerStorageGLES3::reflection_probe_get_aabb(RenderingEntity p_probe) const {
    const auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!reflection_probe, AABB());

    AABB aabb;
    aabb.position = -reflection_probe->extents;
    aabb.size = reflection_probe->extents * 2.0;

    return aabb;
}
RS::ReflectionProbeUpdateMode RasterizerStorageGLES3::reflection_probe_get_update_mode(RenderingEntity p_probe) const {

    const auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!reflection_probe, RS::REFLECTION_PROBE_UPDATE_ALWAYS);

    return reflection_probe->update_mode;
}

uint32_t RasterizerStorageGLES3::reflection_probe_get_cull_mask(RenderingEntity p_probe) const {

    const auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!reflection_probe, 0);

    return reflection_probe->cull_mask;
}

Vector3 RasterizerStorageGLES3::reflection_probe_get_extents(RenderingEntity p_probe) const {

    const auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!reflection_probe, Vector3());

    return reflection_probe->extents;
}
Vector3 RasterizerStorageGLES3::reflection_probe_get_origin_offset(RenderingEntity p_probe) const {

    const auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!reflection_probe, Vector3());

    return reflection_probe->origin_offset;
}

bool RasterizerStorageGLES3::reflection_probe_renders_shadows(RenderingEntity p_probe) const {

    const auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!reflection_probe, false);

    return reflection_probe->enable_shadows;
}

float RasterizerStorageGLES3::reflection_probe_get_origin_max_distance(RenderingEntity p_probe) const {

    const auto *reflection_probe = get<RasterizerReflectionProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!reflection_probe, 0);

    return reflection_probe->max_distance;
}

