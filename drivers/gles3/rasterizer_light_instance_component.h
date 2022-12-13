#pragma once

#include "core/engine_entities.h"
#include "core/hash_set.h"
#include "core/math/camera_matrix.h"
#include "core/math/rect2.h"
#include "servers/rendering/render_entity_helpers.h"

class RasterizerSceneGLES3;
struct RasterizerLight3DComponent;

struct RasterizerLightInstanceShadowAtlasesComponent {
    HashSet<RenderingEntity> shadow_atlases {}; //shadow atlases where this light is registered
    MoveOnlyEntityHandle self;

    void unregister_from_atlases() noexcept;

    RasterizerLightInstanceShadowAtlasesComponent(const RasterizerLightInstanceShadowAtlasesComponent &) = delete;
    RasterizerLightInstanceShadowAtlasesComponent &operator=(const RasterizerLightInstanceShadowAtlasesComponent &) = delete;

    RasterizerLightInstanceShadowAtlasesComponent(RasterizerLightInstanceShadowAtlasesComponent &&f) = default;

    RasterizerLightInstanceShadowAtlasesComponent &operator=(RasterizerLightInstanceShadowAtlasesComponent &&f) noexcept {
        unregister_from_atlases();
        // no need to check for self-assignment, if needed it's handled by our member variables.
        shadow_atlases=eastl::move(f.shadow_atlases);
        self = eastl::move(f.self);
        return *this;
    }

    RasterizerLightInstanceShadowAtlasesComponent() = default;
    ~RasterizerLightInstanceShadowAtlasesComponent();

};

struct RasterizerLightInstanceComponent {

    struct ShadowTransform {

        CameraMatrix camera;
        Transform transform;
        float farplane;
        float split;
        float bias_scale;
    };

    ShadowTransform shadow_transform[4];

    MoveOnlyEntityHandle light = entt::null; //RasterizerLight3DComponent *light_ptr;
    Transform transform;

    Vector3 light_vector;
    Vector3 spot_vector;
    float linear_att;

    uint64_t shadow_pass;
    uint64_t last_scene_pass;
    uint64_t last_scene_shadow_pass;
    uint64_t last_pass;
    uint16_t light_index;
    uint16_t light_directional_index;

    uint32_t current_shadow_atlas_key;

    Vector2 dp;

    Rect2 directional_rect;

    RasterizerLightInstanceComponent(const RasterizerLightInstanceComponent &) = delete;
    RasterizerLightInstanceComponent &operator=(const RasterizerLightInstanceComponent &) = delete;

    RasterizerLightInstanceComponent(RasterizerLightInstanceComponent &&f) noexcept {
        memcpy(this,&f,sizeof(RasterizerLightInstanceComponent));
        memset(&f,0,sizeof(RasterizerLightInstanceComponent));
        f.light = entt::null;
    }
    RasterizerLightInstanceComponent &operator=(RasterizerLightInstanceComponent &&f) noexcept {
        if(this!=&f) {
            memcpy(this, &f, sizeof(RasterizerLightInstanceComponent));
        }
        // if this==&f  this will clean-up this object
        memset(&f,0,sizeof(RasterizerLightInstanceComponent));
        f.light = entt::null;
        return *this;
    }

    RasterizerLightInstanceComponent() = default;
};
struct LightDataUBO {

    float light_pos_inv_radius[4];
    float light_direction_attenuation[4];
    float light_color_energy[4];
    float light_params[4]; //spot attenuation, spot angle, specular, shadow enabled
    float light_clamp[4];
    float light_shadow_color_contact[4];
    union {
        struct {
            float matrix1[16]; //up to here for spot and omni, rest is for directional
            float matrix2[16];
            float matrix3[16];
            float matrix4[16];
        };
        float matrix[4 * 16];
    } shadow;
    float shadow_split_offsets[4];
};

//RenderingEntity light_instance_create(RenderingEntity p_light);
//void light_instance_set_transform(RenderingEntity p_light_instance, const Transform &p_transform);
//void light_instance_set_shadow_transform(RenderingEntity p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale);
//void light_instance_mark_visible(RenderingEntity p_light_instance,uint64_t scene_pass);


void _setup_directional_light(RasterizerSceneGLES3 *scene,int p_index, const Transform &p_camera_inverse_transform, bool p_use_shadows);
void _setup_lights(RasterizerSceneGLES3 *self,RenderingEntity *p_light_cull_result, int p_light_cull_count, const Transform &p_camera_inverse_transform, const CameraMatrix &p_camera_projection, RenderingEntity p_shadow_atlas);
