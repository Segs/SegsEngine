/*************************************************************************/
/*  rich_text_effect.cpp                                                 */
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

#include "rich_text_effect.h"

#include "core/script_language.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"

IMPL_GDCLASS(RichTextEffect)

void RichTextEffect::_bind_methods() {
    BIND_VMETHOD(MethodInfo(VariantType::BOOL, "_process_custom_fx", PropertyInfo(VariantType::OBJECT, "char_fx", PropertyHint::ResourceType, "CharFXTransform")));
}

Variant RichTextEffect::get_bbcode() const {
    using namespace PathUtils;
    Variant r;
    if (get_script_instance()) {
        if (!get_script_instance()->get("bbcode", r)) {
            String path = get_script_instance()->get_script()->get_path();
            r = get_basename(get_file(path));
        }
    }
    return r;
}

bool RichTextEffect::_process_effect_impl(const Ref<CharFXTransform>& p_cfx) {
    bool return_value = false;
    if (get_script_instance()) {
        Variant v = get_script_instance()->call("_process_custom_fx", p_cfx);
        if (v.get_type() != VariantType::BOOL) {
            return_value = false;
        } else {
            return_value = v.as<bool>();
        }
    }
    return return_value;
}

RichTextEffect::RichTextEffect() {
}

IMPL_GDCLASS(CharFXTransform)

void CharFXTransform::_bind_methods() {

    SE_BIND_METHOD(CharFXTransform,get_relative_index);
    SE_BIND_METHOD(CharFXTransform,set_relative_index);

    SE_BIND_METHOD(CharFXTransform,get_absolute_index);
    SE_BIND_METHOD(CharFXTransform,set_absolute_index);

    SE_BIND_METHOD(CharFXTransform,get_elapsed_time);
    SE_BIND_METHOD(CharFXTransform,set_elapsed_time);

    SE_BIND_METHOD(CharFXTransform,is_visible);
    SE_BIND_METHOD(CharFXTransform,set_visibility);

    SE_BIND_METHOD(CharFXTransform,get_offset);
    SE_BIND_METHOD(CharFXTransform,set_offset);

    SE_BIND_METHOD(CharFXTransform,get_color);
    SE_BIND_METHOD(CharFXTransform,set_color);

    SE_BIND_METHOD(CharFXTransform,get_environment);
    SE_BIND_METHOD(CharFXTransform,set_environment);

    SE_BIND_METHOD(CharFXTransform,get_character);
    SE_BIND_METHOD(CharFXTransform,set_character);


    ADD_PROPERTY(PropertyInfo(VariantType::INT, "relative_index"), "set_relative_index", "get_relative_index");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "absolute_index"), "set_absolute_index", "get_absolute_index");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "elapsed_time"), "set_elapsed_time", "get_elapsed_time");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "visible"), "set_visibility", "is_visible");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "offset"), "set_offset", "get_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "color"), "set_color", "get_color");
    ADD_PROPERTY(PropertyInfo(VariantType::DICTIONARY, "env"), "set_environment", "get_environment");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "character"), "set_character", "get_character");
}

CharFXTransform::CharFXTransform() {
}
