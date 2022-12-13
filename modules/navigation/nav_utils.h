/*************************************************************************/
/*  nav_utils.h                                                          */
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

#include "core/math/vector3.h"
#include "core/hashfuncs.h"
#include "core/rid.h"
#include "core/vector.h"


class NavRegion;

namespace gd {
struct Polygon;

union PointKey {

    struct {
        int64_t x : 21;
        int64_t y : 22;
        int64_t z : 21;
    };

    uint64_t key;
    bool operator<(const PointKey &p_key) const { return key < p_key.key; }
};

struct EdgeKey {

    PointKey a;
    PointKey b;

    EdgeKey(const PointKey &p_a = PointKey(), const PointKey &p_b = PointKey()) :
            a(p_a),
            b(p_b) {
        if (a.key > b.key) {
            SWAP(a, b);
        }
    }
    bool operator==(const EdgeKey &other) const {
        return a.key==other.a.key && b.key==other.b.key;
    }
    // For default eastl::hash operator
    explicit operator size_t() const noexcept {
        return hash_djb2_buffer64((const uint8_t *)this,next_power_of_2(sizeof(EdgeKey)));
    }
};

struct Point {
    Vector3 pos;
    PointKey key;
};

struct Edge {
    /// This edge ID
    int this_edge = -1;

    /// Other Polygon
    struct Connection {
        Polygon *polygon = nullptr;
        int edge = -1;
        Vector3 pathway_start;
        Vector3 pathway_end;
    };
    Vector<Connection> connections;
};

struct Polygon {
    NavRegion *owner = nullptr;

    /// The points of this `Polygon`
    Vector<Point> points;

    /// Are the points clockwise ?
    bool clockwise;

    /// The edges of this `Polygon`
    Vector<Edge> edges;

    /// The center of this `Polygon`
    Vector3 center;
};


struct NavigationPoly {
    uint32_t self_id = 0;
    /// This poly.
    const Polygon *poly = nullptr;

    /// Those 4 variables are used to travel the path backwards.
    int back_navigation_poly_id = -1;
    uint32_t back_navigation_edge = UINT32_MAX;
    Vector3 back_navigation_edge_pathway_start;
    Vector3 back_navigation_edge_pathway_end;
    /// The entry location of this poly.
    Vector3 entry;
    /// The distance to the destination.
    float traveled_distance = 0.0;

    NavigationPoly() { }

    NavigationPoly(const Polygon *p_poly) :
            poly(p_poly) {}

    bool operator==(const NavigationPoly &other) const {
        return this->poly == other.poly;
    }

    bool operator!=(const NavigationPoly &other) const {
        return !operator==(other);
    }
};

struct ClosestPointQueryResult {
    Vector3 point;
    Vector3 normal;
    RID owner;
};
} // namespace gd
