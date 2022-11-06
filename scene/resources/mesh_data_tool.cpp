/*************************************************************************/
/*  mesh_data_tool.cpp                                                   */
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

#include "mesh_data_tool.h"
#include "core/method_bind.h"
#include "scene/resources/material.h"
#include "core/map.h"

IMPL_GDCLASS(MeshDataTool)

void MeshDataTool::clear() {

    vertices.clear();
    edges.clear();
    faces.clear();
    material = Ref<Material>();
    format = 0;
}

Error MeshDataTool::create_from_surface(const Ref<ArrayMesh> &p_mesh, int p_surface) {

    ERR_FAIL_COND_V(not p_mesh, ERR_INVALID_PARAMETER);
    ERR_FAIL_COND_V(p_mesh->surface_get_primitive_type(p_surface) != Mesh::PRIMITIVE_TRIANGLES, ERR_INVALID_PARAMETER);

    SurfaceArrays arrays = p_mesh->surface_get_arrays(p_surface);
    ERR_FAIL_COND_V(arrays.empty(), ERR_INVALID_PARAMETER);

    Span<const Vector3> varray = arrays.positions3();

    int vcount = varray.size();
    ERR_FAIL_COND_V(vcount == 0, ERR_INVALID_PARAMETER);

    Vector<int> indices;

    if (!arrays.m_indices.empty()) {

        indices = arrays.m_indices;
    } else {
        //make code simpler
        indices.reserve(vcount);
        for (int i = 0; i < vcount; i++)
            indices.emplace_back(i);
    }

    int icount = indices.size();

    ERR_FAIL_COND_V(icount == 0, ERR_INVALID_PARAMETER);
    ERR_FAIL_COND_V(icount % 3, ERR_INVALID_PARAMETER);
    for (int ri : indices) {
        ERR_FAIL_INDEX_V(ri, vcount, ERR_INVALID_PARAMETER);
    }

    clear();
    format = p_mesh->surface_get_format(p_surface);
    material = p_mesh->surface_get_material(p_surface);

    Vector3 *nr=nullptr;
    if (!arrays.m_normals.empty())
        nr = arrays.m_normals.data();

    float *ta=nullptr;
    if (!arrays.m_tangents.empty())
        ta = arrays.m_tangents.data();


    Vector2 *uv=nullptr;
    if (!arrays.m_uv_1.empty())
        uv = arrays.m_uv_1.data();

    Vector2 *uv2=nullptr;
    if (!arrays.m_uv_2.empty())
        uv2 = arrays.m_uv_2.data();

    Color *col=nullptr;
    if (!arrays.m_colors.empty())
        col = arrays.m_colors.data();

    int *bo = nullptr;
    if (!arrays.m_bones.empty())
        bo = arrays.m_bones.data();

    float *we = nullptr;
    if (!arrays.m_weights.empty())
        we = arrays.m_weights.data();

    vertices.resize(vcount);

    for (int i = 0; i < vcount; i++) {

        Vertex v;
        v.vertex = varray[i];
        if (nr)
            v.normal = nr[i];
        if (ta)
            v.tangent = Plane(ta[i * 4 + 0], ta[i * 4 + 1], ta[i * 4 + 2], ta[i * 4 + 3]);
        if (uv)
            v.uv = uv[i];
        if (uv2)
            v.uv2 = uv2[i];
        if (col)
            v.color = col[i];

        if (we) {

            v.weights.push_back(we[i * 4 + 0]);
            v.weights.push_back(we[i * 4 + 1]);
            v.weights.push_back(we[i * 4 + 2]);
            v.weights.push_back(we[i * 4 + 3]);
        }

        if (bo) {

            v.bones.push_back(bo[i * 4 + 0]);
            v.bones.push_back(bo[i * 4 + 1]);
            v.bones.push_back(bo[i * 4 + 2]);
            v.bones.push_back(bo[i * 4 + 3]);
        }

        vertices[i] = v;
    }

    Map<Point2i, int> edge_indices;

    for (int i = 0; i < icount; i += 3) {

        Vertex *v[3] = { &vertices[indices[i + 0]], &vertices[indices[i + 1]], &vertices[indices[i + 2]] };

        int fidx = faces.size();
        Face face;

        for (int j = 0; j < 3; j++) {

            face.v[j] = indices[i + j];

            Point2i edge(indices[i + j], indices[i + (j + 1) % 3]);
            if (edge.x > edge.y) {
                SWAP(edge.x, edge.y);
            }

            if (edge_indices.contains(edge)) {
                face.edges[j] = edge_indices[edge];

            } else {
                face.edges[j] = edge_indices.size();
                edge_indices[edge] = face.edges[j];
                Edge e;
                e.vertex[0] = edge.x;
                e.vertex[1] = edge.y;
                edges.push_back(e);
                v[j]->edges.push_back(face.edges[j]);
                v[(j + 1) % 3]->edges.push_back(face.edges[j]);
            }

            edges[face.edges[j]].faces.push_back(fidx);
            v[j]->faces.push_back(fidx);
        }

        faces.push_back(face);
    }

    return OK;
}

Error MeshDataTool::commit_to_surface(const Ref<ArrayMesh> &p_mesh) {

    ERR_FAIL_COND_V(not p_mesh, ERR_INVALID_PARAMETER);
    SurfaceArrays arr;
    int vcount = vertices.size();

    Vector<Vector3> v;
    Vector<Vector3> n;
    Vector<real_t> t;
    Vector<Vector2> u;
    Vector<Vector2> u2;
    Vector<Color> c;
    Vector<int> b;
    Vector<real_t> w;
    Vector<int> in;

    {

        v.resize(vcount);
        if (format & Mesh::ARRAY_FORMAT_NORMAL) {
            n.resize(vcount);
        }

        if (format & Mesh::ARRAY_FORMAT_TANGENT) {
            t.resize(vcount * 4);
        }

        if (format & Mesh::ARRAY_FORMAT_TEX_UV) {
            u.resize(vcount);
        }

        if (format & Mesh::ARRAY_FORMAT_TEX_UV2) {
            u2.resize(vcount);
        }

        if (format & Mesh::ARRAY_FORMAT_COLOR) {
            c.resize(vcount);
        }

        if (format & Mesh::ARRAY_FORMAT_BONES) {
            b.resize(vcount * 4);
        }

        if (format & Mesh::ARRAY_FORMAT_WEIGHTS) {
            w.resize(vcount * 4);
        }

        for (int i = 0; i < vcount; i++) {

            const Vertex &vtx = vertices[i];

            v[i] = vtx.vertex;
            if (format & Mesh::ARRAY_FORMAT_NORMAL)
                n[i] = vtx.normal;
            if (format & Mesh::ARRAY_FORMAT_TANGENT) {
                t[i * 4 + 0] = vtx.tangent.normal.x;
                t[i * 4 + 1] = vtx.tangent.normal.y;
                t[i * 4 + 2] = vtx.tangent.normal.z;
                t[i * 4 + 3] = vtx.tangent.d;
            }
            if (format & Mesh::ARRAY_FORMAT_TEX_UV)
                u[i] = vtx.uv;
            if (format & Mesh::ARRAY_FORMAT_TEX_UV2)
                u2[i] = vtx.uv2;
            if (format & Mesh::ARRAY_FORMAT_COLOR)
                c[i] = vtx.color;

            if (format & Mesh::ARRAY_FORMAT_WEIGHTS) {

                w[i * 4 + 0] = vtx.weights[0];
                w[i * 4 + 1] = vtx.weights[1];
                w[i * 4 + 2] = vtx.weights[2];
                w[i * 4 + 3] = vtx.weights[3];
            }

            if (format & Mesh::ARRAY_FORMAT_BONES) {

                b[i * 4 + 0] = vtx.bones[0];
                b[i * 4 + 1] = vtx.bones[1];
                b[i * 4 + 2] = vtx.bones[2];
                b[i * 4 + 3] = vtx.bones[3];
            }
        }

        int fc = faces.size();
        in.resize(fc * 3);
        for (int i = 0; i < fc; i++) {

            in[i * 3 + 0] = faces[i].v[0];
            in[i * 3 + 1] = faces[i].v[1];
            in[i * 3 + 2] = faces[i].v[2];
        }
    }

    arr.set_positions(eastl::move(v));
    arr.m_indices = eastl::move(in);
    if (!n.empty())
        arr.m_normals = eastl::move(n);
    if (c.size())
        arr.m_colors = eastl::move(c);
    if (u.size())
        arr.m_uv_1 = eastl::move(u);
    if (u2.size())
        arr.m_uv_2 = eastl::move(u2);
    if (t.size())
        arr.m_tangents = eastl::move(t);
    if (b.size())
        arr.m_bones = eastl::move(b);
    if (w.size())
        arr.m_weights = eastl::move(w);

    Ref<ArrayMesh> ncmesh = p_mesh;
    int sc = ncmesh->get_surface_count();
    ncmesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, eastl::move(arr));
    ncmesh->surface_set_material(sc, material);

    return OK;
}

int MeshDataTool::get_format() const {

    return format;
}

int MeshDataTool::get_vertex_count() const {

    return vertices.size();
}
int MeshDataTool::get_edge_count() const {

    return edges.size();
}
int MeshDataTool::get_face_count() const {

    return faces.size();
}

Vector3 MeshDataTool::get_vertex(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Vector3());
    return vertices[p_idx].vertex;
}
void MeshDataTool::set_vertex(int p_idx, const Vector3 &p_vertex) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices[p_idx].vertex = p_vertex;
}

Vector3 MeshDataTool::get_vertex_normal(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Vector3());
    return vertices[p_idx].normal;
}
void MeshDataTool::set_vertex_normal(int p_idx, const Vector3 &p_normal) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices[p_idx].normal = p_normal;
    format |= Mesh::ARRAY_FORMAT_NORMAL;
}

Plane MeshDataTool::get_vertex_tangent(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Plane());
    return vertices[p_idx].tangent;
}
void MeshDataTool::set_vertex_tangent(int p_idx, const Plane &p_tangent) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices[p_idx].tangent = p_tangent;
    format |= Mesh::ARRAY_FORMAT_TANGENT;
}

Vector2 MeshDataTool::get_vertex_uv(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Vector2());
    return vertices[p_idx].uv;
}
void MeshDataTool::set_vertex_uv(int p_idx, const Vector2 &p_uv) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices[p_idx].uv = p_uv;
    format |= Mesh::ARRAY_FORMAT_TEX_UV;
}

Vector2 MeshDataTool::get_vertex_uv2(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Vector2());
    return vertices[p_idx].uv2;
}
void MeshDataTool::set_vertex_uv2(int p_idx, const Vector2 &p_uv2) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices[p_idx].uv2 = p_uv2;
    format |= Mesh::ARRAY_FORMAT_TEX_UV2;
}

Color MeshDataTool::get_vertex_color(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Color());
    return vertices[p_idx].color;
}
void MeshDataTool::set_vertex_color(int p_idx, const Color &p_color) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices[p_idx].color = p_color;
    format |= Mesh::ARRAY_FORMAT_COLOR;
}

const Vector<int> &MeshDataTool::get_vertex_bones(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, vertices.size(), null_int_pvec);
    return vertices[p_idx].bones;
}
void MeshDataTool::set_vertex_bones(int p_idx, Vector<int> &&p_bones) {
    ERR_FAIL_INDEX(p_idx, vertices.size());
    ERR_FAIL_COND(p_bones.size() != 4);

    vertices[p_idx].bones = eastl::move(p_bones);
    format |= Mesh::ARRAY_FORMAT_BONES;
}

const Vector<float> &MeshDataTool::get_vertex_weights(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), null_float_pvec);
    return vertices[p_idx].weights;
}
void MeshDataTool::set_vertex_weights(int p_idx, Vector<float> &&p_weights) {
    ERR_FAIL_INDEX(p_idx, vertices.size());
    ERR_FAIL_COND(p_weights.size() != 4);

    vertices[p_idx].weights = eastl::move(p_weights);
    format |= Mesh::ARRAY_FORMAT_WEIGHTS;
}

Variant MeshDataTool::get_vertex_meta(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Variant());
    return vertices[p_idx].meta;
}

void MeshDataTool::set_vertex_meta(int p_idx, const Variant &p_meta) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices[p_idx].meta = p_meta;
}

const Vector<int> &MeshDataTool::get_vertex_edges(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), null_int_pvec);
    return vertices[p_idx].edges;
}

const Vector<int> &MeshDataTool::get_vertex_faces(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), null_int_pvec);
    return vertices[p_idx].faces;
}

int MeshDataTool::get_edge_vertex(int p_edge, int p_vertex) const {

    ERR_FAIL_INDEX_V(p_edge, edges.size(), -1);
    ERR_FAIL_INDEX_V(p_vertex, 2, -1);
    return edges[p_edge].vertex[p_vertex];
}

const Vector<int> &MeshDataTool::get_edge_faces(int p_edge) const {

    ERR_FAIL_INDEX_V(p_edge, edges.size(), null_int_pvec);
    return edges[p_edge].faces;
}
Variant MeshDataTool::get_edge_meta(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, edges.size(), Variant());
    return edges[p_idx].meta;
}
void MeshDataTool::set_edge_meta(int p_idx, const Variant &p_meta) {

    ERR_FAIL_INDEX(p_idx, edges.size());
    edges[p_idx].meta = p_meta;
}

int MeshDataTool::get_face_vertex(int p_face, int p_vertex) const {

    ERR_FAIL_INDEX_V(p_face, faces.size(), -1);
    ERR_FAIL_INDEX_V(p_vertex, 3, -1);
    return faces[p_face].v[p_vertex];
}
int MeshDataTool::get_face_edge(int p_face, int p_vertex) const {

    ERR_FAIL_INDEX_V(p_face, faces.size(), -1);
    ERR_FAIL_INDEX_V(p_vertex, 3, -1);
    return faces[p_face].edges[p_vertex];
}
Variant MeshDataTool::get_face_meta(int p_face) const {

    ERR_FAIL_INDEX_V(p_face, faces.size(), Variant());
    return faces[p_face].meta;
}
void MeshDataTool::set_face_meta(int p_face, const Variant &p_meta) {

    ERR_FAIL_INDEX(p_face, faces.size());
    faces[p_face].meta = p_meta;
}

Vector3 MeshDataTool::get_face_normal(int p_face) const {

    ERR_FAIL_INDEX_V(p_face, faces.size(), Vector3());
    Vector3 v0 = vertices[faces[p_face].v[0]].vertex;
    Vector3 v1 = vertices[faces[p_face].v[1]].vertex;
    Vector3 v2 = vertices[faces[p_face].v[2]].vertex;

    return Plane(v0, v1, v2).normal;
}

Ref<Material> MeshDataTool::get_material() const {

    return material;
}

void MeshDataTool::set_material(const Ref<Material> &p_material) {

    material = p_material;
}

void MeshDataTool::_bind_methods() {

    SE_BIND_METHOD(MeshDataTool,clear);
    SE_BIND_METHOD(MeshDataTool,create_from_surface);
    SE_BIND_METHOD(MeshDataTool,commit_to_surface);

    SE_BIND_METHOD(MeshDataTool,get_format);

    SE_BIND_METHOD(MeshDataTool,get_vertex_count);
    SE_BIND_METHOD(MeshDataTool,get_edge_count);
    SE_BIND_METHOD(MeshDataTool,get_face_count);

    SE_BIND_METHOD(MeshDataTool,set_vertex);
    SE_BIND_METHOD(MeshDataTool,get_vertex);

    SE_BIND_METHOD(MeshDataTool,set_vertex_normal);
    SE_BIND_METHOD(MeshDataTool,get_vertex_normal);

    SE_BIND_METHOD(MeshDataTool,set_vertex_tangent);
    SE_BIND_METHOD(MeshDataTool,get_vertex_tangent);

    SE_BIND_METHOD(MeshDataTool,set_vertex_uv);
    SE_BIND_METHOD(MeshDataTool,get_vertex_uv);

    SE_BIND_METHOD(MeshDataTool,set_vertex_uv2);
    SE_BIND_METHOD(MeshDataTool,get_vertex_uv2);

    SE_BIND_METHOD(MeshDataTool,set_vertex_color);
    SE_BIND_METHOD(MeshDataTool,get_vertex_color);

    SE_BIND_METHOD(MeshDataTool,set_vertex_bones);
    SE_BIND_METHOD(MeshDataTool,get_vertex_bones);

    SE_BIND_METHOD(MeshDataTool,set_vertex_weights);
    SE_BIND_METHOD(MeshDataTool,get_vertex_weights);

    SE_BIND_METHOD(MeshDataTool,set_vertex_meta);
    SE_BIND_METHOD(MeshDataTool,get_vertex_meta);

    SE_BIND_METHOD(MeshDataTool,get_vertex_edges);
    SE_BIND_METHOD(MeshDataTool,get_vertex_faces);

    SE_BIND_METHOD(MeshDataTool,get_edge_vertex);
    SE_BIND_METHOD(MeshDataTool,get_edge_faces);

    SE_BIND_METHOD(MeshDataTool,set_edge_meta);
    SE_BIND_METHOD(MeshDataTool,get_edge_meta);

    SE_BIND_METHOD(MeshDataTool,get_face_vertex);
    SE_BIND_METHOD(MeshDataTool,get_face_edge);

    SE_BIND_METHOD(MeshDataTool,set_face_meta);
    SE_BIND_METHOD(MeshDataTool,get_face_meta);

    SE_BIND_METHOD(MeshDataTool,get_face_normal);

    SE_BIND_METHOD(MeshDataTool,set_material);
    SE_BIND_METHOD(MeshDataTool,get_material);
}

MeshDataTool::MeshDataTool() {

    clear();
}
