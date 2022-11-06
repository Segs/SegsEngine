#include "rasterizer_render_target_component.h"

#include "rasterizer_storage_gles3.h"
#include "rasterizer_texture_component.h"

#include "core/string_utils.h"

#include "entt/entity/helper.hpp"

#include "glad/glad.h"

/* RENDER TARGET */
static void _render_target_allocate(RenderingEntity self, RasterizerRenderTargetComponent *rt,
        bool framebuffer_float_supported, bool framebuffer_half_float_supported,bool use_anisotropic,bool use_fast_texture_filter,int anisotropic_level,bool srgb_decode_supported);

void _render_target_clear(RenderingEntity self,RasterizerRenderTargetComponent *rt) {

    rt->fbo.release();
    rt->color.release();

    if (rt->buffers.active) {
        rt->buffers.fbo.release();
        rt->buffers.depth.release();
        rt->buffers.diffuse.release();
        if (rt->buffers.effects_active) {
            rt->buffers.specular.release();
            rt->buffers.normal_rough.release();
            rt->buffers.sss.release();
            rt->buffers.effect_fbo.release();
            rt->buffers.effect.release();
        }

        rt->buffers.effects_active = false;
        rt->buffers.active = false;
    }

    rt->depth.release();

    if (rt->effects.ssao.blur_fbo[0]) {
        rt->effects.ssao.blur_fbo.release();
        rt->effects.ssao.blur_red.release();
        for (int i = 0; i < rt->effects.ssao.depth_mipmap_fbos.size(); i++) {
            rt->effects.ssao.depth_mipmap_fbos[i].release();
        }

        rt->effects.ssao.depth_mipmap_fbos.clear();

        rt->effects.ssao.linear_depth.release();

        rt->effects.ssao.blur_fbo[0] = 0;
        rt->effects.ssao.blur_fbo[1] = 0;
    }

    rt->exposure.fbo.release();
    rt->exposure.color.release();

    // clean up our texture
    auto * tex = get<RasterizerTextureComponent>(self);
    if(tex) { // this might be null if `self` is getting destroyed
    tex->alloc_height = 0;
    tex->alloc_width = 0;
    tex->width = 0;
    tex->height = 0;
    tex->active = false;
    }
    if (rt->external.fbo.is_initialized()) {
        // free this
        rt->external.fbo.release();
        // reset our texture back to the original
        tex->external_tex_id = GLNonOwningHandle(rt->color);

        rt->external.color = {0};
        rt->external.depth = {0};
    }

    for (int i = 0; i < 2; i++) {
        if (rt->effects.mip_maps[i].color.is_initialized()) {
            for (int j = 0; j < rt->effects.mip_maps[i].sizes.size(); j++) {
                rt->effects.mip_maps[i].sizes[j].fbo.release();
            }

            rt->effects.mip_maps[i].color.release();
            rt->effects.mip_maps[i].sizes.clear();
            rt->effects.mip_maps[i].levels = 0;
        }
    }

    /*
    if (rt->effects.screen_space_depth) {
        glDeleteTextures(1,&rt->effects.screen_space_depth);
        rt->effects.screen_space_depth=0;

}
*/
}

void _render_target_allocate(RenderingEntity self, RasterizerRenderTargetComponent *rt,
        bool framebuffer_float_supported, bool framebuffer_half_float_supported,bool use_anisotropic,bool use_fast_texture_filter,int anisotropic_level,bool srgb_decode_supported) {

    if (rt->width <= 0 || rt->height <= 0) {
        return;
    }

    GLuint color_internal_format;
    GLuint color_format;
    GLuint color_type;
    Image::Format image_format;

    const bool hdr = rt->flags[RS::RENDER_TARGET_HDR] && framebuffer_half_float_supported;
    //hdr = false;

    if (!hdr || rt->flags[RS::RENDER_TARGET_NO_3D]) {

        if (rt->flags[RS::RENDER_TARGET_NO_3D_EFFECTS] && !rt->flags[RS::RENDER_TARGET_TRANSPARENT]) {
            //if this is not used, linear colorspace looks pretty bad
            //this is the default mode used for mobile
            color_internal_format = GL_RGB10_A2;
            color_format = GL_RGBA;
            color_type = GL_UNSIGNED_INT_2_10_10_10_REV;
            image_format = ImageData::FORMAT_RGBA8;
        } else {

            color_internal_format = GL_RGBA8;
            color_format = GL_RGBA;
            color_type = GL_UNSIGNED_BYTE;
            image_format = ImageData::FORMAT_RGBA8;
        }
    } else {
        // HDR enabled.
        if (rt->flags[RS::RENDER_TARGET_USE_32_BPC_DEPTH]) {
            // 32 bpc. Can be useful for advanced shaders, but should not be used
            // for general-purpose rendering as it's slower.
            color_internal_format = GL_RGBA32F;
            color_format = GL_RGBA;
            color_type = GL_FLOAT;
            image_format = ImageData::FORMAT_RGBAF;
        } else {
                // 16 bpc. This is the default HDR mode.
            color_internal_format = GL_RGBA16F;
            color_format = GL_RGBA;
            color_type = GL_HALF_FLOAT;
            image_format = ImageData::FORMAT_RGBAH;
        }
    }

    {
        /* FRONT FBO */

        glActiveTexture(GL_TEXTURE0);

        rt->fbo.create();
        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

        rt->depth.create();
        glBindTexture(GL_TEXTURE_2D, rt->depth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, rt->width, rt->height, 0,
                GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_2D, rt->depth, 0);

        if (!rt->external.depth.is_initialized()) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                    GL_TEXTURE_2D, rt->depth, 0);
        } else {
            // Use our external depth texture instead.
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                    GL_TEXTURE_2D, rt->external.depth, 0);
        }
        rt->color.create();
        glBindTexture(GL_TEXTURE_2D, rt->color);

        glTexImage2D(GL_TEXTURE_2D, 0, color_internal_format, rt->width, rt->height, 0, color_format, color_type, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->color, 0);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);

        if (status != GL_FRAMEBUFFER_COMPLETE) {
            printf("framebuffer fail, status: %x\n", status);
        }

        ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);

        auto &tex = VSG::ecs->registry.get<RasterizerTextureComponent>(self);
        tex.format = image_format;
        tex.gl_format_cache = color_format;
        tex.gl_type_cache = color_type;
        tex.gl_internal_format_cache = color_internal_format;
        tex.external_tex_id = GLNonOwningHandle(rt->color);
        tex.width = rt->width;
        tex.alloc_width = rt->width;
        tex.height = rt->height;
        tex.alloc_height = rt->height;
        tex.active = true;

        ::texture_set_flags(&tex, tex.flags, use_anisotropic, use_fast_texture_filter,
                          anisotropic_level, srgb_decode_supported);
    }

    /* BACK FBO */

    if (!rt->flags[RS::RENDER_TARGET_NO_3D] &&
            (!rt->flags[RS::RENDER_TARGET_NO_3D_EFFECTS] || rt->msaa != RS::VIEWPORT_MSAA_DISABLED)) {

        rt->buffers.active = true;

        static const int msaa_value[] = { 0, 2, 4, 8, 16, 4, 16 }; // MSAA_EXT_nX is a GLES2 temporary hack ignored in GLES3 for now...
        int msaa = msaa_value[rt->msaa];

        int max_samples = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
        if (msaa > max_samples) {
            WARN_PRINT("MSAA must be <= GL_MAX_SAMPLES, falling-back to GL_MAX_SAMPLES = " + itos(max_samples));
            msaa = max_samples;
        }

             //regular fbo
        rt->buffers.fbo.create();
        glBindFramebuffer(GL_FRAMEBUFFER, rt->buffers.fbo);

        rt->buffers.depth.create();
        glBindRenderbuffer(GL_RENDERBUFFER, rt->buffers.depth);
        if (msaa == 0) {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, rt->width, rt->height);
        } else {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_DEPTH_COMPONENT24, rt->width, rt->height);
        }

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->buffers.depth);

        rt->buffers.diffuse.create();
        glBindRenderbuffer(GL_RENDERBUFFER, rt->buffers.diffuse);

        if (msaa == 0) {
            glRenderbufferStorage(GL_RENDERBUFFER, color_internal_format, rt->width, rt->height);
        } else {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, color_internal_format, rt->width, rt->height);
        }

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rt->buffers.diffuse);

        if (!rt->flags[RS::RENDER_TARGET_NO_3D_EFFECTS]) {

            rt->buffers.effects_active = true;
            rt->buffers.specular.create();
            glBindRenderbuffer(GL_RENDERBUFFER, rt->buffers.specular);

            if (msaa == 0) {
                glRenderbufferStorage(GL_RENDERBUFFER, color_internal_format, rt->width, rt->height);
            } else {
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, color_internal_format, rt->width, rt->height);
            }

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, rt->buffers.specular);

            rt->buffers.normal_rough.create();
            glBindRenderbuffer(GL_RENDERBUFFER, rt->buffers.normal_rough);

            if (msaa == 0) {
                glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, rt->width, rt->height);
            } else {
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_RGBA8, rt->width, rt->height);
            }

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_RENDERBUFFER, rt->buffers.normal_rough);

            rt->buffers.sss.create();
            glBindRenderbuffer(GL_RENDERBUFFER, rt->buffers.sss);

            if (msaa == 0) {
                glRenderbufferStorage(GL_RENDERBUFFER, GL_R8, rt->width, rt->height);
            } else {
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_R8, rt->width, rt->height);
            }

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_RENDERBUFFER, rt->buffers.sss);

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);

            if (status != GL_FRAMEBUFFER_COMPLETE) {
                printf("err status: %x\n", status);
                _render_target_clear(self,rt);
                ERR_FAIL_MSG("status != GL_FRAMEBUFFER_COMPLETE");
            }

            glBindRenderbuffer(GL_RENDERBUFFER, 0);

                 // effect resolver

            rt->buffers.effect_fbo.create();
            glBindFramebuffer(GL_FRAMEBUFFER, rt->buffers.effect_fbo);

            rt->buffers.effect.create();
            glBindTexture(GL_TEXTURE_2D, rt->buffers.effect);
            glTexImage2D(GL_TEXTURE_2D, 0, color_internal_format, rt->width, rt->height, 0,
                    color_format, color_type, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_2D, rt->buffers.effect, 0);

            status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);

            if (status != GL_FRAMEBUFFER_COMPLETE) {
                printf("err status: %x\n", status);
                _render_target_clear(self,rt);
                ERR_FAIL_MSG("status != GL_FRAMEBUFFER_COMPLETE");
            }

                 ///////////////// ssao

                 //AO strength textures
            rt->effects.ssao.blur_fbo.create();
            rt->effects.ssao.blur_red.create();
            for (int i = 0; i < 2; i++) {

                glBindFramebuffer(GL_FRAMEBUFFER, rt->effects.ssao.blur_fbo[i]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                        GL_TEXTURE_2D, rt->depth, 0);

                glBindTexture(GL_TEXTURE_2D, rt->effects.ssao.blur_red[i]);

                glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, rt->width, rt->height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->effects.ssao.blur_red[i], 0);

                status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                if (status != GL_FRAMEBUFFER_COMPLETE) {
                    _render_target_clear(self,rt);
                    ERR_FAIL_MSG("status != GL_FRAMEBUFFER_COMPLETE");
                }
            }
            //5 mip levels for depth texture, but base is read separately

            rt->effects.ssao.linear_depth.create();
            glBindTexture(GL_TEXTURE_2D, rt->effects.ssao.linear_depth);

            int ssao_w = rt->width / 2;
            int ssao_h = rt->height / 2;

            for (int i = 0; i < 4; i++) { //5, but 4 mips, base is read directly to save bw

                glTexImage2D(GL_TEXTURE_2D, i, GL_R16UI, ssao_w, ssao_h, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, nullptr);
                ssao_w >>= 1;
                ssao_h >>= 1;
            }

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);

            for (int i = 0; i < 4; i++) { //5, but 4 mips, base is read directly to save bw

                GLFBOHandle fbo;
                fbo.create();
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->effects.ssao.linear_depth, i);
                rt->effects.ssao.depth_mipmap_fbos.emplace_back(eastl::move(fbo));
            }

                 //////Exposure

            rt->exposure.fbo.create();
            glBindFramebuffer(GL_FRAMEBUFFER, rt->exposure.fbo);

            rt->exposure.color.create();
            glBindTexture(GL_TEXTURE_2D, rt->exposure.color);
            if (framebuffer_float_supported) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, nullptr);
            } else if (framebuffer_half_float_supported) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, 1, 1, 0, GL_RED, GL_HALF_FLOAT, nullptr);
            } else {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, 1, 1, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullptr);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->exposure.color, 0);

            status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                _render_target_clear(self,rt);
                ERR_FAIL_MSG("status != GL_FRAMEBUFFER_COMPLETE");
            }
        } else {
            rt->buffers.effects_active = false;
        }
    } else {
        rt->buffers.active = false;
        rt->buffers.effects_active = true;
    }

    if (!rt->flags[RS::RENDER_TARGET_NO_SAMPLING] && rt->width >= 2 && rt->height >= 2) {

        for (int i = 0; i < 2; i++) {

            ERR_FAIL_COND(rt->effects.mip_maps[i].sizes.size());
            int w = rt->width;
            int h = rt->height;

            if (i > 0) {
                w >>= 1;
                h >>= 1;
            }

            rt->effects.mip_maps[i].color.create();
            glBindTexture(GL_TEXTURE_2D, rt->effects.mip_maps[i].color);

            int level = 0;
            int fb_w = w;
            int fb_h = h;

            while (true) {

                RasterizerRenderTargetComponent::Effects::MipMaps::Size mm;
                mm.width = w;
                mm.height = h;
                rt->effects.mip_maps[i].sizes.emplace_back(eastl::move(mm));

                w >>= 1;
                h >>= 1;

                if (w < 2 || h < 2) {
                    break;
                }

                level++;
            }

            glTexStorage2DCustom(GL_TEXTURE_2D, level + 1, color_internal_format, fb_w, fb_h, color_format, color_type);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level);
            glDisable(GL_SCISSOR_TEST);
            glColorMask(1, 1, 1, 1);
            if (!rt->buffers.active) {
                glDepthMask(GL_TRUE);
            }

            for (int j = 0; j < rt->effects.mip_maps[i].sizes.size(); j++) {

                RasterizerRenderTargetComponent::Effects::MipMaps::Size &mm = rt->effects.mip_maps[i].sizes[j];

                mm.fbo.create();
                glBindFramebuffer(GL_FRAMEBUFFER, mm.fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->effects.mip_maps[i].color, j);
                bool used_depth = false;
                if (j == 0 && i == 0) { //use always
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->depth, 0);
                    used_depth = true;
                }

                GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                if (status != GL_FRAMEBUFFER_COMPLETE) {
                    _render_target_clear(self,rt);
                    ERR_FAIL_MSG("status != GL_FRAMEBUFFER_COMPLETE");
                }

                float zero[4] = { 1, 0, 1, 0 };
                glViewport(0, 0, rt->effects.mip_maps[i].sizes[j].width, rt->effects.mip_maps[i].sizes[j].height);
                glClearBufferfv(GL_COLOR, 0, zero);
                if (used_depth) {
                    glClearDepth(1.0);
                    glClear(GL_DEPTH_BUFFER_BIT);
                }
            }

            glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
            rt->effects.mip_maps[i].levels = level;

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
}

RenderingEntity RasterizerStorageGLES3::render_target_create() {
    RenderingEntity res = VSG::ecs->create();
    auto &rt = VSG::ecs->registry.emplace<RasterizerRenderTargetComponent>(res);
    auto &t = VSG::ecs->registry.emplace<RasterizerTextureComponent>(res);
    rt.self = res;

    t.self = res;
    t.type = RS::TEXTURE_TYPE_2D;
    t.flags = 0;
    t.width = 0;
    t.height = 0;
    t.alloc_height = 0;
    t.alloc_width = 0;
    t.format = ImageData::FORMAT_R8;
    t.target = GL_TEXTURE_2D;
    t.gl_format_cache = 0;
    t.gl_internal_format_cache = 0;
    t.gl_type_cache = 0;
    t.data_size = 0;
    t.compressed = false;
    t.srgb = false;
    t.total_data_size = 0;
    t.ignore_mipmaps = false;
    t.mipmaps = 1;
    t.active = true;
    t.render_target = res;


    return res;
}

void RasterizerStorageGLES3::render_target_set_size(RenderingEntity p_render_target, int p_width, int p_height) {

    auto *rt = get<RasterizerRenderTargetComponent>(p_render_target);
    ERR_FAIL_COND(!rt);

    if (rt->width == p_width && rt->height == p_height) {
        return;
   } 

    _render_target_clear(p_render_target, rt);
    rt->width = p_width;
    rt->height = p_height;
    _render_target_allocate(p_render_target, rt, config.framebuffer_float_supported,
            config.framebuffer_half_float_supported, config.use_anisotropic_filter, config.use_fast_texture_filter,
            config.anisotropic_level, config.srgb_decode_supported);
}

RenderingEntity RasterizerStorageGLES3::render_target_get_texture(RenderingEntity p_render_target) const {

    auto *rt = get<RasterizerTextureComponent>(p_render_target);
    if(rt) {
        return p_render_target;
    }
    return entt::null;
}
uint32_t RasterizerStorageGLES3::render_target_get_depth_texture_id(RenderingEntity p_render_target) const {

    RasterizerRenderTargetComponent *rt = get<RasterizerRenderTargetComponent>(p_render_target);
    ERR_FAIL_COND_V(!rt,0);

    if (!rt->external.depth.is_initialized()) {
        return rt->depth;
    } else {
        return rt->external.depth;
    }
}


void RasterizerStorageGLES3::render_target_set_external_texture(RenderingEntity p_render_target, unsigned int p_texture_id, unsigned int p_depth_id) {
    RasterizerRenderTargetComponent *rt = get<RasterizerRenderTargetComponent>(p_render_target);
    ERR_FAIL_COND(!rt);

    if (p_texture_id == 0) {
        if (rt->external.fbo.is_initialized()) {
            // return to our original depth buffer
            if (rt->external.depth.is_initialized() && rt->fbo.is_initialized()) {
                glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->depth, 0);
                glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
            }
            RasterizerTextureComponent *t = VSG::ecs->try_get<RasterizerTextureComponent>(p_render_target);
            assert(t);
            // reset our texture back to the original
            t->external_tex_id = GLNonOwningHandle(rt->color);
            t->width = rt->width;
            t->alloc_width = rt->width;
            t->height = rt->height;
            t->alloc_height = rt->height;
            // free this
            rt->external.fbo.release();

            rt->external.color = {0};
            rt->external.depth = {0};

        }
    } else {

        if (!rt->external.fbo.is_initialized()) {
            // create our fbo
            rt->external.fbo.create();
        }
            glBindFramebuffer(GL_FRAMEBUFFER, rt->external.fbo);

        // set our texture, but we don't own it ( will not delete on texture object destruction )
        rt->external.color = GLNonOwningHandle(p_texture_id);
        // Set our texture to the new image, note that we expect formats to be the same (or compatible) so we don't change those
        RasterizerTextureComponent *t = get<RasterizerTextureComponent>(p_render_target);
        t->external_tex_id = GLNonOwningHandle(p_texture_id);

             // size shouldn't be different
        t->width = rt->width;
        t->height = rt->height;
        t->alloc_height = rt->width;
        t->alloc_width = rt->height;


             // set our texture as the destination for our framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_texture_id, 0);

        // check status
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        if (status != GL_FRAMEBUFFER_COMPLETE) {
            printf("framebuffer fail, status: %x\n", status);
        }
        // Copy our depth texture id,
        // if it's 0 then we don't use it,
        // else we use it instead of our normal depth buffer
        rt->external.depth = GLNonOwningHandle(p_depth_id);

        if (rt->external.depth.is_initialized() && rt->fbo.is_initialized()) {
            // Use our external depth texture instead.
            glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->external.depth, 0);

            // check status
            GLenum status2 = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status2 != GL_FRAMEBUFFER_COMPLETE) {
                printf("framebuffer fail, status: %x\n", status2);
            }
        }

        // and unbind
        glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);

        ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
    }
}

void RasterizerStorageGLES3::render_target_set_flag(RenderingEntity p_render_target, RS::RenderTargetFlags p_flag, bool p_value) {

    auto *rt = VSG::ecs->try_get<RasterizerRenderTargetComponent>(p_render_target);
    ERR_FAIL_COND(!rt);

    rt->flags[p_flag] = p_value;

    switch (p_flag) {
        case RS::RENDER_TARGET_HDR:
        case RS::RENDER_TARGET_USE_32_BPC_DEPTH:
        case RS::RENDER_TARGET_NO_3D:
        case RS::RENDER_TARGET_NO_SAMPLING:
        case RS::RENDER_TARGET_NO_3D_EFFECTS: {
            //must reset for these formats
            _render_target_clear(p_render_target,rt);
            _render_target_allocate(p_render_target, rt, config.framebuffer_float_supported,
                    config.framebuffer_half_float_supported, config.use_anisotropic_filter, config.use_fast_texture_filter,
                    config.anisotropic_level, config.srgb_decode_supported);

        } break;
        default: {
        }
    }
}
bool RasterizerStorageGLES3::render_target_was_used(RenderingEntity p_render_target) {

    auto *rt = VSG::ecs->try_get<RasterizerRenderTargetComponent>(p_render_target);
    ERR_FAIL_COND_V(!rt, false);

    return rt->used_in_frame;
}

void RasterizerStorageGLES3::render_target_clear_used(RenderingEntity p_render_target) {

    auto *rt = VSG::ecs->try_get<RasterizerRenderTargetComponent>(p_render_target);
    ERR_FAIL_COND(!rt);

    rt->used_in_frame = false;
}

void RasterizerStorageGLES3::render_target_set_msaa(RenderingEntity p_render_target, RS::ViewportMSAA p_msaa) {

    auto *rt = VSG::ecs->try_get<RasterizerRenderTargetComponent>(p_render_target);
    ERR_FAIL_COND(!rt);

    if (rt->msaa == p_msaa) {
        return;
    }

    _render_target_clear(p_render_target,rt);
    rt->msaa = p_msaa;
    _render_target_allocate(p_render_target, rt, config.framebuffer_float_supported,
            config.framebuffer_half_float_supported, config.use_anisotropic_filter, config.use_fast_texture_filter,
            config.anisotropic_level, config.srgb_decode_supported);}

void RasterizerStorageGLES3::render_target_set_use_fxaa(RenderingEntity p_render_target, bool p_fxaa) {

    RasterizerRenderTargetComponent *rt = get<RasterizerRenderTargetComponent>(p_render_target);
    ERR_FAIL_COND(!rt);

    rt->use_fxaa = p_fxaa;
}

void RasterizerStorageGLES3::render_target_set_use_debanding(RenderingEntity p_render_target, bool p_debanding) {

    RasterizerRenderTargetComponent *rt = get<RasterizerRenderTargetComponent>(p_render_target);
    ERR_FAIL_COND(!rt);

    rt->use_debanding = p_debanding;
}

void RasterizerStorageGLES3::render_target_set_sharpen_intensity(RenderingEntity p_render_target, float p_intensity) {
    auto *rt = VSG::ecs->try_get<RasterizerRenderTargetComponent>(p_render_target);
    ERR_FAIL_COND(!rt);

    rt->sharpen_intensity = p_intensity;
}

RasterizerRenderTargetComponent &RasterizerRenderTargetComponent::operator=(RasterizerRenderTargetComponent &&from)
{
    if(self!=entt::null) // not moved from ?
        _render_target_clear(self, this);

    buffers = eastl::move(from.buffers);
    effects = eastl::move(from.effects);
    exposure = eastl::move(from.exposure);
    external = eastl::move(from.external);

    self = eastl::move(from.self);
    fbo = eastl::move(from.fbo);
    color = eastl::move(from.color);
    depth = eastl::move(from.depth);

    last_exposure_tick = from.last_exposure_tick;

    width = from.width;
    height = from.height;

    if (this != &from) // prevent memcpy with same addresses
        memcpy(flags,from.flags,sizeof(bool)*RS::RENDER_TARGET_FLAG_MAX);
    // Texture component is composed-in during render target construction.
    //RenderingEntity texture;
    msaa = from.msaa;
    used_in_frame=from.used_in_frame;
    use_fxaa=from.use_fxaa;
    use_debanding=from.use_debanding;
    return *this;
}

RasterizerRenderTargetComponent::~RasterizerRenderTargetComponent() {
    _render_target_clear(self,this);
}
