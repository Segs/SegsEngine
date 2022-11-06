#pragma once

#include "core/vector.h"
#include "core/math/transform.h"
#include "core/math/transform_interpolator.h"
#include "core/color.h"
#include "core/list.h"

#include "rendering_server_scene.h"
#include "servers/rendering/render_entity_helpers.h"
#include "servers/rendering_server_enums.h"

#include <core/math/rect2.h>

typedef uint32_t SpatialPartitionID;
typedef uint32_t OcclusionHandle;
struct InstanceBaseData;

struct DirtyGIProbe {};
struct DirtyRefProbe {};
struct GIProbeBakeCheck {};

// p_instance can be mesh, light, poly, area and portal so far.
void instance_set_scenario(RenderingEntity p_instance, RenderingEntity p_scenario);
void instance_set_base(RenderingEntity p_instance, RenderingEntity p_base);
void instance_geometry_set_material_override(RenderingEntity p_instance, RenderingEntity p_material);
void instance_geometry_set_material_overlay(RenderingEntity p_instance, RenderingEntity p_material);
void instance_attach_skeleton(RenderingEntity p_instance, RenderingEntity p_skeleton);
void instance_set_use_lightmap(RenderingEntity p_instance, RenderingEntity p_lightmap_instance, RenderingEntity p_lightmap, int p_lightmap_slice, const Rect2 &p_lightmap_uv_rect);


struct InstanceGeometryData {
    Vector<RenderingEntity> lighting;
    Vector<RenderingEntity> reflection_probes;
    Vector<RenderingEntity> gi_probes;
    Vector<RenderingEntity> lightmap_captures;
};

struct GeometryComponent {
    InstanceGeometryData *Data;
    bool lighting_dirty : 1;
    bool can_cast_shadows : 1;
    bool material_is_animated : 1;
    bool reflection_dirty : 1;
    bool gi_probes_dirty: 1;

    GeometryComponent(const GeometryComponent &) = delete;
    GeometryComponent &operator=(const GeometryComponent &) = delete;

    GeometryComponent(GeometryComponent &&) = default;
    GeometryComponent &operator=(GeometryComponent &&) = default;

    GeometryComponent() {
        Data = nullptr;
        lighting_dirty = false;
        reflection_dirty = true;
        can_cast_shadows = true;
        material_is_animated = true;
        gi_probes_dirty = true;
    }
    GeometryComponent(InstanceGeometryData * _Data) {
        Data = _Data;
        lighting_dirty = false;
        reflection_dirty = true;
        can_cast_shadows = true;
        material_is_animated = true;
        gi_probes_dirty = true;
    }
};

struct RenderingInstanceComponent {

    RS::InstanceType base_type = RS::INSTANCE_NONE;
    MoveOnlyEntityHandle base;
    MoveOnlyEntityHandle self;
    MoveOnlyEntityHandle skeleton;
    MoveOnlyEntityHandle material_override;
    MoveOnlyEntityHandle material_overlay;
    MoveOnlyEntityHandle instance_owner;

    Transform transform;

    int depth_layer=0;
    uint32_t layer_mask=1;

         //RenderingEntity sampled_light;

    Vector<RenderingEntity> materials;
    Vector<RenderingEntity> light_instances;
    Vector<RenderingEntity> reflection_probe_instances;
    Vector<RenderingEntity> gi_probe_instances;

    Vector<float> blend_values;

    RS::ShadowCastingSetting cast_shadows=RS::SHADOW_CASTING_SETTING_ON;

         //fit in 32 bits
    bool mirror : 1;
    bool receive_shadows : 1;
    bool visible : 1;
    bool baked_light : 1; //this flag is only to know if it actually did use baked light
    bool dynamic_gi : 1;
    bool redraw_if_visible : 1;
    float depth; //used for sorting

    MoveOnlyEntityHandle lightmap_capture = entt::null; //RendererInstanceComponent
    MoveOnlyEntityHandle lightmap = entt::null;
    // eastl::unique_ptr<eastl::array<Color,12>>
    Vector<Color> lightmap_capture_data; //in an array (12 values) to avoid wasting space if unused. Alpha is unused, but needed to send to shader

    int lightmap_slice = -1;
    Rect2 lightmap_uv_rect = {0,0,1,1};

    //scenario stuff
    MoveOnlyEntityHandle scenario = entt::null;
    SpatialPartitionID spatial_partition_id = 0;

         //aabb stuff

    GameEntity object_id{ entt::null };
    // rooms & portals
    OcclusionHandle occlusion_handle=0; // handle of instance in occlusion system (or 0)
    RS::InstancePortalMode portal_mode = RS::INSTANCE_PORTAL_MODE_STATIC;

    float lod_begin=0;
    float lod_end=0;
    float lod_begin_hysteresis=0;
    float lod_end_hysteresis=0;
    MoveOnlyEntityHandle lod_instance = entt::null;

    // These are used for the user cull testing function
    // in the BVH, this is precached rather than recalculated each time.
    uint32_t bvh_pairable_mask = 0;
    uint32_t bvh_pairable_type = 0;

    uint64_t last_render_pass=0;
    uint64_t last_frame_pass=0;

    uint64_t version; // changes to this, and changes to base increase version

         //RenderingEntity base_data;

    void base_removed()  {
        instance_set_base(self, entt::null);
    }

    void base_changed(bool p_aabb, bool p_materials) {
        set_instance_dirty(self, p_aabb, p_materials);
    }
    void release_resources();

    RenderingInstanceComponent(const RenderingInstanceComponent&) = delete;
    RenderingInstanceComponent& operator=(const RenderingInstanceComponent&) = delete;

    RenderingInstanceComponent(RenderingInstanceComponent&&);
    RenderingInstanceComponent& operator=(RenderingInstanceComponent&&);

    RenderingInstanceComponent(RenderingEntity s) : self(s) {
        cast_shadows = RS::SHADOW_CASTING_SETTING_ON;
        receive_shadows = true;
        visible = true;
        baked_light = false;
        dynamic_gi = false;
        redraw_if_visible = false;
        lightmap_capture = entt::null;

        visible = true;

        lod_begin = 0;
        lod_end = 0;
        lod_begin_hysteresis = 0;
        lod_end_hysteresis = 0;

        last_render_pass = 0;
        last_frame_pass = 0;
        version = 1;
        //base_data = entt::null;

    }

    ~RenderingInstanceComponent();
};
