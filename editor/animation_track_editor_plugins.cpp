/*************************************************************************/
/*  animation_track_editor_plugins.cpp                                   */
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

#include "animation_track_editor_plugins.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/resource/resource_manager.h"
#include "core/translation_helpers.h"
#include "editor/audio_stream_preview.h"
#include "editor_resource_preview.h"
#include "editor_scale.h"
#include "scene/2d/animated_sprite_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/font.h"
#include "servers/audio/audio_stream.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(AnimationTrackEditBool)
IMPL_GDCLASS(AnimationTrackEditColor)
IMPL_GDCLASS(AnimationTrackEditAudio)
IMPL_GDCLASS(AnimationTrackEditSpriteFrame)
IMPL_GDCLASS(AnimationTrackEditSubAnim)
IMPL_GDCLASS(AnimationTrackEditTypeAudio)
IMPL_GDCLASS(AnimationTrackEditTypeAnimation)
IMPL_GDCLASS(AnimationTrackEditVolumeDB)
IMPL_GDCLASS(AnimationTrackEditDefaultPlugin)

/// BOOL ///
int AnimationTrackEditBool::get_key_height() const {

    Ref<Texture> checked = get_theme_icon("checked", "CheckBox");
    return checked->get_height();
}
Rect2 AnimationTrackEditBool::get_key_rect(int p_index, float p_pixels_sec) {

    Ref<Texture> checked = get_theme_icon("checked", "CheckBox");
    return Rect2(-checked->get_width() / 2, 0, checked->get_width(), get_size().height);
}

bool AnimationTrackEditBool::is_key_selectable_by_distance() const {

    return false;
}
void AnimationTrackEditBool::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {

    bool checked = get_animation()->track_get_key_value(get_track(), p_index).as<bool>();
    Ref<Texture> icon = get_theme_icon(checked ? StringName("checked") : StringName("unchecked"), "CheckBox");

    Vector2 ofs(p_x - icon->get_width() / 2, int(get_size().height - icon->get_height()) / 2);

    if (ofs.x + icon->get_width() / 2 < p_clip_left)
        return;

    if (ofs.x + icon->get_width() / 2 > p_clip_right)
        return;

    draw_texture(icon, ofs);

    if (p_selected) {
        Color color = get_theme_color("accent_color", "Editor");
        draw_rect_clipped(Rect2(ofs, icon->get_size()), color, false);
    }
}

/// COLOR ///

int AnimationTrackEditColor::get_key_height() const {

    Ref<Font> font = get_theme_font("font", "Label");
    return font->get_height() * 0.8;
}
Rect2 AnimationTrackEditColor::get_key_rect(int p_index, float p_pixels_sec) {

    Ref<Font> font = get_theme_font("font", "Label");
    int fh = font->get_height() * 0.8;
    return Rect2(-fh / 2, 0, fh, get_size().height);
}

bool AnimationTrackEditColor::is_key_selectable_by_distance() const {

    return false;
}

void AnimationTrackEditColor::draw_key_link(int p_index, float p_pixels_sec, int p_x, int p_next_x, int p_clip_left, int p_clip_right) {

    Ref<Font> font = get_theme_font("font", "Label");
    int fh = font->get_height() * 0.8;
    fh /= 3;

    int x_from = p_x + fh / 2 - 1;
    int x_to = p_next_x - fh / 2 + 1;
    x_from = M_MAX(x_from, p_clip_left);
    x_to = MIN(x_to, p_clip_right);

    int y_from = (get_size().height - fh) / 2;

    if (x_from > p_clip_right || x_to < p_clip_left)
        return;

    Vector<Color> color_samples;
    color_samples.push_back(get_animation()->track_get_key_value(get_track(), p_index).as<Color>());

    if (get_animation()->track_get_type(get_track()) == Animation::TYPE_VALUE) {
        if (get_animation()->track_get_interpolation_type(get_track()) != Animation::INTERPOLATION_NEAREST &&
                (get_animation()->value_track_get_update_mode(get_track()) == Animation::UPDATE_CONTINUOUS ||
                        get_animation()->value_track_get_update_mode(get_track()) == Animation::UPDATE_CAPTURE) &&
                !Math::is_zero_approx(get_animation()->track_get_key_transition(get_track(), p_index))) {
            float start_time = get_animation()->track_get_key_time(get_track(), p_index);
            float end_time = get_animation()->track_get_key_time(get_track(), p_index + 1);

            Color color_next = get_animation()->value_track_interpolate(get_track(), end_time).as<Color>();

            if (!color_samples[0].is_equal_approx(color_next)) {
                color_samples.resize(1 + (x_to - x_from) / 64); // Make a color sample every 64 px.
                for (int i = 1; i < color_samples.size(); i++) {
                    float j = i;
                    color_samples[i] = get_animation()->value_track_interpolate(
                            get_track(),
                            Math::lerp(start_time, end_time, j / color_samples.size())).as<Color>();
    }

            }
            color_samples.emplace_back(color_next);
        } else {
            color_samples.emplace_back(color_samples[0]);
        }
    } else {
        color_samples.emplace_back(get_animation()->track_get_key_value(get_track(), p_index + 1).as<Color>());
    }
    for (int i = 0; i < color_samples.size() - 1; i++) {

    Vector<Vector2> points;
        Vector<Color> colors;

        points.emplace_back(Math::lerp(x_from, x_to, float(i) / (color_samples.size() - 1)), y_from);
        colors.push_back(color_samples[i]);

        points.emplace_back(Math::lerp(x_from, x_to, float(i + 1) / (color_samples.size() - 1)), y_from);
        colors.push_back(color_samples[i + 1]);

        points.emplace_back(Math::lerp(x_from, x_to, float(i + 1) / (color_samples.size() - 1)), y_from + fh);
        colors.push_back(color_samples[i + 1]);

        points.emplace_back(Math::lerp(x_from, x_to, float(i) / (color_samples.size() - 1)), y_from + fh);
        colors.push_back(color_samples[i]);

    draw_primitive(points, colors, PoolVector<Vector2>());
    }
}

void AnimationTrackEditColor::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {

    Color color = get_animation()->track_get_key_value(get_track(), p_index).as<Color>();

    Ref<Font> font = get_theme_font("font", "Label");
    int fh = font->get_height() * 0.8;

    Rect2 rect(Vector2(p_x - fh / 2, int(get_size().height - fh) / 2), Size2(fh, fh));

    const Color k_color1 = Color(0.4f, 0.4f, 0.4f);
    const Color k_color2 = Color(0.6f, 0.6f, 0.6f);
    draw_rect_clipped(Rect2(rect.position, rect.size / 2), k_color1);
    draw_rect_clipped(Rect2(rect.position + rect.size / 2, rect.size / 2), k_color1);
    draw_rect_clipped(Rect2(rect.position + Vector2(rect.size.x / 2, 0), rect.size / 2), k_color2);
    draw_rect_clipped(Rect2(rect.position + Vector2(0, rect.size.y / 2), rect.size / 2), k_color2);
    draw_rect_clipped(rect, color);

    if (p_selected) {
        Color accent = get_theme_color("accent_color", "Editor");
        draw_rect_clipped(rect, accent, false);
    }
}

/// AUDIO ///

void AnimationTrackEditAudio::_preview_changed(GameEntity p_which) {

    Object *object = object_for_entity(id);

    if (!object)
        return;

    Ref<AudioStream> stream(object->call_va("get_stream"));

    if (stream && stream->get_instance_id() == p_which) {
        update();
    }
}

int AnimationTrackEditAudio::get_key_height() const {

    if (!object_for_entity(id)) {
        return AnimationTrackEdit::get_key_height();
    }

    Ref<Font> font = get_theme_font("font", "Label");
    return int(font->get_height() * 1.5);
}
Rect2 AnimationTrackEditAudio::get_key_rect(int p_index, float p_pixels_sec) {

    Object *object = object_for_entity(id);

    if (!object) {
        return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
    }

    Ref<AudioStream> stream(object->call_va("get_stream"));

    if (!stream) {
        return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
    }

    bool play = get_animation()->track_get_key_value(get_track(), p_index).as<bool>();
    if (play) {
        float len = stream->get_length();

        if (len == 0) {

            Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);
            len = preview->get_length();
        }

        if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
            len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
        }

        return Rect2(0, 0, len * p_pixels_sec, get_size().height);
    } else {
        Ref<Font> font = get_theme_font("font", "Label");
        int fh = font->get_height() * 0.8;
        return Rect2(0, 0, fh, get_size().height);
    }
}

bool AnimationTrackEditAudio::is_key_selectable_by_distance() const {

    return false;
}
void AnimationTrackEditAudio::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {

    Object *object = object_for_entity(id);

    if (!object) {
        AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
        return;
    }

    Ref<AudioStream> stream(object->call_va("get_stream"));

    if (not stream) {
        AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
        return;
    }

    bool play = get_animation()->track_get_key_value(get_track(), p_index).as<bool>();
    if (play) {
        float len = stream->get_length();

        Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);

        float preview_len = preview->get_length();

        if (len == 0) {
            len = preview_len;
        }

        int pixel_len = len * p_pixels_sec;

        int pixel_begin = p_x;
        int pixel_end = p_x + pixel_len;

        if (pixel_end < p_clip_left)
            return;

        if (pixel_begin > p_clip_right)
            return;

        int from_x = M_MAX(pixel_begin, p_clip_left);
        int to_x = MIN(pixel_end, p_clip_right);

        if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
            float limit = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
            int limit_x = pixel_begin + limit * p_pixels_sec;
            to_x = MIN(limit_x, to_x);
        }

        if (to_x <= from_x)
            return;

        Ref<Font> font = get_theme_font("font", "Label");
        float fh = int(font->get_height() * 1.5);
        Rect2 rect = Rect2(from_x, (get_size().height - fh) / 2, to_x - from_x, fh);
        draw_rect_filled(rect, Color(0.25, 0.25, 0.25));

        Vector<Vector2> lines;
        lines.reserve((to_x - from_x + 1) * 2);
        preview_len = preview->get_length();

        for (int i = from_x; i < to_x; i++) {

            float ofs = (i - pixel_begin) * preview_len / pixel_len;
            float ofs_n = (i + 1 - pixel_begin) * preview_len / pixel_len;
            float max = preview->get_max(ofs, ofs_n) * 0.5f + 0.5f;
            float min = preview->get_min(ofs, ofs_n) * 0.5f + 0.5f;

            lines.emplace_back(i, rect.position.y + min * rect.size.y);
            lines.emplace_back(i, rect.position.y + max * rect.size.y);
        }

        Vector<Color> color;
        color.push_back(Color(0.75, 0.75, 0.75));

        RenderingServer::get_singleton()->canvas_item_add_multiline(get_canvas_item(), lines, color);

        if (p_selected) {
            Color accent = get_theme_color("accent_color", "Editor");
            draw_rect_stroke(rect, accent);
        }
    } else {
        Ref<Font> font = get_theme_font("font", "Label");
        int fh = font->get_height() * 0.8;
        Rect2 rect(Vector2(p_x, int(get_size().height - fh) / 2), Size2(fh, fh));

        Color color = get_theme_color("font_color", "Label");
        draw_rect_clipped(rect, color);

        if (p_selected) {
            Color accent = get_theme_color("accent_color", "Editor");
            draw_rect_clipped(rect, accent, false);
        }
    }
}

void AnimationTrackEditAudio::set_node(Object *p_object) {

    id = p_object->get_instance_id();
}

void AnimationTrackEditAudio::_bind_methods() {
    MethodBinder::bind_method("_preview_changed", &AnimationTrackEditAudio::_preview_changed);
}

AnimationTrackEditAudio::AnimationTrackEditAudio() {
    AudioStreamPreviewGenerator::get_singleton()->connect("preview_updated",callable_mp(this, &ClassName::_preview_changed));
}

/// SPRITE FRAME / FRAME_COORDS ///

int AnimationTrackEditSpriteFrame::get_key_height() const {

    if (!object_for_entity(id)) {
        return AnimationTrackEdit::get_key_height();
    }

    Ref<Font> font = get_theme_font("font", "Label");
    return int(font->get_height() * 2);
}
Rect2 AnimationTrackEditSpriteFrame::get_key_rect(int p_index, float p_pixels_sec) {

    Object *object = object_for_entity(id);

    if (!object) {
        return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
    }

    Size2 size;

    if (object_cast<Sprite2D>(object) || object_cast<Sprite3D>(object)) {

        Ref<Texture> texture(object->call_va("get_texture"));
        if (not texture) {
            return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
        }

        size = texture->get_size();

        if (object->call_va("is_region").as<bool>()) {
            size = object->call_va("get_region_rect").as<Rect2>().size;
        }

        int hframes = object->call_va("get_hframes").as<int>();
        int vframes = object->call_va("get_vframes").as<int>();

        if (hframes > 1) {
            size.x /= hframes;
        }
        if (vframes > 1) {
            size.y /= vframes;
        }
    } else if (object_cast<AnimatedSprite2D>(object) || object_cast<AnimatedSprite3D>(object)) {

        Ref<SpriteFrames> sf(object->call_va("get_sprite_frames"));
        if (not sf) {
            return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
        }

        List<StringName> animations;
        sf->get_animation_list(&animations);

        int frame = get_animation()->track_get_key_value(get_track(), p_index).as<int>();
        String animation;
        if (animations.size() == 1) {
            animation = animations.front();
        } else {
            // Go through other track to find if animation is set
            String animation_path(get_animation()->track_get_path(get_track()));
            animation_path = StringUtils::replace(animation_path,":frame", ":animation");
            int animation_track = get_animation()->find_track((NodePath)animation_path);
            float track_time = get_animation()->track_get_key_time(get_track(), p_index);
            int animaiton_index = get_animation()->track_find_key(animation_track, track_time);
            animation = get_animation()->track_get_key_value(animation_track, animaiton_index).as<String>();
        }

        Ref<Texture> texture = sf->get_frame(StringName(animation), frame);
        if (not texture) {
            return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
        }

        size = texture->get_size();
    }

    size = size.floor();

    Ref<Font> font = get_theme_font("font", "Label");
    int height = int(font->get_height() * 2);
    int width = height * size.width / size.height;

    return Rect2(0, 0, width, get_size().height);
}

bool AnimationTrackEditSpriteFrame::is_key_selectable_by_distance() const {

    return false;
}
void AnimationTrackEditSpriteFrame::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {

    Object *object = object_for_entity(id);

    if (!object) {
        AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
        return;
    }

    Ref<Texture> texture;
    Rect2 region;

    if (object_cast<Sprite2D>(object) || object_cast<Sprite3D>(object)) {

        texture = refFromVariant<Texture>(object->call_va("get_texture"));
        if (not texture) {
            AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
            return;
        }

        int hframes = object->call_va("get_hframes").as<int>();
        int vframes = object->call_va("get_vframes").as<int>();

        Vector2 coords;
        if (is_coords) {
            coords = get_animation()->track_get_key_value(get_track(), p_index).as<Vector2>();
        } else {
            int frame = get_animation()->track_get_key_value(get_track(), p_index).as<int>();
            coords.x = frame % hframes;
            coords.y = frame / hframes;
        }

        region.size = texture->get_size();

        if (object->call_va("is_region").as<bool>()) {

            region = object->call_va("get_region_rect").as<Rect2>();
        }

        if (hframes > 1) {
            region.size.x /= hframes;
        }
        if (vframes > 1) {
            region.size.y /= vframes;
        }

        region.position.x += region.size.x * coords.x;
        region.position.y += region.size.y * coords.y;

    } else if (object_cast<AnimatedSprite2D>(object) || object_cast<AnimatedSprite3D>(object)) {

        Ref<SpriteFrames> sf(object->call_va("get_sprite_frames"));
        if (not sf) {
            AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
            return;
        }

        List<StringName> animations;
        sf->get_animation_list(&animations);

        int frame = get_animation()->track_get_key_value(get_track(), p_index).as<int>();
        String animation;
        if (animations.size() == 1) {
            animation = animations.front();
        } else {
            // Go through other track to find if animation is set
            String animation_path(get_animation()->track_get_path(get_track()));
            animation_path = StringUtils::replace(animation_path,":frame", ":animation");
            int animation_track = get_animation()->find_track((NodePath)animation_path);
            float track_time = get_animation()->track_get_key_time(get_track(), p_index);
            int animaiton_index = get_animation()->track_find_key(animation_track, track_time);
            animation = get_animation()->track_get_key_value(animation_track, animaiton_index).as<String>();
        }

        texture = sf->get_frame(StringName(animation), frame);
        if (not texture) {
            AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
            return;
        }

        region.size = texture->get_size();
    }

    Ref<Font> font = get_theme_font("font", "Label");
    int height = int(font->get_height() * 2);

    int width = height * region.size.width / region.size.height;

    Rect2 rect(p_x, int(get_size().height - height) / 2, width, height);

    if (rect.position.x + rect.size.x < p_clip_left)
        return;

    if (rect.position.x > p_clip_right)
        return;

    Color accent = get_theme_color("accent_color", "Editor");
    Color bg = accent;
    bg.a = 0.15;

    draw_rect_clipped(rect, bg);

    draw_texture_region_clipped(texture, rect, region);

    if (p_selected) {
        draw_rect_clipped(rect, accent, false);
    }
}

void AnimationTrackEditSpriteFrame::set_node(Object *p_object) {

    id = p_object->get_instance_id();
}
void AnimationTrackEditSpriteFrame::set_as_coords() {

    is_coords = true;
}
/// SUB ANIMATION ///

int AnimationTrackEditSubAnim::get_key_height() const {

    if (!object_for_entity(id)) {
        return AnimationTrackEdit::get_key_height();
    }

    Ref<Font> font = get_theme_font("font", "Label");
    return int(font->get_height() * 1.5);
}
Rect2 AnimationTrackEditSubAnim::get_key_rect(int p_index, float p_pixels_sec) {

    Object *object = object_for_entity(id);

    if (!object) {
        return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
    }

    AnimationPlayer *ap = object_cast<AnimationPlayer>(object);

    if (!ap) {
        return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
    }

    StringName anim = get_animation()->track_get_key_value(get_track(), p_index).as<StringName>();

    if (anim != StringName("[stop]") && ap->has_animation(anim)) {

        float len = ap->get_animation(anim)->get_length();

        if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
            len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
        }

        return Rect2(0, 0, len * p_pixels_sec, get_size().height);
    } else {
        Ref<Font> font = get_theme_font("font", "Label");
        int fh = font->get_height() * 0.8f;
        return Rect2(0, 0, fh, get_size().height);
    }
}

bool AnimationTrackEditSubAnim::is_key_selectable_by_distance() const {

    return false;
}
void AnimationTrackEditSubAnim::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {

    Object *object = object_for_entity(id);

    if (!object) {
        AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
        return;
    }

    AnimationPlayer *ap = object_cast<AnimationPlayer>(object);

    if (!ap) {
        AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
        return;
    }

    StringName anim = get_animation()->track_get_key_value(get_track(), p_index).as<StringName>();

    if (anim != StringName("[stop]") && ap->has_animation(anim)) {

        float len = ap->get_animation(anim)->get_length();

        if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
            len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
        }

        int pixel_len = len * p_pixels_sec;

        int pixel_begin = p_x;
        int pixel_end = p_x + pixel_len;

        if (pixel_end < p_clip_left)
            return;

        if (pixel_begin > p_clip_right)
            return;

        int from_x = M_MAX(pixel_begin, p_clip_left);
        int to_x = MIN(pixel_end, p_clip_right);

        if (to_x <= from_x)
            return;

        Ref<Font> font = get_theme_font("font", "Label");
        int fh = font->get_height() * 1.5f;

        Rect2 rect(from_x, int(get_size().height - fh) / 2, to_x - from_x, fh);

        Color color = get_theme_color("font_color", "Label");
        Color bg = color;
        bg.r = 1 - color.r;
        bg.g = 1 - color.g;
        bg.b = 1 - color.b;
        draw_rect_filled(rect, bg);

        Vector<Vector2> lines;
        Vector<Color> colorv;
        {
            Ref<Animation> animation = ap->get_animation(anim);

            for (int i = 0; i < animation->get_track_count(); i++) {

                float h = (rect.size.height - 2) / animation->get_track_count();

                int y = 2 + h * i + h / 2;

                for (int j = 0; j < animation->track_get_key_count(i); j++) {

                    float ofs = animation->track_get_key_time(i, j);
                    int x = p_x + ofs * p_pixels_sec + 2;

                    if (x < from_x || x >= to_x - 4)
                        continue;

                    lines.push_back(Point2(x, y));
                    lines.push_back(Point2(x + 1, y));
                }
            }

            colorv.push_back(color);
        }

        if (lines.size() > 2) {
            RenderingServer::get_singleton()->canvas_item_add_multiline(get_canvas_item(), lines, colorv);
        }

        int limit = to_x - from_x - 4;
        if (limit > 0) {
            draw_string(font, Point2(from_x + 2, int(get_size().height - font->get_height()) / 2 + font->get_ascent()), anim, color);
        }

        if (p_selected) {
            Color accent = get_theme_color("accent_color", "Editor");
            draw_rect_stroke(rect, accent);
        }
    } else {
        Ref<Font> font = get_theme_font("font", "Label");
        int fh = font->get_height() * 0.8;
        Rect2 rect(Vector2(p_x, int(get_size().height - fh) / 2), Size2(fh, fh));

        Color color = get_theme_color("font_color", "Label");
        draw_rect_clipped(rect, color);

        if (p_selected) {
            Color accent = get_theme_color("accent_color", "Editor");
            draw_rect_clipped(rect, accent, false);
        }
    }
}

void AnimationTrackEditSubAnim::set_node(Object *p_object) {

    id = p_object->get_instance_id();
}

//// VOLUME DB ////

int AnimationTrackEditVolumeDB::get_key_height() const {

    Ref<Texture> volume_texture = get_theme_icon("ColorTrackVu", "EditorIcons");
    return volume_texture->get_height() * 1.2;
}

void AnimationTrackEditVolumeDB::draw_bg(int p_clip_left, int p_clip_right) {

    Ref<Texture> volume_texture = get_theme_icon("ColorTrackVu", "EditorIcons");
    int tex_h = volume_texture->get_height();

    int y_from = (get_size().height - tex_h) / 2;
    int y_size = tex_h;

    Color color(1, 1, 1, 0.3f);
    draw_texture_rect(volume_texture, Rect2(p_clip_left, y_from, p_clip_right - p_clip_left, y_from + y_size), false, color);
}

void AnimationTrackEditVolumeDB::draw_fg(int p_clip_left, int p_clip_right) {

    Ref<Texture> volume_texture = get_theme_icon("ColorTrackVu", "EditorIcons");
    int tex_h = volume_texture->get_height();
    int y_from = (get_size().height - tex_h) / 2;
    int db0 = y_from + 24 / 80.0f * tex_h;

    draw_line(Vector2(p_clip_left, db0), Vector2(p_clip_right, db0), Color(1, 1, 1, 0.3f));
}

void AnimationTrackEditVolumeDB::draw_key_link(int p_index, float p_pixels_sec, int p_x, int p_next_x, int p_clip_left, int p_clip_right) {

    if (p_x > p_clip_right || p_next_x < p_clip_left)
        return;

    float db = get_animation()->track_get_key_value(get_track(), p_index).as<float>();
    float db_n = get_animation()->track_get_key_value(get_track(), p_index + 1).as<float>();

    db = CLAMP(db, -60.0f, 24.0f);
    db_n = CLAMP(db_n, -60.0f, 24.0f);

    float h = 1.0f - (db + 60) / 84.0f;
    float h_n = 1.0f - (db_n + 60) / 84.0f;

    int from_x = p_x;
    int to_x = p_next_x;

    if (from_x < p_clip_left) {
        h = Math::lerp(h, h_n, float(p_clip_left - from_x) / float(to_x - from_x));
        from_x = p_clip_left;
    }

    if (to_x > p_clip_right) {
        h_n = Math::lerp(h, h_n, float(p_clip_right - from_x) / float(to_x - from_x));
        to_x = p_clip_right;
    }

    Ref<Texture> volume_texture = get_theme_icon("ColorTrackVu", "EditorIcons");
    int tex_h = volume_texture->get_height();

    int y_from = (get_size().height - tex_h) / 2;

    Color color = get_theme_color("font_color", "Label");
    color.a *= 0.7f;

    draw_line(Point2(from_x, y_from + h * tex_h), Point2(to_x, y_from + h_n * tex_h), color, 2);
}

////////////////////////

/// AUDIO ///

void AnimationTrackEditTypeAudio::_preview_changed(GameEntity p_which) {

    for (int i = 0; i < get_animation()->track_get_key_count(get_track()); i++) {
        Ref<AudioStream> stream = dynamic_ref_cast<AudioStream>(get_animation()->audio_track_get_key_stream(get_track(), i));
        if (stream && stream->get_instance_id() == p_which) {
            update();
            return;
        }
    }
}

int AnimationTrackEditTypeAudio::get_key_height() const {

    Ref<Font> font = get_theme_font("font", "Label");
    return int(font->get_height() * 1.5);
}
Rect2 AnimationTrackEditTypeAudio::get_key_rect(int p_index, float p_pixels_sec) {

    Ref<AudioStream> stream = dynamic_ref_cast<AudioStream>(get_animation()->audio_track_get_key_stream(get_track(), p_index));

    if (not stream) {
        return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
    }

    float start_ofs = get_animation()->audio_track_get_key_start_offset(get_track(), p_index);
    float end_ofs = get_animation()->audio_track_get_key_end_offset(get_track(), p_index);

    float len = stream->get_length();

    if (len == 0.0f) {

        Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);
        len = preview->get_length();
    }

    len -= end_ofs;
    len -= start_ofs;
    if (len <= 0.001f) {
        len = 0.001f;
    }

    if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
        len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
    }

    return Rect2(0, 0, len * p_pixels_sec, get_size().height);
}

bool AnimationTrackEditTypeAudio::is_key_selectable_by_distance() const {

    return false;
}
void AnimationTrackEditTypeAudio::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {

    Ref<AudioStream> stream = dynamic_ref_cast<AudioStream>(get_animation()->audio_track_get_key_stream(get_track(), p_index));

    if (not stream) {
        AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
        return;
    }

    float start_ofs = get_animation()->audio_track_get_key_start_offset(get_track(), p_index);
    float end_ofs = get_animation()->audio_track_get_key_end_offset(get_track(), p_index);

    if (len_resizing && p_index == len_resizing_index) {
        float ofs_local = -len_resizing_rel / get_timeline()->get_zoom_scale();
        if (len_resizing_start) {
            start_ofs += ofs_local;
            if (start_ofs < 0)
                start_ofs = 0;
        } else {
            end_ofs += ofs_local;
            if (end_ofs < 0)
                end_ofs = 0;
        }
    }

    Ref<Font> font = get_theme_font("font", "Label");
    float fh = int(font->get_height() * 1.5);

    float len = stream->get_length();

    Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);

    float preview_len = preview->get_length();

    if (len == 0) {
        len = preview_len;
    }

    int pixel_total_len = len * p_pixels_sec;

    len -= end_ofs;
    len -= start_ofs;

    if (len <= 0.001f) {
        len = 0.001f;
    }

    int pixel_len = len * p_pixels_sec;

    int pixel_begin = p_x;
    int pixel_end = p_x + pixel_len;

    if (pixel_end < p_clip_left)
        return;

    if (pixel_begin > p_clip_right)
        return;

    int from_x = M_MAX(pixel_begin, p_clip_left);
    int to_x = MIN(pixel_end, p_clip_right);

    if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
        float limit = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
        int limit_x = pixel_begin + limit * p_pixels_sec;
        to_x = MIN(limit_x, to_x);
    }

    if (to_x <= from_x) {
        to_x = from_x + 1;
    }

    int h = get_size().height;
    Rect2 rect = Rect2(from_x, (h - fh) / 2, to_x - from_x, fh);
    draw_rect_filled(rect, Color(0.25, 0.25, 0.25));

    Vector<Vector2> lines;
    lines.reserve((to_x - from_x + 1) * 2);
    preview_len = preview->get_length();

    for (int i = from_x; i < to_x; i++) {

        float ofs = (i - pixel_begin) * preview_len / pixel_total_len;
        float ofs_n = (i + 1 - pixel_begin) * preview_len / pixel_total_len;
        ofs += start_ofs;
        ofs_n += start_ofs;

        float max = preview->get_max(ofs, ofs_n) * 0.5f + 0.5f;
        float min = preview->get_min(ofs, ofs_n) * 0.5f + 0.5f;

        int idx = i - from_x;
        lines.emplace_back(idx, rect.position.y + min * rect.size.y);
        lines.emplace_back(idx, rect.position.y + max * rect.size.y);
    }

    Vector<Color> color;
    color.push_back(Color(0.75, 0.75, 0.75));

    RenderingServer::get_singleton()->canvas_item_add_multiline(get_canvas_item(), lines, color);

    Color cut_color = get_theme_color("accent_color", "Editor");
    cut_color.a = 0.7;
    if (start_ofs > 0 && pixel_begin > p_clip_left) {
        draw_rect_filled(Rect2(pixel_begin, rect.position.y, 1, rect.size.y), cut_color);
    }
    if (end_ofs > 0 && pixel_end < p_clip_right) {
        draw_rect_filled(Rect2(pixel_end, rect.position.y, 1, rect.size.y), cut_color);
    }

    if (p_selected) {
        Color accent = get_theme_color("accent_color", "Editor");
        draw_rect_stroke(rect, accent);
    }
}

void AnimationTrackEditTypeAudio::_bind_methods() {
    MethodBinder::bind_method("_preview_changed", &AnimationTrackEditTypeAudio::_preview_changed);
}

AnimationTrackEditTypeAudio::AnimationTrackEditTypeAudio() {
    AudioStreamPreviewGenerator::get_singleton()->connect("preview_updated",callable_mp(this, &ClassName::_preview_changed));
    len_resizing = false;
}

bool AnimationTrackEditTypeAudio::can_drop_data(const Point2 &p_point, const Variant &p_data) const {

    if (p_point.x > get_timeline()->get_name_limit() && p_point.x < get_size().width - get_timeline()->get_buttons_width()) {

        Dictionary drag_data = p_data.as<Dictionary>();
        if (drag_data.has("type") && drag_data["type"].as<String>() == "resource") {
            Ref<AudioStream> res(drag_data["resource"]);
            if (res) {
                return true;
            }
        }

        if (drag_data.has("type") && drag_data["type"].as<String>() == "files") {

            PoolVector<String> files = drag_data["files"].as<PoolVector<String>>();

            if (files.size() == 1) {
                const String &file = files[0];
                Ref<AudioStream> res = dynamic_ref_cast<AudioStream>(gResourceManager().load(file));
                if (res) {
                    return true;
                }
            }
        }
    }

    return AnimationTrackEdit::can_drop_data(p_point, p_data);
}
void AnimationTrackEditTypeAudio::drop_data(const Point2 &p_point, const Variant &p_data) {

    if (p_point.x > get_timeline()->get_name_limit() && p_point.x < get_size().width - get_timeline()->get_buttons_width()) {

        Ref<AudioStream> stream;
        Dictionary drag_data = p_data.as<Dictionary>();
        if (drag_data.has("type") && drag_data["type"].as<String>() == "resource") {
            stream = refFromVariant<AudioStream>(drag_data["resource"]);
        } else if (drag_data.has("type") && drag_data["type"].as<String>() == "files") {

            PoolVector<String> files = drag_data["files"].as<PoolVector<String>>();

            if (files.size() == 1) {
                const String &file = files[0];
                stream = dynamic_ref_cast<AudioStream>(gResourceManager().load(file));
            }
        }

        if (stream) {

            int x = p_point.x - get_timeline()->get_name_limit();
            float ofs = x / get_timeline()->get_zoom_scale();
            ofs += get_timeline()->get_value();

            ofs = get_editor()->snap_time(ofs);

            while (get_animation()->track_find_key(get_track(), ofs, true) != -1) { //make sure insertion point is valid
                ofs += 0.001;
            }

            get_undo_redo()->create_action(TTR("Add Audio Track Clip"));
            get_undo_redo()->add_do_method(get_animation().get(), "audio_track_insert_key", get_track(), ofs, stream);
            get_undo_redo()->add_undo_method(get_animation().get(), "track_remove_key_at_position", get_track(), ofs);
            get_undo_redo()->commit_action();

            update();
            return;
        }
    }

    AnimationTrackEdit::drop_data(p_point, p_data);
}

void AnimationTrackEditTypeAudio::_gui_input(const Ref<InputEvent> &p_event) {

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);
    if (!len_resizing && mm) {
        bool use_hsize_cursor = false;
        for (int i = 0; i < get_animation()->track_get_key_count(get_track()); i++) {

            Ref<AudioStream> stream = dynamic_ref_cast<AudioStream>(get_animation()->audio_track_get_key_stream(get_track(), i));

            if (not stream) {
                continue;
            }

            float start_ofs = get_animation()->audio_track_get_key_start_offset(get_track(), i);
            float end_ofs = get_animation()->audio_track_get_key_end_offset(get_track(), i);
            float len = stream->get_length();

            if (len == 0) {
                Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);
                float preview_len = preview->get_length();
                len = preview_len;
            }

            len -= end_ofs;
            len -= start_ofs;
            if (len <= 0.001) {
                len = 0.001;
            }

            if (get_animation()->track_get_key_count(get_track()) > i + 1) {
                len = MIN(len, get_animation()->track_get_key_time(get_track(), i + 1) - get_animation()->track_get_key_time(get_track(), i));
            }

            float ofs = get_animation()->track_get_key_time(get_track(), i);

            ofs -= get_timeline()->get_value();
            ofs *= get_timeline()->get_zoom_scale();
            ofs += get_timeline()->get_name_limit();

            int end = ofs + len * get_timeline()->get_zoom_scale();

            if (end >= get_timeline()->get_name_limit() && end <= get_size().width - get_timeline()->get_buttons_width() && ABS(mm->get_position().x - end) < 5 * EDSCALE) {
                use_hsize_cursor = true;
                len_resizing_index = i;
            }
        }

        over_drag_position = use_hsize_cursor;
    }

    if (len_resizing && mm) {
        len_resizing_rel += mm->get_relative().x;
        len_resizing_start = mm->get_shift();
        update();
        accept_event();
        return;
    }

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);
    if (mb && mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT && over_drag_position) {

        len_resizing = true;
        len_resizing_start = mb->get_shift();
        len_resizing_from_px = mb->get_position().x;
        len_resizing_rel = 0;
        update();
        accept_event();
        return;
    }

    if (len_resizing && mb && !mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {

        float ofs_local = -len_resizing_rel / get_timeline()->get_zoom_scale();
        if (len_resizing_start) {
            float prev_ofs = get_animation()->audio_track_get_key_start_offset(get_track(), len_resizing_index);
            get_undo_redo()->create_action(TTR("Change Audio Track Clip Start Offset"));
            get_undo_redo()->add_do_method(get_animation().get(), "audio_track_set_key_start_offset", get_track(), len_resizing_index, prev_ofs + ofs_local);
            get_undo_redo()->add_undo_method(get_animation().get(), "audio_track_set_key_start_offset", get_track(), len_resizing_index, prev_ofs);
            get_undo_redo()->commit_action();

        } else {
            float prev_ofs = get_animation()->audio_track_get_key_end_offset(get_track(), len_resizing_index);
            get_undo_redo()->create_action(TTR("Change Audio Track Clip End Offset"));
            get_undo_redo()->add_do_method(get_animation().get(), "audio_track_set_key_end_offset", get_track(), len_resizing_index, prev_ofs + ofs_local);
            get_undo_redo()->add_undo_method(get_animation().get(), "audio_track_set_key_end_offset", get_track(), len_resizing_index, prev_ofs);
            get_undo_redo()->commit_action();
        }

        len_resizing = false;
        len_resizing_index = -1;
        update();
        accept_event();
        return;
    }

    AnimationTrackEdit::_gui_input(p_event);
}

Control::CursorShape AnimationTrackEditTypeAudio::get_cursor_shape(const Point2 &p_pos) const {
    if (over_drag_position || len_resizing) {
        return Control::CURSOR_HSIZE;
    } else {
        return get_default_cursor_shape();
    }
}
////////////////////
/// SUB ANIMATION ///

int AnimationTrackEditTypeAnimation::get_key_height() const {

    if (!object_for_entity(id)) {
        return AnimationTrackEdit::get_key_height();
    }

    Ref<Font> font = get_theme_font("font", "Label");
    return int(font->get_height() * 1.5);
}
Rect2 AnimationTrackEditTypeAnimation::get_key_rect(int p_index, float p_pixels_sec) {

    Object *object = object_for_entity(id);

    if (!object) {
        return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
    }

    AnimationPlayer *ap = object_cast<AnimationPlayer>(object);

    if (!ap) {
        return AnimationTrackEdit::get_key_rect(p_index, p_pixels_sec);
    }

    StringName anim = get_animation()->animation_track_get_key_animation(get_track(), p_index);

    if (anim != StringName("[stop]") && ap->has_animation(anim)) {

        float len = ap->get_animation(anim)->get_length();

        if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
            len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
        }

        return Rect2(0, 0, len * p_pixels_sec, get_size().height);
    } else {
        Ref<Font> font = get_theme_font("font", "Label");
        int fh = font->get_height() * 0.8f;
        return Rect2(0, 0, fh, get_size().height);
    }
}

bool AnimationTrackEditTypeAnimation::is_key_selectable_by_distance() const {

    return false;
}
void AnimationTrackEditTypeAnimation::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {

    Object *object = object_for_entity(id);

    if (!object) {
        AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
        return;
    }

    AnimationPlayer *ap = object_cast<AnimationPlayer>(object);

    if (!ap) {
        AnimationTrackEdit::draw_key(p_index, p_pixels_sec, p_x, p_selected, p_clip_left, p_clip_right);
        return;
    }

    StringName anim = get_animation()->animation_track_get_key_animation(get_track(), p_index);

    if (anim != StringName("[stop]") && ap->has_animation(anim)) {

        float len = ap->get_animation(anim)->get_length();

        if (get_animation()->track_get_key_count(get_track()) > p_index + 1) {
            len = MIN(len, get_animation()->track_get_key_time(get_track(), p_index + 1) - get_animation()->track_get_key_time(get_track(), p_index));
        }

        int pixel_len = len * p_pixels_sec;

        int pixel_begin = p_x;
        int pixel_end = p_x + pixel_len;

        if (pixel_end < p_clip_left)
            return;

        if (pixel_begin > p_clip_right)
            return;

        int from_x = M_MAX(pixel_begin, p_clip_left);
        int to_x = MIN(pixel_end, p_clip_right);

        if (to_x <= from_x)
            return;

        Ref<Font> font = get_theme_font("font", "Label");
        int fh = font->get_height() * 1.5f;

        Rect2 rect(from_x, int(get_size().height - fh) / 2, to_x - from_x, fh);

        Color color = get_theme_color("font_color", "Label");
        Color bg = color;
        bg.r = 1 - color.r;
        bg.g = 1 - color.g;
        bg.b = 1 - color.b;
        draw_rect_filled(rect, bg);

        Vector<Vector2> lines;
        Vector<Color> colorv;
        {
            Ref<Animation> animation = ap->get_animation(anim);

            for (int i = 0; i < animation->get_track_count(); i++) {

                float h = (rect.size.height - 2) / animation->get_track_count();

                int y = 2 + h * i + h / 2;

                for (int j = 0; j < animation->track_get_key_count(i); j++) {

                    float ofs = animation->track_get_key_time(i, j);
                    int x = p_x + ofs * p_pixels_sec + 2;

                    if (x < from_x || x >= to_x - 4)
                        continue;

                    lines.push_back(Point2(x, y));
                    lines.push_back(Point2(x + 1, y));
                }
            }

            colorv.push_back(color);
        }

        if (lines.size() > 2) {
            RenderingServer::get_singleton()->canvas_item_add_multiline(get_canvas_item(), lines, colorv);
        }

        int limit = to_x - from_x - 4;
        if (limit > 0) {
            draw_string(font, Point2(from_x + 2, int(get_size().height - font->get_height()) / 2 + font->get_ascent()), anim, color);
        }

        if (p_selected) {
            Color accent = get_theme_color("accent_color", "Editor");
            draw_rect_stroke(rect, accent);
        }
    } else {
        Ref<Font> font = get_theme_font("font", "Label");
        int fh = font->get_height() * 0.8f;
        Rect2 rect(Vector2(p_x, int(get_size().height - fh) / 2), Size2(fh, fh));

        Color color = get_theme_color("font_color", "Label");
        draw_rect_clipped(rect, color);

        if (p_selected) {
            Color accent = get_theme_color("accent_color", "Editor");
            draw_rect_clipped(rect, accent, false);
        }
    }
}

void AnimationTrackEditTypeAnimation::set_node(Object *p_object) {

    id = p_object->get_instance_id();
}

AnimationTrackEditTypeAnimation::AnimationTrackEditTypeAnimation() = default;

/////////
AnimationTrackEdit *AnimationTrackEditDefaultPlugin::create_value_track_edit(Object *p_object, VariantType p_type, const StringName &p_property, PropertyHint p_hint, StringView p_hint_string, int p_usage) {

    if (p_property == "playing" && (p_object->is_class("AudioStreamPlayer") || p_object->is_class("AudioStreamPlayer2D") || p_object->is_class("AudioStreamPlayer3D"))) {

        AnimationTrackEditAudio *audio = memnew(AnimationTrackEditAudio);
        audio->set_node(p_object);
        return audio;
    }

    if (p_property == "frame" && (p_object->is_class("Sprite2D") || p_object->is_class("Sprite3D") || p_object->is_class("AnimatedSprite2D") || p_object->is_class("AnimatedSprite3D"))) {

        AnimationTrackEditSpriteFrame *sprite = memnew(AnimationTrackEditSpriteFrame);
        sprite->set_node(p_object);
        return sprite;
    }

    if (p_property == "frame_coords" && (p_object->is_class("Sprite2D") || p_object->is_class("Sprite3D"))) {

        AnimationTrackEditSpriteFrame *sprite = memnew(AnimationTrackEditSpriteFrame);
        sprite->set_as_coords();
        sprite->set_node(p_object);
        return sprite;
    }

    if (p_property == "current_animation" && p_object->is_class("AnimationPlayer")) {

        AnimationTrackEditSubAnim *player = memnew(AnimationTrackEditSubAnim);
        player->set_node(p_object);
        return player;
    }

    if (p_property == "volume_db") {

        AnimationTrackEditVolumeDB *vu = memnew(AnimationTrackEditVolumeDB);
        return vu;
    }

    if (p_type == VariantType::BOOL) {
        return memnew(AnimationTrackEditBool);
    }
    if (p_type == VariantType::COLOR) {
        return memnew(AnimationTrackEditColor);
    }

    return nullptr;
}

AnimationTrackEdit *AnimationTrackEditDefaultPlugin::create_audio_track_edit() {

    return memnew(AnimationTrackEditTypeAudio);
}

AnimationTrackEdit *AnimationTrackEditDefaultPlugin::create_animation_track_edit(Object *p_object) {

    AnimationTrackEditTypeAnimation *an = memnew(AnimationTrackEditTypeAnimation);
    an->set_node(p_object);
    return an;
}
