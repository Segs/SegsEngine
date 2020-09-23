/*************************************************************************/
/*  navigation_polygon_editor_plugin.cpp                                 */
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

#include "navigation_polygon_editor_plugin.h"
#include "core/class_db.h"
#include "core/translation_helpers.h"

IMPL_GDCLASS(NavigationPolygonEditor)
IMPL_GDCLASS(NavigationPolygonEditorPlugin)

Ref<NavigationPolygon> NavigationPolygonEditor::_ensure_navpoly() const {

    Ref<NavigationPolygon> navpoly = node->get_navigation_polygon();
    if (not navpoly) {

        navpoly = make_ref_counted<NavigationPolygon>();
        node->set_navigation_polygon(navpoly);
    }
    return navpoly;
}

Node2D *NavigationPolygonEditor::_get_node() const {

    return node;
}

void NavigationPolygonEditor::_set_node(Node *p_polygon) {

    node = object_cast<NavigationPolygonInstance>(p_polygon);
}

int NavigationPolygonEditor::_get_polygon_count() const {

    Ref<NavigationPolygon> navpoly = node->get_navigation_polygon();
    if (navpoly)
        return navpoly->get_outline_count();
    else
        return 0;
}

Variant NavigationPolygonEditor::_get_polygon(int p_idx) const {

    Ref<NavigationPolygon> navpoly = node->get_navigation_polygon();
    if (navpoly)
        return Variant(navpoly->get_outline(p_idx));
    else
        return Variant(Vector<Vector2>());
}

void NavigationPolygonEditor::_set_polygon(int p_idx, const PoolVector<Vector2> &p_polygon) const {

    Ref<NavigationPolygon> navpoly = _ensure_navpoly();
    navpoly->set_outline(p_idx, Variant::from(p_polygon));
    navpoly->make_polygons_from_outlines();
}

void NavigationPolygonEditor::_action_add_polygon(const Variant &p_polygon) {

    Ref<NavigationPolygon> navpoly = _ensure_navpoly();
    undo_redo->add_do_method(navpoly.get(), "add_outline", p_polygon);
    undo_redo->add_undo_method(navpoly.get(), "remove_outline", navpoly->get_outline_count());
    undo_redo->add_do_method(navpoly.get(), "make_polygons_from_outlines");
    undo_redo->add_undo_method(navpoly.get(), "make_polygons_from_outlines");
}

void NavigationPolygonEditor::_action_remove_polygon(int p_idx) {

    Ref<NavigationPolygon> navpoly = _ensure_navpoly();
    undo_redo->add_do_method(navpoly.get(), "remove_outline", p_idx);
    undo_redo->add_undo_method(navpoly.get(), "add_outline_at_index", Variant(navpoly->get_outline(p_idx)), p_idx);
    undo_redo->add_do_method(navpoly.get(), "make_polygons_from_outlines");
    undo_redo->add_undo_method(navpoly.get(), "make_polygons_from_outlines");
}

void NavigationPolygonEditor::_action_set_polygon(int p_idx, const Variant &p_previous, const Variant &p_polygon) {

    Ref<NavigationPolygon> navpoly = _ensure_navpoly();
    undo_redo->add_do_method(navpoly.get(), "set_outline", p_idx, p_polygon);
    undo_redo->add_undo_method(navpoly.get(), "set_outline", p_idx, p_previous);
    undo_redo->add_do_method(navpoly.get(), "make_polygons_from_outlines");
    undo_redo->add_undo_method(navpoly.get(), "make_polygons_from_outlines");
}

bool NavigationPolygonEditor::_has_resource() const {

    return node && node->get_navigation_polygon();
}

void NavigationPolygonEditor::_create_resource() {

    if (!node)
        return;

    undo_redo->create_action(TTR("Create Navigation Polygon"));
    undo_redo->add_do_method(node, "set_navigation_polygon", make_ref_counted<NavigationPolygon>());
    undo_redo->add_undo_method(node, "set_navigation_polygon", Variant(REF()));
    undo_redo->commit_action();

    _menu_option(MODE_CREATE);
}

NavigationPolygonEditor::NavigationPolygonEditor(EditorNode *p_editor) :
        AbstractPolygon2DEditor(p_editor) {
    node = nullptr;
}

NavigationPolygonEditorPlugin::NavigationPolygonEditorPlugin(EditorNode *p_node) :
        AbstractPolygon2DEditorPlugin(p_node, memnew(NavigationPolygonEditor(p_node)), "NavigationPolygonInstance") {
}
