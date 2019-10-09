/*************************************************************************/
/*  particles.cpp                                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "particles.h"

#include "core/os/os.h"
#include "scene/resources/particles_material.h"
#include "scene/resources/mesh.h"
#include "core/method_bind.h"

#include "servers/visual_server.h"

IMPL_GDCLASS(Particles)
VARIANT_ENUM_CAST(Particles::DrawOrder)

AABB Particles::get_aabb() const {

    return AABB();
}
PoolVector<Face3> Particles::get_faces(uint32_t p_usage_flags) const {

    return PoolVector<Face3>();
}

void Particles::set_emitting(bool p_emitting) {

    VisualServer::get_singleton()->particles_set_emitting(particles, p_emitting);

    if (p_emitting && one_shot) {
        set_process_internal(true);
    } else if (!p_emitting) {
        set_process_internal(false);
    }
}

void Particles::set_amount(int p_amount) {

	ERR_FAIL_COND_MSG(p_amount < 1, "Amount of particles cannot be smaller than 1.");
    amount = p_amount;
    VisualServer::get_singleton()->particles_set_amount(particles, amount);
}
void Particles::set_lifetime(float p_lifetime) {

	ERR_FAIL_COND_MSG(p_lifetime <= 0, "Particles lifetime must be greater than 0.");
    lifetime = p_lifetime;
    VisualServer::get_singleton()->particles_set_lifetime(particles, lifetime);
}

void Particles::set_one_shot(bool p_one_shot) {

    one_shot = p_one_shot;
    VisualServer::get_singleton()->particles_set_one_shot(particles, one_shot);

    if (is_emitting()) {

        set_process_internal(true);
        if (!one_shot)
            VisualServer::get_singleton()->particles_restart(particles);
    }

    if (!one_shot)
        set_process_internal(false);
}

void Particles::set_pre_process_time(float p_time) {

    pre_process_time = p_time;
    VisualServer::get_singleton()->particles_set_pre_process_time(particles, pre_process_time);
}
void Particles::set_explosiveness_ratio(float p_ratio) {

    explosiveness_ratio = p_ratio;
    VisualServer::get_singleton()->particles_set_explosiveness_ratio(particles, explosiveness_ratio);
}
void Particles::set_randomness_ratio(float p_ratio) {

    randomness_ratio = p_ratio;
    VisualServer::get_singleton()->particles_set_randomness_ratio(particles, randomness_ratio);
}
void Particles::set_visibility_aabb(const AABB &p_aabb) {

    visibility_aabb = p_aabb;
    VisualServer::get_singleton()->particles_set_custom_aabb(particles, visibility_aabb);
    update_gizmo();
    _change_notify("visibility_aabb");
}
void Particles::set_use_local_coordinates(bool p_enable) {

    local_coords = p_enable;
    VisualServer::get_singleton()->particles_set_use_local_coordinates(particles, local_coords);
}
void Particles::set_process_material(const Ref<Material> &p_material) {

    process_material = p_material;
    RID material_rid;
    if (process_material)
        material_rid = process_material->get_rid();
    VisualServer::get_singleton()->particles_set_process_material(particles, material_rid);

    update_configuration_warning();
}

void Particles::set_speed_scale(float p_scale) {

    speed_scale = p_scale;
    VisualServer::get_singleton()->particles_set_speed_scale(particles, p_scale);
}

bool Particles::is_emitting() const {

    return VisualServer::get_singleton()->particles_get_emitting(particles);
}
int Particles::get_amount() const {

    return amount;
}
float Particles::get_lifetime() const {

    return lifetime;
}
bool Particles::get_one_shot() const {

    return one_shot;
}

float Particles::get_pre_process_time() const {

    return pre_process_time;
}
float Particles::get_explosiveness_ratio() const {

    return explosiveness_ratio;
}
float Particles::get_randomness_ratio() const {

    return randomness_ratio;
}
AABB Particles::get_visibility_aabb() const {

    return visibility_aabb;
}
bool Particles::get_use_local_coordinates() const {

    return local_coords;
}
Ref<Material> Particles::get_process_material() const {

    return process_material;
}

float Particles::get_speed_scale() const {

    return speed_scale;
}

void Particles::set_draw_order(DrawOrder p_order) {

    draw_order = p_order;
    VisualServer::get_singleton()->particles_set_draw_order(particles, VS::ParticlesDrawOrder(p_order));
}

Particles::DrawOrder Particles::get_draw_order() const {

    return draw_order;
}

void Particles::set_draw_passes(int p_count) {

    ERR_FAIL_COND(p_count < 1)
    draw_passes.resize(p_count);
    VisualServer::get_singleton()->particles_set_draw_passes(particles, p_count);
    _change_notify();
}
int Particles::get_draw_passes() const {

    return draw_passes.size();
}

void Particles::set_draw_pass_mesh(int p_pass, const Ref<Mesh> &p_mesh) {

    ERR_FAIL_INDEX(p_pass, draw_passes.size());

    draw_passes.write[p_pass] = p_mesh;

    RID mesh_rid;
    if (p_mesh)
        mesh_rid = p_mesh->get_rid();

    VisualServer::get_singleton()->particles_set_draw_pass_mesh(particles, p_pass, mesh_rid);

    update_configuration_warning();
}

Ref<Mesh> Particles::get_draw_pass_mesh(int p_pass) const {

    ERR_FAIL_INDEX_V(p_pass, draw_passes.size(), Ref<Mesh>());

    return draw_passes[p_pass];
}

void Particles::set_fixed_fps(int p_count) {
    fixed_fps = p_count;
    VisualServer::get_singleton()->particles_set_fixed_fps(particles, p_count);
}

int Particles::get_fixed_fps() const {
    return fixed_fps;
}

void Particles::set_fractional_delta(bool p_enable) {
    fractional_delta = p_enable;
    VisualServer::get_singleton()->particles_set_fractional_delta(particles, p_enable);
}

bool Particles::get_fractional_delta() const {
    return fractional_delta;
}

String Particles::get_configuration_warning() const {

    if (OS::get_singleton()->get_current_video_driver() == OS::VIDEO_DRIVER_GLES2) {
        return TTR("GPU-based particles are not supported by the GLES2 video driver.\nUse the CPUParticles node instead. You can use the \"Convert to CPUParticles\" option for this purpose.");
    }

    String warnings;

    bool meshes_found = false;
    bool anim_material_found = false;

    for (int i = 0; i < draw_passes.size(); i++) {
        if (draw_passes[i]) {
            meshes_found = true;
            for (int j = 0; j < draw_passes[i]->get_surface_count(); j++) {
                anim_material_found = object_cast<ShaderMaterial>(draw_passes[i]->surface_get_material(j).get()) != nullptr;
                SpatialMaterial *spat = object_cast<SpatialMaterial>(draw_passes[i]->surface_get_material(j).get());
                anim_material_found = anim_material_found || (spat && spat->get_billboard_mode() == SpatialMaterial::BILLBOARD_PARTICLES);
            }
            if (anim_material_found) break;
        }
    }

    anim_material_found = anim_material_found || object_cast<ShaderMaterial>(get_material_override().get()) != nullptr;
    SpatialMaterial *spat = object_cast<SpatialMaterial>(get_material_override().get());
    anim_material_found = anim_material_found || (spat && spat->get_billboard_mode() == SpatialMaterial::BILLBOARD_PARTICLES);

    if (!meshes_found) {
        if (!warnings.empty())
            warnings += "\n";
        warnings += "- " + TTR("Nothing is visible because meshes have not been assigned to draw passes.");
    }

    if (not process_material) {
        if (!warnings.empty())
            warnings += "\n";
        warnings += "- " + TTR("A material to process the particles is not assigned, so no behavior is imprinted.");
    } else {
        const ParticlesMaterial *process = object_cast<ParticlesMaterial>(process_material.get());
        if (!anim_material_found && process &&
                (process->get_param(ParticlesMaterial::PARAM_ANIM_SPEED) != 0.0f || process->get_param(ParticlesMaterial::PARAM_ANIM_OFFSET) != 0.0f ||
                        process->get_param_texture(ParticlesMaterial::PARAM_ANIM_SPEED) || process->get_param_texture(ParticlesMaterial::PARAM_ANIM_OFFSET))) {
            if (!warnings.empty())
                warnings += "\n";
            warnings += "- " + TTR("Particles animation requires the usage of a SpatialMaterial whose Billboard Mode is set to \"Particle Billboard\".");
        }
    }

    return warnings;
}

void Particles::restart() {

    VisualServer::get_singleton()->particles_restart(particles);
    VisualServer::get_singleton()->particles_set_emitting(particles, true);
}

AABB Particles::capture_aabb() const {

    return VisualServer::get_singleton()->particles_get_current_aabb(particles);
}

void Particles::_validate_property(PropertyInfo &property) const {

    if (StringUtils::begins_with(property.name,"draw_pass_")) {
        int index = StringUtils::to_int(StringUtils::get_slice(property.name,'_', 2)) - 1;
        if (index >= draw_passes.size()) {
            property.usage = 0;
            return;
        }
    }
}

void Particles::_notification(int p_what) {

    if (p_what == NOTIFICATION_PAUSED || p_what == NOTIFICATION_UNPAUSED) {
        if (can_process()) {
            VisualServer::get_singleton()->particles_set_speed_scale(particles, speed_scale);
        } else {

            VisualServer::get_singleton()->particles_set_speed_scale(particles, 0);
        }
    }

    // Use internal process when emitting and one_shot are on so that when
    // the shot ends the editor can properly update
    if (p_what == NOTIFICATION_INTERNAL_PROCESS) {

        if (one_shot && !is_emitting()) {
            _change_notify();
            set_process_internal(false);
        }
    }
}

void Particles::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_emitting", {"emitting"}), &Particles::set_emitting);
    MethodBinder::bind_method(D_METHOD("set_amount", {"amount"}), &Particles::set_amount);
    MethodBinder::bind_method(D_METHOD("set_lifetime", {"secs"}), &Particles::set_lifetime);
    MethodBinder::bind_method(D_METHOD("set_one_shot", {"enable"}), &Particles::set_one_shot);
    MethodBinder::bind_method(D_METHOD("set_pre_process_time", {"secs"}), &Particles::set_pre_process_time);
    MethodBinder::bind_method(D_METHOD("set_explosiveness_ratio", {"ratio"}), &Particles::set_explosiveness_ratio);
    MethodBinder::bind_method(D_METHOD("set_randomness_ratio", {"ratio"}), &Particles::set_randomness_ratio);
    MethodBinder::bind_method(D_METHOD("set_visibility_aabb", {"aabb"}), &Particles::set_visibility_aabb);
    MethodBinder::bind_method(D_METHOD("set_use_local_coordinates", {"enable"}), &Particles::set_use_local_coordinates);
    MethodBinder::bind_method(D_METHOD("set_fixed_fps", {"fps"}), &Particles::set_fixed_fps);
    MethodBinder::bind_method(D_METHOD("set_fractional_delta", {"enable"}), &Particles::set_fractional_delta);
    MethodBinder::bind_method(D_METHOD("set_process_material", {"material"}), &Particles::set_process_material);
    MethodBinder::bind_method(D_METHOD("set_speed_scale", {"scale"}), &Particles::set_speed_scale);

    MethodBinder::bind_method(D_METHOD("is_emitting"), &Particles::is_emitting);
    MethodBinder::bind_method(D_METHOD("get_amount"), &Particles::get_amount);
    MethodBinder::bind_method(D_METHOD("get_lifetime"), &Particles::get_lifetime);
    MethodBinder::bind_method(D_METHOD("get_one_shot"), &Particles::get_one_shot);
    MethodBinder::bind_method(D_METHOD("get_pre_process_time"), &Particles::get_pre_process_time);
    MethodBinder::bind_method(D_METHOD("get_explosiveness_ratio"), &Particles::get_explosiveness_ratio);
    MethodBinder::bind_method(D_METHOD("get_randomness_ratio"), &Particles::get_randomness_ratio);
    MethodBinder::bind_method(D_METHOD("get_visibility_aabb"), &Particles::get_visibility_aabb);
    MethodBinder::bind_method(D_METHOD("get_use_local_coordinates"), &Particles::get_use_local_coordinates);
    MethodBinder::bind_method(D_METHOD("get_fixed_fps"), &Particles::get_fixed_fps);
    MethodBinder::bind_method(D_METHOD("get_fractional_delta"), &Particles::get_fractional_delta);
    MethodBinder::bind_method(D_METHOD("get_process_material"), &Particles::get_process_material);
    MethodBinder::bind_method(D_METHOD("get_speed_scale"), &Particles::get_speed_scale);

    MethodBinder::bind_method(D_METHOD("set_draw_order", {"order"}), &Particles::set_draw_order);

    MethodBinder::bind_method(D_METHOD("get_draw_order"), &Particles::get_draw_order);

    MethodBinder::bind_method(D_METHOD("set_draw_passes", {"passes"}), &Particles::set_draw_passes);
    MethodBinder::bind_method(D_METHOD("set_draw_pass_mesh", {"pass", "mesh"}), &Particles::set_draw_pass_mesh);

    MethodBinder::bind_method(D_METHOD("get_draw_passes"), &Particles::get_draw_passes);
    MethodBinder::bind_method(D_METHOD("get_draw_pass_mesh", {"pass"}), &Particles::get_draw_pass_mesh);

    MethodBinder::bind_method(D_METHOD("restart"), &Particles::restart);
    MethodBinder::bind_method(D_METHOD("capture_aabb"), &Particles::capture_aabb);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "emitting"), "set_emitting", "is_emitting");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "amount", PROPERTY_HINT_EXP_RANGE, "1,1000000,1"), "set_amount", "get_amount");
    ADD_GROUP("Time", "");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "lifetime", PROPERTY_HINT_EXP_RANGE, "0.01,600.0,0.01,or_greater"), "set_lifetime", "get_lifetime");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "one_shot"), "set_one_shot", "get_one_shot");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "preprocess", PROPERTY_HINT_EXP_RANGE, "0.00,600.0,0.01"), "set_pre_process_time", "get_pre_process_time");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "speed_scale", PROPERTY_HINT_RANGE, "0,64,0.01"), "set_speed_scale", "get_speed_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "explosiveness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_explosiveness_ratio", "get_explosiveness_ratio");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "randomness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_randomness_ratio", "get_randomness_ratio");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "fixed_fps", PROPERTY_HINT_RANGE, "0,1000,1"), "set_fixed_fps", "get_fixed_fps");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "fract_delta"), "set_fractional_delta", "get_fractional_delta");
    ADD_GROUP("Drawing", "");
    ADD_PROPERTY(PropertyInfo(VariantType::AABB, "visibility_aabb"), "set_visibility_aabb", "get_visibility_aabb");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "local_coords"), "set_use_local_coordinates", "get_use_local_coordinates");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "draw_order", PROPERTY_HINT_ENUM, "Index,Lifetime,View Depth"), "set_draw_order", "get_draw_order");
    ADD_GROUP("Process Material", "");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "process_material", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,ParticlesMaterial"), "set_process_material", "get_process_material");
    ADD_GROUP("Draw Passes", "draw_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "draw_passes", PROPERTY_HINT_RANGE, "0," + itos(MAX_DRAW_PASSES) + ",1"), "set_draw_passes", "get_draw_passes");
    for (int i = 0; i < MAX_DRAW_PASSES; i++) {

        ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "draw_pass_" + itos(i + 1), PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_draw_pass_mesh", "get_draw_pass_mesh", i);
    }

    BIND_ENUM_CONSTANT(DRAW_ORDER_INDEX)
    BIND_ENUM_CONSTANT(DRAW_ORDER_LIFETIME)
    BIND_ENUM_CONSTANT(DRAW_ORDER_VIEW_DEPTH)

    BIND_CONSTANT(MAX_DRAW_PASSES);
}

Particles::Particles() {

    particles = VisualServer::get_singleton()->particles_create();
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

Particles::~Particles() {

    VisualServer::get_singleton()->free(particles);
}
