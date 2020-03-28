/*************************************************************************/
/*  baked_lightmap.cpp                                                   */
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

#include "baked_lightmap.h"
#include "voxel_light_baker.h"

#include "scene/main/scene_tree.h"
#include "core/io/config_file.h"
#include "core/io/resource_loader.h"
#include "core/method_bind.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/resource/resource_manager.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"

IMPL_GDCLASS(BakedLightmapData)
IMPL_GDCLASS(BakedLightmap)
VARIANT_ENUM_CAST(BakedLightmap::BakeQuality);
VARIANT_ENUM_CAST(BakedLightmap::BakeMode);
VARIANT_ENUM_CAST(BakedLightmap::BakeError);

void BakedLightmapData::set_bounds(const AABB &p_bounds) {

    bounds = p_bounds;
    VisualServer::get_singleton()->lightmap_capture_set_bounds(baked_light, p_bounds);
}

AABB BakedLightmapData::get_bounds() const {

    return bounds;
}

void BakedLightmapData::set_octree(const PoolVector<uint8_t> &p_octree) {

    VisualServer::get_singleton()->lightmap_capture_set_octree(baked_light, p_octree);
}

PoolVector<uint8_t> BakedLightmapData::get_octree() const {

    return VisualServer::get_singleton()->lightmap_capture_get_octree(baked_light);
}

void BakedLightmapData::set_cell_space_transform(const Transform &p_xform) {

    cell_space_xform = p_xform;
    VisualServer::get_singleton()->lightmap_capture_set_octree_cell_transform(baked_light, p_xform);
}

Transform BakedLightmapData::get_cell_space_transform() const {
    return cell_space_xform;
}

void BakedLightmapData::set_cell_subdiv(int p_cell_subdiv) {
    cell_subdiv = p_cell_subdiv;
    VisualServer::get_singleton()->lightmap_capture_set_octree_cell_subdiv(baked_light, p_cell_subdiv);
}

int BakedLightmapData::get_cell_subdiv() const {
    return cell_subdiv;
}

void BakedLightmapData::set_energy(float p_energy) {

    energy = p_energy;
    VisualServer::get_singleton()->lightmap_capture_set_energy(baked_light, energy);
}

float BakedLightmapData::get_energy() const {

    return energy;
}

void BakedLightmapData::add_user(const NodePath &p_path, const Ref<Texture> &p_lightmap, int p_instance) {

    ERR_FAIL_COND_MSG(not p_lightmap, "It's not a reference to a valid Texture object.");
    User user;
    user.path = p_path;
    user.lightmap = p_lightmap;
    user.instance_index = p_instance;
    users.push_back(user);
}

int BakedLightmapData::get_user_count() const {

    return users.size();
}
NodePath BakedLightmapData::get_user_path(int p_user) const {

    ERR_FAIL_INDEX_V(p_user, users.size(), NodePath());
    return users[p_user].path;
}
Ref<Texture> BakedLightmapData::get_user_lightmap(int p_user) const {

    ERR_FAIL_INDEX_V(p_user, users.size(), Ref<Texture>());
    return users[p_user].lightmap;
}

int BakedLightmapData::get_user_instance(int p_user) const {

    ERR_FAIL_INDEX_V(p_user, users.size(), -1);
    return users[p_user].instance_index;
}

void BakedLightmapData::clear_users() {
    users.clear();
}

void BakedLightmapData::_set_user_data(const Array &p_data) {

    ERR_FAIL_COND(p_data.size() % 3 != 0);

    for (int i = 0; i < p_data.size(); i += 3) {
        add_user(p_data[i], refFromRefPtr<Texture>(p_data[i + 1]), p_data[i + 2]);
    }
}

Array BakedLightmapData::_get_user_data() const {

    Array ret;
    for (int i = 0; i < users.size(); i++) {
        ret.push_back(users[i].path);
        ret.push_back(users[i].lightmap);
        ret.push_back(users[i].instance_index);
    }
    return ret;
}

RID BakedLightmapData::get_rid() const {
    return baked_light;
}
void BakedLightmapData::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_set_user_data", {"data"}), &BakedLightmapData::_set_user_data);
    MethodBinder::bind_method(D_METHOD("_get_user_data"), &BakedLightmapData::_get_user_data);

    MethodBinder::bind_method(D_METHOD("set_bounds", {"bounds"}), &BakedLightmapData::set_bounds);
    MethodBinder::bind_method(D_METHOD("get_bounds"), &BakedLightmapData::get_bounds);

    MethodBinder::bind_method(D_METHOD("set_cell_space_transform", {"xform"}), &BakedLightmapData::set_cell_space_transform);
    MethodBinder::bind_method(D_METHOD("get_cell_space_transform"), &BakedLightmapData::get_cell_space_transform);

    MethodBinder::bind_method(D_METHOD("set_cell_subdiv", {"cell_subdiv"}), &BakedLightmapData::set_cell_subdiv);
    MethodBinder::bind_method(D_METHOD("get_cell_subdiv"), &BakedLightmapData::get_cell_subdiv);

    MethodBinder::bind_method(D_METHOD("set_octree", {"octree"}), &BakedLightmapData::set_octree);
    MethodBinder::bind_method(D_METHOD("get_octree"), &BakedLightmapData::get_octree);

    MethodBinder::bind_method(D_METHOD("set_energy", {"energy"}), &BakedLightmapData::set_energy);
    MethodBinder::bind_method(D_METHOD("get_energy"), &BakedLightmapData::get_energy);

    MethodBinder::bind_method(D_METHOD("add_user", {"path", "lightmap", "instance"}), &BakedLightmapData::add_user);
    MethodBinder::bind_method(D_METHOD("get_user_count"), &BakedLightmapData::get_user_count);
    MethodBinder::bind_method(D_METHOD("get_user_path", {"user_idx"}), &BakedLightmapData::get_user_path);
    MethodBinder::bind_method(D_METHOD("get_user_lightmap", {"user_idx"}), &BakedLightmapData::get_user_lightmap);
    MethodBinder::bind_method(D_METHOD("clear_users"), &BakedLightmapData::clear_users);

    ADD_PROPERTY(PropertyInfo(VariantType::AABB, "bounds", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_bounds", "get_bounds");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM, "cell_space_transform", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_cell_space_transform", "get_cell_space_transform");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "cell_subdiv", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_cell_subdiv", "get_cell_subdiv");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "energy", PropertyHint::Range, "0,16,0.01,or_greater"), "set_energy", "get_energy");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_BYTE_ARRAY, "octree", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_octree", "get_octree");
    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "user_data", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_user_data", "_get_user_data");
}

BakedLightmapData::BakedLightmapData() {

    baked_light = VisualServer::get_singleton()->lightmap_capture_create();
    energy = 1;
    cell_subdiv = 1;
}

BakedLightmapData::~BakedLightmapData() {

    VisualServer::get_singleton()->free_rid(baked_light);
}

///////////////////////////

BakedLightmap::BakeBeginFunc BakedLightmap::bake_begin_function = nullptr;
BakedLightmap::BakeStepFunc BakedLightmap::bake_step_function = nullptr;
BakedLightmap::BakeEndFunc BakedLightmap::bake_end_function = nullptr;

void BakedLightmap::set_bake_cell_size(float p_cell_size) {
    bake_cell_size = p_cell_size;
}

float BakedLightmap::get_bake_cell_size() const {
    return bake_cell_size;
}

void BakedLightmap::set_capture_cell_size(float p_cell_size) {
    capture_cell_size = p_cell_size;
}

float BakedLightmap::get_capture_cell_size() const {
    return capture_cell_size;
}

void BakedLightmap::set_extents(const Vector3 &p_extents) {
    extents = p_extents;
    update_gizmo();
    Object_change_notify(this,"bake_extents");
}

Vector3 BakedLightmap::get_extents() const {
    return extents;
}

void BakedLightmap::set_bake_default_texels_per_unit(const float &p_bake_texels_per_unit) {
    bake_default_texels_per_unit = p_bake_texels_per_unit;
    update_gizmo();
}

float BakedLightmap::get_bake_default_texels_per_unit() const {
    return bake_default_texels_per_unit;
}

void BakedLightmap::_find_meshes_and_lights(Node *p_at_node, Vector<BakedLightmap::PlotMesh> &plot_meshes, Vector<BakedLightmap::PlotLight> &plot_lights) {

    MeshInstance *mi = object_cast<MeshInstance>(p_at_node);
    if (mi && mi->get_flag(GeometryInstance::FLAG_USE_BAKED_LIGHT) && mi->is_visible_in_tree()) {
        Ref<Mesh> mesh = mi->get_mesh();
        if (mesh) {

            bool all_have_uv2 = true;
            for (int i = 0; i < mesh->get_surface_count(); i++) {
                if (!(mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_TEX_UV2)) {
                    all_have_uv2 = false;
                    break;
                }
            }

            if (all_have_uv2) {
                //READY TO BAKE! size hint could be computed if not found, actually..

                AABB aabb = mesh->get_aabb();

                Transform xf = get_global_transform().affine_inverse() * mi->get_global_transform();

                if (AABB(-extents, extents * 2).intersects(xf.xform(aabb))) {
                    PlotMesh pm;
                    pm.local_xform = xf;
                    pm.mesh = mesh;
                    pm.path = get_path_to(mi);
                    pm.instance_idx = -1;
                    for (int i = 0; i < mesh->get_surface_count(); i++) {
                        pm.instance_materials.push_back(mi->get_surface_material(i));
                    }
                    pm.override_material = mi->get_material_override();
                    plot_meshes.push_back(pm);
                }
            }
        }
    }

    Spatial *s = object_cast<Spatial>(p_at_node);

    if (!mi && s) {
        Array meshes = p_at_node->call_va("get_bake_meshes");
        if (!meshes.empty() && (meshes.size() & 1) == 0) {
            Transform xf = get_global_transform().affine_inverse() * s->get_global_transform();
            for (int i = 0; i < meshes.size(); i += 2) {
                PlotMesh pm;
                Transform mesh_xf = meshes[i + 1];
                pm.local_xform = xf * mesh_xf;
                pm.mesh = refFromVariant<Mesh>(meshes[i]);
                pm.instance_idx = i / 2;
                if (not pm.mesh)
                    continue;
                pm.path = get_path_to(s);
                plot_meshes.push_back(pm);
            }
        }
    }

    Light *light = object_cast<Light>(p_at_node);

    if (light && light->get_bake_mode() != Light::BAKE_DISABLED) {
        PlotLight pl;
        Transform xf = get_global_transform().affine_inverse() * light->get_global_transform();

        pl.local_xform = xf;
        pl.light = light;
        plot_lights.push_back(pl);
    }
    for (int i = 0; i < p_at_node->get_child_count(); i++) {

        Node *child = p_at_node->get_child(i);
        if (!child->get_owner())
            continue; //maybe a helper

        _find_meshes_and_lights(child, plot_meshes, plot_lights);
    }
}

void BakedLightmap::set_hdr(bool p_enable) {
    hdr = p_enable;
}

bool BakedLightmap::is_hdr() const {
    return hdr;
}

bool BakedLightmap::_bake_time(void *ud, float p_secs, float p_progress) {

    uint64_t time = OS::get_singleton()->get_ticks_usec();
    BakeTimeData *btd = (BakeTimeData *)ud;

    if (time - btd->last_step > 1000000) {

        int mins_left = p_secs / 60;
        int secs_left = Math::fmod(p_secs, 60.0f);
        int percent = p_progress * 100;
        bool abort =
                bake_step_function(btd->pass + percent, btd->text + " " + FormatVE(RTR_utf8("%d%%").c_str(), percent) + " " +
                                                                FormatVE(RTR_utf8("(Time Left: %d:%02d s)").c_str(), mins_left, secs_left));
        btd->last_step = time;
        if (abort)
            return true;
    }

    return false;
}

BakedLightmap::BakeError BakedLightmap::bake(Node *p_from_node, bool p_create_visual_debug) {

    String save_path;

    if (StringUtils::begins_with(image_path, "res://")) {
        save_path = image_path;
    } else {
        if (!get_filename().empty()) {
            save_path = PathUtils::get_base_dir(get_filename());
        } else if (get_owner() && !get_owner()->get_filename().empty()) {
            save_path = PathUtils::get_base_dir(get_owner()->get_filename());
        }

        if (save_path.empty()) {
            return BAKE_ERROR_NO_SAVE_PATH;
        }
        if (!image_path.empty()) {
            save_path = PathUtils::plus_file(save_path, image_path);
        }
    }
    {
        // check for valid save path
        DirAccessRef d = DirAccess::open(save_path);
        if (!d) {
            ERR_PRINT("Invalid Save Path '" + save_path + "'.");
            return BAKE_ERROR_NO_SAVE_PATH;
        }
    }

    Ref<BakedLightmapData> new_light_data(make_ref_counted<BakedLightmapData>());

    VoxelLightBaker baker;

    int bake_subdiv;
    int capture_subdiv;
    AABB bake_bounds;
    {
        bake_bounds = AABB(-extents, extents * 2.0);
        int subdiv = nearest_power_of_2_templated(int(bake_bounds.get_longest_axis_size() / bake_cell_size));
        bake_bounds.size[bake_bounds.get_longest_axis_index()] = subdiv * bake_cell_size;
        bake_subdiv = nearest_shift(subdiv) + 1;

        capture_subdiv = bake_subdiv;
        float css = bake_cell_size;
        while (css < capture_cell_size && capture_subdiv > 2) {
            capture_subdiv--;
            css *= 2.0f;
        }
    }

    baker.begin_bake(bake_subdiv, bake_bounds);

    Vector<PlotMesh> mesh_list;
    Vector<PlotLight> light_list;

    _find_meshes_and_lights(p_from_node ? p_from_node : get_parent(), mesh_list, light_list);

    if (bake_begin_function) {
        bake_begin_function(mesh_list.size() + light_list.size() + 1 + mesh_list.size() * 100);
    }

    int step = 0;

    int pmc = 0;

    for (PlotMesh &E : mesh_list) {

        if (bake_step_function) {
            bake_step_function(
                    step++, RTR_utf8("Plotting Meshes: ") + " (" + itos(pmc + 1) + "/" + itos(mesh_list.size()) + ")");
        }

        pmc++;
        baker.plot_mesh(E.local_xform, E.mesh, E.instance_materials, E.override_material);
    }

    pmc = 0;
    baker.begin_bake_light(
            VoxelLightBaker::BakeQuality(bake_quality), VoxelLightBaker::BakeMode(bake_mode), propagation, energy);

    for (const PlotLight &pl : light_list) {

        if (bake_step_function) {
            bake_step_function(
                    step++, RTR_utf8("Plotting Lights:") + " (" + itos(pmc + 1) + "/" + itos(light_list.size()) + ")");
        }

        pmc++;
        switch (pl.light->get_light_type()) {
            case VS::LIGHT_DIRECTIONAL: {
                baker.plot_light_directional(-pl.local_xform.basis.get_axis(2), pl.light->get_color(),
                        pl.light->get_param(Light::PARAM_ENERGY), pl.light->get_param(Light::PARAM_INDIRECT_ENERGY),
                        pl.light->get_bake_mode() == Light::BAKE_ALL);
            } break;
            case VS::LIGHT_OMNI: {
                baker.plot_light_omni(pl.local_xform.origin, pl.light->get_color(),
                        pl.light->get_param(Light::PARAM_ENERGY), pl.light->get_param(Light::PARAM_INDIRECT_ENERGY),
                        pl.light->get_param(Light::PARAM_RANGE), pl.light->get_param(Light::PARAM_ATTENUATION),
                        pl.light->get_bake_mode() == Light::BAKE_ALL);
            } break;
            case VS::LIGHT_SPOT: {
                baker.plot_light_spot(pl.local_xform.origin, pl.local_xform.basis.get_axis(2), pl.light->get_color(),
                        pl.light->get_param(Light::PARAM_ENERGY), pl.light->get_param(Light::PARAM_INDIRECT_ENERGY),
                        pl.light->get_param(Light::PARAM_RANGE), pl.light->get_param(Light::PARAM_ATTENUATION),
                        pl.light->get_param(Light::PARAM_SPOT_ANGLE),
                        pl.light->get_param(Light::PARAM_SPOT_ATTENUATION),
                        pl.light->get_bake_mode() == Light::BAKE_ALL);

            } break;
        }
    }
    /*if (bake_step_function) {
        bake_step_function(pmc++, RTR("Finishing Plot"));
    }*/

    baker.end_bake();

    Set<String> used_mesh_names;

    pmc = 0;
    for (PlotMesh &E : mesh_list) {

        String mesh_name = E.mesh->get_name();
        if (mesh_name.empty() || StringUtils::contains(mesh_name, ":") || StringUtils::contains(mesh_name, "/") ) {
            mesh_name = "LightMap";
        }

        if (used_mesh_names.contains(mesh_name)) {
            int idx = 2;
            String base = mesh_name;
            while (true) {
                mesh_name = base + itos(idx);
                if (!used_mesh_names.contains(mesh_name)) break;
                idx++;
            }
        }
        used_mesh_names.insert(mesh_name);

        pmc++;
        VoxelLightBaker::LightMapData lm;

        Error err;
        if (bake_step_function) {
            BakeTimeData btd;
            btd.text = RTR_utf8("Lighting Meshes: ") + mesh_name + " (" + itos(pmc) + "/" + itos(mesh_list.size()) + ")";
            btd.pass = step;
            btd.last_step = 0;
            err = baker.make_lightmap(
                    E.local_xform, E.mesh, bake_default_texels_per_unit, lm, _bake_time, &btd);
            if (err != OK) {
                bake_end_function();
                if (err == ERR_SKIP) return BAKE_ERROR_USER_ABORTED;
                return BAKE_ERROR_CANT_CREATE_IMAGE;
            }
            step += 100;
        } else {

            err = baker.make_lightmap(E.local_xform, E.mesh, bake_default_texels_per_unit, lm);
        }

        if (err == OK) {

            Ref<Image> image(make_ref_counted<Image>());

            uint32_t tex_flags = Texture::FLAGS_DEFAULT;
            if (hdr) {

                // just save a regular image
                PoolVector<uint8_t> data;
                int s = lm.light.size();
                data.resize(lm.light.size() * 2);
                {

                    PoolVector<uint8_t>::Write w = data.write();
                    PoolVector<float>::Read r = lm.light.read();
                    uint16_t *hfw = (uint16_t *)w.ptr();
                    for (int i = 0; i < s; i++) {
                        hfw[i] = Math::make_half_float(r[i]);
                    }
                }

                image->create(lm.width, lm.height, false, Image::FORMAT_RGBH, data);

            } else {

                // just save a regular image
                PoolVector<uint8_t> data;
                int s = lm.light.size();
                data.resize(lm.light.size());
                {

                    PoolVector<uint8_t>::Write w = data.write();
                    PoolVector<float>::Read r = lm.light.read();
                    for (int i = 0; i < s; i += 3) {
                        Color c(r[i + 0], r[i + 1], r[i + 2]);
                        c = c.to_srgb();
                        w[i + 0] = CLAMP(c.r * 255, 0, 255);
                        w[i + 1] = CLAMP(c.g * 255, 0, 255);
                        w[i + 2] = CLAMP(c.b * 255, 0, 255);
                    }
                }

                image->create(lm.width, lm.height, false, Image::FORMAT_RGB8, data);

                // This texture is saved to SRGB for two reasons:
                // 1) first is so it looks better when doing the LINEAR->SRGB conversion (more accurate)
                // 2) So it can be used in the GLES2 backend, which does not support linkear workflow
                tex_flags |= Texture::FLAG_CONVERT_TO_LINEAR;
            }

            String image_path = PathUtils::plus_file(save_path, mesh_name);
            Ref<Texture> texture;

            if (g_import_func) {

                bool srgb = false;
                if (false && hdr) {
                    // TODO: save hdr
                } else {
                    image_path += ".png";
                    print_line("image path saving png: " + image_path);
                    image->save_png(image_path);
                    srgb = true;
                }

                if (!FileAccess::exists(image_path + ".import")) {
                    Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
                    config->set_value("remap", "importer", "texture");
                    config->set_value("remap", "type", "StreamTexture");
                    config->set_value("params", "compress/mode", 2);
                    config->set_value("params", "detect_3d", false);
                    config->set_value("params", "flags/repeat", false);
                    config->set_value("params", "flags/filter", true);
                    config->set_value("params", "flags/mipmaps", false);
                    config->set_value("params", "flags/srgb", srgb);

                    config->save(image_path + ".import");
                }

                g_import_func(image_path);
                texture = dynamic_ref_cast<Texture>(gResourceManager().load(image_path)); // if already loaded, it will be updated on refocus?
            } else {

                image_path += ".text";
                Ref<ImageTexture> tex;
                bool set_path = true;
                if (ResourceCache::has(image_path)) {
                    tex = ObjectNS::cast_to<ImageTexture>(ResourceCache::get(image_path));
                    set_path = false;
                }

                if (not tex) {
                    tex = make_ref_counted<ImageTexture>();
                }

                tex->create_from_image(image, tex_flags);

                err = gResourceManager().save(image_path, tex, ResourceManager::FLAG_CHANGE_PATH);
                if (set_path) {
                    tex->set_path(image_path);
                }
                texture = tex;
            }
            if (err != OK) {
                if (bake_end_function) {
                    bake_end_function();
                }
                ERR_FAIL_COND_V(err != OK, BAKE_ERROR_CANT_CREATE_IMAGE);
            }

            new_light_data->add_user(E.path, texture, E.instance_idx);
        }
    }

    AABB bounds = AABB(-extents, extents * 2);
    new_light_data->set_cell_subdiv(capture_subdiv);
    new_light_data->set_bounds(bounds);
    new_light_data->set_octree(baker.create_capture_octree(capture_subdiv));
    {

        float bake_bound_size = bake_bounds.get_longest_axis_size();
        Transform to_bounds;
        to_bounds.basis.scale(Vector3(bake_bound_size, bake_bound_size, bake_bound_size));
        to_bounds.origin = bounds.position;

        Transform to_grid;
        to_grid.basis.scale(Vector3(1 << (capture_subdiv - 1), 1 << (capture_subdiv - 1), 1 << (capture_subdiv - 1)));

        Transform to_cell_space = to_grid * to_bounds.affine_inverse();
        new_light_data->set_cell_space_transform(to_cell_space);
    }

    if (bake_end_function) {
        bake_end_function();
    }

    // create the data for visual server

    if (p_create_visual_debug) {
        MultiMeshInstance *mmi = memnew(MultiMeshInstance);
        mmi->set_multimesh(baker.create_debug_multimesh(VoxelLightBaker::DEBUG_LIGHT));
        add_child(mmi);
#ifdef TOOLS_ENABLED
        if (get_tree()->get_edited_scene_root() == this) {
            mmi->set_owner(this);
        } else {
            mmi->set_owner(get_owner());
        }
#else
        mmi->set_owner(get_owner());
#endif
    }

    set_light_data(new_light_data);

    return BAKE_ERROR_OK;
}

void BakedLightmap::_notification(int p_what) {
    if (p_what == NOTIFICATION_READY) {

        if (light_data) {
            _assign_lightmaps();
        }
        request_ready(); //will need ready again if re-enters tree
    }

    if (p_what == NOTIFICATION_EXIT_TREE) {

        if (light_data) {
            _clear_lightmaps();
        }
    }
}

void BakedLightmap::_assign_lightmaps() {

    ERR_FAIL_COND(not light_data);

    for (int i = 0; i < light_data->get_user_count(); i++) {
        Ref<Texture> lightmap = light_data->get_user_lightmap(i);
        ERR_CONTINUE(not lightmap);

        Node *node = get_node(light_data->get_user_path(i));
        int instance_idx = light_data->get_user_instance(i);
        if (instance_idx >= 0) {
            RID instance = node->call_va("get_bake_mesh_instance", instance_idx);
            if (instance.is_valid()) {
                VisualServer::get_singleton()->instance_set_use_lightmap(instance, get_instance(), lightmap->get_rid());
            }
        } else {
            VisualInstance *vi = object_cast<VisualInstance>(node);
            ERR_CONTINUE(!vi);
            VisualServer::get_singleton()->instance_set_use_lightmap(vi->get_instance(), get_instance(), lightmap->get_rid());
        }
    }
}

void BakedLightmap::_clear_lightmaps() {
    ERR_FAIL_COND(not light_data);
    for (int i = 0; i < light_data->get_user_count(); i++) {
        Node *node = get_node(light_data->get_user_path(i));
        int instance_idx = light_data->get_user_instance(i);
        if (instance_idx >= 0) {
            RID instance = node->call_va("get_bake_mesh_instance", instance_idx);
            if (instance.is_valid()) {
                VisualServer::get_singleton()->instance_set_use_lightmap(instance, get_instance(), RID());
            }
        } else {
            VisualInstance *vi = object_cast<VisualInstance>(node);
            ERR_CONTINUE(!vi);
            VisualServer::get_singleton()->instance_set_use_lightmap(vi->get_instance(), get_instance(), RID());
        }
    }
}

void BakedLightmap::set_light_data(const Ref<BakedLightmapData> &p_data) {

    if (light_data) {
        if (is_inside_tree()) {
            _clear_lightmaps();
        }
        set_base(RID());
    }
    light_data = p_data;

    if (light_data) {
        set_base(light_data->get_rid());
        if (is_inside_tree()) {
            _assign_lightmaps();
        }
    }
}

Ref<BakedLightmapData> BakedLightmap::get_light_data() const {

    return light_data;
}

void BakedLightmap::debug_bake() {
    bake(get_parent(), true);
}

void BakedLightmap::set_propagation(float p_propagation) {
    propagation = p_propagation;
}

float BakedLightmap::get_propagation() const {

    return propagation;
}

void BakedLightmap::set_energy(float p_energy) {
    energy = p_energy;
}

float BakedLightmap::get_energy() const {

    return energy;
}

void BakedLightmap::set_bake_quality(BakeQuality p_quality) {
    bake_quality = p_quality;
}

BakedLightmap::BakeQuality BakedLightmap::get_bake_quality() const {
    return bake_quality;
}

void BakedLightmap::set_bake_mode(BakeMode p_mode) {
    bake_mode = p_mode;
}

BakedLightmap::BakeMode BakedLightmap::get_bake_mode() const {
    return bake_mode;
}

void BakedLightmap::set_image_path(StringView p_path) {
    image_path = p_path;
}

const String &BakedLightmap::get_image_path() const {
    return image_path;
}

AABB BakedLightmap::get_aabb() const {
    return AABB(-extents, extents * 2);
}
Vector<Face3> BakedLightmap::get_faces(uint32_t p_usage_flags) const {
    return Vector<Face3>();
}

void BakedLightmap::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_light_data", {"data"}), &BakedLightmap::set_light_data);
    MethodBinder::bind_method(D_METHOD("get_light_data"), &BakedLightmap::get_light_data);

    MethodBinder::bind_method(D_METHOD("set_bake_cell_size", {"bake_cell_size"}), &BakedLightmap::set_bake_cell_size);
    MethodBinder::bind_method(D_METHOD("get_bake_cell_size"), &BakedLightmap::get_bake_cell_size);

    MethodBinder::bind_method(D_METHOD("set_capture_cell_size", {"capture_cell_size"}), &BakedLightmap::set_capture_cell_size);
    MethodBinder::bind_method(D_METHOD("get_capture_cell_size"), &BakedLightmap::get_capture_cell_size);

    MethodBinder::bind_method(D_METHOD("set_bake_quality", {"bake_quality"}), &BakedLightmap::set_bake_quality);
    MethodBinder::bind_method(D_METHOD("get_bake_quality"), &BakedLightmap::get_bake_quality);

    MethodBinder::bind_method(D_METHOD("set_bake_mode", {"bake_mode"}), &BakedLightmap::set_bake_mode);
    MethodBinder::bind_method(D_METHOD("get_bake_mode"), &BakedLightmap::get_bake_mode);

    MethodBinder::bind_method(D_METHOD("set_extents", {"extents"}), &BakedLightmap::set_extents);
    MethodBinder::bind_method(D_METHOD("get_extents"), &BakedLightmap::get_extents);

    MethodBinder::bind_method(D_METHOD("set_bake_default_texels_per_unit", {"texels"}), &BakedLightmap::set_bake_default_texels_per_unit);
    MethodBinder::bind_method(D_METHOD("get_bake_default_texels_per_unit"), &BakedLightmap::get_bake_default_texels_per_unit);

    MethodBinder::bind_method(D_METHOD("set_propagation", {"propagation"}), &BakedLightmap::set_propagation);
    MethodBinder::bind_method(D_METHOD("get_propagation"), &BakedLightmap::get_propagation);

    MethodBinder::bind_method(D_METHOD("set_energy", {"energy"}), &BakedLightmap::set_energy);
    MethodBinder::bind_method(D_METHOD("get_energy"), &BakedLightmap::get_energy);

    MethodBinder::bind_method(D_METHOD("set_hdr", {"hdr"}), &BakedLightmap::set_hdr);
    MethodBinder::bind_method(D_METHOD("is_hdr"), &BakedLightmap::is_hdr);

    MethodBinder::bind_method(D_METHOD("set_image_path", {"image_path"}), &BakedLightmap::set_image_path);
    MethodBinder::bind_method(D_METHOD("get_image_path"), &BakedLightmap::get_image_path);

    MethodBinder::bind_method(D_METHOD("bake", {"from_node", "create_visual_debug"}), &BakedLightmap::bake, {DEFVAL(Variant()), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("debug_bake"), &BakedLightmap::debug_bake);
    ClassDB::set_method_flags(get_class_static_name(), StringName("debug_bake"), METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);

    ADD_GROUP("Bake", "bake_");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "bake_cell_size", PropertyHint::Range, "0.01,64,0.01"), "set_bake_cell_size", "get_bake_cell_size");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "bake_quality", PropertyHint::Enum, "Low,Medium,High"), "set_bake_quality", "get_bake_quality");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "bake_mode", PropertyHint::Enum, "ConeTrace,RayTrace"), "set_bake_mode", "get_bake_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "bake_propagation", PropertyHint::Range, "0,1,0.01"), "set_propagation", "get_propagation");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "bake_energy", PropertyHint::Range, "0,32,0.01"), "set_energy", "get_energy");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "bake_hdr"), "set_hdr", "is_hdr");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "bake_extents"), "set_extents", "get_extents");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "bake_default_texels_per_unit"), "set_bake_default_texels_per_unit", "get_bake_default_texels_per_unit");
    ADD_GROUP("Capture", "capture_");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "capture_cell_size", PropertyHint::Range, "0.01,64,0.01"), "set_capture_cell_size", "get_capture_cell_size");
    ADD_GROUP("Data", "");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "image_path", PropertyHint::Dir), "set_image_path", "get_image_path");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "light_data", PropertyHint::ResourceType, "BakedLightmapData"), "set_light_data", "get_light_data");

    BIND_ENUM_CONSTANT(BAKE_QUALITY_LOW)
    BIND_ENUM_CONSTANT(BAKE_QUALITY_MEDIUM)
    BIND_ENUM_CONSTANT(BAKE_QUALITY_HIGH)
    BIND_ENUM_CONSTANT(BAKE_MODE_CONE_TRACE)
    BIND_ENUM_CONSTANT(BAKE_MODE_RAY_TRACE)

    BIND_ENUM_CONSTANT(BAKE_ERROR_OK)
    BIND_ENUM_CONSTANT(BAKE_ERROR_NO_SAVE_PATH)
    BIND_ENUM_CONSTANT(BAKE_ERROR_NO_MESHES)
    BIND_ENUM_CONSTANT(BAKE_ERROR_CANT_CREATE_IMAGE)
    BIND_ENUM_CONSTANT(BAKE_ERROR_USER_ABORTED)
}

BakedLightmap::BakedLightmap() {

    extents = Vector3(10, 10, 10);
    bake_default_texels_per_unit = 20;
    bake_cell_size = 0.25;
    capture_cell_size = 0.5;

    bake_quality = BAKE_QUALITY_MEDIUM;
    bake_mode = BAKE_MODE_CONE_TRACE;
    energy = 1;
    propagation = 1;
    hdr = false;
    image_path = ".";
    set_disable_scale(true);
}
