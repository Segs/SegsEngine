#pragma once

#include "rasterizer_gl_unique_handle.h"
#include <cstdint>

struct RasterizerCanvasLightShadowComponent  {

    int size;
    int height;
    GLFBOHandle fbo;
    GLRenderBufferHandle depth;
    GLTextureHandle distance; //for older devices

    RasterizerCanvasLightShadowComponent(const RasterizerCanvasLightShadowComponent &)=delete;
    RasterizerCanvasLightShadowComponent &operator=(const RasterizerCanvasLightShadowComponent &)=delete;

    RasterizerCanvasLightShadowComponent(RasterizerCanvasLightShadowComponent &&from)=default;
    RasterizerCanvasLightShadowComponent &operator=(RasterizerCanvasLightShadowComponent &&from)=default;

    RasterizerCanvasLightShadowComponent() = default;
};
