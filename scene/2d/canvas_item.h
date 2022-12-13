/*************************************************************************/
/*  canvas_item.h                                                        */
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

#include "scene/main/node.h"
#include "scene/main/scene_tree_notifications.h"
#include "core/math/transform_2d.h"
#include "core/color.h"

class CanvasLayer;
class Viewport;
class Font;
class World2D;
class StyleBox;
class Material;
class Mesh;
class MultiMesh;
class Texture;

class GODOT_EXPORT CanvasItem : public Node {

    GDCLASS(CanvasItem,Node)
//    Q_GADGET
    friend class CanvasLayer;

private:

    RenderingEntity canvas_item;
    char group[32];

    CanvasLayer *canvas_layer;

    Color modulate;
    Color self_modulate;

    Vector<CanvasItem *> children_items;
    CanvasItem * C;
    Ref<Material> material;

    mutable Transform2D global_transform;

    int light_mask;

    uint8_t first_draw : 1;
    uint8_t visible : 1;
    uint8_t toplevel : 1;
    uint8_t drawing : 1;
    uint8_t block_transform_notify : 1;
    uint8_t behind : 1;
    uint8_t use_parent_material : 1;
    uint8_t notify_local_transform : 1;
    uint8_t notify_transform : 1;
    mutable uint8_t global_invalid : 1;

public:
    /*Q_INVOKABLE*/ void _toplevel_raise_self();
    void _toplevel_visibility_changed(bool p_visible);
    /*Q_INVOKABLE*/ void _propagate_visibility_changed(bool p_visible);
    /*Q_INVOKABLE*/ void _update_callback();
private:
    void _enter_canvas();
    void _exit_canvas();

    void _notify_transform(CanvasItem *p_node);
public:
    void _set_on_top(bool p_on_top) { set_draw_behind_parent(!p_on_top); }
    bool _is_on_top() const { return !is_draw_behind_parent_enabled(); }


protected:
    void _notify_transform() {
        if (!is_inside_tree()) {
            return;
        }
        _notify_transform(this);
        if (!block_transform_notify && notify_local_transform) {
            notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
        }
    }

    void item_rect_changed(bool p_size_changed = true);

    void _notification(int p_what);
    static void _bind_methods();

public:
    enum {
        NOTIFICATION_TRANSFORM_CHANGED = SceneTreeNotifications::NOTIFICATION_TRANSFORM_CHANGED, //unique
        NOTIFICATION_DRAW = 30,
        NOTIFICATION_VISIBILITY_CHANGED = 31,
        NOTIFICATION_ENTER_CANVAS = 32,
        NOTIFICATION_EXIT_CANVAS = 33,
        NOTIFICATION_LOCAL_TRANSFORM_CHANGED = 35,
        NOTIFICATION_WORLD_2D_CHANGED = 36,

    };

    /* EDITOR */
#ifdef TOOLS_ENABLED
    // Select the node
    virtual bool _edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const;

    // Save and restore a CanvasItem state
    /*Q_INVOKABLE*/ virtual void _edit_set_state(const Dictionary &/*p_state*/){}
    virtual Dictionary _edit_get_state() const;

    // Used to move the node
    virtual void _edit_set_position(const Point2 &p_position) = 0;
    virtual Point2 _edit_get_position() const = 0;

    // Used to scale the node
    virtual void _edit_set_scale(const Size2 &p_scale) = 0;
    virtual Size2 _edit_get_scale() const = 0;

    // Used to rotate the node
    virtual bool _edit_use_rotation() const { return false; }
    virtual void _edit_set_rotation(float /*p_rotation*/){}
    virtual float _edit_get_rotation() const { return 0.0; }

    // Used to resize/move the node
    virtual bool _edit_use_rect() const { return false; } // MAYBE REPLACE BY A _edit_get_editmode()
    virtual void _edit_set_rect(const Rect2 &/*p_rect*/){}
    virtual Rect2 _edit_get_rect() const { return Rect2(0, 0, 0, 0); }
    virtual Size2 _edit_get_minimum_size() const { return Size2(-1, -1); } // LOOKS WEIRD

    // Used to set a pivot
    virtual bool _edit_use_pivot() const { return false; }
    virtual void _edit_set_pivot(const Point2 &/*p_pivot*/){}
    virtual Point2 _edit_get_pivot() const { return Point2(); }

    virtual Transform2D _edit_get_transform() const;
#else
    bool _edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const { return false; }
#endif
    /* VISIBILITY */

    void set_visible(bool p_visible);
    bool is_visible() const { return visible; }
    bool is_visible_in_tree() const;
    void show() { set_visible(true); }
    void hide() { set_visible(false); }

    void update();

    virtual void set_light_mask(int p_light_mask);
    int get_light_mask() const { return light_mask; }

    void set_modulate(const Color &p_modulate);
    Color get_modulate() const { return modulate; }

    void set_self_modulate(const Color &p_self_modulate);
    Color get_self_modulate() const { return self_modulate; }

    /* DRAWING API */

    void draw_line(const Point2 &p_from, const Point2 &p_to, const Color &p_color, float p_width = 1.0, bool p_antialiased = false);
    void draw_polyline(Span<const Vector2> p_points, const Color &p_color, float p_width = 1.0, bool p_antialiased = false);
    void draw_polyline_colors(Span<const Vector2> p_points, const Vector<Color> &p_colors, float p_width = 1.0, bool p_antialiased = false);
    void draw_arc(const Vector2 &p_center, float p_radius, float p_start_angle, float p_end_angle, int p_point_count, const Color &p_color, float p_width = 1.0, bool p_antialiased = false);
    void draw_multiline(Span<const Vector2> p_points, const Color &p_color, float p_width = 1.0, bool p_antialiased = false);
    void draw_multiline_colors(Span<const Vector2> p_points, const Vector<Color> &p_colors, float p_width = 1.0, bool p_antialiased = false);
    void draw_rect_stroke(const Rect2 &p_rect, const Color &p_color, float p_width = 1.0, bool p_antialiased = false);
    void draw_rect_filled(const Rect2 &p_rect, const Color &p_color);
    void draw_circle(const Point2 &p_pos, float p_radius, const Color &p_color);
    void draw_texture(const Ref<Texture> &p_texture, const Point2 &p_pos, const Color &p_modulate = Color(1, 1, 1, 1));
    void draw_texture_with_normalmap(
            const Ref<Texture> &p_texture, const Ref<Texture> &p_normal_map, const Point2 &p_pos, const Color &p_modulate = Color(1, 1, 1, 1)
            );
    void draw_texture_rect(const Ref<Texture> &p_texture, const Rect2 &p_rect, bool p_tile = false, const Color &p_modulate = Color(1, 1, 1),
            bool p_transpose = false);
    void draw_texture_rect_with_normalmap(const Ref<Texture> &p_texture, const Ref<Texture> &p_normal_map, const Rect2 &p_rect, bool p_tile = false,
            const Color &p_modulate = Color(1, 1, 1),
            bool p_transpose = false);

    void draw_texture_rect_region(const Ref<Texture> &p_texture, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false, bool p_clip_uv = false);
    void draw_texture_with_normalmap_rect_region(const Ref<Texture> &p_texture, const Ref<Texture> &p_normal_map, const Rect2 &p_rect,
            const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false, bool p_clip_uv = false);
    void draw_style_box(const Ref<StyleBox> &p_style_box, const Rect2 &p_rect);
    void draw_primitive(Span<const Vector2> p_points, Span<const Color> p_colors, const PoolVector<Point2> &p_uvs);
    void draw_textured_primitive(Span<const Vector2> p_points, Span<const Color> p_colors, const PoolVector<Point2> &p_uvs, const Ref<Texture> &p_texture, float p_width, const Ref<Texture> &p_normal_map);
    void draw_polygon(Span<const Point2> p_points, Span<const Color> p_colors);
    void draw_textured_polygon(Span<const Point2> p_points, Span<const Color> p_colors, Span<const Point2> p_uvs, Ref<Texture> p_texture, const Ref<Texture> &p_normal_map, bool p_antialiased);
    void draw_colored_polygon(Span<const Point2> p_points, const Color &p_color);
    void draw_colored_textured_polygon(Span<const Point2> p_points, const Color &p_color, Span<const Point2> p_uvs, Ref<Texture> p_texture,
            const Ref<Texture> &p_normal_map, bool p_antialiased);

    void draw_mesh(const Ref<Mesh> &p_mesh, const Ref<Texture> &p_texture, const Ref<Texture> &p_normal_map, const Transform2D &p_transform = Transform2D(), const Color &p_modulate = Color(1, 1, 1));
    void draw_multimesh(const Ref<MultiMesh> &p_multimesh, const Ref<Texture> &p_texture, const Ref<Texture> &p_normal_map);

    void draw_ui_string(const Ref<Font> &p_font, const Point2 &p_pos, const UIString &p_text, const Color &p_modulate = Color(1, 1, 1), int p_clip_w = -1);
    void draw_string(const Ref<Font> &p_font, const Point2 &p_pos, StringView p_text, const Color &p_modulate = Color(1, 1, 1), int p_clip_w = -1);
    float draw_char(const Ref<Font> &p_font, const Point2 &p_pos, QChar p_char, QChar p_next, const Color &p_modulate = Color(1, 1, 1));

    void draw_set_transform(const Point2 &p_offset, float p_rot, const Size2 &p_scale);
    void draw_set_transform_matrix(const Transform2D &p_matrix);


    /* RECT / TRANSFORM */

    void set_as_top_level(bool p_toplevel);
    bool is_set_as_top_level() const { return toplevel; }

    void set_draw_behind_parent(bool p_enable);
    bool is_draw_behind_parent_enabled() const { return behind; }

    CanvasItem *get_parent_item() const;

    virtual Transform2D get_transform() const = 0;

    virtual Transform2D get_global_transform() const;
    virtual Transform2D get_global_transform_with_canvas() const;

    CanvasItem *get_toplevel() const;
    RenderingEntity get_canvas_item() const { return canvas_item; }

    void set_block_transform_notify(bool p_enable);
    bool is_block_transform_notify_enabled() const { return block_transform_notify; }

    Transform2D get_canvas_transform() const;
    Transform2D get_viewport_transform() const;
    Rect2 get_viewport_rect() const;
    RenderingEntity get_viewport_rid() const;
    RenderingEntity get_canvas() const;
    GameEntity get_canvas_layer_instance_id() const;
    Ref<World2D> get_world_2d() const;

    virtual void set_material(const Ref<Material> &p_material);
    Ref<Material> get_material() const;

    virtual void set_use_parent_material(bool p_use_parent_material);
    bool get_use_parent_material() const { return use_parent_material; }

    Ref<InputEvent> make_input_local(const Ref<InputEvent> &p_event) const;
    Vector2 make_canvas_position_local(const Vector2 &screen_point) const;

    Vector2 get_global_mouse_position() const;
    Vector2 get_local_mouse_position() const;

    void set_notify_local_transform(bool p_enable);
    bool is_local_transform_notification_enabled() const { return notify_local_transform; }

    void set_notify_transform(bool p_enable);
    bool is_transform_notification_enabled() const { return notify_transform; }

    void force_update_transform();

    // Used by control nodes to retrieve the parent's anchorable area
    virtual Rect2 get_anchorable_rect() const { return Rect2(0, 0, 0, 0); }

    int get_canvas_layer() const;

    CanvasItem();
    ~CanvasItem() override;
};

bool update_all_pending_canvas_items();
