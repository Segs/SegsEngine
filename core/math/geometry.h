/*************************************************************************/
/*  geometry.h                                                           */
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

#pragma once

#include "core/math/plane.h"
#include "core/math/vector3.h"
#include "core/vector.h"

template<typename T>
class PoolVector;

class Face3;
struct Vector2;
struct Vector2i;

using Size2i = Vector2i;
using Point2i = Vector2i;

struct GeometryMeshData;

class GODOT_EXPORT Geometry {
public:
    Geometry() = delete;

    static real_t get_closest_points_between_segments(Vector2 p1, Vector2 q1, Vector2 p2, Vector2 q2, Vector2 &c1, Vector2 &c2);
    static void get_closest_points_between_segments(
            const Vector3 &p_p0, const Vector3 &p_p1, const Vector3 &p_q0, const Vector3 &p_q1, Vector3 &r_ps, Vector3 &r_qt);
    static real_t get_closest_distance_between_segments(const Vector3 &p_p0, const Vector3 &p_p1, const Vector3 &p_q0, const Vector3 &p_q1);
    static bool ray_intersects_triangle(const Vector3 &p_from, const Vector3 &p_dir, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, Vector3 *r_res = nullptr);
    static bool segment_intersects_triangle(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, Vector3 *r_res = nullptr);
    static bool segment_intersects_sphere(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_sphere_pos, real_t p_sphere_radius, Vector3 *r_res = nullptr, Vector3 *r_norm = nullptr);
    static bool segment_intersects_cylinder(const Vector3 &p_from, const Vector3 &p_to, real_t p_height, real_t p_radius, Vector3 *r_res = nullptr, Vector3 *r_norm = nullptr, int p_cylinder_axis = 2);
    static bool segment_intersects_convex(const Vector3 &p_from, const Vector3 &p_to, const Plane *p_planes, int p_plane_count, Vector3 *p_res, Vector3 *p_norm);
    static Vector3 get_closest_point_to_segment(const Vector3 &p_point, const Vector3 *p_segment);
    static Vector3 get_closest_point_to_segment_uncapped(const Vector3 &p_point, const Vector3 *p_segment);
    static Vector2 get_closest_point_to_segment_2d(const Vector2 &p_point, const Vector2 *p_segment);
    static bool is_point_in_triangle(const Vector2 &s, const Vector2 &a, const Vector2 &b, const Vector2 &c);
    static Vector3 barycentric_coordinates_2d(Vector2 s, Vector2 a, Vector2 b, Vector2 c);
    static Vector2 get_closest_point_to_segment_uncapped_2d(const Vector2 &p_point, const Vector2 *p_segment);
    static bool line_intersects_line_2d(const Vector2 &p_from_a, const Vector2 &p_dir_a, const Vector2 &p_from_b, const Vector2 &p_dir_b, Vector2 &r_result);
    static bool segment_intersects_segment_2d(const Vector2 &p_from_a, const Vector2 &p_to_a, const Vector2 &p_from_b, const Vector2 &p_to_b, Vector2 *r_result);
    static bool point_in_projected_triangle(const Vector3 &p_point, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3);
    static bool triangle_sphere_intersection_test(const Vector3 *p_triangle, const Vector3 &p_normal, const Vector3 &p_sphere_pos, real_t p_sphere_radius, Vector3 &r_triangle_contact, Vector3 &r_sphere_contact);
    static bool is_point_in_circle(const Vector2 &p_point, const Vector2 &p_circle_pos, real_t p_circle_radius);
    static real_t segment_intersects_circle(const Vector2 &p_from, const Vector2 &p_to, const Vector2 &p_circle_pos, real_t p_circle_radius);

    static Vector<Vector3> clip_polygon(Span<const Vector3> &polygon, const Plane &p_plane);

    enum PolyBooleanOperation {
        OPERATION_UNION,
        OPERATION_DIFFERENCE,
        OPERATION_INTERSECTION,
        OPERATION_XOR
    };
    enum PolyJoinType {
        JOIN_SQUARE,
        JOIN_ROUND,
        JOIN_MITER
    };
    enum PolyEndType {
        END_POLYGON,
        END_JOINED,
        END_BUTT,
        END_SQUARE,
        END_ROUND
    };

    static Vector<Vector<Vector2> > merge_polygons_2d(const Vector<Vector2> &p_polygon_a, Span<const Vector2> p_polygon_b);
    static Vector<Vector<Vector2>> clip_polygons_2d(const Vector<Vector2> &p_polygon_a, Span<const Vector2> p_polygon_b);
    static Vector<Vector<Vector2>> intersect_polygons_2d(Span<const Vector2> p_polygon_a, Span<const Vector2> p_polygon_b);
    static Vector<Vector<Vector2>> exclude_polygons_2d(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b);
    static Vector<Vector<Vector2>> clip_polyline_with_polygon_2d(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon);
    static Vector<Vector<Vector2>> intersect_polyline_with_polygon_2d(const Vector<Vector2> &p_polyline, Span<const Vector2> p_polygon);
    static Vector<Vector<Vector2>> offset_polygon_2d(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type);
    static Vector<Vector<Vector2>> offset_polyline_2d(
            const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type, PolyEndType p_end_type);

    static Vector<int> triangulate_delaunay_2d(Span<const Vector2> p_points);

    static Vector<int> triangulate_polygon(Span<const Vector2> p_polygon);

    static bool is_polygon_clockwise(Span<const Vector2> p_polygon);

    // Alternate implementation that should be faster.
    static bool is_point_in_polygon(Vector2 p_point, Span<const Vector2> p_polygon);

    /// Create a "wrap" that encloses the given geometry.
    static Vector<Face3> wrap_geometry(Span<const Face3> p_array, real_t *p_error = nullptr);
    static Vector<Vector2> convex_hull_2d(Span<const Vector2> P);
    static Vector<Vector<Vector2> > decompose_polygon_in_convex(Span<const Vector2> polygon);

    static GeometryMeshData build_convex_mesh(Span<const Plane> p_planes);
    static PoolVector<Plane> build_sphere_planes(real_t p_radius, int p_lats, int p_lons, Vector3::Axis p_axis = Vector3::AXIS_Z);
    static PoolVector<Plane> build_box_planes(const Vector3 &p_extents);
    static PoolVector<Plane> build_cylinder_planes(real_t p_radius, real_t p_height, int p_sides, Vector3::Axis p_axis = Vector3::AXIS_Z);
    static PoolVector<Plane> build_capsule_planes(real_t p_radius, real_t p_height, int p_sides, int p_lats, Vector3::Axis p_axis = Vector3::AXIS_Z);
    static void sort_polygon_winding(Vector<Vector2> &r_verts, bool p_clockwise = true);
    static real_t find_polygon_area(Span<const Vector3> p_verts);

    static void make_atlas(const Vector<Size2i> &p_rects, Vector<Vector2i> &r_result, Size2i &r_size);
    struct PackRectsResult {
        int x;
        int y;
        bool packed;
    };
    static Vector<PackRectsResult> partial_pack_rects(Span<const Vector2i> p_sizes, const Size2i &p_atlas_size);

    static FixedVector<Vector3,8,false> compute_convex_mesh_points_6(Span<const Plane,6> p_planes, real_t p_epsilon = CMP_EPSILON);
    static Vector<Vector3> compute_convex_mesh_points(Span<const Plane> p_planes, real_t p_epsilon = CMP_EPSILON);
    static bool convex_hull_intersects_convex_hull(
            const Plane *p_planes_a, int p_plane_count_a, const Plane *p_planes_b, int p_plane_count_b);
    static real_t calculate_convex_hull_volume(const GeometryMeshData &p_md);
private:
    static Vector<Vector<Vector2> > _polypaths_do_operation(PolyBooleanOperation p_op, Span<const Vector2> p_polypath_a, Span<const Vector2> p_polypath_b, bool is_a_open = false);
    static Vector<Vector<Vector2> > _polypath_offset(const Vector<Vector2> &p_polypath, real_t p_delta, PolyJoinType p_join_type, PolyEndType p_end_type);
};

struct GeometryMeshData {

    struct Face {
        Vector<int> indices;
        Plane plane;
    };
    struct Edge {
        int a;
        int b;
    };

    Vector<Face> faces;
    Vector<Edge> edges;
    Vector<Vector3> vertices;

    void optimize_vertices();
    void clear();
};

// Occluder Meshes contain convex faces which may contain 0 to many convex holes.
// (holes are analogous to portals)
struct OccluderMeshData {
    struct Hole {
        Vector<uint32_t> indices;
    };
    struct Face {
        Vector<uint32_t> indices;
        Vector<Hole> holes;
        Plane plane;
        bool two_way = false;
    };
    Vector<Face> faces;
    Vector<Vector3> vertices;
    void clear();
};
