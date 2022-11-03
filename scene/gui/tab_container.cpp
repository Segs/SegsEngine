/*************************************************************************/
/*  tab_container.cpp                                                    */
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

#include "tab_container.h"

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

IMPL_GDCLASS(TabContainer)
VARIANT_ENUM_CAST(TabContainer::TabAlign);

int TabContainer::_get_top_margin() const {

    if (!tabs_visible)
        return 0;

    // Respect the minimum tab height.
    Ref<StyleBox> tab_bg = get_theme_stylebox("tab_bg");
    Ref<StyleBox> tab_fg = get_theme_stylebox("tab_fg");
    Ref<StyleBox> tab_disabled = get_theme_stylebox("tab_disabled");

    int tab_height = M_MAX(M_MAX(tab_bg->get_minimum_size().height, tab_fg->get_minimum_size().height), tab_disabled->get_minimum_size().height);

    // Font height or higher icon wins.
    Ref<Font> font = get_theme_font("font");
    int content_height = font->get_height();

    Vector<Control *> tabs = _get_tabs();
    for (Control *c : tabs) {

        if (!c->has_meta("_tab_icon"))
            continue;

        Ref<Texture> tex = refFromVariant<Texture>(c->get_meta("_tab_icon"));
        if (not tex)
            continue;
        content_height = M_MAX(content_height, tex->get_size().height);
    }

    return tab_height + content_height;
}

void TabContainer::_gui_input(const Ref<InputEvent> &p_event) {

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);

    if (mb && mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {

        Point2 pos(mb->get_position().x, mb->get_position().y);
        Size2 size = get_size();

        // Click must be on tabs in the tab header area.
        if (pos.x < tabs_ofs_cache || pos.y > _get_top_margin())
            return;

        // Handle menu button.
        Ref<Texture> menu = get_theme_icon("menu");
        if (popup && pos.x > size.width - menu->get_width()) {
            emit_signal("pre_popup_pressed");

            Vector2 popup_pos = get_global_position();
            popup_pos.x += size.width * get_global_transform().get_scale().x - popup->get_size().width * popup->get_global_transform().get_scale().x;
            popup_pos.y += menu->get_height() * get_global_transform().get_scale().y;

            popup->set_global_position(popup_pos);
            popup->popup();
            return;
        }

        // Do not activate tabs when tabs is empty.
        if (get_tab_count() == 0)
            return;


        // Handle navigation buttons.
        if (buttons_visible_cache) {
            int popup_ofs = 0;
            if (popup) {
                popup_ofs = menu->get_width();
            }

            Ref<Texture> increment = get_theme_icon("increment");
            Ref<Texture> decrement = get_theme_icon("decrement");
            if (pos.x > size.width - increment->get_width() - popup_ofs) {

                Vector<Control *> tabs = _get_tabs();

                if (last_tab_cache < tabs.size() - 1) {
                    first_tab_cache += 1;
                    update();
                }
                return;
            } else if (pos.x > size.width - increment->get_width() - decrement->get_width() - popup_ofs) {
                if (first_tab_cache > 0) {
                    first_tab_cache -= 1;
                    update();
                }
                return;
            }
        }

        // Activate the clicked tab.
        pos.x -= tabs_ofs_cache;
        for (int i = first_tab_cache; i <= last_tab_cache; i++) {
            if (get_tab_hidden(i)) {
                continue;
            }
            int tab_width = _get_tab_width(i);
            if (pos.x < tab_width) {
                if (!get_tab_disabled(i)) {
                    set_current_tab(i);
                }
                break;
            }
            pos.x -= tab_width;
        }
    }
    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);

    if (mm) {

        Point2 pos(mm->get_position().x, mm->get_position().y);
        Size2 size = get_size();

        // Mouse must be on tabs in the tab header area.
        if (pos.x < tabs_ofs_cache || pos.y > _get_top_margin()) {

            if (menu_hovered || highlight_arrow > -1) {
                menu_hovered = false;
                highlight_arrow = -1;
                update();
            }
            return;
        }

        Ref<Texture> menu = get_theme_icon("menu");
        if (popup) {

            if (pos.x >= size.width - menu->get_width()) {
                if (!menu_hovered) {
                    menu_hovered = true;
                    highlight_arrow = -1;
                    update();
                    return;
                }
            } else if (menu_hovered) {
                menu_hovered = false;
                update();
            }

            if (menu_hovered) {
                return;
            }
        }

        // Do not activate tabs when tabs is empty.
        if ((get_tab_count() == 0 || !buttons_visible_cache) && menu_hovered) {
            highlight_arrow = -1;
            update();
            return;
        }

        int popup_ofs = 0;
        if (popup) {
            popup_ofs = menu->get_width();
        }

        Ref<Texture> increment = get_theme_icon("increment");
        Ref<Texture> decrement = get_theme_icon("decrement");
        if (pos.x >= size.width - increment->get_width() - popup_ofs) {

            if (highlight_arrow != 1) {
                highlight_arrow = 1;
                update();
            }
        } else if (pos.x >= size.width - increment->get_width() - decrement->get_width() - popup_ofs) {

            if (highlight_arrow != 0) {
                highlight_arrow = 0;
                update();
            }
        } else if (highlight_arrow > -1) {
            highlight_arrow = -1;
            update();
        }
    }
}

void TabContainer::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_TRANSLATION_CHANGED: {

            minimum_size_changed();
            update();
        } break;
        case NOTIFICATION_RESIZED: {

            int side_margin = get_theme_constant("side_margin");
            Ref<Texture> menu = get_theme_icon("menu");
            Ref<Texture> increment = get_theme_icon("increment");
            Ref<Texture> decrement = get_theme_icon("decrement");
            int header_width = get_size().width - side_margin * 2;

            // Find the width of the header area.
            if (popup)
                header_width -= menu->get_width();
            if (buttons_visible_cache)
                header_width -= increment->get_width() + decrement->get_width();
            if (popup || buttons_visible_cache)
                header_width += side_margin;

            // Find the width of all tabs after first_tab_cache.
            int all_tabs_width = 0;
            Vector<Control *> tabs = _get_tabs();
            for (int i = first_tab_cache; i < tabs.size(); i++) {
                int tab_width = _get_tab_width(i);
                all_tabs_width += tab_width;
            }

            // Check if tabs before first_tab_cache would fit into the header area.
            for (int i = first_tab_cache - 1; i >= 0; i--) {
                int tab_width = _get_tab_width(i);

                if (all_tabs_width + tab_width > header_width)
                    break;

                all_tabs_width += tab_width;
                first_tab_cache--;
            }
        } break;
        case NOTIFICATION_DRAW: {

            RenderingEntity canvas = get_canvas_item();
            Size2 size = get_size();

            // Draw only the tab area if the header is hidden.
            Ref<StyleBox> panel = get_theme_stylebox("panel");
            if (!tabs_visible) {
                panel->draw(canvas, Rect2(0, 0, size.width, size.height));
                return;
            }

            Ref<StyleBox> tab_bg = get_theme_stylebox("tab_bg");
            Ref<StyleBox> tab_fg = get_theme_stylebox("tab_fg");
            Ref<StyleBox> tab_disabled = get_theme_stylebox("tab_disabled");
            Ref<Texture> increment = get_theme_icon("increment");
            Ref<Texture> increment_hl = get_theme_icon("increment_highlight");
            Ref<Texture> decrement = get_theme_icon("decrement");
            Ref<Texture> decrement_hl = get_theme_icon("decrement_highlight");
            Ref<Texture> menu = get_theme_icon("menu");
            Ref<Texture> menu_hl = get_theme_icon("menu_highlight");
            Ref<Font> font = get_theme_font("font");
            Color font_color_fg = get_theme_color("font_color_fg");
            Color font_color_bg = get_theme_color("font_color_bg");
            Color font_color_disabled = get_theme_color("font_color_disabled");
            int side_margin = get_theme_constant("side_margin");

            // Find out start and width of the header area.
            int header_x = side_margin;
            int header_width = size.width - side_margin * 2;
            int header_height = _get_top_margin();
            if (popup)
                header_width -= menu->get_width();

            // Check if all tabs would fit into the header area.
            int all_tabs_width = 0;
            Vector<Control *> tabs = _get_tabs();
            for (int i = 0; i < tabs.size(); i++) {
                if (get_tab_hidden(i)) {
                    continue;
                }
                int tab_width = _get_tab_width(i);
                all_tabs_width += tab_width;

                if (all_tabs_width > header_width) {
                    // Not all tabs are visible at the same time - reserve space for navigation buttons.
                    buttons_visible_cache = true;
                    header_width -= decrement->get_width() + increment->get_width();
                    break;
                } else {
                    buttons_visible_cache = false;
                }
            }
            // With buttons, a right side margin does not need to be respected.
            if (popup || buttons_visible_cache) {
                header_width += side_margin;
            }

            if (!buttons_visible_cache) {
                first_tab_cache = 0;
            }

            // Go through the visible tabs to find the width they occupy.
            all_tabs_width = 0;
            FixedVector<int,256,true> tab_widths;
            for (int i = first_tab_cache; i < tabs.size(); i++) {
                if (get_tab_hidden(i)) {
                    tab_widths.emplace_back(0);
                    continue;
                }
                int tab_width = _get_tab_width(i);
                if (all_tabs_width + tab_width > header_width && !tab_widths.empty())
                    break;
                all_tabs_width += tab_width;
                tab_widths.push_back(tab_width);
            }

            // Find the offset at which to draw tabs, according to the alignment.
            switch (align) {
                case ALIGN_LEFT:
                    tabs_ofs_cache = header_x;
                    break;
                case ALIGN_CENTER:
                    tabs_ofs_cache = header_x + (header_width / 2) - (all_tabs_width / 2);
                    break;
                case ALIGN_RIGHT:
                    tabs_ofs_cache = header_x + header_width - all_tabs_width;
                    break;
            }

            if (all_tabs_in_front) {
                // Draw the tab area.
                panel->draw(canvas, Rect2(0, header_height, size.width, size.height - header_height));
            }

            // Draw unselected tabs in back
            int x = 0;
            int x_current = 0;
            int index = 0;
            for (int i = 0; i < tab_widths.size(); i++) {
                index = i + first_tab_cache;
                if (get_tab_hidden(index)) {
                    continue;
                }

                int tab_width = tab_widths[i];
                if (index == current) {
                    x_current = x;
                } else if (get_tab_disabled(index)) {
                    _draw_tab(tab_disabled, font_color_disabled, index, tabs_ofs_cache + x);
                } else {
                    _draw_tab(tab_bg, font_color_bg, index, tabs_ofs_cache + x);
                }

                x += tab_width;
                last_tab_cache = index;
            }

            if (!all_tabs_in_front) {
                // Draw the tab area.
                panel->draw(canvas, Rect2(0, header_height, size.width, size.height - header_height));
            }

            // Draw selected tab in front. Only draw selected tab when it's in visible range.
            if (tabs.size() > 0 && current - first_tab_cache < tab_widths.size() && current >= first_tab_cache) {
                Ref<StyleBox> current_style_box = get_tab_disabled(current) ? tab_disabled : tab_fg;
                _draw_tab(current_style_box, font_color_fg, current, tabs_ofs_cache + x_current);
            }
            // Draw the popup menu.
            x = get_size().width;
            if (popup) {
                x -= menu->get_width();
                if (menu_hovered)
                    menu_hl->draw(get_canvas_item(), Size2(x, (header_height - menu_hl->get_height()) / 2));
                else
                    menu->draw(get_canvas_item(), Size2(x, (header_height - menu->get_height()) / 2));
            }

            // Draw the navigation buttons.
            if (buttons_visible_cache) {

                x -= increment->get_width();
                if (last_tab_cache < tabs.size() - 1) {
                    draw_texture(highlight_arrow == 1 ? increment_hl : increment, Point2(x, (header_height - increment->get_height()) / 2));
                } else {
                    draw_texture(increment, Point2(x, (header_height - increment->get_height()) / 2), Color(1, 1, 1, 0.5f));
                }

                x -= decrement->get_width();
                if (first_tab_cache > 0) {
                    draw_texture(highlight_arrow == 0 ? decrement_hl : decrement, Point2(x, (header_height - decrement->get_height()) / 2));
                } else {
                    draw_texture(decrement, Point2(x, (header_height - decrement->get_height()) / 2), Color(1, 1, 1, 0.5f));
                }
            }
        } break;
        case NOTIFICATION_THEME_CHANGED: {

            minimum_size_changed();
            // Wait until all changed theme.
            call_deferred([this](){ _on_theme_changed();});
        } break;
    }
}

void TabContainer::_draw_tab(Ref<StyleBox> &p_tab_style, Color &p_font_color, int p_index, float p_x) {
    Control *control = get_tab_control(p_index);
    RenderingEntity canvas = get_canvas_item();
    Ref<Font> font = get_theme_font("font");
    int icon_text_distance = get_theme_constant("hseparation");
    int tab_width = _get_tab_width(p_index);
    int header_height = _get_top_margin();

    // Draw the tab background.
    Rect2 tab_rect(p_x, 0, tab_width, header_height);
    p_tab_style->draw(canvas, tab_rect);

    // Draw the tab contents.
    String text = control->has_meta("_tab_name") ? String(tr(String(control->get_meta("_tab_name")))) : String(tr(control->get_name()));

    int x_content = tab_rect.position.x + p_tab_style->get_margin(Margin::Left);
    int top_margin = p_tab_style->get_margin(Margin::Top);
    int y_center = top_margin + (tab_rect.size.y - p_tab_style->get_minimum_size().y) / 2;

    // Draw the tab icon.
    if (control->has_meta("_tab_icon")) {
        Ref<Texture> icon = control->get_meta("_tab_icon");
        if (icon) {
            int y = y_center - (icon->get_height() / 2);
            icon->draw(canvas, Point2i(x_content, y));
            if (text != "") {
                x_content += icon->get_width() + icon_text_distance;
            }
        }
    }

    // Draw the tab text.
    Point2i text_pos(x_content, y_center - (font->get_height() / 2) + font->get_ascent());
    font->draw(canvas, text_pos, text, p_font_color);
}

void TabContainer::_on_theme_changed() {
    if (get_tab_count() > 0) {
        _repaint();
        update();
    }
}
void TabContainer::_on_mouse_exited() {
    if (menu_hovered || highlight_arrow > -1) {
        menu_hovered = false;
        highlight_arrow = -1;
        update();
    }
}
int TabContainer::_get_tab_width(int p_index) const {

    ERR_FAIL_INDEX_V(p_index, get_tab_count(), 0);
    Control *control = get_tab_control(p_index);
    if (!control || get_tab_hidden(p_index)) {
        return 0;
    }

    // Get the width of the text displayed on the tab.
    Ref<Font> font = get_theme_font("font");
    StringName text = control->has_meta("_tab_name") ? tr(control->get_meta("_tab_name").as<StringName>()) : tr(control->get_name());
    int width = font->get_string_size(text).width;

    // Add space for a tab icon.
    if (control->has_meta("_tab_icon")) {
        Ref<Texture> icon = refFromVariant<Texture>(control->get_meta("_tab_icon"));
        if (icon) {
            width += icon->get_width();
            if (!text.empty())
                width += get_theme_constant("hseparation");
        }
    }

    // Respect a minimum size.
    Ref<StyleBox> tab_bg = get_theme_stylebox("tab_bg");
    Ref<StyleBox> tab_fg = get_theme_stylebox("tab_fg");
    Ref<StyleBox> tab_disabled = get_theme_stylebox("tab_disabled");
    if (get_tab_disabled(p_index)) {
        width += tab_disabled->get_minimum_size().width;
    } else if (p_index == current) {
        width += tab_fg->get_minimum_size().width;
    } else {
        width += tab_bg->get_minimum_size().width;
    }

    return width;
}

Vector<Control *> TabContainer::_get_tabs() const {

    Vector<Control *> controls;

    for (int i = 0; i < get_child_count(); i++) {

        Control *control = object_cast<Control>(get_child(i));
        if (!control || control->is_toplevel_control())
            continue;

        controls.push_back(control);
    }
    return controls;
}

void TabContainer::_child_renamed_callback() {

    update();
}

void TabContainer::add_child_notify(Node *p_child) {

    Container::add_child_notify(p_child);

    Control *c = object_cast<Control>(p_child);
    if (!c || c->is_set_as_top_level()) {
        return;
    }

    call_deferred([this]() { _repaint(); });
    update();

    bool first = (get_tab_count() == 1);

    if (first) {
        current = 0;
        previous = 0;
    }

    p_child->connect("renamed",callable_mp(this, &ClassName::_child_renamed_callback));
    if (first && is_inside_tree()) {
        emit_signal("tab_changed", current);
    }
}

void TabContainer::move_child_notify(Node *p_child) {
    Container::move_child_notify(p_child);

    Control *c = object_cast<Control>(p_child);
    if (!c || c->is_set_as_top_level()) {
        return;
}

    _update_current_tab();
    update();
}


int TabContainer::get_tab_count() const {

    return _get_tabs().size();
}

void TabContainer::_repaint()
{
    Ref<StyleBox> sb = get_theme_stylebox("panel");
    Vector<Control *> tabs = _get_tabs();
    for (int i = 0; i < tabs.size(); i++) {

        Control *c = tabs[i];
        if (i == current) {
            c->show();
            c->set_anchors_and_margins_preset(Control::PRESET_WIDE);
            if (tabs_visible)
                c->set_margin(Margin::Top, _get_top_margin());
            c->set_margin(Margin(Margin::Top), c->get_margin(Margin(Margin::Top)) + sb->get_margin(Margin(Margin::Top)));
            c->set_margin(Margin(Margin::Left), c->get_margin(Margin(Margin::Left)) + sb->get_margin(Margin(Margin::Left)));
            c->set_margin(Margin(Margin::Right), c->get_margin(Margin(Margin::Right)) - sb->get_margin(Margin(Margin::Right)));
            c->set_margin(Margin(Margin::Bottom), c->get_margin(Margin(Margin::Bottom)) - sb->get_margin(Margin(Margin::Bottom)));

        } else
            c->hide();
    }
}

void TabContainer::set_current_tab(int p_current) {

    ERR_FAIL_INDEX(p_current, get_tab_count());

    int pending_previous = current;
    current = p_current;

    _repaint();

    Object_change_notify(this,"current_tab");

    if (pending_previous == current)
        emit_signal("tab_selected", current);
    else {
        previous = pending_previous;
        emit_signal("tab_selected", current);
        emit_signal("tab_changed", current);
    }

    update();
}

int TabContainer::get_current_tab() const {

    return current;
}

int TabContainer::get_previous_tab() const {

    return previous;
}

Control *TabContainer::get_tab_control(int p_idx) const {

    Vector<Control *> tabs = _get_tabs();
    if (p_idx >= 0 && p_idx < tabs.size())
        return tabs[p_idx];
    else
        return nullptr;
}

Control *TabContainer::get_current_tab_control() const {
    return get_tab_control(current);
}

void TabContainer::remove_child_notify(Node *p_child) {

    Container::remove_child_notify(p_child);

    Control *c = object_cast<Control>(p_child);
    if (!c || c->is_set_as_top_level()) {
        return;
    }

    // Defer the call because tab is not yet removed (remove_child_notify is called right before p_child is actually removed).
    call_deferred([this]() {_update_current_tab(); });

    p_child->disconnect("renamed",callable_mp(this, &ClassName::_child_renamed_callback));

    update();
}

void TabContainer::_update_current_tab() {

    int tc = get_tab_count();
    if (current >= tc)
        current = tc - 1;
    if (current < 0)
        current = 0;
    else
        set_current_tab(current);
}

Variant TabContainer::get_drag_data(const Point2 &p_point) {

    if (!drag_to_rearrange_enabled)
        return Variant();

    int tab_over = get_tab_idx_at_point(p_point);

    if (tab_over < 0)
        return Variant();

    HBoxContainer *drag_preview = memnew(HBoxContainer);

    Ref<Texture> icon = get_tab_icon(tab_over);
    if (icon) {
        TextureRect *tf = memnew(TextureRect);
        tf->set_texture(icon);
        drag_preview->add_child(tf);
    }
    Label *label = memnew(Label(get_tab_title(tab_over)));
    drag_preview->add_child(label);
    set_drag_preview(drag_preview);

    Dictionary drag_data;
    drag_data["type"] = "tabc_element";
    drag_data["tabc_element"] = tab_over;
    drag_data["from_path"] = get_path();
    return drag_data;
}

bool TabContainer::can_drop_data(const Point2 &p_point, const Variant &p_data) const {

    if (!drag_to_rearrange_enabled)
        return false;

    Dictionary d = p_data.as<Dictionary>();
    if (!d.has("type"))
        return false;

    if (d["type"] == "tabc_element") {

        NodePath from_path = d["from_path"].as<NodePath>();
        NodePath to_path = get_path();
        if (from_path == to_path) {
            return true;
        } else if (get_tabs_rearrange_group() != -1) {
            // drag and drop between other TabContainers
            Node *from_node = get_node(from_path);
            TabContainer *from_tabc = object_cast<TabContainer>(from_node);
            if (from_tabc && from_tabc->get_tabs_rearrange_group() == get_tabs_rearrange_group()) {
                return true;
            }
        }
    }
    return false;
}

void TabContainer::drop_data(const Point2 &p_point, const Variant &p_data) {

    if (!drag_to_rearrange_enabled)
        return;

    int hover_now = get_tab_idx_at_point(p_point);

    Dictionary d = p_data.as<Dictionary>();
    if (!d.has("type"))
        return;

    if (d["type"] == "tabc_element") {

        int tab_from_id = d["tabc_element"].as<int>();
        NodePath from_path = d["from_path"].as<NodePath>();
        NodePath to_path = get_path();
        if (from_path == to_path) {
            if (hover_now < 0)
                hover_now = get_tab_count() - 1;
            move_child(get_tab_control(tab_from_id), get_tab_control(hover_now)->get_index());
            set_current_tab(hover_now);
        } else if (get_tabs_rearrange_group() != -1) {
            // drag and drop between TabContainers
            Node *from_node = get_node(from_path);
            TabContainer *from_tabc = object_cast<TabContainer>(from_node);
            if (from_tabc && from_tabc->get_tabs_rearrange_group() == get_tabs_rearrange_group()) {
                Control *moving_tabc = from_tabc->get_tab_control(tab_from_id);
                from_tabc->remove_child(moving_tabc);
                add_child(moving_tabc);
                if (hover_now < 0)
                    hover_now = get_tab_count() - 1;
                move_child(moving_tabc, get_tab_control(hover_now)->get_index());
                set_current_tab(hover_now);
                emit_signal("tab_changed", hover_now);
            }
        }
    }
    update();
}

int TabContainer::get_tab_idx_at_point(const Point2 &p_point) const {

    if (get_tab_count() == 0)
        return -1;

    // must be on tabs in the tab header area.
    if (p_point.x < tabs_ofs_cache || p_point.y > _get_top_margin())
        return -1;

    Size2 size = get_size();
    int right_ofs = 0;

    if (popup) {
        Ref<Texture> menu = get_theme_icon("menu");
        right_ofs += menu->get_width();
    }
    if (buttons_visible_cache) {
        Ref<Texture> increment = get_theme_icon("increment");
        Ref<Texture> decrement = get_theme_icon("decrement");
        right_ofs += increment->get_width() + decrement->get_width();
    }
    if (p_point.x > size.width - right_ofs) {
        return -1;
    }

    // get the tab at the point
    //Vector<Control *> tabs = _get_tabs();
    int px = p_point.x;
    px -= tabs_ofs_cache;
    for (int i = first_tab_cache; i <= last_tab_cache; i++) {
        int tab_width = _get_tab_width(i);
        if (px < tab_width) {
            return i;
        }
        px -= tab_width;
    }
    return -1;
}

void TabContainer::set_tab_align(TabAlign p_align) {

    ERR_FAIL_INDEX(p_align, 3);
    align = p_align;
    update();

    Object_change_notify(this,"tab_align");
}

TabContainer::TabAlign TabContainer::get_tab_align() const {

    return align;
}

void TabContainer::set_tabs_visible(bool p_visible) {

    if (p_visible == tabs_visible)
        return;

    tabs_visible = p_visible;

    Vector<Control *> tabs = _get_tabs();
    for (Control *c : tabs) {

        if (p_visible)
            c->set_margin(Margin::Top, _get_top_margin());
        else
            c->set_margin(Margin::Top, 0);
    }
    update();
    minimum_size_changed();
}

bool TabContainer::are_tabs_visible() const {

    return tabs_visible;
}

void TabContainer::set_all_tabs_in_front(bool p_in_front) {
    if (p_in_front == all_tabs_in_front) {
        return;
    }

    all_tabs_in_front = p_in_front;

    update();
}

bool TabContainer::is_all_tabs_in_front() const {
    return all_tabs_in_front;
}

void TabContainer::set_tab_title(int p_tab, const StringName &p_title) {

    Control *child = get_tab_control(p_tab);
    ERR_FAIL_COND(!child);
    child->set_meta("_tab_name", p_title);
    update();
}

StringName TabContainer::get_tab_title(int p_tab) const {

    Control *child = get_tab_control(p_tab);
    ERR_FAIL_COND_V(!child, StringName());
    if (child->has_meta("_tab_name"))
        return child->get_meta("_tab_name").as<StringName>();
    else
        return child->get_name();
}

void TabContainer::set_tab_icon(int p_tab, const Ref<Texture> &p_icon) {

    Control *child = get_tab_control(p_tab);
    ERR_FAIL_COND(!child);
    child->set_meta("_tab_icon", p_icon);
    update();
}
Ref<Texture> TabContainer::get_tab_icon(int p_tab) const {

    Control *child = get_tab_control(p_tab);
    ERR_FAIL_COND_V(!child, Ref<Texture>());
    if (child->has_meta("_tab_icon"))
        return refFromVariant<Texture>(child->get_meta("_tab_icon"));
    else
        return Ref<Texture>();
}

void TabContainer::set_tab_disabled(int p_tab, bool p_disabled) {

    Control *child = get_tab_control(p_tab);
    ERR_FAIL_COND(!child);
    child->set_meta("_tab_disabled", p_disabled);
    update();
}

bool TabContainer::get_tab_disabled(int p_tab) const {

    Control *child = get_tab_control(p_tab);
    ERR_FAIL_COND_V(!child, false);
    if (child->has_meta("_tab_disabled"))
        return child->get_meta("_tab_disabled").as<bool>();
    else
        return false;
}

void TabContainer::set_tab_hidden(int p_tab, bool p_hidden) {

    Control *child = get_tab_control(p_tab);
    ERR_FAIL_COND(!child);
    child->set_meta("_tab_hidden", p_hidden);
    update();
    for (int i = 0; i < get_tab_count(); i++) {
        int try_tab = (p_tab + 1 + i) % get_tab_count();
        if (get_tab_disabled(try_tab) || get_tab_hidden(try_tab)) {
            continue;
        }

        set_current_tab(try_tab);
        return;
    }

    //assumed no other tab can be switched to, just hide
    child->hide();
}

bool TabContainer::get_tab_hidden(int p_tab) const {

    Control *child = get_tab_control(p_tab);
    ERR_FAIL_COND_V(!child, false);
    if (child->has_meta("_tab_hidden"))
        return child->get_meta("_tab_hidden").as<bool>();

    return false;
}

void TabContainer::get_translatable_strings(List<String> *p_strings) const {

    Vector<Control *> tabs = _get_tabs();
    for (Control *c : tabs) {

        if (!c->has_meta("_tab_name"))
            continue;

        String name = c->get_meta("_tab_name").as<String>();

        if (!name.empty())
            p_strings->push_back(name);
    }
}

Size2 TabContainer::get_minimum_size() const {

    Size2 ms;

    Vector<Control *> tabs = _get_tabs();
    for (Control *c : tabs) {

        if (!c->is_visible_in_tree() && !use_hidden_tabs_for_min_size)
            continue;

        Size2 cms = c->get_combined_minimum_size();
        ms.x = M_MAX(ms.x, cms.x);
        ms.y = M_MAX(ms.y, cms.y);
    }

    Ref<StyleBox> tab_bg = get_theme_stylebox("tab_bg");
    Ref<StyleBox> tab_fg = get_theme_stylebox("tab_fg");
    Ref<StyleBox> tab_disabled = get_theme_stylebox("tab_disabled");
    Ref<Font> font = get_theme_font("font");

    if (tabs_visible) {
        ms.y += M_MAX(M_MAX(tab_bg->get_minimum_size().y, tab_fg->get_minimum_size().y), tab_disabled->get_minimum_size().y);
        ms.y += font->get_height();
    }

    Ref<StyleBox> sb = get_theme_stylebox("panel");
    ms += sb->get_minimum_size();

    return ms;
}

void TabContainer::set_popup(Node *p_popup) {
    ERR_FAIL_NULL(p_popup);
    popup = object_cast<Popup>(p_popup);
    update();
}

Popup *TabContainer::get_popup() const {
    return popup;
}

void TabContainer::set_drag_to_rearrange_enabled(bool p_enabled) {
    drag_to_rearrange_enabled = p_enabled;
}

bool TabContainer::get_drag_to_rearrange_enabled() const {
    return drag_to_rearrange_enabled;
}
void TabContainer::set_tabs_rearrange_group(int p_group_id) {
    tabs_rearrange_group = p_group_id;
}

int TabContainer::get_tabs_rearrange_group() const {
    return tabs_rearrange_group;
}

void TabContainer::set_use_hidden_tabs_for_min_size(bool p_use_hidden_tabs) {
    use_hidden_tabs_for_min_size = p_use_hidden_tabs;
}

bool TabContainer::get_use_hidden_tabs_for_min_size() const {
    return use_hidden_tabs_for_min_size;
}
void TabContainer::_bind_methods() {

    SE_BIND_METHOD(TabContainer,_gui_input);
    SE_BIND_METHOD(TabContainer,get_tab_count);
    SE_BIND_METHOD(TabContainer,set_current_tab);
    SE_BIND_METHOD(TabContainer,get_current_tab);
    SE_BIND_METHOD(TabContainer,get_previous_tab);
    SE_BIND_METHOD(TabContainer,get_current_tab_control);
    SE_BIND_METHOD(TabContainer,get_tab_control);
    SE_BIND_METHOD(TabContainer,set_tab_align);
    SE_BIND_METHOD(TabContainer,get_tab_align);
    SE_BIND_METHOD(TabContainer,set_tabs_visible);
    SE_BIND_METHOD(TabContainer,are_tabs_visible);
    SE_BIND_METHOD(TabContainer,set_all_tabs_in_front);
    SE_BIND_METHOD(TabContainer,is_all_tabs_in_front);

    SE_BIND_METHOD(TabContainer,set_tab_title);
    SE_BIND_METHOD(TabContainer,get_tab_title);
    SE_BIND_METHOD(TabContainer,set_tab_icon);
    SE_BIND_METHOD(TabContainer,get_tab_icon);
    SE_BIND_METHOD(TabContainer,set_tab_disabled);
    SE_BIND_METHOD(TabContainer,get_tab_disabled);
    SE_BIND_METHOD(TabContainer,set_tab_hidden);
    SE_BIND_METHOD(TabContainer,get_tab_hidden);
    SE_BIND_METHOD(TabContainer,get_tab_idx_at_point);
    SE_BIND_METHOD(TabContainer,set_popup);
    SE_BIND_METHOD(TabContainer,get_popup);
    SE_BIND_METHOD(TabContainer,set_drag_to_rearrange_enabled);
    SE_BIND_METHOD(TabContainer,get_drag_to_rearrange_enabled);
    SE_BIND_METHOD(TabContainer,set_tabs_rearrange_group);
    SE_BIND_METHOD(TabContainer,get_tabs_rearrange_group);

    SE_BIND_METHOD(TabContainer,set_use_hidden_tabs_for_min_size);
    SE_BIND_METHOD(TabContainer,get_use_hidden_tabs_for_min_size);

    ADD_SIGNAL(MethodInfo("tab_changed", PropertyInfo(VariantType::INT, "tab")));
    ADD_SIGNAL(MethodInfo("tab_selected", PropertyInfo(VariantType::INT, "tab")));
    ADD_SIGNAL(MethodInfo("pre_popup_pressed"));

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "tab_align", PropertyHint::Enum, "Left,Center,Right"), "set_tab_align", "get_tab_align");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "current_tab", PropertyHint::Range, "-1,4096,1", PROPERTY_USAGE_EDITOR), "set_current_tab", "get_current_tab");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "tabs_visible"), "set_tabs_visible", "are_tabs_visible");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "all_tabs_in_front"), "set_all_tabs_in_front", "is_all_tabs_in_front");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "drag_to_rearrange_enabled"), "set_drag_to_rearrange_enabled", "get_drag_to_rearrange_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_hidden_tabs_for_min_size"), "set_use_hidden_tabs_for_min_size", "get_use_hidden_tabs_for_min_size");

    BIND_ENUM_CONSTANT(ALIGN_LEFT);
    BIND_ENUM_CONSTANT(ALIGN_CENTER);
    BIND_ENUM_CONSTANT(ALIGN_RIGHT);
}

TabContainer::TabContainer() {

    first_tab_cache = 0;
    last_tab_cache = 0;
    buttons_visible_cache = false;
    menu_hovered = false;
    highlight_arrow = -1;
    tabs_ofs_cache = 0;
    current = 0;
    previous = 0;
    align = ALIGN_CENTER;
    tabs_visible = true;
    popup = nullptr;
    all_tabs_in_front = false;
    drag_to_rearrange_enabled = false;
    tabs_rearrange_group = -1;
    use_hidden_tabs_for_min_size = false;

    connect("mouse_exited",callable_mp(this, &ClassName::_on_mouse_exited));
}
