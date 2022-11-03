/*************************************************************************/
/*  skeleton_3d.h                                                        */
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

#include "core/rid.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/skin.h"
#include "core/string.h"

#ifndef _3D_DISABLED
using BoneId = int;

class PhysicalBone3D;
#endif // _3D_DISABLED

class Skeleton;

class GODOT_EXPORT SkinReference : public RefCounted {
    GDCLASS(SkinReference, RefCounted)
    friend class Skeleton;

    Vector<uint32_t> skin_bone_indices;
    Skeleton *skeleton_node = nullptr;
    RenderingEntity skeleton;
    Ref<Skin> skin;
    uint32_t *skin_bone_indices_ptrs = nullptr;
    uint64_t skeleton_version = 0;
    uint32_t bind_count = 0;
    void _skin_changed();

protected:
    static void _bind_methods();

public:
    Skeleton *get_skeleton_node() const { return skeleton_node; }
    RenderingEntity get_skeleton() const { return skeleton; }
    Ref<Skin> get_skin() const;
    ~SkinReference() override;
};
class GODOT_EXPORT Skeleton : public Node3D {

    GDCLASS(Skeleton,Node3D)
private:
    friend class SkinReference;
    struct Bone {

        String name;

        bool enabled;
        int parent;
        int sort_index; //used for re-sorting process order


        bool disable_rest;
        Transform rest;

        Transform pose;
        Transform pose_global;
        Transform pose_global_no_override;

        bool custom_pose_enable;
        Transform custom_pose;
        float global_pose_override_amount;
        bool global_pose_override_reset;
        Transform global_pose_override;

#ifndef _3D_DISABLED
        PhysicalBone3D* physical_bone;
        PhysicalBone3D* cache_parent_physical_bone;
#endif // _3D_DISABLED

        Vector<GameEntity> nodes_bound;

        Bone() {
            parent = -1;
            enabled = true;
            disable_rest = false;
            custom_pose_enable = false;
            global_pose_override_amount = 0;
            global_pose_override_reset = false;
#ifndef _3D_DISABLED
            physical_bone = nullptr;
            cache_parent_physical_bone = nullptr;
#endif // _3D_DISABLED
        }
    };


    HashSet<SkinReference *> skin_bindings;
    Vector<Bone> bones;
    Vector<int> process_order;
    bool process_order_dirty;
    bool dirty;

    uint64_t version;

    void _skin_changed();
    void _make_dirty();
public:
    // bind helpers
    Array _get_bound_child_nodes_to_bone(int p_bone) const {

        Array bound;
        Vector<Node *> children;
        get_bound_child_nodes_to_bone(p_bone, &children);
        for (int i = 0; i < children.size(); i++) {
            bound.push_back(Variant(children[i]));
        }
        return bound;
    }

    void _update_process_order();

protected:
    bool _get(const StringName &p_path, Variant &r_ret) const;
    bool _set(const StringName &p_path, const Variant &p_value);
    void _get_property_list(Vector<PropertyInfo> *p_list) const;
    void _notification(int p_what);
    static void _bind_methods();

public:
    enum {

        NOTIFICATION_UPDATE_SKELETON = 50
    };


    // skeleton creation api
    void add_bone(StringView p_name);
    int find_bone(StringView p_name) const;
    const String &get_bone_name(int p_bone) const;
    void set_bone_name(int p_bone, const String &p_name);

    bool is_bone_parent_of(int p_bone_id, int p_parent_bone_id) const;

    void set_bone_parent(int p_bone, int p_parent);
    int get_bone_parent(int p_bone) const;

    void unparent_bone_and_rest(int p_bone);


    void set_bone_disable_rest(int p_bone, bool p_disable);
    bool is_bone_rest_disabled(int p_bone) const;

    int get_bone_count() const;

    void set_bone_rest(int p_bone, const Transform &p_rest);
    Transform get_bone_rest(int p_bone) const;
    Transform get_bone_global_pose(int p_bone) const;
    Transform get_bone_global_pose_no_override(int p_bone) const;

    void clear_bones_global_pose_override();
    void set_bone_global_pose_override(int p_bone, const Transform &p_pose, float p_amount, bool p_persistent = false);

    void set_bone_enabled(int p_bone, bool p_enabled);
    bool is_bone_enabled(int p_bone) const;

    void bind_child_node_to_bone(int p_bone, Node *p_node);
    void unbind_child_node_from_bone(int p_bone, Node *p_node);
    void get_bound_child_nodes_to_bone(int p_bone, Vector<Node *> *p_bound) const;

    void clear_bones();

    // posing api

    void set_bone_pose(int p_bone, const Transform &p_pose);
    Transform get_bone_pose(int p_bone) const;

    void set_bone_custom_pose(int p_bone, const Transform &p_custom_pose);
    Transform get_bone_custom_pose(int p_bone) const;

    void localize_rests(); // used for loaders and tools
    int get_process_order(int p_idx);

    Ref<SkinReference> register_skin(const Ref<Skin> &p_skin);

#ifndef _3D_DISABLED
    // Physical bone API

    void bind_physical_bone_to_bone(int p_bone, PhysicalBone3D *p_physical_bone);
    void unbind_physical_bone_from_bone(int p_bone);

    PhysicalBone3D *get_physical_bone(int p_bone);
    PhysicalBone3D *get_physical_bone_parent(int p_bone);

private:
    /// This is a slow API os it's cached
    PhysicalBone3D *_get_physical_bone_parent(int p_bone);
    void _rebuild_physical_bones_cache();

public:
    void physical_bones_stop_simulation();
    void physical_bones_start_simulation_on(const Array &p_bones);
    void physical_bones_add_collision_exception(RID p_exception);
    void physical_bones_remove_collision_exception(RID p_exception);
#endif // _3D_DISABLED

public:
    Skeleton();
    ~Skeleton() override;
};
