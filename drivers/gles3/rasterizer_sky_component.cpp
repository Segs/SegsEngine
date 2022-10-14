#include "rasterizer_sky_component.h"

#include "rasterizer_storage_gles3.h"
#include "rasterizer_texture_component.h"
#include "core/project_settings.h"


RenderingEntity RasterizerStorageGLES3::sky_create() {
    auto res = VSG::ecs->create();
    VSG::ecs->registry.emplace<RasterizerSkyComponent>(res);
    return res;
}

void RasterizerStorageGLES3::sky_set_texture(RenderingEntity p_sky, RenderingEntity p_panorama, int p_radiance_size) {

    RasterizerSkyComponent *sky = VSG::ecs->try_get<RasterizerSkyComponent>(p_sky);
    ERR_FAIL_COND(!sky);

    sky->radiance.release();
    sky->irradiance.release();
    sky->panorama = p_panorama;

    if (sky->panorama==entt::null) { //cleared
        return;
    }

    auto* texture = VSG::ecs->try_get<RasterizerTextureComponent>(sky->panorama);
    if (!texture) {
        sky->panorama = entt::null;
        ERR_FAIL_MSG("sky's panorama texture has no RasterizerTextureComponent component.");
    }

    texture = texture->get_ptr(); //resolve for proxies

    glBindVertexArray(0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texture->target, texture->tex_id);
    glTexParameteri(texture->target, GL_TEXTURE_BASE_LEVEL, 0);

    glTexParameteri(texture->target, GL_TEXTURE_MAX_LEVEL, int(Math::floor(Math::log(float(texture->width)) / Math::log(2.0f))));
    glGenerateMipmap(texture->target);

    // Need Mipmaps regardless of whether they are set in import by user
    glTexParameterf(texture->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(texture->target, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameterf(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glTexParameterf(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (config.srgb_decode_supported && texture->srgb && !texture->using_srgb) {

        glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _DECODE_EXT);
        texture->using_srgb = true;
#ifdef TOOLS_ENABLED
        if (!(texture->flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) {
            texture->flags |= RS::TEXTURE_FLAG_CONVERT_TO_LINEAR;
            //notify that texture must be set to linear beforehand, so it works in other platforms when exported
        }
#endif
    }

    {
        //Irradiance map
        glActiveTexture(GL_TEXTURE1);
        sky->irradiance.create();
        glBindTexture(GL_TEXTURE_2D, sky->irradiance);

        GLuint tmp_fb;

        glGenFramebuffers(1, &tmp_fb);
        glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);

        int size = 32;

        bool use_float = config.framebuffer_half_float_supported;

        GLenum internal_format = use_float ? GL_RGBA16F : GL_RGB10_A2;
        GLenum format = GL_RGBA;
        GLenum type = use_float ? GL_HALF_FLOAT : GL_UNSIGNED_INT_2_10_10_10_REV;

        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, size, size * 2, 0, format, type, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameterf(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sky->irradiance, 0);

        int irradiance_size = GLOBAL_GET("rendering/quality/reflections/irradiance_max_size").as<int>();
        int upscale_size = MIN(int(previous_power_of_2(irradiance_size)), p_radiance_size);

        GLuint tmp_fb2;
        GLuint tmp_tex;
        {
            //generate another one for rendering, as can't read and write from a single texarray it seems
            glGenFramebuffers(1, &tmp_fb2);
            glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb2);
            glGenTextures(1, &tmp_tex);
            glBindTexture(GL_TEXTURE_2D, tmp_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, upscale_size, 2.0f * upscale_size, 0, format, type, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_tex, 0);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef DEBUG_ENABLED
            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
#endif
        }

        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, true);
        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::COMPUTE_IRRADIANCE, true);
        shaders.cubemap_filter.bind();

        // Very large Panoramas require way too much effort to compute irradiance so use a mipmap
        // level that corresponds to a panorama of 1024x512
        shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::SOURCE_MIP_LEVEL, M_MAX(Math::floor(Math::log(float(texture->width)) / Math::log(2.0f)) - 10.0f, 0.0f));

        // Compute Irradiance for a large texture, specified by radiance size and then pull out a low mipmap corresponding to 32x32
        for (int i = 0; i < 2; i++) {
            glViewport(0, i * upscale_size, upscale_size, upscale_size);
            glBindVertexArray(resources.quadie_array);

            shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::Z_FLIP, i > 0);

            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glBindVertexArray(0);
        }
        glGenerateMipmap(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tmp_tex);
        glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);

        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, false);
        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, false);
        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::COMPUTE_IRRADIANCE, false);

        shaders.copy.set_conditional(CopyShaderGLES3::USE_LOD, true);
        shaders.copy.bind();
        shaders.copy.set_uniform(CopyShaderGLES3::MIP_LEVEL, M_MAX(Math::floor(Math::log(float(upscale_size)) / Math::log(2.0f)) - 5.0f, 0.0f)); // Mip level that corresponds to a 32x32 texture

        glViewport(0, 0, size, size * 2.0);
        glBindVertexArray(resources.quadie_array);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);

        shaders.copy.set_conditional(CopyShaderGLES3::USE_LOD, false);

        glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(texture->target, texture->tex_id);
        glDeleteFramebuffers(1, &tmp_fb);
        glDeleteFramebuffers(1, &tmp_fb2);
        glDeleteTextures(1, &tmp_tex);
    }

    // Now compute radiance

    glActiveTexture(GL_TEXTURE1);
    sky->radiance.create();

    if (config.use_texture_array_environment) {

        //texture3D
        glBindTexture(GL_TEXTURE_2D_ARRAY, sky->radiance);

        GLuint tmp_fb;

        glGenFramebuffers(1, &tmp_fb);
        glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);

        int size = p_radiance_size;

        int array_level = 6;

        bool use_float = config.framebuffer_half_float_supported;

        GLenum internal_format = use_float ? GL_RGBA16F : GL_RGB10_A2;
        GLenum format = GL_RGBA;
        GLenum type = use_float ? GL_HALF_FLOAT : GL_UNSIGNED_INT_2_10_10_10_REV;

        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internal_format, size, size * 2, array_level, 0, format, type, nullptr);

        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLuint tmp_fb2;
        GLuint tmp_tex;
        {
            //generate another one for rendering, as can't read and write from a single texarray it seems
            glGenFramebuffers(1, &tmp_fb2);
            glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb2);
            glGenTextures(1, &tmp_tex);
            glBindTexture(GL_TEXTURE_2D, tmp_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, size, size * 2, 0, format, type, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_tex, 0);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#ifdef DEBUG_ENABLED
            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
#endif
        }

        for (int j = 0; j < array_level; j++) {

            glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb2);


            if (j < 3) {

                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, true);
                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID_ARRAY, false);
                shaders.cubemap_filter.bind();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(texture->target, texture->tex_id);
                shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::SOURCE_RESOLUTION, float(texture->width / 4));
            } else {

                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, false);
                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID_ARRAY, true);
                shaders.cubemap_filter.bind();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D_ARRAY, sky->radiance);
                shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::SOURCE_ARRAY_INDEX, j - 1); //read from previous to ensure better blur
            }

            for (int i = 0; i < 2; i++) {
                glViewport(0, i * size, size, size);
                glBindVertexArray(resources.quadie_array);

                shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::Z_FLIP, i > 0);
                shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::ROUGHNESS, j / float(array_level - 1));

                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                glBindVertexArray(0);
            }

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tmp_fb);
            glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, sky->radiance, 0, j);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, tmp_fb2);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBlitFramebuffer(0, 0, size, size * 2, 0, 0, size, size * 2, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        }

        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, false);
        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, false);
        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID_ARRAY, false);

        //restore ranges
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, sky->radiance);

        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        //reset flags on Sky Texture that may have changed
        texture_set_flags(sky->panorama, texture->flags);

        glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
        glDeleteFramebuffers(1, &tmp_fb);
        glDeleteFramebuffers(1, &tmp_fb2);
        glDeleteTextures(1, &tmp_tex);

    } else {
        //regular single texture with mipmaps
        glBindTexture(GL_TEXTURE_2D, sky->radiance);

        GLuint tmp_fb;

        glGenFramebuffers(1, &tmp_fb);
        glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);

        int size = p_radiance_size;

        int lod = 0;

        int mipmaps = 6;

        int mm_level = mipmaps;

        bool use_float = config.framebuffer_half_float_supported;

        GLenum internal_format = use_float ? GL_RGBA16F : GL_RGB10_A2;
        GLenum format = GL_RGBA;
        GLenum type = use_float ? GL_HALF_FLOAT : GL_UNSIGNED_INT_2_10_10_10_REV;

        glTexStorage2DCustom(GL_TEXTURE_2D, mipmaps, internal_format, size, size * 2.0f, format, type);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmaps - 1);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLuint tmp_fb2;
        GLuint tmp_tex;
        {
            // Need a temporary framebuffer for rendering so we can read from previous iterations
            glGenFramebuffers(1, &tmp_fb2);
            glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb2);
            glGenTextures(1, &tmp_tex);
            glBindTexture(GL_TEXTURE_2D, tmp_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, size, size * 2, 0, format, type, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_tex, 0);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef DEBUG_ENABLED
            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
#endif
        }

        lod = 0;
        mm_level = mipmaps;

        size = p_radiance_size;

        while (mm_level) {
            glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sky->radiance, lod);

#ifdef DEBUG_ENABLED
            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            ERR_CONTINUE(status != GL_FRAMEBUFFER_COMPLETE);
#endif
            glBindTexture(GL_TEXTURE_2D, tmp_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, size, size * 2, 0, format, type, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb2);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_tex, 0);

            if (lod < 3) {
                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, true);
                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID, false);
                shaders.cubemap_filter.bind();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(texture->target, texture->tex_id);
                shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::SOURCE_RESOLUTION, float(texture->width / 4));
            } else {

                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, false);
                shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID, true);
                shaders.cubemap_filter.bind();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sky->radiance);
                shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::SOURCE_MIP_LEVEL, float(lod - 1)); //read from previous to ensure better blur
            }

            for (int i = 0; i < 2; i++) {
                glViewport(0, i * size, size, size);
                glBindVertexArray(resources.quadie_array);

                shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::Z_FLIP, i > 0);
                shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::ROUGHNESS, lod / float(mipmaps - 1));

                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                glBindVertexArray(0);
            }

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tmp_fb);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sky->radiance, lod);
            //glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, sky->radiance, 0, lod);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, tmp_fb2);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBlitFramebuffer(0, 0, size, size * 2, 0, 0, size, size * 2, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            if (size > 1) {
                size >>= 1;
            }
            lod++;
            mm_level--;
        }
        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, false);
        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, false);
        shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID, false);

        //restore ranges
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, lod - 1);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        //reset flags on Sky Texture that may have changed
        texture_set_flags(sky->panorama, texture->flags);

        glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
        glDeleteFramebuffers(1, &tmp_fb);
        glDeleteFramebuffers(1, &tmp_fb2);
        glDeleteTextures(1, &tmp_tex);
    }
}
