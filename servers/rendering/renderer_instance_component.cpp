#include "renderer_instance_component.h"

#include "core/os/os.h"
#include "core/os/memory.h"

#include "rendering_server_scene.h"
#include "render_entity_getter.h"
#include "rendering_server_scene.h"


static void instance_clear_scanario(RenderingInstanceComponent *instance) {
    ERR_FAIL_COND(!instance);
    RenderingScenarioComponent *old_scene = get<RenderingScenarioComponent>(instance->scenario);


    if (!old_scene) {
        instance->scenario = entt::null;
        // can't be in a tree if there's no such scene.
        assert(instance->spatial_partition_id==0);
        return;
    }

    old_scene->instances.erase_first_unsorted(instance->self);

    if (instance->spatial_partition_id) {
        old_scene->sps.erase(instance->spatial_partition_id);
#ifdef TRACY_ENABLE
        VSG::bvh_nodes_destroyed++;
#endif
        instance->spatial_partition_id = 0;
    }

    // handle occlusion changes
    if (instance->occlusion_handle) {
        _instance_destroy_occlusion_rep(instance);
    }
    instance->scenario = entt::null;
    switch (instance->base_type) {

        case RS::INSTANCE_LIGHT: {

            auto *light = getUnchecked<VisualServerScene::InstanceLightData>(instance->self);
            if (light->D) {
                old_scene->directional_lights.erase_first(instance->self);
                light->D = false;
            }
        } break;
        case RS::INSTANCE_REFLECTION_PROBE: {

            auto *reflection_probe = getUnchecked<VisualServerScene::InstanceReflectionProbeData>(instance->self);
            VSG::scene_render->reflection_probe_release_atlas_index(reflection_probe->instance);
        } break;
        case RS::INSTANCE_GI_PROBE: {
            VSG::ecs->registry.remove<DirtyGIProbe>(instance->self);
        } break;
        default: {
        }
    }
}

static void instance_clear_base(RenderingInstanceComponent *instance) {
    ERR_FAIL_COND(!instance);

    auto *scenario = get<RenderingScenarioComponent>(instance->scenario);

    if(instance->base_type == RS::INSTANCE_NONE) {
        instance->base = entt::null;
        assert(!(scenario && instance->spatial_partition_id)); // make sure that nothing strange is happening with spatial accel
        return;
    }

    // free anything related to that base

    VSG::storage->instance_remove_dependency(instance->base, instance->self);

    if (instance->base_type == RS::INSTANCE_GI_PROBE) {
        // if gi probe is baking, wait until done baking, else race condition may happen when removing it
        // from octree
        auto *gi_probe = &VSG::ecs->registry.get<VisualServerScene::InstanceGIProbeData>(instance->self);

        //make sure probes are done baking
        auto bake_view(VSG::ecs->registry.view<GIProbeBakeCheck>());
        while (!bake_view.empty()) {
            OS::get_singleton()->delay_usec(1);
        }
        //make sure this one is done baking

        while (gi_probe->dynamic.updating_stage == GIUpdateStage::LIGHTING) {
            //wait until bake is done if it's baking
            OS::get_singleton()->delay_usec(1);
        }
    }

    if (scenario && instance->spatial_partition_id) {
#ifdef TRACY_ENABLE
        VSG::bvh_nodes_destroyed++;
#endif
        scenario->sps.erase(instance->spatial_partition_id);
        instance->spatial_partition_id = 0;
    }

    switch (instance->base_type) {
        case RS::INSTANCE_LIGHT: {
            auto *light = &VSG::ecs->registry.get<VisualServerScene::InstanceLightData>(instance->self);

            if (scenario && light->D) {
                scenario->directional_lights.erase_first(instance->self);
                light->D = false;
            }
            VSG::ecs->registry.remove<VisualServerScene::InstanceLightData>(instance->self);
        } break;
        case RS::INSTANCE_REFLECTION_PROBE: {
            auto *reflection_probe = getUnchecked<VisualServerScene::InstanceReflectionProbeData>(instance->self);
            VSG::storage->free(reflection_probe->instance);
            VSG::ecs->registry.remove<DirtyRefProbe>(instance->self);
            VSG::ecs->registry.remove<VisualServerScene::InstanceReflectionProbeData>(instance->self);
        } break;
        case RS::INSTANCE_LIGHTMAP_CAPTURE: {
            auto &lightmap_capture(VSG::ecs->registry.get<RenderingInstanceLightmapCaptureDataComponent>(instance->self));
            //erase dependencies, since no longer a lightmap
            for(RenderingEntity re : lightmap_capture.users) {
                instance_set_use_lightmap(re, entt::null, entt::null,-1,Rect2(0,0,1,1));
            }
            VSG::ecs->registry.remove<RenderingInstanceLightmapCaptureDataComponent>(instance->self);
        } break;
        case RS::INSTANCE_GI_PROBE: {
            auto *gi_probe = &VSG::ecs->registry.get<VisualServerScene::InstanceGIProbeData>(instance->self);

            VSG::ecs->registry.remove<DirtyGIProbe>(instance->self);
            VSG::storage->free(gi_probe->dynamic.probe_data);

            if (instance->lightmap_capture!=entt::null) {
                auto *lightmap_capture = &VSG::ecs->registry.get<RenderingInstanceLightmapCaptureDataComponent>(instance->lightmap_capture);
                lightmap_capture->users.erase(instance->self);
                instance->lightmap_capture = entt::null;
                instance->lightmap = entt::null;
            }

            VSG::storage->free(gi_probe->probe_instance);
            VSG::ecs->registry.remove<VisualServerScene::InstanceGIProbeData>(instance->self);

        } break;
        default: {
        }
    }

    instance->blend_values.clear();
    instance->blend_values.shrink_to_fit();

    for(RenderingEntity mat_ent : instance->materials) {
        if (mat_ent!=entt::null) {
            VSG::storage->material_remove_instance_owner(mat_ent, instance->self);
        }
    }
    instance->materials.clear();
    instance->base_type = RS::INSTANCE_NONE;
    instance->base = entt::null;
}

void instance_set_base(RenderingEntity p_instance, RenderingEntity p_base) {
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);

    instance_clear_base(instance);

    if (p_base==entt::null) {
        return;
    }
    auto *scenario = get<RenderingScenarioComponent>(instance->scenario);

    instance->base_type = VSG::storage->get_base_type(p_base);
    ERR_FAIL_COND(instance->base_type == RS::INSTANCE_NONE);

    auto instbase=instance->self.value;
    switch (instance->base_type) {
        case RS::INSTANCE_LIGHT: {

            auto *light = &VSG::ecs->registry.emplace<VisualServerScene::InstanceLightData>(instbase);

            if (scenario && VSG::storage->light_get_type(p_base) == RS::LIGHT_DIRECTIONAL) {
                scenario->directional_lights.push_back(p_instance);
                light->D = true;
            }

            light->instance = VSG::scene_render->light_instance_create(p_base);
        } break;
        case RS::INSTANCE_MESH:
        case RS::INSTANCE_MULTIMESH:
        case RS::INSTANCE_IMMEDIATE:
        case RS::INSTANCE_PARTICLES: {
            InstanceGeometryData *geom = memnew(InstanceGeometryData);
            VSG::ecs->registry.emplace_or_replace<GeometryComponent>(instance->self,geom);

            if (instance->base_type == RS::INSTANCE_MESH) {
                instance->blend_values.resize(VSG::storage->mesh_get_blend_shape_count(p_base));
            }
        } break;
        case RS::INSTANCE_REFLECTION_PROBE: {

            auto *reflection_probe = &VSG::ecs->registry.emplace<VisualServerScene::InstanceReflectionProbeData>(instbase);
            reflection_probe->owner = p_instance;

            reflection_probe->instance = VSG::scene_render->reflection_probe_instance_create(p_base);
        } break;
        case RS::INSTANCE_LIGHTMAP_CAPTURE: {
            VSG::ecs->registry.emplace<RenderingInstanceLightmapCaptureDataComponent>(instbase);

                 //lightmap_capture->instance = VSG::scene_render->lightmap_capture_instance_create(p_base);
        } break;
        case RS::INSTANCE_GI_PROBE: {
            VisualServerScene::InstanceGIProbeData *gi_probe = &VSG::ecs->registry.emplace<VisualServerScene::InstanceGIProbeData>(instbase);
            gi_probe->owner = p_instance;

            if (scenario) {
                VSG::ecs->registry.emplace_or_replace<DirtyGIProbe>(instance->self);
            }

            gi_probe->probe_instance = VSG::scene_render->gi_probe_instance_create();

        } break;
        default: {
            VSG::ecs->registry.destroy(instbase);
        }
    }

    VSG::storage->instance_add_dependency(p_base, p_instance);

    instance->base = p_base;

    if (scenario) {
        ::set_instance_dirty(p_instance, true, true);
    }
}

void instance_attach_skeleton(RenderingEntity p_instance, RenderingEntity p_skeleton) {

    RenderingInstanceComponent *instance = VSG::ecs->registry.try_get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);

    if (instance->skeleton == p_skeleton)
        return;

    if (instance->skeleton!=entt::null) {
        VSG::storage->instance_remove_skeleton(instance->skeleton, p_instance);
    }

    instance->skeleton = p_skeleton;

    if (instance->skeleton!=entt::null) {
        VSG::storage->instance_add_skeleton(instance->skeleton, p_instance);
    }

    ::set_instance_dirty(p_instance, true, false);
}

void instance_geometry_set_material_override(RenderingEntity p_instance, RenderingEntity p_material) {
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);

    if (instance->material_override!=entt::null) {
        VSG::storage->material_remove_instance_owner(instance->material_override, p_instance);
    }
    instance->material_override = p_material;
    instance->base_changed(false, true);

    if (instance->material_override!=entt::null) {
        VSG::storage->material_add_instance_owner(instance->material_override, p_instance);
    }
}

void instance_geometry_set_material_overlay(RenderingEntity p_instance, RenderingEntity p_material) {
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);

    if (instance->material_overlay!=entt::null) {
        VSG::storage->material_remove_instance_owner(instance->material_overlay, p_instance);
    }
    instance->material_overlay = p_material;
    instance->base_changed(false, true);

    if (instance->material_overlay != entt::null) {
        VSG::storage->material_add_instance_owner(instance->material_overlay, p_instance);
    }
}
void instance_set_use_lightmap(RenderingEntity p_instance, RenderingEntity p_lightmap_instance, RenderingEntity p_lightmap, int p_lightmap_slice, const Rect2 &p_lightmap_uv_rect) {

    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);

    instance->lightmap = entt::null;
    instance->lightmap_slice = -1;
    instance->lightmap_uv_rect = Rect2(0, 0, 1, 1);
    instance->baked_light = false;

    if (instance->lightmap_capture!=entt::null) {
        auto *lightmap_capture = getUnchecked<RenderingInstanceLightmapCaptureDataComponent>(instance->lightmap_capture);
        lightmap_capture->users.erase(p_instance);
        instance->lightmap_capture = entt::null;
    }

    if (p_lightmap_instance==entt::null) {
        return;
    }
    auto *lightmap_instance = getUnchecked<RenderingInstanceComponent>(p_lightmap_instance);
    ERR_FAIL_COND(!lightmap_instance);
    ERR_FAIL_COND(lightmap_instance->base_type != RS::INSTANCE_LIGHTMAP_CAPTURE);
    instance->lightmap_capture = p_lightmap_instance;

    auto *lightmap_capture = VSG::ecs->try_get<RenderingInstanceLightmapCaptureDataComponent>(instance->lightmap_capture);
    lightmap_capture->users.insert(p_instance);
    instance->lightmap = p_lightmap;
    instance->lightmap_slice = p_lightmap_slice;
    instance->lightmap_uv_rect = p_lightmap_uv_rect;
    instance->baked_light = true;

}


void instance_set_scenario(RenderingEntity p_instance, RenderingEntity p_scenario) {

    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(p_instance);
    ERR_FAIL_COND(!instance);

    instance_clear_scanario(instance);

    if(p_scenario==entt::null) {
        return;
    }

    auto *scenario = get<RenderingScenarioComponent>(p_scenario);
    ERR_FAIL_COND(!scenario);

    instance->scenario = p_scenario;

    scenario->instances.push_back(p_instance);

    switch (instance->base_type) {

        case RS::INSTANCE_LIGHT: {
            auto *light = getUnchecked<VisualServerScene::InstanceLightData>(instance->self);

            if (VSG::storage->light_get_type(instance->base) == RS::LIGHT_DIRECTIONAL) {
                scenario->directional_lights.push_back(p_instance);
                light->D = true;
            }
        } break;
        case RS::INSTANCE_GI_PROBE: {
            VSG::ecs->registry.emplace_or_replace<DirtyGIProbe>(instance->self);
        } break;
        default: {
        }
    }
    // handle occlusion changes if necessary
    _instance_create_occlusion_rep(instance);

    ::set_instance_dirty(instance->self, true, true);

}
RenderingInstanceComponent::RenderingInstanceComponent(RenderingInstanceComponent &&f) : self(entt::null)
{
    *this = eastl::move(f);
}

RenderingInstanceComponent &RenderingInstanceComponent::operator=(RenderingInstanceComponent &&oth)
{
    release_resources();
    assert(this!=&oth);
    if (instance_owner != entt::null && base_type != RenderingServerEnums::INSTANCE_NONE)
    {
        VSG::storage->instance_remove_dependency(instance_owner, self);
    }
    base_type = oth.base_type; oth.base_type = RS::INSTANCE_NONE;
    base = eastl::move(oth.base);
    self = eastl::move(oth.self);
    skeleton = eastl::move(oth.skeleton);
    material_override = eastl::move(oth.material_override);
    material_overlay = eastl::move(oth.material_overlay);
    instance_owner = eastl::move(oth.instance_owner);
    transform = eastl::move(oth.transform);
    depth_layer=oth.depth_layer;
    layer_mask=oth.layer_mask;
    materials = eastl::move(oth.materials);
    light_instances = eastl::move(light_instances);
    reflection_probe_instances = eastl::move(reflection_probe_instances);
    gi_probe_instances = eastl::move(gi_probe_instances);
    blend_values = eastl::move(blend_values);
    cast_shadows = eastl::move(cast_shadows);
    mirror = oth.mirror;
    receive_shadows = oth.receive_shadows;
    visible  = oth.visible;
    baked_light  = oth.baked_light;
    dynamic_gi  = oth.dynamic_gi;
    redraw_if_visible  = oth.redraw_if_visible;
    depth = oth.depth; //used for sorting
    lightmap_capture = eastl::move(oth.lightmap_capture);
    lightmap = eastl::move(oth.lightmap);
    lightmap_capture_data = eastl::move(oth.lightmap_capture_data); //in an array (12 values) to avoid wasting space if unused. Alpha is unused, but needed to send to shader
    scenario = eastl::move(oth.scenario);
    spatial_partition_id = oth.spatial_partition_id;
    object_id = eastl::move(oth.object_id);
    occlusion_handle = eastl::move(oth.occlusion_handle);  oth.occlusion_handle = 0;
    portal_mode = oth.portal_mode; oth.portal_mode = RS::INSTANCE_PORTAL_MODE_STATIC;
    lod_begin = eastl::move(oth.lod_begin);
    lod_end = eastl::move(oth.lod_end);
    lod_begin_hysteresis = eastl::move(oth.lod_begin_hysteresis);
    lod_end_hysteresis = eastl::move(oth.lod_end_hysteresis);
    lod_instance = eastl::move(oth.lod_instance);
    bvh_pairable_mask = oth.bvh_pairable_mask;
    bvh_pairable_type = oth.bvh_pairable_type;
    last_render_pass = eastl::move(oth.last_render_pass);
    last_frame_pass = eastl::move(oth.last_frame_pass);
    version = oth.version;
    return *this;
}
void RenderingInstanceComponent::release_resources() {
    RenderingInstanceLightmapCaptureDataComponent *capture = get<RenderingInstanceLightmapCaptureDataComponent>(lightmap_capture);
    if (capture) {
        capture->users.erase(self);
        lightmap_capture = entt::null;
    }
    instance_clear_scanario(this);
    ::instance_clear_base(this);

    //::instance_geometry_set_material_override(self, entt::null);
    if (material_override!=entt::null) {
        VSG::storage->material_remove_instance_owner(material_override, self);
    }
    material_override = entt::null;
    if (material_overlay!=entt::null) {
        VSG::storage->material_remove_instance_owner(material_overlay, self);
    }
    material_overlay = entt::null;

    //::instance_attach_skeleton(self, entt::null);
    if (skeleton!=entt::null) {
        VSG::storage->instance_remove_skeleton(skeleton, self);
    }
    skeleton = entt::null;
    self = entt::null;

}
RenderingInstanceComponent::~RenderingInstanceComponent() {
//    fprintf(stderr,"D:%x\n", entt::to_integral(self.value));
//    fflush(stderr);
    release_resources();
    if(instance_owner!=entt::null && base_type!= RenderingServerEnums::INSTANCE_NONE)
    {
        VSG::storage->instance_remove_dependency(instance_owner, self);
        instance_owner = entt::null;
    }
}
