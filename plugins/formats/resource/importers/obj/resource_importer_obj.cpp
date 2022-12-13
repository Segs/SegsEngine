/*************************************************************************/
/*  resource_importer_obj.cpp                                            */
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

#include "resource_importer_obj.h"

#include "core/io/resource_loader.h"
#include "core/os/file_access.h"
#include "core/string_formatter.h"
#include "core/print_string.h"
#include "core/resource/resource_manager.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/animation.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"
#include "scene/resources/surface_tool.h"
#include "core/string_utils.h"

//IMPL_GDCLASS(EditorOBJImporter)
//IMPL_GDCLASS(ResourceImporterOBJ)

uint32_t ResourceImporterOBJ::get_import_flags() const {

    return IMPORT_SCENE;
}

static Error _parse_material_library(StringView p_path, Map<String, Ref<SpatialMaterial> > &material_map, Vector<String> *r_missing_deps) {

    FileAccessRef f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!f, ERR_CANT_OPEN, FormatVE("Couldn't open MTL file '%.*s', it may not exist or not be readable.", (int)p_path.size(),p_path.data()));

    Ref<SpatialMaterial> current;
    String current_name;
    String base_path = PathUtils::get_base_dir(p_path);
    while (true) {

        String l(StringUtils::strip_edges(f->get_line()));

        if (StringUtils::begins_with(l,"newmtl ")) {
            //vertex

            current_name = StringUtils::strip_edges(StringUtils::replace(l,"newmtl", ""));
            current = make_ref_counted<SpatialMaterial>();
            current->set_name(current_name);
            material_map[current_name] = current;
        } else if (StringUtils::begins_with(l,"Ka ")) {
            //uv
            WARN_PRINT("OBJ: Ambient light for material '" + current_name + "' is ignored in PBR");

        } else if (StringUtils::begins_with(l,"Kd ")) {
            //normal
            ERR_FAIL_COND_V(not current, ERR_FILE_CORRUPT);
            Vector<StringView> v = StringUtils::split(l," ", false);
            ERR_FAIL_COND_V(v.size() < 4, ERR_INVALID_DATA);
            Color c = current->get_albedo();
            c.r = StringUtils::to_float(v[1]);
            c.g = StringUtils::to_float(v[2]);
            c.b = StringUtils::to_float(v[3]);
            current->set_albedo(c);
        } else if (StringUtils::begins_with(l,"Ks ")) {
            //normal
            ERR_FAIL_COND_V(not current, ERR_FILE_CORRUPT);
            Vector<StringView> v = StringUtils::split(l," ", false);
            ERR_FAIL_COND_V(v.size() < 4, ERR_INVALID_DATA);
            float r = StringUtils::to_float(v[1]);
            float g = StringUtils::to_float(v[2]);
            float b = StringUtils::to_float(v[3]);
            float metalness = M_MAX(r, M_MAX(g, b));
            current->set_metallic(metalness);
        } else if (StringUtils::begins_with(l,"Ns ")) {
            //normal
            ERR_FAIL_COND_V(not current, ERR_FILE_CORRUPT);
            Vector<StringView> v = StringUtils::split(l," ", false);
            ERR_FAIL_COND_V(v.size() != 2, ERR_INVALID_DATA);
            float s = StringUtils::to_float(v[1]);
            current->set_metallic((1000.0f - s) / 1000.0f);
        } else if (StringUtils::begins_with(l,"d ")) {
            //normal
            ERR_FAIL_COND_V(not current, ERR_FILE_CORRUPT);
            Vector<StringView> v = StringUtils::split(l," ", false);
            ERR_FAIL_COND_V(v.size() != 2, ERR_INVALID_DATA);
            float d = StringUtils::to_float(v[1]);
            Color c = current->get_albedo();
            c.a = d;
            current->set_albedo(c);
            if (c.a < 0.99) {
                current->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
            }
        } else if (StringUtils::begins_with(l,"Tr ")) {
            //normal
            ERR_FAIL_COND_V(not current, ERR_FILE_CORRUPT);
            Vector<StringView> v = StringUtils::split(l," ", false);
            ERR_FAIL_COND_V(v.size() != 2, ERR_INVALID_DATA);
            float d = StringUtils::to_float(v[1]);
            Color c = current->get_albedo();
            c.a = 1.0f - d;
            current->set_albedo(c);
            if (c.a < 0.99f) {
                current->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
            }

        } else if (StringUtils::begins_with(l,"map_Ka ")) {
            //uv
            WARN_PRINT("OBJ: Ambient light texture for material '" + current_name + "' is ignored in PBR");

        } else if (StringUtils::begins_with(l,"map_Kd ")) {
            //normal
            ERR_FAIL_COND_V(not current, ERR_FILE_CORRUPT);

            String p(StringUtils::strip_edges(StringUtils::replace(StringUtils::replace(l,"map_Kd", ""),"\\", "/")));
            String path;
            if (PathUtils::is_abs_path(p)) {
                path = p;
            } else {
                path = PathUtils::plus_file(base_path,p);
            }

            Ref<Texture> texture(dynamic_ref_cast<Texture>(gResourceManager().load(path)));

            if (texture) {
                current->set_texture(SpatialMaterial::TEXTURE_ALBEDO, texture);
            } else if (r_missing_deps) {
                r_missing_deps->push_back(path);
            }

        } else if (StringUtils::begins_with(l,"map_Ks ")) {
            //normal
            ERR_FAIL_COND_V(not current, ERR_FILE_CORRUPT);

            String p(StringUtils::strip_edges(StringUtils::replace(StringUtils::replace(l,"map_Ks", ""),"\\", "/")));
            String path;
            if (PathUtils::is_abs_path(p)) {
                path = p;
            } else {
                path = PathUtils::plus_file(base_path,p);
            }

            Ref<Texture> texture = dynamic_ref_cast<Texture>(gResourceManager().load(path));

            if (texture) {
                current->set_texture(SpatialMaterial::TEXTURE_METALLIC, texture);
            } else if (r_missing_deps) {
                r_missing_deps->push_back(path);
            }

        } else if (StringUtils::begins_with(l,"map_Ns ")) {
            //normal
            ERR_FAIL_COND_V(not current, ERR_FILE_CORRUPT);

            String p(StringUtils::strip_edges(StringUtils::replace(StringUtils::replace(l,"map_Ns", ""),"\\", "/")));
            String path;
            if (PathUtils::is_abs_path(p)) {
                path = p;
            } else {
                path = PathUtils::plus_file(base_path,p);
            }

            Ref<Texture> texture(dynamic_ref_cast<Texture>(gResourceManager().load(path)));

            if (texture) {
                current->set_texture(SpatialMaterial::TEXTURE_ROUGHNESS, texture);
            } else if (r_missing_deps) {
                r_missing_deps->push_back(path);
            }
        } else if (StringUtils::begins_with(l,"map_bump ")) {
            //normal
            ERR_FAIL_COND_V(not current, ERR_FILE_CORRUPT);

            String p(StringUtils::strip_edges(StringUtils::replace(StringUtils::replace(l,"map_bump", ""),"\\", "/")));
            String path = PathUtils::plus_file(base_path,p);

            Ref<Texture> texture(dynamic_ref_cast<Texture>(gResourceManager().load(path)));

            if (texture) {
                current->set_feature(SpatialMaterial::FEATURE_NORMAL_MAPPING, true);
                current->set_texture(SpatialMaterial::TEXTURE_NORMAL, texture);
            } else if (r_missing_deps) {
                r_missing_deps->push_back(path);
            }
        } else if (f->eof_reached()) {
            break;
        }
    }

    return OK;
}

static Error _parse_obj(StringView p_path, Vector<Ref<Mesh>> &r_meshes, bool p_single_mesh, bool p_generate_tangents,
                        uint32_t p_compress_flags,
         Vector3 p_scale_mesh, Vector3 p_offset_mesh, Vector<String> *r_missing_deps) {

    FileAccessRef f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!f, ERR_CANT_OPEN, FormatVE("Couldn't open OBJ file '%.*s', it may not exist or not be readable.", (int)p_path.size(),p_path.data()));

    Ref<ArrayMesh> mesh(make_ref_counted<ArrayMesh>());

    bool generate_tangents = p_generate_tangents;
    Vector3 scale_mesh = p_scale_mesh;
    Vector3 offset_mesh = p_offset_mesh;


    Vector<Vector3> vertices;
    Vector<Vector3> normals;
    Vector<Vector2> uvs;
    String name;

    Map<String, Map<String, Ref<SpatialMaterial> > > material_map;

    Ref<SurfaceTool> surf_tool(make_ref_counted<SurfaceTool>());
    surf_tool->begin(Mesh::PRIMITIVE_TRIANGLES);

    String current_material_library;
    String current_material;
    String current_group;

    while (true) {

        String l(StringUtils::strip_edges(f->get_line()));
        while (l.length() && l[l.length() - 1] == '\\') {
            String add(StringUtils::strip_edges(f->get_line()));
            l += add;
            if (add.empty()) {
                break;
            }
        }

        if (StringUtils::begins_with(l,"v ")) {
            //vertex
            Vector<StringView> v = StringUtils::split(l," ", false);
            ERR_FAIL_COND_V(v.size() < 4, ERR_FILE_CORRUPT);
            Vector3 vtx;
            vtx.x = StringUtils::to_float(v[1]) * scale_mesh.x + offset_mesh.x;
            vtx.y = StringUtils::to_float(v[2]) * scale_mesh.y + offset_mesh.y;
            vtx.z = StringUtils::to_float(v[3]) * scale_mesh.z + offset_mesh.z;
            vertices.push_back(vtx);
        } else if (StringUtils::begins_with(l,"vt ")) {
            //uv
            Vector<StringView> v = StringUtils::split(l," ", false);
            ERR_FAIL_COND_V(v.size() < 3, ERR_FILE_CORRUPT);
            Vector2 uv;
            uv.x = StringUtils::to_float(v[1]);
            uv.y = 1.0f - StringUtils::to_float(v[2]);
            uvs.push_back(uv);

        } else if (StringUtils::begins_with(l,"vn ")) {
            //normal
            Vector<StringView> v = StringUtils::split(l," ", false);
            ERR_FAIL_COND_V(v.size() < 4, ERR_FILE_CORRUPT);
            Vector3 nrm;
            nrm.x = StringUtils::to_float(v[1]);
            nrm.y = StringUtils::to_float(v[2]);
            nrm.z = StringUtils::to_float(v[3]);
            normals.push_back(nrm);
        } else if (StringUtils::begins_with(l,"f ")) {
            //vertex

            Vector<StringView> v = StringUtils::split(l," ", false);
            ERR_FAIL_COND_V(v.size() < 4, ERR_FILE_CORRUPT);

            //not very fast, could be sped up

            Vector<StringView> face[3];
            face[0] = StringUtils::split(v[1],"/");
            face[1] = StringUtils::split(v[2],"/");
            ERR_FAIL_COND_V(face[0].empty(), ERR_FILE_CORRUPT);

            ERR_FAIL_COND_V(face[0].size() != face[1].size(), ERR_FILE_CORRUPT);
            for (size_t i = 2; i < v.size() - 1; i++) {

                face[2] = StringUtils::split(v[i + 1],"/");

                ERR_FAIL_COND_V(face[0].size() != face[2].size(), ERR_FILE_CORRUPT);
                for (int j = 0; j < 3; j++) {

                    int idx = j;

                    if (idx < 2) {
                        idx = 1 ^ idx;
                    }

                    if (face[idx].size() == 3) {
                        int norm = StringUtils::to_int(face[idx][2]) - 1;
                        if (norm < 0)
                            norm += normals.size() + 1;
                        ERR_FAIL_INDEX_V(norm, normals.size(), ERR_FILE_CORRUPT);
                        surf_tool->add_normal(normals[norm]);
                    }

                    if (face[idx].size() >= 2 && !face[idx][1].empty()) {
                        int uv = StringUtils::to_int(face[idx][1]) - 1;
                        if (uv < 0)
                            uv += uvs.size() + 1;
                        ERR_FAIL_INDEX_V(uv, uvs.size(), ERR_FILE_CORRUPT);
                        surf_tool->add_uv(uvs[uv]);
                    }

                    int vtx = StringUtils::to_int(face[idx][0]) - 1;
                    if (vtx < 0)
                        vtx += vertices.size() + 1;
                    ERR_FAIL_INDEX_V(vtx, vertices.size(), ERR_FILE_CORRUPT);

                    Vector3 vertex = vertices[vtx];
                    //if (weld_vertices)
                    //  vertex.snap(Vector3(weld_tolerance, weld_tolerance, weld_tolerance));
                    surf_tool->add_vertex(vertex);
                }

                face[1] = face[2];
            }
        } else if (StringUtils::begins_with(l,"s ")) { //smoothing
            StringView what = StringUtils::strip_edges(StringUtils::substr(l,2, l.length()));
            if (what == StringView("off"))
                surf_tool->add_smooth_group(false);
            else
                surf_tool->add_smooth_group(true);
        } else if (/*StringUtils::begins_with(l,"g ") ||*/ StringUtils::begins_with(l,"usemtl ") || (StringUtils::begins_with(l,"o ") || f->eof_reached())) { //commit group to mesh
            //groups are too annoying
            if (!surf_tool->get_vertex_array().empty()) {
                //another group going on, commit it
                if (normals.empty()) {
                    surf_tool->generate_normals();
                }

                if (generate_tangents && !uvs.empty()) {
                    surf_tool->generate_tangents();
                }

                surf_tool->index();

                print_verbose("OBJ: Current material library " + current_material_library + " has " + itos(material_map.contains(current_material_library)));
                print_verbose("OBJ: Current material " + current_material + " has " + itos(material_map.contains(current_material_library) && material_map[current_material_library].contains(current_material)));

                if (material_map.contains(current_material_library) && material_map[current_material_library].contains(current_material)) {
                    surf_tool->set_material(material_map[current_material_library][current_material]);
                }

                mesh = surf_tool->commit(mesh, p_compress_flags);

                if (!current_material.empty()) {
                    mesh->surface_set_name(mesh->get_surface_count() - 1, PathUtils::get_basename(current_material));
                } else if (!current_group.empty()) {
                    mesh->surface_set_name(mesh->get_surface_count() - 1, current_group);
                }

                print_verbose("OBJ: Added surface :" + mesh->surface_get_name(mesh->get_surface_count() - 1));
                surf_tool->clear();
                surf_tool->begin(Mesh::PRIMITIVE_TRIANGLES);
            }

            if (StringUtils::begins_with(l,"o ") || f->eof_reached()) {

                if (!p_single_mesh) {
                    mesh->set_name(name);
                    r_meshes.push_back(mesh);
                    mesh = (make_ref_counted<ArrayMesh>());
                    current_group = "";
                    current_material = "";
                }
            }

            if (f->eof_reached()) {
                break;
            }

            if (StringUtils::begins_with(l,"o ")) {
                name = StringUtils::strip_edges(StringUtils::substr(l,2, l.length()));
            }

            if (StringUtils::begins_with(l,"usemtl ")) {

                current_material = StringUtils::strip_edges(StringUtils::replace(l,"usemtl", ""));
            }

            if (StringUtils::begins_with(l,"g ")) {

                current_group = StringUtils::strip_edges(StringUtils::substr(l,2, l.length()));
            }

        } else if (StringUtils::begins_with(l,"mtllib ")) { //parse material

            current_material_library = StringUtils::strip_edges(StringUtils::replace(l,"mtllib", ""));
            if (!material_map.contains(current_material_library)) {
                Map<String, Ref<SpatialMaterial> > lib;
                Error err = _parse_material_library(current_material_library, lib, r_missing_deps);
                if (err == ERR_CANT_OPEN) {
                    StringView dir = PathUtils::get_base_dir(p_path);
                    err = _parse_material_library(PathUtils::plus_file(dir,current_material_library), lib, r_missing_deps);
                }
                if (err == OK) {
                    material_map[current_material_library] = lib;
                }
            }
        }
    }

    if (p_single_mesh) {

        r_meshes.push_back(mesh);
    }

    return OK;
}

Node *ResourceImporterOBJ::import_scene(StringView p_path, uint32_t p_flags, int p_bake_fps, uint32_t p_compress_flags, Vector<String> *r_missing_deps, Error *r_err) {

    Vector<Ref<Mesh> > meshes;

    Error err = _parse_obj(p_path, meshes, false, p_flags &IMPORT_GENERATE_TANGENT_ARRAYS, p_compress_flags,
            Vector3(1, 1, 1), Vector3(0, 0, 0), r_missing_deps);

    if (err != OK) {
        if (r_err) {
            *r_err = err;
        }
        return nullptr;
    }

    Node3D *scene = memnew(Node3D);

    for (const Ref<Mesh> &E : meshes) {

        MeshInstance3D *mi = memnew(MeshInstance3D);
        mi->set_mesh(E);
        mi->set_name(E->get_name());
        scene->add_child(mi);
        mi->set_owner(scene);
    }

    if (r_err) {
        *r_err = OK;
    }

    return scene;
}
Ref<Animation> ResourceImporterOBJ::import_animation(StringView p_path, uint32_t p_flags, int p_bake_fps) {

    return Ref<Animation>();
}

void ResourceImporterOBJ::get_extensions(Vector<String> &r_extensions) const {

    r_extensions.push_back("obj");
}

ResourceImporterOBJ::ResourceImporterOBJ() {
}
////////////////////////////////////////////////////

const char *ResourceImporterOBJ::get_importer_name() const {
    return "wavefront_obj";
}
const char *ResourceImporterOBJ::get_visible_name() const {
    return "OBJ As Mesh";
}
void ResourceImporterOBJ::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.push_back("obj");
}
StringName ResourceImporterOBJ::get_save_extension() const {
    return "mesh";
}
StringName ResourceImporterOBJ::get_resource_type() const {
    return "Mesh";
}

int ResourceImporterOBJ::get_preset_count() const {
    return 0;
}
StringName ResourceImporterOBJ::get_preset_name(int /*p_idx*/) const {
    return "";
}

void ResourceImporterOBJ::get_import_options(
        Vector<ResourceImporterInterface::ImportOption> *r_options, int p_preset) const {

    r_options->emplace_back(PropertyInfo(VariantType::BOOL, "generate_tangents"), true);
    r_options->emplace_back(PropertyInfo(VariantType::VECTOR3, "scale_mesh"), Vector3(1, 1, 1));
    r_options->emplace_back(PropertyInfo(VariantType::VECTOR3, "offset_mesh"), Vector3(0, 0, 0));
    r_options->emplace_back(PropertyInfo(VariantType::BOOL, "optimize_mesh"), true);
    r_options->emplace_back(PropertyInfo(VariantType::BOOL, "octahedral_compression"), true);
    r_options->emplace_back(PropertyInfo(VariantType::INT, "optimize_mesh_flags", PropertyHint::Flags,
                                    "Vertex,Normal,Tangent,Color,TexUV,TexUV2,Bones,Weights,Index"),
            RS::ARRAY_COMPRESS_DEFAULT >> RS::ARRAY_COMPRESS_BASE);
}
bool ResourceImporterOBJ::get_option_visibility(const StringName &p_option, const HashMap<StringName, Variant> &p_options) const {

    return true;
}

Error ResourceImporterOBJ::import(StringView p_source_file, StringView p_save_path, const HashMap<StringName, Variant> &p_options, Vector<String> &r_missing_deps, Vector<String>
        *r_platform_variants, Vector<String> *r_gen_files, Variant *r_metadata) {

    Vector<Ref<Mesh> > meshes;

    uint32_t compress_flags = p_options.at(StringName("optimize_mesh_flags")).as<int>() << RS::ARRAY_COMPRESS_BASE;

    if (p_options.at(StringName("octahedral_compression")).as<bool>()) {
        compress_flags |= RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION;
    }
    Error err = _parse_obj(p_source_file, meshes, true, p_options.at("generate_tangents").as<bool>(), compress_flags,
            p_options.at("scale_mesh").as<Vector3>(), p_options.at("offset_mesh").as<Vector3>(), nullptr);

    ERR_FAIL_COND_V(err != OK, err);
    ERR_FAIL_COND_V(meshes.size() != 1, ERR_BUG);

    String save_path = String(p_save_path) + ".mesh";

    err = gResourceManager().save(save_path, meshes.front());

    ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save Mesh to file '" + save_path + "'.");

    r_gen_files->push_back(save_path);

    return OK;
}
