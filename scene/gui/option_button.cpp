/*************************************************************************/
/*  option_button.cpp                                                    */
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

#include "option_button.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/print_string.h"
#include "core/os/input.h"
#include "scene/resources/style_box.h"

IMPL_GDCLASS(OptionButton)

Size2 OptionButton::get_minimum_size() const {

    Size2 minsize = Button::get_minimum_size();

    if (has_icon("arrow")) {
        const Size2 padding = get_theme_stylebox("normal")->get_minimum_size();
        const Size2 arrow_size = Control::get_theme_icon("arrow")->get_size();

        Size2 content_size = minsize - padding;
        content_size.width += arrow_size.width + get_theme_constant("hseparation");
        content_size.height = M_MAX(content_size.height, arrow_size.height);

        minsize = content_size + padding;
    }

    return minsize;
}

void OptionButton::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_DRAW: {

            if (!has_icon("arrow"))
                return;

            RenderingEntity ci = get_canvas_item();
            Ref<Texture> arrow = Control::get_theme_icon("arrow");
            Color clr = Color(1, 1, 1);
            if (get_theme_constant("modulate_arrow")) {
                switch (get_draw_mode()) {
                    case DRAW_PRESSED:
                        clr = get_theme_color("font_color_pressed");
                        break;
                    case DRAW_HOVER:
                        clr = get_theme_color("font_color_hover");
                        break;
                    case DRAW_DISABLED:
                        clr = get_theme_color("font_color_disabled");
                        break;
                    default:
                        if (has_focus()) {
                            clr = get_theme_color("font_color_focus");
                        } else {
                        clr = get_theme_color("font_color");
                }
            }
            }

            Size2 size = get_size();

            Point2 ofs(size.width - arrow->get_width() - get_theme_constant("arrow_margin"),
                    int(Math::abs((size.height - arrow->get_height()) / 2)));
            arrow->draw(ci, ofs, clr);
        } break;
        case NOTIFICATION_THEME_CHANGED: {

            if (has_icon("arrow")) {
                _set_internal_margin(Margin::Right, Control::get_theme_icon("arrow")->get_width());
            }
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {

            if (!is_visible_in_tree()) {
                popup->hide();
            }
        } break;
    }
}

void OptionButton::_focused(int p_which) {
    emit_signal("item_focused", p_which);
}

void OptionButton::_selected(int p_which) {

    _select(p_which, true);
}

void OptionButton::pressed() {

    Size2 size = get_size();
    popup->set_global_position(get_global_position() + Size2(0, size.height * get_global_transform().get_scale().y));
    popup->set_size(Size2(size.width, 0));
    popup->set_scale(get_global_transform().get_scale());
    // If not triggered by the mouse, start the popup with its first item selected.
    if (popup->get_item_count() > 0 &&
            ((get_action_mode() == ActionMode::ACTION_MODE_BUTTON_PRESS && Input::get_singleton()->is_action_just_pressed("ui_accept")) ||
                    (get_action_mode() == ActionMode::ACTION_MODE_BUTTON_RELEASE && Input::get_singleton()->is_action_just_released("ui_accept")))) {
        popup->set_current_index(0);
    }
    popup->popup();
}

void OptionButton::add_icon_item(const Ref<Texture> &p_icon, const StringName &p_label, int p_id) {

    popup->add_icon_radio_check_item(p_icon, p_label, p_id);
    if (popup->get_item_count() == 1)
        select(0);
}
void OptionButton::add_item(StringView p_label, int p_id) {

    popup->add_radio_check_item(p_label, p_id);
    if (popup->get_item_count() == 1)
        select(0);
}

void OptionButton::set_item_text(int p_idx, const StringName &p_text) {

    popup->set_item_text(p_idx, p_text);

    if (current == p_idx)
        set_text(p_text);
}
void OptionButton::set_item_icon(int p_idx, const Ref<Texture> &p_icon) {

    popup->set_item_icon(p_idx, p_icon);

    if (current == p_idx)
        set_button_icon(p_icon);
}
void OptionButton::set_item_id(int p_idx, int p_id) {

    popup->set_item_id(p_idx, p_id);
}

void OptionButton::set_item_metadata(int p_idx, const Variant &p_metadata) {

    popup->set_item_metadata(p_idx, p_metadata);
}

void OptionButton::set_item_tooltip(int p_idx, const StringName &p_tooltip) {
    popup->set_item_tooltip(p_idx, p_tooltip);
}
void OptionButton::set_item_disabled(int p_idx, bool p_disabled) {

    popup->set_item_disabled(p_idx, p_disabled);
}

const String & OptionButton::get_item_text(int p_idx) const {
    return popup->get_item_text(p_idx);
}

Ref<Texture> OptionButton::get_item_icon(int p_idx) const {

    return popup->get_item_icon(p_idx);
}

int OptionButton::get_item_id(int p_idx) const {

    return popup->get_item_id(p_idx);
}

int OptionButton::get_item_index(int p_id) const {

    return popup->get_item_index(p_id);
}

Variant OptionButton::get_item_metadata(int p_idx) const {

    return popup->get_item_metadata(p_idx);
}

const String &OptionButton::get_item_tooltip(int p_idx) const {
    return popup->get_item_tooltip(p_idx);
}
bool OptionButton::is_item_disabled(int p_idx) const {

    return popup->is_item_disabled(p_idx);
}

int OptionButton::get_item_count() const {

    return popup->get_item_count();
}

void OptionButton::add_separator() {

    popup->add_separator();
}

void OptionButton::clear() {

    popup->clear();
    set_text("");
    current = -1;
}

void OptionButton::_select(int p_which, bool p_emit) {

    if (p_which < 0)
        return;
    if (p_which == current)
        return;

    ERR_FAIL_INDEX(p_which, popup->get_item_count());

    for (int i = 0; i < popup->get_item_count(); i++) {

        popup->set_item_checked(i, i == p_which);
    }

    current = p_which;
    set_text(popup->get_item_text(current));
    set_button_icon(popup->get_item_icon(current));

    if (is_inside_tree() && p_emit)
        emit_signal("item_selected", current);
}

void OptionButton::_select_int(int p_which) {

    if (p_which < 0 || p_which >= popup->get_item_count())
        return;
    _select(p_which, false);
}

void OptionButton::select(int p_idx) {

    _select(p_idx, false);
}

int OptionButton::get_selected() const {

    return current;
}

int OptionButton::get_selected_id() const {

    int idx = get_selected();
    if (idx < 0)
        return 0;
    return get_item_id(current);
}
Variant OptionButton::get_selected_metadata() const {

    int idx = get_selected();
    if (idx < 0)
        return Variant();
    return get_item_metadata(current);
}

void OptionButton::remove_item(int p_idx) {

    popup->remove_item(p_idx);
}

PopupMenu *OptionButton::get_popup() const {

    return popup;
}

Array OptionButton::_get_items() const {

    Array items;
    for (int i = 0; i < get_item_count(); i++) {

        items.push_back(get_item_text(i));
        items.push_back(get_item_icon(i));
        items.push_back(is_item_disabled(i));
        items.push_back(get_item_id(i));
        items.push_back(get_item_metadata(i));
    }

    return items;
}
void OptionButton::_set_items(const Array &p_items) {

    ERR_FAIL_COND(p_items.size() % 5);
    clear();

    for (int i = 0; i < p_items.size(); i += 5) {

        StringName text = p_items[i + 0].as<StringName>();
        Ref<Texture> icon = refFromVariant<Texture>(p_items[i + 1]);
        bool disabled = p_items[i + 2].as<bool>();
        int id = p_items[i + 3].as<int>();
        Variant meta = p_items[i + 4];

        int idx = get_item_count();
        add_item(text, id);
        set_item_icon(idx, icon);
        set_item_disabled(idx, disabled);
        set_item_metadata(idx, meta);
    }
}

void OptionButton::get_translatable_strings(List<String> *p_strings) const {

    popup->get_translatable_strings(p_strings);
}

void OptionButton::_bind_methods() {
    SE_BIND_METHOD_WITH_DEFAULTS(OptionButton, add_item, DEFVAL(-1));
    SE_BIND_METHOD_WITH_DEFAULTS(OptionButton, add_icon_item, DEFVAL(-1));

    SE_BIND_METHOD(OptionButton,set_item_text);
    SE_BIND_METHOD(OptionButton,set_item_icon);
    SE_BIND_METHOD(OptionButton,set_item_disabled);
    SE_BIND_METHOD(OptionButton,set_item_id);
    SE_BIND_METHOD(OptionButton,set_item_metadata);
    SE_BIND_METHOD(OptionButton,set_item_tooltip);
    SE_BIND_METHOD(OptionButton,get_item_text);
    SE_BIND_METHOD(OptionButton,get_item_icon);
    SE_BIND_METHOD(OptionButton,get_item_id);
    SE_BIND_METHOD(OptionButton,get_item_index);
    SE_BIND_METHOD(OptionButton,get_item_metadata);
    SE_BIND_METHOD(OptionButton,get_item_tooltip);
    SE_BIND_METHOD(OptionButton,is_item_disabled);
    SE_BIND_METHOD(OptionButton,get_item_count);
    SE_BIND_METHOD(OptionButton,add_separator);
    SE_BIND_METHOD(OptionButton,clear);
    SE_BIND_METHOD(OptionButton,select);
    SE_BIND_METHOD(OptionButton,get_selected);
    SE_BIND_METHOD(OptionButton,get_selected_id);
    SE_BIND_METHOD(OptionButton,get_selected_metadata);
    SE_BIND_METHOD(OptionButton,remove_item);
    SE_BIND_METHOD(OptionButton,_select_int);

    SE_BIND_METHOD(OptionButton,get_popup);

    SE_BIND_METHOD(OptionButton,_set_items);
    SE_BIND_METHOD(OptionButton,_get_items);

    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "items", PropertyHint::None, "",
                         PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL),
            "_set_items", "_get_items");
    // "selected" property must come after "items", otherwise GH-10213 occurs.
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "selected"), "_select_int", "get_selected");
    ADD_SIGNAL(MethodInfo("item_selected", PropertyInfo(VariantType::INT, "index")));
    ADD_SIGNAL(MethodInfo("item_focused", PropertyInfo(VariantType::INT, "index")));
}

OptionButton::OptionButton() {

    current = -1;
    set_toggle_mode(true);
    set_text_align(UiTextAlign::ALIGN_LEFT);
    set_action_mode(ACTION_MODE_BUTTON_PRESS);
    if (has_icon("arrow")) {
        _set_internal_margin(Margin::Right, Control::get_theme_icon("arrow")->get_width());
    }
    popup = memnew(PopupMenu);
    popup->hide();
    add_child(popup);
    popup->set_pass_on_modal_close_click(false);
    popup->set_notify_transform(true);
    popup->set_allow_search(true);
    popup->connect("index_pressed",callable_mp(this, &ClassName::_selected));
    popup->connect("id_focused",callable_mp(this, &ClassName::_focused));
    popup->connectF("popup_hide", this, [=]() { set_pressed(false); });
}

OptionButton::~OptionButton() {}
