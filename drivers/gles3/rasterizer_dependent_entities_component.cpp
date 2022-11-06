#include "rasterizer_dependent_entities_component.h"

#include "rasterizer_storage_gles3.h"
#include "servers/rendering/render_entity_getter.h"

void RasterizerInstantiableComponent::instance_change_notify(bool p_aabb, bool p_materials) {
    for (RenderingEntity re : instance_list) {
        //verify the entity has zero or one renderer instance component that is bound to itself
        assert(!VSG::ecs->registry.all_of<RenderingInstanceComponent>(re) ||
               get<RenderingInstanceComponent>(re)->self == re
               );
        auto *ib = getUnchecked<RenderingInstanceComponent>(re);
        ib->base_changed(p_aabb, p_materials);
    }
}


void RasterizerInstantiableComponent::instance_remove_deps() noexcept {
    for(RenderingEntity b : instance_list) {
        assert(VSG::ecs->registry.valid(b)); // must be a valid entity, otherwise it should have removed itself from our instances.
        RenderingInstanceComponent *id = VSG::ecs->registry.try_get<RenderingInstanceComponent>(b);
        if(id)
            id->base_removed();
    }
    instance_list.clear();

}

RasterizerInstantiableComponent::~RasterizerInstantiableComponent() {
    instance_remove_deps();
}
