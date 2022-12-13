/*************************************************************************/
/*  script_editor_plugin.cpp                                             */
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

#include "script_editor_plugin.h"

#include <utility>

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/os/file_access.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/version.h"
#include "core/string_formatter.h"
#include "core/string_utils.inl"
#include "core/version.h"
#include "core/translation_helpers.h"
#include "core/container_tools.h"
#include "core/resource/resource_tools.h"
#include "core/resource/resource_manager.h"
#include "core/resource/resource_manager_tooling.h"
#include "editor/inspector_dock.h"
#include "editor/scene_tree_dock.h"
#include "editor/editor_help.h"
#include "editor/editor_help_search.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/filesystem_dock.h"
#include "editor/find_in_files.h"
#include "editor/node_dock.h"
#include "editor/plugins/shader_editor_plugin.h"
#include "editor/script_editor_debugger.h"
#include "editor/editor_run_script.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/scene_string_names.h"
#include "scene/resources/style_box.h"
#include "scene/gui/item_list.h"
#include "script_text_editor.h"
#include "text_editor.h"

#include "EASTL/sort.h"

IMPL_GDCLASS(ScriptEditorQuickOpen)
IMPL_GDCLASS(ScriptEditorBase)
IMPL_GDCLASS(ScriptEditor)
IMPL_GDCLASS(ScriptEditorPlugin)

/*** SCRIPT EDITOR ****/

void ScriptEditorBase::_bind_methods() {

    ADD_SIGNAL(MethodInfo("name_changed"));
    ADD_SIGNAL(MethodInfo("edited_script_changed"));
    ADD_SIGNAL(MethodInfo("request_help", PropertyInfo(VariantType::STRING, "topic")));
    ADD_SIGNAL(MethodInfo("request_open_script_at_line", PropertyInfo(VariantType::OBJECT, "script"), PropertyInfo(VariantType::INT, "line")));
    ADD_SIGNAL(MethodInfo("request_save_history"));
    ADD_SIGNAL(MethodInfo("go_to_help", PropertyInfo(VariantType::STRING, "what")));
    // TODO: This signal is no use for VisualScript.
    ADD_SIGNAL(MethodInfo("search_in_files_requested", PropertyInfo(VariantType::STRING, "text")));
    ADD_SIGNAL(MethodInfo("replace_in_files_requested", PropertyInfo(VariantType::STRING, "text")));
}

static bool _is_built_in_script(Script *p_script) {
    auto path = p_script->get_path();

    return StringUtils::contains(path,"::");
}

class EditorScriptCodeCompletionCache : public ScriptCodeCompletionCache {

    struct Cache {
        uint64_t time_loaded;
        RES cache;
    };

    Map<String, Cache> cached;

public:
    uint64_t max_time_cache;
    size_t max_cache_size;

    void cleanup() {

        Vector<Map<String, Cache>::iterator> to_clean;

        Map<String, Cache>::iterator I = cached.begin();
        for (;I!=cached.end(); ++I) {
            if (OS::get_singleton()->get_ticks_msec() - I->second.time_loaded > max_time_cache) {
                to_clean.push_back(I);
            }
        }

        for(Map<String, Cache>::iterator i : to_clean) {
            cached.erase(i);
        }
    }

    RES get_cached_resource(StringView p_path) override {

        Map<String, Cache>::iterator E = cached.find_as(p_path);
        if (E==cached.end()) {

            Cache c;
            c.cache = gResourceManager().load(p_path);
            E = cached.emplace(String(p_path), c).first;
        }

        E->second.time_loaded = OS::get_singleton()->get_ticks_msec();

        if (cached.size() > max_cache_size) {
            uint64_t older;
            Map<String, Cache>::iterator O = cached.begin();
            older = O->second.time_loaded;
            Map<String, Cache>::iterator I = O;
            for(;I!=cached.end(); ++I) {
                if (I->second.time_loaded < older) {
                    older = I->second.time_loaded;
                    O = I;
                }
            }

            if (O != E) { //should never happen..
                cached.erase(O);
            }
        }

        return E->second.cache;
    }

    EditorScriptCodeCompletionCache() {

        max_cache_size = 128;
        max_time_cache = 5 * 60 * 1000; //minutes, five
    }

    ~EditorScriptCodeCompletionCache() override = default;
};

void ScriptEditorQuickOpen::popup_dialog(const Vector<String> &p_functions, bool p_dontclear) {

    popup_centered_ratio(0.6f);
    if (p_dontclear)
        search_box->select_all();
    else
        search_box->clear();
    search_box->grab_focus();
    functions = p_functions;
    _update_search();
}

void ScriptEditorQuickOpen::_text_changed(StringView p_newtext) {

    _update_search();
}

void ScriptEditorQuickOpen::_sbox_input(const Ref<InputEvent> &p_ie) {

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_ie);

    if (k && (k->get_keycode() == KEY_UP || k->get_keycode() == KEY_DOWN || k->get_keycode() == KEY_PAGEUP ||
                                k->get_keycode() == KEY_PAGEDOWN)) {

        search_options->call_va("_gui_input", k);
        search_box->accept_event();
    }
}

void ScriptEditorQuickOpen::_update_search() {

    search_options->clear();
    TreeItem *root = search_options->create_item();

    for (int i = 0; i < functions.size(); i++) {

        String file = functions[i];
        if (search_box->get_text_ui().isEmpty() || StringUtils::findn(file,search_box->get_text()) != String::npos) {

            TreeItem *ti = search_options->create_item(root);
            ti->set_text_utf8(0, file);
            if (root->get_children() == ti)
                ti->select(0);
        }
    }

    get_ok()->set_disabled(root->get_children() == nullptr);
}

void ScriptEditorQuickOpen::_confirmed() {

    TreeItem *ti = search_options->get_selected();
    if (!ti)
        return;
    int line = StringUtils::to_int(StringUtils::get_slice(ti->get_text(0),':', 1));

    emit_signal("goto_line", line - 1);
    hide();
}

void ScriptEditorQuickOpen::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            connect("confirmed",callable_mp(this, &ClassName::_confirmed));

            search_box->set_clear_button_enabled(true);
            [[fallthrough]];
        }
        case NOTIFICATION_THEME_CHANGED: {
            search_box->set_right_icon(get_theme_icon("Search", "EditorIcons"));
        } break;
        case NOTIFICATION_EXIT_TREE: {
            disconnect("confirmed",callable_mp(this, &ClassName::_confirmed));
        } break;
    }
}

void ScriptEditorQuickOpen::_bind_methods() {

    ADD_SIGNAL(MethodInfo("goto_line", PropertyInfo(VariantType::INT, "line")));
}

ScriptEditorQuickOpen::ScriptEditorQuickOpen() {

    VBoxContainer *vbc = memnew(VBoxContainer);
    add_child(vbc);
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
    search_options->set_hide_root(true);
    search_options->set_hide_folding(true);
    search_options->add_constant_override("draw_guides", 1);
}

/////////////////////////////////

ScriptEditor *ScriptEditor::script_editor = nullptr;

/*** SCRIPT EDITOR ******/

String ScriptEditor::_get_debug_tooltip(StringView p_text, Node *_se) {

    String val = debugger->get_var_value(p_text);
    if (!val.empty()) {
        return String(p_text) + ": " + val;
    } else {

        return String();
    }
}

void ScriptEditor::_breaked(bool p_breaked, bool p_can_debug)
{

    if (EditorSettings::get_singleton()->getT<bool>("text_editor/external/use_external_editor")) {
        return;
    }

    PopupMenu *popup_menu = debug_menu->get_popup();
    popup_menu->set_item_disabled(popup_menu->get_item_index(DEBUG_NEXT), !(p_breaked && p_can_debug));
    popup_menu->set_item_disabled(popup_menu->get_item_index(DEBUG_STEP), !(p_breaked && p_can_debug));
    popup_menu->set_item_disabled(popup_menu->get_item_index(DEBUG_BREAK), p_breaked);
    popup_menu->set_item_disabled(popup_menu->get_item_index(DEBUG_CONTINUE), !p_breaked);

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se) {
            continue;
        }

        se->set_debugger_active(p_breaked);
    }
}

void ScriptEditor::_show_debugger(bool p_show) {
}

void ScriptEditor::_script_created(Ref<Script> p_script) {
    editor->push_item(p_script.operator->());
}

void ScriptEditor::_goto_script_line2(int p_line) {

    ScriptEditorBase *current = _get_current_editor();
    if (current)
        current->goto_line(p_line);
}

void ScriptEditor::_goto_script_line(REF p_script, int p_line) {

    Ref<Script> script(dynamic_ref_cast<Script>(p_script));
    if (script && (script->has_source_code() || PathUtils::is_resource_file(script->get_path()))) {
        if (edit(script, p_line, 0)) {
            editor->push_item(p_script.get());

            ScriptEditorBase *current = _get_current_editor();
            if (ScriptTextEditor *script_text_editor = object_cast<ScriptTextEditor>(current)) {
                script_text_editor->goto_line_centered(p_line);
            } else if (current) {
                current->goto_line(p_line, true);
            }
        }
    }
}

void ScriptEditor::_set_execution(REF p_script, int p_line) {
    Ref<Script> script(dynamic_ref_cast<Script>(p_script));
    if (script && (script->has_source_code() || PathUtils::is_resource_file(script->get_path()))) {
        for (int i = 0; i < tab_container->get_child_count(); i++) {

            ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
            if (!se)
                continue;

            if ((script != nullptr && se->get_edited_resource() == script) || se->get_edited_resource()->get_path() == script->get_path()) {
                se->set_executing_line(p_line);
            }
        }
    }
}

void ScriptEditor::_clear_execution(REF p_script) {
    Ref<Script> script(dynamic_ref_cast<Script>(p_script));
    if (script && (script->has_source_code() || PathUtils::is_resource_file(script->get_path()))) {
        for (int i = 0; i < tab_container->get_child_count(); i++) {

            ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
            if (!se)
                continue;

            if ((script != nullptr && se->get_edited_resource() == script) || se->get_edited_resource()->get_path() == script->get_path()) {
                se->clear_executing_line();
            }
        }
    }
}

ScriptEditorBase *ScriptEditor::_get_current_editor() const {

    int selected = tab_container->get_current_tab();
    if (selected < 0 || selected >= tab_container->get_child_count())
        return nullptr;

    return object_cast<ScriptEditorBase>(tab_container->get_child(selected));
}

void ScriptEditor::_update_history_arrows() {

    script_back->set_disabled(history_pos <= 0);
    script_forward->set_disabled(history_pos >= history.size() - 1);
}

void ScriptEditor::_save_history() {

    if (history_pos >= 0 && history_pos < history.size() && history[history_pos].control == tab_container->get_current_tab_control()) {

        Node *n = tab_container->get_current_tab_control();

        if (object_cast<ScriptEditorBase>(n)) {

            history[history_pos].state = object_cast<ScriptEditorBase>(n)->get_edit_state();
        }
        if (object_cast<EditorHelp>(n)) {

            history[history_pos].state = object_cast<EditorHelp>(n)->get_scroll();
        }
    }

    history.resize(history_pos + 1);
    ScriptHistory sh;
    sh.control = tab_container->get_current_tab_control();
    sh.state = Variant();

    history.push_back(sh);
    history_pos++;

    _update_history_arrows();
}

void ScriptEditor::_go_to_tab(int p_idx) {

    ScriptEditorBase *current = _get_current_editor();
    if (current) {
        if (current->is_unsaved()) {

            current->apply_code();
        }
    }

    Control *c = object_cast<Control>(tab_container->get_child(p_idx));
    if (!c)
        return;

    if (history_pos >= 0 && history_pos < history.size() && history[history_pos].control == tab_container->get_current_tab_control()) {

        Node *n = tab_container->get_current_tab_control();

        if (object_cast<ScriptEditorBase>(n)) {

            history[history_pos].state = object_cast<ScriptEditorBase>(n)->get_edit_state();
        }
        if (object_cast<EditorHelp>(n)) {

            history[history_pos].state = object_cast<EditorHelp>(n)->get_scroll();
        }
    }

    history.resize(history_pos + 1);
    ScriptHistory sh;
    sh.control = c;
    sh.state = Variant();

    history.push_back(sh);
    history_pos++;

    tab_container->set_current_tab(p_idx);

    c = tab_container->get_current_tab_control();
    auto *seb = object_cast<ScriptEditorBase>(c);
    if (seb) {

        script_name_label->set_text(StringName(seb->get_name()));
        script_icon->set_texture(seb->get_theme_icon());
        if (is_visible_in_tree())
            seb->ensure_focus();

        Ref<Script> script = dynamic_ref_cast<Script>(seb->get_edited_resource());
        if (script != nullptr) {
            notify_script_changed(script);
        }

        seb->validate();
    }
    if (object_cast<EditorHelp>(c)) {

        script_name_label->set_text(object_cast<EditorHelp>(c)->get_class_name());
        script_icon->set_texture(get_theme_icon("Help", "EditorIcons"));
        if (is_visible_in_tree())
            object_cast<EditorHelp>(c)->set_focused();
    }

    c->set_meta("__editor_pass", ++edit_pass);
    _update_history_arrows();
    _update_script_colors();
    _update_members_overview();
    _update_help_overview();
    _update_selected_editor_menu();
    _update_members_overview_visibility();
    _update_help_overview_visibility();
}

void ScriptEditor::_add_recent_script(StringView p_path) {

    if (p_path.empty()) {
        return;
    }

    Array rc = EditorSettings::get_singleton()->get_project_metadataT("recent_files", "scripts", Array());
    if (rc.find(p_path) != -1) {
        rc.erase(p_path);
    }
    rc.push_front(p_path);
    if (rc.size() > 10) {
        rc.resize(10);
    }

    EditorSettings::get_singleton()->set_project_metadata("recent_files", "scripts", rc);
    _update_recent_scripts();
}

void ScriptEditor::_update_recent_scripts() {

    Array rc = EditorSettings::get_singleton()->get_project_metadataT("recent_files", "scripts", Array());
    recent_scripts->clear();

    String path;
    for (int i = 0; i < rc.size(); i++) {

        path = rc[i].as<String>();
        recent_scripts->add_item(StringName(path.replaced("res://", "")));
    }

    recent_scripts->add_separator();
    recent_scripts->add_shortcut(ED_SHORTCUT("script_editor/clear_recent", TTR("Clear Recent Files")));
    recent_scripts->set_item_disabled(recent_scripts->get_item_id(recent_scripts->get_item_count() - 1), rc.empty());

    recent_scripts->set_as_minsize();
}

void ScriptEditor::_open_recent_script(int p_idx) {

    // clear button
    if (p_idx == recent_scripts->get_item_count() - 1) {
        EditorSettings::get_singleton()->set_project_metadata("recent_files", "scripts", Array());
        call_deferred([this]() { _update_recent_scripts(); });
        return;
    }

    Array rc = EditorSettings::get_singleton()->get_project_metadataT("recent_files", "scripts", Array());
    ERR_FAIL_INDEX(p_idx, rc.size());

    String path = rc[p_idx].as<String>();
    // if its not on disk its a help file or deleted
    if (FileAccess::exists(path)) {
        Vector<String> extensions;
        gResourceManager().get_recognized_extensions_for_type("Script", extensions);

        if (extensions.contains(String(PathUtils::get_extension(path)))) {
            Ref<Script> script = dynamic_ref_cast<Script>(gResourceManager().load(path));
            if (script) {
                edit(script, true);
                return;
            }
        }

        Error err;
        Ref<TextFile> text_file = _load_text_file(path, &err);
        if (text_file) {
            edit(text_file, true);
            return;
        }
        // if it's a path then it's most likely a deleted file not help
    } else if (StringUtils::contains(path,"::")) {
        // built-in script
        StringView res_path = StringUtils::get_slice(path,"::", 0);
        if (gResourceManager().get_resource_type(res_path) == "PackedScene") {
            if (!EditorNode::get_singleton()->is_scene_open(res_path)) {
                EditorNode::get_singleton()->load_scene(res_path);
            }
        } else {
            EditorNode::get_singleton()->load_resource(res_path);
        }
        Ref<Script> script = dynamic_ref_cast<Script>(gResourceManager().load(path));
        if (script) {
            edit(script, true);
            return;
        }
    } else if (!PathUtils::is_resource_file(path)) {
        _help_class_open(path);
        return;
    }

    rc.remove(p_idx);
    EditorSettings::get_singleton()->set_project_metadata("recent_files", "scripts", rc);
    _update_recent_scripts();
    _show_error_dialog(path);
}

void ScriptEditor::_show_error_dialog(StringView p_path) {

    UIString translated(TTR("Can't open '%.*s'. The file could have been moved or deleted."));
    error_dialog->set_text(StringName(FormatVE(StringUtils::to_utf8(translated).data(), p_path.size(),p_path.data())));
    error_dialog->popup_centered_minsize();
}

void ScriptEditor::_close_tab(int p_idx, bool p_save, bool p_history_back) {

    int selected = p_idx;
    if (selected < 0 || selected >= tab_container->get_child_count())
        return;

    Node *tselected = tab_container->get_child(selected);

    ScriptEditorBase *current = object_cast<ScriptEditorBase>(tselected);
    if (current) {
        Ref<Script> script = dynamic_ref_cast<Script>(current->get_edited_resource());

        if (p_save) {
            // Do not try to save internal scripts
            if (!script || !(script->get_path().empty() || script->get_path().contains("local://") || script->get_path().contains("::"))) {
                save_current_script();
            }
        }

        if (script != nullptr) {
            previous_scripts.push_back(script->get_path());
            notify_script_close(script);
        }
    }

    // roll back to previous tab
    if (p_history_back) {
        _history_back();
    }

    //remove from history
    history.resize(history_pos + 1);

    for (int i = 0; i < history.size(); i++) {
        if (history[i].control == tselected) {
            history.erase_at(i);
            i--;
            history_pos--;
        }
    }

    if (history_pos >= history.size()) {
        history_pos = history.size() - 1;
    }

    int idx = tab_container->get_current_tab();
    if (current) {
        current->clear_edit_menu();
    }
    memdelete(tselected);
    if (idx >= tab_container->get_child_count())
        idx = tab_container->get_child_count() - 1;
    if (idx >= 0) {

        if (history_pos >= 0) {
            idx = history[history_pos].control->get_index();
        }
        tab_container->set_current_tab(idx);
    } else {
        _update_selected_editor_menu();
    }

    _update_history_arrows();

    _update_script_names();
    _update_members_overview_visibility();
    _update_help_overview_visibility();
    _save_layout();
}

void ScriptEditor::_close_current_tab(bool p_save) {

    _close_tab(tab_container->get_current_tab(),p_save);
}

void ScriptEditor::_close_discard_current_tab(StringView /*p_str*/) {
    _close_tab(tab_container->get_current_tab(), false);
    erase_tab_confirm->hide();
}

void ScriptEditor::_close_docs_tab() {

    int child_count = tab_container->get_child_count();
    for (int i = child_count - 1; i >= 0; i--) {

        EditorHelp *se = object_cast<EditorHelp>(tab_container->get_child(i));

        if (se) {
            _close_tab(i, true, false);
        }
    }
}

void ScriptEditor::_copy_script_path() {
    ScriptEditorBase *se = _get_current_editor();
    if(!se)
        return;

    RES script(se->get_edited_resource());
    OS::get_singleton()->set_clipboard(script->get_path());
}

void ScriptEditor::_close_other_tabs() {

    int current_idx = tab_container->get_current_tab();
    for (int i = tab_container->get_child_count() - 1; i >= 0; i--) {
        if (i != current_idx) {
            script_close_queue.push_back(i);
        }
    }
    _queue_close_tabs();
        }


void ScriptEditor::_close_all_tabs() {
    for (int i = tab_container->get_child_count() - 1; i >= 0; i--) {
        script_close_queue.push_back(i);
            }
    _queue_close_tabs();
        }

void ScriptEditor::_ask_close_current_unsaved_tab(ScriptEditorBase *current) {
    erase_tab_confirm->set_text(TTR("Close and save changes?") + "\n\"" + current->get_name() + "\"");
    erase_tab_confirm->popup_centered_minsize();
}


void ScriptEditor::_queue_close_tabs() {
    while (!script_close_queue.empty()) {
        int idx = script_close_queue.front();
        script_close_queue.pop_front();

        tab_container->set_current_tab(idx);
        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(idx));

        if (se) {

            // Maybe there are unsaved changes.
            if (se->is_unsaved()) {
                _ask_close_current_unsaved_tab(se);
                erase_tab_confirm->connect(SceneStringNames::visibility_changed, callable_mp(this, &ScriptEditor::_queue_close_tabs), ObjectNS::CONNECT_ONESHOT | ObjectNS::CONNECT_QUEUED);
                break;
            }
        }

        _close_current_tab(false);
    }
}

void ScriptEditor::_resave_scripts(StringView /*p_str*/) {

    apply_scripts();

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se)
            continue;

        RES script(se->get_edited_resource());

        if (script->get_path().empty() || PathUtils::is_internal_path(script->get_path()) )
            continue; //internal script, who cares

        if (trim_trailing_whitespace_on_save) {
            se->trim_trailing_whitespace();
        }

        se->insert_final_newline();

        if (convert_indent_on_save) {
            if (use_space_indentation) {
                se->convert_indent_to_spaces();
            } else {
                se->convert_indent_to_tabs();
            }
        }

        Ref<TextFile> text_file = dynamic_ref_cast<TextFile>(script);
        if (text_file != nullptr) {
            se->apply_code();
            _save_text_file(text_file, text_file->get_path());
            break;
        } else {
            editor->save_resource(script);
        }
        se->tag_saved_version();
    }

    disk_changed->hide();
}

void ScriptEditor::reload_scripts() {

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se) {

            continue;
        }

        RES edited_res(se->get_edited_resource());

        if (edited_res->get_path().empty() || PathUtils::is_internal_path(edited_res->get_path()) ) {

            continue; //internal script, who cares
        }

        uint64_t last_date = ResourceTooling::get_last_modified_time(edited_res.get());
        uint64_t date = FileAccess::get_modified_time(edited_res->get_path());

        if (last_date == date) {
            continue;
        }

        Ref<Script> script = dynamic_ref_cast<Script>(edited_res);
        if (script != nullptr) {
            Error r_error;
            FileAccessRef src_file(FileAccess::open(script->get_path(),FileAccess::READ,&r_error));
            ERR_CONTINUE(r_error!=OK);
            script->set_source_code(src_file->get_as_utf8_string());
            ResourceTooling::set_last_modified_time(script.get(),FileAccess::get_modified_time(script->get_path()));
            script->reload(); //update_exports() ???

            /* Original code was trying to load a new script instance instead, and ran afoul of resource cache.
                Ref<Script> rel_script = dynamic_ref_cast<Script>(gResourceManager().load(script->get_path(), script->get_class(), true));
                ERR_CONTINUE(not rel_script);
                script->set_source_code(String(rel_script->get_source_code()));
                script->set_last_modified_time(rel_script->get_last_modified_time());
                script->reload();
             */
        }

        Ref<TextFile> text_file = dynamic_ref_cast<TextFile>(edited_res);
        if (text_file != nullptr) {
            Error err;
            Ref<TextFile> rel_text_file = _load_text_file(text_file->get_path(), &err);
            ERR_CONTINUE(not rel_text_file);
            text_file->set_text(rel_text_file->get_text());
            ResourceTooling::set_last_modified_time_from_another(text_file.get(), rel_text_file.get());
        }
        se->reload_text();
    }

    disk_changed->hide();
    _update_script_names();
}

void ScriptEditor::_res_saved_callback(const Ref<Resource> &p_res) {

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se) {

            continue;
        }

        RES script(se->get_edited_resource());


        if (script == p_res) {

            se->tag_saved_version();
        }
    }

    _update_script_names();
    _trigger_live_script_reload();
}

void ScriptEditor::_scene_saved_callback(const String &p_path) {
    // If scene was saved, mark all built-in scripts from that scene as saved.
    for (int i = 0; i < tab_container->get_child_count(); i++) {
        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se) {
            continue;
        }

        RES edited_res = se->get_edited_resource();
        const String &edited_res_path(edited_res->get_path());
        if (!edited_res_path.empty() && !edited_res_path.contains("::")) {
            continue; // External script, who cares.
        }
        StringView actual_res_path(StringUtils::get_slice(edited_res_path,"::",0));
        if (actual_res_path == p_path) {
            se->tag_saved_version();
        }

        Ref<Script> scr = dynamic_ref_cast<Script>(edited_res);
        if (scr && scr->is_tool()) {
            scr->reload(true);
        }
    }
}


void ScriptEditor::_trigger_live_script_reload() {
    if (!pending_auto_reload && auto_reload_running_scripts) {
        call_deferred([this]() { _live_auto_reload_running_scripts(); });
        pending_auto_reload = true;
    }
}

void ScriptEditor::_live_auto_reload_running_scripts() {
    pending_auto_reload = false;
    debugger->reload_scripts();
}

bool ScriptEditor::_test_script_times_on_disk(const RES& p_for_script) {

    disk_changed_list->clear();
    TreeItem *r = disk_changed_list->create_item();
    disk_changed_list->set_hide_root(true);

    bool need_ask = false;
    bool need_reload = false;
    bool use_autoreload = EDITOR_DEF_T("text_editor/files/auto_reload_scripts_on_external_change", false);

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (se) {

            RES edited_res(se->get_edited_resource());
            if (p_for_script && edited_res && p_for_script != edited_res)
                continue;

            if (edited_res->get_path().empty() || PathUtils::is_internal_path(edited_res->get_path()))
                continue; //internal script, who cares

            uint64_t last_date = ResourceTooling::get_last_modified_time(edited_res.get());
            uint64_t date = FileAccess::get_modified_time(edited_res->get_path());

            if (last_date != date) {

                TreeItem *ti = disk_changed_list->create_item(r);
                ti->set_text_utf8(0, PathUtils::get_file(edited_res->get_path()));

                if (!use_autoreload || se->is_unsaved()) {
                    need_ask = true;
                }
                need_reload = true;
            }
        }
    }

    if (need_reload) {
        if (!need_ask) {
            script_editor->reload_scripts();
            need_reload = false;
        } else {
            disk_changed->call_deferred([dc=disk_changed] { dc->popup_centered_ratio(0.5f);});
        }
    }

    return need_reload;
}

void ScriptEditor::_file_dialog_action(StringView p_file) {

    switch (file_dialog_option) {
        case FILE_NEW_TEXTFILE: {
            Error err;
            FileAccess *file = FileAccess::open(p_file, FileAccess::WRITE, &err);
            if (err) {
                memdelete(file);
                editor->show_warning(TTR("Error writing TextFile:") + "\n" + p_file, TTR("Error!"));
                break;
            }
            file->close();
            memdelete(file);
            [[fallthrough]];
        }
        case ACT_FILE_OPEN: {

            Vector<String> extensions;
            gResourceManager().get_recognized_extensions_for_type("Script", extensions);
            if (ContainerUtils::contains(extensions,PathUtils::get_extension(p_file))) {
                Ref<Script> scr = dynamic_ref_cast<Script>(gResourceManager().load(p_file));
                if (not scr) {
                    editor->show_warning(TTR("Could not load file at:") + "\n\n" + p_file, TTR("Error!"));
                    file_dialog_option = -1;
                    return;
                }

                edit(scr);
                file_dialog_option = -1;
                return;
            }

            Error error;
            Ref<TextFile> text_file = _load_text_file(p_file, &error);
            if (error != OK) {
                editor->show_warning(TTR("Could not load file at:") + "\n\n" + p_file, TTR("Error!"));
            }

            if (text_file) {
                edit(text_file);
                file_dialog_option = -1;
                return;
            }
        } break;
        case FILE_SAVE_AS: {
            ScriptEditorBase *current = _get_current_editor();
            if (current) {
                RES resource = current->get_edited_resource();

                String path = ProjectSettings::get_singleton()->localize_path(p_file);
                Error err = _save_text_file(dynamic_ref_cast<TextFile>(resource), path);

                if (err != OK) {
                    editor->show_accept(TTR("Error saving file!"), TTR("OK"));
                    return;
                }

                resource->set_path(path);
                _update_script_names();
            }
        } break;
        case THEME_SAVE_AS: {
            if (!EditorSettings::get_singleton()->save_text_editor_theme_as(p_file)) {
                editor->show_warning(TTR("Error while saving theme."), TTR("Error Saving"));
            }
        } break;
        case THEME_IMPORT: {
            if (!EditorSettings::get_singleton()->import_text_editor_theme(p_file)) {
                editor->show_warning(TTR("Error importing theme."), TTR("Error Importing"));
            }
        } break;
    }
    file_dialog_option = -1;
}

Ref<Script> ScriptEditor::_get_current_script() {

    ScriptEditorBase *current = _get_current_editor();

    if (current) {
        Ref<Script> script = dynamic_ref_cast<Script>(current->get_edited_resource());
        return Ref<Script>(script);
    } else {
        return Ref<Script>();
    }
}

Array ScriptEditor::_get_open_scripts() const {

    Array ret;
    Vector<Ref<Script> > scripts = get_open_scripts();
    int scrits_amount = scripts.size();
    for (int idx_script = 0; idx_script < scrits_amount; idx_script++) {
        ret.push_back(scripts[idx_script]);
    }
    return ret;
}

bool ScriptEditor::toggle_scripts_panel() {
    list_split->set_visible(!list_split->is_visible());
    EditorSettings::get_singleton()->set_project_metadata("scripts_panel", "show_scripts_panel", list_split->is_visible());
    return list_split->is_visible();
}

bool ScriptEditor::is_scripts_panel_toggled() {
    return list_split->is_visible();
}

void ScriptEditor::_menu_option(int p_option) {
    ScriptEditorBase *current = _get_current_editor();

    switch (p_option) {
        case FILE_NEW: {
            script_create_dialog->config("Node", "new_script", false, false);
            script_create_dialog->popup_centered();
        } break;
        case FILE_NEW_TEXTFILE: {
            file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
            file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
            file_dialog_option = FILE_NEW_TEXTFILE;

            file_dialog->clear_filters();
            file_dialog->popup_centered_ratio();
            file_dialog->set_title(TTR("New Text File..."));
        } break;
        case ACT_FILE_OPEN: {
            file_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
            file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
            file_dialog_option = ACT_FILE_OPEN;

            Vector<String> extensions;
            gResourceManager().get_recognized_extensions_for_type("Script", extensions);
            file_dialog->clear_filters();
            for (const String &ext : extensions) {
                file_dialog->add_filter("*." + ext + " ; " + StringUtils::to_upper(ext));
            }

            file_dialog->popup_centered_ratio();
            file_dialog->set_title(TTR("Open File"));
            return;
        } break;
        case FILE_REOPEN_CLOSED: {
            if (previous_scripts.empty())
                return;

            String path = previous_scripts.back();
            previous_scripts.pop_back();

            Vector<String> extensions;
            gResourceManager().get_recognized_extensions_for_type("Script", extensions);
            bool built_in = !PathUtils::is_resource_file(path);

            if (ContainerUtils::contains(extensions, PathUtils::get_extension(path)) || built_in) {
                if (built_in) {
                    StringView res_path = StringUtils::get_slice(path, "::", 0);
                    if (gResourceManager().get_resource_type(res_path) == "PackedScene") {
                        if (!EditorNode::get_singleton()->is_scene_open(res_path)) {
                            EditorNode::get_singleton()->load_scene(res_path);
                            script_editor->call_deferred([p_option]() { script_editor->_menu_option(p_option); });
                            previous_scripts.push_back(path); // repeat the operation
                            return;
                        }
                    } else {
                        EditorNode::get_singleton()->load_resource(res_path);
                    }
                }

                Ref<Script> scr = dynamic_ref_cast<Script>(gResourceManager().load(path));
                if (not scr) {
                    editor->show_warning(TTR("Could not load file at:") + "\n\n" + path, TTR("Error!"));
                    file_dialog_option = -1;
                    return;
                }

                edit(scr);
                file_dialog_option = -1;
                return;
            } else {
                Error error;
                Ref<TextFile> text_file = dynamic_ref_cast<TextFile>(_load_text_file(path, &error));
                if (error != OK)
                    editor->show_warning(TTR("Could not load file at:") + "\n\n" + path, TTR("Error!"));

                if (text_file) {
                    edit(text_file);
                    file_dialog_option = -1;
                    return;
                }
            }
        } break;
        case FILE_SAVE_ALL: {
            if (_test_script_times_on_disk())
                return;

            save_all_scripts();
        } break;
        case SEARCH_IN_FILES: {
            _on_find_in_files_requested({});
        } break;
        case REPLACE_IN_FILES: {
            _on_replace_in_files_requested("");
        } break;
        case SEARCH_HELP: {
            help_search_dialog->popup_dialog();
        } break;
        case SEARCH_WEBSITE: {
            OS::get_singleton()->shell_open(VERSION_DOCS_URL "/");
        } break;

        case WINDOW_NEXT: {
            _history_forward();
        } break;
        case WINDOW_PREV: {
            _history_back();
        } break;
        case WINDOW_SORT: {
            _sort_list_on_update = true;
            _update_script_names();
        } break;
        case DEBUG_KEEP_DEBUGGER_OPEN: {
            bool ischecked = debug_menu->get_popup()->is_item_checked(
                    debug_menu->get_popup()->get_item_index(DEBUG_KEEP_DEBUGGER_OPEN));
            if (debugger) {
                debugger->set_hide_on_stop(ischecked);
            }
            debug_menu->get_popup()->set_item_checked(
                    debug_menu->get_popup()->get_item_index(DEBUG_KEEP_DEBUGGER_OPEN), !ischecked);
            EditorSettings::get_singleton()->set_project_metadata("debug_options", "keep_debugger_open", !ischecked);
        } break;
        case DEBUG_WITH_EXTERNAL_EDITOR: {
            bool ischecked = debug_menu->get_popup()->is_item_checked(
                    debug_menu->get_popup()->get_item_index(DEBUG_WITH_EXTERNAL_EDITOR));
            if (debugger) {
                debugger->set_debug_with_external_editor(!ischecked);
            }
            debug_menu->get_popup()->set_item_checked(
                    debug_menu->get_popup()->get_item_index(DEBUG_WITH_EXTERNAL_EDITOR), !ischecked);
            EditorSettings::get_singleton()->set_project_metadata(
                    "debug_options", "debug_with_external_editor", !ischecked);
        } break;
        case TOGGLE_SCRIPTS_PANEL: {
            if (current) {
                ScriptTextEditor *editor = object_cast<ScriptTextEditor>(current);
                toggle_scripts_panel();
                if (editor) {
                    editor->update_toggle_scripts_button();
                }
            } else {
                toggle_scripts_panel();
            }
        }
    }

    if (current) {

        switch (p_option) {
            case FILE_SAVE: {

                save_current_script();

            } break;
            case FILE_SAVE_AS: {

                if (trim_trailing_whitespace_on_save)
                    current->trim_trailing_whitespace();

                current->insert_final_newline();

                if (convert_indent_on_save) {
                    if (use_space_indentation) {
                        current->convert_indent_to_spaces();
                    } else {
                        current->convert_indent_to_tabs();
                    }
                }
                RES resource = current->get_edited_resource();
                Ref<TextFile> text_file { dynamic_ref_cast<TextFile>(resource) };
                if (text_file != nullptr) {
                    file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
                    file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
                    file_dialog_option = FILE_SAVE_AS;

                    Vector<String> extensions;
                    gResourceManager().get_recognized_extensions_for_type("Script", extensions);
                    file_dialog->clear_filters();
                    file_dialog->set_current_dir(PathUtils::get_base_dir(text_file->get_path()));
                    file_dialog->set_current_file(PathUtils::get_file(text_file->get_path()));
                    file_dialog->popup_centered_ratio();
                    file_dialog->set_title(TTR("Save File As..."));
                    break;
                }

                editor->push_item(resource.get());
                editor->save_resource_as(resource);

            } break;

            case FILE_TOOL_RELOAD:
            case FILE_TOOL_RELOAD_SOFT: {

                current->reload(p_option == FILE_TOOL_RELOAD_SOFT);

            } break;
            case FILE_RUN: {

                Ref<Script> scr = dynamic_ref_cast<Script>(current->get_edited_resource());
                if (scr == nullptr || not scr) {
                    EditorNode::get_singleton()->show_warning("Can't obtain the script for running.");
                    break;
                }

                current->apply_code();
                Error err = scr->reload(false); //hard reload script before running always

                if (err != OK) {
                    EditorNode::get_singleton()->show_warning("Script failed reloading, check console for errors.");
                    return;
                }
                if (!scr->is_tool()) {

                    EditorNode::get_singleton()->show_warning("Script is not in tool mode, will not be able to run.");
                    return;
                }

                if (!ClassDB::is_parent_class(scr->get_instance_base_type(), "EditorScript")) {

                    EditorNode::get_singleton()->show_warning("To run this script, it must inherit EditorScript and be set to tool mode.");
                    return;
                }

                Ref<EditorScript> es(make_ref_counted<EditorScript>());
                es->set_script(scr.get_ref_ptr());
                es->set_editor(EditorNode::get_singleton());

                es->_run();

                EditorNode::get_undo_redo()->clear_history();
            } break;
            case FILE_CLOSE: {
                if (current->is_unsaved()) {
                    _ask_close_current_unsaved_tab(current);
                } else {
                    _close_current_tab(false);
                }
            } break;
            case FILE_COPY_PATH: {
                _copy_script_path();
            } break;
            case SHOW_IN_FILE_SYSTEM: {
                const RES script = current->get_edited_resource();
                String path = script->get_path();
                if (!path.empty()) {
                    if (path.contains("::")) { // Built-in.
                        path = StringUtils::get_slice(path,"::", 0); // Show the scene instead.
                    }
                    FileSystemDock *file_system_dock = EditorNode::get_singleton()->get_filesystem_dock();
                    file_system_dock->navigate_to_path(path);
                    // Ensure that the FileSystem dock is visible.
                    TabContainer *tab_container = (TabContainer *)file_system_dock->get_parent_control();
                    tab_container->set_current_tab(file_system_dock->get_position_in_parent());
                }
            } break;
            case CLOSE_DOCS: {
                _close_docs_tab();
            } break;
            case CLOSE_OTHER_TABS: {
                _close_other_tabs();
            } break;
            case CLOSE_ALL: {
                _close_all_tabs();
            } break;
            case DEBUG_NEXT: {

                if (debugger)
                    debugger->debug_next();
            } break;
            case DEBUG_STEP: {

                if (debugger)
                    debugger->debug_step();

            } break;
            case DEBUG_BREAK: {

                if (debugger)
                    debugger->debug_break();

            } break;
            case DEBUG_CONTINUE: {

                if (debugger)
                    debugger->debug_continue();

            } break;
            case WINDOW_MOVE_UP: {

                if (tab_container->get_current_tab() > 0) {
                    tab_container->move_child(current, tab_container->get_current_tab() - 1);
                    tab_container->set_current_tab(tab_container->get_current_tab() - 1);
                    _update_script_names();
                }
            } break;
            case WINDOW_MOVE_DOWN: {

                if (tab_container->get_current_tab() < tab_container->get_child_count() - 1) {
                    tab_container->move_child(current, tab_container->get_current_tab() + 1);
                    tab_container->set_current_tab(tab_container->get_current_tab() + 1);
                    _update_script_names();
                }
            } break;
            default: {

                if (p_option >= WINDOW_SELECT_BASE) {

                    tab_container->set_current_tab(p_option - WINDOW_SELECT_BASE);
                    _update_script_names();
                }
            }
        }
    } else {

        EditorHelp *help = object_cast<EditorHelp>(tab_container->get_current_tab_control());
        if (help) {

            switch (p_option) {
                case HELP_SEARCH_FIND: {
                    help->popup_search();
                } break;
                case HELP_SEARCH_FIND_NEXT: {
                    help->search_again();
                } break;
                case HELP_SEARCH_FIND_PREVIOUS: {
                    help->search_again(true);
                } break;
                case FILE_CLOSE: {
                    _close_current_tab();
                } break;
                case CLOSE_DOCS: {
                    _close_docs_tab();
                } break;
                case CLOSE_OTHER_TABS: {
                    _close_other_tabs();
                } break;
                case CLOSE_ALL: {
                    _close_all_tabs();
                } break;
                case WINDOW_MOVE_UP: {

                    if (tab_container->get_current_tab() > 0) {
                        tab_container->move_child(help, tab_container->get_current_tab() - 1);
                        tab_container->set_current_tab(tab_container->get_current_tab() - 1);
                        _update_script_names();
                    }
                } break;
                case WINDOW_MOVE_DOWN: {

                    if (tab_container->get_current_tab() < tab_container->get_child_count() - 1) {
                        tab_container->move_child(help, tab_container->get_current_tab() + 1);
                        tab_container->set_current_tab(tab_container->get_current_tab() + 1);
                        _update_script_names();
                    }
                } break;
            }
        }
    }
}

void ScriptEditor::_update_debug_options() {
    bool keep_debugger_open = EditorSettings::get_singleton()->get_project_metadataT("debug_options", "keep_debugger_open", false);
    bool debug_with_external_editor = EditorSettings::get_singleton()->get_project_metadataT("debug_options", "debug_with_external_editor", false);

    if (keep_debugger_open) {
        _menu_option(DEBUG_KEEP_DEBUGGER_OPEN);
    }
    if (debug_with_external_editor) {
        _menu_option(DEBUG_WITH_EXTERNAL_EDITOR);
    }
}


void ScriptEditor::_theme_option(int p_option) {
    switch (p_option) {
        case THEME_IMPORT: {
            file_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
            file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
            file_dialog_option = THEME_IMPORT;
            file_dialog->clear_filters();
            file_dialog->add_filter("*.tet");
            file_dialog->popup_centered_ratio();
            file_dialog->set_title(TTR("Import Theme"));
        } break;
        case THEME_RELOAD: {
            EditorSettings::get_singleton()->load_text_editor_theme();
        } break;
        case THEME_SAVE: {
            if (EditorSettings::get_singleton()->is_default_text_editor_theme()) {
                ScriptEditor::_show_save_theme_as_dialog();
            } else if (!EditorSettings::get_singleton()->save_text_editor_theme()) {
                editor->show_warning(TTR("Error while saving theme"), TTR("Error saving"));
            }
        } break;
        case THEME_SAVE_AS: {
            ScriptEditor::_show_save_theme_as_dialog();
        } break;
    }
}

void ScriptEditor::_show_save_theme_as_dialog() {
    file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
    file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
    file_dialog_option = THEME_SAVE_AS;
    file_dialog->clear_filters();
    file_dialog->add_filter("*.tet");
    file_dialog->set_current_path(PathUtils::plus_file(EditorSettings::get_singleton()->get_text_editor_themes_dir(),EditorSettings::get_singleton()->get("text_editor/theme/color_theme").as<String>()));
    file_dialog->popup_centered_ratio();
    file_dialog->set_title(TTR("Save Theme As..."));
}
bool ScriptEditor::_has_docs_tab() const {
    const int child_count = tab_container->get_child_count();
    for (int i = 0; i < child_count; i++) {
        if (object_cast<EditorHelp>(tab_container->get_child(i))) {
            return true;
        }
    }
    return false;
}

bool ScriptEditor::_has_script_tab() const {
    const int child_count = tab_container->get_child_count();
    for (int i = 0; i < child_count; i++) {
        if (object_cast<ScriptEditorBase>(tab_container->get_child(i))) {
            return true;
        }
    }
    return false;
}

void ScriptEditor::_prepare_file_menu() {
    PopupMenu *menu = file_menu->get_popup();
    const bool current_is_doc = _get_current_editor() == nullptr;

    menu->set_item_disabled(menu->get_item_index(FILE_REOPEN_CLOSED), previous_scripts.empty());

    menu->set_item_disabled(menu->get_item_index(FILE_SAVE), current_is_doc);
    menu->set_item_disabled(menu->get_item_index(FILE_SAVE_AS), current_is_doc);
    menu->set_item_disabled(menu->get_item_index(FILE_SAVE_ALL), !_has_script_tab());

    menu->set_item_disabled(menu->get_item_index(FILE_TOOL_RELOAD_SOFT), current_is_doc);
    menu->set_item_disabled(menu->get_item_index(FILE_COPY_PATH), current_is_doc);
    menu->set_item_disabled(menu->get_item_index(SHOW_IN_FILE_SYSTEM), current_is_doc);

    menu->set_item_disabled(menu->get_item_index(WINDOW_PREV), history_pos <= 0);
    menu->set_item_disabled(menu->get_item_index(WINDOW_NEXT), history_pos >= history.size() - 1);

    menu->set_item_disabled(menu->get_item_index(FILE_CLOSE), tab_container->get_child_count() < 1);
    menu->set_item_disabled(menu->get_item_index(CLOSE_ALL), tab_container->get_child_count() < 1);
    menu->set_item_disabled(menu->get_item_index(CLOSE_OTHER_TABS), tab_container->get_child_count() <= 1);
    menu->set_item_disabled(menu->get_item_index(CLOSE_DOCS), !_has_docs_tab());

    menu->set_item_disabled(menu->get_item_index(FILE_RUN), current_is_doc);
}
void ScriptEditor::_tab_changed(int p_which) {

    ensure_select_current();
}

void ScriptEditor::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {

            editor->connect("play_pressed",callable_mp(this, &ScriptEditor::_editor_play));
            editor->connect("pause_pressed",callable_mp(this, &ScriptEditor::_editor_pause));
            editor->connect("stop_pressed",callable_mp(this, &ScriptEditor::_editor_stop));
            editor->connect("script_add_function_request",callable_mp(this, &ScriptEditor::_add_callback));
            editor->connect("resource_saved",callable_mp(this, &ScriptEditor::_res_saved_callback));
            editor->connect("scene_saved", callable_mp(this, &ScriptEditor::_scene_saved_callback));
            script_list->connect("item_selected",callable_mp(this, &ScriptEditor::_script_selected));

            members_overview->connect("item_selected",callable_mp(this, &ScriptEditor::_members_overview_selected));
            help_overview->connect("item_selected",callable_mp(this, &ScriptEditor::_help_overview_selected));
            script_split->connect("dragged",callable_mp(this, &ScriptEditor::_script_split_dragged));

            EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &ScriptEditor::_editor_settings_changed));
            EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &ScriptEditor::_filesystem_changed));
            [[fallthrough]];
        }
        case NOTIFICATION_THEME_CHANGED: {

            help_search->set_button_icon(get_theme_icon("HelpSearch", "EditorIcons"));
            site_search->set_button_icon(get_theme_icon("Instance", "EditorIcons"));

            script_forward->set_button_icon(get_theme_icon("Forward", "EditorIcons"));
            script_back->set_button_icon(get_theme_icon("Back", "EditorIcons"));

            members_overview_alphabeta_sort_button->set_button_icon(get_theme_icon("Sort", "EditorIcons"));

            filter_scripts->set_right_icon(get_theme_icon("Search", "EditorIcons"));
            filter_methods->set_right_icon(get_theme_icon("Search", "EditorIcons"));

            filename->add_theme_style_override("normal", editor->get_gui_base()->get_theme_stylebox("normal", "LineEdit"));

            recent_scripts->set_as_minsize();
        } break;

        case NOTIFICATION_READY: {

            get_tree()->connect("tree_changed", callable_mp(this, &ScriptEditor::_tree_changed));
            editor->get_inspector_dock()->connect("request_help", callable_mp(this, &ScriptEditor::_help_class_open));
            editor->connect("request_help_search", callable_mp(this, &ScriptEditor::_help_search));
            _update_debug_options();
        } break;

        case NOTIFICATION_EXIT_TREE: {

            editor->disconnect("stop_pressed", callable_mp(this, &ScriptEditor::_editor_stop));
        } break;

        case MainLoop::NOTIFICATION_WM_FOCUS_IN: {

            _test_script_times_on_disk();
            _update_modified_scripts_for_external_editor();
        } break;

        case CanvasItem::NOTIFICATION_VISIBILITY_CHANGED: {

            if (is_visible()) {
                find_in_files_button->show();
            } else {
                if (find_in_files->is_visible_in_tree()) {
                    editor->hide_bottom_panel();
                }
                find_in_files_button->hide();
            }

        } break;

        default:
            break;
    }
}

bool ScriptEditor::can_take_away_focus() const {

    ScriptEditorBase *current = _get_current_editor();
    if (current) {
        return current->can_lose_focus_on_node_selection();
    } else {
        return true;
    }
}

void ScriptEditor::close_builtin_scripts_from_scene(StringView p_scene) {

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));

        if (se) {

            Ref<Script> script = dynamic_ref_cast<Script>(se->get_edited_resource());
            if (script == nullptr || not script)
                continue;
            //TODO: use PathUtils::is_internal_path ?
            if (StringUtils::contains(script->get_path(),"::") && StringUtils::begins_with(script->get_path(),p_scene)) { //is an internal script and belongs to scene being closed
                _close_tab(i, false);
                i--;
            }
        }
    }
}

void ScriptEditor::edited_scene_changed() {

    _update_modified_scripts_for_external_editor();
}

void ScriptEditor::notify_script_close(const Ref<Script> &p_script) {
    emit_signal("script_close", p_script);
}

void ScriptEditor::notify_script_changed(const Ref<Script> &p_script) {
    emit_signal("editor_script_changed", p_script);
}

void ScriptEditor::get_breakpoints(Vector<String> *p_breakpoints) {

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se)
            continue;

        Ref<Script> script = dynamic_ref_cast<Script>(se->get_edited_resource());
        if (script == nullptr) {
            continue;
        }

        Vector<int> bpoints;
        se->get_breakpoints(&bpoints);
        String base = script->get_path();
        //TODO replace below with PathUtils::is_internal_path ?
        if (base.starts_with("local://") || base.empty()) {
            continue;
        }

        for (int E : bpoints) {

            p_breakpoints->push_back(base + ":" + itos(E + 1));
        }
    }
}

void ScriptEditor::ensure_focus_current() {

    if (!is_inside_tree())
        return;

    ScriptEditorBase *current = _get_current_editor();
    if (current)
        current->ensure_focus();
}

void ScriptEditor::_members_overview_selected(int p_idx) {
    ScriptEditorBase *se = _get_current_editor();
    if (!se) {
        return;
    }
    // Go to the member's line and reset the cursor column. We can't change scroll_position
    // directly until we have gone to the line first, since code might be folded.
    se->goto_line(members_overview->get_item_metadata(p_idx).as<int>());
    Dictionary state = se->get_edit_state().as<Dictionary>();
    state["column"] = 0;
    state["scroll_position"] = members_overview->get_item_metadata(p_idx);
    se->set_edit_state(state);
}

void ScriptEditor::_help_overview_selected(int p_idx) {
    Node *current = tab_container->get_child(tab_container->get_current_tab());
    EditorHelp *se = object_cast<EditorHelp>(current);
    if (!se) {
        return;
    }
    se->scroll_to_section(help_overview->get_item_metadata(p_idx).as<int>());
}

void ScriptEditor::_script_selected(int p_idx) {

    grab_focus_block = !Input::get_singleton()->is_mouse_button_pressed(1); //amazing hack, simply amazing

    _go_to_tab(script_list->get_item_metadata(p_idx).as<int>());
    grab_focus_block = false;
}

void ScriptEditor::ensure_select_current() {

    if (tab_container->get_child_count() && tab_container->get_current_tab() >= 0) {

        ScriptEditorBase *se = _get_current_editor();
        if (se) {
            se->enable_editor();

            if (!grab_focus_block && is_visible_in_tree()) {
                se->ensure_focus();
            }

        }
    }

    _update_selected_editor_menu();
}

void ScriptEditor::_find_scripts(Node *p_base, Node *p_current, Set<Ref<Script> > &used) {
    if (p_current != p_base && p_current->get_owner() != p_base)
        return;

    if (p_current->get_script_instance()) {
        Ref<Script> scr(refFromRefPtr<Script>(p_current->get_script()));
        if (scr)
            used.insert(scr);
    }

    for (int i = 0; i < p_current->get_child_count(); i++) {
        _find_scripts(p_base, p_current->get_child(i), used);
    }
}

struct _ScriptEditorItemData {

    String name;
    String sort_key;
    Ref<Texture> icon;
    int index;
    String tooltip;
    bool used;
    int category;
    Node *ref;

    bool operator<(const _ScriptEditorItemData &id) const {

        if (category == id.category) {
            if (sort_key == id.sort_key) {
                return index < id.index;
            } else {
                return sort_key < id.sort_key;
            }
        } else {
            return category < id.category;
        }
    }
};

void ScriptEditor::_update_members_overview_visibility() {

    ScriptEditorBase *se = _get_current_editor();
    if (!se) {
        members_overview_alphabeta_sort_button->set_visible(false);
        members_overview->set_visible(false);
        overview_vbox->set_visible(false);
        return;
    }

    if (members_overview_enabled && se->show_members_overview()) {
        members_overview_alphabeta_sort_button->set_visible(true);
        members_overview->set_visible(true);
        overview_vbox->set_visible(true);
    } else {
        members_overview_alphabeta_sort_button->set_visible(false);
        members_overview->set_visible(false);
        overview_vbox->set_visible(false);
    }
}

void ScriptEditor::_toggle_members_overview_alpha_sort(bool p_alphabetic_sort) {
    EditorSettings::get_singleton()->set("text_editor/tools/sort_members_outline_alphabetically", p_alphabetic_sort);
    _update_members_overview();
}

void ScriptEditor::_update_members_overview() {
    members_overview->clear();

    ScriptEditorBase *se = _get_current_editor();
    if (!se) {
        return;
    }

    Vector<String> functions = se->get_functions();
    if (EditorSettings::get_singleton()->getT<bool>("text_editor/tools/sort_members_outline_alphabetically")) {
        eastl::sort(functions.begin(), functions.end());
    }

    for (int i = 0; i < functions.size(); i++) {
        String filter = filter_methods->get_text();
        StringView name = StringUtils::get_slice(functions[i],":", 0);
        if (filter.empty() || StringUtils::is_subsequence_of(filter,name,StringUtils::CaseInsensitive)) {
            members_overview->add_item(StringName(name),Ref<Texture>());
            members_overview->set_item_metadata(members_overview->get_item_count() - 1, StringUtils::to_int(StringUtils::get_slice(functions[i],":", 1)) - 1);
        }
    }

    String path = se->get_edited_resource()->get_path();
    bool built_in = !PathUtils::is_resource_file(path);
    StringName name(built_in ? PathUtils::get_file(path) : se->get_name());
    filename->set_text(name);
}

void ScriptEditor::_update_help_overview_visibility() {

    int selected = tab_container->get_current_tab();
    if (selected < 0 || selected >= tab_container->get_child_count()) {
        help_overview->set_visible(false);
        return;
    }

    Node *current = tab_container->get_child(tab_container->get_current_tab());
    EditorHelp *se = object_cast<EditorHelp>(current);
    if (!se) {
        help_overview->set_visible(false);
        return;
    }

    if (help_overview_enabled) {
        members_overview_alphabeta_sort_button->set_visible(false);
        help_overview->set_visible(true);
        overview_vbox->set_visible(true);
        filename->set_text(se->get_name());
    } else {
        help_overview->set_visible(false);
        overview_vbox->set_visible(false);
    }
}

void ScriptEditor::_update_help_overview() {
    help_overview->clear();

    int selected = tab_container->get_current_tab();
    if (selected < 0 || selected >= tab_container->get_child_count())
        return;

    Node *current = tab_container->get_child(tab_container->get_current_tab());
    EditorHelp *se = object_cast<EditorHelp>(current);
    if (!se) {
        return;
    }

    Vector<Pair<String, int> > sections = se->get_sections();
    for (int i = 0; i < sections.size(); i++) {
        help_overview->add_item(StringName(sections[i].first),Ref<Texture>());
        help_overview->set_item_metadata(i, sections[i].second);
    }
}

void ScriptEditor::_update_script_colors() {

    bool script_temperature_enabled = EditorSettings::get_singleton()->getT<bool>("text_editor/script_list/script_temperature_enabled");
    bool highlight_current = EditorSettings::get_singleton()->getT<bool>("text_editor/script_list/highlight_current_script");

    int hist_size = EditorSettings::get_singleton()->getT<int>("text_editor/script_list/script_temperature_history_size");
    Color hot_color = get_theme_color("accent_color", "Editor");
    Color cold_color = get_theme_color("font_color", "Editor");

    for (int i = 0; i < script_list->get_item_count(); i++) {

        int c = script_list->get_item_metadata(i).as<int>();
        Node *n = tab_container->get_child(c);
        if (!n)
            continue;

        script_list->set_item_custom_bg_color(i, Color(0, 0, 0, 0));

        bool current = tab_container->get_current_tab() == c;
        if (current && highlight_current) {
            script_list->set_item_custom_bg_color(i, EditorSettings::get_singleton()->getT<Color>("text_editor/script_list/current_script_background_color"));

        } else if (script_temperature_enabled) {

            if (!n->has_meta("__editor_pass")) {
                continue;
            }

            int pass = n->get_meta("__editor_pass").as<int>();
            int h = edit_pass - pass;
            if (h > hist_size) {
                continue;
            }
            int non_zero_hist_size = hist_size == 0 ? 1 : hist_size;
            float v = Math::ease((edit_pass - pass) / float(non_zero_hist_size), 0.4);

            script_list->set_item_custom_fg_color(i, hot_color.linear_interpolate(cold_color, v));
        }
    }
}

void ScriptEditor::_update_script_names() {

    if (restoring_layout)
        return;

    Set<Ref<Script> > used;
    Node *edited = EditorNode::get_singleton()->get_edited_scene();
    if (edited) {
        _find_scripts(edited, edited, used);
    }

    script_list->clear();
    bool split_script_help = EditorSettings::get_singleton()->getT<bool>("text_editor/script_list/group_help_pages");
    ScriptSortBy sort_by = EditorSettings::get_singleton()->getT<ScriptSortBy>("text_editor/script_list/sort_scripts_by");
    ScriptListName display_as = EditorSettings::get_singleton()->getT<ScriptListName>("text_editor/script_list/list_script_names_as");

    Vector<_ScriptEditorItemData> sedata;

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (se) {

            Ref<Texture> icon = se->get_theme_icon();
            String path = se->get_edited_resource()->get_path();
            bool built_in = !PathUtils::is_resource_file(path);
            String name = se->get_name();

            _ScriptEditorItemData sd;
            sd.icon = icon;
            sd.name = name;
            sd.tooltip = path;
            sd.index = i;
            sd.used = used.contains(dynamic_ref_cast<Script>(se->get_edited_resource()));
            sd.category = 0;
            sd.ref = se;

            switch (sort_by) {
                case SORT_BY_NAME: {
                    sd.sort_key = StringUtils::to_lower(name);
                } break;
                case SORT_BY_PATH: {
                    sd.sort_key = path;
                } break;
                case SORT_BY_NONE: {
                    sd.sort_key = "";
                } break;
            }

            switch (display_as) {
                case DISPLAY_NAME: {
                    sd.name = name;
                } break;
                case DISPLAY_DIR_AND_NAME: {
                    if (!PathUtils::get_file(PathUtils::get_base_dir(path)).empty()) {
                        sd.name = PathUtils::plus_file(PathUtils::get_file(PathUtils::get_base_dir(path)),name);
                    } else {
                        sd.name = name;
                    }
                } break;
                case DISPLAY_FULL_PATH: {
                    sd.name = path;
                } break;
            }

            sedata.emplace_back(eastl::move(sd));
        }
        using namespace PathUtils;
        Vector<String> disambiguated_script_names;
        Vector<String> full_script_paths;
        for (const _ScriptEditorItemData &item : sedata) {
            String name = item.name.replaced("(*)", "");
            ScriptListName script_display = (ScriptListName)(int)EditorSettings::get_singleton()->get("text_editor/script_list/list_script_names_as");
            switch (script_display) {
                case DISPLAY_NAME: {
                    name = get_file(name);
                } break;
                case DISPLAY_DIR_AND_NAME: {
                    name = plus_file(get_file(get_base_dir(name)),get_file(name));
                } break;
                default:
                    break;
            }

            disambiguated_script_names.push_back(name);
            full_script_paths.push_back(item.tooltip);
        }

        EditorNode::disambiguate_filenames(full_script_paths, disambiguated_script_names);

        for (int j = 0; j < sedata.size(); j++) {
            if (sedata[j].name.ends_with("(*)")) {
                sedata[j].name = disambiguated_script_names[j] + "(*)";
            } else {
                sedata[j].name = disambiguated_script_names[j];
            }
        }


        EditorHelp *eh = object_cast<EditorHelp>(tab_container->get_child(i));
        if (eh) {

            String name(eh->get_class());
            Ref<Texture> icon = get_theme_icon("Help", "EditorIcons");
            String tooltip = qPrintable(UIString(TTR("%1 Class Reference")).arg(name.c_str()));

            _ScriptEditorItemData sd;
            sd.icon = icon;
            sd.name = name;
            sd.sort_key = StringUtils::to_lower(name);
            sd.tooltip = tooltip;
            sd.index = i;
            sd.used = false;
            sd.category = split_script_help ? 1 : 0;
            sd.ref = eh;

            sedata.push_back(sd);
        }
    }

    if (_sort_list_on_update && !sedata.empty()) {
        eastl::sort(sedata.begin(), sedata.end());

        // change actual order of tab_container so that the order can be rearranged by user
        int cur_tab = tab_container->get_current_tab();
        int prev_tab = tab_container->get_previous_tab();
        int new_cur_tab = -1;
        int new_prev_tab = -1;
        for (int i = 0; i < sedata.size(); i++) {
            tab_container->move_child(sedata[i].ref, i);
            if (new_prev_tab == -1 && sedata[i].index == prev_tab) {
                new_prev_tab = i;
            }
            if (new_cur_tab == -1 && sedata[i].index == cur_tab) {
                new_cur_tab = i;
            }
            // Update index of sd entries for sorted order
            _ScriptEditorItemData sd = sedata[i];
            sd.index = i;
            sedata[i]=sd;
        }
        tab_container->set_current_tab(new_prev_tab);
        tab_container->set_current_tab(new_cur_tab);
        _sort_list_on_update = false;
    }

    Vector<_ScriptEditorItemData> sedata_filtered;
    for (int i = 0; i < sedata.size(); i++) {
        String filter = filter_scripts->get_text();
        if (filter.empty() || StringUtils::is_subsequence_of(filter,sedata[i].name,StringUtils::CaseInsensitive)) {
            sedata_filtered.push_back(sedata[i]);
        }
    }

    for (int i = 0; i < sedata_filtered.size(); i++) {
        script_list->add_item(StringName(sedata_filtered[i].name), sedata_filtered[i].icon);
        int index = script_list->get_item_count() - 1;
        script_list->set_item_tooltip(index, sedata_filtered[i].tooltip);
        script_list->set_item_metadata(index, sedata_filtered[i].index); /* Saving as metadata the script's index in the tab container and not the filtered one */
        if (sedata_filtered[i].used) {
            script_list->set_item_custom_bg_color(index, Color(88 / 255.0f, 88 / 255.0f, 60 / 255.0f));
        }
        if (tab_container->get_current_tab() == sedata_filtered[i].index) {
            script_list->select(index);
            script_name_label->set_text(StringName(sedata_filtered[i].name));
            script_icon->set_texture(sedata_filtered[i].icon);
            ScriptEditorBase *se = _get_current_editor();
            if (se) {
                se->enable_editor();
                _update_selected_editor_menu();
            }
        }
    }

    if (!waiting_update_names) {
        _update_members_overview();
        _update_help_overview();
    } else {
        waiting_update_names = false;
    }
    _update_members_overview_visibility();
    _update_help_overview_visibility();
    _update_script_colors();

}

void ScriptEditor::_update_script_connections() {
    for (int i = 0; i < tab_container->get_child_count(); i++) {
        ScriptTextEditor *ste = object_cast<ScriptTextEditor>(tab_container->get_child(i));
        if (!ste) {
            continue;
        }
        ste->_update_connected_methods();
    }
}

Ref<TextFile> ScriptEditor::_load_text_file(StringView p_path, Error *r_error) {
    if (r_error) {
        *r_error = ERR_FILE_CANT_OPEN;
    }

    String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
    String path = gResourceRemapper().path_remap(local_path);

    TextFile *text_file = memnew(TextFile);
    Ref text_res(text_file,DoNotAddRef);
    const Error err = text_file->load_text(path);

    ERR_FAIL_COND_V_MSG(err != OK, Ref<TextFile>(), "Cannot load text file '" + path + "'.");

    text_file->set_file_path(local_path);
    text_file->set_path(local_path, true);

    if (ResourceManagerTooling::get_timestamp_on_load()) {
        ResourceTooling::set_last_modified_time(text_file, FileAccess::get_modified_time(path));
    }

    if (r_error) {
        *r_error = OK;
    }

    return text_res;
}

Error ScriptEditor::_save_text_file(Ref<TextFile> p_text_file, StringView p_path) {
    Ref<TextFile> sqscr = dynamic_ref_cast<TextFile>(p_text_file);
    ERR_FAIL_COND_V(not sqscr, ERR_INVALID_PARAMETER);

    String source(sqscr->get_text());

    Error err;
    FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);

    ERR_FAIL_COND_V_MSG(err, err, "Cannot save text file '" + String(p_path) + "'.");

    file->store_string(source);
    if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
        memdelete(file);
        return ERR_CANT_CREATE;
    }
    file->close();
    memdelete(file);

    if (ResourceManagerTooling::get_timestamp_on_save()) {
        ResourceTooling::set_last_modified_time(p_text_file.get(),FileAccess::get_modified_time(p_path));
    }

    _res_saved_callback(sqscr);
    return OK;
}

bool ScriptEditor::edit(const RES &p_resource, int p_line, int p_col, bool p_grab_focus) {

    if (not p_resource)
        return false;

    Ref<Script> script = dynamic_ref_cast<Script>(p_resource);
    // Don't open dominant script if using an external editor.
    bool use_external_editor =
            EditorSettings::get_singleton()->getT<bool>("text_editor/external/use_external_editor") ||
            (script && script->get_language()->overrides_external_editor());
    // Ignore external editor for built-in scripts.
    use_external_editor &= !(script && (script->get_path().empty() || script->get_path().contains("::")));
    const bool open_dominant = EditorSettings::get_singleton()->getT<bool>("text_editor/files/open_dominant_script_on_scene_change");

    const bool should_open = (open_dominant && !use_external_editor) || !EditorNode::get_singleton()->is_changing_scene();


    // refuse to open built-in if scene is not loaded

    // see if already has it

    if (script && script->get_language()->overrides_external_editor()) {
        if (should_open) {
            Error err = script->get_language()->open_in_external_editor(script, p_line >= 0 ? p_line : 0, p_col);
            if (err != OK) {
                ERR_PRINT("Couldn't open script in the overridden external text editor");
            }
        }
        return false;
    }

    if (use_external_editor &&
            (debugger->get_dump_stack_script() != p_resource || debugger->get_debug_with_external_editor()) &&
            PathUtils::is_resource_file(p_resource->get_path()) &&
            p_resource->get_class_name() != StringName("VisualScript")) {

        String path = EditorSettings::get_singleton()->getT<String>("text_editor/external/exec_path");
        String flags = EditorSettings::get_singleton()->getT<String>("text_editor/external/exec_flags");

        Vector<String> args;
        bool has_file_flag = false;
        String script_path = ProjectSettings::get_singleton()->globalize_path(p_resource->get_path());

        if (!flags.empty()) {
            String project_path = ProjectSettings::get_singleton()->get_resource_path();

            flags = StringUtils::replacen(flags,"{line}", itos(p_line > 0 ? p_line : 0));
            flags = StringUtils::replacen(flags,"{col}", itos(p_col));
            flags = StringUtils::replace(StringUtils::strip_edges( flags),"\\\\", "\\");

            int from = 0;
            int num_chars = 0;
            bool inside_quotes = false;

            for (size_t i = 0; i < flags.size(); i++) {

                if (flags[i] == '"' && (!i || flags[i - 1] != '\\')) {

                    if (!inside_quotes) {
                        from++;
                    }
                    inside_quotes = !inside_quotes;

                } else if (flags[i] == '\0' || (!inside_quotes && flags[i] == ' ')) {

                    StringView arg = StringUtils::substr(flags,from, num_chars);
                    if (StringUtils::contains(arg,"{file}")) {
                        has_file_flag = true;
                    }

                    // do path replacement here, else there will be issues with spaces and quotes
                    arg = StringUtils::replacen(arg,"{project}", project_path);
                    arg = StringUtils::replacen(arg,"{file}", script_path);
                    args.emplace_back(arg);

                    from = i + 1;
                    num_chars = 0;
                } else {
                    num_chars++;
                }
            }
        }

        // Default to passing script path if no {file} flag is specified.
        if (!has_file_flag) {
            args.push_back(script_path);
        }

        Error err = OS::get_singleton()->execute(path, args, false);
        if (err == OK)
            return false;
        WARN_PRINT("Couldn't open external text editor, using internal");
    }

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se)
            continue;

        if ((script != nullptr && se->get_edited_resource() == p_resource) ||
                se->get_edited_resource()->get_path() == p_resource->get_path()) {

            if (should_open) {
                se->enable_editor();

                if (tab_container->get_current_tab() != i) {
                    _go_to_tab(i);
                    _update_script_names();
                }
                if (is_visible_in_tree())
                    se->ensure_focus();

                if (p_line > 0) {
                    se->goto_line(p_line - 1);
                }
            }
            _update_script_names();
            script_list->ensure_current_is_visible();
            return true;
        }
    }

    // doesn't have it, make a new one

    ScriptEditorBase *se = nullptr;

    for (int i = script_editor_func_count - 1; i >= 0; i--) {
        se = script_editor_funcs[i](p_resource);
        if (se)
            break;
    }
    ERR_FAIL_COND_V(!se, false);

    se->set_edited_resource(p_resource);

    if (p_resource->get_class_name() != StringName("VisualScript")) {
        bool highlighter_set = false;
        for (int i = 0; i < syntax_highlighters_func_count; i++) {
            SyntaxHighlighter *highlighter = syntax_highlighters_funcs[i]();
            se->add_syntax_highlighter(highlighter);

            if (script != nullptr && !highlighter_set) {
                Vector<String> languages = highlighter->get_supported_languages();
                if (languages.contains(String(script->get_language()->get_name()))) {
                    se->set_syntax_highlighter(highlighter);
                    highlighter_set = true;
                }
            }
        }
    }

    tab_container->add_child(se);
    if (p_grab_focus) {
        se->enable_editor();
    }
    se->set_tooltip_request_func("_get_debug_tooltip", this);
    if (se->get_edit_menu()) {
        se->get_edit_menu()->hide();
        menu_hb->add_child(se->get_edit_menu());
        menu_hb->move_child(se->get_edit_menu(), 1);
    }

    if (p_grab_focus) {
        _go_to_tab(tab_container->get_tab_count() - 1);
        _add_recent_script(p_resource->get_path());

    }

    _sort_list_on_update = true;
    _update_script_names();
    _save_layout();
    se->connect("name_changed",callable_mp(this, &ClassName::_update_script_names));
    se->connect("edited_script_changed",callable_mp(this, &ClassName::_script_changed));
    se->connect("request_help",callable_mp(this, &ClassName::_help_search));
    se->connect("request_open_script_at_line",callable_mp(this, &ClassName::_goto_script_line));
    se->connect("go_to_help",callable_mp(this, &ClassName::_help_class_goto));
    se->connect("request_save_history",callable_mp(this, &ClassName::_save_history));
    se->connect("search_in_files_requested",callable_mp(this, &ClassName::_on_find_in_files_requested));
    se->connect("replace_in_files_requested",callable_mp(this, &ClassName::_on_replace_in_files_requested));

    //test for modification, maybe the script was not edited but was loaded

    _test_script_times_on_disk(p_resource);
    _update_modified_scripts_for_external_editor(script);

    if (p_line > 0) {
        se->goto_line(p_line - 1);
    }

    notify_script_changed(script);
    return true;
}

void ScriptEditor::save_current_script() {
    ScriptEditorBase *current = _get_current_editor();
    if (!current || _test_script_times_on_disk()) {
        return;
    }

    if (trim_trailing_whitespace_on_save) {
        current->trim_trailing_whitespace();
    }

    current->insert_final_newline();

    if (convert_indent_on_save) {
        if (use_space_indentation) {
            current->convert_indent_to_spaces();
        } else {
            current->convert_indent_to_tabs();
        }
    }

    RES resource = current->get_edited_resource();
    Ref<TextFile> text_file = dynamic_ref_cast<TextFile>(resource);

    if (text_file != nullptr) {
        current->apply_code();
        _save_text_file(text_file, text_file->get_path());
        return;
    }

    if (resource->get_path().empty() || resource->get_path().contains("::")) {
        // If built-in script, save the scene instead.
        const String scene_path(StringUtils::get_slice(resource->get_path(),"::", 0));
        if (!scene_path.empty()) {
            StringView scene_to_save[1] = { scene_path };
            editor->save_scene_list(scene_to_save);
        }
    } else {
        editor->save_resource(resource);
    }
}
void ScriptEditor::save_all_scripts() {
    Vector<StringView> scenes_to_save;

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se)
            continue;

        if (convert_indent_on_save) {
            if (use_space_indentation) {
                se->convert_indent_to_spaces();
            } else {
                se->convert_indent_to_tabs();
            }
        }

        if (trim_trailing_whitespace_on_save) {
            se->trim_trailing_whitespace();
        }

        se->insert_final_newline();

        if (!se->is_unsaved())
            continue;

        RES edited_res(se->get_edited_resource());
        if (edited_res) {
            se->apply_code();
        }

        if (!edited_res->get_path().empty() && !PathUtils::is_internal_path(edited_res->get_path())) {
            Ref<TextFile> text_file = dynamic_ref_cast<TextFile>(edited_res);
            if (text_file != nullptr) {
                _save_text_file(text_file, text_file->get_path());
                continue;
            }
            editor->save_resource(edited_res); //external script, save it
        } else {
            // For built-in scripts, save their scenes instead.
            StringView scene_path(StringUtils::get_slice(edited_res->get_path(),"::", 0));
            if (scenes_to_save.contains(scene_path)) {
                scenes_to_save.push_back(scene_path);
        }
    }

    }
    if (!scenes_to_save.empty()) {
        editor->save_scene_list(scenes_to_save);
    }
    _update_script_names();
    EditorFileSystem::get_singleton()->update_script_classes();
}

void ScriptEditor::apply_scripts() const {

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se)
            continue;
        se->apply_code();
    }
}

void ScriptEditor::open_script_create_dialog(StringView p_base_name, StringView p_base_path) {
    _menu_option(FILE_NEW);
    script_create_dialog->config(p_base_name, p_base_path);
}

void ScriptEditor::_editor_play() {

    debugger->start();
    debug_menu->get_popup()->grab_focus();
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_NEXT), true);
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_STEP), true);
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_BREAK), false);
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_CONTINUE), true);
}

void ScriptEditor::_editor_pause() {
}
void ScriptEditor::_editor_stop() {

    debugger->stop();
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_NEXT), true);
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_STEP), true);
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_BREAK), true);
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_CONTINUE), true);

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se) {

            continue;
        }

        se->set_debugger_active(false);
    }
}

void ScriptEditor::_add_callback(Object *p_obj, const StringName &p_function, const PoolVector<String> &p_args) {

    ERR_FAIL_COND(!p_obj);
    Ref<Script> script(refFromRefPtr<Script>(p_obj->get_script()));
    ERR_FAIL_COND(not script);

    editor->push_item(script.get());

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se)
            continue;
        if (se->get_edited_resource() != script)
            continue;

        se->add_callback(p_function, p_args);

        _go_to_tab(i);

        script_list->select(script_list->find_metadata(i));

        // Save the current script so the changes can be picked up by an external editor.
        if (!_is_built_in_script(script.get())) { // But only if it's not built-in script.
            save_current_script();
        }
        break;
    }
}

void ScriptEditor::_save_layout() {

    if (restoring_layout) {
        return;
    }

    editor->save_layout();
}

void ScriptEditor::_editor_settings_changed() {

    trim_trailing_whitespace_on_save = EditorSettings::get_singleton()->getT<bool>("text_editor/files/trim_trailing_whitespace_on_save");
    convert_indent_on_save = EditorSettings::get_singleton()->getT<bool>("text_editor/indent/convert_indent_on_save");
    use_space_indentation = EditorSettings::get_singleton()->getT<bool>("text_editor/indent/type");

    members_overview_enabled = EditorSettings::get_singleton()->getT<bool>("text_editor/script_list/show_members_overview");
    help_overview_enabled = EditorSettings::get_singleton()->getT<bool>("text_editor/help/show_help_index");
    _update_members_overview_visibility();
    _update_help_overview_visibility();

    _update_autosave_timer();
    String editor_theme(EditorSettings::get_singleton()->getT<String>("text_editor/theme/color_theme"));
    if (current_theme.empty()) {
        current_theme = EditorSettings::get_singleton()->getT<String>("text_editor/theme/color_theme");
    } else if (current_theme != editor_theme) {
        current_theme = editor_theme;
        EditorSettings::get_singleton()->load_text_editor_theme();
    }

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se)
            continue;

        se->update_settings();
    }
    _update_script_colors();
    _update_script_names();

    ScriptServer::set_reload_scripts_on_save(EDITOR_DEF_T("text_editor/files/auto_reload_and_parse_scripts_on_save", true));
}

void ScriptEditor::_filesystem_changed() {
    _update_script_names();
}

void ScriptEditor::_autosave_scripts() {

    save_all_scripts();
}

void ScriptEditor::_update_autosave_timer() {

    if (!autosave_timer->is_inside_tree()) {
        return;
    }

    float autosave_time = EditorSettings::get_singleton()->getT<float>("text_editor/files/autosave_interval_secs");
    if (autosave_time > 0) {
        autosave_timer->set_wait_time(autosave_time);
        autosave_timer->start();
    } else {
        autosave_timer->stop();
    }
}

void ScriptEditor::_tree_changed() {

    if (waiting_update_names)
        return;

    waiting_update_names = true;
    call_deferred([this]() {
        _update_script_names();
        _update_script_connections();
    }
    );
}

void ScriptEditor::_script_split_dragged(float) {

    _save_layout();
}

Variant ScriptEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {

    if (tab_container->get_child_count() == 0)
        return Variant();

    Node *cur_node = tab_container->get_child(tab_container->get_current_tab());

    HBoxContainer *drag_preview = memnew(HBoxContainer);
    StringName preview_name;
    Ref<Texture> preview_icon;

    ScriptEditorBase *se = object_cast<ScriptEditorBase>(cur_node);
    if (se) {
        preview_name = StringName(se->get_name());
        preview_icon = se->get_theme_icon();
    }
    EditorHelp *eh = object_cast<EditorHelp>(cur_node);
    if (eh) {
        preview_name = StringName(eh->get_class());
        preview_icon = get_theme_icon("Help", "EditorIcons");
    }

    if (preview_icon) {
        TextureRect *tf = memnew(TextureRect);
        tf->set_texture(preview_icon);
        drag_preview->add_child(tf);
    }
    Label *label = memnew(Label(preview_name));
    drag_preview->add_child(label);
    set_drag_preview(drag_preview);

    Dictionary drag_data;
    drag_data["type"] = "script_list_element"; // using a custom type because node caused problems when dragging to scene tree
    drag_data["script_list_element"] = Variant(cur_node);

    return drag_data;
}

bool ScriptEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {

    Dictionary d = p_data.as<Dictionary>();
    if (!d.has("type"))
        return false;

    if (d["type"] == "script_list_element") {

        Node *node = d["script_list_element"].as<Node *>();

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(node);
        if (se) {
            return true;
        }
        EditorHelp *eh = object_cast<EditorHelp>(node);
        if (eh) {
            return true;
        }
    }

    if (d["type"] == "nodes") {

        Array nodes = d["nodes"].as<Array>();
        if (nodes.empty())
            return false;
        Node *node = get_node(nodes[0].as<NodePath>());

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(node);
        if (se) {
            return true;
        }
        EditorHelp *eh = object_cast<EditorHelp>(node);
        if (eh) {
            return true;
        }
    }

    if (d["type"] == "files") {

        PoolVector<String> files = d["files"].as<PoolVector<String>>();

        if (files.empty())
            return false; //weird

        for (int i = 0; i < files.size(); i++) {
            const String &file(files[i]);
            if (file.empty() || !FileAccess::exists(file))
                continue;
            Ref<Script> scr = dynamic_ref_cast<Script>(gResourceManager().load(file));
            if (scr) {
                return true;
            }
        }
        return true;
    }

    return false;
}

void ScriptEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {

    if (!can_drop_data_fw(p_point, p_data, p_from))
        return;

    Dictionary d = p_data.as<Dictionary>();
    if (!d.has("type"))
        return;

    if (d["type"] == "script_list_element") {

        Node *node = d["script_list_element"].as<Node *>();

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(node);
        EditorHelp *eh = object_cast<EditorHelp>(node);
        if (se || eh) {
            int new_index = 0;
            if (script_list->get_item_count() > 0) {
                new_index = script_list->get_item_metadata(script_list->get_item_at_position(p_point)).as<int>();
            }
            tab_container->move_child(node, new_index);
            tab_container->set_current_tab(new_index);
            _update_script_names();
        }
    }

    if (d["type"] == "nodes") {

        Array nodes = d["nodes"].as<Array>();
        if (nodes.empty())
            return;
        Node *node = get_node(nodes[0].as<NodePath>());

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(node);
        EditorHelp *eh = object_cast<EditorHelp>(node);
        if (se || eh) {
            int new_index = 0;
            if (script_list->get_item_count() > 0) {
                new_index = script_list->get_item_metadata(script_list->get_item_at_position(p_point)).as<int>();
            }
            tab_container->move_child(node, new_index);
            tab_container->set_current_tab(new_index);
            _update_script_names();
        }
    }

    if (d["type"] == "files") {

        PoolVector<String> files(d["files"].as<PoolVector<String>>());

        int new_index = 0;
        if (script_list->get_item_count() > 0) {
            new_index = script_list->get_item_metadata(script_list->get_item_at_position(p_point)).as<int>();
        }
        int num_tabs_before = tab_container->get_child_count();
        for (int i = 0; i < files.size(); i++) {
            const String &file(files[i]);
            if (file.empty() || !FileAccess::exists(file))
                continue;
            Ref<Script> scr = dynamic_ref_cast<Script>(gResourceManager().load(file));
            if (scr) {
                edit(scr);
                if (tab_container->get_child_count() > num_tabs_before) {
                    tab_container->move_child(tab_container->get_child(tab_container->get_child_count() - 1), new_index);
                    num_tabs_before = tab_container->get_child_count();
                } else { /* Maybe script was already open */
                    tab_container->move_child(tab_container->get_child(tab_container->get_current_tab()), new_index);
                }
            }
        }
        tab_container->set_current_tab(new_index);
        _update_script_names();
    }
}

void ScriptEditor::_input(const Ref<InputEvent> &p_event) {
    // This feature can be disabled to avoid interfering with other uses of the additional
    // mouse buttons, such as push-to-talk in a VoIP program.
    if (EDITOR_GET("text_editor/navigation/mouse_extra_buttons_navigate_history")) {
        const Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);

        // Navigate the script history using additional mouse buttons present on some mice.
        // This must be hardcoded as the editor shortcuts dialog doesn't allow assigning
        // more than one shortcut per action.
        if (mb && mb->is_pressed() && is_visible_in_tree() && !get_viewport()->gui_has_modal_stack()) {
            if (mb->get_button_index() == BUTTON_XBUTTON1) {
                _history_back();
            }

            if (mb->get_button_index() == BUTTON_XBUTTON2) {
                _history_forward();
            }
        }
    }
}
void ScriptEditor::_unhandled_input(const Ref<InputEvent> &p_event) {
    ERR_FAIL_COND(!p_event);
    if (!is_visible_in_tree() || !p_event->is_pressed() || p_event->is_echo())
        return;
    if (ED_IS_SHORTCUT("script_editor/next_script", p_event)) {
        if (script_list->get_item_count() > 1) {
            int next_tab = script_list->get_current() + 1;
            next_tab %= script_list->get_item_count();
            _go_to_tab(script_list->get_item_metadata(next_tab).as<int>());
            _update_script_names();
        }
    }
    if (ED_IS_SHORTCUT("script_editor/prev_script", p_event)) {
        if (script_list->get_item_count() > 1) {
            int next_tab = script_list->get_current() - 1;
            next_tab = next_tab >= 0 ? next_tab : script_list->get_item_count() - 1;
            _go_to_tab(script_list->get_item_metadata(next_tab).as<int>());
            _update_script_names();
        }
    }
    if (ED_IS_SHORTCUT("script_editor/window_move_up", p_event)) {
        _menu_option(WINDOW_MOVE_UP);
    }
    if (ED_IS_SHORTCUT("script_editor/window_move_down", p_event)) {
        _menu_option(WINDOW_MOVE_DOWN);
    }
}

void ScriptEditor::_script_list_gui_input(const Ref<InputEvent> &ev) {

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(ev);
    if (mb && mb->is_pressed()) {
        switch (mb->get_button_index()) {

            case BUTTON_MIDDLE: {
                // Right-click selects automatically; middle-click does not.
                int idx = script_list->get_item_at_position(mb->get_position(), true);
                if (idx >= 0) {
                    script_list->select(idx);
                    _script_selected(idx);
                    _menu_option(FILE_CLOSE);
                }
            } break;

            case BUTTON_RIGHT: {
                _make_script_list_context_menu();
            } break;
        }
    }
}

void ScriptEditor::_make_script_list_context_menu() {

    context_menu->clear();

    int selected = tab_container->get_current_tab();
    if (selected < 0 || selected >= tab_container->get_child_count())
        return;

    ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(selected));
    if (se) {
        context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/save"), FILE_SAVE);
        context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/save_as"), FILE_SAVE_AS);
    }
    context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_file"), FILE_CLOSE);
    context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_all"), CLOSE_ALL);
    context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_other_tabs"), CLOSE_OTHER_TABS);
    context_menu->add_separator();
    if (se) {
        Ref<Script> scr = dynamic_ref_cast<Script>(se->get_edited_resource());
        if (scr != nullptr) {
            context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/reload_script_soft"), FILE_TOOL_RELOAD_SOFT);
            if (scr && scr->is_tool()) {
                context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/run_file"), FILE_RUN);
                context_menu->add_separator();
            }
        }
        context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/copy_path"), FILE_COPY_PATH);
        context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/show_in_file_system"), SHOW_IN_FILE_SYSTEM);
        context_menu->add_separator();
    }

    context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/window_move_up"), WINDOW_MOVE_UP);
    context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/window_move_down"), WINDOW_MOVE_DOWN);
    context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/window_sort"), WINDOW_SORT);
    context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/toggle_scripts_panel"), TOGGLE_SCRIPTS_PANEL);
    context_menu->set_item_disabled(context_menu->get_item_index(CLOSE_ALL), tab_container->get_child_count() <= 0);
    context_menu->set_item_disabled(context_menu->get_item_index(CLOSE_OTHER_TABS), tab_container->get_child_count() <= 1);
    context_menu->set_item_disabled(context_menu->get_item_index(WINDOW_MOVE_UP), tab_container->get_current_tab() <= 0);
    context_menu->set_item_disabled(context_menu->get_item_index(WINDOW_MOVE_DOWN), tab_container->get_current_tab() >= tab_container->get_child_count() - 1);
    context_menu->set_item_disabled(context_menu->get_item_index(WINDOW_SORT), tab_container->get_child_count() <= 1);

    context_menu->set_position(get_global_transform().xform(get_local_mouse_position()));
    context_menu->set_size(Vector2(1, 1));
    context_menu->popup();
}

void ScriptEditor::set_window_layout(Ref<ConfigFile> p_layout) {

    if (!bool(EDITOR_DEF_T("text_editor/files/restore_scripts_on_load", true))) {
        return;
    }

    if (!p_layout->has_section_key("ScriptEditor", "open_scripts") && !p_layout->has_section_key("ScriptEditor", "open_help"))
        return;

    Array scripts = p_layout->get_value("ScriptEditor", "open_scripts").as<Array>();
    Array helps;
    if (p_layout->has_section_key("ScriptEditor", "open_help"))
        helps = p_layout->get_value("ScriptEditor", "open_help").as<Array>();

    restoring_layout = true;

    Vector<String> extensions;
    gResourceManager().get_recognized_extensions_for_type("Script", extensions);

    for (int i = 0; i < scripts.size(); i++) {

        String path = scripts[i].as<String>(); //TODO: verify this, same variant used for path and script_info.

        Dictionary script_info = scripts[i].as<Dictionary>();
        if (!script_info.empty()) {
            path = script_info["path"].as<String>();
        }

        if (!FileAccess::exists(path))
            continue;

        if (ContainerUtils::contains(extensions,PathUtils::get_extension(path))) {
            Ref<Script> scr = dynamic_ref_cast<Script>(gResourceManager().load(path));
            if (not scr) {
                continue;
            }
            if (!edit(scr,false)) {
                continue;
            }
        } else {
            Error error;
            Ref<TextFile> text_file = _load_text_file(path, &error);
            if (error != OK || not text_file) {
                continue;
            }
            if (!edit(text_file,false)) {
                continue;
            }
        }

        if (!script_info.empty()) {
            ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(tab_container->get_tab_count() - 1));
            if (se) {
                se->set_edit_state(script_info["state"]);
            }
        }
    }

    for (int i = 0; i < helps.size(); i++) {

        String path = helps[i].as<String>();
        if (path.empty()) { // invalid, skip
            continue;
        }
        _help_class_open(path);
    }

    for (int i = 0; i < tab_container->get_child_count(); i++) {
        tab_container->get_child(i)->set_meta("__editor_pass", Variant());
    }

    if (p_layout->has_section_key("ScriptEditor", "split_offset")) {
        script_split->set_split_offset(p_layout->get_value("ScriptEditor", "split_offset").as<int>());
    }

    restoring_layout = false;

    _update_script_names();
}

void ScriptEditor::get_window_layout(Ref<ConfigFile> p_layout) {

    Array scripts;
    Array helps;

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (se) {

            String path = se->get_edited_resource()->get_path();
            if (!PathUtils::is_resource_file(path))
                continue;

            Dictionary script_info;
            script_info["path"] = path;
            script_info["state"] = se->get_edit_state();

            scripts.push_back(script_info);
        }

        EditorHelp *eh = object_cast<EditorHelp>(tab_container->get_child(i));

        if (eh) {

            helps.push_back(eh->get_class());
        }
    }

    p_layout->set_value("ScriptEditor", "open_scripts", scripts);
    p_layout->set_value("ScriptEditor", "open_help", helps);
    p_layout->set_value("ScriptEditor", "split_offset", script_split->get_split_offset());
}

void ScriptEditor::_help_class_open(StringView p_class) {

    if (p_class.empty())
        return;

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        EditorHelp *eh = object_cast<EditorHelp>(tab_container->get_child(i));

        if (eh && p_class == StringView(eh->get_class())) {

            _go_to_tab(i);
            _update_script_names();
            return;
        }
    }

    EditorHelp *eh = memnew(EditorHelp);

    eh->set_name(p_class);
    tab_container->add_child(eh);
    _go_to_tab(tab_container->get_tab_count() - 1);
    eh->go_to_class(p_class, 0);
    eh->connect("go_to_help",callable_mp(this, &ClassName::_help_class_goto));
    _add_recent_script(p_class);
    _sort_list_on_update = true;
    _update_script_names();
    _save_layout();
}

void ScriptEditor::_help_class_goto(StringView p_desc) {

    StringView cname = StringUtils::get_slice(p_desc,":", 1);

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        EditorHelp *eh = object_cast<EditorHelp>(tab_container->get_child(i));

        if (eh && cname == StringView(eh->get_class())) {

            _go_to_tab(i);
            eh->go_to_help(p_desc);
            _update_script_names();
            return;
        }
    }

    EditorHelp *eh = memnew(EditorHelp);

    eh->set_name(cname);
    tab_container->add_child(eh);
    _go_to_tab(tab_container->get_tab_count() - 1);
    eh->go_to_help(p_desc);
    eh->connect("go_to_help",callable_mp(this, &ClassName::_help_class_goto));
    _add_recent_script(eh->get_class());
    _sort_list_on_update = true;
    _update_script_names();
    _save_layout();
}

void ScriptEditor::_update_selected_editor_menu() {

    for (int i = 0; i < tab_container->get_child_count(); i++) {

        bool current = tab_container->get_current_tab() == i;

        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (se && se->get_edit_menu()) {

            if (current)
                se->get_edit_menu()->show();
            else
                se->get_edit_menu()->hide();
        }
    }

    EditorHelp *eh = object_cast<EditorHelp>(tab_container->get_current_tab_control());
    script_search_menu->get_popup()->clear();
    if (eh) {

        script_search_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/find", TTR("Find..."), KEY_MASK_CMD | KEY_F), HELP_SEARCH_FIND);
        script_search_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/find_next", TTR("Find Next"), KEY_F3), HELP_SEARCH_FIND_NEXT);
        script_search_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/find_previous", TTR("Find Previous"), KEY_MASK_SHIFT | KEY_F3), HELP_SEARCH_FIND_PREVIOUS);
        script_search_menu->get_popup()->add_separator();
        script_search_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/find_in_files", TTR("Find in Files"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_F), SEARCH_IN_FILES);
        script_search_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/replace_in_files", TTR("Replace in Files"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_R), REPLACE_IN_FILES);
        script_search_menu->show();
    } else {

        if (tab_container->get_child_count() == 0) {
            script_search_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/find_in_files", TTR("Find in Files"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_F), SEARCH_IN_FILES);
            script_search_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/replace_in_files", TTR("Replace in Files"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_R), REPLACE_IN_FILES);
            script_search_menu->show();
        } else {
            script_search_menu->hide();
        }
    }
}

void ScriptEditor::_update_history_pos(int p_new_pos) {

    Node *n = tab_container->get_current_tab_control();

    if (object_cast<ScriptEditorBase>(n)) {

        history[history_pos].state = object_cast<ScriptEditorBase>(n)->get_edit_state();
    }
    if (object_cast<EditorHelp>(n)) {

        history[history_pos].state = object_cast<EditorHelp>(n)->get_scroll();
    }

    history_pos = p_new_pos;
    tab_container->set_current_tab(history[history_pos].control->get_index());

    n = history[history_pos].control;

    if (object_cast<ScriptEditorBase>(n)) {

        object_cast<ScriptEditorBase>(n)->set_edit_state(history[history_pos].state);
        object_cast<ScriptEditorBase>(n)->ensure_focus();

        Ref<Script> script = dynamic_ref_cast<Script>(object_cast<ScriptEditorBase>(n)->get_edited_resource());
        if (script != nullptr) {
            notify_script_changed(script);
        }
    }

    if (object_cast<EditorHelp>(n)) {

        object_cast<EditorHelp>(n)->set_scroll(history[history_pos].state.as<int>());
        object_cast<EditorHelp>(n)->set_focused();
    }

    n->set_meta("__editor_pass", ++edit_pass);
    _update_script_names();
    _update_history_arrows();
    _update_selected_editor_menu();
}

void ScriptEditor::_history_forward() {

    if (history_pos < history.size() - 1) {
        _update_history_pos(history_pos + 1);
    }
}

void ScriptEditor::_history_back() {

    if (history_pos > 0) {
        _update_history_pos(history_pos - 1);
    }
}

Vector<Ref<Script>> ScriptEditor::get_open_scripts() const {

    Vector<Ref<Script> > out_scripts;

    for (int i = 0; i < tab_container->get_child_count(); i++) {
        ScriptEditorBase *se = object_cast<ScriptEditorBase>(tab_container->get_child(i));
        if (!se)
            continue;

        Ref<Script> script = dynamic_ref_cast<Script>(se->get_edited_resource());
        if (script != nullptr) {
            out_scripts.emplace_back(eastl::move(script));
        }
    }

    return out_scripts;
}

void ScriptEditor::set_scene_root_script(const Ref<Script>& p_script) {
    // Don't open dominant script if using an external editor.
    bool use_external_editor =
            EditorSettings::get_singleton()->getT<bool>("text_editor/external/use_external_editor") ||
            (p_script && p_script->get_language()->overrides_external_editor());
    // Ignore external editor for built-in scripts.
    use_external_editor &= !(p_script && (p_script->get_path().empty() || p_script->get_path().contains("::")));

    const bool open_dominant =
            EditorSettings::get_singleton()->getT<bool>("text_editor/files/open_dominant_script_on_scene_change");

    if (open_dominant && !use_external_editor && p_script) {
        edit(p_script);
    }
}

bool ScriptEditor::script_goto_method(Ref<Script> p_script, const StringName &p_method) {

    int line = p_script->get_member_line(p_method);

    if (line == -1)
        return false;

    return edit(p_script, line, 0);
}

void ScriptEditor::set_live_auto_reload_running_scripts(bool p_enabled) {

    auto_reload_running_scripts = p_enabled;
}

void ScriptEditor::_help_search(StringView p_text) {
    help_search_dialog->popup_dialog(p_text);
}

void ScriptEditor::_open_script_request(StringView p_path) {

    Ref<Script> script = dynamic_ref_cast<Script>(gResourceManager().load(p_path));
    if (script) {
        script_editor->edit(script, false);
        return;
    }

    Error err;
    Ref<TextFile> text_file = script_editor->_load_text_file(p_path, &err);
    if (text_file) {
        script_editor->edit(text_file, false);
        return;
    }
}

int ScriptEditor::syntax_highlighters_func_count = 0;
CreateSyntaxHighlighterFunc ScriptEditor::syntax_highlighters_funcs[ScriptEditor::SYNTAX_HIGHLIGHTER_FUNC_MAX];

void ScriptEditor::register_create_syntax_highlighter_function(CreateSyntaxHighlighterFunc p_func) {
    ERR_FAIL_COND(syntax_highlighters_func_count == SYNTAX_HIGHLIGHTER_FUNC_MAX);
    syntax_highlighters_funcs[syntax_highlighters_func_count++] = p_func;
}

int ScriptEditor::script_editor_func_count = 0;
CreateScriptEditorFunc ScriptEditor::script_editor_funcs[ScriptEditor::SCRIPT_EDITOR_FUNC_MAX];

void ScriptEditor::register_create_script_editor_function(CreateScriptEditorFunc p_func) {

    ERR_FAIL_COND(script_editor_func_count == SCRIPT_EDITOR_FUNC_MAX);
    script_editor_funcs[script_editor_func_count++] = p_func;
}

void ScriptEditor::_script_changed() {

    NodeDock::singleton->update_lists();
}

void ScriptEditor::_on_find_in_files_requested(StringView text) {
    find_in_files_dialog->set_find_in_files_mode(FindInFilesDialog::SEARCH_MODE);
    find_in_files_dialog->set_search_text(text);
    find_in_files_dialog->popup_centered_minsize();

}

void ScriptEditor::_on_replace_in_files_requested(StringView text) {
    find_in_files_dialog->set_find_in_files_mode(FindInFilesDialog::REPLACE_MODE);
    find_in_files_dialog->set_search_text(text);
    find_in_files_dialog->set_replace_text("");
    find_in_files_dialog->popup_centered_minsize();
}

void ScriptEditor::_on_find_in_files_result_selected(StringView fpath, int line_number, int begin, int end) {

    if (gResourceManager().exists(fpath)) {
        RES res(gResourceManager().load(fpath));
        StringView ext(PathUtils::get_extension(fpath));

        if (ext == StringView("shader") || ext == StringView("gdshader") ) {
            ShaderEditorPlugin *shader_editor = object_cast<ShaderEditorPlugin>(
                    EditorNode::get_singleton()->get_editor_data().get_editor("Shader"));
            shader_editor->edit(res.get());
            shader_editor->make_visible(true);
            shader_editor->get_shader_editor()->goto_line_selection(line_number - 1, begin, end);
            return;
        } else if (ext == "tscn") {
            editor->load_scene(fpath);
            return;
        } else {
            Ref<Script> script = dynamic_ref_cast<Script>(res);
            if (script) {
                edit(script);

                ScriptTextEditor *ste = object_cast<ScriptTextEditor>(_get_current_editor());
                if (ste) {
                    ste->goto_line_selection(line_number - 1, begin, end);
                }
                return;
            }
        }
    }

    // If the file is not a valid resource/script, load it as a text file.
    Error err;
    Ref<TextFile> text_file = _load_text_file(fpath, &err);
    if (text_file) {
        edit(text_file);

        TextEditor *te = object_cast<TextEditor>(_get_current_editor());
        if (te) {
            te->goto_line_selection(line_number - 1, begin, end);
        }
    }
}

void ScriptEditor::_start_find_in_files(bool with_replace) {

    FindInFiles *f = find_in_files->get_finder();

    f->set_search_text(find_in_files_dialog->get_search_text());
    f->set_match_case(find_in_files_dialog->is_match_case());
    f->set_whole_words(find_in_files_dialog->is_whole_words());
    f->set_folder(find_in_files_dialog->get_folder());
    f->set_filter(find_in_files_dialog->get_filter());

    find_in_files->set_with_replace(with_replace);
    find_in_files->set_replace_text(find_in_files_dialog->get_replace_text());
    find_in_files->start_search();

    editor->make_bottom_panel_item_visible(find_in_files);
}

void ScriptEditor::_on_find_in_files_modified_files(const PoolStringArray& paths) {

    _test_script_times_on_disk();
    _update_modified_scripts_for_external_editor();
}

void ScriptEditor::_filter_scripts_text_changed(StringView p_newtext) {
    _update_script_names();
}

void ScriptEditor::_filter_methods_text_changed(StringView p_newtext) {
    _update_members_overview();
}

void ScriptEditor::_bind_methods() {
    MethodBinder::bind_method("_get_debug_tooltip", &ScriptEditor::_get_debug_tooltip);
    MethodBinder::bind_method("_unhandled_input", &ScriptEditor::_unhandled_input);

    SE_BIND_METHOD(ScriptEditor,get_drag_data_fw);
    SE_BIND_METHOD(ScriptEditor,can_drop_data_fw);
    SE_BIND_METHOD(ScriptEditor,drop_data_fw);

    MethodBinder::bind_method(D_METHOD("goto_line", {"line_number"}), &ScriptEditor::_goto_script_line2);
    MethodBinder::bind_method(D_METHOD("get_current_script"), &ScriptEditor::_get_current_script);
    MethodBinder::bind_method(D_METHOD("get_open_scripts"), &ScriptEditor::_get_open_scripts);
    SE_BIND_METHOD(ScriptEditor,open_script_create_dialog);
    SE_BIND_METHOD(ScriptEditor,reload_scripts);

    ADD_SIGNAL(MethodInfo("editor_script_changed", PropertyInfo(VariantType::OBJECT, "script", PropertyHint::ResourceType, "Script")));
    ADD_SIGNAL(MethodInfo("script_close", PropertyInfo(VariantType::OBJECT, "script", PropertyHint::ResourceType, "Script")));
}

ScriptEditor::ScriptEditor(EditorNode *p_editor) {

    current_theme = "";

    completion_cache = memnew(EditorScriptCodeCompletionCache);
    restoring_layout = false;
    waiting_update_names = false;
    pending_auto_reload = false;
    auto_reload_running_scripts = true;
    members_overview_enabled = EditorSettings::get_singleton()->getT<bool>("text_editor/script_list/show_members_overview");
    help_overview_enabled = EditorSettings::get_singleton()->getT<bool>("text_editor/help/show_help_index");
    editor = p_editor;

    VBoxContainer *main_container = memnew(VBoxContainer);
    add_child(main_container);

    menu_hb = memnew(HBoxContainer);
    main_container->add_child(menu_hb);

    script_split = memnew(HSplitContainer);
    main_container->add_child(script_split);
    script_split->set_v_size_flags(SIZE_EXPAND_FILL);

    list_split = memnew(VSplitContainer);
    script_split->add_child(list_split);
    list_split->set_v_size_flags(SIZE_EXPAND_FILL);

    scripts_vbox = memnew(VBoxContainer);
    scripts_vbox->set_v_size_flags(SIZE_EXPAND_FILL);
    list_split->add_child(scripts_vbox);

    filter_scripts = memnew(LineEdit);
    filter_scripts->set_placeholder(TTR("Filter scripts"));
    filter_scripts->set_clear_button_enabled(true);
    filter_scripts->connect("text_changed",callable_mp(this, &ClassName::_filter_scripts_text_changed));
    scripts_vbox->add_child(filter_scripts);

    script_list = memnew(ItemList);
    scripts_vbox->add_child(script_list);
    script_list->set_custom_minimum_size(Size2(150, 60) * EDSCALE); //need to give a bit of limit to avoid it from disappearing
    script_list->set_v_size_flags(SIZE_EXPAND_FILL);
    script_split->set_split_offset(140);
    _sort_list_on_update = true;
    script_list->connect("gui_input",callable_mp(this, &ClassName::_script_list_gui_input), ObjectNS::CONNECT_QUEUED);
    script_list->set_allow_rmb_select(true);
    script_list->set_drag_forwarding(this);

    context_menu = memnew(PopupMenu);
    add_child(context_menu);
    context_menu->connect("id_pressed",callable_mp(this, &ClassName::_menu_option));
    context_menu->set_hide_on_window_lose_focus(true);

    overview_vbox = memnew(VBoxContainer);
    overview_vbox->set_custom_minimum_size(Size2(0, 90));
    overview_vbox->set_v_size_flags(SIZE_EXPAND_FILL);

    list_split->add_child(overview_vbox);
    list_split->set_visible(EditorSettings::get_singleton()->get_project_metadata("scripts_panel", "show_scripts_panel", true).as<bool>());
    buttons_hbox = memnew(HBoxContainer);
    overview_vbox->add_child(buttons_hbox);

    filename = memnew(Label);
    filename->set_clip_text(true);
    filename->set_h_size_flags(SIZE_EXPAND_FILL);
    filename->add_theme_style_override("normal", EditorNode::get_singleton()->get_gui_base()->get_theme_stylebox("normal", "LineEdit"));
    buttons_hbox->add_child(filename);

    members_overview_alphabeta_sort_button = memnew(ToolButton);
    members_overview_alphabeta_sort_button->set_tooltip(TTR("Toggle alphabetical sorting of the method list."));
    members_overview_alphabeta_sort_button->set_toggle_mode(true);
    members_overview_alphabeta_sort_button->set_pressed(EditorSettings::get_singleton()->getT<bool>("text_editor/tools/sort_members_outline_alphabetically"));
    members_overview_alphabeta_sort_button->connect("toggled",callable_mp(this, &ClassName::_toggle_members_overview_alpha_sort));

    buttons_hbox->add_child(members_overview_alphabeta_sort_button);

    filter_methods = memnew(LineEdit);
    filter_methods->set_placeholder(TTR("Filter methods"));
    filter_methods->set_clear_button_enabled(true);
    filter_methods->connect("text_changed",callable_mp(this, &ClassName::_filter_methods_text_changed));
    overview_vbox->add_child(filter_methods);

    members_overview = memnew(ItemList);
    overview_vbox->add_child(members_overview);

    members_overview->set_allow_reselect(true);
    members_overview->set_custom_minimum_size(Size2(0, 60) * EDSCALE); //need to give a bit of limit to avoid it from disappearing
    members_overview->set_v_size_flags(SIZE_EXPAND_FILL);
    members_overview->set_allow_rmb_select(true);

    help_overview = memnew(ItemList);
    overview_vbox->add_child(help_overview);
    help_overview->set_allow_reselect(true);
    help_overview->set_custom_minimum_size(Size2(0, 60) * EDSCALE); //need to give a bit of limit to avoid it from disappearing
    help_overview->set_v_size_flags(SIZE_EXPAND_FILL);

    tab_container = memnew(TabContainer);
    tab_container->set_tabs_visible(false);
    tab_container->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
    script_split->add_child(tab_container);
    tab_container->set_h_size_flags(SIZE_EXPAND_FILL);

    ED_SHORTCUT("script_editor/window_sort", TTR("Sort"));
    ED_SHORTCUT("script_editor/window_move_up", TTR("Move Up"), KEY_MASK_SHIFT | KEY_MASK_ALT | KEY_UP);
    ED_SHORTCUT("script_editor/window_move_down", TTR("Move Down"), KEY_MASK_SHIFT | KEY_MASK_ALT | KEY_DOWN);
    // FIXME: These should be `KEY_GREATER` and `KEY_LESS` but those don't work.
    ED_SHORTCUT("script_editor/next_script", TTR("Next Script"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_PERIOD);
    ED_SHORTCUT("script_editor/prev_script", TTR("Previous Script"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_COMMA);
    set_process_input(true);
    set_process_unhandled_input(true);

    file_menu = memnew(MenuButton);
    menu_hb->add_child(file_menu);
    file_menu->set_text(TTR("File"));
    file_menu->set_switch_on_hover(true);
    file_menu->get_popup()->set_hide_on_window_lose_focus(true);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/new", TTR("New Script...")), FILE_NEW);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/new_textfile", TTR("New Text File...")), FILE_NEW_TEXTFILE);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/open", TTR("Open...")), ACT_FILE_OPEN);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/reopen_closed_script", TTR("Reopen Closed Script"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_T), FILE_REOPEN_CLOSED);
    file_menu->get_popup()->add_submenu_item(TTR("Open Recent"), StringName("RecentScripts"), FILE_OPEN_RECENT);

    recent_scripts = memnew(PopupMenu);
    recent_scripts->set_name("RecentScripts");
    file_menu->get_popup()->add_child(recent_scripts);
    recent_scripts->connect("id_pressed",callable_mp(this, &ClassName::_open_recent_script));
    _update_recent_scripts();

    file_menu->get_popup()->add_separator();
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/save", TTR("Save"), KEY_MASK_ALT | KEY_MASK_CMD | KEY_S), FILE_SAVE);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/save_as", TTR("Save As...")), FILE_SAVE_AS);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/save_all", TTR("Save All"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_MASK_ALT | KEY_S), FILE_SAVE_ALL);
    file_menu->get_popup()->add_separator();
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/reload_script_soft", TTR("Soft Reload Script"), KEY_MASK_CMD | KEY_MASK_ALT | KEY_R), FILE_TOOL_RELOAD_SOFT);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/copy_path", TTR("Copy Script Path")), FILE_COPY_PATH);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/show_in_file_system", TTR("Show in FileSystem")), SHOW_IN_FILE_SYSTEM);
    file_menu->get_popup()->add_separator();

    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/history_previous", TTR("History Previous"), KEY_MASK_ALT | KEY_LEFT), WINDOW_PREV);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/history_next", TTR("History Next"), KEY_MASK_ALT | KEY_RIGHT), WINDOW_NEXT);
    file_menu->get_popup()->add_separator();

    file_menu->get_popup()->add_submenu_item(TTR("Theme"), StringName("Theme"), FILE_THEME);

    theme_submenu = memnew(PopupMenu);
    theme_submenu->set_name("Theme");
    file_menu->get_popup()->add_child(theme_submenu);
    theme_submenu->connect("id_pressed",callable_mp(this, &ClassName::_theme_option));
    theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/import_theme", TTR("Import Theme...")), THEME_IMPORT);
    theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/reload_theme", TTR("Reload Theme")), THEME_RELOAD);

    theme_submenu->add_separator();
    theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/save_theme", TTR("Save Theme")), THEME_SAVE);
    theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/save_theme_as", TTR("Save Theme As...")), THEME_SAVE_AS);

    file_menu->get_popup()->add_separator();
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_file", TTR("Close"), KEY_MASK_CMD | KEY_W), FILE_CLOSE);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_all", TTR("Close All")), CLOSE_ALL);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_other_tabs", TTR("Close Other Tabs")), CLOSE_OTHER_TABS);
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_docs", TTR("Close Docs")), CLOSE_DOCS);

    file_menu->get_popup()->add_separator();
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/run_file", TTR("Run"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_X), FILE_RUN);

    file_menu->get_popup()->add_separator();
    file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/toggle_scripts_panel", TTR("Toggle Scripts Panel"), KEY_MASK_CMD | KEY_BACKSLASH), TOGGLE_SCRIPTS_PANEL);
    file_menu->get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_menu_option));
    file_menu->get_popup()->connect("about_to_show", callable_mp(this, &ClassName::_prepare_file_menu));

    script_search_menu = memnew(MenuButton);
    menu_hb->add_child(script_search_menu);
    script_search_menu->set_text(TTR("Search"));
    script_search_menu->set_switch_on_hover(true);
    script_search_menu->get_popup()->set_hide_on_window_lose_focus(true);
    script_search_menu->get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_menu_option));

    debug_menu = memnew(MenuButton);
    menu_hb->add_child(debug_menu);
    debug_menu->set_text(TTR("Debug"));
    debug_menu->set_switch_on_hover(true);
    debug_menu->get_popup()->set_hide_on_window_lose_focus(true);
    debug_menu->get_popup()->add_shortcut(ED_SHORTCUT("debugger/step_into", TTR("Step Into"), KEY_F11), DEBUG_STEP);
    debug_menu->get_popup()->add_shortcut(ED_SHORTCUT("debugger/step_over", TTR("Step Over"), KEY_F10), DEBUG_NEXT);
    debug_menu->get_popup()->add_separator();
    debug_menu->get_popup()->add_shortcut(ED_SHORTCUT("debugger/break", TTR("Break")), DEBUG_BREAK);
    debug_menu->get_popup()->add_shortcut(ED_SHORTCUT("debugger/continue", TTR("Continue"), KEY_F12), DEBUG_CONTINUE);
    debug_menu->get_popup()->add_separator();
    debug_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("debugger/keep_debugger_open", TTR("Keep Debugger Open")), DEBUG_KEEP_DEBUGGER_OPEN);
    debug_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("debugger/debug_with_external_editor", TTR("Debug with External Editor")), DEBUG_WITH_EXTERNAL_EDITOR);
    debug_menu->get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_menu_option));

    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_NEXT), true);
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_STEP), true);
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_BREAK), true);
    debug_menu->get_popup()->set_item_disabled(debug_menu->get_popup()->get_item_index(DEBUG_CONTINUE), true);

    menu_hb->add_spacer();

    script_icon = memnew(TextureRect);
    menu_hb->add_child(script_icon);
    script_name_label = memnew(Label);
    menu_hb->add_child(script_name_label);

    script_icon->hide();
    script_name_label->hide();

    menu_hb->add_spacer();

    site_search = memnew(ToolButton);
    site_search->set_text(TTR("Online Docs"));
    site_search->connectF("pressed",this,[=]() { _menu_option(SEARCH_WEBSITE); });
    menu_hb->add_child(site_search);
    site_search->set_tooltip(TTR("Open Godot online documentation."));

    help_search = memnew(ToolButton);
    help_search->set_text(TTR("Search Help"));
    help_search->connectF("pressed",this,[=]() { _menu_option(SEARCH_HELP); });
    menu_hb->add_child(help_search);
    help_search->set_tooltip(TTR("Search the reference documentation."));

    menu_hb->add_child(memnew(VSeparator));

    script_back = memnew(ToolButton);
    script_back->connect("pressed",callable_mp(this, &ClassName::_history_back));
    menu_hb->add_child(script_back);
    script_back->set_disabled(true);
    script_back->set_tooltip(TTR("Go to previous edited document."));

    script_forward = memnew(ToolButton);
    script_forward->connect("pressed",callable_mp(this, &ClassName::_history_forward));
    menu_hb->add_child(script_forward);
    script_forward->set_disabled(true);
    script_forward->set_tooltip(TTR("Go to next edited document."));

    tab_container->connect("tab_changed",callable_mp(this, &ClassName::_tab_changed));

    erase_tab_confirm = memnew(ConfirmationDialog);
    erase_tab_confirm->get_ok()->set_text(TTR("Save"));
    erase_tab_confirm->add_button(TTR("Discard"), OS::get_singleton()->get_swap_ok_cancel(), "discard");
    erase_tab_confirm->connect("confirmed",callable_gen(this, [this]() { _close_current_tab(true); }));
    erase_tab_confirm->connect("custom_action",callable_mp(this, &ClassName::_close_discard_current_tab));
    add_child(erase_tab_confirm);

    script_create_dialog = memnew(ScriptCreateDialog);
    script_create_dialog->set_title(TTR("Create Script"));
    add_child(script_create_dialog);
    script_create_dialog->connect("script_created",callable_mp(this, &ClassName::_script_created));

    file_dialog_option = -1;
    file_dialog = memnew(EditorFileDialog);
    add_child(file_dialog);
    file_dialog->connect("file_selected",callable_mp(this, &ClassName::_file_dialog_action));

    error_dialog = memnew(AcceptDialog);
    add_child(error_dialog);

    debugger = memnew(ScriptEditorDebugger(editor));
    debugger->connect("goto_script_line",callable_mp(this, &ClassName::_goto_script_line));
    debugger->connect("set_execution",callable_mp(this, &ClassName::_set_execution));
    debugger->connect("clear_execution",callable_mp(this, &ClassName::_clear_execution));
    debugger->connect("show_debugger",callable_mp(this, &ClassName::_show_debugger));

    disk_changed = memnew(ConfirmationDialog);
    {
        VBoxContainer *vbc = memnew(VBoxContainer);
        disk_changed->add_child(vbc);

        Label *dl = memnew(Label);
        dl->set_text(TTR("The following files are newer on disk.\nWhat action should be taken?:"));
        vbc->add_child(dl);

        disk_changed_list = memnew(Tree);
        vbc->add_child(disk_changed_list);
        disk_changed_list->set_v_size_flags(SIZE_EXPAND_FILL);

        disk_changed->connect("confirmed",callable_mp(this, &ClassName::reload_scripts));
        disk_changed->get_ok()->set_text(TTR("Reload"));

        disk_changed->add_button(TTR("Resave"), !OS::get_singleton()->get_swap_ok_cancel(), "resave");
        disk_changed->connect("custom_action",callable_mp(this, &ClassName::_resave_scripts));
    }

    add_child(disk_changed);

    script_editor = this;

    Button *db = EditorNode::get_singleton()->add_bottom_panel_item(TTR("Debugger"), debugger);
    // Add separation for the warning/error icon that is displayed later.
    db->add_constant_override("hseparation", 6 * EDSCALE);
    debugger->set_tool_button(db);

    debugger->connect("breaked",callable_mp(this, &ClassName::_breaked));

    autosave_timer = memnew(Timer);
    autosave_timer->set_one_shot(false);
    autosave_timer->connect(SceneStringNames::tree_entered, callable_mp(this, &ScriptEditor::_update_autosave_timer));
    autosave_timer->connect("timeout",callable_mp(this, &ClassName::_autosave_scripts));
    add_child(autosave_timer);

    grab_focus_block = false;

    help_search_dialog = memnew(EditorHelpSearch);
    add_child(help_search_dialog);
    help_search_dialog->connect("go_to_help",callable_mp(this, &ClassName::_help_class_goto));

    find_in_files_dialog = memnew(FindInFilesDialog);
    find_in_files_dialog->connect(StaticCString(FindInFilesDialog::SIGNAL_FIND_REQUESTED,true), callable_gen(this, [=]() { _start_find_in_files(false); }));
    find_in_files_dialog->connect(StaticCString(FindInFilesDialog::SIGNAL_REPLACE_REQUESTED,true), callable_gen(this, [=]() { _start_find_in_files(true); }));
    add_child(find_in_files_dialog);
    find_in_files = memnew(FindInFilesPanel);
    find_in_files_button = editor->add_bottom_panel_item(TTR("Search Results"), find_in_files);
    find_in_files->set_custom_minimum_size(Size2(0, 200) * EDSCALE);
    find_in_files->connect(StaticCString(FindInFilesPanel::SIGNAL_RESULT_SELECTED,true), callable_mp(this, &ScriptEditor::_on_find_in_files_result_selected));
    find_in_files->connect(StaticCString(FindInFilesPanel::SIGNAL_FILES_MODIFIED,true), callable_mp(this, &ScriptEditor::_on_find_in_files_modified_files));
    find_in_files->hide();
    find_in_files_button->hide();

    history_pos = -1;
    //debugger_gui->hide();

    edit_pass = 0;
    trim_trailing_whitespace_on_save = EditorSettings::get_singleton()->getT<bool>("text_editor/files/trim_trailing_whitespace_on_save");
    convert_indent_on_save = EditorSettings::get_singleton()->getT<bool>("text_editor/indent/convert_indent_on_save");
    use_space_indentation = EditorSettings::get_singleton()->getT<bool>("text_editor/indent/type");

    ScriptServer::edit_request_func = _open_script_request;

    add_theme_style_override("panel", editor->get_gui_base()->get_theme_stylebox("ScriptEditorPanel", "EditorStyles"));
    tab_container->add_theme_style_override("panel", editor->get_gui_base()->get_theme_stylebox("ScriptEditor", "EditorStyles"));
}

ScriptEditor::~ScriptEditor() {

    memdelete(completion_cache);
}

void ScriptEditorPlugin::edit(Object *p_object) {

    Script *p_script = object_cast<Script>(p_object);
    if (p_script) {

        StringView res_path = StringUtils::get_slice(p_script->get_path(), "::", 0);

        if (_is_built_in_script(p_script)) {
            if (gResourceManager().get_resource_type(res_path) == "PackedScene") {
                if (!EditorNode::get_singleton()->is_scene_open(res_path)) {
                    EditorNode::get_singleton()->load_scene(res_path);
                }
            } else {
                EditorNode::get_singleton()->load_resource(res_path);
            }
        }
        if(p_script)
            p_script->reference(); // is being put in Ref<Script>
        script_editor->edit(Ref<Script>(p_script));
    } else if (object_cast<TextFile>(p_object)) {
        script_editor->edit(RES(object_cast<TextFile>(p_object)));
    }
}

bool ScriptEditorPlugin::handles(Object *p_object) const {

    if (object_cast<TextFile>(p_object)) {
        return true;
    }

    if (object_cast<Script>(p_object)) {
        return true;
    }

    return p_object->is_class("Script");
}

void ScriptEditorPlugin::make_visible(bool p_visible) {

    if (p_visible) {
        script_editor->show();
        script_editor->set_process(true);
        script_editor->ensure_select_current();
    } else {

        script_editor->hide();
        script_editor->set_process(false);
    }
}

void ScriptEditorPlugin::selected_notify() {

    script_editor->ensure_select_current();
}

void ScriptEditorPlugin::save_external_data() {

    script_editor->save_all_scripts();
}

void ScriptEditorPlugin::apply_changes() {

    script_editor->apply_scripts();
}

void ScriptEditorPlugin::restore_global_state() {
}

void ScriptEditorPlugin::save_global_state() {
}

void ScriptEditorPlugin::set_window_layout(Ref<ConfigFile> p_layout) {

    script_editor->set_window_layout(p_layout);
}

void ScriptEditorPlugin::get_window_layout(Ref<ConfigFile> p_layout) {

    script_editor->get_window_layout(p_layout);
}

void ScriptEditorPlugin::get_breakpoints(Vector<String> *p_breakpoints) {

    script_editor->get_breakpoints(p_breakpoints);
}

void ScriptEditorPlugin::edited_scene_changed() {

    script_editor->edited_scene_changed();
}

ScriptEditorPlugin::ScriptEditorPlugin(EditorNode *p_node) {

    editor = p_node;
    script_editor = memnew(ScriptEditor(p_node));
    editor->get_viewport()->add_child(script_editor);
    script_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);

    script_editor->hide();

    EDITOR_DEF("text_editor/files/auto_reload_scripts_on_external_change", true);
    ScriptServer::set_reload_scripts_on_save(EDITOR_DEF_T("text_editor/files/auto_reload_and_parse_scripts_on_save", true));
    EDITOR_DEF("text_editor/files/open_dominant_script_on_scene_change", true);
    EDITOR_DEF("text_editor/external/use_external_editor", false);
    EDITOR_DEF("text_editor/external/exec_path", "");
    EDITOR_DEF("text_editor/script_list/script_temperature_enabled", true);
    EDITOR_DEF("text_editor/script_list/highlight_current_script", true);
    EDITOR_DEF("text_editor/script_list/script_temperature_history_size", 15);
    EDITOR_DEF("text_editor/script_list/current_script_background_color", Color(1, 1, 1, 0.3f));
    EDITOR_DEF("text_editor/script_list/group_help_pages", true);
    EditorSettings::get_singleton()->add_property_hint(PropertyInfo(VariantType::INT, "text_editor/script_list/sort_scripts_by", PropertyHint::Enum, "Name,Path,None"));
    EDITOR_DEF("text_editor/script_list/sort_scripts_by", 0);
    EditorSettings::get_singleton()->add_property_hint(PropertyInfo(VariantType::INT, "text_editor/script_list/list_script_names_as", PropertyHint::Enum, "Name,Parent Directory And Name,Full Path"));
    EDITOR_DEF("text_editor/script_list/list_script_names_as", 0);
    EditorSettings::get_singleton()->add_property_hint(PropertyInfo(VariantType::STRING, "text_editor/external/exec_path", PropertyHint::GlobalFile));
    EDITOR_DEF("text_editor/external/exec_flags", "{file}");
    EditorSettings::get_singleton()->add_property_hint(PropertyInfo(VariantType::STRING, "text_editor/external/exec_flags", PropertyHint::PlaceholderText, "Call flags with placeholders: {project}, {file}, {col}, {line}."));

    ED_SHORTCUT("script_editor/reopen_closed_script", TTR("Reopen Closed Script"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_T);
    ED_SHORTCUT("script_editor/clear_recent", TTR("Clear Recent Scripts"));
}

ScriptEditorPlugin::~ScriptEditorPlugin() {
}

void register_script_editor_plugin_classes() {
    ScriptEditorQuickOpen::initialize_class();
    ScriptEditorBase::initialize_class();
    ScriptEditor::initialize_class();
    ScriptEditorPlugin::initialize_class();
}
