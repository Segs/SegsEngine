/*************************************************************************/
/*  soft_body_3d.cpp                                                     */
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

#include "soft_body_3d.h"


#include "core/callable_method_pointer.h"
#include "core/list.h"
#include "core/method_bind.h"
#include "core/object.h"
#include "core/object_db.h"
#include "core/os/os.h"
#include "core/rid.h"
#include "core/object_tooling.h"
#include "core/translation_helpers.h"
#include "scene/3d/collision_object_3d.h"
#include "scene/3d/physics_body_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/main/scene_tree.h"

#include "servers/physics_server_3d.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(SoftBody3D)

SoftBodyVisualServerHandler::SoftBodyVisualServerHandler() {}

void SoftBodyVisualServerHandler::prepare(RenderingEntity p_mesh, int p_surface) {
    clear();

    ERR_FAIL_COND(p_mesh==entt::null);

    mesh = p_mesh;
    surface = p_surface;

    const uint32_t surface_format = RenderingServer::get_singleton()->mesh_surface_get_format(mesh, surface);
    const int surface_vertex_len = RenderingServer::get_singleton()->mesh_surface_get_array_len(mesh, p_surface);
    const int surface_index_len = RenderingServer::get_singleton()->mesh_surface_get_array_index_len(mesh, p_surface);
    uint32_t surface_offsets[RS::ARRAY_MAX];
    uint32_t surface_strides[RS::ARRAY_MAX];

    buffer = RenderingServer::get_singleton()->mesh_surface_get_array(mesh, surface);
    RenderingServer::get_singleton()->mesh_surface_make_offsets_from_format(
            surface_format, surface_vertex_len, surface_index_len, surface_offsets, surface_strides);
    ERR_FAIL_COND(surface_strides[RS::ARRAY_VERTEX] != surface_strides[RS::ARRAY_NORMAL]);
    stride = surface_strides[RS::ARRAY_VERTEX];
    offset_vertices = surface_offsets[RS::ARRAY_VERTEX];
    offset_normal = surface_offsets[RS::ARRAY_NORMAL];
}

void SoftBodyVisualServerHandler::clear() {

    if (mesh != entt::null) {
        buffer.resize(0);
    }

    mesh = entt::null;
}

void SoftBodyVisualServerHandler::open() {
    write_buffer = buffer.write();
}

void SoftBodyVisualServerHandler::close() {
    write_buffer.release();
}

void SoftBodyVisualServerHandler::commit_changes() {
    RenderingServer::get_singleton()->mesh_surface_update_region(mesh, surface, 0, buffer);
}

void SoftBodyVisualServerHandler::set_vertex(int p_vertex_id, const void *p_vector3) {
    memcpy(&write_buffer[p_vertex_id * stride + offset_vertices], p_vector3, sizeof(float) * 3);
}

void SoftBodyVisualServerHandler::set_normal(int p_vertex_id, const void *p_vector3) {
    Vector2 normal_oct = RenderingServer::get_singleton()->norm_to_oct(*(Vector3 *)p_vector3);
    int16_t v_normal[2] = {
        CLAMP<int16_t>(normal_oct.x * 32767, -32768, 32767),
        CLAMP<int16_t>(normal_oct.y * 32767, -32768, 32767),
    };
    memcpy(&write_buffer[p_vertex_id * stride + offset_normal], v_normal, sizeof(uint16_t) * 2);
}

void SoftBodyVisualServerHandler::set_aabb(const AABB &p_aabb) {
    RenderingServer::get_singleton()->mesh_set_custom_aabb(mesh, p_aabb);
}

SoftBody3D::PinnedPoint::PinnedPoint() :
        point_index(-1),
        spatial_attachment(nullptr) {
}

SoftBody3D::PinnedPoint::PinnedPoint(const PinnedPoint &obj_tocopy) {
    point_index = obj_tocopy.point_index;
    spatial_attachment_path = obj_tocopy.spatial_attachment_path;
    spatial_attachment = obj_tocopy.spatial_attachment;
    offset = obj_tocopy.offset;
}

SoftBody3D::PinnedPoint SoftBody3D::PinnedPoint::operator=(const PinnedPoint &obj) {
    point_index = obj.point_index;
    spatial_attachment_path = obj.spatial_attachment_path;
    spatial_attachment = obj.spatial_attachment;
    offset = obj.offset;
    return *this;
}

void SoftBody3D::_update_pickable() {
    if (!is_inside_tree())
        return;
    bool pickable = ray_pickable && is_visible_in_tree();
    PhysicsServer3D::get_singleton()->soft_body_set_ray_pickable(physics_rid, pickable);
}

bool SoftBody3D::_set(const StringName &p_name, const Variant &p_value) {

    StringView which = StringUtils::get_slice(p_name,'/', 0);

    if (StringView("pinned_points") == which) {

        return _set_property_pinned_points_indices(p_value.as<Array>());

    } else if (StringView("attachments") == which) {

        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        StringView what = StringUtils::get_slice(p_name,'/', 2);

        return _set_property_pinned_points_attachment(idx, what, p_value);
    }

    return false;
}

bool SoftBody3D::_get(const StringName &p_name, Variant &r_ret) const {
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

void SoftBody3D::_get_property_list(Vector<PropertyInfo> *p_list) const {

    const int pinned_points_indices_size = pinned_points.size();

    p_list->push_back(PropertyInfo(VariantType::POOL_INT_ARRAY, "pinned_points"));

    for (int i = 0; i < pinned_points_indices_size; ++i) {
        p_list->push_back(PropertyInfo(VariantType::INT, StringName("attachments/" + itos(i) + "/point_index")));
        p_list->push_back(PropertyInfo(VariantType::NODE_PATH, StringName("attachments/" + itos(i) + "/spatial_attachment_path")));
        p_list->push_back(PropertyInfo(VariantType::VECTOR3, StringName("attachments/" + itos(i) + "/offset")));
    }
}

bool SoftBody3D::_set_property_pinned_points_indices(const Array &p_indices) {

    const int p_indices_size = p_indices.size();

    { // Remove the pined points on physics server that will be removed by resize
        PoolVector<PinnedPoint>::Read r = pinned_points.read();
        if (p_indices_size < pinned_points.size()) {
            for (int i = pinned_points.size() - 1; i >= p_indices_size; --i) {
                set_point_pinned(r[i].point_index, false);
            }
        }
    }

    pinned_points.resize(p_indices_size);

    PoolVector<PinnedPoint>::Write w = pinned_points.write();
    int point_index;
    for (int i = 0; i < p_indices_size; ++i) {
        point_index = p_indices.get(i).as<int>();
        if (w[i].point_index != point_index) {
            if (-1 != w[i].point_index)
                set_point_pinned(w[i].point_index, false);
            w[i].point_index = point_index;
            set_point_pinned(w[i].point_index, true);
        }
    }
    return true;
}

bool SoftBody3D::_set_property_pinned_points_attachment(int p_item, StringView p_what, const Variant &p_value) {
    if (pinned_points.size() <= p_item) {
        return false;
    }

    if (StringView("spatial_attachment_path") == p_what) {
        PoolVector<PinnedPoint>::Write w = pinned_points.write();
        set_point_pinned(w[p_item].point_index, true, p_value.as<NodePath>());
        _make_cache_dirty();
    } else if (StringView("offset") == p_what) {
        PoolVector<PinnedPoint>::Write w = pinned_points.write();
        w[p_item].offset = p_value.as<Vector3>();
    } else {
        return false;
    }

    return true;
}

bool SoftBody3D::_get_property_pinned_points(int p_item, StringView p_what, Variant &r_ret) const {
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

void SoftBody3D::_changed_callback(Object *p_changed, const StringName & p_prop) {
    _prepare_physics_server();
    _reset_points_offsets();
#ifdef TOOLS_ENABLED
    if (p_changed == this) {
        update_configuration_warning();
    }
#endif
}

void SoftBody3D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_WORLD: {

            if (Engine::get_singleton()->is_editor_hint()) {
                //TODO: SEGS: change receptor
                Object_add_change_receptor(this,this);
            }

            RID space = get_world_3d()->get_space();
            PhysicsServer3D::get_singleton()->soft_body_set_space(physics_rid, space);
            _prepare_physics_server();
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

            PhysicsServer3D::get_singleton()->soft_body_set_transform(physics_rid, get_global_transform());

            set_notify_transform(false);
            // Required to be top level with Transform at center of world in order to modify RenderingServer only to support custom Transform
            set_as_top_level(true);
            set_transform(Transform());
            set_notify_transform(true);

        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {

            _update_pickable();

        } break;
        case NOTIFICATION_EXIT_WORLD: {

            PhysicsServer3D::get_singleton()->soft_body_set_space(physics_rid, RID());

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

void SoftBody3D::_bind_methods() {

    SE_BIND_METHOD(SoftBody3D,_draw_soft_mesh);
    SE_BIND_METHOD(SoftBody3D,set_physics_enabled);
    SE_BIND_METHOD(SoftBody3D,is_physics_enabled);

    SE_BIND_METHOD(SoftBody3D,set_collision_mask);
    SE_BIND_METHOD(SoftBody3D,get_collision_mask);

    SE_BIND_METHOD(SoftBody3D,set_collision_layer);
    SE_BIND_METHOD(SoftBody3D,get_collision_layer);

    SE_BIND_METHOD(SoftBody3D,set_collision_mask_bit);
    SE_BIND_METHOD(SoftBody3D,get_collision_mask_bit);

    SE_BIND_METHOD(SoftBody3D,set_collision_layer_bit);
    SE_BIND_METHOD(SoftBody3D,get_collision_layer_bit);

    SE_BIND_METHOD(SoftBody3D,set_parent_collision_ignore);
    SE_BIND_METHOD(SoftBody3D,get_parent_collision_ignore);

    SE_BIND_METHOD(SoftBody3D,get_collision_exceptions);
    SE_BIND_METHOD(SoftBody3D,add_collision_exception_with);
    SE_BIND_METHOD(SoftBody3D,remove_collision_exception_with);

    SE_BIND_METHOD(SoftBody3D,set_simulation_precision);
    SE_BIND_METHOD(SoftBody3D,get_simulation_precision);

    SE_BIND_METHOD(SoftBody3D,set_total_mass);
    SE_BIND_METHOD(SoftBody3D,get_total_mass);

    SE_BIND_METHOD(SoftBody3D,set_linear_stiffness);
    SE_BIND_METHOD(SoftBody3D,get_linear_stiffness);

    SE_BIND_METHOD(SoftBody3D,set_areaAngular_stiffness);
    SE_BIND_METHOD(SoftBody3D,get_areaAngular_stiffness);

    SE_BIND_METHOD(SoftBody3D,set_volume_stiffness);
    SE_BIND_METHOD(SoftBody3D,get_volume_stiffness);

    SE_BIND_METHOD(SoftBody3D,set_pressure_coefficient);
    SE_BIND_METHOD(SoftBody3D,get_pressure_coefficient);

    SE_BIND_METHOD(SoftBody3D,set_pose_matching_coefficient);
    SE_BIND_METHOD(SoftBody3D,get_pose_matching_coefficient);

    SE_BIND_METHOD(SoftBody3D,set_damping_coefficient);
    SE_BIND_METHOD(SoftBody3D,get_damping_coefficient);

    SE_BIND_METHOD(SoftBody3D,set_drag_coefficient);
    SE_BIND_METHOD(SoftBody3D,get_drag_coefficient);
    SE_BIND_METHOD(SoftBody3D,get_point_transform);

    MethodBinder::bind_method(D_METHOD("set_point_pinned", {"point_index", "pinned", "attachment_path"}), &SoftBody3D::set_point_pinned, {DEFVAL(NodePath())});
    SE_BIND_METHOD(SoftBody3D,is_point_pinned);

    SE_BIND_METHOD(SoftBody3D,set_ray_pickable);
    SE_BIND_METHOD(SoftBody3D,is_ray_pickable);
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "physics_enabled"), "set_physics_enabled", "is_physics_enabled");

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

String SoftBody3D::get_configuration_warning() const {

    String warning(MeshInstance3D::get_configuration_warning());

    if (!mesh) {
        if (!warning.empty())
            warning += "\n\n";

        warning += TTR("This body will be ignored until you set a mesh.");
    }

    Transform t = get_transform();
    if ((ABS(t.basis.get_axis(0).length() - 1.0f) > 0.05f || ABS(t.basis.get_axis(1).length() - 1.0f) > 0.05f ||
                ABS(t.basis.get_axis(2).length() - 1.0f) > 0.05f)) {
        if (!warning.empty()) warning += ("\n\n");

        warning += TTR("Size changes to SoftBody3D will be overridden by the physics engine when running.\nChange the size "
                       "in children collision shapes instead.");
    }

    return warning;
}

void SoftBody3D::_draw_soft_mesh() {
    if (!mesh) {
        return;
    }

    RenderingEntity mesh_rid = mesh->get_rid();
    if (owned_mesh != mesh_rid) {
        _become_mesh_owner();
        mesh_rid = mesh->get_rid();
        PhysicsServer3D::get_singleton()->soft_body_set_mesh(physics_rid, mesh);
    }

    if (!rendering_server_handler.is_ready(mesh_rid)) {
        rendering_server_handler.prepare(mesh_rid, 0);

        /// Necessary in order to render the mesh correctly (Soft body nodes are in global space)
        simulation_started = true;
        call_deferred([this]() {
            set_as_top_level(true);
            set_transform(Transform());
        });
    }

    _update_physics_server();

    rendering_server_handler.open();
    PhysicsServer3D::get_singleton()->soft_body_update_rendering_server(physics_rid, &rendering_server_handler);
    rendering_server_handler.close();

    rendering_server_handler.commit_changes();
}

void SoftBody3D::_update_physics_server() {
    if (!simulation_started) {
        return;
    }

    _update_cache_pin_points_datas();
    // Submit bone attachment
    const int pinned_points_indices_size = pinned_points.size();
    PoolVector<PinnedPoint>::Read r = pinned_points.read();
    for (int i = 0; i < pinned_points_indices_size; ++i) {
        if (r[i].spatial_attachment) {
            PhysicsServer3D::get_singleton()->soft_body_move_point(physics_rid, r[i].point_index, r[i].spatial_attachment->get_global_transform().xform(r[i].offset));
        }
    }
}

void SoftBody3D::_prepare_physics_server() {

    if (Engine::get_singleton()->is_editor_hint()) {

        if (mesh)
            PhysicsServer3D::get_singleton()->soft_body_set_mesh(physics_rid, mesh);
        else
            PhysicsServer3D::get_singleton()->soft_body_set_mesh(physics_rid, REF());

        return;
    }
    auto RS = RenderingServer::get_singleton();
    if (mesh && physics_enabled) {
        if (owned_mesh != mesh->get_rid()) {
            _become_mesh_owner();
        }
        PhysicsServer3D::get_singleton()->soft_body_set_mesh(physics_rid, mesh);
        RS->connect("frame_pre_draw",callable_mp(this, &ClassName::_draw_soft_mesh));
    } else {

        PhysicsServer3D::get_singleton()->soft_body_set_mesh(physics_rid, REF());
        if(RS->is_connected("frame_pre_draw",callable_mp(this, &ClassName::_draw_soft_mesh))) {
            RS->disconnect("frame_pre_draw",callable_mp(this, &ClassName::_draw_soft_mesh));
        }
    }
}

void SoftBody3D::_become_mesh_owner() {

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
    owned_mesh = soft_mesh->get_rid();
}

void SoftBody3D::set_collision_mask(uint32_t p_mask) {
    collision_mask = p_mask;
    PhysicsServer3D::get_singleton()->soft_body_set_collision_mask(physics_rid, p_mask);
}

uint32_t SoftBody3D::get_collision_mask() const {
    return collision_mask;
}
void SoftBody3D::set_collision_layer(uint32_t p_layer) {
    collision_layer = p_layer;
    PhysicsServer3D::get_singleton()->soft_body_set_collision_layer(physics_rid, p_layer);
}

uint32_t SoftBody3D::get_collision_layer() const {
    return collision_layer;
}

void SoftBody3D::set_collision_mask_bit(int p_bit, bool p_value) {
    ERR_FAIL_INDEX_MSG(p_bit, 32, "Collision mask bit must be between 0 and 31 inclusive.");
    uint32_t mask = get_collision_mask();
    if (p_value)
        mask |= 1 << p_bit;
    else
        mask &= ~(1 << p_bit);
    set_collision_mask(mask);
}

bool SoftBody3D::get_collision_mask_bit(int p_bit) const {
    ERR_FAIL_INDEX_V_MSG(p_bit, 32, false, "Collision mask bit must be between 0 and 31 inclusive.");
    return get_collision_mask() & (1 << p_bit);
}

void SoftBody3D::set_collision_layer_bit(int p_bit, bool p_value) {
    ERR_FAIL_INDEX_MSG(p_bit, 32, "Collision layer bit must be between 0 and 31 inclusive.");
    uint32_t layer = get_collision_layer();
    if (p_value)
        layer |= 1 << p_bit;
    else
        layer &= ~(1 << p_bit);
    set_collision_layer(layer);
}

bool SoftBody3D::get_collision_layer_bit(int p_bit) const {
    ERR_FAIL_INDEX_V_MSG(p_bit, 32, false, "Collision layer bit must be between 0 and 31 inclusive.");
    return get_collision_layer() & (1 << p_bit);
}

void SoftBody3D::set_parent_collision_ignore(const NodePath &p_parent_collision_ignore) {
    parent_collision_ignore = p_parent_collision_ignore;
}

const NodePath &SoftBody3D::get_parent_collision_ignore() const {
    return parent_collision_ignore;
}

void SoftBody3D::set_physics_enabled(bool p_enabled) {
    if (p_enabled == physics_enabled) {
        return;
    }

    physics_enabled = p_enabled;

    if (is_inside_tree()) {
        _prepare_physics_server();
    }
}

bool SoftBody3D::is_physics_enabled() const {
    return physics_enabled;
}
void SoftBody3D::set_pinned_points_indices(const PoolVector<SoftBody3D::PinnedPoint>& p_pinned_points_indices) {
    pinned_points = p_pinned_points_indices;
    PoolVector<PinnedPoint>::Read w = pinned_points.read();
    for (int i = pinned_points.size() - 1; 0 <= i; --i) {
        set_point_pinned(p_pinned_points_indices[i].point_index, true);
    }
}

PoolVector<SoftBody3D::PinnedPoint> SoftBody3D::get_pinned_points_indices() {
    return pinned_points;
}

Array SoftBody3D::get_collision_exceptions() {
    Vector<RID> exceptions;
    PhysicsServer3D::get_singleton()->soft_body_get_collision_exceptions(physics_rid, &exceptions);
    Array ret;
    for (RID body : exceptions) {
        GameEntity instance_id = PhysicsServer3D::get_singleton()->body_get_object_instance_id(body);
        Object *obj = object_for_entity(instance_id);
        PhysicsBody3D *physics_body = object_cast<PhysicsBody3D>(obj);
        ret.append(Variant(physics_body));
    }
    return ret;
}

void SoftBody3D::add_collision_exception_with(Node *p_node) {
    ERR_FAIL_NULL(p_node);
    CollisionObject3D *collision_object = object_cast<CollisionObject3D>(p_node);
    ERR_FAIL_COND_MSG(!collision_object, "Collision exception only works between two CollisionObject3Ds.");
    PhysicsServer3D::get_singleton()->soft_body_add_collision_exception(physics_rid, collision_object->get_rid());
}

void SoftBody3D::remove_collision_exception_with(Node *p_node) {
    ERR_FAIL_NULL(p_node);
    CollisionObject3D *collision_object = object_cast<CollisionObject3D>(p_node);
    ERR_FAIL_COND_MSG(!collision_object, "Collision exception only works between two CollisionObject3Ds.");
    PhysicsServer3D::get_singleton()->soft_body_remove_collision_exception(physics_rid, collision_object->get_rid());
}

int SoftBody3D::get_simulation_precision() {
    return PhysicsServer3D::get_singleton()->soft_body_get_simulation_precision(physics_rid);
}

void SoftBody3D::set_simulation_precision(int p_simulation_precision) {
    PhysicsServer3D::get_singleton()->soft_body_set_simulation_precision(physics_rid, p_simulation_precision);
}

real_t SoftBody3D::get_total_mass() {
    return PhysicsServer3D::get_singleton()->soft_body_get_total_mass(physics_rid);
}

void SoftBody3D::set_total_mass(real_t p_total_mass) {
    PhysicsServer3D::get_singleton()->soft_body_set_total_mass(physics_rid, p_total_mass);
}

void SoftBody3D::set_linear_stiffness(real_t p_linear_stiffness) {
    PhysicsServer3D::get_singleton()->soft_body_set_linear_stiffness(physics_rid, p_linear_stiffness);
}

real_t SoftBody3D::get_linear_stiffness() {
    return PhysicsServer3D::get_singleton()->soft_body_get_linear_stiffness(physics_rid);
}

void SoftBody3D::set_areaAngular_stiffness(real_t p_areaAngular_stiffness) {
    PhysicsServer3D::get_singleton()->soft_body_set_areaAngular_stiffness(physics_rid, p_areaAngular_stiffness);
}

real_t SoftBody3D::get_areaAngular_stiffness() {
    return PhysicsServer3D::get_singleton()->soft_body_get_areaAngular_stiffness(physics_rid);
}

void SoftBody3D::set_volume_stiffness(real_t p_volume_stiffness) {
    PhysicsServer3D::get_singleton()->soft_body_set_volume_stiffness(physics_rid, p_volume_stiffness);
}

real_t SoftBody3D::get_volume_stiffness() {
    return PhysicsServer3D::get_singleton()->soft_body_get_volume_stiffness(physics_rid);
}

real_t SoftBody3D::get_pressure_coefficient() {
    return PhysicsServer3D::get_singleton()->soft_body_get_pressure_coefficient(physics_rid);
}

void SoftBody3D::set_pose_matching_coefficient(real_t p_pose_matching_coefficient) {
    PhysicsServer3D::get_singleton()->soft_body_set_pose_matching_coefficient(physics_rid, p_pose_matching_coefficient);
}

real_t SoftBody3D::get_pose_matching_coefficient() {
    return PhysicsServer3D::get_singleton()->soft_body_get_pose_matching_coefficient(physics_rid);
}

void SoftBody3D::set_pressure_coefficient(real_t p_pressure_coefficient) {
    PhysicsServer3D::get_singleton()->soft_body_set_pressure_coefficient(physics_rid, p_pressure_coefficient);
}

real_t SoftBody3D::get_damping_coefficient() {
    return PhysicsServer3D::get_singleton()->soft_body_get_damping_coefficient(physics_rid);
}

void SoftBody3D::set_damping_coefficient(real_t p_damping_coefficient) {
    PhysicsServer3D::get_singleton()->soft_body_set_damping_coefficient(physics_rid, p_damping_coefficient);
}

real_t SoftBody3D::get_drag_coefficient() {
    return PhysicsServer3D::get_singleton()->soft_body_get_drag_coefficient(physics_rid);
}

void SoftBody3D::set_drag_coefficient(real_t p_drag_coefficient) {
    PhysicsServer3D::get_singleton()->soft_body_set_drag_coefficient(physics_rid, p_drag_coefficient);
}

Vector3 SoftBody3D::get_point_transform(int p_point_index) {
    return PhysicsServer3D::get_singleton()->soft_body_get_point_global_position(physics_rid, p_point_index);
}

void SoftBody3D::pin_point_toggle(int p_point_index) {
    set_point_pinned(p_point_index, !(-1 != _has_pinned_point(p_point_index)));
}

void SoftBody3D::set_point_pinned(int p_point_index, bool pin, const NodePath &p_spatial_attachment_path) {
    _pin_point_on_physics_server(p_point_index, pin);
    if (pin) {
        _add_pinned_point(p_point_index, p_spatial_attachment_path);
    } else {
        _remove_pinned_point(p_point_index);
    }
}

bool SoftBody3D::is_point_pinned(int p_point_index) const {
    return -1 != _has_pinned_point(p_point_index);
}

void SoftBody3D::set_ray_pickable(bool p_ray_pickable) {

    ray_pickable = p_ray_pickable;
    _update_pickable();
}

bool SoftBody3D::is_ray_pickable() const {

    return ray_pickable;
}

SoftBody3D::SoftBody3D() :
        physics_rid(PhysicsServer3D::get_singleton()->soft_body_create()),
        collision_mask(1),
        collision_layer(1),
        simulation_started(false),
        pinned_points_cache_dirty(true),
        ray_pickable(true) {

    PhysicsServer3D::get_singleton()->body_attach_object_instance_id(physics_rid, get_instance_id());
}

SoftBody3D::~SoftBody3D() {
    PhysicsServer3D::get_singleton()->free_rid(physics_rid);
}

void SoftBody3D::reset_softbody_pin() {
    PhysicsServer3D::get_singleton()->soft_body_remove_all_pinned_points(physics_rid);
    PoolVector<PinnedPoint>::Read pps = pinned_points.read();
    for (int i = pinned_points.size() - 1; 0 < i; --i) {
        PhysicsServer3D::get_singleton()->soft_body_pin_point(physics_rid, pps[i].point_index, true);
    }
}

void SoftBody3D::_make_cache_dirty() {
    pinned_points_cache_dirty = true;
}

void SoftBody3D::_update_cache_pin_points_datas() {
    if (!pinned_points_cache_dirty)
        return;

    pinned_points_cache_dirty = false;

    PoolVector<PinnedPoint>::Write w = pinned_points.write();
    for (int i = pinned_points.size() - 1; 0 <= i; --i) {

        if (!w[i].spatial_attachment_path.is_empty()) {
            w[i].spatial_attachment = object_cast<Node3D>(get_node(w[i].spatial_attachment_path));
        }
        if (!w[i].spatial_attachment) {
            ERR_PRINT("Spatial node not defined in the pinned point, this is undefined behavior for SoftBody!");
        }
    }
}

void SoftBody3D::_pin_point_on_physics_server(int p_point_index, bool pin) {
    PhysicsServer3D::get_singleton()->soft_body_pin_point(physics_rid, p_point_index, pin);
}

void SoftBody3D::_add_pinned_point(int p_point_index, const NodePath &p_spatial_attachment_path) {
    SoftBody3D::PinnedPoint *pinned_point;
    if (-1 == _get_pinned_point(p_point_index, pinned_point)) {

        // Create new
        PinnedPoint pp;
        pp.point_index = p_point_index;
        pp.spatial_attachment_path = p_spatial_attachment_path;

        if (!p_spatial_attachment_path.is_empty() && has_node(p_spatial_attachment_path)) {
            pp.spatial_attachment = object_cast<Node3D>(get_node(p_spatial_attachment_path));
            pp.offset = (pp.spatial_attachment->get_global_transform().affine_inverse() * get_global_transform()).xform(PhysicsServer3D::get_singleton()->soft_body_get_point_global_position(physics_rid, pp.point_index));
        }

        pinned_points.push_back(pp);

    } else {

        pinned_point->point_index = p_point_index;
        pinned_point->spatial_attachment_path = p_spatial_attachment_path;

        if (!p_spatial_attachment_path.is_empty() && has_node(p_spatial_attachment_path)) {
            pinned_point->spatial_attachment = object_cast<Node3D>(get_node(p_spatial_attachment_path));
            pinned_point->offset = (pinned_point->spatial_attachment->get_global_transform().affine_inverse() * get_global_transform()).xform(PhysicsServer3D::get_singleton()->soft_body_get_point_global_position(physics_rid, pinned_point->point_index));
        }
    }
}

void SoftBody3D::_reset_points_offsets() {
    if (!Engine::get_singleton()->is_editor_hint())
        return;

    PoolVector<PinnedPoint>::Read r = pinned_points.read();
    PoolVector<PinnedPoint>::Write w = pinned_points.write();
    for (int i = pinned_points.size() - 1; 0 <= i; --i) {
        if (!r[i].spatial_attachment) {
            if (!r[i].spatial_attachment_path.is_empty() && has_node(r[i].spatial_attachment_path)) {
                w[i].spatial_attachment = object_cast<Node3D>(get_node(r[i].spatial_attachment_path));
            }
        }

        if (!r[i].spatial_attachment)
            continue;

        w[i].offset = (r[i].spatial_attachment->get_global_transform().affine_inverse() * get_global_transform())
                              .xform(PhysicsServer3D::get_singleton()->soft_body_get_point_global_position(physics_rid, r[i].point_index));
    }
}

void SoftBody3D::_remove_pinned_point(int p_point_index) {
    const int id(_has_pinned_point(p_point_index));
    if (-1 != id) {
        pinned_points.remove(id);
    }
}

int SoftBody3D::_get_pinned_point(int p_point_index, SoftBody3D::PinnedPoint *&r_point) const {
    const int id = _has_pinned_point(p_point_index);
    if (-1 == id) {
        r_point = nullptr;
        return -1;
    } else {
        r_point = const_cast<SoftBody3D::PinnedPoint *>(&pinned_points.read()[id]);
        return id;
    }
}

int SoftBody3D::_has_pinned_point(int p_point_index) const {
    PoolVector<PinnedPoint>::Read r = pinned_points.read();
    for (int i = pinned_points.size() - 1; 0 <= i; --i) {
        if (p_point_index == r[i].point_index) {
            return i;
        }
    }
    return -1;
}
