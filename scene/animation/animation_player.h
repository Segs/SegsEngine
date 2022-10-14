/*************************************************************************/
/*  animation_player.h                                                   */
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

#include "scene/main/node.h"
#include "scene/resources/animation.h"
#include "core/map.h"
#include "core/hash_set.h"
#include "core/deque.h"
#include "core/string.h"
#include <EASTL/shared_ptr.h>

#ifdef TOOLS_ENABLED
// To save/restore animated values
class AnimatedValuesBackup {
    struct Entry {
        Object *object;
        Vector<StringName> subpath; // Unused if bone
        int bone_idx; // -1 if not a bone
        Variant value;
    };
    Vector<Entry> entries;

    friend class AnimationPlayer;

public:
    void update_skeletons();
    void restore() const;
};
#endif

class Node2D;
class Node3D;
class Skeleton;

class GODOT_EXPORT AnimationPlayer : public Node {
    GDCLASS(AnimationPlayer,Node)

    OBJ_CATEGORY("Animation Nodes")

public:
    enum AnimationProcessMode {
        ANIMATION_PROCESS_PHYSICS,
        ANIMATION_PROCESS_IDLE,
        ANIMATION_PROCESS_MANUAL,
    };

    enum AnimationMethodCallMode {
        ANIMATION_METHOD_CALL_DEFERRED,
        ANIMATION_METHOD_CALL_IMMEDIATE,
    };

private:
    enum {

        NODE_CACHE_UPDATE_MAX = 1024,
        BLEND_FROM_MAX = 3
    };

    enum SpecialProperty {
        SP_NONE,
        SP_NODE2D_POS,
        SP_NODE2D_ROT,
        SP_NODE2D_SCALE,
    };

    struct TrackNodeCache {

        NodePath path;
        uint32_t id;
        RES resource;
        Node *node;
        Node3D *spatial;
        Node2D *node_2d;
        Skeleton *skeleton;
        int bone_idx;
        // accumulated transforms

        Vector3 loc_accum;
        Quat rot_accum;
        Vector3 scale_accum;
        uint64_t accum_pass;

        bool audio_playing;
        float audio_start;
        float audio_len;

        bool animation_playing;

        struct PropertyAnim {

            TrackNodeCache *owner = nullptr;
            SpecialProperty special = SP_NONE; // small optimization
            Vector<StringName> subpath;
            Object *object = nullptr;
            Variant value_accum;
            uint64_t accum_pass=0;
            Variant capture;
        };

        HashMap<StringName, PropertyAnim> property_anim;

        struct BezierAnim {

            Vector<StringName> bezier_property;
            TrackNodeCache *owner = nullptr;
            float bezier_accum = 0.0f;
            Object *object = nullptr;
            uint64_t accum_pass = 0;
        };

        Map<StringName, BezierAnim> bezier_anim;

        TrackNodeCache() :
                id(0),
                node(nullptr),
                spatial(nullptr),
                node_2d(nullptr),
                skeleton(nullptr),
                bone_idx(-1),
                accum_pass(0),
                audio_playing(false),
                audio_start(0.0),
                audio_len(0.0),
                animation_playing(false) {}
    };

    struct TrackNodeCacheKey {

        GameEntity id;
        int bone_idx;

        bool operator<(const TrackNodeCacheKey &p_right) const {
            if(id==p_right.id)
                return bone_idx<p_right.bone_idx;

            return entt::to_integral(id) < entt::to_integral(p_right.id);
        }
    };

    Map<TrackNodeCacheKey, TrackNodeCache> node_cache_map;

    TrackNodeCache *cache_update[NODE_CACHE_UPDATE_MAX];
    int cache_update_size = 0;
    TrackNodeCache::PropertyAnim *cache_update_prop[NODE_CACHE_UPDATE_MAX];
    int cache_update_prop_size = 0;
    TrackNodeCache::BezierAnim *cache_update_bezier[NODE_CACHE_UPDATE_MAX];
    int cache_update_bezier_size = 0;
    HashSet<TrackNodeCache *> playing_caches;

    uint64_t accum_pass = 1;
    float speed_scale = 1;
    float default_blend_time = 0;

    struct AnimationData {
        String name;
        StringName next;
        Vector<TrackNodeCache *> node_cache;
        Ref<Animation> animation;
    };

    Map<StringName, AnimationData> animation_set;
    struct BlendKey {

        StringName from;
        StringName to;
        bool operator<(const BlendKey &bk) const {
            return from == bk.from ? StringView(to) < StringView(bk.to) :
                                     StringView(from) < StringView(bk.from);
        }
    };

    Map<BlendKey, float> blend_times;

    struct PlaybackData {

        AnimationData *from=nullptr;
        float pos=0.0f;
        float speed_scale=1.0f;
    };

    struct Blend {

        PlaybackData data;

        float blend_time = 0.0f;
        float blend_left = 0.0f;
    };

    struct Playback {

        Vector<Blend> blend;
        PlaybackData current;
        StringName assigned;
        bool seeked;
        bool started;
    } playback;

    Dequeue<StringName> queued;

    bool end_reached = false;
    bool end_notify = false;

    StringName autoplay;
    bool reset_on_save = true;
    AnimationProcessMode animation_process_mode = ANIMATION_PROCESS_IDLE;
    AnimationMethodCallMode method_call_mode = ANIMATION_METHOD_CALL_DEFERRED;
    bool processing = false;
    bool active = true;

    NodePath root;

    void _animation_process_animation(AnimationData *p_anim, float p_time, float p_delta, float p_interp, bool p_is_current = true, bool p_seeked = false, bool p_started = false);

    void _ensure_node_caches(AnimationData *p_anim, Node *p_root_override = nullptr);
    void _animation_process_data(PlaybackData &cd, float p_delta, float p_blend, bool p_seeked, bool p_started);
    void _animation_process2(float p_delta, bool p_started);
    void _animation_update_transforms();
    void _animation_process(float p_delta);

    void _node_removed(Node *p_node);
    void _stop_playing_caches();

    // bind helpers
    void _animation_changed();
    void _ref_anim(const Ref<Animation> &p_anim);
    void _unref_anim(const Ref<Animation> &p_anim);

    void _set_process(bool p_process, bool p_force = false);

    bool playing = false;

protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _validate_property(PropertyInfo &property) const override;
    void _get_property_list(Vector<PropertyInfo> *p_list) const;
    void _notification(int p_what);

    static void _bind_methods();

public:
    StringName find_animation(const Ref<Animation> &p_animation) const;

    Error add_animation(const StringName &p_name, const Ref<Animation> &p_animation);
    void remove_animation(const StringName &p_name);
    void rename_animation(const StringName &p_name, const StringName &p_new_name);
    bool has_animation(const StringName &p_name) const;
    Ref<Animation> get_animation(const StringName &p_name) const;
    Vector<StringName> get_animation_list() const;

    void set_blend_time(const StringName &p_animation1, const StringName &p_animation2, float p_time);
    float get_blend_time(const StringName &p_animation1, const StringName &p_animation2) const;

    void animation_set_next(const StringName &p_animation, const StringName &p_next);
    StringName animation_get_next(const StringName &p_animation) const;

    void set_default_blend_time(float p_default);
    float get_default_blend_time() const;

    void play(const StringName &p_name = StringName(), float p_custom_blend = -1, float p_custom_scale = 1.0, bool p_from_end = false);
    void play_backwards(const StringName &p_name = StringName(), float p_custom_blend = -1);
    void queue(const StringName &p_name);
    PoolVector<String> get_queue();
    void clear_queue();
    void stop(bool p_reset = true);
    bool is_playing() const;
    StringName get_current_animation() const;
    void set_current_animation(const StringName &p_anim);
    StringName get_assigned_animation() const;
    void set_assigned_animation(const StringName &p_anim);
    void set_active(bool p_active);
    bool is_active() const;
    bool is_valid() const;

    void set_speed_scale(float p_speed);
    float get_speed_scale() const;
    float get_playing_speed() const;

    void set_autoplay(const StringName &p_name);
    StringName get_autoplay() const;
    void set_reset_on_save_enabled(bool p_enabled);
    bool is_reset_on_save_enabled() const;

    void set_animation_process_mode(AnimationProcessMode p_mode);
    AnimationProcessMode get_animation_process_mode() const;

    void set_method_call_mode(AnimationMethodCallMode p_mode);
    AnimationMethodCallMode get_method_call_mode() const;

    void seek(float p_time, bool p_update = false);
    void seek_delta(float p_time, float p_delta);
    float get_current_animation_position() const;
    float get_current_animation_length() const;

    void advance(float p_time);

    void set_root(const NodePath &p_root);
    NodePath get_root() const;

    void clear_caches(); ///< must be called by hand if an animation was modified after added

#ifdef TOOLS_ENABLED
    // These may be interesting for games, but are too dangerous for general use
    eastl::shared_ptr<AnimatedValuesBackup> backup_animated_values(Node *p_root_override = nullptr);
    eastl::shared_ptr<AnimatedValuesBackup> apply_reset(bool p_user_initiated = false);
    bool can_apply_reset() const;
#endif

    AnimationPlayer();
    ~AnimationPlayer() override;
};
