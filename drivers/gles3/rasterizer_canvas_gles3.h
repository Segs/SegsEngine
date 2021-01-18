/*************************************************************************/
/*  rasterizer_canvas_gles3.h                                            */
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

#include "rasterizer_canvas_batcher.h"
#include "rasterizer_canvas_base_gles3.h"

class RasterizerCanvasGLES3 : public RasterizerCanvasBaseGLES3, public RasterizerCanvasBatcher<RasterizerCanvasGLES3, RasterizerStorageGLES3> {
    friend class RasterizerCanvasBatcher<RasterizerCanvasGLES3, RasterizerStorageGLES3>;

private:
    struct BatchGLData {
        // for batching
        GLuint batch_vertex_array[5];
    } batch_gl_data;

public:
    virtual void canvas_render_items_begin(const Color &p_modulate, Light3D *p_light, const Transform2D &p_base_transform);
    virtual void canvas_render_items_end();
    virtual void canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light3D *p_light, const Transform2D &p_base_transform);
    virtual void canvas_begin();
    virtual void canvas_end();

private:
    // legacy codepath .. to remove after testing
    void _legacy_canvas_render_item(Item *p_ci, RenderItemState &r_ris);

    // high level batch funcs
    void canvas_render_items_implementation(Item *p_item_list, int p_z, const Color &p_modulate, Light3D *p_light, const Transform2D &p_base_transform);
    void render_joined_item(const BItemJoined &p_bij, RenderItemState &r_ris);
    bool try_join_item(Item *p_ci, RenderItemState &r_ris, bool &r_batch_break);
    void render_batches(Item::Command *const *p_commands, Item *p_current_clip, bool &r_reclip, RasterizerStorageGLES3::Material *p_material);

    // low level batch funcs
    void _batch_upload_buffers();
    void _batch_render_prepare();
    void _batch_render_generic(const Batch &p_batch, RasterizerStorageGLES3::Material *p_material);
    void _batch_render_lines(const Batch &p_batch, RasterizerStorageGLES3::Material *p_material, bool p_anti_alias);

    // funcs used from rasterizer_canvas_batcher template
    void gl_enable_scissor(int p_x, int p_y, int p_width, int p_height) const;
    void gl_disable_scissor() const;

    void gl_checkerror();

public:

    void initialize();
    RasterizerCanvasGLES3();
};
// Should not be exported, it's a helper func.
extern void store_transform(const Transform& p_mtx, float* p_array);
inline void store_camera(const CameraMatrix& p_mtx, float* p_array) {

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            p_array[i * 4 + j] = p_mtx.matrix[i][j];
        }
    }
}
static constexpr const GLenum gl_primitive[] = {
    GL_POINTS,
    GL_LINES,
    GL_LINE_STRIP,
    GL_LINE_LOOP,
    GL_TRIANGLES,
    GL_TRIANGLE_STRIP,
    GL_TRIANGLE_FAN
};
static constexpr const GLenum _cube_side_enum[6] = {

    GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z,

};
