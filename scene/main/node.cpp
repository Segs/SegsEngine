/*************************************************************************/
/*  node.cpp                                                             */
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

#include "node.h"

#include "instance_placeholder.h"
#include "viewport.h"

#include "core/core_string_names.h"
#include "core/debugger/script_debugger.h"
#include "core/ecs_registry.h"
#include "core/io/multiplayer_api.h"
#include "core/io/resource_loader.h"
#include "core/message_queue.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/resource/resource_manager.h"
#include "core/print_string.h"
#include "core/node_path.h"
#include "core/hash_map.h"
#include "core/resource/resource_manager.h"
#include "core/script_language.h"
#include "core/string_formatter.h"
#include "core/ustring.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/packed_scene.h"
#include "scene/scene_string_names.h"
#include "scene/2d/animated_sprite_2d.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#include "core/object_db.h"
#endif
#ifdef DEBUG_ENABLED
#include "core/os/mutex.h"
#include "entt/entity/registry.hpp"
extern ECS_Registry<GameEntity,true> game_object_registry;
#endif

#include "EASTL/vector_set.h"

IMPL_GDCLASS(Node)

//TODO: SEGS: duplicated VARIANT_ENUM_CAST
VARIANT_ENUM_CAST(MultiplayerAPI_RPCMode);

VARIANT_ENUM_CAST(Node::PauseMode);
VARIANT_ENUM_CAST(Node::DuplicateFlags);

int Node::orphan_node_count = 0;
StringView _get_name_num_separator() {
    switch (ProjectSettings::get_singleton()->get("node/name_num_separator").as<int>()) {
        case 0: return "";
        case 1: return " ";
        case 2: return "_";
        case 3: return "-";
    }
    return " ";
}
struct GroupData {
    SceneTreeGroup *group=nullptr;
    bool persistent = false;
};

struct Node::PrivData {
    struct NetData {
        StringName name;
        MultiplayerAPI_RPCMode mode;
    };
    HashMap<StringName, GroupData> grouped;
    Vector<Node *> owned;
    Vector<Node *> children; // list of children
    Vector<NetData> rpc_methods;
    Vector<NetData> rpc_properties;
    Ref<SceneState> instance_state;
    Ref<SceneState> inherited_state;
#ifdef TOOLS_ENABLED
    NodePath import_path; //path used when imported, used by scene editors to keep tracking
#endif

    String *filename=nullptr;
    Node *parent;
    Node *owner;
    Node *OW; // owned element
    Node *pause_owner;
    mutable NodePath *path_cache;
    StringName name;

    int pos;
    int depth;
    int network_master;
    //int process_priority;
    PauseMode pause_mode;


    // variables used to properly sort the node when processing, ignored otherwise
    //should move all the stuff below to bits
    bool physics_process : 1;
    bool idle_process : 1;

    bool physics_process_internal: 1;
    bool idle_process_internal: 1;

    bool input: 1;
    bool unhandled_input: 1;
    bool unhandled_key_input: 1;

    // For certain nodes (e.g. CPU Particles in global mode)
    // It can be useful to not send the instance transform to the
    // VisualServer, and specify the mesh in world space.
    bool use_identity_transform : 1;

    //bool parent_owned : 1;
    bool in_constructor: 1;
    bool use_placeholder: 1;

    bool display_folded: 1;
    bool editable_instance: 1;
    //bool inside_tree : 1;
    bool ready_notified : 1; // this is a small hack, so if a node is added during _ready() to the tree, it correctly
                             // gets the _ready() notification
    bool ready_first : 1;
    uint16_t get_node_rset_property_id(const StringName &p_property) const {
        for (int i = 0; i < rpc_properties.size(); i++) {
            if (rpc_properties[i].name == p_property) {
                // Returns `i` with the high bit set to 1 so we know that this id comes
                // from the node and not the script.
                return i | (1 << 15);
            }
        }
        return UINT16_MAX;
    }

    StringName get_node_rset_property(const uint16_t p_rset_property_id) const {
        // Make sure this is a node generated ID.
        if (((1 << 15) & p_rset_property_id) > 0) {
            int mid = (~(1 << 15)) & p_rset_property_id;
            if (mid < rpc_properties.size()) {
                return rpc_properties[mid].name;
            }
        }
        return StringName();
    }
    uint16_t get_node_rpc_method_id(const StringName &p_method) const {
        for (int i = 0; i < rpc_methods.size(); i++) {
            if (rpc_methods[i].name == p_method) {
                // Returns `i` with the high bit set to 1 so we know that this id comes
                // from the node and not the script.
                return i | (1 << 15);
            }
        }
        return UINT16_MAX;
    }
};


void Node::_notification(int p_notification) {

    switch (p_notification) {

        case NOTIFICATION_PROCESS: {

            if (get_script_instance()) {

                Variant time = get_process_delta_time();
                get_script_instance()->call(SceneStringNames::_process, time);
            }
        } break;
        case NOTIFICATION_PHYSICS_PROCESS: {

            if (get_script_instance()) {

                Variant time = get_physics_process_delta_time();
                get_script_instance()->call(SceneStringNames::_physics_process, time);
            }

        } break;
        case NOTIFICATION_ENTER_TREE: {
            ERR_FAIL_COND(!get_viewport());
            ERR_FAIL_COND(!get_tree());
            game_object_registry.registry.emplace_or_replace<SceneTreeLink>(get_instance_id(),get_tree());

            if (priv_data->pause_mode == PAUSE_MODE_INHERIT) {

                if (priv_data->parent)
                    priv_data->pause_owner = priv_data->parent->priv_data->pause_owner;
                else
                    priv_data->pause_owner = nullptr;
            } else {
                priv_data->pause_owner = this;
            }

            if (priv_data->input)
                add_to_group(StringName("_vp_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));
            if (priv_data->unhandled_input)
                add_to_group(StringName("_vp_unhandled_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));
            if (priv_data->unhandled_key_input)
                add_to_group(StringName("_vp_unhandled_key_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));

            get_tree()->node_count++;
            orphan_node_count--;

        } break;
        case NOTIFICATION_EXIT_TREE: {
            ERR_FAIL_COND(!get_viewport());
            ERR_FAIL_COND(!get_tree());

            game_object_registry.registry.remove<SceneTreeLink>(get_instance_id());
            get_tree()->node_count--;
            orphan_node_count++;

            if (priv_data->input)
                remove_from_group(StringName("_vp_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));
            if (priv_data->unhandled_input)
                remove_from_group(StringName("_vp_unhandled_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));
            if (priv_data->unhandled_key_input)
                remove_from_group(StringName("_vp_unhandled_key_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));

            priv_data->pause_owner = nullptr;
            memdelete(priv_data->path_cache);
            priv_data->path_cache = nullptr;
        } break;
        case NOTIFICATION_PATH_CHANGED: {
            memdelete(priv_data->path_cache);
            priv_data->path_cache = nullptr;
        } break;
        case NOTIFICATION_READY: {

            if (get_script_instance()) {

                if (get_script_instance()->has_method(SceneStringNames::_input)) {
                    set_process_input(true);
                }

                if (get_script_instance()->has_method(SceneStringNames::_unhandled_input)) {
                    set_process_unhandled_input(true);
                }

                if (get_script_instance()->has_method(SceneStringNames::_unhandled_key_input)) {
                    set_process_unhandled_key_input(true);
                }

                if (get_script_instance()->has_method(SceneStringNames::_process)) {
                    set_process(true);
                }

                if (get_script_instance()->has_method(SceneStringNames::_physics_process)) {
                    set_physics_process(true);
                }

                get_script_instance()->call(SceneStringNames::_ready);
            }

        } break;
        case NOTIFICATION_POSTINITIALIZE: {
            priv_data->in_constructor = false;
        } break;
        case NOTIFICATION_PREDELETE: {
            if (priv_data->parent) {
                priv_data->parent->remove_child(this);
            }

            // kill children as cleanly as possible
            while (!priv_data->children.empty()) {

                //begin from the end because its faster and more consistent with creation
                Node *child = priv_data->children[priv_data->children.size() - 1];
                memdelete(child);
            }

        } break;
    }
}

void Node::_propagate_ready() {

    priv_data->ready_notified = true;
    blocked++;
    for (size_t i = 0; i < priv_data->children.size(); i++) {

        priv_data->children[i]->_propagate_ready();
    }
    blocked--;

    notification(NOTIFICATION_POST_ENTER_TREE);

    if (priv_data->ready_first) {
        priv_data->ready_first = false;
        notification(NOTIFICATION_READY);
        emit_signal(SceneStringNames::ready);
    }
}

void Node::_propagate_enter_tree() {
    // this needs to happen to all children before any enter_tree

    if (priv_data->parent) {
        tree = priv_data->parent->tree;
        priv_data->depth = priv_data->parent->priv_data->depth + 1;
    } else {
        priv_data->depth = 1;
    }

    viewport = object_cast<Viewport>(this);
    if (!viewport && priv_data->parent)
        viewport = priv_data->parent->viewport;

    game_object_registry.registry.emplace_or_replace<InTreeMarkerComponent>(get_instance_id());
    inside_tree = true;

    for (eastl::pair<const StringName, GroupData> &E : priv_data->grouped) {
        E.second.group = tree->add_to_group(E.first, this);
    }

    notification(NOTIFICATION_ENTER_TREE);

    if (get_script_instance()) {
        get_script_instance()->call(SceneStringNames::_enter_tree);
    }

    //emit tree_entered();
    emit_signal(SceneStringNames::tree_entered);

    tree->node_added(this);

    if (priv_data->parent) {
        priv_data->parent->emit_signal(SceneStringNames::child_entered_tree, this);
    }
    blocked++;
    //block while adding children

    for (size_t i = 0; i < priv_data->children.size(); i++) {

        if (!priv_data->children[i]->is_inside_tree()) { // could have been added in enter_tree
            priv_data->children[i]->_propagate_enter_tree();
        }
    }

    blocked--;

#ifdef DEBUG_ENABLED

    if (ScriptDebugger::get_singleton() && priv_data->filename && !priv_data->filename->empty()) {
        //used for live edit
        tree->get_live_scene_edit_cache()[*priv_data->filename].insert(this);
    }
#endif
    // enter groups
}

void Node::_propagate_after_exit_branch(bool p_exiting_tree) {
    // Clear owner if it was not part of the pruned branch
    if (priv_data->owner) {
        bool found = false;
        Node *parent = priv_data->parent;

        while (parent) {
            if (parent == priv_data->owner) {
                found = true;
                break;
            }

            parent = parent->priv_data->parent;
        }

        if (!found) {
            priv_data->owner->priv_data->owned.erase_first(priv_data->OW);
            priv_data->owner = nullptr;
        }
    }

    blocked++;
    for (Node *i : priv_data->children) {
        i->_propagate_after_exit_branch(p_exiting_tree);
    }
    blocked--;

    if (p_exiting_tree) {
        emit_signal(SceneStringNames::tree_exited);
    }
}
void Node::_propagate_exit_tree() {

    //block while removing children

#ifdef DEBUG_ENABLED

    if (tree && ScriptDebugger::get_singleton() && priv_data->filename && !priv_data->filename->empty()) {
        //used for live edit
        auto E = tree->get_live_scene_edit_cache().find(*priv_data->filename);
        if (E!=tree->get_live_scene_edit_cache().end()) {
            E->second.erase(this);
            if (E->second.empty()) {
                tree->get_live_scene_edit_cache().erase(E);
            }
        }

        auto F = tree->get_live_edit_remove_list().find(this);
        if (F!=tree->get_live_edit_remove_list().end()) {
            for (eastl::pair<const GameEntity, Node *> &G : F->second) {

                memdelete(G.second);
            }
            tree->get_live_edit_remove_list().erase(F);
        }
    }
#endif
    blocked++;

    for (int i = priv_data->children.size() - 1; i >= 0; i--) {

        priv_data->children[i]->_propagate_exit_tree();
    }

    blocked--;

    if (get_script_instance()) {
        get_script_instance()->call(SceneStringNames::_exit_tree);
    }
    emit_signal(SceneStringNames::tree_exiting);

    notification(NOTIFICATION_EXIT_TREE, true);
    if (tree) {
        tree->node_removed(this);
    }

    if (priv_data->parent) {
        priv_data->parent->emit_signal(SceneStringNames::child_exiting_tree, this);
    }
    // exit groups

    for (eastl::pair<const StringName, GroupData> &E : priv_data->grouped) {
        if (tree)
            tree->remove_from_group(E.first, this);
        E.second.group = nullptr;
    }

    viewport = nullptr;

    if (tree)
        tree->tree_changed();

    game_object_registry.registry.remove<InTreeMarkerComponent>(get_instance_id());
    inside_tree = false;
    priv_data->ready_notified = false;
    tree = nullptr;
    priv_data->depth = -1;
}

void Node::move_child(Node *p_child, int p_pos) {

    ERR_FAIL_NULL(p_child);
    ERR_FAIL_INDEX_MSG(p_pos, priv_data->children.size() + 1, FormatVE("Invalid new child position: %d.", p_pos).c_str());
    ERR_FAIL_COND_MSG(p_child->priv_data->parent != this, "Child is not a child of this node.");
    ERR_FAIL_COND_MSG(blocked > 0, "Parent node is busy setting up children, move_child() failed. Consider using call_deferred(\"move_child\") instead (or \"popup\" if this is from a popup).");

    // Specifying one place beyond the end
    // means the same as moving to the last position
    if (p_pos == priv_data->children.size())
        p_pos--;

    if (p_child->priv_data->pos == p_pos)
        return; //do nothing

    int motion_from = MIN(p_pos, p_child->priv_data->pos);
    int motion_to = M_MAX(p_pos, p_child->priv_data->pos);

    priv_data->children.erase_at(p_child->priv_data->pos);
    priv_data->children.insert_at(p_pos, p_child);

    if (tree) {
        tree->tree_changed();
    }

    blocked++;
    //new pos first
    for (int i = motion_from; i <= motion_to; i++) {

        priv_data->children[i]->priv_data->pos = i;
    }
    // notification second
    move_child_notify(p_child);
    for (int i = motion_from; i <= motion_to; i++) {
        priv_data->children[i]->notification(NOTIFICATION_MOVED_IN_PARENT);
    }
    for (const eastl::pair<const StringName, GroupData> &E : p_child->priv_data->grouped) {
        if (E.second.group)
            E.second.group->changed = true;
    }

    blocked--;
}

void Node::raise() {

    if (!priv_data->parent) {
        return;
    }

    priv_data->parent->move_child(this, priv_data->parent->priv_data->children.size() - 1);
}

void Node::set_physics_process(bool p_process) {

    if (priv_data->physics_process == p_process)
        return;

    priv_data->physics_process = p_process;

    if (priv_data->physics_process)
        add_to_group(SceneStringNames::physics_process, false);
    else
        remove_from_group(SceneStringNames::physics_process);

    Object_change_notify(this,SceneStringNames::physics_process);
}

bool Node::is_physics_processing() const {

    return priv_data->physics_process;
}

void Node::set_physics_process_internal(bool p_process_internal) {

    if (priv_data->physics_process_internal == p_process_internal)
        return;

    priv_data->physics_process_internal = p_process_internal;

    if (priv_data->physics_process_internal)
        add_to_group(SceneStringNames::physics_process_internal, false);
    else
        remove_from_group(SceneStringNames::physics_process_internal);

    Object_change_notify(this,SceneStringNames::physics_process_internal);
}

bool Node::is_physics_processing_internal() const {

    return priv_data->physics_process_internal;
}

void Node::set_pause_mode(PauseMode p_mode) {

    if (priv_data->pause_mode == p_mode)
        return;

    bool prev_inherits = priv_data->pause_mode == PAUSE_MODE_INHERIT;
    priv_data->pause_mode = p_mode;
    if (!is_inside_tree())
        return; //pointless
    if ((priv_data->pause_mode == PAUSE_MODE_INHERIT) == prev_inherits)
        return; ///nothing changed

    Node *owner = nullptr;

    if (priv_data->pause_mode == PAUSE_MODE_INHERIT) {

        if (priv_data->parent)
            owner = priv_data->parent->priv_data->pause_owner;
    } else {
        owner = this;
    }

    _propagate_pause_owner(owner);
}

Node::PauseMode Node::get_pause_mode() const {

    return priv_data->pause_mode;
}

void Node::_propagate_pause_owner(Node *p_owner) {

    if (this != p_owner && priv_data->pause_mode != PAUSE_MODE_INHERIT)
        return;
    priv_data->pause_owner = p_owner;
    for (int i = 0; i < priv_data->children.size(); i++) {

        priv_data->children[i]->_propagate_pause_owner(p_owner);
    }
}

void Node::set_network_master(int p_peer_id, bool p_recursive) {

    priv_data->network_master = p_peer_id;

    if (p_recursive) {
        for (int i = 0; i < priv_data->children.size(); i++) {

            priv_data->children[i]->set_network_master(p_peer_id, true);
        }
    }
}

int Node::get_network_master() const {

    return priv_data->network_master;
}

bool Node::is_network_master() const {

    ERR_FAIL_COND_V(!is_inside_tree(), false);

    return get_multiplayer()->get_network_unique_id() == priv_data->network_master;
}

/***** RPC CONFIG ********/
//MultiplayerAPI::RPCMode Node::get_node_rpc_mode(const StringName &p_method) const {
//    return get_node_rpc_mode_by_id(get_node_rpc_method_id(p_method));
//}


uint16_t Node::rpc_config(const StringName &p_method, MultiplayerAPI_RPCMode p_mode) {
    uint16_t mid = priv_data->get_node_rpc_method_id(p_method);
    if (mid == UINT16_MAX) {
        // It's new
        PrivData::NetData nd;
        nd.name = p_method;
        nd.mode = p_mode;
        priv_data->rpc_methods.push_back(nd);
        return ((uint16_t)priv_data->rpc_properties.size() - 1) | (1 << 15);
    } else {
        int c_mid = (~(1 << 15)) & mid;
        priv_data->rpc_methods[c_mid].mode = p_mode;
        return mid;
    }
}

uint16_t Node::rset_config(const StringName &p_property, MultiplayerAPI_RPCMode p_mode) {
    uint16_t pid = priv_data->get_node_rset_property_id(p_property);
    if (pid == UINT16_MAX) {
        // It's new
        PrivData::NetData nd;
        nd.name = p_property;
        nd.mode = p_mode;
        priv_data->rpc_properties.push_back(nd);
        return ((uint16_t)priv_data->rpc_properties.size() - 1) | (1 << 15);
    } else {
        int c_pid = (~(1 << 15)) & pid;
        priv_data->rpc_properties[c_pid].mode = p_mode;
        return pid;
    }
}

/***** RPC FUNCTIONS ********/

void Node::rpc(const StringName &p_method, VARIANT_ARG_DECLARE) {

    VARIANT_ARGPTRS;

    int argc = 0;
    for (int i = 0; i < VARIANT_ARG_MAX; i++) {
        if (argptr[i]->get_type() == VariantType::NIL)
            break;
        argc++;
    }

    rpcp(0, false, p_method, argptr, argc);
}

void Node::rpc_id(int p_peer_id, const StringName &p_method, VARIANT_ARG_DECLARE) {

    VARIANT_ARGPTRS;

    int argc = 0;
    for (int i = 0; i < VARIANT_ARG_MAX; i++) {
        if (argptr[i]->get_type() == VariantType::NIL)
            break;
        argc++;
    }

    rpcp(p_peer_id, false, p_method, argptr, argc);
}

void Node::rpc_unreliable(const StringName &p_method, VARIANT_ARG_DECLARE) {

    VARIANT_ARGPTRS;

    int argc = 0;
    for (int i = 0; i < VARIANT_ARG_MAX; i++) {
        if (argptr[i]->get_type() == VariantType::NIL)
            break;
        argc++;
    }

    rpcp(0, true, p_method, argptr, argc);
}

void Node::rpc_unreliable_id(int p_peer_id, const StringName &p_method, VARIANT_ARG_DECLARE) {

    VARIANT_ARGPTRS

    int argc = 0;
    for (int i = 0; i < VARIANT_ARG_MAX; i++) {
        if (argptr[i]->get_type() == VariantType::NIL)
            break;
        argc++;
    }

    rpcp(p_peer_id, true, p_method, argptr, argc);
}

Variant Node::_rpc_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    if (p_argcount < 1) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
        r_error.argument = 1;
        return Variant();
    }

    if (p_args[0]->get_type() != VariantType::STRING) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
        r_error.argument = 0;
        r_error.expected = VariantType::STRING;
        return Variant();
    }

    StringName method = p_args[0]->as<StringName>();

    rpcp(0, false, method, &p_args[1], p_argcount - 1);

    r_error.error = Callable::CallError::CALL_OK;
    return Variant();
}

Variant Node::_rpc_id_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    if (p_argcount < 2) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
        r_error.argument = 2;
        return Variant();
    }

    if (p_args[0]->get_type() != VariantType::INT) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
        r_error.argument = 0;
        r_error.expected = VariantType::INT;
        return Variant();
    }

    if (p_args[1]->get_type() != VariantType::STRING) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
        r_error.argument = 1;
        r_error.expected = VariantType::STRING;
        return Variant();
    }

    int peer_id = p_args[0]->as<int>();
    StringName method = p_args[1]->as<StringName>();

    rpcp(peer_id, false, method, &p_args[2], p_argcount - 2);

    r_error.error = Callable::CallError::CALL_OK;
    return Variant();
}

Variant Node::_rpc_unreliable_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    if (p_argcount < 1) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
        r_error.argument = 1;
        return Variant();
    }

    if (p_args[0]->get_type() != VariantType::STRING) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
        r_error.argument = 0;
        r_error.expected = VariantType::STRING;
        return Variant();
    }

    StringName method = p_args[0]->as<StringName>();

    rpcp(0, true, method, &p_args[1], p_argcount - 1);

    r_error.error = Callable::CallError::CALL_OK;
    return Variant();
}

Variant Node::_rpc_unreliable_id_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    if (p_argcount < 2) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
        r_error.argument = 2;
        return Variant();
    }

    if (p_args[0]->get_type() != VariantType::INT) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
        r_error.argument = 0;
        r_error.expected = VariantType::INT;
        return Variant();
    }

    if (p_args[1]->get_type() != VariantType::STRING) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
        r_error.argument = 1;
        r_error.expected = VariantType::STRING;
        return Variant();
    }

    int peer_id = p_args[0]->as<int>();
    StringName method = p_args[1]->as<StringName>();

    rpcp(peer_id, true, method, &p_args[2], p_argcount - 2);

    r_error.error = Callable::CallError::CALL_OK;
    return Variant();
}

void Node::rpcp(int p_peer_id, bool p_unreliable, const StringName &p_method, const Variant **p_arg, int p_argcount) {
    ERR_FAIL_COND(!is_inside_tree());
    get_multiplayer()->rpcp(this, p_peer_id, p_unreliable, p_method, p_arg, p_argcount);
}

void Node::rsetp(int p_peer_id, bool p_unreliable, const StringName &p_property, const Variant &p_value) {
    ERR_FAIL_COND(!is_inside_tree());
    get_multiplayer()->rsetp(this, p_peer_id, p_unreliable, p_property, p_value);
}

/******** RSET *********/
void Node::rset(const StringName &p_property, const Variant &p_value) {

    rsetp(0, false, p_property, p_value);
}

void Node::rset_id(int p_peer_id, const StringName &p_property, const Variant &p_value) {

    rsetp(p_peer_id, false, p_property, p_value);
}

void Node::rset_unreliable(const StringName &p_property, const Variant &p_value) {

    rsetp(0, true, p_property, p_value);
}

void Node::rset_unreliable_id(int p_peer_id, const StringName &p_property, const Variant &p_value) {

    rsetp(p_peer_id, true, p_property, p_value);
}

//////////// end of rpc
Ref<MultiplayerAPI> Node::get_multiplayer() const {
    if (multiplayer)
        return multiplayer;
    if (!is_inside_tree())
        return Ref<MultiplayerAPI>();
    return get_tree()->get_multiplayer();
}

void Node::set_custom_multiplayer(Ref<MultiplayerAPI> p_multiplayer) {

    multiplayer = p_multiplayer;
}

MultiplayerAPI_RPCMode Node::get_node_rpc_mode_by_id(const uint16_t p_rpc_method_id) const {
    // Make sure this is a node generated ID.
    if (((1 << 15) & p_rpc_method_id) > 0) {
        int mid = (~(1 << 15)) & p_rpc_method_id;
        if (mid < priv_data->rpc_methods.size()) {
            return priv_data->rpc_methods[mid].mode;
        }
    }
    return MultiplayerAPI_RPCMode::RPC_MODE_DISABLED;
}
MultiplayerAPI_RPCMode Node::get_node_rpc_mode(const StringName &p_method) const {
    return get_node_rpc_mode_by_id(priv_data->get_node_rpc_method_id(p_method));
}

MultiplayerAPI_RPCMode Node::get_node_rset_mode_by_id(const uint16_t p_rset_property_id) const {
    if (((1 << 15) & p_rset_property_id) > 0) {
        int mid = (~(1 << 15)) & p_rset_property_id;
        if (mid < priv_data->rpc_properties.size()) {
            return priv_data->rpc_properties[mid].mode;
        }
    }
    return MultiplayerAPI_RPCMode::RPC_MODE_DISABLED;
}

MultiplayerAPI_RPCMode Node::get_node_rset_mode(const StringName &p_property) const {
    return get_node_rset_mode_by_id(priv_data->get_node_rset_property_id(p_property));
}

bool Node::can_process_notification(int p_what) const {
    switch (p_what) {
        case NOTIFICATION_PHYSICS_PROCESS: return priv_data->physics_process;
        case NOTIFICATION_PROCESS: return priv_data->idle_process;
        case NOTIFICATION_INTERNAL_PROCESS: return priv_data->idle_process_internal;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: return priv_data->physics_process_internal;
    }

    return true;
}

bool Node::can_process() const {

    ERR_FAIL_COND_V(!is_inside_tree(), false);

    if (get_tree()->is_paused()) {

        if (priv_data->pause_mode == PAUSE_MODE_STOP)
            return false;
        if (priv_data->pause_mode == PAUSE_MODE_PROCESS)
            return true;
        if (priv_data->pause_mode == PAUSE_MODE_INHERIT) {

            if (!priv_data->pause_owner)
                return false; //clearly no pause owner by default

            if (priv_data->pause_owner->priv_data->pause_mode == PAUSE_MODE_PROCESS)
                return true;

            if (priv_data->pause_owner->priv_data->pause_mode == PAUSE_MODE_STOP)
                return false;
        }
    }

    return true;
}

float Node::get_physics_process_delta_time() const {
    return tree ? tree->get_physics_process_time() : 0;
}

float Node::get_process_delta_time() const {
    if (tree) {
        return tree->get_idle_process_time();
    } else {
        return 0;
    }
}

void Node::set_process(bool p_idle_process) {

    if (priv_data->idle_process == p_idle_process)
        return;

    priv_data->idle_process = p_idle_process;

    if (priv_data->idle_process)
        add_to_group("idle_process", false);
    else
        remove_from_group("idle_process");

    Object_change_notify(this,"idle_process");
}

bool Node::is_processing() const {

    return priv_data->idle_process;
}

void Node::set_process_internal(bool p_idle_process_internal) {
    if (priv_data->idle_process_internal == p_idle_process_internal)
        return;

    priv_data->idle_process_internal = p_idle_process_internal;

    if (priv_data->idle_process_internal) {
        add_to_group("idle_process_internal", false);
    } else {
        remove_from_group("idle_process_internal");
    }

    Object_change_notify(this, "idle_process_internal");
}

bool Node::is_processing_internal() const {

    return priv_data->idle_process_internal;
}

void Node::set_process_priority(int p_priority) {
    process_priority = p_priority;

    // Make sure we are in SceneTree.
    if (tree == nullptr) {
        return;
    }

    if (is_processing()) {
        tree->make_group_changed("idle_process");
    }

    if (is_processing_internal()) {
        tree->make_group_changed("idle_process_internal");
    }

    if (is_physics_processing()) {
        tree->make_group_changed(SceneStringNames::physics_process);
    }

    if (is_physics_processing_internal()) {
        tree->make_group_changed(SceneStringNames::physics_process_internal);
    }
}

void Node::set_process_input(bool p_enable) {

    if (p_enable == priv_data->input)
        return;

    priv_data->input = p_enable;
    if (!is_inside_tree())
        return;

    if (p_enable)
        add_to_group(StringName("_vp_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));
    else
        remove_from_group(StringName("_vp_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));
}

bool Node::is_processing_input() const {
    return priv_data->input;
}

void Node::set_process_unhandled_input(bool p_enable) {

    if (p_enable == priv_data->unhandled_input)
        return;
    priv_data->unhandled_input = p_enable;
    if (!is_inside_tree())
        return;

    if (p_enable)
        add_to_group(StringName("_vp_unhandled_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));
    else
        remove_from_group(StringName("_vp_unhandled_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));
}

bool Node::is_processing_unhandled_input() const {
    return priv_data->unhandled_input;
}

void Node::set_process_unhandled_key_input(bool p_enable) {

    if (p_enable == priv_data->unhandled_key_input)
        return;
    priv_data->unhandled_key_input = p_enable;
    if (!is_inside_tree())
        return;

    if (p_enable)
        add_to_group(StringName("_vp_unhandled_key_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));
    else
        remove_from_group(StringName("_vp_unhandled_key_input" + itos(entt::to_integral(get_viewport()->get_instance_id()))));
}

bool Node::is_processing_unhandled_key_input() const {
    return priv_data->unhandled_key_input;
}

void Node::_set_use_identity_transform(bool p_enable) {
    priv_data->use_identity_transform = p_enable;
}

bool Node::_is_using_identity_transform() const {
    return priv_data->use_identity_transform;
}

StringName Node::get_name() const {

    return priv_data->name;
}

void Node::_set_name_nocheck(const StringName &p_name) {

    priv_data->name = p_name;
}

const char *Node::invalid_character(". : @ / \"");
//TODO: SEGS: validate_node_name should do what it's named after, not modify the passed name
bool Node::_validate_node_name(String &p_name) {
    String name = p_name;
    Vector<StringView> chars = StringUtils::split(Node::invalid_character,' ');
    for (size_t i = 0; i < chars.size(); i++) {
        name = StringUtils::replace(name,chars[i], String());
    }
    bool is_valid = name == p_name;
    p_name = name;
    return is_valid;
}

void Node::set_name(StringView p_name) {

    String name(p_name);
    _validate_node_name(name);

    ERR_FAIL_COND(name.empty());
    priv_data->name = StringName(name);

    if (priv_data->parent) {

        priv_data->parent->_validate_child_name(this);
    }

    propagate_notification(NOTIFICATION_PATH_CHANGED);

    if (is_inside_tree()) {

        emit_signal("renamed");
        get_tree()->node_renamed(this);
        get_tree()->tree_changed();
    }
}
static bool node_hrcr = false;
static SafeRefCount node_hrcr_count;

void Node::init_node_hrcr() {
    node_hrcr_count.init(1);
}

void Node::set_human_readable_collision_renaming(bool p_enabled) {

    node_hrcr = p_enabled;
}

#ifdef TOOLS_ENABLED
String Node::validate_child_name(Node *p_child) {

    StringName name = p_child->priv_data->name;
    _generate_serial_child_name(p_child, name);
    return String(name);
}
#endif

void Node::_validate_child_name(Node *p_child, bool p_force_human_readable) {

    /* Make sure the name is unique */

    if (node_hrcr || p_force_human_readable) {

        //this approach to autoset node names is human readable but very slow
        //it's turned on while running in the editor

        StringName name = p_child->priv_data->name;
        _generate_serial_child_name(p_child, name);
        p_child->priv_data->name = name;

    } else {

        //this approach to autoset node names is fast but not as readable
        //it's the default and reserves the '@' character for unique names.

        bool unique = true;

        if (p_child->priv_data->name.empty()) {
            //new unique name must be assigned
            unique = false;
        } else {
            //check if exists
            Node **children = priv_data->children.data();
            int cc = priv_data->children.size();

            for (int i = 0; i < cc; i++) {
                if (children[i] == p_child)
                    continue;
                if (children[i]->priv_data->name == p_child->priv_data->name) {
                    unique = false;
                    break;
                }
            }
        }

        if (!unique) {

            ERR_FAIL_COND(!node_hrcr_count.ref());
            String name = "@" + String(p_child->get_name()) + "@" + itos(node_hrcr_count.get());
            p_child->priv_data->name = StringName(name);
        }
    }
}

// Return s + 1 as if it were an integer
String increase_numeric_string(StringView s) {

    String res(s);
    bool carry = res.length() > 0;

    for (int i = res.length() - 1; i >= 0; i--) {
        if (!carry) {
            break;
        }
        char n = s[i];
        if (n == '9') { // keep carry as true: 9 + 1
            res[i]='0';
        } else {
            res[i] = n + 1;
            carry = false;
        }
    }

    if (carry) {
        res = "1" + res;
    }

    return res;
}

void Node::_generate_serial_child_name(const Node *p_child, StringName &name) const {

    if (name == StringName()) {
        //no name and a new nade is needed, create one.

        name = p_child->get_class_name();
        // Adjust casing according to project setting. The current type name is expected to be in PascalCase.
        switch (ProjectSettings::get_singleton()->get("node/name_casing").as<NameCasing>()) {
            case NAME_CASING_PASCAL_CASE:
                break;
            case NAME_CASING_CAMEL_CASE: {
                String n(name);
                //TODO: SEGS: consider char_lowercase that is correct and returns more then 1 char!
                n[0] = StringUtils::char_lowercase(n[0]);
                name = StringName(n);
            } break;
            case NAME_CASING_SNAKE_CASE:
                name = StringName(StringUtils::camelcase_to_underscore(name,true));
                break;
        }
    }

    //quickly test if proposed name exists
    int cc = priv_data->children.size(); //children count
    const Node *const *children_ptr = priv_data->children.data();

    {

        bool exists = false;

        for (int i = 0; i < cc; i++) {
            if (children_ptr[i] == p_child) { //exclude self in renaming if its already a child
                continue;
            }
            if (children_ptr[i]->priv_data->name == name) {
                exists = true;
            }
        }

        if (!exists) {
            return; //if it does not exist, it does not need validation
        }
    }

    // Extract trailing number
    String name_string(name);
    String nums;
    for (int i = name_string.length() - 1; i >= 0; i--) {
        CharType n = name_string[i];
        if (n >= '0' && n <= '9') {
            nums = name_string[i] + nums;
        } else {
            break;
        }
    }

    StringView nnsep(_get_name_num_separator());
    int name_last_index = name_string.length() - nnsep.length() - nums.length();

    // Assign the base name + separator to name if we have numbers preceded by a separator
    if (nums.length() > 0 && StringUtils::substr(name_string,name_last_index, nnsep.length()) == nnsep) {
        name_string = StringUtils::substr(name_string,0, name_last_index + nnsep.length());
    } else {
        nums = "";
    }

    for (;;) {
        StringName attempt(name_string + nums);
        bool exists = false;

        for (int i = 0; i < cc; i++) {
            if (children_ptr[i] == p_child) {
                continue;
            }
            if (children_ptr[i]->priv_data->name == attempt) {
                exists = true;
            }
        }

        if (!exists) {
            name = attempt;
            return;
        } else {
            if (nums.length() == 0) {
                // Name was undecorated so skip to 2 for a more natural result
                nums = "2";
                name_string += nnsep; // Add separator because nums.length() > 0 was false
            } else {
                nums = increase_numeric_string(nums);
            }
        }
    }
}

void Node::_add_child_nocheck(Node *p_child, const StringName &p_name) {
    //add a child node quickly, without name validation

    p_child->priv_data->name = p_name;
    p_child->priv_data->pos = priv_data->children.size();
    priv_data->children.push_back(p_child);
    p_child->priv_data->parent = this;
    p_child->notification(NOTIFICATION_PARENTED);

    if (tree) {
        p_child->_set_tree(tree);
    }

    /* Notify */
    //recognize children created in this node constructor
    p_child->parent_owned = priv_data->in_constructor;
    add_child_notify(p_child);
}

void Node::add_child(Node *p_child, bool p_legible_unique_name) {

    ERR_FAIL_NULL(p_child);
    ERR_FAIL_COND_MSG(p_child == this, FormatVE("Can't add child '%s' to itself.", p_child->get_name().asCString())); // adding to itself!
    if (unlikely(p_child->priv_data->parent))  // Fail if node has a parent
    {
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' \"p_child->priv_data->parent\" ' is true.", FormatVE("Can't add child '%s' to '%s', already has a parent '%s'.", p_child->get_name().asCString(), get_name().asCString(), p_child->priv_data->parent->get_name().asCString())); //Fail if node has a parent
        return;
    }
#ifdef DEBUG_ENABLED
    ERR_FAIL_COND_MSG(p_child->is_a_parent_of(this), FormatVE("Can't add child '%s' to '%s' as it would result in a cyclic dependency since '%s' is already a parent of '%s'.", p_child->get_name().asCString(), get_name().asCString(), p_child->get_name().asCString(), get_name().asCString()));
#endif
    ERR_FAIL_COND_MSG(blocked > 0, "Parent node is busy setting up children, add_node() failed. Consider using "
                                         "call_deferred(\"add_child\", child) instead.");

    /* Validate name */
    _validate_child_name(p_child, p_legible_unique_name);

    _add_child_nocheck(p_child, p_child->priv_data->name);
}

void Node::add_child_below_node(Node *p_node, Node *p_child, bool p_legible_unique_name) {

    ERR_FAIL_NULL(p_node);
    ERR_FAIL_NULL(p_child);

    add_child(p_child, p_legible_unique_name);

    if (p_node->priv_data->parent == this) {
        move_child(p_child, p_node->get_position_in_parent() + 1);
    } else {
        WARN_PRINT("Cannot move under node " + String(p_node->get_name()) + " as " + p_child->get_name() + " does not share a parent.");
    }
}


void Node::remove_child(Node *p_child) {

    ERR_FAIL_NULL(p_child);
    ERR_FAIL_COND_MSG(blocked > 0, "Parent node is busy setting up children, remove_node() failed. Consider using call_deferred(\"remove_child\", child) instead.");

    int child_count = priv_data->children.size();
    Node **children = priv_data->children.data();
    int idx = -1;

    if (p_child->priv_data->pos >= 0 && p_child->priv_data->pos < child_count) {
        if (children[p_child->priv_data->pos] == p_child) {
            idx = p_child->priv_data->pos;
        }
    }

    if (idx == -1) { //maybe removed while unparenting or something and index was not updated, so just in case the above fails, try this.
        for (int i = 0; i < child_count; i++) {

            if (children[i] == p_child) {

                idx = i;
                break;
            }
        }
    }

    ERR_FAIL_COND_MSG(idx == -1, FormatVE("Cannot remove child node '%s' as it is not a child of this node.", p_child->get_name().asCString()));

    //if (data->scene) { does not matter

    p_child->_set_tree(nullptr);
    //}

    remove_child_notify(p_child);
    p_child->notification(NOTIFICATION_UNPARENTED);

    priv_data->children.erase_at(idx);

    //update pointer and size
    child_count = priv_data->children.size();
    children = priv_data->children.data();

    for (int i = idx; i < child_count; i++) {

        children[i]->priv_data->pos = i;
        children[i]->notification(NOTIFICATION_MOVED_IN_PARENT);
    }

    p_child->priv_data->parent = nullptr;
    p_child->priv_data->pos = -1;

    p_child->_propagate_after_exit_branch(inside_tree);
}

int Node::get_child_count() const {

    return priv_data->children.size();
}
Node *Node::get_child(int p_index) const {

    ERR_FAIL_INDEX_V(p_index, priv_data->children.size(), nullptr);

    return priv_data->children[p_index];
}

const Vector<Node *> &Node::children() const
{
    return priv_data->children;
}

Node *Node::_get_child_by_name(const StringName &p_name) const {

    int cc = priv_data->children.size();
    Node *const *cd = priv_data->children.data();

    for (int i = 0; i < cc; i++) {
        if (cd[i]->priv_data->name == p_name)
            return cd[i];
    }

    return nullptr;
}

static Node *get_by_name(Node *from,StringView name) {
    if(from->get_name()==name)
        return from;
    int cnt=from->get_child_count();
    for(int idx=0; idx<cnt; ++idx) {
        auto res = get_by_name(from->get_child(idx),name);
        if(res)
            return res;
    }
    return nullptr;
}

Node *Node::get_node_or_null(const NodePath &p_path) const {

    if (p_path.is_empty()) {
        return nullptr;
    }

    ERR_FAIL_COND_V_MSG(!inside_tree && (p_path.is_absolute()||p_path.is_locator()), nullptr, "Can't use get_node() with absolute/locator paths from outside the active scene tree.");

    Node *current = nullptr;
    Node *root = nullptr;

    int elem = 0;
    if (!p_path.is_absolute() && !p_path.is_locator()) {
        current = const_cast<Node *>(this); //start from this
    } else if(p_path.is_locator()) {
        current = get_by_name(const_cast<Node *>(this),StringView(p_path.get_name(0)).substr(1));
        elem = 1; // start from second element;
    }
    else {

        root = const_cast<Node *>(this);
        while (root->priv_data->parent)
            root = root->priv_data->parent; //start from root
    }

    for ( ; elem < p_path.get_name_count(); elem++) {

        StringName name = p_path.get_name(elem);
        Node *next = nullptr;

        if (name == SceneStringNames::dot) { // .

            next = current;

        } else if (name == SceneStringNames::doubledot) { // ..

            if (current == nullptr || !current->priv_data->parent)
                return nullptr;

            next = current->priv_data->parent;
        } else if (current == nullptr) {

            if (name == root->get_name()) {
                next = root;
            }

        } else {

            next = nullptr;

            for (int j = 0; j < current->priv_data->children.size(); j++) {

                Node *child = current->priv_data->children[j];

                if (child->priv_data->name == name) {

                    next = child;
                    break;
                }
            }
            if (next == nullptr) {
                return nullptr;
            }
        }
        current = next;
    }

    return current;
}

Node *Node::find_node(StringView p_mask, bool p_recursive, bool p_owned) const
{
    Node *const *cptr = priv_data->children.data();
    int ccount = priv_data->children.size();
    for (int i = 0; i < ccount; i++) {
        if (p_owned && !cptr[i]->priv_data->owner) {
            continue;
        }

        if (StringUtils::match(cptr[i]->priv_data->name,p_mask)) {
            return cptr[i];
        }

        if (!p_recursive) {
            continue;
        }

        Node *ret = cptr[i]->find_node(p_mask, true, p_owned);
        if (ret) {
            return ret;
        }
    }
    return nullptr;
}
Node *Node::get_node(const NodePath &p_path) const {

    Node *node = get_node_or_null(p_path);
    if (unlikely(!node)) {
        if (p_path.is_absolute()) {
            ERR_FAIL_V_MSG(nullptr, FormatSN("(Node not found: \"%s\" (absolute path attempted from \"%s\").)",
                                            p_path.asString().c_str(), get_path().asString().c_str()));
        } else {
            ERR_FAIL_V_MSG(nullptr, FormatSN("(Node not found: \"%s\" (relative to \"%s\").)",
                                            p_path.asString().c_str(), get_path().asString().c_str()));
        }
    }
    return node;
}

bool Node::has_node(const NodePath &p_path) const {

    return get_node_or_null(p_path) != nullptr;
}

Node *Node::get_parent() const {

    return priv_data->parent;
}

bool Node::is_a_parent_of(const Node *p_node) const {

    ERR_FAIL_NULL_V(p_node, false);
    Node *p = p_node->priv_data->parent;
    while (p) {

        if (p == this)
            return true;
        p = p->priv_data->parent;
    }

    return false;
}

bool Node::is_greater_than(const Node *p_node) const {

    ERR_FAIL_NULL_V(p_node, false);
    ERR_FAIL_COND_V(!inside_tree, false);
    ERR_FAIL_COND_V(!p_node->inside_tree, false);

    ERR_FAIL_COND_V(priv_data->depth < 0, false);
    ERR_FAIL_COND_V(p_node->priv_data->depth < 0, false);

    int *this_stack = (int *)alloca(sizeof(int) * priv_data->depth);
    int *that_stack = (int *)alloca(sizeof(int) * p_node->priv_data->depth);

    const Node *n = this;

    int idx = priv_data->depth - 1;
    while (n) {
        ERR_FAIL_INDEX_V(idx, priv_data->depth, false);
        this_stack[idx--] = n->priv_data->pos;
        n = n->priv_data->parent;
    }
    ERR_FAIL_COND_V(idx != -1, false);
    n = p_node;
    idx = p_node->priv_data->depth - 1;
    while (n) {
        ERR_FAIL_INDEX_V(idx, p_node->priv_data->depth, false);
        that_stack[idx--] = n->priv_data->pos;

        n = n->priv_data->parent;
    }
    ERR_FAIL_COND_V(idx != -1, false);
    idx = 0;

    bool res;
    while (true) {

        // using -2 since out-of-tree or nonroot nodes have -1
        int this_idx = (idx >= priv_data->depth) ? -2 : this_stack[idx];
        int that_idx = (idx >= p_node->priv_data->depth) ? -2 : that_stack[idx];

        if (this_idx > that_idx) {
            res = true;
            break;
        } else if (this_idx < that_idx) {
            res = false;
            break;
        } else if (this_idx == -2) {
            res = false; // equal
            break;
        }
        idx++;
    }

    return res;
}

void Node::get_owned_by(Node *p_by, Vector<Node *> &p_owned) {

    if (priv_data->owner == p_by)
        p_owned.emplace_back(this);

    for(Node * chld : priv_data->children) {
        chld->get_owned_by(p_by, p_owned);
    }
}

void Node::_set_owner_nocheck(Node *p_owner) {

    if (priv_data->owner == p_owner)
        return;

    ERR_FAIL_COND(priv_data->owner);
    priv_data->owner = p_owner;
    priv_data->owner->priv_data->owned.push_back(this);
    priv_data->OW = this;
}

void Node::set_owner(Node *p_owner) {

    if (priv_data->owner) {

        priv_data->owner->priv_data->owned.erase_first(priv_data->OW);
        priv_data->OW = nullptr;
        priv_data->owner = nullptr;
    }

    ERR_FAIL_COND(p_owner == this);

    if (!p_owner)
        return;

    Node *check = this->get_parent();
    bool owner_valid = false;

    while (check) {

        if (check == p_owner) {
            owner_valid = true;
            break;
        }

        check = check->priv_data->parent;
    }

    ERR_FAIL_COND(!owner_valid);

    _set_owner_nocheck(p_owner);
}
Node *Node::get_owner() const {

    return priv_data->owner;
}

Node *Node::find_common_parent_with(const Node *p_node) const {

    if (this == p_node)
        return const_cast<Node *>(p_node);

    Set<const Node *> visited;

    const Node *n = this;

    while (n) {

        visited.insert(n);
        n = n->priv_data->parent;
    }

    const Node *common_parent = p_node;

    while (common_parent) {

        if (visited.contains(common_parent))
            break;
        common_parent = common_parent->priv_data->parent;
    }

    if (!common_parent)
        return nullptr;

    return const_cast<Node *>(common_parent);
}

NodePath Node::get_path_to(const Node *p_node) const {

    ERR_FAIL_NULL_V(p_node, NodePath());

    if (this == p_node)
        return NodePath(".");

    Set<const Node *> visited;

    const Node *n = this;

    while (n) {

        visited.insert(n);
        n = n->priv_data->parent;
    }

    const Node *common_parent = p_node;

    while (common_parent) {

        if (visited.contains(common_parent))
            break;
        common_parent = common_parent->priv_data->parent;
    }

    ERR_FAIL_COND_V(!common_parent, NodePath()); //nodes not in the same tree

    visited.clear();

    Vector<StringName> path;

    n = p_node;

    while (n != common_parent) {

        path.push_back(n->get_name());
        n = n->priv_data->parent;
    }

    n = this;
    StringName up("..");

    while (n != common_parent) {

        path.push_back(up);
        n = n->priv_data->parent;
    }

    eastl::reverse(path.begin(),path.end());

    return NodePath(path, false);
}

NodePath Node::get_path() const {

    ERR_FAIL_COND_V_MSG(!is_inside_tree(), NodePath(), "Cannot get path of node as it is not in a scene tree.");

    if (priv_data->path_cache)
        return *priv_data->path_cache;

    const Node *n = this;

    Vector<StringName> path;

    while (n) {
        path.push_back(n->get_name());
        n = n->priv_data->parent;
    }

    eastl::reverse(path.begin(),path.end());

    priv_data->path_cache = memnew(NodePath(path, true));

    return *priv_data->path_cache;
}

bool Node::is_in_group(const StringName &p_identifier) const {

    return priv_data->grouped.contains(p_identifier);
}

void Node::add_to_group(const StringName &p_identifier, bool p_persistent) {

    ERR_FAIL_COND(StringView(p_identifier).empty());

    if (priv_data->grouped.contains(p_identifier))
        return;

    GroupData gd;

    if (tree) {
        gd.group = tree->add_to_group(p_identifier, this);
    } else {
        gd.group = nullptr;
    }

    gd.persistent = p_persistent;

    priv_data->grouped[p_identifier] = gd;
}

void Node::remove_from_group(const StringName &p_identifier) {

    ERR_FAIL_COND(!priv_data->grouped.contains(p_identifier));

    HashMap<StringName, GroupData>::iterator E = priv_data->grouped.find(p_identifier);

    ERR_FAIL_COND(E==priv_data->grouped.end());

    if (tree)
        tree->remove_from_group(E->first, this);

    priv_data->grouped.erase(E);
}

Array Node::_get_groups() const {

    Array groups;
    Vector<GroupInfo> gi;
    get_groups(&gi);
    for (const GroupInfo &E : gi) {
        groups.push_back(E.name);
    }

    return groups;
}

void Node::get_groups(Vector<GroupInfo> *p_groups) const {
    p_groups->reserve(p_groups->size()+priv_data->grouped.size());
    for (const eastl::pair<const StringName, GroupData> &E : priv_data->grouped) {
        GroupInfo gi;
        gi.name = E.first;
        gi.persistent = E.second.persistent;
        p_groups->push_back(gi);
    }
}

int Node::get_persistent_group_count() const {

    int count = 0;

    for (const eastl::pair<const StringName, GroupData> &E : priv_data->grouped) {
        if (E.second.persistent) {
            count += 1;
        }
    }

    return count;
}

void Node::print_tree() {

    _print_tree(this);
}

void Node::_print_tree(const Node *p_node) {
    print_line(String(p_node->get_path_to(this)));
    for (int i = 0; i < priv_data->children.size(); i++)
        priv_data->children[i]->_print_tree(p_node);
}

void Node::_propagate_reverse_notification(int p_notification) {

    blocked++;
    for (int i = priv_data->children.size() - 1; i >= 0; i--) {

        priv_data->children[i]->_propagate_reverse_notification(p_notification);
    }

    notification(p_notification, true);
    blocked--;
}

void Node::_propagate_deferred_notification(int p_notification, bool p_reverse) {

    ERR_FAIL_COND(!is_inside_tree());

    blocked++;

    if (!p_reverse) {
        call_deferred([=]() { notification(p_notification); });
    }

    for (int i = 0; i < priv_data->children.size(); i++) {

        priv_data->children[i]->_propagate_deferred_notification(p_notification, p_reverse);
    }

    if (p_reverse) {
        call_deferred([=]() { notification(p_notification); });
    }

    blocked--;
}

void Node::propagate_notification(int p_notification) {

    blocked++;
    notification(p_notification);

    for (int i = 0; i < priv_data->children.size(); i++) {

        priv_data->children[i]->propagate_notification(p_notification);
    }
    blocked--;
}

void Node::propagate_call(const StringName &p_method, const Array &p_args, const bool p_parent_first) {

    blocked++;

    if (p_parent_first && has_method(p_method)) {
        callv(p_method, p_args);
    }

    for (int i = 0; i < priv_data->children.size(); i++) {
        priv_data->children[i]->propagate_call(p_method, p_args, p_parent_first);
    }

    if (!p_parent_first && has_method(p_method)) {
        callv(p_method, p_args);
    }

    blocked--;
}

void Node::_propagate_replace_owner(Node *p_owner, Node *p_by_owner) {
    if (get_owner() == p_owner)
        set_owner(p_by_owner);

    blocked++;
    for (size_t i = 0; i < priv_data->children.size(); i++)
        priv_data->children[i]->_propagate_replace_owner(p_owner, p_by_owner);
    blocked--;
}

int Node::get_index() const {

    return priv_data->pos;
}
void Node::remove_and_skip() {

    ERR_FAIL_COND(!priv_data->parent);

    Node *new_owner = get_owner();

    Dequeue<Node *> children;

    while (true) {

        bool clear = true;
        for (size_t i = 0; i < priv_data->children.size(); i++) {
            Node *c_node = priv_data->children[i];
            if (!c_node->get_owner())
                continue;

            remove_child(c_node);
            c_node->_propagate_replace_owner(this, nullptr);
            children.push_back(c_node);
            clear = false;
            break;
        }

        if (clear)
            break;
    }

    for(Node * c_node : children) {

        priv_data->parent->add_child(c_node);
        c_node->_propagate_replace_owner(nullptr, new_owner);
    }

    priv_data->parent->remove_child(this);
}

void Node::set_filename(StringView p_filename) {
    if(!priv_data->filename)
        priv_data->filename = new String;
    *priv_data->filename = p_filename;
}
StringView Node::get_filename() const {

    if(priv_data->filename)
        return *priv_data->filename;
    return {};
}

void Node::set_editor_description(StringView p_editor_description) {

    set_meta("_editor_description_", p_editor_description);
}

String Node::get_editor_description() const {

    if (has_meta("_editor_description_")) {
        return get_meta("_editor_description_").as<String>();
    } else {
        return String();
    }
}

void Node::set_editable_instance(Node *p_node, bool p_editable) {
    ERR_FAIL_NULL(p_node);
    ERR_FAIL_COND(!is_a_parent_of(p_node));
    if (!p_editable) {
        p_node->priv_data->editable_instance = false;
        // Avoid this flag being needlessly saved;
        // also give more visual feedback if editable children is re-enabled
        set_display_folded(false);
    } else {
        p_node->priv_data->editable_instance = true;
    }
}

bool Node::is_editable_instance(const Node *p_node) const {
    if (!p_node)
        return false; //easier, null is never editable :)
    ERR_FAIL_COND_V(!is_a_parent_of(p_node), false);
    return p_node->priv_data->editable_instance;
}

Node *Node::get_deepest_editable_node(Node *p_start_node) const {
    ERR_FAIL_NULL_V(p_start_node, nullptr);
    ERR_FAIL_COND_V(!is_a_parent_of(p_start_node), p_start_node);

    Node const *iterated_item = p_start_node;
    Node *node = p_start_node;

    while (iterated_item->get_owner() && iterated_item->get_owner() != this) {
        if (!is_editable_instance(iterated_item->get_owner()))
            node = iterated_item->get_owner();

        iterated_item = iterated_item->get_owner();
    }

    return node;
}

#ifdef TOOLS_ENABLED
void Node::set_property_pinned(const StringName &p_property, bool p_pinned) {
    bool current_pinned = false;
    bool has_pinned = has_meta("_edit_pinned_properties_");
    Array pinned;
    StringName psa = get_property_store_alias(p_property);
    if (has_pinned) {
        pinned = get_meta("_edit_pinned_properties_").as<Array>();
        current_pinned = pinned.contains(psa);
    }

    if (current_pinned != p_pinned) {
        if (p_pinned) {
            pinned.append(psa);
            if (!has_pinned) {
                set_meta("_edit_pinned_properties_", pinned);
            }
        } else {
            pinned.erase(psa);
            if (pinned.empty()) {
                remove_meta("_edit_pinned_properties_");
            }
        }
    }
}

bool Node::is_property_pinned(const StringName &p_property) const {
    if (!has_meta("_edit_pinned_properties_")) {
        return false;
    }
    Array pinned = get_meta("_edit_pinned_properties_").as<Array>();
    StringName psa = get_property_store_alias(p_property);
    return pinned.contains(psa);
}

StringName Node::get_property_store_alias(const StringName &p_property) const {
    return p_property;
}
#endif
void Node::get_storable_properties(Set<StringName> &r_storable_properties) const {
    Vector<PropertyInfo> pi;
    get_property_list(&pi);
    for (const PropertyInfo &prop : pi) {
        if ((prop.usage & PROPERTY_USAGE_STORAGE)) {
            r_storable_properties.insert(prop.name);
        }
    }
}

String Node::to_string() {
    if (get_script_instance()) {
        bool valid;
        String ret = get_script_instance()->to_string(&valid);
        if (valid) {
            return ret;
        }
    }

    return (get_name() ? String(get_name()) + ":" : "") + Object::to_string();
}

void Node::set_scene_instance_state(const Ref<SceneState> &p_state) {

    priv_data->instance_state = p_state;
}

Ref<SceneState> Node::get_scene_instance_state() const {

    return priv_data->instance_state;
}

void Node::set_scene_inherited_state(const Ref<SceneState> &p_state) {

    priv_data->inherited_state = p_state;
}

Ref<SceneState> Node::get_scene_inherited_state() const {

    return priv_data->inherited_state;
}

void Node::set_scene_instance_load_placeholder(bool p_enable) {

    priv_data->use_placeholder = p_enable;
}

bool Node::get_scene_instance_load_placeholder() const {

    return priv_data->use_placeholder;
}

int Node::get_position_in_parent() const {

    return priv_data->pos;
}

Node *Node::_duplicate(int p_flags, HashMap<const Node *, Node *> *r_duplimap) const {

    Node *node = nullptr;

    bool instanced = false;

    const InstancePlaceholder *ip = object_cast<const InstancePlaceholder>(this);

    if (ip) {
        InstancePlaceholder *nip = memnew(InstancePlaceholder);
        nip->set_instance_path(ip->get_instance_path());
        node = nip;

    } else if ((p_flags & DUPLICATE_USE_INSTANCING) && !get_filename().empty()) {

        Ref<PackedScene> res = dynamic_ref_cast<PackedScene>(gResourceManager().load(get_filename()));
        ERR_FAIL_COND_V(not res, nullptr);
        PackedGenEditState ges = GEN_EDIT_STATE_DISABLED;
#ifdef TOOLS_ENABLED
        if (p_flags & DUPLICATE_FROM_EDITOR)
            ges = GEN_EDIT_STATE_INSTANCE;
#endif
        node = res->instance(ges);
        ERR_FAIL_COND_V(!node, nullptr);
        node->set_scene_instance_load_placeholder(get_scene_instance_load_placeholder());

        instanced = true;

    } else {

        Object *obj = ClassDB::instance(get_class_name());
        ERR_FAIL_COND_V(!obj, nullptr);
        node = object_cast<Node>(obj);
        if(!node) {
            memdelete(obj);
        }
        ERR_FAIL_COND_V(!node, nullptr);
    }

    if (!get_filename().empty()) { //an instance
        node->set_filename(get_filename());
        node->priv_data->editable_instance = priv_data->editable_instance;
    }

    StringName script_property_name = CoreStringNames::get_singleton()->_script;

    Dequeue<const Node *> hidden_roots;
    Dequeue<const Node *> node_tree;
    node_tree.emplace_front(this);

    if (instanced) {
        // Since nodes in the instanced hierarchy won't be duplicated explicitly, we need to make an inventory
        // of all the nodes in the tree of the instanced scene in order to transfer the values of the properties
        eastl::vector_set<const Node *> instance_roots;
        instance_roots.insert(this);

        for (auto N = node_tree.begin(); N!=node_tree.end(); ++N) {
            for (int i = 0; i < (*N)->get_child_count(); ++i) {

                Node *descendant = (*N)->get_child(i);
                // Skip nodes not really belonging to the instanced hierarchy; they'll be processed normally later
                // but remember non-instanced nodes that are hidden below instanced ones
                if (instance_roots.contains(descendant->priv_data->owner)) {
                    if (descendant->get_parent() && descendant->get_parent() != this && descendant->priv_data->owner != descendant->get_parent())
                        hidden_roots.emplace_back(descendant);
                    continue;
                }

                node_tree.emplace_back(descendant);
                if (!descendant->get_filename().empty() && instance_roots.contains(descendant->get_owner())) {
                    instance_roots.insert(descendant);
                }
            }
        }
    }

    for (const Node *N : node_tree) {

        Node *current_node = node->get_node(get_path_to(N));
        ERR_CONTINUE(!current_node);

        if (p_flags & DUPLICATE_SCRIPTS) {
            bool is_valid = false;
            Variant script = N->get(script_property_name, &is_valid);
            if (is_valid) {
                current_node->set(script_property_name, script);
            }
        }

        Vector<PropertyInfo> plist;
        N->get_property_list(&plist);

        for (const PropertyInfo & E : plist) {

            if (!(E.usage & PROPERTY_USAGE_STORAGE))
                continue;
            const StringName &name(E.name);
            if (name == script_property_name)
                continue;

            Variant value = N->get(name).duplicate(true);

            if (E.usage & PROPERTY_USAGE_DO_NOT_SHARE_ON_DUPLICATE) {

                Resource *res = value.asT<Resource>();
                if (res) { // Duplicate only if it's a resource
                    current_node->set(name, res->duplicate());
                }

            } else {

                current_node->set(name, value);
            }
        }
    }

    if (!get_name().empty()) {
        node->set_name(get_name());
    }

#ifdef TOOLS_ENABLED
    if ((p_flags & DUPLICATE_FROM_EDITOR) && r_duplimap)
        r_duplimap->emplace(this, node);
#endif

    if (p_flags & DUPLICATE_GROUPS) {
        Vector<GroupInfo> gi;
        get_groups(&gi);
        for (const GroupInfo &E : gi) {

#ifdef TOOLS_ENABLED
            if ((p_flags & DUPLICATE_FROM_EDITOR) && !E.persistent)
                continue;
#endif
            node->add_to_group(E.name, E.persistent);
        }
    }

    for (int i = 0; i < get_child_count(); i++) {

        if (get_child(i)->parent_owned) {
            continue;
        }
        if (instanced && get_child(i)->priv_data->owner == this) {
            continue; //part of instance
        }

        Node *dup = get_child(i)->_duplicate(p_flags, r_duplimap);
        if (!dup) {

            memdelete(node);
            return nullptr;
        }

        node->add_child(dup);
        if (i < node->get_child_count() - 1) {
            node->move_child(dup, i);
        }
    }

    for (const Node *E : hidden_roots) {

        Node *parent = node->get_node(get_path_to(E->priv_data->parent));
        if (!parent) {

            memdelete(node);
            return nullptr;
        }

        Node *dup = E->_duplicate(p_flags, r_duplimap);
        if (!dup) {

            memdelete(node);
            return nullptr;
        }

        parent->add_child(dup);
        int pos = E->get_position_in_parent();

        if (pos < parent->get_child_count() - 1) {
            parent->move_child(dup, pos);
        }
    }

    return node;
}

Node *Node::duplicate(int p_flags) const {

    Node *dupe = _duplicate(p_flags);

    if (dupe && (p_flags & DUPLICATE_SIGNALS)) {
        _duplicate_signals(this, dupe);
    }

    return dupe;
}

#ifdef TOOLS_ENABLED
Node *Node::duplicate_from_editor(HashMap<const Node *, Node *> &r_duplimap) const {
    return duplicate_from_editor(r_duplimap, HashMap<RES, RES>());
}

Node *Node::duplicate_from_editor(HashMap<const Node *, Node *> &r_duplimap, const HashMap<RES, RES> &p_resource_remap) const {

    Node *dupe = _duplicate(
            DUPLICATE_SIGNALS | DUPLICATE_GROUPS | DUPLICATE_SCRIPTS | DUPLICATE_USE_INSTANCING | DUPLICATE_FROM_EDITOR,
            &r_duplimap);
    // This is used by SceneTreeDock's paste functionality. When pasting to foreign scene, resources are duplicated.
    if (!p_resource_remap.empty()) {
        remap_node_resources(dupe, p_resource_remap);
    }

    // Duplication of signals must happen after all the node descendants have been copied,
    // because re-targeting of connections from some descendant to another is not possible
    // if the emitter node comes later in tree order than the receiver
    _duplicate_signals(this, dupe);

    return dupe;
}
void Node::remap_node_resources(Node *p_node, const HashMap<RES, RES> &p_resource_remap) const {
    Vector<PropertyInfo> props;
    p_node->get_property_list(&props);

    for (PropertyInfo &inf : props) {
        if (!(inf.usage & PROPERTY_USAGE_STORAGE)) {
            continue;
        }

        Variant v = p_node->get(inf.name);
        if (v.is_ref()) {
            RES res = v;
            if (res) {
                if (p_resource_remap.contains(res)) {
                    p_node->set(inf.name, p_resource_remap.at(res));
                    remap_nested_resources(res, p_resource_remap);
                }
            }
        }
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {
        remap_node_resources(p_node->get_child(i), p_resource_remap);
    }
}

void Node::remap_nested_resources(const RES &p_resource, const HashMap<RES, RES> &p_resource_remap) const {
    Vector<PropertyInfo> props;
    p_resource->get_property_list(&props);

    for (PropertyInfo &inf : props) {
        if (!(inf.usage & PROPERTY_USAGE_STORAGE)) {
            continue;
        }

        Variant v = p_resource->get(inf.name);
        if (v.is_ref()) {
            RES res = v;
            if (res) {
                if (p_resource_remap.contains(res)) {
                    p_resource->set(inf.name, p_resource_remap.at(res));
                    remap_nested_resources(res, p_resource_remap);
                }
            }
        }
    }
}
#endif

void Node::_duplicate_and_reown(Node *p_new_parent, const HashMap<Node *, Node *> &p_reown_map) const {

    if (get_owner() != get_parent()->get_owner()) {
        return;
    }

    Node *node = nullptr;

    if (!get_filename().empty()) {

        Ref<PackedScene> res = dynamic_ref_cast<PackedScene>(gResourceManager().load(get_filename()));
        ERR_FAIL_COND(not res);
        node = res->instance();
        ERR_FAIL_COND(!node);
    } else {

        Object *obj = ClassDB::instance(get_class_name());
        ERR_FAIL_COND_MSG(!obj, "Node: Could not duplicate: " + String(get_class()) + ".");
        node = object_cast<Node>(obj);
        if (!node) {
            memdelete(obj);
            ERR_FAIL_MSG("Node: Could not duplicate: " + String(get_class()) + ".");
        }
    }

    Vector<PropertyInfo> plist;

    get_property_list(&plist);

    for (const PropertyInfo & E : plist) {

        if (!(E.usage & PROPERTY_USAGE_STORAGE))
            continue;
        StringName name = E.name;

        Variant value = get(name).duplicate(true);

        node->set(name, value);
    }

    Vector<GroupInfo> groups;
    get_groups(&groups);

    for (const GroupInfo &E : groups)
        node->add_to_group(E.name, E.persistent);

    node->set_name(get_name());
    p_new_parent->add_child(node);

    Node *owner = get_owner();

    if (p_reown_map.contains(owner))
        owner = p_reown_map.at(owner);

    if (owner) {
        NodePath p = get_path_to(owner);
        if (owner != this) {
            Node *new_owner = node->get_node(p);
            if (new_owner) {
                node->set_owner(new_owner);
            }
        }
    }

    for (int i = 0; i < get_child_count(); i++) {

        get_child(i)->_duplicate_and_reown(node, p_reown_map);
    }
}

// Duplication of signals must happen after all the node descendants have been copied,
// because re-targeting of connections from some descendant to another is not possible
// if the emitter node comes later in tree order than the receiver
void Node::_duplicate_signals(const Node *p_original, Node *p_copy) const {

    if (this != p_original && (get_owner() != p_original && get_owner() != p_original->get_owner())) {
        return;
    }

    Vector<Connection> conns;
    get_all_signal_connections(&conns);

    for (const Connection &E : conns) {

        if (E.flags & ObjectNS::CONNECT_PERSIST) {
            //user connected
            NodePath p = p_original->get_path_to(this);
            Node *copy = p_copy->get_node(p);

            Node *target = object_cast<Node>(E.callable.get_object());
            if (!target) {
                continue;
            }
            NodePath ptarget = p_original->get_path_to(target);

            Node *copytarget = target;

            // Atempt to find a path to the duplicate target, if it seems it's not part
            // of the duplicated and not yet parented hierarchy then at least try to connect
            // to the same target as the original

            if (p_copy->has_node(ptarget))
                copytarget = p_copy->get_node(ptarget);

            if (copy && copytarget) {
                const Callable copy_callable = Callable(copytarget, E.callable.get_method());
                if (!copy->is_connected(E.signal.get_name(), copy_callable)) {
                    copy->connect(E.signal.get_name(), copy_callable, E.flags);
                }
            }
        }
    }

    for (int i = 0; i < get_child_count(); i++) {
        get_child(i)->_duplicate_signals(p_original, p_copy);
    }
}

Node *Node::duplicate_and_reown(const HashMap<Node *, Node *> &p_reown_map) const {

    ERR_FAIL_COND_V(!get_filename().empty(), nullptr);

    Object *obj = ClassDB::instance(get_class_name());
    ERR_FAIL_COND_V_MSG(!obj, nullptr, "Node: Could not duplicate: " + String(get_class()) + ".");

    Node *node = object_cast<Node>(obj);
    if (!node) {
        memdelete(obj);
        ERR_FAIL_V_MSG(nullptr, "Node: Could not duplicate: " + String(get_class()) + ".");
    }
    node->set_name(get_name());

    Vector<PropertyInfo> plist;

    get_property_list(&plist);

    for (PropertyInfo &E : plist) {

        if (!(E.usage & PROPERTY_USAGE_STORAGE))
            continue;
        StringName name = E.name;
        node->set(name, get(name));
    }

    Vector<GroupInfo> groups;
    get_groups(&groups);

    for (const GroupInfo &E : groups)
        node->add_to_group(E.name, E.persistent);

    for (int i = 0; i < get_child_count(); i++) {

        get_child(i)->_duplicate_and_reown(node, p_reown_map);
    }

    // Duplication of signals must happen after all the node descendants have been copied,
    // because re-targeting of connections from some descendant to another is not possible
    // if the emitter node comes later in tree order than the receiver
    _duplicate_signals(this, node);
    return node;
}

static void find_owned_by(Node *p_by, Node *p_node, Vector<Node *> &p_owned) {

    if (p_node->get_owner() == p_by)
        p_owned.push_back(p_node);

    for(Node *chld : p_node->children()) {
        find_owned_by(p_by, chld, p_owned);
    }
}

//TODO: replace data was never used, so it was commented out.
//struct _NodeReplaceByPair {
//    StringName name;
//    Variant value;
//};

void Node::replace_by(Node *p_node, bool p_keep_data) {

    ERR_FAIL_NULL(p_node);
    ERR_FAIL_COND(p_node->priv_data->parent);

    Vector<Node *> owned(priv_data->owned);
    Vector<Node *> owned_by_owner;
    Node *owner = (priv_data->owner == this) ? p_node : priv_data->owner;

    //Vector<_NodeReplaceByPair> replace_data;

    if (p_keep_data) {

//        Vector<PropertyInfo> plist;
//        get_property_list(&plist);

//        for (const PropertyInfo &E : plist) {

//            _NodeReplaceByPair rd;
//            if (!(E.usage & PROPERTY_USAGE_STORAGE))
//                continue;
//            rd.name = E.name;
//            rd.value = get(rd.name);
//        }

        Vector<GroupInfo> groups;
        get_groups(&groups);

        for (const GroupInfo &E : groups)
            p_node->add_to_group(E.name, E.persistent);
    }

    _replace_connections_target(p_node);

    if (priv_data->owner) {
        for (int i = 0; i < get_child_count(); i++)
            find_owned_by(priv_data->owner, get_child(i), owned_by_owner);
    }

    Node *parent = priv_data->parent;
    int pos_in_parent = priv_data->pos;

    if (priv_data->parent) {

        parent->remove_child(this);
        parent->add_child(p_node);
        parent->move_child(p_node, pos_in_parent);
    }

    while (get_child_count()) {

        Node *child = get_child(0);
        remove_child(child);
        if (!child->is_owned_by_parent()) {
            // add the custom children to the p_node
            p_node->add_child(child);
        }
    }

    p_node->set_owner(owner);

    for (auto & e: owned) {
        e->set_owner(p_node);
    }

    for (Node * n: owned_by_owner) {
        n->set_owner(owner);
    }

    p_node->set_filename(get_filename());

//    for (_NodeReplaceByPair & E : replace_data) {
//        p_node->set(E.name, E.value);
//    }
}

void Node::_replace_connections_target(Node *p_new_target) {

    Vector<Connection> cl;
    get_signals_connected_to_this(&cl);

    for (Connection &c : cl) {

        if (c.flags & ObjectNS::CONNECT_PERSIST) {

            c.signal.get_object()->disconnect(c.signal.get_name(), Callable(this, c.callable.get_method()));
            bool valid = p_new_target->has_method(c.callable.get_method())
                    || !refFromRefPtr<Script>(p_new_target->get_script())
                    || refFromRefPtr<Script>(p_new_target->get_script())->has_method(c.callable.get_method());
            ERR_CONTINUE_MSG(!valid, String("Attempt to connect signal '") + c.signal.get_object()->get_class() + "." + c.signal.get_name() + "' to nonexistent method '" + c.callable.get_object()->get_class() + "." + c.callable.get_method() + "'.");
            c.signal.get_object()->connect(c.signal.get_name(), Callable(p_new_target, c.callable.get_method()), /*c.binds,*/ c.flags);
        }
    }
}

bool Node::has_node_and_resource(const NodePath &p_path) const {

    if (!has_node(p_path))
        return false;
    RES res;
    Vector<StringName> leftover_path;
    Node *node = get_node_and_resource(p_path, res, leftover_path, false);

    return node;
}

Array Node::_get_node_and_resource(const NodePath &p_path) {

    RES res;
    Vector<StringName> leftover_path;
    Node *node = get_node_and_resource(p_path, res, leftover_path, false);
    Array result;

    if (node)
        result.push_back(Variant(node));
    else
        result.push_back(Variant());

    if (res)
        result.push_back(res);
    else
        result.push_back(Variant());

    result.push_back(NodePath(eastl::move(Vector<StringName>()), eastl::move(leftover_path), false));

    return result;
}

Node *Node::get_node_and_resource(const NodePath &p_path, RES &r_res, Vector<StringName> &r_leftover_subpath, bool p_last_is_property) const {

    Node *node = get_node(p_path);
    r_res = RES();
    r_leftover_subpath = Vector<StringName>();
    if (!node) {
        return nullptr;
    }

    if (p_path.get_subname_count()==0) {
        return node;
    }
    // Global resource reference, -> material animations etc.
    if(StringView(p_path.get_subname(0)).starts_with('@')){
        int j=0;
        for (; j < p_path.get_subname_count() - (int)p_last_is_property; j++) {
            Variant new_res_v = j == 0 ? Variant(gResourceManager().load(StringView(p_path.get_subname(0)).substr(1))) : r_res->get(p_path.get_subname(j));

            if (new_res_v.get_type() == VariantType::NIL) { // Found nothing on that path
                return nullptr;
            }

            RES new_res(refFromVariant<Resource>(new_res_v));

            if (not new_res) { // No longer a resource, assume property
                break;
            }

            r_res = new_res;
        }
        for (; j < p_path.get_subname_count(); j++) {
            // Put the rest of the subpath in the leftover path
            r_leftover_subpath.push_back(p_path.get_subname(j));
        }
        return node;
    }

    int j = 0;
    // If not p_last_is_property, we shouldn't consider the last one as part of the resource
    for (; j < p_path.get_subname_count() - (int)p_last_is_property; j++) {
        Variant new_res_v = j == 0 ? node->get(p_path.get_subname(j)) : r_res->get(p_path.get_subname(j));

        if (new_res_v.get_type() == VariantType::NIL) { // Found nothing on that path
            return nullptr;
        }

        RES new_res(refFromVariant<Resource>(new_res_v));

        if (not new_res) { // No longer a resource, assume property
            break;
        }

        r_res = new_res;
    }
    for (; j < p_path.get_subname_count(); j++) {
        // Put the rest of the subpath in the leftover path
        r_leftover_subpath.push_back(p_path.get_subname(j));
    }

    return node;
}

void Node::_set_tree(SceneTree *p_tree) {

    SceneTree *tree_changed_a = nullptr;
    SceneTree *tree_changed_b = nullptr;
    //TODO: consider case where tree==p_tree

    //ERR_FAIL_COND(p_scene && data->parent && !data->parent->data->scene); //nobug if both are null

    if (tree) {
        _propagate_exit_tree();

        tree_changed_a = tree;
    }

    tree = p_tree;

    if (tree) {

        _propagate_enter_tree();
        if (!priv_data->parent || priv_data->parent->priv_data->ready_notified) { // No parent (root) or parent ready
            _propagate_ready(); //reverse_notification(NOTIFICATION_READY);
        }

        tree_changed_b = tree;
    }

    if (tree_changed_a)
        tree_changed_a->tree_changed();
    if (tree_changed_a!=tree_changed_b && tree_changed_b)
        tree_changed_b->tree_changed();
}

#ifdef DEBUG_ENABLED
static void _Node_debug_sn(Node *n) {

    if (n->is_inside_tree())
        return;

    Node *p = n;
    while (p->get_parent()) {
        p = p->get_parent();
    }

    StringName path;
    if (p == n)
        path = n->get_name();
    else
        path = StringName(String(p->get_name()) + "/" + (String)p->get_path_to(n));
    print_line(itos(entt::to_integral(n->get_instance_id())) + " - Stray Node: " + path + " (Type: " + n->get_class() + ")");
}
#endif // DEBUG_ENABLED

void Node::_print_stray_nodes() {

    print_stray_nodes();
}

void Node::print_stray_nodes() {

#ifdef DEBUG_ENABLED
    game_object_registry.lock_registry();
    game_object_registry.registry.each([](const GameEntity ent) {
        ObjectLink *link = game_object_registry.try_get<ObjectLink>(ent);
        Node *obj = link ? object_cast<Node>(link->object) : nullptr;
        if (obj) {
            _Node_debug_sn(obj);
        }
    });
    game_object_registry.unlock_registry();
#endif
}

void Node::queue_delete() {

    if (is_inside_tree()) {
        get_tree()->queue_delete(this);
    } else {
        SceneTree::get_singleton()->queue_delete(this);
    }
}

Array Node::_get_children() const {

    Array arr;
    int cc = get_child_count();
    arr.resize(cc);
    for (int i = 0; i < cc; i++)
        arr[i] = Variant(get_child(i));

    return arr;
}

void Node::set_import_path(const NodePath &p_import_path) {

#ifdef TOOLS_ENABLED
    priv_data->import_path = p_import_path;
#endif
}

NodePath Node::get_import_path() const {

#ifdef TOOLS_ENABLED
    return priv_data->import_path;
#else
    return NodePath();
#endif
}

void Node::clear_internal_tree_resource_paths() {

    clear_internal_resource_paths();
    for (size_t i = 0; i < priv_data->children.size(); i++) {
        priv_data->children[i]->clear_internal_tree_resource_paths();
    }
}

String Node::get_configuration_warning() const {

    if (get_script_instance() && get_script_instance()->get_script() &&
            get_script_instance()->has_method("_get_configuration_warning")) {
        return get_script_instance()->call("_get_configuration_warning").as<String>();
    }
    return String();
}

void Node::update_configuration_warning() {

#ifdef TOOLS_ENABLED
    if (!is_inside_tree())
        return;
    auto edited_root=get_tree()->get_edited_scene_root();
    if (edited_root && (edited_root == this || edited_root->is_a_parent_of(this))) {
        get_tree()->emit_signal(SceneStringNames::node_configuration_warning_changed, this);
    }
#endif
}

bool Node::is_owned_by_parent() const {
    return parent_owned;
}

void Node::set_display_folded(bool p_folded) {
    priv_data->display_folded = p_folded;
}

bool Node::is_displayed_folded() const {

    return priv_data->display_folded;
}

void Node::request_ready() {
    priv_data->ready_first = true;
}

void Node::_bind_methods() {

    GLOBAL_DEF("node/name_num_separator", 0);
    ProjectSettings::get_singleton()->set_custom_property_info("node/name_num_separator", PropertyInfo(VariantType::INT, "node/name_num_separator", PropertyHint::Enum, "None,Space,Underscore,Dash"));
    GLOBAL_DEF("node/name_casing", NAME_CASING_PASCAL_CASE);
    ProjectSettings::get_singleton()->set_custom_property_info("node/name_casing", PropertyInfo(VariantType::INT, "node/name_casing", PropertyHint::Enum, "PascalCase,camelCase,snake_case"));

    MethodBinder::bind_method(D_METHOD("add_child_below_node", {"node", "child_node", "legible_unique_name"}), &Node::add_child_below_node, {DEFVAL(false)});

    SE_BIND_METHOD(Node,set_name);
    SE_BIND_METHOD(Node,get_name);
    MethodBinder::bind_method(D_METHOD("add_child", {"node", "legible_unique_name"}), &Node::add_child, {DEFVAL(false)});
    SE_BIND_METHOD(Node,remove_child);
    SE_BIND_METHOD(Node,get_child_count);
    MethodBinder::bind_method(D_METHOD("get_children"), &Node::_get_children);
    SE_BIND_METHOD(Node,get_child);
    SE_BIND_METHOD(Node,has_node);
    SE_BIND_METHOD(Node,get_node);
    SE_BIND_METHOD(Node,get_node_or_null);
    SE_BIND_METHOD(Node,get_parent);
    SE_BIND_METHOD(Node,has_node_and_resource);
    MethodBinder::bind_method(D_METHOD("get_node_and_resource", {"path"}), &Node::_get_node_and_resource);

    SE_BIND_METHOD(Node,is_inside_tree);
    SE_BIND_METHOD(Node,is_a_parent_of);
    SE_BIND_METHOD(Node,is_greater_than);
    SE_BIND_METHOD(Node,get_path);
    SE_BIND_METHOD(Node,get_path_to);
    MethodBinder::bind_method(D_METHOD("add_to_group", {"group", "persistent"}), &Node::add_to_group, {DEFVAL(false)});
    SE_BIND_METHOD(Node,remove_from_group);
    SE_BIND_METHOD(Node,is_in_group);
    SE_BIND_METHOD(Node,move_child);
    MethodBinder::bind_method(D_METHOD("get_groups"), &Node::_get_groups);
    SE_BIND_METHOD(Node,raise);
    SE_BIND_METHOD(Node,set_owner);
    SE_BIND_METHOD(Node,get_owner);
    SE_BIND_METHOD(Node,remove_and_skip);
    SE_BIND_METHOD(Node,get_index);
    SE_BIND_METHOD(Node,print_tree);

    SE_BIND_METHOD(Node,set_filename);
    SE_BIND_METHOD(Node,get_filename);
    SE_BIND_METHOD(Node,propagate_notification);
    MethodBinder::bind_method(D_METHOD("propagate_call", {"method", "args", "parent_first"}), &Node::propagate_call, {DEFVAL(Array()), DEFVAL(false)});
    SE_BIND_METHOD(Node,set_physics_process);
    SE_BIND_METHOD(Node,get_physics_process_delta_time);
    SE_BIND_METHOD(Node,is_physics_processing);
    SE_BIND_METHOD(Node,get_process_delta_time);
    SE_BIND_METHOD(Node,set_process);
    SE_BIND_METHOD(Node,set_process_priority);
    SE_BIND_METHOD(Node,get_process_priority);
    SE_BIND_METHOD(Node,is_processing);
    SE_BIND_METHOD(Node,set_process_input);
    SE_BIND_METHOD(Node,is_processing_input);
    SE_BIND_METHOD(Node,set_process_unhandled_input);
    SE_BIND_METHOD(Node,is_processing_unhandled_input);
    SE_BIND_METHOD(Node,set_process_unhandled_key_input);
    SE_BIND_METHOD(Node,is_processing_unhandled_key_input);
    SE_BIND_METHOD(Node,set_pause_mode);
    SE_BIND_METHOD(Node,get_pause_mode);
    SE_BIND_METHOD(Node,can_process);
    MethodBinder::bind_method(D_METHOD("print_stray_nodes"), &Node::_print_stray_nodes);
    SE_BIND_METHOD(Node,get_position_in_parent);

    SE_BIND_METHOD(Node,set_display_folded);
    SE_BIND_METHOD(Node,is_displayed_folded);

    SE_BIND_METHOD(Node,set_process_internal);
    SE_BIND_METHOD(Node,is_processing_internal);

    SE_BIND_METHOD(Node,set_physics_process_internal);
    SE_BIND_METHOD(Node,is_physics_processing_internal);

    SE_BIND_METHOD(Node,get_tree);

    MethodBinder::bind_method(D_METHOD("duplicate", {"flags"}), &Node::duplicate, {DEFVAL(DUPLICATE_USE_INSTANCING | DUPLICATE_SIGNALS | DUPLICATE_GROUPS | DUPLICATE_SCRIPTS)});
    MethodBinder::bind_method(D_METHOD("replace_by", {"node", "keep_data"}), &Node::replace_by, {DEFVAL(false)});

    SE_BIND_METHOD(Node,set_scene_instance_load_placeholder);
    SE_BIND_METHOD(Node,get_scene_instance_load_placeholder);

    SE_BIND_METHOD(Node,get_viewport);

    MethodBinder::bind_method(D_METHOD("queue_free"), &Node::queue_delete);

    SE_BIND_METHOD(Node,request_ready);

    MethodBinder::bind_method(D_METHOD("set_network_master", {"id", "recursive"}), &Node::set_network_master, {DEFVAL(true)});
    SE_BIND_METHOD(Node,get_network_master);

    SE_BIND_METHOD(Node,is_network_master);

    SE_BIND_METHOD(Node,get_multiplayer);
    SE_BIND_METHOD(Node,get_custom_multiplayer);
    SE_BIND_METHOD(Node,set_custom_multiplayer);
    SE_BIND_METHOD(Node,rpc_config);
    SE_BIND_METHOD(Node,rset_config);

    MethodBinder::bind_method(D_METHOD("_set_editor_description", {"editor_description"}), &Node::set_editor_description);
    MethodBinder::bind_method(D_METHOD("_get_editor_description"), &Node::get_editor_description);
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "editor_description", PropertyHint::MultilineText, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_editor_description", "_get_editor_description");


    MethodBinder::bind_method(D_METHOD("_set_import_path", {"import_path"}), &Node::set_import_path);
    SE_BIND_METHOD(Node,get_import_path);
    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "_import_path", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_import_path", "get_import_path");

    {
        MethodInfo mi("rpc",PropertyInfo(VariantType::STRING_NAME, "method"));
        MethodBinder::bind_vararg_method("rpc", &Node::_rpc_bind, eastl::move(mi));
    }
    {
        MethodInfo mi("rpc_unreliable",PropertyInfo(VariantType::STRING_NAME, "method"));
        MethodBinder::bind_vararg_method("rpc_unreliable", &Node::_rpc_unreliable_bind, eastl::move(mi));
    }
    {
        MethodInfo mi("rpc_id",PropertyInfo(VariantType::INT, "peer_id"),PropertyInfo(VariantType::STRING_NAME, "method"));
        MethodBinder::bind_vararg_method( "rpc_id", &Node::_rpc_id_bind, eastl::move(mi));
    }
    {
        MethodInfo mi("rpc_unreliable_id",PropertyInfo(VariantType::INT, "peer_id"),PropertyInfo(VariantType::STRING_NAME, "method"));
        MethodBinder::bind_vararg_method( "rpc_unreliable_id", &Node::_rpc_unreliable_id_bind, eastl::move(mi));
    }

    SE_BIND_METHOD(Node,rset);
    SE_BIND_METHOD(Node,rset_id);
    SE_BIND_METHOD(Node,rset_unreliable);
    SE_BIND_METHOD(Node,rset_unreliable_id);

    SE_BIND_METHOD(Node,update_configuration_warning);

    BIND_CONSTANT(NOTIFICATION_ENTER_TREE);
    BIND_CONSTANT(NOTIFICATION_EXIT_TREE);
    BIND_CONSTANT(NOTIFICATION_MOVED_IN_PARENT);
    BIND_CONSTANT(NOTIFICATION_READY);
    BIND_CONSTANT(NOTIFICATION_PAUSED);
    BIND_CONSTANT(NOTIFICATION_UNPAUSED);
    BIND_CONSTANT(NOTIFICATION_PHYSICS_PROCESS);
    BIND_CONSTANT(NOTIFICATION_PROCESS);
    BIND_CONSTANT(NOTIFICATION_PARENTED);
    BIND_CONSTANT(NOTIFICATION_UNPARENTED);
    BIND_CONSTANT(NOTIFICATION_INSTANCED);
    BIND_CONSTANT(NOTIFICATION_DRAG_BEGIN);
    BIND_CONSTANT(NOTIFICATION_DRAG_END);
    BIND_CONSTANT(NOTIFICATION_PATH_CHANGED);
    BIND_CONSTANT(NOTIFICATION_INTERNAL_PROCESS);
    BIND_CONSTANT(NOTIFICATION_INTERNAL_PHYSICS_PROCESS);
    BIND_CONSTANT(NOTIFICATION_POST_ENTER_TREE);

    BIND_CONSTANT(NOTIFICATION_WM_MOUSE_ENTER);
    BIND_CONSTANT(NOTIFICATION_WM_MOUSE_EXIT);
    BIND_CONSTANT(NOTIFICATION_WM_FOCUS_IN);
    BIND_CONSTANT(NOTIFICATION_WM_FOCUS_OUT);
    BIND_CONSTANT(NOTIFICATION_WM_QUIT_REQUEST);
    BIND_CONSTANT(NOTIFICATION_WM_GO_BACK_REQUEST);
    BIND_CONSTANT(NOTIFICATION_WM_UNFOCUS_REQUEST);
    BIND_CONSTANT(NOTIFICATION_OS_MEMORY_WARNING);
    BIND_CONSTANT(NOTIFICATION_TRANSLATION_CHANGED);
    BIND_CONSTANT(NOTIFICATION_WM_ABOUT);
    BIND_CONSTANT(NOTIFICATION_CRASH);
    BIND_CONSTANT(NOTIFICATION_OS_IME_UPDATE);
    BIND_CONSTANT(NOTIFICATION_APP_RESUMED);
    BIND_CONSTANT(NOTIFICATION_APP_PAUSED);

    BIND_ENUM_CONSTANT(PAUSE_MODE_INHERIT);
    BIND_ENUM_CONSTANT(PAUSE_MODE_STOP);
    BIND_ENUM_CONSTANT(PAUSE_MODE_PROCESS);

    BIND_ENUM_CONSTANT(DUPLICATE_SIGNALS);
    BIND_ENUM_CONSTANT(DUPLICATE_GROUPS);
    BIND_ENUM_CONSTANT(DUPLICATE_SCRIPTS);
    BIND_ENUM_CONSTANT(DUPLICATE_USE_INSTANCING);

    ADD_SIGNAL(MethodInfo("ready"));
    ADD_SIGNAL(MethodInfo("renamed"));
    ADD_SIGNAL(MethodInfo("tree_entered"));
    ADD_SIGNAL(MethodInfo("tree_exiting"));
    ADD_SIGNAL(MethodInfo("tree_exited"));
    ADD_SIGNAL(MethodInfo("child_entered_tree",
            PropertyInfo(VariantType::OBJECT, "node", PropertyHint::None, "", PROPERTY_USAGE_DEFAULT, "Node")));
    ADD_SIGNAL(MethodInfo(SceneStringNames::child_exiting_tree,
            PropertyInfo(VariantType::OBJECT, "node", PropertyHint::None, "", PROPERTY_USAGE_DEFAULT, "Node")));

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "pause_mode", PropertyHint::Enum, "Inherit,Stop,Process"), "set_pause_mode", "get_pause_mode");

#ifdef ENABLE_DEPRECATED
    //no longer exists, but remains for compatibility (keep previous scenes folded
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "editor/display_folded", PropertyHint::None, "", 0), "set_display_folded", "is_displayed_folded");
#endif

    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "name", PropertyHint::None, "", 0), "set_name", "get_name");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "filename", PropertyHint::None, "", 0), "set_filename", "get_filename");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "owner", PropertyHint::ResourceType, "Node", 0), "set_owner", "get_owner");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "multiplayer", PropertyHint::ResourceType, "MultiplayerAPI", 0), "", "get_multiplayer");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "custom_multiplayer", PropertyHint::ResourceType, "MultiplayerAPI", 0), "set_custom_multiplayer", "get_custom_multiplayer");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "process_priority"), "set_process_priority", "get_process_priority");
    // Disabled for now
    // ADD_PROPERTY(PropertyInfo(Variant::BOOL, "physics_interpolated"), "set_physics_interpolated",
    // "is_physics_interpolated");


    BIND_VMETHOD(MethodInfo("_process", PropertyInfo(VariantType::FLOAT, "delta")));
    BIND_VMETHOD(MethodInfo("_physics_process", PropertyInfo(VariantType::FLOAT, "delta")));
    BIND_VMETHOD(MethodInfo("_enter_tree"));
    BIND_VMETHOD(MethodInfo("_exit_tree"));
    BIND_VMETHOD(MethodInfo("_ready"));
    BIND_VMETHOD(MethodInfo("_input", PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEvent")));
    BIND_VMETHOD(MethodInfo("_unhandled_input", PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEvent")));
    BIND_VMETHOD(MethodInfo("_unhandled_key_input", PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEventKey")));
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_get_configuration_warning"));
}

Node::Node() : tree(nullptr), viewport(nullptr), blocked(0), process_priority(0), inside_tree(false), parent_owned(false) {
    //TODO: SEGS: create a pool allocator for Node::PrivData !
    priv_data = memnew_basic(Node::PrivData);
    priv_data->pos = -1;
    priv_data->depth = -1;
    priv_data->parent = nullptr;
    priv_data->physics_process = false;
    priv_data->idle_process = false;
    priv_data->physics_process_internal = false;
    priv_data->idle_process_internal = false;
    priv_data->ready_notified = false;
    priv_data->use_identity_transform = false;
    priv_data->owner = nullptr;
    priv_data->OW = nullptr;
    priv_data->input = false;
    priv_data->unhandled_input = false;
    priv_data->unhandled_key_input = false;
    priv_data->pause_mode = PAUSE_MODE_INHERIT;
    priv_data->pause_owner = nullptr;
    priv_data->network_master = 1; //server by default
    priv_data->path_cache = nullptr;
    priv_data->in_constructor = true;
    priv_data->use_placeholder = false;
    priv_data->display_folded = false;
    priv_data->ready_first = true;
    priv_data->editable_instance = false;

    orphan_node_count++;
}

Node::~Node() {
    delete priv_data->filename;
    priv_data->grouped.clear();
    priv_data->owned.clear();
    priv_data->children.clear();

    ERR_FAIL_COND(priv_data->parent);
    ERR_FAIL_COND(priv_data->children.size());
    memdelete(priv_data);
    orphan_node_count--;
}

#ifdef TOOLS_ENABLED
Node *_find_script_node(Node *p_edited_scene, Node *p_current_node, const Ref<Script> &script) {

    if (p_edited_scene != p_current_node && p_current_node->get_owner() != p_edited_scene)
        return nullptr;

    Ref scr(refFromRefPtr<Script>(p_current_node->get_script()));

    if (scr && scr == script)
        return p_current_node;

    for (int i = 0; i < p_current_node->get_child_count(); i++) {
        Node *n = _find_script_node(p_edited_scene, p_current_node->get_child(i), script);
        if (n)
            return n;
    }
    return nullptr;
}

#endif
////////////////////////////////
void mark_dirty_xform(GameEntity ge)
{
    game_object_registry.registry.emplace_or_replace<DirtXFormMarker>(ge);
}

void mark_clean_xform(GameEntity ge)
{
    game_object_registry.registry.remove<DirtXFormMarker>(ge);

}

bool is_dirty_xfrom(GameEntity ge)
{
    return game_object_registry.registry.all_of<DirtXFormMarker>(ge);
}
