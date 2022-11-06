/*************************************************************************/
/*  animation_node_state_machine.cpp                                     */
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

#include "animation_node_state_machine.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "EASTL/sort.h"

IMPL_GDCLASS(AnimationNodeStateMachineTransition)
IMPL_GDCLASS(AnimationNodeStateMachinePlayback)
IMPL_GDCLASS(AnimationNodeStateMachine)
VARIANT_ENUM_CAST(AnimationNodeStateMachineTransition::SwitchMode);

/////////////////////////////////////////////////

void AnimationNodeStateMachineTransition::set_switch_mode(SwitchMode p_mode) {

    switch_mode = p_mode;
}

AnimationNodeStateMachineTransition::SwitchMode AnimationNodeStateMachineTransition::get_switch_mode() const {

    return switch_mode;
}

void AnimationNodeStateMachineTransition::set_auto_advance(bool p_enable) {
    auto_advance = p_enable;
}

bool AnimationNodeStateMachineTransition::has_auto_advance() const {
    return auto_advance;
}

void AnimationNodeStateMachineTransition::set_advance_condition(const StringName &p_condition) {

    ERR_FAIL_COND(StringUtils::contains(p_condition,"/") || StringUtils::contains(p_condition,":"));
    advance_condition = p_condition;
    if (not p_condition.empty()) {
        advance_condition_name = StringName(String("conditions/") + p_condition);
    } else {
        advance_condition_name = StringName();
    }
    emit_signal("advance_condition_changed");
}

StringName AnimationNodeStateMachineTransition::get_advance_condition() const {
    return advance_condition;
}

StringName AnimationNodeStateMachineTransition::get_advance_condition_name() const {
    return advance_condition_name;
}

void AnimationNodeStateMachineTransition::set_xfade_time(float p_xfade) {

    ERR_FAIL_COND(p_xfade < 0);
    xfade = p_xfade;
    emit_changed();
}

float AnimationNodeStateMachineTransition::get_xfade_time() const {
    return xfade;
}

void AnimationNodeStateMachineTransition::set_disabled(bool p_disabled) {
    disabled = p_disabled;
    emit_changed();
}

bool AnimationNodeStateMachineTransition::is_disabled() const {
    return disabled;
}

void AnimationNodeStateMachineTransition::set_priority(int p_priority) {
    priority = p_priority;
    emit_changed();
}

int AnimationNodeStateMachineTransition::get_priority() const {
    return priority;
}

void AnimationNodeStateMachineTransition::_bind_methods() {
    SE_BIND_METHOD(AnimationNodeStateMachineTransition,set_switch_mode);
    SE_BIND_METHOD(AnimationNodeStateMachineTransition,get_switch_mode);

    SE_BIND_METHOD(AnimationNodeStateMachineTransition,set_auto_advance);
    SE_BIND_METHOD(AnimationNodeStateMachineTransition,has_auto_advance);

    SE_BIND_METHOD(AnimationNodeStateMachineTransition,set_advance_condition);
    SE_BIND_METHOD(AnimationNodeStateMachineTransition,get_advance_condition);

    SE_BIND_METHOD(AnimationNodeStateMachineTransition,set_xfade_time);
    SE_BIND_METHOD(AnimationNodeStateMachineTransition,get_xfade_time);

    SE_BIND_METHOD(AnimationNodeStateMachineTransition,set_disabled);
    SE_BIND_METHOD(AnimationNodeStateMachineTransition,is_disabled);

    SE_BIND_METHOD(AnimationNodeStateMachineTransition,set_priority);
    SE_BIND_METHOD(AnimationNodeStateMachineTransition,get_priority);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "switch_mode", PropertyHint::Enum, "Immediate,Sync,AtEnd"), "set_switch_mode", "get_switch_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "auto_advance"), "set_auto_advance", "has_auto_advance");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "advance_condition"), "set_advance_condition", "get_advance_condition");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "xfade_time", PropertyHint::Range, "0,240,0.01"), "set_xfade_time", "get_xfade_time");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "priority", PropertyHint::Range, "0,32,1"), "set_priority", "get_priority");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "disabled"), "set_disabled", "is_disabled");

    BIND_ENUM_CONSTANT(SWITCH_MODE_IMMEDIATE);
    BIND_ENUM_CONSTANT(SWITCH_MODE_SYNC);
    BIND_ENUM_CONSTANT(SWITCH_MODE_AT_END);

    ADD_SIGNAL(MethodInfo("advance_condition_changed"));
}

AnimationNodeStateMachineTransition::AnimationNodeStateMachineTransition() {

    switch_mode = SWITCH_MODE_IMMEDIATE;
    auto_advance = false;
    xfade = 0;
    disabled = false;
    priority = 1;
}

////////////////////////////////////////////////////////

void AnimationNodeStateMachinePlayback::travel(const StringName &p_state) {

    start_request_travel = true;
    start_request = p_state;
    stop_request = false;
}

void AnimationNodeStateMachinePlayback::start(const StringName &p_state) {
    start_request_travel = false;
    start_request = p_state;
    stop_request = false;
}
void AnimationNodeStateMachinePlayback::stop() {

    stop_request = true;
}
bool AnimationNodeStateMachinePlayback::is_playing() const {
    return playing;
}
StringName AnimationNodeStateMachinePlayback::get_current_node() const {
    return current;
}
StringName AnimationNodeStateMachinePlayback::get_blend_from_node() const {
    return fading_from;
}
const Vector<StringName> &AnimationNodeStateMachinePlayback::get_travel_path() const {
    return path;
}
float AnimationNodeStateMachinePlayback::get_current_play_position() const {
    return pos_current;
}
float AnimationNodeStateMachinePlayback::get_current_length() const {
    return len_current;
}

bool AnimationNodeStateMachinePlayback::_travel(AnimationNodeStateMachine *p_state_machine, const StringName &p_travel) {

    ERR_FAIL_COND_V(!playing, false);
    ERR_FAIL_COND_V(!p_state_machine->states.contains(p_travel), false);
    ERR_FAIL_COND_V(!p_state_machine->states.contains(current), false);

    path.clear(); //a new one will be needed

    if (current == p_travel)
        return true; //nothing to do

    loops_current = 0; // reset loops, so fade does not happen immediately

    Vector2 current_pos = p_state_machine->states[current].position;
    Vector2 target_pos = p_state_machine->states[p_travel].position;

    HashMap<StringName, AStarCost> cost_map;

    Vector<int> open_list;
    open_list.reserve(p_state_machine->transitions.size());

    //build open list
    for (int i = 0; i < p_state_machine->transitions.size(); i++) {
        if (p_state_machine->transitions[i].from == current) {
            open_list.emplace_back(i);
            float cost = p_state_machine->states[p_state_machine->transitions[i].to].position.distance_to(current_pos);
            cost *= p_state_machine->transitions[i].transition->get_priority();
            AStarCost ap;
            ap.prev = current;
            ap.distance = cost;
            cost_map[p_state_machine->transitions[i].to] = ap;

            if (p_state_machine->transitions[i].to == p_travel) { //prematurely found it! :D
                path.push_back(p_travel);
                return true;
            }
        }
    }

    //begin astar
    bool found_route = false;
    while (!found_route) {

        if (open_list.empty()) {
            return false; //no path found
        }

        //find the last cost transition
        Vector<int>::iterator least_cost_transition = open_list.end();
        float least_cost = 1e20f;

        for (auto iter=open_list.begin(),fin=open_list.end(); iter!=fin; ++iter) {

            float cost = cost_map[p_state_machine->transitions[*iter].to].distance;
            cost += p_state_machine->states[p_state_machine->transitions[*iter].to].position.distance_to(target_pos);

            if (cost < least_cost) {
                least_cost_transition = iter;
                least_cost = cost;
            }
        }

        StringName transition_prev = p_state_machine->transitions[*least_cost_transition].from;
        StringName transition = p_state_machine->transitions[*least_cost_transition].to;

        for (int i = 0; i < p_state_machine->transitions.size(); i++) {
            if (p_state_machine->transitions[i].from != transition || p_state_machine->transitions[i].to == transition_prev) {
                continue; //not interested on those
            }

            float distance = p_state_machine->states[p_state_machine->transitions[i].from].position.distance_to(p_state_machine->states[p_state_machine->transitions[i].to].position);
            distance *= p_state_machine->transitions[i].transition->get_priority();
            distance += cost_map[p_state_machine->transitions[i].from].distance;

            if (cost_map.contains(p_state_machine->transitions[i].to)) {
                //oh this was visited already, can we win the cost?
                if (distance < cost_map[p_state_machine->transitions[i].to].distance) {
                    cost_map[p_state_machine->transitions[i].to].distance = distance;
                    cost_map[p_state_machine->transitions[i].to].prev = p_state_machine->transitions[i].from;
                }
            } else {
                //add to open list
                AStarCost ac;
                ac.prev = p_state_machine->transitions[i].from;
                ac.distance = distance;
                cost_map[p_state_machine->transitions[i].to] = ac;

                open_list.push_back(i);

                if (p_state_machine->transitions[i].to == p_travel) {
                    found_route = true;
                    break;
                }
            }
        }

        if (found_route) {
            break;
        }

        open_list.erase(least_cost_transition); // TODO: erase_unsorted here?
    }

    //make path
    StringName at = p_travel;
    while (at != current) {
        path.push_back(at);
        at = cost_map[at].prev;
    }
    eastl::reverse(path.begin(),path.end());

    return true;
}

float AnimationNodeStateMachinePlayback::process(AnimationNodeStateMachine *p_state_machine, float p_time, bool p_seek) {

    //if not playing and it can restart, then restart
    if (!playing && start_request == StringName()) {
        if (!stop_request && p_state_machine->start_node) {
            start(p_state_machine->start_node);
        } else {
            return 0;
        }
    }

    if (playing && stop_request) {
        stop_request = false;
        playing = false;
        return 0;
    }

    bool play_start = false;

    if (not start_request.empty()) {

        if (start_request_travel) {
            if (!playing) {
                if (!stop_request && p_state_machine->start_node) {
                    // can restart, just postpone traveling
                    path.clear();
                    current = p_state_machine->start_node;
                    playing = true;
                    play_start = true;
                } else {
                    // stopped, invalid state
                    StringName node_name = start_request;
                    start_request = StringName(); //clear start request
                    ERR_FAIL_V_MSG(0, "Can't travel to '" + node_name + "' if state machine is not playing. Maybe you need to enable Autoplay on Load for one of the nodes in your state machine or call .start() first?");
                }
            } else {
                if (!_travel(p_state_machine, start_request)) {
                    // can't travel, then teleport
                    path.clear();
                    current = start_request;
                }
                start_request = StringName(); //clear start request
            }
        } else {
            // teleport to start
            if (p_state_machine->states.contains(start_request)) {
            path.clear();
            current = start_request;
            playing = true;
            play_start = true;
            start_request = StringName(); //clear start request
            } else {
                StringName node = start_request;
                start_request = StringName(); //clear start request
                ERR_FAIL_V_MSG(0, String("No such node: '") + node + "'");
            }
        }
    }

    bool do_start = (p_seek && p_time == 0) || play_start || current == StringName();

    if (do_start) {

        if (p_state_machine->start_node != StringName() && p_seek && p_time == 0) {
            current = p_state_machine->start_node;
        }

        len_current = p_state_machine->blend_node(current, p_state_machine->states[current].node, 0, true, 1.0, AnimationNode::FILTER_IGNORE, false);
        pos_current = 0;
        loops_current = 0;
    }

    if (!p_state_machine->states.contains(current)) {
        playing = false; //current does not exist
        current = StringName();
        return 0;
    }
    float fade_blend = 1.0;

    if (fading_from != StringName()) {

        if (!p_state_machine->states.contains(fading_from)) {
            fading_from = StringName();
        } else {
            if (!p_seek) {
                fading_pos += p_time;
            }
            fade_blend = MIN(1.0f, fading_pos / fading_time);
            if (fade_blend >= 1.0f) {
                fading_from = StringName();
            }
        }
    }

    float rem = p_state_machine->blend_node(current, p_state_machine->states[current].node, p_time, p_seek, fade_blend, AnimationNode::FILTER_IGNORE, false);

    if (fading_from != StringName()) {

        p_state_machine->blend_node(fading_from, p_state_machine->states[fading_from].node, p_time, p_seek, 1.0f - fade_blend, AnimationNode::FILTER_IGNORE, false);
    }

    //guess playback position
    if (rem > len_current) { // weird but ok
        len_current = rem;
    }

    { //advance and loop check

        float next_pos = len_current - rem;

        if (next_pos < pos_current) {
            loops_current++;
        }
        pos_current = next_pos; //looped
    }

    //find next
    StringName next;
    float next_xfade = 0;
    AnimationNodeStateMachineTransition::SwitchMode switch_mode = AnimationNodeStateMachineTransition::SWITCH_MODE_IMMEDIATE;

    if (!path.empty()) {

        for (int i = 0; i < p_state_machine->transitions.size(); i++) {
            if (p_state_machine->transitions[i].from == current && p_state_machine->transitions[i].to == path[0]) {
                next_xfade = p_state_machine->transitions[i].transition->get_xfade_time();
                switch_mode = p_state_machine->transitions[i].transition->get_switch_mode();
                next = path[0];
            }
        }
    } else {
        float priority_best = 1e20f;
        int auto_advance_to = -1;
        for (int i = 0; i < p_state_machine->transitions.size(); i++) {

            bool auto_advance = false;
            if (p_state_machine->transitions[i].transition->has_auto_advance()) {
                auto_advance = true;
            }
            StringName advance_condition_name = p_state_machine->transitions[i].transition->get_advance_condition_name();
            if (advance_condition_name != StringName() && p_state_machine->get_parameter(advance_condition_name).as<bool>()) {
                auto_advance = true;
            }

            if (p_state_machine->transitions[i].from == current && auto_advance) {

                if (p_state_machine->transitions[i].transition->get_priority() <= priority_best) {
                    priority_best = p_state_machine->transitions[i].transition->get_priority();
                    auto_advance_to = i;
                }
            }
        }

        if (auto_advance_to != -1) {
            next = p_state_machine->transitions[auto_advance_to].to;
            next_xfade = p_state_machine->transitions[auto_advance_to].transition->get_xfade_time();
            switch_mode = p_state_machine->transitions[auto_advance_to].transition->get_switch_mode();
        }
    }

    //if next, see when to transition
    if (next != StringName()) {

        bool goto_next = false;

        if (switch_mode == AnimationNodeStateMachineTransition::SWITCH_MODE_AT_END) {
            goto_next = next_xfade >= (len_current - pos_current) || loops_current > 0;
            if (loops_current > 0) {
                next_xfade = 0;
            }
        } else {
            goto_next = fading_from == StringName();
        }

        if (goto_next) { //loops should be used because fade time may be too small or zero and animation may have looped

            if (next_xfade) {
                //time to fade, baby
                fading_from = current;
                fading_time = next_xfade;
                fading_pos = 0;
            } else {
                fading_from = StringName();
                fading_pos = 0;
            }

            if (!path.empty()) { //if it came from path, remove path
                path.pop_front();
            }
            current = next;
            if (switch_mode == AnimationNodeStateMachineTransition::SWITCH_MODE_SYNC) {
                len_current = p_state_machine->blend_node(current, p_state_machine->states[current].node, 0, true, 0, AnimationNode::FILTER_IGNORE, false);
                pos_current = MIN(pos_current, len_current);
                p_state_machine->blend_node(current, p_state_machine->states[current].node, pos_current, true, 0, AnimationNode::FILTER_IGNORE, false);

            } else {
                len_current = p_state_machine->blend_node(current, p_state_machine->states[current].node, 0, true, 0, AnimationNode::FILTER_IGNORE, false);
                pos_current = 0;
            }

            rem = len_current; //so it does not show 0 on transition
            loops_current = 0;
        }
    }

    //compute time left for transitions by using the end node
    if (p_state_machine->end_node != StringName() && p_state_machine->end_node != current) {

        rem = p_state_machine->blend_node(p_state_machine->end_node, p_state_machine->states[p_state_machine->end_node].node, 0, true, 0, AnimationNode::FILTER_IGNORE, false);
    }

    return rem;
}

void AnimationNodeStateMachinePlayback::_bind_methods() {

    SE_BIND_METHOD(AnimationNodeStateMachinePlayback,travel);
    SE_BIND_METHOD(AnimationNodeStateMachinePlayback,start);
    SE_BIND_METHOD(AnimationNodeStateMachinePlayback,stop);
    SE_BIND_METHOD(AnimationNodeStateMachinePlayback,is_playing);
    SE_BIND_METHOD(AnimationNodeStateMachinePlayback,get_current_node);
    SE_BIND_METHOD(AnimationNodeStateMachinePlayback,get_current_play_position);
    SE_BIND_METHOD(AnimationNodeStateMachinePlayback,get_current_length);
    SE_BIND_METHOD(AnimationNodeStateMachinePlayback,get_travel_path);
}

AnimationNodeStateMachinePlayback::AnimationNodeStateMachinePlayback() {
    set_local_to_scene(true); //only one per instanced scene

    playing = false;
    len_current = 0;
    fading_time = 0;
    stop_request = false;
}

///////////////////////////////////////////////////////

void AnimationNodeStateMachine::get_parameter_list(Vector<PropertyInfo> *r_list) const {
    r_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(playback), PropertyHint::ResourceType, "AnimationNodeStateMachinePlayback", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_DO_NOT_SHARE_ON_DUPLICATE));
    Vector<StringName> advance_conditions;
    for (int i = 0; i < transitions.size(); i++) {
        StringName ac = transitions[i].transition->get_advance_condition_name();
        if (!ac.empty() && advance_conditions.find(ac) == nullptr) {
            advance_conditions.emplace_back(eastl::move(ac));
        }
    }

    eastl::sort(advance_conditions.begin(),advance_conditions.end(),WrapAlphaCompare());
    for (StringName &E : advance_conditions) {
        r_list->push_back(PropertyInfo(VariantType::BOOL, eastl::move(E)));
    }
}

Variant AnimationNodeStateMachine::get_parameter_default_value(const StringName &p_parameter) const {

    if (p_parameter == playback) {
        Ref<AnimationNodeStateMachinePlayback> p(make_ref_counted<AnimationNodeStateMachinePlayback>());
        return p;
    } else {
        return false; //advance condition
    }
}

void AnimationNodeStateMachine::add_node(const StringName &p_name, Ref<AnimationNode> p_node, const Vector2 &p_position) {

    ERR_FAIL_COND(states.contains(p_name));
    ERR_FAIL_COND(not p_node);
    ERR_FAIL_COND(StringUtils::contains(p_name,'/'));

    State state;
    state.node = dynamic_ref_cast<AnimationRootNode>(p_node);
    state.position = p_position;

    states[p_name] = state;

    emit_changed();
    emit_signal("tree_changed");

    p_node->connect("tree_changed",callable_mp(this, &ClassName::_tree_changed),  ObjectNS::CONNECT_REFERENCE_COUNTED);
}

void AnimationNodeStateMachine::replace_node(const StringName &p_name, Ref<AnimationNode> p_node) {

    ERR_FAIL_COND(states.contains(p_name) == false);
    ERR_FAIL_COND(not p_node);
    ERR_FAIL_COND(String(p_name).find("/") != -1);

    {
        Ref<AnimationNode> node = states[p_name].node;
        if (node) {
            node->disconnect("tree_changed",callable_mp(this, &ClassName::_tree_changed));
        }
    }

    states[p_name].node = dynamic_ref_cast<AnimationRootNode>(p_node);

    emit_changed();
    emit_signal("tree_changed");

    p_node->connectF("tree_changed",this,[=]() { _tree_changed(); }, ObjectNS::CONNECT_REFERENCE_COUNTED);
}

Ref<AnimationNode> AnimationNodeStateMachine::get_node(const StringName &p_name) const {

    ERR_FAIL_COND_V(!states.contains(p_name), Ref<AnimationNode>());

    return states.at(p_name).node;
}

StringName AnimationNodeStateMachine::get_node_name(const Ref<AnimationNode> &p_node) const {
    for (const eastl::pair<const StringName,State> &E : states) {
        if (Ref<AnimationNode>(E.second.node) == p_node) {
            return E.first;
        }
    }

    ERR_FAIL_V(StringName());
}

void AnimationNodeStateMachine::get_child_nodes(Vector<AnimationNode::ChildNode> *r_child_nodes) {
    Vector<StringName> nodes;

    for (eastl::pair<const StringName,State> &E : states) {
        nodes.push_back(E.first);
    }
    eastl::sort(nodes.begin(), nodes.end(), WrapAlphaCompare());

    for (int i = 0; i < nodes.size(); i++) {
        ChildNode cn;
        cn.name = nodes[i];
        cn.node = states[cn.name].node;
        r_child_nodes->push_back(cn);
    }
}
//TODO: SEGS: use string_view as a parameter here
bool AnimationNodeStateMachine::has_node(const StringName &p_name) const {
    //TODO: SEGS: use contains_as to prevent allocations
    return states.contains(p_name);
}
void AnimationNodeStateMachine::remove_node(const StringName &p_name) {

    ERR_FAIL_COND(!states.contains(p_name));

    {
        Ref<AnimationNode> node = states[p_name].node;

        ERR_FAIL_COND(not node);

        node->disconnect("tree_changed",callable_mp(this, &ClassName::_tree_changed));
    }

    states.erase(p_name);
    //path.erase(p_name);

    for (int i = 0; i < transitions.size(); i++) {
        if (transitions[i].from == p_name || transitions[i].to == p_name) {
            transitions[i].transition->disconnect("advance_condition_changed",callable_mp(this, &ClassName::_tree_changed));
            transitions.erase_at(i);
            i--;
        }
    }

    if (start_node == p_name) {
        start_node = StringName();
    }

    if (end_node == p_name) {
        end_node = StringName();
    }

    /*if (playing && current == p_name) {
        stop();
    }*/

    emit_changed();
    emit_signal("tree_changed");
}

void AnimationNodeStateMachine::rename_node(const StringName &p_name, const StringName &p_new_name) {

    ERR_FAIL_COND(!states.contains(p_name));
    ERR_FAIL_COND(states.contains(p_new_name));

    states[p_new_name] = states[p_name];
    states.erase(p_name);

    for (int i = 0; i < transitions.size(); i++) {
        if (transitions[i].from == p_name) {
            transitions[i].from = p_new_name;
        }

        if (transitions[i].to == p_name) {
            transitions[i].to = p_new_name;
        }
    }

    if (start_node == p_name) {
        start_node = p_new_name;
    }

    if (end_node == p_name) {
        end_node = p_new_name;
    }

    /*if (playing && current == p_name) {
        current = p_new_name;
    }*/

    //path.clear(); //clear path
    emit_signal("tree_changed");
}

void AnimationNodeStateMachine::get_node_list(List<StringName> *r_nodes) const {

    List<StringName> nodes;
    for (const eastl::pair<const StringName,State> &E : states) {
        nodes.push_back(E.first);
    }
    nodes.sort(WrapAlphaCompare());
    r_nodes->splice(r_nodes->end(), eastl::move(nodes));
}

bool AnimationNodeStateMachine::has_transition(const StringName &p_from, const StringName &p_to) const {

    for (int i = 0; i < transitions.size(); i++) {
        if (transitions[i].from == p_from && transitions[i].to == p_to)
            return true;
    }
    return false;
}

int AnimationNodeStateMachine::find_transition(const StringName &p_from, const StringName &p_to) const {

    for (int i = 0; i < transitions.size(); i++) {
        if (transitions[i].from == p_from && transitions[i].to == p_to)
            return i;
    }
    return -1;
}

void AnimationNodeStateMachine::add_transition(const StringName &p_from, const StringName &p_to, const Ref<AnimationNodeStateMachineTransition> &p_transition) {

    ERR_FAIL_COND(p_from == p_to);
    ERR_FAIL_COND(!states.contains(p_from));
    ERR_FAIL_COND(!states.contains(p_to));
    ERR_FAIL_COND(not p_transition);

    for (int i = 0; i < transitions.size(); i++) {
        ERR_FAIL_COND(transitions[i].from == p_from && transitions[i].to == p_to);
    }

    Transition tr;
    tr.from = p_from;
    tr.to = p_to;
    tr.transition = p_transition;

    tr.transition->connect("advance_condition_changed",callable_mp(this, &ClassName::_tree_changed), ObjectNS::CONNECT_REFERENCE_COUNTED);

    transitions.push_back(tr);
}

Ref<AnimationNodeStateMachineTransition> AnimationNodeStateMachine::get_transition(int p_transition) const {
    ERR_FAIL_INDEX_V(p_transition, transitions.size(), Ref<AnimationNodeStateMachineTransition>());
    return transitions[p_transition].transition;
}
StringName AnimationNodeStateMachine::get_transition_from(int p_transition) const {

    ERR_FAIL_INDEX_V(p_transition, transitions.size(), StringName());
    return transitions[p_transition].from;
}
StringName AnimationNodeStateMachine::get_transition_to(int p_transition) const {

    ERR_FAIL_INDEX_V(p_transition, transitions.size(), StringName());
    return transitions[p_transition].to;
}

int AnimationNodeStateMachine::get_transition_count() const {

    return transitions.size();
}
void AnimationNodeStateMachine::remove_transition(const StringName &p_from, const StringName &p_to) {

    for (int i = 0; i < transitions.size(); i++) {
        if (transitions[i].from == p_from && transitions[i].to == p_to) {
            transitions[i].transition->disconnect("advance_condition_changed",callable_mp(this, &ClassName::_tree_changed));
            transitions.erase_at(i);
            return;
        }
    }

    /*if (playing) {
        path.clear();
    }*/
}

void AnimationNodeStateMachine::remove_transition_by_index(int p_transition) {

    ERR_FAIL_INDEX(p_transition, transitions.size());
    transitions[p_transition].transition->disconnect("advance_condition_changed",callable_mp(this, &ClassName::_tree_changed));
    transitions.erase_at(p_transition);
    /*if (playing) {
        path.clear();
    }*/
}

void AnimationNodeStateMachine::set_start_node(const StringName &p_node) {

    ERR_FAIL_COND(p_node != StringName() && !states.contains(p_node));
    start_node = p_node;
}

StringName AnimationNodeStateMachine::get_start_node() const {

    return start_node;
}

void AnimationNodeStateMachine::set_end_node(const StringName &p_node) {

    ERR_FAIL_COND(p_node != StringName() && !states.contains(p_node));
    end_node = p_node;
}

StringName AnimationNodeStateMachine::get_end_node() const {

    return end_node;
}

void AnimationNodeStateMachine::set_graph_offset(const Vector2 &p_offset) {
    graph_offset = p_offset;
}

Vector2 AnimationNodeStateMachine::get_graph_offset() const {
    return graph_offset;
}

float AnimationNodeStateMachine::process(float p_time, bool p_seek) {

    Ref<AnimationNodeStateMachinePlayback> playback = refFromVariant<AnimationNodeStateMachinePlayback>(get_parameter(this->playback));
    ERR_FAIL_COND_V(not playback, 0.0);

    return playback->process(this, p_time, p_seek);
}

StringView AnimationNodeStateMachine::get_caption() const {
    return ("StateMachine");
}

void AnimationNodeStateMachine::_notification(int p_what) {
}

Ref<AnimationNode> AnimationNodeStateMachine::get_child_by_name(const StringName &p_name) {
    return get_node(p_name);
}

bool AnimationNodeStateMachine::_set(const StringName &p_name, const Variant &p_value) {

    if (StringUtils::begins_with(p_name,"states/")) {
        StringName node_name(StringUtils::get_slice(p_name,'/', 1));
        StringView what(StringUtils::get_slice(p_name,'/', 2));

        if (what == StringView("node")) {
            Ref<AnimationNode> anode = refFromVariant<AnimationNode>(p_value);
            if (anode) {
                add_node(node_name, anode);
            }
            return true;
        }

        if (what == StringView("position")) {

            if (states.contains(node_name)) {
                states[node_name].position = p_value.as<Vector2>();
            }
            return true;
        }
    } else if (p_name == "transitions") {

        Array trans = p_value.as<Array>();
        ERR_FAIL_COND_V(trans.size() % 3 != 0, false);

        for (int i = 0; i < trans.size(); i += 3) {
            add_transition(trans[i].as<StringName>(), trans[i + 1].as<StringName>(), refFromVariant<AnimationNodeStateMachineTransition>(trans[i + 2]));
        }
        return true;
    } else if (p_name == "start_node") {
        set_start_node(p_value.as<StringName>());
        return true;
    } else if (p_name == "end_node") {
        set_end_node(p_value.as<StringName>());
        return true;
    } else if (p_name == "graph_offset") {
        set_graph_offset(p_value.as<Vector2>());
        return true;
    }

    return false;
}

bool AnimationNodeStateMachine::_get(const StringName &p_name, Variant &r_ret) const {

    if (StringUtils::begins_with(p_name,"states/")) {
        StringName node_name(StringUtils::get_slice(p_name,'/', 1));
        StringView what = StringUtils::get_slice(p_name,'/', 2);

        if (what == StringView("node")) {
            if (states.contains(node_name)) {
                r_ret = states.at(node_name).node;
                return true;
            }
        }

        if (what == StringView("position")) {

            if (states.contains(node_name)) {
                r_ret = states.at(node_name).position;
                return true;
            }
        }
    } else if (p_name == "transitions") {
        Array trans;
        trans.resize(transitions.size() * 3);

        for (int i = 0; i < transitions.size(); i++) {
            trans[i * 3 + 0] = transitions[i].from;
            trans[i * 3 + 1] = transitions[i].to;
            trans[i * 3 + 2] = transitions[i].transition;
        }

        r_ret = trans;
        return true;
    } else if (p_name == "start_node") {
        r_ret = get_start_node();
        return true;
    } else if (p_name == "end_node") {
        r_ret = get_end_node();
        return true;
    } else if (p_name == "graph_offset") {
        r_ret = get_graph_offset();
        return true;
    }

    return false;
}
void AnimationNodeStateMachine::_get_property_list(Vector<PropertyInfo> *p_list) const {

    Vector<StringName> names;
    for (const eastl::pair<const StringName,State> &E : states) {
        names.push_back(E.first);
    }
    eastl::sort(names.begin(),names.end(),WrapAlphaCompare());

    for (const StringName &E : names) {
        p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(String("states/") + E + "/node"), PropertyHint::ResourceType, "AnimationNode", PROPERTY_USAGE_NOEDITOR));
        p_list->push_back(PropertyInfo(VariantType::VECTOR2, StringName(String("states/") + E + "/position"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));
    }

    p_list->push_back(PropertyInfo(VariantType::ARRAY, "transitions", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));
    p_list->push_back(PropertyInfo(VariantType::STRING_NAME, "start_node", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));
    p_list->push_back(PropertyInfo(VariantType::STRING_NAME, "end_node", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));
    p_list->push_back(PropertyInfo(VariantType::VECTOR2, "graph_offset", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));
}

void AnimationNodeStateMachine::set_node_position(const StringName &p_name, const Vector2 &p_position) {
    ERR_FAIL_COND(!states.contains(p_name));
    states[p_name].position = p_position;
}

Vector2 AnimationNodeStateMachine::get_node_position(const StringName &p_name) const {

    ERR_FAIL_COND_V(!states.contains(p_name), Vector2());
    return states.at(p_name).position;
}

void AnimationNodeStateMachine::_tree_changed() {
    emit_signal("tree_changed");
}

void AnimationNodeStateMachine::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("add_node", {"name", "node", "position"}), &AnimationNodeStateMachine::add_node, {DEFVAL(Vector2())});
    SE_BIND_METHOD(AnimationNodeStateMachine,replace_node);
    SE_BIND_METHOD(AnimationNodeStateMachine,get_node);
    SE_BIND_METHOD(AnimationNodeStateMachine,remove_node);
    SE_BIND_METHOD(AnimationNodeStateMachine,rename_node);
    SE_BIND_METHOD(AnimationNodeStateMachine,has_node);
    SE_BIND_METHOD(AnimationNodeStateMachine,get_node_name);

    SE_BIND_METHOD(AnimationNodeStateMachine,set_node_position);
    SE_BIND_METHOD(AnimationNodeStateMachine,get_node_position);

    SE_BIND_METHOD(AnimationNodeStateMachine,has_transition);
    SE_BIND_METHOD(AnimationNodeStateMachine,add_transition);
    SE_BIND_METHOD(AnimationNodeStateMachine,get_transition);
    SE_BIND_METHOD(AnimationNodeStateMachine,get_transition_from);
    SE_BIND_METHOD(AnimationNodeStateMachine,get_transition_to);
    SE_BIND_METHOD(AnimationNodeStateMachine,get_transition_count);
    SE_BIND_METHOD(AnimationNodeStateMachine,remove_transition_by_index);
    SE_BIND_METHOD(AnimationNodeStateMachine,remove_transition);

    SE_BIND_METHOD(AnimationNodeStateMachine,set_start_node);
    SE_BIND_METHOD(AnimationNodeStateMachine,get_start_node);

    SE_BIND_METHOD(AnimationNodeStateMachine,set_end_node);
    SE_BIND_METHOD(AnimationNodeStateMachine,get_end_node);

    SE_BIND_METHOD(AnimationNodeStateMachine,set_graph_offset);
    SE_BIND_METHOD(AnimationNodeStateMachine,get_graph_offset);

}

AnimationNodeStateMachine::AnimationNodeStateMachine() {

    playback = "playback";
}
