/*************************************************************************/
/*  ray_cast_3d.h                                                        */
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

#include "scene/3d/node_3d.h"
#include "core/color.h"
#include "core/hash_set.h"
#include "core/rid.h"

class Material;
class SpatialMaterial;

class GODOT_EXPORT RayCast3D : public Node3D {

    GDCLASS(RayCast3D,Node3D)

    HashSet<RID> exclude;
    Vector3 collision_point;
    Vector3 collision_normal;
    Vector3 cast_to{ 0, -1, 0 };
    Ref<Material> debug_material;
    Color debug_shape_custom_color = Color(0.0, 0.0, 0.0);
    int debug_shape_thickness = 2;
    Vector<Vector3> debug_shape_vertices;
    Vector<Vector3> debug_line_vertices;
    Node *debug_shape = nullptr;

    GameEntity against{ entt::null };
    int against_shape = 0;
    uint32_t collision_mask = 1;
    bool exclude_parent_body = true;
    bool collide_with_areas = false;
    bool collide_with_bodies = true;
    bool enabled = false;
    bool collided = false;



    void _create_debug_shape();
    void _update_debug_shape();
    void _update_debug_shape_material(bool p_check_collision = false);
    void _update_debug_shape_vertices();
    void _clear_debug_shape();

protected:
    void _notification(int p_what);
    void _update_raycast_state();
    static void _bind_methods();

public:
    void set_collide_with_areas(bool p_clip);
    bool is_collide_with_areas_enabled() const;

    void set_collide_with_bodies(bool p_clip);
    bool is_collide_with_bodies_enabled() const;

    void set_enabled(bool p_enabled);
    bool is_enabled() const;

    void set_cast_to(const Vector3 &p_point);
    Vector3 get_cast_to() const;

    void set_collision_mask(uint32_t p_mask);
    uint32_t get_collision_mask() const;

    void set_collision_mask_bit(int p_bit, bool p_value);
    bool get_collision_mask_bit(int p_bit) const;

    void set_exclude_parent_body(bool p_exclude_parent_body);
    bool get_exclude_parent_body() const;
    const Color &get_debug_shape_custom_color() const;
    void set_debug_shape_custom_color(const Color &p_color);

    const Vector<Vector3> &get_debug_shape_vertices() const;
    const Vector<Vector3> &get_debug_line_vertices() const;

    Ref<SpatialMaterial> get_debug_material();

    int get_debug_shape_thickness() const;
    void set_debug_shape_thickness(int p_debug_thickness);

    void force_raycast_update();
    bool is_colliding() const;
    Object *get_collider() const;
    int get_collider_shape() const;
    Vector3 get_collision_point() const;
    Vector3 get_collision_normal() const;

    void add_exception_rid(const RID &p_rid);
    void add_exception(const Object *p_object);
    void remove_exception_rid(const RID &p_rid);
    void remove_exception(const Object *p_object);
    void clear_exceptions();

    RayCast3D();
};
