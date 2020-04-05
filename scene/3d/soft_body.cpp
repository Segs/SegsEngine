/*************************************************************************/
/*  soft_body.cpp                                                        */
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

#include "soft_body.h"
#include "core/list.h"
#include "core/method_bind.h"
#include "core/object.h"
#include "core/object_db.h"
#include "core/os/os.h"
#include "core/rid.h"
#include "core/object_tooling.h"
#include "core/translation_helpers.h"
#include "scene/3d/collision_object_3d.h"
#include "scene/3d/physics_body.h"
#include "scene/3d/skeleton.h"
#include "scene/main/scene_tree.h"

#include "servers/physics_server.h"
#include "servers/visual_server.h"

IMPL_GDCLASS(SoftBody)

SoftBodyVisualServerHandler::SoftBodyVisualServerHandler() {}

void SoftBodyVisualServerHandler::prepare(RID p_mesh, int p_surface) {
    clear();

    ERR_FAIL_COND(!p_mesh.is_valid());

    mesh = p_mesh;
    surface = p_surface;

    const uint32_t surface_format = VisualServer::get_singleton()->mesh_surface_get_format(mesh, surface);
    const int surface_vertex_len = VisualServer::get_singleton()->mesh_surface_get_array_len(mesh, p_surface);
    const int surface_index_len = VisualServer::get_singleton()->mesh_surface_get_array_index_len(mesh, p_surface);
    uint32_t surface_offsets[VS::ARRAY_MAX];

    buffer = VisualServer::get_singleton()->mesh_surface_get_array(mesh, surface);
    stride = VisualServer::get_singleton()->mesh_surface_make_offsets_from_format(surface_format, surface_vertex_len, surface_index_len, surface_offsets);
    offset_vertices = surface_offsets[VS::ARRAY_VERTEX];
    offset_normal = surface_offsets[VS::ARRAY_NORMAL];
}

void SoftBodyVisualServerHandler::clear() {

    if (mesh.is_valid()) {
        buffer.resize(0);
    }

    mesh = RID();
}

void SoftBodyVisualServerHandler::open() {
    write_buffer = buffer.write();
}

void SoftBodyVisualServerHandler::close() {
    write_buffer.release();
}

void SoftBodyVisualServerHandler::commit_changes() {
    VisualServer::get_singleton()->mesh_surface_update_region(mesh, surface, 0, buffer);
}

void SoftBodyVisualServerHandler::set_vertex(int p_vertex_id, const void *p_vector3) {
    memcpy(&write_buffer[p_vertex_id * stride + offset_vertices], p_vector3, sizeof(float) * 3);
}

void SoftBodyVisualServerHandler::set_normal(int p_vertex_id, const void *p_vector3) {
    memcpy(&write_buffer[p_vertex_id * stride + offset_normal], p_vector3, sizeof(float) * 3);
}

void SoftBodyVisualServerHandler::set_aabb(const AABB &p_aabb) {
    VisualServer::get_singleton()->mesh_set_custom_aabb(mesh, p_aabb);
}

SoftBody::PinnedPoint::PinnedPoint() :
        point_index(-1),
        spatial_attachment(nullptr) {
}

SoftBody::PinnedPoint::PinnedPoint(const PinnedPoint &obj_tocopy) {
    point_index = obj_tocopy.point_index;
    spatial_attachment_path = obj_tocopy.spatial_attachment_path;
    spatial_attachment = obj_tocopy.spatial_attachment;
    offset = obj_tocopy.offset;
}

SoftBody::PinnedPoint SoftBody::PinnedPoint::operator=(const PinnedPoint &obj) {
    point_index = obj.point_index;
    spatial_attachment_path = obj.spatial_attachment_path;
    spatial_attachment = obj.spatial_attachment;
    offset = obj.offset;
    return *this;
}

void SoftBody::_update_pickable() {
    if (!is_inside_tree())
        return;
    bool pickable = ray_pickable && is_visible_in_tree();
    PhysicsServer::get_singleton()->soft_body_set_ray_pickable(physics_rid, pickable);
}

bool SoftBody::_set(const StringName &p_name, const Variant &p_value) {

    StringView which = StringUtils::get_slice(p_name,'/', 0);

    if (StringView("pinned_points") == which) {

        return _set_property_pinned_points_indices(p_value);

    } else if (StringView("attachments") == which) {

        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        StringView what = StringUtils::get_slice(p_name,'/', 2);

        return _set_property_pinned_points_attachment(idx, what, p_value);
    }

    return false;
}

bool SoftBody::_get(const StringName &p_name, Variant &r_ret) const {
    StringView which = StringUtils::get_slice(p_name,'/', 0);

    if (StringView("pinned_points") == which) {
        Array arr_ret;
        const int pinned_points_indices_size = pinned_points.size();
        PoolVector<PinnedPoint>::Read r = pinned_points.read();
        arr_ret.resize(pinned_points_indices_size);

        for (int i = 0; i < pinned_points_indices_size; ++i) {
            arr_ret[i] = r[i].point_index;
        }

        r_ret = arr_ret;
        return true;

    } else if (StringView("attachments") == which) {

        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        StringView what = StringUtils::get_slice(p_name,'/', 2);

        return _get_property_pinned_points(idx, what, r_ret);
    }

    return false;
}

void SoftBody::_get_property_list(Vector<PropertyInfo> *p_list) const {

    const int pinned_points_indices_size = pinned_points.size();

    p_list->push_back(PropertyInfo(VariantType::POOL_INT_ARRAY, "pinned_points"));

    for (int i = 0; i < pinned_points_indices_size; ++i) {
        p_list->push_back(PropertyInfo(VariantType::INT, StringName("attachments/" + itos(i) + "/point_index")));
        p_list->push_back(PropertyInfo(VariantType::NODE_PATH, StringName("attachments/" + itos(i) + "/spatial_attachment_path")));
        p_list->push_back(PropertyInfo(VariantType::VECTOR3, StringName("attachments/" + itos(i) + "/offset")));
    }
}

bool SoftBody::_set_property_pinned_points_indices(const Array &p_indices) {

    const int p_indices_size = p_indices.size();

    { // Remove the pined points on physics server that will be removed by resize
        PoolVector<PinnedPoint>::Read r = pinned_points.read();
        if (p_indices_size < pinned_points.size()) {
            for (int i = pinned_points.size() - 1; i >= p_indices_size; --i) {
                pin_point(r[i].point_index, false);
            }
        }
    }

    pinned_points.resize(p_indices_size);

    PoolVector<PinnedPoint>::Write w = pinned_points.write();
    int point_index;
    for (int i = 0; i < p_indices_size; ++i) {
        point_index = p_indices.get(i);
        if (w[i].point_index != point_index) {
            if (-1 != w[i].point_index)
                pin_point(w[i].point_index, false);
            w[i].point_index = point_index;
            pin_point(w[i].point_index, true);
        }
    }
    return true;
}

bool SoftBody::_set_property_pinned_points_attachment(int p_item, StringView p_what, const Variant &p_value) {
    if (pinned_points.size() <= p_item) {
        return false;
    }

    if (StringView("spatial_attachment_path") == p_what) {
        PoolVector<PinnedPoint>::Write w = pinned_points.write();
        pin_point(w[p_item].point_index, true, p_value);
        _make_cache_dirty();
    } else if (StringView("offset") == p_what) {
        PoolVector<PinnedPoint>::Write w = pinned_points.write();
        w[p_item].offset = p_value;
    } else {
        return false;
    }

    return true;
}

bool SoftBody::_get_property_pinned_points(int p_item, StringView p_what, Variant &r_ret) const {
    if (pinned_points.size() <= p_item) {
        return false;
    }
    PoolVector<PinnedPoint>::Read r = pinned_points.read();

    if (StringView("point_index") == p_what) {
        r_ret = r[p_item].point_index;
    } else if (StringView("spatial_attachment_path") == p_what) {
        r_ret = r[p_item].spatial_attachment_path;
    } else if (StringView("offset") == p_what) {
        r_ret = r[p_item].offset;
    } else {
        return false;
    }

    return true;
}

void SoftBody::_changed_callback(Object *p_changed, StringName p_prop) {
    update_physics_server();
    _reset_points_offsets();
#ifdef TOOLS_ENABLED
    if (p_changed == this) {
        update_configuration_warning();
    }
#endif
}

void SoftBody::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_WORLD: {

            if (Engine::get_singleton()->is_editor_hint()) {
                //TODO: SEGS: change receptor
                Object_add_change_receptor(this,this);
            }

            RID space = get_world()->get_space();
            PhysicsServer::get_singleton()->soft_body_set_space(physics_rid, space);
            update_physics_server();
        } break;
        case NOTIFICATION_READY: {
            if (!parent_collision_ignore.is_empty())
                add_collision_exception_with(get_node(parent_collision_ignore));

        } break;
        case NOTIFICATION_TRANSFORM_CHANGED: {

            if (Engine::get_singleton()->is_editor_hint()) {
                _reset_points_offsets();
                return;
            }

            PhysicsServer::get_singleton()->soft_body_set_transform(physics_rid, get_global_transform());

            set_notify_transform(false);
            // Required to be top level with Transform at center of world in order to modify VisualServer only to support custom Transform
            set_as_toplevel(true);
            set_transform(Transform());
            set_notify_transform(true);

        } break;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {

            if (!simulation_started)
                return;

            _update_cache_pin_points_datas();
            // Submit bone attachment
            const int pinned_points_indices_size = pinned_points.size();
            PoolVector<PinnedPoint>::Read r = pinned_points.read();
            for (int i = 0; i < pinned_points_indices_size; ++i) {
                if (r[i].spatial_attachment) {
                    PhysicsServer::get_singleton()->soft_body_move_point(physics_rid, r[i].point_index, r[i].spatial_attachment->get_global_transform().xform(r[i].offset));
                }
            }
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {

            _update_pickable();

        } break;
        case NOTIFICATION_EXIT_WORLD: {

            PhysicsServer::get_singleton()->soft_body_set_space(physics_rid, RID());

        } break;
    }

#ifdef TOOLS_ENABLED

    if (p_what == NOTIFICATION_LOCAL_TRANSFORM_CHANGED) {
        if (Engine::get_singleton()->is_editor_hint()) {
            update_configuration_warning();
        }
    }

#endif
}

void SoftBody::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_draw_soft_mesh"), &SoftBody::_draw_soft_mesh);

    MethodBinder::bind_method(D_METHOD("set_collision_mask", {"collision_mask"}), &SoftBody::set_collision_mask);
    MethodBinder::bind_method(D_METHOD("get_collision_mask"), &SoftBody::get_collision_mask);

    MethodBinder::bind_method(D_METHOD("set_collision_layer", {"collision_layer"}), &SoftBody::set_collision_layer);
    MethodBinder::bind_method(D_METHOD("get_collision_layer"), &SoftBody::get_collision_layer);

    MethodBinder::bind_method(D_METHOD("set_collision_mask_bit", {"bit", "value"}), &SoftBody::set_collision_mask_bit);
    MethodBinder::bind_method(D_METHOD("get_collision_mask_bit", {"bit"}), &SoftBody::get_collision_mask_bit);

    MethodBinder::bind_method(D_METHOD("set_collision_layer_bit", {"bit", "value"}), &SoftBody::set_collision_layer_bit);
    MethodBinder::bind_method(D_METHOD("get_collision_layer_bit", {"bit"}), &SoftBody::get_collision_layer_bit);

    MethodBinder::bind_method(D_METHOD("set_parent_collision_ignore", {"parent_collision_ignore"}), &SoftBody::set_parent_collision_ignore);
    MethodBinder::bind_method(D_METHOD("get_parent_collision_ignore"), &SoftBody::get_parent_collision_ignore);

    MethodBinder::bind_method(D_METHOD("get_collision_exceptions"), &SoftBody::get_collision_exceptions);
    MethodBinder::bind_method(D_METHOD("add_collision_exception_with", {"body"}), &SoftBody::add_collision_exception_with);
    MethodBinder::bind_method(D_METHOD("remove_collision_exception_with", {"body"}), &SoftBody::remove_collision_exception_with);

    MethodBinder::bind_method(D_METHOD("set_simulation_precision", {"simulation_precision"}), &SoftBody::set_simulation_precision);
    MethodBinder::bind_method(D_METHOD("get_simulation_precision"), &SoftBody::get_simulation_precision);

    MethodBinder::bind_method(D_METHOD("set_total_mass", {"mass"}), &SoftBody::set_total_mass);
    MethodBinder::bind_method(D_METHOD("get_total_mass"), &SoftBody::get_total_mass);

    MethodBinder::bind_method(D_METHOD("set_linear_stiffness", {"linear_stiffness"}), &SoftBody::set_linear_stiffness);
    MethodBinder::bind_method(D_METHOD("get_linear_stiffness"), &SoftBody::get_linear_stiffness);

    MethodBinder::bind_method(D_METHOD("set_areaAngular_stiffness", {"areaAngular_stiffness"}), &SoftBody::set_areaAngular_stiffness);
    MethodBinder::bind_method(D_METHOD("get_areaAngular_stiffness"), &SoftBody::get_areaAngular_stiffness);

    MethodBinder::bind_method(D_METHOD("set_volume_stiffness", {"volume_stiffness"}), &SoftBody::set_volume_stiffness);
    MethodBinder::bind_method(D_METHOD("get_volume_stiffness"), &SoftBody::get_volume_stiffness);

    MethodBinder::bind_method(D_METHOD("set_pressure_coefficient", {"pressure_coefficient"}), &SoftBody::set_pressure_coefficient);
    MethodBinder::bind_method(D_METHOD("get_pressure_coefficient"), &SoftBody::get_pressure_coefficient);

    MethodBinder::bind_method(D_METHOD("set_pose_matching_coefficient", {"pose_matching_coefficient"}), &SoftBody::set_pose_matching_coefficient);
    MethodBinder::bind_method(D_METHOD("get_pose_matching_coefficient"), &SoftBody::get_pose_matching_coefficient);

    MethodBinder::bind_method(D_METHOD("set_damping_coefficient", {"damping_coefficient"}), &SoftBody::set_damping_coefficient);
    MethodBinder::bind_method(D_METHOD("get_damping_coefficient"), &SoftBody::get_damping_coefficient);

    MethodBinder::bind_method(D_METHOD("set_drag_coefficient", {"drag_coefficient"}), &SoftBody::set_drag_coefficient);
    MethodBinder::bind_method(D_METHOD("get_drag_coefficient"), &SoftBody::get_drag_coefficient);

    MethodBinder::bind_method(D_METHOD("set_ray_pickable", {"ray_pickable"}), &SoftBody::set_ray_pickable);
    MethodBinder::bind_method(D_METHOD("is_ray_pickable"), &SoftBody::is_ray_pickable);

    ADD_GROUP("Collision", "collision_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_layer", PropertyHint::Layers3DPhysics), "set_collision_layer", "get_collision_layer");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_mask", PropertyHint::Layers3DPhysics), "set_collision_mask", "get_collision_mask");

    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "parent_collision_ignore", PropertyHint::PropertyOfVariantType, "Parent collision object"), "set_parent_collision_ignore", "get_parent_collision_ignore");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "simulation_precision", PropertyHint::Range, "1,100,1"), "set_simulation_precision", "get_simulation_precision");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "total_mass", PropertyHint::Range, "0.01,10000,1"), "set_total_mass", "get_total_mass");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "linear_stiffness", PropertyHint::Range, "0,1,0.01"), "set_linear_stiffness", "get_linear_stiffness");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "areaAngular_stiffness", PropertyHint::Range, "0,1,0.01"), "set_areaAngular_stiffness", "get_areaAngular_stiffness");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "volume_stiffness", PropertyHint::Range, "0,1,0.01"), "set_volume_stiffness", "get_volume_stiffness");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "pressure_coefficient"), "set_pressure_coefficient", "get_pressure_coefficient");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "damping_coefficient", PropertyHint::Range, "0,1,0.01"), "set_damping_coefficient", "get_damping_coefficient");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "drag_coefficient", PropertyHint::Range, "0,1,0.01"), "set_drag_coefficient", "get_drag_coefficient");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "pose_matching_coefficient", PropertyHint::Range, "0,1,0.01"), "set_pose_matching_coefficient", "get_pose_matching_coefficient");

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "ray_pickable"), "set_ray_pickable", "is_ray_pickable");
}

StringName SoftBody::get_configuration_warning() const {

    String warning(MeshInstance::get_configuration_warning());

    if (not get_mesh()) {
        if (!warning.empty())
            warning += "\n\n";

        warning += TTR("This body will be ignored until you set a mesh.");
    }

    Transform t = get_transform();
    if ((ABS(t.basis.get_axis(0).length() - 1.0f) > 0.05f || ABS(t.basis.get_axis(1).length() - 1.0f) > 0.05f ||
                ABS(t.basis.get_axis(2).length() - 1.0f) > 0.05f)) {
        if (!warning.empty()) warning += ("\n\n");

        warning += TTR("Size changes to SoftBody will be overridden by the physics engine when running.\nChange the size "
                       "in children collision shapes instead.");
    }

    return StringName(warning);
}

void SoftBody::_draw_soft_mesh() {
    if (not get_mesh())
        return;

    if (!visual_server_handler.is_ready()) {

        visual_server_handler.prepare(get_mesh()->get_rid(), 0);

        /// Necessary in order to render the mesh correctly (Soft body nodes are in global space)
        simulation_started = true;
        call_deferred("set_as_toplevel", true);
        call_deferred("set_transform", Transform());
    }

    visual_server_handler.open();
    PhysicsServer::get_singleton()->soft_body_update_visual_server(physics_rid, &visual_server_handler);
    visual_server_handler.close();

    visual_server_handler.commit_changes();
}

void SoftBody::update_physics_server() {

    if (Engine::get_singleton()->is_editor_hint()) {

        if (get_mesh())
            PhysicsServer::get_singleton()->soft_body_set_mesh(physics_rid, get_mesh());
        else
            PhysicsServer::get_singleton()->soft_body_set_mesh(physics_rid, REF());

        return;
    }
    auto VS = VisualServer::get_singleton();
    if (get_mesh()) {

        become_mesh_owner();
        PhysicsServer::get_singleton()->soft_body_set_mesh(physics_rid, get_mesh());
        VS->connect("frame_pre_draw", this, "_draw_soft_mesh");
    } else {

        PhysicsServer::get_singleton()->soft_body_set_mesh(physics_rid, REF());
        if(VS->is_connected("frame_pre_draw", this, "_draw_soft_mesh")) {
            VS->disconnect("frame_pre_draw", this, "_draw_soft_mesh");
        }
    }
}

void SoftBody::become_mesh_owner() {
    if (not mesh)
        return;

    if (mesh_owner) // TODO: SEGS: already has owner, report this ?
        return;

    mesh_owner = true;

    Vector<Ref<Material> > copy_materials(materials);

    ERR_FAIL_COND(!mesh->get_surface_count());

    // Get current mesh array and create new mesh array with necessary flag for softbody
    SurfaceArrays surface_arrays = mesh->surface_get_arrays(0);
    Vector<SurfaceArrays> surface_blend_arrays = mesh->surface_get_blend_shape_arrays(0);
    uint32_t surface_format = mesh->surface_get_format(0);

    surface_format &= ~(Mesh::ARRAY_COMPRESS_VERTEX | Mesh::ARRAY_COMPRESS_NORMAL);
    surface_format |= Mesh::ARRAY_FLAG_USE_DYNAMIC_UPDATE;

    Ref<ArrayMesh> soft_mesh(make_ref_counted<ArrayMesh>());
    soft_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, eastl::move(surface_arrays), eastl::move(surface_blend_arrays), surface_format);
    soft_mesh->surface_set_material(0, mesh->surface_get_material(0));

    set_mesh(soft_mesh);

    for (int i = copy_materials.size() - 1; 0 <= i; --i) {
        set_surface_material(i, copy_materials[i]);
    }
}

void SoftBody::set_collision_mask(uint32_t p_mask) {
    collision_mask = p_mask;
    PhysicsServer::get_singleton()->soft_body_set_collision_mask(physics_rid, p_mask);
}

uint32_t SoftBody::get_collision_mask() const {
    return collision_mask;
}
void SoftBody::set_collision_layer(uint32_t p_layer) {
    collision_layer = p_layer;
    PhysicsServer::get_singleton()->soft_body_set_collision_layer(physics_rid, p_layer);
}

uint32_t SoftBody::get_collision_layer() const {
    return collision_layer;
}

void SoftBody::set_collision_mask_bit(int p_bit, bool p_value) {
    uint32_t mask = get_collision_mask();
    if (p_value)
        mask |= 1 << p_bit;
    else
        mask &= ~(1 << p_bit);
    set_collision_mask(mask);
}

bool SoftBody::get_collision_mask_bit(int p_bit) const {
    return get_collision_mask() & (1 << p_bit);
}

void SoftBody::set_collision_layer_bit(int p_bit, bool p_value) {
    uint32_t layer = get_collision_layer();
    if (p_value)
        layer |= 1 << p_bit;
    else
        layer &= ~(1 << p_bit);
    set_collision_layer(layer);
}

bool SoftBody::get_collision_layer_bit(int p_bit) const {
    return get_collision_layer() & (1 << p_bit);
}

void SoftBody::set_parent_collision_ignore(const NodePath &p_parent_collision_ignore) {
    parent_collision_ignore = p_parent_collision_ignore;
}

const NodePath &SoftBody::get_parent_collision_ignore() const {
    return parent_collision_ignore;
}

void SoftBody::set_pinned_points_indices(const PoolVector<SoftBody::PinnedPoint>& p_pinned_points_indices) {
    pinned_points = p_pinned_points_indices;
    PoolVector<PinnedPoint>::Read w = pinned_points.read();
    for (int i = pinned_points.size() - 1; 0 <= i; --i) {
        pin_point(p_pinned_points_indices[i].point_index, true);
    }
}

PoolVector<SoftBody::PinnedPoint> SoftBody::get_pinned_points_indices() {
    return pinned_points;
}

Array SoftBody::get_collision_exceptions() {
    ListOld<RID> exceptions;
    PhysicsServer::get_singleton()->soft_body_get_collision_exceptions(physics_rid, &exceptions);
    Array ret;
    for (ListOld<RID>::Element *E = exceptions.front(); E; E = E->next()) {
        RID body = E->deref();
        ObjectID instance_id = PhysicsServer::get_singleton()->body_get_object_instance_id(body);
        Object *obj = ObjectDB::get_instance(instance_id);
        PhysicsBody *physics_body = object_cast<PhysicsBody>(obj);
        ret.append(Variant(physics_body));
    }
    return ret;
}

void SoftBody::add_collision_exception_with(Node *p_node) {
    ERR_FAIL_NULL(p_node);
    CollisionObject3D *collision_object = object_cast<CollisionObject3D>(p_node);
    ERR_FAIL_COND_MSG(!collision_object, "Collision exception only works between two CollisionObject3Ds.");
    PhysicsServer::get_singleton()->soft_body_add_collision_exception(physics_rid, collision_object->get_rid());
}

void SoftBody::remove_collision_exception_with(Node *p_node) {
    ERR_FAIL_NULL(p_node);
    CollisionObject3D *collision_object = object_cast<CollisionObject3D>(p_node);
    ERR_FAIL_COND_MSG(!collision_object, "Collision exception only works between two CollisionObject3Ds.");
    PhysicsServer::get_singleton()->soft_body_remove_collision_exception(physics_rid, collision_object->get_rid());
}

int SoftBody::get_simulation_precision() {
    return PhysicsServer::get_singleton()->soft_body_get_simulation_precision(physics_rid);
}

void SoftBody::set_simulation_precision(int p_simulation_precision) {
    PhysicsServer::get_singleton()->soft_body_set_simulation_precision(physics_rid, p_simulation_precision);
}

real_t SoftBody::get_total_mass() {
    return PhysicsServer::get_singleton()->soft_body_get_total_mass(physics_rid);
}

void SoftBody::set_total_mass(real_t p_total_mass) {
    PhysicsServer::get_singleton()->soft_body_set_total_mass(physics_rid, p_total_mass);
}

void SoftBody::set_linear_stiffness(real_t p_linear_stiffness) {
    PhysicsServer::get_singleton()->soft_body_set_linear_stiffness(physics_rid, p_linear_stiffness);
}

real_t SoftBody::get_linear_stiffness() {
    return PhysicsServer::get_singleton()->soft_body_get_linear_stiffness(physics_rid);
}

void SoftBody::set_areaAngular_stiffness(real_t p_areaAngular_stiffness) {
    PhysicsServer::get_singleton()->soft_body_set_areaAngular_stiffness(physics_rid, p_areaAngular_stiffness);
}

real_t SoftBody::get_areaAngular_stiffness() {
    return PhysicsServer::get_singleton()->soft_body_get_areaAngular_stiffness(physics_rid);
}

void SoftBody::set_volume_stiffness(real_t p_volume_stiffness) {
    PhysicsServer::get_singleton()->soft_body_set_volume_stiffness(physics_rid, p_volume_stiffness);
}

real_t SoftBody::get_volume_stiffness() {
    return PhysicsServer::get_singleton()->soft_body_get_volume_stiffness(physics_rid);
}

real_t SoftBody::get_pressure_coefficient() {
    return PhysicsServer::get_singleton()->soft_body_get_pressure_coefficient(physics_rid);
}

void SoftBody::set_pose_matching_coefficient(real_t p_pose_matching_coefficient) {
    PhysicsServer::get_singleton()->soft_body_set_pose_matching_coefficient(physics_rid, p_pose_matching_coefficient);
}

real_t SoftBody::get_pose_matching_coefficient() {
    return PhysicsServer::get_singleton()->soft_body_get_pose_matching_coefficient(physics_rid);
}

void SoftBody::set_pressure_coefficient(real_t p_pressure_coefficient) {
    PhysicsServer::get_singleton()->soft_body_set_pressure_coefficient(physics_rid, p_pressure_coefficient);
}

real_t SoftBody::get_damping_coefficient() {
    return PhysicsServer::get_singleton()->soft_body_get_damping_coefficient(physics_rid);
}

void SoftBody::set_damping_coefficient(real_t p_damping_coefficient) {
    PhysicsServer::get_singleton()->soft_body_set_damping_coefficient(physics_rid, p_damping_coefficient);
}

real_t SoftBody::get_drag_coefficient() {
    return PhysicsServer::get_singleton()->soft_body_get_drag_coefficient(physics_rid);
}

void SoftBody::set_drag_coefficient(real_t p_drag_coefficient) {
    PhysicsServer::get_singleton()->soft_body_set_drag_coefficient(physics_rid, p_drag_coefficient);
}

Vector3 SoftBody::get_point_transform(int p_point_index) {
    return PhysicsServer::get_singleton()->soft_body_get_point_global_position(physics_rid, p_point_index);
}

void SoftBody::pin_point_toggle(int p_point_index) {
    pin_point(p_point_index, !(-1 != _has_pinned_point(p_point_index)));
}

void SoftBody::pin_point(int p_point_index, bool pin, const NodePath &p_spatial_attachment_path) {
    _pin_point_on_physics_server(p_point_index, pin);
    if (pin) {
        _add_pinned_point(p_point_index, p_spatial_attachment_path);
    } else {
        _remove_pinned_point(p_point_index);
    }
}

bool SoftBody::is_point_pinned(int p_point_index) const {
    return -1 != _has_pinned_point(p_point_index);
}

void SoftBody::set_ray_pickable(bool p_ray_pickable) {

    ray_pickable = p_ray_pickable;
    _update_pickable();
}

bool SoftBody::is_ray_pickable() const {

    return ray_pickable;
}

SoftBody::SoftBody() :
        physics_rid(PhysicsServer::get_singleton()->soft_body_create()),
        mesh_owner(false),
        collision_mask(1),
        collision_layer(1),
        simulation_started(false),
        pinned_points_cache_dirty(true),
        ray_pickable(true) {

    PhysicsServer::get_singleton()->body_attach_object_instance_id(physics_rid, get_instance_id());
    //set_notify_transform(true);
    set_physics_process_internal(true);
}

SoftBody::~SoftBody() {
    PhysicsServer::get_singleton()->free_rid(physics_rid);
}

void SoftBody::reset_softbody_pin() {
    PhysicsServer::get_singleton()->soft_body_remove_all_pinned_points(physics_rid);
    PoolVector<PinnedPoint>::Read pps = pinned_points.read();
    for (int i = pinned_points.size() - 1; 0 < i; --i) {
        PhysicsServer::get_singleton()->soft_body_pin_point(physics_rid, pps[i].point_index, true);
    }
}

void SoftBody::_make_cache_dirty() {
    pinned_points_cache_dirty = true;
}

void SoftBody::_update_cache_pin_points_datas() {
    if (!pinned_points_cache_dirty)
        return;

    pinned_points_cache_dirty = false;

    PoolVector<PinnedPoint>::Write w = pinned_points.write();
    for (int i = pinned_points.size() - 1; 0 <= i; --i) {

        if (!w[i].spatial_attachment_path.is_empty()) {
            w[i].spatial_attachment = object_cast<Node3D>(get_node(w[i].spatial_attachment_path));
        }
        if (!w[i].spatial_attachment) {
            ERR_PRINT("Node3D node not defined in the pinned point, Softbody undefined behaviour!");
        }
    }
}

void SoftBody::_pin_point_on_physics_server(int p_point_index, bool pin) {
    PhysicsServer::get_singleton()->soft_body_pin_point(physics_rid, p_point_index, pin);
}

void SoftBody::_add_pinned_point(int p_point_index, const NodePath &p_spatial_attachment_path) {
    SoftBody::PinnedPoint *pinned_point;
    if (-1 == _get_pinned_point(p_point_index, pinned_point)) {

        // Create new
        PinnedPoint pp;
        pp.point_index = p_point_index;
        pp.spatial_attachment_path = p_spatial_attachment_path;

        if (!p_spatial_attachment_path.is_empty() && has_node(p_spatial_attachment_path)) {
            pp.spatial_attachment = object_cast<Node3D>(get_node(p_spatial_attachment_path));
            pp.offset = (pp.spatial_attachment->get_global_transform().affine_inverse() * get_global_transform()).xform(PhysicsServer::get_singleton()->soft_body_get_point_global_position(physics_rid, pp.point_index));
        }

        pinned_points.push_back(pp);

    } else {

        pinned_point->point_index = p_point_index;
        pinned_point->spatial_attachment_path = p_spatial_attachment_path;

        if (!p_spatial_attachment_path.is_empty() && has_node(p_spatial_attachment_path)) {
            pinned_point->spatial_attachment = object_cast<Node3D>(get_node(p_spatial_attachment_path));
            pinned_point->offset = (pinned_point->spatial_attachment->get_global_transform().affine_inverse() * get_global_transform()).xform(PhysicsServer::get_singleton()->soft_body_get_point_global_position(physics_rid, pinned_point->point_index));
        }
    }
}

void SoftBody::_reset_points_offsets() {

    if (!Engine::get_singleton()->is_editor_hint())
        return;

    PoolVector<PinnedPoint>::Read r = pinned_points.read();
    PoolVector<PinnedPoint>::Write w = pinned_points.write();
    for (int i = pinned_points.size() - 1; 0 <= i; --i) {

        if (!r[i].spatial_attachment)
            w[i].spatial_attachment = object_cast<Node3D>(get_node(r[i].spatial_attachment_path));

        if (!r[i].spatial_attachment)
            continue;

        w[i].offset = (r[i].spatial_attachment->get_global_transform().affine_inverse() * get_global_transform()).xform(PhysicsServer::get_singleton()->soft_body_get_point_global_position(physics_rid, r[i].point_index));
    }
}

void SoftBody::_remove_pinned_point(int p_point_index) {
    const int id(_has_pinned_point(p_point_index));
    if (-1 != id) {
        pinned_points.remove(id);
    }
}

int SoftBody::_get_pinned_point(int p_point_index, SoftBody::PinnedPoint *&r_point) const {
    const int id = _has_pinned_point(p_point_index);
    if (-1 == id) {
        r_point = nullptr;
        return -1;
    } else {
        r_point = const_cast<SoftBody::PinnedPoint *>(&pinned_points.read()[id]);
        return id;
    }
}

int SoftBody::_has_pinned_point(int p_point_index) const {
    PoolVector<PinnedPoint>::Read r = pinned_points.read();
    for (int i = pinned_points.size() - 1; 0 <= i; --i) {
        if (p_point_index == r[i].point_index) {
            return i;
        }
    }
    return -1;
}
