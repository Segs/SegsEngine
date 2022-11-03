/*************************************************************************/
/*  rendering_server_canvas.cpp                                             */
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

#include "rendering_server_canvas.h"
#include "rendering_server_globals.h"
#include "rendering_server_raster.h"
#include "rendering_server_viewport.h"
#include "servers/rendering/render_entity_getter.h"

#include "core/ecs_registry.h"

#include "entt/entity/helper.hpp"
#include "EASTL/sort.h"

static const int z_range = RS::CANVAS_ITEM_Z_MAX - RS::CANVAS_ITEM_Z_MIN + 1;
namespace {

struct ChildSorter {
    bool operator()(RenderingCanvasComponent::ChildItem a, RenderingCanvasComponent::ChildItem b) const {
        const auto &a_item(VSG::ecs->registry.get<RenderingCanvasItemComponent>(a.item));
        const auto &b_item(VSG::ecs->registry.get<RenderingCanvasItemComponent>(b.item));
        return a_item.index < b_item.index;
    }
};
struct ItemIndexSort {

    bool operator()(RenderingEntity p_left, RenderingEntity p_right) const {
        const auto &a_item(VSG::ecs->registry.get<RenderingCanvasItemComponent>(p_left));
        const auto &b_item(VSG::ecs->registry.get<RenderingCanvasItemComponent>(p_right));

        return a_item.index < b_item.index;
    }
};

void _mark_ysort_dirty(RenderingCanvasItemComponent *ysort_owner) {
    do {
        ysort_owner->ysort_children_count = -1;
        ysort_owner = get<RenderingCanvasItemComponent>(ysort_owner->parent);
    } while (ysort_owner && ysort_owner->sort_y);
}

static void _collect_ysort_children(RenderingCanvasItemComponent *p_canvas_item, Transform2D p_transform,
        RenderingCanvasItemComponent *p_material_owner, const Color &p_modulate, RenderingEntity *r_items,
        int &r_index) {
    auto canvas_items_view(VSG::ecs->registry.view<RenderingCanvasItemComponent>());
    int child_item_count = p_canvas_item->child_items.size();
    RenderingEntity *child_items = p_canvas_item->child_items.data();
    for (int i = 0; i < child_item_count; i++) {
        assert(canvas_items_view.contains(child_items[i]));
        auto &child = canvas_items_view.get<RenderingCanvasItemComponent>(child_items[i]);
        if (child.visible) {
            if (r_items) {
                r_items[r_index] = child_items[i];
                child.ysort_modulate = p_modulate;
                child.ysort_xform = p_transform;
                child.ysort_pos = p_transform.xform(child.xform.elements[2]);
                child.material_owner = child.use_parent_material ? p_material_owner : nullptr;
                child.ysort_index = r_index;
            }

            r_index++;

            if (child.sort_y)
                _collect_ysort_children(&child, p_transform * child.xform,
                        child.use_parent_material ? p_material_owner : &child, p_modulate * child.modulate, r_items,
                        r_index);
        }
    }
}
void _render_canvas_item(RenderingEntity p_canvas_item, const Transform2D &p_transform,
        const Rect2 &p_clip_rect, const Color &p_modulate, int p_z, Span<Dequeue<RasterizerCanvas::Item *>> z_list,
        RenderingCanvasItemComponent *p_canvas_clip,
        RenderingCanvasItemComponent *p_material_owner) {
    auto canvas_items_view(VSG::ecs->registry.view<RenderingCanvasItemComponent>());

    RenderingCanvasItemComponent *ci = &canvas_items_view.get<RenderingCanvasItemComponent>(p_canvas_item);

    if (!ci->visible) {
        return;
    }
    
    if (ci->children_order_dirty) {

        eastl::sort(ci->child_items.begin(),ci->child_items.end(),ItemIndexSort());
        ci->children_order_dirty = false;
    }

    Rect2 rect = ci->get_rect();
    Transform2D xform = ci->xform;
    xform = p_transform * xform;
    Rect2 global_rect = xform.xform(rect);
    global_rect.position += p_clip_rect.position;

    if (ci->use_parent_material && p_material_owner)
        ci->material_owner = p_material_owner;
    else {
        p_material_owner = ci;
        ci->material_owner = nullptr;
    }

    Color modulate(ci->modulate.r * p_modulate.r, ci->modulate.g * p_modulate.g, ci->modulate.b * p_modulate.b, ci->modulate.a * p_modulate.a);

    if (modulate.a < 0.007f)
        return;

    int child_item_count = ci->child_items.size();
    RenderingEntity *child_items = ci->child_items.data();

    if (ci->clip) {
        if (p_canvas_clip != nullptr) {
            ci->final_clip_rect = p_canvas_clip->final_clip_rect.clip(global_rect);
        } else {
            ci->final_clip_rect = global_rect;
        }
        ci->final_clip_rect.position = ci->final_clip_rect.position.round();
        ci->final_clip_rect.size = ci->final_clip_rect.size.round();
        ci->final_clip_owner = ci;

    } else {
        ci->final_clip_owner = p_canvas_clip;
    }

    if (ci->sort_y) {

        if (ci->ysort_children_count == -1) {
            ci->ysort_children_count = 0;
            _collect_ysort_children(ci, Transform2D(), p_material_owner, Color(1, 1, 1, 1), nullptr, ci->ysort_children_count);
        }

        child_item_count = ci->ysort_children_count;
        child_items = (RenderingEntity *)alloca(child_item_count * sizeof(RenderingEntity));

        int i = 0;
        _collect_ysort_children(ci, Transform2D(), p_material_owner, Color(1, 1, 1, 1), child_items, i);

        eastl::sort(child_items,child_items+child_item_count,[&](RenderingEntity a,RenderingEntity b)->bool {
            auto & item_a(canvas_items_view.get<RenderingCanvasItemComponent>(a));
            auto & item_b(canvas_items_view.get<RenderingCanvasItemComponent>(b));
            if (Math::is_equal_approx(item_a.ysort_pos.y, item_b.ysort_pos.y)) {
                return item_a.ysort_index < item_b.ysort_index;
            }

            return item_a.ysort_pos.y < item_b.ysort_pos.y;
        });
    }

    if (ci->z_relative)
        p_z = CLAMP<int>(p_z + ci->z_index, RS::CANVAS_ITEM_Z_MIN, RS::CANVAS_ITEM_Z_MAX);
    else
        p_z = ci->z_index;

    for (int i = 0; i < child_item_count; i++) {

        auto & child(canvas_items_view.get<RenderingCanvasItemComponent>(child_items[i]));
        if (!child.behind || (ci->sort_y && child.sort_y))
            continue;
        if (ci->sort_y) {
            _render_canvas_item(child_items[i], xform * child.ysort_xform, p_clip_rect, modulate * child.ysort_modulate, p_z, z_list, (RenderingCanvasItemComponent *)ci->final_clip_owner, (RenderingCanvasItemComponent *)child.material_owner);
        } else {
            _render_canvas_item(child_items[i], xform, p_clip_rect, modulate, p_z, z_list, (RenderingCanvasItemComponent *)ci->final_clip_owner, p_material_owner);
        }
    }

    if (ci->copy_back_buffer) {
        ci->copy_back_buffer->screen_rect = xform.xform(ci->copy_back_buffer->rect).clip(p_clip_rect);
    }

    if (ci->update_when_visible) {
        RenderingServerRaster::redraw_request(false);
    }

    if ((!ci->commands.empty() && p_clip_rect.intersects(global_rect,true)) || ci->vp_render || ci->copy_back_buffer) {
        //something to draw?
        ci->final_transform = xform;
        ci->final_modulate = Color(modulate.r * ci->self_modulate.r, modulate.g * ci->self_modulate.g, modulate.b * ci->self_modulate.b, modulate.a * ci->self_modulate.a);
        ci->global_rect_cache = global_rect;
        ci->global_rect_cache.position -= p_clip_rect.position;
        ci->light_masked = false;

        int zidx = p_z - RS::CANVAS_ITEM_Z_MIN;

        z_list[zidx].emplace_back(ci);
    }

    for (int i = 0; i < child_item_count; i++) {
        auto & child(canvas_items_view.get<RenderingCanvasItemComponent>(child_items[i]));

        if (child.behind || (ci->sort_y && child.sort_y)) {
            continue;
        }
        if (ci->sort_y) {
            _render_canvas_item(child_items[i], xform * child.ysort_xform, p_clip_rect, modulate * child.ysort_modulate, p_z, z_list,
                    ci->final_clip_owner, child.material_owner);
        } else {
            _render_canvas_item(child_items[i], xform, p_clip_rect, modulate, p_z, z_list, (RenderingCanvasItemComponent *)ci->final_clip_owner, p_material_owner);
        }
    }
}
}

void RenderingServerCanvas::_render_canvas_item_tree(RenderingEntity p_canvas_item, const Transform2D &p_transform, const Rect2 &p_clip_rect, const Color &p_modulate, Span<RasterizerCanvasLight3DComponent *> p_lights) {
    z_sort_arr.clear();
    z_sort_arr.resize(z_range);

    _render_canvas_item(p_canvas_item, p_transform, p_clip_rect, Color(1, 1, 1, 1), 0, z_sort_arr, nullptr, nullptr);

    VSG::canvas_render->canvas_render_items_begin(p_modulate, p_lights, p_transform);
    for (int i = 0; i < z_range; i++) {
        if (z_sort_arr[i].empty())
            continue;
        VSG::canvas_render->canvas_render_items(z_sort_arr[i], RS::CANVAS_ITEM_Z_MIN + i, p_modulate, p_lights, p_transform);
    }
    VSG::canvas_render->canvas_render_items_end();
}


void _light_mask_canvas_items(int p_z, const Dequeue<RasterizerCanvas::Item *> &p_canvas_item, Span<RasterizerCanvasLight3DComponent *> p_masked_lights) {

    if (p_masked_lights.empty())
        return;

    for(RasterizerCanvas::Item *ci : p_canvas_item) {
        for(RasterizerCanvasLight3DComponent *light : p_masked_lights) {
            if (ci->light_mask & light->item_mask && p_z >= light->z_min && p_z <= light->z_max && ci->global_rect_cache.intersects_transformed(light->xform_cache, light->rect_cache)) {
                ci->light_masked = true;
            }
        }

    }
}

void RenderingServerCanvas::render_canvas(RenderingCanvasComponent *p_canvas, const Transform2D &p_transform,
        Span<RasterizerCanvasLight3DComponent *> p_lights, Span<RasterizerCanvasLight3DComponent *> p_masked_lights,
        const Rect2 &p_clip_rect) {

    VSG::canvas_render->canvas_begin();

    if (p_canvas->children_order_dirty) {

        eastl::sort(p_canvas->child_items.begin(),p_canvas->child_items.end(),ChildSorter());
        p_canvas->children_order_dirty = false;
    }

    int l = p_canvas->child_items.size();
    RenderingCanvasComponent::ChildItem *ci = p_canvas->child_items.data();

    bool has_mirror = false;
    for (int i = 0; i < l; i++) {
        if (ci[i].mirror.x || ci[i].mirror.y) {
            has_mirror = true;
            break;
        }
    }

    if (!has_mirror) {

        constexpr int z_range = RS::CANVAS_ITEM_Z_MAX - RS::CANVAS_ITEM_Z_MIN + 1;
        Dequeue<RasterizerCanvas::Item *> z_list[z_range];

        for (int i = 0; i < l; i++) {
            _render_canvas_item(ci[i].item, p_transform, p_clip_rect, Color(1, 1, 1, 1), 0, z_list, nullptr, nullptr);
        }
        VSG::canvas_render->canvas_render_items_begin(p_canvas->modulate, p_lights, p_transform);
        for (int i = 0; i < z_range; i++) {
            if (z_list[i].empty())
                continue;

            if (!p_masked_lights.empty()) {
                _light_mask_canvas_items(RS::CANVAS_ITEM_Z_MIN + i, z_list[i], p_masked_lights);
            }

            VSG::canvas_render->canvas_render_items(z_list[i], RS::CANVAS_ITEM_Z_MIN + i, p_canvas->modulate, p_lights, p_transform);
        }
        VSG::canvas_render->canvas_render_items_end();
    } else {

        for (int i = 0; i < l; i++) {

            const RenderingCanvasComponent::ChildItem &ci2 = p_canvas->child_items[i];
            _render_canvas_item_tree(ci2.item, p_transform, p_clip_rect, p_canvas->modulate, p_lights);

            //mirroring (useful for scrolling backgrounds)
            if (ci2.mirror.x != 0) {

                Transform2D xform2 = p_transform * Transform2D(0, Vector2(ci2.mirror.x, 0));
                _render_canvas_item_tree(ci2.item, xform2, p_clip_rect, p_canvas->modulate, p_lights);
            }
            if (ci2.mirror.y != 0) {

                Transform2D xform2 = p_transform * Transform2D(0, Vector2(0, ci2.mirror.y));
                _render_canvas_item_tree(ci2.item, xform2, p_clip_rect, p_canvas->modulate, p_lights);
            }
            if (ci2.mirror.y != 0 && ci2.mirror.x != 0) {

                Transform2D xform2 = p_transform * Transform2D(0, ci2.mirror);
                _render_canvas_item_tree(ci2.item, xform2, p_clip_rect, p_canvas->modulate, p_lights);
            }
        }
    }

    VSG::canvas_render->canvas_end();
}

RenderingEntity RenderingServerCanvas::canvas_create() {
    auto res = VSG::ecs->create();
    VSG::ecs->registry.emplace<RenderingCanvasComponent>(res).self=res;
    return res;
}

void RenderingServerCanvas::canvas_set_item_mirroring(RenderingEntity p_canvas, RenderingEntity p_item, const Point2 &p_mirroring) {

    auto *canvas = VSG::ecs->try_get<RenderingCanvasComponent>(p_canvas);
    ERR_FAIL_COND(!canvas);
    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    int idx = canvas->find_item(p_canvas);
    ERR_FAIL_COND(idx == -1);
    canvas->child_items[idx].mirror = p_mirroring;
}
void RenderingServerCanvas::canvas_set_modulate(RenderingEntity p_canvas, const Color &p_color) {

    auto *canvas = VSG::ecs->try_get<RenderingCanvasComponent>(p_canvas);
    ERR_FAIL_COND(!canvas);
    canvas->modulate = p_color;
}

void RenderingServerCanvas::canvas_set_disable_scale(bool p_disable) {
    disable_scale = p_disable;
}

void RenderingServerCanvas::canvas_set_parent(RenderingEntity p_canvas, RenderingEntity p_parent, float p_scale) {

    auto *canvas = VSG::ecs->try_get<RenderingCanvasComponent>(p_canvas);
    ERR_FAIL_COND(!canvas);

    canvas->parent = p_parent;
    canvas->parent_scale = p_scale;
}

RenderingEntity RenderingServerCanvas::canvas_item_create() {
    auto res=VSG::ecs->create();
    VSG::ecs->registry.emplace<RenderingCanvasItemComponent>(res).self = res;
    return res;
}

void RenderingServerCanvas::canvas_item_set_parent(RenderingEntity p_item, RenderingEntity p_parent) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);
    auto *new_canvas_parent = get<RenderingCanvasComponent>(p_parent);
    auto *new_canvas_item_parent = get<RenderingCanvasItemComponent>(p_parent);
    auto *old_canvas_parent = get<RenderingCanvasComponent>(canvas_item->parent);
    auto *old_canvas_item_parent = get<RenderingCanvasItemComponent>(canvas_item->parent);


    if (old_canvas_parent||old_canvas_item_parent) {
        if (old_canvas_parent) {
            old_canvas_parent->erase_item(p_item);
        }
        if (old_canvas_item_parent) {

            old_canvas_item_parent->child_items.erase_first(p_item);

            if (old_canvas_item_parent->sort_y) {
                _mark_ysort_dirty(old_canvas_item_parent);
            }
        }

        canvas_item->parent = entt::null;
    }

    if (new_canvas_parent||new_canvas_item_parent) {
        if (new_canvas_parent) {
            RenderingCanvasComponent::ChildItem ci;
            ci.item = p_item;
            new_canvas_parent->child_items.emplace_back(ci);
            new_canvas_parent->children_order_dirty = true;
        }
        if (new_canvas_item_parent) {
            new_canvas_item_parent->child_items.emplace_back(p_item);
            new_canvas_item_parent->children_order_dirty = true;

            if (new_canvas_item_parent->sort_y) {
                _mark_ysort_dirty(new_canvas_item_parent);
            }
            }
        } else {

        if(p_parent!=entt::null) {
            ERR_FAIL_MSG("Invalid parent.");
        }
    }

    canvas_item->parent = p_parent;
}
void RenderingServerCanvas::canvas_item_set_visible(RenderingEntity p_item, bool p_visible) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->visible = p_visible;

    _mark_ysort_dirty(canvas_item);
}
void RenderingServerCanvas::canvas_item_set_light_mask(RenderingEntity p_item, int p_mask) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->light_mask = p_mask;
}

void RenderingServerCanvas::canvas_item_set_transform(RenderingEntity p_item, const Transform2D &p_transform) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->xform = p_transform;
}
void RenderingServerCanvas::canvas_item_set_clip(RenderingEntity p_item, bool p_clip) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->clip = p_clip;
}
void RenderingServerCanvas::canvas_item_set_distance_field_mode(RenderingEntity p_item, bool p_enable) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->distance_field = p_enable;
}
void RenderingServerCanvas::canvas_item_set_custom_rect(RenderingEntity p_item, bool p_custom_rect, const Rect2 &p_rect) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->custom_rect = p_custom_rect;
    canvas_item->rect = p_rect;
}
void RenderingServerCanvas::canvas_item_set_modulate(RenderingEntity p_item, const Color &p_color) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->modulate = p_color;
}
void RenderingServerCanvas::canvas_item_set_self_modulate(RenderingEntity p_item, const Color &p_color) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->self_modulate = p_color;
}

void RenderingServerCanvas::canvas_item_set_draw_behind_parent(RenderingEntity p_item, bool p_enable) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->behind = p_enable;
}

void RenderingServerCanvas::canvas_item_set_update_when_visible(RenderingEntity p_item, bool p_update) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->update_when_visible = p_update;
}

void RenderingServerCanvas::canvas_item_add_line(RenderingEntity p_item, const Point2 &p_from, const Point2 &p_to, const Color &p_color, float p_width, bool p_antialiased) {
    // Try drawing as a poly, because polys are batched and thus should run faster than thick lines,
    // which run extremely slowly.
    if (!p_antialiased && (p_width > 1.0f)) {
        // use poly drawing, as it is faster as it can use batching
        Point2 uvs[4];
        Vector2 side = p_to - p_from;
        real_t length = side.length();
        if (length == 0.0f) {
            // Not sure yet whether zero length is a noop operation later on,
            // watch for visual errors. If there are visual errors, pass through
            // to the line drawing routine below.
            return;
        }

             // normalize
        side /= length;

             // 90 degrees
        side = Vector2(-side.y, side.x);
        side *= p_width * 0.5f;
        const Point2 points[4] = {
            p_from + side,
            p_from - side,
            p_to - side,
            p_to + side
        };
        const Color colors[4] {p_color,p_color,p_color,p_color};

        canvas_item_add_polygon(p_item, points, colors, uvs, entt::null, entt::null, false);
        return;
    }

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandLine *line = memnew(RenderingCanvasItemComponent::CommandLine);
    ERR_FAIL_COND(!line);
    line->color = p_color;
    line->from = p_from;
    line->to = p_to;
    line->width = p_width;
    line->antialiased = p_antialiased;
    canvas_item->rect_dirty = true;

    canvas_item->commands.push_back(line);
}

void RenderingServerCanvas::canvas_item_add_polyline(RenderingEntity  p_item, Span<const Vector2> p_points, Span<const Color> p_colors, float p_width, bool p_antialiased) {

    ERR_FAIL_COND(p_points.size() < 2);
    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandPolyLine *pline = memnew(RenderingCanvasItemComponent::CommandPolyLine);
    ERR_FAIL_COND(!pline);

    pline->antialiased = p_antialiased;
    pline->multiline = false;

    if (p_width <= 1) {
        pline->lines.assign(p_points.begin(),p_points.end());
        pline->line_colors.assign(p_colors.begin(),p_colors.end());
        if (pline->line_colors.empty()) {
            pline->line_colors.push_back(Color(1, 1, 1, 1));
        } else if (pline->line_colors.size() > 1 && pline->line_colors.size() != pline->lines.size()) {
            pline->line_colors.resize(1);
        }
    } else {
        //make a trianglestrip for drawing the line...
        Vector2 prev_t;
        pline->triangles.resize(p_points.size() * 2);
        if (p_antialiased) {
            pline->lines.resize(p_points.size() * 2);
        }

        if (p_colors.empty()) {
            pline->triangle_colors.assign(p_antialiased ? 2 : 1,Color(1, 1, 1, 1));
        } else if (p_colors.size() == 1) {
            pline->triangle_colors.assign(p_colors.begin(),p_colors.end());
            pline->line_colors.assign(p_colors.begin(),p_colors.end());
        } else {
            if (p_colors.size() != p_points.size()) {
                pline->triangle_colors.push_back(p_colors[0]);
                pline->line_colors.push_back(p_colors[0]);
            } else {
                pline->triangle_colors.resize(pline->triangles.size());
                pline->line_colors.resize(pline->lines.size());
            }
        }
        auto &linewrite = pline->lines;
        for (size_t i = 0; i < p_points.size(); i++) {

            Vector2 t;
            if (i == p_points.size() - 1) {
                t = prev_t;
            } else {
                t = (p_points[i + 1] - p_points[i]).normalized().tangent();
                if (i == 0) {
                    prev_t = t;
                }
            }

            Vector2 tangent = ((t + prev_t).normalized()) * p_width * 0.5;

            if (p_antialiased) {
                linewrite[i] = p_points[i] + tangent;
                linewrite[p_points.size() * 2 - i - 1] = p_points[i] - tangent;
                if (pline->line_colors.size() > 1) {
                    pline->line_colors[i] = p_colors[i];
                    pline->line_colors[p_points.size() * 2 - i - 1] = p_colors[i];
                }
            }

            pline->triangles[i * 2 + 0] = p_points[i] + tangent;
            pline->triangles[i * 2 + 1] = p_points[i] - tangent;

            if (pline->triangle_colors.size() > 1) {

                pline->triangle_colors[i * 2 + 0] = p_colors[i];
                pline->triangle_colors[i * 2 + 1] = p_colors[i];
            }

            prev_t = t;
        }
    }
    canvas_item->rect_dirty = true;
    canvas_item->commands.push_back(pline);
}

void RenderingServerCanvas::canvas_item_add_multiline(RenderingEntity  p_item, Span<const Vector2> p_points, Span<const Color> p_colors, float p_width, bool p_antialiased) {

    ERR_FAIL_COND(p_points.size() < 2);
    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandPolyLine *pline = memnew(RenderingCanvasItemComponent::CommandPolyLine);
    ERR_FAIL_COND(!pline);

    pline->antialiased = false; //todo
    pline->multiline = true;

    pline->lines.assign(p_points.begin(),p_points.end());
    pline->line_colors.assign(p_colors.begin(),p_colors.end());
    if (pline->line_colors.empty()) {
        pline->line_colors.emplace_back(Color(1, 1, 1, 1));
    } else if (pline->line_colors.size() > 1 && pline->line_colors.size() != pline->lines.size()) {
        pline->line_colors.resize(1);
    }

    canvas_item->rect_dirty = true;
    canvas_item->commands.push_back(pline);
}

void RenderingServerCanvas::canvas_item_add_rect(RenderingEntity p_item, const Rect2 &p_rect, const Color &p_color) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandRect *rect = memnew(RenderingCanvasItemComponent::CommandRect);
    ERR_FAIL_COND(!rect);
    rect->modulate = p_color;
    rect->rect = p_rect;
    canvas_item->rect_dirty = true;

    canvas_item->commands.push_back(rect);
}

void RenderingServerCanvas::canvas_item_add_circle(RenderingEntity p_item, const Point2 &p_pos, float p_radius, const Color &p_color) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandCircle *circle = memnew(RenderingCanvasItemComponent::CommandCircle);
    ERR_FAIL_COND(!circle);
    circle->color = p_color;
    circle->pos = p_pos;
    circle->radius = p_radius;

    canvas_item->commands.push_back(circle);
}

void RenderingServerCanvas::canvas_item_add_texture_rect(RenderingEntity p_item, const Rect2 &p_rect, RenderingEntity p_texture, bool p_tile, const Color &p_modulate, bool p_transpose, RenderingEntity p_normal_map) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);
    assert(p_texture!=entt::null||p_normal_map!=entt::null);

    RenderingCanvasItemComponent::CommandRect *rect = memnew(RenderingCanvasItemComponent::CommandRect);
    ERR_FAIL_COND(!rect);
    rect->modulate = p_modulate;
    rect->rect = p_rect;
    rect->flags = 0;
    if (p_tile) {
        rect->flags |= RasterizerCanvas::CANVAS_RECT_TILE;
        rect->flags |= RasterizerCanvas::CANVAS_RECT_REGION;
        rect->source = Rect2(0, 0, fabsf(p_rect.size.width), fabsf(p_rect.size.height));
    }

    if (p_rect.size.x < 0) {

        rect->flags |= RasterizerCanvas::CANVAS_RECT_FLIP_H;
        rect->rect.size.x = -rect->rect.size.x;
    }
    if (p_rect.size.y < 0) {

        rect->flags |= RasterizerCanvas::CANVAS_RECT_FLIP_V;
        rect->rect.size.y = -rect->rect.size.y;
    }
    if (p_transpose) {
        rect->flags |= RasterizerCanvas::CANVAS_RECT_TRANSPOSE;
        SWAP(rect->rect.size.x, rect->rect.size.y);
    }
    rect->texture = p_texture;
    rect->normal_map = p_normal_map;
    canvas_item->rect_dirty = true;
    canvas_item->commands.push_back(rect);
}

void RenderingServerCanvas::canvas_item_add_texture_rect_region(RenderingEntity p_item, const Rect2 &p_rect, RenderingEntity p_texture, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, RenderingEntity p_normal_map, bool p_clip_uv) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);
    assert(p_texture!=entt::null||p_normal_map!=entt::null);

    RenderingCanvasItemComponent::CommandRect *rect = memnew(RenderingCanvasItemComponent::CommandRect);
    ERR_FAIL_COND(!rect);
    rect->modulate = p_modulate;
    rect->rect = p_rect;
    rect->texture = p_texture;
    rect->normal_map = p_normal_map;
    rect->source = p_src_rect;
    rect->flags = RasterizerCanvas::CANVAS_RECT_REGION;

    if (p_rect.size.x < 0) {

        rect->flags |= RasterizerCanvas::CANVAS_RECT_FLIP_H;
        rect->rect.size.x = -rect->rect.size.x;
    }
    if (p_src_rect.size.x < 0) {

        rect->flags ^= RasterizerCanvas::CANVAS_RECT_FLIP_H;
        rect->source.size.x = -rect->source.size.x;
    }
    if (p_rect.size.y < 0) {

        rect->flags |= RasterizerCanvas::CANVAS_RECT_FLIP_V;
        rect->rect.size.y = -rect->rect.size.y;
    }
    if (p_src_rect.size.y < 0) {

        rect->flags ^= RasterizerCanvas::CANVAS_RECT_FLIP_V;
        rect->source.size.y = -rect->source.size.y;
    }
    if (p_transpose) {
        rect->flags |= RasterizerCanvas::CANVAS_RECT_TRANSPOSE;
        SWAP(rect->rect.size.x, rect->rect.size.y);
    }

    if (p_clip_uv) {
        rect->flags |= RasterizerCanvas::CANVAS_RECT_CLIP_UV;
    }

    canvas_item->rect_dirty = true;

    canvas_item->commands.push_back(rect);
}

void RenderingServerCanvas::canvas_item_add_nine_patch(RenderingEntity p_item, const Rect2 &p_rect, const Rect2 &p_source, RenderingEntity p_texture, const Vector2 &p_topleft, const Vector2 &p_bottomright, RS::NinePatchAxisMode p_x_axis_mode, RS::NinePatchAxisMode p_y_axis_mode, bool p_draw_center, const Color &p_modulate, RenderingEntity p_normal_map) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandNinePatch *style = memnew(RenderingCanvasItemComponent::CommandNinePatch);
    ERR_FAIL_COND(!style);
    style->texture = p_texture;
    style->normal_map = p_normal_map;
    style->rect = p_rect;
    style->source = p_source;
    style->draw_center = p_draw_center;
    style->color = p_modulate;
    style->margin[(int8_t)Margin::Left] = p_topleft.x;
    style->margin[(int8_t)Margin::Top] = p_topleft.y;
    style->margin[(int8_t)Margin::Right] = p_bottomright.x;
    style->margin[(int8_t)Margin::Bottom] = p_bottomright.y;
    style->axis_x = p_x_axis_mode;
    style->axis_y = p_y_axis_mode;
    canvas_item->rect_dirty = true;

    canvas_item->commands.push_back(style);
}
void RenderingServerCanvas::canvas_item_add_primitive(RenderingEntity  p_item, Span<const Point2> p_points, Span<const Color> p_colors, const PoolVector<Point2> &p_uvs, RenderingEntity  p_texture, float p_width, RenderingEntity  p_normal_map) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandPrimitive *prim = memnew(RenderingCanvasItemComponent::CommandPrimitive);
    ERR_FAIL_COND(!prim);
    prim->texture = p_texture;
    prim->normal_map = p_normal_map;
    prim->points.assign(p_points.begin(),p_points.end());;
    prim->uvs = p_uvs;
    prim->colors.assign(p_colors.begin(),p_colors.end());
    prim->width = p_width;
    canvas_item->rect_dirty = true;

    canvas_item->commands.push_back(prim);
}

void RenderingServerCanvas::canvas_item_add_polygon(RenderingEntity p_item, Span<const Point2> p_points, Span<const Color> p_colors, Span<const Point2> p_uvs, RenderingEntity p_texture, RenderingEntity p_normal_map, bool p_antialiased) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);
#ifdef DEBUG_ENABLED
    int pointcount = p_points.size();
    ERR_FAIL_COND(pointcount < 3);
    int color_size = p_colors.size();
    int uv_size = p_uvs.size();
    ERR_FAIL_COND(color_size != 0 && color_size != 1 && color_size != pointcount);
    ERR_FAIL_COND(uv_size != 0 && (uv_size != pointcount));
#endif
    Vector<int> indices = Geometry::triangulate_polygon(p_points);
    ERR_FAIL_COND_MSG(indices.empty(), "Invalid polygon data, triangulation failed.");

    RenderingCanvasItemComponent::CommandPolygon *polygon = memnew(RenderingCanvasItemComponent::CommandPolygon);
    ERR_FAIL_COND(!polygon);
    polygon->texture = p_texture;
    polygon->normal_map = p_normal_map;
    polygon->points.assign(p_points.begin(),p_points.end());
    polygon->uvs.assign(p_uvs.begin(),p_uvs.end());
    polygon->colors.assign(p_colors.begin(),p_colors.end());
    polygon->indices = indices;
    polygon->count = indices.size();
    polygon->antialiased = p_antialiased;
    polygon->antialiasing_use_indices = false;
    canvas_item->rect_dirty = true;

    canvas_item->commands.push_back(polygon);
}

void RenderingServerCanvas::canvas_item_add_triangle_array(RenderingEntity p_item, Span<const int> p_indices,
        Span<const Point2> p_points, Span<const Color> p_colors, Span<const Point2> p_uvs,
        const PoolVector<int> &p_bones, const PoolVector<float> &p_weights, RenderingEntity p_texture, int p_count,
        RenderingEntity p_normal_map, bool p_antialiased, bool p_antialiasing_use_indices) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    int vertex_count(p_points.size());
    ERR_FAIL_COND(vertex_count == 0);
    ERR_FAIL_COND(!p_colors.empty() && p_colors.size() != vertex_count && p_colors.size() != 1);
    ERR_FAIL_COND(!p_uvs.empty() && p_uvs.size() != vertex_count);
    ERR_FAIL_COND(!p_bones.empty() && p_bones.size() != vertex_count * 4);
    ERR_FAIL_COND(!p_weights.empty() && p_weights.size() != vertex_count * 4);

    int count = p_count * 3;

    if (p_indices.empty()) {

        ERR_FAIL_COND(vertex_count % 3 != 0);
        if (p_count == -1)
            count = vertex_count;
    } else {

        ERR_FAIL_COND(p_indices.size() % 3 != 0);
        if (p_count == -1)
            count = p_indices.size();
    }

    RenderingCanvasItemComponent::CommandPolygon *polygon = memnew(RenderingCanvasItemComponent::CommandPolygon);
    ERR_FAIL_COND(!polygon);
    polygon->texture = p_texture;
    polygon->normal_map = p_normal_map;
    polygon->points.assign(p_points.begin(),p_points.end());
    polygon->uvs.assign(p_uvs.begin(),p_uvs.end());
    polygon->colors.assign(p_colors.begin(),p_colors.end());
    polygon->bones = p_bones;
    polygon->weights = p_weights;
    polygon->indices.assign(p_indices.begin(),p_indices.end());
    polygon->count = count;
    polygon->antialiased = p_antialiased;
    polygon->antialiasing_use_indices = p_antialiasing_use_indices;
    canvas_item->rect_dirty = true;

    canvas_item->commands.push_back(polygon);
}

void RenderingServerCanvas::canvas_item_add_set_transform(RenderingEntity p_item, const Transform2D &p_transform) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandTransform *tr = memnew(RenderingCanvasItemComponent::CommandTransform);
    ERR_FAIL_COND(!tr);
    tr->xform = p_transform;

    canvas_item->commands.push_back(tr);
}

void RenderingServerCanvas::canvas_item_add_mesh(RenderingEntity  p_item, const RenderingEntity  &p_mesh, const Transform2D &p_transform, const Color &p_modulate, RenderingEntity  p_texture, RenderingEntity  p_normal_map) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandMesh *m = memnew(RenderingCanvasItemComponent::CommandMesh);
    ERR_FAIL_COND(!m);
    m->mesh = p_mesh;
    m->texture = p_texture;
    m->normal_map = p_normal_map;
    m->transform = p_transform;
    m->modulate = p_modulate;

    canvas_item->commands.push_back(m);
}
void RenderingServerCanvas::canvas_item_add_particles(RenderingEntity p_item, RenderingEntity p_particles, RenderingEntity p_texture, RenderingEntity p_normal) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandParticles *part = memnew(RenderingCanvasItemComponent::CommandParticles);
    ERR_FAIL_COND(!part);
    part->particles = p_particles;
    part->texture = p_texture;
    part->normal_map = p_normal;

    //take the chance and request processing for them, at least once until they become visible again
    VSG::storage->particles_request_process(p_particles);

    canvas_item->rect_dirty = true;
    canvas_item->commands.push_back(part);
}

void RenderingServerCanvas::canvas_item_add_multimesh(RenderingEntity p_item, RenderingEntity p_mesh, RenderingEntity p_texture, RenderingEntity p_normal_map) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandMultiMesh *mm = memnew(RenderingCanvasItemComponent::CommandMultiMesh);
    ERR_FAIL_COND(!mm);
    mm->multimesh = p_mesh;
    mm->texture = p_texture;
    mm->normal_map = p_normal_map;

    canvas_item->rect_dirty = true;
    canvas_item->commands.push_back(mm);
}

void RenderingServerCanvas::canvas_item_add_clip_ignore(RenderingEntity p_item, bool p_ignore) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    RenderingCanvasItemComponent::CommandClipIgnore *ci = memnew(RenderingCanvasItemComponent::CommandClipIgnore);
    ERR_FAIL_COND(!ci);
    ci->ignore = p_ignore;

    canvas_item->commands.push_back(ci);
}
void RenderingServerCanvas::canvas_item_set_sort_children_by_y(RenderingEntity p_item, bool p_enable) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->sort_y = p_enable;

    _mark_ysort_dirty(canvas_item);
}
void RenderingServerCanvas::canvas_item_set_z_index(RenderingEntity p_item, int p_z) {

    ERR_FAIL_COND(p_z < RS::CANVAS_ITEM_Z_MIN || p_z > RS::CANVAS_ITEM_Z_MAX);

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->z_index = p_z;
}
void RenderingServerCanvas::canvas_item_set_z_as_relative_to_parent(RenderingEntity p_item, bool p_enable) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->z_relative = p_enable;
}

void RenderingServerCanvas::canvas_item_attach_skeleton(RenderingEntity p_item, RenderingEntity p_skeleton) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->skeleton = p_skeleton;
}

void RenderingServerCanvas::canvas_item_set_copy_to_backbuffer(RenderingEntity p_item, bool p_enable, const Rect2 &p_rect) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);
    if (bool(canvas_item->copy_back_buffer != nullptr) != p_enable) {
        if (p_enable) {
            canvas_item->copy_back_buffer = memnew(RasterizerCanvas::Item::CopyBackBuffer);
        } else {
            memdelete(canvas_item->copy_back_buffer.value);
            canvas_item->copy_back_buffer = nullptr;
        }
    }

    if (p_enable) {
        canvas_item->copy_back_buffer->rect = p_rect;
        canvas_item->copy_back_buffer->full = p_rect == Rect2();
    }
}

void RenderingServerCanvas::canvas_item_clear(RenderingEntity p_item) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->clear();
}
void RenderingServerCanvas::canvas_item_set_draw_index(RenderingEntity p_item, int p_index) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->index = p_index;

    auto *canvas_parent = VSG::ecs->try_get<RenderingCanvasComponent>(canvas_item->parent);
    auto *canvas_item_parent = VSG::ecs->try_get<RenderingCanvasItemComponent>(canvas_item->parent);

    if (canvas_parent) {
        canvas_parent->children_order_dirty = true;
    }
    if (canvas_item_parent) {
        canvas_item_parent->children_order_dirty = true;
    }
}

void RenderingServerCanvas::canvas_item_set_material(RenderingEntity p_item, RenderingEntity p_material) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->material = p_material;
}

void RenderingServerCanvas::canvas_item_set_use_parent_material(RenderingEntity p_item, bool p_enable) {

    auto *canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(p_item);
    ERR_FAIL_COND(!canvas_item);

    canvas_item->use_parent_material = p_enable;
}

RenderingEntity RenderingServerCanvas::canvas_light_create() {
    auto res = VSG::ecs->create();
    auto &clight(VSG::ecs->registry.emplace<RasterizerCanvasLight3DComponent>(res));
    clight.self = res;
    clight.light_internal = VSG::canvas_render->light_internal_create();
    return res;
}
void RenderingServerCanvas::canvas_light_attach_to_canvas(RenderingEntity p_light, RenderingEntity p_canvas) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    if (clight->canvas!=entt::null) {

        RenderingCanvasComponent *canvas = VSG::ecs->try_get<RenderingCanvasComponent>(clight->canvas);
        canvas->lights.erase(p_light);
    }

    auto *canvas_cmp = get<RenderingCanvasComponent>(p_canvas);
    if (!canvas_cmp)
        p_canvas = entt::null;

    clight->canvas = p_canvas;

    if (canvas_cmp) {
        canvas_cmp->lights.insert(p_light);
    }
}

void RenderingServerCanvas::canvas_light_set_enabled(RenderingEntity p_light, bool p_enabled) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->enabled = p_enabled;
}
void RenderingServerCanvas::canvas_light_set_scale(RenderingEntity p_light, float p_scale) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->scale = p_scale;
}
void RenderingServerCanvas::canvas_light_set_transform(RenderingEntity p_light, const Transform2D &p_transform) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->xform = p_transform;
}
void RenderingServerCanvas::canvas_light_set_texture(RenderingEntity p_light, RenderingEntity p_texture) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->texture = p_texture;
}
void RenderingServerCanvas::canvas_light_set_texture_offset(RenderingEntity p_light, const Vector2 &p_offset) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->texture_offset = p_offset;
}
void RenderingServerCanvas::canvas_light_set_color(RenderingEntity p_light, const Color &p_color) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->color = p_color;
}
void RenderingServerCanvas::canvas_light_set_height(RenderingEntity p_light, float p_height) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->height = p_height;
}
void RenderingServerCanvas::canvas_light_set_energy(RenderingEntity p_light, float p_energy) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->energy = p_energy;
}
void RenderingServerCanvas::canvas_light_set_z_range(RenderingEntity p_light, int p_min_z, int p_max_z) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->z_min = p_min_z;
    clight->z_max = p_max_z;
}
void RenderingServerCanvas::canvas_light_set_layer_range(RenderingEntity p_light, int p_min_layer, int p_max_layer) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->layer_max = p_max_layer;
    clight->layer_min = p_min_layer;
}
void RenderingServerCanvas::canvas_light_set_item_cull_mask(RenderingEntity p_light, int p_mask) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->item_mask = p_mask;
}
void RenderingServerCanvas::canvas_light_set_item_shadow_cull_mask(RenderingEntity p_light, int p_mask) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->item_shadow_mask = p_mask;
}
void RenderingServerCanvas::canvas_light_set_mode(RenderingEntity p_light, RS::CanvasLightMode p_mode) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->mode = p_mode;
}

void RenderingServerCanvas::canvas_light_set_shadow_enabled(RenderingEntity p_light, bool p_enabled) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    if ((clight->shadow_buffer!=entt::null) == p_enabled)
        return;
    if (p_enabled) {
        clight->shadow_buffer = VSG::storage->canvas_light_shadow_buffer_create(clight->shadow_buffer_size);
    } else {
        VSG::storage->free(clight->shadow_buffer);
        clight->shadow_buffer = entt::null;
    }
}
void RenderingServerCanvas::canvas_light_set_shadow_buffer_size(RenderingEntity p_light, int p_size) {

    ERR_FAIL_COND(p_size < 32 || p_size > 16384);

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    int new_size = next_power_of_2(p_size);
    if (new_size == clight->shadow_buffer_size)
        return;

    clight->shadow_buffer_size = next_power_of_2(p_size);

    if (clight->shadow_buffer!=entt::null) {
        VSG::storage->free(clight->shadow_buffer);
        clight->shadow_buffer = VSG::storage->canvas_light_shadow_buffer_create(clight->shadow_buffer_size);
    }
}

void RenderingServerCanvas::canvas_light_set_shadow_gradient_length(RenderingEntity p_light, float p_length) {

    ERR_FAIL_COND(p_length < 0);

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->shadow_gradient_length = p_length;
}
void RenderingServerCanvas::canvas_light_set_shadow_filter(RenderingEntity p_light, RS::CanvasLightShadowFilter p_filter) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->shadow_filter = p_filter;
}
void RenderingServerCanvas::canvas_light_set_shadow_color(RenderingEntity p_light, const Color &p_color) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);

    clight->shadow_color = p_color;
}

void RenderingServerCanvas::canvas_light_set_shadow_smooth(RenderingEntity p_light, float p_smooth) {

    auto *clight = VSG::ecs->try_get<RasterizerCanvasLight3DComponent>(p_light);
    ERR_FAIL_COND(!clight);
    clight->shadow_smooth = p_smooth;
}

RenderingEntity RenderingServerCanvas::canvas_light_occluder_create() {
    auto res = VSG::ecs->create();

    VSG::ecs->registry.emplace<RasterizerCanvasLightOccluderInstanceComponent>(res).self=res;
    return res;
}
void RenderingServerCanvas::canvas_light_occluder_attach_to_canvas(RenderingEntity p_occluder, RenderingEntity p_canvas) {

    auto *occluder = VSG::ecs->try_get<RasterizerCanvasLightOccluderInstanceComponent>(p_occluder);
    ERR_FAIL_COND(!occluder);

    if (occluder->canvas!=entt::null) {

        auto *canvas = VSG::ecs->try_get<RenderingCanvasComponent>(occluder->canvas);
        canvas->occluders.erase(p_occluder);
    }

    auto *new_canvas = get<RenderingCanvasComponent>(p_canvas);

    if (!new_canvas)
        p_canvas = entt::null;

    occluder->canvas = p_canvas;

    if (new_canvas) {
        new_canvas->occluders.insert(p_occluder);
    }
}
void RenderingServerCanvas::canvas_light_occluder_set_enabled(RenderingEntity p_occluder, bool p_enabled) {

    auto *occluder = VSG::ecs->try_get<RasterizerCanvasLightOccluderInstanceComponent>(p_occluder);
    ERR_FAIL_COND(!occluder);

    occluder->enabled = p_enabled;
}
void RenderingServerCanvas::canvas_light_occluder_set_polygon(RenderingEntity p_occluder, RenderingEntity p_polygon) {

    auto *occluder = VSG::ecs->try_get<RasterizerCanvasLightOccluderInstanceComponent>(p_occluder);
    ERR_FAIL_COND(!occluder);

    if (occluder->polygon!=entt::null) {
        auto *occluder_poly = VSG::ecs->try_get<LightOccluderPolygonComponent>(occluder->polygon);
        if (occluder_poly) {
            occluder_poly->owners.erase(p_occluder);
        }
    }

    occluder->polygon = p_polygon;
    occluder->polygon_buffer = entt::null;

    if (occluder->polygon == entt::null) {
        return;
    }
    auto *occluder_poly = VSG::ecs->try_get<LightOccluderPolygonComponent>(p_polygon);
    if (!occluder_poly) {
        occluder->polygon = entt::null;
        ERR_FAIL_MSG("!occluder_poly");
    } else {
        occluder_poly->owners.insert(p_occluder);
        occluder->polygon_buffer = occluder_poly->occluder.value;
        occluder->aabb_cache = occluder_poly->aabb;
        occluder->cull_cache = occluder_poly->cull_mode;
    }
}
void RenderingServerCanvas::canvas_light_occluder_set_transform(RenderingEntity p_occluder, const Transform2D &p_xform) {

    auto *occluder = VSG::ecs->try_get<RasterizerCanvasLightOccluderInstanceComponent>(p_occluder);
    ERR_FAIL_COND(!occluder);

    occluder->xform = p_xform;
}
void RenderingServerCanvas::canvas_light_occluder_set_light_mask(RenderingEntity p_occluder, int p_mask) {

    auto *occluder = VSG::ecs->try_get<RasterizerCanvasLightOccluderInstanceComponent>(p_occluder);
    ERR_FAIL_COND(!occluder);

    occluder->light_mask = p_mask;
}

RenderingEntity RenderingServerCanvas::canvas_occluder_polygon_create() {
    auto res = VSG::ecs->create();
    auto &occluder_poly(VSG::ecs->registry.emplace<LightOccluderPolygonComponent>(res));
    occluder_poly.occluder = VSG::storage->canvas_light_occluder_create();
    return res;
}
void RenderingServerCanvas::canvas_occluder_polygon_set_shape(RenderingEntity p_occluder_polygon, Span<const Vector2> p_shape, bool p_closed) {

    if (p_shape.size() < 3) {
        canvas_occluder_polygon_set_shape_as_lines(p_occluder_polygon, p_shape);
        return;
    }

    Vector<Vector2> lines;
    const int lc = p_shape.size() * 2;
    const int max = (lc / 2) - (p_closed ? 0 : 1);

    lines.reserve(2*max);

    for (int i = 0; i < max; i++) {

        Vector2 a = p_shape[i];
        Vector2 b = p_shape[(i + 1) % (lc / 2)];
        lines.emplace_back(a);
        lines.emplace_back(b);
    }
    canvas_occluder_polygon_set_shape_as_lines(p_occluder_polygon, lines);
}
void RenderingServerCanvas::canvas_occluder_polygon_set_shape_as_lines(RenderingEntity p_occluder_polygon, Span<const Vector2> p_shape) {

    auto *occluder_poly = VSG::ecs->try_get<LightOccluderPolygonComponent>(p_occluder_polygon);
    ERR_FAIL_COND(!occluder_poly);
    ERR_FAIL_COND(p_shape.size() & 1);

    int lc = p_shape.size();
    occluder_poly->aabb = Rect2();
    {
        for (int i = 0; i < lc; i++) {
            if (i == 0)
                occluder_poly->aabb.position = p_shape[i];
            else
                occluder_poly->aabb.expand_to(p_shape[i]);
        }
    }

    VSG::storage->canvas_light_occluder_set_polylines(occluder_poly->occluder, p_shape);
    for (RenderingEntity  e: occluder_poly->owners) {
        VSG::ecs->try_get<RasterizerCanvasLightOccluderInstanceComponent>(e)->aabb_cache = occluder_poly->aabb;
    }
}

void RenderingServerCanvas::canvas_occluder_polygon_set_cull_mode(RenderingEntity p_occluder_polygon, RS::CanvasOccluderPolygonCullMode p_mode) {

    auto *occluder_poly = VSG::ecs->try_get<LightOccluderPolygonComponent>(p_occluder_polygon);
    ERR_FAIL_COND(!occluder_poly);
    occluder_poly->cull_mode = p_mode;
    for (RenderingEntity  e: occluder_poly->owners) {
        VSG::ecs->try_get<RasterizerCanvasLightOccluderInstanceComponent>(e)->cull_cache = p_mode;
    }
}

bool RenderingServerCanvas::free(RenderingEntity p_rid) {

    if (VSG::ecs->registry.any_of<RenderingCanvasComponent,RenderingCanvasItemComponent,RasterizerCanvasLight3DComponent,RasterizerCanvasLightOccluderInstanceComponent,LightOccluderPolygonComponent>(p_rid)) {
    } else {
        return false;
    }
    VSG::ecs->registry.destroy(p_rid);
    return true;
}

RenderingServerCanvas::RenderingServerCanvas() {
    z_sort_arr.resize(z_range);

    disable_scale = false;
}

RenderingServerCanvas::~RenderingServerCanvas() {
}

void RenderingCanvasComponent::release_resources() {
    if(self!=entt::null) {
    for (RenderingEntity vp_ent : viewports) {
        auto *vp = VSG::ecs->try_get<RenderingViewportCanvasComponent>(vp_ent);
        ERR_FAIL_COND(!vp);

        auto E = vp->canvas_map.find(self);
        ERR_FAIL_COND(E == vp->canvas_map.end());
            vp->canvas_map.erase(E);
        }
    }
    viewports.clear();
    if(!child_items.empty()) {
        auto view = VSG::ecs->registry.view<RenderingCanvasItemComponent>();
        for (const ChildItem & ci : child_items) {
            view.get<RenderingCanvasItemComponent>(ci.item).parent = entt::null;
        }
        child_items.clear();
    }
    auto lights_view(VSG::ecs->registry.view<RasterizerCanvasLight3DComponent>());
    auto occluders_view(VSG::ecs->registry.view<RasterizerCanvasLightOccluderInstanceComponent>());

    for (auto light_ent : lights) {
        auto *E = &lights_view.get<RasterizerCanvasLight3DComponent>(light_ent);
        E->canvas = entt::null;
    }
    lights.clear();

    for (auto occluder_ent : occluders) {
        auto *E = &occluders_view.get<RasterizerCanvasLightOccluderInstanceComponent>(occluder_ent);
        E->canvas = entt::null;
    }
    occluders.clear();
}

RenderingCanvasComponent &RenderingCanvasComponent::operator=(RenderingCanvasComponent &&from) {
    release_resources();

    viewports = eastl::move(from.viewports);
    lights = eastl::move(from.lights);
    occluders = eastl::move(from.occluders);
    child_items = eastl::move(from.child_items);
    modulate = from.modulate;
    parent = eastl::move(from.parent);
    self = eastl::move(from.self);
    parent_scale = from.parent_scale;
    children_order_dirty = from.children_order_dirty;
    return *this;
}

void RenderingCanvasItemComponent::release_resources() {
    auto view = VSG::ecs->registry.view<RenderingCanvasItemComponent>();

    if (parent != entt::null) {
        auto *parent_canvas = VSG::ecs->try_get<RenderingCanvasComponent>(parent);
        if (parent_canvas) {
            parent_canvas->erase_item(self);
        }
        auto *parent_canvas_item = VSG::ecs->try_get<RenderingCanvasItemComponent>(parent);

        if (parent_canvas_item) {
            parent_canvas_item->child_items.erase_first(self);

            if (parent_canvas_item->sort_y) {
                _mark_ysort_dirty(parent_canvas_item);
            }
        }
    }
    for (int i = 0; i < child_items.size(); i++) {
        view.get<RenderingCanvasItemComponent>(child_items[i]).parent = entt::null;
        //child_items[i]->parent = entt::null;
    }
    //TODO: investigate releasiong material ownership here ?
    /*
    if (canvas_item->material) {
        canvas_item->material->owners.erase(canvas_item);
    }
    */
}

RenderingCanvasItemComponent &RenderingCanvasItemComponent::operator=(RenderingCanvasItemComponent &&from) noexcept {
    release_resources();
    // move the base class fields as well !
    Item::operator=(eastl::move(from));
    parent = eastl::move(from.parent);
    self = eastl::move(from.self);
    z_index=from.z_index;
    z_relative=from.z_relative;
    sort_y=from.sort_y;
    modulate = from.modulate;
    self_modulate = from.self_modulate;
    use_parent_material=from.use_parent_material;
    index = from.index;
    children_order_dirty = from.children_order_dirty;
    ysort_children_count = from.ysort_children_count;
    ysort_modulate = from.ysort_modulate;
    ysort_xform = from.ysort_xform;
    ysort_pos = from.ysort_pos;
    ysort_index = from.ysort_index;

    return *this;
}

void LightOccluderPolygonComponent::release_resources() {

    if(occluder!=entt::null) {
        VSG::storage->free(occluder);
    }
    occluder = entt::null;

    for(RenderingEntity e : owners) {
        VSG::ecs->try_get<RasterizerCanvasLightOccluderInstanceComponent>(e)->polygon = entt::null;
    }
    owners.clear();
}

LightOccluderPolygonComponent::~LightOccluderPolygonComponent()
{
    release_resources();
}
