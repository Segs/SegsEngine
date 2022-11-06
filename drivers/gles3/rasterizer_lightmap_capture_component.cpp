#include "rasterizer_lightmap_capture_component.h"

#include "drivers/gles3/rasterizer_dependent_entities_component.h"
#include "rasterizer_storage_gles3.h"
#include "servers/rendering/render_entity_getter.h"


/////////////////////////////

struct DirtyLightmap {
// A dummy empty component that marks the entities to update
};

RenderingEntity RasterizerStorageGLES3::lightmap_capture_create() {
    auto res = VSG::ecs->create();
    VSG::ecs->registry.emplace<RasterizerLightmapCaptureComponent>(res);
    VSG::ecs->registry.emplace<RasterizerInstantiableComponent>(res);
    return res;
}

void RasterizerStorageGLES3::lightmap_capture_set_bounds(RenderingEntity p_capture, const AABB &p_bounds) {

    auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    auto *deps = get<RasterizerInstantiableComponent>(p_capture);
    ERR_FAIL_COND(!capture||!deps);
    capture->bounds = p_bounds;
    deps->instance_change_notify(true, false);
}
AABB RasterizerStorageGLES3::lightmap_capture_get_bounds(RenderingEntity p_capture) const {

    const auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    ERR_FAIL_COND_V(!capture, AABB());
    return capture->bounds;
}
void RasterizerStorageGLES3::lightmap_capture_set_octree(RenderingEntity p_capture, const PoolVector<uint8_t> &p_octree) {

    auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    auto *deps = get<RasterizerInstantiableComponent>(p_capture);
    ERR_FAIL_COND(!capture || !deps);

    ERR_FAIL_COND(p_octree.size() == 0 || (p_octree.size() % sizeof(LightmapCaptureOctree)) != 0);

    capture->octree.resize(p_octree.size() / sizeof(LightmapCaptureOctree));
    if (p_octree.size()) {
        PoolVector<LightmapCaptureOctree>::Write w = capture->octree.write();
        PoolVector<uint8_t>::Read r = p_octree.read();
        memcpy(w.ptr(), r.ptr(), p_octree.size());
    }
    deps->instance_change_notify(true, false);
}
PoolVector<uint8_t> RasterizerStorageGLES3::lightmap_capture_get_octree(RenderingEntity p_capture) const {

    const auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    ERR_FAIL_COND_V(!capture, PoolVector<uint8_t>());

    if (capture->octree.size() == 0) {
        return PoolVector<uint8_t>();
    }

    PoolVector<uint8_t> ret;
    ret.resize(capture->octree.size() * sizeof(LightmapCaptureOctree));
    {
        PoolVector<LightmapCaptureOctree>::Read r = capture->octree.read();
        PoolVector<uint8_t>::Write w = ret.write();
        memcpy(w.ptr(), r.ptr(), ret.size());
    }

    return ret;
}

void RasterizerStorageGLES3::lightmap_capture_set_octree_cell_transform(RenderingEntity p_capture, const Transform &p_xform) {
    auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    ERR_FAIL_COND(!capture);
    capture->cell_xform = p_xform;
}

Transform RasterizerStorageGLES3::lightmap_capture_get_octree_cell_transform(RenderingEntity p_capture) const {
    const auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    ERR_FAIL_COND_V(!capture, Transform());
    return capture->cell_xform;
}

void RasterizerStorageGLES3::lightmap_capture_set_octree_cell_subdiv(RenderingEntity p_capture, int p_subdiv) {
    auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    ERR_FAIL_COND(!capture);
    capture->cell_subdiv = p_subdiv;
}

int RasterizerStorageGLES3::lightmap_capture_get_octree_cell_subdiv(RenderingEntity p_capture) const {
    const auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    ERR_FAIL_COND_V(!capture, 0);
    return capture->cell_subdiv;
}

void RasterizerStorageGLES3::lightmap_capture_set_energy(RenderingEntity p_capture, float p_energy) {

    auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    ERR_FAIL_COND(!capture);
    capture->energy = p_energy;

    assert(VSG::ecs->registry.valid(p_capture));
    VSG::ecs->registry.emplace_or_replace<DirtyLightmap>(p_capture);
}

float RasterizerStorageGLES3::lightmap_capture_get_energy(RenderingEntity p_capture) const {

    const auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    ERR_FAIL_COND_V(!capture, 0);
    return capture->energy;
}

void RasterizerStorageGLES3::lightmap_capture_set_interior(RenderingEntity p_capture, bool p_interior) {
    RasterizerLightmapCaptureComponent *capture = get<RasterizerLightmapCaptureComponent>(p_capture);

    ERR_FAIL_COND(!capture);
    capture->interior = p_interior;

    assert(VSG::ecs->registry.valid(p_capture));
    VSG::ecs->registry.emplace_or_replace<DirtyLightmap>(p_capture);
}

bool RasterizerStorageGLES3::lightmap_capture_is_interior(RenderingEntity p_capture) const {
    const RasterizerLightmapCaptureComponent *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    ERR_FAIL_COND_V(!capture, false);
    return capture->interior;
}

const PoolVector<RasterizerStorage::LightmapCaptureOctree> *RasterizerStorageGLES3::lightmap_capture_get_octree_ptr(RenderingEntity p_capture) const {
    const auto *capture = get<RasterizerLightmapCaptureComponent>(p_capture);
    ERR_FAIL_COND_V(!capture, nullptr);
    return &capture->octree;
}

void RasterizerStorageGLES3::update_dirty_captures() {
    assert(VSG::ecs->is_registry_access_valid_from_this_thread());

    auto to_update = VSG::ecs->registry.view<DirtyLightmap,RasterizerInstantiableComponent>();
    to_update.each([](const RenderingEntity , RasterizerInstantiableComponent &deps) {
        deps.instance_change_notify(false, true);
    }
    );
    VSG::ecs->registry.clear<DirtyLightmap>();
}
