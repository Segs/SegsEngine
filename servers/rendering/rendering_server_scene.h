/*************************************************************************/
/*  rendering_server_scene.h                                                */
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

#pragma once

#include "servers/rendering/rasterizer.h"
#include "core/hash_map.h"
#include "core/math/geometry.h"
#include "core/math/bvh.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/os/mutex.h"
#include "core/list.h"
#include "core/self_list.h"
#include "core/deque.h"
#include "servers/rendering/portals/portal_renderer.h"
#include "servers/rendering/render_entity_helpers.h"
#include "servers/rendering/rendering_server_globals.h"

struct NewOctree {};
enum ARVREyes : int8_t;
class ARVRInterface;
struct RenderingInstanceComponent;
class RenderingServerCallbacks;

enum class GIUpdateStage : int8_t {
    CHECK,
    LIGHTING,
    UPLOADING,
};

struct ComponentPairInfo {
    //light entity in geometry
    //gi probe entity in geometry
    //reflection entity in geometry
    RenderingEntity L;
    RenderingEntity geometry = entt::null;
};

struct Camera3DComponent {

    enum Type {
        PERSPECTIVE,
        ORTHOGONAL,
        FRUSTUM
    };
    Type type = PERSPECTIVE;
    float fov = 70.0f;
    float znear = 0.05f;
    float zfar = 100.0f;
    float size = 1.0f;
    Vector2 offset{};
    uint32_t visible_layers = 0xFFFFFFFF;
    bool vaspect = false;
    MoveOnlyEntityHandle env;

    // transform_prev is only used when using fixed timestep interpolation
    Transform transform;
    int32_t previous_room_id_hint=-1;
};

// common interface for all spatial partitioning schemes
// this is a bit excessive boilerplate-wise but can be removed if we decide to stick with one method

// note this is actually the BVH id +1, so that visual server can test against zero
// for validity to maintain compatibility with octree (where 0 indicates invalid)
typedef uint32_t SpatialPartitionID;

class SpatialPartitioningScene_BVH {
    class UserPairTestFunction {
    public:
        static bool user_pair_check(RenderingEntity p_a, RenderingEntity p_b) {
            // return false if no collision, decided by masks etc
            return true;
        }
    };

    class UserCullTestFunction {
        // write this logic once for use in all routines
        // double check this as a possible source of bugs in future.
        static bool _cull_pairing_mask_test_hit(uint32_t p_maskA, uint32_t p_typeA, uint32_t p_maskB, uint32_t p_typeB) {
            // double check this as a possible source of bugs in future.
            const bool A_match_B = p_maskA & p_typeB;

            if (!A_match_B) {
                bool B_match_A = p_maskB & p_typeA;
                if (!B_match_A) {
                    return false;
                }
            }

            return true;
        }

    public:
        static bool user_cull_check(const RenderingEntity p_a, const RenderingEntity p_b);
    };
    // Note that SpatialPartitionIDs are +1 based when stored in visual server, to enable 0 to indicate invalid ID.
    BVH_Manager<RenderingEntity, true, 256, UserPairTestFunction, UserCullTestFunction> _bvh;
    RenderingEntity _dummy_cull_object;

public:
    typedef void *(*PairCallback)(void *, uint32_t, RenderingEntity, int, uint32_t, RenderingEntity, int);
    typedef void (*UnpairCallback)(void *, uint32_t, RenderingEntity, int, uint32_t, RenderingEntity, int, void *);

    SpatialPartitionID create(RenderingEntity p_userdata, const AABB &p_aabb = AABB(), int p_subindex = 0, bool p_pairable = false, uint32_t p_pairable_type = 0, uint32_t p_pairable_mask = 1);
    void erase(SpatialPartitionID p_handle) { _bvh.erase(p_handle - 1); check_bvh_userdata(); }
    void move(SpatialPartitionID p_handle, const AABB &p_aabb) { _bvh.move(p_handle - 1, p_aabb); check_bvh_userdata(); }
    void activate(SpatialPartitionID p_handle, const AABB &p_aabb);
    void deactivate(SpatialPartitionID p_handle);
    void force_collision_check(SpatialPartitionID p_handle);
    void update() { _bvh.update(); check_bvh_userdata();}
    void update_collisions() { _bvh.update_collisions(); check_bvh_userdata(); }
    void set_pairable(RenderingInstanceComponent *p_instance, bool p_pairable, uint32_t p_pairable_type, uint32_t p_pairable_mask);
    int cull_convex(Span<const Plane> p_convex, Span<RenderingEntity> p_result_array, uint32_t p_mask = 0xFFFFFFFF);
    int cull_aabb(const AABB &p_aabb, Span<RenderingEntity> p_result_array, int *p_subindex_array = nullptr, uint32_t p_mask = 0xFFFFFFFF);
    int cull_segment(const Vector3 &p_from, const Vector3 &p_to, Span<RenderingEntity> p_result_array, int *p_subindex_array = nullptr, uint32_t p_mask = 0xFFFFFFFF);
    void set_pair_callback(PairCallback p_callback, void *p_userdata) {
        _bvh.set_pair_callback(p_callback, p_userdata);
    }
    void set_unpair_callback(UnpairCallback p_callback, void *p_userdata) {
        _bvh.set_unpair_callback(p_callback, p_userdata);
    }
    void check_bvh_userdata();
    void params_set_node_expansion(real_t p_value) { _bvh.params_set_node_expansion(p_value); }
    void params_set_pairing_expansion(real_t p_value) { _bvh.params_set_pairing_expansion(p_value); }
    SpatialPartitioningScene_BVH &operator=(SpatialPartitioningScene_BVH &&from) noexcept {
        _bvh = eastl::move(from._bvh);
        _dummy_cull_object = from._dummy_cull_object;
        from._dummy_cull_object = entt::null;
        return *this;
    }
    SpatialPartitioningScene_BVH();
    ~SpatialPartitioningScene_BVH();
    SpatialPartitioningScene_BVH(const SpatialPartitioningScene_BVH&) = delete;
    SpatialPartitioningScene_BVH &operator=(const SpatialPartitioningScene_BVH &) = delete;
};

struct RenderingScenarioComponent {


    SpatialPartitioningScene_BVH sps;

    Vector<RenderingEntity> directional_lights;
    PortalRenderer _portal_renderer;
    MoveOnlyEntityHandle self;
    MoveOnlyEntityHandle environment;
    MoveOnlyEntityHandle fallback_environment;
    MoveOnlyEntityHandle reflection_probe_shadow_atlas;
    MoveOnlyEntityHandle reflection_atlas;
    Vector<RenderingEntity> instances;
    RS::ScenarioDebugMode debug=RS::SCENARIO_DEBUG_DISABLED;

    void unregister_scenario();

    RenderingScenarioComponent(const RenderingScenarioComponent&) = delete;
    RenderingScenarioComponent &operator=(const RenderingScenarioComponent&) = delete;

    RenderingScenarioComponent(RenderingScenarioComponent &&from) noexcept {
        *this = eastl::move(from);
    }
    RenderingScenarioComponent &operator=(RenderingScenarioComponent &&from) noexcept {
        unregister_scenario();
        if(this==&from)
        {
            assert(false);
            sps = {};
            directional_lights.clear();
        } else {
            sps = eastl::move(from.sps);
            directional_lights = eastl::move(from.directional_lights);
            instances = eastl::move(from.instances);
        }
        self = eastl::move(from.self);
        environment = eastl::move(from.environment);
        fallback_environment = eastl::move(from.fallback_environment);
        reflection_probe_shadow_atlas = eastl::move(from.reflection_probe_shadow_atlas);
        reflection_atlas = eastl::move(from.reflection_atlas);
        debug = eastl::move(from.debug);
        return *this;
    }

    RenderingScenarioComponent() {}
    ~RenderingScenarioComponent()
    {
        unregister_scenario();
    }
};

struct RenderingInstanceLightmapCaptureDataComponent  {
    List<ComponentPairInfo> geometries;

    HashSet<RenderingEntity> users; // RenderingInstanceComponent *

    RenderingInstanceLightmapCaptureDataComponent() {}
};
namespace RoomAPI {
/* ROOMS */
RenderingEntity room_create();
void room_set_scenario(RenderingEntity p_room, RenderingEntity p_scenario);
void room_add_instance(RenderingEntity p_room, RenderingEntity p_instance, const AABB &p_aabb, const Vector<Vector3> &p_object_pts);
void room_add_ghost(RenderingEntity p_room, GameEntity p_object_id, const AABB &p_aabb);
void room_set_bound(RenderingEntity p_room, GameEntity p_room_object_id, const Vector<Plane> &p_convex, const AABB &p_aabb, const Vector<Vector3> &p_verts);
void room_prepare(RenderingEntity p_room, int32_t p_priority);
void rooms_and_portals_clear(RenderingEntity p_scenario);
void rooms_unload(RenderingEntity p_scenario, String p_reason);
void rooms_finalize(RenderingEntity p_scenario, bool p_generate_pvs, bool p_cull_using_pvs, bool p_use_secondary_pvs, bool p_use_signals, String p_pvs_filename, bool p_use_simple_pvs, bool p_log_pvs_generation);
void rooms_override_camera(RenderingEntity p_scenario, bool p_override, const Vector3 &p_point, const Span<const Plane> *p_convex);
void rooms_set_active(RenderingEntity p_scenario, bool p_active);
void rooms_set_params(RenderingEntity p_scenario, int p_portal_depth_limit, real_t p_roaming_expansion_margin);
void rooms_set_debug_feature(RenderingEntity p_scenario, RS::RoomsDebugFeature p_feature, bool p_active);
void rooms_update_gameplay_monitor(RenderingEntity p_scenario, const Vector<Vector3> &p_camera_positions);

// don't use this in a game
bool rooms_is_loaded(RenderingEntity p_scenario);

/* ROOMGROUPS */
RenderingEntity roomgroup_create();
void roomgroup_prepare(RenderingEntity p_roomgroup, GameEntity p_roomgroup_object_id);
void roomgroup_set_scenario(RenderingEntity p_roomgroup, RenderingEntity p_scenario);
void roomgroup_add_room(RenderingEntity p_roomgroup, RenderingEntity p_room);

// Occlusion 'ghosts'
RenderingEntity ghost_create();
void ghost_set_scenario(RenderingEntity p_ghost, RenderingEntity p_scenario, GameEntity p_id, const AABB &p_aabb);
void ghost_update(RenderingEntity p_ghost, const AABB &p_aabb);

}
class VisualServerScene {
    friend void render_ref_probes();
    /* CAMERA API */

public:
    enum {

        MAX_INSTANCE_CULL = 65536,
        MAX_LIGHTS_CULLED = 4096,
        MAX_REFLECTION_PROBES_CULLED = 4096,
        MAX_ROOM_CULL = 32,
        MAX_EXTERIOR_PORTALS = 128,
    };

    uint64_t render_pass = 1;

    static VisualServerScene *singleton;

    /* EVENT QUEUING */

    void tick();
    void pre_draw(bool p_will_draw);
// FIXME: Kept as reference for future implementation


    RenderingEntity camera_create();
    void camera_set_perspective(RenderingEntity p_camera, float p_fovy_degrees, float p_z_near, float p_z_far);
    void camera_set_orthogonal(RenderingEntity p_camera, float p_size, float p_z_near, float p_z_far);
    void camera_set_frustum(RenderingEntity p_camera, float p_size, Vector2 p_offset, float p_z_near, float p_z_far);
    void camera_set_transform(RenderingEntity p_camera, const Transform &p_transform);
    void camera_set_cull_mask(RenderingEntity p_camera, uint32_t p_layers);
    void camera_set_environment(RenderingEntity p_camera, RenderingEntity p_env);
    void camera_set_use_vertical_aspect(RenderingEntity p_camera, bool p_enable);
    static bool owns_camera(RenderingEntity p_camera);
    /* SCENARIO API */

    static void *_instance_pair(void *p_self, SpatialPartitionID, RenderingEntity p_A, int, SpatialPartitionID, RenderingEntity p_B, int);
    static void _instance_unpair(void *p_self, SpatialPartitionID, RenderingEntity p_A, int, SpatialPartitionID, RenderingEntity p_B, int, void *);

    RenderingEntity scenario_create();

    void scenario_set_debug(RenderingEntity p_scenario, RS::ScenarioDebugMode p_debug_mode);
    void scenario_set_environment(RenderingEntity p_scenario, RenderingEntity p_environment);
    void scenario_set_fallback_environment(RenderingEntity p_scenario, RenderingEntity p_environment);
    void scenario_set_reflection_atlas_size(RenderingEntity p_scenario, int p_size, int p_subdiv);

    /* INSTANCING API */
    void _instance_queue_update(RenderingInstanceComponent *p_instance, bool p_update_aabb, bool p_update_materials = false);

    struct InstanceReflectionProbeData   {
        List<ComponentPairInfo> geometries;

        RenderingEntity owner = entt::null; //RenderingInstanceComponent
        RenderingEntity instance = entt::null;
        int32_t previous_room_id_hint = -1;
        int render_step = -1;
        bool reflection_dirty = true;
    };


    struct InstanceLightData {
        List<ComponentPairInfo> geometries;
        RenderingEntity instance = entt::null;
        uint64_t last_version = 0;
        bool D = false; // directional light in scenario
        bool shadow_dirty = true;
        int32_t previous_room_id_hint = -1;
    };

    struct InstanceGIProbeData {

        RenderingEntity owner = entt::null;
        List<ComponentPairInfo> geometries;

        HashSet<RenderingInstanceComponent *> lights;

        struct LightCache {

            RS::LightType type= RS::LIGHT_DIRECTIONAL;
            Transform transform;
            Color color;
            float energy=1.0f;
            float radius = 1.0f;
            float attenuation = 1.0f;
            float spot_angle = 1.0f;
            float spot_attenuation = 1.0f;
            bool visible=true;

            bool operator==(const LightCache &p_cache) const noexcept {

                return (type == p_cache.type &&
                        transform == p_cache.transform &&
                        color == p_cache.color &&
                        energy == p_cache.energy &&
                        radius == p_cache.radius &&
                        attenuation == p_cache.attenuation &&
                        spot_angle == p_cache.spot_angle &&
                        spot_attenuation == p_cache.spot_attenuation &&
                        visible == p_cache.visible);
            }

            bool operator!=(const LightCache &p_cache) const noexcept {

                return !operator==(p_cache);
            }

            LightCache() = default;
        };

        struct LocalData {
            uint16_t pos[3];
            uint16_t energy[3]; //using 0..1024 for float range 0..1. integer is needed for deterministic add/remove of lights
        };

        struct CompBlockS3TC {
            uint32_t offset; //offset in mipmap
            uint32_t source_count; //sources
            uint32_t sources[16]; //id for each source
            uint8_t alpha[8]; //alpha block is pre-computed
        };

        struct Dynamic {

            HashMap<RenderingEntity, LightCache> light_cache;
            HashMap<RenderingEntity, LightCache> light_cache_changes;
            PoolVector<int> light_data;
            Vector<LocalData> local_data;
            Vector<Vector<uint32_t> > level_cell_lists;
            Vector<Vector<uint8_t> > mipmaps_3d;
            Vector<PoolVector<CompBlockS3TC> > mipmaps_s3tc; //for s3tc

            Transform light_to_cell_xform;
            RenderingEntity probe_data = entt::null;
            int bake_dynamic_range;
            int grid_size[3];
            float propagate;
            bool enabled;
            GIUpdateStage updating_stage;
        } dynamic;

        RenderingEntity probe_instance = entt::null;

        bool invalid;
        uint32_t base_version;

        InstanceGIProbeData() {
            invalid = true;
            base_version = 0;
            dynamic.updating_stage = GIUpdateStage::CHECK;
        }
    };

    int instance_cull_count;
    RenderingEntity instance_cull_result[MAX_INSTANCE_CULL];
    RenderingEntity instance_shadow_cull_result[MAX_INSTANCE_CULL]; //used for generating shadowmaps
    RenderingInstanceComponent *light_cull_result[MAX_LIGHTS_CULLED];
    RenderingEntity light_instance_cull_result[MAX_LIGHTS_CULLED];
    int light_cull_count;
    int directional_light_count;
    RenderingEntity reflection_probe_instance_cull_result[MAX_REFLECTION_PROBES_CULLED];
    int reflection_probe_cull_count;

    RenderingEntity instance_create();

    void instance_set_base(RenderingEntity p_instance, RenderingEntity p_base);
    void instance_set_scenario(RenderingEntity p_instance, RenderingEntity p_scenario);
    void instance_set_layer_mask(RenderingEntity p_instance, uint32_t p_mask);
    void instance_set_transform(RenderingEntity p_instance, const Transform &p_transform);
    void instance_attach_object_instance_id(RenderingEntity p_instance, GameEntity p_id);
    void instance_set_blend_shape_weight(RenderingEntity p_instance, int p_shape, float p_weight);
    void instance_set_surface_material(RenderingEntity p_instance, int p_surface, RenderingEntity p_material);
    void instance_set_visible(RenderingEntity p_instance, bool p_visible);
    static void instance_set_use_lightmap(RenderingEntity p_instance, RenderingEntity p_lightmap_instance, RenderingEntity p_lightmap, int p_lightmap_slice, const Rect2 &p_lightmap_uv_rect);

    void instance_set_custom_aabb(RenderingEntity p_instance, AABB p_aabb);

    void instance_attach_skeleton(RenderingEntity p_instance, RenderingEntity p_skeleton);

    void instance_set_extra_visibility_margin(RenderingEntity p_instance, real_t p_margin);
    // Portals
    void instance_set_portal_mode(RenderingEntity p_instance, RS::InstancePortalMode p_mode);
    RenderingEntity portal_create();
    void portal_set_scenario(RenderingEntity p_portal, RenderingEntity p_scenario);
    void portal_set_geometry(RenderingEntity p_portal, const Vector<Vector3> &p_points, real_t p_margin);
    void portal_link(RenderingEntity p_portal, RenderingEntity p_room_from, RenderingEntity p_room_to, bool p_two_way);
    void portal_set_active(RenderingEntity p_portal, bool p_active);
public:
    RenderingEntity occluder_instance_create();
    void occluder_instance_set_scenario(RenderingEntity p_occluder_instance, RenderingEntity p_scenario);
    void occluder_instance_link_resource(RenderingEntity p_occluder_instance, RenderingEntity p_occluder_resource);
    void occluder_instance_set_transform(RenderingEntity p_occluder, const Transform &p_xform);
    void occluder_instance_set_active(RenderingEntity p_occluder, bool p_active);

    RenderingEntity occluder_resource_create();
    void occluder_resource_prepare(RenderingEntity p_occluder_resource, RS::OccluderType p_type);
    void occluder_resource_spheres_update(RenderingEntity p_occluder_resource, const Vector<Plane> &p_spheres);
    void occluder_resource_mesh_update(RenderingEntity p_occluder_resource, const OccluderMeshData &p_mesh_data);

    void set_use_occlusion_culling(bool p_enable);

    // editor only .. slow
    Geometry::MeshData occlusion_debug_get_current_polys(RenderingEntity p_scenario) const;
    const PortalResources &get_portal_resources() const { return _portal_resources; }
    PortalResources &get_portal_resources() { return _portal_resources; }

    void callbacks_register(RenderingServerCallbacks *p_callbacks);
    RenderingServerCallbacks *get_callbacks() const { return _visual_server_callbacks; }
    // don't use these in a game!
    Vector<GameEntity> instances_cull_aabb(const AABB &p_aabb, RenderingEntity p_scenario) const;
    Vector<GameEntity> instances_cull_ray(const Vector3 &p_from, const Vector3 &p_to, RenderingEntity p_scenario) const;
    Vector<GameEntity> instances_cull_convex(Span<const Plane> p_convex, RenderingEntity p_scenario) const;

    void instance_geometry_set_flag(RenderingEntity p_instance, RS::InstanceFlags p_flags, bool p_enabled);
    void instance_geometry_set_cast_shadows_setting(RenderingEntity p_instance, RS::ShadowCastingSetting p_shadow_casting_setting);
    void instance_geometry_set_material_override(RenderingEntity p_instance, RenderingEntity p_material);
    void instance_geometry_set_material_overlay(RenderingEntity p_instance, RenderingEntity p_material);

    void instance_geometry_set_draw_range(RenderingEntity p_instance, float p_min, float p_max, float p_min_margin, float p_max_margin);
    void instance_geometry_set_as_instance_lod(RenderingEntity p_instance, RenderingEntity p_as_lod_of_instance);

    _FORCE_INLINE_ void _update_instance(RenderingInstanceComponent *p_instance);
    _FORCE_INLINE_ void _update_instance_aabb(RenderingInstanceComponent *p_instance);
    _FORCE_INLINE_ void _update_dirty_instance(RenderingInstanceComponent *p_instance);
    void _update_instance_material(RenderingInstanceComponent *p_instance);
    _FORCE_INLINE_ void _update_instance_lightmap_captures(RenderingInstanceComponent *p_instance);

    _FORCE_INLINE_ bool _light_instance_update_shadow(RenderingInstanceComponent *p_instance, const Transform &p_cam_transform,
            const CameraMatrix &p_cam_projection, bool p_cam_orthogonal, RenderingEntity p_shadow_atlas, RenderingScenarioComponent *p_scenario);

    void _prepare_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_orthogonal,
            RenderingEntity p_force_environment, uint32_t p_visible_layers, RenderingEntity p_scenario, RenderingEntity p_shadow_atlas,
            RenderingEntity p_reflection_probe, int32_t &r_previous_room_id_hint);
    void _render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, const int p_eye, bool p_cam_orthogonal,
            RenderingEntity p_force_environment, RenderingEntity p_scenario, RenderingEntity p_shadow_atlas, RenderingEntity p_reflection_probe,
            int p_reflection_probe_pass);
    void render_empty_scene(RenderingEntity p_scenario, RenderingEntity p_shadow_atlas);

    void render_camera(RenderingEntity p_camera, RenderingEntity p_scenario, Size2 p_viewport_size, RenderingEntity p_shadow_atlas);
    void render_camera(Ref<ARVRInterface> &p_interface, ARVREyes p_eye, RenderingEntity p_camera, RenderingEntity p_scenario,
            Size2 p_viewport_size, RenderingEntity p_shadow_atlas);
    void update_dirty_instances();

    //probes
    struct GIProbeDataHeader {

        uint32_t version;
        uint32_t cell_subdiv;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t cell_count;
        uint32_t leaf_cell_count;
    };

    struct GIProbeDataCell {

        uint32_t children[8];
        uint32_t albedo;
        uint32_t emission;
        uint32_t normal;
        uint32_t level_alpha;
    };

    void _gi_probe_bake_thread();
    static void _gi_probe_bake_threads(void *);

    SafeFlag probe_bake_thread_exit;
    Thread probe_bake_thread;
    Semaphore probe_bake_sem;
    Mutex probe_bake_mutex;

    void render_probes();


    VisualServerScene();
    virtual ~VisualServerScene();
private:
    RenderingServerCallbacks *_visual_server_callbacks = nullptr;
    PortalResources _portal_resources;
protected:
    bool _render_reflection_probe_step(RenderingInstanceComponent *p_instance, int p_step);
    void _bake_gi_probe_light(const GIProbeDataHeader *header, const GIProbeDataCell *cells,
            InstanceGIProbeData::LocalData *local_data, const uint32_t *leaves, int p_leaf_count,
            const InstanceGIProbeData::LightCache &light_cache, int p_sign);
    void _bake_gi_probe(RenderingInstanceComponent *p_gi_probe);
};

extern void set_instance_dirty(RenderingEntity id, bool p_update_aabb, bool p_update_materials);
extern void _instance_destroy_occlusion_rep(RenderingInstanceComponent *p_instance);
extern void _instance_create_occlusion_rep(RenderingInstanceComponent *p_instance);
extern bool _instance_get_transformed_aabb(RenderingEntity p_instance, AABB &r_aabb);
extern GameEntity _instance_get_object_ID(VSInstance *p_instance);
extern bool _instance_cull_check(const VSInstance *p_instance, uint32_t p_cull_mask);
extern VSInstance *_instance_get_from_rid(RenderingEntity p_instance);
extern bool _instance_get_transformed_aabb_for_occlusion(RenderingEntity p_instance, AABB &r_aabb);

