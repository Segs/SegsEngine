/*************************************************************************/
/*  graph_node.cpp                                                       */
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

#include "graph_node.h"

#include "core/input/input_event.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/string_formatter.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"


IMPL_GDCLASS(GraphNode)
VARIANT_ENUM_CAST(GraphNode::Overlay);

struct _MinSizeCache {
    int min_size;
    bool will_stretch;
    int final_size;
};
bool GraphNode::_set(const StringName &p_name, const Variant &p_value) {

    if (!StringUtils::begins_with(p_name,"slot/"))
        return false;

    int idx = StringUtils::to_int(StringUtils::get_slice(p_name,"/", 1));
    StringView what = StringUtils::get_slice(p_name,"/", 2);

    Slot si;
    if (slot_info.contains(idx))
        si = slot_info[idx];

    if (what == StringView("left_enabled"))
        si.enable_left = p_value.as<bool>();
    else if (what == StringView("left_type"))
        si.type_left = p_value.as<int>();
    else if (what == StringView("left_color"))
        si.color_left = p_value.as<Color>();
    else if (what == StringView("right_enabled"))
        si.enable_right = p_value.as<bool>();
    else if (what == StringView("right_type"))
        si.type_right = p_value.as<int>();
    else if (what == StringView("right_color"))
        si.color_right = p_value.as<Color>();
    else
        return false;

    set_slot(idx, si.enable_left, si.type_left, si.color_left, si.enable_right, si.type_right, si.color_right);
    update();
    return true;
}

bool GraphNode::_get(const StringName &p_name, Variant &r_ret) const {
    using namespace eastl;

    if (!StringUtils::begins_with(p_name,"slot/")) {
        return false;
    }

    int idx = StringUtils::to_int(StringUtils::get_slice(p_name,"/", 1));
    StringView what = StringUtils::get_slice(p_name,"/", 2);

    Slot si=slot_info.at(idx,{});

    if (what == "left_enabled"_sv)
        r_ret = si.enable_left;
    else if (what == "left_type"_sv)
        r_ret = si.type_left;
    else if (what == "left_color"_sv)
        r_ret = si.color_left;
    else if (what == "right_enabled"_sv)
        r_ret = si.enable_right;
    else if (what == "right_type"_sv)
        r_ret = si.type_right;
    else if (what == "right_color"_sv)
        r_ret = si.color_right;
    else
        return false;

    return true;
}
void GraphNode::_get_property_list(Vector<PropertyInfo> *p_list) const {

    int idx = 0;
    for (int i = 0; i < get_child_count(); i++) {
        Control *c = object_cast<Control>(get_child(i));
        if (!c || c->is_set_as_top_level())
            continue;

        String base = "slot/" + itos(idx) + "/";

        p_list->push_back(PropertyInfo(VariantType::BOOL, StringName(base + "left_enabled")));
        p_list->push_back(PropertyInfo(VariantType::INT, StringName(base + "left_type")));
        p_list->push_back(PropertyInfo(VariantType::COLOR, StringName(base + "left_color")));
        p_list->push_back(PropertyInfo(VariantType::BOOL, StringName(base + "right_enabled")));
        p_list->push_back(PropertyInfo(VariantType::INT, StringName(base + "right_type")));
        p_list->push_back(PropertyInfo(VariantType::COLOR, StringName(base + "right_color")));

        idx++;
    }
}

void GraphNode::_resort() {
    /** First pass, determine minimum size AND amount of stretchable elements */

    Size2 new_size = get_size();

    Ref<StyleBox> sb = get_theme_stylebox("frame");

    int sep = get_theme_constant("separation");

    int children_count = 0;
    int stretch_min = 0;
    int stretch_avail = 0;
    float stretch_ratio_total = 0;
    Map<Control *, _MinSizeCache> min_size_cache;

    for (int i = 0; i < get_child_count(); i++) {
        Control *c = object_cast<Control>(get_child(i));
        if (!c || !c->is_visible_in_tree()) {
            continue;
        }
        if (c->is_set_as_top_level())
            continue;

        Size2i size = c->get_combined_minimum_size();
        _MinSizeCache msc;

        stretch_min += size.height;
        msc.min_size = size.height;
        msc.will_stretch = c->get_v_size_flags() & SIZE_EXPAND;

        if (msc.will_stretch) {
            stretch_avail += msc.min_size;
            stretch_ratio_total += c->get_stretch_ratio();
        }
        msc.final_size = msc.min_size;
        min_size_cache[c] = msc;
        children_count++;
    }

    if (children_count == 0) {
        return;
    }

    int stretch_max = new_size.height - (children_count - 1) * sep;
    int stretch_diff = stretch_max - stretch_min;
    if (stretch_diff < 0) {
        //avoid negative stretch space
        stretch_diff = 0;
    }

    stretch_avail += stretch_diff - sb->get_margin(Margin::Bottom) - sb->get_margin(Margin::Top); //available stretch space.
    /** Second, pass successively to discard elements that can't be stretched, this will run while stretchable
        elements exist */

    while (stretch_ratio_total > 0) { // first of all, don't even be here if no stretchable objects exist
        bool refit_successful = true; //assume refit-test will go well

        for (int i = 0; i < get_child_count(); i++) {
            Control *c = object_cast<Control>(get_child(i));
            if (!c || !c->is_visible_in_tree()) {
                continue;
            }
            if (c->is_set_as_top_level()) {
                continue;
            }

            ERR_FAIL_COND(!min_size_cache.contains(c));
            _MinSizeCache &msc = min_size_cache[c];

            if (msc.will_stretch) { //wants to stretch
                //let's see if it can really stretch

                int final_pixel_size = stretch_avail * c->get_stretch_ratio() / stretch_ratio_total;
                if (final_pixel_size < msc.min_size) {
                    //if available stretching area is too small for widget,
                    //then remove it from stretching area
                    msc.will_stretch = false;
                    stretch_ratio_total -= c->get_stretch_ratio();
                    refit_successful = false;
                    stretch_avail -= msc.min_size;
                    msc.final_size = msc.min_size;
                    break;
                } else {
                    msc.final_size = final_pixel_size;
                }
            }
        }

        if (refit_successful) { //uf refit went well, break
            break;
        }
    }

    /** Final pass, draw and stretch elements **/

    int ofs = sb->get_margin(Margin::Top);

    bool first = true;
    int idx = 0;
    cache_y.clear();
    int w = new_size.width - sb->get_minimum_size().x;
    for (int i = 0; i < get_child_count(); i++) {
        Control *c = object_cast<Control>(get_child(i));
        if (!c || !c->is_visible_in_tree()) {
            continue;
        }
        if (c->is_set_as_top_level()) {
            continue;
        }

        _MinSizeCache &msc = min_size_cache[c];

        if (first) {
            first = false;
        } else {
            ofs += sep;
        }

        int from = ofs;
        int to = ofs + msc.final_size;

        if (msc.will_stretch && idx == children_count - 1) {
            //adjust so the last one always fits perfect
            //compensating for numerical imprecision

            to = new_size.height - sb->get_margin(Margin::Bottom);
        }

        int size = to - from;

        Rect2 rect(sb->get_margin(Margin::Left), from, w, size);

        fit_child_in_rect(c, rect);
        cache_y.push_back(from - sb->get_margin(Margin::Top) + size * 0.5);

        ofs = to;
        idx++;
    }

    update();
    connpos_dirty = true;
}

bool GraphNode::has_point(const Point2 &p_point) const {

    if (comment) {
        Ref<StyleBox> comment = get_theme_stylebox("comment");
        Ref<Texture> resizer = get_theme_icon("resizer");

        if (Rect2(get_size() - resizer->get_size(), resizer->get_size()).has_point(p_point)) {
            return true;
        }

        if (Rect2(0, 0, get_size().width, comment->get_margin(Margin::Top)).has_point(p_point)) {
            return true;
        }

        return false;

    } else {
        return Control::has_point(p_point);
    }
}

void GraphNode::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_DRAW: {

            Ref<StyleBox> sb;

            if (comment) {
                sb = get_theme_stylebox(selected ? StringName("commentfocus") : StringName("comment"));

            } else {

                sb = get_theme_stylebox(selected ? StringName("selectedframe") : StringName("frame"));
            }

            //sb=sb->duplicate();
            //sb->call("set_modulate",modulate);
            Ref<Texture> port = get_theme_icon("port");
            Ref<Texture> close = get_theme_icon("close");
            Ref<Texture> resizer = get_theme_icon("resizer");
            int close_offset = get_theme_constant("close_offset");
            int close_h_offset = get_theme_constant("close_h_offset");
            Color close_color = get_theme_color("close_color");
            Color resizer_color = get_theme_color("resizer_color");
            Ref<Font> title_font = get_theme_font("title_font");
            int title_offset = get_theme_constant("title_offset");
            int title_h_offset = get_theme_constant("title_h_offset");
            Color title_color = get_theme_color("title_color");
            Point2i icofs = -port->get_size() * 0.5;
            int edgeofs = get_theme_constant("port_offset");
            icofs.y += sb->get_margin(Margin::Top);

            draw_style_box(sb, Rect2(Point2(), get_size()));

            switch (overlay) {
                case OVERLAY_DISABLED: {

                } break;
                case OVERLAY_BREAKPOINT: {

                    draw_style_box(get_theme_stylebox("breakpoint"), Rect2(Point2(), get_size()));
                } break;
                case OVERLAY_POSITION: {
                    draw_style_box(get_theme_stylebox("position"), Rect2(Point2(), get_size()));

                } break;
            }

            int w = get_size().width - sb->get_minimum_size().x;

            if (show_close)
                w -= close->get_width();

            draw_string(title_font,
                    Point2(sb->get_margin(Margin::Left) + title_h_offset,
                            -title_font->get_height() + title_font->get_ascent() + title_offset),
                    title, title_color, w);
            if (show_close) {
                Vector2 cpos = Point2(w + sb->get_margin(Margin::Left) + close_h_offset, -close->get_height() + close_offset);
                draw_texture(close, cpos, close_color);
                close_rect.position = cpos;
                close_rect.size = close->get_size();
            } else {
                close_rect = Rect2();
            }

            for (eastl::pair<const int,Slot> &E : slot_info) {

                if (E.first < 0 || E.first >= cache_y.size())
                    continue;
                if (!slot_info.contains(E.first))
                    continue;
                const Slot &s = slot_info[E.first];
                //left
                if (s.enable_left) {
                    Ref<Texture> p = port;
                    if (s.custom_slot_left) {
                        p = s.custom_slot_left;
                    }
                    p->draw(get_canvas_item(), icofs + Point2(edgeofs, cache_y[E.first]), s.color_left);
                }
                if (s.enable_right) {
                    Ref<Texture> p = port;
                    if (s.custom_slot_right) {
                        p = s.custom_slot_right;
                    }
                    p->draw(get_canvas_item(), icofs + Point2(get_size().x - edgeofs, cache_y[E.first]), s.color_right);
                }
            }

            if (resizable) {
                draw_texture(resizer, get_size() - resizer->get_size(), resizer_color);
            }
        } break;

        case NOTIFICATION_SORT_CHILDREN: {

            _resort();
        } break;

        case NOTIFICATION_THEME_CHANGED: {

            minimum_size_changed();
        } break;
    }
}

void GraphNode::set_slot(int p_idx, bool p_enable_left, int p_type_left, const Color &p_color_left, bool p_enable_right, int p_type_right, const Color &p_color_right, const Ref<Texture> &p_custom_left, const Ref<Texture> &p_custom_right) {
    ERR_FAIL_COND_MSG(p_idx < 0, FormatVE("Cannot set slot with p_idx (%d) lesser than zero.", p_idx));

    ERR_FAIL_COND(p_idx < 0);

    if (!p_enable_left && p_type_left == 0 && p_color_left == Color(1, 1, 1, 1) && !p_enable_right && p_type_right == 0 && p_color_right == Color(1, 1, 1, 1)) {
        slot_info.erase(p_idx);
        return;
    }

    Slot s;
    s.enable_left = p_enable_left;
    s.type_left = p_type_left;
    s.color_left = p_color_left;
    s.enable_right = p_enable_right;
    s.type_right = p_type_right;
    s.color_right = p_color_right;
    s.custom_slot_left = p_custom_left;
    s.custom_slot_right = p_custom_right;
    slot_info[p_idx] = s;
    update();
    connpos_dirty = true;
    emit_signal("slot_updated", p_idx);
}

void GraphNode::clear_slot(int p_idx) {

    slot_info.erase(p_idx);
    update();
    connpos_dirty = true;
}
void GraphNode::clear_all_slots() {

    slot_info.clear();
    update();
    connpos_dirty = true;
}
bool GraphNode::is_slot_enabled_left(int p_idx) const {

    if (!slot_info.contains(p_idx))
        return false;
    return slot_info.at(p_idx).enable_left;
}

void GraphNode::set_slot_enabled_left(int p_idx, bool p_enable_left) {
    ERR_FAIL_COND_MSG(p_idx < 0, FormatVE("Cannot set enable_left for the slot with p_idx (%d) lesser than zero.", p_idx));

    slot_info[p_idx].enable_left = p_enable_left;
    update();
    connpos_dirty = true;

    emit_signal("slot_updated", p_idx);
}

void GraphNode::set_slot_type_left(int p_idx, int p_type_left) {
    ERR_FAIL_COND_MSG(!slot_info.contains(p_idx), FormatVE("Cannot set type_left for the slot '%d' because it hasn't been enabled.", p_idx));

    slot_info[p_idx].type_left = p_type_left;
    update();
    connpos_dirty = true;

    emit_signal("slot_updated", p_idx);
}
int GraphNode::get_slot_type_left(int p_idx) const {

    if (!slot_info.contains(p_idx))
        return 0;
    return slot_info.at(p_idx).type_left;
}
void GraphNode::set_slot_color_left(int p_idx, const Color &p_color_left) {
    ERR_FAIL_COND_MSG(!slot_info.contains(p_idx), FormatVE("Cannot set color_left for the slot '%d' because it hasn't been enabled.", p_idx));

    slot_info[p_idx].color_left = p_color_left;
    update();
    connpos_dirty = true;

    emit_signal("slot_updated", p_idx);
}

Color GraphNode::get_slot_color_left(int p_idx) const {

    if (!slot_info.contains(p_idx))
        return Color(1, 1, 1, 1);
    return slot_info.at(p_idx).color_left;
}

bool GraphNode::is_slot_enabled_right(int p_idx) const {

    if (!slot_info.contains(p_idx))
        return false;
    return slot_info.at(p_idx).enable_right;
}

void GraphNode::set_slot_enabled_right(int p_idx, bool p_enable_right) {
    ERR_FAIL_COND_MSG(p_idx < 0, FormatVE("Cannot set enable_right for the slot with p_idx (%d) lesser than zero.", p_idx));

    slot_info[p_idx].enable_right = p_enable_right;
    update();
    connpos_dirty = true;

    emit_signal("slot_updated", p_idx);
}

void GraphNode::set_slot_type_right(int p_idx, int p_type_right) {
    ERR_FAIL_COND_MSG(!slot_info.contains(p_idx), FormatVE("Cannot set type_right for the slot '%d' because it hasn't been enabled.", p_idx));

    slot_info[p_idx].type_right = p_type_right;
    update();
    connpos_dirty = true;

    emit_signal("slot_updated", p_idx);
}

int GraphNode::get_slot_type_right(int p_idx) const {

    if (!slot_info.contains(p_idx))
        return 0;
    return slot_info.at(p_idx).type_right;
}

void GraphNode::set_slot_color_right(int p_idx, const Color &p_color_right) {
    ERR_FAIL_COND_MSG(!slot_info.contains(p_idx), FormatVE("Cannot set color_right for the slot '%d' because it hasn't been enabled.", p_idx));

    slot_info[p_idx].color_right = p_color_right;
    update();
    connpos_dirty = true;

    emit_signal("slot_updated", p_idx);
}
Color GraphNode::get_slot_color_right(int p_idx) const {

    if (!slot_info.contains(p_idx))
        return Color(1, 1, 1, 1);
    return slot_info.at(p_idx).color_right;
}

Size2 GraphNode::get_minimum_size() const {

    Ref<Font> title_font = get_theme_font("title_font");

    int sep = get_theme_constant("separation");
    Ref<StyleBox> sb = get_theme_stylebox("frame");
    bool first = true;

    Size2 minsize;
    minsize.x = title_font->get_string_size(title).x;
    if (show_close) {
        Ref<Texture> close = get_theme_icon("close");
        minsize.x += sep + close->get_width();
    }

    for (int i = 0; i < get_child_count(); i++) {

        Control *c = object_cast<Control>(get_child(i));
        if (!c)
            continue;
        if (c->is_set_as_top_level())
            continue;

        Size2i size = c->get_combined_minimum_size();

        minsize.y += size.y;
        minsize.x = M_MAX(minsize.x, size.x);

        if (first)
            first = false;
        else
            minsize.y += sep;
    }

    return minsize + sb->get_minimum_size();
}

void GraphNode::set_title(StringView _title) {
    if (title == _title)
        return;
    title = _title;
    update();
    Object_change_notify(this,"title");
    minimum_size_changed();
}

String GraphNode::get_title() const {

    return title;
}

void GraphNode::set_offset(const Vector2 &p_offset) {

    offset = p_offset;
    emit_signal("offset_changed");
    update();
}

Vector2 GraphNode::get_offset() const {

    return offset;
}

void GraphNode::set_selected(bool p_selected) {
    selected = p_selected;
    update();
}

bool GraphNode::is_selected() {
    return selected;
}

void GraphNode::set_drag(bool p_drag) {
    if (p_drag)
        drag_from = get_offset();
    else
        emit_signal("dragged", drag_from, get_offset()); //useful for undo/redo
}

Vector2 GraphNode::get_drag_from() {
    return drag_from;
}

void GraphNode::set_show_close_button(bool p_enable) {

    show_close = p_enable;
    update();
}
bool GraphNode::is_close_button_visible() const {

    return show_close;
}

void GraphNode::_connpos_update() {

    int edgeofs = get_theme_constant("port_offset");
    int sep = get_theme_constant("separation");

    Ref<StyleBox> sb = get_theme_stylebox("frame");
    conn_input_cache.clear();
    conn_output_cache.clear();
    int vofs = 0;

    int idx = 0;

    for (int i = 0; i < get_child_count(); i++) {
        Control *c = object_cast<Control>(get_child(i));
        if (!c)
            continue;
        if (c->is_set_as_top_level())
            continue;

        Size2i size = c->get_rect().size;

        int y = sb->get_margin(Margin::Top) + vofs;
        int h = size.y;

        if (slot_info.contains(idx)) {

            if (slot_info[idx].enable_left) {
                ConnCache cc;
                cc.pos = Point2i(edgeofs, y + h / 2);
                cc.type = slot_info[idx].type_left;
                cc.color = slot_info[idx].color_left;
                conn_input_cache.push_back(cc);
            }
            if (slot_info[idx].enable_right) {
                ConnCache cc;
                cc.pos = Point2i(get_size().width - edgeofs, y + h / 2);
                cc.type = slot_info[idx].type_right;
                cc.color = slot_info[idx].color_right;
                conn_output_cache.push_back(cc);
            }
        }

        vofs += sep;
        vofs += size.y;
        idx++;
    }

    connpos_dirty = false;
}

int GraphNode::get_connection_input_count() {

    if (connpos_dirty)
        _connpos_update();

    return conn_input_cache.size();
}
int GraphNode::get_connection_output_count() {

    if (connpos_dirty)
        _connpos_update();

    return conn_output_cache.size();
}

Vector2 GraphNode::get_connection_input_position(int p_idx) {

    if (connpos_dirty)
        _connpos_update();

    ERR_FAIL_INDEX_V(p_idx, conn_input_cache.size(), Vector2());
    Vector2 pos = conn_input_cache[p_idx].pos;
    pos.x *= get_scale().x;
    pos.y *= get_scale().y;
    return pos;
}

int GraphNode::get_connection_input_type(int p_idx) {

    if (connpos_dirty)
        _connpos_update();

    ERR_FAIL_INDEX_V(p_idx, conn_input_cache.size(), 0);
    return conn_input_cache[p_idx].type;
}

Color GraphNode::get_connection_input_color(int p_idx) {

    if (connpos_dirty)
        _connpos_update();

    ERR_FAIL_INDEX_V(p_idx, conn_input_cache.size(), Color());
    return conn_input_cache[p_idx].color;
}

Vector2 GraphNode::get_connection_output_position(int p_idx) {

    if (connpos_dirty)
        _connpos_update();

    ERR_FAIL_INDEX_V(p_idx, conn_output_cache.size(), Vector2());
    Vector2 pos = conn_output_cache[p_idx].pos;
    pos.x *= get_scale().x;
    pos.y *= get_scale().y;
    return pos;
}

int GraphNode::get_connection_output_type(int p_idx) {

    if (connpos_dirty)
        _connpos_update();

    ERR_FAIL_INDEX_V(p_idx, conn_output_cache.size(), 0);
    return conn_output_cache[p_idx].type;
}

Color GraphNode::get_connection_output_color(int p_idx) {

    if (connpos_dirty)
        _connpos_update();

    ERR_FAIL_INDEX_V(p_idx, conn_output_cache.size(), Color());
    return conn_output_cache[p_idx].color;
}

void GraphNode::_gui_input(const Ref<InputEvent> &p_ev) {

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_ev);
    if (mb) {

        ERR_FAIL_COND_MSG(get_parent_control() == nullptr, "GraphNode must be the child of a GraphEdit node.");

        if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {

            Vector2 mpos = Vector2(mb->get_position().x, mb->get_position().y);
            if (close_rect.size != Size2() && close_rect.has_point(mpos)) {
                //send focus to parent
                get_parent_control()->grab_focus();
                emit_signal("close_request");
                accept_event();
                return;
            }

            Ref<Texture> resizer = get_theme_icon("resizer");

            if (resizable && mpos.x > get_size().x - resizer->get_width() && mpos.y > get_size().y - resizer->get_height()) {

                resizing = true;
                resizing_from = mpos;
                resizing_from_size = get_size();
                accept_event();
                return;
            }

            emit_signal("raise_request");
        }

        if (!mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
            resizing = false;
        }
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_ev);
    if (resizing && mm) {
        Vector2 mpos = mm->get_position();

        Vector2 diff = mpos - resizing_from;

        emit_signal("resize_request", resizing_from_size + diff);
    }
}

void GraphNode::set_overlay(Overlay p_overlay) {

    overlay = p_overlay;
    update();
}

GraphNode::Overlay GraphNode::get_overlay() const {

    return overlay;
}

void GraphNode::set_comment(bool p_enable) {

    comment = p_enable;
    update();
}

bool GraphNode::is_comment() const {

    return comment;
}

void GraphNode::set_resizable(bool p_enable) {

    resizable = p_enable;
    update();
}

bool GraphNode::is_resizable() const {

    return resizable;
}

void GraphNode::_bind_methods() {

    SE_BIND_METHOD(GraphNode,set_title);
    SE_BIND_METHOD(GraphNode,get_title);
    SE_BIND_METHOD(GraphNode,_gui_input);

    MethodBinder::bind_method(D_METHOD("set_slot", {"idx", "enable_left", "type_left", "color_left", "enable_right", "type_right", "color_right", "custom_left", "custom_right"}), &GraphNode::set_slot, {DEFVAL(Ref<Texture>()), DEFVAL(Ref<Texture>())});
    SE_BIND_METHOD(GraphNode,clear_slot);
    SE_BIND_METHOD(GraphNode,clear_all_slots);
    SE_BIND_METHOD(GraphNode,is_slot_enabled_left);
    SE_BIND_METHOD(GraphNode,set_slot_enabled_left);
    SE_BIND_METHOD(GraphNode,get_slot_type_left);
    SE_BIND_METHOD(GraphNode,set_slot_type_left);
    SE_BIND_METHOD(GraphNode,get_slot_color_left);
    SE_BIND_METHOD(GraphNode,set_slot_color_left);
    SE_BIND_METHOD(GraphNode,is_slot_enabled_right);
    SE_BIND_METHOD(GraphNode,set_slot_enabled_right);
    SE_BIND_METHOD(GraphNode,get_slot_type_right);
    SE_BIND_METHOD(GraphNode,set_slot_type_right);
    SE_BIND_METHOD(GraphNode,get_slot_color_right);
    SE_BIND_METHOD(GraphNode,set_slot_color_right);

    SE_BIND_METHOD(GraphNode,set_offset);
    SE_BIND_METHOD(GraphNode,get_offset);

    SE_BIND_METHOD(GraphNode,set_comment);
    SE_BIND_METHOD(GraphNode,is_comment);

    SE_BIND_METHOD(GraphNode,set_resizable);
    SE_BIND_METHOD(GraphNode,is_resizable);

    SE_BIND_METHOD(GraphNode,set_selected);
    SE_BIND_METHOD(GraphNode,is_selected);

    SE_BIND_METHOD(GraphNode,get_connection_output_count);
    SE_BIND_METHOD(GraphNode,get_connection_input_count);

    SE_BIND_METHOD(GraphNode,get_connection_output_position);
    SE_BIND_METHOD(GraphNode,get_connection_output_type);
    SE_BIND_METHOD(GraphNode,get_connection_output_color);
    SE_BIND_METHOD(GraphNode,get_connection_input_position);
    SE_BIND_METHOD(GraphNode,get_connection_input_type);
    SE_BIND_METHOD(GraphNode,get_connection_input_color);

    SE_BIND_METHOD(GraphNode,set_show_close_button);
    SE_BIND_METHOD(GraphNode,is_close_button_visible);

    SE_BIND_METHOD(GraphNode,set_overlay);
    SE_BIND_METHOD(GraphNode,get_overlay);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "title"), "set_title", "get_title");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "offset"), "set_offset", "get_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "show_close"), "set_show_close_button", "is_close_button_visible");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "resizable"), "set_resizable", "is_resizable");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "selected"), "set_selected", "is_selected");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "comment"), "set_comment", "is_comment");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "overlay", PropertyHint::Enum, "Disabled,Breakpoint,Position"), "set_overlay", "get_overlay");

    ADD_SIGNAL(MethodInfo("offset_changed"));
    ADD_SIGNAL(MethodInfo("slot_updated", PropertyInfo(VariantType::INT, "idx")));
    ADD_SIGNAL(MethodInfo("dragged", PropertyInfo(VariantType::VECTOR2, "from"), PropertyInfo(VariantType::VECTOR2, "to")));
    ADD_SIGNAL(MethodInfo("raise_request"));
    ADD_SIGNAL(MethodInfo("close_request"));
    ADD_SIGNAL(MethodInfo("resize_request", PropertyInfo(VariantType::VECTOR2, "new_minsize")));

    BIND_ENUM_CONSTANT(OVERLAY_DISABLED);
    BIND_ENUM_CONSTANT(OVERLAY_BREAKPOINT);
    BIND_ENUM_CONSTANT(OVERLAY_POSITION);
}

GraphNode::GraphNode() {

    overlay = OVERLAY_DISABLED;
    show_close = false;
    connpos_dirty = true;
    set_mouse_filter(MOUSE_FILTER_STOP);
    comment = false;
    resizable = false;
    resizing = false;
    selected = false;
}
