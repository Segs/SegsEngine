/*************************************************************************/
/*  room.h                                                               */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "core/vector.h"
#include "core/rid.h"
#include "node_3d.h"
#include "core/math/geometry.h"

class Portal;

class Room : public Node3D {
    GDCLASS(Room, Node3D);

    friend class RoomManager;
    friend class RoomGroup;
    friend class Portal;
    friend class RoomGizmoPlugin;
    friend class RoomEditorPlugin;
    friend class RoomSpatialGizmo;

    RenderingEntity _room_rid;

public:
    struct SimplifyInfo {
        SimplifyInfo() { set_simplify(0.5); }
        void set_simplify(real_t p_value, real_t p_room_size = 0.0);
        bool add_plane_if_unique(Vector<Plane> &r_planes, const Plane &p) const;
        real_t _plane_simplify = 0.5;
        real_t _plane_simplify_dot = 0.98f;
        real_t _plane_simplify_dist = 0.08f;
    };

    Room();
    ~Room() override;

    void set_room_simplify(real_t p_value);
    real_t get_room_simplify() const { return _simplify_info._plane_simplify; }

    // whether to use the room manager default
    void set_use_default_simplify(bool p_use);
    bool get_use_default_simplify() const { return _use_default_simplify; }

    void set_points(const Vector<Vector3> &p_points);
    const Vector<Vector3> &get_points() const;

    // primarily for the gizmo
    void set_point(int p_idx, const Vector3 &p_point);

    // editor only
    PoolVector<Vector3> generate_points();

    String get_configuration_warning() const override;

private:
    // call during each conversion
    void clear();

    void _changed(bool p_regenerate_bounds = false);
    template <class T>
    static bool detect_nodes_of_type(const Node *p_node, bool p_ignore_first_node = true);
    template <typename T>
    static bool detect_nodes_using_lambda(const Node *p_node, T p_lambda, bool p_ignore_first_node = true);

    // note this is client side, and does not use the final planes stored in the PortalRenderer
    bool contains_point(const Vector3 &p_pt) const;

    // planes forming convex hull of room
    Vector<Plane> _planes;

    // preliminary planes are created during the first conversion pass,
    // they do not include the portals, and are used for identifying auto
    // linkage of rooms by portals
    Vector<Plane> _preliminary_planes;

    GeometryMeshData _bound_mesh_data;
    AABB _aabb;

    // editable points making up the bound
    Vector<Vector3> _bound_pts;

#ifdef TOOLS_ENABLED
    // to help with editing, when converting, we can generate overlap zones
    // that occur between rooms. Ideally these should not occur, as rooms
    // should be convex and non-overlapping. But if they do occur, they should
    // be minimized.
    Vector<GeometryMeshData> _gizmo_overlap_zones;
#endif

    // makes sure lrooms are not converted more than once per
    // call to rooms_convert
    int _conversion_tick = -1;

    // room ID during conversion, used for matching portals links to rooms
    int _room_ID;

    // room priority allows rooms to be placed inside other rooms,
    // such as a house on a landscape room.
    // If the camera is inside more than one room, the higher priority room
    // will *win* (e.g. house, rather than landscape)
    int _room_priority = 0;

    // a room may be in one or several roomgroups
    Vector<int> _roomgroups;

    // list of portal ids from or to this room, just used in conversion to determine room bound
    Vector<int> _portals;

    // each room now stores simplification data
    SimplifyInfo _simplify_info;
    bool _use_default_simplify = true;

protected:
    static void _bind_methods();
    void _notification(int p_what);
};

template <class T>
bool Room::detect_nodes_of_type(const Node *p_node, bool p_ignore_first_node) {
    if (object_cast<T>(p_node) && (!p_ignore_first_node)) {
        return true;
    }

    for (int n = 0; n < p_node->get_child_count(); n++) {
        if (detect_nodes_of_type<T>(p_node->get_child(n), false)) {
            return true;
        }
    }

    return false;
}

template <typename T>
bool Room::detect_nodes_using_lambda(const Node *p_node, T p_lambda, bool p_ignore_first_node) {
    if (p_lambda(p_node) && !p_ignore_first_node) {
        return true;
    }

    for (int n = 0; n < p_node->get_child_count(); n++) {
        if (detect_nodes_using_lambda(p_node->get_child(n), p_lambda, false)) {
            return true;
        }
    }
    return false;
}
