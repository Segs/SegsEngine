/*************************************************************************/
/*  rasterizer_canvas_base_gles3.h                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#pragma once

#include "rasterizer_storage_gles3.h"
#include "rasterizer_gl_unique_handle.h"
#include "servers/rendering/rasterizer.h"

#include "gles3/shaders/canvas_shadow.glsl.gen.h"
#include "gles3/shaders/canvas.glsl.gen.h"
#include "gles3/shaders/lens_distorted.glsl.gen.h"

class RasterizerSceneGLES3;
struct RasterizerTextureComponent;


class RasterizerCanvasBaseGLES3 : public RasterizerCanvas {
public:
    struct CanvasItemUBO {

        float projection_matrix[16];
        float time;
        uint8_t padding[12];
    };

    RasterizerSceneGLES3 *scene_render;

    struct Data {

        enum { NUM_QUAD_ARRAY_VARIATIONS = 8 };

        GLBufferHandle canvas_quad_vertices;
        GLVAOHandle canvas_quad_array;

        GLBufferHandle polygon_buffer;
        GLMultiVAOHandle<NUM_QUAD_ARRAY_VARIATIONS> polygon_buffer_quad_arrays;
        GLVAOHandle polygon_buffer_pointer_array;
        GLBufferHandle polygon_index_buffer;

        GLBufferHandle particle_quad_vertices;
        GLVAOHandle particle_quad_array;

        uint32_t polygon_buffer_size;
        uint32_t polygon_index_buffer_size;

    } data;

    struct State {
        CanvasItemUBO canvas_item_ubo_data;
        GLBufferHandle canvas_item_ubo;
        bool canvas_texscreen_used;
        CanvasShaderGLES3 canvas_shader;
        CanvasShadowShaderGLES3 canvas_shadow_shader;
        LensDistortedShaderGLES3 lens_shader;

        bool using_texture_rect;
        bool using_ninepatch;

        bool using_light_angle;
        bool using_modulate;
        bool using_large_vertex;

        RenderingEntity current_tex;
        RenderingEntity current_normal;
        RenderingEntity current_tex_ptr; // can be a proxy from current_tex

        Transform vp;

        Color canvas_item_modulate;
        Transform2D extra_matrix;
        Transform2D final_transform;
        bool using_skeleton;
        Transform2D skeleton_transform;
        Transform2D skeleton_transform_inverse;

    } state;

    RasterizerStorageGLES3 *storage;

    // allow user to choose api usage
    GLenum _buffer_upload_usage_flag;

    RenderingEntity light_internal_create() override;
    void light_internal_update(RenderingEntity p_rid, RasterizerCanvasLight3DComponent *p_light) override;
    void light_internal_free(RenderingEntity p_rid) override;

    void canvas_begin() override;
    void canvas_end() override;

    void _set_texture_rect_mode(bool p_enable, bool p_ninepatch = false, bool p_light_angle = false, bool p_modulate = false, bool p_large_vertex = false);
    RasterizerTextureComponent *_bind_canvas_texture(RenderingEntity p_texture, RenderingEntity p_normal_map, bool p_force = false);

    void _draw_gui_primitive(int p_points, const Vector2 *p_vertices, const Color *p_colors, const Vector2 *p_uvs, const float *p_light_angles = nullptr);
    void _draw_polygon(const int *p_indices, int p_index_count, int p_vertex_count, const Vector2 *p_vertices, const Vector2 *p_uvs, const Color *p_colors, bool p_singlecolor, const int *p_bones, const float *p_weights);
    void _draw_generic(GLuint p_primitive, int p_vertex_count, const Vector2 *p_vertices, const Vector2 *p_uvs, const Color *p_colors, bool p_singlecolor);
    void _draw_generic_indices(GLuint p_primitive, const int *p_indices, int p_index_count, int p_vertex_count, const Vector2 *p_vertices, const Vector2 *p_uvs, const Color *p_colors, bool p_singlecolor);

    void _copy_texscreen(Rect2 p_rect);

    void canvas_debug_viewport_shadows(Span<RasterizerCanvasLight3DComponent *> p_lights_with_shadow) override;

    void canvas_light_shadow_buffer_update(RenderingEntity p_buffer, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, RenderingEntity p_occluders, CameraMatrix *p_xform_cache) override;

    void reset_canvas() override;

    void draw_generic_textured_rect(Rect2 p_rect, const Rect2 &p_src);
    void draw_lens_distortion_rect(const Rect2 &p_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample);
    void render_rect_nvidia_workaround(const Item::CommandRect *p_rect, const RasterizerTextureComponent *p_texture);

    void initialize();
    void finalize();

    void draw_window_margins(int *black_margin, RenderingEntity *black_image) override;

    RasterizerCanvasBaseGLES3();
};
