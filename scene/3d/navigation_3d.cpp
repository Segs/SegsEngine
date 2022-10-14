/*************************************************************************/
/*  navigation_3d.cpp                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "navigation_3d.h"

#include "servers/navigation_server.h"
#include "core/method_bind.h"
#include "core/method_bind_interface.h"

IMPL_GDCLASS(Navigation3D)

Vector<Vector3> Navigation3D::get_simple_path(const Vector3 &p_start, const Vector3 &p_end, bool p_optimize) {

    return NavigationServer::get_singleton()->map_get_path(map, p_start, p_end, p_optimize);
}

void Navigation3D::set_up_vector(const Vector3 &p_up) {

    up = p_up;
    NavigationServer::get_singleton()->map_set_up(map, up);
}

Vector3 Navigation3D::get_up_vector() const {

    return up;
}

void Navigation3D::set_cell_size(float p_cell_size) {
    cell_size = p_cell_size;
    NavigationServer::get_singleton()->map_set_cell_size(map, cell_size);
}

void Navigation3D::set_edge_connection_margin(float p_edge_connection_margin) {
    edge_connection_margin = p_edge_connection_margin;
    NavigationServer::get_singleton()->map_set_edge_connection_margin(map, edge_connection_margin);
}

void Navigation3D::_bind_methods() {

    BIND_METHOD(Navigation3D,get_rid);

    MethodBinder::bind_method(D_METHOD("get_simple_path", {"start", "end", "optimize"}),&Navigation3D::get_simple_path, {DEFVAL(true)});

    BIND_METHOD(Navigation3D,set_up_vector);
    BIND_METHOD(Navigation3D,get_up_vector);

    BIND_METHOD(Navigation3D,set_cell_size);
    BIND_METHOD(Navigation3D,get_cell_size);

    BIND_METHOD(Navigation3D,set_edge_connection_margin);
    BIND_METHOD(Navigation3D,get_edge_connection_margin);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "up_vector"), "set_up_vector", "get_up_vector");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "cell_size"), "set_cell_size", "get_cell_size");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "edge_connection_margin"), "set_edge_connection_margin", "get_edge_connection_margin");
}

void Navigation3D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {
            NavigationServer::get_singleton()->map_set_active(map, true);
        } break;
        case NOTIFICATION_EXIT_TREE: {

            NavigationServer::get_singleton()->map_set_active(map, false);
        } break;
    }
}

Navigation3D::Navigation3D() {

    map = NavigationServer::get_singleton()->map_create();

    set_cell_size(0.3f);
    set_edge_connection_margin(5.0); // Five meters, depends alot on the agents radius

    up = Vector3(0, 1, 0);
}

Navigation3D::~Navigation3D() {
    NavigationServer::get_singleton()->free_rid(map);
}
