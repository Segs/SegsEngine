/*************************************************************************/
/*  rendering_server_canvas.h                                               */
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
#include "rendering_server_viewport.h"

struct RenderingCanvasItemComponent : public RasterizerCanvas::Item {

    MoveOnlyEntityHandle parent; // canvas it belongs to
    MoveOnlyEntityHandle self;
    int z_index=0;
    bool z_relative=true;
    bool sort_y=false;
    Color modulate = {1, 1, 1, 1};
    Color self_modulate = {1, 1, 1, 1};
    bool use_parent_material=false;
    int index=0;
    bool children_order_dirty = true;
    int ysort_children_count = -1;
    Color ysort_modulate;
    Transform2D ysort_xform;
    Vector2 ysort_pos;
    int ysort_index=0;

    Vector<RenderingEntity> child_items;

    void release_resources();
    RenderingCanvasItemComponent(const RenderingCanvasItemComponent &) = delete;
    RenderingCanvasItemComponent &operator=(const RenderingCanvasItemComponent &) = delete;


    RenderingCanvasItemComponent(RenderingCanvasItemComponent &&oth) noexcept { *this = eastl::move(oth); }
    RenderingCanvasItemComponent &operator=(RenderingCanvasItemComponent &&from) noexcept;

    RenderingCanvasItemComponent() {
    }
    ~RenderingCanvasItemComponent() { release_resources(); }
};

struct RenderingCanvasComponent {

    struct ChildItem {

        Point2 mirror;
        RenderingEntity item;
    };

    HashSet<RenderingEntity> viewports;
    HashSet<RenderingEntity> lights;
    HashSet<RenderingEntity> occluders;
    Vector<ChildItem> child_items;
    Color modulate=Color(1, 1, 1, 1);
    MoveOnlyEntityHandle parent;
    MoveOnlyEntityHandle self;
    float parent_scale=1.0f;
    bool children_order_dirty=true;

    int find_item(RenderingEntity p_item) {
        for (size_t i = 0; i < child_items.size(); i++) {
            if (child_items[i].item == p_item)
                return i;
        }
        return -1;
    }
    void erase_item(RenderingEntity p_item) {
        int idx = find_item(p_item);
        if (idx >= 0)
            child_items.erase_at(idx);
    }

    void release_resources();

    RenderingCanvasComponent(const RenderingCanvasComponent &) = delete;
    RenderingCanvasComponent &operator=(const RenderingCanvasComponent &) = delete;

    RenderingCanvasComponent(RenderingCanvasComponent &&) = default;
    RenderingCanvasComponent &operator=(RenderingCanvasComponent &&from);

    RenderingCanvasComponent() {
    }
    ~RenderingCanvasComponent() { release_resources(); }
};

struct LightOccluderPolygonComponent {

    bool active = false;
    Rect2 aabb;
    RS::CanvasOccluderPolygonCullMode cull_mode = RS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED;
    MoveOnlyEntityHandle occluder;
    HashSet<RenderingEntity> owners;

    void release_resources();

    LightOccluderPolygonComponent(const LightOccluderPolygonComponent &) = delete;
    LightOccluderPolygonComponent &operator=(const LightOccluderPolygonComponent &) = delete;

    LightOccluderPolygonComponent(LightOccluderPolygonComponent &&) = default;
    LightOccluderPolygonComponent &operator=(LightOccluderPolygonComponent &&from) {
        release_resources();

        active=from.active;
        aabb=from.aabb;
        cull_mode = from.cull_mode;
        occluder = eastl::move(from.occluder);
        owners = eastl::move(from.owners);
        return *this;
    }

    LightOccluderPolygonComponent() {
    }
    ~LightOccluderPolygonComponent();
};

class RenderingServerCanvas {
public:


//    HashSet<RenderingEntity> canvas_light_occluder_polygon_owner;
//    HashSet<RenderingEntity> canvas_light_occluder_owner;
//    HashSet<RenderingEntity> canvas_item_owner; //Item
//    HashSet<RenderingEntity> canvas_light_owner; //RasterizerCanvasLight3DComponent

    bool disable_scale;

private:
    void _render_canvas_item_tree(RenderingEntity p_canvas_item, const Transform2D &p_transform,
            const Rect2 &p_clip_rect, const Color &p_modulate, Span<RasterizerCanvasLight3DComponent *> p_lights);

    Vector<Dequeue<RasterizerCanvas::Item *>> z_sort_arr;

public:
    void render_canvas(RenderingCanvasComponent *p_canvas, const Transform2D &p_transform,
            Span<RasterizerCanvasLight3DComponent *> p_lights, Span<RasterizerCanvasLight3DComponent *> p_masked_lights, const Rect2 &p_clip_rect);

    RenderingEntity canvas_create();
    void canvas_set_item_mirroring(RenderingEntity p_canvas, RenderingEntity p_item, const Point2 &p_mirroring);
    void canvas_set_modulate(RenderingEntity p_canvas, const Color &p_color);
    void canvas_set_parent(RenderingEntity p_canvas, RenderingEntity p_parent, float p_scale);
    void canvas_set_disable_scale(bool p_disable);

    RenderingEntity canvas_item_create();
    void canvas_item_set_parent(RenderingEntity p_item, RenderingEntity p_parent);

    void canvas_item_set_visible(RenderingEntity p_item, bool p_visible);
    void canvas_item_set_light_mask(RenderingEntity p_item, int p_mask);

    void canvas_item_set_transform(RenderingEntity p_item, const Transform2D &p_transform);
    void canvas_item_set_clip(RenderingEntity p_item, bool p_clip);
    void canvas_item_set_distance_field_mode(RenderingEntity p_item, bool p_enable);
    void canvas_item_set_custom_rect(RenderingEntity p_item, bool p_custom_rect, const Rect2 &p_rect = Rect2());
    void canvas_item_set_modulate(RenderingEntity p_item, const Color &p_color);
    void canvas_item_set_self_modulate(RenderingEntity p_item, const Color &p_color);

    void canvas_item_set_draw_behind_parent(RenderingEntity p_item, bool p_enable);

    void canvas_item_set_update_when_visible(RenderingEntity p_item, bool p_update);

    void canvas_item_add_line(RenderingEntity p_item, const Point2 &p_from, const Point2 &p_to, const Color &p_color, float p_width = 1.0, bool p_antialiased = false);
    void canvas_item_add_polyline(RenderingEntity  p_item, Span<const Vector2> p_points, Span<const Color> p_colors, float p_width = 1.0, bool p_antialiased = false);
    void canvas_item_add_multiline(RenderingEntity  p_item, Span<const Vector2> p_points, Span<const Color> p_colors, float p_width = 1.0, bool p_antialiased = false);
    void canvas_item_add_rect(RenderingEntity p_item, const Rect2 &p_rect, const Color &p_color);
    void canvas_item_add_circle(RenderingEntity p_item, const Point2 &p_pos, float p_radius, const Color &p_color);
    void canvas_item_add_texture_rect(RenderingEntity p_item, const Rect2 &p_rect, RenderingEntity p_texture, bool p_tile = false, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false, RenderingEntity p_normal_map = entt::null);
    void canvas_item_add_texture_rect_region(RenderingEntity p_item, const Rect2 &p_rect, RenderingEntity p_texture, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false, RenderingEntity p_normal_map = entt::null, bool p_clip_uv = false);
    void canvas_item_add_nine_patch(RenderingEntity p_item, const Rect2 &p_rect, const Rect2 &p_source, RenderingEntity p_texture, const Vector2 &p_topleft, const Vector2 &p_bottomright, RS::NinePatchAxisMode p_x_axis_mode = RS::NINE_PATCH_STRETCH, RS::NinePatchAxisMode p_y_axis_mode = RS::NINE_PATCH_STRETCH, bool p_draw_center = true, const Color &p_modulate = Color(1, 1, 1), RenderingEntity p_normal_map = entt::null);
    void canvas_item_add_primitive(RenderingEntity  p_item, Span<const Point2> p_points, Span<const Color> p_colors, const PoolVector<Point2> &p_uvs, RenderingEntity p_texture, float p_width = 1.0, RenderingEntity p_normal_map = entt::null);
    void canvas_item_add_polygon(RenderingEntity p_item, Span<const Point2> p_points, Span<const Color> p_colors, Span<const Point2> p_uvs = {}, RenderingEntity p_texture = entt::null, RenderingEntity p_normal_map = entt::null, bool p_antialiased = false);
    void canvas_item_add_triangle_array(RenderingEntity p_item, Span<const int> p_indices, Span<const Point2> p_points,
            Span<const Color> p_colors, Span<const Point2> p_uvs = {}, const PoolVector<int> &p_bones = PoolVector<int>(),
            const PoolVector<float> &p_weights = PoolVector<float>(), RenderingEntity p_texture = entt::null,
            int p_count = -1, RenderingEntity p_normal_map = entt::null, bool p_antialiased = false,
            bool p_antialiasing_use_indices = false);
    void canvas_item_add_mesh(RenderingEntity  p_item, const RenderingEntity  &p_mesh, const Transform2D &p_transform = Transform2D(), const Color &p_modulate = Color(1, 1, 1), RenderingEntity  p_texture = entt::null, RenderingEntity  p_normal_map = entt::null);
    void canvas_item_add_multimesh(RenderingEntity p_item, RenderingEntity p_mesh, RenderingEntity p_texture = entt::null, RenderingEntity p_normal_map = entt::null);
    void canvas_item_add_particles(RenderingEntity p_item, RenderingEntity p_particles, RenderingEntity p_texture, RenderingEntity p_normal);
    void canvas_item_add_set_transform(RenderingEntity p_item, const Transform2D &p_transform);
    void canvas_item_add_clip_ignore(RenderingEntity p_item, bool p_ignore);
    void canvas_item_set_sort_children_by_y(RenderingEntity p_item, bool p_enable);
    void canvas_item_set_z_index(RenderingEntity p_item, int p_z);
    void canvas_item_set_z_as_relative_to_parent(RenderingEntity p_item, bool p_enable);
    void canvas_item_set_copy_to_backbuffer(RenderingEntity p_item, bool p_enable, const Rect2 &p_rect);
    void canvas_item_attach_skeleton(RenderingEntity p_item, RenderingEntity p_skeleton);

    void canvas_item_clear(RenderingEntity p_item);
    void canvas_item_set_draw_index(RenderingEntity p_item, int p_index);

    void canvas_item_set_material(RenderingEntity p_item, RenderingEntity p_material);

    void canvas_item_set_use_parent_material(RenderingEntity p_item, bool p_enable);

    RenderingEntity canvas_light_create();
    void canvas_light_attach_to_canvas(RenderingEntity p_light, RenderingEntity p_canvas);
    void canvas_light_set_enabled(RenderingEntity p_light, bool p_enabled);
    void canvas_light_set_scale(RenderingEntity p_light, float p_scale);
    void canvas_light_set_transform(RenderingEntity p_light, const Transform2D &p_transform);
    void canvas_light_set_texture(RenderingEntity p_light, RenderingEntity p_texture);
    void canvas_light_set_texture_offset(RenderingEntity p_light, const Vector2 &p_offset);
    void canvas_light_set_color(RenderingEntity p_light, const Color &p_color);
    void canvas_light_set_height(RenderingEntity p_light, float p_height);
    void canvas_light_set_energy(RenderingEntity p_light, float p_energy);
    void canvas_light_set_z_range(RenderingEntity p_light, int p_min_z, int p_max_z);
    void canvas_light_set_layer_range(RenderingEntity p_light, int p_min_layer, int p_max_layer);
    void canvas_light_set_item_cull_mask(RenderingEntity p_light, int p_mask);
    void canvas_light_set_item_shadow_cull_mask(RenderingEntity p_light, int p_mask);

    void canvas_light_set_mode(RenderingEntity p_light, RS::CanvasLightMode p_mode);

    void canvas_light_set_shadow_enabled(RenderingEntity p_light, bool p_enabled);
    void canvas_light_set_shadow_buffer_size(RenderingEntity p_light, int p_size);
    void canvas_light_set_shadow_gradient_length(RenderingEntity p_light, float p_length);
    void canvas_light_set_shadow_filter(RenderingEntity p_light, RS::CanvasLightShadowFilter p_filter);
    void canvas_light_set_shadow_color(RenderingEntity p_light, const Color &p_color);
    void canvas_light_set_shadow_smooth(RenderingEntity p_light, float p_smooth);

    RenderingEntity canvas_light_occluder_create();
    void canvas_light_occluder_attach_to_canvas(RenderingEntity p_occluder, RenderingEntity p_canvas);
    void canvas_light_occluder_set_enabled(RenderingEntity p_occluder, bool p_enabled);
    void canvas_light_occluder_set_polygon(RenderingEntity p_occluder, RenderingEntity p_polygon);
    void canvas_light_occluder_set_transform(RenderingEntity p_occluder, const Transform2D &p_xform);
    void canvas_light_occluder_set_light_mask(RenderingEntity p_occluder, int p_mask);

    RenderingEntity canvas_occluder_polygon_create();
    void canvas_occluder_polygon_set_shape(RenderingEntity p_occluder_polygon, Span<const Vector2> p_shape, bool p_closed);
    void canvas_occluder_polygon_set_shape_as_lines(RenderingEntity p_occluder_polygon, Span<const Vector2> p_shape);

    void canvas_occluder_polygon_set_cull_mode(RenderingEntity p_occluder_polygon, RS::CanvasOccluderPolygonCullMode p_mode);

    bool free(RenderingEntity p_rid);
    RenderingServerCanvas();
    ~RenderingServerCanvas();
};
