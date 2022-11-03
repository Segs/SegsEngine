#include "rasterizer_canvas_light_shadow_component.h"

#include "rasterizer_canvas_base_gles3.h"
#include "rasterizer_canvas_occluder_component.h"
#include "rasterizer_render_target_component.h"
#include "servers/rendering/render_entity_getter.h"
#include "servers/rendering/rendering_server_canvas.h"

/* CANVAS SHADOW */

RenderingEntity RasterizerStorageGLES3::canvas_light_shadow_buffer_create(int p_width) {

    auto res = VSG::ecs->create();
    auto &cls(VSG::ecs->registry.emplace<RasterizerCanvasLightShadowComponent>(res));

    if (p_width > config.max_texture_size)
        p_width = config.max_texture_size;

    cls.size = p_width;
    cls.height = 16;

    glActiveTexture(GL_TEXTURE0);

    cls.fbo.create();
    glBindFramebuffer(GL_FRAMEBUFFER, cls.fbo);

    cls.depth.create();
    glBindRenderbuffer(GL_RENDERBUFFER, cls.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, cls.size, cls.height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cls.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    cls.distance.create();
    glBindTexture(GL_TEXTURE_2D, cls.distance);
    if (config.use_rgba_2d_shadows) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cls.size, cls.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, cls.size, cls.height, 0, GL_RED, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cls.distance, 0);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    //printf("errnum: %x\n",status);
    glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        VSG::ecs->registry.destroy(res);
        res = entt::null;
        ERR_FAIL_COND_V(status != GL_FRAMEBUFFER_COMPLETE, entt::null);
    }

    return res;
}


void RasterizerCanvasBaseGLES3::canvas_debug_viewport_shadows(Span<RasterizerCanvasLight3DComponent *> p_lights_with_shadow) {
    auto * current_rt = get<RasterizerRenderTargetComponent>(storage->frame.current_rt);

    canvas_begin(); //reset
    glVertexAttrib4f(RS::ARRAY_COLOR, 1, 1, 1, 1);
    int h = 10;
    int w = current_rt->width;
    int ofs = h;
    glDisable(GL_BLEND);

    for(const RasterizerCanvasLight3DComponent *light : p_lights_with_shadow) {
        if (light->shadow_buffer!=entt::null) {

            RasterizerCanvasLightShadowComponent *sb = get<RasterizerCanvasLightShadowComponent>(light->shadow_buffer);
            if (sb) {
                glBindTexture(GL_TEXTURE_2D, sb->distance);
                draw_generic_textured_rect(Rect2(h, ofs, w - h * 2, h), Rect2(0, 0, 1, 1));
                ofs += h * 2;
            }
        }
    }

    canvas_end();
}

void RasterizerCanvasBaseGLES3::canvas_light_shadow_buffer_update(RenderingEntity p_buffer,
        const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far,
        RenderingEntity p_occluders, CameraMatrix *p_xform_cache) {

    auto *cls = VSG::ecs->try_get<RasterizerCanvasLightShadowComponent>(p_buffer);
    ERR_FAIL_COND(!cls);

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(true);

    glBindFramebuffer(GL_FRAMEBUFFER, cls->fbo);

    state.canvas_shadow_shader.bind();

    glViewport(0, 0, cls->size, cls->height);
    glClearDepth(1.0f);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    RS::CanvasOccluderPolygonCullMode cull = RS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED;

    for (int i = 0; i < 4; i++) {

             //make sure it remains orthogonal, makes easy to read angle later

        Transform light;
        light.origin[0] = p_light_xform[2][0];
        light.origin[1] = p_light_xform[2][1];
        light.basis[0][0] = p_light_xform[0][0];
        light.basis[0][1] = p_light_xform[1][0];
        light.basis[1][0] = p_light_xform[0][1];
        light.basis[1][1] = p_light_xform[1][1];

             //light.basis.scale(Vector3(to_light.elements[0].length(),to_light.elements[1].length(),1));

             //p_near=1;
        CameraMatrix projection;
        {
            real_t fov = 90;
            real_t nearp = p_near;
            real_t farp = p_far;
            real_t aspect = 1.0;

            real_t ymax = nearp * Math::tan(Math::deg2rad(fov * 0.5));
            real_t ymin = -ymax;
            real_t xmin = ymin * aspect;
            real_t xmax = ymax * aspect;

            projection.set_frustum(xmin, xmax, ymin, ymax, nearp, farp);
        }

        Vector3 cam_target = Basis(Vector3(0, 0, Math_PI * 2 * (i / 4.0))).xform(Vector3(0, 1, 0));
        projection = projection * CameraMatrix(Transform().looking_at(cam_target, Vector3(0, 0, -1)).affine_inverse());

        state.canvas_shadow_shader.set_uniform(CanvasShadowShaderGLES3::PROJECTION_MATRIX, projection);
        state.canvas_shadow_shader.set_uniform(CanvasShadowShaderGLES3::LIGHT_MATRIX, light);
        state.canvas_shadow_shader.set_uniform(CanvasShadowShaderGLES3::DISTANCE_NORM, 1.0 / p_far);

        if (i == 0)
            *p_xform_cache = projection;

        glViewport(0, (cls->height / 4) * i, cls->size, cls->height / 4);

        auto occluder_iter = p_occluders;

        while (occluder_iter != entt::null) {

            auto *instance = get<RasterizerCanvasLightOccluderInstanceComponent>(occluder_iter);
            auto *cc = VSG::ecs->try_get<RasterizerCanvasOccluderComponent>(instance->polygon_buffer);
            if (!cc || cc->len == 0 || !(p_light_mask & instance->light_mask)) {

                occluder_iter = instance->next;
                continue;
            }

            state.canvas_shadow_shader.set_uniform(CanvasShadowShaderGLES3::WORLD_MATRIX, instance->xform_cache);

            RS::CanvasOccluderPolygonCullMode transformed_cull_cache = instance->cull_cache;

            if (transformed_cull_cache != RS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED &&
                    (p_light_xform.basis_determinant() * instance->xform_cache.basis_determinant()) < 0) {
                transformed_cull_cache =
                        transformed_cull_cache == RS::CANVAS_OCCLUDER_POLYGON_CULL_CLOCKWISE ?
                                RS::CANVAS_OCCLUDER_POLYGON_CULL_COUNTER_CLOCKWISE :
                                RS::CANVAS_OCCLUDER_POLYGON_CULL_CLOCKWISE;
            }

            if (cull != transformed_cull_cache) {

                cull = transformed_cull_cache;
                switch (cull) {
                    case RS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED: {

                        glDisable(GL_CULL_FACE);

                    } break;
                    case RS::CANVAS_OCCLUDER_POLYGON_CULL_CLOCKWISE: {

                        glEnable(GL_CULL_FACE);
                        glCullFace(GL_FRONT);
                    } break;
                    case RS::CANVAS_OCCLUDER_POLYGON_CULL_COUNTER_CLOCKWISE: {

                        glEnable(GL_CULL_FACE);
                        glCullFace(GL_BACK);

                    } break;
                }
            }

            glBindVertexArray(cc->array_id);
            glDrawElements(GL_TRIANGLES, cc->len * 3, GL_UNSIGNED_SHORT, nullptr);

            occluder_iter = instance->next;
        }
    }

    glBindVertexArray(0);
}
