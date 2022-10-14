/*************************************************************************/
/*  mesh_library.cpp                                                     */
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

#include "mesh_library.h"

#include "box_shape_3d.h"
#include "scene/resources/texture.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/engine.h"

IMPL_GDCLASS(MeshLibrary)
RES_BASE_EXTENSION_IMPL(MeshLibrary,"meshlib")

bool MeshLibrary::_set(const StringName &p_name, const Variant &p_value) {
    using namespace eastl;
    StringView name = p_name;
    if (StringUtils::begins_with(name,"item/")) {

        int idx = StringUtils::to_int(StringUtils::get_slice(name,'/', 1));
        StringView what = StringUtils::get_slice(name,'/', 2);
        if (!item_map.contains(idx)) {
            create_item(idx);
        }

        if (what == "name"_sv) {
            set_item_name(idx, p_value.as<String>());
        }
        else if (what == "mesh"_sv) {
            set_item_mesh(idx, refFromVariant<Mesh>(p_value));
        } else if (what == "mesh_transform"_sv) {
            set_item_mesh_transform(idx, p_value.as<Transform>());
        } else if (what == "shape"_sv) {
            PoolVector<ShapeData> shapes;
            ShapeData sd;
            sd.shape = refFromVariant<Shape>(p_value);
            shapes.push_back(sd);
            set_item_shapes(idx, shapes);
        } else if (what == "shapes"_sv) {
            _set_item_shapes(idx, p_value.as<Array>());
        } else if (what == "preview"_sv)
            set_item_preview(idx, refFromVariant<Texture>(p_value));
        else if (what == "navmesh"_sv) {
            set_item_navmesh(idx, refFromVariant<NavigationMesh>(p_value));
        }
        else if (what == "navmesh_transform"_sv) {
            set_item_navmesh_transform(idx, p_value.as<Transform>());
        }
        else {
            return false;
        }

        return true;
    }

    return false;
}

bool MeshLibrary::_get(const StringName &p_name, Variant &r_ret) const {
    using namespace eastl;

    StringView name(p_name);
    int idx = StringUtils::to_int(StringUtils::get_slice(name,'/', 1));
    ERR_FAIL_COND_V(!item_map.contains(idx), false);
    StringView what = StringUtils::get_slice(name,'/', 2);

    if (what == "name"_sv)
        r_ret = get_item_name(idx);
    else if (what == "mesh"_sv)
        r_ret = get_item_mesh(idx);
    else if (what == "mesh_transform"_sv) {
        r_ret = get_item_mesh_transform(idx);
    }
    else if (what == "shapes"_sv)
        r_ret = _get_item_shapes(idx);
    else if (what == "navmesh"_sv)
        r_ret = get_item_navmesh(idx);
    else if (what == "navmesh_transform"_sv)
        r_ret = get_item_navmesh_transform(idx);
    else if (what == "preview"_sv)
        r_ret = get_item_preview(idx);
    else
        return false;

    return true;
}

void MeshLibrary::_get_property_list(Vector<PropertyInfo> *p_list) const {

    for (const eastl::pair<const int,Item> &E : item_map) {

        String name = "item/" + itos(E.first) + "/";
        p_list->push_back(PropertyInfo(VariantType::STRING, StringName(name + "name")));
        p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(name + "mesh"), PropertyHint::ResourceType, "Mesh"));
        p_list->push_back(PropertyInfo(VariantType::TRANSFORM, StringName(name + "mesh_transform")));
        p_list->push_back(PropertyInfo(VariantType::ARRAY, StringName(name + "shapes")));
        p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(name + "navmesh"), PropertyHint::ResourceType, "NavigationMesh"));
        p_list->push_back(PropertyInfo(VariantType::TRANSFORM, StringName(name + "navmesh_transform")));
        p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(name + "preview"), PropertyHint::ResourceType, "Texture")); //, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_EDITOR_HELPER
    }
}

void MeshLibrary::create_item(int p_item) {

    ERR_FAIL_COND(p_item < 0);
    ERR_FAIL_COND(item_map.contains(p_item));
    item_map[p_item] = Item();
    Object_change_notify(this);
}

void MeshLibrary::set_item_name(int p_item, StringView p_name) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].name = p_name;
    emit_changed();
    Object_change_notify(this);
}

void MeshLibrary::set_item_mesh(int p_item, const Ref<Mesh> &p_mesh) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].mesh = p_mesh;
    notify_change_to_owners();
    emit_changed();
    Object_change_notify(this);
}

void MeshLibrary::set_item_mesh_transform(int p_item, const Transform &p_transform) {
    ERR_FAIL_COND_MSG(!item_map.contains(p_item), "Requested for nonexistent MeshLibrary item '" + itos(p_item) + "'.");
    item_map[p_item].mesh_transform = p_transform;
    notify_change_to_owners();
    emit_changed();
    Object_change_notify(this);
}

void MeshLibrary::set_item_shapes(int p_item, const PoolVector<ShapeData> &p_shapes) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].shapes = p_shapes;
    Object_change_notify(this);
    notify_change_to_owners();
    emit_changed();
    Object_change_notify(this);
}

void MeshLibrary::set_item_navmesh(int p_item, const Ref<NavigationMesh> &p_navmesh) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].navmesh = p_navmesh;
    Object_change_notify(this);
    notify_change_to_owners();
    emit_changed();
    Object_change_notify(this);
}

void MeshLibrary::set_item_navmesh_transform(int p_item, const Transform &p_transform) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].navmesh_transform = p_transform;
    notify_change_to_owners();
    emit_changed();
    Object_change_notify(this);
}

void MeshLibrary::set_item_preview(int p_item, const Ref<Texture> &p_preview) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].preview = p_preview;
    emit_changed();
    Object_change_notify(this);
}

const String &MeshLibrary::get_item_name(int p_item) const {

    ERR_FAIL_COND_V_MSG(!item_map.contains(p_item), null_string, "Requested for nonexistent MeshLibrary item '" + itos(p_item) + "'.");
    return item_map.at(p_item).name;
}

Ref<Mesh> MeshLibrary::get_item_mesh(int p_item) const {

    ERR_FAIL_COND_V_MSG(!item_map.contains(p_item), Ref<Mesh>(), "Requested for nonexistent MeshLibrary item '" + itos(p_item) + "'.");
    return item_map.at(p_item).mesh;
}

Transform MeshLibrary::get_item_mesh_transform(int p_item) const {
    ERR_FAIL_COND_V_MSG(!item_map.contains(p_item), Transform(), "Requested for nonexistent MeshLibrary item '" + itos(p_item) + "'.");
    return item_map.at(p_item).mesh_transform;
}

PoolVector<MeshLibrary::ShapeData> MeshLibrary::get_item_shapes(int p_item) const {

    ERR_FAIL_COND_V(!item_map.contains(p_item), PoolVector<ShapeData>());
    return item_map.at(p_item).shapes;
}

Ref<NavigationMesh> MeshLibrary::get_item_navmesh(int p_item) const {

    ERR_FAIL_COND_V(!item_map.contains(p_item), Ref<NavigationMesh>());
    return item_map.at(p_item).navmesh;
}

Transform MeshLibrary::get_item_navmesh_transform(int p_item) const {

    ERR_FAIL_COND_V(!item_map.contains(p_item), Transform());
    return item_map.at(p_item).navmesh_transform;
}

Ref<Texture> MeshLibrary::get_item_preview(int p_item) const {

    if (!Engine::get_singleton()->is_editor_hint()) {
        ERR_PRINT("MeshLibrary item previews are only generated in an editor context, which means they aren't available in a running project.");
        return Ref<Texture>();
    }

    ERR_FAIL_COND_V_MSG(!item_map.contains(p_item), Ref<Texture>(), "Requested for nonexistent MeshLibrary item '" + itos(p_item) + "'.");
    return item_map.at(p_item).preview;
}

bool MeshLibrary::has_item(int p_item) const {

    return item_map.contains(p_item);
}
void MeshLibrary::remove_item(int p_item) {

    ERR_FAIL_COND_MSG(!item_map.contains(p_item), "Requested for nonexistent MeshLibrary item '" + itos(p_item) + "'.");
    item_map.erase(p_item);
    notify_change_to_owners();
    Object_change_notify(this);
    emit_changed();
}

void MeshLibrary::clear() {

    item_map.clear();
    notify_change_to_owners();
    Object_change_notify(this);
    emit_changed();
}

Vector<int> MeshLibrary::get_item_list() const {

    Vector<int> ret;
    ret.reserve(item_map.size());

    for (const eastl::pair<const int,Item> &E : item_map) {

        ret.push_back(E.first);
    }

    return ret;
}

int MeshLibrary::find_item_by_name(StringView p_name) const {

    for (const eastl::pair<const int,Item> &E : item_map) {

        if (E.second.name == p_name)
            return E.first;
    }
    return -1;
}

int MeshLibrary::get_last_unused_item_id() const {

    if (item_map.empty())
        return 0;
    else
        return item_map.rbegin()->first + 1;
}

void MeshLibrary::_set_item_shapes(int p_item, const Array &p_shapes) {
    Array arr_shapes = p_shapes;
    int size = p_shapes.size();
    if (size & 1) {
        ERR_FAIL_COND_MSG(!item_map.contains(p_item), "Requested for nonexistent MeshLibrary item '" + itos(p_item) + "'.");
        int prev_size = item_map[p_item].shapes.size() * 2;

        if (prev_size < size) {
            // Check if last element is a shape.
            Ref<Shape> shape = refFromVariant<Shape>(p_shapes[size - 1]);
            if (!shape) {
                Ref<BoxShape3D> box_shape = make_ref_counted<BoxShape3D>();
                arr_shapes[size - 1] = box_shape;
            }

            // Make sure the added element is a Transform.
            arr_shapes.push_back(Transform());
            size++;
        } else {
            size--;
            arr_shapes.resize(size);
        }
    }
    PoolVector<ShapeData> shapes;
    for (int i = 0; i < size; i += 2) {
        ShapeData sd;
        sd.shape = refFromVariant<Shape>(arr_shapes[i + 0]);
        sd.local_transform = arr_shapes[i + 1].as<Transform>();

        if (sd.shape) {
            shapes.push_back(sd);
        }
    }

    set_item_shapes(p_item, shapes);
}

Array MeshLibrary::_get_item_shapes(int p_item) const {

    PoolVector<ShapeData> shapes = get_item_shapes(p_item);
    Array ret;
    for (int i = 0; i < shapes.size(); i++) {
        ret.push_back(shapes[i].shape);
        ret.push_back(shapes[i].local_transform);
    }

    return ret;
}

void MeshLibrary::_bind_methods() {

    BIND_METHOD(MeshLibrary,create_item);
    BIND_METHOD(MeshLibrary,set_item_name);
    BIND_METHOD(MeshLibrary,set_item_mesh);
    BIND_METHOD(MeshLibrary,set_item_mesh_transform);
    BIND_METHOD(MeshLibrary,set_item_navmesh);
    BIND_METHOD(MeshLibrary,set_item_navmesh_transform);
    MethodBinder::bind_method(D_METHOD("set_item_shapes", {"id", "shapes"}), &MeshLibrary::_set_item_shapes);
    BIND_METHOD(MeshLibrary,set_item_preview);
    BIND_METHOD(MeshLibrary,get_item_name);
    BIND_METHOD(MeshLibrary,get_item_mesh);
    BIND_METHOD(MeshLibrary,get_item_mesh_transform);
    BIND_METHOD(MeshLibrary,get_item_navmesh);
    BIND_METHOD(MeshLibrary,get_item_navmesh_transform);
    MethodBinder::bind_method(D_METHOD("get_item_shapes", {"id"}), &MeshLibrary::_get_item_shapes);
    BIND_METHOD(MeshLibrary,get_item_preview);
    BIND_METHOD(MeshLibrary,remove_item);
    BIND_METHOD(MeshLibrary,find_item_by_name);

    BIND_METHOD(MeshLibrary,clear);
    BIND_METHOD(MeshLibrary,get_item_list);
    BIND_METHOD(MeshLibrary,get_last_unused_item_id);
}

MeshLibrary::MeshLibrary() {
}
MeshLibrary::~MeshLibrary() {
}
