/*************************************************************************/
/*  rasterizer.cpp                                                       */
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

#include "rasterizer.h"
#include "rendering_server_canvas.h"

#include "servers/rendering/render_entity_getter.h"
#include "servers/rendering/rendering_server_globals.h"
#include "core/os/os.h"
#include "core/print_string.h"

Rasterizer *(*Rasterizer::_create_func)() = nullptr;

Rasterizer *Rasterizer::create() {

    return _create_func();
}

RasterizerStorage *RasterizerStorage::base_singleton = nullptr;

RasterizerStorage::RasterizerStorage() {

    base_singleton = this;
}

RasterizerScene::~RasterizerScene() {}

const Rect2 &RasterizerCanvas::Item::get_rect() const {
    if (custom_rect) {
        return rect;

    }
    if (!rect_dirty && !update_when_visible) {
        if (skeleton == entt::null) {
            return rect;
        } else {
            // special case for skeletons
            uint32_t rev = RasterizerStorage::base_singleton->skeleton_get_revision(skeleton);
            if (rev == skeleton_revision) {
                // no change to the skeleton since we last calculated the bounding rect
                return rect;
            } else {
                // We need to recalculate.
                // Mark as done for next time.
                skeleton_revision = rev;
            }
        }
    }
         //must update rect
    int s = commands.size();
    if (s == 0) {

        rect = Rect2();
        rect_dirty = false;
        return rect;
    }

    Transform2D xf;
    bool found_xform = false;
    bool first = true;

    const Item::Command *const *cmd = &commands[0];

    for (int i = 0; i < s; i++) {

        const Item::Command *c = cmd[i];
        Rect2 r;

        switch (c->type) {
            case Item::Command::TYPE_LINE: {

                const Item::CommandLine *line = static_cast<const Item::CommandLine *>(c);
                r.position = line->from;
                r.expand_to(line->to);
            } break;
            case Item::Command::TYPE_POLYLINE: {

                const Item::CommandPolyLine *pline = static_cast<const Item::CommandPolyLine *>(c);
                if (!pline->triangles.empty()) {
                    for (int j = 0; j < pline->triangles.size(); j++) {

                        if (j == 0) {
                            r.position = pline->triangles[j];
                        } else {
                            r.expand_to(pline->triangles[j]);
                        }
                    }
                } else {

                    for (int j = 0; j < pline->lines.size(); j++) {

                        if (j == 0) {
                            r.position = pline->lines[j];
                        } else {
                            r.expand_to(pline->lines[j]);
                        }
                    }
                }

            } break;
            case Item::Command::TYPE_RECT: {

                const Item::CommandRect *crect = static_cast<const Item::CommandRect *>(c);
                r = crect->rect;

            } break;
            case Item::Command::TYPE_NINEPATCH: {

                const Item::CommandNinePatch *style = static_cast<const Item::CommandNinePatch *>(c);
                r = style->rect;
            } break;
            case Item::Command::TYPE_PRIMITIVE: {

                const Item::CommandPrimitive *primitive = static_cast<const Item::CommandPrimitive *>(c);
                r.position = primitive->points[0];
                for (int j = 1; j < primitive->points.size(); j++) {
                    r.expand_to(primitive->points[j]);
                }
            } break;
            case Item::Command::TYPE_POLYGON: {

                const Item::CommandPolygon *polygon = static_cast<const Item::CommandPolygon *>(c);
                int l = polygon->points.size();
                const Point2 *pp = &polygon->points[0];
                r.position = pp[0];
                for (int j = 1; j < l; j++) {
                    r.expand_to(pp[j]);
                }
            } break;
            case Item::Command::TYPE_MESH: {

                const Item::CommandMesh *mesh = static_cast<const Item::CommandMesh *>(c);
                AABB aabb = RasterizerStorage::base_singleton->mesh_get_aabb(mesh->mesh, entt::null);

                r = Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y);

            } break;
            case Item::Command::TYPE_MULTIMESH: {

                const Item::CommandMultiMesh *multimesh = static_cast<const Item::CommandMultiMesh *>(c);
                AABB aabb = RasterizerStorage::base_singleton->multimesh_get_aabb(multimesh->multimesh);

                r = Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y);

            } break;
            case Item::Command::TYPE_PARTICLES: {

                const Item::CommandParticles *particles_cmd = static_cast<const Item::CommandParticles *>(c);
                if (particles_cmd->particles!=entt::null) {
                    AABB aabb = RasterizerStorage::base_singleton->particles_get_aabb(particles_cmd->particles);
                    r = Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y);
                }

            } break;
            case Item::Command::TYPE_CIRCLE: {

                const Item::CommandCircle *circle = static_cast<const Item::CommandCircle *>(c);
                r.position = Point2(-circle->radius, -circle->radius) + circle->pos;
                r.size = Point2(circle->radius * 2.0, circle->radius * 2.0);
            } break;
            case Item::Command::TYPE_TRANSFORM: {

                const Item::CommandTransform *transform = static_cast<const Item::CommandTransform *>(c);
                xf = transform->xform;
                found_xform = true;
                continue;
            } break;

            case Item::Command::TYPE_CLIP_IGNORE: {

            } break;
        }

        if (found_xform) {
            r = xf.xform(r);
        }

        if (first) {
            rect = r;
            first = false;
        } else
            rect = rect.merge(r);
    }

    rect_dirty = false;
    return rect;
}


void RasterizerCanvasLight3DComponent::release_resources()
{
    if (canvas != entt::null) {
        auto *bound_canvas = VSG::ecs->try_get<RenderingCanvasComponent>(canvas);
        if (bound_canvas)
            bound_canvas->lights.erase(self);
        canvas = entt::null;
    }

    if (shadow_buffer != entt::null) {
        VSG::storage->free(shadow_buffer);
        shadow_buffer = entt::null;
    }
    if (light_internal != entt::null) {
        VSG::canvas_render->light_internal_free(light_internal);
        light_internal = entt::null;
    }
}

RasterizerCanvasLight3DComponent &RasterizerCanvasLight3DComponent::operator=(RasterizerCanvasLight3DComponent &&from)
{
    release_resources();

    enabled = eastl::move(from.enabled);

    color = eastl::move(from.color);
    xform = eastl::move(from.xform);
    height = eastl::move(from.height);
    energy = eastl::move(from.energy);
    scale = eastl::move(from.scale);
    z_min = eastl::move(from.z_min);
    z_max = eastl::move(from.z_max);
    layer_min = eastl::move(from.layer_min);
    layer_max = eastl::move(from.layer_max);
    item_mask = eastl::move(from.item_mask);
    item_shadow_mask = eastl::move(from.item_shadow_mask);
    texture_offset = eastl::move(from.texture_offset);
    texture = eastl::move(from.texture);
    self = eastl::move(from.self);
    canvas = eastl::move(from.canvas);
    shadow_buffer = eastl::move(from.shadow_buffer);
    shadow_color = eastl::move(from.shadow_color);
    shadow_gradient_length = eastl::move(from.shadow_gradient_length);
    shadow_smooth = eastl::move(from.shadow_smooth);
    shadow_buffer_size = eastl::move(from.shadow_buffer_size);

    shadow_matrix_cache = eastl::move(from.shadow_matrix_cache);
    rect_cache = eastl::move(from.rect_cache);
    xform_cache = eastl::move(from.xform_cache);
    texture_cache = eastl::move(from.texture_cache);
    radius_cache = eastl::move(from.radius_cache);

    light_shader_xform = eastl::move(from.light_shader_xform);
    light_shader_pos = eastl::move(from.light_shader_pos);

    light_internal = eastl::move(from.light_internal);

    mode = eastl::move(from.mode);
    shadow_filter = eastl::move(from.shadow_filter);

    return *this;
}
void RasterizerCanvasLightOccluderInstanceComponent::release_resources() {
    if (polygon != entt::null) {
        auto *occluder_poly = VSG::ecs->try_get<LightOccluderPolygonComponent>(polygon);
        if (occluder_poly) {
            occluder_poly->owners.erase(self);
        }
        polygon = entt::null;
    }
    auto *our_canvas = get<RenderingCanvasComponent>(canvas);
    if (our_canvas) {
        our_canvas->occluders.erase(self);
    }
    canvas = entt::null;
}

RasterizerCanvasLightOccluderInstanceComponent &RasterizerCanvasLightOccluderInstanceComponent::operator=(RasterizerCanvasLightOccluderInstanceComponent &&from) {
    release_resources();

    aabb_cache = from.aabb_cache;
    xform = from.xform;
    xform_cache = from.xform_cache;
    next = from.next;
    from.next=entt::null;

    self = eastl::move(from.self);
    canvas = eastl::move(from.canvas);
    polygon = eastl::move(from.polygon);
    polygon_buffer=from.polygon_buffer; // not used in destructor
    light_mask = from.light_mask;
    cull_cache = from.cull_cache;
    enabled =  from.enabled;
    return *this;
}
