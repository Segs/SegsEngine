/*************************************************************************/
/*  spring_arm_3d.h                                                         */
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
#include "core/hash_set.h"
#include "core/rid.h"

class Shape;
class SpringArm3D : public Node3D {
    GDCLASS(SpringArm3D,Node3D)

    Ref<Shape> shape;
    HashSet<RID> excluded_objects;
    float spring_length=1.0f;
    float current_spring_length=0.0f;
    float margin=0.01f;
    uint32_t mask=1;
    bool keep_child_basis=false;

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    void set_length(float p_length);
    float get_length() const;
    void set_shape(const Ref<Shape>& p_shape);
    Ref<Shape> get_shape() const;
    void set_collision_mask(uint32_t p_mask);
    uint32_t get_collision_mask() { return mask; }
    void add_excluded_object(RID p_rid);
    bool remove_excluded_object(RID p_rid);
    void clear_excluded_objects();
    float get_hit_length() const { return current_spring_length; }
    void set_margin(float p_margin);
    float get_margin() const { return margin; }

    SpringArm3D();

private:
    void process_spring();
};
