/*************************************************************************/
/*  concave_polygon_shape.cpp                                            */
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

#include "concave_polygon_shape.h"

#include "core/pool_vector.h"
#include "servers/physics_server.h"
#include "core/method_bind.h"

IMPL_GDCLASS(ConcavePolygonShape)

namespace {
struct DrawEdge {

    Vector3 a;
    Vector3 b;
    bool operator<(const DrawEdge &p_edge) const {
        if (a == p_edge.a)
            return b < p_edge.b;
        else
            return a < p_edge.a;
    }

    DrawEdge(const Vector3 &p_a = Vector3(), const Vector3 &p_b = Vector3()) {
        a = p_a;
        b = p_b;
        if (a < b) {
            SWAP(a, b);
        }
    }
};
} // end of anonymous namespace

PODVector<Vector3> ConcavePolygonShape::get_debug_mesh_lines() {

    Set<DrawEdge> edges;

    PoolVector<Vector3> data = get_faces();
    int datalen = data.size();
    ERR_FAIL_COND_V((datalen % 3) != 0, PODVector<Vector3>())

    PoolVector<Vector3>::Read r = data.read();

    for (int i = 0; i < datalen; i += 3) {

        for (int j = 0; j < 3; j++) {

            DrawEdge de(r[i + j], r[i + ((j + 1) % 3)]);
            edges.insert(de);
        }
    }

    PODVector<Vector3> points;
    points.resize(edges.size() * 2);
    int idx = 0;
    for (const DrawEdge &E : edges) {

        points[idx + 0] = E.a;
        points[idx + 1] = E.b;
        idx += 2;
    }

    return points;
}

void ConcavePolygonShape::_update_shape() {
    Shape::_update_shape();
}

void ConcavePolygonShape::set_faces(const PoolVector<Vector3> &p_faces) {

    PhysicsServer::get_singleton()->shape_set_data(get_shape(), p_faces);
    notify_change_to_owners();
}

PoolVector<Vector3> ConcavePolygonShape::get_faces() const {

    return PhysicsServer::get_singleton()->shape_get_data(get_shape());
}

void ConcavePolygonShape::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_faces", {"faces"}), &ConcavePolygonShape::set_faces);
    MethodBinder::bind_method(D_METHOD("get_faces"), &ConcavePolygonShape::get_faces);
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR3_ARRAY, "data", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "set_faces", "get_faces");
}

ConcavePolygonShape::ConcavePolygonShape() :
        Shape(PhysicsServer::get_singleton()->shape_create(PhysicsServer::SHAPE_CONCAVE_POLYGON)) {

    //set_planes(Vector3(1,1,1));
}
