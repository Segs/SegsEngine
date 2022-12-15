/*************************************************************************/
/*  plane_shape.cpp                                                      */
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

#include "plane_shape.h"

#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "servers/physics_server_3d.h"

IMPL_GDCLASS(PlaneShape)

Vector<Vector3> PlaneShape::get_debug_mesh_lines() {

    Plane p = get_plane();

    Vector3 n1 = p.get_any_perpendicular_normal();
    Vector3 n2 = p.normal.cross(n1).normalized();

    Vector3 pface[4] = {
        p.normal * p.d + n1 * 10.0 + n2 * 10.0,
        p.normal * p.d + n1 * 10.0 + n2 * -10.0,
        p.normal * p.d + n1 * -10.0 + n2 * -10.0,
        p.normal * p.d + n1 * -10.0 + n2 * 10.0,
    };

    Vector3 points[10];
    uint8_t widx=0;
    points[widx++] = pface[0];
    points[widx++] = pface[1];
    points[widx++] = pface[1];
    points[widx++] = pface[2];
    points[widx++] = pface[2];
    points[widx++] = pface[3];
    points[widx++] = pface[3];
    points[widx++] = pface[0];
    points[widx++] = p.normal * p.d;
    points[widx++] = p.normal * p.d + p.normal * 3;

    return {points,points+widx};
}

void PlaneShape::_update_shape() {

    PhysicsServer3D::get_singleton()->shape_set_data(get_shape(), plane);
    Shape::_update_shape();
}

void PlaneShape::set_plane(Plane p_plane) {

    plane = p_plane;
    PlaneShape::_update_shape();
    notify_change_to_owners();
    Object_change_notify(this,"plane");
}

Plane PlaneShape::get_plane() const {

    return plane;
}

void PlaneShape::_bind_methods() {

    SE_BIND_METHOD(PlaneShape,set_plane);
    SE_BIND_METHOD(PlaneShape,get_plane);

    ADD_PROPERTY(PropertyInfo(VariantType::PLANE, "plane"), "set_plane", "get_plane");
}

PlaneShape::PlaneShape() :
        Shape(PhysicsServer3D::get_singleton()->shape_create(PhysicsServer3D::SHAPE_PLANE)) {

    set_plane(Plane(0, 1, 0, 0));
}
