/*************************************************************************/
/*  style_box.cpp                                                        */
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

#include "style_box.h"
#include "scene/2d/canvas_item.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/string_name.h"

#include <climits>

IMPL_GDCLASS(StyleBox)
IMPL_GDCLASS(StyleBoxEmpty)
IMPL_GDCLASS(StyleBoxTexture)
IMPL_GDCLASS(StyleBoxFlat)
IMPL_GDCLASS(StyleBoxLine)
RES_BASE_EXTENSION_IMPL(StyleBox,"stylebox")

VARIANT_ENUM_CAST(StyleBoxTexture::AxisStretchMode);

bool StyleBox::test_mask(const Point2 &p_point, const Rect2 &p_rect) const {

    return true;
}

void StyleBox::set_default_margin(Margin p_margin, float p_value) {
    ERR_FAIL_INDEX((int)p_margin, (int)Margin::Max);

    margin[(int)p_margin] = p_value;
    emit_changed();
}
float StyleBox::get_default_margin(Margin p_margin) const {
    ERR_FAIL_INDEX_V((int)p_margin, (int)Margin::Max,0);

    return margin[(int)p_margin];
}

float StyleBox::get_margin(Margin p_margin) const {
    ERR_FAIL_INDEX_V((int)p_margin, (int)Margin::Max,0);

    if (margin[(int)p_margin] < 0)
        return get_style_margin(p_margin);
    else
        return margin[(int)p_margin];
}


Size2 StyleBox::get_minimum_size() const {

    return Size2(get_margin(Margin::Left) + get_margin(Margin::Right), get_margin(Margin::Top) + get_margin(Margin::Bottom));
}

Point2 StyleBox::get_offset() const {

    return Point2(get_margin(Margin::Left), get_margin(Margin::Top));
}

Size2 StyleBox::get_center_size() const {

    return Size2();
}

Rect2 StyleBox::get_draw_rect(const Rect2 &p_rect) const {
    return p_rect;
}

void StyleBox::_bind_methods() {

    SE_BIND_METHOD(StyleBox,test_mask);

    SE_BIND_METHOD(StyleBox,set_default_margin);
    SE_BIND_METHOD(StyleBox,get_default_margin);


    SE_BIND_METHOD(StyleBox,get_margin);
    SE_BIND_METHOD(StyleBox,get_minimum_size);
    SE_BIND_METHOD(StyleBox,get_center_size);
    SE_BIND_METHOD(StyleBox,get_offset);

    SE_BIND_METHOD(StyleBox,draw);

    ADD_GROUP("Content Margin", "content_margin_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "content_margin_left", PropertyHint::Range, "-1,2048,1"), "set_default_margin", "get_default_margin", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "content_margin_right", PropertyHint::Range, "-1,2048,1"), "set_default_margin", "get_default_margin", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "content_margin_top", PropertyHint::Range, "-1,2048,1"), "set_default_margin", "get_default_margin", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "content_margin_bottom", PropertyHint::Range, "-1,2048,1"), "set_default_margin", "get_default_margin", (int)Margin::Bottom);
}

StyleBox::StyleBox() {

    for (int i = 0; i < 4; i++) {

        margin[i] = -1;
    }
}

void StyleBoxTexture::set_texture(const Ref<Texture>& p_texture) {

    if (texture == p_texture)
        return;
    texture = p_texture;
    if (not p_texture) {
        region_rect = Rect2(0, 0, 0, 0);
    } else {
        region_rect = Rect2(Point2(), texture->get_size());
    }
    emit_signal("texture_changed");
    emit_changed();
    Object_change_notify(this,"texture");
}

Ref<Texture> StyleBoxTexture::get_texture() const {

    return texture;
}

void StyleBoxTexture::set_normal_map(const Ref<Texture>& p_normal_map) {

    if (normal_map == p_normal_map)
        return;
    normal_map = p_normal_map;
    emit_changed();
}

Ref<Texture> StyleBoxTexture::get_normal_map() const {

    return normal_map;
}

void StyleBoxTexture::set_margin_size(Margin p_margin, float p_size) {

    ERR_FAIL_INDEX((int)p_margin, 4);

    margin[(int)p_margin] = p_size;
    emit_changed();
    static const char *margin_prop[4] = {
        "content_margin_left",
        "content_margin_top",
        "content_margin_right",
        "content_margin_bottom",
    };
    Object_change_notify(this,StaticCString(margin_prop[(int)p_margin],true));
}
float StyleBoxTexture::get_margin_size(Margin p_margin) const {
    ERR_FAIL_INDEX_V((int)p_margin, (int)Margin::Max,0);

    return margin[(int)p_margin];
}

float StyleBoxTexture::get_style_margin(Margin p_margin) const {
    ERR_FAIL_INDEX_V((int)p_margin, (int)Margin::Max,0);

    return margin[(int)p_margin];
}

Rect2 StyleBoxTexture::get_draw_rect(const Rect2 &p_rect) const {
    return p_rect.grow_individual(expand_margin[(int8_t)Margin::Left], expand_margin[(int8_t)Margin::Top], expand_margin[(int8_t)Margin::Right], expand_margin[(int8_t)Margin::Bottom]);
}
void StyleBoxTexture::draw(RenderingEntity p_canvas_item, const Rect2 &p_rect) const {
    if (not texture)
        return;

    Rect2 rect = p_rect;
    Rect2 src_rect = region_rect;

    texture->get_rect_region(rect, src_rect, rect, src_rect);

    rect.position.x -= expand_margin[(int8_t)Margin::Left];
    rect.position.y -= expand_margin[(int8_t)Margin::Top];
    rect.size.x += expand_margin[(int8_t)Margin::Left] + expand_margin[(int8_t)Margin::Right];
    rect.size.y += expand_margin[(int8_t)Margin::Top] + expand_margin[(int8_t)Margin::Bottom];

    RenderingEntity normal_rid=entt::null;
    if (normal_map)
        normal_rid = normal_map->get_rid();

    RenderingServer::get_singleton()->canvas_item_add_nine_patch(p_canvas_item, rect, src_rect, texture->get_rid(),
        Vector2(margin[(int8_t)Margin::Left], margin[(int8_t)Margin::Top]),
        Vector2(margin[(int8_t)Margin::Right], margin[(int8_t)Margin::Bottom]), RS::NinePatchAxisMode(axis_h),
        RS::NinePatchAxisMode(axis_v), draw_center, modulate, normal_rid);
}

void StyleBoxTexture::set_draw_center(bool p_enabled) {

    draw_center = p_enabled;
    emit_changed();
}

Size2 StyleBoxTexture::get_center_size() const {

    if (not texture)
        return Size2();

    return region_rect.size - get_minimum_size();
}

void StyleBoxTexture::set_expand_margin_size(Margin p_expand_margin, float p_size) {

    ERR_FAIL_INDEX((int)p_expand_margin, 4);
    expand_margin[(int)p_expand_margin] = p_size;
    emit_changed();
}

void StyleBoxTexture::set_expand_margin_size_individual(float p_left, float p_top, float p_right, float p_bottom) {
    expand_margin[(int8_t)Margin::Left] = p_left;
    expand_margin[(int8_t)Margin::Top] = p_top;
    expand_margin[(int8_t)Margin::Right] = p_right;
    expand_margin[(int8_t)Margin::Bottom] = p_bottom;
    emit_changed();
}

void StyleBoxTexture::set_expand_margin_size_all(float p_expand_margin_size) {
    for (int i = 0; i < 4; i++) {

        expand_margin[i] = p_expand_margin_size;
    }
    emit_changed();
}

float StyleBoxTexture::get_expand_margin_size(Margin p_expand_margin) const {

    ERR_FAIL_INDEX_V((int)p_expand_margin, 4, 0);
    return expand_margin[(int)p_expand_margin];
}

void StyleBoxTexture::set_region_rect(const Rect2 &p_region_rect) {

    if (region_rect == p_region_rect)
        return;

    region_rect = p_region_rect;
    emit_changed();
    Object_change_notify(this,"region");
}

Rect2 StyleBoxTexture::get_region_rect() const {

    return region_rect;
}

void StyleBoxTexture::set_h_axis_stretch_mode(AxisStretchMode p_mode) {
    ERR_FAIL_INDEX((int)p_mode, 3);

    axis_h = p_mode;
    emit_changed();
}

StyleBoxTexture::AxisStretchMode StyleBoxTexture::get_h_axis_stretch_mode() const {

    return axis_h;
}

void StyleBoxTexture::set_v_axis_stretch_mode(AxisStretchMode p_mode) {
    ERR_FAIL_INDEX((int)p_mode, 3);

    axis_v = p_mode;
    emit_changed();
}

StyleBoxTexture::AxisStretchMode StyleBoxTexture::get_v_axis_stretch_mode() const {

    return axis_v;
}

void StyleBoxTexture::set_modulate(const Color &p_modulate) {
    if (modulate == p_modulate)
        return;
    modulate = p_modulate;
    emit_changed();
}

void StyleBoxTexture::_bind_methods() {

    SE_BIND_METHOD(StyleBoxTexture,set_texture);
    SE_BIND_METHOD(StyleBoxTexture,get_texture);

    SE_BIND_METHOD(StyleBoxTexture,set_normal_map);
    SE_BIND_METHOD(StyleBoxTexture,get_normal_map);

    SE_BIND_METHOD(StyleBoxTexture,set_margin_size);
    SE_BIND_METHOD(StyleBoxTexture,get_margin_size);

    SE_BIND_METHOD(StyleBoxTexture,set_expand_margin_size);
    MethodBinder::bind_method(D_METHOD("set_expand_margin_all", {"size"}), &StyleBoxTexture::set_expand_margin_size_all);
    MethodBinder::bind_method(D_METHOD("set_expand_margin_individual", {"size_left", "size_top", "size_right", "size_bottom"}), &StyleBoxTexture::set_expand_margin_size_individual);
    SE_BIND_METHOD(StyleBoxTexture,get_expand_margin_size);

    SE_BIND_METHOD(StyleBoxTexture,set_region_rect);
    SE_BIND_METHOD(StyleBoxTexture,get_region_rect);

    SE_BIND_METHOD(StyleBoxTexture,set_draw_center);
    SE_BIND_METHOD(StyleBoxTexture,is_draw_center_enabled);

    SE_BIND_METHOD(StyleBoxTexture,set_modulate);
    SE_BIND_METHOD(StyleBoxTexture,get_modulate);

    SE_BIND_METHOD(StyleBoxTexture,set_h_axis_stretch_mode);
    SE_BIND_METHOD(StyleBoxTexture,get_h_axis_stretch_mode);

    SE_BIND_METHOD(StyleBoxTexture,set_v_axis_stretch_mode);
    SE_BIND_METHOD(StyleBoxTexture,get_v_axis_stretch_mode);

    ADD_SIGNAL(MethodInfo("texture_changed"));

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "normal_map", PropertyHint::ResourceType, "Texture"), "set_normal_map", "get_normal_map");
    ADD_PROPERTY(PropertyInfo(VariantType::RECT2, "region_rect"), "set_region_rect", "get_region_rect");
    ADD_GROUP("Margin", "margin_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "margin_left", PropertyHint::Range, "0,2048,1"), "set_margin_size", "get_margin_size", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "margin_right", PropertyHint::Range, "0,2048,1"), "set_margin_size", "get_margin_size", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "margin_top", PropertyHint::Range, "0,2048,1"), "set_margin_size", "get_margin_size", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "margin_bottom", PropertyHint::Range, "0,2048,1"), "set_margin_size", "get_margin_size", (int)Margin::Bottom);
    ADD_GROUP("Expand Margin", "expand_margin_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "expand_margin_left", PropertyHint::Range, "0,2048,1"), "set_expand_margin_size", "get_expand_margin_size", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "expand_margin_right", PropertyHint::Range, "0,2048,1"), "set_expand_margin_size", "get_expand_margin_size", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "expand_margin_top", PropertyHint::Range, "0,2048,1"), "set_expand_margin_size", "get_expand_margin_size", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "expand_margin_bottom", PropertyHint::Range, "0,2048,1"), "set_expand_margin_size", "get_expand_margin_size", (int)Margin::Bottom);
    ADD_GROUP("Axis Stretch", "axis_stretch_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "axis_stretch_horizontal", PropertyHint::Enum, "Stretch,Tile,Tile Fit"), "set_h_axis_stretch_mode", "get_h_axis_stretch_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "axis_stretch_vertical", PropertyHint::Enum, "Stretch,Tile,Tile Fit"), "set_v_axis_stretch_mode", "get_v_axis_stretch_mode");
    ADD_GROUP("Modulate", "modulate_");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "modulate_color"), "set_modulate", "get_modulate");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "draw_center"), "set_draw_center", "is_draw_center_enabled");

    BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_STRETCH);
    BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_TILE);
    BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_TILE_FIT);
}

StyleBoxTexture::StyleBoxTexture() {

    for (int i = 0; i < 4; i++) {
        margin[i] = 0;
        expand_margin[i] = 0;
    }
    draw_center = true;
    modulate = Color(1, 1, 1, 1);

    axis_h = AXIS_STRETCH_MODE_STRETCH;
    axis_v = AXIS_STRETCH_MODE_STRETCH;
}
StyleBoxTexture::~StyleBoxTexture() {
}

////////////////

void StyleBoxFlat::set_bg_color(const Color &p_color) {

    bg_color = p_color;
    emit_changed();
}

Color StyleBoxFlat::get_bg_color() const {

    return bg_color;
}

void StyleBoxFlat::set_border_color(const Color &p_color) {

    border_color = p_color;
    emit_changed();
}
Color StyleBoxFlat::get_border_color() const {

    return border_color;
}

void StyleBoxFlat::set_border_width_all(int p_size) {
    border_width[0] = p_size;
    border_width[1] = p_size;
    border_width[2] = p_size;
    border_width[3] = p_size;
    emit_changed();
}
int StyleBoxFlat::get_border_width_min() const {

    return MIN(MIN(border_width[0], border_width[1]), MIN(border_width[2], border_width[3]));
}

void StyleBoxFlat::set_border_width(Margin p_margin, int p_width) {
    ERR_FAIL_INDEX((int)p_margin, 4);
    border_width[(int)p_margin] = p_width;
    emit_changed();
}

int StyleBoxFlat::get_border_width(Margin p_margin) const {
    ERR_FAIL_INDEX_V((int)p_margin, 4, 0);
    return border_width[(int)p_margin];
}

void StyleBoxFlat::set_border_blend(bool p_blend) {

    blend_border = p_blend;
    emit_changed();
}
bool StyleBoxFlat::get_border_blend() const {

    return blend_border;
}

void StyleBoxFlat::set_corner_radius_all(int radius) {

    for (int i = 0; i < 4; i++) {
        corner_radius[i] = radius;
    }

    emit_changed();
}
void StyleBoxFlat::set_corner_radius_individual(const int radius_top_left, const int radius_top_right, const int radius_bottom_right, const int radius_bottom_left) {
    corner_radius[0] = radius_top_left;
    corner_radius[1] = radius_top_right;
    corner_radius[2] = radius_bottom_right;
    corner_radius[3] = radius_bottom_left;

    emit_changed();
}
int StyleBoxFlat::get_corner_radius_min() const {
    int smallest = corner_radius[0];
    for (int i = 1; i < 4; i++) {
        if (smallest > corner_radius[i]) {
            smallest = corner_radius[i];
        }
    }
    return smallest;
}

void StyleBoxFlat::set_corner_radius(const Corner p_corner, const int radius) {

    ERR_FAIL_INDEX((int)p_corner, 4);
    corner_radius[p_corner] = radius;
    emit_changed();
}
int StyleBoxFlat::get_corner_radius(const Corner p_corner) const {
    ERR_FAIL_INDEX_V((int)p_corner, 4, 0);
    return corner_radius[p_corner];
}

void StyleBoxFlat::set_expand_margin_size(Margin p_expand_margin, float p_size) {

    ERR_FAIL_INDEX((int)p_expand_margin, 4);
    expand_margin[(int)p_expand_margin] = p_size;
    emit_changed();
}

void StyleBoxFlat::set_expand_margin_size_individual(float p_left, float p_top, float p_right, float p_bottom) {
    expand_margin[(int8_t)Margin::Left] = p_left;
    expand_margin[(int8_t)Margin::Top] = p_top;
    expand_margin[(int8_t)Margin::Right] = p_right;
    expand_margin[(int8_t)Margin::Bottom] = p_bottom;
    emit_changed();
}

void StyleBoxFlat::set_expand_margin_size_all(float p_expand_margin_size) {
    for (int i = 0; i < 4; i++) {

        expand_margin[i] = p_expand_margin_size;
    }
    emit_changed();
}

float StyleBoxFlat::get_expand_margin_size(Margin p_expand_margin) const {

    ERR_FAIL_INDEX_V((int)p_expand_margin, 4, 0.0);
    return expand_margin[(int)p_expand_margin];
}
void StyleBoxFlat::set_draw_center(bool p_enabled) {

    draw_center = p_enabled;
    emit_changed();
}
bool StyleBoxFlat::is_draw_center_enabled() const {

    return draw_center;
}

void StyleBoxFlat::set_shadow_color(const Color &p_color) {

    shadow_color = p_color;
    emit_changed();
}
Color StyleBoxFlat::get_shadow_color() const {

    return shadow_color;
}

void StyleBoxFlat::set_shadow_size(const int &p_size) {

    shadow_size = p_size;
    emit_changed();
}
int StyleBoxFlat::get_shadow_size() const {

    return shadow_size;
}

void StyleBoxFlat::set_shadow_offset(const Point2 &p_offset) {

    shadow_offset = p_offset;
    emit_changed();
}
Point2 StyleBoxFlat::get_shadow_offset() const {

    return shadow_offset;
}

void StyleBoxFlat::set_anti_aliased(const bool &p_anti_aliased) {
    anti_aliased = p_anti_aliased;
    emit_changed();
}
bool StyleBoxFlat::is_anti_aliased() const {
    return anti_aliased;
}

void StyleBoxFlat::set_aa_size(const float &p_aa_size) {
    aa_size = CLAMP(p_aa_size, 0.01f, 10.0f);
    emit_changed();
}
float StyleBoxFlat::get_aa_size() const {
    return aa_size;
}

void StyleBoxFlat::set_corner_detail(const int &p_corner_detail) {
    corner_detail = CLAMP(p_corner_detail, 1, 20);
    emit_changed();
}
int StyleBoxFlat::get_corner_detail() const {
    return corner_detail;
}

Size2 StyleBoxFlat::get_center_size() const {

    return Size2();
}

inline void set_inner_corner_radius(const Rect2 style_rect, const Rect2 inner_rect, const float corner_radius[4], float *inner_corner_radius) {
    float border_left = inner_rect.position.x - style_rect.position.x;
    float border_top = inner_rect.position.y - style_rect.position.y;
    float border_right = style_rect.size.width - inner_rect.size.width - border_left;
    float border_bottom = style_rect.size.height - inner_rect.size.height - border_top;

    float rad;
    // Top Left
    rad = MIN(border_top, border_left);
    inner_corner_radius[0] = M_MAX(corner_radius[0] - rad, 0);

    //tr
    rad = MIN(border_top, border_right);
    inner_corner_radius[1] = M_MAX(corner_radius[1] - rad, 0);

    //br
    rad = MIN(border_bottom, border_right);
    inner_corner_radius[2] = M_MAX(corner_radius[2] - rad, 0);

    //bl
    rad = MIN(border_bottom, border_left);
    inner_corner_radius[3] = M_MAX(corner_radius[3] - rad, 0);
}
inline void draw_ring(Vector<Vector2> &verts, Vector<int> &indices, Vector<Color> &colors, const Rect2 &style_rect, const float corner_radius[4],
        Rect2 ring_rect, Rect2 inner_rect, const Color &inner_color, const Color &outer_color, const int corner_detail, const bool fill_center = false) {

    int vert_offset = verts.size();

    float adapted_corner_detail = (corner_radius[0] == 0 && corner_radius[1] == 0 && corner_radius[2] == 0 && corner_radius[3] == 0) ? 1 : corner_detail;

    float ring_corner_radius[4];
    set_inner_corner_radius(style_rect, ring_rect, corner_radius, ring_corner_radius);

    // Corner radius center points.
    const eastl::array<Point2,4> outer_points = {
        Point2(ring_rect.position + Vector2(ring_corner_radius[0], ring_corner_radius[0])), //tl
        Point2(ring_rect.position.x + ring_rect.size.x - ring_corner_radius[1], ring_rect.position.y + ring_corner_radius[1]), //tr
        Point2(ring_rect.position + ring_rect.size - Vector2(ring_corner_radius[2], ring_corner_radius[2])), //br
        Point2(ring_rect.position.x + ring_corner_radius[3], ring_rect.position.y + ring_rect.size.y - ring_corner_radius[3]), //bl
    };

    float inner_corner_radius[4];
    set_inner_corner_radius(style_rect, inner_rect, corner_radius, inner_corner_radius);

    const eastl::array<Point2,4> inner_points = {
        Point2(inner_rect.position + Vector2(inner_corner_radius[0], inner_corner_radius[0])), //tl
        Point2(inner_rect.position.x + inner_rect.size.x - inner_corner_radius[1], inner_rect.position.y + inner_corner_radius[1]), //tr
        Point2(inner_rect.position + inner_rect.size - Vector2(inner_corner_radius[2], inner_corner_radius[2])), //br
        Point2(inner_rect.position.x + inner_corner_radius[3], inner_rect.position.y + inner_rect.size.y - inner_corner_radius[3]) //bl
    };

    //calculate the vert array
    for (int corner_index = 0; corner_index < 4; corner_index++) {
        for (int detail = 0; detail <= adapted_corner_detail; detail++) {
            for (int inner_outer = 0; inner_outer < 2; inner_outer++) {
                float radius;
                Color color;
                Point2 corner_point;
                if (inner_outer == 0) {
                    radius = inner_corner_radius[corner_index];
                    color = inner_color;
                    corner_point = inner_points[corner_index];
                } else {
                    radius = ring_corner_radius[corner_index];
                    color = outer_color;
                    corner_point = outer_points[corner_index];
                }
                float x = radius * (float)cos((float)corner_index * Math_PI / 2.0f + (float)detail / (float)adapted_corner_detail * Math_PI / 2.0f + Math_PI) + corner_point.x;
                float y = radius * (float)sin((float)corner_index * Math_PI / 2.0f + (float)detail / (float)adapted_corner_detail * Math_PI / 2.0f + Math_PI) + corner_point.y;
                verts.emplace_back(x, y);
                colors.emplace_back(color);
            }
        }
    }

    int ring_vert_count = verts.size() - vert_offset;

    // Fill the indices and the colors for the border.
    for (int i = 0; i < ring_vert_count; i++) {
        indices.push_back(vert_offset + ((i + 0) % ring_vert_count));
        indices.push_back(vert_offset + ((i + 2) % ring_vert_count));
        indices.push_back(vert_offset + ((i + 1) % ring_vert_count));
    }

    if (fill_center) {
        // Fill the indices and the colors for the center.
        for (int index = 0; index < ring_vert_count / 2; index += 2) {
            int i = index;
            // Polygon 1.
            indices.push_back(vert_offset + i);
            indices.push_back(vert_offset + ring_vert_count - 4 - i);
            indices.push_back(vert_offset + i + 2);
            // Polygon 2.
            indices.push_back(vert_offset + i);
            indices.push_back(vert_offset + ring_vert_count - 2 - i);
            indices.push_back(vert_offset + ring_vert_count - 4 - i);
        }
    }
}

inline void adapt_values(int p_index_a, int p_index_b, float *adapted_values, const float *p_values, const real_t p_width, const float p_max_a, const float p_max_b) {
    if (p_values[p_index_a] + p_values[p_index_b] > p_width) {

        float factor = (float)p_width / (float)(p_values[p_index_a] + p_values[p_index_b]);
        float newValue = (float)(p_values[p_index_a] * factor);

        if (newValue < adapted_values[p_index_a]) {
            adapted_values[p_index_a] = newValue;
        }
        newValue = (int)(p_values[p_index_b] * factor);
        if (newValue < adapted_values[p_index_b]) {
            adapted_values[p_index_b] = newValue;
        }
    } else {
        adapted_values[p_index_a] = MIN(p_values[p_index_a], adapted_values[p_index_a]);
        adapted_values[p_index_b] = MIN(p_values[p_index_b], adapted_values[p_index_b]);
    }
    adapted_values[p_index_a] = MIN(p_max_a, adapted_values[p_index_a]);
    adapted_values[p_index_b] = MIN(p_max_b, adapted_values[p_index_b]);
}

Rect2 StyleBoxFlat::get_draw_rect(const Rect2 &p_rect) const {
    Rect2 draw_rect = p_rect.grow_individual(expand_margin[(int8_t)Margin::Left], expand_margin[(int8_t)Margin::Top], expand_margin[(int8_t)Margin::Right], expand_margin[(int8_t)Margin::Bottom]);

    if (shadow_size > 0) {
        Rect2 shadow_rect = draw_rect.grow(shadow_size);
        shadow_rect.position += shadow_offset;
        draw_rect = draw_rect.merge(shadow_rect);
    }

    return draw_rect;
}

void StyleBoxFlat::draw(RenderingEntity p_canvas_item, const Rect2 &p_rect) const {

    bool draw_border = (border_width[0] > 0) || (border_width[1] > 0) || (border_width[2] > 0) || (border_width[3] > 0);
    bool draw_shadow = (shadow_size > 0);
    if (!draw_border && !draw_center && !draw_shadow) {
        return;
    }

    Rect2 style_rect = p_rect.grow_individual(expand_margin[(int8_t)Margin::Left], expand_margin[(int8_t)Margin::Top], expand_margin[(int8_t)Margin::Right], expand_margin[(int8_t)Margin::Bottom]);
    if (Math::is_zero_approx(style_rect.size.width) || Math::is_zero_approx(style_rect.size.height)) {
        return;
    }

    bool rounded_corners = (corner_radius[0] > 0) || (corner_radius[1] > 0) || (corner_radius[2] > 0) || (corner_radius[3] > 0);
    bool aa_on = rounded_corners && anti_aliased;

    bool blend_on = blend_border && draw_border;

    Color border_color_alpha = Color(border_color.r, border_color.g, border_color.b, 0);
    Color border_color_blend = (draw_center ? bg_color : border_color_alpha);
    Color border_color_inner = blend_on ? border_color_blend : border_color;

    //adapt borders (prevent weird overlapping/glitchy drawings)
    float width = M_MAX(style_rect.size.width, 0);
    float height = M_MAX(style_rect.size.height, 0);
    float adapted_border[4] = { 1000000.0f, 1000000.0f, 1000000.0f, 1000000.0f };
    adapt_values((int)Margin::Top, (int)Margin::Bottom, adapted_border, border_width, height, height, height);
    adapt_values((int)Margin::Left, (int)Margin::Right, adapted_border, border_width, width, width, width);

    //adapt corners (prevent weird overlapping/glitchy drawings)
    float adapted_corner[4] = { 1000000.0f, 1000000.0f, 1000000.0f, 1000000.0f };
    adapt_values(CORNER_TOP_RIGHT, CORNER_BOTTOM_RIGHT, adapted_corner, corner_radius, height, height - adapted_border[(int8_t)Margin::Bottom], height - adapted_border[(int8_t)Margin::Top]);
    adapt_values(CORNER_TOP_LEFT, CORNER_BOTTOM_LEFT, adapted_corner, corner_radius, height, height - adapted_border[(int8_t)Margin::Bottom], height - adapted_border[(int8_t)Margin::Top]);
    adapt_values(CORNER_TOP_LEFT, CORNER_TOP_RIGHT, adapted_corner, corner_radius, width, width - adapted_border[(int8_t)Margin::Right], width - adapted_border[(int8_t)Margin::Left]);
    adapt_values(CORNER_BOTTOM_LEFT, CORNER_BOTTOM_RIGHT, adapted_corner, corner_radius, width, width - adapted_border[(int8_t)Margin::Right], width - adapted_border[(int8_t)Margin::Left]);

    Rect2 infill_rect = style_rect.grow_individual(-adapted_border[(int8_t)Margin::Left], -adapted_border[(int8_t)Margin::Top], -adapted_border[(int8_t)Margin::Right], -adapted_border[(int8_t)Margin::Bottom]);

    Rect2 border_style_rect = style_rect;
    if (aa_on) {
        for (int i = 0; i < 4; i++) {
            if (border_width[i] > 0) {
                border_style_rect = border_style_rect.grow_margin((Margin)i, -aa_size);
            }
        }
    }
    Vector<Point2> verts;
    Vector<int> indices;
    Vector<Color> colors;
    Vector<Point2> uvs;

    // Create shadow
    if (draw_shadow) {

        Rect2 shadow_inner_rect = style_rect;
        shadow_inner_rect.position += shadow_offset;

        Rect2 shadow_rect = style_rect.grow(shadow_size);
        shadow_rect.position += shadow_offset;

        Color shadow_color_transparent = Color(shadow_color.r, shadow_color.g, shadow_color.b, 0);

        draw_ring(verts, indices, colors, shadow_inner_rect, adapted_corner,
                shadow_rect, shadow_inner_rect, shadow_color, shadow_color_transparent, corner_detail);

        if (draw_center) {
            draw_ring(verts, indices, colors, shadow_inner_rect, adapted_corner,
                    shadow_inner_rect, shadow_inner_rect, shadow_color, shadow_color, corner_detail, true);
        }
    }

    // Create border (no AA).
    if (draw_border && !aa_on) {
        draw_ring(verts, indices, colors, border_style_rect, adapted_corner,
                border_style_rect, infill_rect, border_color_inner, border_color, corner_detail);
    }

    // Create infill (no AA).
    if (draw_center && (!aa_on || blend_on || !draw_border)) {
        draw_ring(verts, indices, colors, border_style_rect, adapted_corner,
                infill_rect, infill_rect, bg_color, bg_color, corner_detail, true);
    }
    if (aa_on) {
        float aa_border_width[4];
        float aa_fill_width[4];
        if (draw_border) {
            for (int i = 0; i < 4; i++) {
                if (border_width[i] > 0) {
                    aa_border_width[i] = aa_size;
                    aa_fill_width[i] = 0;
                } else {
                    aa_border_width[i] = 0;
                    aa_fill_width[i] = aa_size;
                }
            }
        } else {
            for (int i = 0; i < 4; i++) {
                aa_border_width[i] = 0;
                aa_fill_width[i] = aa_size;
            }
        }

        Rect2 infill_inner_rect = infill_rect.grow_individual(-aa_border_width[(int8_t)Margin::Left], -aa_border_width[(int8_t)Margin::Top],
                -aa_border_width[(int8_t)Margin::Right], -aa_border_width[(int8_t)Margin::Bottom]);

        if (draw_center) {
            if (!blend_on && draw_border) {
                Rect2 infill_inner_rect_aa = infill_inner_rect.grow_individual(aa_border_width[(int8_t)Margin::Left], aa_border_width[(int8_t)Margin::Top],
                        aa_border_width[(int8_t)Margin::Right], aa_border_width[(int8_t)Margin::Bottom]);
                // Create infill within AA border.
                draw_ring(verts, indices, colors, border_style_rect, adapted_corner,
                        infill_inner_rect_aa, infill_inner_rect_aa, bg_color, bg_color, corner_detail, true);
            }

            if (!blend_on || !draw_border) {
                Rect2 infill_rect_aa = infill_rect.grow_individual(aa_fill_width[(int8_t)Margin::Left], aa_fill_width[(int8_t)Margin::Top],
                        aa_fill_width[(int8_t)Margin::Right], aa_fill_width[(int8_t)Margin::Bottom]);

                Color alpha_bg = Color(bg_color.r, bg_color.g, bg_color.b, 0);

                // Create infill fake AA gradient.
                draw_ring(verts, indices, colors, style_rect, adapted_corner,
                        infill_rect_aa, infill_rect, bg_color, alpha_bg, corner_detail);
            }
        }

        if (draw_border) {
            const Rect2 infill_rect_aa =
                    infill_rect.grow_individual(aa_border_width[(int8_t)Margin::Left], aa_border_width[(int8_t)Margin::Top],
                            aa_border_width[(int8_t)Margin::Right], aa_border_width[(int8_t)Margin::Bottom]);
            const Rect2 style_rect_aa =
                    style_rect.grow_individual(aa_border_width[(int8_t)Margin::Left], aa_border_width[(int8_t)Margin::Top],
                            aa_border_width[(int8_t)Margin::Right], aa_border_width[(int8_t)Margin::Bottom]);
            const Rect2 border_style_rect_aa = border_style_rect.grow_individual(aa_border_width[(int8_t)Margin::Left],
                    aa_border_width[(int8_t)Margin::Top], aa_border_width[(int8_t)Margin::Right],
                    aa_border_width[(int8_t)Margin::Bottom]);

            // Create border.
            draw_ring(verts, indices, colors, border_style_rect, adapted_corner, border_style_rect_aa,
                    ((blend_on) ? infill_rect : infill_rect_aa), border_color_inner, border_color, corner_detail);
            if (!blend_on) {
                // Create inner border fake AA gradient.
                draw_ring(verts, indices, colors, border_style_rect, adapted_corner, infill_rect_aa, infill_rect,
                        border_color_blend, border_color, corner_detail);
            }

            // Create outer border fake AA gradient.
            draw_ring(verts, indices, colors, border_style_rect, adapted_corner, style_rect_aa, border_style_rect_aa,
                    border_color, border_color_alpha, corner_detail);
        }
    }

    // Compute UV coordinates.
    Rect2 uv_rect = style_rect.grow(aa_on ? aa_size : 0);
    uvs.reserve(verts.size());
    for (int i = 0; i < verts.size(); i++) {
        uvs.emplace_back(
                    (verts[i].x - uv_rect.position.x) / uv_rect.size.width,
                        (verts[i].y - uv_rect.position.y) / uv_rect.size.height
       );
    }
    // Draw stylebox.
    RenderingServer *vs = RenderingServer::get_singleton();
    vs->canvas_item_add_triangle_array(p_canvas_item, indices, verts, colors, uvs);
}

float StyleBoxFlat::get_style_margin(Margin p_margin) const {
    ERR_FAIL_INDEX_V((int)p_margin, 4, 0.0f);
    return border_width[(int)p_margin];
}
void StyleBoxFlat::_bind_methods() {

    SE_BIND_METHOD(StyleBoxFlat,set_bg_color);
    SE_BIND_METHOD(StyleBoxFlat,get_bg_color);

    SE_BIND_METHOD(StyleBoxFlat,set_border_color);
    SE_BIND_METHOD(StyleBoxFlat,get_border_color);

    SE_BIND_METHOD(StyleBoxFlat,set_border_width_all);
    SE_BIND_METHOD(StyleBoxFlat,get_border_width_min);

    SE_BIND_METHOD(StyleBoxFlat,set_border_width);
    SE_BIND_METHOD(StyleBoxFlat,get_border_width);

    SE_BIND_METHOD(StyleBoxFlat,set_border_blend);
    SE_BIND_METHOD(StyleBoxFlat,get_border_blend);

    SE_BIND_METHOD(StyleBoxFlat,set_corner_radius_individual);
    SE_BIND_METHOD(StyleBoxFlat,set_corner_radius_all);

    SE_BIND_METHOD(StyleBoxFlat,set_corner_radius);
    SE_BIND_METHOD(StyleBoxFlat,get_corner_radius);

    MethodBinder::bind_method(D_METHOD("set_expand_margin", {"margin", "size"}), &StyleBoxFlat::set_expand_margin_size);
    MethodBinder::bind_method(D_METHOD("set_expand_margin_all", {"size"}), &StyleBoxFlat::set_expand_margin_size_all);
    MethodBinder::bind_method(D_METHOD("set_expand_margin_individual", {"size_left", "size_top", "size_right", "size_bottom"}), &StyleBoxFlat::set_expand_margin_size_individual);
    MethodBinder::bind_method(D_METHOD("get_expand_margin", {"margin"}), &StyleBoxFlat::get_expand_margin_size);

    SE_BIND_METHOD(StyleBoxFlat,set_draw_center);
    SE_BIND_METHOD(StyleBoxFlat,is_draw_center_enabled);

    SE_BIND_METHOD(StyleBoxFlat,set_shadow_color);
    SE_BIND_METHOD(StyleBoxFlat,get_shadow_color);

    SE_BIND_METHOD(StyleBoxFlat,set_shadow_size);
    SE_BIND_METHOD(StyleBoxFlat,get_shadow_size);

    SE_BIND_METHOD(StyleBoxFlat,set_shadow_offset);
    SE_BIND_METHOD(StyleBoxFlat,get_shadow_offset);

    SE_BIND_METHOD(StyleBoxFlat,set_anti_aliased);
    SE_BIND_METHOD(StyleBoxFlat,is_anti_aliased);

    SE_BIND_METHOD(StyleBoxFlat,set_aa_size);
    SE_BIND_METHOD(StyleBoxFlat,get_aa_size);

    SE_BIND_METHOD(StyleBoxFlat,set_corner_detail);
    SE_BIND_METHOD(StyleBoxFlat,get_corner_detail);

    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "bg_color"), "set_bg_color", "get_bg_color");

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "draw_center"), "set_draw_center", "is_draw_center_enabled");

    ADD_GROUP("Border Width", "border_width_");
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "border_width_left", PropertyHint::Range, "0,1024,1"), "set_border_width", "get_border_width", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "border_width_top", PropertyHint::Range, "0,1024,1"), "set_border_width", "get_border_width", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "border_width_right", PropertyHint::Range, "0,1024,1"), "set_border_width", "get_border_width", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "border_width_bottom", PropertyHint::Range, "0,1024,1"), "set_border_width", "get_border_width", (int)Margin::Bottom);

    ADD_GROUP("Border", "border_");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "border_color"), "set_border_color", "get_border_color");

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "border_blend"), "set_border_blend", "get_border_blend");

    ADD_GROUP("Corner Radius", "corner_radius_");
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "corner_radius_top_left", PropertyHint::Range, "0,1024,1"), "set_corner_radius", "get_corner_radius", CORNER_TOP_LEFT);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "corner_radius_top_right", PropertyHint::Range, "0,1024,1"), "set_corner_radius", "get_corner_radius", CORNER_TOP_RIGHT);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "corner_radius_bottom_right", PropertyHint::Range, "0,1024,1"), "set_corner_radius", "get_corner_radius", CORNER_BOTTOM_RIGHT);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "corner_radius_bottom_left", PropertyHint::Range, "0,1024,1"), "set_corner_radius", "get_corner_radius", CORNER_BOTTOM_LEFT);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "corner_detail", PropertyHint::Range, "1,20,1"), "set_corner_detail", "get_corner_detail");

    ADD_GROUP("Expand Margin", "expand_margin_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "expand_margin_left", PropertyHint::Range, "0,2048,1"), "set_expand_margin", "get_expand_margin", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "expand_margin_right", PropertyHint::Range, "0,2048,1"), "set_expand_margin", "get_expand_margin", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "expand_margin_top", PropertyHint::Range, "0,2048,1"), "set_expand_margin", "get_expand_margin", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "expand_margin_bottom", PropertyHint::Range, "0,2048,1"), "set_expand_margin", "get_expand_margin", (int)Margin::Bottom);

    ADD_GROUP("Shadow", "shadow_");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "shadow_color"), "set_shadow_color", "get_shadow_color");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "shadow_size", PropertyHint::Range, "0,100,1,or_greater"), "set_shadow_size", "get_shadow_size");

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "shadow_offset"), "set_shadow_offset", "get_shadow_offset");

    ADD_GROUP("Anti Aliasing", "anti_aliasing_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "anti_aliasing_enabled"), "set_anti_aliased", "is_anti_aliased");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "anti_aliasing_size", PropertyHint::Range, "0.01,10,0.001"), "set_aa_size", "get_aa_size");
}

StyleBoxFlat::StyleBoxFlat() {

    bg_color = Color(0.6, 0.6, 0.6);
    shadow_color = Color(0, 0, 0, 0.6);
    border_color = Color(0.8, 0.8, 0.8);

    blend_border = false;
    draw_center = true;
    anti_aliased = true;

    shadow_size = 0;
    shadow_offset = Point2(0, 0);
    corner_detail = 8;
    aa_size = 0.625f;

    border_width[0] = 0;
    border_width[1] = 0;
    border_width[2] = 0;
    border_width[3] = 0;

    expand_margin[0] = 0;
    expand_margin[1] = 0;
    expand_margin[2] = 0;
    expand_margin[3] = 0;

    corner_radius[0] = 0;
    corner_radius[1] = 0;
    corner_radius[2] = 0;
    corner_radius[3] = 0;
}
StyleBoxFlat::~StyleBoxFlat() {
}

void StyleBoxLine::set_color(const Color &p_color) {
    color = p_color;
    emit_changed();
}
Color StyleBoxLine::get_color() const {
    return color;
}

void StyleBoxLine::set_thickness(int p_thickness) {
    thickness = p_thickness;
    emit_changed();
}
int StyleBoxLine::get_thickness() const {
    return thickness;
}

void StyleBoxLine::set_vertical(bool p_vertical) {
    vertical = p_vertical;
    emit_changed();
}
bool StyleBoxLine::is_vertical() const {
    return vertical;
}

void StyleBoxLine::set_grow_end(float p_grow_end) {
    grow_end = p_grow_end;
    emit_changed();
}
float StyleBoxLine::get_grow_end() const {
    return grow_end;
}

void StyleBoxLine::set_grow_begin(float p_grow_begin) {
    grow_begin = p_grow_begin;
    emit_changed();
}
float StyleBoxLine::get_grow_begin() const {
    return grow_begin;
}

void StyleBoxLine::_bind_methods() {

    SE_BIND_METHOD(StyleBoxLine,set_color);
    SE_BIND_METHOD(StyleBoxLine,get_color);
    SE_BIND_METHOD(StyleBoxLine,set_thickness);
    SE_BIND_METHOD(StyleBoxLine,get_thickness);
    SE_BIND_METHOD(StyleBoxLine,set_grow_begin);
    SE_BIND_METHOD(StyleBoxLine,get_grow_begin);
    SE_BIND_METHOD(StyleBoxLine,set_grow_end);
    SE_BIND_METHOD(StyleBoxLine,get_grow_end);
    SE_BIND_METHOD(StyleBoxLine,set_vertical);
    SE_BIND_METHOD(StyleBoxLine,is_vertical);

    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "color"), "set_color", "get_color");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "grow_begin", PropertyHint::Range, "-300,300,1"), "set_grow_begin", "get_grow_begin");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "grow_end", PropertyHint::Range, "-300,300,1"), "set_grow_end", "get_grow_end");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "thickness", PropertyHint::Range, "0,10"), "set_thickness", "get_thickness");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "vertical"), "set_vertical", "is_vertical");
}
float StyleBoxLine::get_style_margin(Margin p_margin) const {
    ERR_FAIL_INDEX_V((int)p_margin, 4, 0);

    if (vertical) {
        if (p_margin == Margin::Left || p_margin == Margin::Right) {
            return thickness / 2.0f;
}
    } else if (p_margin == Margin::Top || p_margin == Margin::Bottom) {
        return thickness / 2.0f;
    }

    return 0;
}
Size2 StyleBoxLine::get_center_size() const {
    return Size2();
}

void StyleBoxLine::draw(RenderingEntity p_canvas_item, const Rect2 &p_rect) const {
    RenderingServer *vs = RenderingServer::get_singleton();
    Rect2i r = p_rect;

    if (vertical) {
        r.position.y -= grow_begin;
        r.size.y += (grow_begin + grow_end);
        r.size.x = thickness;
    } else {
        r.position.x -= grow_begin;
        r.size.x += (grow_begin + grow_end);
        r.size.y = thickness;
    }

    vs->canvas_item_add_rect(p_canvas_item, r, color);
}

StyleBoxLine::StyleBoxLine() {
    grow_begin = 1.0;
    grow_end = 1.0;
    thickness = 1;
    color = Color(0.0, 0.0, 0.0);
    vertical = false;
}
StyleBoxLine::~StyleBoxLine() {}
