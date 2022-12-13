/*************************************************************************/
/*  particles_material.cpp                                               */
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

#include "particles_material.h"

#include "core/version.h"
#include "core/object_tooling.h"
#include "scene/resources/curve_texture.h"
#include "servers/rendering_server.h"
#include "core/method_bind.h"
#include "core/os/mutex.h"

Mutex ParticlesMaterial::material_mutex;

namespace  {

struct ParticleShaderNames {
    ParticleShaderNames() :
        direction("direction"),
        spread("spread"),
        flatness("flatness"),
        initial_linear_velocity("initial_linear_velocity"),
        initial_angle("initial_angle"),
        angular_velocity("angular_velocity"),
        orbit_velocity("orbit_velocity"),
        linear_accel("linear_accel"),
        radial_accel("radial_accel"),
        tangent_accel("tangent_accel"),
        damping("damping"),
        scale("scale"),
        hue_variation("hue_variation"),
        anim_speed("anim_speed"),
        anim_offset("anim_offset"),
        initial_linear_velocity_random("initial_linear_velocity_random"),
        initial_angle_random("initial_angle_random"),
        angular_velocity_random("angular_velocity_random"),
        orbit_velocity_random("orbit_velocity_random"),
        linear_accel_random("linear_accel_random"),
        radial_accel_random("radial_accel_random"),
        tangent_accel_random("tangent_accel_random"),
        damping_random("damping_random"),
        scale_random("scale_random"),
        hue_variation_random("hue_variation_random"),
        anim_speed_random("anim_speed_random"),
        anim_offset_random("anim_offset_random"),

        angle_texture("angle_texture"),
        angular_velocity_texture("angular_velocity_texture"),
        orbit_velocity_texture("orbit_velocity_texture"),
        linear_accel_texture("linear_accel_texture"),
        radial_accel_texture("radial_accel_texture"),
        tangent_accel_texture("tangent_accel_texture"),
        damping_texture("damping_texture"),
        scale_texture("scale_texture"),
        hue_variation_texture("hue_variation_texture"),
        anim_speed_texture("anim_speed_texture"),
        anim_offset_texture("anim_offset_texture"),
        color("color_value"),
        color_ramp("color_ramp"),
        color_initial_ramp("color_initial_ramp"),
        emission_sphere_radius("emission_sphere_radius"),
        emission_box_extents("emission_box_extents"),
        emission_texture_point_count("emission_texture_point_count"),
        emission_texture_points("emission_texture_points"),
        emission_texture_normal("emission_texture_normal"),
        emission_texture_color("emission_texture_color"),
        emission_ring_radius("emission_ring_radius"),
        emission_ring_inner_radius("emission_ring_inner_radius"),
        emission_ring_height("emission_ring_height"),
        emission_ring_axis("emission_ring_axis"),
        trail_divisor("trail_divisor"),
        trail_size_modifier("trail_size_modifier"),
        trail_color_modifier("trail_color_modifier"),
        gravity("gravity"),
        lifetime_randomness("lifetime_randomness")
    {

    }
    const StringName direction;
    const StringName spread;
    const StringName flatness;
    const StringName initial_linear_velocity;
    const StringName initial_angle;
    const StringName angular_velocity;
    const StringName orbit_velocity;
    const StringName linear_accel;
    const StringName radial_accel;
    const StringName tangent_accel;
    const StringName damping;
    const StringName scale;
    const StringName hue_variation;
    const StringName anim_speed;
    const StringName anim_offset;

    const StringName initial_linear_velocity_random;
    const StringName initial_angle_random;
    const StringName angular_velocity_random;
    const StringName orbit_velocity_random;
    const StringName linear_accel_random;
    const StringName radial_accel_random;
    const StringName tangent_accel_random;
    const StringName damping_random;
    const StringName scale_random;
    const StringName hue_variation_random;
    const StringName anim_speed_random;
    const StringName anim_offset_random;

    const StringName angle_texture;
    const StringName angular_velocity_texture;
    const StringName orbit_velocity_texture;
    const StringName linear_accel_texture;
    const StringName radial_accel_texture;
    const StringName tangent_accel_texture;
    const StringName damping_texture;
    const StringName scale_texture;
    const StringName hue_variation_texture;
    const StringName anim_speed_texture;
    const StringName anim_offset_texture;

    const StringName color;
    const StringName color_ramp;
    const StringName color_initial_ramp;

    const StringName emission_sphere_radius;
    const StringName emission_box_extents;
    const StringName emission_texture_point_count;
    const StringName emission_texture_points;
    const StringName emission_texture_normal;
    const StringName emission_texture_color;

    const StringName emission_ring_radius;
    const StringName emission_ring_inner_radius;
    const StringName emission_ring_height;
    const StringName emission_ring_axis;
    const StringName trail_divisor;
    const StringName trail_size_modifier;
    const StringName trail_color_modifier;

    const StringName gravity;

    const StringName lifetime_randomness;
};
// Not using deque here to make use of vector's fast erase idiom.
Vector<ParticlesMaterial *> s_particle_dirty_materials;
ParticleShaderNames *particle_shader_names=nullptr;

} // end of anonmous namespace

HashMap<ParticlesMaterial::MaterialKey, ParticlesMaterial::ShaderData> ParticlesMaterial::shader_map;


IMPL_GDCLASS(ParticlesMaterial)
VARIANT_ENUM_CAST(ParticlesMaterial::Parameter);
VARIANT_ENUM_CAST(ParticlesMaterial::Flags);
VARIANT_ENUM_CAST(ParticlesMaterial::EmissionShape);

void ParticlesMaterial::init_shaders() {
    particle_shader_names = memnew(ParticleShaderNames);
}

void ParticlesMaterial::finish_shaders() {
    s_particle_dirty_materials.clear();

    memdelete(particle_shader_names);
}

void ParticlesMaterial::_update_shader() {

    is_dirty_element = false;
    MaterialKey mk = _compute_key();
    if (mk.key == current_key.key)
        return; //no update required in the end

    if (shader_map.contains(current_key)) {
        shader_map[current_key].users--;
        if (shader_map[current_key].users == 0) {
            //deallocate shader, as it's no longer in use
            RenderingServer::get_singleton()->free_rid(shader_map[current_key].shader);
            shader_map.erase(current_key);
        }
    }

    current_key = mk;

    if (shader_map.contains(mk)) {

        RenderingServer::get_singleton()->material_set_shader(_get_material(), shader_map[mk].shader);
        shader_map[mk].users++;
        return;
    }

    //must create a shader!

    String code = "// NOTE: Shader automatically converted from " VERSION_NAME " " VERSION_FULL_CONFIG "'s ParticlesMaterial.\n\n";

    code += "shader_type particles;\n";

    code += R"raw(uniform vec3 direction;
    uniform float spread;
    uniform float flatness;
    uniform float initial_linear_velocity;
    uniform float initial_angle;
    uniform float angular_velocity;
    uniform float orbit_velocity;
    uniform float linear_accel;
    uniform float radial_accel;
    uniform float tangent_accel;
    uniform float damping;
    uniform float scale;
    uniform float hue_variation;
    uniform float anim_speed;
    uniform float anim_offset;

    uniform float initial_linear_velocity_random;
    uniform float initial_angle_random;
    uniform float angular_velocity_random;
    uniform float orbit_velocity_random;
    uniform float linear_accel_random;
    uniform float radial_accel_random;
    uniform float tangent_accel_random;
    uniform float damping_random;
    uniform float scale_random;
    uniform float hue_variation_random;
    uniform float anim_speed_random;
    uniform float anim_offset_random;
    uniform float lifetime_randomness;)raw";

    switch (emission_shape) {
        case EMISSION_SHAPE_POINT: {
            //do none
        } break;
        case EMISSION_SHAPE_SPHERE: {
            code += "uniform float emission_sphere_radius;\n";
        } break;
        case EMISSION_SHAPE_BOX: {
            code += "uniform vec3 emission_box_extents;\n";
        } break;
        case EMISSION_SHAPE_DIRECTED_POINTS: {
            code += "uniform sampler2D emission_texture_normal : hint_black;\n";
            [[fallthrough]];
        }
        case EMISSION_SHAPE_POINTS: {
            code += "uniform sampler2D emission_texture_points : hint_black;\n";
            code += "uniform int emission_texture_point_count;\n";
            if (emission_color_texture) {
                code += "uniform sampler2D emission_texture_color : hint_white;\n";
            }
        } break;
        case EMISSION_SHAPE_RING: {
            code += "uniform float ring_radius = 2.0;\n";
            code += "uniform float ring_height = 1.0;\n";
            code += "uniform float ring_inner_radius = 0.0;\n";
            code += "uniform vec3 ring_axis = vec3(0.0, 0.0, 1.0);\n";
        } break;
        case EMISSION_SHAPE_MAX: { // Max value for validity check.
            break;
        }
    }

    code += "uniform vec4 color_value : hint_color;\n";

    code += "uniform int trail_divisor;\n";

    code += "uniform vec3 gravity;\n";

    if (color_ramp) {
        code += "uniform sampler2D color_ramp;\n";

    }
    if (color_initial_ramp) {
        code += "uniform sampler2D color_initial_ramp;\n";
    }
    if (tex_parameters[PARAM_INITIAL_LINEAR_VELOCITY])
        code += "uniform sampler2D linear_velocity_texture;\n";
    if (tex_parameters[PARAM_ORBIT_VELOCITY])
        code += "uniform sampler2D orbit_velocity_texture;\n";
    if (tex_parameters[PARAM_ANGULAR_VELOCITY])
        code += "uniform sampler2D angular_velocity_texture;\n";
    if (tex_parameters[PARAM_LINEAR_ACCEL])
        code += "uniform sampler2D linear_accel_texture;\n";
    if (tex_parameters[PARAM_RADIAL_ACCEL])
        code += "uniform sampler2D radial_accel_texture;\n";
    if (tex_parameters[PARAM_TANGENTIAL_ACCEL])
        code += "uniform sampler2D tangent_accel_texture;\n";
    if (tex_parameters[PARAM_DAMPING])
        code += "uniform sampler2D damping_texture;\n";
    if (tex_parameters[PARAM_ANGLE])
        code += "uniform sampler2D angle_texture;\n";
    if (tex_parameters[PARAM_SCALE])
        code += "uniform sampler2D scale_texture;\n";
    if (tex_parameters[PARAM_HUE_VARIATION])
        code += "uniform sampler2D hue_variation_texture;\n";
    if (tex_parameters[PARAM_ANIM_SPEED])
        code += "uniform sampler2D anim_speed_texture;\n";
    if (tex_parameters[PARAM_ANIM_OFFSET])
        code += "uniform sampler2D anim_offset_texture;\n";

    if (trail_size_modifier) {
        code += "uniform sampler2D trail_size_modifier;\n";
    }

    if (trail_color_modifier) {
        code += "uniform sampler2D trail_color_modifier;\n";
    }

    //need a random function
    code += "\n\n";
    code += "float rand_from_seed(inout uint seed) {\n";
    code += "    int k;\n";
    code += "    int s = int(seed);\n";
    code += "    if (s == 0)\n";
    code += "    s = 305420679;\n";
    code += "    k = s / 127773;\n";
    code += "    s = 16807 * (s - k * 127773) - 2836 * k;\n";
    code += "    if (s < 0)\n";
    code += "        s += 2147483647;\n";
    code += "    seed = uint(s);\n";
    code += "    return float(seed % uint(65536)) / 65535.0;\n";
    code += "}\n";
    code += "\n";

    code += "float rand_from_seed_m1_p1(inout uint seed) {\n";
    code += "    return rand_from_seed(seed) * 2.0 - 1.0;\n";
    code += "}\n";
    code += "\n";

    //improve seed quality
    code += "uint hash(uint x) {\n";
    code += "    x = ((x >> uint(16)) ^ x) * uint(73244475);\n";
    code += "    x = ((x >> uint(16)) ^ x) * uint(73244475);\n";
    code += "    x = (x >> uint(16)) ^ x;\n";
    code += "    return x;\n";
    code += "}\n";
    code += "\n";

    code += R"(void vertex() {
    uint base_number = NUMBER / uint(trail_divisor);
    uint alt_seed = hash(base_number + uint(1) + RANDOM_SEED);
    float angle_rand = rand_from_seed(alt_seed);
    float scale_rand = rand_from_seed(alt_seed);
    float hue_rot_rand = rand_from_seed(alt_seed);
    float anim_offset_rand = rand_from_seed(alt_seed);
)";
    if (color_initial_ramp) {
        code += "    float color_initial_rand = rand_from_seed(alt_seed);\n";
    }
    code += "    float pi = 3.14159;\n";
    code += "    float degree_to_rad = pi / 180.0;\n";
    code += "\n";

    if (emission_shape == EMISSION_SHAPE_POINTS || emission_shape == EMISSION_SHAPE_DIRECTED_POINTS) {
        code += "    int point = min(emission_texture_point_count - 1, int(rand_from_seed(alt_seed) * float(emission_texture_point_count)));\n";
        code += "    ivec2 emission_tex_size = textureSize(emission_texture_points, 0);\n";
        code += "    ivec2 emission_tex_ofs = ivec2(point % emission_tex_size.x, point / emission_tex_size.x);\n";
    }
    code +=
R"raw(
    bool restart = false;
    float tv = 0.0;
    if (CUSTOM.y > CUSTOM.w) {
        restart = true;
        tv = 1.0f;
    }

    if (RESTART || restart) {
        uint alt_restart_seed = hash(base_number + uint(301184) + RANDOM_SEED);
)raw";

    if (tex_parameters[PARAM_INITIAL_LINEAR_VELOCITY])
        code += "        float tex_linear_velocity = textureLod(linear_velocity_texture, vec2(0.0, 0.0), 0.0).r;\n";
    else
        code += "        float tex_linear_velocity = 0.0;\n";

    if (tex_parameters[PARAM_ANGLE])
        code += "        float tex_angle = textureLod(angle_texture, vec2(0.0, 0.0), 0.0).r;\n";
    else
        code += "        float tex_angle = 0.0;\n";

    if (tex_parameters[PARAM_ANIM_OFFSET])
        code += "        float tex_anim_offset = textureLod(anim_offset_texture, vec2(0.0, 0.0), 0.0).r;\n";
    else
        code += "        float tex_anim_offset = 0.0;\n";

    code += "        float spread_rad = spread * degree_to_rad;\n";

    if (flags[FLAG_DISABLE_Z]) {
        code +=
R"(        {
            float angle1_rad = rand_from_seed_m1_p1(alt_restart_seed) * spread_rad;
            angle1_rad += direction.x != 0.0 ? atan(direction.y, direction.x) : sign(direction.y) * (pi / 2.0);
            vec3 rot = vec3(cos(angle1_rad), sin(angle1_rad), 0.0);
            VELOCITY = rot * initial_linear_velocity * mix(1.0, rand_from_seed(alt_restart_seed), initial_linear_velocity_random);
        }
)";

    } else {
        //initiate velocity spread in 3D
        code += "        {\n";
        code += "            float angle1_rad = rand_from_seed_m1_p1(alt_restart_seed) * spread_rad;\n";
        code += "            float angle2_rad = rand_from_seed_m1_p1(alt_restart_seed) * spread_rad * (1.0 - flatness);\n";
        code += "        vec3 direction_xz = vec3(sin(angle1_rad), 0.0, cos(angle1_rad));\n";
        code += "        vec3 direction_yz = vec3(0.0, sin(angle2_rad), cos(angle2_rad));\n";
        code += "        direction_yz.z = direction_yz.z / max(0.0001,sqrt(abs(direction_yz.z))); // better uniform distribution\n";
        code += "            vec3 spread_direction = vec3(direction_xz.x * direction_yz.z, direction_yz.y, direction_xz.z * direction_yz.z);\n";
        code += "            vec3 direction_nrm = length(direction) > 0.0 ? normalize(direction) : vec3(0.0, 0.0, 1.0);\n";
        code += "            // rotate spread to direction\n";
        code += "            vec3 binormal = cross(vec3(0.0, 1.0, 0.0), direction_nrm);\n";
        code += "            if (length(binormal) < 0.0001) {\n";
        code += "                // direction is parallel to Y. Choose Z as the binormal.\n";
        code += "                binormal = vec3(0.0, 0.0, 1.0);\n";
        code += "            }\n";
        code += "            binormal = normalize(binormal);\n";
        code += "            vec3 normal = cross(binormal, direction_nrm);\n";
        code += "            spread_direction = binormal * spread_direction.x + normal * spread_direction.y + direction_nrm * spread_direction.z;\n";
        code += "            VELOCITY = spread_direction * initial_linear_velocity * mix(1.0, rand_from_seed(alt_restart_seed), initial_linear_velocity_random);\n";
        code += "        }\n";    }

    code += "        float base_angle = (initial_angle + tex_angle) * mix(1.0, angle_rand, initial_angle_random);\n";
    code += "        CUSTOM.x = base_angle * degree_to_rad;\n"; // angle
    code += "        CUSTOM.y = 0.0;\n"; // phase
    code += "        CUSTOM.w = (1.0 - lifetime_randomness * rand_from_seed(alt_restart_seed));\n";
    code += "        CUSTOM.z = (anim_offset + tex_anim_offset) * mix(1.0, anim_offset_rand, anim_offset_random);\n"; // animation offset (0-1)

    switch (emission_shape) {
        case EMISSION_SHAPE_POINT: {
            //do none
        } break;
        case EMISSION_SHAPE_SPHERE: {
            code += "        float s = rand_from_seed(alt_restart_seed) * 2.0 - 1.0;\n";
            code += "        float t = rand_from_seed(alt_restart_seed) * 2.0 * pi;\n";
            code += "        float radius = emission_sphere_radius * sqrt(1.0 - s * s);\n";
            code += "        TRANSFORM[3].xyz = vec3(radius * cos(t), radius * sin(t), emission_sphere_radius * s);\n";
        } break;
        case EMISSION_SHAPE_BOX: {
            code += "        TRANSFORM[3].xyz = vec3(rand_from_seed(alt_restart_seed) * 2.0 - 1.0, rand_from_seed(alt_restart_seed) * 2.0 - 1.0, rand_from_seed(alt_restart_seed) * 2.0 - 1.0) * emission_box_extents;\n";
        } break;
        case EMISSION_SHAPE_POINTS:
        case EMISSION_SHAPE_DIRECTED_POINTS: {
            code += "        TRANSFORM[3].xyz = texelFetch(emission_texture_points, emission_tex_ofs, 0).xyz;\n";

            if (emission_shape == EMISSION_SHAPE_DIRECTED_POINTS) {
                code += "        {\n";
                if (flags[FLAG_DISABLE_Z]) {

                    code += "        mat2 rotm;";
                    code += "        rotm[0] = texelFetch(emission_texture_normal, emission_tex_ofs, 0).xy;\n";
                    code += "        rotm[1] = rotm[0].yx * vec2(1.0, -1.0);\n";
                    code += "        VELOCITY.xy = rotm * VELOCITY.xy;\n";
                } else {
                    code += "        vec3 normal = texelFetch(emission_texture_normal, emission_tex_ofs, 0).xyz;\n";
                    code += "        vec3 v0 = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);\n";
                    code += "        vec3 tangent = normalize(cross(v0, normal));\n";
                    code += "        vec3 bitangent = normalize(cross(tangent, normal));\n";
                    code += "        VELOCITY = mat3(tangent, bitangent, normal) * VELOCITY;\n";
                }
                code += "        }\n";
            }
        } break;
        case EMISSION_SHAPE_RING: {
            code += "        float ring_spawn_angle = rand_from_seed(alt_restart_seed) * 2.0 * pi;\n";
            code += "        float ring_random_radius = rand_from_seed(alt_restart_seed) * (ring_radius - ring_inner_radius) + ring_inner_radius;\n";
            code += "        vec3 axis = normalize(ring_axis);\n";
            code += "        vec3 ortho_axis = vec3(0.0);\n";
            code += "        if (axis == vec3(1.0, 0.0, 0.0)) {\n";
            code += "            ortho_axis = cross(axis, vec3(0.0, 1.0, 0.0));\n";
            code += "        } else {\n";
            code += "             ortho_axis = cross(axis, vec3(1.0, 0.0, 0.0));\n";
            code += "        }\n";
            code += "        ortho_axis = normalize(ortho_axis);\n";
            code += "        float s = sin(ring_spawn_angle);\n";
            code += "        float c = cos(ring_spawn_angle);\n";
            code += "        float oc = 1.0 - c;\n";
            code += "        ortho_axis = mat3(\n";
            code += "            vec3(c + axis.x * axis.x * oc, axis.x * axis.y * oc - axis.z * s, axis.x * axis.z *oc + axis.y * s),\n";
            code += "            vec3(axis.x * axis.y * oc + s * axis.z, c + axis.y * axis.y * oc, axis.y * axis.z * oc - axis.x * s),\n";
            code += "            vec3(axis.z * axis.x * oc - axis.y * s, axis.z * axis.y * oc + axis.x * s, c + axis.z * axis.z * oc)\n";
            code += "            ) * ortho_axis;\n";
            code += "        ortho_axis = normalize(ortho_axis);\n";
            code += "        TRANSFORM[3].xyz = ortho_axis * ring_random_radius + (rand_from_seed(alt_restart_seed) * ring_height - ring_height / 2.0) * axis;\n";
        } break;
        case EMISSION_SHAPE_MAX: { // Max value for validity check.
            break;
        }
    }
    code += "        VELOCITY = (EMISSION_TRANSFORM * vec4(VELOCITY, 0.0)).xyz;\n";
    code += "        TRANSFORM = EMISSION_TRANSFORM * TRANSFORM;\n";
    if (flags[FLAG_DISABLE_Z]) {
        code += "        VELOCITY.z = 0.0;\n";
        code += "        TRANSFORM[3].z = 0.0;\n";
    }

    code += "    } else {\n";

    code += "        CUSTOM.y += DELTA / LIFETIME;\n";
    code += "        tv = CUSTOM.y / CUSTOM.w;\n";
    if (tex_parameters[PARAM_INITIAL_LINEAR_VELOCITY])
        code += "        float tex_linear_velocity = textureLod(linear_velocity_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "        float tex_linear_velocity = 0.0;\n";

    if (flags[FLAG_DISABLE_Z]) {

        if (tex_parameters[PARAM_ORBIT_VELOCITY])
            code += "        float tex_orbit_velocity = textureLod(orbit_velocity_texture, vec2(tv, 0.0), 0.0).r;\n";
        else
            code += "        float tex_orbit_velocity = 0.0;\n";
    }

    if (tex_parameters[PARAM_ANGULAR_VELOCITY])
        code += "        float tex_angular_velocity = textureLod(angular_velocity_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "        float tex_angular_velocity = 0.0;\n";

    if (tex_parameters[PARAM_LINEAR_ACCEL])
        code += "        float tex_linear_accel = textureLod(linear_accel_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "        float tex_linear_accel = 0.0;\n";

    if (tex_parameters[PARAM_RADIAL_ACCEL])
        code += "        float tex_radial_accel = textureLod(radial_accel_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "        float tex_radial_accel = 0.0;\n";

    if (tex_parameters[PARAM_TANGENTIAL_ACCEL])
        code += "        float tex_tangent_accel = textureLod(tangent_accel_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "        float tex_tangent_accel = 0.0;\n";

    if (tex_parameters[PARAM_DAMPING])
        code += "        float tex_damping = textureLod(damping_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "        float tex_damping = 0.0;\n";

    if (tex_parameters[PARAM_ANGLE])
        code += "        float tex_angle = textureLod(angle_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "        float tex_angle = 0.0;\n";

    if (tex_parameters[PARAM_ANIM_SPEED])
        code += "        float tex_anim_speed = textureLod(anim_speed_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "        float tex_anim_speed = 0.0;\n";

    if (tex_parameters[PARAM_ANIM_OFFSET])
        code += "        float tex_anim_offset = textureLod(anim_offset_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "        float tex_anim_offset = 0.0;\n";

    code += "        vec3 force = gravity;\n";
    code += "        vec3 pos = TRANSFORM[3].xyz;";
    if (flags[FLAG_DISABLE_Z]) {
        code += "        pos.z = 0.0;";
    }
    code += R"(
        // apply linear acceleration
        force += length(VELOCITY) > 0.0 ? normalize(VELOCITY) * (linear_accel + tex_linear_accel) * mix(1.0, rand_from_seed(alt_seed), linear_accel_random) : vec3(0.0);
        // apply radial acceleration
        vec3 org = EMISSION_TRANSFORM[3].xyz;
        vec3 diff = pos - org;
        force += length(diff) > 0.0 ? normalize(diff) * (radial_accel + tex_radial_accel) * mix(1.0, rand_from_seed(alt_seed), radial_accel_random) : vec3(0.0);
        // apply tangential acceleration;
    )";
    if (flags[FLAG_DISABLE_Z]) {
        code += "        force += length(diff.yx) > 0.0 ? vec3(normalize(diff.yx * vec2(-1.0, 1.0)), 0.0) * ((tangent_accel + tex_tangent_accel) * mix(1.0, rand_from_seed(alt_seed), tangent_accel_random)) : vec3(0.0);\n";

    } else {
        code += "        vec3 crossDiff = cross(normalize(diff), normalize(gravity));\n";
        code += "        force += length(crossDiff) > 0.0 ? normalize(crossDiff) * ((tangent_accel + tex_tangent_accel) * mix(1.0, rand_from_seed(alt_seed), tangent_accel_random)) : vec3(0.0);\n";
    }
    code += R"(
        // apply attractor forces
        VELOCITY += force * DELTA;
        // orbit velocity
    )";
    if (flags[FLAG_DISABLE_Z]) {
        code += R"(
        float orbit_amount = (orbit_velocity + tex_orbit_velocity) * mix(1.0, rand_from_seed(alt_seed), orbit_velocity_random);
        if (orbit_amount != 0.0) {
             float ang = orbit_amount * DELTA * pi * 2.0;
             mat2 rot = mat2(vec2(cos(ang), -sin(ang)), vec2(sin(ang), cos(ang)));
             TRANSFORM[3].xy -= diff.xy;
             TRANSFORM[3].xy += rot * diff.xy;
        }
        )";
    }

    if (tex_parameters[PARAM_INITIAL_LINEAR_VELOCITY]) {
        code += "        VELOCITY = normalize(VELOCITY) * tex_linear_velocity;\n";
    }
    code += "        if (damping + tex_damping > 0.0) {\n";
    code += "            float v = length(VELOCITY);\n";
    code += "            float damp = (damping + tex_damping) * mix(1.0, rand_from_seed(alt_seed), damping_random);\n";
    code += "            v -= damp * DELTA;\n";
    code += "            if (v < 0.0) {\n";
    code += "                VELOCITY = vec3(0.0);\n";
    code += "            } else {\n";
    code += "                VELOCITY = normalize(VELOCITY) * v;\n";
    code += "            }\n";
    code += "        }\n";
    code += "        float base_angle = (initial_angle + tex_angle) * mix(1.0, angle_rand, initial_angle_random);\n";
    code += "        base_angle += CUSTOM.y * LIFETIME * (angular_velocity + tex_angular_velocity) * mix(1.0, rand_from_seed(alt_seed) * 2.0 - 1.0, angular_velocity_random);\n";
    code += "        CUSTOM.x = base_angle * degree_to_rad;\n"; // angle
    code += "        CUSTOM.z = (anim_offset + tex_anim_offset) * mix(1.0, anim_offset_rand, anim_offset_random) + tv * (anim_speed + tex_anim_speed) * mix(1.0, rand_from_seed(alt_seed), anim_speed_random);\n"; // angle
    code += "    }\n";
    // apply color
    // apply hue rotation
    if (tex_parameters[PARAM_SCALE])
        code += "    float tex_scale = textureLod(scale_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "    float tex_scale = 1.0;\n";

    if (tex_parameters[PARAM_HUE_VARIATION])
        code += "    float tex_hue_variation = textureLod(hue_variation_texture, vec2(tv, 0.0), 0.0).r;\n";
    else
        code += "    float tex_hue_variation = 0.0;\n";

    code += "    float hue_rot_angle = (hue_variation + tex_hue_variation) * pi * 2.0 * mix(1.0, hue_rot_rand * 2.0 - 1.0, hue_variation_random);\n";
    code += "    float hue_rot_c = cos(hue_rot_angle);\n";
    code += "    float hue_rot_s = sin(hue_rot_angle);\n";
    code += "    mat4 hue_rot_mat = mat4(vec4(0.299, 0.587, 0.114, 0.0),\n";
    code += "            vec4(0.299, 0.587, 0.114, 0.0),\n";
    code += "            vec4(0.299, 0.587, 0.114, 0.0),\n";
    code += "            vec4(0.000, 0.000, 0.000, 1.0)) +\n";
    code += "        mat4(vec4(0.701, -0.587, -0.114, 0.0),\n";
    code += "            vec4(-0.299, 0.413, -0.114, 0.0),\n";
    code += "            vec4(-0.300, -0.588, 0.886, 0.0),\n";
    code += "            vec4(0.000, 0.000, 0.000, 0.0)) * hue_rot_c +\n";
    code += "        mat4(vec4(0.168, 0.330, -0.497, 0.0),\n";
    code += "            vec4(-0.328, 0.035,  0.292, 0.0),\n";
    code += "            vec4(1.250, -1.050, -0.203, 0.0),\n";
    code += "            vec4(0.000, 0.000, 0.000, 0.0)) * hue_rot_s;\n";
    if (color_ramp) {
        code += "    COLOR = hue_rot_mat * textureLod(color_ramp, vec2(tv, 0.0), 0.0) * color_value;\n";
    } else {
        code += "    COLOR = hue_rot_mat * color_value;\n";
    }
    if (color_initial_ramp) {
        code += "    vec4 start_color = textureLod(color_initial_ramp, vec2(color_initial_rand, 0.0), 0.0);\n";
        code += "    COLOR *= start_color;\n";
    }
    if (emission_color_texture && (emission_shape == EMISSION_SHAPE_POINTS || emission_shape == EMISSION_SHAPE_DIRECTED_POINTS)) {
        code += "    COLOR *= texelFetch(emission_texture_color, emission_tex_ofs, 0);\n";
    }
    if (trail_color_modifier) {
        code += "    if (trail_divisor > 1) {\n";
        code += "        COLOR *= textureLod(trail_color_modifier, vec2(float(int(NUMBER) % trail_divisor) / float(trail_divisor - 1), 0.0), 0.0);\n";
        code += "    }\n";
    }
    code += "\n";

    if (flags[FLAG_DISABLE_Z]) {

        if (flags[FLAG_ALIGN_Y_TO_VELOCITY]) {
            code += "    if (length(VELOCITY) > 0.0) {\n";
            code += "        TRANSFORM[1].xyz = normalize(VELOCITY);\n";
            code += "    } else {\n";
            code += "        TRANSFORM[1].xyz = normalize(TRANSFORM[1].xyz);\n";
            code += "    }\n";
            code += "    TRANSFORM[0].xyz = normalize(cross(TRANSFORM[1].xyz, TRANSFORM[2].xyz));\n";
            code += "    TRANSFORM[2] = vec4(0.0, 0.0, 1.0, 0.0);\n";
        } else {
            code += "    TRANSFORM[0] = vec4(cos(CUSTOM.x), -sin(CUSTOM.x), 0.0, 0.0);\n";
            code += "    TRANSFORM[1] = vec4(sin(CUSTOM.x), cos(CUSTOM.x), 0.0, 0.0);\n";
            code += "    TRANSFORM[2] = vec4(0.0, 0.0, 1.0, 0.0);\n";
        }

    } else {
        // orient particle Y towards velocity
        if (flags[FLAG_ALIGN_Y_TO_VELOCITY]) {
            code += "    if (length(VELOCITY) > 0.0) {\n";
            code += "        TRANSFORM[1].xyz = normalize(VELOCITY);\n";
            code += "    } else {\n";
            code += "        TRANSFORM[1].xyz = normalize(TRANSFORM[1].xyz);\n";
            code += "    }\n";
            code += "    if (TRANSFORM[1].xyz == normalize(TRANSFORM[0].xyz)) {\n";
            code += "        TRANSFORM[0].xyz = normalize(cross(normalize(TRANSFORM[1].xyz), normalize(TRANSFORM[2].xyz)));\n";
            code += "        TRANSFORM[2].xyz = normalize(cross(normalize(TRANSFORM[0].xyz), normalize(TRANSFORM[1].xyz)));\n";
            code += "    } else {\n";
            code += "        TRANSFORM[2].xyz = normalize(cross(normalize(TRANSFORM[0].xyz), normalize(TRANSFORM[1].xyz)));\n";
            code += "        TRANSFORM[0].xyz = normalize(cross(normalize(TRANSFORM[1].xyz), normalize(TRANSFORM[2].xyz)));\n";
            code += "    }\n";
        } else {
            code += "    TRANSFORM[0].xyz = normalize(TRANSFORM[0].xyz);\n";
            code += "    TRANSFORM[1].xyz = normalize(TRANSFORM[1].xyz);\n";
            code += "    TRANSFORM[2].xyz = normalize(TRANSFORM[2].xyz);\n";
        }
        // turn particle by rotation in Y
        if (flags[FLAG_ROTATE_Y]) {
            code += "    TRANSFORM = mat4(vec4(cos(CUSTOM.x), 0.0, -sin(CUSTOM.x), 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(sin(CUSTOM.x), 0.0, cos(CUSTOM.x), 0.0), TRANSFORM[3]);\n";
        }
    }
    //scale by scale
    code += "    float base_scale = tex_scale * mix(scale, 1.0, scale_random * scale_rand);\n";
    code += "    if (base_scale < 0.000001) {\n";
    code += "        base_scale = 0.000001;\n";
    code += "    }\n";
    if (trail_size_modifier) {
        code += "    if (trail_divisor > 1) {\n";
        code += "        base_scale *= textureLod(trail_size_modifier, vec2(float(int(NUMBER) % trail_divisor) / float(trail_divisor - 1), 0.0), 0.0).r;\n";
        code += "    }\n";
    }

    code += "    TRANSFORM[0].xyz *= base_scale;\n";
    code += "    TRANSFORM[1].xyz *= base_scale;\n";
    code += "    TRANSFORM[2].xyz *= base_scale;\n";
    if (flags[FLAG_DISABLE_Z]) {
        code += "    VELOCITY.z = 0.0;\n";
        code += "    TRANSFORM[3].z = 0.0;\n";
    }
    code += "    if (CUSTOM.y > CUSTOM.w) {";
    code += "        ACTIVE = false;\n";
    code += "    }\n";
    code += "}\n";
    code += "\n";

    ShaderData shader_data;
    shader_data.shader = RenderingServer::get_singleton()->shader_create();
    shader_data.users = 1;

    RenderingServer::get_singleton()->shader_set_code(shader_data.shader, code);

    shader_map[mk] = shader_data;

    RenderingServer::get_singleton()->material_set_shader(_get_material(), shader_data.shader);
}

void ParticlesMaterial::flush_changes() {
    MutexGuard guard(material_mutex);

    for(ParticlesMaterial *mat : s_particle_dirty_materials )
        mat->_update_shader();

    s_particle_dirty_materials.clear();
}

void ParticlesMaterial::_queue_shader_change() {

    MutexGuard guard(material_mutex);

    if (is_initialized && !is_dirty_element) {
        s_particle_dirty_materials.emplace_back(this);
        is_dirty_element= true;
    }

}

//bool ParticlesMaterial::_is_shader_dirty() const {

//    bool dirty = false;

//    if (material_mutex)
//        material_mutex->lock();

//    dirty = element.in_list();

//    if (material_mutex)
//        material_mutex->unlock();

//    return dirty;
//}

void ParticlesMaterial::set_direction(Vector3 p_direction) {

    direction = p_direction;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->direction, direction);
}

Vector3 ParticlesMaterial::get_direction() const {

    return direction;
}

void ParticlesMaterial::set_spread(float p_spread) {

    spread = p_spread;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->spread, p_spread);
}

float ParticlesMaterial::get_spread() const {

    return spread;
}

void ParticlesMaterial::set_flatness(float p_flatness) {

    flatness = p_flatness;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->flatness, p_flatness);
}
float ParticlesMaterial::get_flatness() const {

    return flatness;
}

void ParticlesMaterial::set_param(Parameter p_param, float p_value) {

    ERR_FAIL_INDEX(p_param, PARAM_MAX);

    parameters[p_param] = p_value;
    auto *rs(RenderingServer::get_singleton());
    switch (p_param) {
        case PARAM_INITIAL_LINEAR_VELOCITY: {
            rs->material_set_param(_get_material(), particle_shader_names->initial_linear_velocity, p_value);
        } break;
        case PARAM_ANGULAR_VELOCITY: {
            rs->material_set_param(_get_material(), particle_shader_names->angular_velocity, p_value);
        } break;
        case PARAM_ORBIT_VELOCITY: {
            rs->material_set_param(_get_material(), particle_shader_names->orbit_velocity, p_value);
        } break;
        case PARAM_LINEAR_ACCEL: {
            rs->material_set_param(_get_material(), particle_shader_names->linear_accel, p_value);
        } break;
        case PARAM_RADIAL_ACCEL: {
            rs->material_set_param(_get_material(), particle_shader_names->radial_accel, p_value);
        } break;
        case PARAM_TANGENTIAL_ACCEL: {
            rs->material_set_param(_get_material(), particle_shader_names->tangent_accel, p_value);
        } break;
        case PARAM_DAMPING: {
            rs->material_set_param(_get_material(), particle_shader_names->damping, p_value);
        } break;
        case PARAM_ANGLE: {
            rs->material_set_param(_get_material(), particle_shader_names->initial_angle, p_value);
        } break;
        case PARAM_SCALE: {
            rs->material_set_param(_get_material(), particle_shader_names->scale, p_value);
        } break;
        case PARAM_HUE_VARIATION: {
            rs->material_set_param(_get_material(), particle_shader_names->hue_variation, p_value);
        } break;
        case PARAM_ANIM_SPEED: {
            rs->material_set_param(_get_material(), particle_shader_names->anim_speed, p_value);
        } break;
        case PARAM_ANIM_OFFSET: {
            rs->material_set_param(_get_material(), particle_shader_names->anim_offset, p_value);
        } break;
        case PARAM_MAX: break; // Can't happen, but silences warning
    }
}
float ParticlesMaterial::get_param(Parameter p_param) const {

    ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);

    return parameters[p_param];
}

void ParticlesMaterial::set_param_randomness(Parameter p_param, float p_value) {

    ERR_FAIL_INDEX(p_param, PARAM_MAX);

    randomness[p_param] = p_value;
    auto *rs(RenderingServer::get_singleton());
    switch (p_param) {
        case PARAM_INITIAL_LINEAR_VELOCITY: {
            rs->material_set_param(_get_material(), particle_shader_names->initial_linear_velocity_random, p_value);
        } break;
        case PARAM_ANGULAR_VELOCITY: {
            rs->material_set_param(_get_material(), particle_shader_names->angular_velocity_random, p_value);
        } break;
        case PARAM_ORBIT_VELOCITY: {
            rs->material_set_param(_get_material(), particle_shader_names->orbit_velocity_random, p_value);
        } break;
        case PARAM_LINEAR_ACCEL: {
            rs->material_set_param(_get_material(), particle_shader_names->linear_accel_random, p_value);
        } break;
        case PARAM_RADIAL_ACCEL: {
            rs->material_set_param(_get_material(), particle_shader_names->radial_accel_random, p_value);
        } break;
        case PARAM_TANGENTIAL_ACCEL: {
            rs->material_set_param(_get_material(), particle_shader_names->tangent_accel_random, p_value);
        } break;
        case PARAM_DAMPING: {
            rs->material_set_param(_get_material(), particle_shader_names->damping_random, p_value);
        } break;
        case PARAM_ANGLE: {
            rs->material_set_param(_get_material(), particle_shader_names->initial_angle_random, p_value);
        } break;
        case PARAM_SCALE: {
            rs->material_set_param(_get_material(), particle_shader_names->scale_random, p_value);
        } break;
        case PARAM_HUE_VARIATION: {
            rs->material_set_param(_get_material(), particle_shader_names->hue_variation_random, p_value);
        } break;
        case PARAM_ANIM_SPEED: {
            rs->material_set_param(_get_material(), particle_shader_names->anim_speed_random, p_value);
        } break;
        case PARAM_ANIM_OFFSET: {
            rs->material_set_param(_get_material(), particle_shader_names->anim_offset_random, p_value);
        } break;
        case PARAM_MAX:
            break; // Can't happen, but silences warning
    }
}
float ParticlesMaterial::get_param_randomness(Parameter p_param) const {

    ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);

    return randomness[p_param];
}

void ParticlesMaterial::set_param_texture(Parameter p_param, const Ref<Texture> &p_texture) {

    ERR_FAIL_INDEX(p_param, PARAM_MAX);

    tex_parameters[p_param] = p_texture;
    const CurveRange range_to_set = c_default_curve_ranges[p_param];
    StringName texture_slot_name;
    switch (p_param) {
        case PARAM_INITIAL_LINEAR_VELOCITY: {
            //do none for this one
        } break;
        case PARAM_ANGULAR_VELOCITY: {
            texture_slot_name=particle_shader_names->angular_velocity_texture;
        } break;
        case PARAM_ORBIT_VELOCITY: {
            texture_slot_name=particle_shader_names->orbit_velocity_texture;
        } break;
        case PARAM_LINEAR_ACCEL: {
            texture_slot_name=particle_shader_names->linear_accel_texture;
        } break;
        case PARAM_RADIAL_ACCEL: {
            texture_slot_name=particle_shader_names->radial_accel_texture;
        } break;
        case PARAM_TANGENTIAL_ACCEL: {
            texture_slot_name=particle_shader_names->tangent_accel_texture;
        } break;
        case PARAM_DAMPING: {
            texture_slot_name=particle_shader_names->damping_texture;
        } break;
        case PARAM_ANGLE: {
            texture_slot_name=particle_shader_names->angle_texture;
        } break;
        case PARAM_SCALE: {
            texture_slot_name=particle_shader_names->scale_texture;
        } break;
        case PARAM_HUE_VARIATION: {
            texture_slot_name=particle_shader_names->hue_variation_texture;
        } break;
        case PARAM_ANIM_SPEED: {
            texture_slot_name=particle_shader_names->anim_speed_texture;
        } break;
        case PARAM_ANIM_OFFSET: {
            texture_slot_name=particle_shader_names->anim_offset_texture;
        } break;
        case PARAM_MAX: break; // Can't happen, but silences warning
    }
    if(texture_slot_name) {
        RenderingServer::get_singleton()->material_set_param(_get_material(), texture_slot_name, p_texture);
    }
    if(range_to_set.valid()) {
        Ref<CurveTexture> curve_tex = dynamic_ref_cast<CurveTexture>(p_texture);
        if(curve_tex)
            curve_tex->ensure_default_setup(range_to_set.curve_min, range_to_set.curve_max);
    }
    _queue_shader_change();
}
Ref<Texture> ParticlesMaterial::get_param_texture(Parameter p_param) const {

    ERR_FAIL_INDEX_V(p_param, PARAM_MAX, Ref<Texture>());

    return tex_parameters[p_param];
}

void ParticlesMaterial::set_color(const Color &p_color) {

    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->color, p_color);
    color = p_color;
}

Color ParticlesMaterial::get_color() const {

    return color;
}

void ParticlesMaterial::set_color_ramp(const Ref<Texture> &p_texture) {

    color_ramp = p_texture;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->color_ramp, p_texture);
    _queue_shader_change();
    Object_change_notify(this);
}

Ref<Texture> ParticlesMaterial::get_color_ramp() const {

    return color_ramp;
}

void ParticlesMaterial::set_color_initial_ramp(const Ref<Texture> &p_texture) {
    color_initial_ramp = p_texture;
    RenderingServer::get_singleton()->material_set_param(
            _get_material(), particle_shader_names->color_initial_ramp, p_texture);
    _queue_shader_change();
    Object_change_notify(this); // TODO: "color_initial_ramp" ??
}

Ref<Texture> ParticlesMaterial::get_color_initial_ramp() const {
    return color_initial_ramp;
}

void ParticlesMaterial::set_flag(Flags p_flag, bool p_enable) {
    ERR_FAIL_INDEX(p_flag, FLAG_MAX);
    flags[p_flag] = p_enable;
    _queue_shader_change();
    if (p_flag == FLAG_DISABLE_Z) {
        Object_change_notify(this);
    }
}

bool ParticlesMaterial::get_flag(Flags p_flag) const {
    ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
    return flags[p_flag];
}

void ParticlesMaterial::set_emission_shape(EmissionShape p_shape) {
    ERR_FAIL_INDEX(p_shape, EMISSION_SHAPE_MAX);

    emission_shape = p_shape;
    Object_change_notify(this);
    _queue_shader_change();
}

void ParticlesMaterial::set_emission_sphere_radius(float p_radius) {

    emission_sphere_radius = p_radius;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->emission_sphere_radius, p_radius);
}

void ParticlesMaterial::set_emission_box_extents(Vector3 p_extents) {

    emission_box_extents = p_extents;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->emission_box_extents, p_extents);
}

void ParticlesMaterial::set_emission_point_texture(const Ref<Texture> &p_points) {

    emission_point_texture = p_points;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->emission_texture_points, p_points);
}

void ParticlesMaterial::set_emission_normal_texture(const Ref<Texture> &p_normals) {

    emission_normal_texture = p_normals;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->emission_texture_normal, p_normals);
}

void ParticlesMaterial::set_emission_color_texture(const Ref<Texture> &p_colors) {

    emission_color_texture = p_colors;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->emission_texture_color, p_colors);
    _queue_shader_change();
}

void ParticlesMaterial::set_emission_point_count(int p_count) {

    emission_point_count = p_count;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->emission_texture_point_count, p_count);
}

void ParticlesMaterial::set_emission_ring_height(float p_height) {
    emission_ring_height = p_height;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->emission_ring_height, p_height);
}

void ParticlesMaterial::set_emission_ring_radius(float p_radius) {
    emission_ring_radius = p_radius;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->emission_ring_radius, p_radius);
}

void ParticlesMaterial::set_emission_ring_inner_radius(float p_offset) {
    emission_ring_inner_radius = p_offset;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->emission_ring_inner_radius, p_offset);
}

void ParticlesMaterial::set_emission_ring_axis(Vector3 p_axis) {
    emission_ring_axis = p_axis;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->emission_ring_axis, p_axis);
}

ParticlesMaterial::EmissionShape ParticlesMaterial::get_emission_shape() const {

    return emission_shape;
}

float ParticlesMaterial::get_emission_sphere_radius() const {

    return emission_sphere_radius;
}
Vector3 ParticlesMaterial::get_emission_box_extents() const {

    return emission_box_extents;
}
Ref<Texture> ParticlesMaterial::get_emission_point_texture() const {

    return emission_point_texture;
}
Ref<Texture> ParticlesMaterial::get_emission_normal_texture() const {

    return emission_normal_texture;
}

Ref<Texture> ParticlesMaterial::get_emission_color_texture() const {

    return emission_color_texture;
}

int ParticlesMaterial::get_emission_point_count() const {

    return emission_point_count;
}

float ParticlesMaterial::get_emission_ring_height() const {
    return emission_ring_height;
}

float ParticlesMaterial::get_emission_ring_inner_radius() const {
    return emission_ring_inner_radius;
}

float ParticlesMaterial::get_emission_ring_radius() const {
    return emission_ring_radius;
}

Vector3 ParticlesMaterial::get_emission_ring_axis() const {
    return emission_ring_axis;
}

void ParticlesMaterial::set_trail_divisor(int p_divisor) {

    trail_divisor = p_divisor;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->trail_divisor, p_divisor);
}

int ParticlesMaterial::get_trail_divisor() const {

    return trail_divisor;
}

void ParticlesMaterial::set_trail_size_modifier(const Ref<CurveTexture> &p_trail_size_modifier) {

    trail_size_modifier = p_trail_size_modifier;

    Ref<CurveTexture> curve = trail_size_modifier;
    if (curve) {
        curve->ensure_default_setup();
    }

    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->trail_size_modifier, curve);
    _queue_shader_change();
}

Ref<CurveTexture> ParticlesMaterial::get_trail_size_modifier() const {

    return trail_size_modifier;
}

void ParticlesMaterial::set_trail_color_modifier(const Ref<GradientTexture> &p_trail_color_modifier) {

    trail_color_modifier = p_trail_color_modifier;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->trail_color_modifier, p_trail_color_modifier);
    _queue_shader_change();
}

Ref<GradientTexture> ParticlesMaterial::get_trail_color_modifier() const {

    return trail_color_modifier;
}

void ParticlesMaterial::set_gravity(const Vector3 &p_gravity) {

    gravity = p_gravity;
    Vector3 gset = gravity;
    if (gset == Vector3()) {
        gset = Vector3(0, -0.000001f, 0); //as gravity is used as upvector in some calculations
    }
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->gravity, gset);
}

Vector3 ParticlesMaterial::get_gravity() const {

    return gravity;
}

void ParticlesMaterial::set_lifetime_randomness(float p_lifetime) {

    lifetime_randomness = p_lifetime;
    RenderingServer::get_singleton()->material_set_param(_get_material(), particle_shader_names->lifetime_randomness, lifetime_randomness);
}

float ParticlesMaterial::get_lifetime_randomness() const {

    return lifetime_randomness;
}

RenderingEntity ParticlesMaterial::get_shader_rid() const {

    ERR_FAIL_COND_V(!shader_map.contains(current_key), entt::null);
    return shader_map[current_key].shader;
}

void ParticlesMaterial::_validate_property(PropertyInfo &property) const {


    if (property.name == "emission_sphere_radius" && emission_shape != EMISSION_SHAPE_SPHERE) {
        property.usage = 0;
    }

    if (property.name == "emission_box_extents" && emission_shape != EMISSION_SHAPE_BOX) {
        property.usage = 0;
    }

    if ((property.name == "emission_point_texture" || property.name == "emission_color_texture") && (emission_shape != EMISSION_SHAPE_POINTS) && emission_shape != EMISSION_SHAPE_DIRECTED_POINTS) {
        property.usage = 0;
    }

    if (property.name == "emission_normal_texture" && emission_shape != EMISSION_SHAPE_DIRECTED_POINTS) {
        property.usage = 0;
    }

    if (property.name == "emission_point_count" && (emission_shape != EMISSION_SHAPE_POINTS && emission_shape != EMISSION_SHAPE_DIRECTED_POINTS)) {
        property.usage = 0;
    }

    if (emission_shape != EMISSION_SHAPE_RING && StringView(property.name).starts_with("emission_ring_")) {
        property.usage = 0;
    }

    if (StringUtils::begins_with(property.name,"orbit_") && !flags[FLAG_DISABLE_Z]) {
        property.usage = 0;
    }
}

RenderingServerEnums::ShaderMode ParticlesMaterial::get_shader_mode() const {

    return RenderingServerEnums::ShaderMode::PARTICLES;
}

void ParticlesMaterial::_bind_methods() {

    SE_BIND_METHOD(ParticlesMaterial,set_direction);
    SE_BIND_METHOD(ParticlesMaterial,get_direction);

    SE_BIND_METHOD(ParticlesMaterial,set_spread);
    SE_BIND_METHOD(ParticlesMaterial,get_spread);

    SE_BIND_METHOD(ParticlesMaterial,set_flatness);
    SE_BIND_METHOD(ParticlesMaterial,get_flatness);

    SE_BIND_METHOD(ParticlesMaterial,set_param);
    SE_BIND_METHOD(ParticlesMaterial,get_param);

    SE_BIND_METHOD(ParticlesMaterial,set_param_randomness);
    SE_BIND_METHOD(ParticlesMaterial,get_param_randomness);

    SE_BIND_METHOD(ParticlesMaterial,set_param_texture);
    SE_BIND_METHOD(ParticlesMaterial,get_param_texture);

    SE_BIND_METHOD(ParticlesMaterial,set_color);
    SE_BIND_METHOD(ParticlesMaterial,get_color);

    SE_BIND_METHOD(ParticlesMaterial,set_color_ramp);
    SE_BIND_METHOD(ParticlesMaterial,get_color_ramp);

    SE_BIND_METHOD(ParticlesMaterial,set_color_initial_ramp);
    SE_BIND_METHOD(ParticlesMaterial,get_color_initial_ramp);
    SE_BIND_METHOD(ParticlesMaterial,set_flag);
    SE_BIND_METHOD(ParticlesMaterial,get_flag);

    SE_BIND_METHOD(ParticlesMaterial,set_emission_shape);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_shape);

    SE_BIND_METHOD(ParticlesMaterial,set_emission_sphere_radius);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_sphere_radius);

    SE_BIND_METHOD(ParticlesMaterial,set_emission_box_extents);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_box_extents);

    SE_BIND_METHOD(ParticlesMaterial,set_emission_point_texture);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_point_texture);

    SE_BIND_METHOD(ParticlesMaterial,set_emission_normal_texture);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_normal_texture);

    SE_BIND_METHOD(ParticlesMaterial,set_emission_color_texture);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_color_texture);

    SE_BIND_METHOD(ParticlesMaterial,set_emission_point_count);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_point_count);
    SE_BIND_METHOD(ParticlesMaterial,set_emission_ring_radius);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_ring_radius);

    SE_BIND_METHOD(ParticlesMaterial,set_emission_ring_inner_radius);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_ring_inner_radius);

    SE_BIND_METHOD(ParticlesMaterial,set_emission_ring_height);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_ring_height);

    SE_BIND_METHOD(ParticlesMaterial,set_emission_ring_axis);
    SE_BIND_METHOD(ParticlesMaterial,get_emission_ring_axis);

    SE_BIND_METHOD(ParticlesMaterial,set_trail_divisor);
    SE_BIND_METHOD(ParticlesMaterial,get_trail_divisor);

    SE_BIND_METHOD(ParticlesMaterial,set_trail_size_modifier);
    SE_BIND_METHOD(ParticlesMaterial,get_trail_size_modifier);

    SE_BIND_METHOD(ParticlesMaterial,set_trail_color_modifier);
    SE_BIND_METHOD(ParticlesMaterial,get_trail_color_modifier);

    SE_BIND_METHOD(ParticlesMaterial,get_gravity);
    SE_BIND_METHOD(ParticlesMaterial,set_gravity);

    SE_BIND_METHOD(ParticlesMaterial,set_lifetime_randomness);
    SE_BIND_METHOD(ParticlesMaterial,get_lifetime_randomness);

    ADD_GROUP("Time", "tm_");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "tm_lifetime_randomness", PropertyHint::Range, "0,1,0.01"), "set_lifetime_randomness", "get_lifetime_randomness");
    ADD_GROUP("Trail", "trail_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "trail_divisor", PropertyHint::Range, "1,1000000,1"), "set_trail_divisor", "get_trail_divisor");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "trail_size_modifier", PropertyHint::ResourceType, "CurveTexture"), "set_trail_size_modifier", "get_trail_size_modifier");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "trail_color_modifier", PropertyHint::ResourceType, "GradientTexture"), "set_trail_color_modifier", "get_trail_color_modifier");
    ADD_GROUP("Emission Shape", "emission_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "emission_shape", PropertyHint::Enum, "Point,Sphere,Box,Points,Directed Points,Ring"), "set_emission_shape", "get_emission_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "emission_sphere_radius", PropertyHint::Range, "0.01,128,0.01,or_greater"), "set_emission_sphere_radius", "get_emission_sphere_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "emission_box_extents"), "set_emission_box_extents", "get_emission_box_extents");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "emission_point_texture", PropertyHint::ResourceType, "Texture"), "set_emission_point_texture", "get_emission_point_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "emission_normal_texture", PropertyHint::ResourceType, "Texture"), "set_emission_normal_texture", "get_emission_normal_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "emission_color_texture", PropertyHint::ResourceType, "Texture"), "set_emission_color_texture", "get_emission_color_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "emission_point_count", PropertyHint::Range, "0,1000000,1"), "set_emission_point_count", "get_emission_point_count");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "emission_ring_radius", PropertyHint::Range, "0.01,1000,0.01,or_greater"), "set_emission_ring_radius", "get_emission_ring_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "emission_ring_inner_radius", PropertyHint::Range, "0.0,1000,0.01,or_greater"), "set_emission_ring_inner_radius", "get_emission_ring_inner_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "emission_ring_height", PropertyHint::Range, "0.0,100,0.01,or_greater"), "set_emission_ring_height", "get_emission_ring_height");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "emission_ring_axis"), "set_emission_ring_axis", "get_emission_ring_axis");
    ADD_GROUP("Flags", "flag_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flag_align_y"), "set_flag", "get_flag", FLAG_ALIGN_Y_TO_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flag_rotate_y"), "set_flag", "get_flag", FLAG_ROTATE_Y);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flag_disable_z"), "set_flag", "get_flag", FLAG_DISABLE_Z);
    ADD_GROUP("Direction", "dir_");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "dir_direction"), "set_direction", "get_direction");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "dir_spread", PropertyHint::Range, "0,180,0.01"), "set_spread", "get_spread");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "dir_flatness", PropertyHint::Range, "0,1,0.01"), "set_flatness", "get_flatness");
    ADD_GROUP("Gravity", "grv_");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "grv_gravity"), "set_gravity", "get_gravity");
    ADD_GROUP("Initial Velocity", "initial_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "initial_velocity", PropertyHint::Range, "0,1000,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_INITIAL_LINEAR_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "initial_velocity_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_INITIAL_LINEAR_VELOCITY);
    ADD_GROUP("Angular Velocity", "angular_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "angular_velocity", PropertyHint::Range, "-720,720,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_ANGULAR_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "angular_velocity_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_ANGULAR_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "angular_velocity_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_ANGULAR_VELOCITY);
    ADD_GROUP("Orbit Velocity", "orbit_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "orbit_velocity", PropertyHint::Range, "-1000,1000,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_ORBIT_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "orbit_velocity_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_ORBIT_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "orbit_velocity_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_ORBIT_VELOCITY);
    ADD_GROUP("Linear Accel", "linear_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "linear_accel", PropertyHint::Range, "-100,100,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_LINEAR_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "linear_accel_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_LINEAR_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "linear_accel_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_LINEAR_ACCEL);
    ADD_GROUP("Radial Accel", "radial_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "radial_accel", PropertyHint::Range, "-100,100,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_RADIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "radial_accel_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_RADIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "radial_accel_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_RADIAL_ACCEL);
    ADD_GROUP("Tangential Accel", "tangential_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "tangential_accel", PropertyHint::Range, "-100,100,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_TANGENTIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "tangential_accel_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_TANGENTIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "tangential_accel_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_TANGENTIAL_ACCEL);
    ADD_GROUP("Damping", "dmp_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "dmp_damping", PropertyHint::Range, "0,100,0.01,or_greater"), "set_param", "get_param", PARAM_DAMPING);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "dmp_damping_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_DAMPING);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "dmp_damping_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_DAMPING);
    ADD_GROUP("Angle", "ang_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "ang_angle", PropertyHint::Range, "-720,720,0.1,or_lesser,or_greater"), "set_param", "get_param", PARAM_ANGLE);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "ang_angle_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_ANGLE);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "ang_angle_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_ANGLE);
    ADD_GROUP("Scale", "scl_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "scl_scale", PropertyHint::Range, "0,1000,0.01,or_greater"), "set_param", "get_param", PARAM_SCALE);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "scl_scale_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_SCALE);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "scl_scale_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_SCALE);
    ADD_GROUP("Color", "clr_");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "clr_color"), "set_color", "get_color");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "clr_color_ramp", PropertyHint::ResourceType, "GradientTexture"), "set_color_ramp", "get_color_ramp");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "clr_color_initial_ramp", PropertyHint::ResourceType, "GradientTexture"), "set_color_initial_ramp", "get_color_initial_ramp");

    ADD_GROUP("Hue Variation", "hue_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "hue_variation", PropertyHint::Range, "-1,1,0.01"), "set_param", "get_param", PARAM_HUE_VARIATION);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "hue_variation_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_HUE_VARIATION);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "hue_variation_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_HUE_VARIATION);
    ADD_GROUP("Animation", "anim_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_speed", PropertyHint::Range, "0,128,0.01,or_greater"), "set_param", "get_param", PARAM_ANIM_SPEED);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_speed_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_ANIM_SPEED);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "anim_speed_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_ANIM_SPEED);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_offset", PropertyHint::Range, "0,1,0.01"), "set_param", "get_param", PARAM_ANIM_OFFSET);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_offset_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_ANIM_OFFSET);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "anim_offset_curve", PropertyHint::ResourceType, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_ANIM_OFFSET);

    REGISTER_ENUM(Parameter,uint8_t)
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
    BIND_ENUM_CONSTANT(FLAG_ROTATE_Y);
    BIND_ENUM_CONSTANT(FLAG_DISABLE_Z);
    BIND_ENUM_CONSTANT(FLAG_MAX);

    BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINT);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_SPHERE);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_BOX);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINTS);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_DIRECTED_POINTS);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_RING);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_MAX);
}

ParticlesMaterial::ParticlesMaterial() {

    is_dirty_element=false;
    set_direction(Vector3(1, 0, 0));
    set_spread(45);
    set_flatness(0);
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
    set_emission_box_extents(Vector3(1, 1, 1));
    set_trail_divisor(1);
    set_gravity(Vector3(0, -9.8, 0));
    set_lifetime_randomness(0);
    emission_point_count = 1;
    set_emission_ring_height(1.0);
    set_emission_ring_inner_radius(0.0);
    set_emission_ring_radius(2.0);
    set_emission_ring_axis(Vector3(0.0, 0.0, 1.0));

    for (int i = 0; i < PARAM_MAX; i++) {
        set_param_randomness(Parameter(i), 0);
    }

    for (int i = 0; i < FLAG_MAX; i++) {
        flags[i] = false;
    }

    set_color(Color(1, 1, 1, 1));

    current_key.key = 0;
    current_key.invalid_key = 1;

    is_initialized = true;
    _queue_shader_change();
}

ParticlesMaterial::~ParticlesMaterial() {

    MutexGuard guard(material_mutex);

    if (shader_map.contains(current_key)) {
        shader_map[current_key].users--;
        if (shader_map[current_key].users == 0) {
            //deallocate shader, as it's no longer in use
            RenderingServer::get_singleton()->free_rid(shader_map[current_key].shader);
            shader_map.erase(current_key);
        }

        RenderingServer::get_singleton()->material_set_shader(_get_material(), entt::null);
    }

    s_particle_dirty_materials.erase_first_unsorted(this);
}
