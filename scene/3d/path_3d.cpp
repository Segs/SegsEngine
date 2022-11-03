/*************************************************************************/
/*  path_3d.cpp                                                          */
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

#include "path_3d.h"

#include "core/callable_method_pointer.h"
#include "core/engine.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/translation_helpers.h"
#include "scene/scene_string_names.h"

IMPL_GDCLASS(Path3D)
IMPL_GDCLASS(PathFollow3D)
VARIANT_ENUM_CAST(PathFollow3D::RotationMode);

void Path3D::_notification(int p_what) {
}

void Path3D::_curve_changed() {

    if (is_inside_tree() && Engine::get_singleton()->is_editor_hint())
        update_gizmo();
    if (is_inside_tree()) {
        emit_signal("curve_changed");
    }

    // update the configuration warnings of all children of type PathFollow3D
    // previously used for PathFollowOriented (now enforced orientation is done in PathFollow3D)
    if (is_inside_tree()) {
        for (int i = 0; i < get_child_count(); i++) {
            PathFollow3D *child = object_cast<PathFollow3D>(get_child(i));
            if (child) {
                child->update_configuration_warning();
            }
        }
    }
}

void Path3D::set_curve(const Ref<Curve3D> &p_curve) {

    if (curve) {
        curve->disconnect("changed",callable_mp(this, &ClassName::_curve_changed));
    }

    curve = p_curve;

    if (curve) {
        curve->connect("changed",callable_mp(this, &ClassName::_curve_changed));
    }
    _curve_changed();
}

Ref<Curve3D> Path3D::get_curve() const {

    return curve;
}

void Path3D::_bind_methods() {

    SE_BIND_METHOD(Path3D,set_curve);
    SE_BIND_METHOD(Path3D,get_curve);
    SE_BIND_METHOD(Path3D,_curve_changed);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "curve", PropertyHint::ResourceType, "Curve3D"), "set_curve", "get_curve");

    ADD_SIGNAL(MethodInfo("curve_changed"));
}

Path3D::Path3D() {

    set_curve(make_ref_counted<Curve3D>()); //create one by default
}

//////////////

void PathFollow3D::_update_transform(bool p_update_xyz_rot) {

    if (!path)
        return;

    Ref<Curve3D> c = path->get_curve();
    if (not c)
        return;

    float bl = c->get_baked_length();
    if (bl == 0.0) {
        return;
    }
    float bi = c->get_bake_interval();
    float o_next = offset + bi;
    float o_prev = offset - bi;

    if (loop) {
        o_next = Math::fposmod(o_next, bl);
        o_prev = Math::fposmod(o_prev, bl);
    } else if (rotation_mode == ROTATION_ORIENTED) {
        if (o_next >= bl) {
            o_next = bl;
        }
        if (o_prev <= 0) {
            o_prev = 0;
        }
    }

    Vector3 pos = c->interpolate_baked(offset, cubic);
    Transform t = get_transform();
    // Vector3 pos_offset = Vector3(h_offset, v_offset, 0); not used in all cases
    // will be replaced by "Vector3(h_offset, v_offset, 0)" where it was formerly used

    if (rotation_mode == ROTATION_ORIENTED) {

        Vector3 forward = c->interpolate_baked(o_next, cubic) - pos;

        // Try with the previous position
        if (forward.length_squared() < CMP_EPSILON2) {
            forward = pos - c->interpolate_baked(o_prev, cubic);
        }

        if (forward.length_squared() < CMP_EPSILON2)
            forward = Vector3(0, 0, 1);
        else
            forward.normalize();

        Vector3 up = c->interpolate_baked_up_vector(offset, true);

        if (o_next < offset) {
            Vector3 up1 = c->interpolate_baked_up_vector(o_next, true);
            Vector3 axis = up.cross(up1);

            if (axis.length_squared() < CMP_EPSILON2)
                axis = forward;
            else
                axis.normalize();

            up.rotate(axis, up.angle_to(up1) * 0.5f);
        }

        Vector3 scale = t.basis.get_scale();
        Vector3 sideways = up.cross(forward).normalized();
        up = forward.cross(sideways).normalized();

        t.basis.set(sideways, up, forward);
        t.basis.scale_local(scale);

        t.origin = pos + sideways * h_offset + up * v_offset;
    } else if (rotation_mode != ROTATION_NONE) {
        // perform parallel transport
        //
        // see C. Dougan, The Parallel Transport Frame, Game Programming Gems 2 for example
        // for a discussion about why not Frenet frame.

        t.origin = pos;
        if (p_update_xyz_rot && delta_offset != 0) { // Only update rotation if some parameter has changed - i.e. not on addition to scene tree.

            Vector3 t_prev = (pos - c->interpolate_baked(offset - delta_offset, cubic)).normalized();
            Vector3 t_cur = (c->interpolate_baked(offset + delta_offset, cubic) - pos).normalized();

            Vector3 axis = t_prev.cross(t_cur);
            float dot = t_prev.dot(t_cur);
            float angle = Math::acos(CLAMP(dot, -1.0f, 1.0f));

            if (likely(!Math::is_zero_approx(angle))) {
                if (rotation_mode == ROTATION_Y) {
                    // assuming we're referring to global Y-axis. is this correct?
                    axis.x = 0;
                    axis.z = 0;
                } else if (rotation_mode == ROTATION_XY) {
                    axis.z = 0;
                } else if (rotation_mode == ROTATION_XYZ) {
                    // all components are allowed
                }

                if (likely(!Math::is_zero_approx(axis.length()))) {
                    t.rotate_basis(axis.normalized(), angle);
                }
            }

            // do the additional tilting
            float tilt_angle = c->interpolate_baked_tilt(offset);
            Vector3 tilt_axis = t_cur; // not sure what tilt is supposed to do, is this correct??

            if (likely(!Math::is_zero_approx(Math::abs(tilt_angle)))) {
                if (rotation_mode == ROTATION_Y) {
                    tilt_axis.x = 0;
                    tilt_axis.z = 0;
                } else if (rotation_mode == ROTATION_XY) {
                    tilt_axis.z = 0;
                } else if (rotation_mode == ROTATION_XYZ) {
                    // all components are allowed
                }

                if (likely(!Math::is_zero_approx(tilt_axis.length()))) {
                    t.rotate_basis(tilt_axis.normalized(), tilt_angle);
                }
            }
        }
        t.translate(Vector3(h_offset, v_offset, 0));
    } else {
        t.origin = pos + Vector3(h_offset, v_offset, 0);
    }

    set_transform(t);
}

void PathFollow3D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {

            Node *parent = get_parent();
            if (parent) {
                path = object_cast<Path3D>(parent);
                if (path) {
                    _update_transform(false);
                }
            }

        } break;
        case NOTIFICATION_EXIT_TREE: {

            path = nullptr;
        } break;
    }
}

void PathFollow3D::set_cubic_interpolation(bool p_enable) {

    cubic = p_enable;
}

bool PathFollow3D::get_cubic_interpolation() const {

    return cubic;
}

void PathFollow3D::_validate_property(PropertyInfo &property) const {

    if (property.name == "offset") {

        float max = 10000;
        if (path && path->get_curve())
            max = path->get_curve()->get_baked_length();

        property.hint_string = "0," + rtos(max) + ",0.01,or_lesser,or_greater";
    }
}

String PathFollow3D::get_configuration_warning() const {

    if (!is_visible_in_tree() || !is_inside_tree()) {
        return String();
    }

    String warning = BaseClassName::get_configuration_warning();
    Path3D *path = object_cast<Path3D>(get_parent());
    if (!path) {
        if (warning != String()) {
            warning += "\n\n";
        }
        warning += TTR("PathFollow only works when set as a child of a Path node.");
    } else {
        if (path->get_curve() && !path->get_curve()->is_up_vector_enabled() && rotation_mode == ROTATION_ORIENTED) {
            if (warning != String()) {
                warning += "\n\n";
            }
            warning += TTR("PathFollow's ROTATION_ORIENTED requires \"Up Vector\" to be enabled in its parent Path's "
                           "Curve resource.");
        }
    }

    return warning;
}

void PathFollow3D::_bind_methods() {

    SE_BIND_METHOD(PathFollow3D,set_offset);
    SE_BIND_METHOD(PathFollow3D,get_offset);

    SE_BIND_METHOD(PathFollow3D,set_h_offset);
    SE_BIND_METHOD(PathFollow3D,get_h_offset);

    SE_BIND_METHOD(PathFollow3D,set_v_offset);
    SE_BIND_METHOD(PathFollow3D,get_v_offset);

    SE_BIND_METHOD(PathFollow3D,set_unit_offset);
    SE_BIND_METHOD(PathFollow3D,get_unit_offset);

    SE_BIND_METHOD(PathFollow3D,set_rotation_mode);
    SE_BIND_METHOD(PathFollow3D,get_rotation_mode);

    SE_BIND_METHOD(PathFollow3D,set_cubic_interpolation);
    SE_BIND_METHOD(PathFollow3D,get_cubic_interpolation);

    SE_BIND_METHOD(PathFollow3D,set_loop);
    SE_BIND_METHOD(PathFollow3D,has_loop);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "offset", PropertyHint::Range, "0,10000,0.01,or_lesser,or_greater"), "set_offset", "get_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "unit_offset", PropertyHint::Range, "0,1,0.0001,or_lesser,or_greater", PROPERTY_USAGE_EDITOR), "set_unit_offset", "get_unit_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "h_offset"), "set_h_offset", "get_h_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "v_offset"), "set_v_offset", "get_v_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "rotation_mode", PropertyHint::Enum, "None,Y,XY,XYZ,Oriented"), "set_rotation_mode", "get_rotation_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "cubic_interp"), "set_cubic_interpolation", "get_cubic_interpolation");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "loop"), "set_loop", "has_loop");

    BIND_ENUM_CONSTANT(ROTATION_NONE);
    BIND_ENUM_CONSTANT(ROTATION_Y);
    BIND_ENUM_CONSTANT(ROTATION_XY);
    BIND_ENUM_CONSTANT(ROTATION_XYZ);
    BIND_ENUM_CONSTANT(ROTATION_ORIENTED);
}

void PathFollow3D::set_offset(float p_offset) {
    delta_offset = p_offset - offset;
    offset = p_offset;

    if (path) {
        if (path->get_curve()) {
            float path_length = path->get_curve()->get_baked_length();

            if (loop) {
                offset = Math::fposmod(offset, path_length);
                if (!Math::is_zero_approx(p_offset) && Math::is_zero_approx(offset)) {
                    offset = path_length;
                }
            } else {
                offset = CLAMP<float>(offset, 0, path_length);
            }
        }

        _update_transform();
    }
    Object_change_notify(this,"offset");
    Object_change_notify(this,"unit_offset");
}


void PathFollow3D::set_h_offset(float p_h_offset) {

    h_offset = p_h_offset;
    if (path)
        _update_transform();
}

float PathFollow3D::get_h_offset() const {

    return h_offset;
}

void PathFollow3D::set_v_offset(float p_v_offset) {

    v_offset = p_v_offset;
    if (path)
        _update_transform();
}

float PathFollow3D::get_v_offset() const {

    return v_offset;
}

float PathFollow3D::get_offset() const {

    return offset;
}

void PathFollow3D::set_unit_offset(float p_unit_offset) {

    if (path && path->get_curve() && path->get_curve()->get_baked_length())
        set_offset(p_unit_offset * path->get_curve()->get_baked_length());
}

float PathFollow3D::get_unit_offset() const {

    if (path && path->get_curve() && path->get_curve()->get_baked_length())
        return get_offset() / path->get_curve()->get_baked_length();
    else
        return 0;
}

void PathFollow3D::set_rotation_mode(RotationMode p_rotation_mode) {

    rotation_mode = p_rotation_mode;

    update_configuration_warning();
    _update_transform();
}

PathFollow3D::RotationMode PathFollow3D::get_rotation_mode() const {

    return rotation_mode;
}

void PathFollow3D::set_loop(bool p_loop) {

    loop = p_loop;
}

bool PathFollow3D::has_loop() const {

    return loop;
}

PathFollow3D::PathFollow3D() {

    offset = 0;
    delta_offset = 0;
    h_offset = 0;
    v_offset = 0;
    path = nullptr;
    rotation_mode = ROTATION_XYZ;
    cubic = true;
    loop = true;
}
