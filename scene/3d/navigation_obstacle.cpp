/*************************************************************************/
/*  navigation_obstacle.cpp                                              */
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

#include "navigation_obstacle.h"

#include "core/method_bind_interface.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "scene/3d/collision_shape_3d.h"
#include "scene/3d/navigation_3d.h"
#include "scene/3d/physics_body_3d.h"
#include "core/translation_helpers.h"
#include "servers/navigation_server.h"

IMPL_GDCLASS(NavigationObstacle)

void NavigationObstacle::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_navigation", {"navigation"}),&NavigationObstacle::set_navigation_node);
    MethodBinder::bind_method(D_METHOD("get_navigation"), &NavigationObstacle::get_navigation_node);
    SE_BIND_METHOD(NavigationObstacle,is_radius_estimated);
    SE_BIND_METHOD(NavigationObstacle,set_estimate_radius);
    SE_BIND_METHOD(NavigationObstacle,set_radius);
    SE_BIND_METHOD(NavigationObstacle,get_radius);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "estimate_radius"), "set_estimate_radius", "is_radius_estimated");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "radius", PropertyHint::Range, "0.01,100,0.01"), "set_radius", "get_radius");
}

void NavigationObstacle::_validate_property(PropertyInfo &p_property) const {
    if (p_property.name == "radius") {
        if (estimate_radius) {
            p_property.usage = PROPERTY_USAGE_NOEDITOR;
        }
    }
}

void NavigationObstacle::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            parent_spatial = object_cast<Node3D>(get_parent());
            reevaluate_agent_radius();

            // Search the navigation node and set it
            {
                Navigation3D *nav = nullptr;
                Node *p = get_parent();
                while (p != nullptr) {
                    nav = object_cast<Navigation3D>(p);
                    if (nav != nullptr) {
                        p = nullptr;
                   } else {
                        p = p->get_parent();
                   }
                }

                set_navigation(nav);
            }

            set_physics_process_internal(true);
        } break;
        case NOTIFICATION_EXIT_TREE: {
            set_navigation(nullptr);
            set_physics_process_internal(false);
            request_ready(); // required to solve an issue with losing the navigation
        } break;
        case NOTIFICATION_PARENTED: {
            parent_spatial = object_cast<Node3D>(get_parent());
            reevaluate_agent_radius();
        } break;
        case NOTIFICATION_UNPARENTED: {
            parent_spatial = nullptr;
        } break;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
            if (parent_spatial) {
                NavigationServer::get_singleton()->agent_set_position(agent, parent_spatial->get_global_transform().origin);
            }

            PhysicsBody3D *rigid = object_cast<PhysicsBody3D>(get_parent());
            if (rigid) {

                Vector3 v = rigid->get_linear_velocity();
                NavigationServer::get_singleton()->agent_set_velocity(agent, v);
                NavigationServer::get_singleton()->agent_set_target_velocity(agent, v);
            }

        } break;
    }
}

NavigationObstacle::NavigationObstacle() :
        navigation(nullptr),
        agent(RID()) {
    agent = NavigationServer::get_singleton()->agent_create();
    initialize_agent();
}

NavigationObstacle::~NavigationObstacle() {
    NavigationServer::get_singleton()->free_rid(agent);
    agent = RID(); // Pointless
}

void NavigationObstacle::set_navigation(Navigation3D *p_nav) {
    if (navigation == p_nav) {
        return; // Pointless
    }

    navigation = p_nav;
    NavigationServer::get_singleton()->agent_set_map(agent, navigation == nullptr ? RID() : navigation->get_rid());
}

void NavigationObstacle::set_navigation_node(Node *p_nav) {
    Navigation3D *nav = object_cast<Navigation3D>(p_nav);
    ERR_FAIL_COND(nav == nullptr);
    set_navigation(nav);
}

Node *NavigationObstacle::get_navigation_node() const {
    return object_cast<Node>(navigation);
}

String NavigationObstacle::get_configuration_warning() const {
    if (!object_cast<Node3D>(get_parent())) {

        return TTRS("The NavigationObstacle only serves to provide collision avoidance to a spatial object.");
    }

    return String();
}

void NavigationObstacle::initialize_agent() {
    NavigationServer::get_singleton()->agent_set_neighbor_dist(agent, 0.0);
    NavigationServer::get_singleton()->agent_set_max_neighbors(agent, 0);
    NavigationServer::get_singleton()->agent_set_time_horizon(agent, 0.0);
    NavigationServer::get_singleton()->agent_set_max_speed(agent, 0.0);
}

void NavigationObstacle::reevaluate_agent_radius() {
    if (!estimate_radius) {
        NavigationServer::get_singleton()->agent_set_radius(agent, radius);
    } else if (parent_spatial && parent_spatial->is_inside_tree()) {
        NavigationServer::get_singleton()->agent_set_radius(agent, estimate_agent_radius());
    }
}

real_t NavigationObstacle::estimate_agent_radius() const {
    if (parent_spatial) {
    // Estimate the radius of this physics body
        real_t radius = 0.0f;
        for (int i(0); i < parent_spatial->get_child_count(); i++) {
        // For each collision shape
            CollisionShape3D *cs = object_cast<CollisionShape3D>(parent_spatial->get_child(i));
        if (cs) {
            // Take the distance between the Body center to the shape center
            real_t r = cs->get_transform().origin.length();
            if (cs->get_shape()) {
                // and add the enclosing shape radius
                r += cs->get_shape()->get_enclosing_radius();
            }
            Vector3 s = cs->get_global_transform().basis.get_scale();
            r *= M_MAX(s.x, M_MAX(s.y, s.z));
            // Takes the biggest radius
            radius = M_MAX(radius, r);
        }
    }
        Vector3 s = parent_spatial->get_global_transform().basis.get_scale();
        radius *= M_MAX(s.x, M_MAX(s.y, s.z));
    }

    if (radius > 0.0f) {
        return radius;
    }

    return 1.0f; // Never a 0 radius
}

void NavigationObstacle::set_estimate_radius(bool p_estimate_radius) {
    estimate_radius = p_estimate_radius;
    Object_change_notify(this,"estimate_radius");
    reevaluate_agent_radius();
}

void NavigationObstacle::set_radius(real_t p_radius) {
    ERR_FAIL_COND_MSG(p_radius <= 0.0, "Radius must be greater than 0.");
    radius = p_radius;
    reevaluate_agent_radius();
}
