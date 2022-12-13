/*************************************************************************/
/*  cpu_particles_2d.cpp                                                 */
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

#include "cpu_particles_2d.h"

#include "core/callable_method_pointer.h"
#include "core/core_string_names.h"
#include "core/math/basis.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/translation_helpers.h"
#include "scene/2d/canvas_item.h"
#include "scene/2d/canvas_item_material.h"
#include "scene/2d/gpu_particles_2d.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/particles_material.h"
#include "servers/rendering_server.h"


IMPL_GDCLASS(CPUParticles2D)
VARIANT_ENUM_CAST(CPUParticles2D::DrawOrder);
VARIANT_ENUM_CAST(CPUParticles2D::Parameter);
VARIANT_ENUM_CAST(CPUParticles2D::Flags);
VARIANT_ENUM_CAST(CPUParticles2D::EmissionShape);

void CPUParticles2D::set_emitting(bool p_emitting) {

    if (emitting == p_emitting)
        return;

    emitting = p_emitting;
    if (emitting)
        set_process_internal(true);
}

void CPUParticles2D::set_amount(int p_amount) {

    ERR_FAIL_COND_MSG(p_amount < 1, "Amount of particles must be greater than 0.");

    particles.resize(p_amount);
    {
        PoolVector<Particle>::Write w = particles.write();

        //TODO: consider resetting whole object contents here, instead of only flag.
        for (int i = 0; i < p_amount; i++) {
            w[i].active = false;
        }
    }

    particle_data.resize((8 + 4 + 1) * p_amount);
    RenderingServer::get_singleton()->multimesh_allocate(
            multimesh, p_amount, RS::MULTIMESH_TRANSFORM_2D, RS::MULTIMESH_COLOR_8BIT, RS::MULTIMESH_CUSTOM_DATA_FLOAT);

    particle_order.resize(p_amount);
}
void CPUParticles2D::set_lifetime(float p_lifetime) {

    ERR_FAIL_COND_MSG(p_lifetime <= 0, "Particles lifetime must be greater than 0.");
    lifetime = p_lifetime;
}

void CPUParticles2D::set_one_shot(bool p_one_shot) {

    one_shot = p_one_shot;
}

void CPUParticles2D::set_pre_process_time(float p_time) {

    pre_process_time = p_time;
}
void CPUParticles2D::set_explosiveness_ratio(float p_ratio) {

    explosiveness_ratio = p_ratio;
}
void CPUParticles2D::set_randomness_ratio(float p_ratio) {

    randomness_ratio = p_ratio;
}
void CPUParticles2D::set_lifetime_randomness(float p_random) {

    lifetime_randomness = p_random;
}
void CPUParticles2D::set_use_local_coordinates(bool p_enable) {

    local_coords = p_enable;
    set_notify_transform(!p_enable);
}

void CPUParticles2D::set_speed_scale(float p_scale) {

    speed_scale = p_scale;
}

bool CPUParticles2D::is_emitting() const {

    return emitting;
}
int CPUParticles2D::get_amount() const {

    return particles.size();
}
float CPUParticles2D::get_lifetime() const {

    return lifetime;
}
bool CPUParticles2D::get_one_shot() const {

    return one_shot;
}

float CPUParticles2D::get_pre_process_time() const {

    return pre_process_time;
}
float CPUParticles2D::get_explosiveness_ratio() const {

    return explosiveness_ratio;
}
float CPUParticles2D::get_randomness_ratio() const {

    return randomness_ratio;
}
float CPUParticles2D::get_lifetime_randomness() const {

    return lifetime_randomness;
}

bool CPUParticles2D::get_use_local_coordinates() const {

    return local_coords;
}

float CPUParticles2D::get_speed_scale() const {

    return speed_scale;
}

void CPUParticles2D::set_draw_order(DrawOrder p_order) {

    draw_order = p_order;
}

CPUParticles2D::DrawOrder CPUParticles2D::get_draw_order() const {

    return draw_order;
}

void CPUParticles2D::_update_mesh_texture() {

    Size2 tex_size;
    if (texture) {
        tex_size = texture->get_size();
    } else {
        tex_size = Size2(1, 1);
    }
    Vector<Vector2> vertices {
        Vector2(-tex_size * 0.5),
        Vector2(-tex_size * 0.5 + Vector2(tex_size.x, 0)),
        Vector2(-tex_size * 0.5 + Vector2(tex_size.x, tex_size.y)),
        Vector2(-tex_size * 0.5 + Vector2(0, tex_size.y)),
    };

    Vector<Vector2> uvs;
    uvs.reserve(4);
    AtlasTexture *atlas_texure = object_cast<AtlasTexture>(texture.get());
    if (atlas_texure && atlas_texure->get_atlas()) {
        Rect2 region_rect = atlas_texure->get_region();
        Size2 atlas_size = atlas_texure->get_atlas()->get_size();
        uvs.emplace_back(region_rect.position.x / atlas_size.x, region_rect.position.y / atlas_size.y);
        uvs.emplace_back(
                (region_rect.position.x + region_rect.size.x) / atlas_size.x, region_rect.position.y / atlas_size.y);
        uvs.emplace_back((region_rect.position.x + region_rect.size.x) / atlas_size.x,
                (region_rect.position.y + region_rect.size.y) / atlas_size.y);
        uvs.emplace_back(
                region_rect.position.x / atlas_size.x, (region_rect.position.y + region_rect.size.y) / atlas_size.y);
    } else {
        uvs.emplace_back(0, 0);
        uvs.emplace_back(1, 0);
        uvs.emplace_back(1, 1);
        uvs.emplace_back(0, 1);
    }
    Vector<Color> colors {
        Color(1, 1, 1, 1),
        Color(1, 1, 1, 1),
        Color(1, 1, 1, 1),
        Color(1, 1, 1, 1),
    };
    Vector<int> indices{
        0,
        1,
        2,
        2,
        3,
        0,
    };

    SurfaceArrays arr(eastl::move(vertices));
    arr.m_uv_1 = eastl::move(uvs);
    arr.m_colors = eastl::move(colors);
    arr.m_indices = eastl::move(indices);

    RenderingServer::get_singleton()->mesh_clear(mesh);
    RenderingServer::get_singleton()->mesh_add_surface_from_arrays(mesh, RS::PRIMITIVE_TRIANGLES, eastl::move(arr));
}

void CPUParticles2D::set_texture(const Ref<Texture> &p_texture) {
    if (p_texture == texture)
        return;

    if (texture)
        texture->disconnect(
                CoreStringNames::get_singleton()->changed, callable_mp(this, &CPUParticles2D::_texture_changed));

    texture = p_texture;

    if (texture)
        texture->connect(
                CoreStringNames::get_singleton()->changed, callable_mp(this, &CPUParticles2D::_texture_changed));

    update();
    _update_mesh_texture();
}

void CPUParticles2D::_texture_changed() {

    if (texture) {
        update();
        _update_mesh_texture();
    }
}

Ref<Texture> CPUParticles2D::get_texture() const {

    return texture;
}

void CPUParticles2D::set_normalmap(const Ref<Texture> &p_normalmap) {

    normalmap = p_normalmap;
    update();
}

Ref<Texture> CPUParticles2D::get_normalmap() const {

    return normalmap;
}

void CPUParticles2D::set_fixed_fps(int p_count) {
    fixed_fps = p_count;
}

int CPUParticles2D::get_fixed_fps() const {
    return fixed_fps;
}

void CPUParticles2D::set_fractional_delta(bool p_enable) {
    fractional_delta = p_enable;
}

bool CPUParticles2D::get_fractional_delta() const {
    return fractional_delta;
}

String CPUParticles2D::get_configuration_warning() const {

    String warnings = BaseClassName::get_configuration_warning();

    CanvasItemMaterial *mat = object_cast<CanvasItemMaterial>(get_material().get());

    if (not get_material() || (mat && !mat->get_particles_animation())) {
        if (get_param(PARAM_ANIM_SPEED) != 0.0f || get_param(PARAM_ANIM_OFFSET) != 0.0f ||
                get_param_curve(PARAM_ANIM_SPEED) || get_param_curve(PARAM_ANIM_OFFSET)) {
            if (!warnings.empty())
                warnings += "\n\n";
            warnings += String("- ") + TTR("CPUParticles2D animation requires the usage of a CanvasItemMaterial with "
                                           "\"Particles Animation\" enabled.");
        }
    }

    return warnings;
}

void CPUParticles2D::restart() {

    time = 0;
    inactive_time = 0;
    frame_remainder = 0;
    cycle = 0;
    emitting = false;

    {
        int pc = particles.size();
        PoolVector<Particle>::Write w = particles.write();

        for (int i = 0; i < pc; i++) {
            w[i].active = false;
        }
    }

    set_emitting(true);
}

void CPUParticles2D::set_direction(Vector2 p_direction) {

    direction = p_direction;
}

Vector2 CPUParticles2D::get_direction() const {

    return direction;
}

void CPUParticles2D::set_spread(float p_spread) {

    spread = p_spread;
}

float CPUParticles2D::get_spread() const {

    return spread;
}

void CPUParticles2D::set_param(Parameter p_param, float p_value) {

    ERR_FAIL_INDEX(p_param, PARAM_MAX);

    parameters[p_param] = p_value;
}
float CPUParticles2D::get_param(Parameter p_param) const {

    ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);

    return parameters[p_param];
}

void CPUParticles2D::set_param_randomness(Parameter p_param, float p_value) {

    ERR_FAIL_INDEX(p_param, PARAM_MAX);

    randomness[p_param] = p_value;
}
float CPUParticles2D::get_param_randomness(Parameter p_param) const {

    ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);

    return randomness[p_param];
}

void CPUParticles2D::set_param_curve(Parameter p_param, const Ref<Curve> &p_curve) {

    ERR_FAIL_INDEX(p_param, PARAM_MAX);

    curve_parameters[p_param] = p_curve;
    const CurveRange range_to_set = c_default_curve_ranges[p_param];
    if(p_curve)
        p_curve->ensure_default_setup(range_to_set.curve_min,range_to_set.curve_max);
}
Ref<Curve> CPUParticles2D::get_param_curve(Parameter p_param) const {

    ERR_FAIL_INDEX_V(p_param, PARAM_MAX, Ref<Curve>());

    return curve_parameters[p_param];
}

void CPUParticles2D::set_color(const Color &p_color) {

    color = p_color;
}

Color CPUParticles2D::get_color() const {

    return color;
}

void CPUParticles2D::set_color_ramp(const Ref<Gradient> &p_ramp) {

    color_ramp = p_ramp;
}

Ref<Gradient> CPUParticles2D::get_color_ramp() const {

    return color_ramp;
}

void CPUParticles2D::set_color_initial_ramp(const Ref<Gradient> &p_ramp) {
    color_initial_ramp = p_ramp;
}

Ref<Gradient> CPUParticles2D::get_color_initial_ramp() const {
    return color_initial_ramp;
}
void CPUParticles2D::set_particle_flag(Flags p_flag, bool p_enable) {
    ERR_FAIL_INDEX(p_flag, FLAG_MAX);
    flags[p_flag] = p_enable;
}

bool CPUParticles2D::get_particle_flag(Flags p_flag) const {
    ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
    return flags[p_flag];
}

void CPUParticles2D::set_emission_shape(EmissionShape p_shape) {
    ERR_FAIL_INDEX(p_shape, EMISSION_SHAPE_MAX);

    emission_shape = p_shape;
    Object_change_notify(this);
}

void CPUParticles2D::set_emission_sphere_radius(float p_radius) {

    emission_sphere_radius = p_radius;
}

void CPUParticles2D::set_emission_rect_extents(Vector2 p_extents) {

    emission_rect_extents = p_extents;
}

void CPUParticles2D::set_emission_points(const PoolVector<Vector2> &p_points) {

    emission_points = p_points;
}

void CPUParticles2D::set_emission_normals(const PoolVector<Vector2> &p_normals) {

    emission_normals = p_normals;
}

void CPUParticles2D::set_emission_colors(const PoolVector<Color> &p_colors) {

    emission_colors = p_colors;
}

float CPUParticles2D::get_emission_sphere_radius() const {

    return emission_sphere_radius;
}
Vector2 CPUParticles2D::get_emission_rect_extents() const {

    return emission_rect_extents;
}
PoolVector<Vector2> CPUParticles2D::get_emission_points() const {

    return emission_points;
}
PoolVector<Vector2> CPUParticles2D::get_emission_normals() const {

    return emission_normals;
}

PoolVector<Color> CPUParticles2D::get_emission_colors() const {

    return emission_colors;
}

CPUParticles2D::EmissionShape CPUParticles2D::get_emission_shape() const {
    return emission_shape;
}
void CPUParticles2D::set_gravity(const Vector2 &p_gravity) {

    gravity = p_gravity;
}

Vector2 CPUParticles2D::get_gravity() const {

    return gravity;
}

void CPUParticles2D::_validate_property(PropertyInfo &property) const {


    if (property.name == "emission_sphere_radius" && emission_shape != EMISSION_SHAPE_SPHERE) {
        property.usage = 0;
    }

    if (property.name == "emission_rect_extents" && emission_shape != EMISSION_SHAPE_RECTANGLE) {
        property.usage = 0;
    }

    if ((property.name == "emission_point_texture" || property.name == "emission_color_texture") &&
            (emission_shape < EMISSION_SHAPE_POINTS)) {
        property.usage = 0;
    }

    if (property.name == "emission_normals" && emission_shape != EMISSION_SHAPE_DIRECTED_POINTS) {
        property.usage = 0;
    }

    if (property.name == "emission_points" && emission_shape != EMISSION_SHAPE_POINTS &&
            emission_shape != EMISSION_SHAPE_DIRECTED_POINTS) {
        property.usage = 0;
    }

    if (property.name == "emission_colors" && emission_shape != EMISSION_SHAPE_POINTS &&
            emission_shape != EMISSION_SHAPE_DIRECTED_POINTS) {
        property.usage = 0;
    }
}

void CPUParticles2D::_particles_process(float p_delta) {
    using namespace ParticleUtils;

    p_delta *= speed_scale;

    int pcount = particles.size();
    PoolVector<Particle>::Write w = particles.write();

    Particle *parray = w.ptr();

    float prev_time = time;
    time += p_delta;
    if (time > lifetime) {
        time = Math::fmod(time, lifetime);
        cycle++;
        if (one_shot && cycle > 0) {
            set_emitting(false);
            Object_change_notify(this);
        }
    }

    Transform2D emission_xform;
    Transform2D velocity_xform;
    if (!local_coords) {
        emission_xform = get_global_transform();
        velocity_xform = emission_xform;
        velocity_xform[2] = Vector2();
    }

    float system_phase = time / lifetime;

    for (int i = 0; i < pcount; i++) {

        Particle &p = parray[i];

        if (!emitting && !p.active)
            continue;

        float local_delta = p_delta;

        // The phase is a ratio between 0 (birth) and 1 (end of life) for each particle.
        // While we use time in tests later on, for randomness we use the phase as done in the
        // original shader code, and we later multiply by lifetime to get the time.
        float restart_phase = float(i) / float(pcount);

        if (randomness_ratio > 0.0) {
            uint32_t seed = cycle;
            if (restart_phase >= system_phase) {
                seed -= uint32_t(1);
            }
            seed *= uint32_t(pcount);
            seed += uint32_t(i);
            float random = float(idhash(seed) % uint32_t(65536)) / 65536.0;
            restart_phase += randomness_ratio * random * 1.0 / float(pcount);
        }

        restart_phase *= (1.0 - explosiveness_ratio);
        float restart_time = restart_phase * lifetime;
        bool restart = false;

        if (time > prev_time) {
            // restart_time >= prev_time is used so particles emit in the first frame they are processed

            if (restart_time >= prev_time && restart_time < time) {
                restart = true;
                if (fractional_delta) {
                    local_delta = time - restart_time;
                }
            }

        } else if (local_delta > 0.0) {
            if (restart_time >= prev_time) {
                restart = true;
                if (fractional_delta) {
                    local_delta = lifetime - restart_time + time;
                }

            } else if (restart_time < time) {
                restart = true;
                if (fractional_delta) {
                    local_delta = time - restart_time;
                }
            }
        }

        if (p.time * (1.0f - explosiveness_ratio) > p.lifetime) {
            restart = true;
        }
        float tv = 0.0f;

        if (restart) {

            if (!emitting) {
                p.active = false;
                continue;
            }
            p.active = true;


            float tex_angle = 0.0;
            if (curve_parameters[PARAM_ANGLE]) {
                tex_angle = curve_parameters[PARAM_ANGLE]->interpolate(tv);
            }

            float tex_anim_offset = 0.0;
            if (curve_parameters[PARAM_ANGLE]) {
                tex_anim_offset = curve_parameters[PARAM_ANGLE]->interpolate(tv);
            }

            p.seed = Math::rand();

            p.angle_rand = Math::randf();
            p.scale_rand = Math::randf();
            p.hue_rot_rand = Math::randf();
            p.anim_offset_rand = Math::randf();

            if (color_initial_ramp) {
                p.start_color_rand = color_initial_ramp->get_color_at_offset(Math::randf());
            } else {
                p.start_color_rand = Color(1, 1, 1, 1);
            }

            float angle1_rad =
                    Math::atan2(direction.y, direction.x) + (Math::randf() * 2.0 - 1.0) * Math_PI * spread / 180.0;
            Vector2 rot = Vector2(Math::cos(angle1_rad), Math::sin(angle1_rad));
            p.velocity = rot * parameters[PARAM_INITIAL_LINEAR_VELOCITY] *
                         Math::lerp(1.0f, float(Math::randf()), randomness[PARAM_INITIAL_LINEAR_VELOCITY]);

            float base_angle =
                    (parameters[PARAM_ANGLE] + tex_angle) * Math::lerp(1.0f, p.angle_rand, randomness[PARAM_ANGLE]);
            p.rotation = Math::deg2rad(base_angle);

            p.custom[0] = 0.0; // unused
            p.custom[1] = 0.0; // phase [0..1]
            p.custom[2] = (parameters[PARAM_ANIM_OFFSET] + tex_anim_offset) *
                          Math::lerp(1.0f, p.anim_offset_rand, randomness[PARAM_ANIM_OFFSET]); // animation phase [0..1]
            p.custom[3] = 0.0;
            p.transform = Transform2D();
            p.time = 0;
            p.lifetime = lifetime * (1.0 - Math::randf() * lifetime_randomness);
            p.base_color = Color(1, 1, 1, 1);

            switch (emission_shape) {
                case EMISSION_SHAPE_POINT: {
                    //do none
                } break;
                case EMISSION_SHAPE_SPHERE: {
                    float s = Math::randf(), t = 2.0 * Math_PI * Math::randf();
                    float radius = emission_sphere_radius * Math::sqrt(1.0 - s * s);
                    p.transform[2] = Vector2(Math::cos(t), Math::sin(t)) * radius;
                } break;
                case EMISSION_SHAPE_RECTANGLE: {
                    p.transform[2] =
                            Vector2(Math::randf() * 2.0 - 1.0, Math::randf() * 2.0 - 1.0) * emission_rect_extents;
                } break;
                case EMISSION_SHAPE_POINTS:
                case EMISSION_SHAPE_DIRECTED_POINTS: {

                    int pc = emission_points.size();
                    if (pc == 0)
                        break;

                    int random_idx = Math::rand() % pc;

                    p.transform[2] = emission_points.get(random_idx);

                    if (emission_shape == EMISSION_SHAPE_DIRECTED_POINTS && emission_normals.size() == pc) {
                        Vector2 normal = emission_normals.get(random_idx);
                        Transform2D m2;
                        m2.set_axis(0, normal);
                        m2.set_axis(1, normal.tangent());
                        p.velocity = m2.basis_xform(p.velocity);
                    }

                    if (emission_colors.size() == pc) {
                        p.base_color = emission_colors.get(random_idx);
                    }
                } break;
                case EMISSION_SHAPE_MAX: { // Max value for validity check.
                    break;
                }
            }

            if (!local_coords) {
                p.velocity = velocity_xform.xform(p.velocity);
                p.transform = emission_xform * p.transform;
            }

        } else if (!p.active) {
            continue;
        } else if (p.time > p.lifetime) {
            p.active = false;
            tv = 1.0;
        } else {

            uint32_t alt_seed = p.seed;

            p.time += local_delta;
            tv = p.time / p.lifetime;
            p.custom[1] = tv;

            float tex_linear_velocity = 0.0;
            if (curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY]) {
                tex_linear_velocity = curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY]->interpolate(tv);
            }

            float tex_orbit_velocity = 0.0;
            if (curve_parameters[PARAM_ORBIT_VELOCITY]) {
                tex_orbit_velocity = curve_parameters[PARAM_ORBIT_VELOCITY]->interpolate(tv);
            }

            float tex_angular_velocity = 0.0;
            if (curve_parameters[PARAM_ANGULAR_VELOCITY]) {
                tex_angular_velocity = curve_parameters[PARAM_ANGULAR_VELOCITY]->interpolate(tv);
            }

            float tex_linear_accel = 0.0;
            if (curve_parameters[PARAM_LINEAR_ACCEL]) {
                tex_linear_accel = curve_parameters[PARAM_LINEAR_ACCEL]->interpolate(tv);
            }

            float tex_tangential_accel = 0.0;
            if (curve_parameters[PARAM_TANGENTIAL_ACCEL]) {
                tex_tangential_accel = curve_parameters[PARAM_TANGENTIAL_ACCEL]->interpolate(tv);
            }

            float tex_radial_accel = 0.0;
            if (curve_parameters[PARAM_RADIAL_ACCEL]) {
                tex_radial_accel = curve_parameters[PARAM_RADIAL_ACCEL]->interpolate(tv);
            }

            float tex_damping = 0.0;
            if (curve_parameters[PARAM_DAMPING]) {
                tex_damping = curve_parameters[PARAM_DAMPING]->interpolate(tv);
            }

            float tex_angle = 0.0;
            if (curve_parameters[PARAM_ANGLE]) {
                tex_angle = curve_parameters[PARAM_ANGLE]->interpolate(tv);
            }
            float tex_anim_speed = 0.0;
            if (curve_parameters[PARAM_ANIM_SPEED]) {
                tex_anim_speed = curve_parameters[PARAM_ANIM_SPEED]->interpolate(tv);
            }

            float tex_anim_offset = 0.0;
            if (curve_parameters[PARAM_ANIM_OFFSET]) {
                tex_anim_offset = curve_parameters[PARAM_ANIM_OFFSET]->interpolate(tv);
            }

            Vector2 force = gravity;
            Vector2 pos = p.transform[2];

            //apply linear acceleration
            force += p.velocity.length() > 0.0 ?
                             p.velocity.normalized() * (parameters[PARAM_LINEAR_ACCEL] + tex_linear_accel) *
                                     Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_LINEAR_ACCEL]) :
                             Vector2();
            //apply radial acceleration
            Vector2 org = emission_xform[2];
            Vector2 diff = pos - org;
            force += diff.length() > 0.0 ?
                             diff.normalized() * (parameters[PARAM_RADIAL_ACCEL] + tex_radial_accel) *
                                     Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_RADIAL_ACCEL]) :
                             Vector2();
            //apply tangential acceleration;
            Vector2 yx = Vector2(diff.y, diff.x);
            force += yx.length() > 0.0 ? (yx * Vector2(-1.0, 1.0)).normalized() *
                                                 ((parameters[PARAM_TANGENTIAL_ACCEL] + tex_tangential_accel) *
                                                         Math::lerp(1.0f, rand_from_seed(alt_seed),
                                                                 randomness[PARAM_TANGENTIAL_ACCEL])) :
                                         Vector2();
            //apply attractor forces
            p.velocity += force * local_delta;
            //orbit velocity
            float orbit_amount = (parameters[PARAM_ORBIT_VELOCITY] + tex_orbit_velocity) *
                                 Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_ORBIT_VELOCITY]);
            if (orbit_amount != 0.0) {
                float ang = orbit_amount * local_delta * Math_PI * 2.0;
                // Not sure why the ParticlesMaterial code uses a clockwise rotation matrix,
                // but we use -ang here to reproduce its behavior.
                Transform2D rot = Transform2D(-ang, Vector2());
                p.transform[2] -= diff;
                p.transform[2] += rot.basis_xform(diff);
            }
            if (curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY]) {
                p.velocity = p.velocity.normalized() * tex_linear_velocity;
            }

            if (parameters[PARAM_DAMPING] + tex_damping > 0.0) {

                float v = p.velocity.length();
                float damp = (parameters[PARAM_DAMPING] + tex_damping) *
                             Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_DAMPING]);
                v -= damp * local_delta;
                if (v < 0.0) {
                    p.velocity = Vector2();
                } else {
                    p.velocity = p.velocity.normalized() * v;
                }
            }
            float base_angle =
                    (parameters[PARAM_ANGLE] + tex_angle) * Math::lerp(1.0f, p.angle_rand, randomness[PARAM_ANGLE]);
            base_angle += p.custom[1] * lifetime * (parameters[PARAM_ANGULAR_VELOCITY] + tex_angular_velocity) *
                          Math::lerp(1.0f, rand_from_seed(alt_seed) * 2.0f - 1.0f, randomness[PARAM_ANGULAR_VELOCITY]);
            p.rotation = Math::deg2rad(base_angle); //angle
            float animation_phase = (parameters[PARAM_ANIM_OFFSET] + tex_anim_offset) *
                                            Math::lerp(1.0f, p.anim_offset_rand, randomness[PARAM_ANIM_OFFSET]) +
                                    tv * (parameters[PARAM_ANIM_SPEED] + tex_anim_speed) *
                                            Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_ANIM_SPEED]);
            p.custom[2] = animation_phase;
        }
        //apply color
        //apply hue rotation

        float tex_scale = 1.0;
        if (curve_parameters[PARAM_SCALE]) {
            tex_scale = curve_parameters[PARAM_SCALE]->interpolate(tv);
        }

        float tex_hue_variation = 0.0;
        if (curve_parameters[PARAM_HUE_VARIATION]) {
            tex_hue_variation = curve_parameters[PARAM_HUE_VARIATION]->interpolate(tv);
        }

        float hue_rot_angle = (parameters[PARAM_HUE_VARIATION] + tex_hue_variation) * Math_PI * 2.0 *
                              Math::lerp(1.0f, p.hue_rot_rand * 2.0f - 1.0f, randomness[PARAM_HUE_VARIATION]);
        float hue_rot_c = Math::cos(hue_rot_angle);
        float hue_rot_s = Math::sin(hue_rot_angle);

        Basis hue_rot_mat;
        {
            Basis mat1(0.299, 0.587, 0.114, 0.299, 0.587, 0.114, 0.299, 0.587, 0.114);
            Basis mat2(0.701, -0.587, -0.114, -0.299, 0.413, -0.114, -0.300, -0.588, 0.886);
            Basis mat3(0.168, 0.330, -0.497, -0.328, 0.035, 0.292, 1.250, -1.050, -0.203);

            for (int j = 0; j < 3; j++) {
                hue_rot_mat[j] = mat1[j] + mat2[j] * hue_rot_c + mat3[j] * hue_rot_s;
            }
        }

        if (color_ramp) {
            p.color = color_ramp->get_color_at_offset(tv) * color;
        } else {
            p.color = color;
        }

        Vector3 color_rgb = hue_rot_mat.xform_inv(Vector3(p.color.r, p.color.g, p.color.b));
        p.color.r = color_rgb.x;
        p.color.g = color_rgb.y;
        p.color.b = color_rgb.z;

        p.color *= p.base_color * p.start_color_rand;

        if (flags[FLAG_ALIGN_Y_TO_VELOCITY]) {
            if (p.velocity.length() > 0.0) {

                p.transform.elements[1] = p.velocity.normalized();
                p.transform.elements[0] = p.transform.elements[1].tangent();
            }

        } else {
            p.transform.elements[0] = Vector2(Math::cos(p.rotation), -Math::sin(p.rotation));
            p.transform.elements[1] = Vector2(Math::sin(p.rotation), Math::cos(p.rotation));
        }

        //scale by scale
        float base_scale =
                tex_scale * Math::lerp(parameters[PARAM_SCALE], 1.0f, p.scale_rand * randomness[PARAM_SCALE]);
        if (base_scale < 0.000001)
            base_scale = 0.000001;

        p.transform.elements[0] *= base_scale;
        p.transform.elements[1] *= base_scale;

        p.transform[2] += p.velocity * local_delta;
    }
}

void CPUParticles2D::_update_particle_data_buffer() {

    MutexGuard guard(update_mutex);

        int pc = particles.size();

        PoolVector<int>::Write ow;
        int *order = nullptr;

    auto &w = particle_data;
        PoolVector<Particle>::Read r = particles.read();
    float *ptr = w.data();

        if (draw_order != DRAW_ORDER_INDEX) {
            ow = particle_order.write();
            order = ow.ptr();

            for (int i = 0; i < pc; i++) {
                order[i] = i;
            }
            if (draw_order == DRAW_ORDER_LIFETIME) {
                SortArray<int, SortLifetime> sorter;
                sorter.compare.particles = r.ptr();
                sorter.sort(order, pc);
            }
        }

        for (int i = 0; i < pc; i++) {

            int idx = order ? order[i] : i;

            Transform2D t = r[idx].transform;

            if (!local_coords) {
                t = inv_emission_transform * t;
            }

            if (r[idx].active) {

                ptr[0] = t.elements[0][0];
                ptr[1] = t.elements[1][0];
                ptr[2] = 0;
                ptr[3] = t.elements[2][0];
                ptr[4] = t.elements[0][1];
                ptr[5] = t.elements[1][1];
                ptr[6] = 0;
                ptr[7] = t.elements[2][1];

                Color c = r[idx].color;
                uint8_t *data8 = (uint8_t *)&ptr[8];
                data8[0] = CLAMP<uint8_t>(c.r * 255.0f, 0, 255);
                data8[1] = CLAMP<uint8_t>(c.g * 255.0f, 0, 255);
                data8[2] = CLAMP<uint8_t>(c.b * 255.0f, 0, 255);
                data8[3] = CLAMP<uint8_t>(c.a * 255.0f, 0, 255);

                ptr[9] = r[idx].custom[0];
                ptr[10] = r[idx].custom[1];
                ptr[11] = r[idx].custom[2];
                ptr[12] = r[idx].custom[3];

            } else {
                memset(ptr, 0, sizeof(float) * 13);
            }
            ptr += 13;
        }

}

void CPUParticles2D::_set_redraw(bool p_redraw) {
    if (redraw == p_redraw)
        return;
    redraw = p_redraw;

    {
        MutexGuard guard(update_mutex);

    auto RS = RenderingServer::get_singleton();
    if (redraw) {
        RS->connect("frame_pre_draw",callable_mp(this, &ClassName::_update_render_thread));
        RS->canvas_item_set_update_when_visible(get_canvas_item(), true);

        RS->multimesh_set_visible_instances(multimesh, -1);
    } else {
        if(RS->is_connected("frame_pre_draw",callable_mp(this, &ClassName::_update_render_thread))) {
            RS->disconnect("frame_pre_draw",callable_mp(this, &ClassName::_update_render_thread));
        }
        RS->canvas_item_set_update_when_visible(get_canvas_item(), false);

        RS->multimesh_set_visible_instances(multimesh, 0);
    }

    }

    update(); // redraw to update render list
}

void CPUParticles2D::_update_render_thread() {

    if (OS::get_singleton()->is_update_pending(true)) {
        MutexGuard guard(update_mutex);

    RenderingServer::get_singleton()->multimesh_set_as_bulk_array(multimesh, particle_data);

    }
}

void CPUParticles2D::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {
        set_process_internal(emitting);
    } else if (p_what == NOTIFICATION_EXIT_TREE) {
        _set_redraw(false);
    } else if (p_what == NOTIFICATION_DRAW) {
        // first update before rendering to avoid one frame delay after emitting starts
        if (emitting && (time == 0.0f))
            _update_internal();

        if (!redraw)
            return; // don't add to render list

        RenderingEntity texrid = texture ? texture->get_rid() : entt::null;
        RenderingEntity normrid = normalmap ? normalmap->get_rid() : entt::null;

        RenderingServer::get_singleton()->canvas_item_add_multimesh(get_canvas_item(), multimesh, texrid, normrid);

    } else if (p_what == NOTIFICATION_INTERNAL_PROCESS) {
        _update_internal();
    }

    else if (p_what == NOTIFICATION_TRANSFORM_CHANGED) {

        inv_emission_transform = get_global_transform().affine_inverse();

        if (local_coords)
            return;

        int pc = particles.size();

        auto &w = particle_data;
        PoolVector<Particle>::Read r = particles.read();
        float *ptr = w.data();

        for (int i = 0; i < pc; i++) {

            Transform2D t = inv_emission_transform * r[i].transform;

            if (r[i].active) {

                ptr[0] = t.elements[0][0];
                ptr[1] = t.elements[1][0];
                ptr[2] = 0;
                ptr[3] = t.elements[2][0];
                ptr[4] = t.elements[0][1];
                ptr[5] = t.elements[1][1];
                ptr[6] = 0;
                ptr[7] = t.elements[2][1];

            } else {
                memset(ptr, 0, sizeof(float) * 8);
            }

            ptr += 13;
        }
    }
}
void CPUParticles2D::_update_internal() {
    if (particles.empty() || !is_visible_in_tree()) {
        _set_redraw(false);
        return;
    }

    float delta = get_process_delta_time();
    if (emitting) {
        inactive_time = 0;
    } else {
        inactive_time += delta;
        if (inactive_time > lifetime * 1.2f) {
            set_process_internal(false);
            _set_redraw(false);

            //reset variables
            time = 0;
            inactive_time = 0;
            frame_remainder = 0;
            cycle = 0;
            return;
        }
    }
    _set_redraw(true);

    if (time == 0.0f && pre_process_time > 0.0f) {

        float frame_time;
        if (fixed_fps > 0)
            frame_time = 1.0f / fixed_fps;
        else
            frame_time = 1.0f / 30.0f;

        float todo = pre_process_time;

        while (todo >= 0) {
            _particles_process(frame_time);
            todo -= frame_time;
        }
    }

    if (fixed_fps > 0) {
        float frame_time = 1.0f / fixed_fps;
        float decr = frame_time;

        float ldelta = delta;
        if (ldelta > 0.1f) { //avoid recursive stalls if fps goes below 10
            ldelta = 0.1f;
        } else if (ldelta <= 0.0f) { //unlikely but..
            ldelta = 0.001f;
        }
        float todo = frame_remainder + ldelta;

        while (todo >= frame_time) {
            _particles_process(frame_time);
            todo -= decr;
        }

        frame_remainder = todo;

    } else {
        _particles_process(delta);
    }

    _update_particle_data_buffer();

}
void CPUParticles2D::convert_from_particles(Node *p_particles) {
    GPUParticles2D *particles = object_cast<GPUParticles2D>(p_particles);
    ERR_FAIL_COND_MSG(!particles, "Only GPUParticles2D nodes can be converted to CPUParticles2D.");

    set_emitting(particles->is_emitting());
    set_amount(particles->get_amount());
    set_lifetime(particles->get_lifetime());
    set_one_shot(particles->get_one_shot());
    set_pre_process_time(particles->get_pre_process_time());
    set_explosiveness_ratio(particles->get_explosiveness_ratio());
    set_randomness_ratio(particles->get_randomness_ratio());
    set_use_local_coordinates(particles->get_use_local_coordinates());
    set_fixed_fps(particles->get_fixed_fps());
    set_fractional_delta(particles->get_fractional_delta());
    set_speed_scale(particles->get_speed_scale());
    set_draw_order(DrawOrder(particles->get_draw_order()));
    set_texture(particles->get_texture());

    Ref<Material> mat = particles->get_material();
    if (mat) {
        set_material(mat);
    }

    Ref<ParticlesMaterial> material = dynamic_ref_cast<ParticlesMaterial>(particles->get_process_material());
    if (not material)
        return;

    Vector3 dir = material->get_direction();
    set_direction(Vector2(dir.x, dir.y));
    set_spread(material->get_spread());

    set_color(material->get_color());

    Ref<GradientTexture> gt = dynamic_ref_cast<GradientTexture>(material->get_color_ramp());
    if (gt) {
        set_color_ramp(gt->get_gradient());
    }

    Ref<GradientTexture> gti = dynamic_ref_cast<GradientTexture>(material->get_color_initial_ramp());
    if (gti) {
        set_color_initial_ramp(gti->get_gradient());
    }
    set_particle_flag(FLAG_ALIGN_Y_TO_VELOCITY, material->get_flag(ParticlesMaterial::FLAG_ALIGN_Y_TO_VELOCITY));

    set_emission_shape(EmissionShape(material->get_emission_shape()));
    set_emission_sphere_radius(material->get_emission_sphere_radius());
    Vector2 rect_extents = Vector2(material->get_emission_box_extents().x, material->get_emission_box_extents().y);
    set_emission_rect_extents(rect_extents);

    Vector2 gravity = Vector2(material->get_gravity().x, material->get_gravity().y);
    set_gravity(gravity);
    set_lifetime_randomness(material->get_lifetime_randomness());

#define CONVERT_PARAM(m_param)                                                            \
    set_param(m_param, material->get_param(ParticlesMaterial::m_param));                  \
    {                                                                                     \
        Ref<CurveTexture> ctex =                                                                                       \
                dynamic_ref_cast<CurveTexture>(material->get_param_texture(ParticlesMaterial::m_param));               \
        if (ctex)                                                                                                      \
            set_param_curve(m_param, ctex->get_curve());                                                               \
    }                                                                                     \
    set_param_randomness(m_param, material->get_param_randomness(ParticlesMaterial::m_param));

    CONVERT_PARAM(PARAM_INITIAL_LINEAR_VELOCITY)
    CONVERT_PARAM(PARAM_ANGULAR_VELOCITY)
    CONVERT_PARAM(PARAM_ORBIT_VELOCITY)
    CONVERT_PARAM(PARAM_LINEAR_ACCEL)
    CONVERT_PARAM(PARAM_RADIAL_ACCEL)
    CONVERT_PARAM(PARAM_TANGENTIAL_ACCEL)
    CONVERT_PARAM(PARAM_DAMPING)
    CONVERT_PARAM(PARAM_ANGLE)
    CONVERT_PARAM(PARAM_SCALE)
    CONVERT_PARAM(PARAM_HUE_VARIATION)
    CONVERT_PARAM(PARAM_ANIM_SPEED)
    CONVERT_PARAM(PARAM_ANIM_OFFSET)

#undef CONVERT_PARAM
}

void CPUParticles2D::_bind_methods() {

    SE_BIND_METHOD(CPUParticles2D,set_emitting);
    SE_BIND_METHOD(CPUParticles2D,set_amount);
    SE_BIND_METHOD(CPUParticles2D,set_lifetime);
    SE_BIND_METHOD(CPUParticles2D,set_one_shot);
    SE_BIND_METHOD(CPUParticles2D,set_pre_process_time);
    MethodBinder::bind_method(
            D_METHOD("set_explosiveness_ratio", { "ratio" }), &CPUParticles2D::set_explosiveness_ratio);
    SE_BIND_METHOD(CPUParticles2D,set_randomness_ratio);
    MethodBinder::bind_method(
            D_METHOD("set_lifetime_randomness", { "random" }), &CPUParticles2D::set_lifetime_randomness);
    MethodBinder::bind_method(
            D_METHOD("set_use_local_coordinates", { "enable" }), &CPUParticles2D::set_use_local_coordinates);
    SE_BIND_METHOD(CPUParticles2D,set_fixed_fps);
    SE_BIND_METHOD(CPUParticles2D,set_fractional_delta);
    SE_BIND_METHOD(CPUParticles2D,set_speed_scale);

    SE_BIND_METHOD(CPUParticles2D,is_emitting);
    SE_BIND_METHOD(CPUParticles2D,get_amount);
    SE_BIND_METHOD(CPUParticles2D,get_lifetime);
    SE_BIND_METHOD(CPUParticles2D,get_one_shot);
    SE_BIND_METHOD(CPUParticles2D,get_pre_process_time);
    SE_BIND_METHOD(CPUParticles2D,get_explosiveness_ratio);
    SE_BIND_METHOD(CPUParticles2D,get_randomness_ratio);
    SE_BIND_METHOD(CPUParticles2D,get_lifetime_randomness);
    SE_BIND_METHOD(CPUParticles2D,get_use_local_coordinates);
    SE_BIND_METHOD(CPUParticles2D,get_fixed_fps);
    SE_BIND_METHOD(CPUParticles2D,get_fractional_delta);
    SE_BIND_METHOD(CPUParticles2D,get_speed_scale);

    SE_BIND_METHOD(CPUParticles2D,set_draw_order);

    SE_BIND_METHOD(CPUParticles2D,get_draw_order);

    SE_BIND_METHOD(CPUParticles2D,set_texture);
    SE_BIND_METHOD(CPUParticles2D,get_texture);

    SE_BIND_METHOD(CPUParticles2D,set_normalmap);
    SE_BIND_METHOD(CPUParticles2D,get_normalmap);

    SE_BIND_METHOD(CPUParticles2D,restart);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "emitting"), "set_emitting", "is_emitting");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "amount", PropertyHint::ExpRange, "1,1000000,1"), "set_amount",
            "get_amount");
    ADD_GROUP("Time", "tm_");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "tm_lifetime", PropertyHint::Range, "0.01,600.0,0.01,or_greater"),
            "set_lifetime", "get_lifetime");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "tm_one_shot"), "set_one_shot", "get_one_shot");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "tm_preprocess", PropertyHint::Range, "0.00,600.0,0.01"),
            "set_pre_process_time", "get_pre_process_time");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "tm_speed_scale", PropertyHint::Range, "0,64,0.01"),
            "set_speed_scale", "get_speed_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "tm_explosiveness", PropertyHint::Range, "0,1,0.01"),
            "set_explosiveness_ratio", "get_explosiveness_ratio");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "tm_randomness", PropertyHint::Range, "0,1,0.01"),
            "set_randomness_ratio", "get_randomness_ratio");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "tm_lifetime_randomness", PropertyHint::Range, "0,1,0.01"),
            "set_lifetime_randomness", "get_lifetime_randomness");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "tm_fixed_fps", PropertyHint::Range, "0,1000,1"), "set_fixed_fps",
            "get_fixed_fps");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "tm_fract_delta"), "set_fractional_delta", "get_fractional_delta");
    ADD_GROUP("Drawing", "drw_");
    // No visibility_rect property contrarily to GPUParticles2D, it's updated automatically.
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "drw_local_coords"), "set_use_local_coordinates",
            "get_use_local_coordinates");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "drw_draw_order", PropertyHint::Enum, "Index,Lifetime"),
            "set_draw_order", "get_draw_order");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "drw_texture", PropertyHint::ResourceType, "Texture"), "set_texture",
            "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "drw_normalmap", PropertyHint::ResourceType, "Texture"),
            "set_normalmap", "get_normalmap");

    BIND_ENUM_CONSTANT(DRAW_ORDER_INDEX);
    BIND_ENUM_CONSTANT(DRAW_ORDER_LIFETIME);

    ////////////////////////////////

    SE_BIND_METHOD(CPUParticles2D,set_direction);
    SE_BIND_METHOD(CPUParticles2D,get_direction);

    SE_BIND_METHOD(CPUParticles2D,set_spread);
    SE_BIND_METHOD(CPUParticles2D,get_spread);

    SE_BIND_METHOD(CPUParticles2D,set_param);
    SE_BIND_METHOD(CPUParticles2D,get_param);

    MethodBinder::bind_method(
            D_METHOD("set_param_randomness", { "param", "randomness" }), &CPUParticles2D::set_param_randomness);
    SE_BIND_METHOD(CPUParticles2D,get_param_randomness);

    SE_BIND_METHOD(CPUParticles2D,set_param_curve);
    SE_BIND_METHOD(CPUParticles2D,get_param_curve);

    SE_BIND_METHOD(CPUParticles2D,set_color);
    SE_BIND_METHOD(CPUParticles2D,get_color);

    SE_BIND_METHOD(CPUParticles2D,set_color_ramp);
    SE_BIND_METHOD(CPUParticles2D,get_color_ramp);
    SE_BIND_METHOD(CPUParticles2D,set_color_initial_ramp);
    SE_BIND_METHOD(CPUParticles2D,get_color_initial_ramp);

    SE_BIND_METHOD(CPUParticles2D,set_particle_flag);
    SE_BIND_METHOD(CPUParticles2D,get_particle_flag);

    SE_BIND_METHOD(CPUParticles2D,set_emission_shape);
    SE_BIND_METHOD(CPUParticles2D,get_emission_shape);

    MethodBinder::bind_method(
            D_METHOD("set_emission_sphere_radius", { "radius" }), &CPUParticles2D::set_emission_sphere_radius);
    SE_BIND_METHOD(CPUParticles2D,get_emission_sphere_radius);

    MethodBinder::bind_method(
            D_METHOD("set_emission_rect_extents", { "extents" }), &CPUParticles2D::set_emission_rect_extents);
    SE_BIND_METHOD(CPUParticles2D,get_emission_rect_extents);

    SE_BIND_METHOD(CPUParticles2D,set_emission_points);
    SE_BIND_METHOD(CPUParticles2D,get_emission_points);

    SE_BIND_METHOD(CPUParticles2D,set_emission_normals);
    SE_BIND_METHOD(CPUParticles2D,get_emission_normals);

    SE_BIND_METHOD(CPUParticles2D,set_emission_colors);
    SE_BIND_METHOD(CPUParticles2D,get_emission_colors);

    SE_BIND_METHOD(CPUParticles2D,get_gravity);
    SE_BIND_METHOD(CPUParticles2D,set_gravity);

    MethodBinder::bind_method(
            D_METHOD("convert_from_particles", { "particles" }), &CPUParticles2D::convert_from_particles);

    //BIND_METHOD(CPUParticles2D,_update_render_thread);
    //BIND_METHOD(CPUParticles2D,_texture_changed);

    ADD_GROUP("Emission Shape", "emission_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "emission_shape", PropertyHint::Enum,
                         "Point,Sphere,Box,Points,Directed Points",
                         PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED),
            "set_emission_shape", "get_emission_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "emission_sphere_radius", PropertyHint::Range, "0.01,128,0.01,or_greater"),
            "set_emission_sphere_radius", "get_emission_sphere_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "emission_rect_extents"), "set_emission_rect_extents",
            "get_emission_rect_extents");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR2_ARRAY, "emission_points"), "set_emission_points",
            "get_emission_points");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR2_ARRAY, "emission_normals"), "set_emission_normals",
            "get_emission_normals");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_COLOR_ARRAY, "emission_colors"), "set_emission_colors",
            "get_emission_colors");
    ADD_GROUP("Flags", "flag_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flag_align_y"), "set_particle_flag", "get_particle_flag",
            FLAG_ALIGN_Y_TO_VELOCITY);
    ADD_GROUP("Direction", "dir_");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "dir_direction"), "set_direction", "get_direction");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "dir_spread", PropertyHint::Range, "0,180,0.01"), "set_spread",
            "get_spread");
    ADD_GROUP("Gravity", "");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "gravity"), "set_gravity", "get_gravity");
    ADD_GROUP("Initial Velocity", "initial_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "initial_velocity", PropertyHint::Range, "0,1000,0.01,or_greater"),
            "set_param", "get_param", PARAM_INITIAL_LINEAR_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "initial_velocity_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_INITIAL_LINEAR_VELOCITY);
    ADD_GROUP("Angular Velocity", "angular_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "angular_velocity", PropertyHint::Range,
                          "-720,720,0.01,or_lesser,or_greater"),
            "set_param", "get_param", PARAM_ANGULAR_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "angular_velocity_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_ANGULAR_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "angular_velocity_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_ANGULAR_VELOCITY);
    ADD_GROUP("Orbit Velocity", "orbit_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "orbit_velocity", PropertyHint::Range,
                          "-1000,1000,0.01,or_lesser,or_greater"),
            "set_param", "get_param", PARAM_ORBIT_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "orbit_velocity_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_ORBIT_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "orbit_velocity_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_ORBIT_VELOCITY);
    ADD_GROUP("Linear Accel", "linear_");
    ADD_PROPERTYI(
            PropertyInfo(VariantType::FLOAT, "linear_accel", PropertyHint::Range, "-100,100,0.01,or_lesser,or_greater"),
            "set_param", "get_param", PARAM_LINEAR_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "linear_accel_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_LINEAR_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "linear_accel_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_LINEAR_ACCEL);
    ADD_GROUP("Radial Accel", "radial_");
    ADD_PROPERTYI(
            PropertyInfo(VariantType::FLOAT, "radial_accel", PropertyHint::Range, "-100,100,0.01,or_lesser,or_greater"),
            "set_param", "get_param", PARAM_RADIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "radial_accel_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_RADIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "radial_accel_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_RADIAL_ACCEL);
    ADD_GROUP("Tangential Accel", "tangential_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "tangential_accel", PropertyHint::Range,
                          "-100,100,0.01,or_lesser,or_greater"),
            "set_param", "get_param", PARAM_TANGENTIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "tangential_accel_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_TANGENTIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "tangential_accel_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_TANGENTIAL_ACCEL);
    ADD_GROUP("Damping", "");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "damping", PropertyHint::Range, "0,100,0.01,or_greater"), "set_param",
            "get_param", PARAM_DAMPING);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "damping_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_DAMPING);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "damping_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_DAMPING);
    ADD_GROUP("Angle", "");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "angle", PropertyHint::Range, "-720,720,0.1,or_lesser,or_greater"),
            "set_param", "get_param", PARAM_ANGLE);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "angle_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_ANGLE);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "angle_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_ANGLE);
    ADD_GROUP("Scale", "");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "scale_amount", PropertyHint::Range, "0,1000,0.01,or_greater"),
            "set_param", "get_param", PARAM_SCALE);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "scale_amount_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_SCALE);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "scale_amount_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_SCALE);
    ADD_GROUP("Color", "");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "color"), "set_color", "get_color");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "color_ramp", PropertyHint::ResourceType, "Gradient"),
            "set_color_ramp", "get_color_ramp");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "color_initial_ramp", PropertyHint::ResourceType, "Gradient"),
            "set_color_initial_ramp", "get_color_initial_ramp");

    ADD_GROUP("Hue Variation", "hue_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "hue_variation", PropertyHint::Range, "-1,1,0.01"), "set_param",
            "get_param", PARAM_HUE_VARIATION);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "hue_variation_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_HUE_VARIATION);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "hue_variation_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_HUE_VARIATION);
    ADD_GROUP("Animation", "anim_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_speed", PropertyHint::Range, "0,128,0.01,or_greater"),
            "set_param", "get_param", PARAM_ANIM_SPEED);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_speed_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_ANIM_SPEED);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "anim_speed_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_ANIM_SPEED);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_offset", PropertyHint::Range, "0,1,0.01"), "set_param",
            "get_param", PARAM_ANIM_OFFSET);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_offset_random", PropertyHint::Range, "0,1,0.01"),
            "set_param_randomness", "get_param_randomness", PARAM_ANIM_OFFSET);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "anim_offset_curve", PropertyHint::ResourceType, "Curve"),
            "set_param_curve", "get_param_curve", PARAM_ANIM_OFFSET);

    BIND_ENUM_CONSTANT(PARAM_INITIAL_LINEAR_VELOCITY);
    BIND_ENUM_CONSTANT(PARAM_ANGULAR_VELOCITY);
    BIND_ENUM_CONSTANT(PARAM_ORBIT_VELOCITY);
    BIND_ENUM_CONSTANT(PARAM_LINEAR_ACCEL);
    BIND_ENUM_CONSTANT(PARAM_RADIAL_ACCEL);
    BIND_ENUM_CONSTANT(PARAM_TANGENTIAL_ACCEL);
    BIND_ENUM_CONSTANT(PARAM_DAMPING);
    BIND_ENUM_CONSTANT(PARAM_ANGLE);
    BIND_ENUM_CONSTANT(PARAM_SCALE);
    BIND_ENUM_CONSTANT(PARAM_HUE_VARIATION);
    BIND_ENUM_CONSTANT(PARAM_ANIM_SPEED);
    BIND_ENUM_CONSTANT(PARAM_ANIM_OFFSET);
    BIND_ENUM_CONSTANT(PARAM_MAX);

    BIND_ENUM_CONSTANT(FLAG_ALIGN_Y_TO_VELOCITY);
    BIND_ENUM_CONSTANT(FLAG_ROTATE_Y); // Unused, but exposed for consistency with 3D.
    BIND_ENUM_CONSTANT(FLAG_DISABLE_Z); // Unused, but exposed for consistency with 3D.
    BIND_ENUM_CONSTANT(FLAG_MAX);

    BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINT);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_SPHERE);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_RECTANGLE);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINTS);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_DIRECTED_POINTS);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_MAX);
}

CPUParticles2D::CPUParticles2D() {

    inactive_time = 0;
    frame_remainder = 0;
    cycle = 0;
    redraw = false;
    emitting = false;

    mesh = RenderingServer::get_singleton()->mesh_create();
    multimesh = RenderingServer::get_singleton()->multimesh_create();
    RenderingServer::get_singleton()->multimesh_set_mesh(multimesh, mesh);

    set_emitting(true);
    set_one_shot(false);
    set_amount(8);
    set_lifetime(1);
    set_fixed_fps(0);
    set_fractional_delta(true);
    set_pre_process_time(0);
    set_explosiveness_ratio(0);
    set_randomness_ratio(0);
    set_lifetime_randomness(0);
    set_use_local_coordinates(true);

    set_draw_order(DRAW_ORDER_INDEX);
    set_speed_scale(1);

    set_direction(Vector2(1, 0));
    set_spread(45);
    set_param(PARAM_INITIAL_LINEAR_VELOCITY, 0);
    set_param(PARAM_ANGULAR_VELOCITY, 0);
    set_param(PARAM_ORBIT_VELOCITY, 0);
    set_param(PARAM_LINEAR_ACCEL, 0);
    set_param(PARAM_RADIAL_ACCEL, 0);
    set_param(PARAM_TANGENTIAL_ACCEL, 0);
    set_param(PARAM_DAMPING, 0);
    set_param(PARAM_ANGLE, 0);
    set_param(PARAM_SCALE, 1);
    set_param(PARAM_HUE_VARIATION, 0);
    set_param(PARAM_ANIM_SPEED, 0);
    set_param(PARAM_ANIM_OFFSET, 0);
    set_emission_shape(EMISSION_SHAPE_POINT);
    set_emission_sphere_radius(1);
    set_emission_rect_extents(Vector2(1, 1));

    set_gravity(Vector2(0, 98));

    for (int i = 0; i < PARAM_MAX; i++) {
        set_param_randomness(Parameter(i), 0);
    }

    for (int i = 0; i < FLAG_MAX; i++) {
        flags[i] = false;
    }

    set_color(Color(1, 1, 1, 1));


    _update_mesh_texture();
}

CPUParticles2D::~CPUParticles2D() {
    RenderingServer::get_singleton()->free_rid(multimesh);
    RenderingServer::get_singleton()->free_rid(mesh);

}

