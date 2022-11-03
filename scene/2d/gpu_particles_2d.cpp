/*************************************************************************/
/*  gpu_particles_2d.cpp                                                 */
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

#include "gpu_particles_2d.h"

#include "core/ecs_registry.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/os.h"
#include "core/translation_helpers.h"
#include "scene/2d/canvas_item_material.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/particles_material.h"
#include "scene/scene_string_names.h"
#include "servers/rendering_server.h"

#ifdef TOOLS_ENABLED
#include "core/engine.h"
#endif

#include "entt/entity/registry.hpp"

extern ECS_Registry<GameEntity, true> game_object_registry;
IMPL_GDCLASS(GPUParticles2D)
VARIANT_ENUM_CAST(GPUParticles2D::DrawOrder);

struct ParticleVisibilityEditor {
    bool visbility_rect;
};
namespace GpuParticle2D_Tools {
void set_show_visibility_rect(GPUParticles2D *part, bool show_hide) {
    game_object_registry.registry.get_or_emplace<ParticleVisibilityEditor>(part->get_instance_id()).visbility_rect =
            show_hide;
    part->update();
}
} // namespace GpuParicles2D_Tools
void GPUParticles2D::set_emitting(bool p_emitting) {

    RenderingServer::get_singleton()->particles_set_emitting(particles, p_emitting);

    if (p_emitting && one_shot) {
        set_process_internal(true);
    } else if (!p_emitting) {
        set_process_internal(false);
    }
}

void GPUParticles2D::set_amount(int p_amount) {

    ERR_FAIL_COND_MSG(p_amount < 1, "Amount of particles cannot be smaller than 1.");
    amount = p_amount;
    RenderingServer::get_singleton()->particles_set_amount(particles, amount);
}
void GPUParticles2D::set_lifetime(float p_lifetime) {

    ERR_FAIL_COND_MSG(p_lifetime <= 0, "Particles lifetime must be greater than 0.");
    lifetime = p_lifetime;
    RenderingServer::get_singleton()->particles_set_lifetime(particles, lifetime);
}

void GPUParticles2D::set_one_shot(bool p_enable) {

    one_shot = p_enable;
    RenderingServer::get_singleton()->particles_set_one_shot(particles, one_shot);

    if (is_emitting()) {

        set_process_internal(true);
        if (!one_shot)
            RenderingServer::get_singleton()->particles_restart(particles);
    }

    if (!one_shot)
        set_process_internal(false);
}
void GPUParticles2D::set_pre_process_time(float p_time) {

    pre_process_time = p_time;
    RenderingServer::get_singleton()->particles_set_pre_process_time(particles, pre_process_time);
}
void GPUParticles2D::set_explosiveness_ratio(float p_ratio) {

    explosiveness_ratio = p_ratio;
    RenderingServer::get_singleton()->particles_set_explosiveness_ratio(particles, explosiveness_ratio);
}
void GPUParticles2D::set_randomness_ratio(float p_ratio) {

    randomness_ratio = p_ratio;
    RenderingServer::get_singleton()->particles_set_randomness_ratio(particles, randomness_ratio);
}
void GPUParticles2D::set_visibility_rect(const Rect2 &p_visibility_rect) {

    visibility_rect = p_visibility_rect;
    AABB aabb;
    aabb.position.x = p_visibility_rect.position.x;
    aabb.position.y = p_visibility_rect.position.y;
    aabb.size.x = p_visibility_rect.size.x;
    aabb.size.y = p_visibility_rect.size.y;

    RenderingServer::get_singleton()->particles_set_custom_aabb(particles, aabb);

    Object_change_notify(this,"visibility_rect");
    update();
}
void GPUParticles2D::set_use_local_coordinates(bool p_enable) {

    local_coords = p_enable;
    RenderingServer::get_singleton()->particles_set_use_local_coordinates(particles, local_coords);
    set_notify_transform(!p_enable);
    if (!p_enable && is_inside_tree()) {
        _update_particle_emission_transform();
    }
}

void GPUParticles2D::_update_particle_emission_transform() {

    Transform2D xf2d = get_global_transform();
    Transform xf;
    xf.basis.set_axis(0, Vector3(xf2d.get_axis(0).x, xf2d.get_axis(0).y, 0));
    xf.basis.set_axis(1, Vector3(xf2d.get_axis(1).x, xf2d.get_axis(1).y, 0));
    xf.set_origin(Vector3(xf2d.get_origin().x, xf2d.get_origin().y, 0));

    RenderingServer::get_singleton()->particles_set_emission_transform(particles, xf);
}

void GPUParticles2D::set_process_material(const Ref<Material> &p_material) {

    process_material = p_material;
    Ref<ParticlesMaterial> pm = dynamic_ref_cast<ParticlesMaterial>(p_material);
    if (pm && !pm->get_flag(ParticlesMaterial::FLAG_DISABLE_Z) && pm->get_gravity() == Vector3(0, -9.8, 0)) {
        // Likely a new (3D) material, modify it to match 2D space
        pm->set_flag(ParticlesMaterial::FLAG_DISABLE_Z, true);
        pm->set_gravity(Vector3(0, 98, 0));
    }
    RenderingEntity material_rid = entt::null;
    if (process_material)
        material_rid = process_material->get_rid();
    RenderingServer::get_singleton()->particles_set_process_material(particles, material_rid);

    update_configuration_warning();
}

void GPUParticles2D::set_speed_scale(float p_scale) {

    speed_scale = p_scale;
    RenderingServer::get_singleton()->particles_set_speed_scale(particles, p_scale);
}

bool GPUParticles2D::is_emitting() const {

    return RenderingServer::get_singleton()->particles_get_emitting(particles);
}
int GPUParticles2D::get_amount() const {

    return amount;
}
float GPUParticles2D::get_lifetime() const {

    return lifetime;
}

bool GPUParticles2D::get_one_shot() const {

    return one_shot;
}
float GPUParticles2D::get_pre_process_time() const {

    return pre_process_time;
}


Rect2 GPUParticles2D::get_visibility_rect() const {

    return visibility_rect;
}

Ref<Material> GPUParticles2D::get_process_material() const {

    return process_material;
}

void GPUParticles2D::set_draw_order(DrawOrder p_order) {

    draw_order = p_order;
    RenderingServer::get_singleton()->particles_set_draw_order(particles, RS::ParticlesDrawOrder(p_order));
}

GPUParticles2D::DrawOrder GPUParticles2D::get_draw_order() const {

    return draw_order;
}

void GPUParticles2D::set_fixed_fps(int p_count) {
    fixed_fps = p_count;
    RenderingServer::get_singleton()->particles_set_fixed_fps(particles, p_count);
}

int GPUParticles2D::get_fixed_fps() const {
    return fixed_fps;
}

void GPUParticles2D::set_fractional_delta(bool p_enable) {
    fractional_delta = p_enable;
    RenderingServer::get_singleton()->particles_set_fractional_delta(particles, p_enable);
}

bool GPUParticles2D::get_fractional_delta() const {
    return fractional_delta;
}

String GPUParticles2D::get_configuration_warning() const {

    String warning = BaseClassName::get_configuration_warning();

#ifdef OSX_ENABLED
    if (!warning.empty()) {
        warning += "\n\n";
    }

    warning += "- " +
               TTR("On macOS, Particles2D rendering is much slower than CPUParticles2D due to transform feedback being "
                   "implemented on the CPU instead of the GPU.\nConsider using CPUParticles2D instead when targeting "
                   "macOS.\nYou can use the \"Convert to CPUParticles2D\" toolbar option for this purpose.");
#endif
    if (!process_material) {
        if (!warning.empty())
            warning += "\n\n";
        warning +=
                String("- ") + TTR("A material to process the particles is not assigned, so no behavior is imprinted.");
    } else {

        CanvasItemMaterial *mat = object_cast<CanvasItemMaterial>(get_material().get());

        if (!get_material() || (mat && !mat->get_particles_animation())) {
            const ParticlesMaterial *process = object_cast<ParticlesMaterial>(process_material.get());
            if (process && ((float)process->get_param(ParticlesMaterial::PARAM_ANIM_SPEED) != 0.0 ||
                     (float)process->get_param(ParticlesMaterial::PARAM_ANIM_OFFSET) != 0.0 ||
                            process->get_param_texture(ParticlesMaterial::PARAM_ANIM_SPEED) ||
                            process->get_param_texture(ParticlesMaterial::PARAM_ANIM_OFFSET))) {
                if (warning != String())
                    warning += "\n\n";
                warning += String("- ") + TTR("Particles2D animation requires the usage of a CanvasItemMaterial with "
                                              "\"Particles Animation\" enabled.");
            }
        }
    }

    return warning;
}

Rect2 GPUParticles2D::capture_rect() const {

    AABB aabb = RenderingServer::get_singleton()->particles_get_current_aabb(particles);
    Rect2 r;
    r.position.x = aabb.position.x;
    r.position.y = aabb.position.y;
    r.size.x = aabb.size.x;
    r.size.y = aabb.size.y;
    return r;
}

void GPUParticles2D::set_texture(const Ref<Texture> &p_texture) {
    texture = p_texture;
    update();
}

Ref<Texture> GPUParticles2D::get_texture() const {
    return texture;
}

void GPUParticles2D::set_normal_map(const Ref<Texture> &p_normal_map) {

    normal_map = p_normal_map;
    update();
}

Ref<Texture> GPUParticles2D::get_normal_map() const {
    return normal_map;
}

void GPUParticles2D::_validate_property(PropertyInfo &property) const {}

void GPUParticles2D::restart() {
    RenderingServer::get_singleton()->particles_restart(particles);
    RenderingServer::get_singleton()->particles_set_emitting(particles, true);
}

void GPUParticles2D::_notification(int p_what) {

    if (p_what == NOTIFICATION_DRAW) {
        RenderingEntity texture_rid = texture ? texture->get_rid() : entt::null;
        RenderingEntity normal_rid = normal_map ? normal_map->get_rid() : entt::null;

        RenderingServer::get_singleton()->canvas_item_add_particles(
                get_canvas_item(), particles, texture_rid, normal_rid);

#ifdef TOOLS_ENABLED
        auto *editing_visibility = game_object_registry.try_get<ParticleVisibilityEditor>(get_instance_id());
        if (editing_visibility && editing_visibility->visbility_rect) {
            draw_rect_stroke(visibility_rect, Color(0, 0.7f, 0.9f, 0.4f));
        }
#endif
    }

    if (p_what == NOTIFICATION_PAUSED || p_what == NOTIFICATION_UNPAUSED) {
        if (can_process()) {
            RenderingServer::get_singleton()->particles_set_speed_scale(particles, speed_scale);
        } else {

            RenderingServer::get_singleton()->particles_set_speed_scale(particles, 0);
        }
    }

    if (p_what == NOTIFICATION_TRANSFORM_CHANGED) {
        _update_particle_emission_transform();
    }

    if (p_what == NOTIFICATION_INTERNAL_PROCESS) {

        if (one_shot && !is_emitting()) {
            Object_change_notify(this);
            set_process_internal(false);
        }
    }
}

void GPUParticles2D::_bind_methods() {

    SE_BIND_METHOD(GPUParticles2D,set_emitting);
    SE_BIND_METHOD(GPUParticles2D,set_amount);
    SE_BIND_METHOD(GPUParticles2D,set_lifetime);
    SE_BIND_METHOD(GPUParticles2D,set_one_shot);
    SE_BIND_METHOD(GPUParticles2D,set_pre_process_time);
    MethodBinder::bind_method(
            D_METHOD("set_explosiveness_ratio", { "ratio" }), &GPUParticles2D::set_explosiveness_ratio);
    SE_BIND_METHOD(GPUParticles2D,set_randomness_ratio);
    MethodBinder::bind_method(
            D_METHOD("set_visibility_rect", { "visibility_rect" }), &GPUParticles2D::set_visibility_rect);
    MethodBinder::bind_method(
            D_METHOD("set_use_local_coordinates", { "enable" }), &GPUParticles2D::set_use_local_coordinates);
    SE_BIND_METHOD(GPUParticles2D,set_fixed_fps);
    SE_BIND_METHOD(GPUParticles2D,set_fractional_delta);
    SE_BIND_METHOD(GPUParticles2D,set_process_material);
    SE_BIND_METHOD(GPUParticles2D,set_speed_scale);

    SE_BIND_METHOD(GPUParticles2D,is_emitting);
    SE_BIND_METHOD(GPUParticles2D,get_amount);
    SE_BIND_METHOD(GPUParticles2D,get_lifetime);
    SE_BIND_METHOD(GPUParticles2D,get_one_shot);
    SE_BIND_METHOD(GPUParticles2D,get_pre_process_time);
    SE_BIND_METHOD(GPUParticles2D,get_explosiveness_ratio);
    SE_BIND_METHOD(GPUParticles2D,get_randomness_ratio);
    SE_BIND_METHOD(GPUParticles2D,get_visibility_rect);
    SE_BIND_METHOD(GPUParticles2D,get_use_local_coordinates);
    SE_BIND_METHOD(GPUParticles2D,get_fixed_fps);
    SE_BIND_METHOD(GPUParticles2D,get_fractional_delta);
    SE_BIND_METHOD(GPUParticles2D,get_process_material);
    SE_BIND_METHOD(GPUParticles2D,get_speed_scale);

    SE_BIND_METHOD(GPUParticles2D,set_draw_order);
    SE_BIND_METHOD(GPUParticles2D,get_draw_order);

    SE_BIND_METHOD(GPUParticles2D,set_texture);
    SE_BIND_METHOD(GPUParticles2D,get_texture);

    SE_BIND_METHOD(GPUParticles2D,set_normal_map);
    SE_BIND_METHOD(GPUParticles2D,get_normal_map);

    SE_BIND_METHOD(GPUParticles2D,capture_rect);

    SE_BIND_METHOD(GPUParticles2D,restart);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "emitting"), "set_emitting", "is_emitting");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "amount", PropertyHint::ExpRange, "1,1000000,1"), "set_amount",
            "get_amount");
    ADD_GROUP("Time", "");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "lifetime", PropertyHint::Range, "0.01,600.0,0.01,or_greater"),
            "set_lifetime", "get_lifetime");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "one_shot"), "set_one_shot", "get_one_shot");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "preprocess", PropertyHint::Range, "0.00,600.0,0.01"),
            "set_pre_process_time", "get_pre_process_time");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "speed_scale", PropertyHint::Range, "0,64,0.01"), "set_speed_scale",
            "get_speed_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "explosiveness", PropertyHint::Range, "0,1,0.01"),
            "set_explosiveness_ratio", "get_explosiveness_ratio");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "randomness", PropertyHint::Range, "0,1,0.01"),
            "set_randomness_ratio", "get_randomness_ratio");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "fixed_fps", PropertyHint::Range, "0,1000,1"), "set_fixed_fps",
            "get_fixed_fps");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "fract_delta"), "set_fractional_delta", "get_fractional_delta");
    ADD_GROUP("Drawing", "");
    ADD_PROPERTY(PropertyInfo(VariantType::RECT2, "visibility_rect"), "set_visibility_rect", "get_visibility_rect");
    ADD_PROPERTY(
            PropertyInfo(VariantType::BOOL, "local_coords"), "set_use_local_coordinates", "get_use_local_coordinates");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "draw_order", PropertyHint::Enum, "Index,Lifetime"), "set_draw_order",
            "get_draw_order");
    ADD_GROUP("Process Material", "process_");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "process_material", PropertyHint::ResourceType,
                         "ShaderMaterial,ParticlesMaterial"),
            "set_process_material", "get_process_material");
    ADD_GROUP("Textures", "");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture", PropertyHint::ResourceType, "Texture"), "set_texture",
            "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "normal_map", PropertyHint::ResourceType, "Texture"),
            "set_normal_map", "get_normal_map");

    BIND_ENUM_CONSTANT(DRAW_ORDER_INDEX);
    BIND_ENUM_CONSTANT(DRAW_ORDER_LIFETIME);
}

GPUParticles2D::GPUParticles2D() {

    particles = RenderingServer::get_singleton()->particles_create();

    one_shot = false; // Needed so that set_emitting doesn't access uninitialized values
    set_emitting(true);
    set_one_shot(false);
    set_amount(8);
    set_lifetime(1);
    set_fixed_fps(0);
    set_fractional_delta(true);
    set_pre_process_time(0);
    set_explosiveness_ratio(0);
    set_randomness_ratio(0);
    set_visibility_rect(Rect2(Vector2(-100, -100), Vector2(200, 200)));
    set_use_local_coordinates(true);
    set_draw_order(DRAW_ORDER_INDEX);
    set_speed_scale(1);
}

GPUParticles2D::~GPUParticles2D() {

    RenderingServer::get_singleton()->free_rid(particles);
}
