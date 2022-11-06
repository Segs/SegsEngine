#include "rasterizer_light_instance_component.h"


#include "drivers/gles3/rasterizer_canvas_gles3.h"
#include "drivers/gles3/rasterizer_shadow_atlas_component.h"
#include "rasterizer_light3d_component.h"
#include "rasterizer_scene_gles3.h"
#include "servers/rendering/render_entity_getter.h"
#include "core/external_profiler.h"


RenderingEntity RasterizerSceneGLES3::light_instance_create(RenderingEntity p_light) {

    auto res = VSG::ecs->create();
    auto &light_instance(VSG::ecs->registry.emplace<RasterizerLightInstanceComponent>(res));

    light_instance.last_pass = 0;
    light_instance.last_scene_pass = 0;
    light_instance.last_scene_shadow_pass = 0;

    light_instance.light = p_light;
    auto *light_ptr = get<RasterizerLight3DComponent>(p_light);

    if (!light_ptr) {
        VSG::ecs->registry.destroy(res);
        ERR_FAIL_V_MSG(entt::null, "Condition ' !light_ptr ' is true.");
    }

    auto &light_atlases(VSG::ecs->registry.emplace<RasterizerLightInstanceShadowAtlasesComponent>(res));
    light_atlases.self = res;

    return res;
}

void RasterizerSceneGLES3::light_instance_set_transform(RenderingEntity p_light_instance, const Transform &p_transform) {

    auto *light_instance = get<RasterizerLightInstanceComponent>(p_light_instance);
    ERR_FAIL_COND(!light_instance);

    light_instance->transform = p_transform;
}

void RasterizerSceneGLES3::light_instance_set_shadow_transform(RenderingEntity p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale) {


    auto *light_instance = get<RasterizerLightInstanceComponent>(p_light_instance);
    auto *light_ptr = get<RasterizerLight3DComponent>(light_instance->light);
    ERR_FAIL_COND(!light_instance || !light_ptr);
    if (light_ptr->type != RS::LIGHT_DIRECTIONAL) {
        p_pass = 0;
    }

    ERR_FAIL_INDEX(p_pass, 4);

    light_instance->shadow_transform[p_pass].camera = p_projection;
    light_instance->shadow_transform[p_pass].transform = p_transform;
    light_instance->shadow_transform[p_pass].farplane = p_far;
    light_instance->shadow_transform[p_pass].split = p_split;
    light_instance->shadow_transform[p_pass].bias_scale = p_bias_scale;
}

void RasterizerSceneGLES3::light_instance_mark_visible(RenderingEntity p_light_instance) {

    auto *light_instance = get<RasterizerLightInstanceComponent>(p_light_instance);
    ERR_FAIL_COND(!light_instance);

    light_instance->last_scene_pass = scene_pass;
}

//////////////////////

void _setup_directional_light(RasterizerSceneGLES3 *self, int p_index, const Transform &p_camera_inverse_transform, bool p_use_shadows) {

    RasterizerLightInstanceComponent *li = self->directional_lights[p_index];
    auto *light_ptr = VSG::ecs->registry.try_get<RasterizerLight3DComponent>(li->light);
    ERR_FAIL_COND(!light_ptr);

    LightDataUBO ubo_data; //used for filling

    float sign = light_ptr->negative ? -1 : 1;

    Color linear_col = light_ptr->color.to_linear();
    //compensate normalized diffuse range by multiplying by PI
    ubo_data.light_color_energy[0] = linear_col.r * sign * light_ptr->param[RS::LIGHT_PARAM_ENERGY] * Math_PI;
    ubo_data.light_color_energy[1] = linear_col.g * sign * light_ptr->param[RS::LIGHT_PARAM_ENERGY] * Math_PI;
    ubo_data.light_color_energy[2] = linear_col.b * sign * light_ptr->param[RS::LIGHT_PARAM_ENERGY] * Math_PI;
    ubo_data.light_color_energy[3] = 0;

         //omni, keep at 0
    ubo_data.light_pos_inv_radius[0] = 0.0;
    ubo_data.light_pos_inv_radius[1] = 0.0;
    ubo_data.light_pos_inv_radius[2] = 0.0;
    ubo_data.light_pos_inv_radius[3] = 0.0;

    Vector3 direction = p_camera_inverse_transform.basis.xform(li->transform.basis.xform(Vector3(0, 0, -1))).normalized();
    ubo_data.light_direction_attenuation[0] = direction.x;
    ubo_data.light_direction_attenuation[1] = direction.y;
    ubo_data.light_direction_attenuation[2] = direction.z;
    ubo_data.light_direction_attenuation[3] = 1.0;

    ubo_data.light_params[0] = 0;
    ubo_data.light_params[1] = 0;
    ubo_data.light_params[2] = light_ptr->param[RS::LIGHT_PARAM_SPECULAR];
    ubo_data.light_params[3] = 0;

    Color shadow_color = light_ptr->shadow_color.to_linear();
    ubo_data.light_shadow_color_contact[0] = shadow_color.r;
    ubo_data.light_shadow_color_contact[1] = shadow_color.g;
    ubo_data.light_shadow_color_contact[2] = shadow_color.b;
    ubo_data.light_shadow_color_contact[3] = light_ptr->param[RS::LIGHT_PARAM_CONTACT_SHADOW_SIZE];

    if (p_use_shadows && light_ptr->shadow) {

        int shadow_count = 0;

        switch (light_ptr->directional_shadow_mode) {
            case RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL: {
                shadow_count = 1;
            } break;
            case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS: {
                shadow_count = 2;
            } break;
            case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS: {
                shadow_count = 4;
            } break;
        }

        for (int j = 0; j < shadow_count; j++) {

            uint32_t x = li->directional_rect.position.x;
            uint32_t y = li->directional_rect.position.y;
            uint32_t width = li->directional_rect.size.x;
            uint32_t height = li->directional_rect.size.y;

            if (light_ptr->directional_shadow_mode == RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS) {

                width /= 2;
                height /= 2;

                if (j == 1) {
                    x += width;
                } else if (j == 2) {
                    y += height;
                } else if (j == 3) {
                    x += width;
                    y += height;
                }

            } else if (light_ptr->directional_shadow_mode == RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS) {

                height /= 2;

                if (j != 0) {
                    y += height;
                }
            }

            ubo_data.shadow_split_offsets[j] = li->shadow_transform[j].split;

            Transform modelview = (p_camera_inverse_transform * li->shadow_transform[j].transform).affine_inverse();

            CameraMatrix bias;
            bias.set_light_bias();
            CameraMatrix rectm;
            Rect2 atlas_rect = Rect2(float(x) / self->directional_shadow.size, float(y) / self->directional_shadow.size,
                    float(width) / self->directional_shadow.size, float(height) / self->directional_shadow.size);
            rectm.set_light_atlas_rect(atlas_rect);

            CameraMatrix shadow_mtx = rectm * bias * li->shadow_transform[j].camera * modelview;

            store_camera(shadow_mtx, &ubo_data.shadow.matrix[16 * j]);

            ubo_data.light_clamp[0] = atlas_rect.position.x;
            ubo_data.light_clamp[1] = atlas_rect.position.y;
            ubo_data.light_clamp[2] = atlas_rect.size.x;
            ubo_data.light_clamp[3] = atlas_rect.size.y;
        }
    }

    glBindBuffer(GL_UNIFORM_BUFFER, self->state.directional_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(LightDataUBO), &ubo_data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    self->directional_light = li;

    glBindBufferBase(GL_UNIFORM_BUFFER, 3, self->state.directional_ubo);
}

void _setup_lights(RasterizerSceneGLES3 *self, RenderingEntity *p_light_cull_result, int p_light_cull_count,
        const Transform &p_camera_inverse_transform, const CameraMatrix &p_camera_projection,
        RenderingEntity p_shadow_atlas) {
    SCOPE_AUTONAMED

    self->state.omni_light_count = 0;
    self->state.spot_light_count = 0;
    self->state.directional_light_count = 0;

    self->directional_light = nullptr;

    RasterizerShadowAtlasComponent *shadow_atlas = get<RasterizerShadowAtlasComponent>(p_shadow_atlas);

    for (int i = 0; i < p_light_cull_count; i++) {
        ERR_BREAK(i >= self->render_list.max_lights);

        auto culled_light_ent=p_light_cull_result[i];
        auto *li = getUnchecked<RasterizerLightInstanceComponent>(culled_light_ent);
        auto *light_ptr = VSG::ecs->registry.try_get<RasterizerLight3DComponent>(li->light);
        ERR_FAIL_COND(!light_ptr);
        LightDataUBO ubo_data; //used for filling

        switch (light_ptr->type) {

            case RS::LIGHT_DIRECTIONAL: {

                if (self->state.directional_light_count < RenderListConstants::MAX_DIRECTIONAL_LIGHTS) {
                    self->directional_lights[self->state.directional_light_count++] = li;
                }

            } break;
            case RS::LIGHT_OMNI: {
                ERR_BREAK(self->state.omni_light_count >= self->state.max_ubo_lights);
                float sign = light_ptr->negative ? -1 : 1;

                Color linear_col = light_ptr->color.to_linear();
                ubo_data.light_color_energy[0] = linear_col.r * sign * light_ptr->param[RS::LIGHT_PARAM_ENERGY] * Math_PI;
                ubo_data.light_color_energy[1] = linear_col.g * sign * light_ptr->param[RS::LIGHT_PARAM_ENERGY] * Math_PI;
                ubo_data.light_color_energy[2] = linear_col.b * sign * light_ptr->param[RS::LIGHT_PARAM_ENERGY] * Math_PI;
                ubo_data.light_color_energy[3] = 0;

                Vector3 pos = p_camera_inverse_transform.xform(li->transform.origin);

                     //directional, keep at 0
                ubo_data.light_pos_inv_radius[0] = pos.x;
                ubo_data.light_pos_inv_radius[1] = pos.y;
                ubo_data.light_pos_inv_radius[2] = pos.z;
                ubo_data.light_pos_inv_radius[3] = 1.0f / M_MAX(0.001f, light_ptr->param[RS::LIGHT_PARAM_RANGE]);

                ubo_data.light_direction_attenuation[0] = 0;
                ubo_data.light_direction_attenuation[1] = 0;
                ubo_data.light_direction_attenuation[2] = 0;
                ubo_data.light_direction_attenuation[3] = light_ptr->param[RS::LIGHT_PARAM_ATTENUATION];

                ubo_data.light_params[0] = 0;
                ubo_data.light_params[1] = 0;
                ubo_data.light_params[2] = light_ptr->param[RS::LIGHT_PARAM_SPECULAR];
                ubo_data.light_params[3] = 0;

                Color shadow_color = light_ptr->shadow_color.to_linear();
                ubo_data.light_shadow_color_contact[0] = shadow_color.r;
                ubo_data.light_shadow_color_contact[1] = shadow_color.g;
                ubo_data.light_shadow_color_contact[2] = shadow_color.b;
                ubo_data.light_shadow_color_contact[3] = light_ptr->param[RS::LIGHT_PARAM_CONTACT_SHADOW_SIZE];

                if (light_ptr->shadow && shadow_atlas && shadow_atlas->shadow_owners.contains(culled_light_ent)) {
                    // fill in the shadow information

                    uint32_t key = shadow_atlas->shadow_owners[culled_light_ent];

                    uint32_t quadrant = (key >> RasterizerShadowAtlasComponent::QUADRANT_SHIFT) & 0x3;
                    uint32_t shadow = key & RasterizerShadowAtlasComponent::SHADOW_INDEX_MASK;

                    ERR_CONTINUE(shadow >= (uint32_t)shadow_atlas->quadrants[quadrant].shadows.size());

                    uint32_t atlas_size = shadow_atlas->size;
                    uint32_t quadrant_size = atlas_size >> 1;

                    uint32_t x = (quadrant & 1) * quadrant_size;
                    uint32_t y = (quadrant >> 1) * quadrant_size;

                    uint32_t shadow_size = (quadrant_size / shadow_atlas->quadrants[quadrant].subdivision);
                    x += (shadow % shadow_atlas->quadrants[quadrant].subdivision) * shadow_size;
                    y += (shadow / shadow_atlas->quadrants[quadrant].subdivision) * shadow_size;

                    uint32_t width = shadow_size;
                    uint32_t height = shadow_size;

                    if (light_ptr->omni_shadow_detail == RS::LIGHT_OMNI_SHADOW_DETAIL_HORIZONTAL) {

                        height /= 2;
                    } else {
                        width /= 2;
                    }

                    Transform proj = (p_camera_inverse_transform * li->transform).inverse();

                    store_transform(proj, ubo_data.shadow.matrix1);

                    ubo_data.light_params[3] = 1.0; //means it has shadow
                    ubo_data.light_clamp[0] = float(x) / atlas_size;
                    ubo_data.light_clamp[1] = float(y) / atlas_size;
                    ubo_data.light_clamp[2] = float(width) / atlas_size;
                    ubo_data.light_clamp[3] = float(height) / atlas_size;
                }

                li->light_index = self->state.omni_light_count;
                memcpy(&self->state.omni_array_tmp[li->light_index * self->state.ubo_light_size], &ubo_data,
                        self->state.ubo_light_size);
                self->state.omni_light_count++;

            } break;
            case RS::LIGHT_SPOT: {
                ERR_BREAK(self->state.spot_light_count >= self->state.max_ubo_lights);

                float sign = light_ptr->negative ? -1 : 1;

                Color linear_col = light_ptr->color.to_linear();
                ubo_data.light_color_energy[0] = linear_col.r * sign * light_ptr->param[RS::LIGHT_PARAM_ENERGY] * Math_PI;
                ubo_data.light_color_energy[1] = linear_col.g * sign * light_ptr->param[RS::LIGHT_PARAM_ENERGY] * Math_PI;
                ubo_data.light_color_energy[2] = linear_col.b * sign * light_ptr->param[RS::LIGHT_PARAM_ENERGY] * Math_PI;
                ubo_data.light_color_energy[3] = 0;

                Vector3 pos = p_camera_inverse_transform.xform(li->transform.origin);

                     //directional, keep at 0
                ubo_data.light_pos_inv_radius[0] = pos.x;
                ubo_data.light_pos_inv_radius[1] = pos.y;
                ubo_data.light_pos_inv_radius[2] = pos.z;
                ubo_data.light_pos_inv_radius[3] = 1.0f / M_MAX(0.001f, light_ptr->param[RS::LIGHT_PARAM_RANGE]);

                Vector3 direction = p_camera_inverse_transform.basis.xform(li->transform.basis.xform(Vector3(0, 0, -1))).normalized();
                ubo_data.light_direction_attenuation[0] = direction.x;
                ubo_data.light_direction_attenuation[1] = direction.y;
                ubo_data.light_direction_attenuation[2] = direction.z;
                ubo_data.light_direction_attenuation[3] = light_ptr->param[RS::LIGHT_PARAM_ATTENUATION];

                ubo_data.light_params[0] = light_ptr->param[RS::LIGHT_PARAM_SPOT_ATTENUATION];
                ubo_data.light_params[1] = Math::cos(Math::deg2rad(light_ptr->param[RS::LIGHT_PARAM_SPOT_ANGLE]));
                ubo_data.light_params[2] = light_ptr->param[RS::LIGHT_PARAM_SPECULAR];
                ubo_data.light_params[3] = 0;

                Color shadow_color = light_ptr->shadow_color.to_linear();
                ubo_data.light_shadow_color_contact[0] = shadow_color.r;
                ubo_data.light_shadow_color_contact[1] = shadow_color.g;
                ubo_data.light_shadow_color_contact[2] = shadow_color.b;
                ubo_data.light_shadow_color_contact[3] = light_ptr->param[RS::LIGHT_PARAM_CONTACT_SHADOW_SIZE];

                if (light_ptr->shadow && shadow_atlas && shadow_atlas->shadow_owners.contains(culled_light_ent)) {
                    // fill in the shadow information

                    uint32_t key = shadow_atlas->shadow_owners[culled_light_ent];

                    uint32_t quadrant = (key >> RasterizerShadowAtlasComponent::QUADRANT_SHIFT) & 0x3;
                    uint32_t shadow = key & RasterizerShadowAtlasComponent::SHADOW_INDEX_MASK;

                    ERR_CONTINUE(shadow >= (uint32_t)shadow_atlas->quadrants[quadrant].shadows.size());

                    uint32_t atlas_size = shadow_atlas->size;
                    uint32_t quadrant_size = atlas_size >> 1;

                    uint32_t x = (quadrant & 1) * quadrant_size;
                    uint32_t y = (quadrant >> 1) * quadrant_size;

                    uint32_t shadow_size = (quadrant_size / shadow_atlas->quadrants[quadrant].subdivision);
                    x += (shadow % shadow_atlas->quadrants[quadrant].subdivision) * shadow_size;
                    y += (shadow / shadow_atlas->quadrants[quadrant].subdivision) * shadow_size;

                    uint32_t width = shadow_size;
                    uint32_t height = shadow_size;

                    Rect2 rect(float(x) / atlas_size, float(y) / atlas_size, float(width) / atlas_size, float(height) / atlas_size);

                    ubo_data.light_params[3] = 1.0; //means it has shadow
                    ubo_data.light_clamp[0] = rect.position.x;
                    ubo_data.light_clamp[1] = rect.position.y;
                    ubo_data.light_clamp[2] = rect.size.x;
                    ubo_data.light_clamp[3] = rect.size.y;

                    Transform modelview = (p_camera_inverse_transform * li->transform).inverse();

                    CameraMatrix bias;
                    bias.set_light_bias();
                    CameraMatrix rectm;
                    rectm.set_light_atlas_rect(rect);

                    CameraMatrix shadow_mtx = rectm * bias * li->shadow_transform[0].camera * modelview;

                    store_camera(shadow_mtx, ubo_data.shadow.matrix1);
                }

                li->light_index = self->state.spot_light_count;
                memcpy(&self->state.spot_array_tmp[li->light_index * self->state.ubo_light_size], &ubo_data,
                        self->state.ubo_light_size);
                self->state.spot_light_count++;

            } break;
        }

        li->last_pass = self->render_pass;

    }

    //update UBO for forward rendering, blit to texture for clustered
    if (self->state.omni_light_count) {

        glBindBuffer(GL_UNIFORM_BUFFER, self->state.omni_array_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, self->state.omni_light_count * self->state.ubo_light_size,
                self->state.omni_array_tmp);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, 4, self->state.omni_array_ubo);

    if (self->state.spot_light_count) {

        glBindBuffer(GL_UNIFORM_BUFFER, self->state.spot_array_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, self->state.spot_light_count * self->state.ubo_light_size,
                self->state.spot_array_tmp);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, 5, self->state.spot_array_ubo);
}



int RasterizerSceneGLES3::get_directional_light_shadow_size(RenderingEntity p_light_intance) {

    ERR_FAIL_COND_V(directional_shadow.light_count == 0, 0);

    int shadow_size;

    if (directional_shadow.light_count == 1) {
        shadow_size = directional_shadow.size;
    } else {
        shadow_size = directional_shadow.size / 2; //more than 4 not supported anyway
    }

    RasterizerLightInstanceComponent *light_instance = get<RasterizerLightInstanceComponent>(p_light_intance);
    ERR_FAIL_COND_V(!light_instance, 0);
    RasterizerLight3DComponent *light_ptr = get<RasterizerLight3DComponent>(light_instance->light);
    ERR_FAIL_COND_V(!light_ptr, 0);
    switch (light_ptr->directional_shadow_mode) {
        case RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL:
            break; //none
        case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS:
        case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS: shadow_size /= 2; break;
    }

    return shadow_size;
}

//!remove self from shadow atlases..
void RasterizerLightInstanceShadowAtlasesComponent::unregister_from_atlases() noexcept {
    for (RenderingEntity E : shadow_atlases) {
        auto *shadow_atlas = get<RasterizerShadowAtlasComponent>(E);
        auto iter = shadow_atlas->shadow_owners.find(self);
        ERR_CONTINUE(iter==shadow_atlas->shadow_owners.end());
        uint32_t key = shadow_atlas->shadow_owners[self];
        uint32_t q = (key >> RasterizerShadowAtlasComponent::QUADRANT_SHIFT) & 0x3;
        uint32_t s = key & RasterizerShadowAtlasComponent::SHADOW_INDEX_MASK;

        shadow_atlas->quadrants[q].shadows[s].owner = entt::null;
        shadow_atlas->shadow_owners.erase(iter);
    }
    shadow_atlases.clear();
}

RasterizerLightInstanceShadowAtlasesComponent::~RasterizerLightInstanceShadowAtlasesComponent()
{
    unregister_from_atlases();
}
