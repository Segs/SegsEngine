/*************************************************************************/
/*  gpu_particles_3d.cpp                                                        */
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

#include "gpu_particles_3d.h"

#include "core/object_tooling.h"
#include "core/os/os.h"
#include "scene/resources/particles_material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"
#include "core/method_bind.h"
#include "core/translation_helpers.h"

#include "servers/rendering_server.h"

IMPL_GDCLASS(GPUParticles3D)
VARIANT_ENUM_CAST(GPUParticles3D::DrawOrder);

AABB GPUParticles3D::get_aabb() const {

    return AABB();
}
Vector<Face3> GPUParticles3D::get_faces(uint32_t p_usage_flags) const {

    return Vector<Face3>();
}

void GPUParticles3D::set_emitting(bool p_emitting) {

    RenderingServer::get_singleton()->particles_set_emitting(particles, p_emitting);

    if (p_emitting && one_shot) {
        set_process_internal(true);
    } else if (!p_emitting) {
        set_process_internal(false);
    }
}

void GPUParticles3D::set_amount(int p_amount) {

    ERR_FAIL_COND_MSG(p_amount < 1, "Amount of particles cannot be smaller than 1.");
    amount = p_amount;
    RenderingServer::get_singleton()->particles_set_amount(particles, amount);
}
void GPUParticles3D::set_lifetime(float p_lifetime) {

    ERR_FAIL_COND_MSG(p_lifetime <= 0, "Particles lifetime must be greater than 0.");
    lifetime = p_lifetime;
    RenderingServer::get_singleton()->particles_set_lifetime(particles, lifetime);
}

void GPUParticles3D::set_one_shot(bool p_one_shot) {

    one_shot = p_one_shot;
    RenderingServer::get_singleton()->particles_set_one_shot(particles, one_shot);

    if (is_emitting()) {

        set_process_internal(true);
        if (!one_shot)
            RenderingServer::get_singleton()->particles_restart(particles);
    }

    if (!one_shot)
        set_process_internal(false);
}

void GPUParticles3D::set_pre_process_time(float p_time) {

    pre_process_time = p_time;
    RenderingServer::get_singleton()->particles_set_pre_process_time(particles, pre_process_time);
}
void GPUParticles3D::set_explosiveness_ratio(float p_ratio) {

    explosiveness_ratio = p_ratio;
    RenderingServer::get_singleton()->particles_set_explosiveness_ratio(particles, explosiveness_ratio);
}
void GPUParticles3D::set_randomness_ratio(float p_ratio) {

    randomness_ratio = p_ratio;
    RenderingServer::get_singleton()->particles_set_randomness_ratio(particles, randomness_ratio);
}
void GPUParticles3D::set_visibility_aabb(const AABB &p_aabb) {

    visibility_aabb = p_aabb;
    RenderingServer::get_singleton()->particles_set_custom_aabb(particles, visibility_aabb);
    update_gizmo();
    Object_change_notify(this,"visibility_aabb");
}
void GPUParticles3D::set_use_local_coordinates(bool p_enable) {

    local_coords = p_enable;
    RenderingServer::get_singleton()->particles_set_use_local_coordinates(particles, local_coords);
}
void GPUParticles3D::set_process_material(const Ref<Material> &p_material) {

    process_material = p_material;
    RenderingEntity material_rid = process_material ? process_material->get_rid() : entt::null;
    RenderingServer::get_singleton()->particles_set_process_material(particles, material_rid);

    update_configuration_warning();
}

void GPUParticles3D::set_speed_scale(float p_scale) {

    speed_scale = p_scale;
    RenderingServer::get_singleton()->particles_set_speed_scale(particles, p_scale);
}

bool GPUParticles3D::is_emitting() const {

    return RenderingServer::get_singleton()->particles_get_emitting(particles);
}
int GPUParticles3D::get_amount() const {

    return amount;
}
float GPUParticles3D::get_lifetime() const {

    return lifetime;
}
bool GPUParticles3D::get_one_shot() const {

    return one_shot;
}

float GPUParticles3D::get_pre_process_time() const {

    return pre_process_time;
}
float GPUParticles3D::get_explosiveness_ratio() const {

    return explosiveness_ratio;
}
float GPUParticles3D::get_randomness_ratio() const {

    return randomness_ratio;
}
AABB GPUParticles3D::get_visibility_aabb() const {

    return visibility_aabb;
}
bool GPUParticles3D::get_use_local_coordinates() const {

    return local_coords;
}
Ref<Material> GPUParticles3D::get_process_material() const {

    return process_material;
}

float GPUParticles3D::get_speed_scale() const {

    return speed_scale;
}

void GPUParticles3D::set_draw_order(DrawOrder p_order) {

    draw_order = p_order;
    RenderingServer::get_singleton()->particles_set_draw_order(particles, RS::ParticlesDrawOrder(p_order));
}

GPUParticles3D::DrawOrder GPUParticles3D::get_draw_order() const {

    return draw_order;
}

void GPUParticles3D::set_draw_passes(int p_count) {

    ERR_FAIL_COND(p_count < 1);
    draw_passes.resize(p_count);
    RenderingServer::get_singleton()->particles_set_draw_passes(particles, p_count);
    Object_change_notify(this);
}
int GPUParticles3D::get_draw_passes() const {

    return draw_passes.size();
}

void GPUParticles3D::set_draw_pass_mesh(int p_pass, const Ref<Mesh> &p_mesh) {

    ERR_FAIL_INDEX(p_pass, draw_passes.size());

    draw_passes[p_pass] = p_mesh;

    RenderingEntity mesh_rid =  p_mesh ? p_mesh->get_rid() : entt::null;

    RenderingServer::get_singleton()->particles_set_draw_pass_mesh(particles, p_pass, mesh_rid);

    update_configuration_warning();
}

Ref<Mesh> GPUParticles3D::get_draw_pass_mesh(int p_pass) const {

    ERR_FAIL_INDEX_V(p_pass, draw_passes.size(), Ref<Mesh>());

    return draw_passes[p_pass];
}

void GPUParticles3D::set_fixed_fps(int p_count) {
    fixed_fps = p_count;
    RenderingServer::get_singleton()->particles_set_fixed_fps(particles, p_count);
}

int GPUParticles3D::get_fixed_fps() const {
    return fixed_fps;
}

void GPUParticles3D::set_fractional_delta(bool p_enable) {
    fractional_delta = p_enable;
    RenderingServer::get_singleton()->particles_set_fractional_delta(particles, p_enable);
}

bool GPUParticles3D::get_fractional_delta() const {
    return fractional_delta;
}

String GPUParticles3D::get_configuration_warning() const {

    String warnings = BaseClassName::get_configuration_warning();
#ifdef OSX_ENABLED
    if (!warnings.empty()) {
        warnings += "\n\n";
    }

    warnings +=
            "- " + TTR("On macOS, Particles rendering is much slower than CPUParticles due to transform feedback being "
                       "implemented on the CPU instead of the GPU.\nConsider using CPUParticles instead when targeting "
                       "macOS.\nYou can use the \"Convert to CPUParticles\" toolbar option for this purpose.");
#endif

    bool meshes_found = false;
    bool anim_material_found = false;

    for (const Ref<Mesh> & pass : draw_passes) {
        if (!pass) {
            continue;
        }
        Mesh *m = pass.get();
        meshes_found = true;
        for (int j = 0; j < m->get_surface_count(); j++) {
            anim_material_found = object_cast<ShaderMaterial>(m->surface_get_material(j).get()) != nullptr;
            SpatialMaterial *spat = object_cast<SpatialMaterial>(m->surface_get_material(j).get());
            anim_material_found = anim_material_found || (spat && spat->get_billboard_mode() == SpatialMaterial::BILLBOARD_PARTICLES);
        }
        if (anim_material_found) {
            break;
        }
    }

    SpatialMaterial *spat = object_cast<SpatialMaterial>(get_material_override().get());
    anim_material_found = anim_material_found || spat != nullptr;

    if (!meshes_found) {
        if (!warnings.empty())
            warnings += "\n\n";
        warnings += String("- ") + TTRS("Nothing is visible because meshes have not been assigned to draw passes.");
    }

    if (not process_material) {
        if (!warnings.empty())
            warnings += '\n';
        warnings += String("- ") + TTRS("A material to process the particles is not assigned, so no behavior is imprinted.");
    } else {
        anim_material_found = anim_material_found || (spat && spat->get_billboard_mode() == SpatialMaterial::BILLBOARD_PARTICLES);
        const ParticlesMaterial *process = object_cast<ParticlesMaterial>(process_material.get());
        if (!anim_material_found && process &&
                (process->get_param(ParticlesMaterial::PARAM_ANIM_SPEED) != 0.0f || process->get_param(ParticlesMaterial::PARAM_ANIM_OFFSET) != 0.0f ||
                        process->get_param_texture(ParticlesMaterial::PARAM_ANIM_SPEED) || process->get_param_texture(ParticlesMaterial::PARAM_ANIM_OFFSET))) {
            if (!warnings.empty())
                warnings += '\n';
            warnings += String("- ") + TTRS("Particles animation requires the usage of a SpatialMaterial whose Billboard Mode is set to \"Particle Billboard\".");
        }
    }

    return warnings;
}

void GPUParticles3D::restart() {

    RenderingServer::get_singleton()->particles_restart(particles);
    RenderingServer::get_singleton()->particles_set_emitting(particles, true);
}

AABB GPUParticles3D::capture_aabb() const {

    return RenderingServer::get_singleton()->particles_get_current_aabb(particles);
}

void GPUParticles3D::_validate_property(PropertyInfo &property) const {

    if (StringUtils::begins_with(property.name,"draw_pass/")) {
        FixedVector<StringView, 3> parts;
        String::split_ref(parts, property.name, '/');
        int index = StringUtils::to_int(parts[1]) - 1;
        if (index >= draw_passes.size()) {
            property.usage = 0;
        }
    }
}

void GPUParticles3D::_notification(int p_what) {
    auto RS = RenderingServer::get_singleton();
    if (p_what == NOTIFICATION_PAUSED || p_what == NOTIFICATION_UNPAUSED) {
        if (can_process()) {
            RS->particles_set_speed_scale(particles, speed_scale);
        } else {

            RS->particles_set_speed_scale(particles, 0);
        }
    }

    // Use internal process when emitting and one_shot are on so that when
    // the shot ends the editor can properly update
    if (p_what == NOTIFICATION_INTERNAL_PROCESS) {

        if (one_shot && !is_emitting()) {
            Object_change_notify(this);
            set_process_internal(false);
        }
    }
    if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
        // make sure particles are updated before rendering occurs if they were active before
        if (is_visible_in_tree() && !RS->particles_is_inactive(particles)) {
            RS->particles_request_process(particles);
        }
    }
}

void GPUParticles3D::_bind_methods() {

    SE_BIND_METHOD(GPUParticles3D,set_emitting);
    SE_BIND_METHOD(GPUParticles3D,set_amount);
    SE_BIND_METHOD(GPUParticles3D,set_lifetime);
    SE_BIND_METHOD(GPUParticles3D,set_one_shot);
    SE_BIND_METHOD(GPUParticles3D,set_pre_process_time);
    SE_BIND_METHOD(GPUParticles3D,set_explosiveness_ratio);
    SE_BIND_METHOD(GPUParticles3D,set_randomness_ratio);
    SE_BIND_METHOD(GPUParticles3D,set_visibility_aabb);
    SE_BIND_METHOD(GPUParticles3D,set_use_local_coordinates);
    SE_BIND_METHOD(GPUParticles3D,set_fixed_fps);
    SE_BIND_METHOD(GPUParticles3D,set_fractional_delta);
    SE_BIND_METHOD(GPUParticles3D,set_process_material);
    SE_BIND_METHOD(GPUParticles3D,set_speed_scale);

    SE_BIND_METHOD(GPUParticles3D,is_emitting);
    SE_BIND_METHOD(GPUParticles3D,get_amount);
    SE_BIND_METHOD(GPUParticles3D,get_lifetime);
    SE_BIND_METHOD(GPUParticles3D,get_one_shot);
    SE_BIND_METHOD(GPUParticles3D,get_pre_process_time);
    SE_BIND_METHOD(GPUParticles3D,get_explosiveness_ratio);
    SE_BIND_METHOD(GPUParticles3D,get_randomness_ratio);
    SE_BIND_METHOD(GPUParticles3D,get_visibility_aabb);
    SE_BIND_METHOD(GPUParticles3D,get_use_local_coordinates);
    SE_BIND_METHOD(GPUParticles3D,get_fixed_fps);
    SE_BIND_METHOD(GPUParticles3D,get_fractional_delta);
    SE_BIND_METHOD(GPUParticles3D,get_process_material);
    SE_BIND_METHOD(GPUParticles3D,get_speed_scale);

    SE_BIND_METHOD(GPUParticles3D,set_draw_order);

    SE_BIND_METHOD(GPUParticles3D,get_draw_order);

    SE_BIND_METHOD(GPUParticles3D,set_draw_passes);
    SE_BIND_METHOD(GPUParticles3D,set_draw_pass_mesh);

    SE_BIND_METHOD(GPUParticles3D,get_draw_passes);
    SE_BIND_METHOD(GPUParticles3D,get_draw_pass_mesh);

    SE_BIND_METHOD(GPUParticles3D,restart);
    SE_BIND_METHOD(GPUParticles3D,capture_aabb);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "emitting"), "set_emitting", "is_emitting");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "amount", PropertyHint::ExpRange, "1,1000000,1"), "set_amount", "get_amount");
    ADD_GROUP("Time", "");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "lifetime", PropertyHint::ExpRange, "0.01,600.0,0.01,or_greater"), "set_lifetime", "get_lifetime");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "one_shot"), "set_one_shot", "get_one_shot");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "preprocess", PropertyHint::ExpRange, "0.00,600.0,0.01"), "set_pre_process_time", "get_pre_process_time");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "speed_scale", PropertyHint::Range, "0,64,0.01"), "set_speed_scale", "get_speed_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "explosiveness", PropertyHint::Range, "0,1,0.01"), "set_explosiveness_ratio", "get_explosiveness_ratio");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "randomness", PropertyHint::Range, "0,1,0.01"), "set_randomness_ratio", "get_randomness_ratio");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "fixed_fps", PropertyHint::Range, "0,1000,1"), "set_fixed_fps", "get_fixed_fps");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "fract_delta"), "set_fractional_delta", "get_fractional_delta");
    ADD_GROUP("Drawing", "");
    ADD_PROPERTY(PropertyInfo(VariantType::AABB, "visibility_aabb"), "set_visibility_aabb", "get_visibility_aabb");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "local_coords"), "set_use_local_coordinates", "get_use_local_coordinates");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "draw_order", PropertyHint::Enum, "Index,Lifetime,View Depth"), "set_draw_order", "get_draw_order");
    ADD_GROUP("Process Material", "");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "process_material", PropertyHint::ResourceType, "ShaderMaterial,ParticlesMaterial"), "set_process_material", "get_process_material");

    ADD_GROUP("Draw Passes", "draw_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "draw_passes", PropertyHint::Range, "0," + itos(MAX_DRAW_PASSES) + ",1"), "set_draw_passes", "get_draw_passes");
    ADD_PROPERTY_ARRAY("Draw Passes",MAX_DRAW_PASSES,"draw_pass");

    for (int i = 0; i < MAX_DRAW_PASSES; i++) {

        ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, StringName("draw_pass/" + itos(i + 1) + "/mesh"), PropertyHint::ResourceType, "Mesh"), "set_draw_pass_mesh", "get_draw_pass_mesh", i);
    }

    BIND_ENUM_CONSTANT(DRAW_ORDER_INDEX);
    BIND_ENUM_CONSTANT(DRAW_ORDER_LIFETIME);
    BIND_ENUM_CONSTANT(DRAW_ORDER_VIEW_DEPTH);

    BIND_CONSTANT(MAX_DRAW_PASSES);
}

GPUParticles3D::GPUParticles3D() {

    particles = RenderingServer::get_singleton()->particles_create();
    set_base(particles);
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
    set_visibility_aabb(AABB(Vector3(-4, -4, -4), Vector3(8, 8, 8)));
    set_use_local_coordinates(true);
    set_draw_passes(1);
    set_draw_order(DRAW_ORDER_INDEX);
    set_speed_scale(1);
}

GPUParticles3D::~GPUParticles3D() {

    RenderingServer::get_singleton()->free_rid(particles);
}
