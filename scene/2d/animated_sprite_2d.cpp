/*************************************************************************/
/*  animated_sprite_2d.cpp                                               */
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

#include "animated_sprite_2d.h"

#include "core/os/os.h"
#include "core/callable_method_pointer.h"
#include "scene/scene_string_names.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/translation_helpers.h"
#include "core/string_formatter.h"
#include "EASTL/sort.h"

IMPL_GDCLASS(SpriteFrames)
IMPL_GDCLASS(AnimatedSprite2D)

#define NORMAL_SUFFIX "_normal"
#define SPECULAR_SUFFIX "_specular"

#ifdef TOOLS_ENABLED

Dictionary AnimatedSprite2D::_edit_get_state() const {
    Dictionary state = Node2D::_edit_get_state();
    state["offset"] = offset;
    return state;
}

void AnimatedSprite2D::_edit_set_state(const Dictionary &p_state) {
    Node2D::_edit_set_state(p_state);
    set_offset(p_state["offset"].as<Vector2>());
}

void AnimatedSprite2D::_edit_set_pivot(const Point2 &p_pivot) {
    set_offset(get_offset() - p_pivot);
    set_position(get_transform().xform(p_pivot));
}

Point2 AnimatedSprite2D::_edit_get_pivot() const {
    return Vector2();
}

bool AnimatedSprite2D::_edit_use_pivot() const {
    return true;
}

Rect2 AnimatedSprite2D::_edit_get_rect() const {
    return _get_rect();
}

bool AnimatedSprite2D::_edit_use_rect() const {
    if (not frames || !frames->has_animation(animation) || frame < 0 || frame >= frames->get_frame_count(animation)) {
        return false;
    }
    Ref<Texture> t;
    if (animation)
        t = frames->get_frame(animation, frame);
    return t;
}
#endif

Rect2 AnimatedSprite2D::get_anchorable_rect() const {
    return _get_rect();
}

Rect2 AnimatedSprite2D::_get_rect() const {
    if (not frames || !frames->has_animation(animation) || frame < 0 || frame >= frames->get_frame_count(animation)) {
        return Rect2();
    }

    Ref<Texture> t;
    if (animation)
        t = frames->get_frame(animation, frame);
    if (not t)
        return Rect2();
    Size2 s = t->get_size();

    Point2 ofs = offset;
    if (centered)
        ofs -= s / 2;

    if (s == Size2(0, 0))
        s = Size2(1, 1);

    return Rect2(ofs, s);
}

void SpriteFrames::add_frame(const StringName &p_anim, const Ref<Texture> &p_frame, int p_at_pos) {

    HashMap<StringName, Anim>::iterator E = animations.find(p_anim);
    ERR_FAIL_COND_MSG(E==animations.end(), "Animation '" + String(p_anim) + "' doesn't exist.");

    if (p_at_pos >= 0 && p_at_pos < E->second.frames.size())
        E->second.frames.insert(p_at_pos, p_frame);
    else
        E->second.frames.push_back(p_frame);

    emit_changed();
}

int SpriteFrames::get_frame_count(const StringName &p_anim) const {
    const HashMap<StringName, Anim>::const_iterator E = animations.find(p_anim);
    ERR_FAIL_COND_V_MSG(E==animations.end(), 0, "Animation '" + String(p_anim) + "' doesn't exist.");

    return E->second.frames.size();
}

void SpriteFrames::remove_frame(const StringName &p_anim, int p_idx) {

    HashMap<StringName, Anim>::iterator E = animations.find(p_anim);
    ERR_FAIL_COND_MSG(E==animations.end(), "Animation '" + String(p_anim) + "' doesn't exist.");

    E->second.frames.remove(p_idx);
    emit_changed();
}
void SpriteFrames::clear(const StringName &p_anim) {

    HashMap<StringName, Anim>::iterator E = animations.find(p_anim);
    ERR_FAIL_COND_MSG(E==animations.end(), "Animation '" + String(p_anim) + "' doesn't exist.");

    E->second.frames.clear();
    emit_changed();
}

void SpriteFrames::clear_all() {

    animations.clear();
    add_animation("default");
}

void SpriteFrames::add_animation(const StringName &p_anim) {

    ERR_FAIL_COND_MSG(animations.contains(p_anim), "SpriteFrames already has animation '" + String(p_anim) + "'.");

    animations[p_anim] = Anim();
    animations[p_anim].normal_name = StringName(String(p_anim) + NORMAL_SUFFIX);
}

bool SpriteFrames::has_animation(const StringName &p_anim) const {

    return animations.contains(p_anim);
}
void SpriteFrames::remove_animation(const StringName &p_anim) {

    animations.erase(p_anim);
}

void SpriteFrames::rename_animation(const StringName &p_prev, const StringName &p_next) {

    ERR_FAIL_COND_MSG(!animations.contains(p_prev), "SpriteFrames doesn't have animation '" + String(p_prev) + "'.");
    ERR_FAIL_COND_MSG(animations.contains(p_next), "Animation '" + String(p_next) + "' already exists.");

    Anim anim = animations[p_prev];
    animations.erase(p_prev);
    animations[p_next] = anim;
    animations[p_next].normal_name = StringName(String(p_next) + NORMAL_SUFFIX);
}

PoolVector<String> SpriteFrames::_get_animation_list() const {

    PoolVector<String> ret;
    List<StringName> al;
    get_animation_list(&al);
    for (const StringName &E : al) {

        ret.push_back(String(E));
    }

    return ret;
}

void SpriteFrames::report_missing_animation(const char *name)
{
    String msg="Animation '" + String(name) + "' doesn't exist.";
    ERR_PRINT(msg);
}

void SpriteFrames::get_animation_list(List<StringName> *r_animations) const {

    for (const eastl::pair<const StringName,Anim> &E : animations) {
        r_animations->push_back(E.first);
    }
}

PoolVector<String> SpriteFrames::get_animation_names() const {

    PoolVector<String> names;
    Vector<StringName> name_keys;
    animations.keys_into(name_keys);
    //TODO: SEGS: returned keys should be sorted already ??
    eastl::sort(name_keys.begin(),name_keys.end());
    for (const StringName &k : name_keys) {
        names.push_back(String(k));
    }
    return names;
}

void SpriteFrames::set_animation_speed(const StringName &p_anim, float p_fps) {

    ERR_FAIL_COND_MSG(p_fps < 0, "Animation speed cannot be negative (" + itos(p_fps) + ").");
    HashMap<StringName, Anim>::iterator E = animations.find(p_anim);
    ERR_FAIL_COND_MSG(E==animations.end(), "Animation '" + String(p_anim) + "' doesn't exist.");
    E->second.speed = p_fps;
}
float SpriteFrames::get_animation_speed(const StringName &p_anim) const {

    HashMap<StringName, Anim>::const_iterator E = animations.find(p_anim);
    ERR_FAIL_COND_V_MSG(E==animations.end(),0, "Animation '" + String(p_anim) + "' doesn't exist.");
    return E->second.speed;
}

void SpriteFrames::set_animation_loop(const StringName &p_anim, bool p_loop) {
    HashMap<StringName, Anim>::iterator E = animations.find(p_anim);
    ERR_FAIL_COND_MSG(E==animations.end(), "Animation '" + String(p_anim) + "' doesn't exist.");
    E->second.loop = p_loop;
}
bool SpriteFrames::get_animation_loop(const StringName &p_anim) const {
    HashMap<StringName, Anim>::const_iterator E = animations.find(p_anim);
    ERR_FAIL_COND_V_MSG(E==animations.end(),false, "Animation '" + String(p_anim) + "' doesn't exist.");
    return E->second.loop;
}

void SpriteFrames::_set_frames(const Array &p_frames) {

    clear_all();
    HashMap<StringName, Anim>::iterator E = animations.find(SceneStringNames::_default);
    ERR_FAIL_COND(E==animations.end());

    E->second.frames.resize(p_frames.size());
    auto wr(E->second.frames.write());
    for (int i = 0; i < E->second.frames.size(); i++)
        wr[i] = refFromVariant<Texture>(p_frames[i]);
}
Array SpriteFrames::_get_frames() const {

    return Array();
}

Array SpriteFrames::_get_animations() const {

    Array anims;
    for (const eastl::pair<const StringName,Anim> &E : animations) {
        Dictionary d;
        d["name"] = E.first;
        d["speed"] = E.second.speed;
        d["loop"] = E.second.loop;
        Array frames;
        for (int i = 0; i < E.second.frames.size(); i++) {
            frames.push_back(E.second.frames[i]);
        }
        d["frames"] = frames;
        anims.push_back(d);
    }

    return anims;
}
void SpriteFrames::_set_animations(const Array &p_animations) {

    animations.clear();
    for (int i = 0; i < p_animations.size(); i++) {

        Dictionary d = p_animations[i].as<Dictionary>();

        ERR_CONTINUE(!d.has("name"));
        ERR_CONTINUE(!d.has("speed"));
        ERR_CONTINUE(!d.has("loop"));
        ERR_CONTINUE(!d.has("frames"));

        Anim anim;
        anim.speed = d["speed"].as<float>();
        anim.loop = d["loop"].as<bool>();
        Array frames = d["frames"].as<Array>();
        for (int j = 0; j < frames.size(); j++) {

            Ref<Texture> res(refFromVariant<Texture>(frames[j]));
            anim.frames.push_back(res);
        }

        animations[d["name"].as<StringName>()] = anim;
    }
}

void SpriteFrames::_bind_methods() {

    SE_BIND_METHOD(SpriteFrames,add_animation);
    SE_BIND_METHOD(SpriteFrames,has_animation);
    SE_BIND_METHOD(SpriteFrames,remove_animation);
    SE_BIND_METHOD(SpriteFrames,rename_animation);

    SE_BIND_METHOD(SpriteFrames,get_animation_names);

    SE_BIND_METHOD(SpriteFrames,set_animation_speed);
    SE_BIND_METHOD(SpriteFrames,get_animation_speed);

    SE_BIND_METHOD(SpriteFrames,set_animation_loop);
    SE_BIND_METHOD(SpriteFrames,get_animation_loop);

    MethodBinder::bind_method(D_METHOD("add_frame", {"anim", "frame", "at_position"}), &SpriteFrames::add_frame, {DEFVAL(-1)});
    SE_BIND_METHOD(SpriteFrames,get_frame_count);
    SE_BIND_METHOD(SpriteFrames,get_frame);
    SE_BIND_METHOD(SpriteFrames,set_frame);
    SE_BIND_METHOD(SpriteFrames,remove_frame);
    SE_BIND_METHOD(SpriteFrames,clear);
    SE_BIND_METHOD(SpriteFrames,clear_all);

    SE_BIND_METHOD(SpriteFrames,_set_frames);
    SE_BIND_METHOD(SpriteFrames,_get_frames);

    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "frames", PropertyHint::None, "", 0), "_set_frames", "_get_frames"); //compatibility

    SE_BIND_METHOD(SpriteFrames,_set_animations);
    SE_BIND_METHOD(SpriteFrames,_get_animations);

    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "animations", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_animations", "_get_animations"); //compatibility
}

SpriteFrames::SpriteFrames() {

    add_animation(SceneStringNames::_default);
}

void AnimatedSprite2D::_validate_property(PropertyInfo &property) const {

    if (not frames)
        return;
    if (property.name == "animation") {

        property.hint = PropertyHint::Enum;
        List<StringName> names;
        frames->get_animation_list(&names);
        names.sort(WrapAlphaCompare());

        bool current_found = false;

        for (List<StringName>::iterator E = names.begin(); E!=names.end(); ++E) {
            if (E!=names.begin()) {
                property.hint_string += ',';
            }

            property.hint_string += *E;
            if (animation == *E) {
                current_found = true;
            }
        }

        if (!current_found) {
            if (property.hint_string.empty()) {
                property.hint_string = animation;
            } else {
                property.hint_string = String(animation) + "," + property.hint_string;
            }
        }
    }

    if (property.name == "frame") {
        property.hint = PropertyHint::Range;
        if (frames->has_animation(animation) && frames->get_frame_count(animation) > 1) {
            property.hint_string = "0," + itos(frames->get_frame_count(animation) - 1) + ",1";
        }
        property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
    }
}

void AnimatedSprite2D::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_INTERNAL_PROCESS: {

            if (not frames)
                return;
            if (!frames->has_animation(animation))
                return;
            if (frame < 0)
                return;


            float remaining = get_process_delta_time();

            while (remaining) {
                float speed = frames->get_animation_speed(animation) * speed_scale;
                if (speed == 0.0f) {
                    return; // do nothing
                }

                if (timeout <= 0) {

                    timeout = _get_frame_duration();

                    int fc = frames->get_frame_count(animation);
                    if ((!backwards && frame >= fc - 1) || (backwards && frame <= 0)) {
                        if (frames->get_animation_loop(animation)) {
                            if (backwards)
                                frame = fc - 1;
                            else
                                frame = 0;

                            emit_signal(SceneStringNames::animation_finished);
                        } else {
                            if (backwards)
                                frame = 0;
                            else
                                frame = fc - 1;

                            if (!is_over) {
                                is_over = true;
                                emit_signal(SceneStringNames::animation_finished);
                            }
                        }
                    } else {
                        if (backwards)
                            frame--;
                        else
                            frame++;
                    }

                    update();
                    Object_change_notify(this,"frame");
                    emit_signal(SceneStringNames::frame_changed);
                }

                float to_process = MIN(timeout, remaining);
                remaining -= to_process;
                timeout -= to_process;
            }
        } break;

        case NOTIFICATION_DRAW: {

            if (not frames)
                return;
            if (frame < 0)
                return;
            if (!frames->has_animation(animation))
                return;

            Ref<Texture> texture = frames->get_frame(animation, frame);
            if (not texture)
                return;

            Ref<Texture> normal = frames->get_normal_frame(animation, frame);

            RenderingEntity ci = get_canvas_item();

            Size2i s = texture->get_size();
            Point2 ofs = offset;
            if (centered)
                ofs -= s / 2;

            if (Engine::get_singleton()->get_use_gpu_pixel_snap()) {
                ofs = ofs.floor();
            }
            Rect2 dst_rect(ofs, s);

            if (hflip)
                dst_rect.size.x = -dst_rect.size.x;
            if (vflip)
                dst_rect.size.y = -dst_rect.size.y;

            texture->draw_rect_region(ci, dst_rect, Rect2(Vector2(), texture->get_size()), Color(1, 1, 1), false, normal);

        } break;
    }
}

void AnimatedSprite2D::set_sprite_frames(const Ref<SpriteFrames> &p_frames) {

    if (frames)
        frames->disconnect("changed",callable_mp(this, &ClassName::_res_changed));
    frames = p_frames;
    if (frames)
        frames->connect("changed",callable_mp(this, &ClassName::_res_changed));

    if (not frames) {
        frame = 0;
    } else {
        set_frame(frame);
    }

    Object_change_notify(this);
    _reset_timeout();
    update();
    update_configuration_warning();
}

Ref<SpriteFrames> AnimatedSprite2D::get_sprite_frames() const {

    return frames;
}

void AnimatedSprite2D::set_frame(int p_frame) {

    if (not frames) {
        return;
    }

    if (frames->has_animation(animation)) {
        int limit = frames->get_frame_count(animation);
        if (p_frame >= limit)
            p_frame = limit - 1;
    }

    if (p_frame < 0)
        p_frame = 0;

    if (frame == p_frame)
        return;

    frame = p_frame;
    _reset_timeout();
    update();
    Object_change_notify(this,"frame");
    emit_signal(SceneStringNames::frame_changed);
}
int AnimatedSprite2D::get_frame() const {

    return frame;
}

void AnimatedSprite2D::set_speed_scale(float p_speed_scale) {

    float elapsed = _get_frame_duration() - timeout;

    speed_scale = M_MAX(p_speed_scale, 0.0f);

    // We adapt the timeout so that the animation speed adapts as soon as the speed scale is changed
    _reset_timeout();
    timeout -= elapsed;
}

float AnimatedSprite2D::get_speed_scale() const {

    return speed_scale;
}

void AnimatedSprite2D::set_centered(bool p_center) {

    centered = p_center;
    update();
    item_rect_changed();
}

bool AnimatedSprite2D::is_centered() const {

    return centered;
}

void AnimatedSprite2D::set_offset(const Point2 &p_offset) {

    offset = p_offset;
    update();
    item_rect_changed();
    Object_change_notify(this,"offset");
}
Point2 AnimatedSprite2D::get_offset() const {

    return offset;
}

void AnimatedSprite2D::set_flip_h(bool p_flip) {

    hflip = p_flip;
    update();
}
bool AnimatedSprite2D::is_flipped_h() const {

    return hflip;
}

void AnimatedSprite2D::set_flip_v(bool p_flip) {

    vflip = p_flip;
    update();
}
bool AnimatedSprite2D::is_flipped_v() const {

    return vflip;
}

void AnimatedSprite2D::_res_changed() {

    set_frame(frame);
    Object_change_notify(this,"frame");
    Object_change_notify(this,"animation");
    update();
}

void AnimatedSprite2D::set_playing(bool p_playing) {

    if (playing == p_playing)
        return;
    playing = p_playing;
    _reset_timeout();
    set_process_internal(playing);
}

bool AnimatedSprite2D::is_playing() const {

    return playing;
}

void AnimatedSprite2D::play(const StringName &p_animation, const bool p_backwards) {

    backwards = p_backwards;

    if (p_animation) {
        set_animation(p_animation);
        if (frames && backwards && get_frame() == 0)
            set_frame(frames->get_frame_count(p_animation) - 1);
    }

    set_playing(true);
}

void AnimatedSprite2D::stop() {

    set_playing(false);
}

float AnimatedSprite2D::_get_frame_duration() {
    if (frames && frames->has_animation(animation)) {
        float speed = frames->get_animation_speed(animation) * speed_scale;
        if (speed > 0) {
            return 1.0f / speed;
        }
    }
    return 0.0;
}

void AnimatedSprite2D::_reset_timeout() {

    if (!playing)
        return;

    timeout = _get_frame_duration();
    is_over = false;
}

void AnimatedSprite2D::set_animation(const StringName &p_animation) {

    ERR_FAIL_COND_MSG(frames == nullptr, FormatVE("There is no animation with name '%s'.", p_animation.asCString()));
    ERR_FAIL_COND_MSG(not frames->animation_name_map().contains(p_animation), FormatVE("There is no animation with name '%s'.",p_animation.asCString()));

    if (animation == p_animation)
        return;

    animation = p_animation;
    _reset_timeout();
    set_frame(0);
    Object_change_notify(this);
    update();
}
StringName AnimatedSprite2D::get_animation() const {

    return animation;
}

String AnimatedSprite2D::get_configuration_warning() const {

    String warning = BaseClassName::get_configuration_warning();
    if (!frames) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("A SpriteFrames resource must be created or set in the \"Frames\" property in order for AnimatedSprite to display frames.");
    }

    return warning;
}

void AnimatedSprite2D::_bind_methods() {

    SE_BIND_METHOD(AnimatedSprite2D,set_sprite_frames);
    SE_BIND_METHOD(AnimatedSprite2D,get_sprite_frames);

    SE_BIND_METHOD(AnimatedSprite2D,set_animation);
    SE_BIND_METHOD(AnimatedSprite2D,get_animation);

    SE_BIND_METHOD(AnimatedSprite2D,set_playing);
    MethodBinder::bind_method(D_METHOD("is_playing"), &AnimatedSprite2D::_is_playing);

    MethodBinder::bind_method(D_METHOD("play", {"anim", "backwards"}), &AnimatedSprite2D::play, {DEFVAL(StringName()), DEFVAL(false)});
    SE_BIND_METHOD(AnimatedSprite2D,stop);

    SE_BIND_METHOD(AnimatedSprite2D,set_centered);
    SE_BIND_METHOD(AnimatedSprite2D,is_centered);

    SE_BIND_METHOD(AnimatedSprite2D,set_offset);
    SE_BIND_METHOD(AnimatedSprite2D,get_offset);

    SE_BIND_METHOD(AnimatedSprite2D,set_flip_h);
    SE_BIND_METHOD(AnimatedSprite2D,is_flipped_h);

    SE_BIND_METHOD(AnimatedSprite2D,set_flip_v);
    SE_BIND_METHOD(AnimatedSprite2D,is_flipped_v);

    SE_BIND_METHOD(AnimatedSprite2D,set_frame);
    SE_BIND_METHOD(AnimatedSprite2D,get_frame);

    SE_BIND_METHOD(AnimatedSprite2D,set_speed_scale);
    SE_BIND_METHOD(AnimatedSprite2D,get_speed_scale);

    SE_BIND_METHOD(AnimatedSprite2D,_res_changed);

    ADD_SIGNAL(MethodInfo("frame_changed"));
    ADD_SIGNAL(MethodInfo("animation_finished"));

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "frames", PropertyHint::ResourceType, "SpriteFrames"), "set_sprite_frames", "get_sprite_frames");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "animation"), "set_animation", "get_animation");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "frame"), "set_frame", "get_frame");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "speed_scale"), "set_speed_scale", "get_speed_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "playing"), "set_playing", "is_playing");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "centered"), "set_centered", "is_centered");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "offset"), "set_offset", "get_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "flip_h"), "set_flip_h", "is_flipped_h");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "flip_v"), "set_flip_v", "is_flipped_v");
}

AnimatedSprite2D::AnimatedSprite2D() {

    centered = true;
    hflip = false;
    vflip = false;

    frame = 0;
    speed_scale = 1.0f;
    playing = false;
    backwards = false;
    animation = "default";
    timeout = 0;
    is_over = false;
}
