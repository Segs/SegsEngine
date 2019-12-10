/*************************************************************************/
/*  rich_text_effect.h                                                   */
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

#pragma once

#include "core/resource.h"
#include "core/math/vector2.h"
#include "core/color.h"

#include <QChar>
using CharType = class QChar;

class RichTextEffect : public Resource {
    GDCLASS(RichTextEffect, Resource)
    OBJ_SAVE_TYPE(RichTextEffect)

protected:
    static void _bind_methods();

public:
    Variant get_bbcode() const;
    bool _process_effect_impl(const Ref<class CharFXTransform>& p_cfx);

    RichTextEffect();
};

class CharFXTransform : public RefCounted {
    GDCLASS(CharFXTransform, RefCounted)

protected:
    static void _bind_methods();

public:
    Dictionary environment;
    Point2 offset=Point2();
    Color color= Color();
    uint64_t relative_index=0;
    uint64_t absolute_index=0;
    float elapsed_time=0;
    QChar character = 0;
    bool visibility=true;

    CharFXTransform();
    uint64_t get_relative_index() { return relative_index; }
    void set_relative_index(uint64_t p_index) { relative_index = p_index; }
    uint64_t get_absolute_index() { return absolute_index; }
    void set_absolute_index(uint64_t p_index) { absolute_index = p_index; }
    float get_elapsed_time() { return elapsed_time; }
    void set_elapsed_time(float p_elapsed_time) { elapsed_time = p_elapsed_time; }
    bool is_visible() { return visibility; }
    void set_visibility(bool p_vis) { visibility = p_vis; }
    Point2 get_offset() { return offset; }
    void set_offset(Point2 p_offset) { offset = p_offset; }
    Color get_color() { return color; }
    void set_color(Color p_color) { color = p_color; }
    int get_character() { return (int)character.unicode(); }
    void set_character(int p_char) { character = (CharType)p_char; }
    Dictionary get_environment() { return environment; }
    void set_environment(const Dictionary &p_environment) { environment = p_environment; }

    Variant get_value_or(se_string_view p_key, const Variant& p_default_value);
};
