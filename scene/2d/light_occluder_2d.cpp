/*************************************************************************/
/*  light_occluder_2d.cpp                                                */
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

#include "light_occluder_2d.h"

#include "core/callable_method_pointer.h"
#include "core/engine.h"
#include "core/math/geometry.h"
#include "core/method_bind.h"
#include "core/translation_helpers.h"
#include "servers/rendering_server.h"

#define LINE_GRAB_WIDTH 8

IMPL_GDCLASS(OccluderPolygon2D)
IMPL_GDCLASS(LightOccluder2D)
VARIANT_ENUM_CAST(OccluderPolygon2D::CullMode);

#ifdef TOOLS_ENABLED
Rect2 OccluderPolygon2D::_edit_get_rect() const {

    if (likely(!rect_cache_dirty))
        return item_rect;

    rect_cache_dirty = false;

    if (polygon.empty()) {
        item_rect = Rect2();
        return item_rect;
    }
    if (closed) {
        item_rect.position = polygon.front();
        for (Vector2 pos : polygon) {
            item_rect.expand_to(pos);
        }
    } else {
        Vector2 d = Vector2(LINE_GRAB_WIDTH, LINE_GRAB_WIDTH);
        item_rect = Rect2(polygon[0] - d, 2 * d);
        for (int i = 1; i < polygon.size(); i++) {
            item_rect.expand_to(polygon[i] - d);
            item_rect.expand_to(polygon[i] + d);
        }
    }
    return item_rect;
}

bool OccluderPolygon2D::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {

    if (closed) {
        return Geometry::is_point_in_polygon(p_point, polygon);
    }
    const real_t d = LINE_GRAB_WIDTH / 2.0f + p_tolerance;
    for (int i = 0; i < polygon.size() - 1; i++) {
        Vector2 p = Geometry::get_closest_point_to_segment_2d(p_point, &polygon[i]);
        if (p.distance_to(p_point) <= d)
            return true;
    }

    return false;
}
#endif

void OccluderPolygon2D::set_polygon(const Vector<Vector2> &p_polygon) {

    polygon = p_polygon;
    rect_cache_dirty = true;
    RenderingServer::get_singleton()->canvas_occluder_polygon_set_shape(occ_polygon, polygon, closed);
    emit_changed();
}



void OccluderPolygon2D::set_closed(bool p_closed) {

    if (closed == p_closed)
        return;
    closed = p_closed;
    if (polygon.size())
        RenderingServer::get_singleton()->canvas_occluder_polygon_set_shape(occ_polygon, polygon, closed);
    emit_changed();
}

bool OccluderPolygon2D::is_closed() const {

    return closed;
}

void OccluderPolygon2D::set_cull_mode(CullMode p_mode) {

    cull = p_mode;
    RenderingServer::get_singleton()->canvas_occluder_polygon_set_cull_mode(occ_polygon, RS::CanvasOccluderPolygonCullMode(p_mode));
}

OccluderPolygon2D::CullMode OccluderPolygon2D::get_cull_mode() const {

    return cull;
}

RenderingEntity OccluderPolygon2D::get_rid() const {

    return occ_polygon;
}

void OccluderPolygon2D::_bind_methods() {

    SE_BIND_METHOD(OccluderPolygon2D,set_closed);
    SE_BIND_METHOD(OccluderPolygon2D,is_closed);

    SE_BIND_METHOD(OccluderPolygon2D,set_cull_mode);
    SE_BIND_METHOD(OccluderPolygon2D,get_cull_mode);

    SE_BIND_METHOD(OccluderPolygon2D,set_polygon);
    SE_BIND_METHOD(OccluderPolygon2D,get_polygon);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "closed"), "set_closed", "is_closed");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "cull_mode", PropertyHint::Enum, "Disabled,ClockWise,CounterClockWise"), "set_cull_mode", "get_cull_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR2_ARRAY, "polygon"), "set_polygon", "get_polygon");

    BIND_ENUM_CONSTANT(CULL_DISABLED);
    BIND_ENUM_CONSTANT(CULL_CLOCKWISE);
    BIND_ENUM_CONSTANT(CULL_COUNTER_CLOCKWISE);
}

OccluderPolygon2D::OccluderPolygon2D() {

    occ_polygon = RenderingServer::get_singleton()->canvas_occluder_polygon_create();
    closed = true;
    cull = CULL_DISABLED;
    rect_cache_dirty = true;
}

OccluderPolygon2D::~OccluderPolygon2D() {

    RenderingServer::get_singleton()->free_rid(occ_polygon);
}

void LightOccluder2D::_poly_changed() {

#ifdef DEBUG_ENABLED
    update();
#endif
}

void LightOccluder2D::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_CANVAS) {

        RenderingServer::get_singleton()->canvas_light_occluder_attach_to_canvas(occluder, get_canvas());
        RenderingServer::get_singleton()->canvas_light_occluder_set_transform(occluder, get_global_transform());
        RenderingServer::get_singleton()->canvas_light_occluder_set_enabled(occluder, is_visible_in_tree());
    }
    if (p_what == NOTIFICATION_TRANSFORM_CHANGED) {

        RenderingServer::get_singleton()->canvas_light_occluder_set_transform(occluder, get_global_transform());
    }
    if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {

        RenderingServer::get_singleton()->canvas_light_occluder_set_enabled(occluder, is_visible_in_tree());
    }

    if (p_what == NOTIFICATION_DRAW) {

        if (Engine::get_singleton()->is_editor_hint()) {

            if (occluder_polygon) {

                const Vector<Vector2> &poly = occluder_polygon->get_polygon();

                if (poly.size()) {
                    if (occluder_polygon->is_closed()) {
                        Color color[1] = {Color(0, 0, 0, 0.6f)};
                        draw_polygon(poly, color);
                    } else {

                        int ps = poly.size();
                        for (int i = 0; i < ps - 1; i++) {
                            draw_line(poly[i], poly[i + 1], Color(0, 0, 0, 0.6f), 3);
                        }
                    }
                }
            }
        }
    }

    if (p_what == NOTIFICATION_EXIT_CANVAS) {

        RenderingServer::get_singleton()->canvas_light_occluder_attach_to_canvas(occluder, entt::null);
    }
}
#ifdef TOOLS_ENABLED
Rect2 LightOccluder2D::_edit_get_rect() const {

    return occluder_polygon ? occluder_polygon->_edit_get_rect() : Rect2();
}

bool LightOccluder2D::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {

    return occluder_polygon ? occluder_polygon->_edit_is_selected_on_click(p_point, p_tolerance) : false;
}
#endif
void LightOccluder2D::set_occluder_polygon(const Ref<OccluderPolygon2D> &p_polygon) {

#ifdef DEBUG_ENABLED
    if (occluder_polygon)
        occluder_polygon->disconnect("changed",callable_mp(this, &ClassName::_poly_changed));
#endif
    occluder_polygon = p_polygon;

    if (occluder_polygon)
        RenderingServer::get_singleton()->canvas_light_occluder_set_polygon(occluder, occluder_polygon->get_rid());
    else
        RenderingServer::get_singleton()->canvas_light_occluder_set_polygon(occluder, entt::null);

#ifdef DEBUG_ENABLED
    if (occluder_polygon)
        occluder_polygon->connect("changed",callable_mp(this, &ClassName::_poly_changed));
    update();
#endif
}

Ref<OccluderPolygon2D> LightOccluder2D::get_occluder_polygon() const {

    return occluder_polygon;
}

void LightOccluder2D::set_occluder_light_mask(int p_mask) {

    mask = p_mask;
    RenderingServer::get_singleton()->canvas_light_occluder_set_light_mask(occluder, mask);
}

int LightOccluder2D::get_occluder_light_mask() const {

    return mask;
}

String LightOccluder2D::get_configuration_warning() const {

    String warning = BaseClassName::get_configuration_warning();
    if (!occluder_polygon) {
        if (!warning.empty()){
            warning += "\n\n";
        }
        warning += TTR("An occluder polygon must be set (or drawn) for this occluder to take effect.");
    }

    if (occluder_polygon && occluder_polygon->get_polygon().size() == 0) {
        if (!warning.empty()){
            warning += "\n\n";
        }
        warning += TTR("The occluder polygon for this occluder is empty. Please draw a polygon.");
    }

    return warning;
}

void LightOccluder2D::_bind_methods() {

    SE_BIND_METHOD(LightOccluder2D,set_occluder_polygon);
    SE_BIND_METHOD(LightOccluder2D,get_occluder_polygon);

    SE_BIND_METHOD(LightOccluder2D,set_occluder_light_mask);
    SE_BIND_METHOD(LightOccluder2D,get_occluder_light_mask);

    MethodBinder::bind_method("_poly_changed", &LightOccluder2D::_poly_changed);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "occluder", PropertyHint::ResourceType, "OccluderPolygon2D"), "set_occluder_polygon", "get_occluder_polygon");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "light_mask", PropertyHint::Layers2DRenderer), "set_occluder_light_mask", "get_occluder_light_mask");
}

LightOccluder2D::LightOccluder2D() {

    occluder = RenderingServer::get_singleton()->canvas_light_occluder_create();
    mask = 1;
    set_notify_transform(true);
}

LightOccluder2D::~LightOccluder2D() {

    RenderingServer::get_singleton()->free_rid(occluder);
}
