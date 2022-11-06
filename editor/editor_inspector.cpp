/*************************************************************************/
/*  editor_inspector.cpp                                                 */
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

#include "editor_inspector.h"

#include "array_property_edit.h"
#include "dictionary_property_edit.h"
#include "editor/editor_settings.h"
#include "editor_feature_profile.h"
#include "editor_help.h"
#include "editor_node.h"
#include "editor_property_name_processor.h"
#include "editor_scale.h"
#include "multi_node_edit.h"

#include "core/string_formatter.h"
#include "core/callable_method_pointer.h"
#include "core/doc_support/doc_data.h"
#include "core/ecs_registry.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/translation_helpers.h"
#include "scene/gui/box_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/main/scene_tree.h"
#include "scene/property_utils.h"
#include "scene/resources/font.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/style_box.h"

#include "entt/entity/registry.hpp"
#include "entt/meta/factory.hpp"
#include "entt/meta/meta.hpp"
#include "entt/meta/resolve.hpp"

extern ECS_Registry<GameEntity, true> game_object_registry;

IMPL_GDCLASS(EditorProperty)
IMPL_GDCLASS(EditorInspectorPlugin)
IMPL_GDCLASS(EditorInspectorCategory)
IMPL_GDCLASS(EditorInspectorSection)
IMPL_GDCLASS(EditorInspector)

bool _property_path_matches(StringView p_property_path, StringView p_filter, EditorPropertyNameStyle p_style) {
    if (StringUtils::contains(p_property_path,p_filter,StringUtils::CaseInsensitive)) {
        return true;
    }

    FixedVector<StringView,8,true> sections;
    String::split_ref(sections,p_property_path,'/');
    for (StringView part : sections) {
        if (StringUtils::contains(p_filter,EditorPropertyNameProcessor::process_name(part, p_style),StringUtils::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

static Variant get_property_revert_value(Object *p_object, const StringName &p_property, bool *r_is_valid) {
    // If the object implements property_can_revert, rely on that completely
    // (i.e. don't then try to revert to default value - the property_get_revert implementation
    // can do that if so desired)
    if (p_object->has_method("property_can_revert") && p_object->call_va("property_can_revert", p_property)) {
        if (r_is_valid) {
            *r_is_valid = true;
        }
        return p_object->call_va("property_get_revert", p_property);
    }
    return PropertyUtils::get_property_default_value(p_object, p_property, r_is_valid);
}

Size2 EditorProperty::get_minimum_size() const {
    Size2 ms;
    Ref<Font> font = get_theme_font("font", "Tree");
    ms.height = font->get_height();

    for (int i = 0; i < get_child_count(); i++) {
        Control *c = object_cast<Control>(get_child(i));
        if (!c) {
            continue;
        }
        if (c->is_set_as_top_level()) {
            continue;
        }
        if (!c->is_visible()) {
            continue;
        }
        if (c == bottom_editor) {
            continue;
        }

        Size2 minsize = c->get_combined_minimum_size();
        ms.width = M_MAX(ms.width, minsize.width);
        ms.height = M_MAX(ms.height, minsize.height);
    }

    if (keying) {
        Ref<Texture> key = get_theme_icon("Key", "EditorIcons");
        ms.width += key->get_width() + get_theme_constant("hseparator", "Tree");
    }

    if (checkable) {
        Ref<Texture> check = get_theme_icon("checked", "CheckBox");
        ms.width += check->get_width() + get_theme_constant("hseparation", "CheckBox") +
                    get_theme_constant("hseparator", "Tree");
    }

    if (bottom_editor != nullptr && bottom_editor->is_visible()) {
        ms.height += get_theme_constant("vseparation");
        Size2 bems = bottom_editor->get_combined_minimum_size();
        // bems.width += get_constant("item_margin", "Tree");
        ms.height += bems.height;
        ms.width = M_MAX(ms.width, bems.width);
    }

    return ms;
}

void EditorProperty::emit_changed(
        const StringName &p_property, const Variant &p_value, const StringName &p_field, bool p_changing) {
    emit_signal("property_changed", p_property, p_value, p_field, p_changing);
}

void EditorProperty::_notification(int p_what) {
    if (p_what == NOTIFICATION_SORT_CHILDREN) {
        Size2 size = get_size();
        Rect2 rect;
        Rect2 bottom_rect;

        right_child_rect = Rect2();
        bottom_child_rect = Rect2();

        {
            int child_room = size.width * (1.0 - split_ratio);
            Ref<Font> font = get_theme_font("font", "Tree");
            int height = font->get_height();
            bool no_children = true;

            // compute room needed
            for (int i = 0; i < get_child_count(); i++) {
                Control *c = object_cast<Control>(get_child(i));
                if (!c)
                    continue;
                if (c->is_set_as_top_level())
                    continue;
                if (c == bottom_editor)
                    continue;

                Size2 minsize = c->get_combined_minimum_size();
                child_room = M_MAX(child_room, minsize.width);
                height = M_MAX(height, minsize.height);
                no_children = false;
            }

            if (no_children) {
                text_size = size.width;
                rect = Rect2(size.width - 1, 0, 1, height);
            } else {
                text_size = M_MAX(0, size.width - (child_room + 4 * EDSCALE));
                rect = Rect2(size.width - child_room, 0, child_room, height);
            }

            if (bottom_editor) {
                int m = 0; // get_constant("item_margin", "Tree");

                bottom_rect = Rect2(m, rect.size.height + get_theme_constant("vseparation"), size.width - m,
                        bottom_editor->get_combined_minimum_size().height);
            }

            if (keying) {
                Ref<Texture> key;

                if (use_keying_next()) {
                    key = get_theme_icon("KeyNext", "EditorIcons");
                } else {
                    key = get_theme_icon("Key", "EditorIcons");
                }

                rect.size.x -= key->get_width() + get_theme_constant("hseparator", "Tree");

                if (no_children) {
                    text_size -= key->get_width() + 4 * EDSCALE;
                }
            }
        }

        // set children
        for (int i = 0; i < get_child_count(); i++) {
            Control *c = object_cast<Control>(get_child(i));
            if (!c)
                continue;
            if (c->is_set_as_top_level())
                continue;
            if (c == bottom_editor)
                continue;

            fit_child_in_rect(c, rect);
            right_child_rect = rect;
        }

        if (bottom_editor) {
            fit_child_in_rect(bottom_editor, bottom_rect);
            bottom_child_rect = bottom_rect;
        }

        update(); // need to redraw text
    }

    if (p_what == NOTIFICATION_DRAW) {
        Ref<Font> font = get_theme_font("font", "Tree");
        Color dark_color = get_theme_color("dark_color_2", "Editor");

        Size2 size = get_size();
        if (bottom_editor) {
            size.height = bottom_editor->get_margin(Margin::Top);
        } else if (label_reference) {
            size.height = label_reference->get_size().height;
        }

        Ref<StyleBox> sb = get_theme_stylebox(StringName(selected ? "bg_selected" : "bg"));

        draw_style_box(sb, Rect2(Vector2(), size));

        if (draw_top_bg && right_child_rect != Rect2()) {
            draw_rect_filled(right_child_rect, dark_color);
        }
        if (bottom_child_rect != Rect2()) {
            draw_rect_filled(bottom_child_rect, dark_color);
        }

        Color color;
        if (draw_red) {
            color = get_theme_color("warning_color");
        } else {
            color = get_theme_color("property_color");
        }
        if (StringUtils::contains(label, '.')) {
            color.a = 0.5; // this should be un-hacked honestly, as it's used for editor overrides
        }

        int ofs = get_theme_constant("font_offset");
        int text_limit = text_size - ofs;

        if (checkable) {
            Ref<Texture> checkbox;
            if (checked)
                checkbox = get_theme_icon("GuiChecked", "EditorIcons");
            else
                checkbox = get_theme_icon("GuiUnchecked", "EditorIcons");

            Color color2(1, 1, 1);
            if (check_hover) {
                color2.r *= 1.2f;
                color2.g *= 1.2f;
                color2.b *= 1.2f;
            }
            check_rect = Rect2(
                    ofs, (size.height - checkbox->get_height()) / 2, checkbox->get_width(), checkbox->get_height());
            draw_texture(checkbox, check_rect.position, color2);

            int check_ofs = get_theme_constant("hseparator", "Tree") + checkbox->get_width() +
                            get_theme_constant("hseparation", "CheckBox");
            ofs += check_ofs;
            text_limit -= check_ofs;
        } else {
            check_rect = Rect2();
        }

        if (can_revert) {
            Ref<Texture> reload_icon = get_theme_icon("ReloadSmall", "EditorIcons");
            text_limit -= reload_icon->get_width() + get_theme_constant("hseparator", "Tree") * 2;
            revert_rect = Rect2(ofs + text_limit, (size.height - reload_icon->get_height()) / 2,
                    reload_icon->get_width(), reload_icon->get_height());

            Color color2(1, 1, 1);
            if (revert_hover) {
                color2.r *= 1.2f;
                color2.g *= 1.2f;
                color2.b *= 1.2f;
            }

            draw_texture(reload_icon, revert_rect.position, color2);
        } else {
            revert_rect = Rect2();
        }

        if (!pin_hidden && is_pinned) {
            Ref<Texture> pinned_icon = get_theme_icon("Pin", "EditorIcons");
            int margin_w = get_theme_constant("hseparator", "Tree") * 2;
            text_limit -= margin_w + pinned_icon->get_width();
            int text_w = MIN(font->get_string_size(label).x, text_limit);
            draw_texture(pinned_icon, Vector2(ofs + text_w + margin_w, (size.height - pinned_icon->get_height()) / 2),
                    color);
        }

        int v_ofs = (size.height - font->get_height()) / 2;
        draw_ui_string(font, Point2(ofs, v_ofs + font->get_ascent()), StringUtils::from_utf8(label), color, text_limit);

        if (keying) {
            Ref<Texture> key;

            if (use_keying_next()) {
                key = get_theme_icon("KeyNext", "EditorIcons");
            } else {
                key = get_theme_icon("Key", "EditorIcons");
            }

            ofs = size.width - key->get_width() - get_theme_constant("hseparator", "Tree");

            Color color2(1, 1, 1);
            if (keying_hover) {
                color2.r *= 1.2f;
                color2.g *= 1.2f;
                color2.b *= 1.2f;
            }
            keying_rect = Rect2(ofs, (size.height - key->get_height()) / 2, key->get_width(), key->get_height());
            draw_texture(key, keying_rect.position, color2);
        } else {
            keying_rect = Rect2();
        }
    }
}

void EditorProperty::set_label(StringView p_label) {
    label = p_label;
    update();
}

const String &EditorProperty::get_label() const {
    return label;
}

Object *EditorProperty::get_edited_object() {
    return object;
}

StringName EditorProperty::get_edited_property() {
    return property;
}

void EditorProperty::update_property() {
    if (get_script_instance())
        get_script_instance()->call("update_property");
}

void EditorProperty::set_read_only(bool p_read_only) {
    read_only = p_read_only;
}

bool EditorProperty::is_read_only() const {
    return read_only;
}

bool EditorPropertyRevert::can_property_revert(Object *p_object, const StringName &p_property) {
    bool is_valid_revert = false;
    Variant revert_value = get_property_revert_value(p_object, p_property, &is_valid_revert);
    if (!is_valid_revert) {
        return false;
    }
    Variant current_value = p_object->get(p_property);
    return PropertyUtils::is_property_value_different(current_value, revert_value);
}

void EditorProperty::update_revert_and_pin_status() {
    if (property.empty()) {
        return; // no property, so nothing to do
    }

    bool new_is_pinned = false;
    if (can_pin) {
        Node *node = object_cast<Node>(object);
        CRASH_COND(!node);
        new_is_pinned = node->is_property_pinned(property);
    }
    bool new_can_revert = EditorPropertyRevert::can_property_revert(object, property);

    if (new_can_revert != can_revert || new_is_pinned != is_pinned) {
        can_revert = new_can_revert;
        is_pinned = new_is_pinned;
        update();
    }
}

bool EditorProperty::use_keying_next() const {
    Vector<PropertyInfo> plist;
    object->get_property_list(&plist, true);

    for (const PropertyInfo &p : plist) {
        if (p.name == property) {
            return p.usage & PROPERTY_USAGE_KEYING_INCREMENTS;
        }
    }

    return false;
}
void EditorProperty::set_checkable(bool p_checkable) {
    checkable = p_checkable;
    update();
    queue_sort();
}

void EditorProperty::set_checked(bool p_checked) {
    checked = p_checked;
    update();
}

void EditorProperty::set_draw_red(bool p_draw_red) {
    draw_red = p_draw_red;
    update();
}

void EditorProperty::set_keying(bool p_keying) {
    keying = p_keying;
    update();
    queue_sort();
}

bool EditorProperty::is_keying() const {
    return keying;
}

bool EditorProperty::is_draw_red() const {
    return draw_red;
}

void EditorProperty::_focusable_focused(int p_index) {
    if (!selectable)
        return;
    bool already_selected = selected;
    selected = true;
    selected_focusable = p_index;
    update();
    if (!already_selected && selected) {
        emit_signal("selected", property, selected_focusable);
    }
}

void EditorProperty::add_focusable(Control *p_control) {
    p_control->connectF("focus_entered", this, [this, sz = focusables.size()]() { _focusable_focused(sz); });
    focusables.push_back(p_control);
}

void EditorProperty::select(int p_focusable) {
    bool already_selected = selected;

    if (p_focusable >= 0) {
        ERR_FAIL_INDEX(p_focusable, focusables.size());
        focusables[p_focusable]->grab_focus();
    } else {
        selected = true;
        update();
    }

    if (!already_selected && selected) {
        emit_signal("selected", property, selected_focusable);
    }
}

void EditorProperty::deselect() {
    selected = false;
    selected_focusable = -1;
    update();
}

void EditorProperty::_gui_input(const Ref<InputEvent> &p_event) {
    if (property == StringName())
        return;

    Ref<InputEventMouse> me = dynamic_ref_cast<InputEventMouse>(p_event);

    if (me) {
        bool button_left = me->get_button_mask() & BUTTON_MASK_LEFT;

        bool new_keying_hover = keying_rect.has_point(me->get_position()) && !button_left;
        if (new_keying_hover != keying_hover) {
            keying_hover = new_keying_hover;
            update();
        }

        bool new_revert_hover = revert_rect.has_point(me->get_position()) && !button_left;
        if (new_revert_hover != revert_hover) {
            revert_hover = new_revert_hover;
            update();
        }

        bool new_check_hover = check_rect.has_point(me->get_position()) && !button_left;
        if (new_check_hover != check_hover) {
            check_hover = new_check_hover;
            update();
        }
    }

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);

    if (mb && mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
        if (!selected && selectable) {
            selected = true;
            emit_signal("selected", property, -1);
            update();
        }

        if (keying_rect.has_point(mb->get_position())) {
            emit_signal("property_keyed", property, use_keying_next());

            if (use_keying_next()) {
                if (property == "frame_coords" && (object->is_class("Sprite2D") || object->is_class("Sprite3D"))) {
                    Vector2 new_coords = object->get(property).as<Vector2>();
                    new_coords.x++;
                    if (new_coords.x >= object->get("hframes").as<int64_t>()) {
                        new_coords.x = 0;
                        new_coords.y++;
                    }

                    call_deferred([this, new_coords] { emit_changed(property, new_coords, "", false); });
                } else {
                    call_deferred(
                            [this] { emit_changed(property, object->get(property).as<int64_t>() + 1, "", false); });
                }

                call_deferred([this] { update_property(); });
            }
        }
        if (revert_rect.has_point(mb->get_position())) {
            bool is_valid_revert = false;
            Variant revert_value = get_property_revert_value(object, property, &is_valid_revert);
            ERR_FAIL_COND(!is_valid_revert);
            emit_changed(property, revert_value);
            update_property();
        }
        if (check_rect.has_point(mb->get_position())) {
            checked = !checked;
            update();
            emit_signal("property_checked", property, checked);
        }
    } else if (mb && mb->is_pressed() && mb->get_button_index() == BUTTON_RIGHT) {
        _update_popup();
        if (menu->get_item_count()) {
            menu->set_position(get_global_mouse_position());
            menu->set_as_minsize();
            menu->popup();
            select();
        }
    }
}

void EditorProperty::_unhandled_key_input(const Ref<InputEvent> &p_event) {
    if (!selected || !is_visible_in_tree()) {
        return;
    }

    const Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_event);

    if (k && k->is_pressed()) {
        if (ED_IS_SHORTCUT("property_editor/copy_property", p_event)) {
            _menu_option(MENU_COPY_PROPERTY);
            accept_event();
        } else if (ED_IS_SHORTCUT("property_editor/paste_property", p_event) && !is_read_only()) {
            _menu_option(MENU_PASTE_PROPERTY);
            accept_event();
        } else if (ED_IS_SHORTCUT("property_editor/copy_property_path", p_event)) {
            _menu_option(MENU_COPY_PROPERTY_PATH);
            accept_event();
        }
    }
}

void EditorProperty::set_label_reference(Control *p_control) {
    label_reference = p_control;
}
void EditorProperty::set_bottom_editor(Control *p_control) {
    bottom_editor = p_control;
}
Variant EditorProperty::get_drag_data(const Point2 &p_point) {
    if (property.empty())
        return Variant();

    Dictionary dp;
    dp["type"] = "obj_property";
    dp["object"] = Variant(object);
    dp["property"] = property;
    dp["value"] = object->get(property);

    Label *label = memnew(Label);
    label->set_text(property);
    set_drag_preview(label);
    return dp;
}

void EditorProperty::expand_all_folding() {}

void EditorProperty::collapse_all_folding() {}

void EditorProperty::set_object_and_property(Object *p_object, const StringName &p_property) {
    object = p_object;
    property = p_property;
    _update_pin_flags();
}

static bool _is_value_potential_override(Node *p_node, const StringName &p_property) {
    // Consider a value is potentially overriding another if either of the following is true:
    // a) The node is foreign (inheriting or an instance), so the original value may come from another scene.
    // b) The node belongs to the scene, but the original value comes from somewhere but the builtin class (i.e., a
    // script).
    Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
    Dequeue<SceneState::PackState> states_stack = PropertyUtils::get_node_states_stack(p_node, edited_scene);
    if (!states_stack.empty()) {
        return true;
    }
    bool is_valid_default = false;
    bool is_class_default = false;
    PropertyUtils::get_property_default_value(
            p_node, p_property, &is_valid_default, &states_stack, false, nullptr, &is_class_default);
    return !is_class_default;
}

void EditorProperty::_update_pin_flags() {
    can_pin = false;
    pin_hidden = true;
    if (read_only) {
        return;
    }
    Node *node = object_cast<Node>(object);
    if (!node) {
        return;
    }
    // Avoid errors down the road by ignoring nodes which are not part of a scene
    if (!node->get_owner()) {
        bool is_scene_root = false;
        for (int i = 0; i < EditorNode::get_editor_data().get_edited_scene_count(); ++i) {
            if (EditorNode::get_editor_data().get_edited_scene_root(i) == node) {
                is_scene_root = true;
                break;
            }
        }
        if (!is_scene_root) {
            return;
        }
    }
    if (!_is_value_potential_override(node, property)) {
        return;
    }
    pin_hidden = false;
    {
        Set<StringName> storable_properties;
        node->get_storable_properties(storable_properties);
        if (storable_properties.contains(node->get_property_store_alias(property))) {
            can_pin = true;
        }
    }
}

Control *EditorProperty::make_custom_tooltip(StringView p_text) const {
    EditorHelpBit *help_bit = memnew(EditorHelpBit);
    help_bit->add_theme_style_override("panel", get_theme_stylebox("panel", "TooltipPanel"));
    help_bit->get_rich_text()->set_fixed_size_to_width(360 * EDSCALE);

    auto slices = StringUtils::split(p_text, "::", false);

    if (!slices.empty()) {
        StringView property_name = StringUtils::strip_edges(slices[0]);
        String text = String(TTR("Property:")) + " [u][b]" + property_name + "[/b][/u]\n";

        if (slices.size() > 1) {
            StringView property_doc = StringUtils::strip_edges(slices[1]);
            if (property_name != property_doc) {
                text += "\n" + property_doc;
            }
        }
        // hack so it uses proper theme once inside scene
        help_bit->call_deferred([help_bit, text] { help_bit->set_text(text); });
    }

    return help_bit;
}

void EditorProperty::_bind_methods() {
    SE_BIND_METHOD(EditorProperty,set_label);
    SE_BIND_METHOD(EditorProperty,get_label);

    SE_BIND_METHOD(EditorProperty,set_read_only);
    SE_BIND_METHOD(EditorProperty,is_read_only);

    SE_BIND_METHOD(EditorProperty,set_checkable);
    SE_BIND_METHOD(EditorProperty,is_checkable);

    SE_BIND_METHOD(EditorProperty,set_checked);
    SE_BIND_METHOD(EditorProperty,is_checked);

    SE_BIND_METHOD(EditorProperty,set_draw_red);
    SE_BIND_METHOD(EditorProperty,is_draw_red);

    SE_BIND_METHOD(EditorProperty,set_keying);
    SE_BIND_METHOD(EditorProperty,is_keying);

    SE_BIND_METHOD(EditorProperty,get_edited_property);
    SE_BIND_METHOD(EditorProperty,get_edited_object);

    SE_BIND_METHOD(EditorProperty,_gui_input);

    SE_BIND_METHOD(EditorProperty,add_focusable);
    SE_BIND_METHOD(EditorProperty,set_bottom_editor);

    MethodBinder::bind_method(D_METHOD("emit_changed", { "property", "value", "field", "changing" }),
            &EditorProperty::emit_changed, { DEFVAL(StringName()), DEFVAL(false) });

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "label"), "set_label", "get_label");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "read_only"), "set_read_only", "is_read_only");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "checkable"), "set_checkable", "is_checkable");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "checked"), "set_checked", "is_checked");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "draw_red"), "set_draw_red", "is_draw_red");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "keying"), "set_keying", "is_keying");
    ADD_SIGNAL(MethodInfo("property_changed", PropertyInfo(VariantType::STRING_NAME, "property"),
            PropertyInfo(VariantType::NIL, "value", PropertyHint::None, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
    ADD_SIGNAL(MethodInfo("multiple_properties_changed", PropertyInfo(VariantType::POOL_STRING_ARRAY, "properties"),
            PropertyInfo(VariantType::ARRAY, "value")));
    ADD_SIGNAL(MethodInfo("property_keyed", PropertyInfo(VariantType::STRING_NAME, "property")));
    ADD_SIGNAL(MethodInfo("property_keyed_with_value", PropertyInfo(VariantType::STRING_NAME, "property"),
            PropertyInfo(VariantType::NIL, "value", PropertyHint::None, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
    ADD_SIGNAL(MethodInfo("property_checked", PropertyInfo(VariantType::STRING_NAME, "property"),
            PropertyInfo(VariantType::BOOL, "checked")));
    ADD_SIGNAL(MethodInfo("property_pinned", PropertyInfo(VariantType::STRING_NAME, "property"),
            PropertyInfo(VariantType::BOOL, "pinned")));
    ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(VariantType::STRING, "path"),
            PropertyInfo(VariantType::OBJECT, "resource", PropertyHint::ResourceType, "Resource")));
    ADD_SIGNAL(MethodInfo("object_id_selected", PropertyInfo(VariantType::STRING_NAME, "property"),
            PropertyInfo(VariantType::INT, "id")));
    ADD_SIGNAL(MethodInfo(
            "selected", PropertyInfo(VariantType::STRING, "path"), PropertyInfo(VariantType::INT, "focusable_idx")));

    MethodInfo vm;
    vm.name = "update_property";
    BIND_VMETHOD(vm);
}

EditorProperty::EditorProperty() {
    selectable = true;
    text_size = 0;
    read_only = false;
    checkable = false;
    checked = false;
    draw_red = false;
    keying = false;
    keying_hover = false;
    revert_hover = false;
    check_hover = false;
    can_revert = false;
    can_pin = false;
    pin_hidden = false;
    is_pinned = false;
    use_folding = false;
    property_usage = 0;
    selected = false;
    selected_focusable = -1;
    label_reference = nullptr;
    bottom_editor = nullptr;
    menu = nullptr;

    set_process_unhandled_key_input(true);
}
void EditorProperty::_update_popup() {
    if (menu) {
        menu->clear();
    } else {
        menu = memnew(PopupMenu);
        add_child(menu);
        menu->connect("id_pressed", callable_mp(this, &EditorProperty::_menu_option));
    }

    menu->add_shortcut(ED_GET_SHORTCUT("property_editor/copy_property"), MENU_COPY_PROPERTY);
    menu->add_shortcut(ED_GET_SHORTCUT("property_editor/paste_property"), MENU_PASTE_PROPERTY);
    menu->add_shortcut(ED_GET_SHORTCUT("property_editor/copy_property_path"), MENU_COPY_PROPERTY_PATH);
    menu->set_item_disabled(MENU_PASTE_PROPERTY, is_read_only());

    if (!pin_hidden) {
        menu->add_separator();
        if (can_pin) {
            menu->add_check_item(TTR("Pin value"), MENU_PIN_VALUE);
            menu->set_item_checked(menu->get_item_index(MENU_PIN_VALUE), is_pinned);
            menu->set_item_tooltip(menu->get_item_index(MENU_PIN_VALUE),
                    TTR("Pinning a value forces it to be saved even if it's equal to the default."));
        } else {
            menu->add_check_item(
                    FormatSN(TTR("Pin value [Disabled because '%s' is editor-only]").asCString(), property.asCString()), MENU_PIN_VALUE);
            menu->set_item_disabled(menu->get_item_index(MENU_PIN_VALUE), true);
        }
    }
}

void EditorProperty::_menu_option(int p_option) {
    switch (p_option) {
        case MENU_PIN_VALUE: {
            emit_signal("property_pinned", property, !is_pinned);
            update();
        } break;
        case MENU_COPY_PROPERTY: {
            EditorNode::get_singleton()->get_inspector()->set_property_clipboard(object->get(property));
        } break;
        case MENU_PASTE_PROPERTY: {
            emit_changed(property, EditorNode::get_singleton()->get_inspector()->get_property_clipboard());
        } break;
        case MENU_COPY_PROPERTY_PATH: {
            OS::get_singleton()->set_clipboard(property_path);
        } break;
    }
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorPlugin::add_custom_control(Control *control) {
    AddedEditor ae;
    ae.property_editor = control;
    added_editors.emplace_back(ae);
}

void EditorInspectorPlugin::add_property_editor(StringView p_for_property, Control *p_prop) {
    ERR_FAIL_COND(object_cast<EditorProperty>(p_prop) == nullptr);

    AddedEditor ae;
    ae.properties.emplace_back(p_for_property);
    ae.property_editor = p_prop;
    added_editors.emplace_back(ae);
}

void EditorInspectorPlugin::add_property_editor_for_multiple_properties(
        StringView p_label, const Vector<String> &p_properties, Control *p_prop) {
    AddedEditor ae;
    ae.properties = p_properties;
    ae.property_editor = p_prop;
    ae.label = p_label;
    added_editors.emplace_back(ae);
}

bool EditorInspectorPlugin::can_handle(Object *p_object) {
    if (get_script_instance()) {
        return get_script_instance()->call("can_handle", Variant(p_object)).as<bool>();
    }
    return false;
}
void EditorInspectorPlugin::parse_begin(Object *p_object) {
    if (get_script_instance()) {
        get_script_instance()->call("parse_begin", Variant(p_object));
    }
}

void EditorInspectorPlugin::parse_category(Object *p_object, StringView p_parse_category) {
    if (get_script_instance()) {
        get_script_instance()->call("parse_category", Variant(p_object), p_parse_category);
    }
}

bool EditorInspectorPlugin::parse_property(Object *p_object, VariantType p_type, StringView p_path, PropertyHint p_hint,
        StringView p_hint_text, int p_usage) {
    if (get_script_instance()) {
        Variant arg[6] = { Variant(p_object), p_type, p_path, p_hint, p_hint_text, p_usage };
        const Variant *argptr[6] = { &arg[0], &arg[1], &arg[2], &arg[3], &arg[4], &arg[5] };

        Callable::CallError err;
        return get_script_instance()->call("parse_property", (const Variant **)&argptr, 6, err).as<bool>();
    }
    return false;
}
void EditorInspectorPlugin::parse_end() {
    if (get_script_instance()) {
        get_script_instance()->call("parse_end");
    }
}

void EditorInspectorPlugin::_bind_methods() {
    MethodBinder::bind_method(
            D_METHOD("add_custom_control", { "control" }), &EditorInspectorPlugin::add_custom_control);
    MethodBinder::bind_method(
            D_METHOD("add_property_editor", { "property", "editor" }), &EditorInspectorPlugin::add_property_editor);
    MethodBinder::bind_method(
            D_METHOD("add_property_editor_for_multiple_properties", { "label", "properties", "editor" }),
            &EditorInspectorPlugin::add_property_editor_for_multiple_properties);

    MethodInfo vm(VariantType::BOOL, "can_handle", PropertyInfo(VariantType::OBJECT, "object"));
    BIND_VMETHOD(vm);
    vm.name = "parse_begin";
    vm.return_val.type = VariantType::NIL;
    BIND_VMETHOD(vm);
    vm.name = "parse_category";
    vm.arguments.emplace_back(VariantType::STRING, "category");
    BIND_VMETHOD(vm);
    vm.arguments.pop_back();
    vm.name = "parse_property";
    vm.return_val.type = VariantType::BOOL;
    vm.arguments.emplace_back(VariantType::INT, "type");
    vm.arguments.emplace_back(VariantType::STRING, "path");
    vm.arguments.emplace_back(VariantType::INT, "hint");
    vm.arguments.emplace_back(VariantType::STRING, "hint_text");
    vm.arguments.emplace_back(VariantType::INT, "usage");
    BIND_VMETHOD(vm);
    vm.arguments.clear();
    vm.name = "parse_end";
    vm.return_val.type = VariantType::NIL;
    BIND_VMETHOD(vm);
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorCategory::_notification(int p_what) {
    if (p_what != NOTIFICATION_DRAW) {
        return;
    }

    draw_rect_filled(Rect2(Vector2(), get_size()), bg_color);
    Ref<Font> font = get_theme_font("font", "Tree");

    int hs = get_theme_constant("hseparation", "Tree");
    int w = font->get_string_size(label).width;

    if (icon) {
        w += hs + icon->get_width();
    }

    int ofs = (get_size().width - w) / 2;

    if (icon) {
        draw_texture(icon, Point2(ofs, (get_size().height - icon->get_height()) / 2).floor());
        ofs += hs + icon->get_width();
    }

    Color color = get_theme_color("font_color", "Tree");
    draw_string(font, Point2(ofs, font->get_ascent() + (get_size().height - font->get_height()) / 2).floor(), label,
            color, get_size().width);
}

Control *EditorInspectorCategory::make_custom_tooltip(StringView p_text) const {
    EditorHelpBit *help_bit = memnew(EditorHelpBit);
    help_bit->add_theme_style_override("panel", get_theme_stylebox("panel", "TooltipPanel"));
    help_bit->get_rich_text()->set_fixed_size_to_width(360 * EDSCALE);

    auto slices = StringUtils::split(p_text, "::", false);
    if (!slices.empty()) {
        auto property_name = StringUtils::strip_edges(slices[0]);
        String text = "[u][b]" + String(property_name) + "[/b][/u]";

        if (slices.size() > 1) {
            auto property_doc = StringUtils::strip_edges(slices[1]);
            if (property_name != property_doc) {
                text += "\n" + property_doc;
            }
        }
        // hack so it uses proper theme once inside scene
        help_bit->call_deferred([help_bit, text] { help_bit->set_text(text); });
    }
    return help_bit;
}

Size2 EditorInspectorCategory::get_minimum_size() const {
    Ref<Font> font = get_theme_font("font", "Tree");

    Size2 ms;
    ms.width = 1;
    ms.height = font->get_height();
    if (icon) {
        ms.height = M_MAX(icon->get_height(), ms.height);
    }
    ms.height += get_theme_constant("vseparation", "Tree");

    return ms;
}

void EditorInspectorCategory::_bind_methods() {
}

EditorInspectorCategory::EditorInspectorCategory() = default;

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorSection::_test_unfold() {
    if (!vbox_added) {
        add_child(vbox);
        vbox_added = true;
    }
}

void EditorInspectorSection::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_SORT_CHILDREN: {
            Ref<Font> font = get_theme_font("font", "Tree");
            Ref<Texture> arrow;

            if (foldable) {
                bool unfolded = object->get_tooling_interface()->editor_is_section_unfolded(section);
                arrow = get_theme_icon(unfolded ? StringName("arrow") : StringName("arrow_collapsed"), "Tree");
            }

            Size2 size = get_size();
            Point2 offset;
            offset.y = font->get_height();
            if (arrow) {
                offset.y = M_MAX(offset.y, arrow->get_height());
            }

            offset.y += get_theme_constant("vseparation", "Tree");
            offset.x += get_theme_constant("inspector_margin", "Editor");

            Rect2 rect(offset, size - offset);

            // set children
            for (int i = 0; i < get_child_count(); i++) {
                Control *c = object_cast<Control>(get_child(i));
                if (!c)
                    continue;
                if (c->is_set_as_top_level())
                    continue;
                if (!c->is_visible_in_tree())
                    continue;

                fit_child_in_rect(c, rect);
            }

            update(); // need to redraw text
            break;
        }
        case NOTIFICATION_DRAW: {
            Ref<Texture> arrow;

            if (foldable) {
                bool unfolded = object->get_tooling_interface()->editor_is_section_unfolded(section);
                arrow = get_theme_icon(unfolded ? StringName("arrow") : StringName("arrow_collapsed"), "Tree");
            }

            Ref<Font> font = get_theme_font("font", "Tree");

            int h = font->get_height();
            if (arrow) {
                h = M_MAX(h, arrow->get_height());
            }
            h += get_theme_constant("vseparation", "Tree");

            Rect2 header_rect = Rect2(Vector2(), Vector2(get_size().width, h));
            Color c = bg_color;
            c.a *= 0.4f;
            if (foldable && header_rect.has_point(get_local_mouse_position())) {
                c = c.lightened(Input::get_singleton()->is_mouse_button_pressed(BUTTON_LEFT) ? -0.05 : 0.2);
            }
            draw_rect_filled(header_rect, c);

            const int arrow_margin = 3;

            Color color = get_theme_color("font_color", "Tree");
            draw_string(font,
                    Point2(Math::round((16 + arrow_margin) * EDSCALE),
                            font->get_ascent() + (h - font->get_height()) / 2)
                            .floor(),
                    label, color, get_size().width);

            if (arrow) {
                draw_texture(arrow, Point2(Math::round(arrow_margin * EDSCALE), (h - arrow->get_height()) / 2).floor());
            }
            break;
        }
        case NOTIFICATION_MOUSE_ENTER:
        case NOTIFICATION_MOUSE_EXIT: {
            update();
        } break;
    }
}

Size2 EditorInspectorSection::get_minimum_size() const {
    Size2 ms;
    for (int i = 0; i < get_child_count(); i++) {
        Control *c = object_cast<Control>(get_child(i));
        if (!c)
            continue;
        if (c->is_set_as_top_level())
            continue;
        if (!c->is_visible())
            continue;
        Size2 minsize = c->get_combined_minimum_size();
        ms.width = M_MAX(ms.width, minsize.width);
        ms.height = M_MAX(ms.height, minsize.height);
    }

    Ref<Font> font = get_theme_font("font", "Tree");
    ms.height += font->get_height() + get_theme_constant("vseparation", "Tree");
    ms.width += get_theme_constant("inspector_margin", "Editor");

    return ms;
}

void EditorInspectorSection::setup(
        StringView p_section, StringView p_label, Object *p_object, const Color &p_bg_color, bool p_foldable) {
    section = p_section;
    label = p_label;
    object = p_object;
    bg_color = p_bg_color;
    foldable = p_foldable;
    if (!foldable && !vbox_added) {
        add_child(vbox);
        vbox_added = true;
    }

    if (foldable) {
        _test_unfold();
        if (object->get_tooling_interface()->editor_is_section_unfolded(section)) {
            vbox->show();
        } else {
            vbox->hide();
        }
    }
}

void EditorInspectorSection::_gui_input(const Ref<InputEvent> &p_event) {
    if (!foldable)
        return;

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);
    if (!mb) {
        return;
    }
    if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
        Ref<Font> font = get_theme_font("font", "Tree");
        if (mb->get_position().y > font->get_height()) { // clicked outside
            return;
        }

        _test_unfold();

        bool unfold = !object->get_tooling_interface()->editor_is_section_unfolded(section);
        object->get_tooling_interface()->editor_set_section_unfold(section, unfold);
        if (unfold) {
            vbox->show();
        } else {
            vbox->hide();
        }
    } else if (!mb->is_pressed()) {
        update();
    }
}

VBoxContainer *EditorInspectorSection::get_vbox() {
    return vbox;
}

void EditorInspectorSection::unfold() {
    if (!foldable)
        return;

    _test_unfold();

    object->get_tooling_interface()->editor_set_section_unfold(section, true);
    vbox->show();
    update();
}

void EditorInspectorSection::fold() {
    if (!foldable)
        return;

    if (!vbox_added)
        return; // kinda pointless

    object->get_tooling_interface()->editor_set_section_unfold(section, false);
    vbox->hide();
    update();
}

void EditorInspectorSection::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("setup", { "section", "label", "object", "bg_color", "foldable" }),
            &EditorInspectorSection::setup);
    SE_BIND_METHOD(EditorInspectorSection,get_vbox);
    SE_BIND_METHOD(EditorInspectorSection,unfold);
    SE_BIND_METHOD(EditorInspectorSection,fold);
    SE_BIND_METHOD(EditorInspectorSection,_gui_input);
}

EditorInspectorSection::EditorInspectorSection() {
    object = nullptr;
    foldable = false;
    vbox = memnew(VBoxContainer);
    vbox_added = false;
}

EditorInspectorSection::~EditorInspectorSection() {
    if (!vbox_added) {
        memdelete(vbox);
    }
}

////////////////////////////////////////////////
////////////////////////////////////////////////

FixedVector<Ref<EditorInspectorPlugin>, EditorInspector::MAX_PLUGINS> EditorInspector::inspector_plugins;

EditorProperty *EditorInspector::instantiate_property_editor(Object *p_object, VariantType p_type, StringView p_path,
        PropertyHint p_hint, StringView p_hint_text, int p_usage) {
    for (int i = inspector_plugins.size() - 1; i >= 0; i--) {
        inspector_plugins[i]->parse_property(p_object, p_type, p_path, p_hint, p_hint_text, p_usage);
        if (!inspector_plugins[i]->added_editors.empty()) {
            for (int j = 1; j < inspector_plugins[i]->added_editors.size(); j++) { // only keep first one
                memdelete(inspector_plugins[i]->added_editors[j].property_editor);
            }

            EditorProperty *prop = object_cast<EditorProperty>(inspector_plugins[i]->added_editors[0].property_editor);
            if (prop) {
                inspector_plugins[i]->added_editors.clear();
                return prop;
            }
            memdelete(inspector_plugins[i]->added_editors[0].property_editor);
            inspector_plugins[i]->added_editors.clear();
        }
    }
    return nullptr;
}

void EditorInspector::add_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {
    ERR_FAIL_COND(inspector_plugins.size() == MAX_PLUGINS);
    if (inspector_plugins.contains(p_plugin)) {
        return; // already exists
    }
    inspector_plugins.emplace_back(p_plugin);
}

void EditorInspector::remove_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {
    ERR_FAIL_COND(inspector_plugins.empty());

    auto iter = inspector_plugins.erase_first(p_plugin);
    ERR_FAIL_COND_MSG(inspector_plugins.end()==iter, "Trying to remove nonexistent inspector plugin.");
}

void EditorInspector::cleanup_plugins() {
    inspector_plugins.clear();
}

void EditorInspector::set_undo_redo(UndoRedo *p_undo_redo) {
    undo_redo = p_undo_redo;
}

void EditorInspector::_parse_added_editors(VBoxContainer *current_vbox, const Ref<EditorInspectorPlugin> &ped) {
    for (const EditorInspectorPlugin::AddedEditor &F : ped->added_editors) {
        EditorProperty *ep = object_cast<EditorProperty>(F.property_editor);
        current_vbox->add_child(F.property_editor);

        if (!ep) {
            continue;
        }

        ep->object = object;
        ep->connect("property_changed", callable_mp(this, &ClassName::_property_changed));
        ep->connect("property_keyed", callable_mp(this, &ClassName::_property_keyed));
        ep->connect("property_keyed_with_value", callable_mp(this, &ClassName::_property_keyed_with_value));
        ep->connect("property_checked", callable_mp(this, &ClassName::_property_checked));
        ep->connect("property_pinned", callable_mp(this, &ClassName::_property_pinned));
        ep->connect("selected", callable_mp(this, &ClassName::_property_selected));
        ep->connect("multiple_properties_changed", callable_mp(this, &ClassName::_multiple_properties_changed));
        ep->connect("resource_selected", callable_mp(this, &ClassName::_resource_selected), ObjectNS::CONNECT_QUEUED);
        ep->connect("object_id_selected", callable_mp(this, &ClassName::_object_id_selected), ObjectNS::CONNECT_QUEUED);

        if (!F.properties.empty()) {
            if (F.properties.size() == 1) {
                // since it's one, associate:
                ep->property = StringName(F.properties[0]);
                ep->property_path = property_prefix + F.properties[0];
                ep->property_usage = 0;
            }

            if (!F.label.empty()) {
                ep->set_label(F.label);
            }

            for (int i = 0; i < F.properties.size(); i++) {
                StringName prop(F.properties[i]);

                editor_property_map[prop].push_back(ep);
            }
        }

        ep->set_read_only(read_only);
        ep->update_property();
        ep->_update_pin_flags();
        ep->update_revert_and_pin_status();
    }

    ped->added_editors.clear();
}

bool EditorInspector::_is_property_disabled_by_feature_profile(const StringName &p_property) {
    Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
    if (not profile) {
        return false;
    }

    StringName class_name = object->get_class_name();

    while (class_name != StringName()) {
        if (profile->is_class_property_disabled(class_name, p_property)) {
            return true;
        }
        if (profile->is_class_disabled(class_name)) {
            // won't see properties of a disabled class
            return true;
        }
        class_name = ClassDB::get_parent_class(class_name);
    }

    return false;
}

String EditorInspector::process_doc_hints(const PropertyInfo &pi) {
    StringName classname = object->get_class_name();
    if (!object_class.empty()) {
        classname = object_class;
    }
    StringName propname(property_prefix + pi.name);
    String descr;
    bool found = false;

    auto E = descr_cache.find(classname);
    if (E != descr_cache.end()) {
        auto F = E->second.find(propname);
        if (F != E->second.end()) {
            found = true;
            descr = F->second;
        }
    }
    if (found) {
        return descr;
    }

    DocData *dd = EditorHelp::get_doc_data();
    auto F = dd->class_list.find(classname.asCString());
    while (F != dd->class_list.end() && descr.empty()) {
        for (size_t i = 0; i < F->second.properties.size(); i++) {
            if (F->second.properties[i].name == propname.asCString()) {
                descr = DTR(F->second.properties[i].description);
                break;
            }
        }
        Vector<StringView> slices;
        String::split_ref(slices, propname, '/');
        if (slices.size() == 2 && slices[0].starts_with("custom_")) {
            // Likely a theme property.
            for (const DocContents::ThemeItemDoc &pdoc : F->second.theme_properties) {
                if (pdoc.name == slices[1]) {
                    descr = DTR(pdoc.description);
                    break;
                }
            }
        }
        if (!F->second.inherits.empty()) {
            F = dd->class_list.find(F->second.inherits);
        } else {
            break;
        }
    }
    descr_cache[classname][propname] = descr;

    return descr;
}

bool allCategoryEntriesHidden(Vector<PropertyInfo>::iterator I, const Vector<PropertyInfo> &plist ) {
    auto N = eastl::next(I);
    // walk through following properties
    // if no visible properties in category, skip it.
    for(;N != plist.end(); ++N) {
        if (N->usage & PROPERTY_USAGE_EDITOR)
            return false;
        if (N->usage & PROPERTY_USAGE_CATEGORY) {
            return true;
        }
    }
    return false;
}

void EditorInspector::update_tree() {
    using namespace StringUtils;
    // to update properly if all is refreshed
    StringName current_selected = property_selected;
    int current_focusable = -1;

    if (property_focusable != -1) {
        // check focusable is really focusable
        bool restore_focus = false;
        Control *focused = get_focus_owner();
        if (focused) {
            Node *parent = focused->get_parent();
            while (parent) {
                EditorInspector *inspector = object_cast<EditorInspector>(parent);
                if (inspector) {
                    restore_focus = inspector == this; // may be owned by another inspector
                    break; // exit after the first inspector is found, since there may be nested ones
                }
                parent = parent->get_parent();
            }
        }

        if (restore_focus) {
            current_focusable = property_focusable;
        }
    }

    _clear();

    if (!object) {
        return;
    }

    {
        // MutexGuard guard(game_registry_w_lock);
        auto entity_id = object->get_instance_id();
        for(auto iter : game_object_registry.registry.storage()) {
            if(iter.second.contains(entity_id)) {
                const auto &ti(iter.second.type());
                // auto by_type_id = entt::resolve(ti);
                printf("entt:%.*s\n", (int)ti.name().size(), ti.name().data());
            }
        }
        for(auto [id, storage]: game_object_registry.registry.storage()) {
            if(!storage.contains(entity_id)) {
                continue;
            }
            const auto &ti(storage.type());
            entt::meta_type type = entt::resolve(ti);
            if(type) { // registered ?
                printf("entt:%.*s\n", (int)ti.name().size(), ti.name().data());
                //entt::meta_any comp2 = type.get();
                entt::meta_any comp = type.invoke(entt::hashed_string("get"), {}, entt::forward_as_meta(game_object_registry.registry), entity_id);

                for(entt::meta_data data: type.data()) {
                    printf("entt meta data\n");
                }
            }
        }
    }

    Vector<Ref<EditorInspectorPlugin>> valid_plugins;

    for (int i = inspector_plugins.size() - 1; i >= 0; i--) { // start by last, so lastly added can override newly added
        if (!inspector_plugins[i]->can_handle(object)) {
            continue;
        }
        valid_plugins.push_back(inspector_plugins[i]);
    }

    bool draw_red = false;

    {
        Node *nod = object_cast<Node>(object);
        Node *es = EditorNode::get_singleton()->get_edited_scene();
        if (nod && es != nod && nod->get_owner() != es) {
            draw_red = true;
        }
    }

    //	TreeItem *current_category = NULL;

    String filter(search_box ? search_box->get_text() : "");
    String group;
    String group_base;
    VBoxContainer *category_vbox = nullptr;

    Vector<PropertyInfo> plist;
    object->get_property_list(&plist, true);

    HashMap<String, VBoxContainer *> item_path;
    HashMap<VBoxContainer *, EditorInspectorSection *> section_map;

    item_path[""] = main_vbox;

    Color sscolor = get_theme_color("prop_subsection", "Editor");

    // Order of property handlers

    for (const Ref<EditorInspectorPlugin> &ped : valid_plugins) {
        ped->parse_begin(object);
        _parse_added_editors(main_vbox, ped);
    }

    bool in_script_variables = false;

    for (auto I = plist.begin(); I != plist.end(); ++I) {
        const PropertyInfo &obj_property = *I;

        // make sure the property can be edited

        if (obj_property.usage & PROPERTY_USAGE_GROUP) {
            group = obj_property.name;
            group_base = obj_property.hint_string;

            continue;

        } else if (obj_property.usage & PROPERTY_USAGE_CATEGORY) {
            group.clear();
            group_base.clear();

            if (!show_categories)
                continue;

            if (allCategoryEntriesHidden(I,plist)) {
                continue; // empty, ignore
            }

            EditorInspectorCategory *category = memnew(EditorInspectorCategory);
            main_vbox->add_child(category);
            category_vbox = nullptr; // reset

            in_script_variables = obj_property.name == "Script Variables";

            String type(obj_property.name);
            category->icon = EditorNode::get_singleton()->get_class_icon(StringName(type), "Object");
            category->label = type;

            category->bg_color = get_theme_color("prop_category", "Editor");
            if (use_doc_hints) {
                StringName type2 = obj_property.name;
                if (!class_descr_cache.contains(type2)) {
                    String descr;
                    DocData *dd = EditorHelp::get_doc_data();
                    auto E = dd->class_list.find(type2.asCString());
                    if (E != dd->class_list.end()) {
                        descr = DTR(E->second.brief_description);
                    }
                    class_descr_cache[type2] = descr;
                }

                category->set_tooltip(String(obj_property.name.asCString()) +
                                           "::" + (class_descr_cache[type2].empty() ? "" : class_descr_cache[type2]));
            }

            for (const Ref<EditorInspectorPlugin> &ped : valid_plugins) {
                ped->parse_category(object, obj_property.name);
                _parse_added_editors(main_vbox, ped);
            }

            continue;

        } else if (!(obj_property.usage & PROPERTY_USAGE_EDITOR) || _is_property_disabled_by_feature_profile(obj_property.name))
            continue;

        if (obj_property.name == "script" && (hide_script || object->call_va("_hide_script_from_inspector").as<bool>())) {
            continue;
        }

        String basename(obj_property.name);
        if (!group.empty() && !group_base.empty()) {
            if (begins_with(basename, group_base)) {
                basename = replace_first(basename, group_base, "");
            } else if (begins_with(group_base, basename)) {
                // keep it, this is used pretty often
            } else {
                group.clear(); // no longer using group base, clear
            }
        }

        if (!group.empty()) {
            basename = group + "/" + basename;
        }
        auto last_slash_loc = basename.rfind('/');
        String name = (last_slash_loc != String::npos) ? basename.substr(last_slash_loc + 1) : basename;
        String name_override = name;
        String feature_tag;
        {
            const auto dot = name.find(".");
            if (dot != String::npos) {
                name_override = name.substr(0, dot);
                feature_tag = name.substr(dot);
            }
        }

        // Don't localize properties in Script Variables category.
        EditorPropertyNameStyle name_style = property_name_style;
        if (in_script_variables && name_style == EditorPropertyNameStyle::LOCALIZED) {
            name_style = EditorPropertyNameStyle::CAPITALIZED;
        }
        name = EditorPropertyNameProcessor::process_name(name_override, name_style) + feature_tag;

        String path = basename.left(basename.rfind("/"));

        if (use_filter && !filter.empty()) {
            const String property_path = property_prefix + (path.empty() ? "" : path + "/") + name_override;
            if (!_property_path_matches(property_path, filter, property_name_style)) {
                continue;
            }
        }

        if (category_vbox == nullptr) {
            category_vbox = memnew(VBoxContainer);
            main_vbox->add_child(category_vbox);
        }

        VBoxContainer *current_vbox = main_vbox;

        {
            String acc_path;
            int level = 1;

            for (int i = 0; i < StringUtils::get_slice_count(path, '/'); i++) {
                StringView path_name = StringUtils::get_slice(path, '/', i);
                if (i > 0) {
                    acc_path += '/';
                }
                acc_path += path_name;
                if (!item_path.contains(acc_path)) {
                    EditorInspectorSection *section = memnew(EditorInspectorSection);
                    current_vbox->add_child(section);
                    sections.push_back(section);

                    String label;
                    String tooltip;

                    // Only process group label if this is not the group or subgroup.
                    if ((i == 0 && path_name == group)) {
                        if (property_name_style == EditorPropertyNameStyle::LOCALIZED) {
                            label = TTRGET(path_name);
                            tooltip = path_name;
                        } else {
                            label = path_name;
                            tooltip = TTRGET(path_name);
                        }
                    } else {
                        label = EditorPropertyNameProcessor::process_name(
                                path_name, property_name_style);
                        tooltip = EditorPropertyNameProcessor::process_name(
                                path_name, EditorPropertyNameProcessor::get_tooltip_style(property_name_style));
                    }

                    Color c = sscolor;
                    c.a /= level;
                    section->setup(acc_path, label, object, c, use_folding);
                    section->set_tooltip(tooltip);

                    VBoxContainer *vb = section->get_vbox();
                    item_path[acc_path] = vb;
                    section_map[vb] = section;
                }
                current_vbox = item_path[acc_path];
                level = MIN(level + 1, 4);
            }

            if (current_vbox == main_vbox) {
                // do not add directly to the main vbox, given it has no spacing
                if (category_vbox == nullptr) {
                    category_vbox = memnew(VBoxContainer);
                }
                current_vbox = category_vbox;
            }
        }

        bool checkable = false;
        bool checked = false;
        if (obj_property.usage & PROPERTY_USAGE_CHECKABLE) {
            checkable = true;
            checked = obj_property.usage & PROPERTY_USAGE_CHECKED;
        }

        if (obj_property.usage & PROPERTY_USAGE_RESTART_IF_CHANGED) {
            restart_request_props.insert(obj_property.name);
        }

        String doc_hint;

        if (use_doc_hints) {
            doc_hint = process_doc_hints(obj_property);
        }

        for (const Ref<EditorInspectorPlugin> &ped : valid_plugins) {
            bool exclusive = ped->parse_property(object, obj_property.type, obj_property.name, obj_property.hint, obj_property.hint_string, obj_property.usage);
            // make to a temporary, since plugins may be used again in a sub-inspector
            Vector<EditorInspectorPlugin::AddedEditor> editors = eastl::move(ped->added_editors);
            ped->added_editors = {}; // reinitialize from moved-from state to empty

            for (const EditorInspectorPlugin::AddedEditor &F : editors) {
                EditorProperty *ep = object_cast<EditorProperty>(F.property_editor);

                if (ep) {
                    // set all this before the control gets the ENTER_TREE notification
                    ep->object = object;

                    if (!F.properties.empty()) {
                        if (F.properties.size() == 1) {
                            // since it's one, associate:
                            ep->property = StringName(F.properties[0]);
                            ep->property_path = property_prefix + F.properties[0];
                            ep->property_usage = obj_property.usage;
                            // and set label?
                        }

                        if (!F.label.empty()) {
                            ep->set_label(F.label);
                        } else {
                            // use existing one
                            ep->set_label(name);
                        }
                        for (size_t i = 0; i < F.properties.size(); i++) {
                            StringName prop(F.properties[i]);
                            editor_property_map[prop].push_back(ep);
                        }
                    }
                    ep->set_draw_red(draw_red);
                    ep->set_use_folding(use_folding);
                    ep->set_checkable(checkable);
                    ep->set_checked(checked);
                    ep->set_keying(keying);

                    ep->set_read_only(read_only);
                }

                current_vbox->add_child(F.property_editor);

                if (ep) {
                    ep->connect("property_changed", callable_mp(this, &ClassName::_property_changed));
                    if (obj_property.usage & PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED) {
                        ep->connect("property_changed", callable_mp(this, &ClassName::_property_changed_update_all),
                                ObjectNS::CONNECT_QUEUED);
                    }
                    ep->connect("property_keyed", callable_mp(this, &ClassName::_property_keyed));
                    ep->connect("property_keyed_with_value", callable_mp(this, &ClassName::_property_keyed_with_value));
                    ep->connect("property_checked", callable_mp(this, &ClassName::_property_checked));
                    ep->connect("selected", callable_mp(this, &ClassName::_property_selected));
                    ep->connect(
                            "multiple_properties_changed", callable_mp(this, &ClassName::_multiple_properties_changed));
                    ep->connect("resource_selected", callable_mp(this, &ClassName::_resource_selected),
                            ObjectNS::CONNECT_QUEUED);
                    ep->connect("object_id_selected", callable_mp(this, &ClassName::_object_id_selected),
                            ObjectNS::CONNECT_QUEUED);
                    if (!doc_hint.empty()) {
                        ep->set_tooltip(property_prefix + obj_property.name + "::" + doc_hint);
                    } else {
                        ep->set_tooltip(property_prefix + obj_property.name);
                    }
                    ep->update_property();
                    ep->update_revert_and_pin_status();

                    if (current_selected && ep->property == current_selected) {
                        ep->select(current_focusable);
                    }
                }
            }

            if (exclusive) {
                break;
            }
        }
    }

    for (const Ref<EditorInspectorPlugin> &ped : valid_plugins) {
        ped->parse_end();
        _parse_added_editors(main_vbox, ped);
    }

    // see if this property exists and should be kept
}
void EditorInspector::update_property(const StringName &p_prop) {
    if (!editor_property_map.contains(p_prop))
        return;

    for (EditorProperty *E : editor_property_map[p_prop]) {
        E->update_property();
        E->update_revert_and_pin_status();
    }
}

void EditorInspector::_clear() {
    while (main_vbox->get_child_count()) {
        memdelete(main_vbox->get_child(0));
    }
    property_selected = StringName();
    property_focusable = -1;
    editor_property_map.clear();
    sections.clear();
    pending.clear();
    restart_request_props.clear();
}

void EditorInspector::refresh() {
    if (refresh_countdown > 0 || changing) {
        return;
    }
    refresh_countdown = EditorSettings::get_singleton()->get("docks/property_editor/auto_refresh_interval").as<float>();
}

Object *EditorInspector::get_edited_object() {
    return object;
}

void EditorInspector::edit(Object *p_object) {
    if (object == p_object) {
        return;
    }

    if (object) {
        _clear();
        Object_remove_change_receptor(object, this);
    }

    object = p_object;

    if (object) {
        update_scroll_request = 0; // reset
        if (scroll_cache.contains(object->get_instance_id())) { // if exists, set something else
            update_scroll_request = scroll_cache[object->get_instance_id()]; // done this way because wait until full
                                                                             // size is accommodated
        }
        Object_add_change_receptor(object, this);
        update_tree();
    }
}

void EditorInspector::set_keying(bool p_active) {
    if (keying == p_active)
        return;
    keying = p_active;
    update_tree();
}
void EditorInspector::set_read_only(bool p_read_only) {
    read_only = p_read_only;
    update_tree();
}

EditorPropertyNameStyle EditorInspector::get_property_name_style() const {
    return property_name_style;
}

void EditorInspector::set_property_name_style(EditorPropertyNameStyle p_style) {
    if (property_name_style == p_style) {
        return;
    }
    property_name_style = p_style;
    update_tree();
}

void EditorInspector::set_autoclear(bool p_enable) {
    autoclear = p_enable;
}

void EditorInspector::set_show_categories(bool p_show) {
    show_categories = p_show;
    update_tree();
}

void EditorInspector::set_use_doc_hints(bool p_enable) {
    use_doc_hints = p_enable;
    update_tree();
}
void EditorInspector::set_hide_script(bool p_hide) {
    hide_script = p_hide;
    update_tree();
}
void EditorInspector::set_use_filter(bool p_use) {
    use_filter = p_use;
    update_tree();
}
void EditorInspector::register_text_enter(Node *p_line_edit) {
    search_box = object_cast<LineEdit>(p_line_edit);
    if (search_box)
        search_box->connect("text_changed", callable_mp(this, &ClassName::_filter_changed));
}

void EditorInspector::_filter_changed(StringView /*p_text*/) {
    _clear();
    update_tree();
}

void EditorInspector::set_use_folding(bool p_enable) {
    use_folding = p_enable;
    update_tree();
}

bool EditorInspector::is_using_folding() {
    return use_folding;
}

void EditorInspector::collapse_all_folding() {
    for (EditorInspectorSection *E : sections) {
        E->fold();
    }

    for (eastl::pair<const StringName, Vector<EditorProperty *>> &F : editor_property_map) {
        for (EditorProperty *E : F.second) {
            E->collapse_all_folding();
        }
    }
}

void EditorInspector::expand_all_folding() {
    for (EditorInspectorSection *E : sections) {
        E->unfold();
    }
    for (eastl::pair<const StringName, Vector<EditorProperty *>> &F : editor_property_map) {
        for (EditorProperty *E : F.second) {
            E->expand_all_folding();
        }
    }
}

void EditorInspector::set_scroll_offset(int p_offset) {
    set_v_scroll(p_offset);
}

int EditorInspector::get_scroll_offset() const {
    return get_v_scroll();
}

void EditorInspector::_update_inspector_bg() {
    if (sub_inspector) {
        int count_subinspectors = 0;
        Node *n = get_parent();
        while (n) {
            EditorInspector *ei = object_cast<EditorInspector>(n);
            if (ei && ei->sub_inspector) {
                count_subinspectors++;
            }
            n = n->get_parent();
        }
        count_subinspectors = MIN(15, count_subinspectors);
        add_theme_style_override(
                "bg", get_theme_stylebox(StringName("sub_inspector_bg" + itos(count_subinspectors)), "Editor"));
    } else {
        add_theme_style_override("bg", get_theme_stylebox("bg", "Tree"));
    }
}

void EditorInspector::set_sub_inspector(bool p_enable) {
    sub_inspector = p_enable;
    if (!is_inside_tree())
        return;

    _update_inspector_bg();
}

void EditorInspector::set_property_clipboard(const Variant &p_value) {
    property_clipboard = p_value;
}

Variant EditorInspector::get_property_clipboard() const {
    return property_clipboard;
}

// TODO: pass p_property as StringName
void EditorInspector::_edit_request_change(Object *p_object, StringView p_property) {
    if (object != p_object) // may be undoing/redoing for a non edited object, so ignore
        return;

    if (changing)
        return;

    if (p_property.empty())
        update_tree_pending = true;
    else {
        pending.emplace(p_property);
    }
}

void EditorInspector::_edit_set(
        StringView p_name, const Variant &p_value, bool p_refresh_all, StringView p_changed_field) {
    auto iter = editor_property_map.find_as(p_name, eastl::hash<StringView>(), SNSVComparer());
    if (autoclear && editor_property_map.end() != iter) {
        for (EditorProperty *E : iter->second) {
            if (E->is_checkable()) {
                E->set_checked(true);
            }
        }
    }

    if (!undo_redo || object->call_va("_dont_undo_redo").as<bool>()) {
        object->set(StringName(p_name), p_value);
        if (p_refresh_all)
            _edit_request_change(object, "");
        else
            _edit_request_change(object, StringName(p_name));

        emit_signal(_prop_edited, p_name);

    } else if (object_cast<MultiNodeEdit>(object)) {
        object_cast<MultiNodeEdit>(object)->set_property_field(StringName(p_name), p_value, p_changed_field);
        _edit_request_change(object, p_name);
        emit_signal(_prop_edited, p_name);
    } else {
        undo_redo->create_action(FormatVE(TTR("Set %.*s").asCString(),p_name.size(),p_name.data()), UndoRedo::MERGE_ENDS);
        undo_redo->add_do_property(object, p_name, p_value);
        undo_redo->add_undo_property(object, p_name, object->get(StringName(p_name)));

        if (p_refresh_all) {
            undo_redo->add_do_method(this, "_edit_request_change", Variant(object), "");
            undo_redo->add_undo_method(this, "_edit_request_change", Variant(object), "");
        } else {
            undo_redo->add_do_method(this, "_edit_request_change", Variant(object), p_name);
            undo_redo->add_undo_method(this, "_edit_request_change", Variant(object), p_name);
        }

        Resource *r = object_cast<Resource>(object);
        if (r) {
            if (p_name == StringView("resource_local_to_scene")) {
                bool prev = object->get(StringName(p_name)).as<bool>();
                bool next = p_value.as<bool>();
                if (next) {
                    undo_redo->add_do_method(r, "setup_local_to_scene");
                }
                if (prev) {
                    undo_redo->add_undo_method(r, "setup_local_to_scene");
                }
            }
        }
        undo_redo->add_do_method(this, "emit_signal", _prop_edited, p_name);
        undo_redo->add_undo_method(this, "emit_signal", _prop_edited, p_name);
        undo_redo->commit_action();
    }

    if (editor_property_map.contains(StringName(p_name))) {
        for (EditorProperty *E : editor_property_map[StringName(p_name)]) {
            E->update_revert_and_pin_status();
        }
    }
}

void EditorInspector::_property_changed(StringView p_path, const Variant &p_value, StringView p_name, bool changing) {
    // The "changing" variable must be true for properties that trigger events as typing occurs,
    // like "text_changed" signal. eg: Text property of Label, Button, RichTextLabel, etc.
    if (changing)
        this->changing++;

    _edit_set(p_path, p_value, false, p_name);

    if (changing)
        this->changing--;

    if (restart_request_props.contains(StringName(p_path))) {
        emit_signal("restart_requested");
    }
}

void EditorInspector::_property_changed_update_all(
        StringView /*p_path*/, const Variant & /*p_value*/, StringView /*p_name*/, bool /*p_changing*/) {
    update_tree();
}

void EditorInspector::_multiple_properties_changed(const Vector<String> &p_paths, Array p_values) {
    ERR_FAIL_COND(p_paths.empty() || p_values.empty());
    ERR_FAIL_COND(p_paths.size() != p_values.size());

    String names = String::joined(p_paths, ",");

    undo_redo->create_action(TTR("Set Multiple:") + " " + names, UndoRedo::MERGE_ENDS);
    for (size_t i = 0; i < p_paths.size(); i++) {
        _edit_set(StringName(p_paths[i]), p_values[i], false, StringView());
        if (restart_request_props.contains(StringName(p_paths[i]))) {
            emit_signal("restart_requested");
        }
    }
    changing++;
    undo_redo->commit_action();
    changing--;
}

void EditorInspector::_property_keyed(const StringName &p_path, bool p_advance) {
    if (!object)
        return;

    emit_signal("property_keyed", p_path, object->get(p_path), p_advance); // second param is deprecated
}

void EditorInspector::_property_keyed_with_value(StringView p_path, const Variant &p_value, bool p_advance) {
    if (!object)
        return;

    emit_signal("property_keyed", p_path, p_value, p_advance); // second param is deprecated
}

void EditorInspector::_property_checked(const StringName &p_path, bool p_checked) {
    if (!object) {
        return;
    }

    // property checked
    if (!autoclear) {
        emit_signal("property_toggled", p_path, p_checked);
        return;
    }

    if (!p_checked) {
        object->set(p_path, Variant());
    } else {
        Variant to_create;
        Vector<PropertyInfo> pinfo;
        object->get_property_list(&pinfo);
        for (const PropertyInfo &E : pinfo) {
            if (E.name == p_path) {
                to_create = Variant::construct_default(E.type);
                break;
            }
        }
        object->set(p_path, to_create);
    }

    if (editor_property_map.contains(p_path)) {
        for (EditorProperty *E : editor_property_map[p_path]) {
            E->update_property();
            E->update_revert_and_pin_status();
        }
    }
}

struct PinAction : public UndoableAction {
    Vector<EditorProperty *> m_properties;
    GameEntity m_node_handle;
    StringName m_pinned_path;
    bool pinned;

    StringName name() const override {
        return FormatSN(
                pinned ? TTR("Pinned %s").asCString() : TTR("Unpinned %s").asCString(), m_pinned_path.asCString());
    }

    void redo() override {
        Node *node = object_cast<Node>(object_for_entity(m_node_handle));
        node->set_property_pinned(m_pinned_path, pinned);
        for (EditorProperty *prop : m_properties) {
            prop->update_revert_and_pin_status();
        }
    }

    void undo() override {
        Node *node = object_cast<Node>(object_for_entity(m_node_handle));
        node->set_property_pinned(m_pinned_path, !pinned);
        for (EditorProperty *prop : m_properties) {
            prop->update_revert_and_pin_status();
        }
    }

    bool can_apply() override {
        return object_for_entity(m_node_handle) != nullptr;
    }

    PinAction(Node *ob1, const Vector<EditorProperty *> &src, StringName sn, bool pin) :
        m_properties(src),
        m_node_handle(ob1->get_instance_id()),
        m_pinned_path(eastl::move(sn)),
        pinned(pin) {

    }
};

void EditorInspector::_property_pinned(const StringName &p_path, bool p_pinned) {
    if (!object) {
        return;
    }

    Node *node = object_cast<Node>(object);
    ERR_FAIL_COND(!node);

    if (undo_redo) {
        undo_redo->add_action(new PinAction(node, editor_property_map.at(p_path, {}), p_path, p_pinned)
                );
        undo_redo->commit_action();
    } else {
        node->set_property_pinned(p_path, p_pinned);
        if (editor_property_map.contains(p_path)) {
            for (EditorProperty *E : editor_property_map[p_path]) {
                E->update_revert_and_pin_status();
            }
        }
    }
}

void EditorInspector::_property_selected(const StringName &p_path, int p_focusable) {
    property_selected = p_path;
    property_focusable = p_focusable;
    // deselect the others
    for (eastl::pair<const StringName, Vector<EditorProperty *>> &F : editor_property_map) {
        if (F.first == property_selected)
            continue;
        for (EditorProperty *E : F.second) {
            if (E->is_selected())
                E->deselect();
        }
    }

    emit_signal("property_selected", p_path);
}

void EditorInspector::_object_id_selected(StringView p_path, GameEntity p_id) {
    emit_signal("object_id_selected", Variant::from(p_id));
}

void EditorInspector::_resource_selected(StringView p_path, const RES &p_resource) {
    emit_signal("resource_selected", p_resource, p_path);
}

void EditorInspector::_node_removed(Node *p_node) {
    if (p_node == object) {
        edit(nullptr);
    }
}

void EditorInspector::_notification(int p_what) {
    if (p_what == NOTIFICATION_READY) {
        EditorFeatureProfileManager::get_singleton()->connect(
                "current_feature_profile_changed", callable_mp(this, &ClassName::_feature_profile_changed));
        _update_inspector_bg();
    }

    if (p_what == NOTIFICATION_ENTER_TREE) {
        if (!sub_inspector) {
            get_tree()->connect("node_removed", callable_mp(this, &ClassName::_node_removed));
        }
    }
    if (p_what == NOTIFICATION_PREDELETE) {
        edit(nullptr); // just in case
    }
    if (p_what == NOTIFICATION_EXIT_TREE) {
        if (!sub_inspector) {
            get_tree()->disconnect("node_removed", callable_mp(this, &ClassName::_node_removed));
        }
        edit(nullptr);
    }

    if (p_what == NOTIFICATION_PROCESS) {
        if (update_scroll_request >= 0) {
            auto sb = get_v_scrollbar();
            sb->call_deferred([sb, req = update_scroll_request] { sb->set_value(req); });
            update_scroll_request = -1;
        }
        if (refresh_countdown > 0) {
            refresh_countdown -= get_process_delta_time();
            if (refresh_countdown <= 0) {
                for (eastl::pair<const StringName, Vector<EditorProperty *>> &F : editor_property_map) {
                    for (EditorProperty *E : F.second) {
                        E->update_property();
                        E->update_revert_and_pin_status();
                    }
                }
            }
        }

        changing++;

        if (update_tree_pending) {
            update_tree();
            update_tree_pending = false;
            pending.clear();

        } else {
            while (!pending.empty()) {
                StringName prop = *pending.begin();
                if (editor_property_map.contains(prop)) {
                    for (EditorProperty *E : editor_property_map[prop]) {
                        E->update_property();
                        E->update_revert_and_pin_status();
                    }
                }
                pending.erase(pending.begin());
            }
        }

        changing--;
    }

    if (p_what == EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED) {
        _update_inspector_bg();

        update_tree();
    }
}

void EditorInspector::_changed_callback(Object *p_changed, StringName p_prop) {
    // this is called when property change is notified via _change_notify()
    _edit_request_change(p_changed, p_prop);
}

void EditorInspector::_vscroll_changed(double p_offset) {
    if (update_scroll_request >= 0) // waiting, do nothing
        return;

    if (object) {
        scroll_cache[object->get_instance_id()] = p_offset;
    }
}

void EditorInspector::set_property_prefix(const String &p_prefix) {
    property_prefix = p_prefix;
}

const String &EditorInspector::get_property_prefix() const {
    return property_prefix;
}

void EditorInspector::set_object_class(const StringName &p_class) {
    object_class = p_class;
}

// const StringName &EditorInspector::get_object_class() const {
//     return object_class;
// }

void EditorInspector::_feature_profile_changed() {
    update_tree();
}

void EditorInspector::_bind_methods() {
    MethodBinder::bind_method("_edit_request_change", &EditorInspector::_edit_request_change);
    MethodBinder::bind_method("_resource_selected", &EditorInspector::_resource_selected);

    MethodBinder::bind_method("refresh", &EditorInspector::refresh);

    ADD_SIGNAL(MethodInfo("property_selected", PropertyInfo(VariantType::STRING, "property")));
    ADD_SIGNAL(MethodInfo("property_keyed", PropertyInfo(VariantType::STRING, "property")));
    ADD_SIGNAL(MethodInfo(
            "resource_selected", PropertyInfo(VariantType::OBJECT, "res"), PropertyInfo(VariantType::STRING, "prop")));
    ADD_SIGNAL(MethodInfo("object_id_selected", PropertyInfo(VariantType::INT, "id")));
    ADD_SIGNAL(MethodInfo("property_edited", PropertyInfo(VariantType::STRING, "property")));
    ADD_SIGNAL(MethodInfo("property_toggled", PropertyInfo(VariantType::STRING, "property"),
            PropertyInfo(VariantType::BOOL, "checked")));
    ADD_SIGNAL(MethodInfo("restart_requested"));
}

EditorInspector::EditorInspector() {
    object = nullptr;
    undo_redo = nullptr;
    main_vbox = memnew(VBoxContainer);
    main_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
    main_vbox->add_constant_override("separation", 0);
    add_child(main_vbox);
    set_enable_h_scroll(false);
    set_enable_v_scroll(true);

    hide_script = true;
    use_doc_hints = false;
    property_name_style = EditorPropertyNameStyle::CAPITALIZED;
    use_filter = false;
    autoclear = false;
    changing = 0;
    use_folding = false;
    update_all_pending = false;
    update_tree_pending = false;
    refresh_countdown = 0;
    read_only = false;
    search_box = nullptr;
    keying = false;
    _prop_edited = "property_edited";
    set_process(true);
    property_focusable = -1;
    sub_inspector = false;
    property_clipboard = Variant();

    get_v_scrollbar()->connect("value_changed", callable_mp(this, &ClassName::_vscroll_changed));
    update_scroll_request = -1;

    ED_SHORTCUT("property_editor/copy_property", TTR("Copy Property"), KEY_MASK_CMD | KEY_C);
    ED_SHORTCUT("property_editor/paste_property", TTR("Paste Property"), KEY_MASK_CMD | KEY_V);
    ED_SHORTCUT("property_editor/copy_property_path", TTR("Copy Property Path"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_C);
}
