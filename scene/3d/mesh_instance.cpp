/*************************************************************************/
/*  mesh_instance.cpp                                                    */
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

#include "mesh_instance.h"

#include "collision_shape.h"
#include "core/method_bind.h"
#include "core/core_string_names.h"
#include "core/object_tooling.h"
#include "physics_body.h"
#include "scene/resources/material.h"
#include "scene/scene_string_names.h"
#include "skeleton.h"
#include "scene/main/scene_tree.h"
#include "servers/visual_server.h"
#include "EASTL/sort.h"

IMPL_GDCLASS(MeshInstance)

bool MeshInstance::_set(const StringName &p_name, const Variant &p_value) {

    //this is not _too_ bad performance wise, really. it only arrives here if the property was not set anywhere else.
    //add to it that it's probably found on first call to _set anyway.

    if (!get_instance().is_valid())
        return false;

    Map<StringName, BlendShapeTrack>::iterator E = blend_shape_tracks.find(p_name);
    if (E!=blend_shape_tracks.end()) {
        E->second.value = p_value;
        VisualServer::get_singleton()->instance_set_blend_shape_weight(get_instance(), E->second.idx, E->second.value);
        return true;
    }

    if (StringUtils::begins_with(p_name,"material/")) {
        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        if (idx >= materials.size() || idx < 0)
            return false;

        set_surface_material(idx, refFromRefPtr<Material>(p_value));
        return true;
    }

    return false;
}

bool MeshInstance::_get(const StringName &p_name, Variant &r_ret) const {

    if (!get_instance().is_valid())
        return false;

    const Map<StringName, BlendShapeTrack>::const_iterator E = blend_shape_tracks.find(p_name);
    if (E!=blend_shape_tracks.end()) {
        r_ret = E->second.value;
        return true;
    }

    if (StringUtils::begins_with(p_name,"material/")) {
        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        if (idx >= materials.size() || idx < 0)
            return false;
        r_ret = materials[idx];
        return true;
    }
    return false;
}

void MeshInstance::_get_property_list(PODVector<PropertyInfo> *p_list) const {

    PODVector<StringName> ls;
    ls.reserve(blend_shape_tracks.size());
    for (const eastl::pair<const StringName,BlendShapeTrack> &E : blend_shape_tracks) {

        ls.emplace_back(E.first);
    }

    eastl::sort(ls.begin(), ls.end());

    for (const StringName &E : ls) {
        p_list->push_back(PropertyInfo(VariantType::REAL, E, PropertyHint::Range, "0,1,0.00001"));
    }

    if (mesh) {
        for (int i = 0; i < mesh->get_surface_count(); i++) {
            p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName("material/" + itos(i)), PropertyHint::ResourceType, "ShaderMaterial,SpatialMaterial"));
        }
    }
}

void MeshInstance::set_mesh(const Ref<Mesh> &p_mesh) {

    if (mesh == p_mesh)
        return;

    if (mesh) {
        mesh->disconnect(CoreStringNames::get_singleton()->changed, this, SceneStringNames::get_singleton()->_mesh_changed);
        materials.clear();
    }

    mesh = p_mesh;

    blend_shape_tracks.clear();
    if (mesh) {

        for (int i = 0; i < mesh->get_blend_shape_count(); i++) {

            BlendShapeTrack mt;
            mt.idx = i;
            mt.value = 0;
            blend_shape_tracks[StringName("blend_shapes/" + String(mesh->get_blend_shape_name(i)))] = mt;
        }

        mesh->connect(CoreStringNames::get_singleton()->changed, this, SceneStringNames::get_singleton()->_mesh_changed);
        materials.resize(mesh->get_surface_count());

        set_base(mesh->get_rid());
    } else {

        set_base(RID());
    }

    update_gizmo();

    Object_change_notify(this);
}
Ref<Mesh> MeshInstance::get_mesh() const {

    return mesh;
}

void MeshInstance::_resolve_skeleton_path() {

    Ref<SkinReference> new_skin_reference;

    if (!skeleton_path.is_empty()) {
    Skeleton *skeleton = object_cast<Skeleton>(get_node(skeleton_path));
        if (skeleton) {
            new_skin_reference = skeleton->register_skin(skin_internal);
            if (not skin_internal) {
                //a skin was created for us
                skin_internal = new_skin_reference->get_skin();
                Object_change_notify(this);
            }
        }
    }

    skin_ref = new_skin_reference;

    if (skin_ref) {
        VisualServer::get_singleton()->instance_attach_skeleton(get_instance(), skin_ref->get_skeleton());
    } else {
        VisualServer::get_singleton()->instance_attach_skeleton(get_instance(), RID());
    }
}

void MeshInstance::set_skin(const Ref<Skin> &p_skin) {
    skin_internal = p_skin;
    skin = p_skin;
    if (!is_inside_tree())
        return;
    _resolve_skeleton_path();
}

Ref<Skin> MeshInstance::get_skin() const {
    return skin;
}

void MeshInstance::set_skeleton_path(const NodePath &p_skeleton) {

    skeleton_path = p_skeleton;
    if (!is_inside_tree())
        return;
    _resolve_skeleton_path();
}

NodePath MeshInstance::get_skeleton_path() {
    return skeleton_path;
}

AABB MeshInstance::get_aabb() const {

    if (mesh)
        return mesh->get_aabb();

    return AABB();
}

PoolVector<Face3> MeshInstance::get_faces(uint32_t p_usage_flags) const {

    if (!(p_usage_flags & (FACES_SOLID | FACES_ENCLOSING)))
        return PoolVector<Face3>();

    if (not mesh)
        return PoolVector<Face3>();

    return mesh->get_faces();
}

Node *MeshInstance::create_trimesh_collision_node() {

    if (not mesh)
        return nullptr;

    Ref<Shape> shape = mesh->create_trimesh_shape();
    if (not shape)
        return nullptr;

    StaticBody *static_body = memnew(StaticBody);
    CollisionShape *cshape = memnew(CollisionShape);
    cshape->set_shape(shape);
    static_body->add_child(cshape);
    return static_body;
}

void MeshInstance::create_trimesh_collision() {

    StaticBody *static_body = object_cast<StaticBody>(create_trimesh_collision_node());
    ERR_FAIL_COND(!static_body);
    static_body->set_name(String(get_name()) + "_col");

    add_child(static_body);
    if (get_owner()) {
        CollisionShape *cshape = object_cast<CollisionShape>(static_body->get_child(0));
        static_body->set_owner(get_owner());
        cshape->set_owner(get_owner());
    }
}

Node *MeshInstance::create_convex_collision_node() {

    if (not mesh)
        return nullptr;

    Ref<Shape> shape = mesh->create_convex_shape();
    if (not shape)
        return nullptr;

    StaticBody *static_body = memnew(StaticBody);
    CollisionShape *cshape = memnew(CollisionShape);
    cshape->set_shape(shape);
    static_body->add_child(cshape);
    return static_body;
}

void MeshInstance::create_convex_collision() {

    StaticBody *static_body = object_cast<StaticBody>(create_convex_collision_node());
    ERR_FAIL_COND(!static_body);
    static_body->set_name(String(get_name()) + "_col");

    add_child(static_body);
    if (get_owner()) {
        CollisionShape *cshape = object_cast<CollisionShape>(static_body->get_child(0));
        static_body->set_owner(get_owner());
        cshape->set_owner(get_owner());
    }
}

void MeshInstance::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {
        _resolve_skeleton_path();
    }
}

int MeshInstance::get_surface_material_count() const {

    return materials.size();
}

void MeshInstance::set_surface_material(int p_surface, const Ref<Material> &p_material) {

    ERR_FAIL_INDEX(p_surface, materials.size());

    materials[p_surface] = p_material;

    if (materials[p_surface])
        VisualServer::get_singleton()->instance_set_surface_material(get_instance(), p_surface, materials[p_surface]->get_rid());
    else
        VisualServer::get_singleton()->instance_set_surface_material(get_instance(), p_surface, RID());
}

Ref<Material> MeshInstance::get_surface_material(int p_surface) const {

    ERR_FAIL_INDEX_V(p_surface, materials.size(), Ref<Material>());

    return materials[p_surface];
}

void MeshInstance::_mesh_changed() {

    materials.resize(mesh->get_surface_count());
}

void MeshInstance::create_debug_tangents() {

    PODVector<Vector3> lines;
    PODVector<Color> colors;

    Ref<Mesh> mesh = get_mesh();
    if (not mesh)
        return;

    for (int i = 0; i < mesh->get_surface_count(); i++) {
        Array arrays = mesh->surface_get_arrays(i);
        Vector<Vector3> verts = arrays[Mesh::ARRAY_VERTEX].as<Vector<Vector3>>();
        Vector<Vector3> norms = arrays[Mesh::ARRAY_NORMAL].as<Vector<Vector3>>();
        if (norms.empty())
            continue;
        Vector<float> tangents = arrays[Mesh::ARRAY_TANGENT].as<Vector<float>>();
        if (tangents.empty())
            continue;
        lines.reserve(6*verts.size());
        for (int j = 0; j < verts.size(); j++) {
            Vector3 v = verts[j];
            Vector3 n = norms[j];
            Vector3 t = Vector3(tangents[j * 4 + 0], tangents[j * 4 + 1], tangents[j * 4 + 2]);
            Vector3 b = (n.cross(t)).normalized() * tangents[j * 4 + 3];

            lines.emplace_back(v); //normal
            colors.emplace_back(0, 0, 1); //color
            lines.emplace_back(v + n * 0.04f); //normal
            colors.emplace_back(0, 0, 1); //color

            lines.emplace_back(v); //tangent
            colors.emplace_back(1, 0, 0); //color
            lines.emplace_back(v + t * 0.04f); //tangent
            colors.emplace_back(1, 0, 0); //color

            lines.emplace_back(v); //binormal
            colors.emplace_back(0, 1, 0); //color
            lines.emplace_back(v + b * 0.04f); //binormal
            colors.emplace_back(0, 1, 0); //color
        }
    }

    if (!lines.empty()) {

        Ref<SpatialMaterial> sm(make_ref_counted<SpatialMaterial>());

        sm->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
        sm->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
        sm->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);

        Ref<ArrayMesh> am(make_ref_counted<ArrayMesh>());
        Array a;
        a.resize(Mesh::ARRAY_MAX);
        a[Mesh::ARRAY_VERTEX] = Variant::from(lines);
        a[Mesh::ARRAY_COLOR] = Variant::from(colors);

        am->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, a);
        am->surface_set_material(0, sm);

        MeshInstance *mi = memnew(MeshInstance);
        mi->set_mesh(am);
        mi->set_name("DebugTangents");
        add_child(mi);
#ifdef TOOLS_ENABLED

        if (this == get_tree()->get_edited_scene_root())
            mi->set_owner(this);
        else
            mi->set_owner(get_owner());
#endif
    }
}

void MeshInstance::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_mesh", {"mesh"}), &MeshInstance::set_mesh);
    MethodBinder::bind_method(D_METHOD("get_mesh"), &MeshInstance::get_mesh);
    MethodBinder::bind_method(D_METHOD("set_skeleton_path", {"skeleton_path"}), &MeshInstance::set_skeleton_path);
    MethodBinder::bind_method(D_METHOD("get_skeleton_path"), &MeshInstance::get_skeleton_path);
    MethodBinder::bind_method(D_METHOD("set_skin", {"skin"}), &MeshInstance::set_skin);
    MethodBinder::bind_method(D_METHOD("get_skin"), &MeshInstance::get_skin);

    MethodBinder::bind_method(D_METHOD("get_surface_material_count"), &MeshInstance::get_surface_material_count);
    MethodBinder::bind_method(D_METHOD("set_surface_material", {"surface", "material"}), &MeshInstance::set_surface_material);
    MethodBinder::bind_method(D_METHOD("get_surface_material", {"surface"}), &MeshInstance::get_surface_material);

    MethodBinder::bind_method(D_METHOD("create_trimesh_collision"), &MeshInstance::create_trimesh_collision);
    ClassDB::set_method_flags("MeshInstance", "create_trimesh_collision", METHOD_FLAGS_DEFAULT);
    MethodBinder::bind_method(D_METHOD("create_convex_collision"), &MeshInstance::create_convex_collision);
    ClassDB::set_method_flags("MeshInstance", "create_convex_collision", METHOD_FLAGS_DEFAULT);
    MethodBinder::bind_method(D_METHOD("_mesh_changed"), &MeshInstance::_mesh_changed);

    MethodBinder::bind_method(D_METHOD("create_debug_tangents"), &MeshInstance::create_debug_tangents);
    ClassDB::set_method_flags("MeshInstance", "create_debug_tangents", METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "mesh", PropertyHint::ResourceType, "Mesh"), "set_mesh", "get_mesh");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "skin", PropertyHint::ResourceType, "Skin"), "set_skin", "get_skin");
    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "skeleton", PropertyHint::NodePathValidTypes, "Skeleton"), "set_skeleton_path", "get_skeleton_path");
}

MeshInstance::MeshInstance() {
    skeleton_path = NodePath("..");
}

MeshInstance::~MeshInstance() {
}
