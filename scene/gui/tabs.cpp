/*************************************************************************/
/*  tabs.cpp                                                             */
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

#include "tabs.h"

#include "core/callable_method_pointer.h"
#include "core/dictionary.h"
#include "core/input/input_event.h"
#include "core/message_queue.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"

IMPL_GDCLASS(Tabs)
VARIANT_ENUM_CAST(Tabs::TabAlign);
VARIANT_ENUM_CAST(Tabs::CloseButtonDisplayPolicy);

Size2 Tabs::get_minimum_size() const {

    Ref<StyleBox> tab_bg = get_theme_stylebox("tab_bg");
    Ref<StyleBox> tab_fg = get_theme_stylebox("tab_fg");
    Ref<StyleBox> tab_disabled = get_theme_stylebox("tab_disabled");
    Ref<Font> font = get_theme_font("font");

    Size2 ms(0, M_MAX(M_MAX(tab_bg->get_minimum_size().height, tab_fg->get_minimum_size().height), tab_disabled->get_minimum_size().height) + font->get_height());

    for (int i = 0; i < tabs.size(); i++) {

        Ref<Texture> tex = tabs[i].icon;
        if (tex) {
            ms.height = M_MAX(ms.height, tex->get_size().height);
            if (!tabs[i].text.empty())
                ms.width += get_theme_constant("hseparation");
        }

        ms.width += Math::ceil(font->get_string_size(tabs[i].xl_text).width);

        if (tabs[i].disabled)
            ms.width += tab_disabled->get_minimum_size().width;
        else if (current == i)
            ms.width += tab_fg->get_minimum_size().width;
        else
            ms.width += tab_bg->get_minimum_size().width;

        if (tabs[i].right_button) {
            Ref<Texture> rb = tabs[i].right_button;
            Size2 bms = rb->get_size();
            bms.width += get_theme_constant("hseparation");
            ms.width += bms.width;
            ms.height = M_MAX(bms.height + tab_bg->get_minimum_size().height, ms.height);
        }

        if (cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && i == current)) {
            Ref<Texture> cb = get_theme_icon("close");
            Size2 bms = cb->get_size();
            bms.width += get_theme_constant("hseparation");
            ms.width += bms.width;
            ms.height = M_MAX(bms.height + tab_bg->get_minimum_size().height, ms.height);
        }
    }

    ms.width = 0; //TODO: should make this optional.
    return ms;
}

void Tabs::_gui_input(const Ref<InputEvent> &p_event) {

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);

    if (mm) {

        Point2 pos = mm->get_position();

        if (buttons_visible) {

            Ref<Texture> incr = get_theme_icon("increment");
            Ref<Texture> decr = get_theme_icon("decrement");

            int limit = get_size().width - incr->get_width() - decr->get_width();

            if (pos.x > limit + decr->get_width()) {
                if (highlight_arrow != 1) {
                highlight_arrow = 1;
                    update();
                }
            } else if (pos.x > limit) {
                if (highlight_arrow != 0) {
                highlight_arrow = 0;
                    update();
                }
            } else if (highlight_arrow != -1) {
                highlight_arrow = -1;
                update();
            }
        }

        _update_hover();
        return;
    }

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);

    if (mb) {

        if (mb->is_pressed() && mb->get_button_index() == BUTTON_WHEEL_UP && !mb->get_command()) {

            if (scrolling_enabled && buttons_visible) {
                if (offset > 0) {
                    offset--;
                    update();
                }
            }
        }

        if (mb->is_pressed() && mb->get_button_index() == BUTTON_WHEEL_DOWN && !mb->get_command()) {
            if (scrolling_enabled && buttons_visible) {
                if (missing_right) {
                    offset++;
                    _ensure_no_over_offset(); // Avoid overreaching when scrolling fast.
                    update();
                }
            }
        }

        if (rb_pressing && !mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {

            if (rb_hover != -1) {
                // Right mouse button pressed.
                emit_signal("right_button_pressed", rb_hover);
            }

            rb_pressing = false;
            update();
        }

        if (cb_pressing && !mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {

            if (cb_hover != -1) {
                // Close button pressed.
                emit_signal("tab_close", cb_hover);
            }

            cb_pressing = false;
            update();
        }

        if (mb->is_pressed() && (mb->get_button_index() == BUTTON_LEFT || (select_with_rmb && mb->get_button_index() == BUTTON_RIGHT))) {

            // Clicks.
            Point2 pos(mb->get_position().x, mb->get_position().y);

            if (buttons_visible) {

                Ref<Texture> incr = get_theme_icon("increment");
                Ref<Texture> decr = get_theme_icon("decrement");

                int limit = get_size().width - incr->get_width() - decr->get_width();

                if (pos.x > limit + decr->get_width()) {
                    if (missing_right) {
                        offset++;
                        update();
                    }
                    return;
                } else if (pos.x > limit) {
                    if (offset > 0) {
                        offset--;
                        update();
                    }
                    return;
                }
            }

            if (tabs.empty()) {
                // Return early if there are no actual tabs to handle input for.
                return;
            }

            int found = -1;
            for (int i = offset; i <= max_drawn_tab; i++) {

                if (i < offset)
                    continue;

                if (tabs[i].rb_rect.has_point(pos)) {
                    rb_pressing = true;
                    update();
                    return;
                }

                if (tabs[i].cb_rect.has_point(pos) &&
                        (cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS ||
                        (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && i == current))) {
                    cb_pressing = true;
                    update();
                    return;
                }

                if (pos.x >= tabs[i].ofs_cache && pos.x < tabs[i].ofs_cache + tabs[i].size_cache) {
                    if (!tabs[i].disabled) {
                        found = i;
                    }
                    break;
                }
            }

            if (found != -1) {

                set_current_tab(found);
                emit_signal("tab_clicked", found);
            }
        }
    }
}

void Tabs::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_TRANSLATION_CHANGED: {
            for (int i = 0; i < tabs.size(); ++i) {
                tabs[i].xl_text = tr(tabs[i].text);
            }
            minimum_size_changed();
            update();
        } break;
        case NOTIFICATION_RESIZED: {
            _update_cache();
            _ensure_no_over_offset();
            ensure_tab_visible(current);
        } break;
        case NOTIFICATION_DRAW: {
            _update_cache();
            RenderingEntity ci = get_canvas_item();

            Ref<StyleBox> tab_bg = get_theme_stylebox("tab_bg");
            Ref<StyleBox> tab_fg = get_theme_stylebox("tab_fg");
            Ref<StyleBox> tab_disabled = get_theme_stylebox("tab_disabled");
            Ref<Font> font = get_theme_font("font");
            Color color_fg = get_theme_color("font_color_fg");
            Color color_bg = get_theme_color("font_color_bg");
            Color color_disabled = get_theme_color("font_color_disabled");
            Ref<Texture> close = get_theme_icon("close");

            int h = get_size().height;
            int w = 0;
            int mw = 0;

            for (int i = 0; i < tabs.size(); i++) {

                tabs[i].ofs_cache = mw;
                mw += get_tab_width(i);
            }

            if (tab_align == ALIGN_CENTER) {
                w = (get_size().width - mw) / 2;
            } else if (tab_align == ALIGN_RIGHT) {
                w = get_size().width - mw;
            }

            if (w < 0) {
                w = 0;
            }

            Ref<Texture> incr = get_theme_icon("increment");
            Ref<Texture> decr = get_theme_icon("decrement");
            Ref<Texture> incr_hl = get_theme_icon("increment_highlight");
            Ref<Texture> decr_hl = get_theme_icon("decrement_highlight");

            int limit = get_size().width - incr->get_size().width - decr->get_size().width;

            missing_right = false;

            for (int i = 0; i < tabs.size(); i++) {

                if (i < offset)
                    continue;

                tabs[i].ofs_cache = w;

                int lsize = tabs[i].size_cache;

                Ref<StyleBox> sb;
                Color col;

                if (tabs[i].disabled) {
                    sb = tab_disabled;
                    col = color_disabled;
                } else if (i == current) {
                    sb = tab_fg;
                    col = color_fg;
                } else {
                    sb = tab_bg;
                    col = color_bg;
                }

                if (w + lsize > limit) {
                    max_drawn_tab = i - 1;
                    missing_right = true;
                    break;
                } else {
                    max_drawn_tab = i;
                }

                Rect2 sb_rect = Rect2(w, 0, tabs[i].size_cache, h);
                sb->draw(ci, sb_rect);

                w += sb->get_margin(Margin::Left);

                Size2i sb_ms = sb->get_minimum_size();
                Ref<Texture> icon = tabs[i].icon;
                if (icon) {

                    icon->draw(ci, Point2i(w, sb->get_margin(Margin::Top) + ((sb_rect.size.y - sb_ms.y) - icon->get_height()) / 2));
                    if (!tabs[i].text.empty())
                        w += icon->get_width() + get_theme_constant("hseparation");
                }

                font->draw(ci,
                        Point2i(w, sb->get_margin(Margin::Top) + ((sb_rect.size.y - sb_ms.y) - font->get_height()) / 2 +
                                           font->get_ascent()),
                        tabs[i].xl_text, col, tabs[i].size_text);

                w += tabs[i].size_text;

                if (tabs[i].right_button) {

                    Ref<StyleBox> style = get_theme_stylebox("button");
                    Ref<Texture> rb = tabs[i].right_button;

                    w += get_theme_constant("hseparation");

                    Rect2 rb_rect;
                    rb_rect.size = style->get_minimum_size() + rb->get_size();
                    rb_rect.position.x = w;
                    rb_rect.position.y = sb->get_margin(Margin::Top) + ((sb_rect.size.y - sb_ms.y) - (rb_rect.size.y)) / 2;

                    if (rb_hover == i) {
                        if (rb_pressing)
                            get_theme_stylebox("button_pressed")->draw(ci, rb_rect);
                        else
                            style->draw(ci, rb_rect);
                    }

                    rb->draw(ci, Point2i(w + style->get_margin(Margin::Left), rb_rect.position.y + style->get_margin(Margin::Top)));
                    w += rb->get_width();
                    tabs[i].rb_rect = rb_rect;
                }

                if (cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && i == current)) {

                    Ref<StyleBox> style = get_theme_stylebox("button");
                    Ref<Texture> cb = close;

                    w += get_theme_constant("hseparation");

                    Rect2 cb_rect;
                    cb_rect.size = style->get_minimum_size() + cb->get_size();
                    cb_rect.position.x = w;
                    cb_rect.position.y = sb->get_margin(Margin::Top) + ((sb_rect.size.y - sb_ms.y) - (cb_rect.size.y)) / 2;

                    if (!tabs[i].disabled && cb_hover == i) {
                        if (cb_pressing)
                            get_theme_stylebox("button_pressed")->draw(ci, cb_rect);
                        else
                            style->draw(ci, cb_rect);
                    }

                    cb->draw(ci, Point2i(w + style->get_margin(Margin::Left), cb_rect.position.y + style->get_margin(Margin::Top)));
                    w += cb->get_width();
                    tabs[i].cb_rect = cb_rect;
                }

                w += sb->get_margin(Margin::Right);
            }

            if (offset > 0 || missing_right) {

                int vofs = (get_size().height - incr->get_size().height) / 2;

                if (offset > 0)
                    draw_texture(highlight_arrow == 0 ? decr_hl : decr, Point2(limit, vofs));
                else
                    draw_texture(decr, Point2(limit, vofs), Color(1, 1, 1, 0.5));

                if (missing_right)
                    draw_texture(highlight_arrow == 1 ? incr_hl : incr, Point2(limit + decr->get_size().width, vofs));
                else
                    draw_texture(incr, Point2(limit + decr->get_size().width, vofs), Color(1, 1, 1, 0.5));

                buttons_visible = true;
            } else {
                buttons_visible = false;
            }
        } break;
    }
}

int Tabs::get_tab_count() const {

    return tabs.size();
}

void Tabs::set_current_tab(int p_current) {

    if (current == p_current) {return;}
    ERR_FAIL_INDEX(p_current, get_tab_count());

    previous = current;
    current = p_current;

    Object_change_notify(this,"current_tab");
    _update_cache();
    update();

    emit_signal("tab_changed", p_current);
}

int Tabs::get_current_tab() const {

    return current;
}

int Tabs::get_previous_tab() const {
    return previous;
}

int Tabs::get_hovered_tab() const {
    return hover;
}

int Tabs::get_tab_offset() const {
    return offset;
}

bool Tabs::get_offset_buttons_visible() const {
    return buttons_visible;
}

void Tabs::set_tab_title(int p_tab, const StringName &p_title) {

    ERR_FAIL_INDEX(p_tab, tabs.size());
    tabs[p_tab].text = p_title;
    tabs[p_tab].xl_text = tr(p_title);
    update();
    minimum_size_changed();
}

StringName Tabs::get_tab_title(int p_tab) const {

    ERR_FAIL_INDEX_V(p_tab, tabs.size(), StringName());
    return tabs[p_tab].text;
}

void Tabs::set_tab_icon(int p_tab, const Ref<Texture> &p_icon) {

    ERR_FAIL_INDEX(p_tab, tabs.size());
    tabs[p_tab].icon = p_icon;
    update();
    minimum_size_changed();
}

Ref<Texture> Tabs::get_tab_icon(int p_tab) const {

    ERR_FAIL_INDEX_V(p_tab, tabs.size(), Ref<Texture>());
    return tabs[p_tab].icon;
}

void Tabs::set_tab_disabled(int p_tab, bool p_disabled) {

    ERR_FAIL_INDEX(p_tab, tabs.size());
    tabs[p_tab].disabled = p_disabled;
    update();
}
bool Tabs::get_tab_disabled(int p_tab) const {

    ERR_FAIL_INDEX_V(p_tab, tabs.size(), false);
    return tabs[p_tab].disabled;
}

void Tabs::set_tab_right_button(int p_tab, const Ref<Texture> &p_right_button) {

    ERR_FAIL_INDEX(p_tab, tabs.size());
    tabs[p_tab].right_button = p_right_button;
    _update_cache();
    update();
    minimum_size_changed();
}
Ref<Texture> Tabs::get_tab_right_button(int p_tab) const {

    ERR_FAIL_INDEX_V(p_tab, tabs.size(), Ref<Texture>());
    return tabs[p_tab].right_button;
}

void Tabs::_update_hover() {

    if (!is_inside_tree()) {
        return;
    }

    const Point2 &pos = get_local_mouse_position();
    // Test hovering to display right or close button.
    int hover_now = -1;
    int hover_buttons = -1;
    for (int i = 0; i < tabs.size(); i++) {

        if (i < offset)
            continue;

        Rect2 rect = get_tab_rect(i);
        if (rect.has_point(pos)) {
            hover_now = i;
        }
        if (tabs[i].rb_rect.has_point(pos)) {
            rb_hover = i;
            cb_hover = -1;
            hover_buttons = i;
            break;
        } else if (!tabs[i].disabled && tabs[i].cb_rect.has_point(pos)) {
            cb_hover = i;
            rb_hover = -1;
            hover_buttons = i;
            break;
        }
    }
    if (hover != hover_now) {
        hover = hover_now;
        emit_signal("tab_hover", hover);
    }

    if (hover_buttons == -1) { // No hover.
        rb_hover = hover_buttons;
        cb_hover = hover_buttons;
    }
}

void Tabs::_update_cache() {
    Ref<StyleBox> tab_disabled = get_theme_stylebox("tab_disabled");
    Ref<StyleBox> tab_bg = get_theme_stylebox("tab_bg");
    Ref<StyleBox> tab_fg = get_theme_stylebox("tab_fg");
    Ref<Font> font = get_theme_font("font");
    Ref<Texture> incr = get_theme_icon("increment");
    Ref<Texture> decr = get_theme_icon("decrement");
    int limit = get_size().width - incr->get_width() - decr->get_width();

    int w = 0;
    int mw = 0;
    int size_fixed = 0;
    int count_resize = 0;
    for (int i = 0; i < tabs.size(); i++) {
        tabs[i].ofs_cache = mw;
        tabs[i].size_cache = get_tab_width(i);
        tabs[i].size_text = Math::ceil(font->get_string_size(tabs[i].xl_text).width);
        mw += tabs[i].size_cache;
        if (tabs[i].size_cache <= min_width || i == current) {
            size_fixed += tabs[i].size_cache;
        } else {
            count_resize++;
        }
    }
    int m_width = min_width;
    if (count_resize > 0) {
        m_width = M_MAX((limit - size_fixed) / count_resize, min_width);
    }
    for (int i = 0; i < tabs.size(); i++) {
        if (i < offset)
            continue;
        Ref<StyleBox> sb;
        if (tabs[i].disabled) {
            sb = tab_disabled;
        } else if (i == current) {
            sb = tab_fg;
        } else {
            sb = tab_bg;
        }
        int lsize = tabs[i].size_cache;
        int slen = tabs[i].size_text;
        if (min_width > 0 && mw > limit && i != current) {
            if (lsize > m_width) {
                slen = m_width - (sb->get_margin(Margin::Left) + sb->get_margin(Margin::Right));
                if (tabs[i].icon) {
                    slen -= tabs[i].icon->get_width();
                    slen -= get_theme_constant("hseparation");
                }
                if (cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && i == current)) {
                    Ref<Texture> cb = get_theme_icon("close");
                    slen -= cb->get_width();
                    slen -= get_theme_constant("hseparation");
                }
                slen = M_MAX(slen, 1);
                lsize = m_width;
            }
        }
        tabs[i].ofs_cache = w;
        tabs[i].size_cache = lsize;
        tabs[i].size_text = slen;
        w += lsize;
    }
}

void Tabs::_on_mouse_exited() {

    rb_hover = -1;
    cb_hover = -1;
    hover = -1;
    highlight_arrow = -1;
    update();
}

void Tabs::add_tab(const StringName &p_str, const Ref<Texture> &p_icon) {

    Tab t;
    t.text = p_str;
    t.xl_text = tr(p_str);
    t.icon = p_icon;
    t.disabled = false;
    t.ofs_cache = 0;
    t.size_cache = 0;

    tabs.push_back(t);
    _update_cache();
    call_deferred([this]() { _update_hover(); });
    update();
    minimum_size_changed();
}

void Tabs::clear_tabs() {
    tabs.clear();
    current = 0;
    previous = 0;

    call_deferred([this]() { _update_hover(); });
    update();
}

void Tabs::remove_tab(int p_idx) {

    ERR_FAIL_INDEX(p_idx, tabs.size());
    tabs.erase_at(p_idx);
    if (current >= p_idx)
        current--;
    _update_cache();
    call_deferred([this]() { _update_hover(); });
    update();
    minimum_size_changed();

    if (current < 0) {
        current = 0;
        previous = 0;
    }
    if (current >= tabs.size())
        current = tabs.size() - 1;

    _ensure_no_over_offset();
}

Variant Tabs::get_drag_data(const Point2 &p_point) {

    if (!drag_to_rearrange_enabled)
        return Variant();

    int tab_over = get_tab_idx_at_point(p_point);

    if (tab_over < 0)
        return Variant();

    HBoxContainer *drag_preview = memnew(HBoxContainer);

    if (tabs[tab_over].icon) {
        TextureRect *tf = memnew(TextureRect);
        tf->set_texture(tabs[tab_over].icon);
        drag_preview->add_child(tf);
    }
    Label *label = memnew(Label(tabs[tab_over].xl_text));
    drag_preview->add_child(label);
    if (tabs[tab_over].right_button) {
        TextureRect *tf = memnew(TextureRect);
        tf->set_texture(tabs[tab_over].right_button);
        drag_preview->add_child(tf);
    }
    set_drag_preview(drag_preview);

    Dictionary drag_data;
    drag_data["type"] = "tab_element";
    drag_data["tab_element"] = tab_over;
    drag_data["from_path"] = get_path();
    return drag_data;
}

bool Tabs::can_drop_data(const Point2 &p_point, const Variant &p_data) const {

    if (!drag_to_rearrange_enabled)
        return false;

    Dictionary d = p_data.as<Dictionary>();
    if (!d.has("type"))
        return false;

    if (d["type"] == "tab_element") {

        NodePath from_path = d["from_path"].as<NodePath>();
        NodePath to_path = get_path();
        if (from_path == to_path) {
            return true;
        } else if (get_tabs_rearrange_group() != -1) {
            // Drag and drop between other Tabs.
            Node *from_node = get_node(from_path);
            Tabs *from_tabs = object_cast<Tabs>(from_node);
            if (from_tabs && from_tabs->get_tabs_rearrange_group() == get_tabs_rearrange_group()) {
                return true;
            }
        }
    }
    return false;
}

void Tabs::drop_data(const Point2 &p_point, const Variant &p_data) {

    if (!drag_to_rearrange_enabled)
        return;

    int hover_now = get_tab_idx_at_point(p_point);

    Dictionary d = p_data.as<Dictionary>();
    if (!d.has("type"))
        return;

    if (d["type"] == "tab_element") {

        int tab_from_id = d["tab_element"].as<int>();
        NodePath from_path = d["from_path"].as<NodePath>();
        NodePath to_path = get_path();
        if (from_path == to_path) {
            if (hover_now < 0)
                hover_now = get_tab_count() - 1;
            move_tab(tab_from_id, hover_now);
            emit_signal("reposition_active_tab_request", hover_now);
            set_current_tab(hover_now);
        } else if (get_tabs_rearrange_group() != -1) {
            // Drag and drop between Tabs.
            Node *from_node = get_node(from_path);
            Tabs *from_tabs = object_cast<Tabs>(from_node);
            if (from_tabs && from_tabs->get_tabs_rearrange_group() == get_tabs_rearrange_group()) {
                if (tab_from_id >= from_tabs->get_tab_count())
                    return;
                Tab moving_tab = from_tabs->tabs[tab_from_id];
                if (hover_now < 0)
                    hover_now = get_tab_count();
                tabs.insert_at(hover_now, moving_tab);
                from_tabs->remove_tab(tab_from_id);
                set_current_tab(hover_now);
                emit_signal("tab_changed", hover_now);
                _update_cache();
            }
        }
    }
    update();
}

int Tabs::get_tab_idx_at_point(const Point2 &p_point) const {

    int hover_now = -1;
    for (int i = offset; i <= max_drawn_tab; i++) {

        Rect2 rect = get_tab_rect(i);
        if (rect.has_point(p_point)) {
            hover_now = i;
        }
    }

    return hover_now;
}

void Tabs::set_tab_align(TabAlign p_align) {

    ERR_FAIL_INDEX(p_align, ALIGN_MAX);
    tab_align = p_align;
    update();
}

Tabs::TabAlign Tabs::get_tab_align() const {

    return tab_align;
}

void Tabs::move_tab(int from, int to) {

    if (from == to)
        return;

    ERR_FAIL_INDEX(from, tabs.size());
    ERR_FAIL_INDEX(to, tabs.size());

    Tab tab_from = tabs[from];
    tabs.erase_at(from);
    tabs.insert_at(to, eastl::move(tab_from));

    _update_cache();
    update();
}

int Tabs::get_tab_width(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, tabs.size(), 0);

    Ref<StyleBox> tab_bg = get_theme_stylebox("tab_bg");
    Ref<StyleBox> tab_fg = get_theme_stylebox("tab_fg");
    Ref<StyleBox> tab_disabled = get_theme_stylebox("tab_disabled");
    Ref<Font> font = get_theme_font("font");

    int x = 0;

    Ref<Texture> tex = tabs[p_idx].icon;
    if (tex) {
        x += tex->get_width();
        if (!tabs[p_idx].text.empty())
            x += get_theme_constant("hseparation");
    }

    x += Math::ceil(font->get_string_size(tabs[p_idx].xl_text).width);

    if (tabs[p_idx].disabled)
        x += tab_disabled->get_minimum_size().width;
    else if (current == p_idx)
        x += tab_fg->get_minimum_size().width;
    else
        x += tab_bg->get_minimum_size().width;

    if (tabs[p_idx].right_button) {
        Ref<Texture> rb = tabs[p_idx].right_button;
        x += rb->get_width();
        x += get_theme_constant("hseparation");
    }

    if (cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && p_idx == current)) {
        Ref<Texture> cb = get_theme_icon("close");
        x += cb->get_width();
        x += get_theme_constant("hseparation");
    }

    return x;
}

void Tabs::_ensure_no_over_offset() {

    if (!is_inside_tree())
        return;

    Ref<Texture> incr = get_theme_icon("increment");
    Ref<Texture> decr = get_theme_icon("decrement");

    int limit = get_size().width - incr->get_width() - decr->get_width();

    while (offset > 0) {

        int total_w = 0;
        for (int i = 0; i < tabs.size(); i++) {

            if (i < offset - 1)
                continue;

            total_w += tabs[i].size_cache;
        }

        if (total_w < limit) {
            offset--;
            update();
        } else {
            break;
        }
    }
}

void Tabs::ensure_tab_visible(int p_idx) {

    if (!is_inside_tree() || tabs.empty()) {
        return;
    }

    ERR_FAIL_INDEX(p_idx, tabs.size());

    if (p_idx == offset) {
        return;
    }
    if (p_idx < offset) {
        offset = p_idx;
        update();
        return;
    }

    int prev_offset = offset;
    Ref<Texture> incr = get_theme_icon("increment");
    Ref<Texture> decr = get_theme_icon("decrement");
    int limit = get_size().width - incr->get_width() - decr->get_width();
    for (int i = offset; i <= p_idx; i++) {
        if (tabs[i].ofs_cache + tabs[i].size_cache > limit) {
            offset++;
        }
    }

    if (prev_offset != offset) {
        update();
    }
}

Rect2 Tabs::get_tab_rect(int p_tab) const {

    ERR_FAIL_INDEX_V(p_tab, tabs.size(), Rect2());
    return Rect2(tabs[p_tab].ofs_cache, 0, tabs[p_tab].size_cache, get_size().height);
}

void Tabs::set_tab_close_display_policy(CloseButtonDisplayPolicy p_policy) {

    ERR_FAIL_INDEX(p_policy, CLOSE_BUTTON_MAX);
    cb_displaypolicy = p_policy;
    update();
}

Tabs::CloseButtonDisplayPolicy Tabs::get_tab_close_display_policy() const {

    return cb_displaypolicy;
}

void Tabs::set_min_width(int p_width) {
    min_width = p_width;
}

void Tabs::set_scrolling_enabled(bool p_enabled) {
    scrolling_enabled = p_enabled;
}

bool Tabs::get_scrolling_enabled() const {
    return scrolling_enabled;
}

void Tabs::set_drag_to_rearrange_enabled(bool p_enabled) {
    drag_to_rearrange_enabled = p_enabled;
}

bool Tabs::get_drag_to_rearrange_enabled() const {
    return drag_to_rearrange_enabled;
}
void Tabs::set_tabs_rearrange_group(int p_group_id) {
    tabs_rearrange_group = p_group_id;
}

int Tabs::get_tabs_rearrange_group() const {
    return tabs_rearrange_group;
}

void Tabs::set_select_with_rmb(bool p_enabled) {
    select_with_rmb = p_enabled;
}

bool Tabs::get_select_with_rmb() const {
    return select_with_rmb;
}

void Tabs::_bind_methods() {

    SE_BIND_METHOD(Tabs,_gui_input);
    SE_BIND_METHOD(Tabs,_update_hover);
    SE_BIND_METHOD(Tabs,get_tab_count);
    SE_BIND_METHOD(Tabs,set_current_tab);
    SE_BIND_METHOD(Tabs,get_current_tab);
    SE_BIND_METHOD(Tabs,get_previous_tab);
    SE_BIND_METHOD(Tabs,set_tab_title);
    SE_BIND_METHOD(Tabs,get_tab_title);
    SE_BIND_METHOD(Tabs,set_tab_icon);
    SE_BIND_METHOD(Tabs,get_tab_icon);
    SE_BIND_METHOD(Tabs,set_tab_disabled);
    SE_BIND_METHOD(Tabs,get_tab_disabled);
    SE_BIND_METHOD(Tabs,remove_tab);
    MethodBinder::bind_method(D_METHOD("add_tab", {"title", "icon"}), &Tabs::add_tab, {DEFVAL(""), DEFVAL(Ref<Texture>())});
    SE_BIND_METHOD(Tabs,set_tab_align);
    SE_BIND_METHOD(Tabs,get_tab_align);
    SE_BIND_METHOD(Tabs,get_tab_offset);
    SE_BIND_METHOD(Tabs,get_offset_buttons_visible);
    SE_BIND_METHOD(Tabs,ensure_tab_visible);
    SE_BIND_METHOD(Tabs,get_tab_rect);
    SE_BIND_METHOD(Tabs,move_tab);
    SE_BIND_METHOD(Tabs,set_tab_close_display_policy);
    SE_BIND_METHOD(Tabs,get_tab_close_display_policy);
    SE_BIND_METHOD(Tabs,set_scrolling_enabled);
    SE_BIND_METHOD(Tabs,get_scrolling_enabled);
    SE_BIND_METHOD(Tabs,set_drag_to_rearrange_enabled);
    SE_BIND_METHOD(Tabs,get_drag_to_rearrange_enabled);
    SE_BIND_METHOD(Tabs,set_tabs_rearrange_group);
    SE_BIND_METHOD(Tabs,get_tabs_rearrange_group);

    SE_BIND_METHOD(Tabs,set_select_with_rmb);
    SE_BIND_METHOD(Tabs,get_select_with_rmb);

    ADD_SIGNAL(MethodInfo("tab_changed", PropertyInfo(VariantType::INT, "tab")));
    ADD_SIGNAL(MethodInfo("right_button_pressed", PropertyInfo(VariantType::INT, "tab")));
    ADD_SIGNAL(MethodInfo("tab_close", PropertyInfo(VariantType::INT, "tab")));
    ADD_SIGNAL(MethodInfo("tab_hover", PropertyInfo(VariantType::INT, "tab")));
    ADD_SIGNAL(MethodInfo("reposition_active_tab_request", PropertyInfo(VariantType::INT, "idx_to")));
    ADD_SIGNAL(MethodInfo("tab_clicked", PropertyInfo(VariantType::INT, "tab")));

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "current_tab", PropertyHint::Range, "-1,4096,1", PROPERTY_USAGE_EDITOR), "set_current_tab", "get_current_tab");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "tab_align", PropertyHint::Enum, "Left,Center,Right"), "set_tab_align", "get_tab_align");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "tab_close_display_policy", PropertyHint::Enum, "Show Never,Show Active Only,Show Always"), "set_tab_close_display_policy", "get_tab_close_display_policy");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "scrolling_enabled"), "set_scrolling_enabled", "get_scrolling_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "drag_to_rearrange_enabled"), "set_drag_to_rearrange_enabled", "get_drag_to_rearrange_enabled");

    BIND_ENUM_CONSTANT(ALIGN_LEFT);
    BIND_ENUM_CONSTANT(ALIGN_CENTER);
    BIND_ENUM_CONSTANT(ALIGN_RIGHT);
    BIND_ENUM_CONSTANT(ALIGN_MAX);

    BIND_ENUM_CONSTANT(CLOSE_BUTTON_SHOW_NEVER);
    BIND_ENUM_CONSTANT(CLOSE_BUTTON_SHOW_ACTIVE_ONLY);
    BIND_ENUM_CONSTANT(CLOSE_BUTTON_SHOW_ALWAYS);
    BIND_ENUM_CONSTANT(CLOSE_BUTTON_MAX);
}

Tabs::Tabs() {

    current = 0;
    previous = 0;
    tab_align = ALIGN_CENTER;
    rb_hover = -1;
    rb_pressing = false;
    highlight_arrow = -1;

    cb_hover = -1;
    cb_pressing = false;
    cb_displaypolicy = CLOSE_BUTTON_SHOW_NEVER;
    offset = 0;
    max_drawn_tab = 0;

    select_with_rmb = false;

    min_width = 0;
    scrolling_enabled = true;
    buttons_visible = false;
    hover = -1;
    drag_to_rearrange_enabled = false;
    tabs_rearrange_group = -1;
    connect("mouse_exited",callable_mp(this, &ClassName::_on_mouse_exited));
}
