/*************************************************************************/
/*  editor_inspector.cpp                                                 */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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
#include "editor_node.h"
#include "editor_scale.h"
#include "multi_node_edit.h"
#include "editor_feature_profile.h"
#include "scene/resources/packed_scene.h"
#include "core/method_bind.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/style_box.h"

IMPL_GDCLASS(EditorProperty)
IMPL_GDCLASS(EditorInspectorPlugin)
IMPL_GDCLASS(EditorInspectorCategory)
IMPL_GDCLASS(EditorInspectorSection)
IMPL_GDCLASS(EditorInspector)

Size2 EditorProperty::get_minimum_size() const {

    Size2 ms;
    Ref<Font> font = get_font("font", "Tree");
    ms.height = font->get_height();

    for (int i = 0; i < get_child_count(); i++) {

        Control *c = object_cast<Control>(get_child(i));
        if (!c)
            continue;
        if (c->is_set_as_toplevel())
            continue;
        if (!c->is_visible())
            continue;
        if (c == bottom_editor)
            continue;

        Size2 minsize = c->get_combined_minimum_size();
        ms.width = MAX(ms.width, minsize.width);
        ms.height = MAX(ms.height, minsize.height);
    }

    if (keying) {
        Ref<Texture> key = get_icon("Key", "EditorIcons");
        ms.width += key->get_width() + get_constant("hseparator", "Tree");
    }

    if (checkable) {
        Ref<Texture> check = get_icon("checked", "CheckBox");
        ms.width += check->get_width() + get_constant("hseparation", "CheckBox") + get_constant("hseparator", "Tree");
    }

    if (bottom_editor != nullptr && bottom_editor->is_visible()) {
        ms.height += get_constant("vseparation", "Tree");
        Size2 bems = bottom_editor->get_combined_minimum_size();
        //bems.width += get_constant("item_margin", "Tree");
        ms.height += bems.height;
        ms.width = MAX(ms.width, bems.width);
    }

    return ms;
}

void EditorProperty::emit_changed(const StringName &p_property, const Variant &p_value, const StringName &p_field, bool p_changing) {

    Variant args[4] = { p_property, p_value, p_field, p_changing };
    const Variant *argptrs[4] = { &args[0], &args[1], &args[2], &args[3] };

    emit_signal("property_changed", (const Variant **)argptrs, 4);
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
            Ref<Font> font = get_font("font", "Tree");
            int height = font->get_height();
            bool no_children = true;

            //compute room needed
            for (int i = 0; i < get_child_count(); i++) {

                Control *c = object_cast<Control>(get_child(i));
                if (!c)
                    continue;
                if (c->is_set_as_toplevel())
                    continue;
                if (c == bottom_editor)
                    continue;

                Size2 minsize = c->get_combined_minimum_size();
                child_room = MAX(child_room, minsize.width);
                height = MAX(height, minsize.height);
                no_children = false;
            }

            if (no_children) {
                text_size = size.width;
                rect = Rect2(size.width - 1, 0, 1, height);
            } else {
                text_size = MAX(0, size.width - (child_room + 4 * EDSCALE));
                rect = Rect2(size.width - child_room, 0, child_room, height);
            }

            if (bottom_editor) {

                int m = 0; //get_constant("item_margin", "Tree");

                bottom_rect = Rect2(m, rect.size.height + get_constant("vseparation", "Tree"), size.width - m, bottom_editor->get_combined_minimum_size().height);
            }

            if (keying) {
                Ref<Texture> key;

                if (use_keying_next()) {
                    key = get_icon("KeyNext", "EditorIcons");
                } else {
                    key = get_icon("Key", "EditorIcons");
                }

                rect.size.x -= key->get_width() + get_constant("hseparator", "Tree");

                if (no_children) {
                    text_size -= key->get_width() + 4 * EDSCALE;
                }
            }
        }

        //set children
        for (int i = 0; i < get_child_count(); i++) {

            Control *c = object_cast<Control>(get_child(i));
            if (!c)
                continue;
            if (c->is_set_as_toplevel())
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

        update(); //need to redraw text
    }

    if (p_what == NOTIFICATION_DRAW) {
        Ref<Font> font = get_font("font", "Tree");
        Color dark_color = get_color("dark_color_2", "Editor");

        Size2 size = get_size();
        if (bottom_editor) {
            size.height = bottom_editor->get_margin(MARGIN_TOP);
        } else if (label_reference) {
            size.height = label_reference->get_size().height;
        }

        if (selected) {
            Ref<StyleBox> sb = get_stylebox("selected", "Tree");
            draw_style_box(sb, Rect2(Vector2(), size));
        }

        if (draw_top_bg && right_child_rect != Rect2()) {
            draw_rect(right_child_rect, dark_color);
        }
        if (bottom_child_rect != Rect2()) {
            draw_rect(bottom_child_rect, dark_color);
        }

        Color color;
        if (draw_red) {
            color = get_color("error_color", "Editor");
        } else {
            color = get_color("property_color", "Editor");
        }
        if (StringUtils::find(label,".") != -1) {
            color.a = 0.5; //this should be un-hacked honestly, as it's used for editor overrides
        }

        int ofs = 0;
        int text_limit = text_size;

        if (checkable) {
            Ref<Texture> checkbox;
            if (checked)
                checkbox = get_icon("GuiChecked", "EditorIcons");
            else
                checkbox = get_icon("GuiUnchecked", "EditorIcons");

            Color color2(1, 1, 1);
            if (check_hover) {
                color2.r *= 1.2f;
                color2.g *= 1.2f;
                color2.b *= 1.2f;
            }
            check_rect = Rect2(ofs, ((size.height - checkbox->get_height()) / 2), checkbox->get_width(), checkbox->get_height());
            draw_texture(checkbox, check_rect.position, color2);
            ofs += get_constant("hseparator", "Tree") + checkbox->get_width() + get_constant("hseparation", "CheckBox");
            text_limit -= ofs;
        } else {
            check_rect = Rect2();
        }

        if (can_revert) {
            Ref<Texture> reload_icon = get_icon("ReloadSmall", "EditorIcons");
            text_limit -= reload_icon->get_width() + get_constant("hseparator", "Tree") * 2;
            revert_rect = Rect2(text_limit + get_constant("hseparator", "Tree"), (size.height - reload_icon->get_height()) / 2, reload_icon->get_width(), reload_icon->get_height());

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

        int v_ofs = (size.height - font->get_height()) / 2;
        draw_string(font, Point2(ofs, v_ofs + font->get_ascent()), label, color, text_limit);

        if (keying) {
            Ref<Texture> key;

            if (use_keying_next()) {
                key = get_icon("KeyNext", "EditorIcons");
            } else {
                key = get_icon("Key", "EditorIcons");
            }

            ofs = size.width - key->get_width() - get_constant("hseparator", "Tree");

            Color color2(1, 1, 1);
            if (keying_hover) {
                color2.r *= 1.2f;
                color2.g *= 1.2f;
                color2.b *= 1.2f;
            }
            keying_rect = Rect2(ofs, ((size.height - key->get_height()) / 2), key->get_width(), key->get_height());
            draw_texture(key, keying_rect.position, color2);
        } else {
            keying_rect = Rect2();
        }
    }
}

void EditorProperty::set_label(const String &p_label) {
    label = p_label;
    update();
}

String EditorProperty::get_label() const {
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

bool EditorPropertyRevert::may_node_be_in_instance(Node *p_node) {

    Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();

    bool might_be = false;
    Node *node = p_node;

    while (node) {

        if (node->get_scene_instance_state()) {
            might_be = true;
            break;
        }
        if (node == edited_scene) {
            if (node->get_scene_inherited_state()) {
                might_be = true;
                break;
            }
            might_be = false;
            break;
        }
        node = node->get_owner();
    }

    return might_be; // or might not be
}

bool EditorPropertyRevert::get_instanced_node_original_property(Node *p_node, const StringName &p_prop, Variant &value) {

    Node *node = p_node;
    Node *orig = node;

    Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();

    bool found = false;

    while (node) {

        Ref<SceneState> ss;

        if (node == edited_scene) {
            ss = node->get_scene_inherited_state();

        } else {
            ss = node->get_scene_instance_state();
        }

        if (ss) {

            NodePath np = node->get_path_to(orig);
            int node_idx = ss->find_node_by_path(np);
            if (node_idx >= 0) {
                bool lfound = false;
                Variant lvar;
                lvar = ss->get_property_value(node_idx, p_prop, lfound);
                if (lfound) {

                    found = true;
                    value = lvar;
                }
            }
        }
        if (node == edited_scene) {
            //just in case
            break;
        }
        node = node->get_owner();
    }

    if (!found && node) {
        //if not found, try default class value
        Variant attempt = ClassDB::class_get_default_property_value(node->get_class_name(), p_prop);
        if (attempt.get_type() != VariantType::NIL) {
            found = true;
            value = attempt;
        }
    }

    return found;
}

bool EditorPropertyRevert::is_node_property_different(Node *p_node, const Variant &p_current, const Variant &p_orig) {

    // this is a pretty difficult function, because a property may not be saved but may have
    // the flag to not save if one or if zero

    //make sure there is an actual state
    {
        Node *node = p_node;
        if (!node)
            return false;

        Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
        bool found_state = false;

        while (node) {

            Ref<SceneState> ss;

            if (node == edited_scene) {
                ss = node->get_scene_inherited_state();

            } else {
                ss = node->get_scene_instance_state();
            }

            if (ss) {
                found_state = true;
            }
            if (node == edited_scene) {
                //just in case
                break;
            }
            node = node->get_owner();
        }

        if (!found_state)
            return false; //pointless to check if we are not comparing against anything.
    }

    if (p_current.get_type() == VariantType::REAL && p_orig.get_type() == VariantType::REAL) {
        float a = p_current;
        float b = p_orig;

        return !Math::is_equal_approx(a, b); //this must be done because, as some scenes save as text, there might be a tiny difference in floats due to numerical error
    }

    return bool(Variant::evaluate(Variant::OP_NOT_EQUAL, p_current, p_orig));
}

bool EditorPropertyRevert::can_property_revert(Object *p_object, const StringName &p_property) {

    bool has_revert = false;

    Node *node = object_cast<Node>(p_object);

    if (node && EditorPropertyRevert::may_node_be_in_instance(node)) {
        //check for difference including instantiation
        Variant vorig;
        if (EditorPropertyRevert::get_instanced_node_original_property(node, p_property, vorig)) {
            Variant v = p_object->get(p_property);

            if (EditorPropertyRevert::is_node_property_different(node, v, vorig)) {
                has_revert = true;
            }
        }
    } else {
        //check for difference against default class value instead
        Variant default_value = ClassDB::class_get_default_property_value(p_object->get_class_name(), p_property);
        if (default_value != Variant() && default_value != p_object->get(p_property)) {
            has_revert = true;
        }
    }

    if (p_object->call("property_can_revert", p_property).operator bool()) {

        has_revert = true;
    }

    if (!has_revert && !p_object->get_script().is_null()) {
        Ref<Script> scr(refFromRefPtr<Script>(p_object->get_script()));
        if (scr) {
            Variant orig_value;
            if (scr->get_property_default_value(p_property, orig_value)) {
                if (orig_value != p_object->get(p_property)) {
                    has_revert = true;
                }
            }
        }
    }

    return has_revert;
}

void EditorProperty::update_reload_status() {

    if (property == StringName())
        return; //no property, so nothing to do

    bool has_reload = EditorPropertyRevert::can_property_revert(object, property);

    if (has_reload != can_revert) {
        can_revert = has_reload;
        update();
    }
}

bool EditorProperty::use_keying_next() const {
    ListPOD<PropertyInfo> plist;
    object->get_property_list(&plist, true);

    for (const PropertyInfo &p : plist) {
        if (p.name == property) {
            return (p.usage & PROPERTY_USAGE_KEYING_INCREMENTS);
        }
    }

    return false;
}
void EditorProperty::set_checkable(bool p_checkable) {

    checkable = p_checkable;
    update();
    queue_sort();
}

bool EditorProperty::is_checkable() const {

    return checkable;
}

void EditorProperty::set_checked(bool p_checked) {

    checked = p_checked;
    update();
}

bool EditorProperty::is_checked() const {

    return checked;
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

    p_control->connect("focus_entered", this, "_focusable_focused", varray(focusables.size()));
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

bool EditorProperty::is_selected() const {
    return selected;
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
                call_deferred("emit_changed", property, object->get(property).operator int64_t() + 1, "", false);
                call_deferred("update_property");
            }
        }

        if (revert_rect.has_point(mb->get_position())) {

            Variant vorig;

            Node *node = object_cast<Node>(object);
            if (node && EditorPropertyRevert::may_node_be_in_instance(node) && EditorPropertyRevert::get_instanced_node_original_property(node, property, vorig)) {

                emit_changed(property, vorig.duplicate(true));
                update_property();
                return;
            }

            if (object->call("property_can_revert", property).operator bool()) {
                Variant rev = object->call("property_get_revert", property);
                emit_changed(property, rev);
                update_property();
                return;
            }

            if (!object->get_script().is_null()) {
                Ref<Script> scr(refFromRefPtr<Script>(object->get_script()));
                if (scr) {
                    Variant orig_value;
                    if (scr->get_property_default_value(property, orig_value)) {
                        emit_changed(property, orig_value);
                        update_property();
                        return;
                    }
                }
            }

            Variant default_value = ClassDB::class_get_default_property_value(object->get_class_name(), property);
            if (default_value != Variant()) {
                emit_changed(property, default_value);
                update_property();
                return;
            }
        }
        if (check_rect.has_point(mb->get_position())) {
            checked = !checked;
            update();
            emit_signal("property_checked", property, checked);
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

    if (property == StringName())
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

void EditorProperty::set_use_folding(bool p_use_folding) {

    use_folding = p_use_folding;
}

bool EditorProperty::is_using_folding() const {

    return use_folding;
}

void EditorProperty::expand_all_folding() {
}

void EditorProperty::collapse_all_folding() {
}

void EditorProperty::set_selectable(bool p_selectable) {
    selectable = p_selectable;
}

bool EditorProperty::is_selectable() const {
    return selectable;
}

void EditorProperty::set_name_split_ratio(float p_ratio) {
    split_ratio = p_ratio;
}

float EditorProperty::get_name_split_ratio() const {

    return split_ratio;
}

void EditorProperty::set_object_and_property(Object *p_object, const StringName &p_property) {
    object = p_object;
    property = p_property;
}

Control *EditorProperty::make_custom_tooltip(const String &p_text) const {

    tooltip_text = p_text;
    EditorHelpBit *help_bit = memnew(EditorHelpBit);
    help_bit->add_style_override("panel", get_stylebox("panel", "TooltipPanel"));
    help_bit->get_rich_text()->set_fixed_size_to_width(360 * EDSCALE);

    String text = TTR("Property:") + " [u][b]" + StringUtils::get_slice(p_text,"::", 0) + "[/b][/u]\n";
    text += StringUtils::strip_edges(StringUtils::get_slice(p_text,"::", 1));
    help_bit->set_text(text);
    help_bit->call_deferred("set_text", text); //hack so it uses proper theme once inside scene
    return help_bit;
}

String EditorProperty::get_tooltip_text() const {
    return tooltip_text;
}

void EditorProperty::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_label", {"text"}), &EditorProperty::set_label);
    MethodBinder::bind_method(D_METHOD("get_label"), &EditorProperty::get_label);

    MethodBinder::bind_method(D_METHOD("set_read_only", {"read_only"}), &EditorProperty::set_read_only);
    MethodBinder::bind_method(D_METHOD("is_read_only"), &EditorProperty::is_read_only);

    MethodBinder::bind_method(D_METHOD("set_checkable", {"checkable"}), &EditorProperty::set_checkable);
    MethodBinder::bind_method(D_METHOD("is_checkable"), &EditorProperty::is_checkable);

    MethodBinder::bind_method(D_METHOD("set_checked", {"checked"}), &EditorProperty::set_checked);
    MethodBinder::bind_method(D_METHOD("is_checked"), &EditorProperty::is_checked);

    MethodBinder::bind_method(D_METHOD("set_draw_red", {"draw_red"}), &EditorProperty::set_draw_red);
    MethodBinder::bind_method(D_METHOD("is_draw_red"), &EditorProperty::is_draw_red);

    MethodBinder::bind_method(D_METHOD("set_keying", {"keying"}), &EditorProperty::set_keying);
    MethodBinder::bind_method(D_METHOD("is_keying"), &EditorProperty::is_keying);

    MethodBinder::bind_method(D_METHOD("get_edited_property"), &EditorProperty::get_edited_property);
    MethodBinder::bind_method(D_METHOD("get_edited_object"), &EditorProperty::get_edited_object);

    MethodBinder::bind_method(D_METHOD("_gui_input"), &EditorProperty::_gui_input);
    MethodBinder::bind_method(D_METHOD("_focusable_focused"), &EditorProperty::_focusable_focused);

    MethodBinder::bind_method(D_METHOD("get_tooltip_text"), &EditorProperty::get_tooltip_text);

    MethodBinder::bind_method(D_METHOD("add_focusable", {"control"}), &EditorProperty::add_focusable);
    MethodBinder::bind_method(D_METHOD("set_bottom_editor", {"editor"}), &EditorProperty::set_bottom_editor);

    MethodBinder::bind_method(D_METHOD("emit_changed", {"property", "value", "field", "changing"}), &EditorProperty::emit_changed, {DEFVAL(StringName()), DEFVAL(false)});

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "label"), "set_label", "get_label");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "read_only"), "set_read_only", "is_read_only");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "checkable"), "set_checkable", "is_checkable");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "checked"), "set_checked", "is_checked");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "draw_red"), "set_draw_red", "is_draw_red");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "keying"), "set_keying", "is_keying");
    ADD_SIGNAL(MethodInfo("property_changed", PropertyInfo(VariantType::STRING, "property"), PropertyInfo(VariantType::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
    ADD_SIGNAL(MethodInfo("multiple_properties_changed", PropertyInfo(VariantType::POOL_STRING_ARRAY, "properties"), PropertyInfo(VariantType::ARRAY, "value")));
    ADD_SIGNAL(MethodInfo("property_keyed", PropertyInfo(VariantType::STRING, "property")));
    ADD_SIGNAL(MethodInfo("property_keyed_with_value", PropertyInfo(VariantType::STRING, "property"), PropertyInfo(VariantType::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
    ADD_SIGNAL(MethodInfo("property_checked", PropertyInfo(VariantType::STRING, "property"), PropertyInfo(VariantType::STRING, "bool")));
    ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(VariantType::STRING, "path"), PropertyInfo(VariantType::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource")));
    ADD_SIGNAL(MethodInfo("object_id_selected", PropertyInfo(VariantType::STRING, "property"), PropertyInfo(VariantType::INT, "id")));
    ADD_SIGNAL(MethodInfo("selected", PropertyInfo(VariantType::STRING, "path"), PropertyInfo(VariantType::INT, "focusable_idx")));

    MethodInfo vm;
    vm.name = "update_property";
    BIND_VMETHOD(vm);
}

EditorProperty::EditorProperty() {

    draw_top_bg = true;
    object = nullptr;
    split_ratio = 0.5;
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
    use_folding = false;
    property_usage = 0;
    selected = false;
    selected_focusable = -1;
    label_reference = nullptr;
    bottom_editor = nullptr;
}
////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorPlugin::add_custom_control(Control *control) {

    AddedEditor ae;
    ae.property_editor = control;
    added_editors.push_back(ae);
}

void EditorInspectorPlugin::add_property_editor(const String &p_for_property, Control *p_prop) {

    ERR_FAIL_COND(object_cast<EditorProperty>(p_prop) == nullptr)

    AddedEditor ae;
    ae.properties.push_back(p_for_property);
    ae.property_editor = p_prop;
    added_editors.push_back(ae);
}

void EditorInspectorPlugin::add_property_editor_for_multiple_properties(const String &p_label, const Vector<String> &p_properties, Control *p_prop) {

    AddedEditor ae;
    ae.properties = p_properties;
    ae.property_editor = p_prop;
    ae.label = p_label;
    added_editors.push_back(ae);
}

bool EditorInspectorPlugin::can_handle(Object *p_object) {

    if (get_script_instance()) {
        return get_script_instance()->call("can_handle", Variant(p_object));
    }
    return false;
}
void EditorInspectorPlugin::parse_begin(Object *p_object) {

    if (get_script_instance()) {
        get_script_instance()->call("parse_begin", Variant(p_object));
    }
}

void EditorInspectorPlugin::parse_category(Object *p_object, const String &p_parse_category) {

    if (get_script_instance()) {
        get_script_instance()->call("parse_category", Variant(p_object), p_parse_category);
    }
}

bool EditorInspectorPlugin::parse_property(Object *p_object, VariantType p_type, const String &p_path, PropertyHint p_hint, const String &p_hint_text, int p_usage) {

    if (get_script_instance()) {
        Variant arg[6] = {
            Variant(p_object), p_type, p_path, p_hint, p_hint_text, p_usage
        };
        const Variant *argptr[6] = {
            &arg[0], &arg[1], &arg[2], &arg[3], &arg[4], &arg[5]
        };

        Variant::CallError err;
        return get_script_instance()->call("parse_property", (const Variant **)&argptr, 6, err);
    }
    return false;
}
void EditorInspectorPlugin::parse_end() {

    if (get_script_instance()) {
        get_script_instance()->call("parse_end");
    }
}

void EditorInspectorPlugin::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("add_custom_control", {"control"}), &EditorInspectorPlugin::add_custom_control);
    MethodBinder::bind_method(D_METHOD("add_property_editor", {"property", "editor"}), &EditorInspectorPlugin::add_property_editor);
    MethodBinder::bind_method(D_METHOD("add_property_editor_for_multiple_properties", {"label", "properties", "editor"}), &EditorInspectorPlugin::add_property_editor_for_multiple_properties);

    MethodInfo vm;
    vm.name = "can_handle";
    vm.return_val.type = VariantType::BOOL;
    vm.arguments.push_back(PropertyInfo(VariantType::OBJECT, "object"));
    BIND_VMETHOD(vm);
    vm.name = "parse_begin";
    vm.return_val.type = VariantType::NIL;
    BIND_VMETHOD(vm);
    vm.name = "parse_category";
    vm.arguments.push_back(PropertyInfo(VariantType::STRING, "category"));
    BIND_VMETHOD(vm);
    vm.arguments.pop_back();
    vm.name = "parse_property";
    vm.return_val.type = VariantType::BOOL;
    vm.arguments.push_back(PropertyInfo(VariantType::INT, "type"));
    vm.arguments.push_back(PropertyInfo(VariantType::STRING, "path"));
    vm.arguments.push_back(PropertyInfo(VariantType::INT, "hint"));
    vm.arguments.push_back(PropertyInfo(VariantType::STRING, "hint_text"));
    vm.arguments.push_back(PropertyInfo(VariantType::INT, "usage"));
    BIND_VMETHOD(vm);
    vm.arguments.clear();
    vm.name = "parse_end";
    vm.return_val.type = VariantType::NIL;
    BIND_VMETHOD(vm);
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorCategory::_notification(int p_what) {

    if (p_what == NOTIFICATION_DRAW) {

        draw_rect(Rect2(Vector2(), get_size()), bg_color);
        Ref<Font> font = get_font("font", "Tree");

        int hs = get_constant("hseparation", "Tree");

        int w = font->get_string_size(label).width;
        if (icon) {
            w += hs + icon->get_width();
        }

        int ofs = (get_size().width - w) / 2;

        if (icon) {
            draw_texture(icon, Point2(ofs, (get_size().height - icon->get_height()) / 2).floor());
            ofs += hs + icon->get_width();
        }

        Color color = get_color("font_color", "Tree");
        draw_string(font, Point2(ofs, font->get_ascent() + (get_size().height - font->get_height()) / 2).floor(), label, color, get_size().width);
    }
}

Control *EditorInspectorCategory::make_custom_tooltip(const String &p_text) const {

    tooltip_text = p_text;
    EditorHelpBit *help_bit = memnew(EditorHelpBit);
    help_bit->add_style_override("panel", get_stylebox("panel", "TooltipPanel"));
    help_bit->get_rich_text()->set_fixed_size_to_width(360 * EDSCALE);

    String text = "[u][b]" + StringUtils::get_slice(p_text,"::", 0) + "[/b][/u]\n";
    text += StringUtils::strip_edges(StringUtils::get_slice(p_text,"::", 1));
    help_bit->set_text(text);
    help_bit->call_deferred("set_text", text); //hack so it uses proper theme once inside scene
    return help_bit;
}

Size2 EditorInspectorCategory::get_minimum_size() const {

    Ref<Font> font = get_font("font", "Tree");

    Size2 ms;
    ms.width = 1;
    ms.height = font->get_height();
    if (icon) {
        ms.height = MAX(icon->get_height(), ms.height);
    }
    ms.height += get_constant("vseparation", "Tree");

    return ms;
}

void EditorInspectorCategory::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("get_tooltip_text"), &EditorInspectorCategory::get_tooltip_text);
}

String EditorInspectorCategory::get_tooltip_text() const {

    return tooltip_text;
}

EditorInspectorCategory::EditorInspectorCategory() {
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorSection::_test_unfold() {

    if (!vbox_added) {
        add_child(vbox);
        vbox_added = true;
    }
}

void EditorInspectorSection::_notification(int p_what) {

    if (p_what == NOTIFICATION_SORT_CHILDREN) {

        Ref<Font> font = get_font("font", "Tree");
        Ref<Texture> arrow;

        if (foldable) {
            bool unfolded=object->get_tooling_interface()->editor_is_section_unfolded(section);
            arrow = get_icon(unfolded ? StringName("arrow") : StringName("arrow_collapsed"), "Tree");
        }

        Size2 size = get_size();
        Point2 offset;
        offset.y = font->get_height();
        if (arrow) {
            offset.y = MAX(offset.y, arrow->get_height());
        }

        offset.y += get_constant("vseparation", "Tree");
        offset.x += get_constant("inspector_margin", "Editor");

        Rect2 rect(offset, size - offset);

        //set children
        for (int i = 0; i < get_child_count(); i++) {

            Control *c = object_cast<Control>(get_child(i));
            if (!c)
                continue;
            if (c->is_set_as_toplevel())
                continue;
            if (!c->is_visible_in_tree())
                continue;

            fit_child_in_rect(c, rect);
        }

        update(); //need to redraw text
    }

    if (p_what == NOTIFICATION_DRAW) {

        Ref<Texture> arrow;

        if (foldable) {
            bool unfolded=object->get_tooling_interface()->editor_is_section_unfolded(section);
            arrow = get_icon(unfolded ? StringName("arrow") : StringName("arrow_collapsed"), "Tree");
        }

        Ref<Font> font = get_font("font", "Tree");

        int h = font->get_height();
        if (arrow) {
            h = MAX(h, arrow->get_height());
        }
        h += get_constant("vseparation", "Tree");

        draw_rect(Rect2(Vector2(), Vector2(get_size().width, h)), bg_color);

        const int arrow_margin = 3;

        Color color = get_color("font_color", "Tree");
        draw_string(font, Point2(Math::round((16 + arrow_margin) * EDSCALE), font->get_ascent() + (h - font->get_height()) / 2).floor(), label, color, get_size().width);

        if (arrow) {
            draw_texture(arrow, Point2(Math::round(arrow_margin * EDSCALE), (h - arrow->get_height()) / 2).floor());
        }
    }
}

Size2 EditorInspectorSection::get_minimum_size() const {

    Size2 ms;
    for (int i = 0; i < get_child_count(); i++) {

        Control *c = object_cast<Control>(get_child(i));
        if (!c)
            continue;
        if (c->is_set_as_toplevel())
            continue;
        if (!c->is_visible())
            continue;
        Size2 minsize = c->get_combined_minimum_size();
        ms.width = MAX(ms.width, minsize.width);
        ms.height = MAX(ms.height, minsize.height);
    }

    Ref<Font> font = get_font("font", "Tree");
    ms.height += font->get_height() + get_constant("vseparation", "Tree");
    ms.width += get_constant("inspector_margin", "Editor");

    return ms;
}

void EditorInspectorSection::setup(const String &p_section, const String &p_label, Object *p_object, const Color &p_bg_color, bool p_foldable) {

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
    if (mb && mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {

        Ref<Font> font = get_font("font", "Tree");
        if (mb->get_position().y > font->get_height()) { //clicked outside
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
        return; //kinda pointless

    object->get_tooling_interface()->editor_set_section_unfold(section, false);
    vbox->hide();
    update();
}

void EditorInspectorSection::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("setup", {"section", "label", "object", "bg_color", "foldable"}), &EditorInspectorSection::setup);
    MethodBinder::bind_method(D_METHOD("get_vbox"), &EditorInspectorSection::get_vbox);
    MethodBinder::bind_method(D_METHOD("unfold"), &EditorInspectorSection::unfold);
    MethodBinder::bind_method(D_METHOD("fold"), &EditorInspectorSection::fold);
    MethodBinder::bind_method(D_METHOD("_gui_input"), &EditorInspectorSection::_gui_input);
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

Ref<EditorInspectorPlugin> EditorInspector::inspector_plugins[MAX_PLUGINS];
int EditorInspector::inspector_plugin_count = 0;

EditorProperty *EditorInspector::instantiate_property_editor(Object *p_object, VariantType p_type, const String &p_path, PropertyHint p_hint, const String &p_hint_text, int p_usage) {

    for (int i = inspector_plugin_count - 1; i >= 0; i--) {

        inspector_plugins[i]->parse_property(p_object, p_type, p_path, p_hint, p_hint_text, p_usage);
        if (!inspector_plugins[i]->added_editors.empty()) {
            for (int j = 1; j < inspector_plugins[i]->added_editors.size(); j++) { //only keep first one
                memdelete(inspector_plugins[i]->added_editors[j].property_editor);
            }

            EditorProperty *prop = object_cast<EditorProperty>(inspector_plugins[i]->added_editors[0].property_editor);
            if (prop) {

                inspector_plugins[i]->added_editors.clear();
                return prop;
            } else {
                memdelete(inspector_plugins[i]->added_editors[0].property_editor);
                inspector_plugins[i]->added_editors.clear();
            }
        }
    }
    return nullptr;
}

void EditorInspector::add_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {

    ERR_FAIL_COND(inspector_plugin_count == MAX_PLUGINS)

    for (int i = 0; i < inspector_plugin_count; i++) {
        if (inspector_plugins[i] == p_plugin)
            return; //already exists
    }
    inspector_plugins[inspector_plugin_count++] = p_plugin;
}

void EditorInspector::remove_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {

    ERR_FAIL_COND(inspector_plugin_count == MAX_PLUGINS)

    int idx = -1;
    for (int i = 0; i < inspector_plugin_count; i++) {
        if (inspector_plugins[i] == p_plugin) {
            idx = i;
            break;
        }
    }

    ERR_FAIL_COND_CMSG(idx == -1, "Trying to remove nonexistent inspector plugin.")
    for (int i = idx; i < inspector_plugin_count - 1; i++) {
        inspector_plugins[i] = inspector_plugins[i + 1];
    }

    if (idx == inspector_plugin_count - 1)
        inspector_plugins[idx] = Ref<EditorInspectorPlugin>();

    inspector_plugin_count--;
}

void EditorInspector::cleanup_plugins() {
    for (int i = 0; i < inspector_plugin_count; i++) {
        inspector_plugins[i].unref();
    }
    inspector_plugin_count = 0;
}

void EditorInspector::set_undo_redo(UndoRedo *p_undo_redo) {
    undo_redo = p_undo_redo;
}

String EditorInspector::get_selected_path() const {

    return property_selected;
}

void EditorInspector::_parse_added_editors(VBoxContainer *current_vbox, const Ref<EditorInspectorPlugin>& ped) {

    for (List<EditorInspectorPlugin::AddedEditor>::Element *F = ped->added_editors.front(); F; F = F->next()) {

        EditorProperty *ep = object_cast<EditorProperty>(F->deref().property_editor);
        current_vbox->add_child(F->deref().property_editor);

        if (ep) {

            ep->object = object;
            ep->connect("property_changed", this, "_property_changed");
            ep->connect("property_keyed", this, "_property_keyed");
            ep->connect("property_keyed_with_value", this, "_property_keyed_with_value");
            ep->connect("property_checked", this, "_property_checked");
            ep->connect("selected", this, "_property_selected");
            ep->connect("multiple_properties_changed", this, "_multiple_properties_changed");
            ep->connect("resource_selected", this, "_resource_selected", varray(), ObjectNS::CONNECT_DEFERRED);
            ep->connect("object_id_selected", this, "_object_id_selected", varray(), ObjectNS::CONNECT_DEFERRED);

            if (!F->deref().properties.empty()) {

                if (F->deref().properties.size() == 1) {
                    //since it's one, associate:
                    ep->property = F->deref().properties[0];
                    ep->property_usage = 0;
                }

                if (!F->deref().label.empty()) {
                    ep->set_label(F->deref().label);
                }

                for (int i = 0; i < F->deref().properties.size(); i++) {
                    String prop = F->deref().properties[i];

                    if (!editor_property_map.contains(prop)) {
                        editor_property_map[prop] = List<EditorProperty *>();
                    }
                    editor_property_map[prop].push_back(ep);
                }
            }

            ep->set_read_only(read_only);
            ep->update_property();
            ep->update_reload_status();
        }
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
            //won't see properties of a disabled class
            return true;
        }
        class_name = ClassDB::get_parent_class(class_name);
    }

    return false;
}

void EditorInspector::update_tree() {

    //to update properly if all is refreshed
    StringName current_selected = property_selected;
    int current_focusable = -1;

    if (property_focusable != -1) {
        //check focusable is really focusable
        bool restore_focus = false;
        Control *focused = get_focus_owner();
        if (focused) {
            Node *parent = focused->get_parent();
            while (parent) {
                EditorInspector *inspector = object_cast<EditorInspector>(parent);
                if (inspector) {
                    restore_focus = inspector == this; //may be owned by another inspector
                    break; //exit after the first inspector is found, since there may be nested ones
                }
                parent = parent->get_parent();
            }
        }

        if (restore_focus) {
            current_focusable = property_focusable;
        }
    }

    _clear();

    if (!object)
        return;

    List<Ref<EditorInspectorPlugin> > valid_plugins;

    for (int i = inspector_plugin_count - 1; i >= 0; i--) { //start by last, so lastly added can override newly added
        if (!inspector_plugins[i]->can_handle(object))
            continue;
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

    String filter = search_box ? search_box->get_text() : "";
    String group;
    String group_base;
    VBoxContainer *category_vbox = nullptr;

    ListPOD<PropertyInfo> plist;
    object->get_property_list(&plist, true);

    HashMap<String, VBoxContainer *> item_path;
    Map<VBoxContainer *, EditorInspectorSection *> section_map;

    item_path[""] = main_vbox;

    Color sscolor = get_color("prop_subsection", "Editor");

    for (List<Ref<EditorInspectorPlugin> >::Element *E = valid_plugins.front(); E; E = E->next()) {
        Ref<EditorInspectorPlugin> ped = E->deref();
        ped->parse_begin(object);
        _parse_added_editors(main_vbox, ped);
    }

    for (auto I = plist.begin(); I!=plist.end(); ++I) {

        const PropertyInfo &p = *I;

        //make sure the property can be edited

        if (p.usage & PROPERTY_USAGE_GROUP) {

            group = p.name;
            group_base = p.hint_string;

            continue;

        } else if (p.usage & PROPERTY_USAGE_CATEGORY) {

            group = "";
            group_base = "";

            if (!show_categories)
                continue;

            auto N = eastl::next(I);
            bool valid = true;
            //if no properties in category, skip
            while (N!=plist.end()) {
                if (N->usage & PROPERTY_USAGE_EDITOR)
                    break;
                if (N->usage & PROPERTY_USAGE_CATEGORY) {
                    valid = false;
                    break;
                }
                ++N;
            }
            if (!valid)
                continue; //empty, ignore

            EditorInspectorCategory *category = memnew(EditorInspectorCategory);
            main_vbox->add_child(category);
            category_vbox = nullptr; //reset

            String type = p.name;
            category->icon = EditorNode::get_singleton()->get_class_icon(type, "Object");
            category->label = type;

            category->bg_color = get_color("prop_category", "Editor");
            if (use_doc_hints) {
                StringName type2 = p.name;
                if (!class_descr_cache.contains(type2)) {

                    String descr;
                    DocData *dd = EditorHelp::get_doc_data();
                    Map<String, DocData::ClassDoc>::iterator E = dd->class_list.find(type2);
                    if (E!=dd->class_list.end()) {
                        descr = E->second.brief_description;
                    }
                    class_descr_cache[type2] = descr;
                }

                category->set_tooltip(p.name + "::" + (class_descr_cache[type2].empty() ? "" : class_descr_cache[type2]));
            }

            for (List<Ref<EditorInspectorPlugin> >::Element *E = valid_plugins.front(); E; E = E->next()) {
                Ref<EditorInspectorPlugin> ped = E->deref();
                ped->parse_category(object, p.name);
                _parse_added_editors(main_vbox, ped);
            }

            continue;

        } else if (!(p.usage & PROPERTY_USAGE_EDITOR) || _is_property_disabled_by_feature_profile(p.name))
            continue;

        if (p.usage & PROPERTY_USAGE_HIGH_END_GFX && VisualServer::get_singleton()->is_low_end())
            continue; //do not show this property in low end gfx

        if (p.name == "script" && (hide_script || bool(object->call("_hide_script_from_inspector")))) {
            continue;
        }

        String basename = p.name;
        if (!group.empty()) {
            if (!group_base.empty()) {
                if (StringUtils::begins_with(basename,group_base)) {
                    basename = StringUtils::replace_first(basename,group_base, "");
                } else if (StringUtils::begins_with(group_base,basename)) {
                    //keep it, this is used pretty often
                } else {
                    group = ""; //no longer using group base, clear
                }
            }
        }

        if (!group.empty()) {
            basename = group + "/" + basename;
        }

        String name = (StringUtils::find(basename,"/") != -1) ? StringUtils::right(basename,StringUtils::find_last(basename,"/") + 1) : basename;

        if (capitalize_paths) {
            int dot = StringUtils::find(name,".");
            if (dot != -1) {
                String ov = StringUtils::right(name,dot);
                name = StringUtils::substr(name,0, dot);
                name = StringUtils::capitalize(name);
                name += ov;

            } else {
                name = StringUtils::capitalize(name);
            }
        }

        String path = StringUtils::left(basename,StringUtils::find_last(basename,"/"));

        if (use_filter && !filter.empty()) {

            String cat = path;

            if (capitalize_paths)
                cat = StringUtils::capitalize(cat);

            if (!StringUtils::is_subsequence_of(filter,cat,StringUtils::CaseInsensitive) && !StringUtils::is_subsequence_of(filter,name))
                continue;
        }

        if (category_vbox == nullptr) {
            category_vbox = memnew(VBoxContainer);
            main_vbox->add_child(category_vbox);
        }

        VBoxContainer *current_vbox = main_vbox;

        {

            String acc_path = "";
            int level = 1;
            for (int i = 0; i < StringUtils::get_slice_count(path,"/"); i++) {
                String path_name = StringUtils::get_slice(path,"/", i);
                if (i > 0)
                    acc_path += "/";
                acc_path += path_name;
                if (!item_path.contains(acc_path)) {
                    EditorInspectorSection *section = memnew(EditorInspectorSection);
                    current_vbox->add_child(section);
                    sections.push_back(section);

                    if (capitalize_paths)
                        path_name = StringUtils::capitalize(path_name);

                    Color c = sscolor;
                    c.a /= level;
                    section->setup(acc_path, path_name, object, c, use_folding);

                    VBoxContainer *vb = section->get_vbox();
                    item_path[acc_path] = vb;
                    section_map[vb] = section;
                }
                current_vbox = item_path[acc_path];
                level = (MIN(level + 1, 4));
            }

            if (current_vbox == main_vbox) {
                //do not add directly to the main vbox, given it has no spacing
                if (category_vbox == nullptr) {
                    category_vbox = memnew(VBoxContainer);
                }
                current_vbox = category_vbox;
            }
        }

        bool checkable = false;
        bool checked = false;
        if (p.usage & PROPERTY_USAGE_CHECKABLE) {
            checkable = true;
            checked = p.usage & PROPERTY_USAGE_CHECKED;
        }

        if (p.usage & PROPERTY_USAGE_RESTART_IF_CHANGED) {
            restart_request_props.insert(p.name);
        }

        String doc_hint;

        if (use_doc_hints) {

            StringName classname = object->get_class_name();
            if ( !object_class.empty() ) {
                classname = object_class;
            }
            StringName propname = property_prefix + p.name;
            String descr;
            bool found = false;

            Map<StringName, Map<StringName, String> >::iterator E = descr_cache.find(classname);
            if (E!=descr_cache.end()) {
                Map<StringName, String>::iterator F = E->second.find(propname);
                if (F!=E->second.end()) {
                    found = true;
                    descr = F->second;
                }
            }

            if (!found) {
                DocData *dd = EditorHelp::get_doc_data();
                Map<String, DocData::ClassDoc>::iterator F = dd->class_list.find(classname);
                while (F!=dd->class_list.end() && descr.empty()) {
                    for (int i = 0; i < F->second.properties.size(); i++) {
                        if (F->second.properties[i].name == propname.operator String()) {
                            descr = StringUtils::strip_edges(F->second.properties[i].description);
                            break;
                        }
                    }
                    if (!F->second.inherits.empty()) {
                        F = dd->class_list.find(F->second.inherits);
                    } else {
                        break;
                    }
                }
                descr_cache[classname][propname] = descr;
            }

            doc_hint = descr;
        }

        for (List<Ref<EditorInspectorPlugin> >::Element *E = valid_plugins.front(); E; E = E->next()) {
            Ref<EditorInspectorPlugin> ped = E->deref();
            bool exclusive = ped->parse_property(object, p.type, p.name, p.hint, p.hint_string, p.usage);

            List<EditorInspectorPlugin::AddedEditor> editors = ped->added_editors; //make a copy, since plugins may be used again in a sub-inspector
            ped->added_editors.clear();

            for (List<EditorInspectorPlugin::AddedEditor>::Element *F = editors.front(); F; F = F->next()) {

                EditorProperty *ep = object_cast<EditorProperty>(F->deref().property_editor);

                if (ep) {
                    //set all this before the control gets the ENTER_TREE notification
                    ep->object = object;

                    if (!F->deref().properties.empty()) {

                        if (F->deref().properties.size() == 1) {
                            //since it's one, associate:
                            ep->property = F->deref().properties[0];
                            ep->property_usage = p.usage;
                            //and set label?
                        }

                        if (!F->deref().label.empty()) {
                            ep->set_label(F->deref().label);
                        } else {
                            //use existin one
                            ep->set_label(name);
                        }
                        for (int i = 0; i < F->deref().properties.size(); i++) {
                            String prop = F->deref().properties[i];

                            if (!editor_property_map.contains(prop)) {
                                editor_property_map[prop] = List<EditorProperty *>();
                            }
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

                current_vbox->add_child(F->deref().property_editor);

                if (ep) {

                    ep->connect("property_changed", this, "_property_changed");
                    if (p.usage & PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED) {
                        ep->connect("property_changed", this, "_property_changed_update_all", varray(), ObjectNS::CONNECT_DEFERRED);
                    }
                    ep->connect("property_keyed", this, "_property_keyed");
                    ep->connect("property_keyed_with_value", this, "_property_keyed_with_value");
                    ep->connect("property_checked", this, "_property_checked");
                    ep->connect("selected", this, "_property_selected");
                    ep->connect("multiple_properties_changed", this, "_multiple_properties_changed");
                    ep->connect("resource_selected", this, "_resource_selected", varray(), ObjectNS::CONNECT_DEFERRED);
                    ep->connect("object_id_selected", this, "_object_id_selected", varray(), ObjectNS::CONNECT_DEFERRED);
                    if (!doc_hint.empty()) {
                        ep->set_tooltip(property_prefix + p.name + "::" + doc_hint);
                    } else {
                        ep->set_tooltip(property_prefix + p.name);
                    }
                    ep->update_property();
                    ep->update_reload_status();

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

    for (List<Ref<EditorInspectorPlugin> >::Element *E = valid_plugins.front(); E; E = E->next()) {
        Ref<EditorInspectorPlugin> ped = E->deref();
        ped->parse_end();
        _parse_added_editors(main_vbox, ped);
    }

    //see if this property exists and should be kept
}
void EditorInspector::update_property(const String &p_prop) {
    if (!editor_property_map.contains(p_prop))
        return;

    for (List<EditorProperty *>::Element *E = editor_property_map[p_prop].front(); E; E = E->next()) {
        E->deref()->update_property();
        E->deref()->update_reload_status();
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

    if (refresh_countdown > 0 || changing)
        return;
    refresh_countdown = EditorSettings::get_singleton()->get("docks/property_editor/auto_refresh_interval");
}

Object *EditorInspector::get_edited_object() {
    return object;
}

void EditorInspector::edit(Object *p_object) {
    if (object == p_object)
        return;
    if (object) {

        _clear();
        object->remove_change_receptor(this);
    }

    object = p_object;

    if (object) {
        update_scroll_request = 0; //reset
        if (scroll_cache.contains(object->get_instance_id())) { //if exists, set something else
            update_scroll_request = scroll_cache[object->get_instance_id()]; //done this way because wait until full size is accommodated
        }
        object->add_change_receptor(this);
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

bool EditorInspector::is_capitalize_paths_enabled() const {

    return capitalize_paths;
}
void EditorInspector::set_enable_capitalize_paths(bool p_capitalize) {
    capitalize_paths = p_capitalize;
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
        search_box->connect("text_changed", this, "_filter_changed");
}

void EditorInspector::_filter_changed(const String &p_text) {

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

    for (List<EditorInspectorSection *>::Element *E = sections.front(); E; E = E->next()) {
        E->deref()->fold();
    }

    for (eastl::pair<const StringName,List<EditorProperty *> > &F : editor_property_map) {
        for (List<EditorProperty *>::Element *E = F.second.front(); E; E = E->next()) {
            E->deref()->collapse_all_folding();
        }
    }
}

void EditorInspector::expand_all_folding() {
    for (List<EditorInspectorSection *>::Element *E = sections.front(); E; E = E->next()) {
        E->deref()->unfold();
    }
    for (eastl::pair<const StringName,List<EditorProperty *> > &F : editor_property_map) {
        for (List<EditorProperty *>::Element *E = F.second.front(); E; E = E->next()) {
            E->deref()->expand_all_folding();
        }
    }
}

void EditorInspector::set_scroll_offset(int p_offset) {
    set_v_scroll(p_offset);
}

int EditorInspector::get_scroll_offset() const {
    return get_v_scroll();
}

void EditorInspector::set_sub_inspector(bool p_enable) {

    sub_inspector = p_enable;
    if (!is_inside_tree())
        return;

    if (sub_inspector) {
        add_style_override("bg", get_stylebox("sub_inspector_bg", "Editor"));
    } else {
        add_style_override("bg", get_stylebox("bg", "Tree"));
    }
}

void EditorInspector::_edit_request_change(Object *p_object, const String &p_property) {

    if (object != p_object) //may be undoing/redoing for a non edited object, so ignore
        return;

    if (changing)
        return;

    if (p_property.empty())
        update_tree_pending = true;
    else {
        pending.insert(p_property);
    }
}

void EditorInspector::_edit_set(const String &p_name, const Variant &p_value, bool p_refresh_all, const String &p_changed_field) {

    if (autoclear && editor_property_map.contains(p_name)) {
        for (List<EditorProperty *>::Element *E = editor_property_map[p_name].front(); E; E = E->next()) {
            if (E->deref()->is_checkable()) {
                E->deref()->set_checked(true);
            }
        }
    }

    if (!undo_redo || bool(object->call("_dont_undo_redo"))) {

        object->set(p_name, p_value);
        if (p_refresh_all)
            _edit_request_change(object, "");
        else
            _edit_request_change(object, p_name);

        emit_signal(_prop_edited, p_name);

    } else if (object_cast<MultiNodeEdit>(object)) {

        object_cast<MultiNodeEdit>(object)->set_property_field(p_name, p_value, p_changed_field);
        _edit_request_change(object, p_name);
        emit_signal(_prop_edited, p_name);
    } else {

        undo_redo->create_action(TTR("Set") + " " + p_name, UndoRedo::MERGE_ENDS);
        undo_redo->add_do_property(object, p_name, p_value);
        undo_redo->add_undo_property(object, p_name, object->get(p_name));

        if (p_refresh_all) {
            undo_redo->add_do_method(this, "_edit_request_change", Variant(object), "");
            undo_redo->add_undo_method(this, "_edit_request_change", Variant(object), "");
        } else {

            undo_redo->add_do_method(this, "_edit_request_change", Variant(object), p_name);
            undo_redo->add_undo_method(this, "_edit_request_change", Variant(object), p_name);
        }

        Resource *r = object_cast<Resource>(object);
        if (r) {

            if (String(p_name) == "resource_local_to_scene") {
                bool prev = object->get(p_name);
                bool next = p_value;
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

    if (editor_property_map.contains(p_name)) {
        for (List<EditorProperty *>::Element *E = editor_property_map[p_name].front(); E; E = E->next()) {
            E->deref()->update_reload_status();
        }
    }
}

void EditorInspector::_property_changed(const String &p_path, const Variant &p_value, const String &p_name, bool changing) {

    // The "changing" variable must be true for properties that trigger events as typing occurs,
    // like "text_changed" signal. eg: Text property of Label, Button, RichTextLabel, etc.
    if (changing)
        this->changing++;

    _edit_set(p_path, p_value, false, p_name);

    if (changing)
        this->changing--;

    if (restart_request_props.contains(p_path)) {
        emit_signal("restart_requested");
    }
}

void EditorInspector::_property_changed_update_all(const String &p_path, const Variant &p_value, const String &p_name, bool p_changing) {
    update_tree();
}

void EditorInspector::_multiple_properties_changed(const Vector<String>& p_paths, Array p_values) {

    ERR_FAIL_COND(p_paths.empty() || p_values.empty())
    ERR_FAIL_COND(p_paths.size() != p_values.size())
    String names;
    for (int i = 0; i < p_paths.size(); i++) {
        if (i > 0)
            names += ",";
        names += p_paths[i];
    }
    undo_redo->create_action(TTR("Set Multiple:") + " " + names, UndoRedo::MERGE_ENDS);
    for (int i = 0; i < p_paths.size(); i++) {
        _edit_set(p_paths[i], p_values[i], false, "");
        if (restart_request_props.contains(p_paths[i])) {
            emit_signal("restart_requested");
        }
    }
    changing++;
    undo_redo->commit_action();
    changing--;
}

void EditorInspector::_property_keyed(const String &p_path, bool p_advance) {

    if (!object)
        return;

    emit_signal("property_keyed", p_path, object->get(p_path), p_advance); //second param is deprecated
}

void EditorInspector::_property_keyed_with_value(const String &p_path, const Variant &p_value, bool p_advance) {

    if (!object)
        return;

    emit_signal("property_keyed", p_path, p_value, p_advance); //second param is deprecated
}

void EditorInspector::_property_checked(const String &p_path, bool p_checked) {

    if (!object)
        return;

    //property checked
    if (autoclear) {

        if (!p_checked) {
            object->set(p_path, Variant());
        } else {

            Variant to_create;
            ListPOD<PropertyInfo> pinfo;
            object->get_property_list(&pinfo);
            for (const PropertyInfo &E : pinfo) {
                if (E.name == p_path) {
                    Variant::CallError ce;
                    to_create = Variant::construct(E.type, nullptr, 0, ce);
                    break;
                }
            }
            object->set(p_path, to_create);
        }

        if (editor_property_map.contains(p_path)) {
            for (List<EditorProperty *>::Element *E = editor_property_map[p_path].front(); E; E = E->next()) {
                E->deref()->update_property();
                E->deref()->update_reload_status();
            }
        }

    } else {
        emit_signal("property_toggled", p_path, p_checked);
    }
}

void EditorInspector::_property_selected(const String &p_path, int p_focusable) {

    property_selected = p_path;
    property_focusable = p_focusable;
    //deselect the others
    for (eastl::pair<const StringName,List<EditorProperty *> > &F : editor_property_map) {
        if (F.first == property_selected)
            continue;
        for (List<EditorProperty *>::Element *E = F.second.front(); E; E = E->next()) {
            if (E->deref()->is_selected())
                E->deref()->deselect();
        }
    }

    emit_signal("property_selected", p_path);
}

void EditorInspector::_object_id_selected(const String &p_path, ObjectID p_id) {

    emit_signal("object_id_selected", p_id);
}

void EditorInspector::_resource_selected(const String &p_path, const RES& p_resource) {
    emit_signal("resource_selected", p_resource, p_path);
}

void EditorInspector::_node_removed(Node *p_node) {

    if (p_node == object) {
        edit(nullptr);
    }
}

void EditorInspector::_notification(int p_what) {

    if (p_what == NOTIFICATION_READY) {
        EditorFeatureProfileManager::get_singleton()->connect("current_feature_profile_changed", this, "_feature_profile_changed");
    }

    if (p_what == NOTIFICATION_ENTER_TREE) {

        if (sub_inspector) {
            add_style_override("bg", get_stylebox("sub_inspector_bg", "Editor"));
        } else {
            add_style_override("bg", get_stylebox("bg", "Tree"));
            get_tree()->connect("node_removed", this, "_node_removed");
        }
    }
    if (p_what == NOTIFICATION_PREDELETE) {
        edit(nullptr); //just in case
    }
    if (p_what == NOTIFICATION_EXIT_TREE) {

        if (!sub_inspector) {
            get_tree()->disconnect("node_removed", this, "_node_removed");
        }
        edit(nullptr);
    }

    if (p_what == NOTIFICATION_PROCESS) {

        if (update_scroll_request >= 0) {
            get_v_scrollbar()->call_deferred("set_value", update_scroll_request);
            update_scroll_request = -1;
        }
        if (refresh_countdown > 0) {
            refresh_countdown -= get_process_delta_time();
            if (refresh_countdown <= 0) {
                for (eastl::pair<const StringName,List<EditorProperty *> > &F : editor_property_map) {
                    for (List<EditorProperty *>::Element *E = F.second.front(); E; E = E->next()) {
                        E->deref()->update_property();
                        E->deref()->update_reload_status();
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
                    for (List<EditorProperty *>::Element *E = editor_property_map[prop].front(); E; E = E->next()) {
                        E->deref()->update_property();
                        E->deref()->update_reload_status();
                    }
                }
                pending.erase(pending.begin());
            }
        }

        changing--;
    }

    if (p_what == EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED) {

        if (sub_inspector) {
            add_style_override("bg", get_stylebox("sub_inspector_bg", "Editor"));
        } else if (is_inside_tree()) {
            add_style_override("bg", get_stylebox("bg", "Tree"));
        }

        update_tree();
    }
}

void EditorInspector::_changed_callback(Object *p_changed, const char *p_prop) {
    //this is called when property change is notified via _change_notify()
    _edit_request_change(p_changed, p_prop);
}

void EditorInspector::_vscroll_changed(double p_offset) {

    if (update_scroll_request >= 0) //waiting, do nothing
        return;

    if (object) {
        scroll_cache[object->get_instance_id()] = p_offset;
    }
}

void EditorInspector::set_property_prefix(const String &p_prefix) {
    property_prefix = p_prefix;
}

String EditorInspector::get_property_prefix() const {
    return property_prefix;
}

void EditorInspector::set_object_class(const String &p_class) {
    object_class = p_class;
}

String EditorInspector::get_object_class() const {
    return object_class;
}

void EditorInspector::_feature_profile_changed() {

    update_tree();
}

void EditorInspector::_bind_methods() {

    MethodBinder::bind_method("_property_changed", &EditorInspector::_property_changed, {DEFVAL(""), DEFVAL(false)});
    MethodBinder::bind_method("_multiple_properties_changed", &EditorInspector::_multiple_properties_changed);
    MethodBinder::bind_method("_property_changed_update_all", &EditorInspector::_property_changed_update_all);

    MethodBinder::bind_method("_edit_request_change", &EditorInspector::_edit_request_change);
    MethodBinder::bind_method("_node_removed", &EditorInspector::_node_removed);
    MethodBinder::bind_method("_filter_changed", &EditorInspector::_filter_changed);
    MethodBinder::bind_method("_property_keyed", &EditorInspector::_property_keyed);
    MethodBinder::bind_method("_property_keyed_with_value", &EditorInspector::_property_keyed_with_value);
    MethodBinder::bind_method("_property_checked", &EditorInspector::_property_checked);
    MethodBinder::bind_method("_property_selected", &EditorInspector::_property_selected);
    MethodBinder::bind_method("_resource_selected", &EditorInspector::_resource_selected);
    MethodBinder::bind_method("_object_id_selected", &EditorInspector::_object_id_selected);
    MethodBinder::bind_method("_vscroll_changed", &EditorInspector::_vscroll_changed);
    MethodBinder::bind_method("_feature_profile_changed", &EditorInspector::_feature_profile_changed);

    MethodBinder::bind_method("refresh", &EditorInspector::refresh);

    ADD_SIGNAL(MethodInfo("property_selected", PropertyInfo(VariantType::STRING, "property")));
    ADD_SIGNAL(MethodInfo("property_keyed", PropertyInfo(VariantType::STRING, "property")));
    ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(VariantType::OBJECT, "res"), PropertyInfo(VariantType::STRING, "prop")));
    ADD_SIGNAL(MethodInfo("object_id_selected", PropertyInfo(VariantType::INT, "id")));
    ADD_SIGNAL(MethodInfo("property_edited", PropertyInfo(VariantType::STRING, "property")));
    ADD_SIGNAL(MethodInfo("property_toggled", PropertyInfo(VariantType::STRING, "property"), PropertyInfo(VariantType::BOOL, "checked")));
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

    show_categories = false;
    hide_script = true;
    use_doc_hints = false;
    capitalize_paths = true;
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

    get_v_scrollbar()->connect("value_changed", this, "_vscroll_changed");
    update_scroll_request = -1;
}
