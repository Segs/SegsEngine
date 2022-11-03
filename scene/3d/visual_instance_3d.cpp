/*************************************************************************/
/*  visual_instance_3d.cpp                                                  */
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

#include "visual_instance_3d.h"

#include "core/ecs_registry.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "servers/rendering_server.h"
#include "scene/resources/world_3d.h"

IMPL_GDCLASS(VisualInstance3D)
IMPL_GDCLASS(GeometryInstance)

VARIANT_ENUM_CAST(GeometryInstance::Flags);
VARIANT_ENUM_CAST(GeometryInstance::LightmapScale);
VARIANT_ENUM_CAST(GeometryInstance::ShadowCastingSetting);

AABB VisualInstance3D::get_transformed_aabb() const {

    return get_global_transform().xform(get_aabb());
}

void VisualInstance3D::_update_visibility() {

    if (!is_inside_tree()) {
        return;
    }

    bool visible = is_visible_in_tree();

    // keep a quick flag available in each node.
    // no need to call is_visible_in_tree all over the place,
    // providing it is propagated with a notification.
    bool already_visible = _is_vi_visible();
    _set_vi_visible(visible);

    // if making visible, make sure the visual server is up to date with the transform
    if (visible && !already_visible && !_is_using_identity_transform()) {
        Transform gt = get_global_transform();
        RenderingServer::get_singleton()->instance_set_transform(instance, gt);
    }

    Object_change_notify(this,"visible");
    RenderingServer::get_singleton()->instance_set_visible(get_instance(), visible);
}

void VisualInstance3D::set_instance_use_identity_transform(bool p_enable) {
    // prevent sending instance transforms when using global coords
    _set_use_identity_transform(p_enable);

    if (is_inside_tree()) {
        if (p_enable) {
            // want to make sure instance is using identity transform
            RenderingServer::get_singleton()->instance_set_transform(instance, get_global_transform());
        } else {
            // want to make sure instance is up to date
            RenderingServer::get_singleton()->instance_set_transform(instance, Transform());
        }
    }
}

void VisualInstance3D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_WORLD: {

            // CHECK SKELETON => moving skeleton attaching logic to MeshInstance3D
            /*
            Skeleton *skeleton=object_cast<Skeleton>(get_parent());
            if (skeleton)
                RenderingServer::get_singleton()->instance_attach_skeleton( instance, skeleton->get_skeleton() );
            */
            ERR_FAIL_COND(not get_world_3d());
            RenderingServer::get_singleton()->instance_set_scenario(instance, get_world_3d()->get_scenario());
            _update_visibility();

        } break;
        case NOTIFICATION_TRANSFORM_CHANGED: {
            if (_is_vi_visible()) {
                if (!_is_using_identity_transform()) {
                    Transform gt = get_global_transform();
                    RenderingServer::get_singleton()->instance_set_transform(instance, gt);
                }
            }
        } break;
        case NOTIFICATION_EXIT_WORLD: {

            RenderingServer::get_singleton()->instance_set_scenario(instance, entt::null);
            RenderingServer::get_singleton()->instance_attach_skeleton(instance, entt::null);
            //RenderingServer::get_singleton()->instance_geometry_set_baked_light_sampler(instance, entt::null );

            //  the vi visible flag is always set to invisible when outside the tree,
            //  so it can detect re-entering the tree and becoming visible, and send
            //  the transform to the visual server
            _set_vi_visible(false);
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {

            _update_visibility();
        } break;
    }
}

void VisualInstance3D::set_layer_mask(uint32_t p_mask) {

    layers = p_mask;
    RenderingServer::get_singleton()->instance_set_layer_mask(instance, p_mask);
}

void VisualInstance3D::set_layer_mask_bit(int p_layer, bool p_enable) {
    ERR_FAIL_INDEX(p_layer, 32);
    if (p_enable) {
        set_layer_mask(layers | (1 << p_layer));
    } else {
        set_layer_mask(layers & (~(1 << p_layer)));
    }
}

bool VisualInstance3D::get_layer_mask_bit(int p_layer) const {
    ERR_FAIL_INDEX_V(p_layer, 32, false);
    return (layers & (1 << p_layer));
}

void VisualInstance3D::_bind_methods() {

    //MethodBinder::bind_method(D_METHOD("_get_visual_instance_rid"), &VisualInstance3D::get_instance);
    SE_BIND_METHOD(VisualInstance3D,set_base);
    SE_BIND_METHOD(VisualInstance3D,get_base);
    SE_BIND_METHOD(VisualInstance3D,get_instance);
    SE_BIND_METHOD(VisualInstance3D,set_layer_mask);
    SE_BIND_METHOD(VisualInstance3D,get_layer_mask);
    SE_BIND_METHOD(VisualInstance3D,set_layer_mask_bit);
    SE_BIND_METHOD(VisualInstance3D,get_layer_mask_bit);

    SE_BIND_METHOD(VisualInstance3D,get_transformed_aabb);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "layers", PropertyHint::Layers3DRenderer), "set_layer_mask", "get_layer_mask");
}

void VisualInstance3D::set_base(RenderingEntity p_base) {

    RenderingServer::get_singleton()->instance_set_base(instance, p_base);
    base = p_base;
}

VisualInstance3D::VisualInstance3D(): layers(1) {

    instance = RenderingServer::get_singleton()->instance_create();
    game_object_registry.registry.emplace<CullInstanceComponent>(get_instance_id());
    RenderingServer::get_singleton()->instance_attach_object_instance_id(instance, get_instance_id());
    set_notify_transform(true);
}

VisualInstance3D::~VisualInstance3D() {
    game_object_registry.registry.remove<CullInstanceComponent>(get_instance_id());

    RenderingServer::get_singleton()->free_rid(instance);
}

void GeometryInstance::set_material_override(const Ref<Material> &p_material) {

    material_override = p_material;
    RenderingServer::get_singleton()->instance_geometry_set_material_override(get_instance(), p_material ? p_material->get_rid() : entt::null);
}

Ref<Material> GeometryInstance::get_material_override() const {

    return material_override;
}

void GeometryInstance::set_material_overlay(const Ref<Material> &p_material) {
    material_overlay = p_material;
    RenderingServer::get_singleton()->instance_geometry_set_material_overlay(
            get_instance(), p_material ? p_material->get_rid() : entt::null);
}
void GeometryInstance::set_generate_lightmap(bool p_enabled) {
    generate_lightmap = p_enabled;
}

bool GeometryInstance::get_generate_lightmap() {
    return generate_lightmap;
}

void GeometryInstance::set_lightmap_scale(LightmapScale p_scale) {
    ERR_FAIL_INDEX(p_scale, LIGHTMAP_SCALE_MAX);
    lightmap_scale = p_scale;
}

GeometryInstance::LightmapScale GeometryInstance::get_lightmap_scale() const {
    return lightmap_scale;
}

void GeometryInstance::set_lod_min_distance(float p_dist) {

    lod_min_distance = p_dist;
    RenderingServer::get_singleton()->instance_geometry_set_draw_range(get_instance(), lod_min_distance, lod_max_distance, lod_min_hysteresis, lod_max_hysteresis);
}

void GeometryInstance::set_lod_max_distance(float p_dist) {

    lod_max_distance = p_dist;
    RenderingServer::get_singleton()->instance_geometry_set_draw_range(get_instance(), lod_min_distance, lod_max_distance, lod_min_hysteresis, lod_max_hysteresis);
}

float GeometryInstance::get_lod_max_distance() const {

    return lod_max_distance;
}

void GeometryInstance::set_lod_min_hysteresis(float p_dist) {

    lod_min_hysteresis = p_dist;
    RenderingServer::get_singleton()->instance_geometry_set_draw_range(get_instance(), lod_min_distance, lod_max_distance, lod_min_hysteresis, lod_max_hysteresis);
}

float GeometryInstance::get_lod_min_hysteresis() const {

    return lod_min_hysteresis;
}

void GeometryInstance::set_lod_max_hysteresis(float p_dist) {

    lod_max_hysteresis = p_dist;
    RenderingServer::get_singleton()->instance_geometry_set_draw_range(get_instance(), lod_min_distance, lod_max_distance, lod_min_hysteresis, lod_max_hysteresis);
}

float GeometryInstance::get_lod_max_hysteresis() const {

    return lod_max_hysteresis;
}

void GeometryInstance::_notification(int p_what) {
}

void GeometryInstance::set_flag(Flags p_flag, bool p_value) {

    ERR_FAIL_INDEX(p_flag, FLAG_MAX);
    if (flags[p_flag] == p_value)
        return;

    flags[p_flag] = p_value;
    RenderingServer::get_singleton()->instance_geometry_set_flag(get_instance(), (RS::InstanceFlags)p_flag, p_value);
}

bool GeometryInstance::get_flag(Flags p_flag) const {

    ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);

    return flags[p_flag];
}

void GeometryInstance::set_cast_shadows_setting(ShadowCastingSetting p_shadow_casting_setting) {

    shadow_casting_setting = p_shadow_casting_setting;

    RenderingServer::get_singleton()->instance_geometry_set_cast_shadows_setting(get_instance(), (RS::ShadowCastingSetting)p_shadow_casting_setting);
}

GeometryInstance::ShadowCastingSetting GeometryInstance::get_cast_shadows_setting() const {

    return shadow_casting_setting;
}

void GeometryInstance::set_extra_cull_margin(float p_margin) {

    ERR_FAIL_COND(p_margin < 0);
    extra_cull_margin = p_margin;
    RenderingServer::get_singleton()->instance_set_extra_visibility_margin(get_instance(), extra_cull_margin);
}

float GeometryInstance::get_extra_cull_margin() const {

    return extra_cull_margin;
}

void GeometryInstance::set_custom_aabb(AABB aabb) {

    RenderingServer::get_singleton()->instance_set_custom_aabb(get_instance(), aabb);
}

void GeometryInstance::_bind_methods() {

    SE_BIND_METHOD(GeometryInstance,set_material_override);
    SE_BIND_METHOD(GeometryInstance,get_material_override);

    SE_BIND_METHOD(GeometryInstance,set_material_overlay);
    SE_BIND_METHOD(GeometryInstance,get_material_overlay);
    SE_BIND_METHOD(GeometryInstance,set_flag);
    SE_BIND_METHOD(GeometryInstance,get_flag);

    SE_BIND_METHOD(GeometryInstance,set_cast_shadows_setting);
    SE_BIND_METHOD(GeometryInstance,get_cast_shadows_setting);

    SE_BIND_METHOD(GeometryInstance,set_generate_lightmap);
    SE_BIND_METHOD(GeometryInstance,get_generate_lightmap);

    SE_BIND_METHOD(GeometryInstance,set_lightmap_scale);
    SE_BIND_METHOD(GeometryInstance,get_lightmap_scale);
    SE_BIND_METHOD(GeometryInstance,set_lod_max_hysteresis);
    SE_BIND_METHOD(GeometryInstance,get_lod_max_hysteresis);

    SE_BIND_METHOD(GeometryInstance,set_lod_max_distance);
    SE_BIND_METHOD(GeometryInstance,get_lod_max_distance);

    SE_BIND_METHOD(GeometryInstance,set_lod_min_hysteresis);
    SE_BIND_METHOD(GeometryInstance,get_lod_min_hysteresis);

    SE_BIND_METHOD(GeometryInstance,set_lod_min_distance);
    SE_BIND_METHOD(GeometryInstance,get_lod_min_distance);

    SE_BIND_METHOD(GeometryInstance,set_extra_cull_margin);
    SE_BIND_METHOD(GeometryInstance,get_extra_cull_margin);

    SE_BIND_METHOD(GeometryInstance,set_custom_aabb);

    SE_BIND_METHOD(GeometryInstance,get_aabb);

    ADD_GROUP("Geometry", "");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "material_override", PropertyHint::ResourceType, "ShaderMaterial,SpatialMaterial"), "set_material_override", "get_material_override");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "material_overlay", PropertyHint::ResourceType, "ShaderMaterial,SpatialMaterial"), "set_material_overlay", "get_material_overlay");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "cast_shadow", PropertyHint::Enum, "Off,On,Double-Sided,Shadows Only"), "set_cast_shadows_setting", "get_cast_shadows_setting");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "extra_cull_margin", PropertyHint::Range, "0,16384,0.01"), "set_extra_cull_margin", "get_extra_cull_margin");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "use_in_baked_light"), "set_flag", "get_flag", FLAG_USE_BAKED_LIGHT);
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "generate_lightmap"), "set_generate_lightmap", "get_generate_lightmap");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "lightmap_scale", PropertyHint::Enum, "1x,2x,4x,8x"), "set_lightmap_scale", "get_lightmap_scale");

    ADD_GROUP("LOD", "lod_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "lod_min_distance", PropertyHint::Range, "0,32768,0.01"), "set_lod_min_distance", "get_lod_min_distance");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "lod_min_hysteresis", PropertyHint::Range, "0,32768,0.01"), "set_lod_min_hysteresis", "get_lod_min_hysteresis");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "lod_max_distance", PropertyHint::Range, "0,32768,0.01"), "set_lod_max_distance", "get_lod_max_distance");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "lod_max_hysteresis", PropertyHint::Range, "0,32768,0.01"), "set_lod_max_hysteresis", "get_lod_max_hysteresis");

    //ADD_SIGNAL( MethodInfo("visibility_changed"));

    BIND_ENUM_CONSTANT(LIGHTMAP_SCALE_1X);
    BIND_ENUM_CONSTANT(LIGHTMAP_SCALE_2X);
    BIND_ENUM_CONSTANT(LIGHTMAP_SCALE_4X);
    BIND_ENUM_CONSTANT(LIGHTMAP_SCALE_8X);
    BIND_ENUM_CONSTANT(LIGHTMAP_SCALE_MAX);
    BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_OFF);
    BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_ON);
    BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_DOUBLE_SIDED);
    BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_SHADOWS_ONLY);

    BIND_ENUM_CONSTANT(FLAG_USE_BAKED_LIGHT);
    BIND_ENUM_CONSTANT(FLAG_DRAW_NEXT_FRAME_IF_VISIBLE);
    BIND_ENUM_CONSTANT(FLAG_MAX);
}

GeometryInstance::GeometryInstance() {
    lod_min_distance = 0;
    lod_max_distance = 0;
    lod_min_hysteresis = 0;
    lod_max_hysteresis = 0;

    for (bool & flag : flags) {
        flag = false;
    }

    shadow_casting_setting = SHADOW_CASTING_SETTING_ON;
    extra_cull_margin = 0;
    generate_lightmap = true;
    lightmap_scale = LightmapScale::LIGHTMAP_SCALE_1X;
    //RenderingServer::get_singleton()->instance_geometry_set_baked_light_texture_index(get_instance(),0);
}
