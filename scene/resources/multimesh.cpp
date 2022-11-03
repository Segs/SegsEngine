/*************************************************************************/
/*  multimesh.cpp                                                        */
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

#include "multimesh.h"
#include "scene/resources/mesh.h"
#include "servers/rendering_server.h"
#include "core/method_bind.h"

IMPL_GDCLASS(MultiMesh)
RES_BASE_EXTENSION_IMPL(MultiMesh,"multimesh")
VARIANT_ENUM_CAST(MultiMesh::TransformFormat);
VARIANT_ENUM_CAST(MultiMesh::ColorFormat);
VARIANT_ENUM_CAST(MultiMesh::CustomDataFormat);

void MultiMesh::_set_transform_array(const PoolVector<Vector3> &p_array) {
    if (transform_format != TRANSFORM_3D)
        return;

    const PoolVector<Vector3> &xforms = p_array;
    int len = xforms.size();
    ERR_FAIL_COND((len / 4) != instance_count);
    if (len == 0)
        return;

    PoolVector<Vector3>::Read r = xforms.read();

    for (int i = 0; i < len / 4; i++) {

        Transform t;
        t.basis[0] = r[i * 4 + 0];
        t.basis[1] = r[i * 4 + 1];
        t.basis[2] = r[i * 4 + 2];
        t.origin = r[i * 4 + 3];

        set_instance_transform(i, t);
    }
}

PoolVector<Vector3> MultiMesh::_get_transform_array() const {

    if (transform_format != TRANSFORM_3D)
        return PoolVector<Vector3>();

    if (instance_count == 0)
        return PoolVector<Vector3>();

    PoolVector<Vector3> xforms;
    xforms.resize(instance_count * 4);

    PoolVector<Vector3>::Write w = xforms.write();

    for (int i = 0; i < instance_count; i++) {

        Transform t = get_instance_transform(i);
        w[i * 4 + 0] = t.basis[0];
        w[i * 4 + 1] = t.basis[1];
        w[i * 4 + 2] = t.basis[2];
        w[i * 4 + 3] = t.origin;
    }

    return xforms;
}

void MultiMesh::_set_transform_2d_array(const PoolVector<Vector2> &p_array) {

    if (transform_format != TRANSFORM_2D)
        return;

    const PoolVector<Vector2> &xforms = p_array;
    int len = xforms.size();
    ERR_FAIL_COND((len / 3) != instance_count);
    if (len == 0)
        return;

    PoolVector<Vector2>::Read r = xforms.read();

    for (int i = 0; i < len / 3; i++) {

        Transform2D t;
        t.elements[0] = r[i * 3 + 0];
        t.elements[1] = r[i * 3 + 1];
        t.elements[2] = r[i * 3 + 2];

        set_instance_transform_2d(i, t);
    }
}

PoolVector<Vector2> MultiMesh::_get_transform_2d_array() const {

    if (transform_format != TRANSFORM_2D)
        return PoolVector<Vector2>();

    if (instance_count == 0)
        return PoolVector<Vector2>();

    PoolVector<Vector2> xforms;
    xforms.resize(instance_count * 3);

    PoolVector<Vector2>::Write w = xforms.write();

    for (int i = 0; i < instance_count; i++) {

        Transform2D t = get_instance_transform_2d(i);
        w[i * 3 + 0] = t.elements[0];
        w[i * 3 + 1] = t.elements[1];
        w[i * 3 + 2] = t.elements[2];
    }

    return xforms;
}

void MultiMesh::_set_color_array(const PoolVector<Color> &p_array) {

    const PoolVector<Color> &colors = p_array;
    int len = colors.size();
    if (len == 0)
        return;
    ERR_FAIL_COND(len != instance_count);

    PoolVector<Color>::Read r = colors.read();

    for (int i = 0; i < len; i++) {

        set_instance_color(i, r[i]);
    }
}

PoolVector<Color> MultiMesh::_get_color_array() const {

    if (instance_count == 0 || color_format == COLOR_NONE)
        return PoolVector<Color>();

    PoolVector<Color> colors;
    colors.resize(instance_count);

    for (int i = 0; i < instance_count; i++) {

        colors.set(i, get_instance_color(i));
    }

    return colors;
}

void MultiMesh::_set_custom_data_array(const PoolVector<Color> &p_array) {

    const PoolVector<Color> &custom_datas = p_array;
    int len = custom_datas.size();
    if (len == 0)
        return;
    ERR_FAIL_COND(len != instance_count);

    PoolVector<Color>::Read r = custom_datas.read();

    for (int i = 0; i < len; i++) {

        set_instance_custom_data(i, r[i]);
    }
}

PoolVector<Color> MultiMesh::_get_custom_data_array() const {

    if (instance_count == 0 || custom_data_format == CUSTOM_DATA_NONE)
        return PoolVector<Color>();

    PoolVector<Color> custom_datas;
    custom_datas.resize(instance_count);

    for (int i = 0; i < instance_count; i++) {

        custom_datas.set(i, get_instance_custom_data(i));
    }

    return custom_datas;
}
void MultiMesh::set_mesh(const Ref<Mesh> &p_mesh) {

    mesh = p_mesh;
    if (mesh)
        RenderingServer::get_singleton()->multimesh_set_mesh(multimesh, mesh->get_rid());
    else
        RenderingServer::get_singleton()->multimesh_set_mesh(multimesh, entt::null);
}

Ref<Mesh> MultiMesh::get_mesh() const {

    return mesh;
}

void MultiMesh::set_instance_count(int p_count) {
    ERR_FAIL_COND(p_count < 0);
    RenderingServer::get_singleton()->multimesh_allocate(multimesh, p_count, RS::MultimeshTransformFormat(transform_format), RS::MultimeshColorFormat(color_format), RS::MultimeshCustomDataFormat(custom_data_format));
    instance_count = p_count;
}
int MultiMesh::get_instance_count() const {

    return instance_count;
}

void MultiMesh::set_visible_instance_count(int p_count) {
    ERR_FAIL_COND(p_count < -1);
    RenderingServer::get_singleton()->multimesh_set_visible_instances(multimesh, p_count);
    visible_instance_count = p_count;
}
int MultiMesh::get_visible_instance_count() const {

    return visible_instance_count;
}

void MultiMesh::set_instance_transform(int p_instance, const Transform &p_transform) {

    RenderingServer::get_singleton()->multimesh_instance_set_transform(multimesh, p_instance, p_transform);
}

void MultiMesh::set_instance_transform_2d(int p_instance, const Transform2D &p_transform) {

    RenderingServer::get_singleton()->multimesh_instance_set_transform_2d(multimesh, p_instance, p_transform);
    emit_changed();
}

Transform MultiMesh::get_instance_transform(int p_instance) const {

    return RenderingServer::get_singleton()->multimesh_instance_get_transform(multimesh, p_instance);
}

Transform2D MultiMesh::get_instance_transform_2d(int p_instance) const {

    return RenderingServer::get_singleton()->multimesh_instance_get_transform_2d(multimesh, p_instance);
}

void MultiMesh::set_instance_color(int p_instance, const Color &p_color) {

    RenderingServer::get_singleton()->multimesh_instance_set_color(multimesh, p_instance, p_color);
}
Color MultiMesh::get_instance_color(int p_instance) const {

    return RenderingServer::get_singleton()->multimesh_instance_get_color(multimesh, p_instance);
}

void MultiMesh::set_instance_custom_data(int p_instance, const Color &p_custom_data) {

    RenderingServer::get_singleton()->multimesh_instance_set_custom_data(multimesh, p_instance, p_custom_data);
}
Color MultiMesh::get_instance_custom_data(int p_instance) const {

    return RenderingServer::get_singleton()->multimesh_instance_get_custom_data(multimesh, p_instance);
}

void MultiMesh::set_as_bulk_array(Span<const float> p_array) {

    RenderingServer::get_singleton()->multimesh_set_as_bulk_array(multimesh, p_array);
}

AABB MultiMesh::get_aabb() const {

    return RenderingServer::get_singleton()->multimesh_get_aabb(multimesh);
}

RenderingEntity MultiMesh::get_rid() const {

    return multimesh;
}

void MultiMesh::set_color_format(ColorFormat p_color_format) {

    ERR_FAIL_COND(instance_count > 0);
    color_format = p_color_format;
}

MultiMesh::ColorFormat MultiMesh::get_color_format() const {

    return color_format;
}

void MultiMesh::set_custom_data_format(CustomDataFormat p_custom_data_format) {

    ERR_FAIL_COND(instance_count > 0);
    custom_data_format = p_custom_data_format;
}

MultiMesh::CustomDataFormat MultiMesh::get_custom_data_format() const {

    return custom_data_format;
}

void MultiMesh::set_transform_format(TransformFormat p_transform_format) {

    ERR_FAIL_COND(instance_count > 0);
    transform_format = p_transform_format;
}
MultiMesh::TransformFormat MultiMesh::get_transform_format() const {

    return transform_format;
}

void MultiMesh::_bind_methods() {

    SE_BIND_METHOD(MultiMesh,set_mesh);
    SE_BIND_METHOD(MultiMesh,get_mesh);
    SE_BIND_METHOD(MultiMesh,set_color_format);
    SE_BIND_METHOD(MultiMesh,get_color_format);
    SE_BIND_METHOD(MultiMesh,set_custom_data_format);
    SE_BIND_METHOD(MultiMesh,get_custom_data_format);
    SE_BIND_METHOD(MultiMesh,set_transform_format);
    SE_BIND_METHOD(MultiMesh,get_transform_format);

    SE_BIND_METHOD(MultiMesh,set_instance_count);
    SE_BIND_METHOD(MultiMesh,get_instance_count);
    SE_BIND_METHOD(MultiMesh,set_visible_instance_count);
    SE_BIND_METHOD(MultiMesh,get_visible_instance_count);
    SE_BIND_METHOD(MultiMesh,set_instance_transform);
    SE_BIND_METHOD(MultiMesh,set_instance_transform_2d);
    SE_BIND_METHOD(MultiMesh,get_instance_transform);
    SE_BIND_METHOD(MultiMesh,get_instance_transform_2d);
    SE_BIND_METHOD(MultiMesh,set_instance_color);
    SE_BIND_METHOD(MultiMesh,get_instance_color);
    SE_BIND_METHOD(MultiMesh,set_instance_custom_data);
    SE_BIND_METHOD(MultiMesh,get_instance_custom_data);
    SE_BIND_METHOD(MultiMesh,set_as_bulk_array);
    SE_BIND_METHOD(MultiMesh,get_aabb);

    SE_BIND_METHOD(MultiMesh,_set_transform_array);
    SE_BIND_METHOD(MultiMesh,_get_transform_array);
    SE_BIND_METHOD(MultiMesh,_set_transform_2d_array);
    SE_BIND_METHOD(MultiMesh,_get_transform_2d_array);
    SE_BIND_METHOD(MultiMesh,_set_color_array);
    SE_BIND_METHOD(MultiMesh,_get_color_array);
    SE_BIND_METHOD(MultiMesh,_set_custom_data_array);
    SE_BIND_METHOD(MultiMesh,_get_custom_data_array);

    // Properties and constants
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "color_format", PropertyHint::Enum, "None,Byte,Float"), "set_color_format", "get_color_format");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "transform_format", PropertyHint::Enum, "2D,3D"), "set_transform_format", "get_transform_format");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "custom_data_format", PropertyHint::Enum, "None,Byte,Float"), "set_custom_data_format", "get_custom_data_format");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "instance_count", PropertyHint::Range, "0,16384,1,or_greater"), "set_instance_count", "get_instance_count");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "visible_instance_count", PropertyHint::Range, "-1,16384,1,or_greater"), "set_visible_instance_count", "get_visible_instance_count");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "mesh", PropertyHint::ResourceType, "Mesh"), "set_mesh", "get_mesh");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR3_ARRAY, "transform_array", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_transform_array", "_get_transform_array");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR2_ARRAY, "transform_2d_array", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_transform_2d_array", "_get_transform_2d_array");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_COLOR_ARRAY, "color_array", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_color_array", "_get_color_array");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_COLOR_ARRAY, "custom_data_array", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_custom_data_array", "_get_custom_data_array");

    BIND_ENUM_CONSTANT(TRANSFORM_2D);
    BIND_ENUM_CONSTANT(TRANSFORM_3D);

    BIND_ENUM_CONSTANT(COLOR_NONE);
    BIND_ENUM_CONSTANT(COLOR_8BIT);
    BIND_ENUM_CONSTANT(COLOR_FLOAT);

    BIND_ENUM_CONSTANT(CUSTOM_DATA_NONE);
    BIND_ENUM_CONSTANT(CUSTOM_DATA_8BIT);
    BIND_ENUM_CONSTANT(CUSTOM_DATA_FLOAT);
}

MultiMesh::MultiMesh() {

    multimesh = RenderingServer::get_singleton()->multimesh_create();
    color_format = COLOR_NONE;
    custom_data_format = CUSTOM_DATA_NONE;
    transform_format = TRANSFORM_2D;
    visible_instance_count = -1;
    instance_count = 0;
}

MultiMesh::~MultiMesh() {

    RenderingServer::get_singleton()->free_rid(multimesh);
}
