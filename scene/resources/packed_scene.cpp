/*************************************************************************/
/*  packed_scene.cpp                                                     */
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

#include "packed_scene.h"

#include "packed_scene_tooling.h"

#include "core/core_string_names.h"
#include "core/engine.h"
#include "core/method_bind.h"
#include "core/pair.h"
#include "core/pool_vector.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "core/string_formatter.h"
#include "core/resource/resource_manager.h"
#include "scene/property_utils.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/node_3d.h"
#include "scene/gui/control.h"
#include "scene/main/instance_placeholder.h"

#include "EASTL/deque.h"
#include "EASTL/sort.h"

namespace {
struct SVCompare
{
    bool operator()(const StringName &a,StringView b) const {
        return StringView(a.asCString())<b;
    }
    bool operator()(StringView a,const StringName &b) const {
        return a<StringView(b.asCString());
    }
};
Node* nodeFromId(const Vector<NodePath>& node_paths, Span<Node*> ret_nodes, int nc, int p_id) {

    Node* p_name;
    if (p_id & SceneState::FLAG_ID_IS_PATH) {
        const NodePath &np = node_paths[p_id & SceneState::FLAG_MASK];
        p_name = ret_nodes[0]->get_node_or_null(np);
    }
    else {
        ERR_FAIL_INDEX_V(p_id & SceneState::FLAG_MASK, nc, nullptr);
        p_name = ret_nodes[p_id & SceneState::FLAG_MASK];
    }
    return p_name;
}
int _nm_get_string(const StringName& p_string, Map<StringName, int>& name_map) {

    if (name_map.contains(p_string))
        return name_map[p_string];

    int idx = name_map.size();
    name_map[p_string] = idx;
    return idx;
}
int _nm_get_string(StringView p_string, Map<StringName, int>& name_map) {
    //FIXME: this allocates strings, but name_map.find_as does not work here since StringName comparisons are done using data pointer
    auto iter = name_map.find(StringName(p_string));
    if (iter != name_map.end())
        return iter->second;

    int idx = name_map.size();
    name_map.emplace(StringName(p_string), idx);
    return idx;
}
int _vm_get_variant(const Variant& p_variant, HashMap<Variant, int, Hasher<Variant>, VariantComparator>& variant_map) {

    if (variant_map.contains(p_variant))
        return variant_map[p_variant];

    int idx = variant_map.size();
    variant_map[p_variant] = idx;
    return idx;
}
constexpr int PACKED_SCENE_VERSION = 2;
} //end of anonymous namespace


IMPL_GDCLASS(SceneState)
IMPL_GDCLASS(PackedScene)
RES_BASE_EXTENSION_IMPL(PackedScene,"scn")
VARIANT_ENUM_CAST(PackedGenEditState);

static Array _sanitize_node_pinned_properties(Node *p_node) {
    if (!p_node->has_meta("_edit_pinned_properties_")) {
        return Array();
    }
    Array pinned = p_node->get_meta("_edit_pinned_properties_").as<Array>();
    if (pinned.empty()) {
        return Array();
    }
    Set<StringName> storable_properties;
    p_node->get_storable_properties(storable_properties);
    int i = 0;
    do {
        if (storable_properties.contains(pinned[i].as<StringName>())) {
            i++;
        } else {
            pinned.remove(i);
        }
    } while (i < pinned.size());
    if (pinned.empty()) {
        p_node->remove_meta("_edit_pinned_properties_");
    }
    return pinned;
}
bool SceneState::can_instance() const {

    return !nodes.empty();
}
bool SceneState::handleProperties(PackedGenEditState p_edit_state, Node *node,Span<Node *> ret_nodes, const SceneState::NodeData &n, Map<Ref<Resource>, Ref<Resource> > & resources_local_to_scene) const {
    int nprop_count = n.properties.size();
    if (!nprop_count)
        return true;

    size_t sname_count = names.size();
    const Variant* props = nullptr;
    int prop_count = variants.size();
    if (prop_count)
        props = &variants[0];
    int i = eastl::distance(nodes.data(),&n); // find out which not are we on

    for (const auto &property : n.properties) {

        bool valid;
        ERR_FAIL_INDEX_V(property.name, sname_count, false);
        ERR_FAIL_INDEX_V(property.value, prop_count, false);

        if (names[property.name] == CoreStringNames::get_singleton()->_script) {
            //work around to avoid old script variables from disappearing, should be the proper fix to:
            //https://github.com/godotengine/godot/issues/2958

            //store old state
            Vector<Pair<StringName, Variant> > old_state;
            if (node->get_script_instance()) {
                node->get_script_instance()->get_property_state(old_state);
            }

            node->set(names[property.name], props[property.value], &valid);

            //restore old state for new script, if exists
            for (const Pair<StringName, Variant>& E : old_state) {
                node->set(E.first, E.second);
            }
            continue;
        }
        Variant value = props[property.value];

        if (value.get_type() == VariantType::OBJECT) {
            //handle resources that are local to scene by duplicating them if needed
            Ref<Resource> res(value);
            if (res && res->is_local_to_scene()) {
                Map<Ref<Resource>, Ref<Resource> >::const_iterator E = resources_local_to_scene.find(res);

                if (E != resources_local_to_scene.end()) {
                    value = E->second;
                }
                else {

                    Node* base = i == 0 ? node : ret_nodes[0];

                    if (p_edit_state == GEN_EDIT_STATE_MAIN || p_edit_state == GEN_EDIT_STATE_MAIN_INHERITED) {
                        //for the main scene, use the resource as is
                        res->configure_for_local_scene(base, resources_local_to_scene);
                        resources_local_to_scene[res] = res;

                    }
                    else {
                        //for instances, a copy must be made
                        Node* base2 = i == 0 ? node : ret_nodes[0];
                        Ref<Resource> local_dupe = res->duplicate_for_local_scene(base2, resources_local_to_scene);
                        resources_local_to_scene[res] = local_dupe;
                        res = local_dupe;
                        value = local_dupe;
                    }
                }
                //must make a copy, because this res is local to scene
            }
        }
        else if (p_edit_state == GEN_EDIT_STATE_INSTANCE) {
            value = value.duplicate(true); // Duplicate arrays and dictionaries for the editor
        }
        node->set(names[property.name], value, &valid);
    }
    return true;
}

void SceneState::handleConnections(int nc, Span<Node*> ret_nodes) const {

    for (const ConnectionData& c : connections) {
        Node* cfrom = nodeFromId(node_paths, ret_nodes, nc, c.from);
        Node* cto = nodeFromId(node_paths, ret_nodes, nc, c.to);

        if (!cfrom || !cto)
            continue;

        cfrom->connect(names[c.signal], Callable(cto, names[c.method]), ObjectNS::CONNECT_PERSIST | c.flags);

    }
}

Node *SceneState::instance(PackedGenEditState p_edit_state) const {

    // nodes where instancing failed (because something is missing)
    Vector<Node *> stray_instances;

    int nc = nodes.size();
    ERR_FAIL_COND_V(nodes.empty(), nullptr);

    const Vector<StringName> &snames(names);
    size_t sname_count = names.size();

    //Vector<Variant> properties;
    //TODO: SEGS: introduce stack-based allocator for temporary vectors
    FixedVector<Node*,1024,true> ret_nodes(nodes.size());

    bool gen_node_path_cache = p_edit_state != GEN_EDIT_STATE_DISABLED && node_path_cache.empty();

    Map<Ref<Resource>, Ref<Resource> > resources_local_to_scene;
    bool first_node=true;
    int node_data_idx=-1;

    for (const NodeData& n : nodes) {
        node_data_idx++;
        first_node = node_data_idx==0;

        Node *parent = nullptr;

        if (!first_node) {

            ERR_FAIL_COND_V_MSG(n.parent == -1, nullptr, FormatVE("Invalid scene: node %s does not specify its parent node.", snames[n.name].asCString()));
            parent = nodeFromId(node_paths, ret_nodes, nc, n.parent);
#ifdef DEBUG_ENABLED
            if (!parent && n.parent & FLAG_ID_IS_PATH) {

                WARN_PRINT("Parent path '" + (String)node_paths[n.parent & FLAG_MASK] + "' for node '" + snames[n.name] + "' has vanished when instancing: '" + (String)get_path() + "'.");
            }
#endif
        }
        else {
            ERR_FAIL_COND_V_MSG(n.parent != -1, nullptr, FormatVE("Invalid scene: root node %s cannot specify a parent node.", snames[n.name].asCString()));
            ERR_FAIL_COND_V_MSG(n.type == TYPE_INSTANCED && base_scene_idx < 0, nullptr, FormatVE("Invalid scene: root node %s in an instance, but there's no base scene.", snames[n.name].asCString()));
        }
        Node *node = nullptr;

        if (first_node && base_scene_idx >= 0) {
            //scene inheritance on root node
            Ref<PackedScene> sdata(variants[base_scene_idx]);
            ERR_FAIL_COND_V(not sdata, nullptr);
            node = sdata->instance(p_edit_state == GEN_EDIT_STATE_DISABLED ? GEN_EDIT_STATE_DISABLED : GEN_EDIT_STATE_INSTANCE); //only main gets main edit state
            ERR_FAIL_COND_V(!node, nullptr);
            if (p_edit_state != GEN_EDIT_STATE_DISABLED) {
                node->set_scene_inherited_state(sdata->get_state());
            }

        } else if (n.instance >= 0) {
            //instance a scene into this node
            if (n.instance & FLAG_INSTANCE_IS_PLACEHOLDER) {

                String path = variants[n.instance & FLAG_MASK].as<String>();
                if (disable_placeholders) {

                    Ref<PackedScene> sdata = dynamic_ref_cast<PackedScene>(gResourceManager().load(path, "PackedScene"));
                    ERR_FAIL_COND_V(not sdata, nullptr);
                    node = sdata->instance(p_edit_state == GEN_EDIT_STATE_DISABLED ? GEN_EDIT_STATE_DISABLED : GEN_EDIT_STATE_INSTANCE);
                    ERR_FAIL_COND_V(!node, nullptr);
                } else {
                    InstancePlaceholder *ip = memnew(InstancePlaceholder);
                    ip->set_instance_path(path);
                    node = ip;
                }
                node->set_scene_instance_load_placeholder(true);
            } else {
                Ref<PackedScene> sdata(variants[n.instance & FLAG_MASK]);
                ERR_FAIL_COND_V(not sdata, nullptr);
                node = sdata->instance(p_edit_state == GEN_EDIT_STATE_DISABLED ? GEN_EDIT_STATE_DISABLED : GEN_EDIT_STATE_INSTANCE);
                ERR_FAIL_COND_V(!node, nullptr);
            }

        } else if (n.type == TYPE_INSTANCED) {
            //get the node from somewhere, it likely already exists from another instance
            if (parent) {
                node = parent->_get_child_by_name(snames[n.name]);
#ifdef DEBUG_ENABLED
                if (!node) {
                    WARN_PRINT("Node '" + ret_nodes.front()->get_path_to(parent).asString() + "/" + snames[n.name] + "' was modified from inside an instance, but it has vanished.");
                }
#endif
            }
        } else  {
            //node belongs to this scene and must be created
            Object *obj = ClassDB::instance(snames[n.type]);
            node = object_cast<Node>(obj);
            if (!node) {

                memdelete(obj);
                obj = nullptr;

                WARN_PRINT(FormatSN("Node %s of type %s cannot be created. A placeholder will be created instead.", snames[n.name].asCString(), snames[n.type].asCString()));
                if (n.parent >= 0 && n.parent < nc && ret_nodes[n.parent]) {
                    if (object_cast<Node3D>(ret_nodes[n.parent])) {
                        obj = memnew(Node3D);
                    } else if (object_cast<Control>(ret_nodes[n.parent])) {
                        obj = memnew(Control);
                    } else if (object_cast<Node2D>(ret_nodes[n.parent])) {
                        obj = memnew(Node2D);
                    }
                }

                if (!obj) {
                    obj = memnew(Node);
                }
            }

            node = object_cast<Node>(obj);

        }

        if (node) {
            // may not have found the node (part of instanced scene and removed)
            // if found all is good, otherwise ignore

            //properties
            if(not handleProperties(p_edit_state,node,ret_nodes,n,resources_local_to_scene))
                return nullptr;

            //name

            //groups
            for (int grp : n.groups) {

                ERR_FAIL_INDEX_V(grp, sname_count, nullptr);
                node->add_to_group(snames[grp], true);
            }

            if (n.instance >= 0 || n.type != TYPE_INSTANCED || first_node) {
                //if node was not part of instance, must set its name, parenthood and ownership
                if (!first_node) {
                    if (parent) {
                        parent->_add_child_nocheck(node, snames[n.name]);
                        if (n.index >= 0 && n.index < parent->get_child_count() - 1)
                            parent->move_child(node, n.index);
                    } else {
                        //it may be possible that an instanced scene has changed
                        //and the node has nowhere to go anymore
                        stray_instances.push_back(node); //can't be added, go to stray list
                    }
                } else {
                    if (Engine::get_singleton()->is_editor_hint()) {
                        //validate name if using editor, to avoid broken
                        node->set_name(snames[n.name]);
                    } else {
                        node->_set_name_nocheck(snames[n.name]);
                    }
                }
            }

            if (n.owner >= 0) {
                Node* owner = nodeFromId(node_paths, ret_nodes, nc, n.owner);
                if (owner)
                    node->_set_owner_nocheck(owner);
            }
            // we only want to deal with pinned flag if instancing as pure main (no instance, no inheriting)
            if (p_edit_state == GEN_EDIT_STATE_MAIN) {
                _sanitize_node_pinned_properties(node);
            } else {
                node->remove_meta("_edit_pinned_properties_");
            }
        }

        ret_nodes[node_data_idx] = node;

        if (node && gen_node_path_cache && ret_nodes[0]) {
            NodePath n2 = ret_nodes[0]->get_path_to(node);
            node_path_cache[n2] = node_data_idx;
        }
    }

    for (eastl::pair<const Ref<Resource>,Ref<Resource> > &E : resources_local_to_scene) {

        E.second->setup_local_to_scene();
    }

    //do connections

    handleConnections(nc, ret_nodes);

    //Node *s = ret_nodes[0];

    //remove nodes that could not be added, likely as a result that
    for(Node * n : stray_instances) {
        memdelete(n);
    }

    for (const auto &editable_instance : editable_instances) {
        Node *ei = ret_nodes[0]->get_node_or_null(editable_instance);
        if (ei) {
            ret_nodes[0]->set_editable_instance(ei, true);
        }
    }

    return ret_nodes[0];
}

Error SceneState::_parse_node(Node *p_owner, Node *p_node, int p_parent_idx, Map<StringName, int> &name_map, HashMap<Variant, int, Hasher<Variant>, VariantComparator> &variant_map, HashMap<Node *, int> &node_map, HashMap<Node *, int> &nodepath_map) {

    // this function handles all the work related to properly packing scenes, be it
    // instanced or inherited.
    // given the complexity of this process, an attempt will be made to properly
    // document it. if you fail to understand something, please ask!

    //discard nodes that do not belong to be processed
    if (p_node != p_owner && p_node->get_owner() != p_owner && !p_owner->is_editable_instance(p_node->get_owner()))
        return OK;

    bool is_editable_instance = false;

    // save the child instantiated scenes that are chosen as editable, so they can be restored
    // upon load back
    if (p_node != p_owner && !p_node->get_filename().empty() && p_owner->is_editable_instance(p_node)) {
        editable_instances.push_back(p_owner->get_path_to(p_node));
        // Node is the root of an editable instance.
        is_editable_instance = true;
    } else if (p_node->get_owner() && p_owner->is_a_parent_of(p_node->get_owner()) && p_owner->is_editable_instance(p_node->get_owner())) {
        // Node is part of an editable instance.
        is_editable_instance = true;
    }

    NodeData nd;

    nd.name = _nm_get_string(p_node->get_name(), name_map);
    nd.instance = -1; //not instanced by default
    //The default -1 index is set if the node is a scene root, so its index is irrelevant or it belongs to saved scene and scene is not inherited
    nd.index = -1;

    //really convoluted condition, but it basically checks that index is only saved when part of an inherited scene OR the node parent is from the edited scene
    if (p_owner->get_scene_inherited_state() ||
            (p_node != p_owner &&
            (p_node->get_owner() != p_owner ||
             (p_node->get_parent() != p_owner && p_node->get_parent()->get_owner() != p_owner)))) {
        //part of an inherited scene, or parent is from an instanced scene
        nd.index = p_node->get_index();
    }
    // if this node is part of an instanced scene or sub-instanced scene
    // we need to get the corresponding instance states.
    // with the instance states, we can query for identical properties/groups
    // and only save what has changed

    bool instanced_by_owner = false;
    Dequeue<PackState> states_stack =
            PropertyUtils::get_node_states_stack(p_node, p_owner, &instanced_by_owner);

    if (!p_node->get_filename().empty() && p_node->get_owner() == p_owner && instanced_by_owner) {

        if (p_node->get_scene_instance_load_placeholder()) {
            //it's a placeholder, use the placeholder path
            nd.instance = _vm_get_variant(p_node->get_filename(), variant_map);
            nd.instance |= FLAG_INSTANCE_IS_PLACEHOLDER;
        } else {
            //must instance ourselves
            Ref<PackedScene> instance = gResourceManager().loadT<PackedScene>(p_node->get_filename());
            if (!instance) {
                return ERR_CANT_OPEN;
            }

            nd.instance = _vm_get_variant(instance, variant_map);
        }
    }


    // all setup, we then proceed to check all properties for the node
    // and save the ones that are worth saving

    Vector<PropertyInfo> plist;
    p_node->get_property_list(&plist);
    Array pinned_props = _sanitize_node_pinned_properties(p_node);

    for (const PropertyInfo &E : plist) {

        if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
            continue;
        }

        Variant forced_value;

        // If instance or inheriting, not saving if property requested so, or it's meta
        if (!states_stack.empty()) {
            if ((E.usage & PROPERTY_USAGE_NO_INSTANCE_STATE)) {
                continue;
            }

            // Meta is normally not saved in instances/inherited (see GH-12838), but we need to save the pinned list
            if (E.name == "__meta__") {
                if (!pinned_props.empty()) {
                    Dictionary meta_override;
                    meta_override["_edit_pinned_properties_"] = pinned_props;
                    forced_value = meta_override;
                }
                }
            }

        StringName name = E.name;
        Variant value = forced_value.get_type() == VariantType::NIL ? p_node->get(name) : forced_value;

        if (!pinned_props.contains(name) && forced_value.get_type() == VariantType::NIL) {
            bool is_valid_default = false;
            Variant default_value =
                    PropertyUtils::get_property_default_value(p_node, name, &is_valid_default, &states_stack, true);
            if (is_valid_default && !PropertyUtils::is_property_value_different(value, default_value)) {
                continue;
            }
        }

        NodeData::Property prop;
        prop.name = _nm_get_string(name, name_map);
        prop.value = _vm_get_variant(value, variant_map);
        nd.properties.push_back(prop);
    }

    // save the groups this node is into
    // discard groups that come from the original scene

    Vector<Node::GroupInfo> groups;
    p_node->get_groups(&groups);
    for (Node::GroupInfo &gi : groups) {

        if (!gi.persistent)
            continue;
        /*
        if (instance_state_node>=0 && instance_state->is_node_in_group(instance_state_node,gi.name))
            continue; //group was instanced, don't add here
        */

        bool skip = false;
        for (const PackState& ps : states_stack) {
            //check all levels of pack to see if the group was added somewhere
            if (ps.state->is_node_in_group(ps.node, gi.name)) {
                skip = true;
                break;
            }
        }

        if (skip)
            continue;

        nd.groups.push_back(_nm_get_string(gi.name, name_map));
    }

    // save the right owner
    // for the saved scene root this is -1
    // for nodes of the saved scene this is 0
    // for nodes of instanced scenes this is >0

    nd.owner = -1; //saved scene root or p_node->get_owner() != p_owner
    if (p_node != p_owner && p_node->get_owner() == p_owner) {
        //part of saved scene
        nd.owner = 0;
    }
    // Save the right type. If this node was created by an instance
    // then flag that the node should not be created but reused
    if (states_stack.empty() && !is_editable_instance) {
        //this node is not part of an instancing process, so save the type
        nd.type = _nm_get_string(p_node->get_class(), name_map);
    } else {
        // this node is part of an instanced process, so do not save the type.
        // instead, save that it was instanced
        nd.type = TYPE_INSTANCED;
    }

    // determine whether to save this node or not
    // if this node is part of an instanced sub-scene, we can skip storing it if basically
    // no properties changed and no groups were added to it.
    // below condition is true for all nodes of the scene being saved, and ones in subscenes
    // that hold changes

    bool save_node = !nd.properties.empty() || !nd.groups.empty(); // some local properties or groups exist
    save_node = save_node || p_node == p_owner; // owner is always saved
    save_node = save_node || (p_node->get_owner() == p_owner && instanced_by_owner); //part of scene and not instanced

    int idx = nodes.size();
    int parent_node = NO_PARENT_SAVED;

    if (save_node) {

        //don't save the node if nothing and subscene

        node_map[p_node] = idx;

        //ok validate parent node
        if (p_parent_idx == NO_PARENT_SAVED) {

            int sidx;
            if (nodepath_map.contains(p_node->get_parent())) {
                sidx = nodepath_map[p_node->get_parent()];
            } else {
                sidx = nodepath_map.size();
                nodepath_map[p_node->get_parent()] = sidx;
            }

            nd.parent = FLAG_ID_IS_PATH | sidx;
        } else {
            nd.parent = p_parent_idx;
        }

        parent_node = idx;
        nodes.push_back(nd);
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {

        Node *c = p_node->get_child(i);
        Error err = _parse_node(p_owner, c, parent_node, name_map, variant_map, node_map, nodepath_map);
        if (err)
            return err;
    }

    return OK;
}

Error SceneState::_parse_connections(Node *p_owner, Node *p_node, Map<StringName, int> &name_map, HashMap<Variant, int, Hasher<Variant>, VariantComparator> &variant_map, HashMap<Node *, int> &node_map, HashMap<Node *, int> &nodepath_map) {

    if (p_node != p_owner && p_node->get_owner() && p_node->get_owner() != p_owner && !p_owner->is_editable_instance(p_node->get_owner()))
        return OK;

    Vector<MethodInfo> _signals;
    p_node->get_signal_list(&_signals);
    eastl::sort(_signals.begin(), _signals.end());


    //ERR_FAIL_COND_V( !node_map.has(p_node), ERR_BUG);
    //NodeData &nd = nodes[node_map[p_node]];

    for (const MethodInfo &E : _signals) {

        Vector<Node::Connection> conns;
        p_node->get_signal_connection_list(E.name, &conns);

        eastl::sort(conns.begin(),conns.end());

        for (const Node::Connection &c : conns) {

            if (!(c.flags & ObjectNS::CONNECT_PERSIST)) //only persistent connections get saved
                continue;

            // only connections that originate or end into main saved scene are saved
            // everything else is discarded

            Node *target = object_cast<Node>(c.callable.get_object());

            if (!target) {
                continue;
            }

            //find if this connection already exists
            Node *common_parent = target->find_common_parent_with(p_node);

            ERR_CONTINUE(!common_parent);

            if (common_parent != p_owner && common_parent->get_filename().empty()) {
                common_parent = common_parent->get_owner();
            }

            bool exists = false;

            //go through ownership chain to see if this exists
            while (common_parent) {

                Ref<SceneState> ps;

                if (common_parent == p_owner)
                    ps = common_parent->get_scene_inherited_state();
                else
                    ps = common_parent->get_scene_instance_state();

                if (ps) {

                    NodePath signal_from = common_parent->get_path_to(p_node);
                    NodePath signal_to = common_parent->get_path_to(target);

                    if (ps->has_connection(signal_from, c.signal.get_name(), signal_to, c.callable.get_method())) {
                        exists = true;
                        break;
                    }
                }

                if (common_parent == p_owner)
                    break;
                else
                    common_parent = common_parent->get_owner();
            }

            if (exists) { //already exists (comes from instance or inheritance), so don't save
                continue;
            }

            {
                Node *nl = p_node;

                bool exists2 = false;

                while (nl) {

                    if (nl == p_owner) {

                        Ref<SceneState> state = nl->get_scene_inherited_state();
                        if (state) {
                            int from_node = state->find_node_by_path(nl->get_path_to(p_node));
                            int to_node = state->find_node_by_path(nl->get_path_to(target));

                            if (from_node >= 0 && to_node >= 0) {
                                //this one has state for this node, save
                                if (state->is_connection(from_node, c.signal.get_name(), to_node, c.callable.get_method())) {
                                    exists2 = true;
                                    break;
                                }
                            }
                        }

                        nl = nullptr;
                    } else {
                        if (!nl->get_filename().empty()) {
                            //is an instance
                            Ref<SceneState> state = nl->get_scene_instance_state();
                            if (state) {
                                int from_node = state->find_node_by_path(nl->get_path_to(p_node));
                                int to_node = state->find_node_by_path(nl->get_path_to(target));

                                if (from_node >= 0 && to_node >= 0) {
                                    //this one has state for this node, save
                                    if (state->is_connection(from_node, c.signal.get_name(), to_node, c.callable.get_method())) {
                                        exists2 = true;
                                        break;
                                    }
                                }
                            }
                        }
                        nl = nl->get_owner();
                    }
                }

                if (exists2) {
                    continue;
                }
            }

            int src_id;

            if (node_map.contains(p_node)) {
                src_id = node_map[p_node];
            } else {
                if (nodepath_map.contains(p_node)) {
                    src_id = FLAG_ID_IS_PATH | nodepath_map[p_node];
                } else {
                    int sidx = nodepath_map.size();
                    nodepath_map[p_node] = sidx;
                    src_id = FLAG_ID_IS_PATH | sidx;
                }
            }

            int target_id;

            if (node_map.contains(target)) {
                target_id = node_map[target];
            } else {
                if (nodepath_map.contains(target)) {
                    target_id = FLAG_ID_IS_PATH | nodepath_map[target];
                } else {
                    int sidx = nodepath_map.size();
                    nodepath_map[target] = sidx;
                    target_id = FLAG_ID_IS_PATH | sidx;
                }
            }

            ConnectionData cd;
            cd.from = src_id;
            cd.to = target_id;
            cd.method = _nm_get_string(c.callable.get_method(), name_map);
            cd.signal = _nm_get_string(c.signal.get_name(), name_map);
            cd.flags = c.flags;
            connections.push_back(cd);
        }
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {

        Node *c = p_node->get_child(i);
        Error err = _parse_connections(p_owner, c, name_map, variant_map, node_map, nodepath_map);
        if (err) {
            return err;
        }
    }

    return OK;
}

Error SceneState::pack(Node *p_scene) {
    ERR_FAIL_NULL_V(p_scene, ERR_INVALID_PARAMETER);

    clear();

    Node *scene = p_scene;

    Map<StringName, int> name_map;
    HashMap<Variant, int, Hasher<Variant>, VariantComparator> variant_map;
    HashMap<Node *, int> node_map;
    HashMap<Node *, int> nodepath_map;

    // If using scene inheritance, pack the scene it inherits from.
    if (scene->get_scene_inherited_state()) {
        String path = scene->get_scene_inherited_state()->get_path();
        Ref<PackedScene> instance = dynamic_ref_cast<PackedScene>(gResourceManager().load(path));
        if (instance) {

            base_scene_idx = _vm_get_variant(instance, variant_map);
        }
    }

    // Instanced, only direct sub-scenes are supported of course.
    Error err = _parse_node(scene, scene, -1, name_map, variant_map, node_map, nodepath_map);
    if (err) {
        clear();
        ERR_FAIL_V(err);
    }

    err = _parse_connections(scene, scene, name_map, variant_map, node_map, nodepath_map);
    if (err) {
        clear();
        ERR_FAIL_V(err);
    }

    names.resize(name_map.size());

    for (eastl::pair<const StringName,int> &E : name_map) {

        names[E.second] = E.first;
    }

    variants.resize(variant_map.size());
    for(auto & e : variant_map) {
        variants[e.second] = e.first;
    }

    node_paths.resize(nodepath_map.size());
    for (const eastl::pair<Node *const,int> &E : nodepath_map) {

        node_paths[E.second] = scene->get_path_to(E.first);
    }

    if (Engine::get_singleton()->is_editor_hint()) {
        // Build node path cache
        for (const auto &E : node_map) {
            node_path_cache[scene->get_path_to(E.first)] = E.second;
        }
    }
    return OK;
}

void SceneState::set_path(StringView p_path) {

    path = p_path;
}

const String &SceneState::get_path() const {

    return path;
}

void SceneState::clear() {

    names.clear();
    variants.clear();
    nodes.clear();
    connections.clear();
    node_path_cache.clear();
    node_paths.clear();
    editable_instances.clear();
    base_scene_idx = -1;
}

Ref<SceneState> SceneState::get_base_scene_state() const {

    if (base_scene_idx >= 0) {

        Ref<PackedScene> ps(variants[base_scene_idx]);
        if (ps) {
            return ps->get_state();
        }
    }

    return Ref<SceneState>();
}

int SceneState::find_node_by_path(const NodePath &p_node) const {
    ERR_FAIL_COND_V_MSG(node_path_cache.empty(), -1, "This operation requires the node cache to have been built.");

    if (!node_path_cache.contains(p_node)) {
        if (get_base_scene_state()) {
            int idx = get_base_scene_state()->find_node_by_path(p_node);
            if (idx != -1) {
                int rkey = _find_base_scene_node_remap_key(idx);
                if (rkey == -1) {
                    rkey = nodes.size() + base_scene_node_remap.size();
                    base_scene_node_remap[rkey] = idx;
                }
                return rkey;
            }
        }
        return -1;
    }

    int nid = node_path_cache[p_node];

    if (get_base_scene_state() && !base_scene_node_remap.contains(nid)) {
        //for nodes that _do_ exist in current scene, still try to look for
        //the node in the instanced scene, as a property may be missing
        //from the local one
        int idx = get_base_scene_state()->find_node_by_path(p_node);
        if (idx != -1) {
            base_scene_node_remap[nid] = idx;
        }
    }

    return nid;
}

int SceneState::_find_base_scene_node_remap_key(int p_idx) const {

    for (eastl::pair<const int,int> &E : base_scene_node_remap) {
        if (E.second == p_idx) {
            return E.first;
        }
    }
    return -1;
}

Variant SceneState::get_property_value(int p_node, const StringName &p_property, bool &found) const {

    found = false;

    ERR_FAIL_COND_V(p_node < 0, Variant());

    if (p_node < nodes.size()) {
        //find in built-in nodes
        const Vector<NodeData::Property> &props(nodes[p_node].properties);
        const StringName *namep = names.data();

        for (const auto &prop : props) {
            if (p_property == namep[prop.name]) {
                found = true;
                return variants[prop.value];
            }
        }
    }

    //property not found, try on instance

    if (base_scene_node_remap.contains(p_node)) {
        return get_base_scene_state()->get_property_value(base_scene_node_remap[p_node], p_property, found);
    }

    return Variant();
}

bool SceneState::is_node_in_group(int p_node, const StringName &p_group) const {

    ERR_FAIL_COND_V(p_node < 0, false);

    if (p_node < nodes.size()) {
        const StringName *namep = names.data();
        const Vector<int> &groups(nodes[p_node].groups);
        for (int grp : groups) {
            if (namep[grp] == p_group)
                return true;
        }
    }

    if (base_scene_node_remap.contains(p_node)) {
        return get_base_scene_state()->is_node_in_group(base_scene_node_remap[p_node], p_group);
    }

    return false;
}

bool SceneState::disable_placeholders = false;

void SceneState::set_disable_placeholders(bool p_disable) {

    disable_placeholders = p_disable;
}

bool SceneState::is_connection(int p_node, const StringName &p_signal, int p_to_node, const StringName &p_to_method) const {

    ERR_FAIL_COND_V(p_node < 0, false);
    ERR_FAIL_COND_V(p_to_node < 0, false);

    if (p_node < nodes.size() && p_to_node < nodes.size()) {

        int signal_idx = -1;
        int method_idx = -1;
        for (int i = 0; i < names.size(); i++) {
            if (names[i] == p_signal) {
                signal_idx = i;
            } else if (names[i] == p_to_method) {
                method_idx = i;
            }
        }

        if (signal_idx >= 0 && method_idx >= 0) {
            //signal and method strings are stored..

            for (const ConnectionData &connection : connections) {

                if (connection.from == p_node && connection.to == p_to_node && connection.signal == signal_idx &&
                    connection.method == method_idx) {

                    return true;
                }
            }
        }
    }

    if (base_scene_node_remap.contains(p_node) && base_scene_node_remap.contains(p_to_node)) {
        return get_base_scene_state()->is_connection(base_scene_node_remap[p_node], p_signal, base_scene_node_remap[p_to_node], p_to_method);
    }

    return false;
}

void SceneState::set_bundled_scene(const Dictionary &p_dictionary) {

    ERR_FAIL_COND(!p_dictionary.has("names"));
    ERR_FAIL_COND(!p_dictionary.has("variants"));
    ERR_FAIL_COND(!p_dictionary.has("node_count"));
    ERR_FAIL_COND(!p_dictionary.has("nodes"));
    ERR_FAIL_COND(!p_dictionary.has("conn_count"));
    ERR_FAIL_COND(!p_dictionary.has("conns"));
    //ERR_FAIL_COND();

    int version = 1;
    if (p_dictionary.has("version"))
        version = p_dictionary["version"].as<int>();

    ERR_FAIL_COND_MSG(version > PACKED_SCENE_VERSION, "Save format version too new.");

    const int node_count = p_dictionary["node_count"].as<int>();
    const PoolVector<int> snodes = p_dictionary["nodes"].as<PoolVector<int>>();
    ERR_FAIL_COND(snodes.size() < node_count);

    const int conn_count = p_dictionary["conn_count"].as<int>();
    const PoolVector<int> sconns = p_dictionary["conns"].as<PoolVector<int>>();
    ERR_FAIL_COND(sconns.size() < conn_count);

    PoolVector<String> snames = p_dictionary["names"].as<PoolVector<String>>();
    if (!snames.empty()) {

        int namecount = snames.size();
        names.resize(namecount);
        PoolVector<String>::Read r = snames.read();
        for (size_t i = 0; i < names.size(); i++)
            names[i] = StringName(r[i]);
    }

    Array svariants = p_dictionary["variants"].as<Array>();

    if (!svariants.empty()) {
        int varcount = svariants.size();
        variants.resize(varcount);
        for (int i = 0; i < varcount; i++) {

            variants[i] = svariants[i];
        }

    } else {
        variants.clear();
    }

    nodes.resize(node_count);
    if (node_count) {
        PoolVector<int> snodes = p_dictionary["nodes"].as<PoolVector<int>>();
        PoolVector<int>::Read r = snodes.read();
        int idx = 0;
        for (NodeData &nd : nodes) {
            nd.parent = r[idx++];
            nd.owner = r[idx++];
            nd.type = r[idx++];
            uint32_t name_index = r[idx++];
            nd.name = name_index & (1 << NAME_INDEX_BITS) - 1;
            nd.index = name_index >> NAME_INDEX_BITS;
            nd.index--; //0 is invalid, stored as 1
            nd.instance = r[idx++];
            nd.properties.resize(r[idx++]);
            for (auto &propertie : nd.properties) {

                propertie.name = r[idx++];
                propertie.value = r[idx++];
            }
            nd.groups.resize(r[idx++]);
            for (int &group : nd.groups) {

                group = r[idx++];
            }
        }
    }

    connections.resize(conn_count);
    if (conn_count) {

        PoolVector<int> sconns = p_dictionary["conns"].as<PoolVector<int>>();
        PoolVector<int>::Read r = sconns.read();
        int idx = 0;
        for (ConnectionData& cd : connections) {
            cd.from = r[idx++];
            cd.to = r[idx++];
            cd.signal = r[idx++];
            cd.method = r[idx++];
            cd.flags = r[idx++];
        }
    }

    Array np;
    if (p_dictionary.has("node_paths")) {
        np = p_dictionary["node_paths"].as<Array>();
    }
    node_paths.resize(np.size());
    for (int i = 0; i < np.size(); i++) {
        node_paths[i] = np[i].as<NodePath>();
    }

    Array ei;
    if (p_dictionary.has("editable_instances")) {
        ei = p_dictionary["editable_instances"].as<Array>();
    }

    if (p_dictionary.has("base_scene")) {
        base_scene_idx = p_dictionary["base_scene"].as<int>();
    }

    editable_instances.resize(ei.size());
    for (int i = 0; i < editable_instances.size(); i++) {
        editable_instances[i] = ei[i].as<NodePath>();
    }

    //path=p_dictionary["path"];
}

Dictionary SceneState::get_bundled_scene() const {

    PoolVector<String> rnames;
    rnames.resize(names.size());

    if (!names.empty()) {

        PoolVector<String>::Write r = rnames.write();

        for (size_t i = 0; i < names.size(); i++)
            r[i] = names[i];
    }

    Dictionary d;
    d["names"] = rnames;
    d["variants"] = Variant::from(variants);

    Vector<int> rnodes;
    d["node_count"] = nodes.size();
    rnodes.reserve(nodes.size()*15);
    for (const NodeData& nd : nodes) {

        rnodes.emplace_back(nd.parent);
        rnodes.emplace_back(nd.owner);
        rnodes.emplace_back(nd.type);
        uint32_t name_index = nd.name;
        if (nd.index < (1 << (32 - NAME_INDEX_BITS)) - 1) { //save if less than 16k children
            name_index |= uint32_t(nd.index + 1) << NAME_INDEX_BITS; //for backwards compatibility, index 0 is no index
        }
        rnodes.emplace_back(name_index);
        rnodes.emplace_back(nd.instance);
        rnodes.emplace_back(nd.properties.size());
        for (auto propertie : nd.properties) {

            rnodes.emplace_back(propertie.name);
            rnodes.emplace_back(propertie.value);
        }
        rnodes.emplace_back(nd.groups.size());
        rnodes.insert(rnodes.end(), nd.groups.begin(), nd.groups.end());
    }

    d["nodes"] = rnodes;

    Vector<int> rconns;
    d["conn_count"] = connections.size();
    rconns.reserve(connections.size()*12);
    for (const ConnectionData &cd : connections) {

        rconns.push_back(cd.from);
        rconns.push_back(cd.to);
        rconns.push_back(cd.signal);
        rconns.push_back(cd.method);
        rconns.push_back(cd.flags);
    }

    d["conns"] = rconns;

    Array rnode_paths;
    rnode_paths.resize(node_paths.size());
    for (const NodePath &np :  node_paths) {
        rnode_paths.emplace_back(np);
    }
    d["node_paths"] = rnode_paths;

    Array reditable_instances;
    reditable_instances.resize(editable_instances.size());
    for (int i = 0; i < editable_instances.size(); i++) {
        reditable_instances[i] = editable_instances[i];
    }
    d["editable_instances"] = reditable_instances;
    if (base_scene_idx >= 0) {
        d["base_scene"] = base_scene_idx;
    }

    d["version"] = PACKED_SCENE_VERSION;

    return d;
}

int SceneState::get_node_count() const {

    return nodes.size();
}

StringName SceneState::get_node_type(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, nodes.size(), StringName());
    if (nodes[p_idx].type == TYPE_INSTANCED)
        return StringName();
    return names[nodes[p_idx].type];
}

StringName SceneState::get_node_name(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, nodes.size(), StringName());
    return names[nodes[p_idx].name];
}

int SceneState::get_node_index(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, nodes.size(), -1);
    return nodes[p_idx].index;
}

bool SceneState::is_node_instance_placeholder(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, nodes.size(), false);

    return nodes[p_idx].instance >= 0 && nodes[p_idx].instance & FLAG_INSTANCE_IS_PLACEHOLDER;
}

Ref<PackedScene> SceneState::get_node_instance(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, nodes.size(), Ref<PackedScene>());

    if (nodes[p_idx].instance >= 0) {
        if (nodes[p_idx].instance & FLAG_INSTANCE_IS_PLACEHOLDER)
            return Ref<PackedScene>();

        return refFromVariant<PackedScene>(variants[nodes[p_idx].instance & FLAG_MASK]);
    }
    if (nodes[p_idx].parent < 0 || nodes[p_idx].parent == NO_PARENT_SAVED) {

        if (base_scene_idx >= 0) {
            return refFromVariant<PackedScene>(variants[base_scene_idx]);
        }
    }

    return Ref<PackedScene>();
}

String SceneState::get_node_instance_placeholder(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, nodes.size(), String());

    if (nodes[p_idx].instance >= 0 && nodes[p_idx].instance & FLAG_INSTANCE_IS_PLACEHOLDER) {
        return variants[nodes[p_idx].instance & FLAG_MASK].as<String>();
    }

    return String();
}

Vector<StringName> SceneState::get_node_groups(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, nodes.size(), {});
    Vector<StringName> groups;
    groups.reserve(nodes[p_idx].groups.size());

    for (int group : nodes[p_idx].groups) {
        groups.emplace_back(names[group]);
    }
    return groups;
}

NodePath SceneState::get_node_path(int p_idx, bool p_for_parent) const {

    ERR_FAIL_INDEX_V(p_idx, nodes.size(), NodePath());

    if (nodes[p_idx].parent < 0 || nodes[p_idx].parent == NO_PARENT_SAVED) {
        if (p_for_parent) {
            return NodePath();
        }
        return NodePath(".");
    }

    Vector<StringName> sub_path;
    NodePath base_path;
    int nidx = p_idx;
    while (true) {
        if (nodes[nidx].parent == NO_PARENT_SAVED || nodes[nidx].parent < 0) {

            sub_path.push_front(".");
            break;
        }

        if (!p_for_parent || p_idx != nidx) {
            sub_path.push_front(names[nodes[nidx].name]);
        }

        if (nodes[nidx].parent & FLAG_ID_IS_PATH) {
            base_path = node_paths[nodes[nidx].parent & FLAG_MASK];
            break;
        } else {
            nidx = nodes[nidx].parent & FLAG_MASK;
        }
    }

    for (int i = base_path.get_name_count() - 1; i >= 0; i--) {
        sub_path.push_front(base_path.get_name(i));
    }

    if (sub_path.empty()) {
        return NodePath(".");
    }

    return NodePath(sub_path, false);
}

int SceneState::get_node_property_count(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, nodes.size(), -1);
    return nodes[p_idx].properties.size();
}
StringName SceneState::get_node_property_name(int p_idx, int p_prop) const {
    ERR_FAIL_INDEX_V(p_idx, nodes.size(), StringName());
    ERR_FAIL_INDEX_V(p_prop, nodes[p_idx].properties.size(), StringName());
    return names[nodes[p_idx].properties[p_prop].name];
}
Variant SceneState::get_node_property_value(int p_idx, int p_prop) const {
    ERR_FAIL_INDEX_V(p_idx, nodes.size(), Variant());
    ERR_FAIL_INDEX_V(p_prop, nodes[p_idx].properties.size(), Variant());

    return variants[nodes[p_idx].properties[p_prop].value];
}

NodePath SceneState::get_node_owner_path(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, nodes.size(), NodePath());
    if (nodes[p_idx].owner < 0 || nodes[p_idx].owner == NO_PARENT_SAVED)
        return NodePath(); //root likely
    if (nodes[p_idx].owner & FLAG_ID_IS_PATH) {
        return node_paths[nodes[p_idx].owner & FLAG_MASK];
    }
    return get_node_path(nodes[p_idx].owner & FLAG_MASK);
}

int SceneState::get_connection_count() const {

    return connections.size();
}
NodePath SceneState::get_connection_source(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, connections.size(), NodePath());
    if (connections[p_idx].from & FLAG_ID_IS_PATH) {
        return node_paths[connections[p_idx].from & FLAG_MASK];
    } else {
        return get_node_path(connections[p_idx].from & FLAG_MASK);
    }
}

StringName SceneState::get_connection_signal(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, connections.size(), StringName());
    return names[connections[p_idx].signal];
}
NodePath SceneState::get_connection_target(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, connections.size(), NodePath());
    if (connections[p_idx].to & FLAG_ID_IS_PATH) {
        return node_paths[connections[p_idx].to & FLAG_MASK];
    } else {
        return get_node_path(connections[p_idx].to & FLAG_MASK);
    }
}
StringName SceneState::get_connection_method(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, connections.size(), StringName());
    return names[connections[p_idx].method];
}

int SceneState::get_connection_flags(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, connections.size(), -1);
    return connections[p_idx].flags;
}


bool SceneState::has_connection(const NodePath &p_node_from, const StringName &p_signal, const NodePath &p_node_to, const StringName &p_method) {

    // this method cannot be const because of this
    Ref<SceneState> ss(this);

    do {
        for (const ConnectionData& c : ss->connections) {

            NodePath np_from;

            if (c.from & FLAG_ID_IS_PATH) {
                np_from = ss->node_paths[c.from & FLAG_MASK];
            } else {
                np_from = ss->get_node_path(c.from);
            }

            NodePath np_to;

            if (c.to & FLAG_ID_IS_PATH) {
                np_to = ss->node_paths[c.to & FLAG_MASK];
            } else {
                np_to = ss->get_node_path(c.to);
            }

            StringName sn_signal = ss->names[c.signal];
            StringName sn_method = ss->names[c.method];

            if (np_from == p_node_from && sn_signal == p_signal && np_to == p_node_to && sn_method == p_method) {
                return true;
            }
        }

        ss = ss->get_base_scene_state();
    } while (ss);

    return false;
}

const Vector<NodePath> &SceneState::get_editable_instances() const {
    return editable_instances;
}
//add

int SceneState::add_name(const StringName &p_name) {
    int idx = names.size();
    names.push_back(p_name);
    return idx;
}

int SceneState::find_name(const StringName &p_name) const {

    for (size_t i = 0; i < names.size(); i++) {
        if (names[i] == p_name)
            return i;
    }

    return -1;
}

int SceneState::add_value(const Variant &p_value) {

    variants.push_back(p_value);
    return variants.size() - 1;
}

int SceneState::add_node_path(const NodePath &p_path) {

    node_paths.push_back(p_path);
    return (node_paths.size() - 1) | FLAG_ID_IS_PATH;
}
int SceneState::add_node(int p_parent, int p_owner, int p_type, int p_name, int p_instance, int p_index) {

    NodeData nd;
    nd.parent = p_parent;
    nd.owner = p_owner;
    nd.type = p_type;
    nd.name = p_name;
    nd.instance = p_instance;
    nd.index = p_index;

    nodes.push_back(nd);

    return nodes.size() - 1;
}
void SceneState::add_node_property(int p_node, int p_name, int p_value) {

    ERR_FAIL_INDEX(p_node, nodes.size());
    ERR_FAIL_INDEX(p_name, names.size());
    ERR_FAIL_INDEX(p_value, variants.size());

    NodeData::Property prop { p_name,p_value };
    nodes[p_node].properties.emplace_back(prop);
}
void SceneState::add_node_group(int p_node, int p_group) {

    ERR_FAIL_INDEX(p_node, nodes.size());
    ERR_FAIL_INDEX(p_group, names.size());
    nodes[p_node].groups.push_back(p_group);
}
void SceneState::set_base_scene(int p_idx) {

    ERR_FAIL_INDEX(p_idx, variants.size());
    base_scene_idx = p_idx;
}
void SceneState::add_connection(int p_from, int p_to, int p_signal, int p_method, int p_flags) {

    ERR_FAIL_INDEX(p_signal, names.size());
    ERR_FAIL_INDEX(p_method, names.size());

    ConnectionData c;
    c.from = p_from;
    c.to = p_to;
    c.signal = p_signal;
    c.method = p_method;
    c.flags = p_flags;
    connections.emplace_back(c);
}
void SceneState::add_editable_instance(const NodePath &p_path) {

    editable_instances.emplace_back(p_path);
}

PoolVector<String> SceneState::_get_node_groups(int p_idx) const {

    Vector<StringName> groups = get_node_groups(p_idx);
    PoolVector<String> ret;

    for (const auto &group : groups)
        ret.push_back(group.asCString());

    return ret;
}

void SceneState::_bind_methods() {

    //unbuild API

    SE_BIND_METHOD(SceneState,get_node_count);
    SE_BIND_METHOD(SceneState,get_node_type);
    SE_BIND_METHOD(SceneState,get_node_name);
    MethodBinder::bind_method(D_METHOD("get_node_path", {"idx", "for_parent"}), &SceneState::get_node_path, {DEFVAL(false)});
    SE_BIND_METHOD(SceneState,get_node_owner_path);
    SE_BIND_METHOD(SceneState,is_node_instance_placeholder);
    SE_BIND_METHOD(SceneState,get_node_instance_placeholder);
    SE_BIND_METHOD(SceneState,get_node_instance);
    MethodBinder::bind_method(D_METHOD("get_node_groups", {"idx"}), &SceneState::_get_node_groups);
    SE_BIND_METHOD(SceneState,get_node_index);
    SE_BIND_METHOD(SceneState,get_node_property_count);
    SE_BIND_METHOD(SceneState,get_node_property_name);
    SE_BIND_METHOD(SceneState,get_node_property_value);
    SE_BIND_METHOD(SceneState,get_connection_count);
    SE_BIND_METHOD(SceneState,get_connection_source);
    SE_BIND_METHOD(SceneState,get_connection_signal);
    SE_BIND_METHOD(SceneState,get_connection_target);
    SE_BIND_METHOD(SceneState,get_connection_method);
    SE_BIND_METHOD(SceneState,get_connection_flags);

}

SceneState::SceneState() {

}

////////////////

void PackedScene::_set_bundled_scene(const Dictionary &p_scene) {

    state->set_bundled_scene(p_scene);
}

Dictionary PackedScene::_get_bundled_scene() const {

    return state->get_bundled_scene();
}

Error PackedScene::pack(Node *p_scene) {

    return state->pack(p_scene);
}

void PackedScene::clear() {

    state->clear();
}

bool PackedScene::can_instance() const {

    return state->can_instance();
}

Node *PackedScene::instance(PackedGenEditState p_edit_state) const {

    if(!PackedSceneTooling::can_instance_state(p_edit_state)) {
        return nullptr;
    }

    Node *s = state->instance(p_edit_state);
    if (!s) {
        return nullptr;
    }

    if (p_edit_state != GEN_EDIT_STATE_DISABLED) {
        s->set_scene_instance_state(state);
    }

    if (!get_path().empty() && !StringUtils::contains(get_path(),"::"))
        s->set_filename(get_path());

    s->notification(Node::NOTIFICATION_INSTANCED);

    return s;
}

void PackedScene::replace_state(Ref<SceneState> p_by) {

    state = eastl::move(p_by);
    state->set_path(get_path());
    on_state_changed();
}

void PackedScene::recreate_state() {

    state = make_ref_counted<SceneState>();
    state->set_path(get_path());
    on_state_changed();
}

void PackedScene::set_path(StringView p_path, bool p_take_over) {

    state->set_path(p_path);
    Resource::set_path(p_path, p_take_over);
}

void PackedScene::_bind_methods() {

    SE_BIND_METHOD(PackedScene,pack);
    MethodBinder::bind_method(D_METHOD("instance", {"edit_state"}), &PackedScene::instance, {DEFVAL(GEN_EDIT_STATE_DISABLED)});
    SE_BIND_METHOD(PackedScene,can_instance);
    SE_BIND_METHOD(PackedScene,_set_bundled_scene);
    SE_BIND_METHOD(PackedScene,_get_bundled_scene);
    SE_BIND_METHOD(PackedScene,get_state);

    ADD_PROPERTY(PropertyInfo(VariantType::DICTIONARY, "_bundled"), "_set_bundled_scene", "_get_bundled_scene");

    BIND_GLOBAL_ENUM_CONSTANT(GEN_EDIT_STATE_DISABLED);
    BIND_GLOBAL_ENUM_CONSTANT(GEN_EDIT_STATE_INSTANCE);
    BIND_GLOBAL_ENUM_CONSTANT(GEN_EDIT_STATE_MAIN);
    BIND_GLOBAL_ENUM_CONSTANT(GEN_EDIT_STATE_MAIN_INHERITED);
}

PackedScene::PackedScene() {

    state = make_ref_counted<SceneState>();
}
