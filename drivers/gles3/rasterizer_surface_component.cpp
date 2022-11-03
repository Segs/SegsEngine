#include "rasterizer_surface_component.h"

#include "rasterizer_dependent_entities_component.h"
#include "rasterizer_mesh_component.h"
#include "rasterizer_storage_gles3.h"


void RasterizerSurfaceComponent::material_changed_notify() {
    VSG::ecs->registry.get<RasterizerInstantiableComponent>(mesh).instance_change_notify(false, true);
    VSG::ecs->registry.get<RasterizerMeshComponent>(mesh).update_multimeshes();
}

RasterizerSurfaceComponent::RasterizerSurfaceComponent() = default;
