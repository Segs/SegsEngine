/*************************************************************************/
/*  item_list_editor_plugin.cpp                                          */
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

#include "item_list_editor_plugin.h"

#include "core/callable_method_pointer.h"
#include "core/io/resource_loader.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"
#include "scene/gui/tool_button.h"
#include "scene/main/scene_tree.h"

using namespace eastl;

IMPL_GDCLASS(ItemListPlugin)
IMPL_GDCLASS(ItemListOptionButtonPlugin)
IMPL_GDCLASS(ItemListPopupMenuPlugin)
IMPL_GDCLASS(ItemListItemListPlugin)
IMPL_GDCLASS(ItemListEditor)
IMPL_GDCLASS(ItemListEditorPlugin)

bool ItemListPlugin::_set(const StringName &p_name, const Variant &p_value) {

    int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 0));
    StringView what = StringUtils::get_slice(p_name,'/', 1);

    if (what == "text"_sv)
        set_item_text(idx, p_value.as<StringName>());
    else if (what == "icon"_sv)
        set_item_icon(idx, refFromVariant<Texture>(p_value));
    else if (what == "checkable"_sv) {
        // This keeps compatibility to/from versions where this property was a boolean, before radio buttons
        switch ((int)p_value.as<int>()) {
            case 0:
            case 1:
                set_item_checkable(idx, p_value.as<bool>());
                break;
            case 2:
                set_item_radio_checkable(idx, true);
                break;
        }
    } else if (what == "checked"_sv)
        set_item_checked(idx, p_value.as<bool>());
    else if (what == "id"_sv)
        set_item_id(idx, p_value.as<int>());
    else if (what == "enabled"_sv)
        set_item_enabled(idx, p_value.as<bool>());
    else if (what == "separator"_sv)
        set_item_separator(idx, p_value.as<bool>());
    else
        return false;

    return true;
}

bool ItemListPlugin::_get(const StringName &p_name, Variant &r_ret) const {

    int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 0));
    StringView what = StringUtils::get_slice(p_name,'/', 1);

    if (what == "text"_sv)
        r_ret = get_item_text(idx);
    else if (what == "icon"_sv)
        r_ret = get_item_icon(idx);
    else if (what == "checkable"_sv) {
        // This keeps compatibility to/from versions where this property was a boolean, before radio buttons
        if (!is_item_checkable(idx)) {
            r_ret = 0;
        } else {
            r_ret = is_item_radio_checkable(idx) ? 2 : 1;
        }
    } else if (what == "checked"_sv)
        r_ret = is_item_checked(idx);
    else if (what == "id"_sv)
        r_ret = get_item_id(idx);
    else if (what == "enabled"_sv)
        r_ret = is_item_enabled(idx);
    else if (what == "separator"_sv)
        r_ret = is_item_separator(idx);
    else
        return false;

    return true;
}
void ItemListPlugin::_get_property_list(Vector<PropertyInfo> *p_list) const {

    for (int i = 0; i < get_item_count(); i++) {

        String base = itos(i) + "/";

        p_list->push_back(PropertyInfo(VariantType::STRING, StringName(base + "text")));
        p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(base + "icon"), PropertyHint::ResourceType, "Texture"));

        int flags = get_flags();

        if (flags & FLAG_CHECKABLE) {
            p_list->push_back(PropertyInfo(VariantType::INT, StringName(base + "checkable"), PropertyHint::Enum, "No,As checkbox,As radio button"));
            p_list->push_back(PropertyInfo(VariantType::BOOL, StringName(base + "checked")));
        }

        if (flags & FLAG_ID)
            p_list->push_back(PropertyInfo(VariantType::INT, StringName(base + "id"), PropertyHint::Range, "-1,4096"));

        if (flags & FLAG_ENABLE)
            p_list->push_back(PropertyInfo(VariantType::BOOL, StringName(base + "enabled")));

        if (flags & FLAG_SEPARATOR)
            p_list->push_back(PropertyInfo(VariantType::BOOL, StringName(base + "separator")));
    }
}

///////////////////////////////////////////////////////////////
///////////////////////// PLUGINS /////////////////////////////
///////////////////////////////////////////////////////////////

void ItemListOptionButtonPlugin::set_object(Object *p_object) {

    ob = object_cast<OptionButton>(p_object);
}

bool ItemListOptionButtonPlugin::handles(Object *p_object) const {

    return p_object->is_class("OptionButton");
}

int ItemListOptionButtonPlugin::get_flags() const {

    return FLAG_ICON | FLAG_ID | FLAG_ENABLE;
}

void ItemListOptionButtonPlugin::add_item() {

    ob->add_item(StringName(FormatVE(TTR("Item %d").asCString(), ob->get_item_count())));
    Object_change_notify(this);
}

int ItemListOptionButtonPlugin::get_item_count() const {

    return ob->get_item_count();
}

void ItemListOptionButtonPlugin::erase(int p_idx) {

    ob->remove_item(p_idx);
    Object_change_notify(this);
}

ItemListOptionButtonPlugin::ItemListOptionButtonPlugin() {

    ob = nullptr;
}

///////////////////////////////////////////////////////////////

void ItemListPopupMenuPlugin::set_object(Object *p_object) {

    if (p_object->is_class("MenuButton"))
        pp = object_cast<MenuButton>(p_object)->get_popup();
    else
        pp = object_cast<PopupMenu>(p_object);
}

bool ItemListPopupMenuPlugin::handles(Object *p_object) const {

    return p_object->is_class("PopupMenu") || p_object->is_class("MenuButton");
}

int ItemListPopupMenuPlugin::get_flags() const {

    return FLAG_ICON | FLAG_CHECKABLE | FLAG_ID | FLAG_ENABLE | FLAG_SEPARATOR;
}

void ItemListPopupMenuPlugin::add_item() {

    pp->add_item(StringName(FormatVE(TTR("Item %d").asCString(), pp->get_item_count())));
    Object_change_notify(this);
}

int ItemListPopupMenuPlugin::get_item_count() const {

    return pp->get_item_count();
}

void ItemListPopupMenuPlugin::erase(int p_idx) {

    pp->remove_item(p_idx);
    Object_change_notify(this);
}

ItemListPopupMenuPlugin::ItemListPopupMenuPlugin() {

    pp = nullptr;
}

///////////////////////////////////////////////////////////////

void ItemListItemListPlugin::set_object(Object *p_object) {

    pp = object_cast<ItemList>(p_object);
}

bool ItemListItemListPlugin::handles(Object *p_object) const {

    return p_object->is_class("ItemList");
}

int ItemListItemListPlugin::get_flags() const {

    return FLAG_ICON | FLAG_ENABLE;
}

void ItemListItemListPlugin::set_item_text(int p_idx, const StringName &p_text) { pp->set_item_text(p_idx, p_text); }

const String &ItemListItemListPlugin::get_item_text(int p_idx) const { return pp->get_item_text(p_idx); }

void ItemListItemListPlugin::set_item_icon(int p_idx, const Ref<Texture> &p_tex) { pp->set_item_icon(p_idx, p_tex); }

Ref<Texture> ItemListItemListPlugin::get_item_icon(int p_idx) const { return pp->get_item_icon(p_idx); }

void ItemListItemListPlugin::set_item_enabled(int p_idx, int p_enabled) { pp->set_item_disabled(p_idx, !p_enabled); }

bool ItemListItemListPlugin::is_item_enabled(int p_idx) const { return !pp->is_item_disabled(p_idx); }

void ItemListItemListPlugin::add_item() {
    pp->add_item(StringName(FormatVE(TTR("Item %d").asCString(), pp->get_item_count())),Ref<Texture>());
    Object_change_notify(this);
}

int ItemListItemListPlugin::get_item_count() const {

    return pp->get_item_count();
}

void ItemListItemListPlugin::erase(int p_idx) {

    pp->remove_item(p_idx);
    Object_change_notify(this);
}

ItemListItemListPlugin::ItemListItemListPlugin() {

    pp = nullptr;
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

void ItemListEditor::_node_removed(Node *p_node) {

    if (p_node == item_list) {
        item_list = nullptr;
        hide();
        dialog->hide();
    }
}

void ItemListEditor::_notification(int p_notification) {
    switch (p_notification) {
        case NOTIFICATION_ENTER_TREE:
        case NOTIFICATION_THEME_CHANGED: {
            add_button->set_button_icon(get_theme_icon("Add", "EditorIcons"));
            del_button->set_button_icon(get_theme_icon("Remove", "EditorIcons"));
        } break;

        case NOTIFICATION_READY: {
            get_tree()->connect("node_removed", callable_mp(this, &ClassName::_node_removed));
        } break;

        case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
            property_editor->set_property_name_style(EditorPropertyNameProcessor::get_settings_style());
        } break;
    }
}

void ItemListEditor::_add_pressed() {

    if (selected_idx == -1)
        return;

    item_plugins[selected_idx]->add_item();
}

void ItemListEditor::_delete_pressed() {

    if (selected_idx == -1)
        return;

    StringName current_selected(property_editor->get_selected_path());

    if (current_selected.empty())
        return;

    // FIXME: Currently relying on selecting a *property* to derive what item to delete
    // e.g. you select "1/enabled" to delete item 1.
    // This should be fixed so that you can delete by selecting the item section header,
    // or a delete button on that header.

    int idx = StringUtils::to_int(StringUtils::get_slice(current_selected,'/', 0));

    item_plugins[selected_idx]->erase(idx);
}

void ItemListEditor::_edit_items() {
    dialog->popup_centered_clamped(Vector2(425, 1200) * EDSCALE, 0.8);
}

void ItemListEditor::edit(Node *p_item_list) {

    item_list = p_item_list;

    if (!item_list) {
        selected_idx = -1;
        property_editor->edit(nullptr);
        return;
    }

    for (int i = 0; i < item_plugins.size(); i++) {
        if (item_plugins[i]->handles(p_item_list)) {

            item_plugins[i]->set_object(p_item_list);
            property_editor->edit(item_plugins[i]);

            toolbar_button->set_button_icon(EditorNode::get_singleton()->get_object_icon(item_list, StringName()));

            selected_idx = i;
            return;
        }
    }

    selected_idx = -1;
    property_editor->edit(nullptr);
}

bool ItemListEditor::handles(Object *p_object) const {

    for (int i = 0; i < item_plugins.size(); i++) {
        if (item_plugins[i]->handles(p_object)) {
            return true;
        }
    }

    return false;
}

void ItemListEditor::_bind_methods() {
}

ItemListEditor::ItemListEditor() : item_list(nullptr), selected_idx(-1) {

    toolbar_button = memnew(ToolButton);
    toolbar_button->set_text(TTR("Items"));
    add_child(toolbar_button);
    toolbar_button->connect("pressed",callable_mp(this, &ClassName::_edit_items));

    dialog = memnew(AcceptDialog);
    dialog->set_title(TTR("Item List Editor"));
    add_child(dialog);

    VBoxContainer *vbc = memnew(VBoxContainer);
    dialog->add_child(vbc);
    //dialog->set_child_rect(vbc);

    HBoxContainer *hbc = memnew(HBoxContainer);
    hbc->set_h_size_flags(SIZE_EXPAND_FILL);
    vbc->add_child(hbc);

    add_button = memnew(Button);
    add_button->set_text(TTR("Add"));
    hbc->add_child(add_button);
    add_button->connect("pressed",callable_mp(this, &ClassName::_add_pressed));

    hbc->add_spacer();

    del_button = memnew(Button);
    del_button->set_text(TTR("Delete"));
    hbc->add_child(del_button);
    del_button->connect("pressed",callable_mp(this, &ClassName::_delete_pressed));

    property_editor = memnew(EditorInspector);
    vbc->add_child(property_editor);
    property_editor->set_v_size_flags(SIZE_EXPAND_FILL);
    property_editor->set_property_name_style(EditorPropertyNameProcessor::get_settings_style());
}

ItemListEditor::~ItemListEditor() {

    for (int i = 0; i < item_plugins.size(); i++) {
        memdelete(item_plugins[i]);
    }
}

void ItemListEditorPlugin::edit(Object *p_object) {

    item_list_editor->edit(object_cast<Node>(p_object));
}

bool ItemListEditorPlugin::handles(Object *p_object) const {

    return item_list_editor->handles(p_object);
}

void ItemListEditorPlugin::make_visible(bool p_visible) {

    if (p_visible) {
        item_list_editor->show();
    } else {

        item_list_editor->hide();
        item_list_editor->edit(nullptr);
    }
}

ItemListEditorPlugin::ItemListEditorPlugin(EditorNode *p_node) {

    editor = p_node;
    item_list_editor = memnew(ItemListEditor);
    CanvasItemEditor::get_singleton()->add_control_to_menu_panel(item_list_editor);

    item_list_editor->hide();
    item_list_editor->add_plugin(memnew(ItemListOptionButtonPlugin));
    item_list_editor->add_plugin(memnew(ItemListPopupMenuPlugin));
    item_list_editor->add_plugin(memnew(ItemListItemListPlugin));
}

ItemListEditorPlugin::~ItemListEditorPlugin() {
}
