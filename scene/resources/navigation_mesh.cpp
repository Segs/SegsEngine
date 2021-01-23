/*************************************************************************/
/*  navigation_mesh.cpp                                                  */
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

#include "navigation_mesh.h"

#include "core/class_db.h"
#include "core/method_bind_interface.h"
#include "core/method_bind.h"
#include "core/map.h"
#include "core/object_tooling.h"

IMPL_GDCLASS(NavigationMesh)

void NavigationMesh::create_from_mesh(const Ref<Mesh> &p_mesh) {

    vertices = {};
    clear_polygons();

    for (int i = 0; i < p_mesh->get_surface_count(); i++) {

        if (p_mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES)
            continue;
        SurfaceArrays arr = p_mesh->surface_get_arrays(i);
        auto varr = arr.positions3();
        const auto &iarr = arr.m_indices;
        if (varr.size() == 0 || iarr.size() == 0)
            continue;

        int from = vertices.size();
        vertices.insert(vertices.end(),varr.begin(),varr.end());
        int rlen = iarr.size();

        for (int j = 0; j < rlen; j += 3) {
            Vector<int> vi {
                iarr[j + 0] + from,
                iarr[j + 1] + from,
                iarr[j + 2] + from,
            };
            add_polygon(eastl::move(vi));
        }
    }
}

void NavigationMesh::set_sample_partition_type(int p_value) {
    ERR_FAIL_COND(p_value >= SAMPLE_PARTITION_MAX);
    partition_type = static_cast<SamplePartitionType>(p_value);
}

int NavigationMesh::get_sample_partition_type() const {
    return static_cast<int>(partition_type);
}

void NavigationMesh::set_parsed_geometry_type(int p_value) {
    ERR_FAIL_COND(p_value >= PARSED_GEOMETRY_MAX);
    parsed_geometry_type = static_cast<ParsedGeometryType>(p_value);
    Object_change_notify(this,StringName());
}

int NavigationMesh::get_parsed_geometry_type() const {
    return parsed_geometry_type;
}

void NavigationMesh::set_collision_mask(uint32_t p_mask) {

    collision_mask = p_mask;
}

uint32_t NavigationMesh::get_collision_mask() const {

    return collision_mask;
}

void NavigationMesh::set_collision_mask_bit(int p_bit, bool p_value) {

    uint32_t mask = get_collision_mask();
    if (p_value)
        mask |= 1 << p_bit;
    else
        mask &= ~(1 << p_bit);
    set_collision_mask(mask);
}

bool NavigationMesh::get_collision_mask_bit(int p_bit) const {

    return get_collision_mask() & (1 << p_bit);
}

void NavigationMesh::set_source_geometry_mode(int p_geometry_mode) {
    ERR_FAIL_INDEX(p_geometry_mode, SOURCE_GEOMETRY_MAX);
    source_geometry_mode = static_cast<SourceGeometryMode>(p_geometry_mode);
    Object_change_notify(this,StringName());
}

int NavigationMesh::get_source_geometry_mode() const {
    return source_geometry_mode;
}

void NavigationMesh::set_source_group_name(StringName p_group_name) {
    source_group_name = p_group_name;
}

StringName NavigationMesh::get_source_group_name() const {
    return source_group_name;
}

void NavigationMesh::set_cell_size(float p_value) {
    cell_size = p_value;
}

float NavigationMesh::get_cell_size() const {
    return cell_size;
}

void NavigationMesh::set_cell_height(float p_value) {
    cell_height = p_value;
}

float NavigationMesh::get_cell_height() const {
    return cell_height;
}

void NavigationMesh::set_agent_height(float p_value) {
    agent_height = p_value;
}

float NavigationMesh::get_agent_height() const {
    return agent_height;
}

void NavigationMesh::set_agent_radius(float p_value) {
    agent_radius = p_value;
}

float NavigationMesh::get_agent_radius() {
    return agent_radius;
}

void NavigationMesh::set_agent_max_climb(float p_value) {
    agent_max_climb = p_value;
}

float NavigationMesh::get_agent_max_climb() const {
    return agent_max_climb;
}

void NavigationMesh::set_agent_max_slope(float p_value) {
    agent_max_slope = p_value;
}

float NavigationMesh::get_agent_max_slope() const {
    return agent_max_slope;
}

void NavigationMesh::set_region_min_size(float p_value) {
    region_min_size = p_value;
}

float NavigationMesh::get_region_min_size() const {
    return region_min_size;
}

void NavigationMesh::set_region_merge_size(float p_value) {
    region_merge_size = p_value;
}

float NavigationMesh::get_region_merge_size() const {
    return region_merge_size;
}

void NavigationMesh::set_edge_max_length(float p_value) {
    edge_max_length = p_value;
}

float NavigationMesh::get_edge_max_length() const {
    return edge_max_length;
}

void NavigationMesh::set_edge_max_error(float p_value) {
    edge_max_error = p_value;
}

float NavigationMesh::get_edge_max_error() const {
    return edge_max_error;
}

void NavigationMesh::set_verts_per_poly(float p_value) {
    verts_per_poly = p_value;
}

float NavigationMesh::get_verts_per_poly() const {
    return verts_per_poly;
}

void NavigationMesh::set_detail_sample_distance(float p_value) {
    detail_sample_distance = p_value;
}

float NavigationMesh::get_detail_sample_distance() const {
    return detail_sample_distance;
}

void NavigationMesh::set_detail_sample_max_error(float p_value) {
    detail_sample_max_error = p_value;
}

float NavigationMesh::get_detail_sample_max_error() const {
    return detail_sample_max_error;
}

void NavigationMesh::set_filter_low_hanging_obstacles(bool p_value) {
    filter_low_hanging_obstacles = p_value;
}

bool NavigationMesh::get_filter_low_hanging_obstacles() const {
    return filter_low_hanging_obstacles;
}

void NavigationMesh::set_filter_ledge_spans(bool p_value) {
    filter_ledge_spans = p_value;
}

bool NavigationMesh::get_filter_ledge_spans() const {
    return filter_ledge_spans;
}

void NavigationMesh::set_filter_walkable_low_height_spans(bool p_value) {
    filter_walkable_low_height_spans = p_value;
}

bool NavigationMesh::get_filter_walkable_low_height_spans() const {
    return filter_walkable_low_height_spans;
}

void NavigationMesh::set_vertices(Vector<Vector3> &&p_vertices) {

    vertices = eastl::move(p_vertices);
    Object_change_notify(this,StringName());
}

const Vector<Vector3> &NavigationMesh::get_vertices() const {

    return vertices;
}

void NavigationMesh::_set_polygons(const Array &p_array) {

    polygons.resize(p_array.size());
    for (int i = 0; i < p_array.size(); i++) {
        polygons[i].indices = p_array[i].as<Vector<int>>();
    }
    Object_change_notify(this,StringName());
}

Array NavigationMesh::_get_polygons() const {

    Array ret;
    ret.resize(polygons.size());
    for (int i = 0; i < ret.size(); i++) {
        ret[i] = polygons[i].indices;
    }

    return ret;
}

void NavigationMesh::add_polygon(Vector<int> &&p_polygon) {

    Polygon polygon;
    polygon.indices = eastl::move(p_polygon);
    polygons.emplace_back(eastl::move(polygon));
    Object_change_notify(this,StringName());
}
int NavigationMesh::get_polygon_count() const {

    return polygons.size();
}
const Vector<int> &NavigationMesh::get_polygon(int p_idx) {

    ERR_FAIL_INDEX_V(p_idx, polygons.size(), null_int_pvec);
    return polygons[p_idx].indices;
}
void NavigationMesh::clear_polygons() {

    polygons.clear();
}

Ref<Mesh> NavigationMesh::get_debug_mesh() {

    if (debug_mesh)
        return debug_mesh;

    const Vector<Vector3> &vertices = get_vertices();
    Vector<Face3> faces;
    size_t face_count=0;
    for (int i = 0; i < get_polygon_count(); i++) {
        const Vector<int> &p = get_polygon(i);
        face_count += p.size()-2;
    }
    faces.reserve(face_count);

    for (int i = 0; i < get_polygon_count(); i++) {
        const Vector<int> &p = get_polygon(i);

        for (int j = 2; j < p.size(); j++) {
            faces.emplace_back(vertices[p[0]],vertices[p[j - 1]],vertices[p[j]]);
        }
    }

    Map<_EdgeKey, bool> edge_map;
    PoolVector<Vector3> tmeshfaces;
    tmeshfaces.resize(faces.size() * 3);

    {
        PoolVector<Vector3>::Write tw = tmeshfaces.write();
        int tidx = 0;

        for (const Face3 &f :faces) {

            for (int j = 0; j < 3; j++) {

                tw[tidx++] = f.vertex[j];
                _EdgeKey ek;
                ek.from = f.vertex[j].snapped(Vector3(CMP_EPSILON, CMP_EPSILON, CMP_EPSILON));
                ek.to = f.vertex[(j + 1) % 3].snapped(Vector3(CMP_EPSILON, CMP_EPSILON, CMP_EPSILON));
                if (ek.from < ek.to)
                    SWAP(ek.from, ek.to);

                auto F = edge_map.find(ek);

                if (F!=edge_map.end()) {

                    F->second = false;

                } else {

                    edge_map[ek] = true;
                }
            }
        }
    }
    Vector<Vector3> lines;

    for (const eastl::pair<const _EdgeKey, bool> &E : edge_map) {

        if (E.second) {
            lines.push_back(E.first.from);
            lines.push_back(E.first.to);
        }
    }

    debug_mesh = make_ref_counted<ArrayMesh>();

    SurfaceArrays arr(eastl::move(lines));

    debug_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, eastl::move(arr));

    return debug_mesh;
}

void NavigationMesh::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_sample_partition_type", {"sample_partition_type"}), &NavigationMesh::set_sample_partition_type);
    MethodBinder::bind_method(D_METHOD("get_sample_partition_type"), &NavigationMesh::get_sample_partition_type);

    MethodBinder::bind_method(D_METHOD("set_parsed_geometry_type", {"geometry_type"}),&NavigationMesh::set_parsed_geometry_type);
    MethodBinder::bind_method(D_METHOD("get_parsed_geometry_type"), &NavigationMesh::get_parsed_geometry_type);

    MethodBinder::bind_method(D_METHOD("set_collision_mask", {"mask"}),&NavigationMesh::set_collision_mask);
    MethodBinder::bind_method(D_METHOD("get_collision_mask"), &NavigationMesh::get_collision_mask);

    MethodBinder::bind_method(D_METHOD("set_collision_mask_bit", {"bit", "value"}),&NavigationMesh::set_collision_mask_bit);
    MethodBinder::bind_method(D_METHOD("get_collision_mask_bit", {"bit"}),&NavigationMesh::get_collision_mask_bit);

    MethodBinder::bind_method(D_METHOD("set_source_geometry_mode", {"mask"}),&NavigationMesh::set_source_geometry_mode);
    MethodBinder::bind_method(D_METHOD("get_source_geometry_mode"), &NavigationMesh::get_source_geometry_mode);

    MethodBinder::bind_method(D_METHOD("set_source_group_name", {"mask"}),&NavigationMesh::set_source_group_name);
    MethodBinder::bind_method(D_METHOD("get_source_group_name"), &NavigationMesh::get_source_group_name);

    MethodBinder::bind_method(D_METHOD("set_cell_size", {"cell_size"}),&NavigationMesh::set_cell_size);
    MethodBinder::bind_method(D_METHOD("get_cell_size"), &NavigationMesh::get_cell_size);

    MethodBinder::bind_method(D_METHOD("set_cell_height", {"cell_height"}),&NavigationMesh::set_cell_height);
    MethodBinder::bind_method(D_METHOD("get_cell_height"), &NavigationMesh::get_cell_height);

    MethodBinder::bind_method(D_METHOD("set_agent_height", {"agent_height"}),&NavigationMesh::set_agent_height);
    MethodBinder::bind_method(D_METHOD("get_agent_height"), &NavigationMesh::get_agent_height);

    MethodBinder::bind_method(D_METHOD("set_agent_radius", {"agent_radius"}),&NavigationMesh::set_agent_radius);
    MethodBinder::bind_method(D_METHOD("get_agent_radius"), &NavigationMesh::get_agent_radius);

    MethodBinder::bind_method(D_METHOD("set_agent_max_climb", {"agent_max_climb"}),&NavigationMesh::set_agent_max_climb);
    MethodBinder::bind_method(D_METHOD("get_agent_max_climb"), &NavigationMesh::get_agent_max_climb);

    MethodBinder::bind_method(D_METHOD("set_agent_max_slope", {"agent_max_slope"}),&NavigationMesh::set_agent_max_slope);
    MethodBinder::bind_method(D_METHOD("get_agent_max_slope"), &NavigationMesh::get_agent_max_slope);

    MethodBinder::bind_method(D_METHOD("set_region_min_size", {"region_min_size"}),&NavigationMesh::set_region_min_size);
    MethodBinder::bind_method(D_METHOD("get_region_min_size"), &NavigationMesh::get_region_min_size);

    MethodBinder::bind_method(D_METHOD("set_region_merge_size", {"region_merge_size"}),&NavigationMesh::set_region_merge_size);
    MethodBinder::bind_method(D_METHOD("get_region_merge_size"), &NavigationMesh::get_region_merge_size);

    MethodBinder::bind_method(D_METHOD("set_edge_max_length", {"edge_max_length"}),&NavigationMesh::set_edge_max_length);
    MethodBinder::bind_method(D_METHOD("get_edge_max_length"), &NavigationMesh::get_edge_max_length);

    MethodBinder::bind_method(D_METHOD("set_edge_max_error", {"edge_max_error"}),&NavigationMesh::set_edge_max_error);
    MethodBinder::bind_method(D_METHOD("get_edge_max_error"), &NavigationMesh::get_edge_max_error);

    MethodBinder::bind_method(D_METHOD("set_verts_per_poly", {"verts_per_poly"}),&NavigationMesh::set_verts_per_poly);
    MethodBinder::bind_method(D_METHOD("get_verts_per_poly"), &NavigationMesh::get_verts_per_poly);

    MethodBinder::bind_method(D_METHOD("set_detail_sample_distance", {"detail_sample_dist"}),&NavigationMesh::set_detail_sample_distance);
    MethodBinder::bind_method(D_METHOD("get_detail_sample_distance"), &NavigationMesh::get_detail_sample_distance);

    MethodBinder::bind_method(D_METHOD("set_detail_sample_max_error", {"detail_sample_max_error"}),&NavigationMesh::set_detail_sample_max_error);
    MethodBinder::bind_method(D_METHOD("get_detail_sample_max_error"), &NavigationMesh::get_detail_sample_max_error);

    MethodBinder::bind_method(D_METHOD("set_filter_low_hanging_obstacles", {"filter_low_hanging_obstacles"}),&NavigationMesh::set_filter_low_hanging_obstacles);
    MethodBinder::bind_method(D_METHOD("get_filter_low_hanging_obstacles"), &NavigationMesh::get_filter_low_hanging_obstacles);

    MethodBinder::bind_method(D_METHOD("set_filter_ledge_spans", {"filter_ledge_spans"}),&NavigationMesh::set_filter_ledge_spans);
    MethodBinder::bind_method(D_METHOD("get_filter_ledge_spans"), &NavigationMesh::get_filter_ledge_spans);

    MethodBinder::bind_method(D_METHOD("set_filter_walkable_low_height_spans", {"filter_walkable_low_height_spans"}),&NavigationMesh::set_filter_walkable_low_height_spans);
    MethodBinder::bind_method(D_METHOD("get_filter_walkable_low_height_spans"), &NavigationMesh::get_filter_walkable_low_height_spans);

    MethodBinder::bind_method(D_METHOD("set_vertices", {"vertices"}),&NavigationMesh::set_vertices);
    MethodBinder::bind_method(D_METHOD("get_vertices"), &NavigationMesh::get_vertices);

    MethodBinder::bind_method(D_METHOD("add_polygon", {"polygon"}),&NavigationMesh::add_polygon);
    MethodBinder::bind_method(D_METHOD("get_polygon_count"), &NavigationMesh::get_polygon_count);
    MethodBinder::bind_method(D_METHOD("get_polygon", {"idx"}),&NavigationMesh::get_polygon);
    MethodBinder::bind_method(D_METHOD("clear_polygons"), &NavigationMesh::clear_polygons);

    MethodBinder::bind_method(D_METHOD("create_from_mesh", {"mesh"}),&NavigationMesh::create_from_mesh);

    MethodBinder::bind_method(D_METHOD("_set_polygons", {"polygons"}),&NavigationMesh::_set_polygons);
    MethodBinder::bind_method(D_METHOD("_get_polygons"), &NavigationMesh::_get_polygons);

    BIND_CONSTANT(SAMPLE_PARTITION_WATERSHED)
    BIND_CONSTANT(SAMPLE_PARTITION_MONOTONE)
    BIND_CONSTANT(SAMPLE_PARTITION_LAYERS)

    BIND_CONSTANT(PARSED_GEOMETRY_MESH_INSTANCES)
    BIND_CONSTANT(PARSED_GEOMETRY_STATIC_COLLIDERS)
    BIND_CONSTANT(PARSED_GEOMETRY_BOTH)

    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR3_ARRAY, "vertices", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "set_vertices", "get_vertices");
    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "polygons", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_polygons", "_get_polygons");

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "sample_partition_type/sample_partition_type", PropertyHint::Enum, "Watershed,Monotone,Layers"), "set_sample_partition_type", "get_sample_partition_type");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "geometry/parsed_geometry_type", PropertyHint::Enum, "Mesh Instances,Static Colliders,Both"), "set_parsed_geometry_type", "get_parsed_geometry_type");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "geometry/collision_mask", PropertyHint::Layers3DPhysics), "set_collision_mask", "get_collision_mask");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "geometry/source_geometry_mode", PropertyHint::Enum, "Navmesh Children, Group With Children, Group Explicit"), "set_source_geometry_mode", "get_source_geometry_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "geometry/source_group_name"), "set_source_group_name", "get_source_group_name");

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "cell/size", PropertyHint::Range, "0.1,1.0,0.01,or_greater"), "set_cell_size", "get_cell_size");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "cell/height", PropertyHint::Range, "0.1,1.0,0.01,or_greater"), "set_cell_height", "get_cell_height");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "agent/height", PropertyHint::Range, "0.1,5.0,0.01,or_greater"), "set_agent_height", "get_agent_height");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "agent/radius", PropertyHint::Range, "0.1,5.0,0.01,or_greater"), "set_agent_radius", "get_agent_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "agent/max_climb", PropertyHint::Range, "0.1,5.0,0.01,or_greater"), "set_agent_max_climb", "get_agent_max_climb");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "agent/max_slope", PropertyHint::Range, "0.0,90.0,0.1"), "set_agent_max_slope", "get_agent_max_slope");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "region/min_size", PropertyHint::Range, "0.0,150.0,0.01,or_greater"), "set_region_min_size", "get_region_min_size");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "region/merge_size", PropertyHint::Range, "0.0,150.0,0.01,or_greater"), "set_region_merge_size", "get_region_merge_size");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "edge/max_length", PropertyHint::Range, "0.0,50.0,0.01,or_greater"), "set_edge_max_length", "get_edge_max_length");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "edge/max_error", PropertyHint::Range, "0.1,3.0,0.01,or_greater"), "set_edge_max_error", "get_edge_max_error");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "polygon/verts_per_poly", PropertyHint::Range, "3.0,12.0,1.0,or_greater"), "set_verts_per_poly", "get_verts_per_poly");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "detail/sample_distance", PropertyHint::Range, "0.0,16.0,0.01,or_greater"), "set_detail_sample_distance", "get_detail_sample_distance");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "detail/sample_max_error", PropertyHint::Range, "0.0,16.0,0.01,or_greater"), "set_detail_sample_max_error", "get_detail_sample_max_error");

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "filter/low_hanging_obstacles"), "set_filter_low_hanging_obstacles", "get_filter_low_hanging_obstacles");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "filter/ledge_spans"), "set_filter_ledge_spans", "get_filter_ledge_spans");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "filter/filter_walkable_low_height_spans"), "set_filter_walkable_low_height_spans", "get_filter_walkable_low_height_spans");
}

void NavigationMesh::_validate_property(PropertyInfo &property) const {
    if (property.name == "geometry/collision_mask") {
        if (parsed_geometry_type == PARSED_GEOMETRY_MESH_INSTANCES) {
            property.usage = 0;
            return;
        }
    }

    if (property.name == "geometry/source_group_name") {
        if (source_geometry_mode == SOURCE_GEOMETRY_NAVMESH_CHILDREN) {
            property.usage = 0;
            return;
        }
    }
}

NavigationMesh::NavigationMesh() {
    cell_size = 0.3f;
    cell_height = 0.2f;
    agent_height = 2.0f;
    agent_radius = 0.6f;
    agent_max_climb = 0.9f;
    agent_max_slope = 45.0f;
    region_min_size = 8.0f;
    region_merge_size = 20.0f;
    edge_max_length = 12.0f;
    edge_max_error = 1.3f;
    verts_per_poly = 6.0f;
    detail_sample_distance = 6.0f;
    detail_sample_max_error = 1.0f;

    partition_type = SAMPLE_PARTITION_WATERSHED;
    parsed_geometry_type = PARSED_GEOMETRY_MESH_INSTANCES;
    collision_mask = 0xFFFFFFFF;
    source_geometry_mode = SOURCE_GEOMETRY_NAVMESH_CHILDREN;
    source_group_name = "navmesh";
    filter_low_hanging_obstacles = false;
    filter_ledge_spans = false;
    filter_walkable_low_height_spans = false;
}
