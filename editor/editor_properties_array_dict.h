/*************************************************************************/
/*  editor_properties_array_dict.h                                       */
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

#include "core/dictionary.h"
#include "editor/editor_inspector.h"
#include "editor/editor_spin_slider.h"
#include "scene/gui/button.h"

class HBoxContainer;

class EditorPropertyArrayObject : public RefCounted {

    GDCLASS(EditorPropertyArrayObject,RefCounted)

    Variant array;

protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;

public:
    void set_array(const Variant &p_array);
    Variant get_array();

    EditorPropertyArrayObject();
};

class EditorPropertyDictionaryObject : public RefCounted {

    GDCLASS(EditorPropertyDictionaryObject,RefCounted)

    Variant new_item_key;
    Variant new_item_value;
    Dictionary dict;

protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;

public:
    void set_dict(const Dictionary &p_dict);
    Dictionary get_dict();

    void set_new_item_key(const Variant &p_new_item);
    Variant get_new_item_key();

    void set_new_item_value(const Variant &p_new_item);
    Variant get_new_item_value();

    EditorPropertyDictionaryObject();
};

class EditorPropertyArray : public EditorProperty {
    GDCLASS(EditorPropertyArray,EditorProperty)

    PopupMenu *change_type;

    Ref<EditorPropertyArrayObject> object;
    Button *edit;
    VBoxContainer *vbox = nullptr;
    EditorSpinSlider *size_slider = nullptr;
    EditorSpinSlider *page_slider = nullptr;
    HBoxContainer *page_hbox = nullptr;
    VariantType array_type;
    VariantType subtype = VariantType::NIL;
    PropertyHint subtype_hint= PropertyHint::None;
    String subtype_hint_string;
    int page_length = 20;
    int page_index = 0;
    int changing_type_index = -1;
    int reorder_from_index = -1;
    int reorder_to_index = -1;
    float reorder_mouse_y_delta = 0.0f;
    HBoxContainer *reorder_selected_element_hbox = nullptr;
    Button *reorder_selected_button = nullptr;
    bool updating = false;
    bool dropping = false;

    void _page_changed(double p_page);
    void _length_changed(double p_page);
    void _edit_pressed();
    void _property_changed(const StringName &p_prop, const Variant& p_value, StringName p_name = StringName(), bool changing = false);
    void _change_type(Object *p_button, int p_index);
    void _change_type_menu(int p_index);

    void _object_id_selected(StringView p_property, GameEntity p_id);
    void _remove_pressed(int p_index);
    void _reorder_button_gui_input(const Ref<InputEvent> &p_event);
    void _reorder_button_down(int p_index);
    void _reorder_button_up();

    void _button_draw();
    bool _is_drop_valid(const Dictionary &p_drag_data) const;
    bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
    void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

protected:
    static void _bind_methods() {}
    void _notification(int p_what);

public:
    void setup(VariantType p_array_type, StringView p_hint_string = {});
    void update_property() override;
    EditorPropertyArray();
};

class EditorPropertyDictionary : public EditorProperty {
    GDCLASS(EditorPropertyDictionary,EditorProperty)

    PopupMenu *change_type;
    bool updating;

    Ref<EditorPropertyDictionaryObject> object;
    int page_length = 20;
    int page_index = 0;
    int changing_type_index;
    Button *edit;
    VBoxContainer *vbox;
    EditorSpinSlider *size_slider = nullptr;
    EditorSpinSlider *page_slider = nullptr;
    HBoxContainer *page_hbox = nullptr;
    Button *button_add_item = nullptr;

    void _page_changed(double p_page);
    void _edit_pressed();
    void _property_changed(const StringName &p_prop, const Variant& p_value, StringName p_name = StringName(), bool changing = false);
    void _change_type(Object *p_button, int p_index);
    void _change_type_menu(int p_index);

    void _add_key_value();
    void _object_id_selected(StringView p_property, GameEntity p_id);

protected:
    static void _bind_methods() {}
    void _notification(int p_what);

public:
    void update_property() override;
    EditorPropertyDictionary();
};
