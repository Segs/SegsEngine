/*************************************************************************/
/*  light_3d.h                                                              */
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

#pragma once

#include "scene/3d/visual_instance_3d.h"
#include "servers/rendering_server.h"

class GODOT_EXPORT Light3D : public VisualInstance3D {

    GDCLASS(Light3D,VisualInstance3D)

    OBJ_CATEGORY("3D Light3D Nodes")

public:
    enum Param {
        PARAM_ENERGY = RS::LIGHT_PARAM_ENERGY,
        PARAM_INDIRECT_ENERGY = RS::LIGHT_PARAM_INDIRECT_ENERGY,
        PARAM_SIZE = RS::LIGHT_PARAM_SIZE,
        PARAM_SPECULAR = RS::LIGHT_PARAM_SPECULAR,
        PARAM_RANGE = RS::LIGHT_PARAM_RANGE,
        PARAM_ATTENUATION = RS::LIGHT_PARAM_ATTENUATION,
        PARAM_SPOT_ANGLE = RS::LIGHT_PARAM_SPOT_ANGLE,
        PARAM_SPOT_ATTENUATION = RS::LIGHT_PARAM_SPOT_ATTENUATION,
        PARAM_CONTACT_SHADOW_SIZE = RS::LIGHT_PARAM_CONTACT_SHADOW_SIZE,
        PARAM_SHADOW_MAX_DISTANCE = RS::LIGHT_PARAM_SHADOW_MAX_DISTANCE,
        PARAM_SHADOW_SPLIT_1_OFFSET = RS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET,
        PARAM_SHADOW_SPLIT_2_OFFSET = RS::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET,
        PARAM_SHADOW_SPLIT_3_OFFSET = RS::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET,
        PARAM_SHADOW_NORMAL_BIAS = RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS,
        PARAM_SHADOW_BIAS = RS::LIGHT_PARAM_SHADOW_BIAS,
        PARAM_SHADOW_BIAS_SPLIT_SCALE = RS::LIGHT_PARAM_SHADOW_BIAS_SPLIT_SCALE,
        PARAM_MAX = RS::LIGHT_PARAM_MAX
    };

    enum BakeMode : int8_t {
        BAKE_DISABLED,
        BAKE_INDIRECT,
        BAKE_ALL
    };

private:
    Color color;
    float param[PARAM_MAX];
    Color shadow_color;
    uint32_t cull_mask;
    RS::LightType type;
    BakeMode bake_mode;
    bool shadow;
    bool negative;
    bool reverse_cull;
    bool editor_only;

    void _update_visibility();

protected:
    RenderingEntity light;

    static void _bind_methods();
    void _notification(int p_what);
    void _validate_property(PropertyInfo &property) const override;

    Light3D(RS::LightType p_type);

public:
    RS::LightType get_light_type() const { return type; }

    void set_editor_only(bool p_editor_only);
    bool is_editor_only() const;

    void set_param(Param p_param, float p_value);
    float get_param(Param p_param) const;

    void set_shadow(bool p_enable);
    bool has_shadow() const;

    void set_negative(bool p_enable);
    bool is_negative() const;

    void set_cull_mask(uint32_t p_cull_mask);
    uint32_t get_cull_mask() const;

    void set_color(const Color &p_color);
    Color get_color() const;

    void set_shadow_color(const Color &p_shadow_color);
    Color get_shadow_color() const;

    void set_shadow_reverse_cull_face(bool p_enable);
    bool get_shadow_reverse_cull_face() const;

    void set_bake_mode(BakeMode p_mode);
    BakeMode get_bake_mode() const { return bake_mode; }

    AABB get_aabb() const override;
    Vector<Face3> get_faces(uint32_t p_usage_flags) const override;

    Light3D();
    ~Light3D() override;
};

class GODOT_EXPORT DirectionalLight3D : public Light3D {

    GDCLASS(DirectionalLight3D,Light3D)

public:
    enum ShadowMode {
        SHADOW_ORTHOGONAL,
        SHADOW_PARALLEL_2_SPLITS,
        SHADOW_PARALLEL_4_SPLITS
    };

    enum ShadowDepthRange {
        SHADOW_DEPTH_RANGE_STABLE = RS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE,
        SHADOW_DEPTH_RANGE_OPTIMIZED = RS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_OPTIMIZED,
    };

private:
    bool blend_splits;
    ShadowMode shadow_mode;
    ShadowDepthRange shadow_depth_range;

protected:
    static void _bind_methods();
    void _validate_property(PropertyInfo &property) const override;

public:
    void set_shadow_mode(ShadowMode p_mode);
    ShadowMode get_shadow_mode() const;

    void set_shadow_depth_range(ShadowDepthRange p_range);
    ShadowDepthRange get_shadow_depth_range() const;

    void set_blend_splits(bool p_enable);
    bool is_blend_splits_enabled() const;

    DirectionalLight3D();
};


class GODOT_EXPORT OmniLight3D : public Light3D {

    GDCLASS(OmniLight3D,Light3D)

public:
    // omni light
    enum ShadowMode : int8_t {
        SHADOW_DUAL_PARABOLOID,
        SHADOW_CUBE,
    };

    // omni light
    enum ShadowDetail : int8_t {
        SHADOW_DETAIL_VERTICAL,
        SHADOW_DETAIL_HORIZONTAL
    };

private:
    ShadowMode shadow_mode;
    ShadowDetail shadow_detail;

protected:
    static void _bind_methods();

public:
    void set_shadow_mode(ShadowMode p_mode);
    ShadowMode get_shadow_mode() const;

    void set_shadow_detail(ShadowDetail p_detail);
    ShadowDetail get_shadow_detail() const;

    OmniLight3D();
};


class GODOT_EXPORT SpotLight3D : public Light3D {

    GDCLASS(SpotLight3D,Light3D)

protected:
    static void _bind_methods();

public:
    String get_configuration_warning() const override;

    SpotLight3D() :
            Light3D(RS::LIGHT_SPOT) {}
};
