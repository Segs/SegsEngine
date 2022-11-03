#pragma once

#include "core/ecs_registry.h"
#include "servers/rendering/rendering_server_globals.h"

template<typename T>
T *get(RenderingEntity re) {
    return re!=entt::null ? VSG::ecs->try_get<T>(re) : nullptr;
}

template<typename T>
T *getUnchecked(RenderingEntity re) {
    return &VSG::ecs->registry.get<T>(re);
}
