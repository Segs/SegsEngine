/*************************************************************************/
/*  mesh_instance_2d.cpp                                                 */
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

#include "mesh_instance_2d.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"

IMPL_GDCLASS(MeshInstance2D)

#ifdef TOOLS_ENABLED
Rect2 MeshInstance2D::_edit_get_rect() const {

    if (mesh) {
        AABB aabb = mesh->get_aabb();
        return Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y);
    }

    return Node2D::_edit_get_rect();
}
bool MeshInstance2D::_edit_use_rect() const {
    return mesh!=nullptr;
}
#endif

void MeshInstance2D::_notification(int p_what) {

    if (p_what == NOTIFICATION_DRAW) {
        if (mesh) {
            draw_mesh(mesh, texture, normal_map);
        }
    }
}

void MeshInstance2D::_bind_methods() {

    SE_BIND_METHOD(MeshInstance2D,set_mesh);
    SE_BIND_METHOD(MeshInstance2D,get_mesh);

    SE_BIND_METHOD(MeshInstance2D,set_texture);
    SE_BIND_METHOD(MeshInstance2D,get_texture);

    SE_BIND_METHOD(MeshInstance2D,set_normal_map);
    SE_BIND_METHOD(MeshInstance2D,get_normal_map);

    ADD_SIGNAL(MethodInfo("texture_changed"));

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "mesh", PropertyHint::ResourceType, "Mesh"), "set_mesh", "get_mesh");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "normal_map", PropertyHint::ResourceType, "Texture"), "set_normal_map", "get_normal_map");
}

void MeshInstance2D::set_mesh(const Ref<Mesh> &p_mesh) {

    mesh = p_mesh;
    update();
}

Ref<Mesh> MeshInstance2D::get_mesh() const {

    return mesh;
}

void MeshInstance2D::set_texture(const Ref<Texture> &p_texture) {

    if (p_texture == texture)
        return;
    texture = p_texture;
    update();
    emit_signal("texture_changed");
    Object_change_notify(this,"texture");
}

void MeshInstance2D::set_normal_map(const Ref<Texture> &p_texture) {

    normal_map = p_texture;
    update();
}

Ref<Texture> MeshInstance2D::get_normal_map() const {

    return normal_map;
}

Ref<Texture> MeshInstance2D::get_texture() const {

    return texture;
}


MeshInstance2D::MeshInstance2D() = default;
