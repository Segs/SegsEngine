/*************************************************************************/
/*  broad_phase_2d_hash_grid.cpp                                         */
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

#include "broad_phase_2d_hash_grid.h"

#include "collision_object_2d_sw.h"
#include "core/project_settings.h"
#include "core/property_info.h"

#define LARGE_ELEMENT_FI 1.01239812f

void BroadPhase2DHashGrid::_pair_attempt(Element *p_elem, Element *p_with) {
    auto E = p_elem->paired.find(p_with);

    ERR_FAIL_COND(p_elem->_static && p_with->_static);

    if (E == p_elem->paired.end()) {
        PairData *pd = memnew(PairData);
        p_elem->paired[p_with] = pd;
        p_with->paired[p_elem] = pd;
    } else {
        E->second->rc++;
    }
}

void BroadPhase2DHashGrid::_unpair_attempt(Element *p_elem, Element *p_with) {
    auto E = p_elem->paired.find(p_with);

    ERR_FAIL_COND(E == p_elem->paired.end()); // this should really be paired..

    E->second->rc--;

    if (E->second->rc == 0) {
        if (E->second->colliding) {
            // uncollide
            if (unpair_callback) {
                unpair_callback(p_elem->owner, p_elem->subindex, p_with->owner, p_with->subindex, E->second->ud,
                        unpair_userdata);
            }
        }

        memdelete(E->second);
        p_elem->paired.erase(E);
        p_with->paired.erase(p_elem);
    }
}

void BroadPhase2DHashGrid::_check_motion(Element *p_elem) {
    for (const eastl::pair<Element *const, PairData *> &E : p_elem->paired) {
        bool physical_collision = p_elem->aabb.intersects(E.first->aabb);
        bool logical_collision = p_elem->owner->test_collision_mask(E.first->owner);

        if (physical_collision) {
            if (!E.second->colliding || (logical_collision && !E.second->ud && pair_callback)) {
                E.second->ud = pair_callback(
                        p_elem->owner, p_elem->subindex, E.first->owner, E.first->subindex, pair_userdata);
            } else if (E.second->colliding && !logical_collision && E.second->ud && unpair_callback) {
                unpair_callback(p_elem->owner, p_elem->subindex, E.first->owner, E.first->subindex, E.second->ud,
                        unpair_userdata);
                E.second->ud = nullptr;
            }
            E.second->colliding = true;
        } else { // No physcial_collision
            if (E.second->colliding && unpair_callback) {
                unpair_callback(p_elem->owner, p_elem->subindex, E.first->owner, E.first->subindex, E.second->ud,
                        unpair_userdata);
            }
            E.second->colliding = false;
        }
    }
}

void BroadPhase2DHashGrid::_enter_grid(Element *p_elem, const Rect2 &p_rect, bool p_static) {
    Vector2 sz = (p_rect.size / cell_size * LARGE_ELEMENT_FI); // use magic number to avoid floating point issues
    if (sz.width * sz.height > large_object_min_surface) {
        // large object, do not use grid, must check against all elements
        for (eastl::pair<const ID, Element> &E : element_map) {
            if (E.first == p_elem->self) {
                continue; // do not pair against itself
            }
            if (E.second.owner == p_elem->owner) {
                continue;
            }
            if (E.second._static && p_static) {
                continue;
            }

            _pair_attempt(p_elem, &E.second);
        }

        large_elements[p_elem].inc();
        return;
    }

    Point2i from = (p_rect.position / cell_size).floor();
    Point2i to = ((p_rect.position + p_rect.size) / cell_size).floor();

    for (int i = from.x; i <= to.x; i++) {
        for (int j = from.y; j <= to.y; j++) {
            PosKey pk;
            pk.x = i;
            pk.y = j;

            uint32_t idx = pk.hash() % hash_table_size;
            PosBin *pb = hash_table[idx];

            while (pb) {
                if (pb->key == pk) {
                    break;
                }

                pb = pb->next;
            }

            bool entered = false;

            if (!pb) {
                // does not exist, create!
                pb = memnew(PosBin);
                pb->key = pk;
                pb->next = hash_table[idx];
                hash_table[idx] = pb;
            }

            if (p_static) {
                if (pb->static_object_set[p_elem].inc() == 1) {
                    entered = true;
                }
            } else {
                if (pb->object_set[p_elem].inc() == 1) {
                    entered = true;
                }
            }

            if (entered) {
                for (eastl::pair<Element *, RC> E : pb->object_set) {
                    if (E.first->owner == p_elem->owner) {
                        continue;
                    }
                    _pair_attempt(p_elem, E.first);
                }

                if (!p_static) {
                    for (eastl::pair<Element *, RC> E : pb->static_object_set) {
                        if (E.first->owner == p_elem->owner) {
                            continue;
                        }
                        _pair_attempt(p_elem, E.first);
                    }
                }
            }
        }
    }

    // pair separatedly with large elements

    for (eastl::pair<Element *const, RC> &E : large_elements) {
        if (E.first == p_elem) {
            continue; // do not pair against itself
        }
        if (E.first->owner == p_elem->owner) {
            continue;
        }
        if (E.first->_static && p_static) {
            continue;
        }

        _pair_attempt(E.first, p_elem);
    }
}

void BroadPhase2DHashGrid::_exit_grid(Element *p_elem, const Rect2 &p_rect, bool p_static) {
    Vector2 sz = (p_rect.size / cell_size * LARGE_ELEMENT_FI);
    if (sz.width * sz.height > large_object_min_surface) {
        // unpair all elements, instead of checking all, just check what is already paired, so we at least save from
        // checking static vs static
        auto E = p_elem->paired.begin();
        while (E != p_elem->paired.end()) {
            auto next = E;
            ++next;
            _unpair_attempt(p_elem, E->first);
            E = next;
        }

        if (large_elements[p_elem].dec() == 0) {
            large_elements.erase(p_elem);
        }
        return;
    }

    Point2i from = (p_rect.position / cell_size).floor();
    Point2i to = ((p_rect.position + p_rect.size) / cell_size).floor();

    for (int i = from.x; i <= to.x; i++) {
        for (int j = from.y; j <= to.y; j++) {
            PosKey pk;
            pk.x = i;
            pk.y = j;

            uint32_t idx = pk.hash() % hash_table_size;
            PosBin *pb = hash_table[idx];

            while (pb) {
                if (pb->key == pk) {
                    break;
                }

                pb = pb->next;
            }

            ERR_CONTINUE(!pb); // should exist!!

            bool exited = false;

            if (p_static) {
                if (pb->static_object_set[p_elem].dec() == 0) {
                    pb->static_object_set.erase(p_elem);
                    exited = true;
                }
            } else {
                if (pb->object_set[p_elem].dec() == 0) {
                    pb->object_set.erase(p_elem);
                    exited = true;
                }
            }

            if (exited) {
                for (eastl::pair<Element *const, RC> &E : pb->object_set) {
                    if (E.first->owner == p_elem->owner) {
                        continue;
                    }
                    _unpair_attempt(p_elem, E.first);
                }

                if (!p_static) {
                    for (eastl::pair<Element *const, RC> &E : pb->static_object_set) {
                        if (E.first->owner == p_elem->owner) {
                            continue;
                        }
                        _unpair_attempt(p_elem, E.first);
                    }
                }
            }

            if (pb->object_set.empty() && pb->static_object_set.empty()) {
                if (hash_table[idx] == pb) {
                    hash_table[idx] = pb->next;
                } else {
                    PosBin *px = hash_table[idx];

                    while (px) {
                        if (px->next == pb) {
                            px->next = pb->next;
                            break;
                        }

                        px = px->next;
                    }

                    ERR_CONTINUE(!px);
                }

                memdelete(pb);
            }
        }
    }

    for (const eastl::pair<Element *const, RC> &E : large_elements) {
        if (E.first == p_elem) {
            continue; // do not pair against itself
        }
        if (E.first->owner == p_elem->owner) {
            continue;
        }
        if (E.first->_static && p_static) {
            continue;
        }

        // unpair from large elements
        _unpair_attempt(p_elem, E.first);
    }
}

BroadPhase2DHashGrid::ID BroadPhase2DHashGrid::create(CollisionObject2DSW *p_object, int p_subindex) {
    current++;

    Element e;
    e.owner = p_object;
    e._static = false;
    e.subindex = p_subindex;
    e.self = current;
    e.pass = 0;

    element_map[current] = e;
    return current;
}

void BroadPhase2DHashGrid::move(ID p_id, const Rect2 &p_aabb) {
    auto E = element_map.find(p_id);
    ERR_FAIL_COND(E == element_map.end());

    Element &e = E->second;

    if (p_aabb != e.aabb) {
        if (p_aabb != Rect2()) {
            _enter_grid(&e, p_aabb, e._static);
        }
        if (e.aabb != Rect2()) {
            _exit_grid(&e, e.aabb, e._static);
        }
        e.aabb = p_aabb;
    }

    _check_motion(&e);
}
void BroadPhase2DHashGrid::set_static(ID p_id, bool p_static) {
    auto E = element_map.find(p_id);
    ERR_FAIL_COND(E == element_map.end());

    Element &e = E->second;

    if (e._static == p_static) {
        return;
    }

    if (e.aabb != Rect2()) {
        _exit_grid(&e, e.aabb, e._static);
    }

    e._static = p_static;

    if (e.aabb != Rect2()) {
        _enter_grid(&e, e.aabb, e._static);
        _check_motion(&e);
    }
}
void BroadPhase2DHashGrid::remove(ID p_id) {
    auto E = element_map.find(p_id);
    ERR_FAIL_COND(E == element_map.end());

    Element &e = E->second;

    if (e.aabb != Rect2()) {
        _exit_grid(&e, e.aabb, e._static);
    }

    element_map.erase(p_id);
}

CollisionObject2DSW *BroadPhase2DHashGrid::get_object(ID p_id) const {
    auto E = element_map.find(p_id);
    ERR_FAIL_COND_V(E == element_map.end(), nullptr);
    return E->second.owner;
}
bool BroadPhase2DHashGrid::is_static(ID p_id) const {
    auto E = element_map.find(p_id);
    ERR_FAIL_COND_V(E == element_map.end(), false);
    return E->second._static;
}
int BroadPhase2DHashGrid::get_subindex(ID p_id) const {
    auto E = element_map.find(p_id);
    ERR_FAIL_COND_V(E == element_map.end(), -1);
    return E->second.subindex;
}

template <bool use_aabb, bool use_segment>
void BroadPhase2DHashGrid::_cull(const Point2i p_cell, const Rect2 &p_aabb, const Point2 &p_from, const Point2 &p_to,
        CollisionObject2DSW **p_results, int p_max_results, int *p_result_indices, int &index) {
    PosKey pk;
    pk.x = p_cell.x;
    pk.y = p_cell.y;

    uint32_t idx = pk.hash() % hash_table_size;
    PosBin *pb = hash_table[idx];

    while (pb) {
        if (pb->key == pk) {
            break;
        }

        pb = pb->next;
    }

    if (!pb) {
        return;
    }

    for (eastl::pair<Element *, RC> E : pb->object_set) {
        if (index >= p_max_results) {
            break;
        }
        if (E.first->pass == pass) {
            continue;
        }

        E.first->pass = pass;

        if (use_aabb && !p_aabb.intersects(E.first->aabb)) {
            continue;
        }

        if (use_segment && !E.first->aabb.intersects_segment(p_from, p_to)) {
            continue;
        }

        p_results[index] = E.first->owner;
        p_result_indices[index] = E.first->subindex;
        index++;
    }

    for (const eastl::pair<Element *const, RC> &E : pb->static_object_set) {
        if (index >= p_max_results) {
            break;
        }
        if (E.first->pass == pass) {
            continue;
        }

        if (use_aabb && !p_aabb.intersects(E.first->aabb)) {
            continue;
        }

        if (use_segment && !E.first->aabb.intersects_segment(p_from, p_to)) {
            continue;
        }

        E.first->pass = pass;
        p_results[index] = E.first->owner;
        p_result_indices[index] = E.first->subindex;
        index++;
    }
}

int BroadPhase2DHashGrid::cull_segment(const Vector2 &p_from, const Vector2 &p_to, CollisionObject2DSW **p_results,
        int p_max_results, int *p_result_indices) {
    pass++;

    Vector2 dir = (p_to - p_from);
    if (dir == Vector2()) {
        return 0;
    }
    // avoid divisions by zero
    dir.normalize();
    if (dir.x == 0.0f) {
        dir.x = 0.000001f;
    }
    if (dir.y == 0.0f) {
        dir.y = 0.000001f;
    }
    Vector2 delta = dir.abs();

    delta.x = cell_size / delta.x;
    delta.y = cell_size / delta.y;

    Point2i pos = (p_from / cell_size).floor();
    Point2i end = (p_to / cell_size).floor();

    Point2i step = Vector2(SGN(dir.x), SGN(dir.y));

    Vector2 max;

    if (dir.x < 0) {
        max.x = (Math::floor((double)pos.x) * cell_size - p_from.x) / dir.x;
    } else {
        max.x = (Math::floor((double)pos.x + 1) * cell_size - p_from.x) / dir.x;
    }

    if (dir.y < 0) {
        max.y = (Math::floor((double)pos.y) * cell_size - p_from.y) / dir.y;
    } else {
        max.y = (Math::floor((double)pos.y + 1) * cell_size - p_from.y) / dir.y;
    }

    int cullcount = 0;
    _cull<false, true>(pos, Rect2(), p_from, p_to, p_results, p_max_results, p_result_indices, cullcount);

    bool reached_x = false;
    bool reached_y = false;

    while (true) {
        if (max.x < max.y) {
            max.x += delta.x;
            pos.x += step.x;
        } else {
            max.y += delta.y;
            pos.y += step.y;
        }

        if (step.x > 0) {
            if (pos.x >= end.x) {
                reached_x = true;
            }
        } else if (pos.x <= end.x) {
            reached_x = true;
        }

        if (step.y > 0) {
            if (pos.y >= end.y) {
                reached_y = true;
            }
        } else if (pos.y <= end.y) {
            reached_y = true;
        }

        _cull<false, true>(pos, Rect2(), p_from, p_to, p_results, p_max_results, p_result_indices, cullcount);

        if (reached_x && reached_y) {
            break;
        }
    }

    for (const eastl::pair<Element *const, RC> &E : large_elements) {
        if (cullcount >= p_max_results) {
            break;
        }
        if (E.first->pass == pass) {
            continue;
        }

        E.first->pass = pass;

        /*
        if (use_aabb && !p_aabb.intersects(E.first->aabb))
            continue;
        */

        if (!E.first->aabb.intersects_segment(p_from, p_to)) {
            continue;
        }

        p_results[cullcount] = E.first->owner;
        p_result_indices[cullcount] = E.first->subindex;
        cullcount++;
    }

    return cullcount;
}

int BroadPhase2DHashGrid::cull_aabb(
        const Rect2 &p_aabb, CollisionObject2DSW **p_results, int p_max_results, int *p_result_indices) {
    pass++;

    Point2i from = (p_aabb.position / cell_size).floor();
    Point2i to = ((p_aabb.position + p_aabb.size) / cell_size).floor();
    int cullcount = 0;

    for (int i = from.x; i <= to.x; i++) {
        for (int j = from.y; j <= to.y; j++) {
            _cull<true, false>(
                    Point2i(i, j), p_aabb, Point2(), Point2(), p_results, p_max_results, p_result_indices, cullcount);
        }
    }

    for (eastl::pair<Element *, RC> E : large_elements) {
        if (cullcount >= p_max_results) {
            break;
        }
        if (E.first->pass == pass) {
            continue;
        }

        E.first->pass = pass;

        if (!p_aabb.intersects(E.first->aabb)) {
            continue;
        }

        /*
        if (!E.first->aabb.intersects_segment(p_from,p_to))
            continue;
        */

        p_results[cullcount] = E.first->owner;
        p_result_indices[cullcount] = E.first->subindex;
        cullcount++;
    }
    return cullcount;
}

void BroadPhase2DHashGrid::set_pair_callback(PairCallback p_pair_callback, void *p_userdata) {
    pair_callback = p_pair_callback;
    pair_userdata = p_userdata;
}

void BroadPhase2DHashGrid::set_unpair_callback(UnpairCallback p_unpair_callback, void *p_userdata) {
    unpair_callback = p_unpair_callback;
    unpair_userdata = p_userdata;
}

void BroadPhase2DHashGrid::update() {}

BroadPhase2DSW *BroadPhase2DHashGrid::_create() {
    return memnew(BroadPhase2DHashGrid);
}

BroadPhase2DHashGrid::BroadPhase2DHashGrid() {
    hash_table_size = T_GLOBAL_DEF<int>("physics/2d/bp_hash_table_size", 4096);
    ProjectSettings::get_singleton()->set_custom_property_info(
            "physics/2d/bp_hash_table_size", PropertyInfo(VariantType::INT, "physics/2d/bp_hash_table_size",
                                                     PropertyHint::Range, "0,8192,1,or_greater"));
    hash_table_size = Math::larger_prime(hash_table_size);
    hash_table = memnew_arr(PosBin *, hash_table_size);

    cell_size = T_GLOBAL_DEF<int>("physics/2d/cell_size", 128);
    ProjectSettings::get_singleton()->set_custom_property_info("physics/2d/cell_size",
            PropertyInfo(VariantType::INT, "physics/2d/cell_size", PropertyHint::Range, "0,512,1,or_greater"));

    large_object_min_surface = T_GLOBAL_DEF<int>("physics/2d/large_object_surface_threshold_in_cells", 512);
    ProjectSettings::get_singleton()->set_custom_property_info("physics/2d/large_object_surface_threshold_in_cells",
            PropertyInfo(VariantType::INT, "physics/2d/large_object_surface_threshold_in_cells", PropertyHint::Range,
                    "0,1024,1,or_greater"));

    for (uint32_t i = 0; i < hash_table_size; i++) {
        hash_table[i] = nullptr;
    }
    pass = 1;

    current = 0;
}

BroadPhase2DHashGrid::~BroadPhase2DHashGrid() {
    for (uint32_t i = 0; i < hash_table_size; i++) {
        while (hash_table[i]) {
            PosBin *pb = hash_table[i];
            hash_table[i] = pb->next;
            memdelete(pb);
        }
    }

    memdelete_arr(hash_table);
}

/* 3D version of voxel traversal:

public IEnumerable<Point3D> GetCellsOnRay(Ray ray, int maxDepth)
{
    // Implementation is based on:
    // "A Fast Voxel Traversal Algorithm for Ray Tracing"
    // John Amanatides, Andrew Woo
    // http://www.cse.yorku.ca/~amana/research/grid.pdf
    //
https://web.archive.org/web/20100616193049/http://www.devmaster.net/articles/raytracing_series/A%20faster%20voxel%20traversal%20algorithm%20for%20ray%20tracing.pdf

    // NOTES:
    // * This code assumes that the ray's position and direction are in 'cell coordinates', which means
    //   that one unit equals one cell in all directions.
    // * When the ray doesn't start within the voxel grid, calculate the first position at which the
    //   ray could enter the grid. If it never enters the grid, there is nothing more to do here.
    // * Also, it is important to test when the ray exits the voxel grid when the grid isn't infinite.
    // * The Point3D structure is a simple structure having three integer fields (X, Y and Z).

    // The cell in which the ray starts.
    Point3D start = GetCellAt(ray.Position);        // Rounds the position's X, Y and Z down to the nearest integer
values. int x = start.X; int y = start.Y; int z = start.Z;

    // Determine which way we go.
    int stepX = Math.Sign(ray.Direction.X);
    int stepY = Math.Sign(ray.Direction.Y);
    int stepZ = Math.Sign(ray.Direction.Z);

    // Calculate cell boundaries. When the step (i.e. direction sign) is positive,
    // the next boundary is AFTER our current position, meaning that we have to add 1.
    // Otherwise, it is BEFORE our current position, in which case we add nothing.
    Point3D cellBoundary = new Point3D(
    x + (stepX > 0 ? 1 : 0),
    y + (stepY > 0 ? 1 : 0),
    z + (stepZ > 0 ? 1 : 0));

    // NOTE: For the following calculations, the result will be Single.PositiveInfinity
    // when ray.Direction.X, Y or Z equals zero, which is OK. However, when the left-hand
    // value of the division also equals zero, the result is Single.NaN, which is not OK.

    // Determine how far we can travel along the ray before we hit a voxel boundary.
    Vector3 tMax = new Vector3(
    (cellBoundary.X - ray.Position.X) / ray.Direction.X,    // Boundary is a plane on the YZ axis.
    (cellBoundary.Y - ray.Position.Y) / ray.Direction.Y,    // Boundary is a plane on the XZ axis.
    (cellBoundary.Z - ray.Position.Z) / ray.Direction.Z);    // Boundary is a plane on the XY axis.
    if (Single.IsNaN(tMax.X)) tMax.X = Single.PositiveInfinity;
    if (Single.IsNaN(tMax.Y)) tMax.Y = Single.PositiveInfinity;
    if (Single.IsNaN(tMax.Z)) tMax.Z = Single.PositiveInfinity;

    // Determine how far we must travel along the ray before we have crossed a gridcell.
    Vector3 tDelta = new Vector3(
    stepX / ray.Direction.X,                    // Crossing the width of a cell.
    stepY / ray.Direction.Y,                    // Crossing the height of a cell.
    stepZ / ray.Direction.Z);                    // Crossing the depth of a cell.
    if (Single.IsNaN(tDelta.X)) tDelta.X = Single.PositiveInfinity;
    if (Single.IsNaN(tDelta.Y)) tDelta.Y = Single.PositiveInfinity;
    if (Single.IsNaN(tDelta.Z)) tDelta.Z = Single.PositiveInfinity;

    // For each step, determine which distance to the next voxel boundary is lowest (i.e.
    // which voxel boundary is nearest) and walk that way.
    for (int i = 0; i < maxDepth; i++)
    {
    // Return it.
    yield return new Point3D(x, y, z);

    // Do the next step.
    if (tMax.X < tMax.Y && tMax.X < tMax.Z)
    {
        // tMax.X is the lowest, an YZ cell boundary plane is nearest.
        x += stepX;
        tMax.X += tDelta.X;
    }
    else if (tMax.Y < tMax.Z)
    {
        // tMax.Y is the lowest, an XZ cell boundary plane is nearest.
        y += stepY;
        tMax.Y += tDelta.Y;
    }
    else
    {
        // tMax.Z is the lowest, an XY cell boundary plane is nearest.
        z += stepZ;
        tMax.Z += tDelta.Z;
    }
    }

    */
