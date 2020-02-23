#pragma once

#include "core/os/memory.h"

#include "entt/entity/registry.hpp"

class ECS_Registry {
public:
	entt::basic_registry<entt::entity,wrap_allocator> registry;
};
