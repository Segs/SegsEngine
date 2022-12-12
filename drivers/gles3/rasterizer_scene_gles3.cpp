/*************************************************************************/
/*  rasterizer_scene_gles3.cpp                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "drivers/gles3/rasterizer_gi_probe_component.h"
#include "drivers/gles3/rasterizer_light_instance_component.h"
#include "drivers/gles3/rasterizer_reflection_atlas_component.h"
#include "drivers/gles3/rasterizer_reflection_probe_component.h"
#include "drivers/gles3/rasterizer_shadow_atlas_component.h"
#include "drivers/gles3/rasterizer_skeleton_component.h"
#include "rasterizer_common_geometry_component.h"
#include "rasterizer_immediate_geometry_component.h"
#include "rasterizer_material_component.h"
#include "rasterizer_scene_gles3.h"
#include "rasterizer_surface_component.h"

#include "drivers/gles3/rasterizer_environment_component.h"
#include "drivers/gles3/rasterizer_light3d_component.h"
#include "drivers/gles3/rasterizer_lightmap_capture_component.h"
#include "drivers/gles3/rasterizer_material_component.h"
#include "drivers/gles3/rasterizer_mesh_component.h"
#include "drivers/gles3/rasterizer_multimesh_component.h"
#include "drivers/gles3/rasterizer_particle_component.h"
#include "drivers/gles3/rasterizer_shader_component.h"
#include "drivers/gles3/rasterizer_sky_component.h"
#include "rasterizer_canvas_gles3.h"
#include "rasterizer_texture_component.h"

#include "core/math/math_funcs.h"
#include "core/external_profiler.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "servers/camera/camera_feed.h"
#include "servers/rendering/render_entity_getter.h"
#include "servers/rendering/rendering_server_raster.h"

RenderingEntity RasterizerSceneGLES3::reflection_atlas_create() {
    return ::reflection_atlas_create();
}

void RasterizerSceneGLES3::reflection_atlas_set_size(RenderingEntity p_ref_atlas, int p_size) {
    ::reflection_atlas_set_size(p_ref_atlas,p_size);
}

void RasterizerSceneGLES3::reflection_atlas_set_subdivision(RenderingEntity p_ref_atlas, int p_subdiv) {
    ::reflection_atlas_set_subdivision(p_ref_atlas,p_subdiv);
}


bool RasterizerSceneGLES3::is_environment(RenderingEntity p_env) {
    return p_env!=entt::null ? ::is_environment(p_env) : false;
}
//////////////////////

RenderingEntity RasterizerSceneGLES3::gi_probe_instance_create() {
    return ::gi_probe_instance_create();
}

void RasterizerSceneGLES3::gi_probe_instance_set_light_data(RenderingEntity p_probe, RenderingEntity p_base, RenderingEntity p_data) {
    ::gi_probe_instance_set_light_data(p_probe, p_base, p_data);
}
void RasterizerSceneGLES3::gi_probe_instance_set_transform_to_data(RenderingEntity p_probe, const Transform &p_xform) {
    ::gi_probe_instance_set_transform_to_data(p_probe, p_xform);
}

void RasterizerSceneGLES3::gi_probe_instance_set_bounds(RenderingEntity p_probe, const Vector3 &p_bounds) {
    ::gi_probe_instance_set_bounds(p_probe, p_bounds);
}
////////////////////////////
////////////////////////////
////////////////////////////

bool RasterizerSceneGLES3::_setup_material(RasterizerMaterialComponent *p_material, bool p_depth_pass, bool p_alpha_pass) {
    SCOPE_AUTONAMED;

    /* this is handled outside
    if (p_material->shader->spatial.cull_mode == RasterizerStorageGLES3::Shader::Node3D::CULL_MODE_DISABLED) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
    } */

    if (state.current_line_width != p_material->line_width) {
        //glLineWidth(M_MAX(p_material->line_width,1.0));
        state.current_line_width = p_material->line_width;
    }
    assert(p_material->shader!=entt::null);
    auto *shader=getUnchecked<RasterizerShaderComponent>(p_material->shader);

    if (state.current_depth_test != (!shader->spatial.no_depth_test)) {
        if (shader->spatial.no_depth_test) {
            glDisable(GL_DEPTH_TEST);

        } else {
            glEnable(GL_DEPTH_TEST);
        }

        state.current_depth_test = !shader->spatial.no_depth_test;
    }

    if (state.current_depth_draw != shader->spatial.depth_draw_mode) {
        switch (shader->spatial.depth_draw_mode) {
            case RasterizerShaderComponent::Node3D::DEPTH_DRAW_ALPHA_PREPASS: {
                glDepthMask(p_depth_pass);
                // If some transparent objects write to depth, we need to re-copy depth texture when we need it
                if (p_alpha_pass && !state.used_depth_prepass) {
                    state.prepared_depth_texture = false;
                }
            } break;
            case RasterizerShaderComponent::Node3D::DEPTH_DRAW_OPAQUE: {

                glDepthMask(!p_alpha_pass);
            } break;
            case RasterizerShaderComponent::Node3D::DEPTH_DRAW_ALWAYS: {
                glDepthMask(GL_TRUE);
                // If some transparent objects write to depth, we need to re-copy depth texture when we need it
                if (p_alpha_pass) {
                    state.prepared_depth_texture = false;
                }
            } break;
            case RasterizerShaderComponent::Node3D::DEPTH_DRAW_NEVER: {
                glDepthMask(GL_FALSE);
            } break;
        }

        state.current_depth_draw = shader->spatial.depth_draw_mode;
    }

    //material parameters

    state.scene_shader.set_custom_shader(shader->custom_code_id);
    bool rebind = state.scene_shader.bind();

    if (p_material->ubo_id) {

        glBindBufferBase(GL_UNIFORM_BUFFER, 1, p_material->ubo_id);
    }

    int tc = p_material->textures.size();
    RenderingEntity *textures = p_material->textures.data();
    ShaderLanguage::ShaderNode::Uniform::Hint *texture_hints = shader->texture_hints.data();
    const ShaderLanguage::DataType *texture_types = shader->texture_types.data();

    state.current_main_tex = 0;

    for (int i = 0; i < tc; i++) {

        glActiveTexture(GL_TEXTURE0 + i);

        GLenum target = GL_TEXTURE_2D;
        GLuint tex = 0;

        RasterizerTextureComponent *t = get<RasterizerTextureComponent>(textures[i]);

        if (t) {

            if (t->redraw_if_visible) { //must check before proxy because this is often used with proxies
                RenderingServerRaster::redraw_request();
            }

            t = t->get_ptr(); //resolve for proxies

#ifdef TOOLS_ENABLED
            if (t->detect_3d) {
                t->detect_3d(t->detect_3d_ud);
            }
            if (t->detect_normal && texture_hints[i] == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL) {
                t->detect_normal(t->detect_normal_ud);
            }
#endif
            if (t->render_target!=entt::null) {
                VSG::ecs->registry.get<RasterizerRenderTargetComponent>(t->render_target).used_in_frame = true;
            }

            target = t->target;
            tex = t->tex_id;
        } else {

            switch (texture_types[i]) {
                case ShaderLanguage::TYPE_ISAMPLER2D:
                case ShaderLanguage::TYPE_USAMPLER2D:
                case ShaderLanguage::TYPE_SAMPLER2D: {
                    target = GL_TEXTURE_2D;

                    switch (texture_hints[i]) {
                        case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK_ALBEDO:
                        case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK: {
                            tex = storage->resources.black_tex;
                        } break;
                        case ShaderLanguage::ShaderNode::Uniform::HINT_TRANSPARENT: {
                            tex = storage->resources.transparent_tex;
                        } break;
                        case ShaderLanguage::ShaderNode::Uniform::HINT_ANISO: {
                            tex = storage->resources.aniso_tex;
                        } break;
                        case ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL: {
                            tex = storage->resources.normal_tex;

                        } break;
                        default: {
                            tex = storage->resources.white_tex;
                        } break;
                    }

                } break;

                case ShaderLanguage::TYPE_SAMPLERCUBE: {
                    // TODO
                } break;

                case ShaderLanguage::TYPE_ISAMPLER3D:
                case ShaderLanguage::TYPE_USAMPLER3D:
                case ShaderLanguage::TYPE_SAMPLER3D: {

                    target = GL_TEXTURE_3D;
                    tex = storage->resources.white_tex_3d;

                    //switch (texture_hints[i]) {
                    // TODO
                    //}

                } break;

                case ShaderLanguage::TYPE_ISAMPLER2DARRAY:
                case ShaderLanguage::TYPE_USAMPLER2DARRAY:
                case ShaderLanguage::TYPE_SAMPLER2DARRAY: {

                    target = GL_TEXTURE_2D_ARRAY;
                    tex = storage->resources.white_tex_array;

                    //switch (texture_hints[i]) {
                    // TODO
                    //}

                } break;

                default: {
                    break;
                }
            }
        }

        glBindTexture(target, tex);

        if (t && storage->config.srgb_decode_supported) {
            //if SRGB decode extension is present, simply switch the texture to whatever is needed
            bool must_srgb = false;

            if (t->srgb && (texture_hints[i] == ShaderLanguage::ShaderNode::Uniform::HINT_ALBEDO || texture_hints[i] == ShaderLanguage::ShaderNode::Uniform::HINT_BLACK_ALBEDO)) {
                must_srgb = true;
            }

            if (t->using_srgb != must_srgb) {
                if (must_srgb) {
                    glTexParameteri(t->target, _TEXTURE_SRGB_DECODE_EXT, _DECODE_EXT);
#ifdef TOOLS_ENABLED
                    if (t->detect_srgb) {
                        t->detect_srgb(t->detect_srgb_ud);
                    }
#endif

                } else {
                    glTexParameteri(t->target, _TEXTURE_SRGB_DECODE_EXT, _SKIP_DECODE_EXT);
                }
                t->using_srgb = must_srgb;
            }
        }

        if (i == 0) {
            state.current_main_tex = tex;
        }
    }

    return rebind;
}

struct RasterizerGLES3Particle {

    float color[4];
    float velocity_active[4];
    float custom[4];
    float xform_1[4];
    float xform_2[4];
    float xform_3[4];
};

struct RasterizerGLES3ParticleSort {

    Vector3 z_dir;
    bool operator()(const RasterizerGLES3Particle &p_a, const RasterizerGLES3Particle &p_b) const {

        return z_dir.dot(Vector3(p_a.xform_1[3], p_a.xform_2[3], p_a.xform_3[3])) < z_dir.dot(Vector3(p_b.xform_1[3], p_b.xform_2[3], p_b.xform_3[3]));
    }
};

void RasterizerSceneGLES3::_setup_geometry(RenderListElement *e, const Transform &p_view_transform) {

    auto *instance = get<RenderingInstanceComponent>(e->instance);
    switch (instance->base_type) {
        case RS::INSTANCE_MESH: {
    auto *s = get<RasterizerSurfaceComponent>(e->geometry);

            if (!s->blend_shapes.empty() && !instance->blend_values.empty()) {
                //blend shapes, use transform feedback
                storage->mesh_render_blend_shapes(s, instance->blend_values.data());
                //rebind shader
                state.scene_shader.bind();
#ifdef DEBUG_ENABLED
            } else if (state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME && s->array_wireframe_id) {
                glBindVertexArray(s->array_wireframe_id); // everything is so easy nowadays
#endif
            } else {
                glBindVertexArray(s->array_id); // everything is so easy nowadays
            }

        } break;

        case RS::INSTANCE_MULTIMESH: {

            auto *multi_mesh = get<RasterizerMultiMeshComponent>(e->owner);
            auto *s = get<RasterizerSurfaceComponent>(e->geometry);
#ifdef DEBUG_ENABLED
            if (state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME && s->instancing_array_wireframe_id) {

                glBindVertexArray(s->instancing_array_wireframe_id); // use the instancing array ID
            } else
#endif
            {
                glBindVertexArray(s->instancing_array_id); // use the instancing array ID
            }

            glBindBuffer(GL_ARRAY_BUFFER, multi_mesh->buffer); //modify the buffer

            int stride = (multi_mesh->xform_floats + multi_mesh->color_floats + multi_mesh->custom_data_floats) * 4;
            glEnableVertexAttribArray(8);
            glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, nullptr);
            glVertexAttribDivisor(8, 1);
            glEnableVertexAttribArray(9);
            glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(4 * 4));
            glVertexAttribDivisor(9, 1);

            int color_ofs;

            if (multi_mesh->transform_format == RS::MULTIMESH_TRANSFORM_3D) {
                glEnableVertexAttribArray(10);
                glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(8 * 4));
                glVertexAttribDivisor(10, 1);
                color_ofs = 12 * 4;
            } else {
                glDisableVertexAttribArray(10);
                glVertexAttrib4f(10, 0, 0, 1, 0);
                color_ofs = 8 * 4;
            }

            int custom_data_ofs = color_ofs;

            switch (multi_mesh->color_format) {

                case RS::MULTIMESH_COLOR_MAX:
                case RS::MULTIMESH_COLOR_NONE: {
                    glDisableVertexAttribArray(11);
                    glVertexAttrib4f(11, 1, 1, 1, 1);
                } break;
                case RS::MULTIMESH_COLOR_8BIT: {
                    glEnableVertexAttribArray(11);
                    glVertexAttribPointer(11, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, CAST_INT_TO_UCHAR_PTR(color_ofs));
                    glVertexAttribDivisor(11, 1);
                    custom_data_ofs += 4;

                } break;
                case RS::MULTIMESH_COLOR_FLOAT: {
                    glEnableVertexAttribArray(11);
                    glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(color_ofs));
                    glVertexAttribDivisor(11, 1);
                    custom_data_ofs += 4 * 4;
                } break;
            }

            switch (multi_mesh->custom_data_format) {
                case RS::MULTIMESH_CUSTOM_DATA_MAX:
                case RS::MULTIMESH_CUSTOM_DATA_NONE: {
                    glDisableVertexAttribArray(12);
                    glVertexAttrib4f(12, 1, 1, 1, 1);
                } break;
                case RS::MULTIMESH_CUSTOM_DATA_8BIT: {
                    glEnableVertexAttribArray(12);
                    glVertexAttribPointer(12, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, CAST_INT_TO_UCHAR_PTR(custom_data_ofs));
                    glVertexAttribDivisor(12, 1);

                } break;
                case RS::MULTIMESH_CUSTOM_DATA_FLOAT: {
                    glEnableVertexAttribArray(12);
                    glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(custom_data_ofs));
                    glVertexAttribDivisor(12, 1);
                } break;
            }

        } break;
        case RS::INSTANCE_PARTICLES: {

            RasterizerParticlesComponent *particles=get<RasterizerParticlesComponent>(e->owner);
            RasterizerSurfaceComponent *s=get<RasterizerSurfaceComponent>(e->geometry);

            if (particles->draw_order == RS::PARTICLES_DRAW_ORDER_VIEW_DEPTH && particles->particle_valid_histories[1]) {

                glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffer_histories[1]); //modify the buffer, this was used 2 frames ago so it should be good enough for flushing
                RasterizerGLES3Particle *particle_array;
                particle_array = static_cast<RasterizerGLES3Particle *>(glMapBufferRange(GL_ARRAY_BUFFER, 0, particles->amount * 24 * sizeof(float), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));

                SortArray<RasterizerGLES3Particle, RasterizerGLES3ParticleSort> sorter;

                if (particles->use_local_coords) {
                    sorter.compare.z_dir = instance->transform.affine_inverse().xform(p_view_transform.basis.get_axis(2)).normalized();
                } else {
                    sorter.compare.z_dir = p_view_transform.basis.get_axis(2).normalized();
                }

                sorter.sort(particle_array, particles->amount);

                glUnmapBuffer(GL_ARRAY_BUFFER);
#ifdef DEBUG_ENABLED
                if (state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME && s->instancing_array_wireframe_id) {
                    glBindVertexArray(s->instancing_array_wireframe_id); // use the wireframe instancing array ID
                } else
#endif
                {

                    glBindVertexArray(s->instancing_array_id); // use the instancing array ID
                }
                glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffer_histories[1]); //modify the buffer

            } else {
#ifdef DEBUG_ENABLED
                if (state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME && s->instancing_array_wireframe_id) {
                    glBindVertexArray(s->instancing_array_wireframe_id); // use the wireframe instancing array ID
                } else
#endif
                {
                    glBindVertexArray(s->instancing_array_id); // use the instancing array ID
                }
                glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffers[0]); //modify the buffer
            }

            int stride = sizeof(float) * 4 * 6;

            //transform

            if (particles->draw_order != RS::PARTICLES_DRAW_ORDER_LIFETIME) {

                glEnableVertexAttribArray(8); //xform x
                glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 3));
                glVertexAttribDivisor(8, 1);
                glEnableVertexAttribArray(9); //xform y
                glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 4));
                glVertexAttribDivisor(9, 1);
                glEnableVertexAttribArray(10); //xform z
                glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 5));
                glVertexAttribDivisor(10, 1);
                glEnableVertexAttribArray(11); //color
                glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, nullptr);
                glVertexAttribDivisor(11, 1);
                glEnableVertexAttribArray(12); //custom
                glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 2));
                glVertexAttribDivisor(12, 1);
            }

        } break;
        default: {
        }
    }
}

void RasterizerSceneGLES3::_render_geometry(RenderListElement *e) {
    auto *instance = get<RenderingInstanceComponent>(e->instance);

    switch (instance->base_type) {
        case RS::INSTANCE_MESH: {
    auto *s = get<RasterizerSurfaceComponent>(e->geometry);

#ifdef DEBUG_ENABLED

            if (state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME && s->array_wireframe_id) {

                glDrawElements(GL_LINES, s->index_wireframe_len, GL_UNSIGNED_INT, nullptr);
                get_rasterizer_storage_info().render.vertices_count += s->index_array_len;
            } else
#endif
                    if (s->index_array_len > 0) {

                glDrawElements(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, nullptr);

                get_rasterizer_storage_info().render.vertices_count += s->index_array_len;

            } else {

                glDrawArrays(gl_primitive[s->primitive], 0, s->array_len);

                get_rasterizer_storage_info().render.vertices_count += s->array_len;
            }

        } break;
        case RS::INSTANCE_MULTIMESH: {

            auto *multi_mesh = get<RasterizerMultiMeshComponent>(e->owner);
            auto *s = get<RasterizerSurfaceComponent>(e->geometry);

            int amount = MIN(multi_mesh->size, multi_mesh->visible_instances);

            if (amount == -1) {
                amount = multi_mesh->size;
            }
            if (!amount) {
                return;
            }
#ifdef DEBUG_ENABLED

            if (state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME && s->array_wireframe_id) {

                glDrawElementsInstanced(GL_LINES, s->index_wireframe_len, GL_UNSIGNED_INT, nullptr, amount);
                get_rasterizer_storage_info().render.vertices_count += s->index_array_len * amount;
            } else
#endif
                    if (s->index_array_len > 0) {

                glDrawElementsInstanced(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, nullptr, amount);

                get_rasterizer_storage_info().render.vertices_count += s->index_array_len * amount;

            } else {

                glDrawArraysInstanced(gl_primitive[s->primitive], 0, s->array_len, amount);

                get_rasterizer_storage_info().render.vertices_count += s->array_len * amount;
            }

        } break;
        case RS::INSTANCE_IMMEDIATE: {

            bool restore_tex = false;
            const auto *im = get<RasterizerImmediateGeometryComponent>(e->geometry);

            if (im->building) {
                return;
            }

            glBindBuffer(GL_ARRAY_BUFFER, state.immediate_buffer);
            glBindVertexArray(state.immediate_array);

            for (const RasterizerImmediateGeometryComponent::Chunk &c : im->chunks) {

                if (c.vertices.empty()) {
                    continue;
                }

                auto vertices = c.vertices.size();
                uint32_t buf_ofs = 0;

                get_rasterizer_storage_info().render.vertices_count += vertices;

                RasterizerTextureComponent *t = get<RasterizerTextureComponent>(c.texture);
                if (t) {

                    if (t->redraw_if_visible) {
                        RenderingServerRaster::redraw_request();
                    }
                    t = t->get_ptr(); //resolve for proxies

#ifdef TOOLS_ENABLED
                    if (t->detect_3d) {
                        t->detect_3d(t->detect_3d_ud);
                    }
#endif

                    auto *rt = get<RasterizerRenderTargetComponent>(t->render_target);
                    if (rt) {
                        rt->used_in_frame = true;
                    }

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(t->target, t->get_texture_id());
                    restore_tex = true;

                } else if (restore_tex) {

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, state.current_main_tex);
                    restore_tex = false;
                }

                if (!c.normals.empty()) {

                    glEnableVertexAttribArray(RS::ARRAY_NORMAL);
                    glBufferSubData(GL_ARRAY_BUFFER, buf_ofs, sizeof(Vector3) * vertices, c.normals.data());
                    glVertexAttribPointer(RS::ARRAY_NORMAL, 3, GL_FLOAT, false, sizeof(Vector3), CAST_INT_TO_UCHAR_PTR(buf_ofs));
                    buf_ofs += sizeof(Vector3) * vertices;

                } else {

                    glDisableVertexAttribArray(RS::ARRAY_NORMAL);
                }

                if (!c.tangents.empty()) {

                    glEnableVertexAttribArray(RS::ARRAY_TANGENT);
                    glBufferSubData(GL_ARRAY_BUFFER, buf_ofs, sizeof(Plane) * vertices, c.tangents.data());
                    glVertexAttribPointer(RS::ARRAY_TANGENT, 4, GL_FLOAT, false, sizeof(Plane), CAST_INT_TO_UCHAR_PTR(buf_ofs));
                    buf_ofs += sizeof(Plane) * vertices;

                } else {

                    glDisableVertexAttribArray(RS::ARRAY_TANGENT);
                }

                if (!c.colors.empty()) {

                    glEnableVertexAttribArray(RS::ARRAY_COLOR);
                    glBufferSubData(GL_ARRAY_BUFFER, buf_ofs, sizeof(Color) * vertices, c.colors.data());
                    glVertexAttribPointer(RS::ARRAY_COLOR, 4, GL_FLOAT, false, sizeof(Color), CAST_INT_TO_UCHAR_PTR(buf_ofs));
                    buf_ofs += sizeof(Color) * vertices;

                } else {

                    glDisableVertexAttribArray(RS::ARRAY_COLOR);
                    glVertexAttrib4f(RS::ARRAY_COLOR, 1, 1, 1, 1);
                }

                if (!c.uvs.empty()) {

                    glEnableVertexAttribArray(RS::ARRAY_TEX_UV);
                    glBufferSubData(GL_ARRAY_BUFFER, buf_ofs, sizeof(Vector2) * vertices, c.uvs.data());
                    glVertexAttribPointer(RS::ARRAY_TEX_UV, 2, GL_FLOAT, false, sizeof(Vector2), CAST_INT_TO_UCHAR_PTR(buf_ofs));
                    buf_ofs += sizeof(Vector2) * vertices;

                } else {

                    glDisableVertexAttribArray(RS::ARRAY_TEX_UV);
                }

                if (!c.uvs2.empty()) {

                    glEnableVertexAttribArray(RS::ARRAY_TEX_UV2);
                    glBufferSubData(GL_ARRAY_BUFFER, buf_ofs, sizeof(Vector2) * vertices, c.uvs2.data());
                    glVertexAttribPointer(RS::ARRAY_TEX_UV2, 2, GL_FLOAT, false, sizeof(Vector2), CAST_INT_TO_UCHAR_PTR(buf_ofs));
                    buf_ofs += sizeof(Vector2) * vertices;

                } else {

                    glDisableVertexAttribArray(RS::ARRAY_TEX_UV2);
                }

                glEnableVertexAttribArray(RS::ARRAY_VERTEX);
                glBufferSubData(GL_ARRAY_BUFFER, buf_ofs, sizeof(Vector3) * vertices, c.vertices.data());
                glVertexAttribPointer(RS::ARRAY_VERTEX, 3, GL_FLOAT, false, sizeof(Vector3), CAST_INT_TO_UCHAR_PTR(buf_ofs));
                glDrawArrays(gl_primitive[c.primitive], 0, c.vertices.size());
            }

            if (restore_tex) {

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, state.current_main_tex);
                restore_tex = false;
            }
        } break;
        case RS::INSTANCE_PARTICLES: {

            auto *particles = get<RasterizerParticlesComponent>(e->owner);
            auto *s = get<RasterizerSurfaceComponent>(e->geometry);

            if (!particles->use_local_coords) { //not using local coordinates? then clear transform..
                state.scene_shader.set_uniform(SceneShaderGLES3::WORLD_TRANSFORM, Transform());
            }

            int amount = particles->amount;

            if (particles->draw_order == RS::PARTICLES_DRAW_ORDER_LIFETIME) {
                //split

                int stride = sizeof(float) * 4 * 6;
                int split = int(Math::ceil(particles->phase * particles->amount));

                if (amount - split > 0) {
                    glEnableVertexAttribArray(8); //xform x
                    glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 3));
                    glVertexAttribDivisor(8, 1);
                    glEnableVertexAttribArray(9); //xform y
                    glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 4));
                    glVertexAttribDivisor(9, 1);
                    glEnableVertexAttribArray(10); //xform z
                    glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 5));
                    glVertexAttribDivisor(10, 1);
                    glEnableVertexAttribArray(11); //color
                    glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + 0));
                    glVertexAttribDivisor(11, 1);
                    glEnableVertexAttribArray(12); //custom
                    glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 2));
                    glVertexAttribDivisor(12, 1);
#ifdef DEBUG_ENABLED

                    if (state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME && s->array_wireframe_id) {

                        glDrawElementsInstanced(GL_LINES, s->index_wireframe_len, GL_UNSIGNED_INT, nullptr, amount - split);
                        get_rasterizer_storage_info().render.vertices_count += s->index_array_len * (amount - split);
                    } else
#endif
                            if (s->index_array_len > 0) {

                        glDrawElementsInstanced(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, nullptr, amount - split);

                        get_rasterizer_storage_info().render.vertices_count += s->index_array_len * (amount - split);

                    } else {

                        glDrawArraysInstanced(gl_primitive[s->primitive], 0, s->array_len, amount - split);

                        get_rasterizer_storage_info().render.vertices_count += s->array_len * (amount - split);
                    }
                }

                if (split > 0) {
                    glEnableVertexAttribArray(8); //xform x
                    glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 3));
                    glVertexAttribDivisor(8, 1);
                    glEnableVertexAttribArray(9); //xform y
                    glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 4));
                    glVertexAttribDivisor(9, 1);
                    glEnableVertexAttribArray(10); //xform z
                    glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 5));
                    glVertexAttribDivisor(10, 1);
                    glEnableVertexAttribArray(11); //color
                    glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, nullptr);
                    glVertexAttribDivisor(11, 1);
                    glEnableVertexAttribArray(12); //custom
                    glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 2));
                    glVertexAttribDivisor(12, 1);
#ifdef DEBUG_ENABLED

                    if (state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME && s->array_wireframe_id) {

                        glDrawElementsInstanced(GL_LINES, s->index_wireframe_len, GL_UNSIGNED_INT, nullptr, split);
                        get_rasterizer_storage_info().render.vertices_count += s->index_array_len * split;
                    } else
#endif
                            if (s->index_array_len > 0) {

                        glDrawElementsInstanced(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, nullptr, split);

                        get_rasterizer_storage_info().render.vertices_count += s->index_array_len * split;

                    } else {

                        glDrawArraysInstanced(gl_primitive[s->primitive], 0, s->array_len, split);

                        get_rasterizer_storage_info().render.vertices_count += s->array_len * split;
                    }
                }

            } else {

#ifdef DEBUG_ENABLED

                if (state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME && s->array_wireframe_id) {

                    glDrawElementsInstanced(GL_LINES, s->index_wireframe_len, GL_UNSIGNED_INT, nullptr, amount);
                    get_rasterizer_storage_info().render.vertices_count += s->index_array_len * amount;
                } else
#endif

                        if (s->index_array_len > 0) {

                    glDrawElementsInstanced(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, nullptr, amount);

                    get_rasterizer_storage_info().render.vertices_count += s->index_array_len * amount;

                } else {

                    glDrawArraysInstanced(gl_primitive[s->primitive], 0, s->array_len, amount);

                    get_rasterizer_storage_info().render.vertices_count += s->array_len * amount;
                }
            }

        } break;
        default: {
        }
    }
}

void RasterizerSceneGLES3::_setup_light(RenderListElement *e, const Transform &p_view_transform) {

    int maxobj = state.max_forward_lights_per_object;
    int *omni_indices = (int *)alloca(maxobj * sizeof(int));
    int omni_count = 0;
    int *spot_indices = (int *)alloca(maxobj * sizeof(int));
    int spot_count = 0;
    int reflection_indices[16];
    int reflection_count = 0;
    RenderingInstanceComponent *instance = get<RenderingInstanceComponent>(e->instance);
    for (RenderingEntity light : instance->light_instances) {
        auto *li = get<RasterizerLightInstanceComponent>(light);

        if (!li || li->last_pass != render_pass) {
            continue; // Not visible
        }
        auto *light_ptr = get<RasterizerLight3DComponent>(li->light);
        ERR_FAIL_COND(!light_ptr);

        if (instance->baked_light && light_ptr->bake_mode == RS::LightBakeMode::LIGHT_BAKE_ALL) {
            continue; // This light is already included in the lightmap
        }

        if (light_ptr->type == RS::LIGHT_OMNI) {
            if (omni_count < maxobj && instance->layer_mask & light_ptr->cull_mask) {
                omni_indices[omni_count++] = li->light_index;
            }
        }

        if (light_ptr->type == RS::LIGHT_SPOT) {
            if (spot_count < maxobj && instance->layer_mask & light_ptr->cull_mask) {
                spot_indices[spot_count++] = li->light_index;
            }
        }
    }

    state.scene_shader.set_uniform(SceneShaderGLES3::OMNI_LIGHT_COUNT, omni_count);

    if (omni_count) {
        glUniform1iv(state.scene_shader.get_uniform(SceneShaderGLES3::OMNI_LIGHT_INDICES), omni_count, omni_indices);
    }

    state.scene_shader.set_uniform(SceneShaderGLES3::SPOT_LIGHT_COUNT, spot_count);
    if (spot_count) {
        glUniform1iv(state.scene_shader.get_uniform(SceneShaderGLES3::SPOT_LIGHT_INDICES), spot_count, spot_indices);
    }

    for (RenderingEntity reflection : instance->reflection_probe_instances) {
        auto *rpi = get<RasterizerReflectionProbeInstanceComponent>(reflection);
        if (rpi->last_pass != render_pass) { //not visible
            continue;
        }

        if (reflection_count < maxobj) {
            reflection_indices[reflection_count++] = rpi->reflection_index;
        }
    }

    state.scene_shader.set_uniform(SceneShaderGLES3::REFLECTION_COUNT, reflection_count);
    if (reflection_count) {
        glUniform1iv(state.scene_shader.get_uniform(SceneShaderGLES3::REFLECTION_INDICES), reflection_count, reflection_indices);
    }

    bool probe_based = setup_probes(e->instance,state.scene_shader,storage->config.max_texture_image_units,p_view_transform,false);
    if(probe_based)
        return;

    if (!instance->lightmap_capture_data.empty()) {
        glUniform4fv(state.scene_shader.get_uniform_location(SceneShaderGLES3::LIGHTMAP_CAPTURES), 12, (const GLfloat *)instance->lightmap_capture_data.data());

    } else if (instance->lightmap!=entt::null) {
        RasterizerTextureComponent *lightmap = get<RasterizerTextureComponent>(instance->lightmap);
        if (instance->lightmap_slice == -1) {
            glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 10);
        } else {
            glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 11);
            state.scene_shader.set_uniform(SceneShaderGLES3::LIGHTMAP_LAYER, instance->lightmap_slice);
        }
        glBindTexture(GL_TEXTURE_2D_ARRAY, lightmap->get_texture_id());
        const Rect2 &uvr = instance->lightmap_uv_rect;
        state.scene_shader.set_uniform(SceneShaderGLES3::LIGHTMAP_UV_RECT, Color(uvr.get_position().x, uvr.get_position().y, uvr.get_size().x, uvr.get_size().y));
        if (storage->config.use_lightmap_filter_bicubic) {
            state.scene_shader.set_uniform(SceneShaderGLES3::LIGHTMAP_TEXTURE_SIZE, Vector2(lightmap->width, lightmap->height));
        }
        auto *lc = get<RenderingInstanceComponent>(instance->lightmap_capture);
        auto *capture = get<RasterizerLightmapCaptureComponent>(lc->base);

        if (capture) {
            state.scene_shader.set_uniform(SceneShaderGLES3::LIGHTMAP_ENERGY, capture->energy);
        }
    }
}

void RasterizerSceneGLES3::_set_cull(bool p_front, bool p_disabled, bool p_reverse_cull) {

    bool front = p_front;
    if (p_reverse_cull) {
        front = !front;
    }

    if (p_disabled != state.cull_disabled) {
        if (p_disabled) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
        }
        state.cull_disabled = p_disabled;
    }

    if (front != state.cull_front) {

        glCullFace(front ? GL_FRONT : GL_BACK);
        state.cull_front = front;
    }
}

void RasterizerSceneGLES3::_render_list(Span<RenderListElement *> p_elements, const Transform &p_view_transform, const CameraMatrix &p_projection, RasterizerSkyComponent *p_sky, bool p_reverse_cull, bool p_alpha_pass, bool p_shadow, bool p_directional_add, bool p_directional_shadows) {
    auto * current_rt = get<RasterizerRenderTargetComponent>(storage->frame.current_rt);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, state.scene_ubo); //bind globals ubo

    bool use_radiance_map = false;
    if (!p_shadow && !p_directional_add) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, state.env_radiance_ubo); //bind environment radiance info

        if (p_sky != nullptr) {
            if (storage->config.use_texture_array_environment) {
                glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 3);
                glBindTexture(GL_TEXTURE_2D_ARRAY, p_sky->radiance);
            } else {
                glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 2);
                glBindTexture(GL_TEXTURE_2D, p_sky->radiance);
            }
            glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 7);
            glBindTexture(GL_TEXTURE_2D, p_sky->irradiance);
            state.scene_shader.set_conditional(SceneShaderGLES3::USE_RADIANCE_MAP, true);
            state.scene_shader.set_conditional(SceneShaderGLES3::USE_RADIANCE_MAP_ARRAY, storage->config.use_texture_array_environment);
            use_radiance_map = true;
        } else {
            state.scene_shader.set_conditional(SceneShaderGLES3::USE_RADIANCE_MAP, false);
            state.scene_shader.set_conditional(SceneShaderGLES3::USE_RADIANCE_MAP_ARRAY, false);
        }
    } else {

        state.scene_shader.set_conditional(SceneShaderGLES3::USE_RADIANCE_MAP, false);
        state.scene_shader.set_conditional(SceneShaderGLES3::USE_RADIANCE_MAP_ARRAY, false);
    }

    state.cull_front = false;
    state.cull_disabled = false;
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    state.current_depth_test = true;
    glEnable(GL_DEPTH_TEST);


    state.current_blend_mode = -1;
    state.current_line_width = -1;
    state.current_depth_draw = -1;

    RasterizerMaterialComponent *prev_material = nullptr;
    RenderingEntity prev_geometry = entt::null;
    RenderingEntity prev_owner = entt::null;
    RS::InstanceType prev_base_type = RS::INSTANCE_MAX;

    int current_blend_mode = -1;

    uint32_t prev_shading = 0xFFFFFFFF;
    RasterizerSkeletonComponent *prev_skeleton = nullptr;

    state.scene_shader.set_conditional(SceneShaderGLES3::SHADELESS, true); //by default unshaded (easier to set)
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_SKELETON, false);

    bool first = true;
    bool prev_use_instancing = false;
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_INSTANCING, false);
    bool prev_octahedral_compression = false;
    state.scene_shader.set_conditional(SceneShaderGLES3::ENABLE_OCTAHEDRAL_COMPRESSION, false);

    get_rasterizer_storage_info().render.draw_call_count += p_elements.size();
    bool prev_opaque_prepass = false;
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_OPAQUE_PREPASS, false);

    for (RenderListElement *e : p_elements) {
        auto *instance = get<RenderingInstanceComponent>(e->instance);
        auto *material = get<RasterizerMaterialComponent>(e->material);
        RasterizerSkeletonComponent *skeleton = nullptr;
        assert(!VSG::ecs->registry.any_of<RenderingInstanceComponent>(e->instance) ||
               get<RenderingInstanceComponent>(e->instance)->self == e->instance
               );

        skeleton = get<RasterizerSkeletonComponent>(instance->skeleton);

        bool rebind = first;

        uint32_t shading = (e->sort_key >> RenderListConstants::SORT_KEY_SHADING_SHIFT) & RenderListConstants::SORT_KEY_SHADING_MASK;

        if (!p_shadow) {
            bool use_directional = directional_light != nullptr;

            if (p_directional_add) {
                auto *light_ptr = use_directional ? get<RasterizerLight3DComponent>(directional_light->light) : nullptr;
                ERR_FAIL_COND(!light_ptr);
                use_directional = use_directional && !(instance->baked_light && light_ptr->bake_mode == RS::LightBakeMode::LIGHT_BAKE_ALL);
                use_directional = use_directional && ((instance->layer_mask & light_ptr->cull_mask) != 0);
                use_directional = use_directional && ((e->sort_key & SORT_KEY_UNSHADED_FLAG) == 0);
                if (!use_directional) {
                    continue;
                }

            } else {
                use_directional = use_directional && (e->sort_key & SORT_KEY_NO_DIRECTIONAL_FLAG) == 0;
            }

            if (shading != prev_shading) {

                if (e->sort_key & SORT_KEY_UNSHADED_FLAG) {

                    state.scene_shader.set_conditional(SceneShaderGLES3::SHADELESS, true);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_FORWARD_LIGHTING, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_VERTEX_LIGHTING, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHT_DIRECTIONAL, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_DIRECTIONAL_SHADOW, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM4, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM2, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM_BLEND, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM_BLEND, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::SHADOW_MODE_PCF_5, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::SHADOW_MODE_PCF_13, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_GI_PROBES, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHTMAP_CAPTURE, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHTMAP, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHTMAP_LAYERED, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_RADIANCE_MAP, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_CONTACT_SHADOWS, false);

                } else {

                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_GI_PROBES, !instance->gi_probe_instances.empty());
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHTMAP, instance->lightmap!=entt::null && instance->gi_probe_instances.empty());
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHTMAP_CAPTURE, !instance->lightmap_capture_data.empty() && instance->lightmap==entt::null && instance->gi_probe_instances.empty());

                    state.scene_shader.set_conditional(SceneShaderGLES3::SHADELESS, false);

                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_FORWARD_LIGHTING, !p_directional_add);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_VERTEX_LIGHTING, (e->sort_key & SORT_KEY_VERTEX_LIT_FLAG));

                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHT_DIRECTIONAL, use_directional);
                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_DIRECTIONAL_SHADOW, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM4, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM2, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM_BLEND, false);
                    state.scene_shader.set_conditional(SceneShaderGLES3::SHADOW_MODE_PCF_5, shadow_filter_mode == SHADOW_FILTER_PCF5);
                    state.scene_shader.set_conditional(SceneShaderGLES3::SHADOW_MODE_PCF_13, shadow_filter_mode == SHADOW_FILTER_PCF13);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_RADIANCE_MAP, use_radiance_map);
                    state.scene_shader.set_conditional(SceneShaderGLES3::USE_CONTACT_SHADOWS, state.used_contact_shadows);

                    if (use_directional) {
                        auto *light_ptr = get<RasterizerLight3DComponent>(directional_light->light);
                        ERR_FAIL_COND(!light_ptr);

                        state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHT_DIRECTIONAL, true);

                        if (p_directional_shadows && light_ptr->shadow) {
                            state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_DIRECTIONAL_SHADOW, true);

                            switch (light_ptr->directional_shadow_mode) {
                                case RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL:
                                    break; //none
                                case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS:
                                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM2, true);
                                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM_BLEND, light_ptr->directional_blend_splits);
                                    break;
                                case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS:
                                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM4, true);
                                    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM_BLEND, light_ptr->directional_blend_splits);
                                    break;
                            }
                        }
                    }
                }

                rebind = true;
            }

            if (p_alpha_pass || p_directional_add) {
                int desired_blend_mode;
                if (p_directional_add) {
                    desired_blend_mode = RasterizerShaderComponent::Node3D::BLEND_MODE_ADD;
                } else {
                    assert(material->shader!=entt::null);
                    desired_blend_mode = getUnchecked<RasterizerShaderComponent>(material->shader)->spatial.blend_mode;
                }

                if (desired_blend_mode != current_blend_mode) {

                    switch (desired_blend_mode) {

                        case RasterizerShaderComponent::Node3D::BLEND_MODE_MIX: {
                            glBlendEquation(GL_FUNC_ADD);
                            if (current_rt && current_rt->flags[RS::RENDER_TARGET_TRANSPARENT]) {
                                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                            } else {
                                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                            }

                        } break;
                        case RasterizerShaderComponent::Node3D::BLEND_MODE_ADD: {

                            glBlendEquation(GL_FUNC_ADD);
                            glBlendFunc(p_alpha_pass ? GL_SRC_ALPHA : GL_ONE, GL_ONE);

                        } break;
                        case RasterizerShaderComponent::Node3D::BLEND_MODE_SUB: {

                            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                        } break;
                        case RasterizerShaderComponent::Node3D::BLEND_MODE_MUL: {
                            glBlendEquation(GL_FUNC_ADD);
                            if (current_rt && current_rt->flags[RS::RENDER_TARGET_TRANSPARENT]) {
                                glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
                            } else {
                                glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
                            }

                        } break;
                    }

                    current_blend_mode = desired_blend_mode;
                }
            }
        }

        bool use_opaque_prepass = e->sort_key & RenderListConstants::SORT_KEY_OPAQUE_PRE_PASS;

        if (use_opaque_prepass != prev_opaque_prepass) {
            state.scene_shader.set_conditional(SceneShaderGLES3::USE_OPAQUE_PREPASS, use_opaque_prepass);
            rebind = true;
        }

        bool use_instancing = instance->base_type == RS::INSTANCE_MULTIMESH || instance->base_type == RS::INSTANCE_PARTICLES;

        if (use_instancing != prev_use_instancing) {
            state.scene_shader.set_conditional(SceneShaderGLES3::USE_INSTANCING, use_instancing);
            rebind = true;
        }

        if (prev_skeleton != skeleton) {
            if ((prev_skeleton == nullptr) != (skeleton == nullptr)) {
                state.scene_shader.set_conditional(SceneShaderGLES3::USE_SKELETON, skeleton != nullptr);
                rebind = true;
            }

            if (skeleton) {
                glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 1);
                glBindTexture(GL_TEXTURE_2D, skeleton->texture);
            }
        }

        state.scene_shader.set_conditional(SceneShaderGLES3::USE_PHYSICAL_LIGHT_ATTENUATION, storage->config.use_physical_light_attenuation);
        auto *geom_surf = get<RasterizerSurfaceComponent>(e->geometry);
        bool octahedral_compression = instance->base_type != RS::INSTANCE_IMMEDIATE &&
                geom_surf->format & RS::ArrayFormat::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION &&
                !(geom_surf->blend_shapes.size() && instance->blend_values.size());
        if (octahedral_compression != prev_octahedral_compression) {
            state.scene_shader.set_conditional(SceneShaderGLES3::ENABLE_OCTAHEDRAL_COMPRESSION, octahedral_compression);
            rebind = true;
        }

        if (material != prev_material || rebind) {

            get_rasterizer_storage_info().render.material_switch_count++;

            rebind = _setup_material(material, use_opaque_prepass, p_alpha_pass);

            if (rebind) {
                get_rasterizer_storage_info().render.shader_rebind_count++;
            }
        }

        if (!(e->sort_key & SORT_KEY_UNSHADED_FLAG) && !p_directional_add && !p_shadow) {
            _setup_light(e, p_view_transform);
        }

        if (e->owner != prev_owner || prev_base_type != instance->base_type || prev_geometry != e->geometry) {

            _setup_geometry(e, p_view_transform);
            get_rasterizer_storage_info().render.surface_switch_count++;
        }

        _set_cull(e->sort_key & RenderListConstants::SORT_KEY_MIRROR_FLAG, e->sort_key & RenderListConstants::SORT_KEY_CULL_DISABLED_FLAG, p_reverse_cull);

        state.scene_shader.set_uniform(SceneShaderGLES3::WORLD_TRANSFORM, instance->transform);

        _render_geometry(e);

        prev_material = material;
        prev_base_type = instance->base_type;
        prev_geometry = e->geometry;
        prev_owner = e->owner;
        prev_shading = shading;
        prev_skeleton = skeleton;
        prev_use_instancing = use_instancing;
        prev_octahedral_compression = octahedral_compression;
        prev_opaque_prepass = use_opaque_prepass;
        first = false;
    }

    glBindVertexArray(0);

    state.scene_shader.set_conditional(SceneShaderGLES3::ENABLE_OCTAHEDRAL_COMPRESSION, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_INSTANCING, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_SKELETON, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_RADIANCE_MAP, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_FORWARD_LIGHTING, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHT_DIRECTIONAL, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_DIRECTIONAL_SHADOW, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM4, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM2, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::LIGHT_USE_PSSM_BLEND, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::SHADELESS, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::SHADOW_MODE_PCF_5, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::SHADOW_MODE_PCF_13, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_GI_PROBES, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHTMAP, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHTMAP_LAYERED, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHTMAP_CAPTURE, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_CONTACT_SHADOWS, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_VERTEX_LIGHTING, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_OPAQUE_PREPASS, false);
}

void _add_geometry_with_material(RasterizerSceneGLES3 *self,RenderingEntity p_geometry, RenderingEntity p_instance, RenderingEntity p_owner, RenderingEntity p_material, bool p_depth_pass, bool p_shadow_pass) {
    auto *instance = getUnchecked<RenderingInstanceComponent>(p_instance);
    auto *material = getUnchecked<RasterizerMaterialComponent>(p_material);
    assert(instance&&material);
    auto *shader = getUnchecked<RasterizerShaderComponent>(material->shader);

    bool has_base_alpha = (shader->spatial.uses_alpha && !shader->spatial.uses_alpha_scissor) || shader->spatial.uses_screen_texture || shader->spatial.uses_depth_texture;
    bool has_blend_alpha = shader->spatial.blend_mode != RasterizerShaderComponent::Node3D::BLEND_MODE_MIX;
    bool has_alpha = has_base_alpha || has_blend_alpha;

    bool mirror = instance->mirror;
    bool no_cull = false;

    if (shader->spatial.cull_mode == RasterizerShaderComponent::Node3D::CULL_MODE_DISABLED) {
        no_cull = true;
        mirror = false;
    } else if (shader->spatial.cull_mode == RasterizerShaderComponent::Node3D::CULL_MODE_FRONT) {
        mirror = !mirror;
    }

    if (shader->spatial.uses_sss) {
        self->state.used_sss = true;
    }

    if (shader->spatial.uses_screen_texture) {
        self->state.used_screen_texture = true;
    }

    if (shader->spatial.uses_depth_texture) {
        self->state.used_depth_texture = true;
    }

    if (p_depth_pass) {

        if (has_blend_alpha || shader->spatial.uses_depth_texture ||
                ((has_base_alpha || instance->cast_shadows == RS::SHADOW_CASTING_SETTING_OFF) &&
                        shader->spatial.depth_draw_mode !=
                                RasterizerShaderComponent::Node3D::DEPTH_DRAW_ALPHA_PREPASS) ||
                shader->spatial.depth_draw_mode == RasterizerShaderComponent::Node3D::DEPTH_DRAW_NEVER ||
                shader->spatial.no_depth_test) {
            return; //bye
        }
        if (!p_shadow_pass && !shader->shader->is_custom_code_ready_for_render(shader->custom_code_id)) {
            // The shader is not guaranteed to be able to render (i.e., a not yet ready async hidden one);
            // skip depth rendering because otherwise we risk masking out pixels that won't get written to at the actual render pass
            return;
        }

        if (!shader->spatial.uses_alpha_scissor && !shader->spatial.writes_modelview_or_projection && !shader->spatial.uses_vertex && !shader->spatial.uses_discard && shader->spatial.depth_draw_mode != RasterizerShaderComponent::Node3D::DEPTH_DRAW_ALPHA_PREPASS) {
            //shader does not use discard and does not write a vertex position, use generic material
            RenderingEntity material_ent;
            if (instance->cast_shadows == RS::SHADOW_CASTING_SETTING_DOUBLE_SIDED) {
                material_ent = !p_shadow_pass && shader->spatial.uses_world_coordinates ? self->default_worldcoord_material_twosided : self->default_material_twosided;
                no_cull = true;
                mirror = false;
            } else {
                material_ent = !p_shadow_pass && shader->spatial.uses_world_coordinates ? self->default_worldcoord_material : self->default_material;
            }
            material = getUnchecked<RasterizerMaterialComponent>(material_ent);
            p_material = material_ent;
        }

        has_alpha = false;
    }

    RenderListElement *e = (has_alpha || shader->spatial.no_depth_test) ? self->render_list.add_alpha_element(instance->depth) : self->render_list.add_element(instance->depth);

    if (!e) {
        return;
    }

    e->geometry = p_geometry;
    e->material = p_material;
    e->instance = p_instance;
    e->owner = p_owner;
    e->sort_key = 0;
    auto * geom_data = get<RasterizerCommonGeometryComponent>(p_geometry);
    if (geom_data->last_pass != self->render_pass) {
        geom_data->last_pass = self->render_pass;
        geom_data->index = self->current_geometry_index++;
    }
    if(!p_depth_pass && self->directional_light) {
        auto *directional = get<RasterizerLight3DComponent>(self->directional_light->light);
        if ((directional->cull_mask & instance->layer_mask) == 0) {
            e->sort_key |= SORT_KEY_NO_DIRECTIONAL_FLAG;
        }
    }

    e->sort_key |= uint64_t(geom_data->index) << RenderListConstants::SORT_KEY_GEOMETRY_INDEX_SHIFT;
    e->sort_key |= uint64_t(instance->base_type) << RenderListConstants::SORT_KEY_GEOMETRY_TYPE_SHIFT;

    if (material->last_pass != self->render_pass) {
        material->last_pass = self->render_pass;
        material->index = self->current_material_index++;
    }

    e->sort_key |= uint64_t(material->index) << RenderListConstants::SORT_KEY_MATERIAL_INDEX_SHIFT;
    e->sort_key |= uint64_t(instance->depth_layer) << RenderListConstants::SORT_KEY_OPAQUE_DEPTH_LAYER_SHIFT;

    if (!p_depth_pass) {

        if (!instance->gi_probe_instances.empty()) {
            e->sort_key |= SORT_KEY_GI_PROBES_FLAG;
        }

        if (instance->lightmap!=entt::null) {
            e->sort_key |= SORT_KEY_LIGHTMAP_FLAG;
            if (instance->lightmap_slice != -1) {
                e->sort_key |= SORT_KEY_LIGHTMAP_LAYERED_FLAG;
            }
        }

        if (!instance->lightmap_capture_data.empty()) {
            e->sort_key |= SORT_KEY_LIGHTMAP_CAPTURE_FLAG;
        }

        e->sort_key |= (uint64_t(material->render_priority) + 128) << RenderListConstants::SORT_KEY_PRIORITY_SHIFT;
    }

    /*
    if (geometry.type==RasterizerStorageGLES3::Geometry::GEOMETRY_MULTISURFACE)
        e->sort_flags|=RenderList::SORT_FLAG_INSTANCING;
    */

    if (mirror) {
        e->sort_key |= RenderListConstants::SORT_KEY_MIRROR_FLAG;
    }

    if (no_cull) {
        e->sort_key |= RenderListConstants::SORT_KEY_CULL_DISABLED_FLAG;
    }

    //e->light_type=0xFF; // no lights!

    if (p_depth_pass || shader->spatial.unshaded || self->state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_UNSHADED) {
        e->sort_key |= SORT_KEY_UNSHADED_FLAG;
    }

    if (p_depth_pass && shader->spatial.depth_draw_mode == RasterizerShaderComponent::Node3D::DEPTH_DRAW_ALPHA_PREPASS) {
        e->sort_key |= RenderListConstants::SORT_KEY_OPAQUE_PRE_PASS;
    }

    if (!p_depth_pass && (shader->spatial.uses_vertex_lighting || self->storage->config.force_vertex_shading)) {

        e->sort_key |= SORT_KEY_VERTEX_LIT_FLAG;
    }

    if (shader->spatial.uses_time) {
        RenderingServerRaster::redraw_request(false);
    }
}
void _add_geometry(RasterizerSceneGLES3 *self,RenderingEntity p_geometry, RenderingInstanceComponent *p_instance, RenderingEntity p_owner, int p_material, bool p_depth_pass, bool p_shadow_pass) {

    RasterizerMaterialComponent *m = nullptr;
    RasterizerCommonGeometryComponent *geom = get<RasterizerCommonGeometryComponent>(p_geometry);
    RenderingEntity m_src = (p_instance->material_override != entt::null) ?
                    p_instance->material_override.value :
                    (p_material >= 0 ? p_instance->materials[p_material] : geom->material.value);

    if (self->state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_OVERDRAW) {
        m_src = self->default_overdraw_material;
    }

    if (m_src!=entt::null) {
        m = get<RasterizerMaterialComponent>(m_src);
        auto *shader = m!=nullptr ? get<RasterizerShaderComponent>(m->shader) : nullptr;

        if (!shader || !shader->valid) {
            m = nullptr;
        }
    }

    if (!m) {
        m = get<RasterizerMaterialComponent>(self->default_material);
    }

    ERR_FAIL_COND(!m);

    _add_geometry_with_material(self, p_geometry, p_instance->self, p_owner, m->self, p_depth_pass, p_shadow_pass);

    while (m->next_pass!=entt::null) {
        m = get<RasterizerMaterialComponent>(m->next_pass);
        auto *shader = m ? get<RasterizerShaderComponent>(m->shader) : nullptr;
        if (!shader || !shader->valid)
            break;
        _add_geometry_with_material(self, p_geometry, p_instance->self, p_owner, m->self, p_depth_pass, p_shadow_pass);
    }

    // Repeat the "nested chain" logic also for the overlay
    if (p_instance->material_overlay!=entt::null) {
        m = get<RasterizerMaterialComponent>(p_instance->material_overlay);
        auto *shader = m!=nullptr ? get<RasterizerShaderComponent>(m->shader) : nullptr;

        if (!shader || !shader->valid) {
            return;
        }

        _add_geometry_with_material(self, p_geometry, p_instance->self, p_owner, m->self, p_depth_pass, p_shadow_pass);

        while (m->next_pass!=entt::null) {
            m = get<RasterizerMaterialComponent>(m->next_pass);
            auto *shader = m ? get<RasterizerShaderComponent>(m->shader) : nullptr;
            if (!shader || !shader->valid) {
                break;
            }
            _add_geometry_with_material(self, p_geometry, p_instance->self, p_owner, m->self, p_depth_pass, p_shadow_pass);
        }
    }
}

void RasterizerSceneGLES3::_draw_sky(RasterizerSkyComponent *p_sky, const CameraMatrix &p_projection, const Transform &p_transform, bool p_vflip, float p_custom_fov, float p_energy, const Basis &p_sky_orientation) {

    ERR_FAIL_COND(!p_sky);

    RasterizerTextureComponent *tex = get<RasterizerTextureComponent>(p_sky->panorama);

    ERR_FAIL_COND(!tex);
    glActiveTexture(GL_TEXTURE0);

    tex = tex->get_ptr(); //resolve for proxies

    glBindTexture(tex->target, tex->get_texture_id());

    if (storage->config.srgb_decode_supported && tex->srgb && !tex->using_srgb) {

        glTexParameteri(tex->target, _TEXTURE_SRGB_DECODE_EXT, _DECODE_EXT);
        tex->using_srgb = true;
#ifdef TOOLS_ENABLED
        if (!(tex->flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) {
            tex->flags |= RS::TEXTURE_FLAG_CONVERT_TO_LINEAR;
            //notify that texture must be set to linear beforehand, so it works in other platforms when exported
        }
#endif
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LEQUAL);
    glColorMask(1, 1, 1, 1);

    // Camera
    CameraMatrix camera;

    if (p_custom_fov) {

        float near_plane = p_projection.get_z_near();
        float far_plane = p_projection.get_z_far();
        float aspect = p_projection.get_aspect();

        camera.set_perspective(p_custom_fov, aspect, near_plane, far_plane);

    } else {
        camera = p_projection;
    }

    float flip_sign = p_vflip ? -1 : 1;

    /*
        If matrix[2][0] or matrix[2][1] we're dealing with an asymmetrical projection matrix. This is the case for stereoscopic rendering (i.e. VR).
        To ensure the image rendered is perspective correct we need to move some logic into the shader. For this the USE_ASYM_PANO option is introduced.
        It also means the uv coordinates are ignored in this mode and we don't need our loop.
    */
    bool asymmetrical = ((camera.matrix[2][0] != 0.0) || (camera.matrix[2][1] != 0.0));

    Vector3 vertices[8] = {
        Vector3(-1, -1 * flip_sign, 1),
        Vector3(0, 1, 0),
        Vector3(1, -1 * flip_sign, 1),
        Vector3(1, 1, 0),
        Vector3(1, 1 * flip_sign, 1),
        Vector3(1, 0, 0),
        Vector3(-1, 1 * flip_sign, 1),
        Vector3(0, 0, 0)
    };

    if (!asymmetrical) {
        Vector2 vp_he = camera.get_viewport_half_extents();
        float zn = p_projection.get_z_near();

        for (int i = 0; i < 4; i++) {
            Vector3 uv = vertices[i * 2 + 1];
            uv.x = (uv.x * 2.0 - 1.0) * vp_he.x;
            uv.y = -(uv.y * 2.0 - 1.0) * vp_he.y;
            uv.z = -zn;
            vertices[i * 2 + 1] = p_transform.basis.xform(uv).normalized();
            vertices[i * 2 + 1].z = -vertices[i * 2 + 1].z;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, state.sky_verts);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vector3) * 8, vertices, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

    glBindVertexArray(state.sky_array);

    storage->shaders.copy.set_conditional(CopyShaderGLES3::USE_ASYM_PANO, asymmetrical);
    storage->shaders.copy.set_conditional(CopyShaderGLES3::USE_PANORAMA, !asymmetrical);
    storage->shaders.copy.set_conditional(CopyShaderGLES3::USE_MULTIPLIER, true);
    storage->shaders.copy.bind();

    storage->shaders.copy.set_uniform(CopyShaderGLES3::MULTIPLIER, p_energy);

    // don't know why but I always have problems setting a uniform mat3, so we're using a transform
    storage->shaders.copy.set_uniform(CopyShaderGLES3::SKY_TRANSFORM, Transform(p_sky_orientation, Vector3(0.0, 0.0, 0.0)).affine_inverse());

    if (asymmetrical) {
        // pack the bits we need from our projection matrix
        storage->shaders.copy.set_uniform(CopyShaderGLES3::ASYM_PROJ, camera.matrix[2][0], camera.matrix[0][0], camera.matrix[2][1], camera.matrix[1][1]);
        ///@TODO I couldn't get mat3 + p_transform.basis to work, that would be better here.
        storage->shaders.copy.set_uniform(CopyShaderGLES3::PANO_TRANSFORM, p_transform);
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindVertexArray(0);
    glColorMask(1, 1, 1, 1);

    storage->shaders.copy.set_conditional(CopyShaderGLES3::USE_ASYM_PANO, false);
    storage->shaders.copy.set_conditional(CopyShaderGLES3::USE_MULTIPLIER, false);
    storage->shaders.copy.set_conditional(CopyShaderGLES3::USE_PANORAMA, false);
}

static void _setup_environment(RasterizerSceneGLES3 *self,RasterizerEnvironmentComponent *env, const CameraMatrix &p_cam_projection, const Transform &p_cam_transform, const int p_eye=0, bool p_no_fog=false) {
    Transform sky_orientation;

    //store camera into ubo
    store_camera(p_cam_projection, self->state.ubo_data.projection_matrix);
    store_camera(p_cam_projection.inverse(), self->state.ubo_data.inv_projection_matrix);
    store_transform(p_cam_transform, self->state.ubo_data.camera_matrix);
    store_transform(p_cam_transform.affine_inverse(), self->state.ubo_data.camera_inverse_matrix);

    //time global variables
    self->state.ubo_data.time = self->storage->frame.time[0];
    // eye we are rendering
    self->state.ubo_data.view_index = p_eye == 2 ? 1 : 0;

    self->state.ubo_data.z_far = p_cam_projection.get_z_far();
    //bg and ambient
    if (env) {
        self->state.ubo_data.bg_energy = env->bg_energy;
        self->state.ubo_data.ambient_energy = env->ambient_energy;
        Color linear_ambient_color = env->ambient_color.to_linear();
        self->state.ubo_data.ambient_light_color[0] = linear_ambient_color.r;
        self->state.ubo_data.ambient_light_color[1] = linear_ambient_color.g;
        self->state.ubo_data.ambient_light_color[2] = linear_ambient_color.b;
        self->state.ubo_data.ambient_light_color[3] = linear_ambient_color.a;

        Color bg_color;

        switch (env->bg_mode) {
            case RS::ENV_BG_CLEAR_COLOR: {
                bg_color = self->storage->frame.clear_request_color.to_linear();
            } break;
            case RS::ENV_BG_COLOR: {
                bg_color = env->bg_color.to_linear();
            } break;
            default: {
                bg_color = Color(0, 0, 0, 1);
            } break;
        }

        self->state.ubo_data.bg_color[0] = bg_color.r;
        self->state.ubo_data.bg_color[1] = bg_color.g;
        self->state.ubo_data.bg_color[2] = bg_color.b;
        self->state.ubo_data.bg_color[3] = bg_color.a;

        //use the inverse of our sky_orientation, we may need to skip this if we're using a reflection probe?
        sky_orientation = Transform(env->sky_orientation, Vector3(0.0, 0.0, 0.0)).affine_inverse();

        self->state.env_radiance_data.ambient_contribution = env->ambient_sky_contribution;
        self->state.ubo_data.ambient_occlusion_affect_light = env->ssao_light_affect;
        self->state.ubo_data.ambient_occlusion_affect_ssao = env->ssao_ao_channel_affect;

        //fog

        Color linear_fog = env->fog_color.to_linear();
        self->state.ubo_data.fog_color_enabled[0] = linear_fog.r;
        self->state.ubo_data.fog_color_enabled[1] = linear_fog.g;
        self->state.ubo_data.fog_color_enabled[2] = linear_fog.b;
        self->state.ubo_data.fog_color_enabled[3] = (!p_no_fog && env->fog_enabled) ? 1.0 : 0.0;
        self->state.ubo_data.fog_density = linear_fog.a;

        Color linear_sun = env->fog_sun_color.to_linear();
        self->state.ubo_data.fog_sun_color_amount[0] = linear_sun.r;
        self->state.ubo_data.fog_sun_color_amount[1] = linear_sun.g;
        self->state.ubo_data.fog_sun_color_amount[2] = linear_sun.b;
        self->state.ubo_data.fog_sun_color_amount[3] = env->fog_sun_amount;
        self->state.ubo_data.fog_depth_enabled = env->fog_depth_enabled;
        self->state.ubo_data.fog_depth_begin = env->fog_depth_begin;
        self->state.ubo_data.fog_depth_end = env->fog_depth_end;
        self->state.ubo_data.fog_depth_curve = env->fog_depth_curve;
        self->state.ubo_data.fog_transmit_enabled = env->fog_transmit_enabled;
        self->state.ubo_data.fog_transmit_curve = env->fog_transmit_curve;
        self->state.ubo_data.fog_height_enabled = env->fog_height_enabled;
        self->state.ubo_data.fog_height_min = env->fog_height_min;
        self->state.ubo_data.fog_height_max = env->fog_height_max;
        self->state.ubo_data.fog_height_curve = env->fog_height_curve;

    } else {
        self->state.ubo_data.bg_energy = 1.0;
        self->state.ubo_data.ambient_energy = 1.0;
        //use from clear color instead, since there is no ambient
        Color linear_ambient_color = self->storage->frame.clear_request_color.to_linear();
        self->state.ubo_data.ambient_light_color[0] = linear_ambient_color.r;
        self->state.ubo_data.ambient_light_color[1] = linear_ambient_color.g;
        self->state.ubo_data.ambient_light_color[2] = linear_ambient_color.b;
        self->state.ubo_data.ambient_light_color[3] = linear_ambient_color.a;

        self->state.ubo_data.bg_color[0] = linear_ambient_color.r;
        self->state.ubo_data.bg_color[1] = linear_ambient_color.g;
        self->state.ubo_data.bg_color[2] = linear_ambient_color.b;
        self->state.ubo_data.bg_color[3] = linear_ambient_color.a;

        self->state.env_radiance_data.ambient_contribution = 0;
        self->state.ubo_data.ambient_occlusion_affect_light = 0;

        self->state.ubo_data.fog_color_enabled[3] = 0.0;
    }

    {
        //directional shadow

        self->state.ubo_data.shadow_directional_pixel_size[0] = 1.0f / self->directional_shadow.size;
        self->state.ubo_data.shadow_directional_pixel_size[1] = 1.0f / self->directional_shadow.size;

        glActiveTexture(GL_TEXTURE0 + self->storage->config.max_texture_image_units - 5);
        glBindTexture(GL_TEXTURE_2D, self->directional_shadow.depth);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, self->state.scene_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(RasterizerSceneGLES3::State::SceneDataUBO), &self->state.ubo_data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //fill up environment

    store_transform(sky_orientation * p_cam_transform, self->state.env_radiance_data.transform);

    glBindBuffer(GL_UNIFORM_BUFFER, self->state.env_radiance_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(RasterizerSceneGLES3::State::EnvironmentRadianceUBO), &self->state.env_radiance_data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void RasterizerSceneGLES3::_setup_reflections(RenderingEntity *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, const Transform &p_camera_inverse_transform, const CameraMatrix &p_camera_projection, RenderingEntity p_reflection_atlas, RasterizerEnvironmentComponent *p_env) {

    state.reflection_probe_count = 0;

    for (int i = 0; i < p_reflection_probe_cull_count; i++) {

        auto *rpi = get<RasterizerReflectionProbeInstanceComponent>(p_reflection_probe_cull_result[i]);
        ERR_CONTINUE(!rpi);

        auto *reflection_atlas = get<RasterizerReflectionAtlasComponent>(p_reflection_atlas);
        ERR_CONTINUE(!reflection_atlas);

        ERR_CONTINUE(rpi->reflection_atlas_index < 0);

        if (state.reflection_probe_count >= state.max_ubo_reflections) {
            break;
        }

        rpi->last_pass = render_pass;

        auto probe_ptr = getUnchecked<RasterizerReflectionProbeComponent>(rpi->probe);
        ReflectionProbeDataUBO reflection_ubo;

        reflection_ubo.box_extents[0] = probe_ptr->extents.x;
        reflection_ubo.box_extents[1] = probe_ptr->extents.y;
        reflection_ubo.box_extents[2] = probe_ptr->extents.z;
        reflection_ubo.box_extents[3] = 0;

        reflection_ubo.box_ofs[0] = probe_ptr->origin_offset.x;
        reflection_ubo.box_ofs[1] = probe_ptr->origin_offset.y;
        reflection_ubo.box_ofs[2] = probe_ptr->origin_offset.z;
        reflection_ubo.box_ofs[3] = 0;

        reflection_ubo.params[0] = probe_ptr->intensity;
        reflection_ubo.params[1] = 0;
        reflection_ubo.params[2] = probe_ptr->interior ? 1.0 : 0.0;
        reflection_ubo.params[3] = probe_ptr->box_projection ? 1.0 : 0.0;

        if (probe_ptr->interior) {
            Color ambient_linear = probe_ptr->interior_ambient.to_linear();
            reflection_ubo.ambient[0] = ambient_linear.r * probe_ptr->interior_ambient_energy;
            reflection_ubo.ambient[1] = ambient_linear.g * probe_ptr->interior_ambient_energy;
            reflection_ubo.ambient[2] = ambient_linear.b * probe_ptr->interior_ambient_energy;
            reflection_ubo.ambient[3] = probe_ptr->interior_ambient_probe_contrib;
        } else {
            Color ambient_linear;
            if (p_env) {
                ambient_linear = p_env->ambient_color.to_linear();
                ambient_linear.r *= p_env->ambient_energy;
                ambient_linear.g *= p_env->ambient_energy;
                ambient_linear.b *= p_env->ambient_energy;
            }

            reflection_ubo.ambient[0] = ambient_linear.r;
            reflection_ubo.ambient[1] = ambient_linear.g;
            reflection_ubo.ambient[2] = ambient_linear.b;
            reflection_ubo.ambient[3] = 0; //not used in exterior mode, since it just blends with regular ambient light
        }

        int cell_size = reflection_atlas->size / reflection_atlas->subdiv;
        int x = (rpi->reflection_atlas_index % reflection_atlas->subdiv) * cell_size;
        int y = (rpi->reflection_atlas_index / reflection_atlas->subdiv) * cell_size;
        int width = cell_size;
        int height = cell_size;

        reflection_ubo.atlas_clamp[0] = float(x) / reflection_atlas->size;
        reflection_ubo.atlas_clamp[1] = float(y) / reflection_atlas->size;
        reflection_ubo.atlas_clamp[2] = float(width) / reflection_atlas->size;
        reflection_ubo.atlas_clamp[3] = float(height) / reflection_atlas->size;

        Transform proj = (p_camera_inverse_transform * rpi->transform).inverse();
        store_transform(proj, reflection_ubo.local_matrix);

        rpi->reflection_index = state.reflection_probe_count;
        memcpy(&state.reflection_array_tmp[rpi->reflection_index * sizeof(ReflectionProbeDataUBO)], &reflection_ubo, sizeof(ReflectionProbeDataUBO));
        state.reflection_probe_count++;
    }

    if (state.reflection_probe_count) {

        glBindBuffer(GL_UNIFORM_BUFFER, state.reflection_array_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, state.reflection_probe_count * sizeof(ReflectionProbeDataUBO), state.reflection_array_tmp);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, 6, state.reflection_array_ubo);
}

void RasterizerSceneGLES3::_copy_screen(bool p_invalidate_color, bool p_invalidate_depth) {

    glBindVertexArray(storage->resources.quadie_array);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void RasterizerSceneGLES3::_copy_texture_to_front_buffer(GLuint p_texture) {

    auto * current_rt = get<RasterizerRenderTargetComponent>(storage->frame.current_rt);
    //copy to front buffer
    glBindFramebuffer(GL_FRAMEBUFFER, current_rt->fbo);

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LEQUAL);
    glColorMask(1, 1, 1, 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, p_texture);

    glViewport(0, 0, current_rt->width * 0.5f, current_rt->height * 0.5f);

    storage->shaders.copy.set_conditional(CopyShaderGLES3::DISABLE_ALPHA, true);
    storage->shaders.copy.bind();

    _copy_screen();

    //turn off everything used
    storage->shaders.copy.set_conditional(CopyShaderGLES3::LINEAR_TO_SRGB, false);
    storage->shaders.copy.set_conditional(CopyShaderGLES3::DISABLE_ALPHA, false);
}

void RasterizerSceneGLES3::_fill_render_list(Span<RenderingEntity> p_cull_results, bool p_depth_pass, bool p_shadow_pass) {

    current_geometry_index = 0;
    current_material_index = 0;
    state.used_sss = false;
    state.used_screen_texture = false;
    state.used_depth_texture = false;

    //fill list

    for (int i = 0; i < p_cull_results.size(); i++) {
        RenderingInstanceComponent *inst = get<RenderingInstanceComponent>(p_cull_results[i]);
        switch (inst->base_type) {

            case RS::INSTANCE_MESH: {

                auto *mesh = get<RasterizerMeshComponent>(inst->base);
                ERR_CONTINUE(!mesh);

                int ssize = mesh->surfaces.size();

                for (int j = 0; j < ssize; j++) {

                    int mat_idx = inst->materials[j]!=entt::null ? j : -1;
                    _add_geometry(this, mesh->surfaces[j], inst, entt::null, mat_idx, p_depth_pass, p_shadow_pass);
                }

                //mesh->last_pass=frame;

            } break;
            case RS::INSTANCE_MULTIMESH: {

                auto *multi_mesh = get<RasterizerMultiMeshComponent>(inst->base);
                ERR_CONTINUE(!multi_mesh);

                if (multi_mesh->size == 0 || multi_mesh->visible_instances == 0) {
                    continue;
                }

                auto *mesh = get<RasterizerMeshComponent>(multi_mesh->mesh);
                if (!mesh) {
                    continue; //mesh not assigned
                }

                int ssize = mesh->surfaces.size();

                for (int j = 0; j < ssize; j++) {

                    _add_geometry(this, mesh->surfaces[j], inst, inst->base, -1, p_depth_pass, p_shadow_pass);
                }

            } break;
            case RS::INSTANCE_IMMEDIATE: {

                auto *immediate = get<RasterizerImmediateGeometryComponent>(inst->base);
                ERR_CONTINUE(!immediate);

                _add_geometry(this, inst->base, inst, entt::null, -1, p_depth_pass, p_shadow_pass);

            } break;
            case RS::INSTANCE_PARTICLES: {

                auto *particles = get<RasterizerParticlesComponent>(inst->base);
                ERR_CONTINUE(!particles);

                for (int j = 0; j < particles->draw_passes.size(); j++) {

                    RenderingEntity pmesh = particles->draw_passes[j];
                    if (pmesh==entt::null) {
                        continue;
                    }
                    RasterizerMeshComponent *mesh = get<RasterizerMeshComponent>(pmesh);
                    if (!mesh)
                        continue; //mesh not assigned

                    int ssize = mesh->surfaces.size();

                    for (int k = 0; k < ssize; k++) {

                        _add_geometry(this, mesh->surfaces[k], inst, inst->base, -1, p_depth_pass, p_shadow_pass);
                    }
                }

            } break;
            default: {
            }
        }
    }
}

void RasterizerSceneGLES3::_blur_effect_buffer() {

    auto * current_rt = get<RasterizerRenderTargetComponent>(storage->frame.current_rt);
    //blur diffuse into effect mipmaps using separatable convolution
    //storage->shaders.copy.set_conditional(CopyShaderGLES3::GAUSSIAN_HORIZONTAL,true);
    for (int i = 0; i < current_rt->effects.mip_maps[1].sizes.size(); i++) {

        int vp_w = current_rt->effects.mip_maps[1].sizes[i].width;
        int vp_h = current_rt->effects.mip_maps[1].sizes[i].height;
        glViewport(0, 0, vp_w, vp_h);
        //horizontal pass
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GAUSSIAN_HORIZONTAL, true);
        state.effect_blur_shader.bind();
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::PIXEL_SIZE, Vector2(1.0f / vp_w, 1.0f / vp_h));
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::LOD, float(i));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[0].color); //previous level, since mipmaps[0] starts one level bigger
        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[1].sizes[i].fbo);
        _copy_screen(true);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GAUSSIAN_HORIZONTAL, false);

        //vertical pass
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GAUSSIAN_VERTICAL, true);
        state.effect_blur_shader.bind();
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::PIXEL_SIZE, Vector2(1.0f / vp_w, 1.0f / vp_h));
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::LOD, float(i));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[1].color);
        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[i + 1].fbo); //next level, since mipmaps[0] starts one level bigger
        _copy_screen(true);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GAUSSIAN_VERTICAL, false);
    }
}

void RasterizerSceneGLES3::_prepare_depth_texture() {
    auto *current_rt = get<RasterizerRenderTargetComponent>(storage->frame.current_rt);
    if (!state.prepared_depth_texture) {
        //resolve depth buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, current_rt->buffers.fbo);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_rt->fbo);
        glBlitFramebuffer(0, 0, current_rt->width, current_rt->height, 0, 0, current_rt->width, current_rt->height,
                GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        state.prepared_depth_texture = true;
    }
}

void RasterizerSceneGLES3::_bind_depth_texture() {
    if (!state.bound_depth_texture) {
        auto * current_rt = get<RasterizerRenderTargetComponent>(storage->frame.current_rt);
        ERR_FAIL_COND(!state.prepared_depth_texture);
        //bind depth for read
        glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 9);
        glBindTexture(GL_TEXTURE_2D, current_rt->depth);
        state.bound_depth_texture = true;
    }
}

void RasterizerSceneGLES3::_render_mrts(RasterizerEnvironmentComponent *env, const CameraMatrix &p_cam_projection) {
    auto * current_rt = get<RasterizerRenderTargetComponent>(storage->frame.current_rt);

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    _prepare_depth_texture();

    if (env->ssao_enabled || env->ssr_enabled) {

        //copy normal and roughness to effect buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, current_rt->buffers.fbo);
        glReadBuffer(GL_COLOR_ATTACHMENT2);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_rt->buffers.effect_fbo);
        glBlitFramebuffer(0, 0, current_rt->width, current_rt->height, 0, 0, current_rt->width, current_rt->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    if (env->ssao_enabled) {
        //copy diffuse to front buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, current_rt->buffers.fbo);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_rt->fbo);
        glBlitFramebuffer(0, 0, current_rt->width, current_rt->height, 0, 0, current_rt->width, current_rt->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        //copy from depth, convert to linear
        GLint ss[2];
        ss[0] = current_rt->width;
        ss[1] = current_rt->height;

        for (int i = 0; i < current_rt->effects.ssao.depth_mipmap_fbos.size(); i++) {

            state.ssao_minify_shader.set_conditional(SsaoMinifyShaderGLES3::MINIFY_START, i == 0);
            state.ssao_minify_shader.set_conditional(SsaoMinifyShaderGLES3::USE_ORTHOGONAL_PROJECTION, p_cam_projection.is_orthogonal());
            state.ssao_minify_shader.bind();
            state.ssao_minify_shader.set_uniform(SsaoMinifyShaderGLES3::CAMERA_Z_FAR, p_cam_projection.get_z_far());
            state.ssao_minify_shader.set_uniform(SsaoMinifyShaderGLES3::CAMERA_Z_NEAR, p_cam_projection.get_z_near());
            state.ssao_minify_shader.set_uniform(SsaoMinifyShaderGLES3::SOURCE_MIPMAP, M_MAX(0, i - 1));
            glUniform2iv(state.ssao_minify_shader.get_uniform(SsaoMinifyShaderGLES3::FROM_SIZE), 1, ss);
            ss[0] >>= 1;
            ss[1] >>= 1;

            glActiveTexture(GL_TEXTURE0);
            if (i == 0) {
                glBindTexture(GL_TEXTURE_2D, current_rt->depth);
            } else {
                glBindTexture(GL_TEXTURE_2D, current_rt->effects.ssao.linear_depth);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.ssao.depth_mipmap_fbos[i]); //copy to front first
            glViewport(0, 0, ss[0], ss[1]);

            _copy_screen(true);
        }
        ss[0] = current_rt->width;
        ss[1] = current_rt->height;

        glViewport(0, 0, ss[0], ss[1]);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_GREATER);
        // do SSAO!
        state.ssao_shader.set_conditional(SsaoShaderGLES3::ENABLE_RADIUS2, env->ssao_radius2 > 0.001);
        state.ssao_shader.set_conditional(SsaoShaderGLES3::USE_ORTHOGONAL_PROJECTION, p_cam_projection.is_orthogonal());
        state.ssao_shader.set_conditional(SsaoShaderGLES3::SSAO_QUALITY_LOW, env->ssao_quality == RS::ENV_SSAO_QUALITY_LOW);
        state.ssao_shader.set_conditional(SsaoShaderGLES3::SSAO_QUALITY_HIGH, env->ssao_quality == RS::ENV_SSAO_QUALITY_HIGH);
        state.ssao_shader.bind();
        state.ssao_shader.set_uniform(SsaoShaderGLES3::CAMERA_Z_FAR, p_cam_projection.get_z_far());
        state.ssao_shader.set_uniform(SsaoShaderGLES3::CAMERA_Z_NEAR, p_cam_projection.get_z_near());
        glUniform2iv(state.ssao_shader.get_uniform(SsaoShaderGLES3::SCREEN_SIZE), 1, ss);
        float radius = env->ssao_radius;
        state.ssao_shader.set_uniform(SsaoShaderGLES3::RADIUS, radius);
        float intensity = env->ssao_intensity;
        state.ssao_shader.set_uniform(SsaoShaderGLES3::INTENSITY_DIV_R6, intensity / pow(radius, 6.0f));

        if (env->ssao_radius2 > 0.001) {

            float radius2 = env->ssao_radius2;
            state.ssao_shader.set_uniform(SsaoShaderGLES3::RADIUS2, radius2);
            float intensity2 = env->ssao_intensity2;
            state.ssao_shader.set_uniform(SsaoShaderGLES3::INTENSITY_DIV_R62, intensity2 / pow(radius2, 6.0f));
        }

        float proj_info[4] = {
            -2.0f / (ss[0] * p_cam_projection.matrix[0][0]),
            -2.0f / (ss[1] * p_cam_projection.matrix[1][1]),
            (1.0f - p_cam_projection.matrix[0][2]) / p_cam_projection.matrix[0][0],
            (1.0f + p_cam_projection.matrix[1][2]) / p_cam_projection.matrix[1][1]
        };

        glUniform4fv(state.ssao_shader.get_uniform(SsaoShaderGLES3::PROJ_INFO), 1, proj_info);
        float pixels_per_meter = float(p_cam_projection.get_pixels_per_meter(ss[0]));

        state.ssao_shader.set_uniform(SsaoShaderGLES3::PROJ_SCALE, pixels_per_meter);
        state.ssao_shader.set_uniform(SsaoShaderGLES3::BIAS, env->ssao_bias);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->depth);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.ssao.linear_depth);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, current_rt->buffers.effect);

        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.ssao.blur_fbo[0]); //copy to front first
        Color white(1, 1, 1, 1);
        glClearBufferfv(GL_COLOR, 0, &white.r); // specular

        _copy_screen(true);

        //do the batm, i mean blur

        state.ssao_blur_shader.bind();

        if (env->ssao_filter) {
            for (int i = 0; i < 2; i++) {

                state.ssao_blur_shader.set_uniform(SsaoBlurShaderGLES3::CAMERA_Z_FAR, p_cam_projection.get_z_far());
                state.ssao_blur_shader.set_uniform(SsaoBlurShaderGLES3::CAMERA_Z_NEAR, p_cam_projection.get_z_near());
                state.ssao_blur_shader.set_uniform(SsaoBlurShaderGLES3::EDGE_SHARPNESS, env->ssao_bilateral_sharpness);
                state.ssao_blur_shader.set_uniform(SsaoBlurShaderGLES3::FILTER_SCALE, int(env->ssao_filter));

                GLint axis[2] = { i, 1 - i };
                glUniform2iv(state.ssao_blur_shader.get_uniform(SsaoBlurShaderGLES3::AXIS), 1, axis);
                glUniform2iv(state.ssao_blur_shader.get_uniform(SsaoBlurShaderGLES3::SCREEN_SIZE), 1, ss);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, current_rt->effects.ssao.blur_red[i]);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, current_rt->depth);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, current_rt->buffers.effect);
                glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.ssao.blur_fbo[1 - i]);
                if (i == 0) {
                    glClearBufferfv(GL_COLOR, 0, &white.r); // specular
                }
                _copy_screen(true);
            }
        }

        glDisable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // just copy diffuse while applying SSAO

        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::SSAO_MERGE, true);
        state.effect_blur_shader.bind();
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::SSAO_COLOR, env->ssao_color);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->color); //previous level, since mipmaps[0] starts one level bigger
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.ssao.blur_red[0]); //previous level, since mipmaps[0] starts one level bigger
        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[0].fbo); // copy to base level
        _copy_screen(true);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::SSAO_MERGE, false);

    } else {

        //copy diffuse to effect buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, current_rt->buffers.fbo);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[0].fbo);
        glBlitFramebuffer(0, 0, current_rt->width, current_rt->height, 0, 0, current_rt->width, current_rt->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    if (state.used_sss) { //sss enabled
        //copy diffuse while performing sss

        Plane p = p_cam_projection.xform4(Plane(1, 0, -1, 1));
        p.normal /= p.d;
        float unit_size = p.normal.x;

        //copy normal and roughness to effect buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, current_rt->buffers.fbo);
        glReadBuffer(GL_COLOR_ATTACHMENT3);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_rt->effects.ssao.blur_fbo[0]);
        glBlitFramebuffer(0, 0, current_rt->width, current_rt->height, 0, 0, current_rt->width, current_rt->height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        state.sss_shader.set_conditional(SubsurfScatteringShaderGLES3::USE_ORTHOGONAL_PROJECTION, p_cam_projection.is_orthogonal());
        state.sss_shader.set_conditional(SubsurfScatteringShaderGLES3::USE_11_SAMPLES, subsurface_scatter_quality == SSS_QUALITY_LOW);
        state.sss_shader.set_conditional(SubsurfScatteringShaderGLES3::USE_17_SAMPLES, subsurface_scatter_quality == SSS_QUALITY_MEDIUM);
        state.sss_shader.set_conditional(SubsurfScatteringShaderGLES3::USE_25_SAMPLES, subsurface_scatter_quality == SSS_QUALITY_HIGH);
        state.sss_shader.set_conditional(SubsurfScatteringShaderGLES3::ENABLE_FOLLOW_SURFACE, subsurface_scatter_follow_surface);
        state.sss_shader.set_conditional(SubsurfScatteringShaderGLES3::ENABLE_STRENGTH_WEIGHTING, subsurface_scatter_weight_samples);
        state.sss_shader.bind();
        state.sss_shader.set_uniform(SubsurfScatteringShaderGLES3::MAX_RADIUS, subsurface_scatter_size);
        state.sss_shader.set_uniform(SubsurfScatteringShaderGLES3::UNIT_SIZE, unit_size);
        state.sss_shader.set_uniform(SubsurfScatteringShaderGLES3::CAMERA_Z_NEAR, p_cam_projection.get_z_near());
        state.sss_shader.set_uniform(SubsurfScatteringShaderGLES3::CAMERA_Z_FAR, p_cam_projection.get_z_far());
        state.sss_shader.set_uniform(SubsurfScatteringShaderGLES3::DIR, Vector2(1, 0));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[0].color);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); //disable filter (fixes bugs on AMD)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.ssao.blur_red[0]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, current_rt->depth);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->fbo); //copy to front first

        _copy_screen(true);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->color);
        state.sss_shader.set_uniform(SubsurfScatteringShaderGLES3::DIR, Vector2(0, 1));
        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[0].fbo); // copy to base level
        _copy_screen(true);

        glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[0].color); //restore filter
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }

    if (env->ssr_enabled) {

        //blur diffuse into effect mipmaps using separatable convolution
        //storage->shaders.copy.set_conditional(CopyShaderGLES3::GAUSSIAN_HORIZONTAL,true);
        _blur_effect_buffer();

        //perform SSR

        state.ssr_shader.set_conditional(ScreenSpaceReflectionShaderGLES3::REFLECT_ROUGHNESS, env->ssr_roughness);
        state.ssr_shader.set_conditional(ScreenSpaceReflectionShaderGLES3::USE_ORTHOGONAL_PROJECTION, p_cam_projection.is_orthogonal());

        state.ssr_shader.bind();

        int ssr_w = current_rt->effects.mip_maps[1].sizes[0].width;
        int ssr_h = current_rt->effects.mip_maps[1].sizes[0].height;

        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::PIXEL_SIZE, Vector2(1.0 / (ssr_w * 0.5), 1.0 / (ssr_h * 0.5)));
        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::CAMERA_Z_NEAR, p_cam_projection.get_z_near());
        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::CAMERA_Z_FAR, p_cam_projection.get_z_far());
        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::PROJECTION, p_cam_projection);
        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::INVERSE_PROJECTION, p_cam_projection.inverse());
        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::VIEWPORT_SIZE, Size2(ssr_w, ssr_h));
        //state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::FRAME_INDEX,int(render_pass));
        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::FILTER_MIPMAP_LEVELS, float(current_rt->effects.mip_maps[0].sizes.size()));
        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::NUM_STEPS, env->ssr_max_steps);
        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::DEPTH_TOLERANCE, env->ssr_depth_tolerance);
        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::DISTANCE_FADE, env->ssr_fade_out);
        state.ssr_shader.set_uniform(ScreenSpaceReflectionShaderGLES3::CURVE_FADE_IN, env->ssr_fade_in);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[0].color);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, current_rt->buffers.effect);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, current_rt->depth);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[1].sizes[0].fbo);
        glViewport(0, 0, ssr_w, ssr_h);

        _copy_screen(true);
        glViewport(0, 0, current_rt->width, current_rt->height);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, current_rt->buffers.fbo);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_rt->fbo);
    //glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, current_rt->width, current_rt->height, 0, 0, current_rt->width, current_rt->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    //copy reflection over diffuse, resolving SSR if needed
    state.resolve_shader.set_conditional(ResolveShaderGLES3::USE_SSR, env->ssr_enabled);
    state.resolve_shader.bind();
    state.resolve_shader.set_uniform(ResolveShaderGLES3::PIXEL_SIZE, Vector2(1.0 / current_rt->width, 1.0 / current_rt->height));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, current_rt->color);
    if (env->ssr_enabled) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[1].color);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[0].fbo);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE); //use additive to accumulate one over the other

    _copy_screen(true);

    glDisable(GL_BLEND); //end additive

    if (state.used_screen_texture) {
        _blur_effect_buffer();
        //restored framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[0].fbo);
        glViewport(0, 0, current_rt->width, current_rt->height);
    }

    state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::SIMPLE_COPY, true);
    state.effect_blur_shader.bind();
    state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::LOD, float(0));

    {
        GLuint db = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &db);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, current_rt->buffers.fbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[0].color);

    _copy_screen(true);

    state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::SIMPLE_COPY, false);
}

void RasterizerSceneGLES3::_post_process(RasterizerEnvironmentComponent *env, const CameraMatrix &p_cam_projection) {

    auto * current_rt = get<RasterizerRenderTargetComponent>(storage->frame.current_rt);
    //copy to front buffer

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LEQUAL);
    glColorMask(1, 1, 1, 1);

    //turn off everything used

    //copy specular to front buffer
    //copy diffuse to effect buffer

    if (current_rt->buffers.active) {
        //transfer to effect buffer if using buffers, also resolve MSAA
        glBindFramebuffer(GL_READ_FRAMEBUFFER, current_rt->buffers.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[0].fbo);
        glBlitFramebuffer(0, 0, current_rt->width, current_rt->height, 0, 0, current_rt->width, current_rt->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }
    if ((!env || current_rt->flags[RS::RENDER_TARGET_TRANSPARENT] || current_rt->width < 4 || current_rt->height < 4) && !current_rt->use_fxaa && !current_rt->use_debanding  && current_rt->sharpen_intensity < 0.001) { //no post process on small render targets
        //no environment or transparent render, simply return and convert to SRGB
        if (current_rt->external.fbo.is_initialized()) {
            glBindFramebuffer(GL_FRAMEBUFFER, current_rt->external.fbo);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, current_rt->fbo);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[0].color);
        storage->shaders.copy.set_conditional(CopyShaderGLES3::LINEAR_TO_SRGB, !current_rt->flags[RS::RENDER_TARGET_KEEP_3D_LINEAR]);
        storage->shaders.copy.set_conditional(CopyShaderGLES3::V_FLIP, current_rt->flags[RS::RENDER_TARGET_VFLIP]);
        storage->shaders.copy.set_conditional(CopyShaderGLES3::DISABLE_ALPHA, !current_rt->flags[RS::RENDER_TARGET_TRANSPARENT]);
        storage->shaders.copy.bind();

        _copy_screen(true);

        storage->shaders.copy.set_conditional(CopyShaderGLES3::LINEAR_TO_SRGB, false);
        storage->shaders.copy.set_conditional(CopyShaderGLES3::DISABLE_ALPHA, false); //compute luminance
        storage->shaders.copy.set_conditional(CopyShaderGLES3::V_FLIP, false);

        return;
    }

    //order of operation
    //1) DOF Blur (first blur, then copy to buffer applying the blur)
    //2) FXAA
    //3) Bloom (Glow)
    //4) Tonemap
    //5) Adjustments

    GLuint composite_from = current_rt->effects.mip_maps[0].color;

    if (env && env->dof_blur_far_enabled) {

        //blur diffuse into effect mipmaps using separatable convolution
        //storage->shaders.copy.set_conditional(CopyShaderGLES3::GAUSSIAN_HORIZONTAL,true);

        int vp_h = current_rt->height;
        int vp_w = current_rt->width;

        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::USE_ORTHOGONAL_PROJECTION, p_cam_projection.is_orthogonal());
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_FAR_BLUR, true);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_LOW, env->dof_blur_far_quality == RS::ENV_DOF_BLUR_QUALITY_LOW);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_MEDIUM, env->dof_blur_far_quality == RS::ENV_DOF_BLUR_QUALITY_MEDIUM);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_HIGH, env->dof_blur_far_quality == RS::ENV_DOF_BLUR_QUALITY_HIGH);

        state.effect_blur_shader.bind();
        int qsteps[3] = { 4, 10, 20 };

        float radius = (env->dof_blur_far_amount * env->dof_blur_far_amount) / qsteps[env->dof_blur_far_quality];

        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_BEGIN, env->dof_blur_far_distance);
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_END, env->dof_blur_far_distance + env->dof_blur_far_transition);
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_DIR, Vector2(1, 0));
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_RADIUS, radius);
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::PIXEL_SIZE, Vector2(1.0 / vp_w, 1.0 / vp_h));
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::CAMERA_Z_NEAR, p_cam_projection.get_z_near());
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::CAMERA_Z_FAR, p_cam_projection.get_z_far());

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, current_rt->depth);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, composite_from);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->fbo); //copy to front first

        _copy_screen(true);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->color);
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_DIR, Vector2(0, 1));
        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[0].fbo); // copy to base level
        _copy_screen();

        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_FAR_BLUR, false);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_LOW, false);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_MEDIUM, false);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_HIGH, false);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::USE_ORTHOGONAL_PROJECTION, false);

        composite_from = current_rt->effects.mip_maps[0].color;
    }

    if (env && env->dof_blur_near_enabled) {

        //blur diffuse into effect mipmaps using separatable convolution
        //storage->shaders.copy.set_conditional(CopyShaderGLES3::GAUSSIAN_HORIZONTAL,true);

        int vp_h = current_rt->height;
        int vp_w = current_rt->width;

        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::USE_ORTHOGONAL_PROJECTION, p_cam_projection.is_orthogonal());
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_NEAR_BLUR, true);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_NEAR_FIRST_TAP, true);

        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_LOW, env->dof_blur_near_quality == RS::ENV_DOF_BLUR_QUALITY_LOW);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_MEDIUM, env->dof_blur_near_quality == RS::ENV_DOF_BLUR_QUALITY_MEDIUM);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_HIGH, env->dof_blur_near_quality == RS::ENV_DOF_BLUR_QUALITY_HIGH);

        state.effect_blur_shader.bind();
        int qsteps[3] = { 4, 10, 20 };

        float radius = (env->dof_blur_near_amount * env->dof_blur_near_amount) / qsteps[env->dof_blur_near_quality];

        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_BEGIN, env->dof_blur_near_distance);
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_END, env->dof_blur_near_distance - env->dof_blur_near_transition);
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_DIR, Vector2(1, 0));
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_RADIUS, radius);
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::PIXEL_SIZE, Vector2(1.0f / vp_w, 1.0f / vp_h));
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::CAMERA_Z_NEAR, p_cam_projection.get_z_near());
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::CAMERA_Z_FAR, p_cam_projection.get_z_far());

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, current_rt->depth);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, composite_from);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->fbo); //copy to front first

        _copy_screen();
        //manually do the blend if this is the first operation resolving from the diffuse buffer
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_NEAR_BLUR_MERGE, current_rt->buffers.diffuse == composite_from);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_NEAR_FIRST_TAP, false);
        state.effect_blur_shader.bind();

        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_BEGIN, env->dof_blur_near_distance);
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_END, env->dof_blur_near_distance - env->dof_blur_near_transition);
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_DIR, Vector2(0, 1));
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::DOF_RADIUS, radius);
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::PIXEL_SIZE, Vector2(1.0 / vp_w, 1.0 / vp_h));
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::CAMERA_Z_NEAR, p_cam_projection.get_z_near());
        state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::CAMERA_Z_FAR, p_cam_projection.get_z_far());

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->color);

        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[0].fbo); // copy to base level

        if (current_rt->buffers.diffuse != composite_from) {

            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            // Alpha was used by the horizontal pass, it should not carry over.
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);

        } else {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, current_rt->buffers.diffuse);
        }

        _copy_screen(true);

        if (current_rt->buffers.diffuse != composite_from) {

            glDisable(GL_BLEND);
        }

        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_NEAR_BLUR, false);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_NEAR_FIRST_TAP, false);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_NEAR_BLUR_MERGE, false);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_LOW, false);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_MEDIUM, false);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::DOF_QUALITY_HIGH, false);
        state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::USE_ORTHOGONAL_PROJECTION, false);

        composite_from = current_rt->effects.mip_maps[0].color;
    }

    if (env && (env->dof_blur_near_enabled || env->dof_blur_far_enabled)) {
        //these needed to disable filtering, reenamble
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[0].color);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (env && env->auto_exposure) {

        //compute auto exposure
        //first step, copy from image to luminance buffer
        state.exposure_shader.set_conditional(ExposureShaderGLES3::EXPOSURE_BEGIN, true);
        state.exposure_shader.bind();
        int ss[2] = {
            current_rt->width,
            current_rt->height,
        };
        int ds[2] = {
            exposure_shrink_size,
            exposure_shrink_size,
        };

        glUniform2iv(state.exposure_shader.get_uniform(ExposureShaderGLES3::SOURCE_RENDER_SIZE), 1, ss);
        glUniform2iv(state.exposure_shader.get_uniform(ExposureShaderGLES3::TARGET_SIZE), 1, ds);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, composite_from);

        glBindFramebuffer(GL_FRAMEBUFFER, exposure_shrink[0].fbo);
        glViewport(0, 0, exposure_shrink_size, exposure_shrink_size);

        _copy_screen(true);

        //second step, shrink to 2x2 pixels
        state.exposure_shader.set_conditional(ExposureShaderGLES3::EXPOSURE_BEGIN, false);
        state.exposure_shader.bind();
        //shrink from second to previous to last level

        int s_size = exposure_shrink_size / 3;
        for (int i = 1; i < exposure_shrink.size() - 1; i++) {

            glBindFramebuffer(GL_FRAMEBUFFER, exposure_shrink[i].fbo);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, exposure_shrink[i - 1].color);

            _copy_screen();

            glViewport(0, 0, s_size, s_size);

            s_size /= 3;
        }
        //third step, shrink to 1x1 pixel taking in consideration the previous exposure
        state.exposure_shader.set_conditional(ExposureShaderGLES3::EXPOSURE_END, true);

        uint64_t tick = OS::get_singleton()->get_ticks_usec();
        uint64_t tick_diff = current_rt->last_exposure_tick == 0 ? 0 : tick - current_rt->last_exposure_tick;
        current_rt->last_exposure_tick = tick;

        if (tick_diff == 0 || tick_diff > 1000000) {
            state.exposure_shader.set_conditional(ExposureShaderGLES3::EXPOSURE_FORCE_SET, true);
        }

        state.exposure_shader.bind();

        glBindFramebuffer(GL_FRAMEBUFFER, exposure_shrink[exposure_shrink.size() - 1].fbo);
        glViewport(0, 0, 1, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, exposure_shrink[exposure_shrink.size() - 2].color);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, current_rt->exposure.color); //read from previous

        state.exposure_shader.set_uniform(ExposureShaderGLES3::EXPOSURE_ADJUST, env->auto_exposure_speed * (tick_diff / 1000000.0));
        state.exposure_shader.set_uniform(ExposureShaderGLES3::MAX_LUMINANCE, env->auto_exposure_max);
        state.exposure_shader.set_uniform(ExposureShaderGLES3::MIN_LUMINANCE, env->auto_exposure_min);

        _copy_screen(true);

        state.exposure_shader.set_conditional(ExposureShaderGLES3::EXPOSURE_FORCE_SET, false);
        state.exposure_shader.set_conditional(ExposureShaderGLES3::EXPOSURE_END, false);

        //last step, swap with the framebuffer exposure, so the right exposure is kept int he framebuffer
        eastl::swap(exposure_shrink.back(), current_rt->exposure);

        glViewport(0, 0, current_rt->width, current_rt->height);

        RenderingServerRaster::redraw_request(); //if using auto exposure, redraw must happen
    }

    int max_glow_level = -1;
    int glow_mask = 0;

    if (env && env->glow_enabled) {

        for (int i = 0; i < RS::MAX_GLOW_LEVELS; i++) {
            if (env->glow_levels & (1 << i)) {

                if (i >= current_rt->effects.mip_maps[1].sizes.size()) {
                    max_glow_level = current_rt->effects.mip_maps[1].sizes.size() - 1;
                    glow_mask |= 1 << max_glow_level;

                } else {
                    max_glow_level = i;
                    glow_mask |= (1 << i);
                }
            }
        }

        //blur diffuse into effect mipmaps using separatable convolution
        //storage->shaders.copy.set_conditional(CopyShaderGLES3::GAUSSIAN_HORIZONTAL,true);

        for (int i = 0; i < (max_glow_level + 1); i++) {

            int vp_w = current_rt->effects.mip_maps[1].sizes[i].width;
            int vp_h = current_rt->effects.mip_maps[1].sizes[i].height;
            glViewport(0, 0, vp_w, vp_h);
            //horizontal pass
            if (i == 0) {
                state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GLOW_FIRST_PASS, true);
                state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GLOW_USE_AUTO_EXPOSURE, env->auto_exposure);
            }

            state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GLOW_GAUSSIAN_HORIZONTAL, true);
            state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::USE_GLOW_HIGH_QUALITY, env->glow_high_quality);
            state.effect_blur_shader.bind();
            state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::PIXEL_SIZE, Vector2(1.0 / vp_w, 1.0 / vp_h));
            state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::LOD, float(i));
            state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::GLOW_STRENGTH, env->glow_strength);
            state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::LUMINANCE_CAP, env->glow_hdr_luminance_cap);

            glActiveTexture(GL_TEXTURE0);
            if (i == 0) {
                glBindTexture(GL_TEXTURE_2D, composite_from);

                state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::EXPOSURE, env->tone_mapper_exposure);
                if (env->auto_exposure) {
                    state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::AUTO_EXPOSURE_GREY, env->auto_exposure_grey);
                }

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, current_rt->exposure.color);

                state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::GLOW_BLOOM, env->glow_bloom);
                state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::GLOW_HDR_THRESHOLD, env->glow_hdr_bleed_threshold);
                state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::GLOW_HDR_SCALE, env->glow_hdr_bleed_scale);

            } else {
                glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[0].color); //previous level, since mipmaps[0] starts one level bigger
            }
            glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[1].sizes[i].fbo);
            _copy_screen(true);
            state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GLOW_GAUSSIAN_HORIZONTAL, false);
            state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GLOW_FIRST_PASS, false);
            state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GLOW_USE_AUTO_EXPOSURE, false);

            //vertical pass
            state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GLOW_GAUSSIAN_VERTICAL, true);
            state.effect_blur_shader.bind();
            state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::PIXEL_SIZE, Vector2(1.0 / vp_w, 1.0 / vp_h));
            state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::LOD, float(i));
            state.effect_blur_shader.set_uniform(EffectBlurShaderGLES3::GLOW_STRENGTH, env->glow_strength);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[1].color);
            glBindFramebuffer(GL_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[i + 1].fbo); //next level, since mipmaps[0] starts one level bigger
            _copy_screen();
            state.effect_blur_shader.set_conditional(EffectBlurShaderGLES3::GLOW_GAUSSIAN_VERTICAL, false);
        }

        glViewport(0, 0, current_rt->width, current_rt->height);
    }

    if (current_rt->external.fbo.is_initialized()) {
        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->external.fbo);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->fbo);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, composite_from);

    if (env) {
        state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_FILMIC_TONEMAPPER, env->tone_mapper == RS::ENV_TONE_MAPPER_FILMIC);
        state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_ACES_TONEMAPPER, env->tone_mapper == RS::ENV_TONE_MAPPER_ACES);
        state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_ACES_FITTED_TONEMAPPER, env->tone_mapper == RS::ENV_TONE_MAPPER_ACES_FITTED);
        state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_REINHARD_TONEMAPPER, env->tone_mapper == RS::ENV_TONE_MAPPER_REINHARD);
        state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_AUTO_EXPOSURE, env->auto_exposure);
        state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_FILTER_BICUBIC, env->glow_bicubic_upscale);
    }
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::KEEP_3D_LINEAR, current_rt->flags[RS::RENDER_TARGET_KEEP_3D_LINEAR]);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_FXAA, current_rt->use_fxaa);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_DEBANDING, current_rt->use_debanding);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_SHARPENING, current_rt->sharpen_intensity >= 0.001);

    if (env && max_glow_level >= 0) {

        for (int i = 0; i < (max_glow_level + 1); i++) {

            if (glow_mask & (1 << i)) {
                if (i == 0) {
                    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL1, true);
                }
                if (i == 1) {
                    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL2, true);
                }
                if (i == 2) {
                    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL3, true);
                }
                if (i == 3) {
                    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL4, true);
                }
                if (i == 4) {
                    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL5, true);
                }
                if (i == 5) {
                    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL6, true);
                }
                if (i == 6) {
                    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL7, true);
                }
            }
        }

        state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_SCREEN, env->glow_blend_mode == RS::GLOW_BLEND_MODE_SCREEN);
        state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_SOFTLIGHT, env->glow_blend_mode == RS::GLOW_BLEND_MODE_SOFTLIGHT);
        state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_REPLACE, env->glow_blend_mode == RS::GLOW_BLEND_MODE_REPLACE);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[0].color);
    }

    if (env && env->adjustments_enabled) {

        state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_BCS, true);
        auto *tex = get<RasterizerTextureComponent>(env->color_correction);
        if (tex) {
            state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_COLOR_CORRECTION, true);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(tex->target, tex->tex_id);
        }
    }

    state.tonemap_shader.set_conditional(TonemapShaderGLES3::DISABLE_ALPHA, !current_rt->flags[RS::RENDER_TARGET_TRANSPARENT]);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::V_FLIP, current_rt->flags[RS::RENDER_TARGET_VFLIP]);
    state.tonemap_shader.bind();
    if(env) {
        state.tonemap_shader.set_uniform(TonemapShaderGLES3::EXPOSURE, env->tone_mapper_exposure);
        state.tonemap_shader.set_uniform(TonemapShaderGLES3::WHITE, env->tone_mapper_exposure_white);

        if (max_glow_level >= 0) {

            state.tonemap_shader.set_uniform(TonemapShaderGLES3::GLOW_INTENSITY, env->glow_intensity);
            int ss[2] = {
                current_rt->width,
                current_rt->height,
            };
            glUniform2iv(state.tonemap_shader.get_uniform(TonemapShaderGLES3::GLOW_TEXTURE_SIZE), 1, ss);
        }

        if (env->auto_exposure) {

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, current_rt->exposure.color);
            state.tonemap_shader.set_uniform(TonemapShaderGLES3::AUTO_EXPOSURE_GREY, env->auto_exposure_grey);
        }

        if (env->adjustments_enabled) {

            state.tonemap_shader.set_uniform(TonemapShaderGLES3::BCS, Vector3(env->adjustments_brightness, env->adjustments_contrast, env->adjustments_saturation));
        }
    } else {
        // No environment, so no exposure.
        state.tonemap_shader.set_uniform(TonemapShaderGLES3::EXPOSURE, 1.0);
    }

    if (current_rt->use_fxaa) {
        state.tonemap_shader.set_uniform(TonemapShaderGLES3::PIXEL_SIZE, Vector2(1.0 / current_rt->width, 1.0 / current_rt->height));
    }

    if (current_rt->sharpen_intensity >= 0.001f) {
        state.tonemap_shader.set_uniform(TonemapShaderGLES3::SHARPEN_INTENSITY, current_rt->sharpen_intensity);
    }
    _copy_screen(true, true);

    //turn off everything used
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_FXAA, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_DEBANDING, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_SHARPENING, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_AUTO_EXPOSURE, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_FILMIC_TONEMAPPER, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_ACES_TONEMAPPER, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_ACES_FITTED_TONEMAPPER, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_REINHARD_TONEMAPPER, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL1, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL2, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL3, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL4, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL5, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL6, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_LEVEL7, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_REPLACE, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_SCREEN, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_SOFTLIGHT, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_GLOW_FILTER_BICUBIC, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_BCS, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::USE_COLOR_CORRECTION, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::V_FLIP, false);
    state.tonemap_shader.set_conditional(TonemapShaderGLES3::DISABLE_ALPHA, false);
}

bool _element_needs_directional_add(RasterizerSceneGLES3 *self,RenderListElement *e) {
    // return whether this element should take part in directional add
    if (e->sort_key & SORT_KEY_UNSHADED_FLAG) {
        return false;
    }
    auto *instance = get<RenderingInstanceComponent>(e->instance);

    for (int i = 0; i < self->state.directional_light_count; i++) {
        RasterizerLightInstanceComponent *l = self->directional_lights[i];
        auto *light_ptr = get<RasterizerLight3DComponent>(l->light);

        // any unbaked and unculled light?
        if (instance->baked_light && light_ptr->bake_mode == RS::LightBakeMode::LIGHT_BAKE_ALL) {
            continue;
        }
        if ((instance->layer_mask & light_ptr->cull_mask) == 0) {
            continue;
        }
        return true;
    }
    return false; // no visible unbaked light
}

void RasterizerSceneGLES3::render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, const int p_eye,
        bool p_cam_ortogonal, Span<RenderingEntity> p_cull_result, RenderingEntity *p_light_cull_result,
        int p_light_cull_count, RenderingEntity *p_reflection_probe_cull_result, int p_reflection_probe_cull_count,
        RenderingEntity p_environment, RenderingEntity p_shadow_atlas, RenderingEntity p_reflection_atlas, RenderingEntity p_reflection_probe,
        int p_reflection_probe_pass) {
    SCOPE_AUTONAMED

    auto * current_rt = get<RasterizerRenderTargetComponent>(storage->frame.current_rt);
    //first of all, make a new render pass
    render_pass++;

    //fill up ubo

    get_rasterizer_storage_info().render.object_count += p_cull_result.size();

    auto *env = get<RasterizerEnvironmentComponent>(p_environment);
    auto *shadow_atlas = get<RasterizerShadowAtlasComponent>(p_shadow_atlas);
    auto *reflection_atlas = get<RasterizerReflectionAtlasComponent>(p_reflection_atlas);

    bool use_shadows = shadow_atlas && shadow_atlas->size;

    state.scene_shader.set_conditional(SceneShaderGLES3::USE_SHADOW, use_shadows);

    if (use_shadows) {
        glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 6);
        glBindTexture(GL_TEXTURE_2D, shadow_atlas->depth);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
        state.ubo_data.shadow_atlas_pixel_size[0] = 1.0f / shadow_atlas->size;
        state.ubo_data.shadow_atlas_pixel_size[1] = 1.0f / shadow_atlas->size;
    } else {
        if (storage->config.async_compilation_enabled) {
            // Avoid GL UB message id 131222 caused by shadow samplers not properly set up in the ubershader
            glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 6);
            glBindTexture(GL_TEXTURE_2D, storage->resources.depth_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        }
    }

    if (reflection_atlas && reflection_atlas->size) {
        glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 4);
        glBindTexture(GL_TEXTURE_2D, reflection_atlas->color);
    }

    if (p_reflection_probe!=entt::null) {
        state.ubo_data.reflection_multiplier = 0.0f;
    } else {
        state.ubo_data.reflection_multiplier = 1.0f;
    }

    state.ubo_data.subsurface_scatter_width = subsurface_scatter_size;

    state.ubo_data.z_offset = 0;
    state.ubo_data.z_slope_scale = 0;
    state.ubo_data.shadow_dual_paraboloid_render_side = 0;
    state.ubo_data.shadow_dual_paraboloid_render_zfar = 0;
    state.ubo_data.opaque_prepass_threshold = 0.99f;

    if (current_rt) {
        int viewport_width_pixels = current_rt->width;
        int viewport_height_pixels = current_rt->height;

        state.ubo_data.viewport_size[0] = viewport_width_pixels;
        state.ubo_data.viewport_size[1] = viewport_height_pixels;

        state.ubo_data.screen_pixel_size[0] = 1.0 / viewport_width_pixels;
        state.ubo_data.screen_pixel_size[1] = 1.0 / viewport_height_pixels;
    }

    _setup_environment(this, env, p_cam_projection, p_cam_transform, p_eye, p_reflection_probe!=entt::null);

    bool fb_cleared = false;

    glDepthFunc(GL_LEQUAL);

    state.used_contact_shadows = false;
    state.prepared_depth_texture = false;
    state.bound_depth_texture = false;

    for (int i = 0; i < p_light_cull_count; i++) {

        ERR_BREAK(i >= render_list.max_lights);

        auto *li = get<RasterizerLightInstanceComponent>(p_light_cull_result[i]);
        auto *light_ptr = li ? get<RasterizerLight3DComponent>(li->light) : nullptr;
        ERR_FAIL_COND(!light_ptr);

        if (light_ptr->param[RS::LIGHT_PARAM_CONTACT_SHADOW_SIZE] > CMP_EPSILON) {
            state.used_contact_shadows = true;
        }
    }

    // Do depth prepass if it's explicitly enabled
    bool use_depth_prepass = storage->config.use_depth_prepass;

    // If contact shadows are used then we need to do depth prepass even if it's otherwise disabled
    use_depth_prepass = use_depth_prepass || state.used_contact_shadows;

    // Never do depth prepass if effects are disabled or if we render overdraws
    use_depth_prepass = use_depth_prepass && current_rt && !current_rt->flags[RS::RENDER_TARGET_NO_3D_EFFECTS];
    use_depth_prepass = use_depth_prepass && state.debug_draw != RS::VIEWPORT_DEBUG_DRAW_OVERDRAW;

    if (use_depth_prepass) {
        //pre z pass

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, current_rt->buffers.fbo);
        glDrawBuffers(0, nullptr);

        glViewport(0, 0, current_rt->width, current_rt->height);

        glColorMask(0, 0, 0, 0);
        glClearDepth(1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        render_list.clear();
        _fill_render_list(p_cull_result, true, false);
        render_list.sort_by_key(false);
        state.scene_shader.set_conditional(SceneShaderGLES3::RENDER_DEPTH, true);
        _render_list(render_list.elements, p_cam_transform, p_cam_projection, nullptr, false, false, true, false, false);
        state.scene_shader.set_conditional(SceneShaderGLES3::RENDER_DEPTH, false);

        glColorMask(1, 1, 1, 1);

        if (state.used_contact_shadows) {

            _prepare_depth_texture();
            _bind_depth_texture();
        }

        fb_cleared = true;
        render_pass++;
        state.used_depth_prepass = true;
    } else {
        state.used_depth_prepass = false;
    }

    _setup_lights(this, p_light_cull_result, p_light_cull_count, p_cam_transform.affine_inverse(), p_cam_projection, p_shadow_atlas);
    _setup_reflections(p_reflection_probe_cull_result, p_reflection_probe_cull_count, p_cam_transform.affine_inverse(), p_cam_projection, p_reflection_atlas, env);

    bool use_mrt = false;

    render_list.clear();
    _fill_render_list(p_cull_result, false, false);
    //

    glEnable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    //rendering to a probe cubemap side
    auto *probe = get<RasterizerReflectionProbeInstanceComponent>(p_reflection_probe);
    GLuint current_fbo;

    if (probe) {
        auto *ref_atlas = get<RasterizerReflectionAtlasComponent>(probe->atlas);
        ERR_FAIL_COND(!ref_atlas);

        int target_size = ref_atlas->size / ref_atlas->subdiv;

        int cubemap_index = reflection_cubemaps.size() - 1;

        for (int i = reflection_cubemaps.size() - 1; i >= 0; i--) {
            //find appropriate cubemap to render to
            if (reflection_cubemaps[i].size > target_size * 2) {
                break;
            }

            cubemap_index = i;
        }

        current_fbo = reflection_cubemaps[cubemap_index].fbo_id[p_reflection_probe_pass];
        use_mrt = false;
        state.scene_shader.set_conditional(SceneShaderGLES3::USE_MULTIPLE_RENDER_TARGETS, false);

        glViewport(0, 0, reflection_cubemaps[cubemap_index].size, reflection_cubemaps[cubemap_index].size);
        glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);

    } else {

        use_mrt = env && (state.used_sss || env->ssao_enabled || env->ssr_enabled || env->dof_blur_far_enabled || env->dof_blur_near_enabled); //only enable MRT rendering if any of these is enabled
        //effects disabled and transparency also prevent using MRTs
        use_mrt = use_mrt && !current_rt->flags[RS::RENDER_TARGET_TRANSPARENT];
        use_mrt = use_mrt && !current_rt->flags[RS::RENDER_TARGET_NO_3D_EFFECTS];
        use_mrt = use_mrt && state.debug_draw != RS::VIEWPORT_DEBUG_DRAW_OVERDRAW;
        use_mrt = use_mrt && (env->bg_mode != RS::ENV_BG_KEEP && env->bg_mode != RS::ENV_BG_CANVAS);

        glViewport(0, 0, current_rt->width, current_rt->height);

        if (use_mrt) {

            current_fbo = current_rt->buffers.fbo;

            glBindFramebuffer(GL_FRAMEBUFFER, current_rt->buffers.fbo);
            state.scene_shader.set_conditional(SceneShaderGLES3::USE_MULTIPLE_RENDER_TARGETS, true);

            FixedVector<GLenum,4,false> draw_buffers;
            draw_buffers.push_back(GL_COLOR_ATTACHMENT0);
            draw_buffers.push_back(GL_COLOR_ATTACHMENT1);
            draw_buffers.push_back(GL_COLOR_ATTACHMENT2);
            if (state.used_sss) {
                draw_buffers.push_back(GL_COLOR_ATTACHMENT3);
            }
            glDrawBuffers(draw_buffers.size(), draw_buffers.data());

            Color black(0, 0, 0, 0);
            glClearBufferfv(GL_COLOR, 1, &black.r); // specular
            glClearBufferfv(GL_COLOR, 2, &black.r); // normal metal rough
            if (state.used_sss) {
                glClearBufferfv(GL_COLOR, 3, &black.r); // normal metal rough
            }

        } else {

            if (current_rt->buffers.active) {
                current_fbo = current_rt->buffers.fbo;
            } else {
                if (current_rt->effects.mip_maps[0].sizes.empty()) {
                    ERR_PRINT_ONCE("Can't use canvas background mode in a render target configured without sampling");
                    return;
                }
                current_fbo = current_rt->effects.mip_maps[0].sizes[0].fbo;
            }

            glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);
            state.scene_shader.set_conditional(SceneShaderGLES3::USE_MULTIPLE_RENDER_TARGETS, false);

            GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, draw_buffers);
        }
    }

    if (!fb_cleared) {
        glClearDepth(1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    Color clear_color(0, 0, 0, 0);

    RasterizerSkyComponent *sky = nullptr;
    Ref<CameraFeed> feed;

    if (state.debug_draw == RS::VIEWPORT_DEBUG_DRAW_OVERDRAW) {
        clear_color = Color(0, 0, 0, 0);
        storage->frame.clear_request = false;
    } else if (!probe && current_rt->flags[RS::RENDER_TARGET_TRANSPARENT]) {
        clear_color = Color(0, 0, 0, 0);
        storage->frame.clear_request = false;

    } else if (!env || env->bg_mode == RS::ENV_BG_CLEAR_COLOR) {

        if (storage->frame.clear_request) {

            clear_color = storage->frame.clear_request_color.to_linear();
            storage->frame.clear_request = false;
        }

    } else if (env->bg_mode == RS::ENV_BG_CANVAS) {

        clear_color = env->bg_color.to_linear();
        storage->frame.clear_request = false;
    } else if (env->bg_mode == RS::ENV_BG_COLOR) {

        clear_color = env->bg_color.to_linear();
        storage->frame.clear_request = false;
    } else if (env->bg_mode == RS::ENV_BG_SKY) {

        storage->frame.clear_request = false;

    } else if (env->bg_mode == RS::ENV_BG_COLOR_SKY) {

        clear_color = env->bg_color.to_linear();
        storage->frame.clear_request = false;

    } else if (env->bg_mode == RS::ENV_BG_CAMERA_FEED) {
        feed = CameraServer::get_singleton()->get_feed_by_id(env->camera_feed_id);
        storage->frame.clear_request = false;
    } else {
        storage->frame.clear_request = false;
    }

    if (!env || env->bg_mode != RS::ENV_BG_KEEP) {
        glClearBufferfv(GL_COLOR, 0, &clear_color.r); // specular
    }

    RS::EnvironmentBG bg_mode = (!env || (probe && env->bg_mode == RS::ENV_BG_CANVAS)) ? RS::ENV_BG_CLEAR_COLOR : env->bg_mode; //if no environment, or canvas while rendering a probe (invalid use case), use color.

    if (env) {
        switch (bg_mode) {
            case RS::ENV_BG_COLOR_SKY:
            case RS::ENV_BG_SKY:

                sky = get<RasterizerSkyComponent>(env->sky);
                break;
            case RS::ENV_BG_CANVAS:
                //copy canvas to 3d buffer and convert it to linear

                glDisable(GL_BLEND);
                glDepthMask(GL_FALSE);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, current_rt->color);

                storage->shaders.copy.set_conditional(CopyShaderGLES3::DISABLE_ALPHA, true);

                storage->shaders.copy.set_conditional(CopyShaderGLES3::SRGB_TO_LINEAR, true);

                storage->shaders.copy.bind();

                _copy_screen(true, true);

                //turn off everything used
                storage->shaders.copy.set_conditional(CopyShaderGLES3::SRGB_TO_LINEAR, false);
                storage->shaders.copy.set_conditional(CopyShaderGLES3::DISABLE_ALPHA, false);

                //restore
                glEnable(GL_BLEND);
                glDepthMask(GL_TRUE);
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_CULL_FACE);
                break;
            case RS::ENV_BG_CAMERA_FEED:
                if (feed && (feed->get_base_width() > 0) && (feed->get_base_height() > 0)) {
                    // copy our camera feed to our background

                    glDisable(GL_BLEND);
                    glDepthMask(GL_FALSE);
                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);

                    storage->shaders.copy.set_conditional(CopyShaderGLES3::USE_DISPLAY_TRANSFORM, true);
                    storage->shaders.copy.set_conditional(CopyShaderGLES3::DISABLE_ALPHA, true);
                    storage->shaders.copy.set_conditional(CopyShaderGLES3::SRGB_TO_LINEAR, true);

                    if (feed->get_datatype() == CameraFeed::FEED_RGB) {
                        RenderingEntity camera_RGBA = feed->get_texture(CameraServer::FEED_RGBA_IMAGE);

                        RenderingServer::get_singleton()->texture_bind(camera_RGBA, 0);
                    } else if (feed->get_datatype() == CameraFeed::FEED_YCBCR) {
                        RenderingEntity camera_YCbCr = feed->get_texture(CameraServer::FEED_YCBCR_IMAGE);

                        RenderingServer::get_singleton()->texture_bind(camera_YCbCr, 0);

                        storage->shaders.copy.set_conditional(CopyShaderGLES3::YCBCR_TO_SRGB, true);

                    } else if (feed->get_datatype() == CameraFeed::FEED_YCBCR_SEP) {
                        RenderingEntity camera_Y = feed->get_texture(CameraServer::FEED_Y_IMAGE);
                        RenderingEntity camera_CbCr = feed->get_texture(CameraServer::FEED_CBCR_IMAGE);

                        RenderingServer::get_singleton()->texture_bind(camera_Y, 0);
                        RenderingServer::get_singleton()->texture_bind(camera_CbCr, 1);

                        storage->shaders.copy.set_conditional(CopyShaderGLES3::SEP_CBCR_TEXTURE, true);
                        storage->shaders.copy.set_conditional(CopyShaderGLES3::YCBCR_TO_SRGB, true);
                    }

                    storage->shaders.copy.bind();
                    storage->shaders.copy.set_uniform(CopyShaderGLES3::DISPLAY_TRANSFORM, feed->get_transform());

                    _copy_screen(true, true);

                    //turn off everything used
                    storage->shaders.copy.set_conditional(CopyShaderGLES3::USE_DISPLAY_TRANSFORM, false);
                    storage->shaders.copy.set_conditional(CopyShaderGLES3::DISABLE_ALPHA, false);
                    storage->shaders.copy.set_conditional(CopyShaderGLES3::SRGB_TO_LINEAR, false);
                    storage->shaders.copy.set_conditional(CopyShaderGLES3::SEP_CBCR_TEXTURE, false);
                    storage->shaders.copy.set_conditional(CopyShaderGLES3::YCBCR_TO_SRGB, false);

                    //restore
                    glEnable(GL_BLEND);
                    glDepthMask(GL_TRUE);
                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_CULL_FACE);
                } else {
                    // don't have a feed, just show greenscreen :)
                    clear_color = Color(0.0, 1.0, 0.0, 1.0);
                }
                break;
            default: {
            }
        }
    }

    if (probe && getUnchecked<RasterizerReflectionProbeComponent>(probe->probe)->interior) {
        sky = nullptr; //for rendering probe interiors, radiance must not be used.
    }

    state.texscreen_copied = false;

    glBlendEquation(GL_FUNC_ADD);

    if (current_rt && current_rt->flags[RS::RENDER_TARGET_TRANSPARENT]) {
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
    } else {
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
        glDisable(GL_BLEND);
    }

    render_list.sort_by_key(false);

    if (state.directional_light_count == 0) {
        directional_light = nullptr;
        _render_list(render_list.elements, p_cam_transform, p_cam_projection, sky, false, false, false, false, use_shadows);
    } else {
        for (int i = 0; i < state.directional_light_count; i++) {
            directional_light = directional_lights[i];
            if (i > 0) {
                glEnable(GL_BLEND);
            }
            _setup_directional_light(this, i, p_cam_transform.affine_inverse(), use_shadows);
            _render_list(render_list.elements, p_cam_transform, p_cam_projection, sky, false, false, false, i > 0, use_shadows);
        }
    }

    state.scene_shader.set_conditional(SceneShaderGLES3::USE_MULTIPLE_RENDER_TARGETS, false);

    if (use_mrt) {
        GLenum gldb = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &gldb);
    }

    if (env && env->bg_mode == RS::ENV_BG_SKY && (!current_rt || (!current_rt->flags[RS::RENDER_TARGET_TRANSPARENT] && state.debug_draw != RS::VIEWPORT_DEBUG_DRAW_OVERDRAW))) {

        /*
        if (use_mrt) {
            glBindFramebuffer(GL_FRAMEBUFFER,current_rt->buffers.fbo); //switch to alpha fbo for sky, only diffuse/ambient matters
        */

        if (sky && sky->panorama!=entt::null) {
            _draw_sky(sky, p_cam_projection, p_cam_transform, false, env->sky_custom_fov, env->bg_energy, env->sky_orientation);
        }
    }

    //_render_list_forward(&alpha_render_list,camera_transform,camera_transform_inverse,camera_projection,false,fragment_lighting,true);
    //glColorMask(1,1,1,1);

    //state.scene_shader.set_conditional( SceneShaderGLES3::USE_FOG,false);

    if (use_mrt) {

        _render_mrts(env, p_cam_projection);
    } else {
        // Here we have to do the blits/resolves that otherwise are done in the MRT rendering, in particular
        // - prepare screen texture for any geometry that uses a shader with screen texture
        // - prepare depth texture for any geometry that uses a shader with depth texture

        bool framebuffer_dirty = false;

        if (current_rt && current_rt->buffers.active && state.used_screen_texture) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, current_rt->buffers.fbo);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_rt->effects.mip_maps[0].sizes[0].fbo);
            glBlitFramebuffer(0, 0, current_rt->width, current_rt->height, 0, 0, current_rt->width, current_rt->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            _blur_effect_buffer();
            framebuffer_dirty = true;
        }

        if (current_rt && current_rt->buffers.active && state.used_depth_texture) {
            _prepare_depth_texture();
            framebuffer_dirty = true;
        }

        if (framebuffer_dirty) {
            // Restore framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, current_rt->buffers.fbo);
            glViewport(0, 0, current_rt->width, current_rt->height);
        }
    }

    if (current_rt && state.used_depth_texture && current_rt->buffers.active) {
        _bind_depth_texture();
    }

    if (current_rt && state.used_screen_texture && current_rt->buffers.active) {
        glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 8);
        glBindTexture(GL_TEXTURE_2D, current_rt->effects.mip_maps[0].color);
    }

    glEnable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    render_list.sort_by_reverse_depth_and_priority(true);

    if (state.directional_light_count <= 1) {
        if (state.directional_light_count == 1) {
            directional_light = directional_lights[0];
            _setup_directional_light(this, 0, p_cam_transform.affine_inverse(), use_shadows);
        } else {
        directional_light = nullptr;
        }
        _render_list(render_list.alpha_elements, p_cam_transform, p_cam_projection, sky, false, true, false, false, use_shadows);
    } else {
        // special handling for multiple directional lights

        // first chunk_start
        int chunk_split = 0;
        int num_alpha = render_list.alpha_elements.size();
        while (chunk_split < num_alpha) {
            int chunk_start = chunk_split;
            bool first = true;
            bool chunk_directional_add = false;
            uint32_t chunk_priority = 0;

            // determine chunk end
            for (; chunk_split < num_alpha; chunk_split++) {
                bool directional_add = _element_needs_directional_add(this, render_list.alpha_elements[chunk_split]);
                uint32_t priority = uint32_t(render_list.alpha_elements[chunk_split]->sort_key >> RenderListConstants::SORT_KEY_PRIORITY_SHIFT);
                if (first) {
                    chunk_directional_add = directional_add;
                    chunk_priority = priority;
                    first = false;
                }
                if ((directional_add != chunk_directional_add) || (priority != chunk_priority)) {
                    break;
                }
            }
            Span<RenderListElement *> subarr(render_list.alpha_elements.data(),chunk_split - chunk_start);

            if (chunk_directional_add) {
        for (int i = 0; i < state.directional_light_count; i++) {
            directional_light = directional_lights[i];
                    _setup_directional_light(this,i, p_cam_transform.affine_inverse(), use_shadows);
                    _render_list(subarr, p_cam_transform, p_cam_projection, sky, false, true, false, i > 0, use_shadows);
                }
            } else {
                directional_light = nullptr;
                _render_list(subarr, p_cam_transform, p_cam_projection, sky, false, true, false, false, use_shadows);
            }
        }
    }

    if (probe) {
        //rendering a probe, do no more!
        return;
    }

    if (env && (env->dof_blur_far_enabled || env->dof_blur_near_enabled) && current_rt && current_rt->buffers.active) {
        _prepare_depth_texture();
    }
    _post_process(env, p_cam_projection);
}

void RasterizerSceneGLES3::render_shadow(RenderingEntity p_light, RenderingEntity p_shadow_atlas, int p_pass, Span<RenderingEntity> p_cull_result) {

    render_pass++;

    directional_light = nullptr;

    auto *light_instance = get<RasterizerLightInstanceComponent>(p_light);
    ERR_FAIL_COND(!light_instance);
    auto *light = get<RasterizerLight3DComponent>(light_instance->light);
    ERR_FAIL_COND(!light);

    uint32_t x, y, width, height;

    float dp_direction = 0.0;
    float zfar = 0;
    bool flip_facing = false;
    int custom_vp_size = 0;
    GLuint fbo;
    int current_cubemap = -1;
    float bias = 0;
    float normal_bias = 0;

    state.used_depth_prepass = false;

    CameraMatrix light_projection;
    Transform light_transform;

    if (light->type == RS::LIGHT_DIRECTIONAL) {
        //set pssm stuff
        if (light_instance->last_scene_shadow_pass != scene_pass) {
            //assign rect if unassigned
            light_instance->light_directional_index = directional_shadow.current_light;
            light_instance->last_scene_shadow_pass = scene_pass;
            directional_shadow.current_light++;

            if (directional_shadow.light_count == 1) {
                light_instance->directional_rect = Rect2(0, 0, directional_shadow.size, directional_shadow.size);
            } else if (directional_shadow.light_count == 2) {
                light_instance->directional_rect = Rect2(0, 0, directional_shadow.size, directional_shadow.size / 2);
                if (light_instance->light_directional_index == 1) {
                    light_instance->directional_rect.position.x += light_instance->directional_rect.size.x;
                }
            } else { //3 and 4
                light_instance->directional_rect = Rect2(0, 0, directional_shadow.size / 2, directional_shadow.size / 2);
                if (light_instance->light_directional_index & 1) {
                    light_instance->directional_rect.position.x += light_instance->directional_rect.size.x;
                }
                if (light_instance->light_directional_index / 2) {
                    light_instance->directional_rect.position.y += light_instance->directional_rect.size.y;
                }
            }
        }

        light_projection = light_instance->shadow_transform[p_pass].camera;
        light_transform = light_instance->shadow_transform[p_pass].transform;

        x = light_instance->directional_rect.position.x;
        y = light_instance->directional_rect.position.y;
        width = light_instance->directional_rect.size.x;
        height = light_instance->directional_rect.size.y;

        if (light->directional_shadow_mode == RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS) {

            width /= 2;
            height /= 2;

            if (p_pass == 1) {
                x += width;
            } else if (p_pass == 2) {
                y += height;
            } else if (p_pass == 3) {
                x += width;
                y += height;
            }

        } else if (light->directional_shadow_mode == RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS) {

            height /= 2;

            if (p_pass == 0) {

            } else {
                y += height;
            }
        }

        float bias_mult = Math::lerp(1.0f, light_instance->shadow_transform[p_pass].bias_scale, light->param[RS::LIGHT_PARAM_SHADOW_BIAS_SPLIT_SCALE]);
        zfar = light->param[RS::LIGHT_PARAM_RANGE];
        bias = light->param[RS::LIGHT_PARAM_SHADOW_BIAS] * bias_mult;
        normal_bias = light->param[RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS] * bias_mult;
        fbo = directional_shadow.fbo;

    } else {
        //set from shadow atlas

        auto *shadow_atlas = get<RasterizerShadowAtlasComponent>(p_shadow_atlas);
        ERR_FAIL_COND(!shadow_atlas);
        ERR_FAIL_COND(!shadow_atlas->shadow_owners.contains(p_light));

        fbo = shadow_atlas->fbo;

        uint32_t key = shadow_atlas->shadow_owners[p_light];

        uint32_t quadrant = (key >> RasterizerShadowAtlasComponent::QUADRANT_SHIFT) & 0x3;
        uint32_t shadow = key & RasterizerShadowAtlasComponent::SHADOW_INDEX_MASK;

        ERR_FAIL_INDEX((int)shadow, shadow_atlas->quadrants[quadrant].shadows.size());

        uint32_t quadrant_size = shadow_atlas->size >> 1;

        x = (quadrant & 1) * quadrant_size;
        y = (quadrant >> 1) * quadrant_size;

        uint32_t shadow_size = (quadrant_size / shadow_atlas->quadrants[quadrant].subdivision);
        x += (shadow % shadow_atlas->quadrants[quadrant].subdivision) * shadow_size;
        y += (shadow / shadow_atlas->quadrants[quadrant].subdivision) * shadow_size;

        width = shadow_size;
        height = shadow_size;

        if (light->type == RS::LIGHT_OMNI) {

            if (light->omni_shadow_mode == RS::LIGHT_OMNI_SHADOW_CUBE) {

                int cubemap_index = shadow_cubemaps.size() - 1;

                for (int i = shadow_cubemaps.size() - 1; i >= 0; i--) {
                    //find appropriate cubemap to render to
                    if (shadow_cubemaps[i].size > shadow_size) {
                        break;
                    }

                    cubemap_index = i;
                }

                fbo = shadow_cubemaps[cubemap_index].fbo_id[p_pass];
                light_projection = light_instance->shadow_transform[0].camera;
                light_transform = light_instance->shadow_transform[0].transform;
                custom_vp_size = shadow_cubemaps[cubemap_index].size;
                zfar = light->param[RS::LIGHT_PARAM_RANGE];

                current_cubemap = cubemap_index;

            } else {

                light_projection = light_instance->shadow_transform[0].camera;
                light_transform = light_instance->shadow_transform[0].transform;

                if (light->omni_shadow_detail == RS::LIGHT_OMNI_SHADOW_DETAIL_HORIZONTAL) {

                    height /= 2;
                    y += p_pass * height;
                } else {
                    width /= 2;
                    x += p_pass * width;
                }

                dp_direction = p_pass == 0 ? 1.0 : -1.0;
                flip_facing = (p_pass == 1);
                zfar = light->param[RS::LIGHT_PARAM_RANGE];
                bias = light->param[RS::LIGHT_PARAM_SHADOW_BIAS];

                state.scene_shader.set_conditional(SceneShaderGLES3::RENDER_DEPTH_DUAL_PARABOLOID, true);
            }

        } else if (light->type == RS::LIGHT_SPOT) {

            light_projection = light_instance->shadow_transform[0].camera;
            light_transform = light_instance->shadow_transform[0].transform;

            dp_direction = 1.0;
            flip_facing = false;
            zfar = light->param[RS::LIGHT_PARAM_RANGE];
            bias = light->param[RS::LIGHT_PARAM_SHADOW_BIAS];
            normal_bias = light->param[RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS];
        }
    }

    render_list.clear();
    _fill_render_list(p_cull_result, true, true);

    render_list.sort_by_depth(false); //shadow is front to back for performance

    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glDepthMask(true);
    glColorMask(0, 0, 0, 0);

    if (custom_vp_size) {
        glViewport(0, 0, custom_vp_size, custom_vp_size);
        glScissor(0, 0, custom_vp_size, custom_vp_size);

    } else {
        glViewport(x, y, width, height);
        glScissor(x, y, width, height);
    }

    glEnable(GL_SCISSOR_TEST);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    state.ubo_data.z_offset = bias;
    state.ubo_data.z_slope_scale = normal_bias;
    state.ubo_data.shadow_dual_paraboloid_render_side = dp_direction;
    state.ubo_data.shadow_dual_paraboloid_render_zfar = zfar;
    state.ubo_data.opaque_prepass_threshold = 0.1f;

    if (storage->config.async_compilation_enabled) {
        // Avoid GL UB message id 131222 caused by shadow samplers not properly set up in the ubershader
        glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 6);
        glBindTexture(GL_TEXTURE_2D, storage->resources.depth_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    }

    _setup_environment(this,nullptr, light_projection, light_transform);

    state.scene_shader.set_conditional(SceneShaderGLES3::RENDER_DEPTH, true);

    if (light->reverse_cull) {
        flip_facing = !flip_facing;
    }
    _render_list(render_list.elements, light_transform, light_projection, nullptr, flip_facing, false, true, false, false);

    state.scene_shader.set_conditional(SceneShaderGLES3::RENDER_DEPTH, false);
    state.scene_shader.set_conditional(SceneShaderGLES3::RENDER_DEPTH_DUAL_PARABOLOID, false);

    if (light->type == RS::LIGHT_OMNI && light->omni_shadow_mode == RS::LIGHT_OMNI_SHADOW_CUBE && p_pass == 5) {
        //convert the chosen cubemap to dual paraboloid!

        RasterizerShadowAtlasComponent *shadow_atlas = get<RasterizerShadowAtlasComponent>(p_shadow_atlas);

        glBindFramebuffer(GL_FRAMEBUFFER, shadow_atlas->fbo);
        state.cube_to_dp_shader.bind();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, shadow_cubemaps[current_cubemap].cubemap);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glDisable(GL_CULL_FACE);

        for (int i = 0; i < 2; i++) {

            state.cube_to_dp_shader.set_uniform(CubeToDpShaderGLES3::Z_FLIP, i == 1);
            state.cube_to_dp_shader.set_uniform(CubeToDpShaderGLES3::Z_NEAR, light_projection.get_z_near());
            state.cube_to_dp_shader.set_uniform(CubeToDpShaderGLES3::Z_FAR, light_projection.get_z_far());
            state.cube_to_dp_shader.set_uniform(CubeToDpShaderGLES3::BIAS, light->param[RS::LIGHT_PARAM_SHADOW_BIAS]);

            uint32_t local_width = width, local_height = height;
            uint32_t local_x = x, local_y = y;
            if (light->omni_shadow_detail == RS::LIGHT_OMNI_SHADOW_DETAIL_HORIZONTAL) {

                local_height /= 2;
                local_y += i * local_height;
            } else {
                local_width /= 2;
                local_x += i * local_width;
            }

            glViewport(local_x, local_y, local_width, local_height);
            glScissor(local_x, local_y, local_width, local_height);
            glEnable(GL_SCISSOR_TEST);
            glClearDepth(1.0f);
            glClear(GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
            //glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            _copy_screen();
        }
    }

    glColorMask(1, 1, 1, 1);
}

void RasterizerSceneGLES3::set_scene_pass(uint64_t p_pass) {
    scene_pass = p_pass;
}

void RasterizerSceneGLES3::set_debug_draw_mode(RS::ViewportDebugDraw p_debug_draw) {

    state.debug_draw = p_debug_draw;
}

void RasterizerSceneGLES3::initialize() {

    render_pass = 0;

    state.scene_shader.init();

    {
        //default material and shader

        default_shader = storage->shader_create();
        storage->shader_set_code(default_shader, "shader_type spatial;\n");
        default_material = storage->material_create();
        storage->material_set_shader(default_material, default_shader);

        default_shader_twosided = storage->shader_create();
        default_material_twosided = storage->material_create();
        storage->shader_set_code(default_shader_twosided, "shader_type spatial; render_mode cull_disabled;\n");
        storage->material_set_shader(default_material_twosided, default_shader_twosided);

        //default for shaders using world coordinates (typical for triplanar)

        default_worldcoord_shader = storage->shader_create();
        storage->shader_set_code(default_worldcoord_shader, "shader_type spatial; render_mode world_vertex_coords;\n");
        default_worldcoord_material = storage->material_create();
        storage->material_set_shader(default_worldcoord_material, default_worldcoord_shader);

        default_worldcoord_shader_twosided = storage->shader_create();
        default_worldcoord_material_twosided = storage->material_create();
        storage->shader_set_code(default_worldcoord_shader_twosided, "shader_type spatial; render_mode cull_disabled,world_vertex_coords;\n");
        storage->material_set_shader(default_worldcoord_material_twosided, default_worldcoord_shader_twosided);
    }

    {
        //default material and shader

        default_overdraw_shader = storage->shader_create();
        storage->shader_set_code(default_overdraw_shader, "shader_type spatial;\nrender_mode blend_add,unshaded;\n void "
                                                                 "fragment() { ALBEDO=vec3(0.4,0.8,0.8); ALPHA=0.1; }");
        default_overdraw_material = storage->material_create();
        storage->material_set_shader(default_overdraw_material, default_overdraw_shader);
    }

    state.scene_ubo.create();
    glBindBuffer(GL_UNIFORM_BUFFER, state.scene_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(State::SceneDataUBO), &state.scene_ubo, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    state.env_radiance_ubo.create();
    glBindBuffer(GL_UNIFORM_BUFFER, state.env_radiance_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(State::EnvironmentRadianceUBO), &state.env_radiance_ubo, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    render_list.max_elements = GLOBAL_DEF_RST("rendering/limits/rendering/max_renderable_elements", (int)RenderListConstants::DEFAULT_MAX_ELEMENTS).as<int>();
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/rendering/max_renderable_elements", PropertyInfo(VariantType::INT, "rendering/limits/rendering/max_renderable_elements", PropertyHint::Range, "1024,65536,1"));
    render_list.max_lights = GLOBAL_DEF("rendering/limits/rendering/max_renderable_lights", (int)RenderListConstants::DEFAULT_MAX_LIGHTS).as<int>();
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/rendering/max_renderable_lights", PropertyInfo(VariantType::INT, "rendering/limits/rendering/max_renderable_lights", PropertyHint::Range, "16,4096,1"));
    render_list.max_reflections = GLOBAL_DEF("rendering/limits/rendering/max_renderable_reflections", (int)RenderListConstants::DEFAULT_MAX_REFLECTIONS).as<int>();
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/rendering/max_renderable_reflections", PropertyInfo(VariantType::INT, "rendering/limits/rendering/max_renderable_reflections", PropertyHint::Range, "8,1024,1"));
    render_list.max_lights_per_object = GLOBAL_DEF_RST("rendering/limits/rendering/max_lights_per_object", (int)RenderListConstants::DEFAULT_MAX_LIGHTS_PER_OBJECT).as<int>();
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/rendering/max_lights_per_object", PropertyInfo(VariantType::INT, "rendering/limits/rendering/max_lights_per_object", PropertyHint::Range, "8,1024,1"));

    {
        //quad buffers

        state.sky_verts.create();
        glBindBuffer(GL_ARRAY_BUFFER, state.sky_verts);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vector3) * 8, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

        state.sky_array.create();
        glBindVertexArray(state.sky_array);
        glBindBuffer(GL_ARRAY_BUFFER, state.sky_verts);
        glVertexAttribPointer(RS::ARRAY_VERTEX, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3) * 2, nullptr);
        glEnableVertexAttribArray(RS::ARRAY_VERTEX);
        glVertexAttribPointer(RS::ARRAY_TEX_UV, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3) * 2, CAST_INT_TO_UCHAR_PTR(sizeof(Vector3)));
        glEnableVertexAttribArray(RS::ARRAY_TEX_UV);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
    }
    render_list.init();
    state.cube_to_dp_shader.init();

    shadow_atlas_realloc_tolerance_msec = 500;

    int max_shadow_cubemap_sampler_size = MIN(int(T_GLOBAL_GET<int>("rendering/quality/shadow_atlas/cubemap_size")), storage->config.max_cubemap_texture_size);

    int cube_size = max_shadow_cubemap_sampler_size;

    glActiveTexture(GL_TEXTURE0);

    while (cube_size >= 32) {

        ShadowCubeMap cube;
        cube.size = cube_size;

        cube.cubemap.create();
        glBindTexture(GL_TEXTURE_CUBE_MAP, cube.cubemap);
        //gen cubemap first
        for (int i = 0; i < 6; i++) {

            glTexImage2D(_cube_side_enum[i], 0, GL_DEPTH_COMPONENT24, cube.size, cube.size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // Remove artifact on the edges of the shadowmap
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        //gen renderbuffers second, because it needs a complete cubemap
        cube.fbo_id.create();
        for (int i = 0; i < 6; i++) {

            glBindFramebuffer(GL_FRAMEBUFFER, cube.fbo_id[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _cube_side_enum[i], cube.cubemap, 0);

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            ERR_CONTINUE(status != GL_FRAMEBUFFER_COMPLETE);
        }

        shadow_cubemaps.emplace_back(eastl::move(cube));

        cube_size >>= 1;
    }

    directional_shadow_create();

    {
        //spot and omni ubos

        int max_ubo_size;
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_ubo_size);
        const int ubo_light_size = 160;
        state.ubo_light_size = ubo_light_size;
        state.max_ubo_lights = MIN(render_list.max_lights, max_ubo_size / ubo_light_size);

        state.spot_array_tmp = (uint8_t *)memalloc(ubo_light_size * state.max_ubo_lights);
        state.omni_array_tmp = (uint8_t *)memalloc(ubo_light_size * state.max_ubo_lights);

        state.spot_array_ubo.create();
        glBindBuffer(GL_UNIFORM_BUFFER, state.spot_array_ubo);
        glBufferData(GL_UNIFORM_BUFFER, ubo_light_size * state.max_ubo_lights, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        state.omni_array_ubo.create();
        glBindBuffer(GL_UNIFORM_BUFFER, state.omni_array_ubo);
        glBufferData(GL_UNIFORM_BUFFER, ubo_light_size * state.max_ubo_lights, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        state.directional_ubo.create();
        glBindBuffer(GL_UNIFORM_BUFFER, state.directional_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(LightDataUBO), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        state.max_forward_lights_per_object = MIN(state.max_ubo_lights, render_list.max_lights_per_object);

        state.scene_shader.add_custom_define("#define MAX_LIGHT_DATA_STRUCTS " + ::to_string(state.max_ubo_lights) + "\n");
        state.scene_shader.add_custom_define("#define MAX_FORWARD_LIGHTS " + ::to_string(state.max_forward_lights_per_object) + "\n");

        state.max_ubo_reflections = MIN(render_list.max_reflections, max_ubo_size / (int)sizeof(ReflectionProbeDataUBO));

        state.reflection_array_tmp = (uint8_t *)memalloc(sizeof(ReflectionProbeDataUBO) * state.max_ubo_reflections);

        state.reflection_array_ubo.create();
        glBindBuffer(GL_UNIFORM_BUFFER, state.reflection_array_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(ReflectionProbeDataUBO) * state.max_ubo_reflections, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        state.scene_shader.add_custom_define("#define MAX_REFLECTION_DATA_STRUCTS " + to_string(state.max_ubo_reflections) + "\n");

        state.max_skeleton_bones = MIN(2048, max_ubo_size / (12 * sizeof(float)));
        state.scene_shader.add_custom_define("#define MAX_SKELETON_BONES " + to_string(state.max_skeleton_bones) + "\n");
    }

    shadow_filter_mode = SHADOW_FILTER_NEAREST;

    { //reflection cubemaps
        int max_reflection_cubemap_sampler_size = 512;

        int rcube_size = max_reflection_cubemap_sampler_size;

        glActiveTexture(GL_TEXTURE0);

        GLenum internal_format = GL_RGBA16F;
        GLenum format = GL_RGBA;
        GLenum type = GL_HALF_FLOAT;

        while (rcube_size >= 32) {

            RasterizerReflectionCubeMap cube;
            cube.size = rcube_size;

            cube.depth.create();
            glBindTexture(GL_TEXTURE_2D, cube.depth);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, cube.size, cube.size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            cube.cubemap.create();
            glBindTexture(GL_TEXTURE_CUBE_MAP, cube.cubemap);
            //gen cubemap first
            for (int i = 0; i < 6; i++) {

                glTexImage2D(_cube_side_enum[i], 0, internal_format, cube.size, cube.size, 0, format, type, nullptr);
            }

            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            // Remove artifact on the edges of the reflectionmap
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            //gen renderbuffers second, because it needs a complete cubemap
            cube.fbo_id.create();
            for (int i = 0; i < 6; i++) {

                glBindFramebuffer(GL_FRAMEBUFFER, cube.fbo_id[i]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _cube_side_enum[i], cube.cubemap, 0);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, cube.depth, 0);

                GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                ERR_CONTINUE(status != GL_FRAMEBUFFER_COMPLETE);
            }

            reflection_cubemaps.emplace_back(eastl::move(cube));

            rcube_size >>= 1;
        }
    }

    {

        uint32_t immediate_buffer_size = T_GLOBAL_DEF<uint32_t>("rendering/limits/buffers/immediate_buffer_size_kb", 2048);
        ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/buffers/immediate_buffer_size_kb", PropertyInfo(VariantType::INT, "rendering/limits/buffers/immediate_buffer_size_kb", PropertyHint::Range, "0,8192,1,or_greater"));

        state.immediate_buffer.create();
        glBindBuffer(GL_ARRAY_BUFFER, state.immediate_buffer);
        glBufferData(GL_ARRAY_BUFFER, immediate_buffer_size * 1024, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        state.immediate_array.create();
    }

    //"desktop" opengl needs this.
    glEnable(GL_PROGRAM_POINT_SIZE);

    state.resolve_shader.init();
    state.ssr_shader.init();
    state.effect_blur_shader.init();
    state.sss_shader.init();
    state.ssao_minify_shader.init();
    state.ssao_shader.init();
    state.ssao_blur_shader.init();
    state.exposure_shader.init();
    state.tonemap_shader.init();

    {
        GLOBAL_DEF("rendering/quality/subsurface_scattering/quality", 1);
        ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/subsurface_scattering/quality", PropertyInfo(VariantType::INT, "rendering/quality/subsurface_scattering/quality", PropertyHint::Enum, "Low,Medium,High"));
        GLOBAL_DEF("rendering/quality/subsurface_scattering/scale", 1.0);
        ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/subsurface_scattering/scale", PropertyInfo(VariantType::INT, "rendering/quality/subsurface_scattering/scale", PropertyHint::Range, "0.01,8,0.01"));
        GLOBAL_DEF("rendering/quality/subsurface_scattering/follow_surface", false);
        GLOBAL_DEF("rendering/quality/subsurface_scattering/weight_samples", true);

        GLOBAL_DEF("rendering/quality/voxel_cone_tracing/high_quality", false);
    }

    exposure_shrink_size = 243;
    int max_exposure_shrink_size = exposure_shrink_size;

    while (max_exposure_shrink_size > 0) {

        RasterizerRenderTargetComponent::Exposure e;

        e.fbo.create();
        glBindFramebuffer(GL_FRAMEBUFFER, e.fbo);

        e.color.create();
        glBindTexture(GL_TEXTURE_2D, e.color);

        if (storage->config.framebuffer_float_supported) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, max_exposure_shrink_size, max_exposure_shrink_size, 0, GL_RED, GL_FLOAT, nullptr);
        } else if (storage->config.framebuffer_half_float_supported) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, max_exposure_shrink_size, max_exposure_shrink_size, 0, GL_RED, GL_HALF_FLOAT, nullptr);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, max_exposure_shrink_size, max_exposure_shrink_size, 0, GL_RED, GL_UNSIGNED_INT_2_10_10_10_REV, nullptr);
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, e.color, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        exposure_shrink.emplace_back(eastl::move(e));
        max_exposure_shrink_size /= 3;

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        ERR_CONTINUE(status != GL_FRAMEBUFFER_COMPLETE);
    }

    state.debug_draw = RS::VIEWPORT_DEBUG_DRAW_DISABLED;

    glFrontFace(GL_CW);
    if (storage->config.async_compilation_enabled) {
        state.scene_shader.init_async_compilation();
    }
}

void RasterizerSceneGLES3::iteration() {
    shadow_filter_mode = GLOBAL_GET("rendering/quality/shadows/filter_mode").as<ShadowFilterMode>();

    const int directional_shadow_size_new = next_power_of_2(T_GLOBAL_GET<int>("rendering/quality/directional_shadow/size"));
    if (directional_shadow_size != directional_shadow_size_new) {
        directional_shadow_size = directional_shadow_size_new;
        directional_shadow_create();
    }
    subsurface_scatter_follow_surface = GLOBAL_GET("rendering/quality/subsurface_scattering/follow_surface").as<bool>();
    subsurface_scatter_weight_samples = GLOBAL_GET("rendering/quality/subsurface_scattering/weight_samples").as<bool>();
    subsurface_scatter_quality = GLOBAL_GET("rendering/quality/subsurface_scattering/quality").as<SubSurfaceScatterQuality>();
    subsurface_scatter_size = GLOBAL_GET("rendering/quality/subsurface_scattering/scale").as<float>();

    storage->config.use_lightmap_filter_bicubic = T_GLOBAL_GET<bool>("rendering/quality/lightmapping/use_bicubic_sampling");
    state.scene_shader.set_conditional(SceneShaderGLES3::USE_LIGHTMAP_FILTER_BICUBIC, storage->config.use_lightmap_filter_bicubic);
    state.scene_shader.set_conditional(SceneShaderGLES3::VCT_QUALITY_HIGH, GLOBAL_GET("rendering/quality/voxel_cone_tracing/high_quality").as<bool>());
}

void RasterizerSceneGLES3::finalize() {
}

RasterizerSceneGLES3::RasterizerSceneGLES3() {
    directional_shadow_size = next_power_of_2(T_GLOBAL_GET<int>("rendering/quality/directional_shadow/size"));
}

RasterizerSceneGLES3::~RasterizerSceneGLES3() {

    assert(VSG::ecs);
    VSG::ecs->registry.destroy(default_material);
    VSG::ecs->registry.destroy(default_material_twosided);
    VSG::ecs->registry.destroy(default_shader);
    VSG::ecs->registry.destroy(default_shader_twosided);

    VSG::ecs->registry.destroy(default_worldcoord_material);
    VSG::ecs->registry.destroy(default_worldcoord_material_twosided);
    VSG::ecs->registry.destroy(default_worldcoord_shader);
    VSG::ecs->registry.destroy(default_worldcoord_shader_twosided);

    VSG::ecs->registry.destroy(default_overdraw_material);
    VSG::ecs->registry.destroy(default_overdraw_shader);

    memfree(state.spot_array_tmp);
    memfree(state.omni_array_tmp);
    memfree(state.reflection_array_tmp);
}
