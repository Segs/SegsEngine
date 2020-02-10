/*************************************************************************/
/*  mesh.cpp                                                             */
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

#include "mesh.h"

#include "core/object_tooling.h"
#include "core/pair.h"
#include "core/map.h"
#include "scene/resources/concave_polygon_shape.h"
#include "scene/resources/convex_polygon_shape.h"
#include "scene/resources/material.h"
#include "core/print_string.h"
#include "servers/visual_server.h"
#include "surface_tool.h"
#include "core/method_bind.h"
#include "scene/resources/mesh_enum_casters.h"
#include <cstdlib>

IMPL_GDCLASS(Mesh)
IMPL_GDCLASS(ArrayMesh)
RES_BASE_EXTENSION_IMPL(ArrayMesh,"mesh")

Mesh::ConvexDecompositionFunc Mesh::convex_composition_function = nullptr;

Ref<TriangleMesh> Mesh::generate_triangle_mesh() const {

    if (triangle_mesh)
        return triangle_mesh;

    int facecount = 0;

    for (int i = 0; i < get_surface_count(); i++) {

        if (surface_get_primitive_type(i) != PRIMITIVE_TRIANGLES)
            continue;

        if (surface_get_format(i) & ARRAY_FORMAT_INDEX) {

            facecount += surface_get_array_index_len(i);
        } else {

            facecount += surface_get_array_len(i);
        }
    }

    if (facecount == 0 || (facecount % 3) != 0)
        return triangle_mesh;

    PODVector<Vector3> faces;
    faces.resize(facecount);

    int widx = 0;

    for (int i = 0; i < get_surface_count(); i++) {

        if (surface_get_primitive_type(i) != PRIMITIVE_TRIANGLES)
            continue;

        SurfaceArrays a = surface_get_arrays(i);
        ERR_FAIL_COND_V(a.empty(), Ref<TriangleMesh>());

        int vc = surface_get_array_len(i);
        auto vertices = a.positions3();

        if (surface_get_format(i) & ARRAY_FORMAT_INDEX) {

            int ic = surface_get_array_index_len(i);
            const PODVector<int> &indices = a.m_indices;

            for (int j = 0; j < ic; j++) {
                int index = indices[j];
                faces[widx++] = vertices[index];
            }

        } else {

            for (int j = 0; j < vc; j++)
                faces[widx++] = vertices[j];
        }
    }

    triangle_mesh = make_ref_counted<TriangleMesh>();
    triangle_mesh->create(faces);

    return triangle_mesh;
}

void Mesh::generate_debug_mesh_lines(PODVector<Vector3> &r_lines) {

    if (!debug_lines.empty()) {
        r_lines = debug_lines;
        return;
    }

    Ref<TriangleMesh> tm = generate_triangle_mesh();
    if (not tm)
        return;

    PoolVector<int> triangle_indices;
    tm->get_indices(&triangle_indices);
    const int triangles_num = tm->get_triangles().size();
    const PODVector<Vector3> &vertices = tm->get_vertices();

    debug_lines.resize(tm->get_triangles().size() * 6); // 3 lines x 2 points each line

    PoolVector<int>::Read ind_r = triangle_indices.read();
    for (int j = 0, x = 0, i = 0; i < triangles_num; j += 6, x += 3, ++i) {
        // Triangle line 1
        debug_lines[j + 0] = vertices[ind_r[x + 0]];
        debug_lines[j + 1] = vertices[ind_r[x + 1]];

        // Triangle line 2
        debug_lines[j + 2] = vertices[ind_r[x + 1]];
        debug_lines[j + 3] = vertices[ind_r[x + 2]];

        // Triangle line 3
        debug_lines[j + 4] = vertices[ind_r[x + 2]];
        debug_lines[j + 5] = vertices[ind_r[x + 0]];
    }

    r_lines = debug_lines;
}
void Mesh::generate_debug_mesh_indices(PODVector<Vector3> &r_points) {
    Ref<TriangleMesh> tm = generate_triangle_mesh();
    if (not tm)
        return;

    r_points = tm->get_vertices();
}

bool Mesh::surface_is_softbody_friendly(int p_idx) const {
    const uint32_t surface_format = surface_get_format(p_idx);
    return (surface_format & Mesh::ARRAY_FLAG_USE_DYNAMIC_UPDATE && (!(surface_format & Mesh::ARRAY_COMPRESS_VERTEX)) && (!(surface_format & Mesh::ARRAY_COMPRESS_NORMAL)));
}

PODVector<Face3> Mesh::get_faces() const {

    Ref<TriangleMesh> tm = generate_triangle_mesh();
    if (tm)
        return tm->get_faces();
    return PODVector<Face3>();
    /*
    for (int i=0;i<surfaces.size();i++) {

        if (VisualServer::get_singleton()->mesh_surface_get_primitive_type( mesh, i ) != VisualServer::PRIMITIVE_TRIANGLES )
            continue;

        PoolVector<int> indices;
        PoolVector<Vector3> vertices;

        vertices=VisualServer::get_singleton()->mesh_surface_get_array(mesh, i,VisualServer::ARRAY_VERTEX);

        int len=VisualServer::get_singleton()->mesh_surface_get_array_index_len(mesh, i);
        bool has_indices;

        if (len>0) {

            indices=VisualServer::get_singleton()->mesh_surface_get_array(mesh, i,VisualServer::ARRAY_INDEX);
            has_indices=true;

        } else {

            len=vertices.size();
            has_indices=false;
        }

        if (len<=0)
            continue;

        PoolVector<int>::Read indicesr = indices.read();
        const int *indicesptr = indicesr.ptr();

        PoolVector<Vector3>::Read verticesr = vertices.read();
        const Vector3 *verticesptr = verticesr.ptr();

        int old_faces=faces.size();
        int new_faces=old_faces+(len/3);

        faces.resize(new_faces);

        PoolVector<Face3>::Write facesw = faces.write();
        Face3 *facesptr=facesw.ptr();


        for (int i=0;i<len/3;i++) {

            Face3 face;

            for (int j=0;j<3;j++) {

                int idx=i*3+j;
                face.vertex[j] = has_indices ? verticesptr[ indicesptr[ idx ] ] : verticesptr[idx];
            }

            facesptr[i+old_faces]=face;
        }

    }
*/
}

Ref<Shape> Mesh::create_convex_shape() const {

    PODVector<Vector3> vertices;
    //TODO: SEGS: inefficient usage of surface arrays, when only positions are used.
    for (int i = 0; i < get_surface_count(); i++) {

        SurfaceArrays a = surface_get_arrays(i);
        ERR_FAIL_COND_V(a.empty(), Ref<ConvexPolygonShape>());
        auto vals=a.positions3();
        vertices.insert(vertices.end(),vals.begin(),vals.end());
    }

    Ref<ConvexPolygonShape> shape(make_ref_counted<ConvexPolygonShape>());
    shape->set_points(eastl::move(vertices));
    return shape;
}

Ref<Shape> Mesh::create_trimesh_shape() const {

    PODVector<Face3> faces = get_faces();
    if (faces.empty())
        return Ref<Shape>();

    PoolVector<Vector3> face_points;
    face_points.resize(faces.size() * 3);

    for (int i = 0; i < face_points.size(); i++) {

        Face3 f = faces[i/3];
        face_points.set(i, f.vertex[i % 3]);
    }

    Ref<ConcavePolygonShape> shape(make_ref_counted<ConcavePolygonShape>());
    shape->set_faces(face_points);
    return shape;
}
template<typename T>
static void collectBuffers(T &dst,const T&src,int expected_count) {
    if (!dst.empty()) {
        if (src.empty())
            dst.clear();
        else {
            ERR_FAIL_COND(expected_count!=src.size());
            dst.insert(dst.end(),src.begin(),src.end());
        }
    }
}
Ref<Mesh> Mesh::create_outline(float p_margin) const {

    SurfaceArrays arrays;
    int index_accum = 0;
    for (int i = 0; i < get_surface_count(); i++) {

        if (surface_get_primitive_type(i) != PRIMITIVE_TRIANGLES)
            continue;

        SurfaceArrays a = surface_get_arrays(i);
        ERR_FAIL_COND_V(a.empty(), Ref<ArrayMesh>());

        if (i == 0) {
            arrays = eastl::move(a);
            auto v = arrays.positions3();
            index_accum += v.size();
        } else {
            int vcount = 0;
            ERR_CONTINUE(arrays.m_vertices_2d!=a.m_vertices_2d);
            PODVector<float> &dst = arrays.m_position_data;
            const PODVector<float> &src = a.m_position_data;
            if(!dst.empty()) {
                if(src.empty())
                    arrays.m_position_data = {};
                else {
                    dst.push_back(src);
                }
                vcount = src.size();
            }
            collectBuffers(arrays.m_normals,a.m_normals,vcount);

            collectBuffers(arrays.m_tangents, a.m_tangents, vcount*4); // tangents are 4 per vertex
            collectBuffers(arrays.m_bones, a.m_bones, vcount*4); // 4 per vertex
            collectBuffers(arrays.m_weights, a.m_weights, vcount*4); // 4 per vertex

            collectBuffers(arrays.m_colors, a.m_colors, vcount);
            collectBuffers(arrays.m_uv_1, a.m_uv_1, vcount);
            collectBuffers(arrays.m_uv_2, a.m_uv_2, vcount);

            PODVector<int> &dst_idx = arrays.m_indices;
            PODVector<int> src_idx = a.m_indices;
            if (!dst_idx.empty()) {
                if (src_idx.empty())
                    dst_idx.clear();
                else {
                    int ss = src.size();
                    for (int k = 0; k < ss; k++) {
                        src_idx[k] += index_accum;
                    }
                    dst_idx.push_back(src_idx);
                    index_accum += vcount;
                }
            }
        }
    }

    {
        PODVector<int> &indices = arrays.m_indices;
        bool has_indices = false;
        Span<Vector3> vertices = arrays.writeable_positions3();
        int vc = vertices.size();
        ERR_FAIL_COND_V(!vc, Ref<ArrayMesh>());

        if (indices.size()) {
            ERR_FAIL_COND_V(indices.size() % 3 != 0, Ref<ArrayMesh>());
            vc = indices.size();
            has_indices = true;
        }

        Map<Vector3, Vector3> normal_accum;

        //fill normals with triangle normals
        for (int i = 0; i < vc; i += 3) {

            Vector3 t[3];

            if (has_indices) {
                t[0] = vertices[indices[i + 0]];
                t[1] = vertices[indices[i + 1]];
                t[2] = vertices[indices[i + 2]];
            } else {
                t[0] = vertices[i + 0];
                t[1] = vertices[i + 1];
                t[2] = vertices[i + 2];
            }

            Vector3 n = Plane(t[0], t[1], t[2]).normal;

            for (int j = 0; j < 3; j++) {

                Map<Vector3, Vector3>::iterator E = normal_accum.find(t[j]);
                if (E==normal_accum.end()) {
                    normal_accum[t[j]] = n;
                } else {
                    float d = n.dot(E->second);
                    if (d < 1.0f)
                        E->second += n * (1.0f - d);
                    //E->second+=n;
                }
            }
        }

        //normalize

        for (eastl::pair<const Vector3,Vector3> &E : normal_accum) {
            E.second.normalize();
        }

        //displace normals
        int vc2 = vertices.size();

        for (int i = 0; i < vc2; i++) {

            Vector3 t = vertices[i];

            Map<Vector3, Vector3>::iterator E = normal_accum.find(t);
            ERR_CONTINUE(E==normal_accum.end());

            t += E->second * p_margin;
            vertices[i] = t;
        }

        if (!has_indices) {

            PODVector<int> new_indices;
            new_indices.resize(3*vertices.size());

            for (int j = 0; j < vc2; j += 3) {

                new_indices[j] = j;
                new_indices[j + 1] = j + 2;
                new_indices[j + 2] = j + 1;
            }
            arrays.m_indices = eastl::move(new_indices);

        } else {

            for (int j = 0; j < vc; j += 3) {

                SWAP(indices[j + 1], indices[j + 2]);
            }
        }
    }

    Ref<ArrayMesh> newmesh(make_ref_counted<ArrayMesh>());
    newmesh->add_surface_from_arrays(PRIMITIVE_TRIANGLES, eastl::move(arrays));
    return newmesh;
}

void Mesh::set_lightmap_size_hint(const Vector2 &p_size) {
    lightmap_size_hint = p_size;
}

Size2 Mesh::get_lightmap_size_hint() const {
    return lightmap_size_hint;
}

void Mesh::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_lightmap_size_hint", {"size"}), &Mesh::set_lightmap_size_hint);
    MethodBinder::bind_method(D_METHOD("get_lightmap_size_hint"), &Mesh::get_lightmap_size_hint);
    MethodBinder::bind_method(D_METHOD("get_aabb"), &Mesh::get_aabb);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "lightmap_size_hint"), "set_lightmap_size_hint", "get_lightmap_size_hint");

    MethodBinder::bind_method(D_METHOD("get_surface_count"), &Mesh::get_surface_count);
    MethodBinder::bind_method(D_METHOD("surface_get_arrays", {"surf_idx"}), &Mesh::_surface_get_arrays);
    MethodBinder::bind_method(D_METHOD("surface_get_blend_shape_arrays", {"surf_idx"}), &Mesh::_surface_get_blend_shape_arrays);
    MethodBinder::bind_method(D_METHOD("surface_set_material", {"surf_idx", "material"}), &Mesh::surface_set_material);
    MethodBinder::bind_method(D_METHOD("surface_get_material", {"surf_idx"}), &Mesh::surface_get_material);

    BIND_ENUM_CONSTANT(PRIMITIVE_POINTS)
    BIND_ENUM_CONSTANT(PRIMITIVE_LINES)
    BIND_ENUM_CONSTANT(PRIMITIVE_LINE_STRIP)
    BIND_ENUM_CONSTANT(PRIMITIVE_LINE_LOOP)
    BIND_ENUM_CONSTANT(PRIMITIVE_TRIANGLES)
    BIND_ENUM_CONSTANT(PRIMITIVE_TRIANGLE_STRIP)
    BIND_ENUM_CONSTANT(PRIMITIVE_TRIANGLE_FAN)

    BIND_ENUM_CONSTANT(BLEND_SHAPE_MODE_NORMALIZED)
    BIND_ENUM_CONSTANT(BLEND_SHAPE_MODE_RELATIVE)

    BIND_ENUM_CONSTANT(ARRAY_FORMAT_VERTEX)
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_NORMAL)
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_TANGENT)
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_COLOR)
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_TEX_UV)
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_TEX_UV2)
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_BONES)
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_WEIGHTS)
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_INDEX)

    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_BASE)
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_VERTEX)
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_NORMAL)
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_TANGENT)
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_COLOR)
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_TEX_UV)
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_TEX_UV2)
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_BONES)
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_WEIGHTS)
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_INDEX)

    BIND_ENUM_CONSTANT(ARRAY_FLAG_USE_2D_VERTICES)
    BIND_ENUM_CONSTANT(ARRAY_FLAG_USE_16_BIT_BONES)

    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_DEFAULT)

    BIND_ENUM_CONSTANT(ARRAY_VERTEX)
    BIND_ENUM_CONSTANT(ARRAY_NORMAL)
    BIND_ENUM_CONSTANT(ARRAY_TANGENT)
    BIND_ENUM_CONSTANT(ARRAY_COLOR)
    BIND_ENUM_CONSTANT(ARRAY_TEX_UV)
    BIND_ENUM_CONSTANT(ARRAY_TEX_UV2)
    BIND_ENUM_CONSTANT(ARRAY_BONES)
    BIND_ENUM_CONSTANT(ARRAY_WEIGHTS)
    BIND_ENUM_CONSTANT(ARRAY_INDEX)
    BIND_ENUM_CONSTANT(ARRAY_MAX)

    BIND_CONSTANT(NO_INDEX_ARRAY)
    BIND_CONSTANT(ARRAY_WEIGHTS_SIZE)

}

Array Mesh::_surface_get_arrays(int p_surface) const {
    return Array(this->surface_get_arrays(p_surface));
}

Array Mesh::_surface_get_blend_shape_arrays(int p_surface) const
{
    Array res;
    auto blends(eastl::move(surface_get_blend_shape_arrays(p_surface)));
    res.resize(blends.size());
    int idx=0;
    for(const auto &shp : blends)
        res[idx++] = (Array)shp;
    return res;
}

void Mesh::clear_cache() const {
    triangle_mesh.unref();
    debug_lines.clear();
}

PODVector<Ref<Shape>> Mesh::convex_decompose() const {

    ERR_FAIL_COND_V(!convex_composition_function, {});

    PODVector<Face3> faces = get_faces();
    PODVector<PODVector<Face3> > decomposed = convex_composition_function(faces);

    PODVector<Ref<Shape> > ret;
    ret.reserve(decomposed.size());
    for (int i = 0; i < decomposed.size(); i++) {
        Set<Vector3> points;
        for (int j = 0; j < decomposed[i].size(); j++) {
            points.insert(decomposed[i][j].vertex[0]);
            points.insert(decomposed[i][j].vertex[1]);
            points.insert(decomposed[i][j].vertex[2]);
        }

        PODVector<Vector3> convex_points;
        convex_points.reserve(points.size());
        for (const Vector3 &E : points) {
            convex_points.emplace_back(E);
        }

        Ref<ConvexPolygonShape> shape(make_ref_counted<ConvexPolygonShape>());
        shape->set_points(eastl::move(convex_points));
        ret.emplace_back(eastl::move(shape));
    }

    return ret;
}

Mesh::Mesh() {
}

bool ArrayMesh::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "blend_shape/names") {

        PoolVector<String> sk(p_value.as<PoolVector<String>>());
        int sz = sk.size();
        PoolVector<String>::Read r = sk.read();
        for (int i = 0; i < sz; i++)
            add_blend_shape(StringName(r[i]));
        return true;
    }

    if (p_name == "blend_shape/mode") {

        set_blend_shape_mode(BlendShapeMode(int(p_value)));
        return true;
    }

    if (StringUtils::begins_with(p_name,"surface_")) {

        int sl = StringUtils::find(p_name,"/");
        if (sl == -1)
            return false;
        int idx = StringUtils::to_int(StringUtils::substr(p_name,8, sl - 8)) - 1;
        StringName what( StringUtils::get_slice(p_name,'/', 1));
        if (what == "material")
            surface_set_material(idx, refFromRefPtr<Material>(p_value));
        else if (what == "name")
            surface_set_name(idx, p_value.as<String>());
        return true;
    }

    if (!StringUtils::begins_with(p_name,"surfaces"))
        return false;

    int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
    StringName what(StringUtils::get_slice(p_name,'/', 2));

    if (idx == surfaces.size()) {

        //create
        Dictionary d = p_value;
        ERR_FAIL_COND_V(!d.has("primitive"), false);

        if (d.has("arrays")) {
            //old format
            ERR_FAIL_COND_V(!d.has("morph_arrays"), false);
            PODVector<SurfaceArrays> morph_arrays;
            Array ma=d["morph_arrays"];
            morph_arrays.reserve(ma.size());
            for(int i=0; i<ma.size(); ++i)
                morph_arrays.emplace_back(SurfaceArrays::fromArray(ma[i].as<Array>()));
            add_surface_from_arrays(PrimitiveType(int(d["primitive"])), SurfaceArrays::fromArray(d["arrays"]), eastl::move(morph_arrays));

        } else if (d.has("array_data")) {

            PoolVector<uint8_t> array_data = d["array_data"];
            PoolVector<uint8_t> array_index_data;
            if (d.has("array_index_data"))
                array_index_data = d["array_index_data"];

            ERR_FAIL_COND_V(!d.has("format"), false);
            uint32_t format = d["format"];

            uint32_t primitive = d["primitive"];

            ERR_FAIL_COND_V(!d.has("vertex_count"), false);
            int vertex_count = d["vertex_count"];

            int index_count = 0;
            if (d.has("index_count"))
                index_count = d["index_count"];

            PODVector<PoolVector<uint8_t> > blend_shapes;

            if (d.has("blend_shape_data")) {
                Array blend_shape_data = d["blend_shape_data"];
                blend_shapes.reserve(blend_shape_data.size());
                for (int i = 0; i < blend_shape_data.size(); i++) {
                    PoolVector<uint8_t> shape = blend_shape_data[i];
                    blend_shapes.emplace_back(eastl::move(shape));
                }
            }

            ERR_FAIL_COND_V(!d.has("aabb"), false);
            AABB aabb = d["aabb"];

            PODVector<AABB> bone_aabb;
            if (d.has("skeleton_aabb")) {
                Array baabb = d["skeleton_aabb"];
                bone_aabb.reserve(baabb.size());

                for (int i = 0; i < baabb.size(); i++) {
                    bone_aabb.emplace_back(baabb[i]);
                }
            }

            add_surface(format, PrimitiveType(primitive), array_data, vertex_count, array_index_data, index_count, aabb, blend_shapes, bone_aabb);
        } else {
            ERR_FAIL_V(false);
        }

        if (d.has("material")) {

            surface_set_material(idx, refFromRefPtr<Material>(d["material"]));
        }
        if (d.has("name")) {
            surface_set_name(idx, d["name"].as<String>());
        }

        return true;
    }

    return false;
}

bool ArrayMesh::_get(const StringName &p_name, Variant &r_ret) const {

    if (_is_generated())
        return false;

    if (p_name == "blend_shape/names") {

        PoolVector<String> sk;
        for (int i = 0; i < blend_shapes.size(); i++)
            sk.push_back(blend_shapes[i].asCString());
        r_ret = sk;
        return true;
    } else if (p_name == "blend_shape/mode") {

        r_ret = get_blend_shape_mode();
        return true;
    } else if (StringUtils::begins_with(p_name,"surface_")) {

        int sl = StringUtils::find(p_name,"/");
        if (sl == -1)
            return false;
        int idx = StringUtils::to_int(StringUtils::substr(p_name,8, sl - 8)) - 1;
        StringName what(StringUtils::get_slice(p_name,'/', 1));
        if (what == "material")
            r_ret = surface_get_material(idx);
        else if (what == "name")
            r_ret = surface_get_name(idx);
        return true;
    } else if (!StringUtils::begins_with(p_name,"surfaces"))
        return false;

    int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
    ERR_FAIL_INDEX_V(idx, surfaces.size(), false);

    Dictionary d;

    d["array_data"] = VisualServer::get_singleton()->mesh_surface_get_array(mesh, idx);
    d["vertex_count"] = VisualServer::get_singleton()->mesh_surface_get_array_len(mesh, idx);
    d["array_index_data"] = VisualServer::get_singleton()->mesh_surface_get_index_array(mesh, idx);
    d["index_count"] = VisualServer::get_singleton()->mesh_surface_get_array_index_len(mesh, idx);
    d["primitive"] = VisualServer::get_singleton()->mesh_surface_get_primitive_type(mesh, idx);
    d["format"] = VisualServer::get_singleton()->mesh_surface_get_format(mesh, idx);
    d["aabb"] = VisualServer::get_singleton()->mesh_surface_get_aabb(mesh, idx);

    const PODVector<AABB> &skel_aabb = VisualServer::get_singleton()->mesh_surface_get_skeleton_aabb(mesh, idx);
    Array arr;
    arr.resize(skel_aabb.size());
    for (int i = 0; i < skel_aabb.size(); i++) {
        arr[i] = skel_aabb[i];
    }
    d["skeleton_aabb"] = arr;

    const PODVector<PODVector<uint8_t> > &blend_shape_data = VisualServer::get_singleton()->mesh_surface_get_blend_shapes(mesh, idx);

    Array md;
    for (int i = 0; i < blend_shape_data.size(); i++) {
        md.push_back(blend_shape_data[i]);
    }
    d["blend_shape_data"] = eastl::move(md);

    Ref<Material> m = surface_get_material(idx);
    if (m)
        d["material"] = m;

    String n = surface_get_name(idx);
    if (!n.empty())
        d["name"] = n;

    r_ret = d;

    return true;
}

void ArrayMesh::_get_property_list(PODVector<PropertyInfo> *p_list) const {

    if (_is_generated())
        return;

    if (!blend_shapes.empty()) {
        p_list->push_back(PropertyInfo(VariantType::POOL_STRING_ARRAY, "blend_shape/names", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
        p_list->push_back(PropertyInfo(VariantType::INT, "blend_shape/mode", PropertyHint::Enum, "Normalized,Relative"));
    }

    for (int i = 0; i < surfaces.size(); i++) {

        p_list->push_back(PropertyInfo(VariantType::DICTIONARY, StringName("surfaces/" + itos(i)), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
        p_list->push_back(PropertyInfo(VariantType::STRING, StringName("surface_" + itos(i + 1) + "/name"), PropertyHint::None, "", PROPERTY_USAGE_EDITOR));
        if (surfaces[i].is_2d) {
            p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName("surface_" + itos(i + 1) + "/material"), PropertyHint::ResourceType, "ShaderMaterial,CanvasItemMaterial", PROPERTY_USAGE_EDITOR));
        } else {
            p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName("surface_" + itos(i + 1) + "/material"), PropertyHint::ResourceType, "ShaderMaterial,SpatialMaterial", PROPERTY_USAGE_EDITOR));
        }
    }
}

void ArrayMesh::_recompute_aabb() {

    // regenerate AABB
    aabb = AABB();

    for (int i = 0; i < surfaces.size(); i++) {

        if (i == 0)
            aabb = surfaces[i].aabb;
        else
            aabb.merge_with(surfaces[i].aabb);
    }
}

void ArrayMesh::add_surface(uint32_t p_format, PrimitiveType p_primitive, const PODVector<uint8_t> &p_array, int p_vertex_count, const PODVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const PODVector<PODVector<uint8_t> > &p_blend_shapes, const PODVector<AABB> &p_bone_aabbs) {

    Surface s;
    s.aabb = p_aabb;
    s.is_2d = p_format & ARRAY_FLAG_USE_2D_VERTICES;
    surfaces.emplace_back(eastl::move(s));
    _recompute_aabb();

    VisualServer::get_singleton()->mesh_add_surface(mesh, p_format, (VS::PrimitiveType)p_primitive, p_array, p_vertex_count, p_index_array, p_index_count, p_aabb, p_blend_shapes, p_bone_aabbs);
}
void ArrayMesh::_add_surface_from_arrays(PrimitiveType p_primitive, const Array &p_arrays,
    const Array &p_blend_shapes, uint32_t p_flags) {
    PODVector<SurfaceArrays> inp;
    inp.reserve(p_blend_shapes.size());
    for(int i=0; i<p_blend_shapes.size(); ++i) {
        inp.emplace_back(SurfaceArrays::fromArray(p_blend_shapes[i].as<Array>()));
    }
    add_surface_from_arrays(p_primitive,SurfaceArrays::fromArray(p_arrays),eastl::move(inp),p_flags);
}

void ArrayMesh::add_surface_from_arrays(PrimitiveType p_primitive, SurfaceArrays &&p_arrays, PODVector<SurfaceArrays> &&p_blend_shapes, uint32_t p_flags) {

    Surface s;
    // Update AABB
    {
        ERR_FAIL_COND(p_arrays.empty());
        // check AABB
        AABB aabb;
        if(p_arrays.m_vertices_2d) {
            Span<const Vector2> vertices(p_arrays.positions2());
            int len = vertices.size();
            for (int i = 0; i < len; i++) {

                if (i == 0)
                    aabb.position = Vector3(vertices[i].x,vertices[i].y,0);
                else
                    aabb.expand_to(Vector3(vertices[i].x,vertices[i].y,0));
            }

        }
        else {
            Span<const Vector3> vertices = p_arrays.positions3();
            int len = vertices.size();
            for (int i = 0; i < len; i++) {

                if (i == 0)
                    aabb.position = vertices[i];
                else
                    aabb.expand_to(vertices[i]);
            }

        }


        s.aabb = aabb;
        s.is_2d = p_arrays.m_vertices_2d;
        surfaces.push_back(s);

        _recompute_aabb();
    }
    VisualServer::get_singleton()->mesh_add_surface_from_arrays(mesh, (VS::PrimitiveType)p_primitive, eastl::move(p_arrays), eastl::move(p_blend_shapes), p_flags);



    clear_cache();
    Object_change_notify(this);
    emit_changed();
}
SurfaceArrays ArrayMesh::surface_get_arrays(int p_surface) const {

    ERR_FAIL_INDEX_V(p_surface, surfaces.size(), SurfaceArrays());
    return VisualServer::get_singleton()->mesh_surface_get_arrays(mesh, p_surface);
}
PODVector<SurfaceArrays> ArrayMesh::surface_get_blend_shape_arrays(int p_surface) const {

    ERR_FAIL_INDEX_V(p_surface, surfaces.size(), PODVector<SurfaceArrays>());
    return VisualServer::get_singleton()->mesh_surface_get_blend_shape_arrays(mesh, p_surface);
}

int ArrayMesh::get_surface_count() const {

    return surfaces.size();
}

void ArrayMesh::add_blend_shape(const StringName &p_name) {

    ERR_FAIL_COND_MSG(surfaces.size(), "Can't add a shape key count if surfaces are already created.");

    StringName name = p_name;

    if (blend_shapes.contains(name)) {

        int count = 2;
        do {

            name = p_name + " " + itos(count);
            count++;
        } while (blend_shapes.contains(name));
    }

    blend_shapes.push_back(name);
    VisualServer::get_singleton()->mesh_set_blend_shape_count(mesh, blend_shapes.size());
}

int ArrayMesh::get_blend_shape_count() const {

    return blend_shapes.size();
}
StringName ArrayMesh::get_blend_shape_name(int p_index) const {
    ERR_FAIL_INDEX_V(p_index, blend_shapes.size(), StringName());
    return blend_shapes[p_index];
}
void ArrayMesh::clear_blend_shapes() {

    ERR_FAIL_COND_MSG(surfaces.size(), "Can't set shape key count if surfaces are already created.");

    blend_shapes.clear();
}

void ArrayMesh::set_blend_shape_mode(BlendShapeMode p_mode) {

    blend_shape_mode = p_mode;
    VisualServer::get_singleton()->mesh_set_blend_shape_mode(mesh, (VS::BlendShapeMode)p_mode);
}

ArrayMesh::BlendShapeMode ArrayMesh::get_blend_shape_mode() const {

    return blend_shape_mode;
}

void ArrayMesh::surface_remove(int p_idx) {

    ERR_FAIL_INDEX(p_idx, surfaces.size());
    VisualServer::get_singleton()->mesh_remove_surface(mesh, p_idx);
    surfaces.erase_at(p_idx);

    clear_cache();
    _recompute_aabb();
    Object_change_notify(this);
    emit_changed();
}

int ArrayMesh::surface_get_array_len(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), -1);
    return VisualServer::get_singleton()->mesh_surface_get_array_len(mesh, p_idx);
}

int ArrayMesh::surface_get_array_index_len(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), -1);
    return VisualServer::get_singleton()->mesh_surface_get_array_index_len(mesh, p_idx);
}

uint32_t ArrayMesh::surface_get_format(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), 0);
    return VisualServer::get_singleton()->mesh_surface_get_format(mesh, p_idx);
}

ArrayMesh::PrimitiveType ArrayMesh::surface_get_primitive_type(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), PRIMITIVE_LINES);
    return (PrimitiveType)VisualServer::get_singleton()->mesh_surface_get_primitive_type(mesh, p_idx);
}

void ArrayMesh::surface_set_material(int p_idx, const Ref<Material> &p_material) {

    ERR_FAIL_INDEX(p_idx, surfaces.size());
    if (surfaces[p_idx].material == p_material)
        return;
    surfaces[p_idx].material = p_material;
    VisualServer::get_singleton()->mesh_surface_set_material(mesh, p_idx, not p_material ? RID() : p_material->get_rid());

    Object_change_notify(this,"material");
    emit_changed();
}

int ArrayMesh::surface_find_by_name(const String &p_name) const {
    for (int i = 0; i < surfaces.size(); i++) {
        if (surfaces[i].name == p_name) {
            return i;
        }
    }

    return -1;
}

void ArrayMesh::surface_set_name(int p_idx, se_string_view p_name) {

    ERR_FAIL_INDEX(p_idx, surfaces.size());

    surfaces[p_idx].name = p_name;
    emit_changed();
}

String ArrayMesh::surface_get_name(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), String());
    return surfaces[p_idx].name;
}

void ArrayMesh::surface_update_region(int p_surface, int p_offset, const PODVector<uint8_t> &p_data) {

    ERR_FAIL_INDEX(p_surface, surfaces.size());
    VisualServer::get_singleton()->mesh_surface_update_region(mesh, p_surface, p_offset, p_data);
    emit_changed();
}

void ArrayMesh::surface_set_custom_aabb(int p_idx, const AABB &p_aabb) {

    ERR_FAIL_INDEX(p_idx, surfaces.size());
    surfaces[p_idx].aabb = p_aabb;
    // set custom aabb too?
    emit_changed();
}

Ref<Material> ArrayMesh::surface_get_material(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), Ref<Material>());
    return surfaces[p_idx].material;
}

void ArrayMesh::add_surface_from_mesh_data(Geometry::MeshData &&p_mesh_data) {

    AABB aabb;
    for (int i = 0; i < p_mesh_data.vertices.size(); i++) {

        if (i == 0)
            aabb.position = p_mesh_data.vertices[i];
        else
            aabb.expand_to(p_mesh_data.vertices[i]);
    }
    VisualServer::get_singleton()->mesh_add_surface_from_mesh_data(mesh, eastl::move(p_mesh_data));

    Surface s;
    s.aabb = aabb;
    if (surfaces.empty())
        aabb = s.aabb;
    else
        aabb.merge_with(s.aabb);

    clear_cache();

    surfaces.emplace_back(eastl::move(s));
    Object_change_notify(this);

    emit_changed();
}

RID ArrayMesh::get_rid() const {

    return mesh;
}
AABB ArrayMesh::get_aabb() const {

    return aabb;
}

void ArrayMesh::set_custom_aabb(const AABB &p_custom) {

    custom_aabb = p_custom;
    VisualServer::get_singleton()->mesh_set_custom_aabb(mesh, custom_aabb);
    emit_changed();
}

AABB ArrayMesh::get_custom_aabb() const {

    return custom_aabb;
}

void ArrayMesh::regen_normalmaps() {

    PODVector<Ref<SurfaceTool> > surfs;
    surfs.reserve(get_surface_count());
    for (int i = 0; i < get_surface_count(); i++) {

        Ref<SurfaceTool> st(make_ref_counted<SurfaceTool>());
        st->create_from(Ref<ArrayMesh>(this), i);
        surfs.emplace_back(st);
    }

    while (get_surface_count()) {
        surface_remove(0);
    }

    for (int i = 0; i < surfs.size(); i++) {

        surfs[i]->generate_tangents();
        surfs[i]->commit(Ref<ArrayMesh>(this));
    }
}

//dirty hack
bool (*array_mesh_lightmap_unwrap_callback)(float p_texel_size, const float *p_vertices, const float *p_normals, int p_vertex_count, const int *p_indices, const int *p_face_materials, int p_index_count, float **r_uv, int **r_vertex, int *r_vertex_count, int **r_index, int *r_index_count, int *r_size_hint_x, int *r_size_hint_y) = nullptr;

struct ArrayMeshLightmapSurface {

    Ref<Material> material;
    PODVector<SurfaceTool::Vertex> vertices;
    Mesh::PrimitiveType primitive;
    uint32_t format;
};

Error ArrayMesh::lightmap_unwrap(const Transform &p_base_transform, float p_texel_size) {

    ERR_FAIL_COND_V(!array_mesh_lightmap_unwrap_callback, ERR_UNCONFIGURED);
    ERR_FAIL_COND_V_MSG(!blend_shapes.empty(), ERR_UNAVAILABLE, "Can't unwrap mesh with blend shapes.");

    PODVector<float> vertices;
    PODVector<float> normals;
    PODVector<int> indices;
    PODVector<int> face_materials;
    PODVector<float> uv;
    PODVector<Pair<int, int> > uv_index;

    PODVector<ArrayMeshLightmapSurface> surfaces;
    for (int i = 0; i < get_surface_count(); i++) {
        ArrayMeshLightmapSurface s;
        s.primitive = surface_get_primitive_type(i);

        ERR_FAIL_COND_V_MSG(s.primitive != Mesh::PRIMITIVE_TRIANGLES, ERR_UNAVAILABLE, "Only triangles are supported for lightmap unwrap.");
        s.format = surface_get_format(i);
        ERR_FAIL_COND_V_MSG(!(s.format & ARRAY_FORMAT_NORMAL), ERR_UNAVAILABLE, "Normals are required for lightmap unwrap.");

        SurfaceArrays arrays = surface_get_arrays(i);
        s.material = surface_get_material(i);
        s.vertices = eastl::move(SurfaceTool::create_vertex_array_from_triangle_arrays(arrays));

        Span<const Vector3> rvertices = arrays.positions3();
        size_t vc = rvertices.size();

        const PODVector<Vector3> &rnormals = arrays.m_normals;

        size_t vertex_ofs = vertices.size() / 3;

        vertices.resize((vertex_ofs + vc) * 3);
        normals.resize((vertex_ofs + vc) * 3);
        uv_index.resize(vertex_ofs + vc);

        for (size_t j = 0; j < vc; j++) {

            Vector3 v = p_base_transform.xform(rvertices[j]);
            Vector3 n = p_base_transform.basis.xform(rnormals[j]).normalized();

            vertices[(j + vertex_ofs) * 3 + 0] = v.x;
            vertices[(j + vertex_ofs) * 3 + 1] = v.y;
            vertices[(j + vertex_ofs) * 3 + 2] = v.z;
            normals[(j + vertex_ofs) * 3 + 0] = n.x;
            normals[(j + vertex_ofs) * 3 + 1] = n.y;
            normals[(j + vertex_ofs) * 3 + 2] = n.z;
            uv_index[j + vertex_ofs] = Pair<int, int>(i, j);
        }

        const PODVector<int> &rindices = arrays.m_indices;
        size_t ic = rindices.size();

        if (ic == 0) {

            for (size_t j = 0; j < vc / 3; j++) {
                if (Face3(rvertices[j * 3 + 0], rvertices[j * 3 + 1], rvertices[j * 3 + 2]).is_degenerate())
                    continue;

                indices.push_back(vertex_ofs + j * 3 + 0);
                indices.push_back(vertex_ofs + j * 3 + 1);
                indices.push_back(vertex_ofs + j * 3 + 2);
                face_materials.push_back(i);
            }

        } else {
            for (size_t j = 0; j < ic / 3; j++) {
                if (Face3(rvertices[rindices[j * 3 + 0]], rvertices[rindices[j * 3 + 1]], rvertices[rindices[j * 3 + 2]]).is_degenerate())
                    continue;
                indices.push_back(vertex_ofs + rindices[j * 3 + 0]);
                indices.push_back(vertex_ofs + rindices[j * 3 + 1]);
                indices.push_back(vertex_ofs + rindices[j * 3 + 2]);
                face_materials.push_back(i);
            }
        }

        surfaces.push_back(s);
    }

    //unwrap

    float *gen_uvs;
    int *gen_vertices;
    int *gen_indices;
    int gen_vertex_count;
    int gen_index_count;
    int size_x;
    int size_y;

    bool ok = array_mesh_lightmap_unwrap_callback(p_texel_size, vertices.data(), normals.data(), vertices.size() / 3,
            indices.data(), face_materials.data(), indices.size(), &gen_uvs, &gen_vertices, &gen_vertex_count, &gen_indices,
            &gen_index_count, &size_x, &size_y);

    if (!ok) {
        return ERR_CANT_CREATE;
    }

    //remove surfaces
    while (get_surface_count()) {
        surface_remove(0);
    }

    //create surfacetools for each surface..
    PODVector<Ref<SurfaceTool> > surfaces_tools;
    surfaces_tools.reserve(surfaces.size());
    for (int i = 0; i < surfaces.size(); i++) {
        Ref<SurfaceTool> st(make_ref_counted<SurfaceTool>());
        st->begin(Mesh::PRIMITIVE_TRIANGLES);
        st->set_material(surfaces[i].material);
        surfaces_tools.push_back(st); //stay there
    }

    print_verbose("Mesh: Gen indices: " + itos(gen_index_count));
    //go through all indices
    for (int i = 0; i < gen_index_count; i += 3) {

        ERR_FAIL_INDEX_V(gen_vertices[gen_indices[i + 0]], uv_index.size(), ERR_BUG);
        ERR_FAIL_INDEX_V(gen_vertices[gen_indices[i + 1]], uv_index.size(), ERR_BUG);
        ERR_FAIL_INDEX_V(gen_vertices[gen_indices[i + 2]], uv_index.size(), ERR_BUG);

        ERR_FAIL_COND_V(uv_index[gen_vertices[gen_indices[i + 0]]].first != uv_index[gen_vertices[gen_indices[i + 1]]].first || uv_index[gen_vertices[gen_indices[i + 0]]].first != uv_index[gen_vertices[gen_indices[i + 2]]].first, ERR_BUG);

        int surface = uv_index[gen_vertices[gen_indices[i + 0]]].first;

        for (int j = 0; j < 3; j++) {

            const SurfaceTool::Vertex &v = surfaces[surface].vertices[uv_index[gen_vertices[gen_indices[i + j]]].second];

            if (surfaces[surface].format & ARRAY_FORMAT_COLOR) {
                surfaces_tools[surface]->add_color(v.color);
            }
            if (surfaces[surface].format & ARRAY_FORMAT_TEX_UV) {
                surfaces_tools[surface]->add_uv(v.uv);
            }
            if (surfaces[surface].format & ARRAY_FORMAT_NORMAL) {
                surfaces_tools[surface]->add_normal(v.normal);
            }
            if (surfaces[surface].format & ARRAY_FORMAT_TANGENT) {
                Plane t;
                t.normal = v.tangent;
                t.d = v.binormal.dot(v.normal.cross(v.tangent)) < 0 ? -1 : 1;
                surfaces_tools[surface]->add_tangent(t);
            }
            if (surfaces[surface].format & ARRAY_FORMAT_BONES) {
                surfaces_tools[surface]->add_bones(v.bones);
            }
            if (surfaces[surface].format & ARRAY_FORMAT_WEIGHTS) {
                surfaces_tools[surface]->add_weights(v.weights);
            }

            Vector2 uv2(gen_uvs[gen_indices[i + j] * 2 + 0], gen_uvs[gen_indices[i + j] * 2 + 1]);
            surfaces_tools[surface]->add_uv2(uv2);

            surfaces_tools[surface]->add_vertex(v.vertex);
        }
    }

    //free stuff
    ::free(gen_vertices);
    ::free(gen_indices);
    ::free(gen_uvs);

    //generate surfaces

    for (int i = 0; i < surfaces_tools.size(); i++) {
        surfaces_tools[i]->index();
        surfaces_tools[i]->commit(Ref<ArrayMesh>(this), surfaces[i].format);
    }

    set_lightmap_size_hint(Size2(size_x, size_y));

    return OK;
}

void ArrayMesh::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("add_blend_shape", {"name"}), &ArrayMesh::add_blend_shape);
    MethodBinder::bind_method(D_METHOD("get_blend_shape_count"), &ArrayMesh::get_blend_shape_count);
    MethodBinder::bind_method(D_METHOD("get_blend_shape_name", {"index"}), &ArrayMesh::get_blend_shape_name);
    MethodBinder::bind_method(D_METHOD("clear_blend_shapes"), &ArrayMesh::clear_blend_shapes);
    MethodBinder::bind_method(D_METHOD("set_blend_shape_mode", {"mode"}), &ArrayMesh::set_blend_shape_mode);
    MethodBinder::bind_method(D_METHOD("get_blend_shape_mode"), &ArrayMesh::get_blend_shape_mode);

    MethodBinder::bind_method(D_METHOD("add_surface_from_arrays", {"primitive", "arrays", "blend_shapes", "compress_flags"}), &ArrayMesh::_add_surface_from_arrays, {DEFVAL(Array()), DEFVAL(ARRAY_COMPRESS_DEFAULT)});
    MethodBinder::bind_method(D_METHOD("surface_remove", {"surf_idx"}), &ArrayMesh::surface_remove);
    MethodBinder::bind_method(D_METHOD("surface_update_region", {"surf_idx", "offset", "data"}), &ArrayMesh::surface_update_region);
    MethodBinder::bind_method(D_METHOD("surface_get_array_len", {"surf_idx"}), &ArrayMesh::surface_get_array_len);
    MethodBinder::bind_method(D_METHOD("surface_get_array_index_len", {"surf_idx"}), &ArrayMesh::surface_get_array_index_len);
    MethodBinder::bind_method(D_METHOD("surface_get_format", {"surf_idx"}), &ArrayMesh::surface_get_format);
    MethodBinder::bind_method(D_METHOD("surface_get_primitive_type", {"surf_idx"}), &ArrayMesh::surface_get_primitive_type);
    MethodBinder::bind_method(D_METHOD("surface_find_by_name", {"name"}), &ArrayMesh::surface_find_by_name);
    MethodBinder::bind_method(D_METHOD("surface_set_name", {"surf_idx", "name"}), &ArrayMesh::surface_set_name);
    MethodBinder::bind_method(D_METHOD("surface_get_name", {"surf_idx"}), &ArrayMesh::surface_get_name);
    MethodBinder::bind_method(D_METHOD("create_trimesh_shape"), &ArrayMesh::create_trimesh_shape);
    MethodBinder::bind_method(D_METHOD("create_convex_shape"), &ArrayMesh::create_convex_shape);
    MethodBinder::bind_method(D_METHOD("create_outline", {"margin"}), &ArrayMesh::create_outline);
    MethodBinder::bind_method(D_METHOD("regen_normalmaps"), &ArrayMesh::regen_normalmaps);
    ClassDB::set_method_flags(get_class_static_name(), StringName("regen_normalmaps"), METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);
    MethodBinder::bind_method(D_METHOD("lightmap_unwrap", {"transform", "texel_size"}), &ArrayMesh::lightmap_unwrap);
    ClassDB::set_method_flags(get_class_static_name(), StringName("lightmap_unwrap"), METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);
    MethodBinder::bind_method(D_METHOD("get_faces"), &ArrayMesh::get_faces);
    MethodBinder::bind_method(D_METHOD("generate_triangle_mesh"), &ArrayMesh::generate_triangle_mesh);

    MethodBinder::bind_method(D_METHOD("set_custom_aabb", {"aabb"}), &ArrayMesh::set_custom_aabb);
    MethodBinder::bind_method(D_METHOD("get_custom_aabb"), &ArrayMesh::get_custom_aabb);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "blend_shape_mode", PropertyHint::Enum, "Normalized,Relative", PROPERTY_USAGE_NOEDITOR), "set_blend_shape_mode", "get_blend_shape_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::AABB, "custom_aabb", PropertyHint::None, ""), "set_custom_aabb", "get_custom_aabb");
}

void ArrayMesh::reload_from_file() {
    VisualServer::get_singleton()->mesh_clear(mesh);
    surfaces.clear();
    clear_blend_shapes();
    clear_cache();

    Resource::reload_from_file();

    Object_change_notify(this);
}

ArrayMesh::ArrayMesh() {

    mesh = VisualServer::get_singleton()->mesh_create();
    blend_shape_mode = BLEND_SHAPE_MODE_RELATIVE;
}

ArrayMesh::~ArrayMesh() {

    VisualServer::get_singleton()->free_rid(mesh);
}
