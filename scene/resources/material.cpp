/*************************************************************************/
/*  material.cpp                                                         */
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

#include "material.h"

#include "core/callable_method_pointer.h"
#include "core/project_settings.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering_server.h"
#include "scene/resources/shader.h"
#include "scene/resources/texture.h"
#include "core/class_db.h"
#include "core/engine.h"
#include "core/method_enum_caster.h"
#include "core/object_tooling.h"
#include "core/os/mutex.h"
#include "core/version.h"
#include "core/string_formatter.h"

IMPL_GDCLASS(Material)
IMPL_GDCLASS(ShaderMaterial)
IMPL_GDCLASS(SpatialMaterial)
RES_BASE_EXTENSION_IMPL(Material,"material")

VARIANT_ENUM_CAST(SpatialMaterial::AsyncMode);
VARIANT_ENUM_CAST(SpatialMaterial::TextureParam);
VARIANT_ENUM_CAST(SpatialMaterial::DetailUV);
VARIANT_ENUM_CAST(SpatialMaterial::Feature);
VARIANT_ENUM_CAST(SpatialMaterial::BlendMode);
VARIANT_ENUM_CAST(SpatialMaterial::BillboardMode);
VARIANT_ENUM_CAST(SpatialMaterial::DepthDrawMode);
VARIANT_ENUM_CAST(SpatialMaterial::CullMode);
VARIANT_ENUM_CAST(SpatialMaterial::Flags);
VARIANT_ENUM_CAST(SpatialMaterial::DiffuseMode);
VARIANT_ENUM_CAST(SpatialMaterial::SpecularMode);
VARIANT_ENUM_CAST(SpatialMaterial::TextureChannel);
VARIANT_ENUM_CAST(SpatialMaterial::EmissionOperator);
VARIANT_ENUM_CAST(SpatialMaterial::DistanceFadeMode);

#include "core/method_bind.h"

#include "scene/scene_string_names.h"

namespace  {
struct SpatialShaderNames {
    StringName albedo;
    StringName specular;
    StringName metallic;
    StringName roughness;
    StringName emission;
    StringName emission_energy;
    StringName normal_scale;
    StringName rim;
    StringName rim_tint;
    StringName clearcoat;
    StringName clearcoat_gloss;
    StringName anisotropy;
    StringName depth_scale;
    StringName subsurface_scattering_strength;
    StringName transmission;
    StringName refraction;
    StringName point_size;
    StringName uv1_scale;
    StringName uv1_offset;
    StringName uv2_scale;
    StringName uv2_offset;
    StringName particles_anim_h_frames;
    StringName particles_anim_v_frames;
    StringName particles_anim_loop;
    StringName depth_min_layers;
    StringName depth_max_layers;
    StringName depth_flip;
    StringName uv1_blend_sharpness;
    StringName uv2_blend_sharpness;
    StringName grow;
    StringName proximity_fade_distance;
    StringName distance_fade_min;
    StringName distance_fade_max;
    StringName ao_light_affect;

    StringName metallic_texture_channel;
    StringName roughness_texture_channel;
    StringName ao_texture_channel;
    StringName clearcoat_texture_channel;
    StringName rim_texture_channel;
    StringName depth_texture_channel;
    StringName refraction_texture_channel;
    StringName alpha_scissor_threshold;

    StringName texture_names[SpatialMaterial::TEXTURE_MAX];
    SpatialShaderNames() {
        albedo = "albedo";
        specular = "specular";
        roughness = "roughness";
        metallic = "metallic";
        emission = "emission";
        emission_energy = "emission_energy";
        normal_scale = "normal_scale";
        rim = "rim";
        rim_tint = "rim_tint";
        clearcoat = "clearcoat";
        clearcoat_gloss = "clearcoat_gloss";
        anisotropy = "anisotropy_ratio";
        depth_scale = "depth_scale";
        subsurface_scattering_strength = "subsurface_scattering_strength";
        transmission = "transmission";
        refraction = "refraction";
        point_size = "point_size";
        uv1_scale = "uv1_scale";
        uv1_offset = "uv1_offset";
        uv2_scale = "uv2_scale";
        uv2_offset = "uv2_offset";
        uv1_blend_sharpness = "uv1_blend_sharpness";
        uv2_blend_sharpness = "uv2_blend_sharpness";

        particles_anim_h_frames = "particles_anim_h_frames";
        particles_anim_v_frames = "particles_anim_v_frames";
        particles_anim_loop = "particles_anim_loop";
        depth_min_layers = "depth_min_layers";
        depth_max_layers = "depth_max_layers";
        depth_flip = "depth_flip";

        grow = "grow";

        ao_light_affect = "ao_light_affect";

        proximity_fade_distance = "proximity_fade_distance";
        distance_fade_min = "distance_fade_min";
        distance_fade_max = "distance_fade_max";

        metallic_texture_channel = "metallic_texture_channel";
        roughness_texture_channel = "roughness_texture_channel";
        ao_texture_channel = "ao_texture_channel";
        clearcoat_texture_channel = "clearcoat_texture_channel";
        rim_texture_channel = "rim_texture_channel";
        depth_texture_channel = "depth_texture_channel";
        refraction_texture_channel = "refraction_texture_channel";
        alpha_scissor_threshold = "alpha_scissor_threshold";

        texture_names[SpatialMaterial::TEXTURE_ALBEDO] = "texture_albedo";
        texture_names[SpatialMaterial::TEXTURE_METALLIC] = "texture_metallic";
        texture_names[SpatialMaterial::TEXTURE_ROUGHNESS] = "texture_roughness";
        texture_names[SpatialMaterial::TEXTURE_EMISSION] = "texture_emission";
        texture_names[SpatialMaterial::TEXTURE_NORMAL] = "texture_normal";
        texture_names[SpatialMaterial::TEXTURE_RIM] = "texture_rim";
        texture_names[SpatialMaterial::TEXTURE_CLEARCOAT] = "texture_clearcoat";
        texture_names[SpatialMaterial::TEXTURE_FLOWMAP] = "texture_flowmap";
        texture_names[SpatialMaterial::TEXTURE_AMBIENT_OCCLUSION] = "texture_ambient_occlusion";
        texture_names[SpatialMaterial::TEXTURE_DEPTH] = "texture_depth";
        texture_names[SpatialMaterial::TEXTURE_SUBSURFACE_SCATTERING] = "texture_subsurface_scattering";
        texture_names[SpatialMaterial::TEXTURE_TRANSMISSION] = "texture_transmission";
        texture_names[SpatialMaterial::TEXTURE_REFRACTION] = "texture_refraction";
        texture_names[SpatialMaterial::TEXTURE_DETAIL_MASK] = "texture_detail_mask";
        texture_names[SpatialMaterial::TEXTURE_DETAIL_ALBEDO] = "texture_detail_albedo";
        texture_names[SpatialMaterial::TEXTURE_DETAIL_NORMAL] = "texture_detail_normal";
    }
};
static SpatialShaderNames *shader_names;
static Vector<SpatialMaterial *> s_dirty_materials;

}


void Material::set_next_pass(const Ref<Material> &p_pass) {

    for (Ref<Material> pass_child = p_pass; pass_child != nullptr; pass_child = pass_child->get_next_pass()) {
        ERR_FAIL_COND_MSG(pass_child == this, "Can't set as next_pass one of its parents to prevent crashes due to recursive loop.");
    }

    if (next_pass == p_pass)
        return;

    next_pass = p_pass;
    RenderingEntity next_pass_rid = entt::null;
    if (next_pass)
        next_pass_rid = next_pass->get_rid();
    RenderingServer::get_singleton()->material_set_next_pass(material, next_pass_rid);
}



void Material::set_render_priority(int p_priority) {

    ERR_FAIL_COND(p_priority < RENDER_PRIORITY_MIN);
    ERR_FAIL_COND(p_priority > RENDER_PRIORITY_MAX);
    render_priority = p_priority;
    RenderingServer::get_singleton()->material_set_render_priority(material, p_priority);
}

int Material::get_render_priority() const {

    return render_priority;
}

RenderingEntity Material::get_rid() const {

    return material;
}
void Material::_validate_property(PropertyInfo &property) const {

    if (!_can_do_next_pass() && property.name == "next_pass") {
        property.usage = 0;
    }
}

void Material::_bind_methods() {

    SE_BIND_METHOD(Material,set_next_pass);
    SE_BIND_METHOD(Material,get_next_pass);

    SE_BIND_METHOD(Material,set_render_priority);
    SE_BIND_METHOD(Material,get_render_priority);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "render_priority", PropertyHint::Range, ::to_string(RENDER_PRIORITY_MIN) + "," + ::to_string(RENDER_PRIORITY_MAX) + ",1"), "set_render_priority", "get_render_priority");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "next_pass", PropertyHint::ResourceType, "Material"), "set_next_pass", "get_next_pass");

    BIND_CONSTANT(RENDER_PRIORITY_MAX)
    BIND_CONSTANT(RENDER_PRIORITY_MIN)
}

Material::Material() {

    material = RenderingServer::get_singleton()->material_create();
    render_priority = 0;
}

Material::~Material() {

    RenderingServer::get_singleton()->free_rid(material);
}

///////////////////////////////////

bool ShaderMaterial::_set(const StringName &p_name, const Variant &p_value) {

    if (shader) {

        StringName pr = shader->remap_param(p_name);
        if (!pr) {
            if (StringUtils::begins_with(p_name,"param/") ) { //backwards compatibility
                pr = StringName(StringUtils::substr(p_name,6));
            }
            if (StringUtils::begins_with(p_name,"shader_param/") ) { //backwards compatibility
                pr = StringName(StringUtils::replace_first(p_name,"shader_param/", ""));
            }
        }
        if (pr) {
            RenderingServer::get_singleton()->material_set_param(_get_material(), pr, p_value);
            return true;
        }
    }

    return false;
}

bool ShaderMaterial::_get(const StringName &p_name, Variant &r_ret) const {

    if (shader) {

        StringName pr = shader->remap_param(p_name);
        if (!pr) {
            if (StringUtils::begins_with(p_name,"param/") ) { //backwards compatibility
                pr = StringName(StringUtils::substr(p_name,6));
            }
            if (StringUtils::begins_with(p_name,"shader_param/") ) { //backwards compatibility
                pr = StringName(StringUtils::replace_first(p_name,"shader_param/", ""));
            }
        }

        if (pr) {
            r_ret = RenderingServer::get_singleton()->material_get_param(_get_material(), pr);
            return true;
        }
    }

    return false;
}

void ShaderMaterial::_get_property_list(Vector<PropertyInfo> *p_list) const {

    if (shader) {
        shader->get_param_list(p_list);
    }
}

bool ShaderMaterial::property_can_revert(StringName p_name) {
    if (!shader) {
        return false;
    }

    StringName pr = shader->remap_param(p_name);
    if (pr.empty()) {
        return false;
    }

    Variant default_value = RenderingServer::get_singleton()->material_get_param_default(_get_material(), pr);
    Variant current_value;
    _get(p_name, current_value);
    return default_value.get_type() != VariantType::NIL && default_value != current_value;
}

Variant ShaderMaterial::property_get_revert(StringName p_name) {
    Variant r_ret;
    if (shader) {
        StringName pr = shader->remap_param(p_name);
        if (pr) {
            r_ret = RenderingServer::get_singleton()->material_get_param_default(_get_material(), pr);
        }
    }
    return r_ret;
}

void ShaderMaterial::set_shader(const Ref<Shader> &p_shader) {

    // Only connect/disconnect the signal when running in the editor.
    // This can be a slow operation, and `_change_notify()` (which is called by `_shader_changed()`)
    // does nothing in non-editor builds anyway. See GH-34741 for details.
    if (shader && Engine::get_singleton()->is_editor_hint()) {
        shader->disconnect("changed",callable_mp(this, &ClassName::_shader_changed));
    }

    shader = p_shader;

    RenderingEntity rid=entt::null;
    if (shader) {
        rid = shader->get_rid();
        if (Engine::get_singleton()->is_editor_hint()) {
            shader->connect("changed",callable_mp(this, &ClassName::_shader_changed));
        }
    }

    RenderingServer::get_singleton()->material_set_shader(_get_material(), rid);
    Object_change_notify(this); //properties for shader exposed
    emit_changed();
}

void ShaderMaterial::set_shader_param(const StringName &p_param, const Variant &p_value) {

    RenderingServer::get_singleton()->material_set_param(_get_material(), p_param, p_value);
}

Variant ShaderMaterial::get_shader_param(const StringName &p_param) const {

    return RenderingServer::get_singleton()->material_get_param(_get_material(), p_param);
}

void ShaderMaterial::_shader_changed() {
    Object_change_notify(this); //update all properties
}

void ShaderMaterial::_bind_methods() {

    SE_BIND_METHOD(ShaderMaterial,set_shader);
    SE_BIND_METHOD(ShaderMaterial,get_shader);
    SE_BIND_METHOD(ShaderMaterial,set_shader_param);
    SE_BIND_METHOD(ShaderMaterial,get_shader_param);
    SE_BIND_METHOD(ShaderMaterial,property_can_revert);
    SE_BIND_METHOD(ShaderMaterial,property_get_revert);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "shader", PropertyHint::ResourceType, "Shader"), "set_shader", "get_shader");
}

using namespace RenderingServerEnums;
bool ShaderMaterial::_can_do_next_pass() const {
    return shader && shader->get_mode() == ShaderMode::SPATIAL;
}

ShaderMode ShaderMaterial::get_shader_mode() const {
    if (shader) {
        return shader->get_mode();
    } else {
        return ShaderMode::SPATIAL;
    }
}
ShaderMaterial::ShaderMaterial() = default;
ShaderMaterial::~ShaderMaterial() = default;

/////////////////////////////////

static Mutex g_material_mutex;
HashMap<SpatialMaterial::MaterialKey, SpatialMaterial::ShaderData> SpatialMaterial::shader_map;

void SpatialMaterial::init_shaders() {
    shader_names = memnew(SpatialShaderNames);
}

static HashMap<uint64_t,Ref<SpatialMaterial>> material_cache_for_2d;

void SpatialMaterial::finish_shaders() {

    for (auto & m : material_cache_for_2d) {
        m.second.unref();
    }

    //TODO: material_mutex.unlock() ?

    s_dirty_materials.clear();

    memdelete(shader_names);
}

void SpatialMaterial::_update_shader() {

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

    // Add a comment to describe the shader origin (useful when converting to ShaderMaterial).
    String code = "// NOTE: Shader automatically converted from " VERSION_NAME " " VERSION_FULL_CONFIG "'s SpatialMaterial.\n\n";

    code += "shader_type spatial;\nrender_mode ";
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
    }

    DepthDrawMode ddm = depth_draw_mode;
    if (features[FEATURE_REFRACTION]) {
        ddm = DEPTH_DRAW_ALWAYS;
    }

    switch (ddm) {
        case DEPTH_DRAW_OPAQUE_ONLY:
            code += ",depth_draw_opaque";
            break;
        case DEPTH_DRAW_ALWAYS:
            code += ",depth_draw_always";
            break;
        case DEPTH_DRAW_DISABLED:
            code += ",depth_draw_never";
            break;
        case DEPTH_DRAW_ALPHA_OPAQUE_PREPASS:
            code += ",depth_draw_alpha_prepass";
            break;
    }

    switch (cull_mode) {
        case CULL_BACK:
            code += ",cull_back";
            break;
        case CULL_FRONT:
            code += ",cull_front";
            break;
        case CULL_DISABLED:
            code += ",cull_disabled";
            break;
    }
    switch (diffuse_mode) {
        case DIFFUSE_BURLEY:
            code += ",diffuse_burley";
            break;
        case DIFFUSE_LAMBERT:
            code += ",diffuse_lambert";
            break;
        case DIFFUSE_LAMBERT_WRAP:
            code += ",diffuse_lambert_wrap";
            break;
        case DIFFUSE_OREN_NAYAR:
            code += ",diffuse_oren_nayar";
            break;
        case DIFFUSE_TOON:
            code += ",diffuse_toon";
            break;
    }
    switch (specular_mode) {
        case SPECULAR_SCHLICK_GGX:
            code += ",specular_schlick_ggx";
            break;
        case SPECULAR_BLINN:
            code += ",specular_blinn";
            break;
        case SPECULAR_PHONG:
            code += ",specular_phong";
            break;
        case SPECULAR_TOON:
            code += ",specular_toon";
            break;
        case SPECULAR_DISABLED:
            code += ",specular_disabled";
            break;
    }

    if (flags[FLAG_UNSHADED]) {
        code += ",unshaded";
    }
    if (flags[FLAG_DISABLE_DEPTH_TEST]) {
        code += ",depth_test_disable";
    }
    if (flags[FLAG_USE_VERTEX_LIGHTING] || force_vertex_shading) {
        code += ",vertex_lighting";
    }
    if (flags[FLAG_TRIPLANAR_USE_WORLD] && (flags[FLAG_UV1_USE_TRIPLANAR] || flags[FLAG_UV2_USE_TRIPLANAR])) {
        code += ",world_vertex_coords";
    }
    if (flags[FLAG_DONT_RECEIVE_SHADOWS]) {
        code += ",shadows_disabled";
    }
    if (flags[FLAG_DISABLE_AMBIENT_LIGHT]) {
        code += ",ambient_light_disabled";
    }
    if (flags[FLAG_ENSURE_CORRECT_NORMALS]) {
        code += ",ensure_correct_normals";
    }
    if (flags[FLAG_USE_SHADOW_TO_OPACITY]) {
        code += ",shadow_to_opacity";
    }
    code += ";\n";

    code += "uniform vec4 albedo : hint_color;\n";
    code += "uniform sampler2D texture_albedo : hint_albedo;\n";
    code += "uniform float specular;\n";
    code += "uniform float metallic;\n";
    if (grow_enabled) {
        code += "uniform float grow;\n";
    }

    if (proximity_fade_enabled) {
        code += "uniform float proximity_fade_distance;\n";
    }
    if (distance_fade != DISTANCE_FADE_DISABLED) {
        code += "uniform float distance_fade_min;\n";
        code += "uniform float distance_fade_max;\n";
    }

    if (flags[FLAG_USE_ALPHA_SCISSOR]) {
        code += "uniform float alpha_scissor_threshold;\n";
    }
    code += "uniform float roughness : hint_range(0,1);\n";
    code += "uniform float point_size : hint_range(0,128);\n";

    if (textures[TEXTURE_METALLIC] != nullptr) {
        code += "uniform sampler2D texture_metallic : hint_white;\n";
        code += "uniform vec4 metallic_texture_channel;\n";
    }

    if (textures[TEXTURE_ROUGHNESS] != nullptr) {
        code += "uniform sampler2D texture_roughness : hint_white;\n";
        code += "uniform vec4 roughness_texture_channel;\n";
    }
    if (billboard_mode == BILLBOARD_PARTICLES) {
        code += "uniform int particles_anim_h_frames;\n";
        code += "uniform int particles_anim_v_frames;\n";
        code += "uniform bool particles_anim_loop;\n";
    }

    if (features[FEATURE_EMISSION]) {

        code += "uniform sampler2D texture_emission : hint_black_albedo;\n";
        code += "uniform vec4 emission : hint_color;\n";
        code += "uniform float emission_energy;\n";
    }

    if (features[FEATURE_REFRACTION]) {
        code += "uniform sampler2D texture_refraction;\n";
        code += "uniform float refraction : hint_range(-16,16);\n";
        code += "uniform vec4 refraction_texture_channel;\n";
    }

    if (features[FEATURE_NORMAL_MAPPING]) {
        code += "uniform sampler2D texture_normal : hint_normal;\n";
        code += "uniform float normal_scale : hint_range(-16,16);\n";
    }
    if (features[FEATURE_RIM]) {
        code += "uniform float rim : hint_range(0,1);\n";
        code += "uniform float rim_tint : hint_range(0,1);\n";
        code += "uniform sampler2D texture_rim : hint_white;\n";
    }
    if (features[FEATURE_CLEARCOAT]) {
        code += "uniform float clearcoat : hint_range(0,1);\n";
        code += "uniform float clearcoat_gloss : hint_range(0,1);\n";
        code += "uniform sampler2D texture_clearcoat : hint_white;\n";
    }
    if (features[FEATURE_ANISOTROPY]) {
        code += "uniform float anisotropy_ratio : hint_range(0,256);\n";
        code += "uniform sampler2D texture_flowmap : hint_aniso;\n";
    }
    if (features[FEATURE_AMBIENT_OCCLUSION]) {
        code += "uniform sampler2D texture_ambient_occlusion : hint_white;\n";
        code += "uniform vec4 ao_texture_channel;\n";
        code += "uniform float ao_light_affect;\n";
    }

    if (features[FEATURE_DETAIL]) {
        code += "uniform sampler2D texture_detail_albedo : hint_albedo;\n";
        code += "uniform sampler2D texture_detail_normal : hint_normal;\n";
        code += "uniform sampler2D texture_detail_mask : hint_white;\n";
    }

    if (features[FEATURE_SUBSURACE_SCATTERING]) {

        code += "uniform float subsurface_scattering_strength : hint_range(0,1);\n";
        code += "uniform sampler2D texture_subsurface_scattering : hint_white;\n";
    }

    if (features[FEATURE_TRANSMISSION]) {

        code += "uniform vec4 transmission : hint_color;\n";
        code += "uniform sampler2D texture_transmission : hint_black;\n";
    }

    if (features[FEATURE_DEPTH_MAPPING]) {
        code += "uniform sampler2D texture_depth : hint_black;\n";
        code += "uniform float depth_scale;\n";
        code += "uniform int depth_min_layers;\n";
        code += "uniform int depth_max_layers;\n";
        code += "uniform vec2 depth_flip;\n";
    }
    if (flags[FLAG_UV1_USE_TRIPLANAR]) {
        code += "varying vec3 uv1_triplanar_pos;\n";
    }
    if (flags[FLAG_UV2_USE_TRIPLANAR]) {
        code += "varying vec3 uv2_triplanar_pos;\n";
    }
    if (flags[FLAG_UV1_USE_TRIPLANAR]) {
        code += "uniform float uv1_blend_sharpness;\n";
        code += "varying vec3 uv1_power_normal;\n";
    }

    if (flags[FLAG_UV2_USE_TRIPLANAR]) {
        code += "uniform float uv2_blend_sharpness;\n";
        code += "varying vec3 uv2_power_normal;\n";
    }

    code += "uniform vec3 uv1_scale;\n";
    code += "uniform vec3 uv1_offset;\n";
    code += "uniform vec3 uv2_scale;\n";
    code += "uniform vec3 uv2_offset;\n";

    code += "\n\n";

    code += "void vertex() {\n";

    if (flags[FLAG_SRGB_VERTEX_COLOR]) {

        code += "\tif (!OUTPUT_IS_SRGB) {\n";
        code += "\t\tCOLOR.rgb = mix( pow((COLOR.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), COLOR.rgb* (1.0 / 12.92), lessThan(COLOR.rgb,vec3(0.04045)) );\n";
        code += "\t}\n";
    }
    if (flags[FLAG_USE_POINT_SIZE]) {

        code += "\tPOINT_SIZE=point_size;\n";
    }

    if (flags[FLAG_USE_VERTEX_LIGHTING] || force_vertex_shading) {

        code += "\tROUGHNESS=roughness;\n";
    }

    if (!flags[FLAG_UV1_USE_TRIPLANAR]) {
        code += "\tUV=UV*uv1_scale.xy+uv1_offset.xy;\n";
    }

    switch (billboard_mode) {
        case BILLBOARD_DISABLED: {

        } break;
        case BILLBOARD_ENABLED: {

            code += "\tMODELVIEW_MATRIX = INV_CAMERA_MATRIX * mat4(CAMERA_MATRIX[0],CAMERA_MATRIX[1],CAMERA_MATRIX[2],WORLD_MATRIX[3]);\n";

            if (flags[FLAG_BILLBOARD_KEEP_SCALE]) {
                code += "\tMODELVIEW_MATRIX = MODELVIEW_MATRIX * mat4(vec4(length(WORLD_MATRIX[0].xyz), 0.0, 0.0, 0.0),vec4(0.0, length(WORLD_MATRIX[1].xyz), 0.0, 0.0),vec4(0.0, 0.0, length(WORLD_MATRIX[2].xyz), 0.0),vec4(0.0, 0.0, 0.0, 1.0));\n";
            }
        } break;
        case BILLBOARD_FIXED_Y: {

            code += "\tMODELVIEW_MATRIX = INV_CAMERA_MATRIX * mat4(vec4(normalize(cross(vec3(0.0, 1.0, 0.0), CAMERA_MATRIX[2].xyz)),0.0),vec4(0.0, 1.0, 0.0, 0.0),vec4(normalize(cross(CAMERA_MATRIX[0].xyz, vec3(0.0, 1.0, 0.0))),0.0),WORLD_MATRIX[3]);\n";

            if (flags[FLAG_BILLBOARD_KEEP_SCALE]) {
                code += "\tMODELVIEW_MATRIX = MODELVIEW_MATRIX * mat4(vec4(length(WORLD_MATRIX[0].xyz), 0.0, 0.0, 0.0),vec4(0.0, length(WORLD_MATRIX[1].xyz), 0.0, 0.0),vec4(0.0, 0.0, length(WORLD_MATRIX[2].xyz), 0.0),vec4(0.0, 0.0, 0.0, 1.0));\n";
            }
        } break;
        case BILLBOARD_PARTICLES: {

            //make billboard
            code += "\tmat4 mat_world = mat4(normalize(CAMERA_MATRIX[0])*length(WORLD_MATRIX[0]),normalize(CAMERA_MATRIX[1])*length(WORLD_MATRIX[0]),normalize(CAMERA_MATRIX[2])*length(WORLD_MATRIX[2]),WORLD_MATRIX[3]);\n";
            //rotate by rotation
            code += "\tmat_world = mat_world * mat4( vec4(cos(INSTANCE_CUSTOM.x),-sin(INSTANCE_CUSTOM.x), 0.0, 0.0), vec4(sin(INSTANCE_CUSTOM.x), cos(INSTANCE_CUSTOM.x), 0.0, 0.0),vec4(0.0, 0.0, 1.0, 0.0),vec4(0.0, 0.0, 0.0, 1.0));\n";
            //set modelview
            code += "\tMODELVIEW_MATRIX = INV_CAMERA_MATRIX * mat_world;\n";

            //handle animation
            code += "\tfloat h_frames = float(particles_anim_h_frames);\n";
            code += "\tfloat v_frames = float(particles_anim_v_frames);\n";
            code += "\tfloat particle_total_frames = float(particles_anim_h_frames * particles_anim_v_frames);\n";
            code += "\tfloat particle_frame = floor(INSTANCE_CUSTOM.z * float(particle_total_frames));\n";
            code += "\tif (!particles_anim_loop) {\n";
            code += "\t\tparticle_frame = clamp(particle_frame, 0.0, particle_total_frames - 1.0);\n";
            code += "\t} else {\n";
            code += "\t\tparticle_frame = mod(particle_frame, particle_total_frames);\n";
            code += "\t}";
            code += "\tUV /= vec2(h_frames, v_frames);\n";
            code += "\tUV += vec2(mod(particle_frame, h_frames) / h_frames, floor((particle_frame + 0.5) / h_frames) / v_frames);\n";
        } break;
    }

    if (flags[FLAG_FIXED_SIZE]) {

        code += "\tif (PROJECTION_MATRIX[3][3] != 0.0) {\n";
        //orthogonal matrix, try to do about the same
        //with viewport size
        code += "\t\tfloat h = abs(1.0 / (2.0 * PROJECTION_MATRIX[1][1]));\n";
        code += "\t\tfloat sc = (h * 2.0); //consistent with Y-fov\n";
        code += "\t\tMODELVIEW_MATRIX[0]*=sc;\n";
        code += "\t\tMODELVIEW_MATRIX[1]*=sc;\n";
        code += "\t\tMODELVIEW_MATRIX[2]*=sc;\n";
        code += "\t} else {\n";
        //just scale by depth
        code += "\t\tfloat sc = -(MODELVIEW_MATRIX)[3].z;\n";
        code += "\t\tMODELVIEW_MATRIX[0]*=sc;\n";
        code += "\t\tMODELVIEW_MATRIX[1]*=sc;\n";
        code += "\t\tMODELVIEW_MATRIX[2]*=sc;\n";
        code += "\t}\n";
    }

    if (detail_uv == DETAIL_UV_2 && !flags[FLAG_UV2_USE_TRIPLANAR]) {
        code += "\tUV2=UV2*uv2_scale.xy+uv2_offset.xy;\n";
    }
    if (flags[FLAG_UV1_USE_TRIPLANAR] || flags[FLAG_UV2_USE_TRIPLANAR]) {
        //generate tangent and binormal in world space
        code += "\tTANGENT = vec3(0.0,0.0,-1.0) * abs(NORMAL.x);\n";
        code += "\tTANGENT+= vec3(1.0,0.0,0.0) * abs(NORMAL.y);\n";
        code += "\tTANGENT+= vec3(1.0,0.0,0.0) * abs(NORMAL.z);\n";
        code += "\tTANGENT = normalize(TANGENT);\n";

        code +=
                "\tBINORMAL = vec3(0.0,1.0,0.0) * abs(NORMAL.x);\n"\
                "\tBINORMAL+= vec3(0.0,0.0,-1.0) * abs(NORMAL.y);\n"\
                "\tBINORMAL+= vec3(0.0,1.0,0.0) * abs(NORMAL.z);\n"\
                "\tBINORMAL = normalize(BINORMAL);\n";
    }

    if (flags[FLAG_UV1_USE_TRIPLANAR]) {

        code += "\tuv1_power_normal=pow(abs(NORMAL),vec3(uv1_blend_sharpness));\n";
        code += "\tuv1_power_normal/=dot(uv1_power_normal,vec3(1.0));\n";
        code += "\tuv1_triplanar_pos = VERTEX * uv1_scale + uv1_offset;\n";
        code += "\tuv1_triplanar_pos *= vec3(1.0,-1.0, 1.0);\n";
    }

    if (flags[FLAG_UV2_USE_TRIPLANAR]) {

        code += "\tuv2_power_normal=pow(abs(NORMAL), vec3(uv2_blend_sharpness));\n";
        code += "\tuv2_power_normal/=dot(uv2_power_normal,vec3(1.0));\n";
        code += "\tuv2_triplanar_pos = VERTEX * uv2_scale + uv2_offset;\n";
        code += "\tuv2_triplanar_pos *= vec3(1.0,-1.0, 1.0);\n";
    }

    if (grow_enabled) {
        code += "\tVERTEX+=NORMAL*grow;\n";
    }

    code += "}\n";
    code += "\n\n";
    if (flags[FLAG_UV1_USE_TRIPLANAR] || flags[FLAG_UV2_USE_TRIPLANAR]) {
        code += "vec4 triplanar_texture(sampler2D p_sampler,vec3 p_weights,vec3 p_triplanar_pos) {\n";
        code += "\tvec4 samp=vec4(0.0);\n";
        code += "\tsamp+= texture(p_sampler,p_triplanar_pos.xy) * p_weights.z;\n";
        code += "\tsamp+= texture(p_sampler,p_triplanar_pos.xz) * p_weights.y;\n";
        code += "\tsamp+= texture(p_sampler,p_triplanar_pos.zy * vec2(-1.0,1.0)) * p_weights.x;\n";
        code += "\treturn samp;\n";
        code += "}\n";
    }
    code += "\n\n";
    code += "void fragment() {\n";

    if (!flags[FLAG_UV1_USE_TRIPLANAR]) {
        code += "\tvec2 base_uv = UV;\n";
    }

    if ((features[FEATURE_DETAIL] && detail_uv == DETAIL_UV_2) || (features[FEATURE_AMBIENT_OCCLUSION] && flags[FLAG_AO_ON_UV2]) || (features[FEATURE_EMISSION] && flags[FLAG_EMISSION_ON_UV2])) {
        code += "\tvec2 base_uv2 = UV2;\n";
    }

    if (features[FEATURE_DEPTH_MAPPING] && flags[FLAG_UV1_USE_TRIPLANAR]) {
        // Display both resource name and albedo texture name.
        // Materials are often built-in to scenes, so displaying the resource name alone may not be meaningful.
        // On the other hand, albedo textures are almost always external to the scene.
        if (textures[TEXTURE_ALBEDO]) {
            WARN_PRINT(FormatVE("%s (albedo %s): Depth mapping is not supported on triplanar materials. Ignoring depth mapping in favor of triplanar mapping.", get_path().c_str(), textures[TEXTURE_ALBEDO]->get_path().c_str()));
        } else if (!get_path().empty()) {
            WARN_PRINT(FormatVE("%s: Depth mapping is not supported on triplanar materials. Ignoring depth mapping in favor of triplanar mapping.", get_path().c_str()));
        } else {
            // Resource wasn't saved yet.
            WARN_PRINT("Depth mapping is not supported on triplanar materials. Ignoring depth mapping in favor of triplanar mapping.");
        }
    }

    if (features[FEATURE_DEPTH_MAPPING] && !flags[FLAG_UV1_USE_TRIPLANAR]) { //depthmap not supported with triplanar
        code += "\t{\n";
        code += "\t\tvec3 view_dir = normalize(normalize(-VERTEX)*mat3(TANGENT*depth_flip.x,-BINORMAL*depth_flip.y,NORMAL));\n"; // binormal is negative due to mikktspace, flip 'unflips' it ;-)

        if (deep_parallax) {
            code += "\t\tfloat num_layers = mix(float(depth_max_layers),float(depth_min_layers), abs(dot(vec3(0.0, 0.0, 1.0), view_dir)));\n";
            code += "\t\tfloat layer_depth = 1.0 / num_layers;\n";
            code += "\t\tfloat current_layer_depth = 0.0;\n";
            code += "\t\tvec2 P = view_dir.xy * depth_scale;\n";
            code += "\t\tvec2 delta = P / num_layers;\n";
            code += "\t\tvec2  ofs = base_uv;\n";
            code += "\t\tfloat depth = textureLod(texture_depth, ofs,0.0).r;\n";
            code += "\t\tfloat current_depth = 0.0;\n";
            code += "\t\twhile(current_depth < depth) {\n";
            code += "\t\t\tofs -= delta;\n";
            code += "\t\t\tdepth = textureLod(texture_depth, ofs,0.0).r;\n";
            code += "\t\t\tcurrent_depth += layer_depth;\n";
            code += "\t\t}\n";
            code += "\t\tvec2 prev_ofs = ofs + delta;\n";
            code += "\t\tfloat after_depth  = depth - current_depth;\n";
            code += "\t\tfloat before_depth = textureLod(texture_depth, prev_ofs, 0.0).r - current_depth + layer_depth;\n";
            code += "\t\tfloat weight = after_depth / (after_depth - before_depth);\n";
            code += "\t\tofs = mix(ofs,prev_ofs,weight);\n";

        } else {
            code += "\t\tfloat depth = texture(texture_depth, base_uv).r;\n";
            // Use offset limiting to improve the appearance of non-deep parallax.
            // This reduces the impression of depth, but avoids visible warping in the distance.
            code += "\t\tvec2 ofs = base_uv - view_dir.xy * depth * depth_scale;\n";
        }

        code += "\t\tbase_uv=ofs;\n";
        if (features[FEATURE_DETAIL] && detail_uv == DETAIL_UV_2) {
            code += "\t\tbase_uv2-=ofs;\n";
        }

        code += "\t}\n";
    }

    if (flags[FLAG_USE_POINT_SIZE]) {
        code += "\tvec4 albedo_tex = texture(texture_albedo,POINT_COORD);\n";
    } else {
        if (flags[FLAG_UV1_USE_TRIPLANAR]) {
            code += "\tvec4 albedo_tex = triplanar_texture(texture_albedo,uv1_power_normal,uv1_triplanar_pos);\n";
        } else {
            code += "\tvec4 albedo_tex = texture(texture_albedo,base_uv);\n";
        }
    }

    if (flags[FLAG_ALBEDO_TEXTURE_SDF]) {
        code += "\tconst float smoothing = 0.125;\n";
        code += "\tfloat dist = albedo_tex.a;\n";
        code += "\talbedo_tex.a = smoothstep(0.5 - smoothing, 0.5 + smoothing, dist);\n";
        code += "\talbedo_tex.rgb = vec3(1.0);\n";
    } else if (flags[FLAG_ALBEDO_TEXTURE_FORCE_SRGB]) {
        code += "\talbedo_tex.rgb = mix(pow((albedo_tex.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)),vec3(2.4)),albedo_tex.rgb.rgb * (1.0 / 12.92),lessThan(albedo_tex.rgb,vec3(0.04045)));\n";
    }

    if (flags[FLAG_ALBEDO_FROM_VERTEX_COLOR]) {
        code += "\talbedo_tex *= COLOR;\n";
    }
    code += "\tALBEDO = albedo.rgb * albedo_tex.rgb;\n";

    if (textures[TEXTURE_METALLIC] != nullptr) {
        if (flags[FLAG_UV1_USE_TRIPLANAR]) {
            code += "\tfloat metallic_tex = dot(triplanar_texture(texture_metallic,uv1_power_normal,uv1_triplanar_pos),metallic_texture_channel);\n";
        } else {
            code += "\tfloat metallic_tex = dot(texture(texture_metallic,base_uv),metallic_texture_channel);\n";
        }
        code += "\tMETALLIC = metallic_tex * metallic;\n";
    } else {
        code += "\tMETALLIC = metallic;\n";
    }

    if (textures[TEXTURE_ROUGHNESS] != nullptr) {
        if (flags[FLAG_UV1_USE_TRIPLANAR]) {
            code += "\tfloat roughness_tex = dot(triplanar_texture(texture_roughness,uv1_power_normal,uv1_triplanar_pos),roughness_texture_channel);\n";
        } else {
            code += "\tfloat roughness_tex = dot(texture(texture_roughness,base_uv),roughness_texture_channel);\n";
        }
        code += "\tROUGHNESS = roughness_tex * roughness;\n";
    } else {
        code += "\tROUGHNESS = roughness;\n";
    }
    code += "\tSPECULAR = specular;\n";

    if (features[FEATURE_NORMAL_MAPPING]) {
        if (flags[FLAG_UV1_USE_TRIPLANAR]) {
            code += "\tNORMALMAP = triplanar_texture(texture_normal,uv1_power_normal,uv1_triplanar_pos).rgb;\n";
        } else {
            code += "\tNORMALMAP = texture(texture_normal,base_uv).rgb;\n";
        }
        code += "\tNORMALMAP_DEPTH = normal_scale;\n";
    }

    if (features[FEATURE_EMISSION]) {
        if (flags[FLAG_EMISSION_ON_UV2]) {
            if (flags[FLAG_UV2_USE_TRIPLANAR]) {
                code += "\tvec3 emission_tex = triplanar_texture(texture_emission,uv2_power_normal,uv2_triplanar_pos).rgb;\n";
            } else {
                code += "\tvec3 emission_tex = texture(texture_emission,base_uv2).rgb;\n";
            }
        } else {
            if (flags[FLAG_UV1_USE_TRIPLANAR]) {
                code += "\tvec3 emission_tex = triplanar_texture(texture_emission,uv1_power_normal,uv1_triplanar_pos).rgb;\n";
            } else {
                code += "\tvec3 emission_tex = texture(texture_emission,base_uv).rgb;\n";
            }
        }

        if (emission_op == EMISSION_OP_ADD) {
            code += "\tEMISSION = (emission.rgb+emission_tex)*emission_energy;\n";
        } else {
            code += "\tEMISSION = (emission.rgb*emission_tex)*emission_energy;\n";
        }
    }

    if (features[FEATURE_REFRACTION]) {

        if (features[FEATURE_NORMAL_MAPPING]) {
            code += "\tvec3 unpacked_normal = NORMALMAP;\n";
            code += "\tunpacked_normal.xy = unpacked_normal.xy * 2.0 - 1.0;\n";
            code += "\tunpacked_normal.z = sqrt(max(0.0, 1.0 - dot(unpacked_normal.xy, unpacked_normal.xy)));\n";
            code += "\tvec3 ref_normal = normalize( mix(NORMAL,TANGENT * unpacked_normal.x + BINORMAL * unpacked_normal.y + NORMAL * unpacked_normal.z,NORMALMAP_DEPTH) );\n";
        } else {
            code += "\tvec3 ref_normal = NORMAL;\n";
        }
        if (flags[FLAG_UV1_USE_TRIPLANAR]) {
            code += "\tvec2 ref_ofs = SCREEN_UV - ref_normal.xy * dot(triplanar_texture(texture_refraction,uv1_power_normal,uv1_triplanar_pos),refraction_texture_channel) * refraction;\n";
        } else {
            code += "\tvec2 ref_ofs = SCREEN_UV - ref_normal.xy * dot(texture(texture_refraction,base_uv),refraction_texture_channel) * refraction;\n";
        }
        code += "\tfloat ref_amount = 1.0 - albedo.a * albedo_tex.a;\n";
        code += "\tEMISSION += textureLod(SCREEN_TEXTURE,ref_ofs,ROUGHNESS * 8.0).rgb * ref_amount;\n";
        code += "\tALBEDO *= 1.0 - ref_amount;\n";
        code += "\tALPHA = 1.0;\n";

    } else if (features[FEATURE_TRANSPARENT] || flags[FLAG_USE_ALPHA_SCISSOR] || flags[FLAG_USE_SHADOW_TO_OPACITY] || (distance_fade == DISTANCE_FADE_PIXEL_ALPHA) || proximity_fade_enabled) {
        code += "\tALPHA = albedo.a * albedo_tex.a;\n";
    }

    if (proximity_fade_enabled) {
        code += "\tfloat depth_tex = textureLod(DEPTH_TEXTURE,SCREEN_UV,0.0).r;\n";
        code += "\tvec4 world_pos = INV_PROJECTION_MATRIX * vec4(SCREEN_UV*2.0-1.0,depth_tex*2.0-1.0,1.0);\n";
        code += "\tworld_pos.xyz/=world_pos.w;\n";
        code += "\tALPHA*=clamp(1.0-smoothstep(world_pos.z+proximity_fade_distance,world_pos.z,VERTEX.z),0.0,1.0);\n";
    }

    if (distance_fade != DISTANCE_FADE_DISABLED) {
        if ((distance_fade == DISTANCE_FADE_OBJECT_DITHER || distance_fade == DISTANCE_FADE_PIXEL_DITHER)) {
            code += "\t{\n";

            if (distance_fade == DISTANCE_FADE_OBJECT_DITHER) {
                code += "\t\tfloat fade_distance = abs((INV_CAMERA_MATRIX * WORLD_MATRIX[3]).z);\n";

            } else {
                code += "\t\tfloat fade_distance=-VERTEX.z;\n";
            }
            // Use interleaved gradient noise, which is fast but still looks good.
            code += "\t\tconst vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);";
            code += "\t\tfloat fade = clamp(smoothstep(distance_fade_min, distance_fade_max, fade_distance), 0.0, 1.0);\n";
            // Use a hard cap to prevent a few stray pixels from remaining when past the fade-out distance.
            code += "\t\tif (fade < 0.001 || fade < fract(magic.z * fract(dot(FRAGCOORD.xy, magic.xy)))) {\n";
            code += "\t\t\tdiscard;\n";
            code += "\t\t}\n";

            code += "\t}\n\n";

        } else {
            code += "\tALPHA*=clamp(smoothstep(distance_fade_min,distance_fade_max,-VERTEX.z),0.0,1.0);\n";
        }
    }

    if (features[FEATURE_RIM]) {
        if (flags[FLAG_UV1_USE_TRIPLANAR]) {
            code += "\tvec2 rim_tex = triplanar_texture(texture_rim,uv1_power_normal,uv1_triplanar_pos).xy;\n";
        } else {
            code += "\tvec2 rim_tex = texture(texture_rim,base_uv).xy;\n";
        }
        code += "\tRIM = rim*rim_tex.x;";
        code += "\tRIM_TINT = rim_tint*rim_tex.y;\n";
    }

    if (features[FEATURE_CLEARCOAT]) {
        if (flags[FLAG_UV1_USE_TRIPLANAR]) {
            code += "\tvec2 clearcoat_tex = triplanar_texture(texture_clearcoat,uv1_power_normal,uv1_triplanar_pos).xy;\n";
        } else {
            code += "\tvec2 clearcoat_tex = texture(texture_clearcoat,base_uv).xy;\n";
        }
        code += "\tCLEARCOAT = clearcoat*clearcoat_tex.x;";
        code += "\tCLEARCOAT_GLOSS = clearcoat_gloss*clearcoat_tex.y;\n";
    }

    if (features[FEATURE_ANISOTROPY]) {
        if (flags[FLAG_UV1_USE_TRIPLANAR]) {
            code += "\tvec3 anisotropy_tex = triplanar_texture(texture_flowmap,uv1_power_normal,uv1_triplanar_pos).rga;\n";
        } else {
            code += "\tvec3 anisotropy_tex = texture(texture_flowmap,base_uv).rga;\n";
        }
        code += "\tANISOTROPY = anisotropy_ratio*anisotropy_tex.b;\n";
        code += "\tANISOTROPY_FLOW = anisotropy_tex.rg*2.0-1.0;\n";
    }

    if (features[FEATURE_AMBIENT_OCCLUSION]) {
        if (flags[FLAG_AO_ON_UV2]) {
            if (flags[FLAG_UV2_USE_TRIPLANAR]) {
                code += "\tAO = dot(triplanar_texture(texture_ambient_occlusion,uv2_power_normal,uv2_triplanar_pos),ao_texture_channel);\n";
            } else {
                code += "\tAO = dot(texture(texture_ambient_occlusion,base_uv2),ao_texture_channel);\n";
            }
        } else {
            if (flags[FLAG_UV1_USE_TRIPLANAR]) {
                code += "\tAO = dot(triplanar_texture(texture_ambient_occlusion,uv1_power_normal,uv1_triplanar_pos),ao_texture_channel);\n";
            } else {
                code += "\tAO = dot(texture(texture_ambient_occlusion,base_uv),ao_texture_channel);\n";
            }
        }

        code += "\tAO_LIGHT_AFFECT = ao_light_affect;\n";
    }

    if (features[FEATURE_SUBSURACE_SCATTERING]) {

        if (flags[FLAG_UV1_USE_TRIPLANAR]) {
            code += "\tfloat sss_tex = triplanar_texture(texture_subsurface_scattering,uv1_power_normal,uv1_triplanar_pos).r;\n";
        } else {
            code += "\tfloat sss_tex = texture(texture_subsurface_scattering,base_uv).r;\n";
        }
        code += "\tSSS_STRENGTH=subsurface_scattering_strength*sss_tex;\n";
    }

    if (features[FEATURE_TRANSMISSION]) {
        if (flags[FLAG_UV1_USE_TRIPLANAR]) {
            code += "\tvec3 transmission_tex = triplanar_texture(texture_transmission,uv1_power_normal,uv1_triplanar_pos).rgb;\n";
        } else {
            code += "\tvec3 transmission_tex = texture(texture_transmission,base_uv).rgb;\n";
        }
        code += "\tTRANSMISSION = (transmission.rgb+transmission_tex);\n";
    }

    if (features[FEATURE_DETAIL]) {

        bool triplanar = (flags[FLAG_UV1_USE_TRIPLANAR] && detail_uv == DETAIL_UV_1) || (flags[FLAG_UV2_USE_TRIPLANAR] && detail_uv == DETAIL_UV_2);

        if (triplanar) {
            String tp_uv = detail_uv == DETAIL_UV_1 ? "uv1" : "uv2";
            code += "\tvec4 detail_tex = triplanar_texture(texture_detail_albedo," + tp_uv + "_power_normal," + tp_uv + "_triplanar_pos);\n";
            code += "\tvec4 detail_norm_tex = triplanar_texture(texture_detail_normal," + tp_uv + "_power_normal," + tp_uv + "_triplanar_pos);\n";

        } else {
            String det_uv = detail_uv == DETAIL_UV_1 ? "base_uv" : "base_uv2";
            code += "\tvec4 detail_tex = texture(texture_detail_albedo," + det_uv + ");\n";
            code += "\tvec4 detail_norm_tex = texture(texture_detail_normal," + det_uv + ");\n";
        }

        if (flags[FLAG_UV1_USE_TRIPLANAR]) {

            code += "\tvec4 detail_mask_tex = triplanar_texture(texture_detail_mask,uv1_power_normal,uv1_triplanar_pos);\n";
        } else {
            code += "\tvec4 detail_mask_tex = texture(texture_detail_mask,base_uv);\n";
        }

        switch (detail_blend_mode) {
            case BLEND_MODE_MIX: {
                code += "\tvec3 detail = mix(ALBEDO.rgb,detail_tex.rgb,detail_tex.a);\n";
            } break;
            case BLEND_MODE_ADD: {
                code += "\tvec3 detail = mix(ALBEDO.rgb,ALBEDO.rgb+detail_tex.rgb,detail_tex.a);\n";
            } break;
            case BLEND_MODE_SUB: {
                code += "\tvec3 detail = mix(ALBEDO.rgb,ALBEDO.rgb-detail_tex.rgb,detail_tex.a);\n";
            } break;
            case BLEND_MODE_MUL: {
                code += "\tvec3 detail = mix(ALBEDO.rgb,ALBEDO.rgb*detail_tex.rgb,detail_tex.a);\n";
            } break;
        }

        code += "\tvec3 detail_norm = mix(NORMALMAP,detail_norm_tex.rgb,detail_tex.a);\n";
        code += "\tNORMALMAP = mix(NORMALMAP,detail_norm,detail_mask_tex.r);\n";
        code += "\tALBEDO.rgb = mix(ALBEDO.rgb,detail,detail_mask_tex.r);\n";
    }

    if (flags[FLAG_USE_ALPHA_SCISSOR]) {
        code += "\tALPHA_SCISSOR=alpha_scissor_threshold;\n";
    }

    code += "}\n";
    String fallback_mode_str;
    switch (async_mode) {
        case ASYNC_MODE_VISIBLE: {
            fallback_mode_str = "async_visible";
        } break;
        case ASYNC_MODE_HIDDEN: {
            fallback_mode_str = "async_hidden";
        } break;
    }
    int loc = code.find("render_mode ");
    if (loc!=String::npos) {
        // replace the first occurence
        code.replace(code.begin() + loc, code.begin() + loc + StringView("render_mode ").length(),
                "render_mode " + fallback_mode_str + ",");
    }

    ShaderData shader_data;
    shader_data.shader = RenderingServer::get_singleton()->shader_create();
    shader_data.users = 1;

    RenderingServer::get_singleton()->shader_set_code(shader_data.shader, code);

    shader_map[mk] = shader_data;

    RenderingServer::get_singleton()->material_set_shader(_get_material(), shader_data.shader);
}

void SpatialMaterial::flush_changes() {

    MutexGuard guard(g_material_mutex);

    for(SpatialMaterial *mat : s_dirty_materials) {
        mat->_update_shader();
    }
    s_dirty_materials.clear();

}

void SpatialMaterial::_queue_shader_change() {

    MutexGuard guard(g_material_mutex);

    if (is_initialized && !is_dirty_element) {
        s_dirty_materials.emplace_back(this);
        is_dirty_element = true;
    }

}

//bool SpatialMaterial::_is_shader_dirty() const {

//    bool dirty = false;

//    if (material_mutex)
//        material_mutex.lock();

//    dirty = element.in_list();

//    if (material_mutex)
//        material_mutex.unlock();

//    return dirty;
//}
void SpatialMaterial::set_albedo(const Color &p_albedo) {

    albedo = p_albedo;

    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->albedo, p_albedo);
}

Color SpatialMaterial::get_albedo() const {

    return albedo;
}

void SpatialMaterial::set_specular(float p_specular) {

    specular = p_specular;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->specular, p_specular);
}

float SpatialMaterial::get_specular() const {

    return specular;
}

void SpatialMaterial::set_roughness(float p_roughness) {

    roughness = p_roughness;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->roughness, p_roughness);
}

float SpatialMaterial::get_roughness() const {

    return roughness;
}

void SpatialMaterial::set_metallic(float p_metallic) {

    metallic = p_metallic;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->metallic, p_metallic);
}

float SpatialMaterial::get_metallic() const {

    return metallic;
}

void SpatialMaterial::set_emission(const Color &p_emission) {

    emission = p_emission;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission, p_emission);
}
Color SpatialMaterial::get_emission() const {

    return emission;
}

void SpatialMaterial::set_emission_energy(float p_emission_energy) {

    emission_energy = p_emission_energy;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_energy, p_emission_energy);
}
float SpatialMaterial::get_emission_energy() const {

    return emission_energy;
}

void SpatialMaterial::set_normal_scale(float p_normal_scale) {

    normal_scale = p_normal_scale;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->normal_scale, p_normal_scale);
}
float SpatialMaterial::get_normal_scale() const {

    return normal_scale;
}

void SpatialMaterial::set_rim(float p_rim) {

    rim = p_rim;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->rim, p_rim);
}
float SpatialMaterial::get_rim() const {

    return rim;
}

void SpatialMaterial::set_rim_tint(float p_rim_tint) {

    rim_tint = p_rim_tint;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->rim_tint, p_rim_tint);
}
float SpatialMaterial::get_rim_tint() const {

    return rim_tint;
}

void SpatialMaterial::set_ao_light_affect(float p_ao_light_affect) {

    ao_light_affect = p_ao_light_affect;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->ao_light_affect, p_ao_light_affect);
}
float SpatialMaterial::get_ao_light_affect() const {

    return ao_light_affect;
}

void SpatialMaterial::set_clearcoat(float p_clearcoat) {

    clearcoat = p_clearcoat;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->clearcoat, p_clearcoat);
}

float SpatialMaterial::get_clearcoat() const {

    return clearcoat;
}

void SpatialMaterial::set_clearcoat_gloss(float p_clearcoat_gloss) {

    clearcoat_gloss = p_clearcoat_gloss;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->clearcoat_gloss, p_clearcoat_gloss);
}

float SpatialMaterial::get_clearcoat_gloss() const {

    return clearcoat_gloss;
}

void SpatialMaterial::set_anisotropy(float p_anisotropy) {

    anisotropy = p_anisotropy;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->anisotropy, p_anisotropy);
}
float SpatialMaterial::get_anisotropy() const {

    return anisotropy;
}

void SpatialMaterial::set_depth_scale(float p_depth_scale) {

    depth_scale = p_depth_scale;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->depth_scale, p_depth_scale);
}

float SpatialMaterial::get_depth_scale() const {

    return depth_scale;
}

void SpatialMaterial::set_subsurface_scattering_strength(float p_subsurface_scattering_strength) {

    subsurface_scattering_strength = p_subsurface_scattering_strength;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->subsurface_scattering_strength, subsurface_scattering_strength);
}

float SpatialMaterial::get_subsurface_scattering_strength() const {

    return subsurface_scattering_strength;
}

void SpatialMaterial::set_transmission(const Color &p_transmission) {

    transmission = p_transmission;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->transmission, transmission);
}

Color SpatialMaterial::get_transmission() const {

    return transmission;
}

void SpatialMaterial::set_refraction(float p_refraction) {

    refraction = p_refraction;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->refraction, refraction);
}

float SpatialMaterial::get_refraction() const {

    return refraction;
}

void SpatialMaterial::set_detail_uv(DetailUV p_detail_uv) {

    if (detail_uv == p_detail_uv) {
        return;
    }

    detail_uv = p_detail_uv;
    _queue_shader_change();
}
SpatialMaterial::DetailUV SpatialMaterial::get_detail_uv() const {

    return detail_uv;
}

void SpatialMaterial::set_blend_mode(BlendMode p_mode) {

    if (blend_mode == p_mode) {
        return;
    }

    blend_mode = p_mode;
    _queue_shader_change();
}
SpatialMaterial::BlendMode SpatialMaterial::get_blend_mode() const {

    return blend_mode;
}

void SpatialMaterial::set_detail_blend_mode(BlendMode p_mode) {

    detail_blend_mode = p_mode;
    _queue_shader_change();
}
SpatialMaterial::BlendMode SpatialMaterial::get_detail_blend_mode() const {

    return detail_blend_mode;
}

void SpatialMaterial::set_depth_draw_mode(DepthDrawMode p_mode) {

    if (depth_draw_mode == p_mode) {
        return;
    }

    depth_draw_mode = p_mode;
    _queue_shader_change();
}
SpatialMaterial::DepthDrawMode SpatialMaterial::get_depth_draw_mode() const {

    return depth_draw_mode;
}

void SpatialMaterial::set_cull_mode(CullMode p_mode) {

    if (cull_mode == p_mode) {
        return;
    }

    cull_mode = p_mode;
    _queue_shader_change();
}
SpatialMaterial::CullMode SpatialMaterial::get_cull_mode() const {

    return cull_mode;
}

void SpatialMaterial::set_diffuse_mode(DiffuseMode p_mode) {

    if (diffuse_mode == p_mode) {
        return;
    }

    diffuse_mode = p_mode;
    _queue_shader_change();
}
SpatialMaterial::DiffuseMode SpatialMaterial::get_diffuse_mode() const {

    return diffuse_mode;
}

void SpatialMaterial::set_specular_mode(SpecularMode p_mode) {

    if (specular_mode == p_mode) {
        return;
    }

    specular_mode = p_mode;
    _queue_shader_change();
}
SpatialMaterial::SpecularMode SpatialMaterial::get_specular_mode() const {

    return specular_mode;
}

void SpatialMaterial::set_flag(Flags p_flag, bool p_enabled) {

    ERR_FAIL_INDEX(p_flag, FLAG_MAX);

    if (flags[p_flag] == p_enabled) {
        return;
    }

    flags.set(p_flag,p_enabled);
    if (
            p_flag == FLAG_USE_ALPHA_SCISSOR ||
            p_flag == FLAG_UNSHADED ||
            p_flag == FLAG_USE_SHADOW_TO_OPACITY ||
            p_flag == FLAG_UV1_USE_TRIPLANAR ||
            p_flag == FLAG_UV2_USE_TRIPLANAR) {
        Object_change_notify(this);
    }
    _queue_shader_change();
}

bool SpatialMaterial::get_flag(Flags p_flag) const {

    ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
    return flags[p_flag];
}

void SpatialMaterial::set_feature(Feature p_feature, bool p_enabled) {

    ERR_FAIL_INDEX(p_feature, FEATURE_MAX);
    if (features[p_feature] == p_enabled) {
        return;
    }

    features.set(p_feature,p_enabled);
    Object_change_notify(this);
    _queue_shader_change();
}

bool SpatialMaterial::get_feature(Feature p_feature) const {

    ERR_FAIL_INDEX_V(p_feature, FEATURE_MAX, false);
    return features[p_feature];
}

void SpatialMaterial::set_texture(TextureParam p_param, const Ref<Texture> &p_texture) {

    ERR_FAIL_INDEX(p_param, TEXTURE_MAX);
    textures[p_param] = p_texture;
    RenderingEntity rid = p_texture ? p_texture->get_rid() : entt::null;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->texture_names[p_param], Variant::from(rid));
    Object_change_notify(this);
    _queue_shader_change();
}

Ref<Texture> SpatialMaterial::get_texture(TextureParam p_param) const {

    ERR_FAIL_INDEX_V(p_param, TEXTURE_MAX, Ref<Texture>());
    return textures[p_param];
}

Ref<Texture> SpatialMaterial::get_texture_by_name(const StringName& p_name) const {
    for (int i = 0; i < (int)SpatialMaterial::TEXTURE_MAX; i++) {
        TextureParam param = TextureParam(i);
        if (p_name == shader_names->texture_names[param]) {
            return textures[param];
        }
    }
    return Ref<Texture>();
}

void SpatialMaterial::_validate_feature(StringView text, Feature feature, PropertyInfo &property) const {
    if (StringUtils::begins_with(property.name,text) && property.name != StringName(String(text) + "_enabled") && !features[feature]) {
        property.usage = 0;
    }
}

void SpatialMaterial::_validate_high_end(StringView text, PropertyInfo &property) const {
    if (StringUtils::begins_with(property.name.asCString(),text)) {
        property.usage |= PROPERTY_USAGE_HIGH_END_GFX;
    }
}

void SpatialMaterial::_validate_property(PropertyInfo &property) const {
    _validate_feature("normal", FEATURE_NORMAL_MAPPING, property);
    _validate_feature("emission", FEATURE_EMISSION, property);
    _validate_feature("rim", FEATURE_RIM, property);
    _validate_feature("clearcoat", FEATURE_CLEARCOAT, property);
    _validate_feature("anisotropy", FEATURE_ANISOTROPY, property);
    _validate_feature("ao", FEATURE_AMBIENT_OCCLUSION, property);
    _validate_feature("depth", FEATURE_DEPTH_MAPPING, property);
    _validate_feature("subsurf_scatter", FEATURE_SUBSURACE_SCATTERING, property);
    _validate_feature("transmission", FEATURE_TRANSMISSION, property);
    _validate_feature("refraction", FEATURE_REFRACTION, property);
    _validate_feature("detail", FEATURE_DETAIL, property);

    _validate_high_end("subsurf_scatter", property);
    _validate_high_end("depth", property);

    if (StringUtils::begins_with(property.name,"particles_anim_") && billboard_mode != BILLBOARD_PARTICLES) {
        property.usage = 0;
    }

    if (property.name == "params_grow_amount" && !grow_enabled) {
        property.usage = 0;
    }

    if (property.name == "proximity_fade_distance" && !proximity_fade_enabled) {
        property.usage = 0;
    }

    if ((property.name == "distance_fade_max_distance" || property.name == "distance_fade_min_distance") && distance_fade == DISTANCE_FADE_DISABLED) {
        property.usage = 0;
    }

    if (property.name == "uv1_triplanar_sharpness" && !flags[FLAG_UV1_USE_TRIPLANAR]) {
        property.usage = 0;
    }

    if (property.name == "uv2_triplanar_sharpness" && !flags[FLAG_UV2_USE_TRIPLANAR]) {
        property.usage = 0;
    }

    if (property.name == "params_alpha_scissor_threshold" && !flags[FLAG_USE_ALPHA_SCISSOR]) {
        property.usage = 0;
    }

    if ((property.name == "depth_min_layers" || property.name == "depth_max_layers") && !deep_parallax) {
        property.usage = 0;
    }

    if (flags[FLAG_UNSHADED]) {
        if (StringUtils::begins_with(property.name,"anisotropy")) {
            property.usage = 0;
        }

        if (StringUtils::begins_with(property.name,"ao")) {
            property.usage = 0;
        }

        if (StringUtils::begins_with(property.name,"clearcoat")) {
            property.usage = 0;
        }

        if (StringUtils::begins_with(property.name,"emission")) {
            property.usage = 0;
        }

        if (StringUtils::begins_with(property.name,"metallic")) {
            property.usage = 0;
        }

        if (StringUtils::begins_with(property.name,"normal")) {
            property.usage = 0;
        }

        if (StringUtils::begins_with(property.name,"rim")) {
            property.usage = 0;
        }

        if (StringUtils::begins_with(property.name,"roughness")) {
            property.usage = 0;
        }

        if (StringUtils::begins_with(property.name,"subsurf_scatter")) {
            property.usage = 0;
        }

        if (StringUtils::begins_with(property.name,"transmission")) {
            property.usage = 0;
        }
    }
}

void SpatialMaterial::set_line_width(float p_line_width) {

    line_width = p_line_width;
    RenderingServer::get_singleton()->material_set_line_width(_get_material(), line_width);
}

float SpatialMaterial::get_line_width() const {

    return line_width;
}

void SpatialMaterial::set_point_size(float p_point_size) {

    point_size = p_point_size;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->point_size, p_point_size);
}

float SpatialMaterial::get_point_size() const {

    return point_size;
}

void SpatialMaterial::set_uv1_scale(const Vector3 &p_scale) {

    uv1_scale = p_scale;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->uv1_scale, p_scale);
}

Vector3 SpatialMaterial::get_uv1_scale() const {

    return uv1_scale;
}

void SpatialMaterial::set_uv1_offset(const Vector3 &p_offset) {

    uv1_offset = p_offset;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->uv1_offset, p_offset);
}
Vector3 SpatialMaterial::get_uv1_offset() const {

    return uv1_offset;
}

void SpatialMaterial::set_uv1_triplanar_blend_sharpness(float p_sharpness) {

    // Negative values or values higher than 150 can result in NaNs, leading to broken rendering.
    uv1_triplanar_sharpness = CLAMP(p_sharpness, 0.0f, 150.0f);
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->uv1_blend_sharpness, uv1_triplanar_sharpness);
}

float SpatialMaterial::get_uv1_triplanar_blend_sharpness() const {

    return uv1_triplanar_sharpness;
}

void SpatialMaterial::set_uv2_scale(const Vector3 &p_scale) {

    uv2_scale = p_scale;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->uv2_scale, p_scale);
}

Vector3 SpatialMaterial::get_uv2_scale() const {

    return uv2_scale;
}

void SpatialMaterial::set_uv2_offset(const Vector3 &p_offset) {

    uv2_offset = p_offset;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->uv2_offset, p_offset);
}

Vector3 SpatialMaterial::get_uv2_offset() const {

    return uv2_offset;
}

void SpatialMaterial::set_uv2_triplanar_blend_sharpness(float p_sharpness) {
    // Negative values or values higher than 150 can result in NaNs, leading to broken rendering.
    uv2_triplanar_sharpness = CLAMP(p_sharpness, 0.0f, 150.0f);
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->uv2_blend_sharpness, uv2_triplanar_sharpness);
}

float SpatialMaterial::get_uv2_triplanar_blend_sharpness() const {

    return uv2_triplanar_sharpness;
}

void SpatialMaterial::set_billboard_mode(BillboardMode p_mode) {

    billboard_mode = p_mode;
    _queue_shader_change();
    Object_change_notify(this);
}

SpatialMaterial::BillboardMode SpatialMaterial::get_billboard_mode() const {

    return billboard_mode;
}

void SpatialMaterial::set_particles_anim_h_frames(int p_frames) {

    particles_anim_h_frames = p_frames;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->particles_anim_h_frames, p_frames);
}

int SpatialMaterial::get_particles_anim_h_frames() const {

    return particles_anim_h_frames;
}
void SpatialMaterial::set_particles_anim_v_frames(int p_frames) {

    particles_anim_v_frames = p_frames;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->particles_anim_v_frames, p_frames);
}

int SpatialMaterial::get_particles_anim_v_frames() const {

    return particles_anim_v_frames;
}

void SpatialMaterial::set_particles_anim_loop(bool p_loop) {

    particles_anim_loop = p_loop;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->particles_anim_loop, particles_anim_loop);
}

bool SpatialMaterial::get_particles_anim_loop() const {

    return particles_anim_loop;
}

void SpatialMaterial::set_depth_deep_parallax(bool p_enable) {

    deep_parallax = p_enable;
    _queue_shader_change();
    Object_change_notify(this);
}

bool SpatialMaterial::is_depth_deep_parallax_enabled() const {

    return deep_parallax;
}

void SpatialMaterial::set_depth_deep_parallax_min_layers(int p_layer) {

    deep_parallax_min_layers = p_layer;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->depth_min_layers, p_layer);
}
int SpatialMaterial::get_depth_deep_parallax_min_layers() const {

    return deep_parallax_min_layers;
}

void SpatialMaterial::set_depth_deep_parallax_max_layers(int p_layer) {

    deep_parallax_max_layers = p_layer;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->depth_max_layers, p_layer);
}
int SpatialMaterial::get_depth_deep_parallax_max_layers() const {

    return deep_parallax_max_layers;
}

void SpatialMaterial::set_depth_deep_parallax_flip_tangent(bool p_flip) {

    depth_parallax_flip_tangent = p_flip;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->depth_flip, Vector2(depth_parallax_flip_tangent ? -1 : 1, depth_parallax_flip_binormal ? -1 : 1));
}

bool SpatialMaterial::get_depth_deep_parallax_flip_tangent() const {

    return depth_parallax_flip_tangent;
}

void SpatialMaterial::set_depth_deep_parallax_flip_binormal(bool p_flip) {

    depth_parallax_flip_binormal = p_flip;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->depth_flip, Vector2(depth_parallax_flip_tangent ? -1 : 1, depth_parallax_flip_binormal ? -1 : 1));
}

bool SpatialMaterial::get_depth_deep_parallax_flip_binormal() const {

    return depth_parallax_flip_binormal;
}

void SpatialMaterial::set_grow_enabled(bool p_enable) {
    grow_enabled = p_enable;
    _queue_shader_change();
    Object_change_notify(this);
}

bool SpatialMaterial::is_grow_enabled() const {
    return grow_enabled;
}

void SpatialMaterial::set_alpha_scissor_threshold(float p_threshold) {
    alpha_scissor_threshold = p_threshold;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->alpha_scissor_threshold, p_threshold);
}

float SpatialMaterial::get_alpha_scissor_threshold() const {

    return alpha_scissor_threshold;
}

void SpatialMaterial::set_grow(float p_grow) {
    grow = p_grow;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->grow, p_grow);
}

float SpatialMaterial::get_grow() const {

    return grow;
}

static Plane _get_texture_mask(SpatialMaterial::TextureChannel p_channel) {
    static const Plane masks[5] = {
        Plane(1, 0, 0, 0),
        Plane(0, 1, 0, 0),
        Plane(0, 0, 1, 0),
        Plane(0, 0, 0, 1),
        Plane(0.3333333f, 0.3333333f, 0.3333333f, 0),
    };

    return masks[p_channel];
}

void SpatialMaterial::set_metallic_texture_channel(TextureChannel p_channel) {
    ERR_FAIL_INDEX(p_channel, 5);
    metallic_texture_channel = p_channel;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->metallic_texture_channel, _get_texture_mask(p_channel));
}

SpatialMaterial::TextureChannel SpatialMaterial::get_metallic_texture_channel() const {
    return metallic_texture_channel;
}

void SpatialMaterial::set_roughness_texture_channel(TextureChannel p_channel) {

    ERR_FAIL_INDEX(p_channel, 5);
    roughness_texture_channel = p_channel;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->roughness_texture_channel, _get_texture_mask(p_channel));
}

SpatialMaterial::TextureChannel SpatialMaterial::get_roughness_texture_channel() const {
    return roughness_texture_channel;
}

void SpatialMaterial::set_ao_texture_channel(TextureChannel p_channel) {
    ERR_FAIL_INDEX(p_channel, 5);
    ao_texture_channel = p_channel;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->ao_texture_channel, _get_texture_mask(p_channel));
}

SpatialMaterial::TextureChannel SpatialMaterial::get_ao_texture_channel() const {
    return ao_texture_channel;
}

void SpatialMaterial::set_refraction_texture_channel(TextureChannel p_channel) {
    ERR_FAIL_INDEX(p_channel, 5);

    refraction_texture_channel = p_channel;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->refraction_texture_channel, _get_texture_mask(p_channel));
}

SpatialMaterial::TextureChannel SpatialMaterial::get_refraction_texture_channel() const {
    return refraction_texture_channel;
}

RenderingEntity SpatialMaterial::get_material_rid_for_2d(bool p_shaded, bool p_transparent, bool p_double_sided, bool p_cut_alpha, bool p_opaque_prepass, bool p_billboard, bool p_billboard_y, bool p_no_depth_test, bool p_fixed_size, bool p_sdf) {

    uint64_t hash = 0;
    if (p_shaded) {
        hash |= 1 << 0;
    }
    if (p_transparent) {
        hash |= 1 << 1;
    }
    if (p_cut_alpha) {
        hash |= 1 << 2;
    }
    if (p_opaque_prepass) {
        hash |= 1 << 3;
    }
    if (p_double_sided) {
        hash |= 1 << 4;
    }
    if (p_billboard) {
        hash |= 1 << 5;
    }
    if (p_billboard_y) {
        hash |= 1 << 6;
    }
    if (p_no_depth_test) {
        hash |= 1 << 7;
    }
    if (p_fixed_size) {
        hash |= 1 << 8;
    }
    if (p_sdf) {
        hash |= 1 << 9;
    }

    if (material_cache_for_2d.contains(hash)) {
        return material_cache_for_2d[hash]->get_rid();
    }

    Ref<SpatialMaterial> material(make_ref_counted<SpatialMaterial>());

    material->set_flag(FLAG_UNSHADED, !p_shaded);
    material->set_feature(FEATURE_TRANSPARENT, p_transparent);
    material->set_cull_mode(p_double_sided ? CULL_DISABLED : CULL_BACK);
    material->set_depth_draw_mode(p_opaque_prepass ? DEPTH_DRAW_ALPHA_OPAQUE_PREPASS : DEPTH_DRAW_OPAQUE_ONLY);
    material->set_flag(FLAG_SRGB_VERTEX_COLOR, true);
    material->set_flag(FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    material->set_flag(FLAG_USE_ALPHA_SCISSOR, p_cut_alpha);
    material->set_flag(FLAG_DISABLE_DEPTH_TEST, p_no_depth_test);
    material->set_flag(FLAG_FIXED_SIZE, p_fixed_size);
    material->set_flag(FLAG_ALBEDO_TEXTURE_SDF, p_sdf);

    if (p_billboard || p_billboard_y) {
        material->set_flag(FLAG_BILLBOARD_KEEP_SCALE, true);
        material->set_billboard_mode(p_billboard_y ? BILLBOARD_FIXED_Y : BILLBOARD_ENABLED);
    }

    material_cache_for_2d[hash] = material;
    // flush before using so we can access the shader right away
    flush_changes();

    return material->get_rid();
}

void SpatialMaterial::set_on_top_of_alpha() {
    set_feature(FEATURE_TRANSPARENT, true);
    set_render_priority(RENDER_PRIORITY_MAX);
    set_flag(FLAG_DISABLE_DEPTH_TEST, true);
}

void SpatialMaterial::set_proximity_fade(bool p_enable) {

    proximity_fade_enabled = p_enable;
    _queue_shader_change();
    Object_change_notify(this);
}

bool SpatialMaterial::is_proximity_fade_enabled() const {

    return proximity_fade_enabled;
}

void SpatialMaterial::set_proximity_fade_distance(float p_distance) {

    proximity_fade_distance = p_distance;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->proximity_fade_distance, p_distance);
}
float SpatialMaterial::get_proximity_fade_distance() const {

    return proximity_fade_distance;
}

void SpatialMaterial::set_distance_fade(DistanceFadeMode p_mode) {

    distance_fade = p_mode;
    _queue_shader_change();
    Object_change_notify(this);
}
SpatialMaterial::DistanceFadeMode SpatialMaterial::get_distance_fade() const {

    return distance_fade;
}

void SpatialMaterial::set_distance_fade_max_distance(float p_distance) {

    distance_fade_max_distance = p_distance;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->distance_fade_max, distance_fade_max_distance);
}
float SpatialMaterial::get_distance_fade_max_distance() const {

    return distance_fade_max_distance;
}

void SpatialMaterial::set_distance_fade_min_distance(float p_distance) {

    distance_fade_min_distance = p_distance;
    RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->distance_fade_min, distance_fade_min_distance);
}

float SpatialMaterial::get_distance_fade_min_distance() const {

    return distance_fade_min_distance;
}

void SpatialMaterial::set_emission_operator(EmissionOperator p_op) {

    if (emission_op == p_op) {
        return;
    }
    emission_op = p_op;
    _queue_shader_change();
}

SpatialMaterial::EmissionOperator SpatialMaterial::get_emission_operator() const {

    return emission_op;
}

RenderingEntity SpatialMaterial::get_shader_rid() const {

    ERR_FAIL_COND_V(!shader_map.contains(current_key), entt::null);
    return shader_map[current_key].shader;
}

ShaderMode SpatialMaterial::get_shader_mode() const {

    return ShaderMode::SPATIAL;
}

void SpatialMaterial::set_async_mode(AsyncMode p_mode) {
    async_mode = p_mode;
    _queue_shader_change();
    Object_change_notify(this);
}

SpatialMaterial::AsyncMode SpatialMaterial::get_async_mode() const {
    return async_mode;
}

void SpatialMaterial::_bind_methods() {

    SE_BIND_METHOD(SpatialMaterial,set_albedo);
    SE_BIND_METHOD(SpatialMaterial,get_albedo);

    SE_BIND_METHOD(SpatialMaterial,set_specular);
    SE_BIND_METHOD(SpatialMaterial,get_specular);

    SE_BIND_METHOD(SpatialMaterial,set_metallic);
    SE_BIND_METHOD(SpatialMaterial,get_metallic);

    SE_BIND_METHOD(SpatialMaterial,set_roughness);
    SE_BIND_METHOD(SpatialMaterial,get_roughness);

    SE_BIND_METHOD(SpatialMaterial,set_emission);
    SE_BIND_METHOD(SpatialMaterial,get_emission);

    SE_BIND_METHOD(SpatialMaterial,set_emission_energy);
    SE_BIND_METHOD(SpatialMaterial,get_emission_energy);

    SE_BIND_METHOD(SpatialMaterial,set_normal_scale);
    SE_BIND_METHOD(SpatialMaterial,get_normal_scale);

    SE_BIND_METHOD(SpatialMaterial,set_rim);
    SE_BIND_METHOD(SpatialMaterial,get_rim);

    SE_BIND_METHOD(SpatialMaterial,set_rim_tint);
    SE_BIND_METHOD(SpatialMaterial,get_rim_tint);

    SE_BIND_METHOD(SpatialMaterial,set_clearcoat);
    SE_BIND_METHOD(SpatialMaterial,get_clearcoat);

    SE_BIND_METHOD(SpatialMaterial,set_clearcoat_gloss);
    SE_BIND_METHOD(SpatialMaterial,get_clearcoat_gloss);

    SE_BIND_METHOD(SpatialMaterial,set_anisotropy);
    SE_BIND_METHOD(SpatialMaterial,get_anisotropy);

    SE_BIND_METHOD(SpatialMaterial,set_depth_scale);
    SE_BIND_METHOD(SpatialMaterial,get_depth_scale);

    SE_BIND_METHOD(SpatialMaterial,set_subsurface_scattering_strength);
    SE_BIND_METHOD(SpatialMaterial,get_subsurface_scattering_strength);

    SE_BIND_METHOD(SpatialMaterial,set_transmission);
    SE_BIND_METHOD(SpatialMaterial,get_transmission);

    SE_BIND_METHOD(SpatialMaterial,set_refraction);
    SE_BIND_METHOD(SpatialMaterial,get_refraction);

    SE_BIND_METHOD(SpatialMaterial,set_line_width);
    SE_BIND_METHOD(SpatialMaterial,get_line_width);

    SE_BIND_METHOD(SpatialMaterial,set_point_size);
    SE_BIND_METHOD(SpatialMaterial,get_point_size);

    SE_BIND_METHOD(SpatialMaterial,set_detail_uv);
    SE_BIND_METHOD(SpatialMaterial,get_detail_uv);

    SE_BIND_METHOD(SpatialMaterial,set_blend_mode);
    SE_BIND_METHOD(SpatialMaterial,get_blend_mode);

    SE_BIND_METHOD(SpatialMaterial,set_depth_draw_mode);
    SE_BIND_METHOD(SpatialMaterial,get_depth_draw_mode);

    SE_BIND_METHOD(SpatialMaterial,set_cull_mode);
    SE_BIND_METHOD(SpatialMaterial,get_cull_mode);

    SE_BIND_METHOD(SpatialMaterial,set_diffuse_mode);
    SE_BIND_METHOD(SpatialMaterial,get_diffuse_mode);

    SE_BIND_METHOD(SpatialMaterial,set_specular_mode);
    SE_BIND_METHOD(SpatialMaterial,get_specular_mode);

    SE_BIND_METHOD(SpatialMaterial,set_flag);
    SE_BIND_METHOD(SpatialMaterial,get_flag);

    SE_BIND_METHOD(SpatialMaterial,set_feature);
    SE_BIND_METHOD(SpatialMaterial,get_feature);

    SE_BIND_METHOD(SpatialMaterial,set_texture);
    SE_BIND_METHOD(SpatialMaterial,get_texture);

    SE_BIND_METHOD(SpatialMaterial,set_detail_blend_mode);
    SE_BIND_METHOD(SpatialMaterial,get_detail_blend_mode);

    SE_BIND_METHOD(SpatialMaterial,set_uv1_scale);
    SE_BIND_METHOD(SpatialMaterial,get_uv1_scale);

    SE_BIND_METHOD(SpatialMaterial,set_uv1_offset);
    SE_BIND_METHOD(SpatialMaterial,get_uv1_offset);

    SE_BIND_METHOD(SpatialMaterial,set_uv1_triplanar_blend_sharpness);
    SE_BIND_METHOD(SpatialMaterial,get_uv1_triplanar_blend_sharpness);

    SE_BIND_METHOD(SpatialMaterial,set_uv2_scale);
    SE_BIND_METHOD(SpatialMaterial,get_uv2_scale);

    SE_BIND_METHOD(SpatialMaterial,set_uv2_offset);
    SE_BIND_METHOD(SpatialMaterial,get_uv2_offset);

    SE_BIND_METHOD(SpatialMaterial,set_uv2_triplanar_blend_sharpness);
    SE_BIND_METHOD(SpatialMaterial,get_uv2_triplanar_blend_sharpness);

    SE_BIND_METHOD(SpatialMaterial,set_billboard_mode);
    SE_BIND_METHOD(SpatialMaterial,get_billboard_mode);

    SE_BIND_METHOD(SpatialMaterial,set_particles_anim_h_frames);
    SE_BIND_METHOD(SpatialMaterial,get_particles_anim_h_frames);

    SE_BIND_METHOD(SpatialMaterial,set_particles_anim_v_frames);
    SE_BIND_METHOD(SpatialMaterial,get_particles_anim_v_frames);

    SE_BIND_METHOD(SpatialMaterial,set_particles_anim_loop);
    SE_BIND_METHOD(SpatialMaterial,get_particles_anim_loop);

    SE_BIND_METHOD(SpatialMaterial,set_depth_deep_parallax);
    SE_BIND_METHOD(SpatialMaterial,is_depth_deep_parallax_enabled);

    SE_BIND_METHOD(SpatialMaterial,set_depth_deep_parallax_min_layers);
    SE_BIND_METHOD(SpatialMaterial,get_depth_deep_parallax_min_layers);

    SE_BIND_METHOD(SpatialMaterial,set_depth_deep_parallax_max_layers);
    SE_BIND_METHOD(SpatialMaterial,get_depth_deep_parallax_max_layers);

    SE_BIND_METHOD(SpatialMaterial,set_depth_deep_parallax_flip_tangent);
    SE_BIND_METHOD(SpatialMaterial,get_depth_deep_parallax_flip_tangent);

    SE_BIND_METHOD(SpatialMaterial,set_depth_deep_parallax_flip_binormal);
    SE_BIND_METHOD(SpatialMaterial,get_depth_deep_parallax_flip_binormal);

    SE_BIND_METHOD(SpatialMaterial,set_grow);
    SE_BIND_METHOD(SpatialMaterial,get_grow);

    SE_BIND_METHOD(SpatialMaterial,set_emission_operator);
    SE_BIND_METHOD(SpatialMaterial,get_emission_operator);

    SE_BIND_METHOD(SpatialMaterial,set_ao_light_affect);
    SE_BIND_METHOD(SpatialMaterial,get_ao_light_affect);

    SE_BIND_METHOD(SpatialMaterial,set_alpha_scissor_threshold);
    SE_BIND_METHOD(SpatialMaterial,get_alpha_scissor_threshold);

    SE_BIND_METHOD(SpatialMaterial,set_grow_enabled);
    SE_BIND_METHOD(SpatialMaterial,is_grow_enabled);

    SE_BIND_METHOD(SpatialMaterial,set_metallic_texture_channel);
    SE_BIND_METHOD(SpatialMaterial,get_metallic_texture_channel);

    SE_BIND_METHOD(SpatialMaterial,set_roughness_texture_channel);
    SE_BIND_METHOD(SpatialMaterial,get_roughness_texture_channel);

    SE_BIND_METHOD(SpatialMaterial,set_ao_texture_channel);
    SE_BIND_METHOD(SpatialMaterial,get_ao_texture_channel);

    SE_BIND_METHOD(SpatialMaterial,set_refraction_texture_channel);
    SE_BIND_METHOD(SpatialMaterial,get_refraction_texture_channel);

    SE_BIND_METHOD(SpatialMaterial,set_proximity_fade);
    SE_BIND_METHOD(SpatialMaterial,is_proximity_fade_enabled);

    SE_BIND_METHOD(SpatialMaterial,set_proximity_fade_distance);
    SE_BIND_METHOD(SpatialMaterial,get_proximity_fade_distance);

    SE_BIND_METHOD(SpatialMaterial,set_distance_fade);
    SE_BIND_METHOD(SpatialMaterial,get_distance_fade);

    SE_BIND_METHOD(SpatialMaterial,set_distance_fade_max_distance);
    SE_BIND_METHOD(SpatialMaterial,get_distance_fade_max_distance);

    SE_BIND_METHOD(SpatialMaterial,set_distance_fade_min_distance);
    SE_BIND_METHOD(SpatialMaterial,get_distance_fade_min_distance);

    SE_BIND_METHOD(SpatialMaterial,set_async_mode);
    SE_BIND_METHOD(SpatialMaterial,get_async_mode);
    ADD_GROUP("Flags", "flags_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_transparent"), "set_feature", "get_feature", FEATURE_TRANSPARENT);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_use_shadow_to_opacity"), "set_flag", "get_flag", FLAG_USE_SHADOW_TO_OPACITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_unshaded"), "set_flag", "get_flag", FLAG_UNSHADED);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_vertex_lighting"), "set_flag", "get_flag", FLAG_USE_VERTEX_LIGHTING);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_no_depth_test"), "set_flag", "get_flag", FLAG_DISABLE_DEPTH_TEST);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_use_point_size"), "set_flag", "get_flag", FLAG_USE_POINT_SIZE);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_world_triplanar"), "set_flag", "get_flag", FLAG_TRIPLANAR_USE_WORLD);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_fixed_size"), "set_flag", "get_flag", FLAG_FIXED_SIZE);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_albedo_tex_force_srgb"), "set_flag", "get_flag", FLAG_ALBEDO_TEXTURE_FORCE_SRGB);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_do_not_receive_shadows"), "set_flag", "get_flag", FLAG_DONT_RECEIVE_SHADOWS);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_disable_ambient_light"), "set_flag", "get_flag", FLAG_DISABLE_AMBIENT_LIGHT);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_ensure_correct_normals"), "set_flag", "get_flag", FLAG_ENSURE_CORRECT_NORMALS);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flags_albedo_tex_msdf"), "set_flag", "get_flag", FLAG_ALBEDO_TEXTURE_SDF);

    ADD_GROUP("Vertex Color", "vertex_color");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "vertex_color_use_as_albedo"), "set_flag", "get_flag", FLAG_ALBEDO_FROM_VERTEX_COLOR);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "vertex_color_is_srgb"), "set_flag", "get_flag", FLAG_SRGB_VERTEX_COLOR);

    ADD_GROUP("Parameters", "params_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "params_diffuse_mode", PropertyHint::Enum, "Burley,Lambert,Lambert Wrap,Oren Nayar,Toon"), "set_diffuse_mode", "get_diffuse_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "params_specular_mode", PropertyHint::Enum, "SchlickGGX,Blinn,Phong,Toon,Disabled"), "set_specular_mode", "get_specular_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "params_blend_mode", PropertyHint::Enum, "Mix,Add,Sub,Mul"), "set_blend_mode", "get_blend_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "params_cull_mode", PropertyHint::Enum, "Back,Front,Disabled"), "set_cull_mode", "get_cull_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "params_depth_draw_mode", PropertyHint::Enum, "Opaque Only,Always,Never,Opaque Pre-Pass"), "set_depth_draw_mode", "get_depth_draw_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "params_line_width", PropertyHint::Range, "0.1,128,0.1"), "set_line_width", "get_line_width");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "params_point_size", PropertyHint::Range, "0.1,128,0.1"), "set_point_size", "get_point_size");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "params_billboard_mode", PropertyHint::Enum, "Disabled,Enabled,Y-Billboard,Particle Billboard"), "set_billboard_mode", "get_billboard_mode");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "params_billboard_keep_scale"), "set_flag", "get_flag", FLAG_BILLBOARD_KEEP_SCALE);
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "params_grow"), "set_grow_enabled", "is_grow_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "params_grow_amount", PropertyHint::Range, "-16,16,0.001"), "set_grow", "get_grow");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "params_use_alpha_scissor"), "set_flag", "get_flag", FLAG_USE_ALPHA_SCISSOR);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "params_alpha_scissor_threshold", PropertyHint::Range, "0,1,0.01"), "set_alpha_scissor_threshold", "get_alpha_scissor_threshold");
    ADD_GROUP("Particles Anim", "particles_anim_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "particles_anim_h_frames", PropertyHint::Range, "1,128,1"), "set_particles_anim_h_frames", "get_particles_anim_h_frames");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "particles_anim_v_frames", PropertyHint::Range, "1,128,1"), "set_particles_anim_v_frames", "get_particles_anim_v_frames");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "particles_anim_loop"), "set_particles_anim_loop", "get_particles_anim_loop");

    ADD_GROUP("Albedo", "albedo_");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "albedo_color"), "set_albedo", "get_albedo");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "albedo_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_ALBEDO);

    ADD_GROUP("Metallic", "metallic_");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "metallic_value", PropertyHint::Range, "0,1,0.01"), "set_metallic", "get_metallic");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "metallic_specular", PropertyHint::Range, "0,1,0.01"), "set_specular", "get_specular");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "metallic_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_METALLIC);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "metallic_texture_channel", PropertyHint::Enum, "Red,Green,Blue,Alpha,Gray"), "set_metallic_texture_channel", "get_metallic_texture_channel");

    ADD_GROUP("Roughness", "roughness_");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "roughness_value", PropertyHint::Range, "0,1,0.01"), "set_roughness", "get_roughness");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "roughness_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_ROUGHNESS);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "roughness_texture_channel", PropertyHint::Enum, "Red,Green,Blue,Alpha,Gray"), "set_roughness_texture_channel", "get_roughness_texture_channel");

    ADD_GROUP("Emission", "emission_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "emission_enabled"), "set_feature", "get_feature", FEATURE_EMISSION);
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "emission_color", PropertyHint::ColorNoAlpha), "set_emission", "get_emission");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "emission_energy", PropertyHint::Range, "0,16,0.01,or_greater"), "set_emission_energy", "get_emission_energy");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "emission_operator", PropertyHint::Enum, "Add,Multiply"), "set_emission_operator", "get_emission_operator");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "emission_on_uv2"), "set_flag", "get_flag", FLAG_EMISSION_ON_UV2);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "emission_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_EMISSION);

    ADD_GROUP("NormalMap", "normal_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "normal_enabled"), "set_feature", "get_feature", FEATURE_NORMAL_MAPPING);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "normal_scale", PropertyHint::Range, "-16,16,0.01"), "set_normal_scale", "get_normal_scale");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "normal_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_NORMAL);

    ADD_GROUP("Rim", "rim_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "rim_enabled"), "set_feature", "get_feature", FEATURE_RIM);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "rim_value", PropertyHint::Range, "0,1,0.01"), "set_rim", "get_rim");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "rim_tint", PropertyHint::Range, "0,1,0.01"), "set_rim_tint", "get_rim_tint");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "rim_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_RIM);

    ADD_GROUP("Clearcoat", "clearcoat_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "clearcoat_enabled"), "set_feature", "get_feature", FEATURE_CLEARCOAT);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "clearcoat_value", PropertyHint::Range, "0,1,0.01"), "set_clearcoat", "get_clearcoat");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "clearcoat_gloss", PropertyHint::Range, "0,1,0.01"), "set_clearcoat_gloss", "get_clearcoat_gloss");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "clearcoat_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_CLEARCOAT);

    ADD_GROUP("Anisotropy", "anisotropy_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "anisotropy_enabled"), "set_feature", "get_feature", FEATURE_ANISOTROPY);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "anisotropy_value", PropertyHint::Range, "-1,1,0.01"), "set_anisotropy", "get_anisotropy");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "anisotropy_flowmap", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_FLOWMAP);

    ADD_GROUP("Ambient Occlusion", "ao_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "ao_enabled"), "set_feature", "get_feature", FEATURE_AMBIENT_OCCLUSION);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "ao_light_affect", PropertyHint::Range, "0,1,0.01"), "set_ao_light_affect", "get_ao_light_affect");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "ao_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_AMBIENT_OCCLUSION);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "ao_on_uv2"), "set_flag", "get_flag", FLAG_AO_ON_UV2);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "ao_texture_channel", PropertyHint::Enum, "Red,Green,Blue,Alpha,Gray"), "set_ao_texture_channel", "get_ao_texture_channel");

    ADD_GROUP("Depth", "depth_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "depth_enabled"), "set_feature", "get_feature", FEATURE_DEPTH_MAPPING);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "depth_scale", PropertyHint::Range, "-16,16,0.001"), "set_depth_scale", "get_depth_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "depth_deep_parallax"), "set_depth_deep_parallax", "is_depth_deep_parallax_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "depth_min_layers", PropertyHint::Range, "1,64,1"), "set_depth_deep_parallax_min_layers", "get_depth_deep_parallax_min_layers");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "depth_max_layers", PropertyHint::Range, "1,64,1"), "set_depth_deep_parallax_max_layers", "get_depth_deep_parallax_max_layers");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "depth_flip_tangent"), "set_depth_deep_parallax_flip_tangent", "get_depth_deep_parallax_flip_tangent");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "depth_flip_binormal"), "set_depth_deep_parallax_flip_binormal", "get_depth_deep_parallax_flip_binormal");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "depth_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_DEPTH);

    ADD_GROUP("Subsurf Scatter", "subsurf_scatter_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "subsurf_scatter_enabled"), "set_feature", "get_feature", FEATURE_SUBSURACE_SCATTERING);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "subsurf_scatter_strength", PropertyHint::Range, "0,1,0.01"), "set_subsurface_scattering_strength", "get_subsurface_scattering_strength");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "subsurf_scatter_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_SUBSURFACE_SCATTERING);

    ADD_GROUP("Transmission", "transmission_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "transmission_enabled"), "set_feature", "get_feature", FEATURE_TRANSMISSION);
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "transmission_color", PropertyHint::ColorNoAlpha), "set_transmission", "get_transmission");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "transmission_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_TRANSMISSION);

    ADD_GROUP("Refraction", "refraction_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "refraction_enabled"), "set_feature", "get_feature", FEATURE_REFRACTION);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "refraction_scale", PropertyHint::Range, "-1,1,0.01"), "set_refraction", "get_refraction");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "refraction_texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_REFRACTION);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "refraction_texture_channel", PropertyHint::Enum, "Red,Green,Blue,Alpha,Gray"), "set_refraction_texture_channel", "get_refraction_texture_channel");

    ADD_GROUP("Detail", "detail_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "detail_enabled"), "set_feature", "get_feature", FEATURE_DETAIL);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "detail_mask", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_DETAIL_MASK);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "detail_blend_mode", PropertyHint::Enum, "Mix,Add,Sub,Mul"), "set_detail_blend_mode", "get_detail_blend_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "detail_uv_layer", PropertyHint::Enum, "UV1,UV2"), "set_detail_uv", "get_detail_uv");
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "detail_albedo", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_DETAIL_ALBEDO);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "detail_normal", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture", TEXTURE_DETAIL_NORMAL);

    ADD_GROUP("UV1", "uv1_");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "uv1_scale"), "set_uv1_scale", "get_uv1_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "uv1_offset"), "set_uv1_offset", "get_uv1_offset");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "uv1_triplanar"), "set_flag", "get_flag", FLAG_UV1_USE_TRIPLANAR);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "uv1_triplanar_sharpness", PropertyHint::ExpEasing), "set_uv1_triplanar_blend_sharpness", "get_uv1_triplanar_blend_sharpness");

    ADD_GROUP("UV2", "uv2_");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "uv2_scale"), "set_uv2_scale", "get_uv2_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "uv2_offset"), "set_uv2_offset", "get_uv2_offset");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "uv2_triplanar"), "set_flag", "get_flag", FLAG_UV2_USE_TRIPLANAR);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "uv2_triplanar_sharpness", PropertyHint::ExpEasing), "set_uv2_triplanar_blend_sharpness", "get_uv2_triplanar_blend_sharpness");

    ADD_GROUP("Proximity Fade", "proximity_fade_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "proximity_fade_enable"), "set_proximity_fade", "is_proximity_fade_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "proximity_fade_distance", PropertyHint::Range, "0,4096,0.01"), "set_proximity_fade_distance", "get_proximity_fade_distance");
    ADD_GROUP("Distance Fade", "distance_fade_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "distance_fade_mode", PropertyHint::Enum, "Disabled,PixelAlpha,PixelDither,ObjectDither"), "set_distance_fade", "get_distance_fade");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "distance_fade_min_distance", PropertyHint::Range, "0,4096,0.01"), "set_distance_fade_min_distance", "get_distance_fade_min_distance");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "distance_fade_max_distance", PropertyHint::Range, "0,4096,0.01"), "set_distance_fade_max_distance", "get_distance_fade_max_distance");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "async_mode", PropertyHint::Enum, "Visible,Hidden"), "set_async_mode", "get_async_mode");

    BIND_ENUM_CONSTANT(TEXTURE_ALBEDO);
    BIND_ENUM_CONSTANT(TEXTURE_METALLIC);
    BIND_ENUM_CONSTANT(TEXTURE_ROUGHNESS);
    BIND_ENUM_CONSTANT(TEXTURE_EMISSION);
    BIND_ENUM_CONSTANT(TEXTURE_NORMAL);
    BIND_ENUM_CONSTANT(TEXTURE_RIM);
    BIND_ENUM_CONSTANT(TEXTURE_CLEARCOAT);
    BIND_ENUM_CONSTANT(TEXTURE_FLOWMAP);
    BIND_ENUM_CONSTANT(TEXTURE_AMBIENT_OCCLUSION);
    BIND_ENUM_CONSTANT(TEXTURE_DEPTH);
    BIND_ENUM_CONSTANT(TEXTURE_SUBSURFACE_SCATTERING);
    BIND_ENUM_CONSTANT(TEXTURE_TRANSMISSION);
    BIND_ENUM_CONSTANT(TEXTURE_REFRACTION);
    BIND_ENUM_CONSTANT(TEXTURE_DETAIL_MASK);
    BIND_ENUM_CONSTANT(TEXTURE_DETAIL_ALBEDO);
    BIND_ENUM_CONSTANT(TEXTURE_DETAIL_NORMAL);
    BIND_ENUM_CONSTANT(TEXTURE_MAX);

    BIND_ENUM_CONSTANT(DETAIL_UV_1);
    BIND_ENUM_CONSTANT(DETAIL_UV_2);

    BIND_ENUM_CONSTANT(FEATURE_TRANSPARENT);
    BIND_ENUM_CONSTANT(FEATURE_EMISSION);
    BIND_ENUM_CONSTANT(FEATURE_NORMAL_MAPPING);
    BIND_ENUM_CONSTANT(FEATURE_RIM);
    BIND_ENUM_CONSTANT(FEATURE_CLEARCOAT);
    BIND_ENUM_CONSTANT(FEATURE_ANISOTROPY);
    BIND_ENUM_CONSTANT(FEATURE_AMBIENT_OCCLUSION);
    BIND_ENUM_CONSTANT(FEATURE_DEPTH_MAPPING);
    BIND_ENUM_CONSTANT(FEATURE_SUBSURACE_SCATTERING);
    BIND_ENUM_CONSTANT(FEATURE_TRANSMISSION);
    BIND_ENUM_CONSTANT(FEATURE_REFRACTION);
    BIND_ENUM_CONSTANT(FEATURE_DETAIL);
    BIND_ENUM_CONSTANT(FEATURE_MAX);

    BIND_ENUM_CONSTANT(BLEND_MODE_MIX);
    BIND_ENUM_CONSTANT(BLEND_MODE_ADD);
    BIND_ENUM_CONSTANT(BLEND_MODE_SUB);
    BIND_ENUM_CONSTANT(BLEND_MODE_MUL);

    BIND_ENUM_CONSTANT(DEPTH_DRAW_OPAQUE_ONLY);
    BIND_ENUM_CONSTANT(DEPTH_DRAW_ALWAYS);
    BIND_ENUM_CONSTANT(DEPTH_DRAW_DISABLED);
    BIND_ENUM_CONSTANT(DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);

    BIND_ENUM_CONSTANT(CULL_BACK);
    BIND_ENUM_CONSTANT(CULL_FRONT);
    BIND_ENUM_CONSTANT(CULL_DISABLED);

    BIND_ENUM_CONSTANT(FLAG_UNSHADED);
    BIND_ENUM_CONSTANT(FLAG_USE_VERTEX_LIGHTING);
    BIND_ENUM_CONSTANT(FLAG_DISABLE_DEPTH_TEST);
    BIND_ENUM_CONSTANT(FLAG_ALBEDO_FROM_VERTEX_COLOR);
    BIND_ENUM_CONSTANT(FLAG_SRGB_VERTEX_COLOR);
    BIND_ENUM_CONSTANT(FLAG_USE_POINT_SIZE);
    BIND_ENUM_CONSTANT(FLAG_FIXED_SIZE);
    BIND_ENUM_CONSTANT(FLAG_BILLBOARD_KEEP_SCALE);
    BIND_ENUM_CONSTANT(FLAG_UV1_USE_TRIPLANAR);
    BIND_ENUM_CONSTANT(FLAG_UV2_USE_TRIPLANAR);
    BIND_ENUM_CONSTANT(FLAG_AO_ON_UV2);
    BIND_ENUM_CONSTANT(FLAG_EMISSION_ON_UV2);
    BIND_ENUM_CONSTANT(FLAG_USE_ALPHA_SCISSOR);
    BIND_ENUM_CONSTANT(FLAG_TRIPLANAR_USE_WORLD);
    BIND_ENUM_CONSTANT(FLAG_ALBEDO_TEXTURE_FORCE_SRGB);
    BIND_ENUM_CONSTANT(FLAG_DONT_RECEIVE_SHADOWS);
    BIND_ENUM_CONSTANT(FLAG_DISABLE_AMBIENT_LIGHT);
    BIND_ENUM_CONSTANT(FLAG_ENSURE_CORRECT_NORMALS);
    BIND_ENUM_CONSTANT(FLAG_USE_SHADOW_TO_OPACITY);
    BIND_ENUM_CONSTANT(FLAG_ALBEDO_TEXTURE_SDF);
    BIND_ENUM_CONSTANT(FLAG_MAX);

    BIND_ENUM_CONSTANT(DIFFUSE_BURLEY);
    BIND_ENUM_CONSTANT(DIFFUSE_LAMBERT);
    BIND_ENUM_CONSTANT(DIFFUSE_LAMBERT_WRAP);
    BIND_ENUM_CONSTANT(DIFFUSE_OREN_NAYAR);
    BIND_ENUM_CONSTANT(DIFFUSE_TOON);

    BIND_ENUM_CONSTANT(SPECULAR_SCHLICK_GGX);
    BIND_ENUM_CONSTANT(SPECULAR_BLINN);
    BIND_ENUM_CONSTANT(SPECULAR_PHONG);
    BIND_ENUM_CONSTANT(SPECULAR_TOON);
    BIND_ENUM_CONSTANT(SPECULAR_DISABLED);

    BIND_ENUM_CONSTANT(BILLBOARD_DISABLED);
    BIND_ENUM_CONSTANT(BILLBOARD_ENABLED);
    BIND_ENUM_CONSTANT(BILLBOARD_FIXED_Y);
    BIND_ENUM_CONSTANT(BILLBOARD_PARTICLES);

    BIND_ENUM_CONSTANT(TEXTURE_CHANNEL_RED);
    BIND_ENUM_CONSTANT(TEXTURE_CHANNEL_GREEN);
    BIND_ENUM_CONSTANT(TEXTURE_CHANNEL_BLUE);
    BIND_ENUM_CONSTANT(TEXTURE_CHANNEL_ALPHA);
    BIND_ENUM_CONSTANT(TEXTURE_CHANNEL_GRAYSCALE);

    BIND_ENUM_CONSTANT(EMISSION_OP_ADD);
    BIND_ENUM_CONSTANT(EMISSION_OP_MULTIPLY);

    BIND_ENUM_CONSTANT(DISTANCE_FADE_DISABLED);
    BIND_ENUM_CONSTANT(DISTANCE_FADE_PIXEL_ALPHA);
    BIND_ENUM_CONSTANT(DISTANCE_FADE_PIXEL_DITHER);
    BIND_ENUM_CONSTANT(DISTANCE_FADE_OBJECT_DITHER);
    BIND_ENUM_CONSTANT(ASYNC_MODE_VISIBLE);
    BIND_ENUM_CONSTANT(ASYNC_MODE_HIDDEN);
}

SpatialMaterial::SpatialMaterial() {
    is_dirty_element = false;
    // Initialize to the same values as the shader
    set_albedo(Color(1.0, 1.0, 1.0, 1.0));
    set_specular(0.5);
    set_roughness(1.0);
    set_metallic(0.0);
    set_emission(Color(0, 0, 0));
    set_emission_energy(1.0);
    set_normal_scale(1);
    set_rim(1.0);
    set_rim_tint(0.5);
    set_clearcoat(1);
    set_clearcoat_gloss(0.5);
    set_anisotropy(0);
    set_depth_scale(0.05f);
    set_subsurface_scattering_strength(0);
    set_transmission(Color(0, 0, 0));
    set_refraction(0.05f);
    set_line_width(1);
    set_point_size(1);
    set_uv1_offset(Vector3(0, 0, 0));
    set_uv1_scale(Vector3(1, 1, 1));
    set_uv1_triplanar_blend_sharpness(1);
    set_uv2_offset(Vector3(0, 0, 0));
    set_uv2_scale(Vector3(1, 1, 1));
    set_uv2_triplanar_blend_sharpness(1);
    set_billboard_mode(BILLBOARD_DISABLED);
    set_particles_anim_h_frames(1);
    set_particles_anim_v_frames(1);
    set_particles_anim_loop(false);
    set_alpha_scissor_threshold(0.98f);
    emission_op = EMISSION_OP_ADD;

    proximity_fade_enabled = false;
    distance_fade = DISTANCE_FADE_DISABLED;
    set_proximity_fade_distance(1);
    set_distance_fade_min_distance(0);
    set_distance_fade_max_distance(10);

    set_ao_light_affect(0.0);

    set_metallic_texture_channel(TEXTURE_CHANNEL_RED);
    set_roughness_texture_channel(TEXTURE_CHANNEL_RED);
    set_ao_texture_channel(TEXTURE_CHANNEL_RED);
    set_refraction_texture_channel(TEXTURE_CHANNEL_RED);

    grow_enabled = false;
    set_grow(0.0);

    deep_parallax = false;
    depth_parallax_flip_tangent = false;
    depth_parallax_flip_binormal = false;
    set_depth_deep_parallax_min_layers(8);
    set_depth_deep_parallax_max_layers(32);
    set_depth_deep_parallax_flip_tangent(false); //also sets binormal

    detail_uv = DETAIL_UV_1;
    blend_mode = BLEND_MODE_MIX;
    detail_blend_mode = BLEND_MODE_MIX;
    depth_draw_mode = DEPTH_DRAW_OPAQUE_ONLY;
    cull_mode = CULL_BACK;
    flags.reset();
    force_vertex_shading = T_GLOBAL_GET<bool>("rendering/quality/shading/force_vertex_shading");
    diffuse_mode = DIFFUSE_BURLEY;
    specular_mode = SPECULAR_SCHLICK_GGX;

    async_mode = ASYNC_MODE_VISIBLE;
    features.reset();

    current_key.key = 0;
    current_key.invalid_key = 1;
    is_initialized = true;
    _queue_shader_change();
}

SpatialMaterial::~SpatialMaterial() {

    MutexGuard guard(g_material_mutex);

    if (shader_map.contains(current_key)) {
        shader_map[current_key].users--;
        if (shader_map[current_key].users == 0) {
            //deallocate shader, as it's no longer in use
            RenderingServer::get_singleton()->free_rid(shader_map[current_key].shader);
            shader_map.erase(current_key);
        }
        RenderingServer::get_singleton()->material_set_shader(_get_material(), entt::null);
    }
    if(is_dirty_element)
        s_dirty_materials.erase_first_unsorted(this);

}
