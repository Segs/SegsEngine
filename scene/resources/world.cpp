/*************************************************************************/
/*  world.cpp                                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "world.h"

#include "core/math/camera_matrix.h"
#include "core/math/octree.h"
#include "core/list.h"
#include "scene/3d/camera.h"
#include "scene/3d/visibility_notifier.h"
#include "scene/scene_string_names.h"
#include "core/method_bind.h"

IMPL_GDCLASS(World)
RES_BASE_EXTENSION_IMPL(World,"world")

struct SpatialIndexer {

    Octree<VisibilityNotifier> octree;

    struct NotifierData {

        AABB aabb;
        OctreeElementID id;
    };

    Map<VisibilityNotifier *, NotifierData> notifiers;
    struct CameraData {

        Map<VisibilityNotifier *, uint64_t> notifiers;
    };

    Map<Camera *, CameraData> cameras;

    enum {
        VISIBILITY_CULL_MAX = 32768
    };

    Vector<VisibilityNotifier *> cull;

    bool changed;
    uint64_t pass;
    uint64_t last_frame;

    void _notifier_add(VisibilityNotifier *p_notifier, const AABB &p_rect) {

        ERR_FAIL_COND(notifiers.contains(p_notifier))
        notifiers[p_notifier].aabb = p_rect;
        notifiers[p_notifier].id = octree.create(p_notifier, p_rect);
        changed = true;
    }

    void _notifier_update(VisibilityNotifier *p_notifier, const AABB &p_rect) {

        Map<VisibilityNotifier *, NotifierData>::iterator E = notifiers.find(p_notifier);
        ERR_FAIL_COND(E==notifiers.end())
        if (E->second.aabb == p_rect)
            return;

        E->second.aabb = p_rect;
        octree.move(E->second.id, E->second.aabb);
        changed = true;
    }

    void _notifier_remove(VisibilityNotifier *p_notifier) {

        Map<VisibilityNotifier *, NotifierData>::iterator E = notifiers.find(p_notifier);
        ERR_FAIL_COND(E!=notifiers.end())

        octree.erase(E->second.id);
        notifiers.erase(p_notifier);

        List<Camera *> removed;
        for (eastl::pair< Camera *const,CameraData> &F : cameras) {

            Map<VisibilityNotifier *, uint64_t>::iterator G = F.second.notifiers.find(p_notifier);

            if (G!=F.second.notifiers.end()) {
                F.second.notifiers.erase(G);
                removed.push_back(F.first);
            }
        }

        while (!removed.empty()) {

            p_notifier->_exit_camera(removed.front()->deref());
            removed.pop_front();
        }

        changed = true;
    }

    void _add_camera(Camera *p_camera) {

        ERR_FAIL_COND(cameras.contains(p_camera))
        CameraData vd;
        cameras[p_camera] = vd;
        changed = true;
    }

    void _update_camera(Camera *p_camera) {

        Map<Camera *, CameraData>::iterator E = cameras.find(p_camera);
        ERR_FAIL_COND(E==cameras.end())
        changed = true;
    }

    void _remove_camera(Camera *p_camera) {
        ERR_FAIL_COND(!cameras.contains(p_camera))
        List<VisibilityNotifier *> removed;
        for (auto &E : cameras[p_camera].notifiers) {

            removed.push_back(E.first);
        }

        while (!removed.empty()) {
            removed.front()->deref()->_exit_camera(p_camera);
            removed.pop_front();
        }

        cameras.erase(p_camera);
    }

    void _update(uint64_t p_frame) {

        if (p_frame == last_frame)
            return;
        last_frame = p_frame;

        if (!changed)
            return;

        for (eastl::pair<Camera *const,CameraData> &E : cameras) {

            pass++;

            Camera *c = E.first;

            Vector<Plane> planes = c->get_frustum();

            int culled = octree.cull_convex(planes, cull.ptrw(), cull.size());

            VisibilityNotifier **ptr = cull.ptrw();

            List<VisibilityNotifier *> added;
            List<VisibilityNotifier *> removed;

            for (int i = 0; i < culled; i++) {

                //notifiers in frustum

                Map<VisibilityNotifier *, uint64_t>::iterator H = E.second.notifiers.find(ptr[i]);
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

            while (!added.empty()) {
                added.front()->deref()->_enter_camera(E.first);
                added.pop_front();
            }

            while (!removed.empty()) {
                E.second.notifiers.erase(removed.front()->deref());
                removed.front()->deref()->_exit_camera(E.first);
                removed.pop_front();
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

void World::_register_camera(Camera *p_camera) {

#ifndef _3D_DISABLED
    indexer->_add_camera(p_camera);
#endif
}

void World::_update_camera(Camera *p_camera) {

#ifndef _3D_DISABLED
    indexer->_update_camera(p_camera);
#endif
}
void World::_remove_camera(Camera *p_camera) {

#ifndef _3D_DISABLED
    indexer->_remove_camera(p_camera);
#endif
}

void World::_register_notifier(VisibilityNotifier *p_notifier, const AABB &p_rect) {

#ifndef _3D_DISABLED
    indexer->_notifier_add(p_notifier, p_rect);
#endif
}

void World::_update_notifier(VisibilityNotifier *p_notifier, const AABB &p_rect) {

#ifndef _3D_DISABLED
    indexer->_notifier_update(p_notifier, p_rect);
#endif
}

void World::_remove_notifier(VisibilityNotifier *p_notifier) {

#ifndef _3D_DISABLED
    indexer->_notifier_remove(p_notifier);
#endif
}

void World::_update(uint64_t p_frame) {

#ifndef _3D_DISABLED
    indexer->_update(p_frame);
#endif
}

RID World::get_space() const {

    return space;
}
RID World::get_scenario() const {

    return scenario;
}

void World::set_environment(const Ref<Environment> &p_environment) {

    environment = p_environment;
    VisualServer::get_singleton()->scenario_set_environment(scenario, environment ? environment->get_rid() : RID());
}

Ref<Environment> World::get_environment() const {

    return environment;
}

void World::set_fallback_environment(const Ref<Environment> &p_environment) {

    fallback_environment = p_environment;
    VisualServer::get_singleton()->scenario_set_fallback_environment(scenario, fallback_environment ? fallback_environment->get_rid() : RID());
}

Ref<Environment> World::get_fallback_environment() const {

    return fallback_environment;
}

PhysicsDirectSpaceState *World::get_direct_space_state() {

    return PhysicsServer::get_singleton()->space_get_direct_state(space);
}

void World::get_camera_list(List<Camera *> *r_cameras) {

    for (const eastl::pair<Camera *const,SpatialIndexer::CameraData> &E : indexer->cameras) {
        r_cameras->push_back(E.first);
    }
}

void World::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("get_space"), &World::get_space);
    MethodBinder::bind_method(D_METHOD("get_scenario"), &World::get_scenario);
    MethodBinder::bind_method(D_METHOD("set_environment", {"env"}), &World::set_environment);
    MethodBinder::bind_method(D_METHOD("get_environment"), &World::get_environment);
    MethodBinder::bind_method(D_METHOD("set_fallback_environment", {"env"}), &World::set_fallback_environment);
    MethodBinder::bind_method(D_METHOD("get_fallback_environment"), &World::get_fallback_environment);
    MethodBinder::bind_method(D_METHOD("get_direct_space_state"), &World::get_direct_space_state);
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "environment", PROPERTY_HINT_RESOURCE_TYPE, "Environment"), "set_environment", "get_environment");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "fallback_environment", PROPERTY_HINT_RESOURCE_TYPE, "Environment"), "set_fallback_environment", "get_fallback_environment");
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "space", PROPERTY_HINT_NONE, "", 0), "", "get_space");
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "scenario", PROPERTY_HINT_NONE, "", 0), "", "get_scenario");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "direct_space_state", PROPERTY_HINT_RESOURCE_TYPE, "PhysicsDirectSpaceState", 0), "", "get_direct_space_state");
}

World::World() {

    space = PhysicsServer::get_singleton()->space_create();
    scenario = VisualServer::get_singleton()->scenario_create();

    PhysicsServer::get_singleton()->space_set_active(space, true);
    PhysicsServer::get_singleton()->area_set_param(space, PhysicsServer::AREA_PARAM_GRAVITY, GLOBAL_DEF("physics/3d/default_gravity", 9.8));
    PhysicsServer::get_singleton()->area_set_param(space, PhysicsServer::AREA_PARAM_GRAVITY_VECTOR, GLOBAL_DEF("physics/3d/default_gravity_vector", Vector3(0, -1, 0)));
    PhysicsServer::get_singleton()->area_set_param(space, PhysicsServer::AREA_PARAM_LINEAR_DAMP, GLOBAL_DEF("physics/3d/default_linear_damp", 0.1));
    PhysicsServer::get_singleton()->area_set_param(space, PhysicsServer::AREA_PARAM_ANGULAR_DAMP, GLOBAL_DEF("physics/3d/default_angular_damp", 0.1));

#ifdef _3D_DISABLED
    indexer = NULL;
#else
    indexer = memnew(SpatialIndexer);
#endif
}

World::~World() {

    PhysicsServer::get_singleton()->free(space);
    VisualServer::get_singleton()->free(scenario);

#ifndef _3D_DISABLED
    memdelete(indexer);
#endif
}
