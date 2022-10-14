/*************************************************************************/
/*  reflection_probe.cpp                                                 */
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

#include "reflection_probe.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"

IMPL_GDCLASS(ReflectionProbe)
VARIANT_ENUM_CAST(ReflectionProbe::UpdateMode);

void ReflectionProbe::set_intensity(float p_intensity) {

    intensity = p_intensity;
    RenderingServer::get_singleton()->reflection_probe_set_intensity(probe, p_intensity);
}

float ReflectionProbe::get_intensity() const {

    return intensity;
}

void ReflectionProbe::set_interior_ambient(Color p_ambient) {

    interior_ambient = p_ambient;
    RenderingServer::get_singleton()->reflection_probe_set_interior_ambient(probe, p_ambient);
}

void ReflectionProbe::set_interior_ambient_energy(float p_energy) {
    interior_ambient_energy = p_energy;
    RenderingServer::get_singleton()->reflection_probe_set_interior_ambient_energy(probe, p_energy);
}

float ReflectionProbe::get_interior_ambient_energy() const {
    return interior_ambient_energy;
}

Color ReflectionProbe::get_interior_ambient() const {

    return interior_ambient;
}

void ReflectionProbe::set_interior_ambient_probe_contribution(float p_contribution) {

    interior_ambient_probe_contribution = p_contribution;
    RenderingServer::get_singleton()->reflection_probe_set_interior_ambient_probe_contribution(probe, p_contribution);
}

float ReflectionProbe::get_interior_ambient_probe_contribution() const {

    return interior_ambient_probe_contribution;
}

void ReflectionProbe::set_max_distance(float p_distance) {

    max_distance = p_distance;
    RenderingServer::get_singleton()->reflection_probe_set_max_distance(probe, p_distance);
}
float ReflectionProbe::get_max_distance() const {

    return max_distance;
}

void ReflectionProbe::set_extents(const Vector3 &p_extents) {

    extents = p_extents;

    for (int i = 0; i < 3; i++) {
        if (extents[i] < 0.01) {
            extents[i] = 0.01;
        }

        if (extents[i] - 0.01 < ABS(origin_offset[i])) {
            origin_offset[i] = SGN(origin_offset[i]) * (extents[i] - 0.01);
            Object_change_notify(this,"origin_offset");
        }
    }

    RenderingServer::get_singleton()->reflection_probe_set_extents(probe, extents);
    RenderingServer::get_singleton()->reflection_probe_set_origin_offset(probe, origin_offset);
    Object_change_notify(this,"extents");
    update_gizmo();
}
Vector3 ReflectionProbe::get_extents() const {

    return extents;
}

void ReflectionProbe::set_origin_offset(const Vector3 &p_extents) {

    origin_offset = p_extents;

    for (int i = 0; i < 3; i++) {

        if (extents[i] - 0.01 < ABS(origin_offset[i])) {
            origin_offset[i] = SGN(origin_offset[i]) * (extents[i] - 0.01);
        }
    }
    RenderingServer::get_singleton()->reflection_probe_set_extents(probe, extents);
    RenderingServer::get_singleton()->reflection_probe_set_origin_offset(probe, origin_offset);

    Object_change_notify(this,"origin_offset");
    update_gizmo();
}
Vector3 ReflectionProbe::get_origin_offset() const {

    return origin_offset;
}

void ReflectionProbe::set_enable_box_projection(bool p_enable) {

    box_projection = p_enable;
    RenderingServer::get_singleton()->reflection_probe_set_enable_box_projection(probe, p_enable);
}
bool ReflectionProbe::is_box_projection_enabled() const {

    return box_projection;
}

void ReflectionProbe::set_as_interior(bool p_enable) {

    interior = p_enable;
    RenderingServer::get_singleton()->reflection_probe_set_as_interior(probe, interior);
    Object_change_notify(this);
}

bool ReflectionProbe::is_set_as_interior() const {

    return interior;
}

void ReflectionProbe::set_enable_shadows(bool p_enable) {

    enable_shadows = p_enable;
    RenderingServer::get_singleton()->reflection_probe_set_enable_shadows(probe, p_enable);
}
bool ReflectionProbe::are_shadows_enabled() const {

    return enable_shadows;
}

void ReflectionProbe::set_cull_mask(uint32_t p_layers) {

    cull_mask = p_layers;
    RenderingServer::get_singleton()->reflection_probe_set_cull_mask(probe, p_layers);
}
uint32_t ReflectionProbe::get_cull_mask() const {

    return cull_mask;
}

void ReflectionProbe::set_update_mode(UpdateMode p_mode) {
    update_mode = p_mode;
    RenderingServer::get_singleton()->reflection_probe_set_update_mode(probe, RS::ReflectionProbeUpdateMode(p_mode));
}

ReflectionProbe::UpdateMode ReflectionProbe::get_update_mode() const {
    return update_mode;
}

AABB ReflectionProbe::get_aabb() const {

    AABB aabb;
    aabb.position = -origin_offset;
    aabb.size = origin_offset + extents;
    return aabb;
}
Vector<Face3> ReflectionProbe::get_faces(uint32_t p_usage_flags) const {

    return Vector<Face3>();
}

void ReflectionProbe::_validate_property(PropertyInfo &property) const {

    if (property.name == "interior/ambient_color" || property.name == "interior/ambient_energy" || property.name == "interior/ambient_contrib") {
        if (!interior) {
            property.usage = PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL;
        }
    }
}

void ReflectionProbe::_bind_methods() {

    BIND_METHOD(ReflectionProbe,set_intensity);
    BIND_METHOD(ReflectionProbe,get_intensity);

    BIND_METHOD(ReflectionProbe,set_interior_ambient);
    BIND_METHOD(ReflectionProbe,get_interior_ambient);

    BIND_METHOD(ReflectionProbe,set_interior_ambient_energy);
    BIND_METHOD(ReflectionProbe,get_interior_ambient_energy);

    BIND_METHOD(ReflectionProbe,set_interior_ambient_probe_contribution);
    BIND_METHOD(ReflectionProbe,get_interior_ambient_probe_contribution);

    BIND_METHOD(ReflectionProbe,set_max_distance);
    BIND_METHOD(ReflectionProbe,get_max_distance);

    BIND_METHOD(ReflectionProbe,set_extents);
    BIND_METHOD(ReflectionProbe,get_extents);

    BIND_METHOD(ReflectionProbe,set_origin_offset);
    BIND_METHOD(ReflectionProbe,get_origin_offset);

    BIND_METHOD(ReflectionProbe,set_as_interior);
    BIND_METHOD(ReflectionProbe,is_set_as_interior);

    BIND_METHOD(ReflectionProbe,set_enable_box_projection);
    BIND_METHOD(ReflectionProbe,is_box_projection_enabled);

    BIND_METHOD(ReflectionProbe,set_enable_shadows);
    BIND_METHOD(ReflectionProbe,are_shadows_enabled);

    BIND_METHOD(ReflectionProbe,set_cull_mask);
    BIND_METHOD(ReflectionProbe,get_cull_mask);

    BIND_METHOD(ReflectionProbe,set_update_mode);
    BIND_METHOD(ReflectionProbe,get_update_mode);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "update_mode", PropertyHint::Enum, "Once (Fast),Always (Slow)"), "set_update_mode", "get_update_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "intensity", PropertyHint::Range, "0,1,0.01"), "set_intensity", "get_intensity");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "max_distance", PropertyHint::ExpRange, "0,16384,0.1,or_greater"), "set_max_distance", "get_max_distance");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "extents"), "set_extents", "get_extents");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "origin_offset"), "set_origin_offset", "get_origin_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "box_projection"), "set_enable_box_projection", "is_box_projection_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "enable_shadows"), "set_enable_shadows", "are_shadows_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "cull_mask", PropertyHint::Layers3DRenderer), "set_cull_mask", "get_cull_mask");

    ADD_GROUP("Interior", "interior_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "interior_enable"), "set_as_interior", "is_set_as_interior");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "interior_ambient_color", PropertyHint::ColorNoAlpha), "set_interior_ambient", "get_interior_ambient");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "interior_ambient_energy", PropertyHint::Range, "0,16,0.01"), "set_interior_ambient_energy", "get_interior_ambient_energy");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "interior_ambient_contrib", PropertyHint::Range, "0,1,0.01"), "set_interior_ambient_probe_contribution", "get_interior_ambient_probe_contribution");

    BIND_ENUM_CONSTANT(UPDATE_ONCE);
    BIND_ENUM_CONSTANT(UPDATE_ALWAYS);
}

ReflectionProbe::ReflectionProbe() {

    intensity = 1.0;
    interior_ambient = Color(0, 0, 0);
    interior_ambient_probe_contribution = 0;
    interior_ambient_energy = 1.0;
    max_distance = 0;
    extents = Vector3(1, 1, 1);
    origin_offset = Vector3(0, 0, 0);
    box_projection = false;
    interior = false;
    enable_shadows = false;
    cull_mask = (1 << 20) - 1;
    update_mode = UPDATE_ONCE;

    probe = RenderingServer::get_singleton()->reflection_probe_create();
    RenderingServer::get_singleton()->instance_set_base(get_instance(), probe);
    set_disable_scale(true);
}

ReflectionProbe::~ReflectionProbe() {

    RenderingServer::get_singleton()->free_rid(probe);
}
