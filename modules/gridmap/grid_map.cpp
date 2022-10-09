/*************************************************************************/
/*  grid_map.cpp                                                         */
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

#include "grid_map.h"

#include "core/io/marshalls.h"
#include "core/pair.h"
#include "core/object_tooling.h"
#include "core/message_queue.h"
#include "core/method_bind.h"
#include "core/dictionary.h"
#include "core/string.h"
#include "scene/3d/light_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/mesh_library.h"
#include "scene/resources/surface_tool.h"
#include "scene/resources/physics_material.h"
#include "scene/scene_string_names.h"
#include "servers/navigation_server.h"
#include "scene/main/scene_tree.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(GridMap)

using namespace eastl;
bool GridMap::_set(const StringName &p_name, const Variant &p_value) {

    StringView name(p_name);

    if (name == "data"_sv) {

        Dictionary d = p_value.as<Dictionary>();

        if (d.has("cells")) {

            PoolVector<int> cells = d["cells"].as<PoolVector<int>>();
            int amount = cells.size();
            PoolVector<int>::Read r = cells.read();
            ERR_FAIL_COND_V(amount % 3, false); // not even
            cell_map.clear();
            for (int i = 0; i < amount / 3; i++) {

                IndexKey ik;
                ik.key = decode_uint64((const uint8_t *)&r[i * 3]);
                Cell cell;
                cell.cell = decode_uint32((const uint8_t *)&r[i * 3 + 2]);
                cell_map[ik] = cell;
            }
        }

        _recreate_octant_data();

    } else if (name == "baked_meshes"_sv) {

        clear_baked_meshes();

        Array meshes = p_value.as<Array>();

        for (int i = 0; i < meshes.size(); i++) {
            BakedMesh bm;
            bm.mesh = refFromVariant<Mesh>(meshes[i]);
            ERR_CONTINUE(not bm.mesh);
            auto vserver=RenderingServer::get_singleton();
            bm.instance = RID_PRIME(vserver->instance_create());
            vserver->get_singleton()->instance_set_base(bm.instance, bm.mesh->get_rid());
            vserver->instance_attach_object_instance_id(bm.instance, get_instance_id());
            if (is_inside_tree()) {
                vserver->instance_set_scenario(bm.instance, get_world_3d()->get_scenario());
                vserver->instance_set_transform(bm.instance, get_global_transform());
            }
            baked_meshes.push_back(bm);
        }

        _recreate_octant_data();

    } else {
        return false;
    }

    return true;
}

bool GridMap::_get(const StringName &p_name, Variant &r_ret) const {

    StringView name(p_name);

    if (name == "data"_sv) {

        Dictionary d;

        PoolVector<int> cells;
        cells.resize(cell_map.size() * 3);
        {
            PoolVector<int>::Write w = cells.write();
            int i = -1;
            for (const eastl::pair<const IndexKey,Cell> &E : cell_map) {
                i++;
                encode_uint64(E.first.key, (uint8_t *)&w[i * 3]);
                encode_uint32(E.second.cell, (uint8_t *)&w[i * 3 + 2]);
            }
        }

        d["cells"] = cells;

        r_ret = d;
    } else if (name == "baked_meshes"_sv) {

        Array ret;
        ret.resize(baked_meshes.size());
        for (int i = 0; i < baked_meshes.size(); i++) {
            ret[i] = baked_meshes[i].mesh;
        }
        r_ret = ret;

    } else
        return false;

    return true;
}

void GridMap::_get_property_list(Vector<PropertyInfo> *p_list) const {

    if (!baked_meshes.empty()) {
        p_list->emplace_back(VariantType::ARRAY, "baked_meshes", PropertyHint::None, "", PROPERTY_USAGE_STORAGE);
    }

    p_list->emplace_back(VariantType::DICTIONARY, "data", PropertyHint::None, "", PROPERTY_USAGE_STORAGE);
}

void GridMap::set_collision_layer(uint32_t p_layer) {

    collision_layer = p_layer;
    _reset_physic_bodies_collision_filters();
}

uint32_t GridMap::get_collision_layer() const {

    return collision_layer;
}

void GridMap::set_collision_mask(uint32_t p_mask) {

    collision_mask = p_mask;
    _reset_physic_bodies_collision_filters();
}

uint32_t GridMap::get_collision_mask() const {

    return collision_mask;
}

void GridMap::set_collision_mask_bit(int p_bit, bool p_value) {

    ERR_FAIL_INDEX_MSG(p_bit, 32, "Collision mask bit must be between 0 and 31 inclusive.");
    uint32_t mask = get_collision_mask();
    if (p_value)
        mask |= 1 << p_bit;
    else
        mask &= ~(1 << p_bit);
    set_collision_mask(mask);
}

bool GridMap::get_collision_mask_bit(int p_bit) const {

    ERR_FAIL_INDEX_V_MSG(p_bit, 32,false, "Collision mask bit must be between 0 and 31 inclusive.");
    return get_collision_mask() & (1 << p_bit);
}

void GridMap::set_collision_layer_bit(int p_bit, bool p_value) {

    ERR_FAIL_INDEX_MSG(p_bit, 32, "Collision layer bit must be between 0 and 31 inclusive.");
    uint32_t layer = get_collision_layer();
    if (p_value) {
        layer |= 1 << p_bit;
    } else {
        layer &= ~(1 << p_bit);
    }
    set_collision_layer(layer);
}

bool GridMap::get_collision_layer_bit(int p_bit) const {
    ERR_FAIL_INDEX_V_MSG(p_bit, 32, false, "Collision layer bit must be between 0 and 31 inclusive.");

    return get_collision_layer() & (1 << p_bit);
}

void GridMap::set_physics_material(Ref<PhysicsMaterial> p_material) {
    physics_material = p_material;
    _recreate_octant_data();
}

Ref<PhysicsMaterial> GridMap::get_physics_material() const {
    return physics_material;
}

Array GridMap::get_collision_shapes() const {
    Array shapes;
    for (const eastl::pair<const OctantKey, Octant *> &E : octant_map) {
        Octant *g = E.second;
        RID body = g->static_body;
        Transform body_xform = PhysicsServer3D::get_singleton()
                                       ->body_get_state(body, PhysicsServer3D::BODY_STATE_TRANSFORM)
                                       .as<Transform>();
        int nshapes = PhysicsServer3D::get_singleton()->body_get_shape_count(body);
        for (int i = 0; i < nshapes; i++) {
            RID shape = PhysicsServer3D::get_singleton()->body_get_shape(body, i);
            Transform xform = PhysicsServer3D::get_singleton()->body_get_shape_transform(body, i);
            shapes.push_back(body_xform * xform);
            shapes.push_back(shape);
        }
    }

    return shapes;
}

Vector<CollisionShapeAndTransform> GridMap::get_collision_shapes_ex() const
{
    Vector<CollisionShapeAndTransform> shapes;
    for (const eastl::pair<const OctantKey, Octant *> &E : octant_map) {
        Octant *g = E.second;
        RID body = g->static_body;
        Transform body_xform = PhysicsServer3D::get_singleton()
                                       ->body_get_state(body, PhysicsServer3D::BODY_STATE_TRANSFORM)
                                       .as<Transform>();
        int nshapes = PhysicsServer3D::get_singleton()->body_get_shape_count(body);
        for (int i = 0; i < nshapes; i++) {
            RID shape = PhysicsServer3D::get_singleton()->body_get_shape(body, i);
            Transform xform = PhysicsServer3D::get_singleton()->body_get_shape_transform(body, i);
            shapes.emplace_back(shape,body_xform * xform);
        }
    }
    return shapes;
}
void GridMap::set_mesh_library(const Ref<MeshLibrary> &p_mesh_library) {

    if (mesh_library)
        mesh_library->unregister_owner(this);
    mesh_library = p_mesh_library;
    if (mesh_library)
        mesh_library->register_owner(this);

    _recreate_octant_data();
    Object_change_notify(this,"mesh_library");
}

Ref<MeshLibrary> GridMap::get_mesh_library() const {

    return mesh_library;
}

void GridMap::set_use_in_baked_light(bool p_use_baked_light) {
    use_in_baked_light = p_use_baked_light;
}

bool GridMap::get_use_in_baked_light() const {
    return use_in_baked_light;
}

void GridMap::set_cell_size(const Vector3 &p_size) {

    ERR_FAIL_COND(p_size.x < 0.001f || p_size.y < 0.001f || p_size.z < 0.001f);
    cell_size = p_size;
    _recreate_octant_data();
    emit_signal("cell_size_changed", cell_size);
}
Vector3 GridMap::get_cell_size() const {

    return cell_size;
}

void GridMap::set_octant_size(int p_size) {

    ERR_FAIL_COND(p_size == 0);
    octant_size = p_size;
    _recreate_octant_data();
}
int GridMap::get_octant_size() const {

    return octant_size;
}

void GridMap::set_center_x(bool p_enable) {

    center_x = p_enable;
    _recreate_octant_data();
}

bool GridMap::get_center_x() const {
    return center_x;
}

void GridMap::set_center_y(bool p_enable) {

    center_y = p_enable;
    _recreate_octant_data();
}

bool GridMap::get_center_y() const {
    return center_y;
}

void GridMap::set_center_z(bool p_enable) {

    center_z = p_enable;
    _recreate_octant_data();
}

bool GridMap::get_center_z() const {
    return center_z;
}

void GridMap::set_cell_item(int p_x, int p_y, int p_z, int p_item, int p_rot) {

    if (!baked_meshes.empty() && !recreating_octants) {
        //if you set a cell item, baked meshes go good bye
        clear_baked_meshes();
        _recreate_octant_data();
    }

    ERR_FAIL_INDEX(ABS(p_x), 1 << 20);
    ERR_FAIL_INDEX(ABS(p_y), 1 << 20);
    ERR_FAIL_INDEX(ABS(p_z), 1 << 20);

    IndexKey key;
    key.x = p_x;
    key.y = p_y;
    key.z = p_z;

    OctantKey ok;
    ok.x = p_x / octant_size;
    ok.y = p_y / octant_size;
    ok.z = p_z / octant_size;

    if (p_item < 0) {
        //erase
        if (cell_map.contains(key)) {
            OctantKey octantkey = ok;

            ERR_FAIL_COND(!octant_map.contains(octantkey));
            Octant &g = *octant_map[octantkey];
            g.cells.erase(key);
            g.dirty = true;
            cell_map.erase(key);
            _queue_octants_dirty();
        }
        return;
    }

    OctantKey octantkey = ok;

    if (!octant_map.contains(octantkey)) {
        //create octant because it does not exist
        Octant *g = memnew(Octant);
        g->dirty = true;
        PhysicsServer3D * phys_server = PhysicsServer3D::get_singleton();

        g->static_body = phys_server->body_create(PhysicsServer3D::BODY_MODE_STATIC);
        phys_server->body_attach_object_instance_id(g->static_body, get_instance_id());
        phys_server->body_set_collision_layer(g->static_body, collision_layer);
        phys_server->body_set_collision_mask(g->static_body, collision_mask);
        if (physics_material) {
            phys_server->body_set_param(
                    g->static_body, PhysicsServer3D::BODY_PARAM_FRICTION, physics_material->get_friction());
            phys_server->body_set_param(
                    g->static_body, PhysicsServer3D::BODY_PARAM_BOUNCE, physics_material->get_bounce());
        }
        SceneTree *st = SceneTree::get_singleton();

        if (st && st->is_debugging_collisions_hint()) {

            g->collision_debug = RenderingServer::get_singleton()->mesh_create();
            g->collision_debug_instance = RenderingServer::get_singleton()->instance_create();
            RenderingServer::get_singleton()->instance_set_base(g->collision_debug_instance, g->collision_debug);
        }

        octant_map[octantkey] = g;

        if (is_inside_world()) {
            _octant_enter_world(octantkey);
            _octant_transform(octantkey);
        }
    }

    Octant &g = *octant_map[octantkey];
    g.cells.insert(key);
    g.dirty = true;
    _queue_octants_dirty();

    Cell c;
    c.item = p_item;
    c.rot = p_rot;

    cell_map[key] = c;
}

int GridMap::get_cell_item(int p_x, int p_y, int p_z) const {

    ERR_FAIL_INDEX_V(ABS(p_x), 1 << 20, INVALID_CELL_ITEM);
    ERR_FAIL_INDEX_V(ABS(p_y), 1 << 20, INVALID_CELL_ITEM);
    ERR_FAIL_INDEX_V(ABS(p_z), 1 << 20, INVALID_CELL_ITEM);

    IndexKey key;
    key.x = p_x;
    key.y = p_y;
    key.z = p_z;

    if (!cell_map.contains(key))
        return INVALID_CELL_ITEM;
    return cell_map.at(key).item;
}

int GridMap::get_cell_item_orientation(int p_x, int p_y, int p_z) const {

    ERR_FAIL_INDEX_V(ABS(p_x), 1 << 20, -1);
    ERR_FAIL_INDEX_V(ABS(p_y), 1 << 20, -1);
    ERR_FAIL_INDEX_V(ABS(p_z), 1 << 20, -1);

    IndexKey key;
    key.x = p_x;
    key.y = p_y;
    key.z = p_z;

    if (!cell_map.contains(key))
        return -1;
    return cell_map.at(key).rot;
}

Vector3 GridMap::world_to_map(const Vector3 &p_world_pos) const {
    Vector3 map_pos = p_world_pos / cell_size;
    map_pos.x = floor(map_pos.x);
    map_pos.y = floor(map_pos.y);
    map_pos.z = floor(map_pos.z);
    return map_pos;
}

Vector3 GridMap::map_to_world(int p_x, int p_y, int p_z) const {
    Vector3 offset = _get_offset();
    Vector3 world_pos(
            p_x * cell_size.x + offset.x,
            p_y * cell_size.y + offset.y,
            p_z * cell_size.z + offset.z);
    return world_pos;
}

void GridMap::_octant_transform(const OctantKey &p_key) {

    ERR_FAIL_COND(!octant_map.contains(p_key));
    Octant &g = *octant_map[p_key];
    PhysicsServer3D::get_singleton()->body_set_state(g.static_body, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());

    if (g.collision_debug_instance!=entt::null) {
        RenderingServer::get_singleton()->instance_set_transform(g.collision_debug_instance, get_global_transform());
    }

    for (int i = 0; i < g.multimesh_instances.size(); i++) {
        RenderingServer::get_singleton()->instance_set_transform(g.multimesh_instances[i].instance, get_global_transform());
    }
}

bool GridMap::_octant_update(const OctantKey &p_key) {
    ERR_FAIL_COND_V(!octant_map.contains(p_key), false);
    Octant &g = *octant_map[p_key];
    if (!g.dirty)
        return false;

    //erase body shapes
    PhysicsServer3D::get_singleton()->body_clear_shapes(g.static_body);

    //erase body shapes debug
    if (g.collision_debug!=entt::null) {

        RenderingServer::get_singleton()->mesh_clear(g.collision_debug);
    }

    //erase navigation
    for (eastl::pair<const IndexKey,Octant::NavMesh> &E : g.navmesh_ids) {
        NavigationServer::get_singleton()->free_rid(E.second.region);

    }
    g.navmesh_ids.clear();

    //erase multimeshes

    for (int i = 0; i < g.multimesh_instances.size(); i++) {

        RenderingServer::get_singleton()->free_rid(g.multimesh_instances[i].instance);
        RenderingServer::get_singleton()->free_rid(g.multimesh_instances[i].multimesh);
    }
    g.multimesh_instances.clear();

    if (g.cells.empty()) {
        //octant no longer needed
        _octant_clean_up(p_key);
        return true;
    }

    Vector<Vector3> col_debug;

    /*
     * foreach item in this octant,
     * set item's multimesh's instance count to number of cells which have this item
     * and set said multimesh bounding box to one containing all cells which have this item
     */

    Map<int, Vector<Pair<Transform, IndexKey> > > multimesh_items;

    for (IndexKey E : g.cells) {

        ERR_CONTINUE(!cell_map.contains(E));
        const Cell &c = cell_map[E];

        if (not mesh_library || !mesh_library->has_item(c.item))
            continue;

        Vector3 cellpos = Vector3(E.x, E.y, E.z);
        Vector3 ofs = _get_offset();

        Transform xform;

        xform.basis.set_orthogonal_index(c.rot);
        xform.set_origin(cellpos * cell_size + ofs);
        xform.basis.scale(Vector3(cell_scale, cell_scale, cell_scale));
        if (baked_meshes.empty()) {
            if (mesh_library->get_item_mesh(c.item)) {
                multimesh_items[c.item].emplace_back(xform * mesh_library->get_item_mesh_transform(c.item),E);
            }
        }

        PoolVector<MeshLibrary::ShapeData> shapes = mesh_library->get_item_shapes(c.item);
        auto wr(shapes.write());
        // add the item's shape at given xform to octant's static_body
        for (int i = 0; i < shapes.size(); i++) {
            // add the item's shape
            if (not wr[i].shape)
                continue;
            PhysicsServer3D::get_singleton()->body_add_shape(g.static_body, wr[i].shape->get_phys_rid(), xform * wr[i].local_transform);
            if (g.collision_debug!=entt::null) {
                wr[i].shape->add_vertices_to_array(col_debug, xform * wr[i].local_transform);
            }
        }

        // add the item's navmesh at given xform to GridMap's Navigation3D ancestor
        Ref<NavigationMesh> navmesh = mesh_library->get_item_navmesh(c.item);
        if (navmesh) {
            Octant::NavMesh nm;
            nm.xform = xform * mesh_library->get_item_navmesh_transform(c.item);

            if (navigation) {
                RID region = NavigationServer::get_singleton()->region_create();
                NavigationServer::get_singleton()->region_set_navmesh(region, navmesh);
                NavigationServer::get_singleton()->region_set_transform(region, navigation->get_global_transform() * nm.xform);
                NavigationServer::get_singleton()->region_set_map(region, navigation->get_rid());
                nm.region = region;
            }
            g.navmesh_ids[E] = nm;
        }
    }

    //update multimeshes, only if not baked
    if (baked_meshes.empty()) {

        for (auto &E : multimesh_items) {
            Octant::MultimeshInstance mmi;

            RenderingEntity mm = RenderingServer::get_singleton()->multimesh_create();
            RenderingServer::get_singleton()->multimesh_allocate(mm, E.second.size(), RS::MULTIMESH_TRANSFORM_3D, RS::MULTIMESH_COLOR_NONE);
            RenderingServer::get_singleton()->multimesh_set_mesh(mm, mesh_library->get_item_mesh(E.first)->get_rid());

            int idx = 0;
            for (const Pair<Transform, IndexKey> &F : E.second) {
                RenderingServer::get_singleton()->multimesh_instance_set_transform(mm, idx, F.first);
#ifdef TOOLS_ENABLED

                Octant::MultimeshInstance::Item it;
                it.index = idx;
                it.transform = F.first;
                it.key = F.second;
                mmi.items.emplace_back(eastl::move(it));
#endif

                idx++;
            }

            RenderingEntity instance = RenderingServer::get_singleton()->instance_create();
            RenderingServer::get_singleton()->instance_set_base(instance, mm);

            if (is_inside_tree()) {
                RenderingServer::get_singleton()->instance_set_scenario(instance, get_world_3d()->get_scenario());
                RenderingServer::get_singleton()->instance_set_transform(instance, get_global_transform());
            }

            mmi.multimesh = mm;
            mmi.instance = instance;

            g.multimesh_instances.push_back(mmi);
        }
    }

    if (!col_debug.empty()) {

        SurfaceArrays arr;
        arr.set_positions(eastl::move(col_debug));

        RenderingServer::get_singleton()->mesh_add_surface_from_arrays(g.collision_debug, RS::PRIMITIVE_LINES, eastl::move(arr));
        SceneTree *st = SceneTree::get_singleton();
        if (st) {
            RenderingServer::get_singleton()->mesh_surface_set_material(g.collision_debug, 0, st->get_debug_collision_material()->get_rid());
        }
    }

    g.dirty = false;

    return false;
}

void GridMap::_reset_physic_bodies_collision_filters() {
    for (eastl::pair<const OctantKey,Octant *> &E : octant_map) {
        PhysicsServer3D::get_singleton()->body_set_collision_layer(E.second->static_body, collision_layer);
        PhysicsServer3D::get_singleton()->body_set_collision_mask(E.second->static_body, collision_mask);
    }
}

void GridMap::_octant_enter_world(const OctantKey &p_key) {

    ERR_FAIL_COND(!octant_map.contains(p_key));
    Octant &g = *octant_map[p_key];
    PhysicsServer3D::get_singleton()->body_set_state(g.static_body, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());
    PhysicsServer3D::get_singleton()->body_set_space(g.static_body, get_world_3d()->get_space());

    if (g.collision_debug_instance!=entt::null) {
        RenderingServer::get_singleton()->instance_set_scenario(g.collision_debug_instance, get_world_3d()->get_scenario());
        RenderingServer::get_singleton()->instance_set_transform(g.collision_debug_instance, get_global_transform());
    }

    for (int i = 0; i < g.multimesh_instances.size(); i++) {
        RenderingServer::get_singleton()->instance_set_scenario(g.multimesh_instances[i].instance, get_world_3d()->get_scenario());
        RenderingServer::get_singleton()->instance_set_transform(g.multimesh_instances[i].instance, get_global_transform());
    }

    if (navigation && mesh_library) {
        for (eastl::pair<const IndexKey,Octant::NavMesh> &F : g.navmesh_ids) {

            if (cell_map.contains(F.first) && false==F.second.region.is_valid()) {
                Ref<NavigationMesh> nm = mesh_library->get_item_navmesh(cell_map[F.first].item);
                if (nm) {
                    RID region = NavigationServer::get_singleton()->region_create();
                    NavigationServer::get_singleton()->region_set_navmesh(region, nm);
                    NavigationServer::get_singleton()->region_set_transform(region, navigation->get_global_transform() * F.second.xform);
                    NavigationServer::get_singleton()->region_set_map(region, navigation->get_rid());
                    F.second.region = region;
                }
            }
        }
    }
}

void GridMap::_octant_exit_world(const OctantKey &p_key) {

    ERR_FAIL_COND(!octant_map.contains(p_key));
    Octant &g = *octant_map[p_key];
    PhysicsServer3D::get_singleton()->body_set_state(g.static_body, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());
    PhysicsServer3D::get_singleton()->body_set_space(g.static_body, RID());

    if (g.collision_debug_instance!=entt::null) {

        RenderingServer::get_singleton()->instance_set_scenario(g.collision_debug_instance, entt::null);
    }

    for (int i = 0; i < g.multimesh_instances.size(); i++) {
        RenderingServer::get_singleton()->instance_set_scenario(g.multimesh_instances[i].instance, entt::null);
    }

    if (navigation) {
        for (eastl::pair<const IndexKey,Octant::NavMesh> &F : g.navmesh_ids) {

            if (F.second.region.is_valid()) {
                NavigationServer::get_singleton()->free_rid(F.second.region);
                F.second.region = RID();
            }
        }
    }
}

void GridMap::_octant_clean_up(const OctantKey &p_key) {

    ERR_FAIL_COND(!octant_map.contains(p_key));
    Octant &g = *octant_map[p_key];

        RenderingServer::get_singleton()->free_rid(g.collision_debug);
    g.collision_debug = entt::null;
        RenderingServer::get_singleton()->free_rid(g.collision_debug_instance);
    g.collision_debug_instance = entt::null;

    if (g.static_body.is_valid()) {
    PhysicsServer3D::get_singleton()->free_rid(g.static_body);
        g.static_body = RID();
    }

    // Erase navigation
    for (eastl::pair<const IndexKey,Octant::NavMesh> &E : g.navmesh_ids) {
        NavigationServer::get_singleton()->free_rid(E.second.region);
    }
    g.navmesh_ids.clear();

    //erase multimeshes

    for (int i = 0; i < g.multimesh_instances.size(); i++) {

        RenderingServer::get_singleton()->free_rid(g.multimesh_instances[i].instance);
        RenderingServer::get_singleton()->free_rid(g.multimesh_instances[i].multimesh);
    }
    g.multimesh_instances.clear();
}

void GridMap::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_WORLD: {

            Node3D *c = this;
            while (c) {
                navigation = object_cast<Navigation3D>(c);
                if (navigation) {
                    break;
                }

                c = object_cast<Node3D>(c->get_parent());
            }

            last_transform = get_global_transform();

            for (eastl::pair<const OctantKey,Octant *> &E : octant_map) {
                _octant_enter_world(E.first);
            }

            for (int i = 0; i < baked_meshes.size(); i++) {
                RenderingServer::get_singleton()->instance_set_scenario(baked_meshes[i].instance, get_world_3d()->get_scenario());
                RenderingServer::get_singleton()->instance_set_transform(baked_meshes[i].instance, get_global_transform());
            }

        } break;
        case NOTIFICATION_TRANSFORM_CHANGED: {

            Transform new_xform = get_global_transform();
            if (new_xform == last_transform)
                break;
            //update run
            for (eastl::pair<const OctantKey,Octant *> &E : octant_map) {
                _octant_transform(E.first);
            }

            last_transform = new_xform;

            for (int i = 0; i < baked_meshes.size(); i++) {
                RenderingServer::get_singleton()->instance_set_transform(baked_meshes[i].instance, get_global_transform());
            }

        } break;
        case NOTIFICATION_EXIT_WORLD: {

            for (eastl::pair<const OctantKey,Octant *> &E : octant_map) {
                _octant_exit_world(E.first);
            }

            navigation = nullptr;

            //_queue_octants_dirty(MAP_DIRTY_INSTANCES|MAP_DIRTY_TRANSFORMS);
            //_update_octants_callback();
            //_update_area_instances();
            for (int i = 0; i < baked_meshes.size(); i++) {
                RenderingServer::get_singleton()->instance_set_scenario(baked_meshes[i].instance, entt::null);
            }

        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {
            _update_visibility();
        } break;
    }
}

void GridMap::_update_visibility() {
    if (!is_inside_tree())
        return;

    Object_change_notify(this,"visible");

    for (eastl::pair<const OctantKey,Octant *> &e : octant_map) {
        Octant *octant = e.second;
        for (int i = 0; i < octant->multimesh_instances.size(); i++) {
            const Octant::MultimeshInstance &mi = octant->multimesh_instances[i];
            RenderingServer::get_singleton()->instance_set_visible(mi.instance, is_visible_in_tree());
        }
    }
    for (int i = 0; i < baked_meshes.size(); i++) {
        RenderingServer::get_singleton()->instance_set_visible(baked_meshes[i].instance, is_visible_in_tree());
    }
}

void GridMap::_queue_octants_dirty() {

    if (awaiting_update)
        return;

    MessageQueue::get_singleton()->push_call(get_instance_id(),[this]() { _update_octants_callback(); });
    awaiting_update = true;
}

void GridMap::_recreate_octant_data() {

    recreating_octants = true;
    //TODO: SEGS: consider using move semantics to prevent full blown copy here
    HashMap<IndexKey, Cell> cell_copy = cell_map;
    _clear_internal();
    for (eastl::pair<const IndexKey,Cell> &E : cell_copy) {

        set_cell_item(E.first.x, E.first.y, E.first.z, E.second.item, E.second.rot);
    }
    recreating_octants = false;
}

void GridMap::_clear_internal() {

    for (eastl::pair<const OctantKey,Octant *> &E : octant_map) {
        if (is_inside_world())
            _octant_exit_world(E.first);

        _octant_clean_up(E.first);
        memdelete(E.second);
    }

    octant_map.clear();
    cell_map.clear();
}

void GridMap::clear() {

    _clear_internal();
    clear_baked_meshes();
}

void GridMap::resource_changed(const RES &p_res) {

    _recreate_octant_data();
}

void GridMap::_update_octants_callback() {

    if (!awaiting_update)
        return;

    Vector<OctantKey> to_delete;
    //TODO: consider if we can use iter=octant_map.erase(iter) here?
    for (eastl::pair<const OctantKey,Octant *> &E : octant_map) {

        if (_octant_update(E.first)) {
            to_delete.emplace_back(E.first);
        }
    }

    while (!to_delete.empty()) {
        memdelete(octant_map[to_delete.front()]);
        octant_map.erase(to_delete.front());
        to_delete.pop_front();
    }

    _update_visibility();
    awaiting_update = false;
}

void GridMap::_bind_methods() {

    BIND_METHOD(GridMap,set_collision_layer);
    BIND_METHOD(GridMap,get_collision_layer);

    BIND_METHOD(GridMap,set_collision_mask);
    BIND_METHOD(GridMap,get_collision_mask);

    BIND_METHOD(GridMap,set_collision_mask_bit);
    BIND_METHOD(GridMap,get_collision_mask_bit);

    BIND_METHOD(GridMap,set_collision_layer_bit);
    BIND_METHOD(GridMap,get_collision_layer_bit);
    BIND_METHOD(GridMap,set_physics_material);
    BIND_METHOD(GridMap,get_physics_material);

    BIND_METHOD(GridMap,set_mesh_library);
    BIND_METHOD(GridMap,get_mesh_library);

    BIND_METHOD(GridMap,set_cell_size);
    BIND_METHOD(GridMap,get_cell_size);

    BIND_METHOD(GridMap,set_cell_scale);
    BIND_METHOD(GridMap,get_cell_scale);

    BIND_METHOD(GridMap,set_octant_size);
    BIND_METHOD(GridMap,get_octant_size);

    MethodBinder::bind_method(D_METHOD("set_cell_item", {"x", "y", "z", "item", "orientation"}), &GridMap::set_cell_item, {DEFVAL(0)});
    BIND_METHOD(GridMap,get_cell_item);
    BIND_METHOD(GridMap,get_cell_item_orientation);

    BIND_METHOD(GridMap,world_to_map);
    BIND_METHOD(GridMap,map_to_world);

    BIND_METHOD(GridMap,resource_changed);

    BIND_METHOD(GridMap,set_center_x);
    BIND_METHOD(GridMap,get_center_x);
    BIND_METHOD(GridMap,set_center_y);
    BIND_METHOD(GridMap,get_center_y);
    BIND_METHOD(GridMap,set_center_z);
    BIND_METHOD(GridMap,get_center_z);

    MethodBinder::bind_method(D_METHOD("set_clip", {"enabled", "clipabove", "floor", "axis"}), &GridMap::set_clip, {DEFVAL(true), DEFVAL(0), DEFVAL(Vector3::AXIS_X)});

    BIND_METHOD(GridMap,clear);

    BIND_METHOD(GridMap,get_used_cells);
    BIND_METHOD(GridMap,get_cells_used_by_item);

    BIND_METHOD(GridMap,get_meshes);
    BIND_METHOD(GridMap,get_bake_meshes);
    BIND_METHOD(GridMap,get_bake_mesh_instance);

    BIND_METHOD(GridMap,clear_baked_meshes);
    MethodBinder::bind_method(D_METHOD("make_baked_meshes", {"gen_lightmap_uv", "lightmap_uv_texel_size"}), &GridMap::make_baked_meshes, {DEFVAL(false), DEFVAL(0.1)});

    BIND_METHOD(GridMap,set_use_in_baked_light);
    BIND_METHOD(GridMap,get_use_in_baked_light);
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "mesh_library", PropertyHint::ResourceType, "MeshLibrary"), "set_mesh_library", "get_mesh_library");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "physics_material", PropertyHint::ResourceType, "PhysicsMaterial"), "set_physics_material", "get_physics_material");

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_in_baked_light"), "set_use_in_baked_light", "get_use_in_baked_light");
    ADD_GROUP("Cell", "cell_");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "cell_size"), "set_cell_size", "get_cell_size");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "cell_octant_size", PropertyHint::Range, "1,1024,1"), "set_octant_size", "get_octant_size");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "cell_center_x"), "set_center_x", "get_center_x");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "cell_center_y"), "set_center_y", "get_center_y");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "cell_center_z"), "set_center_z", "get_center_z");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "cell_scale"), "set_cell_scale", "get_cell_scale");
    ADD_GROUP("Collision", "collision_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_layer", PropertyHint::Layers3DPhysics), "set_collision_layer", "get_collision_layer");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_mask", PropertyHint::Layers3DPhysics), "set_collision_mask", "get_collision_mask");

    BIND_CONSTANT(INVALID_CELL_ITEM)
    ADD_SIGNAL(MethodInfo("cell_size_changed", PropertyInfo(VariantType::VECTOR3, "cell_size")));
}

void GridMap::set_clip(bool p_enabled, bool p_clip_above, int p_floor, Vector3::Axis p_axis) {

    if (!p_enabled && !clip)
        return;
    if (clip && p_enabled && clip_floor == p_floor && p_clip_above == clip_above && p_axis == clip_axis)
        return;

    clip = p_enabled;
    clip_floor = p_floor;
    clip_axis = p_axis;
    clip_above = p_clip_above;

    //make it all update
    for (eastl::pair<const OctantKey,Octant *> &E : octant_map) {

        Octant *g = E.second;
        g->dirty = true;
    }
    awaiting_update = true;
    _update_octants_callback();
}

void GridMap::set_cell_scale(float p_scale) {

    cell_scale = p_scale;
    _recreate_octant_data();
}

float GridMap::get_cell_scale() const {

    return cell_scale;
}

Array GridMap::get_used_cells() const {

    Array a;
    a.resize(cell_map.size());
    int i = 0;
    for (const eastl::pair<const IndexKey,Cell> &E : cell_map) {
        Vector3 p(E.first.x, E.first.y, E.first.z);
        a[i++] = p;
    }

    return a;
}

Vector<PositionedMeshInfo> GridMap::get_positioned_meshes() const
{
    Vector<PositionedMeshInfo> res;
    if (not mesh_library)
        return res;

    Vector3 ofs = _get_offset();

    for (const eastl::pair<const IndexKey,Cell> &E : cell_map) {

        int id = E.second.item;
        if (!mesh_library->has_item(id))
            continue;
        Ref<Mesh> mesh = mesh_library->get_item_mesh(id);
        if (not mesh)
            continue;

        IndexKey ik = E.first;

        Vector3 cellpos = Vector3(ik.x, ik.y, ik.z);

        Transform xform;

        xform.basis.set_orthogonal_index(E.second.rot);

        xform.set_origin(cellpos * cell_size + ofs);
        xform.basis.scale(Vector3(cell_scale, cell_scale, cell_scale));
        res.emplace_back(PositionedMeshInfo{mesh,xform});
    }

    return res;
}

Array GridMap::get_cells_used_by_item(int p_item) const {
    Array a;
    for (const auto &E : cell_map) {
        if (E.second.item == p_item) {
            Vector3 p(E.first.x, E.first.y, E.first.z);
            a.push_back(p);
        }
    }

    return a;
}

Array GridMap::get_meshes() const {

    if (not mesh_library)
        return Array();

    Vector3 ofs = _get_offset();
    Array meshes;

    for (const eastl::pair<const IndexKey, Cell> &E : cell_map) {

        int id = E.second.item;
        if (!mesh_library->has_item(id))
            continue;
        Ref<Mesh> mesh = mesh_library->get_item_mesh(id);
        if (not mesh)
            continue;

        IndexKey ik = E.first;

        Vector3 cellpos = Vector3(ik.x, ik.y, ik.z);

        Transform xform;

        xform.basis.set_orthogonal_index(E.second.rot);

        xform.set_origin(cellpos * cell_size + ofs);
        xform.basis.scale(Vector3(cell_scale, cell_scale, cell_scale));

        meshes.push_back(xform);
        meshes.push_back(mesh);
    }

    return meshes;
}

Vector3 GridMap::_get_offset() const {
    return Vector3(
            cell_size.x * 0.5 * int(center_x),
            cell_size.y * 0.5 * int(center_y),
            cell_size.z * 0.5 * int(center_z));
}

void GridMap::clear_baked_meshes() {

    for (int i = 0; i < baked_meshes.size(); i++) {
        RenderingServer::get_singleton()->free_rid(baked_meshes[i].instance);
    }
    baked_meshes.clear();

    _recreate_octant_data();
}

void GridMap::make_baked_meshes(bool p_gen_lightmap_uv, float p_lightmap_uv_texel_size) {

    if (not mesh_library)
        return;

    //generate
    Map<OctantKey, Map<Ref<Material>, Ref<SurfaceTool> > > surface_map;

    for (eastl::pair<const IndexKey,Cell> &E : cell_map) {

        IndexKey key = E.first;

        int item = E.second.item;
        if (!mesh_library->has_item(item))
            continue;

        Ref<Mesh> mesh = mesh_library->get_item_mesh(item);
        if (not mesh)
            continue;

        Vector3 cellpos = Vector3(key.x, key.y, key.z);
        Vector3 ofs = _get_offset();

        Transform xform;

        xform.basis.set_orthogonal_index(E.second.rot);
        xform.set_origin(cellpos * cell_size + ofs);
        xform.basis.scale(Vector3(cell_scale, cell_scale, cell_scale));

        OctantKey ok;
        ok.x = key.x / octant_size;
        ok.y = key.y / octant_size;
        ok.z = key.z / octant_size;

        if (!surface_map.contains(ok)) {
            surface_map[ok] = Map<Ref<Material>, Ref<SurfaceTool> >();
        }

        Map<Ref<Material>, Ref<SurfaceTool> > &mat_map = surface_map[ok];

        for (int i = 0; i < mesh->get_surface_count(); i++) {

            if (mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES)
                continue;

            Ref<Material> surf_mat = mesh->surface_get_material(i);
            if (!mat_map.contains(surf_mat)) {
                Ref<SurfaceTool> st(make_ref_counted<SurfaceTool>());
                st->begin(Mesh::PRIMITIVE_TRIANGLES);
                st->set_material(surf_mat);
                mat_map[surf_mat] = st;
            }

            mat_map[surf_mat]->append_from(mesh, i, xform);
        }
    }

    for (const eastl::pair<const OctantKey, Map<Ref<Material>, Ref<SurfaceTool> > > &E : surface_map) {

        Ref<ArrayMesh> mesh(make_ref_counted<ArrayMesh>());
        for (const eastl::pair<const Ref<Material>,Ref<SurfaceTool> > &F : E.second) {
            F.second->commit(mesh);
        }

        BakedMesh bm;
        bm.mesh = mesh;
        bm.instance = RenderingServer::get_singleton()->instance_create();
        RenderingServer::get_singleton()->get_singleton()->instance_set_base(bm.instance, bm.mesh->get_rid());
        RenderingServer::get_singleton()->instance_attach_object_instance_id(bm.instance, get_instance_id());
        if (is_inside_tree()) {
            RenderingServer::get_singleton()->instance_set_scenario(bm.instance, get_world_3d()->get_scenario());
            RenderingServer::get_singleton()->instance_set_transform(bm.instance, get_global_transform());
        }

        if (p_gen_lightmap_uv) {
            mesh->lightmap_unwrap(get_global_transform(), p_lightmap_uv_texel_size);
        }
        baked_meshes.push_back(bm);
    }

    _recreate_octant_data();
}

Array GridMap::get_bake_meshes() {
    if (!use_in_baked_light) {
        return Array();
    }

    if (baked_meshes.empty()) {
        make_baked_meshes(true);
    }

    Array arr;
    for (int i = 0; i < baked_meshes.size(); i++) {
        arr.push_back(baked_meshes[i].mesh);
        arr.push_back(Transform());
    }

    return arr;
}

RenderingEntity GridMap::get_bake_mesh_instance(int p_idx) {

    ERR_FAIL_INDEX_V(p_idx, baked_meshes.size(), entt::null);
    return baked_meshes[p_idx].instance;
}

GridMap::GridMap() {

    collision_layer = 1;
    collision_mask = 1;

    cell_size = Vector3(2, 2, 2);
    octant_size = 8;
    use_in_baked_light = false;
    awaiting_update = false;
    _in_tree = false;
    center_x = true;
    center_y = true;
    center_z = true;

    clip = false;
    clip_floor = 0;
    clip_axis = Vector3::AXIS_Z;
    clip_above = true;
    cell_scale = 1.0;

    navigation = nullptr;
    set_notify_transform(true);
    recreating_octants = false;
}

GridMap::~GridMap() {

    if ( mesh_library)
        mesh_library->unregister_owner(this);

    clear();
}
