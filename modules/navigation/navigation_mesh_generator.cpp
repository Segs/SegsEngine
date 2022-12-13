/*************************************************************************/
/*  navigation_mesh_generator.cpp                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "scene/resources/multimesh.h"
#ifndef _3D_DISABLED

#include "navigation_mesh_generator.h"

#include "core/math/quick_hull.h"
#include "core/os/thread.h"
#include "core/math/geometry.h"
#include "core/method_bind_interface.h"
#include "core/method_bind.h"
#include "core/math/convex_hull.h"
#include "scene/3d/collision_shape_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/3d/physics_body_3d.h"
#include "scene/resources/box_shape_3d.h"
#include "scene/resources/capsule_shape_3d.h"
#include "scene/resources/concave_polygon_shape_3d.h"
#include "scene/resources/convex_polygon_shape_3d.h"
#include "scene/resources/cylinder_shape_3d.h"
#include "scene/resources/plane_shape.h"
#include "scene/resources/primitive_meshes.h"
#include "scene/resources/shape.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/sphere_shape_3d.h"

//#include "modules/modules_enabled.gen.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#endif

#ifdef MODULE_CSG_ENABLED
#include "modules/csg/csg_shape.h"
#endif
#ifdef MODULE_GRIDMAP_ENABLED
#include "modules/gridmap/grid_map.h"
#endif

#include <Recast.h>

#include "EASTL/deque.h"

IMPL_GDCLASS(NavigationMeshGenerator)

NavigationMeshGenerator *NavigationMeshGenerator::singleton = nullptr;

void _add_vertex(const Vector3 &p_vec3, Vector<float> &p_vertices) {
    p_vertices.push_back(p_vec3.x);
    p_vertices.push_back(p_vec3.y);
    p_vertices.push_back(p_vec3.z);
}

void _add_mesh_array(const SurfaceArrays &p_array, const Transform &p_xform, Vector<float> &p_vertices, Vector<int> &p_indices) {
    Span<const Vector3> mesh_vertices = p_array.positions3();

    const Vector<int> &mesh_indices = p_array.m_indices;

    const int face_count = mesh_indices.size() / 3;
    const int current_vertex_count = p_vertices.size() / 3;

    for (int j = 0; j < mesh_vertices.size(); j++) {
        _add_vertex(p_xform.xform(mesh_vertices[j]), p_vertices);
    }

    for (int j = 0; j < face_count; j++) {
        // CCW
        p_indices.push_back(current_vertex_count + (mesh_indices[j * 3 + 0]));
        p_indices.push_back(current_vertex_count + (mesh_indices[j * 3 + 2]));
        p_indices.push_back(current_vertex_count + (mesh_indices[j * 3 + 1]));
    }
}

void NavigationMeshGenerator::_add_mesh(const Ref<Mesh> &p_mesh, const Transform &p_xform, Vector<float> &p_vertices, Vector<int> &p_indices) {
    int current_vertex_count;

    for (int i = 0; i < p_mesh->get_surface_count(); i++) {
        current_vertex_count = p_vertices.size() / 3;

        if (p_mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
            continue;
        }

        int index_count = 0;
        if (p_mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_INDEX) {
            index_count = p_mesh->surface_get_array_index_len(i);
        } else {
            index_count = p_mesh->surface_get_array_len(i);
        }

        ERR_CONTINUE((index_count == 0 || (index_count % 3) != 0));

        int face_count = index_count / 3;

        SurfaceArrays a = p_mesh->surface_get_arrays(i);

        auto mesh_vertices = a.positions3();

        if (p_mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_INDEX) {

            const auto &mesh_indices = a.m_indices;

            for (int j = 0; j < mesh_vertices.size(); j++) {
                _add_vertex(p_xform.xform(mesh_vertices[j]), p_vertices);
            }

            for (int j = 0; j < face_count; j++) {
                // CCW
                p_indices.push_back(current_vertex_count + (mesh_indices[j * 3 + 0]));
                p_indices.push_back(current_vertex_count + (mesh_indices[j * 3 + 2]));
                p_indices.push_back(current_vertex_count + (mesh_indices[j * 3 + 1]));
            }
        } else {
            face_count = mesh_vertices.size() / 3;
            for (int j = 0; j < face_count; j++) {
                _add_vertex(p_xform.xform(mesh_vertices[j * 3 + 0]), p_vertices);
                _add_vertex(p_xform.xform(mesh_vertices[j * 3 + 2]), p_vertices);
                _add_vertex(p_xform.xform(mesh_vertices[j * 3 + 1]), p_vertices);

                p_indices.push_back(current_vertex_count + (j * 3 + 0));
                p_indices.push_back(current_vertex_count + (j * 3 + 1));
                p_indices.push_back(current_vertex_count + (j * 3 + 2));
            }
        }
    }
}

void NavigationMeshGenerator::_add_faces(const PoolVector3Array &p_faces, const Transform &p_xform, Vector<float> &p_vertices, Vector<int> &p_indices) {
    int face_count = p_faces.size() / 3;
    int current_vertex_count = p_vertices.size() / 3;
    p_vertices.reserve(p_vertices.size()+face_count*3*3);
    p_indices.reserve(p_indices.size()+face_count*3);
    for (int j = 0; j < face_count; j++) {
        _add_vertex(p_xform.xform(p_faces[j * 3 + 0]), p_vertices);
        _add_vertex(p_xform.xform(p_faces[j * 3 + 1]), p_vertices);
        _add_vertex(p_xform.xform(p_faces[j * 3 + 2]), p_vertices);

        p_indices.push_back(current_vertex_count + (j * 3 + 0));
        p_indices.push_back(current_vertex_count + (j * 3 + 2));
        p_indices.push_back(current_vertex_count + (j * 3 + 1));
    }
}

void NavigationMeshGenerator::_parse_geometry(const Transform &p_navmesh_xform, Node *p_node, Vector<float> &p_vertices, Vector<int> &p_indices, int p_generate_from, uint32_t p_collision_mask, bool p_recurse_children) {

    if (object_cast<MeshInstance3D>(p_node) && p_generate_from != NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS) {

        MeshInstance3D *mesh_instance = object_cast<MeshInstance3D>(p_node);
        Ref<Mesh> mesh = mesh_instance->get_mesh();
        if (mesh) {
            _add_mesh(mesh, p_navmesh_xform * mesh_instance->get_global_transform(), p_vertices, p_indices);
        }
    }

    if (object_cast<MultiMeshInstance3D>(p_node) && p_generate_from != NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS) {
        MultiMeshInstance3D *multimesh_instance = object_cast<MultiMeshInstance3D>(p_node);
        Ref<MultiMesh> multimesh = multimesh_instance->get_multimesh();
        Ref<Mesh> mesh = multimesh->get_mesh();
        if (mesh) {
            int n = multimesh->get_visible_instance_count();
            if (n == -1) {
                n = multimesh->get_instance_count();
            }
            for (int i = 0; i < n; i++) {
                _add_mesh(mesh, p_navmesh_xform * multimesh_instance->get_global_transform() * multimesh->get_instance_transform(i), p_vertices, p_indices);
            }
        }
    }
#ifdef MODULE_CSG_ENABLED
    if (object_cast<CSGShape>(p_node) && p_generate_from != NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS) {

        CSGShape *csg_shape = object_cast<CSGShape>(p_node);
        PositionedMeshInfo meshes(csg_shape->get_meshes_root());
        if (meshes.root_mesh) {
            _add_mesh(meshes.root_mesh, p_navmesh_xform * csg_shape->get_global_transform(), p_vertices, p_indices);
        }
    }
#endif

    if (object_cast<StaticBody3D>(p_node) && p_generate_from != NavigationMesh::PARSED_GEOMETRY_MESH_INSTANCES) {
        StaticBody3D *static_body = object_cast<StaticBody3D>(p_node);

        if (static_body->get_collision_layer() & p_collision_mask) {

            for (int i = 0; i < p_node->get_child_count(); ++i) {
                SurfaceArrays arr;
                Node *child = p_node->get_child(i);
                if (object_cast<CollisionShape3D>(child)) {
                    CollisionShape3D *col_shape = object_cast<CollisionShape3D>(child);

                    Transform transform = p_navmesh_xform * static_body->get_transform() * col_shape->get_transform();

                    Ref<Shape> s = col_shape->get_shape();

                    BoxShape3D *box = object_cast<BoxShape3D>(s.get());
                    if (box) {
                        CubeMesh::create_mesh_array(arr,box->get_extents() * 2.0);
                        _add_mesh_array(arr, transform, p_vertices, p_indices);
                    }

                    CapsuleShape3D *capsule = object_cast<CapsuleShape3D>(s.get());
                    if (capsule) {
                        CapsuleMesh::create_mesh_array(arr, capsule->get_radius(), capsule->get_height() / 2.0);
                        _add_mesh_array(arr, transform, p_vertices, p_indices);
                    }

                    CylinderShape3D *cylinder = object_cast<CylinderShape3D>(s.get());
                    if (cylinder) {
                        CylinderMesh::create_mesh_array(arr, cylinder->get_radius(), cylinder->get_radius(), cylinder->get_height());
                        _add_mesh_array(arr, transform, p_vertices, p_indices);
                    }

                    SphereShape3D *sphere = object_cast<SphereShape3D>(s.get());
                    if (sphere) {
                        SphereMesh::create_mesh_array(arr, sphere->get_radius(), sphere->get_radius() * 2.0);
                        _add_mesh_array(arr, transform, p_vertices, p_indices);
                    }

                    ConcavePolygonShape3D *concave_polygon = object_cast<ConcavePolygonShape3D>(s.get());
                    if (concave_polygon) {
                        _add_faces(concave_polygon->get_faces(), transform, p_vertices, p_indices);
                    }

                    ConvexPolygonShape3D *convex_polygon = object_cast<ConvexPolygonShape3D>(s.get());
                    if (convex_polygon) {
                        const auto &varr = convex_polygon->get_points();
                        GeometryMeshData md;

                        Error err = ConvexHullComputer::convex_hull(varr, md);

                        if (err == OK) {
                            PoolVector3Array faces;

                            for (int j = 0; j < md.faces.size(); ++j) {
                                GeometryMeshData::Face face = md.faces[j];

                                for (int k = 2; k < face.indices.size(); ++k) {
                                    faces.push_back(md.vertices[face.indices[0]]);
                                    faces.push_back(md.vertices[face.indices[k - 1]]);
                                    faces.push_back(md.vertices[face.indices[k]]);
                                }
                            }

                            _add_faces(faces, transform, p_vertices, p_indices);
                        }
                    }
                }
            }
        }
    }

#ifdef MODULE_GRIDMAP_ENABLED
    GridMap *gridmap = object_cast<GridMap>(p_node);
    if (gridmap && p_generate_from != NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS) {
        Vector<PositionedMeshInfo> meshes(gridmap->get_positioned_meshes());
        Transform xform = gridmap->get_transform();
        for (PositionedMeshInfo & m : meshes) {
            if (m.root_mesh) {
                _add_mesh(m.root_mesh, p_navmesh_xform * xform * m.transform, p_vertices, p_indices);
            }
        }
        if (p_generate_from != NavigationMesh::PARSED_GEOMETRY_MESH_INSTANCES && (gridmap->get_collision_layer() & p_collision_mask)) {
            Vector<CollisionShapeAndTransform> shapes = gridmap->get_collision_shapes_ex();
            for (int i = 0; i < shapes.size(); i ++) {
                SurfaceArrays arr;

                const Transform &transform(shapes[i].transform);
                RID shape = shapes[i].shape;
                PhysicsServer3D::ShapeType type = PhysicsServer3D::get_singleton()->shape_get_type(shape);
                Variant data = PhysicsServer3D::get_singleton()->shape_get_data(shape);

                switch (type) {
                    case PhysicsServer3D::SHAPE_SPHERE: {
                        real_t radius = data.as<float>();
                        SphereMesh::create_mesh_array(arr, radius, radius * 2.0);
                        _add_mesh_array(arr, transform, p_vertices, p_indices);
                    } break;
                    case PhysicsServer3D::SHAPE_BOX: {
                        Vector3 extents = data.as<Vector3>();
                        CubeMesh::create_mesh_array(arr, extents * 2.0);
                        _add_mesh_array(arr, transform, p_vertices, p_indices);
                    } break;
                    case PhysicsServer3D::SHAPE_CAPSULE: {
                        Dictionary dict = data.as<Dictionary>();
                        real_t radius = dict["radius"].as<float>();
                        real_t height = dict["height"].as<float>();
                        CapsuleMesh::create_mesh_array(arr, radius, height * 0.5);
                        _add_mesh_array(arr, transform, p_vertices, p_indices);
                    } break;
                    case PhysicsServer3D::SHAPE_CYLINDER: {
                        Dictionary dict = data.as<Dictionary>();
                        real_t radius = dict["radius"].as<float>();
                        real_t height = dict["height"].as<float>();
                        CylinderMesh::create_mesh_array(arr, radius, radius, height);
                        _add_mesh_array(arr, transform, p_vertices, p_indices);
                    } break;
                    case PhysicsServer3D::SHAPE_CONVEX_POLYGON: {
                        PoolVector3Array vertices = data.as<PoolVector3Array>();
                        GeometryMeshData md;

                        Error err = ConvexHullComputer::convex_hull(vertices, md);

                        if (err == OK) {
                            PoolVector3Array faces;

                            for (int j = 0; j < md.faces.size(); ++j) {
                                GeometryMeshData::Face face = md.faces[j];

                                for (int k = 2; k < face.indices.size(); ++k) {
                                    faces.push_back(md.vertices[face.indices[0]]);
                                    faces.push_back(md.vertices[face.indices[k - 1]]);
                                    faces.push_back(md.vertices[face.indices[k]]);
    }
                            }

                            _add_faces(faces, transform, p_vertices, p_indices);
    }
                    } break;
                    case PhysicsServer3D::SHAPE_CONCAVE_POLYGON: {
                        PoolVector3Array faces = data.as<PoolVector3Array>();
                        _add_faces(faces, transform, p_vertices, p_indices);
                    } break;
                    default: {
                        WARN_PRINT("Unsupported collision shape type.");
                    } break;
                }
            }
        }
    }

#endif

    if (p_recurse_children) {
        for (int i = 0; i < p_node->get_child_count(); i++) {
            _parse_geometry(p_navmesh_xform, p_node->get_child(i), p_vertices, p_indices, p_generate_from, p_collision_mask, p_recurse_children);
        }
    }
}

void NavigationMeshGenerator::_convert_detail_mesh_to_native_navigation_mesh(const rcPolyMeshDetail *p_detail_mesh, Ref<NavigationMesh> p_nav_mesh) {

    Vector<Vector3> nav_vertices;
    nav_vertices.reserve(p_detail_mesh->nverts);
    for (int i = 0; i < p_detail_mesh->nverts; i++) {
        const float *v = &p_detail_mesh->verts[i * 3];
        nav_vertices.emplace_back(v[0], v[1], v[2]);
    }
    p_nav_mesh->set_vertices(eastl::move(nav_vertices));

    for (int i = 0; i < p_detail_mesh->nmeshes; i++) {
        const unsigned int *m = &p_detail_mesh->meshes[i * 4];
        const unsigned int bverts = m[0];
        const unsigned int btris = m[2];
        const unsigned int ntris = m[3];
        const unsigned char *tris = &p_detail_mesh->tris[btris * 4];
        for (unsigned int j = 0; j < ntris; j++) {
            // Polygon order in recast is opposite than godot's
            Vector<int> nav_indices {
                (int)(bverts + tris[j * 4 + 0]),
                (int)(bverts + tris[j * 4 + 2]),
                (int)(bverts + tris[j * 4 + 1]),
            };
            p_nav_mesh->add_polygon(eastl::move(nav_indices));
        }
    }
}

void NavigationMeshGenerator::_build_recast_navigation_mesh(
        Ref<NavigationMesh> p_nav_mesh,
#ifdef TOOLS_ENABLED
        EditorProgress *ep,
#endif
        rcHeightfield *hf,
        rcCompactHeightfield *chf,
        rcContourSet *cset,
        rcPolyMesh *poly_mesh,
        rcPolyMeshDetail *detail_mesh,
        Vector<float> &vertices,
        Vector<int> &indices) {
    rcContext ctx;

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Setting up Configuration..."), 1);
#endif

    const float *verts = vertices.data();
    const int nverts = vertices.size() / 3;
    const int *tris = indices.data();
    const int ntris = indices.size() / 3;

    float bmin[3], bmax[3];
    rcCalcBounds(verts, nverts, bmin, bmax);

    rcConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.cs = p_nav_mesh->get_cell_size();
    cfg.ch = p_nav_mesh->get_cell_height();
    cfg.walkableSlopeAngle = p_nav_mesh->get_agent_max_slope();
    cfg.walkableHeight = (int)Math::ceil(p_nav_mesh->get_agent_height() / cfg.ch);
    cfg.walkableClimb = (int)Math::floor(p_nav_mesh->get_agent_max_climb() / cfg.ch);
    cfg.walkableRadius = (int)Math::ceil(p_nav_mesh->get_agent_radius() / cfg.cs);
    cfg.maxEdgeLen = (int)(p_nav_mesh->get_edge_max_length() / p_nav_mesh->get_cell_size());
    cfg.maxSimplificationError = p_nav_mesh->get_edge_max_error();
    cfg.minRegionArea = (int)(p_nav_mesh->get_region_min_size() * p_nav_mesh->get_region_min_size());
    cfg.mergeRegionArea = (int)(p_nav_mesh->get_region_merge_size() * p_nav_mesh->get_region_merge_size());
    cfg.maxVertsPerPoly = (int)p_nav_mesh->get_verts_per_poly();
    cfg.detailSampleDist = p_nav_mesh->get_detail_sample_distance() < 0.9f ? 0 : p_nav_mesh->get_cell_size() * p_nav_mesh->get_detail_sample_distance();
    cfg.detailSampleMaxError = p_nav_mesh->get_cell_height() * p_nav_mesh->get_detail_sample_max_error();

    cfg.bmin[0] = bmin[0];
    cfg.bmin[1] = bmin[1];
    cfg.bmin[2] = bmin[2];
    cfg.bmax[0] = bmax[0];
    cfg.bmax[1] = bmax[1];
    cfg.bmax[2] = bmax[2];

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Calculating grid size..."), 2);
#endif
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Creating heightfield..."), 3);
#endif
    hf = rcAllocHeightfield();

    ERR_FAIL_COND(!hf);
    ERR_FAIL_COND(!rcCreateHeightfield(&ctx, *hf, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch));

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Marking walkable triangles..."), 4);
#endif
    {
        Vector<uint8_t> tri_areas;
        tri_areas.resize(ntris);

        ERR_FAIL_COND(tri_areas.size() == 0);

        memset(tri_areas.data(), 0, ntris * sizeof(unsigned char));
        rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts, nverts, tris, ntris, tri_areas.data());

        ERR_FAIL_COND(!rcRasterizeTriangles(&ctx, verts, nverts, tris, tri_areas.data(), ntris, *hf, cfg.walkableClimb));
    }

    if (p_nav_mesh->get_filter_low_hanging_obstacles()) {
        rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *hf);
}
    if (p_nav_mesh->get_filter_ledge_spans()) {
        rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf);
}
    if (p_nav_mesh->get_filter_walkable_low_height_spans()) {
        rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *hf);
}

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Constructing compact heightfield..."), 5);
#endif

    chf = rcAllocCompactHeightfield();

    ERR_FAIL_COND(!chf);
    ERR_FAIL_COND(!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf, *chf));

    rcFreeHeightField(hf);
    hf = nullptr;

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Eroding walkable area..."), 6);
#endif

    ERR_FAIL_COND(!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf));

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Partitioning..."), 7);
#endif

    if (p_nav_mesh->get_sample_partition_type() == NavigationMesh::SAMPLE_PARTITION_WATERSHED) {
        ERR_FAIL_COND(!rcBuildDistanceField(&ctx, *chf));
        ERR_FAIL_COND(!rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea));
    } else if (p_nav_mesh->get_sample_partition_type() == NavigationMesh::SAMPLE_PARTITION_MONOTONE) {
        ERR_FAIL_COND(!rcBuildRegionsMonotone(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea));
    } else {
        ERR_FAIL_COND(!rcBuildLayerRegions(&ctx, *chf, 0, cfg.minRegionArea));
    }

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Creating contours..."), 8);
#endif

    cset = rcAllocContourSet();

    ERR_FAIL_COND(!cset);
    ERR_FAIL_COND(!rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset));

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Creating polymesh..."), 9);
#endif

    poly_mesh = rcAllocPolyMesh();
    ERR_FAIL_COND(!poly_mesh);
    ERR_FAIL_COND(!rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *poly_mesh));

    detail_mesh = rcAllocPolyMeshDetail();
    ERR_FAIL_COND(!detail_mesh);
    ERR_FAIL_COND(!rcBuildPolyMeshDetail(&ctx, *poly_mesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *detail_mesh));

    rcFreeCompactHeightfield(chf);
    chf = nullptr;
    rcFreeContourSet(cset);
    cset = nullptr;

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Converting to native navigation mesh..."), 10);
#endif

    _convert_detail_mesh_to_native_navigation_mesh(detail_mesh, p_nav_mesh);

    rcFreePolyMesh(poly_mesh);
    poly_mesh = nullptr;
    rcFreePolyMeshDetail(detail_mesh);
    detail_mesh = nullptr;
}

NavigationMeshGenerator *NavigationMeshGenerator::get_singleton() {
    return singleton;
}

NavigationMeshGenerator::NavigationMeshGenerator() {
    singleton = this;
}

NavigationMeshGenerator::~NavigationMeshGenerator() {
}

void NavigationMeshGenerator::bake(Ref<NavigationMesh> p_nav_mesh, Node *p_node) {

    ERR_FAIL_COND_MSG(!p_nav_mesh, "Invalid Navigation Mesh");

#ifdef TOOLS_ENABLED
    EditorProgress *ep(nullptr);
    if (Engine::get_singleton()->is_editor_hint()) {
        ep = memnew(EditorProgress("bake", TTR("Navigation Mesh Generator Setup:"), 11));
    }

    if (ep)
        ep->step(TTR("Parsing Geometry..."), 0);
#endif

    Vector<float> vertices;
    Vector<int> indices;

    Dequeue<Node *> parse_nodes;

    if (p_nav_mesh->get_source_geometry_mode() == NavigationMesh::SOURCE_GEOMETRY_NAVMESH_CHILDREN) {
        parse_nodes.push_back(p_node);
    } else {
        p_node->get_tree()->get_nodes_in_group(p_nav_mesh->get_source_group_name(), &parse_nodes);
    }

    Transform navmesh_xform = object_cast<Node3D>(p_node)->get_global_transform().affine_inverse();
    for (Node * E : parse_nodes) {
        NavigationMesh::ParsedGeometryType geometry_type = p_nav_mesh->get_parsed_geometry_type();
        uint32_t collision_mask = p_nav_mesh->get_collision_mask();
        bool recurse_children = p_nav_mesh->get_source_geometry_mode() != NavigationMesh::SOURCE_GEOMETRY_GROUPS_EXPLICIT;
        _parse_geometry(navmesh_xform, E, vertices, indices, geometry_type, collision_mask, recurse_children);
    }

    if (vertices.size() > 0 && indices.size() > 0) {

        rcHeightfield *hf = nullptr;
        rcCompactHeightfield *chf = nullptr;
        rcContourSet *cset = nullptr;
        rcPolyMesh *poly_mesh = nullptr;
        rcPolyMeshDetail *detail_mesh = nullptr;

        _build_recast_navigation_mesh(
                p_nav_mesh,
#ifdef TOOLS_ENABLED
                ep,
#endif
                hf,
                chf,
                cset,
                poly_mesh,
                detail_mesh,
                vertices,
                indices);

        rcFreeHeightField(hf);
        hf = nullptr;

        rcFreeCompactHeightfield(chf);
        chf = nullptr;

        rcFreeContourSet(cset);
        cset = nullptr;

        rcFreePolyMesh(poly_mesh);
        poly_mesh = nullptr;

        rcFreePolyMeshDetail(detail_mesh);
        detail_mesh = nullptr;
    }

#ifdef TOOLS_ENABLED
    if (ep)
        ep->step(TTR("Done!"), 11);

    memdelete(ep);
#endif
    p_nav_mesh->property_list_changed_notify();
}

void NavigationMeshGenerator::clear(Ref<NavigationMesh> p_nav_mesh) {
    if (p_nav_mesh) {
        p_nav_mesh->clear_polygons();
        p_nav_mesh->set_vertices({});
    }
}

void NavigationMeshGenerator::_bind_methods() {
    SE_BIND_METHOD(NavigationMeshGenerator,bake);
    SE_BIND_METHOD(NavigationMeshGenerator,clear);
}

#endif
