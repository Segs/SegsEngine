#pragma once

#include "rasterizer_gl_unique_handle.h"
#include "servers/rendering_server_enums.h"
#include "servers/rendering/render_entity_helpers.h"
#include "core/engine_entities.h"
#include "core/vector.h"


struct RasterizerRenderTargetComponent  {

    struct Buffers {
        int8_t active : 1;
        int8_t effects_active : 1;

        GLFBOHandle fbo;
        GLRenderBufferHandle depth;
        GLRenderBufferHandle specular;
        GLRenderBufferHandle diffuse;
        GLRenderBufferHandle normal_rough;
        GLRenderBufferHandle sss;

        GLFBOHandle effect_fbo;
        GLTextureHandle effect;

    };



    struct Effects {

        struct MipMaps {

            struct Size {
                GLFBOHandle fbo;
                int width;
                int height;
            };

            Vector<Size> sizes;
            GLTextureHandle color;
            int levels = 0;
        };
        struct SSAO {
            GLMultiFBOHandle<2> blur_fbo; // blur fbo
            GLMultiTextureHandle<2> blur_red; // 8 bits red buffer
            GLTextureHandle linear_depth;
            Vector<GLFBOHandle> depth_mipmap_fbos; //fbos for depth mipmapsla ver
        };

        MipMaps mip_maps[2]; //first mipmap chain starts from full-screen
        //GLuint depth2; //depth for the second mipmap chain, in case of desiring upsampling

        SSAO ssao;

        Effects() {}

    };

    struct Exposure {
        GLFBOHandle fbo;
        GLTextureHandle color;

    };
    // External FBO to render our final result to (mostly used for ARVR)
    struct External {
        GLFBOHandle fbo;
        GLNonOwningHandle color;
        GLNonOwningHandle depth;
    };

    Buffers buffers;
    Effects effects;
    Exposure exposure;
    External external;

    MoveOnlyEntityHandle self;
    GLFBOHandle fbo;
    GLTextureHandle color;
    GLTextureHandle depth;

    uint64_t last_exposure_tick = 0;
    float sharpen_intensity=0.0f;
    int width = 0;
    int height = 0;

    bool flags[RS::RENDER_TARGET_FLAG_MAX];

    // Texture component is composed-in during render target construction.
    //RenderingEntity texture;
    RS::ViewportMSAA msaa = RS::VIEWPORT_MSAA_DISABLED;
    uint8_t used_in_frame:1;
    uint8_t use_fxaa:1;
    uint8_t use_debanding:1;

    RasterizerRenderTargetComponent(const RasterizerRenderTargetComponent &) = delete;
    RasterizerRenderTargetComponent &operator=(const RasterizerRenderTargetComponent &) = delete;

    RasterizerRenderTargetComponent(RasterizerRenderTargetComponent &&) = default;
    RasterizerRenderTargetComponent &operator=(RasterizerRenderTargetComponent && from);

    RasterizerRenderTargetComponent() :
            used_in_frame(false),
            use_fxaa(false),
            use_debanding(false)  {
        for (bool & flag : flags) {
            flag = false;
        }
        flags[RS::RENDER_TARGET_HDR] = true;
        buffers.active = false;
        buffers.effects_active = false;
    }
    ~RasterizerRenderTargetComponent();
};
void _render_target_clear(RenderingEntity self, RasterizerRenderTargetComponent *rt);
