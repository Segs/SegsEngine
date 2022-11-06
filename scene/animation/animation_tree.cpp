/*************************************************************************/
/*  animation_tree.cpp                                                   */
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

#include "animation_tree.h"

#include "animation_blend_tree.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/object_tooling.h"
#include "core/engine.h"
#include "core/string_formatter.h"
#include "core/script_language.h"
#include "core/translation_helpers.h"

#include "scene/scene_string_names.h"
#include "servers/audio/audio_stream.h"

IMPL_GDCLASS(AnimationNode)
IMPL_GDCLASS(AnimationRootNode)
IMPL_GDCLASS(AnimationTree)
VARIANT_ENUM_CAST(AnimationNode::FilterAction);
VARIANT_ENUM_CAST(AnimationTree::AnimationProcessMode);

void AnimationNode::get_parameter_list(Vector<PropertyInfo> *r_list) const {
    if (get_script_instance()) {
        Array parameters = get_script_instance()->call("get_parameter_list").as<Array>();
        for (int i = 0; i < parameters.size(); i++) {
            Dictionary d = parameters[i].as<Dictionary>();
            ERR_CONTINUE(d.empty());
            r_list->push_back(PropertyInfo::from_dict(d));
        }
    }
}

Variant AnimationNode::get_parameter_default_value(const StringName &p_parameter) const {
    if (get_script_instance()) {
        return get_script_instance()->call("get_parameter_default_value", p_parameter);
    }
    return Variant();
}

void AnimationNode::set_parameter(const StringName &p_name, const Variant &p_value) {
    ERR_FAIL_COND(!state);
    ERR_FAIL_COND(!state->tree->property_parent_map.contains(base_path));
    ERR_FAIL_COND(!state->tree->property_parent_map[base_path].contains(p_name));
    StringName path = state->tree->property_parent_map[base_path][p_name];

    state->tree->property_map[path] = p_value;
}

Variant AnimationNode::get_parameter(const StringName &p_name) const {
    ERR_FAIL_COND_V(!state, Variant());
    ERR_FAIL_COND_V(!state->tree->property_parent_map.contains(base_path), Variant());
    ERR_FAIL_COND_V(!state->tree->property_parent_map[base_path].contains(p_name), Variant());

    StringName path = state->tree->property_parent_map[base_path][p_name];
    return state->tree->property_map[path];
}

void AnimationNode::get_child_nodes(Vector<AnimationNode::ChildNode> *r_child_nodes) {

    if (get_script_instance()) {
        Dictionary cn = get_script_instance()->call("get_child_nodes").as<Dictionary>();
        auto keys(cn.get_key_list());
        for (const auto &E :keys) {
            ChildNode child;
            child.name = E;
            child.node = refFromVariant<AnimationNode>(cn[E]);
            r_child_nodes->push_back(child);
        }
    }
}

void AnimationNode::blend_animation(const StringName &p_animation, float p_time, float p_delta, bool p_seeked, float p_blend) {

    ERR_FAIL_COND(!state);
    ERR_FAIL_COND(!state->player->has_animation(p_animation));

    Ref<Animation> animation = state->player->get_animation(p_animation);

    if (not animation) {

        AnimationNodeBlendTree *btree = object_cast<AnimationNodeBlendTree>(parent);
        if (btree) {
            StringName name = btree->get_node_name(Ref<AnimationNode>(this));
            make_invalid(FormatVE(RTR_utf8("In node '%s', invalid animation: '%s'.").c_str(), name.asCString(), p_animation.asCString()));
        } else {
            make_invalid(FormatVE(RTR_utf8("Invalid animation: '%s'.").c_str(), p_animation.asCString()));
        }
        return;
    }

    ERR_FAIL_COND(not animation);

    AnimationState anim_state;
    anim_state.blend = p_blend;
    anim_state.track_blends = &blends;
    anim_state.delta = p_delta;
    anim_state.time = p_time;
    anim_state.animation = animation;
    anim_state.seeked = p_seeked;

    state->animation_states.emplace_back(eastl::move(anim_state));
}

float AnimationNode::_pre_process(const StringName &p_base_path, AnimationNode *p_parent, State *p_state, float p_time, bool p_seek, const Vector<
        StringName> &p_connections) {

    base_path = p_base_path;
    parent = p_parent;
    connections = p_connections;
    state = p_state;

    float t = process(p_time, p_seek);

    state = nullptr;
    parent = nullptr;
    base_path = StringName();
    connections.clear();

    return t;
}

void AnimationNode::make_invalid(const String &p_reason) {
    ERR_FAIL_COND(!state);
    state->valid = false;
    if (!state->invalid_reasons.empty()) {
        state->invalid_reasons += '\n';
    }
    state->invalid_reasons += "- " + p_reason;
}

float AnimationNode::blend_input(int p_input, float p_time, bool p_seek, float p_blend, FilterAction p_filter, bool p_optimize) {
    ERR_FAIL_INDEX_V(p_input, inputs.size(), 0);
    ERR_FAIL_COND_V(!state, 0);

    AnimationNodeBlendTree *blend_tree = object_cast<AnimationNodeBlendTree>(parent);
    ERR_FAIL_COND_V(!blend_tree, 0);

    StringName node_name = connections[p_input];

    if (!blend_tree->has_node(node_name)) {
        StringName name = blend_tree->get_node_name(Ref<AnimationNode>(this));
        make_invalid(FormatVE(RTR_utf8("Nothing connected to input '%s' of node '%s'.").c_str(), get_input_name(p_input).c_str(), name.asCString()));
        return 0;
    }

    Ref<AnimationNode> node = blend_tree->get_node(node_name);

    //inputs.write[p_input].last_pass = state->last_pass;
    float activity = 0;
    float ret = _blend_node(node_name, blend_tree->get_node_connection_array(node_name), nullptr, node, p_time, p_seek, p_blend, p_filter, p_optimize, &activity);
    auto ac_iter = state->tree->input_activity_map.find(base_path);

    if(state->tree->input_activity_map.end()==ac_iter)
        return ret;

    Vector<AnimationTree::Activity> &activity_ptr = ac_iter->second;

    if (p_input < activity_ptr.size()) {
        activity_ptr[p_input].last_pass = state->last_pass;
        activity_ptr[p_input].activity = activity;
    }
    return ret;
}

float AnimationNode::blend_node(const StringName &p_sub_path, const Ref<AnimationNode>& p_node, float p_time, bool p_seek, float p_blend, FilterAction p_filter, bool p_optimize) {

    return _blend_node(p_sub_path, {}, this, p_node, p_time, p_seek, p_blend, p_filter, p_optimize);
}

float AnimationNode::_blend_node(const StringName &p_subpath, const Vector<StringName> &p_connections, AnimationNode *p_new_parent, Ref<AnimationNode> p_node, float p_time, bool p_seek, float p_blend, FilterAction p_filter, bool p_optimize, float *r_max) {

    ERR_FAIL_COND_V(not p_node, 0);
    ERR_FAIL_COND_V(!state, 0);

    int blend_count = blends.size();

    if (p_node->blends.size() != blend_count) {
        p_node->blends.resize(blend_count);
    }

    float *blendw = p_node->blends.data();
    const float *blendr = blends.data();

    bool any_valid = false;

    if (has_filter() && is_filter_enabled() && p_filter != FILTER_IGNORE) {

        for (int i = 0; i < blend_count; i++) {
            blendw[i] = 0.0; //all to zero by default
        }

        for(const auto &e : filter) {
            if (!state->track_map.contains(e)) {
                continue;
            }
            int idx = state->track_map[e];
            blendw[idx] = 1.0; //filtered goes to one
        }

        switch (p_filter) {
            case FILTER_IGNORE:
                break; //will not happen anyway
            case FILTER_PASS: {
                //values filtered pass, the rest don't
                for (int i = 0; i < blend_count; i++) {
                    if (blendw[i] == 0) //not filtered, does not pass
                        continue;

                    blendw[i] = blendr[i] * p_blend;
                    if (blendw[i] > CMP_EPSILON) {
                        any_valid = true;
                    }
                }

            } break;
            case FILTER_STOP: {

                //values filtered don't pass, the rest are blended

                for (int i = 0; i < blend_count; i++) {
                    if (blendw[i] > 0) //filtered, does not pass
                        continue;

                    blendw[i] = blendr[i] * p_blend;
                    if (blendw[i] > CMP_EPSILON) {
                        any_valid = true;
                    }
                }

            } break;
            case FILTER_BLEND: {

                //filtered values are blended, the rest are passed without blending

                for (int i = 0; i < blend_count; i++) {
                    if (blendw[i] == 1.0) {
                        blendw[i] = blendr[i] * p_blend; //filtered, blend
                    } else {
                        blendw[i] = blendr[i]; //not filtered, do not blend
                    }

                    if (blendw[i] > CMP_EPSILON) {
                        any_valid = true;
                    }
                }

            } break;
        }
    } else {
        for (int i = 0; i < blend_count; i++) {

            //regular blend
            blendw[i] = blendr[i] * p_blend;
            if (blendw[i] > CMP_EPSILON) {
                any_valid = true;
            }
        }
    }

    if (r_max) {
        *r_max = 0;
        for (int i = 0; i < blend_count; i++) {
            *r_max = M_MAX(*r_max, blendw[i]);
        }
    }

    if (!p_seek && p_optimize && !any_valid) //pointless to go on, all are zero
        return 0;

    StringName new_path;
    AnimationNode *new_parent;

    //this is the slowest part of processing, but as strings process in powers of 2, and the paths always exist, it will not result in that many allocations
    if (p_new_parent) {
        new_parent = p_new_parent;
        new_path = StringName(String(base_path) + p_subpath + "/");
    } else {
        ERR_FAIL_COND_V(!parent, 0);
        new_parent = parent;
        new_path = StringName(String(parent->base_path) + p_subpath + "/");
    }
    return p_node->_pre_process(new_path, new_parent, state, p_time, p_seek, p_connections);
}

int AnimationNode::get_input_count() const {

    return inputs.size();
}
String AnimationNode::get_input_name(int p_input) {
    ERR_FAIL_INDEX_V(p_input, inputs.size(), String());
    return inputs[p_input].name;
}

StringView AnimationNode::get_caption() const {
    thread_local char buf[512];
    if (get_script_instance()) {
        buf[0]=0;
        strncat(buf,get_script_instance()->call("get_caption").as<String>().c_str(),511);
        return buf;
    }

    return "Node";
}

void AnimationNode::add_input(const String &p_name) {
    //root nodes can't add inputs
    ERR_FAIL_COND(object_cast<AnimationRootNode>(this) != nullptr);
    Input input;
    ERR_FAIL_COND(StringUtils::contains(p_name,".") || StringUtils::contains(p_name,"/") );
    input.name = p_name;
    inputs.push_back(input);
    emit_changed();
}

void AnimationNode::set_input_name(int p_input, StringView p_name) {
    ERR_FAIL_INDEX(p_input, inputs.size());
    ERR_FAIL_COND(StringUtils::contains(p_name,".") || StringUtils::contains(p_name,"/"));
    inputs[p_input].name = p_name;
    emit_changed();
}

void AnimationNode::remove_input(int p_index) {
    ERR_FAIL_INDEX(p_index, inputs.size());
    inputs.erase_at(p_index);
    emit_changed();
}

float AnimationNode::process(float p_time, bool p_seek) {

    if (get_script_instance()) {
        return get_script_instance()->call("process", p_time, p_seek).as<float>();
    }

    return 0;
}

void AnimationNode::set_filter_path(const NodePath &p_path, bool p_enable) {
    if (p_enable) {
        filter.insert(p_path);
    } else {
        filter.erase(p_path);
    }
}

void AnimationNode::set_filter_enabled(bool p_enable) {
    filter_enabled = p_enable;
}

bool AnimationNode::is_filter_enabled() const {
    return filter_enabled;
}

bool AnimationNode::is_path_filtered(const NodePath &p_path) const {
    return filter.contains(p_path);
}

bool AnimationNode::has_filter() const {
    return false;
}

Array AnimationNode::_get_filters() const {

    Array paths;

    for(const auto &e : filter) {
        paths.push_back(e.asString()); //use strings, so sorting is possible
    }
    paths.sort(); //done so every time the scene is saved, it does not change

    return paths;
}
void AnimationNode::_set_filters(const Array &p_filters) {
    filter.clear();
    for (int i = 0; i < p_filters.size(); i++) {
        set_filter_path(p_filters[i].as<NodePath>(), true);
    }
}

void AnimationNode::_validate_property(PropertyInfo &property) const {
    if (!has_filter() && (property.name == "filter_enabled" || property.name == "filters")) {
        property.usage = 0;
    }
}

Ref<AnimationNode> AnimationNode::get_child_by_name(const StringName &p_name) {
    if (get_script_instance()) {
        return refFromVariant<AnimationNode>(get_script_instance()->call("get_child_by_name",p_name));
    }
    return Ref<AnimationNode>();
}

void AnimationNode::_bind_methods() {

    SE_BIND_METHOD(AnimationNode,get_input_count);
    SE_BIND_METHOD(AnimationNode,get_input_name);

    SE_BIND_METHOD(AnimationNode,add_input);
    SE_BIND_METHOD(AnimationNode,remove_input);

    SE_BIND_METHOD(AnimationNode,set_filter_path);
    SE_BIND_METHOD(AnimationNode,is_path_filtered);

    SE_BIND_METHOD(AnimationNode,set_filter_enabled);
    SE_BIND_METHOD(AnimationNode,is_filter_enabled);

    SE_BIND_METHOD(AnimationNode,_set_filters);
    SE_BIND_METHOD(AnimationNode,_get_filters);

    SE_BIND_METHOD(AnimationNode,blend_animation);
    MethodBinder::bind_method(D_METHOD("blend_node", {"name", "node", "time", "seek", "blend", "filter", "optimize"}), &AnimationNode::blend_node, {DEFVAL(FILTER_IGNORE), DEFVAL(true)});
    MethodBinder::bind_method(D_METHOD("blend_input", {"input_index", "time", "seek", "blend", "filter", "optimize"}), &AnimationNode::blend_input, {DEFVAL(FILTER_IGNORE), DEFVAL(true)});

    SE_BIND_METHOD(AnimationNode,set_parameter);
    SE_BIND_METHOD(AnimationNode,get_parameter);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "filter_enabled", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_filter_enabled", "is_filter_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "filters", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_filters", "_get_filters");

    BIND_VMETHOD(MethodInfo(VariantType::DICTIONARY, "get_child_nodes"));
    BIND_VMETHOD(MethodInfo(VariantType::ARRAY, "get_parameter_list"));
    BIND_VMETHOD(MethodInfo(VariantType::OBJECT, "get_child_by_name", PropertyInfo(VariantType::STRING, "name")));
    {
        MethodInfo mi = MethodInfo(VariantType::NIL, "get_parameter_default_value", PropertyInfo(VariantType::STRING_NAME, "name"));
        mi.return_val.usage = PROPERTY_USAGE_NIL_IS_VARIANT;
        BIND_VMETHOD(mi);
    }
    BIND_VMETHOD(MethodInfo("process", PropertyInfo(VariantType::FLOAT, "time"), PropertyInfo(VariantType::BOOL, "seek")));
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "get_caption"));
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "has_filter"));

    ADD_SIGNAL(MethodInfo("removed_from_graph"));

    ADD_SIGNAL(MethodInfo("tree_changed"));

    BIND_ENUM_CONSTANT(FILTER_IGNORE);
    BIND_ENUM_CONSTANT(FILTER_PASS);
    BIND_ENUM_CONSTANT(FILTER_STOP);
    BIND_ENUM_CONSTANT(FILTER_BLEND);
}

AnimationNode::AnimationNode() {

    state = nullptr;
    parent = nullptr;
    filter_enabled = false;
}

////////////////////

void AnimationTree::set_tree_root(const Ref<AnimationNode> &p_root) {

    if (root) {
        root->disconnect("tree_changed",callable_mp(this, &ClassName::_tree_changed));
    }

    root = p_root;

    if (root) {
        root->connect("tree_changed",callable_mp(this, &ClassName::_tree_changed));
    }

    properties_dirty = true;

    update_configuration_warning();
}

Ref<AnimationNode> AnimationTree::get_tree_root() const {
    return root;
}

void AnimationTree::set_active(bool p_active) {

    if (active == p_active)
        return;

    active = p_active;
    started = active;

    if (process_mode == ANIMATION_PROCESS_IDLE) {
        set_process_internal(active);
    } else {

        set_physics_process_internal(active);
    }

    if (!active && is_inside_tree()) {
        for (TrackCache * E : playing_caches) {

            if (object_for_entity(E->object_id)) {
                E->object->call_va("stop");
            }
        }

        playing_caches.clear();
    }
}

bool AnimationTree::is_active() const {

    return active;
}

void AnimationTree::set_process_mode(AnimationProcessMode p_mode) {

    if (process_mode == p_mode)
        return;

    bool was_active = is_active();
    if (was_active) {
        set_active(false);
    }

    process_mode = p_mode;

    if (was_active) {
        set_active(true);
    }
}

AnimationTree::AnimationProcessMode AnimationTree::get_process_mode() const {
    return process_mode;
}

void AnimationTree::_node_removed(Node *p_node) {
    cache_valid = false;
}

bool AnimationTree::_update_caches(AnimationPlayer *player) {

    setup_pass++;

    if (!player->has_node(player->get_root())) {
        ERR_PRINT("AnimationTree: AnimationPlayer root is invalid.");
        set_active(false);
        return false;
    }
    Node *parent = player->get_node(player->get_root());

    Vector<StringName> sname(player->get_animation_list());

    for (const StringName &E : sname) {
        Ref<Animation> anim = player->get_animation(E);
        for (int i = 0; i < anim->get_track_count(); i++) {
            NodePath path = anim->track_get_path(i);
            Animation::TrackType track_type = anim->track_get_type(i);

            TrackCache *track = nullptr;
            if (track_cache.contains(path)) {
                track = track_cache.at(path);
            }

            //if not valid, delete track
            if (track && (track->type != track_type || object_for_entity(track->object_id) == nullptr)) {
                playing_caches.erase(track);
                memdelete(track);
                track_cache.erase(path);
                track = nullptr;
            }

            if (!track) {

                RES resource;
                Vector<StringName> leftover_path;
                Node *child = parent->get_node_and_resource(path, resource, leftover_path);

                if (!child) {
                    ERR_PRINT("AnimationTree: '" + String(E) + "', couldn't resolve track:  '" + String(path) + "'");
                    continue;
                }

                if (!child->is_connected("tree_exited",callable_mp(this, &ClassName::_node_removed))) {
                    child->connectF("tree_exited",this,[=]() { _node_removed(child); });
                }

                switch (track_type) {
                    case Animation::TYPE_VALUE: {

                        TrackCacheValue *track_value = memnew(TrackCacheValue);

                        if (resource) {
                            track_value->object = resource.get();
                        } else {
                            track_value->object = child;
                        }

                        track_value->subpath = leftover_path;
                        track_value->object_id = track_value->object->get_instance_id();

                        track = track_value;

                    } break;
                    case Animation::TYPE_TRANSFORM: {

                        Node3D *spatial = object_cast<Node3D>(child);

                        if (!spatial) {
                            ERR_PRINT("AnimationTree: '" + String(E) + "', transform track does not point to spatial:  '" + String(path) + "'");
                            continue;
                        }

                        TrackCacheTransform *track_xform = memnew(TrackCacheTransform);

                        track_xform->spatial = spatial;
                        track_xform->skeleton = nullptr;
                        track_xform->bone_idx = -1;
                        Skeleton *sk;
                        if (path.get_subname_count() == 1 && (sk=object_cast<Skeleton>(spatial))) {
                            assert(sk);

                            track_xform->skeleton = sk;
                            int bone_idx = sk->find_bone(path.get_subname(0));
                            if (bone_idx != -1) {

                                track_xform->bone_idx = bone_idx;
                            }
                        }

                        track_xform->object = spatial;
                        track_xform->object_id = track_xform->object->get_instance_id();

                        track = track_xform;

                    } break;
                    case Animation::TYPE_METHOD: {

                        TrackCacheMethod *track_method = memnew(TrackCacheMethod);

                        if (resource) {
                            track_method->object = resource.get();
                        } else {
                            track_method->object = child;
                        }

                        track_method->object_id = track_method->object->get_instance_id();

                        track = track_method;

                    } break;
                    case Animation::TYPE_BEZIER: {

                        TrackCacheBezier *track_bezier = memnew(TrackCacheBezier);

                        if (resource) {
                            track_bezier->object = resource.get();
                        } else {
                            track_bezier->object = child;
                        }

                        track_bezier->subpath = leftover_path;
                        track_bezier->object_id = track_bezier->object->get_instance_id();

                        track = track_bezier;
                    } break;
                    case Animation::TYPE_AUDIO: {

                        TrackCacheAudio *track_audio = memnew(TrackCacheAudio);

                        track_audio->object = child;
                        track_audio->object_id = track_audio->object->get_instance_id();

                        track = track_audio;

                    } break;
                    case Animation::TYPE_ANIMATION: {

                        TrackCacheAnimation *track_animation = memnew(TrackCacheAnimation);

                        track_animation->object = child;
                        track_animation->object_id = track_animation->object->get_instance_id();

                        track = track_animation;

                    } break;
                    default: {
                        ERR_PRINT("Animation corrupted (invalid track type)");
                        continue;
                    }
                }

                track_cache[path] = track;
            }

            track->setup_pass = setup_pass;
        }
    }

    FixedVector<NodePath,16,true> to_delete;

    for(const auto & e : track_cache) {
        TrackCache *tc = e.second;
        if (tc->setup_pass != setup_pass) {
            to_delete.emplace_back(e.first);
        }
    }

    while (!to_delete.empty()) {
        const NodePath &np = to_delete.front();
        memdelete(track_cache[np]);
        track_cache.erase(np);
        to_delete.pop_front();
    }

    state.track_map.clear();


    int idx = 0;
    for(const auto &e : track_cache) {
        state.track_map[e.first] = idx;
        idx++;
    }

    state.track_count = idx;

    cache_valid = true;

    return true;
}

void AnimationTree::_clear_caches() {

    for(const auto &e : track_cache) {
        memdelete(e.second);
    }
    playing_caches.clear();

    track_cache.clear();
    cache_valid = false;
}

void AnimationTree::_process_graph(float p_delta) {

    _update_properties(); //if properties need updating, update them

    //check all tracks, see if they need modification

    root_motion_transform = Transform();

    if (not root) {
        ERR_PRINT("AnimationTree: root AnimationNode is not set, disabling playback.");
        set_active(false);
        cache_valid = false;
        return;
    }

    if (!has_node(animation_player)) {
        ERR_PRINT("AnimationTree: no valid AnimationPlayer path set, disabling playback");
        set_active(false);
        cache_valid = false;
        return;
    }

    AnimationPlayer *player = object_cast<AnimationPlayer>(get_node(animation_player));

    GameEntity current_animation_player = entt::null;

    if (player) {
        current_animation_player = player->get_instance_id();
    }

    if (last_animation_player != current_animation_player) {

        if (last_animation_player!=entt::null) {
            Object *old_player = object_for_entity(last_animation_player);
            if (old_player) {
                old_player->disconnect("caches_cleared",callable_mp(this, &ClassName::_clear_caches));
            }
        }

        if (player) {
            player->connect("caches_cleared",callable_mp(this, &ClassName::_clear_caches));
        }

        last_animation_player = current_animation_player;
    }

    if (!player) {
        ERR_PRINT("AnimationTree: path points to a node not an AnimationPlayer, disabling playback");
        set_active(false);
        cache_valid = false;
        return;
    }

    if (!cache_valid) {
        if (!_update_caches(player)) {
            return;
        }
    }

    { //setup

        process_pass++;

        state.valid = true;
        state.invalid_reasons = "";
        state.animation_states.clear(); //will need to be re-created
        state.valid = true;
        state.player = player;
        state.last_pass = process_pass;
        state.tree = this;

        // root source blends

        root->blends.resize(state.track_count);
        float *src_blendsw = root->blends.data();
        for (int i = 0; i < state.track_count; i++) {
            src_blendsw[i] = 1.0; //by default all go to 1 for the root input
        }
    }

    //process

    {

        if (started) {
            //if started, seek
            root->_pre_process(SceneStringNames::parameters_base_path, nullptr, &state, 0, true, {});
            started = false;
        }

        root->_pre_process(SceneStringNames::parameters_base_path, nullptr, &state, p_delta, false, {});
    }

    if (!state.valid) {
        return; //state is not valid. do nothing.
    }
    //apply value/transform/bezier blends to track caches and execute method/audio/animation tracks

    {

        bool can_call = is_inside_tree() && !Engine::get_singleton()->is_editor_hint();

        for (const AnimationNode::AnimationState& as : state.animation_states) {

            Ref<Animation> a = as.animation;
            float time = as.time;
            float delta = as.delta;
            float weight = as.blend;
            bool seeked = as.seeked;

            for (int i = 0; i < a->get_track_count(); i++) {

                NodePath path = a->track_get_path(i);

                ERR_CONTINUE(!track_cache.contains(path));

                TrackCache *track = track_cache[path];
                if (track->type != a->track_get_type(i)) {
                    continue; //may happen should not
                }

                track->root_motion = root_motion_track == path;

                ERR_CONTINUE(!state.track_map.contains(path));
                int blend_idx = state.track_map[path];

                ERR_CONTINUE(blend_idx < 0 || blend_idx >= state.track_count);

                float blend = (*as.track_blends)[blend_idx] * weight;

                if (blend < CMP_EPSILON)
                    continue; //nothing to blend

                switch (track->type) {

                    case Animation::TYPE_TRANSFORM: {

                        TrackCacheTransform *t = static_cast<TrackCacheTransform *>(track);

                        if (track->root_motion) {

                            if (t->process_pass != process_pass) {

                                t->process_pass = process_pass;
                                t->loc = Vector3();
                                t->rot = Quat();
                                t->rot_blend_accum = 0;
                                t->scale = Vector3(1, 1, 1);
                            }

                            float prev_time = time - delta;
                            if (prev_time < 0) {
                                if (!a->has_loop()) {
                                    prev_time = 0;
                                } else {
                                    prev_time = a->get_length() + prev_time;
                                }
                            }

                            Vector3 loc[2];
                            Quat rot[2];
                            Vector3 scale[2];

                            if (prev_time > time) {

                                Error err = a->transform_track_interpolate(i, prev_time, &loc[0], &rot[0], &scale[0]);
                                if (err != OK) {
                                    continue;
                                }

                                a->transform_track_interpolate(i, a->get_length(), &loc[1], &rot[1], &scale[1]);

                                t->loc += (loc[1] - loc[0]) * blend;
                                t->scale += (scale[1] - scale[0]) * blend;
                                Quat q = Quat().slerp(rot[0].normalized().inverse() * rot[1].normalized(), blend).normalized();
                                t->rot = (t->rot * q).normalized();

                                prev_time = 0;
                            }

                            Error err = a->transform_track_interpolate(i, prev_time, &loc[0], &rot[0], &scale[0]);
                            if (err != OK) {
                                continue;
                            }

                            a->transform_track_interpolate(i, time, &loc[1], &rot[1], &scale[1]);

                            t->loc += (loc[1] - loc[0]) * blend;
                            t->scale += (scale[1] - scale[0]) * blend;
                            Quat q = Quat().slerp(rot[0].normalized().inverse() * rot[1].normalized(), blend).normalized();
                            t->rot = (t->rot * q).normalized();

                            prev_time = 0;

                        } else {
                            Vector3 loc;
                            Quat rot;
                            Vector3 scale;

                            Error err = a->transform_track_interpolate(i, time, &loc, &rot, &scale);
                            //ERR_CONTINUE(err!=OK); //used for testing, should be removed

                            if (t->process_pass != process_pass) {

                                t->process_pass = process_pass;
                                t->loc = loc;
                                t->rot = rot;
                                t->rot_blend_accum = 0;
                                t->scale = scale;
                            }

                            if (err != OK)
                                continue;

                            t->loc = t->loc.linear_interpolate(loc, blend);
                            if (t->rot_blend_accum == 0) {
                                t->rot = rot;
                                t->rot_blend_accum = blend;
                            } else {
                                float rot_total = t->rot_blend_accum + blend;
                                t->rot = rot.slerp(t->rot, t->rot_blend_accum / rot_total).normalized();
                                t->rot_blend_accum = rot_total;
                            }
                            t->scale = t->scale.linear_interpolate(scale, blend);
                        }

                    } break;
                    case Animation::TYPE_VALUE: {

                        TrackCacheValue *t = static_cast<TrackCacheValue *>(track);

                        Animation::UpdateMode update_mode = a->value_track_get_update_mode(i);

                        if (update_mode == Animation::UPDATE_CONTINUOUS || update_mode == Animation::UPDATE_CAPTURE) { //delta == 0 means seek

                            Variant value = a->value_track_interpolate(i, time);

                            if (value == Variant())
                                continue;

                            if (t->process_pass != process_pass) {
                                t->value = value;
                                t->process_pass = process_pass;
                            }

                            Variant::interpolate(t->value, value, blend, t->value);

                        } else {

                            Vector<int> indices;
                            a->value_track_get_key_indices(i, time, delta, &indices);

                            for (int F : indices) {

                                Variant value = a->track_get_key_value(i, F);
                                t->object->set_indexed(t->subpath, value);
                            }
                        }

                    } break;
                    case Animation::TYPE_METHOD: {

                        if (delta == 0) {
                            continue;
                        }
                        TrackCacheMethod *t = static_cast<TrackCacheMethod *>(track);

                        Vector<int> indices;

                        a->method_track_get_key_indices(i, time, delta, &indices);

                        for (int F : indices) {

                            StringName method = a->method_track_get_name(i, F);
                            const Vector<Variant> &params = a->method_track_get_params(i, F);

                            int s = params.size();

                            ERR_CONTINUE(s > VARIANT_ARG_MAX);
                            if (can_call) {
                                t->object->call_deferred(
                                        method,
                                        s >= 1 ? params[0] : Variant(),
                                        s >= 2 ? params[1] : Variant(),
                                        s >= 3 ? params[2] : Variant(),
                                        s >= 4 ? params[3] : Variant(),
                                        s >= 5 ? params[4] : Variant());
                            }
                        }

                    } break;
                    case Animation::TYPE_BEZIER: {

                        TrackCacheBezier *t = static_cast<TrackCacheBezier *>(track);

                        float bezier = a->bezier_track_interpolate(i, time);

                        if (t->process_pass != process_pass) {
                            t->value = bezier;
                            t->process_pass = process_pass;
                        }

                        t->value = Math::lerp(t->value, bezier, blend);

                    } break;
                    case Animation::TYPE_AUDIO: {

                        TrackCacheAudio *t = static_cast<TrackCacheAudio *>(track);

                        if (seeked) {
                            //find whatever should be playing
                            int idx = a->track_find_key(i, time);
                            if (idx < 0)
                                continue;

                            Ref<AudioStream> stream = dynamic_ref_cast<AudioStream>(a->audio_track_get_key_stream(i, idx));
                            if (not stream) {
                                t->object->call_va("stop");
                                t->playing = false;
                                playing_caches.erase(t);
                            } else {
                                float start_ofs = a->audio_track_get_key_start_offset(i, idx);
                                start_ofs += time - a->track_get_key_time(i, idx);
                                float end_ofs = a->audio_track_get_key_end_offset(i, idx);
                                float len = stream->get_length();

                                if (start_ofs > len - end_ofs) {
                                    t->object->call_va("stop");
                                    t->playing = false;
                                    playing_caches.erase(t);
                                    continue;
                                }

                                t->object->call_va("set_stream", stream);
                                t->object->call_va("play", start_ofs);

                                t->playing = true;
                                playing_caches.insert(t);
                                if (len && end_ofs > 0) { //force a end at a time
                                    t->len = len - start_ofs - end_ofs;
                                } else {
                                    t->len = 0;
                                }

                                t->start = time;
                            }

                        } else {
                            //find stuff to play
                            Vector<int> to_play;
                            a->track_get_key_indices_in_range(i, time, delta, &to_play);
                            if (!to_play.empty()) {
                                int idx = to_play.back();

                                Ref<AudioStream> stream = dynamic_ref_cast<AudioStream>(a->audio_track_get_key_stream(i, idx));
                                if (not stream) {
                                    t->object->call_va("stop");
                                    t->playing = false;
                                    playing_caches.erase(t);
                                } else {
                                    float start_ofs = a->audio_track_get_key_start_offset(i, idx);
                                    float end_ofs = a->audio_track_get_key_end_offset(i, idx);
                                    float len = stream->get_length();

                                    t->object->call_va("set_stream", stream);
                                    t->object->call_va("play", start_ofs);

                                    t->playing = true;
                                    playing_caches.insert(t);
                                    if (len && end_ofs > 0) { //force a end at a time
                                        t->len = len - start_ofs - end_ofs;
                                    } else {
                                        t->len = 0;
                                    }

                                    t->start = time;
                                }
                            } else if (t->playing) {

                                bool loop = a->has_loop();

                                bool stop = false;

                                if (!loop && time < t->start) {
                                    stop = true;
                                } else if (t->len > 0) {
                                    float len = t->start > time ? (a->get_length() - t->start) + time : time - t->start;

                                    if (len > t->len) {
                                        stop = true;
                                    }
                                }

                                if (stop) {
                                    //time to stop
                                    t->object->call_va("stop");
                                    t->playing = false;
                                    playing_caches.erase(t);
                                }
                            }
                        }

                        float db = Math::linear2db(M_MAX(blend, 0.00001));
                        if (t->object->has_method("set_unit_db")) {
                            t->object->call_va("set_unit_db", db);
                        } else {
                            t->object->call_va("set_volume_db", db);
                        }
                    } break;
                    case Animation::TYPE_ANIMATION: {

                        TrackCacheAnimation *t = static_cast<TrackCacheAnimation *>(track);

                        AnimationPlayer *player2 = object_cast<AnimationPlayer>(t->object);

                        if (!player2)
                            continue;

                        if (delta == 0 || seeked) {
                            //seek
                            int idx = a->track_find_key(i, time);
                            if (idx < 0)
                                continue;

                            float pos = a->track_get_key_time(i, idx);

                            StringName anim_name = a->animation_track_get_key_animation(i, idx);
                            if (anim_name == "[stop]" || !player2->has_animation(anim_name))
                                continue;

                            Ref<Animation> anim = player2->get_animation(anim_name);

                            float at_anim_pos;

                            if (anim->has_loop()) {
                                at_anim_pos = Math::fposmod(time - pos, anim->get_length()); //seek to loop
                            } else {
                                at_anim_pos = M_MAX(anim->get_length(), time - pos); //seek to end
                            }

                            if (player2->is_playing() || seeked) {
                                player2->play(anim_name);
                                player2->seek(at_anim_pos);
                                t->playing = true;
                                playing_caches.insert(t);
                            } else {
                                player2->set_assigned_animation(anim_name);
                                player2->seek(at_anim_pos, true);
                            }
                        } else {
                            //find stuff to play
                            Vector<int> to_play;
                            a->track_get_key_indices_in_range(i, time, delta, &to_play);
                            if (!to_play.empty()) {
                                int idx = to_play.back();

                                StringName anim_name = a->animation_track_get_key_animation(i, idx);
                                if (anim_name == "[stop]" || !player2->has_animation(anim_name)) {

                                    if (playing_caches.contains(t)) {
                                        playing_caches.erase(t);
                                        player2->stop();
                                        t->playing = false;
                                    }
                                } else {
                                    player2->play(anim_name);
                                    t->playing = true;
                                    playing_caches.insert(t);
                                }
                            }
                        }

                    } break;
                }
            }
        }
    }

    {
        // finally, set the tracks
        for(const auto &e : track_cache) {
            TrackCache *track = e.second;
            if (track->process_pass != process_pass)
                continue; //not processed, ignore

            switch (track->type) {

                case Animation::TYPE_TRANSFORM: {

                    TrackCacheTransform *t = static_cast<TrackCacheTransform *>(track);

                    Transform xform;
                    xform.origin = t->loc;

                    xform.basis.set_quat_scale(t->rot, t->scale);

                    if (t->root_motion) {

                        root_motion_transform = xform;

                        if (t->skeleton && t->bone_idx >= 0) {
                            root_motion_transform = (t->skeleton->get_bone_rest(t->bone_idx) * root_motion_transform) * t->skeleton->get_bone_rest(t->bone_idx).affine_inverse();
                        }
                    } else if (t->skeleton && t->bone_idx >= 0) {

                        t->skeleton->set_bone_pose(t->bone_idx, xform);

                   } else if (!t->skeleton) {

                        t->spatial->set_transform(xform);
                    }

                } break;
                case Animation::TYPE_VALUE: {

                    TrackCacheValue *t = static_cast<TrackCacheValue *>(track);

                    t->object->set_indexed(t->subpath, t->value);

                } break;
                case Animation::TYPE_BEZIER: {

                    TrackCacheBezier *t = static_cast<TrackCacheBezier *>(track);

                    t->object->set_indexed(t->subpath, t->value);

                } break;
                default: {
                } //the rest don't matter
            }
        }
    }
}

void AnimationTree::advance(float p_time) {

    _process_graph(p_time);
}

void AnimationTree::_notification(int p_what) {

    if (active && OS::get_singleton()->is_update_pending()) {
        if (p_what == NOTIFICATION_INTERNAL_PHYSICS_PROCESS && process_mode == ANIMATION_PROCESS_PHYSICS) {
        _process_graph(get_physics_process_delta_time());
    }

        if (p_what == NOTIFICATION_INTERNAL_PROCESS && process_mode == ANIMATION_PROCESS_IDLE) {
        _process_graph(get_process_delta_time());
    }

    }
    if (p_what == NOTIFICATION_EXIT_TREE) {
        _clear_caches();
        if (last_animation_player!=entt::null) {

            Object *player = object_for_entity(last_animation_player);
            if (player) {
                player->disconnect("caches_cleared",callable_mp(this, &ClassName::_clear_caches));
            }
        }
    } else if (p_what == NOTIFICATION_ENTER_TREE) {
        if (last_animation_player!=entt::null) {

            Object *player = object_for_entity(last_animation_player);
            if (player) {
                player->connect("caches_cleared",callable_mp(this, &ClassName::_clear_caches));
            }
        }
    }
}

void AnimationTree::set_animation_player(const NodePath &p_player) {
    animation_player = p_player;
    update_configuration_warning();
}

NodePath AnimationTree::get_animation_player() const {
    return animation_player;
}

bool AnimationTree::is_state_invalid() const {

    return !state.valid;
}
String AnimationTree::get_invalid_state_reason() const {

    return state.invalid_reasons;
}

uint64_t AnimationTree::get_last_process_pass() const {
    return process_pass;
}

String AnimationTree::get_configuration_warning() const {

    String warning(Node::get_configuration_warning());

    if (not root) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTRS("No root AnimationNode for the graph is set.");
    }

    if (!has_node(animation_player)) {

        if (!warning.empty()) {
            warning += "\n\n";
        }

        warning += TTRS("Path to an AnimationPlayer node containing animations is not set.");
        return warning;
    }

    AnimationPlayer *player = object_cast<AnimationPlayer>(get_node(animation_player));

    if (!player) {
        if (!warning.empty()) {
            warning += ("\n\n");
        }

        warning += TTRS("Path set for AnimationPlayer does not lead to an AnimationPlayer node.");
    }
    else if (!player->has_node(player->get_root())) {
        if (!warning.empty()) {
            warning += "\n\n";
        }

        warning += TTRS("The AnimationPlayer root node is not a valid node.");
    }
    return warning;
}

void AnimationTree::set_root_motion_track(const NodePath &p_track) {
    root_motion_track = p_track;
}

NodePath AnimationTree::get_root_motion_track() const {
    return root_motion_track;
}

Transform AnimationTree::get_root_motion_transform() const {
    return root_motion_transform;
}

void AnimationTree::_tree_changed() {
    if (properties_dirty) {
        return;
    }

    call_deferred([this]() {_update_properties();});
    properties_dirty = true;
}

void AnimationTree::_update_properties_for_node(const StringName &p_base_path, Ref<AnimationNode> node) {
    ERR_FAIL_COND(not node);

    if (!property_parent_map.contains(p_base_path)) {
        property_parent_map[p_base_path] = {};
    }

    if (node->get_input_count() && !input_activity_map.contains(p_base_path)) {

        Vector<Activity> activity;
        activity.reserve(node->get_input_count());
        for (int i = 0; i < node->get_input_count(); i++) {
            Activity a;
            a.activity = 0;
            a.last_pass = 0;
            activity.emplace_back(a);
        }
        input_activity_map[p_base_path] = eastl::move(activity);
        //TODO: why is the last character trimmed below, document this or remove the trimming.
        input_activity_map_get[StringName(StringUtils::substr(p_base_path, 0, StringView(p_base_path).length() - 1))] =
                &input_activity_map[p_base_path];
    }

    Vector<PropertyInfo> plist;
    node->get_parameter_list(&plist);
    for (PropertyInfo pinfo : plist) {

        StringName key = pinfo.name;
        StringName concat(String(p_base_path)+key);
        if (!property_map.contains(concat)) {
            property_map[concat] = node->get_parameter_default_value(key);
        }

        property_parent_map[p_base_path][key] = concat;

        pinfo.name = concat;
        properties.emplace_back(eastl::move(pinfo));
    }

    Vector<AnimationNode::ChildNode> children;
    node->get_child_nodes(&children);

    for (const AnimationNode::ChildNode &E : children) {
        _update_properties_for_node(StringName(String(p_base_path) + E.name + "/"), E.node);
    }
}

void AnimationTree::_update_properties() {
    if (!properties_dirty) {
        return;
    }

    properties.clear();
    property_parent_map.clear();
    input_activity_map.clear();
    input_activity_map_get.clear();

    if (root) {
        _update_properties_for_node(SceneStringNames::parameters_base_path, root);
    }

    properties_dirty = false;

    Object_change_notify(this);
}

bool AnimationTree::_set(const StringName &p_name, const Variant &p_value) {
    if (properties_dirty) {
        _update_properties();
    }

    if (property_map.contains(p_name)) {
        property_map[p_name] = p_value;
        Object_change_notify(this,p_name);
        return true;
    }

    return false;
}

bool AnimationTree::_get(const StringName &p_name, Variant &r_ret) const {
    if (properties_dirty) {
        const_cast<AnimationTree *>(this)->_update_properties();
    }

    if (property_map.contains(p_name)) {
        r_ret = property_map.at(p_name);
        return true;
    }

    return false;
}
void AnimationTree::_get_property_list(Vector<PropertyInfo> *p_list) const {
    if (properties_dirty) {
        const_cast<AnimationTree *>(this)->_update_properties();
    }
    p_list->push_back(properties);
}

void AnimationTree::rename_parameter(StringView p_base, StringView p_new_base) {

    //rename values first
    for (const PropertyInfo &E : properties) {
        if (StringUtils::begins_with(E.name,p_base)) {
            StringName new_name(StringUtils::replace_first(E.name,p_base, p_new_base));
            property_map[new_name] = property_map[E.name];
        }
    }

    //update tree second
    properties_dirty = true;
    _update_properties();
}

float AnimationTree::get_connection_activity(const StringName &p_path, int p_connection) const {

    if (!input_activity_map_get.contains(p_path)) {
        return 0;
    }
    const Vector<Activity> *activity = input_activity_map_get.at(p_path);

    if (!activity || p_connection < 0 || p_connection >= activity->size()) {
        return 0;
    }

    if ((*activity)[p_connection].last_pass != process_pass) {
        return 0;
    }

    return (*activity)[p_connection].activity;
}

void AnimationTree::_bind_methods() {
    SE_BIND_METHOD(AnimationTree,set_active);
    SE_BIND_METHOD(AnimationTree,is_active);

    SE_BIND_METHOD(AnimationTree,set_tree_root);
    SE_BIND_METHOD(AnimationTree,get_tree_root);

    SE_BIND_METHOD(AnimationTree,set_process_mode);
    SE_BIND_METHOD(AnimationTree,get_process_mode);

    SE_BIND_METHOD(AnimationTree,set_animation_player);
    SE_BIND_METHOD(AnimationTree,get_animation_player);

    SE_BIND_METHOD(AnimationTree,set_root_motion_track);
    SE_BIND_METHOD(AnimationTree,get_root_motion_track);

    SE_BIND_METHOD(AnimationTree,get_root_motion_transform);

    SE_BIND_METHOD(AnimationTree,rename_parameter);

    SE_BIND_METHOD(AnimationTree,advance);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "tree_root", PropertyHint::ResourceType, "AnimationRootNode"), "set_tree_root", "get_tree_root");
    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "anim_player", PropertyHint::NodePathValidTypes, "AnimationPlayer"), "set_animation_player", "get_animation_player");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "active"), "set_active", "is_active");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "process_mode", PropertyHint::Enum, "Physics,Idle,Manual"), "set_process_mode", "get_process_mode");
    ADD_GROUP("Root Motion", "root_motion_");
    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "root_motion_track"), "set_root_motion_track", "get_root_motion_track");

    BIND_ENUM_CONSTANT(ANIMATION_PROCESS_PHYSICS);
    BIND_ENUM_CONSTANT(ANIMATION_PROCESS_IDLE);
    BIND_ENUM_CONSTANT(ANIMATION_PROCESS_MANUAL);
}

AnimationTree::AnimationTree() {

    last_animation_player = entt::null;
}

AnimationTree::~AnimationTree() {
}
