/*************************************************************************/
/*  light_3d.cpp                                                            */
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

#include "light_3d.h"

#include "core/engine.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/project_settings.h"
#include "core/translation_helpers.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/surface_tool.h"

IMPL_GDCLASS(Light3D)
IMPL_GDCLASS(DirectionalLight3D)
IMPL_GDCLASS(OmniLight3D)
IMPL_GDCLASS(SpotLight3D)
VARIANT_ENUM_CAST(Light3D::Param);
VARIANT_ENUM_CAST(Light3D::BakeMode);
VARIANT_ENUM_CAST(DirectionalLight3D::ShadowMode);
VARIANT_ENUM_CAST(DirectionalLight3D::ShadowDepthRange);
VARIANT_ENUM_CAST(OmniLight3D::ShadowMode);
VARIANT_ENUM_CAST(OmniLight3D::ShadowDetail);


void Light3D::set_param(Param p_param, float p_value) {

    ERR_FAIL_INDEX(p_param, PARAM_MAX);
    param[p_param] = p_value;

    RenderingServer::get_singleton()->light_set_param(light, RS::LightParam(p_param), p_value);

    if (p_param == PARAM_SPOT_ANGLE || p_param == PARAM_RANGE) {
        update_gizmo();

        if (p_param == PARAM_SPOT_ANGLE) {
            Object_change_notify(this,"spot_angle");
            update_configuration_warning();
        } else if (p_param == PARAM_RANGE) {
            Object_change_notify(this,"omni_range");
            Object_change_notify(this,"spot_range");
        }
    }
}

float Light3D::get_param(Param p_param) const {

    ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
    return param[p_param];
}

void Light3D::set_shadow(bool p_enable) {

    shadow = p_enable;
    RenderingServer::get_singleton()->light_set_shadow(light, p_enable);

    if (type == RS::LIGHT_SPOT) {
        update_configuration_warning();
    }
}
bool Light3D::has_shadow() const {

    return shadow;
}

void Light3D::set_negative(bool p_enable) {

    negative = p_enable;
    RenderingServer::get_singleton()->light_set_negative(light, p_enable);
}
bool Light3D::is_negative() const {

    return negative;
}

void Light3D::set_cull_mask(uint32_t p_cull_mask) {

    cull_mask = p_cull_mask;
    RenderingServer::get_singleton()->light_set_cull_mask(light, p_cull_mask);
}
uint32_t Light3D::get_cull_mask() const {

    return cull_mask;
}

void Light3D::set_color(const Color &p_color) {

    color = p_color;
    RenderingServer::get_singleton()->light_set_color(light, p_color);
    // The gizmo color depends on the light color, so update it.
    update_gizmo();

}
Color Light3D::get_color() const {

    return color;
}

void Light3D::set_shadow_color(const Color &p_shadow_color) {

    shadow_color = p_shadow_color;
    RenderingServer::get_singleton()->light_set_shadow_color(light, p_shadow_color);
}

Color Light3D::get_shadow_color() const {

    return shadow_color;
}

void Light3D::set_shadow_reverse_cull_face(bool p_enable) {
    reverse_cull = p_enable;
    RenderingServer::get_singleton()->light_set_reverse_cull_face_mode(light, reverse_cull);
}

bool Light3D::get_shadow_reverse_cull_face() const {

    return reverse_cull;
}

AABB Light3D::get_aabb() const {

    if (type == RS::LIGHT_DIRECTIONAL) {

        return AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));

    } else if (type == RS::LIGHT_OMNI) {

        return AABB(Vector3(-1, -1, -1) * param[PARAM_RANGE], Vector3(2, 2, 2) * param[PARAM_RANGE]);

    } else if (type == RS::LIGHT_SPOT) {

        float len = param[PARAM_RANGE];
        float size = Math::tan(Math::deg2rad(param[PARAM_SPOT_ANGLE])) * len;
        return AABB(Vector3(-size, -size, -len), Vector3(size * 2, size * 2, len));
    }

    return AABB();
}

Vector<Face3> Light3D::get_faces(uint32_t p_usage_flags) const {

    return Vector<Face3>();
}

void Light3D::set_bake_mode(BakeMode p_mode) {
    bake_mode = p_mode;
    RenderingServer::get_singleton()->light_set_bake_mode(light, RS::LightBakeMode(bake_mode));
    Object_change_notify(this);
}

void Light3D::_update_visibility() {

    if (!is_inside_tree()) {
        return;
    }

    bool editor_ok = true;

#ifdef TOOLS_ENABLED
    if (editor_only) {
        if (!Engine::get_singleton()->is_editor_hint()) {
            editor_ok = false;
        } else {
            editor_ok = (get_tree()->get_edited_scene_root() &&
                         (this == get_tree()->get_edited_scene_root() ||
                                 get_owner() == get_tree()->get_edited_scene_root()));
        }
    }
#else
    if (editor_only) {
        editor_ok = false;
    }
#endif

    RenderingServer::get_singleton()->instance_set_visible(get_instance(), is_visible_in_tree() && editor_ok);

    Object_change_notify(this,"geometry/visible");
}

void Light3D::_notification(int p_what) {

    if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {

        _update_visibility();
    }

    if (p_what == NOTIFICATION_ENTER_TREE) {
        _update_visibility();
    }
}

void Light3D::set_editor_only(bool p_editor_only) {

    editor_only = p_editor_only;
    _update_visibility();
}

bool Light3D::is_editor_only() const {

    return editor_only;
}

void Light3D::_validate_property(PropertyInfo &property) const {

    //if (RenderingServer::get_singleton()->is_low_end() && property.name == "shadow_contact") {
    //    property.usage = PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL;
    //}
    if (bake_mode != BAKE_ALL && property.name == "light_size") {
        property.usage = PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL;
    }
}

void Light3D::_bind_methods() {

    SE_BIND_METHOD(Light3D,set_editor_only);
    SE_BIND_METHOD(Light3D,is_editor_only);

    SE_BIND_METHOD(Light3D,set_param);
    SE_BIND_METHOD(Light3D,get_param);

    SE_BIND_METHOD(Light3D,set_shadow);
    SE_BIND_METHOD(Light3D,has_shadow);

    SE_BIND_METHOD(Light3D,set_negative);
    SE_BIND_METHOD(Light3D,is_negative);

    SE_BIND_METHOD(Light3D,set_cull_mask);
    SE_BIND_METHOD(Light3D,get_cull_mask);

    SE_BIND_METHOD(Light3D,set_color);
    SE_BIND_METHOD(Light3D,get_color);

    MethodBinder::bind_method(
            D_METHOD("set_shadow_reverse_cull_face", { "enable" }), &Light3D::set_shadow_reverse_cull_face);
    SE_BIND_METHOD(Light3D,get_shadow_reverse_cull_face);

    SE_BIND_METHOD(Light3D,set_shadow_color);
    SE_BIND_METHOD(Light3D,get_shadow_color);

    SE_BIND_METHOD(Light3D,set_bake_mode);
    SE_BIND_METHOD(Light3D,get_bake_mode);

    ADD_GROUP("Light", "light_");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "light_color", PropertyHint::ColorNoAlpha), "set_color", "get_color");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "light_energy", PropertyHint::Range, "0,16,0.001,or_greater"),
            "set_param", "get_param", PARAM_ENERGY);
    ADD_PROPERTYI(
            PropertyInfo(VariantType::FLOAT, "light_indirect_energy", PropertyHint::Range, "0,16,0.001,or_greater"),
            "set_param", "get_param", PARAM_INDIRECT_ENERGY);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "light_size", PropertyHint::Range, "0,1,0.001,or_greater"), "set_param",
            "get_param", PARAM_SIZE);
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "light_negative"), "set_negative", "is_negative");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "light_specular", PropertyHint::Range, "0,16,0.001,or_greater"), "set_param",
            "get_param", PARAM_SPECULAR);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "light_bake_mode", PropertyHint::Enum, "Disable,Indirect Only,All (Direct + Indirect)"),
            "set_bake_mode", "get_bake_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "light_cull_mask", PropertyHint::Layers3DRenderer), "set_cull_mask",
            "get_cull_mask");
    ADD_GROUP("Shadow", "shadow_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "shadow_enabled"), "set_shadow", "has_shadow");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "shadow_color", PropertyHint::ColorNoAlpha), "set_shadow_color",
            "get_shadow_color");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "shadow_bias", PropertyHint::Range, "-10,10,0.001"), "set_param",
            "get_param", PARAM_SHADOW_BIAS);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "shadow_contact", PropertyHint::Range, "0,10,0.001"), "set_param",
            "get_param", PARAM_CONTACT_SHADOW_SIZE);
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "shadow_reverse_cull_face"), "set_shadow_reverse_cull_face",
            "get_shadow_reverse_cull_face");
    ADD_GROUP("Editor", "");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "editor_only"), "set_editor_only", "is_editor_only");
    ADD_GROUP("", "");

    BIND_ENUM_CONSTANT(PARAM_ENERGY);
    BIND_ENUM_CONSTANT(PARAM_INDIRECT_ENERGY);
    BIND_ENUM_CONSTANT(PARAM_SIZE);
    BIND_ENUM_CONSTANT(PARAM_SPECULAR);
    BIND_ENUM_CONSTANT(PARAM_RANGE);
    BIND_ENUM_CONSTANT(PARAM_ATTENUATION);
    BIND_ENUM_CONSTANT(PARAM_SPOT_ANGLE);
    BIND_ENUM_CONSTANT(PARAM_SPOT_ATTENUATION);
    BIND_ENUM_CONSTANT(PARAM_CONTACT_SHADOW_SIZE);
    BIND_ENUM_CONSTANT(PARAM_SHADOW_MAX_DISTANCE);
    BIND_ENUM_CONSTANT(PARAM_SHADOW_SPLIT_1_OFFSET);
    BIND_ENUM_CONSTANT(PARAM_SHADOW_SPLIT_2_OFFSET);
    BIND_ENUM_CONSTANT(PARAM_SHADOW_SPLIT_3_OFFSET);
    BIND_ENUM_CONSTANT(PARAM_SHADOW_NORMAL_BIAS);
    BIND_ENUM_CONSTANT(PARAM_SHADOW_BIAS);
    BIND_ENUM_CONSTANT(PARAM_SHADOW_BIAS_SPLIT_SCALE);
    BIND_ENUM_CONSTANT(PARAM_MAX);

    BIND_ENUM_CONSTANT(BAKE_DISABLED);
    BIND_ENUM_CONSTANT(BAKE_INDIRECT);
    BIND_ENUM_CONSTANT(BAKE_ALL);
}

Light3D::Light3D(RS::LightType p_type) {

    type = p_type;
    switch (p_type) {
        case RS::LIGHT_DIRECTIONAL:
            light = RenderingServer::get_singleton()->directional_light_create();
            break;
        case RS::LIGHT_OMNI:
            light = RenderingServer::get_singleton()->omni_light_create();
            break;
        case RS::LIGHT_SPOT:
            light = RenderingServer::get_singleton()->spot_light_create();
            break;
        default: {
        }
    }

    RenderingServer::get_singleton()->instance_set_base(get_instance(), light);

    reverse_cull = false;
    bake_mode = BAKE_INDIRECT;

    editor_only = false;
    set_color(Color(1, 1, 1, 1));
    set_shadow(false);
    set_negative(false);
    set_cull_mask(0xFFFFFFFF);

    set_param(PARAM_ENERGY, 1);
    set_param(PARAM_INDIRECT_ENERGY, 1);
    set_param(PARAM_SIZE, 0);
    set_param(PARAM_SPECULAR, 0.5);
    set_param(PARAM_RANGE, 5);
    set_param(PARAM_ATTENUATION, 1);
    set_param(PARAM_SPOT_ANGLE, 45);
    set_param(PARAM_SPOT_ATTENUATION, 1);
    set_param(PARAM_CONTACT_SHADOW_SIZE, 0);
    set_param(PARAM_SHADOW_MAX_DISTANCE, 0);
    set_param(PARAM_SHADOW_SPLIT_1_OFFSET, 0.1f);
    set_param(PARAM_SHADOW_SPLIT_2_OFFSET, 0.2f);
    set_param(PARAM_SHADOW_SPLIT_3_OFFSET, 0.5f);
    set_param(PARAM_SHADOW_NORMAL_BIAS, 0.0);
    set_param(PARAM_SHADOW_BIAS, 0.15f);
    set_disable_scale(true);
}

Light3D::Light3D() {

    type = RS::LIGHT_DIRECTIONAL;
    ERR_PRINT("Light3D should not be instanced directly; use the DirectionalLight3D, OmniLight3D or SpotLight3D "
              "subtypes instead.");
}

Light3D::~Light3D() {

    RenderingServer::get_singleton()->instance_set_base(get_instance(), entt::null);

    if (light!=entt::null) {
        RenderingServer::get_singleton()->free_rid(light);
    }
}
/////////////////////////////////////////

void DirectionalLight3D::set_shadow_mode(ShadowMode p_mode) {

    shadow_mode = p_mode;
    RenderingServer::get_singleton()->light_directional_set_shadow_mode(light, RS::LightDirectionalShadowMode(p_mode));
    property_list_changed_notify();
}

DirectionalLight3D::ShadowMode DirectionalLight3D::get_shadow_mode() const {

    return shadow_mode;
}

void DirectionalLight3D::set_shadow_depth_range(ShadowDepthRange p_range) {
    shadow_depth_range = p_range;
    RenderingServer::get_singleton()->light_directional_set_shadow_depth_range_mode(
            light, RS::LightDirectionalShadowDepthRangeMode(p_range));
}

DirectionalLight3D::ShadowDepthRange DirectionalLight3D::get_shadow_depth_range() const {

    return shadow_depth_range;
}

void DirectionalLight3D::set_blend_splits(bool p_enable) {

    blend_splits = p_enable;
    RenderingServer::get_singleton()->light_directional_set_blend_splits(light, p_enable);
}

bool DirectionalLight3D::is_blend_splits_enabled() const {

    return blend_splits;
}

void DirectionalLight3D::_validate_property(PropertyInfo &property) const {
    if (shadow_mode == SHADOW_ORTHOGONAL && (property.name == "directional_shadow_split_1" || property.name == "directional_shadow_blend_splits" ||
                                                    property.name == "directional_shadow_bias_split_scale")) {
        // Split 2, split blending and bias split scale are only used with the PSSM 2 Splits and PSSM 4 Splits shadow modes.
        property.usage = PROPERTY_USAGE_NOEDITOR;
    }

    if ((shadow_mode == SHADOW_ORTHOGONAL || shadow_mode == SHADOW_PARALLEL_2_SPLITS) &&
            (property.name == "directional_shadow_split_2" || property.name == "directional_shadow_split_3")) {
        // Splits 3 and 4 are only used with the PSSM 4 Splits shadow mode.
        property.usage = PROPERTY_USAGE_NOEDITOR;
    }

    Light3D::_validate_property(property);
}

void DirectionalLight3D::_bind_methods() {

    SE_BIND_METHOD(DirectionalLight3D,set_shadow_mode);
    SE_BIND_METHOD(DirectionalLight3D,get_shadow_mode);

    MethodBinder::bind_method(
            D_METHOD("set_shadow_depth_range", { "mode" }), &DirectionalLight3D::set_shadow_depth_range);
    SE_BIND_METHOD(DirectionalLight3D,get_shadow_depth_range);

    SE_BIND_METHOD(DirectionalLight3D,set_blend_splits);
    SE_BIND_METHOD(DirectionalLight3D,is_blend_splits_enabled);

    ADD_GROUP("Directional Shadow", "directional_shadow_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "directional_shadow_mode", PropertyHint::Enum,
                         "Orthogonal,PSSM 2 Splits,PSSM 4 Splits"),
            "set_shadow_mode", "get_shadow_mode");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "directional_shadow_split_1", PropertyHint::Range, "0,1,0.001"),
            "set_param", "get_param", PARAM_SHADOW_SPLIT_1_OFFSET);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "directional_shadow_split_2", PropertyHint::Range, "0,1,0.001"),
            "set_param", "get_param", PARAM_SHADOW_SPLIT_2_OFFSET);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "directional_shadow_split_3", PropertyHint::Range, "0,1,0.001"),
            "set_param", "get_param", PARAM_SHADOW_SPLIT_3_OFFSET);
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "directional_shadow_blend_splits"), "set_blend_splits",
            "is_blend_splits_enabled");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "directional_shadow_normal_bias", PropertyHint::Range, "0,10,0.001"),
            "set_param", "get_param", PARAM_SHADOW_NORMAL_BIAS);
    ADD_PROPERTYI(
            PropertyInfo(VariantType::FLOAT, "directional_shadow_bias_split_scale", PropertyHint::Range, "0,1,0.001"),
            "set_param", "get_param", PARAM_SHADOW_BIAS_SPLIT_SCALE);
    ADD_PROPERTY(
            PropertyInfo(VariantType::INT, "directional_shadow_depth_range", PropertyHint::Enum, "Stable,Optimized"),
            "set_shadow_depth_range", "get_shadow_depth_range");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "directional_shadow_max_distance", PropertyHint::ExpRange,
                          "0,8192,0.1,or_greater"),
            "set_param", "get_param", PARAM_SHADOW_MAX_DISTANCE);

    BIND_ENUM_CONSTANT(SHADOW_ORTHOGONAL);
    BIND_ENUM_CONSTANT(SHADOW_PARALLEL_2_SPLITS);
    BIND_ENUM_CONSTANT(SHADOW_PARALLEL_4_SPLITS);

    BIND_ENUM_CONSTANT(SHADOW_DEPTH_RANGE_STABLE);
    BIND_ENUM_CONSTANT(SHADOW_DEPTH_RANGE_OPTIMIZED);
}

DirectionalLight3D::DirectionalLight3D() : Light3D(RS::LIGHT_DIRECTIONAL) {

    set_param(PARAM_SHADOW_NORMAL_BIAS, 0.8f);
    set_param(PARAM_SHADOW_BIAS, 0.1f);
    set_param(PARAM_SHADOW_MAX_DISTANCE, 100);
    set_param(PARAM_SHADOW_BIAS_SPLIT_SCALE, 0.25);
    set_shadow_mode(SHADOW_PARALLEL_4_SPLITS);
    set_shadow_depth_range(SHADOW_DEPTH_RANGE_STABLE);

    blend_splits = false;
}

void OmniLight3D::set_shadow_mode(ShadowMode p_mode) {

    shadow_mode = p_mode;
    RenderingServer::get_singleton()->light_omni_set_shadow_mode(light, RS::LightOmniShadowMode(p_mode));
}

OmniLight3D::ShadowMode OmniLight3D::get_shadow_mode() const {

    return shadow_mode;
}

void OmniLight3D::set_shadow_detail(ShadowDetail p_detail) {

    shadow_detail = p_detail;
    RenderingServer::get_singleton()->light_omni_set_shadow_detail(light, RS::LightOmniShadowDetail(p_detail));
}
OmniLight3D::ShadowDetail OmniLight3D::get_shadow_detail() const {

    return shadow_detail;
}

void OmniLight3D::_bind_methods() {

    SE_BIND_METHOD(OmniLight3D,set_shadow_mode);
    SE_BIND_METHOD(OmniLight3D,get_shadow_mode);

    SE_BIND_METHOD(OmniLight3D,set_shadow_detail);
    SE_BIND_METHOD(OmniLight3D,get_shadow_detail);

    ADD_GROUP("Omni", "omni_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "omni_range", PropertyHint::ExpRange, "0,4096,0.001,or_greater"),
            "set_param", "get_param", PARAM_RANGE);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "omni_attenuation", PropertyHint::ExpEasing, "attenuation"),
            "set_param", "get_param", PARAM_ATTENUATION);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "omni_shadow_mode", PropertyHint::Enum, "Dual Paraboloid,Cube"),
            "set_shadow_mode", "get_shadow_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "omni_shadow_detail", PropertyHint::Enum, "Vertical,Horizontal"),
            "set_shadow_detail", "get_shadow_detail");

    BIND_ENUM_CONSTANT(SHADOW_DUAL_PARABOLOID);
    BIND_ENUM_CONSTANT(SHADOW_CUBE);

    BIND_ENUM_CONSTANT(SHADOW_DETAIL_VERTICAL);
    BIND_ENUM_CONSTANT(SHADOW_DETAIL_HORIZONTAL);
}

OmniLight3D::OmniLight3D() : Light3D(RS::LIGHT_OMNI) {

    set_shadow_mode(SHADOW_CUBE);
    set_shadow_detail(SHADOW_DETAIL_HORIZONTAL);
}

String SpotLight3D::get_configuration_warning() const {
    String warning(Light3D::get_configuration_warning());

    if (has_shadow() && get_param(PARAM_SPOT_ANGLE) >= 90.0f) {
        if (!warning.empty()) {
            warning += "\n\n";
        }

        warning += TTRS("A SpotLight3D with an angle wider than 90 degrees cannot cast shadows.");
    }

    return warning;
}

void SpotLight3D::_bind_methods() {

    ADD_GROUP("Spot", "spot_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "spot_range", PropertyHint::ExpRange, "0,4096,0.001,or_greater"),
            "set_param", "get_param", PARAM_RANGE);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "spot_attenuation", PropertyHint::ExpEasing, "attenuation"),
            "set_param", "get_param", PARAM_ATTENUATION);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "spot_angle", PropertyHint::Range, "0,180,0.01"), "set_param",
            "get_param", PARAM_SPOT_ANGLE);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "spot_angle_attenuation", PropertyHint::ExpEasing, "attenuation"),
            "set_param", "get_param", PARAM_SPOT_ATTENUATION);
}
