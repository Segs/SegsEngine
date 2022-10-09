/*************************************************************************/
/*  rasterizer_storage_gles3.cpp                                         */
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

#include "rasterizer_storage_gles3.h"
#include "shader_cache_gles3.h"

#include "rasterizer_gi_probe_component.h"
#include "rasterizer_immediate_geometry_component.h"
#include "rasterizer_lightmap_capture_component.h"
#include "rasterizer_mesh_component.h"
#include "rasterizer_particle_component.h"
#include "rasterizer_reflection_probe_component.h"
#include "rasterizer_skeleton_component.h"
#include "rasterizer_sky_component.h"
#include "rasterizer_canvas_light_shadow_component.h"
#include "rasterizer_canvas_occluder_component.h"
#include "rasterizer_scene_gles3.h"
#include "rasterizer_shader_component.h"
#include "rasterizer_material_component.h"
#include "rasterizer_texture_component.h"
#include "rasterizer_multimesh_component.h"
#include "rasterizer_common_geometry_component.h"
#include "rasterizer_dependent_entities_component.h"
#include "rasterizer_light3d_component.h"
#include "core/print_string.h"
#include "core/engine.h"
#include "core/project_settings.h"
#include "core/string_utils.h"
#include "core/threaded_callable_queue.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/rendering_server_scene.h"
#include "core/os/os.h"
#include "main/main_class.h"
#include "glad/glad.h"

/* TEXTURE API */


void glTexStorage2DCustom(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type) {
    for (int i = 0; i < levels; i++) {
        glTexImage2D(target, i, internalformat, width, height, 0, format, type, nullptr);
        width = M_MAX(1, (width / 2));
        height = M_MAX(1, (height / 2));
    }
}

GLuint RasterizerStorageGLES3::system_fbo = 0;


////////

void RasterizerStorageGLES3::instance_add_dependency(RenderingEntity p_base, RenderingEntity p_instance) {

    RasterizerInstantiableComponent *inst;
    auto comp_inst = get<RenderingInstanceComponent>(p_instance);
    assert(comp_inst->instance_owner==entt::null);

    switch (comp_inst->base_type) {
        case RS::INSTANCE_MESH:
        case RS::INSTANCE_MULTIMESH:
        case RS::INSTANCE_IMMEDIATE:
        case RS::INSTANCE_PARTICLES:
        case RS::INSTANCE_LIGHT:
        case RS::INSTANCE_REFLECTION_PROBE:
        case RS::INSTANCE_GI_PROBE:
        case RS::INSTANCE_LIGHTMAP_CAPTURE: {
            inst = &VSG::ecs->registry.get<RasterizerInstantiableComponent>(p_base);
        } break;
        default: {
            ERR_FAIL();
        }
    }
    comp_inst->instance_owner = p_base;
    inst->instance_list.push_back(p_instance);
}

void RasterizerStorageGLES3::instance_remove_dependency(RenderingEntity p_base, RenderingEntity p_instance) {

    RasterizerInstantiableComponent *inst = nullptr;
    auto comp_inst = get<RenderingInstanceComponent>(p_instance);
    assert(comp_inst->instance_owner != entt::null);
    switch (comp_inst->base_type) {
        case RS::INSTANCE_MESH:
        case RS::INSTANCE_MULTIMESH:
        case RS::INSTANCE_IMMEDIATE:
        case RS::INSTANCE_PARTICLES:
        case RS::INSTANCE_LIGHT:
        case RS::INSTANCE_REFLECTION_PROBE:
        case RS::INSTANCE_GI_PROBE:
        case RS::INSTANCE_LIGHTMAP_CAPTURE: {
            inst = &VSG::ecs->registry.get<RasterizerInstantiableComponent>(p_base);
        } break;
        default: {
            CRASH_NOW_MSG("Unhandled type in instance_remove_dependency");
        }
    }

    inst->instance_list.erase_first(p_instance);
    comp_inst->instance_owner = entt::null;
}


RS::InstanceType RasterizerStorageGLES3::get_base_type(RenderingEntity p_rid) const {

    if (VSG::ecs->registry.any_of<RasterizerMeshComponent>(p_rid)) {
        return RS::INSTANCE_MESH;
    }

    if (VSG::ecs->registry.any_of<RasterizerMultiMeshComponent>(p_rid)) {
        return RS::INSTANCE_MULTIMESH;
    }

    if (VSG::ecs->registry.any_of<RasterizerImmediateGeometryComponent>(p_rid)) {
        return RS::INSTANCE_IMMEDIATE;
    }

    if (VSG::ecs->registry.any_of<RasterizerParticlesComponent>(p_rid)) {
        return RS::INSTANCE_PARTICLES;
    }

    if (VSG::ecs->registry.any_of<RasterizerLight3DComponent>(p_rid)) {
        return RS::INSTANCE_LIGHT;
    }

    if (VSG::ecs->registry.any_of<RasterizerReflectionProbeComponent>(p_rid)) {
        return RS::INSTANCE_REFLECTION_PROBE;
    }
    if (VSG::ecs->registry.any_of<RasterizerGIProbeComponent>(p_rid)) {
        return RS::INSTANCE_GI_PROBE;
    }

    if (VSG::ecs->registry.any_of<RasterizerLightmapCaptureComponent>(p_rid)) {
        return RS::INSTANCE_LIGHTMAP_CAPTURE;
    }

    return RS::INSTANCE_NONE;
}

bool RasterizerStorageGLES3::free(RenderingEntity p_rid) {
//    if(auto *texture=VSG::ecs->try_get<RasterizerTextureComponent>(p_rid)) {
//        ERR_FAIL_COND_V(texture->render_target!=entt::null, true); //can't free the render target texture, dude
//    }
    VSG::ecs->registry.destroy(p_rid);
    // Make sure first_directional_light is invalidated

    if (scene && p_rid == scene->first_directional_light) {
        scene->first_directional_light = entt::null;
    }
    return true;
}

bool RasterizerStorageGLES3::has_os_feature(const StringName &p_feature) const {

    if (p_feature == "bptc")
        return config.bptc_supported;

    if (p_feature == "s3tc")
        return config.s3tc_supported;

    return false;
}

////////////////////////////////////////////

void RasterizerStorageGLES3::set_debug_generate_wireframes(bool p_generate) {

    config.generate_wireframes = p_generate;
}

void RasterizerStorageGLES3::render_info_begin_capture() {

    get_rasterizer_storage_info().snap = get_rasterizer_storage_info().render;
}

void RasterizerStorageGLES3::render_info_end_capture() {

    get_rasterizer_storage_info().end_capture();

}

int RasterizerStorageGLES3::get_captured_render_info(RS::RenderInfo p_info) {

    switch (p_info) {
        case RS::INFO_OBJECTS_IN_FRAME: {

            return get_rasterizer_storage_info().snap.object_count;
        }
        case RS::INFO_VERTICES_IN_FRAME: {

            return get_rasterizer_storage_info().snap.vertices_count;
        } break;
        case RS::INFO_MATERIAL_CHANGES_IN_FRAME: {
            return get_rasterizer_storage_info().snap.material_switch_count;
        } break;
        case RS::INFO_SHADER_CHANGES_IN_FRAME: {
            return get_rasterizer_storage_info().snap.shader_rebind_count;
        } break;
        case RS::INFO_SHADER_COMPILES_IN_FRAME: {
            return get_rasterizer_storage_info().snap.shader_compiles_in_progress_count;
        } break;
        case RS::INFO_SURFACE_CHANGES_IN_FRAME: {
            return get_rasterizer_storage_info().snap.surface_switch_count;
        } break;
        case RS::INFO_DRAW_CALLS_IN_FRAME: {
            return get_rasterizer_storage_info().snap.draw_call_count;
        } break;
        case RS::INFO_2D_ITEMS_IN_FRAME: {
            return get_rasterizer_storage_info().snap._2d_item_count;
        } break;
        case RS::INFO_2D_DRAW_CALLS_IN_FRAME: {
            return get_rasterizer_storage_info().snap._2d_draw_call_count;
        } break;
        default: {
            return get_render_info(p_info);
        }
    }
}

uint64_t RasterizerStorageGLES3::get_render_info(RS::RenderInfo p_info) {

    switch (p_info) {
        case RS::INFO_OBJECTS_IN_FRAME:
            return get_rasterizer_storage_info().render_final.object_count;
        case RS::INFO_VERTICES_IN_FRAME:
            return get_rasterizer_storage_info().render_final.vertices_count;
        case RS::INFO_MATERIAL_CHANGES_IN_FRAME:
            return get_rasterizer_storage_info().render_final.material_switch_count;
        case RS::INFO_SHADER_CHANGES_IN_FRAME:
            return get_rasterizer_storage_info().render_final.shader_rebind_count;
        case RS::INFO_SHADER_COMPILES_IN_FRAME:
            return get_rasterizer_storage_info().render.shader_compiles_in_progress_count;
        case RS::INFO_SURFACE_CHANGES_IN_FRAME:
            return get_rasterizer_storage_info().render_final.surface_switch_count;
        case RS::INFO_DRAW_CALLS_IN_FRAME:
            return get_rasterizer_storage_info().render_final.draw_call_count;
        case RS::INFO_2D_ITEMS_IN_FRAME:
            return get_rasterizer_storage_info().render_final._2d_item_count;
        case RS::INFO_2D_DRAW_CALLS_IN_FRAME:
            return get_rasterizer_storage_info().render_final._2d_draw_call_count;
        case RS::INFO_USAGE_VIDEO_MEM_TOTAL:
            return 0; //no idea
        case RS::INFO_VIDEO_MEM_USED:
            return get_rasterizer_storage_info().vertex_mem + get_rasterizer_storage_info().texture_mem;
        case RS::INFO_TEXTURE_MEM_USED:
            return get_rasterizer_storage_info().texture_mem;
        case RS::INFO_VERTEX_MEM_USED:
            return get_rasterizer_storage_info().vertex_mem;
        default:
            return 0; //no idea either
    }
}

const char *RasterizerStorageGLES3::get_video_adapter_name() const {

    return (const char *)glGetString(GL_RENDERER);
}

const char *RasterizerStorageGLES3::get_video_adapter_vendor() const {

    return (const char *)glGetString(GL_VENDOR);
}

void RasterizerStorageGLES3::initialize() {

    RasterizerStorageGLES3::system_fbo = 0;

    //// extensions config
    ///

    {

        int max_extensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &max_extensions);
        for (int i = 0; i < max_extensions; i++) {
            const GLubyte *s = glGetStringi(GL_EXTENSIONS, i);
            if (!s) {
                break;
            }
            config.extensions.insert((const char *)s);
        }
    }

    config.shrink_textures_x2 = false;
    config.use_fast_texture_filter = ProjectSettings::get_singleton()->get("rendering/quality/filters/use_nearest_mipmap_filter").as<int>();

    config.latc_supported = config.extensions.contains("GL_EXT_texture_compression_latc");
    config.bptc_supported = config.extensions.contains("GL_ARB_texture_compression_bptc");

    config.s3tc_supported = true;
    config.rgtc_supported = true; //RGTC - core since OpenGL version 3.0
    config.texture_float_linear_supported = true;
    config.framebuffer_float_supported = true;
    config.framebuffer_half_float_supported = true;
    // not yet detected on GLES3 (is this mandated?)
    config.support_npot_repeat_mipmap = true;

    config.srgb_decode_supported = config.extensions.contains("GL_EXT_texture_sRGB_decode");

    config.anisotropic_level = 1.0;
    config.use_anisotropic_filter = config.extensions.contains("GL_EXT_texture_filter_anisotropic");
    if (config.use_anisotropic_filter) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &config.anisotropic_level);
        config.anisotropic_level = eastl::min(ProjectSettings::get_singleton()->getT<int>("rendering/quality/filters/anisotropic_filter_level"), config.anisotropic_level);
    }

    config.program_binary_supported = config.extensions.contains("GL_ARB_get_program_binary");
    config.parallel_shader_compile_supported = config.extensions.contains("GL_ARB_parallel_shader_compile") ||
            config.extensions.contains("GL_KHR_parallel_shader_compile");

    int compilation_mode = 0;

    if (!(Engine::get_singleton()->is_editor_hint() || Main::is_project_manager())) {
        compilation_mode = ProjectSettings::get_singleton()->getT<int>("rendering/gles3/shaders/shader_compilation_mode");
    }
    config.async_compilation_enabled = compilation_mode >= 1;
    config.shader_cache_enabled = compilation_mode == 2;

    if (config.async_compilation_enabled) {
        ShaderGLES3::max_simultaneous_compiles = M_MAX(1, ProjectSettings::get_singleton()->getT<int>("rendering/gles3/shaders/max_simultaneous_compiles"));
        if (GLAD_GL_ARB_parallel_shader_compile) {
            glMaxShaderCompilerThreadsARB(ShaderGLES3::max_simultaneous_compiles);
        } else if (GLAD_GL_KHR_parallel_shader_compile) {
            glMaxShaderCompilerThreadsKHR(ShaderGLES3::max_simultaneous_compiles);
        }

    } else {
        ShaderGLES3::max_simultaneous_compiles = 0;
    }

#ifdef DEBUG_ENABLED
    ShaderGLES3::log_active_async_compiles_count = ProjectSettings::get_singleton()->getT<bool>("rendering/gles3/shaders/log_active_async_compiles_count");
#endif
    frame.clear_request = false;

    shaders.compile_queue = nullptr;
    shaders.cache = nullptr;
    shaders.cache_write_queue = nullptr;
    bool effectively_on = false;
    if (config.async_compilation_enabled) {
        if (config.parallel_shader_compile_supported) {
            print_line("Async. shader compilation: ON (full native support)");
            effectively_on = true;
        } else if (config.program_binary_supported && OS::get_singleton()->is_offscreen_gl_available()) {
            shaders.compile_queue = memnew(ThreadedCallableQueue<GLuint>());
            shaders.compile_queue->enqueue(0, []() { OS::get_singleton()->set_offscreen_gl_current(true); });
            print_line("Async. shader compilation: ON (via secondary context)");
            effectively_on = true;
        } else {
            print_line("Async. shader compilation: OFF (enabled for " + String(Engine::get_singleton()->is_editor_hint() ? "editor" : "project") + ", but not supported)");
        }
        if (effectively_on) {
            if (config.shader_cache_enabled) {
                if (config.program_binary_supported) {
                    print_line("Shader cache: ON");
                    shaders.cache = memnew(ShaderCacheGLES3);
                    shaders.cache_write_queue = memnew(ThreadedCallableQueue<GLuint>());
                } else {
                    print_line("Shader cache: OFF (enabled, but not supported)");
                }
            } else {
                print_line("Shader cache: OFF");
            }
        }
    } else {
        print_line("Async. shader compilation: OFF");
    }
    ShaderGLES3::compile_queue = shaders.compile_queue;
    ShaderGLES3::parallel_compile_supported = config.parallel_shader_compile_supported;
    ShaderGLES3::shader_cache = shaders.cache;
    ShaderGLES3::cache_write_queue = shaders.cache_write_queue;
    shaders.copy.init();

    {
        // Generate default textures.

        // Opaque white color.

        resources.white_tex.create();
        unsigned char whitetexdata[8 * 8 * 3];
        for (int i = 0; i < 8 * 8 * 3; i++) {
            whitetexdata[i] = 255;
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, resources.white_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, whitetexdata);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Opaque black color.
        resources.black_tex.create();
        unsigned char blacktexdata[8 * 8 * 3];
        for (int i = 0; i < 8 * 8 * 3; i++) {
            blacktexdata[i] = 0;
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, resources.black_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, blacktexdata);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Transparent black color.
        resources.transparent_tex.create();
        unsigned char transparenttexdata[8 * 8 * 4];
        for (int i = 0; i < 8 * 8 * 4; i++) {
            transparenttexdata[i] = 0;
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, resources.transparent_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, transparenttexdata);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

         // Opaque "flat" normal map color.

        resources.normal_tex.create();
        unsigned char normaltexdata[8 * 8 * 3];
        for (int i = 0; i < 8 * 8 * 3; i += 3) {
            normaltexdata[i + 0] = 128;
            normaltexdata[i + 1] = 128;
            normaltexdata[i + 2] = 255;
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, resources.normal_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, normaltexdata);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        resources.aniso_tex.create();
        unsigned char anisotexdata[8 * 8 * 3];
        for (int i = 0; i < 8 * 8 * 3; i += 3) {
            anisotexdata[i + 0] = 255;
            anisotexdata[i + 1] = 128;
            anisotexdata[i + 2] = 0;
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, resources.aniso_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, anisotexdata);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);


        resources.depth_tex.create();
        unsigned char depthtexdata[8 * 8 * 2] = {};

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, resources.depth_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 8, 8, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, depthtexdata);
        glBindTexture(GL_TEXTURE_2D, 0);

        resources.white_tex_3d.create();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, resources.white_tex_3d);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 2, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, whitetexdata);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);

        resources.white_tex_array.create();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, resources.white_tex_array);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, 8, 8, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 8, 8, 1, GL_RGB, GL_UNSIGNED_BYTE, whitetexdata);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &config.max_texture_image_units);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &config.max_texture_size);
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &config.max_cubemap_texture_size);

    config.use_rgba_2d_shadows = !config.framebuffer_float_supported;

    //generic quadie for copying

    {
        //quad buffers

        resources.quadie.create();
        glBindBuffer(GL_ARRAY_BUFFER, resources.quadie);
        {
            const float qv[16] = {
                -1, -1, 0, 0,
                -1,  1, 0, 1,
                 1,  1, 1, 1,
                 1, -1, 1, 0,
            };

            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, qv, GL_STATIC_DRAW);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

        resources.quadie_array.create();
        glBindVertexArray(resources.quadie_array);
        glBindBuffer(GL_ARRAY_BUFFER, resources.quadie);
        glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(RS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, CAST_INT_TO_UCHAR_PTR(8));
        glEnableVertexAttribArray(4);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
    }

    //generic quadie for copying without touching sky

    {
        //transform feedback buffers
        uint32_t xf_feedback_size = GLOBAL_DEF_T_RST("rendering/limits/buffers/blend_shape_max_buffer_size_kb", 4096,uint32_t);
        ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/buffers/blend_shape_max_buffer_size_kb", PropertyInfo(VariantType::INT, "rendering/limits/buffers/blend_shape_max_buffer_size_kb", PropertyHint::Range, "0,8192,1,or_greater"));

        for (int i = 0; i < 2; i++) {

            glGenBuffers(1, &resources.transform_feedback_buffers[i]);
            glBindBuffer(GL_ARRAY_BUFFER, resources.transform_feedback_buffers[i]);
            glBufferData(GL_ARRAY_BUFFER, xf_feedback_size * 1024, nullptr, GL_STREAM_DRAW);
        }

        shaders.blend_shapes.init();

        resources.transform_feedback_array.create();
    }

    shaders.cubemap_filter.init();
    bool ggx_hq = GLOBAL_GET("rendering/quality/reflections/high_quality_ggx").as<bool>();
    shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::LOW_QUALITY, !ggx_hq);
    shaders.particles.init();
    if (config.async_compilation_enabled) {
        shaders.particles.init_async_compilation();
    }

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    frame.count = 0;
    frame.delta = 0;
    frame.current_rt = entt::null;
    config.keep_original_textures = false;
    config.generate_wireframes = false;
    config.use_texture_array_environment = GLOBAL_GET("rendering/quality/reflections/texture_array_reflections").as<bool>();

    config.force_vertex_shading = GLOBAL_GET("rendering/quality/shading/force_vertex_shading").as<bool>();

    String renderer = (const char *)glGetString(GL_RENDERER);
    GLOBAL_DEF("rendering/quality/lightmapping/use_bicubic_sampling", true);
    config.use_lightmap_filter_bicubic = T_GLOBAL_GET<bool>("rendering/quality/lightmapping/use_bicubic_sampling");

    config.use_physical_light_attenuation = T_GLOBAL_GET<bool>("rendering/quality/shading/use_physical_light_attenuation");

    config.use_depth_prepass = GLOBAL_GET("rendering/quality/depth_prepass/enable").as<bool>();
}

void RasterizerStorageGLES3::finalize() {

    resources.white_tex.release();
    resources.black_tex.release();
    resources.normal_tex.release();
}

void RasterizerStorageGLES3::update_dirty_resources() {

    update_dirty_multimeshes();
    update_dirty_skeletons();
    update_dirty_shaders();
    update_dirty_materials();
    update_particles();
}

RasterizerStorageGLES3::RasterizerStorageGLES3() {
}
RasterizerStorageGLES3::~RasterizerStorageGLES3()
{
    if (shaders.cache) {
        memdelete(shaders.cache);
    }
    if (shaders.cache_write_queue) {
        memdelete(shaders.cache_write_queue);
    }
    eastl::function<void()> job = []() { OS::get_singleton()->set_offscreen_gl_current(false); };
    if (shaders.compile_queue) {
        shaders.compile_queue->enqueue(0u,job);
        memdelete(shaders.compile_queue);
    }
}

RasterizerStorageInfo s_info;
RasterizerStorageInfo &get_rasterizer_storage_info()
{
    return s_info;
}
