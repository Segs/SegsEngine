/*************************************************************************/
/*  editor_properties_array_dict.cpp                                     */
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

#include "editor_properties_array_dict.h"

#include "core/callable_method_pointer.h"
#include "core/object_tooling.h"
#include "core/io/marshalls.h"
#include "core/method_bind.h"
#include "scene/resources/style_box.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "editor/editor_scale.h"
#include "editor_properties.h"

IMPL_GDCLASS(EditorPropertyArrayObject)
IMPL_GDCLASS(EditorPropertyDictionaryObject)
IMPL_GDCLASS(EditorPropertyArray)
IMPL_GDCLASS(EditorPropertyDictionary)


bool EditorPropertyArrayObject::_set(const StringName &p_name, const Variant &p_value) {

    if (StringUtils::begins_with(p_name,"indices")) {
        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        array.set(idx, p_value);
        return true;
    }

    return false;
}

bool EditorPropertyArrayObject::_get(const StringName &p_name, Variant &r_ret) const {

    if (StringUtils::begins_with(p_name,"indices")) {

        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        bool valid;
        r_ret = array.get(idx, &valid);
        if (r_ret.get_type() == VariantType::OBJECT && r_ret.asT<EncodedObjectAsID>()) {
            r_ret = Variant::from(r_ret.asT<EncodedObjectAsID>()->get_object_id());
        }

        return valid;
    }

    return false;
}

void EditorPropertyArrayObject::set_array(const Variant &p_array) {
    array = p_array;
}

Variant EditorPropertyArrayObject::get_array() {
    return array;
}

EditorPropertyArrayObject::EditorPropertyArrayObject() {
}

///////////////////

bool EditorPropertyDictionaryObject::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "new_item_key") {

        new_item_key = p_value;
        return true;
    }

    if (p_name == "new_item_value") {

        new_item_value = p_value;
        return true;
    }

    if (StringUtils::begins_with(p_name,"indices")) {
        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        Variant key = dict.get_key_at_index(idx);
        dict[key] = p_value;
        return true;
    }

    return false;
}

bool EditorPropertyDictionaryObject::_get(const StringName &p_name, Variant &r_ret) const {

    if (p_name == "new_item_key") {

        r_ret = new_item_key;
        return true;
    }

    if (p_name == "new_item_value") {

        r_ret = new_item_value;
        return true;
    }

    if (StringUtils::begins_with(p_name,"indices")) {

        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        Variant key = dict.get_key_at_index(idx);
        r_ret = dict[key];
        if (r_ret.get_type() == VariantType::OBJECT && r_ret.asT<EncodedObjectAsID>()) {
            r_ret = Variant::from(r_ret.asT<EncodedObjectAsID>()->get_object_id());
        }

        return true;
    }

    return false;
}

void EditorPropertyDictionaryObject::set_dict(const Dictionary &p_dict) {
    dict = p_dict;
}

Dictionary EditorPropertyDictionaryObject::get_dict() {
    return dict;
}

void EditorPropertyDictionaryObject::set_new_item_key(const Variant &p_new_item) {
    new_item_key = p_new_item;
}

Variant EditorPropertyDictionaryObject::get_new_item_key() {
    return new_item_key;
}

void EditorPropertyDictionaryObject::set_new_item_value(const Variant &p_new_item) {
    new_item_value = p_new_item;
}

Variant EditorPropertyDictionaryObject::get_new_item_value() {
    return new_item_value;
}

EditorPropertyDictionaryObject::EditorPropertyDictionaryObject() {
}

///////////////////// ARRAY ///////////////////////////

void EditorPropertyArray::_property_changed(const StringName &p_prop, const Variant& p_value, StringName p_name, bool changing) {

    if (StringUtils::begins_with(p_prop,"indices")) {
        int idx = StringUtils::to_int(StringUtils::get_slice(p_prop,"/", 1));
        Variant array = object->get_array();
        array.set(idx, p_value);
        emit_changed(get_edited_property(), array, "", true);

        if (array.get_type() == VariantType::ARRAY) {
            array = VariantOps::duplicate(array); //dupe, so undo/redo works better
        }
        object->set_array(array);
    }
}

void EditorPropertyArray::_change_type(Object *p_button, int p_index) {

    Button *button = object_cast<Button>(p_button);
    changing_type_idx = p_index;
    Rect2 rect = button->get_global_rect();
    change_type->set_as_minsize();
    change_type->set_global_position(rect.position + rect.size - Vector2(change_type->get_combined_minimum_size().x, 0));
    change_type->popup();
}

void EditorPropertyArray::_change_type_menu(int p_index) {

    if (p_index == (int)VariantType::VARIANT_MAX) {
        _remove_pressed(changing_type_idx);
        return;
    }

    Variant value = Variant::construct_default(VariantType(p_index));
    Variant array = object->get_array();
    array.set(changing_type_idx, value);

    emit_changed(get_edited_property(), array, "", true);

    if (array.get_type() == VariantType::ARRAY) {
        array = VariantOps::duplicate(array); //dupe, so undo/redo works better
    }

    object->set_array(array);
    update_property();
}

void EditorPropertyArray::_object_id_selected(StringView p_property, ObjectID p_id) {
    emit_signal("object_id_selected", p_property, Variant::from(p_id));
}

void EditorPropertyArray::update_property() {

    Variant array = get_edited_object()->get(get_edited_property());

    StringName arrtype;
    switch (array_type) {
        case VariantType::ARRAY: {
            arrtype = "Array";

        } break;

        // arrays
        case VariantType::POOL_BYTE_ARRAY: {
            arrtype = "PoolByteArray";

        } break;
        case VariantType::POOL_INT_ARRAY: {
            arrtype = "PoolIntArray";

        } break;
        case VariantType::POOL_REAL_ARRAY: {

            arrtype = "PoolFloatArray";
        } break;
        case VariantType::POOL_STRING_ARRAY: {

            arrtype = "PoolStringArray";
        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {

            arrtype = "PoolVector2Array";
        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {
            arrtype = "PoolVector3Array";

        } break;
        case VariantType::POOL_COLOR_ARRAY: {
            arrtype = "PoolColorArray";
        } break;
        default: {
        }
    }

    if (array.get_type() == VariantType::NIL) {
        edit->set_text_utf8(String("(Nil) ") + arrtype);
        edit->set_pressed(false);
        if (vbox) {
            set_bottom_editor(nullptr);
            memdelete(vbox);
            vbox = nullptr;
        }
        return;
    }

    edit->set_text_utf8(String(arrtype) + " (size " + itos(VariantOps::size(array)) + ")");

    bool unfolded = get_edited_object()->get_tooling_interface()->editor_is_section_unfolded(get_edited_property());
    if (edit->is_pressed() != unfolded) {
        edit->set_pressed(unfolded);
    }

    if (unfolded) {

        updating = true;

        if (!vbox) {

            vbox = memnew(VBoxContainer);
            add_child(vbox);
            set_bottom_editor(vbox);
            HBoxContainer *hbc = memnew(HBoxContainer);
            vbox->add_child(hbc);
            Label *label = memnew(Label(TTR("Size: ")));
            label->set_h_size_flags(SIZE_EXPAND_FILL);
            hbc->add_child(label);
            length = memnew(EditorSpinSlider);
            length->set_step(1);
            length->set_max(1000000);
            length->set_h_size_flags(SIZE_EXPAND_FILL);
            hbc->add_child(length);
            length->connect("value_changed",callable_mp(this, &ClassName::_length_changed));

            page_hb = memnew(HBoxContainer);
            vbox->add_child(page_hb);
            label = memnew(Label(TTR("Page: ")));
            label->set_h_size_flags(SIZE_EXPAND_FILL);
            page_hb->add_child(label);
            page = memnew(EditorSpinSlider);
            page->set_step(1);
            page_hb->add_child(page);
            page->set_h_size_flags(SIZE_EXPAND_FILL);
            page->connect("value_changed",callable_mp(this, &ClassName::_page_changed));
        } else {
            //bye bye children of the box
            while (vbox->get_child_count() > 2) {
                vbox->get_child(2)->queue_delete(); // button still needed after pressed is called
                vbox->remove_child(vbox->get_child(2));
            }
        }

        int len = VariantOps::size(array);

        length->set_value(len);

        int pages = M_MAX(0, len - 1) / page_len + 1;

        page->set_max(pages);
        page_idx = MIN(page_idx, pages - 1);
        page->set_value(page_idx);
        page_hb->set_visible(pages > 1);

        int offset = page_idx * page_len;

        int amount = MIN(len - offset, page_len);

        if (array.get_type() == VariantType::ARRAY) {
            array = VariantOps::duplicate(array);
        }

        object->set_array(array);

        for (int i = 0; i < amount; i++) {
            StringName prop_name("indices/" + itos(i + offset));

            EditorProperty *prop = nullptr;
            Variant value = array.get(i + offset);
            VariantType value_type = value.get_type();

            if (value_type == VariantType::NIL && subtype != VariantType::NIL) {
                value_type = subtype;
            }

            if (value_type == VariantType::OBJECT && value.asT<EncodedObjectAsID>()) {
                EditorPropertyObjectID *editor = memnew(EditorPropertyObjectID);
                editor->setup("Object");
                prop = editor;
            } else {
                prop = EditorInspector::instantiate_property_editor(nullptr, value_type, {}, subtype_hint, subtype_hint_string, 0);
            }

            prop->set_object_and_property(object.get(), prop_name);
            prop->set_label(itos(i + offset));
            prop->set_selectable(false);
            prop->connect("property_changed",callable_mp(this, &ClassName::_property_changed));
            prop->connect("object_id_selected",callable_mp(this, &ClassName::_object_id_selected));
            prop->set_h_size_flags(SIZE_EXPAND_FILL);

            HBoxContainer *hb = memnew(HBoxContainer);

            vbox->add_child(hb);
            hb->add_child(prop);

            bool is_untyped_array = array.get_type() == VariantType::ARRAY && subtype == VariantType::NIL;

            if (is_untyped_array) {

                Button *edit = memnew(Button);
                edit->set_button_icon(get_icon("Edit", "EditorIcons"));
                hb->add_child(edit);
                edit->connect("pressed",callable_mp(this, &ClassName::_change_type), varray(Variant(edit), i + offset));
            } else {

                Button *remove = memnew(Button);
                remove->set_button_icon(get_icon("Remove", "EditorIcons"));
                remove->connect("pressed",callable_mp(this, &ClassName::_remove_pressed), varray(i + offset));
                hb->add_child(remove);
            }

            prop->update_property();
        }

        updating = false;

    } else {
        if (vbox) {
            set_bottom_editor(nullptr);
            memdelete(vbox);
            vbox = nullptr;
        }
    }
}

void EditorPropertyArray::_remove_pressed(int p_index) {

    Variant array = object->get_array();

    VariantOps::remove(array,p_index);

    if (array.get_type() == VariantType::ARRAY) {
        array = VariantOps::duplicate(array);
    }

    emit_changed(get_edited_property(), array, "", false);
    object->set_array(array);
    update_property();
}

void EditorPropertyArray::_notification(int p_what) {
}
void EditorPropertyArray::_edit_pressed() {

    Variant array = get_edited_object()->get(get_edited_property());
    if (!array.is_array()) {
        array = Variant::construct_default(array_type);

        get_edited_object()->set(get_edited_property(), array);
    }

    get_edited_object()->get_tooling_interface()->editor_set_section_unfold(get_edited_property(), edit->is_pressed());
    update_property();
}

void EditorPropertyArray::_page_changed(double p_page) {
    if (updating)
        return;
    page_idx = p_page;
    update_property();
}

void EditorPropertyArray::_length_changed(double p_page) {
    if (updating)
        return;

    Variant array = object->get_array();
    int previous_size = VariantOps::size(array);
    VariantOps::resize(array,int(p_page));

    if (array.get_type() == VariantType::ARRAY) {
        if (subtype != VariantType::NIL) {
            int size = VariantOps::size(array);
            for (int i = previous_size; i < size; i++) {
                if (array.get(i).get_type() == VariantType::NIL) {
                    array.set(i, Variant::construct_default(subtype));
                }
            }
        }
        array = VariantOps::duplicate(array); //dupe, so undo/redo works better
    } else {
        int size = VariantOps::size(array);
        // Pool*Array don't initialize their elements, have to do it manually
        for (int i = previous_size; i < size; i++) {
            array.set(i, Variant::construct_default(array.get(i).get_type()));
        }
    }

    emit_changed(get_edited_property(), array, "", false);
    object->set_array(array);
    update_property();
}

void EditorPropertyArray::setup(VariantType p_array_type, StringView p_hint_string) {

    array_type = p_array_type;

    if (array_type == VariantType::ARRAY && !p_hint_string.empty()) {
        auto hint_subtype_separator = StringUtils::find(p_hint_string,":");
        if (hint_subtype_separator != String::npos) {
            StringView subtype_string = StringUtils::substr(p_hint_string,0, hint_subtype_separator);
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

void EditorPropertyArray::_bind_methods() {
    MethodBinder::bind_method("_edit_pressed", &EditorPropertyArray::_edit_pressed);
    MethodBinder::bind_method("_page_changed", &EditorPropertyArray::_page_changed);
    MethodBinder::bind_method("_length_changed", &EditorPropertyArray::_length_changed);
    MethodBinder::bind_method("_property_changed", &EditorPropertyArray::_property_changed, {DEFVAL(StringView()), DEFVAL(false)});
    MethodBinder::bind_method("_change_type", &EditorPropertyArray::_change_type);
    MethodBinder::bind_method("_change_type_menu", &EditorPropertyArray::_change_type_menu);
    MethodBinder::bind_method("_object_id_selected", &EditorPropertyArray::_object_id_selected);
    MethodBinder::bind_method("_remove_pressed", &EditorPropertyArray::_remove_pressed);
}

EditorPropertyArray::EditorPropertyArray() {

    object = make_ref_counted<EditorPropertyArrayObject>();
    page_idx = 0;
    page_len = 10;
    edit = memnew(Button);
    edit->set_flat(true);
    edit->set_h_size_flags(SIZE_EXPAND_FILL);
    edit->set_clip_text(true);
    edit->connect("pressed",callable_mp(this, &ClassName::_edit_pressed));
    edit->set_toggle_mode(true);
    add_child(edit);
    add_focusable(edit);
    vbox = nullptr;
    page = nullptr;
    length = nullptr;
    updating = false;
    change_type = memnew(PopupMenu);
    add_child(change_type);
    change_type->connect("id_pressed",callable_mp(this, &ClassName::_change_type_menu));

    for (int i = 0; i < (int)VariantType::VARIANT_MAX; i++) {
        StringName type(Variant::interned_type_name(VariantType(i)));
        change_type->add_item(type, i);
    }
    change_type->add_separator();
    change_type->add_item(TTR("Remove Item"), (int)VariantType::VARIANT_MAX);
    changing_type_idx = -1;

    subtype = VariantType::NIL;
    subtype_hint = PropertyHint::None;
    subtype_hint_string = "";
}

///////////////////// DICTIONARY ///////////////////////////

void EditorPropertyDictionary::_property_changed(const StringName &p_prop, const Variant& p_value, StringName p_name, bool changing) {

    if (p_prop == "new_item_key") {

        object->set_new_item_key(p_value);
    } else if (p_prop == "new_item_value") {

        object->set_new_item_value(p_value);
    } else if (StringUtils::begins_with(p_prop,"indices")) {
        int idx = StringUtils::to_int(StringUtils::get_slice(p_prop,"/", 1));
        Dictionary dict = object->get_dict();
        Variant key = dict.get_key_at_index(idx);
        dict[key] = p_value;

        emit_changed(get_edited_property(), dict, "", true);

        dict = dict.duplicate(); //dupe, so undo/redo works better
        object->set_dict(dict);
    }
}

void EditorPropertyDictionary::_change_type(Object *p_button, int p_index) {

    Button *button = object_cast<Button>(p_button);

    Rect2 rect = button->get_global_rect();
    change_type->set_as_minsize();
    change_type->set_global_position(rect.position + rect.size - Vector2(change_type->get_combined_minimum_size().x, 0));
    change_type->popup();
    changing_type_idx = p_index;
}

void EditorPropertyDictionary::_add_key_value() {

    // Do not allow nil as valid key. I experienced errors with this
    if (object->get_new_item_key().get_type() == VariantType::NIL) {
        return;
    }

    Dictionary dict = object->get_dict();

    dict[object->get_new_item_key()] = object->get_new_item_value();
    object->set_new_item_key(Variant());
    object->set_new_item_value(Variant());

    emit_changed(get_edited_property(), dict, "", false);

    dict = dict.duplicate(); //dupe, so undo/redo works better
    object->set_dict(dict);
    update_property();
}

void EditorPropertyDictionary::_change_type_menu(int p_index) {

    if (changing_type_idx < 0) {
        Variant value = Variant::construct_default(VariantType(p_index));
        if (changing_type_idx == -1) {
            object->set_new_item_key(value);
        } else {
            object->set_new_item_value(value);
        }
        update_property();
        return;
    }

    Dictionary dict = object->get_dict();

    if (p_index < (int)VariantType::VARIANT_MAX) {

        Variant value = Variant::construct_default(VariantType(p_index));
        Variant key = dict.get_key_at_index(changing_type_idx);
        dict[key] = value;
    } else {
        Variant key = dict.get_key_at_index(changing_type_idx);
        dict.erase(key);
    }

    emit_changed(get_edited_property(), dict, "", false);

    dict = dict.duplicate(); //dupe, so undo/redo works better
    object->set_dict(dict);
    update_property();
}

void EditorPropertyDictionary::update_property() {

    Variant updated_val = get_edited_object()->get(get_edited_property());

    if (updated_val.get_type() == VariantType::NIL) {
        edit->set_text("Dictionary (Nil)"); //This provides symmetry with the array property.
        edit->set_pressed(false);
        if (vbox) {
            set_bottom_editor(nullptr);
            memdelete(vbox);
            vbox = nullptr;
        }
        return;
    }

    Dictionary dict = updated_val.as<Dictionary>();

    edit->set_text_utf8(String("Dictionary (size ") + itos(dict.size()) + ")");

    bool unfolded = get_edited_object()->get_tooling_interface()->editor_is_section_unfolded(get_edited_property());
    if (edit->is_pressed() != unfolded) {
        edit->set_pressed(unfolded);
    }

    if (unfolded) {

        updating = true;

        if (!vbox) {

            vbox = memnew(VBoxContainer);
            add_child(vbox);
            set_bottom_editor(vbox);

            page_hb = memnew(HBoxContainer);
            vbox->add_child(page_hb);
            Label *label = memnew(Label(TTR("Page: ")));
            label->set_h_size_flags(SIZE_EXPAND_FILL);
            page_hb->add_child(label);
            page = memnew(EditorSpinSlider);
            page->set_step(1);
            page_hb->add_child(page);
            page->set_h_size_flags(SIZE_EXPAND_FILL);
            page->connect("value_changed",callable_mp(this, &ClassName::_page_changed));
        } else {
            // Queue children for deletion, deleting immediately might cause errors.
            for (int i = 1; i < vbox->get_child_count(); i++) {
                vbox->get_child(i)->queue_delete();
            }
        }

        int len = dict.size();

        int pages = M_MAX(0, len - 1) / page_len + 1;

        page->set_max(pages);
        page_idx = MIN(page_idx, pages - 1);
        page->set_value(page_idx);
        page_hb->set_visible(pages > 1);

        int offset = page_idx * page_len;

        int amount = MIN(len - offset, page_len);

        dict = dict.duplicate();

        object->set_dict(dict);
        VBoxContainer *add_vbox = nullptr;

        for (int i = 0; i < amount + 2; i++) {
            StringName prop_name;
            Variant key;
            Variant value;

            if (i < amount) {
                prop_name = StringName("indices/" + itos(i + offset));
                key = dict.get_key_at_index(i + offset);
                value = dict.get_value_at_index(i + offset);
            } else if (i == amount) {
                prop_name = "new_item_key";
                value = object->get_new_item_key();
            } else if (i == amount + 1) {
                prop_name = "new_item_value";
                value = object->get_new_item_value();
            }

            EditorProperty *prop = nullptr;

            switch (value.get_type()) {
                case VariantType::NIL: {
                    prop = memnew(EditorPropertyNil);

                } break;

                // atomic types
                case VariantType::BOOL: {

                    prop = memnew(EditorPropertyCheck);

                } break;
                case VariantType::INT: {
                    EditorPropertyInteger *editor = memnew(EditorPropertyInteger);
                    editor->setup(-100000, 100000, 1, true, true);
                    prop = editor;

                } break;
                case VariantType::FLOAT: {

                    EditorPropertyFloat *editor = memnew(EditorPropertyFloat);
                    editor->setup(-100000, 100000, 0.001, true, false, true, true);
                    prop = editor;
                } break;
                case VariantType::STRING: {

                    prop = memnew(EditorPropertyText);

                } break;

                // math types
                case VariantType::VECTOR2: {

                    EditorPropertyVector2 *editor = memnew(EditorPropertyVector2);
                    editor->setup(-100000, 100000, 0.001, true);
                    prop = editor;

                } break;
                case VariantType::RECT2: {

                    EditorPropertyRect2 *editor = memnew(EditorPropertyRect2);
                    editor->setup(-100000, 100000, 0.001, true);
                    prop = editor;

                } break;
                case VariantType::VECTOR3: {

                    EditorPropertyVector3 *editor = memnew(EditorPropertyVector3);
                    editor->setup(-100000, 100000, 0.001, true);
                    prop = editor;

                } break;
                case VariantType::TRANSFORM2D: {

                    EditorPropertyTransform2D *editor = memnew(EditorPropertyTransform2D);
                    editor->setup(-100000, 100000, 0.001, true);
                    prop = editor;

                } break;
                case VariantType::PLANE: {

                    EditorPropertyPlane *editor = memnew(EditorPropertyPlane);
                    editor->setup(-100000, 100000, 0.001, true);
                    prop = editor;

                } break;
                case VariantType::QUAT: {

                    EditorPropertyQuat *editor = memnew(EditorPropertyQuat);
                    editor->setup(-100000, 100000, 0.001, true);
                    prop = editor;

                } break;
                case VariantType::AABB: {

                    EditorPropertyAABB *editor = memnew(EditorPropertyAABB);
                    editor->setup(-100000, 100000, 0.001, true);
                    prop = editor;

                } break;
                case VariantType::BASIS: {
                    EditorPropertyBasis *editor = memnew(EditorPropertyBasis);
                    editor->setup(-100000, 100000, 0.001, true);
                    prop = editor;

                } break;
                case VariantType::TRANSFORM: {
                    EditorPropertyTransform *editor = memnew(EditorPropertyTransform);
                    editor->setup(-100000, 100000, 0.001, true);
                    prop = editor;

                } break;

                // misc types
                case VariantType::COLOR: {
                    prop = memnew(EditorPropertyColor);

                } break;
                case VariantType::STRING_NAME: {
                    EditorPropertyText *ept = memnew(EditorPropertyText);
                    ept->set_string_name(true);
                    prop = ept;

                } break;
                case VariantType::NODE_PATH: {
                    prop = memnew(EditorPropertyNodePath);

                } break;
                case VariantType::_RID: {
                    prop = memnew(EditorPropertyRID);

                } break;
                case VariantType::OBJECT: {

                    if (value.asT<EncodedObjectAsID>()) {

                        EditorPropertyObjectID *editor = memnew(EditorPropertyObjectID);
                        editor->setup("Object");
                        prop = editor;

                    } else {

                        EditorPropertyResource *editor = memnew(EditorPropertyResource);
                        editor->setup("Resource");
                        prop = editor;
                    }

                } break;
                case VariantType::DICTIONARY: {
                    prop = memnew(EditorPropertyDictionary);

                } break;
                case VariantType::ARRAY: {
                    EditorPropertyArray *editor = memnew(EditorPropertyArray);
                    editor->setup(VariantType::ARRAY);
                    prop = editor;
                } break;

                // arrays
                case VariantType::POOL_BYTE_ARRAY: {

                    EditorPropertyArray *editor = memnew(EditorPropertyArray);
                    editor->setup(VariantType::POOL_BYTE_ARRAY);
                    prop = editor;
                } break;
                case VariantType::POOL_INT_ARRAY: {

                    EditorPropertyArray *editor = memnew(EditorPropertyArray);
                    editor->setup(VariantType::POOL_INT_ARRAY);
                    prop = editor;
                } break;
                case VariantType::POOL_REAL_ARRAY: {

                    EditorPropertyArray *editor = memnew(EditorPropertyArray);
                    editor->setup(VariantType::POOL_REAL_ARRAY);
                    prop = editor;
                } break;
                case VariantType::POOL_STRING_ARRAY: {

                    EditorPropertyArray *editor = memnew(EditorPropertyArray);
                    editor->setup(VariantType::POOL_STRING_ARRAY);
                    prop = editor;
                } break;
                case VariantType::POOL_VECTOR2_ARRAY: {

                    EditorPropertyArray *editor = memnew(EditorPropertyArray);
                    editor->setup(VariantType::POOL_VECTOR2_ARRAY);
                    prop = editor;
                } break;
                case VariantType::POOL_VECTOR3_ARRAY: {

                    EditorPropertyArray *editor = memnew(EditorPropertyArray);
                    editor->setup(VariantType::POOL_VECTOR3_ARRAY);
                    prop = editor;
                } break;
                case VariantType::POOL_COLOR_ARRAY: {

                    EditorPropertyArray *editor = memnew(EditorPropertyArray);
                    editor->setup(VariantType::POOL_COLOR_ARRAY);
                    prop = editor;
                } break;
                default: {
                }
            }

            if (i == amount) {
                PanelContainer *pc = memnew(PanelContainer);
                vbox->add_child(pc);
                Ref<StyleBoxFlat> flat(make_ref_counted<StyleBoxFlat>());
                for (int j = 0; j < 4; j++) {
                    flat->set_default_margin(Margin(j), 2 * EDSCALE);
                }
                flat->set_bg_color(get_color("prop_subsection", "Editor"));

                pc->add_style_override("panel", flat);
                add_vbox = memnew(VBoxContainer);
                pc->add_child(add_vbox);
            }
            prop->set_object_and_property(object.get(), prop_name);
            int change_index = 0;

            if (i < amount) {
                String cs = key.get_construct_string();
                prop->set_label(key.get_construct_string());
                prop->set_tooltip_utf8(cs);
                change_index = i + offset;
            } else if (i == amount) {
                prop->set_label(TTR("New Key:").asCString());
                change_index = -1;
            } else if (i == amount + 1) {
                prop->set_label(TTR("New Value:").asCString());
                change_index = -2;
            }

            prop->set_selectable(false);
            prop->connect("property_changed",callable_mp(this, &ClassName::_property_changed));
            prop->connect("object_id_selected",callable_mp(this, &ClassName::_object_id_selected));

            HBoxContainer *hb = memnew(HBoxContainer);
            if (add_vbox) {
                add_vbox->add_child(hb);
            } else {
                vbox->add_child(hb);
            }
            hb->add_child(prop);
            prop->set_h_size_flags(SIZE_EXPAND_FILL);
            Button *edit = memnew(Button);
            edit->set_button_icon(get_icon("Edit", "EditorIcons"));
            hb->add_child(edit);
            edit->connect("pressed",callable_mp(this, &ClassName::_change_type), varray(Variant(edit), change_index));

            prop->update_property();

            if (i == amount + 1) {
                Button *butt_add_item = memnew(Button);
                butt_add_item->set_text(TTR("Add Key/Value Pair"));
                butt_add_item->connect("pressed",callable_mp(this, &ClassName::_add_key_value));
                add_vbox->add_child(butt_add_item);
            }
        }

        updating = false;

    } else {
        if (vbox) {
            set_bottom_editor(nullptr);
            memdelete(vbox);
            vbox = nullptr;
        }
    }
}

void EditorPropertyDictionary::_object_id_selected(StringView p_property, ObjectID p_id) {
    emit_signal("object_id_selected", p_property, Variant::from(p_id));
}

void EditorPropertyDictionary::_notification(int p_what) {
}

void EditorPropertyDictionary::_edit_pressed() {

    Variant prop_val = get_edited_object()->get(get_edited_property());
    if (prop_val.get_type() == VariantType::NIL) {
        prop_val = Variant::construct_default(VariantType::DICTIONARY);
        get_edited_object()->set(get_edited_property(), prop_val);
    }

    get_edited_object()->get_tooling_interface()->editor_set_section_unfold(get_edited_property(), edit->is_pressed());
    update_property();
}

void EditorPropertyDictionary::_page_changed(double p_page) {
    if (updating)
        return;
    page_idx = p_page;
    update_property();
}

void EditorPropertyDictionary::_bind_methods() {
    MethodBinder::bind_method("_edit_pressed", &EditorPropertyDictionary::_edit_pressed);
    MethodBinder::bind_method("_page_changed", &EditorPropertyDictionary::_page_changed);
    MethodBinder::bind_method("_property_changed", &EditorPropertyDictionary::_property_changed, {DEFVAL(StringView()), DEFVAL(false)});
    MethodBinder::bind_method("_change_type", &EditorPropertyDictionary::_change_type);
    MethodBinder::bind_method("_change_type_menu", &EditorPropertyDictionary::_change_type_menu);
    MethodBinder::bind_method("_add_key_value", &EditorPropertyDictionary::_add_key_value);
    MethodBinder::bind_method("_object_id_selected", &EditorPropertyDictionary::_object_id_selected);
}

EditorPropertyDictionary::EditorPropertyDictionary() {

    object = make_ref_counted<EditorPropertyDictionaryObject>();
    page_idx = 0;
    page_len = 10;
    edit = memnew(Button);
    edit->set_flat(true);
    edit->set_h_size_flags(SIZE_EXPAND_FILL);
    edit->set_clip_text(true);
    edit->connect("pressed",callable_mp(this, &ClassName::_edit_pressed));
    edit->set_toggle_mode(true);
    add_child(edit);
    add_focusable(edit);
    vbox = nullptr;
    page = nullptr;
    updating = false;
    change_type = memnew(PopupMenu);
    add_child(change_type);
    change_type->connect("id_pressed",callable_mp(this, &ClassName::_change_type_menu));

    for (int i = 0; i < (int)VariantType::VARIANT_MAX; i++) {
        StringName type(Variant::interned_type_name(VariantType(i)));
        change_type->add_item(type, i);
    }
    change_type->add_separator();
    change_type->add_item(TTR("Remove Item"), (int)VariantType::VARIANT_MAX);
    changing_type_idx = -1;
}
