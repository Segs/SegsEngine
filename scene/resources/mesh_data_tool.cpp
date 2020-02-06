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
// null values used in functions that return references
static const PoolVector<int> null_pvint_val;
static const PoolVector<float> null_pvfloat_val;

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

    Array arrays = p_mesh->surface_get_arrays(p_surface);
    ERR_FAIL_COND_V(arrays.empty(), ERR_INVALID_PARAMETER);

    PoolVector<Vector3> varray = arrays[Mesh::ARRAY_VERTEX];

    int vcount = varray.size();
    ERR_FAIL_COND_V(vcount == 0, ERR_INVALID_PARAMETER);

    clear();
    format = p_mesh->surface_get_format(p_surface);
    material = p_mesh->surface_get_material(p_surface);

    PoolVector<Vector3>::Read vr = varray.read();

    PoolVector<Vector3>::Read nr;
    if (arrays[Mesh::ARRAY_NORMAL].get_type() != VariantType::NIL)
        nr = arrays[Mesh::ARRAY_NORMAL].as<PoolVector<Vector3>>().read();

    PoolVector<real_t>::Read ta;
    if (arrays[Mesh::ARRAY_TANGENT].get_type() != VariantType::NIL)
        ta = arrays[Mesh::ARRAY_TANGENT].as<PoolVector<real_t>>().read();

    PoolVector<Vector2>::Read uv;
    if (arrays[Mesh::ARRAY_TEX_UV].get_type() != VariantType::NIL)
        uv = arrays[Mesh::ARRAY_TEX_UV].as<PoolVector<Vector2>>().read();
    PoolVector<Vector2>::Read uv2;
    if (arrays[Mesh::ARRAY_TEX_UV2].get_type() != VariantType::NIL)
        uv2 = arrays[Mesh::ARRAY_TEX_UV2].as<PoolVector<Vector2>>().read();

    PoolVector<Color>::Read col;
    if (arrays[Mesh::ARRAY_COLOR].get_type() != VariantType::NIL)
        col = arrays[Mesh::ARRAY_COLOR].as<PoolVector<Color>>().read();

    PoolVector<int>::Read bo;
    if (arrays[Mesh::ARRAY_BONES].get_type() != VariantType::NIL)
        bo = arrays[Mesh::ARRAY_BONES].as<PoolVector<int>>().read();

    PoolVector<real_t>::Read we;
    if (arrays[Mesh::ARRAY_WEIGHTS].get_type() != VariantType::NIL)
        we = arrays[Mesh::ARRAY_WEIGHTS].as<PoolVector<real_t>>().read();

    vertices.resize(vcount);

    for (int i = 0; i < vcount; i++) {

        Vertex v;
        v.vertex = vr[i];
        if (nr.ptr())
            v.normal = nr[i];
        if (ta.ptr())
            v.tangent = Plane(ta[i * 4 + 0], ta[i * 4 + 1], ta[i * 4 + 2], ta[i * 4 + 3]);
        if (uv.ptr())
            v.uv = uv[i];
        if (uv2.ptr())
            v.uv2 = uv2[i];
        if (col.ptr())
            v.color = col[i];

        if (we.ptr()) {

            v.weights.push_back(we[i * 4 + 0]);
            v.weights.push_back(we[i * 4 + 1]);
            v.weights.push_back(we[i * 4 + 2]);
            v.weights.push_back(we[i * 4 + 3]);
        }

        if (bo.ptr()) {

            v.bones.push_back(bo[i * 4 + 0]);
            v.bones.push_back(bo[i * 4 + 1]);
            v.bones.push_back(bo[i * 4 + 2]);
            v.bones.push_back(bo[i * 4 + 3]);
        }

        vertices.write[i] = v;
    }

    PoolVector<int> indices;

    if (arrays[Mesh::ARRAY_INDEX].get_type() != VariantType::NIL) {

        indices = arrays[Mesh::ARRAY_INDEX];
    } else {
        //make code simpler
        indices.resize(vcount);
        PoolVector<int>::Write iw = indices.write();
        for (int i = 0; i < vcount; i++)
            iw[i] = i;
    }

    int icount = indices.size();
    PoolVector<int>::Read r = indices.read();

    Map<Point2i, int> edge_indices;

    for (int i = 0; i < icount; i += 3) {

        Vertex *v[3] = { &vertices.write[r[i + 0]], &vertices.write[r[i + 1]], &vertices.write[r[i + 2]] };

        int fidx = faces.size();
        Face face;

        for (int j = 0; j < 3; j++) {

            face.v[j] = r[i + j];

            Point2i edge(r[i + j], r[i + (j + 1) % 3]);
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

            edges.write[face.edges[j]].faces.push_back(fidx);
            v[j]->faces.push_back(fidx);
        }

        faces.push_back(face);
    }

    return OK;
}

Error MeshDataTool::commit_to_surface(const Ref<ArrayMesh> &p_mesh) {

    ERR_FAIL_COND_V(not p_mesh, ERR_INVALID_PARAMETER);
    Array arr;
    arr.resize(Mesh::ARRAY_MAX);

    int vcount = vertices.size();

    PoolVector<Vector3> v;
    PoolVector<Vector3> n;
    PoolVector<real_t> t;
    PoolVector<Vector2> u;
    PoolVector<Vector2> u2;
    PoolVector<Color> c;
    PoolVector<int> b;
    PoolVector<real_t> w;
    PoolVector<int> in;

    {

        v.resize(vcount);
        PoolVector<Vector3>::Write vr = v.write();

        PoolVector<Vector3>::Write nr;
        if (format & Mesh::ARRAY_FORMAT_NORMAL) {
            n.resize(vcount);
            nr = n.write();
        }

        PoolVector<real_t>::Write ta;
        if (format & Mesh::ARRAY_FORMAT_TANGENT) {
            t.resize(vcount * 4);
            ta = t.write();
        }

        PoolVector<Vector2>::Write uv;
        if (format & Mesh::ARRAY_FORMAT_TEX_UV) {
            u.resize(vcount);
            uv = u.write();
        }

        PoolVector<Vector2>::Write uv2;
        if (format & Mesh::ARRAY_FORMAT_TEX_UV2) {
            u2.resize(vcount);
            uv2 = u2.write();
        }

        PoolVector<Color>::Write col;
        if (format & Mesh::ARRAY_FORMAT_COLOR) {
            c.resize(vcount);
            col = c.write();
        }

        PoolVector<int>::Write bo;
        if (format & Mesh::ARRAY_FORMAT_BONES) {
            b.resize(vcount * 4);
            bo = b.write();
        }

        PoolVector<real_t>::Write we;
        if (format & Mesh::ARRAY_FORMAT_WEIGHTS) {
            w.resize(vcount * 4);
            we = w.write();
        }

        for (int i = 0; i < vcount; i++) {

            const Vertex &vtx = vertices[i];

            vr[i] = vtx.vertex;
            if (nr.ptr())
                nr[i] = vtx.normal;
            if (ta.ptr()) {
                ta[i * 4 + 0] = vtx.tangent.normal.x;
                ta[i * 4 + 1] = vtx.tangent.normal.y;
                ta[i * 4 + 2] = vtx.tangent.normal.z;
                ta[i * 4 + 3] = vtx.tangent.d;
            }
            if (uv.ptr())
                uv[i] = vtx.uv;
            if (uv2.ptr())
                uv2[i] = vtx.uv2;
            if (col.ptr())
                col[i] = vtx.color;

            if (we.ptr()) {

                we[i * 4 + 0] = vtx.weights[0];
                we[i * 4 + 1] = vtx.weights[1];
                we[i * 4 + 2] = vtx.weights[2];
                we[i * 4 + 3] = vtx.weights[3];
            }

            if (bo.ptr()) {

                bo[i * 4 + 0] = vtx.bones[0];
                bo[i * 4 + 1] = vtx.bones[1];
                bo[i * 4 + 2] = vtx.bones[2];
                bo[i * 4 + 3] = vtx.bones[3];
            }
        }

        int fc = faces.size();
        in.resize(fc * 3);
        PoolVector<int>::Write iw = in.write();
        for (int i = 0; i < fc; i++) {

            iw[i * 3 + 0] = faces[i].v[0];
            iw[i * 3 + 1] = faces[i].v[1];
            iw[i * 3 + 2] = faces[i].v[2];
        }
    }

    arr[Mesh::ARRAY_VERTEX] = v;
    arr[Mesh::ARRAY_INDEX] = in;
    if (n.size())
        arr[Mesh::ARRAY_NORMAL] = n;
    if (c.size())
        arr[Mesh::ARRAY_COLOR] = c;
    if (u.size())
        arr[Mesh::ARRAY_TEX_UV] = Variant(u);
    if (u2.size())
        arr[Mesh::ARRAY_TEX_UV2] = Variant(u2);
    if (t.size())
        arr[Mesh::ARRAY_TANGENT] = t;
    if (b.size())
        arr[Mesh::ARRAY_BONES] = b;
    if (w.size())
        arr[Mesh::ARRAY_WEIGHTS] = w;

    Ref<ArrayMesh> ncmesh = p_mesh;
    int sc = ncmesh->get_surface_count();
    ncmesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arr);
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
    vertices.write[p_idx].vertex = p_vertex;
}

Vector3 MeshDataTool::get_vertex_normal(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Vector3());
    return vertices[p_idx].normal;
}
void MeshDataTool::set_vertex_normal(int p_idx, const Vector3 &p_normal) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices.write[p_idx].normal = p_normal;
    format |= Mesh::ARRAY_FORMAT_NORMAL;
}

Plane MeshDataTool::get_vertex_tangent(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Plane());
    return vertices[p_idx].tangent;
}
void MeshDataTool::set_vertex_tangent(int p_idx, const Plane &p_tangent) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices.write[p_idx].tangent = p_tangent;
    format |= Mesh::ARRAY_FORMAT_TANGENT;
}

Vector2 MeshDataTool::get_vertex_uv(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Vector2());
    return vertices[p_idx].uv;
}
void MeshDataTool::set_vertex_uv(int p_idx, const Vector2 &p_uv) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices.write[p_idx].uv = p_uv;
    format |= Mesh::ARRAY_FORMAT_TEX_UV;
}

Vector2 MeshDataTool::get_vertex_uv2(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Vector2());
    return vertices[p_idx].uv2;
}
void MeshDataTool::set_vertex_uv2(int p_idx, const Vector2 &p_uv2) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices.write[p_idx].uv2 = p_uv2;
    format |= Mesh::ARRAY_FORMAT_TEX_UV2;
}

Color MeshDataTool::get_vertex_color(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Color());
    return vertices[p_idx].color;
}
void MeshDataTool::set_vertex_color(int p_idx, const Color &p_color) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices.write[p_idx].color = p_color;
    format |= Mesh::ARRAY_FORMAT_COLOR;
}

const PoolVector<int> &MeshDataTool::get_vertex_bones(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, vertices.size(), null_pvint_val);
    return vertices[p_idx].bones;
}
void MeshDataTool::set_vertex_bones(int p_idx, const PoolVector<int>& p_bones) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices.write[p_idx].bones = p_bones;
    format |= Mesh::ARRAY_FORMAT_BONES;
}

const PoolVector<float> &MeshDataTool::get_vertex_weights(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), null_pvfloat_val);
    return vertices[p_idx].weights;
}
void MeshDataTool::set_vertex_weights(int p_idx, const PoolVector<float> &p_weights) {
    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices.write[p_idx].weights = p_weights;
    format |= Mesh::ARRAY_FORMAT_WEIGHTS;
}

Variant MeshDataTool::get_vertex_meta(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), Variant());
    return vertices[p_idx].meta;
}

void MeshDataTool::set_vertex_meta(int p_idx, const Variant &p_meta) {

    ERR_FAIL_INDEX(p_idx, vertices.size());
    vertices.write[p_idx].meta = p_meta;
}

PoolVector<int> MeshDataTool::get_vertex_edges(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), PoolVector<int>());
    return vertices[p_idx].edges;
}
PoolVector<int> MeshDataTool::get_vertex_faces(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, vertices.size(), PoolVector<int>());
    return vertices[p_idx].faces;
}

int MeshDataTool::get_edge_vertex(int p_edge, int p_vertex) const {

    ERR_FAIL_INDEX_V(p_edge, edges.size(), -1);
    ERR_FAIL_INDEX_V(p_vertex, 2, -1);
    return edges[p_edge].vertex[p_vertex];
}
PoolVector<int> MeshDataTool::get_edge_faces(int p_edge) const {

    ERR_FAIL_INDEX_V(p_edge, edges.size(), PoolVector<int>());
    return edges[p_edge].faces;
}
Variant MeshDataTool::get_edge_meta(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, edges.size(), Variant());
    return edges[p_idx].meta;
}
void MeshDataTool::set_edge_meta(int p_idx, const Variant &p_meta) {

    ERR_FAIL_INDEX(p_idx, edges.size());
    edges.write[p_idx].meta = p_meta;
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
    faces.write[p_face].meta = p_meta;
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

    MethodBinder::bind_method(D_METHOD("clear"), &MeshDataTool::clear);
    MethodBinder::bind_method(D_METHOD("create_from_surface", {"mesh", "surface"}), &MeshDataTool::create_from_surface);
    MethodBinder::bind_method(D_METHOD("commit_to_surface", {"mesh"}), &MeshDataTool::commit_to_surface);

    MethodBinder::bind_method(D_METHOD("get_format"), &MeshDataTool::get_format);

    MethodBinder::bind_method(D_METHOD("get_vertex_count"), &MeshDataTool::get_vertex_count);
    MethodBinder::bind_method(D_METHOD("get_edge_count"), &MeshDataTool::get_edge_count);
    MethodBinder::bind_method(D_METHOD("get_face_count"), &MeshDataTool::get_face_count);

    MethodBinder::bind_method(D_METHOD("set_vertex", {"idx", "vertex"}), &MeshDataTool::set_vertex);
    MethodBinder::bind_method(D_METHOD("get_vertex", {"idx"}), &MeshDataTool::get_vertex);

    MethodBinder::bind_method(D_METHOD("set_vertex_normal", {"idx", "normal"}), &MeshDataTool::set_vertex_normal);
    MethodBinder::bind_method(D_METHOD("get_vertex_normal", {"idx"}), &MeshDataTool::get_vertex_normal);

    MethodBinder::bind_method(D_METHOD("set_vertex_tangent", {"idx", "tangent"}), &MeshDataTool::set_vertex_tangent);
    MethodBinder::bind_method(D_METHOD("get_vertex_tangent", {"idx"}), &MeshDataTool::get_vertex_tangent);

    MethodBinder::bind_method(D_METHOD("set_vertex_uv", {"idx", "uv"}), &MeshDataTool::set_vertex_uv);
    MethodBinder::bind_method(D_METHOD("get_vertex_uv", {"idx"}), &MeshDataTool::get_vertex_uv);

    MethodBinder::bind_method(D_METHOD("set_vertex_uv2", {"idx", "uv2"}), &MeshDataTool::set_vertex_uv2);
    MethodBinder::bind_method(D_METHOD("get_vertex_uv2", {"idx"}), &MeshDataTool::get_vertex_uv2);

    MethodBinder::bind_method(D_METHOD("set_vertex_color", {"idx", "color"}), &MeshDataTool::set_vertex_color);
    MethodBinder::bind_method(D_METHOD("get_vertex_color", {"idx"}), &MeshDataTool::get_vertex_color);

    MethodBinder::bind_method(D_METHOD("set_vertex_bones", {"idx", "bones"}), &MeshDataTool::set_vertex_bones);
    MethodBinder::bind_method(D_METHOD("get_vertex_bones", {"idx"}), &MeshDataTool::get_vertex_bones);

    MethodBinder::bind_method(D_METHOD("set_vertex_weights", {"idx", "weights"}), &MeshDataTool::set_vertex_weights);
    MethodBinder::bind_method(D_METHOD("get_vertex_weights", {"idx"}), &MeshDataTool::get_vertex_weights);

    MethodBinder::bind_method(D_METHOD("set_vertex_meta", {"idx", "meta"}), &MeshDataTool::set_vertex_meta);
    MethodBinder::bind_method(D_METHOD("get_vertex_meta", {"idx"}), &MeshDataTool::get_vertex_meta);

    MethodBinder::bind_method(D_METHOD("get_vertex_edges", {"idx"}), &MeshDataTool::get_vertex_edges);
    MethodBinder::bind_method(D_METHOD("get_vertex_faces", {"idx"}), &MeshDataTool::get_vertex_faces);

    MethodBinder::bind_method(D_METHOD("get_edge_vertex", {"idx", "vertex"}), &MeshDataTool::get_edge_vertex);
    MethodBinder::bind_method(D_METHOD("get_edge_faces", {"idx"}), &MeshDataTool::get_edge_faces);

    MethodBinder::bind_method(D_METHOD("set_edge_meta", {"idx", "meta"}), &MeshDataTool::set_edge_meta);
    MethodBinder::bind_method(D_METHOD("get_edge_meta", {"idx"}), &MeshDataTool::get_edge_meta);

    MethodBinder::bind_method(D_METHOD("get_face_vertex", {"idx", "vertex"}), &MeshDataTool::get_face_vertex);
    MethodBinder::bind_method(D_METHOD("get_face_edge", {"idx", "edge"}), &MeshDataTool::get_face_edge);

    MethodBinder::bind_method(D_METHOD("set_face_meta", {"idx", "meta"}), &MeshDataTool::set_face_meta);
    MethodBinder::bind_method(D_METHOD("get_face_meta", {"idx"}), &MeshDataTool::get_face_meta);

    MethodBinder::bind_method(D_METHOD("get_face_normal", {"idx"}), &MeshDataTool::get_face_normal);

    MethodBinder::bind_method(D_METHOD("set_material", {"material"}), &MeshDataTool::set_material);
    MethodBinder::bind_method(D_METHOD("get_material"), &MeshDataTool::get_material);
}

MeshDataTool::MeshDataTool() {

    clear();
}
