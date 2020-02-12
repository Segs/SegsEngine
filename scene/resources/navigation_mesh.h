/*************************************************************************/
/*  navigation_mesh.h                                                    */
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

#pragma once

#include "scene/resources/mesh.h"

class Mesh;

class NavigationMesh : public Resource {

    GDCLASS(NavigationMesh, Resource);

    PODVector<Vector3> vertices;
    struct Polygon {
        PODVector<int> indices;
    };
    PODVector<Polygon> polygons;
    Ref<ArrayMesh> debug_mesh;

    struct _EdgeKey {

        Vector3 from;
        Vector3 to;

        bool operator<(const _EdgeKey &p_with) const { return from == p_with.from ? to < p_with.to : from < p_with.from; }
    };

protected:
    static void _bind_methods();
    virtual void _validate_property(PropertyInfo &property) const;
public:
    void _set_polygons(const Array &p_array);
    Array _get_polygons() const;

public:
    enum SamplePartitionType : int8_t {
        SAMPLE_PARTITION_WATERSHED = 0,
        SAMPLE_PARTITION_MONOTONE,
        SAMPLE_PARTITION_LAYERS,
        SAMPLE_PARTITION_MAX
    };

    enum ParsedGeometryType : int8_t {
        PARSED_GEOMETRY_MESH_INSTANCES = 0,
        PARSED_GEOMETRY_STATIC_COLLIDERS,
        PARSED_GEOMETRY_BOTH,
        PARSED_GEOMETRY_MAX
    };

    enum SourceGeometryMode : int8_t {
        SOURCE_GEOMETRY_NAVMESH_CHILDREN = 0,
        SOURCE_GEOMETRY_GROUPS_WITH_CHILDREN,
        SOURCE_GEOMETRY_GROUPS_EXPLICIT,
        SOURCE_GEOMETRY_MAX
    };

protected:
    StringName source_group_name;

    float cell_size;
    float cell_height;
    float agent_height;
    float agent_radius;
    float agent_max_climb;
    float agent_max_slope;
    float region_min_size;
    float region_merge_size;
    float edge_max_length;
    float edge_max_error;
    float verts_per_poly;
    float detail_sample_distance;
    float detail_sample_max_error;

    uint32_t collision_mask;

    SamplePartitionType partition_type;
    ParsedGeometryType parsed_geometry_type;
    SourceGeometryMode source_geometry_mode;

    bool filter_low_hanging_obstacles;
    bool filter_ledge_spans;
    bool filter_walkable_low_height_spans;

public:
    // Recast settings
    void set_sample_partition_type(int p_value);
    int get_sample_partition_type() const;

    void set_parsed_geometry_type(int p_value);
    int get_parsed_geometry_type() const;

    void set_collision_mask(uint32_t p_mask);
    uint32_t get_collision_mask() const;

    void set_collision_mask_bit(int p_bit, bool p_value);
    bool get_collision_mask_bit(int p_bit) const;

    void set_source_geometry_mode(int p_geometry_mode);
    int get_source_geometry_mode() const;

    void set_source_group_name(StringName p_group_name);
    StringName get_source_group_name() const;

    void set_cell_size(float p_value);
    float get_cell_size() const;

    void set_cell_height(float p_value);
    float get_cell_height() const;

    void set_agent_height(float p_value);
    float get_agent_height() const;

    void set_agent_radius(float p_value);
    float get_agent_radius();

    void set_agent_max_climb(float p_value);
    float get_agent_max_climb() const;

    void set_agent_max_slope(float p_value);
    float get_agent_max_slope() const;

    void set_region_min_size(float p_value);
    float get_region_min_size() const;

    void set_region_merge_size(float p_value);
    float get_region_merge_size() const;

    void set_edge_max_length(float p_value);
    float get_edge_max_length() const;

    void set_edge_max_error(float p_value);
    float get_edge_max_error() const;

    void set_verts_per_poly(float p_value);
    float get_verts_per_poly() const;

    void set_detail_sample_distance(float p_value);
    float get_detail_sample_distance() const;

    void set_detail_sample_max_error(float p_value);
    float get_detail_sample_max_error() const;

    void set_filter_low_hanging_obstacles(bool p_value);
    bool get_filter_low_hanging_obstacles() const;

    void set_filter_ledge_spans(bool p_value);
    bool get_filter_ledge_spans() const;

    void set_filter_walkable_low_height_spans(bool p_value);
    bool get_filter_walkable_low_height_spans() const;

    void create_from_mesh(const Ref<Mesh> &p_mesh);

    void set_vertices(PODVector<Vector3> &&p_vertices);
    const PODVector<Vector3> &get_vertices() const;

    void add_polygon(PODVector<int> &&p_polygon);
    int get_polygon_count() const;
    const PODVector<int> &get_polygon(int p_idx);
    void clear_polygons();

    Ref<Mesh> get_debug_mesh();

    NavigationMesh();
};
