/*************************************************************************/
/*  csg_gizmos.cpp                                                       */
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

#include "csg_gizmos.h"

#include "core/class_db.h"
#include "core/math/geometry.h"
#include "core/string.h"
#include "core/translation_helpers.h"
#include "editor/editor_settings.h"
///////////
IMPL_GDCLASS(CSGShapeSpatialGizmoPlugin)
IMPL_GDCLASS(EditorPluginCSG)

CSGShapeSpatialGizmoPlugin::CSGShapeSpatialGizmoPlugin() {

    Color gizmo_color = EDITOR_DEF_T("editors/3d_gizmos/gizmo_colors/csg", Color(0.0, 0.4f, 1, 0.15f));
    create_material("shape_union_material", gizmo_color);
    create_material("shape_union_solid_material", gizmo_color);
    gizmo_color.invert();
    create_material("shape_subtraction_material", gizmo_color);
    create_material("shape_subtraction_solid_material", gizmo_color);
    gizmo_color.r = 0.95f;
    gizmo_color.g = 0.95f;
    gizmo_color.b = 0.95f;
    create_material("shape_intersection_material", gizmo_color);
    create_material("shape_intersection_solid_material", gizmo_color);

    create_handle_material("handles");
}

StringName CSGShapeSpatialGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_idx) const {

    CSGShape *cs = object_cast<CSGShape>(p_gizmo->get_spatial_node());

    if (object_cast<CSGSphere>(cs)) {

        return "Radius";
    }

    if (object_cast<CSGBox>(cs)) {

        static const char *hname[3] = { "Width", "Height", "Depth" };
        return StringName(hname[p_idx]);
    }

    if (object_cast<CSGCylinder>(cs)) {

        return StringName(p_idx == 0 ? "Radius" : "Height");
    }

    if (object_cast<CSGTorus>(cs)) {

        return StringName(p_idx == 0 ? "InnerRadius" : "OuterRadius");
    }

    return StringName();
}
Variant CSGShapeSpatialGizmoPlugin::get_handle_value(EditorNode3DGizmo *p_gizmo, int p_idx) const {

    CSGShape *cs = object_cast<CSGShape>(p_gizmo->get_spatial_node());

    if (object_cast<CSGSphere>(cs)) {

        CSGSphere *s = object_cast<CSGSphere>(cs);
        return s->get_radius();
    }

    if (object_cast<CSGBox>(cs)) {

        CSGBox *s = object_cast<CSGBox>(cs);
        switch (p_idx) {
            case 0:
                return s->get_width();
            case 1:
                return s->get_height();
            case 2:
                return s->get_depth();
        }
    }

    if (object_cast<CSGCylinder>(cs)) {

        CSGCylinder *s = object_cast<CSGCylinder>(cs);
        return p_idx == 0 ? s->get_radius() : s->get_height();
    }

    if (object_cast<CSGTorus>(cs)) {

        CSGTorus *s = object_cast<CSGTorus>(cs);
        return p_idx == 0 ? s->get_inner_radius() : s->get_outer_radius();
    }

    return Variant();
}
void CSGShapeSpatialGizmoPlugin::set_handle(
        EditorNode3DGizmo *p_gizmo, int p_idx, Camera3D *p_camera, const Point2 &p_point) {

    CSGShape *cs = object_cast<CSGShape>(p_gizmo->get_spatial_node());

    Transform gt = cs->get_global_transform();
    //gt.orthonormalize();
    Transform gi = gt.affine_inverse();

    Vector3 ray_from = p_camera->project_ray_origin(p_point);
    Vector3 ray_dir = p_camera->project_ray_normal(p_point);

    Vector3 sg[2] = { gi.xform(ray_from), gi.xform(ray_from + ray_dir * 16384) };

    if (object_cast<CSGSphere>(cs)) {

        CSGSphere *s = object_cast<CSGSphere>(cs);

        Vector3 ra, rb;
        Geometry::get_closest_points_between_segments(Vector3(), Vector3(4096, 0, 0), sg[0], sg[1], ra, rb);
        float d = ra.x;
        if (Node3DEditor::get_singleton()->is_snap_enabled()) {
            d = Math::stepify(d, Node3DEditor::get_singleton()->get_translate_snap());
        }

        if (d < 0.001f)
            d = 0.001f;

        s->set_radius(d);
    }

    if (object_cast<CSGBox>(cs)) {

        CSGBox *s = object_cast<CSGBox>(cs);

        Vector3 axis;
        axis[p_idx] = 1.0;
        Vector3 ra, rb;
        Geometry::get_closest_points_between_segments(Vector3(), axis * 4096, sg[0], sg[1], ra, rb);
        float d = ra[p_idx];
        if (Math::is_nan(d)) {
            // The handle is perpendicular to the camera.
            return;
        }
        if (Node3DEditor::get_singleton()->is_snap_enabled()) {
            d = Math::stepify(d, Node3DEditor::get_singleton()->get_translate_snap());
        }

        if (d < 0.001f)
            d = 0.001f;

        switch (p_idx) {
            case 0:
                s->set_width(d * 2);
                break;
            case 1:
                s->set_height(d * 2);
                break;
            case 2:
                s->set_depth(d * 2);
                break;
        }
    }

    if (object_cast<CSGCylinder>(cs)) {

        CSGCylinder *s = object_cast<CSGCylinder>(cs);

        Vector3 axis;
        axis[p_idx == 0 ? 0 : 1] = 1.0;
        Vector3 ra, rb;
        Geometry::get_closest_points_between_segments(Vector3(), axis * 4096, sg[0], sg[1], ra, rb);
        float d = axis.dot(ra);
        if (Node3DEditor::get_singleton()->is_snap_enabled()) {
            d = Math::stepify(d, Node3DEditor::get_singleton()->get_translate_snap());
        }

        if (d < 0.001f)
            d = 0.001f;

        if (p_idx == 0)
            s->set_radius(d);
        else if (p_idx == 1)
            s->set_height(d * 2.0f);
    }

    if (object_cast<CSGTorus>(cs)) {

        CSGTorus *s = object_cast<CSGTorus>(cs);

        Vector3 axis;
        axis[0] = 1.0;
        Vector3 ra, rb;
        Geometry::get_closest_points_between_segments(Vector3(), axis * 4096, sg[0], sg[1], ra, rb);
        float d = axis.dot(ra);
        if (Node3DEditor::get_singleton()->is_snap_enabled()) {
            d = Math::stepify(d, Node3DEditor::get_singleton()->get_translate_snap());
        }

        if (d < 0.001)
            d = 0.001f;

        if (p_idx == 0)
            s->set_inner_radius(d);
        else if (p_idx == 1)
            s->set_outer_radius(d);
    }
}
void CSGShapeSpatialGizmoPlugin::commit_handle(
        EditorNode3DGizmo *p_gizmo, int p_idx, const Variant &p_restore, bool p_cancel) {

    CSGShape *cs = object_cast<CSGShape>(p_gizmo->get_spatial_node());

    if (object_cast<CSGSphere>(cs)) {
        CSGSphere *s = object_cast<CSGSphere>(cs);
        if (p_cancel) {
            s->set_radius(p_restore.as<float>());
            return;
        }

        UndoRedo *ur = Node3DEditor::get_singleton()->get_undo_redo();
        ur->create_action(TTR("Change Sphere Shape Radius"));
        ur->add_do_method(s, "set_radius", s->get_radius());
        ur->add_undo_method(s, "set_radius", p_restore);
        ur->commit_action();
    }

    if (object_cast<CSGBox>(cs)) {
        CSGBox *s = object_cast<CSGBox>(cs);
        if (p_cancel) {
            switch (p_idx) {
                case 0:
                    s->set_width(p_restore.as<float>());
                    break;
                case 1:
                    s->set_height(p_restore.as<float>());
                    break;
                case 2:
                    s->set_depth(p_restore.as<float>());
                    break;
            }
            return;
        }

        UndoRedo *ur = Node3DEditor::get_singleton()->get_undo_redo();
        ur->create_action(TTR("Change Box Shape Extents"));
        static const char *method[3] = { "set_width", "set_height", "set_depth" };
        float current = 0;
        switch (p_idx) {
            case 0:
                current = s->get_width();
                break;
            case 1:
                current = s->get_height();
                break;
            case 2:
                current = s->get_depth();
                break;
        }

        ur->add_do_method(s, StaticCString(method[p_idx],true), current);
        ur->add_undo_method(s, StaticCString(method[p_idx],true), p_restore);
        ur->commit_action();
    }

    if (object_cast<CSGCylinder>(cs)) {
        CSGCylinder *s = object_cast<CSGCylinder>(cs);
        if (p_cancel) {
            if (p_idx == 0)
                s->set_radius(p_restore.as<float>());
            else
                s->set_height(p_restore.as<float>());
            return;
        }

        UndoRedo *ur = Node3DEditor::get_singleton()->get_undo_redo();
        if (p_idx == 0) {
            ur->create_action(TTR("Change Cylinder Radius"));
            ur->add_do_method(s, "set_radius", s->get_radius());
            ur->add_undo_method(s, "set_radius", p_restore);
        } else {
            ur->create_action(TTR("Change Cylinder Height"));
            ur->add_do_method(s, "set_height", s->get_height());
            ur->add_undo_method(s, "set_height", p_restore);
        }

        ur->commit_action();
    }

    if (object_cast<CSGTorus>(cs)) {
        CSGTorus *s = object_cast<CSGTorus>(cs);
        if (p_cancel) {
            if (p_idx == 0)
                s->set_inner_radius(p_restore.as<float>());
            else
                s->set_outer_radius(p_restore.as<float>());
            return;
        }

        UndoRedo *ur = Node3DEditor::get_singleton()->get_undo_redo();
        if (p_idx == 0) {
            ur->create_action(TTR("Change Torus Inner Radius"));
            ur->add_do_method(s, "set_inner_radius", s->get_inner_radius());
            ur->add_undo_method(s, "set_inner_radius", p_restore);
        } else {
            ur->create_action(TTR("Change Torus Outer Radius"));
            ur->add_do_method(s, "set_outer_radius", s->get_outer_radius());
            ur->add_undo_method(s, "set_outer_radius", p_restore);
        }

        ur->commit_action();
    }
}
bool CSGShapeSpatialGizmoPlugin::has_gizmo(Node3D *p_spatial) {
    return object_cast<CSGSphere>(p_spatial) || object_cast<CSGBox>(p_spatial) || object_cast<CSGCylinder>(p_spatial) ||
           object_cast<CSGTorus>(p_spatial) || object_cast<CSGMesh>(p_spatial) || object_cast<CSGPolygon>(p_spatial);
}

StringView CSGShapeSpatialGizmoPlugin::get_name() const {
    return "CSGShapes";
}

int CSGShapeSpatialGizmoPlugin::get_priority() const {
    return -1;
}

bool CSGShapeSpatialGizmoPlugin::is_selectable_when_hidden() const {
    return true;
}

void CSGShapeSpatialGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
    p_gizmo->clear();

    CSGShape *cs = object_cast<CSGShape>(p_gizmo->get_spatial_node());
    Vector<Vector3> faces = cs->get_brush_faces();
    if (faces.empty()) {
        return;
    }

    Vector<Vector3> lines;
    lines.resize(faces.size() * 2);
    {
        for (size_t i = 0; i < lines.size(); i += 6) {
            int f = i / 6;
            for (int j = 0; j < 3; j++) {
                int j_n = (j + 1) % 3;
                lines[i + j * 2 + 0] = faces[f * 3 + j];
                lines[i + j * 2 + 1] = faces[f * 3 + j_n];
            }
        }
    }

    Ref<Material> material;

    switch (cs->get_operation()) {
        case CSGShape::OPERATION_UNION:
            material = get_material("shape_union_material", p_gizmo);
            break;
        case CSGShape::OPERATION_INTERSECTION:
            material = get_material("shape_intersection_material", p_gizmo);
            break;
        case CSGShape::OPERATION_SUBTRACTION:
            material = get_material("shape_subtraction_material", p_gizmo);
            break;
    }

    Ref<Material> handles_material = get_material("handles");

    p_gizmo->add_lines(lines, material);
    p_gizmo->add_collision_segments(lines);

    Array csg_meshes = cs->get_meshes();
    if (csg_meshes.size() != 2) {
        return;
    }
    Ref<Mesh> csg_mesh = csg_meshes[1];
    if (csg_mesh) {
        p_gizmo->add_collision_triangles(csg_mesh->generate_triangle_mesh());
    }

    if (p_gizmo->is_selected()) {
        // Draw a translucent representation of the CSG node
        Ref<ArrayMesh> mesh(make_ref_counted<ArrayMesh>());
        SurfaceArrays array(eastl::move(faces));

        mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, eastl::move(array));

        Ref<Material> solid_material;
        switch (cs->get_operation()) {
            case CSGShape::OPERATION_UNION:
                solid_material = get_material("shape_union_solid_material", p_gizmo);
                break;
            case CSGShape::OPERATION_INTERSECTION:
                solid_material = get_material("shape_intersection_solid_material", p_gizmo);
                break;
            case CSGShape::OPERATION_SUBTRACTION:
                solid_material = get_material("shape_subtraction_solid_material", p_gizmo);
                break;
        }

        p_gizmo->add_mesh(mesh, false, Ref<SkinReference>(), solid_material);
    }

    if (object_cast<CSGSphere>(cs)) {
        CSGSphere *s = object_cast<CSGSphere>(cs);

        float r = s->get_radius();
        Vector<Vector3> handles {1,Vector3(r, 0, 0)};
        p_gizmo->add_handles(eastl::move(handles), handles_material);
    }

    if (object_cast<CSGBox>(cs)) {
        CSGBox *s = object_cast<CSGBox>(cs);

        Vector<Vector3> handles{ Vector3(s->get_width() * 0.5f, 0, 0), Vector3(0, s->get_height() * 0.5f, 0),
            Vector3(0, 0, s->get_depth() * 0.5f) };
        p_gizmo->add_handles(eastl::move(handles), handles_material);
    }

    if (object_cast<CSGCylinder>(cs)) {
        CSGCylinder *s = object_cast<CSGCylinder>(cs);
        Vector<Vector3> handles{ Vector3(s->get_radius(), 0, 0), Vector3(0, s->get_height() * 0.5f, 0) };
        p_gizmo->add_handles(eastl::move(handles), handles_material);
    }

    if (object_cast<CSGTorus>(cs)) {
        CSGTorus *s = object_cast<CSGTorus>(cs);
        Vector<Vector3> handles{ Vector3(s->get_inner_radius(), 0, 0), Vector3(s->get_outer_radius(), 0, 0) };

        p_gizmo->add_handles(eastl::move(handles), handles_material);
    }
}

EditorPluginCSG::EditorPluginCSG(EditorNode *p_editor) {
    Ref<CSGShapeSpatialGizmoPlugin> gizmo_plugin(make_ref_counted<CSGShapeSpatialGizmoPlugin>());
    Node3DEditor::get_singleton()->add_gizmo_plugin(gizmo_plugin);
}
