/*************************************************************************/
/*  nine_patch_rect.cpp                                                  */
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

#include "nine_patch_rect.h"

#include "scene/resources/texture.h"
#include "servers/rendering_server.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"

IMPL_GDCLASS(NinePatchRect)

VARIANT_ENUM_CAST(NinePatchRect::AxisStretchMode);

void NinePatchRect::_notification(int p_what) {

    if (p_what == NOTIFICATION_DRAW) {

        if (not texture)
            return;

        Rect2 rect = Rect2(Point2(), get_size());
        Rect2 src_rect = region_rect;

        texture->get_rect_region(rect, src_rect, rect, src_rect);

        RenderingEntity ci = get_canvas_item();
        RenderingServer::get_singleton()->canvas_item_add_nine_patch(ci, rect, src_rect, texture->get_rid(), Vector2(margin[(int8_t)Margin::Left], margin[(int8_t)Margin::Top]), Vector2(margin[(int8_t)Margin::Right], margin[(int8_t)Margin::Bottom]), RS::NinePatchAxisMode(axis_h), RS::NinePatchAxisMode(axis_v), draw_center);
    }
}

Size2 NinePatchRect::get_minimum_size() const {

    return Size2(margin[(int8_t)Margin::Left] + margin[(int8_t)Margin::Right], margin[(int8_t)Margin::Top] + margin[(int8_t)Margin::Bottom]);
}
void NinePatchRect::_bind_methods() {

    SE_BIND_METHOD(NinePatchRect,set_texture);
    SE_BIND_METHOD(NinePatchRect,get_texture);
    SE_BIND_METHOD(NinePatchRect,set_patch_margin);
    SE_BIND_METHOD(NinePatchRect,get_patch_margin);
    SE_BIND_METHOD(NinePatchRect,set_region_rect);
    SE_BIND_METHOD(NinePatchRect,get_region_rect);
    SE_BIND_METHOD(NinePatchRect,set_draw_center);
    SE_BIND_METHOD(NinePatchRect,is_draw_center_enabled);
    SE_BIND_METHOD(NinePatchRect,set_h_axis_stretch_mode);
    SE_BIND_METHOD(NinePatchRect,get_h_axis_stretch_mode);
    SE_BIND_METHOD(NinePatchRect,set_v_axis_stretch_mode);
    SE_BIND_METHOD(NinePatchRect,get_v_axis_stretch_mode);

    ADD_SIGNAL(MethodInfo("texture_changed"));

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "draw_center"), "set_draw_center", "is_draw_center_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::RECT2, "region_rect"), "set_region_rect", "get_region_rect");

    ADD_GROUP("Patch Margin", "patch_margin_");
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "patch_margin_left", PropertyHint::Range, "0,16384,1"), "set_patch_margin", "get_patch_margin", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "patch_margin_top", PropertyHint::Range, "0,16384,1"), "set_patch_margin", "get_patch_margin", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "patch_margin_right", PropertyHint::Range, "0,16384,1"), "set_patch_margin", "get_patch_margin", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "patch_margin_bottom", PropertyHint::Range, "0,16384,1"), "set_patch_margin", "get_patch_margin", (int)Margin::Bottom);

    ADD_GROUP("Axis Stretch", "axis_stretch_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "axis_stretch_horizontal", PropertyHint::Enum, "Stretch,Tile,Tile Fit"), "set_h_axis_stretch_mode", "get_h_axis_stretch_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "axis_stretch_vertical", PropertyHint::Enum, "Stretch,Tile,Tile Fit"), "set_v_axis_stretch_mode", "get_v_axis_stretch_mode");

    BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_STRETCH);
    BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_TILE);
    BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_TILE_FIT);
}

void NinePatchRect::set_texture(const Ref<Texture> &p_tex) {

    if (texture == p_tex)
        return;
    texture = p_tex;
    update();
    /*
    if (texture.is_valid())
        texture->set_flags(texture->get_flags()&(~Texture::FLAG_REPEAT)); //remove repeat from texture, it looks bad in sprites
    */
    minimum_size_changed();
    emit_signal("texture_changed");
    Object_change_notify(this,"texture");
}

Ref<Texture> NinePatchRect::get_texture() const {

    return texture;
}

void NinePatchRect::set_patch_margin(Margin p_margin, int p_size) {

    ERR_FAIL_INDEX((int)p_margin, 4);
    margin[(int)p_margin] = p_size;
    update();
    minimum_size_changed();
    switch (p_margin) {
        case Margin::Left:
            Object_change_notify(this,"patch_margin_left");
            break;
        case Margin::Top:
            Object_change_notify(this,"patch_margin_top");
            break;
        case Margin::Right:
            Object_change_notify(this,"patch_margin_right");
            break;
        case Margin::Bottom:
            Object_change_notify(this,"patch_margin_bottom");
            break;
    }
}

int NinePatchRect::get_patch_margin(Margin p_margin) const {

    ERR_FAIL_INDEX_V((int)p_margin, 4, 0);
    return margin[(int)p_margin];
}

void NinePatchRect::set_region_rect(const Rect2 &p_region_rect) {

    if (region_rect == p_region_rect)
        return;

    region_rect = p_region_rect;

    item_rect_changed();
    Object_change_notify(this,"region_rect");
}

Rect2 NinePatchRect::get_region_rect() const {

    return region_rect;
}

void NinePatchRect::set_draw_center(bool p_enabled) {

    draw_center = p_enabled;
    update();
}

bool NinePatchRect::is_draw_center_enabled() const {

    return draw_center;
}

void NinePatchRect::set_h_axis_stretch_mode(AxisStretchMode p_mode) {
    axis_h = p_mode;
    update();
}

NinePatchRect::AxisStretchMode NinePatchRect::get_h_axis_stretch_mode() const {
    return axis_h;
}

void NinePatchRect::set_v_axis_stretch_mode(AxisStretchMode p_mode) {

    axis_v = p_mode;
    update();
}

NinePatchRect::AxisStretchMode NinePatchRect::get_v_axis_stretch_mode() const {

    return axis_v;
}

NinePatchRect::NinePatchRect() {

    margin[(int8_t)Margin::Left] = 0;
    margin[(int8_t)Margin::Right] = 0;
    margin[(int8_t)Margin::Bottom] = 0;
    margin[(int8_t)Margin::Top] = 0;

    set_mouse_filter(MOUSE_FILTER_IGNORE);
    draw_center = true;

    axis_h = AXIS_STRETCH_MODE_STRETCH;
    axis_v = AXIS_STRETCH_MODE_STRETCH;
}

NinePatchRect::~NinePatchRect() {
}
