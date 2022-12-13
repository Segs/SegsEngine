/*************************************************************************/
/*  test_render.cpp                                                      */
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

#include "test_render.h"

#include "core/math/geometry.h"
#include "core/math/math_funcs.h"
#include "core/math/quick_hull.h"
#include "core/input/input_event.h"
#include "core/math/transform.h"
#include "core/os/main_loop.h"
#include "core/string_utils.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "servers/rendering_server.h"


#define OBJECT_COUNT 50

namespace TestRender {

class TestMainLoop : public MainLoop {

    RenderingEntity test_cube=entt::null;
    RenderingEntity instance;
    RenderingEntity camera=entt::null;
    RenderingEntity viewport=entt::null;
    RenderingEntity light=entt::null;
    RenderingEntity scenario=entt::null;

    struct InstanceInfo {

        RenderingEntity instance=entt::null;
        Transform base;
        Vector3 rot_axis;
    };

    Vector<InstanceInfo> instances;

    float ofs;
    bool quit;
protected:
    RenderingEntity test_material;
    RenderingEntity _make_test_cube(RenderingServer *rs) {
        Vector<Vector3> vertices;
        Vector<Vector3> normals;
        Vector<float> tangents;
        Vector<Vector2> uvs;

    #define ADD_VTX(m_idx)                                                                                                 \
        vertices.push_back(face_points[m_idx]);                                                                            \
        normals.push_back(normal_points[m_idx]);                                                                           \
        tangents.push_back(normal_points[m_idx][1]);                                                                       \
        tangents.push_back(normal_points[m_idx][2]);                                                                       \
        tangents.push_back(normal_points[m_idx][0]);                                                                       \
        tangents.push_back(1.0);                                                                                           \
        uvs.push_back(Vector2(uv_points[m_idx * 2 + 0], uv_points[m_idx * 2 + 1]));

        for (int i = 0; i < 6; i++) {
            Vector3 face_points[4];
            Vector3 normal_points[4];
            float uv_points[8] = { 0, 0, 0, 1, 1, 1, 1, 0 };

            for (int j = 0; j < 4; j++) {
                float v[3];
                v[0] = 1.0;
                v[1] = 1 - 2 * ((j >> 1) & 1);
                v[2] = v[1] * (1 - 2 * (j & 1));

                for (int k = 0; k < 3; k++) {
                    if (i < 3) {
                        face_points[j][(i + k) % 3] = v[k];
                    } else {
                        face_points[3 - j][(i + k) % 3] = -v[k];
                    }
                }
                normal_points[j] = Vector3();
                normal_points[j][i % 3] = (i >= 3 ? -1 : 1);
            }

            // tri 1
            ADD_VTX(0)
            ADD_VTX(1)
            ADD_VTX(2)
            // tri 2
            ADD_VTX(2)
            ADD_VTX(3)
            ADD_VTX(0)
        }

        RenderingEntity test_cube = rs->mesh_create();

        Vector<int> indices(vertices.size());

        SurfaceArrays d(eastl::move(vertices));
        d.m_normals = eastl::move(normals);
        d.m_tangents = eastl::move(tangents);
        d.m_uv_1 = eastl::move(uvs);

        for (int i = 0; i < indices.size(); i++) {
            indices[i] = i;
        }
        d.m_indices = eastl::move(indices);

        rs->mesh_add_surface_from_arrays(test_cube, RS::PRIMITIVE_TRIANGLES, d);

        /*
        test_material = fixed_material_create();
        //material_set_flag(material, MATERIAL_FLAG_BILLBOARD_TOGGLE,true);
        fixed_material_set_texture( test_material, FIXED_MATERIAL_PARAM_DIFFUSE, get_test_texture() );
        fixed_material_set_param( test_material, FIXED_MATERIAL_PARAM_SPECULAR_EXP, 70 );
        fixed_material_set_param( test_material, FIXED_MATERIAL_PARAM_EMISSION, Color(0.2,0.2,0.2) );

        fixed_material_set_param( test_material, FIXED_MATERIAL_PARAM_DIFFUSE, Color(1, 1, 1) );
        fixed_material_set_param( test_material, FIXED_MATERIAL_PARAM_SPECULAR, Color(1,1,1) );
    */
        rs->mesh_surface_set_material(test_cube, 0, test_material);

        return test_cube;
    }

    RenderingEntity get_test_cube(RenderingServer *rs) {
        if (test_cube==entt::null) {
            test_cube = _make_test_cube(rs);
        }
        return test_cube;
    }
public:
    void input_event(const Ref<InputEvent> &p_event) override {

        if (p_event->is_pressed())
            quit = true;
    }

    void init() override {

        print_line("INITIALIZING TEST RENDER");
        RenderingServer *vs = RenderingServer::get_singleton();
        test_cube = get_test_cube(vs);
        scenario = vs->scenario_create();

        FixedVector<Vector3,8> vts;

        /*
        PoolVector<Plane> sp = Geometry::build_sphere_planes(2,5,5);
        Geometry::MeshData md2 = Geometry::build_convex_mesh(sp);
        vts=md2.vertices;
*/
        /*

        static const int s = 20;
        for(int i=0;i<s;i++) {
            Basis rot(Vector3(0,1,0),i*Math_PI/s);

            for(int j=0;j<s;j++) {
                Vector3 v;
                v.x=Math::sin(j*Math_PI*2/s);
                v.y=Math::cos(j*Math_PI*2/s);

                vts.push_back( rot.xform(v*2 ) );
            }
        }*/
        /*for(int i=0;i<100;i++) {

            vts.push_back( Vector3(Math::randf()*2-1.0,Math::randf()*2-1.0,Math::randf()*2-1.0).normalized()*2);
        }*/
        /*
        vts.push_back(Vector3(0,0,1));
        vts.push_back(Vector3(0,0,-1));
        vts.push_back(Vector3(0,1,0));
        vts.push_back(Vector3(0,-1,0));
        vts.push_back(Vector3(1,0,0));
        vts.push_back(Vector3(-1,0,0));*/

        vts.push_back(Vector3(1, 1, 1));
        vts.push_back(Vector3(1, -1, 1));
        vts.push_back(Vector3(-1, 1, 1));
        vts.push_back(Vector3(-1, -1, 1));
        vts.push_back(Vector3(1, 1, -1));
        vts.push_back(Vector3(1, -1, -1));
        vts.push_back(Vector3(-1, 1, -1));
        vts.push_back(Vector3(-1, -1, -1));

        GeometryMeshData md;
        Error err = QuickHull::build(vts, md);
        print_line("ERR: " + itos(err));
        test_cube = vs->mesh_create();
        vs->mesh_add_surface_from_mesh_data(test_cube, eastl::move(md));
        //vs->scenario_set_debug(scenario,RS::SCENARIO_DEBUG_WIREFRAME);

        /*
        RenderingEntity sm = vs->shader_create();
        //vs->shader_set_fragment_code(sm,"OUT_ALPHA=mod(TIME,1);");
        //vs->shader_set_vertex_code(sm,"OUT_VERTEX=IN_VERTEX*mod(TIME,1);");
        vs->shader_set_fragment_code(sm,"OUT_DIFFUSE=vec3(1,0,1);OUT_GLOW=abs(sin(TIME));");
        RenderingEntity tcmat = vs->mesh_surface_get_material(test_cube,0);
        vs->material_set_shader(tcmat,sm);
        */

        const Vector<String> &cmdline(OS::get_singleton()->get_cmdline_args());
        int object_count = OBJECT_COUNT;
        if (!cmdline.empty() && StringUtils::to_int(cmdline.back())) {
            object_count = StringUtils::to_int(cmdline.back());
        }

        for (int i = 0; i < object_count; i++) {

            InstanceInfo ii;

            ii.instance = vs->instance_create2(test_cube, scenario);

            ii.base.translate(Math::random(-20, 20), Math::random(-20, 20), Math::random(-20, 18));
            ii.base.rotate(Vector3(0, 1, 0), Math::randf() * Math_PI);
            ii.base.rotate(Vector3(1, 0, 0), Math::randf() * Math_PI);
            vs->instance_set_transform(ii.instance, ii.base);

            ii.rot_axis = Vector3(Math::random(-1, 1), Math::random(-1, 1), Math::random(-1, 1)).normalized();

            instances.push_back(ii);
        }

        camera = vs->camera_create();

        //vs->camera_set_perspective( camera, 60.0,0.1, 100.0 );

        viewport = vs->viewport_create();
        Size2i screen_size = OS::get_singleton()->get_window_size();
        vs->viewport_set_size(viewport, screen_size.x, screen_size.y);
        vs->viewport_attach_to_screen(viewport, Rect2(Vector2(), screen_size));
        vs->viewport_set_active(viewport, true);
        vs->viewport_attach_camera(viewport, camera);
        vs->viewport_set_scenario(viewport, scenario);
        vs->camera_set_transform(camera, Transform(Basis(), Vector3(0, 3, 30)));
        vs->camera_set_perspective(camera, 60, 0.1f, 1000);

        RenderingEntity lightaux = vs->directional_light_create();

        //vs->light_set_color( lightaux, RenderingServer::LIGHT_COLOR_AMBIENT, Color(0.0,0.0,0.0) );
        vs->light_set_color(lightaux, Color(1.0, 1.0, 1.0));
        //vs->light_set_shadow( lightaux, true );
        light = vs->instance_create2(lightaux, scenario);
        Transform lla;
        //lla.set_look_at(Vector3(),Vector3(1,-1,1),Vector3(0,1,0));
        lla.set_look_at(Vector3(), Vector3(-0.000000, -0.836026f, -0.548690f), Vector3(0, 1, 0));

        vs->instance_set_transform(light, lla);

        lightaux = vs->omni_light_create();
        //vs->light_set_color( lightaux, RenderingServer::LIGHT_COLOR_AMBIENT, Color(0.0,0.0,1.0) );
        vs->light_set_color(lightaux, Color(1.0, 1.0, 0.0));
        vs->light_set_param(lightaux, RS::LIGHT_PARAM_RANGE, 4);
        vs->light_set_param(lightaux, RS::LIGHT_PARAM_ENERGY, 8);
        //vs->light_set_shadow( lightaux, true );
        //light = vs->instance_create( lightaux );

        ofs = 0;
        quit = false;
    }
    bool iteration(float p_time) override {

        RenderingServer *vs = RenderingServer::get_singleton();
        //Transform t;
        //t.rotate(Vector3(0, 1, 0), ofs);
        //t.translate(Vector3(0,0,20 ));
        //vs->camera_set_transform(camera, t);

        ofs += p_time * 0.05f;

        //return quit;

        for (const InstanceInfo & E : instances) {

            Transform pre(Basis(E.rot_axis, ofs), Vector3());
            vs->instance_set_transform(E.instance, pre * E.base);
            /*
            if( !E->next() ) {

                vs->free( E->get().instance );
                instances.erase(E );
            }*/
        }

        return quit;
    }

    bool idle(float p_time) override {
        return quit;
    }

    void finish() override {
    }
};

MainLoop *test() {

    return memnew(TestMainLoop);
}
} // namespace TestRender
