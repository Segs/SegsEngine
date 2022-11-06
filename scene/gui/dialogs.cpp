/*************************************************************************/
/*  dialogs.cpp                                                          */
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

#include "dialogs.h"

#include "core/callable_method_pointer.h"
#include "core/print_string.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"
#include "core/ustring.h"
#include "line_edit.h"
#include "core/method_bind.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"


#ifdef TOOLS_ENABLED
#include "editor/editor_scale.h"
#include "editor/editor_node.h"
#include "scene/main/viewport.h" // Only used to check for more modals when dimming the editor.

#endif

IMPL_GDCLASS(WindowDialog)
IMPL_GDCLASS(PopupDialog)
IMPL_GDCLASS(AcceptDialog)
IMPL_GDCLASS(ConfirmationDialog)

// WindowDialog

void WindowDialog::_post_popup() {

    drag_type = DRAG_NONE; // just in case
}

void WindowDialog::_fix_size() {

    // Perhaps this should be called when the viewport resizes as well or windows go out of bounds...

    // Ensure the whole window is visible.
    Point2i pos = get_global_position();
    Size2i size = get_size();
    Size2i viewport_size = get_viewport_rect().size;

    // Windows require additional padding to keep the window chrome visible.
    Ref<StyleBox> panel = get_theme_stylebox("panel", "WindowDialog");
    float top = 0;
    float left = 0;
    float bottom = 0;
    float right = 0;

    // Check validity, because the theme could contain a different type of StyleBox.
    if (0==strcmp(panel->get_class(),"StyleBoxTexture")) {
        Ref<StyleBoxTexture> panel_texture(dynamic_ref_cast<StyleBoxTexture>(panel));
        top = panel_texture->get_expand_margin_size(Margin::Top);
        left = panel_texture->get_expand_margin_size(Margin::Left);
        bottom = panel_texture->get_expand_margin_size(Margin::Bottom);
        right = panel_texture->get_expand_margin_size(Margin::Right);
    } else if (0==strcmp(panel->get_class(),"StyleBoxFlat")) {
        Ref<StyleBoxFlat> panel_flat(dynamic_ref_cast<StyleBoxFlat>(panel));
        top = panel_flat->get_expand_margin_size(Margin::Top);
        left = panel_flat->get_expand_margin_size(Margin::Left);
        bottom = panel_flat->get_expand_margin_size(Margin::Bottom);
        right = panel_flat->get_expand_margin_size(Margin::Right);
    }

    pos.x = M_MAX(left, MIN(pos.x, viewport_size.x - size.x - right));
    pos.y = M_MAX(top, MIN(pos.y, viewport_size.y - size.y - bottom));
    set_global_position(pos);

    if (resizable) {
        size.x = MIN(size.x, viewport_size.x - left - right);
        size.y = MIN(size.y, viewport_size.y - top - bottom);
        set_size(size);
    }
}

bool WindowDialog::has_point(const Point2 &p_point) const {

    Rect2 r(Point2(), get_size());

    // Enlarge upwards for title bar.
    int title_height = get_theme_constant("title_height", "WindowDialog");
    r.position.y -= title_height;
    r.size.y += title_height;

    // Inflate by the resizable border thickness.
    if (resizable) {
        int scaleborder_size = get_theme_constant("scaleborder_size", "WindowDialog");
        r.position.x -= scaleborder_size;
        r.size.width += scaleborder_size * 2;
        r.position.y -= scaleborder_size;
        r.size.height += scaleborder_size * 2;
    }

    return r.has_point(p_point);
}

void WindowDialog::_gui_input(const Ref<InputEvent> &p_event) {

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);

    if (mb && mb->get_button_index() == BUTTON_LEFT) {

        if (mb->is_pressed()) {
            // Begin a possible dragging operation.
            drag_type = _drag_hit_test(Point2(mb->get_position().x, mb->get_position().y));
            if (drag_type != DRAG_NONE)
                drag_offset = get_global_mouse_position() - get_position();
            drag_offset_far = get_position() + get_size() - get_global_mouse_position();
        } else if (drag_type != DRAG_NONE && !mb->is_pressed()) {
            // End a dragging operation.
            drag_type = DRAG_NONE;
        }
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);

    if (mm) {

        if (drag_type == DRAG_NONE) {
            // Update the cursor while moving along the borders.
            CursorShape cursor = CURSOR_ARROW;
            if (resizable) {
                int preview_drag_type = _drag_hit_test(Point2(mm->get_position().x, mm->get_position().y));
                switch (preview_drag_type) {
                    case DRAG_RESIZE_TOP:
                    case DRAG_RESIZE_BOTTOM:
                        cursor = CURSOR_VSIZE;
                        break;
                    case DRAG_RESIZE_LEFT:
                    case DRAG_RESIZE_RIGHT:
                        cursor = CURSOR_HSIZE;
                        break;
                    case DRAG_RESIZE_TOP + DRAG_RESIZE_LEFT:
                    case DRAG_RESIZE_BOTTOM + DRAG_RESIZE_RIGHT:
                        cursor = CURSOR_FDIAGSIZE;
                        break;
                    case DRAG_RESIZE_TOP + DRAG_RESIZE_RIGHT:
                    case DRAG_RESIZE_BOTTOM + DRAG_RESIZE_LEFT:
                        cursor = CURSOR_BDIAGSIZE;
                        break;
                }
            }
            if (get_cursor_shape() != cursor)
                set_default_cursor_shape(cursor);
        } else {
            // Update while in a dragging operation.
            Point2 global_pos = get_global_mouse_position();
            global_pos.y = M_MAX(global_pos.y, 0); // Ensure title bar stays visible.

            Rect2 rect = get_rect();
            Size2 min_size = get_combined_minimum_size();

            if (drag_type == DRAG_MOVE) {
                rect.position = global_pos - drag_offset;
            } else {
                if (drag_type & DRAG_RESIZE_TOP) {
                    int bottom = rect.position.y + rect.size.height;
                    int max_y = bottom - min_size.height;
                    rect.position.y = MIN(global_pos.y - drag_offset.y, max_y);
                    rect.size.height = bottom - rect.position.y;
                } else if (drag_type & DRAG_RESIZE_BOTTOM) {
                    rect.size.height = global_pos.y - rect.position.y + drag_offset_far.y;
                }
                if (drag_type & DRAG_RESIZE_LEFT) {
                    int right = rect.position.x + rect.size.width;
                    int max_x = right - min_size.width;
                    rect.position.x = MIN(global_pos.x - drag_offset.x, max_x);
                    rect.size.width = right - rect.position.x;
                } else if (drag_type & DRAG_RESIZE_RIGHT) {
                    rect.size.width = global_pos.x - rect.position.x + drag_offset_far.x;
                }
            }

            set_size(rect.size);
            set_position(rect.position);
        }
    }
}

void WindowDialog::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_DRAW: {
            RenderingEntity canvas = get_canvas_item();

            // Draw the background.
            Ref<StyleBox> panel = get_theme_stylebox("panel");
            Size2 size = get_size();
            panel->draw(canvas, Rect2(0, 0, size.x, size.y));

            // Draw the title bar text.
            Ref<Font> title_font = get_theme_font("title_font", "WindowDialog");
            Color title_color = get_theme_color("title_color", "WindowDialog");
            int title_height = get_theme_constant("title_height", "WindowDialog");
            int font_height = title_font->get_height() - title_font->get_descent() * 2;
            int x = (size.x - title_font->get_string_size(xl_title).x) / 2;
            int y = (-title_height + font_height) / 2;
            title_font->draw_ui_string(canvas, Point2(x, y), StringUtils::from_utf8(xl_title), title_color, size.x - panel->get_minimum_size().x);
        } break;

        case NOTIFICATION_THEME_CHANGED:
        case NOTIFICATION_ENTER_TREE: {
            close_button->set_normal_texture(get_theme_icon("close", "WindowDialog"));
            close_button->set_pressed_texture(get_theme_icon("close", "WindowDialog"));
            close_button->set_hover_texture(get_theme_icon("close_highlight", "WindowDialog"));
            close_button->set_anchor(Margin::Left, ANCHOR_END);
            close_button->set_begin(Point2(-get_theme_constant("close_h_ofs", "WindowDialog"), -get_theme_constant("close_v_ofs", "WindowDialog")));
        } break;

        case NOTIFICATION_TRANSLATION_CHANGED: {
            StringName new_title(tr(title));
            if (new_title != xl_title) {
                xl_title = new_title;
                minimum_size_changed();
                update();
            }
        } break;

        case NOTIFICATION_MOUSE_EXIT: {
            // Reset the mouse cursor when leaving the resizable window border.
            if (resizable && !drag_type) {
                if (get_default_cursor_shape() != CURSOR_ARROW)
                    set_default_cursor_shape(CURSOR_ARROW);
            }
        } break;

#ifdef TOOLS_ENABLED
    case NOTIFICATION_POST_POPUP: {
        if (get_tree() && Engine::get_singleton()->is_editor_hint() && EditorNode::get_singleton()) {
            was_editor_dimmed = EditorNode::get_singleton()->is_editor_dimmed();
            EditorNode::get_singleton()->dim_editor(true);
        }
    } break;

    case NOTIFICATION_POPUP_HIDE: {
        if (get_tree() && Engine::get_singleton()->is_editor_hint() && EditorNode::get_singleton() && !was_editor_dimmed) {
            EditorNode::get_singleton()->dim_editor(false);
            set_pass_on_modal_close_click(false);
        }
    } break;
#endif
    }
}

void WindowDialog::_closed() {

    _close_pressed();
    hide();
}

int WindowDialog::_drag_hit_test(const Point2 &pos) const {
    int drag_type = DRAG_NONE;

    if (resizable) {
        int title_height = get_theme_constant("title_height", "WindowDialog");
        int scaleborder_size = get_theme_constant("scaleborder_size", "WindowDialog");

        Rect2 rect = get_rect();

        if (pos.y < (-title_height + scaleborder_size))
            drag_type = DRAG_RESIZE_TOP;
        else if (pos.y >= (rect.size.height - scaleborder_size))
            drag_type = DRAG_RESIZE_BOTTOM;
        if (pos.x < scaleborder_size)
            drag_type |= DRAG_RESIZE_LEFT;
        else if (pos.x >= (rect.size.width - scaleborder_size))
            drag_type |= DRAG_RESIZE_RIGHT;
    }

    if (drag_type == DRAG_NONE && pos.y < 0)
        drag_type = DRAG_MOVE;

    return drag_type;
}
void WindowDialog::set_title(StringView p_title) {

    StringName new_title = tr(p_title);
    if (title != p_title) {
        title = p_title;
        xl_title = new_title;
        minimum_size_changed();
        update();
    }
}
const String & WindowDialog::get_title() const {

    return title;
}

void WindowDialog::set_resizable(bool p_resizable) {
    resizable = p_resizable;
}
bool WindowDialog::get_resizable() const {
    return resizable;
}

Size2 WindowDialog::get_minimum_size() const {

    Ref<Font> font = get_theme_font("title_font", "WindowDialog");

    const int button_width = close_button->get_combined_minimum_size().x;
    const int title_width = font->get_string_size(xl_title).x;
    const int padding = button_width / 2;
    const int button_area = button_width + padding;

    // As the title gets centered, title_width + close_button_width is not enough.
    // We want a width w, such that w / 2 - title_width / 2 >= button_area, i.e.
    // w >= 2 * button_area + title_width

    return Size2(2 * button_area + title_width, 1);
}

TextureButton *WindowDialog::get_close_button() {

    return close_button;
}

void WindowDialog::_bind_methods() {

    SE_BIND_METHOD(WindowDialog,_gui_input);
    SE_BIND_METHOD(WindowDialog,set_title);
    SE_BIND_METHOD(WindowDialog,get_title);
    SE_BIND_METHOD(WindowDialog,set_resizable);
    SE_BIND_METHOD(WindowDialog,get_resizable);
    SE_BIND_METHOD(WindowDialog,_closed);
    SE_BIND_METHOD(WindowDialog,get_close_button);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "window_title", PropertyHint::None, "", PROPERTY_USAGE_DEFAULT_INTL), "set_title", "get_title");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "resizable", PropertyHint::None, "", PROPERTY_USAGE_DEFAULT_INTL), "set_resizable", "get_resizable");
}

WindowDialog::WindowDialog() {

    drag_type = DRAG_NONE;
    resizable = false;
    close_button = memnew(TextureButton);
    add_child(close_button);
    close_button->connect("pressed",callable_mp(this, &ClassName::_closed));
#ifdef TOOLS_ENABLED
    was_editor_dimmed = false;
#endif
}

WindowDialog::~WindowDialog() {
}

// PopupDialog

void PopupDialog::_notification(int p_what) {

    if (p_what == NOTIFICATION_DRAW) {
        RenderingEntity ci = get_canvas_item();
        get_theme_stylebox("panel")->draw(ci, Rect2(Point2(), get_size()));
    }
}

PopupDialog::PopupDialog() {
}

PopupDialog::~PopupDialog() {
}

// AcceptDialog



void AcceptDialog::_post_popup() {

    WindowDialog::_post_popup();
    get_ok()->grab_focus();
}

void AcceptDialog::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_MODAL_CLOSE: {
            cancel_pressed();
        } break;

        case NOTIFICATION_READY:
        case NOTIFICATION_RESIZED: {
            _update_child_rects();
        } break;
    }
}

void AcceptDialog::_builtin_text_entered(StringView p_text) {

    _ok_pressed();
}

void AcceptDialog::_ok_pressed() {

    if (hide_on_ok)
        hide();
    ok_pressed();
    emit_signal("confirmed");
}
void AcceptDialog::_cancel_pressed() {
//    Window *parent_window = parent_visible;
//    if (parent_visible) {
//        parent_visible->disconnect("focus_entered", callable_mp(this, &AcceptDialog::_parent_focused));
//        parent_visible = nullptr;
//    }

    call_deferred([this] { hide();} );

    emit_signal("cancelled");

    cancel_pressed();

//    if (parent_window) {
//        //parent_window->grab_focus();
//    }
}

void AcceptDialog::_close_pressed() {

    cancel_pressed();
}

UIString AcceptDialog::get_text_ui() const {

    return StringUtils::from_utf8(label->get_text());
}
String AcceptDialog::get_text() const {

    return label->get_text();
}
void AcceptDialog::set_text(StringView p_text) {

    label->set_text(p_text);
    minimum_size_changed();
    _update_child_rects();
}
void AcceptDialog::set_text_utf8(StringView p_text) {

    label->set_text(StringName(p_text));
    minimum_size_changed();
    _update_child_rects();
}
void AcceptDialog::set_hide_on_ok(bool p_hide) {

    hide_on_ok = p_hide;
}
bool AcceptDialog::get_hide_on_ok() const {

    return hide_on_ok;
}

void AcceptDialog::set_autowrap(bool p_autowrap) {

    label->set_autowrap(p_autowrap);
}
bool AcceptDialog::has_autowrap() {

    return label->has_autowrap();
}

void AcceptDialog::register_text_enter(Node *p_line_edit) {

    ERR_FAIL_NULL(p_line_edit);
    LineEdit *line_edit = object_cast<LineEdit>(p_line_edit);
    if (line_edit)
        line_edit->connect("text_entered",callable_mp(this, &ClassName::_builtin_text_entered));
}

void AcceptDialog::_update_child_rects() {

    Size2 label_size = label->get_minimum_size();
    if (label->get_text().empty()) {
        label_size.height = 0;
    }
    int margin = get_theme_constant("margin", "Dialogs");
    Size2 size = get_size();
    Size2 hminsize = hbc->get_combined_minimum_size();

    Vector2 cpos(margin, margin + label_size.height);
    Vector2 csize(size.x - margin * 2, size.y - margin * 3 - hminsize.y - label_size.height);

    for (int i = 0; i < get_child_count(); i++) {
        Control *c = object_cast<Control>(get_child(i));
        if (!c)
            continue;

        if (c == hbc || c == label || c == get_close_button() || c->is_set_as_top_level())
            continue;

        c->set_position(cpos);
        c->set_size(csize);
    }

    cpos.y += csize.y + margin;
    csize.y = hminsize.y;

    hbc->set_position(cpos);
    hbc->set_size(csize);
}

Size2 AcceptDialog::get_minimum_size() const {

    int margin = get_theme_constant("margin", "Dialogs");
    Size2 minsize = label->get_combined_minimum_size();

    for (int i = 0; i < get_child_count(); i++) {
        Control *c = object_cast<Control>(get_child(i));
        if (!c)
            continue;

        if (c == hbc || c == label || c == const_cast<AcceptDialog *>(this)->get_close_button() || c->is_set_as_top_level())
            continue;

        Size2 cminsize = c->get_combined_minimum_size();
        minsize.x = M_MAX(cminsize.x, minsize.x);
        minsize.y = M_MAX(cminsize.y, minsize.y);
    }

    Size2 hminsize = hbc->get_combined_minimum_size();
    minsize.x = M_MAX(hminsize.x, minsize.x);
    minsize.y += hminsize.y;
    minsize.x += margin * 2;
    minsize.y += margin * 3; //one as separation between hbc and child

    Size2 wmsize = WindowDialog::get_minimum_size();
    minsize.x = M_MAX(wmsize.x, minsize.x);
    return minsize;
}

void AcceptDialog::_custom_action(const StringName &p_action) {

    emit_signal("custom_action", p_action);
    custom_action(p_action);
}

Button *AcceptDialog::add_button(const StringName &p_text, bool p_right, StringView p_action) {

    Button *button = memnew(Button);
    button->set_text(p_text);
    if (p_right) {
        hbc->add_child(button);
        hbc->add_spacer();
    } else {

        hbc->add_child(button);
        hbc->move_child(button, 0);
        hbc->add_spacer(true);
    }

    if (!p_action.empty()) {
        button->connectF("pressed",this,[=,sn(StringName(p_action))]() { _custom_action(sn); });
    }

    return button;
}

Button *AcceptDialog::add_cancel(const StringName &p_cancel) {

    StringName c = p_cancel;
    if (p_cancel.empty())
        c = RTR("Cancel");
    Button *b = swap_ok_cancel ? add_button(c, true) : add_button(c);
    b->connect("pressed",callable_mp(this, &ClassName::_cancel_pressed));
    return b;
}

void AcceptDialog::remove_button(Control *p_button)
{
    Button *button = object_cast<Button>(p_button);
    ERR_FAIL_NULL(button);
    ERR_FAIL_COND_MSG(button->get_parent() != hbc, FormatVE("Cannot remove button %s as it does not belong to this dialog.", button->get_name().asCString()));
    ERR_FAIL_COND_MSG(button == ok, "Cannot remove dialog's OK button.");

    Node *right_spacer = hbc->get_child(button->get_index() + 1);
    // Should always be valid but let's avoid crashing
    if (right_spacer) {
        hbc->remove_child(right_spacer);
        memdelete(right_spacer);
    }
    hbc->remove_child(button);

    if (button->is_connected("pressed", callable_mp(this, &AcceptDialog::_custom_action))) {
        button->disconnect("pressed", callable_mp(this, &AcceptDialog::_custom_action));
    }
    if (button->is_connected("pressed", callable_mp((WindowDialog*)this, &WindowDialog::_closed))) {
        button->disconnect("pressed", callable_mp((WindowDialog*)this, &WindowDialog::_closed));
    }
}
void AcceptDialog::_bind_methods() {

    SE_BIND_METHOD(AcceptDialog,get_ok);
    SE_BIND_METHOD(AcceptDialog,get_label);
    SE_BIND_METHOD(AcceptDialog,set_hide_on_ok);
    SE_BIND_METHOD(AcceptDialog,get_hide_on_ok);
    MethodBinder::bind_method(D_METHOD("add_button", {"text", "right", "action"}), &AcceptDialog::add_button, {DEFVAL(false), DEFVAL("")});
    SE_BIND_METHOD(AcceptDialog,add_cancel);
    SE_BIND_METHOD(AcceptDialog,remove_button);
    SE_BIND_METHOD(AcceptDialog,register_text_enter);
    SE_BIND_METHOD(AcceptDialog,set_text);
    SE_BIND_METHOD(AcceptDialog,get_text);
    SE_BIND_METHOD(AcceptDialog,set_autowrap);
    SE_BIND_METHOD(AcceptDialog,has_autowrap);

    ADD_SIGNAL(MethodInfo("confirmed"));
    ADD_SIGNAL(MethodInfo("cancelled"));
    ADD_SIGNAL(MethodInfo("custom_action", PropertyInfo(VariantType::STRING_NAME, "action")));

    ADD_GROUP("Dialog", "dialog");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "dialog_text", PropertyHint::MultilineText, "", PROPERTY_USAGE_DEFAULT_INTL), "set_text", "get_text");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "dialog_hide_on_ok"), "set_hide_on_ok", "get_hide_on_ok");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "dialog_autowrap"), "set_autowrap", "has_autowrap");
}

bool AcceptDialog::swap_ok_cancel = false;
void AcceptDialog::set_swap_ok_cancel(bool p_swap) {

    swap_ok_cancel = p_swap;
}

AcceptDialog::AcceptDialog() {

    int margin = get_theme_constant("margin", "Dialogs");
    int button_margin = get_theme_constant("button_margin", "Dialogs");

    label = memnew(Label);
    label->set_anchor(Margin::Right, ANCHOR_END);
    label->set_anchor(Margin::Bottom, ANCHOR_END);
    label->set_begin(Point2(margin, margin));
    label->set_end(Point2(-margin, -button_margin - 10));
    add_child(label);

    hbc = memnew(HBoxContainer);
    add_child(hbc);

    hbc->add_spacer();
    ok = memnew(Button);
    ok->set_text(RTR("OK"));
    hbc->add_child(ok);
    hbc->add_spacer();

    ok->connect("pressed",callable_mp(this, &ClassName::_ok_pressed));
    set_as_top_level(true);

    hide_on_ok = true;
    set_title(RTR("Alert!"));
}

AcceptDialog::~AcceptDialog() {
}

// ConfirmationDialog

void ConfirmationDialog::_bind_methods() {

    SE_BIND_METHOD(ConfirmationDialog,get_cancel);
}

Button *ConfirmationDialog::get_cancel() {

    return cancel;
}

ConfirmationDialog::ConfirmationDialog() {

    set_title(RTR("Please Confirm..."));
#ifdef TOOLS_ENABLED
    set_custom_minimum_size(Size2(200, 70) * EDSCALE);
#endif
    cancel = add_cancel();
}
