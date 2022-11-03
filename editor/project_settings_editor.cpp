/*************************************************************************/
/*  project_settings_editor.cpp                                          */
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

#include "project_settings_editor.h"

#include "editor/editor_settings.h"
#include "editor_export.h"

#include "core/method_bind.h"
#include "core/global_constants.h"
#include "core/input/input_map.h"
#include "core/callable_method_pointer.h"
#include "core/os/keyboard.h"
#include "core/string_formatter.h"
#include "core/project_settings.h"
#include "core/string_utils.inl"
#include "core/resource/resource_manager.h"
#include "core/translation.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/editor_locale_dialog.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/tab_container.h"
#include "scene/resources/style_box.h"
#include "EASTL/sort.h"


IMPL_GDCLASS(ProjectSettingsEditor)

ProjectSettingsEditor *ProjectSettingsEditor::singleton = nullptr;

static const char *_button_names[JOY_BUTTON_MAX] = {
    "DualShock Cross, Xbox A, Nintendo B",
    "DualShock Circle, Xbox B, Nintendo A",
    "DualShock Square, Xbox X, Nintendo Y",
    "DualShock Triangle, Xbox Y, Nintendo X",
    "L, L1",
    "R, R1",
    "L2",
    "R2",
    "L3",
    "R3",
    "Select, DualShock Share, Nintendo -",
    "Start, DualShock Options, Nintendo +",
    "D-Pad Up",
    "D-Pad Down",
    "D-Pad Left",
    "D-Pad Right",
    "Home, DualShock PS, Guide",
    "Xbox Share, PS5 Microphone, Nintendo Capture",
    "Xbox Paddle 1",
    "Xbox Paddle 2",
    "Xbox Paddle 3",
    "Xbox Paddle 4",
    "PS4/5 Touchpad",
};

static const char *_axis_names[JOY_AXIS_MAX * 2] = {
    " (Left Stick Left)",
    " (Left Stick Right)",
    " (Left Stick Up)",
    " (Left Stick Down)",
    " (Right Stick Left)",
    " (Right Stick Right)",
    " (Right Stick Up)",
    " (Right Stick Down)",
    "", "", "", "",
    "", " (L2)",
    "", " (R2)"
};

void ProjectSettingsEditor::_unhandled_input(const Ref<InputEvent> &p_event) {

    const Ref<InputEventKey> k(dynamic_ref_cast<InputEventKey>(p_event));

    if (k && is_window_modal_on_top() && k->is_pressed()) {

        if (k->get_keycode_with_modifiers() == (KEY_MASK_CMD | KEY_F)) {
            if (search_button->is_pressed()) {
                search_box->grab_focus();
                search_box->select_all();
            } else {
                // This toggles the search bar display while giving the button its "pressed" appearance
                search_button->set_pressed(true);
            }

            accept_event();
        }
    }
}

void ProjectSettingsEditor::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            globals_editor->edit(ProjectSettings::get_singleton());

            search_button->set_button_icon(get_theme_icon("Search", "EditorIcons"));
            search_box->set_right_icon(get_theme_icon("Search", "EditorIcons"));
            search_box->set_clear_button_enabled(true);

            action_add_error->add_theme_color_override("font_color", get_theme_color("error_color", "Editor"));

            translation_list->connect("button_pressed", callable_mp(this, &ProjectSettingsEditor::_translation_delete));
            _update_actions();
            // List Physical Key before Key to encourage its use.
            // Physical Key should be used for most game inputs as it allows keys to work
            // on non-QWERTY layouts out of the box.
            // This is especially important for WASD movement layouts.
            popup_add->add_icon_item(get_theme_icon("KeyboardPhysical", "EditorIcons"), TTR("Physical Key"), INPUT_KEY_PHYSICAL);
            popup_add->add_icon_item(get_theme_icon("Keyboard", "EditorIcons"), TTR("Key "), INPUT_KEY); //"Key " - because the word 'key' has already been used as a key animation
            popup_add->add_icon_item(get_theme_icon("JoyButton", "EditorIcons"), TTR("Joy Button"), INPUT_JOY_BUTTON);
            popup_add->add_icon_item(get_theme_icon("JoyAxis", "EditorIcons"), TTR("Joy Axis"), INPUT_JOY_MOTION);
            popup_add->add_icon_item(get_theme_icon("Mouse", "EditorIcons"), TTR("Mouse Button"), INPUT_MOUSE_BUTTON);

            Vector<String> tfn;
            gResourceManager().get_recognized_extensions_for_type("Translation", tfn);
            for (const String &E : tfn) {

                translation_file_open->add_filter("*." + E);
            }

            Vector<String> rfn;
            gResourceManager().get_recognized_extensions_for_type("Resource", rfn);
            for (const String &E : rfn) {
                translation_res_file_open->add_filter("*." + E);
                translation_res_option_file_open->add_filter("*." + E);
            }

            restart_close_button->set_button_icon(get_theme_icon("Close", "EditorIcons"));
            restart_container->add_theme_style_override("panel", get_theme_stylebox("bg", "Tree"));
            restart_icon->set_texture(get_theme_icon("StatusWarning", "EditorIcons"));
            restart_label->add_theme_color_override("font_color", get_theme_color("warning_color", "Editor"));

            // The ImportDefaultsEditor changes settings which must be read by this object when changed
            ProjectSettings::get_singleton()->connect("project_settings_changed", callable_mp(this, &ProjectSettingsEditor::_settings_changed));

        } break;
        case NOTIFICATION_EXIT_TREE: {
            if (ProjectSettings::get_singleton()) {
                ProjectSettings::get_singleton()->disconnect("project_settings_changed", callable_mp(this, &ProjectSettingsEditor::_settings_changed));
            }
        } break;
        case NOTIFICATION_POPUP_HIDE: {
            EditorSettings::get_singleton()->set_project_metadata("dialog_bounds", "project_settings", get_rect());
            set_process_unhandled_input(false);
        } break;
        case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
            search_button->set_button_icon(get_theme_icon("Search", "EditorIcons"));
            search_box->set_right_icon(get_theme_icon("Search", "EditorIcons"));
            search_box->set_clear_button_enabled(true);
            action_add_error->add_theme_color_override("font_color", get_theme_color("error_color", "Editor"));
            popup_add->set_item_icon(popup_add->get_item_index(INPUT_KEY_PHYSICAL), get_theme_icon("KeyboardPhysical", "EditorIcons"));
            popup_add->set_item_icon(popup_add->get_item_index(INPUT_KEY), get_theme_icon("Keyboard", "EditorIcons"));
            popup_add->set_item_icon(popup_add->get_item_index(INPUT_JOY_BUTTON), get_theme_icon("JoyButton", "EditorIcons"));
            popup_add->set_item_icon(popup_add->get_item_index(INPUT_JOY_MOTION), get_theme_icon("JoyAxis", "EditorIcons"));
            popup_add->set_item_icon(popup_add->get_item_index(INPUT_MOUSE_BUTTON), get_theme_icon("Mouse", "EditorIcons"));
            _update_actions();
        } break;
        case NOTIFICATION_THEME_CHANGED: {
            _update_theme();
        } break;
    }
}

static bool _validate_action_name(StringView p_name) {
    for(char c : p_name)
        if (c == '/' || c == ':' || c == '"' ||
                c == '=' || c == '\\' || c < 32)
            return false;
    return true;
}

void ProjectSettingsEditor::_action_selected() {

    TreeItem *ti = input_editor->get_selected();
    if (!ti || !ti->is_editable(0))
        return;

    add_at = StringName("input/" + ti->get_text(0));
    edit_idx = -1;
}

void ProjectSettingsEditor::_action_edited() {

    TreeItem *ti = input_editor->get_selected();
    if (!ti)
        return;

    if (input_editor->get_selected_column() == 0) {

        String new_name = ti->get_text(0);
        String old_name(StringUtils::substr(add_at,StringUtils::find(add_at,"/") + 1));

        if (new_name == old_name)
            return;

        if (new_name.empty() || !_validate_action_name(new_name)) {

            ti->set_text_utf8(0, old_name);
            add_at = StringName("input/" + old_name);

            message->set_text(TTR("Invalid action name. It cannot be empty nor contain '/', ':', '=', '\\' or '\"'"));
            message->popup_centered(Size2(300, 100) * EDSCALE);
            return;
        }

        StringName action_prop("input/" + new_name);

        if (ProjectSettings::get_singleton()->has_setting(action_prop)) {

            ti->set_text_utf8(0, old_name);
            add_at = StringName("input/" + old_name);

            message->set_text(FormatSN(TTR("An action with the name '%s' already exists.").asCString(), new_name.c_str()));
            message->popup_centered(Size2(300, 100) * EDSCALE);
            return;
        }

        int order = ProjectSettings::get_singleton()->get_order(add_at);
        Dictionary action = ProjectSettings::get_singleton()->get(add_at).as<Dictionary>();

        setting = true;
        undo_redo->create_action(TTR("Rename Input Action Event"));
        undo_redo->add_do_method(ProjectSettings::get_singleton(), "clear", add_at);
        undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", action_prop, action);
        undo_redo->add_do_method(ProjectSettings::get_singleton(), "set_order", action_prop, order);
        undo_redo->add_undo_method(ProjectSettings::get_singleton(), "clear", action_prop);
        undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", add_at, action);
        undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", add_at, order);
        undo_redo->add_do_method(this, "_update_actions");
        undo_redo->add_undo_method(this, "_update_actions");
        undo_redo->add_do_method(this, "_settings_changed");
        undo_redo->add_undo_method(this, "_settings_changed");
        undo_redo->commit_action();
        setting = false;

        add_at = action_prop;
    } else if (input_editor->get_selected_column() == 1) {

        StringName name("input/" + ti->get_text(0));
        Dictionary old_action = ProjectSettings::get_singleton()->get(name).as<Dictionary>();
        Dictionary new_action = old_action.duplicate();
        new_action["deadzone"] = ti->get_range(1);

        undo_redo->create_action(TTR("Change Action deadzone"));
        undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", name, new_action);
        undo_redo->add_do_method(this, "_settings_changed");
        undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", name, old_action);
        undo_redo->add_undo_method(this, "_settings_changed");
        undo_redo->commit_action();
    }
}

void ProjectSettingsEditor::_device_input_add() {

    Ref<InputEvent> ie;
    StringName name(add_at);
    int idx = edit_idx;
    Dictionary old_val = ProjectSettings::get_singleton()->get(name).as<Dictionary>();
    Dictionary action = old_val.duplicate();
    Array events = action["events"].as<Array>();

    switch (add_type) {

        case INPUT_MOUSE_BUTTON: {

            Ref<InputEventMouseButton> mb(make_ref_counted<InputEventMouseButton>());
            mb->set_button_index(device_index->get_selected() + 1);
            mb->set_device(_get_current_device());

            for (int i = 0; i < events.size(); i++) {

                Ref<InputEventMouseButton> aie(events[i]);
                if (not aie)
                    continue;
                if (aie->get_device() == mb->get_device() && aie->get_button_index() == mb->get_button_index()) {
                    return;
                }
            }

            ie = mb;

        } break;
        case INPUT_JOY_MOTION: {

            Ref<InputEventJoypadMotion> jm(make_ref_counted<InputEventJoypadMotion>());
            jm->set_axis(device_index->get_selected() >> 1);
            jm->set_axis_value(device_index->get_selected() & 1 ? 1 : -1);
            jm->set_device(_get_current_device());

            for (int i = 0; i < events.size(); i++) {

                Ref<InputEventJoypadMotion> aie(events[i]);
                if (not aie)
                    continue;

                if (aie->get_device() == jm->get_device() && aie->get_axis() == jm->get_axis() && aie->get_axis_value() == jm->get_axis_value()) {
                    return;
                }
            }

            ie = jm;

        } break;
        case INPUT_JOY_BUTTON: {

            Ref<InputEventJoypadButton> jb(make_ref_counted<InputEventJoypadButton>());

            jb->set_button_index(device_index->get_selected());
            jb->set_device(_get_current_device());

            for (int i = 0; i < events.size(); i++) {

                Ref<InputEventJoypadButton> aie(events[i]);
                if (not aie)
                    continue;
                if (aie->get_device() == jb->get_device() && aie->get_button_index() == jb->get_button_index()) {
                    return;
                }
            }
            ie = jb;

        } break;
        default: {
        }
    }

    if (idx < 0 || idx >= events.size()) {
        events.push_back(ie);
    } else {
        events[idx] = ie;
    }
    action["events"] = events;

    undo_redo->create_action(TTR("Add Input Action Event"));
    undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", name, action);
    undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", name, old_val);
    undo_redo->add_do_method(this, "_update_actions");
    undo_redo->add_undo_method(this, "_update_actions");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();

    _show_last_added(ie, name);
}

void ProjectSettingsEditor::_set_current_device(int i_device) {
    device_id->select(i_device + 1);
}

int ProjectSettingsEditor::_get_current_device() {
    return device_id->get_selected() - 1;
}

StringName ProjectSettingsEditor::_get_device_string(int i_device) {
    if (i_device == InputMap::ALL_DEVICES)
        return TTR("All Devices");
    return FormatSN(TTR("Device %d").asCString(),i_device);
}

void ProjectSettingsEditor::_press_a_key_confirm() {

    if (not last_wait_for_key){
        return;
    }

    Ref<InputEventKey> ie(make_ref_counted<InputEventKey>());
    if (press_a_key_physical) {
        ie->set_physical_scancode(last_wait_for_key->get_physical_scancode());
        ie->set_keycode(0);
    } else {
        ie->set_physical_scancode(0);
    ie->set_keycode(last_wait_for_key->get_keycode());
    }
    ie->set_shift(last_wait_for_key->get_shift());
    ie->set_alt(last_wait_for_key->get_alt());
    ie->set_control(last_wait_for_key->get_control());
    ie->set_metakey(last_wait_for_key->get_metakey());

    StringName name = add_at;
    int idx = edit_idx;

    Dictionary old_val = ProjectSettings::get_singleton()->get(name).as<Dictionary>();
    Dictionary action = old_val.duplicate();
    Array events = action["events"].as<Array>();

    for (int i = 0; i < events.size(); i++) {

        Ref<InputEventKey> aie(events[i]);
        if (not aie)
            continue;
        if (!press_a_key_physical) {
        if (aie->get_keycode_with_modifiers() == ie->get_keycode_with_modifiers()) {
            return;
        }
        } else {
            if (aie->get_physical_scancode_with_modifiers() == ie->get_physical_scancode_with_modifiers()) {
                return;
            }
        }
    }

    if (idx < 0 || idx >= events.size()) {
        events.push_back(ie);
    } else {
        events[idx] = ie;
    }
    action["events"] = events;

    undo_redo->create_action(TTR("Add Input Action Event"));
    undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", name, action);
    undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", name, old_val);
    undo_redo->add_do_method(this, "_update_actions");
    undo_redo->add_undo_method(this, "_update_actions");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();

    _show_last_added(ie, name);
}

void ProjectSettingsEditor::_show_last_added(const Ref<InputEvent> &p_event, StringView p_name) {
    TreeItem *r = input_editor->get_root();

    String name(p_name);
    StringUtils::erase(name,0, 6);
    if (!r)
        return;
    r = r->get_children();
    if (!r)
        return;
    bool found = false;
    while (r) {
        if (r->get_text(0) != name) {
            r = r->get_next();
            continue;
        }
        TreeItem *child = r->get_children();
        while (child) {
            Variant input = child->get_meta("__input");
            if (p_event == refFromVariant<InputEvent>(input)) {
                r->set_collapsed(false);
                child->select(0);
                found = true;
                break;
            }
            child = child->get_next();
        }
        if (found) {
            break;
        }
        r = r->get_next();
    }

    if (found) {
        input_editor->ensure_cursor_is_visible();
    }
}

void ProjectSettingsEditor::_wait_for_key(const Ref<InputEvent> &p_event) {

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_event);

    if (k && k->is_pressed() && k->get_keycode() != 0) {
        last_wait_for_key = dynamic_ref_cast<InputEventKey>(p_event);
        const String str = (press_a_key_physical) ?
                                   keycode_get_string(k->get_physical_scancode_with_modifiers()) + TTR(" (Physical)") :
                                   keycode_get_string(k->get_keycode_with_modifiers());

        press_a_key_label->set_text(StringName(str));
        press_a_key->get_ok()->set_disabled(false);
        press_a_key->accept_event();
    }
}

void ProjectSettingsEditor::_add_item(int p_item, const Ref<InputEvent>& p_exiting_event) {

    add_type = InputType(p_item);

    switch (add_type) {

        case INPUT_KEY_PHYSICAL: {
            press_a_key_physical = true;
            press_a_key_label->set_text(TTR("Press a Key..."));
            press_a_key->get_ok()->set_disabled(true);
            last_wait_for_key = {};
            press_a_key->popup_centered(Size2(250, 80) * EDSCALE);
            press_a_key->grab_focus();
        } break;
        case INPUT_KEY: {

            press_a_key_physical = false;
            press_a_key_label->set_text(TTR("Press a Key..."));
            press_a_key->get_ok()->set_disabled(true);
            last_wait_for_key = Ref<InputEventKey>();
            press_a_key->popup_centered(Size2(250, 80) * EDSCALE);
            press_a_key->grab_focus();

        } break;
        case INPUT_MOUSE_BUTTON: {

            device_index_label->set_text(TTR("Mouse Button Index:"));
            device_index->clear();
            device_index->add_item(TTR("Left Button"));
            device_index->add_item(TTR("Right Button"));
            device_index->add_item(TTR("Middle Button"));
            device_index->add_item(TTR("Wheel Up Button"));
            device_index->add_item(TTR("Wheel Down Button"));
            device_index->add_item(TTR("Wheel Left Button"));
            device_index->add_item(TTR("Wheel Right Button"));
            device_index->add_item(TTR("X Button 1"));
            device_index->add_item(TTR("X Button 2"));
            device_input->popup_centered_minsize(Size2(350, 95) * EDSCALE);

            Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_exiting_event);
            if (mb) {
                device_index->select(mb->get_button_index() - 1);
                _set_current_device(mb->get_device());
                device_input->get_ok()->set_text(TTR("Change"));
            } else {
                _set_current_device(0);
                device_input->get_ok()->set_text(TTR("Add"));
            }

        } break;
        case INPUT_JOY_MOTION: {

            device_index_label->set_text(TTR("Joypad Axis Index:"));
            device_index->clear();
            for (int i = 0; i < JOY_AXIS_MAX * 2; i++) {

                String desc(_axis_names[i]);
                device_index->add_item(TTR("Axis") + " " + itos(i / 2) + " " + (i & 1 ? "+" : "-") + desc);
            }
            device_input->popup_centered_minsize(Size2(350, 95) * EDSCALE);

            Ref<InputEventJoypadMotion> jm = dynamic_ref_cast<InputEventJoypadMotion>(p_exiting_event);
            if (jm) {
                device_index->select(jm->get_axis() * 2 + (jm->get_axis_value() > 0 ? 1 : 0));
                _set_current_device(jm->get_device());
                device_input->get_ok()->set_text(TTR("Change"));
            } else {
                _set_current_device(0);
                device_input->get_ok()->set_text(TTR("Add"));
            }

        } break;
        case INPUT_JOY_BUTTON: {

            device_index_label->set_text(TTR("Joypad Button Index:"));
            device_index->clear();

            for (int i = 0; i < JOY_BUTTON_MAX; i++) {

                device_index->add_item(StringName(itos(i) + ": " + _button_names[i]));
            }
            device_input->popup_centered_minsize(Size2(350, 95) * EDSCALE);

            Ref<InputEventJoypadButton> jb = dynamic_ref_cast<InputEventJoypadButton>(p_exiting_event);
            if (jb) {
                device_index->select(jb->get_button_index());
                _set_current_device(jb->get_device());
                device_input->get_ok()->set_text(TTR("Change"));
            } else {
                _set_current_device(0);
                device_input->get_ok()->set_text(TTR("Add"));
            }

        } break;
        default: {
        }
    }
}

void ProjectSettingsEditor::_edit_item(const Ref<InputEvent>& p_exiting_event) {

    InputType ie_type;

    if (auto iek = dynamic_ref_cast<InputEventKey>(p_exiting_event)) {
        if (iek->get_keycode() != 0) {
        ie_type = INPUT_KEY;

        } else {
            ie_type = INPUT_KEY_PHYSICAL;
        }
    } else if (dynamic_ref_cast<InputEventJoypadButton>(p_exiting_event)) {
        ie_type = INPUT_JOY_BUTTON;

    } else if (dynamic_ref_cast<InputEventMouseButton>(p_exiting_event)) {
        ie_type = INPUT_MOUSE_BUTTON;

    } else if (dynamic_ref_cast<InputEventJoypadMotion>(p_exiting_event)) {
        ie_type = INPUT_JOY_MOTION;

    } else {
        return;
    }

    _add_item(ie_type, p_exiting_event);
}
void ProjectSettingsEditor::_action_activated() {

    TreeItem *ti = input_editor->get_selected();

    if (!ti || ti->get_parent() == input_editor->get_root())
        return;

    StringName name("input/" + ti->get_parent()->get_text(0));
    int idx = ti->get_metadata(0).as<int>();
    Dictionary action = ProjectSettings::get_singleton()->get(name).as<Dictionary>();
    Array events = action["events"].as<Array>();

    ERR_FAIL_INDEX(idx, events.size());
    Ref<InputEvent> event(events[idx]);
    if (not event)
        return;

    add_at = name;
    edit_idx = idx;
    _edit_item(event);
}

void ProjectSettingsEditor::_action_button_pressed(Object *p_obj, int p_column, int p_id) {

    TreeItem *ti = object_cast<TreeItem>(p_obj);

    ERR_FAIL_COND(!ti);

    if (p_id == 1) {
        // Add action event
        Point2 ofs = input_editor->get_global_position();
        Rect2 ir = input_editor->get_item_rect(ti);
        ir.position.y -= input_editor->get_scroll().y;
        ofs += ir.position + ir.size;
        ofs.x -= 100;
        popup_add->set_position(ofs);
        popup_add->popup();
        add_at = StringName("input/" + ti->get_text(0));
        edit_idx = -1;

    } else if (p_id == 2) {
        // Remove

        if (ti->get_parent() == input_editor->get_root()) {
            // Remove action
            StringName name("input/" + ti->get_text(0));
            Dictionary old_val = ProjectSettings::get_singleton()->get(name).as<Dictionary>();
            int order = ProjectSettings::get_singleton()->get_order(name);

            undo_redo->create_action(TTR("Erase Input Action"));
            undo_redo->add_do_method(ProjectSettings::get_singleton(), "clear", name);
            undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", name, old_val);
            undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", name, order);
            undo_redo->add_do_method(this, "_update_actions");
            undo_redo->add_undo_method(this, "_update_actions");
            undo_redo->add_do_method(this, "_settings_changed");
            undo_redo->add_undo_method(this, "_settings_changed");
            undo_redo->commit_action();

        } else {
            // Remove action event
            StringName name("input/" + ti->get_parent()->get_text(0));
            Dictionary old_val = ProjectSettings::get_singleton()->get(name).as<Dictionary>();
            Dictionary action = old_val.duplicate();
            int idx = ti->get_metadata(0).as<int>();

            Array events = action["events"].as<Array>();
            ERR_FAIL_INDEX(idx, events.size());
            events.remove(idx);
            action["events"] = events;

            undo_redo->create_action(TTR("Erase Input Action Event"));
            undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", name, action);
            undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", name, old_val);
            undo_redo->add_do_method(this, "_update_actions");
            undo_redo->add_undo_method(this, "_update_actions");
            undo_redo->add_do_method(this, "_settings_changed");
            undo_redo->add_undo_method(this, "_settings_changed");
            undo_redo->commit_action();
        }
    } else if (p_id == 3) {
        // Edit

        if (ti->get_parent() == input_editor->get_root()) {
            // Edit action name
            ti->set_as_cursor(0);
            input_editor->edit_selected();

        } else {
            // Edit action event
            StringName name("input/" + ti->get_parent()->get_text(0));
            int idx = ti->get_metadata(0).as<int>();
            Dictionary action = ProjectSettings::get_singleton()->get(name).as<Dictionary>();

            Array events = action["events"].as<Array>();
            ERR_FAIL_INDEX(idx, events.size());

            Ref<InputEvent> event(events[idx]);

            if (not event)
                return;

            ti->set_as_cursor(0);
            add_at = name;
            edit_idx = idx;
            _edit_item(event);
        }
    }
}

void ProjectSettingsEditor::_update_actions() {

    if (setting)
        return;

    Map<String, bool> collapsed;

    if (input_editor->get_root() && input_editor->get_root()->get_children()) {
        for (TreeItem *item = input_editor->get_root()->get_children(); item; item = item->get_next()) {
            collapsed[item->get_text(0)] = item->is_collapsed();
        }
    }

    input_editor->clear();
    TreeItem *root = input_editor->create_item();
    input_editor->set_hide_root(true);

    Vector<PropertyInfo> props;
    ProjectSettings::get_singleton()->get_property_list(&props);

    for (const PropertyInfo &pi : props) {

        if (!StringUtils::begins_with(pi.name,"input/"))
            continue;

        StringName name( StringUtils::get_slice(pi.name,"/", 1));
        if (name.empty())
            continue;

        Dictionary action = ProjectSettings::get_singleton()->get(pi.name).as<Dictionary>();
        Array events = action["events"].as<Array>();

        TreeItem *item = input_editor->create_item(root);
        item->set_text_utf8(0, name);
        item->set_custom_bg_color(0, get_theme_color("prop_subsection", "Editor"));
        auto iter = collapsed.find_as(name);
        if (iter!=collapsed.end())
            item->set_collapsed(iter->second);

        item->set_editable(1, true);
        item->set_cell_mode(1, TreeItem::CELL_MODE_RANGE);
        item->set_range_config(1, 0.0, 1.0, 0.01);
        item->set_range(1, action["deadzone"].as<float>());
        item->set_custom_bg_color(1, get_theme_color("prop_subsection", "Editor"));

        item->add_button(2, get_theme_icon("Add", "EditorIcons"), 1, false, TTR("Add Event"));
        const Vector<String> &presets(ProjectSettings::get_singleton()->get_input_presets());
        bool has_pi = eastl::find(presets.begin(),presets.end(),pi.name)!=presets.end();
        if (!has_pi) {
            item->add_button(2, get_theme_icon("Remove", "EditorIcons"), 2, false, TTR("Remove"));
            item->set_editable(0, true);
        }

        for (int i = 0; i < events.size(); i++) {

            Ref<InputEvent> event(events[i]);
            if (not event) {
                continue;
            }

            TreeItem *action2 = input_editor->create_item(item);

            Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(event);
            if (k) {
                const String str = (k->get_keycode() == 0) ? keycode_get_string(k->get_physical_scancode_with_modifiers()) + TTR(" (Physical)") : keycode_get_string(k->get_keycode_with_modifiers());
                action2->set_text_utf8(0, str);
                if ((k->get_keycode() != 0)) {
                action2->set_icon(0, get_theme_icon("Keyboard", "EditorIcons"));
                } else {
                    action2->set_icon(0, get_theme_icon("KeyboardPhysical", "EditorIcons"));
            }

            }
            Ref<InputEventJoypadButton> jb = dynamic_ref_cast<InputEventJoypadButton>(event);

            if (jb) {

                String str(_get_device_string(jb->get_device()) + ", " + TTR("Button") + " " + StringUtils::num(jb->get_button_index()));
                if (jb->get_button_index() >= 0 && jb->get_button_index() < JOY_BUTTON_MAX) {
                    str += String(" (") + _button_names[jb->get_button_index()] + ").";
                }
                else {
                    str += '.';
                }

                action2->set_text(0, StringName(str));
                action2->set_icon(0, get_theme_icon("JoyButton", "EditorIcons"));
            }

            Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(event);

            if (mb) {
                String str(_get_device_string(mb->get_device()) + ", ");
                switch (mb->get_button_index()) {
                    case BUTTON_LEFT:
                        str += TTR("Left Button.");
                        break;
                    case BUTTON_RIGHT:
                        str += TTR("Right Button.");
                        break;
                    case BUTTON_MIDDLE:
                        str += TTR("Middle Button.");
                        break;
                    case BUTTON_WHEEL_UP:
                        str += TTR("Wheel Up.");
                        break;
                    case BUTTON_WHEEL_DOWN:
                        str += TTR("Wheel Down.");
                        break;
                    default:
                        str += TTR("Button") + " " + itos(mb->get_button_index()) + ".";
                }

                action2->set_text(0, StringName(str));
                action2->set_icon(0, get_theme_icon("Mouse", "EditorIcons"));
            }

            Ref<InputEventJoypadMotion> jm = dynamic_ref_cast<InputEventJoypadMotion>(event);

            if (jm) {

                int ax = jm->get_axis();
                int n = 2 * ax + (jm->get_axis_value() < 0 ? 0 : 1);
                const char * desc(_axis_names[n]);
                StringName str(_get_device_string(jm->get_device()) + ", " + TTR("Axis") + " " + StringUtils::num(ax) + " " + (jm->get_axis_value() < 0 ? "-" : "+") + desc + ".");
                action2->set_text(0, str);
                action2->set_icon(0, get_theme_icon("JoyAxis", "EditorIcons"));
            }
            action2->set_metadata(0, i);
            action2->set_meta("__input", event);

            action2->add_button(2, get_theme_icon("Edit", "EditorIcons"), 3, false, TTR("Edit"));
            action2->add_button(2, get_theme_icon("Remove", "EditorIcons"), 2, false, TTR("Remove"));
        }
    }

    _action_check(action_name->get_text());
}

void ProjectSettingsEditor::popup_project_settings() {

    // Restore valid window bounds or pop up at default size.
    Rect2 saved_size = EditorSettings::get_singleton()->get_project_metadataT("dialog_bounds", "project_settings", Rect2());
    if (saved_size != Rect2()) {
        popup(saved_size);
    } else {
        popup_centered_clamped(Size2(900, 700) * EDSCALE, 0.8f);
    }

    globals_editor->update_category_list();
    _update_translations();
    autoload_settings->update_autoload();
    plugin_settings->update_plugins();
    import_defaults_editor->clear();
    set_process_unhandled_input(true);
}

void ProjectSettingsEditor::update_plugins() {
    plugin_settings->update_plugins();
}

void ProjectSettingsEditor::_item_selected(StringView p_path) {

    if (p_path.empty())
        return;
    property->set_text(PathUtils::plus_file(globals_editor->get_current_section(),p_path));
    popup_copy_to_feature->set_disabled(false);
}

void ProjectSettingsEditor::_item_adds(StringView) {

    _item_add();
}

void ProjectSettingsEditor::_item_add() {

    // Initialize the property with the default value for the given type.
    const Variant value = Variant::construct_default(VariantType(type_box->get_selected_id()));

    StringName name(StringUtils::strip_edges(property->get_text()));

    if (name.empty()) {
        return;
    }

    if (!StringView(name).contains('/')) {
        name = "global/" + name;
    }


    undo_redo->create_action(TTR("Add Global Property"));

    undo_redo->add_do_property(ProjectSettings::get_singleton(), name, value);

    if (ProjectSettings::get_singleton()->has_setting(name)) {
        undo_redo->add_undo_property(ProjectSettings::get_singleton(), name, ProjectSettings::get_singleton()->get(name));
    } else {
        undo_redo->add_undo_property(ProjectSettings::get_singleton(), name, Variant());
    }

    undo_redo->add_do_method(globals_editor, "update_category_list");
    undo_redo->add_undo_method(globals_editor, "update_category_list");

    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");

    undo_redo->commit_action();

    globals_editor->set_current_section(String(StringUtils::get_slice(name,'/',1)));

    _settings_changed();
}

void ProjectSettingsEditor::_item_del() {

    StringName path = globals_editor->get_inspector()->get_selected_path();
    if (path.empty()) {
        EditorNode::get_singleton()->show_warning(TTR("Select a setting item first!"));
        return;
    }

    StringName property(PathUtils::plus_file(globals_editor->get_current_section(),path));

    if (!ProjectSettings::get_singleton()->has_setting(property)) {
        EditorNode::get_singleton()->show_warning(FormatSN(TTR("No property '%s' exists.").asCString(), property.asCString()));
        return;
    }

    if (ProjectSettings::get_singleton()->get_order(property) < ProjectSettings::NO_BUILTIN_ORDER_BASE) {
        EditorNode::get_singleton()->show_warning(FormatSN(TTR("Setting '%s' is internal, and it can't be deleted.").asCString(), property.asCString()));
        return;
    }

    undo_redo->create_action(TTR("Delete Item"));

    Variant value = ProjectSettings::get_singleton()->get(property);
    int order = ProjectSettings::get_singleton()->get_order(property);

    undo_redo->add_do_method(ProjectSettings::get_singleton(), "clear", property);
    undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", property, value);
    undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", property, order);

    undo_redo->add_do_method(globals_editor, "update_category_list");
    undo_redo->add_undo_method(globals_editor, "update_category_list");

    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");

    undo_redo->commit_action();
}

void ProjectSettingsEditor::_action_check(StringView p_action) {

    if (p_action.empty()) {

        action_add->set_disabled(true);
    } else {

        if (!_validate_action_name(p_action)) {

            action_add_error->set_text(TTR("Invalid action name. It cannot be empty nor contain '/', ':', '=', '\\' or '\"'."));
            action_add_error->show();
            action_add->set_disabled(true);
            return;
        }
        if (ProjectSettings::get_singleton()->has_setting(StringName("input/" + String(p_action)))) {

            action_add_error->set_text(FormatSN(TTR("An action with the name '%.*s' already exists.").asCString(), p_action.length(),p_action.data()));
            action_add_error->show();
            action_add->set_disabled(true);
            return;
        }

        action_add->set_disabled(false);
    }

    action_add_error->hide();
}

void ProjectSettingsEditor::_action_adds(StringView) {

    if (!action_add->is_disabled()) {
        _action_add();
    }
}

void ProjectSettingsEditor::_action_add() {

    Dictionary action;
    action["events"] = Array();
    action["deadzone"] = 0.5f;
    String name = "input/" + action_name->get_text();
    undo_redo->create_action(TTR("Add Input Action"));
    undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", name, action);
    undo_redo->add_undo_method(ProjectSettings::get_singleton(), "clear", name);
    undo_redo->add_do_method(this, "_update_actions");
    undo_redo->add_undo_method(this, "_update_actions");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();

    TreeItem *r = input_editor->get_root();

    if (!r)
        return;
    r = r->get_children();
    if (!r)
        return;
    while (r->get_next())
        r = r->get_next();

    r->select(0);
    input_editor->ensure_cursor_is_visible();
    action_add_error->hide();
    action_name->clear();
}

void ProjectSettingsEditor::_item_checked(StringView p_item, bool p_check) {
}

void ProjectSettingsEditor::_save() {

    Error err = ProjectSettings::get_singleton()->save();
    message->set_text(err != OK ? TTR("Error saving settings.") : TTR("Settings saved OK."));
    message->popup_centered(Size2(300, 100) * EDSCALE);
}

void ProjectSettingsEditor::_settings_prop_edited(StringView p_name) {

    // Method needed to discard the mandatory argument of the property_edited signal
    _settings_changed();
}

void ProjectSettingsEditor::_settings_changed() {

    timer->start();
}

void ProjectSettingsEditor::queue_save() {
    _settings_changed();
}

void ProjectSettingsEditor::_copy_to_platform_about_to_show() {

    HashSet<StringName> presets;

    presets.insert("bptc");
    presets.insert("s3tc");
    presets.insert("debug");
    presets.insert("release");
    presets.insert("editor");
    presets.insert("standalone");
    presets.insert("32");
    presets.insert("64");
    // Not available as an export platform yet, so it needs to be added manually
    presets.insert("Server");

    for (int i = 0; i < EditorExport::get_singleton()->get_export_platform_count(); i++) {
        Vector<String> p;
        EditorExport::get_singleton()->get_export_platform(i)->get_platform_features(&p);
        presets.insert(p.begin(), p.end());
    }

    for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {

        Vector<String> p;
        EditorExport::get_singleton()->get_export_preset(i)->get_platform()->get_preset_features(EditorExport::get_singleton()->get_export_preset(i), &p);
        presets.insert(p.begin(), p.end());

        String custom = EditorExport::get_singleton()->get_export_preset(i)->get_custom_features();
        Vector<StringView> custom_list = StringUtils::split(custom,',');
        for (int j = 0; j < custom_list.size(); j++) {
            String f(StringUtils::strip_edges( custom_list[j]));
            if (!f.empty()) {
                presets.emplace(f);
            }
        }
    }

    popup_copy_to_feature->get_popup()->clear();
    int id = 0;
    for (const StringName &E : presets) {
        popup_copy_to_feature->get_popup()->add_item(E, id++);
    }
}

Variant ProjectSettingsEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {

    TreeItem *selected = input_editor->get_selected();
    if (!selected || selected->get_parent() != input_editor->get_root())
        return Variant();

    StringName name(selected->get_text(0));
    VBoxContainer *vb = memnew(VBoxContainer);
    HBoxContainer *hb = memnew(HBoxContainer);
    Label *label = memnew(Label(name));
    hb->set_modulate(Color(1, 1, 1, 1.0f));
    hb->add_child(label);
    vb->add_child(hb);
    set_drag_preview(vb);

    Dictionary drag_data;
    drag_data["type"] = "input_map";

    input_editor->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);

    return drag_data;
}

bool ProjectSettingsEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {

    Dictionary d = p_data.as<Dictionary>();
    if (!d.has("type") || d["type"] != "input_map") {
        return false;
    }

    TreeItem *selected = input_editor->get_selected();
    TreeItem *item = input_editor->get_item_at_position(p_point);
    if (!selected || !item || item == selected || item->get_parent() == selected)
        return false;

    return true;
}

void ProjectSettingsEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {

    if (!can_drop_data_fw(p_point, p_data, p_from)) {
        return;
    }

    TreeItem *selected = input_editor->get_selected();
    TreeItem *item = input_editor->get_item_at_position(p_point);
    if (!item)
        return;
    TreeItem *target = item->get_parent() == input_editor->get_root() ? item : item->get_parent();

    StringName selected_name("input/" + selected->get_text(0));
    int old_order = ProjectSettings::get_singleton()->get_order(selected_name);
    StringName target_name("input/" + target->get_text(0));
    int target_order = ProjectSettings::get_singleton()->get_order(target_name);

    int order = old_order;
    bool is_below = target_order > old_order;
    TreeItem *iterator = is_below ? selected->get_next() : selected->get_prev();

    undo_redo->create_action(TTR("Moved Input Action Event"));
    while (iterator != target) {

        StringName iterator_name("input/" + iterator->get_text(0));
        int iterator_order = ProjectSettings::get_singleton()->get_order(iterator_name);
        undo_redo->add_do_method(ProjectSettings::get_singleton(), "set_order", iterator_name, order);
        undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", iterator_name, iterator_order);
        order = iterator_order;
        iterator = is_below ? iterator->get_next() : iterator->get_prev();
    }

    undo_redo->add_do_method(ProjectSettings::get_singleton(), "set_order", target_name, order);
    undo_redo->add_do_method(ProjectSettings::get_singleton(), "set_order", selected_name, target_order);
    undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", target_name, target_order);
    undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", selected_name, old_order);

    undo_redo->add_do_method(this, "_update_actions");
    undo_redo->add_undo_method(this, "_update_actions");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();
}
void ProjectSettingsEditor::_copy_to_platform(int p_which) {

    StringName path = globals_editor->get_inspector()->get_selected_path();
    if (path.empty()) {
        EditorNode::get_singleton()->show_warning(TTR("Select a setting item first!"));
        return;
    }

    StringName property(PathUtils::plus_file(globals_editor->get_current_section(),path));

    undo_redo->create_action(TTR("Override for Feature"));

    Variant value = ProjectSettings::get_singleton()->get(property);
    if (StringUtils::contains(property,'.') ) { //overwriting overwrite, keep overwrite
        undo_redo->add_do_method(ProjectSettings::get_singleton(), "clear", property);
        undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", property, value);
    }

    String feature = popup_copy_to_feature->get_popup()->get_item_text(p_which);
    StringName new_path(property + String(".") + feature);

    undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", new_path, value);
    if (ProjectSettings::get_singleton()->has_setting(new_path)) {
        undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", new_path, ProjectSettings::get_singleton()->get(new_path));
    }

    undo_redo->add_do_method(globals_editor, "update_category_list");
    undo_redo->add_undo_method(globals_editor, "update_category_list");

    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");

    undo_redo->commit_action();
}

void ProjectSettingsEditor::add_translation(const UIString &p_translation) {

    Vector<String> translations;
    translations.push_back(StringUtils::to_utf8(p_translation));
    _translation_add(translations);
}

void ProjectSettingsEditor::_translation_add(const Vector<String> &p_paths) {

    PoolVector<String> translations =
            ProjectSettings::get_singleton()->get("locale/translations").as<PoolVector<String>>();

    for (const String &p_path : p_paths) {
        bool duplicate = false;
        for (int j = 0; j < translations.size(); j++) {
            if (translations[j] == p_path) {
                duplicate = true;
                break;
            }
        }

        // Don't add duplicate translation paths.
        if (!duplicate) {
            translations.push_back(p_path);
        }
    }

    undo_redo->create_action(FormatVE(TTR("Add %d Translations").asCString(), p_paths.size()));
    undo_redo->add_do_property(ProjectSettings::get_singleton(), "locale/translations", translations);
    undo_redo->add_undo_property(ProjectSettings::get_singleton(), "locale/translations", ProjectSettings::get_singleton()->get("locale/translations"));
    undo_redo->add_do_method(this, "_update_translations");
    undo_redo->add_undo_method(this, "_update_translations");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();
}

void ProjectSettingsEditor::_translation_file_open() {

    translation_file_open->popup_centered_ratio();
}

void ProjectSettingsEditor::_translation_delete(Object *p_item, int p_column, int p_button) {

    TreeItem *ti = object_cast<TreeItem>(p_item);
    ERR_FAIL_COND(!ti);

    int idx = ti->get_metadata(0).as<int>();

    PoolStringArray translations = ProjectSettings::get_singleton()->getT<PoolStringArray>("locale/translations");

    ERR_FAIL_INDEX(idx, translations.size());

    translations.remove(idx);

    undo_redo->create_action(TTR("Remove Translation"));
    undo_redo->add_do_property(ProjectSettings::get_singleton(), "locale/translations", translations);
    undo_redo->add_undo_property(ProjectSettings::get_singleton(), "locale/translations", ProjectSettings::get_singleton()->get("locale/translations"));
    undo_redo->add_do_method(this, "_update_translations");
    undo_redo->add_undo_method(this, "_update_translations");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();
}

void ProjectSettingsEditor::_translation_res_file_open() {

    translation_res_file_open->popup_centered_ratio();
}

void ProjectSettingsEditor::_translation_res_add(const Vector<String> &p_paths) {

    Variant prev;
    Dictionary remaps;

    if (ProjectSettings::get_singleton()->has_setting("locale/translation_remaps")) {
        remaps = ProjectSettings::get_singleton()->get("locale/translation_remaps").as<Dictionary>();
        prev = remaps;
    }

    for (int i = 0; i < p_paths.size(); i++) {
        StringName path(p_paths[i]);
        if (!remaps.has(path)) {
            // Don't overwrite with an empty remap array if an array already exists for the given path.
            remaps[path] = PoolStringArray();
        }
    }
    undo_redo->create_action(FormatVE(TTR("Translation Resource Remap: Add %d Path(s)").asCString(), p_paths.size()));
    undo_redo->add_do_property(ProjectSettings::get_singleton(), "locale/translation_remaps", remaps);
    undo_redo->add_undo_property(ProjectSettings::get_singleton(), "locale/translation_remaps", prev);
    undo_redo->add_do_method(this, "_update_translations");
    undo_redo->add_undo_method(this, "_update_translations");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();
}

void ProjectSettingsEditor::_translation_res_option_file_open() {

    translation_res_option_file_open->popup_centered_ratio();
}
void ProjectSettingsEditor::_translation_res_option_add(const Vector<String> &p_paths) {

    ERR_FAIL_COND(!ProjectSettings::get_singleton()->has_setting("locale/translation_remaps"));

    Dictionary remaps = ProjectSettings::get_singleton()->get("locale/translation_remaps").as<Dictionary>();

    TreeItem *k = translation_remap->get_selected();
    ERR_FAIL_COND(!k);

    StringName key = k->get_metadata(0).as<StringName>();

    ERR_FAIL_COND(!remaps.has(key));
    PoolVector<String> r(remaps[key].as<PoolVector<String>>());
    for (int i = 0; i < p_paths.size(); i++) {
        r.push_back(p_paths[i] + ":" + "en");
    }
    remaps[key] = r;

    undo_redo->create_action(FormatVE(TTR("Translation Resource Remap: Add %d Remap(s)").asCString(), p_paths.size()));
    undo_redo->add_do_property(ProjectSettings::get_singleton(), "locale/translation_remaps", remaps);
    undo_redo->add_undo_property(ProjectSettings::get_singleton(), "locale/translation_remaps", ProjectSettings::get_singleton()->get("locale/translation_remaps"));
    undo_redo->add_do_method(this, "_update_translations");
    undo_redo->add_undo_method(this, "_update_translations");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();
}

void ProjectSettingsEditor::_translation_res_select() {

    if (updating_translations)
        return;
    call_deferred([this] { _update_translations(); });
}

void ProjectSettingsEditor::_translation_res_option_popup(bool p_arrow_clicked) {
    TreeItem *ed = translation_remap_options->get_edited();
    ERR_FAIL_COND(!ed);

    locale_select->set_locale(ed->get_tooltip(1));
    locale_select->popup_locale_dialog();
}

void ProjectSettingsEditor::_translation_res_option_selected(const String &p_locale) {
    TreeItem *ed = translation_remap_options->get_edited();
    ERR_FAIL_COND(!ed);

    ed->set_text_utf8(1, TranslationServer::get_singleton()->get_locale_name(p_locale));
    ed->set_tooltip(1, StringName(p_locale));

    ProjectSettingsEditor::_translation_res_option_changed();
}

void ProjectSettingsEditor::_translation_res_option_changed() {

    if (updating_translations)
        return;

    if (!ProjectSettings::get_singleton()->has_setting("locale/translation_remaps"))
        return;

    Dictionary remaps = ProjectSettings::get_singleton()->get("locale/translation_remaps").as<Dictionary>();

    TreeItem *k = translation_remap->get_selected();
    ERR_FAIL_COND(!k);
    TreeItem *ed = translation_remap_options->get_edited();
    ERR_FAIL_COND(!ed);

    StringName key = k->get_metadata(0).as<StringName>();
    int idx = ed->get_metadata(0).as<int>();
    String path = ed->get_metadata(1).as<String>();
    const String &locale = ed->get_tooltip(1);

    ERR_FAIL_COND(!remaps.has(key));
    PoolStringArray r = remaps[key].as<PoolStringArray>();
    r.set(idx, path + ":" + locale);
    remaps[key] = r;

    updating_translations = true;
    undo_redo->create_action(TTR("Change Resource Remap Language"));
    undo_redo->add_do_property(ProjectSettings::get_singleton(), "locale/translation_remaps", remaps);
    undo_redo->add_undo_property(ProjectSettings::get_singleton(), "locale/translation_remaps", ProjectSettings::get_singleton()->get("locale/translation_remaps"));
    undo_redo->add_do_method(this, "_update_translations");
    undo_redo->add_undo_method(this, "_update_translations");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();
    updating_translations = false;
}

void ProjectSettingsEditor::_translation_res_delete(Object *p_item, int p_column, int p_button) {

    if (updating_translations)
        return;

    if (!ProjectSettings::get_singleton()->has_setting("locale/translation_remaps"))
        return;

    Dictionary remaps = ProjectSettings::get_singleton()->get("locale/translation_remaps").as<Dictionary>();

    TreeItem *k = object_cast<TreeItem>(p_item);

    StringName key = k->get_metadata(0).as<StringName>();
    ERR_FAIL_COND(!remaps.has(key));

    remaps.erase(key);

    undo_redo->create_action(TTR("Remove Resource Remap"));
    undo_redo->add_do_property(ProjectSettings::get_singleton(), "locale/translation_remaps", remaps);
    undo_redo->add_undo_property(ProjectSettings::get_singleton(), "locale/translation_remaps", ProjectSettings::get_singleton()->get("locale/translation_remaps"));
    undo_redo->add_do_method(this, "_update_translations");
    undo_redo->add_undo_method(this, "_update_translations");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();
}

void ProjectSettingsEditor::_translation_res_option_delete(Object *p_item, int p_column, int p_button) {

    if (updating_translations)
        return;

    if (!ProjectSettings::get_singleton()->has_setting("locale/translation_remaps"))
        return;

    Dictionary remaps = ProjectSettings::get_singleton()->get("locale/translation_remaps").as<Dictionary>();

    TreeItem *k = translation_remap->get_selected();
    ERR_FAIL_COND(!k);
    TreeItem *ed = object_cast<TreeItem>(p_item);
    ERR_FAIL_COND(!ed);

    auto key = k->get_metadata(0).as<StringName>();
    int idx = ed->get_metadata(0).as<int>();

    ERR_FAIL_COND(!remaps.has(key));
    PoolStringArray r = remaps[key].as<PoolStringArray>();
    ERR_FAIL_INDEX(idx, r.size());
    r.remove(idx);
    remaps[key] = r;

    undo_redo->create_action(TTR("Remove Resource Remap Option"));
    undo_redo->add_do_property(ProjectSettings::get_singleton(), "locale/translation_remaps", remaps);
    undo_redo->add_undo_property(ProjectSettings::get_singleton(), "locale/translation_remaps", ProjectSettings::get_singleton()->get("locale/translation_remaps"));
    undo_redo->add_do_method(this, "_update_translations");
    undo_redo->add_undo_method(this, "_update_translations");
    undo_redo->add_do_method(this, "_settings_changed");
    undo_redo->add_undo_method(this, "_settings_changed");
    undo_redo->commit_action();
}

void ProjectSettingsEditor::_update_translations() {

    //update translations

    if (updating_translations)
        return;

    updating_translations = true;

    translation_list->clear();
    TreeItem *root = translation_list->create_item(nullptr);
    translation_list->set_hide_root(true);
    if (ProjectSettings::get_singleton()->has_setting("locale/translations")) {

        PoolVector<String> translations(ProjectSettings::get_singleton()->get("locale/translations").as<PoolVector<String>>());
        for (int i = 0; i < translations.size(); i++) {

            TreeItem *t = translation_list->create_item(root);
            t->set_editable(0, false);
            t->set_text(0, StringName(StringUtils::replace_first(translations[i],"res://", StringView(""))));
            t->set_tooltip(0, StringName(translations[i]));
            t->set_metadata(0, i);
            t->add_button(0, get_theme_icon("Remove", "EditorIcons"), 0, false, TTR("Remove"));
        }
    }

    //update translation remaps

    String remap_selected;
    if (translation_remap->get_selected()) {
        remap_selected = translation_remap->get_selected()->get_metadata(0).as<String>();
    }

    translation_remap->clear();
    translation_remap_options->clear();
    root = translation_remap->create_item(nullptr);
    TreeItem *root2 = translation_remap_options->create_item(nullptr);
    translation_remap->set_hide_root(true);
    translation_remap_options->set_hide_root(true);
    translation_res_option_add_button->set_disabled(true);

    if (ProjectSettings::get_singleton()->has_setting("locale/translation_remaps")) {

        Dictionary remaps = ProjectSettings::get_singleton()->getT<Dictionary>("locale/translation_remaps");
        auto keys(remaps.get_key_list());

        eastl::sort(keys.begin(), keys.end(), WrapAlphaCompare());

        for (int i = 0; i < keys.size(); i++) {

            TreeItem *t = translation_remap->create_item(root);
            t->set_editable(0, false);
            t->set_text_utf8(0, StringUtils::replace_first(keys[i],"res://", String()));
            t->set_tooltip(0, StringName(keys[i]));
            t->set_metadata(0, keys[i]);
            t->add_button(0, get_theme_icon("Remove", "EditorIcons"), 0, false, TTR("Remove"));

            if (keys[i] != remap_selected)
                continue;

            t->select(0);
            translation_res_option_add_button->set_disabled(false);

            PoolVector<String> selected(remaps[keys[i]].as<PoolVector<String>>());
            for (int j = 0; j < selected.size(); j++) {

                String s2 = selected[j];
                int qp = StringUtils::rfind(s2,':');
                String path(StringUtils::substr(s2,0, qp));
                String locale(StringUtils::substr(s2,qp + 1, s2.length()));

                TreeItem *t2 = translation_remap_options->create_item(root2);
                t2->set_editable(0, false);
                t2->set_text(0, StringName(StringUtils::replace_first(path,"res://", StringView())));
                t2->set_tooltip(0, StringName(path));
                t2->set_metadata(0, j);
                t2->add_button(0, get_theme_icon("Remove", "EditorIcons"), 0, false, TTR("Remove"));
                t2->set_cell_mode(1, TreeItem::CELL_MODE_CUSTOM);
                t2->set_text_utf8(1, TranslationServer::get_singleton()->get_locale_name(locale));
                t2->set_editable(1, true);
                t2->set_metadata(1, path);
                t2->set_tooltip(1, StringName(locale));
            }
        }
    }

    updating_translations = false;
}

void ProjectSettingsEditor::_toggle_search_bar(bool p_pressed) {

    globals_editor->get_inspector()->set_use_filter(p_pressed);

    if (p_pressed) {

        search_bar->show();
        add_prop_bar->hide();
        search_box->grab_focus();
        search_box->select_all();
    } else {

        search_box->clear();
        search_bar->hide();
        add_prop_bar->show();
    }
}

void ProjectSettingsEditor::set_plugins_page() {

    tab_container->set_current_tab(plugin_settings->get_index());
}

TabContainer *ProjectSettingsEditor::get_tabs() {

    return tab_container;
}

void ProjectSettingsEditor::_editor_restart() {
    ProjectSettings::get_singleton()->save();
    EditorNode::get_singleton()->save_all_scenes();
    EditorNode::get_singleton()->restart_editor();
}

void ProjectSettingsEditor::_editor_restart_request() {
    restart_container->show();
}

void ProjectSettingsEditor::_editor_restart_close() {
    restart_container->hide();
}

void ProjectSettingsEditor::_update_theme() {
    type_box->clear();
    for (int i = 0; i < (int)VariantType::VARIANT_MAX; i++) {
        VariantType as_vt = VariantType(i);
        // There's no point in adding Nil types, and Object types
        // can't be serialized correctly in the project settings.
        if (as_vt != VariantType::NIL && as_vt != VariantType::OBJECT) {
            const StringName type = Variant::interned_type_name(as_vt);
            type_box->add_icon_item(get_theme_icon(type, "EditorIcons"), type, i);
        }
    }
}
void ProjectSettingsEditor::_bind_methods() {

    SE_BIND_METHOD(ProjectSettingsEditor,_unhandled_input);
    SE_BIND_METHOD(ProjectSettingsEditor,_update_actions);
    SE_BIND_METHOD(ProjectSettingsEditor,_update_translations);
    SE_BIND_METHOD(ProjectSettingsEditor,_settings_changed);

    SE_BIND_METHOD(ProjectSettingsEditor,get_tabs);

    SE_BIND_METHOD(ProjectSettingsEditor,get_drag_data_fw);
    SE_BIND_METHOD(ProjectSettingsEditor,can_drop_data_fw);
    SE_BIND_METHOD(ProjectSettingsEditor,drop_data_fw);
}

ProjectSettingsEditor::ProjectSettingsEditor(EditorData *p_data) {

    singleton = this;
    set_title(TTR("Project Settings (project.godot)"));
    set_resizable(true);
    undo_redo = &p_data->get_undo_redo();
    data = p_data;

    tab_container = memnew(TabContainer);
    tab_container->set_tab_align(TabContainer::ALIGN_LEFT);
    tab_container->set_use_hidden_tabs_for_min_size(true);
    add_child(tab_container);

    VBoxContainer *props_base = memnew(VBoxContainer);
    props_base->set_alignment(BoxContainer::ALIGN_BEGIN);
    props_base->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    tab_container->add_child(props_base);
    props_base->set_name(TTR("General"));

    HBoxContainer *hbc = memnew(HBoxContainer);
    hbc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    props_base->add_child(hbc);

    search_button = memnew(Button);
    search_button->set_toggle_mode(true);
    search_button->set_pressed(false);
    search_button->set_text(TTR("Search"));
    hbc->add_child(search_button);
    search_button->connect("toggled", callable_mp(this, &ProjectSettingsEditor::_toggle_search_bar));

    hbc->add_child(memnew(VSeparator));

    add_prop_bar = memnew(HBoxContainer);
    add_prop_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    hbc->add_child(add_prop_bar);

    Label *l = memnew(Label);
    add_prop_bar->add_child(l);
    l->set_text(TTR("Property:"));

    property = memnew(LineEdit);
    property->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    add_prop_bar->add_child(property);
    property->connect("text_entered", callable_mp(this, &ProjectSettingsEditor::_item_adds));

    l = memnew(Label);
    add_prop_bar->add_child(l);
    l->set_text(TTR("Type:"));

    type_box = memnew(OptionButton);
    type_box->set_custom_minimum_size(Size2(100, 0) * EDSCALE);
    add_prop_bar->add_child(type_box);

    Button *add = memnew(Button);
    add_prop_bar->add_child(add);
    add->set_text(TTR("Add"));
    add->connect("pressed", callable_mp(this, &ProjectSettingsEditor::_item_add));

    search_bar = memnew(HBoxContainer);
    search_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    hbc->add_child(search_bar);
    search_bar->hide();

    search_box = memnew(LineEdit);
    search_box->set_placeholder(TTR("Search"));
    search_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    search_bar->add_child(search_box);

    globals_editor = memnew(SectionedInspector);
    props_base->add_child(globals_editor);
    EditorInspector *editor_inspector = globals_editor->get_inspector();
    editor_inspector->set_undo_redo(EditorNode::get_singleton()->get_undo_redo());
    globals_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    globals_editor->register_search_box(search_box);
    editor_inspector->connect("property_selected",callable_mp(this, &ProjectSettingsEditor::_item_selected));
    editor_inspector->connect("property_edited",callable_mp(this, &ProjectSettingsEditor::_settings_prop_edited));
    editor_inspector->connect("restart_requested",callable_mp(this, &ProjectSettingsEditor::_editor_restart_request));

    Button *del = memnew(Button);
    hbc->add_child(del);
    del->set_text(TTR("Delete"));
    del->connect("pressed",callable_mp(this, &ProjectSettingsEditor::_item_del));

    add_prop_bar->add_child(memnew(VSeparator));

    popup_copy_to_feature = memnew(MenuButton);
    popup_copy_to_feature->set_text(TTR("Override For..."));
    popup_copy_to_feature->set_disabled(true);
    add_prop_bar->add_child(popup_copy_to_feature);

    popup_copy_to_feature->get_popup()->connect("id_pressed",callable_mp(this, &ProjectSettingsEditor::_copy_to_platform));
    popup_copy_to_feature->get_popup()->connect("about_to_show",callable_mp(this, &ProjectSettingsEditor::_copy_to_platform_about_to_show));

    get_ok()->set_text(TTR("Close"));
    set_hide_on_ok(true);

    restart_container = memnew(PanelContainer);
    props_base->add_child(restart_container);
    HBoxContainer *restart_hb = memnew(HBoxContainer);
    restart_container->add_child(restart_hb);
    restart_icon = memnew(TextureRect);
    restart_icon->set_v_size_flags(SIZE_SHRINK_CENTER);
    restart_hb->add_child(restart_icon);
    restart_label = memnew(Label);
    restart_label->set_text(TTR("The editor must be restarted for changes to take effect."));
    restart_hb->add_child(restart_label);
    restart_hb->add_spacer();
    Button *restart_button = memnew(Button);
    restart_button->connect("pressed",callable_mp(this, &ProjectSettingsEditor::_editor_restart));
    restart_hb->add_child(restart_button);
    restart_button->set_text(TTR("Save & Restart"));
    restart_close_button = memnew(ToolButton);
    restart_close_button->connect("pressed",callable_mp(this, &ProjectSettingsEditor::_editor_restart_close));
    restart_hb->add_child(restart_close_button);
    restart_container->hide();

    message = memnew(AcceptDialog);
    add_child(message);

    Control *input_base = memnew(Control);
    input_base->set_name(TTR("Input Map"));
    tab_container->add_child(input_base);

    VBoxContainer *vbc = memnew(VBoxContainer);
    input_base->add_child(vbc);
    vbc->set_anchor_and_margin(Margin::Top, ANCHOR_BEGIN, 0);
    vbc->set_anchor_and_margin(Margin::Bottom, ANCHOR_END, 0);
    vbc->set_anchor_and_margin(Margin::Left, ANCHOR_BEGIN, 0);
    vbc->set_anchor_and_margin(Margin::Right, ANCHOR_END, 0);

    hbc = memnew(HBoxContainer);
    vbc->add_child(hbc);

    l = memnew(Label);
    hbc->add_child(l);
    l->set_text(TTR("Action:"));

    action_name = memnew(LineEdit);
    action_name->set_h_size_flags(SIZE_EXPAND_FILL);
    hbc->add_child(action_name);
    action_name->connect("text_entered",callable_mp(this, &ProjectSettingsEditor::_action_adds));
    action_name->connect("text_changed",callable_mp(this, &ProjectSettingsEditor::_action_check));

    action_add_error = memnew(Label);
    hbc->add_child(action_add_error);
    action_add_error->hide();

    add = memnew(Button);
    hbc->add_child(add);
    add->set_text(TTR("Add"));
    add->set_disabled(true);
    add->connect("pressed",callable_mp(this, &ProjectSettingsEditor::_action_add));
    action_add = add;

    input_editor = memnew(Tree);
    vbc->add_child(input_editor);
    input_editor->set_v_size_flags(SIZE_EXPAND_FILL);
    input_editor->set_columns(3);
    input_editor->set_column_titles_visible(true);
    input_editor->set_column_title(0, TTR("Action"));
    input_editor->set_column_title(1, TTR("Deadzone"));
    input_editor->set_column_expand(1, false);
    input_editor->set_column_min_width(1, 80 * EDSCALE);
    input_editor->set_column_expand(2, false);
    input_editor->set_column_min_width(2, 50 * EDSCALE);
    input_editor->connect("item_edited",callable_mp(this, &ProjectSettingsEditor::_action_edited));
    input_editor->connect("item_activated",callable_mp(this, &ProjectSettingsEditor::_action_activated));
    input_editor->connect("cell_selected",callable_mp(this, &ProjectSettingsEditor::_action_selected));
    input_editor->connect("button_pressed",callable_mp(this, &ProjectSettingsEditor::_action_button_pressed));
    input_editor->set_drag_forwarding(this);

    popup_add = memnew(PopupMenu);
    add_child(popup_add);
    popup_add->connect("id_pressed",callable_mp(this, &ProjectSettingsEditor::_add_item));
    press_a_key_physical = false;

    press_a_key = memnew(ConfirmationDialog);
    press_a_key->set_focus_mode(FOCUS_ALL);
    add_child(press_a_key);

    l = memnew(Label);
    l->set_text(TTR("Press a Key..."));
    l->set_anchors_and_margins_preset(Control::PRESET_WIDE);
    l->set_align(Label::ALIGN_CENTER);
    l->set_margin(Margin::Top, 20);
    l->set_anchor_and_margin(Margin::Bottom, ANCHOR_BEGIN, 30);
    press_a_key->get_ok()->set_disabled(true);
    press_a_key_label = l;
    press_a_key->add_child(l);
    press_a_key->connect("gui_input",callable_mp(this, &ProjectSettingsEditor::_wait_for_key));
    press_a_key->connect("confirmed",callable_mp(this, &ProjectSettingsEditor::_press_a_key_confirm));

    device_input = memnew(ConfirmationDialog);
    add_child(device_input);
    device_input->get_ok()->set_text(TTR("Add"));
    device_input->connect("confirmed",callable_mp(this, &ProjectSettingsEditor::_device_input_add));

    hbc = memnew(HBoxContainer);
    device_input->add_child(hbc);

    VBoxContainer *vbc_left = memnew(VBoxContainer);
    hbc->add_child(vbc_left);

    l = memnew(Label);
    l->set_text(TTR("Device:"));
    vbc_left->add_child(l);

    device_id = memnew(OptionButton);
    for (int i = -1; i < 8; i++)
        device_id->add_item(_get_device_string(i));
    _set_current_device(0);
    vbc_left->add_child(device_id);

    VBoxContainer *vbc_right = memnew(VBoxContainer);
    hbc->add_child(vbc_right);
    vbc_right->set_h_size_flags(SIZE_EXPAND_FILL);

    l = memnew(Label);
    l->set_text(TTR("Index:"));
    vbc_right->add_child(l);
    device_index_label = l;

    device_index = memnew(OptionButton);
    device_index->set_clip_text(true);

    vbc_right->add_child(device_index);

    setting = false;

    //translations
    TabContainer *translations = memnew(TabContainer);
    translations->set_tab_align(TabContainer::ALIGN_LEFT);
    translations->set_name(TTR("Localization"));
    tab_container->add_child(translations);

    {

        VBoxContainer *tvb = memnew(VBoxContainer);
        translations->add_child(tvb);
        tvb->set_name(TTR("Translations"));
        HBoxContainer *thb = memnew(HBoxContainer);
        tvb->add_child(thb);
        thb->add_child(memnew(Label(TTR("Translations:"))));
        thb->add_spacer();
        Button *addtr = memnew(Button(TTR("Add...")));
        addtr->connect("pressed",callable_mp(this, &ProjectSettingsEditor::_translation_file_open));
        thb->add_child(addtr);
        VBoxContainer *tmc = memnew(VBoxContainer);
        tvb->add_child(tmc);
        tmc->set_v_size_flags(SIZE_EXPAND_FILL);
        translation_list = memnew(Tree);
        translation_list->set_v_size_flags(SIZE_EXPAND_FILL);
        tmc->add_child(translation_list);

        locale_select = memnew(EditorLocaleDialog);
        locale_select->connect("locale_selected", callable_mp(this, &ProjectSettingsEditor::_translation_res_option_selected));
        add_child(locale_select);

        translation_file_open = memnew(EditorFileDialog);
        add_child(translation_file_open);
        translation_file_open->set_mode(EditorFileDialog::MODE_OPEN_FILES);
        translation_file_open->connect("files_selected",callable_mp(this, &ProjectSettingsEditor::_translation_add));
    }

    {
        VBoxContainer *tvb = memnew(VBoxContainer);
        translations->add_child(tvb);
        tvb->set_name(TTR("Remaps"));
        HBoxContainer *thb = memnew(HBoxContainer);
        tvb->add_child(thb);
        thb->add_child(memnew(Label(TTR("Resources:"))));
        thb->add_spacer();
        Button *addtr = memnew(Button(TTR("Add...")));
        addtr->connect("pressed",callable_mp(this, &ProjectSettingsEditor::_translation_res_file_open));
        thb->add_child(addtr);
        VBoxContainer *tmc = memnew(VBoxContainer);
        tvb->add_child(tmc);
        tmc->set_v_size_flags(SIZE_EXPAND_FILL);
        translation_remap = memnew(Tree);
        translation_remap->set_v_size_flags(SIZE_EXPAND_FILL);
        translation_remap->connect("cell_selected",callable_mp(this, &ProjectSettingsEditor::_translation_res_select));
        tmc->add_child(translation_remap);
        translation_remap->connect("button_pressed",callable_mp(this, &ProjectSettingsEditor::_translation_res_delete));

        translation_res_file_open = memnew(EditorFileDialog);
        add_child(translation_res_file_open);
        translation_res_file_open->set_mode(EditorFileDialog::MODE_OPEN_FILES);
        translation_res_file_open->connect("files_selected",callable_mp(this, &ProjectSettingsEditor::_translation_res_add));

        thb = memnew(HBoxContainer);
        tvb->add_child(thb);
        thb->add_child(memnew(Label(TTR("Remaps by Locale:"))));
        thb->add_spacer();
        addtr = memnew(Button(TTR("Add...")));
        addtr->connect("pressed",callable_mp(this, &ProjectSettingsEditor::_translation_res_option_file_open));
        translation_res_option_add_button = addtr;
        thb->add_child(addtr);
        tmc = memnew(VBoxContainer);
        tvb->add_child(tmc);
        tmc->set_v_size_flags(SIZE_EXPAND_FILL);
        translation_remap_options = memnew(Tree);
        translation_remap_options->set_v_size_flags(SIZE_EXPAND_FILL);
        tmc->add_child(translation_remap_options);

        translation_remap_options->set_columns(2);
        translation_remap_options->set_column_title(0, TTR("Path"));
        translation_remap_options->set_column_title(1, TTR("Locale"));
        translation_remap_options->set_column_titles_visible(true);
        translation_remap_options->set_column_expand(0, true);
        translation_remap_options->set_column_expand(1, false);
        translation_remap_options->set_column_min_width(1, 250 * EDSCALE);
        translation_remap_options->connect("item_edited",callable_mp(this, &ProjectSettingsEditor::_translation_res_option_changed));
        translation_remap_options->connect("button_pressed",callable_mp(this, &ProjectSettingsEditor::_translation_res_option_delete));
        translation_remap_options->connect("custom_popup_edited", callable_mp(this, &ProjectSettingsEditor::_translation_res_option_popup));

        translation_res_option_file_open = memnew(EditorFileDialog);
        add_child(translation_res_option_file_open);
        translation_res_option_file_open->set_mode(EditorFileDialog::MODE_OPEN_FILE);
        translation_res_option_file_open->connect("file_selected",callable_mp(this, &ProjectSettingsEditor::_translation_res_option_add));
    }

    autoload_settings = memnew(EditorAutoloadSettings);
    autoload_settings->set_name(TTR("AutoLoad"));
    tab_container->add_child(autoload_settings);
    autoload_settings->connect("autoload_changed",callable_mp(this, &ProjectSettingsEditor::_settings_changed));

    plugin_settings = memnew(EditorPluginSettings);
    plugin_settings->set_name(TTR("Plugins"));
    tab_container->add_child(plugin_settings);
    import_defaults_editor = memnew(ImportDefaultsEditor);
    import_defaults_editor->set_name(TTR("Import Defaults"));
    tab_container->add_child(import_defaults_editor);

    timer = memnew(Timer);
    timer->set_wait_time(1.5);
    timer->connect("timeout",callable_mp(ProjectSettings::get_singleton(), &ProjectSettings::save));
    timer->set_one_shot(true);
    add_child(timer);

    updating_translations = false;
}
