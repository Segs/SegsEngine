/*************************************************************************/
/*  geometry.cpp                                                         */
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

#include "geometry.h"

#include "core/hash_map.h"
#include "core/map.h"
#include "core/math/delaunay.h"
#include "core/math/face3.h"
#include "core/math/triangulate.h"
#include "core/pool_vector.h"

#include "thirdparty/misc/clipper.hpp"
#include "thirdparty/misc/triangulator.h"
#include "EASTL/sort.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "thirdparty/stb_rect_pack/stb_rect_pack.h"
#define SCALE_FACTOR 100000.0f // Based on CMP_EPSILON.

static real_t vec2_cross(const Vector2 &O, const Vector2 &A, const Vector2 &B) {
    return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

void Geometry::get_closest_points_between_segments(
        const Vector3 &p_p0, const Vector3 &p_p1, const Vector3 &p_q0, const Vector3 &p_q1, Vector3 &r_ps, Vector3 &r_qt) {
    // Based on David Eberly's Computation of Distance Between Line Segments algorithm.

    Vector3 p = p_p1 - p_p0;
    Vector3 q = p_q1 - p_q0;
    Vector3 r = p_p0 - p_q0;

    real_t a = p.dot(p);
    real_t b = p.dot(q);
    real_t c = q.dot(q);
    real_t d = p.dot(r);
    real_t e = q.dot(r);

    real_t s = 0.0f;
    real_t t = 0.0f;

    real_t det = a * c - b * b;
    if (det > CMP_EPSILON) {
        // Non-parallel segments
        real_t bte = b * e;
        real_t ctd = c * d;

        if (bte <= ctd) {
            // s <= 0.0f
            if (e <= 0.0f) {
                // t <= 0.0f
                s = (-d >= a ? 1 : (-d > 0.0f ? -d / a : 0.0f));
                t = 0.0f;
            } else if (e < c) {
                // 0.0f < t < 1
                s = 0.0f;
                t = e / c;
            } else {
                // t >= 1
                s = (b - d >= a ? 1 : (b - d > 0.0f ? (b - d) / a : 0.0f));
                t = 1;
            }
        } else {
            // s > 0.0f
            s = bte - ctd;
            if (s >= det) {
                // s >= 1
                if (b + e <= 0.0f) {
                    // t <= 0.0f
                    s = (-d <= 0.0f ? 0.0f : (-d < a ? -d / a : 1));
                    t = 0.0f;
                } else if (b + e < c) {
                    // 0.0f < t < 1
                    s = 1;
                    t = (b + e) / c;
                } else {
                    // t >= 1
                    s = (b - d <= 0.0f ? 0.0f : (b - d < a ? (b - d) / a : 1));
                    t = 1;
                }
            } else {
                // 0.0f < s < 1
                real_t ate = a * e;
                real_t btd = b * d;

                if (ate <= btd) {
                    // t <= 0.0f
                    s = (-d <= 0.0f ? 0.0f : (-d >= a ? 1 : -d / a));
                    t = 0.0f;
                } else {
                    // t > 0.0f
                    t = ate - btd;
                    if (t >= det) {
                        // t >= 1
                        s = (b - d <= 0.0f ? 0.0f : (b - d >= a ? 1 : (b - d) / a));
                        t = 1;
                    } else {
                        // 0.0f < t < 1
                        s /= det;
                        t /= det;
                    }
                }
            }
        }
    } else {
        // Parallel segments
        if (e <= 0.0f) {
            s = (-d <= 0.0f ? 0.0f : (-d >= a ? 1 : -d / a));
            t = 0.0f;
        } else if (e >= c) {
            s = (b - d <= 0.0f ? 0.0f : (b - d >= a ? 1 : (b - d) / a));
            t = 1;
        } else {
            s = 0.0f;
            t = e / c;
        }
    }

    r_ps = (1 - s) * p_p0 + s * p_p1;
    r_qt = (1 - t) * p_q0 + t * p_q1;
}

real_t Geometry::get_closest_distance_between_segments(const Vector3 &p_p0, const Vector3 &p_p1, const Vector3 &p_q0, const Vector3 &p_q1) {
    Vector3 ps;
    Vector3 qt;
    get_closest_points_between_segments(p_p0, p_p1, p_q0, p_q1, ps, qt);
    Vector3 st = qt - ps;
    return st.length();
}


void OccluderMeshData::clear() {
    faces.clear();
    vertices.clear();
}

void GeometryMeshData::clear() {
    faces.clear();
    edges.clear();
    vertices.clear();
}
void GeometryMeshData::optimize_vertices() {

    HashMap<int, int> vtx_remap;

    for (int i = 0; i < faces.size(); i++) {
        auto &idx_wr(faces[i].indices);

        for (int j = 0; j < idx_wr.size(); j++) {

            int idx = idx_wr[j];
            if (!vtx_remap.contains(idx)) {
                int ni = vtx_remap.size();
                vtx_remap[idx] = ni;
            }

            idx_wr[j] = vtx_remap[idx];
        }
    }

    for (int i = 0; i < edges.size(); i++) {

        int a = edges[i].a;
        int b = edges[i].b;

        if (!vtx_remap.contains(a)) {
            int ni = vtx_remap.size();
            vtx_remap[a] = ni;
        }
        if (!vtx_remap.contains(b)) {
            int ni = vtx_remap.size();
            vtx_remap[b] = ni;
        }

        edges[i].a = vtx_remap[a];
        edges[i].b = vtx_remap[b];
    }

    Vector<Vector3> new_vertices;
    new_vertices.resize(vtx_remap.size());

    for (int i = 0; i < vertices.size(); i++) {

        if (vtx_remap.contains(i))
            new_vertices[vtx_remap[i]] = vertices[i];
    }
    vertices = eastl::move(new_vertices);
}

struct _FaceClassify {

    struct _Link {

        int face;
        int edge;
        void clear() {
            face = -1;
            edge = -1;
        }
        _Link() {
            face = -1;
            edge = -1;
        }
    };
    bool valid;
    int group;
    _Link links[3];
    Face3 face;
    _FaceClassify() {
        group = -1;
        valid = false;
    }
};

/*** GEOMETRY WRAPPER ***/

enum _CellFlags {

    _CELL_SOLID = 1,
    _CELL_EXTERIOR = 2,
    _CELL_STEP_MASK = 0x1C,
    _CELL_STEP_NONE = 0 << 2,
    _CELL_STEP_Y_POS = 1 << 2,
    _CELL_STEP_Y_NEG = 2 << 2,
    _CELL_STEP_X_POS = 3 << 2,
    _CELL_STEP_X_NEG = 4 << 2,
    _CELL_STEP_Z_POS = 5 << 2,
    _CELL_STEP_Z_NEG = 6 << 2,
    _CELL_STEP_DONE = 7 << 2,
    _CELL_PREV_MASK = 0xE0,
    _CELL_PREV_NONE = 0 << 5,
    _CELL_PREV_Y_POS = 1 << 5,
    _CELL_PREV_Y_NEG = 2 << 5,
    _CELL_PREV_X_POS = 3 << 5,
    _CELL_PREV_X_NEG = 4 << 5,
    _CELL_PREV_Z_POS = 5 << 5,
    _CELL_PREV_Z_NEG = 6 << 5,
    _CELL_PREV_FIRST = 7 << 5,

};

static inline void _plot_face(uint8_t ***p_cell_status, int x, int y, int z, int len_x, int len_y, int len_z, const Vector3 &voxelsize, const Face3 &p_face) {

    AABB aabb(Vector3(x, y, z), Vector3(len_x, len_y, len_z));
    aabb.position = aabb.position * voxelsize;
    aabb.size = aabb.size * voxelsize;

    if (!p_face.intersects_aabb(aabb))
        return;

    if (len_x == 1 && len_y == 1 && len_z == 1) {

        p_cell_status[x][y][z] = _CELL_SOLID;
        return;
    }

    int div_x = len_x > 1 ? 2 : 1;
    int div_y = len_y > 1 ? 2 : 1;
    int div_z = len_z > 1 ? 2 : 1;

#define _SPLIT(m_i, m_div, m_v, m_len_v, m_new_v, m_new_len_v) \
    if (m_div == 1) {                                          \
        m_new_v = m_v;                                         \
        m_new_len_v = 1;                                       \
    } else if (m_i == 0) {                                     \
        m_new_v = m_v;                                         \
        m_new_len_v = m_len_v / 2;                             \
    } else {                                                   \
        m_new_v = m_v + m_len_v / 2;                           \
        m_new_len_v = m_len_v - m_len_v / 2;                   \
    }

    int new_x;
    int new_len_x;
    int new_y;
    int new_len_y;
    int new_z;
    int new_len_z;

    for (int i = 0; i < div_x; i++) {

        _SPLIT(i, div_x, x, len_x, new_x, new_len_x)

        for (int j = 0; j < div_y; j++) {

            _SPLIT(j, div_y, y, len_y, new_y, new_len_y)

            for (int k = 0; k < div_z; k++) {

                _SPLIT(k, div_z, z, len_z, new_z, new_len_z)

                _plot_face(p_cell_status, new_x, new_y, new_z, new_len_x, new_len_y, new_len_z, voxelsize, p_face);
            }
        }
    }
}

static inline void _mark_outside(uint8_t ***p_cell_status, int x, int y, int z, int len_x, int len_y, int len_z) {

    if (p_cell_status[x][y][z] & 3)
        return; // Nothing to do, already used and/or visited.

    p_cell_status[x][y][z] = _CELL_PREV_FIRST;

    while (true) {

        uint8_t &c = p_cell_status[x][y][z];


        if ((c & _CELL_STEP_MASK) == _CELL_STEP_NONE) {
            // Haven't been in here, mark as outside.
            p_cell_status[x][y][z] |= _CELL_EXTERIOR;
        }


        if ((c & _CELL_STEP_MASK) != _CELL_STEP_DONE) {
            // If not done, increase step.
            c += 1 << 2;
        }

        if ((c & _CELL_STEP_MASK) == _CELL_STEP_DONE) {
            // Go back.

            switch (c & _CELL_PREV_MASK) {
                case _CELL_PREV_FIRST: {
                    return;
                }
                case _CELL_PREV_Y_POS: {
                    y++;
                    ERR_FAIL_COND(y >= len_y);
                } break;
                case _CELL_PREV_Y_NEG: {
                    y--;
                    ERR_FAIL_COND(y < 0);
                } break;
                case _CELL_PREV_X_POS: {
                    x++;
                    ERR_FAIL_COND(x >= len_x);
                } break;
                case _CELL_PREV_X_NEG: {
                    x--;
                    ERR_FAIL_COND(x < 0);
                } break;
                case _CELL_PREV_Z_POS: {
                    z++;
                    ERR_FAIL_COND(z >= len_z);
                } break;
                case _CELL_PREV_Z_NEG: {
                    z--;
                    ERR_FAIL_COND(z < 0);
                } break;
                default: {
                    ERR_FAIL();
                }
            }
            continue;
        }

        int next_x = x, next_y = y, next_z = z;
        uint8_t prev = 0;

        switch (c & _CELL_STEP_MASK) {

            case _CELL_STEP_Y_POS: {

                next_y++;
                prev = _CELL_PREV_Y_NEG;
            } break;
            case _CELL_STEP_Y_NEG: {
                next_y--;
                prev = _CELL_PREV_Y_POS;
            } break;
            case _CELL_STEP_X_POS: {
                next_x++;
                prev = _CELL_PREV_X_NEG;
            } break;
            case _CELL_STEP_X_NEG: {
                next_x--;
                prev = _CELL_PREV_X_POS;
            } break;
            case _CELL_STEP_Z_POS: {
                next_z++;
                prev = _CELL_PREV_Z_NEG;
            } break;
            case _CELL_STEP_Z_NEG: {
                next_z--;
                prev = _CELL_PREV_Z_POS;
            } break;
            default: ERR_FAIL();
        }

        if (next_x < 0 || next_x >= len_x)
            continue;
        if (next_y < 0 || next_y >= len_y)
            continue;
        if (next_z < 0 || next_z >= len_z)
            continue;

        if (p_cell_status[next_x][next_y][next_z] & 3)
            continue;

        x = next_x;
        y = next_y;
        z = next_z;
        p_cell_status[x][y][z] |= prev;
    }
}

static inline void _build_faces(uint8_t ***p_cell_status, int x, int y, int z, int len_x, int len_y, int len_z, Vector<Face3> &p_faces) {

    ERR_FAIL_INDEX(x, len_x);
    ERR_FAIL_INDEX(y, len_y);
    ERR_FAIL_INDEX(z, len_z);

    if (p_cell_status[x][y][z] & _CELL_EXTERIOR)
        return;

#define vert(m_idx) Vector3(((m_idx)&4) >> 2, ((m_idx)&2) >> 1, (m_idx)&1)

    static constexpr uint8_t indices[6][4] = {
        { 7, 6, 4, 5 },
        { 7, 3, 2, 6 },
        { 7, 5, 1, 3 },
        { 0, 2, 3, 1 },
        { 0, 1, 5, 4 },
        { 0, 4, 6, 2 },

    };

    for (int i = 0; i < 6; i++) {

        Vector3 face_points[4];
        int disp_x = x + ((i % 3) == 0 ? ((i < 3) ? 1 : -1) : 0);
        int disp_y = y + (((i - 1) % 3) == 0 ? ((i < 3) ? 1 : -1) : 0);
        int disp_z = z + (((i - 2) % 3) == 0 ? ((i < 3) ? 1 : -1) : 0);

        bool plot = false;

        if (disp_x < 0 || disp_x >= len_x)
            plot = true;
        if (disp_y < 0 || disp_y >= len_y)
            plot = true;
        if (disp_z < 0 || disp_z >= len_z)
            plot = true;

        if (!plot && (p_cell_status[disp_x][disp_y][disp_z] & _CELL_EXTERIOR))
            plot = true;

        if (!plot)
            continue;

        for (int j = 0; j < 4; j++)
            face_points[j] = vert(indices[i][j]) + Vector3(x, y, z);

        p_faces.push_back(
                Face3(
                        face_points[0],
                        face_points[1],
                        face_points[2]));

        p_faces.push_back(
                Face3(
                        face_points[2],
                        face_points[3],
                        face_points[0]));
    }
}

Vector<Face3> Geometry::wrap_geometry(Span<const Face3> p_array, real_t *p_error) {

constexpr float _MIN_SIZE = 1.0f;
constexpr int _MAX_LENGTH = 20;

    int face_count = p_array.size();
    const Face3 *faces = p_array.data();

    AABB global_aabb;

    for (int i = 0; i < face_count; i++) {

        if (i == 0) {

            global_aabb = faces[i].get_aabb();
        } else {

            global_aabb.merge_with(faces[i].get_aabb());
        }
    }

    global_aabb.grow_by(0.01f); // Avoid numerical error.

    // Determine amount of cells in grid axis.
    int div_x, div_y, div_z;

    if (global_aabb.size.x / _MIN_SIZE < _MAX_LENGTH)
        div_x = (int)(global_aabb.size.x / _MIN_SIZE) + 1;
    else
        div_x = _MAX_LENGTH;

    if (global_aabb.size.y / _MIN_SIZE < _MAX_LENGTH)
        div_y = (int)(global_aabb.size.y / _MIN_SIZE) + 1;
    else
        div_y = _MAX_LENGTH;

    if (global_aabb.size.z / _MIN_SIZE < _MAX_LENGTH)
        div_z = (int)(global_aabb.size.z / _MIN_SIZE) + 1;
    else
        div_z = _MAX_LENGTH;

    Vector3 voxelsize = global_aabb.size;
    voxelsize.x /= div_x;
    voxelsize.y /= div_y;
    voxelsize.z /= div_z;

    // Create and initialize cells to zero.

    uint8_t ***cell_status = memnew_arr(uint8_t **, div_x);
    for (int i = 0; i < div_x; i++) {

        cell_status[i] = memnew_arr(uint8_t *, div_y);

        for (int j = 0; j < div_y; j++) {

            cell_status[i][j] = memnew_arr(uint8_t, div_z);

            for (int k = 0; k < div_z; k++) {

                cell_status[i][j][k] = 0;
            }
        }
    }

    // Plot faces into cells.

    for (int i = 0; i < face_count; i++) {

        Face3 f = faces[i];
        for (int j = 0; j < 3; j++) {

            f.vertex[j] -= global_aabb.position;
        }
        _plot_face(cell_status, 0, 0, 0, div_x, div_y, div_z, voxelsize, f);
    }

    // Determine which cells connect to the outside by traversing the outside and recursively flood-fill marking.

    for (int i = 0; i < div_x; i++) {

        for (int j = 0; j < div_y; j++) {

            _mark_outside(cell_status, i, j, 0, div_x, div_y, div_z);
            _mark_outside(cell_status, i, j, div_z - 1, div_x, div_y, div_z);
        }
    }

    for (int i = 0; i < div_z; i++) {

        for (int j = 0; j < div_y; j++) {

            _mark_outside(cell_status, 0, j, i, div_x, div_y, div_z);
            _mark_outside(cell_status, div_x - 1, j, i, div_x, div_y, div_z);
        }
    }

    for (int i = 0; i < div_x; i++) {

        for (int j = 0; j < div_z; j++) {

            _mark_outside(cell_status, i, 0, j, div_x, div_y, div_z);
            _mark_outside(cell_status, i, div_y - 1, j, div_x, div_y, div_z);
        }
    }

    // Build faces for the inside-outside cell divisors.

    Vector<Face3> wrapped_faces;

    for (int i = 0; i < div_x; i++) {

        for (int j = 0; j < div_y; j++) {

            for (int k = 0; k < div_z; k++) {

                _build_faces(cell_status, i, j, k, div_x, div_y, div_z, wrapped_faces);
            }
        }
    }

    // Transform face vertices to global coords.

    int wrapped_faces_count = wrapped_faces.size();
    Face3 *wrapped_faces_ptr = wrapped_faces.data();

    for (int i = 0; i < wrapped_faces_count; i++) {

        for (int j = 0; j < 3; j++) {

            Vector3 &v = wrapped_faces_ptr[i].vertex[j];
            v = v * voxelsize;
            v += global_aabb.position;
        }
    }

    // clean up grid

    for (int i = 0; i < div_x; i++) {

        for (int j = 0; j < div_y; j++) {

            memdelete_arr(cell_status[i][j]);
        }

        memdelete_arr(cell_status[i]);
    }

    memdelete_arr(cell_status);
    if (p_error)
        *p_error = voxelsize.length();

    return wrapped_faces;
}

/// \returns points on the convex hull in counter-clockwise order.
/// \note the last point in the returned list is the same as the first one.
Vector<Point2> Geometry::convex_hull_2d(Span<const Point2> _P) {
    // since passed points are going to be sorted here, make a copy :(
    Vector<Point2> P(_P.begin(),_P.end());
    size_t n = P.size(), k = 0;
    Vector<Point2> H;
    H.resize(2 * n);

    // Sort points lexicographically.
    eastl::sort(P.begin(),P.end());

    // Build lower hull.
    for (size_t i = 0; i < n; ++i) {
        while (k >= 2 && vec2_cross(H[k - 2], H[k - 1], P[i]) <= 0)
            k--;
        H[k++] = P[i];
    }

    // Build upper hull.
    for (int i = int(n) - 2, t = k + 1; i >= 0; i--) {
        while (k >= t && vec2_cross(H[k - 2], H[k - 1], P[i]) <= 0)
            k--;
        H[k++] = P[i];
    }

    H.resize(k);
    return H;
}

Vector<Vector<Vector2> > Geometry::decompose_polygon_in_convex(Span<const Point2> polygon) {
    Vector<Vector<Vector2> > decomp;
    eastl::list<TriangulatorPoly> in_poly, out_poly;

    TriangulatorPoly inp;
    inp.Init(polygon.size());
    for (int i = 0; i < polygon.size(); i++) {
        inp.GetPoint(i) = polygon[i];
    }
    inp.SetOrientation(TRIANGULATOR_CCW);
    in_poly.push_back(inp);
    TriangulatorPartition tpart;
    if (tpart.ConvexPartition_HM(&in_poly, &out_poly) == 0) { // Failed.
        ERR_PRINT("Convex decomposing failed!");
                return decomp;
    }

    decomp.resize(out_poly.size());
    int idx = 0;
    for (const TriangulatorPoly &tp : out_poly) {

        decomp[idx].resize(tp.GetNumPoints());
        memcpy(decomp[idx].data(),tp.GetPoints(),sizeof(Vector2)*tp.GetNumPoints());

        idx++;
    }

    return decomp;
}

GeometryMeshData Geometry::build_convex_mesh(Span<const Plane> p_planes) {

    GeometryMeshData mesh;

    constexpr float SUBPLANE_SIZE = 1024.0;

    real_t subplane_size = SUBPLANE_SIZE; // Should compute this from the actual plane.
    for (int i = 0; i < p_planes.size(); i++) {

        Plane p = p_planes[i];

        Vector3 ref = Vector3(0.0, 1.0, 0.0);

        if (ABS(p.normal.dot(ref)) > 0.95f)
            ref = Vector3(0.0, 0.0, 1.0); // Change axis.

        Vector3 right = p.normal.cross(ref).normalized();
        Vector3 up = p.normal.cross(right).normalized();

        Vector<Vector3> vertices;

        Vector3 center = p.get_any_point();
        // make a quad clockwise
        vertices.push_back(center - up * subplane_size + right * subplane_size);
        vertices.push_back(center - up * subplane_size - right * subplane_size);
        vertices.push_back(center + up * subplane_size - right * subplane_size);
        vertices.push_back(center + up * subplane_size + right * subplane_size);

        for (int j = 0; j < p_planes.size(); j++) {

            if (j == i) {
                continue;
            }

            Vector<Vector3> new_vertices;
            Plane clip = p_planes[j];

            if (clip.normal.dot(p.normal) > 0.95f) {
                continue;
            }

            if (vertices.size() < 3) {
                break;
            }

            for (size_t k = 0; k < vertices.size(); k++) {

                size_t k_n = (k + 1) % vertices.size();

                Vector3 edge0_A = vertices[k];
                Vector3 edge1_A = vertices[k_n];

                real_t dist0 = clip.distance_to(edge0_A);
                real_t dist1 = clip.distance_to(edge1_A);

                if (dist0 <= 0) { // Behind plane.

                    new_vertices.push_back(vertices[k]);
                }

                // Check for different sides and non coplanar.
                if ((dist0 * dist1) < 0) {

                    // calculate intersection
                    Vector3 rel = edge1_A - edge0_A;

                    real_t den = clip.normal.dot(rel);
                    if (Math::is_zero_approx(den)) {
                        continue; // Point too short.
                    }

                    real_t dist = -(clip.normal.dot(edge0_A) - clip.d) / den;
                    Vector3 inters = edge0_A + rel * dist;
                    new_vertices.push_back(inters);
                }
            }

            vertices = new_vertices;
        }

        if (vertices.size() < 3) {
            continue;
        }

        // Result is a clockwise face.

        GeometryMeshData::Face face;

        // Add face indices.
        for (size_t j = 0; j < vertices.size(); j++) {

            int idx = -1;
            for (size_t k = 0; k < mesh.vertices.size(); k++) {

                if (mesh.vertices[k].distance_to(vertices[j]) < 0.001f) {

                    idx = k;
                    break;
                }
            }

            if (idx == -1) {

                idx = mesh.vertices.size();
                mesh.vertices.push_back(vertices[j]);
            }

            face.indices.push_back(idx);
        }
        face.plane = p;
        mesh.faces.push_back(face);

        // Add edge.

        for (int j = 0; j < face.indices.size(); j++) {

            int a = face.indices[j];
            int b = face.indices[(j + 1) % face.indices.size()];

            bool found = false;
            for (auto edge : mesh.edges) {

                if (edge.a == a && edge.b == b) {
                    found = true;
                    break;
                }
                if (edge.b == a && edge.a == b) {
                    found = true;
                    break;
                }
            }

            if (found)
                continue;
            GeometryMeshData::Edge edge;
            edge.a = a;
            edge.b = b;
            mesh.edges.emplace_back(edge);
        }
    }

    return mesh;
}

PoolVector<Plane> Geometry::build_box_planes(const Vector3 &p_extents) {

    PoolVector<Plane> planes;

    planes.push_back(Plane(Vector3(1, 0, 0), p_extents.x));
    planes.push_back(Plane(Vector3(-1, 0, 0), p_extents.x));
    planes.push_back(Plane(Vector3(0, 1, 0), p_extents.y));
    planes.push_back(Plane(Vector3(0, -1, 0), p_extents.y));
    planes.push_back(Plane(Vector3(0, 0, 1), p_extents.z));
    planes.push_back(Plane(Vector3(0, 0, -1), p_extents.z));

    return planes;
}

PoolVector<Plane> Geometry::build_cylinder_planes(real_t p_radius, real_t p_height, int p_sides, Vector3::Axis p_axis) {
    ERR_FAIL_INDEX_V(p_axis, 3, PoolVector<Plane>());

    PoolVector<Plane> planes;

    for (int i = 0; i < p_sides; i++) {

        Vector3 normal;
        normal[(p_axis + 1) % 3] = Math::cos(i * (2.0f * Math_PI) / p_sides);
        normal[(p_axis + 2) % 3] = Math::sin(i * (2.0f * Math_PI) / p_sides);

        planes.push_back(Plane(normal, p_radius));
    }

    Vector3 axis;
    axis[p_axis] = 1.0;

    planes.push_back(Plane(axis, p_height * 0.5f));
    planes.push_back(Plane(-axis, p_height * 0.5f));

    return planes;
}

PoolVector<Plane> Geometry::build_sphere_planes(real_t p_radius, int p_lats, int p_lons, Vector3::Axis p_axis) {

    ERR_FAIL_INDEX_V(p_axis, 3, PoolVector<Plane>());
    PoolVector<Plane> planes;

    Vector3 axis;
    axis[p_axis] = 1.0;

    Vector3 axis_neg;
    axis_neg[(p_axis + 1) % 3] = 1.0f;
    axis_neg[(p_axis + 2) % 3] = 1.0f;
    axis_neg[p_axis] = -1.0f;

    for (int i = 0; i < p_lons; i++) {

        Vector3 normal;
        normal[(p_axis + 1) % 3] = Math::cos(i * (2.0f * Math_PI) / p_lons);
        normal[(p_axis + 2) % 3] = Math::sin(i * (2.0f * Math_PI) / p_lons);

        planes.push_back(Plane(normal, p_radius));

        for (int j = 1; j <= p_lats; j++) {

            // FIXME: This is stupid.
            Vector3 angle = normal.linear_interpolate(axis, j / (real_t)p_lats).normalized();
            Vector3 pos = angle * p_radius;
            planes.push_back(Plane(pos, angle));
            planes.push_back(Plane(pos * axis_neg, angle * axis_neg));
        }
    }

    return planes;
}

PoolVector<Plane> Geometry::build_capsule_planes(real_t p_radius, real_t p_height, int p_sides, int p_lats, Vector3::Axis p_axis) {

    ERR_FAIL_INDEX_V(p_axis, 3, PoolVector<Plane>());
    PoolVector<Plane> planes;

    Vector3 axis;
    axis[p_axis] = 1.0;

    Vector3 axis_neg;
    axis_neg[(p_axis + 1) % 3] = 1.0;
    axis_neg[(p_axis + 2) % 3] = 1.0;
    axis_neg[p_axis] = -1.0;

    for (int i = 0; i < p_sides; i++) {

        Vector3 normal;
        normal[(p_axis + 1) % 3] = Math::cos(i * (2.0f * Math_PI) / p_sides);
        normal[(p_axis + 2) % 3] = Math::sin(i * (2.0f * Math_PI) / p_sides);

        planes.push_back(Plane(normal, p_radius));

        for (int j = 1; j <= p_lats; j++) {

            Vector3 angle = normal.linear_interpolate(axis, j / (real_t)p_lats).normalized();
            Vector3 pos = axis * p_height * 0.5 + angle * p_radius;
            planes.push_back(Plane(pos, angle));
            planes.push_back(Plane(pos * axis_neg, angle * axis_neg));
        }
    }

    return planes;
}

struct _AtlasWorkRect {

    Size2i s;
    Point2i p;
    int idx;
    _FORCE_INLINE_ bool operator<(const _AtlasWorkRect &p_r) const { return s.width > p_r.s.width; }
};

struct _AtlasWorkRectResult {

    Vector<_AtlasWorkRect> result;
    int max_w;
    int max_h;
};

void Geometry::make_atlas(const Vector<Size2i> &p_rects, Vector<Point2i> &r_result, Size2i &r_size) {

    // Super simple, almost brute force scanline stacking fitter.
    // It's pretty basic for now, but it tries to make sure that the aspect ratio of the
    // resulting atlas is somehow square. This is necessary because video cards have limits.
    // On texture size (usually 2048 or 4096), so the more square a texture, the more chances.
    // It will work in every hardware.
    // For example, it will prioritize a 1024x1024 atlas (works everywhere) instead of a
    // 256x8192 atlas (won't work anywhere).

    ERR_FAIL_COND(p_rects.empty());
    for (int i = 0; i < p_rects.size(); i++) {
        ERR_FAIL_COND(p_rects[i].width <= 0);
        ERR_FAIL_COND(p_rects[i].height <= 0);
    }

    Vector<_AtlasWorkRect> wrects;
    wrects.resize(p_rects.size());
    for (size_t i = 0; i < p_rects.size(); i++) {
        wrects[i].s = p_rects[i];
        wrects[i].idx = i;
    }
    eastl::sort(wrects.begin(),wrects.end());
    int widest = wrects[0].s.width;

    Vector<_AtlasWorkRectResult> results;

    for (int i = 0; i <= 12; i++) {

        int w = 1 << i;
        int max_h = 0;
        int max_w = 0;
        if (w < widest) {
            continue;
        }

        Vector<int> hmax;
        hmax.resize(w,0);

        //place them
        int ofs = 0;
        int limit_h = 0;
        for (size_t j = 0; j < wrects.size(); j++) {

            const Size2i &wrect_sz(wrects[j].s);
            if (ofs + wrect_sz.width > w) {

                ofs = 0;
            }

            int from_y = 0;
            for (int k = 0; k < wrect_sz.width; k++) {

                if (hmax[ofs + k] > from_y)
                    from_y = hmax[ofs + k];
            }

            wrects[j].p.x = ofs;
            wrects[j].p.y = from_y;
            int end_h = from_y + wrect_sz.height;
            int end_w = ofs + wrect_sz.width;
            if (ofs == 0)
                limit_h = end_h;

            for (int k = 0; k < wrect_sz.width; k++) {

                hmax[ofs + k] = end_h;
            }

            if (end_h > max_h)
                max_h = end_h;

            if (end_w > max_w)
                max_w = end_w;

            if (ofs == 0 || end_h > limit_h) //while h limit not reached, keep stacking
                ofs += wrect_sz.width;
        }

        _AtlasWorkRectResult result;
        result.result = wrects;
        result.max_h = max_h;
        result.max_w = max_w;
        results.emplace_back(result);
    }

    //find the result with the best aspect ratio

    int best = -1;
    real_t best_aspect = 1e20f;

    for (size_t i = 0; i < results.size(); i++) {

        real_t h = next_power_of_2(results[i].max_h);
        real_t w = next_power_of_2(results[i].max_w);
        real_t aspect = h > w ? h / w : w / h;
        if (aspect < best_aspect) {
            best = i;
            best_aspect = aspect;
        }
    }

    r_result.resize(p_rects.size());

    for (size_t i = 0; i < p_rects.size(); i++) {

        r_result[results[best].result[i].idx] = results[best].result[i].p;
    }

    r_size = Size2(results[best].max_w, results[best].max_h);
}

Vector<Vector<Point2> > Geometry::_polypaths_do_operation(PolyBooleanOperation p_op, Span<const Point2> p_polypath_a, Span<const Point2> p_polypath_b, bool is_a_open) {

    using namespace ClipperLib;

    ClipType op = ctUnion;

    switch (p_op) {
        case OPERATION_UNION: op = ctUnion; break;
        case OPERATION_DIFFERENCE: op = ctDifference; break;
        case OPERATION_INTERSECTION: op = ctIntersection; break;
        case OPERATION_XOR: op = ctXor; break;
    }
    Path path_a, path_b;

    path_a.reserve(p_polypath_a.size());
    path_b.reserve(p_polypath_b.size());
    // Need to scale points (Clipper's requirement for robust computation)
    for (const Point2 p : p_polypath_a) {
        path_a.emplace_back(p.x * SCALE_FACTOR, p.y * SCALE_FACTOR);
    }
    for (const Point2 p : p_polypath_b) {
        path_b.emplace_back(p.x * SCALE_FACTOR, p.y * SCALE_FACTOR);
    }
    Clipper clp;
    clp.AddPath(path_a, ptSubject, !is_a_open); // forward compatible with Clipper 10.0.0
    clp.AddPath(path_b, ptClip, true); // polylines cannot be set as clip

    Paths paths;

    if (is_a_open) {
        PolyTree tree; // needed to populate polylines
        clp.Execute(op, tree);
        OpenPathsFromPolyTree(tree, paths);
    } else {
        clp.Execute(op, paths); // works on closed polygons only
    }
    // Have to scale points down now
    Vector<Vector<Vector2> > polypaths;

    for (Paths::size_type i = 0; i < paths.size(); ++i) {
        Vector<Vector2> polypath;

        const Path &scaled_path = paths[i];

        for (IntPoint j : scaled_path) {
            polypath.emplace_back(
                    static_cast<real_t>(j.X) / SCALE_FACTOR,
                    static_cast<real_t>(j.Y) / SCALE_FACTOR);
        }
        polypaths.emplace_back(eastl::move(polypath));
    }
    return polypaths;
}

Vector<Vector<Point2> > Geometry::_polypath_offset(const Vector<Point2> &p_polypath, real_t p_delta, PolyJoinType p_join_type, PolyEndType p_end_type) {

    using namespace ClipperLib;

    JoinType jt = jtSquare;

    switch (p_join_type) {
        case JOIN_SQUARE: jt = jtSquare; break;
        case JOIN_ROUND: jt = jtRound; break;
        case JOIN_MITER: jt = jtMiter; break;
    }

    EndType et = etClosedPolygon;

    switch (p_end_type) {
        case END_POLYGON: et = etClosedPolygon; break;
        case END_JOINED: et = etClosedLine; break;
        case END_BUTT: et = etOpenButt; break;
        case END_SQUARE: et = etOpenSquare; break;
        case END_ROUND: et = etOpenRound; break;
    }
    ClipperOffset co(2.0f, 0.25f * SCALE_FACTOR); // Defaults from ClipperOffset.
    Path path;
    path.reserve(p_polypath.size());
    // Need to scale points (Clipper's requirement for robust computation)
    for (size_t i = 0,fin=p_polypath.size(); i != fin; ++i) {
        path.emplace_back(p_polypath[i].x * SCALE_FACTOR, p_polypath[i].y * SCALE_FACTOR);
    }
    co.AddPath(path, jt, et);

    Paths paths;
    co.Execute(paths, p_delta * SCALE_FACTOR); // inflate/deflate

    // Have to scale points down now
    Vector<Vector<Point2> > polypaths;

    for (Paths::size_type i = 0; i < paths.size(); ++i) {
        Vector<Vector2> polypath;

        const Path &scaled_path = paths[i];

        for (IntPoint j : scaled_path) {
            polypath.emplace_back(
                    static_cast<real_t>(j.X) / SCALE_FACTOR,
                    static_cast<real_t>(j.Y) / SCALE_FACTOR);
        }
        polypaths.emplace_back(eastl::move(polypath));
    }
    return polypaths;
}
real_t Geometry::calculate_convex_hull_volume(const GeometryMeshData &p_md) {
    if (!p_md.vertices.size()) {
        return 0.0;
    }

    // find center
    Vector3 center;
    for (int n = 0; n < p_md.vertices.size(); n++) {
        center += p_md.vertices[n];
    }
    center /= p_md.vertices.size();

    Face3 fa;

    real_t volume = 0.0;

    // volume of each cone is 1/3 * height * area of face
    for (int f = 0; f < p_md.faces.size(); f++) {
        const GeometryMeshData::Face &face = p_md.faces[f];

        real_t height = 0.0;
        real_t face_area = 0.0;

        for (int c = 0; c < face.indices.size() - 2; c++) {
            fa.vertex[0] = p_md.vertices[face.indices[0]];
            fa.vertex[1] = p_md.vertices[face.indices[c + 1]];
            fa.vertex[2] = p_md.vertices[face.indices[c + 2]];

            if (!c) {
                // calculate height
                Plane plane(fa.vertex[0], fa.vertex[1], fa.vertex[2]);
                height = -plane.distance_to(center);
            }

            face_area += Math::sqrt(fa.get_twice_area_squared());
        }
        volume += face_area * height;
    }

    volume *= (1.0f / 3.0f) * 0.5f;
    return volume;
}

// note this function is slow, because it builds meshes etc. Not ideal to use in realtime.
// Planes must face OUTWARD from the center of the convex hull, by convention.
bool Geometry::convex_hull_intersects_convex_hull(const Plane *p_planes_a, int p_plane_count_a, const Plane *p_planes_b, int p_plane_count_b) {
    if (!p_plane_count_a || !p_plane_count_b) {
        return false;
    }

    // OR alternative approach, we can call compute_convex_mesh_points()
    // with both sets of planes, to get an intersection. Not sure which method is
    // faster... this may be faster with more complex hulls.

    // the usual silliness to get from one vector format to another...
    Vector<Plane> planes_a;
    Vector<Plane> planes_b;

    {
        planes_a.resize(p_plane_count_a);
        memcpy(planes_a.data(), p_planes_a, p_plane_count_a * sizeof(Plane));
    }
    {
        planes_b.resize(p_plane_count_b);
        memcpy(planes_b.data(), p_planes_b, p_plane_count_b * sizeof(Plane));
    }

    GeometryMeshData md_A = build_convex_mesh(planes_a);
    GeometryMeshData md_B = build_convex_mesh(planes_b);

    // hull can't be built
    if (!md_A.vertices.size() || !md_B.vertices.size()) {
        return false;
    }

    // first check the points against the planes
    for (int p = 0; p < p_plane_count_a; p++) {
        const Plane &plane = p_planes_a[p];

        for (int n = 0; n < md_B.vertices.size(); n++) {
            if (!plane.is_point_over(md_B.vertices[n])) {
                return true;
            }
        }
    }

    for (int p = 0; p < p_plane_count_b; p++) {
        const Plane &plane = p_planes_b[p];

        for (int n = 0; n < md_A.vertices.size(); n++) {
            if (!plane.is_point_over(md_A.vertices[n])) {
                return true;
            }
        }
    }

    // now check edges
    for (int n = 0; n < md_A.edges.size(); n++) {
        const Vector3 &pt_a = md_A.vertices[md_A.edges[n].a];
        const Vector3 &pt_b = md_A.vertices[md_A.edges[n].b];

        if (segment_intersects_convex(pt_a, pt_b, p_planes_b, p_plane_count_b, nullptr, nullptr)) {
            return true;
        }
    }

    for (int n = 0; n < md_B.edges.size(); n++) {
        const Vector3 &pt_a = md_B.vertices[md_B.edges[n].a];
        const Vector3 &pt_b = md_B.vertices[md_B.edges[n].b];

        if (segment_intersects_convex(pt_a, pt_b, p_planes_a, p_plane_count_a, nullptr, nullptr)) {
            return true;
        }
    }

    return false;
}
FixedVector<Vector3,8,false> Geometry::compute_convex_mesh_points_6(Span<const Plane,6> p_planes, real_t p_epsilon) {

    FixedVector<Vector3,8,false> points;

    // Iterate through every unique combination of any three planes.
    for (int i = p_planes.size() - 1; i >= 0; i--) {
        for (int j = i - 1; j >= 0; j--) {
            for (int k = j - 1; k >= 0; k--) {

                // Find the point where these planes all cross over (if they
                // do at all).
                Vector3 convex_shape_point;
                if (p_planes[i].intersect_3(p_planes[j], p_planes[k], &convex_shape_point)) {

                    // See if any *other* plane excludes this point because it's
                    // on the wrong side.
                    bool excluded = false;
                    for (int n = 0; n < p_planes.size(); n++) {
                        if (n != i && n != j && n != k) {
                            real_t dist = p_planes[n].normal.dot(convex_shape_point);
                            if (dist - p_planes[n].d > p_epsilon) {
                                excluded = true;
                                break;
                            }
                        }
                    }

                    // Only add the point if it passed all tests.
                    if (!excluded) {
                        points.push_back(convex_shape_point);
                    }
                }
            }
        }
    }

    return points;
}
Vector<Vector3> Geometry::compute_convex_mesh_points(Span<const Plane> p_planes, real_t p_epsilon) {
    Vector<Vector3> points;

    // Iterate through every unique combination of any three planes.
    for (int i = p_planes.size() - 1; i >= 0; i--) {
        for (int j = i - 1; j >= 0; j--) {
            for (int k = j - 1; k >= 0; k--) {
                // Find the point where these planes all cross over (if they
                // do at all).
                Vector3 convex_shape_point;
                if (p_planes[i].intersect_3(p_planes[j], p_planes[k], &convex_shape_point)) {
                    // See if any *other* plane excludes this point because it's
                    // on the wrong side.
                    bool excluded = false;
                    for (int n = 0; n < p_planes.size(); n++) {
                        if (n != i && n != j && n != k) {
                            real_t dist = p_planes[n].normal.dot(convex_shape_point);
                            if (dist - p_planes[n].d > p_epsilon) {
                                excluded = true;
                                break;
                            }
                        }
                    }

                    // Only add the point if it passed all tests.
                    if (!excluded) {
                        points.push_back(convex_shape_point);
                    }
                }
            }
        }
    }

    return points;
}

// Expects polygon as a triangle fan
real_t Geometry::find_polygon_area(Span<const Vector3> p_verts) {
    if (p_verts.size() < 3) {
        return 0.0;
    }

    Face3 f;
    f.vertex[0] = p_verts[0];
    f.vertex[1] = p_verts[1];
    f.vertex[2] = p_verts[1];

    real_t area = 0.0;

    for (int n = 2,fin=p_verts.size(); n < fin; n++) {
        f.vertex[1] = f.vertex[2];
        f.vertex[2] = p_verts[n];
        area += Math::sqrt(f.get_twice_area_squared());
    }

    return area * 0.5f;
}

Vector<Geometry::PackRectsResult> Geometry::partial_pack_rects(Span<const Vector2i> p_sizes, const Size2i &p_atlas_size) {

    Vector<stbrp_node> nodes;
    nodes.resize(p_atlas_size.width);
    memset(nodes.data(),0, sizeof(stbrp_node) * nodes.size());

    stbrp_context context;
    stbrp_init_target(&context, p_atlas_size.width, p_atlas_size.height, nodes.data(), p_atlas_size.width);

    Vector<stbrp_rect> rects;
    rects.reserve(p_sizes.size());

    for (int i = 0; i < (int)p_sizes.size(); i++) {
        stbrp_rect val{ i, (unsigned short)p_sizes[i].width, (unsigned short)p_sizes[i].height, 0, 0, 0 };
        rects.emplace_back(val);
    }

    stbrp_pack_rects(&context, rects.data(), rects.size());

    Vector<PackRectsResult> ret;
    ret.resize(p_sizes.size());

    for (int i = 0; i < p_sizes.size(); i++) {
        ret[rects[i].id] = { rects[i].x, rects[i].y, static_cast<bool>(rects[i].was_packed) };
    }

    return ret;
}

// adapted from:
// https://stackoverflow.com/questions/6989100/sort-points-in-clockwise-order
void Geometry::sort_polygon_winding(Vector<Vector2> &r_verts, bool p_clockwise) {
    // sort winding order of a (primarily convex) polygon.
    // It can handle some concave polygons, but not
    // where a vertex 'goes back on' a previous vertex ..
    // i.e. it will change the shape in some concave cases.

    struct ElementComparator {
        Vector2 center;
        bool reverse;
        bool operator()(const Vector2 &a, const Vector2 &b) const {
            if (a.x - center.x >= 0 && b.x - center.x < 0) {
                return true ^ reverse;
            }
            if (a.x - center.x < 0 && b.x - center.x >= 0) {
                return false ^ reverse;
            }
            if (a.x - center.x == 0 && b.x - center.x == 0) {
                if (a.y - center.y >= 0 || b.y - center.y >= 0) {
                    return (a.y > b.y) ^ reverse;
                }
                return (b.y > a.y) ^ reverse;
            }

            // compute the cross product of vectors (center -> a) x (center -> b)
            real_t det = (a.x - center.x) * (b.y - center.y) - (b.x - center.x) * (a.y - center.y);
            if (det < 0.0f) {
                return true ^ reverse;
            }
            if (det > 0.0f) {
                return false ^ reverse;
            }

            // points a and b are on the same line from the center
            // check which point is closer to the center
            real_t d1 = (a.x - center.x) * (a.x - center.x) + (a.y - center.y) * (a.y - center.y);
            real_t d2 = (b.x - center.x) * (b.x - center.x) + (b.y - center.y) * (b.y - center.y);
            return (d1 > d2) ^ reverse;
        }
    };

    int npoints = r_verts.size();
    if (!npoints) {
        return;
    }

    // first calculate center
    Vector2 center;
    for (int n = 0; n < npoints; n++) {
        center += r_verts[n];
    }
    center /= npoints;
    // if not clockwise, reverse order
    ElementComparator cmp { center, !p_clockwise };
    eastl::sort(r_verts.begin(),r_verts.end(),cmp);
}

real_t Geometry::get_closest_points_between_segments(Vector2 p1, Vector2 q1, Vector2 p2, Vector2 q2, Vector2 &c1, Vector2 &c2) {
    const Vector2 d1 = q1 - p1; // Direction vector of segment S1.
    const Vector2 d2 = q2 - p2; // Direction vector of segment S2.
    const Vector2 r = p1 - p2;
    real_t a = d1.dot(d1); // Squared length of segment S1, always nonnegative.
    real_t e = d2.dot(d2); // Squared length of segment S2, always nonnegative.
    real_t f = d2.dot(r);
    real_t s, t;
    // Check if either or both segments degenerate into points.
    if (a <= CMP_EPSILON && e <= CMP_EPSILON) {
        // Both segments degenerate into points.
        c1 = p1;
        c2 = p2;
        return Math::sqrt((c1 - c2).dot(c1 - c2));
    }
    if (a <= CMP_EPSILON) {
        // First segment degenerates into a point.
        s = 0.0;
        t = f / e; // s = 0 => t = (b*s + f) / e = f / e
        t = CLAMP(t, 0.0f, 1.0f);
    } else {
        real_t c = d1.dot(r);
        if (e <= CMP_EPSILON) {
            // Second segment degenerates into a point.
            t = 0.0;
            s = CLAMP(-c / a, 0.0f, 1.0f); // t = 0 => s = (b*t - c) / a = -c / a
        } else {
            // The general nondegenerate case starts here.
            real_t b = d1.dot(d2);
            real_t denom = a * e - b * b; // Always nonnegative.
            // If segments not parallel, compute closest point on L1 to L2 and
            // clamp to segment S1. Else pick arbitrary s (here 0).
            if (denom != 0.0f) {
                s = CLAMP((b * f - c * e) / denom, 0.0f, 1.0f);
            } else
                s = 0.0;
            // Compute point on L2 closest to S1(s) using
            // t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
            t = (b * s + f) / e;

            // If t in [0,1] done. Else clamp t, recompute s for the new value
            //  of t using s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a
            //  and clamp s to [0, 1].
            if (t < 0.0f) {
                t = 0.0;
                s = CLAMP(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0;
                s = CLAMP((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
    return Math::sqrt((c1 - c2).dot(c1 - c2));
}

real_t get_closest_points_between_segments(Vector2 p1, Vector2 q1, Vector2 p2, Vector2 q2, Vector2 &c1, Vector2 &c2) {

    const Vector2 d1 = q1 - p1; // Direction vector of segment S1.
    const Vector2 d2 = q2 - p2; // Direction vector of segment S2.
    const Vector2 r = p1 - p2;
    real_t a = d1.dot(d1); // Squared length of segment S1, always nonnegative.
    real_t e = d2.dot(d2); // Squared length of segment S2, always nonnegative.
    real_t f = d2.dot(r);
    real_t s, t;
    // Check if either or both segments degenerate into points.
    if (a <= CMP_EPSILON && e <= CMP_EPSILON) {
        // Both segments degenerate into points.
        c1 = p1;
        c2 = p2;
        return Math::sqrt((c1 - c2).dot(c1 - c2));
    }
    if (a <= CMP_EPSILON) {
        // First segment degenerates into a point.
        s = 0.0;
        t = f / e; // s = 0 => t = (b*s + f) / e = f / e
        t = CLAMP(t, 0.0f, 1.0f);
    } else {
        real_t c = d1.dot(r);
        if (e <= CMP_EPSILON) {
            // Second segment degenerates into a point.
            t = 0.0;
            s = CLAMP(-c / a, 0.0f, 1.0f); // t = 0 => s = (b*t - c) / a = -c / a
        } else {
            // The general nondegenerate case starts here.
            real_t b = d1.dot(d2);
            real_t denom = a * e - b * b; // Always nonnegative.
            // If segments not parallel, compute closest point on L1 to L2 and
            // clamp to segment S1. Else pick arbitrary s (here 0).
            if (denom != 0.0f) {
                s = CLAMP((b * f - c * e) / denom, 0.0f, 1.0f);
            } else
                s = 0.0;
            // Compute point on L2 closest to S1(s) using
            // t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
            t = (b * s + f) / e;

            //If t in [0,1] done. Else clamp t, recompute s for the new value
            // of t using s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a
            // and clamp s to [0, 1].
            if (t < 0.0f) {
                t = 0.0;
                s = CLAMP(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0;
                s = CLAMP((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
    return Math::sqrt((c1 - c2).dot(c1 - c2));
}

bool Geometry::ray_intersects_triangle(
        const Vector3 &p_from, const Vector3 &p_dir, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, Vector3 *r_res) {
    Vector3 e1 = p_v1 - p_v0;
    Vector3 e2 = p_v2 - p_v0;
    Vector3 h = p_dir.cross(e2);
    real_t a = e1.dot(h);
    if (Math::is_zero_approx(a)) { // Parallel test.
        return false;
    }

    real_t f = 1.0f / a;

    Vector3 s = p_from - p_v0;
    real_t u = f * s.dot(h);

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    Vector3 q = s.cross(e1);

    real_t v = f * p_dir.dot(q);

    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    // At this stage we can compute t to find out where
    // the intersection point is on the line.
    real_t t = f * e2.dot(q);

    if (t > 0.00001f) { // ray intersection
        if (r_res) {
            *r_res = p_from + p_dir * t;
        }
        return true;
    } else // This means that there is a line intersection but not a ray intersection.
        return false;
}

bool Geometry::is_point_in_polygon(Vector2 p_point, Span<const Vector2> p_polygon) {
    int c = p_polygon.size();
    if (c < 3)
        return false;
    const Vector2 *p = p_polygon.data();
    Vector2 further_away(-1e20f, -1e20f);
    Vector2 further_away_opposite(1e20f, 1e20f);

    for (Vector2 pv : p_polygon) {
        further_away.x = M_MAX(pv.x, further_away.x);
        further_away.y = M_MAX(pv.y, further_away.y);
        further_away_opposite.x = MIN(pv.x, further_away_opposite.x);
        further_away_opposite.y = MIN(pv.y, further_away_opposite.y);
    }
    // Make point outside that won't intersect with points in segment from p_point.
    further_away += (further_away - further_away_opposite) * Vector2(1.221313f, 1.512312f);

    int intersections = 0;
    for (int i = 0; i < c; i++) {
        const Vector2 &v1 = p[i];
        const Vector2 &v2 = p[(i + 1) % c];
        if (segment_intersects_segment_2d(v1, v2, p_point, further_away, nullptr)) {
            intersections++;
        }
    }

    return (intersections & 1);
}

Vector<int> Geometry::triangulate_delaunay_2d(Span<const Vector2> p_points) {

    Vector<Delaunay2D::Triangle> tr(Delaunay2D::triangulate(p_points));
    Vector<int> triangles;
    triangles.reserve(tr.size());

    for (const Delaunay2D::Triangle &dt: tr) {
        triangles.push_back(dt.points[0]);
        triangles.push_back(dt.points[1]);
        triangles.push_back(dt.points[2]);
    }
    return triangles;
}

bool Geometry::segment_intersects_triangle(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, Vector3 *r_res) {

    Vector3 rel = p_to - p_from;
    Vector3 e1 = p_v1 - p_v0;
    Vector3 e2 = p_v2 - p_v0;
    Vector3 h = rel.cross(e2);
    real_t a = e1.dot(h);
    if (Math::is_zero_approx(a)) // Parallel test.
        return false;

    real_t f = 1.0f / a;

    Vector3 s = p_from - p_v0;
    real_t u = f * s.dot(h);

    if (u < 0.0f || u > 1.0f)
        return false;

    Vector3 q = s.cross(e1);

    real_t v = f * rel.dot(q);

    if (v < 0.0f || (u + v > 1.0f))
        return false;

    // At this stage we can compute t to find out where
    // the intersection point is on the line.
    real_t t = f * e2.dot(q);

    if (t > CMP_EPSILON && t <= 1.0f) { // Ray intersection.
        if (r_res)
            *r_res = p_from + rel * t;
        return true;
    } else // This means that there is a line intersection but not a ray intersection.
        return false;
}

bool Geometry::segment_intersects_sphere(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_sphere_pos, real_t p_sphere_radius, Vector3 *r_res, Vector3 *r_norm) {

    Vector3 sphere_pos = p_sphere_pos - p_from;
    Vector3 rel = (p_to - p_from);
    real_t rel_l = rel.length();
    if (rel_l < CMP_EPSILON)
        return false; // Both points are the same.
    Vector3 normal = rel / rel_l;

    real_t sphere_d = normal.dot(sphere_pos);

    real_t ray_distance = sphere_pos.distance_to(normal * sphere_d);

    if (ray_distance >= p_sphere_radius)
        return false;

    real_t inters_d2 = p_sphere_radius * p_sphere_radius - ray_distance * ray_distance;
    real_t inters_d = sphere_d;

    if (inters_d2 >= CMP_EPSILON)
        inters_d -= Math::sqrt(inters_d2);

    // Check in segment.
    if (inters_d < 0 || inters_d > rel_l)
        return false;

    Vector3 result = p_from + normal * inters_d;

    if (r_res)
        *r_res = result;
    if (r_norm)
        *r_norm = (result - p_sphere_pos).normalized();

    return true;
}

bool Geometry::segment_intersects_cylinder(const Vector3 &p_from, const Vector3 &p_to, real_t p_height, real_t p_radius, Vector3 *r_res, Vector3 *r_norm, int p_cylinder_axis) {

    Vector3 rel = (p_to - p_from);
    real_t rel_l = rel.length();
    if (rel_l < CMP_EPSILON)
        return false; // Both points are the same.

    ERR_FAIL_COND_V(p_cylinder_axis < 0 || p_cylinder_axis > 2, false);
    Vector3 cylinder_axis;
    cylinder_axis[p_cylinder_axis] = 1.0;
    // First check if they are parallel.
    Vector3 normal = (rel / rel_l);
    Vector3 crs = normal.cross(cylinder_axis);
    real_t crs_l = crs.length();

    Vector3 axis_dir;

    if (crs_l < CMP_EPSILON) {
        Vector3 side_axis;
        side_axis[(p_cylinder_axis + 1) % 3] = 1.0; // Any side axis OK.
        axis_dir = side_axis;
    } else {
        axis_dir = crs / crs_l;
    }

    real_t dist = axis_dir.dot(p_from);

    if (dist >= p_radius)
        return false; // Too far away.

    // Convert to 2D.
    real_t w2 = p_radius * p_radius - dist * dist;
    if (w2 < CMP_EPSILON)
        return false; // Avoid numerical error.
    Size2 size(Math::sqrt(w2), p_height * 0.5f);

    Vector3 side_dir = axis_dir.cross(cylinder_axis).normalized();

    Vector2 from2D(side_dir.dot(p_from), p_from[p_cylinder_axis]);
    Vector2 to2D(side_dir.dot(p_to), p_to[p_cylinder_axis]);

    real_t min = 0, max = 1;

    int axis = -1;

    for (int i = 0; i < 2; i++) {

        real_t seg_from = from2D[i];
        real_t seg_to = to2D[i];
        real_t box_begin = -size[i];
        real_t box_end = size[i];
        real_t cmin, cmax;

        if (seg_from < seg_to) {

            if (seg_from > box_end || seg_to < box_begin)
                return false;
            real_t length = seg_to - seg_from;
            cmin = (seg_from < box_begin) ? ((box_begin - seg_from) / length) : 0;
            cmax = (seg_to > box_end) ? ((box_end - seg_from) / length) : 1;

        } else {

            if (seg_to > box_end || seg_from < box_begin)
                return false;
            real_t length = seg_to - seg_from;
            cmin = (seg_from > box_end) ? (box_end - seg_from) / length : 0;
            cmax = (seg_to < box_begin) ? (box_begin - seg_from) / length : 1;
        }

        if (cmin > min) {
            min = cmin;
            axis = i;
        }
        if (cmax < max)
            max = cmax;
        if (max < min)
            return false;
    }

    // Convert to 3D again.
    Vector3 result = p_from + (rel * min);
    Vector3 res_normal = result;

    if (axis == 0) {
        res_normal[p_cylinder_axis] = 0;
    } else {
        int axis_side = (p_cylinder_axis + 1) % 3;
        res_normal[axis_side] = 0;
        axis_side = (axis_side + 1) % 3;
        res_normal[axis_side] = 0;
    }

    res_normal.normalize();

    if (r_res)
        *r_res = result;
    if (r_norm)
        *r_norm = res_normal;

    return true;
}

bool Geometry::segment_intersects_convex(const Vector3 &p_from, const Vector3 &p_to, const Plane *p_planes, int p_plane_count, Vector3 *p_res, Vector3 *p_norm) {

    real_t min = -1e20f;
    real_t max = 1e20f;

    Vector3 rel = p_to - p_from;
    real_t rel_l = rel.length();

    if (rel_l < CMP_EPSILON)
        return false;

    Vector3 dir = rel / rel_l;

    int min_index = -1;

    for (int i = 0; i < p_plane_count; i++) {

        const Plane &p = p_planes[i];

        real_t den = p.normal.dot(dir);

        if (Math::abs(den) <= CMP_EPSILON)
            continue; // Ignore parallel plane.

        real_t dist = -p.distance_to(p_from) / den;

        if (den > 0) {
            // Backwards facing plane.
            if (dist < max)
                max = dist;
        } else {

            // Front facing plane.
            if (dist > min) {
                min = dist;
                min_index = i;
            }
        }
    }

    if (max <= min || min < 0 || min > rel_l || min_index == -1) // Exit conditions.
        return false; // No intersection.

    if (p_res)
        *p_res = p_from + dir * min;
    if (p_norm)
        *p_norm = p_planes[min_index].normal;

    return true;
}

Vector3 Geometry::get_closest_point_to_segment(const Vector3 &p_point, const Vector3 *p_segment) {

    Vector3 p = p_point - p_segment[0];
    Vector3 n = p_segment[1] - p_segment[0];
    real_t l2 = n.length_squared();
    if (l2 < 1e-20f)
        return p_segment[0]; // Both points are the same, just give any.

    real_t d = n.dot(p) / l2;

    if (d <= 0.0f)
        return p_segment[0]; // Before first point.
    else if (d >= 1.0f)
        return p_segment[1]; // After first point.
    else
        return p_segment[0] + n * d; // Inside.
}

Vector3 Geometry::get_closest_point_to_segment_uncapped(const Vector3 &p_point, const Vector3 *p_segment) {

    Vector3 p = p_point - p_segment[0];
    Vector3 n = p_segment[1] - p_segment[0];
    real_t l2 = n.length_squared();
    if (l2 < 1e-20f)
        return p_segment[0]; // Both points are the same, just give any.

    real_t d = n.dot(p) / l2;

    return p_segment[0] + n * d; // Inside.
}

Vector2 Geometry::get_closest_point_to_segment_2d(const Vector2 &p_point, const Vector2 *p_segment) {

    Vector2 p = p_point - p_segment[0];
    Vector2 n = p_segment[1] - p_segment[0];
    real_t l2 = n.length_squared();
    if (l2 < 1e-20f)
        return p_segment[0]; // Both points are the same, just give any.

    real_t d = n.dot(p) / l2;

    if (d <= 0.0f)
        return p_segment[0]; // Before first point.
    else if (d >= 1.0f)
        return p_segment[1]; // After first point.
    else
        return p_segment[0] + n * d; // Inside.
}

bool Geometry::is_point_in_triangle(const Vector2 &s, const Vector2 &a, const Vector2 &b, const Vector2 &c) {
    Vector2 an = a - s;
    Vector2 bn = b - s;
    Vector2 cn = c - s;

    bool orientation = an.cross(bn) > 0;

    if ((bn.cross(cn) > 0) != orientation) return false;

    return (cn.cross(an) > 0) == orientation;
}

Vector3 Geometry::barycentric_coordinates_2d(Vector2 s, Vector2 a, Vector2 b, Vector2 c) {
    // http://www.blackpawn.com/texts/pointinpoly/
    const Vector2 v0 = c - a;
    const Vector2 v1 = b - a;
    const Vector2 v2 = s - a;

    // Compute dot products
    const auto dot00 = v0.dot(v0);
    const auto dot01 = v0.dot(v1);
    const auto dot02 = v0.dot(v2);
    const auto dot11 = v1.dot(v1);
    const auto dot12 = v1.dot(v2);

    // Check for divide by zero
    float denom = dot00 * dot11 - dot01 * dot01;
    if (denom == 0.0) {
        return Vector3(0.0, 0.0, 0.0);
    }
    // Compute barycentric coordinates
    const auto invDenom = 1.0f / denom;
    const auto b2 = (dot11 * dot02 - dot01 * dot12) * invDenom;
    const auto b1 = (dot00 * dot12 - dot01 * dot02) * invDenom;
    const auto b0 = 1.0f - b2 - b1;
    return Vector3(b0, b1, b2);
}

Vector2 Geometry::get_closest_point_to_segment_uncapped_2d(const Vector2 &p_point, const Vector2 *p_segment) {

    Vector2 p = p_point - p_segment[0];
    Vector2 n = p_segment[1] - p_segment[0];
    real_t l2 = n.length_squared();
    if (l2 < 1e-20f)
        return p_segment[0]; // Both points are the same, just give any.

    real_t d = n.dot(p) / l2;

    return p_segment[0] + n * d; // Inside.
}

bool Geometry::line_intersects_line_2d(const Vector2 &p_from_a, const Vector2 &p_dir_a, const Vector2 &p_from_b, const Vector2 &p_dir_b, Vector2 &r_result) {

    // See http://paulbourke.net/geometry/pointlineplane/

    const real_t denom = p_dir_b.y * p_dir_a.x - p_dir_b.x * p_dir_a.y;
    if (Math::is_zero_approx(denom)) { // Parallel?
        return false;
    }

    const Vector2 v = p_from_a - p_from_b;
    const real_t t = (p_dir_b.x * v.y - p_dir_b.y * v.x) / denom;
    r_result = p_from_a + t * p_dir_a;
    return true;
}

bool Geometry::segment_intersects_segment_2d(const Vector2 &p_from_a, const Vector2 &p_to_a, const Vector2 &p_from_b, const Vector2 &p_to_b, Vector2 *r_result) {

    Vector2 B = p_to_a - p_from_a;
    Vector2 C = p_from_b - p_from_a;
    Vector2 D = p_to_b - p_from_a;

    real_t ABlen = B.dot(B);
    if (ABlen <= 0)
        return false;
    Vector2 Bn = B / ABlen;
    C = Vector2(C.x * Bn.x + C.y * Bn.y, C.y * Bn.x - C.x * Bn.y);
    D = Vector2(D.x * Bn.x + D.y * Bn.y, D.y * Bn.x - D.x * Bn.y);

    if ((C.y < 0 && D.y < 0) || (C.y >= 0 && D.y >= 0))
        return false;

    real_t ABpos = D.x + (C.x - D.x) * D.y / (D.y - C.y);

    //  Fail if segment C-D crosses line A-B outside of segment A-B.
    if (ABpos < 0 || ABpos > 1.0f)
        return false;

    //  (4) Apply the discovered position to line A-B in the original coordinate system.
    if (r_result)
        *r_result = p_from_a + B * ABpos;

    return true;
}

bool Geometry::point_in_projected_triangle(const Vector3 &p_point, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3) {

    Vector3 face_n = (p_v1 - p_v3).cross(p_v1 - p_v2);

    Vector3 n1 = (p_point - p_v3).cross(p_point - p_v2);

    if (face_n.dot(n1) < 0)
        return false;

    Vector3 n2 = (p_v1 - p_v3).cross(p_v1 - p_point);

    if (face_n.dot(n2) < 0)
        return false;

    Vector3 n3 = (p_v1 - p_point).cross(p_v1 - p_v2);

    return face_n.dot(n3) >= 0;
}

bool Geometry::triangle_sphere_intersection_test(const Vector3 *p_triangle, const Vector3 &p_normal, const Vector3 &p_sphere_pos, real_t p_sphere_radius, Vector3 &r_triangle_contact, Vector3 &r_sphere_contact) {

    real_t d = p_normal.dot(p_sphere_pos) - p_normal.dot(p_triangle[0]);

    if (d > p_sphere_radius || d < -p_sphere_radius) // Not touching the plane of the face, return.
        return false;

    Vector3 contact = p_sphere_pos - (p_normal * d);

    /** 2nd) TEST INSIDE TRIANGLE **/

    if (Geometry::point_in_projected_triangle(contact, p_triangle[0], p_triangle[1], p_triangle[2])) {
        r_triangle_contact = contact;
        r_sphere_contact = p_sphere_pos - p_normal * p_sphere_radius;
        //printf("solved inside triangle\n");
        return true;
    }

    /** 3rd TEST INSIDE EDGE CYLINDERS **/

    const Vector3 verts[4] = { p_triangle[0], p_triangle[1], p_triangle[2], p_triangle[0] }; // for() friendly

    for (int i = 0; i < 3; i++) {

        // Check edge cylinder.

        Vector3 n1 = verts[i] - verts[i + 1];
        Vector3 n2 = p_sphere_pos - verts[i + 1];

        ///@TODO Maybe discard by range here to make the algorithm quicker.

        // Check point within cylinder radius.
        Vector3 axis = n1.cross(n2).cross(n1);
        axis.normalize();

        real_t ad = axis.dot(n2);

        if (ABS(ad) > p_sphere_radius) {
            // No chance with this edge, too far away.
            continue;
        }

        // Check point within edge capsule cylinder.
        /** 4th TEST INSIDE EDGE POINTS **/

        real_t sphere_at = n1.dot(n2);

        if (sphere_at >= 0 && sphere_at < n1.dot(n1)) {

            r_triangle_contact = p_sphere_pos - axis * (axis.dot(n2));
            r_sphere_contact = p_sphere_pos - axis * p_sphere_radius;
            // Point inside here.
            return true;
        }

        real_t r2 = p_sphere_radius * p_sphere_radius;

        if (n2.length_squared() < r2) {

            Vector3 n = (p_sphere_pos - verts[i + 1]).normalized();

            r_triangle_contact = verts[i + 1];
            r_sphere_contact = p_sphere_pos - n * p_sphere_radius;
            return true;
        }

        if (n2.distance_squared_to(n1) < r2) {
            Vector3 n = (p_sphere_pos - verts[i]).normalized();

            r_triangle_contact = verts[i];
            r_sphere_contact = p_sphere_pos - n * p_sphere_radius;
            return true;
        }

        break; // It's pointless to continue at this point, so save some CPU cycles.
    }

    return false;
}

real_t Geometry::segment_intersects_circle(const Vector2 &p_from, const Vector2 &p_to, const Vector2 &p_circle_pos, real_t p_circle_radius) {

    Vector2 line_vec = p_to - p_from;
    Vector2 vec_to_line = p_from - p_circle_pos;

    // Create a quadratic formula of the form ax^2 + bx + c = 0
    real_t a, b, c;

    a = line_vec.dot(line_vec);
    b = 2 * vec_to_line.dot(line_vec);
    c = vec_to_line.dot(vec_to_line) - p_circle_radius * p_circle_radius;

    // Solve for t.
    real_t sqrtterm = b * b - 4 * a * c;

    // If the term we intend to square root is less than 0 then the answer won't be real,
    // so it definitely won't be t in the range 0 to 1.
    if (sqrtterm < 0) return -1;

    // If we can assume that the line segment starts outside the circle (e.g. for continuous time collision detection)
    // then the following can be skipped and we can just return the equivalent of res1.
    sqrtterm = Math::sqrt(sqrtterm);
    real_t res1 = (-b - sqrtterm) / (2 * a);
    real_t res2 = (-b + sqrtterm) / (2 * a);

    if (res1 >= 0 && res1 <= 1) return res1;
    if (res2 >= 0 && res2 <= 1) return res2;
    return -1;
}

Vector<Vector3> Geometry::clip_polygon(Span<const Vector3> &polygon, const Plane &p_plane) {

    enum LocationCache {
        LOC_INSIDE = 1,
        LOC_BOUNDARY = 0,
        LOC_OUTSIDE = -1
    };
    size_t poly_count = polygon.size();
    if (poly_count == 0)
        return {};

    int *location_cache = (int *)alloca(sizeof(int) * poly_count);
    int inside_count = 0;
    int outside_count = 0;

    for (size_t a = 0; a < poly_count; a++) {
        real_t dist = p_plane.distance_to(polygon[a]);
        if (dist < -CMP_POINT_IN_PLANE_EPSILON) {
            location_cache[a] = LOC_INSIDE;
            inside_count++;
        } else {
            if (dist > CMP_POINT_IN_PLANE_EPSILON) {
                location_cache[a] = LOC_OUTSIDE;
                outside_count++;
            } else {
                location_cache[a] = LOC_BOUNDARY;
            }
        }
    }

    if (outside_count == 0) {

        return {polygon.begin(),polygon.end()}; // No changes.

    }
    if (inside_count == 0) {

        return {}; // Empty.
    }

    long previous = polygon.size() - 1;
    Vector<Vector3> clipped;
    clipped.reserve(polygon.size()/2);
    for (int index = 0; index < polygon.size(); index++) {
        int loc = location_cache[index];
        if (loc == LOC_OUTSIDE) {
            if (location_cache[previous] == LOC_INSIDE) {
                const Vector3 &v1 = polygon[previous];
                const Vector3 &v2 = polygon[index];

                Vector3 segment = v1 - v2;
                real_t den = p_plane.normal.dot(segment);
                real_t dist = p_plane.distance_to(v1) / den;
                dist = -dist;
                clipped.push_back(v1 + segment * dist);
            }
        } else {
            const Vector3 &v1 = polygon[index];
            if ((loc == LOC_INSIDE) && (location_cache[previous] == LOC_OUTSIDE)) {
                const Vector3 &v2 = polygon[previous];
                Vector3 segment = v1 - v2;
                real_t den = p_plane.normal.dot(segment);
                real_t dist = p_plane.distance_to(v1) / den;
                dist = -dist;
                clipped.push_back(v1 + segment * dist);
            }

            clipped.push_back(v1);
        }

        previous = index;
    }

    return clipped;
}

bool Geometry::is_polygon_clockwise(Span<const Vector2> p_polygon) {
    int c = p_polygon.size();
    if (c < 3)
        return false;
    const Vector2 *p = p_polygon.data();
    real_t sum = 0;
    for (int i = 0; i < c; i++) {
        const Vector2 &v1 = p[i];
        const Vector2 &v2 = p[(i + 1) % c];
        sum += (v2.x - v1.x) * (v2.y + v1.y);
    }

    return sum > 0.0f;
}

Vector<Vector<Point2> > Geometry::merge_polygons_2d(const Vector<Point2> &p_polygon_a, Span<const Vector2> p_polygon_b) {

    return _polypaths_do_operation(OPERATION_UNION, p_polygon_a, p_polygon_b);
}

Vector<Vector<Point2>> Geometry::clip_polygons_2d(const Vector<Point2> &p_polygon_a, Span<const Vector2> p_polygon_b) {
    return _polypaths_do_operation(OPERATION_DIFFERENCE, p_polygon_a, p_polygon_b);
}

Vector<Vector<Point2>> Geometry::intersect_polygons_2d(Span<const Vector2> p_polygon_a, Span<const Vector2> p_polygon_b) {
    return _polypaths_do_operation(OPERATION_INTERSECTION, p_polygon_a, p_polygon_b);
}

Vector<Vector<Point2>> Geometry::exclude_polygons_2d(const Vector<Point2> &p_polygon_a, const Vector<Point2> &p_polygon_b) {
    return _polypaths_do_operation(OPERATION_XOR, p_polygon_a, p_polygon_b);
}

Vector<Vector<Point2>> Geometry::clip_polyline_with_polygon_2d(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon) {
    return _polypaths_do_operation(OPERATION_DIFFERENCE, p_polyline, p_polygon, true);
}

Vector<Vector<Point2>> Geometry::intersect_polyline_with_polygon_2d(const Vector<Vector2> &p_polyline, Span<const Vector2> p_polygon) {
    return _polypaths_do_operation(OPERATION_INTERSECTION, p_polyline, p_polygon, true);
}

Vector<Vector<Point2>> Geometry::offset_polygon_2d(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type) {
    return _polypath_offset(p_polygon, p_delta, p_join_type, END_POLYGON);
}

Vector<Vector<Point2>> Geometry::offset_polyline_2d(
        const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type, PolyEndType p_end_type) {
    ERR_FAIL_COND_V_MSG(
            p_end_type == END_POLYGON, Vector<Vector<Point2>>(), "Attempt to offset a polyline like a polygon (use offset_polygon_2d instead).");

    return _polypath_offset(p_polygon, p_delta, p_join_type, p_end_type);
}

Vector<int> Geometry::triangulate_polygon(Span<const Vector2> p_polygon) {
    Vector<int> triangles;
    if (!Triangulate::triangulate(p_polygon, triangles))
        return Vector<int>(); // fail
    return triangles;
}
bool Geometry::is_point_in_circle(const Vector2 &p_point, const Vector2 &p_circle_pos, real_t p_circle_radius) {
    return p_point.distance_squared_to(p_circle_pos) <= p_circle_radius * p_circle_radius;
}
