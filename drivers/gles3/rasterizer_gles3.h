/*************************************************************************/
/*  rasterizer_gles3.h                                                   */
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

#pragma once

#include "core/error_list.h"                 // for Error
#include "core/reference.h"                  // for Ref
#include "servers/rendering/rasterizer.h"  // for Rasterizer, RasterizerCanvas ...
class Image;
class RasterizerCanvasGLES3;
class RasterizerSceneGLES3;
class RasterizerStorageGLES3;
struct Color;
struct Rect2;
struct Vector2;

class RasterizerGLES3 : public Rasterizer {

    static Rasterizer *_create_current();

    RasterizerStorageGLES3 *storage;
    RasterizerCanvasGLES3 *canvas;
    RasterizerSceneGLES3 *scene;

    double time_total = 0;
    float time_scale=1.0f;

public:
    RasterizerStorage *get_storage() override;
    RasterizerCanvas *get_canvas() override;
    RasterizerScene *get_scene() override;

    void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) override;
    void set_shader_time_scale(float p_scale) override;

    void initialize() override;
    void begin_frame(double frame_step) override;
    void set_current_render_target(RenderingEntity p_render_target) override;
    void restore_render_target(bool p_3d_was_drawn) override;
    void clear_render_target(const Color &p_color) override;
    void blit_render_target_to_screen(RenderingEntity p_render_target, const Rect2 &p_screen_rect, int p_screen = 0) override;
    void output_lens_distorted_to_screen(RenderingEntity p_render_target, const Rect2 &p_screen_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample) override;
    void end_frame(bool p_swap_buffers) override;
    void finalize() override;

    static Error is_viable();
    static void make_current();
    static void register_config();

    static bool gl_check_errors();

    RasterizerGLES3(const RasterizerGLES3 &) = delete;
    RasterizerGLES3 &operator=(const RasterizerGLES3 &) = delete;

    RasterizerGLES3();
    ~RasterizerGLES3() override;
};
