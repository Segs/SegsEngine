/*************************************************************************/
/*  scene_tree.h                                                         */
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

#include "core/os/main_loop.h"
#include "core/os/thread_safe.h"
#include "scene/resources/world_3d.h"
#include "scene/resources/world_2d.h"
#include "scene/main/scene_tree_notifications.h"
#include "core/hash_map.h"
#include "core/hash_set.h"
#include "core/deque.h"

class PackedScene;
class Node;
class Viewport;
class Material;
class Mesh;
class ArrayMesh;
class MultiplayerAPI;
class NetworkedMultiplayerPeer;

/**
 * @brief The SceneTreeLink type is used in ecs to link the given entity to a scene tree
 */
struct SceneTreeLink {
    class SceneTree *my_tree;
};

class ISceneTreeDebugAccessor {
    friend class ScriptDebuggerRemote;

    virtual void _live_edit_node_path_func( const NodePath &p_path, int p_id)=0;
    virtual void _live_edit_res_path_func( StringView p_path, int p_id)=0;

    virtual void _live_edit_node_set_func( int p_id, const StringName &p_prop, const Variant &p_value)=0;
    virtual  void _live_edit_node_set_res_func( int p_id, const StringName &p_prop, StringView p_value)=0;
    virtual  void _live_edit_node_call_func( int p_id, const StringName &p_method, VARIANT_ARG_DECLARE)=0;
    virtual  void _live_edit_res_set_func( int p_id, const StringName &p_prop, const Variant &p_value)=0;
    virtual  void _live_edit_res_set_res_func( int p_id, const StringName &p_prop, StringView p_value)=0;
    virtual  void _live_edit_res_call_func( int p_id, const StringName &p_method, VARIANT_ARG_DECLARE)=0;
    virtual  void _live_edit_root_func( const NodePath &p_scene_path, StringView p_scene_from)=0;
    virtual  void _live_edit_create_node_func( const NodePath &p_parent, const String &p_type, const String &p_name)=0;
    virtual  void _live_edit_instance_node_func(const NodePath &p_parent,StringView p_path, const String &p_name)=0;
    virtual  void _live_edit_remove_node_func( const NodePath &p_at)=0;
    virtual  void _live_edit_remove_and_keep_node_func( const NodePath &p_at, GameEntity p_keep_id)=0;
    virtual  void _live_edit_restore_node_func( GameEntity p_id, const NodePath &p_at, int p_at_pos)=0;
    virtual  void _live_edit_duplicate_node_func( const NodePath &p_at, const String &p_new_name)=0;
    virtual  void _live_edit_reparent_node_func(
             const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos)=0;
public:
    virtual ~ISceneTreeDebugAccessor() {}
    virtual HashMap<String, HashSet<Node *>> &get_live_scene_edit_cache() = 0;
    virtual HashMap<Node *, HashMap<GameEntity, Node *>> &get_live_edit_remove_list() = 0;
};

class GODOT_EXPORT SceneTreeTimer : public RefCounted {
    GDCLASS(SceneTreeTimer,RefCounted)

    float time_left;
    bool process_pause;
    bool ignore_time_scale = false;

protected:
    static void _bind_methods();

public:
    void set_time_left(float p_time);
    float get_time_left() const;

    void set_pause_mode_process(bool p_pause_mode_process);
    bool is_pause_mode_process();

    void set_ignore_time_scale(bool p_ignore);
    bool is_ignore_time_scale();

    void release_connections();

    SceneTreeTimer();
};
struct SceneTreeGroup
{
    Vector<Node *> nodes;
    //uint64_t last_tree_version;
    bool changed=false;
};

class GODOT_EXPORT SceneTree : public MainLoop {

    _THREAD_SAFE_CLASS_

    GDCLASS(SceneTree,MainLoop)

public:
    using IdleCallback = void (*)();

    enum StretchMode {

        STRETCH_MODE_DISABLED,
        STRETCH_MODE_2D,
        STRETCH_MODE_VIEWPORT,
    };

    enum StretchAspect {

        STRETCH_ASPECT_IGNORE,
        STRETCH_ASPECT_KEEP,
        STRETCH_ASPECT_KEEP_WIDTH,
        STRETCH_ASPECT_KEEP_HEIGHT,
        STRETCH_ASPECT_EXPAND,
    };

private:

    Viewport *root;

    uint64_t tree_version;
    float physics_process_time;
    float idle_process_time;
    bool accept_quit;
    bool quit_on_go_back;

#ifdef DEBUG_ENABLED
    bool debug_collisions_hint;
    bool debug_navigation_hint;
#endif
    bool pause;
    int root_lock;

    HashMap<StringName, SceneTreeGroup> group_map;
    bool _quit;
    bool initialized;
    bool input_handled;
    bool _physics_interpolation_enabled;

    Size2 last_screen_size;
    StringName tree_changed_name;
    StringName node_added_name;
    StringName node_removed_name;
    StringName node_renamed_name;

    bool use_font_oversampling;
    int64_t current_frame;
    int64_t current_event;
    int node_count;

#ifdef TOOLS_ENABLED
    Node *edited_scene_root;
#endif
    struct UGCall {

        StringName group;
        StringName call;

        bool operator==(const UGCall &p_with) const { return group == p_with.group && call == p_with.call; }
     private:
        friend eastl::hash<UGCall>;
        explicit operator size_t() const { return (size_t(group.hash())<<16) ^ call.hash(); }

    };

    //safety for when a node is deleted while a group is being called
    int call_lock;
    HashSet<Node *> call_skip; //skip erased nodes

    StretchMode stretch_mode;
    StretchAspect stretch_aspect;
    Size2i stretch_min;
    real_t stretch_scale;

    void _update_font_oversampling(float p_ratio);
    void _update_root_rect();

    Vector<GameEntity> delete_queue;
    //TODO: SEGS: consider replacing Vector below with FixedVector<Variant,VARIANT_ARG_MAX>
    HashMap<UGCall, Vector<Variant> > unique_group_calls;
    bool ugc_locked;
    void _flush_ugc();

    _FORCE_INLINE_ void _update_group_order(SceneTreeGroup &g, bool p_use_priority = false);


    Node *current_scene;

    Color debug_collisions_color;
    Color debug_collision_contact_color;
    Color debug_navigation_color;
    Color debug_navigation_disabled_color;
    Ref<ArrayMesh> debug_contact_mesh;
    Ref<Material> navigation_material;
    Ref<Material> navigation_disabled_material;
    Ref<Material> collision_material;
    int collision_debug_contacts;

    void _change_scene(Node *p_to);
    //void _call_group(uint32_t p_call_flags,const StringName& p_group,const StringName& p_function,const Variant& p_arg1,const Variant& p_arg2);

    List<Ref<SceneTreeTimer> > timers;

    ///network///

    Ref<MultiplayerAPI> multiplayer;
    bool multiplayer_poll;

    void _network_peer_connected(int p_id);
    void _network_peer_disconnected(int p_id);

    void _connected_to_server();
    void _connection_failed();
    void _server_disconnected();

    static SceneTree *singleton;
    friend class Node;
    //optimization
    friend class CanvasItem;
    friend class Node3D;
    friend class Viewport;

    //IntrusiveList<Node> xform_change_list;

    friend class ScriptDebuggerRemote;

    void tree_changed();
    void node_added(Node *p_node);
    void node_removed(Node *p_node);
    void node_renamed(Node *p_node);

    SceneTreeGroup *add_to_group(const StringName &p_group, Node *p_node);
    void remove_from_group(const StringName &p_group, Node *p_node);
    void make_group_changed(const StringName &p_group);

    void _notify_group_pause(const StringName &p_group, int p_notification);
    void _call_input_pause(const StringName &p_group, const StringName &p_method, const Ref<InputEvent> &p_input);

    void _flush_delete_queue();
#ifdef DEBUG_ENABLED
    ISceneTreeDebugAccessor *m_debug_data=nullptr;
    ISceneTreeDebugAccessor *debug() { return m_debug_data;}
    void _debugger_request_tree();
#endif

    enum {
        MAX_IDLE_CALLBACKS = 256
    };

    static IdleCallback idle_callbacks[MAX_IDLE_CALLBACKS];
    static int idle_callback_count;
    void _call_idle_callbacks();

protected:
    void _notification(int p_notification);
    static void _bind_methods();

public:
    Array _get_nodes_in_group(const StringName &p_group);
    Variant _call_group_flags(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
    Variant _call_group(const Variant **p_args, int p_argcount, Callable::CallError &r_error);

public:
    enum GroupCallFlags {
        GROUP_CALL_DEFAULT = 0,
        GROUP_CALL_REVERSE = 1,
        GROUP_CALL_REALTIME = 2,
        GROUP_CALL_UNIQUE = 4,
    };

    _FORCE_INLINE_ Viewport *get_root() const { return root; }

    void call_group_flags(uint32_t p_call_flags, const StringName &p_group, const StringName &p_function, VARIANT_ARG_LIST);
    void notify_group_flags(uint32_t p_call_flags, const StringName &p_group, int p_notification);
    void set_group_flags(uint32_t p_call_flags, const StringName &p_group, const StringName &p_name, const Variant &p_value);

    void call_group(const StringName &p_group, const StringName &p_function, VARIANT_ARG_LIST);
    void notify_group(const StringName &p_group, int p_notification);
    void set_group(const StringName &p_group, const StringName &p_name, const Variant &p_value);

    void flush_transform_notifications();

    void input_text(StringView p_text) override;
    void input_event(const Ref<InputEvent> &p_event) override;
    void init() override;

    bool iteration(float p_time) override;
    bool idle(float p_time) override;

    void finish() override;

    void set_auto_accept_quit(bool p_enable) { accept_quit = p_enable; }
    void set_quit_on_go_back(bool p_enable);

    void quit(int p_exit_code = -1);

    void set_input_as_handled();
    bool is_input_handled();
    _FORCE_INLINE_ float get_physics_process_time() const { return physics_process_time; }
    _FORCE_INLINE_ float get_idle_process_time() const { return idle_process_time; }

#ifdef TOOLS_ENABLED
    bool is_node_being_edited(const Node *p_node) const;
#else
    bool is_node_being_edited(const Node *p_node) const { return false; }
#endif

    void set_pause(bool p_enabled);
    bool is_paused() const;

#ifdef DEBUG_ENABLED
    void set_debug_collisions_hint(bool p_enabled);
    bool is_debugging_collisions_hint() const;

    void set_debug_navigation_hint(bool p_enabled);
    bool is_debugging_navigation_hint() const;

    HashMap<String, HashSet<Node *> > &get_live_scene_edit_cache();
    HashMap<Node *, HashMap<GameEntity, Node *> > &get_live_edit_remove_list();
#else
    void set_debug_collisions_hint(bool p_enabled) {}
    bool is_debugging_collisions_hint() const { return false; }

    void set_debug_navigation_hint(bool p_enabled) {}
    bool is_debugging_navigation_hint() const { return false; }
#endif

    void set_debug_collisions_color(const Color &p_color);
    Color get_debug_collisions_color() const;

    void set_debug_collision_contact_color(const Color &p_color);
    Color get_debug_collision_contact_color() const;

    void set_debug_navigation_color(const Color &p_color);
    Color get_debug_navigation_color() const;

    void set_debug_navigation_disabled_color(const Color &p_color);
    Color get_debug_navigation_disabled_color() const;

    Ref<Material> get_debug_navigation_material();
    Ref<Material> get_debug_navigation_disabled_material();
    Ref<Material> get_debug_collision_material();
    Ref<ArrayMesh> get_debug_contact_mesh();

    int get_collision_debug_contact_count() { return collision_debug_contacts; }

    int64_t get_frame() const;
    int64_t get_event_count() const;

    int get_node_count() const;

    void queue_delete(Object *p_object);

    void get_nodes_in_group(const StringName &p_group, Dequeue<Node *> *p_list);
    bool has_group(const StringName &p_identifier) const;

    void set_screen_stretch(StretchMode p_mode, StretchAspect p_aspect, const Size2 &p_minsize, real_t p_scale = 1.0f);

    void set_use_font_oversampling(bool p_oversampling);
    bool is_using_font_oversampling() const;

    //void change_scene(const String& p_path);
    //Node *get_loaded_scene();

    void set_edited_scene_root(Node *p_node);
    Node *get_edited_scene_root() const;

    void set_current_scene(Node *p_scene);
    Node *get_current_scene() const;
    Error change_scene(StringView p_path);
    Error change_scene_to(const Ref<PackedScene> &p_scene);
    Error reload_current_scene();

    Ref<SceneTreeTimer> create_timer(float p_delay_sec, bool p_process_pause = true);

    //used by Main::start, don't use otherwise
    void add_current_scene(Node *p_current);

    static SceneTree *get_singleton() { return singleton; }

    void drop_files(const Vector<String> &p_files, int p_from_screen = 0) override;
    void global_menu_action(const Variant &p_id, const Variant &p_meta) override;

    //network API

    Ref<MultiplayerAPI> get_multiplayer() const;
    void set_multiplayer_poll_enabled(bool p_enabled);
    bool is_multiplayer_poll_enabled() const;
    void set_multiplayer(const Ref<MultiplayerAPI>& p_multiplayer);
    void set_network_peer(const Ref<NetworkedMultiplayerPeer> &p_network_peer);
    Ref<NetworkedMultiplayerPeer> get_network_peer() const;
    bool is_network_server() const;
    bool has_network_peer() const;
    int get_network_unique_id() const;
    Vector<int> get_network_connected_peers() const;
    int get_rpc_sender_id() const;

    void set_refuse_new_network_connections(bool p_refuse);
    bool is_refusing_new_network_connections() const;

    static void add_idle_callback(IdleCallback p_callback);
    SceneTree();
    ~SceneTree() override;
};

