/*************************************************************************/
/*  check_button.cpp                                                     */
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

#include "check_button.h"

#include "core/class_db.h"
#include "core/print_string.h"
#include "servers/rendering_server.h"
#include "scene/resources/style_box.h"
#include "core/property_info.h"

IMPL_GDCLASS(CheckButton)

Size2 CheckButton::get_icon_size() const {

    Ref<Texture> on = Control::get_theme_icon(is_disabled() ? StringName("on_disabled") : StringName("on"));
    Ref<Texture> off = Control::get_theme_icon(is_disabled() ? StringName("off_disabled") : StringName("off"));
    Size2 tex_size = Size2(0, 0);
    if (on)
        tex_size = Size2(on->get_width(), on->get_height());
    if (off)
        tex_size = Size2(M_MAX(tex_size.width, off->get_width()), M_MAX(tex_size.height, off->get_height()));

    return tex_size;
}

Size2 CheckButton::get_minimum_size() const {

    Size2 minsize = Button::get_minimum_size();
    Size2 tex_size = get_icon_size();
    minsize.width += tex_size.width;
    if (not get_text().empty())
        minsize.width += get_theme_constant("hseparation");
    Ref<StyleBox> sb = get_theme_stylebox("normal");
    minsize.height = M_MAX(minsize.height, tex_size.height + sb->get_margin(Margin::Top) + sb->get_margin(Margin::Bottom));

    return minsize;
}

void CheckButton::_notification(int p_what) {

    if (p_what == NOTIFICATION_THEME_CHANGED) {

        _set_internal_margin(Margin::Right, get_icon_size().width);
    } else if (p_what == NOTIFICATION_DRAW) {

        RenderingEntity ci = get_canvas_item();

        Ref<Texture> on = Control::get_theme_icon(is_disabled() ? StringName("on_disabled") : StringName("on"));
        Ref<Texture> off = Control::get_theme_icon(is_disabled() ? StringName("off_disabled") : StringName("off"));

        Ref<StyleBox> sb = get_theme_stylebox("normal");
        Vector2 ofs;
        Size2 tex_size = get_icon_size();

        ofs.x = get_size().width - (tex_size.width + sb->get_margin(Margin::Right));
        ofs.y = (get_size().height - tex_size.height) / 2 + get_theme_constant("check_vadjust");

        if (is_pressed())
            on->draw(ci, ofs);
        else
            off->draw(ci, ofs);
    }
}

CheckButton::CheckButton() {

    set_toggle_mode(true);
    set_text_align(UiTextAlign::ALIGN_LEFT);

    _set_internal_margin(Margin::Right, get_icon_size().width);
}

CheckButton::~CheckButton() {
}
