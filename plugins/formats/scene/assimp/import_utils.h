/*************************************************************************/
/*  import_utils.h                                                       */
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

#pragma once

#include "core/io/image_loader.h"
#include "import_state.h"
#include "core/ustring.h"
#include "core/print_string.h"

#include <assimp/SceneCombiner.h>
#include <assimp/cexport.h>
#include <assimp/cimport.h>
#include <assimp/matrix4x4.h>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/Logger.hpp>
#include <string>

using namespace AssimpImporter;

#define AI_PROPERTIES aiTextureType_UNKNOWN, 0
#define AI_NULL 0, 0
#define AI_MATKEY_FBX_MAYA_BASE_COLOR_FACTOR "$raw.Maya|baseColor"
#define AI_MATKEY_FBX_MAYA_METALNESS_FACTOR "$raw.Maya|metalness"
#define AI_MATKEY_FBX_MAYA_DIFFUSE_ROUGHNESS_FACTOR "$raw.Maya|diffuseRoughness"

#define AI_MATKEY_FBX_MAYA_EMISSION_TEXTURE "$raw.Maya|emissionColor|file"
#define AI_MATKEY_FBX_MAYA_EMISSIVE_FACTOR "$raw.Maya|emission"
#define AI_MATKEY_FBX_MAYA_METALNESS_TEXTURE "$raw.Maya|metalness|file"
#define AI_MATKEY_FBX_MAYA_METALNESS_UV_XFORM "$raw.Maya|metalness|uvtrafo"
#define AI_MATKEY_FBX_MAYA_DIFFUSE_ROUGHNESS_TEXTURE "$raw.Maya|diffuseRoughness|file"
#define AI_MATKEY_FBX_MAYA_DIFFUSE_ROUGHNESS_UV_XFORM "$raw.Maya|diffuseRoughness|uvtrafo"
#define AI_MATKEY_FBX_MAYA_BASE_COLOR_TEXTURE "$raw.Maya|baseColor|file"
#define AI_MATKEY_FBX_MAYA_BASE_COLOR_UV_XFORM "$raw.Maya|baseColor|uvtrafo"
#define AI_MATKEY_FBX_MAYA_NORMAL_TEXTURE "$raw.Maya|normalCamera|file"
#define AI_MATKEY_FBX_MAYA_NORMAL_UV_XFORM "$raw.Maya|normalCamera|uvtrafo"

#define AI_MATKEY_FBX_NORMAL_TEXTURE "$raw.Maya|normalCamera|file"
#define AI_MATKEY_FBX_NORMAL_UV_XFORM "$raw.Maya|normalCamera|uvtrafo"

#define AI_MATKEY_FBX_MAYA_STINGRAY_DISPLACEMENT_SCALING_FACTOR "$raw.Maya|displacementscaling"
#define AI_MATKEY_FBX_MAYA_STINGRAY_BASE_COLOR_FACTOR "$raw.Maya|base_color"
#define AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_FACTOR "$raw.Maya|emissive"
#define AI_MATKEY_FBX_MAYA_STINGRAY_METALLIC_FACTOR "$raw.Maya|metallic"
#define AI_MATKEY_FBX_MAYA_STINGRAY_ROUGHNESS_FACTOR "$raw.Maya|roughness"
#define AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_INTENSITY_FACTOR "$raw.Maya|emissive_intensity"

#define AI_MATKEY_FBX_MAYA_STINGRAY_NORMAL_TEXTURE "$raw.Maya|TEX_normal_map|file"
#define AI_MATKEY_FBX_MAYA_STINGRAY_NORMAL_UV_XFORM "$raw.Maya|TEX_normal_map|uvtrafo"
#define AI_MATKEY_FBX_MAYA_STINGRAY_COLOR_TEXTURE "$raw.Maya|TEX_color_map|file"
#define AI_MATKEY_FBX_MAYA_STINGRAY_COLOR_UV_XFORM "$raw.Maya|TEX_color_map|uvtrafo"
#define AI_MATKEY_FBX_MAYA_STINGRAY_METALLIC_TEXTURE "$raw.Maya|TEX_metallic_map|file"
#define AI_MATKEY_FBX_MAYA_STINGRAY_METALLIC_UV_XFORM "$raw.Maya|TEX_metallic_map|uvtrafo"
#define AI_MATKEY_FBX_MAYA_STINGRAY_ROUGHNESS_TEXTURE "$raw.Maya|TEX_roughness_map|file"
#define AI_MATKEY_FBX_MAYA_STINGRAY_ROUGHNESS_UV_XFORM "$raw.Maya|TEX_roughness_map|uvtrafo"
#define AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_TEXTURE "$raw.Maya|TEX_emissive_map|file"
#define AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_UV_XFORM "$raw.Maya|TEX_emissive_map|uvtrafo"
#define AI_MATKEY_FBX_MAYA_STINGRAY_AO_TEXTURE "$raw.Maya|TEX_ao_map|file"
#define AI_MATKEY_FBX_MAYA_STINGRAY_AO_UV_XFORM "$raw.Maya|TEX_ao_map|uvtrafo"

/**
 * Assimp Utils
 * Conversion tools / glue code to convert from assimp to godot
*/
class AssimpUtils {
public:
    /**
     * calculate tangents for mesh data from assimp data
     */
    static void calc_tangent_from_mesh(const aiMesh *ai_mesh, uint32_t i, uint32_t tri_index, uint32_t index, Color *w) {
        const aiVector3D normals = ai_mesh->mAnimMeshes[i]->mNormals[tri_index];
        const Vector3 godot_normal = Vector3(normals.x, normals.y, normals.z);
        const aiVector3D tangent = ai_mesh->mAnimMeshes[i]->mTangents[tri_index];
        const Vector3 godot_tangent = Vector3(tangent.x, tangent.y, tangent.z);
        const aiVector3D bitangent = ai_mesh->mAnimMeshes[i]->mBitangents[tri_index];
        const Vector3 godot_bitangent = Vector3(bitangent.x, bitangent.y, bitangent.z);
        float d = godot_normal.cross(godot_tangent).dot(godot_bitangent) > 0.0f ? 1.0f : -1.0f;
        Color plane_tangent = Color(tangent.x, tangent.y, tangent.z, d);
        w[index] = plane_tangent;
    }

    struct AssetImportFbx {
        enum ETimeMode {
            TIME_MODE_DEFAULT = 0,
            TIME_MODE_120 = 1,
            TIME_MODE_100 = 2,
            TIME_MODE_60 = 3,
            TIME_MODE_50 = 4,
            TIME_MODE_48 = 5,
            TIME_MODE_30 = 6,
            TIME_MODE_30_DROP = 7,
            TIME_MODE_NTSC_DROP_FRAME = 8,
            TIME_MODE_NTSC_FULL_FRAME = 9,
            TIME_MODE_PAL = 10,
            TIME_MODE_CINEMA = 11,
            TIME_MODE_1000 = 12,
            TIME_MODE_CINEMA_ND = 13,
            TIME_MODE_CUSTOM = 14,
            TIME_MODE_TIME_MODE_COUNT = 15
        };
        enum UpAxis {
            UP_VECTOR_AXIS_X = 1,
            UP_VECTOR_AXIS_Y = 2,
            UP_VECTOR_AXIS_Z = 3
        };
        enum FrontAxis {
            FRONT_PARITY_EVEN = 1,
            FRONT_PARITY_ODD = 2,
        };

        enum CoordAxis {
            COORD_RIGHT = 0,
            COORD_LEFT = 1
        };
    };

    /** Get assimp string
    * automatically filters the string data
    */
    static String get_assimp_string(const aiString &p_string) {
        //convert an assimp String to a Godot String
        String name(p_string.C_Str() /*,p_string.length*/);
        if (StringUtils::contains(name,":")) {
            String replaced_name(StringUtils::split(name,':')[1]);
            print_verbose("Replacing " + name + " containing : with " + replaced_name);
            name = replaced_name;
        }

        return name;
    }

    static String get_anim_string_from_assimp(const aiString &p_string) {

        String name(p_string.C_Str() /*,p_string.length*/);
        if (StringUtils::contains(name,":")) {
            String replaced_name(StringUtils::split(name,":")[1]);
            print_verbose("Replacing " + name + " containing : with " + replaced_name);
            name = replaced_name;
        }
        return name;
    }

    /**
     * No filter logic get_raw_string_from_assimp
     * This just convers the aiString to a parsed utf8 string
     * Without removing special chars etc
     */
    static String get_raw_string_from_assimp(const aiString &p_string) {
        return String(p_string.C_Str() ,p_string.length);
    }

    static Ref<Animation> import_animation(se_string_view p_path, uint32_t p_flags, int p_bake_fps) {
        return Ref<Animation>();
    }

    /**
     * Converts aiMatrix4x4 to godot Transform
    */
    static const Transform assimp_matrix_transform(const aiMatrix4x4 p_matrix) {
        aiMatrix4x4 matrix = p_matrix;
        Transform xform;
        xform.set(matrix.a1, matrix.a2, matrix.a3, matrix.b1, matrix.b2, matrix.b3, matrix.c1, matrix.c2, matrix.c3, matrix.a4, matrix.b4, matrix.c4);
        return xform;
    }

    /** Get fbx fps for time mode meta data
     */
    static float get_fbx_fps(int32_t time_mode, const aiScene *p_scene);

    /**
      * Get global transform for the current node - so we can use world space rather than
      * local space coordinates
      * useful if you need global - although recommend using local wherever possible over global
      * as you could break fbx scaling :)
      */
    static Transform _get_global_assimp_node_transform(const aiNode *p_current_node) {
        aiNode const *current_node = p_current_node;
        Transform xform;
        while (current_node != nullptr) {
            xform = assimp_matrix_transform(current_node->mTransformation) * xform;
            current_node = current_node->mParent;
        }
        return xform;
    }

    /**
      * Find hardcoded textures from assimp which could be in many different directories
      */
    static void find_texture_path(se_string_view p_path, DirAccess *dir, String &path, bool &found, const String &extension);

    /** find the texture path for the supplied fbx path inside godot
      * very simple lookup for subfolders etc for a texture which may or may not be in a directory
      */
    static void find_texture_path(const String &r_p_path, String &r_path, bool &r_found);

    /**
      * set_texture_mapping_mode
      * Helper to check the mapping mode of the texture (repeat, clamp and mirror)
      */
    static void set_texture_mapping_mode(aiTextureMapMode *map_mode, Ref<ImageTexture> texture);

    /**
      * Load or load from cache image :)
      */
    static Ref<Image> load_image(ImportState &state, const aiScene *p_scene, se_string_view p_path);

    /* create texture from assimp data, if found in path */
    static bool CreateAssimpTexture(AssimpImporter::ImportState &state,
            const aiString& texture_path,
            String &filename,
            String &path,
            AssimpImageData &image_state);
    /** GetAssimpTexture
      * Designed to retrieve textures for you
      */
    static bool GetAssimpTexture(
            AssimpImporter::ImportState &state,
            aiMaterial *ai_material,
            aiTextureType texture_type,
            String &filename,
            String &path,
            AssimpImageData &image_state) {
        aiString ai_filename = aiString();
        if (AI_SUCCESS == ai_material->GetTexture(texture_type, 0, &ai_filename, nullptr, nullptr, nullptr, nullptr, image_state.map_mode)) {
            return CreateAssimpTexture(state, ai_filename, filename, path, image_state);
        }

        return false;
    }
};
