/*************************************************************************/
/*  multimesh_instance_2d.cpp                                            */
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

#include "multimesh_instance_2d.h"

#include "core/callable_method_pointer.h"
#include "core/core_string_names.h"
#include "core/method_bind.h"
#include "core/math/aabb.h"
#include "core/object_tooling.h"
#include "scene/resources/texture.h"


IMPL_GDCLASS(MultiMeshInstance2D)

#ifdef TOOLS_ENABLED

Rect2 MultiMeshInstance2D::_edit_get_rect() const {

    if (multimesh) {
        AABB aabb = multimesh->get_aabb();
        return Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y);
    }

    return Node2D::_edit_get_rect();
}
#endif

void MultiMeshInstance2D::_notification(int p_what) {

    if (p_what == NOTIFICATION_DRAW) {
        if (multimesh) {
            draw_multimesh(multimesh, texture, normal_map);
        }
    }
}

void MultiMeshInstance2D::_bind_methods() {

    SE_BIND_METHOD(MultiMeshInstance2D,set_multimesh);
    SE_BIND_METHOD(MultiMeshInstance2D,get_multimesh);

    SE_BIND_METHOD(MultiMeshInstance2D,set_texture);
    SE_BIND_METHOD(MultiMeshInstance2D,get_texture);

    SE_BIND_METHOD(MultiMeshInstance2D,set_normal_map);
    SE_BIND_METHOD(MultiMeshInstance2D,get_normal_map);

    ADD_SIGNAL(MethodInfo("texture_changed"));

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "multimesh", PropertyHint::ResourceType, "MultiMesh"), "set_multimesh", "get_multimesh");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "normal_map", PropertyHint::ResourceType, "Texture"), "set_normal_map", "get_normal_map");
}

void MultiMeshInstance2D::set_multimesh(const Ref<MultiMesh> &p_multimesh) {

    // Cleanup previous connection if any.
    if (multimesh) {
        multimesh->disconnect_all(CoreStringNames::get_singleton()->changed, this->get_instance_id());
    }
    multimesh = p_multimesh;
    // Connect to the multimesh so the AABB can update when instance transforms are changed.
    if (multimesh) {
        multimesh->connect(
                CoreStringNames::get_singleton()->changed, callable_mp((CanvasItem *)this, &CanvasItem::update));
    }
    update();
}

Ref<MultiMesh> MultiMeshInstance2D::get_multimesh() const {

    return multimesh;
}

void MultiMeshInstance2D::set_texture(const Ref<Texture> &p_texture) {

    if (p_texture == texture)
        return;
    texture = p_texture;
    update();
    emit_signal("texture_changed");
    Object_change_notify(this,"texture");
}

Ref<Texture> MultiMeshInstance2D::get_texture() const {

    return texture;
}

void MultiMeshInstance2D::set_normal_map(const Ref<Texture> &p_texture) {

    normal_map = p_texture;
    update();
}

Ref<Texture> MultiMeshInstance2D::get_normal_map() const {

    return normal_map;
}

MultiMeshInstance2D::MultiMeshInstance2D() = default;
MultiMeshInstance2D::~MultiMeshInstance2D() = default;
