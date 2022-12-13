/*************************************************************************/
/*  gd_navigation_server.h                                               */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "core/rid.h"
//#include "core/rid_owner.h"
#include "servers/navigation_server.h"
#include "core/os/mutex.h"
#include "core/rid.h"
#include "nav_map.h"
#include "nav_region.h"
#include "rvo_agent.h"

/// The commands are functions executed during the `sync` phase.

#define MERGE_INTERNAL(A, B) A##B
#define MERGE(A, B) MERGE_INTERNAL(A, B)

#define COMMAND_1(F_NAME, T_0, D_0)     \
    void F_NAME(T_0 D_0) const override; \
    void MERGE(_cmd_, F_NAME)(T_0 D_0)

#define COMMAND_2(F_NAME, T_0, D_0, T_1, D_1)    \
    void F_NAME(T_0 D_0, T_1 D_1) const override; \
    void MERGE(_cmd_, F_NAME)(T_0 D_0, T_1 D_1)

#define COMMAND_4_DEF(F_NAME, T_0, D_0, T_1, D_1, T_2, D_2, T_3, D_3, D_3_DEF) \
    void F_NAME(T_0 D_0, T_1 D_1, T_2 D_2, T_3 D_3 = D_3_DEF) const override;  \
    void MERGE(_cmd_, F_NAME)(T_0 D_0, T_1 D_1, T_2 D_2, T_3 D_3)

class GodotNavigationServer;

struct SetCommand {
    virtual ~SetCommand() {}
    virtual void exec(GodotNavigationServer *server) = 0;
};

class GodotNavigationServer : public NavigationServer {
    mutable Mutex commands_mutex;
    /// Mutex used to make any operation threadsafe.
    mutable Mutex operations_mutex;

    Vector<SetCommand *> commands;

    mutable RID_Owner<NavMap> map_owner;
    mutable RID_Owner<NavRegion> region_owner;
    mutable RID_Owner<RvoAgent> agent_owner;

    bool active = true;
    Vector<NavMap *> active_maps;
    Vector<uint32_t> active_maps_update_id;

public:
    GodotNavigationServer();
    virtual ~GodotNavigationServer();

    void add_command(SetCommand *command) const;

    Array get_maps() const override;

    RID map_create() const override;
    COMMAND_2(map_set_active, RID, p_map, bool, p_active);
    bool map_is_active(RID p_map) const override;

    COMMAND_2(map_set_up, RID, p_map, Vector3, p_up);
    Vector3 map_get_up(RID p_map) const override;

    COMMAND_2(map_set_cell_size, RID, p_map, real_t, p_cell_size);
    real_t map_get_cell_size(RID p_map) const override;

    COMMAND_2(map_set_cell_height, RID, p_map, real_t, p_cell_height);
    real_t map_get_cell_height(RID p_map) const override;
    COMMAND_2(map_set_edge_connection_margin, RID, p_map, real_t, p_connection_margin);
    real_t map_get_edge_connection_margin(RID p_map) const override;

    Vector<Vector3> map_get_path(RID p_map, Vector3 p_origin, Vector3 p_destination, bool p_optimize, uint32_t p_layers = 1) const override;
    Vector3 map_get_closest_point_to_segment(RID p_map, const Vector3 &p_from, const Vector3 &p_to, const bool p_use_collision = false) const override;
    Vector3 map_get_closest_point(RID p_map, const Vector3 &p_point) const override;
    Vector3 map_get_closest_point_normal(RID p_map, const Vector3 &p_point) const override;
    RID map_get_closest_point_owner(RID p_map, const Vector3 &p_point) const override;

    Array map_get_regions(RID p_map) const override;
    Array map_get_agents(RID p_map) const override;

    void map_force_update(RID p_map) override;

    RID region_create() const override;
    COMMAND_2(region_set_enter_cost, RID, p_region, real_t, p_enter_cost);
    real_t region_get_enter_cost(RID p_region) const override;
    COMMAND_2(region_set_travel_cost, RID, p_region, real_t, p_travel_cost);
    real_t region_get_travel_cost(RID p_region) const override;

    bool region_owns_point(RID p_region, const Vector3 &p_point) const override;

    COMMAND_2(region_set_map, RID, p_region, RID, p_map);
    RID region_get_map(RID p_region) const override;
    COMMAND_2(region_set_navigation_layers, RID, p_region, uint32_t, p_navigation_layers);
    uint32_t region_get_navigation_layers(RID p_region) const override;
    COMMAND_2(region_set_transform, RID, p_region, Transform, p_transform);
    COMMAND_2(region_set_navmesh, RID, p_region, Ref<NavigationMesh>, p_nav_mesh);
    void region_bake_navmesh(Ref<NavigationMesh> r_mesh, Node *p_node) const override;

    int region_get_connections_count(RID p_region) const override;
    Vector3 region_get_connection_pathway_start(RID p_region, int p_connection_id) const override;
    Vector3 region_get_connection_pathway_end(RID p_region, int p_connection_id) const override;

    RID agent_create() const override;
    COMMAND_2(agent_set_map, RID, p_agent, RID, p_map);
    RID agent_get_map(RID p_agent) const override;
    COMMAND_2(agent_set_neighbor_dist, RID, p_agent, real_t, p_dist);
    COMMAND_2(agent_set_max_neighbors, RID, p_agent, int, p_count);
    COMMAND_2(agent_set_time_horizon, RID, p_agent, real_t, p_time);
    COMMAND_2(agent_set_radius, RID, p_agent, real_t, p_radius);
    COMMAND_2(agent_set_max_speed, RID, p_agent, real_t, p_max_speed);
    COMMAND_2(agent_set_velocity, RID, p_agent, Vector3, p_velocity);
    COMMAND_2(agent_set_target_velocity, RID, p_agent, Vector3, p_velocity);
    COMMAND_2(agent_set_position, RID, p_agent, Vector3, p_position);
    COMMAND_2(agent_set_ignore_y, RID, p_agent, bool, p_ignore);
    bool agent_is_map_changed(RID p_agent) const override;

    void agent_set_callback(RID p_agent, Callable && cb) const override;
    void _cmd_agent_set_callback(RID p_agent, Callable&& cb);

    COMMAND_1(free_rid, RID, p_object);

    void set_active(bool p_active) const override;
    void flush_queries();
    void process(real_t p_delta_time) override;
};

#undef COMMAND_1
#undef COMMAND_2
#undef COMMAND_4_DEF
