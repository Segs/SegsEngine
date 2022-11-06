/*************************************************************************/
/*  button.cpp                                                           */
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

#include "button.h"

#include "core/ecs_registry.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/translation.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(Button)
namespace  {
    ButtonDrawableComponent &data(GameEntity ent) {
        return game_object_registry.registry.get<ButtonDrawableComponent>(ent);
    }
}

Size2 Button::get_minimum_size() const {
    auto &dat(data(get_instance_id()));

    Size2 minsize = get_theme_font("font")->get_string_size(dat.xl_text);
    if (dat.clip_text)
        minsize.width = 0;

    if (!dat.expand_icon) {
        Ref<Texture> _icon;
        if (not dat.icon && has_icon("icon"))
            _icon = Control::get_theme_icon("icon");
        else
            _icon = dat.icon;

        if (_icon) {

            minsize.height = M_MAX(minsize.height, _icon->get_height());
            if (dat.icon_align != UiTextAlign::ALIGN_CENTER) {
            minsize.width += _icon->get_width();
                if (!dat.xl_text.empty()) {
                    minsize.width += get_theme_constant("hseparation");
                }
            } else {
                minsize.width = M_MAX(minsize.width, _icon->get_width());
            }
        }
    }

    return get_theme_stylebox("normal")->get_minimum_size() + minsize;
}

void Button::_set_internal_margin(Margin p_margin, float p_value) {
    auto &dat(data(get_instance_id()));
    dat._internal_margin[(int8_t)p_margin] = p_value;
}

void Button::_notification(int p_what) {

    auto &dat(data(get_instance_id()));
    switch (p_what) {
        case NOTIFICATION_TRANSLATION_CHANGED: {

            dat.xl_text = tr(StringName(dat.text));
            minimum_size_changed();
            update();
        } break;
        case NOTIFICATION_DRAW: {

            RenderingEntity ci = get_canvas_item();
            Size2 size = get_size();
            Color color;
            Color color_icon(1, 1, 1, 1);

            Ref<StyleBox> style = get_theme_stylebox("normal");

            switch (get_draw_mode()) {

                case DRAW_NORMAL: {

                    style = get_theme_stylebox("normal");
                    if (!dat.flat)
                        style->draw(ci, Rect2(Point2(0, 0), size));
                    // Focus colors only take precedence over normal state.
                    if (has_focus()) {
                        color = get_theme_color("font_color_focus");
                        if (has_color("icon_color_focus")) {
                            color_icon = get_theme_color("icon_color_focus");
                        }
                    } else {
                    color = get_theme_color("font_color");
                        if (has_color("icon_color_normal")) {
                            color_icon = get_theme_color("icon_color_normal");
                        }
                    }
                } break;
                case DRAW_HOVER_PRESSED: {
                    if (has_stylebox("hover_pressed") && has_stylebox_override("hover_pressed")) {
                        style = get_theme_stylebox("hover_pressed");
                        if (!dat.flat)
                            style->draw(ci, Rect2(Point2(0, 0), size));
                        if (has_color("font_color_hover_pressed"))
                            color = get_theme_color("font_color_hover_pressed");
                        else
                            color = get_theme_color("font_color");
                        if (has_color("icon_color_hover_pressed"))
                            color_icon = get_theme_color("icon_color_hover_pressed");

                        break;
                    }
                    [[fallthrough]];
                }
                case DRAW_PRESSED: {

                    style = get_theme_stylebox("pressed");
                    if (!dat.flat)
                        style->draw(ci, Rect2(Point2(0, 0), size));
                    if (has_color("font_color_pressed"))
                        color = get_theme_color("font_color_pressed");
                    else
                        color = get_theme_color("font_color");
                    if (has_color("icon_color_pressed"))
                        color_icon = get_theme_color("icon_color_pressed");

                } break;
                case DRAW_HOVER: {

                    style = get_theme_stylebox("hover");
                    if (!dat.flat)
                        style->draw(ci, Rect2(Point2(0, 0), size));
                    color = get_theme_color("font_color_hover");
                    if (has_color("icon_color_hover"))
                        color_icon = get_theme_color("icon_color_hover");

                } break;
                case DRAW_DISABLED: {

                    style = get_theme_stylebox("disabled");
                    if (!dat.flat)
                        style->draw(ci, Rect2(Point2(0, 0), size));
                    color = get_theme_color("font_color_disabled");
                    if (has_color("icon_color_disabled")) {
                        color_icon = get_theme_color("icon_color_disabled");
                    } else {
                        color_icon.a = 0.4;
                    }

                } break;
            }

            if (has_focus()) {

                Ref<StyleBox> style2 = get_theme_stylebox("focus");
                style2->draw(ci, Rect2(Point2(), size));
            }

            Ref<Font> font = get_theme_font("font");
            Ref<Texture> _icon;
            if (not dat.icon && has_icon("icon"))
                _icon = Control::get_theme_icon("icon");
            else
                _icon = dat.icon;

            Rect2 icon_region = Rect2();
            if (_icon) {

                int valign = size.height - style->get_minimum_size().y;

                float icon_ofs_region = 0;
                Point2 style_offset;
                Size2 icon_size = _icon->get_size();
                if (dat.icon_align == UiTextAlign::ALIGN_LEFT) {
                    style_offset.x = style->get_margin(Margin::Left);
                if (dat._internal_margin[(int8_t)Margin::Left] > 0) {
                    icon_ofs_region = dat._internal_margin[(int8_t)Margin::Left] + get_theme_constant("hseparation");
                }
                } else if (dat.icon_align == UiTextAlign::ALIGN_CENTER) {
                    style_offset.x = 0;
                } else if (dat.icon_align == UiTextAlign::ALIGN_RIGHT) {
                    style_offset.x = -style->get_margin(Margin::Right);
                    if (dat._internal_margin[(int8_t)Margin::Right] > 0) {
                        icon_ofs_region =
                                -dat._internal_margin[(int8_t)Margin::Right] - get_theme_constant("hseparation");
                    }
                }
                style_offset.y = style->get_margin(Margin::Top);

                if (dat.expand_icon) {
                    Size2 _size = get_size() - style->get_offset() * 2;
                    _size.width -= get_theme_constant("hseparation") + icon_ofs_region;
                    if (!dat.clip_text && dat.icon_align != UiTextAlign::ALIGN_CENTER) {
                        _size.width -= get_theme_font("font")->get_string_size(dat.xl_text).width;
                    }
                    float icon_width = _icon->get_width() * _size.height / _icon->get_height();
                    float icon_height = _size.height;

                    if (icon_width > _size.width) {
                        icon_width = _size.width;
                        icon_height = _icon->get_height() * icon_width / _icon->get_width();
                    }

                    icon_size = Size2(icon_width, icon_height);
                }

                if (dat.icon_align == UiTextAlign::ALIGN_LEFT) {
                    icon_region =
                            Rect2(style_offset + Point2(icon_ofs_region, Math::floor((valign - icon_size.y) * 0.5f)),
                                    icon_size);
                } else if (dat.icon_align == UiTextAlign::ALIGN_CENTER) {
                    icon_region =
                            Rect2(style_offset + Point2(icon_ofs_region + Math::floor((size.x - icon_size.x) * 0.5f),
                                                         Math::floor((valign - icon_size.y) * 0.5f)),
                                    icon_size);
                } else {
                    icon_region = Rect2(style_offset + Point2(icon_ofs_region + size.x - icon_size.x,
                                                               Math::floor((valign - icon_size.y) * 0.5f)),
                            icon_size);
                }

                if (icon_region.size.width > 0) {
                    draw_texture_rect_region(_icon, icon_region, Rect2(Point2(), _icon->get_size()), color_icon);
                }
            }

            Point2 icon_ofs = _icon ? Point2(icon_region.size.width + get_theme_constant("hseparation"), 0) : Point2();
            if (dat.align == UiTextAlign::ALIGN_CENTER&& dat.icon_align == UiTextAlign::ALIGN_CENTER) {
                icon_ofs.x = 0;
            }
            int text_clip = size.width - style->get_minimum_size().width - icon_ofs.width;
            if (dat._internal_margin[(int8_t)Margin::Left] > 0) {
                text_clip -= dat._internal_margin[(int8_t)Margin::Left] + get_theme_constant("hseparation");
            }
            if (dat._internal_margin[(int8_t)Margin::Right] > 0) {
                text_clip -= dat._internal_margin[(int8_t)Margin::Right] + get_theme_constant("hseparation");
            }
            Point2 text_ofs =
                    (size - style->get_minimum_size() - icon_ofs - font->get_string_size(dat.xl_text) -
                            Point2(dat._internal_margin[(int8_t)Margin::Right] - dat._internal_margin[(int8_t)Margin::Left],
                                    0)) /
                              2.0;

            switch (dat.align) {
                case UiTextAlign::ALIGN_LEFT: {
                    if (dat.icon_align != UiTextAlign::ALIGN_LEFT) {
                        icon_ofs.x = 0;
                    }
                    if (dat._internal_margin[(int8_t)Margin::Left] > 0) {
                        text_ofs.x = style->get_margin(Margin::Left) + icon_ofs.x +
                                     dat._internal_margin[(int8_t)Margin::Left] + get_theme_constant("hseparation");
                    } else {
                        text_ofs.x = style->get_margin(Margin::Left) + icon_ofs.x;
                    }
                    text_ofs.y += style->get_offset().y;
                } break;
                case UiTextAlign::ALIGN_CENTER: {
                    if (text_ofs.x < 0) {
                        text_ofs.x = 0;
                    }
                    if (dat.icon_align == UiTextAlign::ALIGN_LEFT) {
                    text_ofs += icon_ofs;
                    }
                    text_ofs += style->get_offset();
                } break;
                case UiTextAlign::ALIGN_RIGHT: {
                    int text_width = font->get_string_size(dat.xl_text).x;
                    if (dat._internal_margin[(int8_t)Margin::Right] > 0) {
                        text_ofs.x = size.x - style->get_margin(Margin::Right) - text_width -
                                     dat._internal_margin[(int8_t)Margin::Right] - get_theme_constant("hseparation");
                    } else {
                        text_ofs.x = size.x - style->get_margin(Margin::Right) - text_width;
                    }
                    text_ofs.y += style->get_offset().y;
                    if (dat.icon_align == UiTextAlign::ALIGN_RIGHT) {
                        text_ofs.x -= icon_ofs.x;
                    }
                } break;
            }

            text_ofs.y += font->get_ascent();
            font->draw(ci, text_ofs.floor(), dat.xl_text, color, dat.clip_text ? text_clip : -1);
        } break;
    }
}

void Button::set_text(StringView p_text) {

    auto &dat(data(get_instance_id()));
    if (dat.text == p_text)
        return;

    dat.text = p_text;
    dat.xl_text = tr(p_text);
    update();
    Object_change_notify(this,"text");
    minimum_size_changed();
}

const String & Button::get_text() const {
    const auto &dat(data(get_instance_id()));

    return dat.text;
}


void Button::set_button_icon(const Ref<Texture> &p_icon) {

    auto &dat(data(get_instance_id()));
    if (dat.icon == p_icon)
        return;
    dat.icon = p_icon;
    update();
    Object_change_notify(this,"icon");
    minimum_size_changed();
}

Ref<Texture> Button::get_button_icon() const {

    const auto &dat(data(get_instance_id()));
    return dat.icon;
}

void Button::set_expand_icon(bool p_expand_icon) {

    auto &dat(data(get_instance_id()));
    dat.expand_icon = p_expand_icon;
    update();
    minimum_size_changed();
}

bool Button::is_expand_icon() const {

    const auto &dat(data(get_instance_id()));
    return dat.expand_icon;
}

void Button::set_flat(bool p_flat) {

    auto &dat(data(get_instance_id()));
    dat.flat = p_flat;
    update();
    Object_change_notify(this,"flat");
}

bool Button::is_flat() const {

    const auto &dat(data(get_instance_id()));
    return dat.flat;
}

void Button::set_clip_text(bool p_clip_text) {

    auto &dat(data(get_instance_id()));
    dat.clip_text = p_clip_text;
    update();
    minimum_size_changed();
}

bool Button::get_clip_text() const {
    const auto &dat(data(get_instance_id()));
    return dat.clip_text;
}

void Button::set_text_align(UiTextAlign p_align) {
    auto &dat(data(get_instance_id()));
    dat.align = p_align;
    update();
}

UiTextAlign Button::get_text_align() const {
    const auto &dat(data(get_instance_id()));
    return dat.align;
}

void Button::set_icon_align(UiTextAlign p_align) {
    auto &dat(data(get_instance_id()));
    dat.icon_align = p_align;
    minimum_size_changed();
    update();
}

UiTextAlign Button::get_icon_align() const {
    const auto &dat(data(get_instance_id()));
    return dat.icon_align;
}

void Button::_bind_methods() {

    SE_BIND_METHOD(Button,set_text);
    SE_BIND_METHOD(Button,get_text);
    SE_BIND_METHOD(Button,set_button_icon);
    SE_BIND_METHOD(Button,get_button_icon);
    SE_BIND_METHOD(Button,set_flat);
    SE_BIND_METHOD(Button,is_flat);
    SE_BIND_METHOD(Button,set_clip_text);
    SE_BIND_METHOD(Button,get_clip_text);
    SE_BIND_METHOD(Button,set_text_align);
    SE_BIND_METHOD(Button,get_text_align);
    SE_BIND_METHOD(Button,set_icon_align);
    SE_BIND_METHOD(Button,get_icon_align);
    SE_BIND_METHOD(Button,set_expand_icon);
    SE_BIND_METHOD(Button,is_expand_icon);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "text", PropertyHint::None, "", PROPERTY_USAGE_DEFAULT_INTL),
            "set_text", "get_text");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "icon", PropertyHint::ResourceType, "Texture"), "set_button_icon",
            "get_button_icon");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "flat"), "set_flat", "is_flat");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "clip_text"), "set_clip_text", "get_clip_text");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "align", PropertyHint::Enum, "Left,Center,Right"), "set_text_align",
            "get_text_align");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "icon_align", PropertyHint::Enum, "Left,Center,Right"), "set_icon_align", "get_icon_align");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "expand_icon"), "set_expand_icon", "is_expand_icon");
}

Button::Button(const StringName &p_text) {

    auto &dat(game_object_registry.registry.emplace<ButtonDrawableComponent>(get_instance_id()));
    dat.flat = false;
    dat.clip_text = false;
    dat.expand_icon = false;
    set_mouse_filter(MOUSE_FILTER_STOP);
    set_text(p_text);
    dat.align = UiTextAlign::ALIGN_CENTER;
    dat.icon_align = UiTextAlign::ALIGN_LEFT;

    for (float &margin : dat._internal_margin) {
        margin = 0;
    }
}

Button::~Button() = default;
