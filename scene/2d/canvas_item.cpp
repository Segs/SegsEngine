/*************************************************************************/
/*  canvas_item.cpp                                                      */
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

#include "canvas_item.h"

#include "core/message_queue.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/input.h"
#include "core/version.h"
#include "core/os/mutex.h"
#include "core/script_language.h"
#include "core/object_tooling.h"
#include "editor/animation_track_editor.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/font.h"
#include "scene/resources/mesh.h"
#include "scene/resources/multimesh.h"
#include "scene/resources/shader.h"
#include "scene/resources/style_box.h"
#include "scene/resources/texture.h"
#include "scene/scene_string_names.h"
#include "servers/rendering/rendering_server_raster.h"
#include "servers/rendering_server.h"

#include <QDebug>

IMPL_GDCLASS(CanvasItem)


struct CanvasItemPendingUpdateComponent {};


///////////////////////////////////////////////////////////////////

#ifdef TOOLS_ENABLED
bool CanvasItem::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {
    if (_edit_use_rect()) {
        return _edit_get_rect().has_point(p_point);
    }
    return p_point.length() < p_tolerance;
}
Transform2D CanvasItem::_edit_get_transform() const {
    return Transform2D(_edit_get_rotation(), _edit_get_position() + _edit_get_pivot());
}
Dictionary CanvasItem::_edit_get_state() const {
    return Dictionary();
}
#endif
bool CanvasItem::is_visible_in_tree() const {

    if (!is_inside_tree()) {
        return false;
    }

    const CanvasItem *p = this;

    while (p) {
        if (!p->visible) {
            return false;
        }
        p = p->get_parent_item();
    }

    if (canvas_layer) {
        return canvas_layer->is_visible();
    }
    return true;
}

void CanvasItem::_toplevel_visibility_changed(bool p_visible) {
    RenderingServer::get_singleton()->canvas_item_set_visible(canvas_item, visible && p_visible);

    if (visible) {
        _propagate_visibility_changed(p_visible);
    } else {
        notification(NOTIFICATION_VISIBILITY_CHANGED);
    }
}
void CanvasItem::_propagate_visibility_changed(bool p_visible) {

    if (p_visible && first_draw) { //avoid propagating it twice
        first_draw = false;
    }
    notification(NOTIFICATION_VISIBILITY_CHANGED);

    if (p_visible) {
        update(); // Todo optimize.
    } else {
        emit_signal(SceneStringNames::hide);
    }
    _block();

    for (int i = 0; i < get_child_count(); i++) {

        CanvasItem *c = object_cast<CanvasItem>(get_child(i));

        if (c && c->visible && !c->toplevel) {
            c->_propagate_visibility_changed(p_visible);
        }
    }

    _unblock();
}


void CanvasItem::_update_callback() {
    if (!is_inside_tree()) {
        return;
    }

    RenderingServer::get_singleton()->canvas_item_clear(get_canvas_item());
    // todo updating = true - only allow drawing here
    if (is_visible_in_tree()) { // Todo optimize this!!
        if (first_draw) {
            notification(NOTIFICATION_VISIBILITY_CHANGED);
            first_draw = false;
        }
        drawing = true;
        notification(NOTIFICATION_DRAW);
        emit_signal(SceneStringNames::draw);
        if (get_script_instance()) {
            get_script_instance()->call(SceneStringNames::_draw);
        }
        drawing = false;
    }
}

Transform2D CanvasItem::get_global_transform_with_canvas() const {

    if (canvas_layer) {
        return canvas_layer->get_transform() * get_global_transform();
    }
    if (is_inside_tree()) {
        return get_viewport()->get_canvas_transform() * get_global_transform();
    }

    return get_global_transform();
}

Transform2D CanvasItem::get_global_transform() const {
#ifdef DEBUG_ENABLED
    ERR_FAIL_COND_V(!is_inside_tree(), get_transform());
#endif
    if (global_invalid) {

        const CanvasItem *pi = get_parent_item();
        if (pi) {
            global_transform = pi->get_global_transform() * get_transform();
        } else {
            global_transform = get_transform();
        }

        global_invalid = false;
    }

    return global_transform;
}

void CanvasItem::_toplevel_raise_self() {

    if (!is_inside_tree()) {
        return;
    }

    int idx;
    if (canvas_layer) {
        idx = canvas_layer->get_sort_index();
    } else {
        idx = get_viewport()->gui_get_canvas_sort_index();
    }

    RenderingServer::get_singleton()->canvas_item_set_draw_index(canvas_item, idx);
}

void CanvasItem::_enter_canvas() {
    auto *rs = RenderingServer::get_singleton();

    if ((!object_cast<CanvasItem>(get_parent())) || toplevel) {

        Node *n = this;

        canvas_layer = nullptr;

        while (n) {

            canvas_layer = object_cast<CanvasLayer>(n);
            if (canvas_layer) {
                break;
            }
            if (object_cast<Viewport>(n)) {
                break;
            }
            n = n->get_parent();
        }

        RenderingEntity canvas;
        if (canvas_layer) {
            canvas = canvas_layer->get_canvas();
        } else {
            canvas = get_viewport()->find_world_2d()->get_canvas();
        }

        rs->canvas_item_set_parent(canvas_item, canvas);
        snprintf(group,31,"root_canvas%d",entt::to_integral(canvas));
        group[31]=0;

        StringName gname {StringView(group)};
        add_to_group(gname);
        if (canvas_layer) {
            canvas_layer->reset_sort_index();
        } else {
            get_viewport()->gui_reset_canvas_sort_index();
        }

        get_tree()->call_group_flags(SceneTree::GROUP_CALL_UNIQUE, gname, "_toplevel_raise_self");

    } else {

        CanvasItem *parent = get_parent_item();
        canvas_layer = parent->canvas_layer;
        rs->canvas_item_set_parent(canvas_item, parent->get_canvas_item());
        rs->canvas_item_set_draw_index(canvas_item, get_index());
    }

    update();

    notification(NOTIFICATION_ENTER_CANVAS);
}

void CanvasItem::_exit_canvas() {

    notification(NOTIFICATION_EXIT_CANVAS, true); //reverse the notification
    RenderingServer::get_singleton()->canvas_item_set_parent(canvas_item, entt::null);
    canvas_layer = nullptr;
    group[0]=0;
}

void CanvasItem::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {

            ERR_FAIL_COND(!is_inside_tree());
            first_draw = true;
            Node *parent = get_parent();
            if (parent) {
                CanvasItem *ci = object_cast<CanvasItem>(parent);
                if (ci) {
                    ci->children_items.push_back(this);
                    C = this;
                }
            }
            _enter_canvas();
            if (!block_transform_notify) {
                mark_dirty_xform(get_instance_id());
            }
        } break;
        case NOTIFICATION_MOVED_IN_PARENT: {

            if (!is_inside_tree()) {
                break;
            }

            if (group[0]!=0) {
                get_tree()->call_group_flags(SceneTree::GROUP_CALL_UNIQUE, StringName(group), "_toplevel_raise_self");
            } else {
                CanvasItem *p = get_parent_item();
                ERR_FAIL_COND(!p);
                RenderingServer::get_singleton()->canvas_item_set_draw_index(canvas_item, get_index());
            }

        } break;
        case NOTIFICATION_EXIT_TREE: {
            mark_clean_xform(get_instance_id());
            _exit_canvas();
            if (C!=nullptr) {
                object_cast<CanvasItem>(get_parent())->children_items.erase_first(C);
                C = {};
            }
            global_invalid = true;
        } break;
        case NOTIFICATION_DRAW:
        case NOTIFICATION_TRANSFORM_CHANGED: {

        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {

            emit_signal(SceneStringNames::visibility_changed);
        } break;
    }
}

void CanvasItem::set_visible(bool p_visible) {

    if (visible==p_visible) {
        return;
    }

    visible = p_visible;
    RenderingServer::get_singleton()->canvas_item_set_visible(canvas_item, p_visible);

    if (!is_inside_tree()) {
        return;
    }

    _propagate_visibility_changed(p_visible);
    Object_change_notify(this,"visible");
}


bool update_all_pending_canvas_items() {
    // Must be in tree and marked for update.
    auto view(game_object_registry.registry.view<InTreeMarkerComponent,
              CanvasItemPendingUpdateComponent,
              ObjectLink>());

    view.each([](GameEntity ent,ObjectLink &lnk) {
        assert(object_cast<CanvasItem>(lnk.object)!=nullptr);
        game_object_registry.registry.remove<CanvasItemPendingUpdateComponent>(ent);
        reinterpret_cast<CanvasItem *>(lnk.object)->_update_callback();
    });

    return game_object_registry.registry.storage<CanvasItemPendingUpdateComponent>().empty();
}


void CanvasItem::update() {

    if (!is_inside_tree() ) {
        return;
    }

    game_object_registry.registry.emplace_or_replace<CanvasItemPendingUpdateComponent>(get_instance_id());

    //MessageQueue::get_singleton()->push_call(get_instance_id(), [this]() {this->_update_callback();});
}

void CanvasItem::set_modulate(const Color &p_modulate) {

    if (modulate == p_modulate) {
        return;
    }

    modulate = p_modulate;
    RenderingServer::get_singleton()->canvas_item_set_modulate(canvas_item, modulate);
}

void CanvasItem::set_as_top_level(bool p_toplevel) {

    if (toplevel == p_toplevel) {
        return;
    }

    if (!is_inside_tree()) {
        toplevel = p_toplevel;
        return;
    }

    _exit_canvas();
    toplevel = p_toplevel;
    _enter_canvas();

    _notify_transform();
}

CanvasItem *CanvasItem::get_parent_item() const {

    if (toplevel) {
        return nullptr;
    }

    return object_cast<CanvasItem>(get_parent());
}

void CanvasItem::set_self_modulate(const Color &p_self_modulate) {

    if (self_modulate == p_self_modulate) {
        return;
    }

    self_modulate = p_self_modulate;
    RenderingServer::get_singleton()->canvas_item_set_self_modulate(canvas_item, self_modulate);
}

void CanvasItem::set_light_mask(int p_light_mask) {

    if (light_mask == p_light_mask) {
        return;
    }
    light_mask = p_light_mask;

    RenderingServer::get_singleton()->canvas_item_set_light_mask(canvas_item, p_light_mask);
}

void CanvasItem::item_rect_changed(bool p_size_changed) {

    if (p_size_changed) {
        update();
    }
    emit_signal(SceneStringNames::item_rect_changed);
}

void CanvasItem::draw_line(const Point2 &p_from, const Point2 &p_to, const Color &p_color, float p_width, bool p_antialiased) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    RenderingServer::get_singleton()->canvas_item_add_line(canvas_item, p_from, p_to, p_color, p_width, p_antialiased);
}

void CanvasItem::draw_polyline(Span<const Vector2> p_points, const Color &p_color, float p_width, bool p_antialiased) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    Vector<Color> colors;
    colors.push_back(p_color);
    RenderingServer::get_singleton()->canvas_item_add_polyline(canvas_item, p_points, colors, p_width, p_antialiased);
}

void CanvasItem::draw_polyline_colors(Span<const Vector2> p_points, const Vector<Color> &p_colors, float p_width, bool p_antialiased) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    RenderingServer::get_singleton()->canvas_item_add_polyline(canvas_item, p_points, p_colors, p_width, p_antialiased);
}

void CanvasItem::draw_arc(const Vector2 &p_center, float p_radius, float p_start_angle, float p_end_angle, int p_point_count, const Color &p_color, float p_width, bool p_antialiased) {

    Vector<Vector2> points;
    points.reserve(p_point_count);
    const float delta_angle = p_end_angle - p_start_angle;
    for (int i = 0; i < p_point_count; i++) {
        float theta = (i / (p_point_count - 1.0f)) * delta_angle + p_start_angle;
        points.emplace_back(p_center + Vector2(Math::cos(theta), Math::sin(theta)) * p_radius);
    }

    draw_polyline(points, p_color, p_width, p_antialiased);
}

void CanvasItem::draw_multiline(Span<const Vector2> p_points, const Color &p_color, float p_width, bool p_antialiased) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    Color colors[1] = {p_color};
    RenderingServer::get_singleton()->canvas_item_add_multiline(canvas_item, p_points, colors, p_width, p_antialiased);
}

void CanvasItem::draw_multiline_colors(Span<const Vector2> p_points, const Vector<Color> &p_colors, float p_width, bool p_antialiased) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    RenderingServer::get_singleton()->canvas_item_add_multiline(canvas_item, p_points, p_colors, p_width, p_antialiased);
}
void CanvasItem::draw_rect_stroke(const Rect2 &p_rect, const Color &p_color, float p_width, bool p_antialiased) {
    if (p_rect == Rect2()) {
        return;
    }

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

        // Thick lines are offset depending on their width to avoid partial overlapping.
        // Thin lines don't require an offset, so don't apply one in this case
        float offset;
        if (p_width >= 2) {
            offset = p_width / 2.0f;
        } else {
            offset = 0.0;
        }

        RenderingServer::get_singleton()->canvas_item_add_line(
                canvas_item,
                p_rect.position + Size2(-offset, 0),
                p_rect.position + Size2(p_rect.size.width + offset, 0),
                p_color,
                p_width,
                p_antialiased);
        RenderingServer::get_singleton()->canvas_item_add_line(
                canvas_item,
                p_rect.position + Size2(p_rect.size.width, offset),
                p_rect.position + Size2(p_rect.size.width, p_rect.size.height - offset),
                p_color,
                p_width,
                p_antialiased);
        RenderingServer::get_singleton()->canvas_item_add_line(
                canvas_item,
                p_rect.position + Size2(p_rect.size.width + offset, p_rect.size.height),
                p_rect.position + Size2(-offset, p_rect.size.height),
                p_color,
                p_width,
                p_antialiased);
        RenderingServer::get_singleton()->canvas_item_add_line(
                canvas_item,
                p_rect.position + Size2(0, p_rect.size.height - offset),
                p_rect.position + Size2(0, offset),
                p_color,
                p_width,
                p_antialiased);
    }
void CanvasItem::draw_rect_filled(const Rect2 &p_rect, const Color &p_color) {
    if(p_rect==Rect2())
        return;
    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    RenderingServer::get_singleton()->canvas_item_add_rect(canvas_item, p_rect, p_color);
}

void CanvasItem::draw_circle(const Point2 &p_pos, float p_radius, const Color &p_color) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    RenderingServer::get_singleton()->canvas_item_add_circle(canvas_item, p_pos, p_radius, p_color);
}

void CanvasItem::draw_texture(const Ref<Texture> &p_texture, const Point2 &p_pos, const Color &p_modulate) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    ERR_FAIL_COND(not p_texture);

    p_texture->draw(canvas_item, p_pos, p_modulate, false, Ref<Texture>());
}

void CanvasItem::draw_texture_with_normalmap(
        const Ref<Texture> &p_texture, const Ref<Texture> &p_normal_map, const Point2 &p_pos, const Color &p_modulate) {
    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    ERR_FAIL_COND(not p_texture);

    p_texture->draw(canvas_item, p_pos, p_modulate, false, p_normal_map);
}

void CanvasItem::draw_texture_rect(const Ref<Texture> &p_texture, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    ERR_FAIL_COND(not p_texture);
    p_texture->draw_rect(canvas_item, p_rect, p_tile, p_modulate, p_transpose);
}

void CanvasItem::draw_texture_rect_with_normalmap(const Ref<Texture> &p_texture, const Ref<Texture> &p_normal_map, const Rect2 &p_rect, bool p_tile,
        const Color &p_modulate, bool p_transpose) {
    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    ERR_FAIL_COND(not p_texture);
    p_texture->draw_rect(canvas_item, p_rect, p_tile, p_modulate, p_transpose, p_normal_map);
}

void CanvasItem::draw_texture_rect_region(const Ref<Texture> &p_texture, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, bool p_clip_uv) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
    ERR_FAIL_COND(not p_texture);
    p_texture->draw_rect_region(canvas_item, p_rect, p_src_rect, p_modulate, p_transpose, Ref<Texture>(), p_clip_uv);
}

void CanvasItem::draw_texture_with_normalmap_rect_region(const Ref<Texture> &p_texture, const Ref<Texture> &p_normal_map, const Rect2 &p_rect,
        const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, bool p_clip_uv) {
    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
    ERR_FAIL_COND(not p_texture);
    p_texture->draw_rect_region(canvas_item, p_rect, p_src_rect, p_modulate, p_transpose, p_normal_map, p_clip_uv);
}

void CanvasItem::draw_style_box(const Ref<StyleBox> &p_style_box, const Rect2 &p_rect) {
    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    ERR_FAIL_COND(not p_style_box);

    p_style_box->draw(canvas_item, p_rect);
}
void CanvasItem::draw_primitive(Span<const Vector2> p_points, Span<const Color> p_colors, const PoolVector<Point2> &p_uvs) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    RenderingServer::get_singleton()->canvas_item_add_primitive(canvas_item, p_points, p_colors, p_uvs, entt::null, 1.0f, entt::null);
}

void CanvasItem::draw_textured_primitive(Span<const Vector2> p_points, Span<const Color> p_colors, const PoolVector<Point2> &p_uvs,
        const Ref<Texture> &p_texture, float p_width, const Ref<Texture> &p_normal_map) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    RenderingEntity rid = p_texture ? p_texture->get_rid() : entt::null;
    RenderingEntity rid_normal = p_normal_map ? p_normal_map->get_rid() : entt::null;

    RenderingServer::get_singleton()->canvas_item_add_primitive(canvas_item, p_points, p_colors, p_uvs, rid, p_width, rid_normal);
}

void CanvasItem::draw_set_transform(const Point2 &p_offset, float p_rot, const Size2 &p_scale) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    Transform2D xform(p_rot, p_offset);
    xform.scale_basis(p_scale);
    RenderingServer::get_singleton()->canvas_item_add_set_transform(canvas_item, xform);
}

void CanvasItem::draw_set_transform_matrix(const Transform2D &p_matrix) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    RenderingServer::get_singleton()->canvas_item_add_set_transform(canvas_item, p_matrix);
}

void CanvasItem::draw_polygon(Span<const Point2> p_points, Span<const Color> p_colors) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    RenderingServer::get_singleton()->canvas_item_add_polygon(canvas_item, p_points, p_colors, {}, entt::null, entt::null, false);
}

void CanvasItem::draw_textured_polygon(Span<const Point2> p_points, Span<const Color> p_colors, Span<const Point2> p_uvs, Ref<Texture> p_texture, const Ref<Texture> &p_normal_map, bool p_antialiased) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    RenderingEntity rid = p_texture ? p_texture->get_rid() : entt::null;
    RenderingEntity rid_normal = p_normal_map ? p_normal_map->get_rid() : entt::null;

    RenderingServer::get_singleton()->canvas_item_add_polygon(canvas_item, p_points, p_colors, p_uvs, rid, rid_normal, p_antialiased);
}


void CanvasItem::draw_colored_polygon(Span<const Point2> p_points, const Color &p_color) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    Color colors[1]={p_color};
    RenderingEntity rid = entt::null;
    RenderingEntity rid_normal = entt::null;

    RenderingServer::get_singleton()->canvas_item_add_polygon(canvas_item, p_points, colors, {}, rid, rid_normal, false);
}

void CanvasItem::draw_colored_textured_polygon(Span<const Point2> p_points, const Color &p_color, Span<const Point2> p_uvs, Ref<Texture> p_texture,
        const Ref<Texture> &p_normal_map, bool p_antialiased) {
    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    Color colors[1]={p_color};
    RenderingEntity rid = p_texture ? p_texture->get_rid() : entt::null;
    RenderingEntity rid_normal = p_normal_map ? p_normal_map->get_rid() : entt::null;

    RenderingServer::get_singleton()->canvas_item_add_polygon(canvas_item, p_points, colors, p_uvs, rid, rid_normal, p_antialiased);

}

void CanvasItem::draw_mesh(const Ref<Mesh> &p_mesh, const Ref<Texture> &p_texture, const Ref<Texture> &p_normal_map, const Transform2D &p_transform, const Color &p_modulate) {

    ERR_FAIL_COND(not p_mesh);
    RenderingEntity texture_rid = p_texture ? p_texture->get_rid() : entt::null;
    RenderingEntity normal_map_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;

    RenderingServer::get_singleton()->canvas_item_add_mesh(canvas_item, p_mesh->get_rid(), p_transform, p_modulate, texture_rid, normal_map_rid);
}
void CanvasItem::draw_multimesh(const Ref<MultiMesh> &p_multimesh, const Ref<Texture> &p_texture, const Ref<Texture> &p_normal_map) {

    ERR_FAIL_COND(not p_multimesh);
    RenderingEntity texture_rid = p_texture ? p_texture->get_rid() : entt::null;
    RenderingEntity normal_map_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_multimesh(canvas_item, p_multimesh->get_rid(), texture_rid, normal_map_rid);
}

void CanvasItem::draw_ui_string(const Ref<Font> &p_font, const Point2 &p_pos, const UIString &p_text, const Color &p_modulate, int p_clip_w) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    ERR_FAIL_COND(not p_font);
    p_font->draw_ui_string(canvas_item, p_pos, p_text, p_modulate, p_clip_w);
}
void CanvasItem::draw_string(const Ref<Font> &p_font, const Point2 &p_pos, StringView p_text, const Color &p_modulate, int p_clip_w) {

    ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");

    ERR_FAIL_COND(not p_font);
    p_font->draw_ui_string(canvas_item, p_pos, StringUtils::from_utf8(p_text), p_modulate, p_clip_w);
}

float CanvasItem::draw_char(const Ref<Font> &p_font, const Point2 &p_pos, QChar p_char, QChar p_next, const Color &p_modulate) {

    ERR_FAIL_COND_V_MSG(!drawing, 0, "Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
    ERR_FAIL_COND_V(not p_font, 0);

    if (p_font->has_outline()) {
        p_font->draw_char(canvas_item, p_pos, p_char, p_next, Color(1, 1, 1), true);
    }
    return p_font->draw_char(canvas_item, p_pos, p_char, p_next, p_modulate);
}

void CanvasItem::_notify_transform(CanvasItem *p_node) {

    /* This check exists to avoid re-propagating the transform
     * notification down the tree on dirty nodes. It provides
     * optimization by avoiding redundancy (nodes are dirty, will get the
     * notification anyway).
     */

    if (/*p_node->xform_change.in_list() &&*/ p_node->global_invalid) {
        return; //nothing to do
    }

    p_node->global_invalid = true;

    if (p_node->notify_transform && !is_dirty_xfrom(p_node->get_instance_id())) {
        if (!p_node->block_transform_notify) {
            if (p_node->is_inside_tree()) {
                mark_dirty_xform(p_node->get_instance_id());
            }
        }
    }

    for (CanvasItem *ci : p_node->children_items) {

        if (ci->toplevel)
            continue;
        _notify_transform(ci);
    }
}

Rect2 CanvasItem::get_viewport_rect() const {

    ERR_FAIL_COND_V(!is_inside_tree(), Rect2());
    return get_viewport()->get_visible_rect();
}

RenderingEntity CanvasItem::get_canvas() const {

    ERR_FAIL_COND_V(!is_inside_tree(), entt::null);

    if (canvas_layer)
        return canvas_layer->get_canvas();
    else
        return get_viewport()->find_world_2d()->get_canvas();
}

GameEntity CanvasItem::get_canvas_layer_instance_id() const {

    if (canvas_layer) {
        return canvas_layer->get_instance_id();
    } else {
        return entt::null;
    }
}

CanvasItem *CanvasItem::get_toplevel() const {

    CanvasItem *ci = const_cast<CanvasItem *>(this);
    while (!ci->toplevel && object_cast<CanvasItem>(ci->get_parent())) {
        ci = object_cast<CanvasItem>(ci->get_parent());
    }

    return ci;
}

Ref<World2D> CanvasItem::get_world_2d() const {

    ERR_FAIL_COND_V(!is_inside_tree(), Ref<World2D>());

    CanvasItem *tl = get_toplevel();

    if (tl->get_viewport()) {
        return tl->get_viewport()->find_world_2d();
    }
    return Ref<World2D>();
}

RenderingEntity CanvasItem::get_viewport_rid() const {

    ERR_FAIL_COND_V(!is_inside_tree(), entt::null);
    return get_viewport()->get_viewport_rid();
}

void CanvasItem::set_block_transform_notify(bool p_enable) {
    block_transform_notify = p_enable;
}

void CanvasItem::set_draw_behind_parent(bool p_enable) {

    if (behind == p_enable) {
        return;
    }
    behind = p_enable;
    RenderingServer::get_singleton()->canvas_item_set_draw_behind_parent(canvas_item, behind);
}

void CanvasItem::set_material(const Ref<Material> &p_material) {

    material = p_material;
    RenderingEntity rid=entt::null;
    if (material) {
        rid = material->get_rid();
    }
    RenderingServer::get_singleton()->canvas_item_set_material(canvas_item, rid);
    Object_change_notify(this); //properties for material exposed
}

void CanvasItem::set_use_parent_material(bool p_use_parent_material) {

    use_parent_material = p_use_parent_material;
    RenderingServer::get_singleton()->canvas_item_set_use_parent_material(canvas_item, p_use_parent_material);
}



Ref<Material> CanvasItem::get_material() const {

    return material;
}

Vector2 CanvasItem::make_canvas_position_local(const Vector2 &screen_point) const {

    ERR_FAIL_COND_V(!is_inside_tree(), screen_point);

    Transform2D local_matrix = (get_canvas_transform() * get_global_transform()).affine_inverse();

    return local_matrix.xform(screen_point);
}

Ref<InputEvent> CanvasItem::make_input_local(const Ref<InputEvent> &p_event) const {

    ERR_FAIL_COND_V(not p_event, p_event);
    ERR_FAIL_COND_V(!is_inside_tree(), p_event);

    return p_event->xformed_by((get_canvas_transform() * get_global_transform()).affine_inverse());
}

Vector2 CanvasItem::get_global_mouse_position() const {

    ERR_FAIL_COND_V(!get_viewport(), Vector2());
    return get_canvas_transform().affine_inverse().xform(get_viewport()->get_mouse_position());
}

Vector2 CanvasItem::get_local_mouse_position() const {

    ERR_FAIL_COND_V(!get_viewport(), Vector2());

    return get_global_transform().affine_inverse().xform(get_global_mouse_position());
}

void CanvasItem::force_update_transform() {
    ERR_FAIL_COND(!is_inside_tree());
    if (!is_dirty_xfrom(get_instance_id())) {
        return;
    }

    mark_clean_xform(get_instance_id());

    notification(NOTIFICATION_TRANSFORM_CHANGED);
}

void CanvasItem::_bind_methods() {

    SE_BIND_METHOD(CanvasItem,_toplevel_raise_self);
#ifdef TOOLS_ENABLED
    MethodBinder::bind_method(D_METHOD("_edit_set_state", {"state"}), &CanvasItem::_edit_set_state,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_get_state"), &CanvasItem::_edit_get_state,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_set_position", {"position"}), &CanvasItem::_edit_set_position,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_get_position"), &CanvasItem::_edit_get_position,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_set_scale", {"scale"}), &CanvasItem::_edit_set_scale,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_get_scale"), &CanvasItem::_edit_get_scale,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_set_rect", {"rect"}), &CanvasItem::_edit_set_rect,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_get_rect"), &CanvasItem::_edit_get_rect,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_use_rect"), &CanvasItem::_edit_use_rect,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_set_rotation", {"degrees"}), &CanvasItem::_edit_set_rotation,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_get_rotation"), &CanvasItem::_edit_get_rotation,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_use_rotation"), &CanvasItem::_edit_use_rotation,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_set_pivot", {"pivot"}), &CanvasItem::_edit_set_pivot,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_get_pivot"), &CanvasItem::_edit_get_pivot,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_use_pivot"), &CanvasItem::_edit_use_pivot,METHOD_FLAG_EDITOR_ONLY);
    MethodBinder::bind_method(D_METHOD("_edit_get_transform"), &CanvasItem::_edit_get_transform,METHOD_FLAG_EDITOR_ONLY);
#endif

    SE_BIND_METHOD(CanvasItem,get_canvas_item);

    SE_BIND_METHOD(CanvasItem,set_visible);
    SE_BIND_METHOD(CanvasItem,is_visible);
    SE_BIND_METHOD(CanvasItem,is_visible_in_tree);
    SE_BIND_METHOD(CanvasItem,show);
    SE_BIND_METHOD(CanvasItem,hide);

    SE_BIND_METHOD(CanvasItem,update);

    SE_BIND_METHOD(CanvasItem,set_as_top_level);
    SE_BIND_METHOD(CanvasItem,is_set_as_top_level);

    SE_BIND_METHOD(CanvasItem,set_light_mask);
    SE_BIND_METHOD(CanvasItem,get_light_mask);

    SE_BIND_METHOD(CanvasItem,set_modulate);
    SE_BIND_METHOD(CanvasItem,get_modulate);
    SE_BIND_METHOD(CanvasItem,set_self_modulate);
    SE_BIND_METHOD(CanvasItem,get_self_modulate);

    SE_BIND_METHOD(CanvasItem,set_draw_behind_parent);
    SE_BIND_METHOD(CanvasItem,is_draw_behind_parent_enabled);

    SE_BIND_METHOD(CanvasItem,_set_on_top);
    SE_BIND_METHOD(CanvasItem,_is_on_top);

    MethodBinder::bind_method(D_METHOD("draw_line", {"from", "to", "color", "width", "antialiased"}), &CanvasItem::draw_line, {DEFVAL(1.0), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("draw_polyline", {"points", "color", "width", "antialiased"}), &CanvasItem::draw_polyline, {DEFVAL(1.0), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("draw_polyline_colors", {"points", "colors", "width", "antialiased"}), &CanvasItem::draw_polyline_colors, {DEFVAL(1.0), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("draw_arc", {"center", "radius", "start_angle", "end_angle", "point_count", "color", "width", "antialiased"}), &CanvasItem::draw_arc, {DEFVAL(1.0), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("draw_multiline", {"points", "color", "width", "antialiased"}), &CanvasItem::draw_multiline, {DEFVAL(1.0), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("draw_multiline_colors", {"points", "colors", "width", "antialiased"}), &CanvasItem::draw_multiline_colors, {DEFVAL(1.0), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("draw_rect_stroke", {"rect", "color", "width", "antialiased"}), &CanvasItem::draw_rect_stroke, {DEFVAL(1.0f), DEFVAL(false)});
    SE_BIND_METHOD(CanvasItem,draw_rect_filled);
    SE_BIND_METHOD(CanvasItem,draw_circle);
    MethodBinder::bind_method(D_METHOD("draw_texture", {"texture", "position", "modulate"}), &CanvasItem::draw_texture, {DEFVAL(Color(1, 1, 1, 1))});
    MethodBinder::bind_method(D_METHOD("draw_texture_rect", {"texture", "rect", "tile", "modulate", "transpose"}), &CanvasItem::draw_texture_rect, {DEFVAL(Color(1, 1, 1)), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("draw_texture_rect_region", {"texture", "rect", "src_rect", "modulate", "transpose", "clip_uv"}), &CanvasItem::draw_texture_rect_region, {DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(true)});
    SE_BIND_METHOD(CanvasItem,draw_style_box);
    SE_BIND_METHOD(CanvasItem,draw_primitive);
    SE_BIND_METHOD(CanvasItem,draw_textured_primitive);
    SE_BIND_METHOD(CanvasItem,draw_polygon);
    SE_BIND_METHOD(CanvasItem,draw_textured_polygon);
    SE_BIND_METHOD(CanvasItem,draw_colored_polygon);
    SE_BIND_METHOD(CanvasItem,draw_colored_textured_polygon);
    MethodBinder::bind_method(D_METHOD("draw_string", {"font", "position", "text", "modulate", "clip_w"}), &CanvasItem::draw_string, {DEFVAL(Color(1, 1, 1)), DEFVAL(-1)});
    MethodBinder::bind_method(D_METHOD("draw_char", {"font", "position", "char", "next", "modulate"}), &CanvasItem::draw_char, {DEFVAL(Color(1, 1, 1))});
    MethodBinder::bind_method(D_METHOD("draw_mesh", {"mesh", "texture", "normal_map", "transform", "modulate"}), &CanvasItem::draw_mesh, {DEFVAL(Ref<Texture>()), DEFVAL(Transform2D()), DEFVAL(Color(1, 1, 1))});
    MethodBinder::bind_method(D_METHOD("draw_multimesh", {"multimesh", "texture", "normal_map"}), &CanvasItem::draw_multimesh, {DEFVAL(Ref<Texture>())});

    SE_BIND_METHOD(CanvasItem,draw_set_transform);
    SE_BIND_METHOD(CanvasItem,draw_set_transform_matrix);
    SE_BIND_METHOD(CanvasItem,get_transform);
    SE_BIND_METHOD(CanvasItem,get_global_transform);
    SE_BIND_METHOD(CanvasItem,get_global_transform_with_canvas);
    SE_BIND_METHOD(CanvasItem,get_viewport_transform);
    SE_BIND_METHOD(CanvasItem,get_viewport_rect);
    SE_BIND_METHOD(CanvasItem,get_canvas_transform);
    SE_BIND_METHOD(CanvasItem,get_local_mouse_position);
    SE_BIND_METHOD(CanvasItem,get_global_mouse_position);
    SE_BIND_METHOD(CanvasItem,get_canvas);
    SE_BIND_METHOD(CanvasItem,get_world_2d);

    SE_BIND_METHOD(CanvasItem,set_material);
    SE_BIND_METHOD(CanvasItem,get_material);

    SE_BIND_METHOD(CanvasItem,set_use_parent_material);
    SE_BIND_METHOD(CanvasItem,get_use_parent_material);

    SE_BIND_METHOD(CanvasItem,set_notify_local_transform);
    SE_BIND_METHOD(CanvasItem,is_local_transform_notification_enabled);

    SE_BIND_METHOD(CanvasItem,set_notify_transform);
    SE_BIND_METHOD(CanvasItem,is_transform_notification_enabled);

    SE_BIND_METHOD(CanvasItem,force_update_transform);

    SE_BIND_METHOD(CanvasItem,make_canvas_position_local);
    SE_BIND_METHOD(CanvasItem,make_input_local);

    BIND_VMETHOD(MethodInfo("_draw"));

    ADD_GROUP("Visibility", "vis_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "vis_visible"), "set_visible", "is_visible");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "vis_modulate"), "set_modulate", "get_modulate");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "vis_self_modulate"), "set_self_modulate", "get_self_modulate");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "vis_show_behind_parent"), "set_draw_behind_parent", "is_draw_behind_parent_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "vis_show_on_top", PropertyHint::None, "", 0), "_set_on_top", "_is_on_top"); //compatibility
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "vis_light_mask", PropertyHint::Layers2DRenderer), "set_light_mask", "get_light_mask");

    ADD_GROUP("Material", "mat_");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "mat_material", PropertyHint::ResourceType, "ShaderMaterial,CanvasItemMaterial"), "set_material", "get_material");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "mat_use_parent_material"), "set_use_parent_material", "get_use_parent_material");
    //exporting these things doesn't really make much sense i think
    // ADD_PROPERTY(PropertyInfo(VariantType::BOOL,"transform/notify"),"set_transform_notify","is_transform_notify_enabled");

    ADD_SIGNAL(MethodInfo("draw"));
    ADD_SIGNAL(MethodInfo("visibility_changed"));
    ADD_SIGNAL(MethodInfo("hide"));
    ADD_SIGNAL(MethodInfo("item_rect_changed"));

    BIND_CONSTANT(NOTIFICATION_TRANSFORM_CHANGED);
    BIND_CONSTANT(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
    BIND_CONSTANT(NOTIFICATION_DRAW);
    BIND_CONSTANT(NOTIFICATION_VISIBILITY_CHANGED);
    BIND_CONSTANT(NOTIFICATION_ENTER_CANVAS);
    BIND_CONSTANT(NOTIFICATION_EXIT_CANVAS);
}

Transform2D CanvasItem::get_canvas_transform() const {

    ERR_FAIL_COND_V(!is_inside_tree(), Transform2D());

    if (canvas_layer) {
        return canvas_layer->get_transform();
    }

    if (object_cast<CanvasItem>(get_parent())) {
        return object_cast<CanvasItem>(get_parent())->get_canvas_transform();
    }

    return get_viewport()->get_canvas_transform();
}

Transform2D CanvasItem::get_viewport_transform() const {
    ERR_FAIL_COND_V(!is_inside_tree(), Transform2D());

    if (canvas_layer) {
        if (get_viewport()) {
            return get_viewport()->get_final_transform() * canvas_layer->get_transform();
        } else {
            return canvas_layer->get_transform();
        }

    } else {
        return get_viewport()->get_final_transform() * get_viewport()->get_canvas_transform();
    }
}

void CanvasItem::set_notify_local_transform(bool p_enable) {
    notify_local_transform = p_enable;
}

void CanvasItem::set_notify_transform(bool p_enable) {
    if (notify_transform == p_enable)
        return;

    notify_transform = p_enable;

    if (notify_transform && is_inside_tree()) {
        //this ensures that invalid globals get resolved, so notifications can be received
        (void)get_global_transform();
    }
}

int CanvasItem::get_canvas_layer() const {
    return canvas_layer ? canvas_layer->get_layer() : 0;
}

CanvasItem::CanvasItem() {
    memset(group,0,32);
    canvas_item = RenderingServer::get_singleton()->canvas_item_create();
    game_object_registry.registry.emplace<GameRenderableComponent>(get_instance_id(),canvas_item,get_instance_id());
    visible = true;
    modulate = Color(1, 1, 1, 1);
    self_modulate = Color(1, 1, 1, 1);
    toplevel = false;
    first_draw = false;
    drawing = false;
    behind = false;
    block_transform_notify = false;
    canvas_layer = nullptr;
    use_parent_material = false;
    global_invalid = true;
    notify_local_transform = false;
    notify_transform = false;
    light_mask = 1;

    C = nullptr;
}

CanvasItem::~CanvasItem() {
    RenderingServer::get_singleton()->free_rid(canvas_item);
}

