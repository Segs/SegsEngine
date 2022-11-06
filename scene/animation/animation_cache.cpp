/*************************************************************************/
/*  animation_cache.cpp                                                  */
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

#include "animation_cache.h"


#include "core/callable_method_pointer.h"
#include "core/method_bind.h"

#include "scene/resources/animation.h"

IMPL_GDCLASS(AnimationCache)

void AnimationCache::_node_exit_tree(Node *p_node) {

    //it is one shot, so it disconnects upon arrival

    ERR_FAIL_COND(!connected_nodes.contains(p_node));

    connected_nodes.erase(p_node);

    for (int i = 0; i < path_cache.size(); i++) {

        if (path_cache[i].node != p_node)
            continue;

        path_cache[i].valid = false; //invalidate path cache
    }
}

void AnimationCache::_animation_changed() {

    _clear_cache();
}

void AnimationCache::_clear_cache() {

    while (!connected_nodes.empty()) {

        (*connected_nodes.begin())->disconnect("tree_exiting",callable_mp(this, &ClassName::_node_exit_tree));
        connected_nodes.erase(connected_nodes.begin());
    }
    path_cache.clear();
    cache_valid = false;
    cache_dirty = true;
}

void AnimationCache::_update_cache() {

    cache_valid = false;

    ERR_FAIL_COND(!root);
    ERR_FAIL_COND(!root->is_inside_tree());
    ERR_FAIL_COND(not animation);

    for (int i = 0; i < animation->get_track_count(); i++) {

        NodePath np = animation->track_get_path(i);

        Node *node = root->get_node(np);
        if (!node) {

            path_cache.push_back(Path());
            ERR_CONTINUE_MSG(!node, "Invalid track path in animation: " + (String)np + ".");
        }

        Path path;

        Ref<Resource> res;

        if (animation->track_get_type(i) == Animation::TYPE_TRANSFORM) {

            if (np.get_subname_count() > 1) {
                path_cache.push_back(Path());
                ERR_CONTINUE_MSG(animation->track_get_type(i) == Animation::TYPE_TRANSFORM, "Transform tracks can't have a subpath: " + (String)np + ".");
            }

            Node3D *sp = object_cast<Node3D>(node);

            if (!sp) {

                path_cache.push_back(Path());
                ERR_CONTINUE_MSG(!sp, "Transform track not of type Node3D: " + (String)np + ".");
            }

            if (np.get_subname_count() == 1) {
                StringName property = np.get_subname(0);


                Skeleton *sk = object_cast<Skeleton>(node);
                if (!sk) {

                    path_cache.push_back(Path());
                    ERR_PRINT("Property defined in Transform track, but not a Skeleton!: " + (String)np + ".");
                    continue;
                }

                int idx = sk->find_bone(property);
                if (idx == -1) {
                    path_cache.push_back(Path());
                    ERR_PRINT("Property defined in Transform track, but not a Skeleton Bone!: " + (String)np + ".");
                    continue;
                }

                path.bone_idx = idx;
                path.skeleton = sk;
            }

            path.spatial = sp;

        } else {
            if (np.get_subname_count() > 0) {

                RES res2;
                Vector<StringName> leftover_subpath;

                // We don't want to cache the last resource unless it is a method call
                bool is_method = animation->track_get_type(i) == Animation::TYPE_METHOD;
                root->get_node_and_resource(np, res2, leftover_subpath, is_method);

                if (res2) {
                    path.resource = res2;
                } else {
                    path.node = node;
                }
                path.object = res2 ? res2.get() : (Object *)node;
                path.subpath = leftover_subpath;

            } else {

                path.node = node;
                path.object = node;
                path.subpath = np.get_subnames();
            }
        }

        if (animation->track_get_type(i) == Animation::TYPE_VALUE) {

            if (np.get_subname_count() == 0) {

                path_cache.push_back(Path());
                ERR_CONTINUE_MSG(np.get_subname_count() == 0, "Value Track lacks property: " + (String)np + ".");
            }

        } else if (animation->track_get_type(i) == Animation::TYPE_METHOD) {

            if (!path.subpath.empty()) { // Trying to call a method of a non-resource

                path_cache.push_back(Path());
                ERR_CONTINUE_MSG(!path.subpath.empty(), "Method Track has property: " + (String)np + ".");
            }
        }

        path.valid = true;

        path_cache.push_back(path);

        if (!connected_nodes.contains(path.node)) {
            connected_nodes.insert(path.node);
            path.node->connect("tree_exiting", callable_gen(this, [=]() { _node_exit_tree(path.node);}), ObjectNS::CONNECT_ONESHOT);
        }
    }

    cache_dirty = false;
    cache_valid = true;
}

void AnimationCache::set_track_transform(int p_idx, const Transform &p_transform) {

    if (cache_dirty)
        _update_cache();

    ERR_FAIL_COND(!cache_valid);
    ERR_FAIL_INDEX(p_idx, path_cache.size());
    Path &p = path_cache[p_idx];
    if (!p.valid)
        return;

    ERR_FAIL_COND(!p.node);
    ERR_FAIL_COND(!p.spatial);

    if (p.skeleton) {
        p.skeleton->set_bone_pose(p.bone_idx, p_transform);
    } else {
        p.spatial->set_transform(p_transform);
    }
}

void AnimationCache::set_track_value(int p_idx, const Variant &p_value) {

    if (cache_dirty)
        _update_cache();

    ERR_FAIL_COND(!cache_valid);
    ERR_FAIL_INDEX(p_idx, path_cache.size());
    Path &p = path_cache[p_idx];
    if (!p.valid)
        return;

    ERR_FAIL_COND(!p.object);
    p.object->set_indexed(p.subpath, p_value);
}

void AnimationCache::call_track(int p_idx, const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    if (cache_dirty)
        _update_cache();

    ERR_FAIL_COND(!cache_valid);
    ERR_FAIL_INDEX(p_idx, path_cache.size());
    Path &p = path_cache[p_idx];
    if (!p.valid)
        return;

    ERR_FAIL_COND(!p.object);
    p.object->call(p_method, p_args, p_argcount, r_error);
}

void AnimationCache::set_all(float p_time, float p_delta) {

    if (cache_dirty)
        _update_cache();

    ERR_FAIL_COND(!cache_valid);

    int tc = animation->get_track_count();
    for (int i = 0; i < tc; i++) {

        switch (animation->track_get_type(i)) {

            case Animation::TYPE_TRANSFORM: {

                Vector3 loc, scale;
                Quat rot;
                animation->transform_track_interpolate(i, p_time, &loc, &rot, &scale);
                Transform tr(Basis(rot), loc);
                tr.basis.scale(scale);

                set_track_transform(i, tr);

            } break;
            case Animation::TYPE_VALUE: {

                if (animation->value_track_get_update_mode(i) == Animation::UPDATE_CONTINUOUS || (animation->value_track_get_update_mode(i) == Animation::UPDATE_DISCRETE && p_delta == 0)) {
                    Variant v = animation->value_track_interpolate(i, p_time);
                    set_track_value(i, v);
                } else {

                    Vector<int> indices;
                    animation->value_track_get_key_indices(i, p_time, p_delta, &indices);

                    for (int E : indices) {

                        Variant v = animation->track_get_key_value(i, E);
                        set_track_value(i, v);
                    }
                }

            } break;
            case Animation::TYPE_METHOD: {

                Vector<int> indices;
                animation->method_track_get_key_indices(i, p_time, p_delta, &indices);

                for (int E : indices) {

                    const Vector<Variant> &args = animation->method_track_get_params(i, E);
                    StringName name = animation->method_track_get_name(i, E);
                    Callable::CallError err;

                    if (args.empty()) {

                        call_track(i, name, nullptr, 0, err);
                    } else {

                        Vector<const Variant *> argptrs;
                        argptrs.resize(args.size());
                        for (int j = 0; j < args.size(); j++) {

                            argptrs[j] = &args[j];
                        }

                        call_track(i, name, (const Variant **)&argptrs[0], args.size(), err);
                    }
                }

            } break;
            default: {
            }
        }
    }
}

void AnimationCache::set_animation(const Ref<Animation> &p_animation) {

    _clear_cache();

    if (animation)
        animation->disconnect("changed",callable_mp(this, &ClassName::_animation_changed));

    animation = p_animation;

    if (animation)
        animation->connect("changed",callable_mp(this, &ClassName::_animation_changed));
}

void AnimationCache::_bind_methods() {

    SE_BIND_METHOD(AnimationCache,_node_exit_tree);
    SE_BIND_METHOD(AnimationCache,_animation_changed);
}

void AnimationCache::set_root(Node *p_root) {

    _clear_cache();
    root = p_root;
}

AnimationCache::AnimationCache() {

    root = nullptr;
    cache_dirty = true;
    cache_valid = false;
}
