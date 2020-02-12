/*************************************************************************/
/*  array_property_edit.cpp                                              */
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

#include "array_property_edit.h"

#include "core/io/marshalls.h"
#include "core/object_db.h"
#include "core/object_tooling.h"
#include "core/translation_helpers.h"
#include "editor_node.h"
#include "core/method_bind.h"

#define ITEMS_PER_PAGE 100

IMPL_GDCLASS(ArrayPropertyEdit)

Variant ArrayPropertyEdit::get_array() const {

    Object *o = ObjectDB::get_instance(obj);
    if (!o)
        return Array();
    Variant arr = o->get(property);
    if (!arr.is_array()) {
        Variant::CallError ce;
        arr = Variant::construct(default_type, nullptr, 0, ce);
    }
    return arr;
}

void ArrayPropertyEdit::_notif_change() {
    Object_change_notify(this);
}
void ArrayPropertyEdit::_notif_changev(StringName p_v) {

    Object_change_notify(this,p_v);
}

void ArrayPropertyEdit::_set_size(int p_size) {

    Variant arr = get_array();
    arr.call("resize", p_size);
    Object *o = ObjectDB::get_instance(obj);
    if (!o)
        return;

    o->set(property, arr);
}

void ArrayPropertyEdit::_set_value(int p_idx, const Variant &p_value) {

    Variant arr = get_array();
    arr.set(p_idx, p_value);
    Object *o = ObjectDB::get_instance(obj);
    if (!o)
        return;

    o->set(property, arr);
}

bool ArrayPropertyEdit::_set(const StringName &p_name, const Variant &p_value) {

    StringName pn = p_name;

    if (StringUtils::begins_with(pn,"array/")) {

        if (pn == "array/size") {

            Variant arr = get_array();
            int size = arr.call("size");

            int newsize = p_value;
            if (newsize == size)
                return true;

            UndoRedo *ur = EditorNode::get_undo_redo();
            ur->create_action_ui(TTR("Resize Array"));
            ur->add_do_method(this, "_set_size", newsize);
            ur->add_undo_method(this, "_set_size", size);
            if (newsize < size) {
                for (int i = newsize; i < size; i++) {
                    ur->add_undo_method(this, "_set_value", i, arr.get(i));
                }
            } else if (newsize > size) {

                Variant init;
                Variant::CallError ce;
                VariantType new_type = subtype;
                if (new_type == VariantType::NIL && size) {
                    new_type = arr.get(size - 1).get_type();
                }
                if (new_type != VariantType::NIL) {
                    init = Variant::construct(new_type, nullptr, 0, ce);
                    for (int i = size; i < newsize; i++) {
                        ur->add_do_method(this, "_set_value", i, init);
                    }
                }
            }
            ur->add_do_method(this, "_notif_change");
            ur->add_undo_method(this, "_notif_change");
            ur->commit_action();
            return true;
        }
        if (pn == "array/page") {
            page = p_value;
            Object_change_notify(this);
            return true;
        }

    } else if (StringUtils::begins_with(pn,"indices")) {

        if (StringUtils::contains(pn,'_')) {
            //type
            int idx = StringUtils::to_int(StringUtils::get_slice(StringUtils::get_slice(pn,'/', 1),'_', 0));

            int type = p_value;

            Variant arr = get_array();

            Variant value = arr.get(idx);
            if ((int)value.get_type() != type && type >= 0 && type < (int)VariantType::VARIANT_MAX) {
                Variant::CallError ce;
                Variant new_value = Variant::construct(VariantType(type), nullptr, 0, ce);
                UndoRedo *ur = EditorNode::get_undo_redo();

                ur->create_action_ui(TTR("Change Array Value Type"));
                ur->add_do_method(this, "_set_value", idx, new_value);
                ur->add_undo_method(this, "_set_value", idx, value);
                ur->add_do_method(this, "_notif_change");
                ur->add_undo_method(this, "_notif_change");
                ur->commit_action();
            }
            return true;

        } else {
            int idx = StringUtils::to_int(StringUtils::get_slice(pn,'/', 1));
            Variant arr = get_array();

            Variant value = arr.get(idx);
            UndoRedo *ur = EditorNode::get_undo_redo();

            ur->create_action_ui(TTR("Change Array Value"));
            ur->add_do_method(this, "_set_value", idx, p_value);
            ur->add_undo_method(this, "_set_value", idx, value);
            ur->add_do_method(this, "_notif_changev", p_name);
            ur->add_undo_method(this, "_notif_changev", p_name);
            ur->commit_action();
            return true;
        }
    }

    return false;
}

bool ArrayPropertyEdit::_get(const StringName &p_name, Variant &r_ret) const {

    Variant arr = get_array();
    //int size = arr.call("size");

    StringName pn = p_name;
    if (StringUtils::begins_with(pn,"array/")) {

        if (pn == "array/size") {
            r_ret = arr.call("size");
            return true;
        }
        if (pn == "array/page") {
            r_ret = page;
            return true;
        }
    } else if (StringUtils::begins_with(pn,"indices")) {

        if (StringUtils::contains(pn,'_')) {
            //type
            int idx = StringUtils::to_int(StringUtils::get_slice(StringUtils::get_slice(pn,'/', 1),'_', 0));
            bool valid;
            r_ret = arr.get(idx, &valid);
            if (valid)
                r_ret = r_ret.get_type();
            return valid;

        } else {
            int idx = StringUtils::to_int(StringUtils::get_slice(pn,'/', 1));
            bool valid;
            r_ret = arr.get(idx, &valid);

            if (r_ret.get_type() == VariantType::OBJECT && object_cast<EncodedObjectAsID>(r_ret)) {
                r_ret = object_cast<EncodedObjectAsID>(r_ret)->get_object_id();
            }

            return valid;
        }
    }

    return false;
}

void ArrayPropertyEdit::_get_property_list(Vector<PropertyInfo> *p_list) const {

    Variant arr = get_array();
    int size = arr.call("size");

    p_list->push_back(PropertyInfo(VariantType::INT, "array/size", PropertyHint::Range, "0,100000,1"));
    int pages = size / ITEMS_PER_PAGE;
    if (pages > 0)
        p_list->push_back(PropertyInfo(VariantType::INT, "array/page", PropertyHint::Range, "0," + itos(pages) + ",1"));

    int offset = page * ITEMS_PER_PAGE;

    int items = MIN(size - offset, ITEMS_PER_PAGE);

    for (int i = 0; i < items; i++) {

        Variant v = arr.get(i + offset);
        bool is_typed = arr.get_type() != VariantType::ARRAY || subtype != VariantType::NIL;

        if (!is_typed) {
            p_list->push_back(PropertyInfo(VariantType::INT, StringName("indices/" + itos(i + offset) + "_type"), PropertyHint::Enum, vtypes));
        }

        if (v.get_type() == VariantType::OBJECT && object_cast<EncodedObjectAsID>(v)) {
            p_list->push_back(PropertyInfo(VariantType::INT, StringName("indices/" + itos(i + offset)), PropertyHint::ObjectID, "Object"));
            continue;
        }

        if (is_typed || v.get_type() != VariantType::NIL) {
            PropertyInfo pi(v.get_type(), StringName("indices/" + itos(i + offset)));
            if (subtype != VariantType::NIL) {
                pi.type = VariantType(subtype);
                pi.hint = PropertyHint(subtype_hint);
                pi.hint_string = subtype_hint_string;
            } else if (v.get_type() == VariantType::OBJECT) {
                pi.hint = PropertyHint::ResourceType;
                pi.hint_string = "Resource";
            }

            p_list->push_back(pi);
        }
    }
}

void ArrayPropertyEdit::edit(Object *p_obj, const StringName &p_prop, se_string_view p_hint_string, VariantType p_deftype) {

    page = 0;
    property = p_prop;
    obj = p_obj->get_instance_id();
    default_type = p_deftype;

    if (!p_hint_string.empty()) {
        auto hint_subtype_separator = StringUtils::find(p_hint_string,":");
        if (hint_subtype_separator != String::npos) {
            se_string_view subtype_string = StringUtils::substr(p_hint_string,0, hint_subtype_separator);

            auto slash_pos = StringUtils::find(subtype_string,"/");
            if (slash_pos != String::npos) {
                subtype_hint = PropertyHint(StringUtils::to_int(StringUtils::substr(subtype_string,slash_pos + 1, subtype_string.size() - slash_pos - 1)));
                subtype_string = StringUtils::substr(subtype_string,0, slash_pos);
            }

            subtype_hint_string = StringUtils::substr(p_hint_string,hint_subtype_separator + 1, p_hint_string.size() - hint_subtype_separator - 1);
            subtype = VariantType(StringUtils::to_int(subtype_string));
        }
    }
}

Node *ArrayPropertyEdit::get_node() {

    return object_cast<Node>(ObjectDB::get_instance(obj));
}

bool ArrayPropertyEdit::_dont_undo_redo() {
    return true;
}

void ArrayPropertyEdit::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_set_size"), &ArrayPropertyEdit::_set_size);
    MethodBinder::bind_method(D_METHOD("_set_value"), &ArrayPropertyEdit::_set_value);
    MethodBinder::bind_method(D_METHOD("_notif_change"), &ArrayPropertyEdit::_notif_change);
    MethodBinder::bind_method(D_METHOD("_notif_changev"), &ArrayPropertyEdit::_notif_changev);
    MethodBinder::bind_method(D_METHOD("_dont_undo_redo"), &ArrayPropertyEdit::_dont_undo_redo);
}

ArrayPropertyEdit::ArrayPropertyEdit() {
    page = 0;
    for (int i = 0; i < int(VariantType::VARIANT_MAX); i++) {

        if (i > 0)
            vtypes += ',';
        vtypes += Variant::get_type_name(VariantType(i));
    }
    default_type = VariantType::NIL;
    subtype = VariantType::NIL;
    subtype_hint = PropertyHint::None;
    subtype_hint_string = "";
}
