/*************************************************************************/
/*  quick_hull.cpp                                                       */
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

#include "quick_hull.h"

#include "core/map.h"
#include "core/set.h"
#include "core/math/aabb.h"

#include "EASTL/sort.h"
uint32_t QuickHull::debug_stop_after = 0xFFFFFFFF;
namespace {
struct QHEdge {

    union {
        uint32_t vertices[2];
        uint64_t id;
    };

    bool operator<(const QHEdge &p_edge) const {
        return id < p_edge.id;
    }

    QHEdge(uint32_t p_vtx_a = 0, uint32_t p_vtx_b = 0) {

        if (p_vtx_a > p_vtx_b) {
            SWAP(p_vtx_a, p_vtx_b);
        }

        vertices[0] = p_vtx_a;
        vertices[1] = p_vtx_b;
    }
};

struct QHFace {

    Plane plane;
    uint32_t vertices[3];
    Vector<int> points_over;

    bool operator<(const QHFace &p_face) const {

        return points_over.size() < p_face.points_over.size();
    }
};
struct QHFaceConnect {
    List<QHFace>::iterator left, right;
    QHFaceConnect(List<QHFace>::iterator end) : left(end),right(end) {}
};
struct QHRetFaceConnect {
    List<Geometry::MeshData::Face>::iterator left, right;
};
} // end of anonymous namespace

Error QuickHull::build(Span<const Vector3> p_points, Geometry::MeshData &r_mesh) {

    /* CREATE AABB VOLUME */

    AABB aabb;
    for (int i = 0; i < p_points.size(); i++) {

        if (i == 0) {
            aabb.position = p_points[i];
        } else {
            aabb.expand_to(p_points[i]);
        }
    }

    if (aabb.size == Vector3()) {
        return ERR_CANT_CREATE;
    }

    Vector<bool> valid_points;
    valid_points.resize(p_points.size(),false);
    Set<Vector3> valid_cache;

    for (size_t i = 0; i < valid_points.size(); i++) {

        Vector3 sp = p_points[i].snapped(Vector3(0.0001f, 0.0001f, 0.0001f));
        if (!valid_cache.contains(sp)) {
            valid_points[i] = true;
            valid_cache.insert(sp);
        }
    }

    /* CREATE INITIAL SIMPLEX */

    int longest_axis = aabb.get_longest_axis_index();

    //first two vertices are the most distant
    int simplex[4] = { 0 };

    {
        real_t max = 0, min = 0;

        for (int i = 0; i < p_points.size(); i++) {

            if (!valid_points[i])
                continue;
            real_t d = p_points[i][longest_axis];
            if (i == 0 || d < min) {

                simplex[0] = i;
                min = d;
            }

            if (i == 0 || d > max) {
                simplex[1] = i;
                max = d;
            }
        }
    }

    //third vertex is one most further away from the line

    {
        real_t maxd = 0;
        Vector3 rel12 = p_points[simplex[0]] - p_points[simplex[1]];

        for (int i = 0; i < p_points.size(); i++) {

            if (!valid_points[i])
                continue;

            Vector3 n = rel12.cross(p_points[simplex[0]] - p_points[i]).cross(rel12).normalized();
            real_t d = Math::abs(n.dot(p_points[simplex[0]]) - n.dot(p_points[i]));

            if (i == 0 || d > maxd) {

                maxd = d;
                simplex[2] = i;
            }
        }
    }

    //fourth vertex is the one  most further away from the plane

    {
        real_t maxd = 0;
        Plane p(p_points[simplex[0]], p_points[simplex[1]], p_points[simplex[2]]);

        for (int i = 0; i < p_points.size(); i++) {

            if (!valid_points[i])
                continue;

            real_t d = Math::abs(p.distance_to(p_points[i]));

            if (i == 0 || d > maxd) {

                maxd = d;
                simplex[3] = i;
            }
        }
    }

    //compute center of simplex, this is a point always warranted to be inside
    Vector3 center;

    for (int i = 0; i < 4; i++) {
        center += p_points[simplex[i]];
    }

    center /= 4.0;

    //add faces

    List<QHFace> faces;

    for (int i = 0; i < 4; i++) {

        static const int face_order[4][3] = {
            { 0, 1, 2 },
            { 0, 1, 3 },
            { 0, 2, 3 },
            { 1, 2, 3 }
        };

        QHFace f;
        for (int j = 0; j < 3; j++) {
            f.vertices[j] = simplex[face_order[i][j]];
        }

        Plane p(p_points[f.vertices[0]], p_points[f.vertices[1]], p_points[f.vertices[2]]);

        if (p.is_point_over(center)) {
            //flip face to clockwise if facing inwards
            SWAP(f.vertices[0], f.vertices[1]);
            p = -p;
        }

        f.plane = p;

        faces.push_back(f);
    }

    real_t over_tolerance = 3 * UNIT_EPSILON * (aabb.size.x + aabb.size.y + aabb.size.z);

    /* COMPUTE AVAILABLE VERTICES */

    for (int i = 0,fin=(int)p_points.size(); i < fin; i++) {

        if (i == simplex[0])
            continue;
        if (i == simplex[1])
            continue;
        if (i == simplex[2])
            continue;
        if (i == simplex[3])
            continue;
        if (!valid_points[i])
            continue;

        for (QHFace &E : faces) {

            if (E.plane.distance_to(p_points[i]) > over_tolerance) {

                E.points_over.push_back(i);
                break;
            }
        }
    }
    faces.sort(); // sort them, so the ones with points are in the back


    /* BUILD HULL */

    //poop face (while still remain)
    //find further away point
    //find lit faces
    //determine horizon edges
    //build new faces with horizon edges, them assign points side from all lit faces
    //remove lit faces

    uint32_t debug_stop = debug_stop_after;

    while (debug_stop > 0 && !faces.back().points_over.empty()) {

        debug_stop--;
        QHFace &f = faces.back();

        //find vertex most outside
        int next = -1;
        real_t next_d = 0;

        for (size_t i = 0; i < f.points_over.size(); i++) {

            real_t d = f.plane.distance_to(p_points[f.points_over[i]]);

            if (d > next_d) {
                next_d = d;
                next = i;
            }
        }

        ERR_FAIL_COND_V(next == -1, ERR_BUG);

        Vector3 v = p_points[f.points_over[next]];

        //find lit faces and lit edges
        List<List<QHFace>::iterator> lit_faces; //lit face is a death sentence

        Map<QHEdge, QHFaceConnect> lit_edges; //create this on the flight, should not be that bad for performance and simplifies code a lot

        for (auto iter=faces.begin(),fin=faces.end(); iter!=fin; ++iter) {

            if (iter->plane.distance_to(v) > 0) {

                lit_faces.push_back(iter);

                for (int i = 0; i < 3; i++) {
                    uint32_t a = iter->vertices[i];
                    uint32_t b = iter->vertices[(i + 1) % 3];
                    QHEdge e(a, b);

                    Map<QHEdge, QHFaceConnect>::iterator F = lit_edges.find(e);
                    if (F==lit_edges.end()) {
                        F = lit_edges.emplace(e, QHFaceConnect(faces.end())).first;
                    }
                    if (e.vertices[0] == a) {
                        //left
                        F->second.left = iter;
                    } else {

                        F->second.right = iter;
                    }
                }
            }
        }

        //create new faces from horizon edges
        List<List<QHFace>::iterator> new_faces; //new faces

        for (eastl::pair<const QHEdge,QHFaceConnect> &E : lit_edges) {

            QHFaceConnect &fc = E.second;
            if (fc.left!=faces.end() && fc.right!=faces.end()) {
                continue; //edge is uninteresting, not on horizont
            }

            //create new face!

            QHFace face;
            face.vertices[0] = f.points_over[next];
            face.vertices[1] = E.first.vertices[0];
            face.vertices[2] = E.first.vertices[1];

            Plane p(p_points[face.vertices[0]], p_points[face.vertices[1]], p_points[face.vertices[2]]);

            if (p.is_point_over(center)) {
                //flip face to clockwise if facing inwards
                SWAP(face.vertices[0], face.vertices[1]);
                p = -p;
            }

            face.plane = p;
            new_faces.push_back(faces.insert(faces.end(),face));
        }

        //distribute points into new faces

        for (List<QHFace>::iterator F : lit_faces) {

            QHFace &lf = *F;

            for (size_t i = 0; i < lf.points_over.size(); i++) {

                if (lf.points_over[i] == f.points_over[next]) //do not add current one
                    continue;

                Vector3 p = p_points[lf.points_over[i]];
                for (List<QHFace>::iterator E : new_faces) {

                    QHFace &f2 = *E;
                    if (f2.plane.distance_to(p) > over_tolerance) {
                        f2.points_over.push_back(lf.points_over[i]);
                        break;
                    }
                }
            }
        }

        //erase lit faces

        while (!lit_faces.empty()) {

            faces.erase(lit_faces.front());
            lit_faces.pop_front();
        }

        //put faces that contain no points on the front

        for (List<QHFace>::iterator E : new_faces) {

            QHFace &f2 = *E;
            if (f2.points_over.empty()) {
                faces.splice( faces.begin(), faces, E ); // faces.move_to_front(E);
            }
        }

        //whew, done with iteration, go next
    }

    /* CREATE MESHDATA */

    //make a map of edges again
    Map<QHEdge, QHRetFaceConnect> ret_edges;
    List<Geometry::MeshData::Face> ret_faces;

    for (const QHFace &E : faces) {

        Geometry::MeshData::Face f;
        f.plane = E.plane;

        for (int i = 0; i < 3; i++) {
            f.indices.push_back(E.vertices[i]);
        }

        List<Geometry::MeshData::Face>::iterator F = ret_faces.insert(ret_faces.end(),f);

        for (int i = 0; i < 3; i++) {

            uint32_t a = E.vertices[i];
            uint32_t b = E.vertices[(i + 1) % 3];
            QHEdge e(a, b);

            Map<QHEdge, QHRetFaceConnect>::iterator G = ret_edges.find(e);
            if (G==ret_edges.end()) {
                G = ret_edges.emplace(e, QHRetFaceConnect()).first;
            }
            if (e.vertices[0] == a) {
                //left
                G->second.left = F;
            } else {

                G->second.right = F;
            }
        }
    }

    //fill faces

    for (List<Geometry::MeshData::Face>::iterator E = ret_faces.begin(); E!= ret_faces.end(); ++E) {

        Geometry::MeshData::Face &f = *E;

        for (int i = 0; i < f.indices.size(); i++) {

            int a = E->indices[i];
            int b = E->indices[(i + 1) % f.indices.size()];
            QHEdge e(a, b);

            Map<QHEdge, QHRetFaceConnect>::iterator F = ret_edges.find(e);

            ERR_CONTINUE(F==ret_edges.end());
            List<Geometry::MeshData::Face>::iterator O = F->second.left == E ? F->second.right : F->second.left;
            ERR_CONTINUE(O == E);
            ERR_CONTINUE(O == ret_faces.end());

            if (!O->plane.is_equal_approx(f.plane))
                continue;

            //merge and delete edge and contiguous face, while repointing edges (uuugh!)
            int ois = O->indices.size();
            int merged = 0;

            for (int j = 0; j < ois; j++) {
                //search a
                if (O->indices[j] == a) {
                    //append the rest
                    for (int k = 0; k < ois; k++) {

                        int idx = O->indices[(k + j) % ois];
                        int idxn = O->indices[(k + j + 1) % ois];
                        if (idx == b && idxn == a) { //already have b!
                            break;
                        }
                        if (idx != a) {
                            f.indices.insert_at(i + 1, idx);
                            i++;
                            merged++;
                        }
                        QHEdge e2(idx, idxn);

                        Map<QHEdge, QHRetFaceConnect>::iterator F2 = ret_edges.find(e2);
                        ERR_CONTINUE(F2==ret_edges.end());
                        //change faceconnect, point to this face instead
                        if (F2->second.left == O)
                            F2->second.left = E;
                        else if (F2->second.right == O)
                            F2->second.right = E;
                    }

                    break;
                }
            }

            // remove all edge connections to this face
            for (eastl::pair<const QHEdge,QHRetFaceConnect> &G : ret_edges) {
                if (G.second.left == O)
                    G.second.left = ret_faces.end();

                if (G.second.right == O)
                    G.second.right = ret_faces.end();
            }

            ret_edges.erase(F); //remove the edge
            ret_faces.erase(O); //remove the face
        }
    }

    //fill mesh
    r_mesh.faces.assign(eastl::move_iterator(ret_faces.begin()), eastl::move_iterator(ret_faces.end()));
    //TODO: consider r_mesh.faces.shrink_to_fit() here ?
    //NOTE: at this point QHRetFaceConnect in ret_edges is no longer valid, since the underlying container's contents was moved-from

    r_mesh.edges.reserve(ret_edges.size());
    int idx = 0;
    for (eastl::pair<const QHEdge,QHRetFaceConnect> &E : ret_edges) {

        Geometry::MeshData::Edge e;
        e.a = E.first.vertices[0];
        e.b = E.first.vertices[1];
        r_mesh.edges.emplace_back(e);
    }

    r_mesh.vertices.assign(p_points.begin(),p_points.end());

    return OK;
}
