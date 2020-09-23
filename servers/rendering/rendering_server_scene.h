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

#include "core/math/geometry.h"
#include "core/math/octree.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/self_list.h"
#include "core/deque.h"

struct NewOctree {};
enum ARVREyes : int8_t;
class ARVRInterface;

class VisualServerScene {
    /* CAMERA API */

    struct Camera3D : public RID_Data {

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
        RID env;

        Transform transform;
    };
public:
    enum {

        MAX_INSTANCE_CULL = 65536,
        MAX_LIGHTS_CULLED = 4096,
        MAX_REFLECTION_PROBES_CULLED = 4096,
        MAX_ROOM_CULL = 32,
        MAX_EXTERIOR_PORTALS = 128,
    };

    uint64_t render_pass;

    static VisualServerScene *singleton;

// FIXME: Kept as reference for future implementation


    RID camera_create();
    void camera_set_perspective(RID p_camera, float p_fovy_degrees, float p_z_near, float p_z_far);
    void camera_set_orthogonal(RID p_camera, float p_size, float p_z_near, float p_z_far);
    void camera_set_frustum(RID p_camera, float p_size, Vector2 p_offset, float p_z_near, float p_z_far);
    void camera_set_transform(RID p_camera, const Transform &p_transform);
    void camera_set_cull_mask(RID p_camera, uint32_t p_layers);
    void camera_set_environment(RID p_camera, RID p_env);
    void camera_set_use_vertical_aspect(RID p_camera, bool p_enable);
    static bool owns_camera(RID p_camera);
    /* SCENARIO API */

    struct Instance;

    struct Scenario : RID_Data {

        RS::ScenarioDebugMode debug;
        RID self;

        Octree<Instance, true> octree;

        Vector<Instance *> directional_lights;
        RID environment;
        RID fallback_environment;
        RID reflection_probe_shadow_atlas;
        RID reflection_atlas;

        IntrusiveList<Instance> instances;

        Scenario() { debug = RS::SCENARIO_DEBUG_DISABLED; }
    };

    mutable RID_Owner<Scenario> scenario_owner;

    static void *_instance_pair(void *p_self, OctreeElementID, Instance *p_A, int, OctreeElementID, Instance *p_B, int);
    static void _instance_unpair(void *p_self, OctreeElementID, Instance *p_A, int, OctreeElementID, Instance *p_B, int, void *);

    RID scenario_create();

    void scenario_set_debug(RID p_scenario, RS::ScenarioDebugMode p_debug_mode);
    void scenario_set_environment(RID p_scenario, RID p_environment);
    void scenario_set_fallback_environment(RID p_scenario, RID p_environment);
    void scenario_set_reflection_atlas_size(RID p_scenario, int p_size, int p_subdiv);

    /* INSTANCING API */

    struct InstanceBaseData {

        virtual ~InstanceBaseData() = default;
    };

    struct Instance : RasterizerScene::InstanceBase {

        RID self;
        //scenario stuff
        Scenario *scenario = nullptr;
        IntrusiveListNode<Instance> scenario_item;
        OctreeElementID octree_id = 0;

        //aabb stuff

        ObjectID object_id {0ULL};

        float lod_begin;
        float lod_end;
        float lod_begin_hysteresis;
        float lod_end_hysteresis;
        RID lod_instance;

        uint64_t last_render_pass;
        uint64_t last_frame_pass;

        uint64_t version; // changes to this, and changes to base increase version

        InstanceBaseData *base_data;

        void base_removed() override {

            singleton->instance_set_base(self, RID());
        }

        void base_changed(bool p_aabb, bool p_materials) override {

            singleton->_instance_queue_update(this, p_aabb, p_materials);
        }

        Instance() :
                scenario_item(this) {

            visible = true;

            lod_begin = 0;
            lod_end = 0;
            lod_begin_hysteresis = 0;
            lod_end_hysteresis = 0;

            last_render_pass = 0;
            last_frame_pass = 0;
            version = 1;
            base_data = nullptr;

        }

        ~Instance() override {

            if (base_data)
                memdelete(base_data);
        }
    };

    void _instance_queue_update(Instance *p_instance, bool p_update_aabb, bool p_update_materials = false);

    struct InstanceReflectionProbeData : public InstanceBaseData {

        Instance *owner;

        struct PairInfo {
            List<Instance *>::iterator L; //reflection iterator in geometry
            Instance *geometry;
        };
        List<PairInfo> geometries;

        RID instance;
        bool reflection_dirty;
        IntrusiveListNode<InstanceReflectionProbeData> update_list;

        int render_step;

        InstanceReflectionProbeData() :
                update_list(this) {

            reflection_dirty = true;
            render_step = -1;
        }
    };

    IntrusiveList<InstanceReflectionProbeData> reflection_probe_render_list;

    struct InstanceLightData : public InstanceBaseData {

        struct PairInfo {
            List<Instance *>::iterator L; //light iterator in geometry
            Instance *geometry;
        };

        RID instance;
        uint64_t last_version;
        bool D; // directional light in scenario
        bool shadow_dirty;

        List<PairInfo> geometries;

        Instance *baked_light;

        InstanceLightData() {

            shadow_dirty = true;
            D = false;
            last_version = 0;
            baked_light = nullptr;
        }
    };

    struct InstanceGIProbeData : public InstanceBaseData {

        Instance *owner;

        struct PairInfo {
            List<Instance *>::iterator L; //gi probe iterator in geometry
            Instance *geometry;
        };

        List<PairInfo> geometries;

        HashSet<Instance *> lights;

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

            HashMap<RID, LightCache> light_cache;
            HashMap<RID, LightCache> light_cache_changes;
            PoolVector<int> light_data;
            Vector<LocalData> local_data;
            Vector<Vector<uint32_t> > level_cell_lists;
            RID probe_data;
            int bake_dynamic_range;
            bool enabled;
            RasterizerStorage::GIProbeCompression compression;

            Vector<Vector<uint8_t> > mipmaps_3d;
            Vector<PoolVector<CompBlockS3TC> > mipmaps_s3tc; //for s3tc

            int updating_stage;
            float propagate;

            int grid_size[3];

            Transform light_to_cell_xform;

        } dynamic;

        RID probe_instance;

        bool invalid;
        uint32_t base_version;

        IntrusiveListNode<InstanceGIProbeData> update_element;

        InstanceGIProbeData() :
                update_element(this) {
            invalid = true;
            base_version = 0;
            dynamic.updating_stage = GI_UPDATE_STAGE_CHECK;
        }
    };

    IntrusiveList<InstanceGIProbeData> gi_probe_update_list;

    struct InstanceLightmapCaptureData : public InstanceBaseData {

        struct PairInfo {
            List<Instance *>::iterator L; //iterator in geometry
            Instance *geometry;
        };
        List<PairInfo> geometries;

        HashSet<Instance *> users;

        InstanceLightmapCaptureData() {}
    };

    int instance_cull_count;
    Instance *instance_cull_result[MAX_INSTANCE_CULL];
    Instance *instance_shadow_cull_result[MAX_INSTANCE_CULL]; //used for generating shadowmaps
    Instance *light_cull_result[MAX_LIGHTS_CULLED];
    RID light_instance_cull_result[MAX_LIGHTS_CULLED];
    int light_cull_count;
    int directional_light_count;
    RID reflection_probe_instance_cull_result[MAX_REFLECTION_PROBES_CULLED];
    int reflection_probe_cull_count;

    RID_Owner<Instance> instance_owner;

    RID instance_create();

    void instance_set_base(RID p_instance, RID p_base); // from can be mesh, light, poly, area and portal so far.
    void instance_set_scenario(RID p_instance, RID p_scenario); // from can be mesh, light, poly, area and portal so far.
    void instance_set_layer_mask(RID p_instance, uint32_t p_mask);
    void instance_set_transform(RID p_instance, const Transform &p_transform);
    void instance_attach_object_instance_id(RID p_instance, ObjectID p_id);
    void instance_set_blend_shape_weight(RID p_instance, int p_shape, float p_weight);
    void instance_set_surface_material(RID p_instance, int p_surface, RID p_material);
    void instance_set_visible(RID p_instance, bool p_visible);
    void instance_set_use_lightmap(RID p_instance, RID p_lightmap_instance, RID p_lightmap);

    void instance_set_custom_aabb(RID p_instance, AABB p_aabb);

    void instance_attach_skeleton(RID p_instance, RID p_skeleton);

    void instance_set_extra_visibility_margin(RID p_instance, real_t p_margin);

    // don't use these in a game!
    Vector<ObjectID> instances_cull_aabb(const AABB &p_aabb, RID p_scenario = RID()) const;
    Vector<ObjectID> instances_cull_ray(const Vector3 &p_from, const Vector3 &p_to, RID p_scenario = RID()) const;
    Vector<ObjectID> instances_cull_convex(Span<const Plane> p_convex, RID p_scenario = RID()) const;

    void instance_geometry_set_flag(RID p_instance, RS::InstanceFlags p_flags, bool p_enabled);
    void instance_geometry_set_cast_shadows_setting(RID p_instance, RS::ShadowCastingSetting p_shadow_casting_setting);
    void instance_geometry_set_material_override(RID p_instance, RID p_material);

    void instance_geometry_set_draw_range(RID p_instance, float p_min, float p_max, float p_min_margin, float p_max_margin);
    void instance_geometry_set_as_instance_lod(RID p_instance, RID p_as_lod_of_instance);

    _FORCE_INLINE_ void _update_instance(Instance *p_instance);
    _FORCE_INLINE_ void _update_instance_aabb(Instance *p_instance);
    _FORCE_INLINE_ void _update_dirty_instance(Instance *p_instance);
    void _update_instance_material(Instance *p_instance);
    _FORCE_INLINE_ void _update_instance_lightmap_captures(Instance *p_instance);

    _FORCE_INLINE_ bool _light_instance_update_shadow(Instance *p_instance, const Transform p_cam_transform,
            const CameraMatrix &p_cam_projection, bool p_cam_orthogonal, RID p_shadow_atlas, Scenario *p_scenario);

    void _prepare_scene(const Transform p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_orthogonal,
            RID p_force_environment, uint32_t p_visible_layers, RID p_scenario, RID p_shadow_atlas,
            RID p_reflection_probe);
    void _render_scene(const Transform p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_orthogonal,
            RID p_force_environment, RID p_scenario, RID p_shadow_atlas, RID p_reflection_probe,
            int p_reflection_probe_pass);
    void render_empty_scene(RID p_scenario, RID p_shadow_atlas);

    void render_camera(RID p_camera, RID p_scenario, Size2 p_viewport_size, RID p_shadow_atlas);
    void render_camera(Ref<ARVRInterface> &p_interface, ARVREyes p_eye, RID p_camera, RID p_scenario,
            Size2 p_viewport_size, RID p_shadow_atlas);
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

    enum {
        GI_UPDATE_STAGE_CHECK,
        GI_UPDATE_STAGE_LIGHTING,
        GI_UPDATE_STAGE_UPLOADING,
    };

    void _gi_probe_bake_thread();
    static void _gi_probe_bake_threads(void *);

    volatile bool probe_bake_thread_exit;
    Thread *probe_bake_thread;
    Semaphore *probe_bake_sem;
    Mutex *probe_bake_mutex;
    Deque<Instance *> probe_bake_list;

    bool _render_reflection_probe_step(Instance *p_instance, int p_step);
    void _gi_probe_fill_local_data(int p_idx, int p_level, int p_x, int p_y, int p_z, const GIProbeDataCell *p_cell,
            const GIProbeDataHeader *p_header, InstanceGIProbeData::LocalData *p_local_data,
            Vector<uint32_t> *prev_cell);

    _FORCE_INLINE_ uint32_t _gi_bake_find_cell(const GIProbeDataCell *cells, int x, int y, int z, int p_cell_subdiv);
    void _bake_gi_downscale_light(int p_idx, int p_level, const GIProbeDataCell *p_cells,
            const GIProbeDataHeader *p_header, InstanceGIProbeData::LocalData *p_local_data, float p_propagate);
    void _bake_gi_probe_light(const GIProbeDataHeader *header, const GIProbeDataCell *cells,
            InstanceGIProbeData::LocalData *local_data, const uint32_t *leaves, int p_leaf_count,
            const InstanceGIProbeData::LightCache &light_cache, int p_sign);
    void _bake_gi_probe(Instance *p_gi_probe);
    bool _check_gi_probe(Instance *p_gi_probe);
    void _setup_gi_probe(Instance *p_instance);

    void render_probes();

    bool free(RID p_rid);

    VisualServerScene();
    virtual ~VisualServerScene();
};

