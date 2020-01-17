/*************************************************************************/
/*  array_property_edit.h                                                */
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

#include "scene/main/node.h"
#include "core/se_string.h"

class ArrayPropertyEdit : public RefCounted {

    GDCLASS(ArrayPropertyEdit,RefCounted)

    int page;
    ObjectID obj;
    StringName property;
    String vtypes;
    String subtype_hint_string;
    PropertyHint subtype_hint;
    VariantType subtype;
    Variant get_array() const;
    VariantType default_type;

    void _notif_change();
    void _notif_changev(StringName p_v);
    void _set_size(int p_size);
    void _set_value(int p_idx, const Variant &p_value);

    bool _dont_undo_redo();

protected:
    static void _bind_methods();
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(ListPOD<PropertyInfo> *p_list) const;

public:
    void edit(Object *p_obj, const StringName &p_prop, se_string_view p_hint_string, VariantType p_deftype);

    Node *get_node();

    ArrayPropertyEdit();
};
