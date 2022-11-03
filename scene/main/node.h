/*************************************************************************/
/*  node.h                                                               */
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
#include "core/forward_decls.h"
#include "core/object.h"
#include "core/os/main_loop.h"

#include "core/hash_map.h"

class Viewport;
class SceneState;
class MultiplayerAPI;
class SceneTree;
class Resource;
struct SceneTreeGroup;
class wrap_allocator;
class NodePath;


enum MultiplayerAPI_RPCMode : int8_t;

// added to all entities (GameEntity) that need xform update.
struct DirtXFormMarker {

};
struct InTreeMarkerComponent {

};
struct GameRenderableComponent {
    RenderingEntity render_side;
    GameEntity game_side;
};
extern void mark_dirty_xform(GameEntity);
extern void mark_clean_xform(GameEntity);
extern bool is_dirty_xfrom(GameEntity);

class GODOT_EXPORT Node : public Object {

    GDCLASS(Node,Object)
//    Q_GADGET
//    Q_CLASSINFO("Category","Nodes")
    OBJ_CATEGORY("Nodes")

public:
    //jl::Signal<> tree_entered;
    enum PauseMode : int8_t {

        PAUSE_MODE_INHERIT,
        PAUSE_MODE_STOP,
        PAUSE_MODE_PROCESS
    };

    enum DuplicateFlags : int8_t {

        DUPLICATE_SIGNALS = 1,
        DUPLICATE_GROUPS = 2,
        DUPLICATE_SCRIPTS = 4,
        DUPLICATE_USE_INSTANCING = 8,
#ifdef TOOLS_ENABLED
        DUPLICATE_FROM_EDITOR = 16,
#endif
    };

    enum NameCasing : int8_t {
        NAME_CASING_PASCAL_CASE,
        NAME_CASING_CAMEL_CASE,
        NAME_CASING_SNAKE_CASE
    };
    struct GroupInfo {

        StringName name;
        bool persistent;
    };

    struct Comparator {
        bool operator()(const Node *p_a, const Node *p_b) const { return p_b->is_greater_than(p_a); }
    };

    struct ComparatorWithPriority {

        bool operator()(const Node *p_a, const Node *p_b) const {
            return p_b->process_priority == p_a->process_priority ? p_b->is_greater_than(p_a) :
                                                                    p_b->process_priority > p_a->process_priority;
        }
    };

    static int orphan_node_count;

private:
    struct PrivData;

    SceneTree *tree;
    Viewport *viewport;
    PrivData *priv_data;
    Ref<MultiplayerAPI> multiplayer;

    int blocked; // safeguard that throws an error when attempting to modify the tree in a harmful way while being traversed.
    int process_priority;
    bool inside_tree;
    bool parent_owned;

    static const char *invalid_character;

public:
    void _print_tree_pretty(const UIString &prefix, const bool last);
    void _print_tree(const Node *p_node);

    Node *_get_child_by_name(const StringName &p_name) const;

    void _replace_connections_target(Node *p_new_target);

    void _validate_child_name(Node *p_child, bool p_force_human_readable = false);
    void _generate_serial_child_name(const Node *p_child, StringName &name) const;

    void _propagate_reverse_notification(int p_notification);
    void _propagate_deferred_notification(int p_notification, bool p_reverse);
    void _propagate_enter_tree();
    void _propagate_ready();
    void _propagate_exit_tree();
    void _propagate_after_exit_branch(bool);

    void _print_stray_nodes();
    void _propagate_pause_owner(Node *p_owner);
    Array _get_node_and_resource(const NodePath &p_path);

    void _duplicate_signals(const Node *p_original, Node *p_copy) const;
    void _duplicate_and_reown(Node *p_new_parent, const HashMap<Node *, Node *> &p_reown_map) const;
    Node *_duplicate(int p_flags, HashMap<const Node *, Node *> *r_duplimap = nullptr) const;
    Array _get_children() const;
    Array _get_groups() const;
    Variant _rpc_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
    Variant _rpc_unreliable_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
    Variant _rpc_id_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
    Variant _rpc_unreliable_id_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
private:
    friend class SceneTree;

    void _set_tree(SceneTree *p_tree);

#ifdef TOOLS_ENABLED
    friend class SceneTreeEditor;
public: // _validate_node_name is public in editor builds
#endif
    static bool _validate_node_name(String &p_name);

protected:
    void _block() { blocked++; }
    void _unblock() { blocked--; }

    void _notification(int p_notification);

    //! By default child add/move/remove notifications are no-ops
    virtual void add_child_notify(Node *p_child) { }
    virtual void remove_child_notify(Node *p_child) {}
    virtual void move_child_notify(Node *p_child) { }

    void _propagate_replace_owner(Node *p_owner, Node *p_by_owner);

    static void _bind_methods();

    friend class SceneState;

    void _add_child_nocheck(Node *p_child, const StringName &p_name);
    void _set_owner_nocheck(Node *p_owner);
    void _set_name_nocheck(const StringName &p_name);

    void _set_use_identity_transform(bool p_enable);
    bool _is_using_identity_transform() const;
public:
    enum NodeNotification {

        // you can make your own, but don't use the same numbers as other notifications in other nodes
        NOTIFICATION_ENTER_TREE = 10,
        NOTIFICATION_EXIT_TREE = 11,
        NOTIFICATION_MOVED_IN_PARENT = 12,
        NOTIFICATION_READY = 13,
        NOTIFICATION_PAUSED = 14,
        NOTIFICATION_UNPAUSED = 15,
        NOTIFICATION_PHYSICS_PROCESS = 16,
        NOTIFICATION_PROCESS = 17,
        NOTIFICATION_PARENTED = 18,
        NOTIFICATION_UNPARENTED = 19,
        NOTIFICATION_INSTANCED = 20,
        NOTIFICATION_DRAG_BEGIN = 21,
        NOTIFICATION_DRAG_END = 22,
        NOTIFICATION_PATH_CHANGED = 23,
        //NOTIFICATION_TRANSLATION_CHANGED = 24, moved below
        NOTIFICATION_INTERNAL_PROCESS = 25,
        NOTIFICATION_INTERNAL_PHYSICS_PROCESS = 26,
        NOTIFICATION_POST_ENTER_TREE = 27,
        NOTIFICATION_RESET_PHYSICS_INTERPOLATION = 28,
        //keep these linked to node
        NOTIFICATION_WM_MOUSE_ENTER = MainLoop::NOTIFICATION_WM_MOUSE_ENTER,
        NOTIFICATION_WM_MOUSE_EXIT = MainLoop::NOTIFICATION_WM_MOUSE_EXIT,
        NOTIFICATION_WM_FOCUS_IN = MainLoop::NOTIFICATION_WM_FOCUS_IN,
        NOTIFICATION_WM_FOCUS_OUT = MainLoop::NOTIFICATION_WM_FOCUS_OUT,
        NOTIFICATION_WM_QUIT_REQUEST = MainLoop::NOTIFICATION_WM_QUIT_REQUEST,
        NOTIFICATION_WM_GO_BACK_REQUEST = MainLoop::NOTIFICATION_WM_GO_BACK_REQUEST,
        NOTIFICATION_WM_UNFOCUS_REQUEST = MainLoop::NOTIFICATION_WM_UNFOCUS_REQUEST,
        NOTIFICATION_OS_MEMORY_WARNING = MainLoop::NOTIFICATION_OS_MEMORY_WARNING,
        NOTIFICATION_TRANSLATION_CHANGED = MainLoop::NOTIFICATION_TRANSLATION_CHANGED,
        NOTIFICATION_WM_ABOUT = MainLoop::NOTIFICATION_WM_ABOUT,
        NOTIFICATION_CRASH = MainLoop::NOTIFICATION_CRASH,
        NOTIFICATION_OS_IME_UPDATE = MainLoop::NOTIFICATION_OS_IME_UPDATE,
        NOTIFICATION_APP_RESUMED = MainLoop::NOTIFICATION_APP_RESUMED,
        NOTIFICATION_APP_PAUSED = MainLoop::NOTIFICATION_APP_PAUSED

    };

    /* NODE/TREE */

    StringName get_name() const;
    void set_name(StringView p_name);

    void add_child(Node *p_child, bool p_legible_unique_name = false);
    void add_child_below_node(Node *p_node, Node *p_child, bool p_legible_unique_name = false);
    void remove_child(Node *p_child);

    int get_child_count() const;
    Node *get_child(int p_index) const;
    const Vector<Node *> &children() const;

    bool has_node(const NodePath &p_path) const;
    Node *get_node(const NodePath &p_path) const;
    Node *get_node_or_null(const NodePath &p_path) const;
    Node *find_node(StringView p_mask, bool p_recursive = true, bool p_owned = true) const;
    bool has_node_and_resource(const NodePath &p_path) const;
    Node *get_node_and_resource(const NodePath &p_path, Ref<Resource> &r_res, Vector<StringName> &r_leftover_subpath, bool p_last_is_property = true) const;

    Node *get_parent() const;

    _FORCE_INLINE_ SceneTree *get_tree() const {
        ERR_FAIL_COND_V(!tree, nullptr);
        return tree;
    }

    bool is_inside_tree() const { return inside_tree; }

    bool is_a_parent_of(const Node *p_node) const;
    bool is_greater_than(const Node *p_node) const;

    NodePath get_path() const;
    NodePath get_path_to(const Node *p_node) const;
    Node *find_common_parent_with(const Node *p_node) const;

    void add_to_group(const StringName &p_identifier, bool p_persistent = false);
    void remove_from_group(const StringName &p_identifier);
    bool is_in_group(const StringName &p_identifier) const;

    void get_groups(Vector<GroupInfo> *p_groups) const;
    int get_persistent_group_count() const;

    void move_child(Node *p_child, int p_pos);
    void raise();

    void set_owner(Node *p_owner);
    Node *get_owner() const;
    void get_owned_by(Node *p_by, Vector<Node *> &p_owned);

    void remove_and_skip();
    int get_index() const;

    void print_tree();
    void print_tree_pretty();

    void set_filename(StringView p_filename);
    StringView get_filename() const;

    void set_editor_description(StringView p_editor_description);
    String get_editor_description() const;

    void set_editable_instance(Node *p_node, bool p_editable);
    bool is_editable_instance(const Node *p_node) const;
    Node *get_deepest_editable_node(Node *start_node) const;

#ifdef TOOLS_ENABLED
    void set_property_pinned(const StringName &p_property, bool p_pinned);
    bool is_property_pinned(const StringName &p_property) const;
    virtual StringName get_property_store_alias(const StringName &p_property) const;
#endif
    void get_storable_properties(Set<StringName> &r_storable_properties) const;

    String to_string() override;
    /* NOTIFICATIONS */

    void propagate_notification(int p_notification);

    void propagate_call(const StringName &p_method, const Array &p_args = Array(), const bool p_parent_first = false);

    /* PROCESSING */
    void set_physics_process(bool p_process);
    float get_physics_process_delta_time() const;
    bool is_physics_processing() const;

    void set_process(bool p_idle_process);
    float get_process_delta_time() const;
    bool is_processing() const;

    void set_physics_process_internal(bool p_process_internal);
    bool is_physics_processing_internal() const;

    void set_process_internal(bool p_idle_process_internal);
    bool is_processing_internal() const;

    void set_process_priority(int p_priority);
    int get_process_priority() const { return process_priority; }

    void set_process_input(bool p_enable);
    bool is_processing_input() const;

    void set_process_unhandled_input(bool p_enable);
    bool is_processing_unhandled_input() const;

    void set_process_unhandled_key_input(bool p_enable);
    bool is_processing_unhandled_key_input() const;

    int get_position_in_parent() const;

    Node *duplicate(int p_flags = DUPLICATE_GROUPS | DUPLICATE_SIGNALS | DUPLICATE_SCRIPTS) const;
    Node *duplicate_and_reown(const HashMap<Node *, Node *> &p_reown_map) const;
#ifdef TOOLS_ENABLED
    Node *duplicate_from_editor(HashMap<const Node *, Node *> &r_duplimap) const;
    Node *duplicate_from_editor(HashMap<const Node *, Node *> &r_duplimap, const HashMap<Ref<Resource>, Ref<Resource>> &p_resource_remap) const;
    void remap_node_resources(Node *p_node, const HashMap<Ref<Resource>, Ref<Resource>> &p_resource_remap) const;
    void remap_nested_resources(const Ref<Resource> &p_resource, const HashMap<Ref<Resource>, Ref<Resource>> &p_resource_remap) const;
#endif

    // used by editors, to save what has changed only
    void set_scene_instance_state(const Ref<SceneState> &p_state);
    Ref<SceneState> get_scene_instance_state() const;

    void set_scene_inherited_state(const Ref<SceneState> &p_state);
    Ref<SceneState> get_scene_inherited_state() const;

    void set_scene_instance_load_placeholder(bool p_enable);
    bool get_scene_instance_load_placeholder() const;

    //static Vector<Variant> make_binds(VARIANT_ARG_LIST);
    template<typename ...Args>
    static Vector<Variant> make_binds(Args... args) {
        return Vector<Variant> {args...};
    }

    void replace_by(Node *p_node, bool p_keep_data = false);

    void set_pause_mode(PauseMode p_mode);
    PauseMode get_pause_mode() const;
    bool can_process() const;
    bool can_process_notification(int p_what) const;

    void request_ready();

    static void print_stray_nodes();

#ifdef TOOLS_ENABLED
    String validate_child_name(Node *p_child);
#endif

    void queue_delete();

    //hacks for speed
    static void set_human_readable_collision_renaming(bool p_enabled);
    static void init_node_hrcr();

    void force_parent_owned() { parent_owned = true; } //hack to avoid duplicate nodes

    void set_import_path(const NodePath &p_import_path); //path used when imported, used by scene editors to keep tracking
    NodePath get_import_path() const;

    bool is_owned_by_parent() const;

    void clear_internal_tree_resource_paths();

    Viewport *get_viewport() const { return viewport; }

    virtual String get_configuration_warning() const;

    void update_configuration_warning();

    void set_display_folded(bool p_folded);
    bool is_displayed_folded() const;
    /* NETWORK */

    void set_network_master(int p_peer_id, bool p_recursive = true);
    int get_network_master() const;
    bool is_network_master() const;

    uint16_t rpc_config(const StringName &p_method, MultiplayerAPI_RPCMode p_mode); // config a local method for RPC
    uint16_t rset_config(const StringName &p_property, MultiplayerAPI_RPCMode p_mode); // config a local property for RPC

    void rpc(const StringName &p_method, VARIANT_ARG_LIST); //rpc call, honors RPCMode
    void rpc_unreliable(const StringName &p_method, VARIANT_ARG_LIST); //rpc call, honors RPCMode
    void rpc_id(int p_peer_id, const StringName &p_method, VARIANT_ARG_LIST); //rpc call, honors RPCMode
    void rpc_unreliable_id(int p_peer_id, const StringName &p_method, VARIANT_ARG_LIST); //rpc call, honors RPCMode

    void rset(const StringName &p_property, const Variant &p_value); //remote set call, honors RPCMode
    void rset_unreliable(const StringName &p_property, const Variant &p_value); //remote set call, honors RPCMode
    void rset_id(int p_peer_id, const StringName &p_property, const Variant &p_value); //remote set call, honors RPCMode
    void rset_unreliable_id(int p_peer_id, const StringName &p_property, const Variant &p_value); //remote set call, honors RPCMode

    void rpcp(int p_peer_id, bool p_unreliable, const StringName &p_method, const Variant **p_arg, int p_argcount);
    void rsetp(int p_peer_id, bool p_unreliable, const StringName &p_property, const Variant &p_value);

    Ref<MultiplayerAPI> get_multiplayer() const;
    const Ref<MultiplayerAPI> &get_custom_multiplayer() const { return multiplayer; }
    void set_custom_multiplayer(Ref<MultiplayerAPI> p_multiplayer);
    MultiplayerAPI_RPCMode get_node_rpc_mode(const StringName &p_method) const;
    MultiplayerAPI_RPCMode get_node_rpc_mode_by_id(const uint16_t p_rpc_method_id) const;
    MultiplayerAPI_RPCMode get_node_rset_mode(const StringName &p_property) const;
    MultiplayerAPI_RPCMode get_node_rset_mode_by_id(const uint16_t p_rset_property_id) const;
#ifdef DEBUG_ENABLED
    /// Used in ObjectDB::cleanup() warning print
    const char *get_dbg_name() const override { return get_name().asCString(); }
#endif

    Node();
    ~Node() override;
};

#ifdef TOOLS_ENABLED
// Internal function used by a few editors
Node *_find_script_node(Node *p_edited_scene, Node *p_current_node, const Ref<Script> &script);
#endif
