/*************************************************************************/
/*  editor_properties.cpp                                                */
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

#include "editor_properties.h"
#include "filesystem_dock.h"

#include "core/callable_method_pointer.h"
#include "core/math/quat.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/object_tooling.h"
#include "core/resource/resource_manager.h"
#include "core/string_formatter.h"
#include "editor/create_dialog.h"
#include "editor/editor_resource_picker.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_scale.h"
#include "editor/editor_spin_slider.h"
#include "editor/inspector_dock.h"
#include "editor/property_selector.h"
#include "editor/scene_tree_dock.h"
#include "editor/scene_tree_editor.h"
#include "editor_node.h"
#include "editor_properties_array_dict.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/text_edit.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"

#include <QList>

IMPL_GDCLASS(EditorPropertyNil)
IMPL_GDCLASS(EditorPropertyText)
IMPL_GDCLASS(EditorPropertyMultilineText)
IMPL_GDCLASS(EditorPropertyTextEnum)
IMPL_GDCLASS(EditorPropertyPath)
IMPL_GDCLASS(EditorPropertyClassName)
IMPL_GDCLASS(EditorPropertyMember)
IMPL_GDCLASS(EditorPropertyCheck)
IMPL_GDCLASS(EditorPropertyEnum)
IMPL_GDCLASS(EditorPropertyFlags)
IMPL_GDCLASS(EditorPropertyLayers)
IMPL_GDCLASS(EditorPropertyInteger)
IMPL_GDCLASS(EditorPropertyObjectID)
IMPL_GDCLASS(EditorPropertyFloat)
IMPL_GDCLASS(EditorPropertyEasing)
IMPL_GDCLASS(EditorPropertyVector2)
IMPL_GDCLASS(EditorPropertyRect2)
IMPL_GDCLASS(EditorPropertyVector3)
IMPL_GDCLASS(EditorPropertyPlane)
IMPL_GDCLASS(EditorPropertyQuat)
IMPL_GDCLASS(EditorPropertyAABB)
IMPL_GDCLASS(EditorPropertyTransform2D)
IMPL_GDCLASS(EditorPropertyBasis)
IMPL_GDCLASS(EditorPropertyTransform)
IMPL_GDCLASS(EditorPropertyColor)
IMPL_GDCLASS(EditorPropertyNodePath)
IMPL_GDCLASS(EditorPropertyRID)
IMPL_GDCLASS(EditorPropertyResource)
IMPL_GDCLASS(EditorInspectorDefaultPlugin)


///////////////////// NULL /////////////////////////

void EditorPropertyNil::update_property() {
}

EditorPropertyNil::EditorPropertyNil() {
    Label *label = memnew(Label);
    label->set_text("[null]");
    add_child(label);
}

///////////////////// TEXT /////////////////////////

void EditorPropertyText::_text_entered(StringView p_string) {
    if (updating)
        return;

    if (text->has_focus()) {
        text->release_focus();
        _text_changed(p_string);
    }
}

void EditorPropertyText::_text_changed(StringView p_string) {
    if (updating) {
        return;
    }

    if (string_name) {
        emit_changed(get_edited_property(), StringName(p_string), "", true);
    } else {
        emit_changed(get_edited_property(), p_string, "", true);
    }
}

void EditorPropertyText::update_property() {
    String s = get_edited_object()->get(get_edited_property()).as<String>();
    updating = true;
    if (text->get_text() != s) {
        text->set_text(s);
    }
    text->set_editable(!is_read_only());
    updating = false;
}

void EditorPropertyText::set_string_name(bool p_enabled) {
    string_name = p_enabled;
}

void EditorPropertyText::set_placeholder(const StringName &p_string) {
    text->set_placeholder(p_string);
}

void EditorPropertyText::_bind_methods() {
}

EditorPropertyText::EditorPropertyText() {
    text = memnew(LineEdit);
    add_child(text);
    add_focusable(text);
    text->connect("text_changed",callable_mp(this, &ClassName::_text_changed));
    text->connect("text_entered",callable_mp(this, &ClassName::_text_entered));

    updating = false;
}

///////////////////// MULTILINE TEXT /////////////////////////

void EditorPropertyMultilineText::_big_text_changed() {
    text->set_text(big_text->get_text());
    emit_changed(get_edited_property(), big_text->get_text_utf8(), "", true);
}

void EditorPropertyMultilineText::_text_changed() {
    emit_changed(get_edited_property(), text->get_text_utf8(), "", true);
}

void EditorPropertyMultilineText::_open_big_text() {

    if (!big_text_dialog) {
        big_text = memnew(TextEdit);
        big_text->connect("text_changed",callable_mp(this, &ClassName::_big_text_changed));
        big_text->set_wrap_enabled(true);
        big_text_dialog = memnew(AcceptDialog);
        big_text_dialog->add_child(big_text);
        big_text_dialog->set_title(TTR("Edit Text:"));
        add_child(big_text_dialog);
    }

    big_text_dialog->popup_centered_clamped(Size2(1000, 900) * EDSCALE, 0.8f);
    big_text->set_text(text->get_text());
    big_text->grab_focus();
}

void EditorPropertyMultilineText::update_property() {
    String t = get_edited_object()->get(get_edited_property()).as<String>();

    if (text->get_text() == t)
        return;

    text->set_text(t);
    if (big_text && big_text->is_visible_in_tree()) {
        big_text->set_text(t);
    }
}

void EditorPropertyMultilineText::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_THEME_CHANGED:
        case NOTIFICATION_ENTER_TREE: {
            Ref<Texture> df = get_theme_icon("DistractionFree", "EditorIcons");
            open_big_text->set_button_icon(df);
            Ref<Font> font = get_theme_font("font", "Label");
            text->set_custom_minimum_size(Vector2(0, font->get_height() * 6));

        } break;
    }
}



EditorPropertyMultilineText::EditorPropertyMultilineText()  {
    HBoxContainer *hb = memnew(HBoxContainer);
    add_child(hb);
    set_bottom_editor(hb);
    text = memnew(TextEdit);
    text->connect("text_changed",callable_mp(this, &ClassName::_text_changed));
    text->set_wrap_enabled(true);
    add_focusable(text);
    hb->add_child(text);
    text->set_h_size_flags(SIZE_EXPAND_FILL);
    open_big_text = memnew(ToolButton);
    open_big_text->connect("pressed",callable_mp(this, &ClassName::_open_big_text));
    hb->add_child(open_big_text);
    big_text_dialog = nullptr;
    big_text = nullptr;
}

///////////////////// TEXT ENUM /////////////////////////

void EditorPropertyTextEnum::_emit_changed_value(StringView p_string) {
    if (string_name) {
        emit_changed(get_edited_property(), StringName(p_string));
    } else {
        emit_changed(get_edited_property(), p_string);
    }
}

void EditorPropertyTextEnum::_option_selected(int p_which) {
    _emit_changed_value(option_button->get_item_text(p_which));
}
void EditorPropertyTextEnum::_edit_custom_value() {
    default_layout->hide();
    edit_custom_layout->show();
    custom_value_edit->grab_focus();
}

void EditorPropertyTextEnum::_custom_value_submitted(String p_value) {
    edit_custom_layout->hide();
    default_layout->show();

    _emit_changed_value(StringUtils::strip_edges(p_value));
}

void EditorPropertyTextEnum::_custom_value_accepted() {
    String new_value(StringUtils::strip_edges(custom_value_edit->get_text()));
    _custom_value_submitted(new_value);
}

void EditorPropertyTextEnum::_custom_value_cancelled() {
    custom_value_edit->set_text(get_edited_object()->getT<StringName>(get_edited_property()));

    edit_custom_layout->hide();
    default_layout->show();
}
void EditorPropertyTextEnum::update_property() {
    StringName current_value = get_edited_object()->getT<StringName>(get_edited_property());
    int default_option = options.index_of(current_value);

         // The list can change in the loose mode.
    if (!loose_mode) {
        option_button->select(default_option);
        return;
    }
    custom_value_edit->set_text(current_value);
    option_button->clear();

    // Manually entered value.
    if (default_option < 0 && !current_value.empty()) {
        option_button->add_item(current_value, options.size() + 1001);
        option_button->select(0);

        option_button->add_separator();
    }

    // Add an explicit empty value for clearing the property.
    option_button->add_item("", options.size() + 1000);

    for (int i = 0; i < options.size(); i++) {
        option_button->add_item(options[i], i);
        if (options[i] == current_value) {
            option_button->select(option_button->get_item_count() - 1);
        }
    }
}
void EditorPropertyTextEnum::setup(const Vector<StringView> &p_options, bool p_string_name , bool p_loose_mode) {
    loose_mode = p_loose_mode;

    options.clear();

    if (loose_mode) {
        // Add an explicit empty value for clearing the property in the loose mode.
        option_button->add_item("", options.size() + 1000);
    }

    for (int i = 0; i < p_options.size(); i++) {
        options.emplace_back(p_options[i]);
        option_button->add_item(options.back(), i);
    }

    if (loose_mode) {
        edit_button->show();
    }
    string_name = p_string_name;
}


void EditorPropertyTextEnum::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE:
        case NOTIFICATION_THEME_CHANGED:
            edit_button->set_button_icon(get_theme_icon("Edit", "EditorIcons"));
            accept_button->set_button_icon(get_theme_icon("ImportCheck", "EditorIcons"));
            cancel_button->set_button_icon(get_theme_icon("ImportFail", "EditorIcons"));
            break;
    }
}
EditorPropertyTextEnum::EditorPropertyTextEnum() {

    default_layout = memnew(HBoxContainer);
    add_child(default_layout);

    edit_custom_layout = memnew(HBoxContainer);
    edit_custom_layout->hide();
    add_child(edit_custom_layout);

    option_button = memnew(OptionButton);
    option_button->set_h_size_flags(SIZE_EXPAND_FILL);
    option_button->set_clip_text(true);
    option_button->set_flat(true);
    default_layout->add_child(option_button);
    option_button->connect("item_selected", callable_mp(this, &ClassName::_option_selected));

    edit_button = memnew(Button);
    edit_button->set_flat(true);
    edit_button->hide();
    default_layout->add_child(edit_button);
    edit_button->connect("pressed", callable_mp(this, &ClassName::_edit_custom_value));

    custom_value_edit = memnew(LineEdit);
    custom_value_edit->set_h_size_flags(SIZE_EXPAND_FILL);
    edit_custom_layout->add_child(custom_value_edit);
    custom_value_edit->connect("text_entered", callable_mp(this, &ClassName::_custom_value_submitted));

    accept_button = memnew(Button);
    accept_button->set_flat(true);
    edit_custom_layout->add_child(accept_button);
    accept_button->connect("pressed", callable_mp(this, &ClassName::_custom_value_accepted));

    cancel_button = memnew(Button);
    cancel_button->set_flat(true);
    edit_custom_layout->add_child(cancel_button);
    cancel_button->connect("pressed", callable_mp(this, &ClassName::_custom_value_cancelled));

    add_focusable(option_button);
    add_focusable(edit_button);
    add_focusable(custom_value_edit);
    add_focusable(accept_button);
    add_focusable(cancel_button);
}
///////////////////// PATH /////////////////////////

void EditorPropertyPath::_path_selected(StringView p_path) {

    emit_changed(get_edited_property(), p_path);
    update_property();
}
void EditorPropertyPath::_path_pressed() {

    if (!dialog) {
        dialog = memnew(EditorFileDialog);
        dialog->connect("file_selected",callable_mp(this, &ClassName::_path_selected));
        dialog->connect("dir_selected",callable_mp(this, &ClassName::_path_selected));
        add_child(dialog);
    }

    String full_path = get_edited_object()->get(get_edited_property()).as<String>();

    dialog->clear_filters();

    EditorFileDialog::Access acc_mode = EditorFileDialog::ACCESS_RESOURCES;
    if (global) {
        acc_mode = EditorFileDialog::ACCESS_FILESYSTEM;
    }
    dialog->set_access(acc_mode);

    if (folder) {
        dialog->set_mode(EditorFileDialog::MODE_OPEN_DIR);
        dialog->set_current_dir(full_path);
    } else {
        dialog->set_mode(save_mode ? EditorFileDialog::MODE_SAVE_FILE : EditorFileDialog::MODE_OPEN_FILE);
        for (auto & extension : extensions) {
            StringView e =StringUtils::strip_edges( extension);
            if (!e.empty()) {
                dialog->add_filter(StringUtils::strip_edges(extension));
            }
        }
        dialog->set_current_path(full_path);
    }

    dialog->popup_centered_ratio();
}

void EditorPropertyPath::update_property() {

    StringName full_path = get_edited_object()->get(get_edited_property()).as<StringName>();
    path->set_text(full_path);
    path->set_tooltip(full_path);
}

void EditorPropertyPath::setup(const Vector<StringView> &p_extensions, bool p_folder, bool p_global) {
    extensions.reserve(p_extensions.size());

    for(StringView sv : p_extensions)
        extensions.emplace_back(sv);

    folder = p_folder;
    global = p_global;
}

void EditorPropertyPath::set_save_mode() {

    save_mode = true;
}

void EditorPropertyPath::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        path_edit->set_button_icon(get_theme_icon("Folder", "EditorIcons"));
    }
}

void EditorPropertyPath::_path_focus_exited() {

    _path_selected(path->get_text());
}

EditorPropertyPath::EditorPropertyPath() {
    HBoxContainer *path_hb = memnew(HBoxContainer);
    add_child(path_hb);
    path = memnew(LineEdit);
    path_hb->add_child(path);
    path->connect("text_entered",callable_mp(this, &ClassName::_path_selected));
    path->connect("focus_exited",callable_mp(this, &ClassName::_path_focus_exited));
    path->set_h_size_flags(SIZE_EXPAND_FILL);

    path_edit = memnew(Button);
    path_edit->set_clip_text(true);
    path_hb->add_child(path_edit);
    add_focusable(path);
    dialog = nullptr;
    path_edit->connect("pressed",callable_mp(this, &ClassName::_path_pressed));
    folder = false;
    global = false;
    save_mode = false;
}

///////////////////// CLASS NAME /////////////////////////

void EditorPropertyClassName::setup(const StringName &p_base_type, const StringName &p_selected_type) {

    base_type = p_base_type;
    dialog->set_base_type(base_type);
    selected_type = p_selected_type;
    property->set_text(selected_type);
}

void EditorPropertyClassName::update_property() {

    StringName s = get_edited_object()->get(get_edited_property()).as<StringName>();
    property->set_text(s);
    selected_type = s;
}

void EditorPropertyClassName::_property_selected() {
    dialog->popup_create(true);
}

void EditorPropertyClassName::_dialog_created() {
    selected_type = dialog->get_selected_type();
    emit_changed(get_edited_property(), selected_type);
    update_property();
}



EditorPropertyClassName::EditorPropertyClassName() {
    property = memnew(Button);
    property->set_clip_text(true);
    add_child(property);
    add_focusable(property);
    property->set_text(selected_type);
    property->connect("pressed",callable_mp(this, &ClassName::_property_selected));
    dialog = memnew(CreateDialog);
    dialog->set_base_type(base_type);
    dialog->connect("create",callable_mp(this, &ClassName::_dialog_created));
    add_child(dialog);
}

///////////////////// MEMBER /////////////////////////

void EditorPropertyMember::_property_selected(StringView p_selected) {

    emit_changed(get_edited_property(), p_selected);
    update_property();
}

void EditorPropertyMember::_property_select() {

    if (!selector) {
        selector = memnew(PropertySelector);
        selector->connect("selected",callable_mp(this, &ClassName::_property_selected));
        add_child(selector);
    }

    UIString current = get_edited_object()->get(get_edited_property()).as<UIString>();
    switch (hint) {
//        case EditorPropertyMember::MEMBER_METHOD_OF_VARIANT_TYPE: {
//            VariantType type = VariantType::NIL;
//            for (int i = 0; i < (int)VariantType::VARIANT_MAX; i++) {
//                if (hint_text == Variant::get_type_name(VariantType(i))) {
//                    type = VariantType(i);
//                }
//            }
//            if (type != VariantType::NIL)
//                selector->select_method_from_basic_type(type, current);
//            break;
//        }
//        case EditorPropertyMember::MEMBER_METHOD_OF_BASE_TYPE: {
//            selector->select_method_from_base_type(StringName(hint_text), current);
//            break;
//        }
//        case EditorPropertyMember::MEMBER_METHOD_OF_INSTANCE: {

//            Object *instance = object_for_entity(GameEntity(StringUtils::to_int64(hint_text)));
//            if (instance)
//                selector->select_method_from_instance(instance, current);

//            break;
//        }
//        case EditorPropertyMember::MEMBER_METHOD_OF_SCRIPT: {

//            Object *obj = object_for_entity(GameEntity(StringUtils::to_int64(hint_text)));
//            if (object_cast<Script>(obj)) {
//                selector->select_method_from_script(Ref<Script>(object_cast<Script>(obj)), current);
//            }

//            break;
//        }
        case EditorPropertyMember::MEMBER_PROPERTY_OF_VARIANT_TYPE: {

            VariantType type = VariantType::NIL;
            String tname = hint_text;
            if (StringUtils::contains(tname, ".")) tname = StringUtils::get_slice(tname, ".", 0);
            for (int i = 0; i < (int)VariantType::VARIANT_MAX; i++) {
                if (tname == Variant::get_type_name(VariantType(i))) {
                    type = VariantType(VariantType(i));
                }
            }

            if (type != VariantType::NIL)
                selector->select_property_from_basic_type(type, current);

            break;
        }
//        case EditorPropertyMember::MEMBER_PROPERTY_OF_BASE_TYPE: {
//            selector->select_property_from_base_type(StringName(hint_text), current);
//            break;
//        }
//        case EditorPropertyMember::MEMBER_PROPERTY_OF_INSTANCE: {

//            Object *instance = object_for_entity(GameEntity(StringUtils::to_int64(hint_text)));
//            if (instance)
//                selector->select_property_from_instance(instance, current);

//            break;
//        }
//        case EditorPropertyMember::MEMBER_PROPERTY_OF_SCRIPT: {

//            Object *obj = object_for_entity(GameEntity(StringUtils::to_int64(hint_text)));
//            if (object_cast<Script>(obj)) {
//                selector->select_property_from_script(Ref<Script>(object_cast<Script>(obj)), current);
//            }
//            break;
//        }
    }
}

void EditorPropertyMember::setup(Type p_hint, StringView p_hint_text) {
    hint = p_hint;
    hint_text = p_hint_text;
}

void EditorPropertyMember::update_property() {

    StringName full_path = get_edited_object()->getT<StringName>(get_edited_property());
    property->set_text(full_path);
}



EditorPropertyMember::EditorPropertyMember() {
    selector = nullptr;
    property = memnew(Button);
    property->set_clip_text(true);
    add_child(property);
    add_focusable(property);
    property->connect("pressed",callable_mp(this, &ClassName::_property_select));
}

///////////////////// CHECK /////////////////////////
void EditorPropertyCheck::_checkbox_pressed() {

    emit_changed(get_edited_property(), checkbox->is_pressed());
}

void EditorPropertyCheck::update_property() {
    bool c = get_edited_object()->getT<bool>(get_edited_property());
    checkbox->set_pressed(c);
    checkbox->set_disabled(is_read_only());
}

EditorPropertyCheck::EditorPropertyCheck() {
    checkbox = memnew(CheckBox);
    checkbox->set_text(TTR("On"));
    add_child(checkbox);
    add_focusable(checkbox);
    checkbox->connect("pressed",callable_mp(this, &ClassName::_checkbox_pressed));
}

///////////////////// ENUM /////////////////////////

void EditorPropertyEnum::_option_selected(int p_which) {

    int64_t val = options->get_item_metadata(p_which).as<int64_t>();
    emit_changed(get_edited_property(), val);
}

void EditorPropertyEnum::update_property() {

    int64_t which = get_edited_object()->get(get_edited_property()).as<int64_t>();

    for (int i = 0; i < options->get_item_count(); i++) {
        if (which == options->get_item_metadata(i).as<int64_t>()) {
            options->select(i);
            return;
        }
    }
}

void EditorPropertyEnum::setup(const Vector<StringView> &p_options) {

    int64_t current_val = 0;
    for (int i = 0; i < p_options.size(); i++) {
        Vector<StringView> text_split = StringUtils::split(p_options[i],':');
        if (text_split.size() != 1)
            current_val = StringUtils::to_int64(text_split[1]);
        options->add_item(StringName(text_split[0]));
        options->set_item_metadata(i, current_val);
        current_val += 1;
    }
}

void EditorPropertyEnum::set_option_button_clip(bool p_enable) {
    options->set_clip_text(p_enable);
}

EditorPropertyEnum::EditorPropertyEnum() {
    options = memnew(OptionButton);
    options->set_clip_text(true);
    options->set_flat(true);
    add_child(options);
    add_focusable(options);
    options->connect("item_selected",callable_mp(this, &ClassName::_option_selected));
}

///////////////////// FLAGS /////////////////////////

void EditorPropertyFlags::_flag_toggled() {

    uint32_t value = 0;
    for (int i = 0; i < flags.size(); i++) {
        if (flags[i]->is_pressed()) {
            uint32_t val = 1;
            val <<= flag_indices[i];
            value |= val;
        }
    }

    emit_changed(get_edited_property(), value);
}

void EditorPropertyFlags::update_property() {

    uint32_t value = get_edited_object()->getT<uint32_t>(get_edited_property());

    for (int i = 0; i < flags.size(); i++) {
        uint32_t val = 1;
        val <<= flag_indices[i];
        if (value & val) {

            flags[i]->set_pressed(true);
        } else {
            flags[i]->set_pressed(false);
        }
    }
}

void EditorPropertyFlags::setup(const Vector<StringView> &p_options) {
    ERR_FAIL_COND(flags.size());

    bool first = true;
    for (int i = 0; i < p_options.size(); i++) {
        StringView option =StringUtils::strip_edges( p_options[i]);
        if (!option.empty()) {
            CheckBox *cb = memnew(CheckBox);
            cb->set_text(option);
            cb->set_clip_text(true);
            cb->connect("pressed",callable_mp(this, &ClassName::_flag_toggled));
            add_focusable(cb);
            vbox->add_child(cb);
            flags.push_back(cb);
            flag_indices.push_back(i);
            if (first) {
                set_label_reference(cb);
                first = false;
            }
        }
    }
}

EditorPropertyFlags::EditorPropertyFlags() {

    vbox = memnew(VBoxContainer);
    add_child(vbox);
}

///////////////////// LAYERS /////////////////////////

class EditorPropertyLayersGrid : public Control {
    GDCLASS(EditorPropertyLayersGrid,Control)
private:
    Vector<Rect2> flag_rects;
    Rect2 expand_rect;
    bool expand_hovered = false;
    bool expanded = false;
    int expansion_rows = 0;
    int hovered_index = -1; // Nothing is hovered.

    Size2 get_grid_size() const {
        Ref<Font> font = get_theme_font("font", "Label");
        return Size2(0, font->get_height() * 3);
    }

public:
    uint32_t value = 0;
    int layer_group_size = 0;
    int layer_count = 0;
    Vector<StringName> names;
    Vector<String> tooltips;

    Size2 get_minimum_size() const override {
        Size2 min_size = get_grid_size();

        // Add extra rows when expanded.
        if (expanded) {
            const int bsize = (min_size.height * 80 / 100) / 2;
            for (int i = 0; i < expansion_rows; ++i) {
                min_size.y += 2 * (bsize + 1) + 3;
            }
        }

        return min_size;
    }

    const String & get_tooltip(const Point2 &p_pos) const override {
        for (int i = 0; i < flag_rects.size(); i++) {
            if (i < tooltips.size() && flag_rects[i].has_point(p_pos)) {
                return tooltips[i];
            }
        }
        return null_string;
    }
    void _gui_input(const Ref<InputEvent> &p_ev) {
        const Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_ev);

        if (mm) {
            bool expand_was_hovered = expand_hovered;
            expand_hovered = expand_rect.has_point(mm->get_position());
            if (expand_hovered != expand_was_hovered) {
                update();
            }
            if (!expand_hovered) {
                for (int i = 0; i < flag_rects.size(); i++) {
                    if (flag_rects[i].has_point(mm->get_position())) {
                        // Used to highlight the hovered flag in the layers grid.
                        hovered_index = i;
                        update();
                        return;
                    }
                }
            }
            // Remove highlight when no square is hovered.
            if (hovered_index != -1) {
                hovered_index = -1;
                update();
            }

            return;
        }
        Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_ev);
        if (mb && mb->get_button_index() == BUTTON_LEFT && mb->is_pressed()) {
            if(hovered_index >= 0) {
                // Toggle the flag.
                // We base our choice on the hovered flag, so that it always matches the hovered flag.
                if (value & (1 << hovered_index)) {
                    value &= ~(1 << hovered_index);
                } else {
                    value |= (1 << hovered_index);
                }

                emit_signal("flag_changed", value);
                update();
            } else if (expand_hovered) {
                expanded = !expanded;
                minimum_size_changed();
                update();
            }
        }
    }

    void _notification(int p_what) {
        switch (p_what) {
            case NOTIFICATION_DRAW: {
                Size2 grid_size = get_grid_size();
                grid_size.x = get_size().x;
                flag_rects.clear();
                int prev_expansion_rows = expansion_rows;
                expansion_rows = 0;

                const int bsize = (grid_size.height * 80 / 100) / 2;
                const int h = bsize * 2 + 1;

                Color color = get_theme_color("highlight_color", "Editor");

                Color text_color = get_theme_color("font_color", "Editor");
                text_color.a *= 0.5;

                Color text_color_on = get_theme_color("dark_color_3", "Editor");
                text_color_on.a *= 0.8;

                const int vofs = (grid_size.height - h) / 2;

                int layer_index = 0;
                int block_index = 0;

                Point2 arrow_pos;

                Point2 block_ofs(4, vofs);

                while (true) {
                    Point2 ofs = block_ofs;

                    for (int i = 0; i < 2; i++) {
                        for (int j = 0; j < layer_group_size; j++) {
                            const bool on = value & (1 << layer_index);
                            Rect2 rect2 = Rect2(ofs, Size2(bsize, bsize));

                            color.a = on ? 0.6 : 0.2;
                            if (layer_index == hovered_index) {
                                // Add visual feedback when hovering a flag.
                                color.a += 0.15;
                            }

                            draw_rect_filled(rect2, color);
                            flag_rects.push_back(rect2);

                            Ref<Font> font = get_theme_font("font", "Label");
                            Vector2 offset;
                            if (layer_index + 1 > 9) {
                                // Offset for double digit numbers.
                                offset.x = rect2.size.x * 0.1;
                            } else {
                                offset.x = rect2.size.x * 0.3;
                            }
                            offset.y = rect2.size.y * 0.75;

                            draw_string(font, rect2.position + offset, itos(layer_index + 1), on ? text_color_on : text_color);

                            ofs.x += bsize + 1;

                            ++layer_index;
                        }

                        ofs.x = block_ofs.x;
                        ofs.y += bsize + 1;
                    }

                    if (layer_index >= layer_count) {
                        if (!flag_rects.empty() && (expansion_rows == 0)) {
                            const Rect2 &last_rect = flag_rects[flag_rects.size() - 1];
                            arrow_pos = last_rect.position + last_rect.size;
                        }
                        break;
                    }

                    int block_size_x = layer_group_size * (bsize + 1);
                    block_ofs.x += block_size_x + 3;

                    if (block_ofs.x + block_size_x + 12 > grid_size.width) {
                        // Keep last valid cell position for the expansion icon.
                        if (!flag_rects.empty() && (expansion_rows == 0)) {
                            const Rect2 &last_rect = flag_rects[flag_rects.size() - 1];
                            arrow_pos = last_rect.position + last_rect.size;
                        }
                        ++expansion_rows;

                        if (expanded) {
                            // Expand grid to next line.
                            block_ofs.x = 4;
                            block_ofs.y += 2 * (bsize + 1) + 3;
                        } else {
                            // Skip remaining blocks.
                            break;
                        }
                    }

                    ++block_index;
                }

                if ((expansion_rows != prev_expansion_rows) && expanded) {
                    minimum_size_changed();
                }

                if ((expansion_rows == 0) && (layer_index == layer_count)) {
                    // Whole grid was drawn, no need for expansion icon.
                    break;
                }

                Ref<Texture> arrow = get_theme_icon("arrow", "Tree");
                ERR_FAIL_COND(!arrow);

                Color arrow_color = get_theme_color("highlight_color", "Editor");
                arrow_color.a = expand_hovered ? 1.0 : 0.6;

                arrow_pos.x += 2.0;
                arrow_pos.y -= arrow->get_height();

                Rect2 arrow_draw_rect(arrow_pos, arrow->get_size());
                expand_rect = arrow_draw_rect;
                if (expanded) {
                    arrow_draw_rect.size.y *= -1.0; // Flip arrow vertically when expanded.
                }

                RenderingEntity ci = get_canvas_item();
                arrow->draw_rect(ci, arrow_draw_rect, false, arrow_color);
            } break;
            case NOTIFICATION_MOUSE_EXIT: {
            if (expand_hovered) {
                expand_hovered = false;
                update();
            }
            if (hovered_index != -1) {
                hovered_index = -1;
                update();
            }
            } break;
            default:
                break;
        }
    }

    void set_flag(uint32_t p_flag) {
        value = p_flag;
        update();
    }

    static void _bind_methods() {

        SE_BIND_METHOD(EditorPropertyLayersGrid,_gui_input);
        ADD_SIGNAL(MethodInfo("flag_changed", PropertyInfo(VariantType::INT, "flag")));
    }
};

IMPL_GDCLASS(EditorPropertyLayersGrid)


void EditorPropertyLayers::_grid_changed(uint32_t p_grid) {

    emit_changed(get_edited_property(), p_grid);
}

void EditorPropertyLayers::update_property() {

    uint32_t value = get_edited_object()->getT<uint32_t>(get_edited_property());

    grid->set_flag(value);
}

void EditorPropertyLayers::setup(LayerType p_layer_type) {
    String basename;
    int layer_group_size = 0;
    int layer_count = 0;

    layer_type = p_layer_type;

    switch (p_layer_type) {
        case LAYER_RENDER_2D: {
            basename = "layer_names/2d_render";
            layer_group_size = 5;
            layer_count = 20;
        } break;
        case LAYER_PHYSICS_2D: {
            basename = "layer_names/2d_physics";
            layer_group_size = 4;
            layer_count = 32;
        } break;

        case LAYER_RENDER_3D: {
            basename = "layer_names/3d_render";
            layer_group_size = 5;
            layer_count = 20;
        } break;

        case LAYER_PHYSICS_3D: {
            basename = "layer_names/3d_physics";
            layer_group_size = 4;
            layer_count = 32;
        } break;
    }

    Vector<StringName> names;
    Vector<String> tooltips;
    for (int i = 0; i < layer_count; i++) {
        StringName name;
        StringName lname(basename + "/layer_" + itos(i + 1));
        if (ProjectSettings::get_singleton()->has_setting(lname)) {
            name = ProjectSettings::get_singleton()->get(lname).as<StringName>();
        }

        if (name.empty()) {
            name = TTR("Layer") + " " + itos(i + 1);
        }

        names.push_back(name);
        tooltips.push_back(String(name) + "\n" + FormatVE(TTR("Bit %d, value %d").asCString(), i, 1 << i));
    }

    grid->names = eastl::move(names);
    grid->tooltips = eastl::move(tooltips);
    grid->layer_group_size = layer_group_size;
    grid->layer_count = layer_count;
}

void EditorPropertyLayers::_button_pressed() {
    int layer_count = grid->layer_count;
    int layer_group_size = grid->layer_group_size;

    layers->clear();
    for (int i = 0; i < layer_count; i++) {
        if ((i != 0) && ((i % layer_group_size) == 0)) {
            layers->add_separator();
        }
        layers->add_check_item_utf8(grid->names[i], i);
        int idx = layers->get_item_index(i);
        layers->set_item_checked(idx, grid->value & 1 << i);
    }

    Rect2 gp = button->get_global_rect();
    layers->set_as_minsize();
    Vector2 popup_pos = gp.position - Vector2(layers->get_combined_minimum_size().x, 0);
    layers->set_global_position(popup_pos);
    layers->popup();
}

void EditorPropertyLayers::_menu_pressed(int p_menu) {
    if (grid->value & 1 << p_menu) {
        grid->value &= ~(1 << p_menu);
    } else {
        grid->value |= 1 << p_menu;
    }
    grid->update();
    layers->set_item_checked(layers->get_item_index(p_menu), grid->value & 1 << p_menu);
    _grid_changed(grid->value);
}

void EditorPropertyLayers::_refresh_names() {
    setup(layer_type);
}

EditorPropertyLayers::EditorPropertyLayers() {

    HBoxContainer *hb = memnew(HBoxContainer);
    hb->set_clip_contents(true);
    add_child(hb);
    grid = memnew(EditorPropertyLayersGrid);
    grid->connect("flag_changed",callable_mp(this, &ClassName::_grid_changed));
    grid->set_h_size_flags(SIZE_EXPAND_FILL);
    hb->add_child(grid);
    button = memnew(Button);
    button->set_toggle_mode(true);
    button->set_text("..");
    button->connect("pressed",callable_mp(this, &ClassName::_button_pressed));
    hb->add_child(button);
    set_bottom_editor(hb);
    layers = memnew(PopupMenu);
    add_child(layers);
    layers->set_hide_on_checkable_item_selection(false);
    layers->connect("id_pressed",callable_mp(this, &ClassName::_menu_pressed));
    layers->connectF("popup_hide", button,[button=this->button]() { button->set_pressed(false); });

    ProjectSettings::get_singleton()->connect("project_settings_changed", callable_mp(this, &ClassName::_refresh_names));
}

///////////////////// INT /////////////////////////

void EditorPropertyInteger::_value_changed(int64_t val) {
    if (setting)
        return;
    emit_changed(get_edited_property(), val);
}

void EditorPropertyInteger::update_property() {
    int64_t val = get_edited_object()->getT<int64_t>(get_edited_property());
    setting = true;
    spin->set_value(val);
    setting = false;
#ifdef DEBUG_ENABLED
    // If spin (currently EditorSplinSlider : Range) is changed so that it can use int64_t, then the below warning wouldn't be a problem.
    if (val != (int64_t)(double)(val)) {
        WARN_PRINT("Cannot reliably represent '" + itos(val) + "' in the inspector, value is too large.");
    }
#endif
}



void EditorPropertyInteger::setup(int64_t p_min, int64_t p_max, int64_t p_step, bool p_allow_greater, bool p_allow_lesser) {
    spin->set_min(p_min);
    spin->set_max(p_max);
    spin->set_step(p_step);
    spin->set_allow_greater(p_allow_greater);
    spin->set_allow_lesser(p_allow_lesser);
}

EditorPropertyInteger::EditorPropertyInteger() {
    spin = memnew(EditorSpinSlider);
    spin->set_flat(true);
    add_child(spin);
    add_focusable(spin);
    spin->connect("value_changed",callable_mp(this, &ClassName::_value_changed));
    setting = false;
}

///////////////////// OBJECT ID /////////////////////////

void EditorPropertyObjectID::_edit_pressed() {

    emit_signal("object_id_selected", get_edited_property(), get_edited_object()->get(get_edited_property()));
}

void EditorPropertyObjectID::update_property() {
    StringName type = base_type;
    if (type.empty())
        type = "Object";

    GameEntity id = get_edited_object()->get(get_edited_property()).as<GameEntity>();
    if (id!=entt::null) {
        edit->set_text(String(type) + " ID: " + itos(entt::to_integral(id)));
        edit->set_disabled(false);
        edit->set_button_icon(EditorNode::get_singleton()->get_class_icon(type));
    } else {
        edit->set_text(TTR("[Empty]"));
        edit->set_disabled(true);
        edit->set_button_icon(Ref<Texture>());
    }
}

void EditorPropertyObjectID::setup(const StringName &p_base_type) {
    base_type = p_base_type;
}

EditorPropertyObjectID::EditorPropertyObjectID() {
    edit = memnew(Button);
    add_child(edit);
    add_focusable(edit);
    edit->connect("pressed",callable_mp(this, &ClassName::_edit_pressed));
}

///////////////////// FLOAT /////////////////////////

void EditorPropertyFloat::_value_changed(double val) {
    if (setting)
        return;

    emit_changed(get_edited_property(), val);
}

void EditorPropertyFloat::update_property() {
    double val = get_edited_object()->getT<float>(get_edited_property());
    setting = true;
    spin->set_value(val);
    setting = false;
}

void EditorPropertyFloat::setup(double p_min, double p_max, double p_step, bool p_no_slider, bool p_exp_range, bool p_greater, bool p_lesser) {

    spin->set_min(p_min);
    spin->set_max(p_max);
    spin->set_step(p_step);
    spin->set_hide_slider(p_no_slider);
    spin->set_exp_ratio(p_exp_range);
    spin->set_allow_greater(p_greater);
    spin->set_allow_lesser(p_lesser);
}

EditorPropertyFloat::EditorPropertyFloat() {
    spin = memnew(EditorSpinSlider);
    spin->set_flat(true);
    add_child(spin);
    add_focusable(spin);
    spin->connect("value_changed",callable_mp(this, &ClassName::_value_changed));
    setting = false;
}

///////////////////// EASING /////////////////////////

void EditorPropertyEasing::_drag_easing(const Ref<InputEvent> &p_ev) {

    const Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_ev);
    if (mb) {
        if (mb->is_doubleclick() && mb->get_button_index() == BUTTON_LEFT) {
            _setup_spin();
        }

        if (mb->is_pressed() && mb->get_button_index() == BUTTON_RIGHT) {
        preset->set_global_position(easing_draw->get_global_transform().xform(mb->get_position()));
        preset->popup();
            // Ensure the easing doesn't appear as being dragged
            dragging = false;
            easing_draw->update();
    }
        if (mb->get_button_index() == BUTTON_LEFT) {
            dragging = mb->is_pressed();
            // Update to display the correct dragging color
            easing_draw->update();
        }
    }

    const Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_ev);

    if (dragging && mm && mm->get_button_mask() & BUTTON_MASK_LEFT) {

        float rel = mm->get_relative().x;
        if (rel == 0)
            return;

        if (flip)
            rel = -rel;

        float val = get_edited_object()->getT<float>(get_edited_property());

        bool sg = val < 0;
        val = Math::absf(val);

        val = Math::log(val) / Math::log(2.0f);
        // Logarithmic space.
        val += rel * 0.05f;

        val = Math::pow(2.0f, val);
        if (sg)
            val = -val;

        // 0 is a singularity, but both positive and negative values
        // are otherwise allowed. Enforce 0+ as workaround.
        if (Math::is_zero_approx(val)) {
            val = 0.00001;
        }

        // Limit to a reasonable value to prevent the curve going into infinity,
        // which can cause crashes and other issues.
        val = CLAMP<float>(val, -1000000, 1000000);

        emit_changed(get_edited_property(), val);
        easing_draw->update();
    }
}

void EditorPropertyEasing::_draw_easing() {

    RenderingEntity ci = easing_draw->get_canvas_item();

    Size2 s = easing_draw->get_size();

    const int point_count = 48;

    const float exp = get_edited_object()->getT<float>(get_edited_property());

    const Ref<Font> f = get_theme_font("font", "Label");
    const Color font_color = get_theme_color("font_color", "Label");
    Color line_color;
    if (dragging) {
        line_color = get_theme_color("accent_color", "Editor");
    } else {
        line_color = get_theme_color("font_color", "Label") * Color(1, 1, 1, 0.9f);
    }

    Vector<Point2> points;
    points.reserve(point_count+1);

    for (int i = 0; i <= point_count; i++) {
        float ifl = i / float(point_count);

        const float h = 1.0 - Math::ease(ifl, exp);

        if (flip) {
            ifl = 1.0 - ifl;
        }

        points.emplace_back(ifl * s.width, h * s.height);
    }

    easing_draw->draw_polyline(points, line_color, 1.0, true);
    // Draw more decimals for small numbers since higher precision is usually required for fine adjustments.
    int decimals;
    if (Math::abs(exp) < 0.1 - CMP_EPSILON) {
        decimals = 4;
    } else if (Math::abs(exp) < 1 - CMP_EPSILON) {
        decimals = 3;
    } else if (Math::abs(exp) < 10 - CMP_EPSILON) {
        decimals = 2;
    } else {
        decimals = 1;
    }
    f->draw(ci, Point2(10, 10 + f->get_ascent()), StringUtils::pad_decimals(rtos(exp),decimals), font_color);
}

void EditorPropertyEasing::update_property() {
    easing_draw->update();
}

void EditorPropertyEasing::_set_preset(int p_preset) {
    static const float preset_value[EASING_MAX] = { 0.0, 1.0, 2.0, 0.5, -2.0, -0.5 };

    emit_changed(get_edited_property(), preset_value[p_preset]);
    easing_draw->update();
}

void EditorPropertyEasing::_setup_spin() {
    setting = true;
    spin->setup_and_show();
    spin->get_line_edit()->set_text(rtos(get_edited_object()->getT<float>(get_edited_property())));
    setting = false;
    spin->show();
}

void EditorPropertyEasing::_spin_value_changed(double p_value) {
    if (setting)
        return;

    // 0 is a singularity, but both positive and negative values
    // are otherwise allowed. Enforce 0+ as workaround.
    if (Math::is_zero_approx(p_value)) {
        p_value = 0.00001f;
    }
    // Limit to a reasonable value to prevent the curve going into infinity,
    // which can cause crashes and other issues.
    p_value = CLAMP<float>(p_value, -1000000, 1000000);


    emit_changed(get_edited_property(), p_value);
    _spin_focus_exited();
}

void EditorPropertyEasing::_spin_focus_exited() {
    spin->hide();
    // Ensure the easing doesn't appear as being dragged
    dragging = false;
    easing_draw->update();
}

void EditorPropertyEasing::setup(bool p_full, bool p_flip) {

    flip = p_flip;
    full = p_full;
}

void EditorPropertyEasing::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_THEME_CHANGED:
        case NOTIFICATION_ENTER_TREE: {
            preset->clear();
            preset->add_icon_item(get_theme_icon("CurveConstant", "EditorIcons"), "Zero", EASING_ZERO);
            preset->add_icon_item(get_theme_icon("CurveLinear", "EditorIcons"), "Linear", EASING_LINEAR);
            preset->add_icon_item(get_theme_icon("CurveIn", "EditorIcons"), "In", EASING_IN);
            preset->add_icon_item(get_theme_icon("CurveOut", "EditorIcons"), "Out", EASING_OUT);
            if (full) {
                preset->add_icon_item(get_theme_icon("CurveInOut", "EditorIcons"), "In-Out", EASING_IN_OUT);
                preset->add_icon_item(get_theme_icon("CurveOutIn", "EditorIcons"), "Out-In", EASING_OUT_IN);
            }
            easing_draw->set_custom_minimum_size(Size2(0, get_theme_font("font", "Label")->get_height() * 2));
        } break;
    }
}

EditorPropertyEasing::EditorPropertyEasing() {

    easing_draw = memnew(Control);
    easing_draw->connect("draw",callable_mp(this, &ClassName::_draw_easing));
    easing_draw->connect("gui_input",callable_mp(this, &ClassName::_drag_easing));
    easing_draw->set_default_cursor_shape(Control::CURSOR_MOVE);
    add_child(easing_draw);

    preset = memnew(PopupMenu);
    add_child(preset);
    preset->connect("id_pressed",callable_mp(this, &ClassName::_set_preset));

    spin = memnew(EditorSpinSlider);
    spin->set_flat(true);
    spin->set_min(-100);
    spin->set_max(100);
    spin->set_step(0);
    spin->set_hide_slider(true);
    spin->set_allow_lesser(true);
    spin->set_allow_greater(true);
    spin->connect("value_changed",callable_mp(this, &ClassName::_spin_value_changed));
    spin->get_line_edit()->connect("focus_exited",callable_mp(this, &ClassName::_spin_focus_exited));
    spin->hide();
    add_child(spin);

    dragging = false;
    flip = false;
    full = false;
}

///////////////////// VECTOR2 /////////////////////////

void EditorPropertyVector2::_value_changed(double val, const StringName &p_name) {
    if (setting)
        return;

    Vector2 v2;
    v2.x = spin[0]->get_value();
    v2.y = spin[1]->get_value();
    emit_changed(get_edited_property(), v2, p_name);
}

void EditorPropertyVector2::update_property() {
    Vector2 val = get_edited_object()->getT<Vector2>(get_edited_property());
    setting = true;
    spin[0]->set_value(val.x);
    spin[1]->set_value(val.y);
    setting = false;
}

void EditorPropertyVector2::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        Color base = get_theme_color("accent_color", "Editor");
        for (int i = 0; i < 2; i++) {

            Color c = base;
            c.set_hsv(float(i) / 3.0f + 0.05f, c.get_s() * 0.75f, c.get_v());
            spin[i]->set_custom_label_color(true, c);
        }
    }
}

void EditorPropertyVector2::setup(double p_min, double p_max, double p_step, bool p_no_slider) {
    for (int i = 0; i < 2; i++) {
        spin[i]->set_min(p_min);
        spin[i]->set_max(p_max);
        spin[i]->set_step(p_step);
        spin[i]->set_hide_slider(p_no_slider);
        spin[i]->set_allow_greater(true);
        spin[i]->set_allow_lesser(true);
    }
}

EditorPropertyVector2::EditorPropertyVector2() {
    bool horizontal = EDITOR_GET_T<bool>("interface/inspector/horizontal_vector2_editing");

    BoxContainer *bc;

    if (horizontal) {
        bc = memnew(HBoxContainer);
        add_child(bc);
        set_bottom_editor(bc);
    } else {
        bc = memnew(VBoxContainer);
        add_child(bc);
    }

    static const char *desc[2] = { "x", "y" };
    for (int i = 0; i < 2; i++) {
        spin[i] = memnew(EditorSpinSlider);
        spin[i]->set_flat(true);
        spin[i]->set_label(desc[i]);
        bc->add_child(spin[i]);
        add_focusable(spin[i]);
        spin[i]->connect("value_changed",callable_gen(this,[=](float v) { _value_changed(v,StringName(desc[i])); }));
        if (horizontal) {
            spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
        }
    }

    if (!horizontal) {
        set_label_reference(spin[0]); //show text and buttons around this
    }
    setting = false;
}

///////////////////// RECT2 /////////////////////////

void EditorPropertyRect2::_value_changed(double val, const StringName &p_name) {
    if (setting)
        return;

    Rect2 r2;
    r2.position.x = spin[0]->get_value();
    r2.position.y = spin[1]->get_value();
    r2.size.x = spin[2]->get_value();
    r2.size.y = spin[3]->get_value();
    emit_changed(get_edited_property(), r2, p_name);
}

void EditorPropertyRect2::update_property() {
    Rect2 val = get_edited_object()->getT<Rect2>(get_edited_property());
    setting = true;
    spin[0]->set_value(val.position.x);
    spin[1]->set_value(val.position.y);
    spin[2]->set_value(val.size.x);
    spin[3]->set_value(val.size.y);
    setting = false;
}
void EditorPropertyRect2::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        Color base = get_theme_color("accent_color", "Editor");
        for (int i = 0; i < 4; i++) {

            Color c = base;
            c.set_hsv(float(i % 2) / 3.0f + 0.05f, c.get_s() * 0.75f, c.get_v());
            spin[i]->set_custom_label_color(true, c);
        }
    }
}

void EditorPropertyRect2::setup(double p_min, double p_max, double p_step, bool p_no_slider) {
    for (int i = 0; i < 4; i++) {
        spin[i]->set_min(p_min);
        spin[i]->set_max(p_max);
        spin[i]->set_step(p_step);
        spin[i]->set_hide_slider(p_no_slider);
        spin[i]->set_allow_greater(true);
        spin[i]->set_allow_lesser(true);
    }
}

EditorPropertyRect2::EditorPropertyRect2() {

    bool horizontal = EDITOR_GET_T<bool>("interface/inspector/horizontal_vector_types_editing");

    BoxContainer *bc;

    if (horizontal) {
        bc = memnew(HBoxContainer);
        add_child(bc);
        set_bottom_editor(bc);
    } else {
        bc = memnew(VBoxContainer);
        add_child(bc);
    }

    static const char *desc[4] = { "x", "y", "w", "h" };
    for (int i = 0; i < 4; i++) {
        spin[i] = memnew(EditorSpinSlider);
        spin[i]->set_label(desc[i]);
        spin[i]->set_flat(true);
        bc->add_child(spin[i]);
        add_focusable(spin[i]);
        spin[i]->connect("value_changed",callable_gen(this,[=](float v) { _value_changed(v,StringName(desc[i])); }));
        if (horizontal) {
            spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
        }
    }

    if (!horizontal) {
        set_label_reference(spin[0]); //show text and buttons around this
    }
    setting = false;
}

///////////////////// VECTOR3 /////////////////////////

void EditorPropertyVector3::_value_changed(double val, const StringName &p_name) {
    if (setting)
        return;

    Vector3 v3;
    v3.x = spin[0]->get_value();
    v3.y = spin[1]->get_value();
    v3.z = spin[2]->get_value();
    emit_changed(get_edited_property(), v3, p_name);
}

void EditorPropertyVector3::update_property() {
    Vector3 val = get_edited_object()->getT<Vector3>(get_edited_property());
    setting = true;
    spin[0]->set_value(val.x);
    spin[1]->set_value(val.y);
    spin[2]->set_value(val.z);
    setting = false;
}
void EditorPropertyVector3::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        Color base = get_theme_color("accent_color", "Editor");
        for (int i = 0; i < 3; i++) {

            Color c = base;
            c.set_hsv(float(i) / 3.0f + 0.05f, c.get_s() * 0.75f, c.get_v());
            spin[i]->set_custom_label_color(true, c);
        }
    }
}

void EditorPropertyVector3::setup(double p_min, double p_max, double p_step, bool p_no_slider) {
    for (int i = 0; i < 3; i++) {
        spin[i]->set_min(p_min);
        spin[i]->set_max(p_max);
        spin[i]->set_step(p_step);
        spin[i]->set_hide_slider(p_no_slider);
        spin[i]->set_allow_greater(true);
        spin[i]->set_allow_lesser(true);
    }
}

EditorPropertyVector3::EditorPropertyVector3() {
    bool horizontal = EDITOR_GET_T<bool>("interface/inspector/horizontal_vector_types_editing");

    BoxContainer *bc;

    if (horizontal) {
        bc = memnew(HBoxContainer);
        add_child(bc);
        set_bottom_editor(bc);
    } else {
        bc = memnew(VBoxContainer);
        add_child(bc);
    }

    static const char *desc[3] = { "x", "y", "z" };
    for (int i = 0; i < 3; i++) {
        spin[i] = memnew(EditorSpinSlider);
        spin[i]->set_flat(true);
        spin[i]->set_label(desc[i]);
        bc->add_child(spin[i]);
        add_focusable(spin[i]);
        spin[i]->connect("value_changed",callable_gen(this,[=](float v) { _value_changed(v,StringName(desc[i])); }));
        if (horizontal) {
            spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
        }
    }

    if (!horizontal) {
        set_label_reference(spin[0]); //show text and buttons around this
    }
    setting = false;
}
///////////////////// PLANE /////////////////////////

void EditorPropertyPlane::_value_changed(double val, const StringName &p_name) {
    if (setting)
        return;

    Plane p;
    p.normal.x = spin[0]->get_value();
    p.normal.y = spin[1]->get_value();
    p.normal.z = spin[2]->get_value();
    p.d = spin[3]->get_value();
    emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyPlane::update_property() {
    Plane val = get_edited_object()->getT<Plane>(get_edited_property());
    setting = true;
    spin[0]->set_value(val.normal.x);
    spin[1]->set_value(val.normal.y);
    spin[2]->set_value(val.normal.z);
    spin[3]->set_value(val.d);
    setting = false;
}
void EditorPropertyPlane::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        Color base = get_theme_color("accent_color", "Editor");
        for (int i = 0; i < 3; i++) {

            Color c = base;
            c.set_hsv(float(i) / 3.0f + 0.05f, c.get_s() * 0.75f, c.get_v());
            spin[i]->set_custom_label_color(true, c);
        }
    }
}

void EditorPropertyPlane::setup(double p_min, double p_max, double p_step, bool p_no_slider) {
    for (int i = 0; i < 4; i++) {
        spin[i]->set_min(p_min);
        spin[i]->set_max(p_max);
        spin[i]->set_step(p_step);
        spin[i]->set_hide_slider(p_no_slider);
        spin[i]->set_allow_greater(true);
        spin[i]->set_allow_lesser(true);
    }
}

EditorPropertyPlane::EditorPropertyPlane() {

    bool horizontal = EDITOR_GET_T<bool>("interface/inspector/horizontal_vector_types_editing");

    BoxContainer *bc;

    if (horizontal) {
        bc = memnew(HBoxContainer);
        add_child(bc);
        set_bottom_editor(bc);
    } else {
        bc = memnew(VBoxContainer);
        add_child(bc);
    }

    static const char *desc[4] = { "x", "y", "z", "d" };
    for (int i = 0; i < 4; i++) {
        spin[i] = memnew(EditorSpinSlider);
        spin[i]->set_flat(true);
        spin[i]->set_label(desc[i]);
        bc->add_child(spin[i]);
        add_focusable(spin[i]);
        spin[i]->connect("value_changed",callable_gen(this,[=](float v) { _value_changed(v,StringName(desc[i])); }));
        if (horizontal) {
            spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
        }
    }

    if (!horizontal) {
        set_label_reference(spin[0]); //show text and buttons around this
    }
    setting = false;
}

///////////////////// QUAT /////////////////////////

void EditorPropertyQuat::_value_changed(double val, const StringName &p_name) {
    if (setting)
        return;

    Quat p;
    p.x = spin[0]->get_value();
    p.y = spin[1]->get_value();
    p.z = spin[2]->get_value();
    p.w = spin[3]->get_value();
    emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyQuat::update_property() {
    Quat val = get_edited_object()->getT<Quat>(get_edited_property());
    setting = true;
    spin[0]->set_value(val.x);
    spin[1]->set_value(val.y);
    spin[2]->set_value(val.z);
    spin[3]->set_value(val.w);
    setting = false;
}
void EditorPropertyQuat::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        Color base = get_theme_color("accent_color", "Editor");
        for (int i = 0; i < 3; i++) {

            Color c = base;
            c.set_hsv(float(i) / 3.0f + 0.05f, c.get_s() * 0.75f, c.get_v());
            spin[i]->set_custom_label_color(true, c);
        }
    }
}

void EditorPropertyQuat::setup(double p_min, double p_max, double p_step, bool p_no_slider) {
    for (int i = 0; i < 4; i++) {
        spin[i]->set_min(p_min);
        spin[i]->set_max(p_max);
        spin[i]->set_step(p_step);
        spin[i]->set_hide_slider(p_no_slider);
        spin[i]->set_allow_greater(true);
        spin[i]->set_allow_lesser(true);
    }
}

EditorPropertyQuat::EditorPropertyQuat() {
    bool horizontal = EDITOR_GET_T<bool>("interface/inspector/horizontal_vector_types_editing");

    BoxContainer *bc;

    if (horizontal) {
        bc = memnew(HBoxContainer);
        add_child(bc);
        set_bottom_editor(bc);
    } else {
        bc = memnew(VBoxContainer);
        add_child(bc);
    }

    static const char *desc[4] = { "x", "y", "z", "w" };
    for (int i = 0; i < 4; i++) {
        spin[i] = memnew(EditorSpinSlider);
        spin[i]->set_flat(true);
        spin[i]->set_label(desc[i]);
        bc->add_child(spin[i]);
        add_focusable(spin[i]);
        spin[i]->connect("value_changed",callable_gen(this,[=](float v) { _value_changed(v,StringName(desc[i])); }));
        if (horizontal) {
            spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
        }
    }

    if (!horizontal) {
        set_label_reference(spin[0]); //show text and buttons around this
    }
    setting = false;
}

///////////////////// AABB /////////////////////////

void EditorPropertyAABB::_value_changed(double val, const StringName &p_name) {
    if (setting)
        return;

    AABB p;
    p.position.x = spin[0]->get_value();
    p.position.y = spin[1]->get_value();
    p.position.z = spin[2]->get_value();
    p.size.x = spin[3]->get_value();
    p.size.y = spin[4]->get_value();
    p.size.z = spin[5]->get_value();

    emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyAABB::update_property() {
    AABB val = get_edited_object()->getT<AABB>(get_edited_property());
    setting = true;
    spin[0]->set_value(val.position.x);
    spin[1]->set_value(val.position.y);
    spin[2]->set_value(val.position.z);
    spin[3]->set_value(val.size.x);
    spin[4]->set_value(val.size.y);
    spin[5]->set_value(val.size.z);

    setting = false;
}
void EditorPropertyAABB::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        Color base = get_theme_color("accent_color", "Editor");
        for (int i = 0; i < 6; i++) {

            Color c = base;
            c.set_hsv(float(i % 3) / 3.0f + 0.05f, c.get_s() * 0.75f, c.get_v());
            spin[i]->set_custom_label_color(true, c);
        }
    }
}

void EditorPropertyAABB::setup(double p_min, double p_max, double p_step, bool p_no_slider) {
    for (int i = 0; i < 6; i++) {
        spin[i]->set_min(p_min);
        spin[i]->set_max(p_max);
        spin[i]->set_step(p_step);
        spin[i]->set_hide_slider(p_no_slider);
        spin[i]->set_allow_greater(true);
        spin[i]->set_allow_lesser(true);
    }
}

EditorPropertyAABB::EditorPropertyAABB() {
    GridContainer *g = memnew(GridContainer);
    g->set_columns(3);
    add_child(g);

    static const char *desc[6] = { "x", "y", "z", "w", "h", "d" };
    for (int i = 0; i < 6; i++) {
        spin[i] = memnew(EditorSpinSlider);
        spin[i]->set_label(desc[i]);
        spin[i]->set_flat(true);

        g->add_child(spin[i]);
        spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
        add_focusable(spin[i]);
        spin[i]->connect("value_changed",callable_gen(this,[=](float v) { _value_changed(v,StringName(desc[i])); }));
    }
    set_bottom_editor(g);
    setting = false;
}

///////////////////// TRANSFORM2D /////////////////////////

void EditorPropertyTransform2D::_value_changed(double val, const StringName &p_name) {
    if (setting)
        return;

    Transform2D p;
    p[0][0] = spin[0]->get_value();
    p[0][1] = spin[1]->get_value();
    p[1][0] = spin[2]->get_value();
    p[1][1] = spin[3]->get_value();
    p[2][0] = spin[4]->get_value();
    p[2][1] = spin[5]->get_value();

    emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyTransform2D::update_property() {
    Transform2D val = get_edited_object()->getT<Transform2D>(get_edited_property());
    setting = true;
    spin[0]->set_value(val[0][0]);
    spin[1]->set_value(val[0][1]);
    spin[2]->set_value(val[1][0]);
    spin[3]->set_value(val[1][1]);
    spin[4]->set_value(val[2][0]);
    spin[5]->set_value(val[2][1]);

    setting = false;
}
void EditorPropertyTransform2D::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        Color base = get_theme_color("accent_color", "Editor");
        for (int i = 0; i < 6; i++) {

            Color c = base;
            c.set_hsv(float(i % 2) / 3.0f + 0.05f, c.get_s() * 0.75f, c.get_v());
            spin[i]->set_custom_label_color(true, c);
        }
    }
}

void EditorPropertyTransform2D::setup(double p_min, double p_max, double p_step, bool p_no_slider) {
    for (int i = 0; i < 6; i++) {
        spin[i]->set_min(p_min);
        spin[i]->set_max(p_max);
        spin[i]->set_step(p_step);
        spin[i]->set_hide_slider(p_no_slider);
        spin[i]->set_allow_greater(true);
        spin[i]->set_allow_lesser(true);
    }
}

EditorPropertyTransform2D::EditorPropertyTransform2D() {
    GridContainer *g = memnew(GridContainer);
    g->set_columns(2);
    add_child(g);

    static const char *desc[6] = { "x", "y", "x", "y", "x", "y" };
    for (int i = 0; i < 6; i++) {
        spin[i] = memnew(EditorSpinSlider);
        spin[i]->set_label(desc[i]);
        spin[i]->set_flat(true);
        g->add_child(spin[i]);
        spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
        add_focusable(spin[i]);
        spin[i]->connect("value_changed",callable_gen(this,[=](float v) { _value_changed(v,StringName(desc[i])); }));
    }
    set_bottom_editor(g);
    setting = false;
}

///////////////////// BASIS /////////////////////////

void EditorPropertyBasis::_value_changed(double val, const StringName &p_name) {
    if (setting)
        return;

    Basis p;
    p[0][0] = spin[0]->get_value();
    p[1][0] = spin[1]->get_value();
    p[2][0] = spin[2]->get_value();
    p[0][1] = spin[3]->get_value();
    p[1][1] = spin[4]->get_value();
    p[2][1] = spin[5]->get_value();
    p[0][2] = spin[6]->get_value();
    p[1][2] = spin[7]->get_value();
    p[2][2] = spin[8]->get_value();

    emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyBasis::update_property() {
    Basis val = get_edited_object()->getT<Basis>(get_edited_property());
    setting = true;
    spin[0]->set_value(val[0][0]);
    spin[1]->set_value(val[1][0]);
    spin[2]->set_value(val[2][0]);
    spin[3]->set_value(val[0][1]);
    spin[4]->set_value(val[1][1]);
    spin[5]->set_value(val[2][1]);
    spin[6]->set_value(val[0][2]);
    spin[7]->set_value(val[1][2]);
    spin[8]->set_value(val[2][2]);

    setting = false;
}
void EditorPropertyBasis::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        Color base = get_theme_color("accent_color", "Editor");
        for (int i = 0; i < 9; i++) {

            Color c = base;
            c.set_hsv(float(i % 3) / 3.0f + 0.05f, c.get_s() * 0.75f, c.get_v());
            spin[i]->set_custom_label_color(true, c);
        }
    }
}

void EditorPropertyBasis::setup(double p_min, double p_max, double p_step, bool p_no_slider) {
    for (int i = 0; i < 9; i++) {
        spin[i]->set_min(p_min);
        spin[i]->set_max(p_max);
        spin[i]->set_step(p_step);
        spin[i]->set_hide_slider(p_no_slider);
        spin[i]->set_allow_greater(true);
        spin[i]->set_allow_lesser(true);
    }
}

EditorPropertyBasis::EditorPropertyBasis() {
    GridContainer *g = memnew(GridContainer);
    g->set_columns(3);
    add_child(g);

    static const char *desc[9] = { "x", "y", "z", "x", "y", "z", "x", "y", "z" };
    for (int i = 0; i < 9; i++) {
        spin[i] = memnew(EditorSpinSlider);
        spin[i]->set_label(desc[i]);
        spin[i]->set_flat(true);
        g->add_child(spin[i]);
        spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
        add_focusable(spin[i]);
        spin[i]->connect("value_changed",callable_gen(this,[=](float v) { _value_changed(v,StringName(desc[i])); }));
    }
    set_bottom_editor(g);
    setting = false;
}

///////////////////// TRANSFORM /////////////////////////

void EditorPropertyTransform::_value_changed(double val, const StringName &p_name) {
    if (setting)
        return;

    Transform p;
    p.basis[0][0] = spin[0]->get_value();
    p.basis[1][0] = spin[1]->get_value();
    p.basis[2][0] = spin[2]->get_value();
    p.basis[0][1] = spin[3]->get_value();
    p.basis[1][1] = spin[4]->get_value();
    p.basis[2][1] = spin[5]->get_value();
    p.basis[0][2] = spin[6]->get_value();
    p.basis[1][2] = spin[7]->get_value();
    p.basis[2][2] = spin[8]->get_value();
    p.origin[0] = spin[9]->get_value();
    p.origin[1] = spin[10]->get_value();
    p.origin[2] = spin[11]->get_value();

    emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyTransform::update_property() {
    Transform val = get_edited_object()->getT<Transform>(get_edited_property());
    setting = true;
    spin[0]->set_value(val.basis[0][0]);
    spin[1]->set_value(val.basis[1][0]);
    spin[2]->set_value(val.basis[2][0]);
    spin[3]->set_value(val.basis[0][1]);
    spin[4]->set_value(val.basis[1][1]);
    spin[5]->set_value(val.basis[2][1]);
    spin[6]->set_value(val.basis[0][2]);
    spin[7]->set_value(val.basis[1][2]);
    spin[8]->set_value(val.basis[2][2]);
    spin[9]->set_value(val.origin[0]);
    spin[10]->set_value(val.origin[1]);
    spin[11]->set_value(val.origin[2]);

    setting = false;
}
void EditorPropertyTransform::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        Color base = get_theme_color("accent_color", "Editor");
        for (int i = 0; i < 12; i++) {

            Color c = base;
            c.set_hsv(float(i % 3) / 3.0f + 0.05f, c.get_s() * 0.75f, c.get_v());
            spin[i]->set_custom_label_color(true, c);
        }
    }
}

void EditorPropertyTransform::setup(double p_min, double p_max, double p_step, bool p_no_slider) {
    for (int i = 0; i < 12; i++) {
        spin[i]->set_min(p_min);
        spin[i]->set_max(p_max);
        spin[i]->set_step(p_step);
        spin[i]->set_hide_slider(p_no_slider);
        spin[i]->set_allow_greater(true);
        spin[i]->set_allow_lesser(true);
    }
}

EditorPropertyTransform::EditorPropertyTransform() {
    GridContainer *g = memnew(GridContainer);
    g->set_columns(3);
    add_child(g);

    static const char *desc[12] = { "x", "y", "z", "x", "y", "z", "x", "y", "z", "x", "y", "z" };
    for (int i = 0; i < 12; i++) {
        spin[i] = memnew(EditorSpinSlider);
        spin[i]->set_label(desc[i]);
        spin[i]->set_flat(true);
        g->add_child(spin[i]);
        spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
        add_focusable(spin[i]);
        spin[i]->connect("value_changed",callable_gen(this,[=](float v) { _value_changed(v,StringName(desc[i])); }));
    }
    set_bottom_editor(g);
    setting = false;
}

////////////// COLOR PICKER //////////////////////

void EditorPropertyColor::_color_changed(const Color &p_color) {
    // Cancel the color change if the current color is identical to the new one.
    if (get_edited_object()->get(get_edited_property()) == p_color) {
        return;
    }
    emit_changed(get_edited_property(), p_color, "", true);
}

void EditorPropertyColor::_popup_closed() {
    if (picker->get_pick_color() != last_color) {
        emit_changed(get_edited_property(), picker->get_pick_color(), "", false);
    }
}

void EditorPropertyColor::_picker_created() {
    // get default color picker mode from editor settings
    int default_color_mode = EDITOR_GET_T<int>("interface/inspector/default_color_picker_mode");
    if (default_color_mode == 1)
        picker->get_picker()->set_hsv_mode(true);
    else if (default_color_mode == 2)
        picker->get_picker()->set_raw_mode(true);
}

void EditorPropertyColor::_picker_opening() {
    last_color = picker->get_pick_color();
}


void EditorPropertyColor::update_property() {

    picker->set_pick_color(get_edited_object()->getT<Color>(get_edited_property()));
    const Color color = picker->get_pick_color();

    // Add a tooltip to display each channel's values without having to click the ColorPickerButton
    if (picker->is_editing_alpha()) {
        picker->set_tooltip(FormatSN(
                "R: %s\nG: %s\nB: %s\nA: %s",
                StringUtils::pad_decimals(StringUtils::num(color.r),2).c_str(),
                StringUtils::pad_decimals(StringUtils::num(color.g),2).c_str(),
                StringUtils::pad_decimals(StringUtils::num(color.b),2).c_str(),
                StringUtils::pad_decimals(StringUtils::num(color.a),2).c_str()
                ));
    } else {
        picker->set_tooltip(FormatSN(
                "R: %s\nG: %s\nB: %s",
                StringUtils::pad_decimals(StringUtils::num(color.r),2).c_str(),
                StringUtils::pad_decimals(StringUtils::num(color.g),2).c_str(),
                StringUtils::pad_decimals(StringUtils::num(color.b),2).c_str()
                ));
    }
}

void EditorPropertyColor::setup(bool p_show_alpha) {
    picker->set_edit_alpha(p_show_alpha);
}

EditorPropertyColor::EditorPropertyColor() {

    picker = memnew(ColorPickerButton);
    add_child(picker);
    picker->set_flat(true);
    picker->connect("color_changed",callable_mp(this, &ClassName::_color_changed));
    picker->connect("popup_closed",callable_mp(this, &ClassName::_popup_closed));
    picker->connect("picker_created",callable_mp(this, &ClassName::_picker_created));
    picker->get_popup()->connect("about_to_show", callable_mp(this, &ClassName::_picker_opening));
}

////////////// NODE PATH //////////////////////

void EditorPropertyNodePath::_node_selected(const NodePath &p_path) {

    NodePath path = p_path;
    Node *base_node = nullptr;

    if (!use_path_from_scene_root) {
        base_node = object_cast<Node>(get_edited_object());

        if (!base_node) {
            //try a base node within history
            if (EditorNode::get_singleton()->get_editor_history()->get_path_size() > 0) {
                Object *base = object_for_entity(EditorNode::get_singleton()->get_editor_history()->get_path_object(0));
                if (base) {
                    base_node = object_cast<Node>(base);
                }
            }
        }
    }

    if (!base_node && get_edited_object()->has_method("get_root_path")) {
        base_node = get_edited_object()->call_va("get_root_path").as<Node *>();
    }

    if (!base_node && object_cast<RefCounted>(get_edited_object())) {
        Node *to_node = get_node(p_path);
        ERR_FAIL_COND(!to_node);
        path = get_tree()->get_edited_scene_root()->get_path_to(to_node);
    }

    if (base_node) { // for AnimationTrackKeyEdit
        path = base_node->get_path().rel_path_to(p_path);
    }
    emit_changed(get_edited_property(), path);
    update_property();
}

void EditorPropertyNodePath::_node_assign() {
    if (!scene_tree) {
        scene_tree = memnew(SceneTreeDialog);
        scene_tree->get_scene_tree()->set_show_enabled_subscene(true);
        scene_tree->get_scene_tree()->set_valid_types(valid_types);
        add_child(scene_tree);
        scene_tree->connect("selected",callable_mp(this, &ClassName::_node_selected));
    }
    scene_tree->popup_centered_ratio();
}

void EditorPropertyNodePath::_node_clear() {

    emit_changed(get_edited_property(), NodePath());
    update_property();
}

bool EditorPropertyNodePath::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
    return !is_read_only() && is_drop_valid(p_data.as<Dictionary>());
}

void EditorPropertyNodePath::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
    Dictionary data = p_data.as<Dictionary>();
    ERR_FAIL_COND(!is_drop_valid(data));
    Array nodes = data["nodes"].as<Array>();
    Node *node = get_tree()->get_edited_scene_root()->get_node(nodes[0].as<NodePath>());

    if (node) {
        _node_selected(node->get_path());
    }
}

bool EditorPropertyNodePath::is_drop_valid(const Dictionary &p_drag_data) const {
    if (p_drag_data["type"] != "nodes") {
        return false;
    }
    Array nodes = p_drag_data["nodes"].as<Array>();
    return nodes.size() == 1;
}

void EditorPropertyNodePath::update_property() {

    NodePath p = get_edited_object()->get(get_edited_property()).as<NodePath>();

    assign->set_tooltip(String(p));
    if (p == NodePath()) {
        assign->set_button_icon(Ref<Texture>());
        assign->set_text(TTR("Assign..."));
        assign->set_flat(false);
        return;
    }
    assign->set_flat(true);

    Node *base_node = nullptr;
    if (!base_hint.empty()) {
        if (get_tree()->get_root()->has_node(base_hint)) {
            base_node = get_tree()->get_root()->get_node(base_hint);
        }
    } else {
        base_node = object_cast<Node>(get_edited_object());
    }

    if (!base_node || !base_node->has_node(p)) {
        assign->set_button_icon(Ref<Texture>());
        assign->set_text(String(p));
        return;
    }

    Node *target_node = base_node->get_node(p);
    ERR_FAIL_COND(!target_node);

    if (StringUtils::contains(String(target_node->get_name()),"@")) {
        assign->set_button_icon(Ref<Texture>());
        assign->set_text(String(p));
        return;
    }

    assign->set_text(target_node->get_name());
    assign->set_button_icon(EditorNode::get_singleton()->get_object_icon(target_node, "Node"));
}

void EditorPropertyNodePath::setup(const NodePath &p_base_hint, Vector<StringName> &&p_valid_types, bool p_use_path_from_scene_root) {

    base_hint = p_base_hint;
    valid_types = p_valid_types;
    use_path_from_scene_root = p_use_path_from_scene_root;
}

void EditorPropertyNodePath::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
        Ref<Texture> t = get_theme_icon("Clear", "EditorIcons");
        clear->set_button_icon(t);
    }
}

EditorPropertyNodePath::EditorPropertyNodePath() {

    HBoxContainer *hbc = memnew(HBoxContainer);
    add_child(hbc);
    assign = memnew(Button);
    assign->set_flat(true);
    assign->set_h_size_flags(SIZE_EXPAND_FILL);
    assign->set_clip_text(true);
    assign->connect("pressed",callable_mp(this, &ClassName::_node_assign));
    assign->set_drag_forwarding(this);
    hbc->add_child(assign);

    clear = memnew(Button);
    clear->set_flat(true);
    clear->connect("pressed",callable_mp(this, &ClassName::_node_clear));
    hbc->add_child(clear);
    use_path_from_scene_root = false;

    scene_tree = nullptr; //do not allocate unnecessarily
}

///////////////////// RID /////////////////////////

void EditorPropertyRID::update_property() {
    RID rid = get_edited_object()->get(get_edited_property()).as<RID>();
    if (rid.is_valid()) {
        int id = rid.get_id();
        label->set_text(StringName("RID: " + itos(id)));
    } else {
        label->set_text(TTR("Invalid RID"));
    }
}

EditorPropertyRID::EditorPropertyRID() {
    label = memnew(Label);
    add_child(label);
}

////////////// RESOURCE //////////////////////

void EditorPropertyResource::_resource_selected(const RES &p_resource, bool p_edit) {
    if (!p_edit && _can_use_sub_inspector(p_resource)) {
        auto tool_iface=get_edited_object()->get_tooling_interface();
        bool unfold = !tool_iface->editor_is_section_unfolded(get_edited_property());
        tool_iface->editor_set_section_unfold(get_edited_property(), unfold);
        update_property();
    } else {
        emit_signal("resource_selected", get_edited_property(), p_resource);
    }
}
void EditorPropertyResource::_resource_changed(const RES &p_resource) {
    // Make visual script the correct type.
    Ref<Script> s = dynamic_ref_cast<Script>(p_resource);
    if (get_edited_object() && s) {
        s->call_va("set_instance_base_type", get_edited_object()->get_class());
    }

    // Prevent the creation of invalid ViewportTextures when possible.
    Ref<ViewportTexture> vpt = dynamic_ref_cast<ViewportTexture>(p_resource);
    if (vpt) {
        Resource *r = object_cast<Resource>(get_edited_object());
        if (r && PathUtils::is_resource_file(r->get_path())) {
            EditorNode::get_singleton()->show_warning(TTR("Can't create a ViewportTexture on resources saved as a file.\nResource needs to belong to a scene."));
            emit_changed(get_edited_property(), RES());
            update_property();
            return;
        }

        if (r && !r->is_local_to_scene()) {
            EditorNode::get_singleton()->show_warning(TTR("Can't create a ViewportTexture on this resource because it's not set as local to scene.\nPlease switch on the 'local to scene' property on it (and all resources containing it up to a node)."));
            emit_changed(get_edited_property(), RES());
            update_property();
            return;
        }
    }

    emit_changed(get_edited_property(), p_resource);
    update_property();

    // Automatically suggest setting up the path for a ViewportTexture.
    if (vpt && vpt->get_viewport_path_in_scene().is_empty()) {
        if (!scene_tree) {
            scene_tree = memnew(SceneTreeDialog);
            scene_tree->set_title(TTR("Pick a Viewport"));

            Vector<StringName> valid_types;
            valid_types.push_back("Viewport");
            scene_tree->get_scene_tree()->set_valid_types(valid_types);
            scene_tree->get_scene_tree()->set_show_enabled_subscene(true);

            add_child(scene_tree);
            scene_tree->connect("selected", callable_mp(this, &EditorPropertyResource::_viewport_selected));
        }

        scene_tree->popup_centered_ratio();
    }
}
void EditorPropertyResource::_sub_inspector_property_keyed(StringView p_property, const Variant &p_value, bool) {

    emit_signal("property_keyed_with_value", String(get_edited_property()) + ":" + p_property, p_value, false);
}

void EditorPropertyResource::_sub_inspector_resource_selected(const RES &p_resource, StringView p_property) {

    emit_signal("resource_selected", String(get_edited_property()) + ":" + p_property, p_resource);
}

void EditorPropertyResource::_sub_inspector_object_id_selected(int p_id) {

    emit_signal("object_id_selected", get_edited_property(), p_id);
}

void EditorPropertyResource::_open_editor_pressed() {
    RES res(get_edited_object()->get(get_edited_property()));
    if (res) {
        // May clear the editor so do it deferred.
        EditorNode::get_singleton()->call_deferred([res = eastl::move(res)] {
            EditorNode::get_singleton()->edit_item_resource(res);
        });
    }
}

void EditorPropertyResource::_fold_other_editors(Object *p_self) {

    if (this == p_self) {
        return;
    }

    RES res(get_edited_object()->get(get_edited_property()));

    if (not res)
        return;
    bool use_editor = false;
    for (int i = 0; i < EditorNode::get_editor_data().get_editor_plugin_count(); i++) {
        EditorPlugin *ep = EditorNode::get_editor_data().get_editor_plugin(i);
        if (ep->handles(res.get())) {
            use_editor = true;
        }
    }

    if (!use_editor)
        return;

    opened_editor = false;
    bool unfolded = get_edited_object()->get_tooling_interface()->editor_is_section_unfolded(get_edited_property());
    if (unfolded) {
        // Refold.
        resource_picker->set_toggle_pressed(false);
        get_edited_object()->get_tooling_interface()->editor_set_section_unfold(get_edited_property(), false);
        update_property();
    }
}

void EditorPropertyResource::_update_property_bg() {
    if (!is_inside_tree()) {
        return;
    }

    updating_theme = true;
    if (sub_inspector != nullptr) {
        int count_subinspectors = 0;
        Node *n = get_parent();
        while (n) {
            EditorInspector *ei = object_cast<EditorInspector>(n);
            if (ei && ei->is_sub_inspector()) {
                count_subinspectors++;
            }
            n = n->get_parent();
        }
        count_subinspectors = MIN(15, count_subinspectors);

        add_theme_color_override("property_color", get_theme_color("sub_inspector_property_color", "Editor"));
        add_theme_style_override("bg_selected", get_theme_stylebox(StringName("sub_inspector_property_bg_selected" + itos(count_subinspectors)), "Editor"));
        add_theme_style_override("bg", get_theme_stylebox(StringName("sub_inspector_property_bg" + itos(count_subinspectors)), "Editor"));

        add_constant_override("font_offset", get_theme_constant("sub_inspector_font_offset", "Editor"));
        add_constant_override("vseparation", 0);
    } else {
        add_theme_color_override("property_color", get_theme_color("property_color", "EditorProperty"));
        add_theme_style_override("bg_selected", get_theme_stylebox("bg_selected", "EditorProperty"));
        add_theme_style_override("bg", get_theme_stylebox("bg", "EditorProperty"));
        add_constant_override("vseparation", get_theme_constant("vseparation", "EditorProperty"));
        add_constant_override("font_offset", get_theme_constant("font_offset", "EditorProperty"));
    }

    updating_theme = false;
    update();
}

void EditorPropertyResource::_viewport_selected(const NodePath &p_path) {
    Node *to_node = get_node(p_path);
    if (!object_cast<Viewport>(to_node)) {
        EditorNode::get_singleton()->show_warning(TTR("Selected node is not a Viewport!"));
        return;
    }

    Ref<ViewportTexture> vt(make_ref_counted<ViewportTexture>());
    vt->set_viewport_path_in_scene(get_tree()->get_edited_scene_root()->get_path_to(to_node));
    vt->setup_local_to_scene();

    emit_changed(get_edited_property(), vt);
    update_property();
}

bool EditorPropertyResource::_can_use_sub_inspector(const RES &p_resource) {
    bool use_editor = false;
    if (p_resource) {
        for (int i = 0; i < EditorNode::get_editor_data().get_editor_plugin_count(); i++) {
            EditorPlugin *ep = EditorNode::get_editor_data().get_editor_plugin(i);
            if (ep->handles(p_resource.get())) {
                use_editor = true;
            }
        }
    }

    return !use_editor && use_sub_inspector;
}
void EditorPropertyResource::setup(Object *p_object, StringView p_path, const StringName &p_base_type) {
    if (resource_picker) {
        resource_picker->disconnect("resource_selected", callable_mp(this, &EditorPropertyResource::_resource_selected));
        resource_picker->disconnect("resource_changed", callable_mp(this, &EditorPropertyResource::_resource_changed));
        memdelete(resource_picker);
    }

    if (p_path == "script" && p_base_type == "Script" && object_cast<Node>(p_object)) {
        EditorScriptPicker *script_picker = memnew(EditorScriptPicker);
        script_picker->set_script_owner(object_cast<Node>(p_object));
        resource_picker = script_picker;
    } else {
        resource_picker = memnew(EditorResourcePicker);
    }

    resource_picker->set_base_type(p_base_type);
    resource_picker->set_editable(true);
    resource_picker->set_h_size_flags(SIZE_EXPAND_FILL);
    add_child(resource_picker);

    resource_picker->connect("resource_selected", callable_mp(this, &EditorPropertyResource::_resource_selected));
    resource_picker->connect("resource_changed", callable_mp(this, &EditorPropertyResource::_resource_changed));

    for (int i = 0; i < resource_picker->get_child_count(); i++) {
        Button *b = object_cast<Button>(resource_picker->get_child(i));
        if (b) {
            add_focusable(b);
        }
    }
}
void EditorPropertyResource::update_property() {
    RES res(get_edited_object()->get(get_edited_property()));

    if (!_can_use_sub_inspector(res)) {
        resource_picker->set_edited_resource(res);
        return;
    }
    if (bool(res) != resource_picker->is_toggle_mode()) {
        resource_picker->set_toggle_mode(bool(res));
    }

    if (res && get_edited_object()->get_tooling_interface()->editor_is_section_unfolded(get_edited_property())) {
        if (!sub_inspector) {
            sub_inspector = memnew(EditorInspector);
            sub_inspector->set_enable_v_scroll(false);
            sub_inspector->set_use_doc_hints(true);

            sub_inspector->set_sub_inspector(true);
            sub_inspector->set_property_name_style(
                    EditorNode::get_singleton()->get_inspector_dock()->get_property_name_style());

            sub_inspector->connect("property_keyed", callable_mp(this, &ClassName::_sub_inspector_property_keyed));
            sub_inspector->connect(
                    "resource_selected", callable_mp(this, &ClassName::_sub_inspector_resource_selected));
            sub_inspector->connect(
                    "object_id_selected", callable_mp(this, &ClassName::_sub_inspector_object_id_selected));
            sub_inspector->set_keying(is_keying());
            sub_inspector->set_read_only(is_read_only());
            sub_inspector->set_use_folding(is_using_folding());
            sub_inspector->set_undo_redo(EditorNode::get_undo_redo());

            sub_inspector_vbox = memnew(VBoxContainer);
            add_child(sub_inspector_vbox);
            set_bottom_editor(sub_inspector_vbox);

            sub_inspector_vbox->add_child(sub_inspector);
            resource_picker->set_toggle_pressed(true);

            bool use_editor = false;
            for (int i = 0; i < EditorNode::get_editor_data().get_editor_plugin_count(); i++) {
                EditorPlugin *ep = EditorNode::get_editor_data().get_editor_plugin(i);
                if (ep->handles(res.get())) {
                    use_editor = true;
                }
            }
            if (use_editor) {
                // Open editor directly and hide other such editors which are currently open.
                _open_editor_pressed();
                if (is_inside_tree()) {
                    get_tree()->call_deferred([=]() {
                        get_tree()->call_group(
                                "_editor_resource_properties", "_fold_other_editors", Variant::from(this));
                    });
                }
                opened_editor = true;
            }
            _update_property_bg();
        }

        if (res.get() != sub_inspector->get_edited_object()) {
            sub_inspector->edit(res.get());
        }

        sub_inspector->refresh();
    } else {
        if (sub_inspector) {
            set_bottom_editor(nullptr);
            memdelete(sub_inspector_vbox);
            sub_inspector = nullptr;
            sub_inspector_vbox = nullptr;
            if (opened_editor) {
                EditorNode::get_singleton()->hide_top_editors();
                opened_editor = false;
            }
            _update_property_bg();
        }
    }
    resource_picker->set_edited_resource(res);
}

void EditorPropertyResource::collapse_all_folding() {
    if (sub_inspector) {
        sub_inspector->collapse_all_folding();
    }
}

void EditorPropertyResource::expand_all_folding() {

    if (sub_inspector) {
        sub_inspector->expand_all_folding();
    }
}

void EditorPropertyResource::set_use_sub_inspector(bool p_enable) {
    use_sub_inspector = p_enable;
}

void EditorPropertyResource::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE:
        case NOTIFICATION_THEME_CHANGED: {
            if (!updating_theme) {
                _update_property_bg();
            }
        } break;
    }
}

void EditorPropertyResource::_bind_methods() {

    SE_BIND_METHOD(EditorPropertyResource,_resource_selected);
    SE_BIND_METHOD(EditorPropertyResource,_fold_other_editors);
}

EditorPropertyResource::EditorPropertyResource() {

    use_sub_inspector = EDITOR_GET_T<bool>("interface/inspector/open_resources_in_current_inspector");
    add_to_group("_editor_resource_properties");
}

////////////// DEFAULT PLUGIN //////////////////////

bool EditorInspectorDefaultPlugin::can_handle(Object *p_object) {
    return true; //can handle everything
}

void EditorInspectorDefaultPlugin::parse_begin(Object *p_object) {
    //do none
}

bool EditorInspectorDefaultPlugin::parse_property(Object *p_object, VariantType p_type, StringView p_path, PropertyHint p_hint, StringView p_hint_text, int p_usage) {

    float default_float_step = EDITOR_GET_T<float>("interface/inspector/default_float_step");

    switch (p_type) {

        // atomic types
        case VariantType::NIL: {
            EditorPropertyNil *editor = memnew(EditorPropertyNil);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::BOOL: {
            EditorPropertyCheck *editor = memnew(EditorPropertyCheck);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::INT: {

            if (p_hint == PropertyHint::Enum) {
                EditorPropertyEnum *editor = memnew(EditorPropertyEnum);
                Vector<StringView> options = StringUtils::split(p_hint_text,',');
                editor->setup(options);
                add_property_editor(p_path, editor);

            } else if (p_hint == PropertyHint::Flags) {
                EditorPropertyFlags *editor = memnew(EditorPropertyFlags);
                Vector<StringView> options = StringUtils::split(p_hint_text,',');
                editor->setup(options);
                add_property_editor(p_path, editor);

            } else if (p_hint == PropertyHint::Layers2DPhysics || p_hint == PropertyHint::Layers2DRenderer || p_hint == PropertyHint::Layers3DPhysics || p_hint == PropertyHint::Layers3DRenderer) {

                EditorPropertyLayers::LayerType lt = EditorPropertyLayers::LAYER_RENDER_2D;
                switch (p_hint) {
                    case PropertyHint::Layers2DRenderer:
                        lt = EditorPropertyLayers::LAYER_RENDER_2D;
                        break;
                    case PropertyHint::Layers2DPhysics:
                        lt = EditorPropertyLayers::LAYER_PHYSICS_2D;
                        break;
                    case PropertyHint::Layers3DRenderer:
                        lt = EditorPropertyLayers::LAYER_RENDER_3D;
                        break;
                    case PropertyHint::Layers3DPhysics:
                        lt = EditorPropertyLayers::LAYER_PHYSICS_3D;
                        break;
                    default: {
                    } //compiler could be smarter here and realize this can't happen
                }
                EditorPropertyLayers *editor = memnew(EditorPropertyLayers);
                editor->setup(lt);
                add_property_editor(p_path, editor);
            } else if (p_hint == PropertyHint::ObjectID) {

                EditorPropertyObjectID *editor = memnew(EditorPropertyObjectID);
                editor->setup(StringName(p_hint_text));
                add_property_editor(p_path, editor);

            } else {
                EditorPropertyInteger *editor = memnew(EditorPropertyInteger);
                int min = 0, max = 65535, step = 1;
                bool greater = true, lesser = true;
                FixedVector<StringView,6,true> slices;
                String::split_ref(slices,p_hint_text,',');

                if (p_hint == PropertyHint::Range && slices.size() >= 2) {
                    greater = false; // If using ranged, assume false by default.
                    lesser = false;
                    min = StringUtils::to_int(slices[0]);
                    max = StringUtils::to_int(slices[1]);

                    if (slices.size() >= 3 && StringUtils::is_valid_integer(slices[2])) {
                        // Step is optional, could be something else if not a number.
                        step = StringUtils::to_int(slices[2]);
                    }

                    for (int i = 2; i < slices.size(); i++) {
                        StringView slice = StringUtils::strip_edges(slices[i]);
                        if (slice == StringView("or_greater")) {
                            greater = true;
                        }
                        if (slice == StringView("or_lesser")) {
                            lesser = true;
                        }
                    }
                }

                editor->setup(min, max, step, greater, lesser);

                add_property_editor(p_path, editor);
            }
        } break;
        case VariantType::FLOAT: {

            if (p_hint == PropertyHint::ExpEasing) {
                EditorPropertyEasing *editor = memnew(EditorPropertyEasing);
                bool full = true;
                bool flip = false;
                Vector<StringView> hints = StringUtils::split(p_hint_text,',');
                for (auto & hint : hints) {
                    StringView h =StringUtils::strip_edges( hint);
                    if (h == StringView("attenuation")) {
                        flip = true;
                    }
                    if (h == StringView("inout")) {
                        full = true;
                    }
                }

                editor->setup(full, flip);
                add_property_editor(p_path, editor);

            } else {
                EditorPropertyFloat *editor = memnew(EditorPropertyFloat);
                double min = -65535, max = 65535, step = default_float_step;
                bool hide_slider = true;
                bool exp_range = false;
                bool greater = true, lesser = true;
                FixedVector<StringView,6,true> slices;
                String::split_ref(slices,p_hint_text,',');

                if ((p_hint == PropertyHint::Range || p_hint == PropertyHint::ExpRange) && slices.size() >= 2) {
                    greater = false; // If using ranged, assume false by default.
                    lesser = false;
                    min = StringUtils::to_double(slices[0]);
                    max = StringUtils::to_double(slices[1]);
                    if (slices.size() >= 3) { // Step is optional, could be something else if not a number.
                        step = StringUtils::to_double(slices[2]);
                    }
                    hide_slider = false;
                    exp_range = p_hint == PropertyHint::ExpRange;
                    for (int i = 2; i <slices.size(); i++) {
                        StringView slice = StringUtils::strip_edges(slices[i]);
                        if (slice == StringView("or_greater")) {
                            greater = true;
                        }
                        if (slice == StringView("or_lesser")) {
                            lesser = true;
                        }
                    }
                }

                editor->setup(min, max, step, hide_slider, exp_range, greater, lesser);

                add_property_editor(p_path, editor);
            }
        } break;
        case VariantType::STRING: {

            if (p_hint == PropertyHint::Enum) {
                EditorPropertyTextEnum *editor = memnew(EditorPropertyTextEnum);
                Vector<StringView> options = StringUtils::split(p_hint_text,',');
                editor->setup(options);
                add_property_editor(p_path, editor);
            } else if (p_hint == PropertyHint::MultilineText) {
                EditorPropertyMultilineText *editor = memnew(EditorPropertyMultilineText);
                add_property_editor(p_path, editor);
            } else if (p_hint == PropertyHint::TypeString) {
                EditorPropertyClassName *editor = memnew(EditorPropertyClassName);
                editor->setup("Object", StringName(p_hint_text));
                add_property_editor(p_path, editor);
            } else if (p_hint == PropertyHint::Dir || p_hint == PropertyHint::File || p_hint == PropertyHint::SaveFile || p_hint == PropertyHint::GlobalDir || p_hint == PropertyHint::GlobalFile) {

                Vector<StringView> extensions = StringUtils::split(p_hint_text,',');
                bool global = p_hint == PropertyHint::GlobalDir || p_hint == PropertyHint::GlobalFile;
                bool folder = p_hint == PropertyHint::Dir || p_hint == PropertyHint::GlobalDir;
                bool save = p_hint == PropertyHint::SaveFile;
                EditorPropertyPath *editor = memnew(EditorPropertyPath);
                editor->setup(extensions, folder, global);
                if (save) {
                    editor->set_save_mode();
                }
                add_property_editor(p_path, editor);
            } else if (p_hint == PropertyHint::PropertyOfVariantType) {

                EditorPropertyMember *editor = memnew(EditorPropertyMember);

                EditorPropertyMember::Type type = EditorPropertyMember::MEMBER_PROPERTY_OF_VARIANT_TYPE;
                type = EditorPropertyMember::MEMBER_PROPERTY_OF_VARIANT_TYPE;
                editor->setup(type, p_hint_text);
                add_property_editor(p_path, editor);

            } else {

                EditorPropertyText *editor = memnew(EditorPropertyText);
                if (p_hint == PropertyHint::PlaceholderText) {
                    editor->set_placeholder(StringName(p_hint_text));
                }
                add_property_editor(p_path, editor);
            }
        } break;

            // math types

        case VariantType::VECTOR2: {
            EditorPropertyVector2 *editor = memnew(EditorPropertyVector2);
            double min = -65535, max = 65535, step = default_float_step;
            bool hide_slider = true;

            if (p_hint == PropertyHint::Range &&StringUtils::get_slice_count( p_hint_text,',') >= 2) {
                min = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 0));
                max = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 1));
                if (StringUtils::get_slice_count(p_hint_text,',') >= 3) {
                    step = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 2));
                }
                hide_slider = false;
            }

            editor->setup(min, max, step, hide_slider);
            add_property_editor(p_path, editor);

        } break; // 5
        case VariantType::RECT2: {
            EditorPropertyRect2 *editor = memnew(EditorPropertyRect2);
            double min = -65535, max = 65535, step = default_float_step;
            bool hide_slider = true;

            if (p_hint == PropertyHint::Range &&StringUtils::get_slice_count( p_hint_text,',') >= 2) {
                min = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 0));
                max = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 1));
                if (StringUtils::get_slice_count(p_hint_text,',') >= 3) {
                    step = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 2));
                }
                hide_slider = false;
            }

            editor->setup(min, max, step, hide_slider);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::VECTOR3: {
            EditorPropertyVector3 *editor = memnew(EditorPropertyVector3);
            double min = -65535, max = 65535, step = default_float_step;
            bool hide_slider = true;

            if (p_hint == PropertyHint::Range && StringUtils::get_slice_count(p_hint_text,',') >= 2) {
                min = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 0));
                max = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 1));
                if (StringUtils::get_slice_count(p_hint_text,',') >= 3) {
                    step = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 2));
                }
                hide_slider = false;
            }

            editor->setup(min, max, step, hide_slider);
            add_property_editor(p_path, editor);

        } break;
        case VariantType::TRANSFORM2D: {
            EditorPropertyTransform2D *editor = memnew(EditorPropertyTransform2D);
            double min = -65535, max = 65535, step = default_float_step;
            bool hide_slider = true;

            if (p_hint == PropertyHint::Range && StringUtils::get_slice_count(p_hint_text,',') >= 2) {
                min = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 0));
                max = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 1));
                if (StringUtils::get_slice_count(p_hint_text,',') >= 3) {
                    step = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 2));
                }
                hide_slider = false;
            }

            editor->setup(min, max, step, hide_slider);
            add_property_editor(p_path, editor);

        } break;
        case VariantType::PLANE: {
            EditorPropertyPlane *editor = memnew(EditorPropertyPlane);
            double min = -65535, max = 65535, step = default_float_step;
            bool hide_slider = true;

            if (p_hint == PropertyHint::Range && StringUtils::get_slice_count(p_hint_text,',') >= 2) {
                min = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 0));
                max = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 1));
                if (StringUtils::get_slice_count(p_hint_text,',') >= 3) {
                    step = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 2));
                }
                hide_slider = false;
            }

            editor->setup(min, max, step, hide_slider);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::QUAT: {
            EditorPropertyQuat *editor = memnew(EditorPropertyQuat);
            double min = -65535, max = 65535, step = default_float_step;
            bool hide_slider = true;

            if (p_hint == PropertyHint::Range && StringUtils::get_slice_count(p_hint_text,',') >= 2) {
                min = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 0));
                max = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 1));
                if (StringUtils::get_slice_count(p_hint_text,',') >= 3) {
                    step = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 2));
                }
                hide_slider = false;
            }

            editor->setup(min, max, step, hide_slider);
            add_property_editor(p_path, editor);
        } break; // 10
        case VariantType::AABB: {
            EditorPropertyAABB *editor = memnew(EditorPropertyAABB);
            double min = -65535, max = 65535, step = default_float_step;
            bool hide_slider = true;

            if (p_hint == PropertyHint::Range && StringUtils::get_slice_count(p_hint_text,',') >= 2) {
                min = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 0));
                max = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 1));
                if (StringUtils::get_slice_count(p_hint_text,',') >= 3) {
                    step = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 2));
                }
                hide_slider = false;
            }

            editor->setup(min, max, step, hide_slider);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::BASIS: {
            EditorPropertyBasis *editor = memnew(EditorPropertyBasis);
            double min = -65535, max = 65535, step = default_float_step;
            bool hide_slider = true;

            if (p_hint == PropertyHint::Range && StringUtils::get_slice_count(p_hint_text,',') >= 2) {
                min = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 0));
                max = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 1));
                if (StringUtils::get_slice_count(p_hint_text,',') >= 3) {
                    step = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 2));
                }
                hide_slider = false;
            }

            editor->setup(min, max, step, hide_slider);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::TRANSFORM: {
            EditorPropertyTransform *editor = memnew(EditorPropertyTransform);
            double min = -65535, max = 65535, step = default_float_step;
            bool hide_slider = true;

            if (p_hint == PropertyHint::Range && StringUtils::get_slice_count(p_hint_text,',') >= 2) {
                min = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 0));
                max = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 1));
                if (StringUtils::get_slice_count(p_hint_text,',') >= 3) {
                    step = StringUtils::to_double(StringUtils::get_slice(p_hint_text,',', 2));
                }
                hide_slider = false;
            }

            editor->setup(min, max, step, hide_slider);
            add_property_editor(p_path, editor);

        } break;

        // misc types
        case VariantType::COLOR: {
            EditorPropertyColor *editor = memnew(EditorPropertyColor);
            editor->setup(p_hint != PropertyHint::ColorNoAlpha);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::NODE_PATH: {

            EditorPropertyNodePath *editor = memnew(EditorPropertyNodePath);
            if (p_hint == PropertyHint::NodePathToEditedNode && !p_hint_text.empty()) {
                editor->setup((NodePath)p_hint_text, {}, p_usage & PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT);
            }
            if (p_hint == PropertyHint::NodePathValidTypes && !p_hint_text.empty()) {
                FixedVector<StringView,8,true> parts;
                String::split_ref(parts,p_hint_text,',');
                Vector<StringName> sn;
                sn.reserve(parts.size());

                for(StringView s : parts)
                    sn.emplace_back(s);
                editor->setup(NodePath(), eastl::move(sn), p_usage & PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT);
            }
            add_property_editor(p_path, editor);

        } break; // 15
        case VariantType::_RID: {
            EditorPropertyRID *editor = memnew(EditorPropertyRID);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::OBJECT: {
            EditorPropertyResource *editor = memnew(EditorPropertyResource);
            editor->setup(p_object, p_path, p_hint == PropertyHint::ResourceType ? StringName(p_hint_text) : StringName("Resource"));

            if (p_hint == PropertyHint::ResourceType) {
                String open_in_new = EDITOR_GET_T<String>("interface/inspector/resources_to_open_in_new_inspector");
                Vector<StringView> allowed_types;
                String::split_ref(allowed_types,open_in_new,",");
                for (StringView entry : allowed_types) {
                    StringName type(StringUtils::strip_edges(entry));
                    for (int j = 0; j < StringUtils::get_slice_count(p_hint_text,','); j++) {
                        StringView inherits = StringUtils::get_slice(p_hint_text,',', j);
                        if (ClassDB::is_parent_class(StringName(inherits), type)) {

                            editor->set_use_sub_inspector(false);
                        }
                    }
                }
            }

            add_property_editor(p_path, editor);

        } break;
        case VariantType::DICTIONARY: {
            EditorPropertyDictionary *editor = memnew(EditorPropertyDictionary);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::ARRAY: {
            EditorPropertyArray *editor = memnew(EditorPropertyArray);
            editor->setup(VariantType::ARRAY, p_hint_text);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::POOL_BYTE_ARRAY: {
            EditorPropertyArray *editor = memnew(EditorPropertyArray);
            editor->setup(VariantType::POOL_BYTE_ARRAY);
            add_property_editor(p_path, editor);
        } break; // 20
        case VariantType::POOL_INT_ARRAY: {
            EditorPropertyArray *editor = memnew(EditorPropertyArray);
            editor->setup(VariantType::POOL_INT_ARRAY);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::POOL_FLOAT32_ARRAY: {
            EditorPropertyArray *editor = memnew(EditorPropertyArray);
            editor->setup(VariantType::POOL_FLOAT32_ARRAY);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::POOL_STRING_ARRAY: {
            EditorPropertyArray *editor = memnew(EditorPropertyArray);
            editor->setup(VariantType::POOL_STRING_ARRAY);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {
            EditorPropertyArray *editor = memnew(EditorPropertyArray);
            editor->setup(VariantType::POOL_VECTOR2_ARRAY);
            add_property_editor(p_path, editor);
        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {
            EditorPropertyArray *editor = memnew(EditorPropertyArray);
            editor->setup(VariantType::POOL_VECTOR3_ARRAY);
            add_property_editor(p_path, editor);
        } break; // 25
        case VariantType::POOL_COLOR_ARRAY: {
            EditorPropertyArray *editor = memnew(EditorPropertyArray);
            editor->setup(VariantType::POOL_COLOR_ARRAY);
            add_property_editor(p_path, editor);
        } break;
        default: {
        }
    }

    return false; //can be overridden, although it will most likely be last anyway
}

void EditorInspectorDefaultPlugin::parse_end() {
    //do none
}

void register_editor_properties_classes()
{
    EditorPropertyNil::initialize_class();
    EditorPropertyText::initialize_class();
    EditorPropertyMultilineText::initialize_class();
    EditorPropertyTextEnum::initialize_class();
    EditorPropertyPath::initialize_class();
    EditorPropertyClassName::initialize_class();
    EditorPropertyMember::initialize_class();
    EditorPropertyCheck::initialize_class();
    EditorPropertyEnum::initialize_class();
    EditorPropertyFlags::initialize_class();
    EditorPropertyLayers::initialize_class();
    EditorPropertyInteger::initialize_class();
    EditorPropertyObjectID::initialize_class();
    EditorPropertyFloat::initialize_class();
    EditorPropertyEasing::initialize_class();
    EditorPropertyVector2::initialize_class();
    EditorPropertyRect2::initialize_class();
    EditorPropertyVector3::initialize_class();
    EditorPropertyPlane::initialize_class();
    EditorPropertyQuat::initialize_class();
    EditorPropertyAABB::initialize_class();
    EditorPropertyTransform2D::initialize_class();
    EditorPropertyBasis::initialize_class();
    EditorPropertyTransform::initialize_class();
    EditorPropertyColor::initialize_class();
    EditorPropertyNodePath::initialize_class();
    EditorPropertyRID::initialize_class();
    EditorPropertyResource::initialize_class();
    EditorPropertyArrayObject::initialize_class();
    EditorPropertyDictionaryObject::initialize_class();
    EditorPropertyArray::initialize_class();
    EditorPropertyDictionary::initialize_class();

    EditorInspectorDefaultPlugin::initialize_class();

    EditorPropertyLayersGrid::initialize_class();
}
