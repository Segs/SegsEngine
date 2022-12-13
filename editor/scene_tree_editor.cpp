/*************************************************************************/
/*  scene_tree_editor.cpp                                                */
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

#include "scene_tree_editor.h"

#include "core/script_language.h"
#include "editor/plugins/script_editor_plugin.h"
#include "node_dock.h"
#include "scene/gui/texture_rect.h"

#include "core/method_bind.h"
#include "core/message_queue.h"
#include "core/callable_method_pointer.h"
#include "core/object_db.h"
#include "core/string_formatter.h"
#include "core/string_utils.inl"
#include "editor/editor_data.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/editor_file_system.h"
#include "editor_node.h"
#include "scene/gui/label.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/packed_scene.h"
#include "scene/main/canvas_layer.h"

IMPL_GDCLASS(SceneTreeEditor)
IMPL_GDCLASS(SceneTreeDialog)

namespace {
    [[nodiscard]] Node *get_scene_node(const SceneTreeEditor* self) {

        ERR_FAIL_COND_V(!self || !self->is_inside_tree(), nullptr);

        return self->get_tree()->get_edited_scene_root();
    }
    static void update_node_tooltip(SceneTreeEditor* self, Node* p_node, TreeItem* item)
    {
        // Display the node name in all tooltips so that long node names can be previewed
        // without having to rename them.
        if (p_node == get_scene_node(self) && p_node->get_scene_inherited_state()) {
            item->add_button(0, self->get_theme_icon("InstanceOptions", "EditorIcons"), SceneTreeEditor::BUTTON_SUBSCENE, false, TTR("Open in Editor"));
            String tooltip(String(p_node->get_name()) + "\n" + TTR("Inherits:") + " " + p_node->get_scene_inherited_state()->get_path() + "\n" + TTR("Type:") + " " + p_node->get_class());
            if (!p_node->get_editor_description().empty()) {
                tooltip += "\n\n" + p_node->get_editor_description();
            }

            item->set_tooltip(0, StringName(tooltip));
        }
        else if (p_node != get_scene_node(self) && !p_node->get_filename().empty() && self->can_open_instances()) {
            item->add_button(0, self->get_theme_icon("InstanceOptions", "EditorIcons"), SceneTreeEditor::BUTTON_SUBSCENE, false, TTR("Open in Editor"));

            String tooltip(String(p_node->get_name()) + "\n" + TTR("Instance:") + " " + p_node->get_filename() + "\n" + TTR("Type:") + " " + p_node->get_class());
            if (!p_node->get_editor_description().empty()) {
                tooltip += "\n\n" + p_node->get_editor_description();
            }

            item->set_tooltip(0, StringName(tooltip));
        }
        else {
            StringName type(EditorNode::get_singleton()->get_object_custom_type_name(p_node));
            if (type == StringName()) {
                type = p_node->get_class_name();
            }

            String tooltip(String(p_node->get_name()) + "\n" + TTR("Type:") + " " + type);
            if (!p_node->get_editor_description().empty()) {
                tooltip += "\n\n" + p_node->get_editor_description();
            }

            item->set_tooltip(0, StringName(tooltip));
        }
    }
    void _toggle_visible(UndoRedo* undo_redo, Node* p_node) {
        if (!p_node->has_method("is_visible") || !p_node->has_method("set_visible")) {
            return;
        }

        bool v = p_node->call_va("is_visible").as<bool>();
        undo_redo->add_do_method(p_node, "set_visible", !v);
        undo_redo->add_undo_method(p_node, "set_visible", v);
    }

    void _compute_hash(Node* p_node, uint64_t& hash) {

        hash = hash_djb2_one_64(entt::to_integral(p_node->get_instance_id()), hash);
        if (p_node->get_parent()) {
            hash = hash_djb2_one_64(entt::to_integral(p_node->get_parent()->get_instance_id()), hash); //so a reparent still produces a different hash
        }

        for (int i = 0; i < p_node->get_child_count(); i++) {
            _compute_hash(p_node->get_child(i), hash);
        }
    }
    void _update_visibility_color(Node* p_node, TreeItem* p_item) {
        if (!p_node->is_class("CanvasItem") && !p_node->is_class("Node3D"))
            return;

        Color color(1, 1, 1, 1);
        bool visible_on_screen = p_node->call_va("is_visible_in_tree").as<bool>();
        if (!visible_on_screen) {
            color.a = 0.6f;
        }
        int idx = p_item->get_button_by_id(0, SceneTreeEditor::BUTTON_VISIBILITY);
        p_item->set_button_color(0, idx, color);
    }


} //end of anonymous namespace
struct SceneTreeEditorImpl
{
    static void on_groups_or_signals_pressed(SceneTreeEditor* tgt, Node* n, bool groups)
    {
        tgt->editor_selection->clear();
        tgt->editor_selection->add_node(n);

        tgt->set_selected(n);

        NodeDock::singleton->get_parent()->call_va("set_current_tab", NodeDock::singleton->get_index());
        if (groups) {
            NodeDock::singleton->show_groups();
        }
        else {
            NodeDock::singleton->show_connections();
        }
    }
    static void on_warning_pressed(SceneTreeEditor* tgt, Node* n)
    {
        UIString config_err(StringUtils::from_utf8(n->get_configuration_warning()));
        if (config_err.isEmpty()) {
            return;
        }
        config_err = StringUtils::word_wrap(config_err, 80);
        tgt->warning->set_text(StringName(StringUtils::to_utf8(config_err)));
        tgt->warning->popup_centered_minsize();
    }
    static void on_subscene_pressed(SceneTreeEditor* tgt, Node* n)
    {
        if (n == get_scene_node(tgt)) {
            if (n && n->get_scene_inherited_state()) {
                tgt->emit_signal("open", n->get_scene_inherited_state()->get_path());
            }
        }
        else {
            tgt->emit_signal("open", n->get_filename());
        }
    }

    static void on_script_pressed(SceneTreeEditor* tgt, Node* n)
    {
        RefPtr script = n->get_script();
        Ref<Script> script_typed(refFromRefPtr<Script>(script));
        if (script_typed) {
            tgt->emit_signal("open_script", Variant(script));
        }
    }

    static void on_visibility_pressed(SceneTreeEditor* tgt, Node* n)
    {
        tgt->undo_redo->create_action(TTR("Toggle Visible"));
        _toggle_visible(tgt->undo_redo, n);
        const Vector<Node*>& selection = tgt->editor_selection->get_selected_node_list();
        if (selection.size() > 1 && selection.find(n) != nullptr) {
            for (Node* nv : selection) {
                ERR_FAIL_COND(!nv);
                if (nv == n) {
                    continue;
                }
                _toggle_visible(tgt->undo_redo, nv);
            }
        }
        tgt->undo_redo->commit_action();
    }
    static void on_lock_pressed(SceneTreeEditor* tgt, Node* n)
    {
        tgt->undo_redo->create_action(TTR("Unlock Node"));

        if (n->is_class("CanvasItem") || n->is_class("Node3D")) {

            tgt->undo_redo->add_do_method(n, "remove_meta", "_edit_lock_");
            tgt->undo_redo->add_do_method(tgt, "_update_tree", Variant());
            tgt->undo_redo->add_do_method(tgt, "emit_signal", "node_changed");
            tgt->undo_redo->add_undo_method(n, "set_meta", "_edit_lock_", true);
            tgt->undo_redo->add_undo_method(tgt, "_update_tree", Variant());
            tgt->undo_redo->add_undo_method(tgt, "emit_signal", "node_changed");
        }
        tgt->undo_redo->commit_action();
    }

    static void on_group_pressed(SceneTreeEditor* tgt, Node* n)
    {
        tgt->undo_redo->create_action(TTR("Button Group"));

        if (n->is_class("CanvasItem") || n->is_class("Node3D")) {

            tgt->undo_redo->add_do_method(n, "remove_meta", "_edit_group_");
            tgt->undo_redo->add_undo_method(n, "set_meta", "_edit_group_", true);
            tgt->undo_redo->add_do_method(tgt, "_update_tree", Variant());
            tgt->undo_redo->add_undo_method(tgt, "_update_tree", Variant());
            tgt->undo_redo->add_do_method(tgt, "emit_signal", "node_changed");
            tgt->undo_redo->add_undo_method(tgt, "emit_signal", "node_changed");
        }
        tgt->undo_redo->commit_action();
    }
    static void on_pin_pressed(SceneTreeEditor* tgt, Node* n)
    {
        if (n->is_class("AnimationPlayer")) {
            AnimationPlayerEditor::singleton->unpin();
            tgt->_update_tree();
        }
    }
    static void on_unique_pressed(SceneTreeEditor *tgt, Node *n) {
        tgt->undo_redo->create_action(TTR("Disable Scene Unique Name"));
        tgt->undo_redo->add_do_method(n, "set_unique_name_in_owner", false);
        tgt->undo_redo->add_undo_method(n, "set_unique_name_in_owner", true);
        tgt->undo_redo->add_do_method(tgt, "_update_tree");
        tgt->undo_redo->add_undo_method(tgt, "_update_tree");
        tgt->undo_redo->commit_action();
    }
};

void SceneTreeEditor::_cell_button_pressed(Object *p_item, int p_column, int p_id) {

    if (connect_to_script_mode) {
        return; //don't do anything in this mode
    }

    TreeItem *item = object_cast<TreeItem>(p_item);
    ERR_FAIL_COND(!item);

    Node *n = get_node(item->get_metadata(0).as<NodePath>());

    ERR_FAIL_COND(!n);
    ButtonId action_id((ButtonId)p_id);
    switch (action_id)
    {
    case BUTTON_SUBSCENE: SceneTreeEditorImpl::on_subscene_pressed(this,n);  break;
    case BUTTON_VISIBILITY: SceneTreeEditorImpl::on_visibility_pressed(this, n); break;
    case BUTTON_SCRIPT: SceneTreeEditorImpl::on_script_pressed(this, n);  break;
    case BUTTON_LOCK: SceneTreeEditorImpl::on_lock_pressed(this, n); break;
    case BUTTON_GROUP: SceneTreeEditorImpl::on_group_pressed(this, n); break;
    case BUTTON_WARNING: SceneTreeEditorImpl::on_warning_pressed(this,n);  break;
    case BUTTON_SIGNALS: SceneTreeEditorImpl::on_groups_or_signals_pressed(this, n, false); break;
    case BUTTON_GROUPS: SceneTreeEditorImpl::on_groups_or_signals_pressed(this,n,true); break;
    case BUTTON_PIN: SceneTreeEditorImpl::on_pin_pressed(this, n); break;
    case BUTTON_UNIQUE: SceneTreeEditorImpl::on_unique_pressed(this, n); break;
    }
}

bool SceneTreeEditor::_add_nodes(Node *p_node, TreeItem *p_parent, bool p_scroll_to_selected) {

    if (!p_node) {
        return false;
    }

    // only owned nodes are editable, since nodes can create their own (manually owned) child nodes,
    // which the editor needs not to know about.

    bool part_of_subscene;
    Node *scene_node= get_scene_node(this);
    if (!display_foreign && p_node->get_owner() != scene_node && p_node != scene_node) {

        if ((!show_enabled_subscene && !can_open_instance) || !p_node->get_owner() ||
                !scene_node->is_editable_instance(p_node->get_owner())) {
            return false;
        }
        //allow
        part_of_subscene = true;
    } else {
        part_of_subscene = p_node != scene_node && scene_node->get_scene_inherited_state() && scene_node->get_scene_inherited_state()->find_node_by_path(scene_node->get_path_to(p_node)) >= 0;
    }

    TreeItem *item = tree->create_item(p_parent);

    item->set_text_utf8(0, p_node->get_name());
    item->set_selectable(0, true);
    if (can_rename) {
        if (!part_of_subscene) {
            item->set_editable(0, true);
        }
        bool collapsed = p_node->is_displayed_folded();
        if (collapsed) {
            item->set_collapsed(true);
        }
    }

    Ref<Texture> icon = EditorNode::get_singleton()->get_object_icon(p_node, "Node");
    item->set_icon(0, icon);
    item->set_metadata(0, p_node->get_path());

    if (connect_to_script_mode) {
        Color accent = get_theme_color("accent_color", "Editor");

        Ref<Script> script(refFromRefPtr<Script>(p_node->get_script()));
        if (script && EditorNode::get_singleton()->get_object_custom_type_base(p_node) != script) {
            //has script
            item->add_button(0, get_theme_icon("Script", "EditorIcons"), BUTTON_SCRIPT);
        } else {
            //has no script (or script is a custom type)
            item->set_custom_color(0, get_theme_color("disabled_font_color", "Editor"));
            item->set_selectable(0, false);

            if (script) { // make sure to mark the script if a custom type
                item->add_button(0, get_theme_icon("Script", "EditorIcons"), BUTTON_SCRIPT);
                item->set_button_disabled(0, item->get_button_count(0) - 1, true);
            }

            accent.a *= 0.7f;
        }

        if (marked.contains(p_node)) {
            String node_name(p_node->get_name());
            if (connecting_signal) {
                node_name += " " + TTR("(Connecting From)");
            }
            item->set_text_utf8(0, node_name);
            item->set_custom_color(0, accent);
        }
    } else if (part_of_subscene) {

        if (valid_types.empty()) {
            item->set_custom_color(0, get_theme_color("disabled_font_color", "Editor"));
        }
    } else if (marked.contains(p_node)) {

        String node_name(p_node->get_name());
        if (connecting_signal) {
            node_name += " " + TTR("(Connecting From)");
        }
        item->set_text_utf8(0, node_name);
        item->set_selectable(0, marked_selectable);
        item->set_custom_color(0, get_theme_color("accent_color", "Editor"));
    } else if (!marked_selectable && !marked_children_selectable) {

        Node *node = p_node;
        while (node) {
            if (marked.contains(node)) {
                item->set_selectable(0, false);
                item->set_custom_color(0, get_theme_color("error_color", "Editor"));
                break;
            }
            node = node->get_parent();
        }
    }

    if (can_rename) { //should be can edit..

        String warning = p_node->get_configuration_warning();
        if (!warning.empty()) {
            item->add_button(0, get_theme_icon("NodeWarning", "EditorIcons"), BUTTON_WARNING, false, TTR("Node configuration warning:") + "\n" + p_node->get_configuration_warning());
        }
//        if (p_node->is_unique_name_in_owner()) {
//            item->add_button(0, get_theme_icon("SceneUniqueName", "EditorIcons"), BUTTON_UNIQUE, false,
//                    FormatVE(TTR("This node can be accessed from within anywhere in the scene by preceding it with the '%s' prefix in a node "
//                                 "path.\nClick to disable this.")
//                                     .asCString(),
//                            UNIQUE_NODE_PREFIX));
//        }

        int num_connections = p_node->get_persistent_signal_connection_count();
        int num_groups = p_node->get_persistent_group_count();

        if (num_connections >= 1 && num_groups >= 1) {
            item->add_button(
                    0,
                    get_theme_icon("SignalsAndGroups", "EditorIcons"),
                    BUTTON_SIGNALS,
                    false,
                    FormatSN(TTR("Node has %d connection(s) and %d group(s).\nClick to show signals dock.").asCString(), num_connections, num_groups));
        } else if (num_connections >= 1) {
            item->add_button(
                    0,
                    get_theme_icon("Signals", "EditorIcons"),
                    BUTTON_SIGNALS,
                    false,
                    FormatSN(TTR("Node has %d connection(s).\nClick to show signals dock.").asCString(), num_connections));
        } else if (num_groups >= 1) {
            item->add_button(
                    0,
                    get_theme_icon("Groups", "EditorIcons"),
                    BUTTON_GROUPS,
                    false,
                    FormatSN(TTR("Node is in %d group(s).\nClick to show groups dock.").asCString(), num_groups));
        }
    }

    update_node_tooltip(this,p_node, item);

    auto visibility_change_handler = callable_gen(this, [=]() { _node_visibility_changed(p_node); });
    if (can_open_instance && undo_redo) { //Show buttons only when necessary(SceneTreeDock) to avoid crashes

        if (!p_node->is_connected("script_changed",callable_mp(this, &SceneTreeEditor::_node_script_changed))) {
            p_node->connectF("script_changed",this,[=]() { _node_script_changed(p_node);});
        }

        Ref<Script> script(refFromRefPtr<Script>(p_node->get_script()));
        if (script) {
            String additional_notes;
            Color button_color = Color(1, 1, 1);
            // Can't set tooltip after adding button, need to do it before.
            if (script->is_tool()) {
                additional_notes += "\n" + TTR("This script is currently running in the editor.");
                button_color = get_theme_color("accent_color", "Editor");
            }
            if (EditorNode::get_singleton()->get_object_custom_type_base(p_node) == script) {
                additional_notes += "\n" + TTR("This script is a custom type.");
                button_color.a = 0.5;
            }
            item->add_button(0, get_theme_icon("Script", "EditorIcons"), BUTTON_SCRIPT, false, TTR("Open Script:") + " " + script->get_path() + additional_notes);
            item->set_button_color(0, item->get_button_count(0) - 1, button_color);
        }
        bool is_canvas_item= p_node->is_class("CanvasItem");
        bool is_node3d_item = p_node->is_class("Node3D");
        if (is_canvas_item || is_node3d_item)
        {
            bool is_locked = p_node->has_meta("_edit_lock_"); //_edit_group_
            if (is_locked) {
                item->add_button(0, get_theme_icon("Lock", "EditorIcons"), BUTTON_LOCK, false, TTR("Node is locked.\nClick to unlock it."));
            }
            bool is_grouped = p_node->has_meta("_edit_group_");
            if (is_grouped) {
                item->add_button(0, get_theme_icon("Group", "EditorIcons"), BUTTON_GROUP, false, TTR("Children are not selectable.\nClick to make them selectable."));
            }

            const StaticCString icon_name(p_node->call_va("is_visible").as<bool>() ? "GuiVisibilityVisible" : "GuiVisibilityHidden", true);
            item->add_button(0, get_theme_icon(icon_name, "EditorIcons"), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
            if (!p_node->is_connected("visibility_changed", visibility_change_handler)) {
                p_node->connect("visibility_changed", visibility_change_handler);
            }
            _update_visibility_color(p_node, item);
        } else if (p_node->is_class("CanvasLayer")) {
            auto *lr = object_cast<CanvasLayer>(p_node);
            bool v = lr->is_visible();
            if (v) {
                item->add_button(0, get_theme_icon("GuiVisibilityVisible", "EditorIcons"), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
            } else {
                item->add_button(0, get_theme_icon("GuiVisibilityHidden", "EditorIcons"), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
            }

            if (!p_node->is_connected("visibility_changed", visibility_change_handler)) {
                p_node->connect("visibility_changed", visibility_change_handler);
            }
        } else if (p_node->is_class("AnimationPlayer")) {

            bool is_pinned = (void*)AnimationPlayerEditor::singleton->get_player() == (void*)p_node && AnimationPlayerEditor::singleton->is_pinned();

            if (is_pinned) {
                item->add_button(0, get_theme_icon("Pin", "EditorIcons"), BUTTON_PIN, false, TTR("AnimationPlayer is pinned.\nClick to unpin."));
            }
        }
    }

    bool scroll = false;
    if (editor_selection && editor_selection->is_selected(p_node)) {
        item->select(0);
        scroll = p_scroll_to_selected;
    }

    if (selected == p_node) {
        if (!editor_selection) {
            item->select(0);
            scroll = p_scroll_to_selected;
        }
        item->set_as_cursor(0);
    }

    bool keep = is_subsequence_of(UIString(p_node->get_name()),filter,StringUtils::CaseInsensitive);

    for (int i = 0; i < p_node->get_child_count(); i++) {

        bool child_keep = _add_nodes(p_node->get_child(i), item, p_scroll_to_selected);

        keep = keep || child_keep;
    }

    if (!valid_types.empty()) {
        bool valid = false;
        for (const StringName &vt : valid_types) {
            if (p_node->is_class(vt)) {
                valid = true;
                break;
            }
        }

        if (!valid) {
            //item->set_selectable(0,marked_selectable);
            item->set_custom_color(0, get_theme_color("disabled_font_color", "Editor"));
            item->set_selectable(0, false);
        }
    }

    if (keep) {
        if (scroll) {
            tree->scroll_to_item(item);
        }
        return true;
    }

    if (editor_selection) {
        Node *n = get_node(item->get_metadata(0).as<NodePath>());
        if (n) {
            editor_selection->remove_node(n);
        }
    }
    memdelete(item);
    return false;
}

void SceneTreeEditor::_node_visibility_changed(Node *p_node) {

    if (!p_node || (p_node != get_scene_node(this) && !p_node->get_owner())) {
        return;
    }

    TreeItem *item = _find(tree->get_root(), p_node->get_path());

    if (!item) {
        return;
    }

    int idx = item->get_button_by_id(0, BUTTON_VISIBILITY);
    ERR_FAIL_COND(idx == -1);

    bool visible = false;

    if (p_node->is_class("CanvasItem")) {
        visible = p_node->call_va("is_visible").as<bool>();
        CanvasItemEditor::get_singleton()->get_viewport_control()->update();
    } else if (p_node->is_class("CanvasLayer")) {
        auto *layer = object_cast<CanvasLayer>(p_node);
        visible = layer->is_visible();
        CanvasItemEditor::get_singleton()->get_viewport_control()->update();
    } else if (p_node->is_class("Node3D")) {
        visible = p_node->call_va("is_visible").as<bool>();
    }

    const StaticCString icon_name(visible ? "GuiVisibilityVisible" : "GuiVisibilityHidden", true);
    item->set_button(0, idx, get_theme_icon(icon_name, "EditorIcons"));

    _update_visibility_color(p_node, item);
}

void SceneTreeEditor::_node_script_changed(Node *p_node) {
    if (tree_dirty) {
        return;
    }

    MessageQueue::get_singleton()->push_call(get_instance_id(),[this]() { _update_tree(); });
    tree_dirty = true;
}

void SceneTreeEditor::_node_removed(Node *p_node) {

    if (EditorNode::get_singleton()->is_exiting())
        return; //speed up exit

    if (p_node->is_connected("script_changed",callable_mp(this, &ClassName::_node_script_changed)))
        p_node->disconnect("script_changed",callable_mp(this, &ClassName::_node_script_changed));

    if (p_node->is_class("Node3D") || p_node->is_class("CanvasItem") || p_node->is_class("CanvasLayer")) {
        if (p_node->is_connected("visibility_changed",callable_mp(this, &ClassName::_node_visibility_changed)))
            p_node->disconnect("visibility_changed",callable_mp(this, &ClassName::_node_visibility_changed));
    }

    if (p_node == selected) {
        selected = nullptr;
        emit_signal("node_selected");
    }
}

void SceneTreeEditor::_node_renamed(Node *p_node) {
    if (p_node != get_scene_node(this) && !get_scene_node(this)->is_a_parent_of(p_node)) {
        return;
    }
    emit_signal("node_renamed");

    if (!tree_dirty) {
        MessageQueue::get_singleton()->push_call(this, "_update_tree");
        tree_dirty = true;
    }
}

void SceneTreeEditor::_update_tree(bool p_scroll_to_selected) {

    if (!is_inside_tree()) {
        tree_dirty = false;
        return;
    }

    updating_tree = true;
    tree->clear();
    Node *scene_node = get_scene_node(this);
    if (scene_node) {
        _add_nodes(scene_node, nullptr, p_scroll_to_selected);
        last_hash = hash_djb2_one_64(0);
        _compute_hash(scene_node, last_hash);
    }
    updating_tree = false;
    tree_dirty = false;
}

void SceneTreeEditor::_test_update_tree() {

    pending_test_update = false;
    // don't even bother if not in tree or it's not updated.
    if (tree_dirty || !is_inside_tree()) {
        return;
    }

    uint64_t hash = hash_djb2_one_64(0);
    Node *scene_node= get_scene_node(this);
    if (scene_node) {
        _compute_hash(scene_node, hash);
    }
    //test hash
    if (hash == last_hash) {
        return; // did not change
    }

    MessageQueue::get_singleton()->push_call(get_instance_id(),[this]() { _update_tree(); });
    tree_dirty = true;
}

void SceneTreeEditor::_tree_changed() {

    if (EditorNode::get_singleton()->is_exiting()) {
        return; //speed up exit
    }
    if (pending_test_update || tree_dirty) {
        return;
    }
    MessageQueue::get_singleton()->push_call(this->get_instance_id(),[this]() {_test_update_tree();});
    pending_test_update = true;
}

void SceneTreeEditor::_selected_changed() {

    TreeItem *s = tree->get_selected();
    ERR_FAIL_COND(!s);

    Node *n = get_node(s->get_metadata(0).as<NodePath>());

    if (n == selected) {
        return;
    }
    selected = n;

    blocked++;
    emit_signal("node_selected");
    blocked--;
}

void SceneTreeEditor::_deselect_items() {

    // Clear currently selected items in scene tree dock.
    if (editor_selection) {
        editor_selection->clear();
        emit_signal("node_changed");
    }
}

void SceneTreeEditor::_cell_multi_selected(Object *p_object, int p_cell, bool p_selected) {

    TreeItem *item = object_cast<TreeItem>(p_object);
    ERR_FAIL_COND(!item);

    Node *n = get_node(item->get_metadata(0).as<NodePath>());

    if (!n || !editor_selection) {
        return;
    }

    if (p_selected) {
        editor_selection->add_node(n);
    } else {
        editor_selection->remove_node(n);
    }
    emit_signal("node_changed");
}

void SceneTreeEditor::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            SceneTree* s_tree = get_tree();

            s_tree->connect("tree_changed",callable_mp(this, &SceneTreeEditor::_tree_changed));
            s_tree->connect("node_removed",callable_mp(this, &SceneTreeEditor::_node_removed));
            s_tree->connect("node_renamed",callable_mp(this, &SceneTreeEditor::_node_renamed));
            s_tree->connect("node_configuration_warning_changed",callable_mp(this, &SceneTreeEditor::_warning_changed));
            tree->connect("item_collapsed",callable_mp(this, &SceneTreeEditor::_cell_collapsed));

            _update_tree();
        } break;
        case NOTIFICATION_EXIT_TREE: {
            SceneTree* s_tree = get_tree();
            s_tree->disconnect("tree_changed",callable_mp(this, &SceneTreeEditor::_tree_changed));
            s_tree->disconnect("node_removed",callable_mp(this, &SceneTreeEditor::_node_removed));
            s_tree->disconnect("node_renamed",callable_mp(this, &SceneTreeEditor::_node_renamed));
            s_tree->disconnect("node_configuration_warning_changed",callable_mp(this, &SceneTreeEditor::_warning_changed));
            tree->disconnect("item_collapsed", callable_mp(this, &SceneTreeEditor::_cell_collapsed));
        } break;
        case NOTIFICATION_THEME_CHANGED: {
            _update_tree();
        } break;
    }
}

TreeItem *SceneTreeEditor::_find(TreeItem *p_node, const NodePath &p_path) {

    if (!p_node) {
        return nullptr;
    }

    NodePath np = p_node->get_metadata(0).as<NodePath>();
    if (np == p_path) {
        return p_node;
    }

    TreeItem *children = p_node->get_children();
    while (children) {

        TreeItem *n = _find(children, p_path);
        if (n) {
            return n;
        }
        children = children->get_next();
    }

    return nullptr;
}

void SceneTreeEditor::set_selected(Node *p_node, bool p_emit_selected) {

    ERR_FAIL_COND(blocked > 0);

    if (pending_test_update) {
        _test_update_tree();
    }
    if (tree_dirty) {
        _update_tree();
    }

    if (selected == p_node) {
        return;
    }

    TreeItem *item = p_node ? _find(tree->get_root(), p_node->get_path()) : nullptr;

    if (item) {
        // make visible when it's collapsed
        TreeItem *node = item->get_parent();
        while (node && node != tree->get_root()) {
            node->set_collapsed(false);
            node = node->get_parent();
        }
        item->select(0);
        item->set_as_cursor(0);
        selected = p_node;
        tree->ensure_cursor_is_visible();

    } else {
        if (!p_node) {
            selected = nullptr;
        }
        _update_tree();
        selected = p_node;
    }

    if (p_emit_selected) {
        emit_signal("node_selected");
    }
}

void SceneTreeEditor::_rename_node(GameEntity p_node, StringView p_name) {

    Object *o = object_for_entity(p_node);
    ERR_FAIL_COND(!o);
    Node *n = object_cast<Node>(o);
    ERR_FAIL_COND(!n);
    TreeItem *item = _find(tree->get_root(), n->get_path());
    ERR_FAIL_COND(!item);

    n->set_name(p_name);
    item->set_metadata(0, n->get_path());
    item->set_text_utf8(0, p_name);
}

void SceneTreeEditor::_renamed() {

    TreeItem *which = tree->get_edited();

    ERR_FAIL_COND(!which);
    NodePath np = which->get_metadata(0).as<NodePath>();
    Node *n = get_node(np);
    ERR_FAIL_COND(!n);

    // Empty node names are not allowed, so resets it to previous text and show warning
    if (StringUtils::strip_edges(which->get_text(0)).empty()) {
        which->set_text_utf8(0, n->get_name());
        EditorNode::get_singleton()->show_warning(TTR("No name provided."));
        return;
    }

    String new_name(which->get_text(0));
    if (!_validate_node_name(new_name)) {

        error->set_text(TTR("Invalid node name, the following characters are not allowed:") + "\n" + invalid_character);
        error->popup_centered_minsize();

        if (new_name.empty()) {
            which->set_text_utf8(0, n->get_name());
            return;
        }

        which->set_text_utf8(0, new_name);
    }

    if (new_name == n->get_name()) {
        return;
    }
    // Trim leading/trailing whitespace to prevent node names from containing accidental whitespace, which would make it
    // more difficult to get the node via `get_node()`.
    new_name = StringUtils::strip_edges(new_name);

    // if (n->is_unique_name_in_owner() && get_tree()->get_edited_scene_root()->get_node_or_null(NodePath("%" + new_name)) != nullptr) {
    //     error->set_text(TTR("Another node already uses this unique name in the scene."));
    //     error->popup_centered();
    //     which->set_text(0, n->get_name());
    //     return;
    // }

    if (!undo_redo) {
        n->set_name(new_name);
        which->set_metadata(0, n->get_path());
        emit_signal("node_renamed");
    } else {
        undo_redo->create_action(TTR("Rename Node"));
        emit_signal("node_prerename", Variant(n), new_name);
        undo_redo->add_do_method(this, "_rename_node", Variant::from(n->get_instance_id()), new_name);
        undo_redo->add_undo_method(this, "_rename_node", Variant::from(n->get_instance_id()), n->get_name());
        undo_redo->commit_action();
    }
}

void SceneTreeEditor::set_marked(const HashSet<Node *> &p_marked, bool p_selectable, bool p_children_selectable) {

    if (tree_dirty) {
        _update_tree();
    }
    marked = p_marked;
    marked_selectable = p_selectable;
    marked_children_selectable = p_children_selectable;
    _update_tree();
}

void SceneTreeEditor::set_marked(Node *p_marked, bool p_selectable, bool p_children_selectable) {

    HashSet<Node *> s;
    if (p_marked) {
        s.insert(p_marked);
    }
    set_marked(s, p_selectable, p_children_selectable);
}

void SceneTreeEditor::set_filter(const UIString &p_filter) {

    filter = p_filter;
    _update_tree(true);
}

UIString SceneTreeEditor::get_filter() const {

    return filter;
}

void SceneTreeEditor::set_display_foreign_nodes(bool p_display) {

    display_foreign = p_display;
    _update_tree();
}

void SceneTreeEditor::set_valid_types(const Vector<StringName> &p_valid) {
    valid_types = p_valid;
}

void SceneTreeEditor::set_editor_selection(EditorSelection *p_selection) {

    editor_selection = p_selection;
    tree->set_select_mode(Tree::SELECT_MULTI);
    tree->set_cursor_can_exit_tree(false);
    editor_selection->connect("selection_changed",callable_mp(this, &SceneTreeEditor::_selection_changed));
}

void SceneTreeEditor::_update_selection(TreeItem *item) {

    ERR_FAIL_COND(!item);

    NodePath np = item->get_metadata(0).as<NodePath>();

    if (!has_node(np)) {
        return;
    }

    Node *n = get_node(np);

    if (!n) {
        return;
    }

    if (editor_selection->is_selected(n)) {
        item->select(0);
    } else {
        item->deselect(0);
    }
    TreeItem *c = item->get_children();

    while (c) {
        _update_selection(c);
        c = c->get_next();
    }
}

void SceneTreeEditor::_selection_changed() {

    if (!editor_selection) {
        return;
    }

    TreeItem *root = tree->get_root();

    if (!root) {
        return;
    }
    _update_selection(root);
}

void SceneTreeEditor::_cell_collapsed(Object *p_obj) {

    if (updating_tree || !can_rename) {
        return;
    }

    TreeItem *ti = object_cast<TreeItem>(p_obj);
    if (!ti) {
        return;
    }

    bool collapsed = ti->is_collapsed();

    Node *n = get_node(ti->get_metadata(0).as<NodePath>());
    ERR_FAIL_COND(!n);

    n->set_display_folded(collapsed);
}

Variant SceneTreeEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
    if (!can_rename) {
        return Variant(); //not editable tree
    }

    if (tree->get_button_id_at_position(p_point) != -1) {
        return Variant(); //dragging from button
    }

    Vector<Node *> selected;
    Vector<Ref<Texture> > icons;
    TreeItem *next = tree->get_next_selected(nullptr);
    while (next) {

        NodePath np = next->get_metadata(0).as<NodePath>();

        Node *n = get_node(np);
        if (n) {
            // Only allow selection if not part of an instanced scene.
            if (!n->get_owner() || n->get_owner() == get_scene_node(this) || n->get_owner()->get_filename().empty()) {
                selected.push_back(n);
                icons.emplace_back(eastl::move(next->get_icon(0)));
            }
        }
        next = tree->get_next_selected(next);
    }

    if (selected.empty()) {
        return Variant();
    }

    VBoxContainer *vb = memnew(VBoxContainer);
    Array objs;
    int list_max = 10;
    float opacity_step = 1.0f / list_max;
    float opacity_item = 1.0f;
    for (int i = 0; i < selected.size(); i++) {

        if (i < list_max) {
            HBoxContainer *hb = memnew(HBoxContainer);
            TextureRect *tf = memnew(TextureRect);
            tf->set_texture(icons[i]);
            tf->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
            hb->add_child(tf);
            Label *label = memnew(Label(selected[i]->get_name()));
            hb->add_child(label);
            vb->add_child(hb);
            hb->set_modulate(Color(1, 1, 1, opacity_item));
            opacity_item -= opacity_step;
        }
        NodePath p = selected[i]->get_path();
        objs.push_back(p);
    }

    set_drag_preview(vb);
    Dictionary drag_data;
    drag_data["type"] = "nodes";
    drag_data["nodes"] = objs;

    tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN | Tree::DROP_MODE_ON_ITEM);
    emit_signal("nodes_dragged");

    return drag_data;
}

bool SceneTreeEditor::_is_script_type(const StringName &p_type) const {
    return script_types->contains(p_type);
}

bool SceneTreeEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {

    if (!can_rename) {
        return false; //not editable tree
    }

    Dictionary d = p_data.as<Dictionary>();
    if (!d.has("type")) {
        return false;
    }

    TreeItem *item = tree->get_item_at_position(p_point);
    if (!item) {
        return false;
    }

    int section = tree->get_drop_section_at_position(p_point);
    if (section < -1 || (section == -1 && !item->get_parent()))
        return false;

    String drop_type(d["type"].as<String>());

    if (drop_type == "files") {

        Vector<String> files(d["files"].as<Vector<String>>());

        if (files.empty()) {
            return false; //weird
        }

        if (_is_script_type(EditorFileSystem::get_singleton()->get_file_type(files[0]))) {
            tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);
            return true;
        }

        bool scene_drop = true;
        for (const String &file : files) {
            StringName ftype = EditorFileSystem::get_singleton()->get_file_type(file);
            if (ftype != "PackedScene") {
                scene_drop = false;
                break;
            }
        }

        if (scene_drop) {
            tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN | Tree::DROP_MODE_ON_ITEM);
        } else {
            if (files.size() > 1) {
                return false;
            }
            tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);
        }


        return true;
    }

    if (drop_type == "script_list_element") {
        ScriptEditorBase *se = d["script_list_element"].asT<ScriptEditorBase>();
        if (se) {
            String sp = se->get_edited_resource()->get_path();
            if (_is_script_type(EditorFileSystem::get_singleton()->get_file_type(sp))) {
                tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);
                return true;
            }
        }
    }

    return String(d["type"]) == "nodes" && filter.isEmpty();
}

void SceneTreeEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {

    if (!can_drop_data_fw(p_point, p_data, p_from)) {
        return;
    }

    TreeItem *item = tree->get_item_at_position(p_point);
    if (!item) {
        return;
    }
    int section = tree->get_drop_section_at_position(p_point);
    if (section < -1) {
        return;
    }

    NodePath np = item->get_metadata(0).as<NodePath>();
    Node *n = get_node(np);
    if (!n) {
        return;
    }

    Dictionary d = p_data.as<Dictionary>();

    if (d["type"].as<String>() == "nodes") {
        Array nodes = d["nodes"].as<Array>();
        emit_signal("nodes_rearranged", nodes, np, section);
    }

    if (d["type"].as<String>() == "files") {

        Vector<String> files(d["files"].as<Vector<String>>());

        StringName ftype = EditorFileSystem::get_singleton()->get_file_type(files[0]);
        if (_is_script_type(ftype)) {
            emit_signal("script_dropped", files[0], np);
        } else {
            emit_signal("files_dropped", Variant::from(files), np, section);
        }
    }

    if (d["type"].as<String>() == "script_list_element") {
        ScriptEditorBase *se = d["script_list_element"].asT<ScriptEditorBase>();
        if (se) {
            String sp = se->get_edited_resource()->get_path();
            if (_is_script_type(EditorFileSystem::get_singleton()->get_file_type(sp))) {
                emit_signal("script_dropped", sp, np);
            }
        }
    }
}

void SceneTreeEditor::_rmb_select(const Vector2 &p_pos) {

    emit_signal("rmb_pressed", tree->get_global_transform().xform(p_pos));
}

void SceneTreeEditor::_warning_changed(Node *p_for_node) {

    //should use a timer
    update_timer->start();
}

void SceneTreeEditor::set_connect_to_script_mode(bool p_enable) {
    connect_to_script_mode = p_enable;
    update_tree();
}

void SceneTreeEditor::set_connecting_signal(bool p_enable) {
    connecting_signal = p_enable;
    update_tree();
}

void SceneTreeEditor::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_update_tree",{"scroll_to_selected"}), &SceneTreeEditor::_update_tree, {DEFVAL(false)});
    MethodBinder::bind_method("_renamed", &SceneTreeEditor::_renamed);
    MethodBinder::bind_method("_rename_node", &SceneTreeEditor::_rename_node);

    SE_BIND_METHOD(SceneTreeEditor,get_drag_data_fw);
    SE_BIND_METHOD(SceneTreeEditor,can_drop_data_fw);
    SE_BIND_METHOD(SceneTreeEditor,drop_data_fw);

    SE_BIND_METHOD(SceneTreeEditor,update_tree);

    ADD_SIGNAL(MethodInfo("node_selected"));
    ADD_SIGNAL(MethodInfo("node_renamed"));
    ADD_SIGNAL(MethodInfo("node_prerename"));
    ADD_SIGNAL(MethodInfo("node_changed"));
    ADD_SIGNAL(MethodInfo("nodes_dragged"));
    ADD_SIGNAL(MethodInfo("nodes_rearranged", PropertyInfo(VariantType::ARRAY, "paths"), PropertyInfo(VariantType::NODE_PATH, "to_path"), PropertyInfo(VariantType::INT, "type")));
    ADD_SIGNAL(MethodInfo("files_dropped", PropertyInfo(VariantType::POOL_STRING_ARRAY, "files"), PropertyInfo(VariantType::NODE_PATH, "to_path"), PropertyInfo(VariantType::INT, "type")));
    ADD_SIGNAL(MethodInfo("script_dropped", PropertyInfo(VariantType::STRING, "file"), PropertyInfo(VariantType::NODE_PATH, "to_path")));
    ADD_SIGNAL(MethodInfo("rmb_pressed", PropertyInfo(VariantType::VECTOR2, "position")));

    ADD_SIGNAL(MethodInfo("open"));
    ADD_SIGNAL(MethodInfo("open_script"));
}

SceneTreeEditor::SceneTreeEditor(bool p_label, bool p_can_rename, bool p_can_open_instance) {

    connect_to_script_mode = false;
    connecting_signal = false;
    undo_redo = nullptr;
    tree_dirty = true;
    selected = nullptr;

    marked_selectable = false;
    marked_children_selectable = false;
    can_rename = p_can_rename;
    can_open_instance = p_can_open_instance;
    display_foreign = false;
    editor_selection = nullptr;

    if (p_label) {
        Label *label = memnew(Label);
        label->set_position(Point2(10, 0));
        label->set_text(TTR("Scene Tree (Nodes):"));

        add_child(label);
    }

    tree = memnew(Tree);
    tree->set_anchor(Margin::Right, ANCHOR_END);
    tree->set_anchor(Margin::Bottom, ANCHOR_END);
    tree->set_begin(Point2(0, p_label ? 18 : 0));
    tree->set_end(Point2(0, 0));
    tree->set_allow_reselect(true);
    tree->add_constant_override("button_margin", 0);

    add_child(tree);

    tree->set_drag_forwarding(this);
    if (p_can_rename) {
        tree->set_allow_rmb_select(true);
        tree->connect("item_rmb_selected",callable_mp(this, &SceneTreeEditor::_rmb_select));
        tree->connect("empty_tree_rmb_selected",callable_mp(this, &SceneTreeEditor::_rmb_select));
    }

    tree->connect("cell_selected",callable_mp(this, &SceneTreeEditor::_selected_changed));
    tree->connect("item_edited",callable_mp(this, &SceneTreeEditor::_renamed), ObjectNS::CONNECT_QUEUED);
    tree->connect("multi_selected",callable_mp(this, &SceneTreeEditor::_cell_multi_selected));
    tree->connect("button_pressed",callable_mp(this, &SceneTreeEditor::_cell_button_pressed));
    tree->connect("nothing_selected",callable_mp(this, &SceneTreeEditor::_deselect_items));
    //tree->connect("item_edited", this,"_renamed",Vector<Variant>(),true);

    error = memnew(AcceptDialog);
    add_child(error);

    warning = memnew(AcceptDialog);
    add_child(warning);
    warning->set_title(TTR("Node Configuration Warning!"));

    show_enabled_subscene = false;

    last_hash = 0;
    pending_test_update = false;
    updating_tree = false;
    blocked = 0;

    update_timer = memnew(Timer);
    update_timer->connect("timeout",callable_gen(this, [this]() { _update_tree(false); }));
    update_timer->set_one_shot(true);
    update_timer->set_wait_time(0.5f);
    add_child(update_timer);

    script_types = memnew(Vector<StringName>);
    ClassDB::get_inheriters_from_class("Script", script_types);
}

SceneTreeEditor::~SceneTreeEditor() {

    memdelete(script_types);
}

/******** DIALOG *********/

void SceneTreeDialog::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_VISIBILITY_CHANGED: {
            if (is_visible()) {
                tree->update_tree();
            }
        } break;
        case NOTIFICATION_ENTER_TREE: {
            connect("confirmed",callable_mp(this, &SceneTreeDialog::_select));
            filter->set_right_icon(tree->get_theme_icon("Search", "EditorIcons"));
            filter->set_clear_button_enabled(true);
        } break;
        case NOTIFICATION_EXIT_TREE: {
            disconnect("confirmed",callable_mp(this, &SceneTreeDialog::_select));
        } break;
    }
}

void SceneTreeDialog::_cancel() {

    hide();
}

void SceneTreeDialog::_select() {

    if (tree->get_selected()) {
        emit_signal("selected", tree->get_selected()->get_path());
        hide();
    }
}

void SceneTreeDialog::_selected_changed() {
    get_ok()->set_disabled(!tree->get_selected());
}

void SceneTreeDialog::_filter_changed(StringView p_filter) {

    tree->set_filter(StringUtils::from_utf8(p_filter));
}

void SceneTreeDialog::_bind_methods() {
    ADD_SIGNAL(MethodInfo("selected", PropertyInfo(VariantType::NODE_PATH, "path")));
}

SceneTreeDialog::SceneTreeDialog() {

    set_title(TTR("Select a Node"));
    VBoxContainer *vbc = memnew(VBoxContainer);
    add_child(vbc);

    filter = memnew(LineEdit);
    filter->set_h_size_flags(SIZE_EXPAND_FILL);
    filter->set_placeholder(TTR("Filter nodes"));
    filter->add_constant_override("minimum_spaces", 0);
    filter->connect("text_changed",callable_mp(this, &SceneTreeDialog::_filter_changed));
    vbc->add_child(filter);

    tree = memnew(SceneTreeEditor(false, false, true));
    tree->set_v_size_flags(SIZE_EXPAND_FILL);
    tree->get_scene_tree()->connect("item_activated",callable_mp(this, &SceneTreeDialog::_select));
    vbc->add_child(tree);

    // Disable the OK button when no node is selected.
    get_ok()->set_disabled(!tree->get_selected());
    tree->connect("node_selected", callable_mp(this, &SceneTreeDialog::_selected_changed));
}

SceneTreeDialog::~SceneTreeDialog() = default;
