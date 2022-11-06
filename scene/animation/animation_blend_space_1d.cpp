/*************************************************************************/
/*  animation_blend_space_1d.cpp                                         */
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

#include "animation_blend_space_1d.h"

#include "core/callable_method_pointer.h"
#include "core/fixed_string.h"
#include "core/method_bind.h"

IMPL_GDCLASS(AnimationNodeBlendSpace1D)

void AnimationNodeBlendSpace1D::get_parameter_list(Vector<PropertyInfo> *r_list) const {
    r_list->emplace_back(VariantType::FLOAT, StringName(blend_position));
}
Variant AnimationNodeBlendSpace1D::get_parameter_default_value(const StringName &p_parameter) const {
    return 0;
}

Ref<AnimationNode> AnimationNodeBlendSpace1D::get_child_by_name(const StringName &p_name) {
    return get_blend_point_node(StringUtils::to_int(p_name));
}

void AnimationNodeBlendSpace1D::_validate_property(PropertyInfo &property) const {
    if (StringUtils::begins_with(property.name,"blend_point/")) {
        FixedVector<StringView, 3> parts;
        String::split_ref(parts, property.name, '/');
        int idx = StringUtils::to_int(parts[1]);
        if (idx >= blend_points_used) {
            property.usage = 0;
        }
    }
    AnimationRootNode::_validate_property(property);
}

void AnimationNodeBlendSpace1D::_tree_changed() {
    emit_signal("tree_changed");
}

void AnimationNodeBlendSpace1D::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("add_blend_point", {"node", "pos", "at_index"}), &AnimationNodeBlendSpace1D::add_blend_point, {DEFVAL(-1)});
    SE_BIND_METHOD(AnimationNodeBlendSpace1D,set_blend_point_position);
    SE_BIND_METHOD(AnimationNodeBlendSpace1D,get_blend_point_position);
    SE_BIND_METHOD(AnimationNodeBlendSpace1D,set_blend_point_node);
    SE_BIND_METHOD(AnimationNodeBlendSpace1D,get_blend_point_node);
    SE_BIND_METHOD(AnimationNodeBlendSpace1D,remove_blend_point);
    SE_BIND_METHOD(AnimationNodeBlendSpace1D,get_blend_point_count);

    SE_BIND_METHOD(AnimationNodeBlendSpace1D,set_min_space);
    SE_BIND_METHOD(AnimationNodeBlendSpace1D,get_min_space);

    SE_BIND_METHOD(AnimationNodeBlendSpace1D,set_max_space);
    SE_BIND_METHOD(AnimationNodeBlendSpace1D,get_max_space);

    SE_BIND_METHOD(AnimationNodeBlendSpace1D,set_snap);
    SE_BIND_METHOD(AnimationNodeBlendSpace1D,get_snap);

    SE_BIND_METHOD(AnimationNodeBlendSpace1D,set_value_label);
    SE_BIND_METHOD(AnimationNodeBlendSpace1D,get_value_label);

    SE_BIND_METHOD(AnimationNodeBlendSpace1D,_add_blend_point);

    ADD_PROPERTY_ARRAY("Blend Points",MAX_BLEND_POINTS,"blend_point");
    for (int i = 0; i < MAX_BLEND_POINTS; i++) {
        ADD_PROPERTYI(
                PropertyInfo(VariantType::OBJECT, StringName("blend_point/" + itos(i) + "/node"),
                        PropertyHint::ResourceType, "AnimationRootNode", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL),
                "_add_blend_point", "get_blend_point_node", i);
        ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, StringName("blend_point/" + itos(i) + "/pos"), PropertyHint::None,
                              "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL),
                "set_blend_point_position", "get_blend_point_position", i);
    }

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "min_space", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_min_space", "get_min_space");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "max_space", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_max_space", "get_max_space");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "snap", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_snap", "get_snap");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "value_label", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_value_label", "get_value_label");
}

void AnimationNodeBlendSpace1D::get_child_nodes(Vector<AnimationNode::ChildNode> *r_child_nodes) {
    for (int i = 0; i < blend_points_used; i++) {
        ChildNode cn;
        cn.name = StringName(itos(i));
        cn.node = blend_points[i].node;
        r_child_nodes->push_back(cn);
    }
}

void AnimationNodeBlendSpace1D::add_blend_point(const Ref<AnimationRootNode> &p_node, float p_position, int p_at_index) {
    ERR_FAIL_COND(blend_points_used >= MAX_BLEND_POINTS);
    ERR_FAIL_COND(not p_node);

    ERR_FAIL_COND(p_at_index < -1 || p_at_index > blend_points_used);

    if (p_at_index == -1 || p_at_index == blend_points_used) {
        p_at_index = blend_points_used;
    } else {
        for (int i = blend_points_used - 1; i > p_at_index; i--) {
            blend_points[i] = blend_points[i - 1];
        }
    }

    blend_points[p_at_index].node = p_node;
    blend_points[p_at_index].position = p_position;

    blend_points[p_at_index].node->connect("tree_changed",callable_mp(this, &ClassName::_tree_changed),  ObjectNS::CONNECT_REFERENCE_COUNTED);

    blend_points_used++;
    emit_signal("tree_changed");
}

void AnimationNodeBlendSpace1D::set_blend_point_position(int p_point, float p_position) {
    ERR_FAIL_INDEX(p_point, blend_points_used);

    blend_points[p_point].position = p_position;
}

void AnimationNodeBlendSpace1D::set_blend_point_node(int p_point, const Ref<AnimationRootNode> &p_node) {
    ERR_FAIL_INDEX(p_point, blend_points_used);
    ERR_FAIL_COND(not p_node);

    if (blend_points[p_point].node) {
        blend_points[p_point].node->disconnect("tree_changed",callable_mp(this, &ClassName::_tree_changed));
    }

    blend_points[p_point].node = p_node;
    blend_points[p_point].node->connect("tree_changed",callable_mp(this, &ClassName::_tree_changed), ObjectNS::CONNECT_REFERENCE_COUNTED);

    emit_signal("tree_changed");
}

float AnimationNodeBlendSpace1D::get_blend_point_position(int p_point) const {
    ERR_FAIL_INDEX_V(p_point, blend_points_used, 0);
    return blend_points[p_point].position;
}

Ref<AnimationRootNode> AnimationNodeBlendSpace1D::get_blend_point_node(int p_point) const {
    ERR_FAIL_INDEX_V(p_point, blend_points_used, Ref<AnimationRootNode>());
    return blend_points[p_point].node;
}

void AnimationNodeBlendSpace1D::remove_blend_point(int p_point) {
    ERR_FAIL_INDEX(p_point, blend_points_used);

    ERR_FAIL_COND(not blend_points[p_point].node);

    blend_points[p_point].node->disconnect("tree_changed",callable_mp(this, &ClassName::_tree_changed));

    for (int i = p_point; i < blend_points_used - 1; i++) {
        blend_points[i] = blend_points[i + 1];
    }

    blend_points_used--;
    emit_signal("tree_changed");
}

int AnimationNodeBlendSpace1D::get_blend_point_count() const {

    return blend_points_used;
}

void AnimationNodeBlendSpace1D::set_min_space(float p_min) {
    min_space = p_min;

    if (min_space >= max_space) {
        min_space = max_space - 1;
    }
}

float AnimationNodeBlendSpace1D::get_min_space() const {
    return min_space;
}

void AnimationNodeBlendSpace1D::set_max_space(float p_max) {
    max_space = p_max;

    if (max_space <= min_space) {
        max_space = min_space + 1;
    }
}

float AnimationNodeBlendSpace1D::get_max_space() const {
    return max_space;
}

void AnimationNodeBlendSpace1D::set_snap(float p_snap) {
    snap = p_snap;
}

float AnimationNodeBlendSpace1D::get_snap() const {
    return snap;
}

void AnimationNodeBlendSpace1D::set_value_label(StringView p_label) {
    value_label = p_label;
}

const String & AnimationNodeBlendSpace1D::get_value_label() const {
    return value_label;
}

void AnimationNodeBlendSpace1D::_add_blend_point(int p_index, const Ref<AnimationRootNode> &p_node) {
    if (p_index == blend_points_used) {
        add_blend_point(p_node, 0);
    } else {
        set_blend_point_node(p_index, p_node);
    }
}

float AnimationNodeBlendSpace1D::process(float p_time, bool p_seek) {

    if (blend_points_used == 0) {
        return 0.0;
    }

    if (blend_points_used == 1) {
        // only one point available, just play that animation
        return blend_node(blend_points[0].name, blend_points[0].node, p_time, p_seek, 1.0, FILTER_IGNORE, false);
    }

    float blend_pos = get_parameter(blend_position).as<float>();

    float weights[MAX_BLEND_POINTS] = {};

    int point_lower = -1;
    float pos_lower = 0.0;
    int point_higher = -1;
    float pos_higher = 0.0;

    // find the closest two points to blend between
    for (int i = 0; i < blend_points_used; i++) {

        float pos = blend_points[i].position;

        if (pos <= blend_pos) {
            if (point_lower == -1) {
                point_lower = i;
                pos_lower = pos;
            } else if ((blend_pos - pos) < (blend_pos - pos_lower)) {
                point_lower = i;
                pos_lower = pos;
            }
        } else {
            if (point_higher == -1) {
                point_higher = i;
                pos_higher = pos;
            } else if ((pos - blend_pos) < (pos_higher - blend_pos)) {
                point_higher = i;
                pos_higher = pos;
            }
        }
    }

    // fill in weights

    if (point_lower == -1 && point_higher != -1) {
        // we are on the left side, no other point to the left
        // we just play the next point.

        weights[point_higher] = 1.0;
    } else if (point_higher == -1) {
        // we are on the right side, no other point to the right
        // we just play the previous point

        weights[point_lower] = 1.0f;
    } else {

        // we are between two points.
        // figure out weights, then blend the animations

        float distance_between_points = pos_higher - pos_lower;

        float current_pos_inbetween = blend_pos - pos_lower;

        float blend_percentage = current_pos_inbetween / distance_between_points;

        float blend_lower = 1.0 - blend_percentage;
        float blend_higher = blend_percentage;

        weights[point_lower] = blend_lower;
        weights[point_higher] = blend_higher;
    }

    // actually blend the animations now

    float max_time_remaining = 0.0;

    for (int i = 0; i < blend_points_used; i++) {
        float remaining = blend_node(blend_points[i].name, blend_points[i].node, p_time, p_seek, weights[i], FILTER_IGNORE, false);

        max_time_remaining = M_MAX(max_time_remaining, remaining);
    }

    return max_time_remaining;
}

StringView AnimationNodeBlendSpace1D::get_caption() const {
    return ("BlendSpace1D");
}

AnimationNodeBlendSpace1D::AnimationNodeBlendSpace1D() {
    TmpString<16,false> ts;
    for (int i = 0; i < MAX_BLEND_POINTS; i++) {
        ts.clear();
        ts.append_sprintf("%d",i);
        blend_points[i].name = StringName(ts.c_str());
    }
    blend_points_used = 0;
    max_space = 1;
    min_space = -1;

    snap = 0.1f;
    value_label = "value";

    blend_position = "blend_position";
}

AnimationNodeBlendSpace1D::~AnimationNodeBlendSpace1D() {
}
