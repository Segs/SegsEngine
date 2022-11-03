/*************************************************************************/
/*  property_selector.cpp                                                */
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

#include "property_selector.h"

#include "core/callable_method_pointer.h"
#include "core/doc_support/doc_data.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/os/keyboard.h"
#include "core/string_formatter.h"
#include "editor/editor_node.h"
#include "editor/editor_help.h"

#include "editor_scale.h"

IMPL_GDCLASS(PropertySelector)

void PropertySelector::_text_changed(StringView p_newtext) {

    _update_search();
}

void PropertySelector::_sbox_input(const Ref<InputEvent> &p_ie) {

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_ie);

    if (k) {

        switch (k->get_keycode()) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_PAGEUP:
            case KEY_PAGEDOWN: {

                search_options->call_va("_gui_input", k);
                search_box->accept_event();

                TreeItem *root = search_options->get_root();
                if (!root->get_children())
                    break;

                TreeItem *current = search_options->get_selected();

                TreeItem *item = search_options->get_next_selected(root);
                while (item) {
                    item->deselect(0);
                    item = search_options->get_next_selected(item);
                }

                current->select(0);

            } break;
        }
    }
}

void PropertySelector::_update_search() {

    if (properties)
        set_title(TTR("Select Property"));
    else if (virtuals_only)
        set_title(TTR("Select Virtual Method"));
    else
        set_title(TTR("Select Method"));

    search_options->clear();
    help_bit->set_text("");

    TreeItem *root = search_options->create_item();

    // Allow using spaces in place of underscores in the search string (makes the search more fault-tolerant).
    const String search_text = search_box->get_text().replaced(" ", "_");

    if (properties) {

        Vector<PropertyInfo> props;

        if (instance) {
            instance->get_property_list(&props, true);
        } else if (type != VariantType::NIL) {
            Variant v = Variant::construct_default(type);

            v.get_property_list(&props);
        } else {

            Object *obj = object_for_entity(script);
            if (object_cast<Script>(obj)) {

                props.push_back(PropertyInfo(VariantType::NIL, "Script Variables", PropertyHint::None, "", PROPERTY_USAGE_CATEGORY));
                object_cast<Script>(obj)->get_script_property_list(&props);
            }

            StringName base(base_type);
            while (base) {
                props.push_back(PropertyInfo(VariantType::NIL, StringName(base), PropertyHint::None, "", PROPERTY_USAGE_CATEGORY));
                ClassDB::get_property_list(base, &props, true);
                base = ClassDB::get_parent_class(base);
            }
        }

        TreeItem *category = nullptr;

        bool found = false;

        const Ref<Texture> type_icons[(int)VariantType::VARIANT_MAX] = {
            Control::get_theme_icon("Variant", "EditorIcons"),
            Control::get_theme_icon("bool", "EditorIcons"),
            Control::get_theme_icon("int", "EditorIcons"),
            Control::get_theme_icon("float", "EditorIcons"),
            Control::get_theme_icon("String", "EditorIcons"),
            Control::get_theme_icon("Vector2", "EditorIcons"),
            Control::get_theme_icon("Rect2", "EditorIcons"),
            Control::get_theme_icon("Vector3", "EditorIcons"),
            Control::get_theme_icon("Transform2D", "EditorIcons"),
            Control::get_theme_icon("Plane", "EditorIcons"),
            Control::get_theme_icon("Quat", "EditorIcons"),
            Control::get_theme_icon("AABB", "EditorIcons"),
            Control::get_theme_icon("Basis", "EditorIcons"),
            Control::get_theme_icon("Transform", "EditorIcons"),
            Control::get_theme_icon("Color", "EditorIcons"),
            Control::get_theme_icon("Path", "EditorIcons"),
            Control::get_theme_icon("RID", "EditorIcons"),
            Control::get_theme_icon("Object", "EditorIcons"),
            Control::get_theme_icon("Dictionary", "EditorIcons"),
            Control::get_theme_icon("Array", "EditorIcons"),
            Control::get_theme_icon("PoolByteArray", "EditorIcons"),
            Control::get_theme_icon("PoolIntArray", "EditorIcons"),
            Control::get_theme_icon("PoolRealArray", "EditorIcons"),
            Control::get_theme_icon("PoolStringArray", "EditorIcons"),
            Control::get_theme_icon("PoolVector2Array", "EditorIcons"),
            Control::get_theme_icon("PoolVector3Array", "EditorIcons"),
            Control::get_theme_icon("PoolColorArray", "EditorIcons")
        };

        for (const PropertyInfo &E : props) {
            if (E.usage == PROPERTY_USAGE_CATEGORY) {
                if (category && category->get_children() == nullptr) {
                    memdelete(category); //old category was unused
                }
                category = search_options->create_item(root);
                category->set_text_utf8(0, E.name);
                category->set_selectable(0, false);

                Ref<Texture> icon;
                if (E.name == "Script Variables") {
                    icon = get_theme_icon("Script", "EditorIcons");
                } else {
                    icon = EditorNode::get_singleton()->get_class_icon(E.name);
                }
                category->set_icon(0, icon);
                continue;
            }

            if (!(E.usage & PROPERTY_USAGE_EDITOR) && !(E.usage & PROPERTY_USAGE_SCRIPT_VARIABLE))
                continue;

            if (!search_text.empty() && not StringUtils::contains(E.name,search_text))
                continue;

            if (!type_filter.empty() && !type_filter.contains(E.type))
                continue;

            TreeItem *item = search_options->create_item(category ? category : root);
            item->set_text_utf8(0, E.name);
            item->set_metadata(0, E.name);
            item->set_icon(0, type_icons[(int)E.type]);

            if (!found && !search_text.empty() && StringUtils::contains(E.name,search_text)) {
                item->select(0);
                found = true;
            }

            item->set_selectable(0, true);
        }

        if (category && category->get_children() == nullptr) {
            memdelete(category); //old category was unused
        }
    } else {

        Vector<MethodInfo> methods;

        if (type != VariantType::NIL) {
            Variant v = Variant::construct_default(type);
            //v.get_method_list(&methods);
        } else {

            Object *obj = object_for_entity(script);
            if (object_cast<Script>(obj)) {

                methods.push_back(MethodInfo("*Script Methods"));
                object_cast<Script>(obj)->get_script_method_list(&methods);
            }

            StringName base(base_type);
            while (base) {
                methods.push_back(MethodInfo((String("*") + base_type).c_str()));
                ClassDB::get_method_list(base, &methods, true, true);
                base = ClassDB::get_parent_class(base);
            }
        }

        TreeItem *category = nullptr;

        bool found = false;
        bool script_methods = false;

        for (const MethodInfo &E : methods) {
            if (StringUtils::begins_with(E.name,"*")) {
                if (category && category->get_children() == nullptr) {
                    memdelete(category); //old category was unused
                }
                category = search_options->create_item(root);
                category->set_text_utf8(0, StringUtils::replace_first(E.name,"*", ""));
                category->set_selectable(0, false);

                Ref<Texture> icon;
                script_methods = false;
                String rep = StringUtils::replace(E.name,"*", "");
                if (E.name == "*Script Methods") {
                    icon = get_theme_icon("Script", "EditorIcons");
                    script_methods = true;
                } else {
                    icon = EditorNode::get_singleton()->get_class_icon(StringName(rep));
                }
                category->set_icon(0, icon);

                continue;
            }

            StringView name = StringUtils::get_slice(E.name,":", 0);
            if (!script_methods && StringUtils::begins_with(name,"_") && !(E.flags & METHOD_FLAG_VIRTUAL))
                continue;

            if (virtuals_only && !(E.flags & METHOD_FLAG_VIRTUAL)) {
                continue;
            }
            if (!virtuals_only && E.flags & METHOD_FLAG_VIRTUAL) {
                continue;
            }

            if (!search_text.empty() && not StringUtils::contains(name,search_text))
                continue;

            TreeItem *item = search_options->create_item(category ? category : root);

            MethodInfo mi = E;

            String desc;
            if (StringUtils::contains(mi.name,":")) {
                desc = String(StringUtils::get_slice(mi.name,":", 1)) + " ";
                mi.name = StringName(StringUtils::get_slice(mi.name,":", 0));
            } else if (mi.return_val.type != VariantType::NIL) {
                desc = Variant::get_type_name(mi.return_val.type);
            }
            else {
                desc = "void ";
            }

            desc += String(String::CtorSprintf()," %s(",mi.name.asCString());

            for (size_t i = 0; i < mi.arguments.size(); i++) {

                if (i > 0)
                    desc += ", ";

                desc += mi.arguments[i].name;

                if (mi.arguments[i].type == VariantType::NIL)
                    desc += ": Variant";
                else if (StringUtils::contains(mi.arguments[i].name,":")) {
                    desc += ": "+StringUtils::get_slice(mi.arguments[i].name,":", 1);
                    mi.arguments[i].name = StringName(StringUtils::get_slice(mi.arguments[i].name,":", 0));
                } else {
                    desc += ": "+String(Variant::get_type_name(mi.arguments[i].type));
                }

            }

            desc += " )";

            if (E.flags & METHOD_FLAG_CONST)
                desc += " const";

            if (E.flags & METHOD_FLAG_VIRTUAL)
                desc += " virtual";

            item->set_text_utf8(0, desc);
            item->set_metadata(0, name);
            item->set_selectable(0, true);

            if (!found && !search_text.empty() && StringUtils::contains(name,search_text)) {
                item->select(0);
                found = true;
            }
        }

        if (category && category->get_children() == nullptr) {
            memdelete(category); //old category was unused
        }
    }

    get_ok()->set_disabled(root->get_children() == nullptr);
}

void PropertySelector::_confirmed() {

    TreeItem *ti = search_options->get_selected();
    if (!ti)
        return;
    emit_signal("selected", ti->get_metadata(0));
    hide();
}

void PropertySelector::_item_selected() {

    help_bit->set_text("");

    TreeItem *item = search_options->get_selected();
    if (!item)
        return;
    String name = item->get_metadata(0).as<String>();

    StringName class_type;
    if (type != VariantType::NIL) {
        class_type = Variant::interned_type_name(type);
    } else if (!base_type.empty()) {
        class_type = base_type;
    } else if (instance) {
        class_type = instance->get_class_name();
    }

    DocData *dd = EditorHelp::get_doc_data();
    String text;

    if (properties) {
        while (!class_type.empty()) {
            auto E = dd->class_list.find_as(class_type);
            if (dd->class_list.end()!=E) {
                for (int i = 0; i < E->second.properties.size(); i++) {
                    if (E->second.properties[i].name == name) {
                        text = DTR(E->second.properties[i].description);
                        break;
                    }
                }
            }

            if (text != String()) {
                break;
            }

            // The property may be from a parent class, keep looking.
            class_type = ClassDB::get_parent_class(class_type);
        }
    } else {
        while (class_type != String()) {
            auto E = dd->class_list.find_as(class_type);
            if (dd->class_list.end()!=E) {
                for (int i = 0; i < E->second.methods.size(); i++) {
                    if (E->second.methods[i].name == name) {
                        text = DTR(E->second.description);
                        break;
                    }
                }
            }

            if (!text.empty()) {
                break;
            }

            // The method may be from a parent class, keep looking.
            class_type = ClassDB::get_parent_class(class_type);
        }
    }

    if (text != String()) {
        // Display both property name and description, since the help bit may be displayed
        // far away from the location (especially if the dialog was resized to be taller).
        help_bit->set_text(FormatVE("[b]%s[/b]: %s", name.c_str(), text.c_str()));
        help_bit->get_rich_text()->set_self_modulate(Color(1, 1, 1, 1));
    } else {
        // Use nested `vformat()` as translators shouldn't interfere with BBCode tags.
        help_bit->set_text(FormatVE(TTR("No description available for %s.").asCString(), FormatVE("[b]%s[/b]", name.c_str()).c_str()));
        help_bit->get_rich_text()->set_self_modulate(Color(1, 1, 1, 0.5));
    }
}


void PropertySelector::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {

        connect("confirmed",callable_mp(this, &ClassName::_confirmed));
    } else if (p_what == NOTIFICATION_EXIT_TREE) {
        disconnect("confirmed",callable_mp(this, &ClassName::_confirmed));
    }
}

//void PropertySelector::select_method_from_base_type(const StringName &p_base, const UIString &p_current, bool p_virtuals_only) {

//    base_type = p_base;
//    selected = p_current;
//    type = VariantType::NIL;
//    script = entt::null;
//    properties = false;
//    instance = nullptr;
//    virtuals_only = p_virtuals_only;

//    popup_centered_ratio(0.6f);
//    search_box->set_text("");
//    search_box->grab_focus();
//    _update_search();
//}

//void PropertySelector::select_method_from_script(const Ref<Script> &p_script, const UIString &p_current) {

//    ERR_FAIL_COND(not p_script);
//    base_type = p_script->get_instance_base_type();
//    selected = p_current;
//    type = VariantType::NIL;
//    script = p_script->get_instance_id();
//    properties = false;
//    instance = nullptr;
//    virtuals_only = false;

//    popup_centered_ratio(0.6f);
//    search_box->set_text("");
//    search_box->grab_focus();
//    _update_search();
//}
//void PropertySelector::select_method_from_basic_type(VariantType p_type, const UIString &p_current) {

//    ERR_FAIL_COND(p_type == VariantType::NIL);
//    base_type = "";
//    selected = p_current;
//    type = p_type;
//    script = entt::null;
//    properties = false;
//    instance = nullptr;
//    virtuals_only = false;

//    popup_centered_ratio(0.6f);
//    search_box->set_text("");
//    search_box->grab_focus();
//    _update_search();
//}

void PropertySelector::select_method_from_instance(Object *p_instance, const UIString &p_current) {

    base_type = StringName(p_instance->get_class());
    selected = p_current;
    type = VariantType::NIL;
    script = entt::null;
    {
        Ref<Script> scr(refFromRefPtr<Script>(p_instance->get_script()));
        if (scr)
            script = scr->get_instance_id();
    }
    properties = false;
    instance = nullptr;
    virtuals_only = false;

    popup_centered_ratio(0.6f);
    search_box->set_text("");
    search_box->grab_focus();
    _update_search();
}

//void PropertySelector::select_property_from_base_type(const StringName &p_base, const UIString &p_current) {

//    base_type = p_base;
//    selected = p_current;
//    type = VariantType::NIL;
//    script = entt::null;
//    properties = true;
//    instance = nullptr;
//    virtuals_only = false;

//    popup_centered_ratio(0.6f);
//    search_box->set_text("");
//    search_box->grab_focus();
//    _update_search();
//}

//void PropertySelector::select_property_from_script(const Ref<Script> &p_script, const UIString &p_current) {

//    ERR_FAIL_COND(not p_script);

//    base_type = p_script->get_instance_base_type();
//    selected = p_current;
//    type = VariantType::NIL;
//    script = p_script->get_instance_id();
//    properties = true;
//    instance = nullptr;
//    virtuals_only = false;

//    popup_centered_ratio(0.6f);
//    search_box->set_text("");
//    search_box->grab_focus();
//    _update_search();
//}

void PropertySelector::select_property_from_basic_type(VariantType p_type, const UIString &p_current) {

    ERR_FAIL_COND(p_type == VariantType::NIL);
    base_type = "";
    selected = p_current;
    type = p_type;
    script = entt::null;
    properties = true;
    instance = nullptr;
    virtuals_only = false;

    popup_centered_ratio(0.6f);
    search_box->set_text("");
    search_box->grab_focus();
    _update_search();
}

void PropertySelector::select_property_from_instance(Object *p_instance, const UIString &p_current) {

    base_type = "";
    selected = p_current;
    type = VariantType::NIL;
    script = entt::null;
    properties = true;
    instance = p_instance;
    virtuals_only = false;

    popup_centered_ratio(0.6f);
    search_box->set_text("");
    search_box->grab_focus();
    _update_search();
}

void PropertySelector::set_type_filter(const Vector<VariantType> &p_type_filter) {
    type_filter = p_type_filter;
}

void PropertySelector::_bind_methods() {

    ADD_SIGNAL(MethodInfo("selected", PropertyInfo(VariantType::STRING, "name")));
}
void PropertySelector::_hide_requested() {
    _cancel_pressed(); // From AcceptDialog.
}
PropertySelector::PropertySelector() {

    VBoxContainer *vbc = memnew(VBoxContainer);
    add_child(vbc);
    //set_child_rect(vbc);
    search_box = memnew(LineEdit);
    vbc->add_margin_child(TTR("Search:"), search_box);
    search_box->connect("text_changed",callable_mp(this, &ClassName::_text_changed));
    search_box->connect("gui_input",callable_mp(this, &ClassName::_sbox_input));
    search_options = memnew(Tree);
    vbc->add_margin_child(TTR("Matches:"), search_options, true);
    get_ok()->set_text(TTR("Open"));
    get_ok()->set_disabled(true);
    register_text_enter(search_box);
    set_hide_on_ok(false);
    search_options->connect("item_activated",callable_mp(this, &ClassName::_confirmed));
    search_options->connect("cell_selected",callable_mp(this, &ClassName::_item_selected));
    search_options->set_hide_root(true);
    search_options->set_hide_folding(true);
    virtuals_only = false;

    help_bit = memnew(EditorHelpBit);
    vbc->add_margin_child(TTR("Description:"), help_bit);
    help_bit->connect("request_hide",callable_mp(this, &PropertySelector::_hide_requested));
}
