#include "rasterizer_gi_probe_component.h"

#include "rasterizer_dependent_entities_component.h"
#include "rasterizer_storage_gles3.h"
#include "servers/rendering/render_entity_getter.h"

#include "gles3/shaders/scene.glsl.gen.h"

////////////////////////////////////////////////////

RenderingEntity gi_probe_instance_create() {
    return VSG::ecs->create<RasterizerGIProbeInstance>();
}

void gi_probe_instance_set_light_data(RenderingEntity p_probe, RenderingEntity p_base, RenderingEntity p_data) {

    RasterizerGIProbeInstance *gipi = get<RasterizerGIProbeInstance>(p_probe);
    ERR_FAIL_COND(!gipi);
    gipi->data = p_data;
    gipi->probe = VSG::ecs->registry.any_of<RasterizerGIProbeComponent>(p_base) ? p_base : entt::null;
    if (p_data!= entt::null) {
        auto *gipd = get<RasterizerGIProbeDataComponent>(p_data);
        ERR_FAIL_COND(!gipd);

        gipi->tex_cache = GLNonOwningHandle(gipd->tex_id);
        gipi->cell_size_cache.x = 1.0f / gipd->width;
        gipi->cell_size_cache.y = 1.0f / gipd->height;
        gipi->cell_size_cache.z = 1.0f / gipd->depth;
    }
}
void gi_probe_instance_set_transform_to_data(RenderingEntity p_probe, const Transform &p_xform) {

    RasterizerGIProbeInstance *gipi = get<RasterizerGIProbeInstance>(p_probe);
    ERR_FAIL_COND(!gipi);
    gipi->transform_to_data = p_xform;
}

void gi_probe_instance_set_bounds(RenderingEntity p_probe, Vector3 p_bounds) {

    auto *gipi = VSG::ecs->registry.try_get<RasterizerGIProbeInstance>(p_probe);
    ERR_FAIL_COND(!gipi);
    gipi->bounds = p_bounds;
}

//////////////////////
/* GI PROBE API */
//////////////////////

RenderingEntity RasterizerStorageGLES3::gi_probe_create() {
    RenderingEntity res = VSG::ecs->create();
    auto &gip(VSG::ecs->registry.emplace<RasterizerGIProbeComponent>(res));
    VSG::ecs->registry.emplace<RasterizerInstantiableComponent>(res);

    gip.bounds = AABB(Vector3(), Vector3(1, 1, 1));
    gip.dynamic_range = 1;
    gip.energy = 1.0f;
    gip.propagation = 1.0f;
    gip.bias = 0.4f;
    gip.normal_bias = 0.4f;
    gip.interior = false;
    gip.compress = false;
    gip.version = 1;
    gip.cell_size = 1.0f;

    return res;
}

void RasterizerStorageGLES3::gi_probe_set_bounds(RenderingEntity p_probe, const AABB &p_bounds) {

    auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    auto *deps = VSG::ecs->registry.try_get<RasterizerInstantiableComponent>(p_probe);
    ERR_FAIL_COND(!gip || !deps);

    gip->bounds = p_bounds;
    gip->version++;
    deps->instance_change_notify(true, false);
}
AABB RasterizerStorageGLES3::gi_probe_get_bounds(RenderingEntity p_probe) const {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, AABB());

    return gip->bounds;
}

void RasterizerStorageGLES3::gi_probe_set_cell_size(RenderingEntity p_probe, float p_size) {

    auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    auto *deps = VSG::ecs->registry.try_get<RasterizerInstantiableComponent>(p_probe);

    ERR_FAIL_COND(!gip || !deps);

    gip->cell_size = p_size;
    gip->version++;
    deps->instance_change_notify(true, false);
}

float RasterizerStorageGLES3::gi_probe_get_cell_size(RenderingEntity p_probe) const {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, 0);

    return gip->cell_size;
}

void RasterizerStorageGLES3::gi_probe_set_to_cell_xform(RenderingEntity p_probe, const Transform &p_xform) {

    auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND(!gip);

    gip->to_cell = p_xform;
}

Transform RasterizerStorageGLES3::gi_probe_get_to_cell_xform(RenderingEntity p_probe) const {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, Transform());

    return gip->to_cell;
}

void RasterizerStorageGLES3::gi_probe_set_dynamic_data(RenderingEntity p_probe, const PoolVector<int> &p_data) {
    auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    auto *deps = VSG::ecs->registry.try_get<RasterizerInstantiableComponent>(p_probe);
    ERR_FAIL_COND(!gip||!deps);

    gip->dynamic_data = p_data;
    gip->version++;
    deps->instance_change_notify(true, false);
}
PoolVector<int> RasterizerStorageGLES3::gi_probe_get_dynamic_data(RenderingEntity p_probe) const {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, PoolVector<int>());

    return gip->dynamic_data;
}

void RasterizerStorageGLES3::gi_probe_set_dynamic_range(RenderingEntity p_probe, int p_range) {

    auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND(!gip);

    gip->dynamic_range = p_range;
}
int RasterizerStorageGLES3::gi_probe_get_dynamic_range(RenderingEntity p_probe) const {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, 0);

    return gip->dynamic_range;
}

void RasterizerStorageGLES3::gi_probe_set_energy(RenderingEntity p_probe, float p_range) {

    auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND(!gip);

    gip->energy = p_range;
}

void RasterizerStorageGLES3::gi_probe_set_bias(RenderingEntity p_probe, float p_range) {

    auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND(!gip);

    gip->bias = p_range;
}

void RasterizerStorageGLES3::gi_probe_set_normal_bias(RenderingEntity p_probe, float p_range) {

    auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND(!gip);

    gip->normal_bias = p_range;
}

void RasterizerStorageGLES3::gi_probe_set_propagation(RenderingEntity p_probe, float p_range) {

    auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND(!gip);

    gip->propagation = p_range;
}

void RasterizerStorageGLES3::gi_probe_set_interior(RenderingEntity p_probe, bool p_enable) {

    auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND(!gip);

    gip->interior = p_enable;
}

bool RasterizerStorageGLES3::gi_probe_is_interior(RenderingEntity p_probe) const {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, false);

    return gip->interior;
}

float RasterizerStorageGLES3::gi_probe_get_energy(RenderingEntity p_probe) const {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, 0);

    return gip->energy;
}

float RasterizerStorageGLES3::gi_probe_get_bias(RenderingEntity p_probe) const {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, 0);

    return gip->bias;
}

float RasterizerStorageGLES3::gi_probe_get_normal_bias(RenderingEntity p_probe) const {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, 0);

    return gip->normal_bias;
}

float RasterizerStorageGLES3::gi_probe_get_propagation(RenderingEntity p_probe) const {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, 0);

    return gip->propagation;
}

uint32_t RasterizerStorageGLES3::gi_probe_get_version(RenderingEntity p_probe) {

    const auto *gip = VSG::ecs->try_get<RasterizerGIProbeComponent>(p_probe);
    ERR_FAIL_COND_V(!gip, 0);

    return gip->version;
}

RenderingEntity RasterizerStorageGLES3::gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth) {

    RenderingEntity res = VSG::ecs->create();
    auto &gipd(VSG::ecs->registry.emplace<RasterizerGIProbeDataComponent>(res));

    gipd.width = p_width;
    gipd.height = p_height;
    gipd.depth = p_depth;

    glActiveTexture(GL_TEXTURE0);
    gipd.tex_id.create();
    glBindTexture(GL_TEXTURE_3D, gipd.tex_id);

    int level = 0;
    int min_size = 1;


    while (true) {

        glTexImage3D(GL_TEXTURE_3D, level, GL_RGBA8, p_width, p_height, p_depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        if (p_width <= min_size || p_height <= min_size || p_depth <= min_size) {
            break;
        }
        p_width >>= 1;
        p_height >>= 1;
        p_depth >>= 1;
        level++;
    }

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, level);

    gipd.levels = level + 1;

    return res;
}

void RasterizerStorageGLES3::gi_probe_dynamic_data_update(RenderingEntity p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data) {

    auto *gipd = VSG::ecs->try_get<RasterizerGIProbeDataComponent>(p_gi_probe_data);
    ERR_FAIL_COND(!gipd);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, gipd->tex_id);
    glTexSubImage3D(GL_TEXTURE_3D, p_mipmap, 0, 0, p_depth_slice, gipd->width >> p_mipmap, gipd->height >> p_mipmap, p_slice_count, GL_RGBA, GL_UNSIGNED_BYTE, p_data);
    //glTexImage3D(GL_TEXTURE_3D,p_mipmap,GL_RGBA8,gipd.width>>p_mipmap,gipd.height>>p_mipmap,gipd.depth>>p_mipmap,0,GL_RGBA,GL_UNSIGNED_BYTE,p_data);
    //glTexImage3D(GL_TEXTURE_3D,p_mipmap,GL_RGBA8,gipd.width>>p_mipmap,gipd.height>>p_mipmap,gipd.depth>>p_mipmap,0,GL_RGBA,GL_UNSIGNED_BYTE,data.ptr());
}

/////////////////////////////

bool setup_probes(RenderingEntity inst, SceneShaderGLES3 &scene_shader, int max_texture_image_units, const Transform &p_view_transform, bool p_async_in_use)
{
    RenderingInstanceComponent *instance = getUnchecked<RenderingInstanceComponent>(inst);
    int gi_probe_count = instance->gi_probe_instances.size();
    if (!gi_probe_count)
        return false;
    const auto &rd(instance->gi_probe_instances);
    const RenderingEntity *ridp = rd.data();

    auto * gipi = get<RasterizerGIProbeInstance>(ridp[0]);
    auto * probe = get<RasterizerGIProbeComponent>(gipi->probe);
    float bias_scale = instance->baked_light ? 1 : 0;
    // Normally, lightmapping uses the same texturing units than the GI probes; however, in the case of the ubershader
    // that's not a good idea because some hardware/drivers (Android/Intel) may fail to render if a single texturing unit
    // is used through multiple kinds of samplers in the same shader.
    // Moreover, since we don't know at this point if we are going to consume these textures from the ubershader or
    // a conditioned one, the fact that async compilation is enabled is enough for us to switch to the alternative
    // arrangement of texturing units.
    if (p_async_in_use) {
        glActiveTexture(GL_TEXTURE0 + max_texture_image_units - 12);
    } else {
        glActiveTexture(GL_TEXTURE0 + max_texture_image_units - 10);
    }
    glBindTexture(GL_TEXTURE_3D, gipi->tex_cache);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_XFORM1, gipi->transform_to_data * p_view_transform);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_BOUNDS1, gipi->bounds);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_MULTIPLIER1, probe ? probe->dynamic_range * probe->energy : 0.0);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_BIAS1, probe ? probe->bias * bias_scale : 0.0);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_NORMAL_BIAS1, probe ? probe->normal_bias * bias_scale : 0.0);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_BLEND_AMBIENT1, probe ? !probe->interior : false);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_CELL_SIZE1, gipi->cell_size_cache);
    bool has_probe2 = gi_probe_count > 1;
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE2_ENABLED, has_probe2);

    if (!has_probe2) {
        return true;
    }
    auto * gipi2 = get<RasterizerGIProbeInstance>(ridp[1]);
    auto * probe2 = get<RasterizerGIProbeComponent>(gipi->probe);
    if (p_async_in_use) {
        glActiveTexture(GL_TEXTURE0 + max_texture_image_units - 13);
    } else {
        glActiveTexture(GL_TEXTURE0 + max_texture_image_units - 11);
    }
    glBindTexture(GL_TEXTURE_3D, gipi2->tex_cache);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_XFORM2, gipi2->transform_to_data * p_view_transform);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_BOUNDS2, gipi2->bounds);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_CELL_SIZE2, gipi2->cell_size_cache);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_MULTIPLIER2, probe2 ? probe2->dynamic_range * probe2->energy : 0.0);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_BIAS2, probe2 ? probe2->bias * bias_scale : 0.0);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_NORMAL_BIAS2, probe2 ? probe2->normal_bias * bias_scale : 0.0);
    scene_shader.set_uniform(SceneShaderGLES3::GI_PROBE_BLEND_AMBIENT2, probe2 ? !probe2->interior : false);
    return true;
}
