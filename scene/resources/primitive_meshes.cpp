/*************************************************************************/
/*  primitive_meshes.cpp                                                 */
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

#include "primitive_meshes.h"
#include "servers/rendering_server.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "scene/resources/material.h"

IMPL_GDCLASS(PrimitiveMesh)
IMPL_GDCLASS(CapsuleMesh)
IMPL_GDCLASS(CubeMesh)
IMPL_GDCLASS(CylinderMesh)
IMPL_GDCLASS(PlaneMesh)
IMPL_GDCLASS(PrismMesh)
IMPL_GDCLASS(QuadMesh)
IMPL_GDCLASS(SphereMesh)
IMPL_GDCLASS(PointMesh)

/**
  PrimitiveMesh
*/
void PrimitiveMesh::_update() const {

    SurfaceArrays arr;
    _create_mesh_array(arr);

    Span<const Vector3> points = arr.positions3();

    aabb = AABB();

    int pc = points.size();
    ERR_FAIL_COND(pc == 0);
    {

        for (int i = 0; i < pc; i++) {
            if (i == 0)
                aabb.position = points[i];
            else
                aabb.expand_to(points[i]);
        }
    }

    if (flip_faces) {
        Vector<Vector3> &normals = arr.m_normals;
        Vector<int> &indices = arr.m_indices;
        if (normals.size() && indices.size()) {

            {
                int nc = normals.size();
                for (int i = 0; i < nc; i++) {
                    normals[i] = -normals[i];
                }
            }

            {
                int ic = indices.size();
                for (int i = 0; i < ic; i += 3) {
                    SWAP(indices[i + 0], indices[i + 1]);
                }
            }
        }
    }

    // in with the new
    RenderingServer::get_singleton()->mesh_clear(mesh);
    RenderingServer::get_singleton()->mesh_add_surface_from_arrays(mesh, (RS::PrimitiveType)primitive_type, eastl::move(arr));
    RenderingServer::get_singleton()->mesh_surface_set_material(mesh, 0, not material ? entt::null : material->get_rid());

    pending_request = false;

    clear_cache();

    const_cast<PrimitiveMesh *>(this)->emit_changed();
}

void PrimitiveMesh::_request_update() {

    if (pending_request)
        return;
    _update();
}

int PrimitiveMesh::get_surface_count() const {
    if (pending_request) {
        _update();
    }
    return 1;
}

int PrimitiveMesh::surface_get_array_len(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, 1, -1);
    if (pending_request) {
        _update();
    }

    return RenderingServer::get_singleton()->mesh_surface_get_array_len(mesh, 0);
}

int PrimitiveMesh::surface_get_array_index_len(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, 1, -1);
    if (pending_request) {
        _update();
    }

    return RenderingServer::get_singleton()->mesh_surface_get_array_index_len(mesh, 0);
}

SurfaceArrays PrimitiveMesh::surface_get_arrays(int p_surface) const {
    ERR_FAIL_INDEX_V(p_surface, 1, SurfaceArrays());
    if (pending_request) {
        _update();
    }

    return RenderingServer::get_singleton()->mesh_surface_get_arrays(mesh, 0);
}

Vector<SurfaceArrays> PrimitiveMesh::surface_get_blend_shape_arrays(int p_surface) const {
    ERR_FAIL_INDEX_V(p_surface, 1, Vector<SurfaceArrays>());
    if (pending_request) {
        _update();
    }

    return Vector<SurfaceArrays>();
}

uint32_t PrimitiveMesh::surface_get_format(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, 1, 0);
    if (pending_request) {
        _update();
    }

    return RenderingServer::get_singleton()->mesh_surface_get_format(mesh, 0);
}

Mesh::PrimitiveType PrimitiveMesh::surface_get_primitive_type(int p_idx) const {
    return primitive_type;
}

void PrimitiveMesh::surface_set_material(int p_idx, const Ref<Material> &p_material) {
    ERR_FAIL_INDEX(p_idx, 1);

    set_material(p_material);
}

Ref<Material> PrimitiveMesh::surface_get_material(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, 1, Ref<Material>());

    return material;
}

AABB PrimitiveMesh::get_aabb() const {
    if (pending_request) {
        _update();
    }

    return aabb;
}

RenderingEntity PrimitiveMesh::get_rid() const {
    if (pending_request) {
        _update();
    }
    return mesh;
}

void PrimitiveMesh::_bind_methods() {
    SE_BIND_METHOD(PrimitiveMesh,set_material);
    SE_BIND_METHOD(PrimitiveMesh,get_material);

    MethodBinder::bind_method(D_METHOD("get_mesh_arrays"), &PrimitiveMesh::_get_mesh_arrays);

    SE_BIND_METHOD(PrimitiveMesh,set_custom_aabb);
    SE_BIND_METHOD(PrimitiveMesh,get_custom_aabb);

    SE_BIND_METHOD(PrimitiveMesh,set_flip_faces);
    SE_BIND_METHOD(PrimitiveMesh,get_flip_faces);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "material", PropertyHint::ResourceType, "SpatialMaterial,ShaderMaterial"), "set_material", "get_material");
    ADD_PROPERTY(PropertyInfo(VariantType::AABB, "custom_aabb", PropertyHint::None, ""), "set_custom_aabb", "get_custom_aabb");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "flip_faces"), "set_flip_faces", "get_flip_faces");
}

void PrimitiveMesh::set_material(const Ref<Material> &p_material) {
    material = p_material;
    if (!pending_request) {
        // just apply it, else it'll happen when _update is called.
        RenderingServer::get_singleton()->mesh_surface_set_material(mesh, 0, not material ? entt::null : material->get_rid());
        Object_change_notify(this);
        emit_changed();
    }
}

Ref<Material> PrimitiveMesh::get_material() const {
    return material;
}
Array PrimitiveMesh::_get_mesh_arrays() const {
    return (Array)get_mesh_arrays();
}

SurfaceArrays PrimitiveMesh::get_mesh_arrays() const {
    return surface_get_arrays(0);
}

void PrimitiveMesh::set_custom_aabb(const AABB &p_custom) {

    custom_aabb = p_custom;
    RenderingServer::get_singleton()->mesh_set_custom_aabb(mesh, custom_aabb);
    emit_changed();
}

AABB PrimitiveMesh::get_custom_aabb() const {

    return custom_aabb;
}

void PrimitiveMesh::set_flip_faces(bool p_enable) {
    flip_faces = p_enable;
    _request_update();
}

bool PrimitiveMesh::get_flip_faces() const {
    return flip_faces;
}

PrimitiveMesh::PrimitiveMesh() {

    flip_faces = false;
    // defaults
    mesh = RenderingServer::get_singleton()->mesh_create();

    // assume primitive triangles as the type, correct for all but one and it will change this :)
    primitive_type = Mesh::PRIMITIVE_TRIANGLES;

    // make sure we do an update after we've finished constructing our object
    pending_request = true;
}

PrimitiveMesh::~PrimitiveMesh() {
    RenderingServer::get_singleton()->free_rid(mesh);
}

/**
    CapsuleMesh
*/

void CapsuleMesh::_create_mesh_array(SurfaceArrays &p_arr) const {
    create_mesh_array(p_arr, radius, mid_height, radial_segments, rings);
}

void CapsuleMesh::create_mesh_array(
        SurfaceArrays &p_arr, const float radius, const float mid_height, const int radial_segments, const int rings) {
    int i, j, prevrow, thisrow, point;
    float x, y, z, u, v, w;
    float onethird = 1.0f / 3.0f;
    float twothirds = 2.0f / 3.0f;

    // note, this has been aligned with our collision shape but I've left the descriptions as top/middle/bottom

    Vector<Vector3> points;
    Vector<Vector3> normals;
    Vector<float> tangents;
    Vector<Vector2> uvs;
    Vector<int> indices;
    point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
    tangents.push_back(m_x);            \
    tangents.push_back(m_y);            \
    tangents.push_back(m_z);            \
    tangents.push_back(m_d);

    /* top hemisphere */
    thisrow = 0;
    prevrow = 0;
    for (j = 0; j <= (rings + 1); j++) {
        v = j;

        v /= (rings + 1);
        w = sin(0.5 * Math_PI * v);
        z = radius * cos(0.5 * Math_PI * v);

        for (i = 0; i <= radial_segments; i++) {
            u = i;
            u /= radial_segments;

            x = sin(u * (Math_PI * 2.0f));
            y = -cos(u * (Math_PI * 2.0f));

            Vector3 p = Vector3(x * radius * w, y * radius * w, z);
            points.push_back(p + Vector3(0.0, 0.0, 0.5f * mid_height));
            normals.push_back(p.normalized());
            ADD_TANGENT(-y, x, 0.0, 1.0)
            uvs.push_back(Vector2(u, v * onethird));
            point++;

            if (i > 0 && j > 0) {
                indices.push_back(prevrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i - 1);

                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i);
                indices.push_back(thisrow + i - 1);
            }
        }

        prevrow = thisrow;
        thisrow = point;
    }

    /* cylinder */
    thisrow = point;
    prevrow = 0;
    for (j = 0; j <= (rings + 1); j++) {
        v = j;
        v /= (rings + 1);

        z = mid_height * v;
        z = (mid_height * 0.5) - z;

        for (i = 0; i <= radial_segments; i++) {
            u = i;
            u /= radial_segments;

            x = sin(u * (Math_PI * 2.0));
            y = -cos(u * (Math_PI * 2.0));

            Vector3 p = Vector3(x * radius, y * radius, z);
            points.push_back(p);
            normals.push_back(Vector3(x, y, 0.0));
            ADD_TANGENT(-y, x, 0.0, 1.0)
            uvs.push_back(Vector2(u, onethird + (v * onethird)));
            point++;

            if (i > 0 && j > 0) {
                indices.push_back(prevrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i - 1);

                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i);
                indices.push_back(thisrow + i - 1);
            };
        };

        prevrow = thisrow;
        thisrow = point;
    };

    /* bottom hemisphere */
    thisrow = point;
    prevrow = 0;
    for (j = 0; j <= (rings + 1); j++) {
        v = j;

        v /= (rings + 1);
        v += 1.0;
        w = sin(0.5 * Math_PI * v);
        z = radius * cos(0.5 * Math_PI * v);

        for (i = 0; i <= radial_segments; i++) {
            float u2 = i;
            u2 /= radial_segments;

            x = sin(u2 * (Math_PI * 2.0));
            y = -cos(u2 * (Math_PI * 2.0));

            Vector3 p = Vector3(x * radius * w, y * radius * w, z);
            points.push_back(p + Vector3(0.0, 0.0, -0.5 * mid_height));
            normals.push_back(p.normalized());
            ADD_TANGENT(-y, x, 0.0, 1.0)
            uvs.push_back(Vector2(u2, twothirds + ((v - 1.0) * onethird)));
            point++;

            if (i > 0 && j > 0) {
                indices.push_back(prevrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i - 1);

                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i);
                indices.push_back(thisrow + i - 1);
            }
        }

        prevrow = thisrow;
        thisrow = point;
    }

    p_arr.set_positions(eastl::move(points));
    p_arr.m_normals = eastl::move(normals);
    p_arr.m_tangents = eastl::move(tangents);
    p_arr.m_uv_1 = eastl::move(uvs);
    p_arr.m_indices = eastl::move(indices);
#undef ADD_TANGENT
}

void CapsuleMesh::_bind_methods() {
    SE_BIND_METHOD(CapsuleMesh,set_radius);
    SE_BIND_METHOD(CapsuleMesh,get_radius);
    SE_BIND_METHOD(CapsuleMesh,set_mid_height);
    SE_BIND_METHOD(CapsuleMesh,get_mid_height);

    SE_BIND_METHOD(CapsuleMesh,set_radial_segments);
    SE_BIND_METHOD(CapsuleMesh,get_radial_segments);
    SE_BIND_METHOD(CapsuleMesh,set_rings);
    SE_BIND_METHOD(CapsuleMesh,get_rings);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "radius", PropertyHint::Range, "0.001,100.0,0.001,or_greater"), "set_radius", "get_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "mid_height", PropertyHint::Range, "0.001,100.0,0.001,or_greater"), "set_mid_height", "get_mid_height");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "radial_segments", PropertyHint::Range, "1,100,1,or_greater"), "set_radial_segments", "get_radial_segments");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "rings", PropertyHint::Range, "1,100,1,or_greater"), "set_rings", "get_rings");
}

void CapsuleMesh::set_radius(const float p_radius) {
    radius = p_radius;
    _request_update();
}

float CapsuleMesh::get_radius() const {
    return radius;
}

void CapsuleMesh::set_mid_height(const float p_mid_height) {
    mid_height = p_mid_height;
    _request_update();
}

float CapsuleMesh::get_mid_height() const {
    return mid_height;
}

void CapsuleMesh::set_radial_segments(const int p_segments) {
    radial_segments = p_segments > 4 ? p_segments : 4;
    _request_update();
}

int CapsuleMesh::get_radial_segments() const {
    return radial_segments;
}

void CapsuleMesh::set_rings(const int p_rings) {
    rings = p_rings > 1 ? p_rings : 1;
    _request_update();
}

int CapsuleMesh::get_rings() const {
    return rings;
}

CapsuleMesh::CapsuleMesh() {
    // defaults
    radius = 1.0;
    mid_height = 1.0;
    radial_segments = default_radial_segments;
    rings = default_rings;
}

/**
  CubeMesh
*/
void CubeMesh::_create_mesh_array(SurfaceArrays &p_arr) const {
    create_mesh_array(p_arr, size, subdivide_w, subdivide_h, subdivide_d);
}

void CubeMesh::create_mesh_array(
        SurfaceArrays &p_arr, const Vector3 size, const int subdivide_w, const int subdivide_h, const int subdivide_d) {
    int i, j, prevrow, thisrow, point;
    float x, y, z;
    const float onethird = 1.0f / 3.0f;
    const float twothirds = 2.0f / 3.0f;

    Vector3 start_pos = size * -0.5;

    // set our bounding box

    Vector<Vector3> points;
    Vector<Vector3> normals;
    Vector<float> tangents;
    Vector<Vector2> uvs;
    Vector<int> indices;
    point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
    tangents.push_back(m_x);            \
    tangents.push_back(m_y);            \
    tangents.push_back(m_z);            \
    tangents.push_back(m_d);

    // front + back
    y = start_pos.y;
    thisrow = point;
    prevrow = 0;
    for (j = 0; j <= subdivide_h + 1; j++) {
        x = start_pos.x;
        for (i = 0; i <= subdivide_w + 1; i++) {
            float u = i;
            float v = j;
            u /= (3.0 * (subdivide_w + 1.0));
            v /= (2.0 * (subdivide_h + 1.0));

            // front
            points.push_back(Vector3(x, -y, -start_pos.z)); // double negative on the Z!
            normals.push_back(Vector3(0.0, 0.0, 1.0));
            ADD_TANGENT(1.0, 0.0, 0.0, 1.0);
            uvs.push_back(Vector2(u, v));
            point++;

            // back
            points.push_back(Vector3(-x, -y, start_pos.z));
            normals.push_back(Vector3(0.0, 0.0, -1.0));
            ADD_TANGENT(-1.0, 0.0, 0.0, 1.0);
            uvs.push_back(Vector2(twothirds + u, v));
            point++;

            if (i > 0 && j > 0) {
                int i2 = i * 2;

                // front
                indices.push_back(prevrow + i2 - 2);
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2 - 2);
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2);
                indices.push_back(thisrow + i2 - 2);

                // back
                indices.push_back(prevrow + i2 - 1);
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
            };

            x += size.x / (subdivide_w + 1.0);
        };

        y += size.y / (subdivide_h + 1.0);
        prevrow = thisrow;
        thisrow = point;
    };

    // left + right
    y = start_pos.y;
    thisrow = point;
    prevrow = 0;
    for (j = 0; j <= (subdivide_h + 1); j++) {
        z = start_pos.z;
        for (i = 0; i <= (subdivide_d + 1); i++) {
            float u = i;
            float v = j;
            u /= (3.0 * (subdivide_d + 1.0));
            v /= (2.0 * (subdivide_h + 1.0));

            // right
            points.push_back(Vector3(-start_pos.x, -y, -z));
            normals.push_back(Vector3(1.0, 0.0, 0.0));
            ADD_TANGENT(0.0, 0.0, -1.0, 1.0);
            uvs.push_back(Vector2(onethird + u, v));
            point++;

            // left
            points.push_back(Vector3(start_pos.x, -y, z));
            normals.push_back(Vector3(-1.0, 0.0, 0.0));
            ADD_TANGENT(0.0, 0.0, 1.0, 1.0);
            uvs.push_back(Vector2(u, 0.5 + v));
            point++;

            if (i > 0 && j > 0) {
                int i2 = i * 2;

                // right
                indices.push_back(prevrow + i2 - 2);
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2 - 2);
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2);
                indices.push_back(thisrow + i2 - 2);

                // left
                indices.push_back(prevrow + i2 - 1);
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
            };

            z += size.z / (subdivide_d + 1.0);
        };

        y += size.y / (subdivide_h + 1.0);
        prevrow = thisrow;
        thisrow = point;
    };

    // top + bottom
    z = start_pos.z;
    thisrow = point;
    prevrow = 0;
    for (j = 0; j <= (subdivide_d + 1); j++) {
        x = start_pos.x;
        for (i = 0; i <= (subdivide_w + 1); i++) {
            float u = i;
            float v = j;
            u /= (3.0 * (subdivide_w + 1.0));
            v /= (2.0 * (subdivide_d + 1.0));

            // top
            points.push_back(Vector3(-x, -start_pos.y, -z));
            normals.push_back(Vector3(0.0, 1.0, 0.0));
            ADD_TANGENT(-1.0, 0.0, 0.0, 1.0);
            uvs.push_back(Vector2(onethird + u, 0.5 + v));
            point++;

            // bottom
            points.push_back(Vector3(x, start_pos.y, -z));
            normals.push_back(Vector3(0.0, -1.0, 0.0));
            ADD_TANGENT(1.0, 0.0, 0.0, 1.0);
            uvs.push_back(Vector2(twothirds + u, 0.5 + v));
            point++;

            if (i > 0 && j > 0) {
                int i2 = i * 2;

                // top
                indices.push_back(prevrow + i2 - 2);
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2 - 2);
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2);
                indices.push_back(thisrow + i2 - 2);

                // bottom
                indices.push_back(prevrow + i2 - 1);
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
            };

            x += size.x / (subdivide_w + 1.0);
        };

        z += size.z / (subdivide_d + 1.0);
        prevrow = thisrow;
        thisrow = point;
    };

    p_arr.set_positions(eastl::move(points));
    p_arr.m_normals = eastl::move(normals);
    p_arr.m_tangents = eastl::move(tangents);
    p_arr.m_uv_1 = eastl::move(uvs);
    p_arr.m_indices = eastl::move(indices);;
}

void CubeMesh::_bind_methods() {
    SE_BIND_METHOD(CubeMesh,set_size);
    SE_BIND_METHOD(CubeMesh,get_size);

    SE_BIND_METHOD(CubeMesh,set_subdivide_width);
    SE_BIND_METHOD(CubeMesh,get_subdivide_width);
    SE_BIND_METHOD(CubeMesh,set_subdivide_height);
    SE_BIND_METHOD(CubeMesh,get_subdivide_height);
    SE_BIND_METHOD(CubeMesh,set_subdivide_depth);
    SE_BIND_METHOD(CubeMesh,get_subdivide_depth);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "size"), "set_size", "get_size");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "subdivide_width", PropertyHint::Range, "0,100,1,or_greater"), "set_subdivide_width", "get_subdivide_width");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "subdivide_height", PropertyHint::Range, "0,100,1,or_greater"), "set_subdivide_height", "get_subdivide_height");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "subdivide_depth", PropertyHint::Range, "0,100,1,or_greater"), "set_subdivide_depth", "get_subdivide_depth");
}

void CubeMesh::set_size(const Vector3 &p_size) {
    size = p_size;
    _request_update();
}

Vector3 CubeMesh::get_size() const {
    return size;
}

void CubeMesh::set_subdivide_width(const int p_divisions) {
    subdivide_w = p_divisions > 0 ? p_divisions : 0;
    _request_update();
}

int CubeMesh::get_subdivide_width() const {
    return subdivide_w;
}

void CubeMesh::set_subdivide_height(const int p_divisions) {
    subdivide_h = p_divisions > 0 ? p_divisions : 0;
    _request_update();
}

int CubeMesh::get_subdivide_height() const {
    return subdivide_h;
}

void CubeMesh::set_subdivide_depth(const int p_divisions) {
    subdivide_d = p_divisions > 0 ? p_divisions : 0;
    _request_update();
}

int CubeMesh::get_subdivide_depth() const {
    return subdivide_d;
}

CubeMesh::CubeMesh() {
    // defaults
    size = Vector3(2.0, 2.0, 2.0);
    subdivide_w = default_subdivide_w;
    subdivide_h = default_subdivide_h;
    subdivide_d = default_subdivide_d;
}

/**
  CylinderMesh
*/

void CylinderMesh::_create_mesh_array(SurfaceArrays &p_arr) const {
    create_mesh_array(p_arr, top_radius, bottom_radius, height, radial_segments, rings);
}
void CylinderMesh::create_mesh_array(
        SurfaceArrays &p_arr, float top_radius, float bottom_radius, float height, int radial_segments, int rings) {
    int i, j, prevrow, thisrow, point;
    float x, y, z, u, v, radius;

    Vector<Vector3> points;
    Vector<Vector3> normals;
    Vector<float> tangents;
    Vector<Vector2> uvs;
    Vector<int> indices;
    point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
    tangents.push_back(m_x);            \
    tangents.push_back(m_y);            \
    tangents.push_back(m_z);            \
    tangents.push_back(m_d);

    thisrow = 0;
    prevrow = 0;
    for (j = 0; j <= (rings + 1); j++) {
        v = j;
        v /= (rings + 1);

        radius = top_radius + ((bottom_radius - top_radius) * v);

        y = height * v;
        y = (height * 0.5) - y;

        for (i = 0; i <= radial_segments; i++) {
            u = i;
            u /= radial_segments;

            x = sin(u * (Math_PI * 2.0));
            z = cos(u * (Math_PI * 2.0));

            Vector3 p = Vector3(x * radius, y, z * radius);
            points.push_back(p);
            normals.push_back(Vector3(x, 0.0, z));
            ADD_TANGENT(z, 0.0, -x, 1.0)
            uvs.push_back(Vector2(u, v * 0.5));
            point++;

            if (i > 0 && j > 0) {
                indices.push_back(prevrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i - 1);

                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i);
                indices.push_back(thisrow + i - 1);
            };
        };

        prevrow = thisrow;
        thisrow = point;
    };

    // add top
    if (top_radius > 0.0) {
        y = height * 0.5;

        thisrow = point;
        points.push_back(Vector3(0.0, y, 0.0));
        normals.push_back(Vector3(0.0, 1.0, 0.0));
        ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
        uvs.push_back(Vector2(0.25, 0.75));
        point++;

        for (i = 0; i <= radial_segments; i++) {
            float r = i;
            r /= radial_segments;

            x = sin(r * (Math_PI * 2.0));
            z = cos(r * (Math_PI * 2.0));

            u = ((x + 1.0) * 0.25);
            v = 0.5 + ((z + 1.0) * 0.25);

            Vector3 p = Vector3(x * top_radius, y, z * top_radius);
            points.push_back(p);
            normals.push_back(Vector3(0.0, 1.0, 0.0));
            ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
            uvs.push_back(Vector2(u, v));
            point++;

            if (i > 0) {
                indices.push_back(thisrow);
                indices.push_back(point - 1);
                indices.push_back(point - 2);
            };
        };
    };

    // add bottom
    if (bottom_radius > 0.0) {
        y = height * -0.5f;

        thisrow = point;
        points.push_back(Vector3(0.0, y, 0.0));
        normals.push_back(Vector3(0.0, -1.0, 0.0));
        ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
        uvs.push_back(Vector2(0.75f, 0.75f));
        point++;

        for (i = 0; i <= radial_segments; i++) {
            float r = i;
            r /= radial_segments;

            x = sin(r * (Math_PI * 2.0));
            z = cos(r * (Math_PI * 2.0));

            u = 0.5 + ((x + 1.0) * 0.25);
            v = 1.0 - ((z + 1.0) * 0.25);

            Vector3 p = Vector3(x * bottom_radius, y, z * bottom_radius);
            points.push_back(p);
            normals.push_back(Vector3(0.0, -1.0, 0.0));
            ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
            uvs.push_back(Vector2(u, v));
            point++;

            if (i > 0) {
                indices.push_back(thisrow);
                indices.push_back(point - 2);
                indices.push_back(point - 1);
            };
        };
    };

    p_arr.set_positions(eastl::move(points));
    p_arr.m_normals = eastl::move(normals);
    p_arr.m_tangents = eastl::move(tangents);
    p_arr.m_uv_1 = eastl::move(uvs);
    p_arr.m_indices = eastl::move(indices);;
}

void CylinderMesh::_bind_methods() {
    SE_BIND_METHOD(CylinderMesh,set_top_radius);
    SE_BIND_METHOD(CylinderMesh,get_top_radius);
    SE_BIND_METHOD(CylinderMesh,set_bottom_radius);
    SE_BIND_METHOD(CylinderMesh,get_bottom_radius);
    SE_BIND_METHOD(CylinderMesh,set_height);
    SE_BIND_METHOD(CylinderMesh,get_height);

    SE_BIND_METHOD(CylinderMesh,set_radial_segments);
    SE_BIND_METHOD(CylinderMesh,get_radial_segments);
    SE_BIND_METHOD(CylinderMesh,set_rings);
    SE_BIND_METHOD(CylinderMesh,get_rings);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "top_radius", PropertyHint::Range, "0,100.0,0.001,or_greater"), "set_top_radius", "get_top_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "bottom_radius", PropertyHint::Range, "0,100.0,0.001,or_greater"), "set_bottom_radius", "get_bottom_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "height", PropertyHint::Range, "0.001,100,0.001,or_greater"), "set_height", "get_height");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "radial_segments", PropertyHint::Range, "1,100,1,or_greater"), "set_radial_segments", "get_radial_segments");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "rings", PropertyHint::Range, "0,100,1,or_greater"), "set_rings", "get_rings");
}

void CylinderMesh::set_top_radius(const float p_radius) {
    top_radius = p_radius;
    _request_update();
}

float CylinderMesh::get_top_radius() const {
    return top_radius;
}

void CylinderMesh::set_bottom_radius(const float p_radius) {
    bottom_radius = p_radius;
    _request_update();
}

float CylinderMesh::get_bottom_radius() const {
    return bottom_radius;
}

void CylinderMesh::set_height(const float p_height) {
    height = p_height;
    _request_update();
}

float CylinderMesh::get_height() const {
    return height;
}

void CylinderMesh::set_radial_segments(const int p_segments) {
    radial_segments = p_segments > 4 ? p_segments : 4;
    _request_update();
}

int CylinderMesh::get_radial_segments() const {
    return radial_segments;
}

void CylinderMesh::set_rings(const int p_rings) {
    rings = p_rings > 0 ? p_rings : 0;
    _request_update();
}

int CylinderMesh::get_rings() const {
    return rings;
}

CylinderMesh::CylinderMesh() {
    // defaults
    top_radius = 1.0f;
    bottom_radius = 1.0f;
    height = 2.0f;
    radial_segments = default_radial_segments;
    rings = default_rings;
}

/**
  PlaneMesh
*/

void PlaneMesh::_create_mesh_array(SurfaceArrays &p_arr) const {
    int i, j, prevrow, thisrow, point;
    float x, z;

    Size2 start_pos = size * -0.5;

    Vector<Vector3> points;
    Vector<Vector3> normals;
    Vector<float> tangents;
    Vector<Vector2> uvs;
    Vector<int> indices;
    point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
    tangents.push_back(m_x);            \
    tangents.push_back(m_y);            \
    tangents.push_back(m_z);            \
    tangents.push_back(m_d);

    /* top + bottom */
    z = start_pos.y;
    thisrow = point;
    prevrow = 0;
    for (j = 0; j <= (subdivide_d + 1); j++) {
        x = start_pos.x;
        for (i = 0; i <= (subdivide_w + 1); i++) {
            float u = i;
            float v = j;
            u /= (subdivide_w + 1.0);
            v /= (subdivide_d + 1.0);

            points.push_back(Vector3(-x, 0.0, -z) + center_offset);
            normals.push_back(Vector3(0.0, 1.0, 0.0));
            ADD_TANGENT(1.0, 0.0, 0.0, 1.0);
            uvs.push_back(Vector2(1.0 - u, 1.0 - v)); /* 1.0 - uv to match orientation with Quad */
            point++;

            if (i > 0 && j > 0) {
                indices.push_back(prevrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i);
                indices.push_back(thisrow + i - 1);
            };

            x += size.x / (subdivide_w + 1.0);
        };

        z += size.y / (subdivide_d + 1.0);
        prevrow = thisrow;
        thisrow = point;
    };

    p_arr.set_positions(eastl::move(points));
    p_arr.m_normals = eastl::move(normals);
    p_arr.m_tangents = eastl::move(tangents);
    p_arr.m_uv_1 = eastl::move(uvs);
    p_arr.m_indices = eastl::move(indices);;
}

void PlaneMesh::_bind_methods() {
    SE_BIND_METHOD(PlaneMesh,set_size);
    SE_BIND_METHOD(PlaneMesh,get_size);

    SE_BIND_METHOD(PlaneMesh,set_subdivide_width);
    SE_BIND_METHOD(PlaneMesh,get_subdivide_width);
    SE_BIND_METHOD(PlaneMesh,set_subdivide_depth);
    SE_BIND_METHOD(PlaneMesh,get_subdivide_depth);
    SE_BIND_METHOD(PlaneMesh,set_center_offset);
    SE_BIND_METHOD(PlaneMesh,get_center_offset);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "size"), "set_size", "get_size");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "subdivide_width", PropertyHint::Range, "0,100,1,or_greater"), "set_subdivide_width", "get_subdivide_width");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "subdivide_depth", PropertyHint::Range, "0,100,1,or_greater"), "set_subdivide_depth", "get_subdivide_depth");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "center_offset"), "set_center_offset", "get_center_offset");
}

void PlaneMesh::set_size(const Size2 &p_size) {
    size = p_size;
    _request_update();
}

Size2 PlaneMesh::get_size() const {
    return size;
}

void PlaneMesh::set_subdivide_width(const int p_divisions) {
    subdivide_w = p_divisions > 0 ? p_divisions : 0;
    _request_update();
}

int PlaneMesh::get_subdivide_width() const {
    return subdivide_w;
}

void PlaneMesh::set_subdivide_depth(const int p_divisions) {
    subdivide_d = p_divisions > 0 ? p_divisions : 0;
    _request_update();
}

int PlaneMesh::get_subdivide_depth() const {
    return subdivide_d;
}

void PlaneMesh::set_center_offset(const Vector3 p_offset) {
    center_offset = p_offset;
    _request_update();
}

Vector3 PlaneMesh::get_center_offset() const {
    return center_offset;
}

/**
  PrismMesh
*/

void PrismMesh::_create_mesh_array(SurfaceArrays &p_arr) const {
    int i, j, prevrow, thisrow, point;
    float x, y, z;
    const float onethird = 1.0f / 3.0f;
    const float twothirds = 2.0f / 3.0f;

    Vector3 start_pos = size * -0.5f;

    // set our bounding box

    Vector<Vector3> points;
    Vector<Vector3> normals;
    Vector<float> tangents;
    Vector<Vector2> uvs;
    Vector<int> indices;
    point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
    tangents.push_back(m_x);            \
    tangents.push_back(m_y);            \
    tangents.push_back(m_z);            \
    tangents.push_back(m_d);

    /* front + back */
    y = start_pos.y;
    thisrow = point;
    prevrow = 0;
    for (j = 0; j <= (subdivide_h + 1); j++) {
        float scale = (y - start_pos.y) / size.y;
        float scaled_size_x = size.x * scale;
        float start_x = start_pos.x + (1.0 - scale) * size.x * left_to_right;
        float offset_front = (1.0 - scale) * onethird * left_to_right;
        float offset_back = (1.0 - scale) * onethird * (1.0 - left_to_right);

        x = 0.0;
        for (i = 0; i <= (subdivide_w + 1); i++) {
            float u = i;
            float v = j;
            u /= (3.0 * (subdivide_w + 1.0));
            v /= (2.0 * (subdivide_h + 1.0));

            u *= scale;

            /* front */
            points.push_back(Vector3(start_x + x, -y, -start_pos.z)); // double negative on the Z!
            normals.push_back(Vector3(0.0, 0.0, 1.0));
            ADD_TANGENT(1.0, 0.0, 0.0, 1.0);
            uvs.push_back(Vector2(offset_front + u, v));
            point++;

            /* back */
            points.push_back(Vector3(start_x + scaled_size_x - x, -y, start_pos.z));
            normals.push_back(Vector3(0.0, 0.0, -1.0));
            ADD_TANGENT(-1.0, 0.0, 0.0, 1.0);
            uvs.push_back(Vector2(twothirds + offset_back + u, v));
            point++;

            if (i > 0 && j == 1) {
                int i2 = i * 2;

                /* front */
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2);
                indices.push_back(thisrow + i2 - 2);

                /* back */
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
            } else if (i > 0 && j > 0) {
                int i2 = i * 2;

                /* front */
                indices.push_back(prevrow + i2 - 2);
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2 - 2);
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2);
                indices.push_back(thisrow + i2 - 2);

                /* back */
                indices.push_back(prevrow + i2 - 1);
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
            };

            x += scale * size.x / (subdivide_w + 1.0);
        };

        y += size.y / (subdivide_h + 1.0);
        prevrow = thisrow;
        thisrow = point;
    };

    /* left + right */
    Vector3 normal_left, normal_right;

    normal_left = Vector3(-size.y, size.x * left_to_right, 0.0f);
    normal_right = Vector3(size.y, size.x * (1.0f - left_to_right), 0.0f);
    normal_left.normalize();
    normal_right.normalize();

    y = start_pos.y;
    thisrow = point;
    prevrow = 0;
    for (j = 0; j <= (subdivide_h + 1); j++) {
        float left, right;
        float scale = (y - start_pos.y) / size.y;

        left = start_pos.x + (size.x * (1.0 - scale) * left_to_right);
        right = left + (size.x * scale);

        z = start_pos.z;
        for (i = 0; i <= (subdivide_d + 1); i++) {
            float u = i;
            float v = j;
            u /= (3.0 * (subdivide_d + 1.0));
            v /= (2.0 * (subdivide_h + 1.0));

            /* right */
            points.push_back(Vector3(right, -y, -z));
            normals.push_back(normal_right);
            ADD_TANGENT(0.0, 0.0, -1.0, 1.0);
            uvs.push_back(Vector2(onethird + u, v));
            point++;

            /* left */
            points.push_back(Vector3(left, -y, z));
            normals.push_back(normal_left);
            ADD_TANGENT(0.0, 0.0, 1.0, 1.0);
            uvs.push_back(Vector2(u, 0.5 + v));
            point++;

            if (i > 0 && j > 0) {
                int i2 = i * 2;

                /* right */
                indices.push_back(prevrow + i2 - 2);
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2 - 2);
                indices.push_back(prevrow + i2);
                indices.push_back(thisrow + i2);
                indices.push_back(thisrow + i2 - 2);

                /* left */
                indices.push_back(prevrow + i2 - 1);
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
                indices.push_back(prevrow + i2 + 1);
                indices.push_back(thisrow + i2 + 1);
                indices.push_back(thisrow + i2 - 1);
            };

            z += size.z / (subdivide_d + 1.0);
        };

        y += size.y / (subdivide_h + 1.0);
        prevrow = thisrow;
        thisrow = point;
    };

    /* bottom */
    z = start_pos.z;
    thisrow = point;
    prevrow = 0;
    for (j = 0; j <= (subdivide_d + 1); j++) {
        x = start_pos.x;
        for (i = 0; i <= (subdivide_w + 1); i++) {
            float u = i;
            float v = j;
            u /= (3.0f * (subdivide_w + 1.0f));
            v /= (2.0f * (subdivide_d + 1.0f));

            /* bottom */
            points.push_back(Vector3(x, start_pos.y, -z));
            normals.push_back(Vector3(0.0f, -1.0f, 0.0f));
            ADD_TANGENT(1.0, 0.0, 0.0, 1.0);
            uvs.push_back(Vector2(twothirds + u, 0.5f + v));
            point++;

            if (i > 0 && j > 0) {
                /* bottom */
                indices.push_back(prevrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i);
                indices.push_back(thisrow + i - 1);
            };

            x += size.x / (subdivide_w + 1.0f);
        };

        z += size.z / (subdivide_d + 1.0f);
        prevrow = thisrow;
        thisrow = point;
    };

    p_arr.set_positions(eastl::move(points));
    p_arr.m_normals = eastl::move(normals);
    p_arr.m_tangents = eastl::move(tangents);
    p_arr.m_uv_1 = eastl::move(uvs);
    p_arr.m_indices = eastl::move(indices);;
}

void PrismMesh::_bind_methods() {
    SE_BIND_METHOD(PrismMesh,set_left_to_right);
    SE_BIND_METHOD(PrismMesh,get_left_to_right);

    SE_BIND_METHOD(PrismMesh,set_size);
    SE_BIND_METHOD(PrismMesh,get_size);

    SE_BIND_METHOD(PrismMesh,set_subdivide_width);
    SE_BIND_METHOD(PrismMesh,get_subdivide_width);
    SE_BIND_METHOD(PrismMesh,set_subdivide_height);
    SE_BIND_METHOD(PrismMesh,get_subdivide_height);
    SE_BIND_METHOD(PrismMesh,set_subdivide_depth);
    SE_BIND_METHOD(PrismMesh,get_subdivide_depth);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "left_to_right", PropertyHint::Range, "-2.0,2.0,0.1"), "set_left_to_right", "get_left_to_right");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "size"), "set_size", "get_size");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "subdivide_width", PropertyHint::Range, "0,100,1,or_greater"), "set_subdivide_width", "get_subdivide_width");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "subdivide_height", PropertyHint::Range, "0,100,1,or_greater"), "set_subdivide_height", "get_subdivide_height");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "subdivide_depth", PropertyHint::Range, "0,100,1,or_greater"), "set_subdivide_depth", "get_subdivide_depth");
}

void PrismMesh::set_left_to_right(const float p_left_to_right) {
    left_to_right = p_left_to_right;
    _request_update();
}

float PrismMesh::get_left_to_right() const {
    return left_to_right;
}

void PrismMesh::set_size(const Vector3 &p_size) {
    size = p_size;
    _request_update();
}

Vector3 PrismMesh::get_size() const {
    return size;
}

void PrismMesh::set_subdivide_width(const int p_divisions) {
    subdivide_w = p_divisions > 0 ? p_divisions : 0;
    _request_update();
}

int PrismMesh::get_subdivide_width() const {
    return subdivide_w;
}

void PrismMesh::set_subdivide_height(const int p_divisions) {
    subdivide_h = p_divisions > 0 ? p_divisions : 0;
    _request_update();
}

int PrismMesh::get_subdivide_height() const {
    return subdivide_h;
}

void PrismMesh::set_subdivide_depth(const int p_divisions) {
    subdivide_d = p_divisions > 0 ? p_divisions : 0;
    _request_update();
}

int PrismMesh::get_subdivide_depth() const {
    return subdivide_d;
}

PrismMesh::PrismMesh() {
    // defaults
    left_to_right = 0.5;
    size = Vector3(2.0, 2.0, 2.0);
    subdivide_w = 0;
    subdivide_h = 0;
    subdivide_d = 0;
}

/**
  QuadMesh
*/

void QuadMesh::_create_mesh_array(SurfaceArrays &p_arr) const {
    Vector<Vector3> faces;
    Vector<Vector3> normals;
    Vector<float> tangents;
    Vector<Vector2> uvs;

    faces.resize(6);
    normals.resize(6);
    tangents.resize(6 * 4);
    uvs.resize(6);

    Vector2 _size = Vector2(size.x / 2.0f, size.y / 2.0f);

    const Vector3 quad_faces[4] = {
        Vector3(-_size.x, -_size.y, 0) + center_offset,
        Vector3(-_size.x, _size.y, 0) + center_offset,
        Vector3(_size.x, _size.y, 0) + center_offset,
        Vector3(_size.x, -_size.y, 0) + center_offset,
    };

    static const int indices[6] = {
        0, 1, 2,
        0, 2, 3
    };

    for (int i = 0; i < 6; i++) {

        int j = indices[i];
        faces[i] = quad_faces[j];
        normals[i] = Vector3(0, 0, 1);
        tangents[i * 4 + 0] = 1.0;
        tangents[i * 4 + 1] = 0.0;
        tangents[i * 4 + 2] = 0.0;
        tangents[i * 4 + 3] = 1.0;

        static const Vector2 quad_uv[4] = {
            Vector2(0, 1),
            Vector2(0, 0),
            Vector2(1, 0),
            Vector2(1, 1),
        };

        uvs[i]=quad_uv[j];
    }

    p_arr.set_positions(eastl::move(faces));
    p_arr.m_normals = eastl::move(normals);
    p_arr.m_tangents = eastl::move(tangents);
    p_arr.m_uv_1 = eastl::move(uvs);
}

void QuadMesh::_bind_methods() {
    SE_BIND_METHOD(QuadMesh,set_size);
    SE_BIND_METHOD(QuadMesh,get_size);
    SE_BIND_METHOD(QuadMesh,set_center_offset);
    SE_BIND_METHOD(QuadMesh,get_center_offset);
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "size"), "set_size", "get_size");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "center_offset"), "set_center_offset", "get_center_offset");
}

QuadMesh::QuadMesh() {
    primitive_type = PRIMITIVE_TRIANGLES;
    size = Size2(1.0, 1.0);
}

void QuadMesh::set_size(const Size2 &p_size) {
    size = p_size;
    _request_update();
}

Size2 QuadMesh::get_size() const {
    return size;
}

void QuadMesh::set_center_offset(Vector3 p_center_offset) {
    center_offset = p_center_offset;
    _request_update();
}

Vector3 QuadMesh::get_center_offset() const {
    return center_offset;
}
/**
  SphereMesh
*/

void SphereMesh::_create_mesh_array(SurfaceArrays &p_arr) const {
    create_mesh_array(p_arr, radius, height, radial_segments, rings, is_hemisphere);
}
void SphereMesh::create_mesh_array(
        SurfaceArrays &p_arr, float radius, float height, int radial_segments, int rings, bool is_hemisphere) {
    int i, j, prevrow, thisrow, point;
    float y;

    float scale = height * (is_hemisphere ? 1.0f : 0.5f);
    // set our bounding box

    Vector<Vector3> points;
    Vector<Vector3> normals;
    Vector<float> tangents;
    Vector<Vector2> uvs;
    Vector<int> indices;
    point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
    tangents.push_back(m_x);            \
    tangents.push_back(m_y);            \
    tangents.push_back(m_z);            \
    tangents.push_back(m_d);

    thisrow = 0;
    prevrow = 0;
    for (j = 0; j <= (rings + 1); j++) {
        float v = j;
        float w;

        v /= (rings + 1);
        w = sin(Math_PI * v);
        y = scale * std::cos(Math_PI * v);

        for (i = 0; i <= radial_segments; i++) {
            float u = i;
            u /= radial_segments;

            float x = std::sin(u * (Math_PI * 2.0f));
            float z = std::cos(u * (Math_PI * 2.0f));

            if (is_hemisphere && y < 0.0) {
                points.push_back(Vector3(x * radius * w, 0.0, z * radius * w));
                normals.push_back(Vector3(0.0, -1.0, 0.0));
            } else {
                Vector3 p = Vector3(x * radius * w, y, z * radius * w);
                points.push_back(p);
                Vector3 normal = Vector3(x * w * scale, radius * (y / scale), z * w * scale);
                normals.push_back(normal.normalized());
            };
            ADD_TANGENT(z, 0.0, -x, 1.0)
            uvs.push_back(Vector2(u, v));
            point++;

            if (i > 0 && j > 0) {
                indices.push_back(prevrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i - 1);

                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i);
                indices.push_back(thisrow + i - 1);
            };
        };

        prevrow = thisrow;
        thisrow = point;
    };

    p_arr.set_positions(eastl::move(points));
    p_arr.m_normals = eastl::move(normals);
    p_arr.m_tangents = eastl::move(tangents);
    p_arr.m_uv_1 = eastl::move(uvs);
    p_arr.m_indices = eastl::move(indices);;
}

void SphereMesh::_bind_methods() {
    SE_BIND_METHOD(SphereMesh,set_radius);
    SE_BIND_METHOD(SphereMesh,get_radius);
    SE_BIND_METHOD(SphereMesh,set_height);
    SE_BIND_METHOD(SphereMesh,get_height);

    SE_BIND_METHOD(SphereMesh,set_radial_segments);
    SE_BIND_METHOD(SphereMesh,get_radial_segments);
    SE_BIND_METHOD(SphereMesh,set_rings);
    SE_BIND_METHOD(SphereMesh,get_rings);

    SE_BIND_METHOD(SphereMesh,set_is_hemisphere);
    SE_BIND_METHOD(SphereMesh,get_is_hemisphere);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "radius", PropertyHint::Range, "0.001,100.0,0.001,or_greater"), "set_radius", "get_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "height", PropertyHint::Range, "0.001,100.0,0.001,or_greater"), "set_height", "get_height");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "radial_segments", PropertyHint::Range, "1,100,1,or_greater"), "set_radial_segments", "get_radial_segments");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "rings", PropertyHint::Range, "1,100,1,or_greater"), "set_rings", "get_rings");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "is_hemisphere"), "set_is_hemisphere", "get_is_hemisphere");
}

void SphereMesh::set_radius(const float p_radius) {
    radius = p_radius;
    _request_update();
}

float SphereMesh::get_radius() const {
    return radius;
}

void SphereMesh::set_height(const float p_height) {
    height = p_height;
    _request_update();
}

float SphereMesh::get_height() const {
    return height;
}

void SphereMesh::set_radial_segments(const int p_radial_segments) {
    radial_segments = p_radial_segments > 4 ? p_radial_segments : 4;
    _request_update();
}

int SphereMesh::get_radial_segments() const {
    return radial_segments;
}

void SphereMesh::set_rings(const int p_rings) {
    rings = p_rings > 1 ? p_rings : 1;
    _request_update();
}

int SphereMesh::get_rings() const {
    return rings;
}

void SphereMesh::set_is_hemisphere(const bool p_is_hemisphere) {
    is_hemisphere = p_is_hemisphere;
    _request_update();
}

bool SphereMesh::get_is_hemisphere() const {
    return is_hemisphere;
}

SphereMesh::SphereMesh() {
    // defaults
    radius = 1.0;
    height = 2.0;
    radial_segments = default_radial_segments;
    rings = default_rings;
    is_hemisphere = default_is_hemisphere;
}

/**
  PointMesh
*/

void PointMesh::_create_mesh_array(SurfaceArrays &p_arr) const {
    Vector<Vector3> faces {
        Vector3(0.0, 0.0, 0.0)
    };

    p_arr.set_positions(eastl::move(faces));
}

PointMesh::PointMesh() {
    primitive_type = PRIMITIVE_POINTS;
}
