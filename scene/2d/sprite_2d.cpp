/*************************************************************************/
/*  sprite.cpp                                                           */
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

#include "sprite_2d.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/core_string_names.h"
#include "core/os/os.h"
#include "core/object_tooling.h"
#include "scene/main/viewport.h"
#include "scene/scene_string_names.h"

IMPL_GDCLASS(Sprite2D)
#ifdef TOOLS_ENABLED
Dictionary Sprite2D::_edit_get_state() const {
    Dictionary state = Node2D::_edit_get_state();
    state["offset"] = offset;
    return state;
}

void Sprite2D::_edit_set_state(const Dictionary &p_state) {
    Node2D::_edit_set_state(p_state);
    set_offset(p_state["offset"].as<Vector2>());
}

void Sprite2D::_edit_set_pivot(const Point2 &p_pivot) {
    set_offset(get_offset() - p_pivot);
    set_position(get_transform().xform(p_pivot));
}

Point2 Sprite2D::_edit_get_pivot() const {
    return Vector2();
}

bool Sprite2D::_edit_use_pivot() const {
    return true;
}

Rect2 Sprite2D::_edit_get_rect() const {
    return get_rect();
}

bool Sprite2D::_edit_use_rect() const {
    return texture;
}

bool Sprite2D::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {

    return is_pixel_opaque(p_point);
}

#endif

Rect2 Sprite2D::get_anchorable_rect() const {
    return get_rect();
}

void Sprite2D::_get_rects(Rect2 &r_src_rect, Rect2 &r_dst_rect, bool &r_filter_clip) const {

    Rect2 base_rect;

    if (region) {
        r_filter_clip = region_filter_clip;
        base_rect = region_rect;
    } else {
        r_filter_clip = false;
        base_rect = Rect2(0, 0, texture->get_width(), texture->get_height());
    }

    Size2 frame_size = base_rect.size / Size2(hframes, vframes);
    Point2 frame_offset = Point2(frame % hframes, frame / hframes);
    frame_offset *= frame_size;

    r_src_rect.size = frame_size;
    r_src_rect.position = base_rect.position + frame_offset;

    Point2 dest_offset = offset;
    if (centered)
        dest_offset -= frame_size / 2;
    if (Engine::get_singleton()->get_use_gpu_pixel_snap()) {
        dest_offset = dest_offset.floor();
    }

    r_dst_rect = Rect2(dest_offset, frame_size);

    if (hflip)
        r_dst_rect.size.x = -r_dst_rect.size.x;
    if (vflip)
        r_dst_rect.size.y = -r_dst_rect.size.y;
}

void Sprite2D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_DRAW: {

            if (not texture)
                return;

            RenderingEntity ci = get_canvas_item();

            /*
            texture->draw(ci,Point2());
            break;
            */

            Rect2 src_rect, dst_rect;
            bool filter_clip;
            _get_rects(src_rect, dst_rect, filter_clip);
            texture->draw_rect_region(ci, dst_rect, src_rect, Color(1, 1, 1), false, normal_map, filter_clip);

        } break;
    }
}

void Sprite2D::set_texture(const Ref<Texture> &p_texture) {

    if (p_texture == texture)
        return;

    if (texture)
        texture->disconnect(CoreStringNames::get_singleton()->changed, callable_mp(this, &Sprite2D::_texture_changed));

    texture = p_texture;

    if (texture)
        texture->connect(CoreStringNames::get_singleton()->changed, callable_mp(this, &Sprite2D::_texture_changed));

    update();
    emit_signal("texture_changed");
    item_rect_changed();
    Object_change_notify(this,"texture");
}

void Sprite2D::set_normal_map(const Ref<Texture> &p_texture) {

    normal_map = p_texture;
    update();
}

Ref<Texture> Sprite2D::get_normal_map() const {

    return normal_map;
}

Ref<Texture> Sprite2D::get_texture() const {

    return texture;
}

void Sprite2D::set_centered(bool p_center) {

    centered = p_center;
    update();
    item_rect_changed();
}

bool Sprite2D::is_centered() const {

    return centered;
}

void Sprite2D::set_offset(const Point2 &p_offset) {

    offset = p_offset;
    update();
    item_rect_changed();
    Object_change_notify(this,"offset");
}
Point2 Sprite2D::get_offset() const {

    return offset;
}

void Sprite2D::set_flip_h(bool p_flip) {

    hflip = p_flip;
    update();
}
bool Sprite2D::is_flipped_h() const {

    return hflip;
}

void Sprite2D::set_flip_v(bool p_flip) {

    vflip = p_flip;
    update();
}
bool Sprite2D::is_flipped_v() const {

    return vflip;
}

void Sprite2D::set_region(bool p_region) {

    if (p_region == region)
        return;

    region = p_region;
    update();
}

bool Sprite2D::is_region() const {

    return region;
}

void Sprite2D::set_region_rect(const Rect2 &p_region_rect) {

    if (region_rect == p_region_rect)
        return;

    region_rect = p_region_rect;

    if (region)
        item_rect_changed();

    Object_change_notify(this,"region_rect");
}

Rect2 Sprite2D::get_region_rect() const {

    return region_rect;
}

void Sprite2D::set_region_filter_clip(bool p_enable) {
    region_filter_clip = p_enable;
    update();
}

bool Sprite2D::is_region_filter_clip_enabled() const {
    return region_filter_clip;
}

void Sprite2D::set_frame(int p_frame) {

    ERR_FAIL_INDEX(p_frame, vframes * hframes);

    if (frame != p_frame)
        item_rect_changed();

    frame = p_frame;

    Object_change_notify(this,"frame");
    Object_change_notify(this,"frame_coords");
    emit_signal(SceneStringNames::frame_changed);
}

int Sprite2D::get_frame() const {

    return frame;
}

void Sprite2D::set_frame_coords(const Vector2 &p_coord) {
    ERR_FAIL_INDEX(int(p_coord.x), hframes);
    ERR_FAIL_INDEX(int(p_coord.y), vframes);

    set_frame(int(p_coord.y) * hframes + int(p_coord.x));
}

Vector2 Sprite2D::get_frame_coords() const {
    return Vector2(frame % hframes, frame / hframes);
}

void Sprite2D::set_vframes(int p_amount) {

    ERR_FAIL_COND_MSG(p_amount < 1, "Amount of vframes cannot be smaller than 1.");
    vframes = p_amount;
    update();
    item_rect_changed();
    Object_change_notify(this);
}
int Sprite2D::get_vframes() const {

    return vframes;
}

void Sprite2D::set_hframes(int p_amount) {

    ERR_FAIL_COND_MSG(p_amount < 1, "Amount of hframes cannot be smaller than 1.");
    hframes = p_amount;
    update();
    item_rect_changed();
    Object_change_notify(this);
}
int Sprite2D::get_hframes() const {

    return hframes;
}

bool Sprite2D::is_pixel_opaque(const Point2 &p_point) const {

    if (not texture)
        return false;

    if (texture->get_size().width == 0 || texture->get_size().height == 0)
        return false;

    Rect2 src_rect, dst_rect;
    bool filter_clip;
    _get_rects(src_rect, dst_rect, filter_clip);
    dst_rect.size = dst_rect.size.abs();

    if (!dst_rect.has_point(p_point))
        return false;

    Vector2 q = (p_point - dst_rect.position) / dst_rect.size;
    if (hflip)
        q.x = 1.0f - q.x;
    if (vflip)
        q.y = 1.0f - q.y;
    q = q * src_rect.size + src_rect.position;

    bool is_repeat = texture->get_flags() & Texture::FLAG_REPEAT;
    bool is_mirrored_repeat = texture->get_flags() & Texture::FLAG_MIRRORED_REPEAT;
    if (is_repeat) {
        int mirror_x = 0;
        int mirror_y = 0;
        if (is_mirrored_repeat) {
            mirror_x = (int)(q.x / texture->get_size().width);
            mirror_y = (int)(q.y / texture->get_size().height);
        }
        q.x = Math::fmod(q.x, texture->get_size().width);
        q.y = Math::fmod(q.y, texture->get_size().height);
        if (mirror_x % 2 == 1) {
            q.x = texture->get_size().width - q.x - 1;
        }
        if (mirror_y % 2 == 1) {
            q.y = texture->get_size().height - q.y - 1;
        }
    } else {
        q.x = MIN(q.x, texture->get_size().width - 1);
        q.y = MIN(q.y, texture->get_size().height - 1);
    }

    return texture->is_pixel_opaque((int)q.x, (int)q.y);
}

Rect2 Sprite2D::get_rect() const {

    if (not texture)
        return Rect2(0, 0, 1, 1);

    Size2i s;

    if (region) {
        s = region_rect.size;
    } else {
        s = texture->get_size();
    }

    s = s / Point2(hframes, vframes);

    Point2 ofs = offset;
    if (centered)
        ofs -= Size2(s) / 2;

    if (Engine::get_singleton()->get_use_gpu_pixel_snap()) {
        ofs = ofs.floor();
    }

    if (s == Size2(0, 0))
        s = Size2(1, 1);

    return Rect2(ofs, s);
}

void Sprite2D::_validate_property(PropertyInfo &property) const {

    if (property.name == "frame") {
        property.hint = PropertyHint::Range;
        property.hint_string = "0," + itos(vframes * hframes - 1) + ",1";
        property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
    }
    if (property.name == "frame_coords") {
        property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
    }
}

void Sprite2D::_texture_changed() {

    // Changes to the texture need to trigger an update to make
    // the editor redraw the sprite with the updated texture.
    if (texture) {
        update();
    }
}

void Sprite2D::_bind_methods() {

    SE_BIND_METHOD(Sprite2D,set_texture);
    SE_BIND_METHOD(Sprite2D,get_texture);

    SE_BIND_METHOD(Sprite2D,set_normal_map);
    SE_BIND_METHOD(Sprite2D,get_normal_map);

    SE_BIND_METHOD(Sprite2D,set_centered);
    SE_BIND_METHOD(Sprite2D,is_centered);

    SE_BIND_METHOD(Sprite2D,set_offset);
    SE_BIND_METHOD(Sprite2D,get_offset);

    SE_BIND_METHOD(Sprite2D,set_flip_h);
    SE_BIND_METHOD(Sprite2D,is_flipped_h);

    SE_BIND_METHOD(Sprite2D,set_flip_v);
    SE_BIND_METHOD(Sprite2D,is_flipped_v);

    SE_BIND_METHOD(Sprite2D,set_region);
    SE_BIND_METHOD(Sprite2D,is_region);

    SE_BIND_METHOD(Sprite2D,is_pixel_opaque);

    SE_BIND_METHOD(Sprite2D,set_region_rect);
    SE_BIND_METHOD(Sprite2D,get_region_rect);

    SE_BIND_METHOD(Sprite2D,set_region_filter_clip);
    SE_BIND_METHOD(Sprite2D,is_region_filter_clip_enabled);

    SE_BIND_METHOD(Sprite2D,set_frame);
    SE_BIND_METHOD(Sprite2D,get_frame);

    SE_BIND_METHOD(Sprite2D,set_frame_coords);
    SE_BIND_METHOD(Sprite2D,get_frame_coords);

    SE_BIND_METHOD(Sprite2D,set_vframes);
    SE_BIND_METHOD(Sprite2D,get_vframes);

    SE_BIND_METHOD(Sprite2D,set_hframes);
    SE_BIND_METHOD(Sprite2D,get_hframes);

    SE_BIND_METHOD(Sprite2D,get_rect);

    SE_BIND_METHOD(Sprite2D,_texture_changed);

    ADD_SIGNAL(MethodInfo("frame_changed"));
    ADD_SIGNAL(MethodInfo("texture_changed"));

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "normal_map", PropertyHint::ResourceType, "Texture"), "set_normal_map", "get_normal_map");
    ADD_GROUP("Offset", "");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "centered"), "set_centered", "is_centered");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "offset"), "set_offset", "get_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "flip_h"), "set_flip_h", "is_flipped_h");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "flip_v"), "set_flip_v", "is_flipped_v");
    ADD_GROUP("Animation", "");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "hframes", PropertyHint::Range, "1,16384,1"), "set_hframes", "get_hframes");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "vframes", PropertyHint::Range, "1,16384,1"), "set_vframes", "get_vframes");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "frame"), "set_frame", "get_frame");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "frame_coords", PropertyHint::None, "", PROPERTY_USAGE_EDITOR), "set_frame_coords", "get_frame_coords");

    ADD_GROUP("Region", "region_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "region_enabled"), "set_region", "is_region");
    ADD_PROPERTY(PropertyInfo(VariantType::RECT2, "region_rect"), "set_region_rect", "get_region_rect");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "region_filter_clip"), "set_region_filter_clip", "is_region_filter_clip_enabled");
}

Sprite2D::Sprite2D() {

    centered = true;
    hflip = false;
    vflip = false;
    region = false;
    region_filter_clip = false;

    frame = 0;

    vframes = 1;
    hframes = 1;
}

Sprite2D::~Sprite2D() = default;
