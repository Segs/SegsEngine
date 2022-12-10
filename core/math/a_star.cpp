/*************************************************************************/
/*  a_star.cpp                                                           */
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

#include "a_star.h"

#include "core/math/geometry.h"
#include "core/math/vector2.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/pool_vector.h"
#include "core/script_language.h"
#include "core/string_formatter.h"
#include "scene/scene_string_names.h"

IMPL_GDCLASS(AStar)
IMPL_GDCLASS(AStar2D)

struct AStarPoint {

    AStarPoint() :
            neighbours(4u),
            unlinked_neighbours(4u) {}
    int id;
    Vector3 pos;
    real_t weight_scale;
    bool enabled;
    OAHashMap<int, AStarPoint *> neighbours;
    OAHashMap<int, AStarPoint *> unlinked_neighbours;

    // Used for pathfinding.

    AStarPoint *prev_point;
    real_t g_score;
    real_t f_score;
    uint64_t open_pass;
    uint64_t closed_pass;
};

struct SortPoints {
    _FORCE_INLINE_ bool operator()(const AStarPoint *A, const AStarPoint *B) const { // Returns true when the AStarPoint A is worse than AStarPoint B.
        if (A->f_score > B->f_score) {
            return true;
        } else if (A->f_score < B->f_score) {
            return false;
        } else {
            return A->g_score < B->g_score; // If the f_costs are the same then prioritize the points that are further away from the start.
        }
    }
};

int AStar::get_available_point_id() const {

    if (points.empty()) {
        return 1;
    }

    // calculate our new next available point id if bigger than before or next id already contained in set of points.
    if (points.has(last_free_id)) {
        int cur_new_id = last_free_id;
        while (points.has(cur_new_id)) {
            cur_new_id++;
        }
        int &non_const = const_cast<int &>(last_free_id);
        non_const = cur_new_id;
    }

    return last_free_id;
}

void AStar::add_point(int p_id, const Vector3 &p_pos, real_t p_weight_scale) {
    ERR_FAIL_COND_MSG(p_id < 0, FormatVE("Can't add a point with negative id: %d.", p_id));
    ERR_FAIL_COND_MSG(p_weight_scale < 0, FormatVE("Can't add a point with weight scale less than 0.0: %f.", p_weight_scale));


    AStarPoint *found_pt;
    bool p_exists = points.lookup(p_id, found_pt);

    if (!p_exists) {
        AStarPoint *pt = memnew(AStarPoint);
        pt->id = p_id;
        pt->pos = p_pos;
        pt->weight_scale = p_weight_scale;
        pt->prev_point = nullptr;
        pt->open_pass = 0;
        pt->closed_pass = 0;
        pt->enabled = true;
        points.set(p_id, pt);
    } else {
        found_pt->pos = p_pos;
        found_pt->weight_scale = p_weight_scale;
    }
}

Vector3 AStar::get_point_position(int p_id) const {

    AStarPoint *p;
    bool p_exists = points.lookup(p_id, p);
    ERR_FAIL_COND_V(!p_exists, Vector3());

    return p->pos;
}

void AStar::set_point_position(int p_id, const Vector3 &p_pos) {

    AStarPoint *p;
    bool p_exists = points.lookup(p_id, p);
    ERR_FAIL_COND(!p_exists);

    p->pos = p_pos;
}

real_t AStar::get_point_weight_scale(int p_id) const {

    AStarPoint *p;
    bool p_exists = points.lookup(p_id, p);
    ERR_FAIL_COND_V(!p_exists, 0);

    return p->weight_scale;
}

void AStar::set_point_weight_scale(int p_id, real_t p_weight_scale) {

    AStarPoint *p;
    bool p_exists = points.lookup(p_id, p);
    ERR_FAIL_COND_MSG(!p_exists, FormatVE("Can't set point's weight scale. Point with id: %d doesn't exist.", p_id));
    ERR_FAIL_COND_MSG(p_weight_scale < 0, FormatVE("Can't set point's weight scale less than 0.0: %f.", p_weight_scale));

    p->weight_scale = p_weight_scale;
}

void AStar::remove_point(int p_id) {

    AStarPoint *p;
    bool p_exists = points.lookup(p_id, p);
    ERR_FAIL_COND(!p_exists);

    for (OAHashMap<int, AStarPoint *>::Iterator it = p->neighbours.iter(); it.valid; it = p->neighbours.next_iter(it)) {

        Segment s(p_id, (*it.key));
        segments.erase(s);

        (*it.value)->neighbours.remove(p->id);
        (*it.value)->unlinked_neighbours.remove(p->id);
    }

    for (OAHashMap<int, AStarPoint *>::Iterator it = p->unlinked_neighbours.iter(); it.valid; it = p->unlinked_neighbours.next_iter(it)) {

        Segment s(p_id, (*it.key));
        segments.erase(s);

        (*it.value)->neighbours.remove(p->id);
        (*it.value)->unlinked_neighbours.remove(p->id);
    }

    memdelete(p);
    points.remove(p_id);
    last_free_id = p_id;
}

void AStar::connect_points(int p_id, int p_with_id, bool bidirectional) {

    ERR_FAIL_COND(p_id == p_with_id);

    AStarPoint *a;
    bool from_exists = points.lookup(p_id, a);
    ERR_FAIL_COND(!from_exists);

    AStarPoint *b;
    bool to_exists = points.lookup(p_with_id, b);
    ERR_FAIL_COND(!to_exists);

    a->neighbours.set(b->id, b);

    if (bidirectional) {
        b->neighbours.set(a->id, a);
    } else {
        b->unlinked_neighbours.set(a->id, a);
    }

    Segment s(p_id, p_with_id);
    if (bidirectional) {
        s.direction = Segment::BIDIRECTIONAL;
    }

    Set<Segment>::iterator element = segments.find(s);
    if (element != segments.end()) {
        s.direction |= element->direction;
        if (s.direction == Segment::BIDIRECTIONAL) {
            // Both are neighbours of each other now
            a->unlinked_neighbours.remove(b->id);
            b->unlinked_neighbours.remove(a->id);
        }
        segments.erase(element);
    }

    segments.insert(s);
}

void AStar::disconnect_points(int p_id, int p_with_id, bool bidirectional) {

    AStarPoint *a;
    bool a_exists = points.lookup(p_id, a);
    ERR_FAIL_COND(!a_exists);

    AStarPoint *b;
    bool b_exists = points.lookup(p_with_id, b);
    ERR_FAIL_COND(!b_exists);

    Segment s(p_id, p_with_id);
    int remove_direction = bidirectional ? (int)Segment::BIDIRECTIONAL : s.direction;

    Set<Segment>::iterator element = segments.find(s);
    if (element != segments.end()) {
        // s is the new segment
        // Erase the directions to be removed
        s.direction = (element->direction & ~remove_direction);

        a->neighbours.remove(b->id);
        if (bidirectional) {
            b->neighbours.remove(a->id);
            if (element->direction != Segment::BIDIRECTIONAL) {
                a->unlinked_neighbours.remove(b->id);
                b->unlinked_neighbours.remove(a->id);
            }
        } else {
            if (s.direction == Segment::NONE) {
                b->unlinked_neighbours.remove(a->id);
            } else {
                a->unlinked_neighbours.set(b->id, b);
            }
        }

        segments.erase(element);
        if (s.direction != Segment::NONE) {
            segments.insert(s);
        }
    }
}

bool AStar::has_point(int p_id) const {

    return points.has(p_id);
}

Array AStar::get_points() {

    Array point_list;

    for (OAHashMap<int, AStarPoint *>::Iterator it = points.iter(); it.valid; it = points.next_iter(it)) {
        point_list.push_back(*(it.key));
    }

    return point_list;
}

Vector<int> AStar::get_point_connections(int p_id) {

    AStarPoint *p;
    bool p_exists = points.lookup(p_id, p);
    ERR_FAIL_COND_V(!p_exists, Vector<int>());

    Vector<int> point_list;
    point_list.reserve(p->neighbours.get_num_elements());
    for (OAHashMap<int, AStarPoint *>::Iterator it = p->neighbours.iter(); it.valid; it = p->neighbours.next_iter(it)) {
        point_list.push_back((*it.key));
    }

    return point_list;
}

bool AStar::are_points_connected(int p_id, int p_with_id, bool bidirectional) const {

    Segment s(p_id, p_with_id);
    const Set<Segment>::iterator element = segments.find(s);

    return element != segments.end() &&
           (bidirectional || (element->direction & s.direction) == s.direction);
}

void AStar::clear() {

    last_free_id = 0;
    for (OAHashMap<int, AStarPoint *>::Iterator it = points.iter(); it.valid; it = points.next_iter(it)) {
        memdelete(*(it.value));
    }
    segments.clear();
    points.clear();
}

int AStar::get_point_count() const {
    return points.get_num_elements();
}

int AStar::get_point_capacity() const {
    return points.get_capacity();
}

void AStar::reserve_space(int p_num_nodes) {
    ERR_FAIL_COND_MSG(p_num_nodes <= 0, "New capacity must be greater than 0, was: " + itos(p_num_nodes) + ".");
    ERR_FAIL_COND_MSG((uint32_t)p_num_nodes < points.get_capacity(), "New capacity must be greater than current capacity: " + itos(points.get_capacity()) + ", new was: " + itos(p_num_nodes) + ".");
    points.reserve(p_num_nodes);
}

int AStar::get_closest_point(const Vector3 &p_point, bool p_include_disabled) const {

    int closest_id = -1;
    real_t closest_dist = 1e20f;

    for (OAHashMap<int, AStarPoint *>::Iterator it = points.iter(); it.valid; it = points.next_iter(it)) {

        if (!p_include_disabled && !(*it.value)->enabled) continue; // Disabled points should not be considered.

        real_t d = p_point.distance_squared_to((*it.value)->pos);
        if (closest_id < 0 || d < closest_dist) {
            closest_dist = d;
            closest_id = *(it.key);
        }
    }

    return closest_id;
}

Vector3 AStar::get_closest_position_in_segment(const Vector3 &p_point) const {

    bool found = false;
    real_t closest_dist = 1e20f;
    Vector3 closest_point;

    for (const Segment &E : segments) {


        AStarPoint *from_point = nullptr, *to_point = nullptr;
        points.lookup(E.u, from_point);
        points.lookup(E.v, to_point);

        if (!(from_point->enabled && to_point->enabled)) {
            continue;
        }

        Vector3 segment[2] = {
            from_point->pos,
            to_point->pos,
        };

        Vector3 p = Geometry::get_closest_point_to_segment(p_point, segment);
        real_t d = p_point.distance_squared_to(p);
        if (!found || d < closest_dist) {

            closest_point = p;
            closest_dist = d;
            found = true;
        }
    }

    return closest_point;
}

bool AStar::_solve(AStarPoint *begin_point, AStarPoint *end_point) {

    pass++;

    if (!end_point->enabled) return false;

    bool found_route = false;

    Vector<AStarPoint *> open_list;
    SortArray<AStarPoint *, SortPoints> sorter;

    begin_point->g_score = 0;
    begin_point->f_score = _estimate_cost(begin_point->id, end_point->id);
    open_list.push_back(begin_point);

    while (!open_list.empty()) {

        AStarPoint *p = open_list[0]; // The currently processed point

        if (p == end_point) {
            found_route = true;
            break;
        }

        sorter.pop_heap(0, open_list.size(), open_list.data()); // Remove the current point from the open list
        open_list.pop_back();
        p->closed_pass = pass; // Mark the point as closed

        for (OAHashMap<int, AStarPoint *>::Iterator it = p->neighbours.iter(); it.valid; it = p->neighbours.next_iter(it)) {

            AStarPoint *e = *(it.value); // The neighbour point

            if (!e->enabled || e->closed_pass == pass) {
                continue;
            }

            real_t tentative_g_score = p->g_score + _compute_cost(p->id, e->id) * e->weight_scale;

            bool new_point = false;

            if (e->open_pass != pass) { // The point wasn't inside the open list.
                e->open_pass = pass;
                open_list.push_back(e);
                new_point = true;
            } else if (tentative_g_score >= e->g_score) { // The new path is worse than the previous.
                continue;
            }

            e->prev_point = p;
            e->g_score = tentative_g_score;
            e->f_score = e->g_score + _estimate_cost(e->id, end_point->id);

            if (new_point) { // The position of the new points is already known.
                sorter.push_heap(0, open_list.size() - 1, 0, e, open_list.data());
            } else {
                sorter.push_heap(0, open_list.index_of(e), 0, e, open_list.data());
            }
        }
    }

    return found_route;
}

float AStar::_estimate_cost(int p_from_id, int p_to_id) {

    if (get_script_instance() && get_script_instance()->has_method(SceneStringNames::_estimate_cost))
        return get_script_instance()->call(SceneStringNames::_estimate_cost, p_from_id, p_to_id).as<float>();

    AStarPoint *from_point;
    bool from_exists = points.lookup(p_from_id, from_point);
    ERR_FAIL_COND_V(!from_exists, 0);

    AStarPoint *to_point;
    bool to_exists = points.lookup(p_to_id, to_point);
    ERR_FAIL_COND_V(!to_exists, 0);

    return from_point->pos.distance_to(to_point->pos);
}

float AStar::_compute_cost(int p_from_id, int p_to_id) {

    if (get_script_instance() && get_script_instance()->has_method(SceneStringNames::_compute_cost))
        return get_script_instance()->call(SceneStringNames::_compute_cost, p_from_id, p_to_id).as<float>();

    AStarPoint *from_point;
    bool from_exists = points.lookup(p_from_id, from_point);
    CRASH_COND(!from_exists);

    AStarPoint *to_point;
    bool to_exists = points.lookup(p_to_id, to_point);
    CRASH_COND(!to_exists);

    return from_point->pos.distance_to(to_point->pos);
}

PoolVector<Vector3> AStar::get_point_path(int p_from_id, int p_to_id) {

    AStarPoint *a;
    bool from_exists = points.lookup(p_from_id, a);
    ERR_FAIL_COND_V(!from_exists, PoolVector<Vector3>());

    AStarPoint *b;
    bool to_exists = points.lookup(p_to_id, b);
    ERR_FAIL_COND_V(!to_exists, PoolVector<Vector3>());

    if (a == b) {
        PoolVector<Vector3> ret;
        ret.push_back(a->pos);
        return ret;
    }

    AStarPoint *begin_point = a;
    AStarPoint *end_point = b;

    bool found_route = _solve(begin_point, end_point);
    if (!found_route) return PoolVector<Vector3>();

    AStarPoint *p = end_point;
    int pc = 1; // Begin point
    while (p != begin_point) {
        pc++;
        p = p->prev_point;
    }

    PoolVector<Vector3> path;
    path.resize(pc);

    {
        PoolVector<Vector3>::Write w = path.write();

        AStarPoint *p2 = end_point;
        int idx = pc - 1;
        while (p2 != begin_point) {
            w[idx--] = p2->pos;
            p2 = p2->prev_point;
        }

        w[0] = p2->pos; // Assign first
    }

    return path;
}

PoolVector<int> AStar::get_id_path(int p_from_id, int p_to_id) {

    AStarPoint *a;
    bool from_exists = points.lookup(p_from_id, a);
    ERR_FAIL_COND_V(!from_exists, PoolVector<int>());

    AStarPoint *b;
    bool to_exists = points.lookup(p_to_id, b);
    ERR_FAIL_COND_V(!to_exists, PoolVector<int>());

    if (a == b) {
        PoolVector<int> ret;
        ret.push_back(a->id);
        return ret;
    }

    AStarPoint *begin_point = a;
    AStarPoint *end_point = b;

    bool found_route = _solve(begin_point, end_point);
    if (!found_route) return PoolVector<int>();

    AStarPoint *p = end_point;
    int pc = 1; // Begin point
    while (p != begin_point) {
        pc++;
        p = p->prev_point;
    }

    PoolVector<int> path;
    path.resize(pc);

    {
        PoolVector<int>::Write w = path.write();

        p = end_point;
        int idx = pc - 1;
        while (p != begin_point) {
            w[idx--] = p->id;
            p = p->prev_point;
        }

        w[0] = p->id; // Assign first
    }

    return path;
}

void AStar::set_point_disabled(int p_id, bool p_disabled) {

    AStarPoint *p;
    bool p_exists = points.lookup(p_id, p);
    ERR_FAIL_COND(!p_exists);

    p->enabled = !p_disabled;
}

bool AStar::is_point_disabled(int p_id) const {

    AStarPoint *p;
    bool p_exists = points.lookup(p_id, p);
    ERR_FAIL_COND_V(!p_exists, false);

    return !p->enabled;
}

void AStar::_bind_methods() {

    SE_BIND_METHOD(AStar,get_available_point_id);
    SE_BIND_METHOD_WITH_DEFAULTS(AStar, add_point,DEFVAL(1.0));
    SE_BIND_METHOD(AStar,get_point_position);
    SE_BIND_METHOD(AStar,set_point_position);
    SE_BIND_METHOD(AStar,get_point_weight_scale);
    SE_BIND_METHOD(AStar,set_point_weight_scale);
    SE_BIND_METHOD(AStar,remove_point);
    SE_BIND_METHOD(AStar,has_point);
    SE_BIND_METHOD(AStar,get_point_connections);
    SE_BIND_METHOD(AStar,get_points);

    SE_BIND_METHOD_WITH_DEFAULTS(AStar, set_point_disabled,DEFVAL(true));
    SE_BIND_METHOD(AStar,is_point_disabled);

    SE_BIND_METHOD_WITH_DEFAULTS(AStar, connect_points,DEFVAL(true));
    SE_BIND_METHOD_WITH_DEFAULTS(AStar, disconnect_points,DEFVAL(true));
    SE_BIND_METHOD_WITH_DEFAULTS(AStar, are_points_connected,DEFVAL(true));

    SE_BIND_METHOD(AStar,get_point_count);
    SE_BIND_METHOD(AStar,get_point_capacity);
    SE_BIND_METHOD(AStar,reserve_space);
    SE_BIND_METHOD(AStar,clear);

    SE_BIND_METHOD_WITH_DEFAULTS(AStar, get_closest_point,DEFVAL(false));
    SE_BIND_METHOD(AStar,get_closest_position_in_segment);

    SE_BIND_METHOD(AStar,get_point_path);
    SE_BIND_METHOD(AStar,get_id_path);

    BIND_VMETHOD(MethodInfo(VariantType::FLOAT, "_estimate_cost", PropertyInfo(VariantType::INT, "from_id"), PropertyInfo(VariantType::INT, "to_id")));
    BIND_VMETHOD(MethodInfo(VariantType::FLOAT, "_compute_cost", PropertyInfo(VariantType::INT, "from_id"), PropertyInfo(VariantType::INT, "to_id")));
}

AStar::AStar() {
    last_free_id = 0;
    pass = 1;
}

AStar::~AStar() {

    clear();
}

/////////////////////////////////////////////////////////////

int AStar2D::get_available_point_id() const {
    return astar.get_available_point_id();
}

void AStar2D::add_point(int p_id, const Vector2 &p_pos, real_t p_weight_scale) {
    astar.add_point(p_id, Vector3(p_pos.x, p_pos.y, 0), p_weight_scale);
}

Vector2 AStar2D::get_point_position(int p_id) const {
    Vector3 p = astar.get_point_position(p_id);
    return Vector2(p.x, p.y);
}

void AStar2D::set_point_position(int p_id, const Vector2 &p_pos) {
    astar.set_point_position(p_id, Vector3(p_pos.x, p_pos.y, 0));
}

real_t AStar2D::get_point_weight_scale(int p_id) const {
    return astar.get_point_weight_scale(p_id);
}

void AStar2D::set_point_weight_scale(int p_id, real_t p_weight_scale) {
    astar.set_point_weight_scale(p_id, p_weight_scale);
}

void AStar2D::remove_point(int p_id) {
    astar.remove_point(p_id);
}

bool AStar2D::has_point(int p_id) const {
    return astar.has_point(p_id);
}

Vector<int> AStar2D::get_point_connections(int p_id) {
    return astar.get_point_connections(p_id);
}

Array AStar2D::get_points() {
    return astar.get_points();
}

void AStar2D::set_point_disabled(int p_id, bool p_disabled) {
    astar.set_point_disabled(p_id, p_disabled);
}

bool AStar2D::is_point_disabled(int p_id) const {
    return astar.is_point_disabled(p_id);
}

void AStar2D::connect_points(int p_id, int p_with_id, bool p_bidirectional) {
    astar.connect_points(p_id, p_with_id, p_bidirectional);
}

void AStar2D::disconnect_points(int p_id, int p_with_id, bool p_bidirectional) {
    astar.disconnect_points(p_id, p_with_id, p_bidirectional);
}

bool AStar2D::are_points_connected(int p_id, int p_with_id, bool p_bidirectional) const {
    return astar.are_points_connected(p_id, p_with_id, p_bidirectional);
}

int AStar2D::get_point_count() const {
    return astar.get_point_count();
}

int AStar2D::get_point_capacity() const {
    return astar.get_point_capacity();
}
void AStar2D::clear() {
    astar.clear();
}

void AStar2D::reserve_space(int p_num_nodes) {
    astar.reserve_space(p_num_nodes);
}
int AStar2D::get_closest_point(const Vector2 &p_point, bool p_include_disabled) const {
    return astar.get_closest_point(Vector3(p_point.x, p_point.y, 0), p_include_disabled);
}

Vector2 AStar2D::get_closest_position_in_segment(const Vector2 &p_point) const {
    Vector3 p = astar.get_closest_position_in_segment(Vector3(p_point.x, p_point.y, 0));
    return Vector2(p.x, p.y);
}

float AStar2D::_estimate_cost(int p_from_id, int p_to_id) {

    if (get_script_instance() && get_script_instance()->has_method(SceneStringNames::_estimate_cost))
        return get_script_instance()->call(SceneStringNames::_estimate_cost, p_from_id, p_to_id).as<float>();

    AStarPoint *from_point;
    bool from_exists = astar.points.lookup(p_from_id, from_point);
    ERR_FAIL_COND_V(!from_exists, 0);

    AStarPoint *to_point;
    bool to_exists = astar.points.lookup(p_to_id, to_point);
    ERR_FAIL_COND_V(!to_exists, 0);

    return from_point->pos.distance_to(to_point->pos);
}

float AStar2D::_compute_cost(int p_from_id, int p_to_id) {

    if (get_script_instance() && get_script_instance()->has_method(SceneStringNames::_compute_cost))
        return get_script_instance()->call(SceneStringNames::_compute_cost, p_from_id, p_to_id).as<float>();

    AStarPoint *from_point;
    bool from_exists = astar.points.lookup(p_from_id, from_point);
    ERR_FAIL_COND_V(!from_exists, 0);

    AStarPoint *to_point;
    bool to_exists = astar.points.lookup(p_to_id, to_point);
    ERR_FAIL_COND_V(!to_exists, 0);

    return from_point->pos.distance_to(to_point->pos);
}

PoolVector<Vector2> AStar2D::get_point_path(int p_from_id, int p_to_id) {

    AStarPoint *a;
    bool from_exists = astar.points.lookup(p_from_id, a);
    ERR_FAIL_COND_V(!from_exists, PoolVector<Vector2>());

    AStarPoint *b;
    bool to_exists = astar.points.lookup(p_to_id, b);
    ERR_FAIL_COND_V(!to_exists, PoolVector<Vector2>());

    if (a == b) {
        PoolVector<Vector2> ret;
        ret.push_back(Vector2(a->pos.x, a->pos.y));
        return ret;
    }

    AStarPoint *begin_point = a;
    AStarPoint *end_point = b;

    bool found_route = _solve(begin_point, end_point);
    if (!found_route) return PoolVector<Vector2>();

    AStarPoint *p = end_point;
    int pc = 1; // Begin point
    while (p != begin_point) {
        pc++;
        p = p->prev_point;
    }

    PoolVector<Vector2> path;
    path.resize(pc);

    {
        auto w = path.write();

        AStarPoint *p2 = end_point;
        int idx = pc - 1;
        while (p2 != begin_point) {
            w[idx--] = Vector2(p2->pos.x, p2->pos.y);
            p2 = p2->prev_point;
        }

        w[0] = Vector2(p2->pos.x, p2->pos.y); // Assign first
    }

    return path;
}

PoolVector<int> AStar2D::get_id_path(int p_from_id, int p_to_id) {

    AStarPoint *a;
    bool from_exists = astar.points.lookup(p_from_id, a);
    ERR_FAIL_COND_V(!from_exists, PoolVector<int>());

    AStarPoint *b;
    bool to_exists = astar.points.lookup(p_to_id, b);
    ERR_FAIL_COND_V(!to_exists, PoolVector<int>());

    if (a == b) {
        PoolVector<int> ret;
        ret.push_back(a->id);
        return ret;
    }

    AStarPoint *begin_point = a;
    AStarPoint *end_point = b;

    bool found_route = _solve(begin_point, end_point);
    if (!found_route) {
        return PoolVector<int>();
    }

    AStarPoint *p = end_point;
    int pc = 1; // Begin point
    while (p != begin_point) {
        pc++;
        p = p->prev_point;
    }

    PoolVector<int> path;
    path.resize(pc);

    {
        auto w = path.write();

        p = end_point;
        int idx = pc - 1;
        while (p != begin_point) {
            w[idx--] = p->id;
            p = p->prev_point;
        }

        w[0] = p->id; // Assign first
    }

    return path;
}

bool AStar2D::_solve(AStarPoint *begin_point, AStarPoint *end_point) {

    astar.pass++;

    if (!end_point->enabled) {
        return false;
    }

    bool found_route = false;

    Vector<AStarPoint *> open_list;
    SortArray<AStarPoint *, SortPoints> sorter;

    begin_point->g_score = 0;
    begin_point->f_score = _estimate_cost(begin_point->id, end_point->id);
    open_list.push_back(begin_point);

    while (!open_list.empty()) {

        AStarPoint *p = open_list[0]; // The currently processed point

        if (p == end_point) {
            found_route = true;
            break;
        }

        sorter.pop_heap(0, open_list.size(), open_list.data()); // Remove the current point from the open list
        open_list.erase_at(open_list.size() - 1);
        p->closed_pass = astar.pass; // Mark the point as closed

        for (OAHashMap<int, AStarPoint *>::Iterator it = p->neighbours.iter(); it.valid; it = p->neighbours.next_iter(it)) {

            AStarPoint *e = *(it.value); // The neighbour point

            if (!e->enabled || e->closed_pass == astar.pass) {
                continue;
            }

            real_t tentative_g_score = p->g_score + _compute_cost(p->id, e->id) * e->weight_scale;

            bool new_point = false;

            if (e->open_pass != astar.pass) { // The point wasn't inside the open list.
                e->open_pass = astar.pass;
                open_list.push_back(e);
                new_point = true;
            } else if (tentative_g_score >= e->g_score) { // The new path is worse than the previous.
                continue;
            }

            e->prev_point = p;
            e->g_score = tentative_g_score;
            e->f_score = e->g_score + _estimate_cost(e->id, end_point->id);

            if (new_point) { // The position of the new points is already known.
                sorter.push_heap(0, open_list.size() - 1, 0, e, open_list.data());
            } else {
                sorter.push_heap(0, open_list.index_of(e), 0, e, open_list.data());
            }
        }
    }

    return found_route;
}

void AStar2D::_bind_methods() {

    SE_BIND_METHOD(AStar2D,get_available_point_id);
    SE_BIND_METHOD_WITH_DEFAULTS(AStar2D, add_point,DEFVAL(1.0));
    SE_BIND_METHOD(AStar2D,get_point_position);
    SE_BIND_METHOD(AStar2D,set_point_position);
    SE_BIND_METHOD(AStar2D,get_point_weight_scale);
    SE_BIND_METHOD(AStar2D,set_point_weight_scale);
    SE_BIND_METHOD(AStar2D,remove_point);
    SE_BIND_METHOD(AStar2D,has_point);
    SE_BIND_METHOD(AStar2D,get_point_connections);
    SE_BIND_METHOD(AStar2D,get_points);

    SE_BIND_METHOD_WITH_DEFAULTS(AStar2D, set_point_disabled, DEFVAL(true));
    SE_BIND_METHOD(AStar2D,is_point_disabled);

    SE_BIND_METHOD_WITH_DEFAULTS(AStar2D, connect_points, DEFVAL(true));
    SE_BIND_METHOD(AStar2D,disconnect_points);
    SE_BIND_METHOD(AStar2D,are_points_connected);

    SE_BIND_METHOD(AStar2D,get_point_count);
    SE_BIND_METHOD(AStar2D,get_point_capacity);
    SE_BIND_METHOD(AStar2D,reserve_space);
    SE_BIND_METHOD(AStar2D,clear);

    SE_BIND_METHOD_WITH_DEFAULTS(AStar2D, get_closest_point,DEFVAL(false));
    SE_BIND_METHOD(AStar2D,get_closest_position_in_segment);

    SE_BIND_METHOD(AStar2D,get_point_path);
    SE_BIND_METHOD(AStar2D,get_id_path);

    BIND_VMETHOD(MethodInfo(VariantType::FLOAT, "_estimate_cost", PropertyInfo(VariantType::INT, "from_id"), PropertyInfo(VariantType::INT, "to_id")));
    BIND_VMETHOD(MethodInfo(VariantType::FLOAT, "_compute_cost", PropertyInfo(VariantType::INT, "from_id"), PropertyInfo(VariantType::INT, "to_id")));
}

AStar2D::AStar2D() = default;

AStar2D::~AStar2D() = default;
