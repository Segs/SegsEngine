/*************************************************************************/
/*  graph_edit.cpp                                                       */
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

#include "graph_edit.h"

#include "core/callable_method_pointer.h"
#include "core/dictionary.h"
#include "core/math/math_funcs.h"
#include "core/method_bind.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/translation_helpers.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/resources/style_box.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_scale.h"
#endif

IMPL_GDCLASS(GraphEditFilter)
IMPL_GDCLASS(GraphEditMinimap)
IMPL_GDCLASS(GraphEdit)

constexpr int MINIMAP_OFFSET = 12;
constexpr int MINIMAP_PADDING = 5;

bool GraphEditFilter::has_point(const Point2 &p_point) const {

    return ge->_filter_input(p_point);
}

GraphEditFilter::GraphEditFilter(GraphEdit *p_edit) {

    ge = p_edit;
}

void GraphEditMinimap::_bind_methods() {
    SE_BIND_METHOD(GraphEditMinimap,_gui_input);
}

GraphEditMinimap::GraphEditMinimap(GraphEdit *p_edit) {
    ge = p_edit;

    graph_proportions = Vector2(1, 1);
    graph_padding = Vector2(0, 0);
    camera_position = Vector2(100, 50);
    camera_size = Vector2(200, 200);
    minimap_padding = Vector2(MINIMAP_PADDING, MINIMAP_PADDING);
    minimap_offset = minimap_padding + _convert_from_graph_position(graph_padding);

    is_pressing = false;
    is_resizing = false;
}

void GraphEditMinimap::update_minimap() {
    Vector2 graph_offset = _get_graph_offset();
    Vector2 graph_size = _get_graph_size();

    camera_position = ge->get_scroll_ofs() - graph_offset;
    camera_size = ge->get_size();

    Vector2 render_size = _get_render_size();
    float target_ratio = render_size.x / render_size.y;
    float graph_ratio = graph_size.x / graph_size.y;

    graph_proportions = graph_size;
    graph_padding = Vector2(0, 0);
    if (graph_ratio > target_ratio) {
        graph_proportions.x = graph_size.x;
        graph_proportions.y = graph_size.x / target_ratio;
        graph_padding.y = Math::abs(graph_size.y - graph_proportions.y) / 2;
    } else {
        graph_proportions.x = graph_size.y * target_ratio;
        graph_proportions.y = graph_size.y;
        graph_padding.x = Math::abs(graph_size.x - graph_proportions.x) / 2;
    }

    // This centers minimap inside the minimap rectangle.
    minimap_offset = minimap_padding + _convert_from_graph_position(graph_padding);
}

Rect2 GraphEditMinimap::get_camera_rect() {
    Vector2 camera_center = _convert_from_graph_position(camera_position + camera_size / 2) + minimap_offset;
    Vector2 camera_viewport = _convert_from_graph_position(camera_size);
    Vector2 camera_position = (camera_center - camera_viewport / 2);
    return Rect2(camera_position, camera_viewport);
}

Vector2 GraphEditMinimap::_get_render_size() {
    if (!is_inside_tree()) {
        return Vector2(0, 0);
    }

    return get_size() - 2 * minimap_padding;
}

Vector2 GraphEditMinimap::_get_graph_offset() {
    return Vector2(ge->h_scroll->get_min(), ge->v_scroll->get_min());
}

Vector2 GraphEditMinimap::_get_graph_size() {
    Vector2 graph_size = Vector2(ge->h_scroll->get_max(), ge->v_scroll->get_max()) - Vector2(ge->h_scroll->get_min(), ge->v_scroll->get_min());

    if (graph_size.x == 0) {
        graph_size.x = 1;
    }
    if (graph_size.y == 0) {
        graph_size.y = 1;
    }

    return graph_size;
}

Vector2 GraphEditMinimap::_convert_from_graph_position(const Vector2 &p_position) {
    Vector2 map_position = Vector2(0, 0);
    Vector2 render_size = _get_render_size();

    map_position.x = p_position.x * render_size.x / graph_proportions.x;
    map_position.y = p_position.y * render_size.y / graph_proportions.y;

    return map_position;
}

Vector2 GraphEditMinimap::_convert_to_graph_position(const Vector2 &p_position) {
    Vector2 graph_position = Vector2(0, 0);
    Vector2 render_size = _get_render_size();

    graph_position.x = p_position.x * graph_proportions.x / render_size.x;
    graph_position.y = p_position.y * graph_proportions.y / render_size.y;

    return graph_position;
}

void GraphEditMinimap::_gui_input(const Ref<InputEvent> &p_ev) {
    if (!ge->is_minimap_enabled()) {
        return;
    }

    Ref<InputEventMouseButton> mb(dynamic_ref_cast<InputEventMouseButton>(p_ev));
    Ref<InputEventMouseMotion> mm(dynamic_ref_cast<InputEventMouseMotion>(p_ev));

    if (mb && mb->get_button_index() == BUTTON_LEFT) {
        if (mb->is_pressed()) {
            is_pressing = true;

            Ref<Texture> resizer = get_theme_icon("resizer");
            Rect2 resizer_hitbox = Rect2(Point2(), resizer->get_size());
            if (resizer_hitbox.has_point(mb->get_position())) {
                is_resizing = true;
            } else {
                Vector2 click_position = _convert_to_graph_position(mb->get_position() - minimap_padding) - graph_padding;
                _adjust_graph_scroll(click_position);
            }
        } else {
            is_pressing = false;
            is_resizing = false;
        }
        accept_event();
    } else if (mm && is_pressing) {
        if (is_resizing) {
            // Prevent setting minimap wider than GraphEdit
            Vector2 new_minimap_size;
            new_minimap_size.x = MIN(get_size().x - mm->get_relative().x, ge->get_size().x - 2.0 * minimap_padding.x);
            new_minimap_size.y = MIN(get_size().y - mm->get_relative().y, ge->get_size().y - 2.0 * minimap_padding.y);
            ge->set_minimap_size(new_minimap_size);
            update();
        } else {
            Vector2 click_position = _convert_to_graph_position(mm->get_position() - minimap_padding) - graph_padding;
            _adjust_graph_scroll(click_position);
        }
        accept_event();
    }
}

void GraphEditMinimap::_adjust_graph_scroll(const Vector2 &p_offset) {
    Vector2 graph_offset = _get_graph_offset();
    ge->set_scroll_ofs(p_offset + graph_offset - camera_size / 2);
}


Error GraphEdit::connect_node(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port) {

    if (is_node_connected(p_from, p_from_port, p_to, p_to_port))
        return OK;
    Connection c;
    c.from = p_from;
    c.from_port = p_from_port;
    c.to = p_to;
    c.to_port = p_to_port;
    c.activity = 0;
    connections.emplace_back(eastl::move(c));
    top_layer->update();
    minimap->update();
    update();
    connections_layer->update();

    return OK;
}

bool GraphEdit::is_node_connected(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port) {

    for (const Connection &E : connections) {

        if (E.from == p_from && E.from_port == p_from_port && E.to == p_to && E.to_port == p_to_port)
            return true;
    }

    return false;
}

void GraphEdit::disconnect_node(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port) {

    for (auto E = connections.begin(); E!= connections.end(); ++E) {

        if (E->from == p_from && E->from_port == p_from_port && E->to == p_to && E->to_port == p_to_port) {

            connections.erase(E);
            top_layer->update();
            minimap->update();
            update();
            connections_layer->update();
            return;
        }
    }
}

bool GraphEdit::clips_input() const {

    return true;
}

void GraphEdit::get_connection_list(List<GraphEdit::Connection> *r_connections) const {

    *r_connections = connections;
}

void GraphEdit::set_scroll_ofs(const Vector2 &p_ofs) {

    setting_scroll_ofs = true;
    h_scroll->set_value(p_ofs.x);
    v_scroll->set_value(p_ofs.y);
    _update_scroll();
    setting_scroll_ofs = false;
}

Vector2 GraphEdit::get_scroll_ofs() const {

    return Vector2(h_scroll->get_value(), v_scroll->get_value());
}

void GraphEdit::_scroll_moved(double) {

    if (!awaiting_scroll_offset_update) {
        call_deferred([this]() {_update_scroll_offset();});
        awaiting_scroll_offset_update = true;
    }
    top_layer->update();
    minimap->update();
    update();

    if (!setting_scroll_ofs) { //in godot, signals on change value are avoided as a convention
        emit_signal("scroll_offset_changed", get_scroll_ofs());
    }
}

void GraphEdit::_update_scroll_offset() {

    set_block_minimum_size_adjust(true);

    for (int i = 0; i < get_child_count(); i++) {

        GraphNode *gn = object_cast<GraphNode>(get_child(i));
        if (!gn)
            continue;

        Point2 pos = gn->get_offset() * zoom;
        pos -= Point2(h_scroll->get_value(), v_scroll->get_value());
        gn->set_position(pos);
        if (gn->get_scale() != Vector2(zoom, zoom)) {
            gn->set_scale(Vector2(zoom, zoom));
        }
    }

    connections_layer->set_position(-Point2(h_scroll->get_value(), v_scroll->get_value()));
    set_block_minimum_size_adjust(false);
    awaiting_scroll_offset_update = false;
}

void GraphEdit::_update_scroll() {

    if (updating)
        return;

    updating = true;

    set_block_minimum_size_adjust(true);

    Rect2 screen;
    for (int i = 0; i < get_child_count(); i++) {

        GraphNode *gn = object_cast<GraphNode>(get_child(i));
        if (!gn)
            continue;

        Rect2 r;
        r.position = gn->get_offset() * zoom;
        r.size = gn->get_size() * zoom;
        screen = screen.merge(r);
    }

    screen.position -= get_size();
    screen.size += get_size() * 2.0;

    h_scroll->set_min(screen.position.x);
    h_scroll->set_max(screen.position.x + screen.size.x);
    h_scroll->set_page(get_size().x);
    if (h_scroll->get_max() - h_scroll->get_min() <= h_scroll->get_page())
        h_scroll->hide();
    else
        h_scroll->show();

    v_scroll->set_min(screen.position.y);
    v_scroll->set_max(screen.position.y + screen.size.y);
    v_scroll->set_page(get_size().y);

    if (v_scroll->get_max() - v_scroll->get_min() <= v_scroll->get_page())
        v_scroll->hide();
    else
        v_scroll->show();

    Size2 hmin = h_scroll->get_combined_minimum_size();
    Size2 vmin = v_scroll->get_combined_minimum_size();

    // Avoid scrollbar overlapping.
    h_scroll->set_anchor_and_margin(Margin::Right, ANCHOR_END, v_scroll->is_visible() ? -vmin.width : 0);
    v_scroll->set_anchor_and_margin(Margin::Bottom, ANCHOR_END, h_scroll->is_visible() ? -hmin.height : 0);

    set_block_minimum_size_adjust(false);

    if (!awaiting_scroll_offset_update) {
        call_deferred([this]() {_update_scroll_offset();});
        awaiting_scroll_offset_update = true;
    }

    updating = false;
}

void GraphEdit::_graph_node_raised(Node *p_gn) {

    GraphNode *gn = object_cast<GraphNode>(p_gn);
    ERR_FAIL_COND(!gn);
    if (gn->is_comment()) {
        move_child(gn, 0);
    } else {
        gn->raise();
    }
    int first_not_comment = 0;
    for (int i = 0; i < get_child_count(); i++) {
        GraphNode *gn2 = object_cast<GraphNode>(get_child(i));
        if (gn2 && !gn2->is_comment()) {
            first_not_comment = i;
            break;
        }
    }

    move_child(connections_layer, first_not_comment);
    top_layer->raise();
    emit_signal("node_selected", Variant(p_gn));
}

void GraphEdit::_graph_node_slot_updated(int p_index, Node *p_gn) {
    GraphNode *gn = object_cast<GraphNode>(p_gn);
    ERR_FAIL_COND(!gn);
    top_layer->update();
    minimap->update();
    update();
    connections_layer->update();
}

void GraphEdit::_graph_node_moved(Node *p_gn) {

    GraphNode *gn = object_cast<GraphNode>(p_gn);
    ERR_FAIL_COND(!gn);
    top_layer->update();
    minimap->update();
    update();
    connections_layer->update();
}

void GraphEdit::add_child_notify(Node *p_child) {

    Control::add_child_notify(p_child);

    // Top layer always on top!
    GraphEditFilter *top_layer_copy=top_layer;
    top_layer->call_deferred([top_layer_copy](){ top_layer_copy->raise();});

    GraphNode *gn = object_cast<GraphNode>(p_child);
    if (gn) {
        gn->set_scale(Vector2(zoom, zoom));
        gn->connect("offset_changed",callable_gen(this,[=]() { _graph_node_moved(gn); }));
        gn->connect("slot_updated", callable_gen(this,[=](int idx) { _graph_node_slot_updated(idx,gn); }));
        gn->connect("raise_request",callable_gen(this,[=]() { _graph_node_raised(gn); }));
        gn->connect("item_rect_changed", callable_mp((CanvasItem *)connections_layer, &CanvasItem::update));
        gn->connect("item_rect_changed", callable_mp((CanvasItem *)minimap, &CanvasItem::update));
        _graph_node_moved(gn);
    }
}

void GraphEdit::remove_child_notify(Node *p_child) {

    Control::remove_child_notify(p_child);

    if (p_child == top_layer) {
        top_layer = nullptr;
        minimap = nullptr;
    } else if (p_child == connections_layer) {
        connections_layer = nullptr;
    }

    if (top_layer != nullptr && is_inside_tree()) {
        top_layer->call_deferred("raise"); // Top layer always on top!
    }

    GraphNode *gn = object_cast<GraphNode>(p_child);
    if (gn) {
        gn->disconnect_all("slot_updated", get_instance_id()); //"_graph_node_slot_updated"
        gn->disconnect("offset_changed",callable_mp(this, &GraphEdit::_graph_node_moved));
        gn->disconnect("raise_request",callable_mp(this, &GraphEdit::_graph_node_raised));

        // In case of the whole GraphEdit being destroyed these references can already be freed.
        if (connections_layer != nullptr && connections_layer->is_inside_tree()) {
            gn->disconnect("item_rect_changed", callable_mp((CanvasItem *)connections_layer, &CanvasItem::update));
        }
        if (minimap != nullptr && minimap->is_inside_tree()) {
            gn->disconnect("item_rect_changed", callable_mp((CanvasItem *)minimap, &CanvasItem::update));
        }
    }
}

void GraphEdit::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        port_grab_distance_horizontal = get_theme_constant("port_grab_distance_horizontal");
        port_grab_distance_vertical = get_theme_constant("port_grab_distance_vertical");

        zoom_minus->set_button_icon(get_theme_icon("minus"));
        zoom_reset->set_button_icon(get_theme_icon("reset"));
        zoom_plus->set_button_icon(get_theme_icon("more"));
        snap_button->set_button_icon(get_theme_icon("snap"));
        minimap_button->set_button_icon(get_theme_icon("minimap"));
    }
    if (p_what == NOTIFICATION_READY) {
        Size2 hmin = h_scroll->get_combined_minimum_size();
        Size2 vmin = v_scroll->get_combined_minimum_size();


        h_scroll->set_anchor_and_margin(Margin::Left, ANCHOR_BEGIN, 0);
        h_scroll->set_anchor_and_margin(Margin::Right, ANCHOR_END, 0);
        h_scroll->set_anchor_and_margin(Margin::Top, ANCHOR_END, -hmin.height);
        h_scroll->set_anchor_and_margin(Margin::Bottom, ANCHOR_END, 0);

        v_scroll->set_anchor_and_margin(Margin::Left, ANCHOR_END, -vmin.width);
        v_scroll->set_anchor_and_margin(Margin::Right, ANCHOR_END, 0);
        v_scroll->set_anchor_and_margin(Margin::Top, ANCHOR_BEGIN, 0);
        v_scroll->set_anchor_and_margin(Margin::Bottom, ANCHOR_END, 0);
    }
    if (p_what == NOTIFICATION_DRAW) {

        draw_style_box(get_theme_stylebox("bg"), Rect2(Point2(), get_size()));

        if (is_using_snap()) {
            //draw grid

            int snap = get_snap();

            Vector2 offset = get_scroll_ofs() / zoom;
            Size2 size = get_size() / zoom;

            Point2i from = (offset / float(snap)).floor();
            Point2i len = (size / float(snap)).floor() + Vector2(1, 1);

            Color grid_minor = get_theme_color("grid_minor");
            Color grid_major = get_theme_color("grid_major");

            for (int i = from.x; i < from.x + len.x; i++) {

                Color color;

                if (ABS(i) % 10 == 0)
                    color = grid_major;
                else
                    color = grid_minor;

                float base_ofs = i * snap * zoom - offset.x * zoom;
                draw_line(Vector2(base_ofs, 0), Vector2(base_ofs, get_size().height), color);
            }

            for (int i = from.y; i < from.y + len.y; i++) {

                Color color;

                if (ABS(i) % 10 == 0)
                    color = grid_major;
                else
                    color = grid_minor;

                float base_ofs = i * snap * zoom - offset.y * zoom;
                draw_line(Vector2(0, base_ofs), Vector2(get_size().width, base_ofs), color);
            }
        }
    }

    if (p_what == NOTIFICATION_RESIZED) {
        _update_scroll();
        top_layer->update();
        minimap->update();
    }
}

bool GraphEdit::_filter_input(const Point2 &p_point) {

    Ref<Texture> port = get_theme_icon("port", "GraphNode");
    Vector2i port_size = Vector2i(port->get_width(), port->get_height());

    for (int i = get_child_count() - 1; i >= 0; i--) {

        GraphNode *gn = object_cast<GraphNode>(get_child(i));
        if (!gn)
            continue;

        for (int j = 0; j < gn->get_connection_output_count(); j++) {

            Vector2 pos = gn->get_connection_output_position(j) + gn->get_position();
            if (is_in_hot_zone(pos / zoom, p_point / zoom, port_size, false)) {
                return true;
            }
        }

        for (int j = 0; j < gn->get_connection_input_count(); j++) {

            Vector2 pos = gn->get_connection_input_position(j) + gn->get_position();
            if (is_in_hot_zone(pos / zoom, p_point / zoom, port_size, true)) {
                return true;
            }
        }
    }

    return false;
}

void GraphEdit::_top_layer_input(const Ref<InputEvent> &p_ev) {

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_ev);
    if (mb && mb->get_button_index() == BUTTON_LEFT && mb->is_pressed()) {
        connecting_valid = false;
        Ref<Texture> port = get_theme_icon("port", "GraphNode");
        Vector2i port_size = Vector2i(port->get_width(), port->get_height());

        connecting_valid = false;
        click_pos = mb->get_position() / zoom;
        for (int i = get_child_count() - 1; i >= 0; i--) {

            GraphNode *gn = object_cast<GraphNode>(get_child(i));
            if (!gn)
                continue;

            for (int j = 0; j < gn->get_connection_output_count(); j++) {

                Vector2 pos = gn->get_connection_output_position(j) + gn->get_position();
                if (is_in_hot_zone(pos / zoom, click_pos, port_size, false)) {

                    if (valid_left_disconnect_types.contains(gn->get_connection_output_type(j))) {
                        //check disconnect
                        for (Connection &E : connections) {

                            if (E.from == gn->get_name() && E.from_port == j) {

                                Node *to = get_node((NodePath)(E.to));
                                if (object_cast<GraphNode>(to)) {

                                    connecting_from = E.to;
                                    connecting_index = E.to_port;
                                    connecting_out = false;
                                    connecting_type = object_cast<GraphNode>(to)->get_connection_input_type(E.to_port);
                                    connecting_color =
                                            object_cast<GraphNode>(to)->get_connection_input_color(E.to_port);
                                    connecting_target = false;
                                    connecting_to = pos;
                                    just_disconnected = true;

                                    emit_signal("disconnection_request", E.from, E.from_port, E.to, E.to_port);
                                    to = get_node((NodePath)connecting_from); // maybe it was erased
                                    if (object_cast<GraphNode>(to)) {
                                        connecting = true;
                                    }
                                    return;
                                }
                            }
                        }
                    }

                    connecting = true;
                    connecting_from = gn->get_name();
                    connecting_index = j;
                    connecting_out = true;
                    connecting_type = gn->get_connection_output_type(j);
                    connecting_color = gn->get_connection_output_color(j);
                    connecting_target = false;
                    connecting_to = pos;
                    just_disconnected = false;
                    return;
                }
            }

            for (int j = 0; j < gn->get_connection_input_count(); j++) {

                Vector2 pos = gn->get_connection_input_position(j) + gn->get_position();
                if (is_in_hot_zone(pos / zoom, click_pos, port_size, true)) {

                    if (right_disconnects || valid_right_disconnect_types.contains(gn->get_connection_input_type(j))) {
                        //check disconnect
                        for (const Connection &E : connections) {

                            if (E.to != gn->get_name() || E.to_port != j)
                                continue;

                            Node *fr = get_node((NodePath)(E.from));
                            if (object_cast<GraphNode>(fr)) {

                                connecting_from = E.from;
                                connecting_index = E.from_port;
                                connecting_out = true;
                                connecting_type = object_cast<GraphNode>(fr)->get_connection_output_type(E.from_port);
                                connecting_color = object_cast<GraphNode>(fr)->get_connection_output_color(E.from_port);
                                connecting_target = false;
                                connecting_to = pos;
                                just_disconnected = true;

                                emit_signal("disconnection_request", E.from, E.from_port, E.to, E.to_port);
                                fr = get_node((NodePath)(connecting_from)); // maybe it was erased
                                if (object_cast<GraphNode>(fr)) {
                                    connecting = true;
                                }
                                return;
                            }
                        }
                    }

                    connecting = true;
                    connecting_from = gn->get_name();
                    connecting_index = j;
                    connecting_out = false;
                    connecting_type = gn->get_connection_input_type(j);
                    connecting_color = gn->get_connection_input_color(j);
                    connecting_target = false;
                    connecting_to = pos;
                    just_disconnected = false;

                    return;
                }
            }
        }
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_ev);
    if (mm && connecting) {

        connecting_to = mm->get_position();
        connecting_target = false;
        top_layer->update();
        minimap->update();
        connecting_valid = just_disconnected || click_pos.distance_to(connecting_to / zoom) > 20.0;

        if (connecting_valid) {
        Ref<Texture> port = get_theme_icon("port", "GraphNode");
            Vector2i port_size = Vector2i(port->get_width(), port->get_height());

            Vector2 mpos = mm->get_position() / zoom;
        for (int i = get_child_count() - 1; i >= 0; i--) {

            GraphNode *gn = object_cast<GraphNode>(get_child(i));
                if (!gn) {
                continue;
                }

            if (!connecting_out) {
                for (int j = 0; j < gn->get_connection_output_count(); j++) {

                    Vector2 pos = gn->get_connection_output_position(j) + gn->get_position();
                    int type = gn->get_connection_output_type(j);
                        if ((type == connecting_type ||
                             valid_connection_types.contains(ConnType(connecting_type, type))) &&
                                is_in_hot_zone(pos / zoom, mpos, port_size, false)) {

                        connecting_target = true;
                        connecting_to = pos;
                        connecting_target_to = gn->get_name();
                        connecting_target_index = j;
                        return;
                    }
                }
            } else {

                for (int j = 0; j < gn->get_connection_input_count(); j++) {

                    Vector2 pos = gn->get_connection_input_position(j) + gn->get_position();
                    int type = gn->get_connection_input_type(j);
                        if ((type == connecting_type ||
                                    valid_connection_types.contains(ConnType(connecting_type, type))) &&
                                is_in_hot_zone(pos / zoom, mpos, port_size, true)) {
                        connecting_target = true;
                        connecting_to = pos;
                        connecting_target_to = gn->get_name();
                        connecting_target_index = j;
                        return;
                    }
                }
            }
        }
    }
    }

    if (mb && mb->get_button_index() == BUTTON_LEFT && !mb->is_pressed()) {

        if (connecting_valid) {
        if (connecting && connecting_target) {

            StringName from = connecting_from;
            int from_slot = connecting_index;
            StringName to = connecting_target_to;
            int to_slot = connecting_target_index;

            if (!connecting_out) {
                SWAP(from, to);
                SWAP(from_slot, to_slot);
            }
            emit_signal("connection_request", from, from_slot, to, to_slot);

        } else if (!just_disconnected) {

            StringName from = connecting_from;
            int from_slot = connecting_index;
            Vector2 ofs = Vector2(mb->get_position().x, mb->get_position().y);

            if (!connecting_out) {
                emit_signal("connection_from_empty", from, from_slot, ofs);
            } else {
                emit_signal("connection_to_empty", from, from_slot, ofs);
            }
        }

        }
        connecting = false;
        top_layer->update();
        minimap->update();
        update();
        connections_layer->update();
    }
}

bool GraphEdit::_check_clickable_control(Control *p_control, const Vector2 &pos) {

    if (p_control->is_set_as_top_level() || !p_control->is_visible())
        return false;

    if (!p_control->has_point(pos) || p_control->get_mouse_filter() == MOUSE_FILTER_IGNORE) {
        //test children
        for (int i = 0; i < p_control->get_child_count(); i++) {
            Control *subchild = object_cast<Control>(p_control->get_child(i));
            if (!subchild)
                continue;
            if (_check_clickable_control(subchild, pos - subchild->get_position())) {
                return true;
            }
        }

        return false;
    } else {
        return true;
    }
}

bool GraphEdit::is_in_hot_zone(const Vector2 &pos, const Vector2 &p_mouse_pos, const Vector2i &p_port_size, bool p_left) {
    if (p_left) {
        if (!Rect2(
                    pos.x - p_port_size.x / 2 - port_grab_distance_horizontal,
                    pos.y - p_port_size.y / 2 - port_grab_distance_vertical / 2,
                    p_port_size.x + port_grab_distance_horizontal,
                    p_port_size.y + port_grab_distance_vertical)
                        .has_point(p_mouse_pos)) {
        return false;
        }
    } else {
        if (!Rect2(
                    pos.x - p_port_size.x / 2,
                    pos.y - p_port_size.y / 2 - port_grab_distance_vertical / 2,
                    p_port_size.x + port_grab_distance_horizontal,
                    p_port_size.y + port_grab_distance_vertical)
                        .has_point(p_mouse_pos)) {
            return false;
        }
    }

    for (int i = 0; i < get_child_count(); i++) {
        Control *child = object_cast<Control>(get_child(i));
        if (!child) {
            continue;
        }
        Rect2 rect = child->get_rect();
        // To prevent intersections with other nodes.
        rect.position *= zoom;
        rect.size *= zoom;
        if (rect.has_point(p_mouse_pos)) {

            //check sub-controls
            Vector2 subpos = p_mouse_pos - rect.position;

            for (int j = 0; j < child->get_child_count(); j++) {
                Control *subchild = object_cast<Control>(child->get_child(j));
                if (!subchild) {
                    continue;
                }

                if (_check_clickable_control(subchild, subpos - subchild->get_position())) {
                    return false;
                }
            }
        }
    }

    return true;
}

template <class Vector2>
static _FORCE_INLINE_ Vector2 _bezier_interp(real_t t, Vector2 start, Vector2 control_1, Vector2 control_2, Vector2 end) {
    /* Formula from Wikipedia article on Bezier curves. */
    real_t omt = (1.0 - t);
    real_t omt2 = omt * omt;
    real_t omt3 = omt2 * omt;
    real_t t2 = t * t;
    real_t t3 = t2 * t;

    return start * omt3 + control_1 * omt2 * t * 3.0 + control_2 * omt * t2 * 3.0 + end * t3;
}

void GraphEdit::_bake_segment2d(Vector<Vector2> &points, Vector<Color> &colors, float p_begin, float p_end, const Vector2 &p_a, const Vector2 &p_out, const Vector2 &p_b, const Vector2 &p_in, int p_depth, int p_min_depth, int p_max_depth, float p_tol, const Color &p_color, const Color &p_to_color, int &lines) const {

    float mp = p_begin + (p_end - p_begin) * 0.5;
    Vector2 beg = _bezier_interp(p_begin, p_a, p_a + p_out, p_b + p_in, p_b);
    Vector2 mid = _bezier_interp(mp, p_a, p_a + p_out, p_b + p_in, p_b);
    Vector2 end = _bezier_interp(p_end, p_a, p_a + p_out, p_b + p_in, p_b);

    Vector2 na = (mid - beg).normalized();
    Vector2 nb = (end - mid).normalized();
    float dp = Math::rad2deg(Math::acos(na.dot(nb)));

    if (p_depth >= p_min_depth && (dp < p_tol || p_depth >= p_max_depth)) {

        points.push_back((beg + end) * 0.5);
        colors.push_back(p_color.linear_interpolate(p_to_color, mp));
        lines++;
    } else {
        _bake_segment2d(points, colors, p_begin, mp, p_a, p_out, p_b, p_in, p_depth + 1, p_min_depth, p_max_depth, p_tol, p_color, p_to_color, lines);
        _bake_segment2d(points, colors, mp, p_end, p_a, p_out, p_b, p_in, p_depth + 1, p_min_depth, p_max_depth, p_tol, p_color, p_to_color, lines);
    }
}

void GraphEdit::_draw_cos_line(CanvasItem *p_where, const Vector2 &p_from, const Vector2 &p_to, const Color &p_color,
        const Color &p_to_color, float p_width, float p_bezier_ratio) {

    //cubic bezier code
    float diff = p_to.x - p_from.x;
    float cp_offset;
    int cp_len = get_theme_constant("bezier_len_pos") * p_bezier_ratio;
    int cp_neg_len = get_theme_constant("bezier_len_neg") * p_bezier_ratio;

    if (diff > 0) {
        cp_offset = MIN(cp_len, diff * 0.5);
    } else {
        cp_offset = M_MAX(MIN(cp_len - diff, cp_neg_len), -diff * 0.5);
    }

    Vector2 c1 = Vector2(cp_offset * zoom, 0);
    Vector2 c2 = Vector2(-cp_offset * zoom, 0);

    int lines = 0;

    Vector<Vector2> points;
    Vector<Color> colors;
    points.push_back(p_from);
    colors.push_back(p_color);
    _bake_segment2d(points, colors, 0, 1, p_from, c1, p_to, c2, 0, 3, 9, 3, p_color, p_to_color, lines);
    points.push_back(p_to);
    colors.push_back(p_to_color);

#ifdef TOOLS_ENABLED
    p_where->draw_polyline_colors(points, colors, Math::floor(p_width * EDSCALE), true);
#else
    p_where->draw_polyline_colors(points, colors, p_width, true);
#endif
}

void GraphEdit::_connections_layer_draw() {

    Color activity_color = get_theme_color("activity");
    //draw connections

    for (auto E = connections.begin(); E!= connections.end(); ) {

        NodePath fromnp(E->from);

        Node *from = get_node(fromnp);
        if (!from) {
            E = connections.erase(E);
            continue;
        }

        GraphNode *gfrom = object_cast<GraphNode>(from);

        if (!gfrom) {
            E = connections.erase(E);
            continue;
        }

        NodePath tonp(E->to);
        Node *to = get_node(tonp);
        if (!to) {
            E = connections.erase(E);
            continue;
        }

        GraphNode *gto = object_cast<GraphNode>(to);

        if (!gto) {
            E = connections.erase(E);
            continue;
        }

        Vector2 frompos = gfrom->get_connection_output_position(E->from_port) + gfrom->get_offset() * zoom;
        Color color = gfrom->get_connection_output_color(E->from_port);
        Vector2 topos = gto->get_connection_input_position(E->to_port) + gto->get_offset() * zoom;
        Color tocolor = gto->get_connection_input_color(E->to_port);

        if (E->activity > 0) {
            color = color.linear_interpolate(activity_color, E->activity);
            tocolor = tocolor.linear_interpolate(activity_color, E->activity);
        }
        _draw_cos_line(connections_layer, frompos, topos, color, tocolor);

        ++E;
    }
}

void GraphEdit::_top_layer_draw() {

    _update_scroll();

    if (connecting) {

        Node *fromn = get_node((NodePath)connecting_from);
        ERR_FAIL_COND(!fromn);
        GraphNode *from = object_cast<GraphNode>(fromn);
        ERR_FAIL_COND(!from);
        Vector2 pos;
        if (connecting_out)
            pos = from->get_connection_output_position(connecting_index);
        else
            pos = from->get_connection_input_position(connecting_index);
        pos += from->get_position();

        Vector2 topos = connecting_to;

        Color col = connecting_color;

        if (connecting_target) {
            col.r += 0.4f;
            col.g += 0.4f;
            col.b += 0.4f;
        }

        if (!connecting_out) {
            SWAP(pos, topos);
        }
        _draw_cos_line(top_layer, pos, topos, col, col);
    }

    if (box_selecting) {
        top_layer->draw_rect_filled(box_selecting_rect, get_theme_color("selection_fill"));
        top_layer->draw_rect_stroke(box_selecting_rect, get_theme_color("selection_stroke"));
    }
}

void GraphEdit::_minimap_draw() {
    if (!is_minimap_enabled()) {
        return;
    }

    minimap->update_minimap();

    // Draw the minimap background.
    Rect2 minimap_rect = Rect2(Point2(), minimap->get_size());
    minimap->draw_style_box(minimap->get_theme_stylebox("bg"), minimap_rect);

    Vector2 graph_offset = minimap->_get_graph_offset();
    Vector2 minimap_offset = minimap->minimap_offset;

    // Draw comment graph nodes.
    for (int i = get_child_count() - 1; i >= 0; i--) {
        GraphNode *gn = object_cast<GraphNode>(get_child(i));
        if (!gn || !gn->is_comment()) {
            continue;
        }

        Vector2 node_position = minimap->_convert_from_graph_position(gn->get_offset() * zoom - graph_offset) + minimap_offset;
        Vector2 node_size = minimap->_convert_from_graph_position(gn->get_size() * zoom);
        Rect2 node_rect = Rect2(node_position, node_size);

        Ref<StyleBoxFlat> sb_minimap = dynamic_ref_cast<StyleBoxFlat>(minimap->get_theme_stylebox("node")->duplicate());

        // Override default values with colors provided by the GraphNode's stylebox, if possible.
        Ref<StyleBoxFlat> sbf = dynamic_ref_cast<StyleBoxFlat>(
                gn->get_theme_stylebox(StringName(gn->is_selected() ? "commentfocus" : "comment")));
        if (sbf) {
            Color node_color = sbf->get_bg_color();
            sb_minimap->set_bg_color(node_color);
        }

        minimap->draw_style_box(sb_minimap, node_rect);
    }

    // Draw regular graph nodes.
    for (int i = get_child_count() - 1; i >= 0; i--) {
        GraphNode *gn = object_cast<GraphNode>(get_child(i));
        if (!gn || gn->is_comment()) {
            continue;
        }

        Vector2 node_position = minimap->_convert_from_graph_position(gn->get_offset() * zoom - graph_offset) + minimap_offset;
        Vector2 node_size = minimap->_convert_from_graph_position(gn->get_size() * zoom);
        Rect2 node_rect = Rect2(node_position, node_size);

        Ref<StyleBoxFlat> sb_minimap = dynamic_ref_cast<StyleBoxFlat>(minimap->get_theme_stylebox("node")->duplicate());

        // Override default values with colors provided by the GraphNode's stylebox, if possible.
        Ref<StyleBoxFlat> sbf = dynamic_ref_cast<StyleBoxFlat>(
                    gn->get_theme_stylebox(StringName(gn->is_selected() ? "selectedframe" : "frame")));
        if (sbf) {
            Color node_color = sbf->get_border_color();
            sb_minimap->set_bg_color(node_color);
        }

        minimap->draw_style_box(sb_minimap, node_rect);
    }

    // Draw node connections.
    Color activity_color = get_theme_color("activity");
    for (Connection & E : connections) {
        NodePath fromnp(E.from);

        Node *from = get_node(fromnp);
        if (!from) {
            continue;
        }
        GraphNode *gfrom = object_cast<GraphNode>(from);
        if (!gfrom) {
            continue;
        }

        NodePath tonp(E.to);
        Node *to = get_node(tonp);
        if (!to) {
            continue;
        }
        GraphNode *gto = object_cast<GraphNode>(to);
        if (!gto) {
            continue;
        }

        Vector2 from_slot_position = gfrom->get_offset() * zoom + gfrom->get_connection_output_position(E.from_port);
        Vector2 from_position = minimap->_convert_from_graph_position(from_slot_position - graph_offset) + minimap_offset;
        Color from_color = gfrom->get_connection_output_color(E.from_port);
        Vector2 to_slot_position = gto->get_offset() * zoom + gto->get_connection_input_position(E.to_port);
        Vector2 to_position = minimap->_convert_from_graph_position(to_slot_position - graph_offset) + minimap_offset;
        Color to_color = gto->get_connection_input_color(E.to_port);

        if (E.activity > 0) {
            from_color = from_color.linear_interpolate(activity_color, E.activity);
            to_color = to_color.linear_interpolate(activity_color, E.activity);
        }
        _draw_cos_line(minimap, from_position, to_position, from_color, to_color, 1.0, 0.5);
    }

    // Draw the "camera" viewport.
    Rect2 camera_rect = minimap->get_camera_rect();
    minimap->draw_style_box(minimap->get_theme_stylebox("camera"), camera_rect);

    // Draw the resizer control.
    Ref<Texture> resizer = minimap->get_theme_icon("resizer");
    Color resizer_color = minimap->get_theme_color("resizer_color");
    minimap->draw_texture(resizer, Point2(), resizer_color);
}

void GraphEdit::set_selected(Node *p_child) {

    for (int i = get_child_count() - 1; i >= 0; i--) {

        GraphNode *gn = object_cast<GraphNode>(get_child(i));
        if (!gn)
            continue;

        gn->set_selected(gn == p_child);
    }
}

void GraphEdit::_gui_input(const Ref<InputEvent> &p_ev) {

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_ev);
    if (mm && (mm->get_button_mask() & BUTTON_MASK_MIDDLE || (mm->get_button_mask() & BUTTON_MASK_LEFT && Input::get_singleton()->is_key_pressed(KEY_SPACE)))) {
        Vector2i relative = Input::get_singleton()->warp_mouse_motion(mm, get_global_rect());
        h_scroll->set_value(h_scroll->get_value() - relative.x);
        v_scroll->set_value(v_scroll->get_value() - relative.y);
    }

    if (mm && dragging) {
        if (!moving_selection) {
            emit_signal("_begin_node_move");
            moving_selection = true;
        }
        just_selected = true;
        // TODO: Remove local mouse pos hack if/when InputEventMouseMotion is fixed to support floats
        //drag_accum+=Vector2(mm->get_relative().x,mm->get_relative().y);
        drag_accum = get_local_mouse_position() - drag_origin;
        for (int i = get_child_count() - 1; i >= 0; i--) {
            GraphNode *gn = object_cast<GraphNode>(get_child(i));
            if (gn && gn->is_selected()) {

                Vector2 pos = (gn->get_drag_from() * zoom + drag_accum) / zoom;
                // Snapping can be toggled temporarily by holding down Ctrl.
                // This is done here as to not toggle the grid when holding down Ctrl.
                if (is_using_snap() ^ Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
                    const int snap = get_snap();
                    pos = pos.snapped(Vector2(snap, snap));
                }

                gn->set_offset(pos);
            }
        }
    }

    if (mm && box_selecting) {
        box_selecting_to = get_local_mouse_position();

        box_selecting_rect = Rect2(MIN(box_selecting_from.x, box_selecting_to.x),
                MIN(box_selecting_from.y, box_selecting_to.y),
                ABS(box_selecting_from.x - box_selecting_to.x),
                ABS(box_selecting_from.y - box_selecting_to.y));

        for (int i = get_child_count() - 1; i >= 0; i--) {

            GraphNode *gn = object_cast<GraphNode>(get_child(i));
            if (!gn)
                continue;

            Rect2 r = gn->get_rect();
            r.size *= zoom;
            bool in_box = r.intersects(box_selecting_rect);

            if (in_box) {
                if (!gn->is_selected() && box_selection_mode_additive) {
                    emit_signal("node_selected", Variant::from(gn));
                } else if (gn->is_selected() && !box_selection_mode_additive) {
                    emit_signal("node_unselected", Variant::from(gn));
                }
                gn->set_selected(box_selection_mode_additive);
            } else {
                bool select = (previous_selected.find(gn) != nullptr);
                if (gn->is_selected() && !select) {
                    emit_signal("node_unselected", Variant::from(gn));
                } else if (!gn->is_selected() && select) {
                    emit_signal("node_selected", Variant::from(gn));
                }
                gn->set_selected(select);
            }
        }

        top_layer->update();
        minimap->update();
    }

    Ref<InputEventMouseButton> b = dynamic_ref_cast<InputEventMouseButton>(p_ev);
    if (b) {

        if (b->get_button_index() == BUTTON_RIGHT && b->is_pressed()) {
            if (box_selecting) {
                box_selecting = false;
                box_selecting_rect = Rect2();
                for (int i = get_child_count() - 1; i >= 0; i--) {

                    GraphNode *gn = object_cast<GraphNode>(get_child(i));
                    if (!gn)
                        continue;

                    bool select = (previous_selected.find(gn) != nullptr);
                    if (gn->is_selected() && !select) {
                        emit_signal("node_unselected", Variant::from(gn));
                    } else if (!gn->is_selected() && select) {
                        emit_signal("node_selected", Variant::from(gn));
                    }
                    gn->set_selected(select);
                }
                top_layer->update();
                minimap->update();
            } else {
                if (connecting) {
                    connecting = false;
                    top_layer->update();
                    minimap->update();
                } else {
                    emit_signal("popup_request", b->get_global_position());
                }
            }
        }

        if (b->get_button_index() == BUTTON_LEFT && !b->is_pressed() && dragging) {
            if (!just_selected && drag_accum == Vector2() && Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
                //deselect current node
                for (int i = get_child_count() - 1; i >= 0; i--) {
                    GraphNode *gn = object_cast<GraphNode>(get_child(i));

                    if (gn) {
                        Rect2 r = gn->get_rect();
                        r.size *= zoom;
                        if (r.has_point(get_local_mouse_position())) {
                            gn->set_selected(false);
                            emit_signal("node_unselected", Variant::from(gn));
                        }
                    }
                }
            }

            if (drag_accum != Vector2()) {
                for (int i = get_child_count() - 1; i >= 0; i--) {
                    GraphNode *gn = object_cast<GraphNode>(get_child(i));
                    if (gn && gn->is_selected())
                        gn->set_drag(false);
                }
            }

            if (moving_selection) {
                emit_signal("_end_node_move");
                moving_selection = false;
            }

            dragging = false;

            top_layer->update();
            minimap->update();
            update();
            connections_layer->update();
        }

        if (b->get_button_index() == BUTTON_LEFT && b->is_pressed()) {

            GraphNode *gn = nullptr;

            for (int i = get_child_count() - 1; i >= 0; i--) {

                GraphNode *gn_selected = object_cast<GraphNode>(get_child(i));

                if (gn_selected) {
                    if (gn_selected->is_resizing())
                        continue;

                    if (gn_selected->has_point((b->get_position() - gn_selected->get_position()) / zoom)) {
                        gn = gn_selected;
                        break;
                    }
                }
            }

            if (gn) {

                if (_filter_input(b->get_position()))
                    return;

                dragging = true;
                drag_accum = Vector2();
                drag_origin = get_local_mouse_position();
                just_selected = !gn->is_selected();
                if (!gn->is_selected() && !Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
                    for (int i = 0; i < get_child_count(); i++) {
                        GraphNode *o_gn = object_cast<GraphNode>(get_child(i));
                        if (o_gn) {
                            if (o_gn == gn) {
                                o_gn->set_selected(true);
                            } else {
                                if (o_gn->is_selected()) {
                                    emit_signal("node_unselected", Variant(o_gn));
                                }
                                o_gn->set_selected(false);
                            }
                        }
                    }
                }

                gn->set_selected(true);
                for (int i = 0; i < get_child_count(); i++) {
                    GraphNode *o_gn = object_cast<GraphNode>(get_child(i));
                    if (!o_gn)
                        continue;
                    if (o_gn->is_selected())
                        o_gn->set_drag(true);
                }

            } else {
                if (_filter_input(b->get_position()))
                    return;
                if (Input::get_singleton()->is_key_pressed(KEY_SPACE))
                    return;

                box_selecting = true;
                box_selecting_from = get_local_mouse_position();
                if (b->get_control()) {
                    box_selection_mode_additive = true;
                    previous_selected.clear();
                    previous_selected.reserve(get_child_count());
                    for (int i = get_child_count() - 1; i >= 0; i--) {

                        GraphNode *gn2 = object_cast<GraphNode>(get_child(i));
                        if (!gn2 || !gn2->is_selected())
                            continue;

                        previous_selected.emplace_back(gn2);
                    }
                } else if (b->get_shift()) {
                    box_selection_mode_additive = false;
                    previous_selected.clear();
                    previous_selected.reserve(get_child_count());
                    for (int i = get_child_count() - 1; i >= 0; i--) {

                        GraphNode *gn2 = object_cast<GraphNode>(get_child(i));
                        if (!gn2 || !gn2->is_selected())
                            continue;

                        previous_selected.emplace_back(gn2);
                    }
                } else {
                    box_selection_mode_additive = true;
                    previous_selected.clear();
                    for (int i = get_child_count() - 1; i >= 0; i--) {

                        GraphNode *gn2 = object_cast<GraphNode>(get_child(i));
                        if (!gn2)
                            continue;
                        if (gn2->is_selected()) {
                            emit_signal("node_unselected", Variant(gn2));
                        }
                        gn2->set_selected(false);
                    }
                }
            }
        }

        if (b->get_button_index() == BUTTON_LEFT && !b->is_pressed() && box_selecting) {
            box_selecting = false;
            previous_selected.clear();
            top_layer->update();
            minimap->update();
        }

        int scroll_direction =
                (b->get_button_index() == BUTTON_WHEEL_DOWN) - (b->get_button_index() == BUTTON_WHEEL_UP);
        if (scroll_direction != 0) {
            if (b->get_control()) {
                if (b->get_shift()) {
                    // Horizontal scrolling.
                    h_scroll->set_value(
                            h_scroll->get_value() + (h_scroll->get_page() * b->get_factor() / 8) * scroll_direction);
                } else {
                    // Vertical scrolling.
                    v_scroll->set_value(
                            v_scroll->get_value() + (v_scroll->get_page() * b->get_factor() / 8) * scroll_direction);
        }

            } else {
                // Zooming.
                set_zoom_custom(scroll_direction < 0 ? zoom * zoom_step : zoom / zoom_step, b->get_position());
        }
        }
    }

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_ev);

    if (k) {

        if (k->get_keycode() == KEY_D && k->is_pressed() && k->get_command()) {
            emit_signal("duplicate_nodes_request");
            accept_event();
        }

        if (k->get_keycode() == KEY_C && k->is_pressed() && k->get_command()) {
            emit_signal("copy_nodes_request");
            accept_event();
        }

        if (k->get_keycode() == KEY_V && k->is_pressed() && k->get_command()) {
            emit_signal("paste_nodes_request");
            accept_event();
        }

        if (k->get_keycode() == KEY_DELETE && k->is_pressed()) {
            emit_signal("delete_nodes_request");
            accept_event();
        }
    }

    Ref<InputEventMagnifyGesture> magnify_gesture = dynamic_ref_cast<InputEventMagnifyGesture>(p_ev);
    if (magnify_gesture) {

        set_zoom_custom(zoom * magnify_gesture->get_factor(), magnify_gesture->get_position());
    }

    Ref<InputEventPanGesture> pan_gesture = dynamic_ref_cast<InputEventPanGesture>(p_ev);
    if (pan_gesture) {

        h_scroll->set_value(h_scroll->get_value() + h_scroll->get_page() * pan_gesture->get_delta().x / 8);
        v_scroll->set_value(v_scroll->get_value() + v_scroll->get_page() * pan_gesture->get_delta().y / 8);
    }
}

void GraphEdit::set_connection_activity(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port, float p_activity) {

    for (Connection &E : connections) {

        if (E.from == p_from && E.from_port == p_from_port && E.to == p_to && E.to_port == p_to_port) {

            if (Math::is_equal_approx(E.activity, p_activity)) {
                //update only if changed
                top_layer->update();
                minimap->update();
                connections_layer->update();
            }
            E.activity = p_activity;
            return;
        }
    }
}

void GraphEdit::clear_connections() {

    connections.clear();
    minimap->update();
    update();
    connections_layer->update();
}

void GraphEdit::set_zoom(float p_zoom) {

    set_zoom_custom(p_zoom, get_size() / 2);
}

void GraphEdit::set_zoom_custom(float p_zoom, const Vector2 &p_center) {

    p_zoom = CLAMP(p_zoom, zoom_min, zoom_max);
    if (zoom == p_zoom)
        return;


    Vector2 sbofs = (Vector2(h_scroll->get_value(), v_scroll->get_value()) + p_center) / zoom;

    zoom = p_zoom;
    top_layer->update();
    zoom_minus->set_disabled(zoom == zoom_min);
    zoom_plus->set_disabled(zoom == zoom_max);

    _update_scroll();
    minimap->update();
    connections_layer->update();

    if (is_visible_in_tree()) {

        Vector2 ofs = sbofs * zoom - p_center;
        h_scroll->set_value(ofs.x);
        v_scroll->set_value(ofs.y);
    }

    _update_zoom_label();
    update();
}

float GraphEdit::get_zoom() const {
    return zoom;
}

void GraphEdit::set_zoom_step(float p_zoom_step) {
    p_zoom_step = abs(p_zoom_step);
    if (zoom_step == p_zoom_step) {
        return;
    }

    zoom_step = p_zoom_step;
}

float GraphEdit::get_zoom_step() const {
    return zoom_step;
}

void GraphEdit::set_zoom_min(float p_zoom_min) {
    ERR_FAIL_COND_MSG(p_zoom_min > zoom_max, "Cannot set min zoom level greater than max zoom level.");

    if (zoom_min == p_zoom_min) {
        return;
    }

    zoom_min = p_zoom_min;
    set_zoom(zoom);
}

float GraphEdit::get_zoom_min() const {
    return zoom_min;
}

void GraphEdit::set_zoom_max(float p_zoom_max) {
    ERR_FAIL_COND_MSG(p_zoom_max < zoom_min, "Cannot set max zoom level lesser than min zoom level.");

    if (zoom_max == p_zoom_max) {
        return;
    }

    zoom_max = p_zoom_max;
    set_zoom(zoom);
}

float GraphEdit::get_zoom_max() const {
    return zoom_max;
}

void GraphEdit::set_show_zoom_label(bool p_enable) {
    if (zoom_label->is_visible() == p_enable) {
        return;
    }

    zoom_label->set_visible(p_enable);
}

bool GraphEdit::is_showing_zoom_label() const {
    return zoom_label->is_visible();
}
void GraphEdit::set_right_disconnects(bool p_enable) {

    right_disconnects = p_enable;
}

bool GraphEdit::is_right_disconnects_enabled() const {

    return right_disconnects;
}

void GraphEdit::add_valid_right_disconnect_type(int p_type) {

    valid_right_disconnect_types.insert(p_type);
}

void GraphEdit::remove_valid_right_disconnect_type(int p_type) {

    valid_right_disconnect_types.erase(p_type);
}

void GraphEdit::add_valid_left_disconnect_type(int p_type) {

    valid_left_disconnect_types.insert(p_type);
}

void GraphEdit::remove_valid_left_disconnect_type(int p_type) {

    valid_left_disconnect_types.erase(p_type);
}

Array GraphEdit::_get_connection_list() const {

    List<Connection> conns;
    get_connection_list(&conns);
    Array arr;
    for (const Connection &E : conns) {
        Dictionary d;
        d["from"] = E.from;
        d["from_port"] = E.from_port;
        d["to"] = E.to;
        d["to_port"] = E.to_port;
        arr.push_back(d);
    }
    return arr;
}

void GraphEdit::_zoom_minus() {

    set_zoom(zoom / zoom_step);
}
void GraphEdit::_zoom_reset() {

    set_zoom(1);
}

void GraphEdit::_zoom_plus() {

    set_zoom(zoom * zoom_step);
}

void GraphEdit::_update_zoom_label() {
    int zoom_percent = static_cast<int>(Math::round(zoom * 100));
    String zoom_text = itos(zoom_percent) + "%";
    zoom_label->set_text(zoom_text);
}

void GraphEdit::add_valid_connection_type(int p_type, int p_with_type) {

    ConnType ct(p_type, p_with_type);

    valid_connection_types.insert(ct);
}

void GraphEdit::remove_valid_connection_type(int p_type, int p_with_type) {

    ConnType ct(p_type, p_with_type);

    valid_connection_types.erase(ct);
}

bool GraphEdit::is_valid_connection_type(int p_type, int p_with_type) const {

    ConnType ct(p_type, p_with_type);

    return valid_connection_types.contains(ct);
}

void GraphEdit::set_use_snap(bool p_enable) {

    snap_button->set_pressed(p_enable);
    update();
}

bool GraphEdit::is_using_snap() const {

    return snap_button->is_pressed();
}

int GraphEdit::get_snap() const {

    return snap_amount->get_value();
}

void GraphEdit::set_snap(int p_snap) {

    ERR_FAIL_COND(p_snap < 5);
    snap_amount->set_value(p_snap);
    update();
}
void GraphEdit::_snap_toggled() {
    update();
}

void GraphEdit::_snap_value_changed(double) {

    update();
}

void GraphEdit::set_minimap_size(Vector2 p_size) {
    minimap->set_size(p_size);
    Vector2 minimap_size = minimap->get_size(); // The size might've been adjusted by the minimum size.

    minimap->set_anchors_preset(Control::PRESET_BOTTOM_RIGHT);
    minimap->set_margin(Margin::Left, -minimap_size.x - MINIMAP_OFFSET);
    minimap->set_margin(Margin::Top, -minimap_size.y - MINIMAP_OFFSET);
    minimap->set_margin(Margin::Right, -MINIMAP_OFFSET);
    minimap->set_margin(Margin::Bottom, -MINIMAP_OFFSET);
    minimap->update();
}

Vector2 GraphEdit::get_minimap_size() const {
    return minimap->get_size();
}

void GraphEdit::set_minimap_opacity(float p_opacity) {
    minimap->set_modulate(Color(1, 1, 1, p_opacity));
    minimap->update();
}

float GraphEdit::get_minimap_opacity() const {
    Color minimap_modulate = minimap->get_modulate();
    return minimap_modulate.a;
}

void GraphEdit::set_minimap_enabled(bool p_enable) {
    minimap_button->set_pressed(p_enable);
    _minimap_toggled();
    minimap->update();
}

bool GraphEdit::is_minimap_enabled() const {
    return minimap_button->is_pressed();
}

void GraphEdit::_minimap_toggled() {
    if (is_minimap_enabled()) {
        minimap->set_visible(true);
    minimap->update();
    } else {
        minimap->set_visible(false);
    }
}

HBoxContainer *GraphEdit::get_zoom_hbox() {
    return zoom_hb;
}

void GraphEdit::_bind_methods() {

    SE_BIND_METHOD(GraphEdit,connect_node);
    SE_BIND_METHOD(GraphEdit,is_node_connected);
    SE_BIND_METHOD(GraphEdit,disconnect_node);
    SE_BIND_METHOD(GraphEdit,set_connection_activity);
    MethodBinder::bind_method(D_METHOD("get_connection_list"), &GraphEdit::_get_connection_list);
    SE_BIND_METHOD(GraphEdit,clear_connections);
    SE_BIND_METHOD(GraphEdit,get_scroll_ofs);
    SE_BIND_METHOD(GraphEdit,set_scroll_ofs);

    SE_BIND_METHOD(GraphEdit,add_valid_right_disconnect_type);
    SE_BIND_METHOD(GraphEdit,remove_valid_right_disconnect_type);
    SE_BIND_METHOD(GraphEdit,add_valid_left_disconnect_type);
    SE_BIND_METHOD(GraphEdit,remove_valid_left_disconnect_type);
    SE_BIND_METHOD(GraphEdit,add_valid_connection_type);
    SE_BIND_METHOD(GraphEdit,remove_valid_connection_type);
    SE_BIND_METHOD(GraphEdit,is_valid_connection_type);

    SE_BIND_METHOD(GraphEdit,set_zoom);
    SE_BIND_METHOD(GraphEdit,get_zoom);

    SE_BIND_METHOD(GraphEdit,set_zoom_min);
    SE_BIND_METHOD(GraphEdit,get_zoom_min);

    SE_BIND_METHOD(GraphEdit,set_zoom_max);
    SE_BIND_METHOD(GraphEdit,get_zoom_max);

    SE_BIND_METHOD(GraphEdit,set_zoom_step);
    SE_BIND_METHOD(GraphEdit,get_zoom_step);

    SE_BIND_METHOD(GraphEdit,set_show_zoom_label);
    SE_BIND_METHOD(GraphEdit,is_showing_zoom_label);
    SE_BIND_METHOD(GraphEdit,set_snap);
    SE_BIND_METHOD(GraphEdit,get_snap);

    SE_BIND_METHOD(GraphEdit,set_use_snap);
    SE_BIND_METHOD(GraphEdit,is_using_snap);

    SE_BIND_METHOD(GraphEdit,set_minimap_size);
    SE_BIND_METHOD(GraphEdit,get_minimap_size);
    SE_BIND_METHOD(GraphEdit,set_minimap_opacity);
    SE_BIND_METHOD(GraphEdit,get_minimap_opacity);

    SE_BIND_METHOD(GraphEdit,set_minimap_enabled);
    SE_BIND_METHOD(GraphEdit,is_minimap_enabled);
    SE_BIND_METHOD(GraphEdit,_minimap_toggled);
    SE_BIND_METHOD(GraphEdit,_minimap_draw);

    SE_BIND_METHOD(GraphEdit,set_right_disconnects);
    SE_BIND_METHOD(GraphEdit,is_right_disconnects_enabled);

    SE_BIND_METHOD(GraphEdit,_gui_input);

    SE_BIND_METHOD(GraphEdit,get_zoom_hbox);

    SE_BIND_METHOD(GraphEdit,set_selected);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "right_disconnects"), "set_right_disconnects", "is_right_disconnects_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "scroll_offset"), "set_scroll_ofs", "get_scroll_ofs");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "snap_distance"), "set_snap", "get_snap");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_snap"), "set_use_snap", "is_using_snap");
    ADD_GROUP("Zoom", "");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "zoom"), "set_zoom", "get_zoom");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "zoom_min"), "set_zoom_min", "get_zoom_min");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "zoom_max"), "set_zoom_max", "get_zoom_max");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "zoom_step"), "set_zoom_step", "get_zoom_step");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "show_zoom_label"), "set_show_zoom_label", "is_showing_zoom_label");

    ADD_GROUP("Minimap", "minimap_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "minimap_enabled"), "set_minimap_enabled", "is_minimap_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "minimap_size"), "set_minimap_size", "get_minimap_size");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "minimap_opacity"), "set_minimap_opacity", "get_minimap_opacity");

    ADD_SIGNAL(MethodInfo("connection_request", PropertyInfo(VariantType::STRING_NAME, "from"), PropertyInfo(VariantType::INT, "from_slot"), PropertyInfo(VariantType::STRING_NAME, "to"), PropertyInfo(VariantType::INT, "to_slot")));
    ADD_SIGNAL(MethodInfo("disconnection_request", PropertyInfo(VariantType::STRING_NAME, "from"), PropertyInfo(VariantType::INT, "from_slot"), PropertyInfo(VariantType::STRING_NAME, "to"), PropertyInfo(VariantType::INT, "to_slot")));
    ADD_SIGNAL(MethodInfo("popup_request", PropertyInfo(VariantType::VECTOR2, "position")));
    ADD_SIGNAL(MethodInfo("duplicate_nodes_request"));
    ADD_SIGNAL(MethodInfo("copy_nodes_request"));
    ADD_SIGNAL(MethodInfo("paste_nodes_request"));
    ADD_SIGNAL(MethodInfo("node_selected", PropertyInfo(VariantType::OBJECT, "node", PropertyHint::ResourceType, "Node")));
    ADD_SIGNAL(MethodInfo("node_unselected", PropertyInfo(VariantType::OBJECT, "node", PropertyHint::ResourceType, "Node")));
    ADD_SIGNAL(MethodInfo("connection_to_empty", PropertyInfo(VariantType::STRING_NAME, "from"), PropertyInfo(VariantType::INT, "from_slot"), PropertyInfo(VariantType::VECTOR2, "release_position")));
    ADD_SIGNAL(MethodInfo("connection_from_empty", PropertyInfo(VariantType::STRING_NAME, "to"), PropertyInfo(VariantType::INT, "to_slot"), PropertyInfo(VariantType::VECTOR2, "release_position")));
    ADD_SIGNAL(MethodInfo("delete_nodes_request"));
    ADD_SIGNAL(MethodInfo("_begin_node_move"));
    ADD_SIGNAL(MethodInfo("_end_node_move"));
    ADD_SIGNAL(MethodInfo("scroll_offset_changed", PropertyInfo(VariantType::VECTOR2, "ofs")));
}

GraphEdit::GraphEdit() {
    set_focus_mode(FOCUS_ALL);

    // Allow dezooming 8 times from the default zoom level.
    // At low zoom levels, text is unreadable due to its small size and poor filtering,
    // but this is still useful for previewing and navigation.
    zoom_min = (1 / Math::pow(zoom_step, 8));
    // Allow zooming 4 times from the default zoom level.
    zoom_max = (1 * Math::pow(zoom_step, 4));
    top_layer = memnew(GraphEditFilter(this));
    add_child(top_layer);
    top_layer->set_mouse_filter(MOUSE_FILTER_PASS);
    top_layer->set_anchors_and_margins_preset(Control::PRESET_WIDE);
    top_layer->connect("draw",callable_mp(this, &GraphEdit::_top_layer_draw));
    top_layer->connect("gui_input",callable_mp(this, &GraphEdit::_top_layer_input));

    connections_layer = memnew(Control);
    add_child(connections_layer);
    connections_layer->connect("draw",callable_mp(this, &GraphEdit::_connections_layer_draw));
    connections_layer->set_name("CLAYER");
    connections_layer->set_disable_visibility_clip(true); // so it can draw freely and be offset
    connections_layer->set_mouse_filter(MOUSE_FILTER_IGNORE);

    h_scroll = memnew(HScrollBar);
    h_scroll->set_name("_h_scroll");
    top_layer->add_child(h_scroll);

    v_scroll = memnew(VScrollBar);
    v_scroll->set_name("_v_scroll");
    top_layer->add_child(v_scroll);

    //set large minmax so it can scroll even if not resized yet
    h_scroll->set_min(-10000);
    h_scroll->set_max(10000);

    v_scroll->set_min(-10000);
    v_scroll->set_max(10000);

    h_scroll->connect("value_changed",callable_mp(this, &GraphEdit::_scroll_moved));
    v_scroll->connect("value_changed",callable_mp(this, &GraphEdit::_scroll_moved));


    zoom_hb = memnew(HBoxContainer);
    top_layer->add_child(zoom_hb);
    zoom_hb->set_position(Vector2(10, 10));
    zoom_label = memnew(Label);
    zoom_hb->add_child(zoom_label);
    zoom_label->set_visible(false);
    zoom_label->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    zoom_label->set_align(Label::ALIGN_CENTER);
#ifdef TOOLS_ENABLED
    zoom_label->set_custom_minimum_size(Size2(48, 0) * EDSCALE);
#else
    zoom_label->set_custom_minimum_size(Size2(48, 0));
#endif
    _update_zoom_label();

    zoom_minus = memnew(ToolButton);
    zoom_hb->add_child(zoom_minus);
    zoom_minus->set_tooltip(RTR("Zoom Out"));
    zoom_minus->connect("pressed",callable_mp(this, &GraphEdit::_zoom_minus));
    zoom_minus->set_focus_mode(FOCUS_NONE);

    zoom_reset = memnew(ToolButton);
    zoom_hb->add_child(zoom_reset);
    zoom_reset->set_tooltip(RTR("Zoom Reset"));
    zoom_reset->connect("pressed",callable_mp(this, &GraphEdit::_zoom_reset));
    zoom_reset->set_focus_mode(FOCUS_NONE);

    zoom_plus = memnew(ToolButton);
    zoom_hb->add_child(zoom_plus);
    zoom_plus->set_tooltip(RTR("Zoom In"));
    zoom_plus->connect("pressed",callable_mp(this, &GraphEdit::_zoom_plus));
    zoom_plus->set_focus_mode(FOCUS_NONE);

    snap_button = memnew(ToolButton);
    snap_button->set_toggle_mode(true);
    snap_button->set_tooltip(RTR("Enable snap and show grid."));
    snap_button->connect("pressed",callable_mp(this, &GraphEdit::_snap_toggled));
    snap_button->set_pressed(true);
    snap_button->set_focus_mode(FOCUS_NONE);
    zoom_hb->add_child(snap_button);

    snap_amount = memnew(SpinBox);
    snap_amount->set_min(5);
    snap_amount->set_max(100);
    snap_amount->set_step(1);
    snap_amount->set_value(20);
    snap_amount->connect("value_changed",callable_mp(this, &GraphEdit::_snap_value_changed));
    zoom_hb->add_child(snap_amount);

    minimap_button = memnew(Button);
    minimap_button->set_flat(true);
    minimap_button->set_toggle_mode(true);
    minimap_button->set_tooltip(RTR("Enable grid minimap."));
    minimap_button->connect("pressed", callable_mp(this, &GraphEdit::_minimap_toggled));
    minimap_button->set_pressed(true);
    minimap_button->set_focus_mode(FOCUS_NONE);
    zoom_hb->add_child(minimap_button);

    Vector2 minimap_size = Vector2(240, 160);
    float minimap_opacity = 0.65;

    minimap = memnew(GraphEditMinimap(this));
    top_layer->add_child(minimap);
    minimap->set_name("_minimap");
    minimap->set_modulate(Color(1, 1, 1, minimap_opacity));
    minimap->set_mouse_filter(MOUSE_FILTER_PASS);
    minimap->set_custom_minimum_size(Vector2(50, 50));
    minimap->set_size(minimap_size);
    minimap->set_anchors_preset(Control::PRESET_BOTTOM_RIGHT);
    minimap->set_margin(Margin::Left, -minimap_size.x - MINIMAP_OFFSET);
    minimap->set_margin(Margin::Top, -minimap_size.y - MINIMAP_OFFSET);
    minimap->set_margin(Margin::Right, -MINIMAP_OFFSET);
    minimap->set_margin(Margin::Bottom, -MINIMAP_OFFSET);
    minimap->connect("draw", callable_mp(this, &GraphEdit::_minimap_draw));

    set_clip_contents(true);
}
