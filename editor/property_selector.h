/*************************************************************************/
/*  property_selector.h                                                  */
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

#pragma once

#include "editor/property_editor.h"
#include "scene/gui/rich_text_label.h"

class EditorHelpBit;

class PropertySelector : public ConfirmationDialog {
    GDCLASS(PropertySelector,ConfirmationDialog)

    LineEdit *search_box;
    Tree *search_options;

    void _update_search();

    void _sbox_input(const Ref<InputEvent> &p_ie);

    void _confirmed();
    void _text_changed(StringView p_newtext);

    EditorHelpBit *help_bit;

    bool properties;
    UIString selected;
    VariantType type;
    StringName base_type;
    GameEntity script;
    Object *instance;
    bool virtuals_only;

    void _item_selected();

    Vector<VariantType> type_filter;

protected:
    void _notification(int p_what);
    void _hide_requested();
    static void _bind_methods();

public:
    void select_method_from_instance(Object *p_instance, const UIString &p_current = UIString());

    void select_property_from_basic_type(VariantType p_type, const UIString &p_current = UIString());
    void select_property_from_instance(Object *p_instance, const UIString &p_current = UIString());

    void set_type_filter(const Vector<VariantType> &p_type_filter);

    PropertySelector();
};
