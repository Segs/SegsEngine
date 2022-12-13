/*************************************************************************/
/*  test_physics.cpp                                                     */
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

#include "test_physics.h"


#include "core/callable_method_pointer.h"
#include "core/map.h"
#include "core/method_bind.h"
#include "core/math/geometry.h"
#include "core/math/math_funcs.h"
#include "core/math/quick_hull.h"
#include "core/input/input_event.h"
#include "core/os/main_loop.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "servers/physics_server_3d.h"
#include "servers/rendering_server.h"


class TestPhysicsMainLoop : public MainLoop {

    GDCLASS(TestPhysicsMainLoop,MainLoop)


    enum {
        LINK_COUNT = 20,
    };

    RID test_cube;

    RID plane;
    RID sphere;
    RenderingEntity light;
    RenderingEntity camera;
    RID mover;
    RenderingEntity scenario;
    RID space;

    RID character;

    float ofs_x, ofs_y;

    Point2 joy_direction;

    Vector<RID> bodies;
    Map<PhysicsServer3D::ShapeType, RID> type_shape_map;
    Map<PhysicsServer3D::ShapeType, RenderingEntity> type_mesh_map;

    void body_changed_transform(Object *p_state, RenderingEntity p_visual_instance) {

        PhysicsDirectBodyState3D *state = (PhysicsDirectBodyState3D *)p_state;
        RenderingServer *vs = RenderingServer::get_singleton();
        Transform t = state->get_transform();
        vs->instance_set_transform(p_visual_instance, t);
    }

    bool quit;

protected:
    static void _bind_methods() {

        MethodBinder::bind_method("body_changed_transform", &TestPhysicsMainLoop::body_changed_transform);
    }

    RID create_body(PhysicsServer3D::ShapeType p_shape, PhysicsServer3D::BodyMode p_body, const Transform p_location, bool p_active_default = true, const Transform &p_shape_xform = Transform()) {

        RenderingServer *vs = RenderingServer::get_singleton();
        PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

        RenderingEntity mesh_instance = vs->instance_create2(type_mesh_map[p_shape], scenario);
        RID body = ps->body_create(p_body, !p_active_default);
        ps->body_set_space(body, space);
        ps->body_set_param(body, PhysicsServer3D::BODY_PARAM_BOUNCE, 0.0);
        //todo set space
        ps->body_add_shape(body, type_shape_map[p_shape]);
        ps->body_set_force_integration_callback(body, callable_gen(this, ([this, mesh_instance](Object* ob)->void { body_changed_transform(ob, mesh_instance); })));

        ps->body_set_state(body, PhysicsServer3D::BODY_STATE_TRANSFORM, p_location);
        bodies.push_back(body);

        if (p_body == PhysicsServer3D::BODY_MODE_STATIC) {

            vs->instance_set_transform(mesh_instance, p_location);
        }
        return body;
    }

    RID create_static_plane(const Plane &p_plane) {

        PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

        RID plane_shape = ps->shape_create(PhysicsServer3D::SHAPE_PLANE);
        ps->shape_set_data(plane_shape, p_plane);

        RID b = ps->body_create(PhysicsServer3D::BODY_MODE_STATIC);
        ps->body_set_space(b, space);
        //todo set space
        ps->body_add_shape(b, plane_shape);
        return b;
    }

    void configure_body(RID p_body, float p_mass, float p_friction, float p_bounce) {

        PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
        ps->body_set_param(p_body, PhysicsServer3D::BODY_PARAM_MASS, p_mass);
        ps->body_set_param(p_body, PhysicsServer3D::BODY_PARAM_FRICTION, p_friction);
        ps->body_set_param(p_body, PhysicsServer3D::BODY_PARAM_BOUNCE, p_bounce);
    }

    void init_shapes() {

        RenderingServer *vs = RenderingServer::get_singleton();
        PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

        /* SPHERE SHAPE */
        RenderingEntity sphere_mesh = vs->make_sphere_mesh(10, 20, 0.5);
        type_mesh_map[PhysicsServer3D::SHAPE_SPHERE] = sphere_mesh;

        RID sphere_shape = ps->shape_create(PhysicsServer3D::SHAPE_SPHERE);
        ps->shape_set_data(sphere_shape, 0.5);
        type_shape_map[PhysicsServer3D::SHAPE_SPHERE] = sphere_shape;

        /* BOX SHAPE */

        PoolVector<Plane> box_planes = Geometry::build_box_planes(Vector3(0.5, 0.5, 0.5));
        RenderingEntity box_mesh = vs->mesh_create();
        GeometryMeshData box_data = Geometry::build_convex_mesh(box_planes.toSpan());
        vs->mesh_add_surface_from_mesh_data(box_mesh, eastl::move(box_data));
        type_mesh_map[PhysicsServer3D::SHAPE_BOX] = box_mesh;

        RID box_shape = ps->shape_create(PhysicsServer3D::SHAPE_BOX);
        ps->shape_set_data(box_shape, Vector3(0.5, 0.5, 0.5));
        type_shape_map[PhysicsServer3D::SHAPE_BOX] = box_shape;

        /* CAPSULE SHAPE */

        PoolVector<Plane> capsule_planes = Geometry::build_capsule_planes(0.5, 0.7f, 12, Vector3::AXIS_Z);

        RenderingEntity capsule_mesh = vs->mesh_create();
        GeometryMeshData capsule_data = Geometry::build_convex_mesh(capsule_planes.toSpan());
        vs->mesh_add_surface_from_mesh_data(capsule_mesh, eastl::move(capsule_data));

        type_mesh_map[PhysicsServer3D::SHAPE_CAPSULE] = capsule_mesh;

        RID capsule_shape = ps->shape_create(PhysicsServer3D::SHAPE_CAPSULE);
        Dictionary capsule_params;
        capsule_params["radius"] = 0.5;
        capsule_params["height"] = 1.4;
        ps->shape_set_data(capsule_shape, capsule_params);
        type_shape_map[PhysicsServer3D::SHAPE_CAPSULE] = capsule_shape;

        /* CONVEX SHAPE */

        PoolVector<Plane> convex_planes = Geometry::build_cylinder_planes(0.5, 0.7f, 5, Vector3::AXIS_Z);

        RenderingEntity convex_mesh = vs->mesh_create();
        GeometryMeshData convex_data = Geometry::build_convex_mesh(convex_planes.toSpan());
        QuickHull::build(convex_data.vertices, convex_data);
        vs->mesh_add_surface_from_mesh_data(convex_mesh, eastl::move(convex_data));

        type_mesh_map[PhysicsServer3D::SHAPE_CONVEX_POLYGON] = convex_mesh;

        RID convex_shape = ps->shape_create(PhysicsServer3D::SHAPE_CONVEX_POLYGON);
        ps->shape_set_data(convex_shape, Variant::from(convex_data.vertices));
        type_shape_map[PhysicsServer3D::SHAPE_CONVEX_POLYGON] = convex_shape;
    }

    void make_trimesh(Vector<Vector3> p_faces, const Transform &p_xform = Transform()) {

        RenderingServer *vs = RenderingServer::get_singleton();
        PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
        RID trimesh_shape = ps->shape_create(PhysicsServer3D::SHAPE_CONCAVE_POLYGON);
        ps->shape_set_data(trimesh_shape, Variant::from(p_faces));
        p_faces = ps->shape_get_data(trimesh_shape).as<Vector<Vector3>>(); // optimized one
        Vector<Vector3> normals; // for drawing
        for (int i = 0; i < p_faces.size() / 3; i++) {

            Plane p(p_faces[i * 3 + 0], p_faces[i * 3 + 1], p_faces[i * 3 + 2]);
            normals.push_back(p.normal);
            normals.push_back(p.normal);
            normals.push_back(p.normal);
        }

        RenderingEntity trimesh_mesh = vs->mesh_create();
        SurfaceArrays d;
        d.set_positions(eastl::move(p_faces));
        d.m_normals = eastl::move(normals);
        vs->mesh_add_surface_from_arrays(trimesh_mesh, RS::PRIMITIVE_TRIANGLES, d);

        RenderingEntity triins = vs->instance_create2(trimesh_mesh, scenario);

        RID tribody = ps->body_create(PhysicsServer3D::BODY_MODE_STATIC);
        ps->body_set_space(tribody, space);
        //todo set space
        ps->body_add_shape(tribody, trimesh_shape);
        Transform tritrans = p_xform;
        ps->body_set_state(tribody, PhysicsServer3D::BODY_STATE_TRANSFORM, tritrans);
        vs->instance_set_transform(triins, tritrans);
    }

    void make_grid(int p_width, int p_height, float p_cellsize, float p_cellheight, const Transform &p_xform = Transform()) {

        Vector<Vector<float> > grid;

        grid.resize(p_width);

        for (int i = 0; i < p_width; i++) {

            grid[i].resize(p_height);

            for (int j = 0; j < p_height; j++) {

                grid[i][j] = 1.0f + Math::random(-p_cellheight, p_cellheight);
            }
        }

        Vector<Vector3> faces;

        for (int i = 1; i < p_width; i++) {

            for (int j = 1; j < p_height; j++) {

#define MAKE_VERTEX(m_x, m_z) \
    faces.push_back(Vector3((m_x - p_width / 2) * p_cellsize, grid[m_x][m_z], (m_z - p_height / 2) * p_cellsize))

                MAKE_VERTEX(i, j - 1);
                MAKE_VERTEX(i, j);
                MAKE_VERTEX(i - 1, j);

                MAKE_VERTEX(i - 1, j - 1);
                MAKE_VERTEX(i, j - 1);
                MAKE_VERTEX(i - 1, j);
            }
        }

        make_trimesh(faces, p_xform);
    }

public:
    void input_event(const Ref<InputEvent> &p_event) override {

        Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);
        if (mm && mm->get_button_mask() & 4) {

            ofs_y -= mm->get_relative().y / 200.0f;
            ofs_x += mm->get_relative().x / 200.0f;
        }

        if (mm && mm->get_button_mask() & 1) {

            float y = -mm->get_relative().y / 20.0f;
            float x = mm->get_relative().x / 20.0f;

            if (mover.is_valid()) {

                PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
                Transform t = ps->body_get_state(mover, PhysicsServer3D::BODY_STATE_TRANSFORM).as<Transform>();
                t.origin += Vector3(x, y, 0);

                ps->body_set_state(mover, PhysicsServer3D::BODY_STATE_TRANSFORM, t);
            }
        }
    }

    virtual void request_quit() {

        quit = true;
    }
    void init() override {

        ofs_x = ofs_y = 0;
        init_shapes();

        PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
        space = ps->space_create();
        ps->space_set_active(space, true);

        RenderingServer *vs = RenderingServer::get_singleton();

        /* LIGHT */
        RenderingEntity lightaux = vs->directional_light_create();
        scenario = vs->scenario_create();
        vs->light_set_shadow(lightaux, true);
        light = vs->instance_create2(lightaux, scenario);
        Transform t;
        t.rotate(Vector3(1.0, 0, 0), 0.6f);
        vs->instance_set_transform(light, t);

        /* CAMERA */

        camera = vs->camera_create();

        RenderingEntity viewport = vs->viewport_create();
        Size2i screen_size = OS::get_singleton()->get_window_size();
        vs->viewport_set_size(viewport, screen_size.x, screen_size.y);
        vs->viewport_attach_to_screen(viewport, Rect2(Vector2(), screen_size));
        vs->viewport_set_active(viewport, true);
        vs->viewport_attach_camera(viewport, camera);
        vs->viewport_set_scenario(viewport, scenario);

        vs->camera_set_perspective(camera, 60, 0.1f, 40.0);
        vs->camera_set_transform(camera, Transform(Basis(), Vector3(0, 9, 12)));

        Transform gxf;
        gxf.basis.scale(Vector3(1.4f, 0.4f, 1.4f));
        gxf.origin = Vector3(-2, 1, -2);
        make_grid(5, 5, 2.5, 1, gxf);
        test_fall();
        quit = false;
    }
    bool iteration(float p_time) override {

        if (mover.is_valid()) {
            static float joy_speed = 10;
            PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
            Transform t = ps->body_get_state(mover, PhysicsServer3D::BODY_STATE_TRANSFORM).as<Transform>();
            t.origin += Vector3(joy_speed * joy_direction.x * p_time, -joy_speed * joy_direction.y * p_time, 0);
            ps->body_set_state(mover, PhysicsServer3D::BODY_STATE_TRANSFORM, t);
        }

        Transform cameratr;
        cameratr.rotate(Vector3(0, 1, 0), ofs_x);
        cameratr.rotate(Vector3(1, 0, 0), -ofs_y);
        cameratr.translate(Vector3(0, 2, 8));
        RenderingServer *vs = RenderingServer::get_singleton();
        vs->camera_set_transform(camera, cameratr);

        return quit;
    }
    void finish() override {
    }

    void test_joint() {
    }

    void test_hinge() {
    }

    void test_character() {

        RenderingServer *vs = RenderingServer::get_singleton();
        PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

        PoolVector<Plane> capsule_planes = Geometry::build_capsule_planes(0.5, 1, 12, 5, Vector3::AXIS_Y);

        RenderingEntity capsule_mesh = vs->mesh_create();
        GeometryMeshData capsule_data = Geometry::build_convex_mesh(capsule_planes.toSpan());
        vs->mesh_add_surface_from_mesh_data(capsule_mesh, eastl::move(capsule_data));
        type_mesh_map[PhysicsServer3D::SHAPE_CAPSULE] = capsule_mesh;

        RID capsule_shape = ps->shape_create(PhysicsServer3D::SHAPE_CAPSULE);
        Dictionary capsule_params;
        capsule_params["radius"] = 0.5;
        capsule_params["height"] = 1;
        Transform shape_xform;
        shape_xform.rotate(Vector3(1, 0, 0), Math_PI / 2.0);
        //shape_xform.origin=Vector3(1,1,1);
        ps->shape_set_data(capsule_shape, capsule_params);

        RenderingEntity mesh_instance = vs->instance_create2(capsule_mesh, scenario);
        character = ps->body_create(PhysicsServer3D::BODY_MODE_CHARACTER);
        ps->body_set_space(character, space);
        //todo add space
        ps->body_add_shape(character, capsule_shape);

        eastl::function<void(Object* ob)> cb = [this, mesh_instance](Object* ob) { body_changed_transform(ob, mesh_instance); };
        ps->body_set_force_integration_callback(character, callable_gen(this, cb));

        ps->body_set_state(character, PhysicsServer3D::BODY_STATE_TRANSFORM, Transform(Basis(), Vector3(-2, 5, -2)));
        bodies.push_back(character);
    }

    void test_fall() {

        for (int i = 0; i < 35; i++) {

            static const PhysicsServer3D::ShapeType shape_idx[] = {
                PhysicsServer3D::SHAPE_CAPSULE,
                PhysicsServer3D::SHAPE_BOX,
                PhysicsServer3D::SHAPE_SPHERE,
                PhysicsServer3D::SHAPE_CONVEX_POLYGON
            };

            PhysicsServer3D::ShapeType type = shape_idx[i % 4];

            Transform t;

            t.origin = Vector3(0.0f * i, 3.5f + 1.1f * i, 0.7f + 0.0f * i);
            t.basis.rotate(Vector3(0.2f, -1, 0), Math_PI / 2 * 0.6f);

            create_body(type, PhysicsServer3D::BODY_MODE_RIGID, t);
        }

        create_static_plane(Plane(Vector3(0, 1, 0), -1));
    }

    void test_activate() {

        create_body(PhysicsServer3D::SHAPE_BOX, PhysicsServer3D::BODY_MODE_RIGID, Transform(Basis(), Vector3(0, 2, 0)), true);
        create_static_plane(Plane(Vector3(0, 1, 0), -1));
    }

    bool idle(float p_time) override {
        return false;
    }

    TestPhysicsMainLoop() {
    }
};

IMPL_GDCLASS(TestPhysicsMainLoop)

namespace TestPhysics {

MainLoop *test() {

    return memnew(TestPhysicsMainLoop);
}
} // namespace TestPhysics
