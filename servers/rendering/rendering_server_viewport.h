/*************************************************************************/
/*  rendering_server_viewport.h                                             */
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

#include "rasterizer.h"
#include "servers/arvr/arvr_interface.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering_server.h"

struct RenderingViewportComponent;
struct RenderingViewportCanvasComponent;
struct RenderingCanvasComponent;
class VisualServerViewport {
public:
    Vector<RenderingEntity> active_viewports;

private:
    Color clear_color;
    void _draw_3d(RenderingViewportComponent *p_viewport, ARVREyes p_eye);
    void _draw_viewport(RenderingViewportComponent *p_viewport, RenderingViewportCanvasComponent *p_vp_canvas, ARVREyes p_eye = ARVREyes::EYE_MONO);

public:
    RenderingEntity viewport_create();

    void viewport_set_use_arvr(RenderingEntity p_viewport, bool p_use_arvr);

    void viewport_set_size(RenderingEntity p_viewport, int p_width, int p_height);

    void viewport_attach_to_screen(RenderingEntity p_viewport, const Rect2 &p_rect = Rect2(), int p_screen = 0);
    void viewport_detach(RenderingEntity p_viewport);

    void viewport_set_active(RenderingEntity p_viewport, bool p_active);
    void viewport_set_parent_viewport(RenderingEntity p_viewport, RenderingEntity p_parent_viewport);
    void viewport_set_update_mode(RenderingEntity p_viewport, RS::ViewportUpdateMode p_mode);
    void viewport_set_vflip(RenderingEntity p_viewport, bool p_enable);

    void viewport_set_clear_mode(RenderingEntity p_viewport, RS::ViewportClearMode p_clear_mode);

    RenderingEntity viewport_get_texture(RenderingEntity p_viewport) const;

    void viewport_set_hide_scenario(RenderingEntity p_viewport, bool p_hide);
    void viewport_set_hide_canvas(RenderingEntity p_viewport, bool p_hide);
    void viewport_set_disable_environment(RenderingEntity p_viewport, bool p_disable);
    void viewport_set_disable_3d(RenderingEntity p_viewport, bool p_disable);
    void viewport_set_keep_3d_linear(RenderingEntity p_viewport, bool p_keep_3d_linear);

    void viewport_attach_camera(RenderingEntity p_viewport, RenderingEntity p_camera);
    void viewport_set_scenario(RenderingEntity p_viewport, RenderingEntity p_scenario);
    void viewport_attach_canvas(RenderingEntity p_viewport, RenderingEntity p_canvas);
    void viewport_remove_canvas(RenderingEntity p_viewport, RenderingEntity p_canvas);
    void viewport_set_canvas_transform(RenderingEntity p_viewport, RenderingEntity p_canvas, const Transform2D &p_offset);
    void viewport_set_transparent_background(RenderingEntity p_viewport, bool p_enabled);

    void viewport_set_global_canvas_transform(RenderingEntity p_viewport, const Transform2D &p_transform);
    void viewport_set_canvas_stacking(RenderingEntity p_viewport, RenderingEntity p_canvas, int p_layer, int p_sublayer);

    void viewport_set_shadow_atlas_size(RenderingEntity p_viewport, int p_size);
    void viewport_set_shadow_atlas_quadrant_subdivision(RenderingEntity p_viewport, int p_quadrant, int p_subdiv);

    void viewport_set_msaa(RenderingEntity p_viewport, RS::ViewportMSAA p_msaa);
    void viewport_set_use_fxaa(RenderingEntity p_viewport, bool p_fxaa);
    void viewport_set_use_debanding(RenderingEntity p_viewport, bool p_debanding);
    void viewport_set_sharpen_intensity(RenderingEntity p_viewport, float p_intensity);
    void viewport_set_hdr(RenderingEntity p_viewport, bool p_enabled);
    void viewport_set_use_32_bpc_depth(RenderingEntity p_viewport, bool p_enabled);
    void viewport_set_usage(RenderingEntity p_viewport, RS::ViewportUsage p_usage);

    virtual uint64_t viewport_get_render_info(RenderingEntity p_viewport, RS::ViewportRenderInfo p_info);
    virtual void viewport_set_debug_draw(RenderingEntity p_viewport, RS::ViewportDebugDraw p_draw);

    void set_default_clear_color(const Color &p_color);
    void draw_viewports();

    VisualServerViewport() = default;
    virtual ~VisualServerViewport() = default;
};

struct RenderingViewportCanvasComponent {
    struct CanvasData {

        MoveOnlyEntityHandle canvas; //RenderingCanvasComponent *
        Transform2D transform;
        int layer;
        int sublayer;
    };
    HashMap<RenderingEntity , CanvasData> canvas_map {};
    MoveOnlyEntityHandle self;

    void unregister_from_canvas();

    RenderingViewportCanvasComponent(const RenderingViewportComponent &) = delete;
    RenderingViewportCanvasComponent &operator=(const RenderingViewportCanvasComponent &) = delete;

    RenderingViewportCanvasComponent(RenderingViewportCanvasComponent &&f) = default;
    RenderingViewportCanvasComponent &operator=(RenderingViewportCanvasComponent &&f) {
        unregister_from_canvas();
        canvas_map = eastl::move(f.canvas_map);
        self = eastl::move(f.self);
        return *this;
    }

    ~RenderingViewportCanvasComponent();
    RenderingViewportCanvasComponent() = default;
};

struct RenderingViewportComponent {

    Transform2D global_transform;
    MoveOnlyEntityHandle self;
    MoveOnlyEntityHandle parent;
    MoveOnlyEntityHandle camera;
    MoveOnlyEntityHandle scenario;
    MoveOnlyEntityHandle render_target;
    //MoveOnlyEntityHandle render_target_texture;
    MoveOnlyEntityHandle shadow_atlas;
    Size2i size;
    Rect2 viewport_to_screen_rect;

    eastl::array<int,RS::VIEWPORT_RENDER_INFO_MAX> render_info;
    int viewport_to_screen = 0;
    int shadow_atlas_size = 0;
    RS::ViewportUpdateMode update_mode = RS::VIEWPORT_UPDATE_WHEN_VISIBLE;
    RS::ViewportDebugDraw debug_draw = RS::VIEWPORT_DEBUG_DRAW_DISABLED;
    RS::ViewportClearMode clear_mode = RS::VIEWPORT_CLEAR_ALWAYS;

    bool hide_scenario : 1;
    bool hide_canvas : 1;
    bool disable_environment : 1;
    bool disable_3d : 1;
    bool disable_3d_by_usage : 1;
    bool keep_3d_linear : 1;
    bool use_arvr : 1; /* use arvr interface to override camera positioning and projection matrices and control output */

    bool transparent_bg : 1;

    RenderingViewportComponent() :
        disable_environment(false),
        disable_3d(false),
        disable_3d_by_usage(false),
        keep_3d_linear(false),
        use_arvr(false),
        transparent_bg(false) {
        render_info.fill(0);
    }
    void unregister_from_active_viewports();

    RenderingViewportComponent(const RenderingViewportComponent &) = delete;
    RenderingViewportComponent &operator=(const RenderingViewportComponent &) = delete;

    RenderingViewportComponent(RenderingViewportComponent &&f)=default;
    RenderingViewportComponent &operator=(RenderingViewportComponent &&f);

    ~RenderingViewportComponent();
};

