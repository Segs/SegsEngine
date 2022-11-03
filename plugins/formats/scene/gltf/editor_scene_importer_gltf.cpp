/*************************************************************************/
/*  editor_scene_importer_gltf.cpp                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "editor_scene_importer_gltf.h"

#include "core/crypto/crypto_core.h"
#include "core/io/image_loader.h"
#include "core/io/json.h"
#include "core/print_string.h"
#include "core/io/resource_loader.h"
#include "core/math/disjoint_set.h"
#include "core/math/math_defs.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/string_formatter.h"
#include "core/resource/resource_manager.h"
#include "scene/3d/bone_attachment_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/surface_tool.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "scene/resources/material.h"
#include "scene/resources/texture.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/light_3d.h"

#include "EASTL/sort.h"
#include "EASTL/deque.h"


#include <QtCore/QRegularExpression>


namespace {
    struct VariantAutoCaster {
        const Variant& from;
        constexpr VariantAutoCaster(const Variant& src) : from(src) {}
        template<class T>
        operator T() const {
            return from.as<T>();
        }
    };
    /*******************************************************************************************************/
    // Types
    /*******************************************************************************************************/

    using GLTFAccessorIndex = int;
    using GLTFAnimationIndex = int;
    using GLTFBufferIndex = int;
    using GLTFBufferViewIndex = int;
    using GLTFCameraIndex = int;
    using GLTFImageIndex = int;
    using GLTFMaterialIndex = int;
    using GLTFMeshIndex = int;
    using GLTFLightIndex = int;
    using GLTFNodeIndex = int;
    using GLTFSkeletonIndex = int;
    using GLTFSkinIndex = int;
    using GLTFTextureIndex = int;

    enum {
        ARRAY_BUFFER = 34962,
        ELEMENT_ARRAY_BUFFER = 34963,

        TYPE_BYTE = 5120,
        TYPE_UNSIGNED_BYTE = 5121,
        TYPE_SHORT = 5122,
        TYPE_UNSIGNED_SHORT = 5123,
        TYPE_UNSIGNED_INT = 5125,
        TYPE_FLOAT = 5126,

        COMPONENT_TYPE_BYTE = 5120,
        COMPONENT_TYPE_UNSIGNED_BYTE = 5121,
        COMPONENT_TYPE_SHORT = 5122,
        COMPONENT_TYPE_UNSIGNED_SHORT = 5123,
        COMPONENT_TYPE_INT = 5125,
        COMPONENT_TYPE_FLOAT = 5126,

    };
    enum GLTFType {
        TYPE_SCALAR,
        TYPE_VEC2,
        TYPE_VEC3,
        TYPE_VEC4,
        TYPE_MAT2,
        TYPE_MAT3,
        TYPE_MAT4,
    };

    struct GLTFNode {
        //matrices need to be transformed to this
        GLTFNodeIndex parent = -1;
        int height = -1;

        Transform xform;
        StringName name;

        GLTFMeshIndex mesh = -1;
        GLTFCameraIndex camera = -1;
        GLTFSkinIndex skin = -1;

        GLTFSkeletonIndex skeleton = -1;
        bool joint = false;

        Vector3 translation = Vector3(0, 0, 0);
        Quat rotation;
        Vector3 scale = Vector3(1, 1, 1);

        Vector<int> children;
        GLTFLightIndex light = -1;
    };

    struct GLTFBufferView {

        GLTFBufferIndex buffer;
        int byte_offset;
        int byte_length;
        int byte_stride;
        bool indices;
        //matrices need to be transformed to this
        GLTFBufferView() :
            buffer(-1),
            byte_offset(0),
            byte_length(0),
            byte_stride(0),
            indices(false) {
        }
    };

    struct GLTFAccessor {

        GLTFBufferViewIndex buffer_view = 0;
        int byte_offset = 0;
        int component_type = 0;
        bool normalized = false;
        int count = 0;
        GLTFType type = TYPE_SCALAR;
        float min = 0;
        float max = 0;
        int sparse_count = 0;
        int sparse_indices_buffer_view = 0;
        int sparse_indices_byte_offset = 0;
        int sparse_indices_component_type;
        int sparse_values_buffer_view = 0;
        int sparse_values_byte_offset = 0;
    };
    struct GLTFTexture {
        GLTFImageIndex src_image;
    };

    struct GLTFSkeleton {
        // The *synthesized* skeletons joints
        Vector<GLTFNodeIndex> joints;

        // The roots of the skeleton. If there are multiple, each root must have the same parent
        // (ie roots are siblings)
        Vector<GLTFNodeIndex> roots;

        // The created Skeleton for the scene
        Skeleton* godot_skeleton;

        // Set of unique bone names for the skeleton
        Set<String> unique_names;

        GLTFSkeleton() :
            godot_skeleton(nullptr) {
        }
    };

    struct GLTFSkin {
        String name;

        // The "skeleton" property defined in the gltf spec. -1 = Scene Root
        GLTFNodeIndex skin_root;

        Vector<GLTFNodeIndex> joints_original;
        Vector<Transform> inverse_binds;

        // Note: joints + non_joints should form a complete subtree, or subtrees with a common parent

        // All nodes that are skins that are caught in-between the original joints
        // (inclusive of joints_original)
        Vector<GLTFNodeIndex> joints;

        // All Nodes that are caught in-between skin joint nodes, and are not defined
        // as joints by any skin
        Vector<GLTFNodeIndex> non_joints;

        // The roots of the skin. In the case of multiple roots, their parent *must*
        // be the same (the roots must be siblings)
        Vector<GLTFNodeIndex> roots;

        // The GLTF Skeleton this Skin points to (after we determine skeletons)
        GLTFSkeletonIndex skeleton;

        // A mapping from the joint indices (in the order of joints_original) to the
        // Godot Skeleton's bone_indices
        HashMap<int, int> joint_i_to_bone_i;
        Map<int, StringName> joint_i_to_name;

        // The Actual Skin that will be created as a mapping between the IBM's of this skin
        // to the generated skeleton for the mesh instances.
        Ref<Skin> godot_skin;

        GLTFSkin() :
            skin_root(-1),
            skeleton(-1) {}
    };

    struct GLTFMesh {
        Ref<ArrayMesh> mesh;
        Vector<float> blend_weights;
    };

    struct GLTFCamera {

        bool perspective=true;
        float fov_size=65.0f;
        float zfar=500.0f;
        float znear=0.1f;
    };
    struct GLTFLight {
        Color color = Color(1.0f, 1.0f, 1.0f);
        float intensity = 1.0f;
        String type = "";
        float range = Math_INF;
        float inner_cone_angle = 0.0f;
        float outer_cone_angle = Math_PI / 4.0f;
    };
    struct GLTFAnimation {

        enum Interpolation {
            INTERP_LINEAR,
            INTERP_STEP,
            INTERP_CATMULLROMSPLINE,
            INTERP_CUBIC_SPLINE
        };

        template <class T>
        struct Channel {
            Interpolation interpolation;
            Vector<float> times;
            Vector<T> values;
        };

        struct Track {

            Channel<Vector3> translation_track;
            Channel<Quat> rotation_track;
            Channel<Vector3> scale_track;
            Vector<Channel<float> > weight_tracks;
        };

        String name;

        HashMap<int, Track> tracks;
        bool loop = false;
    };
    struct GLTFState {

        Dictionary json;
        int major_version;
        int minor_version;
        Vector<uint8_t> glb_data;

        Vector<GLTFNode*> nodes;
        Vector<Vector<uint8_t> > buffers;
        Vector<GLTFBufferView> buffer_views;
        Vector<GLTFAccessor> accessors;

        Vector<GLTFMesh> meshes; //meshes are loaded directly, no reason not to.
        Vector<Ref<Material> > materials;

        String scene_name;
        Vector<int> root_nodes;

        Vector<GLTFTexture> textures;
        Vector<Ref<Texture> > images;

        Vector<GLTFSkin> skins;
        Vector<GLTFCamera> cameras;
        Vector<GLTFLight> lights;

        Set<String> unique_names;
        Set<String> unique_animation_names;

        Vector<GLTFSkeleton> skeletons;
        Vector<GLTFAnimation> animations;

        HashMap<GLTFNodeIndex, Node*> scene_nodes;

        bool use_named_skin_binds;
        bool use_legacy_names;

        ~GLTFState() {
            for (int i = 0; i < nodes.size(); i++) {
                memdelete(nodes[i]);
            }
        }
    };
    /*******************************************************************************************************/
    // Function Prototypes
    /*******************************************************************************************************/
    void _compute_node_heights(GLTFState& state);
    Error _reparent_non_joint_skeleton_subtrees(GLTFState& state, GLTFSkeleton& skeleton, const Vector<GLTFNodeIndex>& non_joints);
    Error _determine_skeleton_roots(GLTFState& state, const GLTFSkeletonIndex skel_i);
    Error _map_skin_joints_indices_to_skeleton_bone_indices(GLTFState& state);
    void _remove_duplicate_skins(GLTFState& state);
    /*******************************************************************************************************/
    // Implementation
    /*******************************************************************************************************/


    template <class T>
    struct EditorSceneImporterGLTFInterpolate {

        T lerp(const T& a, const T& b, float c) const {
            return a + (b - a) * c;
        }

        T catmull_rom(const T& p0, const T& p1, const T& p2, const T& p3, float t) {

            const float t2 = t * t;
            const float t3 = t2 * t;

            return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4 * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
        }

        T bezier(T start, T control_1, T control_2, T end, float t) {
            /* Formula from Wikipedia article on Bezier curves. */
            const real_t omt = (1.0f - t);
            const real_t omt2 = omt * omt;
            const real_t omt3 = omt2 * omt;
            const real_t t2 = t * t;
            const real_t t3 = t2 * t;

            return start * omt3 + control_1 * omt2 * t * 3.0f + control_2 * omt * t2 * 3.0f + end * t3;
        }
    };

    template <class T>
    T _interpolate_track(const Vector<float>& p_times, const Vector<T>& p_values, const float p_time, const GLTFAnimation::Interpolation p_interp) {

        //could use binary search, worth it?
        int idx = -1;
        for (float t : p_times) {
            if (t > p_time)
                break;
            idx++;
        }

        EditorSceneImporterGLTFInterpolate<T> interp;

        switch (p_interp) {
        case GLTFAnimation::INTERP_LINEAR: {

            if (idx == -1) {
                return p_values[0];
            }
            else if (idx >= p_times.size() - 1) {
                return p_values[p_times.size() - 1];
            }

            const float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

            return interp.lerp(p_values[idx], p_values[idx + 1], c);

        } break;
        case GLTFAnimation::INTERP_STEP: {

            if (idx == -1) {
                return p_values[0];
            }
            else if (idx >= p_times.size() - 1) {
                return p_values[p_times.size() - 1];
            }

            return p_values[idx];

        } break;
        case GLTFAnimation::INTERP_CATMULLROMSPLINE: {

            if (idx == -1) {
                return p_values[1];
            }
            else if (idx >= p_times.size() - 1) {
                return p_values[1 + p_times.size() - 1];
            }

            const float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

            return interp.catmull_rom(p_values[idx - 1], p_values[idx], p_values[idx + 1], p_values[idx + 3], c);

        } break;
        case GLTFAnimation::INTERP_CUBIC_SPLINE: {

            if (idx == -1) {
                return p_values[1];
            }
            else if (idx >= p_times.size() - 1) {
                return p_values[(p_times.size() - 1) * 3 + 1];
            }

            const float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

            const T from = p_values[idx * 3 + 1];
            const T c1 = from + p_values[idx * 3 + 2];
            const T to = p_values[idx * 3 + 4];
            const T c2 = to + p_values[idx * 3 + 3];

            return interp.bezier(from, c1, c2, to, c);

        } break;
        }

        ERR_FAIL_V(p_values[0]);
    }

    const char* _get_component_type_name(const uint32_t p_component) {

        switch (p_component) {
        case COMPONENT_TYPE_BYTE: return "Byte";
        case COMPONENT_TYPE_UNSIGNED_BYTE: return "UByte";
        case COMPONENT_TYPE_SHORT: return "Short";
        case COMPONENT_TYPE_UNSIGNED_SHORT: return "UShort";
        case COMPONENT_TYPE_INT: return "Int";
        case COMPONENT_TYPE_FLOAT: return "Float";
        }

        return "<Error>";
    }

    const char* _get_type_name(const GLTFType p_component) {

        static const char* names[] = {
            "float",
            "vec2",
            "vec3",
            "vec4",
            "mat2",
            "mat3",
            "mat4"
        };

        return names[p_component];
    }

    Error _decode_buffer_view(GLTFState& state, double* dst, const GLTFBufferViewIndex p_buffer_view, const int skip_every, const int skip_bytes, const int element_size, const int count, const GLTFType type, const int component_count, const int component_type, const int component_size, const bool normalized, const int byte_offset, const bool for_vertex) {

        const GLTFBufferView& bv = state.buffer_views[p_buffer_view];

        int stride = bv.byte_stride ? bv.byte_stride : element_size;
        if (for_vertex && stride % 4) {
            stride += 4 - (stride % 4); //according to spec must be multiple of 4
        }

        ERR_FAIL_INDEX_V(bv.buffer, state.buffers.size(), ERR_PARSE_ERROR);

        const uint32_t offset = bv.byte_offset + byte_offset;
        const Vector<uint8_t>& buffer = state.buffers[bv.buffer]; //copy on write, so no performance hit
        const uint8_t* bufptr = buffer.data();

        //use to debug
        print_verbose(String("glTF: type ") + _get_type_name(type) + " component type: " + _get_component_type_name(component_type) + " stride: " + itos(stride) + " amount " + itos(count));
        print_verbose("glTF: accessor offset" + itos(byte_offset) + " view offset: " + itos(bv.byte_offset) + " total buffer len: " + itos(buffer.size()) + " view len " + itos(bv.byte_length));

        const int buffer_end = (stride * (count - 1)) + element_size;
        ERR_FAIL_COND_V(buffer_end > bv.byte_length, ERR_PARSE_ERROR);

        ERR_FAIL_COND_V((int)(offset + buffer_end) > buffer.size(), ERR_PARSE_ERROR);

        //fill everything as doubles

        for (int i = 0; i < count; i++) {

            const uint8_t* src = &bufptr[offset + i * stride];

            for (int j = 0; j < component_count; j++) {

                if (skip_every && j > 0 && (j % skip_every) == 0) {
                    src += skip_bytes;
                }

                double d = 0;

                switch (component_type) {
                case COMPONENT_TYPE_BYTE: {
                    int8_t b = int8_t(*src);
                    if (normalized) {
                        d = (double(b) / 128.0);
                    }
                    else {
                        d = double(b);
                    }
                } break;
                case COMPONENT_TYPE_UNSIGNED_BYTE: {
                    uint8_t b = *src;
                    if (normalized) {
                        d = (double(b) / 255.0);
                    }
                    else {
                        d = double(b);
                    }
                } break;
                case COMPONENT_TYPE_SHORT: {
                    int16_t s = *(int16_t*)src;
                    if (normalized) {
                        d = (double(s) / 32768.0);
                    }
                    else {
                        d = double(s);
                    }
                } break;
                case COMPONENT_TYPE_UNSIGNED_SHORT: {
                    uint16_t s = *(uint16_t*)src;
                    if (normalized) {
                        d = (double(s) / 65535.0);
                    }
                    else {
                        d = double(s);
                    }

                } break;
                case COMPONENT_TYPE_INT: {
                    d = *(int*)src;
                } break;
                case COMPONENT_TYPE_FLOAT: {
                    d = *(float*)src;
                } break;
                }

                *dst++ = d;
                src += component_size;
            }
        }

        return OK;
    }
    Error _parse_json(StringView p_path, GLTFState& state) {

        Error err;
        FileAccessRef f = FileAccess::open(p_path, FileAccess::READ, &err);
        if (!f) {
            return err;
        }
        uint64_t sz= f->get_len();
        auto val=eastl::make_unique<uint8_t[]>(sz);
        f->get_buffer(val.get(), sz);
        String text((const char*)val.get(), f->get_len());

        String err_txt;
        int err_line;
        Variant v;
        err = JSON::parse(text, v, err_txt, err_line);
        if (err != OK) {
            _err_print_error("", String(p_path).c_str(), err_line, err_txt, {}, ERR_HANDLER_SCRIPT);
            return err;
        }
        state.json = v.as<Dictionary>();

        return OK;
    }

    Error _parse_glb(StringView p_path, GLTFState& state) {

        Error err;
        FileAccessRef f = FileAccess::open(p_path, FileAccess::READ, &err);
        if (!f) {
            return err;
        }

        uint32_t magic = f->get_32();
        ERR_FAIL_COND_V(magic != 0x46546C67, ERR_FILE_UNRECOGNIZED); //glTF
        f->get_32(); // version
        f->get_32(); // length

        uint32_t chunk_length = f->get_32();
        uint32_t chunk_type = f->get_32();

        ERR_FAIL_COND_V(chunk_type != 0x4E4F534A, ERR_PARSE_ERROR); //JSON
        String text;
        text.resize(chunk_length);
        uint32_t len = f->get_buffer((uint8_t *)text.data(), chunk_length);
        ERR_FAIL_COND_V(len != chunk_length, ERR_FILE_CORRUPT);

        String err_txt;
        int err_line;
        Variant v;
        err = JSON::parse(text, v, err_txt, err_line);
        if (err != OK) {
            _err_print_error("", String(p_path).c_str(), err_line, err_txt, {}, ERR_HANDLER_SCRIPT);
            return err;
        }

        state.json = v.as<Dictionary>();

        //data?

        chunk_length = f->get_32();
        chunk_type = f->get_32();

        if (f->eof_reached()) {
            return OK; //all good
        }

        ERR_FAIL_COND_V(chunk_type != 0x004E4942, ERR_PARSE_ERROR); //BIN

        state.glb_data.resize(chunk_length);
        len = f->get_buffer(state.glb_data.data(), chunk_length);
        ERR_FAIL_COND_V(len != chunk_length, ERR_FILE_CORRUPT);

        return OK;
    }

    static Vector3 _arr_to_vec3(const Array& p_array) {
        ERR_FAIL_COND_V(p_array.size() != 3, Vector3());
        return Vector3(VariantAutoCaster(p_array[0]), VariantAutoCaster(p_array[1]), VariantAutoCaster(p_array[2]));
    }

    static Quat _arr_to_quat(const Array& p_array) {
        ERR_FAIL_COND_V(p_array.size() != 4, Quat());
        return Quat(VariantAutoCaster(p_array[0]), VariantAutoCaster(p_array[1]), VariantAutoCaster(p_array[2]), VariantAutoCaster(p_array[3]));
    }

    static Transform _arr_to_xform(const Array& p_array) {
        ERR_FAIL_COND_V(p_array.size() != 16, Transform());

        Transform xform;
        xform.basis.set_axis(Vector3::AXIS_X, Vector3(VariantAutoCaster(p_array[0]), VariantAutoCaster(p_array[1]), VariantAutoCaster(p_array[2])));
        xform.basis.set_axis(Vector3::AXIS_Y, Vector3(VariantAutoCaster(p_array[4]), VariantAutoCaster(p_array[5]), VariantAutoCaster(p_array[6])));
        xform.basis.set_axis(Vector3::AXIS_Z, Vector3(VariantAutoCaster(p_array[8]), VariantAutoCaster(p_array[9]), VariantAutoCaster(p_array[10])));
        xform.set_origin(Vector3(VariantAutoCaster(p_array[12]), VariantAutoCaster(p_array[13]), VariantAutoCaster(p_array[14])));

        return xform;
    }

    static String _sanitize_scene_name(GLTFState &state, StringView name) {
        if (state.use_legacy_names) {
        QRegularExpression regex("([^a-zA-Z0-9_ -]+)");
        UIString p_name = QString::fromUtf8(name.data(),name.size()).replace(regex, "");
        return StringUtils::to_utf8(p_name);
        } else {
            String res(name);
            Node::_validate_node_name(res);
            return res;
        }
    }
    static String _legacy_validate_node_name(StringView name) {
        String res(name);
        Node::_validate_node_name(res);
        return res;
    }

    String _gen_unique_name(GLTFState& state, StringView p_name) {

        const String s_name = _sanitize_scene_name(state,p_name);

        String name;
        int index = 1;
        while (true) {
            name = s_name;

            if (index > 1) {
                if (state.use_legacy_names) {
                    name += " ";
                }
                name += itos(index);
            }
            if (!state.unique_names.contains(name)) {
                break;
            }
            index++;
        }

        state.unique_names.insert(name);

        return name;
    }

    String _sanitize_animation_name(GLTFState& state,const String &p_name) {
        // Animations disallow the normal node invalid characters as well as  "," and "["
        // (See animation/animation_player.cpp::add_animation)

        // TODO: Consider adding invalid_characters or a _validate_animation_name to animation_player to mirror Node.
        String name = _sanitize_scene_name(state,p_name);
        name = name.replaced(",", "");
        name = name.replaced("[", "");
        return name;
    }

    String _gen_unique_animation_name(GLTFState &state, const String &p_name) {

        const String s_name = _sanitize_animation_name(state,p_name);

        String name;
        int index = 1;
        while (true) {
            name = s_name;

            if (index > 1) {
                name += itos(index);
            }
            if (!state.unique_animation_names.contains(name)) {
                break;
            }
            index++;
        }

        state.unique_animation_names.insert(name);

        return name;
    }

    String _sanitize_bone_name(GLTFState &state,const StringView name) {
        if (state.use_legacy_names) {
        String p_name = StringUtils::camelcase_to_underscore(name, true);
        QString val(UIString::fromUtf8(name.data(),name.size()));

        QRegularExpression pattern_nocolon(":");
        val.replace(pattern_nocolon, "_");

        QRegularExpression pattern_noslash("/");
        val.replace(pattern_noslash, "_");

        QRegularExpression pattern_nospace(" +");
        val.replace(pattern_nospace, "_");

        QRegularExpression pattern_multiple("_+");
        val.replace(pattern_multiple, "_");

        QRegularExpression pattern_padded("0+(\\d+)");
        val.replace(pattern_padded, "$1");

        return StringUtils::to_utf8(val);
        } else {
            String res_name(res_name);
            res_name = res_name.replaced(":", "_");
            res_name = res_name.replaced("/", "_");
            if (res_name.empty()) {
                res_name = "bone";
            }
            return res_name;
        }
    }
    String _gen_unique_bone_name(GLTFState& state, const GLTFSkeletonIndex skel_i, StringView p_name) {

        String s_name = _sanitize_bone_name(state,p_name);
        String name;
        int index = 1;
        while (true) {
            name = s_name;

            if (index > 1) {
                name += "_" + itos(index);
            }
            if (!state.skeletons[skel_i].unique_names.contains(name)) {
                break;
            }
            index++;
        }

        state.skeletons[skel_i].unique_names.insert(name);

        return name;
    }
    Error _parse_scenes(GLTFState& state) {

        ERR_FAIL_COND_V(!state.json.has("scenes"), ERR_FILE_CORRUPT);
        const Array scenes = state.json["scenes"].as<Array>();
        int loaded_scene = 0;
        if (state.json.has("scene")) {
            loaded_scene = state.json["scene"].as<int>();
        }
        else {
            WARN_PRINT("The load-time scene is not defined in the glTF2 file. Picking the first scene.");
        }

        if (!scenes.empty()) {
            ERR_FAIL_COND_V(loaded_scene >= scenes.size(), ERR_FILE_CORRUPT);
            Dictionary s = scenes[loaded_scene].as<Dictionary>();
            ERR_FAIL_COND_V(!s.has("nodes"), ERR_UNAVAILABLE);
            Array nodes = s["nodes"].as<Array>();
            for (int j = 0; j < nodes.size(); j++) {
                state.root_nodes.push_back(nodes[j].as<int>());
            }

            if (s.has("name") && s["name"] != "") {
                state.scene_name = _gen_unique_name(state, s["name"].as<String>());
            }
            else {
                state.scene_name = _gen_unique_name(state, "Scene");
            }
        }

        return OK;
    }

    Error _parse_nodes(GLTFState& state) {

        ERR_FAIL_COND_V(!state.json.has("nodes"), ERR_FILE_CORRUPT);
        const Array nodes = state.json["nodes"].as<Array>();
        for (int i = 0; i < nodes.size(); i++) {

            GLTFNode* node = memnew(GLTFNode);
            const Dictionary n = nodes[i].as<Dictionary>();

            if (n.has("name")) {
                node->name = n["name"].as<StringName>();
            }
            if (n.has("camera")) {
                node->camera = n["camera"].as<int>();
            }
            if (n.has("mesh")) {
                node->mesh = n["mesh"].as<int>();
            }
            if (n.has("skin")) {
                node->skin = n["skin"].as<int>();
            }
            if (n.has("matrix")) {
                node->xform = _arr_to_xform(n["matrix"].as<Array>());

            }
            else {

                if (n.has("translation")) {
                    node->translation = _arr_to_vec3(n["translation"].as<Array>());
                }
                if (n.has("rotation")) {
                    node->rotation = _arr_to_quat(n["rotation"].as<Array>());
                }
                if (n.has("scale")) {
                    node->scale = _arr_to_vec3(n["scale"].as<Array>());
                }

                node->xform.basis.set_quat_scale(node->rotation, node->scale);
                node->xform.origin = node->translation;
            }
            if (n.has("extensions")) {
                Dictionary extensions = (Dictionary)n["extensions"];
                if (extensions.has("KHR_lights_punctual")) {
                    Dictionary lights_punctual = (Dictionary)extensions["KHR_lights_punctual"];
                    if (lights_punctual.has("light")) {
                        GLTFLightIndex light = (GLTFLightIndex)lights_punctual["light"];
                        node->light = light;
                    }
                }
            }
            if (n.has("children")) {
                const Array children = n["children"].as<Array>();
                for (int j = 0; j < children.size(); j++) {
                    node->children.push_back(children[j].as<int>());
                }
            }

            state.nodes.push_back(node);
        }

        //build the hierarchy
        for (GLTFNodeIndex node_i = 0; node_i < state.nodes.size(); node_i++) {

            for (int j = 0; j < state.nodes[node_i]->children.size(); j++) {
                GLTFNodeIndex child_i = state.nodes[node_i]->children[j];

                ERR_FAIL_INDEX_V(child_i, state.nodes.size(), ERR_FILE_CORRUPT);
                ERR_CONTINUE(state.nodes[child_i]->parent != -1); //node already has a parent, wtf.

                state.nodes[child_i]->parent = node_i;
            }
        }
        _compute_node_heights(state);

        return OK;
    }

    void _compute_node_heights(GLTFState& state) {

        state.root_nodes.clear();
        for (GLTFNodeIndex node_i = 0; node_i < state.nodes.size(); ++node_i) {
            GLTFNode* node = state.nodes[node_i];
            node->height = 0;

            GLTFNodeIndex current_i = node_i;
            while (current_i >= 0) {
                const GLTFNodeIndex parent_i = state.nodes[current_i]->parent;
                if (parent_i >= 0) {
                    ++node->height;
                }
                current_i = parent_i;
            }

            if (node->height == 0) {
                state.root_nodes.push_back(node_i);
            }
        }
    }
    static Vector<uint8_t> _parse_base64_uri(const String& uri) {

        auto start = StringUtils::find(uri, ",");
        ERR_FAIL_COND_V(start == String::npos, Vector<uint8_t>());

        String substr(StringUtils::right(uri, start + 1));

        int strlen = substr.length();

        Vector<uint8_t> buf;
        buf.resize(strlen / 4 * 3 + 1 + 1);

        size_t len = 0;
        Error err = CryptoCore::b64_decode(buf.data(), buf.size(), &len, (unsigned char*)substr.data(), strlen);
        ERR_FAIL_COND_V(err != OK, Vector<uint8_t>());

        buf.resize(len);

        return buf;
    }

    Error _parse_buffers(GLTFState& state, StringView p_base_path) {

        if (!state.json.has("buffers"))
            return OK;

        const Array buffers = state.json["buffers"].as<Array>();
        if (!buffers.empty() && not state.glb_data.empty())
        {
            state.buffers.emplace_back(eastl::move(state.glb_data));
        }
        for (GLTFBufferIndex i = 0; i < buffers.size(); i++) {

            const Dictionary buffer = buffers[i].as<Dictionary>();
            if (buffer.has("uri")) {

                Vector<uint8_t> buffer_data;
                String uri = buffer["uri"].as<String>();

                if (uri.starts_with("data:")) { // Embedded data using base64.
                    // Validate data MIME types and throw an error if it's one we don't know/support.
                    if (!uri.starts_with("data:application/octet-stream;base64") &&
                            !uri.starts_with("data:application/gltf-buffer;base64")) {
                        ERR_PRINT("glTF: Got buffer with an unknown URI data type: " + uri);
                    }
                    buffer_data = _parse_base64_uri(uri);
                } else { // Relative path to an external image file.
                    uri = PathUtils::plus_file(p_base_path,uri).replaced("\\", "/"); // Fix for Windows.
                    buffer_data = FileAccess::get_file_as_array(uri);
                    ERR_FAIL_COND_V_MSG(buffer.size() == 0, ERR_PARSE_ERROR, "glTF: Couldn't load binary file as an array: " + uri);
                }

                ERR_FAIL_COND_V(!buffer.has("byteLength"), ERR_PARSE_ERROR);
                int byteLength = buffer["byteLength"].as<int>();
                ERR_FAIL_COND_V(byteLength < buffer_data.size(), ERR_PARSE_ERROR);
                state.buffers.emplace_back(eastl::move(buffer_data));
            }
        }

        print_verbose("glTF: Total buffers: " + itos(state.buffers.size()));

        return OK;
    }

    Error _parse_buffer_views(GLTFState& state) {

        ERR_FAIL_COND_V(!state.json.has("bufferViews"), ERR_FILE_CORRUPT);
        const Array buffers = state.json["bufferViews"].as<Array>();
        for (GLTFBufferViewIndex i = 0; i < buffers.size(); i++) {

            const Dictionary d = buffers[i].as<Dictionary>();

            GLTFBufferView buffer_view;

            ERR_FAIL_COND_V(!d.has("buffer"), ERR_PARSE_ERROR);
            buffer_view.buffer = d["buffer"].as<int>();
            ERR_FAIL_COND_V(!d.has("byteLength"), ERR_PARSE_ERROR);
            buffer_view.byte_length = d["byteLength"].as<int>();

            if (d.has("byteOffset")) {
                buffer_view.byte_offset = d["byteOffset"].as<int>();
            }

            if (d.has("byteStride")) {
                buffer_view.byte_stride = d["byteStride"].as<int>();
            }

            if (d.has("target")) {
                const int target = d["target"].as<int>();
                buffer_view.indices = target == ELEMENT_ARRAY_BUFFER;
            }

            state.buffer_views.push_back(buffer_view);
        }

        print_verbose("glTF: Total buffer views: " + itos(state.buffer_views.size()));

        return OK;
    }
    GLTFType _get_type_from_str(const String &p_string) {

    if (p_string == "SCALAR")
        return TYPE_SCALAR;

    if (p_string == "VEC2")
        return TYPE_VEC2;
    if (p_string == "VEC3")
        return TYPE_VEC3;
    if (p_string == "VEC4")
        return TYPE_VEC4;

    if (p_string == "MAT2")
        return TYPE_MAT2;
    if (p_string == "MAT3")
        return TYPE_MAT3;
    if (p_string == "MAT4")
        return TYPE_MAT4;

    ERR_FAIL_V(TYPE_SCALAR);
}
    Error _parse_accessors(GLTFState& state) {

        ERR_FAIL_COND_V(!state.json.has("accessors"), ERR_FILE_CORRUPT);
        const Array accessors = state.json["accessors"].as<Array>();
        for (GLTFAccessorIndex i = 0; i < accessors.size(); i++) {

            const Dictionary d = accessors[i].as<Dictionary>();

            GLTFAccessor accessor;

            ERR_FAIL_COND_V(!d.has("componentType"), ERR_PARSE_ERROR);
            accessor.component_type = d["componentType"].as<int>();
            ERR_FAIL_COND_V(!d.has("count"), ERR_PARSE_ERROR);
            accessor.count = d["count"].as<int>();
            ERR_FAIL_COND_V(!d.has("type"), ERR_PARSE_ERROR);
            accessor.type = _get_type_from_str(d["type"].as<String>());

            if (d.has("bufferView")) {
                accessor.buffer_view = d["bufferView"].as<int>(); //optional because it may be sparse...
            }

            if (d.has("byteOffset")) {
                accessor.byte_offset = d["byteOffset"].as<int>();
            }

            if (d.has("normalized")) {
                accessor.normalized = d["normalized"].as<bool>();
            }

            if (d.has("max")) {
                accessor.max = d["max"].as<float>();
            }

            if (d.has("min")) {
                accessor.min = d["min"].as<float>();
            }

            if (d.has("sparse")) {
                //eeh..

                const Dictionary s = d["sparse"].as<Dictionary>();

                ERR_FAIL_COND_V(!s.has("count"), ERR_PARSE_ERROR);
                accessor.sparse_count = s["count"].as<int>();
                ERR_FAIL_COND_V(!s.has("indices"), ERR_PARSE_ERROR);
                const Dictionary si = s["indices"].as<Dictionary>();

                ERR_FAIL_COND_V(!si.has("bufferView"), ERR_PARSE_ERROR);
                accessor.sparse_indices_buffer_view = si["bufferView"].as<int>();
                ERR_FAIL_COND_V(!si.has("componentType"), ERR_PARSE_ERROR);
                accessor.sparse_indices_component_type = si["componentType"].as<int>();

                if (si.has("byteOffset")) {
                    accessor.sparse_indices_byte_offset = si["byteOffset"].as<int>();
                }

                ERR_FAIL_COND_V(!s.has("values"), ERR_PARSE_ERROR);
                const Dictionary sv = s["values"].as<Dictionary>();

                ERR_FAIL_COND_V(!sv.has("bufferView"), ERR_PARSE_ERROR);
                accessor.sparse_values_buffer_view = sv["bufferView"].as<int>();
                if (sv.has("byteOffset")) {
                    accessor.sparse_values_byte_offset = sv["byteOffset"].as<int>();
                }
            }

            state.accessors.push_back(accessor);
        }

        print_verbose("glTF: Total accessors: " + itos(state.accessors.size()));

        return OK;
    }

    int _get_component_type_size(const int component_type) {

        switch (component_type) {
        case COMPONENT_TYPE_BYTE: return 1; break;
        case COMPONENT_TYPE_UNSIGNED_BYTE: return 1; break;
        case COMPONENT_TYPE_SHORT: return 2; break;
        case COMPONENT_TYPE_UNSIGNED_SHORT: return 2; break;
        case COMPONENT_TYPE_INT: return 4; break;
        case COMPONENT_TYPE_FLOAT: return 4; break;
        default: {
            ERR_FAIL_V(0);
        }
        }
        return 0;
    }

    Vector<double> _decode_accessor(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

        //spec, for reference:
        //https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#data-alignment

        ERR_FAIL_INDEX_V(p_accessor, state.accessors.size(), Vector<double>());

        const GLTFAccessor& a = state.accessors[p_accessor];

        const int component_count_for_type[7] = {
            1, 2, 3, 4, 4, 9, 16
        };

        const int component_count = component_count_for_type[a.type];
        const int component_size = _get_component_type_size(a.component_type);
        ERR_FAIL_COND_V(component_size == 0, Vector<double>());
        int element_size = component_count * component_size;

        int skip_every = 0;
        int skip_bytes = 0;
        //special case of alignments, as described in spec
        switch (a.component_type) {
        case COMPONENT_TYPE_BYTE:
        case COMPONENT_TYPE_UNSIGNED_BYTE: {

            if (a.type == TYPE_MAT2) {
                skip_every = 2;
                skip_bytes = 2;
                element_size = 8; //override for this case
            }
            if (a.type == TYPE_MAT3) {
                skip_every = 3;
                skip_bytes = 1;
                element_size = 12; //override for this case
            }

        } break;
        case COMPONENT_TYPE_SHORT:
        case COMPONENT_TYPE_UNSIGNED_SHORT: {
            if (a.type == TYPE_MAT3) {
                skip_every = 6;
                skip_bytes = 4;
                element_size = 16; //override for this case
            }
        } break;
        default: {
        }
        }

        Vector<double> dst_buffer;
        dst_buffer.resize(component_count * a.count);
        double* dst = dst_buffer.data();

        if (a.buffer_view >= 0) {

            ERR_FAIL_INDEX_V(a.buffer_view, state.buffer_views.size(), Vector<double>());

            const Error err = _decode_buffer_view(state, dst, a.buffer_view, skip_every, skip_bytes, element_size, a.count, a.type, component_count, a.component_type, component_size, a.normalized, a.byte_offset, p_for_vertex);
            if (err != OK)
                return Vector<double>();

        }
        else {
            //fill with zeros, as bufferview is not defined.
            for (int i = 0; i < (a.count * component_count); i++) {
                dst_buffer[i] = 0;
            }
        }

        if (a.sparse_count > 0) {
            // I could not find any file using this, so this code is so far untested
            Vector<double> indices;
            indices.resize(a.sparse_count);
            const int indices_component_size = _get_component_type_size(a.sparse_indices_component_type);

            Error err = _decode_buffer_view(state, indices.data(), a.sparse_indices_buffer_view, 0, 0, indices_component_size, a.sparse_count, TYPE_SCALAR, 1, a.sparse_indices_component_type, indices_component_size, false, a.sparse_indices_byte_offset, false);
            if (err != OK)
                return Vector<double>();

            Vector<double> data;
            data.resize(component_count * a.sparse_count);
            err = _decode_buffer_view(state, data.data(), a.sparse_values_buffer_view, skip_every, skip_bytes, element_size, a.sparse_count, a.type, component_count, a.component_type, component_size, a.normalized, a.sparse_values_byte_offset, p_for_vertex);
            if (err != OK)
                return Vector<double>();

            for (int i = 0; i < indices.size(); i++) {
                const int write_offset = int(indices[i]) * component_count;

                for (int j = 0; j < component_count; j++) {
                    dst[write_offset + j] = data[i * component_count + j];
                }
            }
        }

        return dst_buffer;
    }

    Vector<int> _decode_accessor_as_ints(GLTFState& state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

        const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
        Vector<int> ret;
        if (attribs.empty())
            return ret;
        const double* attribs_ptr = attribs.data();
        const int ret_size = attribs.size();
        ret.reserve(ret_size);
        for (int i = 0; i < ret_size; i++) {
            ret.emplace_back(int(attribs_ptr[i]));
        }
        return ret;
    }

    Vector<float> _decode_accessor_as_floats(GLTFState& state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

        const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
        Vector<float> ret;
        if (attribs.empty())
            return ret;
        const double* attribs_ptr = attribs.data();
        const int ret_size = attribs.size();
        ret.reserve(ret_size);
        for (int i = 0; i < ret_size; i++) {
            ret.emplace_back(float(attribs_ptr[i]));
        }
        return ret;
    }

    Vector<Vector2> _decode_accessor_as_vec2(GLTFState& state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

        const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
        Vector<Vector2> ret;
        if (attribs.empty())
            return ret;
        ERR_FAIL_COND_V(attribs.size() % 2 != 0, ret);
        const double* attribs_ptr = attribs.data();
        const int ret_size = attribs.size() / 2;
        ret.reserve(ret_size);
        for (int i = 0; i < ret_size; i++) {
            ret.emplace_back(attribs_ptr[i * 2 + 0], attribs_ptr[i * 2 + 1]);
        }
        return ret;
    }

    Vector<Vector3> _decode_accessor_as_vec3(GLTFState& state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

        const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
        Vector<Vector3> ret;
        if (attribs.empty())
            return ret;
        ERR_FAIL_COND_V(attribs.size() % 3 != 0, ret);
        const double* attribs_ptr = attribs.data();
        const int ret_size = attribs.size() / 3;
        ret.reserve(ret_size);
        for (int i = 0; i < ret_size; i++) {
            ret.emplace_back(attribs_ptr[i * 3 + 0], attribs_ptr[i * 3 + 1], attribs_ptr[i * 3 + 2]);
        }
        return ret;
    }

    Vector<Color> _decode_accessor_as_color(GLTFState& state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

        const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
        Vector<Color> ret;

        if (attribs.empty())
            return ret;

        const int type = state.accessors[p_accessor].type;
        ERR_FAIL_COND_V(!(type == TYPE_VEC3 || type == TYPE_VEC4), ret);
        int vec_len = 3;
        if (type == TYPE_VEC4) {
            vec_len = 4;
        }

        ERR_FAIL_COND_V(attribs.size() % vec_len != 0, ret);
        const double* attribs_ptr = attribs.data();
        const int ret_size = attribs.size() / vec_len;
        ret.reserve(ret_size);
        {
            for (int i = 0; i < ret_size; i++) {
                ret.emplace_back(attribs_ptr[i * vec_len + 0], attribs_ptr[i * vec_len + 1], attribs_ptr[i * vec_len + 2], vec_len == 4 ? attribs_ptr[i * 4 + 3] : 1.0f);
            }
        }
        return ret;
    }
    Vector<Quat> _decode_accessor_as_quat(GLTFState& state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

        const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
        Vector<Quat> ret;
        if (attribs.empty())
            return ret;
        ERR_FAIL_COND_V(attribs.size() % 4 != 0, ret);
        const double* attribs_ptr = attribs.data();
        const int ret_size = attribs.size() / 4;
        ret.reserve(ret_size);
        for (int i = 0; i < ret_size; i++) {
            ret.emplace_back(Quat(attribs_ptr[i * 4 + 0], attribs_ptr[i * 4 + 1], attribs_ptr[i * 4 + 2], attribs_ptr[i * 4 + 3]).normalized());
        }
        return ret;
    }

    Vector<Transform2D> _decode_accessor_as_xform2d(GLTFState &state, const GLTFAccessorIndex p_accessor,
            const bool p_for_vertex) {

        const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
        Vector<Transform2D> ret;
        if (attribs.empty())
            return ret;
        ERR_FAIL_COND_V(attribs.size() % 4 != 0, ret);
        ret.resize(attribs.size() / 4);
        for (int i = 0; i < ret.size(); i++) {
            ret[i][0] = Vector2(attribs[i * 4 + 0], attribs[i * 4 + 1]);
            ret[i][1] = Vector2(attribs[i * 4 + 2], attribs[i * 4 + 3]);
        }
        return ret;
    }

    Vector<Basis> _decode_accessor_as_basis(GLTFState &state, const GLTFAccessorIndex p_accessor, bool p_for_vertex) {

        const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
        Vector<Basis> ret;
        if (attribs.empty())
            return ret;
        ERR_FAIL_COND_V(attribs.size() % 9 != 0, ret);
        ret.resize(attribs.size() / 9);
        for (int i = 0; i < ret.size(); i++) {
            ret[i].set_axis(0, Vector3(attribs[i * 9 + 0], attribs[i * 9 + 1], attribs[i * 9 + 2]));
            ret[i].set_axis(1, Vector3(attribs[i * 9 + 3], attribs[i * 9 + 4], attribs[i * 9 + 5]));
            ret[i].set_axis(2, Vector3(attribs[i * 9 + 6], attribs[i * 9 + 7], attribs[i * 9 + 8]));
        }
        return ret;
    }

    Vector<Transform> _decode_accessor_as_xform(GLTFState &state, const GLTFAccessorIndex p_accessor,
            const bool p_for_vertex) {

        const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
        Vector<Transform> ret;
        if (attribs.empty())
            return ret;
        ERR_FAIL_COND_V(attribs.size() % 16 != 0, ret);
        ret.resize(attribs.size() / 16);
        for (int i = 0; i < ret.size(); i++) {
            ret[i].basis.set_axis(0, Vector3(attribs[i * 16 + 0], attribs[i * 16 + 1], attribs[i * 16 + 2]));
            ret[i].basis.set_axis(1, Vector3(attribs[i * 16 + 4], attribs[i * 16 + 5], attribs[i * 16 + 6]));
            ret[i].basis.set_axis(2, Vector3(attribs[i * 16 + 8], attribs[i * 16 + 9], attribs[i * 16 + 10]));
            ret[i].set_origin(Vector3(attribs[i * 16 + 12], attribs[i * 16 + 13], attribs[i * 16 + 14]));
        }
        return ret;
    }

    Error _parse_meshes(GLTFState& state) {

        if (!state.json.has("meshes"))
            return OK;

        Array meshes = state.json["meshes"].as<Array>();
        for (GLTFMeshIndex i = 0; i < meshes.size(); i++) {

            print_verbose("glTF: Parsing mesh: " + itos(i));
            Dictionary d = meshes[i].as<Dictionary>();

            GLTFMesh mesh;
            mesh.mesh = make_ref_counted<ArrayMesh>();

            ERR_FAIL_COND_V(!d.has("primitives"), ERR_PARSE_ERROR);

            Array primitives = d["primitives"].as<Array>();
            const Dictionary extras = d.has("extras") ? d["extras"].as<Dictionary>() : Dictionary();

            for (int j = 0; j < primitives.size(); j++) {

                Dictionary p = primitives[j].as<Dictionary>();

                SurfaceArrays array;

                ERR_FAIL_COND_V(!p.has("attributes"), ERR_PARSE_ERROR);

                Dictionary a = p["attributes"].as<Dictionary>();

                Mesh::PrimitiveType primitive = Mesh::PRIMITIVE_TRIANGLES;
                if (p.has("mode")) {
                    const int mode = p["mode"].as<int>();
                    ERR_FAIL_INDEX_V(mode, 7, ERR_FILE_CORRUPT);
                    static const Mesh::PrimitiveType primitives2[7] = {
                        Mesh::PRIMITIVE_POINTS,
                        Mesh::PRIMITIVE_LINES,
                        Mesh::PRIMITIVE_LINE_LOOP,
                        Mesh::PRIMITIVE_LINE_STRIP,
                        Mesh::PRIMITIVE_TRIANGLES,
                        Mesh::PRIMITIVE_TRIANGLE_STRIP,
                        Mesh::PRIMITIVE_TRIANGLE_FAN,
                    };

                    primitive = primitives2[mode];
                }

                ERR_FAIL_COND_V(!a.has("POSITION"), ERR_PARSE_ERROR);
                if (a.has("POSITION")) {
                    array.set_positions(_decode_accessor_as_vec3(state, a["POSITION"].as<int>(), true));
                }

                if (a.has("NORMAL")) {
                    array.m_normals = eastl::move(_decode_accessor_as_vec3(state, a["NORMAL"].as<int>(), true));
                }
                if (a.has("TANGENT")) {
                    array.m_tangents = eastl::move(_decode_accessor_as_floats(state, a["TANGENT"].as<int>(), true));
                }
                if (a.has("TEXCOORD_0")) {
                    array.m_uv_1 = eastl::move(_decode_accessor_as_vec2(state, a["TEXCOORD_0"].as<int>(), true));
                }
                if (a.has("TEXCOORD_1")) {
                    array.m_uv_2 = eastl::move(_decode_accessor_as_vec2(state, a["TEXCOORD_1"].as<int>(), true));
                }
                if (a.has("COLOR_0")) {
                    array.m_colors = eastl::move(_decode_accessor_as_color(state, a["COLOR_0"].as<int>(), true));
                }
                if (a.has("JOINTS_0")) {
                    array.m_bones = eastl::move(_decode_accessor_as_ints(state, a["JOINTS_0"].as<int>(), true));
                }
                if (a.has("WEIGHTS_0")) {
                    Vector<float> weights(_decode_accessor_as_floats(state, a["WEIGHTS_0"].as<int>(), true));
                    { //gltf does not seem to normalize the weights for some reason..
                        size_t wc = weights.size();

                        for (size_t k = 0; k < wc; k += 4) {
                            float total = 0.0;
                            total += weights[k + 0];
                            total += weights[k + 1];
                            total += weights[k + 2];
                            total += weights[k + 3];
                            if (total > 0.0f) {
                                weights[k + 0] /= total;
                                weights[k + 1] /= total;
                                weights[k + 2] /= total;
                                weights[k + 3] /= total;
                            }

                        }
                    }
                    array.m_weights = eastl::move(weights);
                }

                if (p.has("indices")) {

                    Vector<int> indices = _decode_accessor_as_ints(state, p["indices"].as<int>(), false);

                    if (primitive == Mesh::PRIMITIVE_TRIANGLES) {
                        //swap around indices, convert ccw to cw for front face
                        const size_t is = indices.size();
                        ERR_FAIL_COND_V(is % 3 != 0, {});
                        for (size_t k = 0; k < is; k += 3) {
                            SWAP(indices[k + 1], indices[k + 2]);
                        }
                    }
                    array.m_indices = eastl::move(indices);
                }
                else if (primitive == Mesh::PRIMITIVE_TRIANGLES) {
                    //generate indices because they need to be swapped for CW/CCW
                    auto vertices = array.positions3();
                    ERR_FAIL_COND_V(vertices.empty(), ERR_PARSE_ERROR);
                    Vector<int> indices;
                    const size_t vs = vertices.size();
                    indices.resize(vs);

                    for (size_t k = 0; k < vs; k += 3) {
                        indices[k] = k;
                        indices[k + 1] = k + 2;
                        indices[k + 2] = k + 1;
                    }
                    array.m_indices = eastl::move(indices);
                }

                Vector<int> erased_indices;

                bool generate_tangents = (primitive == Mesh::PRIMITIVE_TRIANGLES && !a.has("TANGENT") && a.has("TEXCOORD_0") && a.has("NORMAL"));

                if (generate_tangents) {
                    //must generate mikktspace tangents.. ergh..
                    Ref<SurfaceTool> st(make_ref_counted<SurfaceTool>());
                    st->create_from_triangle_arrays(array);
                    st->generate_tangents();
                    array = st->commit_to_arrays();
                }

                Vector<SurfaceArrays> morphs;
                //blend shapes
                if (p.has("targets")) {
                    print_verbose("glTF: Mesh has targets");
                    const Array targets = p["targets"].as<Array>();

                    //ideally BLEND_SHAPE_MODE_RELATIVE since gltf2 stores in displacement
                    //but it could require a larger refactor?
                    mesh.mesh->set_blend_shape_mode(Mesh::BLEND_SHAPE_MODE_NORMALIZED);

                    if (j == 0) {
                        const Array& target_names = extras.has("targetNames") ? extras["targetNames"].as<Array>() : Array();
                        for (int k = 0; k < targets.size(); k++) {
                            const String name = k < target_names.size() ? target_names[k].as<String>() : String("morph_") + itos(k);
                            mesh.mesh->add_blend_shape(StringName(name));
                        }
                    }

                    for (int k = 0; k < targets.size(); k++) {

                        const Dictionary t = targets[k].as<Dictionary>();

                        SurfaceArrays array_copy(array.clone());

                        array_copy.m_indices.clear();

                        if (t.has("POSITION")) {
                            Vector<Vector3> varr(_decode_accessor_as_vec3(state, t["POSITION"].as<int>(), true));
                            auto src_varr = array.positions3();
                            const int size = src_varr.size();
                            ERR_FAIL_COND_V(size == 0, ERR_PARSE_ERROR);
                            {

                                const int max_idx = varr.size();
                                varr.resize(size);

                                for (int l = 0; l < size; l++) {
                                    if (l < max_idx) {
                                        varr[l] += src_varr[l];
                                    }
                                    else {
                                        varr[l] = src_varr[l];
                                    }
                                }
                            }
                            array_copy.set_positions(eastl::move(varr));
                        }
                        if (t.has("NORMAL")) {
                            Vector<Vector3> narr = _decode_accessor_as_vec3(state, t["NORMAL"].as<int>(), true);
                            const auto &src_narr = array.m_normals;
                            size_t size = src_narr.size();
                            ERR_FAIL_COND_V(size == 0, ERR_PARSE_ERROR);
                            {
                                size_t max_idx = narr.size();
                                narr.resize(size);

                                for (size_t l = 0; l < size; l++) {
                                    if (l < max_idx) {
                                        narr[l] += src_narr[l];
                                    }
                                    else {
                                        narr[l] = src_narr[l];
                                    }
                                }
                            }
                            array_copy.m_normals = eastl::move(narr);
                        }
                        if (t.has("TANGENT")) {
                            const Vector<Vector3> tangents_v3 = _decode_accessor_as_vec3(state, t["TANGENT"].as<int>(), true);
                            const auto & src_tangents = array.m_tangents;
                            ERR_FAIL_COND_V(src_tangents.empty(), ERR_PARSE_ERROR);
                            Vector<float> tangents_v4;

                            {

                                size_t max_idx = tangents_v3.size();

                                size_t size4 = src_tangents.size();
                                tangents_v4.resize(size4);

                                for (size_t l = 0; l < size4 / 4; l++) {

                                    if (l < max_idx) {
                                        tangents_v4[l * 4 + 0] = tangents_v3[l].x + src_tangents[l * 4 + 0];
                                        tangents_v4[l * 4 + 1] = tangents_v3[l].y + src_tangents[l * 4 + 1];
                                        tangents_v4[l * 4 + 2] = tangents_v3[l].z + src_tangents[l * 4 + 2];
                                    }
                                    else {
                                        tangents_v4[l * 4 + 0] = src_tangents[l * 4 + 0];
                                        tangents_v4[l * 4 + 1] = src_tangents[l * 4 + 1];
                                        tangents_v4[l * 4 + 2] = src_tangents[l * 4 + 2];
                                    }
                                    tangents_v4[l * 4 + 3] = src_tangents[l * 4 + 3]; //copy flip value
                                }
                            }

                            array_copy.m_tangents = eastl::move(tangents_v4);
                        }

                        if (generate_tangents) {
                            Ref<SurfaceTool> st(make_ref_counted<SurfaceTool>());

                            //array_copy.m_indices = eastl::move(erased_indices); //needed for tangent generation, erased by deindex
                            st->create_from_triangle_arrays(array_copy);
                            st->deindex();
                            st->generate_tangents();
                            array_copy = st->commit_to_arrays();
                        }

                        morphs.emplace_back(eastl::move(array_copy));
                    }
                }

                //just add it
                mesh.mesh->add_surface_from_arrays(primitive, eastl::move(array), eastl::move(morphs));

                if (p.has("material")) {
                    const int material = p["material"].as<int>();
                    ERR_FAIL_INDEX_V(material, state.materials.size(), ERR_FILE_CORRUPT);
                    const Ref<Material> &mat = state.materials[material];

                    mesh.mesh->surface_set_material(mesh.mesh->get_surface_count() - 1, mat);
                } else {
                    Ref<SpatialMaterial> mat(make_ref_counted<SpatialMaterial>());

                    mat->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);

                    mesh.mesh->surface_set_material(mesh.mesh->get_surface_count() - 1, mat);
                }
            }

            mesh.blend_weights.resize(mesh.mesh->get_blend_shape_count(),0.0f);

            if (d.has("weights")) {
                const Array weights = d["weights"].as<Array>();
                ERR_FAIL_COND_V(mesh.blend_weights.size() != weights.size(), ERR_PARSE_ERROR);
                for (int j = 0; j < weights.size(); j++) {
                    mesh.blend_weights[j] = weights[j].as<int>();
                }
            }

            state.meshes.push_back(mesh);
        }

        print_verbose("glTF: Total meshes: " + itos(state.meshes.size()));

        return OK;
    }

    Error _parse_images(GLTFState &state, const String &p_base_path) {

        if (!state.json.has("images"))
            return OK;

        // Ref: https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#images

        const Array images = state.json["images"].as<Array>();
        for (int i = 0; i < images.size(); i++) {

            const Dictionary d = images[i].as<Dictionary>();

            // glTF 2.0 supports PNG and JPEG types, which can be specified as (from spec):
            // "- a URI to an external file in one of the supported images formats, or
            //  - a URI with embedded base64-encoded data, or
            //  - a reference to a bufferView; in that case mimeType must be defined."
            // Since mimeType is optional for external files and base64 data, we'll have to
            // fall back on letting Godot parse the data to figure out if it's PNG or JPEG.

            // We'll assume that we use either URI or bufferView, so let's warn the user
            // if their image somehow uses both. And fail if it has neither.
            ERR_CONTINUE_MSG(!d.has("uri") && !d.has("bufferView"), "Invalid image definition in glTF file, it should specific an 'uri' or 'bufferView'.");
            if (d.has("uri") && d.has("bufferView")) {
                WARN_PRINT("Invalid image definition in glTF file using both 'uri' and 'bufferView'. 'bufferView' will take precedence.");
            }

            String mimetype;
            if (d.has("mimeType")) { // Should be "image/png" or "image/jpeg".
                mimetype = (String)d["mimeType"];
            }

            Vector<uint8_t> data;
            const uint8_t *data_ptr = nullptr;
            int data_size = 0;

            if (d.has("uri")) {
                // Handles the first two bullet points from the spec (embedded data, or external file).
                String uri = (String)d["uri"];

                if (uri.starts_with("data:")) { // Embedded data using base64.
                    // Validate data MIME types and throw an error if it's one we don't know/support.
                    if (!uri.starts_with("data:application/octet-stream;base64") &&
                            !uri.starts_with("data:application/gltf-buffer;base64") &&
                            !uri.starts_with("data:image/png;base64") &&
                            !uri.starts_with("data:image/jpeg;base64")) {
                        WARN_PRINT(FormatVE("glTF: Image index '%d' uses an unsupported URI data type: %s. Skipping it.", i, uri.c_str()));
                        state.images.push_back(Ref<Texture>()); // Placeholder to keep count.
                        continue;
                    }
                    data = _parse_base64_uri(uri);
                    data_ptr = data.data();
                    data_size = data.size();
                    // mimeType is optional, but if we have it defined in the URI, let's use it.
                    if (mimetype.empty()) {
                        if (uri.starts_with("data:image/png;base64")) {
                            mimetype = "image/png";
                        } else if (uri.starts_with("data:image/jpeg;base64")) {
                            mimetype = "image/jpeg";
                        }
                    }
                } else { // Relative path to an external image file.
                    uri = PathUtils::plus_file(p_base_path,uri).replaced("\\", "/"); // Fix for Windows.
                    // ResourceLoader will rely on the file extension to use the relevant loader.
                    // The spec says that if mimeType is defined, it should take precedence (e.g.
                    // there could be a `.png` image which is actually JPEG), but there's no easy
                    // API for that in Godot, so we'd have to load as a buffer (i.e. embedded in
                    // the material), so we do this only as fallback.
                    Ref<Texture> texture(gResourceManager().loadT<Texture>(uri));
                    if (texture) {
                        state.images.emplace_back(eastl::move(texture));
                        continue;
                    } else if (mimetype == "image/png" || mimetype == "image/jpeg") {
                        // Fallback to loading as byte array.
                        // This enables us to support the spec's requirement that we honor mimetype
                        // regardless of file URI.
                        data = FileAccess::get_file_as_array(uri);
                        if (data.size() == 0) {
                            WARN_PRINT(FormatVE("glTF: Image index '%d' couldn't be loaded as a buffer of MIME type '%s' from URI: %s. Skipping it.", i, mimetype.c_str(), uri.c_str()));
                            state.images.push_back(Ref<Texture>()); // Placeholder to keep count.
                            continue;
                        }
                        data_ptr = data.data();
                        data_size = data.size();
                    } else {
                        WARN_PRINT(FormatVE("glTF: Image index '%d' couldn't be loaded from URI: %s. Skipping it.", i, uri.c_str()));
                        state.images.push_back(Ref<Texture>()); // Placeholder to keep count.
                        continue;
                    }

                }
            } else if (d.has("bufferView")) {
                // Handles the third bullet point from the spec (bufferView).
                ERR_FAIL_COND_V_MSG(mimetype.empty(), ERR_FILE_CORRUPT,
                        FormatVE("glTF: Image index '%d' specifies 'bufferView' but no 'mimeType', which is invalid.", i));

                const GLTFBufferViewIndex bvi = (int)d["bufferView"];

                ERR_FAIL_INDEX_V(bvi, state.buffer_views.size(), ERR_PARAMETER_RANGE_ERROR);

                const GLTFBufferView &bv = state.buffer_views[bvi];

                const GLTFBufferIndex bi = bv.buffer;
                ERR_FAIL_INDEX_V(bi, state.buffers.size(), ERR_PARAMETER_RANGE_ERROR);

                ERR_FAIL_COND_V(bv.byte_offset + bv.byte_length > state.buffers[bi].size(), ERR_FILE_CORRUPT);

                data_ptr = &state.buffers[bi][bv.byte_offset];
                data_size = bv.byte_length;
            }

            Ref<Image> img;
            if (mimetype=="image/png") {
                //is a png
                ImageData img_data = ImageLoader::load_image("png", data_ptr, data_size);

                ERR_FAIL_COND_V(img_data.data.empty(), ERR_FILE_CORRUPT);
                Ref<Image> img(make_ref_counted<Image>());

                img->create(eastl::move(img_data));
            }
            else if (StringUtils::findn(mimetype, "jpeg") != String::npos) {
                //is a jpg
                ImageData img_data = ImageLoader::load_image("jpeg", data_ptr, data_size);

                ERR_FAIL_COND_V(img_data.data.empty(), ERR_FILE_CORRUPT);
                Ref<Image> img(make_ref_counted<Image>());

                img->create(eastl::move(img_data));

            } else  {
                // We can land here if we got an URI with base64-encoded data with application/* MIME type,
                // and the optional mimeType property was not defined to tell us how to handle this data (or was invalid).
                // So let's try PNG first, then JPEG.
                ImageData img_data = ImageLoader::load_image("png", data_ptr, data_size);
                if (img_data.data.empty()) {
                    img_data = ImageLoader::load_image("jpeg", data_ptr, data_size);
                }
                Ref<Image> img(make_ref_counted<Image>());

                img->create(eastl::move(img_data));

            }
            if (!img) {
                ERR_PRINT(FormatVE("glTF: Couldn't load image index '%d' with its given mimetype: %s.", i, mimetype.c_str()));
                state.images.push_back(Ref<Texture>());
                continue;
            }

            Ref<ImageTexture> t(make_ref_counted<ImageTexture>());

            t->create_from_image(img);

            state.images.push_back(t);
        }

        print_verbose("glTF: Total images: " + itos(state.images.size()));

        return OK;
    }

    Error _parse_textures(GLTFState& state) {

        if (!state.json.has("textures"))
            return OK;

        const Array textures = state.json["textures"].as<Array>();
        for (GLTFTextureIndex i = 0; i < textures.size(); i++) {

            const Dictionary d = textures[i].as<Dictionary>();

            ERR_FAIL_COND_V(!d.has("source"), ERR_PARSE_ERROR);

            GLTFTexture t;
            t.src_image = d["source"].as<int>();
            state.textures.push_back(t);
        }

        return OK;
    }
    Ref<Texture> _get_texture(GLTFState& state, const GLTFTextureIndex p_texture) {
        ERR_FAIL_INDEX_V(p_texture, state.textures.size(), Ref<Texture>());
        const GLTFImageIndex image = state.textures[p_texture].src_image;

        ERR_FAIL_INDEX_V(image, state.images.size(), Ref<Texture>());

        return state.images[image];
    }

    Error _parse_materials(GLTFState& state) {

        if (!state.json.has("materials"))
            return OK;

        const Array materials = state.json["materials"].as<Array>();
        for (GLTFMaterialIndex i = 0; i < materials.size(); i++) {

            const Dictionary d = materials[i].as<Dictionary>();

            Ref<SpatialMaterial> material(make_ref_counted<SpatialMaterial>());

            if (d.has("name")) {
                material->set_name(d["name"].as<String>());
            }
            material->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);

            if (d.has("pbrMetallicRoughness")) {

                const Dictionary mr = d["pbrMetallicRoughness"].as<Dictionary>();
                if (mr.has("baseColorFactor")) {
                    const Array arr = mr["baseColorFactor"].as<Array>();
                    ERR_FAIL_COND_V(arr.size() != 4, ERR_PARSE_ERROR);
                    const Color c = Color(arr[0].as<float>(), arr[1].as<float>(), arr[2].as<float>(), arr[3].as<float>()).to_srgb();

                    material->set_albedo(c);
                }

                if (mr.has("baseColorTexture")) {
                    const Dictionary bct = mr["baseColorTexture"].as<Dictionary>();
                    if (bct.has("index")) {
                        material->set_texture(SpatialMaterial::TEXTURE_ALBEDO, _get_texture(state, bct["index"].as<int>()));
                    }
                    if (!mr.has("baseColorFactor")) {
                        material->set_albedo(Color(1, 1, 1));
                    }
                }

                if (mr.has("metallicFactor")) {
                    material->set_metallic(mr["metallicFactor"].as<float>());
                }
                else {
                    material->set_metallic(1.0);
                }

                if (mr.has("roughnessFactor")) {
                    material->set_roughness(mr["roughnessFactor"].as<float>());
                }
                else {
                    material->set_roughness(1.0);
                }

                if (mr.has("metallicRoughnessTexture")) {
                    const Dictionary bct = mr["metallicRoughnessTexture"].as<Dictionary>();
                    if (bct.has("index")) {
                        const Ref<Texture> t = _get_texture(state, bct["index"].as<int>());
                        material->set_texture(SpatialMaterial::TEXTURE_METALLIC, t);
                        material->set_metallic_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_BLUE);
                        material->set_texture(SpatialMaterial::TEXTURE_ROUGHNESS, t);
                        material->set_roughness_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_GREEN);
                        if (!mr.has("metallicFactor")) {
                            material->set_metallic(1);
                        }
                        if (!mr.has("roughnessFactor")) {
                            material->set_roughness(1);
                        }
                    }
                }
            }

            if (d.has("normalTexture")) {
                const Dictionary bct = d["normalTexture"].as<Dictionary>();
                if (bct.has("index")) {
                    material->set_texture(SpatialMaterial::TEXTURE_NORMAL, _get_texture(state, bct["index"].as<int>()));
                    material->set_feature(SpatialMaterial::FEATURE_NORMAL_MAPPING, true);
                }
                if (bct.has("scale")) {
                    material->set_normal_scale(bct["scale"].as<float>());
                }
            }
            if (d.has("occlusionTexture")) {
                const Dictionary bct = d["occlusionTexture"].as<Dictionary>();
                if (bct.has("index")) {
                    material->set_texture(SpatialMaterial::TEXTURE_AMBIENT_OCCLUSION, _get_texture(state, bct["index"].as<int>()));
                    material->set_ao_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_RED);
                    material->set_feature(SpatialMaterial::FEATURE_AMBIENT_OCCLUSION, true);
                }
            }

            if (d.has("emissiveFactor")) {
                const Array arr = d["emissiveFactor"].as<Array>();
                ERR_FAIL_COND_V(arr.size() != 3, ERR_PARSE_ERROR);
                const Color c = Color(arr[0].as<float>(), arr[1].as<float>(), arr[2].as<float>()).to_srgb();
                material->set_feature(SpatialMaterial::FEATURE_EMISSION, true);

                material->set_emission(c);
            }

            if (d.has("emissiveTexture")) {
                const Dictionary bct = d["emissiveTexture"].as<Dictionary>();
                if (bct.has("index")) {
                    material->set_texture(SpatialMaterial::TEXTURE_EMISSION, _get_texture(state, bct["index"].as<int>()));
                    material->set_feature(SpatialMaterial::FEATURE_EMISSION, true);
                    material->set_emission(Color(0, 0, 0));
                }
            }

            if (d.has("doubleSided")) {
                const bool ds = d["doubleSided"].as<bool>();
                if (ds) {
                    material->set_cull_mode(SpatialMaterial::CULL_DISABLED);
                }
            }

            if (d.has("alphaMode")) {
                const String am = d["alphaMode"].as<String>();
                if (am == "BLEND") {
                    material->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
                    material->set_depth_draw_mode(SpatialMaterial::DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);
                }
                else if (am == "MASK") {
                    material->set_flag(SpatialMaterial::FLAG_USE_ALPHA_SCISSOR, true);
                    if (d.has("alphaCutoff")) {
                        material->set_alpha_scissor_threshold(d["alphaCutoff"].as<float>());
                    }
                    else {
                        material->set_alpha_scissor_threshold(0.5f);
                    }
                }
            }

            state.materials.push_back(material);
        }

        print_verbose("glTF: Total materials: " + itos(state.materials.size()));

        return OK;
    }

    GLTFNodeIndex _find_highest_node(GLTFState& state, const Vector<GLTFNodeIndex>& subset) {
        int highest = -1;
        GLTFNodeIndex best_node = -1;

        for (size_t i = 0; i < subset.size(); ++i) {
            const GLTFNodeIndex node_i = subset[i];
            const GLTFNode* node = state.nodes[node_i];

            if (highest == -1 || node->height < highest) {
                highest = node->height;
                best_node = node_i;
            }
        }

        return best_node;
    }

    bool _capture_nodes_in_skin(GLTFState& state, GLTFSkin& skin, const GLTFNodeIndex node_index) {

        bool found_joint = false;

        for (int i = 0; i < state.nodes[node_index]->children.size(); ++i) {
            found_joint |= _capture_nodes_in_skin(state, skin, state.nodes[node_index]->children[i]);
        }

        if (found_joint) {
            // Mark it if we happen to find another skins joint...
            if (state.nodes[node_index]->joint && !skin.joints.contains(node_index)) {
                skin.joints.push_back(node_index);
            }
            else if (!skin.non_joints.contains(node_index)) {
                skin.non_joints.push_back(node_index);
            }
        }
        //TODO: SEGS: it was checking for find result > 0, so it was checking the node_idx was not first in skin.joints ?
        return skin.joints.contains(node_index);
    }

    void _capture_nodes_for_multirooted_skin(GLTFState& state, GLTFSkin& skin) {

        DisjointSet<GLTFNodeIndex> disjoint_set;

        for (int i = 0; i < skin.joints.size(); ++i) {
            const GLTFNodeIndex node_index = skin.joints[i];
            const GLTFNodeIndex parent = state.nodes[node_index]->parent;
            disjoint_set.insert(node_index);

            if (skin.joints.contains(parent)) {
                disjoint_set.create_union(parent, node_index);
            }
        }

        Vector<GLTFNodeIndex> roots;
        disjoint_set.get_representatives(roots);

        if (roots.size() <= 1) {
            return;
        }

        int maxHeight = -1;

        // Determine the max height rooted tree
        for (int i = 0; i < roots.size(); ++i) {
            const GLTFNodeIndex root = roots[i];

            if (maxHeight == -1 || state.nodes[root]->height < maxHeight) {
                maxHeight = state.nodes[root]->height;
            }
        }

        // Go up the tree till all of the multiple roots of the skin are at the same hierarchy level.
        // This sucks, but 99% of all game engines (not just Godot) would have this same issue.
        for (size_t i = 0; i < roots.size(); ++i) {

            GLTFNodeIndex current_node = roots[i];
            while (state.nodes[current_node]->height > maxHeight) {
                GLTFNodeIndex parent = state.nodes[current_node]->parent;

                if (state.nodes[parent]->joint && !skin.joints.contains(parent)) {
                    skin.joints.push_back(parent);
                }
                else if (!skin.non_joints.contains(parent)) {
                    skin.non_joints.push_back(parent);
                }

                current_node = parent;
            }

            // replace the roots
            roots[i] = current_node;
        }

        // Climb up the tree until they all have the same parent
        bool all_same;

        do {
            all_same = true;
            const GLTFNodeIndex first_parent = state.nodes[roots[0]]->parent;

            for (size_t i = 1; i < roots.size(); ++i) {
                all_same &= (first_parent == state.nodes[roots[i]]->parent);
            }

            if (!all_same) {
                for (size_t i = 0; i < roots.size(); ++i) {
                    const GLTFNodeIndex current_node = roots[i];
                    const GLTFNodeIndex parent = state.nodes[current_node]->parent;

                    if (state.nodes[parent]->joint && !skin.joints.contains(parent)) {
                        skin.joints.push_back(parent);
                    }
                    else if (!skin.non_joints.contains(parent) ) {
                        skin.non_joints.push_back(parent);
                    }

                    roots[i] = parent;
                }
            }

        } while (!all_same);
    }

    Error _expand_skin(GLTFState& state, GLTFSkin& skin) {

        _capture_nodes_for_multirooted_skin(state, skin);

        // Grab all nodes that lay in between skin joints/nodes
        DisjointSet<GLTFNodeIndex> disjoint_set;

        Vector<GLTFNodeIndex> all_skin_nodes;
        all_skin_nodes.push_back(skin.joints);
        all_skin_nodes.push_back(skin.non_joints);

        for (size_t i = 0; i < all_skin_nodes.size(); ++i) {
            const GLTFNodeIndex node_index = all_skin_nodes[i];
            const GLTFNodeIndex parent = state.nodes[node_index]->parent;
            disjoint_set.insert(node_index);

            if (all_skin_nodes.contains(parent)) {
                disjoint_set.create_union(parent, node_index);
            }
        }

        Vector<GLTFNodeIndex> out_owners;
        disjoint_set.get_representatives(out_owners);

        Vector<GLTFNodeIndex> out_roots;

        for (size_t i = 0; i < out_owners.size(); ++i) {
            Vector<GLTFNodeIndex> set;
            disjoint_set.get_members(set, out_owners[i]);

            const GLTFNodeIndex root = _find_highest_node(state, set);
            ERR_FAIL_COND_V(root < 0, FAILED);
            out_roots.push_back(root);
        }
        eastl::sort(out_roots.begin(), out_roots.end());

        for (size_t i = 0; i < out_roots.size(); ++i) {
            _capture_nodes_in_skin(state, skin, out_roots[i]);
        }

        skin.roots = eastl::move(out_roots);

        return OK;
    }

    Error _verify_skin(GLTFState& state, GLTFSkin& skin) {
        // This may seem duplicated from expand_skins, but this is really a sanity check! (so it kinda is)
        // In case additional interpolating logic is added to the skins, this will help ensure that you
        // do not cause it to self implode into a fiery blaze

        // We are going to re-calculate the root nodes and compare them to the ones saved in the skin,
        // then ensure the multiple trees (if they exist) are on the same sublevel

        // Grab all nodes that lay in between skin joints/nodes
        DisjointSet<GLTFNodeIndex> disjoint_set;

        Vector<GLTFNodeIndex> all_skin_nodes;
        all_skin_nodes.push_back(skin.joints);
        all_skin_nodes.push_back(skin.non_joints);

        for (int i = 0; i < all_skin_nodes.size(); ++i) {
            const GLTFNodeIndex node_index = all_skin_nodes[i];
            const GLTFNodeIndex parent = state.nodes[node_index]->parent;
            disjoint_set.insert(node_index);

            if (all_skin_nodes.contains(parent)) {
                disjoint_set.create_union(parent, node_index);
            }
        }
        Vector<GLTFNodeIndex> out_owners;
        disjoint_set.get_representatives(out_owners);

        Vector<GLTFNodeIndex> out_roots;
        for (int i = 0; i < out_owners.size(); ++i) {
            Vector<GLTFNodeIndex> set;
            disjoint_set.get_members(set, out_owners[i]);

            const GLTFNodeIndex root = _find_highest_node(state, set);
            ERR_FAIL_COND_V(root < 0, FAILED);
            out_roots.push_back(root);
        }
        eastl::sort(out_roots.begin(), out_roots.end());

        ERR_FAIL_COND_V(out_roots.empty(), FAILED);
        // Make sure the roots are the exact same (they better be)
        ERR_FAIL_COND_V(out_roots.size() != skin.roots.size(), FAILED);
        for (int i = 0; i < out_roots.size(); ++i) {
            ERR_FAIL_COND_V(out_roots[i] != skin.roots[i], FAILED);
        }

        // Single rooted skin? Perfectly ok!
        if (out_roots.size() == 1) {
            return OK;
        }

        // Make sure all parents of a multi-rooted skin are the SAME
        const GLTFNodeIndex parent = state.nodes[out_roots[0]]->parent;
        for (int i = 1; i < out_roots.size(); ++i) {
            if (state.nodes[out_roots[i]]->parent != parent) {
                return FAILED;
            }
        }

        return OK;
    }
    Error _parse_skins(GLTFState& state) {

        if (!state.json.has("skins"))
            return OK;

        const Array skins = state.json["skins"].as<Array>();

        // Create the base skins, and mark nodes that are joints
        for (int i = 0; i < skins.size(); i++) {

            const Dictionary d = skins[i].as<Dictionary>();

            GLTFSkin skin;

            ERR_FAIL_COND_V(!d.has("joints"), ERR_PARSE_ERROR);

            const Array joints = d["joints"].as<Array>();

            if (d.has("inverseBindMatrices")) {
                skin.inverse_binds = _decode_accessor_as_xform(state, d["inverseBindMatrices"].as<int>(), false);
                ERR_FAIL_COND_V(skin.inverse_binds.size() != joints.size(), ERR_PARSE_ERROR);
            }

            for (int j = 0; j < joints.size(); j++) {
                const GLTFNodeIndex node = joints[j].as<int>();
                ERR_FAIL_INDEX_V(node, state.nodes.size(), ERR_PARSE_ERROR);

                skin.joints.push_back(node);
                skin.joints_original.push_back(node);

                state.nodes[node]->joint = true;
            }

            if (d.has("name")) {
                skin.name = d["name"].as<String>();
            }

            if (d.has("skeleton")) {
                skin.skin_root = d["skeleton"].as<int>();
            }
            state.skins.push_back(skin);
        }

        for (GLTFSkinIndex i = 0; i < state.skins.size(); ++i) {
            GLTFSkin& skin = state.skins[i];

            // Expand the skin to capture all the extra non-joints that lie in between the actual joints,
            // and expand the hierarchy to ensure multi-rooted trees lie on the same height level
            ERR_FAIL_COND_V(_expand_skin(state, skin), ERR_PARSE_ERROR);
            ERR_FAIL_COND_V(_verify_skin(state, skin), ERR_PARSE_ERROR);
        }

        print_verbose("glTF: Total skins: " + itos(state.skins.size()));

        return OK;
    }
    Error _determine_skeletons(GLTFState& state) {

        // Using a disjoint set, we are going to potentially combine all skins that are actually branches
        // of a main skeleton, or treat skins defining the same set of nodes as ONE skeleton.
        // This is another unclear issue caused by the current glTF specification.

        DisjointSet<GLTFNodeIndex> skeleton_sets;

        for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
            const GLTFSkin& skin = state.skins[skin_i];

            Vector<GLTFNodeIndex> all_skin_nodes;
            all_skin_nodes.push_back(skin.joints);
            all_skin_nodes.push_back(skin.non_joints);

            for (int i = 0; i < all_skin_nodes.size(); ++i) {
                const GLTFNodeIndex node_index = all_skin_nodes[i];
                const GLTFNodeIndex parent = state.nodes[node_index]->parent;
                skeleton_sets.insert(node_index);

                if (all_skin_nodes.contains(parent)) {
                    skeleton_sets.create_union(parent, node_index);
                }
            }

            // We are going to connect the separate skin subtrees in each skin together
            // so that the final roots are entire sets of valid skin trees
            for (int i = 1; i < skin.roots.size(); ++i) {
                skeleton_sets.create_union(skin.roots[0], skin.roots[i]);
            }
        }

        { // attempt to joint all touching subsets (siblings/parent are part of another skin)
            Vector<GLTFNodeIndex> groups_representatives;
            skeleton_sets.get_representatives(groups_representatives);

            Vector<GLTFNodeIndex> highest_group_members;
            Vector<Vector<GLTFNodeIndex> > groups;
            groups.reserve(groups_representatives.size());
            for (const GLTFNodeIndex grp_rep : groups_representatives) {
                Vector<GLTFNodeIndex> group;
                skeleton_sets.get_members(group, grp_rep);
                highest_group_members.emplace_back(_find_highest_node(state, group));
                groups.emplace_back(eastl::move(group));
            }

            for (int i = 0; i < highest_group_members.size(); ++i) {
                const GLTFNodeIndex node_i = highest_group_members[i];

                // Attach any siblings together (this needs to be done n^2/2 times)
                for (size_t j = i + 1; j < highest_group_members.size(); ++j) {
                    const GLTFNodeIndex node_j = highest_group_members[j];

                    // Even if they are siblings under the root! :)
                    if (state.nodes[node_i]->parent == state.nodes[node_j]->parent) {
                        skeleton_sets.create_union(node_i, node_j);
                    }
                }

                // Attach any parenting going on together (we need to do this n^2 times)
                const GLTFNodeIndex node_i_parent = state.nodes[node_i]->parent;
                if (node_i_parent >= 0) {
                    for (size_t j = 0; j < groups.size() && i != j; ++j) {
                        const Vector<GLTFNodeIndex>& group = groups[j];

                        if (group.contains(node_i_parent)) {
                            const GLTFNodeIndex node_j = highest_group_members[j];
                            skeleton_sets.create_union(node_i, node_j);
                        }
                    }
                }
            }
        }

        // At this point, the skeleton groups should be finalized
        Vector<GLTFNodeIndex> skeleton_owners;
        skeleton_sets.get_representatives(skeleton_owners);

        // Mark all the skins actual skeletons, after we have merged them
        for (GLTFSkeletonIndex skel_i = 0; skel_i < skeleton_owners.size(); ++skel_i) {

            const GLTFNodeIndex skeleton_owner = skeleton_owners[skel_i];
            GLTFSkeleton skeleton;

            Vector<GLTFNodeIndex> skeleton_nodes;
            skeleton_sets.get_members(skeleton_nodes, skeleton_owner);

            for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
                GLTFSkin& skin = state.skins[skin_i];

                // If any of the the skeletons nodes exist in a skin, that skin now maps to the skeleton
                for (int i = 0; i < skeleton_nodes.size(); ++i) {
                    GLTFNodeIndex skel_node_i = skeleton_nodes[i];
                    if (skin.joints.contains(skel_node_i) || skin.non_joints.contains(skel_node_i)) {
                        skin.skeleton = skel_i;
                        continue;
                    }
                }
            }

            Vector<GLTFNodeIndex> non_joints;
            for (int i = 0; i < skeleton_nodes.size(); ++i) {
                const GLTFNodeIndex node_i = skeleton_nodes[i];

                if (state.nodes[node_i]->joint) {
                    skeleton.joints.push_back(node_i);
                }
                else {
                    non_joints.push_back(node_i);
                }
            }

            state.skeletons.push_back(skeleton);

            _reparent_non_joint_skeleton_subtrees(state, state.skeletons[skel_i], non_joints);
        }

        for (GLTFSkeletonIndex skel_i = 0; skel_i < state.skeletons.size(); ++skel_i) {
            GLTFSkeleton& skeleton = state.skeletons[skel_i];

            for (int i = 0; i < skeleton.joints.size(); ++i) {
                const GLTFNodeIndex node_i = skeleton.joints[i];
                GLTFNode* node = state.nodes[node_i];

                ERR_FAIL_COND_V(!node->joint, ERR_PARSE_ERROR);
                ERR_FAIL_COND_V(node->skeleton >= 0, ERR_PARSE_ERROR);
                node->skeleton = skel_i;
            }

            ERR_FAIL_COND_V(_determine_skeleton_roots(state, skel_i), ERR_PARSE_ERROR);
        }

        return OK;
    }

    Error _reparent_non_joint_skeleton_subtrees(GLTFState& state, GLTFSkeleton& skeleton, const Vector<GLTFNodeIndex>& non_joints) {

        DisjointSet<GLTFNodeIndex> subtree_set;

        // Populate the disjoint set with ONLY non joints that are in the skeleton hierarchy (non_joints vector)
        // This way we can find any joints that lie in between joints, as the current glTF specification
        // mentions nothing about non-joints being in between joints of the same skin. Hopefully one day we
        // can remove this code.

        // skinD depicted here explains this issue:
        // https://github.com/KhronosGroup/glTF-Asset-Generator/blob/master/Output/Positive/Animation_Skin

        for (int i = 0; i < non_joints.size(); ++i) {
            const GLTFNodeIndex node_i = non_joints[i];

            subtree_set.insert(node_i);

            const GLTFNodeIndex parent_i = state.nodes[node_i]->parent;
            if (parent_i >= 0 && non_joints.contains(parent_i) && !state.nodes[parent_i]->joint) {
                subtree_set.create_union(parent_i, node_i);
            }
        }

        // Find all the non joint subtrees and re-parent them to a new "fake" joint

        Vector<GLTFNodeIndex> non_joint_subtree_roots;
        subtree_set.get_representatives(non_joint_subtree_roots);

        for (int root_i = 0; root_i < non_joint_subtree_roots.size(); ++root_i) {
            const GLTFNodeIndex subtree_root = non_joint_subtree_roots[root_i];

            Vector<GLTFNodeIndex> subtree_nodes;
            subtree_set.get_members(subtree_nodes, subtree_root);

            for (int subtree_i = 0; subtree_i < subtree_nodes.size(); ++subtree_i) {
                GLTFNode *node = state.nodes[subtree_nodes[subtree_i]];
                node->joint = true;
                // Add the joint to the skeletons joints
                skeleton.joints.push_back(subtree_nodes[subtree_i]);
            }
        }

        return OK;
    }

    Error _determine_skeleton_roots(GLTFState& state, const GLTFSkeletonIndex skel_i) {

        DisjointSet<GLTFNodeIndex> disjoint_set;

        for (GLTFNodeIndex i = 0; i < state.nodes.size(); ++i) {
            const GLTFNode* node = state.nodes[i];

            if (node->skeleton != skel_i) {
                continue;
            }

            disjoint_set.insert(i);

            if (node->parent >= 0 && state.nodes[node->parent]->skeleton == skel_i) {
                disjoint_set.create_union(node->parent, i);
            }
        }

        GLTFSkeleton& skeleton = state.skeletons[skel_i];

        Vector<GLTFNodeIndex> owners;
        disjoint_set.get_representatives(owners);

        Vector<GLTFNodeIndex> roots;
        roots.reserve(owners.size());
        for (size_t i = 0; i < owners.size(); ++i) {
            Vector<GLTFNodeIndex> set;
            disjoint_set.get_members(set, owners[i]);
            const GLTFNodeIndex root = _find_highest_node(state, set);
            ERR_FAIL_COND_V(root < 0, FAILED);
            roots.push_back(root);
        }
        eastl::sort(roots.begin(), roots.end());

        skeleton.roots = roots;

        if (roots.empty()) {
            return FAILED;
        }
        else if (roots.size() == 1) {
            return OK;
        }

        // Check that the subtrees have the same parent root
        const GLTFNodeIndex parent = state.nodes[roots[0]]->parent;
        for (size_t i = 1; i < roots.size(); ++i) {
            if (state.nodes[roots[i]]->parent != parent) {
                return FAILED;
            }
        }

        return OK;
    }

    Error _create_skeletons(GLTFState& state) {
        for (GLTFSkeletonIndex skel_i = 0; skel_i < state.skeletons.size(); ++skel_i) {

            GLTFSkeleton& gltf_skeleton = state.skeletons[skel_i];

            Skeleton* skeleton = memnew(Skeleton);
            gltf_skeleton.godot_skeleton = skeleton;

            // Make a unique name, no gltf node represents this skeleton
            skeleton->set_name(_gen_unique_name(state, "Skeleton"));

            Dequeue<GLTFNodeIndex> bones;

            for (int i = 0; i < gltf_skeleton.roots.size(); ++i) {
                bones.push_back(gltf_skeleton.roots[i]);
            }

            // Make the skeleton creation deterministic by going through the roots in
            // a sorted order, and DEPTH FIRST
            eastl::sort(bones.begin(), bones.end());

            while (!bones.empty()) {
                const GLTFNodeIndex node_i = bones.front();
                bones.pop_front();

                GLTFNode* node = state.nodes[node_i];
                ERR_FAIL_COND_V(node->skeleton != skel_i, FAILED);

                { // Add all child nodes to the stack (deterministically)
                    Vector<GLTFNodeIndex> child_nodes;
                    for (int i = 0; i < node->children.size(); ++i) {
                        const GLTFNodeIndex child_i = node->children[i];
                        if (state.nodes[child_i]->skeleton == skel_i) {
                            child_nodes.push_back(child_i);
                        }
                    }

                    // Depth first insertion
                    eastl::sort(child_nodes.begin(), child_nodes.end());

                    for (int i = child_nodes.size() - 1; i >= 0; --i) {
                        bones.push_front(child_nodes[i]);
                    }
                }

                const int bone_index = skeleton->get_bone_count();

                if (node->name.empty()) {
                    node->name = "bone";
                }

                node->name = StringName(_gen_unique_bone_name(state, skel_i, node->name));

                skeleton->add_bone(node->name);
                skeleton->set_bone_rest(bone_index, node->xform);

                if (node->parent >= 0 && state.nodes[node->parent]->skeleton == skel_i) {
                    const int bone_parent = skeleton->find_bone(state.nodes[node->parent]->name);
                    ERR_FAIL_COND_V(bone_parent < 0, FAILED);
                    skeleton->set_bone_parent(bone_index, skeleton->find_bone(state.nodes[node->parent]->name));
                }

                state.scene_nodes.emplace(node_i, skeleton);
            }
        }

        ERR_FAIL_COND_V(_map_skin_joints_indices_to_skeleton_bone_indices(state), ERR_PARSE_ERROR);

        return OK;
    }

    Error _map_skin_joints_indices_to_skeleton_bone_indices(GLTFState& state) {
        for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
            GLTFSkin& skin = state.skins[skin_i];

            const GLTFSkeleton& skeleton = state.skeletons[skin.skeleton];

            for (size_t joint_index = 0; joint_index < skin.joints_original.size(); ++joint_index) {
                const GLTFNodeIndex node_i = skin.joints_original[joint_index];
                const GLTFNode* node = state.nodes[node_i];

                skin.joint_i_to_name.emplace(joint_index, StringName(node->name));

                const int bone_index = skeleton.godot_skeleton->find_bone(node->name);
                ERR_FAIL_COND_V(bone_index < 0, FAILED);

                skin.joint_i_to_bone_i.emplace(joint_index, bone_index);
            }
        }

        return OK;
    }

    Error _create_skins(GLTFState& state) {
        for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
            GLTFSkin& gltf_skin = state.skins[skin_i];

            Ref<Skin> skin(make_ref_counted<Skin>());

            // Some skins don't have IBM's! What absolute monsters!
            const bool has_ibms = !gltf_skin.inverse_binds.empty();

            for (int joint_i = 0; joint_i < gltf_skin.joints_original.size(); ++joint_i) {

                Transform xform;
                if (has_ibms) {
                    xform = gltf_skin.inverse_binds[joint_i];
                }

                if (state.use_named_skin_binds) {
                    StringName name = gltf_skin.joint_i_to_name[joint_i];
                    skin->add_named_bind(name, xform);
                } else {
                    int bone_i = gltf_skin.joint_i_to_bone_i[joint_i];
                    skin->add_bind(bone_i, xform);
                }
            }

            gltf_skin.godot_skin = skin;
        }

        // Purge the duplicates!
        _remove_duplicate_skins(state);

        // Create unique names now, after removing duplicates
        for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
            Ref<Skin> skin = state.skins[skin_i].godot_skin;
            if (skin->get_name().empty()) {
                // Make a unique name, no gltf node represents this skin
                skin->set_name(_gen_unique_name(state, "Skin"));
            }
        }

        return OK;
    }

    bool _skins_are_same(const Ref<Skin>& skin_a, const Ref<Skin>& skin_b) {
        if (skin_a->get_bind_count() != skin_b->get_bind_count()) {
            return false;
        }

        for (int i = 0; i < skin_a->get_bind_count(); ++i) {

            if (skin_a->get_bind_bone(i) != skin_b->get_bind_bone(i)) {
                return false;
            }
            if (skin_a->get_bind_name(i) != skin_b->get_bind_name(i)) {
                return false;
            }

            Transform a_xform = skin_a->get_bind_pose(i);
            Transform b_xform = skin_b->get_bind_pose(i);

            if (a_xform != b_xform) {
                return false;
            }
        }

        return true;
    }

    void _remove_duplicate_skins(GLTFState& state) {
        for (size_t i = 0; i < state.skins.size(); ++i) {
            for (size_t j = i + 1; j < state.skins.size(); ++j) {
                const Ref<Skin>& skin_i = state.skins[i].godot_skin;
                const Ref<Skin>& skin_j = state.skins[j].godot_skin;

                if (_skins_are_same(skin_i, skin_j)) {
                    // replace it and delete the old
                    state.skins[j].godot_skin = skin_i;
                }
            }
        }
    }
    Error _parse_lights(GLTFState& state) {
        if (!state.json.has("extensions")) {
            return OK;
        }
        Dictionary extensions = (Dictionary)state.json["extensions"];
        if (!extensions.has("KHR_lights_punctual")) {
            return OK;
        }
        Dictionary lights_punctual = (Dictionary)extensions["KHR_lights_punctual"];
        if (!lights_punctual.has("lights")) {
            return OK;
        }

        const Array lights = (Array)lights_punctual["lights"];

        for (GLTFLightIndex light_i = 0; light_i < lights.size(); light_i++) {
            const Dictionary d = (Dictionary)lights[light_i];

            GLTFLight light;
            ERR_FAIL_COND_V(!d.has("type"), ERR_PARSE_ERROR);
            const String type = (String)d["type"];
            light.type = type;

            if (d.has("color")) {
                const Array arr = (Array)d["color"];
                ERR_FAIL_COND_V(arr.size() != 3, ERR_PARSE_ERROR);
                const Color c = Color((float)arr[0], (float)arr[1], (float)arr[2]).to_srgb();
                light.color = c;
            }
            if (d.has("intensity")) {
                light.intensity = (float)d["intensity"];
            }
            if (d.has("range")) {
                light.range = (float)d["range"];
            }
            if (type == "spot") {
                const Dictionary spot = (Dictionary)d["spot"];
                light.inner_cone_angle = (float)spot["innerConeAngle"];
                light.outer_cone_angle = (float)spot["outerConeAngle"];
                ERR_FAIL_COND_V_MSG(light.inner_cone_angle >= light.outer_cone_angle, ERR_PARSE_ERROR, "The inner angle must be smaller than the outer angle.");
            }
            else if (type != "point" && type != "directional") {
                ERR_FAIL_V_MSG(ERR_PARSE_ERROR, "Light type is unknown.");
            }

            state.lights.emplace_back(eastl::move(light));
        }

        print_verbose("glTF: Total lights: " + itos(state.lights.size()));

        return OK;
    }
    Error _parse_cameras(GLTFState& state) {

        if (!state.json.has("cameras"))
            return OK;

        const Array cameras = state.json["cameras"].as<Array>();

        for (GLTFCameraIndex i = 0; i < cameras.size(); i++) {

            const Dictionary d = cameras[i].as<Dictionary>();

            GLTFCamera camera;
            ERR_FAIL_COND_V(!d.has("type"), ERR_PARSE_ERROR);
            const String type = d["type"].as<String>();
            if (type == "orthographic") {

                camera.perspective = false;
                if (d.has("orthographic")) {
                    const Dictionary og = d["orthographic"].as<Dictionary>();
                    camera.fov_size = og["ymag"].as<float>();
                    camera.zfar = og["zfar"].as<float>();
                    camera.znear = og["znear"].as<float>();
                }
                else {
                    camera.fov_size = 10;
                }

            }
            else if (type == "perspective") {

                camera.perspective = true;
                if (d.has("perspective")) {
                    const Dictionary ppt = d["perspective"].as<Dictionary>();
                    // GLTF spec is in radians, Godot's camera is in degrees.
                    camera.fov_size = ppt["yfov"].as<float>() * 180.0f / Math_PI;
                    camera.zfar = ppt["zfar"].as<float>();
                    camera.znear = ppt["znear"].as<float>();
                }
                else {
                    camera.fov_size = 10;
                }
            }
            else {
                ERR_FAIL_V_MSG(ERR_PARSE_ERROR, "Camera3D should be in 'orthographic' or 'perspective'");
            }

            state.cameras.push_back(camera);
        }

        print_verbose("glTF: Total cameras: " + itos(state.cameras.size()));

        return OK;
    }

    Error _parse_animations(GLTFState& state) {

        if (!state.json.has("animations"))
            return OK;

        const Array animations = state.json["animations"].as<Array>();

        for (GLTFAnimationIndex i = 0; i < animations.size(); i++) {

            const Dictionary d = animations[i].as<Dictionary>();

            GLTFAnimation animation;

            if (!d.has("channels") || !d.has("samplers"))
                continue;

            Array channels = d["channels"].as<Array>();
            Array samplers = d["samplers"].as<Array>();

            if (d.has("name")) {
                String name = d["name"].as<String>();
                if (StringUtils::begins_with(name, "loop") ||
                    StringUtils::ends_with(name, "loop") ||
                    StringUtils::begins_with(name, "cycle") ||
                    StringUtils::ends_with(name, "cycle")) {
                    animation.loop = true;
                }
                if (state.use_legacy_names) {
                    animation.name = _sanitize_scene_name(state, name);
                } else {
                    animation.name = _gen_unique_animation_name(state, name);
                }
            }

            for (int j = 0; j < channels.size(); j++) {

                const Dictionary c = channels[j].as<Dictionary>();
                if (!c.has("target"))
                    continue;

                const Dictionary t = c["target"].as<Dictionary>();
                if (!t.has("node") || !t.has("path")) {
                    continue;
                }

                ERR_FAIL_COND_V(!c.has("sampler"), ERR_PARSE_ERROR);
                const int sampler = c["sampler"].as<int>();
                ERR_FAIL_INDEX_V(sampler, samplers.size(), ERR_PARSE_ERROR);

                GLTFNodeIndex node = t["node"].as<int>();
                String path = t["path"].as<String>();

                ERR_FAIL_INDEX_V(node, state.nodes.size(), ERR_PARSE_ERROR);

                GLTFAnimation::Track* track = nullptr;

                if (!animation.tracks.contains(node)) {
                    animation.tracks[node] = GLTFAnimation::Track();
                }

                track = &animation.tracks[node];

                const Dictionary s = samplers[sampler].as<Dictionary>();

                ERR_FAIL_COND_V(!s.has("input"), ERR_PARSE_ERROR);
                ERR_FAIL_COND_V(!s.has("output"), ERR_PARSE_ERROR);

                const int input = s["input"].as<int>();
                const int output = s["output"].as<int>();

                GLTFAnimation::Interpolation interp = GLTFAnimation::INTERP_LINEAR;
                int output_count = 1;
                if (s.has("interpolation")) {
                    String in = s["interpolation"].as<String>();
                    if (in == "STEP") {
                        interp = GLTFAnimation::INTERP_STEP;
                    }
                    else if (in == "LINEAR") {
                        interp = GLTFAnimation::INTERP_LINEAR;
                    }
                    else if (in == "CATMULLROMSPLINE") {
                        interp = GLTFAnimation::INTERP_CATMULLROMSPLINE;
                        output_count = 3;
                    }
                    else if (in == "CUBICSPLINE") {
                        interp = GLTFAnimation::INTERP_CUBIC_SPLINE;
                        output_count = 3;
                    }
                }

                const Vector<float> times = _decode_accessor_as_floats(state, input, false);
                if (path == "translation") {
                    const Vector<Vector3> translations = _decode_accessor_as_vec3(state, output, false);
                    track->translation_track.interpolation = interp;
                    track->translation_track.times = times; //convert via variant
                    track->translation_track.values = translations; //convert via variant
                }
                else if (path == "rotation") {
                    const Vector<Quat> rotations = _decode_accessor_as_quat(state, output, false);
                    track->rotation_track.interpolation = interp;
                    track->rotation_track.times = times; //convert via variant
                    track->rotation_track.values = rotations; //convert via variant
                }
                else if (path == "scale") {
                    const Vector<Vector3> scales = _decode_accessor_as_vec3(state, output, false);
                    track->scale_track.interpolation = interp;
                    track->scale_track.times = times; //convert via variant
                    track->scale_track.values = scales; //convert via variant
                }
                else if (path == "weights") {
                    const Vector<float> weights = _decode_accessor_as_floats(state, output, false);

                    ERR_FAIL_INDEX_V(state.nodes[node]->mesh, state.meshes.size(), ERR_PARSE_ERROR);
                    const GLTFMesh* mesh = &state.meshes[state.nodes[node]->mesh];
                    ERR_FAIL_COND_V(mesh->blend_weights.empty(), ERR_PARSE_ERROR);
                    const int wc = mesh->blend_weights.size();

                    track->weight_tracks.resize(wc);

                    const int expected_value_count = times.size() * output_count * wc;
                    ERR_FAIL_COND_V_MSG(weights.size() != expected_value_count, ERR_PARSE_ERROR,
                        "Invalid weight data, expected " + itos(expected_value_count) + " weight values, got " +
                        itos(weights.size()) + " instead.");

                    const size_t wlen = weights.size() / wc;
                    for (int k = 0; k < wc; k++) { //separate tracks, having them together is not such a good idea
                        GLTFAnimation::Channel<float> cf;
                        cf.interpolation = interp;
                        cf.times = times;
                        Vector<float> wdata;
                        wdata.reserve(wlen);
                        for (size_t l = 0; l < wlen; l++) {
                            wdata.emplace_back(weights[l * wc + k]);
                        }

                        cf.values = eastl::move(wdata);
                        track->weight_tracks[k] = eastl::move(cf);
                    }
                }
                else {
                    WARN_PRINT("Invalid path '" + path + "'.");
                }
            }

            state.animations.push_back(animation);
        }

        print_verbose("glTF: Total animations '" + itos(state.animations.size()) + "'.");

        return OK;
    }

    void _assign_scene_names(GLTFState& state) {

        for (int i = 0; i < state.nodes.size(); i++) {
            GLTFNode* n = state.nodes[i];
            // Any joints get unique names generated when the skeleton is made, unique to the skeleton
            if (n->skeleton >= 0)
                continue;
            if (n->name.empty()) {
                if (n->mesh >= 0) {
                    n->name = "Mesh";
                }
                else if (n->camera >= 0) {
                    n->name = "Camera3D";
                }
                else {
                    n->name = "Node";
                }
            }

            n->name = StringName(_gen_unique_name(state, n->name));
        }
    }

    BoneAttachment3D* _generate_bone_attachment(GLTFState& state, Skeleton* skeleton, const GLTFNodeIndex node_index, const GLTFNodeIndex bone_index) {

        const GLTFNode* gltf_node = state.nodes[node_index];
        const GLTFNode* bone_node = state.nodes[bone_index];

        BoneAttachment3D* bone_attachment = memnew(BoneAttachment3D);
        print_verbose("glTF: Creating bone attachment for: " + gltf_node->name);

        ERR_FAIL_COND_V(!bone_node->joint, nullptr);

        bone_attachment->set_bone_name(bone_node->name);

        return bone_attachment;
    }

    MeshInstance3D* _generate_mesh_instance(GLTFState& state, Node* scene_parent, const GLTFNodeIndex node_index) {
        const GLTFNode* gltf_node = state.nodes[node_index];

        ERR_FAIL_INDEX_V(gltf_node->mesh, state.meshes.size(), nullptr);

        MeshInstance3D* mi = memnew(MeshInstance3D);
        print_verbose("glTF: Creating mesh for: " + gltf_node->name);

        GLTFMesh& mesh = state.meshes[gltf_node->mesh];
        mi->set_mesh(mesh.mesh);
        if (mesh.mesh->get_name().empty()) {
            mesh.mesh->set_name(gltf_node->name);
        }
        for (int i = 0; i < mesh.blend_weights.size(); i++) {
            mi->set("blend_shapes/" + mesh.mesh->get_blend_shape_name(i), mesh.blend_weights[i]);
        }

        return mi;
    }
    Node3D *_generate_light(GLTFState& state, Node* scene_parent, const GLTFNodeIndex node_index) {
        const GLTFNode* gltf_node = state.nodes[node_index];

        ERR_FAIL_INDEX_V(gltf_node->light, state.lights.size(), nullptr);

        print_verbose("glTF: Creating light for: " + gltf_node->name);

        const GLTFLight& l = state.lights[gltf_node->light];

        float intensity = l.intensity;
        if (intensity > 10) {
            // GLTF spec has the default around 1, but Blender defaults lights to 100.
            // The only sane way to handle this is to check where it came from and
            // handle it accordingly. If it's over 10, it probably came from Blender.
            intensity /= 100;
        }

        if (l.type == "directional") {
            DirectionalLight3D* light = memnew(DirectionalLight3D);
            light->set_param(Light3D::PARAM_ENERGY, intensity);
            light->set_color(l.color);
            return light;
        }

        const float range = CLAMP<float>(l.range, 0, 4096);
        // Doubling the range will double the effective brightness, so we need double attenuation (half brightness).
        // We want to have double intensity give double brightness, so we need half the attenuation.
        const float attenuation = range / intensity;
        if (l.type == "point") {
            OmniLight3D* light = memnew(OmniLight3D);
            light->set_param(OmniLight3D::PARAM_ATTENUATION, attenuation);
            light->set_param(OmniLight3D::PARAM_RANGE, range);
            light->set_color(l.color);
            return light;
        }
        if (l.type == "spot") {
            SpotLight3D* light = memnew(SpotLight3D);
            light->set_param(SpotLight3D::PARAM_ATTENUATION, attenuation);
            light->set_param(SpotLight3D::PARAM_RANGE, range);
            light->set_param(SpotLight3D::PARAM_SPOT_ANGLE, Math::rad2deg(l.outer_cone_angle));
            light->set_color(l.color);

            // Line of best fit derived from guessing, see https://www.desmos.com/calculator/biiflubp8b
            // The points in desmos are not exact, except for (1, infinity).
            float angle_ratio = l.inner_cone_angle / l.outer_cone_angle;
            float angle_attenuation = 0.2 / (1 - angle_ratio) - 0.1;
            light->set_param(SpotLight3D::PARAM_SPOT_ATTENUATION, angle_attenuation);
            return light;
        }
        return memnew(Node3D);
    }

    Camera3D* _generate_camera(GLTFState& state, Node* scene_parent, const GLTFNodeIndex node_index) {
        const GLTFNode* gltf_node = state.nodes[node_index];

        ERR_FAIL_INDEX_V(gltf_node->camera, state.cameras.size(), nullptr);

        Camera3D* camera = memnew(Camera3D);
        print_verbose("glTF: Creating camera for: " + gltf_node->name);

        const GLTFCamera& c = state.cameras[gltf_node->camera];
        if (c.perspective) {
            camera->set_perspective(c.fov_size, c.znear, c.zfar);
        }
        else {
            camera->set_orthogonal(c.fov_size, c.znear, c.zfar);
        }

        return camera;
    }

    Node3D* _generate_spatial(GLTFState& state, Node* scene_parent, const GLTFNodeIndex node_index) {
        const GLTFNode* gltf_node = state.nodes[node_index];

        Node3D* spatial = memnew(Node3D);
        print_verbose("glTF: Creating spatial for: " + gltf_node->name);

        return spatial;
    }

    void _generate_scene_node(GLTFState& state, Node* scene_parent, Node3D* scene_root, const GLTFNodeIndex node_index);

    void _generate_skeleton_bone_node(GLTFState &state, Node *scene_parent, Node3D *scene_root, const GLTFNodeIndex node_index) {
        const GLTFNode *gltf_node = state.nodes[node_index];

        Node3D *current_node = nullptr;

        Skeleton *skeleton = state.skeletons[gltf_node->skeleton].godot_skeleton;
        // In this case, this node is already a bone in skeleton.
        const bool is_skinned_mesh = (gltf_node->skin >= 0 && gltf_node->mesh >= 0);
        const bool requires_extra_node = (gltf_node->mesh >= 0 || gltf_node->camera >= 0 || gltf_node->light >= 0);

        Skeleton *active_skeleton = object_cast<Skeleton>(scene_parent);
        if (active_skeleton != skeleton) {
            if (active_skeleton) {
                // Bone Attachment - Direct Parented Skeleton Case
                BoneAttachment3D *bone_attachment = _generate_bone_attachment(state, active_skeleton, node_index, gltf_node->parent);

                scene_parent->add_child(bone_attachment);
                bone_attachment->set_owner(scene_root);

                // There is no gltf_node that represent this, so just directly create a unique name
                bone_attachment->set_name(_gen_unique_name(state, "BoneAttachment"));

                // We change the scene_parent to our bone attachment now. We do not set current_node because we want to make the node
                // and attach it to the bone_attachment
                scene_parent = bone_attachment;
                WARN_PRINT(FormatVE("glTF: Generating scene detected direct parented Skeletons at node %d", node_index));
            }

            // Add it to the scene if it has not already been added
            if (skeleton->get_parent() == nullptr) {
                scene_parent->add_child(skeleton);
                skeleton->set_owner(scene_root);
            }
        }

        active_skeleton = skeleton;
        current_node = skeleton;

        // If we have an active skeleton, and the node is node skinned, we need to create a bone attachment
        if (requires_extra_node) {
            // skinned meshes must not be placed in a bone attachment.
            if (!is_skinned_mesh) {
                BoneAttachment3D *bone_attachment = _generate_bone_attachment(state, active_skeleton, node_index, node_index);

                scene_parent->add_child(bone_attachment);
                bone_attachment->set_owner(scene_root);

                // There is no gltf_node that represent this, so just directly create a unique name
                bone_attachment->set_name(_gen_unique_name(state, "BoneAttachment"));

                // We change the scene_parent to our bone attachment now. We do not set current_node because we want to make the node
                // and attach it to the bone_attachment
                scene_parent = bone_attachment;
                current_node = nullptr;
            }

            // We still have not managed to make a node
            if (gltf_node->mesh >= 0) {
                current_node = _generate_mesh_instance(state, scene_parent, node_index);
            } else if (gltf_node->camera >= 0) {
                current_node = _generate_camera(state, scene_parent, node_index);
            } else if (gltf_node->light >= 0) {
                current_node = _generate_light(state, scene_parent, node_index);
            }

            scene_parent->add_child(current_node);
            current_node->set_owner(scene_root);
            // Do not set transform here. Transform is already applied to our bone.
            if (state.use_legacy_names) {
                current_node->set_name(_legacy_validate_node_name(gltf_node->name));
            } else {
                current_node->set_name(gltf_node->name);
            }
        }

        state.scene_nodes.emplace(node_index, current_node);

        for (int i = 0; i < gltf_node->children.size(); ++i) {
            _generate_scene_node(state, active_skeleton, scene_root, gltf_node->children[i]);
        }
    }

    void _generate_scene_node(GLTFState& state, Node* scene_parent, Node3D* scene_root, const GLTFNodeIndex node_index) {

        const GLTFNode* gltf_node = state.nodes[node_index];

        if (gltf_node->skeleton >= 0) {
            _generate_skeleton_bone_node(state, scene_parent, scene_root, node_index);
            return;
        }

        Node3D* current_node = nullptr;

        // Is our parent a skeleton
        Skeleton* active_skeleton = object_cast<Skeleton>(scene_parent);

        const bool non_bone_parented_to_skeleton = active_skeleton;

        // skinned meshes must not be placed in a bone attachment.
        if (non_bone_parented_to_skeleton && gltf_node->skin < 0) {
            // Bone Attachment - Parent Case
            BoneAttachment3D *bone_attachment = _generate_bone_attachment(state, active_skeleton, node_index, gltf_node->parent);

            scene_parent->add_child(bone_attachment);
            bone_attachment->set_owner(scene_root);

            // There is no gltf_node that represent this, so just directly create a unique name
            bone_attachment->set_name(_gen_unique_name(state, "BoneAttachment"));

            // We change the scene_parent to our bone attachment now. We do not set current_node because we want to make the node
            // and attach it to the bone_attachment
            scene_parent = bone_attachment;
        }

        // We still have not managed to make a node
        if (gltf_node->mesh >= 0) {
            current_node = _generate_mesh_instance(state, scene_parent, node_index);
        } else if (gltf_node->camera >= 0) {
            current_node = _generate_camera(state, scene_parent, node_index);
        } else if (gltf_node->light >= 0) {
            current_node = _generate_light(state, scene_parent, node_index);
        } else {
            current_node = _generate_spatial(state, scene_parent, node_index);
        }

        scene_parent->add_child(current_node);
        current_node->set_owner(scene_root);
        current_node->set_transform(gltf_node->xform);
        if (state.use_legacy_names) {
            current_node->set_name(_legacy_validate_node_name(gltf_node->name));
        } else {
            current_node->set_name(gltf_node->name);
        }

        state.scene_nodes.emplace(node_index, current_node);

        for (int i = 0; i < gltf_node->children.size(); ++i) {
            _generate_scene_node(state, current_node, scene_root, gltf_node->children[i]);
        }
    }




    //thank you for existing, partial specialization
    template <>
    struct EditorSceneImporterGLTFInterpolate<Quat> {

        Quat lerp(const Quat& a, const Quat& b, const float c) const {
            ERR_FAIL_COND_V_MSG(!a.is_normalized(), Quat(), "The quaternion \"a\" must be normalized.");
            ERR_FAIL_COND_V_MSG(!b.is_normalized(), Quat(), "The quaternion \"b\" must be normalized.");

            return a.slerp(b, c).normalized();
        }

        Quat catmull_rom(const Quat& p0, const Quat& p1, const Quat& p2, const Quat& p3, const float c) {
            ERR_FAIL_COND_V_MSG(!p1.is_normalized(), Quat(), "The quaternion \"p1\" must be normalized.");
            ERR_FAIL_COND_V_MSG(!p2.is_normalized(), Quat(), "The quaternion \"p2\" must be normalized.");

            return p1.slerp(p2, c).normalized();
        }

        Quat bezier(const Quat start, const Quat control_1, const Quat control_2, const Quat end, const float t) {
            ERR_FAIL_COND_V_MSG(!start.is_normalized(), Quat(), "The start quaternion must be normalized.");
            ERR_FAIL_COND_V_MSG(!end.is_normalized(), Quat(), "The end quaternion must be normalized.");

            return start.slerp(end, t).normalized();
        }
    };


    void _import_animation(GLTFState& state, AnimationPlayer* ap, const GLTFAnimationIndex index, const int bake_fps) {

        const GLTFAnimation& anim = state.animations[index];

        String name = anim.name;
        if (name.empty()) {
            // No node represent these, and they are not in the hierarchy, so just make a unique name
            name = _gen_unique_name(state, "Animation");
        }

        Ref<Animation> animation(make_ref_counted<Animation>());

        animation->set_name(name);
        if (anim.loop) {
            animation->set_loop(true);
        }
        float length = 0;

        for (const eastl::pair<const int, GLTFAnimation::Track>& E : anim.tracks) {

            const GLTFAnimation::Track& track(E.second);
            //need to find the path: for skeletons, weight tracks will affect the mesh
            NodePath node_path;
            //for skeletons, transform tracks always affect bones
            NodePath transform_node_path;

            GLTFNodeIndex node_index = E.first;

            const GLTFNode* node = state.nodes[E.first];

            Node *root = ap->get_parent();
            ERR_FAIL_COND(root == nullptr);
            auto node_element = state.scene_nodes.find(node_index);
            ERR_CONTINUE_MSG(node_element == state.scene_nodes.end(), FormatVE("Unable to find node %d for animation", node_index));
            node_path = root->get_path_to(node_element->second);


            if (node->skeleton >= 0) {
                const Skeleton *sk = state.skeletons[node->skeleton].godot_skeleton;
                ERR_FAIL_COND(sk == nullptr);

                const String path = (String)ap->get_parent()->get_path_to(sk);
                const StringName bone = node->name;
                transform_node_path = NodePath(path + ":" + bone);
            }
            else {
                transform_node_path = node_path;
            }

            for (size_t i = 0; i < track.rotation_track.times.size(); i++) {
                length = M_MAX(length, track.rotation_track.times[i]);
            }
            for (size_t i = 0; i < track.translation_track.times.size(); i++) {
                length = M_MAX(length, track.translation_track.times[i]);
            }
            for (size_t i = 0; i < track.scale_track.times.size(); i++) {
                length = M_MAX(length, track.scale_track.times[i]);
            }

            for (size_t i = 0; i < track.weight_tracks.size(); i++) {
                for (size_t j = 0; j < track.weight_tracks[i].times.size(); j++) {
                    length = M_MAX(length, track.weight_tracks[i].times[j]);
                }
            }
            // Animated TRS properties will not affect a skinned mesh.
            const bool transform_affects_skinned_mesh_instance = node->skeleton < 0 && node->skin >= 0;
            if ((!track.rotation_track.values.empty() || !track.translation_track.values.empty() || !track.scale_track.values.empty()) && !transform_affects_skinned_mesh_instance) {
                //make transform track
                int track_idx = animation->get_track_count();
                animation->add_track(Animation::TYPE_TRANSFORM);
                animation->track_set_path(track_idx, transform_node_path);
                //first determine animation length

                const float increment = 1.0f / float(bake_fps);
                float time = 0.0;

                Vector3 base_pos;
                Quat base_rot;
                Vector3 base_scale = Vector3(1, 1, 1);

                if (track.rotation_track.values.empty()) {
                    base_rot = state.nodes[E.first]->rotation.normalized();
                }

                if (track.translation_track.values.empty()) {
                    base_pos = state.nodes[E.first]->translation;
                }

                if (track.scale_track.values.empty()) {
                    base_scale = state.nodes[E.first]->scale;
                }

                bool last = false;
                while (true) {

                    Vector3 pos = base_pos;
                    Quat rot = base_rot;
                    Vector3 scale = base_scale;

                    if (!track.translation_track.times.empty()) {

                        pos = _interpolate_track<Vector3>(track.translation_track.times, track.translation_track.values, time, track.translation_track.interpolation);
                    }

                    if (!track.rotation_track.times.empty()) {

                        rot = _interpolate_track<Quat>(track.rotation_track.times, track.rotation_track.values, time, track.rotation_track.interpolation);
                    }

                    if (!track.scale_track.times.empty()) {

                        scale = _interpolate_track<Vector3>(track.scale_track.times, track.scale_track.values, time, track.scale_track.interpolation);
                    }

                    if (node->skeleton >= 0) {

                        Transform xform;
                        xform.basis.set_quat_scale(rot, scale);
                        xform.origin = pos;

                        const Skeleton* skeleton = state.skeletons[node->skeleton].godot_skeleton;
                        const int bone_idx = skeleton->find_bone(node->name);
                        xform = skeleton->get_bone_rest(bone_idx).affine_inverse() * xform;

                        rot = xform.basis.get_rotation_quat();
                        rot.normalize();
                        scale = xform.basis.get_scale();
                        pos = xform.origin;
                    }

                    animation->transform_track_insert_key(track_idx, time, pos, rot, scale);

                    if (last) {
                        break;
                    }
                    time += increment;
                    if (time >= length) {
                        last = true;
                        time = length;
                    }
                }
            }

            for (int i = 0; i < track.weight_tracks.size(); i++) {
                ERR_CONTINUE(node->mesh < 0 || node->mesh >= state.meshes.size());
                const GLTFMesh& mesh = state.meshes[node->mesh];
                const StringName prop = "blend_shapes/" + mesh.mesh->get_blend_shape_name(i);

                const String blend_path = String(node_path) + ":" + prop;

                const int track_idx = animation->get_track_count();
                animation->add_track(Animation::TYPE_VALUE);
                animation->track_set_path(track_idx, NodePath(blend_path));

                // Only LINEAR and STEP (NEAREST) can be supported out of the box by Godot's Animation,
                // the other modes have to be baked.
                GLTFAnimation::Interpolation gltf_interp = track.weight_tracks[i].interpolation;
                if (gltf_interp == GLTFAnimation::INTERP_LINEAR || gltf_interp == GLTFAnimation::INTERP_STEP) {
                    animation->track_set_interpolation_type(track_idx, gltf_interp == GLTFAnimation::INTERP_STEP ? Animation::INTERPOLATION_NEAREST : Animation::INTERPOLATION_LINEAR);
                    for (int j = 0; j < track.weight_tracks[i].times.size(); j++) {
                        const float t = track.weight_tracks[i].times[j];
                        const float w = track.weight_tracks[i].values[j];
                        animation->track_insert_key(track_idx, t, w);
                    }
                }
                else {
                    // CATMULLROMSPLINE or CUBIC_SPLINE have to be baked, apologies.
                    const float increment = 1.0f / float(bake_fps);
                    float time = 0.0;
                    bool last = false;
                    while (true) {
                        _interpolate_track<float>(track.weight_tracks[i].times, track.weight_tracks[i].values, time, gltf_interp);
                        if (last) {
                            break;
                        }
                        time += increment;
                        if (time >= length) {
                            last = true;
                            time = length;
                        }
                    }
                }
            }
        }
        animation->set_length(length);

        ap->add_animation(StringName(name), animation);
    }

    void _process_mesh_instances(GLTFState& state, Node3D* scene_root) {
        for (GLTFNodeIndex node_i = 0; node_i < state.nodes.size(); ++node_i) {
            const GLTFNode* node = state.nodes[node_i];

            if (node->skin < 0 || node->mesh < 0)
                continue;

            const GLTFSkinIndex skin_i = node->skin;

            auto mi_element = state.scene_nodes.find(node_i);
            ERR_CONTINUE_MSG(mi_element == state.scene_nodes.end(),FormatVE("Unable to find node %d", node_i));
            MeshInstance3D* mi = object_cast<MeshInstance3D>(mi_element->second);
            ERR_CONTINUE_MSG(mi == nullptr, FormatVE("Unable to cast node %d of type %s to MeshInstance", node_i, mi_element->second->get_class_name().asCString()));

            const GLTFSkeletonIndex skel_i = state.skins[node->skin].skeleton;
            const GLTFSkeleton& gltf_skeleton = state.skeletons[skel_i];
            Skeleton* skeleton = gltf_skeleton.godot_skeleton;
            ERR_CONTINUE_MSG(skeleton == nullptr, FormatVE("Unable to find Skeleton for node %d skin %d", node_i, skin_i));

            mi->get_parent()->remove_child(mi);
            skeleton->add_child(mi);
            mi->set_owner(scene_root);

            mi->set_skin(state.skins[skin_i].godot_skin);
            mi->set_skeleton_path(mi->get_path_to(skeleton));
            mi->set_transform(Transform());
        }
    }

    Node3D* _generate_scene(GLTFState& state, const int p_bake_fps) {

        Node3D* root = memnew(Node3D);

        // scene_name is already unique
        if (state.use_legacy_names) {
            root->set_name(_legacy_validate_node_name(state.scene_name));
        } else {
        root->set_name(state.scene_name);
        }

        for (int i = 0; i < state.root_nodes.size(); ++i) {
            _generate_scene_node(state, root, root, state.root_nodes[i]);
        }

        _process_mesh_instances(state, root);

        if (!state.animations.empty()) {
            AnimationPlayer* ap = memnew(AnimationPlayer);
            ap->set_name("AnimationPlayer");
            root->add_child(ap);
            ap->set_owner(root);

            for (int i = 0; i < state.animations.size(); i++) {
                _import_animation(state, ap, i, p_bake_fps);
            }
        }

        return root;
    }
}


Node *EditorSceneImporterGLTF::import_scene(StringView p_path, uint32_t p_flags, int p_bake_fps, uint32_t p_compress_flags, Vector<String> *r_missing_deps, Error *r_err) {
    print_verbose(FormatVE("glTF: Importing file %.*s as scene.", (int)p_path.size(),p_path.data()));

    GLTFState state;

    if (StringUtils::ends_with(StringUtils::to_lower(p_path),"glb")) {
        //binary file
        //text file
        Error err = _parse_glb(p_path, state);
        if (err)
            return nullptr;
    } else {
        //text file
        Error err = _parse_json(p_path, state);
        if (err)
            return nullptr;
    }

    ERR_FAIL_COND_V(!state.json.has("asset"), nullptr);

    Dictionary asset = state.json["asset"].as<Dictionary>();

    ERR_FAIL_COND_V(!asset.has("version"), nullptr);

    String version = asset["version"].as<String>();

    state.major_version = StringUtils::to_int(StringUtils::get_slice(version,".", 0));
    state.minor_version = StringUtils::to_int(StringUtils::get_slice(version,".", 1));
    state.use_named_skin_binds = p_flags & IMPORT_USE_NAMED_SKIN_BINDS;
    state.use_legacy_names = p_flags & IMPORT_USE_LEGACY_NAMES;

    /* STEP 0 PARSE SCENE */
    Error err = _parse_scenes(state);
    if (err != OK)
        return nullptr;

    /* STEP 1 PARSE NODES */
    err = _parse_nodes(state);
    if (err != OK)
        return nullptr;

    /* STEP 2 PARSE BUFFERS */
    err = _parse_buffers(state, PathUtils::get_base_dir(p_path));
    if (err != OK)
        return nullptr;

    /* STEP 3 PARSE BUFFER VIEWS */
    err = _parse_buffer_views(state);
    if (err != OK)
        return nullptr;

    /* STEP 4 PARSE ACCESSORS */
    err = _parse_accessors(state);
    if (err != OK)
        return nullptr;

    /* STEP 5 PARSE IMAGES */
    err = _parse_images(state, PathUtils::get_base_dir(p_path));
    if (err != OK)
        return nullptr;

    /* STEP 6 PARSE TEXTURES */
    err = _parse_textures(state);
    if (err != OK)
        return nullptr;

    /* STEP 7 PARSE TEXTURES */
    err = _parse_materials(state);
    if (err != OK)
        return nullptr;

    /* STEP 9 PARSE SKINS */
    err = _parse_skins(state);
    if (err != OK)
        return nullptr;

    /* STEP 10 DETERMINE SKELETONS */
    err = _determine_skeletons(state);
    if (err != OK)
        return nullptr;

    /* STEP 11 CREATE SKELETONS */
    err = _create_skeletons(state);
    if (err != OK)
        return nullptr;

    /* STEP 12 CREATE SKINS */
    err = _create_skins(state);
    if (err != OK)
        return nullptr;

    /* STEP 13 PARSE MESHES (we have enough info now) */
    err = _parse_meshes(state);
    if (err != OK)
        return nullptr;

    /* STEP 14 PARSE LIGHTS */
    err = _parse_lights(state);
    if (err != OK) {
        return nullptr;
    }

    /* STEP 15 PARSE CAMERAS */
    err = _parse_cameras(state);
    if (err != OK)
        return nullptr;

    /* STEP 16 PARSE ANIMATIONS */
    err = _parse_animations(state);
    if (err != OK)
        return nullptr;

    /* STEP 17 ASSIGN SCENE NAMES */
    _assign_scene_names(state);

    /* STEP 18 MAKE SCENE! */
    Node3D *scene = _generate_scene(state, p_bake_fps);

    return scene;
}
uint32_t EditorSceneImporterGLTF::get_import_flags() const {

    return IMPORT_SCENE | IMPORT_ANIMATION;
}
void EditorSceneImporterGLTF::get_extensions(Vector<String>& r_extensions) const {

    r_extensions.push_back("gltf");
    r_extensions.push_back("glb");
}

Ref<Animation> EditorSceneImporterGLTF::import_animation(StringView p_path, uint32_t p_flags, int p_bake_fps) {

    return Ref<Animation>();
}

EditorSceneImporterGLTF::EditorSceneImporterGLTF() {}
