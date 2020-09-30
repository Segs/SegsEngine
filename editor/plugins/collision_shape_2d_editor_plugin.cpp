/*************************************************************************/
/*  collision_shape_2d_editor_plugin.cpp                                 */
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

#include "collision_shape_2d_editor_plugin.h"

#include "core/callable_method_pointer.h"
#include "canvas_item_editor_plugin.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/capsule_shape_2d.h"
#include "scene/resources/circle_shape_2d.h"
#include "scene/resources/concave_polygon_shape_2d.h"
#include "scene/resources/convex_polygon_shape_2d.h"
#include "scene/resources/line_shape_2d.h"
#include "scene/resources/rectangle_shape_2d.h"
#include "scene/resources/segment_shape_2d.h"
#include "core/translation_helpers.h"

IMPL_GDCLASS(CollisionShape2DEditor)
IMPL_GDCLASS(CollisionShape2DEditorPlugin)



void CollisionShape2DEditor::_node_removed(Node *p_node) {

    if (p_node == node) {
        node = nullptr;
    }
}

Variant CollisionShape2DEditor::get_handle_value(int idx) const {

    switch (shape_type) {
        case CAPSULE_SHAPE: {
            Ref<CapsuleShape2D> capsule = dynamic_ref_cast<CapsuleShape2D>(node->get_shape());

            if (idx == 0) {
                return capsule->get_radius();
            } else if (idx == 1) {
                return capsule->get_height();
            }

        } break;

        case CIRCLE_SHAPE: {
            Ref<CircleShape2D> circle = dynamic_ref_cast<CircleShape2D>(node->get_shape());

            if (idx == 0) {
                return circle->get_radius();
            }

        } break;

        case CONCAVE_POLYGON_SHAPE: {
            // Cannot be edited directly, use CollisionPolygon2D instead.
        } break;

        case CONVEX_POLYGON_SHAPE: {
            // Cannot be edited directly, use CollisionPolygon2D instead.
        } break;

        case LINE_SHAPE: {
            Ref<LineShape2D> line = dynamic_ref_cast<LineShape2D>(node->get_shape());

            if (idx == 0) {
                return line->get_d();
            } else {
                return line->get_normal();
            }

        } break;

        case RAY_SHAPE: {
            Ref<RayShape2D> ray = dynamic_ref_cast<RayShape2D>(node->get_shape());

            if (idx == 0) {
                return ray->get_length();
            }

        } break;

        case RECTANGLE_SHAPE: {
            Ref<RectangleShape2D> rect = dynamic_ref_cast<RectangleShape2D>(node->get_shape());

            if (idx < 3) {
                return rect->get_extents().abs();
            }

        } break;

        case SEGMENT_SHAPE: {
            Ref<SegmentShape2D> seg = dynamic_ref_cast<SegmentShape2D>(node->get_shape());

            if (idx == 0) {
                return seg->get_a();
            } else if (idx == 1) {
                return seg->get_b();
            }

        } break;
    }

    return Variant();
}

void CollisionShape2DEditor::set_handle(int idx, Point2 &p_point) {

    switch (shape_type) {
        case CAPSULE_SHAPE: {
            if (idx < 2) {
                Ref<CapsuleShape2D> capsule = dynamic_ref_cast<CapsuleShape2D>(node->get_shape());

                real_t parameter = Math::abs(p_point[idx]);

                if (idx == 0) {
                    capsule->set_radius(parameter);
                } else if (idx == 1) {
                    capsule->set_height(parameter * 2 - capsule->get_radius() * 2);
                }

                canvas_item_editor->update_viewport();
            }

        } break;

        case CIRCLE_SHAPE: {
            Ref<CircleShape2D> circle = dynamic_ref_cast<CircleShape2D>(node->get_shape());
            circle->set_radius(p_point.length());

            canvas_item_editor->update_viewport();

        } break;

        case CONCAVE_POLYGON_SHAPE: {

        } break;

        case CONVEX_POLYGON_SHAPE: {

        } break;

        case LINE_SHAPE: {
            if (idx < 2) {
                Ref<LineShape2D> line = dynamic_ref_cast<LineShape2D>(node->get_shape());

                if (idx == 0) {
                    line->set_d(p_point.length());
                } else {
                    line->set_normal(p_point.normalized());
                }

                canvas_item_editor->update_viewport();
            }

        } break;

        case RAY_SHAPE: {
            Ref<RayShape2D> ray = dynamic_ref_cast<RayShape2D>(node->get_shape());

            ray->set_length(Math::abs(p_point.y));

            canvas_item_editor->update_viewport();

        } break;

        case RECTANGLE_SHAPE: {
            if (idx < 3) {
                Ref<RectangleShape2D> rect = dynamic_ref_cast<RectangleShape2D>(node->get_shape());

                Vector2 extents = rect->get_extents();
                if (idx == 2) {
                    extents = p_point;
                } else {
                    extents[idx] = p_point[idx];
                }
                rect->set_extents(extents.abs());

                canvas_item_editor->update_viewport();
            }

        } break;

        case SEGMENT_SHAPE: {
            if (edit_handle < 2) {
                Ref<SegmentShape2D> seg = dynamic_ref_cast<SegmentShape2D>(node->get_shape());

                if (idx == 0) {
                    seg->set_a(p_point);
                } else if (idx == 1) {
                    seg->set_b(p_point);
                }

                canvas_item_editor->update_viewport();
            }

        } break;
    }
    Object_change_notify(node->get_shape().get());
}

void CollisionShape2DEditor::commit_handle(int idx, Variant &p_org) {

    undo_redo->create_action(TTR("Set Handle"));

    switch (shape_type) {
        case CAPSULE_SHAPE: {
            Ref<CapsuleShape2D> capsule = dynamic_ref_cast<CapsuleShape2D>(node->get_shape());

            if (idx == 0) {
                undo_redo->add_do_method(capsule.get(), "set_radius", capsule->get_radius());
                undo_redo->add_do_method(canvas_item_editor, "update_viewport");
                undo_redo->add_undo_method(capsule.get(), "set_radius", p_org);
                undo_redo->add_do_method(canvas_item_editor, "update_viewport");
            } else if (idx == 1) {
                undo_redo->add_do_method(capsule.get(), "set_height", capsule->get_height());
                undo_redo->add_do_method(canvas_item_editor, "update_viewport");
                undo_redo->add_undo_method(capsule.get(), "set_height", p_org);
                undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
            }

        } break;

        case CIRCLE_SHAPE: {
            Ref<CircleShape2D> circle = dynamic_ref_cast<CircleShape2D>(node->get_shape());

            undo_redo->add_do_method(circle.get(), "set_radius", circle->get_radius());
            undo_redo->add_do_method(canvas_item_editor, "update_viewport");
            undo_redo->add_undo_method(circle.get(), "set_radius", p_org);
            undo_redo->add_undo_method(canvas_item_editor, "update_viewport");

        } break;

        case CONCAVE_POLYGON_SHAPE: {

        } break;

        case CONVEX_POLYGON_SHAPE: {

        } break;

        case LINE_SHAPE: {
            Ref<LineShape2D> line = dynamic_ref_cast<LineShape2D>(node->get_shape());

            if (idx == 0) {
                undo_redo->add_do_method(line.get(), "set_d", line->get_d());
                undo_redo->add_do_method(canvas_item_editor, "update_viewport");
                undo_redo->add_undo_method(line.get(), "set_d", p_org);
                undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
            } else {
                undo_redo->add_do_method(line.get(), "set_normal", line->get_normal());
                undo_redo->add_do_method(canvas_item_editor, "update_viewport");
                undo_redo->add_undo_method(line.get(), "set_normal", p_org);
                undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
            }

        } break;

        case RAY_SHAPE: {
            Ref<RayShape2D> ray = dynamic_ref_cast<RayShape2D>(node->get_shape());

            undo_redo->add_do_method(ray.get(), "set_length", ray->get_length());
            undo_redo->add_do_method(canvas_item_editor, "update_viewport");
            undo_redo->add_undo_method(ray.get(), "set_length", p_org);
            undo_redo->add_undo_method(canvas_item_editor, "update_viewport");

        } break;

        case RECTANGLE_SHAPE: {
            Ref<RectangleShape2D> rect = dynamic_ref_cast<RectangleShape2D>(node->get_shape());

            undo_redo->add_do_method(rect.get(), "set_extents", rect->get_extents());
            undo_redo->add_do_method(canvas_item_editor, "update_viewport");
            undo_redo->add_undo_method(rect.get(), "set_extents", p_org);
            undo_redo->add_undo_method(canvas_item_editor, "update_viewport");

        } break;

        case SEGMENT_SHAPE: {
            Ref<SegmentShape2D> seg = dynamic_ref_cast<SegmentShape2D>(node->get_shape());
            if (idx == 0) {
                undo_redo->add_do_method(seg.get(), "set_a", seg->get_a());
                undo_redo->add_do_method(canvas_item_editor, "update_viewport");
                undo_redo->add_undo_method(seg.get(), "set_a", p_org);
                undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
            } else if (idx == 1) {
                undo_redo->add_do_method(seg.get(), "set_b", seg->get_b());
                undo_redo->add_do_method(canvas_item_editor, "update_viewport");
                undo_redo->add_undo_method(seg.get(), "set_b", p_org);
                undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
            }

        } break;
    }

    undo_redo->commit_action();
}

bool CollisionShape2DEditor::forward_canvas_gui_input(const Ref<InputEvent> &p_event) {

    if (!node) {
        return false;
    }

    if (!node->get_shape()) {
        return false;
    }

    if (shape_type == -1) {
        return false;
    }

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);
    Transform2D xform = canvas_item_editor->get_canvas_transform() * node->get_global_transform();

    if (mb) {

        Vector2 gpoint = mb->get_position();

        if (mb->get_button_index() == BUTTON_LEFT) {
            if (mb->is_pressed()) {
                for (int i = 0; i < handles.size(); i++) {
                    if (xform.xform(handles[i]).distance_to(gpoint) < 8) {
                        edit_handle = i;

                        break;
                    }
                }

                if (edit_handle == -1) {
                    pressed = false;

                    return false;
                }

                original = get_handle_value(edit_handle);
                pressed = true;

                return true;

            } else {
                if (pressed) {
                    commit_handle(edit_handle, original);

                    edit_handle = -1;
                    pressed = false;

                    return true;
                }
            }
        }

        return false;
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);

    if (mm) {

        if (edit_handle == -1 || !pressed) {
            return false;
        }

        Vector2 cpoint = canvas_item_editor->snap_point(canvas_item_editor->get_canvas_transform().affine_inverse().xform(mm->get_position()));
        cpoint = node->get_global_transform().affine_inverse().xform(cpoint);

        set_handle(edit_handle, cpoint);

        return true;
    }

    return false;
}

void CollisionShape2DEditor::_get_current_shape_type() {

    if (!node) {
        return;
    }

    Ref<Shape2D> s = node->get_shape();

    if (not s) {
        return;
    }

    if (dynamic_ref_cast<CapsuleShape2D>(s)) {
        shape_type = CAPSULE_SHAPE;
    } else if (dynamic_ref_cast<CircleShape2D>(s)) {
        shape_type = CIRCLE_SHAPE;
    } else if (dynamic_ref_cast<ConcavePolygonShape2D>(s)) {
        shape_type = CONCAVE_POLYGON_SHAPE;
    } else if (dynamic_ref_cast<ConvexPolygonShape2D>(s)) {
        shape_type = CONVEX_POLYGON_SHAPE;
    } else if (dynamic_ref_cast<LineShape2D>(s)) {
        shape_type = LINE_SHAPE;
    } else if (dynamic_ref_cast<RayShape2D>(s)) {
        shape_type = RAY_SHAPE;
    } else if (dynamic_ref_cast<RectangleShape2D>(s)) {
        shape_type = RECTANGLE_SHAPE;
    } else if (dynamic_ref_cast<SegmentShape2D>(s)) {
        shape_type = SEGMENT_SHAPE;
    } else {
        shape_type = -1;
    }

    canvas_item_editor->update_viewport();
}

void CollisionShape2DEditor::forward_canvas_draw_over_viewport(Control *p_overlay) {

    if (!node) {
        return;
    }

    if (not node->get_shape()) {
        return;
    }

    _get_current_shape_type();

    if (shape_type == -1) {
        return;
    }

    Transform2D gt = canvas_item_editor->get_canvas_transform() * node->get_global_transform();

    Ref<Texture> h = get_icon("EditorHandle", "EditorIcons");
    Vector2 size = h->get_size() * 0.5;

    handles.clear();

    switch (shape_type) {
        case CAPSULE_SHAPE: {
            Ref<CapsuleShape2D> shape = dynamic_ref_cast<CapsuleShape2D>(node->get_shape());

            handles.resize(2);
            float radius = shape->get_radius();
            float height = shape->get_height() / 2;

            handles[0] = Point2(radius, -height);
            handles[1] = Point2(0, -(height + radius));

            p_overlay->draw_texture(h, gt.xform(handles[0]) - size);
            p_overlay->draw_texture(h, gt.xform(handles[1]) - size);

        } break;

        case CIRCLE_SHAPE: {
            Ref<CircleShape2D> shape = dynamic_ref_cast<CircleShape2D>(node->get_shape());

            handles.resize(1);
            handles[0] = Point2(shape->get_radius(), 0);

            p_overlay->draw_texture(h, gt.xform(handles[0]) - size);

        } break;

        case CONCAVE_POLYGON_SHAPE: {

        } break;

        case CONVEX_POLYGON_SHAPE: {

        } break;

        case LINE_SHAPE: {
            Ref<LineShape2D> shape = dynamic_ref_cast<LineShape2D>(node->get_shape());

            handles.resize(2);
            handles[0] = shape->get_normal() * shape->get_d();
            handles[1] = shape->get_normal() * (shape->get_d() + 30.0);

            p_overlay->draw_texture(h, gt.xform(handles[0]) - size);
            p_overlay->draw_texture(h, gt.xform(handles[1]) - size);

        } break;

        case RAY_SHAPE: {
            Ref<RayShape2D> shape = dynamic_ref_cast<RayShape2D>(node->get_shape());

            handles.resize(1);
            handles[0] = Point2(0, shape->get_length());

            p_overlay->draw_texture(h, gt.xform(handles[0]) - size);

        } break;

        case RECTANGLE_SHAPE: {
            Ref<RectangleShape2D> shape = dynamic_ref_cast<RectangleShape2D>(node->get_shape());

            handles.resize(3);
            Vector2 ext = shape->get_extents();
            handles[0] = Point2(ext.x, 0);
            handles[1] = Point2(0, -ext.y);
            handles[2] = Point2(ext.x, -ext.y);

            p_overlay->draw_texture(h, gt.xform(handles[0]) - size);
            p_overlay->draw_texture(h, gt.xform(handles[1]) - size);
            p_overlay->draw_texture(h, gt.xform(handles[2]) - size);

        } break;

        case SEGMENT_SHAPE: {
            Ref<SegmentShape2D> shape = dynamic_ref_cast<SegmentShape2D>(node->get_shape());

            handles.resize(2);
            handles[0] = shape->get_a();
            handles[1] = shape->get_b();

            p_overlay->draw_texture(h, gt.xform(handles[0]) - size);
            p_overlay->draw_texture(h, gt.xform(handles[1]) - size);

        } break;
    }
}

void CollisionShape2DEditor::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {
            get_tree()->connect("node_removed",callable_mp(this, &ClassName::_node_removed));
        } break;

        case NOTIFICATION_EXIT_TREE: {
            get_tree()->disconnect("node_removed",callable_mp(this, &ClassName::_node_removed));
        } break;
    }
}

void CollisionShape2DEditor::edit(Node *p_node) {

    if (!canvas_item_editor) {
        canvas_item_editor = CanvasItemEditor::get_singleton();
    }

    if (p_node) {
        node = object_cast<CollisionShape2D>(p_node);

        _get_current_shape_type();

    } else {
        edit_handle = -1;
        shape_type = -1;

        node = nullptr;
    }

    canvas_item_editor->update_viewport();
}

void CollisionShape2DEditor::_bind_methods() {

    MethodBinder::bind_method("_get_current_shape_type", &CollisionShape2DEditor::_get_current_shape_type);
    MethodBinder::bind_method("_node_removed", &CollisionShape2DEditor::_node_removed);
}

CollisionShape2DEditor::CollisionShape2DEditor(EditorNode *p_editor) {

    node = nullptr;
    canvas_item_editor = nullptr;
    editor = p_editor;

    undo_redo = p_editor->get_undo_redo();

    edit_handle = -1;
    pressed = false;
}

void CollisionShape2DEditorPlugin::edit(Object *p_obj) {

    collision_shape_2d_editor->edit(object_cast<Node>(p_obj));
}

bool CollisionShape2DEditorPlugin::handles(Object *p_obj) const {

    return p_obj->is_class("CollisionShape2D");
}

void CollisionShape2DEditorPlugin::make_visible(bool visible) {

    if (!visible) {
        edit(nullptr);
    }
}

CollisionShape2DEditorPlugin::CollisionShape2DEditorPlugin(EditorNode *p_editor) {

    editor = p_editor;

    collision_shape_2d_editor = memnew(CollisionShape2DEditor(p_editor));
    p_editor->get_gui_base()->add_child(collision_shape_2d_editor);
}

CollisionShape2DEditorPlugin::~CollisionShape2DEditorPlugin() {}
