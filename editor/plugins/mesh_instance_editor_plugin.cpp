/*************************************************************************/
/*  mesh_instance_editor_plugin.cpp                                      */
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

#include "mesh_instance_editor_plugin.h"

#include "core/method_bind.h"
#include "core/callable_method_pointer.h"
#include "core/string_formatter.h"
#include "scene/3d/collision_shape_3d.h"
#include "scene/3d/navigation_mesh_instance.h"
#include "scene/3d/physics_body_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/menu_button.h"
#include "scene/main/scene_tree.h"
#include "node_3d_editor_plugin.h"
#include "editor/editor_scale.h"
#include "core/translation_helpers.h"

IMPL_GDCLASS(MeshInstanceEditor)
IMPL_GDCLASS(MeshInstanceEditorPlugin)

void MeshInstanceEditor::_node_removed(Node *p_node) {

    if (p_node == node) {
        node = nullptr;
        options->hide();
    }
}

void MeshInstanceEditor::edit(MeshInstance3D *p_mesh) {

    node = p_mesh;
}

void MeshInstanceEditor::_menu_option(int p_option) {

    Ref<Mesh> mesh = node->get_mesh();
    if (not mesh) {
        err_dialog->set_text(TTR("Mesh is empty!"));
        err_dialog->popup_centered_minsize();
        return;
    }

    switch (p_option) {
        case MENU_OPTION_CREATE_STATIC_TRIMESH_BODY: {

            EditorSelection *editor_selection = EditorNode::get_singleton()->get_editor_selection();
            UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();

            const Vector<Node *> &selection = editor_selection->get_selected_node_list();

            if (selection.empty()) {
                Ref<Shape> shape = mesh->create_trimesh_shape();
                if (!shape) {
                    err_dialog->set_text(TTR("Couldn't create a Trimesh collision shape."));
                    err_dialog->popup_centered_minsize();
                    return;
                }

                CollisionShape3D *cshape = memnew(CollisionShape3D);
                cshape->set_shape(shape);
                StaticBody3D *body = memnew(StaticBody3D);
                body->add_child(cshape);

                Node *owner = node == get_tree()->get_edited_scene_root() ? node : node->get_owner();

                ur->create_action(TTR("Create Static Trimesh Body"));
                ur->add_do_method(node, "add_child", Variant(body));
                ur->add_do_method(body, "set_owner", Variant(owner));
                ur->add_do_method(cshape, "set_owner", Variant(owner));
                ur->add_do_reference(body);
                ur->add_undo_method(node, "remove_child", Variant(body));
                ur->commit_action();
                return;
            }

            ur->create_action(TTR("Create Static Trimesh Body"));

            for (Node * E : selection) {

                MeshInstance3D *instance = object_cast<MeshInstance3D>(E);
                if (!instance) {
                    continue;
                }

                Ref<Mesh> m = instance->get_mesh();
                if (!m) {
                    continue;
                }

                Ref<Shape> shape = m->create_trimesh_shape();
                if (!shape) {
                    continue;
                }

                CollisionShape3D *cshape = memnew(CollisionShape3D);
                cshape->set_shape(shape);
                StaticBody3D *body = memnew(StaticBody3D);
                body->add_child(cshape);

                Node *owner = instance == get_tree()->get_edited_scene_root() ? instance : instance->get_owner();

                ur->add_do_method(instance, "add_child", Variant(body));
                ur->add_do_method(body, "set_owner", Variant(owner));
                ur->add_do_method(cshape, "set_owner", Variant(owner));
                ur->add_do_reference(body);
                ur->add_undo_method(instance, "remove_child", Variant(body));
            }

            ur->commit_action();

        } break;
    case MENU_OPTION_CREATE_TRIMESH_COLLISION_SHAPE: {

        if (node == get_tree()->get_edited_scene_root()) {
            err_dialog->set_text(TTR("This doesn't work on scene root!"));
            err_dialog->popup_centered_minsize();
            return;
        }

        Ref<Shape> shape = mesh->create_trimesh_shape();
        if (not shape)
            return;

        CollisionShape3D *cshape = memnew(CollisionShape3D);
        cshape->set_shape(shape);
        cshape->set_transform(node->get_transform());

        Node *owner = node->get_owner();

        UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();

        ur->create_action(TTR("Create Trimesh Static Shape"));

        ur->add_do_method(node->get_parent(), "add_child", Variant(cshape));
        ur->add_do_method(node->get_parent(), "move_child", Variant(cshape), node->get_index() + 1);
        ur->add_do_method(cshape, "set_owner", Variant(owner));
        ur->add_do_reference(cshape);
        ur->add_undo_method(node->get_parent(), "remove_child", Variant(cshape));
        ur->commit_action();
    } break;
    case MENU_OPTION_CREATE_SINGLE_CONVEX_COLLISION_SHAPE:
    case MENU_OPTION_CREATE_SIMPLIFIED_CONVEX_COLLISION_SHAPE: {

        if (node == get_tree()->get_edited_scene_root()) {
            err_dialog->set_text(TTR("Can't create a single convex collision shape for the scene root."));
            err_dialog->popup_centered_minsize();
            return;
        }
        bool simplify = (p_option == MENU_OPTION_CREATE_SIMPLIFIED_CONVEX_COLLISION_SHAPE);

        Ref<Shape> shape = mesh->create_convex_shape(true, simplify);

        if (!shape) {
            err_dialog->set_text(TTR("Couldn't create a single convex collision shape."));
            err_dialog->popup_centered_minsize();
            return;
        }
        UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();

        if (simplify) {
            ur->create_action(TTR("Create Simplified Convex Shape"));
        } else {
            ur->create_action(TTR("Create Single Convex Shape"));
        }

        CollisionShape3D *cshape = memnew(CollisionShape3D);
        cshape->set_shape(shape);
        cshape->set_transform(node->get_transform());

        Node *owner = node->get_owner();
        Variant v_cshape(cshape);

        ur->add_do_method(node->get_parent(), "add_child", v_cshape);
        ur->add_do_method(node->get_parent(), "move_child", v_cshape, node->get_index() + 1);
        ur->add_do_method(cshape, "set_owner", Variant(owner));
        ur->add_do_reference(cshape);
        ur->add_undo_method(node->get_parent(), "remove_child", v_cshape);

        ur->commit_action();

    } break;

        case MENU_OPTION_CREATE_MULTIPLE_CONVEX_COLLISION_SHAPES: {

            if (node == get_tree()->get_edited_scene_root()) {
                err_dialog->set_text(TTR("Can't create multiple convex collision shapes for the scene root."));
                err_dialog->popup_centered_minsize();
                return;
            }

            Vector<Ref<Shape> > shapes = mesh->convex_decompose();

            if (shapes.empty()) {
                err_dialog->set_text(TTR("Couldn't create any collision shapes."));
                err_dialog->popup_centered_minsize();
                return;
            }
            UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();

            ur->create_action(TTR("Create Multiple Convex Shapes"));

            for (const Ref<Shape> & shp : shapes) {

                CollisionShape3D *cshape = memnew(CollisionShape3D);
                cshape->set_shape(shp);
                cshape->set_transform(node->get_transform());

                Node *owner = node->get_owner();

                ur->add_do_method(node->get_parent(), "add_child", Variant(cshape));
                ur->add_do_method(node->get_parent(), "move_child", Variant(cshape), node->get_index() + 1);
                ur->add_do_method(cshape, "set_owner", Variant(owner));
                ur->add_do_reference(cshape);
                ur->add_undo_method(node->get_parent(), "remove_child", Variant(cshape));
            }
            ur->commit_action();

        } break;

        case MENU_OPTION_CREATE_NAVMESH: {

            Ref<NavigationMesh> nmesh(make_ref_counted<NavigationMesh>());

            if (!nmesh) {
                return;
            }

            nmesh->create_from_mesh(mesh);
            NavigationMeshInstance *nmi = memnew(NavigationMeshInstance);
            nmi->set_navigation_mesh(nmesh);

            Node *owner = node == get_tree()->get_edited_scene_root() ? node : node->get_owner();

            UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
            ur->create_action(TTR("Create Navigation Mesh"));

            ur->add_do_method(node, "add_child", Variant(nmi));
            ur->add_do_method(nmi, "set_owner", Variant(owner));

            ur->add_do_reference(nmi);
            ur->add_undo_method(node, "remove_child", Variant(nmi));
            ur->commit_action();
        } break;

        case MENU_OPTION_CREATE_OUTLINE_MESH: {

            outline_dialog->popup_centered(Vector2(200, 90));
        } break;
        case MENU_OPTION_CREATE_UV2: {

            Ref<ArrayMesh> mesh2 = dynamic_ref_cast<ArrayMesh>(node->get_mesh());
            if (not mesh2) {
                err_dialog->set_text(TTR("Contained Mesh is not of type ArrayMesh."));
                err_dialog->popup_centered_minsize();
                return;
            }

            Error err = mesh2->lightmap_unwrap(node->get_global_transform());
            if (err != OK) {
                err_dialog->set_text(TTR("UV Unwrap failed, mesh may not be manifold?"));
                err_dialog->popup_centered_minsize();
                return;
            }

        } break;
        case MENU_OPTION_DEBUG_UV1: {
            Ref<Mesh> mesh2 = node->get_mesh();
            if (not mesh2) {
                err_dialog->set_text(TTR("No mesh to debug."));
                err_dialog->popup_centered_minsize();
                return;
            }
            _create_uv_lines(0);
        } break;
        case MENU_OPTION_DEBUG_UV2: {
            Ref<Mesh> mesh2 = node->get_mesh();
            if (not mesh2) {
                err_dialog->set_text(TTR("No mesh to debug."));
                err_dialog->popup_centered_minsize();
                return;
            }
            _create_uv_lines(1);
        } break;
    }
}

struct MeshInstanceEditorEdgeSort {

    Vector2 a;
    Vector2 b;

    bool operator<(const MeshInstanceEditorEdgeSort &p_b) const {
        if (a == p_b.a)
            return b < p_b.b;
        else
            return a < p_b.a;
    }

    MeshInstanceEditorEdgeSort() {}
    MeshInstanceEditorEdgeSort(const Vector2 &p_a, const Vector2 &p_b) {
        if (p_a < p_b) {
            a = p_a;
            b = p_b;
        } else {
            a = p_b;
            b = p_a;
        }
    }
};

void MeshInstanceEditor::_create_uv_lines(int p_layer) {

    Ref<Mesh> mesh = node->get_mesh();
    ERR_FAIL_COND(not mesh);

    Set<MeshInstanceEditorEdgeSort> edges;
    uv_lines.clear();
    for (int i = 0; i < mesh->get_surface_count(); i++) {
        if (mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES)
            continue;
        SurfaceArrays a = mesh->surface_get_arrays(i);

        const Vector<Vector2> &uv = p_layer == 0 ? a.m_uv_1 : a.m_uv_2;
        if (uv.size() == 0) {
            err_dialog->set_text(FormatVE(TTR("Mesh has no UV in layer %d.").asCString(), p_layer + 1));
            err_dialog->popup_centered_minsize();
            return;
        }

        const Vector<int> &indices = a.m_indices;

        int ic;
        bool use_indices = !indices.empty();

        if (indices.size()) {
            ic = indices.size();
        } else {
            ic = uv.size();
        }
        uv_lines.reserve(uv_lines.size()+(ic*3*2)/2);
        if (use_indices) {
            for (int j = 0; j < ic; j += 3) {

                for (int k = 0; k < 3; k++) {

                    MeshInstanceEditorEdgeSort edge;
                    edge.a = uv[indices[j + k]];
                    edge.b = uv[indices[j + (k + 1) % 3]];

                    if (edges.contains(edge))
                        continue;

                    uv_lines.push_back(edge.a);
                    uv_lines.push_back(edge.b);
                    edges.insert(edge);
                }
            }
        } else {
            for (int j = 0; j < ic; j += 3) {

                for (int k = 0; k < 3; k++) {

                    MeshInstanceEditorEdgeSort edge;
                    edge.a = uv[j + k];
                    edge.b = uv[j + (k + 1) % 3];

                    if (edges.contains(edge))
                        continue;

                    uv_lines.push_back(edge.a);
                    uv_lines.push_back(edge.b);
                    edges.insert(edge);
                }
            }
        }
    }

    debug_uv_dialog->popup_centered_minsize();
}

void MeshInstanceEditor::_debug_uv_draw() {

    if (uv_lines.empty()) {
        return;
    }

    debug_uv->set_clip_contents(true);
    debug_uv->draw_rect_filled(Rect2(Vector2(), debug_uv->get_size()), get_theme_color("dark_color_3", "Editor"));
    debug_uv->draw_set_transform(Vector2(), 0, debug_uv->get_size());
    // Use a translucent color to allow overlapping triangles to be visible.
    debug_uv->draw_multiline(uv_lines, get_theme_color("mono_color", "Editor") * Color(1, 1, 1, 0.5), Math::round(EDSCALE));
}

void MeshInstanceEditor::_create_outline_mesh() {

    Ref<Mesh> mesh = node->get_mesh();
    if (not mesh) {
        err_dialog->set_text(TTR("MeshInstance3D lacks a Mesh!"));
        err_dialog->popup_centered_minsize();
        return;
    }

    if (mesh->get_surface_count() == 0) {
        err_dialog->set_text(TTR("Mesh has not surface to create outlines from!"));
        err_dialog->popup_centered_minsize();
        return;
    }
    if (mesh->get_surface_count() == 1 && mesh->surface_get_primitive_type(0) != Mesh::PRIMITIVE_TRIANGLES) {
        err_dialog->set_text(TTR("Mesh primitive type is not PRIMITIVE_TRIANGLES!"));
        err_dialog->popup_centered_minsize();
        return;
    }

    Ref<Mesh> mesho = mesh->create_outline(outline_size->get_value());

    if (not mesho) {
        err_dialog->set_text(TTR("Could not create outline!"));
        err_dialog->popup_centered_minsize();
        return;
    }

    MeshInstance3D *mi = memnew(MeshInstance3D);
    mi->set_mesh(mesho);
    Node *owner = node->get_owner();
    if (get_tree()->get_edited_scene_root() == node) {
        owner = node;
    }

    UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();

    ur->create_action(TTR("Create Outline"));

    ur->add_do_method(node, "add_child", Variant(mi));
    ur->add_do_method(mi, "set_owner", Variant(owner));

    ur->add_do_reference(mi);
    ur->add_undo_method(node, "remove_child", Variant(mi));
    ur->commit_action();
}

MeshInstanceEditor::MeshInstanceEditor() {

    options = memnew(MenuButton);
    options->set_switch_on_hover(true);
    Node3DEditor::get_singleton()->add_control_to_menu_panel(options);

    options->set_text(TTR("Mesh"));
    options->set_button_icon(EditorNode::get_singleton()->get_gui_base()->get_theme_icon("MeshInstance3D", "EditorIcons"));

    options->get_popup()->add_item(TTR("Create Trimesh Static Body"), MENU_OPTION_CREATE_STATIC_TRIMESH_BODY);
    options->get_popup()->set_item_tooltip(options->get_popup()->get_item_count() - 1, TTR("Creates a StaticBody3D and assigns a polygon-based collision shape to it automatically.\nThis is the most accurate (but slowest) option for collision detection."));
    options->get_popup()->add_separator();
    options->get_popup()->add_item(TTR("Create Trimesh Collision Sibling"), MENU_OPTION_CREATE_TRIMESH_COLLISION_SHAPE);
    options->get_popup()->set_item_tooltip(options->get_popup()->get_item_count() - 1, TTR("Creates a polygon-based collision shape.\nThis is the most accurate (but slowest) option for collision detection."));
    options->get_popup()->add_item(TTR("Create Single Convex Collision Sibling"), MENU_OPTION_CREATE_SINGLE_CONVEX_COLLISION_SHAPE);
    options->get_popup()->set_item_tooltip(options->get_popup()->get_item_count() - 1, TTR("Creates a single convex collision shape.\nThis is the fastest (but least accurate) option for collision detection."));
    options->get_popup()->add_item(TTR("Create Simplified Convex Collision Sibling"), MENU_OPTION_CREATE_SIMPLIFIED_CONVEX_COLLISION_SHAPE);
    options->get_popup()->set_item_tooltip(options->get_popup()->get_item_count() - 1, TTR("Creates a simplified convex collision shape.\nThis is similar to single collision shape, but can result in a simpler geometry in some cases, at the cost of accuracy."));
    options->get_popup()->add_item(TTR("Create Multiple Convex Collision Siblings"), MENU_OPTION_CREATE_MULTIPLE_CONVEX_COLLISION_SHAPES);
    options->get_popup()->set_item_tooltip(options->get_popup()->get_item_count() - 1, TTR("Creates a polygon-based collision shape.\nThis is a performance middle-ground between a single convex collision and a polygon-based collision."));
    options->get_popup()->add_separator();
    options->get_popup()->add_item(TTR("Create Navigation Mesh"), MENU_OPTION_CREATE_NAVMESH);
    options->get_popup()->add_separator();
    options->get_popup()->add_item(TTR("Create Outline Mesh..."), MENU_OPTION_CREATE_OUTLINE_MESH);
    options->get_popup()->set_item_tooltip(options->get_popup()->get_item_count() - 1, TTR("Creates a static outline mesh. The outline mesh will have its normals flipped automatically.\nThis can be used instead of the SpatialMaterial Grow property when using that property isn't possible."));
    options->get_popup()->add_separator();
    options->get_popup()->add_item(TTR("View UV1"), MENU_OPTION_DEBUG_UV1);
    options->get_popup()->add_item(TTR("View UV2"), MENU_OPTION_DEBUG_UV2);
    options->get_popup()->add_item(TTR("Unwrap UV2 for Lightmap/AO"), MENU_OPTION_CREATE_UV2);

    options->get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_menu_option));

    outline_dialog = memnew(ConfirmationDialog);
    outline_dialog->set_title(TTR("Create Outline Mesh"));
    outline_dialog->get_ok()->set_text(TTR("Create"));

    VBoxContainer *outline_dialog_vbc = memnew(VBoxContainer);
    outline_dialog->add_child(outline_dialog_vbc);
    //outline_dialog->set_child_rect(outline_dialog_vbc);

    outline_size = memnew(SpinBox);
    outline_size->set_min(0.001f);
    outline_size->set_max(1024);
    outline_size->set_step(0.001f);
    outline_size->set_value(0.05f);
    outline_dialog_vbc->add_margin_child(TTR("Outline Size:"), outline_size);

    add_child(outline_dialog);
    outline_dialog->connect("confirmed",callable_mp(this, &ClassName::_create_outline_mesh));

    err_dialog = memnew(AcceptDialog);
    add_child(err_dialog);

    debug_uv_dialog = memnew(AcceptDialog);
    debug_uv_dialog->set_title(TTR("UV Channel Debug"));
    add_child(debug_uv_dialog);
    debug_uv = memnew(Control);
    debug_uv->set_custom_minimum_size(Size2(600, 600) * EDSCALE);
    debug_uv->connect("draw",callable_mp(this, &ClassName::_debug_uv_draw));
    debug_uv_dialog->add_child(debug_uv);
}

void MeshInstanceEditorPlugin::edit(Object *p_object) {

    mesh_editor->edit(object_cast<MeshInstance3D>(p_object));
}

bool MeshInstanceEditorPlugin::handles(Object *p_object) const {

    return p_object->is_class("MeshInstance3D");
}

void MeshInstanceEditorPlugin::make_visible(bool p_visible) {

    if (p_visible) {
        mesh_editor->options->show();
    } else {

        mesh_editor->options->hide();
        mesh_editor->edit(nullptr);
    }
}

MeshInstanceEditorPlugin::MeshInstanceEditorPlugin(EditorNode *p_node) {

    editor = p_node;
    mesh_editor = memnew(MeshInstanceEditor);
    editor->get_viewport()->add_child(mesh_editor);

    mesh_editor->options->hide();
}

MeshInstanceEditorPlugin::~MeshInstanceEditorPlugin() {
}
