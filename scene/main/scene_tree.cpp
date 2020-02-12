/*************************************************************************/
/*  scene_tree.cpp                                                       */
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

#include "scene_tree.h"

#include "core/external_profiler.h"
#include "core/deque.h"
#include "core/io/marshalls.h"
#include "core/io/resource_loader.h"
#include "core/message_queue.h"
#include "core/object_db.h"
#include "core/os/mutex.h"
#include "core/os/dir_access.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/io/multiplayer_api.h"
#include "core/project_settings.h"
#include "core/method_bind.h"
#include "core/script_language.h"
#include "core/translation_helpers.h"
#include "main/input_default.h"
#include "node.h"
#include "scene/debugger/script_debugger_remote.h"
#include "scene/resources/dynamic_font.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/scene_string_names.h"
#include "servers/physics_2d_server.h"
#include "servers/physics_server.h"
#include "viewport.h"

#include <cstdio>

IMPL_GDCLASS(SceneTreeTimer)
IMPL_GDCLASS(SceneTree)

VARIANT_ENUM_CAST(SceneTree::StretchMode);
VARIANT_ENUM_CAST(SceneTree::StretchAspect);
VARIANT_ENUM_CAST(SceneTree::GroupCallFlags);

#ifdef DEBUG_ENABLED
struct SceneTreeDebugAccessor final : public ISceneTreeDebugAccessor{
    SceneTree *m_parent;
    Map<int, NodePath> live_edit_node_path_cache;
    Map<int, String> live_edit_resource_cache;

    NodePath live_edit_root;
    String live_edit_scene;

    Map<String, Set<Node *>> live_scene_edit_cache;
    Map<Node *, Map<ObjectID, Node *>> live_edit_remove_list;

    SceneTreeDebugAccessor(SceneTree *p) : m_parent(p) {

        live_edit_root = NodePath("/root");

    }

    void _live_edit_node_path_func(const NodePath &p_path, int p_id) { live_edit_node_path_cache[p_id] = p_path; }

    void _live_edit_res_path_func(se_string_view p_path, int p_id) { live_edit_resource_cache[p_id] = p_path; }

    void _live_edit_node_set_func(int p_id, const StringName &p_prop, const Variant &p_value) {

        if (!live_edit_node_path_cache.contains(p_id)) return;

        NodePath np = live_edit_node_path_cache[p_id];
        Node *base = nullptr;
        if (m_parent->get_root()->has_node(live_edit_root)) base = m_parent->get_root()->get_node(live_edit_root);

        Map<String, Set<Node *>>::iterator E = live_scene_edit_cache.find(live_edit_scene);
        if (E == live_scene_edit_cache.end()) return; // scene not editable

        for (Node *n : E->second) {

            if (base && !base->is_a_parent_of(n)) continue;

            if (!n->has_node(np)) continue;
            Node *n2 = n->get_node(np);

            n2->set(p_prop, p_value);
        }
    }

    void _live_edit_node_set_res_func(int p_id, const StringName &p_prop, se_string_view p_value) {

        RES r(ResourceLoader::load(p_value));
        if (not r) return;
        _live_edit_node_set_func(p_id, p_prop, r);
    }
    void _live_edit_node_call_func(int p_id, const StringName &p_method, VARIANT_ARG_DECLARE) {

        if (!live_edit_node_path_cache.contains(p_id)) return;

        NodePath np = live_edit_node_path_cache[p_id];
        Node *base = nullptr;
        if (m_parent->get_root()->has_node(live_edit_root)) base = m_parent->get_root()->get_node(live_edit_root);

        Map<String, Set<Node *>>::iterator E = live_scene_edit_cache.find(live_edit_scene);
        if (E == live_scene_edit_cache.end()) return; // scene not editable

        for (Node *n : E->second) {

            if (base && !base->is_a_parent_of(n)) continue;

            if (!n->has_node(np)) continue;
            Node *n2 = n->get_node(np);

            n2->call(p_method, VARIANT_ARG_PASS);
        }
    }
    void _live_edit_res_set_func(int p_id, const StringName &p_prop, const Variant &p_value) {

        if (!live_edit_resource_cache.contains(p_id)) return;

        se_string_view resp = live_edit_resource_cache[p_id];

        if (!ResourceCache::has(resp)) return;

        RES r(ResourceCache::get(resp));
        if (not r) return;

        r->set(p_prop, p_value);
    }
    void _live_edit_res_set_res_func(int p_id, const StringName &p_prop, se_string_view p_value) {

        RES r(ResourceLoader::load(p_value));
        if (not r) return;
        _live_edit_res_set_func(p_id, p_prop, r);
    }
    void _live_edit_res_call_func(int p_id, const StringName &p_method, VARIANT_ARG_DECLARE) {

        if (!live_edit_resource_cache.contains(p_id)) return;

        String resp = live_edit_resource_cache[p_id];

        if (!ResourceCache::has(resp)) return;

        RES r(ResourceCache::get(resp));
        if (not r) return;

        r->call(p_method, VARIANT_ARG_PASS);
    }

    void _live_edit_root_func(const NodePath &p_scene_path, se_string_view p_scene_from) {

        live_edit_root = p_scene_path;
        live_edit_scene = p_scene_from;
    }

    void _live_edit_create_node_func(const NodePath &p_parent, const String &p_type, const String &p_name) {

        Node *base = nullptr;
        if (m_parent->get_root()->has_node(live_edit_root)) base = m_parent->get_root()->get_node(live_edit_root);

        Map<String, Set<Node *>>::iterator E = live_scene_edit_cache.find(live_edit_scene);
        if (E == live_scene_edit_cache.end()) return; // scene not editable

        for (Node *n : E->second) {

            if (base && !base->is_a_parent_of(n)) continue;

            if (!n->has_node(p_parent)) continue;
            Node *n2 = n->get_node(p_parent);

            Node *no = object_cast<Node>(ClassDB::instance(StringName(p_type)));
            if (!no) {
                continue;
            }

            no->set_name(p_name);
            n2->add_child(no);
        }
    }
    void _live_edit_instance_node_func(const NodePath &p_parent, se_string_view p_path, const String &p_name) {

        Ref<PackedScene> ps = dynamic_ref_cast<PackedScene>(ResourceLoader::load(p_path));

        if (not ps) return;

        Node *base = nullptr;
        if (m_parent->get_root()->has_node(live_edit_root)) base = m_parent->get_root()->get_node(live_edit_root);

        Map<String, Set<Node *>>::iterator E = live_scene_edit_cache.find(live_edit_scene);
        if (E == live_scene_edit_cache.end()) return; // scene not editable

        for (Node *n : E->second) {

            if (base && !base->is_a_parent_of(n)) continue;

            if (!n->has_node(p_parent)) continue;
            Node *n2 = n->get_node(p_parent);

            Node *no = ps->instance();
            if (!no) {
                continue;
            }

            no->set_name(p_name);
            n2->add_child(no);
        }
    }
    void _live_edit_remove_node_func(const NodePath &p_at) {

        Node *base = nullptr;
        if (m_parent->get_root()->has_node(live_edit_root)) base = m_parent->get_root()->get_node(live_edit_root);

        Map<String, Set<Node *>>::iterator E = live_scene_edit_cache.find(live_edit_scene);
        if (E == live_scene_edit_cache.end()) return; // scene not editable

        for (Set<Node *>::iterator F = E->second.begin(); F != E->second.end();) {

            Set<Node *>::iterator N = F;
            ++N;

            Node *n = *F;

            if (base && !base->is_a_parent_of(n)) continue;

            if (!n->has_node(p_at)) continue;
            Node *n2 = n->get_node(p_at);

            memdelete(n2);

            F = N;
        }
    }
    void _live_edit_remove_and_keep_node_func(const NodePath &p_at, ObjectID p_keep_id) {

        Node *base = nullptr;
        if (m_parent->get_root()->has_node(live_edit_root)) base = m_parent->get_root()->get_node(live_edit_root);

        Map<String, Set<Node *>>::iterator E = live_scene_edit_cache.find(live_edit_scene);
        if (E == live_scene_edit_cache.end()) return; // scene not editable

        for (Set<Node *>::iterator F = E->second.begin(); F != E->second.end();) {

            Set<Node *>::iterator N = F;
            N++;
            Node *n = *F;

            if (base && !base->is_a_parent_of(n)) continue;

            if (!n->has_node(p_at)) continue;

            Node *n2 = n->get_node(p_at);

            n2->get_parent()->remove_child(n2);

            live_edit_remove_list[n][p_keep_id] = n2;

            F = N;
        }
    }
    void _live_edit_restore_node_func(ObjectID p_id, const NodePath &p_at, int p_at_pos) {

        Node *base = nullptr;
        if (m_parent->get_root()->has_node(live_edit_root)) base = m_parent->get_root()->get_node(live_edit_root);

        Map<String, Set<Node *>>::iterator E = live_scene_edit_cache.find(live_edit_scene);
        if (E == live_scene_edit_cache.end()) return; // scene not editable

        for (Set<Node *>::iterator F = E->second.begin(); F != E->second.end();) {

            Set<Node *>::iterator N = F;
            N++;

            Node *n = *F;

            if (base && !base->is_a_parent_of(n)) continue;

            if (!n->has_node(p_at)) continue;
            Node *n2 = n->get_node(p_at);

            Map<Node *, Map<ObjectID, Node *>>::iterator EN = live_edit_remove_list.find(n);

            if (EN == live_edit_remove_list.end()) continue;

            Map<ObjectID, Node *>::iterator FN = EN->second.find(p_id);

            if (FN == EN->second.end()) continue;
            n2->add_child(FN->second);

            EN->second.erase(FN);

            if (EN->second.empty()) {
                live_edit_remove_list.erase(EN);
            }

            F = N;
        }
    }
    void _live_edit_duplicate_node_func(const NodePath &p_at, const String &p_new_name) {

        Node *base = nullptr;
        if (m_parent->get_root()->has_node(live_edit_root)) base = m_parent->get_root()->get_node(live_edit_root);

        Map<String, Set<Node *>>::iterator E = live_scene_edit_cache.find(live_edit_scene);
        if (E == live_scene_edit_cache.end()) return; // scene not editable

        for (Node *n : E->second) {

            if (base && !base->is_a_parent_of(n)) continue;

            if (!n->has_node(p_at)) continue;
            Node *n2 = n->get_node(p_at);

            Node *dup = n2->duplicate(Node::DUPLICATE_SIGNALS | Node::DUPLICATE_GROUPS | Node::DUPLICATE_SCRIPTS);

            if (!dup) continue;

            dup->set_name(p_new_name);
            n2->get_parent()->add_child(dup);
        }
    }
    void _live_edit_reparent_node_func(
            const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos) {

        Node *base = nullptr;
        if (m_parent->get_root()->has_node(live_edit_root)) base = m_parent->get_root()->get_node(live_edit_root);

        Map<String, Set<Node *>>::iterator E = live_scene_edit_cache.find(live_edit_scene);
        if (E == live_scene_edit_cache.end()) return; // scene not editable

        for (Node *n : E->second) {

            if (base && !base->is_a_parent_of(n)) continue;

            if (!n->has_node(p_at)) continue;
            Node *nfrom = n->get_node(p_at);

            if (!n->has_node(p_new_place)) continue;
            Node *nto = n->get_node(p_new_place);

            nfrom->get_parent()->remove_child(nfrom);
            nfrom->set_name(p_new_name);

            nto->add_child(nfrom);
            if (p_at_pos >= 0) nto->move_child(nfrom, p_at_pos);
        }
    }
    Map<String, Set<Node *>> &get_live_scene_edit_cache() {
        return live_scene_edit_cache;
    }
    Map<Node *, Map<ObjectID, Node *>> &get_live_edit_remove_list() {
        return live_edit_remove_list;
    }


};
static void _fill_array(Node *p_node, Array &array, int p_level) {

    array.push_back(p_node->get_child_count());
    array.push_back(p_node->get_name());
    array.push_back(p_node->get_class());
    array.push_back(p_node->get_instance_id());
    for (int i = 0; i < p_node->get_child_count(); i++) {

        _fill_array(p_node->get_child(i), array, p_level + 1);
    }
}

void SceneTree::_debugger_request_tree() {

    Array arr;
    _fill_array(root, arr, 0);
    ScriptDebugger::get_singleton()->send_message("scene_tree", arr);
}
#endif

void SceneTreeTimer::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_time_left", {"time"}), &SceneTreeTimer::set_time_left);
    MethodBinder::bind_method(D_METHOD("get_time_left"), &SceneTreeTimer::get_time_left);

    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "time_left"), "set_time_left", "get_time_left");

    ADD_SIGNAL(MethodInfo("timeout"));
}

void SceneTreeTimer::set_time_left(float p_time) {
    time_left = p_time;
}

float SceneTreeTimer::get_time_left() const {
    return time_left;
}

void SceneTreeTimer::set_pause_mode_process(bool p_pause_mode_process) {
    process_pause = p_pause_mode_process;
}

bool SceneTreeTimer::is_pause_mode_process() {
    return process_pause;
}

void SceneTreeTimer::release_connections() {

    ListPOD<Connection> connections;
    get_all_signal_connections(&connections);

    for (Connection const &connection : connections) {
        disconnect(connection.signal, connection.target, connection.method);
    }
}

SceneTreeTimer::SceneTreeTimer() {
    time_left = 0;
    process_pause = true;
}

void SceneTree::tree_changed() {

    tree_version++;
    emit_signal(tree_changed_name);
}
#ifdef DEBUG_ENABLED
Map<String, Set<Node *>> &SceneTree::get_live_scene_edit_cache()
{
    return m_debug_data->get_live_scene_edit_cache();
}
Map<Node *, Map<ObjectID, Node *>> &SceneTree::get_live_edit_remove_list() {
    return m_debug_data->get_live_edit_remove_list();
}
#endif
void SceneTree::node_added(Node *p_node) {

    emit_signal(node_added_name, Variant(p_node));
}

void SceneTree::node_removed(Node *p_node) {

    if (current_scene == p_node) {
        current_scene = nullptr;
    }
    emit_signal(node_removed_name, Variant(p_node));
    if (call_lock > 0)
        call_skip.insert(p_node);
}

void SceneTree::node_renamed(Node *p_node) {

    emit_signal(node_renamed_name, Variant(p_node));
}

SceneTreeGroup *SceneTree::add_to_group(const StringName &p_group, Node *p_node) {

    Map<StringName, SceneTreeGroup>::iterator E = group_map.find(p_group);
    if (E==group_map.end()) {
        E = group_map.emplace(p_group, SceneTreeGroup()).first;
    }

    ERR_FAIL_COND_V_MSG(E->second.nodes.contains(p_node), &E->second, "Already in group: " + p_group + ".");
    E->second.nodes.push_back(p_node);
    //E->get().last_tree_version=0;
    E->second.changed = true;
    return &E->second;
}

void SceneTree::remove_from_group(const StringName &p_group, Node *p_node) {

    Map<StringName, SceneTreeGroup>::iterator E = group_map.find(p_group);
    ERR_FAIL_COND(E==group_map.end());

    E->second.nodes.erase_first(p_node);
    if (E->second.nodes.empty())
        group_map.erase(E);
}

void SceneTree::make_group_changed(const StringName &p_group) {
    Map<StringName, SceneTreeGroup>::iterator E = group_map.find(p_group);
    if (E!=group_map.end())
        E->second.changed = true;
}

void SceneTree::flush_transform_notifications() {

    SelfList<Node> *n = xform_change_list.first();
    while (n) {

        Node *node = n->self();
        SelfList<Node> *nx = n->next();
        xform_change_list.remove(n);
        n = nx;
        node->notification(NOTIFICATION_TRANSFORM_CHANGED);
    }
}

void SceneTree::_flush_ugc() {

    ugc_locked = true;

    while (!unique_group_calls.empty()) {

        auto E = unique_group_calls.begin();

        Variant v[VARIANT_ARG_MAX];
        for (int i = 0; i < E->second.size(); i++)
            v[i] = E->second[i];

        call_group_flags(GROUP_CALL_REALTIME, E->first.group, E->first.call, v[0], v[1], v[2], v[3], v[4]);

        unique_group_calls.erase(E);
    }

    ugc_locked = false;
}

void SceneTree::_update_group_order(SceneTreeGroup &g, bool p_use_priority) {

    if (!g.changed)
        return;
    if (g.nodes.empty())
        return;

    Node **nodes = g.nodes.data();
    int node_count = g.nodes.size();

    if (p_use_priority) {
        SortArray<Node *, Node::ComparatorWithPriority> node_sort;
        node_sort.sort(nodes, node_count);
    } else {
        SortArray<Node *, Node::Comparator> node_sort;
        node_sort.sort(nodes, node_count);
    }
    g.changed = false;
}

void SceneTree::call_group_flags(uint32_t p_call_flags, const StringName &p_group, const StringName &p_function, VARIANT_ARG_DECLARE) {

    Map<StringName, SceneTreeGroup>::iterator E = group_map.find(p_group);
    if (E==group_map.end())
        return;
    SceneTreeGroup &g = E->second;
    if (g.nodes.empty())
        return;

    if (p_call_flags & GROUP_CALL_UNIQUE && !(p_call_flags & GROUP_CALL_REALTIME)) {

        ERR_FAIL_COND(ugc_locked);

        UGCall ug;
        ug.call = p_function;
        ug.group = p_group;

        if (unique_group_calls.contains(ug))
            return;

        VARIANT_ARGPTRS

        PODVector<Variant> args;
        for (int i = 0; i < VARIANT_ARG_MAX; i++) {
            if (argptr[i]->get_type() == VariantType::NIL)
                break;
            args.push_back(*argptr[i]);
        }

        unique_group_calls[ug] = eastl::move(args);
        return;
    }

    _update_group_order(g);

    PODVector<Node *> nodes_copy = g.nodes;
    Node **nodes = nodes_copy.data();
    int node_count = nodes_copy.size();

    call_lock++;

    if (p_call_flags & GROUP_CALL_REVERSE) {

        for (int i = node_count - 1; i >= 0; i--) {

            if (call_lock && call_skip.contains(nodes[i]))
                continue;

            if (p_call_flags & GROUP_CALL_REALTIME) {
                if (p_call_flags & GROUP_CALL_MULTILEVEL)
                    nodes[i]->call_multilevel(p_function, VARIANT_ARG_PASS);
                else
                    nodes[i]->call(p_function, VARIANT_ARG_PASS);
            } else
                MessageQueue::get_singleton()->push_call(nodes[i], p_function, VARIANT_ARG_PASS);
        }

    } else {

        for (int i = 0; i < node_count; i++) {

            if (call_lock && call_skip.contains(nodes[i]))
                continue;

            if (p_call_flags & GROUP_CALL_REALTIME) {
                if (p_call_flags & GROUP_CALL_MULTILEVEL)
                    nodes[i]->call_multilevel(p_function, VARIANT_ARG_PASS);
                else
                    nodes[i]->call(p_function, VARIANT_ARG_PASS);
            } else
                MessageQueue::get_singleton()->push_call(nodes[i], p_function, VARIANT_ARG_PASS);
        }
    }

    call_lock--;
    if (call_lock == 0)
        call_skip.clear();
}

void SceneTree::notify_group_flags(uint32_t p_call_flags, const StringName &p_group, int p_notification) {

    Map<StringName, SceneTreeGroup>::iterator E = group_map.find(p_group);
    if (E==group_map.end())
        return;
    SceneTreeGroup &g = E->second;
    if (g.nodes.empty())
        return;

    _update_group_order(g);

    PODVector<Node *> nodes_copy = g.nodes;
    Node **nodes = nodes_copy.data();
    int node_count = nodes_copy.size();

    call_lock++;

    if (p_call_flags & GROUP_CALL_REVERSE) {

        for (int i = node_count - 1; i >= 0; i--) {

            if (call_lock && call_skip.contains(nodes[i]))
                continue;

            if (p_call_flags & GROUP_CALL_REALTIME)
                nodes[i]->notification(p_notification);
            else
                MessageQueue::get_singleton()->push_notification(nodes[i], p_notification);
        }

    } else {

        for (int i = 0; i < node_count; i++) {

            if (call_lock && call_skip.contains(nodes[i]))
                continue;

            if (p_call_flags & GROUP_CALL_REALTIME)
                nodes[i]->notification(p_notification);
            else
                MessageQueue::get_singleton()->push_notification(nodes[i], p_notification);
        }
    }

    call_lock--;
    if (call_lock == 0)
        call_skip.clear();
}

void SceneTree::set_group_flags(uint32_t p_call_flags, const StringName &p_group, const StringName &p_name, const Variant &p_value) {

    Map<StringName, SceneTreeGroup>::iterator E = group_map.find(p_group);
    if (E==group_map.end())
        return;
    SceneTreeGroup &g = E->second;
    if (g.nodes.empty())
        return;

    _update_group_order(g);

    PODVector<Node *> nodes_copy = g.nodes;
    Node **nodes = nodes_copy.data();
    int node_count = nodes_copy.size();

    call_lock++;

    if (p_call_flags & GROUP_CALL_REVERSE) {

        for (int i = node_count - 1; i >= 0; i--) {

            if (call_lock && call_skip.contains(nodes[i]))
                continue;

            if (p_call_flags & GROUP_CALL_REALTIME)
                nodes[i]->set(p_name, p_value);
            else
                MessageQueue::get_singleton()->push_set(nodes[i], p_name, p_value);
        }

    } else {

        for (int i = 0; i < node_count; i++) {

            if (call_lock && call_skip.contains(nodes[i]))
                continue;

            if (p_call_flags & GROUP_CALL_REALTIME)
                nodes[i]->set(p_name, p_value);
            else
                MessageQueue::get_singleton()->push_set(nodes[i], p_name, p_value);
        }
    }

    call_lock--;
    if (call_lock == 0)
        call_skip.clear();
}

void SceneTree::call_group(const StringName &p_group, const StringName &p_function, VARIANT_ARG_DECLARE) {
    call_group_flags(0, p_group, p_function, VARIANT_ARG_PASS);
}

void SceneTree::notify_group(const StringName &p_group, int p_notification) {

    notify_group_flags(0, p_group, p_notification);
}

void SceneTree::set_group(const StringName &p_group, const StringName &p_name, const Variant &p_value) {

    set_group_flags(0, p_group, p_name, p_value);
}

void SceneTree::set_input_as_handled() {

    input_handled = true;
}

void SceneTree::input_text(se_string_view p_text) {

    root_lock++;

    call_group_flags(GROUP_CALL_REALTIME, "_viewports", "_vp_input_text", p_text); //special one for GUI, as controls use their own process check

    root_lock--;
}

bool SceneTree::is_input_handled() {
    return input_handled;
}

void SceneTree::input_event(const Ref<InputEvent> &p_event) {

    if (Engine::get_singleton()->is_editor_hint() && (object_cast<InputEventJoypadButton>(p_event.get()) || object_cast<InputEventJoypadMotion>(p_event.get())))
        return; //avoid joy input on editor

    current_event++;
    root_lock++;

    input_handled = false;

    // Don't make const ref unless you can find and fix what caused GH-34691.
    Ref<InputEvent> ev = p_event;

    MainLoop::input_event(ev);

    call_group_flags(GROUP_CALL_REALTIME, "_viewports", "_vp_input", ev); //special one for GUI, as controls use their own process check

    if (ScriptDebugger::get_singleton() && ScriptDebugger::get_singleton()->is_remote()) {
        //quit from game window using F8
        Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(ev);
        if (k && k->is_pressed() && !k->is_echo() && k->get_scancode() == KEY_F8) {
            ScriptDebugger::get_singleton()->request_quit();
        }
    }

    _flush_ugc();
    root_lock--;
    //MessageQueue::get_singleton()->flush(); //flushing here causes UI and other places slowness

    root_lock++;

    if (!input_handled) {
        call_group_flags(GROUP_CALL_REALTIME, "_viewports", "_vp_unhandled_input", ev); //special one for GUI, as controls use their own process check
        _flush_ugc();
        //		input_handled = true; - no reason to set this as handled
        root_lock--;
        //MessageQueue::get_singleton()->flush(); //flushing here causes UI and other places slowness
    } else {
        //		input_handled = true; - no reason to set this as handled
        root_lock--;
    }

    _call_idle_callbacks();
}

void SceneTree::init() {

    initialized = true;
    root->_set_tree(this);
    MainLoop::init();
}

bool SceneTree::iteration(float p_time) {

    root_lock++;

    current_frame++;

    flush_transform_notifications();

    MainLoop::iteration(p_time);
    physics_process_time = p_time;

    emit_signal("physics_frame");

    _notify_group_pause("physics_process_internal", Node::NOTIFICATION_INTERNAL_PHYSICS_PROCESS);
    _notify_group_pause("physics_process", Node::NOTIFICATION_PHYSICS_PROCESS);
    _flush_ugc();
    MessageQueue::get_singleton()->flush(); //small little hack
    flush_transform_notifications();
    call_group_flags(GROUP_CALL_REALTIME, "_viewports", "update_worlds");
    root_lock--;

    _flush_delete_queue();
    _call_idle_callbacks();

    return _quit;
}

void SceneTree::_update_font_oversampling(float p_ratio) {

    if (use_font_oversampling) {
        DynamicFontAtSize::font_oversampling = p_ratio;
        DynamicFont::update_oversampling();
    }
}

bool SceneTree::idle(float p_time) {
    SCOPE_AUTONAMED
    //print_line("ram: "+itos(OS::get_singleton()->get_static_memory_usage())+" sram: "+itos(OS::get_singleton()->get_dynamic_memory_usage()));
    //print_line("node count: "+itos(get_node_count()));
    //print_line("TEXTURE RAM: "+itos(VisualServer::get_singleton()->get_render_info(VS::INFO_TEXTURE_MEM_USED)));

    root_lock++;

    MainLoop::idle(p_time);

    idle_process_time = p_time;

    if (multiplayer_poll) {
        multiplayer->poll();
    }

    emit_signal("idle_frame");

    MessageQueue::get_singleton()->flush(); //small little hack

    flush_transform_notifications();

    _notify_group_pause("idle_process_internal", Node::NOTIFICATION_INTERNAL_PROCESS);
    _notify_group_pause("idle_process", Node::NOTIFICATION_PROCESS);

    Size2 win_size = Size2(OS::get_singleton()->get_window_size().width, OS::get_singleton()->get_window_size().height);

    if (win_size != last_screen_size) {

        last_screen_size = win_size;
        _update_root_rect();
        emit_signal("screen_resized");
    }

    _flush_ugc();
    MessageQueue::get_singleton()->flush(); //small little hack
    flush_transform_notifications(); //transforms after world update, to avoid unnecessary enter/exit notifications
    call_group_flags(GROUP_CALL_REALTIME, "_viewports", "update_worlds");

    root_lock--;

    _flush_delete_queue();

    //go through timers

    auto L = &timers.back(); //last element
    //break on last, so if new timers were added during list traversal, ignore them.
    for (auto E = timers.begin(); E!=timers.end() && (&(*E) == L);) {

        if (pause && !(*E)->is_pause_mode_process()) {
            ++E;
            continue;
        }
        float time_left = (*E)->get_time_left();
        time_left -= p_time;
        (*E)->set_time_left(time_left);

        if (time_left < 0) {
            (*E)->emit_signal("timeout");
            E=timers.erase(E);
        }
        else
            ++E;
    }

    flush_transform_notifications(); //additional transforms after timers update

    _call_idle_callbacks();

#ifdef TOOLS_ENABLED

    if (Engine::get_singleton()->is_editor_hint()) {
        //simple hack to reload fallback environment if it changed from editor
        String env_path = ProjectSettings::get_singleton()->get("rendering/environment/default_environment");
        env_path =StringUtils::strip_edges( env_path); //user may have added a space or two
        se_string_view cpath;
        Ref<Environment> fallback = get_root()->get_world()->get_fallback_environment();
        if (fallback) {
            cpath = fallback->get_path();
        }
        if (se_string_view(env_path) != cpath) {

            if (!env_path.empty()) {
                fallback = dynamic_ref_cast<Environment>(ResourceLoader::load(env_path));
                if (not fallback) {
                    //could not load fallback, set as empty
                    ProjectSettings::get_singleton()->set("rendering/environment/default_environment", "");
                }
            } else {
                fallback.unref();
            }
            get_root()->get_world()->set_fallback_environment(fallback);
        }
    }

#endif

    return _quit;
}

void SceneTree::finish() {

    _flush_delete_queue();

    _flush_ugc();

    initialized = false;

    MainLoop::finish();

    if (root) {
        root->_set_tree(nullptr);
        root->_propagate_after_exit_tree();
        memdelete(root); //delete root
        root = nullptr;
    }
    // cleanup timers
    for (const Ref<SceneTreeTimer> &E : timers) {
        E->release_connections();
    }
    timers.clear();
}

void SceneTree::quit(int p_exit_code) {
    if (p_exit_code >= 0) {
        // Override the exit code if a positive argument is given (the default is `-1`).
        // This is a shorthand for calling `set_exit_code()` on the OS singleton then quitting.
        OS::get_singleton()->set_exit_code(p_exit_code);
    }

    _quit = true;
}

void SceneTree::_notification(int p_notification) {

    switch (p_notification) {

        case NOTIFICATION_WM_QUIT_REQUEST: {

            get_root()->propagate_notification(p_notification);

            if (accept_quit) {
                _quit = true;
                break;
            }
        } break;
        case NOTIFICATION_WM_GO_BACK_REQUEST: {

            get_root()->propagate_notification(p_notification);

            if (quit_on_go_back) {
                _quit = true;
                break;
            }
        } break;

        case NOTIFICATION_WM_FOCUS_IN: {

            InputDefault *id = object_cast<InputDefault>(Input::get_singleton());
            if (id) {
                id->ensure_touch_mouse_raised();
            }

            get_root()->propagate_notification(p_notification);
        } break;
        case NOTIFICATION_TRANSLATION_CHANGED: {
            if (!Engine::get_singleton()->is_editor_hint()) {
                get_root()->propagate_notification(p_notification);
            }
        } break;
        case NOTIFICATION_WM_UNFOCUS_REQUEST: {

            notify_group_flags(GROUP_CALL_REALTIME | GROUP_CALL_MULTILEVEL, "input", NOTIFICATION_WM_UNFOCUS_REQUEST);

            get_root()->propagate_notification(p_notification);

        } break;

        case NOTIFICATION_OS_MEMORY_WARNING:
        case NOTIFICATION_OS_IME_UPDATE:
        case NOTIFICATION_WM_MOUSE_ENTER:
        case NOTIFICATION_WM_MOUSE_EXIT:
        case NOTIFICATION_WM_FOCUS_OUT:
        case NOTIFICATION_WM_ABOUT:
        case NOTIFICATION_CRASH:
        case NOTIFICATION_APP_RESUMED:
        case NOTIFICATION_APP_PAUSED: {

            get_root()->propagate_notification(p_notification);
        } break;

        default:
            break;
    }
};

void SceneTree::set_auto_accept_quit(bool p_enable) {

    accept_quit = p_enable;
}

void SceneTree::set_quit_on_go_back(bool p_enable) {

    quit_on_go_back = p_enable;
}

#ifdef TOOLS_ENABLED

bool SceneTree::is_node_being_edited(const Node *p_node) const {

    return Engine::get_singleton()->is_editor_hint() && edited_scene_root && (edited_scene_root->is_a_parent_of(p_node) || edited_scene_root == p_node);
}
#endif

#ifdef DEBUG_ENABLED
void SceneTree::set_debug_collisions_hint(bool p_enabled) {

    debug_collisions_hint = p_enabled;
}

bool SceneTree::is_debugging_collisions_hint() const {

    return debug_collisions_hint;
}

void SceneTree::set_debug_navigation_hint(bool p_enabled) {

    debug_navigation_hint = p_enabled;
}

bool SceneTree::is_debugging_navigation_hint() const {

    return debug_navigation_hint;
}
#endif

void SceneTree::set_debug_collisions_color(const Color &p_color) {

    debug_collisions_color = p_color;
}

Color SceneTree::get_debug_collisions_color() const {

    return debug_collisions_color;
}

void SceneTree::set_debug_collision_contact_color(const Color &p_color) {

    debug_collision_contact_color = p_color;
}

Color SceneTree::get_debug_collision_contact_color() const {

    return debug_collision_contact_color;
}

void SceneTree::set_debug_navigation_color(const Color &p_color) {

    debug_navigation_color = p_color;
}

Color SceneTree::get_debug_navigation_color() const {

    return debug_navigation_color;
}

void SceneTree::set_debug_navigation_disabled_color(const Color &p_color) {

    debug_navigation_disabled_color = p_color;
}

Color SceneTree::get_debug_navigation_disabled_color() const {

    return debug_navigation_disabled_color;
}

Ref<Material> SceneTree::get_debug_navigation_material() {

    if (navigation_material)
        return navigation_material;

    Ref<SpatialMaterial> line_material(make_ref_counted<SpatialMaterial>());
    line_material->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
    line_material->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
    line_material->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
    line_material->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    line_material->set_albedo(get_debug_navigation_color());

    navigation_material = line_material;

    return navigation_material;
}

Ref<Material> SceneTree::get_debug_navigation_disabled_material() {

    if (navigation_disabled_material)
        return navigation_disabled_material;

    Ref<SpatialMaterial> line_material(make_ref_counted<SpatialMaterial>());
    line_material->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
    line_material->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
    line_material->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
    line_material->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    line_material->set_albedo(get_debug_navigation_disabled_color());

    navigation_disabled_material = line_material;

    return navigation_disabled_material;
}
Ref<Material> SceneTree::get_debug_collision_material() {

    if (collision_material)
        return collision_material;

    Ref<SpatialMaterial> line_material(make_ref_counted<SpatialMaterial>());
    line_material->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
    line_material->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
    line_material->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
    line_material->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    line_material->set_albedo(get_debug_collisions_color());

    collision_material = line_material;

    return collision_material;
}

Ref<ArrayMesh> SceneTree::get_debug_contact_mesh() {

    if (debug_contact_mesh)
        return debug_contact_mesh;

    debug_contact_mesh = make_ref_counted<ArrayMesh>();

    Ref<SpatialMaterial> mat(make_ref_counted<SpatialMaterial>());
    mat->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
    mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
    mat->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
    mat->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_albedo(get_debug_collision_contact_color());

    Vector3 diamond[6] = {
        Vector3(-1, 0, 0),
        Vector3(1, 0, 0),
        Vector3(0, -1, 0),
        Vector3(0, 1, 0),
        Vector3(0, 0, -1),
        Vector3(0, 0, 1)
    };

    /* clang-format off */
    int diamond_faces[8 * 3] = {
        0, 2, 4,
        0, 3, 4,
        1, 2, 4,
        1, 3, 4,
        0, 2, 5,
        0, 3, 5,
        1, 2, 5,
        1, 3, 5,
    };
    /* clang-format on */

    PODVector<int> indices;
    for (int i = 0; i < 8 * 3; i++)
        indices.push_back(diamond_faces[i]);

    PODVector<Vector3> vertices;
    for (int i = 0; i < 6; i++)
        vertices.push_back(diamond[i] * 0.1f);

    SurfaceArrays arr(eastl::move(vertices));
    arr.m_indices = eastl::move(indices);

    debug_contact_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, eastl::move(arr));
    debug_contact_mesh->surface_set_material(0, mat);

    return debug_contact_mesh;
}

void SceneTree::set_pause(bool p_enabled) {

    if (p_enabled == pause)
        return;
    pause = p_enabled;
    PhysicsServer::get_singleton()->set_active(!p_enabled);
    Physics2DServer::get_singleton()->set_active(!p_enabled);
    if (get_root())
        get_root()->propagate_notification(p_enabled ? Node::NOTIFICATION_PAUSED : Node::NOTIFICATION_UNPAUSED);
}

bool SceneTree::is_paused() const {

    return pause;
}

void SceneTree::_call_input_pause(const StringName &p_group, const StringName &p_method, const Ref<InputEvent> &p_input) {

    Map<StringName, SceneTreeGroup>::iterator E = group_map.find(p_group);
    if (E==group_map.end())
        return;
    SceneTreeGroup &g = E->second;
    if (g.nodes.empty())
        return;

    _update_group_order(g);

    //copy, so copy on write happens in case something is removed from process while being called
    //performance is not lost because only if something is added/removed the vector is copied.
    PODVector<Node *> nodes_copy = g.nodes;

    int node_count = nodes_copy.size();
    Node **nodes = nodes_copy.data();

    Variant arg = p_input;
    const Variant *v[1] = { &arg };

    call_lock++;

    for (int i = node_count - 1; i >= 0; i--) {

        if (input_handled)
            break;

        Node *n = nodes[i];
        if (call_lock && call_skip.contains(n))
            continue;

        if (!n->can_process())
            continue;

        n->call_multilevel(p_method, (const Variant **)v, 1);
        //ERR_FAIL_COND();
    }

    call_lock--;
    if (call_lock == 0)
        call_skip.clear();
}

void SceneTree::_notify_group_pause(const StringName &p_group, int p_notification) {

    Map<StringName, SceneTreeGroup>::iterator E = group_map.find(p_group);
    if (E==group_map.end())
        return;
    SceneTreeGroup &g = E->second;
    if (g.nodes.empty())
        return;

    _update_group_order(g, p_notification == Node::NOTIFICATION_PROCESS || p_notification == Node::NOTIFICATION_INTERNAL_PROCESS || p_notification == Node::NOTIFICATION_PHYSICS_PROCESS || p_notification == Node::NOTIFICATION_INTERNAL_PHYSICS_PROCESS);

    //copy, so copy on write happens in case something is removed from process while being called
    //performance is not lost because only if something is added/removed the vector is copied.
    PODVector<Node *> nodes_copy = g.nodes;

    int node_count = nodes_copy.size();
    Node **nodes = nodes_copy.data();

    call_lock++;

    for (int i = 0; i < node_count; i++) {

        Node *n = nodes[i];
        if (call_lock && call_skip.contains(n))
            continue;

        if (!n->can_process())
            continue;
        if (!n->can_process_notification(p_notification))
            continue;

        n->notification(p_notification);
        //ERR_FAIL_COND();
    }

    call_lock--;
    if (call_lock == 0)
        call_skip.clear();
}

/*
void SceneMainLoop::_update_listener_2d() {

    if (listener_2d) {

        SpatialSound2DServer::get_singleton()->listener_set_space( listener_2d, world_2d->get_sound_space() );
    }

}
*/

Variant SceneTree::_call_group_flags(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {

    r_error.error = Variant::CallError::CALL_OK;

    ERR_FAIL_COND_V(p_argcount < 3, Variant());
    ERR_FAIL_COND_V(!p_args[0]->is_num(), Variant());
    ERR_FAIL_COND_V(p_args[1]->get_type() != VariantType::STRING, Variant());
    ERR_FAIL_COND_V(p_args[2]->get_type() != VariantType::STRING, Variant());

    int flags = *p_args[0];
    StringName group = *p_args[1];
    StringName method = *p_args[2];
    Variant v[VARIANT_ARG_MAX];

    for (int i = 0; i < MIN(p_argcount - 3, 5); i++) {

        v[i] = *p_args[i + 3];
    }

    call_group_flags(flags, group, method, v[0], v[1], v[2], v[3], v[4]);
    return Variant();
}

Variant SceneTree::_call_group(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {

    r_error.error = Variant::CallError::CALL_OK;

    ERR_FAIL_COND_V(p_argcount < 2, Variant());
    ERR_FAIL_COND_V(p_args[0]->get_type() != VariantType::STRING, Variant());
    ERR_FAIL_COND_V(p_args[1]->get_type() != VariantType::STRING, Variant());

    StringName group = *p_args[0];
    StringName method = *p_args[1];
    Variant v[VARIANT_ARG_MAX];

    for (int i = 0; i < MIN(p_argcount - 2, 5); i++) {

        v[i] = *p_args[i + 2];
    }

    call_group_flags(0, group, method, v[0], v[1], v[2], v[3], v[4]);
    return Variant();
}

int64_t SceneTree::get_frame() const {

    return current_frame;
}
int64_t SceneTree::get_event_count() const {

    return current_event;
}

Array SceneTree::_get_nodes_in_group(const StringName &p_group) {

    Array ret;
    Map<StringName, SceneTreeGroup>::iterator E = group_map.find(p_group);
    if (E==group_map.end())
        return ret;

    _update_group_order(E->second); //update order just in case
    int nc = E->second.nodes.size();
    if (nc == 0)
        return ret;

    ret.resize(nc);

    Node **ptr = E->second.nodes.data();
    for (int i = 0; i < nc; i++) {

        ret[i] = Variant(ptr[i]);
    }

    return ret;
}

bool SceneTree::has_group(const StringName &p_identifier) const {

    return group_map.contains(p_identifier);
}
void SceneTree::get_nodes_in_group(const StringName &p_group, Deque<Node *> *p_list) {

    Map<StringName, SceneTreeGroup>::iterator E = group_map.find(p_group);
    if (E==group_map.end())
        return;

    _update_group_order(E->second); //update order just in case
    int nc = E->second.nodes.size();
    if (nc == 0)
        return;
    Node **ptr = E->second.nodes.data();
    for (int i = 0; i < nc; i++) {

        p_list->push_back(ptr[i]);
    }
}

void SceneTree::_flush_delete_queue() {

    _THREAD_SAFE_METHOD_

    for(ObjectID id : delete_queue) {
        Object *obj = ObjectDB::get_instance(id);
        if (obj) {
            memdelete(obj);
        }
    }
    delete_queue.clear();
}

void SceneTree::queue_delete(Object *p_object) {

    _THREAD_SAFE_METHOD_
    ERR_FAIL_NULL(p_object);
    p_object->deleteLater();
    delete_queue.push_back(p_object->get_instance_id());
}

int SceneTree::get_node_count() const {

    return node_count;
}

void SceneTree::_update_root_rect() {

    if (stretch_mode == STRETCH_MODE_DISABLED) {

        _update_font_oversampling(1.0);
        root->set_size((last_screen_size / stretch_shrink).floor());
        root->set_attach_to_screen_rect(Rect2(Point2(), last_screen_size));
        root->set_size_override_stretch(false);
        root->set_size_override(false, Size2());
        root->update_canvas_items();
        return; //user will take care
    }

    //actual screen video mode
    Size2 video_mode = Size2(OS::get_singleton()->get_window_size().width, OS::get_singleton()->get_window_size().height);
    Size2 desired_res = stretch_min;

    Size2 viewport_size;
    Size2 screen_size;

    float viewport_aspect = desired_res.aspect();
    float video_mode_aspect = video_mode.aspect();

    if (use_font_oversampling && stretch_aspect == STRETCH_ASPECT_IGNORE) {
        WARN_PRINT("Font oversampling only works with the resize modes 'Keep Width', 'Keep Height', and 'Expand'.");
    }

    if (stretch_aspect == STRETCH_ASPECT_IGNORE || Math::is_equal_approx(viewport_aspect, video_mode_aspect)) {
        //same aspect or ignore aspect
        viewport_size = desired_res;
        screen_size = video_mode;
    } else if (viewport_aspect < video_mode_aspect) {
        // screen ratio is smaller vertically

        if (stretch_aspect == STRETCH_ASPECT_KEEP_HEIGHT || stretch_aspect == STRETCH_ASPECT_EXPAND) {

            //will stretch horizontally
            viewport_size.x = desired_res.y * video_mode_aspect;
            viewport_size.y = desired_res.y;
            screen_size = video_mode;

        } else {
            //will need black bars
            viewport_size = desired_res;
            screen_size.x = video_mode.y * viewport_aspect;
            screen_size.y = video_mode.y;
        }
    } else {
        //screen ratio is smaller horizontally
        if (stretch_aspect == STRETCH_ASPECT_KEEP_WIDTH || stretch_aspect == STRETCH_ASPECT_EXPAND) {

            //will stretch horizontally
            viewport_size.x = desired_res.x;
            viewport_size.y = desired_res.x / video_mode_aspect;
            screen_size = video_mode;

        } else {
            //will need black bars
            viewport_size = desired_res;
            screen_size.x = video_mode.x;
            screen_size.y = video_mode.x / viewport_aspect;
        }
    }

    screen_size = screen_size.floor();
    viewport_size = viewport_size.floor();

    Size2 margin;
    Size2 offset;
    //black bars and margin
    if (stretch_aspect != STRETCH_ASPECT_EXPAND && screen_size.x < video_mode.x) {
        margin.x = Math::round((video_mode.x - screen_size.x) / 2.0f);
        VisualServer::get_singleton()->black_bars_set_margins(margin.x, 0, margin.x, 0);
        offset.x = Math::round(margin.x * viewport_size.y / screen_size.y);
    } else if (stretch_aspect != STRETCH_ASPECT_EXPAND && screen_size.y < video_mode.y) {
        margin.y = Math::round((video_mode.y - screen_size.y) / 2.0f);
        VisualServer::get_singleton()->black_bars_set_margins(0, margin.y, 0, margin.y);
        offset.y = Math::round(margin.y * viewport_size.x / screen_size.x);
    } else {
        VisualServer::get_singleton()->black_bars_set_margins(0, 0, 0, 0);
    }

    switch (stretch_mode) {
        case STRETCH_MODE_DISABLED: {
            // Already handled above
            _update_font_oversampling(1.0);
        } break;
        case STRETCH_MODE_2D: {

            _update_font_oversampling(screen_size.x / viewport_size.x); //screen / viewport radio drives oversampling
            root->set_size((screen_size / stretch_shrink).floor());
            root->set_attach_to_screen_rect(Rect2(margin, screen_size));
            root->set_size_override_stretch(true);
            root->set_size_override(true, (viewport_size / stretch_shrink).floor());
            root->update_canvas_items(); //force them to update just in case

        } break;
        case STRETCH_MODE_VIEWPORT: {

            _update_font_oversampling(1.0);
            root->set_size((viewport_size / stretch_shrink).floor());
            root->set_attach_to_screen_rect(Rect2(margin, screen_size));
            root->set_size_override_stretch(false);
            root->set_size_override(false, Size2());
            root->update_canvas_items(); //force them to update just in case

            if (use_font_oversampling) {
                WARN_PRINT("Font oversampling does not work in 'Viewport' stretch mode, only '2D'.");
            }

        } break;
    }
}

void SceneTree::set_screen_stretch(StretchMode p_mode, StretchAspect p_aspect, const Size2 &p_minsize, real_t p_shrink) {

    stretch_mode = p_mode;
    stretch_aspect = p_aspect;
    stretch_min = p_minsize;
    stretch_shrink = p_shrink;
    _update_root_rect();
}

void SceneTree::set_edited_scene_root(Node *p_node) {
#ifdef TOOLS_ENABLED
    edited_scene_root = p_node;
#endif
}

Node *SceneTree::get_edited_scene_root() const {

#ifdef TOOLS_ENABLED
    return edited_scene_root;
#else
    return NULL;
#endif
}

void SceneTree::set_current_scene(Node *p_scene) {

    ERR_FAIL_COND(p_scene && p_scene->get_parent() != root);
    current_scene = p_scene;
}

Node *SceneTree::get_current_scene() const {

    return current_scene;
}

void SceneTree::_change_scene(Node *p_to) {

    if (current_scene) {
        memdelete(current_scene);
        current_scene = nullptr;
    }
    // If we're quitting, abort.
    if (unlikely(_quit)) {
        if (p_to) { // Prevent memory leak.
            memdelete(p_to);
        }
        return;
    }

    if (p_to) {
        current_scene = p_to;
        root->add_child(p_to);
    }
}

Error SceneTree::change_scene(se_string_view p_path) {

    Ref<PackedScene> new_scene = dynamic_ref_cast<PackedScene>(ResourceLoader::load(p_path));
    if (not new_scene)
        return ERR_CANT_OPEN;

    return change_scene_to(new_scene);
}
Error SceneTree::change_scene_to(const Ref<PackedScene> &p_scene) {

    Node *new_scene = nullptr;
    if (p_scene) {
        new_scene = p_scene->instance();
        ERR_FAIL_COND_V(!new_scene, ERR_CANT_CREATE);
    }

    call_deferred("_change_scene", Variant(new_scene));
    return OK;
}
Error SceneTree::reload_current_scene() {

    ERR_FAIL_COND_V(!current_scene, ERR_UNCONFIGURED);
    se_string_view fname = current_scene->get_filename();
    return change_scene(fname);
}

void SceneTree::add_current_scene(Node *p_current) {

    current_scene = p_current;
    root->add_child(p_current);
}

void SceneTree::drop_files(const PODVector<String> &p_files, int p_from_screen) {

    emit_signal("files_dropped", Variant::from(p_files), p_from_screen);
    MainLoop::drop_files(p_files, p_from_screen);
}

void SceneTree::global_menu_action(const Variant &p_id, const Variant &p_meta) {

    emit_signal("global_menu_action", p_id, p_meta);
    MainLoop::global_menu_action(p_id, p_meta);
}

Ref<SceneTreeTimer> SceneTree::create_timer(float p_delay_sec, bool p_process_pause) {

    Ref<SceneTreeTimer> stt(make_ref_counted<SceneTreeTimer>());
    stt->set_pause_mode_process(p_process_pause);
    stt->set_time_left(p_delay_sec);
    timers.push_back(stt);
    return stt;
}

void SceneTree::_network_peer_connected(int p_id) {

    emit_signal("network_peer_connected", p_id);
}

void SceneTree::_network_peer_disconnected(int p_id) {

    emit_signal("network_peer_disconnected", p_id);
}

void SceneTree::_connected_to_server() {

    emit_signal("connected_to_server");
}

void SceneTree::_connection_failed() {

    emit_signal("connection_failed");
}

void SceneTree::_server_disconnected() {

    emit_signal("server_disconnected");
}

Ref<MultiplayerAPI> SceneTree::get_multiplayer() const {
    return multiplayer;
}

void SceneTree::set_multiplayer_poll_enabled(bool p_enabled) {
    multiplayer_poll = p_enabled;
}

bool SceneTree::is_multiplayer_poll_enabled() const {
    return multiplayer_poll;
}

void SceneTree::set_multiplayer(const Ref<MultiplayerAPI>& p_multiplayer) {
    ERR_FAIL_COND(not p_multiplayer);

    if (multiplayer) {
        multiplayer->disconnect("network_peer_connected", this, "_network_peer_connected");
        multiplayer->disconnect("network_peer_disconnected", this, "_network_peer_disconnected");
        multiplayer->disconnect("connected_to_server", this, "_connected_to_server");
        multiplayer->disconnect("connection_failed", this, "_connection_failed");
        multiplayer->disconnect("server_disconnected", this, "_server_disconnected");
    }

    multiplayer = p_multiplayer;
    multiplayer->set_root_node(root);

    multiplayer->connect("network_peer_connected", this, "_network_peer_connected");
    multiplayer->connect("network_peer_disconnected", this, "_network_peer_disconnected");
    multiplayer->connect("connected_to_server", this, "_connected_to_server");
    multiplayer->connect("connection_failed", this, "_connection_failed");
    multiplayer->connect("server_disconnected", this, "_server_disconnected");
}

void SceneTree::set_network_peer(const Ref<NetworkedMultiplayerPeer> &p_network_peer) {

    multiplayer->set_network_peer(p_network_peer);
}

Ref<NetworkedMultiplayerPeer> SceneTree::get_network_peer() const {

    return multiplayer->get_network_peer();
}

bool SceneTree::is_network_server() const {

    return multiplayer->is_network_server();
}

bool SceneTree::has_network_peer() const {
    return multiplayer->has_network_peer();
}

int SceneTree::get_network_unique_id() const {

    return multiplayer->get_network_unique_id();
}

PODVector<int> SceneTree::get_network_connected_peers() const {

    return multiplayer->get_network_connected_peers();
}

int SceneTree::get_rpc_sender_id() const {
    return multiplayer->get_rpc_sender_id();
}

void SceneTree::set_refuse_new_network_connections(bool p_refuse) {
    multiplayer->set_refuse_new_network_connections(p_refuse);
}

bool SceneTree::is_refusing_new_network_connections() const {
    return multiplayer->is_refusing_new_network_connections();
}

void SceneTree::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("get_root"), &SceneTree::get_root);
    MethodBinder::bind_method(D_METHOD("has_group", {"name"}), &SceneTree::has_group);

    MethodBinder::bind_method(D_METHOD("set_auto_accept_quit", {"enabled"}), &SceneTree::set_auto_accept_quit);
    MethodBinder::bind_method(D_METHOD("set_quit_on_go_back", {"enabled"}), &SceneTree::set_quit_on_go_back);

    MethodBinder::bind_method(D_METHOD("set_debug_collisions_hint", {"enable"}), &SceneTree::set_debug_collisions_hint);
    MethodBinder::bind_method(D_METHOD("is_debugging_collisions_hint"), &SceneTree::is_debugging_collisions_hint);
    MethodBinder::bind_method(D_METHOD("set_debug_navigation_hint", {"enable"}), &SceneTree::set_debug_navigation_hint);
    MethodBinder::bind_method(D_METHOD("is_debugging_navigation_hint"), &SceneTree::is_debugging_navigation_hint);

    MethodBinder::bind_method(D_METHOD("set_edited_scene_root", {"scene"}), &SceneTree::set_edited_scene_root);
    MethodBinder::bind_method(D_METHOD("get_edited_scene_root"), &SceneTree::get_edited_scene_root);

    MethodBinder::bind_method(D_METHOD("set_pause", {"enable"}), &SceneTree::set_pause);
    MethodBinder::bind_method(D_METHOD("is_paused"), &SceneTree::is_paused);
    MethodBinder::bind_method(D_METHOD("set_input_as_handled"), &SceneTree::set_input_as_handled);
    MethodBinder::bind_method(D_METHOD("is_input_handled"), &SceneTree::is_input_handled);

    MethodBinder::bind_method(D_METHOD("create_timer", {"time_sec", "pause_mode_process"}), &SceneTree::create_timer, {DEFVAL(true)});

    MethodBinder::bind_method(D_METHOD("get_node_count"), &SceneTree::get_node_count);
    MethodBinder::bind_method(D_METHOD("get_frame"), &SceneTree::get_frame);
    MethodBinder::bind_method(D_METHOD("quit", {"exit_code"}), &SceneTree::quit, {DEFVAL(-1)});

    MethodBinder::bind_method(D_METHOD("set_screen_stretch", {"mode", "aspect", "minsize", "shrink"}), &SceneTree::set_screen_stretch, {DEFVAL(1)});

    MethodBinder::bind_method(D_METHOD("queue_delete", {"obj"}), &SceneTree::queue_delete);

    MethodInfo mi("call_group_flags",
                  PropertyInfo(VariantType::INT, "flags"),
                  PropertyInfo(VariantType::STRING, "group"),
                  PropertyInfo(VariantType::STRING, "method"));

    MethodBinder::bind_vararg_method("call_group_flags", &SceneTree::_call_group_flags, eastl::move(mi));

    MethodBinder::bind_method(D_METHOD("notify_group_flags", {"call_flags", "group", "notification"}), &SceneTree::notify_group_flags);
    MethodBinder::bind_method(D_METHOD("set_group_flags", {"call_flags", "group", "property", "value"}), &SceneTree::set_group_flags);

    MethodInfo mi2("call_group",
                   PropertyInfo(VariantType::STRING, "group"),
                   PropertyInfo(VariantType::STRING, "method"));

    MethodBinder::bind_vararg_method( "call_group", &SceneTree::_call_group, eastl::move(mi2));

    MethodBinder::bind_method(D_METHOD("notify_group", {"group", "notification"}), &SceneTree::notify_group);
    MethodBinder::bind_method(D_METHOD("set_group", {"group", "property", "value"}), &SceneTree::set_group);

    MethodBinder::bind_method(D_METHOD("get_nodes_in_group", {"group"}), &SceneTree::_get_nodes_in_group);

    MethodBinder::bind_method(D_METHOD("set_current_scene", {"child_node"}), &SceneTree::set_current_scene);
    MethodBinder::bind_method(D_METHOD("get_current_scene"), &SceneTree::get_current_scene);

    MethodBinder::bind_method(D_METHOD("change_scene", {"path"}), &SceneTree::change_scene);
    MethodBinder::bind_method(D_METHOD("change_scene_to", {"packed_scene"}), &SceneTree::change_scene_to);

    MethodBinder::bind_method(D_METHOD("reload_current_scene"), &SceneTree::reload_current_scene);

    MethodBinder::bind_method(D_METHOD("_change_scene"), &SceneTree::_change_scene);

    MethodBinder::bind_method(D_METHOD("set_multiplayer", {"multiplayer"}), &SceneTree::set_multiplayer);
    MethodBinder::bind_method(D_METHOD("get_multiplayer"), &SceneTree::get_multiplayer);
    MethodBinder::bind_method(D_METHOD("set_multiplayer_poll_enabled", {"enabled"}), &SceneTree::set_multiplayer_poll_enabled);
    MethodBinder::bind_method(D_METHOD("is_multiplayer_poll_enabled"), &SceneTree::is_multiplayer_poll_enabled);
    MethodBinder::bind_method(D_METHOD("set_network_peer", {"peer"}), &SceneTree::set_network_peer);
    MethodBinder::bind_method(D_METHOD("get_network_peer"), &SceneTree::get_network_peer);
    MethodBinder::bind_method(D_METHOD("is_network_server"), &SceneTree::is_network_server);
    MethodBinder::bind_method(D_METHOD("has_network_peer"), &SceneTree::has_network_peer);
    MethodBinder::bind_method(D_METHOD("get_network_connected_peers"), &SceneTree::get_network_connected_peers);
    MethodBinder::bind_method(D_METHOD("get_network_unique_id"), &SceneTree::get_network_unique_id);
    MethodBinder::bind_method(D_METHOD("get_rpc_sender_id"), &SceneTree::get_rpc_sender_id);
    MethodBinder::bind_method(D_METHOD("set_refuse_new_network_connections", {"refuse"}), &SceneTree::set_refuse_new_network_connections);
    MethodBinder::bind_method(D_METHOD("is_refusing_new_network_connections"), &SceneTree::is_refusing_new_network_connections);
    MethodBinder::bind_method(D_METHOD("_network_peer_connected"), &SceneTree::_network_peer_connected);
    MethodBinder::bind_method(D_METHOD("_network_peer_disconnected"), &SceneTree::_network_peer_disconnected);
    MethodBinder::bind_method(D_METHOD("_connected_to_server"), &SceneTree::_connected_to_server);
    MethodBinder::bind_method(D_METHOD("_connection_failed"), &SceneTree::_connection_failed);
    MethodBinder::bind_method(D_METHOD("_server_disconnected"), &SceneTree::_server_disconnected);

    MethodBinder::bind_method(D_METHOD("set_use_font_oversampling", {"enable"}), &SceneTree::set_use_font_oversampling);
    MethodBinder::bind_method(D_METHOD("is_using_font_oversampling"), &SceneTree::is_using_font_oversampling);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "debug_collisions_hint"), "set_debug_collisions_hint", "is_debugging_collisions_hint");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "debug_navigation_hint"), "set_debug_navigation_hint", "is_debugging_navigation_hint");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "paused"), "set_pause", "is_paused");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "refuse_new_network_connections"), "set_refuse_new_network_connections", "is_refusing_new_network_connections");
    ADD_PROPERTY_DEFAULT("refuse_new_network_connections", false);
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_font_oversampling"), "set_use_font_oversampling", "is_using_font_oversampling");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "edited_scene_root", PropertyHint::ResourceType, "Node", 0), "set_edited_scene_root", "get_edited_scene_root");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "current_scene", PropertyHint::ResourceType, "Node", 0), "set_current_scene", "get_current_scene");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "network_peer", PropertyHint::ResourceType, "NetworkedMultiplayerPeer", 0), "set_network_peer", "get_network_peer");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "root", PropertyHint::ResourceType, "Node", 0), "", "get_root");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "multiplayer", PropertyHint::ResourceType, "MultiplayerAPI", 0), "set_multiplayer", "get_multiplayer");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "multiplayer_poll"), "set_multiplayer_poll_enabled", "is_multiplayer_poll_enabled");

    ADD_SIGNAL(MethodInfo("tree_changed"));
    ADD_SIGNAL(MethodInfo("node_added", PropertyInfo(VariantType::OBJECT, "node", PropertyHint::ResourceType, "Node")));
    ADD_SIGNAL(MethodInfo("node_removed", PropertyInfo(VariantType::OBJECT, "node", PropertyHint::ResourceType, "Node")));
    ADD_SIGNAL(MethodInfo("node_renamed", PropertyInfo(VariantType::OBJECT, "node", PropertyHint::ResourceType, "Node")));
    ADD_SIGNAL(MethodInfo("screen_resized"));
    ADD_SIGNAL(MethodInfo("node_configuration_warning_changed", PropertyInfo(VariantType::OBJECT, "node", PropertyHint::ResourceType, "Node")));

    ADD_SIGNAL(MethodInfo("idle_frame"));
    ADD_SIGNAL(MethodInfo("physics_frame"));

    ADD_SIGNAL(MethodInfo("files_dropped", PropertyInfo(VariantType::POOL_STRING_ARRAY, "files"), PropertyInfo(VariantType::INT, "screen")));
    ADD_SIGNAL(MethodInfo("global_menu_action", PropertyInfo(VariantType::NIL, "id"), PropertyInfo(VariantType::NIL, "meta")));
    ADD_SIGNAL(MethodInfo("network_peer_connected", PropertyInfo(VariantType::INT, "id")));
    ADD_SIGNAL(MethodInfo("network_peer_disconnected", PropertyInfo(VariantType::INT, "id")));
    ADD_SIGNAL(MethodInfo("connected_to_server"));
    ADD_SIGNAL(MethodInfo("connection_failed"));
    ADD_SIGNAL(MethodInfo("server_disconnected"));

    BIND_ENUM_CONSTANT(GROUP_CALL_DEFAULT)
    BIND_ENUM_CONSTANT(GROUP_CALL_REVERSE)
    BIND_ENUM_CONSTANT(GROUP_CALL_REALTIME)
    BIND_ENUM_CONSTANT(GROUP_CALL_UNIQUE)

    BIND_ENUM_CONSTANT(STRETCH_MODE_DISABLED)
    BIND_ENUM_CONSTANT(STRETCH_MODE_2D)
    BIND_ENUM_CONSTANT(STRETCH_MODE_VIEWPORT)

    BIND_ENUM_CONSTANT(STRETCH_ASPECT_IGNORE)
    BIND_ENUM_CONSTANT(STRETCH_ASPECT_KEEP)
    BIND_ENUM_CONSTANT(STRETCH_ASPECT_KEEP_WIDTH)
    BIND_ENUM_CONSTANT(STRETCH_ASPECT_KEEP_HEIGHT)
    BIND_ENUM_CONSTANT(STRETCH_ASPECT_EXPAND)
}

SceneTree *SceneTree::singleton = nullptr;

SceneTree::IdleCallback SceneTree::idle_callbacks[SceneTree::MAX_IDLE_CALLBACKS];
int SceneTree::idle_callback_count = 0;

void SceneTree::_call_idle_callbacks() {

    for (int i = 0; i < idle_callback_count; i++) {
        idle_callbacks[i]();
    }
}

void SceneTree::add_idle_callback(IdleCallback p_callback) {
    ERR_FAIL_COND(idle_callback_count >= MAX_IDLE_CALLBACKS);
    idle_callbacks[idle_callback_count++] = p_callback;
}

void SceneTree::set_use_font_oversampling(bool p_oversampling) {

    if (use_font_oversampling == p_oversampling)
        return;

    use_font_oversampling = p_oversampling;
    _update_root_rect();
}

bool SceneTree::is_using_font_oversampling() const {
    return use_font_oversampling;
}

void SceneTree::get_argument_options(const StringName &p_function, int p_idx, ListPOD<String> *r_options) const {
    using namespace PathUtils;

    if (p_function == "change_scene") {
        DirAccessRef dir_access = DirAccess::create(DirAccess::ACCESS_RESOURCES);
        List<String> directories;
        directories.push_back(dir_access->get_current_dir());

        while (!directories.empty()) {
            dir_access->change_dir(directories.back()->deref());
            directories.pop_back();

            dir_access->list_dir_begin();
            String filename = dir_access->get_next();

            while (!filename.empty()) {
                if (filename == "." || filename == "..") {
                    filename = dir_access->get_next();
                    continue;
                }

                if (dir_access->dir_exists(filename)) {
                    directories.push_back(plus_file(dir_access->get_current_dir(),filename));
                } else if (StringUtils::ends_with(filename,".tscn") || StringUtils::ends_with(filename,".scn")) {
                    r_options->push_back("\"" + plus_file(dir_access->get_current_dir(),filename) + "\"");
                }

                filename = dir_access->get_next();
            }
        }
    }
}
SceneTree::SceneTree() {
    __thread__safe__.reset(new Mutex);

    if (singleton == nullptr) singleton = this;
    _quit = false;
    accept_quit = true;
    quit_on_go_back = true;
    initialized = false;
    use_font_oversampling = false;
#ifdef DEBUG_ENABLED
    //TODO: SEGS: make it possible for m_debug_data to be nullptr and still be callable ?
    m_debug_data = new SceneTreeDebugAccessor(this);
    debug_collisions_hint = false;
    debug_navigation_hint = false;
#endif
    debug_collisions_color = GLOBAL_DEF("debug/shapes/collision/shape_color", Color(0.0, 0.6f, 0.7f, 0.5));
    debug_collision_contact_color = GLOBAL_DEF("debug/shapes/collision/contact_color", Color(1.0, 0.2f, 0.1f, 0.8f));
    debug_navigation_color = GLOBAL_DEF("debug/shapes/navigation/geometry_color", Color(0.1f, 1.0, 0.7f, 0.4f));
    debug_navigation_disabled_color = GLOBAL_DEF("debug/shapes/navigation/disabled_geometry_color", Color(1.0, 0.7f, 0.1f, 0.4f));
    collision_debug_contacts = GLOBAL_DEF("debug/shapes/collision/max_contacts_displayed", 10000);
    ProjectSettings::get_singleton()->set_custom_property_info("debug/shapes/collision/max_contacts_displayed", PropertyInfo(VariantType::INT, "debug/shapes/collision/max_contacts_displayed", PropertyHint::Range, "0,20000,1")); // No negative

    tree_version = 1;
    physics_process_time = 1;
    idle_process_time = 1;

    root = nullptr;
    input_handled = false;
    pause = false;
    current_frame = 0;
    current_event = 0;
    tree_changed_name = "tree_changed";
    node_added_name = "node_added";
    node_removed_name = "node_removed";
    node_renamed_name = "node_renamed";
    ugc_locked = false;
    call_lock = 0;
    root_lock = 0;
    node_count = 0;

    //create with mainloop

    root = memnew(Viewport);
    root->set_name("root");
    root->set_handle_input_locally(false);
    if (not root->get_world())
        root->set_world(make_ref_counted<World>());

    // Initialize network state
    multiplayer_poll = true;
    set_multiplayer(make_ref_counted<MultiplayerAPI>());

    //root->set_world_2d( Ref<World2D>( memnew( World2D )));
    root->set_as_audio_listener(true);
    root->set_as_audio_listener_2d(true);
    current_scene = nullptr;

    int ref_atlas_size = GLOBAL_DEF("rendering/quality/reflections/atlas_size", 2048);
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/reflections/atlas_size", PropertyInfo(VariantType::INT, "rendering/quality/reflections/atlas_size", PropertyHint::Range, "0,8192,or_greater")); //next_power_of_2 will return a 0 as min value
    int ref_atlas_subdiv = GLOBAL_DEF("rendering/quality/reflections/atlas_subdiv", 8);
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/reflections/atlas_subdiv", PropertyInfo(VariantType::INT, "rendering/quality/reflections/atlas_subdiv", PropertyHint::Range, "0,32,or_greater")); //next_power_of_2 will return a 0 as min value
    int msaa_mode = GLOBAL_DEF("rendering/quality/filters/msaa", 0);
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/filters/msaa", PropertyInfo(VariantType::INT, "rendering/quality/filters/msaa", PropertyHint::Enum, "Disabled,2x,4x,8x,16x,AndroidVR 2x,AndroidVR 4x"));
    root->set_msaa(Viewport::MSAA(msaa_mode));

    GLOBAL_DEF("rendering/quality/depth/hdr", true);
    GLOBAL_DEF("rendering/quality/depth/hdr.mobile", false);

    bool hdr = GLOBAL_GET("rendering/quality/depth/hdr");
    root->set_hdr(hdr);

    VisualServer::get_singleton()->scenario_set_reflection_atlas_size(root->get_world()->get_scenario(), ref_atlas_size, ref_atlas_subdiv);

    { //load default fallback environment
        //get possible extensions
        PODVector<String> exts;
        ResourceLoader::get_recognized_extensions_for_type("Environment", exts);
        String ext_hint;
        for (const String &E : exts) {
            if (!ext_hint.empty())
                ext_hint += ',';
            ext_hint += "*." + E;
        }
        //get path
        String env_path = GLOBAL_DEF("rendering/environment/default_environment", "");
        //setup property
        ProjectSettings::get_singleton()->set_custom_property_info("rendering/environment/default_environment",
                PropertyInfo(VariantType::STRING, "rendering/viewport/default_environment", PropertyHint::File, ext_hint));
        env_path =StringUtils::strip_edges( env_path);
        if (!env_path.empty()) {
            Ref<Environment> env = dynamic_ref_cast<Environment>(ResourceLoader::load(env_path));
            if (env) {
                root->get_world()->set_fallback_environment(env);
            } else {
                if (Engine::get_singleton()->is_editor_hint()) {
                    //file was erased, clear the field.
                    ProjectSettings::get_singleton()->set("rendering/environment/default_environment", "");
                } else {
                    //file was erased, notify user.
                    ERR_PRINT(RTR("Default Environment as specified in Project Settings (Rendering -> Environment -> Default Environment) could not be loaded."));
                }
            }
        }
    }

    stretch_mode = STRETCH_MODE_DISABLED;
    stretch_aspect = STRETCH_ASPECT_IGNORE;
    stretch_shrink = 1;

    last_screen_size = Size2(OS::get_singleton()->get_window_size().width, OS::get_singleton()->get_window_size().height);
    _update_root_rect();

    if (ScriptDebugger::get_singleton()) {
        if (ScriptDebugger::get_singleton()->is_remote()) {
            ScriptDebuggerRemote *remote_debugger = static_cast<ScriptDebuggerRemote *>(ScriptDebugger::get_singleton());

            remote_debugger->set_scene_tree(this);
        }
        ScriptDebugger::get_singleton()->set_multiplayer(multiplayer);
    }

    root->set_physics_object_picking(GLOBAL_DEF("physics/common/enable_object_picking", true));

#ifdef TOOLS_ENABLED
    edited_scene_root = nullptr;
#endif
}

SceneTree::~SceneTree() {
    if (root) {
        root->_set_tree(nullptr);
        root->_propagate_after_exit_tree();
        memdelete(root);
    }

    if (singleton == this) singleton = nullptr;
#ifdef DEBUG_ENABLED
    delete m_debug_data;
#endif
}
