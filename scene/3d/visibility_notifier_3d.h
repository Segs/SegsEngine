/*************************************************************************/
/*  visibility_notifier_3d.h                                                */
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

#include "core/hash_set.h"
#include "core/hash_map.h"
#include "scene/3d/node_3d.h"

class Camera3D;
class World3D;
class GODOT_EXPORT VisibilityNotifier3D : public Node3D {

    GDCLASS(VisibilityNotifier3D,Node3D)

    HashSet<Camera3D *> cameras;

    Ref<World3D> world;
    AABB aabb = AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));
    Vector3 _world_aabb_center;
    // if using rooms and portals
    RenderingEntity _cull_instance_rid = entt::null;
    bool _in_gameplay = false;
    bool _max_distance_active = false;
    real_t _max_distance=0;
    real_t _max_distance_squared=0;

    // This is a first number of frames where distance objects
    // are forced seen as visible, to make sure their animations
    // and physics positions etc are something reasonable.
    uint32_t _max_distance_leadin_counter=1;

protected:
    virtual void _screen_enter() {}
    virtual void _screen_exit() {}

    void _notification(int p_what);
    static void _bind_methods();
    friend struct SpatialIndexer;

    void _enter_camera(Camera3D *p_camera);
    void _exit_camera(Camera3D *p_camera);

public:
    void set_aabb(const AABB &p_aabb);
    AABB get_aabb() const;
    bool is_on_screen() const;
    // This is only currently kept up to date if max_distance is active
    const Vector3 &get_world_aabb_center() const { return _world_aabb_center; }

    void set_max_distance(real_t p_max_distance);
    real_t get_max_distance() const { return _max_distance; }
    real_t get_max_distance_squared() const { return _max_distance_squared; }
    bool is_max_distance_active() const { return _max_distance_active; }
    bool inside_max_distance_leadin() {
        if (!_max_distance_leadin_counter) {
            return false;
        }
        _max_distance_leadin_counter--;
        return true;
    }

    VisibilityNotifier3D();
    ~VisibilityNotifier3D() override;
};

class GODOT_EXPORT VisibilityEnabler3D : public VisibilityNotifier3D {

    GDCLASS(VisibilityEnabler3D,VisibilityNotifier3D)

public:
    enum Enabler {
        ENABLER_PAUSE_ANIMATIONS,
        ENABLER_FREEZE_BODIES,
        ENABLER_MAX
    };

protected:
    void _screen_enter() override;
    void _screen_exit() override;

    bool visible;

    void _find_nodes(Node *p_node);

    HashMap<Node *, Variant> nodes;
    void _node_removed(Node *p_node);
    bool enabler[ENABLER_MAX];

    void _change_node_state(Node *p_node, bool p_enabled);

    void _notification(int p_what);
    static void _bind_methods();

public:
    void set_enabler(Enabler p_enabler, bool p_enable);
    bool is_enabler_enabled(Enabler p_enabler) const;

    VisibilityEnabler3D();
};

