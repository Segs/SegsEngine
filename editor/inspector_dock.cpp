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

void InspectorDock::_menu_option(int p_option) {
    switch (p_option) {
        case EXPAND_ALL: {
            _menu_expandall();
        } break;
        case COLLAPSE_ALL: {
            _menu_collapseall();
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

        case RESOURCE_SAVE: {
            _save_resource(false);
        } break;
        case RESOURCE_SAVE_AS: {
            _save_resource(true);
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
    uint32_t current = EditorNode::get_singleton()->get_editor_history()->get_current();
    Object *current_obj = current > 0 ? ObjectDB::get_instance(current) : nullptr;
    RES current_res(object_cast<Resource>(current_obj));

    ERR_FAIL_COND(not current_res);


    if (save_as)
        editor->save_resource_as(current_res);
    else
        editor->save_resource(current_res);
}

void InspectorDock::_unref_resource() const {
    uint32_t current = EditorNode::get_singleton()->get_editor_history()->get_current();
    Object *current_obj = current > 0 ? ObjectDB::get_instance(current) : nullptr;
    RES current_res(object_cast<Resource>(current_obj));

    ERR_FAIL_COND(not current_res);

    current_res->set_path(StringView());
    editor->edit_current();
}

void InspectorDock::_copy_resource() const {
    uint32_t current = EditorNode::get_singleton()->get_editor_history()->get_current();
    Object *current_obj = current > 0 ? ObjectDB::get_instance(current) : nullptr;
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

void InspectorDock::_prepare_history() {
    EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();

    int history_to = MAX(0, editor_history->get_history_len() - 25);

    history_menu->get_popup()->clear();

    Ref<Texture> base_icon = get_icon("Object", "EditorIcons");
    Set<ObjectID> already;
    for (int i = editor_history->get_history_len() - 1; i >= history_to; i--) {

        ObjectID id = editor_history->get_history_obj(i);
        Object *obj = ObjectDB::get_instance(id);
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

void InspectorDock::_select_history(int p_idx) const {
    //push it to the top, it is not correct, but it's more useful
    ObjectID id = EditorNode::get_singleton()->get_editor_history()->get_history_obj(p_idx);
    Object *obj = ObjectDB::get_instance(id);
    if (!obj)
        return;
    editor->push_item(obj);
}

void InspectorDock::_resource_created() const {
    Object *c = new_resource_dialog->instance_selected();

    ERR_FAIL_COND(!c);
    Resource *r = object_cast<Resource>(c);
    ERR_FAIL_COND(!r);

    REF res(r);
    editor->push_item(c);
}

void InspectorDock::_resource_selected(const RES &p_res, const StringName &p_property) const {
    if (not p_res)
        return;

    editor->push_item(p_res.get(), p_property);
}

void InspectorDock::_edit_forward() {
    if (EditorNode::get_singleton()->get_editor_history()->next())
        editor->edit_current();
}
void InspectorDock::_edit_back() {
    EditorHistory *editor_history = EditorNode::get_singleton()->get_editor_history();
    if (current && editor_history->previous() || editor_history->get_path_size() == 1)
        editor->edit_current();
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
            resource_new_button->set_button_icon(get_icon("New", "EditorIcons"));
            resource_load_button->set_button_icon(get_icon("Load", "EditorIcons"));
            resource_save_button->set_button_icon(get_icon("Save", "EditorIcons"));
            backward_button->set_button_icon(get_icon("Back", "EditorIcons"));
            forward_button->set_button_icon(get_icon("Forward", "EditorIcons"));
            history_menu->set_button_icon(get_icon("History", "EditorIcons"));
            object_menu->set_button_icon(get_icon("Tools", "EditorIcons"));
            warning->set_button_icon(get_icon("NodeWarning", "EditorIcons"));
        } break;
    }
}

void InspectorDock::_bind_methods() {
    MethodBinder::bind_method("_menu_option", &InspectorDock::_menu_option);

    MethodBinder::bind_method("update_keying", &InspectorDock::update_keying);
    MethodBinder::bind_method("_property_keyed", &InspectorDock::_property_keyed);
    MethodBinder::bind_method("_transform_keyed", &InspectorDock::_transform_keyed);

    MethodBinder::bind_method("_new_resource", &InspectorDock::_new_resource);
    MethodBinder::bind_method("_resource_file_selected", &InspectorDock::_resource_file_selected);
    MethodBinder::bind_method("_open_resource_selector", &InspectorDock::_open_resource_selector);
    MethodBinder::bind_method("_unref_resource", &InspectorDock::_unref_resource);
    MethodBinder::bind_method("_paste_resource", &InspectorDock::_paste_resource);
    MethodBinder::bind_method("_copy_resource", &InspectorDock::_copy_resource);

    MethodBinder::bind_method("_select_history", &InspectorDock::_select_history);
    MethodBinder::bind_method("_prepare_history", &InspectorDock::_prepare_history);
    MethodBinder::bind_method("_resource_created", &InspectorDock::_resource_created);
    MethodBinder::bind_method("_resource_selected", &InspectorDock::_resource_selected, {DEFVAL("")});
    MethodBinder::bind_method("_menu_collapseall", &InspectorDock::_menu_collapseall);
    MethodBinder::bind_method("_menu_expandall", &InspectorDock::_menu_expandall);
    MethodBinder::bind_method("_warning_pressed", &InspectorDock::_warning_pressed);
    MethodBinder::bind_method("_edit_forward", &InspectorDock::_edit_forward);
    MethodBinder::bind_method("_edit_back", &InspectorDock::_edit_back);

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

    if (!p_object) {
        object_menu->set_disabled(true);
        warning->hide();
        search->set_editable(false);

        editor_path->set_disabled(true);
        editor_path->set_text("");
        editor_path->set_tooltip("");
        editor_path->set_button_icon(Ref<Texture>());

        return;
    }

    bool is_resource = p_object->is_class("Resource");
    bool is_node = p_object->is_class("Node");

    object_menu->set_disabled(false);
    search->set_editable(true);
    editor_path->set_disabled(false);
    resource_save_button->set_disabled(!is_resource);

    PopupMenu *p = object_menu->get_popup();

    p->clear();
    p->add_shortcut(ED_SHORTCUT("property_editor/expand_all", TTR("Expand All Properties")), EXPAND_ALL);
    p->add_shortcut(ED_SHORTCUT("property_editor/collapse_all", TTR("Collapse All Properties")), COLLAPSE_ALL);
    p->add_separator();
    if (is_resource) {
        p->add_item(TTR("Save"), RESOURCE_SAVE);
        p->add_item(TTR("Save As..."), RESOURCE_SAVE_AS);
        p->add_separator();
    }
    p->add_shortcut(ED_SHORTCUT("property_editor/copy_params", TTR("Copy Params")), OBJECT_COPY_PARAMS);
    p->add_shortcut(ED_SHORTCUT("property_editor/paste_params", TTR("Paste Params")), OBJECT_PASTE_PARAMS);
    p->add_separator();

    p->add_shortcut(ED_SHORTCUT("property_editor/paste_resource", TTR("Edit Resource Clipboard")), RESOURCE_EDIT_CLIPBOARD);
    if (is_resource) {
        p->add_shortcut(ED_SHORTCUT("property_editor/copy_resource", TTR("Copy Resource")), RESOURCE_COPY);
        p->add_shortcut(ED_SHORTCUT("property_editor/unref_resource", TTR("Make Built-In")), RESOURCE_MAKE_BUILT_IN);
    }

    if (is_resource || is_node) {
        p->add_separator();
        p->add_shortcut(ED_SHORTCUT("property_editor/make_subresources_unique", TTR("Make Sub-Resources Unique")), OBJECT_UNIQUE_RESOURCES);
        p->add_separator();
        p->add_icon_shortcut(get_icon("HelpSearch", "EditorIcons"), ED_SHORTCUT("property_editor/open_help", TTR("Open in Help")), OBJECT_REQUEST_HELP);
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

            Object *obj = ObjectDB::get_instance(editor_history->get_path_object(0));
            if (object_cast<Node>(obj)) {

                valid = true;
            }
        }
    }

    inspector->set_keying(valid);
}

InspectorDock::InspectorDock(EditorNode *p_editor, EditorData &p_editor_data) {
    set_name("Inspector");
    set_theme(p_editor->get_gui_base()->get_theme());

    editor = p_editor;
    editor_data = &p_editor_data;

    HBoxContainer *general_options_hb = memnew(HBoxContainer);
    add_child(general_options_hb);

    resource_new_button = memnew(ToolButton);
    resource_new_button->set_tooltip(TTR("Create a new resource in memory and edit it."));
    resource_new_button->set_button_icon(get_icon("New", "EditorIcons"));
    general_options_hb->add_child(resource_new_button);
    resource_new_button->connect("pressed", this, "_new_resource");
    resource_new_button->set_focus_mode(Control::FOCUS_NONE);

    resource_load_button = memnew(ToolButton);
    resource_load_button->set_tooltip(TTR("Load an existing resource from disk and edit it."));
    resource_load_button->set_button_icon(get_icon("Load", "EditorIcons"));
    general_options_hb->add_child(resource_load_button);
    resource_load_button->connect("pressed", this, "_open_resource_selector");
    resource_load_button->set_focus_mode(Control::FOCUS_NONE);

    resource_save_button = memnew(MenuButton);
    resource_save_button->set_tooltip(TTR("Save the currently edited resource."));
    resource_save_button->set_button_icon(get_icon("Save", "EditorIcons"));
    general_options_hb->add_child(resource_save_button);
    resource_save_button->get_popup()->add_item(TTR("Save"), RESOURCE_SAVE);
    resource_save_button->get_popup()->add_item(TTR("Save As..."), RESOURCE_SAVE_AS);
    resource_save_button->get_popup()->connect("id_pressed", this, "_menu_option");
    resource_save_button->set_focus_mode(Control::FOCUS_NONE);
    resource_save_button->set_disabled(true);

    general_options_hb->add_spacer();

    backward_button = memnew(ToolButton);
    general_options_hb->add_child(backward_button);
    backward_button->set_button_icon(get_icon("Back", "EditorIcons"));
    backward_button->set_flat(true);
    backward_button->set_tooltip(TTR("Go to the previous edited object in history."));
    backward_button->set_disabled(true);
    backward_button->connect("pressed", this, "_edit_back");

    forward_button = memnew(ToolButton);
    general_options_hb->add_child(forward_button);
    forward_button->set_button_icon(get_icon("Forward", "EditorIcons"));
    forward_button->set_flat(true);
    forward_button->set_tooltip(TTR("Go to the next edited object in history."));
    forward_button->set_disabled(true);
    forward_button->connect("pressed", this, "_edit_forward");

    history_menu = memnew(MenuButton);
    history_menu->set_tooltip(TTR("History of recently edited objects."));
    history_menu->set_button_icon(get_icon("History", "EditorIcons"));
    general_options_hb->add_child(history_menu);
    history_menu->connect("about_to_show", this, "_prepare_history");
    history_menu->get_popup()->connect("id_pressed", this, "_select_history");

    HBoxContainer *node_info_hb = memnew(HBoxContainer);
    add_child(node_info_hb);

    editor_path = memnew(EditorPath(editor->get_editor_history()));
    editor_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    node_info_hb->add_child(editor_path);

    object_menu = memnew(MenuButton);
    object_menu->set_button_icon(get_icon("Tools", "EditorIcons"));
    node_info_hb->add_child(object_menu);
    object_menu->set_tooltip(TTR("Object properties."));
    object_menu->get_popup()->connect("id_pressed", this, "_menu_option");

    new_resource_dialog = memnew(CreateDialog);
    editor->get_gui_base()->add_child(new_resource_dialog);
    new_resource_dialog->set_base_type("Resource");
    new_resource_dialog->connect("create", this, "_resource_created");

    search = memnew(LineEdit);
    search->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    search->set_placeholder(TTR("Filter properties"));
    search->set_right_icon(get_icon("Search", "EditorIcons"));
    search->set_clear_button_enabled(true);
    add_child(search);

    warning = memnew(Button);
    add_child(warning);
    warning->set_text(TTR("Changes may be lost!"));
    warning->set_button_icon(get_icon("NodeWarning", "EditorIcons"));
    warning->set_clip_text(true);
    warning->hide();
    warning->connect("pressed", this, "_warning_pressed");

    warning_dialog = memnew(AcceptDialog);
    editor->get_gui_base()->add_child(warning_dialog);

    load_resource_dialog = memnew(EditorFileDialog);
    add_child(load_resource_dialog);
    load_resource_dialog->set_current_dir("res://");
    load_resource_dialog->connect("file_selected", this, "_resource_file_selected");

    inspector = memnew(EditorInspector);
    add_child(inspector);
    inspector->set_autoclear(true);
    inspector->set_show_categories(true);
    inspector->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    inspector->set_use_doc_hints(true);
    inspector->set_hide_script(false);
    inspector->set_enable_capitalize_paths(bool(EDITOR_GET("interface/inspector/capitalize_properties")));
    inspector->set_use_folding(!bool(EDITOR_GET("interface/inspector/disable_folding")));
    inspector->register_text_enter(search);
    inspector->set_undo_redo(&editor_data->get_undo_redo());

    inspector->set_use_filter(true); // TODO: check me

    inspector->connect("resource_selected", this, "_resource_selected");
    inspector->connect("property_keyed", this, "_property_keyed");
}

InspectorDock::~InspectorDock() = default;
