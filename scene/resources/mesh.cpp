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

#include "core/math/transform.h"
#include "surface_tool.h"

#include "core/dictionary.h"
#include "core/crypto/crypto_core.h"
#include "core/map.h"
#include "core/math/geometry.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/pair.h"
#include "core/print_string.h"
#include "core/math/convex_hull.h"
#include "scene/resources/concave_polygon_shape_3d.h"
#include "scene/resources/convex_polygon_shape_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh_enum_casters.h"
#include "servers/rendering_server.h"

#include <cstdlib>

IMPL_GDCLASS(Mesh)
IMPL_GDCLASS(ArrayMesh)
RES_BASE_EXTENSION_IMPL(ArrayMesh,"mesh")

Mesh::ConvexDecompositionFunc Mesh::convex_decomposition_function = nullptr;

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

    Vector<Vector3> faces;
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
            const Vector<int> &indices = a.m_indices;

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

void Mesh::generate_debug_mesh_lines(Vector<Vector3> &r_lines) {

    if (!debug_lines.empty()) {
        r_lines = debug_lines;
        return;
    }

    Ref<TriangleMesh> tm = generate_triangle_mesh();
    if (not tm)
        return;

    Vector<uint32_t> triangle_indices;
    tm->get_indices(triangle_indices);
    const int triangles_num = tm->get_triangles().size();
    const Vector<Vector3> &vertices = tm->get_vertices();

    debug_lines.resize(tm->get_triangles().size() * 6); // 3 lines x 2 points each line

    for (int j = 0, x = 0, i = 0; i < triangles_num; j += 6, x += 3, ++i) {
        // Triangle line 1
        debug_lines[j + 0] = vertices[triangle_indices[x + 0]];
        debug_lines[j + 1] = vertices[triangle_indices[x + 1]];

        // Triangle line 2
        debug_lines[j + 2] = vertices[triangle_indices[x + 1]];
        debug_lines[j + 3] = vertices[triangle_indices[x + 2]];

        // Triangle line 3
        debug_lines[j + 4] = vertices[triangle_indices[x + 2]];
        debug_lines[j + 5] = vertices[triangle_indices[x + 0]];
    }

    r_lines = debug_lines;
}
void Mesh::generate_debug_mesh_indices(Vector<Vector3> &r_points) {
    Ref<TriangleMesh> tm = generate_triangle_mesh();
    if (not tm)
        return;

    r_points = tm->get_vertices();
}

bool Mesh::surface_is_softbody_friendly(int p_idx) const {
    const uint32_t surface_format = surface_get_format(p_idx);
    return (surface_format & Mesh::ARRAY_FLAG_USE_DYNAMIC_UPDATE && (!(surface_format & Mesh::ARRAY_COMPRESS_VERTEX)) && (!(surface_format & Mesh::ARRAY_COMPRESS_NORMAL)));
}

Vector<Face3> Mesh::get_faces() const {

    Ref<TriangleMesh> tm = generate_triangle_mesh();
    if (tm)
        return tm->get_faces();
    return Vector<Face3>();
    /*
    for (int i=0;i<surfaces.size();i++) {

        if (RenderingServer::get_singleton()->mesh_surface_get_primitive_type( mesh, i ) != RenderingServer::PRIMITIVE_TRIANGLES )
            continue;

        PoolVector<int> indices;
        PoolVector<Vector3> vertices;

        vertices=RenderingServer::get_singleton()->mesh_surface_get_array(mesh, i,RenderingServer::ARRAY_VERTEX);

        int len=RenderingServer::get_singleton()->mesh_surface_get_array_index_len(mesh, i);
        bool has_indices;

        if (len>0) {

            indices=RenderingServer::get_singleton()->mesh_surface_get_array(mesh, i,RenderingServer::ARRAY_INDEX);
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

Ref<Shape> Mesh::create_convex_shape(bool p_clean, bool p_simplify) const {
    if (p_simplify) {
        Vector<Ref<Shape>> decomposed = convex_decompose(1);
        if (decomposed.size() == 1) {
            return decomposed[0];
        } else {
            ERR_PRINT("Convex shape simplification failed, falling back to simpler process.");
        }
    }

    Vector<Vector3> vertices;
    //TODO: SEGS: inefficient usage of surface arrays, when only positions are used.
    for (int i = 0; i < get_surface_count(); i++) {

        SurfaceArrays a = surface_get_arrays(i);
        ERR_FAIL_COND_V(a.empty(), Ref<ConvexPolygonShape3D>());
        auto vals=a.positions3();
        vertices.insert(vertices.end(),vals.begin(),vals.end());
    }

    Ref<ConvexPolygonShape3D> shape(make_ref_counted<ConvexPolygonShape3D>());

    if (p_clean) {
        GeometryMeshData md;
        Error err = ConvexHullComputer::convex_hull(vertices, md);
        if (err == OK) {
            vertices = eastl::move(md.vertices);
        } else {
            ERR_PRINT("Convex shape cleaning failed, falling back to simpler process.");
        }
    }

    shape->set_points(eastl::move(vertices));
    return shape;
}


Ref<Shape> Mesh::create_trimesh_shape() const {

    Vector<Face3> faces = get_faces();
    if (faces.empty())
        return Ref<Shape>();

    PoolVector<Vector3> face_points;
    face_points.resize(faces.size() * 3);

    for (int i = 0; i < face_points.size(); i += 3) {

        Face3 f = faces[i / 3];
        face_points.set(i, f.vertex[0]);
        face_points.set(i + 1, f.vertex[1]);
        face_points.set(i + 2, f.vertex[2]);
    }

    Ref<ConcavePolygonShape3D> shape(make_ref_counted<ConcavePolygonShape3D>());
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
            Vector<float> &dst = arrays.m_position_data;
            const Vector<float> &src = a.m_position_data;
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

            Vector<int> &dst_idx = arrays.m_indices;
            Vector<int> src_idx = a.m_indices;
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
        Vector<int> &indices = arrays.m_indices;
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

            Vector<int> new_indices;
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

    SE_BIND_METHOD(Mesh,set_lightmap_size_hint);
    SE_BIND_METHOD(Mesh,get_lightmap_size_hint);
    SE_BIND_METHOD(Mesh,get_aabb);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "lightmap_size_hint"), "set_lightmap_size_hint", "get_lightmap_size_hint");

    SE_BIND_METHOD(Mesh,get_surface_count);
    MethodBinder::bind_method(D_METHOD("surface_get_arrays", {"surf_idx"}), &Mesh::_surface_get_arrays);
    MethodBinder::bind_method(D_METHOD("surface_get_blend_shape_arrays", {"surf_idx"}), &Mesh::_surface_get_blend_shape_arrays);
    SE_BIND_METHOD(Mesh,surface_set_material);
    SE_BIND_METHOD(Mesh,surface_get_material);

    BIND_ENUM_CONSTANT(PRIMITIVE_POINTS);
    BIND_ENUM_CONSTANT(PRIMITIVE_LINES);
    BIND_ENUM_CONSTANT(PRIMITIVE_LINE_STRIP);
    BIND_ENUM_CONSTANT(PRIMITIVE_LINE_LOOP);
    BIND_ENUM_CONSTANT(PRIMITIVE_TRIANGLES);
    BIND_ENUM_CONSTANT(PRIMITIVE_TRIANGLE_STRIP);
    BIND_ENUM_CONSTANT(PRIMITIVE_TRIANGLE_FAN);

    BIND_ENUM_CONSTANT(BLEND_SHAPE_MODE_NORMALIZED);
    BIND_ENUM_CONSTANT(BLEND_SHAPE_MODE_RELATIVE);

    BIND_ENUM_CONSTANT(ARRAY_FORMAT_VERTEX);
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_NORMAL);
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_TANGENT);
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_COLOR);
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_TEX_UV);
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_TEX_UV2);
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_BONES);
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_WEIGHTS);
    BIND_ENUM_CONSTANT(ARRAY_FORMAT_INDEX);

    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_BASE);
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_VERTEX);
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_NORMAL);
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_TANGENT);
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_COLOR);
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_TEX_UV);
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_TEX_UV2);
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_BONES);
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_WEIGHTS);
    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_INDEX);

    BIND_ENUM_CONSTANT(ARRAY_FLAG_USE_2D_VERTICES);
    BIND_ENUM_CONSTANT(ARRAY_FLAG_USE_16_BIT_BONES);
    BIND_ENUM_CONSTANT(ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION);

    BIND_ENUM_CONSTANT(ARRAY_COMPRESS_DEFAULT);

    BIND_ENUM_CONSTANT(ARRAY_VERTEX);
    BIND_ENUM_CONSTANT(ARRAY_NORMAL);
    BIND_ENUM_CONSTANT(ARRAY_TANGENT);
    BIND_ENUM_CONSTANT(ARRAY_COLOR);
    BIND_ENUM_CONSTANT(ARRAY_TEX_UV);
    BIND_ENUM_CONSTANT(ARRAY_TEX_UV2);
    BIND_ENUM_CONSTANT(ARRAY_BONES);
    BIND_ENUM_CONSTANT(ARRAY_WEIGHTS);
    BIND_ENUM_CONSTANT(ARRAY_INDEX);
    BIND_ENUM_CONSTANT(ARRAY_MAX);

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

Vector<Ref<Shape>> Mesh::convex_decompose(int p_max_convex_hulls) const {
    ERR_FAIL_COND_V(!convex_decomposition_function, {});

    Ref<TriangleMesh> tm(generate_triangle_mesh());
    ERR_FAIL_COND_V(!tm, {});

    Vector<uint32_t> indices;
    tm->get_indices(indices);
    const Vector<Vector3> &vertices = tm->get_vertices();

    Vector< Vector<Vector3>> decomposed = convex_decomposition_function(vertices, indices, p_max_convex_hulls, nullptr);

    Vector<Ref<Shape> > ret;
    ret.reserve(decomposed.size());
    for (int i = 0; i < decomposed.size(); i++) {
        Ref<ConvexPolygonShape3D> shape(make_ref_counted<ConvexPolygonShape3D>());
        shape->set_points(eastl::move(decomposed[i]));
        ret.emplace_back(eastl::move(shape));
    }

    return ret;
}

Mesh::Mesh() {
}

bool ArrayMesh::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "blend_shape/names") {

        PoolVector sk(p_value.as<PoolVector<String>>());
        int sz = sk.size();
        PoolVector<String>::Read r = sk.read();
        for (int i = 0; i < sz; i++)
            add_blend_shape(StringName(r[i]));
        return true;
    }

    if (p_name == "blend_shape/mode") {

        set_blend_shape_mode(p_value.as<BlendShapeMode>());
        return true;
    }

    if (StringUtils::begins_with(p_name,"surface_")) {

        const auto sl = StringUtils::find(p_name,"/");
        if (sl == String::npos)
            return false;

        int idx = StringUtils::to_int(StringUtils::substr(p_name,8, sl - 8)) - 1;
        const StringName what( StringUtils::get_slice(p_name,'/', 1));
        if (what == StringView("material"))
            surface_set_material(idx, refFromVariant<Material>(p_value));
        else if (what == StringView("name"))
            surface_set_name(idx, p_value.as<String>());
        return true;
    }

    if (!StringUtils::begins_with(p_name,"surfaces"))
        return false;

    int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
    StringName what(StringUtils::get_slice(p_name,'/', 2));

    if (idx != surfaces.size()) {
        return false;
    }

    //create
    Dictionary d = p_value.as<Dictionary>();
    ERR_FAIL_COND_V(!d.has("primitive"), false);

    if (d.has("arrays")) {
        //old format
        ERR_FAIL_COND_V(!d.has("morph_arrays"), false);
        Vector<SurfaceArrays> morph_arrays;
        Array ma=d["morph_arrays"].as<Array>();
        morph_arrays.reserve(ma.size());
        for(int i=0; i<ma.size(); ++i)
            morph_arrays.emplace_back(SurfaceArrays::fromArray(ma[i].as<Array>()));
        add_surface_from_arrays(d["primitive"].as< PrimitiveType>(), SurfaceArrays::fromArray(d["arrays"].as<Array>()), eastl::move(morph_arrays));

    } else if (d.has("array_data")) {

        PoolVector<uint8_t> array_data = d["array_data"].as<PoolVector<uint8_t>>();
        PoolVector<uint8_t> array_index_data;
        if (d.has("array_index_data"))
            array_index_data = d["array_index_data"].as<PoolVector<uint8_t>>();

        ERR_FAIL_COND_V(!d.has("format"), false);
        uint32_t format = d["format"].as<uint32_t>();

        uint32_t primitive = d["primitive"].as<uint32_t>();

        ERR_FAIL_COND_V(!d.has("vertex_count"), false);
        int vertex_count = d["vertex_count"].as<int>();

        int index_count = 0;
        if (d.has("index_count"))
            index_count = d["index_count"].as<int>();

        Vector<PoolVector<uint8_t> > blend_shapes;

        if (d.has("blend_shape_data")) {
            Array blend_shape_data = d["blend_shape_data"].as<Array>();
            blend_shapes.reserve(blend_shape_data.size());
            for (int i = 0; i < blend_shape_data.size(); i++) {
                PoolVector<uint8_t> shape = blend_shape_data[i].as<PoolVector<uint8_t>>();
                blend_shapes.emplace_back(eastl::move(shape));
            }
        }

        ERR_FAIL_COND_V(!d.has("aabb"), false);
        AABB aabb = d["aabb"].as<AABB>();

        PoolVector<AABB> bone_aabb;
        if (d.has("skeleton_aabb")) {
            Array baabb = d["skeleton_aabb"].as<Array>();
            bone_aabb.resize(baabb.size());
            auto wr(bone_aabb.write());
            for (int i = 0; i < baabb.size(); i++) {
                wr[i] = baabb[i].as<AABB>();
            }
        }

        add_surface(format, PrimitiveType(primitive), array_data, vertex_count, array_index_data, index_count, aabb, blend_shapes, bone_aabb);
    } else {
        ERR_FAIL_V(false);
    }

    if (d.has("material")) {

        surface_set_material(idx, refFromVariant<Material>(d["material"]));
    }
    if (d.has("name")) {
        surface_set_name(idx, d["name"].as<String>());
    }

    return true;
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
    }
    if (p_name == "blend_shape/mode") {

        r_ret = get_blend_shape_mode();
        return true;
    }
    if (StringUtils::begins_with(p_name,"surface_")) {

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

    d["array_data"] = RenderingServer::get_singleton()->mesh_surface_get_array(mesh, idx);
    d["vertex_count"] = RenderingServer::get_singleton()->mesh_surface_get_array_len(mesh, idx);
    d["array_index_data"] = RenderingServer::get_singleton()->mesh_surface_get_index_array(mesh, idx);
    d["index_count"] = RenderingServer::get_singleton()->mesh_surface_get_array_index_len(mesh, idx);
    d["primitive"] = RenderingServer::get_singleton()->mesh_surface_get_primitive_type(mesh, idx);
    d["format"] = RenderingServer::get_singleton()->mesh_surface_get_format(mesh, idx);
    d["aabb"] = RenderingServer::get_singleton()->mesh_surface_get_aabb(mesh, idx);

    const Vector<AABB> &skel_aabb = RenderingServer::get_singleton()->mesh_surface_get_skeleton_aabb(mesh, idx);
    Array arr;
    arr.resize(skel_aabb.size());
    for (int i = 0; i < skel_aabb.size(); i++) {
        arr[i] = skel_aabb[i];
    }
    d["skeleton_aabb"] = arr;

    const Vector<Vector<uint8_t> > &blend_shape_data = RenderingServer::get_singleton()->mesh_surface_get_blend_shapes(mesh, idx);

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

void ArrayMesh::_get_property_list(Vector<PropertyInfo> *p_list) const {

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

void ArrayMesh::add_surface(uint32_t p_format, PrimitiveType p_primitive, const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t> > &p_blend_shapes, const PoolVector<AABB> &p_bone_aabbs) {

    Surface s;
    s.aabb = p_aabb;
    s.is_2d = p_format & ARRAY_FLAG_USE_2D_VERTICES;
    surfaces.emplace_back(eastl::move(s));
    _recompute_aabb();

    RenderingServer::get_singleton()->mesh_add_surface(mesh, p_format, (RS::PrimitiveType)p_primitive, p_array, p_vertex_count, p_index_array, p_index_count, p_aabb, p_blend_shapes, p_bone_aabbs);
}
void ArrayMesh::_add_surface_from_arrays(PrimitiveType p_primitive, const Array &p_arrays,
    const Array &p_blend_shapes, uint32_t p_flags) {
    Vector<SurfaceArrays> inp;
    inp.reserve(p_blend_shapes.size());
    for(int i=0; i<p_blend_shapes.size(); ++i) {
        inp.emplace_back(SurfaceArrays::fromArray(p_blend_shapes[i].as<Array>()));
    }
    add_surface_from_arrays(p_primitive,SurfaceArrays::fromArray(p_arrays),eastl::move(inp),p_flags);
}

void ArrayMesh::add_surface_from_arrays(PrimitiveType p_primitive, SurfaceArrays &&p_arrays, Vector<SurfaceArrays> &&p_blend_shapes, uint32_t p_flags) {

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
    RenderingServer::get_singleton()->mesh_add_surface_from_arrays(mesh, (RS::PrimitiveType)p_primitive, eastl::move(p_arrays), eastl::move(p_blend_shapes), p_flags);



    clear_cache();
    Object_change_notify(this);
    emit_changed();
}
SurfaceArrays ArrayMesh::surface_get_arrays(int p_surface) const {

    ERR_FAIL_INDEX_V(p_surface, surfaces.size(), SurfaceArrays());
    return RenderingServer::get_singleton()->mesh_surface_get_arrays(mesh, p_surface);
}
Vector<SurfaceArrays> ArrayMesh::surface_get_blend_shape_arrays(int p_surface) const {

    ERR_FAIL_INDEX_V(p_surface, surfaces.size(), Vector<SurfaceArrays>());
    return RenderingServer::get_singleton()->mesh_surface_get_blend_shape_arrays(mesh, p_surface);
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
    RenderingServer::get_singleton()->mesh_set_blend_shape_count(mesh, blend_shapes.size());
}

int ArrayMesh::get_blend_shape_count() const {

    return blend_shapes.size();
}
StringName ArrayMesh::get_blend_shape_name(int p_index) const {
    ERR_FAIL_INDEX_V(p_index, blend_shapes.size(), StringName());
    return blend_shapes[p_index];
}

void ArrayMesh::set_blend_shape_name(int p_index, const StringName &p_name) {
    ERR_FAIL_INDEX(p_index, blend_shapes.size());

    StringName name = p_name;
    auto found = blend_shapes.find(name);
    auto tgt_iter = blend_shapes.begin()+p_index;
    if (found != blend_shapes.end() && found != tgt_iter) {
        int count = 2;
        do {
            name = StringName(String(p_name) + " " + itos(count));
            count++;
        } while (blend_shapes.contains(name));
    }

    *tgt_iter = name;
}

void ArrayMesh::clear_blend_shapes() {

    ERR_FAIL_COND_MSG(surfaces.size(), "Can't set shape key count if surfaces are already created.");

    blend_shapes.clear();
}

void ArrayMesh::set_blend_shape_mode(BlendShapeMode p_mode) {

    blend_shape_mode = p_mode;
    RenderingServer::get_singleton()->mesh_set_blend_shape_mode(mesh, (RS::BlendShapeMode)p_mode);
}

ArrayMesh::BlendShapeMode ArrayMesh::get_blend_shape_mode() const {

    return blend_shape_mode;
}

void ArrayMesh::surface_remove(int p_idx) {

    ERR_FAIL_INDEX(p_idx, surfaces.size());
    RenderingServer::get_singleton()->mesh_remove_surface(mesh, p_idx);
    surfaces.erase_at(p_idx);

    clear_cache();
    _recompute_aabb();
    Object_change_notify(this);
    emit_changed();
}

int ArrayMesh::surface_get_array_len(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), -1);
    return RenderingServer::get_singleton()->mesh_surface_get_array_len(mesh, p_idx);
}

int ArrayMesh::surface_get_array_index_len(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), -1);
    return RenderingServer::get_singleton()->mesh_surface_get_array_index_len(mesh, p_idx);
}

uint32_t ArrayMesh::surface_get_format(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), 0);
    return RenderingServer::get_singleton()->mesh_surface_get_format(mesh, p_idx);
}

ArrayMesh::PrimitiveType ArrayMesh::surface_get_primitive_type(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), PRIMITIVE_LINES);
    return (PrimitiveType)RenderingServer::get_singleton()->mesh_surface_get_primitive_type(mesh, p_idx);
}

void ArrayMesh::surface_set_material(int p_idx, const Ref<Material> &p_material) {

    ERR_FAIL_INDEX(p_idx, surfaces.size());
    if (surfaces[p_idx].material == p_material)
        return;
    surfaces[p_idx].material = p_material;
    RenderingServer::get_singleton()->mesh_surface_set_material(mesh, p_idx, not p_material ? entt::null : p_material->get_rid());

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

void ArrayMesh::surface_set_name(int p_idx, StringView p_name) {

    ERR_FAIL_INDEX(p_idx, surfaces.size());

    surfaces[p_idx].name = p_name;
    emit_changed();
}

String ArrayMesh::surface_get_name(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, surfaces.size(), String());
    return surfaces[p_idx].name;
}

void ArrayMesh::surface_update_region(int p_surface, int p_offset, const PoolVector<uint8_t> &p_data) {

    ERR_FAIL_INDEX(p_surface, surfaces.size());
    RenderingServer::get_singleton()->mesh_surface_update_region(mesh, p_surface, p_offset, p_data);
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

void ArrayMesh::add_surface_from_mesh_data(GeometryMeshData &&p_mesh_data) {

    AABB aabb;
    for (int i = 0; i < p_mesh_data.vertices.size(); i++) {

        if (i == 0)
            aabb.position = p_mesh_data.vertices[i];
        else
            aabb.expand_to(p_mesh_data.vertices[i]);
    }
    RenderingServer::get_singleton()->mesh_add_surface_from_mesh_data(mesh, eastl::move(p_mesh_data));

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

RenderingEntity ArrayMesh::get_rid() const {

    return mesh;
}
AABB ArrayMesh::get_aabb() const {

    return aabb;
}

void ArrayMesh::clear_surfaces() {
    if (mesh==entt::null) {
        return;
    }
    RenderingServer::get_singleton()->mesh_clear(mesh);
    surfaces.clear();
    aabb = AABB();
}

void ArrayMesh::set_custom_aabb(const AABB &p_custom) {

    custom_aabb = p_custom;
    RenderingServer::get_singleton()->mesh_set_custom_aabb(mesh, custom_aabb);
    emit_changed();
}

AABB ArrayMesh::get_custom_aabb() const {

    return custom_aabb;
}

void ArrayMesh::regen_normalmaps() {

    Vector<Ref<SurfaceTool> > surfs;
    surfs.reserve(get_surface_count());
    for (int i = 0; i < get_surface_count(); i++) {

        Ref<SurfaceTool> st(make_ref_counted<SurfaceTool>());
        st->create_from(Ref<ArrayMesh>(this), i);
        surfs.emplace_back(st);
    }

    while (get_surface_count()) {
        surface_remove(0);
    }

    for (size_t i = 0; i < surfs.size(); i++) {

        surfs[i]->generate_tangents();
        surfs[i]->commit(Ref<ArrayMesh>(this));
    }
}

//dirty hack
bool (*array_mesh_lightmap_unwrap_callback)(float p_texel_size, const float *p_vertices, const float *p_normals, int p_vertex_count, const int *p_indices, const int *p_face_materials, int p_index_count, float **r_uv, int **r_vertex, int *r_vertex_count, int **r_index, int *r_index_count, int *r_size_hint_x, int *r_size_hint_y) = nullptr;

struct ArrayMeshLightmapSurface {

    Ref<Material> material;
    Vector<SurfaceTool::Vertex> vertices;
    Mesh::PrimitiveType primitive;
    uint32_t format;
};

Error ArrayMesh::lightmap_unwrap(const Transform &p_base_transform, float p_texel_size) {
    int *cache_data = nullptr;
    unsigned int cache_size = 0;
    bool use_cache = false; // Don't use cache
    return lightmap_unwrap_cached(cache_data, cache_size, use_cache, p_base_transform, p_texel_size);
}

Error ArrayMesh::lightmap_unwrap_cached(int *&r_cache_data, unsigned int &r_cache_size, bool &r_used_cache, const Transform &p_base_transform, float p_texel_size) {
    ERR_FAIL_COND_V(!array_mesh_lightmap_unwrap_callback, ERR_UNCONFIGURED);
    ERR_FAIL_COND_V_MSG(!blend_shapes.empty(), ERR_UNAVAILABLE, "Can't unwrap mesh with blend shapes.");
    ERR_FAIL_COND_V_MSG(p_texel_size <= 0.0f, ERR_PARAMETER_RANGE_ERROR, "Texel size must be greater than 0.");

    Vector<float> vertices;
    Vector<float> normals;
    Vector<int> indices;
    Vector<int> face_materials;
    Vector<float> uv;
    Vector<Pair<int, int>> uv_indices;
    Vector<ArrayMeshLightmapSurface> lightmap_surfaces;

    // Keep only the scale
    Basis basis = p_base_transform.get_basis();
    Vector3 scale = Vector3(basis.get_axis(0).length(), basis.get_axis(1).length(), basis.get_axis(2).length());

    Transform transform;
    transform.scale(scale);

    Basis normal_basis = transform.basis.inverse().transposed();

    for (int i = 0; i < get_surface_count(); i++) {
        ArrayMeshLightmapSurface s;
        s.primitive = surface_get_primitive_type(i);

        ERR_FAIL_COND_V_MSG(s.primitive != Mesh::PRIMITIVE_TRIANGLES, ERR_UNAVAILABLE, "Only triangles are supported for lightmap unwrap.");
        s.format = surface_get_format(i);
        ERR_FAIL_COND_V_MSG(!(s.format & ARRAY_FORMAT_NORMAL), ERR_UNAVAILABLE, "Normals are required for lightmap unwrap.");

        SurfaceArrays arrays = surface_get_arrays(i);
        s.material = surface_get_material(i);
        s.vertices = SurfaceTool::create_vertex_array_from_triangle_arrays(arrays);

        Span<const Vector3> rvertices = arrays.positions3();
        int vc = rvertices.size();

        Span<const Vector3> rnormals = arrays.m_normals;

        int vertex_ofs = vertices.size() / 3;

        vertices.resize((vertex_ofs + vc) * 3);
        normals.resize((vertex_ofs + vc) * 3);
        uv_indices.resize(vertex_ofs + vc);

        for (int j = 0; j < vc; j++) {
            Vector3 v = transform.xform(rvertices[j]);
            Vector3 n = normal_basis.xform(rnormals[j]).normalized();

            vertices[(j + vertex_ofs) * 3 + 0] = v.x;
            vertices[(j + vertex_ofs) * 3 + 1] = v.y;
            vertices[(j + vertex_ofs) * 3 + 2] = v.z;
            normals[(j + vertex_ofs) * 3 + 0] = n.x;
            normals[(j + vertex_ofs) * 3 + 1] = n.y;
            normals[(j + vertex_ofs) * 3 + 2] = n.z;
            uv_indices[j + vertex_ofs] = Pair<int, int>(i, j);
        }

        Span<const int> rindices = arrays.m_indices;
        int ic = rindices.size();

        float eps = 1.19209290e-7F; // Taken from xatlas.h
        if (ic == 0) {
            for (int j = 0; j < vc / 3; j++) {
                Vector3 p0 = transform.xform(rvertices[j * 3 + 0]);
                Vector3 p1 = transform.xform(rvertices[j * 3 + 1]);
                Vector3 p2 = transform.xform(rvertices[j * 3 + 2]);

                if ((p0 - p1).length_squared() < eps || (p1 - p2).length_squared() < eps || (p2 - p0).length_squared() < eps) {
                    continue;
                }

                indices.push_back(vertex_ofs + j * 3 + 0);
                indices.push_back(vertex_ofs + j * 3 + 1);
                indices.push_back(vertex_ofs + j * 3 + 2);
                face_materials.push_back(i);
            }

        } else {
            for (int j = 0; j < ic / 3; j++) {
                Vector3 p0 = transform.xform(rvertices[rindices[j * 3 + 0]]);
                Vector3 p1 = transform.xform(rvertices[rindices[j * 3 + 1]]);
                Vector3 p2 = transform.xform(rvertices[rindices[j * 3 + 2]]);

                if ((p0 - p1).length_squared() < eps || (p1 - p2).length_squared() < eps || (p2 - p0).length_squared() < eps) {
                    continue;
                }

                indices.push_back(vertex_ofs + rindices[j * 3 + 0]);
                indices.push_back(vertex_ofs + rindices[j * 3 + 1]);
                indices.push_back(vertex_ofs + rindices[j * 3 + 2]);
                face_materials.push_back(i);
            }
        }

        lightmap_surfaces.push_back(s);
    }

    CryptoCore::MD5Context ctx;
    ctx.start();

    ctx.update((unsigned char *)&p_texel_size, sizeof(float));
    ctx.update((unsigned char *)indices.data(), sizeof(int) * indices.size());
    ctx.update((unsigned char *)face_materials.data(), sizeof(int) * face_materials.size());
    ctx.update((unsigned char *)vertices.data(), sizeof(float) * vertices.size());
    ctx.update((unsigned char *)normals.data(), sizeof(float) * normals.size());

    unsigned char hash[16];
    ctx.finish(hash);

    bool cached = false;
    unsigned int cache_idx = 0;

    if (r_used_cache && r_cache_data) {
        //Check if hash is in cache data

        int *cache_data = r_cache_data;
        int n_entries = cache_data[0];
        unsigned int r_idx = 1;
        for (int i = 0; i < n_entries; ++i) {
            if (memcmp(&cache_data[r_idx], hash, 16) == 0) {
                cached = true;
                cache_idx = r_idx;
                break;
            }

            r_idx += 4; // hash
            r_idx += 2; // size hint

            int vertex_count = cache_data[r_idx];
            r_idx += 1; // vertex count
            r_idx += vertex_count; // vertex
            r_idx += vertex_count * 2; // uvs

            int index_count = cache_data[r_idx];
            r_idx += 1; // index count
            r_idx += index_count; // indices
        }
    }

    //unwrap

    float *gen_uvs;
    int *gen_vertices;
    int *gen_indices;
    int gen_vertex_count;
    int gen_index_count;
    int size_x;
    int size_y;

    if (r_used_cache && cached) {
        int *cache_data = r_cache_data;

        // Return cache data pointer to the caller
        r_cache_data = &cache_data[cache_idx];

        cache_idx += 4;

        // Load size
        size_x = ((int *)cache_data)[cache_idx];
        size_y = ((int *)cache_data)[cache_idx + 1];
        cache_idx += 2;

        // Load vertices
        gen_vertex_count = cache_data[cache_idx];
        cache_idx++;
        gen_vertices = &cache_data[cache_idx];
        cache_idx += gen_vertex_count;

        // Load UVs
        gen_uvs = (float *)&cache_data[cache_idx];
        cache_idx += gen_vertex_count * 2;

        // Load indices
        gen_index_count = cache_data[cache_idx];
        cache_idx++;
        gen_indices = &cache_data[cache_idx];

        // Return cache data size to the caller
        r_cache_size = sizeof(int) * (4 + 2 + 1 + gen_vertex_count + (gen_vertex_count * 2) + 1 + gen_index_count); // hash + size hint + vertex_count + vertices + uvs + index_count + indices
        r_used_cache = true;
    }

    if (!cached) {
        bool ok = array_mesh_lightmap_unwrap_callback(p_texel_size, vertices.data(), normals.data(), vertices.size() / 3, indices.data(), face_materials.data(), indices.size(), &gen_uvs, &gen_vertices, &gen_vertex_count, &gen_indices, &gen_index_count, &size_x, &size_y);

        if (!ok) {
            return ERR_CANT_CREATE;
        }

        if (r_used_cache) {
            unsigned int new_cache_size = 4 + 2 + 1 + gen_vertex_count + (gen_vertex_count * 2) + 1 + gen_index_count; // hash + size hint + vertex_count + vertices + uvs + index_count + indices
            new_cache_size *= sizeof(int);
            int *new_cache_data = (int *)memalloc(new_cache_size);
            unsigned int new_cache_idx = 0;

            // hash
            memcpy(&new_cache_data[new_cache_idx], hash, 16);
            new_cache_idx += 4;

            // size hint
            new_cache_data[new_cache_idx] = size_x;
            new_cache_data[new_cache_idx + 1] = size_y;
            new_cache_idx += 2;

            // vertex count
            new_cache_data[new_cache_idx] = gen_vertex_count;
            new_cache_idx++;

            // vertices
            memcpy(&new_cache_data[new_cache_idx], gen_vertices, sizeof(int) * gen_vertex_count);
            new_cache_idx += gen_vertex_count;

            // uvs
            memcpy(&new_cache_data[new_cache_idx], gen_uvs, sizeof(float) * gen_vertex_count * 2);
            new_cache_idx += gen_vertex_count * 2;

            // index count
            new_cache_data[new_cache_idx] = gen_index_count;
            new_cache_idx++;

            // indices
            memcpy(&new_cache_data[new_cache_idx], gen_indices, sizeof(int) * gen_index_count);
            new_cache_idx += gen_index_count;

            // Return cache data to the caller
            r_cache_data = new_cache_data;
            r_cache_size = new_cache_size;
            r_used_cache = false;
        }
    }

    //remove surfaces
    while (get_surface_count()) {
        surface_remove(0);
    }

    //create surfacetools for each surface..
    Vector<Ref<SurfaceTool>> surfaces_tools;

    for (int i = 0; i < lightmap_surfaces.size(); i++) {
        Ref<SurfaceTool> st(make_ref_counted<SurfaceTool>());
        st->begin(Mesh::PRIMITIVE_TRIANGLES);
        st->set_material(lightmap_surfaces[i].material);
        surfaces_tools.push_back(st); //stay there
    }

    print_verbose("Mesh: Gen indices: " + itos(gen_index_count));
    //go through all indices
    for (int i = 0; i < gen_index_count; i += 3) {
        ERR_FAIL_INDEX_V(gen_vertices[gen_indices[i + 0]], (int)uv_indices.size(), ERR_BUG);
        ERR_FAIL_INDEX_V(gen_vertices[gen_indices[i + 1]], (int)uv_indices.size(), ERR_BUG);
        ERR_FAIL_INDEX_V(gen_vertices[gen_indices[i + 2]], (int)uv_indices.size(), ERR_BUG);

        ERR_FAIL_COND_V(uv_indices[gen_vertices[gen_indices[i + 0]]].first != uv_indices[gen_vertices[gen_indices[i + 1]]].first || uv_indices[gen_vertices[gen_indices[i + 0]]].first != uv_indices[gen_vertices[gen_indices[i + 2]]].first, ERR_BUG);

        int surface = uv_indices[gen_vertices[gen_indices[i + 0]]].first;

        for (int j = 0; j < 3; j++) {
            SurfaceTool::Vertex v = lightmap_surfaces[surface].vertices[uv_indices[gen_vertices[gen_indices[i + j]]].second];

            if (lightmap_surfaces[surface].format & ARRAY_FORMAT_COLOR) {
                surfaces_tools[surface]->add_color(v.color);
            }
            if (lightmap_surfaces[surface].format & ARRAY_FORMAT_TEX_UV) {
                surfaces_tools[surface]->add_uv(v.uv);
            }
            if (lightmap_surfaces[surface].format & ARRAY_FORMAT_NORMAL) {
                surfaces_tools[surface]->add_normal(v.normal);
            }
            if (lightmap_surfaces[surface].format & ARRAY_FORMAT_TANGENT) {
                Plane t;
                t.normal = v.tangent;
                t.d = v.binormal.dot(v.normal.cross(v.tangent)) < 0 ? -1 : 1;
                surfaces_tools[surface]->add_tangent(t);
            }
            if (lightmap_surfaces[surface].format & ARRAY_FORMAT_BONES) {
                surfaces_tools[surface]->add_bones(v.bones);
            }
            if (lightmap_surfaces[surface].format & ARRAY_FORMAT_WEIGHTS) {
                surfaces_tools[surface]->add_weights(v.weights);
            }

            Vector2 uv2(gen_uvs[gen_indices[i + j] * 2 + 0], gen_uvs[gen_indices[i + j] * 2 + 1]);
            surfaces_tools[surface]->add_uv2(uv2);

            surfaces_tools[surface]->add_vertex(v.vertex);
        }
    }

    //generate surfaces
    for (unsigned int i = 0; i < surfaces_tools.size(); i++) {
        surfaces_tools[i]->index();
        surfaces_tools[i]->commit(Ref<ArrayMesh>((ArrayMesh *)this), lightmap_surfaces[i].format);
    }

    set_lightmap_size_hint(Size2(size_x, size_y));

    if (!cached) {
        //free stuff
        ::free(gen_vertices);
        ::free(gen_indices);
        ::free(gen_uvs);
    }

    return OK;
}

void ArrayMesh::_bind_methods() {

    SE_BIND_METHOD(ArrayMesh,add_blend_shape);
    SE_BIND_METHOD(ArrayMesh,get_blend_shape_count);
    SE_BIND_METHOD(ArrayMesh,get_blend_shape_name);
    SE_BIND_METHOD(ArrayMesh,set_blend_shape_name);
    SE_BIND_METHOD(ArrayMesh,clear_blend_shapes);
    SE_BIND_METHOD(ArrayMesh,set_blend_shape_mode);
    SE_BIND_METHOD(ArrayMesh,get_blend_shape_mode);

    MethodBinder::bind_method(D_METHOD("add_surface_from_arrays", {"primitive", "arrays", "blend_shapes", "compress_flags"}), &ArrayMesh::_add_surface_from_arrays, {DEFVAL(Array()), DEFVAL(ARRAY_COMPRESS_DEFAULT)});
    SE_BIND_METHOD(ArrayMesh,clear_surfaces);
    SE_BIND_METHOD(ArrayMesh,surface_remove);
    SE_BIND_METHOD(ArrayMesh,surface_update_region);
    SE_BIND_METHOD(ArrayMesh,surface_get_array_len);
    SE_BIND_METHOD(ArrayMesh,surface_get_array_index_len);
    SE_BIND_METHOD(ArrayMesh,surface_get_format);
    SE_BIND_METHOD(ArrayMesh,surface_get_primitive_type);
    SE_BIND_METHOD(ArrayMesh,surface_find_by_name);
    SE_BIND_METHOD(ArrayMesh,surface_set_name);
    SE_BIND_METHOD(ArrayMesh,surface_get_name);
    SE_BIND_METHOD(ArrayMesh,create_trimesh_shape);
    SE_BIND_METHOD_WITH_DEFAULTS(ArrayMesh,create_convex_shape,DEFVAL(true), DEFVAL(false));
    SE_BIND_METHOD(ArrayMesh,create_outline);
    MethodBinder::bind_method(D_METHOD("regen_normalmaps"), &ArrayMesh::regen_normalmaps,METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);
    MethodBinder::bind_method(D_METHOD("lightmap_unwrap", {"transform", "texel_size"}), &ArrayMesh::lightmap_unwrap,METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);
    SE_BIND_METHOD(ArrayMesh,get_faces);
    SE_BIND_METHOD(ArrayMesh,generate_triangle_mesh);

    SE_BIND_METHOD(ArrayMesh,set_custom_aabb);
    SE_BIND_METHOD(ArrayMesh,get_custom_aabb);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "blend_shape_mode", PropertyHint::Enum, "Normalized,Relative", PROPERTY_USAGE_NOEDITOR), "set_blend_shape_mode", "get_blend_shape_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::AABB, "custom_aabb", PropertyHint::None, ""), "set_custom_aabb", "get_custom_aabb");
}

void ArrayMesh::reload_from_file() {
    RenderingServer::get_singleton()->mesh_clear(mesh);
    surfaces.clear();
    clear_blend_shapes();
    clear_cache();

    Resource::reload_from_file();

    Object_change_notify(this);
}

ArrayMesh::ArrayMesh() {

    mesh = RenderingServer::get_singleton()->mesh_create();
    blend_shape_mode = BLEND_SHAPE_MODE_RELATIVE;
}

ArrayMesh::~ArrayMesh() {

    RenderingServer::get_singleton()->free_rid(mesh);
}
