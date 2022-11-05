/*************************************************************************/
/*  rendering_server_scene.cpp                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "rendering_server_scene.h"

#include "rendering_server_globals.h"
#include "rendering_server_raster.h"
#include "servers/rendering/renderer_instance_component.h"
#include "servers/rendering/portals/portal_resources.h"
#include "servers/rendering/render_entity_getter.h"

#include "core/ecs_registry.h"
#include "core/external_profiler.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/map.h"
#include <new>

namespace {

template<typename T>
bool has_component(RenderingEntity id) {
    return VSG::ecs->registry.valid(id) && VSG::ecs->registry.any_of<T>(id);
}
template<typename T>
T & get_component(RenderingEntity id) {

    CRASH_COND(!VSG::ecs->registry.valid(id) );
    CRASH_COND(!VSG::ecs->registry.any_of<T>(id) );

    return VSG::ecs->registry.get<T>(id);
}
template<typename T>
void clear_component(RenderingEntity id) {
    if (VSG::ecs->registry.valid(id) && VSG::ecs->registry.any_of<T>(id))
    {
        VSG::ecs->registry.remove<T>(id);
    }
}

struct Dirty {
    //aabb stuff
    bool update_aabb : 1;
    bool update_materials : 1;

    constexpr Dirty() : update_aabb(false), update_materials(false) { }

    constexpr Dirty(bool aabb,bool material) : update_aabb(aabb),update_materials(material) {

    }

};

struct InstanceBoundsComponent {

    AABB aabb;
    AABB transformed_aabb;
    AABB custom_aabb; // <Zylann> would using aabb directly with a bool be better?
    float extra_margin = 0.0f;
    float sorting_offset=0.0f; 
    bool use_aabb_center = false;
    bool use_custom_aabb = false;
};
struct PortalComponent {
    // all interactions with actual portals are indirect, as the portal is part of the scenario
    uint32_t scenario_portal_id = 0;
    RenderingEntity scenario = entt::null;
    ~PortalComponent() {
        if (scenario!=entt::null) {
            get_component<RenderingScenarioComponent>(scenario)._portal_renderer.portal_destroy(scenario_portal_id);
            scenario = entt::null;
            scenario_portal_id = 0;
        }
    }
};
struct RoomComponent {
    // all interactions with actual rooms are indirect, as the room is part of the scenario
    uint32_t scenario_room_id = 0;
    RenderingEntity scenario = entt::null;
    ~RoomComponent() {
        if (scenario!=entt::null) {
            get_component<RenderingScenarioComponent>(scenario)._portal_renderer.room_destroy(scenario_room_id);
            scenario = entt::null;
            scenario_room_id = 0;
        }
    }
};
struct RoomGroupComponent {
    // all interactions with actual roomgroups are indirect, as the roomgroup is part of the scenario
    uint32_t scenario_roomgroup_id = 0;
    RenderingEntity scenario = entt::null;
    virtual ~RoomGroupComponent() {
        if (scenario!=entt::null) {
            get_component<RenderingScenarioComponent>(scenario)._portal_renderer.roomgroup_destroy(scenario_roomgroup_id);
            scenario = entt::null;
            scenario_roomgroup_id = 0;
        }
    }
};

struct OcclusionGhostComponent {
    // all interactions with actual ghosts are indirect, as the ghost is part of the scenario
    RenderingEntity scenario = entt::null;
    GameEntity object_id = entt::null;
    RGhostHandle rghost_handle = 0; // handle in occlusion system (or 0)
    AABB aabb;
    virtual ~OcclusionGhostComponent() {
        if (scenario!=entt::null) {
            if (rghost_handle) {
                get_component<RenderingScenarioComponent>(scenario)._portal_renderer.rghost_destroy(rghost_handle);
                rghost_handle = 0;
            }
            scenario = entt::null;
        }
    }
};

// Occluders
struct OccluderInstanceComponent {
    uint32_t scenario_occluder_id = 0;
    RenderingEntity scenario = entt::null;
    virtual ~OccluderInstanceComponent() {
        if (scenario!=entt::null) {
            get_component<RenderingScenarioComponent>(scenario)._portal_renderer.occluder_instance_destroy(
                    scenario_occluder_id);
            scenario = entt::null;
            scenario_occluder_id = 0;
        }
    }
};

struct OccluderResourceComponent {
    uint32_t occluder_resource_id = 0;
    void destroy(PortalResources &r_portal_resources) {
        r_portal_resources.occluder_resource_destroy(occluder_resource_id);
        occluder_resource_id = 0;
    }
    ~OccluderResourceComponent() {
        DEV_ASSERT(occluder_resource_id == 0);
    }
};

InstanceGeometryData *get_instance_geometry(RenderingEntity id) {

    if (has_component<GeometryComponent>(id)) {
        return getUnchecked<GeometryComponent>(id)->Data;
    }
    return nullptr;
}

_FORCE_INLINE_ static uint32_t _gi_bake_find_cell(
        const VisualServerScene::GIProbeDataCell *cells, int x, int y, int z, int p_cell_subdiv) {

    uint32_t cell = 0;

    int ofs_x = 0;
    int ofs_y = 0;
    int ofs_z = 0;
    int size = 1 << (p_cell_subdiv - 1);
    int half = size / 2;

    if (x < 0 || x >= size)
        return ~0U;
    if (y < 0 || y >= size)
        return ~0U;
    if (z < 0 || z >= size)
        return ~0U;

    for (int i = 0; i < p_cell_subdiv - 1; i++) {

        const VisualServerScene::GIProbeDataCell *bc = &cells[cell];

        int child = 0;
        if (x >= ofs_x + half) {
            child |= 1;
            ofs_x += half;
        }
        if (y >= ofs_y + half) {
            child |= 2;
            ofs_y += half;
        }
        if (z >= ofs_z + half) {
            child |= 4;
            ofs_z += half;
        }

        cell = bc->children[child];
        if (cell == 0xFFFFFFFF)
            return 0xFFFFFFFF;

        half >>= 1;
    }

    return cell;
}

static float _get_normal_advance(const Vector3 &p_normal) {

    Vector3 normal = p_normal;
    Vector3 unorm = normal.abs();

    if ((unorm.x >= unorm.y) && (unorm.x >= unorm.z)) {
        // x code
        unorm = Vector3(copysignf(1.0f,normal.x), 0.0, 0.0);
    } else if ((unorm.y > unorm.x) && (unorm.y >= unorm.z)) {
        // y code
        unorm = Vector3(0.0, copysignf(1.0f,normal.y), 0.0);
    } else if ((unorm.z > unorm.x) && (unorm.z > unorm.y)) {
        // z code
        unorm = Vector3(0.0, 0.0f, copysignf(1.0f,normal.z));
    } else {
        // oh-no we messed up code
        // has to be
        unorm = Vector3(1.0, 0.0, 0.0);
    }

    return 1.0f / normal.dot(unorm);
}

static void _bake_gi_downscale_light(int p_idx, int p_level, const VisualServerScene::GIProbeDataCell *p_cells,
        const VisualServerScene::GIProbeDataHeader *p_header, VisualServerScene::InstanceGIProbeData::LocalData *p_local_data, float p_propagate) {

    //average light to upper level

    float divisor = 0;
    float sum[3] = { 0.0, 0.0, 0.0 };

    for (int i = 0; i < 8; i++) {

        uint32_t child = p_cells[p_idx].children[i];

        if (child == 0xFFFFFFFF)
            continue;

        if (p_level + 1 < (int)p_header->cell_subdiv - 1) {
            _bake_gi_downscale_light(child, p_level + 1, p_cells, p_header, p_local_data, p_propagate);
        }

        sum[0] += p_local_data[child].energy[0];
        sum[1] += p_local_data[child].energy[1];
        sum[2] += p_local_data[child].energy[2];
        divisor += 1.0f;
    }

    divisor = Math::lerp((float)8.0, divisor, p_propagate);
    sum[0] /= divisor;
    sum[1] /= divisor;
    sum[2] /= divisor;

    //divide by eight for average
    p_local_data[p_idx].energy[0] = Math::fast_ftoi(sum[0]);
    p_local_data[p_idx].energy[1] = Math::fast_ftoi(sum[1]);
    p_local_data[p_idx].energy[2] = Math::fast_ftoi(sum[2]);
}
void _gi_probe_fill_local_data(int p_idx, int p_level, int p_x, int p_y, int p_z,
        const VisualServerScene::GIProbeDataCell *p_cell, const VisualServerScene::GIProbeDataHeader *p_header,
        VisualServerScene::InstanceGIProbeData::LocalData *p_local_data, Vector<uint32_t> *prev_cell) {

    if ((uint32_t)p_level == p_header->cell_subdiv - 1) {

        Vector3 emission;
        emission.x = (p_cell[p_idx].emission >> 24) / 255.0f;
        emission.y = ((p_cell[p_idx].emission >> 16) & 0xFF) / 255.0f;
        emission.z = ((p_cell[p_idx].emission >> 8) & 0xFF) / 255.0f;
        float l = (p_cell[p_idx].emission & 0xFF) / 255.0f;
        l *= 8.0f;

        emission *= l;

        p_local_data[p_idx].energy[0] = uint16_t(emission.x * 1024); //go from 0 to 1024 for light
        p_local_data[p_idx].energy[1] = uint16_t(emission.y * 1024); //go from 0 to 1024 for light
        p_local_data[p_idx].energy[2] = uint16_t(emission.z * 1024); //go from 0 to 1024 for light
    } else {

        p_local_data[p_idx].energy[0] = 0;
        p_local_data[p_idx].energy[1] = 0;
        p_local_data[p_idx].energy[2] = 0;

        int half = (1 << (p_header->cell_subdiv - 1)) >> (p_level + 1);

        for (int i = 0; i < 8; i++) {

            uint32_t child = p_cell[p_idx].children[i];

            if (child == 0xFFFFFFFF)
                continue;

            int x = p_x;
            int y = p_y;
            int z = p_z;

            if (i & 1)
                x += half;
            if (i & 2)
                y += half;
            if (i & 4)
                z += half;

            _gi_probe_fill_local_data(child, p_level + 1, x, y, z, p_cell, p_header, p_local_data, prev_cell);
        }
    }

    //position for each part of the mipmaped texture
    p_local_data[p_idx].pos[0] = p_x >> (p_header->cell_subdiv - p_level - 1);
    p_local_data[p_idx].pos[1] = p_y >> (p_header->cell_subdiv - p_level - 1);
    p_local_data[p_idx].pos[2] = p_z >> (p_header->cell_subdiv - p_level - 1);

    prev_cell[p_level].emplace_back(p_idx);
}

static bool _check_gi_probe(RenderingInstanceComponent *p_gi_probe) {
    auto view_i(VSG::ecs->registry.view<RenderingInstanceComponent>());

    VisualServerScene::InstanceGIProbeData *probe_data = getUnchecked<VisualServerScene::InstanceGIProbeData>(p_gi_probe->self);

    probe_data->dynamic.light_cache_changes.clear();

    bool all_equal = true;
    const auto &scenario(VSG::ecs->registry.get<RenderingScenarioComponent>(p_gi_probe->scenario));

    for (RenderingEntity lght : scenario.directional_lights) {
        RenderingInstanceComponent &E(view_i.get<RenderingInstanceComponent>(lght));

        if (VSG::storage->light_get_bake_mode(E.base) == RS::LightBakeMode::LIGHT_BAKE_DISABLED)
            continue;

        assert(E.self==lght);
        VisualServerScene::InstanceGIProbeData::LightCache lc;
        lc.type = VSG::storage->light_get_type(E.base);
        lc.color = VSG::storage->light_get_color(E.base);
        lc.energy = VSG::storage->light_get_param(E.base, RS::LIGHT_PARAM_ENERGY) * VSG::storage->light_get_param(E.base, RS::LIGHT_PARAM_INDIRECT_ENERGY);
        lc.radius = VSG::storage->light_get_param(E.base, RS::LIGHT_PARAM_RANGE);
        lc.attenuation = VSG::storage->light_get_param(E.base, RS::LIGHT_PARAM_ATTENUATION);
        lc.spot_angle = VSG::storage->light_get_param(E.base, RS::LIGHT_PARAM_SPOT_ANGLE);
        lc.spot_attenuation = VSG::storage->light_get_param(E.base, RS::LIGHT_PARAM_SPOT_ATTENUATION);
        lc.transform = probe_data->dynamic.light_to_cell_xform * E.transform;
        lc.visible = E.visible;

        if (!probe_data->dynamic.light_cache.contains(lght) || probe_data->dynamic.light_cache[lght] != lc) {
            all_equal = false;
        }

        probe_data->dynamic.light_cache_changes[lght] = lc;
    }

    for (RenderingInstanceComponent * E : probe_data->lights) {

        if (VSG::storage->light_get_bake_mode(E->base) == RS::LightBakeMode::LIGHT_BAKE_DISABLED)
            continue;

        VisualServerScene::InstanceGIProbeData::LightCache lc;
        lc.type = VSG::storage->light_get_type(E->base);
        lc.color = VSG::storage->light_get_color(E->base);
        lc.energy = VSG::storage->light_get_param(E->base, RS::LIGHT_PARAM_ENERGY) * VSG::storage->light_get_param(E->base, RS::LIGHT_PARAM_INDIRECT_ENERGY);
        lc.radius = VSG::storage->light_get_param(E->base, RS::LIGHT_PARAM_RANGE);
        lc.attenuation = VSG::storage->light_get_param(E->base, RS::LIGHT_PARAM_ATTENUATION);
        lc.spot_angle = VSG::storage->light_get_param(E->base, RS::LIGHT_PARAM_SPOT_ANGLE);
        lc.spot_attenuation = VSG::storage->light_get_param(E->base, RS::LIGHT_PARAM_SPOT_ATTENUATION);
        lc.transform = probe_data->dynamic.light_to_cell_xform * E->transform;
        lc.visible = E->visible;

        if (!probe_data->dynamic.light_cache.contains(E->self) || probe_data->dynamic.light_cache[E->self] != lc) {
            all_equal = false;
        }

        probe_data->dynamic.light_cache_changes[E->self] = lc;
    }

    //lighting changed from after to before, must do some updating
    return !all_equal || probe_data->dynamic.light_cache_changes.size() != probe_data->dynamic.light_cache.size();
}
// thin wrapper to allow rooms / portals to take over culling if active
int _cull_convex_from_point(RenderingScenarioComponent *p_scenario, const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, Span<const Plane> p_convex, Span<RenderingEntity> p_result_array, int32_t &r_previous_room_id_hint, uint32_t p_mask = 0xFFFFFFFF) {
    int res = -1;

    if (p_scenario->_portal_renderer.is_active()) {
        // Note that the portal renderer ASSUMES that the planes exactly match the convention in
        // CameraMatrix of enum Planes (6 planes, in order, near, far etc)
        // If this is not the case, it should not be used.
        res = p_scenario->_portal_renderer.cull_convex(
                p_cam_transform, p_cam_projection, p_convex, p_result_array, p_mask, r_previous_room_id_hint);
    }
    // fallback to BVH  / octree if portals not active
    if (res == -1) {
        res = p_scenario->sps.cull_convex(p_convex, p_result_array, p_mask);

        // Opportunity for occlusion culling on the main scene. This will be a noop if no occluders.
        if (p_scenario->_portal_renderer.occlusion_is_active()) {
            res = p_scenario->_portal_renderer.occlusion_cull(
                    p_cam_transform, p_cam_projection, p_convex, p_result_array, res);
    }
    }
    return res;
}

static void _ghost_create_occlusion_rep(OcclusionGhostComponent *p_ghost) {
    ERR_FAIL_COND(!p_ghost);
    RenderingScenarioComponent *pscenario = get<RenderingScenarioComponent>(p_ghost->scenario);
    ERR_FAIL_COND(!pscenario);

    if (!p_ghost->rghost_handle) {
        p_ghost->rghost_handle =pscenario->_portal_renderer.rghost_create(p_ghost->object_id, p_ghost->aabb);
    }
}

static void _ghost_destroy_occlusion_rep(OcclusionGhostComponent *p_ghost) {
    ERR_FAIL_COND(!p_ghost);

    // not an error, can occur
    if (!p_ghost->rghost_handle) {
        return;
    }

    RenderingScenarioComponent *pscenario = get<RenderingScenarioComponent>(p_ghost->scenario);
    ERR_FAIL_COND(!pscenario);
    pscenario->_portal_renderer.rghost_destroy(p_ghost->rghost_handle);
    p_ghost->rghost_handle = 0;
}
static void _rooms_instance_update(RenderingInstanceComponent *p_instance, const AABB &p_aabb) {
    // magic number for instances in the room / portal system, but not requiring an update
    // (due to being a STATIC or DYNAMIC object within a room)
    // Must match the value in PortalRenderer in VisualServer
    const uint32_t OCCLUSION_HANDLE_ROOM_BIT = 1 << 31;

    // if the instance is a moving object in the room / portal system, update it
    // Note that if rooms and portals is not in use, occlusion_handle should be zero in all cases unless the portal_mode
    // has been set to global or roaming. (which is unlikely as the default is static).
    // The exception is editor user interface elements.
    // These are always set to global and will always keep their aabb up to date in the portal renderer unnecessarily.
    // There is no easy way around this, but it should be very cheap, and have no impact outside the editor.
    if (p_instance->occlusion_handle && (p_instance->occlusion_handle != OCCLUSION_HANDLE_ROOM_BIT)) {
        RenderingScenarioComponent *pscenario = get<RenderingScenarioComponent>(p_instance->scenario);

        pscenario->_portal_renderer.instance_moving_update(p_instance->occlusion_handle, p_aabb);
    }
}
} // end of anonymous namespace

//! when an instace's source instantiable changes, we mark them for an update
void set_instance_dirty(RenderingEntity id, bool p_update_aabb, bool p_update_materials) {

    // must have an instance!
    assert(VSG::ecs->registry.any_of<RenderingInstanceComponent>(id));
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(id) ||
           get<RenderingInstanceComponent>(id)->self==id
           );
    auto &reg = VSG::ecs->registry;
    if (!has_component<Dirty>(id)) {
        reg.emplace<Dirty>(id, p_update_aabb,p_update_materials);
    }
    else if(p_update_aabb|| p_update_materials) {
        auto &c_data(get_component<Dirty>(id));
        c_data.update_aabb |= p_update_aabb;
        c_data.update_materials |= p_update_materials;
    }
}

/* CAMERA API */

RenderingEntity VisualServerScene::camera_create() {
    auto eid = VSG::ecs->create();
    VSG::ecs->registry.emplace<Camera3DComponent>(eid);
    return eid;
}

void VisualServerScene::camera_set_perspective(RenderingEntity p_camera, float p_fovy_degrees, float p_z_near, float p_z_far) {

    ERR_FAIL_COND(!VSG::ecs->registry.valid(p_camera) || !VSG::ecs->registry.any_of<Camera3DComponent>(p_camera));

    Camera3DComponent &camera = VSG::ecs->registry.get<Camera3DComponent>(p_camera);

    camera.type = Camera3DComponent::PERSPECTIVE;
    camera.fov = p_fovy_degrees;
    camera.znear = p_z_near;
    camera.zfar = p_z_far;
}

void VisualServerScene::camera_set_orthogonal(RenderingEntity p_camera, float p_size, float p_z_near, float p_z_far) {
    ERR_FAIL_COND(!VSG::ecs->registry.valid(p_camera) || !VSG::ecs->registry.any_of<Camera3DComponent>(p_camera));
    Camera3DComponent &camera = VSG::ecs->registry.get<Camera3DComponent>(p_camera);

    camera.type = Camera3DComponent::ORTHOGONAL;
    camera.size = p_size;
    camera.znear = p_z_near;
    camera.zfar = p_z_far;
}

void VisualServerScene::camera_set_frustum(RenderingEntity p_camera, float p_size, Vector2 p_offset, float p_z_near, float p_z_far) {
    ERR_FAIL_COND(!VSG::ecs->registry.valid(p_camera) || !VSG::ecs->registry.any_of<Camera3DComponent>(p_camera));

    Camera3DComponent &camera = VSG::ecs->registry.get<Camera3DComponent>(p_camera);

    camera.type = Camera3DComponent::FRUSTUM;
    camera.size = p_size;
    camera.offset = p_offset;
    camera.znear = p_z_near;
    camera.zfar = p_z_far;
}

void VisualServerScene::camera_set_transform(RenderingEntity p_camera, const Transform &p_transform) {
    ERR_FAIL_COND(!VSG::ecs->registry.valid(p_camera) || !VSG::ecs->registry.any_of<Camera3DComponent>(p_camera));

    Camera3DComponent &camera = VSG::ecs->registry.get<Camera3DComponent>(p_camera);

    camera.transform = p_transform.orthonormalized();
}

void VisualServerScene::camera_set_cull_mask(RenderingEntity p_camera, uint32_t p_layers) {
    ERR_FAIL_COND(!VSG::ecs->registry.valid(p_camera) || !VSG::ecs->registry.any_of<Camera3DComponent>(p_camera));

    Camera3DComponent &camera = VSG::ecs->registry.get<Camera3DComponent>(p_camera);

    camera.visible_layers = p_layers;
}

void VisualServerScene::camera_set_environment(RenderingEntity p_camera, RenderingEntity p_env) {
    ERR_FAIL_COND(!VSG::ecs->registry.valid(p_camera) || !VSG::ecs->registry.any_of<Camera3DComponent>(p_camera));
    Camera3DComponent &camera = VSG::ecs->registry.get<Camera3DComponent>(p_camera);

    camera.env = p_env;
}

/* SPATIAL PARTITIONING */
SpatialPartitionID SpatialPartitioningScene_BVH::create(RenderingEntity p_userdata, const AABB &p_aabb, int p_subindex, bool p_pairable, uint32_t p_pairable_type, uint32_t p_pairable_mask) {
    // we are relying on this instance to be valid in order to pass
    // the visible flag to the bvh.
    assert(VSG::ecs->registry.valid(p_userdata));
    auto *inst= get<RenderingInstanceComponent>(p_userdata);
    assert(inst!=nullptr);

    // cache the pairable mask and pairable type on the instance as it is needed for user callbacks from the BVH, and this is
    // too complex to calculate each callback...
    inst->bvh_pairable_mask = p_pairable_mask;
    inst->bvh_pairable_type = p_pairable_type;

    uint32_t tree_id = p_pairable ? 1 : 0;
    uint32_t tree_collision_mask = 3;

    auto res = _bvh.create(p_userdata, inst->visible, tree_id, tree_collision_mask, p_aabb, p_subindex) + 1;
    check_bvh_userdata();
    return res;
}

void SpatialPartitioningScene_BVH::activate(SpatialPartitionID p_handle, const AABB &p_aabb) {
    // be very careful here, we are deferring the collision check, expecting a set_pairable to be called
    // immediately after.
    // see the notes in the BVH function.
    _bvh.activate(p_handle - 1, p_aabb, true);
}

void SpatialPartitioningScene_BVH::deactivate(SpatialPartitionID p_handle) {
    _bvh.deactivate(p_handle - 1);
}

void SpatialPartitioningScene_BVH::force_collision_check(SpatialPartitionID p_handle) {
    _bvh.force_collision_check(p_handle - 1);
}

void SpatialPartitioningScene_BVH::set_pairable(RenderingInstanceComponent *p_instance, bool p_pairable,
        uint32_t p_pairable_type, uint32_t p_pairable_mask) {
    SpatialPartitionID handle = p_instance->spatial_partition_id;

    p_instance->bvh_pairable_mask = p_pairable_mask;
    p_instance->bvh_pairable_type = p_pairable_type;

    uint32_t tree_id = p_pairable ? 1 : 0;
    uint32_t tree_collision_mask = 3;

    _bvh.set_tree(handle - 1, tree_id, tree_collision_mask);
}

int SpatialPartitioningScene_BVH::cull_convex(Span<const Plane> p_convex, Span<RenderingEntity> p_result_array, uint32_t p_mask) {
    check_bvh_userdata();
    auto &ric = VSG::ecs->registry.get<RenderingInstanceComponent>(_dummy_cull_object);
    ric.bvh_pairable_mask = p_mask;
    ric.bvh_pairable_type = 0;
    return _bvh.cull_convex(p_convex, p_result_array, _dummy_cull_object);
}

int SpatialPartitioningScene_BVH::cull_aabb(const AABB &p_aabb, Span<RenderingEntity> p_result_array, int *p_subindex_array, uint32_t p_mask) {
    check_bvh_userdata();
    auto &ric = VSG::ecs->registry.get<RenderingInstanceComponent>(_dummy_cull_object);
    ric.bvh_pairable_mask = p_mask;
    ric.bvh_pairable_type = 0;
    return _bvh.cull_aabb(p_aabb, p_result_array, _dummy_cull_object, 0xFFFFFFFF, p_subindex_array);
}

int SpatialPartitioningScene_BVH::cull_segment(const Vector3 &p_from, const Vector3 &p_to, Span<RenderingEntity> p_result_array, int *p_subindex_array, uint32_t p_mask) {
    check_bvh_userdata();
    auto &ric = VSG::ecs->registry.get<RenderingInstanceComponent>(_dummy_cull_object);
    ric.bvh_pairable_mask = p_mask;
    ric.bvh_pairable_type = 0;
    return _bvh.cull_segment(p_from, p_to, p_result_array, _dummy_cull_object, 0xFFFFFFFF, p_subindex_array);
}

void SpatialPartitioningScene_BVH::check_bvh_userdata()
{
    _bvh.visit_all_user_data([](RenderingEntity r) {
        assert(VSG::ecs->registry.valid(r));
    });
}

SpatialPartitioningScene_BVH::SpatialPartitioningScene_BVH()
{
    _bvh.params_set_pairing_expansion(T_GLOBAL_GET<float>("rendering/quality/spatial_partitioning/bvh_collision_margin"));
    _dummy_cull_object = VSG::ecs->create();//
    VSG::ecs->registry.emplace<RenderingInstanceComponent>(_dummy_cull_object,_dummy_cull_object);
}

SpatialPartitioningScene_BVH::~SpatialPartitioningScene_BVH() {
    VSG::ecs->registry.destroy(_dummy_cull_object);
}

void VisualServerScene::camera_set_use_vertical_aspect(RenderingEntity p_camera, bool p_enable) {
    ERR_FAIL_COND(!VSG::ecs->registry.valid(p_camera) || !VSG::ecs->registry.any_of<Camera3DComponent>(p_camera));

    Camera3DComponent &camera = VSG::ecs->registry.get<Camera3DComponent>(p_camera);

    camera.vaspect = p_enable;
}
bool VisualServerScene::owns_camera(RenderingEntity p_camera) {
    return VSG::ecs->registry.valid(p_camera) && VSG::ecs->registry.any_of<Camera3DComponent>(p_camera);
}
/* SCENARIO API */

void *VisualServerScene::_instance_pair(void *p_self, SpatialPartitionID, RenderingEntity p_A, int, SpatialPartitionID, RenderingEntity p_B, int) {

    //VisualServerScene *self = (VisualServerScene*)p_self;
    RenderingInstanceComponent *A = getUnchecked< RenderingInstanceComponent>(p_A);
    RenderingInstanceComponent *B = getUnchecked< RenderingInstanceComponent>(p_B);

    //instance indices are designed so greater always contains lesser
    if (A->base_type > B->base_type) {
        SWAP(A, B); //lesser always first
        SWAP(p_A, p_B); //lesser always first
    }
    ComponentPairInfo pair_info { p_B, p_A };

    if(A->base_type == RS::INSTANCE_MESH) {
        assert(has_component<GeometryComponent>(p_A));
    }

    if (B->base_type == RS::INSTANCE_LIGHT && has_component<GeometryComponent>(p_A)) {

        InstanceLightData *light = getUnchecked<InstanceLightData>(p_B);
        InstanceGeometryData *geom = get_instance_geometry(p_A);

        geom->lighting.emplace_back(p_B);

        auto E = light->geometries.insert(light->geometries.end(),pair_info);
        GeometryComponent &cm_geom(get_component<GeometryComponent>(p_A));
        if (cm_geom.can_cast_shadows) {
            light->shadow_dirty = true;
        }
        cm_geom.lighting_dirty = true;

        return E.mpNode; //this element should make freeing faster
    } else if (B->base_type == RS::INSTANCE_REFLECTION_PROBE && has_component<GeometryComponent>(p_A)) {

        InstanceReflectionProbeData *reflection_probe = getUnchecked<InstanceReflectionProbeData>(p_B);
        InstanceGeometryData *geom = get_instance_geometry(p_A);

        geom->reflection_probes.emplace_back(p_B);

        auto E = reflection_probe->geometries.insert(reflection_probe->geometries.end(),pair_info);

        get_component<GeometryComponent>(p_A).reflection_dirty = true;

        return E.mpNode; //this element should make freeing faster
    } else if (B->base_type == RS::INSTANCE_LIGHTMAP_CAPTURE && has_component<GeometryComponent>(p_A)) {

        RenderingInstanceLightmapCaptureDataComponent *lightmap_capture = getUnchecked<RenderingInstanceLightmapCaptureDataComponent>(p_B);
        InstanceGeometryData *geom = get_instance_geometry(p_A);

        geom->lightmap_captures.emplace_back(p_B);

        auto E = lightmap_capture->geometries.insert(lightmap_capture->geometries.end(),pair_info);
        ((VisualServerScene *)p_self)->_instance_queue_update(A, false, false); //need to update capture

        return E.mpNode; //this element should make freeing faster
    } else if (B->base_type == RS::INSTANCE_GI_PROBE && has_component<GeometryComponent>(p_A)) {

        InstanceGIProbeData *gi_probe = getUnchecked<InstanceGIProbeData>(p_B);
        InstanceGeometryData *geom = get_instance_geometry(p_A);

        geom->gi_probes.emplace_back(p_B);

        auto E = gi_probe->geometries.insert(gi_probe->geometries.end(),pair_info);

        get_component<GeometryComponent>(p_A).gi_probes_dirty = true;

        return E.mpNode; //this element should make freeing faster

    } else if (B->base_type == RS::INSTANCE_GI_PROBE && A->base_type == RS::INSTANCE_LIGHT) {

        InstanceGIProbeData *gi_probe = getUnchecked<InstanceGIProbeData>(p_B);
        gi_probe->lights.insert(A);
        return A;
    }

    return nullptr;
}
void VisualServerScene::_instance_unpair(void *p_self, SpatialPartitionID, RenderingEntity p_A, int, SpatialPartitionID, RenderingEntity p_B, int, void *udata) {
    static_assert(sizeof(List<ComponentPairInfo>::iterator)==sizeof(void*));
    //VisualServerScene *self = (VisualServerScene*)p_self;
    auto *A = get<RenderingInstanceComponent>(p_A);
    auto *B = get<RenderingInstanceComponent>(p_B);

    //instance indices are designed so greater always contains lesser
    if (A->base_type > B->base_type) {
        SWAP(A, B); //lesser always first
        SWAP(p_A,p_B);
    }

    if (B->base_type == RS::INSTANCE_LIGHT && (has_component<GeometryComponent>(p_A))) {

        InstanceLightData *light = getUnchecked<InstanceLightData>(p_B);
        InstanceGeometryData *geom = get_instance_geometry(p_A);

        List<ComponentPairInfo>::iterator E(reinterpret_cast<eastl::ListNode<ComponentPairInfo> *>(udata));

        geom->lighting.erase_first_unsorted(E->L);
        light->geometries.erase(E);
        GeometryComponent &cm_geom(get_component<GeometryComponent>(p_A));
        if (cm_geom.can_cast_shadows) {
            light->shadow_dirty = true;
        }
        cm_geom.lighting_dirty = true;

    } else if (B->base_type == RS::INSTANCE_REFLECTION_PROBE && (has_component<GeometryComponent>(p_A))) {

        InstanceReflectionProbeData *reflection_probe = getUnchecked<InstanceReflectionProbeData>(p_B);
        InstanceGeometryData *geom = get_instance_geometry(p_A);

        List<ComponentPairInfo>::iterator E(reinterpret_cast<eastl::ListNode<ComponentPairInfo> *>(udata));

        geom->reflection_probes.erase_first_unsorted(E->L);
        reflection_probe->geometries.erase(E);

        get_component<GeometryComponent>(p_A).reflection_dirty = true;

    } else if (B->base_type == RS::INSTANCE_LIGHTMAP_CAPTURE && (has_component<GeometryComponent>(p_A))) {

        RenderingInstanceLightmapCaptureDataComponent *lightmap_capture = getUnchecked<RenderingInstanceLightmapCaptureDataComponent>(p_B);
        InstanceGeometryData *geom = get_instance_geometry(p_A);

        List<ComponentPairInfo>::iterator E(reinterpret_cast<eastl::ListNode<ComponentPairInfo> *>(udata));

        geom->lightmap_captures.erase_first_unsorted(E->L);
        lightmap_capture->geometries.erase(E);
        //need to update capture
        ::set_instance_dirty(p_A,false,false);
        //((VisualServerScene *)p_self)->_instance_queue_update(A, false, false);

    } else if (B->base_type == RS::INSTANCE_GI_PROBE && (has_component<GeometryComponent>(p_A))) {

        InstanceGIProbeData *gi_probe = getUnchecked<InstanceGIProbeData>(p_B);
        InstanceGeometryData *geom = get_instance_geometry(p_A);

        List<ComponentPairInfo>::iterator E(reinterpret_cast<eastl::ListNode<ComponentPairInfo> *>(udata));

        geom->gi_probes.erase_first_unsorted(E->L);
        gi_probe->geometries.erase(E);

        get_component<GeometryComponent>(p_A).gi_probes_dirty = true;

    } else if (B->base_type == RS::INSTANCE_GI_PROBE && A->base_type == RS::INSTANCE_LIGHT) {

        InstanceGIProbeData *gi_probe = getUnchecked<InstanceGIProbeData>(p_B);
        RenderingInstanceComponent *E = reinterpret_cast<RenderingInstanceComponent *>(udata);

        gi_probe->lights.erase(E);
    }
}

RenderingEntity VisualServerScene::scenario_create() {
    auto res = VSG::ecs->create();
    auto &scenario(VSG::ecs->registry.emplace<RenderingScenarioComponent>(res));

    scenario.self = res;
    scenario.sps.set_pair_callback(_instance_pair, this);
    scenario.sps.set_unpair_callback(_instance_unpair, this);
    scenario.reflection_probe_shadow_atlas = VSG::scene_render->shadow_atlas_create();
    VSG::scene_render->shadow_atlas_set_size(scenario.reflection_probe_shadow_atlas, 1024); //make enough shadows for close distance, don't bother with rest
    VSG::scene_render->shadow_atlas_set_quadrant_subdivision(scenario.reflection_probe_shadow_atlas, 0, 4);
    VSG::scene_render->shadow_atlas_set_quadrant_subdivision(scenario.reflection_probe_shadow_atlas, 1, 4);
    VSG::scene_render->shadow_atlas_set_quadrant_subdivision(scenario.reflection_probe_shadow_atlas, 2, 4);
    VSG::scene_render->shadow_atlas_set_quadrant_subdivision(scenario.reflection_probe_shadow_atlas, 3, 8);
    scenario.reflection_atlas = VSG::scene_render->reflection_atlas_create();

    return res;
}
void VisualServerScene::tick() {
    //if (_interpolation_data.interpolation_enabled) {
    //    update_interpolation_tick(true);
    //}
}

void VisualServerScene::pre_draw(bool p_will_draw) {
    // even when running and not drawing scenes, we still need to clear intermediate per frame
    // interpolation data .. hence the p_will_draw flag (so we can reduce the processing if the frame
    // will not be drawn)
    //if (_interpolation_data.interpolation_enabled) {
        //update_interpolation_frame(p_will_draw);
   // }
}

void VisualServerScene::scenario_set_debug(RenderingEntity p_scenario, RS::ScenarioDebugMode p_debug_mode) {

    RenderingScenarioComponent *scenario = getUnchecked<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    scenario->debug = p_debug_mode;
}

void VisualServerScene::scenario_set_environment(RenderingEntity p_scenario, RenderingEntity p_environment) {

    RenderingScenarioComponent *scenario = getUnchecked<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    scenario->environment = p_environment;
}

void VisualServerScene::scenario_set_fallback_environment(RenderingEntity p_scenario, RenderingEntity p_environment) {

    RenderingScenarioComponent *scenario = getUnchecked<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    scenario->fallback_environment = p_environment;
}

void VisualServerScene::scenario_set_reflection_atlas_size(RenderingEntity p_scenario, int p_size, int p_subdiv) {

    RenderingScenarioComponent *scenario = getUnchecked<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    VSG::scene_render->reflection_atlas_set_size(scenario->reflection_atlas, p_size);
    VSG::scene_render->reflection_atlas_set_subdivision(scenario->reflection_atlas, p_subdiv);
}

/* INSTANCING API */

void VisualServerScene::_instance_queue_update(RenderingInstanceComponent *p_instance, bool p_update_aabb, bool p_update_materials) {

    ::set_instance_dirty(p_instance->self, p_update_aabb, p_update_materials);

}

RenderingEntity VisualServerScene::instance_create() {
    auto instance_rid = VSG::ecs->create();
    VSG::ecs->registry.emplace<RenderingInstanceComponent>(instance_rid, instance_rid);
    VSG::ecs->registry.emplace<InstanceBoundsComponent>(instance_rid);
    return instance_rid;
}

void VisualServerScene::instance_set_base(RenderingEntity p_instance, RenderingEntity p_base) {
    ::instance_set_base(p_instance,p_base);
}

void VisualServerScene::instance_set_scenario(RenderingEntity p_instance, RenderingEntity p_scenario) {

    ::instance_set_scenario(p_instance,p_scenario);
}

void VisualServerScene::instance_set_layer_mask(RenderingEntity p_instance, uint32_t p_mask) {

    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    if (instance->layer_mask == p_mask) {
        return;
    }
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );

    instance->layer_mask = p_mask;
    // update lights to show / hide shadows according to the new mask
    if ((1 << instance->base_type) & RS::INSTANCE_GEOMETRY_MASK) {
        GeometryComponent *geom = get<GeometryComponent>(p_instance);

        if (geom->can_cast_shadows) {
            for (auto E : geom->Data->lighting) {
                InstanceLightData *light = get<InstanceLightData>(E);
                light->shadow_dirty = true;
            }
        }
    }
}
void VisualServerScene::instance_set_transform(RenderingEntity p_instance, const Transform &p_transform) {

    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );

    if (instance->transform == p_transform)
        return; //must be checked to avoid worst evil

#ifdef DEBUG_ENABLED

    for (int i = 0; i < 4; i++) {
        const Vector3 &v = i < 3 ? p_transform.basis.elements[i] : p_transform.origin;
        ERR_FAIL_COND(Math::is_inf(v.x));
        ERR_FAIL_COND(Math::is_nan(v.x));
        ERR_FAIL_COND(Math::is_inf(v.y));
        ERR_FAIL_COND(Math::is_nan(v.y));
        ERR_FAIL_COND(Math::is_inf(v.z));
        ERR_FAIL_COND(Math::is_nan(v.z));
    }

#endif
    instance->transform = p_transform;
    _instance_queue_update(instance, true);
}
void VisualServerScene::instance_attach_object_instance_id(RenderingEntity p_instance, GameEntity p_id) {

    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );
    instance->object_id = p_id;
}
void VisualServerScene::instance_set_blend_shape_weight(RenderingEntity p_instance, int p_shape, float p_weight) {

    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );

    if (!has_component<Dirty>(p_instance)) { // not marked for update, do it now?
        _update_dirty_instance(instance);
    }

    ERR_FAIL_INDEX(p_shape, instance->blend_values.size());
    instance->blend_values[p_shape] = p_weight;
    VSG::storage->mesh_set_blend_shape_values(instance->base, instance->blend_values);
}

void VisualServerScene::instance_set_surface_material(RenderingEntity p_instance, int p_surface, RenderingEntity p_material) {
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );

    if (instance->base_type == RS::INSTANCE_MESH) {
        //may not have been updated yet
        instance->materials.resize(VSG::storage->mesh_get_surface_count(instance->base),entt::null);
    }

    ERR_FAIL_INDEX(p_surface, instance->materials.size());

    if (instance->materials[p_surface]!=entt::null) {
        VSG::storage->material_remove_instance_owner(instance->materials[p_surface], p_instance);
    }
    instance->materials[p_surface] = p_material;
    instance->base_changed(false, true);

    if (instance->materials[p_surface]!=entt::null) {
        VSG::storage->material_add_instance_owner(instance->materials[p_surface], p_instance);
    }
}

void VisualServerScene::instance_set_visible(RenderingEntity p_instance, bool p_visible) {
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );

    if (instance->visible == p_visible)
        return;

    instance->visible = p_visible;
    auto *scenario = instance->scenario!=entt::null ? get<RenderingScenarioComponent>(instance->scenario) : nullptr;
    // give the opportunity for the spatial partitioning scene to use a special implementation of visibility
    // for efficiency (supported in BVH but not octree)

    // slightly bug prone optimization here - we want to avoid doing a collision check twice
    // once when activating, and once when calling set_pairable. We do this by deferring the collision check.
    // However, in some cases (notably meshes), set_pairable never gets called. So we want to catch this case
    // and force a collision check (see later in this function).
    // This is only done in two stages to maintain compatibility with the octree.
    if (instance->spatial_partition_id && scenario) {

        if (p_visible) {
            InstanceBoundsComponent& bounds = get_component<InstanceBoundsComponent>(p_instance);
            scenario->sps.activate(instance->spatial_partition_id, bounds.transformed_aabb);
        } else {
            scenario->sps.deactivate(instance->spatial_partition_id);
        }
    }
    // when showing or hiding geometry, lights must be kept up to date to show / hide shadows
    if ((1 << instance->base_type) & RS::INSTANCE_GEOMETRY_MASK) {
        InstanceGeometryData *geom = get_instance_geometry(instance->self);
        auto &cm_geom(get_component<GeometryComponent>(instance->self));

        if (cm_geom.can_cast_shadows) {
            for (auto E : geom->lighting) {
                InstanceLightData *light = getUnchecked<InstanceLightData>(E);
                light->shadow_dirty = true;
            }
        }
    }
    if(!scenario || !instance->spatial_partition_id) {
        return;
    }
    switch (instance->base_type) {
        case RS::INSTANCE_LIGHT: {
            if (VSG::storage->light_get_type(instance->base) != RS::LIGHT_DIRECTIONAL) {
                scenario->sps.set_pairable(instance, p_visible, 1 << RS::INSTANCE_LIGHT, p_visible ? RS::INSTANCE_GEOMETRY_MASK : 0);
            }

        } break;
        case RS::INSTANCE_REFLECTION_PROBE: {
                scenario->sps.set_pairable(instance, p_visible, 1 << RS::INSTANCE_REFLECTION_PROBE, p_visible ? RS::INSTANCE_GEOMETRY_MASK : 0);
        } break;
        case RS::INSTANCE_LIGHTMAP_CAPTURE: {
            scenario->sps.set_pairable(instance, p_visible, 1 << RS::INSTANCE_LIGHTMAP_CAPTURE, p_visible ? RS::INSTANCE_GEOMETRY_MASK : 0);
        } break;
        case RS::INSTANCE_GI_PROBE: {
            scenario->sps.set_pairable(instance, p_visible, 1 << RS::INSTANCE_GI_PROBE, p_visible ? (RS::INSTANCE_GEOMETRY_MASK | (1 << RS::INSTANCE_LIGHT)) : 0);
        } break;
        default: {
            // if we haven't called set_pairable, we STILL need to do a collision check
            // for activated items because we deferred it earlier in the call to activate.
            if (instance->spatial_partition_id && scenario && p_visible) {
                scenario->sps.force_collision_check(instance->spatial_partition_id);
            }
        }
    }
}

inline bool is_geometry_instance(RS::InstanceType p_type) {
    return p_type == RS::INSTANCE_MESH || p_type == RS::INSTANCE_MULTIMESH || p_type == RS::INSTANCE_PARTICLES || p_type == RS::INSTANCE_IMMEDIATE;
}

void VisualServerScene::instance_set_use_lightmap(RenderingEntity p_instance, RenderingEntity p_lightmap_instance, RenderingEntity p_lightmap, int p_lightmap_slice, const Rect2 &p_lightmap_uv_rect) {
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );
    ::instance_set_use_lightmap(p_instance, p_lightmap_instance, p_lightmap, p_lightmap_slice, p_lightmap_uv_rect);
}

void VisualServerScene::instance_set_custom_aabb(RenderingEntity p_instance, AABB p_aabb) {

    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    ERR_FAIL_COND(!is_geometry_instance(instance->base_type));
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );

    InstanceBoundsComponent& bounds = get_component<InstanceBoundsComponent>(p_instance);

    if (p_aabb != AABB()) {

        bounds.custom_aabb = p_aabb;
        bounds.use_custom_aabb = true;
    } else {

        // Clear custom AABB
        bounds.use_custom_aabb = false;
    }

    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(instance->scenario);
    if (scenario)
        _instance_queue_update(instance, true, false);
}

void VisualServerScene::instance_attach_skeleton(RenderingEntity p_instance, RenderingEntity p_skeleton) {
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );
    ::instance_attach_skeleton(p_instance, p_skeleton);
}

void VisualServerScene::instance_set_extra_visibility_margin(RenderingEntity p_instance, real_t p_margin) {
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );

    InstanceBoundsComponent& bounds = get_component<InstanceBoundsComponent>(p_instance);
    bounds.extra_margin = p_margin;
    _instance_queue_update(instance, true, false);
}

static void collect_culled(Span<RenderingEntity> src,Vector<GameEntity> &dst) {
    dst.reserve(src.size()/2);
    auto view(VSG::ecs->registry.view<RenderingInstanceComponent>());
    for (RenderingEntity ic : src) {
        assert(ic!=entt::null);
        auto &instance = view.get<RenderingInstanceComponent>(ic);
        if (instance.object_id==entt::null)
            continue;

        dst.emplace_back(instance.object_id);
    }
}
// Portals


void _instance_create_occlusion_rep(RenderingInstanceComponent *p_instance) {
    ERR_FAIL_COND(!p_instance);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_instance->scenario);
    ERR_FAIL_COND(!scenario);
    InstanceBoundsComponent *bounds = get<InstanceBoundsComponent>(p_instance->self);
    ERR_FAIL_COND(!bounds);


    switch (p_instance->portal_mode) {
        default: {
            p_instance->occlusion_handle = 0;
        } break;
        case RS::InstancePortalMode::INSTANCE_PORTAL_MODE_ROAMING: {
            p_instance->occlusion_handle = scenario->_portal_renderer.instance_moving_create(p_instance, p_instance->self, false, bounds->transformed_aabb);
        } break;
        case RS::InstancePortalMode::INSTANCE_PORTAL_MODE_GLOBAL: {
            p_instance->occlusion_handle = scenario->_portal_renderer.instance_moving_create(p_instance, p_instance->self, true, bounds->transformed_aabb);
        } break;
    }
}
void _instance_destroy_occlusion_rep(RenderingInstanceComponent *p_instance) {
    ERR_FAIL_COND(!p_instance);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_instance->scenario);
    ERR_FAIL_COND(!scenario);

    // not an error, can occur
    if (!p_instance->occlusion_handle) {
        return;
    }

    scenario->_portal_renderer.instance_moving_destroy(p_instance->occlusion_handle);

    // unset
    p_instance->occlusion_handle = 0;
}

void VisualServerScene::instance_set_portal_mode(RenderingEntity p_instance, RS::InstancePortalMode p_mode) {
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);

    // no change?
    if (instance->portal_mode == p_mode) {
        return;
    }

    // should this happen?
    if (instance->scenario==entt::null) {
        instance->portal_mode = p_mode;
        return;
    }

    // destroy previous occlusion instance?
    _instance_destroy_occlusion_rep(instance);
    instance->portal_mode = p_mode;
    _instance_create_occlusion_rep(instance);
}

bool _instance_get_transformed_aabb(RenderingEntity p_instance, AABB &r_aabb) {
    InstanceBoundsComponent *bounds = get<InstanceBoundsComponent>(p_instance);
    ERR_FAIL_COND_V(!bounds,false);
    r_aabb = bounds->transformed_aabb;
    return true;
}

GameEntity _instance_get_object_ID(VSInstance *p_instance) {
    if (p_instance) {
        return ((RenderingInstanceComponent *)p_instance)->object_id;
    }
    return entt::null;
}

VSInstance *_instance_get_from_rid(RenderingEntity p_instance) {
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    return instance;
}

bool _instance_get_transformed_aabb_for_occlusion(RenderingEntity p_instance, AABB &r_aabb) {
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    InstanceBoundsComponent *bounds = get<InstanceBoundsComponent>(p_instance);
    ERR_FAIL_COND_V(!bounds, false);

    r_aabb = bounds->transformed_aabb;
    return instance->portal_mode != RS::INSTANCE_PORTAL_MODE_GLOBAL;
}

bool _instance_cull_check(const VSInstance *p_instance, uint32_t p_cull_mask) {
    uint32_t pairable_type = 1 << ((const RenderingInstanceComponent *)p_instance)->base_type;
    return pairable_type & p_cull_mask;
}

// the portal has to be associated with a scenario, this is assumed to be
// the same scenario as the portal node
RenderingEntity VisualServerScene::portal_create() {
    RenderingEntity instance_rid = VSG::ecs->create();

    VSG::ecs->registry.emplace<PortalComponent>(instance_rid);
    return instance_rid;
}
// should not be called multiple times, different scenarios etc, but just in case, we will support this
void VisualServerScene::portal_set_scenario(RenderingEntity p_portal, RenderingEntity p_scenario) {
    PortalComponent *portal = get<PortalComponent>(p_portal);
    ERR_FAIL_COND(!portal);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);

    // noop?
    if (portal->scenario == p_scenario) {
        return;
    }

    // if the portal is in a scenario already, remove it
    if (portal->scenario!=entt::null) {
        RenderingScenarioComponent *pscenario = get<RenderingScenarioComponent>(portal->scenario);
        ERR_FAIL_COND(!scenario);

        pscenario->_portal_renderer.portal_destroy(portal->scenario_portal_id);
        portal->scenario = entt::null;
        portal->scenario_portal_id = 0;
    }

    // create when entering the world
    if (scenario) {
        portal->scenario = p_scenario;

        // defer the actual creation to here
        portal->scenario_portal_id = scenario->_portal_renderer.portal_create();
    }
}

void VisualServerScene::portal_set_geometry(RenderingEntity p_portal, const Vector<Vector3> &p_points, real_t p_margin) {
    PortalComponent *portal = get<PortalComponent>(p_portal);
    ERR_FAIL_COND(!portal);
    RenderingScenarioComponent *pscenario = get<RenderingScenarioComponent>(portal->scenario);
    ERR_FAIL_COND(!pscenario);
    pscenario->_portal_renderer.portal_set_geometry(portal->scenario_portal_id, p_points, p_margin);
}

void VisualServerScene::portal_link(RenderingEntity p_portal, RenderingEntity p_room_from, RenderingEntity p_room_to, bool p_two_way) {
    PortalComponent *portal = get<PortalComponent>(p_portal);
    ERR_FAIL_COND(!portal);
    RenderingScenarioComponent *pscenario = get<RenderingScenarioComponent>(portal->scenario);
    ERR_FAIL_COND(!pscenario);

    RoomComponent *room_from = get<RoomComponent>(p_room_from);
    ERR_FAIL_COND(!room_from);
    RoomComponent *room_to = get<RoomComponent>(p_room_to);
    ERR_FAIL_COND(!room_to);

    pscenario->_portal_renderer.portal_link(portal->scenario_portal_id, room_from->scenario_room_id, room_to->scenario_room_id, p_two_way);
}

void VisualServerScene::portal_set_active(RenderingEntity p_portal, bool p_active) {
    PortalComponent *portal = get<PortalComponent>(p_portal);
    ERR_FAIL_COND(!portal);
    RenderingScenarioComponent *pscenario = get<RenderingScenarioComponent>(portal->scenario);
    ERR_FAIL_COND(!pscenario);
    pscenario->_portal_renderer.portal_set_active(portal->scenario_portal_id, p_active);
}

RenderingEntity RoomAPI::ghost_create() {
    RenderingEntity instance_rid = VSG::ecs->create();

    VSG::ecs->registry.emplace<OcclusionGhostComponent>(instance_rid);
    return instance_rid;
}

void RoomAPI::ghost_set_scenario(RenderingEntity p_ghost, RenderingEntity p_scenario, GameEntity p_id, const AABB &p_aabb) {
    OcclusionGhostComponent *ci = get<OcclusionGhostComponent>(p_ghost);
    ERR_FAIL_COND(!ci);

    ci->aabb = p_aabb;
    ci->object_id = p_id;


    // noop?
    if (ci->scenario == p_scenario) {
        return;
    }

    RenderingScenarioComponent *ghost_scenario = get<RenderingScenarioComponent>(ci->scenario);

    RenderingScenarioComponent *pscenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!pscenario);
    // if the portal is in a scenario already, remove it
    if (ghost_scenario) {
        _ghost_destroy_occlusion_rep(ci);
        ci->scenario = entt::null;
    }

    // create when entering the world
    if (pscenario) {
        ci->scenario = p_scenario;

        // defer the actual creation to here
        _ghost_create_occlusion_rep(ci);
    }
}

void RoomAPI::ghost_update(RenderingEntity p_ghost, const AABB &p_aabb) {
    OcclusionGhostComponent *ci = get<OcclusionGhostComponent>(p_ghost);
    ERR_FAIL_COND(!ci);
    RenderingScenarioComponent *pscenario = get<RenderingScenarioComponent>(ci->scenario);
    ERR_FAIL_COND(!pscenario);

    ci->aabb = p_aabb;

    if (ci->rghost_handle) {
        pscenario->_portal_renderer.rghost_update(ci->rghost_handle, p_aabb);
    }
}

RenderingEntity RoomAPI::roomgroup_create() {
    RenderingEntity instance_rid = VSG::ecs->create();

    VSG::ecs->registry.emplace<RoomGroupComponent>(instance_rid);
    return instance_rid;
}

void RoomAPI::roomgroup_prepare(RenderingEntity p_roomgroup, GameEntity p_roomgroup_object_id) {
    RoomGroupComponent *roomgroup = get<RoomGroupComponent>(p_roomgroup);
    ERR_FAIL_COND(!roomgroup);
    RenderingScenarioComponent *pscenario = get<RenderingScenarioComponent>(roomgroup->scenario);
    ERR_FAIL_COND(!pscenario);
    pscenario->_portal_renderer.roomgroup_prepare(roomgroup->scenario_roomgroup_id, p_roomgroup_object_id);
}

void RoomAPI::roomgroup_set_scenario(RenderingEntity p_roomgroup, RenderingEntity p_scenario) {
    RoomGroupComponent *rg = get<RoomGroupComponent>(p_roomgroup);
    ERR_FAIL_COND(!rg);
    RenderingScenarioComponent *rg_scenario = get<RenderingScenarioComponent>(rg->scenario);
    ERR_FAIL_COND(!rg_scenario);

    // noop?
    if (rg->scenario == p_scenario) {
        return;
    }

    // if the portal is in a scenario already, remove it
    if (rg_scenario) {
        rg_scenario->_portal_renderer.roomgroup_destroy(rg->scenario_roomgroup_id);
        rg->scenario = entt::null;
        rg->scenario_roomgroup_id = 0;
    }
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);

    // create when entering the world
    if (scenario) {
        rg->scenario = p_scenario;

        // defer the actual creation to here
        rg->scenario_roomgroup_id = scenario->_portal_renderer.roomgroup_create();
    }
}
void RoomAPI::roomgroup_add_room(RenderingEntity p_roomgroup, RenderingEntity p_room) {
    RoomGroupComponent *rg = get<RoomGroupComponent>(p_roomgroup);
    ERR_FAIL_COND(!rg);
    RenderingScenarioComponent *rg_scenario = get<RenderingScenarioComponent>(rg->scenario);
    ERR_FAIL_COND(!rg_scenario);

    RoomComponent *room = get<RoomComponent>(p_room);
    ERR_FAIL_COND(!room);
    ERR_FAIL_COND(room->scenario==entt::null);

    ERR_FAIL_COND(rg->scenario != room->scenario);
    rg_scenario->_portal_renderer.roomgroup_add_room(rg->scenario_roomgroup_id, room->scenario_room_id);
}
// Occluders
RenderingEntity VisualServerScene::occluder_instance_create() {
    RenderingEntity instance_rid = VSG::ecs->create();

    VSG::ecs->registry.emplace<OccluderInstanceComponent>(instance_rid);
    return instance_rid;
}

RenderingEntity VisualServerScene::occluder_resource_create()
{
    RenderingEntity occluder_resource_rid = VSG::ecs->create();
    OccluderResourceComponent & e = VSG::ecs->registry.emplace<OccluderResourceComponent>(occluder_resource_rid);

    e.occluder_resource_id = _portal_resources.occluder_resource_create();

    return occluder_resource_rid;
}
void VisualServerScene::occluder_resource_prepare(RenderingEntity p_occluder_resource, RS::OccluderType p_type) {
    OccluderResourceComponent *ro = get<OccluderResourceComponent>(p_occluder_resource);
    ERR_FAIL_COND(!ro);
    _portal_resources.occluder_resource_prepare(ro->occluder_resource_id, (VSOccluderType)p_type);
}

void VisualServerScene::occluder_instance_link_resource(RenderingEntity p_occluder_instance, RenderingEntity p_occluder_resource) {
    OccluderInstanceComponent *oi = get<OccluderInstanceComponent>(p_occluder_instance);
    ERR_FAIL_COND(!oi);
    ERR_FAIL_COND(oi->scenario==entt::null);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(oi->scenario);

    OccluderResourceComponent *res = get<OccluderResourceComponent>(p_occluder_instance);
    ERR_FAIL_COND(!res);

    scenario->_portal_renderer.occluder_instance_link(oi->scenario_occluder_id, res->occluder_resource_id);
}

void VisualServerScene::occluder_instance_set_scenario(RenderingEntity p_occluder_instance, RenderingEntity p_scenario) {
    OccluderInstanceComponent *ro = get<OccluderInstanceComponent>(p_occluder_instance);
    ERR_FAIL_COND(!ro);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);

    // noop?
    if (ro->scenario == p_scenario) {
        return;
    }

    // if the portal is in a scenario already, remove it
    if (ro->scenario!=entt::null) {
        RenderingScenarioComponent *ro_scenario = get<RenderingScenarioComponent>(ro->scenario);
        ERR_FAIL_COND(!ro_scenario);
        ro_scenario->_portal_renderer.occluder_instance_destroy(ro->scenario_occluder_id);
        ro->scenario = entt::null;
        ro->scenario_occluder_id = 0;
    }

    // create when entering the world
    if (scenario) {
        ro->scenario = p_scenario;
        ro->scenario_occluder_id = scenario->_portal_renderer.occluder_instance_create();
    }
}
void VisualServerScene::occluder_instance_set_active(RenderingEntity p_occluder, bool p_active) {
    OccluderInstanceComponent *ro = get<OccluderInstanceComponent>(p_occluder);
    ERR_FAIL_COND(!ro);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(ro->scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.occluder_instance_set_active(ro->scenario_occluder_id, p_active);
}



void VisualServerScene::occluder_instance_set_transform(RenderingEntity p_occluder, const Transform &p_xform) {
    OccluderInstanceComponent *ro = get<OccluderInstanceComponent>(p_occluder);
    ERR_FAIL_COND(!ro);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(ro->scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.occluder_instance_set_transform(ro->scenario_occluder_id, p_xform);
}
void VisualServerScene::occluder_resource_spheres_update(RenderingEntity p_occluder, const Vector<Plane> &p_spheres) {
    OccluderResourceComponent *ro = get<OccluderResourceComponent>(p_occluder);
    ERR_FAIL_COND(!ro);
    _portal_resources.occluder_resource_update_spheres(ro->occluder_resource_id, p_spheres);
}

void VisualServerScene::occluder_resource_mesh_update(RenderingEntity p_occluder, const OccluderMeshData &p_mesh_data) {
    OccluderResourceComponent *ro = get<OccluderResourceComponent>(p_occluder);
    ERR_FAIL_COND(!ro);
    _portal_resources.occluder_resource_update_mesh(ro->occluder_resource_id, p_mesh_data);
}
void VisualServerScene::set_use_occlusion_culling(bool p_enable) {
    // this is not scenario specific, and is global
    // (mainly for debugging)
    PortalRenderer::use_occlusion_culling = p_enable;
}

Geometry::MeshData VisualServerScene::occlusion_debug_get_current_polys(RenderingEntity p_scenario) const {
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    if (!scenario) {
        return {};
    }

    return scenario->_portal_renderer.occlusion_debug_get_current_polys();
}

void VisualServerScene::callbacks_register(RenderingServerCallbacks *p_callbacks) {
    _visual_server_callbacks = p_callbacks;
}
// Rooms

// the room has to be associated with a scenario, this is assumed to be
// the same scenario as the room node
RenderingEntity RoomAPI::room_create() {
    RenderingEntity instance_rid = VSG::ecs->create();

    VSG::ecs->registry.emplace<RoomComponent>(instance_rid);
    return instance_rid;
}


// should not be called multiple times, different scenarios etc, but just in case, we will support this
void RoomAPI::room_set_scenario(RenderingEntity p_room, RenderingEntity p_scenario) {
    RoomComponent *room = get<RoomComponent>(p_room);
    ERR_FAIL_COND(!room);

    // no change?
    if (room->scenario == p_scenario) {
        return;
    }
    // if the room has an existing scenario, remove from it
    if (room->scenario!=entt::null) {
        RenderingScenarioComponent *rscenario = get<RenderingScenarioComponent>(room->scenario);
        ERR_FAIL_COND(!rscenario);
        rscenario->_portal_renderer.room_destroy(room->scenario_room_id);
        room->scenario = entt::null;
        room->scenario_room_id = 0;
    }

    // create when entering the world
    if (p_scenario!=entt::null) {
        room->scenario = p_scenario;
        RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
        ERR_FAIL_COND(!scenario);

        // defer the actual creation to here
        room->scenario_room_id = scenario->_portal_renderer.room_create();
    }
}

void RoomAPI::room_add_ghost(RenderingEntity p_room, GameEntity p_object_id, const AABB &p_aabb) {
    RoomComponent *room = get<RoomComponent>(p_room);
    ERR_FAIL_COND(!room);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(room->scenario);
    ERR_FAIL_COND(!scenario);

    scenario->_portal_renderer.room_add_ghost(room->scenario_room_id, p_object_id, p_aabb);
}
void RoomAPI::room_add_instance(RenderingEntity p_room, RenderingEntity p_instance, const AABB &p_aabb, const Vector<Vector3> &p_object_pts) {
    RoomComponent *room = get<RoomComponent>(p_room);
    ERR_FAIL_COND(!room);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(room->scenario);
    ERR_FAIL_COND(!scenario);
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    InstanceBoundsComponent *bounds = get<InstanceBoundsComponent>(p_instance);
    ERR_FAIL_COND(!bounds);
    AABB bb = p_aabb;

    // the aabb passed from the client takes no account of the extra cull margin,
    // so we need to add this manually.
    // It is assumed it is in world space.
    if (bounds->extra_margin != 0.0) {
        bb.grow_by(bounds->extra_margin);
    }

    bool dynamic = false;

    // don't add if portal mode is not static or dynamic
    switch (instance->portal_mode) {
        default: {
            return; // this should be taken care of by the calling function, but just in case
        } break;
        case RS::InstancePortalMode::INSTANCE_PORTAL_MODE_DYNAMIC: {
            dynamic = true;
        } break;
        case RS::InstancePortalMode::INSTANCE_PORTAL_MODE_STATIC: {
            dynamic = false;
        } break;
    }

    instance->occlusion_handle = scenario->_portal_renderer.room_add_instance(room->scenario_room_id, p_instance, bb, dynamic, p_object_pts);
}

void RoomAPI::room_prepare(RenderingEntity p_room, int32_t p_priority) {
    RoomComponent *room = get<RoomComponent>(p_room);
    ERR_FAIL_COND(!room);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(room->scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.room_prepare(room->scenario_room_id, p_priority);
}

void RoomAPI::room_set_bound(RenderingEntity p_room, GameEntity p_room_object_id, const Vector<Plane> &p_convex, const AABB &p_aabb, const Vector<Vector3> &p_verts) {
    RoomComponent *room = get<RoomComponent>(p_room);
    ERR_FAIL_COND(!room);
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(room->scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.room_set_bound(room->scenario_room_id, p_room_object_id, p_convex, p_aabb, p_verts);
}

void RoomAPI::rooms_unload(RenderingEntity p_scenario, String p_reason) {
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.rooms_unload(p_reason);
}

void RoomAPI::rooms_and_portals_clear(RenderingEntity p_scenario) {
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.rooms_and_portals_clear();
}
void RoomAPI::rooms_finalize(RenderingEntity p_scenario, bool p_generate_pvs, bool p_cull_using_pvs, bool p_use_secondary_pvs, bool p_use_signals, String p_pvs_filename, bool p_use_simple_pvs, bool p_log_pvs_generation) {
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.rooms_finalize(p_generate_pvs, p_cull_using_pvs, p_use_secondary_pvs, p_use_signals, p_pvs_filename, p_use_simple_pvs, p_log_pvs_generation);
}

void RoomAPI::rooms_override_camera(RenderingEntity p_scenario, bool p_override, const Vector3 &p_point, const Span<const Plane> *p_convex) {
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.rooms_override_camera(p_override, p_point, p_convex);
}

void RoomAPI::rooms_set_active(RenderingEntity p_scenario, bool p_active) {
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.rooms_set_active(p_active);
}

void RoomAPI::rooms_set_params(RenderingEntity p_scenario, int p_portal_depth_limit, real_t p_roaming_expansion_margin) {
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.rooms_set_params(p_portal_depth_limit, p_roaming_expansion_margin);
}

void RoomAPI::rooms_set_debug_feature(RenderingEntity p_scenario, RS::RoomsDebugFeature p_feature, bool p_active) {
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    switch (p_feature) {
        default: {
        } break;
        case RS::ROOMS_DEBUG_SPRAWL: {
            scenario->_portal_renderer.set_debug_sprawl(p_active);
        } break;
    }
}
void RoomAPI::rooms_update_gameplay_monitor(RenderingEntity p_scenario, const Vector<Vector3> &p_camera_positions) {
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);
    scenario->_portal_renderer.rooms_update_gameplay_monitor(p_camera_positions);
}

bool RoomAPI::rooms_is_loaded(RenderingEntity p_scenario) {
    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND_V(!scenario, false);
    return scenario->_portal_renderer.rooms_is_loaded();
}
Vector<GameEntity> VisualServerScene::instances_cull_aabb(const AABB &p_aabb, RenderingEntity p_scenario) const {

    Vector<GameEntity> instances;
    RenderingScenarioComponent *scenario = getUnchecked<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND_V(!scenario, instances);

    const_cast<VisualServerScene *>(this)->update_dirty_instances(); // check dirty instances before culling

    RenderingEntity cull[1024];
    Span<RenderingEntity,1024> cull_buffer=cull;
    int culled = scenario->sps.cull_aabb(p_aabb, cull_buffer);

    collect_culled(cull_buffer.subspan(0,culled),instances);

    return instances;
}

Vector<GameEntity> VisualServerScene::instances_cull_ray(const Vector3 &p_from, const Vector3 &p_to, RenderingEntity p_scenario) const {

    Vector<GameEntity> instances;
    RenderingScenarioComponent *scenario = getUnchecked<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND_V(!scenario, instances);
    const_cast<VisualServerScene *>(this)->update_dirty_instances(); // check dirty instances before culling

    RenderingEntity cull[1024]; //RenderingInstanceComponent

    Span<RenderingEntity,1024> cull_buffer=cull;
    int culled = scenario->sps.cull_segment(p_from, p_from + p_to * 10000, cull);

    collect_culled(cull_buffer.subspan(0,culled),instances);

    return instances;
}
Vector<GameEntity> VisualServerScene::instances_cull_convex(Span<const Plane> p_convex, RenderingEntity p_scenario) const {

    Vector<GameEntity> instances;
    RenderingScenarioComponent *scenario = getUnchecked<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND_V(!scenario, instances);
    const_cast<VisualServerScene *>(this)->update_dirty_instances(); // check dirty instances before culling

    RenderingEntity cull[1024];

    Span<RenderingEntity,1024> cull_buffer=cull;
    int culled = scenario->sps.cull_convex(p_convex, cull_buffer);
    collect_culled(cull_buffer.subspan(0,culled),instances);

    return instances;
}

void VisualServerScene::instance_geometry_set_flag(RenderingEntity p_instance, RS::InstanceFlags p_flags, bool p_enabled) {

    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );

    switch (p_flags) {

        case RS::INSTANCE_FLAG_USE_BAKED_LIGHT: {

            instance->baked_light = p_enabled;

        } break;
        case RS::INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE: {

            instance->redraw_if_visible = p_enabled;

        } break;
        default: {
        }
    }
}
void VisualServerScene::instance_geometry_set_cast_shadows_setting(RenderingEntity p_instance, RS::ShadowCastingSetting p_shadow_casting_setting) {

    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );

    instance->cast_shadows = p_shadow_casting_setting;
    instance->base_changed(false, true); // to actually compute if shadows are visible or not
}

void VisualServerScene::instance_geometry_set_material_override(RenderingEntity p_instance, RenderingEntity p_material) {
    ::instance_geometry_set_material_override(p_instance, p_material);
    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(p_instance) ||
           get<RenderingInstanceComponent>(p_instance)->self==p_instance
           );
}

void VisualServerScene::instance_geometry_set_material_overlay(RenderingEntity p_instance, RenderingEntity p_material) {
    ::instance_geometry_set_material_overlay(p_instance, p_material);
}

void VisualServerScene::instance_geometry_set_draw_range(RenderingEntity p_instance, float p_min, float p_max, float p_min_margin, float p_max_margin) {
}
void VisualServerScene::instance_geometry_set_as_instance_lod(RenderingEntity p_instance, RenderingEntity p_as_lod_of_instance) {
}

void VisualServerScene::_update_instance(RenderingInstanceComponent *p_instance) {

    p_instance->version++;

    // when not using interpolation the transform is used straight
    const Transform &instance_xform = p_instance->transform;

    // Can possibly use the most up to date current transform here when using physics interpolation ..
    // uncomment the next line for this..
    // if (p_instance->is_currently_interpolated()) {
    // instance_xform = &p_instance->transform_curr;
    // }
    // However it does seem that using the interpolated transform (transform) works for keeping AABBs
    // up to date to avoid culling errors.
    InstanceBoundsComponent& bounds = get_component<InstanceBoundsComponent>(p_instance->self);

    if (p_instance->base_type == RS::INSTANCE_LIGHT) {

        InstanceLightData *light = getUnchecked<InstanceLightData>(p_instance->self);

        VSG::scene_render->light_instance_set_transform(light->instance, instance_xform);
        light->shadow_dirty = true;
    }

    if (p_instance->base_type == RS::INSTANCE_REFLECTION_PROBE) {

        InstanceReflectionProbeData *reflection_probe = getUnchecked<InstanceReflectionProbeData>(p_instance->self);

        VSG::scene_render->reflection_probe_instance_set_transform(reflection_probe->instance, instance_xform);
        reflection_probe->reflection_dirty = true;
    }

    if (p_instance->base_type == RS::INSTANCE_PARTICLES) {

        VSG::storage->particles_set_emission_transform(p_instance->base, instance_xform);
    }

    if (bounds.aabb.has_no_surface()) {
        return;
    }

    if ((1 << p_instance->base_type) & RS::INSTANCE_GEOMETRY_MASK) {
        InstanceGeometryData *geom = get_instance_geometry(p_instance->self);
        //make sure lights are updated if it casts shadow
        auto &cm_geom(get_component<GeometryComponent>(p_instance->self));
        if (cm_geom.can_cast_shadows) {
            for (auto E : geom->lighting) {
                InstanceLightData *light = getUnchecked<InstanceLightData>(E);
                light->shadow_dirty = true;
            }
        }

        if (p_instance->lightmap_capture == entt::null && !geom->lightmap_captures.empty()) {
            //affected by lightmap captures, must update capture info!
            _update_instance_lightmap_captures(p_instance);
        } else {
            if (!p_instance->lightmap_capture_data.empty()) {
                p_instance->lightmap_capture_data.clear(); //not in use, clear capture data
            }
        }
    }

    p_instance->mirror = instance_xform.basis.determinant() < 0.0f;

    AABB new_aabb = instance_xform.xform(bounds.aabb);

    bounds.transformed_aabb = new_aabb;

    auto *scenario = get<RenderingScenarioComponent>(p_instance->scenario);
    if (!scenario) {
        return;
    }

    if (p_instance->spatial_partition_id == 0) {

        uint32_t base_type = 1 << p_instance->base_type;
        uint32_t pairable_mask = 0;
        bool pairable = false;

        if (p_instance->base_type == RS::INSTANCE_LIGHT || p_instance->base_type == RS::INSTANCE_REFLECTION_PROBE || p_instance->base_type == RS::INSTANCE_LIGHTMAP_CAPTURE) {

            pairable_mask = p_instance->visible ? RS::INSTANCE_GEOMETRY_MASK : 0;
            pairable = true;
        }

        if (p_instance->base_type == RS::INSTANCE_GI_PROBE) {
            //lights and geometries
            pairable_mask = p_instance->visible ? (RS::INSTANCE_GEOMETRY_MASK | (1 << RS::INSTANCE_LIGHT)) : 0;
            pairable = true;
        }

        // not inside octree
#ifdef TRACY_ENABLE
        VSG::bvh_nodes_created++;
#endif
        p_instance->spatial_partition_id = scenario->sps.create(p_instance->self, new_aabb, 0, pairable, base_type, pairable_mask);

    } else {

        /*
        if (new_aabb==p_instance->data.transformed_aabb)
            return;
        */

        scenario->sps.move(p_instance->spatial_partition_id, new_aabb);
    }
    // keep rooms and portals instance up to date if present
    _rooms_instance_update(p_instance, new_aabb);
}

void VisualServerScene::_update_instance_aabb(RenderingInstanceComponent *p_instance) {

    AABB new_aabb;

    ERR_FAIL_COND(p_instance->base_type != RS::INSTANCE_NONE && p_instance->base==entt::null);

    InstanceBoundsComponent& bounds = get_component<InstanceBoundsComponent>(p_instance->self);

    switch (p_instance->base_type) {
        case RS::INSTANCE_NONE: {

            // do nothing
        } break;
        case RS::INSTANCE_MESH: {

            if (bounds.use_custom_aabb)
                new_aabb = bounds.custom_aabb;
            else
                new_aabb = VSG::storage->mesh_get_aabb(p_instance->base, p_instance->skeleton);

        } break;

        case RS::INSTANCE_MULTIMESH: {

            if (bounds.use_custom_aabb)
                new_aabb = bounds.custom_aabb;
            else
                new_aabb = VSG::storage->multimesh_get_aabb(p_instance->base);

        } break;
        case RS::INSTANCE_IMMEDIATE: {

            if (bounds.use_custom_aabb)
                new_aabb = bounds.custom_aabb;
            else
                new_aabb = VSG::storage->immediate_get_aabb(p_instance->base);

        } break;
        case RS::INSTANCE_PARTICLES: {

            if (bounds.use_custom_aabb)
                new_aabb = bounds.custom_aabb;
            else
                new_aabb = VSG::storage->particles_get_aabb(p_instance->base);

        } break;
        case RS::INSTANCE_LIGHT: {

            new_aabb = VSG::storage->light_get_aabb(p_instance->base);

        } break;
        case RS::INSTANCE_REFLECTION_PROBE: {

            new_aabb = VSG::storage->reflection_probe_get_aabb(p_instance->base);

        } break;
        case RS::INSTANCE_GI_PROBE: {

            new_aabb = VSG::storage->gi_probe_get_bounds(p_instance->base);

        } break;
        case RS::INSTANCE_LIGHTMAP_CAPTURE: {

            new_aabb = VSG::storage->lightmap_capture_get_bounds(p_instance->base);

        } break;
        default: {
        }
    }

    // <Zylann> This is why I didn't re-use Instance::aabb to implement custom AABBs
    if (bounds.extra_margin)
        new_aabb.grow_by(bounds.extra_margin);

    bounds.aabb = new_aabb;
}

_FORCE_INLINE_ static void _light_capture_sample_octree(const RasterizerStorage::LightmapCaptureOctree *p_octree, int p_cell_subdiv, const Vector3 &p_pos, const Vector3 &p_dir, float p_level, Vector3 &r_color, float &r_alpha) {

    static const Vector3 aniso_normal[6] = {
        Vector3(-1, 0, 0),
        Vector3(1, 0, 0),
        Vector3(0, -1, 0),
        Vector3(0, 1, 0),
        Vector3(0, 0, -1),
        Vector3(0, 0, 1)
    };

    int size = 1 << (p_cell_subdiv - 1);

    int clamp_v = size - 1;
    //first of all, clamp
    Vector3 pos;
    pos.x = CLAMP<float>(p_pos.x, 0, clamp_v);
    pos.y = CLAMP<float>(p_pos.y, 0, clamp_v);
    pos.z = CLAMP<float>(p_pos.z, 0, clamp_v);

    float level = (p_cell_subdiv - 1) - p_level;

    int target_level;
    float level_filter;
    if (level <= 0.0f) {
        level_filter = 0;
        target_level = 0;
    } else {
        target_level = Math::ceil(level);
        level_filter = target_level - level;
    }

    Vector3 color[2][8];
    float alpha[2][8];
    memset(alpha, 0, sizeof(float) * 2 * 8);

    //find cell at given level first

    for (int c = 0; c < 2; c++) {

        int current_level = M_MAX(0, target_level - c);
        int level_cell_size = (1 << (p_cell_subdiv - 1)) >> current_level;

        for (int n = 0; n < 8; n++) {

            int x = int(pos.x);
            int y = int(pos.y);
            int z = int(pos.z);

            if (n & 1)
                x += level_cell_size;
            if (n & 2)
                y += level_cell_size;
            if (n & 4)
                z += level_cell_size;

            int ofs_x = 0;
            int ofs_y = 0;
            int ofs_z = 0;

            x = CLAMP(x, 0, clamp_v);
            y = CLAMP(y, 0, clamp_v);
            z = CLAMP(z, 0, clamp_v);

            int half = size / 2;
            uint32_t cell = 0;
            for (int i = 0; i < current_level; i++) {

                const RasterizerStorage::LightmapCaptureOctree *bc = &p_octree[cell];

                int child = 0;
                if (x >= ofs_x + half) {
                    child |= 1;
                    ofs_x += half;
                }
                if (y >= ofs_y + half) {
                    child |= 2;
                    ofs_y += half;
                }
                if (z >= ofs_z + half) {
                    child |= 4;
                    ofs_z += half;
                }

                cell = bc->children[child];
                if (cell == RasterizerStorage::LightmapCaptureOctree::CHILD_EMPTY)
                    break;

                half >>= 1;
            }

            if (cell == RasterizerStorage::LightmapCaptureOctree::CHILD_EMPTY) {
                alpha[c][n] = 0;
            } else {
                alpha[c][n] = p_octree[cell].alpha;

                for (int i = 0; i < 6; i++) {
                    //anisotropic read light
                    float amount = p_dir.dot(aniso_normal[i]);
                    if (amount < 0)
                        amount = 0;
                    color[c][n].x += p_octree[cell].light[i][0] / 1024.0f * amount;
                    color[c][n].y += p_octree[cell].light[i][1] / 1024.0f * amount;
                    color[c][n].z += p_octree[cell].light[i][2] / 1024.0f * amount;
                }
            }

            //print_line("\tlev " + itos(c) + " - " + itos(n) + " alpha: " + rtos(cells[test_cell].alpha) + " col: " + color[c][n]);
        }
    }

    float target_level_size = size >> target_level;
    Vector3 pos_fract[2];

    pos_fract[0].x = Math::fmod(pos.x, target_level_size) / target_level_size;
    pos_fract[0].y = Math::fmod(pos.y, target_level_size) / target_level_size;
    pos_fract[0].z = Math::fmod(pos.z, target_level_size) / target_level_size;

    target_level_size = size >> M_MAX(0, target_level - 1);

    pos_fract[1].x = Math::fmod(pos.x, target_level_size) / target_level_size;
    pos_fract[1].y = Math::fmod(pos.y, target_level_size) / target_level_size;
    pos_fract[1].z = Math::fmod(pos.z, target_level_size) / target_level_size;

    float alpha_interp[2];
    Vector3 color_interp[2];

    for (int i = 0; i < 2; i++) {

        Vector3 color_x00 = color[i][0].linear_interpolate(color[i][1], pos_fract[i].x);
        Vector3 color_xy0 = color[i][2].linear_interpolate(color[i][3], pos_fract[i].x);
        Vector3 blend_z0 = color_x00.linear_interpolate(color_xy0, pos_fract[i].y);

        Vector3 color_x0z = color[i][4].linear_interpolate(color[i][5], pos_fract[i].x);
        Vector3 color_xyz = color[i][6].linear_interpolate(color[i][7], pos_fract[i].x);
        Vector3 blend_z1 = color_x0z.linear_interpolate(color_xyz, pos_fract[i].y);

        color_interp[i] = blend_z0.linear_interpolate(blend_z1, pos_fract[i].z);

        float alpha_x00 = Math::lerp(alpha[i][0], alpha[i][1], pos_fract[i].x);
        float alpha_xy0 = Math::lerp(alpha[i][2], alpha[i][3], pos_fract[i].x);
        float alpha_z0 = Math::lerp(alpha_x00, alpha_xy0, pos_fract[i].y);

        float alpha_x0z = Math::lerp(alpha[i][4], alpha[i][5], pos_fract[i].x);
        float alpha_xyz = Math::lerp(alpha[i][6], alpha[i][7], pos_fract[i].x);
        float alpha_z1 = Math::lerp(alpha_x0z, alpha_xyz, pos_fract[i].y);

        alpha_interp[i] = Math::lerp(alpha_z0, alpha_z1, pos_fract[i].z);
    }

    r_color = color_interp[0].linear_interpolate(color_interp[1], level_filter);
    r_alpha = Math::lerp(alpha_interp[0], alpha_interp[1], level_filter);

    //print_line("pos: " + p_posf + " level " + rtos(p_level) + " down to " + itos(target_level) + "." + rtos(level_filter) + " color " + r_color + " alpha " + rtos(r_alpha));
}

_FORCE_INLINE_ static Color _light_capture_voxel_cone_trace(const RasterizerStorage::LightmapCaptureOctree *p_octree, const Vector3 &p_pos, const Vector3 &p_dir, float p_aperture, int p_cell_subdiv) {

    float bias = 0.0; //no need for bias here
    float max_distance = (Vector3(1, 1, 1) * (1 << (p_cell_subdiv - 1))).length();

    float dist = bias;
    float alpha = 0.0;
    Vector3 color;

    Vector3 scolor;
    float salpha;

    while (dist < max_distance && alpha < 0.95f) {
        float diameter = M_MAX(1.0, 2.0f * p_aperture * dist);
        _light_capture_sample_octree(p_octree, p_cell_subdiv, p_pos + dist * p_dir, p_dir, log2(diameter), scolor, salpha);
        float a = (1.0f - alpha);
        color += scolor * a;
        alpha += a * salpha;
        dist += diameter * 0.5f;
    }

    return Color(color.x, color.y, color.z, alpha);
}

void VisualServerScene::_update_instance_lightmap_captures(RenderingInstanceComponent *p_instance) {

    InstanceGeometryData *geom = get_instance_geometry(p_instance->self);

    static const Vector3 cone_traces[12] = {
        Vector3(0, 0, 1),
        Vector3(0.866025f, 0, 0.5f),
        Vector3(0.267617f, 0.823639f, 0.5f),
        Vector3(-0.700629f, 0.509037f, 0.5f),
        Vector3(-0.700629f, -0.509037f, 0.5f),
        Vector3(0.267617f, -0.823639f, 0.5f),
        Vector3(0, 0, -1),
        Vector3(0.866025f, 0, -0.5f),
        Vector3(0.267617f, 0.823639f, -0.5f),
        Vector3(-0.700629f, 0.509037f, -0.5f),
        Vector3(-0.700629f, -0.509037f, -0.5f),
        Vector3(0.267617f, -0.823639f, -0.5f)
    };

    float cone_aperture = 0.577f; // tan(angle) 60 degrees

    if (p_instance->lightmap_capture_data.empty()) {
        p_instance->lightmap_capture_data.resize(12);
    }

    //print_line("update captures for pos: " + p_instance->transform.origin);

    for (int i = 0; i < 12; i++)
        new (&p_instance->lightmap_capture_data.data()[i]) Color;

    //this could use some sort of blending..
    for (RenderingEntity E : geom->lightmap_captures) {
        auto *inst = getUnchecked<RenderingInstanceComponent>(E);
        const PoolVector<RasterizerStorage::LightmapCaptureOctree> *octree = VSG::storage->lightmap_capture_get_octree_ptr(inst->base);
        //print_line("octree size: " + itos(octree->size()));
        if (octree->size() == 0)
            continue;
        Transform to_cell_xform = VSG::storage->lightmap_capture_get_octree_cell_transform(inst->base);
        int cell_subdiv = VSG::storage->lightmap_capture_get_octree_cell_subdiv(inst->base);
        to_cell_xform = to_cell_xform * inst->transform.affine_inverse();

        PoolVector<RasterizerStorage::LightmapCaptureOctree>::Read octree_r = octree->read();

        Vector3 pos = to_cell_xform.xform(p_instance->transform.origin);

        const float capture_energy = VSG::storage->lightmap_capture_get_energy(inst->base);

        for (int i = 0; i < 12; i++) {

            Vector3 dir = to_cell_xform.basis.xform(cone_traces[i]).normalized();
            Color capture = _light_capture_voxel_cone_trace(octree_r.ptr(), pos, dir, cone_aperture, cell_subdiv);
            capture.r *= capture_energy;
            capture.g *= capture_energy;
            capture.b *= capture_energy;
            p_instance->lightmap_capture_data[i] += capture;
        }
    }
}

bool VisualServerScene::_light_instance_update_shadow(RenderingInstanceComponent *p_instance, const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_orthogonal, RenderingEntity p_shadow_atlas, RenderingScenarioComponent *p_scenario) {

    InstanceLightData *light = getUnchecked<InstanceLightData>(p_instance->self);

    Transform light_transform = p_instance->transform;
    light_transform.orthonormalize(); //scale does not count on lights

    bool animated_material_found = false;

    auto instance_view(VSG::ecs->registry.view<RenderingInstanceComponent>());
    switch (VSG::storage->light_get_type(p_instance->base)) {

        case RS::LIGHT_DIRECTIONAL: {

            float max_distance = p_cam_projection.get_z_far();
            float shadow_max = VSG::storage->light_get_param(p_instance->base, RS::LIGHT_PARAM_SHADOW_MAX_DISTANCE);
            if (shadow_max > 0 && !p_cam_orthogonal) { //its impractical (and leads to unwanted behaviors) to set max distance in orthogonal camera
                max_distance = MIN(shadow_max, max_distance);
            }
            max_distance = M_MAX(max_distance, p_cam_projection.get_z_near() + 0.001f);
            float min_distance = MIN(p_cam_projection.get_z_near(), max_distance);

            RS::LightDirectionalShadowDepthRangeMode depth_range_mode = VSG::storage->light_directional_get_shadow_depth_range_mode(p_instance->base);

            if (depth_range_mode == RS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_OPTIMIZED) {
                //optimize min/max
                Frustum planes = p_cam_projection.get_projection_planes(p_cam_transform);
                int cull_count = p_scenario->sps.cull_convex(planes, instance_shadow_cull_result, RS::INSTANCE_GEOMETRY_MASK);
                int room_hint=0;//light->previous_room_id_hint
                Plane base(p_cam_transform.origin, -p_cam_transform.basis.get_axis(2));
                //check distance max and min

                bool found_items = false;
                float z_max = -1e20f;
                float z_min = 1e20f;

                for (int i = 0; i < cull_count; i++) {
                    auto &instance = instance_view.get<RenderingInstanceComponent>(instance_shadow_cull_result[i]);
                    assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(instance_shadow_cull_result[i]) ||
                           get<RenderingInstanceComponent>(instance_shadow_cull_result[i])->self==instance_shadow_cull_result[i]
                           );
                    if (!instance.visible || !(has_component<GeometryComponent>(instance.self)) ) {
                        continue;
                    }

                    auto &cm_geom(get_component<GeometryComponent>(instance.self));
                    if(!cm_geom.can_cast_shadows)
                        continue;

                    if (cm_geom.material_is_animated) {
                        animated_material_found = true;
                    }

                    float max, min;
                    get_component<InstanceBoundsComponent>(instance.self).transformed_aabb.project_range_in_plane(base, min, max);

                    if (max > z_max) {
                        z_max = max;
                    }

                    if (min < z_min) {
                        z_min = min;
                    }

                    found_items = true;
                }

                if (found_items) {
                    min_distance = M_MAX(min_distance, z_min);
                    max_distance = MIN(max_distance, z_max);
                }
            }

            float range = max_distance - min_distance;

            int splits = 0;
            switch (VSG::storage->light_directional_get_shadow_mode(p_instance->base)) {
                case RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL: splits = 1; break;
                case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS: splits = 2; break;
                case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS: splits = 4; break;
            }

            float distances[5];

            distances[0] = min_distance;
            for (int i = 0; i < splits; i++) {
                distances[i + 1] = min_distance + VSG::storage->light_get_param(p_instance->base, RS::LightParam(RS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET + i)) * range;
            }

            distances[splits] = max_distance;

            float texture_size = VSG::scene_render->get_directional_light_shadow_size(light->instance);

            bool overlap = VSG::storage->light_directional_get_blend_splits(p_instance->base);

            float first_radius = 0.0;

            for (int i = 0; i < splits; i++) {

                // setup a camera matrix for that range!
                CameraMatrix camera_matrix;

                float aspect = p_cam_projection.get_aspect();

                if (p_cam_orthogonal) {

                    Vector2 vp_he = p_cam_projection.get_viewport_half_extents();

                    camera_matrix.set_orthogonal(vp_he.y * 2.0f, aspect, distances[(i == 0 || !overlap) ? i : i - 1], distances[i + 1], false);
                } else {

                    float fov = p_cam_projection.get_fov();
                    camera_matrix.set_perspective(fov, aspect, distances[(i == 0 || !overlap) ? i : i - 1], distances[i + 1], false);
                }

                //obtain the frustum endpoints

                Vector3 endpoints[8]; // frustum plane endpoints
                bool res = camera_matrix.get_endpoints(p_cam_transform, endpoints);
                ERR_CONTINUE(!res);

                // obtain the light frustm ranges (given endpoints)

                Transform transform = light_transform; //discard scale and stabilize light

                Vector3 x_vec = transform.basis.get_axis(Vector3::AXIS_X).normalized();
                Vector3 y_vec = transform.basis.get_axis(Vector3::AXIS_Y).normalized();
                Vector3 z_vec = transform.basis.get_axis(Vector3::AXIS_Z).normalized();
                //z_vec points agsint the camera, like in default opengl

                float x_min = 0.f, x_max = 0.f;
                float y_min = 0.f, y_max = 0.f;
                float z_min = 0.f, z_max = 0.f;

                // FIXME: z_max_cam is defined, computed, but not used below when setting up
                // ortho_camera. Commented out for now to fix warnings but should be investigated.
                float x_min_cam = 0.f, x_max_cam = 0.f;
                float y_min_cam = 0.f, y_max_cam = 0.f;
                float z_min_cam = 0.f;
                //float z_max_cam = 0.f;

                float bias_scale = 1.0;

                //used for culling

                for (int j = 0; j < 8; j++) {

                    float d_x = x_vec.dot(endpoints[j]);
                    float d_y = y_vec.dot(endpoints[j]);
                    float d_z = z_vec.dot(endpoints[j]);

                    if (j == 0 || d_x < x_min)
                        x_min = d_x;
                    if (j == 0 || d_x > x_max)
                        x_max = d_x;

                    if (j == 0 || d_y < y_min)
                        y_min = d_y;
                    if (j == 0 || d_y > y_max)
                        y_max = d_y;

                    if (j == 0 || d_z < z_min)
                        z_min = d_z;
                    if (j == 0 || d_z > z_max)
                        z_max = d_z;
                }

                {
                    //camera viewport stuff

                    Vector3 center;

                    for (int j = 0; j < 8; j++) {

                        center += endpoints[j];
                    }
                    center /= 8.0;

                    //center=x_vec*(x_max-x_min)*0.5 + y_vec*(y_max-y_min)*0.5 + z_vec*(z_max-z_min)*0.5;

                    float radius = 0;

                    for (int j = 0; j < 8; j++) {

                        float d = center.distance_to(endpoints[j]);
                        if (d > radius)
                            radius = d;
                    }

                    radius *= texture_size / (texture_size - 2.0f); //add a texel by each side

                    if (i == 0) {
                        first_radius = radius;
                    } else {
                        bias_scale = radius / first_radius;
                    }

                    x_max_cam = x_vec.dot(center) + radius;
                    x_min_cam = x_vec.dot(center) - radius;
                    y_max_cam = y_vec.dot(center) + radius;
                    y_min_cam = y_vec.dot(center) - radius;
                    //z_max_cam = z_vec.dot(center) + radius;
                    z_min_cam = z_vec.dot(center) - radius;

                    if (depth_range_mode == RS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE) {
                        //this trick here is what stabilizes the shadow (make potential jaggies to not move)
                        //at the cost of some wasted resolution. Still the quality increase is very well worth it

                        float unit = radius * 2.0f / texture_size;

                        x_max_cam = Math::stepify(x_max_cam, unit);
                        x_min_cam = Math::stepify(x_min_cam, unit);
                        y_max_cam = Math::stepify(y_max_cam, unit);
                        y_min_cam = Math::stepify(y_min_cam, unit);
                    }
                }

                //now that we now all ranges, we can proceed to make the light frustum planes, for culling octree

                Frustum light_frustum_planes;

                //right/left
                light_frustum_planes[0] = Plane(x_vec, x_max);
                light_frustum_planes[1] = Plane(-x_vec, -x_min);
                //top/bottom
                light_frustum_planes[2] = Plane(y_vec, y_max);
                light_frustum_planes[3] = Plane(-y_vec, -y_min);
                //near/far
                light_frustum_planes[4] = Plane(z_vec, z_max + 1e6f);
                light_frustum_planes[5] = Plane(-z_vec, -z_min); // z_min is ok, since casters further than far-light plane are not needed

                int cull_count = p_scenario->sps.cull_convex(light_frustum_planes, instance_shadow_cull_result, RS::INSTANCE_GEOMETRY_MASK);

                // a pre pass will need to be needed to determine the actual z-near to be used

                Plane near_plane(light_transform.origin, -light_transform.basis.get_axis(2));

                for (int j = 0; j < cull_count; j++) {

                    float min, max;
                    auto &instance = instance_view.get<RenderingInstanceComponent>(instance_shadow_cull_result[j]);

                    if (!instance.visible || !has_component<GeometryComponent>(instance.self) ||
                            !get_component<GeometryComponent>(instance.self).can_cast_shadows) {
                        cull_count--;
                        SWAP(instance_shadow_cull_result[j], instance_shadow_cull_result[cull_count]);
                        j--;
                        continue;
                    }

                    get_component<InstanceBoundsComponent>(instance.self).transformed_aabb.project_range_in_plane(Plane(z_vec, 0), min, max);
                    instance.depth = near_plane.distance_to(instance.transform.origin);
                    instance.depth_layer = 0;
                    if (max > z_max)
                        z_max = max;
                }

                {

                    CameraMatrix ortho_camera;
                    real_t half_x = (x_max_cam - x_min_cam) * 0.5f;
                    real_t half_y = (y_max_cam - y_min_cam) * 0.5f;

                    ortho_camera.set_orthogonal(-half_x, half_x, -half_y, half_y, 0, (z_max - z_min_cam));

                    Transform ortho_transform;
                    ortho_transform.basis = transform.basis;
                    ortho_transform.origin = x_vec * (x_min_cam + half_x) + y_vec * (y_min_cam + half_y) + z_vec * z_max;

                    VSG::scene_render->light_instance_set_shadow_transform(light->instance, ortho_camera, ortho_transform, 0, distances[i + 1], i, bias_scale);
                }

                VSG::scene_render->render_shadow(light->instance, p_shadow_atlas, i, Span<RenderingEntity>(instance_shadow_cull_result, cull_count));
            }

        } break;
        case RS::LIGHT_OMNI: {

            RS::LightOmniShadowMode shadow_mode = VSG::storage->light_omni_get_shadow_mode(p_instance->base);

            if (shadow_mode == RS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID || !VSG::scene_render->light_instances_can_render_shadow_cube()) {

                for (int i = 0; i < 2; i++) {

                    //using this one ensures that raster deferred will have it

                    float radius = VSG::storage->light_get_param(p_instance->base, RS::LIGHT_PARAM_RANGE);

                    float z = i == 0 ? -1 : 1;
                    Plane planes[6] = {
                        light_transform.xform(Plane(Vector3(0, 0, z), radius)),
                        light_transform.xform(Plane(Vector3(1, 0, z).normalized(), radius)),
                        light_transform.xform(Plane(Vector3(-1, 0, z).normalized(), radius)),
                        light_transform.xform(Plane(Vector3(0, 1, z).normalized(), radius)),
                        light_transform.xform(Plane(Vector3(0, -1, z).normalized(), radius)),
                        light_transform.xform(Plane(Vector3(0, 0, -z).normalized(), radius)),
                    };

                    int cull_count = p_scenario->sps.cull_convex(planes, instance_shadow_cull_result, RS::INSTANCE_GEOMETRY_MASK);
                    Plane near_plane(light_transform.origin, light_transform.basis.get_axis(2) * z);

                    for (int j = 0; j < cull_count; j++) {

                        auto *instance = getUnchecked<RenderingInstanceComponent>(instance_shadow_cull_result[j]);
                        if (!instance->visible || !has_component<GeometryComponent>(instance->self) ||
                                !get_component<GeometryComponent>(instance->self).can_cast_shadows) {
                            cull_count--;
                            SWAP(instance_shadow_cull_result[j], instance_shadow_cull_result[cull_count]);
                            j--;
                        } else {
                            if (get_component<GeometryComponent>(instance->self).material_is_animated) {
                                animated_material_found = true;
                            }

                            instance->depth = near_plane.distance_to(instance->transform.origin);
                            instance->depth_layer = 0;
                        }
                    }

                    VSG::scene_render->light_instance_set_shadow_transform(light->instance, CameraMatrix(), light_transform, radius, 0, i);
                    VSG::scene_render->render_shadow(light->instance, p_shadow_atlas, i, Span<RenderingEntity>(instance_shadow_cull_result, cull_count));
                }
            } else { //shadow cube

                float radius = VSG::storage->light_get_param(p_instance->base, RS::LIGHT_PARAM_RANGE);
                CameraMatrix cm;
                cm.set_perspective(90, 1, 0.01f, radius);

                for (int i = 0; i < 6; i++) {

                    //using this one ensures that raster deferred will have it

                    static constexpr const Vector3 view_normals[6] = {
                        Vector3(-1, 0, 0),
                        Vector3(+1, 0, 0),
                        Vector3(0, -1, 0),
                        Vector3(0, +1, 0),
                        Vector3(0, 0, -1),
                        Vector3(0, 0, +1)
                    };
                    static constexpr const Vector3 view_up[6] = {
                        Vector3(0, -1, 0),
                        Vector3(0, -1, 0),
                        Vector3(0, 0, -1),
                        Vector3(0, 0, +1),
                        Vector3(0, -1, 0),
                        Vector3(0, -1, 0)
                    };

                    Transform xform = light_transform * Transform().looking_at(view_normals[i], view_up[i]);

                    Frustum planes = cm.get_projection_planes(xform);

                    int cull_count = _cull_convex_from_point(p_scenario, light_transform, cm, planes,
                            instance_shadow_cull_result, light->previous_room_id_hint,
                            RS::INSTANCE_GEOMETRY_MASK);

                    Plane near_plane(xform.origin, -xform.basis.get_axis(2));
                    for (int j = 0; j < cull_count; j++) {

                        auto &instance = instance_view.get<RenderingInstanceComponent>(instance_shadow_cull_result[j]);
                        if (!instance.visible || !has_component<GeometryComponent>(instance.self) ||
                                !get_component<GeometryComponent>(instance.self).can_cast_shadows) {
                            cull_count--;
                            SWAP(instance_shadow_cull_result[j], instance_shadow_cull_result[cull_count]);
                            j--;
                        } else {
                            if (get_component<GeometryComponent>(instance.self).material_is_animated) {
                                animated_material_found = true;
                            }
                            instance.depth = near_plane.distance_to(instance.transform.origin);
                            instance.depth_layer = 0;
                        }
                    }

                    VSG::scene_render->light_instance_set_shadow_transform(light->instance, cm, xform, radius, 0, i);
                    VSG::scene_render->render_shadow(light->instance, p_shadow_atlas, i, Span<RenderingEntity>(instance_shadow_cull_result, cull_count));
                }

                //restore the regular DP matrix
                VSG::scene_render->light_instance_set_shadow_transform(light->instance, CameraMatrix(), light_transform, radius, 0, 0);
            }

        } break;
        case RS::LIGHT_SPOT: {

            float radius = VSG::storage->light_get_param(p_instance->base, RS::LIGHT_PARAM_RANGE);
            float angle = VSG::storage->light_get_param(p_instance->base, RS::LIGHT_PARAM_SPOT_ANGLE);

            CameraMatrix cm;
            cm.set_perspective(angle * 2.0f, 1.0, 0.01f, radius);

            Frustum planes = cm.get_projection_planes(light_transform);
            int room_hint = 0; // light->previous_room_id_hint;
            int cull_count = _cull_convex_from_point(p_scenario, light_transform, cm, planes,
                    instance_shadow_cull_result, room_hint, RS::INSTANCE_GEOMETRY_MASK);
            Plane near_plane(light_transform.origin, -light_transform.basis.get_axis(2));
            for (int j = 0; j < cull_count; j++) {

                RenderingInstanceComponent *instance = getUnchecked<RenderingInstanceComponent>(instance_shadow_cull_result[j]);
                if (!instance->visible || !has_component<GeometryComponent>(instance->self) ||
                        !get_component<GeometryComponent>(instance->self).can_cast_shadows) {
                    cull_count--;
                    SWAP(instance_shadow_cull_result[j], instance_shadow_cull_result[cull_count]);
                    j--;
                } else {
                    if (get_component<GeometryComponent>(instance->self).material_is_animated) {
                        animated_material_found = true;
                    }
                    instance->depth = near_plane.distance_to(instance->transform.origin);
                    instance->depth_layer = 0;
                }
            }

            VSG::scene_render->light_instance_set_shadow_transform(light->instance, cm, light_transform, radius, 0, 0);
            VSG::scene_render->render_shadow(light->instance, p_shadow_atlas, 0, Span<RenderingEntity>(instance_shadow_cull_result, cull_count));

        } break;
    }

    return animated_material_found;
}

void VisualServerScene::render_camera(RenderingEntity p_camera, RenderingEntity p_scenario, Size2 p_viewport_size, RenderingEntity p_shadow_atlas) {
// render to mono camera
#ifndef _3D_DISABLED
    Camera3DComponent *camera = get<Camera3DComponent>(p_camera);

    ERR_FAIL_COND(!camera);

    /* STEP 1 - SETUP CAMERA */
    CameraMatrix camera_matrix;
    bool ortho = false;

    switch (camera->type) {
        case Camera3DComponent::ORTHOGONAL: {

            camera_matrix.set_orthogonal(
                    camera->size,
                    p_viewport_size.width / (float)p_viewport_size.height,
                    camera->znear,
                    camera->zfar,
                    camera->vaspect);
            ortho = true;
        } break;
        case Camera3DComponent::PERSPECTIVE: {

            camera_matrix.set_perspective(
                    camera->fov,
                    p_viewport_size.width / (float)p_viewport_size.height,
                    camera->znear,
                    camera->zfar,
                    camera->vaspect);
            ortho = false;

        } break;
        case Camera3DComponent::FRUSTUM: {

            camera_matrix.set_frustum(
                    camera->size,
                    p_viewport_size.width / (float)p_viewport_size.height,
                    camera->offset,
                    camera->znear,
                    camera->zfar,
                    camera->vaspect);
            ortho = false;
        } break;
    }
    Transform camera_transform = camera->transform;

    _prepare_scene(camera_transform, camera_matrix, ortho, camera->env, camera->visible_layers, p_scenario,
            p_shadow_atlas, entt::null, camera->previous_room_id_hint);
    _render_scene(camera_transform, camera_matrix, 0, ortho, camera->env, p_scenario, p_shadow_atlas, entt::null, -1);
#endif
}

void VisualServerScene::render_camera(Ref<ARVRInterface> &p_interface, ARVREyes p_eye, RenderingEntity p_camera, RenderingEntity p_scenario, Size2 p_viewport_size, RenderingEntity p_shadow_atlas) {
    // render for AR/VR interface

    Camera3DComponent *camera = get<Camera3DComponent>(p_camera); //camera_owner.getornull(p_camera);
    ERR_FAIL_COND(!camera);

    /* SETUP CAMERA, we are ignoring type and FOV here */
    float aspect = p_viewport_size.width / (float)p_viewport_size.height;
    CameraMatrix camera_matrix = p_interface->get_projection_for_eye(p_eye, aspect, camera->znear, camera->zfar);

    // We also ignore our camera position, it will have been positioned with a slightly old tracking position.
    // Instead we take our origin point and have our ar/vr interface add fresh tracking data! Whoohoo!
    Transform world_origin = ARVRServer::get_singleton()->get_world_origin();
    Transform cam_transform = p_interface->get_transform_for_eye(p_eye, world_origin);

    // For stereo render we only prepare for our left eye and then reuse the outcome for our right eye
    if (p_eye == ARVREyes::EYE_LEFT) {
        ///@TODO possibly move responsibility for this into our ARVRServer or ARVRInterface?

        // Center our transform, we assume basis is equal.
        Transform mono_transform = cam_transform;
        Transform right_transform = p_interface->get_transform_for_eye(ARVREyes::EYE_RIGHT, world_origin);
        mono_transform.origin += right_transform.origin;
        mono_transform.origin *= 0.5;

        // We need to combine our projection frustums for culling.
        // Ideally we should use our clipping planes for this and combine them,
        // however our shadow map logic uses our projection matrix.
        // Note: as our left and right frustums should be mirrored, we don't need our right projection matrix.

        // - get some base values we need
        float eye_dist = (mono_transform.origin - cam_transform.origin).length();
        float z_near = camera_matrix.get_z_near(); // get our near plane
        float z_far = camera_matrix.get_z_far(); // get our far plane
        float width = (2.0f * z_near) / camera_matrix.matrix[0][0];
        float x_shift = width * camera_matrix.matrix[2][0];
        float height = (2.0f * z_near) / camera_matrix.matrix[1][1];
        float y_shift = height * camera_matrix.matrix[2][1];

        // printf("Eye_dist = %f, Near = %f, Far = %f, Width = %f, Shift = %f\n", eye_dist, z_near, z_far, width, x_shift);

        // - calculate our near plane size (horizontal only, right_near is mirrored)
        float left_near = -eye_dist - ((width - x_shift) * 0.5f);

        // - calculate our far plane size (horizontal only, right_far is mirrored)
        float left_far = -eye_dist - (z_far * (width - x_shift) * 0.5f / z_near);
        float left_far_right_eye = eye_dist - (z_far * (width + x_shift) * 0.5f / z_near);
        if (left_far > left_far_right_eye) {
            // on displays smaller then double our iod, the right eye far frustrum can overtake the left eyes.
            left_far = left_far_right_eye;
        }

        // - figure out required z-shift
        float slope = (left_far - left_near) / (z_far - z_near);
        float z_shift = (left_near / slope) - z_near;

        // - figure out new vertical near plane size (this will be slightly oversized thanks to our z-shift)
        float top_near = (height - y_shift) * 0.5f;
        top_near += (top_near / z_near) * z_shift;
        float bottom_near = -(height + y_shift) * 0.5f;
        bottom_near += (bottom_near / z_near) * z_shift;

        // printf("Left_near = %f, Left_far = %f, Top_near = %f, Bottom_near = %f, Z_shift = %f\n", left_near, left_far, top_near, bottom_near, z_shift);

        // - generate our frustum
        CameraMatrix combined_matrix;
        combined_matrix.set_frustum(left_near, -left_near, bottom_near, top_near, z_near + z_shift, z_far + z_shift);

        // and finally move our camera back
        Transform apply_z_shift;
        apply_z_shift.origin = Vector3(0.0, 0.0, z_shift); // z negative is forward so this moves it backwards
        mono_transform *= apply_z_shift;

        // now prepare our scene with our adjusted transform projection matrix
        _prepare_scene(mono_transform, combined_matrix, false, camera->env, camera->visible_layers, p_scenario, p_shadow_atlas, entt::null,camera->previous_room_id_hint);
    } else if (p_eye == ARVREyes::EYE_MONO) {
        // For mono render, prepare as per usual
        _prepare_scene(cam_transform, camera_matrix, false, camera->env, camera->visible_layers, p_scenario, p_shadow_atlas, entt::null,camera->previous_room_id_hint);
    }

    // And render our scene...
    _render_scene(cam_transform, camera_matrix, p_eye, false, camera->env, p_scenario, p_shadow_atlas, entt::null, -1);
}

void VisualServerScene::_prepare_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection,
        bool p_cam_orthogonal, RenderingEntity p_force_environment, uint32_t p_visible_layers,
        RenderingEntity p_scenario, RenderingEntity p_shadow_atlas, RenderingEntity p_reflection_probe, int32_t &r_previous_room_id_hint) {
    SCOPE_AUTONAMED

    // Note, in stereo rendering:
    // - p_cam_transform will be a transform in the middle of our two eyes
    // - p_cam_projection is a wider frustrum that encompasses both eyes

    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);

    render_pass++;
    uint32_t camera_layer_mask = p_visible_layers;

    VSG::scene_render->set_scene_pass(render_pass);

    //rasterizer->set_camera(camera->transform, camera_matrix,ortho);

    Frustum planes = p_cam_projection.get_projection_planes(p_cam_transform);

    Plane near_plane(p_cam_transform.origin, -p_cam_transform.basis.get_axis(2).normalized());
    float z_far = p_cam_projection.get_z_far();

    update_dirty_instances();
    /* STEP 2 - CULL */
    {
        SCOPE_PROFILE("InstanceCull");
        int room_hint = r_previous_room_id_hint;
        instance_cull_count = _cull_convex_from_point(scenario, p_cam_transform, p_cam_projection, planes,
                instance_cull_result, room_hint);
    }
    light_cull_count = 0;

    reflection_probe_cull_count = 0;

    //light_samplers_culled=0;

    /*
    print_line("OT: "+rtos( (OS::get_singleton()->get_ticks_usec()-t)/1000.0));
    print_line("OTO: "+itos(p_scenario->octree.get_octant_count()));
    print_line("OTE: "+itos(p_scenario->octree.get_elem_count()));
    print_line("OTP: "+itos(p_scenario->octree.get_pair_count()));
    */

    /* STEP 3 - PROCESS PORTALS, VALIDATE ROOMS */
    //removed, will replace with culling

    /* STEP 4 - REMOVE FURTHER CULLED OBJECTS, ADD LIGHTS */

    auto inst_view(VSG::ecs->registry.view<RenderingInstanceComponent>());
    int invalid_entities_in_sps=0;
    for (int i = 0; i < instance_cull_count; i++) {
        assert(VSG::ecs->registry.valid(instance_cull_result[i]));
        if(!VSG::ecs->registry.valid(instance_cull_result[i])) {
            // swap and pop, to remove the invalid entity
            eastl::swap(instance_cull_result[i],instance_cull_result[instance_cull_count-1]);
            instance_cull_count--;
            i--;
            invalid_entities_in_sps++;
            continue;
        }
        RenderingInstanceComponent *ins = &inst_view.get<RenderingInstanceComponent>(instance_cull_result[i]);
        bool keep = false;

        if ((camera_layer_mask & ins->layer_mask) == 0) {

            //failure
        } else if (ins->base_type == RS::INSTANCE_LIGHT && ins->visible) {

            if (light_cull_count < MAX_LIGHTS_CULLED) {

                InstanceLightData *light = getUnchecked<InstanceLightData>(ins->self);

                //do not add this light if no geometry is affected by it..
                if (!light->geometries.empty()) {
                    assert(VSG::storage->light_get_type(ins->base)!=RS::LIGHT_DIRECTIONAL);
                    light_cull_result[light_cull_count] = ins;
                    light_instance_cull_result[light_cull_count] = light->instance;
                    if (p_shadow_atlas!=entt::null && VSG::storage->light_has_shadow(ins->base)) {
                        VSG::scene_render->light_instance_mark_visible(light->instance); //mark it visible for shadow allocation later
                    }

                    light_cull_count++;
                }
                for (int i = 0; i < light_cull_count; i++) {

                    ins = light_cull_result[i];
                    assert(VSG::storage->light_get_type(ins->base)!=RS::LIGHT_DIRECTIONAL);
                }
            }
        } else if (ins->base_type == RS::INSTANCE_REFLECTION_PROBE && ins->visible) {

            if (reflection_probe_cull_count < MAX_REFLECTION_PROBES_CULLED) {

                InstanceReflectionProbeData *reflection_probe = getUnchecked<InstanceReflectionProbeData>(ins->self);

                if (p_reflection_probe != reflection_probe->instance) {
                    //avoid entering The Matrix

                    if (!reflection_probe->geometries.empty()) {
                        //do not add this light if no geometry is affected by it..

                        if (reflection_probe->reflection_dirty || VSG::scene_render->reflection_probe_instance_needs_redraw(reflection_probe->instance)) {
                            if (!VSG::ecs->registry.any_of<DirtyRefProbe>(ins->self)) {
                                reflection_probe->render_step = 0;
                                VSG::ecs->registry.emplace<DirtyRefProbe>(ins->self);
                            }

                            reflection_probe->reflection_dirty = false;
                        }

                        if (VSG::scene_render->reflection_probe_instance_has_reflection(reflection_probe->instance)) {
                            reflection_probe_instance_cull_result[reflection_probe_cull_count] = reflection_probe->instance;
                            reflection_probe_cull_count++;
                        }
                    }
                }
            }

        } else if (ins->base_type == RS::INSTANCE_GI_PROBE && ins->visible) {
            VSG::ecs->registry.emplace_or_replace<DirtyGIProbe>(ins->self);
        } else if (has_component<GeometryComponent>(ins->self) && ins->visible && ins->cast_shadows != RS::SHADOW_CASTING_SETTING_SHADOWS_ONLY) {

            keep = true;

            InstanceGeometryData *geom = get_instance_geometry(ins->self);
            GeometryComponent & gcomp = get_component<GeometryComponent>(ins->self);
            if (ins->redraw_if_visible) {
                RenderingServerRaster::redraw_request(false);
            }

            if (ins->base_type == RS::INSTANCE_PARTICLES) {
                //particles visible? process them
                if (VSG::storage->particles_is_inactive(ins->base)) {
                    //but if nothing is going on, don't do it.
                    keep = false;
                } else {
                    if (OS::get_singleton()->is_update_pending(true)) {
                    VSG::storage->particles_request_process(ins->base);
                    //particles visible? request redraw
                        RenderingServerRaster::redraw_request(false);
                    }
                }
            }

            if (gcomp.lighting_dirty) {

                //only called when lights AABB enter/exit this geometry
                ins->light_instances.clear();
                ins->light_instances.reserve(geom->lighting.size());
                auto &l_wr(ins->light_instances);
                for (auto E : geom->lighting) {

                    InstanceLightData *light = getUnchecked<InstanceLightData>(E);

                    l_wr.emplace_back(light->instance);
                }

                gcomp.lighting_dirty = false;
            }

            if (gcomp.reflection_dirty) {

                //only called when reflection probe AABB enter/exit this geometry
                ins->reflection_probe_instances.clear();
                ins->reflection_probe_instances.reserve(geom->reflection_probes.size());
                auto &wr(ins->reflection_probe_instances);
                for (auto E : geom->reflection_probes) {

                    InstanceReflectionProbeData *reflection_probe = getUnchecked<InstanceReflectionProbeData>(E);

                    wr.emplace_back(reflection_probe->instance);
                }

                gcomp.reflection_dirty = false;
            }

            if (gcomp.gi_probes_dirty) {
                //only called when reflection probe AABB enter/exit this geometry
                ins->gi_probe_instances.clear();
                ins->gi_probe_instances.reserve(geom->gi_probes.size());
                auto &wr(ins->gi_probe_instances);

                for (auto E : geom->gi_probes) {

                    InstanceGIProbeData *gi_probe = getUnchecked<InstanceGIProbeData>(E);

                    wr.emplace_back(gi_probe->probe_instance);
                }

                gcomp.gi_probes_dirty = false;
            }
        }
        if (!keep) {
            // remove, no reason to keep
            instance_cull_count--;
            SWAP(instance_cull_result[i], instance_cull_result[instance_cull_count]);
            i--;
            ins->last_render_pass = 0; // make invalid
        } else {

            ins->last_render_pass = render_pass;
        }
    }
    if(invalid_entities_in_sps) {
        printf("BVH had %d invalidated entities in it\n",invalid_entities_in_sps);
        invalid_entities_in_sps=0;
    }
    /* STEP 5 - PROCESS LIGHTS */
    for (int i = 0; i < light_cull_count; i++) {

        RenderingInstanceComponent *ins = light_cull_result[i];
        assert(VSG::storage->light_get_type(ins->base)!=RS::LIGHT_DIRECTIONAL);
    }
    RenderingEntity *directional_light_ptr = &light_instance_cull_result[light_cull_count];
    directional_light_count = 0;

    // directional lights
    {

        RenderingInstanceComponent **lights_with_shadow = (RenderingInstanceComponent **)alloca(sizeof(RenderingInstanceComponent *) * scenario->directional_lights.size());
        int directional_shadow_count = 0;

        for (auto E : scenario->directional_lights) {

            RenderingInstanceComponent *dir_light = get<RenderingInstanceComponent>(E);

            if (light_cull_count + directional_light_count >= MAX_LIGHTS_CULLED) {
                break;
            }

            if (!dir_light->visible)
                continue;

            InstanceLightData *light = getUnchecked<InstanceLightData>(E);

            //check shadow..

            if (light) {
                if (p_shadow_atlas!=entt::null && VSG::storage->light_has_shadow(dir_light->base)) {
                    lights_with_shadow[directional_shadow_count++] = dir_light;
                }
                //add to list
                directional_light_ptr[directional_light_count++] = light->instance;
            }
        }

        VSG::scene_render->set_directional_shadow_count(directional_shadow_count);

        for (int i = 0; i < directional_shadow_count; i++) {

            _light_instance_update_shadow(lights_with_shadow[i], p_cam_transform, p_cam_projection, p_cam_orthogonal, p_shadow_atlas, scenario);
        }
    }

    { //setup shadow maps

        //SortArray<Instance*,_InstanceLightsort> sorter;
        //sorter.sort(light_cull_result,light_cull_count);
        for (int i = 0; i < light_cull_count; i++) {

            RenderingInstanceComponent *ins = light_cull_result[i];

            if (p_shadow_atlas==entt::null || !VSG::storage->light_has_shadow(ins->base))
                continue;

            InstanceLightData *light = getUnchecked<InstanceLightData>(ins->self);

            float coverage = 0.f;

            { //compute coverage

                Transform cam_xf = p_cam_transform;
                float zn = p_cam_projection.get_z_near();
                Plane p(cam_xf.origin + cam_xf.basis.get_axis(2) * -zn, -cam_xf.basis.get_axis(2)); //camera near plane

                // near plane half width and height
                Vector2 vp_half_extents = p_cam_projection.get_viewport_half_extents();

                switch (VSG::storage->light_get_type(ins->base)) {

                    case RS::LIGHT_OMNI: {

                        float radius = VSG::storage->light_get_param(ins->base, RS::LIGHT_PARAM_RANGE);

                        //get two points parallel to near plane
                        Vector3 points[2] = {
                            ins->transform.origin,
                            ins->transform.origin + cam_xf.basis.get_axis(0) * radius
                        };

                        if (!p_cam_orthogonal) {
                            //if using perspetive, map them to near plane
                            for (int j = 0; j < 2; j++) {
                                if (p.distance_to(points[j]) < 0) {
                                    points[j].z = -zn; //small hack to keep size constant when hitting the screen
                                }

                                p.intersects_segment(cam_xf.origin, points[j], &points[j]); //map to plane
                            }
                        }

                        float screen_diameter = points[0].distance_to(points[1]) * 2;
                        coverage = screen_diameter / (vp_half_extents.x + vp_half_extents.y);
                    } break;
                    case RS::LIGHT_SPOT: {

                        float radius = VSG::storage->light_get_param(ins->base, RS::LIGHT_PARAM_RANGE);
                        float angle = VSG::storage->light_get_param(ins->base, RS::LIGHT_PARAM_SPOT_ANGLE);

                        float w = radius * Math::sin(Math::deg2rad(angle));
                        float d = radius * Math::cos(Math::deg2rad(angle));

                        Vector3 base = ins->transform.origin - ins->transform.basis.get_axis(2).normalized() * d;

                        Vector3 points[2] = {
                            base,
                            base + cam_xf.basis.get_axis(0) * w
                        };

                        if (!p_cam_orthogonal) {
                            //if using perspetive, map them to near plane
                            for (int j = 0; j < 2; j++) {
                                if (p.distance_to(points[j]) < 0) {
                                    points[j].z = -zn; //small hack to keep size constant when hitting the screen
                                }

                                p.intersects_segment(cam_xf.origin, points[j], &points[j]); //map to plane
                            }
                        }

                        float screen_diameter = points[0].distance_to(points[1]) * 2;
                        coverage = screen_diameter / (vp_half_extents.x + vp_half_extents.y);

                    } break;
                    default: {
                        ERR_PRINT("Invalid Light Type");
                    }
                }
            }

            if (light->shadow_dirty) {
                light->last_version++;
                light->shadow_dirty = false;
            }

            bool redraw = VSG::scene_render->shadow_atlas_update_light(p_shadow_atlas, light->instance, coverage, light->last_version);

            if (redraw) {
                //must redraw!
                light->shadow_dirty = _light_instance_update_shadow(ins, p_cam_transform, p_cam_projection, p_cam_orthogonal, p_shadow_atlas, scenario);
            }
        }
    }

    // Calculate instance->depth from the camera, after shadow calculation has stopped overwriting instance->depth
    for (int i = 0; i < instance_cull_count; i++) {
        RenderingInstanceComponent *ins = get<RenderingInstanceComponent>(instance_cull_result[i]);
        if (((1 << ins->base_type) & RS::INSTANCE_GEOMETRY_MASK) && ins->visible && ins->cast_shadows != RS::SHADOW_CASTING_SETTING_SHADOWS_ONLY) {
            InstanceBoundsComponent& bounds = get_component<InstanceBoundsComponent>(instance_cull_result[i]);
            Vector3 center = ins->transform.origin;
            if (bounds.use_aabb_center) {
                center = bounds.transformed_aabb.position + (bounds.transformed_aabb.size * 0.5f);
            }
            if (p_cam_orthogonal) {
                ins->depth = near_plane.distance_to(center) - bounds.sorting_offset;
            } else {
                ins->depth = p_cam_transform.origin.distance_to(center) - bounds.sorting_offset;
            }
            ins->depth_layer = CLAMP(int(ins->depth * 16 / z_far), 0, 15);
        }
    }
}

void VisualServerScene::_render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, const int p_eye,
        bool p_cam_orthogonal, RenderingEntity p_force_environment, RenderingEntity p_scenario,
        RenderingEntity p_shadow_atlas, RenderingEntity p_reflection_probe, int p_reflection_probe_pass) {
    SCOPE_AUTONAMED

    RenderingScenarioComponent *scenario = get<RenderingScenarioComponent>(p_scenario);

    /* ENVIRONMENT */

    RenderingEntity environment=entt::null;
    if (p_force_environment!=entt::null) //camera has more environment priority
        environment = p_force_environment;
    else if (scenario->environment!=entt::null)
        environment = scenario->environment;
    else
        environment = scenario->fallback_environment;

    /* PROCESS GEOMETRY AND DRAW SCENE */

    VSG::scene_render->render_scene(p_cam_transform, p_cam_projection, p_cam_orthogonal,p_eye, Span<RenderingEntity>(instance_cull_result, instance_cull_count), light_instance_cull_result, light_cull_count + directional_light_count, reflection_probe_instance_cull_result, reflection_probe_cull_count, environment, p_shadow_atlas, scenario->reflection_atlas, p_reflection_probe, p_reflection_probe_pass);
}

void VisualServerScene::render_empty_scene(RenderingEntity p_scenario, RenderingEntity p_shadow_atlas) {

#ifndef _3D_DISABLED

    auto *scenario = get<RenderingScenarioComponent>(p_scenario);

    RenderingEntity environment;
    if (scenario->environment!=entt::null)
        environment = scenario->environment;
    else
        environment = scenario->fallback_environment;
    VSG::scene_render->render_scene(Transform(), CameraMatrix(), 0, true, Span<RenderingEntity>(), nullptr, 0, nullptr,
            0, environment, p_shadow_atlas, scenario->reflection_atlas, entt::null, 0);
#endif
}

bool VisualServerScene::_render_reflection_probe_step(RenderingInstanceComponent *p_instance, int p_step) {

    InstanceReflectionProbeData *reflection_probe = getUnchecked<InstanceReflectionProbeData>(p_instance->self);
    auto *scenario = get<RenderingScenarioComponent>(p_instance->scenario);
    ERR_FAIL_COND_V(!scenario, true);

    RenderingServerRaster::redraw_request(false); //update, so it updates in editor

    if (p_step == 0) {

        if (!VSG::scene_render->reflection_probe_instance_begin_render(reflection_probe->instance, scenario->reflection_atlas)) {
            return true; //sorry, all full :(
        }
    }

    if (p_step < 0 || p_step >= 6) {
        // do roughness postprocess step until it believes it's done
        return VSG::scene_render->reflection_probe_instance_postprocess_step(reflection_probe->instance);
    }
    static const Vector3 view_normals[6] = {
        Vector3(-1, 0, 0),
        Vector3(+1, 0, 0),
        Vector3(0, -1, 0),
        Vector3(0, +1, 0),
        Vector3(0, 0, -1),
        Vector3(0, 0, +1)
    };

    Vector3 extents = VSG::storage->reflection_probe_get_extents(p_instance->base);
    Vector3 origin_offset = VSG::storage->reflection_probe_get_origin_offset(p_instance->base);
    float max_distance = VSG::storage->reflection_probe_get_origin_max_distance(p_instance->base);

    Vector3 edge = view_normals[p_step] * extents;
    float distance = ABS(view_normals[p_step].dot(edge) - view_normals[p_step].dot(origin_offset)); //distance from origin offset to actual view distance limit

    max_distance = M_MAX(max_distance, distance);

    // render cubemap side
    CameraMatrix cm;
    cm.set_perspective(90, 1, 0.01f, max_distance);

    static const Vector3 view_up[6] = {
        Vector3(0, -1, 0),
        Vector3(0, -1, 0),
        Vector3(0, 0, -1),
        Vector3(0, 0, +1),
        Vector3(0, -1, 0),
        Vector3(0, -1, 0)
    };

    Transform local_view;
    local_view.set_look_at(origin_offset, origin_offset + view_normals[p_step], view_up[p_step]);

    Transform xform = p_instance->transform * local_view;

    RenderingEntity shadow_atlas = entt::null;

    if (VSG::storage->reflection_probe_renders_shadows(p_instance->base)) {
        shadow_atlas = scenario->reflection_probe_shadow_atlas;
    }

    _prepare_scene(xform, cm, false, entt::null, VSG::storage->reflection_probe_get_cull_mask(p_instance->base), p_instance->scenario, shadow_atlas, reflection_probe->instance, reflection_probe->previous_room_id_hint);
    bool async_forbidden_backup = VSG::storage->is_shader_async_hidden_forbidden();
    VSG::storage->set_shader_async_hidden_forbidden(true);
    _render_scene(xform, cm, 0, false, entt::null, p_instance->scenario, shadow_atlas, reflection_probe->instance, p_step);
    VSG::storage->set_shader_async_hidden_forbidden(async_forbidden_backup);
    return false;
}


void VisualServerScene::_gi_probe_bake_threads(void *self) {

    VisualServerScene *vss = (VisualServerScene *)self;
    vss->_gi_probe_bake_thread();
}

void _setup_gi_probe(RenderingInstanceComponent *p_instance) {

    VisualServerScene::InstanceGIProbeData *probe = getUnchecked<VisualServerScene::InstanceGIProbeData>(p_instance->self);

    if (probe->dynamic.probe_data!=entt::null) {
        VSG::storage->free(probe->dynamic.probe_data);
        probe->dynamic.probe_data = entt::null;
    }

    probe->dynamic.light_data = VSG::storage->gi_probe_get_dynamic_data(p_instance->base);

    if (probe->dynamic.light_data.empty())
        return;
    //using dynamic data
    PoolVector<int>::Read r = probe->dynamic.light_data.read();

    const VisualServerScene::GIProbeDataHeader *header = (VisualServerScene::GIProbeDataHeader *)r.ptr();

    probe->dynamic.local_data.resize(header->cell_count);

    int cell_count = probe->dynamic.local_data.size();
    Vector<VisualServerScene::InstanceGIProbeData::LocalData> &ldw = probe->dynamic.local_data;
    const VisualServerScene::GIProbeDataCell *cells = (VisualServerScene::GIProbeDataCell *)&r[16];

    probe->dynamic.level_cell_lists.resize(header->cell_subdiv);

    _gi_probe_fill_local_data(0, 0, 0, 0, 0, cells, header, ldw.data(), probe->dynamic.level_cell_lists.data());

    probe->dynamic.probe_data = VSG::storage->gi_probe_dynamic_data_create(header->width, header->height, header->depth);

    probe->dynamic.bake_dynamic_range = VSG::storage->gi_probe_get_dynamic_range(p_instance->base);

    probe->dynamic.mipmaps_3d.clear();
    probe->dynamic.propagate = VSG::storage->gi_probe_get_propagation(p_instance->base);

    probe->dynamic.grid_size[0] = header->width;
    probe->dynamic.grid_size[1] = header->height;
    probe->dynamic.grid_size[2] = header->depth;

    int size_limit = 1;
    int size_divisor = 1;

    for (int i = 0; i < (int)header->cell_subdiv; i++) {

        int x = header->width >> i;
        int y = header->height >> i;
        int z = header->depth >> i;

        //create and clear mipmap
        int size = x * y * z * 4;
        size /= size_divisor;

        Vector<uint8_t> mipmap(size,uint8_t(0));

        probe->dynamic.mipmaps_3d.emplace_back(eastl::move(mipmap));

        if (x <= size_limit || y <= size_limit || z <= size_limit)
            break;
    }

    probe->dynamic.updating_stage = GIUpdateStage::CHECK;
    probe->invalid = false;
    probe->dynamic.enabled = true;

    Transform cell_to_xform = VSG::storage->gi_probe_get_to_cell_xform(p_instance->base);
    AABB bounds = VSG::storage->gi_probe_get_bounds(p_instance->base);
    float cell_size = VSG::storage->gi_probe_get_cell_size(p_instance->base);

    probe->dynamic.light_to_cell_xform = cell_to_xform * p_instance->transform.affine_inverse();

    VSG::scene_render->gi_probe_instance_set_light_data(probe->probe_instance, p_instance->base, probe->dynamic.probe_data);
    VSG::scene_render->gi_probe_instance_set_transform_to_data(probe->probe_instance, probe->dynamic.light_to_cell_xform);

    VSG::scene_render->gi_probe_instance_set_bounds(probe->probe_instance, bounds.size / cell_size);

    probe->base_version = VSG::storage->gi_probe_get_version(p_instance->base);

}

void VisualServerScene::_gi_probe_bake_thread() {
    auto bake_view = VSG::ecs->registry.view<GIProbeBakeCheck>();
    auto instance_view = VSG::ecs->registry.view<RenderingInstanceComponent>();


    while (true) {

        probe_bake_sem.wait();
        if (probe_bake_thread_exit.is_set()) {
            break;
        }

        RenderingInstanceComponent *to_bake = nullptr;

        {
            MutexLock guard(probe_bake_mutex);

            if (!bake_view.empty()) {
                RenderingEntity baked_entity = bake_view.front();
                VSG::ecs->registry.remove<GIProbeBakeCheck>(baked_entity);
                assert(instance_view.contains(baked_entity));
                to_bake = &instance_view.get<RenderingInstanceComponent>(baked_entity);
            }
        }

        if (!to_bake)
            continue;

        _bake_gi_probe(to_bake);
    }
}

void VisualServerScene::_bake_gi_probe_light(const GIProbeDataHeader *header, const GIProbeDataCell *cells,
        InstanceGIProbeData::LocalData *local_data, const uint32_t *leaves, int p_leaf_count,
        const InstanceGIProbeData::LightCache &light_cache, int p_sign) {

    int light_r = int(light_cache.color.r * light_cache.energy * 1024.0f) * p_sign;
    int light_g = int(light_cache.color.g * light_cache.energy * 1024.0f) * p_sign;
    int light_b = int(light_cache.color.b * light_cache.energy * 1024.0f) * p_sign;

    float limits[3] = { float(header->width), float(header->height), float(header->depth) };
    int clip_planes = 0;

    switch (light_cache.type) {

        case RS::LIGHT_DIRECTIONAL: {
            Plane clip[3];

            float max_len = Vector3(limits[0], limits[1], limits[2]).length() * 1.1f;

            Vector3 light_axis = -light_cache.transform.basis.get_axis(2).normalized();

            for (int i = 0; i < 3; i++) {

                if (Math::is_zero_approx(light_axis[i]))
                    continue;

                clip[clip_planes].normal[i] = 1.0;

                if (light_axis[i] < 0) {

                    clip[clip_planes].d = limits[i] + 1;
                } else {
                    clip[clip_planes].d -= 1.0f;
                }

                clip_planes++;
            }

            float distance_adv = _get_normal_advance(light_axis);


            for (int i = 0; i < p_leaf_count; i++) {

                uint32_t idx = leaves[i];

                const GIProbeDataCell *cell = &cells[idx];
                InstanceGIProbeData::LocalData *light = &local_data[idx];

                Vector3 to(light->pos[0] + 0.5f, light->pos[1] + 0.5f, light->pos[2] + 0.5f);
                to += -light_axis.sign() * 0.47f; //make it more likely to receive a ray

                Vector3 norm(
                        (((cells[idx].normal >> 16) & 0xFF) / 255.0f) * 2.0f - 1.0f,
                        (((cells[idx].normal >> 8) & 0xFF) / 255.0f) * 2.0f - 1.0f,
                        (((cells[idx].normal >> 0) & 0xFF) / 255.0f) * 2.0f - 1.0f);

                float att = norm.dot(-light_axis);
                if (att < 0.001f) {
                    //not lighting towards this
                    continue;
                }

                Vector3 from = to - max_len * light_axis;

                for (int j = 0; j < clip_planes; j++) {

                    clip[j].intersects_segment(from, to, &from);
                }

                float distance = (to - from).length();
                distance += distance_adv - Math::fmod(distance, distance_adv); //make it reach the center of the box always
                from = to - light_axis * distance;

                uint32_t result = 0xFFFFFFFF;

                while (distance > -distance_adv) { //use this to avoid precision errors

                    result = _gi_bake_find_cell(cells, int(floor(from.x)), int(floor(from.y)), int(floor(from.z)), header->cell_subdiv);
                    if (result != 0xFFFFFFFF) {
                        break;
                    }

                    from += light_axis * distance_adv;
                    distance -= distance_adv;
                }

                if (result == idx) {
                    //cell hit itself! hooray!
                    light->energy[0] += int32_t(light_r * att * ((cell->albedo >> 16) & 0xFF) / 255.0f);
                    light->energy[1] += int32_t(light_g * att * ((cell->albedo >> 8) & 0xFF) / 255.0f);
                    light->energy[2] += int32_t(light_b * att * ((cell->albedo) & 0xFF) / 255.0f);
                }
            }


        } break;
        case RS::LIGHT_OMNI:
        case RS::LIGHT_SPOT: {
            Plane clip[3];


            Vector3 light_pos = light_cache.transform.origin;
            Vector3 spot_axis = -light_cache.transform.basis.get_axis(2).normalized();

            float local_radius = light_cache.radius * light_cache.transform.basis.get_axis(2).length();

            for (int i = 0; i < p_leaf_count; i++) {

                uint32_t idx = leaves[i];

                const GIProbeDataCell *cell = &cells[idx];
                InstanceGIProbeData::LocalData *light = &local_data[idx];

                Vector3 to(light->pos[0] + 0.5f, light->pos[1] + 0.5f, light->pos[2] + 0.5f);
                to += (light_pos - to).sign() * 0.47f; //make it more likely to receive a ray

                Vector3 norm(
                        (((cells[idx].normal >> 16) & 0xFF) / 255.0f) * 2.0f - 1.0f,
                        (((cells[idx].normal >> 8) & 0xFF) / 255.0f) * 2.0f - 1.0f,
                        (((cells[idx].normal >> 0) & 0xFF) / 255.0f) * 2.0f - 1.0f);

                Vector3 light_axis = (to - light_pos).normalized();
                float distance_adv = _get_normal_advance(light_axis);

                float att = norm.dot(-light_axis);
                if (att < 0.001) {
                    //not lighting towards this
                    continue;
                }

                {
                    float d = light_pos.distance_to(to);
                    if (d + distance_adv > local_radius)
                        continue; // too far away

                    float dt = CLAMP<float>((d + distance_adv) / local_radius, 0, 1);
                    att *= powf(1.0f - dt, light_cache.attenuation);
                }

                if (light_cache.type == RS::LIGHT_SPOT) {

                    float angle = Math::rad2deg(acos(light_axis.dot(spot_axis)));
                    if (angle > light_cache.spot_angle)
                        continue;

                    float d = CLAMP<float>(angle / light_cache.spot_angle, 0, 1);
                    att *= powf(1.0f - d, light_cache.spot_attenuation);
                }

                clip_planes = 0;

                for (int c = 0; c < 3; c++) {

                    if (Math::is_zero_approx(light_axis[c]))
                        continue;
                    clip[clip_planes].normal[c] = 1.0;

                    if (light_axis[c] < 0) {

                        clip[clip_planes].d = limits[c] + 1;
                    } else {
                        clip[clip_planes].d -= 1.0f;
                    }

                    clip_planes++;
                }

                Vector3 from = light_pos;

                for (int j = 0; j < clip_planes; j++) {

                    clip[j].intersects_segment(from, to, &from);
                }

                float distance = (to - from).length();

                distance -= Math::fmod(distance, distance_adv); //make it reach the center of the box always, but this tame make it closer
                from = to - light_axis * distance;

                uint32_t result = 0xFFFFFFFF;

                while (distance > -distance_adv) { //use this to avoid precision errors

                    result = _gi_bake_find_cell(cells, int(floor(from.x)), int(floor(from.y)), int(floor(from.z)), header->cell_subdiv);
                    if (result != 0xFFFFFFFF) {
                        break;
                    }

                    from += light_axis * distance_adv;
                    distance -= distance_adv;
                }

                if (result == idx) {
                    //cell hit itself! hooray!

                    light->energy[0] += int32_t(light_r * att * ((cell->albedo >> 16) & 0xFF) / 255.0f);
                    light->energy[1] += int32_t(light_g * att * ((cell->albedo >> 8) & 0xFF) / 255.0f);
                    light->energy[2] += int32_t(light_b * att * ((cell->albedo) & 0xFF) / 255.0f);
                }
            }
        } break;
    }
}


void VisualServerScene::_bake_gi_probe(RenderingInstanceComponent *p_gi_probe) {

    InstanceGIProbeData *probe_data = getUnchecked<InstanceGIProbeData>(p_gi_probe->self);

    PoolVector<int>::Read r = probe_data->dynamic.light_data.read();

    const GIProbeDataHeader *header = (const GIProbeDataHeader *)r.ptr();
    const GIProbeDataCell *cells = (const GIProbeDataCell *)&r[16];

    int leaf_count = probe_data->dynamic.level_cell_lists[header->cell_subdiv - 1].size();
    const uint32_t *leaves = probe_data->dynamic.level_cell_lists[header->cell_subdiv - 1].data();

    InstanceGIProbeData::LocalData *local_data = probe_data->dynamic.local_data.data();

    //remove what must be removed
    for (eastl::pair<const RenderingEntity,InstanceGIProbeData::LightCache> &E : probe_data->dynamic.light_cache) {

        RenderingEntity rid = E.first;
        const InstanceGIProbeData::LightCache &lc = E.second;

        if ((!probe_data->dynamic.light_cache_changes.contains(rid) || probe_data->dynamic.light_cache_changes[rid] != lc) && lc.visible) {
            //erase light data

            _bake_gi_probe_light(header, cells, local_data, leaves, leaf_count, lc, -1);
        }
    }

    //add what must be added
    for (eastl::pair<const RenderingEntity,InstanceGIProbeData::LightCache> &E : probe_data->dynamic.light_cache_changes) {

        RenderingEntity rid = E.first;
        const InstanceGIProbeData::LightCache &lc = E.second;

        if ((!probe_data->dynamic.light_cache.contains(rid) || probe_data->dynamic.light_cache[rid] != lc) && lc.visible) {
            //add light data

            _bake_gi_probe_light(header, cells, local_data, leaves, leaf_count, lc, 1);
        }
    }

    SWAP(probe_data->dynamic.light_cache_changes, probe_data->dynamic.light_cache);

    //downscale to lower res levels
    _bake_gi_downscale_light(0, 0, cells, header, local_data, probe_data->dynamic.propagate);

    //plot result to 3D texture!


    for (int i = 0; i < (int)header->cell_subdiv; i++) {

        int stage = header->cell_subdiv - i - 1;

        if (stage >= probe_data->dynamic.mipmaps_3d.size())
            continue; //no mipmap for this one

        //print_line("generating mipmap stage: " + itos(stage));
        int level_cell_count = probe_data->dynamic.level_cell_lists[i].size();
        const uint32_t *level_cells = probe_data->dynamic.level_cell_lists[i].data();

        uint8_t *mipmapw = probe_data->dynamic.mipmaps_3d[stage].data();

        uint32_t sizes[3] = { header->width >> stage, header->height >> stage, header->depth >> stage };

        for (int j = 0; j < level_cell_count; j++) {

            uint32_t idx = level_cells[j];

            uint32_t r2 = (uint32_t(local_data[idx].energy[0]) / probe_data->dynamic.bake_dynamic_range) >> 2;
            uint32_t g = (uint32_t(local_data[idx].energy[1]) / probe_data->dynamic.bake_dynamic_range) >> 2;
            uint32_t b = (uint32_t(local_data[idx].energy[2]) / probe_data->dynamic.bake_dynamic_range) >> 2;
            uint32_t a = (cells[idx].level_alpha >> 8) & 0xFF;

            uint32_t mm_ofs = sizes[0] * sizes[1] * (local_data[idx].pos[2]) + sizes[0] * (local_data[idx].pos[1]) + (local_data[idx].pos[0]);
            mm_ofs *= 4; //for RGBA (4 bytes)

            mipmapw[mm_ofs + 0] = uint8_t(MIN(r2, 255));
            mipmapw[mm_ofs + 1] = uint8_t(MIN(g, 255));
            mipmapw[mm_ofs + 2] = uint8_t(MIN(b, 255));
            mipmapw[mm_ofs + 3] = uint8_t(MIN(a, 255));
        }
    }

    //send back to main thread to update un little chunks
    {
        MutexLock guard(probe_bake_mutex);
        probe_data->dynamic.updating_stage = GIUpdateStage::UPLOADING;
    }
}

void render_gi_probes()
{
    auto dirty_probe_view(VSG::ecs->registry.view<DirtyGIProbe,RenderingInstanceComponent>());

    dirty_probe_view.each([=](auto ent, RenderingInstanceComponent &comp) {
        RenderingInstanceComponent *instance_probe = &comp;
        auto *probe = getUnchecked<VisualServerScene::InstanceGIProbeData>(ent);
        assert(probe);

        //check if probe must be setup, but don't do if on the lighting thread

        bool force_lighting = false;

        if (probe->invalid || (probe->dynamic.updating_stage == GIUpdateStage::CHECK &&
                                      probe->base_version != VSG::storage->gi_probe_get_version(instance_probe->base))) {

            _setup_gi_probe(instance_probe);
            force_lighting = true;
        }

        float propagate = VSG::storage->gi_probe_get_propagation(instance_probe->base);

        if (probe->dynamic.propagate != propagate) {
            probe->dynamic.propagate = propagate;
            force_lighting = true;
        }

        if (!probe->invalid && probe->dynamic.enabled) {

            switch (probe->dynamic.updating_stage) {
                case GIUpdateStage::CHECK: {

                    if (_check_gi_probe(instance_probe) || force_lighting) { //send to lighting thread
                        {
                            MutexLock guard(VSG::scene->probe_bake_mutex);
                            probe->dynamic.updating_stage = GIUpdateStage::LIGHTING;
                            VSG::ecs->registry.emplace<GIProbeBakeCheck>(ent);
                        }
                        VSG::scene->probe_bake_sem.post();
                    }
                } break;
                case GIUpdateStage::LIGHTING: {
                    //do none, wait til done!

                } break;
                case GIUpdateStage::UPLOADING: {

                         //uint64_t us = OS::get_singleton()->get_ticks_usec();

                    for (int i = 0; i < (int)probe->dynamic.mipmaps_3d.size(); i++) {

                        const Vector<uint8_t> &r(probe->dynamic.mipmaps_3d[i]);
                        VSG::storage->gi_probe_dynamic_data_update(probe->dynamic.probe_data, 0, probe->dynamic.grid_size[2] >> i, i, r.data());
                    }

                    probe->dynamic.updating_stage = GIUpdateStage::CHECK;

                         //print_line("UPLOAD TIME: " + rtos((OS::get_singleton()->get_ticks_usec() - us) / 1000000.0));
                } break;
            }
        }
    });
}

void render_ref_probes()
{
    auto dirty_probe_view(VSG::ecs->registry.view<DirtyRefProbe,RenderingInstanceComponent,VisualServerScene::InstanceReflectionProbeData>());
    bool busy = false;
    dirty_probe_view.each([&](auto ent, RenderingInstanceComponent &comp, VisualServerScene::InstanceReflectionProbeData &refl) {
        assert(refl.owner==ent);
        assert(comp.self==ent);
        switch (VSG::storage->reflection_probe_get_update_mode(ent)) {
            case RS::REFLECTION_PROBE_UPDATE_ONCE: {
                if (busy) //already rendering something
                    break;

                bool done = VSG::scene->_render_reflection_probe_step(&comp, refl.render_step);
                if (!done) {
                    refl.render_step++;
                }

                busy = true; //do not render another one of this kind
            } break;
            case RS::REFLECTION_PROBE_UPDATE_ALWAYS: {

                int step = 0;
                bool done = false;
                while (!done) {
                    done = VSG::scene->_render_reflection_probe_step(&comp, step);
                    step++;
                }
            } break;
        }
        VSG::ecs->registry.remove<DirtyRefProbe>(ent);
    });


}

void VisualServerScene::render_probes() {

    /* REFLECTION PROBES */


    render_ref_probes();

    /* GI PROBES */

    render_gi_probes();
}

_FORCE_INLINE_ void VisualServerScene::_update_dirty_instance(RenderingInstanceComponent *p_instance)
{
    const Dirty & dt = get_component<Dirty>(p_instance->self);

    if (dt.update_aabb) {
        _update_instance_aabb(p_instance);
    }

    if (dt.update_materials) {

        _update_instance_material(p_instance);
    }

    _update_instance(p_instance);
    clear_component<Dirty>(p_instance->self);
}

void VisualServerScene::_update_instance_material(RenderingInstanceComponent *p_instance) {

    if (p_instance->base_type == RS::INSTANCE_MESH) {
        //remove materials no longer used and un-own them

        int new_mat_count = VSG::storage->mesh_get_surface_count(p_instance->base);
        for (int i = p_instance->materials.size() - 1; i >= new_mat_count; i--) {
            if (p_instance->materials[i]!=entt::null) {
                VSG::storage->material_remove_instance_owner(p_instance->materials[i], p_instance->self);
            }
        }
        p_instance->materials.resize(new_mat_count,entt::null);

        int new_blend_shape_count = VSG::storage->mesh_get_blend_shape_count(p_instance->base);
        if (new_blend_shape_count != p_instance->blend_values.size()) {
            p_instance->blend_values.resize(new_blend_shape_count);
            for (int i = 0; i < new_blend_shape_count; i++) {
                p_instance->blend_values[i] = 0;
            }
        }
    }
    if (has_component<GeometryComponent>(p_instance->self)) {

        InstanceGeometryData *geom = get_instance_geometry(p_instance->self);
        auto & gcomp = get_component<GeometryComponent>(p_instance->self);

        bool can_cast_shadows = true;
        bool is_animated = false;

        if (p_instance->cast_shadows == RS::SHADOW_CASTING_SETTING_OFF) {
            can_cast_shadows = false;
        } else if (p_instance->material_override!=entt::null) {
            can_cast_shadows = VSG::storage->material_casts_shadows(p_instance->material_override);
            is_animated = VSG::storage->material_is_animated(p_instance->material_override);
        } else {

            if (p_instance->base_type == RS::INSTANCE_MESH) {
                RenderingEntity mesh = p_instance->base;

                if (mesh!=entt::null) {
                    bool cast_shadows = false;

                    for (int i = 0; i < p_instance->materials.size(); i++) {

                        RenderingEntity mat = p_instance->materials[i]!=entt::null ? p_instance->materials[i] : VSG::storage->mesh_surface_get_material(mesh, i);

                        if (mat==entt::null) {
                            cast_shadows = true;
                        } else {

                            if (VSG::storage->material_casts_shadows(mat)) {
                                cast_shadows = true;
                            }

                            if (VSG::storage->material_is_animated(mat)) {
                                is_animated = true;
                            }
                        }
                    }

                    if (!cast_shadows) {
                        can_cast_shadows = false;
                    }
                }

            } else if (p_instance->base_type == RS::INSTANCE_MULTIMESH) {
                RenderingEntity mesh = VSG::storage->multimesh_get_mesh(p_instance->base);
                if (mesh!=entt::null) {

                    bool cast_shadows = false;

                    int sc = VSG::storage->mesh_get_surface_count(mesh);
                    for (int i = 0; i < sc; i++) {

                        RenderingEntity mat = VSG::storage->mesh_surface_get_material(mesh, i);

                        if (mat==entt::null) {
                            cast_shadows = true;

                        } else {

                            if (VSG::storage->material_casts_shadows(mat)) {
                                cast_shadows = true;
                            }
                            if (VSG::storage->material_is_animated(mat)) {
                                is_animated = true;
                            }
                        }
                    }

                    if (!cast_shadows) {
                        can_cast_shadows = false;
                    }
                }
            } else if (p_instance->base_type == RS::INSTANCE_IMMEDIATE) {

                RenderingEntity mat = VSG::storage->immediate_get_material(p_instance->base);

                can_cast_shadows = mat==entt::null || VSG::storage->material_casts_shadows(mat);

                if (mat!=entt::null && VSG::storage->material_is_animated(mat)) {
                    is_animated = true;
                }
            } else if (p_instance->base_type == RS::INSTANCE_PARTICLES) {

                bool cast_shadows = false;

                int dp = VSG::storage->particles_get_draw_passes(p_instance->base);

                for (int i = 0; i < dp; i++) {

                    RenderingEntity mesh = VSG::storage->particles_get_draw_pass_mesh(p_instance->base, i);
                    if (mesh==entt::null) {
                        continue;
                    }

                    int sc = VSG::storage->mesh_get_surface_count(mesh);
                    for (int j = 0; j < sc; j++) {

                        RenderingEntity mat = VSG::storage->mesh_surface_get_material(mesh, j);

                        if (mat==entt::null) {
                            cast_shadows = true;
                        } else {

                            if (VSG::storage->material_casts_shadows(mat)) {
                                cast_shadows = true;
                            }

                            if (VSG::storage->material_is_animated(mat)) {
                                is_animated = true;
                            }
                        }
                    }
                }

                if (!cast_shadows) {
                    can_cast_shadows = false;
                }
            }
        }

        if (p_instance->material_overlay!=entt::null) {
            can_cast_shadows = can_cast_shadows || VSG::storage->material_casts_shadows(p_instance->material_overlay);
            is_animated = is_animated || VSG::storage->material_is_animated(p_instance->material_overlay);
        }
        if (can_cast_shadows != gcomp.can_cast_shadows) {
            //ability to cast shadows change, let lights now
            for (auto E : geom->lighting) {
                InstanceLightData *light = getUnchecked<InstanceLightData>(E);
                light->shadow_dirty = true;
            }

            gcomp.can_cast_shadows = can_cast_shadows;
        }

        gcomp.material_is_animated = is_animated;
    }

    clear_component<Dirty>(p_instance->self);

    _update_instance(p_instance);

}

void VisualServerScene::update_dirty_instances() {

    SCOPE_AUTONAMED

    {
        SCOPE_PROFILE(update_resources);
        VSG::storage->update_dirty_resources();
    }

    auto view = VSG::ecs->registry.view<RenderingInstanceComponent, Dirty>();
    FixedVector<RenderingScenarioComponent *,16,true> scenarios_to_update;
    for (auto entity : view) {
        RenderingInstanceComponent *p_instance = &view.get<RenderingInstanceComponent>(entity);
        const Dirty & dt = view.get<Dirty>(entity);
        if (dt.update_aabb) {
            _update_instance_aabb(p_instance);
        }
        if (dt.update_materials) {
            _update_instance_material(p_instance);
        }
        _update_instance(p_instance);
        auto *scenario = get<RenderingScenarioComponent>(p_instance->scenario);
        if(scenario && !scenarios_to_update.contains(scenario)) {
            scenarios_to_update.emplace_back(scenario);
        }
    }
    //remove dirty for everything
    VSG::ecs->registry.clear<Dirty>();
    for(auto scn : scenarios_to_update) {
        scn->sps.update();
    }
}


VisualServerScene *VisualServerScene::singleton = nullptr;

VisualServerScene::VisualServerScene() {

    probe_bake_thread.start(_gi_probe_bake_threads, this);
    singleton = this;
    GLOBAL_DEF("rendering/quality/spatial_partitioning/bvh_collision_margin", 0.1);
    ProjectSettings::get_singleton()->set_custom_property_info(
            "rendering/quality/spatial_partitioning/bvh_collision_margin",
            PropertyInfo(VariantType::FLOAT, "rendering/quality/spatial_partitioning/bvh_collision_margin",
                    PropertyHint::Range, "0.0,2.0,0.01"));
}

VisualServerScene::~VisualServerScene() {
    probe_bake_thread_exit.set();
    probe_bake_sem.post();
    probe_bake_thread.wait_to_finish();
}

void RenderingScenarioComponent::unregister_scenario() {
    for(RenderingEntity inst : instances) {
        ::instance_set_scenario(inst, entt::null);
    }
    instances.clear();
    if(reflection_probe_shadow_atlas!=entt::null) {
        VSG::storage->free(reflection_probe_shadow_atlas);
        reflection_probe_shadow_atlas = entt::null;
    }

    if(reflection_atlas!=entt::null) {
        VSG::storage->free(reflection_atlas);
        reflection_atlas=entt::null;
    }
}
bool SpatialPartitioningScene_BVH::UserCullTestFunction::user_cull_check(const RenderingEntity p_a, const RenderingEntity p_b) {
    assert(p_a!=entt::null);
    assert(p_b!=entt::null);
    RenderingInstanceComponent *A =  get<RenderingInstanceComponent>(p_a);
    RenderingInstanceComponent *B =  get<RenderingInstanceComponent>(p_b);
    uint32_t a_mask = A->bvh_pairable_mask;
    uint32_t a_type = A->bvh_pairable_type;
    uint32_t b_mask = B->bvh_pairable_mask;
    uint32_t b_type = B->bvh_pairable_type;

    if (!_cull_pairing_mask_test_hit(a_mask, a_type, b_mask, b_type)) {
        return false;
    }

    return true;
}

