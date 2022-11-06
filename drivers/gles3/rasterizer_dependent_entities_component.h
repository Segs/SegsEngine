#pragma once
#include "core/engine_entities.h"
#include "core/deque.h"

struct RasterizerInstantiableComponent {
    Dequeue<RenderingEntity> instance_list;
    void instance_change_notify(bool p_aabb, bool p_materials);
    void instance_remove_deps() noexcept;

    RasterizerInstantiableComponent(const RasterizerInstantiableComponent&) = delete;
    RasterizerInstantiableComponent &operator=(const RasterizerInstantiableComponent&) = delete;

    RasterizerInstantiableComponent(RasterizerInstantiableComponent&&from) noexcept {
        *this = eastl::move(from);
    }
    RasterizerInstantiableComponent &operator=(RasterizerInstantiableComponent&&from) noexcept {
        instance_remove_deps();
        instance_list = eastl::move(from.instance_list);
        return *this;
    }

    RasterizerInstantiableComponent() {}
    ~RasterizerInstantiableComponent();
};
