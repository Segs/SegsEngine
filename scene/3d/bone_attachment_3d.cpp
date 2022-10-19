/*************************************************************************/
/*  bone_attachment_3d.cpp                                               */
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

#include "bone_attachment_3d.h"
#include "core/method_bind.h"

IMPL_GDCLASS(BoneAttachment3D)
void BoneAttachment3D::_validate_property(PropertyInfo &property) const {

    if (property.name != "bone_name")
        return;

    Skeleton *parent = object_cast<Skeleton>(get_parent());

    if (parent) {

        String names;
        for (int i = 0; i < parent->get_bone_count(); i++) {
            if (i > 0)
                names += ',';
            names += parent->get_bone_name(i);
        }

        property.hint = PropertyHint::Enum;
        property.hint_string = names;
    } else {

        property.hint = PropertyHint::None;
        property.hint_string = "";
    }
}

void BoneAttachment3D::_check_bind() {

    Skeleton *sk = object_cast<Skeleton>(get_parent());
    if (sk) {

        int idx = sk->find_bone(bone_name);
        if (idx != -1) {
            sk->bind_child_node_to_bone(idx, this);
            set_transform(sk->get_bone_global_pose(idx));
            bound = true;
        }
    }
}

void BoneAttachment3D::_check_unbind() {

    if (bound) {

        Skeleton *sk = object_cast<Skeleton>(get_parent());
        if (sk) {

            int idx = sk->find_bone(bone_name);
            if (idx != -1) {
                sk->unbind_child_node_from_bone(idx, this);
            }
        }
        bound = false;
    }
}

void BoneAttachment3D::set_bone_name(const StringName &p_name) {

    if (is_inside_tree())
        _check_unbind();

    bone_name = p_name;

    if (is_inside_tree())
        _check_bind();
}



void BoneAttachment3D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {

            _check_bind();
        } break;
        case NOTIFICATION_EXIT_TREE: {

            _check_unbind();
        } break;
    }
}

BoneAttachment3D::BoneAttachment3D() {
    bound = false;
}

void BoneAttachment3D::_bind_methods() {
    SE_BIND_METHOD(BoneAttachment3D,set_bone_name);
    SE_BIND_METHOD(BoneAttachment3D,get_bone_name);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "bone_name"), "set_bone_name", "get_bone_name");
}
