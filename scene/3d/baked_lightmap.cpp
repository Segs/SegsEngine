/*************************************************************************/
/*  baked_lightmap.cpp                                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "core/io/config_file.h"
#include "core/io/resource_loader.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/resource/resource_manager.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"
#include "scene/3d/gi_probe.h"
#include "scene/3d/voxel_light_baker.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/texture.h"

RES_BASE_EXTENSION_IMPL(BakedLightmapData,"lmbake")

IMPL_GDCLASS(BakedLightmapData)
IMPL_GDCLASS(BakedLightmap)
VARIANT_ENUM_CAST(BakedLightmap::BakeQuality);
VARIANT_ENUM_CAST(BakedLightmap::EnvironmentMode);
VARIANT_ENUM_CAST(BakedLightmap::BakeError);

void BakedLightmapData::set_bounds(const AABB &p_bounds) {

    bounds = p_bounds;
    RenderingServer::get_singleton()->lightmap_capture_set_bounds(baked_light, p_bounds);
}

AABB BakedLightmapData::get_bounds() const {

    return bounds;
}

void BakedLightmapData::set_octree(const PoolVector<uint8_t> &p_octree) {

    RenderingServer::get_singleton()->lightmap_capture_set_octree(baked_light, p_octree);
}

PoolVector<uint8_t> BakedLightmapData::get_octree() const {

    return RenderingServer::get_singleton()->lightmap_capture_get_octree(baked_light);
}

void BakedLightmapData::set_cell_space_transform(const Transform &p_xform) {

    cell_space_xform = p_xform;
    RenderingServer::get_singleton()->lightmap_capture_set_octree_cell_transform(baked_light, p_xform);
}

Transform BakedLightmapData::get_cell_space_transform() const {
    return cell_space_xform;
}

void BakedLightmapData::set_cell_subdiv(int p_cell_subdiv) {
    cell_subdiv = p_cell_subdiv;
    RenderingServer::get_singleton()->lightmap_capture_set_octree_cell_subdiv(baked_light, p_cell_subdiv);
}

int BakedLightmapData::get_cell_subdiv() const {
    return cell_subdiv;
}

void BakedLightmapData::set_energy(float p_energy) {

    energy = p_energy;
    RenderingServer::get_singleton()->lightmap_capture_set_energy(baked_light, energy);
}

float BakedLightmapData::get_energy() const {

    return energy;
}

void BakedLightmapData::set_interior(bool p_interior) {
    interior = p_interior;
    RenderingServer::get_singleton()->lightmap_capture_set_interior(baked_light, interior);
}

bool BakedLightmapData::is_interior() const {
    return interior;
}

void BakedLightmapData::add_user(const NodePath &p_path, const Ref<Resource> &p_lightmap, int p_lightmap_slice,
        const Rect2 &p_lightmap_uv_rect, int p_instance) {
    ERR_FAIL_COND_MSG(!p_lightmap, "It's not a reference to a valid Texture object.");
    ERR_FAIL_COND(p_lightmap_slice == -1 && !object_cast<Texture>(p_lightmap.get()));
    ERR_FAIL_COND(p_lightmap_slice != -1 && !object_cast<TextureLayered>(p_lightmap.get()));
    User user;
    user.path = p_path;
    if (p_lightmap_slice == -1) {
        user.lightmap.single = dynamic_ref_cast<Texture>(p_lightmap);
    } else {
        user.lightmap.layered = dynamic_ref_cast<TextureLayered>(p_lightmap);
    }
    user.lightmap_slice = p_lightmap_slice;
    user.lightmap_uv_rect = p_lightmap_uv_rect;
    user.instance_index = p_instance;
    users.emplace_back(eastl::move(user));
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
    if (users[p_user].lightmap_slice == -1) {
        return users[p_user].lightmap.single;
    } else {
        return dynamic_ref_cast<Texture>(users[p_user].lightmap.layered);
    }
}

int BakedLightmapData::get_user_lightmap_slice(int p_user) const {
    ERR_FAIL_INDEX_V(p_user, users.size(), -1);
    return users[p_user].lightmap_slice;
}

Rect2 BakedLightmapData::get_user_lightmap_uv_rect(int p_user) const {
    ERR_FAIL_INDEX_V(p_user, users.size(), Rect2(0, 0, 1, 1));
    return users[p_user].lightmap_uv_rect;
}
int BakedLightmapData::get_user_instance(int p_user) const {

    ERR_FAIL_INDEX_V(p_user, users.size(), -1);
    return users[p_user].instance_index;
}

void BakedLightmapData::clear_users() {
    users.clear();
}

void BakedLightmapData::clear_data() {
    clear_users();
    if (baked_light != entt::null) {
        RenderingServer::get_singleton()->free_rid(baked_light);
    }
    baked_light = RenderingServer::get_singleton()->lightmap_capture_create();
}
void BakedLightmapData::_set_user_data(const Array &p_data) {
    ERR_FAIL_COND(p_data.size() <= 0);

    // Detect old lightmapper format
    if (p_data.size() % 3 == 0) {
        bool is_old_format = true;

    for (int i = 0; i < p_data.size(); i += 3) {
            is_old_format = is_old_format && p_data[i + 0].get_type() == VariantType::NODE_PATH;
            is_old_format = is_old_format && p_data[i + 1].is_ref();
            is_old_format = is_old_format && p_data[i + 2].get_type() == VariantType::INT;
            if (!is_old_format) {
                break;
            }
        }
        if (is_old_format) {
#ifdef DEBUG_ENABLED
            WARN_PRINT("Geometry at path " + String(p_data[0]) + " is using old lightmapper data. Please re-bake.");
#endif
            Array adapted_data;
            adapted_data.resize((p_data.size() / 3) * 5);
            for (int i = 0; i < p_data.size() / 3; i++) {
                adapted_data[i * 5 + 0] = p_data[i * 3 + 0];
                adapted_data[i * 5 + 1] = p_data[i * 3 + 1];
                adapted_data[i * 5 + 2] = -1;
                adapted_data[i * 5 + 3] = Rect2(0, 0, 1, 1);
                adapted_data[i * 5 + 4] = p_data[i * 3 + 2];
            }
            _set_user_data(adapted_data);
            return;
        }
    }

    ERR_FAIL_COND((p_data.size() % 5) != 0);

    for (int i = 0; i < p_data.size(); i += 5) {
        add_user(p_data[i].as<NodePath>(), refFromVariant<Texture>(p_data[i + 1]), p_data[i + 2].as<int>(),
                p_data[i + 3].as<Rect2>(), p_data[i + 4].as<int>());
    }
}

Array BakedLightmapData::_get_user_data() const {

    Array ret;
    for (int i = 0; i < users.size(); i++) {
        ret.push_back(users[i].path);
        ret.push_back(users[i].lightmap_slice == -1 ? Ref<Resource>(users[i].lightmap.single) :
                                                      Ref<Resource>(users[i].lightmap.layered));
        ret.push_back(users[i].lightmap_slice);
        ret.push_back(users[i].lightmap_uv_rect);
        ret.push_back(users[i].instance_index);
    }
    return ret;
}

RenderingEntity BakedLightmapData::get_rid() const {
    return baked_light;
}
void BakedLightmapData::_bind_methods() {

    SE_BIND_METHOD(BakedLightmapData,_set_user_data);
    SE_BIND_METHOD(BakedLightmapData,_get_user_data);

    SE_BIND_METHOD(BakedLightmapData,set_bounds);
    SE_BIND_METHOD(BakedLightmapData,get_bounds);

    MethodBinder::bind_method(
            D_METHOD("set_cell_space_transform", { "xform" }), &BakedLightmapData::set_cell_space_transform);
    SE_BIND_METHOD(BakedLightmapData,get_cell_space_transform);

    SE_BIND_METHOD(BakedLightmapData,set_cell_subdiv);
    SE_BIND_METHOD(BakedLightmapData,get_cell_subdiv);

    SE_BIND_METHOD(BakedLightmapData,set_octree);
    SE_BIND_METHOD(BakedLightmapData,get_octree);

    SE_BIND_METHOD(BakedLightmapData,set_energy);
    SE_BIND_METHOD(BakedLightmapData,get_energy);

    SE_BIND_METHOD(BakedLightmapData,set_interior);
    SE_BIND_METHOD(BakedLightmapData,is_interior);

    MethodBinder::bind_method(
            D_METHOD("add_user", { "path", "lightmap", "lightmap_slice", "lightmap_uv_rect", "instance" }),
            &BakedLightmapData::add_user);
    SE_BIND_METHOD(BakedLightmapData,get_user_count);
    SE_BIND_METHOD(BakedLightmapData,get_user_path);
    SE_BIND_METHOD(BakedLightmapData,get_user_lightmap);
    SE_BIND_METHOD(BakedLightmapData,clear_users);
    SE_BIND_METHOD(BakedLightmapData,clear_data);

    ADD_PROPERTY(PropertyInfo(VariantType::AABB, "bounds", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR),
            "set_bounds", "get_bounds");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM, "cell_space_transform", PropertyHint::None, "",
                         PROPERTY_USAGE_NOEDITOR),
            "set_cell_space_transform", "get_cell_space_transform");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "cell_subdiv", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR),
            "set_cell_subdiv", "get_cell_subdiv");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "energy", PropertyHint::Range, "0,16,0.01,or_greater"), "set_energy",
            "get_energy");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "interior"), "set_interior", "is_interior");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_BYTE_ARRAY, "octree", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR),
            "set_octree", "get_octree");
    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "user_data", PropertyHint::None, "",
                         PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL),
            "_set_user_data", "_get_user_data");
}

BakedLightmapData::BakedLightmapData() {

    baked_light = RenderingServer::get_singleton()->lightmap_capture_create();
    energy = 1;
    cell_subdiv = 1;
    interior = false;
}

BakedLightmapData::~BakedLightmapData() {

    RenderingServer::get_singleton()->free_rid(baked_light);
}

///////////////////////////

Lightmapper::BakeStepFunc BakedLightmap::bake_step_function;
Lightmapper::BakeStepFunc BakedLightmap::bake_substep_function;
Lightmapper::BakeEndFunc BakedLightmap::bake_end_function;

Size2i BakedLightmap::_compute_lightmap_size(const MeshesFound &p_mesh) {
    double area = 0;
    double uv_area = 0;
    for (int i = 0; i < p_mesh.mesh->get_surface_count(); i++) {
        SurfaceArrays arrays(p_mesh.mesh->surface_get_arrays(i));
        auto vr = arrays.positions3();
        auto &u2r = arrays.m_uv_2;
        auto &ir = arrays.m_indices;

        ERR_FAIL_COND_V(vr.empty(), Vector2());
        ERR_FAIL_COND_V(u2r.empty(), Vector2());

        int vc = vr.size();
        int ic = 0;

        if (ir.size()) {
            ic = ir.size();
}

        int faces = ic ? ic / 3 : vc / 3;
        for (int j = 0; j < faces; j++) {
            Vector3 vertex[3];
            Vector2 uv[3];

            for (int k = 0; k < 3; k++) {
                int idx = ic ? ir[j * 3 + k] : j * 3 + k;
                vertex[k] = p_mesh.xform.xform(vr[idx]);
                uv[k] = u2r[idx];
}

            Vector3 p1 = vertex[0];
            Vector3 p2 = vertex[1];
            Vector3 p3 = vertex[2];
            double a = p1.distance_to(p2);
            double b = p2.distance_to(p3);
            double c = p3.distance_to(p1);
            double halfPerimeter = (a + b + c) / 2.0;
            area += sqrt(halfPerimeter * (halfPerimeter - a) * (halfPerimeter - b) * (halfPerimeter - c));

            Vector2 uv_p1 = uv[0];
            Vector2 uv_p2 = uv[1];
            Vector2 uv_p3 = uv[2];
            double uv_a = uv_p1.distance_to(uv_p2);
            double uv_b = uv_p2.distance_to(uv_p3);
            double uv_c = uv_p3.distance_to(uv_p1);
            double uv_halfPerimeter = (uv_a + uv_b + uv_c) / 2.0;
            uv_area += sqrt(uv_halfPerimeter * (uv_halfPerimeter - uv_a) * (uv_halfPerimeter - uv_b) *
                            (uv_halfPerimeter - uv_c));
        }
}

    if (uv_area < 0.0001f) {
        uv_area = 1.0f;
}

    int pixels = Math::round(ceil((1.0 / sqrt(uv_area)) * sqrt(area * default_texels_per_unit)));
    int size = CLAMP(pixels, 2, 4096);
    return Vector2i(size, size);
}

void BakedLightmap::_find_meshes_and_lights(Node *p_at_node, Vector<MeshesFound> &meshes, Vector<LightsFound> &lights) {
    AABB bounds = AABB(-extents, extents * 2.0);

    MeshInstance3D *mi = object_cast<MeshInstance3D>(p_at_node);
    if (mi && mi->get_flag(GeometryInstance::FLAG_USE_BAKED_LIGHT) && mi->is_visible_in_tree()) {
        Ref<Mesh> mesh = mi->get_mesh();
        if (mesh) {

            bool all_have_uv2_and_normal = true;
            bool surfaces_found = false;
            for (int i = 0; i < mesh->get_surface_count(); i++) {
                if (mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
                    continue;
                }
                if (!(mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_TEX_UV2)) {
                    all_have_uv2_and_normal = false;
                    break;
                }
                if (!(mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_NORMAL)) {
                    all_have_uv2_and_normal = false;
                    break;
                }
                surfaces_found = true;
            }

            if (surfaces_found && all_have_uv2_and_normal) {
                Transform mesh_xform = get_global_transform().affine_inverse() * mi->get_global_transform();

                AABB aabb = mesh_xform.xform(mesh->get_aabb());

                if (bounds.intersects(aabb)) {
                    MeshesFound mf;
                    mf.cast_shadows = mi->get_cast_shadows_setting() != GeometryInstance::SHADOW_CASTING_SETTING_OFF;
                    mf.generate_lightmap = mi->get_generate_lightmap();
                    mf.xform = mesh_xform;
                    mf.node_path = get_path_to(mi);
                    mf.subindex = -1;
                    mf.mesh = mesh;

                    static const int lightmap_scale[4] = { 1, 2, 4,
                        8 }; // GeometryInstance3D::LIGHTMAP_SCALE_MAX = { 1, 2, 4, 8 };
                    mf.lightmap_scale = lightmap_scale[mi->get_lightmap_scale()];

                    Ref<Material> all_override = mi->get_material_override();
                    for (int i = 0; i < mesh->get_surface_count(); i++) {
                        if (all_override) {
                            mf.overrides.push_back(all_override);
                        } else {
                            mf.overrides.push_back(mi->get_surface_material(i));
                    }
                    }

                    meshes.push_back(mf);
                }
            }
        }
    }

    Node3D *s = object_cast<Node3D>(p_at_node);

    if (!mi && s) {
        Callable::CallError ce;
        Array bmeshes = p_at_node->call("get_bake_meshes", nullptr, 0, ce).as<Array>();
        if (bmeshes.size() && (bmeshes.size() & 1) == 0) {
            Transform xf = get_global_transform().affine_inverse() * s->get_global_transform();
            Ref<Material> all_override;

            GeometryInstance *gi = object_cast<GeometryInstance>(p_at_node);
            if (gi) {
                all_override = gi->get_material_override();
            }

            for (int i = 0; i < bmeshes.size(); i += 2) {
                Ref<Mesh> mesh = bmeshes[i];
                if (!mesh) {
                    continue;
                }
                Transform mesh_xform = xf * bmeshes[i + 1].as<Transform>();

                AABB aabb = mesh_xform.xform(mesh->get_aabb());

                if (!bounds.intersects(aabb)) {
                    continue;
                }
                MeshesFound mf;
                mf.xform = mesh_xform;
                mf.node_path = get_path_to(s);
                mf.subindex = i / 2;
                mf.lightmap_scale = 1;
                mf.mesh = mesh;

                if (gi) {
                    mf.cast_shadows = gi->get_cast_shadows_setting() != GeometryInstance::SHADOW_CASTING_SETTING_OFF;
                    mf.generate_lightmap = gi->get_generate_lightmap();
                } else {
                    mf.cast_shadows = true;
                    mf.generate_lightmap = true;
                }

                for (int j = 0; j < mesh->get_surface_count(); j++) {
                    mf.overrides.push_back(all_override);
                }

                meshes.push_back(mf);
            }
        }
    }

    Light3D *light = object_cast<Light3D>(p_at_node);

    if (light && light->get_bake_mode() != Light3D::BAKE_DISABLED) {
        LightsFound lf;
        lf.xform = get_global_transform().affine_inverse() * light->get_global_transform();
        lf.light = light;
        lights.push_back(lf);
    }

    for (int i = 0; i < p_at_node->get_child_count(); i++) {

        Node *child = p_at_node->get_child(i);
        if (!child->get_owner()) {
            continue; //maybe a helper
        }

        _find_meshes_and_lights(child, meshes, lights);
    }
}

void BakedLightmap::_get_material_images(const MeshesFound &p_found_mesh, Lightmapper::MeshData &r_mesh_data,
        Vector<Ref<Texture>> &r_albedo_textures, Vector<Ref<Texture>> &r_emission_textures) {
    for (int i = 0; i < p_found_mesh.mesh->get_surface_count(); ++i) {
        Ref<SpatialMaterial> mat = dynamic_ref_cast<SpatialMaterial>(p_found_mesh.overrides[i]);

        if (!mat) {
            mat = dynamic_ref_cast<SpatialMaterial>(p_found_mesh.mesh->surface_get_material(i));
}

        Ref<Texture> albedo_texture;
        Color albedo_add = Color(1, 1, 1, 1);
        Color albedo_mul = Color(1, 1, 1, 1);

        Ref<Texture> emission_texture;
        Color emission_add = Color(0, 0, 0, 0);
        Color emission_mul = Color(1, 1, 1, 1);

        if (mat) {
            albedo_texture = mat->get_texture(SpatialMaterial::TEXTURE_ALBEDO);

            if (albedo_texture) {
                albedo_mul = mat->get_albedo();
                albedo_add = Color(0, 0, 0, 0);
            } else {
                albedo_add = mat->get_albedo();
    }

            emission_texture = mat->get_texture(SpatialMaterial::TEXTURE_EMISSION);
            Color emission_color = mat->get_emission();
            float emission_energy = mat->get_emission_energy();

            if (mat->get_emission_operator() == SpatialMaterial::EMISSION_OP_ADD) {
                emission_mul = Color(1, 1, 1) * emission_energy;
                emission_add = emission_color * emission_energy;
            } else {
                emission_mul = emission_color * emission_energy;
                emission_add = Color(0, 0, 0);
            }
}

        Lightmapper::MeshData::TextureDef albedo;
        albedo.mul = albedo_mul;
        albedo.add = albedo_add;

        if (albedo_texture) {
            albedo.tex_rid = albedo_texture->get_rid();
            r_albedo_textures.push_back(albedo_texture);
        }

        r_mesh_data.albedo.push_back(albedo);

        Lightmapper::MeshData::TextureDef emission;
        emission.mul = emission_mul;
        emission.add = emission_add;

        if (emission_texture) {
            emission.tex_rid = emission_texture->get_rid();
            r_emission_textures.push_back(emission_texture);
        }
        r_mesh_data.emission.push_back(emission);
    }
}

void BakedLightmap::_save_image(String &r_base_path, Ref<Image> r_img, bool p_use_srgb) {
    if (use_hdr) {
        r_base_path += ".exr";
    } else {
        r_base_path += ".png";
        }

    String relative_path = r_base_path;
    if (relative_path.starts_with("res://")) {
        relative_path = relative_path.substr(6, relative_path.length());
    }
    bool hdr_grayscale = use_hdr && !use_color;

    r_img->lock();
    for (int i = 0; i < r_img->get_height(); i++) {
        for (int j = 0; j < r_img->get_width(); j++) {
            Color c = r_img->get_pixel(j, i);

            c.r = eastl::max(c.r, environment_min_light.r);
            c.g = eastl::max(c.g, environment_min_light.g);
            c.b = eastl::max(c.b, environment_min_light.b);

            if (hdr_grayscale) {
                c = Color(c.get_v(), 0.0f, 0.0f);
            }
            if (p_use_srgb) {
                c = c.to_srgb();
            }
            r_img->set_pixel(j, i, c);
        }
    }
    r_img->unlock();

    if (!use_color) {
        if (use_hdr) {
            r_img->convert(ImageData::FORMAT_RH);
        } else {
            r_img->convert(ImageData::FORMAT_L8);
        }
    }

    if (use_hdr) {
        r_img->save_exr(relative_path, !use_color);
    } else {
        r_img->save_png(relative_path);
    }
}

bool BakedLightmap::_lightmap_bake_step_function(float p_completion, StringView p_text, void *ud, bool p_refresh) {
    BakeStepUD *bsud = (BakeStepUD *)ud;
    bool ret = false;
    if (bsud->func) {
        ret = bsud->func(bsud->from_percent + p_completion * (bsud->to_percent - bsud->from_percent), p_text, bsud->ud,
                p_refresh);
    }
    return ret;
}

BakedLightmap::BakeError BakedLightmap::bake(Node *p_from_node, StringView p_data_save_path) {
    if (!p_from_node && !get_parent()) {
        return BAKE_ERROR_NO_ROOT;
    }
    bool no_save_path = false;
    if (p_data_save_path.empty() && (!get_light_data() || !PathUtils::is_resource_file(get_light_data()->get_path()))) {
        no_save_path = true;
    }

    if (p_data_save_path.empty()) {
        if (!get_light_data()) {
            no_save_path = true;
        } else {
            p_data_save_path = get_light_data()->get_path();
            if (!PathUtils::is_resource_file(p_data_save_path)) {
                no_save_path = true;
            }
        }
    }

    if (no_save_path) {
        if (image_path == "") {
            return BAKE_ERROR_NO_SAVE_PATH;
        } else {
            p_data_save_path = image_path;
        }
        WARN_PRINT("Using the deprecated property \"image_path\" as a save path, consider providing a better save path "
                   "via the \"data_save_path\" parameter");
        p_data_save_path = PathUtils::plus_file(image_path, "BakedLightmap.lmbake");
        }
    String save_path(PathUtils::get_base_dir(p_data_save_path));
    {
        // check for valid save path
        Error err;
        DirAccessRef d(DirAccess::open(save_path, &err));
        if (!d) {
            ERR_PRINT("Invalid Save Path '" + save_path + "'.");
            return BAKE_ERROR_NO_SAVE_PATH;
        }
    }

    uint32_t time_started = OS::get_singleton()->get_ticks_msec();

    if (bake_step_function) {
        bool cancelled = bake_step_function(0.0, TTR("Finding meshes and lights"), nullptr, true);
        if (cancelled) {
            bake_end_function(time_started);
            return BAKE_ERROR_USER_ABORTED;
        }
    }

    Ref<Lightmapper> lightmapper = Lightmapper::create();
    if (!lightmapper) {
        bake_end_function(time_started);
        return BAKE_ERROR_NO_LIGHTMAPPER;
    }

    Vector<LightsFound> lights_found;
    Vector<MeshesFound> meshes_found;

    _find_meshes_and_lights(p_from_node ? p_from_node : get_parent(), meshes_found, lights_found);

    if (meshes_found.empty()) {
        bake_end_function(time_started);
        return BAKE_ERROR_NO_MESHES;
    }

    for (int m_i = 0; m_i < meshes_found.size(); m_i++) {
        if (bake_step_function) {
            float p = (float)(m_i) / meshes_found.size();
            bool cancelled = bake_step_function(p * 0.05,
                    FormatVE(TTR("Preparing geometry (%d/%d)").asCString(), m_i + 1, meshes_found.size()), nullptr,
                    false);
            if (cancelled) {
                bake_end_function(time_started);
                return BAKE_ERROR_USER_ABORTED;
            }
        }

        MeshesFound &mf = meshes_found[m_i];

        Size2i lightmap_size = mf.mesh->get_lightmap_size_hint();

        if (lightmap_size == Vector2i(0, 0)) {
            lightmap_size = _compute_lightmap_size(mf);
        }
        lightmap_size *= mf.lightmap_scale;

        Lightmapper::MeshData md;
    {
            Dictionary d;
            d["path"] = mf.node_path;
            if (mf.subindex >= 0) {
                d["subindex"] = mf.subindex;
            }
            d["cast_shadows"] = mf.cast_shadows;
            d["generate_lightmap"] = mf.generate_lightmap;
            d["node_name"] = mf.node_path.get_name(mf.node_path.get_name_count() - 1);
            md.userdata = d;
        }

        Basis normal_xform = mf.xform.basis.inverse().transposed();

        for (int i = 0; i < mf.mesh->get_surface_count(); i++) {
            if (mf.mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
                continue;
        }
            SurfaceArrays a = mf.mesh->surface_get_arrays(i);
            ERR_CONTINUE(a.m_uv_2.empty());
            ERR_CONTINUE(a.m_normals.empty());

            const Vector3 *vr = a.positions3().data();
            const Vector2 *uv2r = a.m_uv_2.data();
            const Vector2 *uvr = a.m_uv_1.empty() ? nullptr : a.m_uv_1.data();
            const Vector3 *nr = a.m_normals.data();
            Vector<int> &index = a.m_indices;

            int facecount;
            const int *ir = nullptr;

            if (index.size()) {
                facecount = index.size() / 3;
                ir = index.data();
            } else {
                facecount = a.positions3().size() / 3;
    }

            md.surface_facecounts.push_back(facecount);

            for (int j = 0; j < facecount; j++) {
                uint32_t vidx[3];

                if (ir) {
                    for (int k = 0; k < 3; k++) {
                        vidx[k] = ir[j * 3 + k];
                    }
                } else {
                    for (int k = 0; k < 3; k++) {
                        vidx[k] = j * 3 + k;
                    }
                }

                for (int k = 0; k < 3; k++) {
                    Vector3 v = mf.xform.xform(vr[vidx[k]]);
                    md.points.push_back(v);

                    md.uv2.push_back(uv2r[vidx[k]]);
                    md.normal.push_back(normal_xform.xform(nr[vidx[k]]).normalized());

                    if (uvr != nullptr) {
                        md.uv.push_back(uvr[vidx[k]]);
                    }
                }
            }
    }

        Vector<Ref<Texture>> albedo_textures;
        Vector<Ref<Texture>> emission_textures;

        _get_material_images(mf, md, albedo_textures, emission_textures);

        for (int j = 0; j < albedo_textures.size(); j++) {
            lightmapper->add_albedo_texture(albedo_textures[j]);
        }

        for (int j = 0; j < emission_textures.size(); j++) {
            lightmapper->add_emission_texture(emission_textures[j]);
        }

        lightmapper->add_mesh(md, lightmap_size);
    }

    for (int i = 0; i < lights_found.size(); i++) {
        Light3D *light = lights_found[i].light;
        Transform xf = lights_found[i].xform;

        if (auto *l = object_cast<DirectionalLight3D>(light)) {
            lightmapper->add_directional_light(light->get_bake_mode() == Light3D::BAKE_ALL,
                    -xf.basis.get_axis(Vector3::AXIS_Z).normalized(), l->get_color(), l->get_param(Light3D::PARAM_ENERGY),
                    l->get_param(Light3D::PARAM_INDIRECT_ENERGY), l->get_param(Light3D::PARAM_SIZE));

        } else if (auto *l = object_cast<OmniLight3D>(light)) {
            lightmapper->add_omni_light(light->get_bake_mode() == Light3D::BAKE_ALL, xf.origin, l->get_color(),
                    l->get_param(Light3D::PARAM_ENERGY), l->get_param(Light3D::PARAM_INDIRECT_ENERGY),
                    l->get_param(Light3D::PARAM_RANGE), l->get_param(Light3D::PARAM_ATTENUATION),
                    l->get_param(Light3D::PARAM_SIZE));

        } else if (auto *l = object_cast<SpotLight3D>(light)) {
            lightmapper->add_spot_light(light->get_bake_mode() == Light3D::BAKE_ALL, xf.origin,
                    -xf.basis.get_axis(Vector3::AXIS_Z).normalized(), l->get_color(), l->get_param(Light3D::PARAM_ENERGY),
                    l->get_param(Light3D::PARAM_INDIRECT_ENERGY), l->get_param(Light3D::PARAM_RANGE),
                    l->get_param(Light3D::PARAM_ATTENUATION), l->get_param(Light3D::PARAM_SPOT_ANGLE),
                    l->get_param(Light3D::PARAM_SPOT_ATTENUATION), l->get_param(Light3D::PARAM_SIZE));
        }
    }

    Ref<Image> environment_image;
    Basis environment_xform;

    if (environment_mode != ENVIRONMENT_MODE_DISABLED) {
        if (bake_step_function) {
            bake_step_function(0.1, TTR("Preparing environment"), nullptr, true);
        }

        switch (environment_mode) {
            case ENVIRONMENT_MODE_DISABLED: {
                // nothing
            } break;
            case ENVIRONMENT_MODE_SCENE: {
                Ref<World3D> world = get_world_3d();
                if (world) {
                    Ref<Environment> env = world->get_environment();
                    if (!env) {
                        env = world->get_fallback_environment();
                    }

                    if (env) {
                        environment_image = _get_irradiance_map(env, Vector2i(128, 64));
                        environment_xform = get_global_transform().affine_inverse().basis * env->get_sky_orientation();
                    }
                }
            } break;
            case ENVIRONMENT_MODE_CUSTOM_SKY: {
                if (environment_custom_sky) {
                    environment_image = _get_irradiance_from_sky(environment_custom_sky, environment_custom_energy, Vector2i(128, 64));
                    environment_xform.set_euler(environment_custom_sky_rotation_degrees * Math_PI / 180.0);
                }

            } break;
            case ENVIRONMENT_MODE_CUSTOM_COLOR: {
                environment_image = make_ref_counted<Image>();
                environment_image->create(128, 64, false, ImageData::FORMAT_RGBF);
                Color c = environment_custom_color;
                c.r *= environment_custom_energy;
                c.g *= environment_custom_energy;
                c.b *= environment_custom_energy;
                environment_image->lock();
                for (int i = 0; i < 128; i++) {
                    for (int j = 0; j < 64; j++) {
                        environment_image->set_pixel(i, j, c);
        }
    }
                environment_image->unlock();
            } break;
        }
    }

    BakeStepUD bsud;
    bsud.func = bake_step_function;
    bsud.ud = nullptr;
    bsud.from_percent = 0.1f;
    bsud.to_percent = 0.9f;

    bool gen_atlas = generate_atlas;

    Lightmapper::BakeError bake_err = lightmapper->bake(Lightmapper::BakeQuality(bake_quality), use_denoiser, bounces,
            bounce_indirect_energy,
            bias, gen_atlas, max_atlas_size, environment_image, environment_xform, _lightmap_bake_step_function, &bsud,
            bake_substep_function);

    if (bake_err != Lightmapper::BAKE_OK) {
        bake_end_function(time_started);
        switch (bake_err) {
            case Lightmapper::BAKE_ERROR_USER_ABORTED: {
                return BAKE_ERROR_USER_ABORTED;
        }

            case Lightmapper::BAKE_ERROR_LIGHTMAP_TOO_SMALL: {
                return BAKE_ERROR_LIGHTMAP_SIZE;
            }
            case Lightmapper::BAKE_ERROR_NO_MESHES: {
                return BAKE_ERROR_NO_MESHES;
        }
            default: {
            }
        }
        return BAKE_ERROR_NO_LIGHTMAPPER;
    }

    Ref<BakedLightmapData> data;
    if (get_light_data()) {
        data = get_light_data();
        set_light_data(Ref<BakedLightmapData>()); // clear
        data->clear_data();
    } else {
        data = make_ref_counted<BakedLightmapData>();
    }

    if (capture_enabled) {
        if (bake_step_function) {
            bool cancelled = bake_step_function(0.85, TTR("Generating capture"), nullptr, true);
            if (cancelled) {
                bake_end_function(time_started);
                return BAKE_ERROR_USER_ABORTED;
            }
        }

        VoxelLightBaker voxel_baker;

        int bake_subdiv;
        int capture_subdiv;
        AABB bake_bounds;
        {
            bake_bounds = AABB(-extents, extents * 2.0);
            int subdiv = nearest_power_of_2_templated(int(bake_bounds.get_longest_axis_size() / capture_cell_size));
            bake_bounds.size[bake_bounds.get_longest_axis_index()] = subdiv * capture_cell_size;
            bake_subdiv = nearest_shift(subdiv) + 1;

            capture_subdiv = bake_subdiv;
            float css = capture_cell_size;
            while (css < capture_cell_size && capture_subdiv > 2) {
                capture_subdiv--;
                css *= 2.0;
            }
        }

        voxel_baker.begin_bake(capture_subdiv + 1, bake_bounds);

        for (int mesh_id = 0; mesh_id < meshes_found.size(); mesh_id++) {
            MeshesFound &mf = meshes_found[mesh_id];
            voxel_baker.plot_mesh(mf.xform, mf.mesh, mf.overrides, Ref<Material>());
        }

        VoxelLightBaker::BakeQuality capt_quality = VoxelLightBaker::BakeQuality::BAKE_QUALITY_HIGH;
        if (capture_quality == BakedLightmap::BakeQuality::BAKE_QUALITY_LOW) {
            capt_quality = VoxelLightBaker::BakeQuality::BAKE_QUALITY_LOW;
        } else if (capture_quality == BakedLightmap::BakeQuality::BAKE_QUALITY_MEDIUM) {
            capt_quality = VoxelLightBaker::BakeQuality::BAKE_QUALITY_MEDIUM;
        }

        voxel_baker.begin_bake_light(capt_quality, capture_propagation);

        for (int i = 0; i < lights_found.size(); i++) {
            LightsFound &lf = lights_found[i];
            switch (lf.light->get_light_type()) {
                case RS::LIGHT_DIRECTIONAL: {
                    voxel_baker.plot_light_directional(-lf.xform.basis.get_axis(2), lf.light->get_color(),
                            lf.light->get_param(Light3D::PARAM_ENERGY),
                            lf.light->get_param(Light3D::PARAM_INDIRECT_ENERGY),
                            lf.light->get_bake_mode() == Light3D::BAKE_ALL);
                } break;
                case RS::LIGHT_OMNI: {
                    voxel_baker.plot_light_omni(lf.xform.origin, lf.light->get_color(),
                            lf.light->get_param(Light3D::PARAM_ENERGY),
                            lf.light->get_param(Light3D::PARAM_INDIRECT_ENERGY),
                            lf.light->get_param(Light3D::PARAM_RANGE), lf.light->get_param(Light3D::PARAM_ATTENUATION),
                            lf.light->get_bake_mode() == Light3D::BAKE_ALL);
                } break;
                case RS::LIGHT_SPOT: {
                    voxel_baker.plot_light_spot(lf.xform.origin, lf.xform.basis.get_axis(2), lf.light->get_color(),
                            lf.light->get_param(Light3D::PARAM_ENERGY),
                            lf.light->get_param(Light3D::PARAM_INDIRECT_ENERGY),
                            lf.light->get_param(Light3D::PARAM_RANGE), lf.light->get_param(Light3D::PARAM_ATTENUATION),
                            lf.light->get_param(Light3D::PARAM_SPOT_ANGLE),
                            lf.light->get_param(Light3D::PARAM_SPOT_ATTENUATION),
                            lf.light->get_bake_mode() == Light3D::BAKE_ALL);

                } break;
            }
        }

        voxel_baker.end_bake();

        AABB bounds = AABB(-extents, extents * 2);
        data->set_cell_subdiv(capture_subdiv);
        data->set_bounds(bounds);
        data->set_octree(voxel_baker.create_capture_octree(capture_subdiv));
                {
            float bake_bound_size = bake_bounds.get_longest_axis_size();
            Transform to_bounds;
            to_bounds.basis.scale(Vector3(bake_bound_size, bake_bound_size, bake_bound_size));
            to_bounds.origin = bounds.position;

            Transform to_grid;
            to_grid.basis.scale(
                    Vector3(1 << (capture_subdiv - 1), 1 << (capture_subdiv - 1), 1 << (capture_subdiv - 1)));

            Transform to_cell_space = to_grid * to_bounds.affine_inverse();
            data->set_cell_space_transform(to_cell_space);
                    }
                }

    if (bake_step_function) {
        bool cancelled = bake_step_function(0.9, TTR("Saving lightmaps"), nullptr, true);
        if (cancelled) {
            bake_end_function(time_started);
            return BAKE_ERROR_USER_ABORTED;
        }
    }

    Vector<Ref<Image>> images;
    for (int i = 0; i < lightmapper->get_bake_texture_count(); i++) {
        images.push_back(lightmapper->get_bake_texture(i));
    }

    bool use_srgb = use_color && !use_hdr;

    if (gen_atlas) {
        int slice_count = images.size();
        int slice_width = images[0]->get_width();
        int slice_height = images[0]->get_height();

        int slices_per_texture = ImageData::MAX_HEIGHT / slice_height;
        int texture_count = Math::ceil(slice_count / (float)slices_per_texture);

        Vector<Ref<TextureLayered>> textures;
        textures.resize(texture_count);
        String base_path(PathUtils::get_basename(p_data_save_path));

        int last_count = slice_count % slices_per_texture;
        for (int i = 0; i < texture_count; i++) {
            String texture_path = texture_count > 1 ? base_path + "_" + itos(i) : base_path;
            int texture_slice_count = (i == texture_count - 1 && last_count != 0) ? last_count : slices_per_texture;

            Ref<Image> large_image(make_ref_counted<Image>());

            large_image->create(slice_width, slice_height * texture_slice_count, false, images[0]->get_format());

            for (int j = 0; j < texture_slice_count; j++) {
                large_image->blit_rect(images[i * slices_per_texture + j], Rect2(0, 0, slice_width, slice_height),
                        Point2(0, slice_height * j));
                    }
            _save_image(texture_path, large_image, use_srgb);

            Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
            if (FileAccess::exists(texture_path + ".import")) {
                config->load(texture_path + ".import");
            } else {
                // Set only if settings don't exist, to keep user choice
                config->set_value("params", "compress/mode", 0);
                }
            config->set_value("remap", "importer", "texture_array");
            config->set_value("remap", "type", "TextureArray");
            config->set_value("params", "detect_3d", false);
            config->set_value("params", "flags/repeat", false);
            config->set_value("params", "flags/filter", true);
            config->set_value("params", "flags/mipmaps", false);
            config->set_value("params", "flags/srgb", use_srgb);
            config->set_value("params", "slices/horizontal", 1);
            config->set_value("params", "slices/vertical", texture_slice_count);

            config->save(texture_path + ".import");
            g_import_func(texture_path);
            textures[i] = gResourceManager().loadT<TextureLayered>(texture_path); // if already loaded, it will be updated on refocus?
        }

        for (int i = 0; i < lightmapper->get_bake_mesh_count(); i++) {
            if (!meshes_found[i].generate_lightmap) {
                continue;
            }
            Dictionary d = lightmapper->get_bake_mesh_userdata(i).as<Dictionary>();
            NodePath np = d["path"].as<NodePath>();
            int32_t subindex = -1;
            if (d.has("subindex")) {
                subindex = d["subindex"].as<int>();
            }

            Rect2 uv_rect = lightmapper->get_bake_mesh_uv_scale(i);
            int slice_index = lightmapper->get_bake_mesh_texture_slice(i);
            data->add_user(np, textures[slice_index / slices_per_texture], slice_index % slices_per_texture, uv_rect,
                    subindex);
        }
    } else {
        for (int i = 0; i < lightmapper->get_bake_mesh_count(); i++) {
            if (!meshes_found[i].generate_lightmap) {
                continue;
            }
            Ref<Texture> texture;
            String base_path = PathUtils::plus_file(PathUtils::get_base_dir(p_data_save_path), images[i]->get_name());

            if (g_import_func) {
                _save_image(base_path, images[i], use_srgb);

                Ref<ConfigFile> config(make_ref_counted<ConfigFile>());

                if (FileAccess::exists(base_path + ".import")) {
                    config->load(base_path + ".import");
                } else {
                    // Set only if settings don't exist, to keep user choice
                    config->set_value("params", "compress/mode", 0);
                }

                    config->set_value("remap", "importer", "texture");
                    config->set_value("remap", "type", "StreamTexture");
                    config->set_value("params", "detect_3d", false);
                    config->set_value("params", "flags/repeat", false);
                    config->set_value("params", "flags/filter", true);
                    config->set_value("params", "flags/mipmaps", false);
                config->set_value("params", "flags/srgb", use_srgb);

                config->save(base_path + ".import");

                g_import_func(base_path);
                // if already loaded, it will be updated on refocus?
                texture = dynamic_ref_cast<Texture>(gResourceManager().load(base_path));
            } else {

                base_path += ".tex";
                Ref<ImageTexture> tex;
                bool set_path = true;
                if (ResourceCache::has(base_path)) {
                    tex = dynamic_ref_cast<ImageTexture>(Ref<Resource>(ResourceCache::get(base_path)));
                    set_path = false;
                }

                if (!tex) {
                    tex = make_ref_counted<ImageTexture>();
                }

                tex->create_from_image(images[i], Texture::FLAGS_DEFAULT);
                gResourceManager().save(base_path, tex, ResourceManager::FLAG_CHANGE_PATH);
                if (set_path) {
                    tex->set_path(base_path);
                }
                texture = tex;
            }
            Dictionary d = lightmapper->get_bake_mesh_userdata(i).as<Dictionary>();
            NodePath np = d["path"].as<NodePath>();
            int32_t subindex = -1;
            if (d.has("subindex")) {
                subindex = d["subindex"].as<int>();
                }
            Rect2 uv_rect = Rect2(0, 0, 1, 1);
            int slice_index = -1;
            data->add_user(np, texture, slice_index, uv_rect, subindex);
        }
            }

    if (bake_step_function) {
        bool cancelled = bake_step_function(1.0, TTR("Done"), nullptr, true);
        if (cancelled) {
            bake_end_function(time_started);
            return BAKE_ERROR_USER_ABORTED;
        }
    }

    Error err = gResourceManager().save(p_data_save_path, data);
    data->set_path(p_data_save_path);

    if (err != OK) {
        bake_end_function(time_started);
        return BAKE_ERROR_CANT_CREATE_IMAGE;
    }

    set_light_data(data);
    bake_end_function(time_started);

    return BAKE_ERROR_OK;
    }

void BakedLightmap::set_capture_cell_size(float p_cell_size) {
    capture_cell_size = eastl::max(0.1f, p_cell_size);
    }

float BakedLightmap::get_capture_cell_size() const {
    return capture_cell_size;
}

void BakedLightmap::set_extents(const Vector3 &p_extents) {
    extents = p_extents;
    update_gizmo();
    Object_change_notify(this,"extents");
        }
Vector3 BakedLightmap::get_extents() const {
    return extents;
    }

void BakedLightmap::set_default_texels_per_unit(const float &p_bake_texels_per_unit) {
    default_texels_per_unit = eastl::max(0.0f, p_bake_texels_per_unit);
}

float BakedLightmap::get_default_texels_per_unit() const {
    return default_texels_per_unit;
}

void BakedLightmap::set_capture_enabled(bool p_enable) {
    capture_enabled = p_enable;
    Object_change_notify(this);
}

bool BakedLightmap::get_capture_enabled() const {
    return capture_enabled;
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
        ERR_CONTINUE(!object_cast<Texture>(lightmap.get()) && !object_cast<TextureLayered>(lightmap.get()));

        Node *node = get_node(light_data->get_user_path(i));
        int instance_idx = light_data->get_user_instance(i);
        if (instance_idx >= 0) {
            RenderingEntity instance = node->call_va("get_bake_mesh_instance", instance_idx).as<RenderingEntity>();
            if (instance != entt::null) {
                int slice = light_data->get_user_lightmap_slice(i);
                RenderingServer::get_singleton()->instance_set_use_lightmap(
                        instance, get_instance(), lightmap->get_rid(), slice, light_data->get_user_lightmap_uv_rect(i));
            }
        } else {
            VisualInstance3D *vi = object_cast<VisualInstance3D>(node);
            ERR_CONTINUE(!vi);
            int slice = light_data->get_user_lightmap_slice(i);
            RenderingServer::get_singleton()->instance_set_use_lightmap(vi->get_instance(), get_instance(),
                    lightmap->get_rid(), slice, light_data->get_user_lightmap_uv_rect(i));
        }
    }
}

void BakedLightmap::_clear_lightmaps() {
    ERR_FAIL_COND(not light_data);
    for (int i = 0; i < light_data->get_user_count(); i++) {
        Node *node = get_node(light_data->get_user_path(i));
        int instance_idx = light_data->get_user_instance(i);
        if (instance_idx >= 0) {
            RenderingEntity instance = node->call_va("get_bake_mesh_instance", instance_idx).as<RenderingEntity>();
            if (instance != entt::null) {
                RenderingServer::get_singleton()->instance_set_use_lightmap(
                        instance, get_instance(), entt::null, -1, Rect2(0, 0, 1, 1));
            }
        } else {
            VisualInstance3D *vi = object_cast<VisualInstance3D>(node);
            ERR_CONTINUE(!vi);
            RenderingServer::get_singleton()->instance_set_use_lightmap(
                    vi->get_instance(), get_instance(), entt::null, -1, Rect2(0, 0, 1, 1));
        }
    }
}

Ref<Image> BakedLightmap::_get_irradiance_from_sky(Ref<Sky> p_sky, float p_energy, Vector2i p_size) {
    if (!p_sky) {
        return Ref<Image>();
    }

    Ref<Image> sky_image;
    Ref<PanoramaSky> panorama = dynamic_ref_cast<PanoramaSky>(p_sky);
    if (panorama) {
        sky_image = panorama->get_panorama()->get_data();
    }
    Ref<ProceduralSky> procedural = dynamic_ref_cast<ProceduralSky>(p_sky);
    if (procedural) {
        sky_image = procedural->get_data();
    }

    if (sky_image) {
        return Ref<Image>();
    }

    sky_image->convert(ImageData::FORMAT_RGBF);
    sky_image->resize(p_size.x, p_size.y, Image::INTERPOLATE_CUBIC);
    if (p_energy != 1.0f) {
        sky_image->lock();
        for (int i = 0; i < p_size.y; i++) {
            for (int j = 0; j < p_size.x; j++) {
                sky_image->set_pixel(j, i, sky_image->get_pixel(j, i) * p_energy);
            }
        }
        sky_image->unlock();
    }
    return sky_image;
}

Ref<Image> BakedLightmap::_get_irradiance_map(Ref<Environment> p_env, Vector2i p_size) {
    Environment::BGMode bg_mode = p_env->get_background();
    switch (bg_mode) {
        case Environment::BG_SKY: {
            return _get_irradiance_from_sky(p_env->get_sky(), p_env->get_bg_energy(), Vector2i(128, 64));
        }
        case Environment::BG_CLEAR_COLOR:
        case Environment::BG_COLOR: {
            Color c = bg_mode == Environment::BG_CLEAR_COLOR ?
                              Color(GLOBAL_GET("rendering/environment/default_clear_color")) :
                              p_env->get_bg_color();
            c.r *= p_env->get_bg_energy();
            c.g *= p_env->get_bg_energy();
            c.b *= p_env->get_bg_energy();

            Ref<Image> ret = make_ref_counted<Image>();
            ret->create(p_size.x, p_size.y, false, ImageData::FORMAT_RGBF);
            ret->fill(c);
            return ret;
        }
        default: {
        }
    }
    return Ref<Image>();
}
void BakedLightmap::set_light_data(const Ref<BakedLightmapData> &p_data) {

    if (light_data) {
        if (is_inside_tree()) {
            _clear_lightmaps();
        }
        set_base(entt::null);
    }
    light_data = p_data;
    Object_change_notify(this);

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

void BakedLightmap::set_capture_propagation(float p_propagation) {
    capture_propagation = p_propagation;
}

float BakedLightmap::get_capture_propagation() const {
    return capture_propagation;
}

void BakedLightmap::set_capture_quality(BakeQuality p_quality) {
    capture_quality = p_quality;
}

BakedLightmap::BakeQuality BakedLightmap::get_capture_quality() const {
    return capture_quality;
}

void BakedLightmap::set_generate_atlas(bool p_enabled) {
    generate_atlas = p_enabled;
}

bool BakedLightmap::is_generate_atlas_enabled() const {
    return generate_atlas;
}

void BakedLightmap::set_max_atlas_size(int p_size) {
    ERR_FAIL_COND(p_size < 2048);
    max_atlas_size = p_size;
}

int BakedLightmap::get_max_atlas_size() const {
    return max_atlas_size;
}

void BakedLightmap::set_bake_quality(BakeQuality p_quality) {
    bake_quality = p_quality;
    Object_change_notify(this);
}

BakedLightmap::BakeQuality BakedLightmap::get_bake_quality() const {
    return bake_quality;
}

#ifndef DISABLE_DEPRECATED

void BakedLightmap::set_image_path(StringView p_path) {
    image_path = p_path;
}

StringView BakedLightmap::get_image_path() const {
    return image_path;
}
#endif

void BakedLightmap::set_use_denoiser(bool p_enable) {

    use_denoiser = p_enable;
}

bool BakedLightmap::is_using_denoiser() const {

    return use_denoiser;
}

void BakedLightmap::set_use_hdr(bool p_enable) {

    use_hdr = p_enable;
}

bool BakedLightmap::is_using_hdr() const {

    return use_hdr;
}

void BakedLightmap::set_use_color(bool p_enable) {

    use_color = p_enable;
}

bool BakedLightmap::is_using_color() const {

    return use_color;
}

void BakedLightmap::set_environment_mode(EnvironmentMode p_mode) {
    environment_mode = p_mode;
    Object_change_notify(this);
}

BakedLightmap::EnvironmentMode BakedLightmap::get_environment_mode() const {
    return environment_mode;
}

void BakedLightmap::set_environment_custom_sky(const Ref<Sky> &p_sky) {
    environment_custom_sky = p_sky;
}

Ref<Sky> BakedLightmap::get_environment_custom_sky() const {
    return environment_custom_sky;
}

void BakedLightmap::set_environment_custom_sky_rotation_degrees(const Vector3 &p_rotation) {
    environment_custom_sky_rotation_degrees = p_rotation;
}

Vector3 BakedLightmap::get_environment_custom_sky_rotation_degrees() const {
    return environment_custom_sky_rotation_degrees;
}

void BakedLightmap::set_environment_custom_color(const Color &p_color) {
    environment_custom_color = p_color;
}
Color BakedLightmap::get_environment_custom_color() const {
    return environment_custom_color;
}

void BakedLightmap::set_environment_custom_energy(float p_energy) {
    environment_custom_energy = p_energy;
}
float BakedLightmap::get_environment_custom_energy() const {
    return environment_custom_energy;
}

void BakedLightmap::set_environment_min_light(Color p_min_light) {
    environment_min_light = p_min_light;
}

Color BakedLightmap::get_environment_min_light() const {
    return environment_min_light;
}

void BakedLightmap::set_bounces(int p_bounces) {
    ERR_FAIL_COND(p_bounces < 0 || p_bounces > 16);
    bounces = p_bounces;
}

int BakedLightmap::get_bounces() const {
    return bounces;
}

void BakedLightmap::set_bounce_indirect_energy(float p_indirect_energy) {
    ERR_FAIL_COND(p_indirect_energy < 0.0);
    bounce_indirect_energy = p_indirect_energy;
}

float BakedLightmap::get_bounce_indirect_energy() const {
    return bounce_indirect_energy;
}

void BakedLightmap::set_bias(float p_bias) {
    ERR_FAIL_COND(p_bias < 0.00001f);
    bias = p_bias;
}

float BakedLightmap::get_bias() const {
    return bias;
}
AABB BakedLightmap::get_aabb() const {
    return AABB(-extents, extents * 2);
}
Vector<Face3> BakedLightmap::get_faces(uint32_t p_usage_flags) const {
    return Vector<Face3>();
}
void BakedLightmap::_validate_property(PropertyInfo &property) const {

    if (StringView(property.name).starts_with("environment_custom_sky") && environment_mode != ENVIRONMENT_MODE_CUSTOM_SKY) {
        property.usage = 0;
    }

    if (property.name == "environment_custom_color" && environment_mode != ENVIRONMENT_MODE_CUSTOM_COLOR) {
        property.usage = 0;
    }

    if (property.name == "environment_custom_energy" && environment_mode != ENVIRONMENT_MODE_CUSTOM_COLOR && environment_mode != ENVIRONMENT_MODE_CUSTOM_SKY) {
        property.usage = 0;
    }

    if (StringView(property.name).starts_with("capture") && property.name != "capture_enabled" && !capture_enabled) {
        property.usage = 0;
    }
}
void BakedLightmap::_bind_methods() {

    SE_BIND_METHOD(BakedLightmap,set_light_data);
    SE_BIND_METHOD(BakedLightmap,get_light_data);

    SE_BIND_METHOD(BakedLightmap,set_bake_quality);
    SE_BIND_METHOD(BakedLightmap,get_bake_quality);

    SE_BIND_METHOD(BakedLightmap,set_bounces);
    SE_BIND_METHOD(BakedLightmap,get_bounces);

    SE_BIND_METHOD(BakedLightmap,set_bounce_indirect_energy);
    SE_BIND_METHOD(BakedLightmap,get_bounce_indirect_energy);

    SE_BIND_METHOD(BakedLightmap,set_bias);
    SE_BIND_METHOD(BakedLightmap,get_bias);

    SE_BIND_METHOD(BakedLightmap,set_environment_mode);
    SE_BIND_METHOD(BakedLightmap,get_environment_mode);

    SE_BIND_METHOD(BakedLightmap,set_environment_custom_sky);
    SE_BIND_METHOD(BakedLightmap,get_environment_custom_sky);

    SE_BIND_METHOD(BakedLightmap,set_environment_custom_sky_rotation_degrees);
    SE_BIND_METHOD(BakedLightmap,get_environment_custom_sky_rotation_degrees);

    SE_BIND_METHOD(BakedLightmap,set_environment_custom_color);
    SE_BIND_METHOD(BakedLightmap,get_environment_custom_color);

    SE_BIND_METHOD(BakedLightmap,set_environment_custom_energy);
    SE_BIND_METHOD(BakedLightmap,get_environment_custom_energy);

    SE_BIND_METHOD(BakedLightmap,set_environment_min_light);
    SE_BIND_METHOD(BakedLightmap,get_environment_min_light);

    SE_BIND_METHOD(BakedLightmap,set_use_denoiser);
    SE_BIND_METHOD(BakedLightmap,is_using_denoiser);

    SE_BIND_METHOD(BakedLightmap,set_use_hdr);
    SE_BIND_METHOD(BakedLightmap,is_using_hdr);

    SE_BIND_METHOD(BakedLightmap,set_use_color);
    SE_BIND_METHOD(BakedLightmap,is_using_color);

    SE_BIND_METHOD(BakedLightmap,set_generate_atlas);
    SE_BIND_METHOD(BakedLightmap,is_generate_atlas_enabled);

    SE_BIND_METHOD(BakedLightmap,set_max_atlas_size);
    SE_BIND_METHOD(BakedLightmap,get_max_atlas_size);

    SE_BIND_METHOD(BakedLightmap,set_capture_quality);
    SE_BIND_METHOD(BakedLightmap,get_capture_quality);

    SE_BIND_METHOD(BakedLightmap,set_extents);
    SE_BIND_METHOD(BakedLightmap,get_extents);

    SE_BIND_METHOD(BakedLightmap,set_default_texels_per_unit);
    SE_BIND_METHOD(BakedLightmap,get_default_texels_per_unit);

    SE_BIND_METHOD(BakedLightmap,set_capture_propagation);
    SE_BIND_METHOD(BakedLightmap,get_capture_propagation);

    SE_BIND_METHOD(BakedLightmap,set_capture_enabled);
    SE_BIND_METHOD(BakedLightmap,get_capture_enabled);

    SE_BIND_METHOD(BakedLightmap,set_capture_cell_size);
    SE_BIND_METHOD(BakedLightmap,get_capture_cell_size);
#ifndef DISABLE_DEPRECATED

    SE_BIND_METHOD(BakedLightmap,set_image_path);
    SE_BIND_METHOD(BakedLightmap,get_image_path);
#endif
    MethodBinder::bind_method(D_METHOD("bake", {"from_node", "data_save_path"}), &BakedLightmap::bake, {DEFVAL(Variant()), DEFVAL("")});

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "extents"), "set_extents", "get_extents");

    ADD_GROUP("Tweaks", "");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "quality", PropertyHint::Enum, "Low,Medium,High,Ultra"), "set_bake_quality", "get_bake_quality");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "bounces", PropertyHint::Range, "0,16,1"), "set_bounces", "get_bounces");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "bounce_indirect_energy", PropertyHint::Range, "0,16,0.01"), "set_bounce_indirect_energy", "get_bounce_indirect_energy");

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_denoiser"), "set_use_denoiser", "is_using_denoiser");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_hdr"), "set_use_hdr", "is_using_hdr");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_color"), "set_use_color", "is_using_color");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "bias", PropertyHint::Range, "0.00001,0.1,0.00001,or_greater"), "set_bias", "get_bias");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "default_texels_per_unit", PropertyHint::Range, "0.0,64.0,0.01,or_greater"), "set_default_texels_per_unit", "get_default_texels_per_unit");

    ADD_GROUP("Atlas", "atlas_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "atlas_generate"), "set_generate_atlas", "is_generate_atlas_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "atlas_max_size"), "set_max_atlas_size", "get_max_atlas_size");

    ADD_GROUP("Environment", "environment_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "environment_mode", PropertyHint::Enum, "Disabled,Scene,Custom Sky,Custom Color"), "set_environment_mode", "get_environment_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "environment_custom_sky", PropertyHint::ResourceType, "Sky"), "set_environment_custom_sky", "get_environment_custom_sky");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "environment_custom_sky_rotation_degrees"), "set_environment_custom_sky_rotation_degrees", "get_environment_custom_sky_rotation_degrees");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "environment_custom_color", PropertyHint::ColorNoAlpha), "set_environment_custom_color", "get_environment_custom_color");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "environment_custom_energy", PropertyHint::Range, "0,64,0.01"), "set_environment_custom_energy", "get_environment_custom_energy");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "environment_min_light", PropertyHint::ColorNoAlpha), "set_environment_min_light", "get_environment_min_light");
    ADD_GROUP("Capture", "capture_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "capture_enabled"), "set_capture_enabled", "get_capture_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "capture_cell_size", PropertyHint::Range, "0.25,2.0,0.05,or_greater"), "set_capture_cell_size", "get_capture_cell_size");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "capture_quality", PropertyHint::Enum, "Low,Medium,High"), "set_capture_quality", "get_capture_quality");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "capture_propagation", PropertyHint::Range, "0,1,0.01"), "set_capture_propagation", "get_capture_propagation");
    ADD_GROUP("Data", "");
#ifndef DISABLE_DEPRECATED
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "image_path", PropertyHint::Dir, "", 0), "set_image_path", "get_image_path");
#endif
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "light_data", PropertyHint::ResourceType, "BakedLightmapData"), "set_light_data", "get_light_data");

    BIND_ENUM_CONSTANT(BAKE_QUALITY_LOW);
    BIND_ENUM_CONSTANT(BAKE_QUALITY_MEDIUM);
    BIND_ENUM_CONSTANT(BAKE_QUALITY_HIGH);
    BIND_ENUM_CONSTANT(BAKE_QUALITY_ULTRA);

    BIND_ENUM_CONSTANT(BAKE_ERROR_OK);
    BIND_ENUM_CONSTANT(BAKE_ERROR_NO_SAVE_PATH);
    BIND_ENUM_CONSTANT(BAKE_ERROR_NO_MESHES);
    BIND_ENUM_CONSTANT(BAKE_ERROR_CANT_CREATE_IMAGE);
    BIND_ENUM_CONSTANT(BAKE_ERROR_LIGHTMAP_SIZE);
    BIND_ENUM_CONSTANT(BAKE_ERROR_INVALID_MESH);
    BIND_ENUM_CONSTANT(BAKE_ERROR_USER_ABORTED);
    BIND_ENUM_CONSTANT(BAKE_ERROR_NO_LIGHTMAPPER);
    BIND_ENUM_CONSTANT(BAKE_ERROR_NO_ROOT);

    BIND_ENUM_CONSTANT(ENVIRONMENT_MODE_DISABLED);
    BIND_ENUM_CONSTANT(ENVIRONMENT_MODE_SCENE);
    BIND_ENUM_CONSTANT(ENVIRONMENT_MODE_CUSTOM_SKY);
    BIND_ENUM_CONSTANT(ENVIRONMENT_MODE_CUSTOM_COLOR);
}

BakedLightmap::BakedLightmap() {

    extents = Vector3(10, 10, 10);

    bake_quality = BAKE_QUALITY_MEDIUM;
    capture_quality = BAKE_QUALITY_MEDIUM;
    image_path = "";
    set_disable_scale(true);
    capture_cell_size = 0.5;
    environment_mode = ENVIRONMENT_MODE_DISABLED;
    environment_custom_color = Color(0.2, 0.7, 1.0);
    environment_custom_energy = 1.0;
    environment_min_light = Color(0.0, 0.0, 0.0);
    bias = 0.005;
    generate_atlas = true;
    max_atlas_size = 4096;
}
