/*************************************************************************/
/*  inspector_dock.cpp                                                   */
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

#include "inspector_dock.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/resource/resource_manager.h"
#include "create_dialog.h"
#include "editor/animation_track_editor.h"
#include "editor/editor_node.h"
#include "editor/editor_path.h"
#include "editor/editor_settings.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "scene/resources/style_box.h"
#include "scene/resources/theme.h"

IMPL_GDCLASS(InspectorDock)

void InspectorDock::_prepare_menu() {
    PopupMenu *menu = object_menu->get_popup();
    for (int i = (int)EditorPropertyNameStyle::RAW; i <= (int)EditorPropertyNameStyle::LOCALIZED; i++) {
        menu->set_item_checked(menu->get_item_index(PROPERTY_NAME_STYLE_RAW + i), i == (int)property_name_style);
    }
}
void InspectorDock::_menu_option(int p_option) {
    switch (p_option) {
        case EXPAND_ALL: {
            _menu_expandall();
        } break;
        case COLLAPSE_ALL: {
            _menu_collapseall();
        } break;
        case RESOURCE_SAVE: {
            _save_resource(false);
        } break;
        case RESOURCE_SAVE_AS: {
            _save_resource(true);
        } break;
        case RESOURCE_MAKE_BUILT_IN: {
            _unref_resource();
        } break;
        case RESOURCE_COPY: {
            _copy_resource();
        } break;
        case RESOURCE_EDIT_CLIPBOARD: {
            _paste_resource();
        } break;


        case OBJECT_REQUEST_HELP: {
            if (current) {
                editor->set_visible_editor(EditorNode::EDITOR_SCRIPT);
                emit_signal("request_help", current->get_class());
            }
        } break;

        case OBJECT_COPY_PARAMS: {
            editor_data->apply_changes_in_editors();
            if (current)
                editor_data->copy_object_params(current);
        } break;

        case OBJECT_PASTE_PARAMS: {
            editor_data->apply_changes_in_editors();
            if (current)
                editor_data->paste_object_params(current);
        } break;

        case OBJECT_UNIQUE_RESOURCES: {
            editor_data->apply_changes_in_editors();
            if (current) {
                Vector<PropertyInfo> props;
                current->get_property_list(&props);
                HashMap<RES, RES> duplicates;
                for (const PropertyInfo &E : props) {

                    if (!(E.usage & PROPERTY_USAGE_STORAGE))
                        continue;

                    Variant v = current->get(E.name);
                    if (v.is_ref()) {
                        REF ref(v);
                        if (ref) {

                            RES res = dynamic_ref_cast<Resource>(ref);
                            if (res) {

                                if (!duplicates.contains(res)) {
                                    duplicates[res] = res->duplicate();
                                }
                                res = duplicates[res];

                                current->set(E.name, res);
                                editor->get_inspector()->update_property(E.name);
                            }
                        }
                    }
                }
            }

            editor_data->get_undo_redo().clear_history();

            editor->get_editor_plugins_over()->edit(nullptr);
            editor->get_editor_plugins_over()->edit(current);

        } break;

        case PROPERTY_NAME_STYLE_RAW:
        case PROPERTY_NAME_STYLE_CAPITALIZED:
        case PROPERTY_NAME_STYLE_LOCALIZED: {
            property_name_style = (EditorPropertyNameStyle)(p_option - PROPERTY_NAME_STYLE_RAW);
            inspector->set_property_name_style(property_name_style);
        } break;
        default: {
            if (p_option >= OBJECT_METHOD_BASE) {
                ERR_FAIL_COND(!current);

                int idx = p_option - OBJECT_METHOD_BASE;

                Vector<MethodInfo> methods;
                current->get_method_list(&methods);

                ERR_FAIL_INDEX(idx, methods.size());
                StringName name = methods[idx].name;

                current->call_va(name);
            }
        }
    }
}

void InspectorDock::_new_resource() {
    new_resource_dialog->popup_create(true);
}

void InspectorDock::_load_resource(StringView p_type) {
    load_resource_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);

    Vector<String> extensions;
    gResourceManager().get_recognized_extensions_for_type(p_type, extensions);

    load_resource_dialog->clear_filters();
    for (const String &ext : extensions) {
        load_resource_dialog->add_filter("*." + ext + " ; " + StringUtils::to_upper(ext));
    }

    load_resource_dialog->popup_centered_ratio();
}

void InspectorDock::_resource_file_selected(StringView p_file) {
    RES res(gResourceManager().load(p_file));

    if (not res) {
        warning_dialog->set_text(TTR("Failed to load resource."));
        return;
    }

    editor->push_item(res.operator->());
}

void InspectorDock::_save_resource(bool save_as) const {
    auto current = EditorNode::get_singleton()->get_editor_history()->get_current();
    Object *current_obj = current!=entt::null ? object_for_entity(current) : nullptr;
    RES current_res(object_cast<Resource>(current_obj));

    ERR_FAIL_COND(not current_res);


    if (save_as)
        editor->save_resource_as(current_res);
    else
        editor->save_resource(current_res);
}

void InspectorDock::_unref_resource() const {
    auto current = EditorNode::get_singleton()->get_editor_history()->get_current();
    Object *current_obj = current!=entt::null ? object_for_entity(current) : nullptr;
    RES current_res(object_cast<Resource>(current_obj));

    ERR_FAIL_COND(not current_res);

    current_res->set_path(StringView());
    editor->edit_current();
}

void InspectorDock::_copy_resource() const {
    auto current = EditorNode::get_singleton()->get_editor_history()->get_current();
    Object *current_obj = current!=entt::null ? object_for_entity(current) : nullptr;
    RES current_res(object_cast<Resource>(current_obj));

    ERR_FAIL_COND(not current_res);


    EditorSettings::get_singleton()->set_resource_clipboard(current_res);
}

void InspectorDock::_paste_resource() const {
    RES r(EditorSettings::get_singleton()->get_resource_clipboard());
    if (r) {
        editor->push_item(EditorSettings::get_singleton()->get_resource_clipboard().get(), StringView());
    }
}

void InspectorDock::_prepare_resource_extra_popup() {
    RES r = EditorSettings::get_singleton()->get_resource_clipboard();
    PopupMenu *popup = resource_extra_button->get_popup();
    popup->set_item_disabled(popup->get_item_index(RESOURCE_EDIT_CLIPBOARD), !r);
}

void InspectorDock::_prepare_history() {
    EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();

    int history_to = M_MAX(0, editor_history->get_history_len() - 25);

    history_menu->get_popup()->clear();

    Ref<Texture> base_icon = get_theme_icon("Object", "EditorIcons");
    HashSet<GameEntity> already;
    for (int i = editor_history->get_history_len() - 1; i >= history_to; i--) {

        GameEntity id = editor_history->get_history_obj(i);
        Object *obj = object_for_entity(id);
        if (!obj || already.contains(id)) {
            if (history_to > 0) {
                history_to--;
            }
            continue;
        }

        already.insert(id);

        Ref<Texture> icon = EditorNode::get_singleton()->get_object_icon(obj, "");
        if (not icon) {
            icon = base_icon;
        }

        String text;
        if (object_cast<Resource>(obj)) {
            Resource *r = object_cast<Resource>(obj);
            if (PathUtils::is_resource_file(r->get_path()))
                text = PathUtils::get_file(r->get_path());
            else if (!r->get_name().empty()) {
                text = r->get_name();
            } else {
                text = r->get_class();
            }
        } else if (object_cast<Node>(obj)) {
            text = object_cast<Node>(obj)->get_name();
        } else if (obj->is_class("ScriptEditorDebuggerInspectedObject")) {
            text = obj->call_va("get_title").as<String>();
        } else {
            text = obj->get_class();
        }

        if (i == editor_history->get_history_pos() && current) {
            text = "[" + text + "]";
        }
        history_menu->get_popup()->add_icon_item(icon, StringName(text), i);
    }
}

void InspectorDock::_select_history(int p_idx) {
    //push it to the top, it is not correct, but it's more useful
    GameEntity id = EditorNode::get_singleton()->get_editor_history()->get_history_obj(p_idx);
    Object *obj = object_for_entity(id);
    if (!obj)
        return;
    editor->push_item(obj);
}

void InspectorDock::_resource_created() {
    Object *c = new_resource_dialog->instance_selected();

    ERR_FAIL_COND(!c);
    RES r(object_cast<Resource>(c));
    ERR_FAIL_COND(!r);

    editor->push_item(c);
}

void InspectorDock::_resource_selected(const RES &p_res, const StringName &p_property) {
    if (not p_res)
        return;

    editor->push_item(p_res.get(), p_property);
}

void InspectorDock::_edit_forward() {
    if (EditorNode::get_singleton()->get_editor_history()->next()) {
        editor->edit_current();
    }
}
void InspectorDock::_edit_back() {
    EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();
    if ((current && editor_history->previous()) || editor_history->get_path_size() == 1) {
        editor->edit_current();
    }
}

void InspectorDock::_menu_collapseall() {
    inspector->collapse_all_folding();
}

void InspectorDock::_menu_expandall() {
    inspector->expand_all_folding();
}

void InspectorDock::_property_keyed(StringView p_keyed, const Variant &p_value, bool p_advance) {
    AnimationPlayerEditor::singleton->get_track_editor()->insert_value_key(p_keyed, p_value, p_advance);
}

void InspectorDock::_transform_keyed(Object *sp, StringView p_sub, const Transform &p_key) {
    Node3D *s = object_cast<Node3D>(sp);
    if (!s)
        return;
    AnimationPlayerEditor::singleton->get_track_editor()->insert_transform_key(s, p_sub, p_key);
}

void InspectorDock::_warning_pressed() {
    warning_dialog->popup_centered_minsize();
}

Container *InspectorDock::get_addon_area() {
    return this;
}

void InspectorDock::_notification(int p_what) {
    switch (p_what) {
        case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
            set_theme(editor->get_gui_base()->get_theme());
            resource_new_button->set_button_icon(get_theme_icon("New", "EditorIcons"));
            resource_load_button->set_button_icon(get_theme_icon("Load", "EditorIcons"));
            resource_save_button->set_button_icon(get_theme_icon("Save", "EditorIcons"));
            resource_extra_button->set_button_icon(get_theme_icon("GuiTabMenuHl", "EditorIcons"));

            PopupMenu *resource_extra_popup = resource_extra_button->get_popup();
            resource_extra_popup->set_item_icon(resource_extra_popup->get_item_index(RESOURCE_EDIT_CLIPBOARD), get_theme_icon("ActionPaste", "EditorIcons"));
            resource_extra_popup->set_item_icon(resource_extra_popup->get_item_index(RESOURCE_COPY), get_theme_icon("ActionCopy", "EditorIcons"));
            backward_button->set_button_icon(get_theme_icon("Back", "EditorIcons"));
            forward_button->set_button_icon(get_theme_icon("Forward", "EditorIcons"));
            history_menu->set_button_icon(get_theme_icon("History", "EditorIcons"));
            object_menu->set_button_icon(get_theme_icon("Tools", "EditorIcons"));
            warning->set_button_icon(get_theme_icon("NodeWarning", "EditorIcons"));
            warning->add_theme_color_override("font_color", get_theme_color("warning_color", "Editor"));
        } break;
    }
}

void InspectorDock::_bind_methods() {

    MethodBinder::bind_method("update_keying", &InspectorDock::update_keying);

    //MethodBinder::bind_method("_resource_selected", &InspectorDock::_resource_selected, {DEFVAL("")});

    ADD_SIGNAL(MethodInfo("request_help"));
}

void InspectorDock::edit_resource(const Ref<Resource> &p_resource) {
    _resource_selected(p_resource, StringName());
}

void InspectorDock::open_resource(StringView p_type) {
    _load_resource(p_type);
}

void InspectorDock::set_warning(const StringName &p_message) {
    warning->hide();
    if (!p_message.empty()) {
        warning->show();
        warning_dialog->set_text(p_message);
    }
}

void InspectorDock::clear() {
}

void InspectorDock::update(Object *p_object) {

    EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();
    backward_button->set_disabled(editor_history->is_at_beginning());
    forward_button->set_disabled(editor_history->is_at_end());

    history_menu->set_disabled(true);
    if (editor_history->get_history_len() > 0) {
        history_menu->set_disabled(false);
    }
    editor_path->update_path();

    current = p_object;

    const bool is_object = p_object != nullptr;
    const bool is_resource = is_object && p_object->is_class("Resource");
    const bool is_node = is_object && p_object->is_class("Node");

    object_menu->set_disabled(!is_object);
    search->set_editable(is_object);
    resource_save_button->set_disabled(!is_resource);
    open_docs_button->set_disabled(!is_resource && !is_node);

    PopupMenu *resource_extra_popup = resource_extra_button->get_popup();
    resource_extra_popup->set_item_disabled(resource_extra_popup->get_item_index(RESOURCE_COPY), !is_resource);
    resource_extra_popup->set_item_disabled(resource_extra_popup->get_item_index(RESOURCE_MAKE_BUILT_IN), !is_resource);

    if (!is_object) {
        warning->hide();
        editor_path->clear_path();
        return;
    }

    editor_path->enable_path();

    PopupMenu *p = object_menu->get_popup();

    p->clear();
    p->add_icon_shortcut(get_theme_icon("GuiTreeArrowDown", "EditorIcons"), ED_SHORTCUT("property_editor/expand_all", TTR("Expand All")), EXPAND_ALL);
    p->add_icon_shortcut(get_theme_icon("GuiTreeArrowRight", "EditorIcons"), ED_SHORTCUT("property_editor/collapse_all", TTR("Collapse All")), COLLAPSE_ALL);
    p->add_separator(TTR("Property Name Style"));
    p->add_radio_check_item(TTR("Raw"), PROPERTY_NAME_STYLE_RAW);
    p->add_radio_check_item(TTR("Capitalized"), PROPERTY_NAME_STYLE_CAPITALIZED);
    p->add_radio_check_item(TTR("Localized"), PROPERTY_NAME_STYLE_LOCALIZED);

    if (!EditorPropertyNameProcessor::is_localization_available()) {
        const int index = p->get_item_index(PROPERTY_NAME_STYLE_LOCALIZED);
        p->set_item_disabled(index, true);
        p->set_item_tooltip(index, TTR("Localization not available for current language."));
    }
    p->add_separator();

    p->add_shortcut(ED_SHORTCUT("property_editor/copy_params", TTR("Copy Properties")), OBJECT_COPY_PARAMS);
    p->add_shortcut(ED_SHORTCUT("property_editor/paste_params", TTR("Paste Properties")), OBJECT_PASTE_PARAMS);

    if (is_resource || is_node) {
        p->add_separator();
        p->add_shortcut(ED_SHORTCUT("property_editor/make_subresources_unique", TTR("Make Sub-Resources Unique")), OBJECT_UNIQUE_RESOURCES);
    }

    Vector<MethodInfo> methods;
    p_object->get_method_list(&methods);

    if (!methods.empty()) {

        bool found = false;
        int i = 0;
        for(const MethodInfo &mi : methods) {

            if (mi.flags & METHOD_FLAG_EDITOR) {
                if (!found) {
                    p->add_separator();
                    found = true;
                }
                p->add_item(StringName(StringUtils::capitalize(mi.name)), OBJECT_METHOD_BASE + i);
            }
            i++;
        }
    }
}

void InspectorDock::go_back() {
    _edit_back();
}

void InspectorDock::update_keying() {
    bool valid = false;

    if (AnimationPlayerEditor::singleton->get_track_editor()->has_keying()) {

        EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();
        if (editor_history->get_path_size() >= 1) {

            Object *obj = object_for_entity(editor_history->get_path_object(0));
            if (object_cast<Node>(obj)) {

                valid = true;
            }
        }
    }

    inspector->set_keying(valid);
}

EditorPropertyNameStyle InspectorDock::get_property_name_style() const {
    return property_name_style;
}

InspectorDock::InspectorDock(EditorNode *p_editor, EditorData &p_editor_data) {
    set_name("Inspector");
    set_theme(p_editor->get_gui_base()->get_theme());

    editor = p_editor;
    editor_data = &p_editor_data;
    property_name_style = EditorPropertyNameProcessor::get_default_inspector_style();

    HBoxContainer *general_options_hb = memnew(HBoxContainer);
    add_child(general_options_hb);

    resource_new_button = memnew(ToolButton);
    resource_new_button->set_tooltip(TTR("Create a new resource in memory and edit it."));
    resource_new_button->set_button_icon(get_theme_icon("New", "EditorIcons"));
    general_options_hb->add_child(resource_new_button);
    resource_new_button->connect("pressed",callable_mp(this, &ClassName::_new_resource));
    resource_new_button->set_focus_mode(Control::FOCUS_NONE);

    resource_load_button = memnew(ToolButton);
    resource_load_button->set_tooltip(TTR("Load an existing resource from disk and edit it."));
    resource_load_button->set_button_icon(get_theme_icon("Load", "EditorIcons"));
    general_options_hb->add_child(resource_load_button);
    resource_load_button->connect("pressed",callable_mp(this, &ClassName::_open_resource_selector));
    resource_load_button->set_focus_mode(Control::FOCUS_NONE);

    resource_save_button = memnew(MenuButton);
    resource_save_button->set_tooltip(TTR("Save the currently edited resource."));
    resource_save_button->set_button_icon(get_theme_icon("Save", "EditorIcons"));
    general_options_hb->add_child(resource_save_button);
    resource_save_button->get_popup()->add_item(TTR("Save"), RESOURCE_SAVE);
    resource_save_button->get_popup()->add_item(TTR("Save As..."), RESOURCE_SAVE_AS);
    resource_save_button->get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_menu_option));
    resource_save_button->set_focus_mode(Control::FOCUS_NONE);
    resource_save_button->set_disabled(true);

    resource_extra_button = memnew(MenuButton);
    resource_extra_button->set_button_icon(get_theme_icon("GuiTabMenuHl", "EditorIcons"));
    resource_extra_button->set_tooltip(TTR("Extra resource options."));
    general_options_hb->add_child(resource_extra_button);
    resource_extra_button->connect("about_to_show", callable_mp(this, &InspectorDock::_prepare_resource_extra_popup));
    resource_extra_button->get_popup()->add_icon_shortcut(get_theme_icon("ActionPaste", "EditorIcons"), ED_SHORTCUT("property_editor/paste_resource", TTR("Edit Resource from Clipboard")), RESOURCE_EDIT_CLIPBOARD);
    resource_extra_button->get_popup()->add_icon_shortcut(get_theme_icon("ActionCopy", "EditorIcons"), ED_SHORTCUT("property_editor/copy_resource", TTR("Copy Resource")), RESOURCE_COPY);
    resource_extra_button->get_popup()->set_item_disabled(1, true);
    resource_extra_button->get_popup()->add_separator();
    resource_extra_button->get_popup()->add_shortcut(ED_SHORTCUT("property_editor/unref_resource", TTR("Make Resource Built-In")), RESOURCE_MAKE_BUILT_IN);
    resource_extra_button->get_popup()->set_item_disabled(3, true);
    resource_extra_button->get_popup()->connect("id_pressed", callable_mp(this, &InspectorDock::_menu_option));
    general_options_hb->add_spacer();

    backward_button = memnew(ToolButton);
    general_options_hb->add_child(backward_button);
    backward_button->set_button_icon(get_theme_icon("Back", "EditorIcons"));
    backward_button->set_flat(true);
    backward_button->set_tooltip(TTR("Go to the previous edited object in history."));
    backward_button->set_disabled(true);
    backward_button->connect("pressed",callable_mp(this, &ClassName::_edit_back));

    forward_button = memnew(ToolButton);
    general_options_hb->add_child(forward_button);
    forward_button->set_button_icon(get_theme_icon("Forward", "EditorIcons"));
    forward_button->set_flat(true);
    forward_button->set_tooltip(TTR("Go to the next edited object in history."));
    forward_button->set_disabled(true);
    forward_button->connect("pressed",callable_mp(this, &ClassName::_edit_forward));

    history_menu = memnew(MenuButton);
    history_menu->set_tooltip(TTR("History of recently edited objects."));
    history_menu->set_button_icon(get_theme_icon("History", "EditorIcons"));
    general_options_hb->add_child(history_menu);
    history_menu->connect("about_to_show",callable_mp(this, &ClassName::_prepare_history));
    history_menu->get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_select_history));

    HBoxContainer *subresource_hb = memnew(HBoxContainer);
    add_child(subresource_hb);

    editor_path = memnew(EditorPath(editor->get_editor_history()));
    editor_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    subresource_hb->add_child(editor_path);

    open_docs_button = memnew(Button);
    open_docs_button->set_flat(true);
    open_docs_button->set_disabled(true);
    open_docs_button->set_tooltip(TTR("Open documentation for this object."));
    open_docs_button->set_button_icon(get_theme_icon("HelpSearch", "EditorIcons"));
    open_docs_button->set_shortcut(ED_SHORTCUT("property_editor/open_help", TTR("Open Documentation")));
    subresource_hb->add_child(open_docs_button);
    open_docs_button->connect("pressed", callable_gen(this,[this]() { _menu_option(OBJECT_REQUEST_HELP); }));

    object_menu = memnew(MenuButton);
    object_menu->set_button_icon(get_theme_icon("Tools", "EditorIcons"));
    subresource_hb->add_child(object_menu);
    object_menu->set_tooltip(TTR("Object properties."));
    object_menu->get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_menu_option));

    new_resource_dialog = memnew(CreateDialog);
    editor->get_gui_base()->add_child(new_resource_dialog);
    new_resource_dialog->set_base_type("Resource");
    new_resource_dialog->connect("create",callable_mp(this, &ClassName::_resource_created));

    HBoxContainer *property_tools_hb = memnew(HBoxContainer);
    add_child(property_tools_hb);
    search = memnew(LineEdit);
    search->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    search->set_placeholder(TTR("Filter properties"));
    search->set_right_icon(get_theme_icon("Search", "EditorIcons"));
    search->set_clear_button_enabled(true);
    property_tools_hb->add_child(search);

    object_menu = memnew(MenuButton);
    object_menu->set_button_icon(get_theme_icon("Tools", "EditorIcons"));
    property_tools_hb->add_child(object_menu);
    object_menu->set_tooltip(TTR("Manage object properties."));
    object_menu->get_popup()->connect("about_to_show", callable_mp(this, &ClassName::_prepare_menu));
    object_menu->get_popup()->connect("id_pressed", callable_mp(this, &ClassName::_menu_option));

    warning = memnew(Button);
    add_child(warning);
    warning->set_text(TTR("Changes may be lost!"));
    warning->set_button_icon(get_theme_icon("NodeWarning", "EditorIcons"));
    warning->add_theme_color_override("font_color", get_theme_color("warning_color", "Editor"));
    warning->set_clip_text(true);
    warning->hide();
    warning->connect("pressed",callable_mp(this, &ClassName::_warning_pressed));

    warning_dialog = memnew(AcceptDialog);
    editor->get_gui_base()->add_child(warning_dialog);

    load_resource_dialog = memnew(EditorFileDialog);
    add_child(load_resource_dialog);
    load_resource_dialog->set_current_dir("res://");
    load_resource_dialog->connect("file_selected",callable_mp(this, &ClassName::_resource_file_selected));

    inspector = memnew(EditorInspector);
    add_child(inspector);
    inspector->set_autoclear(true);
    inspector->set_show_categories(true);
    inspector->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    inspector->set_use_doc_hints(true);
    inspector->set_hide_script(false);
    inspector->set_property_name_style(EditorPropertyNameProcessor::get_default_inspector_style());
    inspector->set_use_folding(!EDITOR_GET_T<bool>("interface/inspector/disable_folding"));
    inspector->register_text_enter(search);
    inspector->set_undo_redo(&editor_data->get_undo_redo());

    inspector->set_use_filter(true); // TODO: check me

    inspector->connect("resource_selected",callable_mp(this, &ClassName::_resource_selected));
    inspector->connect("property_keyed",callable_mp(this, &ClassName::_property_keyed));
}

InspectorDock::~InspectorDock() = default;
