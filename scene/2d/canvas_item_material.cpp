/*************************************************************************/
/*  canvas_item.cpp                                                      */
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

#include "canvas_item_material.h"

#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/version.h"
#include "core/os/mutex.h"
#include "core/object_tooling.h"
#include "scene/resources/shader.h"
#include "servers/rendering_server.h"

#include <QDebug>

IMPL_GDCLASS(CanvasItemMaterial)

VARIANT_ENUM_CAST(CanvasItemMaterial::BlendMode);
VARIANT_ENUM_CAST(CanvasItemMaterial::LightMode);

namespace {

static Vector<CanvasItemMaterial *> s_dirty_canvas_materials;
struct CanvasShaderNames {
    StringName particles_anim_h_frames { "particles_anim_h_frames" };
    StringName particles_anim_v_frames { "particles_anim_v_frames" };
    StringName particles_anim_loop { "particles_anim_loop" };
};
static CanvasShaderNames* s_canvas_shader_names;

struct CanvasItemPendingUpdateComponent {};

} // end of anonymous namespace

Mutex CanvasItemMaterial::material_mutex;

HashMap<CanvasItemMaterial::MaterialKey, CanvasItemMaterial::ShaderData> CanvasItemMaterial::shader_map;

void CanvasItemMaterial::init_shaders() {

    s_canvas_shader_names = memnew(CanvasShaderNames);
}

void CanvasItemMaterial::finish_shaders() {

    s_dirty_canvas_materials.clear();
    memdelete(s_canvas_shader_names);
}

void CanvasItemMaterial::_update_shader() {

    is_dirty_element = false;

    MaterialKey mk = _compute_key();
    if (mk.key == current_key.key) {
        return; //no update required in the end
    }

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
    // Add a comment to describe the shader origin (useful when converting to ShaderMaterial).
    String code = "// NOTE: Shader automatically converted from " VERSION_NAME " " VERSION_FULL_CONFIG "'s CanvasItemMaterial.\n\n";

    code += "shader_type canvas_item;\nrender_mode ";
    switch (blend_mode) {
        case BLEND_MODE_MIX:
            code += "blend_mix";
            break;
        case BLEND_MODE_ADD:
            code += "blend_add";
            break;
        case BLEND_MODE_SUB:
            code += "blend_sub";
            break;
        case BLEND_MODE_MUL:
            code += "blend_mul";
            break;
        case BLEND_MODE_PREMULT_ALPHA:
            code += "blend_premul_alpha";
            break;
        case BLEND_MODE_DISABLED:
            code += "blend_disabled";
            break;
    }

    switch (light_mode) {
        case LIGHT_MODE_NORMAL:
            break;
        case LIGHT_MODE_UNSHADED:
            code += ",unshaded";
            break;
        case LIGHT_MODE_LIGHT_ONLY:
            code += ",light_only";
            break;
    }

    code += ";\n";

    if (particles_animation) {

        code += "uniform int particles_anim_h_frames;\n";
        code += "uniform int particles_anim_v_frames;\n";
        code += "uniform bool particles_anim_loop;\n";

        code += "void vertex() {\n";

        code += "\tfloat h_frames = float(particles_anim_h_frames);\n";
        code += "\tfloat v_frames = float(particles_anim_v_frames);\n";

        code += "\tVERTEX.xy /= vec2(h_frames, v_frames);\n";

        code += "\tfloat particle_total_frames = float(particles_anim_h_frames * particles_anim_v_frames);\n";
        code += "\tfloat particle_frame = floor(INSTANCE_CUSTOM.z * float(particle_total_frames));\n";
        code += "\tif (!particles_anim_loop) {\n";
        code += "\t\tparticle_frame = clamp(particle_frame, 0.0, particle_total_frames - 1.0);\n";
        code += "\t} else {\n";
        code += "\t\tparticle_frame = mod(particle_frame, particle_total_frames);\n";
        code += "\t}";
        code += "\tUV /= vec2(h_frames, v_frames);\n";
        code += "\tUV += vec2(mod(particle_frame, h_frames) / h_frames, floor((particle_frame + 0.5) / h_frames) / v_frames);\n";
        code += "}\n";
    }

    ShaderData shader_data;
    shader_data.shader = RenderingServer::get_singleton()->shader_create();
    shader_data.users = 1;

    RenderingServer::get_singleton()->shader_set_code(shader_data.shader, code);

    shader_map[mk] = shader_data;

    RenderingServer::get_singleton()->material_set_shader(_get_material(), shader_data.shader);
}

void CanvasItemMaterial::flush_changes() {

    MutexGuard guard(material_mutex);

    for(CanvasItemMaterial * mat : s_dirty_canvas_materials) {
        mat->_update_shader();
    }

    s_dirty_canvas_materials.clear();
}

void CanvasItemMaterial::_queue_shader_change() {

    MutexGuard guard(material_mutex);

    if (is_initialized && !is_dirty_element) {
        s_dirty_canvas_materials.push_back(this);
        is_dirty_element = true;
    }
}

//bool CanvasItemMaterial::_is_shader_dirty() const {

//    bool dirty = false;

//    if (material_mutex)
//        material_mutex->lock();

//    dirty = element.in_list();

//    if (material_mutex)
//        material_mutex->unlock();

//    return dirty;
//}
void CanvasItemMaterial::set_blend_mode(BlendMode p_blend_mode) {

    blend_mode = p_blend_mode;
    _queue_shader_change();
}

CanvasItemMaterial::BlendMode CanvasItemMaterial::get_blend_mode() const {
    return blend_mode;
}

void CanvasItemMaterial::set_light_mode(LightMode p_light_mode) {

    light_mode = p_light_mode;
    _queue_shader_change();
}

CanvasItemMaterial::LightMode CanvasItemMaterial::get_light_mode() const {

    return light_mode;
}

void CanvasItemMaterial::set_particles_animation(bool p_particles_anim) {
    particles_animation = p_particles_anim;
    _queue_shader_change();
    Object_change_notify(this);
}

bool CanvasItemMaterial::get_particles_animation() const {
    return particles_animation;
}

void CanvasItemMaterial::set_particles_anim_h_frames(int p_frames) {

    particles_anim_h_frames = p_frames;
    RenderingServer::get_singleton()->material_set_param(_get_material(), s_canvas_shader_names->particles_anim_h_frames, p_frames);
}

int CanvasItemMaterial::get_particles_anim_h_frames() const {

    return particles_anim_h_frames;
}
void CanvasItemMaterial::set_particles_anim_v_frames(int p_frames) {

    particles_anim_v_frames = p_frames;
    RenderingServer::get_singleton()->material_set_param(_get_material(), s_canvas_shader_names->particles_anim_v_frames, p_frames);
}

int CanvasItemMaterial::get_particles_anim_v_frames() const {

    return particles_anim_v_frames;
}

void CanvasItemMaterial::set_particles_anim_loop(bool p_loop) {

    particles_anim_loop = p_loop;
    RenderingServer::get_singleton()->material_set_param(_get_material(), s_canvas_shader_names->particles_anim_loop, particles_anim_loop);
}

bool CanvasItemMaterial::get_particles_anim_loop() const {

    return particles_anim_loop;
}

void CanvasItemMaterial::_validate_property(PropertyInfo &property) const {
    if (StringUtils::begins_with(property.name,"particles_anim_") && !particles_animation) {
        property.usage = 0;
    }
}

RenderingEntity CanvasItemMaterial::get_shader_rid() const {

    ERR_FAIL_COND_V(!shader_map.contains(current_key), entt::null);
    return shader_map[current_key].shader;
}

RenderingServerEnums::ShaderMode CanvasItemMaterial::get_shader_mode() const {

    return RenderingServerEnums::ShaderMode::CANVAS_ITEM;
}

void CanvasItemMaterial::_bind_methods() {

    SE_BIND_METHOD(CanvasItemMaterial,set_blend_mode);
    SE_BIND_METHOD(CanvasItemMaterial,get_blend_mode);

    SE_BIND_METHOD(CanvasItemMaterial,set_light_mode);
    SE_BIND_METHOD(CanvasItemMaterial,get_light_mode);

    SE_BIND_METHOD(CanvasItemMaterial,set_particles_animation);
    SE_BIND_METHOD(CanvasItemMaterial,get_particles_animation);

    SE_BIND_METHOD(CanvasItemMaterial,set_particles_anim_h_frames);
    SE_BIND_METHOD(CanvasItemMaterial,get_particles_anim_h_frames);

    SE_BIND_METHOD(CanvasItemMaterial,set_particles_anim_v_frames);
    SE_BIND_METHOD(CanvasItemMaterial,get_particles_anim_v_frames);

    SE_BIND_METHOD(CanvasItemMaterial,set_particles_anim_loop);
    SE_BIND_METHOD(CanvasItemMaterial,get_particles_anim_loop);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "blend_mode", PropertyHint::Enum, "Mix,Add,Sub,Mul,Premult Alpha"), "set_blend_mode", "get_blend_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "light_mode", PropertyHint::Enum, "Normal,Unshaded,Light Only"), "set_light_mode", "get_light_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "particles_animation"), "set_particles_animation", "get_particles_animation");

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "particles_anim_h_frames", PropertyHint::Range, "1,128,1"), "set_particles_anim_h_frames", "get_particles_anim_h_frames");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "particles_anim_v_frames", PropertyHint::Range, "1,128,1"), "set_particles_anim_v_frames", "get_particles_anim_v_frames");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "particles_anim_loop"), "set_particles_anim_loop", "get_particles_anim_loop");

    BIND_ENUM_CONSTANT(BLEND_MODE_MIX);
    BIND_ENUM_CONSTANT(BLEND_MODE_ADD);
    BIND_ENUM_CONSTANT(BLEND_MODE_SUB);
    BIND_ENUM_CONSTANT(BLEND_MODE_MUL);
    BIND_ENUM_CONSTANT(BLEND_MODE_PREMULT_ALPHA);

    BIND_ENUM_CONSTANT(LIGHT_MODE_NORMAL);
    BIND_ENUM_CONSTANT(LIGHT_MODE_UNSHADED);
    BIND_ENUM_CONSTANT(LIGHT_MODE_LIGHT_ONLY);
}

CanvasItemMaterial::CanvasItemMaterial() {

    is_dirty_element = false;
    blend_mode = BLEND_MODE_MIX;
    light_mode = LIGHT_MODE_NORMAL;
    particles_animation = false;

    set_particles_anim_h_frames(1);
    set_particles_anim_v_frames(1);
    set_particles_anim_loop(false);

    current_key.key = 0;
    current_key.invalid_key = 1;
    is_initialized = true;
    _queue_shader_change();
}

CanvasItemMaterial::~CanvasItemMaterial() {

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
    s_dirty_canvas_materials.erase_first_unsorted(this);
}
