/*************************************************************************/
/*  animation_player.cpp                                                 */
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

#include "animation_player.h"

#include "core/engine.h"
#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/message_queue.h"
#include "core/object_tooling.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/scene_string_names.h"
#include "servers/audio/audio_stream.h"
#include "EASTL/sort.h"
#include "EASTL/vector_set.h"

IMPL_GDCLASS(AnimationPlayer)

VARIANT_ENUM_CAST(AnimationPlayer::AnimationProcessMode);
VARIANT_ENUM_CAST(AnimationPlayer::AnimationMethodCallMode);

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "scene/2d/skeleton_2d.h"

void AnimatedValuesBackup::update_skeletons() {

    for (int i = 0; i < entries.size(); i++) {
        if (entries[i].bone_idx != -1) {
            // 3D bone
            object_cast<Skeleton>(entries[i].object)->notification(Skeleton::NOTIFICATION_UPDATE_SKELETON);
        } else {
            Bone2D *bone = object_cast<Bone2D>(entries[i].object);
            if (bone && bone->skeleton) {
                // 2D bone
                bone->skeleton->_update_transform();
            }
        }
    }
}
void AnimatedValuesBackup::restore() const
{
    for (const AnimatedValuesBackup::Entry &entry : entries) {
        if (entry.bone_idx == -1) {
            entry.object->set_indexed(entry.subpath, entry.value);
        } else {
            object_cast<Skeleton>(entry.object)->set_bone_pose(entry.bone_idx, entry.value.as<Transform>());
        }
    }
}
#endif

bool AnimationPlayer::_set(const StringName &p_name, const Variant &p_value) {

    const StringView name(p_name);

    if (StringUtils::begins_with(name,"playback/play")) { // bw compatibility

        set_current_animation(p_value.as<StringName>());

    } else if (StringUtils::begins_with(name,"anims/")) {

        StringView which = StringUtils::get_slice(name,'/', 1);
        add_animation(StringName(which), refFromVariant<Animation>(p_value));

    } else if (StringUtils::begins_with(name,"next/")) {

        StringView which = StringUtils::get_slice(name,'/', 1);
        animation_set_next(StringName(which), p_value.as<StringName>());

    } else if (p_name == SceneStringNames::blend_times) {

        Array array = p_value.as<Array>();
        int len = array.size();
        ERR_FAIL_COND_V(len % 3, false);

        for (int i = 0; i < len / 3; i++) {

            StringName from = array[i * 3 + 0].as<StringName>();
            StringName to = array[i * 3 + 1].as<StringName>();
            float time = array[i * 3 + 2].as<float>();

            set_blend_time(from, to, time);
        }

    } else
        return false;

    return true;
}

bool AnimationPlayer::_get(const StringName &p_name, Variant &r_ret) const {

    const StringView name(p_name);

    if (name == StringView("playback/play")) { // bw compatibility

        r_ret = get_current_animation();

    } else if (StringUtils::begins_with(name,"anims/")) {

        StringView which = StringUtils::get_slice(name,'/', 1);
        r_ret = Variant(get_animation(StringName(which)).get_ref_ptr());

    } else if (StringUtils::begins_with(name,"next/")) {

        StringView which = StringUtils::get_slice(name,'/', 1);

        r_ret = animation_get_next(StringName(which));

    } else if (name == StringView("blend_times")) {

        eastl::vector_set<BlendKey,eastl::less<BlendKey>,wrap_allocator> keys;
        for (const eastl::pair<const BlendKey,float> &E : blend_times) {
            keys.insert(E.first);
        }

        Array array;
        for (int i = 0; i < keys.size(); i++) {

            array.push_back(keys[i].from);
            array.push_back(keys[i].to);
            array.push_back(blend_times.at(keys[i]));
        }

        r_ret = array;
    } else
        return false;

    return true;
}

void AnimationPlayer::_validate_property(PropertyInfo &property) const {

    if (StringView(property.name) != StringView("current_animation"))
        return;

    Vector<StringView> names;
    names.push_back("[stop]");

    for (const eastl::pair<const StringName,AnimationData> &E : animation_set) {
        names.push_back(E.first);
    }
    // begin()+1 so we don't sort the [stop] entry
    eastl::sort(names.begin()+1,names.end());
    String hint = String::joined(names,",");
    property.hint_string = eastl::move(hint);
}

void AnimationPlayer::_get_property_list(Vector<PropertyInfo> *p_list) const {

    Vector<PropertyInfo> anim_names;
    anim_names.reserve(animation_set.size());
    for (const eastl::pair<const StringName,AnimationData> &E : animation_set) {

        anim_names.emplace_back(VariantType::OBJECT, StringName("anims/" + String(E.first)), PropertyHint::ResourceType, "Animation", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_DO_NOT_SHARE_ON_DUPLICATE);
        if (E.second.next != StringName())
            anim_names.emplace_back(VariantType::STRING, StringName("next/" + String(E.first)), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
    }
    eastl::sort(anim_names.begin(),anim_names.end());
    p_list->insert(p_list->end(),anim_names.begin(),anim_names.end());

    p_list->push_back(PropertyInfo(VariantType::ARRAY, "blend_times", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
}

void AnimationPlayer::advance(float p_time) {

    _animation_process(p_time);
}

void AnimationPlayer::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {

            if (!processing) {
                //make sure that a previous process state was not saved
                //only process if "processing" is set
                set_physics_process_internal(false);
                set_process_internal(false);
            }
            //_set_process(false);
            clear_caches();
        } break;
        case NOTIFICATION_READY: {

            if (!Engine::get_singleton()->is_editor_hint() && animation_set.contains(autoplay)) {
                play(autoplay);
                _animation_process(0);
            }
        } break;
        case NOTIFICATION_INTERNAL_PROCESS: {
            if (animation_process_mode == ANIMATION_PROCESS_PHYSICS)
                break;

            if (processing)
                _animation_process(get_process_delta_time());
        } break;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {

            if (animation_process_mode == ANIMATION_PROCESS_IDLE)
                break;

            if (processing)
                _animation_process(get_physics_process_delta_time());
        } break;
        case NOTIFICATION_EXIT_TREE: {

            clear_caches();
        } break;
    }
}

void AnimationPlayer::_ensure_node_caches(AnimationData *p_anim, Node *p_root_override) {

    // Already cached?
    if (p_anim->node_cache.size() == p_anim->animation->get_track_count())
        return;

    Node *parent = p_root_override ? p_root_override : get_node(root);

    ERR_FAIL_COND(!parent);

    const Animation *a = p_anim->animation.operator->();

    p_anim->node_cache.resize(a->get_track_count());

    for (int i = 0; i < a->get_track_count(); i++) {

        p_anim->node_cache[i] = nullptr;
        RES resource;
        Vector<StringName> leftover_path;
        Node *child = parent->get_node_and_resource(a->track_get_path(i), resource, leftover_path);
        ERR_CONTINUE_MSG(!child, "On Animation: '" + p_anim->name + "', couldn't resolve track:  '" + String(a->track_get_path(i)) + "'."); // couldn't find the child node
        const auto id = resource ? resource->get_instance_id() : child->get_instance_id();
        int bone_idx = -1;

        if (a->track_get_path(i).get_subname_count() == 1 && object_cast<Skeleton>(child)) {

            const Skeleton *sk = object_cast<Skeleton>(child);
            bone_idx = sk->find_bone(a->track_get_path(i).get_subname(0));
            if (bone_idx == -1) {

                continue;
            }
        }

        {
            auto lambda=[=]() { _node_removed(child); };
            if (!child->is_connected(SceneStringNames::tree_exiting,callable_gen(this, lambda)))
                child->connect(SceneStringNames::tree_exiting,callable_gen(this, lambda), ObjectNS::CONNECT_ONESHOT);
        }

        TrackNodeCacheKey key;
        key.id = id;
        key.bone_idx = bone_idx;

        if (!node_cache_map.contains(key))
            node_cache_map[key] = TrackNodeCache();

        p_anim->node_cache[i] = &node_cache_map[key];
        p_anim->node_cache[i]->path = a->track_get_path(i);
        p_anim->node_cache[i]->node = child;
        p_anim->node_cache[i]->resource = resource;
        p_anim->node_cache[i]->node_2d = object_cast<Node2D>(child);
        if (a->track_get_type(i) == Animation::TYPE_TRANSFORM) {
            // special cases and caches for transform tracks

            // cache spatial
            p_anim->node_cache[i]->spatial = object_cast<Node3D>(child);
            // cache skeleton
            p_anim->node_cache[i]->skeleton = object_cast<Skeleton>(child);
            if (p_anim->node_cache[i]->skeleton) {
                if (a->track_get_path(i).get_subname_count() == 1) {
                    StringName bone_name = a->track_get_path(i).get_subname(0);

                    p_anim->node_cache[i]->bone_idx = p_anim->node_cache[i]->skeleton->find_bone(bone_name);
                    if (p_anim->node_cache[i]->bone_idx < 0) {
                        // broken track (nonexistent bone)
                        p_anim->node_cache[i]->skeleton = nullptr;
                        p_anim->node_cache[i]->spatial = nullptr;
                        ERR_CONTINUE(p_anim->node_cache[i]->bone_idx < 0);
                    }
                } else {
                    // no property, just use SpatialNode
                    p_anim->node_cache[i]->skeleton = nullptr;
                }
            }
        }

        if (a->track_get_type(i) == Animation::TYPE_VALUE) {

            if (!p_anim->node_cache[i]->property_anim.contains(a->track_get_path(i).get_concatenated_subnames())) {

                TrackNodeCache::PropertyAnim pa;
                pa.subpath = leftover_path;
                pa.object = resource ? (Object *)resource.get() : (Object *)child;
                pa.special = SP_NONE;
                pa.owner = p_anim->node_cache[i];
                if (false && p_anim->node_cache[i]->node_2d) {

                    if (leftover_path.size() == 1 && leftover_path[0] == SceneStringNames::transform_pos)
                        pa.special = SP_NODE2D_POS;
                    else if (leftover_path.size() == 1 && leftover_path[0] == SceneStringNames::transform_rot)
                        pa.special = SP_NODE2D_ROT;
                    else if (leftover_path.size() == 1 && leftover_path[0] == SceneStringNames::transform_scale)
                        pa.special = SP_NODE2D_SCALE;
                }
                p_anim->node_cache[i]->property_anim[a->track_get_path(i).get_concatenated_subnames()] = pa;
            }
        }

        if (a->track_get_type(i) == Animation::TYPE_BEZIER && !leftover_path.empty()) {

            if (!p_anim->node_cache[i]->bezier_anim.contains(a->track_get_path(i).get_concatenated_subnames())) {

                TrackNodeCache::BezierAnim ba;
                ba.bezier_property = leftover_path;
                ba.object = resource ? (Object *)resource.get() : (Object *)child;
                ba.owner = p_anim->node_cache[i];

                p_anim->node_cache[i]->bezier_anim[a->track_get_path(i).get_concatenated_subnames()] = ba;
            }
        }
    }
}

void AnimationPlayer::_animation_process_animation(AnimationData *p_anim, float p_time, float p_delta, float p_interp, bool p_is_current, bool p_seeked, bool p_started) {

    _ensure_node_caches(p_anim);
    ERR_FAIL_COND(p_anim->node_cache.size() != p_anim->animation->get_track_count());

    Animation *a = p_anim->animation.operator->();
    bool can_call = is_inside_tree() && !Engine::get_singleton()->is_editor_hint();

    for (int i = 0; i < a->get_track_count(); i++) {

        // If an animation changes this animation (or it animates itself)
        // we need to recreate our animation cache
        if (p_anim->node_cache.size() != a->get_track_count()) {
            _ensure_node_caches(p_anim);
        }

        TrackNodeCache *nc = p_anim->node_cache[i];

        if (!nc)
            continue; // no node cache for this track, skip it

        if (!a->track_is_enabled(i))
            continue; // do nothing if the track is disabled

        if (a->track_get_key_count(i) == 0)
            continue; // do nothing if track is empty

        switch (a->track_get_type(i)) {

            case Animation::TYPE_TRANSFORM: {

                if (!nc->spatial)
                    continue;

                Vector3 loc;
                Quat rot;
                Vector3 scale;

                Error err = a->transform_track_interpolate(i, p_time, &loc, &rot, &scale);
                //ERR_CONTINUE(err!=OK); //used for testing, should be removed

                if (err != OK)
                    continue;

                if (nc->accum_pass != accum_pass) {
                    ERR_CONTINUE(cache_update_size >= NODE_CACHE_UPDATE_MAX);
                    cache_update[cache_update_size++] = nc;
                    nc->accum_pass = accum_pass;
                    nc->loc_accum = loc;
                    nc->rot_accum = rot;
                    nc->scale_accum = scale;

                } else {

                    nc->loc_accum = nc->loc_accum.linear_interpolate(loc, p_interp);
                    nc->rot_accum = nc->rot_accum.slerp(rot, p_interp);
                    nc->scale_accum = nc->scale_accum.linear_interpolate(scale, p_interp);
                }

            } break;
            case Animation::TYPE_VALUE: {

                if (!nc->node)
                    continue;

                //StringName property=a->track_get_path(i).get_property();

                HashMap<StringName, TrackNodeCache::PropertyAnim>::iterator E = nc->property_anim.find(a->track_get_path(i).get_concatenated_subnames());
                ERR_CONTINUE(E==nc->property_anim.end()); //should it continue, or create a new one?

                TrackNodeCache::PropertyAnim *pa = &E->second;

                Animation::UpdateMode update_mode = a->value_track_get_update_mode(i);

                if (update_mode == Animation::UPDATE_CAPTURE) {

                    if (p_started) {
                        pa->capture = pa->object->get_indexed(pa->subpath);
                    }

                    int key_count = a->track_get_key_count(i);
                    if (key_count == 0)
                        continue; //eeh not worth it

                    float first_key_time = a->track_get_key_time(i, 0);
                    float transition = 1.0;
                    int first_key = 0;

                    if (first_key_time == 0.0f) {
                        //ignore, use for transition
                        if (key_count == 1)
                            continue; //with one key we can't do anything
                        transition = a->track_get_key_transition(i, 0);
                        first_key_time = a->track_get_key_time(i, 1);
                        first_key = 1;
                    }

                    if (p_time < first_key_time) {
                        float c = Math::ease(p_time / first_key_time, transition);
                        Variant first_value = a->track_get_key_value(i, first_key);
                        Variant interp_value;
                        Variant::interpolate(pa->capture, first_value, c, interp_value);

                        if (pa->accum_pass != accum_pass) {
                            ERR_CONTINUE(cache_update_prop_size >= NODE_CACHE_UPDATE_MAX);
                            cache_update_prop[cache_update_prop_size++] = pa;
                            pa->value_accum = interp_value;
                            pa->accum_pass = accum_pass;
                        } else {
                            Variant::interpolate(pa->value_accum, interp_value, p_interp, pa->value_accum);
                        }

                        continue; //handled
                    }
                }

                if (update_mode == Animation::UPDATE_CONTINUOUS || update_mode == Animation::UPDATE_CAPTURE || (p_delta == 0 && update_mode == Animation::UPDATE_DISCRETE)) { //delta == 0 means seek

                    Variant value = a->value_track_interpolate(i, p_time);

                    if (value == Variant())
                        continue;

                    //thanks to trigger mode, this should be solved now..
                    /*
                    if (p_delta==0 && value.get_type()==VariantType::STRING)
                        continue; // doing this with strings is messy, should find another way
                    */
                    if (pa->accum_pass != accum_pass) {
                        ERR_CONTINUE(cache_update_prop_size >= NODE_CACHE_UPDATE_MAX);
                        cache_update_prop[cache_update_prop_size++] = pa;
                        pa->value_accum = value;
                        pa->accum_pass = accum_pass;
                    } else {
                        Variant::interpolate(pa->value_accum, value, p_interp, pa->value_accum);
                    }

                } else if (p_is_current && p_delta != 0) {

                    Vector<int> indices;
                    a->value_track_get_key_indices(i, p_time, p_delta, &indices);

                    for (int F : indices) {

                        Variant value = a->track_get_key_value(i, F);
                        switch (pa->special) {

                            case SP_NONE: {
                                bool valid;
                                pa->object->set_indexed(pa->subpath, value, &valid); //you are not speshul
#ifdef DEBUG_ENABLED
                                if (!valid) {
                                    ERR_PRINT("Failed setting track value '" + String(pa->owner->path) + "'. Check if property exists or the type of key is valid. Animation '" + a->get_name() + "' at node '" + (String)get_path() + "'.");
                                }
#endif

                            } break;
                            case SP_NODE2D_POS: {
#ifdef DEBUG_ENABLED
                                if (value.get_type() != VariantType::VECTOR2) {
                                    ERR_PRINT("Position key at time " + rtos(p_time) + " in Animation Track '" + String(pa->owner->path) + "' not of type Vector2(). Animation '" + a->get_name() + "' at node '" + (String)get_path() + "'.");
                                }
#endif
                                static_cast<Node2D *>(pa->object)->set_position(value.as<Vector2>());
                            } break;
                            case SP_NODE2D_ROT: {
#ifdef DEBUG_ENABLED
                                if (value.is_num()) {
                                    ERR_PRINT("Rotation key at time " + rtos(p_time) + " in Animation Track '" + String(pa->owner->path) + "' not numerical. Animation '" + a->get_name() + "' at node '" + (String)get_path() + "'.");
                                }
#endif

                                static_cast<Node2D *>(pa->object)->set_rotation(Math::deg2rad(value.as<float>()));
                            } break;
                            case SP_NODE2D_SCALE: {
#ifdef DEBUG_ENABLED
                                if (value.get_type() != VariantType::VECTOR2) {
                                    ERR_PRINT("Scale key at time " + rtos(p_time) + " in Animation Track '" + String(pa->owner->path) + "' not of type Vector2()." + a->get_name() + "' at node '" + (String)get_path() + "'.");
                                }
#endif

                                static_cast<Node2D *>(pa->object)->set_scale(value.as<Vector2>());
                            } break;
                        }
                    }
                }

            } break;
            case Animation::TYPE_METHOD: {

                if (!nc->node || p_delta == 0.0f) {
                    continue;
                }
                if (!p_is_current)
                    break;

                Vector<int> indices;

                a->method_track_get_key_indices(i, p_time, p_delta, &indices);

                for (int E : indices) {

                    StringName method = a->method_track_get_name(i, E);
                    const Vector<Variant> &params = a->method_track_get_params(i, E);

                    int s = params.size();

                    ERR_CONTINUE(s > VARIANT_ARG_MAX);
#ifdef DEBUG_ENABLED
                    if (!nc->node->has_method(method)) {
                        ERR_PRINT("Invalid method call '" + String(method) + "'. '" + a->get_name() + "' at node '" + (String)get_path() + "'.");
                    }
#endif

                    if (can_call) {
                        if (method_call_mode == ANIMATION_METHOD_CALL_DEFERRED) {
                            MessageQueue::get_singleton()->push_call(
                                    nc->node->get_instance_id(),
                                    method,
                                    s >= 1 ? params[0] : Variant(),
                                    s >= 2 ? params[1] : Variant(),
                                    s >= 3 ? params[2] : Variant(),
                                    s >= 4 ? params[3] : Variant(),
                                    s >= 5 ? params[4] : Variant());
                        } else {
                            nc->node->call_va(
                                    method,
                                    s >= 1 ? params[0] : Variant(),
                                    s >= 2 ? params[1] : Variant(),
                                    s >= 3 ? params[2] : Variant(),
                                    s >= 4 ? params[3] : Variant(),
                                    s >= 5 ? params[4] : Variant());
                        }
                    }
                }

            } break;
            case Animation::TYPE_BEZIER: {

                if (!nc->node)
                    continue;

                Map<StringName, TrackNodeCache::BezierAnim>::iterator E = nc->bezier_anim.find(a->track_get_path(i).get_concatenated_subnames());
                ERR_CONTINUE(E==nc->bezier_anim.end()); //should it continue, or create a new one?

                TrackNodeCache::BezierAnim *ba = &E->second;

                float bezier = a->bezier_track_interpolate(i, p_time);
                if (ba->accum_pass != accum_pass) {
                    ERR_CONTINUE(cache_update_bezier_size >= NODE_CACHE_UPDATE_MAX);
                    cache_update_bezier[cache_update_bezier_size++] = ba;
                    ba->bezier_accum = bezier;
                    ba->accum_pass = accum_pass;
                } else {
                    ba->bezier_accum = Math::lerp(ba->bezier_accum, bezier, p_interp);
                }

            } break;
            case Animation::TYPE_AUDIO: {

                if (!nc->node)
                    continue;
                if (p_delta == 0.0f) {
                    continue;
                }

                if (p_seeked) {
                    //find whatever should be playing
                    int idx = a->track_find_key(i, p_time);
                    if (idx < 0)
                        continue;

                    Ref<AudioStream> stream = dynamic_ref_cast<AudioStream>(a->audio_track_get_key_stream(i, idx));
                    if (not stream) {
                        nc->node->call_va("stop");
                        nc->audio_playing = false;
                        playing_caches.erase(nc);
                    } else {
                        float start_ofs = a->audio_track_get_key_start_offset(i, idx);
                        start_ofs += p_time - a->track_get_key_time(i, idx);
                        float end_ofs = a->audio_track_get_key_end_offset(i, idx);
                        float len = stream->get_length();

                        if (start_ofs > len - end_ofs) {
                            nc->node->call_va("stop");
                            nc->audio_playing = false;
                            playing_caches.erase(nc);
                            continue;
                        }

                        nc->node->call_va("set_stream", stream);
                        nc->node->call_va("play", start_ofs);

                        nc->audio_playing = true;
                        playing_caches.insert(nc);
                        if (len && end_ofs > 0) { //force a end at a time
                            nc->audio_len = len - start_ofs - end_ofs;
                        } else {
                            nc->audio_len = 0;
                        }

                        nc->audio_start = p_time;
                    }

                } else {
                    //find stuff to play
                    //TODO: the code below retrieves indices container, but only uses it's last element.
                    Vector<int> to_play;
                    a->track_get_key_indices_in_range(i, p_time, p_delta, &to_play);
                    if (!to_play.empty()) {
                        int idx = to_play.back();

                        Ref<AudioStream> stream = dynamic_ref_cast<AudioStream>(a->audio_track_get_key_stream(i, idx));
                        if (not stream) {
                            nc->node->call_va("stop");
                            nc->audio_playing = false;
                            playing_caches.erase(nc);
                        } else {
                            float start_ofs = a->audio_track_get_key_start_offset(i, idx);
                            float end_ofs = a->audio_track_get_key_end_offset(i, idx);
                            float len = stream->get_length();

                            nc->node->call_va("set_stream", stream);
                            nc->node->call_va("play", start_ofs);

                            nc->audio_playing = true;
                            playing_caches.insert(nc);
                            if (len && end_ofs > 0) { //force a end at a time
                                nc->audio_len = len - start_ofs - end_ofs;
                            } else {
                                nc->audio_len = 0;
                            }

                            nc->audio_start = p_time;
                        }
                    } else if (nc->audio_playing) {

                        bool loop = a->has_loop();

                        bool stop = false;

                        if (!loop && p_time < nc->audio_start) {
                            stop = true;
                        } else if (nc->audio_len > 0) {
                            float len = nc->audio_start > p_time ? (a->get_length() - nc->audio_start) + p_time : p_time - nc->audio_start;

                            if (len > nc->audio_len) {
                                stop = true;
                            }
                        }

                        if (stop) {
                            //time to stop
                            nc->node->call_va("stop");
                            nc->audio_playing = false;
                            playing_caches.erase(nc);
                        }
                    }
                }

            } break;
            case Animation::TYPE_ANIMATION: {

                AnimationPlayer *player = object_cast<AnimationPlayer>(nc->node);
                if (!player)
                    continue;

                if (p_delta == 0.0f || p_seeked) {
                    //seek
                    int idx = a->track_find_key(i, p_time);
                    if (idx < 0)
                        continue;

                    float pos = a->track_get_key_time(i, idx);

                    StringName anim_name = a->animation_track_get_key_animation(i, idx);
                    if (anim_name == "[stop]" || !player->has_animation(anim_name))
                        continue;

                    Ref<Animation> anim = player->get_animation(anim_name);

                    float at_anim_pos;

                    if (anim->has_loop()) {
                        at_anim_pos = Math::fposmod(p_time - pos, anim->get_length()); //seek to loop
                    } else {
                        at_anim_pos = MIN(anim->get_length(), p_time - pos); //seek to end
                    }

                    if (player->is_playing() || p_seeked) {
                        player->play(anim_name);
                        player->seek(at_anim_pos);
                        nc->animation_playing = true;
                        playing_caches.insert(nc);
                    } else {
                        player->set_assigned_animation(anim_name);
                        player->seek(at_anim_pos, true);
                    }
                } else {
                    //find stuff to play
                    Vector<int> to_play;
                    a->track_get_key_indices_in_range(i, p_time, p_delta, &to_play);
                    if (!to_play.empty()) {
                        int idx = to_play.back();

                        StringName anim_name = a->animation_track_get_key_animation(i, idx);
                        if (anim_name == "[stop]" || !player->has_animation(anim_name)) {

                            if (playing_caches.contains(nc)) {
                                playing_caches.erase(nc);
                                player->stop();
                                nc->animation_playing = false;
                            }
                        } else {
                            player->play(anim_name);
                            player->seek(0.0, true);
                            nc->animation_playing = true;
                            playing_caches.insert(nc);
                        }
                    }
                }

            } break;
        }
    }
}

void AnimationPlayer::_animation_process_data(PlaybackData &cd, float p_delta, float p_blend, bool p_seeked, bool p_started) {

    float delta = p_delta * speed_scale * cd.speed_scale;
    float next_pos = cd.pos + delta;

    const float len = cd.from->animation->get_length();
    const bool loop = cd.from->animation->has_loop();

    if (!loop) {

        if (next_pos < 0)
            next_pos = 0;
        else if (next_pos > len)
            next_pos = len;

        const bool backwards = std::signbit(delta); // Negative zero means playing backwards too
        delta = next_pos - cd.pos; // Fix delta (after determination of backwards because negative zero is lost here)

        if (&cd == &playback.current) {

            if (!backwards && cd.pos <= len && next_pos == len /*&& playback.blend.empty()*/) {
                //playback finished
                end_reached = true;
                end_notify = cd.pos < len; // Notify only if not already at the end
            }

            if (backwards && cd.pos >= 0 && next_pos == 0 /*&& playback.blend.empty()*/) {
                //playback finished
                end_reached = true;
                end_notify = cd.pos > 0; // Notify only if not already at the beginning
            }
        }

    } else {

        const float looped_next_pos = Math::fposmod(next_pos, len);
        if (looped_next_pos == 0.0f && next_pos != 0.0f) {
            // Loop multiples of the length to it, rather than 0
            // so state at time=length is previewable in the editor
            next_pos = len;
        } else {
            next_pos = looped_next_pos;
        }
    }

    cd.pos = next_pos;

    _animation_process_animation(cd.from, cd.pos, delta, p_blend, &cd == &playback.current, p_seeked, p_started);
}
void AnimationPlayer::_animation_process2(float p_delta, bool p_started) {

    Playback &c = playback;

    accum_pass++;

    _animation_process_data(c.current, p_delta, 1.0f, c.seeked && p_delta != 0.0f, p_started);
    if (p_delta != 0.0f) {
        c.seeked = false;
    }

    for(auto iter=c.blend.rbegin(),fin=c.blend.rend(); iter!=fin; ++iter) {
        Blend& b = *iter;
        const float blend = b.blend_left / b.blend_time;
        _animation_process_data(b.data, p_delta, blend, false, false);

        b.blend_left -= Math::absf(speed_scale * p_delta);

    }
    // remove finished ones.
    for (auto iter = c.blend.begin(); iter!=c.blend.end(); ) {
        if (iter->blend_left < 0) {

            iter= c.blend.erase(iter);
        }
        else
            ++iter;
    }
}

void AnimationPlayer::_animation_update_transforms() {
    {
        for (int i = 0; i < cache_update_size; i++) {
            Transform t;

            TrackNodeCache *nc = cache_update[i];

            ERR_CONTINUE(nc->accum_pass != accum_pass);

            t.origin = nc->loc_accum;
            t.basis.set_quat_scale(nc->rot_accum, nc->scale_accum);
            if (nc->skeleton && nc->bone_idx >= 0) {

                nc->skeleton->set_bone_pose(nc->bone_idx, t);

            } else if (nc->spatial) {

                nc->spatial->set_transform(t);
            }
        }
    }

    cache_update_size = 0;

    for (int i = 0; i < cache_update_prop_size; i++) {

        TrackNodeCache::PropertyAnim *pa = cache_update_prop[i];

        ERR_CONTINUE(pa->accum_pass != accum_pass);

        switch (pa->special) {

            case SP_NONE: {
                bool valid;
                pa->object->set_indexed(pa->subpath, pa->value_accum, &valid); //you are not speshul
#ifdef DEBUG_ENABLED
                if (!valid) {
                    ERR_PRINT("Failed setting key at time " + rtos(playback.current.pos) + " in Animation '" + get_current_animation() + "' at Node '" + (String)get_path() + "', Track '" + String(pa->owner->path) + "'. Check if property exists or the type of key is right for the property");
                }
#endif

            } break;
            case SP_NODE2D_POS: {
#ifdef DEBUG_ENABLED
                if (pa->value_accum.get_type() != VariantType::VECTOR2) {
                    ERR_PRINT("Position key at time " + rtos(playback.current.pos) + " in Animation '" + get_current_animation() + "' at Node '" + (String)get_path() + "', Track '" + String(pa->owner->path) + "' not of type Vector2()");
                }
#endif
                static_cast<Node2D *>(pa->object)->set_position(pa->value_accum.as<Vector2>());
            } break;
            case SP_NODE2D_ROT: {
#ifdef DEBUG_ENABLED
                if (pa->value_accum.is_num()) {
                    ERR_PRINT("Rotation key at time " + rtos(playback.current.pos) + " in Animation '" + get_current_animation() + "' at Node '" + (String)get_path() + "', Track '" + String(pa->owner->path) + "' not numerical");
                }
#endif

                static_cast<Node2D *>(pa->object)->set_rotation(Math::deg2rad(pa->value_accum.as<float>()));
            } break;
            case SP_NODE2D_SCALE: {
#ifdef DEBUG_ENABLED
                if (pa->value_accum.get_type() != VariantType::VECTOR2) {
                    ERR_PRINT("Scale key at time " + rtos(playback.current.pos) + " in Animation '" + get_current_animation() + "' at Node '" + (String)get_path() + "', Track '" + String(pa->owner->path) + "' not of type Vector2()");
                }
#endif

                static_cast<Node2D *>(pa->object)->set_scale(pa->value_accum.as<Vector2>());
            } break;
        }
    }

    cache_update_prop_size = 0;

    for (int i = 0; i < cache_update_bezier_size; i++) {

        TrackNodeCache::BezierAnim *ba = cache_update_bezier[i];

        ERR_CONTINUE(ba->accum_pass != accum_pass);
        ba->object->set_indexed(ba->bezier_property, ba->bezier_accum);
    }

    cache_update_bezier_size = 0;
}

void AnimationPlayer::_animation_process(float p_delta) {

    if (playback.current.from) {

        end_reached = false;
        end_notify = false;
        _animation_process2(p_delta, playback.started);

        if (playback.started) {
            playback.started = false;
        }

        _animation_update_transforms();
        if (end_reached) {
            if (!queued.empty()) {
                const StringName old = playback.assigned;
                play(queued.front());
                const StringName new_name = playback.assigned;
                queued.pop_front();
                if (end_notify)
                    emit_signal(SceneStringNames::animation_changed, old, new_name);
            } else {
                //stop();
                playing = false;
                _set_process(false);
                if (end_notify)
                    emit_signal(SceneStringNames::animation_finished, playback.assigned);
            }
            end_reached = false;
        }

    } else {
        _set_process(false);
    }
}

Error AnimationPlayer::add_animation(const StringName &p_name, const Ref<Animation> &p_animation) {

#ifdef DEBUG_ENABLED
    ERR_FAIL_COND_V_MSG(StringUtils::contains(p_name, '/') || StringUtils::contains(p_name, ':') ||
                                StringUtils::contains(p_name, ',') || StringUtils::contains(p_name, '['),
            ERR_INVALID_PARAMETER, "Invalid animation name: " + String(p_name) + ".");
#endif

    ERR_FAIL_COND_V(not p_animation, ERR_INVALID_PARAMETER);

    if (animation_set.contains(p_name)) {

        _unref_anim(animation_set[p_name].animation);
        animation_set[p_name].animation = p_animation;
        clear_caches();
    } else {

        AnimationData ad;
        ad.animation = p_animation;
        ad.name = p_name;
        animation_set[p_name] = ad;
    }

    _ref_anim(p_animation);
    Object_change_notify(this);
    return OK;
}

void AnimationPlayer::remove_animation(const StringName &p_name) {

    ERR_FAIL_COND(!animation_set.contains(p_name));

    stop();
    _unref_anim(animation_set[p_name].animation);
    animation_set.erase(p_name);

    clear_caches();
    Object_change_notify(this);
}

void AnimationPlayer::_ref_anim(const Ref<Animation> &p_anim) {
    Ref<Animation>(p_anim)->connect(SceneStringNames::tracks_changed, callable_mp(this, &AnimationPlayer::_animation_changed), ObjectNS::CONNECT_REFERENCE_COUNTED);
}

void AnimationPlayer::_unref_anim(const Ref<Animation> &p_anim) {

    Ref<Animation>(p_anim)->disconnect(SceneStringNames::tracks_changed, callable_mp(this, &AnimationPlayer::_animation_changed));
}

void AnimationPlayer::rename_animation(const StringName &p_name, const StringName &p_new_name) {

    ERR_FAIL_COND(!animation_set.contains(p_name));
    ERR_FAIL_COND(StringUtils::contains(p_new_name,'/') || StringUtils::contains(p_new_name,':'));
    ERR_FAIL_COND(animation_set.contains(p_new_name));

    stop();
    AnimationData ad = animation_set[p_name];
    ad.name = p_new_name;
    animation_set.erase(p_name);
    animation_set[p_new_name] = ad;

    Map<BlendKey, float> to_insert;
    for (auto iter=blend_times.begin(); iter!=blend_times.end(); ) {

        BlendKey bk = iter->first;
        BlendKey new_bk = bk;
        bool erase = false;
        if (bk.from == p_name) {
            new_bk.from = p_new_name;
            erase = true;
        }
        if (bk.to == p_name) {
            new_bk.to = p_new_name;
            erase = true;
        }

        if (erase) {
            to_insert[new_bk] = iter->second;
            iter = blend_times.erase(iter);
        }
        else
            ++iter;
    }

    while (!to_insert.empty()) {
        blend_times[to_insert.begin()->first] = to_insert.begin()->second;
        to_insert.erase(to_insert.begin());
    }

    if (autoplay == p_name)
        autoplay = p_new_name;

    clear_caches();
    Object_change_notify(this);
}

bool AnimationPlayer::has_animation(const StringName &p_name) const {

    return animation_set.contains(p_name);
}
Ref<Animation> AnimationPlayer::get_animation(const StringName &p_name) const {

    ERR_FAIL_COND_V(!animation_set.contains(p_name), Ref<Animation>());

    const AnimationData &data = animation_set.at(p_name);

    return data.animation;
}
Vector<StringName> AnimationPlayer::get_animation_list() const {

    Vector<StringName> anims;
    anims.reserve(animation_set.size());
    for (const eastl::pair<const StringName,AnimationData> &E : animation_set) {
        anims.emplace_back(E.first);
    }
    //TODO: SEGS: this sort will not sort by alpha comparing names !
    eastl::sort(anims.begin(),anims.end());
    return anims;
}

void AnimationPlayer::set_blend_time(const StringName &p_animation1, const StringName &p_animation2, float p_time) {
    ERR_FAIL_COND(!animation_set.contains(p_animation1));
    ERR_FAIL_COND(!animation_set.contains(p_animation2));

    ERR_FAIL_COND_MSG(p_time < 0, "Blend time cannot be smaller than 0.");

    BlendKey bk;
    bk.from = p_animation1;
    bk.to = p_animation2;
    if (p_time == 0)
        blend_times.erase(bk);
    else
        blend_times[bk] = p_time;
}

float AnimationPlayer::get_blend_time(const StringName &p_animation1, const StringName &p_animation2) const {

    BlendKey bk;
    bk.from = p_animation1;
    bk.to = p_animation2;

    return blend_times.at(bk,0);
}

void AnimationPlayer::queue(const StringName &p_name) {

    if (!is_playing())
        play(p_name);
    else
        queued.push_back(p_name);
}

PoolVector<String> AnimationPlayer::get_queue() {
    PoolVector<String> ret;
    for (const StringName &E : queued) {
        ret.push_back(E.asCString());
    }

    return ret;
}

void AnimationPlayer::clear_queue() {
    queued.clear();
}

void AnimationPlayer::play_backwards(const StringName &p_name, float p_custom_blend) {

    play(p_name, p_custom_blend, -1, true);
}

void AnimationPlayer::play(const StringName &p_name, float p_custom_blend, float p_custom_scale, bool p_from_end) {

    StringName name = p_name;

    if (name.empty())
        name = playback.assigned;

    ERR_FAIL_COND_MSG(!animation_set.contains(name), "Animation not found: " + String(name) + ".");

    Playback &c = playback;

    if (c.current.from) {

        float blend_time = 0;
        // find if it can blend
        BlendKey bk;
        bk.from = StringName(c.current.from->name);
        bk.to = name;

        if (p_custom_blend >= 0) {
            blend_time = p_custom_blend;
        } else if (blend_times.contains(bk)) {

            blend_time = blend_times[bk];
        } else {

            bk.from = "*";
            if (blend_times.contains(bk)) {

                blend_time = blend_times[bk];
            } else {

                bk.from = StringName(c.current.from->name);
                bk.to = "*";

                if (blend_times.contains(bk)) {

                    blend_time = blend_times[bk];
                }
            }
        }

        if (p_custom_blend < 0 && blend_time == 0 && default_blend_time)
            blend_time = default_blend_time;
        if (blend_time > 0) {

            Blend b;
            b.data = c.current;
            b.blend_time = b.blend_left = blend_time;
            c.blend.push_back(b);
        }
    }

    if (get_current_animation() != p_name) {
        _stop_playing_caches();
    }

    c.current.from = &animation_set[name];

    if (c.assigned != name) { // reset
        c.current.pos = p_from_end ? c.current.from->animation->get_length() : 0;
    } else {
        if (p_from_end && c.current.pos == 0) {
            // Animation reset BUT played backwards, set position to the end
            c.current.pos = c.current.from->animation->get_length();
        } else if (!p_from_end && c.current.pos == c.current.from->animation->get_length()) {
            // Animation resumed but already ended, set position to the beginning
            c.current.pos = 0;
        }
    }

    c.current.speed_scale = p_custom_scale;
    c.assigned = name;
    c.seeked = false;
    c.started = true;

    if (!end_reached)
        queued.clear();
    _set_process(true); // always process when starting an animation
    playing = true;

    emit_signal(SceneStringNames::animation_started, c.assigned);

    if (is_inside_tree() && Engine::get_singleton()->is_editor_hint())
        return; // no next in this case

    StringName next = animation_get_next(p_name);
    if (next != StringName() && animation_set.contains(next)) {
        queue(next);
    }
}

bool AnimationPlayer::is_playing() const {

    return playing;
    /*
    if (playback.current.from==NULL)
        return false;

    float len=playback.current.from->animation->get_length();
    float pos = playback.current.pos;
    bool loop=playback.current.from->animation->has_loop();
    if (!loop && pos >= len) {
        return false;
    };

    return true;
    */
}

void AnimationPlayer::set_current_animation(const StringName &p_anim) {

    if (p_anim == "[stop]" || p_anim.empty()) {
        stop();
    } else if (!is_playing() || playback.assigned != p_anim) {
        play(p_anim);
    } else {
        // Same animation, do not replay from start
    }
}

StringName AnimationPlayer::get_current_animation() const {

    return is_playing() ? playback.assigned : StringName();
}

void AnimationPlayer::set_assigned_animation(const StringName &p_anim) {

    if (is_playing()) {
        play(p_anim);
    } else {
        ERR_FAIL_COND(!animation_set.contains(p_anim));
        playback.current.pos = 0;
        playback.current.from = &animation_set[p_anim];
        playback.assigned = p_anim;
    }
}

StringName AnimationPlayer::get_assigned_animation() const {

    return playback.assigned;
}

void AnimationPlayer::stop(bool p_reset) {

    _stop_playing_caches();
    Playback &c = playback;
    c.blend.clear();
    if (p_reset) {
        c.current.from = nullptr;
        c.current.speed_scale = 1;
        c.current.pos = 0;
    }
    _set_process(false);
    queued.clear();
    playing = false;
}

void AnimationPlayer::set_speed_scale(float p_speed) {

    speed_scale = p_speed;
}
float AnimationPlayer::get_speed_scale() const {

    return speed_scale;
}
float AnimationPlayer::get_playing_speed() const {

    if (!playing) {
        return 0;
    }
    return speed_scale * playback.current.speed_scale;
}

void AnimationPlayer::seek(float p_time, bool p_update) {

    if (!playback.current.from) {
        if (playback.assigned) {
            ERR_FAIL_COND(!animation_set.contains(playback.assigned));
            playback.current.from = &animation_set[playback.assigned];
        }
        ERR_FAIL_COND(!playback.current.from);
    }

    playback.current.pos = p_time;
    playback.seeked = true;
    if (p_update) {
        _animation_process(0);
    }
}

void AnimationPlayer::seek_delta(float p_time, float p_delta) {

    if (!playback.current.from) {
        if (playback.assigned) {
            ERR_FAIL_COND(!animation_set.contains(playback.assigned));
            playback.current.from = &animation_set[playback.assigned];
        }
        ERR_FAIL_COND(!playback.current.from);
    }

    playback.current.pos = p_time - p_delta;
    if (speed_scale != 0.0f)
        p_delta /= speed_scale;
    _animation_process(p_delta);
    //playback.current.pos=p_time;
}

bool AnimationPlayer::is_valid() const {

    return (playback.current.from);
}

float AnimationPlayer::get_current_animation_position() const {

    ERR_FAIL_COND_V_MSG(!playback.current.from, 0, "AnimationPlayer has no current animation");
    return playback.current.pos;
}

float AnimationPlayer::get_current_animation_length() const {

    ERR_FAIL_COND_V_MSG(!playback.current.from, 0, "AnimationPlayer has no current animation");
    return playback.current.from->animation->get_length();
}

void AnimationPlayer::_animation_changed() {

    clear_caches();
    emit_signal("caches_cleared");
    if (is_playing()) {
        playback.seeked = true; //need to restart stuff, like audio
    }
}

void AnimationPlayer::_stop_playing_caches() {

    for (TrackNodeCache * E : playing_caches) {

        if (E->node && E->audio_playing) {
            E->node->call_va("stop");
        }
        if (E->node && E->animation_playing) {
            AnimationPlayer *player = object_cast<AnimationPlayer>(E->node);
            if (!player)
                continue;
            player->stop();
        }
    }

    playing_caches.clear();
}

void AnimationPlayer::_node_removed(Node *p_node) {

    clear_caches(); // nodes contained here ar being removed, clear the caches
}

void AnimationPlayer::clear_caches() {

    _stop_playing_caches();

    node_cache_map.clear();

    for (eastl::pair<const StringName,AnimationData> &E : animation_set) {

        E.second.node_cache.clear();
    }

    cache_update_size = 0;
    cache_update_prop_size = 0;
    cache_update_bezier_size = 0;
}

void AnimationPlayer::set_active(bool p_active) {

    if (active == p_active)
        return;

    active = p_active;
    _set_process(processing, true);
}

bool AnimationPlayer::is_active() const {

    return active;
}

StringName AnimationPlayer::find_animation(const Ref<Animation> &p_animation) const {

    for (const eastl::pair<const StringName,AnimationData> &E : animation_set) {

        if (E.second.animation == p_animation)
            return E.first;
    }

    return "";
}

void AnimationPlayer::set_autoplay(const StringName & p_name) {
    if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
        WARN_PRINT("Setting autoplay after the node has been added to the scene has no effect.");
    }

    autoplay = p_name;
}

StringName AnimationPlayer::get_autoplay() const {

    return autoplay;
}

void AnimationPlayer::set_reset_on_save_enabled(bool p_enabled) {
    reset_on_save = p_enabled;
}

bool AnimationPlayer::is_reset_on_save_enabled() const {
    return reset_on_save;
}

void AnimationPlayer::set_animation_process_mode(AnimationProcessMode p_mode) {

    if (animation_process_mode == p_mode)
        return;

    const bool pr = processing;
    if (pr)
        _set_process(false);
    animation_process_mode = p_mode;
    if (pr)
        _set_process(true);
}

AnimationPlayer::AnimationProcessMode AnimationPlayer::get_animation_process_mode() const {

    return animation_process_mode;
}

void AnimationPlayer::set_method_call_mode(AnimationMethodCallMode p_mode) {

    method_call_mode = p_mode;
}

AnimationPlayer::AnimationMethodCallMode AnimationPlayer::get_method_call_mode() const {

    return method_call_mode;
}

void AnimationPlayer::_set_process(bool p_process, bool p_force) {

    if (processing == p_process && !p_force)
        return;

    switch (animation_process_mode) {

        case ANIMATION_PROCESS_PHYSICS:
            set_physics_process_internal(p_process && active);
            break;
        case ANIMATION_PROCESS_IDLE:
            set_process_internal(p_process && active);
            break;
        case ANIMATION_PROCESS_MANUAL:
            break;
    }

    processing = p_process;
}

void AnimationPlayer::animation_set_next(const StringName &p_animation, const StringName &p_next) {

    ERR_FAIL_COND(!animation_set.contains(p_animation));
    animation_set[p_animation].next = p_next;
}

StringName AnimationPlayer::animation_get_next(const StringName &p_animation) const {

    if (!animation_set.contains(p_animation))
        return {};
    return animation_set.at(p_animation).next;
}

void AnimationPlayer::set_default_blend_time(float p_default) {

    default_blend_time = p_default;
}

float AnimationPlayer::get_default_blend_time() const {

    return default_blend_time;
}

void AnimationPlayer::set_root(const NodePath &p_root) {

    root = p_root;
    clear_caches();
}

NodePath AnimationPlayer::get_root() const {

    return root;
}

#ifdef TOOLS_ENABLED
eastl::shared_ptr<AnimatedValuesBackup> AnimationPlayer::backup_animated_values(Node *p_root_override) {
    eastl::shared_ptr<AnimatedValuesBackup> backup;

    if (!playback.current.from)
        return backup;

    _ensure_node_caches(playback.current.from, p_root_override);

    backup = eastl::make_shared<AnimatedValuesBackup>();

    for (int i = 0; i < playback.current.from->node_cache.size(); i++) {
        TrackNodeCache *nc = playback.current.from->node_cache[i];
        if (!nc)
            continue;

        if (nc->skeleton) {
            if (nc->bone_idx == -1)
                continue;

            AnimatedValuesBackup::Entry entry;
            entry.object = nc->skeleton;
            entry.bone_idx = nc->bone_idx;
            entry.value = nc->skeleton->get_bone_pose(nc->bone_idx);
            backup->entries.emplace_back(eastl::move(entry));
            continue;
        }
        if (nc->spatial) {
            AnimatedValuesBackup::Entry entry;
            entry.object = nc->spatial;
            entry.subpath.push_back("transform");
            entry.value = nc->spatial->get_transform();
            entry.bone_idx = -1;
            backup->entries.emplace_back(eastl::move(entry));
        } else {
            for (eastl::pair<const StringName,TrackNodeCache::PropertyAnim> &E : nc->property_anim) {
                AnimatedValuesBackup::Entry entry;
                bool valid;
                entry.value = E.second.object->get_indexed(E.second.subpath, &valid);
                if (valid) {
                    entry.object = E.second.object;
                    entry.subpath = E.second.subpath;
                    entry.bone_idx = -1;
                    backup->entries.emplace_back(eastl::move(entry));
                }
            }
        }
    }

    return backup;
}
struct AnimationResetApply : UndoableAction {

public:
    eastl::shared_ptr<AnimatedValuesBackup> old_values;
    eastl::shared_ptr<AnimatedValuesBackup> new_values;
    StringName name() const override
    {
        return TTR("Anim Apply Reset");
    }
    void redo() override
    {
        new_values->restore();
    }
    void undo() override
    {
        old_values->restore();
    }
    bool can_apply() override
    {
        return new_values && old_values;
    }
};

eastl::shared_ptr<AnimatedValuesBackup> AnimationPlayer::apply_reset(bool p_user_initiated) {
    ERR_FAIL_COND_V(!can_apply_reset(), {});

    const Ref<Animation> reset_anim = animation_set["RESET"].animation;

    Node *root_node = get_node_or_null(root);
    ERR_FAIL_COND_V(!root_node, {});

    AnimationPlayer *aux_player = memnew(AnimationPlayer);
    EditorNode::get_singleton()->add_child(aux_player);
    aux_player->add_animation("RESET", reset_anim);
    aux_player->set_assigned_animation("RESET");
    // Forcing the use of the original root because the scene where original player belongs may be not the active one
    Node *root = get_node(get_root());
    auto old_values = aux_player->backup_animated_values(root);
    aux_player->seek(0.0f, true);
    aux_player->queue_delete();

    if (p_user_initiated) {
        auto new_values = aux_player->backup_animated_values();
        old_values->restore();
        auto *reset_apply_action = new AnimationResetApply;
        reset_apply_action->old_values = eastl::move(old_values);
        reset_apply_action->new_values = eastl::move(new_values);
        UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
        ur->add_action(reset_apply_action);
        ur->commit_action();
        }
    return old_values;
    }
bool AnimationPlayer::can_apply_reset() const {
    return has_animation("RESET") && playback.assigned != StringName("RESET");
}
#endif

void AnimationPlayer::_bind_methods() {

    SE_BIND_METHOD(AnimationPlayer,_animation_changed);

    SE_BIND_METHOD(AnimationPlayer,add_animation);
    SE_BIND_METHOD(AnimationPlayer,remove_animation);
    SE_BIND_METHOD(AnimationPlayer,rename_animation);
    SE_BIND_METHOD(AnimationPlayer,has_animation);
    SE_BIND_METHOD(AnimationPlayer,get_animation);
    SE_BIND_METHOD(AnimationPlayer,get_animation_list);

    SE_BIND_METHOD(AnimationPlayer,animation_set_next);
    SE_BIND_METHOD(AnimationPlayer,animation_get_next);

    SE_BIND_METHOD(AnimationPlayer,set_blend_time);
    SE_BIND_METHOD(AnimationPlayer,get_blend_time);

    SE_BIND_METHOD(AnimationPlayer,set_default_blend_time);
    SE_BIND_METHOD(AnimationPlayer,get_default_blend_time);

    MethodBinder::bind_method(D_METHOD("play", {"name", "custom_blend", "custom_speed", "from_end"}), &AnimationPlayer::play, {DEFVAL(StringName()), DEFVAL(-1), DEFVAL(1.0), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("play_backwards", {"name", "custom_blend"}), &AnimationPlayer::play_backwards, {DEFVAL(StringName()), DEFVAL(-1)});
    MethodBinder::bind_method(D_METHOD("stop", {"reset"}), &AnimationPlayer::stop, {DEFVAL(true)});
    SE_BIND_METHOD(AnimationPlayer,is_playing);

    SE_BIND_METHOD(AnimationPlayer,set_current_animation);
    SE_BIND_METHOD(AnimationPlayer,get_current_animation);
    SE_BIND_METHOD(AnimationPlayer,set_assigned_animation);
    SE_BIND_METHOD(AnimationPlayer,get_assigned_animation);
    SE_BIND_METHOD(AnimationPlayer,queue);
    SE_BIND_METHOD(AnimationPlayer,get_queue);
    SE_BIND_METHOD(AnimationPlayer,clear_queue);

    SE_BIND_METHOD(AnimationPlayer,set_active);
    SE_BIND_METHOD(AnimationPlayer,is_active);

    SE_BIND_METHOD(AnimationPlayer,set_speed_scale);
    SE_BIND_METHOD(AnimationPlayer,get_speed_scale);
    SE_BIND_METHOD(AnimationPlayer,get_playing_speed);

    SE_BIND_METHOD(AnimationPlayer,set_autoplay);
    SE_BIND_METHOD(AnimationPlayer,get_autoplay);

    SE_BIND_METHOD(AnimationPlayer,set_reset_on_save_enabled);
    SE_BIND_METHOD(AnimationPlayer,is_reset_on_save_enabled);
    SE_BIND_METHOD(AnimationPlayer,set_root);
    SE_BIND_METHOD(AnimationPlayer,get_root);

    SE_BIND_METHOD(AnimationPlayer,find_animation);

    SE_BIND_METHOD(AnimationPlayer,clear_caches);

    SE_BIND_METHOD(AnimationPlayer,set_animation_process_mode);
    SE_BIND_METHOD(AnimationPlayer,get_animation_process_mode);

    SE_BIND_METHOD(AnimationPlayer,set_method_call_mode);
    SE_BIND_METHOD(AnimationPlayer,get_method_call_mode);

    SE_BIND_METHOD(AnimationPlayer,get_current_animation_position);
    SE_BIND_METHOD(AnimationPlayer,get_current_animation_length);

    MethodBinder::bind_method(D_METHOD("seek", {"seconds", "update"}), &AnimationPlayer::seek, {DEFVAL(false)});
    SE_BIND_METHOD(AnimationPlayer,advance);

    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "root_node"), "set_root", "get_root");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "current_animation", PropertyHint::Enum, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_ANIMATE_AS_TRIGGER), "set_current_animation", "get_current_animation");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "assigned_animation", PropertyHint::None, "", 0), "set_assigned_animation", "get_assigned_animation");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "autoplay", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_autoplay", "get_autoplay");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "current_animation_length", PropertyHint::None, "", 0), "", "get_current_animation_length");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "current_animation_position", PropertyHint::None, "", 0), "", "get_current_animation_position");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "reset_on_save", PropertyHint::None), "set_reset_on_save_enabled", "is_reset_on_save_enabled");

    ADD_GROUP("Playback Options", "playback_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "playback_process_mode", PropertyHint::Enum, "Physics,Idle,Manual"), "set_animation_process_mode", "get_animation_process_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "playback_default_blend_time", PropertyHint::Range, "0,4096,0.01"), "set_default_blend_time", "get_default_blend_time");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "playback_active", PropertyHint::None, "", 0), "set_active", "is_active");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "playback_speed", PropertyHint::Range, "-64,64,0.01"), "set_speed_scale", "get_speed_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "method_call_mode", PropertyHint::Enum, "Deferred,Immediate"), "set_method_call_mode", "get_method_call_mode");

    ADD_SIGNAL(MethodInfo("animation_finished", PropertyInfo(VariantType::STRING, "anim_name")));
    ADD_SIGNAL(MethodInfo("animation_changed", PropertyInfo(VariantType::STRING, "old_name"), PropertyInfo(VariantType::STRING, "new_name")));
    ADD_SIGNAL(MethodInfo("animation_started", PropertyInfo(VariantType::STRING, "anim_name")));
    ADD_SIGNAL(MethodInfo("caches_cleared"));

    BIND_ENUM_CONSTANT(ANIMATION_PROCESS_PHYSICS);
    BIND_ENUM_CONSTANT(ANIMATION_PROCESS_IDLE);
    BIND_ENUM_CONSTANT(ANIMATION_PROCESS_MANUAL);

    BIND_ENUM_CONSTANT(ANIMATION_METHOD_CALL_DEFERRED);
    BIND_ENUM_CONSTANT(ANIMATION_METHOD_CALL_IMMEDIATE);
}

AnimationPlayer::AnimationPlayer() :
    root(SceneStringNames::path_pp) {

    playback.seeked = false;
    playback.started = false;
}

AnimationPlayer::~AnimationPlayer() {
}
