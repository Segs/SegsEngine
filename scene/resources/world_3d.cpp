/*************************************************************************/
/*  world.cpp                                                            */
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

#include "world_3d.h"

#include "core/math/camera_matrix.h"
#include "core/list.h"
#include "core/math/bvh.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/visibility_notifier_3d.h"
#include "scene/scene_string_names.h"
#include "core/method_bind.h"
#include "servers/navigation_server.h"

IMPL_GDCLASS(World3D)
RES_BASE_EXTENSION_IMPL(World3D,"world")
using SpatialPartitionID = uint32_t;
struct SpatialIndexer {

    BVH_Manager<VisibilityNotifier3D *> octree;

    struct NotifierData {

        AABB aabb;
        SpatialPartitionID id;
    };

    HashMap<VisibilityNotifier3D *, NotifierData> notifiers;
    struct CameraData {

        HashMap<VisibilityNotifier3D *, uint64_t> notifiers;
    };

    HashMap<Camera3D *, CameraData> cameras;

    enum {
        VISIBILITY_CULL_MAX = 32768
    };

    Vector<VisibilityNotifier3D *> cull;

    uint64_t pass;
    uint64_t last_frame;
    bool changed;

    void _notifier_add(VisibilityNotifier3D *p_notifier, const AABB &p_rect) {

        ERR_FAIL_COND(notifiers.contains(p_notifier));
        notifiers[p_notifier].aabb = p_rect;
        //TODO: use p_notifier->is_visible() below?
        notifiers[p_notifier].id = octree.create(p_notifier,true, 0,1, p_rect);
        changed = true;
    }

    void _notifier_update(VisibilityNotifier3D *p_notifier, const AABB &p_rect) {

        HashMap<VisibilityNotifier3D *, NotifierData>::iterator E = notifiers.find(p_notifier);
        ERR_FAIL_COND(E==notifiers.end());
        if (E->second.aabb == p_rect)
            return;

        E->second.aabb = p_rect;
        octree.move(E->second.id, E->second.aabb);
        changed = true;
    }

    void _notifier_remove(VisibilityNotifier3D *p_notifier) {

        HashMap<VisibilityNotifier3D *, NotifierData>::iterator E = notifiers.find(p_notifier);
        ERR_FAIL_COND(E!=notifiers.end());

        octree.erase(E->second.id);
        notifiers.erase(p_notifier);

        Vector<Camera3D *> removed;
        for (eastl::pair< Camera3D *const,CameraData> &F : cameras) {

            HashMap<VisibilityNotifier3D *, uint64_t>::iterator G = F.second.notifiers.find(p_notifier);

            if (G!=F.second.notifiers.end()) {
                F.second.notifiers.erase(G);
                removed.push_back(F.first);
            }
        }

        for(Camera3D *c : removed) {
            p_notifier->_exit_camera(c);
        }

        changed = true;
    }

    void _add_camera(Camera3D *p_camera) {

        ERR_FAIL_COND(cameras.contains(p_camera));
        CameraData vd;
        cameras[p_camera] = vd;
        changed = true;
    }

    void _update_camera(Camera3D *p_camera) {

        HashMap<Camera3D *, CameraData>::iterator E = cameras.find(p_camera);
        ERR_FAIL_COND(E==cameras.end());
        changed = true;
    }

    void _remove_camera(Camera3D *p_camera) {
        ERR_FAIL_COND(!cameras.contains(p_camera));
        Vector<VisibilityNotifier3D *> removed;
        for (auto &E : cameras[p_camera].notifiers) {

            removed.push_back(E.first);
        }

        for(auto v : removed) {
            v->_exit_camera(p_camera);
        }

        cameras.erase(p_camera);
    }

    void _update(uint64_t p_frame) {

        if (p_frame == last_frame)
            return;
        last_frame = p_frame;

        if (!changed)
            return;

        for (eastl::pair<Camera3D *const,CameraData> &E : cameras) {

            pass++;

            Camera3D *c = E.first;

            Vector3 cam_pos = c->get_global_transform().origin;
            Frustum planes = c->get_frustum();
            bool cam_is_ortho = c->get_projection() == Camera3D::PROJECTION_ORTHOGONAL;

            int culled = octree.cull_convex(planes, cull,nullptr);

            VisibilityNotifier3D **ptr = cull.data();

            Vector<VisibilityNotifier3D *> added;
            Vector<VisibilityNotifier3D *> removed;

            for (int i = 0; i < culled; i++) {

                //notifiers in frustum

                // check and remove notifiers that have a max range
                VisibilityNotifier3D &nt = *ptr[i];
                if (nt.is_max_distance_active() && !cam_is_ortho) {
                    Vector3 offset = nt.get_world_aabb_center() - cam_pos;
                    if ((offset.length_squared() >= nt.get_max_distance_squared()) && !nt.inside_max_distance_leadin()) {
                        // unordered remove
                        cull[i] = cull[culled - 1];
                        culled--;
                        i--;
                        continue;
                    }
                }
                HashMap<VisibilityNotifier3D *, uint64_t>::iterator H = E.second.notifiers.find(ptr[i]);
                if (H==E.second.notifiers.end()) {

                    E.second.notifiers.emplace(ptr[i], pass);
                    added.push_back(ptr[i]);
                } else {
                    H->second = pass;
                }
            }

            for (auto &F : E.second.notifiers) {

                if (F.second != pass)
                    removed.push_back(F.first);
            }

            for(VisibilityNotifier3D * vn : added) {
                vn->_enter_camera(E.first);
            }

            for(VisibilityNotifier3D * r : removed) {
                E.second.notifiers.erase(r);
                r->_exit_camera(E.first);
            }
        }
        changed = false;
    }

    SpatialIndexer() {

        pass = 0;
        last_frame = 0;
        changed = false;
        cull.resize(VISIBILITY_CULL_MAX);
    }
};

void World3D::_register_camera(Camera3D *p_camera) {

#ifndef _3D_DISABLED
    indexer->_add_camera(p_camera);
#endif
}

void World3D::_update_camera(Camera3D *p_camera) {

#ifndef _3D_DISABLED
    indexer->_update_camera(p_camera);
#endif
}
void World3D::_remove_camera(Camera3D *p_camera) {

#ifndef _3D_DISABLED
    indexer->_remove_camera(p_camera);
#endif
}

void World3D::_register_notifier(VisibilityNotifier3D *p_notifier, const AABB &p_rect) {

#ifndef _3D_DISABLED
    indexer->_notifier_add(p_notifier, p_rect);
#endif
}

void World3D::_update_notifier(VisibilityNotifier3D *p_notifier, const AABB &p_rect) {

#ifndef _3D_DISABLED
    indexer->_notifier_update(p_notifier, p_rect);
#endif
}

void World3D::_remove_notifier(VisibilityNotifier3D *p_notifier) {

#ifndef _3D_DISABLED
    indexer->_notifier_remove(p_notifier);
#endif
}

void World3D::_update(uint64_t p_frame) {

#ifndef _3D_DISABLED
    indexer->_update(p_frame);
#endif
}

RID World3D::get_space() const {

    return physics_space;
}
RenderingEntity World3D::get_scenario() const {

    return renderer_scene;
}

RID World3D::get_navigation_map() const {
    return navigation_map;
}

void World3D::set_environment(const Ref<Environment> &p_environment) {
    if (environment == p_environment) {
        return;
    }
    environment = p_environment;
    RenderingServer::get_singleton()->scenario_set_environment(renderer_scene, environment ? environment->get_rid() : entt::null);

    emit_changed();
}

Ref<Environment> World3D::get_environment() const {

    return environment;
}

void World3D::set_fallback_environment(const Ref<Environment> &p_environment) {
    if (fallback_environment == p_environment) {
        return;
    }

    fallback_environment = p_environment;
    RenderingServer::get_singleton()->scenario_set_fallback_environment(renderer_scene, fallback_environment ? fallback_environment->get_rid() : entt::null);

    emit_changed();
}

Ref<Environment> World3D::get_fallback_environment() const {

    return fallback_environment;
}

PhysicsDirectSpaceState3D *World3D::get_direct_space_state() {

    return PhysicsServer3D::get_singleton()->space_get_direct_state(physics_space);
}

void World3D::get_camera_list(Vector<Camera3D *> *r_cameras) {

    for (const eastl::pair<Camera3D *const,SpatialIndexer::CameraData> &E : indexer->cameras) {
        r_cameras->push_back(E.first);
    }
}

void World3D::_bind_methods() {

    SE_BIND_METHOD(World3D, get_space);
    SE_BIND_METHOD(World3D, get_scenario);
    SE_BIND_METHOD(World3D, get_navigation_map);
    SE_BIND_METHOD(World3D, set_environment);
    SE_BIND_METHOD(World3D, get_environment);
    SE_BIND_METHOD(World3D, set_fallback_environment);
    SE_BIND_METHOD(World3D, get_fallback_environment);
    SE_BIND_METHOD(World3D, get_direct_space_state);
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "environment", PropertyHint::ResourceType, "Environment"), "set_environment", "get_environment");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "fallback_environment", PropertyHint::ResourceType, "Environment"), "set_fallback_environment", "get_fallback_environment");
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "space", PropertyHint::None, "", 0), "", "get_space");
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "scenario", PropertyHint::None, "", 0), "", "get_scenario");
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "navigation_map", PropertyHint::None, "", 0), "", "get_navigation_map");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "direct_space_state", PropertyHint::ResourceType, "PhysicsDirectSpaceState3D", 0), "",
            "get_direct_space_state");
}

World3D::World3D() {

    physics_space = PhysicsServer3D::get_singleton()->space_create();
    renderer_scene = RenderingServer::get_singleton()->scenario_create();

    PhysicsServer3D::get_singleton()->space_set_active(physics_space, true);
    PhysicsServer3D::get_singleton()->area_set_param(physics_space, PhysicsServer3D::AREA_PARAM_GRAVITY, GLOBAL_DEF("physics/3d/default_gravity", 9.8));
    PhysicsServer3D::get_singleton()->area_set_param(physics_space, PhysicsServer3D::AREA_PARAM_GRAVITY_VECTOR, GLOBAL_DEF("physics/3d/default_gravity_vector", Vector3(0, -1, 0)));
    PhysicsServer3D::get_singleton()->area_set_param(physics_space, PhysicsServer3D::AREA_PARAM_LINEAR_DAMP, GLOBAL_DEF("physics/3d/default_linear_damp", 0.1));
    ProjectSettings::get_singleton()->set_custom_property_info("physics/3d/default_linear_damp", PropertyInfo(VariantType::FLOAT, "physics/3d/default_linear_damp", PropertyHint::Range, "-1,100,0.001,or_greater"));
    PhysicsServer3D::get_singleton()->area_set_param(physics_space, PhysicsServer3D::AREA_PARAM_ANGULAR_DAMP, GLOBAL_DEF("physics/3d/default_angular_damp", 0.1));
    ProjectSettings::get_singleton()->set_custom_property_info("physics/3d/default_angular_damp", PropertyInfo(VariantType::FLOAT, "physics/3d/default_angular_damp", PropertyHint::Range, "-1,100,0.001,or_greater"));
    	// Create default navigation map
    navigation_map = NavigationServer::get_singleton()->map_create();
    NavigationServer::get_singleton()->map_set_active(navigation_map, true);
    NavigationServer::get_singleton()->map_set_up(navigation_map, T_GLOBAL_DEF("navigation/3d/default_map_up", Vector3(0, 1, 0)));
    NavigationServer::get_singleton()->map_set_cell_size(navigation_map, T_GLOBAL_DEF("navigation/3d/default_cell_size", 0.25f));
    NavigationServer::get_singleton()->map_set_cell_height(navigation_map, T_GLOBAL_DEF("navigation/3d/default_cell_height", 0.25f));
    NavigationServer::get_singleton()->map_set_edge_connection_margin(
            navigation_map, T_GLOBAL_DEF("navigation/3d/default_edge_connection_margin", 0.25f));

#ifdef _3D_DISABLED
    indexer = NULL;
#else
    indexer = memnew(SpatialIndexer);
#endif
}

World3D::~World3D() {

    PhysicsServer3D::get_singleton()->free_rid(physics_space);
    RenderingServer::get_singleton()->free_rid(renderer_scene);

#ifndef _3D_DISABLED
    memdelete(indexer);
#endif
}
