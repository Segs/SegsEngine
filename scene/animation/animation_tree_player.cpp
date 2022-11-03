/*************************************************************************/
/*  animation_tree_player.cpp                                            */
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

#include "animation_tree_player.h"
#include "animation_player.h"

#include "core/dictionary.h"
#include "core/method_bind.h"
#include "core/pool_vector.h"
#include "core/translation_helpers.h"
#include "core/os/os.h"
#include "scene/scene_string_names.h"

IMPL_GDCLASS(AnimationTreePlayer)
VARIANT_ENUM_CAST(AnimationTreePlayer::NodeType);
VARIANT_ENUM_CAST(AnimationTreePlayer::AnimationProcessMode);

namespace  {
struct AnimationTreeInput {

    StringName node;
    //AnimationTreeInput() { node=-1;  }
};
} // end of anonymous namespace block 1


struct AnimationTreeNodeBase {
    using NodeType = AnimationTreePlayer::NodeType;
    Point2 pos;
    bool cycletest;
    NodeType type;

    Vector<AnimationTreeInput> inputs;

    AnimationTreeNodeBase() { cycletest = false; };
    virtual ~AnimationTreeNodeBase() { cycletest = false; }
};

struct AnimationTreeNodeOut : public AnimationTreeNodeBase {

    AnimationTreeNodeOut() {
        type = NodeType::NODE_OUTPUT;
        inputs.resize(1);
    }
};

struct AnimationTreeNode : public AnimationTreeNodeBase {

    Ref<Animation> animation;

    struct TrackRef {
        AnimationTreePlayer::Track* track;
        int local_track;
        float weight;
    };

    uint64_t last_version;
    List<TrackRef> tref;
    String from;
    AnimationTreeNode *next;
    float time;
    float step;
    bool skip;

    HashMap<NodePath, bool> filter;

    AnimationTreeNode() {
        type = NodeType::NODE_ANIMATION;
        next = nullptr;
        last_version = 0;
        skip = false;
    }
};

namespace {
struct OneShotNode : public AnimationTreeNodeBase {

    float fade_in;
    float fade_out;
    float autorestart_delay;
    float autorestart_random_delay;
    float time;
    float remaining;
    float autorestart_remaining;

    bool active;
    bool start;
    bool autorestart;
    bool mix;


    HashMap<NodePath, bool> filter;

    OneShotNode() {
        type = NodeType::NODE_ONESHOT;
        fade_in = 0;
        fade_out = 0;
        inputs.resize(2);
        autorestart = false;
        autorestart_delay = 1;
        autorestart_remaining = 0;
        mix = false;
        active = false;
        start = false;
    }
};

struct MixNode : public AnimationTreeNodeBase {

    float amount;
    MixNode() {
        type = NodeType::NODE_MIX;
        inputs.resize(2);
    }
};

struct Blend2Node : public AnimationTreeNodeBase {

    HashMap<NodePath, bool> filter;
    float value;
    Blend2Node() {
        type = NodeType::NODE_BLEND2;
        value = 0;
        inputs.resize(2);
    }
};

struct Blend3Node : public AnimationTreeNodeBase {

    float value;
    Blend3Node() {
        type = NodeType::NODE_BLEND3;
        value = 0;
        inputs.resize(3);
    }
};

struct Blend4Node : public AnimationTreeNodeBase {

    Point2 value;
    Blend4Node() {
        type = NodeType::NODE_BLEND4;
        inputs.resize(4);
    }
};

struct TimeScaleNode : public AnimationTreeNodeBase {

    float scale;
    TimeScaleNode() {
        type = NodeType::NODE_TIMESCALE;
        scale = 1;
        inputs.resize(1);
    }
};

struct TimeSeekNode : public AnimationTreeNodeBase {

    float seek_pos;

    TimeSeekNode() {
        type = NodeType::NODE_TIMESEEK;
        inputs.resize(1);
        seek_pos = -1;
    }
};


struct TransitionNode : public AnimationTreeNodeBase {

    struct InputData {

        bool auto_advance;
        InputData() { auto_advance = false; }
    };

    Vector<InputData> input_data;

    float prev_time=0.0f;
    float prev_xfading=0.0f;
    int prev=-1;

    float time;
    int current=0;
    float xfade=0.0f;

    bool switched;

    TransitionNode() {
        type = NodeType::NODE_TRANSITION;
        inputs.resize(1);
        input_data.resize(1);
        switched = false;
    }
    void set_current(int p_current) {

        ERR_FAIL_INDEX(p_current, inputs.size());

        if (current == p_current)
            return;

        prev = current;
        prev_xfading = xfade;
        prev_time = time;
        time = 0;
        current = p_current;
        switched = true;
    }
};

} // end of anonymous namespace

void AnimationTreePlayer::set_animation_process_mode(AnimationProcessMode p_mode) {

    if (animation_process_mode == p_mode)
        return;

    bool pr = processing;
    if (pr)
        _set_process(false);
    animation_process_mode = p_mode;
    if (pr)
        _set_process(true);
}

AnimationTreePlayer::AnimationProcessMode AnimationTreePlayer::get_animation_process_mode() const {

    return animation_process_mode;
}

void AnimationTreePlayer::_set_process(bool p_process, bool p_force) {
    if (processing == p_process && !p_force) {
        return;
    }

    switch (animation_process_mode) {

        case ANIMATION_PROCESS_PHYSICS:
            set_physics_process_internal(p_process && active);
            break;
        case ANIMATION_PROCESS_IDLE:
            set_process_internal(p_process && active);
            break;
    }

    processing = p_process;
}

bool AnimationTreePlayer::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "base_path") {
        set_base_path(p_value.as<NodePath>());
        return true;
    }

    if (p_name == "master_player") {
        set_master_player(p_value.as<NodePath>());
        return true;
    }

    if (p_name == SceneStringNames::playback_active) {
        set_active(p_value.as<bool>());
        return true;
    }

    if (not (p_name == "data"))
        return false;

    Dictionary data = p_value.as<Dictionary>();

    Array nodes = data.get_valid("nodes").as<Array>();

    for (int i = 0; i < nodes.size(); i++) {

        Dictionary node = nodes[i].as<Dictionary>();

        StringName id = node.get_valid("id").as<StringName>();
        Point2 pos = node.get_valid("position").as<Vector2>();

        NodeType nt = NODE_MAX;
        StringName type = node.get_valid("type").as<StringName>();

        if (type == "output")
            nt = NODE_OUTPUT;
        else if (type == "animation")
            nt = NODE_ANIMATION;
        else if (type == "oneshot")
            nt = NODE_ONESHOT;
        else if (type == "mix")
            nt = NODE_MIX;
        else if (type == "blend2")
            nt = NODE_BLEND2;
        else if (type == "blend3")
            nt = NODE_BLEND3;
        else if (type == "blend4")
            nt = NODE_BLEND4;
        else if (type == "timescale")
            nt = NODE_TIMESCALE;
        else if (type == "timeseek")
            nt = NODE_TIMESEEK;
        else if (type == "transition")
            nt = NODE_TRANSITION;

        ERR_FAIL_COND_V(nt == NODE_MAX, false);

        if (nt != NODE_OUTPUT)
            add_node(nt, id);
        node_set_position(id, pos);

        switch (nt) {
            case NODE_OUTPUT: {

            } break;
            case NODE_ANIMATION: {

                if (node.has("from"))
                    animation_node_set_master_animation(id, node.get_valid("from").as<String>());
                else
                    animation_node_set_animation(id, refFromVariant<Animation>(node.get_valid("animation")));
                Array filters = node.get_valid("filter").as<Array>();
                for (int j = 0; j < filters.size(); j++) {

                    animation_node_set_filter_path(id, filters[j].as<NodePath>(), true);
                }
            } break;
            case NODE_ONESHOT: {

                oneshot_node_set_fadein_time(id, node.get_valid("fade_in").as<float>());
                oneshot_node_set_fadeout_time(id, node.get_valid("fade_out").as<float>());
                oneshot_node_set_mix_mode(id, node.get_valid("mix").as<bool>());
                oneshot_node_set_autorestart(id, node.get_valid("autorestart").as<bool>());
                oneshot_node_set_autorestart_delay(id, node.get_valid("autorestart_delay").as<float>());
                oneshot_node_set_autorestart_random_delay(id, node.get_valid("autorestart_random_delay").as<float>());
                Array filters = node.get_valid("filter").as<Array>();
                for (int j = 0; j < filters.size(); j++) {

                    oneshot_node_set_filter_path(id, filters[j].as<NodePath>(), true);
                }

            } break;
            case NODE_MIX: {
                mix_node_set_amount(id, node.get_valid("mix").as<float>());
            } break;
            case NODE_BLEND2: {
                blend2_node_set_amount(id, node.get_valid("blend").as<float>());
                Array filters = node.get_valid("filter").as<Array>();
                for (int j = 0; j < filters.size(); j++) {

                    blend2_node_set_filter_path(id, filters[j].as<NodePath>(), true);
                }
            } break;
            case NODE_BLEND3: {
                blend3_node_set_amount(id, node.get_valid("blend").as<float>());
            } break;
            case NODE_BLEND4: {
                blend4_node_set_amount(id, node.get_valid("blend").as<Vector2>());
            } break;
            case NODE_TIMESCALE: {
                timescale_node_set_scale(id, node.get_valid("scale").as<float>());
            } break;
            case NODE_TIMESEEK: {
            } break;
            case NODE_TRANSITION: {

                transition_node_set_xfade_time(id, node.get_valid("xfade").as<float>());

                Array transitions = node.get_valid("transitions").as<Array>();
                transition_node_set_input_count(id, transitions.size());

                for (int x = 0; x < transitions.size(); x++) {

                    Dictionary d = transitions[x].as<Dictionary>();
                    bool aa = d.get_valid("auto_advance").as<bool>();
                    transition_node_set_input_auto_advance(id, x, aa);
                }

            } break;
            default: {
            };
        }
    }

    Array connections = data.get_valid("connections").as<Array>();
    ERR_FAIL_COND_V(connections.size() % 3, false);

    int cc = connections.size() / 3;

    for (int i = 0; i < cc; i++) {

        StringName src = connections[i * 3 + 0].as<StringName>();
        StringName dst = connections[i * 3 + 1].as<StringName>();
        int dst_in = connections[i * 3 + 2].as<int>();
        connect_nodes(src, dst, dst_in);
    }

    set_active(data.get_valid("active").as<bool>());
    set_master_player(data.get_valid("master").as<NodePath>());

    return true;
}

bool AnimationTreePlayer::_get(const StringName &p_name, Variant &r_ret) const {

    if (p_name == "base_path") {
        r_ret = base_path;
        return true;
    }

    if (p_name == "master_player") {
        r_ret = master;
        return true;
    }

    if (p_name == "playback/active") {
        r_ret = is_active();
        return true;
    }

    if (not (p_name == "data"))
        return false;

    Dictionary data;

    Array nodes;

    for (const eastl::pair<const StringName,AnimationTreeNodeBase *> &E : node_map) {

        AnimationTreeNodeBase *n = E.second;

        Dictionary node;
        node["id"] = E.first;
        node["position"] = n->pos;

        const char *sel = "";
        switch (n->type) {
            case NODE_OUTPUT:
                sel = "output";
                break;
            case NODE_ANIMATION:
                sel = "animation";
                break;
            case NODE_ONESHOT:
                sel = "oneshot";
                break;
            case NODE_MIX:
                sel = "mix";
                break;
            case NODE_BLEND2:
                sel = "blend2";
                break;
            case NODE_BLEND3:
                sel = "blend3";
                break;
            case NODE_BLEND4:
                sel = "blend4";
                break;
            case NODE_TIMESCALE:
                sel = "timescale";
                break;
            case NODE_TIMESEEK:
                sel = "timeseek";
                break;
            case NODE_TRANSITION:
                sel = "transition";
                break;
            default:
                break;
        }
        node["type"] = sel;

        switch (n->type) {
            case NODE_OUTPUT: {

            } break;
            case NODE_ANIMATION: {
                AnimationTreeNode *an = static_cast<AnimationTreeNode *>(n);
                if (master != NodePath() && !an->from.empty()) {
                    node["from"] = an->from;
                } else {
                    node["animation"] = an->animation;
                }
                Array k;
                Vector<NodePath> keys;
                an->filter.keys_into(keys);
                k.resize(keys.size());
                int i = 0;
                for (const NodePath &F : keys) {
                    k[i++] = F;
                }
                node["filter"] = k;
            } break;
            case NODE_ONESHOT: {
                OneShotNode *osn = static_cast<OneShotNode *>(n);
                node["fade_in"] = osn->fade_in;
                node["fade_out"] = osn->fade_out;
                node["mix"] = osn->mix;
                node["autorestart"] = osn->autorestart;
                node["autorestart_delay"] = osn->autorestart_delay;
                node["autorestart_random_delay"] = osn->autorestart_random_delay;

                Array k;
                Vector<NodePath> keys;
                osn->filter.keys_into(keys);
                k.resize(keys.size());
                int i = 0;
                for (const NodePath &F : keys) {
                    k[i++] = F;
                }
                node["filter"] = k;

            } break;
            case NODE_MIX: {
                MixNode *mn = static_cast<MixNode *>(n);
                node["mix"] = mn->amount;
            } break;
            case NODE_BLEND2: {
                Blend2Node *bn = static_cast<Blend2Node *>(n);
                node["blend"] = bn->value;
                Array k;
                Vector<NodePath> keys;
                bn->filter.keys_into(keys);
                k.resize(keys.size());
                int i = 0;
                for (const NodePath &F : keys) {
                    k[i++] = F;
                }
                node["filter"] = k;

            } break;
            case NODE_BLEND3: {
                Blend3Node *bn = static_cast<Blend3Node *>(n);
                node["blend"] = bn->value;
            } break;
            case NODE_BLEND4: {
                Blend4Node *bn = static_cast<Blend4Node *>(n);
                node["blend"] = bn->value;

            } break;
            case NODE_TIMESCALE: {
                TimeScaleNode *tsn = static_cast<TimeScaleNode *>(n);
                node["scale"] = tsn->scale;
            } break;
            case NODE_TIMESEEK: {
            } break;
            case NODE_TRANSITION: {

                TransitionNode *tn = static_cast<TransitionNode *>(n);
                node["xfade"] = tn->xfade;
                Array transitions;

                for (int i = 0; i < tn->input_data.size(); i++) {

                    Dictionary d;
                    d["auto_advance"] = tn->input_data[i].auto_advance;
                    transitions.push_back(d);
                }

                node["transitions"] = transitions;

            } break;
            default: {
            };
        }

        nodes.push_back(node);
    }

    data["nodes"] = nodes;
    //connectiosn

    Vector<Connection> connections(get_connection_list());
    Array connections_arr;
    connections_arr.resize(connections.size() * 3);

    int idx = 0;
    for (const Connection &E : connections) {

        connections_arr.set(idx + 0, E.src_node);
        connections_arr.set(idx + 1, E.dst_node);
        connections_arr.set(idx + 2, E.dst_input);

        idx += 3;
    }

    data["connections"] = connections_arr;
    data["active"] = active;
    data["master"] = master;

    r_ret = data;

    return true;
}

void AnimationTreePlayer::_get_property_list(Vector<PropertyInfo> *p_list) const {

    p_list->emplace_back(VariantType::DICTIONARY, "data", PropertyHint::None, "", PROPERTY_USAGE_STORAGE);// | PROPERTY_USAGE_NETWORK
}

void AnimationTreePlayer::advance(float p_time) {

    _process_animation(p_time);
}

void AnimationTreePlayer::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {

            WARN_DEPRECATED_MSG("AnimationTreePlayer has been deprecated. Use AnimationTree instead.");

            if (!processing) {
                //make sure that a previous process state was not saved
                //only process if "processing" is set
                set_physics_process_internal(false);
                set_process_internal(false);
            }
        } break;
        case NOTIFICATION_READY: {

            dirty_caches = true;
            if (master != NodePath()) {
                _update_sources();
            }
        } break;
        case NOTIFICATION_INTERNAL_PROCESS: {

            if (animation_process_mode == ANIMATION_PROCESS_PHYSICS)
                break;

            if (processing && OS::get_singleton()->is_update_pending()) {
                _process_animation(get_process_delta_time());
            }
        } break;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {

            if (animation_process_mode == ANIMATION_PROCESS_IDLE)
                break;

            if (processing && OS::get_singleton()->is_update_pending()) {
                _process_animation(get_physics_process_delta_time());
            }
        } break;
    }
}

void AnimationTreePlayer::_compute_weights(float *p_fallback_weight, HashMap<NodePath, float> *p_weights, float p_coeff, const HashMap<NodePath, bool> *p_filter, float p_filtered_coeff) {

    if (p_filter != nullptr) {

        FixedVector<NodePath,32,true> key_list;
        p_filter->keys_into(key_list);

        for (const NodePath &E : key_list) {

            if ((*p_filter).at(E,false)) {

                if (p_weights->contains(E)) {
                    (*p_weights)[E] *= p_filtered_coeff;
                } else {
                    (*p_weights)[E] = *p_fallback_weight * p_filtered_coeff;
                }

            } else if (p_weights->contains(E)) {
                (*p_weights)[E] *= p_coeff;
            }
        }
    }

    FixedVector<NodePath,32,true> key_list;
    p_weights->keys_into(key_list);

    for (const NodePath &E : key_list) {
        if (p_filter == nullptr || !p_filter->contains(E)) {
            (*p_weights)[E] *= p_coeff;
        }
    }

    *p_fallback_weight *= p_coeff;
}

float AnimationTreePlayer::_process_node(const StringName &p_node, AnimationTreeNode **r_prev_anim, float p_time, bool p_seek, float p_fallback_weight, HashMap<NodePath, float> *p_weights) {

    ERR_FAIL_COND_V(!node_map.contains(p_node), 0);
    AnimationTreeNodeBase *nb = node_map[p_node];

    //transform to seconds...

    switch (nb->type) {

        case NODE_OUTPUT: {

            AnimationTreeNodeOut *on = static_cast<AnimationTreeNodeOut *>(nb);
            HashMap<NodePath, float> weights;

            return _process_node(on->inputs[0].node, r_prev_anim, p_time, p_seek, p_fallback_weight, &weights);

        } break;
        case NODE_ANIMATION: {

            AnimationTreeNode *an = static_cast<AnimationTreeNode *>(nb);

            float rem = 0;
            if (an->animation) {

                //float pos = an->time;
                //float delta = p_time;

                //const Animation *a = an->animation.operator->();

                if (p_seek) {
                    an->time = p_time;
                    an->step = 0;
                } else {
                    an->time = M_MAX(0, an->time + p_time);
                    an->step = p_time;
                }

                float anim_size = an->animation->get_length();

                if (an->animation->has_loop()) {

                    if (anim_size)
                        an->time = Math::fposmod(an->time, anim_size);

                } else if (an->time > anim_size) {

                    an->time = anim_size;
                }

                an->skip = true;

                for (AnimationTreeNode::TrackRef &E : an->tref) {
                    NodePath track_path = an->animation->track_get_path(E.local_track);
                    if (an->filter.contains(track_path) && an->filter[track_path]) {
                        E.weight = 0;
                    } else {
                        if (p_weights->contains(track_path)) {
                            float weight = (*p_weights)[track_path];
                            E.weight = weight;
                        } else {
                            E.weight = p_fallback_weight;
                        }
                    }
                    if (E.weight > CMP_EPSILON)
                        an->skip = false;
                }

                rem = anim_size - an->time;
            }

            if (!(*r_prev_anim))
                active_list = an;
            else
                (*r_prev_anim)->next = an;

            an->next = nullptr;
            *r_prev_anim = an;

            return rem;

        } break;
        case NODE_ONESHOT: {

            OneShotNode *osn = static_cast<OneShotNode *>(nb);

            if (!osn->active) {
                //make it as if this node doesn't exist, pass input 0 by.
                return _process_node(osn->inputs[0].node, r_prev_anim, p_time, p_seek, p_fallback_weight, p_weights);
            }

            bool os_seek = p_seek;

            if (p_seek)
                osn->time = p_time;
            if (osn->start) {
                osn->time = 0;
                os_seek = true;
            }

            float blend;

            if (osn->time < osn->fade_in) {

                if (osn->fade_in > 0)
                    blend = osn->time / osn->fade_in;
                else
                    blend = 0; //wtf

            } else if (!osn->start && osn->remaining < osn->fade_out) {

                if (osn->fade_out)
                    blend = (osn->remaining / osn->fade_out);
                else
                    blend = 1.0;
            } else
                blend = 1.0;

            float main_rem;
            float os_rem;

            HashMap<NodePath, float> os_weights(*p_weights);
            float os_fallback_weight = p_fallback_weight;
            _compute_weights(&p_fallback_weight, p_weights, osn->mix ? 1.0 : 1.0 - blend, &osn->filter, 1.0);
            _compute_weights(&os_fallback_weight, &os_weights, blend, &osn->filter, 0.0);

            main_rem = _process_node(osn->inputs[0].node, r_prev_anim, p_time, p_seek, p_fallback_weight, p_weights);
            os_rem = _process_node(osn->inputs[1].node, r_prev_anim, p_time, os_seek, os_fallback_weight, &os_weights);

            if (osn->start) {
                osn->remaining = os_rem;
                osn->start = false;
            }

            if (!p_seek) {
                osn->time += p_time;
                osn->remaining = os_rem;
                if (osn->remaining <= 0)
                    osn->active = false;
            }

            return M_MAX(main_rem, osn->remaining);
        } break;
        case NODE_MIX: {
            MixNode *mn = static_cast<MixNode *>(nb);

            HashMap<NodePath, float> mn_weights(*p_weights);
            float mn_fallback_weight = p_fallback_weight;
            _compute_weights(&mn_fallback_weight, &mn_weights, mn->amount);
            float rem = _process_node(mn->inputs[0].node, r_prev_anim, p_time, p_seek, p_fallback_weight, p_weights);
            _process_node(mn->inputs[1].node, r_prev_anim, p_time, p_seek, mn_fallback_weight, &mn_weights);
            return rem;

        } break;
        case NODE_BLEND2: {

            Blend2Node *bn = static_cast<Blend2Node *>(nb);

            HashMap<NodePath, float> bn_weights(*p_weights);
            float bn_fallback_weight = p_fallback_weight;
            _compute_weights(&p_fallback_weight, p_weights, 1.0 - bn->value, &bn->filter, 1.0);
            _compute_weights(&bn_fallback_weight, &bn_weights, bn->value, &bn->filter, 0.0);
            float rem = _process_node(bn->inputs[0].node, r_prev_anim, p_time, p_seek, p_fallback_weight, p_weights);
            _process_node(bn->inputs[1].node, r_prev_anim, p_time, p_seek, bn_fallback_weight, &bn_weights);

            return rem;
        } break;
        case NODE_BLEND3: {
            Blend3Node *bn = static_cast<Blend3Node *>(nb);

            float rem;
            float blend, lower_blend, upper_blend;
            if (bn->value < 0) {
                lower_blend = -bn->value;
                blend = 1.0 - lower_blend;
                upper_blend = 0;
            } else {
                lower_blend = 0;
                blend = 1.0 - bn->value;
                upper_blend = bn->value;
            }

            HashMap<NodePath, float> upper_weights(*p_weights);
            float upper_fallback_weight = p_fallback_weight;
            HashMap<NodePath, float> lower_weights(*p_weights);
            float lower_fallback_weight = p_fallback_weight;
            _compute_weights(&upper_fallback_weight, &upper_weights, upper_blend);
            _compute_weights(&p_fallback_weight, p_weights, blend);
            _compute_weights(&lower_fallback_weight, &lower_weights, lower_blend);

            rem = _process_node(bn->inputs[1].node, r_prev_anim, p_time, p_seek, p_fallback_weight, p_weights);
            _process_node(bn->inputs[0].node, r_prev_anim, p_time, p_seek, lower_fallback_weight, &lower_weights);
            _process_node(bn->inputs[2].node, r_prev_anim, p_time, p_seek, upper_fallback_weight, &upper_weights);

            return rem;
        } break;
        case NODE_BLEND4: {
            Blend4Node *bn = static_cast<Blend4Node *>(nb);

            HashMap<NodePath, float> weights1(*p_weights);
            float fallback_weight1 = p_fallback_weight;
            HashMap<NodePath, float> weights2(*p_weights);
            float fallback_weight2 = p_fallback_weight;
            HashMap<NodePath, float> weights3(*p_weights);
            float fallback_weight3 = p_fallback_weight;

            _compute_weights(&p_fallback_weight, p_weights, 1.0 - bn->value.x);
            _compute_weights(&fallback_weight1, &weights1, bn->value.x);
            _compute_weights(&fallback_weight2, &weights2, 1.0 - bn->value.y);
            _compute_weights(&fallback_weight3, &weights3, bn->value.y);

            float rem = _process_node(bn->inputs[0].node, r_prev_anim, p_time, p_seek, p_fallback_weight, p_weights);
            _process_node(bn->inputs[1].node, r_prev_anim, p_time, p_seek, fallback_weight1, &weights1);
            float rem2 = _process_node(bn->inputs[2].node, r_prev_anim, p_time, p_seek, fallback_weight2, &weights2);
            _process_node(bn->inputs[3].node, r_prev_anim, p_time, p_seek, fallback_weight3, &weights3);

            return M_MAX(rem, rem2);

        } break;
        case NODE_TIMESCALE: {
            TimeScaleNode *tsn = static_cast<TimeScaleNode *>(nb);
            float rem;
            if (p_seek)
                rem = _process_node(tsn->inputs[0].node, r_prev_anim, p_time, true, p_fallback_weight, p_weights);
            else
                rem = _process_node(tsn->inputs[0].node, r_prev_anim, p_time * tsn->scale, false, p_fallback_weight, p_weights);
            if (tsn->scale == 0)
                return Math_INF;
            else
                return rem / tsn->scale;

        } break;
        case NODE_TIMESEEK: {

            TimeSeekNode *tsn = static_cast<TimeSeekNode *>(nb);
            if (tsn->seek_pos >= 0 && !p_seek) {

                p_time = tsn->seek_pos;
                p_seek = true;
            }
            tsn->seek_pos = -1;

            return _process_node(tsn->inputs[0].node, r_prev_anim, p_time, p_seek, p_fallback_weight, p_weights);

        } break;
        case NODE_TRANSITION: {

            TransitionNode *tn = static_cast<TransitionNode *>(nb);
            HashMap<NodePath, float> prev_weights(*p_weights);
            float prev_fallback_weight = p_fallback_weight;

            if (tn->prev < 0) { // process current animation, check for transition

                float rem = _process_node(tn->inputs[tn->current].node, r_prev_anim, p_time, p_seek, p_fallback_weight, p_weights);
                if (p_seek)
                    tn->time = p_time;
                else
                    tn->time += p_time;

                if (tn->input_data[tn->current].auto_advance && rem <= tn->xfade) {

                    tn->set_current((tn->current + 1) % tn->inputs.size());
                }

                return rem;
            } else { // cross-fading from tn->prev to tn->current

                float blend = tn->xfade ? (tn->prev_xfading / tn->xfade) : 1;

                float rem;

                _compute_weights(&p_fallback_weight, p_weights, 1.0 - blend);
                _compute_weights(&prev_fallback_weight, &prev_weights, blend);

                if (!p_seek && tn->switched) { //just switched, seek to start of current

                    rem = _process_node(tn->inputs[tn->current].node, r_prev_anim, 0, true, p_fallback_weight, p_weights);
                } else {

                    rem = _process_node(tn->inputs[tn->current].node, r_prev_anim, p_time, p_seek, p_fallback_weight, p_weights);
                }

                tn->switched = false;

                if (p_seek) { // don't seek prev animation
                    _process_node(tn->inputs[tn->prev].node, r_prev_anim, 0, false, prev_fallback_weight, &prev_weights);
                    tn->time = p_time;
                } else {
                    _process_node(tn->inputs[tn->prev].node, r_prev_anim, p_time, false, prev_fallback_weight, &prev_weights);
                    tn->time += p_time;
                    tn->prev_xfading -= p_time;
                    if (tn->prev_xfading < 0) {

                        tn->prev = -1;
                    }
                }

                return rem;
            }

        } break;
        default: {
        }
    }

    return 0;
}

void AnimationTreePlayer::_process_animation(float p_delta) {

    if (last_error != CONNECT_OK)
        return;

    if (dirty_caches)
        _recompute_caches();

    active_list = nullptr;
    AnimationTreeNode *prev = nullptr;

    if (reset_request) {

        _process_node(out_name, &prev, 0, true);
        reset_request = false;
    } else
        _process_node(out_name, &prev, p_delta);

    if (dirty_caches) {
        //some animation changed.. ignore this pass
        return;
    }

    //update the tracks..

    /* STEP 1 CLEAR TRACKS */

    for (auto &E : track_map) {

        Track &t = E.second;

        t.loc = {0,0,0};
        t.rot = Quat();
        t.scale = {0,0,0};

        t.value = t.object->get_indexed(t.subpath);
        t.value.zero();

        t.skip = false;
    }

    /* STEP 2 PROCESS ANIMATIONS */

    AnimationTreeNode *anim_list = active_list;
    Quat empty_rot;

    while (anim_list) {

        if (anim_list->animation && !anim_list->skip) {
            //check if animation is meaningful
            Animation *a = anim_list->animation.operator->();

            for (AnimationTreeNode::TrackRef &tr : anim_list->tref) {

                if (tr.track == nullptr || tr.local_track < 0 || tr.weight < CMP_EPSILON || !a->track_is_enabled(tr.local_track))
                    continue;

                switch (a->track_get_type(tr.local_track)) {
                    case Animation::TYPE_TRANSFORM: { ///< Transform a node or a bone.

                        Vector3 loc;
                        Quat rot;
                        Vector3 scale;
                        a->transform_track_interpolate(tr.local_track, anim_list->time, &loc, &rot, &scale);

                        tr.track->loc += loc * tr.weight;

                        scale.x -= 1.0f;
                        scale.y -= 1.0f;
                        scale.z -= 1.0f;
                        tr.track->scale += scale * tr.weight;

                        tr.track->rot = tr.track->rot * empty_rot.slerp(rot, tr.weight);

                    } break;
                    case Animation::TYPE_VALUE: { ///< Set a value in a property, can be interpolated.

                        if (a->value_track_get_update_mode(tr.local_track) == Animation::UPDATE_CONTINUOUS) {
                            Variant value = a->value_track_interpolate(tr.local_track, anim_list->time);
                            Variant::blend(tr.track->value, value, tr.weight, tr.track->value);
                        } else {
                            int index = a->track_find_key(tr.local_track, anim_list->time);
                            tr.track->value = a->track_get_key_value(tr.local_track, index);
                        }
                    } break;
                    case Animation::TYPE_METHOD: { ///< Call any method on a specific node.

                        Vector<int> indices;
                        a->method_track_get_key_indices(tr.local_track, anim_list->time, anim_list->step, &indices);
                        for (int F : indices) {

                            StringName method = a->method_track_get_name(tr.local_track, F);
                            Vector<Variant> args = a->method_track_get_params(tr.local_track, F);
                            args.resize(VARIANT_ARG_MAX);
                            tr.track->object->call_va(method, args[0], args[1], args[2], args[3], args[4]);
                        }
                    } break;
                    default: {
                    }
                }
            }
        }

        anim_list = anim_list->next;
    }

    /* STEP 3 APPLY TRACKS */

    for (auto &E : track_map) {

        Track &t = E.second;

        if (t.skip || !t.object)
            continue;

        if (!t.subpath.empty()) { // value track
            t.object->set_indexed(t.subpath, t.value);
            continue;
        }

        Transform xform;
        xform.origin = t.loc;

        t.scale.x += 1.0f;
        t.scale.y += 1.0f;
        t.scale.z += 1.0f;
        xform.basis.set_quat_scale(t.rot, t.scale);

        if (t.bone_idx >= 0) {
            if (t.skeleton)
                t.skeleton->set_bone_pose(t.bone_idx, xform);

        } else if (t.node_3d) {

            t.node_3d->set_transform(xform);
        }
    }
}

void AnimationTreePlayer::add_node(NodeType p_type, const StringName &p_node) {

    ERR_FAIL_COND(p_type == NODE_OUTPUT);
    ERR_FAIL_COND(node_map.contains(p_node));
    ERR_FAIL_INDEX(p_type, NODE_MAX);

    AnimationTreeNodeBase *n = nullptr;

    switch (p_type) {

        case NODE_ANIMATION: {

            n = memnew(AnimationTreeNode);
        } break;
        case NODE_ONESHOT: {

            n = memnew(OneShotNode);

        } break;
        case NODE_MIX: {
            n = memnew(MixNode);

        } break;
        case NODE_BLEND2: {
            n = memnew(Blend2Node);

        } break;
        case NODE_BLEND3: {
            n = memnew(Blend3Node);

        } break;
        case NODE_BLEND4: {
            n = memnew(Blend4Node);

        } break;
        case NODE_TIMESCALE: {
            n = memnew(TimeScaleNode);

        } break;
        case NODE_TIMESEEK: {
            n = memnew(TimeSeekNode);

        } break;
        case NODE_TRANSITION: {
            n = memnew(TransitionNode);

        } break;
        default: {
        }
    }

    //n->name+=" "+itos(p_node);
    node_map[p_node] = n;
}

StringName AnimationTreePlayer::node_get_input_source(const StringName &p_node, int p_input) const {

    ERR_FAIL_COND_V(!node_map.contains(p_node), StringName());
    ERR_FAIL_INDEX_V(p_input, node_map.at(p_node)->inputs.size(), StringName());
    return node_map.at(p_node)->inputs[p_input].node;
}

int AnimationTreePlayer::node_get_input_count(const StringName &p_node) const {

    ERR_FAIL_COND_V(!node_map.contains(p_node), -1);
    return node_map.at(p_node)->inputs.size();
}
#define GET_NODE(m_type, m_cast)                                                             \
    ERR_FAIL_COND(!node_map.contains(p_node));                                                    \
    ERR_FAIL_COND_MSG(node_map.at(p_node)->type != m_type, "Invalid parameter for node type."); \
    m_cast *n = static_cast<m_cast *>(node_map.at(p_node));

void AnimationTreePlayer::animation_node_set_animation(const StringName &p_node, const Ref<Animation> &p_animation) {

    GET_NODE(NODE_ANIMATION, AnimationTreeNode);
    n->animation = p_animation;
    dirty_caches = true;
}

void AnimationTreePlayer::animation_node_set_master_animation(const StringName &p_node, StringView p_master_animation) {

    GET_NODE(NODE_ANIMATION, AnimationTreeNode);
    n->from = p_master_animation;
    dirty_caches = true;
    if (master.empty()) {
        _update_sources();
    }
}

void AnimationTreePlayer::animation_node_set_filter_path(
        const StringName &p_node, const NodePath &p_track_path, bool p_filter) {
    GET_NODE(NODE_ANIMATION, AnimationTreeNode);

    if (p_filter) {
        n->filter[p_track_path] = true;
    } else {
        n->filter.erase(p_track_path);
    }
}

void AnimationTreePlayer::animation_node_set_get_filtered_paths(const StringName &p_node, Vector<NodePath> *r_paths) const {

    GET_NODE(NODE_ANIMATION, AnimationTreeNode)

    n->filter.keys_into(*r_paths);
}

void AnimationTreePlayer::oneshot_node_set_fadein_time(const StringName &p_node, float p_time) {

    GET_NODE(NODE_ONESHOT, OneShotNode)
    n->fade_in = p_time;
}

void AnimationTreePlayer::oneshot_node_set_fadeout_time(const StringName &p_node, float p_time) {

    GET_NODE(NODE_ONESHOT, OneShotNode);
    n->fade_out = p_time;
}

void AnimationTreePlayer::oneshot_node_set_mix_mode(const StringName &p_node, bool p_mix) {

    GET_NODE(NODE_ONESHOT, OneShotNode);
    n->mix = p_mix;
}

void AnimationTreePlayer::oneshot_node_set_autorestart(const StringName &p_node, bool p_active) {

    GET_NODE(NODE_ONESHOT, OneShotNode);
    n->autorestart = p_active;
}

void AnimationTreePlayer::oneshot_node_set_autorestart_delay(const StringName &p_node, float p_time) {

    GET_NODE(NODE_ONESHOT, OneShotNode);
    n->autorestart_delay = p_time;
}
void AnimationTreePlayer::oneshot_node_set_autorestart_random_delay(const StringName &p_node, float p_time) {

    GET_NODE(NODE_ONESHOT, OneShotNode);
    n->autorestart_random_delay = p_time;
}

void AnimationTreePlayer::oneshot_node_start(const StringName &p_node) {

    GET_NODE(NODE_ONESHOT, OneShotNode);
    n->active = true;
    n->start = true;
}

void AnimationTreePlayer::oneshot_node_stop(const StringName &p_node) {

    GET_NODE(NODE_ONESHOT, OneShotNode);
    n->active = false;
}

void AnimationTreePlayer::oneshot_node_set_filter_path(const StringName &p_node, const NodePath &p_filter, bool p_enable) {

    GET_NODE(NODE_ONESHOT, OneShotNode);

    if (p_enable)
        n->filter[p_filter] = true;
    else
        n->filter.erase(p_filter);
}

void AnimationTreePlayer::oneshot_node_set_get_filtered_paths(const StringName &p_node, Vector<NodePath> *r_paths) const {

    GET_NODE(NODE_ONESHOT, OneShotNode)

    n->filter.keys_into(*r_paths);
}

void AnimationTreePlayer::mix_node_set_amount(const StringName &p_node, float p_amount) {

    GET_NODE(NODE_MIX, MixNode)
    n->amount = p_amount;
}

void AnimationTreePlayer::blend2_node_set_amount(const StringName &p_node, float p_amount) {

    GET_NODE(NODE_BLEND2, Blend2Node);
    n->value = p_amount;
}

void AnimationTreePlayer::blend2_node_set_filter_path(const StringName &p_node, const NodePath &p_filter, bool p_enable) {

    GET_NODE(NODE_BLEND2, Blend2Node);

    if (p_enable)
        n->filter[p_filter] = true;
    else
        n->filter.erase(p_filter);
}

void AnimationTreePlayer::blend2_node_set_get_filtered_paths(const StringName &p_node, Vector<NodePath> *r_paths) const {

    GET_NODE(NODE_BLEND2, Blend2Node)

    n->filter.keys_into(*r_paths);
}

void AnimationTreePlayer::blend3_node_set_amount(const StringName &p_node, float p_amount) {

    GET_NODE(NODE_BLEND3, Blend3Node)
    n->value = p_amount;
}
void AnimationTreePlayer::blend4_node_set_amount(const StringName &p_node, const Vector2 &p_amount) {

    GET_NODE(NODE_BLEND4, Blend4Node);
    n->value = p_amount;
}
void AnimationTreePlayer::timescale_node_set_scale(const StringName &p_node, float p_scale) {

    GET_NODE(NODE_TIMESCALE, TimeScaleNode);
    n->scale = p_scale;
}
void AnimationTreePlayer::timeseek_node_seek(const StringName &p_node, float p_pos) {

    GET_NODE(NODE_TIMESEEK, TimeSeekNode);
    n->seek_pos = p_pos;
}
void AnimationTreePlayer::transition_node_set_input_count(const StringName &p_node, int p_inputs) {

    GET_NODE(NODE_TRANSITION, TransitionNode);
    ERR_FAIL_COND(p_inputs < 1);

    n->inputs.resize(p_inputs);
    n->input_data.resize(p_inputs);

    _clear_cycle_test();

    last_error = _cycle_test(out_name);
}
void AnimationTreePlayer::transition_node_set_input_auto_advance(const StringName &p_node, int p_input, bool p_auto_advance) {

    GET_NODE(NODE_TRANSITION, TransitionNode);
    ERR_FAIL_INDEX(p_input, n->input_data.size());

    n->input_data[p_input].auto_advance = p_auto_advance;
}
void AnimationTreePlayer::transition_node_set_xfade_time(const StringName &p_node, float p_time) {

    GET_NODE(NODE_TRANSITION, TransitionNode);
    n->xfade = p_time;
}



void AnimationTreePlayer::transition_node_set_current(const StringName &p_node, int p_current) {

    GET_NODE(NODE_TRANSITION, TransitionNode);
    n->set_current(p_current);
}

void AnimationTreePlayer::node_set_position(const StringName &p_node, const Vector2 &p_pos) {

    ERR_FAIL_COND(!node_map.contains(p_node));
    node_map[p_node]->pos = p_pos;
}

AnimationTreePlayer::NodeType AnimationTreePlayer::node_get_type(const StringName &p_node) const {

    ERR_FAIL_COND_V(!node_map.contains(p_node), NODE_OUTPUT);
    return node_map.at(p_node)->type;
}
Point2 AnimationTreePlayer::node_get_position(const StringName &p_node) const {

    ERR_FAIL_COND_V(!node_map.contains(p_node), Point2());
    return node_map.at(p_node)->pos;
}

#define GET_NODE_V(m_type, m_cast, m_ret)                                                             \
    ERR_FAIL_COND_V(!node_map.contains(p_node), m_ret);                                                    \
    ERR_FAIL_COND_V_MSG(node_map.at(p_node)->type != m_type, m_ret, "Invalid parameter for node type."); \
    m_cast *n = static_cast<m_cast *>(node_map.at(p_node));

Ref<Animation> AnimationTreePlayer::animation_node_get_animation(const StringName &p_node) const {

    GET_NODE_V(NODE_ANIMATION, AnimationTreeNode, Ref<Animation>());
    return n->animation;
}

const String & AnimationTreePlayer::animation_node_get_master_animation(const StringName &p_node) const {

    GET_NODE_V(NODE_ANIMATION, AnimationTreeNode, null_string);
    return n->from;
}

float AnimationTreePlayer::animation_node_get_position(const StringName &p_node) const {

    GET_NODE_V(NODE_ANIMATION, AnimationTreeNode, 0);
    return n->time;
}

bool AnimationTreePlayer::animation_node_is_path_filtered(const StringName &p_node, const NodePath &p_path) const {

    GET_NODE_V(NODE_ANIMATION, AnimationTreeNode, 0);
    return n->filter.contains(p_path);
}

float AnimationTreePlayer::oneshot_node_get_fadein_time(const StringName &p_node) const {

    GET_NODE_V(NODE_ONESHOT, OneShotNode, 0);
    return n->fade_in;
}

float AnimationTreePlayer::oneshot_node_get_fadeout_time(const StringName &p_node) const {

    GET_NODE_V(NODE_ONESHOT, OneShotNode, 0);
    return n->fade_out;
}

bool AnimationTreePlayer::oneshot_node_get_mix_mode(const StringName &p_node) const {

    GET_NODE_V(NODE_ONESHOT, OneShotNode, 0);
    return n->mix;
}
bool AnimationTreePlayer::oneshot_node_has_autorestart(const StringName &p_node) const {

    GET_NODE_V(NODE_ONESHOT, OneShotNode, 0);
    return n->autorestart;
}
float AnimationTreePlayer::oneshot_node_get_autorestart_delay(const StringName &p_node) const {

    GET_NODE_V(NODE_ONESHOT, OneShotNode, 0);
    return n->autorestart_delay;
}
float AnimationTreePlayer::oneshot_node_get_autorestart_random_delay(const StringName &p_node) const {

    GET_NODE_V(NODE_ONESHOT, OneShotNode, 0);
    return n->autorestart_random_delay;
}

bool AnimationTreePlayer::oneshot_node_is_active(const StringName &p_node) const {

    GET_NODE_V(NODE_ONESHOT, OneShotNode, 0);
    return n->active;
}

bool AnimationTreePlayer::oneshot_node_is_path_filtered(const StringName &p_node, const NodePath &p_path) const {

    GET_NODE_V(NODE_ONESHOT, OneShotNode, 0);
    return n->filter.contains(p_path);
}

float AnimationTreePlayer::mix_node_get_amount(const StringName &p_node) const {

    GET_NODE_V(NODE_MIX, MixNode, 0);
    return n->amount;
}
float AnimationTreePlayer::blend2_node_get_amount(const StringName &p_node) const {

    GET_NODE_V(NODE_BLEND2, Blend2Node, 0);
    return n->value;
}

bool AnimationTreePlayer::blend2_node_is_path_filtered(const StringName &p_node, const NodePath &p_path) const {

    GET_NODE_V(NODE_BLEND2, Blend2Node, 0);
    return n->filter.contains(p_path);
}

float AnimationTreePlayer::blend3_node_get_amount(const StringName &p_node) const {

    GET_NODE_V(NODE_BLEND3, Blend3Node, 0);
    return n->value;
}
Vector2 AnimationTreePlayer::blend4_node_get_amount(const StringName &p_node) const {

    GET_NODE_V(NODE_BLEND4, Blend4Node, Vector2());
    return n->value;
}

float AnimationTreePlayer::timescale_node_get_scale(const StringName &p_node) const {

    GET_NODE_V(NODE_TIMESCALE, TimeScaleNode, 0);
    return n->scale;
}

void AnimationTreePlayer::transition_node_delete_input(const StringName &p_node, int p_input) {

    GET_NODE(NODE_TRANSITION, TransitionNode);
    ERR_FAIL_INDEX(p_input, n->inputs.size());

    if (n->inputs.size() <= 1)
        return;

    n->inputs.erase_at(p_input);
    n->input_data.erase_at(p_input);
    last_error = _cycle_test(out_name);
}

int AnimationTreePlayer::transition_node_get_input_count(const StringName &p_node) const {

    GET_NODE_V(NODE_TRANSITION, TransitionNode, 0);
    return n->inputs.size();
}

bool AnimationTreePlayer::transition_node_has_input_auto_advance(const StringName &p_node, int p_input) const {

    GET_NODE_V(NODE_TRANSITION, TransitionNode, false);
    ERR_FAIL_INDEX_V(p_input, n->inputs.size(), false);
    return n->input_data[p_input].auto_advance;
}
float AnimationTreePlayer::transition_node_get_xfade_time(const StringName &p_node) const {

    GET_NODE_V(NODE_TRANSITION, TransitionNode, 0);
    return n->xfade;
}

int AnimationTreePlayer::transition_node_get_current(const StringName &p_node) const {

    GET_NODE_V(NODE_TRANSITION, TransitionNode, -1);
    return n->current;
}

/*misc  */
void AnimationTreePlayer::get_node_list(List<StringName> *p_node_list) const {

    for (const eastl::pair<const StringName,AnimationTreeNodeBase *> &E : node_map) {

        p_node_list->push_back(E.first);
    }
}
Vector<StringName> AnimationTreePlayer::get_node_vector() const {
    Vector<StringName> res;
    res.reserve(node_map.size());
    for (const eastl::pair<const StringName, AnimationTreeNodeBase*>& E : node_map) {

        res.emplace_back(E.first);
    }
    return res;
}
void AnimationTreePlayer::remove_node(const StringName &p_node) {

    ERR_FAIL_COND(!node_map.contains(p_node));
    ERR_FAIL_COND_MSG(p_node == out_name, "Node 0 (output) can't be removed.");

    for (eastl::pair<const StringName,AnimationTreeNodeBase *> &E : node_map) {

        AnimationTreeNodeBase *nb = E.second;
        for (int i = 0; i < nb->inputs.size(); i++) {

            if (nb->inputs[i].node == p_node)
                nb->inputs[i].node = StringName();
        }
    }

    memdelete(node_map[p_node]);
    node_map.erase(p_node);

    _clear_cycle_test();

    // compute last error again, just in case
    last_error = _cycle_test(out_name);
    dirty_caches = true;
}

AnimationTreePlayer::ConnectError AnimationTreePlayer::_cycle_test(const StringName &p_at_node) {

    ERR_FAIL_COND_V(!node_map.contains(p_at_node), CONNECT_INCOMPLETE);

    AnimationTreeNodeBase *nb = node_map[p_at_node];
    if (nb->cycletest)
        return CONNECT_CYCLE;

    nb->cycletest = true;

    for (int i = 0; i < nb->inputs.size(); i++) {
        if (nb->inputs[i].node == StringName())
            return CONNECT_INCOMPLETE;

        ConnectError _err = _cycle_test(nb->inputs[i].node);
        if (_err)
            return _err;
    }

    return CONNECT_OK;
}

// Use this function to not alter next complete _cycle_test().
void AnimationTreePlayer::_clear_cycle_test() {
    for (eastl::pair<const StringName,AnimationTreeNodeBase *> &E : node_map) {
        AnimationTreeNodeBase *nb = E.second;
        nb->cycletest = false;
    }
}

Error AnimationTreePlayer::connect_nodes(const StringName &p_src_node, const StringName &p_dst_node, int p_dst_input) {

    ERR_FAIL_COND_V(!node_map.contains(p_src_node), ERR_INVALID_PARAMETER);
    ERR_FAIL_COND_V(!node_map.contains(p_dst_node), ERR_INVALID_PARAMETER);
    ERR_FAIL_COND_V(p_src_node == p_dst_node, ERR_INVALID_PARAMETER);

    //NodeBase *src = node_map[p_src_node];
    AnimationTreeNodeBase *dst = node_map[p_dst_node];
    ERR_FAIL_INDEX_V(p_dst_input, dst->inputs.size(), ERR_INVALID_PARAMETER);

    //int oldval = dst->inputs[p_dst_input].node;

    for (eastl::pair<const StringName,AnimationTreeNodeBase *> &E : node_map) {

        AnimationTreeNodeBase *nb = E.second;
        for (int i = 0; i < nb->inputs.size(); i++) {

            if (nb->inputs[i].node == p_src_node)
                nb->inputs[i].node = StringName();
        }
    }

    dst->inputs[p_dst_input].node = p_src_node;

    _clear_cycle_test();

    last_error = _cycle_test(out_name);
    if (last_error) {

        if (last_error == CONNECT_INCOMPLETE)
            return ERR_UNCONFIGURED;
        else if (last_error == CONNECT_CYCLE)
            return ERR_CYCLIC_LINK;
    }
    dirty_caches = true;
    return OK;
}

bool AnimationTreePlayer::are_nodes_connected(const StringName &p_src_node, const StringName &p_dst_node, int p_dst_input) const {

    ERR_FAIL_COND_V(!node_map.contains(p_src_node), false);
    ERR_FAIL_COND_V(!node_map.contains(p_dst_node), false);
    ERR_FAIL_COND_V(p_src_node == p_dst_node, false);

    AnimationTreeNodeBase *dst = node_map.at(p_dst_node);

    return dst->inputs[p_dst_input].node == p_src_node;
}

void AnimationTreePlayer::disconnect_nodes(const StringName &p_node, int p_input) {

    ERR_FAIL_COND(!node_map.contains(p_node));

    AnimationTreeNodeBase *dst = node_map[p_node];
    ERR_FAIL_INDEX(p_input, dst->inputs.size());
    dst->inputs[p_input].node = StringName();
    last_error = CONNECT_INCOMPLETE;
    dirty_caches = true;
}

Vector<AnimationTreePlayer::Connection> AnimationTreePlayer::get_connection_list() const {
    Vector<Connection> res;
    res.reserve(node_map.size());

    for (const eastl::pair<const StringName,AnimationTreeNodeBase *> &E : node_map) {

        AnimationTreeNodeBase *nb = E.second;
        for (int i = 0; i < nb->inputs.size(); i++) {

            if (nb->inputs[i].node.empty())
                continue;

            Connection c;
            c.src_node = nb->inputs[i].node;
            c.dst_node = E.first;
            c.dst_input = i;
            res.emplace_back(c);
        }
    }
    return res;
}

AnimationTreePlayer::Track *AnimationTreePlayer::_find_track(const NodePath &p_path) {

    Node *parent = get_node(base_path);
    ERR_FAIL_COND_V(!parent, nullptr);

    RES resource;
    Vector<StringName> leftover_path;
    Node *child = parent->get_node_and_resource(p_path, resource, leftover_path);
    if (!child) {
        String err = "Animation track references unknown Node: '" + String(p_path) + "'.";
        WARN_PRINT(err);
        return nullptr;
    }

    GameEntity id = child->get_instance_id();
    int bone_idx = -1;

    if (p_path.get_subname_count()) {

        if (object_cast<Skeleton>(child))
            bone_idx = object_cast<Skeleton>(child)->find_bone(p_path.get_subname(0));
    }

    TrackKey key;
    key.id = id;
    key.bone_idx = bone_idx;
    key.subpath_concatenated = p_path.get_concatenated_subnames();

    if (!track_map.contains(key)) {

        Track tr;
        tr.id = id;
        tr.object = resource ? (Object *)resource.get() : (Object *)child;
        tr.skeleton = object_cast<Skeleton>(child);
        tr.node_3d = object_cast<Node3D>(child);
        tr.bone_idx = bone_idx;
        if (bone_idx == -1) {
            tr.subpath = leftover_path;
        }

        track_map[key] = tr;
    }

    return &track_map[key];
}

void AnimationTreePlayer::_recompute_caches() {

    track_map.clear();
    _recompute_caches(out_name);
    dirty_caches = false;
}

void AnimationTreePlayer::_recompute_caches(const StringName &p_node) {

    ERR_FAIL_COND(!node_map.contains(p_node));

    AnimationTreeNodeBase *nb = node_map[p_node];

    if (nb->type == NODE_ANIMATION) {

        AnimationTreeNode *an = static_cast<AnimationTreeNode *>(nb);
        an->tref.clear();

        if (an->animation) {

            Ref<Animation> a = an->animation;

            for (int i = 0; i < an->animation->get_track_count(); i++) {

                Track *tr = _find_track(a->track_get_path(i));
                if (!tr)
                    continue;

                AnimationTreeNode::TrackRef tref;
                tref.local_track = i;
                tref.track = tr;
                tref.weight = 0;

                an->tref.push_back(tref);
            }
        }
    }

    for (int i = 0; i < nb->inputs.size(); i++) {

        _recompute_caches(nb->inputs[i].node);
    }
}

void AnimationTreePlayer::recompute_caches() {

    dirty_caches = true;
}

/* playback */

void AnimationTreePlayer::set_active(bool p_active) {

    if (active == p_active)
        return;

    active = p_active;
    processing = active;
    reset_request = p_active;
    _set_process(processing, true);
}

bool AnimationTreePlayer::is_active() const {

    return active;
}

AnimationTreePlayer::ConnectError AnimationTreePlayer::get_last_error() const {

    return last_error;
}

void AnimationTreePlayer::reset() {

    reset_request = true;
}

void AnimationTreePlayer::set_base_path(const NodePath &p_path) {

    base_path = p_path;
    recompute_caches();
}

NodePath AnimationTreePlayer::get_base_path() const {

    return base_path;
}

void AnimationTreePlayer::set_master_player(const NodePath &p_path) {

    if (p_path == master)
        return;

    master = p_path;
    _update_sources();
    recompute_caches();
}

NodePath AnimationTreePlayer::get_master_player() const {

    return master;
}

PoolVector<String> AnimationTreePlayer::_get_node_list() {

    Vector<StringName> nl=get_node_vector();
    PoolVector<String> ret;
    ret.resize(nl.size());
    int idx = 0;
    for (const StringName &E : nl) {
        ret.set(idx++, String(E));
    }

    return ret;
}

void AnimationTreePlayer::_update_sources() {

    if (master == NodePath())
        return;
    if (!is_inside_tree())
        return;

    Node *m = get_node(master);
    if (!m) {
        master = NodePath();
        ERR_FAIL_COND(!m);
    }

    AnimationPlayer *ap = object_cast<AnimationPlayer>(m);

    if (!ap) {

        master = NodePath();
        ERR_FAIL_COND(!ap);
    }

    for (eastl::pair<const StringName,AnimationTreeNodeBase *> &E : node_map) {

        if (E.second->type == NODE_ANIMATION) {

            AnimationTreeNode *an = static_cast<AnimationTreeNode *>(E.second);

            if (!an->from.empty()) {

                an->animation = ap->get_animation(StringName(an->from));
            }
        }
    }
}
//TODO: SEGS: use string_view parameter + contains_as here.
bool AnimationTreePlayer::node_exists(const StringName &p_name) const {

    return (node_map.contains(p_name));
}

Error AnimationTreePlayer::node_rename(const StringName &p_node, const StringName &p_new_name) {

    if (p_new_name == p_node)
        return OK;
    ERR_FAIL_COND_V(!node_map.contains(p_node), ERR_ALREADY_EXISTS);
    ERR_FAIL_COND_V(node_map.contains(p_new_name), ERR_ALREADY_EXISTS);
    ERR_FAIL_COND_V(p_new_name == StringName(), ERR_INVALID_DATA);
    ERR_FAIL_COND_V(p_node == out_name, ERR_INVALID_DATA);
    ERR_FAIL_COND_V(p_new_name == out_name, ERR_INVALID_DATA);

    for (eastl::pair<const StringName,AnimationTreeNodeBase *> &E : node_map) {

        AnimationTreeNodeBase *nb = E.second;
        for (int i = 0; i < nb->inputs.size(); i++) {

            if (nb->inputs[i].node == p_node) {
                nb->inputs[i].node = p_new_name;
            }
        }
    }

    node_map[p_new_name] = node_map[p_node];
    node_map.erase(p_node);

    return OK;
}

String AnimationTreePlayer::get_configuration_warning() const {

    return TTRS("This node has been deprecated. Use AnimationTree instead.");
}

void AnimationTreePlayer::_bind_methods() {

    SE_BIND_METHOD(AnimationTreePlayer,add_node);

    SE_BIND_METHOD(AnimationTreePlayer,node_exists);
    SE_BIND_METHOD(AnimationTreePlayer,node_rename);

    SE_BIND_METHOD(AnimationTreePlayer,node_get_type);
    SE_BIND_METHOD(AnimationTreePlayer,node_get_input_count);
    SE_BIND_METHOD(AnimationTreePlayer,node_get_input_source);

    SE_BIND_METHOD(AnimationTreePlayer,animation_node_set_animation);
    SE_BIND_METHOD(AnimationTreePlayer,animation_node_get_animation);

    SE_BIND_METHOD(AnimationTreePlayer,animation_node_set_master_animation);
    SE_BIND_METHOD(AnimationTreePlayer,animation_node_get_master_animation);
    SE_BIND_METHOD(AnimationTreePlayer,animation_node_get_position);
    SE_BIND_METHOD(AnimationTreePlayer,animation_node_set_filter_path);

    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_set_fadein_time);
    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_get_fadein_time);

    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_set_fadeout_time);
    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_get_fadeout_time);

    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_set_autorestart);
    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_set_autorestart_delay);
    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_set_autorestart_random_delay);

    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_has_autorestart);
    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_get_autorestart_delay);
    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_get_autorestart_random_delay);

    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_start);
    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_stop);
    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_is_active);
    SE_BIND_METHOD(AnimationTreePlayer,oneshot_node_set_filter_path);

    SE_BIND_METHOD(AnimationTreePlayer,mix_node_set_amount);
    SE_BIND_METHOD(AnimationTreePlayer,mix_node_get_amount);

    SE_BIND_METHOD(AnimationTreePlayer,blend2_node_set_amount);
    SE_BIND_METHOD(AnimationTreePlayer,blend2_node_get_amount);
    SE_BIND_METHOD(AnimationTreePlayer,blend2_node_set_filter_path);

    SE_BIND_METHOD(AnimationTreePlayer,blend3_node_set_amount);
    SE_BIND_METHOD(AnimationTreePlayer,blend3_node_get_amount);

    SE_BIND_METHOD(AnimationTreePlayer,blend4_node_set_amount);
    SE_BIND_METHOD(AnimationTreePlayer,blend4_node_get_amount);

    SE_BIND_METHOD(AnimationTreePlayer,timescale_node_set_scale);
    SE_BIND_METHOD(AnimationTreePlayer,timescale_node_get_scale);

    SE_BIND_METHOD(AnimationTreePlayer,timeseek_node_seek);

    SE_BIND_METHOD(AnimationTreePlayer,transition_node_set_input_count);
    SE_BIND_METHOD(AnimationTreePlayer,transition_node_get_input_count);
    SE_BIND_METHOD(AnimationTreePlayer,transition_node_delete_input);

    SE_BIND_METHOD(AnimationTreePlayer,transition_node_set_input_auto_advance);
    SE_BIND_METHOD(AnimationTreePlayer,transition_node_has_input_auto_advance);

    SE_BIND_METHOD(AnimationTreePlayer,transition_node_set_xfade_time);
    SE_BIND_METHOD(AnimationTreePlayer,transition_node_get_xfade_time);

    SE_BIND_METHOD(AnimationTreePlayer,transition_node_set_current);
    SE_BIND_METHOD(AnimationTreePlayer,transition_node_get_current);

    SE_BIND_METHOD(AnimationTreePlayer,node_set_position);
    SE_BIND_METHOD(AnimationTreePlayer,node_get_position);

    SE_BIND_METHOD(AnimationTreePlayer,remove_node);
    SE_BIND_METHOD(AnimationTreePlayer,connect_nodes);
    SE_BIND_METHOD(AnimationTreePlayer,are_nodes_connected);
    SE_BIND_METHOD(AnimationTreePlayer,disconnect_nodes);

    SE_BIND_METHOD(AnimationTreePlayer,set_active);
    SE_BIND_METHOD(AnimationTreePlayer,is_active);

    SE_BIND_METHOD(AnimationTreePlayer,set_base_path);
    SE_BIND_METHOD(AnimationTreePlayer,get_base_path);

    SE_BIND_METHOD(AnimationTreePlayer,set_master_player);
    SE_BIND_METHOD(AnimationTreePlayer,get_master_player);

    SE_BIND_METHOD(AnimationTreePlayer,get_node_vector);

    SE_BIND_METHOD(AnimationTreePlayer,set_animation_process_mode);
    SE_BIND_METHOD(AnimationTreePlayer,get_animation_process_mode);

    SE_BIND_METHOD(AnimationTreePlayer,advance);

    SE_BIND_METHOD(AnimationTreePlayer,reset);

    SE_BIND_METHOD(AnimationTreePlayer,recompute_caches);

    ADD_GROUP("Playback", "playback_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "playback_process_mode", PropertyHint::Enum, "Physics,Idle"), "set_animation_process_mode", "get_animation_process_mode");

    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "master_player", PropertyHint::NodePathValidTypes, "AnimationPlayer"), "set_master_player", "get_master_player");
    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "base_path"), "set_base_path", "get_base_path");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "active"), "set_active", "is_active");

    BIND_ENUM_CONSTANT(NODE_OUTPUT);
    BIND_ENUM_CONSTANT(NODE_ANIMATION);
    BIND_ENUM_CONSTANT(NODE_ONESHOT);
    BIND_ENUM_CONSTANT(NODE_MIX);
    BIND_ENUM_CONSTANT(NODE_BLEND2);
    BIND_ENUM_CONSTANT(NODE_BLEND3);
    BIND_ENUM_CONSTANT(NODE_BLEND4);
    BIND_ENUM_CONSTANT(NODE_TIMESCALE);
    BIND_ENUM_CONSTANT(NODE_TIMESEEK);
    BIND_ENUM_CONSTANT(NODE_TRANSITION);

    BIND_ENUM_CONSTANT(ANIMATION_PROCESS_PHYSICS);
    BIND_ENUM_CONSTANT(ANIMATION_PROCESS_IDLE);
}

AnimationTreePlayer::AnimationTreePlayer() {

    active_list = nullptr;
    out = memnew(AnimationTreeNodeOut);
    out_name = "out";
    out->pos = Point2(40, 40);
    node_map.emplace(out_name, out);
    animation_process_mode = ANIMATION_PROCESS_IDLE;
    processing = false;
    active = false;
    dirty_caches = true;
    reset_request = true;
    last_error = CONNECT_INCOMPLETE;
    base_path = NodePath("..");
}

AnimationTreePlayer::~AnimationTreePlayer() {

    while (!node_map.empty()) {
        memdelete(node_map.begin()->second);
        node_map.erase(node_map.begin());
    }
}
