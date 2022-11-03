/*************************************************************************/
/*  rendering_server_globals.h                                              */
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

#include "rasterizer.h"
#include "core/ecs_registry.h"
#include "core/engine_entities.h"

class RenderingServerCanvas;
class VisualServerViewport;
class VisualServerScene;
template<class Entity, bool multi_threaded>
class ECS_Registry;

class VisualServerGlobals {
public:
    static RasterizerStorage *storage;
    static RasterizerCanvas *canvas_render;
    static RasterizerScene *scene_render;
    static Rasterizer *rasterizer;
    static ECS_Registry<RenderingEntity,true> *ecs;

    static RenderingServerCanvas *canvas;
    static VisualServerViewport *viewport;
    static VisualServerScene *scene;

    static int64_t bvh_nodes_created;
    static int64_t bvh_nodes_destroyed;
};

#define VSG VisualServerGlobals
template <class T>
class RenderingEntity_Owner {
public:
    T *get(RenderingEntity p_rid) {
#ifdef DEBUG_ENABLED
        ERR_FAIL_COND_V(p_rid==entt::null, nullptr);
        ERR_FAIL_COND_V(!VSG::ecs->registry.all_of<T>(p_rid), nullptr);
#endif
        return VSG::ecs->try_get<T>(p_rid);
    }
    bool owns(RenderingEntity re) {
        return re != entt::null && VSG::ecs->registry.all_of<T>(re);
    }
    _FORCE_INLINE_ T *getornull(RenderingEntity p_rid) {
        if (p_rid == entt::null)
            return nullptr;
#ifdef DEBUG_ENABLED
        ERR_FAIL_COND_V(!VSG::ecs->registry.all_of<T>(p_rid), nullptr);
#endif
        return VSG::ecs->try_get<T>(p_rid);
    }

    T *getptr(RenderingEntity p_rid) { return VSG::ecs->try_get<T>(p_rid); }
    void get_owned_list(Vector<RenderingEntity> *p_owned) {
#ifdef DEBUG_ENABLED
        VSG::ecs->registry.view<T>().each([&](RenderingEntity re, const T &v) {
            { p_owned->push_back(re); }
        });
#endif
    }
    void free(RenderingEntity re) {
        VSG::ecs->registry.destroy(re);
    }
};
