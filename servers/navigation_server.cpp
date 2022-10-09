/*************************************************************************/
/*  navigation_server.cpp                                                */
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

/**
    @author AndreaCatania
*/

#include "navigation_server.h"

#include "core/method_bind_interface.h"
#include "core/method_bind.h"

IMPL_GDCLASS(NavigationServer)

NavigationServer *NavigationServer::singleton = nullptr;

void NavigationServer::_bind_methods() {

    BIND_METHOD(NavigationServer,get_maps);

    BIND_METHOD(NavigationServer,map_create);
    BIND_METHOD(NavigationServer,map_set_active);
    BIND_METHOD(NavigationServer,map_is_active);
    BIND_METHOD(NavigationServer,map_set_up);
    BIND_METHOD(NavigationServer,map_get_up);
    BIND_METHOD(NavigationServer,map_set_cell_size);
    BIND_METHOD(NavigationServer,map_get_cell_size);
    BIND_METHOD(NavigationServer,map_set_cell_height);
    BIND_METHOD(NavigationServer,map_get_cell_height);
    BIND_METHOD(NavigationServer,map_set_edge_connection_margin);
    BIND_METHOD(NavigationServer,map_get_edge_connection_margin);
    MethodBinder::bind_method(D_METHOD("map_get_path", {"map", "origin", "destination", "optimize", "navigation_layers"}),&NavigationServer::map_get_path,{DEFVAL(int(1))});
    MethodBinder::bind_method(D_METHOD("map_get_closest_point_to_segment", {"map", "start", "end", "use_collision"}),&NavigationServer::map_get_closest_point_to_segment, {DEFVAL(false)});
    BIND_METHOD(NavigationServer,map_get_closest_point);
    BIND_METHOD(NavigationServer,map_get_closest_point_normal);
    BIND_METHOD(NavigationServer,map_get_closest_point_owner);

    BIND_METHOD(NavigationServer,map_get_regions);
    BIND_METHOD(NavigationServer,map_get_agents);
    BIND_METHOD(NavigationServer,map_force_update);

    BIND_METHOD(NavigationServer,region_create);
    BIND_METHOD(NavigationServer,region_set_enter_cost);
    BIND_METHOD(NavigationServer,region_get_enter_cost);
    BIND_METHOD(NavigationServer,region_set_travel_cost);
    BIND_METHOD(NavigationServer,region_get_travel_cost);
    BIND_METHOD(NavigationServer,region_owns_point);

    BIND_METHOD(NavigationServer,region_set_map);
    BIND_METHOD(NavigationServer,region_get_map);

    BIND_METHOD(NavigationServer,region_set_navigation_layers);
    BIND_METHOD(NavigationServer,region_get_navigation_layers);

    BIND_METHOD(NavigationServer,region_set_transform);
    BIND_METHOD(NavigationServer,region_set_navmesh);
    BIND_METHOD(NavigationServer,region_bake_navmesh);
    BIND_METHOD(NavigationServer,region_get_connections_count);
    BIND_METHOD(NavigationServer,region_get_connection_pathway_start);
    BIND_METHOD(NavigationServer,region_get_connection_pathway_end);

    BIND_METHOD(NavigationServer,agent_create);
    BIND_METHOD(NavigationServer,agent_set_map);
    BIND_METHOD(NavigationServer,agent_get_map);
    BIND_METHOD(NavigationServer,agent_set_neighbor_dist);
    BIND_METHOD(NavigationServer,agent_set_max_neighbors);
    BIND_METHOD(NavigationServer,agent_set_time_horizon);
    BIND_METHOD(NavigationServer,agent_set_radius);
    BIND_METHOD(NavigationServer,agent_set_max_speed);
    BIND_METHOD(NavigationServer,agent_set_velocity);
    BIND_METHOD(NavigationServer,agent_set_target_velocity);
    BIND_METHOD(NavigationServer,agent_set_position);
    BIND_METHOD(NavigationServer,agent_is_map_changed);
    BIND_METHOD(NavigationServer,agent_set_callback);

    BIND_METHOD(NavigationServer,free_rid);

    BIND_METHOD(NavigationServer,set_active);
    BIND_METHOD(NavigationServer,process);

    ADD_SIGNAL(MethodInfo("map_changed", PropertyInfo(VariantType::_RID, "map")));
}

const NavigationServer *NavigationServer::get_singleton() {
    return singleton;
}

NavigationServer *NavigationServer::get_singleton_mut() {
    return singleton;
}

NavigationServer::NavigationServer() {
    ERR_FAIL_COND(singleton != nullptr);
    singleton = this;
}

NavigationServer::~NavigationServer() {
    singleton = nullptr;
}

NavigationServerCallback NavigationServerManager::create_callback = nullptr;

void NavigationServerManager::set_default_server(NavigationServerCallback p_callback) {
    create_callback = p_callback;
}

NavigationServer *NavigationServerManager::new_default_server() {
    ERR_FAIL_COND_V(create_callback == nullptr, nullptr);
    return create_callback();
}
