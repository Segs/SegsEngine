/*************************************************************************/
/*  sphere_shape.cpp                                                     */
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

#include "sphere_shape_3d.h"

#include "core/math/vector2.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "servers/physics_server_3d.h"


IMPL_GDCLASS(SphereShape3D)

Vector<Vector3> SphereShape3D::get_debug_mesh_lines() {

    float r = get_radius();

    Vector<Vector3> points;

    for (int i = 0; i <= 360; i++) {

        float ra = Math::deg2rad((float)i);
        float rb = Math::deg2rad((float)i + 1);
        Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
        Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

        points.push_back(Vector3(a.x, 0, a.y));
        points.push_back(Vector3(b.x, 0, b.y));
        points.push_back(Vector3(0, a.x, a.y));
        points.push_back(Vector3(0, b.x, b.y));
        points.push_back(Vector3(a.x, a.y, 0));
        points.push_back(Vector3(b.x, b.y, 0));
    }

    return points;
}

void SphereShape3D::_update_shape() {

    PhysicsServer3D::get_singleton()->shape_set_data(get_shape(), radius);
    Shape::_update_shape();
}

void SphereShape3D::set_radius(float p_radius) {

    radius = p_radius;
    _update_shape();
    notify_change_to_owners();
    Object_change_notify(this,"radius");
}

float SphereShape3D::get_radius() const {

    return radius;
}

void SphereShape3D::_bind_methods() {

    BIND_METHOD(SphereShape3D,set_radius);
    BIND_METHOD(SphereShape3D,get_radius);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "radius", PropertyHint::Range, "0.001,100,0.001,or_greater"), "set_radius", "get_radius");
}

SphereShape3D::SphereShape3D() :
        Shape(PhysicsServer3D::get_singleton()->shape_create(PhysicsServer3D::SHAPE_SPHERE)) {

    set_radius(1.0);
}
