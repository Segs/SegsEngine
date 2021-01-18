/*************************************************************************/
/*  rendering_server.cpp                                                    */
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

#include "rendering_server.h"
#include "rendering_server_enum_casters.h"

#include "core/image_enum_casters.h"
#include "core/method_bind.h"
#include "core/project_settings.h"

IMPL_GDCLASS(RenderingServer)
RenderingServer *RenderingServer::submission_thread_singleton = nullptr;
RenderingServer* RenderingServer::queueing_thread_singleton = nullptr;
Thread::ID RenderingServer::server_thread;

RID RenderingServer::texture_create_from_image(const Ref<Image> &p_image, uint32_t p_flags) {
    ERR_FAIL_COND_V(not p_image, RID());
    RID texture = texture_create();
    texture_allocate(texture, p_image->get_width(), p_image->get_height(), 0, p_image->get_format(),
            RS::TEXTURE_TYPE_2D, p_flags); // if it has mipmaps, use, else generate
    ERR_FAIL_COND_V(!texture.is_valid(), texture);

    texture_set_data(texture, p_image);

    return texture;
}

Array RenderingServer::_texture_debug_usage_bind() {
    Vector<TextureInfo> tex_infos;
    texture_debug_usage(&tex_infos);
    Array arr;
    for (const TextureInfo &E : tex_infos) {
        Dictionary dict;
        dict["texture"] = E.texture;
        dict["width"] = E.width;
        dict["height"] = E.height;
        dict["depth"] = E.depth;
        dict["format"] = E.format;
        dict["bytes"] = E.bytes;
        dict["path"] = E.path;
        arr.push_back(dict);
    }
    return arr;
}

Array RenderingServer::_shader_get_param_list_bind(RID p_shader) const {
    Vector<PropertyInfo> l;
    shader_get_param_list(p_shader, &l);
    return convert_property_vector(l);
}

static Array to_array(const Vector<ObjectID> &ids) {
    Array a;
    a.resize(ids.size());
    for (int i = 0; i < ids.size(); ++i) {
        a[i] = Variant::from(ids[i]);
    }
    return a;
}

Array RenderingServer::_instances_cull_aabb_bind(const AABB &p_aabb, RID p_scenario) const {
    Vector<ObjectID> ids = instances_cull_aabb(p_aabb, p_scenario);
    return to_array(ids);
}

Array RenderingServer::_instances_cull_ray_bind(const Vector3 &p_from, const Vector3 &p_to, RID p_scenario) const {
    Vector<ObjectID> ids = instances_cull_ray(p_from, p_to, p_scenario);
    return to_array(ids);
}

Array RenderingServer::_instances_cull_convex_bind(const Array &p_convex, RID p_scenario) const {
    // TODO: SEGS: use a fixed vector here, with a sane'ish number of on-stack entries, and marked as growing to allow
    // for larger counts.
    Vector<Plane> planes;
    planes.reserve(p_convex.size());
    for (int i = 0; i < p_convex.size(); ++i) {
        Variant v = p_convex[i];
        ERR_FAIL_COND_V(v.get_type() != VariantType::PLANE, Array());
        planes.push_back(v.as<Plane>());
    }

    Vector<ObjectID> ids = instances_cull_convex(planes, p_scenario);
    return to_array(ids);
}

RID RenderingServer::make_sphere_mesh(int p_lats, int p_lons, float p_radius) {
    Vector<Vector3> vertices;
    Vector<Vector3> normals;

    for (int i = 1; i <= p_lats; i++) {
        double lat0 = Math_PI * (-0.5 + (double)(i - 1) / p_lats);
        double z0 = Math::sin(lat0);
        double zr0 = Math::cos(lat0);

        double lat1 = Math_PI * (-0.5 + (double)i / p_lats);
        double z1 = Math::sin(lat1);
        double zr1 = Math::cos(lat1);

        for (int j = p_lons; j >= 1; j--) {
            double lng0 = 2 * Math_PI * (double)(j - 1) / p_lons;
            double x0 = Math::cos(lng0);
            double y0 = Math::sin(lng0);

            double lng1 = 2 * Math_PI * (double)(j) / p_lons;
            double x1 = Math::cos(lng1);
            double y1 = Math::sin(lng1);

            Vector3 v[4] = { Vector3(x1 * zr0, z0, y1 * zr0), Vector3(x1 * zr1, z1, y1 * zr1),
                Vector3(x0 * zr1, z1, y0 * zr1), Vector3(x0 * zr0, z0, y0 * zr0) };

#define ADD_POINT(m_idx)                                                                                               \
    normals.push_back(v[m_idx]);                                                                                       \
    vertices.push_back(v[m_idx] * p_radius);

            ADD_POINT(0)
            ADD_POINT(1)
            ADD_POINT(2)

            ADD_POINT(2)
            ADD_POINT(3)
            ADD_POINT(0)
        }
    }

    RID mesh = mesh_create();
    SurfaceArrays d(eastl::move(vertices));
    d.m_normals = eastl::move(normals);

    mesh_add_surface_from_arrays(mesh, RS::PRIMITIVE_TRIANGLES, d);

    return mesh;
}

namespace {
constexpr Vector2 SMALL_VEC2(0.00001f, 0.00001f);
constexpr Vector3 SMALL_VEC3(0.00001f, 0.00001f, 0.00001f);

} // namespace

Error RenderingServer::_surface_set_data(const SurfaceArrays &p_arrays, uint32_t p_format, uint32_t *p_offsets,
        uint32_t p_stride, Vector<uint8_t> &r_vertex_array, int p_vertex_array_len, Vector<uint8_t> &r_index_array,
        int p_index_array_len, AABB &r_aabb, Vector<AABB> &r_bone_aabb) {
    int max_bone = 0;

    for (int ai = 0; ai < RS::ARRAY_MAX; ai++) {
        if (!(p_format & (1 << ai))) { // no array
            continue;
        }

        switch (ai) {
            case RS::ARRAY_VERTEX: {
                if (p_format & RS::ARRAY_FLAG_USE_2D_VERTICES) {
                    Span<const Vector2> array = p_arrays.positions2();
                    ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

                    const Vector2 *src = array.data();

                    // setting vertices means regenerating the AABB
                    Rect2 aabb;

                    if (p_format & RS::ARRAY_COMPRESS_VERTEX) {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            uint16_t vector[2] = { Math::make_half_float(src[i].x), Math::make_half_float(src[i].y) };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], vector, sizeof(uint16_t) * 2);

                            if (i == 0) {
                                aabb = Rect2(src[i], SMALL_VEC2); // must have a bit of size
                            } else {
                                aabb.expand_to(src[i]);
                            }
                        }

                    } else {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            float vector[2] = { src[i].x, src[i].y };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], vector, sizeof(float) * 2);

                            if (i == 0) {
                                aabb = Rect2(src[i], SMALL_VEC2); // must have a bit of size
                            } else {
                                aabb.expand_to(src[i]);
                            }
                        }
                    }

                    r_aabb = AABB(Vector3(aabb.position.x, aabb.position.y, 0), Vector3(aabb.size.x, aabb.size.y, 0));

                } else {
                    Span<const Vector3> array = p_arrays.positions3();
                    ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

                    const Vector3 *src = array.data();

                    // setting vertices means regenerating the AABB
                    AABB aabb;

                    if (p_format & RS::ARRAY_COMPRESS_VERTEX) {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            uint16_t vector[4] = { Math::make_half_float(src[i].x), Math::make_half_float(src[i].y),
                                Math::make_half_float(src[i].z), Math::make_half_float(1.0) };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], vector, sizeof(uint16_t) * 4);

                            if (i == 0) {
                                aabb = AABB(src[i], SMALL_VEC3);
                            } else {
                                aabb.expand_to(src[i]);
                            }
                        }

                    } else {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            float vector[3] = { src[i].x, src[i].y, src[i].z };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], vector, sizeof(float) * 3);

                            if (i == 0) {
                                aabb = AABB(src[i], SMALL_VEC3);
                            } else {
                                aabb.expand_to(src[i]);
                            }
                        }
                    }

                    r_aabb = aabb;
                }

            } break;
            case RS::ARRAY_NORMAL: {
                const auto &array = p_arrays.m_normals;
                ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

                const Vector3 *src = array.data();

                // setting vertices means regenerating the AABB

                if (p_format & RS::ARRAY_COMPRESS_NORMAL) {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        int8_t vector[4] = {
                            (int8_t)CLAMP(src[i].x * 127, -128, 127),
                            (int8_t)CLAMP(src[i].y * 127, -128, 127),
                            (int8_t)CLAMP(src[i].z * 127, -128, 127),
                            0,
                        };

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], vector, 4);
                    }

                } else {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        float vector[3] = { src[i].x, src[i].y, src[i].z };
                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], vector, 3 * 4);
                    }
                }

            } break;

            case RS::ARRAY_TANGENT: {
                const Vector<float> &array = p_arrays.m_tangents;

                ERR_FAIL_COND_V(array.size() != p_vertex_array_len * 4, ERR_INVALID_PARAMETER);

                const real_t *src = array.data();

                if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        int8_t xyzw[4] = { (int8_t)CLAMP(src[i * 4 + 0] * 127, -128, 127),
                            (int8_t)CLAMP(src[i * 4 + 1] * 127, -128, 127),
                            (int8_t)CLAMP(src[i * 4 + 2] * 127, -128, 127),
                            (int8_t)CLAMP(src[i * 4 + 3] * 127, -128, 127) };

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], xyzw, 4);
                    }

                } else {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        float xyzw[4] = { src[i * 4 + 0], src[i * 4 + 1], src[i * 4 + 2], src[i * 4 + 3] };

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], xyzw, 4 * 4);
                    }
                }

            } break;
            case RS::ARRAY_COLOR: {
                const Vector<Color> &array = p_arrays.m_colors;

                ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

                const Color *src = array.data();

                if (p_format & RS::ARRAY_COMPRESS_COLOR) {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        uint8_t colors[4];

                        for (int j = 0; j < 4; j++) {
                            colors[j] = CLAMP(int((src[i][j]) * 255.0f), 0, 255);
                        }

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], colors, 4);
                    }
                } else {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], &src[i], 4 * 4);
                    }
                }

            } break;
            case RS::ARRAY_TEX_UV: {
                // TODO: ERR_FAIL_COND_V(p_arrays[ai].get_type() != VariantType::POOL_VECTOR3_ARRAY &&
                // p_arrays[ai].get_type() != VariantType::POOL_VECTOR2_ARRAY, ERR_INVALID_PARAMETER);

                const auto &array = p_arrays.m_uv_1;

                ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

                const Vector2 *src = array.data();

                if (p_format & RS::ARRAY_COMPRESS_TEX_UV) {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        uint16_t uv[2] = { Math::make_half_float(src[i].x), Math::make_half_float(src[i].y) };
                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], uv, 2 * 2);
                    }

                } else {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        float uv[2] = { src[i].x, src[i].y };

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], uv, 2 * 4);
                    }
                }

            } break;

            case RS::ARRAY_TEX_UV2: {
                // TODO: ERR_FAIL_COND_V(p_arrays[ai].get_type() != VariantType::POOL_VECTOR3_ARRAY &&
                // p_arrays[ai].get_type() != VariantType::POOL_VECTOR2_ARRAY, ERR_INVALID_PARAMETER);
                const auto &array = p_arrays.m_uv_2;

                ERR_FAIL_COND_V(array.size() != p_vertex_array_len, ERR_INVALID_PARAMETER);

                const Vector2 *src = array.data();

                if (p_format & RS::ARRAY_COMPRESS_TEX_UV2) {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        uint16_t uv[2] = { Math::make_half_float(src[i].x), Math::make_half_float(src[i].y) };
                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], uv, 2 * 2);
                    }

                } else {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        float uv[2] = { src[i].x, src[i].y };

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], uv, 2 * 4);
                    }
                }
            } break;
            case RS::ARRAY_WEIGHTS: {
                // ERR_FAIL_COND_V(p_arrays[ai].get_type() != VariantType::POOL_REAL_ARRAY, ERR_INVALID_PARAMETER);

                const Vector<real_t> &array = p_arrays.m_weights;

                ERR_FAIL_COND_V(array.size() != p_vertex_array_len * RS::ARRAY_WEIGHTS_SIZE, ERR_INVALID_PARAMETER);

                const real_t *src = array.data();

                if (p_format & RS::ARRAY_COMPRESS_WEIGHTS) {
                    for (size_t i = 0; i < p_vertex_array_len; i++) {
                        uint16_t data[RS::ARRAY_WEIGHTS_SIZE];
                        for (size_t j = 0; j < RS::ARRAY_WEIGHTS_SIZE; j++) {
                            data[j] = CLAMP(src[i * RS::ARRAY_WEIGHTS_SIZE + j] * 65535, 0, 65535);
                        }

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], data, 2 * 4);
                    }
                } else {
                    for (size_t i = 0; i < p_vertex_array_len; i++) {
                        float data[RS::ARRAY_WEIGHTS_SIZE];
                        for (int j = 0; j < RS::ARRAY_WEIGHTS_SIZE; j++) {
                            data[j] = src[i * RS::ARRAY_WEIGHTS_SIZE + j];
                        }

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], data, 4 * 4);
                    }
                }

            } break;
            case RS::ARRAY_BONES: {
                // ERR_FAIL_COND_V(p_arrays[ai].get_type() != VariantType::POOL_INT_ARRAY && p_arrays[ai].get_type() !=
                // VariantType::POOL_REAL_ARRAY, ERR_INVALID_PARAMETER);

                const auto &array = p_arrays.m_bones;

                ERR_FAIL_COND_V(array.size() != p_vertex_array_len * RS::ARRAY_WEIGHTS_SIZE, ERR_INVALID_PARAMETER);

                const int *src = array.data();

                if (!(p_format & RS::ARRAY_FLAG_USE_16_BIT_BONES)) {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        uint8_t data[RS::ARRAY_WEIGHTS_SIZE];
                        for (int j = 0; j < RS::ARRAY_WEIGHTS_SIZE; j++) {
                            data[j] = CLAMP(src[i * RS::ARRAY_WEIGHTS_SIZE + j], 0, 255);
                            max_bone = M_MAX(data[j], max_bone);
                        }

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], data, 4);
                    }

                } else {
                    for (size_t i = 0; i < p_vertex_array_len; i++) {
                        uint16_t data[RS::ARRAY_WEIGHTS_SIZE];
                        for (size_t j = 0; j < RS::ARRAY_WEIGHTS_SIZE; j++) {
                            data[j] = src[i * RS::ARRAY_WEIGHTS_SIZE + j];
                            max_bone = M_MAX(data[j], max_bone);
                        }

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride], data, 2 * 4);
                    }
                }

            } break;
            case RS::ARRAY_INDEX: {
                ERR_FAIL_COND_V(p_index_array_len <= 0, ERR_INVALID_DATA);
                // ERR_FAIL_COND_V(p_arrays[ai].get_type() != VariantType::POOL_INT_ARRAY, ERR_INVALID_PARAMETER);

                const Vector<int> &indices = p_arrays.m_indices;
                ERR_FAIL_COND_V(indices.empty(), ERR_INVALID_PARAMETER);
                ERR_FAIL_COND_V(indices.size() != p_index_array_len, ERR_INVALID_PARAMETER);

                /* determine whether using 16 or 32 bits indices */

                const int *src = indices.data();

                for (int i = 0; i < p_index_array_len; i++) {
                    if (p_vertex_array_len < (1 << 16)) {
                        uint16_t v = src[i];

                        memcpy(&r_index_array[i * 2], &v, 2);
                    } else {
                        uint32_t v = src[i];

                        memcpy(&r_index_array[i * 4], &v, 4);
                    }
                }
            } break;
            default: {
                ERR_FAIL_V(ERR_INVALID_DATA);
            }
        }
    }

    if (p_format & RS::ARRAY_FORMAT_BONES) {
        // create AABBs for each detected bone
        int total_bones = max_bone + 1;

        bool first = r_bone_aabb.empty();

        r_bone_aabb.resize(total_bones);

        if (first) {
            for (int i = 0; i < total_bones; i++) {
                r_bone_aabb[i].size = Vector3(-1, -1, -1); // negative means unused
            }
        }

        auto vertices = p_arrays.positions3();
        const Vector<int> &bones = p_arrays.m_bones;
        const Vector<float> &weights = p_arrays.m_weights;

        bool any_valid = false;

        if (vertices.size() && bones.size() == vertices.size() * 4 && weights.size() == bones.size()) {
            uint32_t vs = vertices.size();

            AABB *bptr = r_bone_aabb.data();

            for (uint32_t i = 0; i < vs; i++) {
                Vector3 v = vertices[i];
                for (uint32_t j = 0; j < 4; j++) {
                    int idx = bones[i * 4 + j];
                    float w = weights[i * 4 + j];
                    if (w == 0.0f) {
                        continue; // break;
                    }
                    ERR_FAIL_INDEX_V(idx, total_bones, ERR_INVALID_DATA);

                    if (bptr[idx].size.x < 0) {
                        // first
                        bptr[idx] = AABB(v, SMALL_VEC3);
                        any_valid = true;
                    } else {
                        bptr[idx].expand_to(v);
                    }
                }
            }
        }

        if (!any_valid && first) {
            r_bone_aabb.clear();
        }
    }
    return OK;
}

uint32_t RenderingServer::mesh_surface_get_format_offset(
        uint32_t p_format, int p_vertex_len, int p_index_len, int p_array_index) const {
    uint32_t offsets[RS::ARRAY_MAX];
    mesh_surface_make_offsets_from_format(p_format, p_vertex_len, p_index_len, offsets);
    return offsets[p_array_index];
}

uint32_t RenderingServer::mesh_surface_get_format_stride(uint32_t p_format, int p_vertex_len, int p_index_len) const {
    uint32_t offsets[RS::ARRAY_MAX];
    return mesh_surface_make_offsets_from_format(p_format, p_vertex_len, p_index_len, offsets);
}

uint32_t RenderingServer::mesh_surface_make_offsets_from_format(
        uint32_t p_format, int p_vertex_len, int p_index_len, uint32_t *r_offsets) const {
    int total_elem_size = 0;

    for (int i = 0; i < RS::ARRAY_MAX; i++) {
        r_offsets[i] = 0; // reset

        if (!(p_format & (1 << i))) { // no array
            continue;
        }

        int elem_size = 0;

        switch (i) {
            case RS::ARRAY_VERTEX: {
                if (p_format & RS::ARRAY_FLAG_USE_2D_VERTICES) {
                    elem_size = 2;
                } else {
                    elem_size = 3;
                }

                if (p_format & RS::ARRAY_COMPRESS_VERTEX) {
                    elem_size *= sizeof(int16_t);
                } else {
                    elem_size *= sizeof(float);
                }

                if (elem_size == 6) {
                    elem_size = 8;
                }

            } break;
            case RS::ARRAY_NORMAL: {
                if (p_format & RS::ARRAY_COMPRESS_NORMAL) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 3;
                }

            } break;

            case RS::ARRAY_TANGENT: {
                if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 4;
                }

            } break;
            case RS::ARRAY_COLOR: {
                if (p_format & RS::ARRAY_COMPRESS_COLOR) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 4;
                }
            } break;
            case RS::ARRAY_TEX_UV: {
                if (p_format & RS::ARRAY_COMPRESS_TEX_UV) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 2;
                }

            } break;

            case RS::ARRAY_TEX_UV2: {
                if (p_format & RS::ARRAY_COMPRESS_TEX_UV2) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 2;
                }

            } break;
            case RS::ARRAY_WEIGHTS: {
                if (p_format & RS::ARRAY_COMPRESS_WEIGHTS) {
                    elem_size = sizeof(uint16_t) * 4;
                } else {
                    elem_size = sizeof(float) * 4;
                }

            } break;
            case RS::ARRAY_BONES: {
                if (p_format & RS::ARRAY_FLAG_USE_16_BIT_BONES) {
                    elem_size = sizeof(uint16_t) * 4;
                } else {
                    elem_size = sizeof(uint32_t);
                }

            } break;
            case RS::ARRAY_INDEX: {
                if (p_index_len <= 0) {
                    ERR_PRINT("index_array_len==NO_INDEX_ARRAY");
                    break;
                }
                /* determine whether using 16 or 32 bits indices */
                if (p_vertex_len >= (1 << 16)) {
                    elem_size = 4;

                } else {
                    elem_size = 2;
                }
                r_offsets[i] = elem_size;
                continue;
            }
            default: {
                ERR_FAIL_V(0);
            }
        }

        r_offsets[i] = total_elem_size;
        total_elem_size += elem_size;
    }
    return total_elem_size;
}

void RenderingServer::mesh_add_surface_from_arrays(RID p_mesh, RS::PrimitiveType p_primitive,
        const SurfaceArrays &p_arrays, Vector<SurfaceArrays> &&p_blend_shapes, uint32_t p_compress_format) {
    ERR_FAIL_INDEX(p_primitive, RS::PRIMITIVE_MAX);

    uint32_t format = p_arrays.get_flags();

    // validation
    int index_array_len = 0;
    int array_len = 0;
    ERR_FAIL_COND(p_arrays.empty());

    if (!p_arrays.empty()) {
        if (p_arrays.m_vertices_2d) {
            array_len = p_arrays.positions2().size();
        } else {
            array_len = p_arrays.positions3().size();
        }
    }
    uint32_t offsets[RS::ARRAY_MAX];
    memset(offsets, 0, RS::ARRAY_MAX * sizeof(uint32_t));

    int total_elem_size = 0;

    int elem_size = 0;

    { // per-Vertex calc
        if (p_arrays.m_vertices_2d) {
            elem_size = 2;
            p_compress_format |= RS::ARRAY_FLAG_USE_2D_VERTICES;
        } else {
            p_compress_format &= ~RS::ARRAY_FLAG_USE_2D_VERTICES;
            elem_size = 3;
        }

        if (p_compress_format & RS::ARRAY_COMPRESS_VERTEX) {
            elem_size *= sizeof(int16_t);
        } else {
            elem_size *= sizeof(float);
        }

        if (elem_size == 6) {
            // had to pad
            elem_size = 8;
        }
        offsets[RS::ARRAY_VERTEX] = total_elem_size;
        total_elem_size += elem_size;
    }
    if (!p_arrays.m_normals.empty()) {
        if (p_compress_format & RS::ARRAY_COMPRESS_NORMAL) {
            elem_size = sizeof(uint32_t);
        } else {
            elem_size = sizeof(float) * 3;
        }

        offsets[RS::ARRAY_NORMAL] = total_elem_size;
        total_elem_size += elem_size;
    }
    if (!p_arrays.m_tangents.empty()) {
        if (p_compress_format & RS::ARRAY_COMPRESS_TANGENT) {
            elem_size = sizeof(uint32_t);
        } else {
            elem_size = sizeof(float) * 4;
        }

        offsets[RS::ARRAY_TANGENT] = total_elem_size;
        total_elem_size += elem_size;
    }
    if (!p_arrays.m_colors.empty()) {
        if (p_compress_format & RS::ARRAY_COMPRESS_COLOR) {
            elem_size = sizeof(uint32_t);
        } else {
            elem_size = sizeof(float) * 4;
        }
        offsets[RS::ARRAY_COLOR] = total_elem_size;
        total_elem_size += elem_size;
    }
    if (!p_arrays.m_uv_1.empty()) {
        if (p_compress_format & RS::ARRAY_COMPRESS_TEX_UV) {
            elem_size = sizeof(uint32_t);
        } else {
            elem_size = sizeof(float) * 2;
        }
        offsets[RS::ARRAY_TEX_UV] = total_elem_size;
        total_elem_size += elem_size;
    }
    if (!p_arrays.m_uv_2.empty()) {
        if (p_compress_format & RS::ARRAY_COMPRESS_TEX_UV2) {
            elem_size = sizeof(uint32_t);
        } else {
            elem_size = sizeof(float) * 2;
        }
        offsets[RS::ARRAY_TEX_UV2] = total_elem_size;
        total_elem_size += elem_size;
    }
    if (!p_arrays.m_weights.empty()) {
        if (p_compress_format & RS::ARRAY_COMPRESS_WEIGHTS) {
            elem_size = sizeof(uint16_t) * 4;
        } else {
            elem_size = sizeof(float) * 4;
        }
        offsets[RS::ARRAY_WEIGHTS] = total_elem_size;
        total_elem_size += elem_size;
    }
    if (!p_arrays.m_bones.empty()) {
        const auto &bones = p_arrays.m_bones;
        int max_bone = 0;

        int bc = bones.size();
        for (int j = 0; j < bc; j++) {
            max_bone = M_MAX(bones[j], max_bone);
        }

        if (max_bone > 255) {
            p_compress_format |= RS::ARRAY_FLAG_USE_16_BIT_BONES;
            elem_size = sizeof(uint16_t) * 4;
        } else {
            p_compress_format &= ~RS::ARRAY_FLAG_USE_16_BIT_BONES;
            elem_size = sizeof(uint32_t);
        }
        offsets[RS::ARRAY_BONES] = total_elem_size;
        total_elem_size += elem_size;
    }
    if (!p_arrays.m_indices.empty()) {
        index_array_len = p_arrays.m_indices.size();
        if (index_array_len <= 0) {
            ERR_PRINT("index_array_len==NO_INDEX_ARRAY");
        } else {
            /* determine whether using 16 or 32 bits indices */
            if (array_len >= (1 << 16)) {
                elem_size = 4;

            } else {
                elem_size = 2;
            }
            offsets[RS::ARRAY_INDEX] = elem_size;
        }
    }

    ERR_FAIL_COND((format & RS::ARRAY_FORMAT_VERTEX) == 0); // mandatory

    if (!p_blend_shapes.empty()) {
        // validate format for morphs
        for (int i = 0; i < p_blend_shapes.size(); i++) {
            const SurfaceArrays &arr = p_blend_shapes[i];
            uint32_t bsformat = arr.get_flags();

            ERR_FAIL_COND((bsformat) != (format & (RS::ARRAY_FORMAT_INDEX - 1)));
        }
    }

    uint32_t mask = (1 << RS::ARRAY_MAX) - 1;
    format |= (~mask) & p_compress_format; // make the full format

    int array_size = total_elem_size * array_len;

    Vector<uint8_t> vertex_array;
    vertex_array.resize(array_size);

    int index_array_size = offsets[RS::ARRAY_INDEX] * index_array_len;

    Vector<uint8_t> index_array;
    index_array.resize(index_array_size);

    AABB aabb;
    Vector<AABB> bone_aabb;

    Error err = _surface_set_data(p_arrays, format, offsets, total_elem_size, vertex_array, array_len, index_array,
            index_array_len, aabb, bone_aabb);
    ERR_FAIL_COND_MSG(err, "Invalid array format for surface.");

    Vector<PoolVector<uint8_t>> blend_shape_data;

    for (const SurfaceArrays &p_blend_shape : p_blend_shapes) {
        Vector<uint8_t> vertex_array_shape;
        vertex_array_shape.resize(array_size);
        Vector<uint8_t> noindex;

        AABB laabb;
        Error err2 = _surface_set_data(p_blend_shape, format & ~RS::ARRAY_FORMAT_INDEX, offsets, total_elem_size,
                vertex_array_shape, array_len, noindex, 0, laabb, bone_aabb);
        aabb.merge_with(laabb);
        ERR_FAIL_COND_MSG(err2 != OK, "Invalid blend shape array format for surface.");
        blend_shape_data.emplace_back(eastl::move(PoolVector<uint8_t>(vertex_array_shape)));
    }
    // TODO: The operations below are inefficient
    // printf("Inefficient surface arrays operation\n");

    mesh_add_surface(p_mesh, format, p_primitive, PoolVector(vertex_array), array_len, PoolVector(index_array),
            index_array_len, aabb, blend_shape_data, PoolVector(bone_aabb));
}

static SurfaceArrays _get_array_from_surface(uint32_t p_format, Span<const uint8_t> p_vertex_data,
        uint32_t p_vertex_len, Span<const uint8_t> p_index_data, int p_index_len) {
    uint32_t offsets[RS::ARRAY_MAX];

    uint32_t total_elem_size = 0;

    for (uint32_t i = 0; i < RS::ARRAY_MAX; i++) {
        offsets[i] = 0; // reset

        if (!(p_format & (1 << i))) { // no array
            continue;
        }

        uint32_t elem_size = 0;

        switch (i) {
            case RS::ARRAY_VERTEX: {
                elem_size = (p_format & RS::ARRAY_FLAG_USE_2D_VERTICES) ? 2 : 3;

                elem_size *= (p_format & RS::ARRAY_COMPRESS_VERTEX) ? sizeof(int16_t) : sizeof(float);

                if (elem_size == 6) {
                    elem_size = 8;
                }

            } break;
            case RS::ARRAY_NORMAL: {
                if (p_format & RS::ARRAY_COMPRESS_NORMAL) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 3;
                }

            } break;

            case RS::ARRAY_TANGENT: {
                if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 4;
                }

            } break;
            case RS::ARRAY_COLOR: {
                if (p_format & RS::ARRAY_COMPRESS_COLOR) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 4;
                }
            } break;
            case RS::ARRAY_TEX_UV: {
                if (p_format & RS::ARRAY_COMPRESS_TEX_UV) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 2;
                }

            } break;

            case RS::ARRAY_TEX_UV2: {
                if (p_format & RS::ARRAY_COMPRESS_TEX_UV2) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 2;
                }

            } break;
            case RS::ARRAY_WEIGHTS: {
                if (p_format & RS::ARRAY_COMPRESS_WEIGHTS) {
                    elem_size = sizeof(uint16_t) * 4;
                } else {
                    elem_size = sizeof(float) * 4;
                }

            } break;
            case RS::ARRAY_BONES: {
                if (p_format & RS::ARRAY_FLAG_USE_16_BIT_BONES) {
                    elem_size = sizeof(uint16_t) * 4;
                } else {
                    elem_size = sizeof(uint32_t);
                }

            } break;
            case RS::ARRAY_INDEX: {
                if (p_index_len <= 0) {
                    ERR_PRINT("index_array_len==NO_INDEX_ARRAY");
                    break;
                }
                /* determine whether using 16 or 32 bits indices */
                if (p_vertex_len >= (1 << 16)) {
                    elem_size = 4;

                } else {
                    elem_size = 2;
                }
                offsets[i] = elem_size;
                continue;
            }
            default: {
                ERR_FAIL_V(SurfaceArrays());
            }
        }

        offsets[i] = total_elem_size;
        total_elem_size += elem_size;
    }

    SurfaceArrays ret;

    for (int i = 0; i < RS::ARRAY_MAX; i++) {
        if (!(p_format & (1 << i))) {
            continue;
        }

        switch (i) {
            case RS::ARRAY_VERTEX: {
                if (p_format & RS::ARRAY_FLAG_USE_2D_VERTICES) {
                    Vector<Vector2> arr_2d;
                    arr_2d.reserve(p_vertex_len);

                    if (p_format & RS::ARRAY_COMPRESS_VERTEX) {
                        for (uint32_t j = 0; j < p_vertex_len; j++) {
                            const uint16_t *v = (const uint16_t *)&p_vertex_data[j * total_elem_size + offsets[i]];
                            arr_2d.emplace_back(Math::halfptr_to_float(&v[0]), Math::halfptr_to_float(&v[1]));
                        }
                    } else {
                        for (uint32_t j = 0; j < p_vertex_len; j++) {
                            const float *v = (const float *)&p_vertex_data[j * total_elem_size + offsets[i]];
                            arr_2d.emplace_back(v[0], v[1]);
                        }
                    }

                    ret.set_positions(eastl::move(arr_2d));
                } else {
                    Vector<Vector3> arr_3d;
                    arr_3d.reserve(p_vertex_len);

                    if (p_format & RS::ARRAY_COMPRESS_VERTEX) {
                        for (uint32_t j = 0; j < p_vertex_len; j++) {
                            const uint16_t *v = (const uint16_t *)&p_vertex_data[j * total_elem_size + offsets[i]];
                            arr_3d.emplace_back(Math::halfptr_to_float(&v[0]), Math::halfptr_to_float(&v[1]),
                                    Math::halfptr_to_float(&v[2]));
                        }
                    } else {
                        for (uint32_t j = 0; j < p_vertex_len; j++) {
                            const float *v = (const float *)&p_vertex_data[j * total_elem_size + offsets[i]];
                            arr_3d.emplace_back(v[0], v[1], v[2]);
                        }
                    }

                    ret.set_positions(eastl::move(arr_3d));
                }

            } break;
            case RS::ARRAY_NORMAL: {
                Vector<Vector3> arr;
                arr.reserve(p_vertex_len);

                if (p_format & RS::ARRAY_COMPRESS_NORMAL) {
                    const float multiplier = 1.f / 127.f;

                    for (uint32_t j = 0; j < p_vertex_len; j++) {
                        const int8_t *v = (const int8_t *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        arr.emplace_back(float(v[0]) * multiplier, float(v[1]) * multiplier, float(v[2]) * multiplier);
                    }
                } else {
                    for (uint32_t j = 0; j < p_vertex_len; j++) {
                        const float *v = (const float *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        arr.emplace_back(v[0], v[1], v[2]);
                    }
                }

                ret.m_normals = eastl::move(arr);

            } break;

            case RS::ARRAY_TANGENT: {
                Vector<float> arr;
                arr.reserve(p_vertex_len * 4);
                if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const int8_t *v = (const int8_t *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        for (int k = 0; k < 4; k++) {
                            arr.emplace_back(float(v[k] / 127.0));
                        }
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const float *v = (const float *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        for (int k = 0; k < 4; k++) {
                            arr.emplace_back(v[k]);
                        }
                    }
                }

                ret.m_tangents = eastl::move(arr);

            } break;
            case RS::ARRAY_COLOR: {
                Vector<Color> arr;
                arr.reserve(p_vertex_len);

                if (p_format & RS::ARRAY_COMPRESS_COLOR) {
                    for (uint32_t j = 0; j < p_vertex_len; j++) {
                        const uint8_t *v = (const uint8_t *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        arr.emplace_back(
                                float(v[0] / 255.0f), float(v[1] / 255.0f), float(v[2] / 255.0f), float(v[3] / 255.0f));
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const float *v = (const float *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        arr.emplace_back(v[0], v[1], v[2], v[3]);
                    }
                }

                ret.m_colors = eastl::move(arr);
            } break;
            case RS::ARRAY_TEX_UV: {
                Vector<Vector2> arr;
                arr.reserve(p_vertex_len);

                if (p_format & RS::ARRAY_COMPRESS_TEX_UV) {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const uint16_t *v = (const uint16_t *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        arr.emplace_back(Math::halfptr_to_float(&v[0]), Math::halfptr_to_float(&v[1]));
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const float *v = (const float *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        arr.emplace_back(v[0], v[1]);
                    }
                }

                ret.m_uv_1 = eastl::move(arr);
            } break;

            case RS::ARRAY_TEX_UV2: {
                Vector<Vector2> arr;
                arr.reserve(p_vertex_len);

                if (p_format & RS::ARRAY_COMPRESS_TEX_UV2) {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const uint16_t *v = (const uint16_t *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        arr.emplace_back(Math::halfptr_to_float(&v[0]), Math::halfptr_to_float(&v[1]));
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const float *v = (const float *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        arr.emplace_back(v[0], v[1]);
                    }
                }

                ret.m_uv_2 = eastl::move(arr);

            } break;
            case RS::ARRAY_WEIGHTS: {
                Vector<float> arr;
                arr.reserve(p_vertex_len * 4);
                if (p_format & RS::ARRAY_COMPRESS_WEIGHTS) {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const uint16_t *v = (const uint16_t *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        for (int k = 0; k < 4; k++) {
                            arr.emplace_back(float(v[k]) / 65535.0f);
                        }
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const float *v = (const float *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        for (int k = 0; k < 4; k++) {
                            arr.emplace_back(v[k]);
                        }
                    }
                }

                ret.m_weights = eastl::move(arr);

            } break;
            case RS::ARRAY_BONES: {
                Vector<int> arr;
                arr.reserve(p_vertex_len * 4);
                if (p_format & RS::ARRAY_FLAG_USE_16_BIT_BONES) {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const uint16_t *v = (const uint16_t *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        arr.insert(arr.end(), v, v + 4);
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const uint8_t *v = (const uint8_t *)&p_vertex_data[j * total_elem_size + offsets[i]];
                        arr.insert(arr.end(), v, v + 4);
                    }
                }

                ret.m_bones = eastl::move(arr);

            } break;
            case RS::ARRAY_INDEX: {
                /* determine whether using 16 or 32 bits indices */

                Vector<int> arr;
                if (p_vertex_len < (1 << 16)) {
                    const uint16_t *v = (const uint16_t *)p_index_data.data();
                    arr.assign(v, v + p_index_len);
                } else {
                    const int *v = (const int *)p_index_data.data();
                    arr.assign(v, v + p_index_len);
                }
                ret.m_indices = eastl::move(arr);
            } break;
            default: {
                ERR_FAIL_V(ret);
            }
        }
    }

    return ret;
}

SurfaceArrays RenderingServer::mesh_surface_get_arrays(RID p_mesh, int p_surface) const {
    PoolVector<uint8_t> vertex_data = mesh_surface_get_array(p_mesh, p_surface);
    ERR_FAIL_COND_V(vertex_data.empty(), SurfaceArrays());
    int vertex_len = mesh_surface_get_array_len(p_mesh, p_surface);

    PoolVector<uint8_t> index_data = mesh_surface_get_index_array(p_mesh, p_surface);
    int index_len = mesh_surface_get_array_index_len(p_mesh, p_surface);

    uint32_t format = mesh_surface_get_format(p_mesh, p_surface);

    return _get_array_from_surface(format, vertex_data.toSpan(), vertex_len, index_data.toSpan(), index_len);
}
Array RenderingServer::_mesh_surface_get_arrays(RID p_mesh, int p_surface) const {
    return (Array)mesh_surface_get_arrays(p_mesh, p_surface);
}
void RenderingServer::_mesh_add_surface_from_arrays(RID p_mesh, RS::PrimitiveType p_primitive, const Array &p_arrays,
        const Array &p_blend_shapes, uint32_t p_compress_format) {
    ERR_FAIL_COND(p_arrays.size() != RS::ARRAY_MAX);
    Vector<SurfaceArrays> blend_shapes;
    blend_shapes.reserve(p_blend_shapes.size());
    for (int i = 0; i < p_blend_shapes.size(); ++i) {
        blend_shapes.emplace_back(SurfaceArrays::fromArray(p_blend_shapes[i].as<Array>()));
    }
    mesh_add_surface_from_arrays(
            p_mesh, p_primitive, SurfaceArrays::fromArray(p_arrays), eastl::move(blend_shapes), p_compress_format);
}
Array RenderingServer::_mesh_surface_get_blend_shape_arrays(RID p_mesh, int p_surface) const {
    Array res;
    Vector<SurfaceArrays> from = mesh_surface_get_blend_shape_arrays(p_mesh, p_surface);
    res.resize(from.size());
    int idx = 0;
    for (const SurfaceArrays &s : from) {
        res[idx++] = (Array)s;
    }
    return res;
}
Vector<SurfaceArrays> RenderingServer::mesh_surface_get_blend_shape_arrays(RID p_mesh, int p_surface) const {
    const Vector<Vector<uint8_t>> &blend_shape_data(mesh_surface_get_blend_shapes(p_mesh, p_surface));
    if (blend_shape_data.empty()) {
        return Vector<SurfaceArrays>();
    }

    int vertex_len = mesh_surface_get_array_len(p_mesh, p_surface);

    PoolVector<uint8_t> index_data = mesh_surface_get_index_array(p_mesh, p_surface);
    int index_len = mesh_surface_get_array_index_len(p_mesh, p_surface);

    uint32_t format = mesh_surface_get_format(p_mesh, p_surface);

    Vector<SurfaceArrays> blend_shape_array;
    blend_shape_array.reserve(blend_shape_data.size());
    for (int i = 0; i < blend_shape_data.size(); i++) {
        blend_shape_array.emplace_back(eastl::move(
                _get_array_from_surface(format, blend_shape_data[i], vertex_len, index_data.toSpan(), index_len)));
    }

    return blend_shape_array;
}

Array RenderingServer::_mesh_surface_get_skeleton_aabb_bind(RID p_mesh, int p_surface) const {
    const Vector<AABB> &vec = RenderingServer::get_singleton()->mesh_surface_get_skeleton_aabb(p_mesh, p_surface);
    Array arr;
    for (int i = 0; i < vec.size(); i++) {
        arr[i] = vec[i];
    }
    return arr;
}

void RenderingServer::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("force_sync"), &RenderingServer::sync);
    MethodBinder::bind_method(D_METHOD("force_draw", { "swap_buffers", "frame_step" }), &RenderingServer::draw,
            { DEFVAL(true), DEFVAL(0.0) });

    MethodBinder::bind_method(D_METHOD("texture_create"), &RenderingServer::texture_create);
    MethodBinder::bind_method(D_METHOD("texture_create_from_image", { "image", "flags" }),
            &RenderingServer::texture_create_from_image, { DEFVAL(RS::TEXTURE_FLAGS_DEFAULT) });
    MethodBinder::bind_method(
            D_METHOD("texture_allocate", { "texture", "width", "height", "depth_3d", "format", "type", "flags" }),
            &RenderingServer::texture_allocate, { DEFVAL(RS::TEXTURE_FLAGS_DEFAULT) });
    MethodBinder::bind_method(D_METHOD("texture_set_data", { "texture", "image", "layer" }),
            &RenderingServer::texture_set_data, { DEFVAL(0) });
    MethodBinder::bind_method(
            D_METHOD("texture_set_data_partial",
                    { "texture", "image", "src_x", "src_y", "src_w", "src_h", "dst_x", "dst_y", "dst_mip", "layer" }),
            &RenderingServer::texture_set_data_partial, { DEFVAL(0) });
    MethodBinder::bind_method(D_METHOD("texture_get_data", { "texture", "cube_side" }),
            &RenderingServer::texture_get_data, { DEFVAL(RS::CUBEMAP_LEFT) });
    MethodBinder::bind_method(
            D_METHOD("texture_set_flags", { "texture", "flags" }), &RenderingServer::texture_set_flags);
    MethodBinder::bind_method(D_METHOD("texture_get_flags", { "texture" }), &RenderingServer::texture_get_flags);
    MethodBinder::bind_method(D_METHOD("texture_get_format", { "texture" }), &RenderingServer::texture_get_format);
    MethodBinder::bind_method(D_METHOD("texture_get_type", { "texture" }), &RenderingServer::texture_get_type);
    MethodBinder::bind_method(D_METHOD("texture_get_texid", { "texture" }), &RenderingServer::texture_get_texid);
    MethodBinder::bind_method(D_METHOD("texture_get_width", { "texture" }), &RenderingServer::texture_get_width);
    MethodBinder::bind_method(D_METHOD("texture_get_height", { "texture" }), &RenderingServer::texture_get_height);
    MethodBinder::bind_method(D_METHOD("texture_get_depth", { "texture" }), &RenderingServer::texture_get_depth);
    MethodBinder::bind_method(D_METHOD("texture_set_size_override", { "texture", "width", "height", "depth" }),
            &RenderingServer::texture_set_size_override);
    MethodBinder::bind_method(D_METHOD("texture_set_path", { "texture", "path" }), &RenderingServer::texture_set_path);
    MethodBinder::bind_method(D_METHOD("texture_get_path", { "texture" }), &RenderingServer::texture_get_path);
    MethodBinder::bind_method(D_METHOD("texture_set_shrink_all_x2_on_set_data", { "shrink" }),
            &RenderingServer::texture_set_shrink_all_x2_on_set_data);
    MethodBinder::bind_method(D_METHOD("texture_bind", { "texture", "number" }), &RenderingServer::texture_bind);

    MethodBinder::bind_method(D_METHOD("texture_debug_usage"), &RenderingServer::_texture_debug_usage_bind);
    MethodBinder::bind_method(
            D_METHOD("textures_keep_original", { "enable" }), &RenderingServer::textures_keep_original);
#ifndef _3D_DISABLED
    MethodBinder::bind_method(D_METHOD("sky_create"), &RenderingServer::sky_create);
    MethodBinder::bind_method(
            D_METHOD("sky_set_texture", { "sky", "cube_map", "radiance_size" }), &RenderingServer::sky_set_texture);
#endif
    MethodBinder::bind_method(D_METHOD("shader_create"), &RenderingServer::shader_create);
    MethodBinder::bind_method(D_METHOD("shader_set_code", { "shader", "code" }), &RenderingServer::shader_set_code);
    MethodBinder::bind_method(D_METHOD("shader_get_code", { "shader" }), &RenderingServer::shader_get_code);
    MethodBinder::bind_method(
            D_METHOD("shader_get_param_list", { "shader" }), &RenderingServer::_shader_get_param_list_bind);
    MethodBinder::bind_method(D_METHOD("shader_set_default_texture_param", { "shader", "name", "texture" }),
            &RenderingServer::shader_set_default_texture_param);
    MethodBinder::bind_method(D_METHOD("shader_get_default_texture_param", { "shader", "name" }),
            &RenderingServer::shader_get_default_texture_param);

    MethodBinder::bind_method(D_METHOD("material_create"), &RenderingServer::material_create);
    MethodBinder::bind_method(
            D_METHOD("material_set_shader", { "shader_material", "shader" }), &RenderingServer::material_set_shader);
    MethodBinder::bind_method(
            D_METHOD("material_get_shader", { "shader_material" }), &RenderingServer::material_get_shader);
    MethodBinder::bind_method(
            D_METHOD("material_set_param", { "material", "parameter", "value" }), &RenderingServer::material_set_param);
    MethodBinder::bind_method(
            D_METHOD("material_get_param", { "material", "parameter" }), &RenderingServer::material_get_param);
    MethodBinder::bind_method(D_METHOD("material_get_param_default", { "material", "parameter" }),
            &RenderingServer::material_get_param_default);
    MethodBinder::bind_method(D_METHOD("material_set_render_priority", { "material", "priority" }),
            &RenderingServer::material_set_render_priority);
    MethodBinder::bind_method(
            D_METHOD("material_set_line_width", { "material", "width" }), &RenderingServer::material_set_line_width);
    MethodBinder::bind_method(D_METHOD("material_set_next_pass", { "material", "next_material" }),
            &RenderingServer::material_set_next_pass);

    MethodBinder::bind_method(D_METHOD("mesh_create"), &RenderingServer::mesh_create);
    MethodBinder::bind_method(
            D_METHOD("mesh_surface_get_format_offset", { "format", "vertex_len", "index_len", "array_index" }),
            &RenderingServer::mesh_surface_get_format_offset);
    MethodBinder::bind_method(D_METHOD("mesh_surface_get_format_stride", { "format", "vertex_len", "index_len" }),
            &RenderingServer::mesh_surface_get_format_stride);
    MethodBinder::bind_method(D_METHOD("mesh_add_surface_from_arrays",
                                      { "mesh", "primitive", "arrays", "blend_shapes", "compress_format" }),
            &RenderingServer::_mesh_add_surface_from_arrays, { DEFVAL(Array()), DEFVAL(RS::ARRAY_COMPRESS_DEFAULT) });
    MethodBinder::bind_method(
            D_METHOD("mesh_set_blend_shape_count", { "mesh", "amount" }), &RenderingServer::mesh_set_blend_shape_count);
    MethodBinder::bind_method(
            D_METHOD("mesh_get_blend_shape_count", { "mesh" }), &RenderingServer::mesh_get_blend_shape_count);
    MethodBinder::bind_method(
            D_METHOD("mesh_set_blend_shape_mode", { "mesh", "mode" }), &RenderingServer::mesh_set_blend_shape_mode);
    MethodBinder::bind_method(
            D_METHOD("mesh_get_blend_shape_mode", { "mesh" }), &RenderingServer::mesh_get_blend_shape_mode);
    MethodBinder::bind_method(D_METHOD("mesh_surface_update_region", { "mesh", "surface", "offset", "data" }),
            &RenderingServer::mesh_surface_update_region);
    MethodBinder::bind_method(D_METHOD("mesh_surface_set_material", { "mesh", "surface", "material" }),
            &RenderingServer::mesh_surface_set_material);
    MethodBinder::bind_method(
            D_METHOD("mesh_surface_get_material", { "mesh", "surface" }), &RenderingServer::mesh_surface_get_material);
    MethodBinder::bind_method(D_METHOD("mesh_surface_get_array_len", { "mesh", "surface" }),
            &RenderingServer::mesh_surface_get_array_len);
    MethodBinder::bind_method(D_METHOD("mesh_surface_get_array_index_len", { "mesh", "surface" }),
            &RenderingServer::mesh_surface_get_array_index_len);
    MethodBinder::bind_method(
            D_METHOD("mesh_surface_get_array", { "mesh", "surface" }), &RenderingServer::mesh_surface_get_array);
    MethodBinder::bind_method(D_METHOD("mesh_surface_get_index_array", { "mesh", "surface" }),
            &RenderingServer::mesh_surface_get_index_array);
    MethodBinder::bind_method(
            D_METHOD("mesh_surface_get_arrays", { "mesh", "surface" }), &RenderingServer::_mesh_surface_get_arrays);
    MethodBinder::bind_method(D_METHOD("mesh_surface_get_blend_shape_arrays", { "mesh", "surface" }),
            &RenderingServer::_mesh_surface_get_blend_shape_arrays);
    MethodBinder::bind_method(
            D_METHOD("mesh_surface_get_format", { "mesh", "surface" }), &RenderingServer::mesh_surface_get_format);
    MethodBinder::bind_method(D_METHOD("mesh_surface_get_primitive_type", { "mesh", "surface" }),
            &RenderingServer::mesh_surface_get_primitive_type);
    MethodBinder::bind_method(
            D_METHOD("mesh_surface_get_aabb", { "mesh", "surface" }), &RenderingServer::mesh_surface_get_aabb);
    MethodBinder::bind_method(D_METHOD("mesh_surface_get_skeleton_aabb", { "mesh", "surface" }),
            &RenderingServer::_mesh_surface_get_skeleton_aabb_bind);
    MethodBinder::bind_method(
            D_METHOD("mesh_remove_surface", { "mesh", "index" }), &RenderingServer::mesh_remove_surface);
    MethodBinder::bind_method(D_METHOD("mesh_get_surface_count", { "mesh" }), &RenderingServer::mesh_get_surface_count);
    MethodBinder::bind_method(
            D_METHOD("mesh_set_custom_aabb", { "mesh", "aabb" }), &RenderingServer::mesh_set_custom_aabb);
    MethodBinder::bind_method(D_METHOD("mesh_get_custom_aabb", { "mesh" }), &RenderingServer::mesh_get_custom_aabb);
    MethodBinder::bind_method(D_METHOD("mesh_clear", { "mesh" }), &RenderingServer::mesh_clear);

    MethodBinder::bind_method(D_METHOD("multimesh_create"), &RenderingServer::multimesh_create);
    MethodBinder::bind_method(D_METHOD("multimesh_allocate", { "multimesh", "instances", "transform_format",
                                                                     "color_format", "custom_data_format" }),
            &RenderingServer::multimesh_allocate, { DEFVAL(RS::MULTIMESH_CUSTOM_DATA_NONE) });
    MethodBinder::bind_method(
            D_METHOD("multimesh_get_instance_count", { "multimesh" }), &RenderingServer::multimesh_get_instance_count);
    MethodBinder::bind_method(
            D_METHOD("multimesh_set_mesh", { "multimesh", "mesh" }), &RenderingServer::multimesh_set_mesh);
    MethodBinder::bind_method(D_METHOD("multimesh_instance_set_transform", { "multimesh", "index", "transform" }),
            &RenderingServer::multimesh_instance_set_transform);
    MethodBinder::bind_method(D_METHOD("multimesh_instance_set_transform_2d", { "multimesh", "index", "transform" }),
            &RenderingServer::multimesh_instance_set_transform_2d);
    MethodBinder::bind_method(D_METHOD("multimesh_instance_set_color", { "multimesh", "index", "color" }),
            &RenderingServer::multimesh_instance_set_color);
    MethodBinder::bind_method(D_METHOD("multimesh_instance_set_custom_data", { "multimesh", "index", "custom_data" }),
            &RenderingServer::multimesh_instance_set_custom_data);
    MethodBinder::bind_method(D_METHOD("multimesh_get_mesh", { "multimesh" }), &RenderingServer::multimesh_get_mesh);
    MethodBinder::bind_method(D_METHOD("multimesh_get_aabb", { "multimesh" }), &RenderingServer::multimesh_get_aabb);
    MethodBinder::bind_method(D_METHOD("multimesh_instance_get_transform", { "multimesh", "index" }),
            &RenderingServer::multimesh_instance_get_transform);
    MethodBinder::bind_method(D_METHOD("multimesh_instance_get_transform_2d", { "multimesh", "index" }),
            &RenderingServer::multimesh_instance_get_transform_2d);
    MethodBinder::bind_method(D_METHOD("multimesh_instance_get_color", { "multimesh", "index" }),
            &RenderingServer::multimesh_instance_get_color);
    MethodBinder::bind_method(D_METHOD("multimesh_instance_get_custom_data", { "multimesh", "index" }),
            &RenderingServer::multimesh_instance_get_custom_data);
    MethodBinder::bind_method(D_METHOD("multimesh_set_visible_instances", { "multimesh", "visible" }),
            &RenderingServer::multimesh_set_visible_instances);
    MethodBinder::bind_method(D_METHOD("multimesh_get_visible_instances", { "multimesh" }),
            &RenderingServer::multimesh_get_visible_instances);
    MethodBinder::bind_method(D_METHOD("multimesh_set_as_bulk_array", { "multimesh", "array" }),
            &RenderingServer::multimesh_set_as_bulk_array);
#ifndef _3D_DISABLED
    MethodBinder::bind_method(D_METHOD("immediate_create"), &RenderingServer::immediate_create);
    MethodBinder::bind_method(D_METHOD("immediate_begin", { "immediate", "primitive", "texture" }),
            &RenderingServer::immediate_begin, { DEFVAL(RID()) });
    MethodBinder::bind_method(
            D_METHOD("immediate_vertex", { "immediate", "vertex" }), &RenderingServer::immediate_vertex);
    MethodBinder::bind_method(
            D_METHOD("immediate_vertex_2d", { "immediate", "vertex" }), &RenderingServer::immediate_vertex_2d);
    MethodBinder::bind_method(
            D_METHOD("immediate_normal", { "immediate", "normal" }), &RenderingServer::immediate_normal);
    MethodBinder::bind_method(
            D_METHOD("immediate_tangent", { "immediate", "tangent" }), &RenderingServer::immediate_tangent);
    MethodBinder::bind_method(D_METHOD("immediate_color", { "immediate", "color" }), &RenderingServer::immediate_color);
    MethodBinder::bind_method(D_METHOD("immediate_uv", { "immediate", "tex_uv" }), &RenderingServer::immediate_uv);
    MethodBinder::bind_method(D_METHOD("immediate_uv2", { "immediate", "tex_uv" }), &RenderingServer::immediate_uv2);
    MethodBinder::bind_method(D_METHOD("immediate_end", { "immediate" }), &RenderingServer::immediate_end);
    MethodBinder::bind_method(D_METHOD("immediate_clear", { "immediate" }), &RenderingServer::immediate_clear);
    MethodBinder::bind_method(
            D_METHOD("immediate_set_material", { "immediate", "material" }), &RenderingServer::immediate_set_material);
    MethodBinder::bind_method(
            D_METHOD("immediate_get_material", { "immediate" }), &RenderingServer::immediate_get_material);
#endif

    MethodBinder::bind_method(D_METHOD("skeleton_create"), &RenderingServer::skeleton_create);
    MethodBinder::bind_method(D_METHOD("skeleton_allocate", { "skeleton", "bones", "is_2d_skeleton" }),
            &RenderingServer::skeleton_allocate, { DEFVAL(false) });
    MethodBinder::bind_method(
            D_METHOD("skeleton_get_bone_count", { "skeleton" }), &RenderingServer::skeleton_get_bone_count);
    MethodBinder::bind_method(D_METHOD("skeleton_bone_set_transform", { "skeleton", "bone", "transform" }),
            &RenderingServer::skeleton_bone_set_transform);
    MethodBinder::bind_method(D_METHOD("skeleton_bone_get_transform", { "skeleton", "bone" }),
            &RenderingServer::skeleton_bone_get_transform);
    MethodBinder::bind_method(D_METHOD("skeleton_bone_set_transform_2d", { "skeleton", "bone", "transform" }),
            &RenderingServer::skeleton_bone_set_transform_2d);
    MethodBinder::bind_method(D_METHOD("skeleton_bone_get_transform_2d", { "skeleton", "bone" }),
            &RenderingServer::skeleton_bone_get_transform_2d);

#ifndef _3D_DISABLED
    MethodBinder::bind_method(D_METHOD("directional_light_create"), &RenderingServer::directional_light_create);
    MethodBinder::bind_method(D_METHOD("omni_light_create"), &RenderingServer::omni_light_create);
    MethodBinder::bind_method(D_METHOD("spot_light_create"), &RenderingServer::spot_light_create);

    MethodBinder::bind_method(D_METHOD("light_set_color", { "light", "color" }), &RenderingServer::light_set_color);
    MethodBinder::bind_method(
            D_METHOD("light_set_param", { "light", "param", "value" }), &RenderingServer::light_set_param);
    MethodBinder::bind_method(D_METHOD("light_set_shadow", { "light", "enabled" }), &RenderingServer::light_set_shadow);
    MethodBinder::bind_method(
            D_METHOD("light_set_shadow_color", { "light", "color" }), &RenderingServer::light_set_shadow_color);
    MethodBinder::bind_method(
            D_METHOD("light_set_projector", { "light", "texture" }), &RenderingServer::light_set_projector);
    MethodBinder::bind_method(
            D_METHOD("light_set_negative", { "light", "enable" }), &RenderingServer::light_set_negative);
    MethodBinder::bind_method(
            D_METHOD("light_set_cull_mask", { "light", "mask" }), &RenderingServer::light_set_cull_mask);
    MethodBinder::bind_method(D_METHOD("light_set_reverse_cull_face_mode", { "light", "enabled" }),
            &RenderingServer::light_set_reverse_cull_face_mode);
    MethodBinder::bind_method(D_METHOD("light_set_use_gi", { "light", "enabled" }), &RenderingServer::light_set_use_gi);
    MethodBinder::bind_method(D_METHOD("light_set_bake_mode", {"light", "bake_mode"}), &RenderingServer::light_set_bake_mode);

    MethodBinder::bind_method(
            D_METHOD("light_omni_set_shadow_mode", { "light", "mode" }), &RenderingServer::light_omni_set_shadow_mode);
    MethodBinder::bind_method(D_METHOD("light_omni_set_shadow_detail", { "light", "detail" }),
            &RenderingServer::light_omni_set_shadow_detail);

    MethodBinder::bind_method(D_METHOD("light_directional_set_shadow_mode", { "light", "mode" }),
            &RenderingServer::light_directional_set_shadow_mode);
    MethodBinder::bind_method(D_METHOD("light_directional_set_blend_splits", { "light", "enable" }),
            &RenderingServer::light_directional_set_blend_splits);
    MethodBinder::bind_method(D_METHOD("light_directional_set_shadow_depth_range_mode", { "light", "range_mode" }),
            &RenderingServer::light_directional_set_shadow_depth_range_mode);

    MethodBinder::bind_method(D_METHOD("reflection_probe_create"), &RenderingServer::reflection_probe_create);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_update_mode", { "probe", "mode" }),
            &RenderingServer::reflection_probe_set_update_mode);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_intensity", { "probe", "intensity" }),
            &RenderingServer::reflection_probe_set_intensity);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_interior_ambient", { "probe", "color" }),
            &RenderingServer::reflection_probe_set_interior_ambient);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_interior_ambient_energy", { "probe", "energy" }),
            &RenderingServer::reflection_probe_set_interior_ambient_energy);
    MethodBinder::bind_method(
            D_METHOD("reflection_probe_set_interior_ambient_probe_contribution", { "probe", "contrib" }),
            &RenderingServer::reflection_probe_set_interior_ambient_probe_contribution);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_max_distance", { "probe", "distance" }),
            &RenderingServer::reflection_probe_set_max_distance);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_extents", { "probe", "extents" }),
            &RenderingServer::reflection_probe_set_extents);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_origin_offset", { "probe", "offset" }),
            &RenderingServer::reflection_probe_set_origin_offset);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_as_interior", { "probe", "enable" }),
            &RenderingServer::reflection_probe_set_as_interior);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_enable_box_projection", { "probe", "enable" }),
            &RenderingServer::reflection_probe_set_enable_box_projection);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_enable_shadows", { "probe", "enable" }),
            &RenderingServer::reflection_probe_set_enable_shadows);
    MethodBinder::bind_method(D_METHOD("reflection_probe_set_cull_mask", { "probe", "layers" }),
            &RenderingServer::reflection_probe_set_cull_mask);

    MethodBinder::bind_method(D_METHOD("gi_probe_create"), &RenderingServer::gi_probe_create);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_set_bounds", { "probe", "bounds" }), &RenderingServer::gi_probe_set_bounds);
    MethodBinder::bind_method(D_METHOD("gi_probe_get_bounds", { "probe" }), &RenderingServer::gi_probe_get_bounds);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_set_cell_size", { "probe", "range" }), &RenderingServer::gi_probe_set_cell_size);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_get_cell_size", { "probe" }), &RenderingServer::gi_probe_get_cell_size);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_set_to_cell_xform", { "probe", "xform" }), &RenderingServer::gi_probe_set_to_cell_xform);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_get_to_cell_xform", { "probe" }), &RenderingServer::gi_probe_get_to_cell_xform);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_set_dynamic_data", { "probe", "data" }), &RenderingServer::gi_probe_set_dynamic_data);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_get_dynamic_data", { "probe" }), &RenderingServer::gi_probe_get_dynamic_data);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_set_dynamic_range", { "probe", "range" }), &RenderingServer::gi_probe_set_dynamic_range);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_get_dynamic_range", { "probe" }), &RenderingServer::gi_probe_get_dynamic_range);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_set_energy", { "probe", "energy" }), &RenderingServer::gi_probe_set_energy);
    MethodBinder::bind_method(D_METHOD("gi_probe_get_energy", { "probe" }), &RenderingServer::gi_probe_get_energy);
    MethodBinder::bind_method(D_METHOD("gi_probe_set_bias", { "probe", "bias" }), &RenderingServer::gi_probe_set_bias);
    MethodBinder::bind_method(D_METHOD("gi_probe_get_bias", { "probe" }), &RenderingServer::gi_probe_get_bias);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_set_normal_bias", { "probe", "bias" }), &RenderingServer::gi_probe_set_normal_bias);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_get_normal_bias", { "probe" }), &RenderingServer::gi_probe_get_normal_bias);
    MethodBinder::bind_method(D_METHOD("gi_probe_set_propagation", { "probe", "propagation" }),
            &RenderingServer::gi_probe_set_propagation);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_get_propagation", { "probe" }), &RenderingServer::gi_probe_get_propagation);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_set_interior", { "probe", "enable" }), &RenderingServer::gi_probe_set_interior);
    MethodBinder::bind_method(D_METHOD("gi_probe_is_interior", { "probe" }), &RenderingServer::gi_probe_is_interior);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_set_compress", { "probe", "enable" }), &RenderingServer::gi_probe_set_compress);
    MethodBinder::bind_method(
            D_METHOD("gi_probe_is_compressed", { "probe" }), &RenderingServer::gi_probe_is_compressed);

    MethodBinder::bind_method(D_METHOD("lightmap_capture_create"), &RenderingServer::lightmap_capture_create);
    MethodBinder::bind_method(D_METHOD("lightmap_capture_set_bounds", { "capture", "bounds" }),
            &RenderingServer::lightmap_capture_set_bounds);
    MethodBinder::bind_method(
            D_METHOD("lightmap_capture_get_bounds", { "capture" }), &RenderingServer::lightmap_capture_get_bounds);
    MethodBinder::bind_method(D_METHOD("lightmap_capture_set_octree", { "capture", "octree" }),
            &RenderingServer::lightmap_capture_set_octree);
    MethodBinder::bind_method(D_METHOD("lightmap_capture_set_octree_cell_transform", { "capture", "xform" }),
            &RenderingServer::lightmap_capture_set_octree_cell_transform);
    MethodBinder::bind_method(D_METHOD("lightmap_capture_get_octree_cell_transform", { "capture" }),
            &RenderingServer::lightmap_capture_get_octree_cell_transform);
    MethodBinder::bind_method(D_METHOD("lightmap_capture_set_octree_cell_subdiv", { "capture", "subdiv" }),
            &RenderingServer::lightmap_capture_set_octree_cell_subdiv);
    MethodBinder::bind_method(D_METHOD("lightmap_capture_get_octree_cell_subdiv", { "capture" }),
            &RenderingServer::lightmap_capture_get_octree_cell_subdiv);
    MethodBinder::bind_method(
            D_METHOD("lightmap_capture_get_octree", { "capture" }), &RenderingServer::lightmap_capture_get_octree);
    MethodBinder::bind_method(D_METHOD("lightmap_capture_set_energy", { "capture", "energy" }),
            &RenderingServer::lightmap_capture_set_energy);
    MethodBinder::bind_method(
            D_METHOD("lightmap_capture_get_energy", { "capture" }), &RenderingServer::lightmap_capture_get_energy);
#endif
    MethodBinder::bind_method(D_METHOD("particles_create"), &RenderingServer::particles_create);
    MethodBinder::bind_method(
            D_METHOD("particles_set_emitting", { "particles", "emitting" }), &RenderingServer::particles_set_emitting);
    MethodBinder::bind_method(
            D_METHOD("particles_get_emitting", { "particles" }), &RenderingServer::particles_get_emitting);
    MethodBinder::bind_method(
            D_METHOD("particles_set_amount", { "particles", "amount" }), &RenderingServer::particles_set_amount);
    MethodBinder::bind_method(
            D_METHOD("particles_set_lifetime", { "particles", "lifetime" }), &RenderingServer::particles_set_lifetime);
    MethodBinder::bind_method(
            D_METHOD("particles_set_one_shot", { "particles", "one_shot" }), &RenderingServer::particles_set_one_shot);
    MethodBinder::bind_method(D_METHOD("particles_set_pre_process_time", { "particles", "time" }),
            &RenderingServer::particles_set_pre_process_time);
    MethodBinder::bind_method(D_METHOD("particles_set_explosiveness_ratio", { "particles", "ratio" }),
            &RenderingServer::particles_set_explosiveness_ratio);
    MethodBinder::bind_method(D_METHOD("particles_set_randomness_ratio", { "particles", "ratio" }),
            &RenderingServer::particles_set_randomness_ratio);
    MethodBinder::bind_method(D_METHOD("particles_set_custom_aabb", { "particles", "aabb" }),
            &RenderingServer::particles_set_custom_aabb);
    MethodBinder::bind_method(D_METHOD("particles_set_speed_scale", { "particles", "scale" }),
            &RenderingServer::particles_set_speed_scale);
    MethodBinder::bind_method(D_METHOD("particles_set_use_local_coordinates", { "particles", "enable" }),
            &RenderingServer::particles_set_use_local_coordinates);
    MethodBinder::bind_method(D_METHOD("particles_set_process_material", { "particles", "material" }),
            &RenderingServer::particles_set_process_material);
    MethodBinder::bind_method(
            D_METHOD("particles_set_fixed_fps", { "particles", "fps" }), &RenderingServer::particles_set_fixed_fps);
    MethodBinder::bind_method(D_METHOD("particles_set_fractional_delta", { "particles", "enable" }),
            &RenderingServer::particles_set_fractional_delta);
    MethodBinder::bind_method(
            D_METHOD("particles_is_inactive", { "particles" }), &RenderingServer::particles_is_inactive);
    MethodBinder::bind_method(
            D_METHOD("particles_request_process", { "particles" }), &RenderingServer::particles_request_process);
    MethodBinder::bind_method(D_METHOD("particles_restart", { "particles" }), &RenderingServer::particles_restart);
    MethodBinder::bind_method(
            D_METHOD("particles_set_draw_order", { "particles", "order" }), &RenderingServer::particles_set_draw_order);
    MethodBinder::bind_method(D_METHOD("particles_set_draw_passes", { "particles", "count" }),
            &RenderingServer::particles_set_draw_passes);
    MethodBinder::bind_method(D_METHOD("particles_set_draw_pass_mesh", { "particles", "pass", "mesh" }),
            &RenderingServer::particles_set_draw_pass_mesh);
    MethodBinder::bind_method(
            D_METHOD("particles_get_current_aabb", { "particles" }), &RenderingServer::particles_get_current_aabb);
    MethodBinder::bind_method(D_METHOD("particles_set_emission_transform", { "particles", "transform" }),
            &RenderingServer::particles_set_emission_transform);

    MethodBinder::bind_method(D_METHOD("camera_create"), &RenderingServer::camera_create);
    MethodBinder::bind_method(D_METHOD("camera_set_perspective", { "camera", "fovy_degrees", "z_near", "z_far" }),
            &RenderingServer::camera_set_perspective);
    MethodBinder::bind_method(D_METHOD("camera_set_orthogonal", { "camera", "size", "z_near", "z_far" }),
            &RenderingServer::camera_set_orthogonal);
    MethodBinder::bind_method(D_METHOD("camera_set_frustum", { "camera", "size", "offset", "z_near", "z_far" }),
            &RenderingServer::camera_set_frustum);
    MethodBinder::bind_method(
            D_METHOD("camera_set_transform", { "camera", "transform" }), &RenderingServer::camera_set_transform);
    MethodBinder::bind_method(
            D_METHOD("camera_set_cull_mask", { "camera", "layers" }), &RenderingServer::camera_set_cull_mask);
    MethodBinder::bind_method(
            D_METHOD("camera_set_environment", { "camera", "env" }), &RenderingServer::camera_set_environment);
    MethodBinder::bind_method(D_METHOD("camera_set_use_vertical_aspect", { "camera", "enable" }),
            &RenderingServer::camera_set_use_vertical_aspect);

    MethodBinder::bind_method(D_METHOD("viewport_create"), &RenderingServer::viewport_create);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_use_arvr", { "viewport", "use_arvr" }), &RenderingServer::viewport_set_use_arvr);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_size", { "viewport", "width", "height" }), &RenderingServer::viewport_set_size);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_active", { "viewport", "active" }), &RenderingServer::viewport_set_active);
    MethodBinder::bind_method(D_METHOD("viewport_set_parent_viewport", { "viewport", "parent_viewport" }),
            &RenderingServer::viewport_set_parent_viewport);
    MethodBinder::bind_method(D_METHOD("viewport_attach_to_screen", { "viewport", "rect", "screen" }),
            &RenderingServer::viewport_attach_to_screen, { DEFVAL(Rect2()), DEFVAL(0) });
    MethodBinder::bind_method(D_METHOD("viewport_set_render_direct_to_screen", { "viewport", "enabled" }),
            &RenderingServer::viewport_set_render_direct_to_screen);
    MethodBinder::bind_method(D_METHOD("viewport_detach", { "viewport" }), &RenderingServer::viewport_detach);
    MethodBinder::bind_method(D_METHOD("viewport_set_update_mode", { "viewport", "update_mode" }),
            &RenderingServer::viewport_set_update_mode);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_vflip", { "viewport", "enabled" }), &RenderingServer::viewport_set_vflip);
    MethodBinder::bind_method(D_METHOD("viewport_set_clear_mode", { "viewport", "clear_mode" }),
            &RenderingServer::viewport_set_clear_mode);
    MethodBinder::bind_method(D_METHOD("viewport_get_texture", { "viewport" }), &RenderingServer::viewport_get_texture);
    MethodBinder::bind_method(D_METHOD("viewport_set_hide_scenario", { "viewport", "hidden" }),
            &RenderingServer::viewport_set_hide_scenario);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_hide_canvas", { "viewport", "hidden" }), &RenderingServer::viewport_set_hide_canvas);
    MethodBinder::bind_method(D_METHOD("viewport_set_disable_environment", { "viewport", "disabled" }),
            &RenderingServer::viewport_set_disable_environment);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_disable_3d", { "viewport", "disabled" }), &RenderingServer::viewport_set_disable_3d);
    MethodBinder::bind_method(
            D_METHOD("viewport_attach_camera", { "viewport", "camera" }), &RenderingServer::viewport_attach_camera);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_scenario", { "viewport", "scenario" }), &RenderingServer::viewport_set_scenario);
    MethodBinder::bind_method(
            D_METHOD("viewport_attach_canvas", { "viewport", "canvas" }), &RenderingServer::viewport_attach_canvas);
    MethodBinder::bind_method(
            D_METHOD("viewport_remove_canvas", { "viewport", "canvas" }), &RenderingServer::viewport_remove_canvas);
    MethodBinder::bind_method(D_METHOD("viewport_set_canvas_transform", { "viewport", "canvas", "offset" }),
            &RenderingServer::viewport_set_canvas_transform);
    MethodBinder::bind_method(D_METHOD("viewport_set_transparent_background", { "viewport", "enabled" }),
            &RenderingServer::viewport_set_transparent_background);
    MethodBinder::bind_method(D_METHOD("viewport_set_global_canvas_transform", { "viewport", "transform" }),
            &RenderingServer::viewport_set_global_canvas_transform);
    MethodBinder::bind_method(D_METHOD("viewport_set_canvas_stacking", { "viewport", "canvas", "layer", "sublayer" }),
            &RenderingServer::viewport_set_canvas_stacking);
    MethodBinder::bind_method(D_METHOD("viewport_set_shadow_atlas_size", { "viewport", "size" }),
            &RenderingServer::viewport_set_shadow_atlas_size);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_shadow_atlas_quadrant_subdivision", { "viewport", "quadrant", "subdivision" }),
            &RenderingServer::viewport_set_shadow_atlas_quadrant_subdivision);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_msaa", { "viewport", "msaa" }), &RenderingServer::viewport_set_msaa);
    MethodBinder::bind_method(D_METHOD("viewport_set_use_fxaa", {"viewport", "fxaa"}), &RenderingServer::viewport_set_use_fxaa);
    MethodBinder::bind_method(D_METHOD("viewport_set_use_debanding", {"viewport", "debanding"}), &RenderingServer::viewport_set_use_debanding);

    MethodBinder::bind_method(
            D_METHOD("viewport_set_hdr", { "viewport", "enabled" }), &RenderingServer::viewport_set_hdr);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_usage", { "viewport", "usage" }), &RenderingServer::viewport_set_usage);
    MethodBinder::bind_method(
            D_METHOD("viewport_get_render_info", { "viewport", "info" }), &RenderingServer::viewport_get_render_info);
    MethodBinder::bind_method(
            D_METHOD("viewport_set_debug_draw", { "viewport", "draw" }), &RenderingServer::viewport_set_debug_draw);

    MethodBinder::bind_method(D_METHOD("environment_create"), &RenderingServer::environment_create);
    MethodBinder::bind_method(
            D_METHOD("environment_set_background", { "env", "bg" }), &RenderingServer::environment_set_background);
    MethodBinder::bind_method(D_METHOD("environment_set_sky", { "env", "sky" }), &RenderingServer::environment_set_sky);
    MethodBinder::bind_method(D_METHOD("environment_set_sky_custom_fov", { "env", "scale" }),
            &RenderingServer::environment_set_sky_custom_fov);
    MethodBinder::bind_method(D_METHOD("environment_set_sky_orientation", { "env", "orientation" }),
            &RenderingServer::environment_set_sky_orientation);
    MethodBinder::bind_method(
            D_METHOD("environment_set_bg_color", { "env", "color" }), &RenderingServer::environment_set_bg_color);
    MethodBinder::bind_method(
            D_METHOD("environment_set_bg_energy", { "env", "energy" }), &RenderingServer::environment_set_bg_energy);
    MethodBinder::bind_method(D_METHOD("environment_set_canvas_max_layer", { "env", "max_layer" }),
            &RenderingServer::environment_set_canvas_max_layer);
    MethodBinder::bind_method(
            D_METHOD("environment_set_ambient_light", { "env", "color", "energy", "sky_contibution" }),
            &RenderingServer::environment_set_ambient_light, { DEFVAL(1.0), DEFVAL(0.0) });
    MethodBinder::bind_method(D_METHOD("environment_set_dof_blur_near",
                                      { "env", "enable", "distance", "transition", "far_amount", "quality" }),
            &RenderingServer::environment_set_dof_blur_near);
    MethodBinder::bind_method(D_METHOD("environment_set_dof_blur_far",
                                      { "env", "enable", "distance", "transition", "far_amount", "quality" }),
            &RenderingServer::environment_set_dof_blur_far);
    MethodBinder::bind_method(
            D_METHOD("environment_set_glow",
                    { "env", "enable", "level_flags", "intensity", "strength", "bloom_threshold", "blend_mode",
                            "hdr_bleed_threshold", "hdr_bleed_scale", "hdr_luminance_cap", "bicubic_upscale" }),
            &RenderingServer::environment_set_glow);
    MethodBinder::bind_method(D_METHOD("environment_set_tonemap",
                                      { "env", "tone_mapper", "exposure", "white", "auto_exposure", "min_luminance",
                                              "max_luminance", "auto_exp_speed", "auto_exp_grey" }),
            &RenderingServer::environment_set_tonemap);
    MethodBinder::bind_method(
            D_METHOD("environment_set_adjustment", { "env", "enable", "brightness", "contrast", "saturation", "ramp" }),
            &RenderingServer::environment_set_adjustment);
    MethodBinder::bind_method(D_METHOD("environment_set_ssr", { "env", "enable", "max_steps", "fade_in", "fade_out",
                                                                      "depth_tolerance", "roughness" }),
            &RenderingServer::environment_set_ssr);
    MethodBinder::bind_method(
            D_METHOD("environment_set_ssao",
                    { "env", "enable", "radius", "intensity", "radius2", "intensity2", "bias", "light_affect",
                            "ao_channel_affect", "color", "quality", "blur", "bilateral_sharpness" }),
            &RenderingServer::environment_set_ssao);
    MethodBinder::bind_method(D_METHOD("environment_set_fog", { "env", "enable", "color", "sun_color", "sun_amount" }),
            &RenderingServer::environment_set_fog);

    MethodBinder::bind_method(
            D_METHOD("environment_set_fog_depth",
                    { "env", "enable", "depth_begin", "depth_end", "depth_curve", "transmit", "transmit_curve" }),
            &RenderingServer::environment_set_fog_depth);

    MethodBinder::bind_method(
            D_METHOD("environment_set_fog_height", { "env", "enable", "min_height", "max_height", "height_curve" }),
            &RenderingServer::environment_set_fog_height);

    MethodBinder::bind_method(D_METHOD("scenario_create"), &RenderingServer::scenario_create);
    MethodBinder::bind_method(
            D_METHOD("scenario_set_debug", { "scenario", "debug_mode" }), &RenderingServer::scenario_set_debug);
    MethodBinder::bind_method(D_METHOD("scenario_set_environment", { "scenario", "environment" }),
            &RenderingServer::scenario_set_environment);
    MethodBinder::bind_method(D_METHOD("scenario_set_reflection_atlas_size", { "scenario", "size", "subdiv" }),
            &RenderingServer::scenario_set_reflection_atlas_size);
    MethodBinder::bind_method(D_METHOD("scenario_set_fallback_environment", { "scenario", "environment" }),
            &RenderingServer::scenario_set_fallback_environment);

#ifndef _3D_DISABLED

    MethodBinder::bind_method(D_METHOD("instance_create2", { "base", "scenario" }), &RenderingServer::instance_create2);
    MethodBinder::bind_method(D_METHOD("instance_create"), &RenderingServer::instance_create);
    MethodBinder::bind_method(
            D_METHOD("instance_set_base", { "instance", "base" }), &RenderingServer::instance_set_base);
    MethodBinder::bind_method(
            D_METHOD("instance_set_scenario", { "instance", "scenario" }), &RenderingServer::instance_set_scenario);
    MethodBinder::bind_method(
            D_METHOD("instance_set_layer_mask", { "instance", "mask" }), &RenderingServer::instance_set_layer_mask);
    MethodBinder::bind_method(
            D_METHOD("instance_set_transform", { "instance", "transform" }), &RenderingServer::instance_set_transform);
    MethodBinder::bind_method(D_METHOD("instance_attach_object_instance_id", { "instance", "id" }),
            &RenderingServer::instance_attach_object_instance_id);
    MethodBinder::bind_method(D_METHOD("instance_set_blend_shape_weight", { "instance", "shape", "weight" }),
            &RenderingServer::instance_set_blend_shape_weight);
    MethodBinder::bind_method(D_METHOD("instance_set_surface_material", { "instance", "surface", "material" }),
            &RenderingServer::instance_set_surface_material);
    MethodBinder::bind_method(
            D_METHOD("instance_set_visible", { "instance", "visible" }), &RenderingServer::instance_set_visible);
    MethodBinder::bind_method(D_METHOD("instance_set_use_lightmap", { "instance", "lightmap_instance", "lightmap" }),
            &RenderingServer::instance_set_use_lightmap);
    MethodBinder::bind_method(
            D_METHOD("instance_set_custom_aabb", { "instance", "aabb" }), &RenderingServer::instance_set_custom_aabb);
    MethodBinder::bind_method(D_METHOD("instance_attach_skeleton", { "instance", "skeleton" }),
            &RenderingServer::instance_attach_skeleton);
    MethodBinder::bind_method(D_METHOD("instance_set_extra_visibility_margin", { "instance", "margin" }),
            &RenderingServer::instance_set_extra_visibility_margin);
    MethodBinder::bind_method(D_METHOD("instance_geometry_set_flag", { "instance", "flag", "enabled" }),
            &RenderingServer::instance_geometry_set_flag);
    MethodBinder::bind_method(
            D_METHOD("instance_geometry_set_cast_shadows_setting", { "instance", "shadow_casting_setting" }),
            &RenderingServer::instance_geometry_set_cast_shadows_setting);
    MethodBinder::bind_method(D_METHOD("instance_geometry_set_material_override", { "instance", "material" }),
            &RenderingServer::instance_geometry_set_material_override);
    MethodBinder::bind_method(
            D_METHOD("instance_geometry_set_draw_range", { "instance", "min", "max", "min_margin", "max_margin" }),
            &RenderingServer::instance_geometry_set_draw_range);
    MethodBinder::bind_method(D_METHOD("instance_geometry_set_as_instance_lod", { "instance", "as_lod_of_instance" }),
            &RenderingServer::instance_geometry_set_as_instance_lod);

    MethodBinder::bind_method(D_METHOD("instances_cull_aabb", { "aabb", "scenario" }),
            &RenderingServer::_instances_cull_aabb_bind, { DEFVAL(RID()) });
    MethodBinder::bind_method(D_METHOD("instances_cull_ray", { "from", "to", "scenario" }),
            &RenderingServer::_instances_cull_ray_bind, { DEFVAL(RID()) });
    MethodBinder::bind_method(D_METHOD("instances_cull_convex", { "convex", "scenario" }),
            &RenderingServer::_instances_cull_convex_bind, { DEFVAL(RID()) });
#endif
    MethodBinder::bind_method(D_METHOD("canvas_create"), &RenderingServer::canvas_create);
    MethodBinder::bind_method(D_METHOD("canvas_set_item_mirroring", { "canvas", "item", "mirroring" }),
            &RenderingServer::canvas_set_item_mirroring);
    MethodBinder::bind_method(
            D_METHOD("canvas_set_modulate", { "canvas", "color" }), &RenderingServer::canvas_set_modulate);

    MethodBinder::bind_method(D_METHOD("canvas_item_create"), &RenderingServer::canvas_item_create);
    MethodBinder::bind_method(
            D_METHOD("canvas_item_set_parent", { "item", "parent" }), &RenderingServer::canvas_item_set_parent);
    MethodBinder::bind_method(
            D_METHOD("canvas_item_set_visible", { "item", "visible" }), &RenderingServer::canvas_item_set_visible);
    MethodBinder::bind_method(
            D_METHOD("canvas_item_set_light_mask", { "item", "mask" }), &RenderingServer::canvas_item_set_light_mask);
    MethodBinder::bind_method(D_METHOD("canvas_item_set_transform", { "item", "transform" }),
            &RenderingServer::canvas_item_set_transform);
    MethodBinder::bind_method(
            D_METHOD("canvas_item_set_clip", { "item", "clip" }), &RenderingServer::canvas_item_set_clip);
    MethodBinder::bind_method(D_METHOD("canvas_item_set_distance_field_mode", { "item", "enabled" }),
            &RenderingServer::canvas_item_set_distance_field_mode);
    MethodBinder::bind_method(D_METHOD("canvas_item_set_custom_rect", { "item", "use_custom_rect", "rect" }),
            &RenderingServer::canvas_item_set_custom_rect, { DEFVAL(Rect2()) });
    MethodBinder::bind_method(
            D_METHOD("canvas_item_set_modulate", { "item", "color" }), &RenderingServer::canvas_item_set_modulate);
    MethodBinder::bind_method(D_METHOD("canvas_item_set_self_modulate", { "item", "color" }),
            &RenderingServer::canvas_item_set_self_modulate);
    MethodBinder::bind_method(D_METHOD("canvas_item_set_draw_behind_parent", { "item", "enabled" }),
            &RenderingServer::canvas_item_set_draw_behind_parent);
    MethodBinder::bind_method(
            D_METHOD("canvas_item_add_line", { "item", "from", "to", "color", "width", "antialiased" }),
            &RenderingServer::canvas_item_add_line, { DEFVAL(1.0), DEFVAL(false) });
    MethodBinder::bind_method(
            D_METHOD("canvas_item_add_polyline", { "item", "points", "colors", "width", "antialiased" }),
            &RenderingServer::canvas_item_add_polyline, { DEFVAL(1.0), DEFVAL(false) });
    MethodBinder::bind_method(
            D_METHOD("canvas_item_add_rect", { "item", "rect", "color" }), &RenderingServer::canvas_item_add_rect);
    MethodBinder::bind_method(D_METHOD("canvas_item_add_circle", { "item", "pos", "radius", "color" }),
            &RenderingServer::canvas_item_add_circle);
    MethodBinder::bind_method(D_METHOD("canvas_item_add_texture_rect",
                                      { "item", "rect", "texture", "tile", "modulate", "transpose", "normal_map" }),
            &RenderingServer::canvas_item_add_texture_rect,
            { DEFVAL(false), DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(RID()) });
    MethodBinder::bind_method(
            D_METHOD("canvas_item_add_texture_rect_region",
                    { "item", "rect", "texture", "src_rect", "modulate", "transpose", "normal_map", "clip_uv" }),
            &RenderingServer::canvas_item_add_texture_rect_region,
            { DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(RID()), DEFVAL(true) });
    MethodBinder::bind_method(D_METHOD("canvas_item_add_nine_patch",
                                      { "item", "rect", "source", "texture", "topleft", "bottomright", "x_axis_mode",
                                              "y_axis_mode", "draw_center", "modulate", "normal_map" }),
            &RenderingServer::canvas_item_add_nine_patch,
            { DEFVAL(RS::NINE_PATCH_STRETCH), DEFVAL(RS::NINE_PATCH_STRETCH), DEFVAL(true), DEFVAL(Color(1, 1, 1)),
                    DEFVAL(RID()) });
    MethodBinder::bind_method(D_METHOD("canvas_item_add_primitive",
                                      { "item", "points", "colors", "uvs", "texture", "width", "normal_map" }),
            &RenderingServer::canvas_item_add_primitive, { DEFVAL(1.0), DEFVAL(RID()) });
    MethodBinder::bind_method(D_METHOD("canvas_item_add_polygon",
                                      { "item", "points", "colors", "uvs", "texture", "normal_map", "antialiased" }),
            &RenderingServer::canvas_item_add_polygon,
            { DEFVAL(Vector<Point2>()), DEFVAL(RID()), DEFVAL(RID()), DEFVAL(false) });
    MethodBinder::bind_method(D_METHOD("canvas_item_add_triangle_array",
                                      { "item", "indices", "points", "colors", "uvs", "bones", "weights", "texture",
                                              "count", "normal_map", "antialiased", "antialiasing_use_indices" }),
            &RenderingServer::canvas_item_add_triangle_array,
            { DEFVAL(Vector<Point2>()), DEFVAL(Vector<int>()), DEFVAL(Vector<float>()), DEFVAL(RID()), DEFVAL(-1),
                    DEFVAL(RID()), DEFVAL(false), DEFVAL(false) });
    MethodBinder::bind_method(
            D_METHOD("canvas_item_add_mesh", { "item", "mesh", "transform", "modulate", "texture", "normal_map" }),
            &RenderingServer::canvas_item_add_mesh,
            { DEFVAL(Transform2D()), DEFVAL(Color(1, 1, 1)), DEFVAL(RID()), DEFVAL(RID()) });

    MethodBinder::bind_method(D_METHOD("canvas_item_add_multimesh", { "item", "mesh", "texture", "normal_map" }),
            &RenderingServer::canvas_item_add_multimesh, { DEFVAL(RID()) });
    MethodBinder::bind_method(D_METHOD("canvas_item_add_particles", { "item", "particles", "texture", "normal_map" }),
            &RenderingServer::canvas_item_add_particles);
    MethodBinder::bind_method(D_METHOD("canvas_item_add_set_transform", { "item", "transform" }),
            &RenderingServer::canvas_item_add_set_transform);
    MethodBinder::bind_method(D_METHOD("canvas_item_add_clip_ignore", { "item", "ignore" }),
            &RenderingServer::canvas_item_add_clip_ignore);
    MethodBinder::bind_method(D_METHOD("canvas_item_set_sort_children_by_y", { "item", "enabled" }),
            &RenderingServer::canvas_item_set_sort_children_by_y);
    MethodBinder::bind_method(
            D_METHOD("canvas_item_set_z_index", { "item", "z_index" }), &RenderingServer::canvas_item_set_z_index);
    MethodBinder::bind_method(D_METHOD("canvas_item_set_z_as_relative_to_parent", { "item", "enabled" }),
            &RenderingServer::canvas_item_set_z_as_relative_to_parent);
    MethodBinder::bind_method(D_METHOD("canvas_item_set_copy_to_backbuffer", { "item", "enabled", "rect" }),
            &RenderingServer::canvas_item_set_copy_to_backbuffer);
    MethodBinder::bind_method(D_METHOD("canvas_item_clear", { "item" }), &RenderingServer::canvas_item_clear);
    MethodBinder::bind_method(
            D_METHOD("canvas_item_set_draw_index", { "item", "index" }), &RenderingServer::canvas_item_set_draw_index);
    MethodBinder::bind_method(
            D_METHOD("canvas_item_set_material", { "item", "material" }), &RenderingServer::canvas_item_set_material);
    MethodBinder::bind_method(D_METHOD("canvas_item_set_use_parent_material", { "item", "enabled" }),
            &RenderingServer::canvas_item_set_use_parent_material);
    MethodBinder::bind_method(D_METHOD("canvas_light_create"), &RenderingServer::canvas_light_create);
    MethodBinder::bind_method(D_METHOD("canvas_light_attach_to_canvas", { "light", "canvas" }),
            &RenderingServer::canvas_light_attach_to_canvas);
    MethodBinder::bind_method(
            D_METHOD("canvas_light_set_enabled", { "light", "enabled" }), &RenderingServer::canvas_light_set_enabled);
    MethodBinder::bind_method(
            D_METHOD("canvas_light_set_scale", { "light", "scale" }), &RenderingServer::canvas_light_set_scale);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_transform", { "light", "transform" }),
            &RenderingServer::canvas_light_set_transform);
    MethodBinder::bind_method(
            D_METHOD("canvas_light_set_texture", { "light", "texture" }), &RenderingServer::canvas_light_set_texture);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_texture_offset", { "light", "offset" }),
            &RenderingServer::canvas_light_set_texture_offset);
    MethodBinder::bind_method(
            D_METHOD("canvas_light_set_color", { "light", "color" }), &RenderingServer::canvas_light_set_color);
    MethodBinder::bind_method(
            D_METHOD("canvas_light_set_height", { "light", "height" }), &RenderingServer::canvas_light_set_height);
    MethodBinder::bind_method(
            D_METHOD("canvas_light_set_energy", { "light", "energy" }), &RenderingServer::canvas_light_set_energy);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_z_range", { "light", "min_z", "max_z" }),
            &RenderingServer::canvas_light_set_z_range);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_layer_range", { "light", "min_layer", "max_layer" }),
            &RenderingServer::canvas_light_set_layer_range);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_item_cull_mask", { "light", "mask" }),
            &RenderingServer::canvas_light_set_item_cull_mask);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_item_shadow_cull_mask", { "light", "mask" }),
            &RenderingServer::canvas_light_set_item_shadow_cull_mask);
    MethodBinder::bind_method(
            D_METHOD("canvas_light_set_mode", { "light", "mode" }), &RenderingServer::canvas_light_set_mode);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_shadow_enabled", { "light", "enabled" }),
            &RenderingServer::canvas_light_set_shadow_enabled);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_shadow_buffer_size", { "light", "size" }),
            &RenderingServer::canvas_light_set_shadow_buffer_size);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_shadow_gradient_length", { "light", "length" }),
            &RenderingServer::canvas_light_set_shadow_gradient_length);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_shadow_filter", { "light", "filter" }),
            &RenderingServer::canvas_light_set_shadow_filter);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_shadow_color", { "light", "color" }),
            &RenderingServer::canvas_light_set_shadow_color);
    MethodBinder::bind_method(D_METHOD("canvas_light_set_shadow_smooth", { "light", "smooth" }),
            &RenderingServer::canvas_light_set_shadow_smooth);

    MethodBinder::bind_method(D_METHOD("canvas_light_occluder_create"), &RenderingServer::canvas_light_occluder_create);
    MethodBinder::bind_method(D_METHOD("canvas_light_occluder_attach_to_canvas", { "occluder", "canvas" }),
            &RenderingServer::canvas_light_occluder_attach_to_canvas);
    MethodBinder::bind_method(D_METHOD("canvas_light_occluder_set_enabled", { "occluder", "enabled" }),
            &RenderingServer::canvas_light_occluder_set_enabled);
    MethodBinder::bind_method(D_METHOD("canvas_light_occluder_set_polygon", { "occluder", "polygon" }),
            &RenderingServer::canvas_light_occluder_set_polygon);
    MethodBinder::bind_method(D_METHOD("canvas_light_occluder_set_transform", { "occluder", "transform" }),
            &RenderingServer::canvas_light_occluder_set_transform);
    MethodBinder::bind_method(D_METHOD("canvas_light_occluder_set_light_mask", { "occluder", "mask" }),
            &RenderingServer::canvas_light_occluder_set_light_mask);

    MethodBinder::bind_method(
            D_METHOD("canvas_occluder_polygon_create"), &RenderingServer::canvas_occluder_polygon_create);
    MethodBinder::bind_method(D_METHOD("canvas_occluder_polygon_set_shape", { "occluder_polygon", "shape", "closed" }),
            &RenderingServer::canvas_occluder_polygon_set_shape);
    MethodBinder::bind_method(D_METHOD("canvas_occluder_polygon_set_shape_as_lines", { "occluder_polygon", "shape" }),
            &RenderingServer::canvas_occluder_polygon_set_shape_as_lines);
    MethodBinder::bind_method(D_METHOD("canvas_occluder_polygon_set_cull_mode", { "occluder_polygon", "mode" }),
            &RenderingServer::canvas_occluder_polygon_set_cull_mode);

    MethodBinder::bind_method(D_METHOD("black_bars_set_margins", { "left", "top", "right", "bottom" }),
            &RenderingServer::black_bars_set_margins);
    MethodBinder::bind_method(D_METHOD("black_bars_set_images", { "left", "top", "right", "bottom" }),
            &RenderingServer::black_bars_set_images);

    MethodBinder::bind_method(
            D_METHOD("free_rid", { "rid" }), &RenderingServer::free_rid); // shouldn't conflict with Object::free()

    MethodBinder::bind_method(
            D_METHOD("request_frame_drawn_callback", { "callback" }), &RenderingServer::request_frame_drawn_callback);
    MethodBinder::bind_method(D_METHOD("has_changed"), &RenderingServer::has_changed);
    MethodBinder::bind_method(D_METHOD("init"), &RenderingServer::init);
    MethodBinder::bind_method(D_METHOD("finish"), &RenderingServer::finish);
    MethodBinder::bind_method(D_METHOD("get_render_info", { "info" }), &RenderingServer::get_render_info);
    //    MethodBinder::bind_method(D_METHOD("get_video_adapter_name"), &RenderingServer::get_video_adapter_name);
    //	MethodBinder::bind_method(D_METHOD("get_video_adapter_vendor"), &RenderingServer::get_video_adapter_vendor);

#ifndef _3D_DISABLED

    MethodBinder::bind_method(
            D_METHOD("make_sphere_mesh", { "latitudes", "longitudes", "radius" }), &RenderingServer::make_sphere_mesh);
#endif

    MethodBinder::bind_method(D_METHOD("set_boot_image", { "image", "color", "scale", "use_filter" }),
            &RenderingServer::set_boot_image, { DEFVAL(true) });
    MethodBinder::bind_method(
            D_METHOD("set_default_clear_color", { "color" }), &RenderingServer::set_default_clear_color);

    MethodBinder::bind_method(D_METHOD("has_feature", { "feature" }), &RenderingServer::has_feature);
    MethodBinder::bind_method(D_METHOD("has_os_feature", { "feature" }), &RenderingServer::has_os_feature);
    MethodBinder::bind_method(
            D_METHOD("set_debug_generate_wireframes", { "generate" }), &RenderingServer::set_debug_generate_wireframes);

    ClassDB::add_namespace("RenderingServerEnums", "servers/rendering_server_enums.h");
    // using namespace RS;
    BIND_NS_CONSTANT(RenderingServerEnums, NO_INDEX_ARRAY);
    BIND_NS_CONSTANT(RenderingServerEnums, ARRAY_WEIGHTS_SIZE);
    BIND_NS_CONSTANT(RenderingServerEnums, CANVAS_ITEM_Z_MIN);
    BIND_NS_CONSTANT(RenderingServerEnums, CANVAS_ITEM_Z_MAX);
    BIND_NS_CONSTANT(RenderingServerEnums, MAX_GLOW_LEVELS);
    BIND_NS_CONSTANT(RenderingServerEnums, MATERIAL_RENDER_PRIORITY_MIN);
    BIND_NS_CONSTANT(RenderingServerEnums, MATERIAL_RENDER_PRIORITY_MAX);

    ClassDB::register_enum_type("RenderingServerEnums", "RenderingServerEnums::CubeMapSide", "uint8_t");
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CUBEMAP_LEFT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CUBEMAP_RIGHT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CUBEMAP_BOTTOM);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CUBEMAP_TOP);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CUBEMAP_FRONT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CUBEMAP_BACK);
    // TODO: use ClassDB::register_enum_type to properly set underlying enum type in all following registrations.

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_TYPE_2D);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_TYPE_CUBEMAP);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_TYPE_2D_ARRAY);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_TYPE_3D);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_FLAG_MIPMAPS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_FLAG_REPEAT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_FLAG_FILTER);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_FLAG_ANISOTROPIC_FILTER);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_FLAG_CONVERT_TO_LINEAR);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_FLAG_MIRRORED_REPEAT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_FLAG_USED_FOR_STREAMING);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, TEXTURE_FLAGS_DEFAULT);

    BIND_NS_ENUM_CLASS_CONSTANT(RenderingServerEnums, ShaderMode, SPATIAL);
    BIND_NS_ENUM_CLASS_CONSTANT(RenderingServerEnums, ShaderMode, CANVAS_ITEM);
    BIND_NS_ENUM_CLASS_CONSTANT(RenderingServerEnums, ShaderMode, PARTICLES);
    BIND_NS_ENUM_CLASS_CONSTANT(RenderingServerEnums, ShaderMode, MAX);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_VERTEX);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_NORMAL);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_TANGENT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COLOR);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_TEX_UV);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_TEX_UV2);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_BONES);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_WEIGHTS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_INDEX);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_MAX);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FORMAT_VERTEX);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FORMAT_NORMAL);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FORMAT_TANGENT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FORMAT_COLOR);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FORMAT_TEX_UV);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FORMAT_TEX_UV2);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FORMAT_BONES);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FORMAT_WEIGHTS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FORMAT_INDEX);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COMPRESS_VERTEX);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COMPRESS_NORMAL);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COMPRESS_TANGENT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COMPRESS_COLOR);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COMPRESS_TEX_UV);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COMPRESS_TEX_UV2);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COMPRESS_BONES);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COMPRESS_WEIGHTS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COMPRESS_INDEX);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FLAG_USE_2D_VERTICES);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FLAG_USE_16_BIT_BONES);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_COMPRESS_DEFAULT);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PRIMITIVE_POINTS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PRIMITIVE_LINES);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PRIMITIVE_LINE_STRIP);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PRIMITIVE_LINE_LOOP);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PRIMITIVE_TRIANGLES);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PRIMITIVE_TRIANGLE_STRIP);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PRIMITIVE_TRIANGLE_FAN);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PRIMITIVE_MAX);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, BLEND_SHAPE_MODE_NORMALIZED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, BLEND_SHAPE_MODE_RELATIVE);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_DIRECTIONAL);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_OMNI);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_SPOT);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_ENERGY);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SPECULAR);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_RANGE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_ATTENUATION);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SPOT_ANGLE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SPOT_ATTENUATION);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_CONTACT_SHADOW_SIZE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SHADOW_MAX_DISTANCE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SHADOW_NORMAL_BIAS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SHADOW_BIAS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SHADOW_BIAS_SPLIT_SCALE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_MAX);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums,LIGHT_BAKE_DISABLED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums,LIGHT_BAKE_INDIRECT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums,LIGHT_BAKE_ALL);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_OMNI_SHADOW_DUAL_PARABOLOID);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_OMNI_SHADOW_CUBE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_OMNI_SHADOW_DETAIL_VERTICAL);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_OMNI_SHADOW_DETAIL_HORIZONTAL);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_OPTIMIZED);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_UPDATE_DISABLED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_UPDATE_ONCE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_UPDATE_WHEN_VISIBLE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_UPDATE_ALWAYS);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_CLEAR_ALWAYS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_CLEAR_NEVER);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_CLEAR_ONLY_NEXT_FRAME);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_MSAA_DISABLED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_MSAA_2X);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_MSAA_4X);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_MSAA_8X);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_MSAA_16X);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_MSAA_EXT_2X);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_MSAA_EXT_4X);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_USAGE_2D);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_USAGE_2D_NO_SAMPLING);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_USAGE_3D);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_USAGE_3D_NO_EFFECTS);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_VERTICES_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_MATERIAL_CHANGES_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_SHADER_CHANGES_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_SURFACE_CHANGES_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_2D_ITEMS_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_2D_DRAW_CALLS_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_MAX);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_DEBUG_DRAW_DISABLED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_DEBUG_DRAW_UNSHADED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_DEBUG_DRAW_OVERDRAW);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_DEBUG_DRAW_WIREFRAME);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, SCENARIO_DEBUG_DISABLED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, SCENARIO_DEBUG_WIREFRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, SCENARIO_DEBUG_OVERDRAW);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, SCENARIO_DEBUG_SHADELESS);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_NONE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_MESH);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_MULTIMESH);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_IMMEDIATE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_PARTICLES);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_LIGHT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_REFLECTION_PROBE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_GI_PROBE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_LIGHTMAP_CAPTURE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_MAX);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_GEOMETRY_MASK);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_FLAG_USE_BAKED_LIGHT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INSTANCE_FLAG_MAX);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, SHADOW_CASTING_SETTING_OFF);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, SHADOW_CASTING_SETTING_ON);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, SHADOW_CASTING_SETTING_DOUBLE_SIDED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, SHADOW_CASTING_SETTING_SHADOWS_ONLY);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, NINE_PATCH_STRETCH);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, NINE_PATCH_TILE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, NINE_PATCH_TILE_FIT);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_LIGHT_MODE_ADD);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_LIGHT_MODE_SUB);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_LIGHT_MODE_MIX);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_LIGHT_MODE_MASK);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_LIGHT_FILTER_NONE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_LIGHT_FILTER_PCF3);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_LIGHT_FILTER_PCF5);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_LIGHT_FILTER_PCF7);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_LIGHT_FILTER_PCF9);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_LIGHT_FILTER_PCF13);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_OCCLUDER_POLYGON_CULL_DISABLED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_OCCLUDER_POLYGON_CULL_CLOCKWISE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CANVAS_OCCLUDER_POLYGON_CULL_COUNTER_CLOCKWISE);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_OBJECTS_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_VERTICES_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_MATERIAL_CHANGES_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_SHADER_CHANGES_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_SURFACE_CHANGES_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_DRAW_CALLS_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_2D_ITEMS_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_2D_DRAW_CALLS_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_USAGE_VIDEO_MEM_TOTAL);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_VIDEO_MEM_USED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_TEXTURE_MEM_USED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_VERTEX_MEM_USED);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, FEATURE_SHADERS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, FEATURE_MULTITHREADED);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, MULTIMESH_TRANSFORM_2D);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, MULTIMESH_TRANSFORM_3D);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, MULTIMESH_COLOR_NONE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, MULTIMESH_COLOR_8BIT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, MULTIMESH_COLOR_FLOAT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, MULTIMESH_CUSTOM_DATA_NONE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, MULTIMESH_CUSTOM_DATA_8BIT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, MULTIMESH_CUSTOM_DATA_FLOAT);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, REFLECTION_PROBE_UPDATE_ONCE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, REFLECTION_PROBE_UPDATE_ALWAYS);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PARTICLES_DRAW_ORDER_INDEX);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PARTICLES_DRAW_ORDER_LIFETIME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, PARTICLES_DRAW_ORDER_VIEW_DEPTH);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_BG_CLEAR_COLOR);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_BG_COLOR);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_BG_SKY);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_BG_COLOR_SKY);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_BG_CANVAS);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_BG_KEEP);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_BG_MAX);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_DOF_BLUR_QUALITY_LOW);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_DOF_BLUR_QUALITY_MEDIUM);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_DOF_BLUR_QUALITY_HIGH);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, GLOW_BLEND_MODE_ADDITIVE);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, GLOW_BLEND_MODE_SCREEN);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, GLOW_BLEND_MODE_SOFTLIGHT);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, GLOW_BLEND_MODE_REPLACE);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_TONE_MAPPER_LINEAR);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_TONE_MAPPER_REINHARD);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_TONE_MAPPER_FILMIC);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_TONE_MAPPER_ACES);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_QUALITY_LOW);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_QUALITY_MEDIUM);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_QUALITY_HIGH);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_BLUR_DISABLED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_BLUR_1x1);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_BLUR_2x2);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_BLUR_3x3);

    ADD_SIGNAL(MethodInfo("frame_pre_draw"));
    ADD_SIGNAL(MethodInfo("frame_post_draw"));
}

void RenderingServer::_canvas_item_add_style_box(RID p_item, const Rect2 &p_rect, const Rect2 &p_source, RID p_texture,
        const Vector<float> &p_margins, const Color &p_modulate) {
    ERR_FAIL_COND(p_margins.size() != 4);
    // canvas_item_add_style_box(p_item,p_rect,p_source,p_texture,Vector2(p_margins[0],p_margins[1]),Vector2(p_margins[2],p_margins[3]),true,p_modulate);
}

void RenderingServer::_camera_set_orthogonal(RID p_camera, float p_size, float p_z_near, float p_z_far) {
    camera_set_orthogonal(p_camera, p_size, p_z_near, p_z_far);
}

void RenderingServer::mesh_add_surface_from_mesh_data(RID p_mesh, const Geometry::MeshData &p_mesh_data) {
    Vector<Vector3> vertices;
    Vector<Vector3> normals;
    size_t cnt = 0;
    for (const Geometry::MeshData::Face &f : p_mesh_data.faces) {
        cnt += f.indices.size() - 2;
    }
    vertices.reserve(cnt * 3);
    normals.reserve(cnt * 3);

#define _ADD_VERTEX(m_idx)                                                                                             \
    vertices.emplace_back(p_mesh_data.vertices[f.indices[m_idx]]);                                                        \
    normals.emplace_back(f.plane.normal);

    for (const Geometry::MeshData::Face &f : p_mesh_data.faces) {
        for (int j = 2; j < f.indices.size(); j++) {
            _ADD_VERTEX(0);
            _ADD_VERTEX(j - 1);
            _ADD_VERTEX(j);
        }
    }

    SurfaceArrays d(eastl::move(vertices));
    d.m_normals = eastl::move(normals);
    mesh_add_surface_from_arrays(p_mesh, RS::PRIMITIVE_TRIANGLES, eastl::move(d));
#undef _ADD_VERTEX
}

void RenderingServer::mesh_add_surface_from_planes(RID p_mesh, const PoolVector<Plane> &p_planes) {
    Geometry::MeshData mdata = Geometry::build_convex_mesh(p_planes);
    mesh_add_surface_from_mesh_data(p_mesh, eastl::move(mdata));
}

void RenderingServer::immediate_vertex_2d(RID p_immediate, const Vector2 &p_vertex) {
    immediate_vertex(p_immediate, Vector3(p_vertex.x, p_vertex.y, 0));
}

RID RenderingServer::instance_create2(RID p_base, RID p_scenario) {
    RID instance = instance_create();
    instance_set_base(instance, p_base);
    instance_set_scenario(instance, p_scenario);
    return instance;
}

RenderingServer::RenderingServer() {
    // ERR_FAIL_COND();
    auto ps(ProjectSettings::get_singleton());
    GLOBAL_DEF_RST("rendering/vram_compression/import_bptc", false);
    GLOBAL_DEF_RST("rendering/vram_compression/import_s3tc", true);
    GLOBAL_DEF_RST("rendering/vram_compression/import_etc", false);
    GLOBAL_DEF_RST("rendering/vram_compression/import_etc2", true);
    GLOBAL_DEF_RST("rendering/vram_compression/import_pvrtc", false);

    GLOBAL_DEF("rendering/limits/time/time_rollover_secs", 3600);
    ps->set_custom_property_info("rendering/limits/time/time_rollover_secs",
            PropertyInfo(VariantType::FLOAT, "rendering/limits/time/time_rollover_secs", PropertyHint::Range,
                    "0,10000,1,or_greater"));

    GLOBAL_DEF("rendering/quality/directional_shadow/size", 4096);
    GLOBAL_DEF("rendering/quality/directional_shadow/size.mobile", 2048);
    ps->set_custom_property_info("rendering/quality/directional_shadow/size",
            PropertyInfo(
                    VariantType::INT, "rendering/quality/directional_shadow/size", PropertyHint::Range, "256,16384"));
    GLOBAL_DEF("rendering/quality/shadow_atlas/size", 4096);
    GLOBAL_DEF("rendering/quality/shadow_atlas/size.mobile", 2048);
    ps->set_custom_property_info("rendering/quality/shadow_atlas/size",
            PropertyInfo(VariantType::INT, "rendering/quality/shadow_atlas/size", PropertyHint::Range, "256,16384"));
    GLOBAL_DEF("rendering/quality/shadow_atlas/quadrant_0_subdiv", 1);
    GLOBAL_DEF("rendering/quality/shadow_atlas/quadrant_1_subdiv", 2);
    GLOBAL_DEF("rendering/quality/shadow_atlas/quadrant_2_subdiv", 3);
    GLOBAL_DEF("rendering/quality/shadow_atlas/quadrant_3_subdiv", 4);
    ps->set_custom_property_info("rendering/quality/shadow_atlas/quadrant_0_subdiv",
            PropertyInfo(VariantType::INT, "rendering/quality/shadow_atlas/quadrant_0_subdiv", PropertyHint::Enum,
                    "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"));
    ps->set_custom_property_info("rendering/quality/shadow_atlas/quadrant_1_subdiv",
            PropertyInfo(VariantType::INT, "rendering/quality/shadow_atlas/quadrant_1_subdiv", PropertyHint::Enum,
                    "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"));
    ps->set_custom_property_info("rendering/quality/shadow_atlas/quadrant_2_subdiv",
            PropertyInfo(VariantType::INT, "rendering/quality/shadow_atlas/quadrant_2_subdiv", PropertyHint::Enum,
                    "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"));
    ps->set_custom_property_info("rendering/quality/shadow_atlas/quadrant_3_subdiv",
            PropertyInfo(VariantType::INT, "rendering/quality/shadow_atlas/quadrant_3_subdiv", PropertyHint::Enum,
                    "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"));

    GLOBAL_DEF("rendering/quality/shadows/filter_mode", 1);
    GLOBAL_DEF("rendering/quality/shadows/filter_mode.mobile", 0);
    ps->set_custom_property_info("rendering/quality/shadows/filter_mode",
            PropertyInfo(VariantType::INT, "rendering/quality/shadows/filter_mode", PropertyHint::Enum,
                    "Disabled,PCF5,PCF13"));

    GLOBAL_DEF("rendering/quality/reflections/texture_array_reflections", true);
    GLOBAL_DEF("rendering/quality/reflections/texture_array_reflections.mobile", false);
    GLOBAL_DEF("rendering/quality/reflections/high_quality_ggx", true);
    GLOBAL_DEF("rendering/quality/reflections/high_quality_ggx.mobile", false);
    GLOBAL_DEF("rendering/quality/reflections/irradiance_max_size", 128);
    ps->set_custom_property_info("rendering/quality/reflections/irradiance_max_size",
            PropertyInfo(VariantType::INT, "rendering/quality/reflections/irradiance_max_size", PropertyHint::Range,
                    "32,2048"));

    GLOBAL_DEF("rendering/quality/shading/force_vertex_shading", false);
    GLOBAL_DEF("rendering/quality/shading/force_vertex_shading.mobile", true);
    GLOBAL_DEF("rendering/quality/shading/force_lambert_over_burley", false);
    GLOBAL_DEF("rendering/quality/shading/force_lambert_over_burley.mobile", true);
    GLOBAL_DEF("rendering/quality/shading/force_blinn_over_ggx", false);
    GLOBAL_DEF("rendering/quality/shading/force_blinn_over_ggx.mobile", true);

    GLOBAL_DEF("rendering/quality/depth_prepass/enable", true);
    // GLOBAL_DEF("rendering/quality/depth_prepass/disable_for_vendors", "PowerVR,Mali,Adreno,Apple");

    GLOBAL_DEF("rendering/quality/filters/use_nearest_mipmap_filter", false);

    GLOBAL_DEF("rendering/quality/skinning/software_skinning_fallback", true);
    GLOBAL_DEF("rendering/quality/skinning/force_software_skinning", false);

    StringName sz_balance_render_tree("rendering/quality/spatial_partitioning/render_tree_balance");
    T_GLOBAL_DEF<float>(sz_balance_render_tree, 0.0f);
    ps->set_custom_property_info(sz_balance_render_tree, PropertyInfo(VariantType::FLOAT, sz_balance_render_tree, PropertyHint::Range, "0.0,1.0,0.01"));

    GLOBAL_DEF("rendering/quality/2d/use_software_skinning", true);
    GLOBAL_DEF("rendering/quality/2d/ninepatch_mode", 0);
    ps->set_custom_property_info("rendering/quality/2d/ninepatch_mode", PropertyInfo(VariantType::INT, "rendering/quality/2d/ninepatch_mode", PropertyHint::Enum, "Default,Scaling"));

    GLOBAL_DEF("rendering/batching/options/use_batching", true);
    GLOBAL_DEF_RST("rendering/batching/options/use_batching_in_editor", true);
    GLOBAL_DEF("rendering/batching/options/single_rect_fallback", false);
    GLOBAL_DEF("rendering/batching/parameters/max_join_item_commands", 16);
    GLOBAL_DEF("rendering/batching/parameters/colored_vertex_format_threshold", 0.25f);
    GLOBAL_DEF("rendering/batching/lights/scissor_area_threshold", 1.0f);
    GLOBAL_DEF("rendering/batching/lights/max_join_items", 32);
    GLOBAL_DEF("rendering/batching/parameters/batch_buffer_size", 16384);
    GLOBAL_DEF("rendering/batching/parameters/item_reordering_lookahead", 4);
    GLOBAL_DEF("rendering/batching/debug/flash_batching", false);
    GLOBAL_DEF("rendering/batching/debug/diagnose_frame", false);
    GLOBAL_DEF("rendering/batching/precision/uv_contract", false);
    GLOBAL_DEF("rendering/batching/precision/uv_contract_amount", 100);

    ps->set_custom_property_info("rendering/batching/parameters/max_join_item_commands", PropertyInfo(VariantType::INT, "rendering/batching/parameters/max_join_item_commands", PropertyHint::Range, "0,65535"));
    ps->set_custom_property_info("rendering/batching/parameters/colored_vertex_format_threshold", PropertyInfo(VariantType::FLOAT, "rendering/batching/parameters/colored_vertex_format_threshold", PropertyHint::Range, "0.0,1.0,0.01"));
    ps->set_custom_property_info("rendering/batching/parameters/batch_buffer_size", PropertyInfo(VariantType::INT, "rendering/batching/parameters/batch_buffer_size", PropertyHint::Range, "1024,65535,1024"));
    ps->set_custom_property_info("rendering/batching/lights/scissor_area_threshold", PropertyInfo(VariantType::FLOAT, "rendering/batching/lights/scissor_area_threshold", PropertyHint::Range, "0.0,1.0"));
    ps->set_custom_property_info("rendering/batching/lights/max_join_items", PropertyInfo(VariantType::INT, "rendering/batching/lights/max_join_items", PropertyHint::Range, "0,512"));
    ps->set_custom_property_info("rendering/batching/parameters/item_reordering_lookahead", PropertyInfo(VariantType::INT, "rendering/batching/parameters/item_reordering_lookahead", PropertyHint::Range, "0,256"));
    ps->set_custom_property_info("rendering/batching/precision/uv_contract_amount", PropertyInfo(VariantType::INT, "rendering/batching/precision/uv_contract_amount", PropertyHint::Range, "0,10000"));

}

RenderingServer::~RenderingServer() {
}
