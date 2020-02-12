/*************************************************************************/
/*  editor_sub_scene.cpp                                                 */
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

#include "editor_sub_scene.h"

#include "editor/editor_node.h"
#include "scene/gui/margin_container.h"
#include "scene/resources/packed_scene.h"
#include "core/method_bind.h"

IMPL_GDCLASS(EditorSubScene)

void EditorSubScene::_path_selected(se_string_view p_path) {

    path->set_text(p_path);
    _path_changed(p_path);
}

void EditorSubScene::_path_changed(se_string_view p_path) {

    tree->clear();

    if (scene) {
        memdelete(scene);
        scene = nullptr;
    }

    if (p_path.empty())
        return;

    Ref<PackedScene> ps = dynamic_ref_cast<PackedScene>(ResourceLoader::load(p_path, "PackedScene"));

    if (not ps)
        return;

    scene = ps->instance();
    if (!scene)
        return;

    _fill_tree(scene, nullptr);
}

void EditorSubScene::_path_browse() {

    file_dialog->popup_centered_ratio();
}

void EditorSubScene::_notification(int p_what) {

    if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {

        if (is_visible() && scene == nullptr)
            _path_browse();
    }
}

void EditorSubScene::_fill_tree(Node *p_node, TreeItem *p_parent) {

    TreeItem *it = tree->create_item(p_parent);
    it->set_metadata(0, Variant(p_node));
    it->set_text(0, p_node->get_name());
    it->set_editable(0, false);
    it->set_selectable(0, true);
    it->set_icon(0, EditorNode::get_singleton()->get_object_icon(p_node, "Node"));

    for (int i = 0; i < p_node->get_child_count(); i++) {

        Node *c = p_node->get_child(i);
        if (c->get_owner() != scene)
            continue;
        _fill_tree(c, it);
    }
}

void EditorSubScene::_selected_changed() {
    TreeItem *item = tree->get_selected();
    ERR_FAIL_COND(!item);
    Node *n = item->get_metadata(0);

    if (!n || !selection.contains(n)) {
        selection.clear();
        is_root = false;
    }
}

void EditorSubScene::_item_multi_selected(Object *p_object, int p_cell, bool p_selected) {
    if (is_root)
        return;

    TreeItem *item = object_cast<TreeItem>(p_object);
    ERR_FAIL_COND(!item);

    Node *n = item->get_metadata(0);

    if (!n)
        return;

    if (p_selected) {
        if (n == scene) {
            is_root = true;
            selection.clear();
        }
        selection.push_back(n);
    } else {
        auto E = selection.find(n);

        if (E!=selection.end())
            selection.erase(E);
    }
}

void EditorSubScene::_remove_selection_child(Node *p_node) {
    if (p_node->get_child_count() <= 0)
        return;

    for (int i = 0; i < p_node->get_child_count(); i++) {
        Node *c = p_node->get_child(i);
        auto iter = selection.find(c);
        if (iter!=selection.end()) {
            selection.erase_unsorted(iter);
        }
        if (c->get_child_count() > 0) {
            _remove_selection_child(c);
        }
    }
}

void EditorSubScene::ok_pressed() {
    if (selection.empty()) {
        return;
    }
    for (Node * c : selection) {
        _remove_selection_child(c);
    }
    emit_signal("subscene_selected");
    hide();
    clear();
}

void EditorSubScene::_reown(Node *p_node, Vector<Node *> *p_to_reown) {

    if (p_node == scene) {

        scene->set_filename(String());
        p_to_reown->push_back(p_node);
    } else if (p_node->get_owner() == scene) {

        p_to_reown->push_back(p_node);
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {
        Node *c = p_node->get_child(i);
        _reown(c, p_to_reown);
    }
}

void EditorSubScene::move(Node *p_new_parent, Node *p_new_owner) {
    if (!scene) {
        return;
    }

    if (selection.empty()) {
        return;
    }

    for (Node *selnode : selection) {
        if (!selnode) {
            return;
        }
        Vector<Node *> to_reown;
        _reown(selnode, &to_reown);
        if (selnode != scene) {
            selnode->get_parent()->remove_child(selnode);
        }

        p_new_parent->add_child(selnode);
        for (Node *F : to_reown) {
            F->set_owner(p_new_owner);
        }
    }
    if (!is_root) {
        memdelete(scene);
    }
    scene = nullptr;
    //return selnode;
}

void EditorSubScene::clear() {

    path->set_text("");
    _path_changed(String());
}

void EditorSubScene::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_path_selected"), &EditorSubScene::_path_selected);
    MethodBinder::bind_method(D_METHOD("_path_changed"), &EditorSubScene::_path_changed);
    MethodBinder::bind_method(D_METHOD("_path_browse"), &EditorSubScene::_path_browse);
    MethodBinder::bind_method(D_METHOD("_item_multi_selected"), &EditorSubScene::_item_multi_selected);
    MethodBinder::bind_method(D_METHOD("_selected_changed"), &EditorSubScene::_selected_changed);
    ADD_SIGNAL(MethodInfo("subscene_selected"));
}

EditorSubScene::EditorSubScene() {

    scene = nullptr;
    is_root = false;

    set_title(TTR("Select Node(s) to Import"));
    set_hide_on_ok(false);

    VBoxContainer *vb = memnew(VBoxContainer);
    add_child(vb);
    //set_child_rect(vb);

    HBoxContainer *hb = memnew(HBoxContainer);
    path = memnew(LineEdit);
    path->connect("text_entered", this, "_path_changed");
    hb->add_child(path);
    path->set_h_size_flags(SIZE_EXPAND_FILL);
    Button *b = memnew(Button);
    b->set_text(TTR("Browse"));
    hb->add_child(b);
    b->connect("pressed", this, "_path_browse");
    vb->add_margin_child(TTR("Scene Path:"), hb);

    tree = memnew(Tree);
    tree->set_v_size_flags(SIZE_EXPAND_FILL);
    vb->add_margin_child(TTR("Import From Node:"), tree, true);
    tree->set_select_mode(Tree::SELECT_MULTI);
    tree->connect("multi_selected", this, "_item_multi_selected");
    //tree->connect("nothing_selected", this, "_deselect_items");
    tree->connect("cell_selected", this, "_selected_changed");

    tree->connect("item_activated", this, "_ok", make_binds(), ObjectNS::CONNECT_QUEUED);

    file_dialog = memnew(EditorFileDialog);
    Vector<String> extensions;
    ResourceLoader::get_recognized_extensions_for_type("PackedScene", extensions);

    for (const String &E : extensions) {

        file_dialog->add_filter("*." + E);
    }

    file_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
    add_child(file_dialog);
    file_dialog->connect("file_selected", this, "_path_selected");
}
