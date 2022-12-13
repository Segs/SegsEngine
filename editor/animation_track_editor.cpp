/*************************************************************************/
/*  animation_track_editor.cpp                                           */
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

#include "animation_track_editor.h"

#include "animation_track_editor_plugins.h"

#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/object_tooling.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/script_language.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"
#include "editor/animation_bezier_editor.h"
#include "editor/editor_settings.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_bar.h"
#include "scene/gui/slider.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tool_button.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"
#include "servers/audio/audio_stream.h"

#include "core/callable_method_pointer.h"

IMPL_GDCLASS(AnimationTimelineEdit)
IMPL_GDCLASS(AnimationTrackEdit)
IMPL_GDCLASS(AnimationTrackEditPlugin)
IMPL_GDCLASS(AnimationTrackEditGroup)
IMPL_GDCLASS(AnimationTrackEditor)

class GODOT_EXPORT AnimationTrackKeyEdit : public Object {

    GDCLASS(AnimationTrackKeyEdit,Object)

public:
    bool setting;

    bool _hide_script_from_inspector() {
        return true;
    }

    bool _dont_undo_redo() {
        return true;
    }

    static void _bind_methods() {

        MethodBinder::bind_method("_update_obj", &AnimationTrackKeyEdit::_update_obj);
        MethodBinder::bind_method("_key_ofs_changed", &AnimationTrackKeyEdit::_key_ofs_changed);
        MethodBinder::bind_method("_hide_script_from_inspector", &AnimationTrackKeyEdit::_hide_script_from_inspector);
        MethodBinder::bind_method("get_root_path", &AnimationTrackKeyEdit::get_root_path);
        MethodBinder::bind_method("_dont_undo_redo", &AnimationTrackKeyEdit::_dont_undo_redo);
    }

    void _fix_node_path(Variant &value) {

        NodePath np= value.as<NodePath>();

        if (np == NodePath())
            return;

        Node *root = EditorNode::get_singleton()->get_tree()->get_root();

        Node *np_node = root->get_node(np);
        ERR_FAIL_COND(!np_node);

        Node *edited_node = root->get_node(base);
        ERR_FAIL_COND(!edited_node);

        value = edited_node->get_path_to(np_node);
    }

    void _update_obj(const Ref<Animation> &p_anim) {

        if (setting || animation != p_anim)
            return;

        notify_change();
    }

    void _key_ofs_changed(const Ref<Animation> &p_anim, float from, float to) {

        if (animation != p_anim || from != key_ofs)
            return;

        key_ofs = to;

        if (setting)
            return;

        notify_change();
    }

    bool _set(const StringName &p_name, const Variant &p_value) {
        using namespace eastl;
        int key = animation->track_find_key(track, key_ofs, true);
        ERR_FAIL_COND_V(key == -1, false);

        StringView name(p_name);
        if (name == "time"_sv || name == "frame"_sv) {

            float new_time= p_value.as<float>();

            if (name == "frame"_sv) {
                float fps = animation->get_step();
                if (fps > 0) {
                    fps = 1.0f / fps;
                }
                new_time /= fps;
            }

            if (new_time == key_ofs)
                return true;

            int existing = animation->track_find_key(track, new_time, true);

            setting = true;
            undo_redo->create_action(TTR("Anim Change Keyframe Time"), UndoRedo::MERGE_ENDS);

            Variant val = animation->track_get_key_value(track, key);
            float trans = animation->track_get_key_transition(track, key);
            Variant v;
            if (existing != -1) {
                v = animation->track_get_key_value(track, existing);
                trans = animation->track_get_key_transition(track, existing);
            }
            undo_redo->create_action_pair(TTR("Anim Change Keyframe Time"),get_instance_id(),
                [=](){
                    animation->track_remove_key(track,key);
                    animation->track_insert_key(track, new_time, val, trans);
                    _key_ofs_changed(animation, key_ofs, new_time);
                },
                [=](){
                    animation->track_remove_key_at_position(track,new_time);
                    animation->track_insert_key(track, key_ofs, val, trans);
                    _key_ofs_changed(animation, new_time, key_ofs);
                    if (existing != -1) {
                        animation->track_insert_key(track, new_time, v, trans);
                    }
                }, UndoRedo::MERGE_ENDS
            );

            undo_redo->commit_action();

            setting = false;
            return true;
        }

        if (name == "easing"_sv) {

            float val= p_value.as<float>();
            float prev_val = animation->track_get_key_transition(track, key);
            setting = true;
            undo_redo->create_action(TTR("Anim Change Transition"), UndoRedo::MERGE_ENDS);
            undo_redo->add_do_method(animation.get(), "track_set_key_transition", track, key, val);
            undo_redo->add_undo_method(animation.get(), "track_set_key_transition", track, key, prev_val);
            undo_redo->add_do_method(this, "_update_obj", animation);
            undo_redo->add_undo_method(this, "_update_obj", animation);
            undo_redo->commit_action();

            setting = false;
            return true;
        }

        switch (animation->track_get_type(track)) {

            case Animation::TYPE_TRANSFORM: {

                Dictionary d_old = animation->track_get_key_value(track, key).as<Dictionary>();
                Dictionary d_new = d_old.duplicate();
                d_new[p_name] = p_value;
                setting = true;
                undo_redo->create_action(TTR("Anim Change Transform"));
                undo_redo->add_do_method(animation.get(), "track_set_key_value", track, key, d_new);
                undo_redo->add_undo_method(animation.get(), "track_set_key_value", track, key, d_old);
                undo_redo->add_do_method(this, "_update_obj", animation);
                undo_redo->add_undo_method(this, "_update_obj", animation);
                undo_redo->commit_action();

                setting = false;
                return true;
            } break;
            case Animation::TYPE_VALUE: {

                if (name == "value"_sv) {

                    Variant value = p_value;

                    if (value.get_type() == VariantType::NODE_PATH) {
                        _fix_node_path(value);
                    }

                    setting = true;
                    undo_redo->create_action(TTR("Anim Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                    Variant prev = animation->track_get_key_value(track, key);
                    undo_redo->add_do_method(animation.get(), "track_set_key_value", track, key, value);
                    undo_redo->add_undo_method(animation.get(), "track_set_key_value", track, key, prev);
                    undo_redo->add_do_method(this, "_update_obj", animation);
                    undo_redo->add_undo_method(this, "_update_obj", animation);
                    undo_redo->commit_action();

                    setting = false;
                    return true;
                }
            } break;
            case Animation::TYPE_METHOD: {

                Dictionary d_old = animation->track_get_key_value(track, key).as<Dictionary>();
                Dictionary d_new = d_old.duplicate();

                bool change_notify_deserved = false;
                bool mergeable = false;

                if (name == "name"_sv) {

                    d_new["method"] = p_value;
                } else if (name == "arg_count"_sv) {

                    Array args = d_old["args"].as<Array>();
                    args.resize(p_value.as<int>());
                    d_new["args"] = Variant::from(args);
                    change_notify_deserved = true;
                } else if (StringUtils::begins_with(name,"args/")) {

                    Array args = d_old["args"].as<Array>();
                    int idx = StringUtils::to_int(StringUtils::get_slice(name,"/", 1));
                    ERR_FAIL_INDEX_V(idx, args.size(), false);

                    StringView what = StringUtils::get_slice(name,"/", 2);
                    if (what == "type"_sv) {
                        VariantType t = p_value.as<VariantType>();

                        if (t != args[idx].get_type()) {
                            Callable::CallError err;
                            if (Variant::can_convert(args[idx].get_type(), t)) {
                                args[idx] = Variant::construct(t, args[idx], err);
                            } else {

                                args[idx] = Variant::construct_default(t);
                            }
                            change_notify_deserved = true;
                            d_new["args"] = Variant::from(args);
                        }
                    } else if (what == "value"_sv) {

                        Variant value = p_value;
                        if (value.get_type() == VariantType::NODE_PATH) {

                            _fix_node_path(value);
                        }

                        args[idx] = value;
                        d_new["args"] = Variant::from(args);
                        mergeable = true;
                    }
                }

                if (mergeable)
                    undo_redo->create_action(TTR("Anim Change Call"), UndoRedo::MERGE_ENDS);
                else
                    undo_redo->create_action(TTR("Anim Change Call"));

                setting = true;
                undo_redo->add_do_method(animation.get(), "track_set_key_value", track, key, d_new);
                undo_redo->add_undo_method(animation.get(), "track_set_key_value", track, key, d_old);
                undo_redo->add_do_method(this, "_update_obj", animation);
                undo_redo->add_undo_method(this, "_update_obj", animation);
                undo_redo->commit_action();

                setting = false;
                if (change_notify_deserved)
                    notify_change();
                return true;
            }
            case Animation::TYPE_BEZIER: {

                if (name == "value"_sv) {

                    const Variant &value = p_value;

                    setting = true;
                    undo_redo->create_action(TTR("Anim Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                    float prev = animation->bezier_track_get_key_value(track, key);
                    undo_redo->add_do_method(animation.get(), "bezier_track_set_key_value", track, key, value);
                    undo_redo->add_undo_method(animation.get(), "bezier_track_set_key_value", track, key, prev);
                    undo_redo->add_do_method(this, "_update_obj", animation);
                    undo_redo->add_undo_method(this, "_update_obj", animation);
                    undo_redo->commit_action();

                    setting = false;
                    return true;
                }

                if (name == "in_handle"_sv) {

                    const Variant &value = p_value;

                    setting = true;
                    undo_redo->create_action(TTR("Anim Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                    Vector2 prev = animation->bezier_track_get_key_in_handle(track, key);
                    undo_redo->add_do_method(animation.get(), "bezier_track_set_key_in_handle", track, key, value);
                    undo_redo->add_undo_method(animation.get(), "bezier_track_set_key_in_handle", track, key, prev);
                    undo_redo->add_do_method(this, "_update_obj", animation);
                    undo_redo->add_undo_method(this, "_update_obj", animation);
                    undo_redo->commit_action();

                    setting = false;
                    return true;
                }

                if (name == "out_handle"_sv) {

                    const Variant &value = p_value;

                    setting = true;
                    undo_redo->create_action(TTR("Anim Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                    Vector2 prev = animation->bezier_track_get_key_out_handle(track, key);
                    undo_redo->add_do_method(animation.get(), "bezier_track_set_key_out_handle", track, key, value);
                    undo_redo->add_undo_method(animation.get(), "bezier_track_set_key_out_handle", track, key, prev);
                    undo_redo->add_do_method(this, "_update_obj", animation);
                    undo_redo->add_undo_method(this, "_update_obj", animation);
                    undo_redo->commit_action();

                    setting = false;
                    return true;
                }
            } break;
            case Animation::TYPE_AUDIO: {

                if (name == "stream"_sv) {

                    Ref<AudioStream> stream(p_value);

                    setting = true;
                    undo_redo->create_action(TTR("Anim Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                    RES prev(animation->audio_track_get_key_stream(track, key));
                    undo_redo->add_do_method(animation.get(), "audio_track_set_key_stream", track, key, stream);
                    undo_redo->add_undo_method(animation.get(), "audio_track_set_key_stream", track, key, prev);
                    undo_redo->add_do_method(this, "_update_obj", animation);
                    undo_redo->add_undo_method(this, "_update_obj", animation);
                    undo_redo->commit_action();

                    setting = false;
                    return true;
                }

                if (name == "start_offset"_sv) {

                    float value= p_value.as<float>();

                    setting = true;
                    undo_redo->create_action(TTR("Anim Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                    float prev = animation->audio_track_get_key_start_offset(track, key);
                    undo_redo->add_do_method(animation.get(), "audio_track_set_key_start_offset", track, key, value);
                    undo_redo->add_undo_method(animation.get(), "audio_track_set_key_start_offset", track, key, prev);
                    undo_redo->add_do_method(this, "_update_obj", animation);
                    undo_redo->add_undo_method(this, "_update_obj", animation);
                    undo_redo->commit_action();

                    setting = false;
                    return true;
                }

                if (name == "end_offset"_sv) {

                    float value= p_value.as<float>();

                    setting = true;
                    undo_redo->create_action(TTR("Anim Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                    float prev = animation->audio_track_get_key_end_offset(track, key);
                    undo_redo->add_do_method(animation.get(), "audio_track_set_key_end_offset", track, key, value);
                    undo_redo->add_undo_method(animation.get(), "audio_track_set_key_end_offset", track, key, prev);
                    undo_redo->add_do_method(this, "_update_obj", animation);
                    undo_redo->add_undo_method(this, "_update_obj", animation);
                    undo_redo->commit_action();

                    setting = false;
                    return true;
                }
            } break;
            case Animation::TYPE_ANIMATION: {

                if (name == "animation"_sv) {

                    StringName anim_name= p_value.as<StringName>();

                    setting = true;
                    undo_redo->create_action(TTR("Anim Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                    StringName prev = animation->animation_track_get_key_animation(track, key);
                    undo_redo->add_do_method(animation.get(), "animation_track_set_key_animation", track, key, anim_name);
                    undo_redo->add_undo_method(animation.get(), "animation_track_set_key_animation", track, key, prev);
                    undo_redo->add_do_method(this, "_update_obj", animation);
                    undo_redo->add_undo_method(this, "_update_obj", animation);
                    undo_redo->commit_action();

                    setting = false;
                    return true;
                }
            } break;
        }

        return false;
    }

    bool _get(const StringName &p_name, Variant &r_ret) const {
        using namespace eastl;

        int key = animation->track_find_key(track, key_ofs, true);
        ERR_FAIL_COND_V(key == -1, false);

        StringView name(p_name);
        if (name == "time"_sv) {
            r_ret = key_ofs;
            return true;
        }

        if (name == "frame"_sv) {

            float fps = animation->get_step();
            if (fps > 0) {
                fps = 1.0f / fps;
            }
            r_ret = key_ofs * fps;
            return true;
        }

        if (name == "easing"_sv) {
            r_ret = animation->track_get_key_transition(track, key);
            return true;
        }

        switch (animation->track_get_type(track)) {
            case Animation::TYPE_TRANSFORM: {

                Dictionary d = animation->track_get_key_value(track, key).as<Dictionary>();
                ERR_FAIL_COND_V(!d.has(StringName(name)), false);
                r_ret = d[p_name];
                return true;

            } break;
            case Animation::TYPE_VALUE: {

                if (name == "value"_sv) {
                    r_ret = animation->track_get_key_value(track, key);
                    return true;
                }

            } break;
            case Animation::TYPE_METHOD: {

                Dictionary d = animation->track_get_key_value(track, key).as<Dictionary>();

                if (name == "name"_sv) {

                    ERR_FAIL_COND_V(!d.has("method"), false);
                    r_ret = d["method"];
                    return true;
                }

                ERR_FAIL_COND_V(!d.has("args"), false);

                Array args = d["args"].as<Array>();

                if (name == "arg_count"_sv) {
                    r_ret = args.size();
                    return true;
                }

                if (StringUtils::begins_with(name,"args/")) {

                    int idx = StringUtils::to_int(StringUtils::get_slice(name,"/", 1));
                    ERR_FAIL_INDEX_V(idx, args.size(), false);

                    StringView what = StringUtils::get_slice(name,"/", 2);
                    if (what == "type"_sv) {
                        r_ret = args[idx].get_type();
                        return true;
                    }

                    if (what == "value"_sv) {
                        r_ret = args[idx];
                        return true;
                    }
                }

            } break;
            case Animation::TYPE_BEZIER: {

                if (name == "value"_sv) {
                    r_ret = animation->bezier_track_get_key_value(track, key);
                    return true;
                }

                if (name == "in_handle"_sv) {
                    r_ret = animation->bezier_track_get_key_in_handle(track, key);
                    return true;
                }

                if (name == "out_handle"_sv) {
                    r_ret = animation->bezier_track_get_key_out_handle(track, key);
                    return true;
                }

            } break;
            case Animation::TYPE_AUDIO: {

                if (name == "stream"_sv) {
                    r_ret = animation->audio_track_get_key_stream(track, key);
                    return true;
                }

                if (name == "start_offset"_sv) {
                    r_ret = animation->audio_track_get_key_start_offset(track, key);
                    return true;
                }

                if (name == "end_offset"_sv) {
                    r_ret = animation->audio_track_get_key_end_offset(track, key);
                    return true;
                }

            } break;
            case Animation::TYPE_ANIMATION: {

                if (name == "animation"_sv) {
                    r_ret = animation->animation_track_get_key_animation(track, key);
                    return true;
                }

            } break;
        }

        return false;
    }
    void _get_property_list(Vector<PropertyInfo> *p_list) const {

        if (not animation)
            return;

        ERR_FAIL_INDEX(track, animation->get_track_count());
        int key = animation->track_find_key(track, key_ofs, true);
        ERR_FAIL_COND(key == -1);

        if (use_fps && animation->get_step() > 0) {
            float max_frame = animation->get_length() / animation->get_step();
            p_list->push_back(PropertyInfo(VariantType::FLOAT, "frame", PropertyHint::Range, "0," + rtos(max_frame) + ",1"));
        } else {
            p_list->push_back(PropertyInfo(VariantType::FLOAT, "time", PropertyHint::Range, "0," + rtos(animation->get_length()) + ",0.01"));
        }

        switch (animation->track_get_type(track)) {

            case Animation::TYPE_TRANSFORM: {

                p_list->push_back(PropertyInfo(VariantType::VECTOR3, "location"));
                p_list->push_back(PropertyInfo(VariantType::QUAT, "rotation"));
                p_list->push_back(PropertyInfo(VariantType::VECTOR3, "scale"));

            } break;
            case Animation::TYPE_VALUE: {

                Variant v = animation->track_get_key_value(track, key);

                if (hint.type != VariantType::NIL) {

                    PropertyInfo pi = hint;
                    pi.name = "value";
                    p_list->push_back(pi);
                } else {

                    PropertyHint hint = PropertyHint::None;
                    String hint_string;

                    if (v.get_type() == VariantType::OBJECT) {
                        //could actually check the object property if exists..? yes i will!
                        Ref<Resource> res(v);
                        if (res) {

                            hint = PropertyHint::ResourceType;
                            hint_string = res->get_class();
                        }
                    }

                    if (v.get_type() != VariantType::NIL)
                        p_list->push_back(PropertyInfo(v.get_type(), "value", hint, hint_string));
                }

            } break;
            case Animation::TYPE_METHOD: {

                p_list->push_back(PropertyInfo(VariantType::STRING_NAME, "name"));
                p_list->push_back(PropertyInfo(VariantType::INT, "arg_count", PropertyHint::Range, "0,5,1"));

                Dictionary d = animation->track_get_key_value(track, key).as<Dictionary>();
                ERR_FAIL_COND(!d.has("args"));
                Array args = d["args"].as<Array>();
                String vtypes;
                for (int i = 0; i < (int)VariantType::VARIANT_MAX; i++) {

                    if (i > 0)
                        vtypes += ',';
                    vtypes += Variant::get_type_name(VariantType(i));
                }

                for (int i = 0; i < args.size(); i++) {

                    p_list->push_back(PropertyInfo(VariantType::INT, StringName("args/" + itos(i) + "/type"), PropertyHint::Enum, vtypes));
                    if (args[i].get_type() != VariantType::NIL)
                        p_list->push_back(PropertyInfo(args[i].get_type(), StringName("args/" + itos(i) + "/value")));
                }

            } break;
            case Animation::TYPE_BEZIER: {

                p_list->push_back(PropertyInfo(VariantType::FLOAT, "value"));
                p_list->push_back(PropertyInfo(VariantType::VECTOR2, "in_handle"));
                p_list->push_back(PropertyInfo(VariantType::VECTOR2, "out_handle"));

            } break;
            case Animation::TYPE_AUDIO: {

                p_list->push_back(PropertyInfo(VariantType::OBJECT, "stream", PropertyHint::ResourceType, "AudioStream"));
                p_list->push_back(PropertyInfo(VariantType::FLOAT, "start_offset", PropertyHint::Range, "0,3600,0.01,or_greater"));
                p_list->push_back(PropertyInfo(VariantType::FLOAT, "end_offset", PropertyHint::Range, "0,3600,0.01,or_greater"));

            } break;
            case Animation::TYPE_ANIMATION: {

                String animations;

                if (root_path && root_path->has_node(animation->track_get_path(track))) {

                    AnimationPlayer *ap = object_cast<AnimationPlayer>(root_path->get_node(animation->track_get_path(track)));
                    if (ap) {
                        Vector<StringName> anims(ap->get_animation_list());
                        animations = String::joined(anims,",");
                    }
                }

                if (!animations.empty()) {
                    animations += ',';
                }
                animations += "[stop]";

                p_list->push_back(PropertyInfo(VariantType::STRING_NAME, "animation", PropertyHint::Enum, animations));

            } break;
        }

        if (animation->track_get_type(track) == Animation::TYPE_VALUE) {
            p_list->push_back(PropertyInfo(VariantType::FLOAT, "easing", PropertyHint::ExpEasing));
        }
    }

    UndoRedo *undo_redo;
    Ref<Animation> animation;
    int track;
    float key_ofs;
    Node *root_path;

    PropertyInfo hint;
    NodePath base;
    bool use_fps;

    void notify_change() {

        Object_change_notify(this);
    }

    Node *get_root_path() {
        return root_path;
    }

    void set_use_fps(bool p_enable) {
        use_fps = p_enable;
        Object_change_notify(this);
    }

    AnimationTrackKeyEdit() {
        use_fps = false;
        key_ofs = 0;
        track = -1;
        setting = false;
        root_path = nullptr;
    }
};

IMPL_GDCLASS(AnimationTrackKeyEdit)

class GODOT_EXPORT AnimationMultiTrackKeyEdit : public Object {

    GDCLASS(AnimationMultiTrackKeyEdit,Object)

public:
    bool setting;

    bool _hide_script_from_inspector() {
        return true;
    }

    bool _dont_undo_redo() {
        return true;
    }

    static void _bind_methods() {

        MethodBinder::bind_method("_update_obj", &AnimationMultiTrackKeyEdit::_update_obj);
        MethodBinder::bind_method("_key_ofs_changed", &AnimationMultiTrackKeyEdit::_key_ofs_changed);
        MethodBinder::bind_method("_hide_script_from_inspector", &AnimationMultiTrackKeyEdit::_hide_script_from_inspector);
        MethodBinder::bind_method("get_root_path", &AnimationMultiTrackKeyEdit::get_root_path);
        MethodBinder::bind_method("_dont_undo_redo", &AnimationMultiTrackKeyEdit::_dont_undo_redo);
    }

    void _fix_node_path(Variant &value, NodePath &base) {

        NodePath np= value.as<NodePath>();

        if (np == NodePath())
            return;

        Node *root = EditorNode::get_singleton()->get_tree()->get_root();

        Node *np_node = root->get_node(np);
        ERR_FAIL_COND(!np_node);

        Node *edited_node = root->get_node(base);
        ERR_FAIL_COND(!edited_node);

        value = edited_node->get_path_to(np_node);
    }

    void _update_obj(const Ref<Animation> &p_anim) {

        if (setting || animation != p_anim)
            return;

        notify_change();
    }

    void _key_ofs_changed(const Ref<Animation> &p_anim, float from, float to) {

        if (animation != p_anim)
            return;

        for (eastl::pair<const int,Vector<float> > &E : key_ofs_map) {
            int key = 0;
            for (float key_ofs : E.second) {

                if (from != key_ofs) {
                    ++key;
                    continue;
                }

                int track = E.first;
                key_ofs_map[track][key] = to;

                if (setting)
                    return;

                notify_change();

                return;
            }
        }
    }

    bool _set(const StringName &p_name, const Variant &p_value) {
        using namespace eastl;

        bool update_obj = false;
        bool change_notify_deserved = false;
        for (eastl::pair<const int,Vector<float> > &E : key_ofs_map) {

            int track = E.first;
            for (float key_ofs : E.second) {

                int key = animation->track_find_key(track, key_ofs, true);
                ERR_FAIL_COND_V(key == -1, false);

                StringView name(p_name);
                if (name == "time"_sv || name == "frame"_sv) {

                    float new_time= p_value.as<float>();

                    if (name == "frame"_sv) {
                        float fps = animation->get_step();
                        if (fps > 0) {
                            fps = 1.0f / fps;
                        }
                        new_time /= fps;
                    }

                    int existing = animation->track_find_key(track, new_time, true);

                    if (!setting) {
                        setting = true;
                        undo_redo->create_action(TTR("Anim Multi Change Keyframe Time"), UndoRedo::MERGE_ENDS);
                    }

                    Variant val = animation->track_get_key_value(track, key);
                    float trans = animation->track_get_key_transition(track, key);

                    undo_redo->add_do_method(animation.get(), "track_remove_key", track, key);
                    undo_redo->add_do_method(animation.get(), "track_insert_key", track, new_time, val, trans);
                    undo_redo->add_do_method(this, "_key_ofs_changed", animation, key_ofs, new_time);
                    undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", track, new_time);
                    undo_redo->add_undo_method(animation.get(), "track_insert_key", track, key_ofs, val, trans);
                    undo_redo->add_undo_method(this, "_key_ofs_changed", animation, new_time, key_ofs);

                    if (existing != -1) {
                        Variant v = animation->track_get_key_value(track, existing);
                        trans = animation->track_get_key_transition(track, existing);
                        undo_redo->add_undo_method(animation.get(), "track_insert_key", track, new_time, v, trans);
                    }
                } else if (name == "easing"_sv) {

                    float val= p_value.as<float>();
                    float prev_val = animation->track_get_key_transition(track, key);

                    if (!setting) {
                        setting = true;
                        undo_redo->create_action(TTR("Anim Multi Change Transition"), UndoRedo::MERGE_ENDS);
                    }
                    undo_redo->add_do_method(animation.get(), "track_set_key_transition", track, key, val);
                    undo_redo->add_undo_method(animation.get(), "track_set_key_transition", track, key, prev_val);
                    update_obj = true;
                }

                switch (animation->track_get_type(track)) {

                    case Animation::TYPE_TRANSFORM: {

                        Dictionary d_old = animation->track_get_key_value(track, key).as<Dictionary>();
                        Dictionary d_new = d_old.duplicate();
                        d_new[p_name] = p_value;

                        if (!setting) {
                            setting = true;
                            undo_redo->create_action(TTR("Anim Multi Change Transform"));
                        }
                        undo_redo->add_do_method(animation.get(), "track_set_key_value", track, key, d_new);
                        undo_redo->add_undo_method(animation.get(), "track_set_key_value", track, key, d_old);
                        update_obj = true;
                    } break;
                    case Animation::TYPE_VALUE: {

                        if (name == "value"_sv) {

                            Variant value = p_value;

                            if (value.get_type() == VariantType::NODE_PATH) {
                                _fix_node_path(value, base_map[track]);
                            }

                            if (!setting) {
                                setting = true;
                                undo_redo->create_action(TTR("Anim Multi Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                            }
                            Variant prev = animation->track_get_key_value(track, key);
                            undo_redo->add_do_method(animation.get(), "track_set_key_value", track, key, value);
                            undo_redo->add_undo_method(animation.get(), "track_set_key_value", track, key, prev);
                            update_obj = true;
                        }
                    } break;
                    case Animation::TYPE_METHOD: {

                        Dictionary d_old = animation->track_get_key_value(track, key).as<Dictionary>();
                        Dictionary d_new = d_old.duplicate();

                        bool mergeable = false;

                        if (name == "name"_sv) {

                            d_new["method"] = p_value;
                        } else if (name == "arg_count"_sv) {

                            Array args = d_old["args"].as<Array>();
                            args.resize(p_value.as<int>());
                            d_new["args"] = Variant::move_from(eastl::move(args));
                            change_notify_deserved = true;
                        } else if (StringUtils::begins_with(name,"args/")) {

                            Array args = d_old["args"].as<Array>();
                            int idx = StringUtils::to_int(StringUtils::get_slice(name,"/", 1));
                            ERR_FAIL_INDEX_V(idx, args.size(), false);

                            StringView what = StringUtils::get_slice(name,"/", 2);
                            if (what == "type"_sv) {
                                VariantType t = p_value.as<VariantType>();

                                if (t != args[idx].get_type()) {
                                    if (Variant::can_convert(args[idx].get_type(), t)) {
                                        Callable::CallError err;
                                        args[idx] = Variant::construct(t, args[idx], err);
                                    } else {

                                        args[idx] = Variant::construct_default(t);
                                    }
                                    change_notify_deserved = true;
                                    d_new["args"] = Variant::from(args);
                                }
                            } else if (what == "value"_sv) {

                                Variant value = p_value;
                                if (value.get_type() == VariantType::NODE_PATH) {

                                    _fix_node_path(value, base_map[track]);
                                }

                                args[idx] = value;
                                d_new["args"] = Variant::move_from(eastl::move(args));
                                mergeable = true;
                            }
                        }

                        Variant prev = animation->track_get_key_value(track, key);

                        if (!setting) {
                            if (mergeable)
                                undo_redo->create_action(TTR("Anim Multi Change Call"), UndoRedo::MERGE_ENDS);
                            else
                                undo_redo->create_action(TTR("Anim Multi Change Call"));

                            setting = true;
                        }

                        undo_redo->add_do_method(animation.get(), "track_set_key_value", track, key, d_new);
                        undo_redo->add_undo_method(animation.get(), "track_set_key_value", track, key, d_old);
                        update_obj = true;
                    } break;
                    case Animation::TYPE_BEZIER: {

                        if (name == "value"_sv) {

                            const Variant &value = p_value;

                            if (!setting) {
                                setting = true;
                                undo_redo->create_action(TTR("Anim Multi Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                            }
                            float prev = animation->bezier_track_get_key_value(track, key);
                            undo_redo->add_do_method(animation.get(), "bezier_track_set_key_value", track, key, value);
                            undo_redo->add_undo_method(animation.get(), "bezier_track_set_key_value", track, key, prev);
                            update_obj = true;
                        } else if (name == "in_handle"_sv) {

                            const Variant &value = p_value;

                            if (!setting) {
                                setting = true;
                                undo_redo->create_action(TTR("Anim Multi Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                            }
                            Vector2 prev = animation->bezier_track_get_key_in_handle(track, key);
                            undo_redo->add_do_method(animation.get(), "bezier_track_set_key_in_handle", track, key, value);
                            undo_redo->add_undo_method(animation.get(), "bezier_track_set_key_in_handle", track, key, prev);
                            update_obj = true;
                        } else if (name == "out_handle"_sv) {

                            const Variant &value = p_value;

                            if (!setting) {
                                setting = true;
                                undo_redo->create_action(TTR("Anim Multi Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                            }
                            Vector2 prev = animation->bezier_track_get_key_out_handle(track, key);
                            undo_redo->add_do_method(animation.get(), "bezier_track_set_key_out_handle", track, key, value);
                            undo_redo->add_undo_method(animation.get(), "bezier_track_set_key_out_handle", track, key, prev);
                            update_obj = true;
                        }
                    } break;
                    case Animation::TYPE_AUDIO: {

                        if (name == "stream"_sv) {

                            Ref<AudioStream> stream(p_value);

                            if (!setting) {
                                setting = true;
                                undo_redo->create_action(TTR("Anim Multi Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                            }
                            RES prev(animation->audio_track_get_key_stream(track, key));
                            undo_redo->add_do_method(animation.get(), "audio_track_set_key_stream", track, key, stream);
                            undo_redo->add_undo_method(animation.get(), "audio_track_set_key_stream", track, key, prev);
                            update_obj = true;
                        } else if (name == "start_offset"_sv) {

                            float value= p_value.as<float>();

                            if (!setting) {
                                setting = true;
                                undo_redo->create_action(TTR("Anim Multi Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                            }
                            float prev = animation->audio_track_get_key_start_offset(track, key);
                            undo_redo->add_do_method(animation.get(), "audio_track_set_key_start_offset", track, key, value);
                            undo_redo->add_undo_method(animation.get(), "audio_track_set_key_start_offset", track, key, prev);
                            update_obj = true;
                        } else if (name == "end_offset"_sv) {

                            float value= p_value.as<float>();

                            if (!setting) {
                                setting = true;
                                undo_redo->create_action(TTR("Anim Multi Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                            }
                            float prev = animation->audio_track_get_key_end_offset(track, key);
                            undo_redo->add_do_method(animation.get(), "audio_track_set_key_end_offset", track, key, value);
                            undo_redo->add_undo_method(animation.get(), "audio_track_set_key_end_offset", track, key, prev);
                            update_obj = true;
                        }
                    } break;
                    case Animation::TYPE_ANIMATION: {

                        if (name == "animation"_sv) {

                            StringName anim_name= p_value.as<StringName>();

                            if (!setting) {
                                setting = true;
                                undo_redo->create_action(TTR("Anim Multi Change Keyframe Value"), UndoRedo::MERGE_ENDS);
                            }
                            StringName prev = animation->animation_track_get_key_animation(track, key);
                            undo_redo->add_do_method(animation.get(), "animation_track_set_key_animation", track, key, anim_name);
                            undo_redo->add_undo_method(animation.get(), "animation_track_set_key_animation", track, key, prev);
                            update_obj = true;
                        }
                    } break;
                }
            }
        }

        if (setting) {

            if (update_obj) {
                undo_redo->add_do_method(this, "_update_obj", animation);
                undo_redo->add_undo_method(this, "_update_obj", animation);
            }

            undo_redo->commit_action();
            setting = false;

            if (change_notify_deserved)
                notify_change();

            return true;
        }

        return false;
    }

    bool _get(const StringName &p_name, Variant &r_ret) const {
        using namespace eastl;

        for (const eastl::pair<const int,Vector<float> > &E : key_ofs_map) {

            int track = E.first;
            for (float key_ofs : E.second) {

                int key = animation->track_find_key(track, key_ofs, true);
                ERR_CONTINUE(key == -1);

                StringView name(p_name);
                if (name == "time"_sv) {
                    r_ret = key_ofs;
                    return true;
                }

                if (name == "frame"_sv) {

                    float fps = animation->get_step();
                    if (fps > 0) {
                        fps = 1.0f / fps;
                    }
                    r_ret = key_ofs * fps;
                    return true;
                }

                if (name == "easing"_sv) {
                    r_ret = animation->track_get_key_transition(track, key);
                    return true;
                }

                switch (animation->track_get_type(track)) {

                    case Animation::TYPE_TRANSFORM: {

                        Dictionary d = animation->track_get_key_value(track, key).as<Dictionary>();
                        ERR_FAIL_COND_V(!d.has(StringName(name)), false);
                        r_ret = d[p_name];
                        return true;

                    } break;
                    case Animation::TYPE_VALUE: {

                        if (name == "value"_sv) {
                            r_ret = animation->track_get_key_value(track, key);
                            return true;
                        }

                    } break;
                    case Animation::TYPE_METHOD: {

                        Dictionary d = animation->track_get_key_value(track, key).as<Dictionary>();

                        if (name == "name"_sv) {

                            ERR_FAIL_COND_V(!d.has("method"), false);
                            r_ret = d["method"];
                            return true;
                        }

                        ERR_FAIL_COND_V(!d.has("args"), false);

                        Array args = d["args"].as<Array>();

                        if (name == "arg_count"_sv) {

                            r_ret = args.size();
                            return true;
                        }

                        if (StringUtils::begins_with(name,"args/")) {

                            int idx = StringUtils::to_int(StringUtils::get_slice(name,"/", 1));
                            ERR_FAIL_INDEX_V(idx, args.size(), false);

                            StringView what = StringUtils::get_slice(name,"/", 2);
                            if (what == "type"_sv) {
                                r_ret = args[idx].get_type();
                                return true;
                            }

                            if (what == "value"_sv) {
                                r_ret = args[idx];
                                return true;
                            }
                        }

                    } break;
                    case Animation::TYPE_BEZIER: {

                        if (name == "value"_sv) {
                            r_ret = animation->bezier_track_get_key_value(track, key);
                            return true;
                        }

                        if (name == "in_handle"_sv) {
                            r_ret = animation->bezier_track_get_key_in_handle(track, key);
                            return true;
                        }

                        if (name == "out_handle"_sv) {
                            r_ret = animation->bezier_track_get_key_out_handle(track, key);
                            return true;
                        }

                    } break;
                    case Animation::TYPE_AUDIO: {

                        if (name == "stream"_sv) {
                            r_ret = animation->audio_track_get_key_stream(track, key);
                            return true;
                        }

                        if (name == "start_offset"_sv) {
                            r_ret = animation->audio_track_get_key_start_offset(track, key);
                            return true;
                        }

                        if (name == "end_offset"_sv) {
                            r_ret = animation->audio_track_get_key_end_offset(track, key);
                            return true;
                        }

                    } break;
                    case Animation::TYPE_ANIMATION: {

                        if (name == "animation"_sv) {
                            r_ret = animation->animation_track_get_key_animation(track, key);
                            return true;
                        }

                    } break;
                }
            }
        }

        return false;
    }
    void _get_property_list(Vector<PropertyInfo> *p_list) const {

        if (not animation)
            return;

        int first_track = -1;
        float first_key = -1.0;

        bool show_time = true;
        bool same_track_type = true;
        bool same_key_type = true;
        for (const eastl::pair<const int,Vector<float> > &E : key_ofs_map) {

            int track = E.first;
            ERR_FAIL_INDEX(track, animation->get_track_count());

            if (first_track < 0)
                first_track = track;

            if (show_time && E.second.size() > 1)
                show_time = false;

            if (same_track_type) {

                if (animation->track_get_type(first_track) != animation->track_get_type(track)) {
                    same_track_type = false;
                    same_key_type = false;
                }

                for (float F : E.second) {

                    int key = animation->track_find_key(track, F, true);
                    ERR_FAIL_COND(key == -1);
                    if (first_key < 0)
                        first_key = key;

                    if (animation->track_get_key_value(first_track, first_key).get_type() != animation->track_get_key_value(track, key).get_type())
                        same_key_type = false;
                }
            }
        }

        if (show_time) {

            if (use_fps && animation->get_step() > 0) {
                float max_frame = animation->get_length() / animation->get_step();
                p_list->push_back(PropertyInfo(VariantType::FLOAT, "frame", PropertyHint::Range, "0," + rtos(max_frame) + ",1"));
            } else {
                p_list->push_back(PropertyInfo(VariantType::FLOAT, "time", PropertyHint::Range, "0," + rtos(animation->get_length()) + ",0.01"));
            }
        }

        if (same_track_type) {
            switch (animation->track_get_type(first_track)) {

                case Animation::TYPE_TRANSFORM: {

                    p_list->push_back(PropertyInfo(VariantType::VECTOR3, "location"));
                    p_list->push_back(PropertyInfo(VariantType::QUAT, "rotation"));
                    p_list->push_back(PropertyInfo(VariantType::VECTOR3, "scale"));
                } break;
                case Animation::TYPE_VALUE: {

                    if (same_key_type)
                    {
                        Variant v = animation->track_get_key_value(first_track, first_key);

                        if (hint.type != VariantType::NIL) {

                            PropertyInfo pi = hint;
                            pi.name = "value";
                            p_list->push_back(pi);
                        } else {

                            PropertyHint hint = PropertyHint::None;
                            String hint_string;

                            if (v.get_type() == VariantType::OBJECT) {
                                //could actually check the object property if exists..? yes i will!
                                Ref<Resource> res(v);
                                if (res) {

                                    hint = PropertyHint::ResourceType;
                                    hint_string = res->get_class();
                                }
                            }

                            if (v.get_type() != VariantType::NIL)
                                p_list->push_back(PropertyInfo(v.get_type(), "value", hint, hint_string));
                        }
                    }

                    p_list->push_back(PropertyInfo(VariantType::FLOAT, "easing", PropertyHint::ExpEasing));
                } break;
                case Animation::TYPE_METHOD: {

                    p_list->push_back(PropertyInfo(VariantType::STRING_NAME, "name"));
                    p_list->push_back(PropertyInfo(VariantType::INT, "arg_count", PropertyHint::Range, "0,5,1"));

                    Dictionary d = animation->track_get_key_value(first_track, first_key).as<Dictionary>();
                    ERR_FAIL_COND(!d.has("args"));
                    Array args = d["args"].as<Array>();
                    String vtypes;
                    for (int i = 0; i < (int)VariantType::VARIANT_MAX; i++) {

                        if (i > 0)
                            vtypes += ',';
                        vtypes += Variant::get_type_name(VariantType(i));
                    }

                    for (int i = 0,fin=args.size(); i < fin; i++) {

                        p_list->push_back(PropertyInfo(VariantType::INT, StringName("args/" + itos(i) + "/type"), PropertyHint::Enum, vtypes));
                        if (args[i].get_type() != VariantType::NIL)
                            p_list->push_back(PropertyInfo(args[i].get_type(), StringName("args/" + itos(i) + "/value")));
                    }
                } break;
                case Animation::TYPE_BEZIER: {

                    p_list->push_back(PropertyInfo(VariantType::FLOAT, "value"));
                    p_list->push_back(PropertyInfo(VariantType::VECTOR2, "in_handle"));
                    p_list->push_back(PropertyInfo(VariantType::VECTOR2, "out_handle"));
                } break;
                case Animation::TYPE_AUDIO: {

                    p_list->push_back(PropertyInfo(VariantType::OBJECT, "stream", PropertyHint::ResourceType, "AudioStream"));
                    p_list->push_back(PropertyInfo(VariantType::FLOAT, "start_offset", PropertyHint::Range, "0,3600,0.01,or_greater"));
                    p_list->push_back(PropertyInfo(VariantType::FLOAT, "end_offset", PropertyHint::Range, "0,3600,0.01,or_greater"));
                } break;
                case Animation::TYPE_ANIMATION: {

                    if (key_ofs_map.size() > 1)
                        break;

                    String animations;
                    Vector<StringName> anims;

                    if (root_path && root_path->has_node(animation->track_get_path(first_track))) {

                        AnimationPlayer *ap = object_cast<AnimationPlayer>(root_path->get_node(animation->track_get_path(first_track)));
                        if (ap) {
                            anims = ap->get_animation_list();
                        }
                    }
                    anims.emplace_back("[stop]");
                    animations = String::joined(anims,",");

                    p_list->push_back(PropertyInfo(VariantType::STRING_NAME, "animation", PropertyHint::Enum, animations));
                } break;
            }
        }
    }

    Ref<Animation> animation;

    Map<int, Vector<float> > key_ofs_map;
    Map<int, NodePath> base_map;
    PropertyInfo hint;

    Node *root_path;

    bool use_fps;

    UndoRedo *undo_redo;

    void notify_change() {

        Object_change_notify(this);
    }

    Node *get_root_path() {
        return root_path;
    }

    void set_use_fps(bool p_enable) {
        use_fps = p_enable;
        Object_change_notify(this);
    }

    AnimationMultiTrackKeyEdit() {
        use_fps = false;
        setting = false;
        root_path = nullptr;
    }
};

IMPL_GDCLASS(AnimationMultiTrackKeyEdit)

void AnimationTimelineEdit::_zoom_changed(double) {

    update();
    play_position->update();
    emit_signal("zoom_changed");
}

float AnimationTimelineEdit::get_zoom_scale() const {

    float zv = zoom->get_max() - zoom->get_value();

    if (zv < 1) {
        zv = 1.0 - zv;
        return Math::pow(1.0f + zv, 8.0f) * 100;
    } else {
        return 1.0 / Math::pow(zv, 8.0f) * 100;
    }
}

void AnimationTimelineEdit::_anim_length_changed(double p_new_len) {

    if (editing)
        return;

    p_new_len = M_MAX(0.001, p_new_len);
    if (use_fps && animation->get_step() > 0) {
        p_new_len *= animation->get_step();
    }

    editing = true;
    undo_redo->create_action(TTR("Change Animation Length"));
    undo_redo->add_do_method(animation.get(), "set_length", p_new_len);
    undo_redo->add_undo_method(animation.get(), "set_length", animation->get_length());
    undo_redo->commit_action();
    editing = false;
    update();

    emit_signal("length_changed", p_new_len);
}

void AnimationTimelineEdit::_anim_loop_pressed() {

    undo_redo->create_action(TTR("Change Animation Loop"));
    undo_redo->add_do_method(animation.get(), "set_loop", loop->is_pressed());
    undo_redo->add_undo_method(animation.get(), "set_loop", animation->has_loop());
    undo_redo->commit_action();
}

int AnimationTimelineEdit::get_buttons_width() const {

    Ref<Texture> interp_mode = get_theme_icon("TrackContinuous", "EditorIcons");
    Ref<Texture> interp_type = get_theme_icon("InterpRaw", "EditorIcons");
    Ref<Texture> loop_type = get_theme_icon("InterpWrapClamp", "EditorIcons");
    Ref<Texture> remove_icon = get_theme_icon("Remove", "EditorIcons");
    Ref<Texture> down_icon = get_theme_icon("select_arrow", "Tree");

    int total_w = interp_mode->get_width() + interp_type->get_width() + loop_type->get_width() + remove_icon->get_width();
    total_w += (down_icon->get_width() + 4 * EDSCALE) * 4;

    return total_w;
}

int AnimationTimelineEdit::get_name_limit() const {

    Ref<Texture> hsize_icon = get_theme_icon("Hsize", "EditorIcons");

    int limit = M_MAX(name_limit, add_track->get_minimum_size().width + hsize_icon->get_width());

    limit = MIN(limit, get_size().width - get_buttons_width() - 1);

    return limit;
}

void AnimationTimelineEdit::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        add_track->set_button_icon(get_theme_icon("Add", "EditorIcons"));
        loop->set_button_icon(get_theme_icon("Loop", "EditorIcons"));
        time_icon->set_texture(get_theme_icon("Time", "EditorIcons"));

        auto *popup(add_track->get_popup());
        popup->clear();
        popup->add_icon_item(get_theme_icon("KeyValue", "EditorIcons"), TTR("Property Track"));
        popup->add_icon_item(get_theme_icon("KeyXform", "EditorIcons"), TTR("3D Transform Track"));
        popup->add_icon_item(get_theme_icon("KeyCall", "EditorIcons"), TTR("Call Method Track"));
        popup->add_icon_item(get_theme_icon("KeyBezier", "EditorIcons"), TTR("Bezier Curve Track"));
        popup->add_icon_item(get_theme_icon("KeyAudio", "EditorIcons"), TTR("Audio Playback Track"));
        popup->add_icon_item(get_theme_icon("KeyAnimation", "EditorIcons"), TTR("Animation Playback Track"));
    }

    if (p_what == NOTIFICATION_RESIZED) {
        len_hb->set_position(Vector2(get_size().width - get_buttons_width(), 0));
        len_hb->set_size(Size2(get_buttons_width(), get_size().height));
    }

    if (p_what == NOTIFICATION_DRAW) {

        int key_range = get_size().width - get_buttons_width() - get_name_limit();

        if (not animation)
            return;

        Ref<Font> font = get_theme_font("font", "Label");
        Color color = get_theme_color("font_color", "Label");

        int zoomw = key_range;
        float scale = get_zoom_scale();
        int h = get_size().height;

        float l = animation->get_length();
        if (l <= 0)
            l = 0.001; //avoid crashor

        Ref<Texture> hsize_icon = get_theme_icon("Hsize", "EditorIcons");
        hsize_rect = Rect2(get_name_limit() - hsize_icon->get_width() - 2 * EDSCALE, (get_size().height - hsize_icon->get_height()) / 2, hsize_icon->get_width(), hsize_icon->get_height());
        draw_texture(hsize_icon, hsize_rect.position);

        {
            float time_min = 0;
            float time_max = animation->get_length();
            for (int i = 0; i < animation->get_track_count(); i++) {

                if (animation->track_get_key_count(i) > 0) {

                    float beg = animation->track_get_key_time(i, 0);
                    /*if (animation->track_get_type(i) == Animation::TYPE_BEZIER) {
                        beg += animation->bezier_track_get_key_in_handle(i, 0).x;
                    }* not worth it since they have no use */

                    if (beg < time_min)
                        time_min = beg;

                    float end = animation->track_get_key_time(i, animation->track_get_key_count(i) - 1);
                    /*if (animation->track_get_type(i) == Animation::TYPE_BEZIER) {
                        end += animation->bezier_track_get_key_out_handle(i, animation->track_get_key_count(i) - 1).x;
                    } not worth it since they have no use */

                    if (end > time_max)
                        time_max = end;
                }
            }

            float extra = zoomw / scale * 0.5;

            //if (time_min < -0.001)
            //  time_min -= extra;
            time_max += extra;
            set_min(time_min);
            set_max(time_max);

            if (zoomw / scale < time_max - time_min) {
                hscroll->show();

            } else {

                hscroll->hide();
            }
        }

        set_page(zoomw / scale);

        int end_px = (l - get_value()) * scale;
        int begin_px = -get_value() * scale;
        const Color notimecol = get_theme_color("dark_color_2", "Editor");
        const Color timecolor = Color(color,0.2f);
        const Color linecolor = Color(color,0.2f);

        {

            draw_rect_filled(Rect2(Point2(get_name_limit(), 0), Point2(zoomw - 1, h)), notimecol);

            if (begin_px < zoomw && end_px > 0) {

                if (begin_px < 0)
                    begin_px = 0;
                if (end_px > zoomw)
                    end_px = zoomw;

                draw_rect_filled(Rect2(Point2(get_name_limit() + begin_px, 0), Point2(end_px - begin_px - 1, h)), timecolor);
            }
        }

        Color color_time_sec = color;
        Color color_time_dec = color;
        color_time_dec.a *= 0.5;
#define SC_ADJ 100
        int min = 30;
        int dec = 1;
        int step = 1;
        int decimals = 2;
        bool step_found = false;

        const int period_width = font->get_char_size('.').width;
        int max_digit_width = font->get_char_size('0').width;
        for (int i = 1; i <= 9; i++) {
            const int digit_width = font->get_char_size('0' + i).width;
            max_digit_width = M_MAX(digit_width, max_digit_width);
        }
        const int max_sc = int(Math::ceil(zoomw / scale));
        const int max_sc_width = StringUtils::num(max_sc).length() * max_digit_width;

        while (!step_found) {

            min = max_sc_width;
            if (decimals > 0)
                min += period_width + max_digit_width * decimals;

            static const int _multp[3] = { 1, 2, 5 };
            for (int i = 0; i < 3; i++) {

                step = _multp[i] * dec;
                if (step * scale / SC_ADJ > min) {
                    step_found = true;
                    break;
                }
            }
            if (step_found)
                break;
            dec *= 10;
            decimals--;
            if (decimals < 0)
                decimals = 0;
        }

        if (use_fps) {

            float step_size = animation->get_step();
            if (step_size > 0) {

                int prev_frame_ofs = -10000000;

                for (int i = 0; i < zoomw; i++) {

                    float pos = get_value() + float(i) / scale;
                    float prev = get_value() + (float(i) - 1.0f) / scale;

                    int frame = pos / step_size;
                    int prev_frame = prev / step_size;

                    bool sub = Math::floor(prev) == Math::floor(pos);

                    if (frame != prev_frame && i >= prev_frame_ofs) {

                        draw_line(Point2(get_name_limit() + i, 0), Point2(get_name_limit() + i, h), linecolor, Math::round(EDSCALE));
                        UIString num(UIString::number(frame));
                        draw_ui_string(font, Point2(get_name_limit() + i + 3 * EDSCALE, (h - font->get_height()) / 2 + font->get_ascent()).floor(), num, sub ? color_time_dec : color_time_sec, zoomw - i);
                        prev_frame_ofs = i + font->get_ui_string_size(num).x + 5 * EDSCALE;
                    }
                }
            }

        } else {
            for (int i = 0; i < zoomw; i++) {

                float pos = get_value() + float(i) / scale;
                float prev = get_value() + (float(i) - 1.0f) / scale;

                int sc = int(Math::floor(pos * SC_ADJ));
                int prev_sc = int(Math::floor(prev * SC_ADJ));
                bool sub = sc % SC_ADJ;

                if (sc / step != prev_sc / step || (prev_sc < 0 && sc >= 0)) {

                    int scd = sc < 0 ? prev_sc : sc;
                    draw_line(Point2(get_name_limit() + i, 0), Point2(get_name_limit() + i, h), linecolor, Math::round(EDSCALE));
                    draw_string(font, Point2(get_name_limit() + i + 3, (h - font->get_height()) / 2 + font->get_ascent()).floor(),
                            StringUtils::num((scd - scd % step) / double(SC_ADJ), decimals),
                                sub ? color_time_dec : color_time_sec, zoomw - i);
                }
            }
        }

        draw_line(Vector2(0, get_size().height), get_size(), linecolor, Math::round(EDSCALE));
    }
}

void AnimationTimelineEdit::set_animation(const Ref<Animation> &p_animation) {
    animation = p_animation;
    if (animation) {
        len_hb->show();
        add_track->show();
        play_position->show();
    } else {
        len_hb->hide();
        add_track->hide();
        play_position->hide();
    }
    update();
    update_values();
}

Size2 AnimationTimelineEdit::get_minimum_size() const {

    Size2 ms = add_track->get_minimum_size();
    Ref<Font> font = get_theme_font("font", "Label");
    ms.height = M_MAX(ms.height, font->get_height());
    ms.width = get_buttons_width() + add_track->get_minimum_size().width + get_theme_icon("Hsize", "EditorIcons")->get_width() + 2;
    return ms;
}

void AnimationTimelineEdit::set_undo_redo(UndoRedo *p_undo_redo) {
    undo_redo = p_undo_redo;
}

void AnimationTimelineEdit::set_zoom(Range *p_zoom) {
    zoom = p_zoom;
    zoom->connect("value_changed",callable_mp(this, &ClassName::_zoom_changed));
}

void AnimationTimelineEdit::set_track_edit(AnimationTrackEdit *p_track_edit) {
    track_edit = p_track_edit;
}

void AnimationTimelineEdit::set_play_position(float p_pos) {

    play_position_pos = p_pos;
    play_position->update();
}

float AnimationTimelineEdit::get_play_position() const {
    return play_position_pos;
}

void AnimationTimelineEdit::update_play_position() {
    play_position->update();
}

void AnimationTimelineEdit::update_values() {

    if (not animation || editing)
        return;

    editing = true;
    if (use_fps && animation->get_step() > 0) {
        length->set_value(animation->get_length() / animation->get_step());
        length->set_step(1);
        length->set_tooltip(TTR("Animation length (frames)"));
        time_icon->set_tooltip(TTR("Animation length (frames)"));
    } else {
        length->set_value(animation->get_length());
        length->set_step(0.001f);
        length->set_tooltip(TTR("Animation length (seconds)"));
        time_icon->set_tooltip(TTR("Animation length (seconds)"));
    }
    loop->set_pressed(animation->has_loop());
    editing = false;
}

void AnimationTimelineEdit::_play_position_draw() {

    if (not animation || play_position_pos < 0)
        return;

    float scale = get_zoom_scale();
    int h = play_position->get_size().height;

    int px = (-get_value() + play_position_pos) * scale + get_name_limit();

    if (px >= get_name_limit() && px < play_position->get_size().width - get_buttons_width()) {
        Color color = get_theme_color("accent_color", "Editor");
        play_position->draw_line(Point2(px, 0), Point2(px, h), color, Math::round(2 * EDSCALE));
        play_position->draw_texture(
                get_theme_icon("TimelineIndicator", "EditorIcons"),
                Point2(px - get_theme_icon("TimelineIndicator", "EditorIcons")->get_width() * 0.5, 0),
                color);
    }
}

void AnimationTimelineEdit::_gui_input(const Ref<InputEvent> &p_event) {
    ERR_FAIL_COND(!p_event);

    const Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);
    if (mb) {
        if (mb->is_pressed() && mb->get_command() && (mb->get_button_index() == BUTTON_WHEEL_UP || mb->get_button_index() == BUTTON_WHEEL_DOWN)) {
            double new_zoom_value;
            double current_zoom_value = get_zoom()->get_value();
            int direction = mb->get_button_index() == BUTTON_WHEEL_UP ? 1 : -1;
            if (current_zoom_value <= 0.1) {
                new_zoom_value = M_MAX(0.01, current_zoom_value + 0.01 * direction);
            } else {
                new_zoom_value = direction < 0 ? M_MAX(0.01, current_zoom_value / 1.05) : current_zoom_value * 1.05;
            }
            get_zoom()->set_value(new_zoom_value);
            accept_event();
        }

        if (mb->is_pressed() && mb->get_alt() && mb->get_button_index() == BUTTON_WHEEL_UP) {
            if (track_edit) {
                track_edit->get_editor()->goto_prev_step(true);
            }
            accept_event();
        }

        if (mb->is_pressed() && mb->get_alt() && mb->get_button_index() == BUTTON_WHEEL_DOWN) {
            if (track_edit) {
                track_edit->get_editor()->goto_next_step(true);
            }
            accept_event();
        }

        if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT && hsize_rect.has_point(mb->get_position())) {
        dragging_hsize = true;
        dragging_hsize_from = mb->get_position().x;
        dragging_hsize_at = name_limit;
    }

        if (!mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT && dragging_hsize) {
        dragging_hsize = false;
    }
        if (mb->get_position().x > get_name_limit() && mb->get_position().x < (get_size().width - get_buttons_width())) {

        if (!panning_timeline && mb->get_button_index() == BUTTON_LEFT) {
            int x = mb->get_position().x - get_name_limit();

            float ofs = x / get_zoom_scale() + get_value();
            emit_signal("timeline_changed", ofs, false);
            dragging_timeline = true;
        }
        if (!dragging_timeline && mb->get_button_index() == BUTTON_MIDDLE) {
            int x = mb->get_position().x - get_name_limit();
            panning_timeline_from = x / get_zoom_scale();
            panning_timeline = true;
            panning_timeline_at = get_value();
        }
    }

        if (dragging_timeline && mb->get_button_index() == BUTTON_LEFT && !mb->is_pressed()) {
        dragging_timeline = false;
    }

        if (panning_timeline && mb->get_button_index() == BUTTON_MIDDLE && !mb->is_pressed()) {
        panning_timeline = false;
    }
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);

    if (mm) {


        if (dragging_hsize) {
            int ofs = mm->get_position().x - dragging_hsize_from;
            name_limit = dragging_hsize_at + ofs;
            update();
            emit_signal("name_limit_changed");
            play_position->update();
        }
        if (dragging_timeline) {
            int x = mm->get_position().x - get_name_limit();
            float ofs = x / get_zoom_scale() + get_value();
            emit_signal("timeline_changed", ofs, false);
        }
        if (panning_timeline) {
            int x = mm->get_position().x - get_name_limit();
            float ofs = x / get_zoom_scale();
            float diff = ofs - panning_timeline_from;
            set_value(panning_timeline_at - diff);
        }
    }
}

Control::CursorShape AnimationTimelineEdit::get_cursor_shape(const Point2 &p_pos) const {
    if (dragging_hsize || hsize_rect.has_point(p_pos)) {
        // Indicate that the track name column's width can be adjusted
        return Control::CURSOR_HSIZE;
    } else {
        return get_default_cursor_shape();
    }
}
void AnimationTimelineEdit::set_use_fps(bool p_use_fps) {
    use_fps = p_use_fps;
    update_values();
    update();
}
bool AnimationTimelineEdit::is_using_fps() const {
    return use_fps;
}

void AnimationTimelineEdit::set_hscroll(HScrollBar *p_hscroll) {

    hscroll = p_hscroll;
}

void AnimationTimelineEdit::_track_added(int p_track) {
    emit_signal("track_added", p_track);
}

void AnimationTimelineEdit::_bind_methods() {
    MethodBinder::bind_method("_anim_length_changed", &AnimationTimelineEdit::_anim_length_changed);
    MethodBinder::bind_method("_anim_loop_pressed", &AnimationTimelineEdit::_anim_loop_pressed);
    MethodBinder::bind_method("_gui_input", &AnimationTimelineEdit::_gui_input);
    MethodBinder::bind_method("_track_added", &AnimationTimelineEdit::_track_added);

    ADD_SIGNAL(MethodInfo("zoom_changed"));
    ADD_SIGNAL(MethodInfo("name_limit_changed"));
    ADD_SIGNAL(MethodInfo("timeline_changed", PropertyInfo(VariantType::FLOAT, "position"), PropertyInfo(VariantType::BOOL, "drag")));
    ADD_SIGNAL(MethodInfo("track_added", PropertyInfo(VariantType::INT, "track")));
    ADD_SIGNAL(MethodInfo("length_changed", PropertyInfo(VariantType::FLOAT, "size")));
}

AnimationTimelineEdit::AnimationTimelineEdit() {

    use_fps = false;
    editing = false;
    name_limit = 150 * EDSCALE;

    play_position_pos = 0;
    play_position = memnew(Control);
    play_position->set_mouse_filter(MOUSE_FILTER_PASS);
    add_child(play_position);
    play_position->set_anchors_and_margins_preset(PRESET_WIDE);
    play_position->connect("draw",callable_mp(this, &ClassName::_play_position_draw));

    add_track = memnew(MenuButton);
    add_track->set_position(Vector2(0, 0));
    add_child(add_track);
    add_track->set_text(TTR("Add Track"));

    len_hb = memnew(HBoxContainer);

    Control *expander = memnew(Control);
    expander->set_h_size_flags(SIZE_EXPAND_FILL);
    len_hb->add_child(expander);
    time_icon = memnew(TextureRect);
    time_icon->set_v_size_flags(SIZE_SHRINK_CENTER);
    time_icon->set_tooltip(TTR("Animation length (seconds)"));
    len_hb->add_child(time_icon);
    length = memnew(EditorSpinSlider);
    length->set_min(0.001);
    length->set_max(36000);
    length->set_step(0.001f);
    length->set_allow_greater(true);
    length->set_custom_minimum_size(Vector2(70 * EDSCALE, 0));
    length->set_hide_slider(true);
    length->set_tooltip(TTR("Animation length (seconds)"));
    length->connect("value_changed",callable_mp(this, &ClassName::_anim_length_changed));
    len_hb->add_child(length);
    loop = memnew(ToolButton);
    loop->set_tooltip(TTR("Animation Looping"));
    loop->connect("pressed",callable_mp(this, &ClassName::_anim_loop_pressed));
    loop->set_toggle_mode(true);
    len_hb->add_child(loop);
    add_child(len_hb);

    add_track->hide();
    add_track->get_popup()->connect("index_pressed",callable_mp(this, &ClassName::_track_added));
    len_hb->hide();

    panning_timeline = false;
    dragging_timeline = false;
    dragging_hsize = false;
}

////////////////////////////////////

void AnimationTrackEdit::_notification(int p_what) {
    if (p_what == NOTIFICATION_THEME_CHANGED) {
        if (!animation) {
            return;
        }
        ERR_FAIL_INDEX(track, animation->get_track_count());

        type_icon = _get_key_type_icon();
        selected_icon = get_theme_icon("KeySelected", "EditorIcons");
    }

    if (p_what == NOTIFICATION_DRAW) {

        if (not animation)
            return;
        ERR_FAIL_INDEX(track, animation->get_track_count());

        int limit = timeline->get_name_limit();

        if (has_focus()) {
            Color accent = get_theme_color("accent_color", "Editor");
            accent.a *= 0.7f;
            // Offside so the horizontal sides aren't cutoff.
            draw_rect_stroke(Rect2(Point2(1 * EDSCALE, 0), get_size() - Size2(1 * EDSCALE, 0)), accent);
        }

        Ref<Font> font = get_theme_font("font", "Label");
        Color color = get_theme_color("font_color", "Label");
        int hsep = get_theme_constant("hseparation", "ItemList");
        Color linecolor = color;
        linecolor.a = 0.2;

        // NAMES AND ICONS //

        {

            Ref<Texture> check = animation->track_is_enabled(track) ? get_theme_icon("checked", "CheckBox") : get_theme_icon("unchecked", "CheckBox");

            int ofs = in_group ? check->get_width() : 0; //not the best reference for margin but..

            check_rect = Rect2(Point2(ofs, int(get_size().height - check->get_height()) / 2), check->get_size());
            draw_texture(check, check_rect.position);
            ofs += check->get_width() + hsep;

            Ref<Texture> type_icon = _get_key_type_icon();
            draw_texture(type_icon, Point2(ofs, int(get_size().height - type_icon->get_height()) / 2));
            ofs += type_icon->get_width() + hsep;

            NodePath path = animation->track_get_path(track);
            Node *node = nullptr;
            if (root && root->has_node(path)) {
                node = root->get_node(path);
            }

            String text;
            Color text_color = color;
            if (node && EditorNode::get_singleton()->get_editor_selection()->is_selected(node)) {
                text_color = get_theme_color("accent_color", "Editor");
            }

            if (in_group) {

                if (animation->track_get_type(track) == Animation::TYPE_METHOD) {
                    text = TTR("Functions:");
                } else if (animation->track_get_type(track) == Animation::TYPE_AUDIO) {
                    text = TTR("Audio Clips:");
                } else if (animation->track_get_type(track) == Animation::TYPE_ANIMATION) {
                    text = TTR("Anim Clips:");
                } else {
                    text += path.get_concatenated_subnames();
                }
                text_color.a *= 0.7f;
            } else if (node) {
                Ref<Texture> icon = EditorNode::get_singleton()->get_object_icon(node, "Node");

                draw_texture(icon, Point2(ofs, int(get_size().height - icon->get_height()) / 2));
                icon_cache = icon;

                text = node->get_name() + ":" + path.get_concatenated_subnames();
                ofs += hsep;
                ofs += icon->get_width();

            } else {
                icon_cache = type_icon;

                text = (String)path;
            }

            path_cache = text;

            path_rect = Rect2(ofs, 0, limit - ofs - hsep, get_size().height);

            Vector2 string_pos = Point2(ofs, (get_size().height - font->get_height()) / 2 + font->get_ascent());
            string_pos = string_pos.floor();
            draw_ui_string(font, string_pos, StringUtils::from_utf8(text), text_color, limit - ofs - hsep);

            draw_line(Point2(limit, 0), Point2(limit, get_size().height), linecolor, Math::round(EDSCALE));
        }

        // KEYFRAMES //

        draw_bg(limit, get_size().width - timeline->get_buttons_width());

        {
            float scale = timeline->get_zoom_scale();
            int limit_end = get_size().width - timeline->get_buttons_width();

            for (int i = 0; i < animation->track_get_key_count(track); i++) {

                float offset = animation->track_get_key_time(track, i) - timeline->get_value();
                if (editor->is_key_selected(track, i) && editor->is_moving_selection()) {
                    offset = editor->snap_time(offset + editor->get_moving_selection_offset(), true);
                }
                offset = offset * scale + limit;
                if (i < animation->track_get_key_count(track) - 1) {

                    float offset_n = animation->track_get_key_time(track, i + 1) - timeline->get_value();
                    if (editor->is_key_selected(track, i + 1) && editor->is_moving_selection()) {
                        offset_n = editor->snap_time(offset_n + editor->get_moving_selection_offset());
                    }
                    offset_n = offset_n * scale + limit;

                    draw_key_link(i, scale, int(offset), int(offset_n), limit, limit_end);
                }

                draw_key(i, scale, int(offset), editor->is_key_selected(track, i), limit, limit_end);
            }
        }

        draw_fg(limit, get_size().width - timeline->get_buttons_width());

        // BUTTONS //

        {

            Ref<Texture> wrap_icon[2] = {
                get_theme_icon("InterpWrapClamp", "EditorIcons"),
                get_theme_icon("InterpWrapLoop", "EditorIcons"),
            };

            Ref<Texture> interp_icon[3] = {
                get_theme_icon("InterpRaw", "EditorIcons"),
                get_theme_icon("InterpLinear", "EditorIcons"),
                get_theme_icon("InterpCubic", "EditorIcons")
            };
            Ref<Texture> cont_icon[4] = {
                get_theme_icon("TrackContinuous", "EditorIcons"),
                get_theme_icon("TrackDiscrete", "EditorIcons"),
                get_theme_icon("TrackTrigger", "EditorIcons"),
                get_theme_icon("TrackCapture", "EditorIcons")
            };

            int ofs = get_size().width - timeline->get_buttons_width();

            Ref<Texture> down_icon = get_theme_icon("select_arrow", "Tree");

            draw_line(Point2(ofs, 0), Point2(ofs, get_size().height), linecolor, Math::round(EDSCALE));

            ofs += hsep;
            {
                //callmode

                Animation::UpdateMode update_mode;

                if (animation->track_get_type(track) == Animation::TYPE_VALUE) {
                    update_mode = animation->value_track_get_update_mode(track);
                } else {
                    update_mode = Animation::UPDATE_CONTINUOUS;
                }

                Ref<Texture> update_icon = cont_icon[update_mode];

                update_mode_rect.position.x = ofs;
                update_mode_rect.position.y = int(get_size().height - update_icon->get_height()) / 2;
                update_mode_rect.size = update_icon->get_size();

                if (animation->track_get_type(track) == Animation::TYPE_VALUE) {
                    draw_texture(update_icon, update_mode_rect.position);
                }
                //make it easier to click
                update_mode_rect.position.y = 0;
                update_mode_rect.size.y = get_size().height;

                ofs += update_icon->get_width() + hsep;
                update_mode_rect.size.x += hsep;

                if (animation->track_get_type(track) == Animation::TYPE_VALUE) {
                    draw_texture(down_icon, Vector2(ofs, int(get_size().height - down_icon->get_height()) / 2));
                    update_mode_rect.size.x += down_icon->get_width();
                    bezier_edit_rect = Rect2();
                } else if (animation->track_get_type(track) == Animation::TYPE_BEZIER) {
                    Ref<Texture> bezier_icon = get_theme_icon("EditBezier", "EditorIcons");
                    update_mode_rect.size.x += down_icon->get_width();
                    bezier_edit_rect.position = update_mode_rect.position + (update_mode_rect.size - bezier_icon->get_size()) / 2;
                    bezier_edit_rect.size = bezier_icon->get_size();
                    draw_texture(bezier_icon, bezier_edit_rect.position);
                    update_mode_rect = Rect2();
                } else {
                    update_mode_rect = Rect2();
                    bezier_edit_rect = Rect2();
                }

                ofs += down_icon->get_width();
                draw_line(Point2(ofs + hsep * 0.5f, 0), Point2(ofs + hsep * 0.5f, get_size().height), linecolor, Math::round(EDSCALE));
                ofs += hsep;
            }

            {
                //interp

                Animation::InterpolationType interp_mode = animation->track_get_interpolation_type(track);

                Ref<Texture> icon = interp_icon[interp_mode];

                interp_mode_rect.position.x = ofs;
                interp_mode_rect.position.y = int(get_size().height - icon->get_height()) / 2;
                interp_mode_rect.size = icon->get_size();

                if (animation->track_get_type(track) == Animation::TYPE_VALUE || animation->track_get_type(track) == Animation::TYPE_TRANSFORM) {
                    draw_texture(icon, interp_mode_rect.position);
                }
                //make it easier to click
                interp_mode_rect.position.y = 0;
                interp_mode_rect.size.y = get_size().height;

                ofs += icon->get_width() + hsep;
                interp_mode_rect.size.x += hsep;

                if (animation->track_get_type(track) == Animation::TYPE_VALUE || animation->track_get_type(track) == Animation::TYPE_TRANSFORM) {
                    draw_texture(down_icon, Vector2(ofs, int(get_size().height - down_icon->get_height()) / 2));
                    interp_mode_rect.size.x += down_icon->get_width();
                } else {
                    interp_mode_rect = Rect2();
                }

                ofs += down_icon->get_width();
                draw_line(Point2(ofs + hsep * 0.5f, 0), Point2(ofs + hsep * 0.5f, get_size().height), linecolor, Math::round(EDSCALE));
                ofs += hsep;
            }

            {
                //loop

                bool loop_wrap = animation->track_get_interpolation_loop_wrap(track);

                Ref<Texture> icon = wrap_icon[loop_wrap ? 1 : 0];

                loop_mode_rect.position.x = ofs;
                loop_mode_rect.position.y = int(get_size().height - icon->get_height()) / 2;
                loop_mode_rect.size = icon->get_size();

                if (animation->track_get_type(track) == Animation::TYPE_VALUE || animation->track_get_type(track) == Animation::TYPE_TRANSFORM) {
                    draw_texture(icon, loop_mode_rect.position);
                }

                loop_mode_rect.position.y = 0;
                loop_mode_rect.size.y = get_size().height;

                ofs += icon->get_width() + hsep;
                loop_mode_rect.size.x += hsep;

                if (animation->track_get_type(track) == Animation::TYPE_VALUE || animation->track_get_type(track) == Animation::TYPE_TRANSFORM) {
                    draw_texture(down_icon, Vector2(ofs, int(get_size().height - down_icon->get_height()) / 2));
                    loop_mode_rect.size.x += down_icon->get_width();
                } else {
                    loop_mode_rect = Rect2();
                }

                ofs += down_icon->get_width();
                draw_line(Point2(ofs + hsep * 0.5f, 0), Point2(ofs + hsep * 0.5f, get_size().height), linecolor, Math::round(EDSCALE));
                ofs += hsep;
            }

            {
                //erase

                Ref<Texture> icon = get_theme_icon("Remove", "EditorIcons");

                remove_rect.position.x = ofs + (get_size().width - ofs - icon->get_width()) / 2;
                remove_rect.position.y = int(get_size().height - icon->get_height()) / 2;
                remove_rect.size = icon->get_size();

                draw_texture(icon, remove_rect.position);
            }
        }

        if (in_group) {
            draw_line(Vector2(timeline->get_name_limit(), get_size().height), get_size(), linecolor, Math::round(EDSCALE));
        } else {
            draw_line(Vector2(0, get_size().height), get_size(), linecolor, Math::round(EDSCALE));
        }

        if (dropping_at != 0) {
            Color drop_color = get_theme_color("accent_color", "Editor");
            if (dropping_at < 0) {
                draw_line(Vector2(0, 0), Vector2(get_size().width, 0), drop_color, Math::round(EDSCALE));
            } else {
                draw_line(Vector2(0, get_size().height), get_size(), drop_color, Math::round(EDSCALE));
            }
        }
    }

    if (p_what == NOTIFICATION_MOUSE_EXIT || p_what == NOTIFICATION_DRAG_END) {
        cancel_drop();
    }
}

int AnimationTrackEdit::get_key_height() const {
    if (not animation)
        return 0;

    return type_icon->get_height();
}
Rect2 AnimationTrackEdit::get_key_rect(int p_index, float p_pixels_sec) {

    if (not animation)
        return Rect2();
    Rect2 rect = Rect2(-type_icon->get_width() / 2, 0, type_icon->get_width(), get_size().height);

    //make it a big easier to click
    rect.position.x -= rect.size.x * 0.5f;
    rect.size.x *= 2;
    return rect;
}

bool AnimationTrackEdit::is_key_selectable_by_distance() const {
    return true;
}

void AnimationTrackEdit::draw_key_link(int p_index, float p_pixels_sec, int p_x, int p_next_x, int p_clip_left, int p_clip_right) {
    if (p_next_x < p_clip_left)
        return;
    if (p_x > p_clip_right)
        return;

    Variant current = animation->track_get_key_value(get_track(), p_index);
    Variant next = animation->track_get_key_value(get_track(), p_index + 1);
    if (current != next)
        return;

    Color color = get_theme_color("font_color", "Label");
    color.a = 0.5;

    int from_x = M_MAX(p_x, p_clip_left);
    int to_x = MIN(p_next_x, p_clip_right);

    draw_line(Point2(from_x + 1, get_size().height / 2), Point2(to_x, get_size().height / 2), color, Math::round(2 * EDSCALE));
}

void AnimationTrackEdit::draw_key(int p_index, float p_pixels_sec, int p_x, bool p_selected, int p_clip_left, int p_clip_right) {

    if (not animation)
        return;

    if (p_x < p_clip_left || p_x > p_clip_right)
        return;

    Ref<Texture> icon_to_draw = p_selected ? selected_icon : type_icon;

    if (animation->track_get_type(track) == Animation::TYPE_VALUE && !Math::is_equal_approx(animation->track_get_key_transition(track, p_index), 1.0f)) {
        // Use a different icon for keys with non-linear easing.
        icon_to_draw = get_theme_icon(StringName(p_selected ? "KeyEasedSelected" : "KeyValueEased"), "EditorIcons");
    }

    // Override type icon for invalid value keys, unless selected.
    if (!p_selected && animation->track_get_type(track) == Animation::TYPE_VALUE) {
        const Variant &v = animation->track_get_key_value(track, p_index);
        VariantType valid_type = VariantType::NIL;
        if (!_is_value_key_valid(v, valid_type)) {
            icon_to_draw = get_theme_icon("KeyInvalid", "EditorIcons");
        }
    }

    Vector2 ofs(p_x - icon_to_draw->get_width() / 2, int(get_size().height - icon_to_draw->get_height()) / 2);

    if (animation->track_get_type(track) == Animation::TYPE_METHOD) {
        Ref<Font> font = get_theme_font("font", "Label");
        Color color = get_theme_color("font_color", "Label");
        color.a = 0.5;

        Dictionary d = animation->track_get_key_value(track, p_index).as<Dictionary>();
        String text;

        if (d.has("method"))
            text += d["method"].as<String>();
        text += '(';
        Array args;
        if (d.has("args"))
            args = d["args"].as<Array>();
        for (int i = 0; i < args.size(); i++) {

            if (i > 0)
                text += ", ";
            text += args[i].as<String>();
        }
        text += ")";

        int limit = M_MAX(0, p_clip_right - p_x - icon_to_draw->get_width());
        if (limit > 0) {
            draw_string(font, Vector2(p_x + icon_to_draw->get_width(), int(get_size().height - font->get_height()) / 2 + font->get_ascent()), text, color, limit);
        }
    }

    draw_texture(icon_to_draw, ofs);
}

//helper
void AnimationTrackEdit::draw_rect_clipped(const Rect2 &p_rect, const Color &p_color, bool p_filled) {

    int clip_left = timeline->get_name_limit();
    int clip_right = get_size().width - timeline->get_buttons_width();

    if (p_rect.position.x > clip_right) {
        return;
    }
    if (p_rect.position.x + p_rect.size.x < clip_left) {
        return;
    }
    Rect2 clip = Rect2(clip_left, 0, clip_right - clip_left, get_size().height);
    if (p_filled)
        draw_rect_filled(clip.clip(p_rect), p_color);
    else
        draw_rect_stroke(clip.clip(p_rect), p_color);
}

void AnimationTrackEdit::draw_bg(int p_clip_left, int p_clip_right) {
}

void AnimationTrackEdit::draw_fg(int p_clip_left, int p_clip_right) {
}

void AnimationTrackEdit::draw_texture_clipped(const Ref<Texture> &p_texture, const Vector2 &p_pos) {

    draw_texture_region_clipped(p_texture, Rect2(p_pos, p_texture->get_size()), Rect2(Point2(), p_texture->get_size()));
}

void AnimationTrackEdit::draw_texture_region_clipped(const Ref<Texture> &p_texture, const Rect2 &p_rect, const Rect2 &p_region) {

    int clip_left = timeline->get_name_limit();
    int clip_right = get_size().width - timeline->get_buttons_width();

    //clip left and right
    if (clip_right < p_rect.position.x)
        return;
    if (p_rect.position.x + p_rect.size.x < clip_left)
        return;

    Rect2 rect = p_rect;
    Rect2 region = p_region;

    if (clip_left > rect.position.x) {
        int rect_pixels = clip_left - rect.position.x;
        int region_pixels = rect_pixels * region.size.x / rect.size.x;

        rect.position.x += rect_pixels;
        rect.size.x -= rect_pixels;

        region.position.x += region_pixels;
        region.size.x -= region_pixels;
    }

    if (clip_right < rect.position.x + rect.size.x) {

        int rect_pixels = rect.position.x + rect.size.x - clip_right;
        int region_pixels = rect_pixels * region.size.x / rect.size.x;

        rect.size.x -= rect_pixels;
        region.size.x -= region_pixels;
    }

    draw_texture_rect_region(p_texture, rect, region);
}

int AnimationTrackEdit::get_track() const {
    return track;
}

Ref<Animation> AnimationTrackEdit::get_animation() const {
    return animation;
}

void AnimationTrackEdit::set_animation_and_track(const Ref<Animation> &p_animation, int p_track) {

    animation = p_animation;
    track = p_track;
    update();


    ERR_FAIL_INDEX(track, animation->get_track_count());

    node_path = animation->track_get_path(p_track);
    type_icon = _get_key_type_icon();
    selected_icon = get_theme_icon("KeySelected", "EditorIcons");
}

NodePath AnimationTrackEdit::get_path() const {
    return node_path;
}

Size2 AnimationTrackEdit::get_minimum_size() const {

    Ref<Texture> texture = get_theme_icon("Object", "EditorIcons");
    Ref<Font> font = get_theme_font("font", "Label");
    int separation = get_theme_constant("vseparation", "ItemList");

    int max_h = M_MAX(texture->get_height(), font->get_height());
    max_h = M_MAX(max_h, get_key_height());

    return Vector2(1, max_h + separation);
}

void AnimationTrackEdit::set_undo_redo(UndoRedo *p_undo_redo) {
    undo_redo = p_undo_redo;
}

void AnimationTrackEdit::set_timeline(AnimationTimelineEdit *p_timeline) {
    timeline = p_timeline;
    timeline->set_track_edit(this);
    timeline->connect("zoom_changed",callable_mp(this, &ClassName::_zoom_changed));
    timeline->connect("name_limit_changed",callable_mp(this, &ClassName::_zoom_changed));
}
void AnimationTrackEdit::set_editor(AnimationTrackEditor *p_editor) {
    editor = p_editor;
}

void AnimationTrackEdit::_play_position_draw() {

    if (not animation || play_position_pos < 0)
        return;

    float scale = timeline->get_zoom_scale();
    int h = get_size().height;

    int px = (-timeline->get_value() + play_position_pos) * scale + timeline->get_name_limit();

    if (px >= timeline->get_name_limit() && px < get_size().width - timeline->get_buttons_width()) {
        Color color = get_theme_color("accent_color", "Editor");
        play_position->draw_line(Point2(px, 0), Point2(px, h), color, Math::round(2 * EDSCALE));
    }
}

void AnimationTrackEdit::set_play_position(float p_pos) {

    play_position_pos = p_pos;
    play_position->update();
}

void AnimationTrackEdit::update_play_position() {
    play_position->update();
}

void AnimationTrackEdit::set_root(Node *p_root) {
    root = p_root;
}
void AnimationTrackEdit::_zoom_changed() {
    update();
    play_position->update();
}

void AnimationTrackEdit::_path_entered(StringView p_text) {
    undo_redo->create_action(TTR("Change Track Path"));
    undo_redo->add_do_method(animation.get(), "track_set_path", track, p_text);
    undo_redo->add_undo_method(animation.get(), "track_set_path", track, animation->track_get_path(track));
    undo_redo->commit_action();
}

bool AnimationTrackEdit::_is_value_key_valid(const Variant &p_key_value, VariantType &r_valid_type) const {

    if (root == nullptr)
        return false;

    RES res;
    Vector<StringName> leftover_path;
    Node *node = root->get_node_and_resource(animation->track_get_path(track), res, leftover_path);

    Object *obj = nullptr;
    if (res) {
        obj = res.get();
    } else if (node) {
        obj = node;
    }

    bool prop_exists = false;
    if (obj) {
        r_valid_type = obj->get_static_property_type_indexed(leftover_path, &prop_exists);
    }

    return !prop_exists || Variant::can_convert(p_key_value.get_type(), r_valid_type);
}

Ref<Texture> AnimationTrackEdit::_get_key_type_icon() const {
    Ref<Texture> type_icons[6] = {
        get_theme_icon("KeyValue", "EditorIcons"),
        get_theme_icon("KeyXform", "EditorIcons"),
        get_theme_icon("KeyCall", "EditorIcons"),
        get_theme_icon("KeyBezier", "EditorIcons"),
        get_theme_icon("KeyAudio", "EditorIcons"),
        get_theme_icon("KeyAnimation", "EditorIcons")
    };
    return type_icons[animation->track_get_type(track)];
}

const String &AnimationTrackEdit::get_tooltip(const Point2 &p_pos) const {
    static String tooltip_contents;
    if (check_rect.has_point(p_pos)) {
        tooltip_contents = TTR("Toggle this track on/off.");
    } else if (path_rect.has_point(p_pos + Size2(type_icon->get_width(), 0))) {
        // Don't overlap track keys if they start at 0.
        tooltip_contents = String(animation->track_get_path(track));
    } else if (update_mode_rect.has_point(p_pos)) {
        tooltip_contents = TTR("Update Mode (How this property is set)");
    }

    else if (interp_mode_rect.has_point(p_pos)) {
        tooltip_contents = TTR("Interpolation Mode");
    }

    else if (loop_mode_rect.has_point(p_pos)) {
        tooltip_contents = TTR("Loop Wrap Mode (Interpolate end with beginning on loop)");
    }

    else if (remove_rect.has_point(p_pos)) {
        tooltip_contents = TTR("Remove this track.");
    } else {
        int limit = timeline->get_name_limit();
        int limit_end = get_size().width - timeline->get_buttons_width();
        // Left Border including space occupied by keyframes on t=0.
        int limit_start_hitbox = limit - type_icon->get_width();

        if (p_pos.x >= limit_start_hitbox && p_pos.x <= limit_end) {
            int key_idx = -1;
            float key_distance = 1e20f;

            // Select should happen in the opposite order of drawing for more accurate overlap select.
            for (int i = animation->track_get_key_count(track) - 1; i >= 0; i--) {
                Rect2 rect = const_cast<AnimationTrackEdit *>(this)->get_key_rect(i, timeline->get_zoom_scale());
                float offset = animation->track_get_key_time(track, i) - timeline->get_value();
                offset = offset * timeline->get_zoom_scale() + limit;
                rect.position.x += offset;

                if (rect.has_point(p_pos)) {
                    if (const_cast<AnimationTrackEdit *>(this)->is_key_selectable_by_distance()) {
                        float distance = ABS(offset - p_pos.x);
                        if (key_idx == -1 || distance < key_distance) {
                            key_idx = i;
                            key_distance = distance;
                        }
                    } else {
                        // first one does it
                        break;
                    }
                }
            }

            if (key_idx != -1) {
                String text(TTR("Time (s): ") + rtos(animation->track_get_key_time(track, key_idx)) + "\n");
                switch (animation->track_get_type(track)) {
                    case Animation::TYPE_TRANSFORM: {
                        Dictionary d = animation->track_get_key_value(track, key_idx).as<Dictionary>();
                        if (d.has("location"))
                            text += "Pos: " + d["location"].as<String>() + "\n";
                        if (d.has("rotation"))
                            text += "Rot: " + d["rotation"].as<String>() + "\n";
                        if (d.has("scale"))
                            text += "Scale: " + d["scale"].as<String>() + "\n";
                    } break;
                    case Animation::TYPE_VALUE: {
                        const Variant &v = animation->track_get_key_value(track, key_idx);
                        text += String("Type: ") + Variant::get_type_name(v.get_type()) + "\n";
                        VariantType valid_type = VariantType::NIL;
                        if (!_is_value_key_valid(v, valid_type)) {
                            text += "Value: " + v.as<String>() + "  (Invalid, expected type: " + Variant::interned_type_name(valid_type) + ")\n";
                        } else {
                            text += "Value: " + v.as<String>() + "\n";
                        }
                        text += "Easing: " + rtos(animation->track_get_key_transition(track, key_idx));

                    } break;
                    case Animation::TYPE_METHOD: {
                        Dictionary d = animation->track_get_key_value(track, key_idx).as<Dictionary>();
                        if (d.has("method"))
                            text += d["method"].as<String>();
                        text += '(';
                        Array args;
                        if (d.has("args"))
                            args = d["args"].as<Array>();
                        for (int i = 0; i < args.size(); i++) {
                            if (i > 0)
                                text += ", ";
                            text += args[i].as<String>();
                        }
                        text += String(")\n");

                    } break;
                    case Animation::TYPE_BEZIER: {
                        float h = animation->bezier_track_get_key_value(track, key_idx);
                        text += "Value: " + rtos(h) + "\n";
                        Vector2 ih = animation->bezier_track_get_key_in_handle(track, key_idx);
                        text += "In-Handle: " + (String)ih + "\n";
                        Vector2 oh = animation->bezier_track_get_key_out_handle(track, key_idx);
                        text += "Out-Handle: " + (String)oh + "\n";
                    } break;
                    case Animation::TYPE_AUDIO: {
                        String stream_name("null");
                        RES stream(animation->audio_track_get_key_stream(track, key_idx));
                        if (stream) {
                            if (PathUtils::is_resource_file(stream->get_path())) {
                                stream_name = PathUtils::get_file(stream->get_path());
                            } else if (!stream->get_name().empty()) {
                                stream_name = stream->get_name();
                            } else {
                                stream_name = stream->get_class();
                            }
                        }

                        text += "Stream: " + stream_name + "\n";
                        float so = animation->audio_track_get_key_start_offset(track, key_idx);
                        text += "Start (s): " + rtos(so) + "\n";
                        float eo = animation->audio_track_get_key_end_offset(track, key_idx);
                        text += "End (s): " + rtos(eo) + "\n";
                    } break;
                    case Animation::TYPE_ANIMATION: {
                        StringName name = animation->animation_track_get_key_animation(track, key_idx);
                        text += String("Animation Clip: ") + name + "\n";
                    } break;
                }
                tooltip_contents = text;
            } else {
                return Control::get_tooltip(p_pos);
            }
        } else {
            return Control::get_tooltip(p_pos);
        }
    }
    return tooltip_contents;
}

void AnimationTrackEdit::_gui_input(const Ref<InputEvent> &p_event) {
    ERR_FAIL_COND(!p_event);
    if (p_event->is_pressed()) {
        if (ED_GET_SHORTCUT("animation_editor/duplicate_selection")->is_shortcut(p_event)) {
            emit_signal("duplicate_request");
            accept_event();
        }

        if (ED_GET_SHORTCUT("animation_editor/duplicate_selection_transposed")->is_shortcut(p_event)) {
            emit_signal("duplicate_transpose_request");
            accept_event();
        }

        if (ED_GET_SHORTCUT("animation_editor/delete_selection")->is_shortcut(p_event)) {
            emit_signal("delete_request");
            accept_event();
        }
    }

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);
    if (mb && mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
        Point2 pos = mb->get_position();

        if (check_rect.has_point(pos)) {
            undo_redo->create_action(TTR("Toggle Track Enabled"));
            undo_redo->add_do_method(animation.get(), "track_set_enabled", track, !animation->track_is_enabled(track));
            undo_redo->add_undo_method(animation.get(), "track_set_enabled", track, animation->track_is_enabled(track));
            undo_redo->commit_action();
            update();
            accept_event();
        }

        // Don't overlap track keys if they start at 0.
        if (path_rect.has_point(pos + Size2(type_icon->get_width(), 0))) {
            clicking_on_name = true;
            accept_event();
        }

        if (update_mode_rect.has_point(pos)) {
            if (!menu) {
                menu = memnew(PopupMenu);
                add_child(menu);
                menu->connect("id_pressed",callable_mp(this, &ClassName::_menu_selected));
            }
            menu->clear();
            menu->add_icon_item(get_theme_icon("TrackContinuous", "EditorIcons"), TTR("Continuous"), MENU_CALL_MODE_CONTINUOUS);
            menu->add_icon_item(get_theme_icon("TrackDiscrete", "EditorIcons"), TTR("Discrete"), MENU_CALL_MODE_DISCRETE);
            menu->add_icon_item(get_theme_icon("TrackTrigger", "EditorIcons"), TTR("Trigger"), MENU_CALL_MODE_TRIGGER);
            menu->add_icon_item(get_theme_icon("TrackCapture", "EditorIcons"), TTR("Capture"), MENU_CALL_MODE_CAPTURE);
            menu->set_as_minsize();

            Vector2 popup_pos = get_global_position() + update_mode_rect.position + Vector2(0, update_mode_rect.size.height);
            menu->set_global_position(popup_pos);
            menu->popup();
            accept_event();
        }

        if (interp_mode_rect.has_point(pos)) {
            if (!menu) {
                menu = memnew(PopupMenu);
                add_child(menu);
                menu->connect("id_pressed",callable_mp(this, &ClassName::_menu_selected));
            }
            menu->clear();
            menu->add_icon_item(get_theme_icon("InterpRaw", "EditorIcons"), TTR("Nearest"), MENU_INTERPOLATION_NEAREST);
            menu->add_icon_item(get_theme_icon("InterpLinear", "EditorIcons"), TTR("Linear"), MENU_INTERPOLATION_LINEAR);
            menu->add_icon_item(get_theme_icon("InterpCubic", "EditorIcons"), TTR("Cubic"), MENU_INTERPOLATION_CUBIC);
            menu->set_as_minsize();

            Vector2 popup_pos = get_global_position() + interp_mode_rect.position + Vector2(0, interp_mode_rect.size.height);
            menu->set_global_position(popup_pos);
            menu->popup();
            accept_event();
        }

        if (loop_mode_rect.has_point(pos)) {
            if (!menu) {
                menu = memnew(PopupMenu);
                add_child(menu);
                menu->connect("id_pressed",callable_mp(this, &ClassName::_menu_selected));
            }
            menu->clear();
            menu->add_icon_item(get_theme_icon("InterpWrapClamp", "EditorIcons"), TTR("Clamp Loop Interp"), MENU_LOOP_CLAMP);
            menu->add_icon_item(get_theme_icon("InterpWrapLoop", "EditorIcons"), TTR("Wrap Loop Interp"), MENU_LOOP_WRAP);
            menu->set_as_minsize();

            Vector2 popup_pos = get_global_position() + loop_mode_rect.position + Vector2(0, loop_mode_rect.size.height);
            menu->set_global_position(popup_pos);
            menu->popup();
            accept_event();
        }

        if (remove_rect.has_point(pos)) {
            emit_signal("remove_request", track);
            accept_event();
            return;
        }

        if (bezier_edit_rect.has_point(pos)) {
            emit_signal("bezier_edit");
            accept_event();
        }

        // Check keyframes.

        float scale = timeline->get_zoom_scale();
        int limit = timeline->get_name_limit();
        int limit_end = get_size().width - timeline->get_buttons_width();
        // Left Border including space occupied by keyframes on t=0.
        int limit_start_hitbox = limit - type_icon->get_width();

        if (pos.x >= limit_start_hitbox && pos.x <= limit_end) {

            int key_idx = -1;
            float key_distance = 1e20;

            // Select should happen in the opposite order of drawing for more accurate overlap select.
            for (int i = animation->track_get_key_count(track) - 1; i >= 0; i--) {

                Rect2 rect = get_key_rect(i, scale);
                float offset = animation->track_get_key_time(track, i) - timeline->get_value();
                offset = offset * scale + limit;
                rect.position.x += offset;

                if (rect.has_point(pos)) {

                    if (is_key_selectable_by_distance()) {
                        float distance = ABS(offset - pos.x);
                        if (key_idx == -1 || distance < key_distance) {
                            key_idx = i;
                            key_distance = distance;
                        }
                    } else {
                        // First one does it.
                        key_idx = i;
                        break;
                    }
                }
            }

            if (key_idx != -1) {
                if (mb->get_command() || mb->get_shift()) {
                    if (editor->is_key_selected(track, key_idx)) {
                        emit_signal("deselect_key", key_idx);
                    } else {
                        emit_signal("select_key", key_idx, false);
                        moving_selection_attempt = true;
                        select_single_attempt = -1;
                        moving_selection_from_ofs = (mb->get_position().x - limit) / timeline->get_zoom_scale();
                    }
                } else {
                    if (!editor->is_key_selected(track, key_idx)) {
                        emit_signal("select_key", key_idx, true);
                        select_single_attempt = -1;
                    } else {
                        select_single_attempt = key_idx;
                    }

                    moving_selection_attempt = true;
                    moving_selection_from_ofs = (mb->get_position().x - limit) / timeline->get_zoom_scale();
                }
                accept_event();
            }
        }
    }

    if (mb && mb->is_pressed() && mb->get_button_index() == BUTTON_RIGHT) {
        Point2 pos = mb->get_position();
        if (pos.x >= timeline->get_name_limit() && pos.x <= get_size().width - timeline->get_buttons_width()) {
            // Can do something with menu too! show insert key.
            float offset = (pos.x - timeline->get_name_limit()) / timeline->get_zoom_scale();
            if (!menu) {
                menu = memnew(PopupMenu);
                add_child(menu);
                menu->connect("id_pressed",callable_mp(this, &ClassName::_menu_selected));
            }

            menu->clear();
            menu->add_icon_item(get_theme_icon("Key", "EditorIcons"), TTR("Insert Key"), MENU_KEY_INSERT);
            if (editor->is_selection_active()) {
                menu->add_separator();
                menu->add_icon_item(get_theme_icon("Duplicate", "EditorIcons"), TTR("Duplicate Key(s)"), MENU_KEY_DUPLICATE);
                AnimationPlayer *player = AnimationPlayerEditor::singleton->get_player();
                if (!player->has_animation("RESET") || animation != player->get_animation("RESET")) {
                    menu->add_icon_item(get_theme_icon("Reload", "EditorIcons"), TTR("Add RESET Value(s)"), MENU_KEY_ADD_RESET);
                }
                menu->add_separator();
                menu->add_icon_item(get_theme_icon("Remove", "EditorIcons"), TTR("Delete Key(s)"), MENU_KEY_DELETE);
            }
            menu->set_as_minsize();

            Vector2 popup_pos = get_global_transform().xform(get_local_mouse_position());
            menu->set_global_position(popup_pos);
            menu->popup();

            insert_at_pos = offset + timeline->get_value();
            accept_event();
        }
    }

    if (mb && !mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT && clicking_on_name) {

        if (!path) {
            path = memnew(LineEdit);
            add_child(path);
            path->set_as_top_level(true);
            path->connect("text_entered",callable_mp(this, &ClassName::_path_entered));
        }

        path->set_text((String)animation->track_get_path(track));
        Vector2 theme_ofs = path->get_theme_stylebox("normal", "LineEdit")->get_offset();
        path->set_position(get_global_position() + path_rect.position - theme_ofs);
        path->set_size(path_rect.size);
        path->show_modal();
        path->grab_focus();
        path->set_cursor_position(path->get_text_ui().length());
        clicking_on_name = false;
    }

    if (mb && moving_selection_attempt) {

        if (!mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
            moving_selection_attempt = false;
            if (moving_selection) {
                emit_signal("move_selection_commit");
            } else if (select_single_attempt != -1) {
                emit_signal("select_key", select_single_attempt, true);
            }
            moving_selection = false;
            select_single_attempt = -1;
        }

        if (moving_selection && mb->is_pressed() && mb->get_button_index() == BUTTON_RIGHT) {

            moving_selection_attempt = false;
            moving_selection = false;
            emit_signal("move_selection_cancel");
        }
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);
    if (mm && mm->get_button_mask() & BUTTON_MASK_LEFT && moving_selection_attempt) {

        if (!moving_selection) {
            moving_selection = true;
            emit_signal("move_selection_begin");
        }

        float new_ofs = (mm->get_position().x - timeline->get_name_limit()) / timeline->get_zoom_scale();
        emit_signal("move_selection", new_ofs - moving_selection_from_ofs);
    }
}

Variant AnimationTrackEdit::get_drag_data(const Point2 &p_point) {

    if (!clicking_on_name)
        return Variant();

    Dictionary drag_data;
    drag_data["type"] = "animation_track";
    String base_path(animation->track_get_path(track));
    base_path = StringUtils::get_slice(base_path,":", 0); // Remove sub-path.
    drag_data["group"] = base_path;
    drag_data["index"] = track;

    ToolButton *tb = memnew(ToolButton);
    tb->set_text(path_cache);
    tb->set_button_icon(icon_cache);
    set_drag_preview(tb);

    clicking_on_name = false;

    return drag_data;
}

bool AnimationTrackEdit::can_drop_data(const Point2 &p_point, const Variant &p_data) const {

    Dictionary d = p_data.as<Dictionary>();
    if (!d.has("type")) {
        return false;
    }

    UIString type= d["type"].as<UIString>();
    if (type != "animation_track") {
        return false;
    }

    // Don't allow moving tracks outside their groups.
    if (get_editor()->is_grouping_tracks()) {
        String base_path(animation->track_get_path(track));
        base_path = StringUtils::get_slice(base_path,":", 0); // Remove sub-path.
        if (d["group"].as<String>() != base_path) {
            return false;
        }
    }

    if (p_point.y < get_size().height / 2) {
        dropping_at = -1;
    } else {
        dropping_at = 1;
    }

    const_cast<AnimationTrackEdit *>(this)->update();
    const_cast<AnimationTrackEdit *>(this)->emit_signal("drop_attempted", track);

    return true;
}
void AnimationTrackEdit::drop_data(const Point2 &p_point, const Variant &p_data) {

    Dictionary d = p_data.as<Dictionary>();
    if (!d.has("type")) {
        return;
    }

    UIString type= d["type"].as<UIString>();
    if (type != "animation_track") {
        return;
    }

    // Don't allow moving tracks outside their groups.
    if (get_editor()->is_grouping_tracks()) {
        String base_path(animation->track_get_path(track));
        base_path = StringUtils::get_slice(base_path,":", 0); // Remove sub-path.
        if (d["group"].as<String>() != base_path) {
            return;
        }
    }

    int from_track= d["index"].as<int>();

    if (dropping_at < 0) {
        emit_signal("dropped", from_track, track);
    } else {
        emit_signal("dropped", from_track, track + 1);
    }
}

void AnimationTrackEdit::_menu_selected(int p_index) {

    switch (p_index) {
        case MENU_CALL_MODE_CONTINUOUS:
        case MENU_CALL_MODE_DISCRETE:
        case MENU_CALL_MODE_TRIGGER:
        case MENU_CALL_MODE_CAPTURE: {

            Animation::UpdateMode update_mode = Animation::UpdateMode(p_index);
            undo_redo->create_action(TTR("Change Animation Update Mode"));
            undo_redo->add_do_method(animation.get(), "value_track_set_update_mode", track, update_mode);
            undo_redo->add_undo_method(animation.get(), "value_track_set_update_mode", track, animation->value_track_get_update_mode(track));
            undo_redo->commit_action();
            update();

        } break;
        case MENU_INTERPOLATION_NEAREST:
        case MENU_INTERPOLATION_LINEAR:
        case MENU_INTERPOLATION_CUBIC: {

            Animation::InterpolationType interp_mode = Animation::InterpolationType(p_index - MENU_INTERPOLATION_NEAREST);
            undo_redo->create_action(TTR("Change Animation Interpolation Mode"));
            undo_redo->add_do_method(animation.get(), "track_set_interpolation_type", track, interp_mode);
            undo_redo->add_undo_method(animation.get(), "track_set_interpolation_type", track, animation->track_get_interpolation_type(track));
            undo_redo->commit_action();
            update();
        } break;
        case MENU_LOOP_WRAP:
        case MENU_LOOP_CLAMP: {

            bool loop_wrap = p_index == MENU_LOOP_WRAP;
            undo_redo->create_action(TTR("Change Animation Loop Mode"));
            undo_redo->add_do_method(animation.get(), "track_set_interpolation_loop_wrap", track, loop_wrap);
            undo_redo->add_undo_method(animation.get(), "track_set_interpolation_loop_wrap", track, animation->track_get_interpolation_loop_wrap(track));
            undo_redo->commit_action();
            update();

        } break;
        case MENU_KEY_INSERT: {
            emit_signal("insert_key", insert_at_pos);
        } break;
        case MENU_KEY_DUPLICATE: {
            emit_signal("duplicate_request");

        } break;
        case MENU_KEY_ADD_RESET: {
            emit_signal("create_reset_request");
        } break;
        case MENU_KEY_DELETE: {
            emit_signal("delete_request");

        } break;
    }
}

void AnimationTrackEdit::cancel_drop() {
    if (dropping_at != 0) {
        dropping_at = 0;
        update();
    }
}
void AnimationTrackEdit::set_in_group(bool p_enable) {
    in_group = p_enable;
    update();
}

void AnimationTrackEdit::append_to_selection(const Rect2 &p_box, bool p_deselection) {
    // Left Border including space occupied by keyframes on t=0.
    int limit_start_hitbox = timeline->get_name_limit() - type_icon->get_width();
    Rect2 select_rect(limit_start_hitbox, 0, get_size().width - timeline->get_name_limit() - timeline->get_buttons_width(), get_size().height);
    select_rect = select_rect.clip(p_box);

    // Select should happen in the opposite order of drawing for more accurate overlap select.
    for (int i = animation->track_get_key_count(track) - 1; i >= 0; i--) {

        Rect2 rect = const_cast<AnimationTrackEdit *>(this)->get_key_rect(i, timeline->get_zoom_scale());
        float offset = animation->track_get_key_time(track, i) - timeline->get_value();
        offset = offset * timeline->get_zoom_scale() + timeline->get_name_limit();
        rect.position.x += offset;

        if (select_rect.intersects(rect)) {
            if (p_deselection)
                emit_signal("deselect_key", i);
            else
                emit_signal("select_key", i, false);
        }
    }
}

void AnimationTrackEdit::_bind_methods() {

    MethodBinder::bind_method("_gui_input", &AnimationTrackEdit::_gui_input);

    ADD_SIGNAL(MethodInfo("timeline_changed", PropertyInfo(VariantType::FLOAT, "position"), PropertyInfo(VariantType::BOOL, "drag")));
    ADD_SIGNAL(MethodInfo("remove_request", PropertyInfo(VariantType::INT, "track")));
    ADD_SIGNAL(MethodInfo("dropped", PropertyInfo(VariantType::INT, "from_track"), PropertyInfo(VariantType::INT, "to_track")));
    ADD_SIGNAL(MethodInfo("insert_key", PropertyInfo(VariantType::FLOAT, "ofs")));
    ADD_SIGNAL(MethodInfo("select_key", PropertyInfo(VariantType::INT, "index"), PropertyInfo(VariantType::BOOL, "single")));
    ADD_SIGNAL(MethodInfo("deselect_key", PropertyInfo(VariantType::INT, "index")));
    ADD_SIGNAL(MethodInfo("bezier_edit"));

    ADD_SIGNAL(MethodInfo("move_selection_begin"));
    ADD_SIGNAL(MethodInfo("move_selection", PropertyInfo(VariantType::FLOAT, "ofs")));
    ADD_SIGNAL(MethodInfo("move_selection_commit"));
    ADD_SIGNAL(MethodInfo("move_selection_cancel"));

    ADD_SIGNAL(MethodInfo("duplicate_request"));
    ADD_SIGNAL(MethodInfo("create_reset_request"));
    ADD_SIGNAL(MethodInfo("duplicate_transpose_request"));
    ADD_SIGNAL(MethodInfo("delete_request"));
}

AnimationTrackEdit::AnimationTrackEdit() {
    undo_redo = nullptr;
    timeline = nullptr;
    root = nullptr;
    path = nullptr;
    menu = nullptr;
    clicking_on_name = false;
    dropping_at = 0;

    in_group = false;

    moving_selection_attempt = false;
    moving_selection = false;
    select_single_attempt = -1;

    play_position_pos = 0;
    play_position = memnew(Control);
    play_position->set_mouse_filter(MOUSE_FILTER_PASS);
    add_child(play_position);
    play_position->set_anchors_and_margins_preset(PRESET_WIDE);
    play_position->connect("draw",callable_mp(this, &ClassName::_play_position_draw));
    set_focus_mode(FOCUS_CLICK);
    set_mouse_filter(MOUSE_FILTER_PASS); //scroll has to work too for selection
}

//////////////////////////////////////

AnimationTrackEdit *AnimationTrackEditPlugin::create_value_track_edit(Object *p_object, VariantType p_type, const StringName &p_property, PropertyHint p_hint, StringView p_hint_string, int p_usage) {
    if (get_script_instance()) {
        Variant args[6] = {
            Variant(p_object),
            p_type,
            p_property,
            p_hint,
            p_hint_string,
            p_usage
        };

        Variant *argptrs[6] = {
            &args[0],
            &args[1],
            &args[2],
            &args[3],
            &args[4],
            &args[5]
        };

        Callable::CallError ce;
        return object_cast<AnimationTrackEdit>(get_script_instance()->call("create_value_track_edit", (const Variant **)&argptrs, 6, ce).as<Object *>());
    }
    return nullptr;
}

AnimationTrackEdit *AnimationTrackEditPlugin::create_audio_track_edit() {

    if (get_script_instance()) {
        return object_cast<AnimationTrackEdit>(get_script_instance()->call("create_audio_track_edit").as<Object *>());
    }
    return nullptr;
}

AnimationTrackEdit *AnimationTrackEditPlugin::create_animation_track_edit(Object *p_object) {
    if (get_script_instance()) {
        return object_cast<AnimationTrackEdit>(get_script_instance()->call("create_animation_track_edit", Variant(p_object)).as<Object *>());
    }
    return nullptr;
}

///////////////////////////////////////

void AnimationTrackEditGroup::_notification(int p_what) {

    if (p_what == NOTIFICATION_DRAW) {
        Ref<Font> font = get_theme_font("font", "Label");
        int separation = get_theme_constant("hseparation", "ItemList");
        Color color = get_theme_color("font_color", "Label");

        if (root && root->has_node(node)) {
            Node *n = root->get_node(node);
            if (n && EditorNode::get_singleton()->get_editor_selection()->is_selected(n)) {
                color = get_theme_color("accent_color", "Editor");
            }
        }

        Color bgcol = get_theme_color("dark_color_2", "Editor");
        bgcol.a *= 0.6f;
        draw_rect_filled(Rect2(Point2(), get_size()), bgcol);
        Color linecolor = color;
        linecolor.a = 0.2f;

        draw_line(Point2(), Point2(get_size().width, 0), linecolor, Math::round(EDSCALE));
        draw_line(Point2(timeline->get_name_limit(), 0), Point2(timeline->get_name_limit(), get_size().height), linecolor, Math::round(EDSCALE));
        draw_line(Point2(get_size().width - timeline->get_buttons_width(), 0), Point2(get_size().width - timeline->get_buttons_width(), get_size().height), linecolor, Math::round(EDSCALE));

        int ofs = 0;
        draw_texture(icon, Point2(ofs, int(get_size().height - icon->get_height()) / 2));
        ofs += separation + icon->get_width();
        draw_ui_string(font, Point2(ofs, int(get_size().height - font->get_height()) / 2 + font->get_ascent()), node_name, color, timeline->get_name_limit() - ofs);

        int px = (-timeline->get_value() + timeline->get_play_position()) * timeline->get_zoom_scale() + timeline->get_name_limit();

        if (px >= timeline->get_name_limit() && px < get_size().width - timeline->get_buttons_width()) {
            Color accent = get_theme_color("accent_color", "Editor");
            draw_line(Point2(px, 0), Point2(px, get_size().height), accent, Math::round(2 * EDSCALE));
        }
    }
}

void AnimationTrackEditGroup::set_type_and_name(const Ref<Texture> &p_type, const UIString &p_name, const NodePath &p_node) {
    icon = p_type;
    node_name = p_name;
    node = p_node;
    update();
    minimum_size_changed();
}

Size2 AnimationTrackEditGroup::get_minimum_size() const {

    Ref<Font> font = get_theme_font("font", "Label");
    int separation = get_theme_constant("vseparation", "ItemList");

    return Vector2(0, M_MAX(font->get_height(), icon->get_height()) + separation);
}

void AnimationTrackEditGroup::set_timeline(AnimationTimelineEdit *p_timeline) {
    timeline = p_timeline;
    timeline->connect("zoom_changed",callable_mp(this, &ClassName::_zoom_changed));
    timeline->connect("name_limit_changed",callable_mp(this, &ClassName::_zoom_changed));
}

void AnimationTrackEditGroup::set_root(Node *p_root) {
    root = p_root;
    update();
}

void AnimationTrackEditGroup::_zoom_changed() {
    update();
}

void AnimationTrackEditGroup::_bind_methods() {
}

AnimationTrackEditGroup::AnimationTrackEditGroup() {
    set_mouse_filter(MOUSE_FILTER_PASS);
}

//////////////////////////////////////

void AnimationTrackEditor::add_track_edit_plugin(const Ref<AnimationTrackEditPlugin> &p_plugin) {

    if ( track_edit_plugins.contains(p_plugin) )
        return;
    track_edit_plugins.push_back(p_plugin);
}

void AnimationTrackEditor::remove_track_edit_plugin(const Ref<AnimationTrackEditPlugin> &p_plugin) {

    track_edit_plugins.erase_first(p_plugin);
}

void AnimationTrackEditor::set_animation(const Ref<Animation> &p_anim) {

    if (animation != p_anim && _get_track_selected() >= 0) {
        track_edits[_get_track_selected()]->release_focus();
    }
    if (animation) {
        animation->disconnect("changed",callable_mp(this, &ClassName::_animation_changed));
        _clear_selection();
    }
    animation = p_anim;
    timeline->set_animation(p_anim);

    _cancel_bezier_edit();
    _update_tracks();

    if (animation) {
        animation->connect("changed",callable_mp(this, &ClassName::_animation_changed));

        hscroll->show();
        edit->set_disabled(false);
        _update_step_spinbox();
        step->set_read_only(false);
        snap->set_disabled(false);
        snap_mode->set_disabled(false);

        imported_anim_warning->hide();
        for (int i = 0; i < animation->get_track_count(); i++) {
            if (animation->track_is_imported(i)) {
                imported_anim_warning->show();
                break;
            }
        }

    } else {
        hscroll->hide();
        edit->set_disabled(true);
        step->set_block_signals(true);
        step->set_value(0);
        step->set_block_signals(false);
        step->set_read_only(true);
        snap->set_disabled(true);
        snap_mode->set_disabled(true);
    }
}

Ref<Animation> AnimationTrackEditor::get_current_animation() const {

    return animation;
}
void AnimationTrackEditor::_root_removed(Node *p_root) {
    root = nullptr;
}

void AnimationTrackEditor::set_root(Node *p_root) {
    if (root) {
        root->disconnect_all("tree_exiting",get_instance_id());
    }

    root = p_root;

    if (root) {
        root->connect("tree_exiting",callable_gen(this,[this]() {  _root_removed(nullptr); }), ObjectNS::CONNECT_ONESHOT);
    }

    _update_tracks();
}

Node *AnimationTrackEditor::get_root() const {

    return root;
}

void AnimationTrackEditor::update_keying() {
    bool keying_enabled = is_visible_in_tree() && animation;

    if (keying_enabled == keying)
        return;

    keying = keying_enabled;
    //_update_menu();
    emit_signal("keying_changed");
}

bool AnimationTrackEditor::has_keying() const {
    return keying;
}
Dictionary AnimationTrackEditor::get_state() const {
    Dictionary state;
    state["fps_mode"] = timeline->is_using_fps();
    state["zoom"] = zoom->get_value();
    state["offset"] = timeline->get_value();
    state["v_scroll"] = scroll->get_v_scrollbar()->get_value();
    return state;
}
void AnimationTrackEditor::set_state(const Dictionary &p_state) {
    if (p_state.has("fps_mode")) {
        bool fps_mode = p_state["fps_mode"].as<bool>();
        if (fps_mode) {
            snap_mode->select(1);
        } else {
            snap_mode->select(0);
        }
        _snap_mode_changed(snap_mode->get_selected());
    } else {
        snap_mode->select(0);
        _snap_mode_changed(snap_mode->get_selected());
    }
    if (p_state.has("zoom")) {
        zoom->set_value(p_state["zoom"].as<float>());
    } else {
        zoom->set_value(1.0);
    }
    if (p_state.has("offset")) {
        timeline->set_value(p_state["offset"].as<float>());
    } else {
        timeline->set_value(0);
    }
    if (p_state.has("v_scroll")) {
        scroll->get_v_scrollbar()->set_value(p_state["v_scroll"].as<float>());
    } else {
        scroll->get_v_scrollbar()->set_value(0);
    }
}

void AnimationTrackEditor::cleanup() {
    set_animation(Ref<Animation>());
}

void AnimationTrackEditor::_name_limit_changed() {

    for (auto & track_edit : track_edits) {
        track_edit->update();
    }
}

void AnimationTrackEditor::_timeline_changed(float p_new_pos, bool p_drag) {

    emit_signal("timeline_changed", p_new_pos, p_drag);
}

void AnimationTrackEditor::_track_remove_request(int p_track) {

    int idx = p_track;
    if (idx >= 0 && idx < animation->get_track_count()) {

        undo_redo->create_action(TTR("Remove Anim Track"));
        undo_redo->add_do_method([this]() { _clear_selection(false); }, get_instance_id());
        undo_redo->add_do_method(animation.get(), "remove_track", idx);
        undo_redo->add_undo_method(animation.get(), "add_track", animation->track_get_type(idx), idx);
        undo_redo->add_undo_method(animation.get(), "track_set_path", idx, animation->track_get_path(idx));
        // TODO interpolation.
        for (int i = 0; i < animation->track_get_key_count(idx); i++) {

            Variant v = animation->track_get_key_value(idx, i);
            float time = animation->track_get_key_time(idx, i);
            float trans = animation->track_get_key_transition(idx, i);

            undo_redo->add_undo_method(animation.get(), "track_insert_key", idx, time, v);
            undo_redo->add_undo_method(animation.get(), "track_set_key_transition", idx, i, trans);
        }

        undo_redo->add_undo_method(animation.get(), "track_set_interpolation_type", idx, animation->track_get_interpolation_type(idx));
        if (animation->track_get_type(idx) == Animation::TYPE_VALUE) {
            undo_redo->add_undo_method(animation.get(), "value_track_set_update_mode", idx, animation->value_track_get_update_mode(idx));
        }

        undo_redo->commit_action();
    }
}

void AnimationTrackEditor::_track_grab_focus(int p_track) {

    // Don't steal focus if not working with the track editor.
    if (object_cast<AnimationTrackEdit>(get_focus_owner()))
        track_edits[p_track]->grab_focus();
}

void AnimationTrackEditor::set_anim_pos(float p_pos) {

    timeline->set_play_position(p_pos);
    for (auto & track_edit : track_edits) {
        track_edit->set_play_position(p_pos);
    }
    for (auto & group : groups) {
        group->update();
    }
    bezier_edit->set_play_position(p_pos);
}

static bool track_type_is_resettable(Animation::TrackType p_type) {
    switch (p_type) {
        case Animation::TYPE_VALUE:
        case Animation::TYPE_BEZIER:
        case Animation::TYPE_TRANSFORM:
            return true;
        default:
            return false;
    }
}
void AnimationTrackEditor::_query_insert(const InsertData &p_id) {

    if (insert_frame != Engine::get_singleton()->get_frames_drawn()) {
        //clear insert list for the frame if frame changed
        if (insert_confirm->is_visible_in_tree())
            return; //do nothing
        insert_data.clear();
        insert_query = false;
    }
    insert_frame = Engine::get_singleton()->get_frames_drawn();

    for (const InsertData &E : insert_data) {
        //prevent insertion of multiple tracks
        if (E.path == p_id.path)
            return; //already inserted a track for this on this frame
    }

    insert_data.push_back(p_id);
    bool reset_allowed = true;
    AnimationPlayer *player = AnimationPlayerEditor::singleton->get_player();
    if (player->has_animation("RESET") && player->get_animation("RESET") == animation) {
        // Avoid messing with the reset animation itself
        reset_allowed = false;
    } else {
        bool some_resettable = false;
        for (int i = 0; i < insert_data.size(); i++) {
            if (track_type_is_resettable(insert_data[i].type)) {
                some_resettable = true;
                break;
            }
        }
        if (!some_resettable) {
            reset_allowed = false;
        }
    }

    if (p_id.track_idx == -1) {
            //potential new key, does not exist
            int num_tracks = 0;

            bool all_bezier = true;
            for (InsertData & dat : insert_data) {
                if (dat.type != Animation::TYPE_VALUE && dat.type != Animation::TYPE_BEZIER)
                    all_bezier = false;

                if (dat.track_idx == -1)
                    ++num_tracks;

                if (dat.type != Animation::TYPE_VALUE)
                    continue;
                switch (dat.value.get_type()) {
                    case VariantType::INT:
                    case VariantType::FLOAT:
                    case VariantType::VECTOR2:
                    case VariantType::VECTOR3:
                    case VariantType::QUAT:
                    case VariantType::PLANE:
                    case VariantType::COLOR: {
                        // Valid.
                    } break;
                    default: {
                        all_bezier = false;
                    }
                }
            }
        if (EDITOR_DEF_T("editors/animation/confirm_insert_track", true)) {
            if (num_tracks == 1) {
                // TRANSLATORS: %s will be replaced by a phrase describing the target of track.
                insert_confirm_text->set_text(
                        FormatSN(TTR("Create NEW track for %s and insert key?").asCString(), p_id.query.c_str()));
            } else
                insert_confirm_text->set_text(
                        FormatSN(TTR("Create %d NEW tracks and insert keys?").asCString(), num_tracks));

            insert_confirm_bezier->set_visible(all_bezier);
            insert_confirm_reset->set_visible(reset_allowed);
            insert_confirm->get_ok()->set_text(TTR("Create"));
            insert_confirm->popup_centered_minsize();
            insert_query = true;
        } else {
            call_deferred([=]() {
                _insert_delay(reset_allowed && EDITOR_GET("editors/animation/default_create_reset_tracks"),
                        all_bezier && EDITOR_GET("editors/animation/default_create_bezier_tracks"));
            });
            insert_queue = true;
        }

    } else {
        if (!insert_query && !insert_queue) {
            // Create Beziers wouldn't make sense in this case, where no tracks are being created
            call_deferred([=]() {
                _insert_delay(reset_allowed && EDITOR_GET("editors/animation/default_create_reset_tracks"), false);
            });
            insert_queue = true;
        }
    }
}

void AnimationTrackEditor::_insert_delay(bool p_create_reset, bool p_create_beziers) {

    if (insert_query) {
        //discard since it's entered into query mode
        insert_queue = false;
        return;
    }

    undo_redo->create_action(TTR("Anim Insert"));
    Ref<Animation> reset_anim;
    if (p_create_reset) {
        reset_anim = _create_and_get_reset_animation();
    }

    TrackIndices next_tracks(animation.get(), reset_anim.get());
    bool advance = false;
    while (!insert_data.empty()) {

        if (insert_data.front().advance)
            advance = true;
        next_tracks = _confirm_insert(insert_data.front(), next_tracks, p_create_reset, reset_anim, p_create_beziers);
        insert_data.pop_front();
    }

    undo_redo->commit_action();

    if (advance) {
        float step = animation->get_step();
        if (step == 0)
            step = 1;

        float pos = timeline->get_play_position();

        pos = Math::stepify(pos + step, step);
        if (pos > animation->get_length())
            pos = animation->get_length();
        set_anim_pos(pos);
        emit_signal("timeline_changed", pos, true);
    }
    insert_queue = false;
}

void AnimationTrackEditor::insert_transform_key(Node3D *p_node, StringView p_sub, const Transform &p_xform) {

    if (!keying)
        return;
    if (not animation)
        return;

    ERR_FAIL_COND(!root);
    //let's build a node path
    String path(root->get_path_to(p_node));
    if (!p_sub.empty())
        path += String(":") + p_sub;

    NodePath np = (NodePath)path;

    int track_idx = -1;

    for (int i = 0; i < animation->get_track_count(); i++) {

        if (animation->track_get_type(i) != Animation::TYPE_TRANSFORM)
            continue;
        if (animation->track_get_path(i) != np)
            continue;

        track_idx = i;
        break;
    }

    InsertData id;
    Dictionary val;

    id.path = np;
    id.track_idx = track_idx;
    id.value = p_xform;
    id.type = Animation::TYPE_TRANSFORM;
    // TRANSLATORS: This describes the target of new animation track, will be inserted into another string.
    id.query = FormatVE(TTR("node '%s'").asCString(), p_node->get_name().asCString());
    id.advance = false;

    //dialog insert

    _query_insert(id);
}

void AnimationTrackEditor::_insert_animation_key(const NodePath& p_path, const Variant &p_value) {

    String path(p_path);

    //animation property is a special case, always creates an animation track
    for (int i = 0; i < animation->get_track_count(); i++) {

        String np(animation->track_get_path(i));

        if (path == np && animation->track_get_type(i) == Animation::TYPE_ANIMATION) {
            //exists
            InsertData id;
            id.path = (NodePath)path;
            id.track_idx = i;
            id.value = p_value;
            id.type = Animation::TYPE_ANIMATION;
            // TRANSLATORS: This describes the target of new animation track, will be inserted into another string.
            id.query = TTR("animation");
            id.advance = false;
            //dialog insert
            _query_insert(id);
            return;
        }
    }

    InsertData id;
    id.path = (NodePath)path;
    id.track_idx = -1;
    id.value = p_value;
    id.type = Animation::TYPE_ANIMATION;
    id.query = TTR("animation");
    id.advance = false;
    //dialog insert
    _query_insert(id);
}

void AnimationTrackEditor::insert_node_value_key(Node *p_node, StringView p_property, const Variant &p_value, bool p_only_if_exists) {

    ERR_FAIL_COND(!root);
    //let's build a node path

    Node *node = p_node;

    String path(root->get_path_to(node));

    if (object_cast<AnimationPlayer>(node) && p_property == StringView("current_animation")) {
        if (node == AnimationPlayerEditor::singleton->get_player()) {
            EditorNode::get_singleton()->show_warning(TTR("AnimationPlayer can't animate itself, only other players."));
            return;
        }
        _insert_animation_key((NodePath)path, p_value);
        return;
    }

    EditorHistory *history = EditorNode::get_singleton()->get_editor_history();
    for (int i = 1; i < history->get_path_size(); i++) {

        String prop = history->get_path_property(i);
        ERR_FAIL_COND(prop.empty());
        path += ":" + prop;
    }

    path += String(":") + p_property;

    NodePath np(path);

    //locate track

    bool inserted = false;

    for (int i = 0; i < animation->get_track_count(); i++) {

        if (animation->track_get_type(i) == Animation::TYPE_VALUE) {
            if (animation->track_get_path(i) != np)
                continue;

            InsertData id;
            id.path = np;
            id.track_idx = i;
            id.value = p_value;
            id.type = Animation::TYPE_VALUE;
            // TRANSLATORS: This describes the target of new animation track, will be inserted into another string.
            id.query = FormatVE(TTR("property '%.*s'").asCString(), p_property.size(),p_property.data());
            id.advance = false;
            //dialog insert
            _query_insert(id);
            inserted = true;
        } else if (animation->track_get_type(i) == Animation::TYPE_BEZIER) {

            Variant value;
            String track_path = (String)animation->track_get_path(i);
            if (track_path == (String)np) {
                value = p_value; //all good
            } else {
                auto sep = StringUtils::rfind(track_path,':');
                if (sep != String::npos) {
                    StringView base_path = StringUtils::substr(track_path,0, sep);
                    if ((String)np == base_path) {
                        auto value_name = StringName(StringUtils::substr(track_path,sep + 1));
                        value = p_value.get_named(value_name);
                    } else
                    continue;
                } else
                    continue;
            }

            InsertData id;
            id.path = animation->track_get_path(i);
            id.track_idx = i;
            id.value = value;
            id.type = Animation::TYPE_BEZIER;
            id.query = FormatVE(TTR("property '%.*s'").asCString(), p_property.size(),p_property.data());
            id.advance = false;
            //dialog insert
            _query_insert(id);
            inserted = true;
        }
    }

    if (inserted || p_only_if_exists)
        return;
    InsertData id;
    id.path = np;
    id.track_idx = -1;
    id.value = p_value;
    id.type = Animation::TYPE_VALUE;
    id.query = FormatVE(TTR("property '%.*s'").asCString(), p_property.size(),p_property.data());
    id.advance = false;
    //dialog insert
    _query_insert(id);
}

void AnimationTrackEditor::insert_value_key(StringView p_property, const Variant &p_value, bool p_advance) {

    EditorHistory *history = EditorNode::get_singleton()->get_editor_history();

    ERR_FAIL_COND(!root);
    //let's build a node path
    ERR_FAIL_COND(history->get_path_size() == 0);
    Object *obj = object_for_entity(history->get_path_object(0));
    ERR_FAIL_COND(!object_cast<Node>(obj));

    Node *node = object_cast<Node>(obj);

    String path(root->get_path_to(node));

    if (object_cast<AnimationPlayer>(node) && p_property == StringView("current_animation")) {
        if (node == AnimationPlayerEditor::singleton->get_player()) {
            EditorNode::get_singleton()->show_warning(TTR("AnimationPlayer can't animate itself, only other players."));
            return;
        }
        _insert_animation_key((NodePath)path, p_value);
        return;
    }

    for (int i = 1; i < history->get_path_size(); i++) {

        String prop = history->get_path_property(i);
        ERR_FAIL_COND(prop.empty());
        path += ":" + prop;
    }

    path += String(":") + p_property;

    NodePath np(path);

    //locate track

    bool inserted = false;

    for (int i = 0; i < animation->get_track_count(); i++) {

        if (animation->track_get_type(i) == Animation::TYPE_VALUE) {
            if (animation->track_get_path(i) != np)
                continue;

            InsertData id;
            id.path = np;
            id.track_idx = i;
            id.value = p_value;
            id.type = Animation::TYPE_VALUE;
            id.query = FormatVE(TTR("property '%.*s'").asCString(), p_property.size(),p_property.data());
            id.advance = p_advance;
            //dialog insert
            _query_insert(id);
            inserted = true;
        } else if (animation->track_get_type(i) == Animation::TYPE_BEZIER) {

            Variant value;
            if (animation->track_get_path(i) == np) {
                value = p_value; //all good
            } else {
                String tpath(animation->track_get_path(i));
                auto index = tpath.rfind(':');
                if (NodePath(tpath.substr(0, index + 1)) == np) {
                    auto subindex = StringName(StringView(tpath).substr(index + 1, tpath.length() - index));
                    value = p_value.get_named(subindex);
                } else {
                    continue;
                }
            }

            InsertData id;
            id.path = animation->track_get_path(i);
            id.track_idx = i;
            id.value = value;
            id.type = Animation::TYPE_BEZIER;
            id.query = FormatVE(TTR("property '%.*s'").asCString(), p_property.size(),p_property.data());
            id.advance = p_advance;
            //dialog insert
            _query_insert(id);
            inserted = true;
        }
    }

    if (!inserted) {
        InsertData id;
        id.path = np;
        id.track_idx = -1;
        id.value = p_value;
        id.type = Animation::TYPE_VALUE;
        id.query = FormatVE(TTR("property '%.*s'").asCString(), p_property.size(),p_property.data());
        id.advance = p_advance;
        //dialog insert
        _query_insert(id);
    }
}

Ref<Animation> AnimationTrackEditor::_create_and_get_reset_animation() {
    AnimationPlayer *player = AnimationPlayerEditor::singleton->get_player();
    if (player->has_animation("RESET")) {
        return player->get_animation("RESET");
    }
    Ref<Animation> reset_anim(make_ref_counted<Animation>());
    reset_anim->set_length(ANIM_MIN_LENGTH);
    undo_redo->add_do_method(player, "add_animation", "RESET", reset_anim);
    undo_redo->add_do_method(AnimationPlayerEditor::singleton, "_animation_player_changed", Variant(player));
    undo_redo->add_undo_method(player, "remove_animation", "RESET");
    undo_redo->add_undo_method(AnimationPlayerEditor::singleton, "_animation_player_changed", Variant(player));
    return reset_anim;
}
void AnimationTrackEditor::_confirm_insert_list() {

    undo_redo->create_action(TTR("Anim Create & Insert"));

    bool create_reset = insert_confirm_reset->is_visible() && insert_confirm_reset->is_pressed();
    Ref<Animation> reset_anim;
    if (create_reset) {
        reset_anim = _create_and_get_reset_animation();
    }

    TrackIndices next_tracks(animation.get(), reset_anim.get());
    while (insert_data.size()) {
        next_tracks = _confirm_insert(insert_data.front(), next_tracks, create_reset, reset_anim, insert_confirm_bezier->is_pressed());
        insert_data.pop_front();
    }

    undo_redo->commit_action();
}

PropertyInfo AnimationTrackEditor::_find_hint_for_track(int p_idx, NodePath &r_base_path, Variant *r_current_val) {

    r_base_path = NodePath();
    ERR_FAIL_COND_V(not animation, PropertyInfo());
    ERR_FAIL_INDEX_V(p_idx, animation->get_track_count(), PropertyInfo());

    if (!root) {
        return PropertyInfo();
    }

    NodePath path = animation->track_get_path(p_idx);

    if (!root->has_node_and_resource(path)) {
        return PropertyInfo();
    }

    RES res;
    Vector<StringName> leftover_path;
    Node *node = root->get_node_and_resource(path, res, leftover_path, true);

    if (node) {
        r_base_path = node->get_path();
    }

    if (leftover_path.empty()) {
        if (r_current_val) {
            if (res) {
                *r_current_val = res;
            } else if (node) {
                *r_current_val = Variant(node);
            }
        }
        return PropertyInfo();
    }

    Variant property_info_base;
    if (res) {
        property_info_base = res;
        if (r_current_val) {
            *r_current_val = res->get_indexed(leftover_path);
        }
    } else if (node) {
        property_info_base = Variant(node);
        if (r_current_val) {
            *r_current_val = node->get_indexed(leftover_path);
        }
    }

    for (int i = 0; i < leftover_path.size() - 1; i++) {
        property_info_base = property_info_base.get_named(leftover_path[i]);
    }

    Vector<PropertyInfo> pinfo;
    property_info_base.get_property_list(&pinfo);

    for(const PropertyInfo & E : pinfo) {

        if (E.name == leftover_path[leftover_path.size() - 1]) {
            return E;
        }
    }

    return PropertyInfo();
}

static Vector<StringView> _get_bezier_subindices_for_type(VariantType p_type, bool *r_valid = nullptr) {
    Vector<StringView> subindices;
    if (r_valid) {
        *r_valid = true;
    }
    switch (p_type) {
        case VariantType::INT:
        case VariantType::FLOAT: {
            subindices.push_back("");
        } break;
        case VariantType::VECTOR2: {
            subindices.push_back(":x");
            subindices.push_back(":y");
        } break;
        case VariantType::VECTOR3: {
            subindices.push_back(":x");
            subindices.push_back(":y");
            subindices.push_back(":z");
        } break;
        case VariantType::QUAT: {
            subindices.push_back(":x");
            subindices.push_back(":y");
            subindices.push_back(":z");
            subindices.push_back(":w");
        } break;
        case VariantType::COLOR: {
            subindices.push_back(":r");
            subindices.push_back(":g");
            subindices.push_back(":b");
            subindices.push_back(":a");
        } break;
        case VariantType::PLANE: {
            subindices.push_back(":x");
            subindices.push_back(":y");
            subindices.push_back(":z");
            subindices.push_back(":d");
        } break;
        default: {
            if (r_valid) {
                *r_valid = false;
            }
        }
    }

    return subindices;
}

AnimationTrackEditor::TrackIndices AnimationTrackEditor::_confirm_insert(InsertData p_id, TrackIndices p_next_tracks,
        bool p_create_reset, Ref<Animation> p_reset_anim, bool p_create_beziers) {

    bool created = false;
    if (p_id.track_idx < 0) {

        if (p_create_beziers) {
            bool valid;
            Vector<StringView> subindices = _get_bezier_subindices_for_type(p_id.value.get_type(), &valid);
            if (valid) {

            for (size_t i = 0; i < subindices.size(); i++) {
                InsertData id = p_id;
                id.type = Animation::TYPE_BEZIER;
                    id.value = p_id.value.get_named(
                            StringName(StringUtils::substr(subindices[i], 1, subindices[i].length())));
                    id.path = NodePath(p_id.path.asString() + subindices[i]);
                    p_next_tracks = _confirm_insert(id, p_next_tracks, p_create_reset, p_reset_anim, false);
            }

                return p_next_tracks;
            }
        }
        created = true;
        undo_redo->create_action(TTR("Anim Insert Track & Key"));
        Animation::UpdateMode update_mode = Animation::UPDATE_DISCRETE;

        if (p_id.type == Animation::TYPE_VALUE || p_id.type == Animation::TYPE_BEZIER) {
            // Wants a new track.

            {
                // Hack.
                NodePath np;
                animation->add_track(p_id.type);
                animation->track_set_path(animation->get_track_count() - 1, p_id.path);
                PropertyInfo h = _find_hint_for_track(animation->get_track_count() - 1, np);
                animation->remove_track(animation->get_track_count() - 1); //hack

                if (h.type == VariantType::FLOAT || h.type == VariantType::VECTOR2 || h.type == VariantType::RECT2 ||
                        h.type == VariantType::VECTOR3 || h.type == VariantType::AABB || h.type == VariantType::QUAT ||
                        h.type == VariantType::COLOR || h.type == VariantType::PLANE ||
                        h.type == VariantType::TRANSFORM2D || h.type == VariantType::TRANSFORM) {

                    update_mode = Animation::UPDATE_CONTINUOUS;
                }

                if (h.usage & PROPERTY_USAGE_ANIMATE_AS_TRIGGER) {
                    update_mode = Animation::UPDATE_TRIGGER;
                }
            }
        }

        p_id.track_idx = p_next_tracks.normal;

        undo_redo->add_do_method(animation.get(), "add_track", p_id.type);
        undo_redo->add_do_method(animation.get(), "track_set_path", p_id.track_idx, p_id.path);
        if (p_id.type == Animation::TYPE_VALUE)
            undo_redo->add_do_method(animation.get(), "value_track_set_update_mode", p_id.track_idx, update_mode);

    } else {
        undo_redo->create_action(TTR("Anim Insert Key"));
    }

    float time = timeline->get_play_position();
    Variant value;

    switch (p_id.type) {

        case Animation::TYPE_VALUE: {

            value = p_id.value;

        } break;
        case Animation::TYPE_TRANSFORM: {

            Transform tr = p_id.value.as<Transform>();
            Dictionary d;
            d["location"] = tr.origin;
            d["scale"] = tr.basis.get_scale();
            d["rotation"] = Quat(tr.basis);
            value = d;
        } break;
        case Animation::TYPE_BEZIER: {
            Array array;
            array.resize(5);
            array[0] = p_id.value;
            array[1] = -0.25;
            array[2] = 0;
            array[3] = 0.25;
            array[4] = 0;
            value = array;

        } break;
        case Animation::TYPE_ANIMATION: {
            value = p_id.value;
        } break;
        default: {
        }
    }

    undo_redo->add_do_method(animation.get(), "track_insert_key", p_id.track_idx, time, value);

    if (created) {

        // Just remove the track.
        undo_redo->add_do_method([this]() { _clear_selection(false); }, get_instance_id());
        undo_redo->add_undo_method(animation.get(), "remove_track", animation->get_track_count());
        p_next_tracks.normal++;
    } else {

        undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", p_id.track_idx, time);
        int existing = animation->track_find_key(p_id.track_idx, time, true);
        if (existing != -1) {
            Variant v = animation->track_get_key_value(p_id.track_idx, existing);
            float trans = animation->track_get_key_transition(p_id.track_idx, existing);
            undo_redo->add_undo_method(animation.get(), "track_insert_key", p_id.track_idx, time, v, trans);
        }
    }

    if (p_create_reset && track_type_is_resettable(p_id.type)) {
        bool create_reset_track = true;
        Animation *reset_anim = p_reset_anim.get();
        for (int i = 0; i < reset_anim->get_track_count(); i++) {
            if (reset_anim->track_get_path(i) == p_id.path) {
                create_reset_track = false;
                break;
            }
        }
        if (create_reset_track) {
            undo_redo->add_do_method(reset_anim, "add_track", p_id.type);
            undo_redo->add_do_method(reset_anim, "track_set_path", p_next_tracks.reset, p_id.path);
            undo_redo->add_do_method(reset_anim, "track_insert_key", p_next_tracks.reset, 0.0f, value);
            undo_redo->add_undo_method(reset_anim, "remove_track", reset_anim->get_track_count());
            p_next_tracks.reset++;
        }
    }
    undo_redo->commit_action();

    return p_next_tracks;
}

void AnimationTrackEditor::show_select_node_warning(bool p_show) {
    info_message->set_visible(p_show);
}

bool AnimationTrackEditor::is_key_selected(int p_track, int p_key) const {

    SelectedKey sk;
    sk.key = p_key;
    sk.track = p_track;

    return selection.contains(sk);
}

bool AnimationTrackEditor::is_selection_active() const {
    return !selection.empty();
}

bool AnimationTrackEditor::is_snap_enabled() const {
    return snap->is_pressed() ^ Input::get_singleton()->is_key_pressed(KEY_CONTROL);
}

void AnimationTrackEditor::_update_tracks() {

    int selected = _get_track_selected();

    while (track_vbox->get_child_count()) {
        memdelete(track_vbox->get_child(0));
    }

    track_edits.clear();
    groups.clear();

    if (not animation)
        return;

    Map<String, VBoxContainer *> group_sort;

    bool use_grouping = !view_group->is_pressed();
    bool use_filter = selected_filter->is_pressed();

    for (int i = 0; i < animation->get_track_count(); i++) {
        AnimationTrackEdit *track_edit = nullptr;

        //find hint and info for plugin

        if (use_filter) {
            NodePath path = animation->track_get_path(i);

            if (root && root->has_node(path)) {
                Node *node = root->get_node(path);
                if (!node) {
                    continue; // no node, no filter
                }
                if (!EditorNode::get_singleton()->get_editor_selection()->is_selected(node)) {
                    continue; //skip track due to not selected
                }
            }
        }

        if (animation->track_get_type(i) == Animation::TYPE_VALUE) {

            NodePath path = animation->track_get_path(i);

            if (root && root->has_node_and_resource(path)) {
                RES res;
                NodePath base_path;
                Vector<StringName> leftover_path;
                Node *node = root->get_node_and_resource(path, res, leftover_path, true);
                PropertyInfo pinfo = _find_hint_for_track(i, base_path);

                Object *object = node;
                if (res) {
                    object = res.get();
                }

                if (object && !leftover_path.empty()) {
                    if (pinfo.name.empty()) {
                        pinfo.name = leftover_path[leftover_path.size() - 1];
                    }

                    for (const Ref<AnimationTrackEditPlugin> & entry : track_edit_plugins) {
                        track_edit = entry->create_value_track_edit(object, pinfo.type, pinfo.name, pinfo.hint, pinfo.hint_string, pinfo.usage);
                        if (track_edit) {
                            break;
                        }
                    }
                }
            }
        }
        if (animation->track_get_type(i) == Animation::TYPE_AUDIO) {

            for (int j = 0; j < track_edit_plugins.size(); j++) {
                track_edit = track_edit_plugins[j]->create_audio_track_edit();
                if (track_edit) {
                    break;
                }
            }
        }

        if (animation->track_get_type(i) == Animation::TYPE_ANIMATION) {
            NodePath path = animation->track_get_path(i);

            Node *node = nullptr;
            if (root && root->has_node(path)) {
                node = root->get_node(path);
            }

            if (node && object_cast<AnimationPlayer>(node)) {
                for (int j = 0; j < track_edit_plugins.size(); j++) {
                    track_edit = track_edit_plugins[j]->create_animation_track_edit(node);
                    if (track_edit) {
                        break;
                    }
                }
            }
        }

        if (track_edit == nullptr) {
            //no valid plugin_found
            track_edit = memnew(AnimationTrackEdit);
        }

        track_edits.push_back(track_edit);

        if (use_grouping) {
            String base_path(animation->track_get_path(i));
            base_path = StringUtils::get_slice(base_path,":", 0); // Remove sub-path.

            if (!group_sort.contains(base_path)) {
                AnimationTrackEditGroup *g = memnew(AnimationTrackEditGroup);
                Ref<Texture> icon = get_theme_icon("Node", "EditorIcons");
                String name = base_path;
                String tooltip;
                if (root && root->has_node((NodePath)base_path)) {
                    Node *n = root->get_node((NodePath)base_path);
                    if (n) {
                        icon = EditorNode::get_singleton()->get_object_icon(n, "Node");
                        name = n->get_name();
                        tooltip = (String)root->get_path_to(n);
                    }
                }

                g->set_type_and_name(icon, StringUtils::from_utf8(name), animation->track_get_path(i));
                g->set_root(root);
                g->set_tooltip(tooltip);
                g->set_timeline(timeline);
                groups.push_back(g);
                VBoxContainer *vb = memnew(VBoxContainer);
                vb->add_constant_override("separation", 0);
                vb->add_child(g);
                track_vbox->add_child(vb);
                group_sort[base_path] = vb;
            }

            track_edit->set_in_group(true);
            group_sort[base_path]->add_child(track_edit);

        } else {
            track_edit->set_in_group(false);
            track_vbox->add_child(track_edit);
        }

        track_edit->set_undo_redo(undo_redo);
        track_edit->set_timeline(timeline);
        track_edit->set_root(root);
        track_edit->set_animation_and_track(animation, i);
        track_edit->set_play_position(timeline->get_play_position());
        track_edit->set_editor(this);

        if (selected == i) {
            track_edit->grab_focus();
        }

        track_edit->connect("timeline_changed",callable_mp(this, &ClassName::_timeline_changed));
        track_edit->connect("remove_request",callable_mp(this, &ClassName::_track_remove_request), ObjectNS::CONNECT_QUEUED);
        track_edit->connect("dropped",callable_mp(this, &ClassName::_dropped_track), ObjectNS::CONNECT_QUEUED);
        auto lambda_insert = [this,i](float val) {
            _insert_key_from_track(val,i);
        };
        track_edit->connect("insert_key",callable_gen(this, lambda_insert), ObjectNS::CONNECT_QUEUED);
        track_edit->connect("select_key",callable_gen(this, [=](int a,bool b){_key_selected(a,b,i);}), ObjectNS::CONNECT_QUEUED);
        track_edit->connect("deselect_key",callable_gen(this, [=](int a){_key_deselected(a,i);}), ObjectNS::CONNECT_QUEUED);
        track_edit->connect("bezier_edit",callable_gen(this, [=](){_bezier_edit(i);}), ObjectNS::CONNECT_QUEUED);
        track_edit->connect("move_selection_begin",callable_mp(this, &ClassName::_move_selection_begin));
        track_edit->connect("move_selection",callable_mp(this, &ClassName::_move_selection));
        track_edit->connect("move_selection_commit",callable_mp(this, &ClassName::_move_selection_commit));
        track_edit->connect("move_selection_cancel",callable_mp(this, &ClassName::_move_selection_cancel));

        track_edit->connect("duplicate_request",callable_gen(this, [=]() { _edit_menu_pressed(EDIT_DUPLICATE_SELECTION); }),ObjectNS::CONNECT_QUEUED);
        track_edit->connect("duplicate_transpose_request",callable_gen(this, [=]() { _edit_menu_pressed(EDIT_DUPLICATE_TRANSPOSED); }),ObjectNS::CONNECT_QUEUED);
        track_edit->connect("create_reset_request",callable_gen(this, [=]() { _edit_menu_pressed(EDIT_ADD_RESET_KEY); }),ObjectNS::CONNECT_QUEUED);
        track_edit->connect("delete_request",callable_gen(this, [=]() { _edit_menu_pressed(EDIT_DELETE_SELECTION); }),ObjectNS::CONNECT_QUEUED);
    }
}

void AnimationTrackEditor::_animation_changed() {

    if (animation_changing_awaiting_update) {
        return; //all will be updated, don't bother with anything
    }

    if (key_edit && key_edit->setting) {
        //if editing a key, just update the edited track, makes refresh less costly
        if (key_edit->track < track_edits.size()) {
            if (animation->track_get_type(key_edit->track) == Animation::TYPE_BEZIER)
                bezier_edit->update();
            else
                track_edits[key_edit->track]->update();
        }
        return;
    }

    animation_changing_awaiting_update = true;
    call_deferred([this]() { _animation_update(); });
}

void AnimationTrackEditor::_snap_mode_changed(int p_mode) {

    timeline->set_use_fps(p_mode == 1);
    if (key_edit) {
        key_edit->set_use_fps(p_mode == 1);
    }
    _update_step_spinbox();
}

void AnimationTrackEditor::_update_step_spinbox() {
    if (not animation) {
        return;
    }
    step->set_block_signals(true);

    if (timeline->is_using_fps()) {
        if (animation->get_step() == 0) {
            step->set_value(0);
        } else {
            step->set_value(1.0 / animation->get_step());
        }

    } else {
        step->set_value(animation->get_step());
    }

    step->set_block_signals(false);
}
void AnimationTrackEditor::_animation_update() {

    timeline->update();
    timeline->update_values();

    bool same = true;

    if (not animation) {
        return;
    }

    if (track_edits.size() == animation->get_track_count()) {
        //check tracks are the same

        for (size_t i = 0; i < track_edits.size(); i++) {
            if (track_edits[i]->get_path() != animation->track_get_path(i)) {
                same = false;
                break;
            }
        }
    } else {
        same = false;
    }

    if (same) {
        for (size_t i = 0; i < track_edits.size(); i++) {
            track_edits[i]->update();
        }
        for (size_t i = 0; i < groups.size(); i++) {
            groups[i]->update();
        }
    } else {
        _update_tracks();
    }

    bezier_edit->update();

    _update_step_spinbox();
    emit_signal("animation_step_changed", animation->get_step());
    emit_signal("animation_len_changed", animation->get_length());
    EditorNode::get_singleton()->get_inspector()->refresh();

    animation_changing_awaiting_update = false;
}

MenuButton *AnimationTrackEditor::get_edit_menu() {
    return edit;
}

void AnimationTrackEditor::_notification(int p_what) {
    if (p_what == NOTIFICATION_THEME_CHANGED || p_what == NOTIFICATION_ENTER_TREE) {
        zoom_icon->set_texture(get_theme_icon("Zoom", "EditorIcons"));
        snap->set_button_icon(get_theme_icon("Snap", "EditorIcons"));
        view_group->set_button_icon(get_theme_icon(view_group->is_pressed() ? StringName("AnimationTrackList") : StringName("AnimationTrackGroup"), "EditorIcons"));
        selected_filter->set_button_icon(get_theme_icon("AnimationFilter", "EditorIcons"));
        imported_anim_warning->set_button_icon(get_theme_icon("NodeWarning", "EditorIcons"));
        main_panel->add_theme_style_override("panel", get_theme_stylebox("bg", "Tree"));
        edit->get_popup()->set_item_icon(edit->get_popup()->get_item_index(EDIT_APPLY_RESET), get_theme_icon("Reload", "EditorIcons"));
    }

    if (p_what == NOTIFICATION_READY) {
        EditorNode::get_singleton()->get_editor_selection()->connect("selection_changed",callable_mp(this, &ClassName::_selection_changed));
    }

    if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
        update_keying();
        EditorNode::get_singleton()->update_keying();
        emit_signal("keying_changed");
    }
}

void AnimationTrackEditor::_update_scroll(double) {
    for (size_t i = 0; i < track_edits.size(); i++) {
        track_edits[i]->update();
    }
    for (size_t i = 0; i < groups.size(); i++) {
        groups[i]->update();
    }
}

void AnimationTrackEditor::_update_step(double p_new_step) {

    undo_redo->create_action(TTR("Change Animation Step"));
    float step_value = p_new_step;
    if (timeline->is_using_fps()) {
        if (step_value != 0.0) {
            step_value = 1.0f / step_value;
        }
    }
    undo_redo->add_do_method(animation.get(), "set_step", step_value);
    undo_redo->add_undo_method(animation.get(), "set_step", animation->get_step());
    step->set_block_signals(true);
    undo_redo->commit_action();
    step->set_block_signals(false);
    emit_signal("animation_step_changed", step_value);
}

void AnimationTrackEditor::_update_length(double p_new_len) {

    emit_signal("animation_len_changed", p_new_len);
}

void AnimationTrackEditor::_dropped_track(int p_from_track, int p_to_track) {
    if (p_from_track == p_to_track || p_from_track == p_to_track - 1) {
        return;
    }

    _clear_selection();
    undo_redo->create_action(TTR("Rearrange Tracks"));
    undo_redo->add_do_method(animation.get(), "track_move_to", p_from_track, p_to_track);
    // Take into account that the position of the tracks that come after the one removed will change.
    int to_track_real = p_to_track > p_from_track ? p_to_track - 1 : p_to_track;
    undo_redo->add_undo_method(animation.get(), "track_move_to", to_track_real, p_to_track > p_from_track ? p_from_track : p_from_track + 1);
    undo_redo->add_do_method([this,to_track_real]() { _track_grab_focus(to_track_real); }, this->get_instance_id());
    undo_redo->add_undo_method([this,p_from_track]() { _track_grab_focus(p_from_track); }, this->get_instance_id());
    undo_redo->commit_action();
}

void AnimationTrackEditor::_new_track_node_selected(const NodePath& p_path) {

    ERR_FAIL_COND(!root);
    Node *node = get_node(p_path);
    ERR_FAIL_COND(!node);
    NodePath path_to = root->get_path_to(node);

    if (adding_track_type == Animation::TYPE_TRANSFORM && !node->is_class("Node3D")) {
        EditorNode::get_singleton()->show_warning(TTR("Transform tracks only apply to Node3D-based nodes."));
        return;
    }

    switch (adding_track_type) {
        case Animation::TYPE_VALUE: {
            adding_track_path = path_to;
            prop_selector->set_type_filter({});
            prop_selector->select_property_from_instance(node);
        } break;
        case Animation::TYPE_TRANSFORM:
        case Animation::TYPE_METHOD: {

            undo_redo->create_action(TTR("Add Track"));
            undo_redo->add_do_method(animation.get(), "add_track", adding_track_type);
            undo_redo->add_do_method(animation.get(), "track_set_path", animation->get_track_count(), path_to);
            undo_redo->add_undo_method(animation.get(), "remove_track", animation->get_track_count());
            undo_redo->commit_action();

        } break;
        case Animation::TYPE_BEZIER: {

            const Vector<VariantType> filter = {
                VariantType::INT,
                VariantType::FLOAT,
                VariantType::VECTOR2,
                VariantType::VECTOR3,
                VariantType::QUAT,
                VariantType::PLANE,
                VariantType::COLOR,
            };

            adding_track_path = path_to;
            prop_selector->set_type_filter(filter);
            prop_selector->select_property_from_instance(node);
        } break;
        case Animation::TYPE_AUDIO: {

            if (!node->is_class("AudioStreamPlayer") && !node->is_class("AudioStreamPlayer2D") && !node->is_class("AudioStreamPlayer3D")) {
                EditorNode::get_singleton()->show_warning(TTR("Audio tracks can only point to nodes of type:\n-AudioStreamPlayer\n-AudioStreamPlayer2D\n-AudioStreamPlayer3D"));
                return;
            }

            undo_redo->create_action(TTR("Add Track"));
            undo_redo->add_do_method(animation.get(), "add_track", adding_track_type);
            undo_redo->add_do_method(animation.get(), "track_set_path", animation->get_track_count(), path_to);
            undo_redo->add_undo_method(animation.get(), "remove_track", animation->get_track_count());
            undo_redo->commit_action();

        } break;
        case Animation::TYPE_ANIMATION: {

            if (!node->is_class("AnimationPlayer")) {
                EditorNode::get_singleton()->show_warning(TTR("Animation tracks can only point to AnimationPlayer nodes."));
                return;
            }

            if (node == AnimationPlayerEditor::singleton->get_player()) {
                EditorNode::get_singleton()->show_warning(TTR("AnimationPlayer can't animate itself, only other players."));
                return;
            }

            undo_redo->create_action(TTR("Add Track"));
            undo_redo->add_do_method(animation.get(), "add_track", adding_track_type);
            undo_redo->add_do_method(animation.get(), "track_set_path", animation->get_track_count(), path_to);
            undo_redo->add_undo_method(animation.get(), "remove_track", animation->get_track_count());
            undo_redo->commit_action();

        } break;
    }
}

void AnimationTrackEditor::_add_track(int p_type) {
    if (!root) {
        EditorNode::get_singleton()->show_warning(TTR("Not possible to add a new track without a root"));
        return;
    }
    adding_track_type = p_type;
    pick_track->popup_centered_ratio();
    pick_track->get_filter_line_edit()->clear();
    pick_track->get_filter_line_edit()->grab_focus();
}

void AnimationTrackEditor::_new_track_property_selected(StringView p_name) {

    String full_path = String(adding_track_path) + ":" + p_name;

    if (adding_track_type == Animation::TYPE_VALUE) {

        Animation::UpdateMode update_mode = Animation::UPDATE_DISCRETE;
        {
            //hack
            NodePath np;
            animation->add_track(Animation::TYPE_VALUE);
            animation->track_set_path(animation->get_track_count() - 1, (NodePath)full_path);
            PropertyInfo h = _find_hint_for_track(animation->get_track_count() - 1, np);
            animation->remove_track(animation->get_track_count() - 1); //hack
            if (h.type == VariantType::FLOAT ||
                    h.type == VariantType::VECTOR2 ||
                    h.type == VariantType::RECT2 ||
                    h.type == VariantType::VECTOR3 ||
                    h.type == VariantType::AABB ||
                    h.type == VariantType::QUAT ||
                    h.type == VariantType::COLOR ||
                    h.type == VariantType::PLANE ||
                    h.type == VariantType::TRANSFORM2D ||
                    h.type == VariantType::TRANSFORM) {

                update_mode = Animation::UPDATE_CONTINUOUS;
            }

            if (h.usage & PROPERTY_USAGE_ANIMATE_AS_TRIGGER) {
                update_mode = Animation::UPDATE_TRIGGER;
            }
        }

        undo_redo->create_action(TTR("Add Track"));
        undo_redo->add_do_method(animation.get(), "add_track", adding_track_type);
        undo_redo->add_do_method(animation.get(), "track_set_path", animation->get_track_count(), full_path);
        undo_redo->add_do_method(animation.get(), "value_track_set_update_mode", animation->get_track_count(), update_mode);
        undo_redo->add_undo_method(animation.get(), "remove_track", animation->get_track_count());
        undo_redo->commit_action();
    } else {
        Vector<StringView> subindices;
        {
            //hack
            NodePath np;
            animation->add_track(Animation::TYPE_VALUE);
            animation->track_set_path(animation->get_track_count() - 1, (NodePath)full_path);
            PropertyInfo h = _find_hint_for_track(animation->get_track_count() - 1, np);
            animation->remove_track(animation->get_track_count() - 1); //hack
            bool valid;
            subindices = _get_bezier_subindices_for_type(h.type, &valid);
            if (!valid) {
                EditorNode::get_singleton()->show_warning("Invalid track for Bezier (no suitable sub-properties)");
                return;
            }
        }

        undo_redo->create_action(TTR("Add Bezier Track"));
        int base_track = animation->get_track_count();
        for (size_t i = 0; i < subindices.size(); i++) {
            undo_redo->add_do_method(animation.get(), "add_track", adding_track_type);
            undo_redo->add_do_method(animation.get(), "track_set_path", base_track + i, full_path + subindices[i]);
            undo_redo->add_undo_method(animation.get(), "remove_track", base_track);
        }
        undo_redo->commit_action();
    }
}

void AnimationTrackEditor::_timeline_value_changed(double) {

    timeline->update_play_position();

    for (int i = 0; i < track_edits.size(); i++) {
        track_edits[i]->update();
        track_edits[i]->update_play_position();
    }

    for (int i = 0; i < groups.size(); i++) {
        groups[i]->update();
    }

    bezier_edit->update();
    bezier_edit->update_play_position();
}

int AnimationTrackEditor::_get_track_selected() {

    for (int i = 0; i < track_edits.size(); i++) {
        if (track_edits[i]->has_focus())
            return i;
    }

    return -1;
}

void AnimationTrackEditor::_insert_key_from_track(float p_ofs, int p_track) {

    ERR_FAIL_INDEX(p_track, animation->get_track_count());

    if (snap->is_pressed() && step->get_value() != 0.0f) {
        p_ofs = snap_time(p_ofs);
    }
    while (animation->track_find_key(p_track, p_ofs, true) != -1) { //make sure insertion point is valid
        p_ofs += 0.001f;
    }

    switch (animation->track_get_type(p_track)) {
        case Animation::TYPE_TRANSFORM: {
            if (!root->has_node(animation->track_get_path(p_track))) {
                EditorNode::get_singleton()->show_warning(TTR("Track path is invalid, so can't add a key."));
                return;
            }
            Node3D *base = object_cast<Node3D>(root->get_node(animation->track_get_path(p_track)));

            if (!base) {
                EditorNode::get_singleton()->show_warning(TTR("Track is not of type Node3D, can't insert key"));
                return;
            }

            Transform xf = base->get_transform();

            Vector3 loc = xf.get_origin();
            Vector3 scale = xf.basis.get_scale_local();
            Quat rot = xf.basis;

            undo_redo->create_action(TTR("Add Transform Track Key"));
            undo_redo->add_do_method(animation.get(), "transform_track_insert_key", p_track, p_ofs, loc, rot, scale);
            undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", p_track, p_ofs);
            undo_redo->commit_action();

        } break;
        case Animation::TYPE_VALUE: {

            NodePath bp;
            Variant value;
            _find_hint_for_track(p_track, bp, &value);

            undo_redo->create_action(TTR("Add Track Key"));
            undo_redo->add_do_method(animation.get(), "track_insert_key", p_track, p_ofs, value);
            undo_redo->add_undo_method([this,anm=animation]() { _clear_selection_for_anim(anm); },get_instance_id() );
            undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", p_track, p_ofs);
            undo_redo->commit_action();

        } break;
        case Animation::TYPE_METHOD: {
            if (!root->has_node(animation->track_get_path(p_track))) {
                EditorNode::get_singleton()->show_warning(TTR("Track path is invalid, so can't add a method key."));
                return;
            }
            Node *base = root->get_node(animation->track_get_path(p_track));

            method_selector->select_method_from_instance(base);

            insert_key_from_track_call_ofs = p_ofs;
            insert_key_from_track_call_track = p_track;

        } break;
        case Animation::TYPE_BEZIER: {

            NodePath bp;
            Variant value;
            _find_hint_for_track(p_track, bp, &value);
            Array arr;
            arr.resize(5);
            arr[0] = value;
            arr[1] = -0.25;
            arr[2] = 0;
            arr[3] = 0.25;
            arr[4] = 0;

            undo_redo->create_action(TTR("Add Track Key"));
            undo_redo->add_do_method(animation.get(), "track_insert_key", p_track, p_ofs, arr);
            undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", p_track, p_ofs);
            undo_redo->commit_action();

        } break;
        case Animation::TYPE_AUDIO: {

            Dictionary ak;
            ak["stream"] = RES();
            ak["start_offset"] = 0;
            ak["end_offset"] = 0;

            undo_redo->create_action(TTR("Add Track Key"));
            undo_redo->add_do_method(animation.get(), "track_insert_key", p_track, p_ofs, ak);
            undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", p_track, p_ofs);
            undo_redo->commit_action();
        } break;
        case Animation::TYPE_ANIMATION: {

            StringName anim = "[stop]";

            undo_redo->create_action(TTR("Add Track Key"));
            undo_redo->add_do_method(animation.get(), "track_insert_key", p_track, p_ofs, anim);
            undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", p_track, p_ofs);
            undo_redo->commit_action();
        } break;
    }
}

void AnimationTrackEditor::_add_method_key(const StringName &p_method) {

    if (!root->has_node(animation->track_get_path(insert_key_from_track_call_track))) {
        EditorNode::get_singleton()->show_warning(TTR("Track path is invalid, so can't add a method key."));
        return;
    }
    Node *base = root->get_node(animation->track_get_path(insert_key_from_track_call_track));

    Vector<MethodInfo> minfo;
    base->get_method_list(&minfo);

    for(const MethodInfo & E : minfo) {
        if (E.name == p_method) {

            Dictionary d;
            d["method"] = p_method;
            Array params;
            size_t first_defarg = E.arguments.size() - E.default_arguments.size();

            for (size_t i = 0; i < E.arguments.size(); i++) {

                if (i >= first_defarg) {
                    Variant arg = E.default_arguments[i - first_defarg];
                    params.push_back(arg);
                } else {
                    Variant arg = Variant::construct_default(E.arguments[i].type);
                    params.push_back(arg);
                }
            }
            d["args"] = params;

            undo_redo->create_action(TTR("Add Method Track Key"));
            undo_redo->add_do_method(animation.get(), "track_insert_key", insert_key_from_track_call_track, insert_key_from_track_call_ofs, d);
            undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", insert_key_from_track_call_track, insert_key_from_track_call_ofs);
            undo_redo->commit_action();

            return;
        }
    }

    EditorNode::get_singleton()->show_warning(TTR("Method not found in object: ") + p_method);
}

void AnimationTrackEditor::_key_selected(int p_key, bool p_single, int p_track) {

    ERR_FAIL_INDEX(p_track, animation->get_track_count());
    ERR_FAIL_INDEX(p_key, animation->track_get_key_count(p_track));

    SelectedKey sk;
    sk.key = p_key;
    sk.track = p_track;

    if (p_single) {
        _clear_selection();
    }

    KeyInfo ki;
    ki.pos = animation->track_get_key_time(p_track, p_key);
    selection[sk] = ki;

    for (size_t i = 0; i < track_edits.size(); i++) {
        track_edits[i]->update();
    }

    _update_key_edit();
}

void AnimationTrackEditor::_key_deselected(int p_key, int p_track) {

    ERR_FAIL_INDEX(p_track, animation->get_track_count());
    ERR_FAIL_INDEX(p_key, animation->track_get_key_count(p_track));

    SelectedKey sk;
    sk.key = p_key;
    sk.track = p_track;

    selection.erase(sk);

    for (int i = 0; i < track_edits.size(); i++) {
        track_edits[i]->update();
    }

    _update_key_edit();
}

void AnimationTrackEditor::_move_selection_begin() {
    moving_selection = true;
    moving_selection_offset = 0;
}

void AnimationTrackEditor::_move_selection(float p_offset) {
    moving_selection_offset = p_offset;

    for (int i = 0; i < track_edits.size(); i++) {
        track_edits[i]->update();
    }
}

struct _AnimMoveRestore {

    int track;
    float time;
    Variant key;
    float transition;
};
//used for undo/redo

void AnimationTrackEditor::_clear_key_edit() {
    if (key_edit) {
        //if key edit is the object being inspected, remove it first
        if (EditorNode::get_singleton()->get_inspector()->get_edited_object() == key_edit) {
            EditorNode::get_singleton()->push_item(nullptr);
        }

        //then actually delete it
        memdelete(key_edit);
        key_edit = nullptr;
    }

    if (multi_key_edit) {
        if (EditorNode::get_singleton()->get_inspector()->get_edited_object() == multi_key_edit) {
            EditorNode::get_singleton()->push_item(nullptr);
        }

        memdelete(multi_key_edit);
        multi_key_edit = nullptr;
    }
}

void AnimationTrackEditor::_clear_selection(bool p_update) {
    selection.clear();
    if (p_update) {
    for (size_t i = 0; i < track_edits.size(); i++) {
        track_edits[i]->update();
    }
    }
    _clear_key_edit();
}

void AnimationTrackEditor::_update_key_edit() {

    _clear_key_edit();
    if (not animation)
        return;

    if (selection.size() == 1) {

        key_edit = memnew(AnimationTrackKeyEdit);
        key_edit->animation = animation;
        key_edit->track = selection.begin()->first.track;
        key_edit->use_fps = timeline->is_using_fps();

        float ofs = animation->track_get_key_time(key_edit->track, selection.begin()->first.key);
        key_edit->key_ofs = ofs;
        key_edit->root_path = root;

        NodePath np;
        key_edit->hint = _find_hint_for_track(key_edit->track, np);
        key_edit->undo_redo = undo_redo;
        key_edit->base = np;

        EditorNode::get_singleton()->push_item(key_edit);
    } else if (selection.size() > 1) {

        multi_key_edit = memnew(AnimationMultiTrackKeyEdit);
        multi_key_edit->animation = animation;

        Map<int, Vector<float> > key_ofs_map;
        Map<int, NodePath> base_map;
        int first_track = -1;
        for (eastl::pair<const SelectedKey,KeyInfo> &E : selection) {

            int track = E.first.track;
            if (first_track < 0)
                first_track = track;

            if (!key_ofs_map.contains(track)) {
                key_ofs_map[track] = Vector<float>();
                base_map[track] = NodePath();
            }

            key_ofs_map[track].push_back(animation->track_get_key_time(track, E.first.key));
        }
        multi_key_edit->key_ofs_map = key_ofs_map;
        multi_key_edit->base_map = base_map;
        multi_key_edit->hint = _find_hint_for_track(first_track, base_map[first_track]);

        multi_key_edit->use_fps = timeline->is_using_fps();

        multi_key_edit->root_path = root;

        multi_key_edit->undo_redo = undo_redo;

        EditorNode::get_singleton()->push_item(multi_key_edit);
    }
}

void AnimationTrackEditor::_clear_selection_for_anim(const Ref<Animation> &p_anim) {

    if (animation != p_anim)
        return;

    _clear_selection();
}

void AnimationTrackEditor::_select_at_anim(const Ref<Animation> &p_anim, int p_track, float p_pos) {

    if (animation != p_anim)
        return;

    int idx = animation->track_find_key(p_track, p_pos, true);
    ERR_FAIL_COND(idx < 0);

    SelectedKey sk;
    sk.track = p_track;
    sk.key = idx;
    KeyInfo ki;
    ki.pos = p_pos;

    selection.emplace(sk, ki);
}

void AnimationTrackEditor::_move_selection_commit() {

    undo_redo->create_action(TTR("Anim Move Keys"));

    Vector<_AnimMoveRestore> to_restore;

    float motion = moving_selection_offset;
    // 1 - remove the keys
    for (auto &E : selection) {

        undo_redo->add_do_method(animation.get(), "track_remove_key", E.first.track, E.first.key);
    }
    // 2 - remove overlapped keys
    for (auto E = selection.rbegin(); E!=selection.rend(); ++E) {

        float newtime = snap_time(E->second.pos + motion);
        int idx = animation->track_find_key(E->first.track, newtime, true);
        if (idx == -1)
            continue;
        SelectedKey sk;
        sk.key = idx;
        sk.track = E->first.track;
        if (selection.contains(sk))
            continue; //already in selection, don't save

        undo_redo->add_do_method(animation.get(), "track_remove_key_at_position", E->first.track, newtime);
        _AnimMoveRestore amr;

        amr.key = animation->track_get_key_value(E->first.track, idx);
        amr.track = E->first.track;
        amr.time = newtime;
        amr.transition = animation->track_get_key_transition(E->first.track, idx);

        to_restore.push_back(amr);
    }

    // 3 - move the keys (re insert them)
    for (auto E = selection.rbegin(); E != selection.rend(); ++E) {

        float newpos = snap_time(E->second.pos + motion);
        undo_redo->add_do_method(animation.get(), "track_insert_key", E->first.track, newpos,
                animation->track_get_key_value(E->first.track, E->first.key),
                animation->track_get_key_transition(E->first.track, E->first.key));
    }

    // 4 - (undo) remove inserted keys
    for (auto E = selection.rbegin(); E != selection.rend(); ++E) {

        float newpos = snap_time(E->second.pos + motion);
        undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", E->first.track, newpos);
    }

    // 5 - (undo) reinsert keys
    for (auto E = selection.rbegin(); E != selection.rend(); ++E) {

        undo_redo->add_undo_method(animation.get(), "track_insert_key", E->first.track, E->second.pos,
                animation->track_get_key_value(E->first.track, E->first.key),
                animation->track_get_key_transition(E->first.track, E->first.key));
    }

    // 6 - (undo) reinsert overlapped keys
    for (_AnimMoveRestore & amr : to_restore) {

        undo_redo->add_undo_method(animation.get(), "track_insert_key", amr.track, amr.time, amr.key, amr.transition);
    }

    undo_redo->add_do_method([this,anm=animation]() { _clear_selection_for_anim(anm); },get_instance_id() );
    undo_redo->add_undo_method([this,anm=animation]() { _clear_selection_for_anim(anm); },get_instance_id() );

    // 7 - reselect
    for (auto E = selection.rbegin(); E != selection.rend(); ++E) {

        float oldpos = E->second.pos;
        float newpos = snap_time(oldpos + motion);

        undo_redo->add_do_method([this,anm=animation,tr=E->first.track,newpos]() { _select_at_anim(anm,tr,newpos); },get_instance_id() );
        undo_redo->add_undo_method([this,anm=animation,tr=E->first.track,oldpos]() { _select_at_anim(anm,tr,oldpos); },get_instance_id() );

    }

    undo_redo->commit_action();

    moving_selection = false;
    for (int i = 0; i < track_edits.size(); i++) {
        track_edits[i]->update();
    }

    _update_key_edit();
}
void AnimationTrackEditor::_move_selection_cancel() {

    moving_selection = false;
    for (int i = 0; i < track_edits.size(); i++) {
        track_edits[i]->update();
    }
}

bool AnimationTrackEditor::is_moving_selection() const {
    return moving_selection;
}
float AnimationTrackEditor::get_moving_selection_offset() const {
    return moving_selection_offset;
}

void AnimationTrackEditor::_box_selection_draw() {

    const Rect2 selection_rect = Rect2(Point2(), box_selection->get_size());
    box_selection->draw_rect_filled(selection_rect, get_theme_color("box_selection_fill_color", "Editor"));
    box_selection->draw_rect_stroke(selection_rect, get_theme_color("box_selection_stroke_color", "Editor"), Math::round(EDSCALE));
}

void AnimationTrackEditor::_scroll_input(const Ref<InputEvent> &p_event) {

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);
    if(mb) {
        if (mb->is_pressed() && mb->get_command() && (mb->get_button_index() == BUTTON_WHEEL_UP || mb->get_button_index() == BUTTON_WHEEL_DOWN)) {
            double new_zoom_value;
            double current_zoom_value = timeline->get_zoom()->get_value();
            int direction = mb->get_button_index() == BUTTON_WHEEL_UP ? 1 : -1;
            if (current_zoom_value <= 0.1) {
                new_zoom_value = M_MAX(0.01, current_zoom_value + 0.01 * direction);
            } else {
                new_zoom_value = direction < 0 ? M_MAX(0.01, current_zoom_value / 1.05) : current_zoom_value * 1.05;
            }
            timeline->get_zoom()->set_value(new_zoom_value);
            accept_event();
        }

        if (mb->is_pressed() && mb->get_alt() && mb->get_button_index() == BUTTON_WHEEL_UP) {
            goto_prev_step(true);
        scroll->accept_event();
    }

        if (mb->is_pressed() && mb->get_alt() && mb->get_button_index() == BUTTON_WHEEL_DOWN) {
            goto_prev_step(true);
        scroll->accept_event();
    }

        if (mb->get_button_index() == BUTTON_LEFT) {
        if (mb->is_pressed()) {
            box_selecting = true;
            box_selecting_from = scroll->get_global_transform().xform(mb->get_position());
            box_select_rect = Rect2();
        } else if (box_selecting) {

            if (box_selection->is_visible_in_tree()) {
                //only if moved
                for (int i = 0; i < track_edits.size(); i++) {

                    Rect2 local_rect = box_select_rect;
                    local_rect.position -= track_edits[i]->get_global_position();
                    track_edits[i]->append_to_selection(local_rect, mb->get_command());
                }

                if (_get_track_selected() == -1 && !track_edits.empty()) { //minimal hack to make shortcuts work
                    track_edits[track_edits.size() - 1]->grab_focus();
                }
            } else {
                _clear_selection(); //clear it
            }

            box_selection->hide();
            box_selecting = false;
        }
    }
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);

    if (mm && mm->get_button_mask() & BUTTON_MASK_MIDDLE) {

        timeline->set_value(timeline->get_value() - mm->get_relative().x / timeline->get_zoom_scale());
    }

    if (mm && box_selecting) {

        if (!(mm->get_button_mask() & BUTTON_MASK_LEFT)) {
            //no longer
            box_selection->hide();
            box_selecting = false;
            return;
        }

        if (!box_selection->is_visible_in_tree()) {
            if (!mm->get_command() && !mm->get_shift()) {
                _clear_selection();
            }
            box_selection->show();
        }

        Vector2 from = box_selecting_from;
        Vector2 to = scroll->get_global_transform().xform(mm->get_position());

        if (from.x > to.x) {
            SWAP(from.x, to.x);
        }

        if (from.y > to.y) {
            SWAP(from.y, to.y);
        }

        Rect2 rect(from, to - from);
        Rect2 scroll_rect = Rect2(scroll->get_global_position(), scroll->get_size());
        rect = scroll_rect.clip(rect);
        box_selection->set_position(rect.position);
        box_selection->set_size(rect.size);

        box_select_rect = rect;

        if (get_local_mouse_position().y < 0) {
            //avoid box selection from going up and lose focus to viewport
            warp_mouse(Vector2(mm->get_position().x, 0));
        }
    }
}

void AnimationTrackEditor::_cancel_bezier_edit() {
    bezier_edit->hide();
    scroll->show();
}

void AnimationTrackEditor::_bezier_edit(int p_for_track) {

    _clear_selection(); //bezier probably wants to use a separate selection mode
    bezier_edit->set_root(root);
    bezier_edit->set_animation_and_track(animation, p_for_track);
    scroll->hide();
    bezier_edit->show();
    //search everything within the track and curve- edit it
}

void AnimationTrackEditor::_anim_duplicate_keys(bool transpose) {
    //duplicait!
    if (selection.empty() || !animation || (transpose && (_get_track_selected() < 0 || _get_track_selected() >= animation->get_track_count())))
        return;

    int top_track = 0x7FFFFFFF;
    float top_time = 1e10f;
    for (auto E = selection.rbegin(); E != selection.rend(); ++E) {

        const SelectedKey &sk = E->first;

        float t = animation->track_get_key_time(sk.track, sk.key);
        if (t < top_time)
            top_time = t;
        if (sk.track < top_track)
            top_track = sk.track;
    }
    ERR_FAIL_COND(top_track == 0x7FFFFFFF || top_time == 1e10f);

    //

    int start_track = transpose ? _get_track_selected() : top_track;

    undo_redo->create_action(TTR("Anim Duplicate Keys"));

    Vector<Pair<int, float> > new_selection_values;

    for (auto E = selection.rbegin(); E != selection.rend(); ++E) {

        const SelectedKey &sk = E->first;

        float t = animation->track_get_key_time(sk.track, sk.key);

        float dst_time = t + (timeline->get_play_position() - top_time);
        int dst_track = sk.track + (start_track - top_track);

        if (dst_track < 0 || dst_track >= animation->get_track_count())
            continue;

        if (animation->track_get_type(dst_track) != animation->track_get_type(sk.track))
            continue;

        int existing_idx = animation->track_find_key(dst_track, dst_time, true);

        undo_redo->add_do_method(animation.get(), "track_insert_key", dst_track, dst_time,
                animation->track_get_key_value(E->first.track, E->first.key),
                animation->track_get_key_transition(E->first.track, E->first.key));
        undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", dst_track, dst_time);

        Pair<int, float> p;
        p.first = dst_track;
        p.second = dst_time;
        new_selection_values.push_back(p);

        if (existing_idx != -1) {

            undo_redo->add_undo_method(animation.get(), "track_insert_key", dst_track, dst_time, animation->track_get_key_value(dst_track, existing_idx), animation->track_get_key_transition(dst_track, existing_idx));
        }
    }

    undo_redo->commit_action();

    //reselect duplicated

    Map<SelectedKey, KeyInfo> new_selection;
    for (const Pair<int, float> &E : new_selection_values) {

        int track = E.first;
        float time = E.second;

        int existing_idx = animation->track_find_key(track, time, true);

        if (existing_idx == -1)
            continue;
        SelectedKey sk2;
        sk2.track = track;
        sk2.key = existing_idx;

        KeyInfo ki;
        ki.pos = time;

        new_selection[sk2] = ki;
    }

    selection = new_selection;
    _update_tracks();
    _update_key_edit();
}
void AnimationTrackEditor::_edit_menu_about_to_show() {
    AnimationPlayer *player = AnimationPlayerEditor::singleton->get_player();
    edit->get_popup()->set_item_disabled(edit->get_popup()->get_item_index(EDIT_APPLY_RESET), !player->can_apply_reset());
}

void AnimationTrackEditor::goto_prev_step(bool p_from_mouse_event) {
    if (!animation) {
        return;
    }
    float step = animation->get_step();
    if (step == 0) {
        step = 1;
    }
    if (p_from_mouse_event && Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
        // Use more precise snapping when holding Shift.
        // This is used when scrobbling the timeline using Alt + Mouse wheel.
        step *= 0.25;
    }

    float pos = timeline->get_play_position();
    pos = Math::stepify(pos - step, step);
    if (pos < 0) {
        pos = 0;
    }
    set_anim_pos(pos);
    emit_signal("timeline_changed", pos, true);
}

void AnimationTrackEditor::goto_next_step(bool p_from_mouse_event) {
    if (!animation) {
        return;
    }
    float step = animation->get_step();
    if (step == 0) {
        step = 1;
    }
    if (p_from_mouse_event && Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
        // Use more precise snapping when holding Shift.
        // This is used when scrobbling the timeline using Alt + Mouse wheel.
        // Do not use precise snapping when using the menu action or keyboard shortcut,
        // as the default keyboard shortcut requires pressing Shift.
        step *= 0.25;
    }

    float pos = timeline->get_play_position();

    pos = Math::stepify(pos + step, step);
    if (pos > animation->get_length()) {
        pos = animation->get_length();
    }
    set_anim_pos(pos);

    emit_signal("timeline_changed", pos, true);
}

struct AddRESETKeysAction : public UndoableAction {
    Ref<Animation> m_reset;
    struct Entry {
        Variant val;
        float trans;

    };
    struct TrackEntry {
        Animation::TrackType track_type;
        NodePath node_path;
        int dst_track;
        Entry new_val;
        Entry prev;
    };
    Vector<TrackEntry> ops;

    int source_track_in_reset(const NodePath &path) const {
        for (int i = 0; i < m_reset->get_track_count(); i++) {
            if (m_reset->track_get_path(i) == path) {
                return i;
            }
        }
        return -1;
    }

    AddRESETKeysAction(const Ref<Animation> &animation, const Ref<Animation> &reset,
            const Map<AnimationTrackEditor::SelectedKey, AnimationTrackEditor::KeyInfo> &selection) :
            m_reset(reset) {
        Set<int> tracks_added;
        for (auto E = selection.rbegin(), fin = selection.rend(); E != fin; ++E) {
            const auto &sk = E->first;

            // Only add one key per track.
            if (tracks_added.contains(sk.track)) {
                continue;
            }

            tracks_added.insert(sk.track);
            const NodePath &path = animation->track_get_path(sk.track);
            Entry new_val {
                animation->track_get_key_value(sk.track, sk.key),
                animation->track_get_key_transition(sk.track, sk.key)
            };
            Entry prev_val {Variant::null_variant,-1};
            int dst_track = source_track_in_reset(path);
            auto track_type = animation->track_get_type(sk.track);
            if (dst_track != -1) {
                // already exists, allow restoration of old value.
                int existing_idx = reset->track_find_key(dst_track, 0, true);
                prev_val = {
                    reset->track_get_key_value(dst_track, existing_idx),
                    reset->track_get_key_transition(dst_track, existing_idx)
                };
            }
            ops.push_back({ track_type, path, dst_track, new_val, prev_val });
        }
    }
    StringName name() const override {
        return TTR("Anim Add RESET Keys");
    }
    void redo() override {
        //NOTE: on first run, on 'do' dst_idx will get filled for missing ones
        for (auto &op : ops) {
            // not in reset yet, add it
            if (op.dst_track == -1) {
                // If adding multiple tracks, make sure that correct track is referenced.
                op.dst_track = m_reset->add_track(op.track_type);
                m_reset->track_set_path(op.dst_track, op.node_path);
            }
            m_reset->track_insert_key(op.dst_track, 0, op.new_val.val, op.new_val.trans);
        }
    }
    void undo() override {
        for (auto &op : ops) {
            int dst_track = op.dst_track;
            assert(dst_track != -1);
            if (op.prev.trans == -1) { // no previous value ? remove the track
                m_reset->remove_track(op.dst_track);
                op.dst_track = -1;
            } else {
                m_reset->track_set_key_value(op.dst_track, 0, op.prev.val);
                m_reset->track_set_key_transition(op.dst_track, 0, op.prev.trans);
            }
        }
    }
    bool can_apply() override {
        return m_reset->get_track_count() > 0;
    }

};

void AnimationTrackEditor::edit_copy_tracks() {
    track_copy_select->clear();
    TreeItem *troot = track_copy_select->create_item();

    for (int i = 0; i < animation->get_track_count(); i++) {

        NodePath path = animation->track_get_path(i);
        Node *node = nullptr;

        if (root && root->has_node(path)) {
            node = root->get_node(path);
        }

        String text;
        Ref<Texture> icon = get_theme_icon("Node", "EditorIcons");
        if (node) {
            if (has_icon(node->get_class_name(), "EditorIcons")) {
                icon = get_theme_icon(node->get_class_name(), "EditorIcons");
            }

            text = node->get_name();
            for (const StringName & s : path.get_subnames()) {
                text += '.';
                text += s.asCString();
            }

            path = NodePath(node->get_path().get_names(), path.get_subnames(), true); //store full path instead for copying
        } else {
            text = path.asString();
            auto sep = StringUtils::find(text,":");
            if (sep != String::npos) {
                text = StringUtils::substr(text,sep + 1, text.length());
            }
        }

        const char *track_type=nullptr;
        switch (animation->track_get_type(i)) {
            case Animation::TYPE_TRANSFORM:
                track_type = "Transform";
                break;
            case Animation::TYPE_METHOD:
                track_type = "Methods";
                break;
            case Animation::TYPE_BEZIER:
                track_type = "Bezier";
                break;
            case Animation::TYPE_AUDIO:
                track_type = "Audio";
                break;
            default: {
            }
        }

        if (track_type) {
            text += FormatVE(" (%s)", track_type);
        }
        TreeItem *it = track_copy_select->create_item(troot);
        it->set_editable(0, true);
        it->set_selectable(0, true);
        it->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
        it->set_icon(0, icon);
        it->set_text_utf8(0, text);
        Dictionary md;
        md["track_idx"] = i;
        md["path"] = path;
        it->set_metadata(0, md);
    }

    track_copy_dialog->popup_centered_minsize(Size2(350, 500) * EDSCALE);
}

void AnimationTrackEditor::edit_cop_tracks_confirm() {
    track_clipboard.clear();
    TreeItem *root = track_copy_select->get_root();
    if (!root) {
        return;
    }
    TreeItem *it = root->get_children();
    while (it) {
        Dictionary md = it->get_metadata(0).as<Dictionary>();
        int idx = md["track_idx"].as<int>();
        if (it->is_checked(0) && idx >= 0 && idx < animation->get_track_count()) {
            TrackClipboard tc;
            tc.base_path = animation->track_get_path(idx);
            tc.full_path = md["path"].as<NodePath>();
            tc.track_type = animation->track_get_type(idx);
            tc.interp_type = animation->track_get_interpolation_type(idx);
            if (tc.track_type == Animation::TYPE_VALUE) {
                tc.update_mode = animation->value_track_get_update_mode(idx);
            }
            tc.loop_wrap = animation->track_get_interpolation_loop_wrap(idx);
            tc.enabled = animation->track_is_enabled(idx);
            tc.keys.reserve(animation->track_get_key_count(idx));
            for (int i = 0; i < animation->track_get_key_count(idx); i++) {
                TrackClipboard::Key k;
                k.time = animation->track_get_key_time(idx, i);
                k.value = animation->track_get_key_value(idx, i);
                k.transition = animation->track_get_key_transition(idx, i);
                tc.keys.emplace_back(k);
            }
            track_clipboard.emplace_back(eastl::move(tc));
        }
        it = it->get_next();
    }
}

void AnimationTrackEditor::edit_scale_confirm() {
    if (selection.empty())
        return;

    float from_t = 1e20f;
    float to_t = -1e20f;
    float len = -1e20f;
    float pivot = 0;

    for (eastl::pair<const SelectedKey,KeyInfo> &E : selection) {
        float t = animation->track_get_key_time(E.first.track, E.first.key);
        if (t < from_t)
            from_t = t;
        if (t > to_t)
            to_t = t;
    }

    len = to_t - from_t;
    if (last_menu_track_opt == EDIT_SCALE_FROM_CURSOR) {
        pivot = timeline->get_play_position();
    } else {
        pivot = from_t;
    }

    float s = scale->get_value();
    if (s == 0) {
        ERR_PRINT("Can't scale to 0");
    }

    undo_redo->create_action(TTR("Anim Scale Keys"));

    Vector<_AnimMoveRestore> to_restore;

    // 1-remove the keys
    for (auto iter = selection.rbegin(); iter!=selection.rend(); ++iter) {

        undo_redo->add_do_method(animation.get(), "track_remove_key", iter->first.track, iter->first.key);
    }
    // 2- remove overlapped keys
    for (auto iter = selection.rbegin(); iter!=selection.rend(); ++iter) {

        float newtime = (iter->second.pos - from_t) * s + from_t;
        int idx = animation->track_find_key(iter->first.track, newtime, true);
        if (idx == -1)
            continue;
        SelectedKey sk;
        sk.key = idx;
        sk.track = iter->first.track;
        if (selection.contains(sk))
            continue; //already in selection, don't save

        undo_redo->add_do_method(animation.get(), "track_remove_key_at_position", iter->first.track, newtime);
        _AnimMoveRestore amr;

        amr.key = animation->track_get_key_value(iter->first.track, idx);
        amr.track = iter->first.track;
        amr.time = newtime;
        amr.transition = animation->track_get_key_transition(iter->first.track, idx);

        to_restore.push_back(amr);
    }

#define _NEW_POS(m_ofs) (((s > 0) ? m_ofs : from_t + (len - (m_ofs - from_t))) - pivot) * ABS(s) + from_t
    // 3-move the keys (re insert them)
    for (auto iter = selection.rbegin(); iter!=selection.rend(); ++iter) {

        float newpos = _NEW_POS(iter->second.pos);
        undo_redo->add_do_method(animation.get(), "track_insert_key", iter->first.track, newpos,
                animation->track_get_key_value(iter->first.track, iter->first.key),
                animation->track_get_key_transition(iter->first.track, iter->first.key));
    }

    // 4-(undo) remove inserted keys
    for (auto iter = selection.rbegin(); iter!=selection.rend(); ++iter) {

        float newpos = _NEW_POS(iter->second.pos);
        undo_redo->add_undo_method(animation.get(), "track_remove_key_at_position", iter->first.track, newpos);
    }

    // 5-(undo) reinsert keys
    for (auto iter = selection.rbegin(); iter!=selection.rend(); ++iter) {

        undo_redo->add_undo_method(animation.get(), "track_insert_key", iter->first.track, iter->second.pos,
                animation->track_get_key_value(iter->first.track, iter->first.key),
                animation->track_get_key_transition(iter->first.track, iter->first.key));
    }

    // 6-(undo) reinsert overlapped keys
    for (_AnimMoveRestore &amr : to_restore) {
        undo_redo->add_undo_method(animation.get(), "track_insert_key", amr.track, amr.time, amr.key, amr.transition);
    }

    undo_redo->add_do_method([this,anm=animation]() { _clear_selection_for_anim(anm); },get_instance_id() );
    undo_redo->add_undo_method([this,anm=animation]() { _clear_selection_for_anim(anm); },get_instance_id() );

    // 7-reselect
    for (auto iter = selection.rbegin(); iter!=selection.rend(); ++iter) {

        float oldpos = iter->second.pos;
        float newpos = _NEW_POS(oldpos);
        if (newpos >= 0) {
            undo_redo->add_do_method([this,anm=animation,tr=iter->first.track,newpos]() { _select_at_anim(anm,tr,newpos); },get_instance_id() );
            undo_redo->add_undo_method([this,anm=animation,tr=iter->first.track,oldpos]() { _select_at_anim(anm,tr,oldpos); },get_instance_id() );
        }
    }
#undef _NEW_POS
    undo_redo->commit_action();
}

void AnimationTrackEditor::_edit_menu_pressed(int p_option) {

    last_menu_track_opt = p_option;
    switch (p_option) {
        case EDIT_COPY_TRACKS: {
            edit_copy_tracks();
        } break;
        case EDIT_COPY_TRACKS_CONFIRM: {

            edit_cop_tracks_confirm();
        } break;
        case EDIT_PASTE_TRACKS: {

            if (track_clipboard.empty()) {
                EditorNode::get_singleton()->show_warning(TTR("Clipboard is empty!"));
                break;
            }

            int base_track = animation->get_track_count();
            undo_redo->create_action(TTR("Paste Tracks"));
            for (size_t i = 0; i < track_clipboard.size(); i++) {
                undo_redo->add_do_method(animation.get(), "add_track", track_clipboard[i].track_type);
                Node *exists = nullptr;
                NodePath path = track_clipboard[i].base_path;

                if (root) {
                    NodePath np = track_clipboard[i].full_path;
                    exists = root->get_node(np);
                    if (exists) {
                        path = NodePath(root->get_path_to(exists).get_names(), track_clipboard[i].full_path.get_subnames(), false);
                    }
                }

                undo_redo->add_do_method(animation.get(), "track_set_path", base_track, path);
                undo_redo->add_do_method(animation.get(), "track_set_interpolation_type", base_track, track_clipboard[i].interp_type);
                undo_redo->add_do_method(animation.get(), "track_set_interpolation_loop_wrap", base_track, track_clipboard[i].loop_wrap);
                undo_redo->add_do_method(animation.get(), "track_set_enabled", base_track, track_clipboard[i].enabled);
                if (track_clipboard[i].track_type == Animation::TYPE_VALUE) {
                    undo_redo->add_do_method(animation.get(), "value_track_set_update_mode", base_track, track_clipboard[i].update_mode);
                }

                for (int j = 0; j < track_clipboard[i].keys.size(); j++) {
                    undo_redo->add_do_method(animation.get(), "track_insert_key", base_track, track_clipboard[i].keys[j].time, track_clipboard[i].keys[j].value, track_clipboard[i].keys[j].transition);
                }

                undo_redo->add_undo_method(animation.get(), "remove_track", animation->get_track_count());

                base_track++;
            }

            undo_redo->commit_action();
        } break;

        case EDIT_SCALE_SELECTION:
        case EDIT_SCALE_FROM_CURSOR: {
            scale_dialog->popup_centered(Size2(200, 100) * EDSCALE);
        } break;
        case EDIT_SCALE_CONFIRM: {
            edit_scale_confirm();
        } break;
        case EDIT_DUPLICATE_SELECTION: {

            if (bezier_edit->is_visible()) {
                bezier_edit->duplicate_selection();
                break;
            }
            _anim_duplicate_keys(false);
        } break;
        case EDIT_DUPLICATE_TRANSPOSED: {
            if (bezier_edit->is_visible()) {
                EditorNode::get_singleton()->show_warning(TTR("This option does not work for Bezier editing, as it's only a single track."));
                break;
            }
            _anim_duplicate_keys(true);
        } break;
        case EDIT_ADD_RESET_KEY: {
            Ref<Animation> reset = _create_and_get_reset_animation();
            auto *act = new AddRESETKeysAction(animation, reset, selection);
            undo_redo->add_action(act);
            undo_redo->commit_action();

        } break;
        case EDIT_DELETE_SELECTION: {

            if (bezier_edit->is_visible()) {
                bezier_edit->delete_selection();
                break;
            }

            if (!selection.empty()) {
                undo_redo->create_action(TTR("Anim Delete Keys"));

                for (auto E = selection.rbegin(); E != selection.rend(); ++E) {
                    undo_redo->add_do_method(animation.get(), "track_remove_key", E->first.track, E->first.key);
                    undo_redo->add_undo_method(animation.get(), "track_insert_key", E->first.track, E->second.pos,
                            animation->track_get_key_value(E->first.track, E->first.key),
                            animation->track_get_key_transition(E->first.track, E->first.key));
                }
                undo_redo->add_do_method([this,anm=animation]() { _clear_selection_for_anim(anm); },get_instance_id() );
                undo_redo->add_undo_method([this,anm=animation]() { _clear_selection_for_anim(anm); },get_instance_id() );

                undo_redo->commit_action();
                //selection.clear();
                _update_key_edit();
            }
        } break;
        case EDIT_GOTO_NEXT_STEP: {

            goto_next_step(false);

        } break;
        case EDIT_GOTO_PREV_STEP: {
            goto_prev_step(false);
        } break;
        case EDIT_APPLY_RESET: {
            AnimationPlayerEditor::singleton->get_player()->apply_reset(true);

        } break;
        case EDIT_OPTIMIZE_ANIMATION: {
            optimize_dialog->popup_centered(Size2(250, 180) * EDSCALE);

        } break;
        case EDIT_OPTIMIZE_ANIMATION_CONFIRM: {
            animation->optimize(optimize_linear_error->get_value(), optimize_angular_error->get_value(), optimize_max_angle->get_value());
            _update_tracks();
            undo_redo->clear_history();

        } break;
        case EDIT_CLEAN_UP_ANIMATION: {
            cleanup_dialog->popup_centered_minsize(Size2(300, 0) * EDSCALE);

        } break;
        case EDIT_CLEAN_UP_ANIMATION_CONFIRM: {
            if (cleanup_all->is_pressed()) {
                Vector<StringName> names = AnimationPlayerEditor::singleton->get_player()->get_animation_list();
                for (const StringName &E : names) {
                    _cleanup_animation(AnimationPlayerEditor::singleton->get_player()->get_animation(E));
                }
            } else {
                _cleanup_animation(animation);
            }

        } break;
    }
}

void AnimationTrackEditor::_cleanup_animation(const Ref<Animation> &p_animation) {

    for (int i = 0; i < p_animation->get_track_count(); i++) {

        bool prop_exists = false;
        VariantType valid_type = VariantType::NIL;
        Object *obj = nullptr;

        RES res;
        Vector<StringName> leftover_path;

        Node *node = root->get_node_and_resource(p_animation->track_get_path(i), res, leftover_path);

        if (res) {
            obj = res.get();
        } else if (node) {
            obj = node;
        }

        if (obj && p_animation->track_get_type(i) == Animation::TYPE_VALUE) {
            valid_type = obj->get_static_property_type_indexed(leftover_path, &prop_exists);
        }

        if (!obj && cleanup_tracks->is_pressed()) {

            p_animation->remove_track(i);
            i--;
            continue;
        }

        if (!prop_exists || p_animation->track_get_type(i) != Animation::TYPE_VALUE || !cleanup_keys->is_pressed())
            continue;

        for (int j = 0; j < p_animation->track_get_key_count(i); j++) {

            Variant v = p_animation->track_get_key_value(i, j);

            if (!Variant::can_convert(v.get_type(), valid_type)) {
                p_animation->track_remove_key(i, j);
                j--;
            }
        }

        if (p_animation->track_get_key_count(i) == 0 && cleanup_tracks->is_pressed()) {
            p_animation->remove_track(i);
            i--;
        }
    }

    undo_redo->clear_history();
    _update_tracks();
}

void AnimationTrackEditor::_view_group_toggle() {

    _update_tracks();
    view_group->set_button_icon(get_theme_icon(view_group->is_pressed() ? StringName("AnimationTrackList") : StringName("AnimationTrackGroup"), "EditorIcons"));
}

bool AnimationTrackEditor::is_grouping_tracks() {

    if (!view_group) {
        return false;
    }

    return !view_group->is_pressed();
}

void AnimationTrackEditor::_selection_changed() {

    if (selected_filter->is_pressed()) {
        _update_tracks(); //needs updatin
    } else {
        for (int i = 0; i < track_edits.size(); i++) {
            track_edits[i]->update();
        }

        for (int i = 0; i < groups.size(); i++) {
            groups[i]->update();
        }
    }
}

float AnimationTrackEditor::snap_time(float p_value, bool p_relative) {

    if (is_snap_enabled()) {

        float snap_increment;
        if (timeline->is_using_fps() && step->get_value() > 0){
            snap_increment = 1.0f / step->get_value();
        } else {
            snap_increment = step->get_value();
        }

        if (Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
            // Use more precise snapping when holding Shift.
            snap_increment *= 0.25;
        }
        if (p_relative) {
            float rel = Math::fmod(timeline->get_value(), snap_increment);
            p_value = Math::stepify(p_value + rel, snap_increment) - rel;
        } else {
            p_value = Math::stepify(p_value, snap_increment);
        }
    }

    return p_value;
}

void AnimationTrackEditor::_show_imported_anim_warning() {

    // It looks terrible on a single line but the TTR extractor doesn't support line breaks yet.
    EditorNode::get_singleton()->show_warning(TTR("This animation belongs to an imported scene, so changes to imported tracks will not be saved.\n\nTo enable the ability to add custom tracks, navigate to the scene's import settings and set\n\"Animation > Storage\" to \"Files\", enable \"Animation > Keep Custom Tracks\", then re-import.\nAlternatively, use an import preset that imports animations to separate files."),
            TTR("Warning: Editing imported animation"));
}

void AnimationTrackEditor::_select_all_tracks_for_copy() {

    TreeItem *track = track_copy_select->get_root()->get_children();
    if (!track)
        return;

    bool all_selected = true;
    while (track) {
        if (!track->is_checked(0))
            all_selected = false;

        track = track->get_next();
    }

    track = track_copy_select->get_root()->get_children();
    while (track) {
        track->set_checked(0, !all_selected);
        track = track->get_next();
    }
}

void AnimationTrackEditor::_bind_methods() {
    ADD_SIGNAL(MethodInfo("timeline_changed", PropertyInfo(VariantType::FLOAT, "position"), PropertyInfo(VariantType::BOOL, "drag")));
    ADD_SIGNAL(MethodInfo("keying_changed"));
    ADD_SIGNAL(MethodInfo("animation_len_changed", PropertyInfo(VariantType::FLOAT, "len")));
    ADD_SIGNAL(MethodInfo("animation_step_changed", PropertyInfo(VariantType::FLOAT, "step")));
}

void AnimationTrackEditor::_pick_track_filter_text_changed(const String &p_text) {
    TreeItem *root_item = pick_track->get_scene_tree()->get_scene_tree()->get_root();

    Vector<Node *> select_candidates;
    Node *to_select = nullptr;

    String filter = pick_track->get_filter_line_edit()->get_text();

    _pick_track_select_recursive(root_item, filter, select_candidates);

    if (!select_candidates.empty()) {
        for (int i = 0; i < select_candidates.size(); ++i) {
            Node *candidate = select_candidates[i];

            if (((String)candidate->get_name()).to_lower().starts_with(filter.to_lower())) {
                to_select = candidate;
                break;
            }
        }

        if (!to_select) {
            to_select = select_candidates[0];
        }
    }

    pick_track->get_scene_tree()->set_selected(to_select);
}

void AnimationTrackEditor::_pick_track_select_recursive(TreeItem *p_item, const String &p_filter, Vector<Node *> &p_select_candidates) {
    if (!p_item) {
        return;
    }

    NodePath np = p_item->get_metadata(0).as<NodePath>();
    Node *node = get_node(np);

    if (!p_filter.empty() && ((String)node->get_name()).contains(p_filter,false)) {
        p_select_candidates.push_back(node);
    }

    TreeItem *c = p_item->get_children();

    while (c) {
        _pick_track_select_recursive(c, p_filter, p_select_candidates);
        c = c->get_next();
    }
}

void AnimationTrackEditor::_pick_track_filter_input(const Ref<InputEvent> &p_ie) {
    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_ie);

    if (k) {
        switch (k->get_keycode()) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_PAGEUP:
            case KEY_PAGEDOWN: {
                pick_track->get_scene_tree()->get_scene_tree()->call_va("_gui_input", k);
                pick_track->get_filter_line_edit()->accept_event();
            } break;
        }
    }
}
AnimationTrackEditor::AnimationTrackEditor() {
    root = nullptr;

    undo_redo = EditorNode::get_singleton()->get_undo_redo();

    main_panel = memnew(PanelContainer);
    add_child(main_panel);
    main_panel->set_v_size_flags(SIZE_EXPAND_FILL);
    HBoxContainer *timeline_scroll = memnew(HBoxContainer);
    main_panel->add_child(timeline_scroll);
    timeline_scroll->set_v_size_flags(SIZE_EXPAND_FILL);

    VBoxContainer *timeline_vbox = memnew(VBoxContainer);
    timeline_scroll->add_child(timeline_vbox);
    timeline_vbox->set_v_size_flags(SIZE_EXPAND_FILL);
    timeline_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
    timeline_vbox->add_constant_override("separation", 0);

    info_message = memnew(Label);
    info_message->set_text(TTR("Select an AnimationPlayer node to create and edit animations."));
    info_message->set_valign(Label::VALIGN_CENTER);
    info_message->set_align(Label::ALIGN_CENTER);
    info_message->set_autowrap(true);
    info_message->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
    info_message->set_anchors_and_margins_preset(PRESET_WIDE, PRESET_MODE_KEEP_SIZE, 8 * EDSCALE);
    main_panel->add_child(info_message);

    timeline = memnew(AnimationTimelineEdit);
    timeline->set_undo_redo(undo_redo);
    timeline_vbox->add_child(timeline);
    timeline->connect("timeline_changed",callable_mp(this, &ClassName::_timeline_changed));
    timeline->connect("name_limit_changed",callable_mp(this, &ClassName::_name_limit_changed));
    timeline->connect("track_added",callable_mp(this, &ClassName::_add_track));
    timeline->connect("value_changed",callable_mp(this, &ClassName::_timeline_value_changed));
    timeline->connect("length_changed",callable_mp(this, &ClassName::_update_length));

    scroll = memnew(ScrollContainer);
    timeline_vbox->add_child(scroll);
    scroll->set_v_size_flags(SIZE_EXPAND_FILL);
    VScrollBar *sb = scroll->get_v_scrollbar();
    scroll->remove_child(sb);
    timeline_scroll->add_child(sb); //move here so timeline and tracks are always aligned
    scroll->connect("gui_input",callable_mp(this, &ClassName::_scroll_input));

    bezier_edit = memnew(AnimationBezierTrackEdit);
    timeline_vbox->add_child(bezier_edit);
    bezier_edit->set_undo_redo(undo_redo);
    bezier_edit->set_editor(this);
    bezier_edit->set_timeline(timeline);
    bezier_edit->hide();
    bezier_edit->set_v_size_flags(SIZE_EXPAND_FILL);
    bezier_edit->connect("close_request",callable_mp(this, &ClassName::_cancel_bezier_edit));

    timeline_vbox->set_custom_minimum_size(Size2(0, 150) * EDSCALE);

    hscroll = memnew(HScrollBar);
    hscroll->share(timeline);
    hscroll->hide();
    hscroll->connect("value_changed",callable_mp(this, &ClassName::_update_scroll));
    timeline_vbox->add_child(hscroll);
    timeline->set_hscroll(hscroll);

    track_vbox = memnew(VBoxContainer);
    scroll->add_child(track_vbox);
    track_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
    scroll->set_enable_h_scroll(false);
    scroll->set_enable_v_scroll(true);
    track_vbox->add_constant_override("separation", 0);

    HBoxContainer *bottom_hb = memnew(HBoxContainer);
    add_child(bottom_hb);

    imported_anim_warning = memnew(Button);
    imported_anim_warning->hide();
    imported_anim_warning->set_tooltip(TTR("Warning: Editing imported animation"));
    imported_anim_warning->connect("pressed",callable_mp(this, &ClassName::_show_imported_anim_warning));
    bottom_hb->add_child(imported_anim_warning);

    bottom_hb->add_spacer();

    selected_filter = memnew(ToolButton);
    selected_filter->connect("pressed",callable_mp(this, &ClassName::_view_group_toggle)); //same function works the same
    selected_filter->set_toggle_mode(true);
    selected_filter->set_tooltip(TTR("Only show tracks from nodes selected in tree."));

    bottom_hb->add_child(selected_filter);

    view_group = memnew(ToolButton);
    view_group->connect("pressed",callable_mp(this, &ClassName::_view_group_toggle));
    view_group->set_toggle_mode(true);
    view_group->set_tooltip(TTR("Group tracks by node or display them as plain list."));

    bottom_hb->add_child(view_group);
    bottom_hb->add_child(memnew(VSeparator));

    snap = memnew(ToolButton);
    snap->set_text(TTR("Snap:") + " ");
    bottom_hb->add_child(snap);
    snap->set_disabled(true);
    snap->set_toggle_mode(true);
    snap->set_pressed(true);

    step = memnew(EditorSpinSlider);
    step->set_min(0);
    step->set_max(1000000);
    step->set_step(0.001);
    step->set_hide_slider(true);
    step->set_custom_minimum_size(Size2(100, 0) * EDSCALE);
    step->set_tooltip(TTR("Animation step value."));
    bottom_hb->add_child(step);
    step->connect("value_changed",callable_mp(this, &ClassName::_update_step));
    step->set_read_only(true);

    snap_mode = memnew(OptionButton);
    snap_mode->add_item(TTR("Seconds"));
    snap_mode->add_item(TTR("FPS"));
    bottom_hb->add_child(snap_mode);
    snap_mode->connect("item_selected",callable_mp(this, &ClassName::_snap_mode_changed));
    snap_mode->set_disabled(true);

    bottom_hb->add_child(memnew(VSeparator));

    zoom_icon = memnew(TextureRect);
    zoom_icon->set_v_size_flags(SIZE_SHRINK_CENTER);
    bottom_hb->add_child(zoom_icon);
    zoom = memnew(HSlider);
    zoom->set_step(0.01);
    zoom->set_min(0.0);
    zoom->set_max(2.0);
    zoom->set_value(1.0);
    zoom->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
    zoom->set_v_size_flags(SIZE_SHRINK_CENTER);
    bottom_hb->add_child(zoom);
    timeline->set_zoom(zoom);

    edit = memnew(MenuButton);
    edit->set_text(TTR("Edit"));
    edit->set_flat(false);
    edit->set_disabled(true);
    edit->set_tooltip(TTR("Animation properties."));
    PopupMenu * edit_popup = edit->get_popup();
    edit_popup->add_item(TTR("Copy Tracks"), EDIT_COPY_TRACKS);
    edit_popup->add_item(TTR("Paste Tracks"), EDIT_PASTE_TRACKS);
    edit_popup->add_separator();
    edit_popup->add_item(TTR("Scale Selection"), EDIT_SCALE_SELECTION);
    edit_popup->add_item(TTR("Scale From Cursor"), EDIT_SCALE_FROM_CURSOR);
    edit_popup->add_separator();
    edit_popup->add_shortcut(ED_SHORTCUT("animation_editor/duplicate_selection", TTR("Duplicate Selection"), KEY_MASK_CMD | KEY_D), EDIT_DUPLICATE_SELECTION);
    edit_popup->add_shortcut(ED_SHORTCUT("animation_editor/duplicate_selection_transposed", TTR("Duplicate Transposed"), KEY_MASK_SHIFT | KEY_MASK_CMD | KEY_D), EDIT_DUPLICATE_TRANSPOSED);
    edit_popup->add_shortcut(ED_SHORTCUT("animation_editor/add_reset_value", TTR("Add RESET Value(s)")));
    edit_popup->set_item_shortcut_disabled(edit_popup->get_item_index(EDIT_DUPLICATE_SELECTION), true);
    edit_popup->set_item_shortcut_disabled(edit_popup->get_item_index(EDIT_DUPLICATE_TRANSPOSED), true);
    edit_popup->add_separator();
    edit_popup->add_shortcut(ED_SHORTCUT("animation_editor/delete_selection", TTR("Delete Selection"), KEY_DELETE), EDIT_DELETE_SELECTION);
    edit_popup->set_item_shortcut_disabled(edit_popup->get_item_index(EDIT_DELETE_SELECTION), true);
    //this shortcut will be checked from the track itself. so no need to enable it here (will conflict with scenetree dock)

    edit_popup->add_separator();
    edit_popup->add_shortcut(ED_SHORTCUT("animation_editor/goto_next_step", TTR("Go to Next Step"), KEY_MASK_CMD | KEY_RIGHT), EDIT_GOTO_NEXT_STEP);
    edit_popup->add_shortcut(ED_SHORTCUT("animation_editor/goto_prev_step", TTR("Go to Previous Step"), KEY_MASK_CMD | KEY_LEFT), EDIT_GOTO_PREV_STEP);
    edit_popup->add_separator();
    edit_popup->add_shortcut(ED_SHORTCUT("animation_editor/apply_reset", TTR("Apply Reset")), EDIT_APPLY_RESET);
    edit_popup->add_separator();
    edit_popup->add_item(TTR("Optimize Animation"), EDIT_OPTIMIZE_ANIMATION);
    edit_popup->add_item(TTR("Clean-Up Animation"), EDIT_CLEAN_UP_ANIMATION);

    edit_popup->connect("id_pressed",callable_mp(this, &ClassName::_edit_menu_pressed));
    edit_popup->connect("about_to_show", callable_mp(this, &ClassName::_edit_menu_about_to_show));

    pick_track = memnew(SceneTreeDialog);
    add_child(pick_track);
    pick_track->register_text_enter(pick_track->get_filter_line_edit());
    pick_track->set_title(TTR("Pick the node that will be animated:"));
    pick_track->connect("selected",callable_mp(this, &ClassName::_new_track_node_selected));
    pick_track->get_filter_line_edit()->connect("text_changed", callable_mp(this, &ClassName::_pick_track_filter_text_changed));
    pick_track->get_filter_line_edit()->connect("gui_input", callable_mp(this, &ClassName::_pick_track_filter_input));
    prop_selector = memnew(PropertySelector);
    add_child(prop_selector);
    prop_selector->connect("selected",callable_mp(this, &ClassName::_new_track_property_selected));

    method_selector = memnew(PropertySelector);
    add_child(method_selector);
    method_selector->connect("selected",callable_mp(this, &ClassName::_add_method_key));

    inserting = false;
    insert_query = false;
    insert_frame = 0;
    insert_queue = false;

    insert_confirm = memnew(ConfirmationDialog);
    add_child(insert_confirm);
    insert_confirm->connect("confirmed",callable_mp(this, &ClassName::_confirm_insert_list));
    VBoxContainer *icvb = memnew(VBoxContainer);
    insert_confirm->add_child(icvb);
    insert_confirm_text = memnew(Label);
    icvb->add_child(insert_confirm_text);
    HBoxContainer *ichb = memnew(HBoxContainer);
    icvb->add_child(ichb);
    insert_confirm_bezier = memnew(CheckBox);
    insert_confirm_bezier->set_text(TTR("Use Bezier Curves"));
    insert_confirm_bezier->set_pressed(EDITOR_GET_T<bool>("editors/animation/default_create_bezier_tracks"));
    ichb->add_child(insert_confirm_bezier);
    insert_confirm_reset = memnew(CheckBox);
    insert_confirm_reset->set_text(TTR("Create RESET Track(s)"));
    insert_confirm_reset->set_pressed(EDITOR_GET_T<bool>("editors/animation/default_create_reset_tracks"));
    ichb->add_child(insert_confirm_reset);
    keying = false;
    moving_selection = false;
    key_edit = nullptr;
    multi_key_edit = nullptr;

    box_selection = memnew(Control);
    add_child(box_selection);
    box_selection->set_as_top_level(true);
    box_selection->set_mouse_filter(MOUSE_FILTER_IGNORE);
    box_selection->hide();
    box_selection->connect("draw",callable_mp(this, &ClassName::_box_selection_draw));
    box_selecting = false;

    //default plugins

    Ref<AnimationTrackEditDefaultPlugin> def_plugin(make_ref_counted<AnimationTrackEditDefaultPlugin>());
    add_track_edit_plugin(def_plugin);

    //dialogs

    optimize_dialog = memnew(ConfirmationDialog);
    add_child(optimize_dialog);
    optimize_dialog->set_title(TTR("Anim. Optimizer"));
    VBoxContainer *optimize_vb = memnew(VBoxContainer);
    optimize_dialog->add_child(optimize_vb);

    optimize_linear_error = memnew(SpinBox);
    optimize_linear_error->set_max(1.0);
    optimize_linear_error->set_min(0.001);
    optimize_linear_error->set_step(0.001);
    optimize_linear_error->set_value(0.05);
    optimize_vb->add_margin_child(TTR("Max. Linear Error:"), optimize_linear_error);
    optimize_angular_error = memnew(SpinBox);
    optimize_angular_error->set_max(1.0);
    optimize_angular_error->set_min(0.001f);
    optimize_angular_error->set_step(0.001f);
    optimize_angular_error->set_value(0.01f);

    optimize_vb->add_margin_child(TTR("Max. Angular Error:"), optimize_angular_error);
    optimize_max_angle = memnew(SpinBox);
    optimize_vb->add_margin_child(TTR("Max Optimizable Angle:"), optimize_max_angle);
    optimize_max_angle->set_max(360.0);
    optimize_max_angle->set_min(0.0);
    optimize_max_angle->set_step(0.1f);
    optimize_max_angle->set_value(22);

    optimize_dialog->get_ok()->set_text(TTR("Optimize"));
    optimize_dialog->connectF("confirmed",this,[=]() {_edit_menu_pressed(EDIT_OPTIMIZE_ANIMATION_CONFIRM);});

    //

    cleanup_dialog = memnew(ConfirmationDialog);
    add_child(cleanup_dialog);
    VBoxContainer *cleanup_vb = memnew(VBoxContainer);
    cleanup_dialog->add_child(cleanup_vb);

    cleanup_keys = memnew(CheckBox);
    cleanup_keys->set_text(TTR("Remove invalid keys"));
    cleanup_keys->set_pressed(true);
    cleanup_vb->add_child(cleanup_keys);

    cleanup_tracks = memnew(CheckBox);
    cleanup_tracks->set_text(TTR("Remove unresolved and empty tracks"));
    cleanup_tracks->set_pressed(true);
    cleanup_vb->add_child(cleanup_tracks);

    cleanup_all = memnew(CheckBox);
    cleanup_all->set_text(TTR("Clean-up all animations"));
    cleanup_vb->add_child(cleanup_all);

    cleanup_dialog->set_title(TTR("Clean-Up Animation(s) (NO UNDO!)"));
    cleanup_dialog->get_ok()->set_text(TTR("Clean-Up"));

    cleanup_dialog->connectF("confirmed",this,[=]() {_edit_menu_pressed(EDIT_CLEAN_UP_ANIMATION_CONFIRM);});

    //
    scale_dialog = memnew(ConfirmationDialog);
    VBoxContainer *vbc = memnew(VBoxContainer);
    scale_dialog->add_child(vbc);

    scale = memnew(SpinBox);
    scale->set_min(-99999);
    scale->set_max(99999);
    scale->set_step(0.001f);
    vbc->add_margin_child(TTR("Scale Ratio:"), scale);
    scale_dialog->connectF("confirmed",this,[=]() {_edit_menu_pressed(EDIT_SCALE_CONFIRM);});
    add_child(scale_dialog);

    track_copy_dialog = memnew(ConfirmationDialog);
    add_child(track_copy_dialog);
    track_copy_dialog->set_title(TTR("Select Tracks to Copy:"));
    track_copy_dialog->get_ok()->set_text(TTR("Copy"));

    VBoxContainer *track_vbox = memnew(VBoxContainer);
    track_copy_dialog->add_child(track_vbox);

    Button *select_all_button = memnew(Button);
    select_all_button->set_text(TTR("Select All/None"));
    select_all_button->connect("pressed",callable_mp(this, &ClassName::_select_all_tracks_for_copy));
    track_vbox->add_child(select_all_button);

    track_copy_select = memnew(Tree);
    track_copy_select->set_h_size_flags(SIZE_EXPAND_FILL);
    track_copy_select->set_v_size_flags(SIZE_EXPAND_FILL);
    track_copy_select->set_hide_root(true);
    track_vbox->add_child(track_copy_select);
    track_copy_dialog->connectF("confirmed",this,[=]() {_edit_menu_pressed(EDIT_COPY_TRACKS_CONFIRM);});
    animation_changing_awaiting_update = false;
}

AnimationTrackEditor::~AnimationTrackEditor() {
    memdelete(key_edit);
    memdelete(multi_key_edit);
}

void register_animation_track_editor_classes()
{
    AnimationBezierTrackEdit::initialize_class();
    AnimationTrackKeyEdit::initialize_class();
    AnimationMultiTrackKeyEdit::initialize_class();

    AnimationTrackEditBool::initialize_class();
    AnimationTrackEditColor::initialize_class();
    AnimationTrackEditAudio::initialize_class();
    AnimationTrackEditSpriteFrame::initialize_class();
    AnimationTrackEditSubAnim::initialize_class();
    AnimationTrackEditTypeAudio::initialize_class();
    AnimationTrackEditTypeAnimation::initialize_class();
    AnimationTrackEditVolumeDB::initialize_class();
    AnimationTrackEditDefaultPlugin::initialize_class();
}
