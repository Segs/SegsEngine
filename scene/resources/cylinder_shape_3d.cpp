/*************************************************************************/
/*  cylinder_shape.cpp                                                   */
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

#include "cylinder_shape_3d.h"

#include "core/dictionary.h"
#include "core/math/vector2.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "servers/physics_server_3d.h"

IMPL_GDCLASS(CylinderShape3D)

Vector<Vector3> CylinderShape3D::get_debug_mesh_lines() {

    float radius = get_radius();
    float height = get_height();
    Vector3 work_area[360*4 + 3*2];
    size_t widx=0;

    Vector3 d(0, height * 0.5f, 0);
    for (int i = 0; i < 360; i++) {

        float ra = Math::deg2rad((float)i);
        float rb = Math::deg2rad((float)i + 1);
        Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * radius;
        Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * radius;

        work_area[widx++] = Vector3(a.x, 0, a.y) + d;
        work_area[widx++] = Vector3(b.x, 0, b.y) + d;

        work_area[widx++] = Vector3(a.x, 0, a.y) - d;
        work_area[widx++] = Vector3(b.x, 0, b.y) - d;

        if (i % 90 == 0) {

            work_area[widx++] = Vector3(a.x, 0, a.y) + d;
            work_area[widx++] = Vector3(a.x, 0, a.y) - d;
        }
    }
    return {work_area,work_area+widx};
}

void CylinderShape3D::_update_shape() {

    Dictionary d;
    d["radius"] = radius;
    d["height"] = height;
    PhysicsServer3D::get_singleton()->shape_set_data(get_shape(), d);
    Shape::_update_shape();
}

void CylinderShape3D::set_radius(float p_radius) {

    radius = p_radius;
    _update_shape();
    notify_change_to_owners();
    Object_change_notify(this,"radius");
}



void CylinderShape3D::set_height(float p_height) {

    height = p_height;
    _update_shape();
    notify_change_to_owners();
    Object_change_notify(this,"height");
}



void CylinderShape3D::_bind_methods() {

    BIND_METHOD(CylinderShape3D,set_radius);
    BIND_METHOD(CylinderShape3D,get_radius);
    BIND_METHOD(CylinderShape3D,set_height);
    BIND_METHOD(CylinderShape3D,get_height);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "height", PropertyHint::Range, "0.001,100,0.001,or_greater"), "set_height", "get_height");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "radius", PropertyHint::Range, "0.001,100,0.001,or_greater"), "set_radius", "get_radius");
}

CylinderShape3D::CylinderShape3D() :
        Shape(PhysicsServer3D::get_singleton()->shape_create(PhysicsServer3D::SHAPE_CYLINDER)) {

    radius = 1.0;
    height = 2.0;
    _update_shape();
}
