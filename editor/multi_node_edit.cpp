/*************************************************************************/
/*  multi_node_edit.cpp                                                  */
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

#include "multi_node_edit.h"

#include "core/math/math_fieldwise.h"
#include "editor_node.h"
#include "core/class_db.h"

IMPL_GDCLASS(MultiNodeEdit)

bool MultiNodeEdit::_set(const StringName &p_name, const Variant &p_value) {

    return _set_impl(p_name, p_value, StringView());
}

bool MultiNodeEdit::_set_impl(const StringName &p_name, const Variant &p_value, StringView p_field) {

    Node *es = EditorNode::get_singleton()->get_edited_scene();
    if (!es)
        return false;

    StringName name = p_name;

    if (name == "scripts") { // script set is intercepted at object level (check Variant Object::get() ) ,so use a different name
        name = "script";
    }

    Node *node_path_target = nullptr;
    if (p_value.get_type() == VariantType::NODE_PATH && p_value != NodePath()) {
        node_path_target = es->get_node(p_value.as<NodePath>());
    }

    UndoRedo *ur = EditorNode::get_undo_redo();

    ur->create_action(TTR("MultiNode Set") + " " + name, UndoRedo::MERGE_ENDS);
    for (const NodePath &E : nodes) {

        if (!es->has_node(E)) {
            continue;
        }

        Node *n = es->get_node(E);
        if (!n) {
            continue;
        }

        if (p_value.get_type() == VariantType::NODE_PATH) {
            NodePath path;
            if (node_path_target) {
                path = n->get_path_to(node_path_target);
            }
            ur->add_do_property(n, name, path);
        } else {
            Variant new_value;
            if (p_field.empty()) {
                // whole value
                new_value = p_value;
            } else {
                // only one field
                new_value = fieldwise_assign(n->get(name), p_value, p_field);
            }
            ur->add_do_property(n, name, new_value);
        }

        ur->add_undo_property(n, name, n->get(name));
    }
    ur->add_do_method(EditorNode::get_singleton()->get_inspector(), "refresh");
    ur->add_undo_method(EditorNode::get_singleton()->get_inspector(), "refresh");

    ur->commit_action();
    return true;
}

bool MultiNodeEdit::_get(const StringName &p_name, Variant &r_ret) const {

    Node *es = EditorNode::get_singleton()->get_edited_scene();
    if (!es)
        return false;

    StringName name = p_name;
    if (name == "scripts") { // script set is intercepted at object level (check Variant Object::get() ) ,so use a different name
        name = "script";
    }

    for (const NodePath &E : nodes) {

        if (!es->has_node(E))
            continue;

        const Node *n = es->get_node(E);
        if (!n)
            continue;

        bool found;
        r_ret = n->get(name, &found);
        if (found)
            return true;
    }

    return false;
}

void MultiNodeEdit::_get_property_list(Vector<PropertyInfo> *p_list) const {
    // TODO: consider using fixed hash map here.
    HashMap<StringName, PLData> usage;

    Node *es = EditorNode::get_singleton()->get_edited_scene();
    if (!es)
        return;

    int nc = 0;

    Vector<PLData *> data_list;

    for (const NodePath &E : nodes) {

        if (!es->has_node(E))
            continue;

        Node *n = es->get_node(E);
        if (!n)
            continue;

        Vector<PropertyInfo> plist;
        n->get_property_list(&plist, true);

        for (const PropertyInfo &F : plist) {

            if (F.name == "script")
                continue; //added later manually, since this is intercepted before being set (check Variant Object::get() )
            if (!usage.contains(F.name)) {
                PLData pld;
                pld.uses = 0;
                pld.info = F;
                usage[F.name] = pld;
                data_list.push_back(&usage.find(F.name)->second);
            }

            // Make sure only properties with the same exact PropertyInfo data will appear
            if (usage[F.name].info == F)
                usage[F.name].uses++;
        }

        nc++;
    }

    for (PLData *E : data_list) {

        if (nc == E->uses) {
            p_list->push_back(E->info);
        }
    }

    p_list->push_back(PropertyInfo(VariantType::OBJECT, "scripts", PropertyHint::ResourceType, "Script"));
}

void MultiNodeEdit::clear_nodes() {

    nodes.clear();
}

void MultiNodeEdit::add_node(const NodePath &p_node) {

    nodes.push_back(p_node);
}
int MultiNodeEdit::get_node_count() const {
    return nodes.size();
}

const NodePath &MultiNodeEdit::get_node(int p_index) const {
    static NodePath dummy;
    ERR_FAIL_INDEX_V(p_index, nodes.size(), dummy);
    return nodes[p_index];
}

void MultiNodeEdit::set_property_field(const StringName &p_property, const Variant &p_value, StringView p_field) {

    _set_impl(p_property, p_value, p_field);
}

MultiNodeEdit::MultiNodeEdit() = default;
