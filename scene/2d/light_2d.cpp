/*************************************************************************/
/*  light_2d.cpp                                                         */
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

#include "light_2d.h"

#include "scene/resources/texture.h"
#include "servers/rendering_server.h"
#include "scene/main/scene_tree.h"
#include "core/engine.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/translation_helpers.h"

IMPL_GDCLASS(Light2D)
VARIANT_ENUM_CAST(Light2D::Mode);
VARIANT_ENUM_CAST(Light2D::ShadowFilter);

#ifdef TOOLS_ENABLED
Dictionary Light2D::_edit_get_state() const {
    Dictionary state = Node2D::_edit_get_state();
    state["offset"] = get_texture_offset();
    return state;
}

void Light2D::_edit_set_state(const Dictionary &p_state) {
    Node2D::_edit_set_state(p_state);
    set_texture_offset(p_state["offset"].as<Vector2>());
}

void Light2D::_edit_set_pivot(const Point2 &p_pivot) {
    set_position(get_transform().xform(p_pivot));
    set_texture_offset(get_texture_offset() - p_pivot);
}

Point2 Light2D::_edit_get_pivot() const {
    return Vector2();
}

bool Light2D::_edit_use_pivot() const {
    return true;
}

Rect2 Light2D::_edit_get_rect() const {
    if (not texture)
        return Rect2();

    Size2 s = texture->get_size() * _scale;
    return Rect2(texture_offset - s / 2.0, s);
}

bool Light2D::_edit_use_rect() const {
    return texture;
}
#endif

Rect2 Light2D::get_anchorable_rect() const {
    if (not texture)
        return Rect2();

    Size2 s = texture->get_size() * _scale;
    return Rect2(texture_offset - s / 2.0, s);
}

void Light2D::_update_light_visibility() {

    if (!is_inside_tree())
        return;

    bool editor_ok = true;

#ifdef TOOLS_ENABLED
    if (editor_only) {
        if (!Engine::get_singleton()->is_editor_hint()) {
            editor_ok = false;
        } else {
            editor_ok = (get_tree()->get_edited_scene_root() && (this == get_tree()->get_edited_scene_root() || get_owner() == get_tree()->get_edited_scene_root()));
        }
    }
#else
    if (editor_only) {
        editor_ok = false;
    }
#endif

    RenderingServer::get_singleton()->canvas_light_set_enabled(canvas_light, enabled && is_visible_in_tree() && editor_ok);
}

void Light2D::set_enabled(bool p_enabled) {

    enabled = p_enabled;
    _update_light_visibility();
}

bool Light2D::is_enabled() const {

    return enabled;
}

void Light2D::set_editor_only(bool p_editor_only) {

    editor_only = p_editor_only;
    _update_light_visibility();
}

bool Light2D::is_editor_only() const {

    return editor_only;
}

void Light2D::set_texture(const Ref<Texture> &p_texture) {

    texture = p_texture;
    if (texture)
        RenderingServer::get_singleton()->canvas_light_set_texture(canvas_light, texture->get_rid());
    else
        RenderingServer::get_singleton()->canvas_light_set_texture(canvas_light, entt::null);

    update_configuration_warning();
}

Ref<Texture> Light2D::get_texture() const {

    return texture;
}

void Light2D::set_texture_offset(const Vector2 &p_offset) {

    texture_offset = p_offset;
    RenderingServer::get_singleton()->canvas_light_set_texture_offset(canvas_light, texture_offset);
    item_rect_changed();
    Object_change_notify(this,"offset");
}

Vector2 Light2D::get_texture_offset() const {

    return texture_offset;
}

void Light2D::set_color(const Color &p_color) {

    color = p_color;
    RenderingServer::get_singleton()->canvas_light_set_color(canvas_light, color);
}
Color Light2D::get_color() const {

    return color;
}

void Light2D::set_height(float p_height) {

    height = p_height;
    RenderingServer::get_singleton()->canvas_light_set_height(canvas_light, height);
}

float Light2D::get_height() const {

    return height;
}

void Light2D::set_energy(float p_energy) {

    energy = p_energy;
    RenderingServer::get_singleton()->canvas_light_set_energy(canvas_light, energy);
}

float Light2D::get_energy() const {

    return energy;
}

void Light2D::set_texture_scale(float p_scale) {

    _scale = p_scale;
    // Avoid having 0 scale values, can lead to errors in physics and rendering.
    if (_scale == 0) {
        _scale = CMP_EPSILON;
    }

    RenderingServer::get_singleton()->canvas_light_set_scale(canvas_light, _scale);
    item_rect_changed();
}

float Light2D::get_texture_scale() const {

    return _scale;
}

void Light2D::set_z_range_min(int p_min_z) {

    z_min = p_min_z;
    RenderingServer::get_singleton()->canvas_light_set_z_range(canvas_light, z_min, z_max);
}
int Light2D::get_z_range_min() const {

    return z_min;
}

void Light2D::set_z_range_max(int p_max_z) {

    z_max = p_max_z;
    RenderingServer::get_singleton()->canvas_light_set_z_range(canvas_light, z_min, z_max);
}
int Light2D::get_z_range_max() const {

    return z_max;
}

void Light2D::set_layer_range_min(int p_min_layer) {

    layer_min = p_min_layer;
    RenderingServer::get_singleton()->canvas_light_set_layer_range(canvas_light, layer_min, layer_max);
}
int Light2D::get_layer_range_min() const {

    return layer_min;
}

void Light2D::set_layer_range_max(int p_max_layer) {

    layer_max = p_max_layer;
    RenderingServer::get_singleton()->canvas_light_set_layer_range(canvas_light, layer_min, layer_max);
}
int Light2D::get_layer_range_max() const {

    return layer_max;
}

void Light2D::set_item_cull_mask(int p_mask) {

    item_mask = p_mask;
    RenderingServer::get_singleton()->canvas_light_set_item_cull_mask(canvas_light, item_mask);
}

int Light2D::get_item_cull_mask() const {

    return item_mask;
}

void Light2D::set_item_shadow_cull_mask(int p_mask) {

    item_shadow_mask = p_mask;
    RenderingServer::get_singleton()->canvas_light_set_item_shadow_cull_mask(canvas_light, item_shadow_mask);
}

int Light2D::get_item_shadow_cull_mask() const {

    return item_shadow_mask;
}

void Light2D::set_mode(Mode p_mode) {

    mode = p_mode;
    RenderingServer::get_singleton()->canvas_light_set_mode(canvas_light, RS::CanvasLightMode(p_mode));
}

Light2D::Mode Light2D::get_mode() const {

    return mode;
}

void Light2D::set_shadow_enabled(bool p_enabled) {

    shadow = p_enabled;
    RenderingServer::get_singleton()->canvas_light_set_shadow_enabled(canvas_light, shadow);
}
bool Light2D::is_shadow_enabled() const {

    return shadow;
}

void Light2D::set_shadow_buffer_size(int p_size) {

    shadow_buffer_size = p_size;
    RenderingServer::get_singleton()->canvas_light_set_shadow_buffer_size(canvas_light, shadow_buffer_size);
}

int Light2D::get_shadow_buffer_size() const {

    return shadow_buffer_size;
}

void Light2D::set_shadow_gradient_length(float p_multiplier) {

    shadow_gradient_length = p_multiplier;
    RenderingServer::get_singleton()->canvas_light_set_shadow_gradient_length(canvas_light, p_multiplier);
}

float Light2D::get_shadow_gradient_length() const {

    return shadow_gradient_length;
}

void Light2D::set_shadow_filter(ShadowFilter p_filter) {
    shadow_filter = p_filter;
    RenderingServer::get_singleton()->canvas_light_set_shadow_filter(canvas_light, RS::CanvasLightShadowFilter(p_filter));
}

Light2D::ShadowFilter Light2D::get_shadow_filter() const {

    return shadow_filter;
}

void Light2D::set_shadow_color(const Color &p_shadow_color) {
    shadow_color = p_shadow_color;
    RenderingServer::get_singleton()->canvas_light_set_shadow_color(canvas_light, shadow_color);
}

Color Light2D::get_shadow_color() const {
    return shadow_color;
}

void Light2D::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {

        RenderingServer::get_singleton()->canvas_light_attach_to_canvas(canvas_light, get_canvas());
        _update_light_visibility();
    }
    else if (p_what == NOTIFICATION_TRANSFORM_CHANGED) {

        RenderingServer::get_singleton()->canvas_light_set_transform(canvas_light, get_global_transform());
    }
    else if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {

        _update_light_visibility();
    }

    else if (p_what == NOTIFICATION_EXIT_TREE) {

        RenderingServer::get_singleton()->canvas_light_attach_to_canvas(canvas_light, entt::null);
        _update_light_visibility();
    }
}

String Light2D::get_configuration_warning() const {

    String warning = Node2D::get_configuration_warning();
    if (!texture) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("A texture with the shape of the light must be supplied to the \"Texture\" property.");
    }

    return warning;

}

void Light2D::set_shadow_smooth(float p_amount) {

    shadow_smooth = p_amount;
    RenderingServer::get_singleton()->canvas_light_set_shadow_smooth(canvas_light, shadow_smooth);
}

float Light2D::get_shadow_smooth() const {

    return shadow_smooth;
}

void Light2D::_bind_methods() {

    SE_BIND_METHOD(Light2D,set_enabled);
    SE_BIND_METHOD(Light2D,is_enabled);

    SE_BIND_METHOD(Light2D,set_editor_only);
    SE_BIND_METHOD(Light2D,is_editor_only);

    SE_BIND_METHOD(Light2D,set_texture);
    SE_BIND_METHOD(Light2D,get_texture);

    SE_BIND_METHOD(Light2D,set_texture_offset);
    SE_BIND_METHOD(Light2D,get_texture_offset);

    SE_BIND_METHOD(Light2D,set_color);
    SE_BIND_METHOD(Light2D,get_color);

    SE_BIND_METHOD(Light2D,set_height);
    SE_BIND_METHOD(Light2D,get_height);

    SE_BIND_METHOD(Light2D,set_energy);
    SE_BIND_METHOD(Light2D,get_energy);

    SE_BIND_METHOD(Light2D,set_texture_scale);
    SE_BIND_METHOD(Light2D,get_texture_scale);

    SE_BIND_METHOD(Light2D,set_z_range_min);
    SE_BIND_METHOD(Light2D,get_z_range_min);

    SE_BIND_METHOD(Light2D,set_z_range_max);
    SE_BIND_METHOD(Light2D,get_z_range_max);

    SE_BIND_METHOD(Light2D,set_layer_range_min);
    SE_BIND_METHOD(Light2D,get_layer_range_min);

    SE_BIND_METHOD(Light2D,set_layer_range_max);
    SE_BIND_METHOD(Light2D,get_layer_range_max);

    SE_BIND_METHOD(Light2D,set_item_cull_mask);
    SE_BIND_METHOD(Light2D,get_item_cull_mask);

    SE_BIND_METHOD(Light2D,set_item_shadow_cull_mask);
    SE_BIND_METHOD(Light2D,get_item_shadow_cull_mask);

    SE_BIND_METHOD(Light2D,set_mode);
    SE_BIND_METHOD(Light2D,get_mode);

    SE_BIND_METHOD(Light2D,set_shadow_enabled);
    SE_BIND_METHOD(Light2D,is_shadow_enabled);

    SE_BIND_METHOD(Light2D,set_shadow_buffer_size);
    SE_BIND_METHOD(Light2D,get_shadow_buffer_size);

    SE_BIND_METHOD(Light2D,set_shadow_smooth);
    SE_BIND_METHOD(Light2D,get_shadow_smooth);

    SE_BIND_METHOD(Light2D,set_shadow_gradient_length);
    SE_BIND_METHOD(Light2D,get_shadow_gradient_length);

    SE_BIND_METHOD(Light2D,set_shadow_filter);
    SE_BIND_METHOD(Light2D,get_shadow_filter);

    SE_BIND_METHOD(Light2D,set_shadow_color);
    SE_BIND_METHOD(Light2D,get_shadow_color);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "enabled"), "set_enabled", "is_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "editor_only"), "set_editor_only", "is_editor_only");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "offset"), "set_texture_offset", "get_texture_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "texture_scale", PropertyHint::Range, "0.01,50,0.01"), "set_texture_scale", "get_texture_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "color"), "set_color", "get_color");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "energy", PropertyHint::Range, "0,16,0.01,or_greater"), "set_energy", "get_energy");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "mode", PropertyHint::Enum, "Add,Sub,Mix,Mask"), "set_mode", "get_mode");
    ADD_GROUP("Range", "range_");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "range_height", PropertyHint::Range, "-2048,2048,0.1,or_lesser,or_greater"), "set_height", "get_height");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "range_z_min", PropertyHint::Range, itos(RS::CANVAS_ITEM_Z_MIN) + "," + itos(RS::CANVAS_ITEM_Z_MAX) + ",1"), "set_z_range_min", "get_z_range_min");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "range_z_max", PropertyHint::Range, itos(RS::CANVAS_ITEM_Z_MIN) + "," + itos(RS::CANVAS_ITEM_Z_MAX) + ",1"), "set_z_range_max", "get_z_range_max");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "range_layer_min", PropertyHint::Range, "-512,512,1"), "set_layer_range_min", "get_layer_range_min");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "range_layer_max", PropertyHint::Range, "-512,512,1"), "set_layer_range_max", "get_layer_range_max");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "range_item_cull_mask", PropertyHint::Layers2DRenderer), "set_item_cull_mask", "get_item_cull_mask");

    ADD_GROUP("Shadow", "shadow_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "shadow_enabled"), "set_shadow_enabled", "is_shadow_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "shadow_color"), "set_shadow_color", "get_shadow_color");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "shadow_buffer_size", PropertyHint::Range, "32,16384,1"), "set_shadow_buffer_size", "get_shadow_buffer_size");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "shadow_gradient_length", PropertyHint::Range, "0,4096,0.1"), "set_shadow_gradient_length", "get_shadow_gradient_length");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "shadow_filter", PropertyHint::Enum, "None,PCF3,PCF5,PCF7,PCF9,PCF13"), "set_shadow_filter", "get_shadow_filter");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "shadow_filter_smooth", PropertyHint::Range, "0,64,0.1"), "set_shadow_smooth", "get_shadow_smooth");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "shadow_item_cull_mask", PropertyHint::Layers2DRenderer), "set_item_shadow_cull_mask", "get_item_shadow_cull_mask");

    BIND_ENUM_CONSTANT(MODE_ADD);
    BIND_ENUM_CONSTANT(MODE_SUB);
    BIND_ENUM_CONSTANT(MODE_MIX);
    BIND_ENUM_CONSTANT(MODE_MASK);

    BIND_ENUM_CONSTANT(SHADOW_FILTER_NONE);
    BIND_ENUM_CONSTANT(SHADOW_FILTER_PCF3);
    BIND_ENUM_CONSTANT(SHADOW_FILTER_PCF5);
    BIND_ENUM_CONSTANT(SHADOW_FILTER_PCF7);
    BIND_ENUM_CONSTANT(SHADOW_FILTER_PCF9);
    BIND_ENUM_CONSTANT(SHADOW_FILTER_PCF13);
}

Light2D::Light2D() {

    canvas_light = RenderingServer::get_singleton()->canvas_light_create();
    enabled = true;
    editor_only = false;
    shadow = false;
    color = Color(1, 1, 1);
    height = 0;
    _scale = 1.0;
    z_min = -1024;
    z_max = 1024;
    layer_min = 0;
    layer_max = 0;
    item_mask = 1;
    item_shadow_mask = 1;
    mode = MODE_ADD;
    shadow_buffer_size = 2048;
    shadow_gradient_length = 0;
    energy = 1.0;
    shadow_color = Color(0, 0, 0, 0);
    shadow_filter = SHADOW_FILTER_NONE;
    shadow_smooth = 0;

    set_notify_transform(true);
}

Light2D::~Light2D() {

    RenderingServer::get_singleton()->free_rid(canvas_light);
}
