/*************************************************************************/
/*  editor_data.cpp                                                      */
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

#include "editor_data.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/io/resource_loader.h"
#include "core/object_db.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor_node.h"
#include "editor_settings.h"
#include "scene/resources/packed_scene.h"

IMPL_GDCLASS(EditorSelection)

void EditorHistory::cleanup_history() {

    for (int i = 0; i < history.size(); i++) {

        bool fail = false;

        for (int j = 0; j < history[i].path.size(); j++) {
            if (history[i].path[j].ref) {
                continue;
            }

            Object *obj = object_for_entity(history[i].path[j].object);
            if (obj) {
                Node *n = object_cast<Node>(obj);
                if (n && n->is_inside_tree()) {
                    continue;
                }
                if (!n) { // Possibly still alive
                    continue;
                }
            }

            if (j <= history[i].level) {
                //before or equal level, complete fail
                fail = true;
            } else {
                //after level, clip
                history[i].path.resize(j);
            }

            break;
        }

        if (fail) {
            history.erase_at(i);
            i--;
        }
    }

    if (current >= history.size()) {
        current = history.size() - 1;
    }
}

void EditorHistory::_add_object(GameEntity p_object, StringView p_property, int p_level_change, bool p_inspector_only) {

    Object *obj = object_for_entity(p_object);
    ERR_FAIL_COND(!obj);
    RefCounted *r = object_cast<RefCounted>(obj);
    Obj o;
    if (r) {
        o.ref = REF(r);
    }
    o.object = p_object;
    o.property = p_property;
    o.inspector_only = p_inspector_only;

    History h;

    bool has_prev = current >= 0 && current < history.size();

    if (has_prev) {
        history.resize(current + 1); //clip history to next
    }

    if (!p_property.empty() && has_prev) {
        //add a sub property
        History &pr = history[current];
        h = pr;
        h.path.resize(h.level + 1);
        h.path.push_back(o);
        h.level++;
    } else if (p_level_change != -1 && has_prev) {
        //add a sub property
        History &pr = history[current];
        h = pr;
        ERR_FAIL_INDEX(p_level_change, h.path.size());
        h.level = p_level_change;
    } else {
        //add a new node
        h.path.push_back(o);
        h.level = 0;
    }

    history.push_back(h);
    current++;
}

void EditorHistory::add_object_inspector_only(GameEntity p_object) {

    _add_object(p_object, null_string, -1, true);
}

void EditorHistory::add_object(GameEntity p_object) {

    _add_object(p_object, null_string, -1);
}

void EditorHistory::add_object(GameEntity p_object, StringView p_subprop) {

    _add_object(p_object, p_subprop, -1);
}

void EditorHistory::add_object(GameEntity p_object, int p_relevel) {

    _add_object(p_object, null_string, p_relevel);
}

int EditorHistory::get_history_len() {
    return history.size();
}
int EditorHistory::get_history_pos() {
    return current;
}

bool EditorHistory::is_history_obj_inspector_only(int p_obj) const {

    ERR_FAIL_INDEX_V(p_obj, history.size(), false);
    ERR_FAIL_INDEX_V(history[p_obj].level, history[p_obj].path.size(), false);
    return history[p_obj].path[history[p_obj].level].inspector_only;
}

GameEntity EditorHistory::get_history_obj(int p_obj) const {
    ERR_FAIL_INDEX_V(p_obj, history.size(), entt::null);
    ERR_FAIL_INDEX_V(history[p_obj].level, history[p_obj].path.size(), entt::null);
    return history[p_obj].path[history[p_obj].level].object;
}

bool EditorHistory::is_at_beginning() const {
    return current <= 0;
}
bool EditorHistory::is_at_end() const {

    return ((current + 1) >= history.size());
}

bool EditorHistory::next() {

    cleanup_history();

    if ((current + 1) < history.size()) {
        current++;
    } else {
        return false;
    }

    return true;
}

bool EditorHistory::previous() {

    cleanup_history();

    if (current > 0) {
        current--;
    } else {
        return false;
    }

    return true;
}

bool EditorHistory::is_current_inspector_only() const {

    if (current < 0 || current >= history.size()) {
        return false;
    }

    const History &h = history[current];
    return h.path[h.level].inspector_only;
}
GameEntity EditorHistory::get_current() {

    if (current < 0 || current >= history.size())
        return entt::null;

    History &h = history[current];
    Object *obj = object_for_entity(h.path[h.level].object);
    if (!obj)
        return entt::null;

    return obj->get_instance_id();
}

int EditorHistory::get_path_size() const {

    if (current < 0 || current >= history.size()) {
        return 0;
    }

    const History &h = history[current];
    return h.path.size();
}

GameEntity EditorHistory::get_path_object(int p_index) const {

    if (current < 0 || current >= history.size())
        return entt::null;

    const History &h = history[current];

    ERR_FAIL_INDEX_V(p_index, h.path.size(), entt::null);

    Object *obj = object_for_entity(h.path[p_index].object);
    if (!obj)
        return entt::null;

    return obj->get_instance_id();
}

String EditorHistory::get_path_property(int p_index) const {

    if (current < 0 || current >= history.size())
        return null_string;

    const History &h = history[current];

    ERR_FAIL_INDEX_V(p_index, h.path.size(), null_string);

    return h.path[p_index].property;
}

void EditorHistory::clear() {

    history.clear();
    current = -1;
}

EditorHistory::EditorHistory() {

    current = -1;
}

EditorPlugin *EditorData::get_editor(Object *p_object) {

    for (int i = 0; i < editor_plugins.size(); i++) {

        if (editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object))
            return editor_plugins[i];
    }

    return nullptr;
}

EditorPlugin *EditorData::get_subeditor(Object *p_object) {

    for (int i = 0; i < editor_plugins.size(); i++) {

        if (!editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object))
            return editor_plugins[i];
    }

    return nullptr;
}

Vector<EditorPlugin *> EditorData::get_subeditors(Object *p_object) {
    Vector<EditorPlugin *> sub_plugins;
    for (int i = 0; i < editor_plugins.size(); i++) {
        if (!editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object)) {
            sub_plugins.push_back(editor_plugins[i]);
        }
    }
    return sub_plugins;
}

EditorPlugin *EditorData::get_editor(StringView p_name) {

    for (int i = 0; i < editor_plugins.size(); i++) {

        if (editor_plugins[i]->get_name() == p_name)
            return editor_plugins[i];
    }

    return nullptr;
}

void EditorData::copy_object_params(Object *p_object) {

    clipboard.clear();

    Vector<PropertyInfo> pinfo;
    p_object->get_property_list(&pinfo);

    for (const PropertyInfo &E : pinfo) {

        if (!(E.usage & PROPERTY_USAGE_EDITOR) || E.name == StringView("script") || E.name == StringView("scripts"))
            continue;

        PropertyData pd;
        pd.name = E.name;
        pd.value = p_object->get(E.name);
        clipboard.emplace_back(pd);
    }
}

void EditorData::get_editor_breakpoints(Vector<String> *p_breakpoints) {

    for (int i = 0; i < editor_plugins.size(); i++) {

        editor_plugins[i]->get_breakpoints(p_breakpoints);
    }
}

Dictionary EditorData::get_editor_states() const {

    Dictionary metadata;
    for (int i = 0; i < editor_plugins.size(); i++) {

        Dictionary state = editor_plugins[i]->get_state();
        if (state.empty()) {
            continue;
        }
        metadata[StringName(editor_plugins[i]->get_name())] = state;
    }

    return metadata;
}

Dictionary EditorData::get_scene_editor_states(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), Dictionary());
    EditedScene es = edited_scene[p_idx];
    return es.editor_states;
}

void EditorData::set_editor_states(const Dictionary &p_states) {

    auto keys(p_states.get_key_list());

    for (const auto & k : keys) {

        String name(k);
        int idx = -1;
        for (int i = 0; i < editor_plugins.size(); i++) {

            if (editor_plugins[i]->get_name() == StringView(name)) {
                idx = i;
                break;
            }
        }

        if (idx == -1) {
            continue;
        }
        editor_plugins[idx]->set_state(p_states[k].as<Dictionary>());
    }
}

void EditorData::notify_edited_scene_changed() {

    for (int i = 0; i < editor_plugins.size(); i++) {

        editor_plugins[i]->edited_scene_changed();
        editor_plugins[i]->notify_scene_changed(get_edited_scene_root());
    }
}

void EditorData::notify_resource_saved(const Ref<Resource> &p_resource) {

    for (int i = 0; i < editor_plugins.size(); i++) {

        editor_plugins[i]->notify_resource_saved(p_resource);
    }
}

void EditorData::clear_editor_states() {

    for (int i = 0; i < editor_plugins.size(); i++) {

        editor_plugins[i]->clear();
    }
}

void EditorData::save_editor_external_data() {

    for (int i = 0; i < editor_plugins.size(); i++) {

        editor_plugins[i]->save_external_data();
    }
}

void EditorData::apply_changes_in_editors() {

    for (int i = 0; i < editor_plugins.size(); i++) {

        editor_plugins[i]->apply_changes();
    }
}

void EditorData::save_editor_global_states() {

    for (int i = 0; i < editor_plugins.size(); i++) {

        editor_plugins[i]->save_global_state();
    }
}

void EditorData::restore_editor_global_states() {

    for (int i = 0; i < editor_plugins.size(); i++) {

        editor_plugins[i]->restore_global_state();
    }
}

void EditorData::paste_object_params(Object *p_object) {
    ERR_FAIL_NULL(p_object);
    undo_redo.create_action(TTR("Paste Params"));
    for (const auto &E : clipboard) {
        const StringName &name = E.name;
        undo_redo.add_do_property(p_object, name, E.value);
        undo_redo.add_undo_property(p_object, name, p_object->get(name));
    }
    undo_redo.commit_action();
}

bool EditorData::call_build() {

    bool result = true;

    for (size_t i = 0; i < editor_plugins.size() && result; i++) {

        result &= editor_plugins[i]->build();
        //FIXME: remove this when we're sure things are working.
        if(result==false)
            printf("I'm here to be a possible breakpoint location in case of plugin `build` failures");
    }

    return result;
}

UndoRedo &EditorData::get_undo_redo() {

    return undo_redo;
}

void EditorData::remove_editor_plugin(EditorPlugin *p_plugin) {

    p_plugin->undo_redo = nullptr;
    editor_plugins.erase_first(p_plugin);
}

void EditorData::add_editor_plugin(EditorPlugin *p_plugin) {

    p_plugin->undo_redo = &undo_redo;
    editor_plugins.push_back(p_plugin);
}

int EditorData::get_editor_plugin_count() const {
    return editor_plugins.size();
}
EditorPlugin *EditorData::get_editor_plugin(int p_idx) {

    ERR_FAIL_INDEX_V(p_idx, editor_plugins.size(), nullptr);
    return editor_plugins[p_idx];
}

void EditorData::add_custom_type(const StringName &p_type, const StringName &p_inherits, const Ref<Script> &p_script, const Ref<Texture> &p_icon) {

    ERR_FAIL_COND_MSG(not p_script, "It's not a reference to a valid Script object.");
    CustomType ct;
    ct.name = p_type;
    ct.icon = p_icon;
    ct.script = p_script;
    if (!custom_types.contains(p_inherits)) {
        custom_types[p_inherits] = {};
    }

    custom_types[p_inherits].push_back(ct);
}

Object *EditorData::instance_custom_type(const StringName &p_type, const StringName & p_inherits) {

    if (!get_custom_types().contains(p_inherits)) {
        return nullptr;
    }

    const Vector<CustomType> &ct(get_custom_types().at(p_inherits));
    for (int i = 0; i < get_custom_types().at(p_inherits).size(); i++) {
        if (ct[i].name == p_type) {
            Ref<Script> script = ct[i].script;

            Object *ob = ClassDB::instance(p_inherits);
            ERR_FAIL_COND_V(!ob, nullptr);
            if (ob->is_class("Node")) {
                ob->call_va("set_name", p_type);
            }
            ob->set_script(script.get_ref_ptr());
            return ob;
        }
    }

    return nullptr;
}

void EditorData::remove_custom_type(const StringName &p_type) {

    for (eastl::pair<const StringName,Vector<CustomType> > &E : custom_types) {

        for (int i = 0; i < E.second.size(); i++) {
            if (E.second[i].name == p_type) {
                E.second.erase_at(i);
                if (E.second.empty()) {
                    custom_types.erase(E.first);
                }
                return;
            }
        }
    }
}

int EditorData::add_edited_scene(int p_at_pos) {

    if (p_at_pos < 0) {
        p_at_pos = edited_scene.size();
    }
    EditedScene es;
    es.root = nullptr;
    es.path = String();
    es.file_modified_time = 0;
    es.history_current = -1;
    es.version = 0;
    es.live_edit_root = NodePath("/root");

    if (p_at_pos == edited_scene.size()) {
        edited_scene.push_back(es);
    } else {
        edited_scene.insert_at(p_at_pos, es);
    }

    if (current_edited_scene < 0) {
        current_edited_scene = 0;
    }
    return p_at_pos;
}

void EditorData::move_edited_scene_index(int p_idx, int p_to_idx) {

    ERR_FAIL_INDEX(p_idx, edited_scene.size());
    ERR_FAIL_INDEX(p_to_idx, edited_scene.size());
    SWAP(edited_scene[p_idx], edited_scene[p_to_idx]);
}

void EditorData::remove_scene(int p_idx) {
    ERR_FAIL_INDEX(p_idx, edited_scene.size());
    if (edited_scene[p_idx].root) {

        for (int i = 0; i < editor_plugins.size(); i++) {
            editor_plugins[i]->notify_scene_closed(edited_scene[p_idx].root->get_filename());
        }

        memdelete(edited_scene[p_idx].root);
    }

    if (current_edited_scene > p_idx) {
        current_edited_scene--;
    } else if (current_edited_scene == p_idx && current_edited_scene > 0) {
        current_edited_scene--;
    }

    if (!edited_scene[p_idx].path.empty()) {
        ScriptEditor::get_singleton()->close_builtin_scripts_from_scene(edited_scene[p_idx].path);
    }

    edited_scene.erase_at(p_idx);
}

bool EditorData::_find_updated_instances(Node *p_root, Node *p_node, Set<String> &checked_paths) {

    /*
    if (p_root!=p_node && p_node->get_owner()!=p_root && !p_root->is_editable_instance(p_node->get_owner()))
        return false;
    */

    Ref<SceneState> ss;

    if (p_node == p_root) {
        ss = p_node->get_scene_inherited_state();
    } else if (!p_node->get_filename().empty()) {
        ss = p_node->get_scene_instance_state();
    }

    if (ss) {
        String path = ss->get_path();

        if (!checked_paths.contains(path)) {

            uint64_t modified_time = FileAccess::get_modified_time(path);
            if (modified_time != ss->get_last_modified_time()) {
                return true; //external scene changed
            }

            checked_paths.insert(path);
        }
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {

        bool found = _find_updated_instances(p_root, p_node->get_child(i), checked_paths);
        if (found) {
            return true;
        }
    }

    return false;
}

bool EditorData::check_and_update_scene(int p_idx) {

    ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), false);
    if (!edited_scene[p_idx].root) {
        return false;
    }

    Set<String> checked_scenes;

    bool must_reload = _find_updated_instances(edited_scene[p_idx].root, edited_scene[p_idx].root, checked_scenes);

    if (must_reload) {
        Ref<PackedScene> pscene(make_ref_counted<PackedScene>());

        EditorProgress ep(("update_scene"), TTR("Updating Scene"), 2);
        ep.step(TTR("Storing local changes..."), 0);
        //pack first, so it stores diffs to previous version of saved scene
        Error err = pscene->pack(edited_scene[p_idx].root);
        ERR_FAIL_COND_V(err != OK, false);
        ep.step(TTR("Updating scene..."), 1);
        Node *new_scene = pscene->instance(GEN_EDIT_STATE_MAIN);
        ERR_FAIL_COND_V(!new_scene, false);

        //transfer selection
        Vector<Node *> new_selection;
        for (Node * E : edited_scene[p_idx].selection) {
            NodePath p = edited_scene[p_idx].root->get_path_to(E);
            Node *new_node = new_scene->get_node(p);
            if (new_node) {
                new_selection.push_back(new_node);
            }
        }

        new_scene->set_filename(edited_scene[p_idx].root->get_filename());

        memdelete(edited_scene[p_idx].root);
        edited_scene[p_idx].root = new_scene;
        if (!new_scene->get_filename().empty())
            edited_scene[p_idx].path = new_scene->get_filename();
        edited_scene[p_idx].selection = new_selection;

        return true;
    }

    return false;
}

int EditorData::get_edited_scene() const {

    return current_edited_scene;
}
void EditorData::set_edited_scene(int p_idx) {

    ERR_FAIL_INDEX(p_idx, edited_scene.size());
    current_edited_scene = p_idx;
    //swap
}
Node *EditorData::get_edited_scene_root(int p_idx) {
    if (p_idx < 0) {
        ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), nullptr);
        return edited_scene[current_edited_scene].root;
    } else {
        ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), nullptr);
        return edited_scene[p_idx].root;
    }
}
void EditorData::set_edited_scene_root(Node *p_root) {

    ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());
    edited_scene[current_edited_scene].root = p_root;
    if (p_root) {
        if (!p_root->get_filename().empty())
            edited_scene[current_edited_scene].path = p_root->get_filename();
        else
            p_root->set_filename(edited_scene[current_edited_scene].path);
    }
    if (!edited_scene[current_edited_scene].path.empty()) {
        edited_scene[current_edited_scene].file_modified_time = FileAccess::get_modified_time(edited_scene[current_edited_scene].path);
    }
}

int EditorData::get_edited_scene_count() const {

    return edited_scene.size();
}

const Vector<EditorData::EditedScene> &EditorData::get_edited_scenes() const {
    return edited_scene;
}

void EditorData::set_edited_scene_version(uint64_t version, int p_scene_idx) {
    ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());
    if (p_scene_idx < 0) {
        edited_scene[current_edited_scene].version = version;
    } else {
        ERR_FAIL_INDEX(p_scene_idx, edited_scene.size());
        edited_scene[p_scene_idx].version = version;
    }
}

uint64_t EditorData::get_edited_scene_version() const {

    ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), 0);
    return edited_scene[current_edited_scene].version;
}
uint64_t EditorData::get_scene_version(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), 0);
    return edited_scene[p_idx].version;
}

void EditorData::set_scene_modified_time(int p_idx, uint64_t p_time) {
    if (p_idx == -1) {
        p_idx = current_edited_scene;
    }

    ERR_FAIL_INDEX(p_idx, edited_scene.size());

    edited_scene[p_idx].file_modified_time = p_time;
}

uint64_t EditorData::get_scene_modified_time(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), 0);
    return edited_scene[p_idx].file_modified_time;
}
UIString EditorData::get_scene_type(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), UIString());
    if (!edited_scene[p_idx].root)
        return UIString();
    return UIString(edited_scene[p_idx].root->get_class());
}
void EditorData::move_edited_scene_to_index(int p_idx) {

    ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());
    ERR_FAIL_INDEX(p_idx, edited_scene.size());

    EditedScene es = eastl::move(edited_scene[current_edited_scene]);
    edited_scene.erase_at(current_edited_scene);
    edited_scene.insert_at(p_idx, eastl::move(es));
    current_edited_scene = p_idx;
}

Ref<Script> EditorData::get_scene_root_script(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), Ref<Script>());
    if (!edited_scene[p_idx].root)
        return Ref<Script>();
    Ref<Script> s(refFromRefPtr<Script>(edited_scene[p_idx].root->get_script()));
    if (not s && edited_scene[p_idx].root->get_child_count()) {
        Node *n = edited_scene[p_idx].root->get_child(0);
        while (not s && n && n->get_filename().empty()) {
            s = refFromRefPtr<Script>(n->get_script());
            n = n->get_parent();
        }
    }
    return s;
}

StringName EditorData::get_scene_title(int p_idx, bool p_always_strip_extension) const {
    ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), StringName());
    if (!edited_scene[p_idx].root) {
        return TTR("[empty]");
    }
    if (edited_scene[p_idx].root->get_filename().empty()) {
        return TTR("[unsaved]");
    }
    const String filename(PathUtils::get_file(edited_scene[p_idx].root->get_filename()));
    const String basename(PathUtils::get_basename(filename));

    if (p_always_strip_extension) {
        return StringName(basename);
    }

    // Return the filename including the extension if there's ambiguity (e.g. both `foo.tscn` and `foo.scn` are being edited).
    for (int i = 0; i < edited_scene.size(); i++) {
        if (i == p_idx) {
            // Don't compare the edited scene against itself.
            continue;
        }
        String ed_scen(PathUtils::get_basename(PathUtils::get_file(edited_scene[i].root->get_filename())));
        if (edited_scene[i].root && basename == ed_scen) {
            return StringName(filename);
        }
    }

    // Else, return just the basename as there's no ambiguity.
    return StringName(basename);
}

void EditorData::set_scene_path(int p_idx, StringView p_path) {

    ERR_FAIL_INDEX(p_idx, edited_scene.size());
    edited_scene[p_idx].path = p_path;

    if (!edited_scene[p_idx].root) {
        return;
    }
    edited_scene[p_idx].root->set_filename(p_path);
}

String EditorData::get_scene_path(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), String());

    if (edited_scene[p_idx].root) {
        if (edited_scene[p_idx].root->get_filename().empty())
            edited_scene[p_idx].root->set_filename(edited_scene[p_idx].path);
        else
            return String(edited_scene[p_idx].root->get_filename());
    }

    return edited_scene[p_idx].path;
}

void EditorData::set_edited_scene_live_edit_root(const NodePath &p_root) {
    ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());

    edited_scene[current_edited_scene].live_edit_root = p_root;
}
NodePath EditorData::get_edited_scene_live_edit_root() {

    ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), NodePath());

    return edited_scene[current_edited_scene].live_edit_root;
}

void EditorData::save_edited_scene_state(EditorSelection *p_selection, EditorHistory *p_history, const Dictionary &p_custom) {

    ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());

    EditedScene &es = edited_scene[current_edited_scene];
    es.selection = p_selection->get_full_selected_node_list();
    es.history_current = p_history->current;
    es.history_stored = p_history->history;
    es.editor_states = get_editor_states();
    es.custom_state = p_custom;
}

Dictionary EditorData::restore_edited_scene_state(EditorSelection *p_selection, EditorHistory *p_history) {
    ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), Dictionary());

    EditedScene &es = edited_scene[current_edited_scene];

    p_history->current = es.history_current;
    p_history->history = es.history_stored;

    p_selection->clear();
    for (Node * E : es.selection) {
        p_selection->add_node(E);
    }
    set_editor_states(es.editor_states);

    return es.custom_state;
}

void EditorData::clear_edited_scenes() {

    for (int i = 0; i < edited_scene.size(); i++) {
        if (edited_scene[i].root) {
            memdelete(edited_scene[i].root);
        }
    }
    edited_scene.clear();
}

void EditorData::set_plugin_window_layout(const Ref<ConfigFile>& p_layout) {
    for (int i = 0; i < editor_plugins.size(); i++) {
        editor_plugins[i]->set_window_layout(p_layout);
    }
}

void EditorData::get_plugin_window_layout(const Ref<ConfigFile>& p_layout) {
    for (int i = 0; i < editor_plugins.size(); i++) {
        editor_plugins[i]->get_window_layout(p_layout);
    }
}

bool EditorData::script_class_is_parent(const StringName & p_class, const StringName &p_inherits) {
    if (!ScriptServer::is_global_class(p_class)) {
        return false;
    }
    StringName base = script_class_get_base(p_class);

    while (base != p_inherits) {
        if (ClassDB::class_exists(base)) {
            return ClassDB::is_parent_class(base, p_inherits);
        }
        if (!ScriptServer::is_global_class(base)) {
            return false;
        }
        base = ScriptServer::get_global_class_base(base);
    }
    return true;
}

StringName EditorData::script_class_get_base(const StringName &p_class) const {

    Ref<Script> script = script_class_load_script(p_class);

    if (not script) {
        return StringName();
    }

    Ref<Script> base_script = script->get_base_script();
    if (not base_script) {
        return ScriptServer::get_global_class_base(p_class);
    }

    return script->get_language()->get_global_class_name(base_script->get_path());
}

Object *EditorData::script_class_instance(const StringName & p_class) {
    if (ScriptServer::is_global_class(p_class)) {
        Object *obj = ClassDB::instance(ScriptServer::get_global_class_native_base(p_class));
        if (obj) {
            Ref<Script> script = script_class_load_script(p_class);
            if (script)
                obj->set_script(script.get_ref_ptr());
            return obj;
        }
    }
    return nullptr;
}
Ref<Script> EditorData::script_class_load_script(StringName p_class) const {

    if (!ScriptServer::is_global_class(p_class)) {
        return Ref<Script>();
    }

    StringView path = ScriptServer::get_global_class_path(p_class);
    return dynamic_ref_cast<Script>(gResourceManager().load(path, "Script"));
}

void EditorData::script_class_set_icon_path(const StringName & p_class, StringView p_icon_path) {
    _script_class_icon_paths[p_class] = p_icon_path;
}

String EditorData::script_class_get_icon_path(const StringName &p_class) const {
    if (!ScriptServer::is_global_class(p_class)) {
        return String();
    }

    StringName current(p_class);
    String ret = _script_class_icon_paths.at(p_class,String());
    while (ret.empty()) {
        current = script_class_get_base(StringName(current));
        if (!ScriptServer::is_global_class(current))
            return String();
        ret = _script_class_icon_paths.at(current,String());
    }

    return ret;
}

StringName EditorData::script_class_get_name(StringView p_path) const {
    auto iter = _script_class_file_to_path.find_as(p_path);
    if(iter==_script_class_file_to_path.end())
        return StringName();
    return iter->second;
}

void EditorData::script_class_set_name(StringView p_path, const StringName &p_class) {
    _script_class_file_to_path[String(p_path)] = p_class;
}

void EditorData::script_class_save_icon_paths() {
    Dictionary d;
    for (const auto &E : _script_class_icon_paths) {
        if (ScriptServer::is_global_class(E.first))
            d[E.first] = _script_class_icon_paths[E.first];
    }

    Dictionary old;
    if (ProjectSettings::get_singleton()->has_setting("_global_script_class_icons")) {
        old = ProjectSettings::get_singleton()->getT<Dictionary>("_global_script_class_icons");
    }
    if ((!old.empty() || d.empty()) && d.hash() == old.hash()) {
        return;
    }
    if (d.empty()) {
        if (ProjectSettings::get_singleton()->has_setting("_global_script_class_icons")) {
            ProjectSettings::get_singleton()->clear("_global_script_class_icons");
        }
    } else {
        ProjectSettings::get_singleton()->set("_global_script_class_icons", d);
    }
    ProjectSettings::get_singleton()->save();
}

void EditorData::script_class_load_icon_paths() {
    script_class_clear_icon_paths();

    if (ProjectSettings::get_singleton()->has_setting("_global_script_class_icons")) {
        Dictionary d = ProjectSettings::get_singleton()->getT<Dictionary>("_global_script_class_icons");
        auto keys(d.get_key_list());

        for (const auto & name : keys) {
            _script_class_icon_paths[name] = d[name].as<String>();

            script_class_set_name(ScriptServer::get_global_class_path(name), name);
        }
    }
}

EditorData::EditorData() {

    current_edited_scene = -1;

    //load_imported_scenes_from_globals();
    script_class_load_icon_paths();
}

///////////
void EditorSelection::_node_removed(Node *p_node) {

    if (!selection.contains(p_node)) {
        return;
    }

    Object *meta = selection[p_node];
    memdelete(meta);
    selection.erase(p_node);
    changed = true;
    nl_changed = true;
}

void EditorSelection::add_node(Node *p_node) {

    ERR_FAIL_NULL(p_node);
    ERR_FAIL_COND(!p_node->is_inside_tree());
    if (selection.contains(p_node)) {
        return;
    }

    changed = true;
    nl_changed = true;
    Object *meta = nullptr;
    for (Object * E :editor_plugins) {

        meta = E->call_va("_get_editor_data", Variant(p_node)).as<Object *>();
        if (meta) {
            break;
        }
    }
    selection[p_node] = meta;

    p_node->connect("tree_exiting",callable_gen(this, [=]() { _node_removed(p_node); }), ObjectNS::CONNECT_ONESHOT);

    //emit_signal("selection_changed");
}

void EditorSelection::remove_node(Node *p_node) {

    ERR_FAIL_NULL(p_node);

    if (!selection.contains(p_node)) {
        return;
    }

    changed = true;
    nl_changed = true;
    Object *meta = selection[p_node];
    memdelete(meta);
    selection.erase(p_node);
    p_node->disconnect_all("tree_exiting",this->get_instance_id());
    //emit_signal("selection_changed");
}
bool EditorSelection::is_selected(Node *p_node) const {

    return selection.contains(p_node);
}

Array EditorSelection::_get_transformable_selected_nodes() {

    Array ret;

    for (Node *E : selected_node_list) {
        ret.push_back(Variant::from(E));
    }

    return ret;
}

Array EditorSelection::get_selected_nodes() {

    Array ret;

    for (const eastl::pair< Node *const,Object *> &E : selection) {

        ret.push_back(Variant(E.first));
    }

    return ret;
}

void EditorSelection::_bind_methods() {

    SE_BIND_METHOD(EditorSelection,clear);
    SE_BIND_METHOD(EditorSelection,add_node);
    SE_BIND_METHOD(EditorSelection,remove_node);
    SE_BIND_METHOD(EditorSelection,get_selected_nodes);
    MethodBinder::bind_method(D_METHOD("get_transformable_selected_nodes"), &EditorSelection::_get_transformable_selected_nodes);
    ADD_SIGNAL(MethodInfo("selection_changed"));
}

void EditorSelection::add_editor_plugin(Object *p_object) {

    editor_plugins.push_back(p_object);
}

void EditorSelection::_update_nl() {

    if (!nl_changed) {
        return;
    }

    selected_node_list.clear();

    for (eastl::pair< Node *,Object *> E : selection) {

        Node *parent = E.first;
        parent = parent->get_parent();
        bool skip = false;
        while (parent) {
            if (selection.contains(parent)) {
                skip = true;
                break;
            }
            parent = parent->get_parent();
        }

        if (skip) {
            continue;
        }
        selected_node_list.push_back(E.first);
    }

    nl_changed = true;
}

void EditorSelection::update() {

    _update_nl();

    if (!changed) {
        return;
    }
    changed = false;
    if (!emitted) {
        emitted = true;
        call_deferred([this]() {_emit_change();});
    }
}

void EditorSelection::_emit_change() {
    emit_signal("selection_changed");
    emitted = false;
}
//! \note This method only returns nodes with common parent.
const Vector<Node *> &EditorSelection::get_selected_node_list() {

    if (changed)
        update();
    else
        _update_nl();
    return selected_node_list;
}
Vector<Node *> EditorSelection::get_full_selected_node_list() {

    Vector<Node *> node_list;
    node_list.reserve(selection.size());

    for (const auto &E : selection) {
        node_list.emplace_back(E.first);
    }

    return node_list;
}
void EditorSelection::clear() {

    while (!selection.empty()) {

        remove_node(selection.begin()->first);
    }

    changed = true;
    nl_changed = true;
}
EditorSelection::EditorSelection() {

    emitted = false;
    changed = false;
    nl_changed = false;
}

EditorSelection::~EditorSelection() {

    clear();
}
