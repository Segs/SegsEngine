#include "rasterizer_common_geometry_component.h"

RasterizerCommonGeometryComponent::~RasterizerCommonGeometryComponent()
{
    assert(material==entt::null);
}
