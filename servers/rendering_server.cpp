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
#include "rendering/rendering_server_wrap_mt.h"

#include "core/image_enum_casters.h"
#include "core/method_bind.h"
#include "core/project_settings.h"

IMPL_GDCLASS(RenderingServer)
RenderingServer *RenderingServer::submission_thread_singleton = nullptr;
RenderingServer* RenderingServer::queueing_thread_singleton = nullptr;
Thread::ID RenderingServer::server_thread;

namespace {
constexpr Vector2 SMALL_VEC2(0.00001f, 0.00001f);
constexpr Vector3 SMALL_VEC3(0.00001f, 0.00001f, 0.00001f);

} // namespace

// Maps normalized vector to an octahedron projected onto the cartesian plane
// Resulting 2D vector in range [-1, 1]
// See http://jcgt.org/published/0003/02/01/ for details
Vector2 RenderingServer::norm_to_oct(const Vector3 v) {
    const float L1Norm = Math::absf(v.x) + Math::absf(v.y) + Math::absf(v.z);

    // NOTE: this will mean it decompresses to 0,0,1
    // Discussed heavily here: https://github.com/godotengine/godot/pull/51268 as to why we did this
    if (Math::is_zero_approx(L1Norm)) {
        WARN_PRINT_ONCE("Octahedral compression cannot be used to compress a zero-length vector, please use normalized "
                        "normal values or disable octahedral compression");
        return Vector2(0, 0);
    }

    const float invL1Norm = 1.0f / L1Norm;

    Vector2 res;
    if (v.z < 0.0f) {
        res.x = (1.0f - Math::absf(v.y * invL1Norm)) * SGN(v.x);
        res.y = (1.0f - Math::absf(v.x * invL1Norm)) * SGN(v.y);
    } else {
        res.x = v.x * invL1Norm;
        res.y = v.y * invL1Norm;
    }

    return res;
}

// Maps normalized tangent vector to an octahedron projected onto the cartesian plane
// Encodes the tangent vector sign in the second component of the returned Vector2 for use in shaders
// high_precision specifies whether the encoding will be 32 bit (true) or 16 bit (false)
// Resulting 2D vector in range [-1, 1]
// See http://jcgt.org/published/0003/02/01/ for details
Vector2 RenderingServer::tangent_to_oct(const Vector3 v, const float sign, const bool high_precision) {
    float bias = high_precision ? 1.0f / 32767 : 1.0f / 127;
    Vector2 res = norm_to_oct(v);
    res.y = res.y * 0.5f + 0.5f;
    res.y = eastl::max(res.y, bias) * SGN(sign);
    return res;
}

// Convert Octahedron-mapped normalized vector back to Cartesian
// Assumes normalized format (elements of v within range [-1, 1])
Vector3 RenderingServer::oct_to_norm(const Vector2 v) {
    Vector3 res(v.x, v.y, 1 - (Math::absf(v.x) + Math::absf(v.y)));
    float t = eastl::max(-res.z, 0.0f);
    res.x += t * -SGN(res.x);
    res.y += t * -SGN(res.y);
    return res.normalized();
}

// Convert Octahedron-mapped normalized tangent vector back to Cartesian
// out_sign provides the direction for the original cartesian tangent
// Assumes normalized format (elements of v within range [-1, 1])
Vector3 RenderingServer::oct_to_tangent(const Vector2 v, float *out_sign) {
    Vector2 v_decompressed = v;
    v_decompressed.y = Math::absf(v_decompressed.y) * 2 - 1;
    const Vector3 res = oct_to_norm(v_decompressed);
    *out_sign = SGN(v[1]);
    return res;
}
RenderingEntity RenderingServer::texture_create_from_image(const Ref<Image> &p_image, uint32_t p_flags) {
    ERR_FAIL_COND_V(not p_image, entt::null);
    RenderingEntity texture = texture_create();
    texture_allocate(texture, p_image->get_width(), p_image->get_height(), 0, p_image->get_format(),
            RS::TEXTURE_TYPE_2D, p_flags); // if it has mipmaps, use, else generate
    ERR_FAIL_COND_V(texture == entt::null, texture);

    texture_set_data(texture, p_image);

    return texture;
}

Array RenderingServer::_texture_debug_usage_bind() {
    Vector<TextureInfo> tex_infos;
    texture_debug_usage(&tex_infos);
    Array arr;
    for (const TextureInfo &E : tex_infos) {
        Dictionary dict;
        dict["texture"] = Variant::from(E.texture);
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

Array RenderingServer::_shader_get_param_list_bind(RenderingEntity p_shader) const {
    Vector<PropertyInfo> l;
    shader_get_param_list(p_shader, &l);
    return convert_property_vector(l);
}

static Array to_array(const Vector<GameEntity> &ids) {
    Array a;
    a.resize(ids.size());
    for (int i = 0; i < ids.size(); ++i) {
        a[i] = Variant::from(ids[i]);
    }
    return a;
}

Array RenderingServer::_instances_cull_aabb_bind(const AABB &p_aabb, RenderingEntity p_scenario) const {
    Vector<GameEntity> ids = instances_cull_aabb(p_aabb, p_scenario);
    return to_array(ids);
}

Array RenderingServer::_instances_cull_ray_bind(
        const Vector3 &p_from, const Vector3 &p_to, RenderingEntity p_scenario) const {
    Vector<GameEntity> ids = instances_cull_ray(p_from, p_to, p_scenario);
    return to_array(ids);
}

Array RenderingServer::_instances_cull_convex_bind(const Array &p_convex, RenderingEntity p_scenario) const {
    // TODO: SEGS: use a fixed vector here, with a sane'ish number of on-stack entries, and marked as growing to allow
    // for larger counts.
    Vector<Plane> planes;
    planes.reserve(p_convex.size());
    for (int i = 0; i < p_convex.size(); ++i) {
        Variant v = p_convex[i];
        ERR_FAIL_COND_V(v.get_type() != VariantType::PLANE, Array());
        planes.push_back(v.as<Plane>());
    }

    Vector<GameEntity> ids = instances_cull_convex(planes, p_scenario);
    return to_array(ids);
}

RenderingEntity RenderingServer::make_sphere_mesh(int p_lats, int p_lons, float p_radius) {
    Vector<Vector3> vertices;
    Vector<Vector3> normals;

    for (int i = 1; i <= p_lats; i++) {
        const double lat0 = Math_PI * (-0.5 + (double)(i - 1) / p_lats);
        const double z0 = Math::sin(lat0);
        const double zr0 = Math::cos(lat0);

        const double lat1 = Math_PI * (-0.5 + (double)i / p_lats);
        const double z1 = Math::sin(lat1);
        const double zr1 = Math::cos(lat1);

        for (int j = p_lons; j >= 1; j--) {
            const double lng0 = 2 * Math_PI * (double)(j - 1) / p_lons;
            const double x0 = Math::cos(lng0);
            const double y0 = Math::sin(lng0);

            const double lng1 = 2 * Math_PI * (double)(j) / p_lons;
            const double x1 = Math::cos(lng1);
            const double y1 = Math::sin(lng1);

            const Vector3 v[4] = {
                Vector3(x1 * zr0, z0, y1 * zr0),
                Vector3(x1 * zr1, z1, y1 * zr1),
                Vector3(x0 * zr1, z1, y0 * zr1),
                Vector3(x0 * zr0, z0, y0 * zr0) };

#define ADD_POINT(m_idx)                                                                                               \
    normals.emplace_back(v[m_idx]);                                                                                       \
    vertices.emplace_back(v[m_idx] * p_radius);

            ADD_POINT(0)
            ADD_POINT(1)
            ADD_POINT(2)

            ADD_POINT(2)
            ADD_POINT(3)
            ADD_POINT(0)
        }
    }

    RenderingEntity mesh = mesh_create();
    SurfaceArrays d(eastl::move(vertices));
    d.m_normals = eastl::move(normals);

    mesh_add_surface_from_arrays(mesh, RS::PRIMITIVE_TRIANGLES, d);

    return mesh;
}


Error RenderingServer::_surface_set_data(const SurfaceArrays &p_arrays, uint32_t p_format, uint32_t *p_offsets,
        uint32_t *p_stride, Vector<uint8_t> &r_vertex_array, int p_vertex_array_len, Vector<uint8_t> &r_index_array,
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

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], vector, sizeof(uint16_t) * 2);

                            if (i == 0) {
                                aabb = Rect2(src[i], SMALL_VEC2); // must have a bit of size
                            } else {
                                aabb.expand_to(src[i]);
                            }
                        }

                    } else {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            float vector[2] = { src[i].x, src[i].y };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], vector, sizeof(float) * 2);

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

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], vector, sizeof(uint16_t) * 4);

                            if (i == 0) {
                                aabb = AABB(src[i], SMALL_VEC3);
                            } else {
                                aabb.expand_to(src[i]);
                            }
                        }

                    } else {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            float vector[3] = { src[i].x, src[i].y, src[i].z };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], vector, sizeof(float) * 3);

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
                if (p_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
                    if ((p_format & RS::ARRAY_COMPRESS_NORMAL) && (p_format & RS::ARRAY_FORMAT_TANGENT) &&
                            (p_format & RS::ARRAY_COMPRESS_TANGENT)) {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            Vector2 res = norm_to_oct(src[i]);
                            int8_t vector[2] = {
                                CLAMP<int8_t>(res.x * 127, -128, 127),
                                CLAMP<int8_t>(res.y * 127, -128, 127),
                            };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], vector, 2);
                        }

                    } else {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            Vector2 res = norm_to_oct(src[i]);
                            int16_t vector[2] = {
                                CLAMP<int16_t>(res.x * 32767, -32768, 32767),
                                CLAMP<int16_t>(res.y * 32767, -32768, 32767),
                            };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], vector, 4);
                        }
                    }
                } else {
                    if (p_format & RS::ARRAY_COMPRESS_NORMAL) {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            int8_t vector[4] = {
                                    CLAMP<int8_t>(src[i].x * 127, -128, 127),
                                    CLAMP<int8_t>(src[i].y * 127, -128, 127),
                                    CLAMP<int8_t>(src[i].z * 127, -128, 127),
                                0,
                            };

                                memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], vector, 4);
                        }

                    } else {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            float vector[3] = { src[i].x, src[i].y, src[i].z };
                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], vector, 3 * 4);
                        }
                    }
                }

            } break;

            case RS::ARRAY_TANGENT: {
                const Vector<float> &array = p_arrays.m_tangents;

                ERR_FAIL_COND_V(array.size() != p_vertex_array_len * 4, ERR_INVALID_PARAMETER);

                const real_t *src = array.data();

                if (p_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
                if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                            Vector3 source(src[i * 4 + 0], src[i * 4 + 1], src[i * 4 + 2]);
                            Vector2 res = tangent_to_oct(source, src[i * 4 + 3], false);

                            int8_t vector[2] = {
                                CLAMP<int8_t>(res.x * 127, -128, 127),
                                CLAMP<int8_t>(res.y * 127, -128, 127) };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], vector, 2);
                        }

                    } else {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            Vector3 source(src[i * 4 + 0], src[i * 4 + 1], src[i * 4 + 2]);
                            Vector2 res = tangent_to_oct(source, src[i * 4 + 3], true);

                            int16_t vector[2] = { CLAMP<int16_t>(res.x * 32767, -32768, 32767),
                                CLAMP<int16_t>(res.y * 32767, -32768, 32767) };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], vector, 4);
                        }
                    }
                } else {
                    if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                        for (int i = 0; i < p_vertex_array_len; i++) {
                            int8_t xyzw[4] = { CLAMP<int8_t>(src[i * 4 + 0] * 127, -128, 127),
                                CLAMP<int8_t>(src[i * 4 + 1] * 127, -128, 127),
                                CLAMP<int8_t>(src[i * 4 + 2] * 127, -128, 127),
                                CLAMP<int8_t>(src[i * 4 + 3] * 127, -128, 127) };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], xyzw, 4);
                    }

                } else {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        float xyzw[4] = { src[i * 4 + 0], src[i * 4 + 1], src[i * 4 + 2], src[i * 4 + 3] };

                            memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], xyzw, 4 * 4);
                        }
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
                            colors[j] = CLAMP(int((src[i].component(j)) * 255.0f), 0, 255);
                        }

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], colors, 4);
                    }
                } else {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], &src[i], 4 * 4);
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
                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], uv, 2 * 2);
                    }

                } else {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        float uv[2] = { src[i].x, src[i].y };

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], uv, 2 * 4);
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
                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], uv, 2 * 2);
                    }

                } else {
                    for (int i = 0; i < p_vertex_array_len; i++) {
                        float uv[2] = { src[i].x, src[i].y };

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], uv, 2 * 4);
                    }
                }
            } break;
            case RS::ARRAY_WEIGHTS: {
                // ERR_FAIL_COND_V(p_arrays[ai].get_type() != VariantType::POOL_FLOAT32_ARRAY, ERR_INVALID_PARAMETER);

                const Vector<real_t> &array = p_arrays.m_weights;

                ERR_FAIL_COND_V(array.size() != p_vertex_array_len * RS::ARRAY_WEIGHTS_SIZE, ERR_INVALID_PARAMETER);

                const real_t *src = array.data();

                if (p_format & RS::ARRAY_COMPRESS_WEIGHTS) {
                    for (size_t i = 0; i < p_vertex_array_len; i++) {
                        uint16_t data[RS::ARRAY_WEIGHTS_SIZE];
                        for (size_t j = 0; j < RS::ARRAY_WEIGHTS_SIZE; j++) {
                            data[j] = CLAMP<float>(src[i * RS::ARRAY_WEIGHTS_SIZE + j] * 65535, 0, 65535);
                        }

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], data, 2 * 4);
                    }
                } else {
                    for (size_t i = 0; i < p_vertex_array_len; i++) {
                        float data[RS::ARRAY_WEIGHTS_SIZE];
                        for (int j = 0; j < RS::ARRAY_WEIGHTS_SIZE; j++) {
                            data[j] = src[i * RS::ARRAY_WEIGHTS_SIZE + j];
                        }

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], data, 4 * 4);
                    }
                }

            } break;
            case RS::ARRAY_BONES: {
                // ERR_FAIL_COND_V(p_arrays[ai].get_type() != VariantType::POOL_INT_ARRAY && p_arrays[ai].get_type() !=
                // VariantType::POOL_FLOAT32_ARRAY, ERR_INVALID_PARAMETER);

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

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], data, 4);
                    }

                } else {
                    for (size_t i = 0; i < p_vertex_array_len; i++) {
                        uint16_t data[RS::ARRAY_WEIGHTS_SIZE];
                        for (size_t j = 0; j < RS::ARRAY_WEIGHTS_SIZE; j++) {
                            data[j] = src[i * RS::ARRAY_WEIGHTS_SIZE + j];
                            max_bone = M_MAX(data[j], max_bone);
                        }

                        memcpy(&r_vertex_array[p_offsets[ai] + i * p_stride[ai]], data, 2 * 4);
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

        if (!vertices.empty() && bones.size() == vertices.size() * 4 && weights.size() == bones.size()) {
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
    ERR_FAIL_INDEX_V(p_array_index, RS::ARRAY_MAX, 0);
    uint32_t offsets[RS::ARRAY_MAX];
    uint32_t strides[RS::ARRAY_MAX];
    mesh_surface_make_offsets_from_format(p_format, p_vertex_len, p_index_len, offsets, strides);
    return offsets[p_array_index];
}

uint32_t RenderingServer::mesh_surface_get_format_stride(
        uint32_t p_format, int p_vertex_len, int p_index_len, int p_array_index) const {
    ERR_FAIL_INDEX_V(p_array_index, RS::ARRAY_MAX, 0);
    uint32_t offsets[RS::ARRAY_MAX];
    uint32_t strides[RS::ARRAY_MAX];
    mesh_surface_make_offsets_from_format(p_format, p_vertex_len, p_index_len, offsets, strides);
    return strides[p_array_index];
}

void RenderingServer::mesh_surface_make_offsets_from_format(
        uint32_t p_format, int p_vertex_len, int p_index_len, uint32_t *r_offsets, uint32_t *r_strides) const {
    const bool use_split_stream =
            GLOBAL_GET("rendering/misc/mesh_storage/split_stream") && !(p_format & RS::ARRAY_FLAG_USE_DYNAMIC_UPDATE);

    int attributes_base_offset = 0;
    int attributes_stride = 0;
    int positions_stride = 0;

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
                r_offsets[i] = 0;
                positions_stride = elem_size;
                if (use_split_stream) {
                    attributes_base_offset = elem_size * p_vertex_len;
                } else {
                    attributes_base_offset = elem_size;
                }

            } break;
            case RS::ARRAY_NORMAL: {
                if (p_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
                    // normal will always be oct32 (4 byte) encoded
                    // UNLESS tangent exists and is also compressed
                    // then it will be oct16 encoded along with tangent
                    if ((p_format & RS::ARRAY_COMPRESS_NORMAL) && (p_format & RS::ARRAY_FORMAT_TANGENT) &&
                            (p_format & RS::ARRAY_COMPRESS_TANGENT)) {
                        elem_size = sizeof(uint8_t) * 2;
                    } else {
                        elem_size = sizeof(uint16_t) * 2;
                    }
                } else {
                if (p_format & RS::ARRAY_COMPRESS_NORMAL) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 3;
                }
                }
                r_offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;

            } break;

            case RS::ARRAY_TANGENT: {
                if (p_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
                    if (p_format & RS::ARRAY_COMPRESS_TANGENT && (p_format & RS::ARRAY_FORMAT_NORMAL) &&
                            (p_format & RS::ARRAY_COMPRESS_NORMAL)) {
                        elem_size = sizeof(uint8_t) * 2;
                    } else {
                        elem_size = sizeof(uint16_t) * 2;
                    }
                } else {
                if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 4;
                }
                }
                r_offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;

            } break;
            case RS::ARRAY_COLOR: {
                if (p_format & RS::ARRAY_COMPRESS_COLOR) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 4;
                }
                r_offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;
            } break;
            case RS::ARRAY_TEX_UV: {
                if (p_format & RS::ARRAY_COMPRESS_TEX_UV) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 2;
                }

                r_offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;
            } break;

            case RS::ARRAY_TEX_UV2: {
                if (p_format & RS::ARRAY_COMPRESS_TEX_UV2) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 2;
                }

                r_offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;
            } break;
            case RS::ARRAY_WEIGHTS: {
                if (p_format & RS::ARRAY_COMPRESS_WEIGHTS) {
                    elem_size = sizeof(uint16_t) * 4;
                } else {
                    elem_size = sizeof(float) * 4;
                }

                r_offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;
            } break;
            case RS::ARRAY_BONES: {
                if (p_format & RS::ARRAY_FLAG_USE_16_BIT_BONES) {
                    elem_size = sizeof(uint16_t) * 4;
                } else {
                    elem_size = sizeof(uint32_t);
                }

                r_offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;
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
                ERR_FAIL();
            }
        }

    }
    if (use_split_stream) {
        r_strides[RS::ARRAY_VERTEX] = positions_stride;
        for (int i = 1; i < RS::ARRAY_MAX - 1; i++) {
            r_strides[i] = attributes_stride;
        }
    } else {
        for (int i = 0; i < RS::ARRAY_MAX - 1; i++) {
            r_strides[i] = positions_stride + attributes_stride;
        }
    }
}

void RenderingServer::mesh_add_surface_from_arrays(RenderingEntity p_mesh, RS::PrimitiveType p_primitive,
        const SurfaceArrays &p_arrays, Vector<SurfaceArrays> &&p_blend_shapes, uint32_t p_compress_format) {
    ERR_FAIL_INDEX(p_primitive, RS::PRIMITIVE_MAX);
    bool use_split_stream = GLOBAL_GET("rendering/misc/mesh_storage/split_stream") &&
                            !(p_compress_format & RS::ARRAY_FLAG_USE_DYNAMIC_UPDATE);

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
    uint32_t strides[RS::ARRAY_MAX];
    memset(offsets, 0, RS::ARRAY_MAX * sizeof(uint32_t));
    memset(strides, 0, RS::ARRAY_MAX * sizeof(uint32_t));

    int attributes_base_offset = 0;
    int attributes_stride = 0;
    int positions_stride = 0;

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
        offsets[RS::ARRAY_VERTEX] = 0;
        positions_stride = elem_size;
        if (use_split_stream) {
            attributes_base_offset = elem_size * array_len;
        } else {
            attributes_base_offset = elem_size;
        }
    }
    if (!p_arrays.m_normals.empty()) {
        if (p_compress_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
            // normal will always be oct32 (4 byte) encoded
            // UNLESS tangent exists and is also compressed
            // then it will be oct16 encoded along with tangent
            if ((p_compress_format & RS::ARRAY_COMPRESS_NORMAL) && (format & RS::ARRAY_FORMAT_TANGENT) &&
                    (p_compress_format & RS::ARRAY_COMPRESS_TANGENT)) {
                elem_size = sizeof(uint8_t) * 2;
            } else {
                elem_size = sizeof(uint16_t) * 2;
            }
        } else {
        if (p_compress_format & RS::ARRAY_COMPRESS_NORMAL) {
            elem_size = sizeof(uint32_t);
        } else {
            elem_size = sizeof(float) * 3;
        }
        }

        offsets[RS::ARRAY_NORMAL] = attributes_base_offset + attributes_stride;
        attributes_stride += elem_size;
    }
    if (!p_arrays.m_tangents.empty()) {
        if (p_compress_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
            if (p_compress_format & RS::ARRAY_COMPRESS_TANGENT && (format & RS::ARRAY_FORMAT_NORMAL) &&
                    (p_compress_format & RS::ARRAY_COMPRESS_NORMAL)) {
                elem_size = sizeof(uint8_t) * 2;
            } else {
                elem_size = sizeof(uint16_t) * 2;
            }
        } else {
        if (p_compress_format & RS::ARRAY_COMPRESS_TANGENT) {
            elem_size = sizeof(uint32_t);
        } else {
            elem_size = sizeof(float) * 4;
        }
        }

        offsets[RS::ARRAY_TANGENT] = attributes_base_offset + attributes_stride;
        attributes_stride += elem_size;
    }
    if (!p_arrays.m_colors.empty()) {
        if (p_compress_format & RS::ARRAY_COMPRESS_COLOR) {
            elem_size = sizeof(uint32_t);
        } else {
            elem_size = sizeof(float) * 4;
        }
        offsets[RS::ARRAY_COLOR] = attributes_base_offset + attributes_stride;
        attributes_stride += elem_size;
    }
    if (!p_arrays.m_uv_1.empty()) {
        if (p_compress_format & RS::ARRAY_COMPRESS_TEX_UV) {
            elem_size = sizeof(uint32_t);
        } else {
            elem_size = sizeof(float) * 2;
        }
        offsets[RS::ARRAY_TEX_UV] = attributes_base_offset + attributes_stride;
        attributes_stride += elem_size;
    }
    if (!p_arrays.m_uv_2.empty()) {
        if (p_compress_format & RS::ARRAY_COMPRESS_TEX_UV2) {
            elem_size = sizeof(uint32_t);
        } else {
            elem_size = sizeof(float) * 2;
        }
        offsets[RS::ARRAY_TEX_UV2] = attributes_base_offset + attributes_stride;
        attributes_stride += elem_size;
    }
    if (!p_arrays.m_weights.empty()) {
        if (p_compress_format & RS::ARRAY_COMPRESS_WEIGHTS) {
            elem_size = sizeof(uint16_t) * 4;
        } else {
            elem_size = sizeof(float) * 4;
        }
        offsets[RS::ARRAY_WEIGHTS] = attributes_base_offset + attributes_stride;
        attributes_stride += elem_size;
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
        offsets[RS::ARRAY_BONES] = attributes_base_offset + attributes_stride;
        attributes_stride += elem_size;
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
        for (const auto &arr : p_blend_shapes) {
            uint32_t bsformat = arr.get_flags();

            ERR_FAIL_COND((bsformat) != (format & (RS::ARRAY_FORMAT_INDEX - 1)));
        }
    }
    if (use_split_stream) {
        strides[RS::ARRAY_VERTEX] = positions_stride;
        for (int i = 1; i < RS::ARRAY_MAX - 1; i++) {
            strides[i] = attributes_stride;
        }
    } else {
        for (int i = 0; i < RS::ARRAY_MAX - 1; i++) {
            strides[i] = positions_stride + attributes_stride;
        }
    }

    uint32_t mask = (1 << RS::ARRAY_MAX) - 1;
    format |= (~mask) & p_compress_format; // make the full format

    int array_size = (positions_stride + attributes_stride) * array_len;

    Vector<uint8_t> vertex_array;
    vertex_array.resize(array_size);

    int index_array_size = offsets[RS::ARRAY_INDEX] * index_array_len;

    Vector<uint8_t> index_array;
    index_array.resize(index_array_size);

    AABB aabb;
    Vector<AABB> bone_aabb;

    Error err = _surface_set_data(
            p_arrays, format, offsets, strides, vertex_array, array_len, index_array, index_array_len, aabb, bone_aabb);
    ERR_FAIL_COND_MSG(err, "Invalid array format for surface.");

    Vector<PoolVector<uint8_t>> blend_shape_data;

    for (const SurfaceArrays &p_blend_shape : p_blend_shapes) {
        Vector<uint8_t> vertex_array_shape;
        vertex_array_shape.resize(array_size);
        Vector<uint8_t> noindex;

        AABB laabb;
        Error err2 = _surface_set_data(p_blend_shape, format & ~RS::ARRAY_FORMAT_INDEX, offsets, strides,
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
    bool use_split_stream =
            GLOBAL_GET("rendering/misc/mesh_storage/split_stream") && !(p_format & RS::ARRAY_FLAG_USE_DYNAMIC_UPDATE);
    uint32_t offsets[RS::ARRAY_MAX];
    uint32_t strides[RS::ARRAY_MAX];

    int attributes_base_offset = 0;
    int attributes_stride = 0;
    int positions_stride = 0;

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

                offsets[i] = 0;
                positions_stride = elem_size;
                if (use_split_stream) {
                    attributes_base_offset = elem_size * p_vertex_len;
                } else {
                    attributes_base_offset = elem_size;
                }
            } break;
            case RS::ARRAY_NORMAL: {
                if (p_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
                    // normal will always be oct32 (4 byte) encoded
                    // UNLESS tangent exists and is also compressed
                    // then it will be oct16 encoded along with tangent
                    if ((p_format & RS::ARRAY_COMPRESS_NORMAL) && (p_format & RS::ARRAY_FORMAT_TANGENT) &&
                            (p_format & RS::ARRAY_COMPRESS_TANGENT)) {
                        elem_size = sizeof(uint8_t) * 2;
                    } else {
                        elem_size = sizeof(uint16_t) * 2;
                    }
                } else {
                if (p_format & RS::ARRAY_COMPRESS_NORMAL) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 3;
                }
                }
                offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;

            } break;

            case RS::ARRAY_TANGENT: {
            if (p_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
                if (p_format & RS::ARRAY_COMPRESS_TANGENT && (p_format & RS::ARRAY_FORMAT_NORMAL) && (p_format & RS::ARRAY_COMPRESS_NORMAL)) {
                    elem_size = sizeof(uint8_t) * 2;
                } else {
                    elem_size = sizeof(uint16_t) * 2;
                }
            } else {
                if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 4;
                }
            }
            offsets[i] = attributes_base_offset + attributes_stride;
            attributes_stride += elem_size;

            } break;
            case RS::ARRAY_COLOR: {
                if (p_format & RS::ARRAY_COMPRESS_COLOR) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 4;
                }
                offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;
            } break;
            case RS::ARRAY_TEX_UV: {
                if (p_format & RS::ARRAY_COMPRESS_TEX_UV) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 2;
                }

                offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;
            } break;

            case RS::ARRAY_TEX_UV2: {
                if (p_format & RS::ARRAY_COMPRESS_TEX_UV2) {
                    elem_size = sizeof(uint32_t);
                } else {
                    elem_size = sizeof(float) * 2;
                }

                offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;
            } break;
            case RS::ARRAY_WEIGHTS: {
                if (p_format & RS::ARRAY_COMPRESS_WEIGHTS) {
                    elem_size = sizeof(uint16_t) * 4;
                } else {
                    elem_size = sizeof(float) * 4;
                }

                offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;
            } break;
            case RS::ARRAY_BONES: {
                if (p_format & RS::ARRAY_FLAG_USE_16_BIT_BONES) {
                    elem_size = sizeof(uint16_t) * 4;
                } else {
                    elem_size = sizeof(uint32_t);
                }

                offsets[i] = attributes_base_offset + attributes_stride;
                attributes_stride += elem_size;
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

    }

    if (use_split_stream) {
        strides[RS::ARRAY_VERTEX] = positions_stride;
        for (int i = 1; i < RS::ARRAY_MAX - 1; i++) {
            strides[i] = attributes_stride;
        }
    } else {
        for (int i = 0; i < RS::ARRAY_MAX - 1; i++) {
            strides[i] = positions_stride + attributes_stride;
        }
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
                            const uint16_t *v = (const uint16_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                            arr_2d.emplace_back(Math::halfptr_to_float(&v[0]), Math::halfptr_to_float(&v[1]));
                        }
                    } else {
                        for (uint32_t j = 0; j < p_vertex_len; j++) {
                            const float *v = (const float *)&p_vertex_data[j * strides[i] + offsets[i]];
                            arr_2d.emplace_back(v[0], v[1]);
                        }
                    }

                    ret.set_positions(eastl::move(arr_2d));
                } else {
                    Vector<Vector3> arr_3d;
                    arr_3d.reserve(p_vertex_len);

                    if (p_format & RS::ARRAY_COMPRESS_VERTEX) {
                        for (uint32_t j = 0; j < p_vertex_len; j++) {
                            const uint16_t *v = (const uint16_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                            arr_3d.emplace_back(Math::halfptr_to_float(&v[0]), Math::halfptr_to_float(&v[1]),
                                    Math::halfptr_to_float(&v[2]));
                        }
                    } else {
                        for (uint32_t j = 0; j < p_vertex_len; j++) {
                            const float *v = (const float *)&p_vertex_data[j * strides[i] + offsets[i]];
                            arr_3d.emplace_back(v[0], v[1], v[2]);
                        }
                    }

                    ret.set_positions(eastl::move(arr_3d));
                }

            } break;
            case RS::ARRAY_NORMAL: {
                Vector<Vector3> arr;
                arr.reserve(p_vertex_len);

                if (p_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
                    if (p_format & RS::ARRAY_COMPRESS_NORMAL && (p_format & RS::ARRAY_FORMAT_TANGENT) && (p_format & RS::ARRAY_COMPRESS_TANGENT)) {

                        for (int j = 0; j < p_vertex_len; j++) {
                            const int8_t *n = (const int8_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                            Vector2 enc(n[0] / 127.0f, n[1] / 127.0f);

                            arr.push_back(RenderingServer::oct_to_norm(enc));
                        }
                    } else {
                        for (int j = 0; j < p_vertex_len; j++) {
                            const int16_t *n = (const int16_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                            Vector2 enc(n[0] / 32767.0f, n[1] / 32767.0f);

                            arr.push_back(RenderingServer::oct_to_norm(enc));
                        }
                    }
                } else {
                if (p_format & RS::ARRAY_COMPRESS_NORMAL) {
                        constexpr float multiplier = 1.f / 127.f;

                        for (int j = 0; j < p_vertex_len; j++) {
                            const int8_t *v = (const int8_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                            arr.push_back(Vector3(float(v[0]) * multiplier, float(v[1]) * multiplier, float(v[2]) * multiplier));
                    }
                } else {
                        for (int j = 0; j < p_vertex_len; j++) {
                            const float *v = (const float *)&p_vertex_data[j * strides[i] + offsets[i]];
                            arr.push_back(Vector3(v[0], v[1], v[2]));
                        }
                    }
                }

                ret.m_normals = eastl::move(arr);

            } break;

            case RS::ARRAY_TANGENT: {
                Vector<float> arr;
                arr.reserve(p_vertex_len * 4);
                if (p_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
                if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                    for (int j = 0; j < p_vertex_len; j++) {
                            const int8_t *t = (const int8_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                            Vector2 enc(t[0] / 127.0f, t[1] / 127.0f);
                            float last;
                            Vector3 dec = RenderingServer::oct_to_tangent(enc, &last);

                            arr.emplace_back(dec.x);
                            arr.emplace_back(dec.y);
                            arr.emplace_back(dec.z);
                            arr.emplace_back(last);
                        }
                    } else {
                        for (int j = 0; j < p_vertex_len; j++) {
                            const int16_t *t = (const int16_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                            Vector2 enc(t[0] / 32767.0f, t[1] / 32767.0f);
                            float last;
                            Vector3 dec = RenderingServer::oct_to_tangent(enc, &last);

                            arr.emplace_back(dec.x);
                            arr.emplace_back(dec.y);
                            arr.emplace_back(dec.z);
                            arr.emplace_back(last);
                        }
                    }
                } else {
                if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const int8_t *v = (const int8_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                        for (int k = 0; k < 4; k++) {
                            arr.emplace_back(float(v[k] / 127.0));
                        }
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                            const float *v = (const float *)&p_vertex_data[j * strides[i] + offsets[i]];
                            arr.insert(arr.end(),v,v+4);
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
                        const uint8_t *v = (const uint8_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                        arr.emplace_back(
                                float(v[0] / 255.0f), float(v[1] / 255.0f), float(v[2] / 255.0f), float(v[3] / 255.0f));
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const float *v = (const float *)&p_vertex_data[j * strides[i] + offsets[i]];
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
                        const uint16_t *v = (const uint16_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                        arr.emplace_back(Math::halfptr_to_float(&v[0]), Math::halfptr_to_float(&v[1]));
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const float *v = (const float *)&p_vertex_data[j * strides[i] + offsets[i]];
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
                        const uint16_t *v = (const uint16_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                        arr.emplace_back(Math::halfptr_to_float(&v[0]), Math::halfptr_to_float(&v[1]));
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const float *v = (const float *)&p_vertex_data[j * strides[i] + offsets[i]];
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
                        const uint16_t *v = (const uint16_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                        for (int k = 0; k < 4; k++) {
                            arr.emplace_back(float(v[k]) / 65535.0f);
                        }
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const float *v = (const float *)&p_vertex_data[j * strides[i] + offsets[i]];
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
                        const uint16_t *v = (const uint16_t *)&p_vertex_data[j * strides[i] + offsets[i]];
                        arr.insert(arr.end(), v, v + 4);
                    }
                } else {
                    for (int j = 0; j < p_vertex_len; j++) {
                        const uint8_t *v = (const uint8_t *)&p_vertex_data[j * strides[i] + offsets[i]];
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

SurfaceArrays RenderingServer::mesh_surface_get_arrays(RenderingEntity p_mesh, int p_surface) const {
    PoolVector<uint8_t> vertex_data = mesh_surface_get_array(p_mesh, p_surface);
    ERR_FAIL_COND_V(vertex_data.empty(), SurfaceArrays());
    const int vertex_len = mesh_surface_get_array_len(p_mesh, p_surface);

    PoolVector<uint8_t> index_data = mesh_surface_get_index_array(p_mesh, p_surface);
    const int index_len = mesh_surface_get_array_index_len(p_mesh, p_surface);

    uint32_t format = mesh_surface_get_format(p_mesh, p_surface);

    return _get_array_from_surface(format, vertex_data.toSpan(), vertex_len, index_data.toSpan(), index_len);
}
Array RenderingServer::_mesh_surface_get_arrays(RenderingEntity p_mesh, int p_surface) const {
    return (Array)mesh_surface_get_arrays(p_mesh, p_surface);
}
void RenderingServer::_mesh_add_surface_from_arrays(RenderingEntity p_mesh, RS::PrimitiveType p_primitive,
        const Array &p_arrays, const Array &p_blend_shapes, uint32_t p_compress_format) {
    ERR_FAIL_COND(p_arrays.size() != RS::ARRAY_MAX);
    Vector<SurfaceArrays> blend_shapes;
    blend_shapes.reserve(p_blend_shapes.size());
    for (int i = 0; i < p_blend_shapes.size(); ++i) {
        blend_shapes.emplace_back(SurfaceArrays::fromArray(p_blend_shapes[i].as<Array>()));
    }
    mesh_add_surface_from_arrays(
            p_mesh, p_primitive, SurfaceArrays::fromArray(p_arrays), eastl::move(blend_shapes), p_compress_format);
}
Array RenderingServer::_mesh_surface_get_blend_shape_arrays(RenderingEntity p_mesh, int p_surface) const {
    Array res;
    Vector<SurfaceArrays> from = mesh_surface_get_blend_shape_arrays(p_mesh, p_surface);
    res.resize(from.size());
    int idx = 0;
    for (const SurfaceArrays &s : from) {
        res[idx++] = (Array)s;
    }
    return res;
}

void RenderingServer::sync_thread() {
    ((RenderingServerWrapMT *)queueing_thread_singleton)->sync();
}

Vector<SurfaceArrays> RenderingServer::mesh_surface_get_blend_shape_arrays(
        RenderingEntity p_mesh, int p_surface) const {
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

Array RenderingServer::_mesh_surface_get_skeleton_aabb_bind(RenderingEntity p_mesh, int p_surface) const {
    const Vector<AABB> &vec = RenderingServer::get_singleton()->mesh_surface_get_skeleton_aabb(p_mesh, p_surface);
    Array arr;
    for (int i = 0; i < vec.size(); i++) {
        arr[i] = vec[i];
    }
    return arr;
}
// WRAP : MethodBinder::bind_method\(D_METHOD\("(\w+)"\), \&(\w+)::_(\w+)\)
// BIND_METHOD_WRAPPER($2, $1, _$3)
// MethodBinder::bind_method\s*\(\s*D_METHOD\("(\w+)".*\)\s*,\s*\&(\w+)::\1[\s\n]*,[\s\n]*\{(.+)\}\)
// BIND_METHOD_DEFAULTS\(\2, \1,\3\)

void RenderingServer::_bind_methods() {
    SE_BIND_METHOD(RenderingServer, force_sync);
    SE_BIND_METHOD_WRAPPER_DEFAULTS(RenderingServer, force_draw, draw, DEFVAL(true), DEFVAL(0.0) );

    SE_BIND_METHOD(RenderingServer, texture_create);
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, texture_create_from_image, DEFVAL(RS::TEXTURE_FLAGS_DEFAULT));
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, texture_allocate, DEFVAL(RS::TEXTURE_FLAGS_DEFAULT) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, texture_set_data, DEFVAL(0) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, texture_set_data_partial, DEFVAL(0) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, texture_get_data, DEFVAL(RS::CUBEMAP_LEFT) );
    SE_BIND_METHOD(RenderingServer, texture_set_flags);
    SE_BIND_METHOD(RenderingServer, texture_get_flags);
    SE_BIND_METHOD(RenderingServer, texture_get_format);
    SE_BIND_METHOD(RenderingServer, texture_get_type);
    SE_BIND_METHOD(RenderingServer, texture_get_texid);
    SE_BIND_METHOD(RenderingServer, texture_get_width);
    SE_BIND_METHOD(RenderingServer, texture_get_height);
    SE_BIND_METHOD(RenderingServer, texture_get_depth);
    SE_BIND_METHOD(RenderingServer, texture_set_size_override);
    SE_BIND_METHOD(RenderingServer, texture_set_path);
    SE_BIND_METHOD(RenderingServer, texture_get_path);
    SE_BIND_METHOD(RenderingServer, texture_set_shrink_all_x2_on_set_data);
    SE_BIND_METHOD(RenderingServer, texture_set_proxy);
    SE_BIND_METHOD(RenderingServer, texture_bind);

    SE_BIND_METHOD_WRAPPER(RenderingServer, texture_debug_usage, _texture_debug_usage_bind);
    SE_BIND_METHOD(RenderingServer, textures_keep_original);
#ifndef _3D_DISABLED
    SE_BIND_METHOD(RenderingServer, sky_create);
    SE_BIND_METHOD(RenderingServer, sky_set_texture);
#endif
    SE_BIND_METHOD(RenderingServer, shader_create);
    SE_BIND_METHOD(RenderingServer, shader_set_code);
    SE_BIND_METHOD(RenderingServer, shader_get_code);
    MethodBinder::bind_method(D_METHOD("shader_get_param_list", { "shader" }), &RenderingServer::_shader_get_param_list_bind);
    SE_BIND_METHOD(RenderingServer, shader_set_default_texture_param);
    SE_BIND_METHOD(RenderingServer, shader_get_default_texture_param);
    SE_BIND_METHOD(RenderingServer, set_shader_async_hidden_forbidden);

    SE_BIND_METHOD(RenderingServer, material_create);
    SE_BIND_METHOD(RenderingServer, material_set_shader);
    SE_BIND_METHOD(RenderingServer, material_get_shader);
    SE_BIND_METHOD(RenderingServer, material_set_param);
    SE_BIND_METHOD(RenderingServer, material_get_param);
    SE_BIND_METHOD(RenderingServer, material_get_param_default);
    SE_BIND_METHOD(RenderingServer, material_set_render_priority);
    SE_BIND_METHOD(RenderingServer, material_set_line_width);
    SE_BIND_METHOD(RenderingServer, material_set_next_pass);

    SE_BIND_METHOD(RenderingServer, mesh_create);
    SE_BIND_METHOD(RenderingServer, mesh_surface_get_format_offset);
    SE_BIND_METHOD(RenderingServer, mesh_surface_get_format_stride);
    SE_BIND_METHOD_WRAPPER_DEFAULTS(RenderingServer, mesh_add_surface_from_arrays, _mesh_add_surface_from_arrays, DEFVAL(Array()), DEFVAL(RS::ARRAY_COMPRESS_DEFAULT) );
    SE_BIND_METHOD(RenderingServer, mesh_set_blend_shape_count);
    SE_BIND_METHOD(RenderingServer, mesh_get_blend_shape_count);
    SE_BIND_METHOD(RenderingServer, mesh_set_blend_shape_mode);
    SE_BIND_METHOD(RenderingServer, mesh_get_blend_shape_mode);
    SE_BIND_METHOD(RenderingServer, mesh_surface_update_region);
    SE_BIND_METHOD(RenderingServer, mesh_surface_set_material);
    SE_BIND_METHOD(RenderingServer, mesh_surface_get_material);
    SE_BIND_METHOD(RenderingServer, mesh_surface_get_array_len);
    SE_BIND_METHOD(RenderingServer, mesh_surface_get_array_index_len);
    SE_BIND_METHOD(RenderingServer, mesh_surface_get_array);
    SE_BIND_METHOD(RenderingServer, mesh_surface_get_index_array);
    SE_BIND_METHOD_WRAPPER(RenderingServer, mesh_surface_get_arrays, _mesh_surface_get_arrays);
    SE_BIND_METHOD_WRAPPER(RenderingServer, mesh_surface_get_blend_shape_arrays, _mesh_surface_get_blend_shape_arrays);
    SE_BIND_METHOD(RenderingServer, mesh_surface_get_format);
    SE_BIND_METHOD(RenderingServer, mesh_surface_get_primitive_type);
    SE_BIND_METHOD(RenderingServer, mesh_surface_get_aabb);
    SE_BIND_METHOD_WRAPPER(RenderingServer, mesh_surface_get_skeleton_aabb, _mesh_surface_get_skeleton_aabb_bind);
    SE_BIND_METHOD(RenderingServer, mesh_remove_surface);
    SE_BIND_METHOD(RenderingServer, mesh_get_surface_count);
    SE_BIND_METHOD(RenderingServer, mesh_set_custom_aabb);
    SE_BIND_METHOD(RenderingServer, mesh_get_custom_aabb);
    SE_BIND_METHOD(RenderingServer, mesh_clear);

    SE_BIND_METHOD(RenderingServer, multimesh_create);
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, multimesh_allocate, DEFVAL(RS::MULTIMESH_CUSTOM_DATA_NONE) );
    SE_BIND_METHOD(RenderingServer, multimesh_get_instance_count);
    SE_BIND_METHOD(RenderingServer, multimesh_set_mesh);
    SE_BIND_METHOD(RenderingServer, multimesh_instance_set_transform);
    SE_BIND_METHOD(RenderingServer, multimesh_instance_set_transform_2d);
    SE_BIND_METHOD(RenderingServer, multimesh_instance_set_color);
    SE_BIND_METHOD(RenderingServer, multimesh_instance_set_custom_data);
    SE_BIND_METHOD(RenderingServer, multimesh_get_mesh);
    SE_BIND_METHOD(RenderingServer, multimesh_get_aabb);
    SE_BIND_METHOD(RenderingServer, multimesh_instance_get_transform);
    SE_BIND_METHOD(RenderingServer, multimesh_instance_get_transform_2d);
    SE_BIND_METHOD(RenderingServer, multimesh_instance_get_color);
    SE_BIND_METHOD(RenderingServer, multimesh_instance_get_custom_data);
    SE_BIND_METHOD(RenderingServer, multimesh_set_visible_instances);
    SE_BIND_METHOD(RenderingServer, multimesh_get_visible_instances);
    SE_BIND_METHOD(RenderingServer, multimesh_set_as_bulk_array);
#ifndef _3D_DISABLED
    SE_BIND_METHOD(RenderingServer, immediate_create);
    SE_BIND_METHOD(RenderingServer, immediate_begin);
    SE_BIND_METHOD(RenderingServer, immediate_vertex);
    SE_BIND_METHOD(RenderingServer, immediate_vertex_2d);
    SE_BIND_METHOD(RenderingServer, immediate_normal);
    SE_BIND_METHOD(RenderingServer, immediate_tangent);
    SE_BIND_METHOD(RenderingServer, immediate_color);
    SE_BIND_METHOD(RenderingServer, immediate_uv);
    SE_BIND_METHOD(RenderingServer, immediate_uv2);
    SE_BIND_METHOD(RenderingServer, immediate_end);
    SE_BIND_METHOD(RenderingServer, immediate_clear);
    SE_BIND_METHOD(RenderingServer, immediate_set_material);
    SE_BIND_METHOD(RenderingServer, immediate_get_material);
#endif

    SE_BIND_METHOD(RenderingServer, skeleton_create);
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, skeleton_allocate, DEFVAL(false) );
    SE_BIND_METHOD(RenderingServer, skeleton_get_bone_count);
    SE_BIND_METHOD(RenderingServer, skeleton_bone_set_transform);
    SE_BIND_METHOD(RenderingServer, skeleton_bone_get_transform);
    SE_BIND_METHOD(RenderingServer, skeleton_bone_set_transform_2d);
    SE_BIND_METHOD(RenderingServer, skeleton_bone_get_transform_2d);

#ifndef _3D_DISABLED
    SE_BIND_METHOD(RenderingServer, directional_light_create);
    SE_BIND_METHOD(RenderingServer, omni_light_create);
    SE_BIND_METHOD(RenderingServer, spot_light_create);

    SE_BIND_METHOD(RenderingServer, light_set_color);
    SE_BIND_METHOD(RenderingServer, light_set_param);
    SE_BIND_METHOD(RenderingServer, light_set_shadow);
    SE_BIND_METHOD(RenderingServer, light_set_shadow_color);
    SE_BIND_METHOD(RenderingServer, light_set_projector);
    SE_BIND_METHOD(RenderingServer, light_set_negative);
    SE_BIND_METHOD(RenderingServer, light_set_cull_mask);
    SE_BIND_METHOD(RenderingServer, light_set_reverse_cull_face_mode);
    SE_BIND_METHOD(RenderingServer, light_set_use_gi);
    SE_BIND_METHOD(RenderingServer, light_set_bake_mode);

    SE_BIND_METHOD(RenderingServer, light_omni_set_shadow_mode);
    SE_BIND_METHOD(RenderingServer, light_omni_set_shadow_detail);

    SE_BIND_METHOD(RenderingServer, light_directional_set_shadow_mode);
    SE_BIND_METHOD(RenderingServer, light_directional_set_blend_splits);
    SE_BIND_METHOD(RenderingServer, light_directional_set_shadow_depth_range_mode);

    SE_BIND_METHOD(RenderingServer, reflection_probe_create);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_update_mode);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_intensity);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_interior_ambient);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_interior_ambient_energy);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_interior_ambient_probe_contribution);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_max_distance);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_extents);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_origin_offset);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_as_interior);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_enable_box_projection);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_enable_shadows);
    SE_BIND_METHOD(RenderingServer, reflection_probe_set_cull_mask);

    SE_BIND_METHOD(RenderingServer, gi_probe_create);
    SE_BIND_METHOD(RenderingServer, gi_probe_set_bounds);
    SE_BIND_METHOD(RenderingServer, gi_probe_get_bounds);
    SE_BIND_METHOD(RenderingServer, gi_probe_set_cell_size);
    SE_BIND_METHOD(RenderingServer, gi_probe_get_cell_size);
    SE_BIND_METHOD(RenderingServer, gi_probe_set_to_cell_xform);
    SE_BIND_METHOD(RenderingServer, gi_probe_get_to_cell_xform);
    SE_BIND_METHOD(RenderingServer, gi_probe_set_dynamic_data);
    SE_BIND_METHOD(RenderingServer, gi_probe_get_dynamic_data);
    SE_BIND_METHOD(RenderingServer, gi_probe_set_dynamic_range);
    SE_BIND_METHOD(RenderingServer, gi_probe_get_dynamic_range);
    SE_BIND_METHOD(RenderingServer, gi_probe_set_energy);
    SE_BIND_METHOD(RenderingServer, gi_probe_get_energy);
    SE_BIND_METHOD(RenderingServer, gi_probe_set_bias);
    SE_BIND_METHOD(RenderingServer, gi_probe_get_bias);
    SE_BIND_METHOD(RenderingServer, gi_probe_set_normal_bias);
    SE_BIND_METHOD(RenderingServer, gi_probe_get_normal_bias);
    SE_BIND_METHOD(RenderingServer, gi_probe_set_propagation);
    SE_BIND_METHOD(RenderingServer, gi_probe_get_propagation);
    SE_BIND_METHOD(RenderingServer, gi_probe_set_interior);
    SE_BIND_METHOD(RenderingServer, gi_probe_is_interior);

    SE_BIND_METHOD(RenderingServer, lightmap_capture_create);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_set_bounds);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_get_bounds);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_set_octree);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_set_octree_cell_transform);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_get_octree_cell_transform);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_set_octree_cell_subdiv);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_get_octree_cell_subdiv);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_get_octree);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_set_energy);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_get_energy);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_set_interior);
    SE_BIND_METHOD(RenderingServer, lightmap_capture_is_interior);
#endif
    SE_BIND_METHOD(RenderingServer, particles_create);
    SE_BIND_METHOD(RenderingServer, particles_set_emitting);
    SE_BIND_METHOD(RenderingServer, particles_get_emitting);
    SE_BIND_METHOD(RenderingServer, particles_set_amount);
    SE_BIND_METHOD(RenderingServer, particles_set_lifetime);
    SE_BIND_METHOD(RenderingServer, particles_set_one_shot);
    SE_BIND_METHOD(RenderingServer, particles_set_pre_process_time);
    SE_BIND_METHOD(RenderingServer, particles_set_explosiveness_ratio);
    SE_BIND_METHOD(RenderingServer, particles_set_randomness_ratio);
    SE_BIND_METHOD(RenderingServer, particles_set_custom_aabb);
    SE_BIND_METHOD(RenderingServer, particles_set_speed_scale);
    SE_BIND_METHOD(RenderingServer, particles_set_use_local_coordinates);
    SE_BIND_METHOD(RenderingServer, particles_set_process_material);
    SE_BIND_METHOD(RenderingServer, particles_set_fixed_fps);
    SE_BIND_METHOD(RenderingServer, particles_set_fractional_delta);
    SE_BIND_METHOD(RenderingServer, particles_is_inactive);
    SE_BIND_METHOD(RenderingServer, particles_request_process);
    SE_BIND_METHOD(RenderingServer, particles_restart);
    SE_BIND_METHOD(RenderingServer, particles_set_draw_order);
    SE_BIND_METHOD(RenderingServer, particles_set_draw_passes);
    SE_BIND_METHOD(RenderingServer, particles_set_draw_pass_mesh);
    SE_BIND_METHOD(RenderingServer, particles_get_current_aabb);
    SE_BIND_METHOD(RenderingServer, particles_set_emission_transform);

    SE_BIND_METHOD(RenderingServer, camera_create);
    SE_BIND_METHOD(RenderingServer, camera_set_perspective);
    SE_BIND_METHOD(RenderingServer, camera_set_orthogonal);
    SE_BIND_METHOD(RenderingServer, camera_set_frustum);
    SE_BIND_METHOD(RenderingServer, camera_set_transform);
    SE_BIND_METHOD(RenderingServer, camera_set_cull_mask);
    SE_BIND_METHOD(RenderingServer, camera_set_environment);
    SE_BIND_METHOD(RenderingServer, camera_set_use_vertical_aspect);

    SE_BIND_METHOD(RenderingServer, viewport_create);
    SE_BIND_METHOD(RenderingServer, viewport_set_use_arvr);
    SE_BIND_METHOD(RenderingServer, viewport_set_size);
    SE_BIND_METHOD(RenderingServer, viewport_set_active);
    SE_BIND_METHOD(RenderingServer, viewport_set_parent_viewport);
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, viewport_attach_to_screen, DEFVAL(Rect2()), DEFVAL(0) );
    SE_BIND_METHOD(RenderingServer, viewport_detach);
    SE_BIND_METHOD(RenderingServer, viewport_set_update_mode);
    SE_BIND_METHOD(RenderingServer, viewport_set_vflip);
    SE_BIND_METHOD(RenderingServer, viewport_set_clear_mode);
    SE_BIND_METHOD(RenderingServer, viewport_get_texture);
    SE_BIND_METHOD(RenderingServer, viewport_set_hide_scenario);
    SE_BIND_METHOD(RenderingServer, viewport_set_hide_canvas);
    SE_BIND_METHOD(RenderingServer, viewport_set_disable_environment);
    SE_BIND_METHOD(RenderingServer, viewport_set_disable_3d);
    SE_BIND_METHOD(RenderingServer, viewport_attach_camera);
    SE_BIND_METHOD(RenderingServer, viewport_set_scenario);
    SE_BIND_METHOD(RenderingServer, viewport_attach_canvas);
    SE_BIND_METHOD(RenderingServer, viewport_remove_canvas);
    SE_BIND_METHOD(RenderingServer, viewport_set_canvas_transform);
    SE_BIND_METHOD(RenderingServer, viewport_set_transparent_background);
    SE_BIND_METHOD(RenderingServer, viewport_set_global_canvas_transform);
    SE_BIND_METHOD(RenderingServer, viewport_set_canvas_stacking);
    SE_BIND_METHOD(RenderingServer, viewport_set_shadow_atlas_size);
    SE_BIND_METHOD(RenderingServer, viewport_set_shadow_atlas_quadrant_subdivision);
    SE_BIND_METHOD(RenderingServer, viewport_set_msaa);
    SE_BIND_METHOD(RenderingServer, viewport_set_use_fxaa);
    SE_BIND_METHOD(RenderingServer, viewport_set_use_debanding);

    SE_BIND_METHOD(RenderingServer, viewport_set_hdr);
    SE_BIND_METHOD(RenderingServer, viewport_set_use_32_bpc_depth);
    SE_BIND_METHOD(RenderingServer, viewport_set_usage);
    SE_BIND_METHOD(RenderingServer, viewport_get_render_info);
    SE_BIND_METHOD(RenderingServer, viewport_set_debug_draw);

    SE_BIND_METHOD(RenderingServer, environment_create);
    SE_BIND_METHOD(RenderingServer, environment_set_background);
    SE_BIND_METHOD(RenderingServer, environment_set_sky);
    SE_BIND_METHOD(RenderingServer, environment_set_sky_custom_fov);
    SE_BIND_METHOD(RenderingServer, environment_set_sky_orientation);
    SE_BIND_METHOD(RenderingServer, environment_set_bg_color);
    SE_BIND_METHOD(RenderingServer, environment_set_bg_energy);
    SE_BIND_METHOD(RenderingServer, environment_set_canvas_max_layer);
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, environment_set_ambient_light, DEFVAL(1.0), DEFVAL(0.0) );
    SE_BIND_METHOD(RenderingServer, environment_set_dof_blur_near);
    SE_BIND_METHOD(RenderingServer, environment_set_dof_blur_far);
    SE_BIND_METHOD(RenderingServer, environment_set_glow);
    SE_BIND_METHOD(RenderingServer, environment_set_tonemap);
    SE_BIND_METHOD(RenderingServer, environment_set_adjustment);
    SE_BIND_METHOD(RenderingServer, environment_set_ssr);
    SE_BIND_METHOD(RenderingServer, environment_set_ssao);
    SE_BIND_METHOD(RenderingServer, environment_set_fog);
    SE_BIND_METHOD(RenderingServer, environment_set_fog_depth);
    SE_BIND_METHOD(RenderingServer, environment_set_fog_height);

    SE_BIND_METHOD(RenderingServer, scenario_create);
    SE_BIND_METHOD(RenderingServer, scenario_set_debug);
    SE_BIND_METHOD(RenderingServer, scenario_set_environment);
    SE_BIND_METHOD(RenderingServer, scenario_set_reflection_atlas_size);
    SE_BIND_METHOD(RenderingServer, scenario_set_fallback_environment);

#ifndef _3D_DISABLED

    SE_BIND_METHOD(RenderingServer, instance_create2);
    SE_BIND_METHOD(RenderingServer, instance_create);
    SE_BIND_METHOD(RenderingServer, instance_set_base);
    SE_BIND_METHOD(RenderingServer, instance_set_scenario);
    SE_BIND_METHOD(RenderingServer, instance_set_layer_mask);
    SE_BIND_METHOD(RenderingServer, instance_set_transform);
    SE_BIND_METHOD(RenderingServer, instance_attach_object_instance_id);
    SE_BIND_METHOD(RenderingServer, instance_set_blend_shape_weight);
    SE_BIND_METHOD(RenderingServer, instance_set_surface_material);
    SE_BIND_METHOD(RenderingServer, instance_set_visible);
    SE_BIND_METHOD(RenderingServer, instance_set_use_lightmap);
    SE_BIND_METHOD(RenderingServer, instance_set_custom_aabb);
    SE_BIND_METHOD(RenderingServer, instance_attach_skeleton);
    SE_BIND_METHOD(RenderingServer, instance_set_extra_visibility_margin);
    SE_BIND_METHOD(RenderingServer, instance_geometry_set_flag);
    SE_BIND_METHOD(RenderingServer, instance_geometry_set_cast_shadows_setting);
    SE_BIND_METHOD(RenderingServer, instance_geometry_set_material_override);
    SE_BIND_METHOD(RenderingServer, instance_geometry_set_material_overlay);
    SE_BIND_METHOD(RenderingServer, instance_geometry_set_draw_range);
    SE_BIND_METHOD(RenderingServer, instance_geometry_set_as_instance_lod);
    SE_BIND_METHOD_WRAPPER(RenderingServer, instances_cull_aabb, _instances_cull_aabb_bind);
    SE_BIND_METHOD_WRAPPER(RenderingServer, instances_cull_ray, _instances_cull_ray_bind);
    SE_BIND_METHOD_WRAPPER(RenderingServer, instances_cull_convex, _instances_cull_convex_bind);
#endif
    SE_BIND_METHOD(RenderingServer, canvas_create);
    SE_BIND_METHOD(RenderingServer, canvas_set_item_mirroring);
    SE_BIND_METHOD(RenderingServer, canvas_set_modulate);

    SE_BIND_METHOD(RenderingServer, canvas_item_create);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_parent);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_visible);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_light_mask);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_transform);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_clip);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_distance_field_mode);
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_set_custom_rect, DEFVAL(Rect2()) );
    SE_BIND_METHOD(RenderingServer, canvas_item_set_modulate);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_self_modulate);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_draw_behind_parent);
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_add_line, DEFVAL(1.0), DEFVAL(false) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_add_polyline, DEFVAL(1.0), DEFVAL(false) );
    SE_BIND_METHOD(RenderingServer, canvas_item_add_rect);
    SE_BIND_METHOD(RenderingServer, canvas_item_add_circle);
    /* TODO: those were removed since we don't handle entt::null default arguments on the c# side
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_add_texture_rect, DEFVAL(false), DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(RenderingEntity(entt::null)) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_add_texture_rect_region, DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(RenderingEntity(entt::null)), DEFVAL(true) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_add_nine_patch, DEFVAL(RS::NINE_PATCH_STRETCH), DEFVAL(RS::NINE_PATCH_STRETCH), DEFVAL(true), DEFVAL(Color(1, 1, 1)), DEFVAL(RenderingEntity(entt::null)) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_add_primitive, DEFVAL(1.0), DEFVAL(RenderingEntity(entt::null)) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_add_polygon, DEFVAL(Vector<Point2>()), DEFVAL(RenderingEntity(entt::null)), DEFVAL(RenderingEntity(entt::null)), DEFVAL(false) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_add_triangle_array, DEFVAL(Vector<Point2>()), DEFVAL(Vector<int>()), DEFVAL(Vector<float>()), DEFVAL(RenderingEntity(entt::null)), DEFVAL(-1), DEFVAL(RenderingEntity(entt::null)), DEFVAL(false), DEFVAL(false) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_add_mesh, DEFVAL(Transform2D()), DEFVAL(Color(1, 1, 1)), DEFVAL(RenderingEntity(entt::null)), DEFVAL(RenderingEntity(entt::null)) );
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, canvas_item_add_multimesh, DEFVAL(RenderingEntity(entt::null)) );
    */
    SE_BIND_METHOD(RenderingServer, canvas_item_add_particles);
    SE_BIND_METHOD(RenderingServer, canvas_item_add_set_transform);
    SE_BIND_METHOD(RenderingServer, canvas_item_add_clip_ignore);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_sort_children_by_y);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_z_index);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_z_as_relative_to_parent);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_copy_to_backbuffer);
    SE_BIND_METHOD(RenderingServer, canvas_item_clear);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_draw_index);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_material);
    SE_BIND_METHOD(RenderingServer, canvas_item_set_use_parent_material);
    SE_BIND_METHOD(RenderingServer, canvas_light_create);
    SE_BIND_METHOD(RenderingServer, canvas_light_attach_to_canvas);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_enabled);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_scale);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_transform);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_texture);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_texture_offset);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_color);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_height);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_energy);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_z_range);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_layer_range);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_item_cull_mask);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_item_shadow_cull_mask);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_mode);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_shadow_enabled);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_shadow_buffer_size);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_shadow_gradient_length);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_shadow_filter);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_shadow_color);
    SE_BIND_METHOD(RenderingServer, canvas_light_set_shadow_smooth);

    SE_BIND_METHOD(RenderingServer, canvas_light_occluder_create);
    SE_BIND_METHOD(RenderingServer, canvas_light_occluder_attach_to_canvas);
    SE_BIND_METHOD(RenderingServer, canvas_light_occluder_set_enabled);
    SE_BIND_METHOD(RenderingServer, canvas_light_occluder_set_polygon);
    SE_BIND_METHOD(RenderingServer, canvas_light_occluder_set_transform);
    SE_BIND_METHOD(RenderingServer, canvas_light_occluder_set_light_mask);

    SE_BIND_METHOD(RenderingServer, canvas_occluder_polygon_create);
    SE_BIND_METHOD(RenderingServer, canvas_occluder_polygon_set_shape);
    SE_BIND_METHOD(RenderingServer, canvas_occluder_polygon_set_shape_as_lines);
    SE_BIND_METHOD(RenderingServer, canvas_occluder_polygon_set_cull_mode);

    SE_BIND_METHOD(RenderingServer, black_bars_set_margins);
    SE_BIND_METHOD(RenderingServer, black_bars_set_images);

    SE_BIND_METHOD(RenderingServer, free_rid); // shouldn't conflict with Object::free()

    SE_BIND_METHOD(RenderingServer, request_frame_drawn_callback);
    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, has_changed, DEFVAL(RS::CHANGED_PRIORITY_ANY) );
    SE_BIND_METHOD(RenderingServer, init);
    SE_BIND_METHOD(RenderingServer, finish);
    SE_BIND_METHOD(RenderingServer, get_render_info);
    //    BIND_METHOD(RenderingServer, get_video_adapter_name);
    //	BIND_METHOD(RenderingServer, get_video_adapter_vendor);

#ifndef _3D_DISABLED

    SE_BIND_METHOD(RenderingServer, make_sphere_mesh);
#endif

    SE_BIND_METHOD_WITH_DEFAULTS(RenderingServer, set_boot_image, DEFVAL(true) );
    SE_BIND_METHOD(RenderingServer, set_default_clear_color);

    SE_BIND_METHOD(RenderingServer, has_feature);
    SE_BIND_METHOD(RenderingServer, has_os_feature);
    SE_BIND_METHOD(RenderingServer, set_debug_generate_wireframes);
    SE_BIND_METHOD(RenderingServer, set_use_occlusion_culling);

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
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION);
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
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_INDIRECT_ENERGY);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, LIGHT_PARAM_SIZE);
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
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_2D_ITEMS_IN_FRAME);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, VIEWPORT_RENDER_INFO_2D_DRAW_CALLS_IN_FRAME);
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
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, INFO_SHADER_COMPILES_IN_FRAME);
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
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_TONE_MAPPER_ACES_FITTED);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_QUALITY_LOW);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_QUALITY_MEDIUM);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_QUALITY_HIGH);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_BLUR_DISABLED);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_BLUR_1x1);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_BLUR_2x2);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, ENV_SSAO_BLUR_3x3);

    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CHANGED_PRIORITY_ANY);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CHANGED_PRIORITY_LOW);
    BIND_NS_ENUM_CONSTANT(RenderingServerEnums, CHANGED_PRIORITY_HIGH);

    ADD_SIGNAL(MethodInfo("frame_pre_draw"));
    ADD_SIGNAL(MethodInfo("frame_post_draw"));
}

void RenderingServer::_canvas_item_add_style_box(RenderingEntity p_item, const Rect2 &p_rect, const Rect2 &p_source,
        RenderingEntity p_texture, const Vector<float> &p_margins, const Color &p_modulate) {
    ERR_FAIL_COND(p_margins.size() != 4);
    // canvas_item_add_style_box(p_item,p_rect,p_source,p_texture,Vector2(p_margins[0],p_margins[1]),Vector2(p_margins[2],p_margins[3]),true,p_modulate);
}

void RenderingServer::_camera_set_orthogonal(RenderingEntity p_camera, float p_size, float p_z_near, float p_z_far) {
    camera_set_orthogonal(p_camera, p_size, p_z_near, p_z_far);
}

void RenderingServer::mesh_add_surface_from_mesh_data(RenderingEntity p_mesh, const Geometry::MeshData &p_mesh_data) {
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

void RenderingServer::mesh_add_surface_from_planes(RenderingEntity p_mesh, const PoolVector<Plane> &p_planes) {
    Geometry::MeshData mdata = Geometry::build_convex_mesh(p_planes);
    mesh_add_surface_from_mesh_data(p_mesh, eastl::move(mdata));
}

void RenderingServer::immediate_vertex_2d(RenderingEntity p_immediate, const Vector2 &p_vertex) {
    immediate_vertex(p_immediate, Vector3(p_vertex.x, p_vertex.y, 0));
}

RenderingEntity RenderingServer::instance_create2(RenderingEntity p_base, RenderingEntity p_scenario) {
    RenderingEntity instance = instance_create();
    instance_set_base(instance, p_base);
    instance_set_scenario(instance, p_scenario);
    instance_set_portal_mode(instance, RS::INSTANCE_PORTAL_MODE_GLOBAL);
    return instance;
}

#ifdef DEBUG_ENABLED
bool RenderingServer::is_force_shader_fallbacks_enabled() const {
    return force_shader_fallbacks;
}

void RenderingServer::set_force_shader_fallbacks_enabled(bool p_enabled) {
    force_shader_fallbacks = p_enabled;
}
#endif

RenderingServer::RenderingServer() {
    // ERR_FAIL_COND();
    auto ps(ProjectSettings::get_singleton());
    GLOBAL_DEF_RST("rendering/vram_compression/import_bptc", false);
    GLOBAL_DEF_RST("rendering/vram_compression/import_s3tc", true);
    GLOBAL_DEF("rendering/misc/lossless_compression/force_png", false);
    GLOBAL_DEF("rendering/misc/lossless_compression/webp_compression_level", 2);
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/misc/lossless_compression/webp_compression_level",
                                                               PropertyInfo(VariantType::INT, "rendering/misc/lossless_compression/webp_compression_level",
                                                                            PropertyHint::Range, "0,9,1"));

    GLOBAL_DEF("rendering/limits/time/time_rollover_secs", 3600);
    ps->set_custom_property_info("rendering/limits/time/time_rollover_secs",
            PropertyInfo(VariantType::FLOAT, "rendering/limits/time/time_rollover_secs", PropertyHint::Range,
                    "0,10000,1,or_greater"));

    GLOBAL_DEF("rendering/quality/directional_shadow/size", 4096);
    ps->set_custom_property_info("rendering/quality/directional_shadow/size",
            PropertyInfo(
                    VariantType::INT, "rendering/quality/directional_shadow/size", PropertyHint::Range, "256,16384,256"));
    GLOBAL_DEF_RST("rendering/quality/shadow_atlas/size", 4096);
    ps->set_custom_property_info("rendering/quality/shadow_atlas/size",
            PropertyInfo(VariantType::INT, "rendering/quality/shadow_atlas/size", PropertyHint::Range, "256,16384,256"));
    GLOBAL_DEF_RST("rendering/quality/shadow_atlas/cubemap_size", 512);
    ps->set_custom_property_info("rendering/quality/shadow_atlas/cubemap_size",
            PropertyInfo(
                    VariantType::INT, "rendering/quality/shadow_atlas/cubemap_size", PropertyHint::Range, "64,16384,64"));
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
    ps->set_custom_property_info("rendering/quality/shadows/filter_mode",
            PropertyInfo(VariantType::INT, "rendering/quality/shadows/filter_mode", PropertyHint::Enum,
                    "Disabled,PCF5,PCF13"));

    GLOBAL_DEF("rendering/quality/reflections/texture_array_reflections", true);
    GLOBAL_DEF("rendering/quality/reflections/high_quality_ggx", true);
    GLOBAL_DEF("rendering/quality/reflections/irradiance_max_size", 128);
    ps->set_custom_property_info("rendering/quality/reflections/irradiance_max_size",
            PropertyInfo(VariantType::INT, "rendering/quality/reflections/irradiance_max_size", PropertyHint::Range,
                    "32,2048"));

    GLOBAL_DEF("rendering/quality/shading/force_vertex_shading", false);
    GLOBAL_DEF("rendering/quality/shading/force_lambert_over_burley", false);
    GLOBAL_DEF("rendering/quality/shading/force_blinn_over_ggx", false);
    GLOBAL_DEF_RST("rendering/misc/mesh_storage/split_stream", false);

    GLOBAL_DEF_RST("rendering/quality/shading/use_physical_light_attenuation", false);

    GLOBAL_DEF("rendering/quality/depth_prepass/enable", true);
    // GLOBAL_DEF("rendering/quality/depth_prepass/disable_for_vendors", "PowerVR,Mali,Adreno,Apple");

    GLOBAL_DEF("rendering/quality/filters/anisotropic_filter_level", 4);
    ps->set_custom_property_info("rendering/quality/filters/anisotropic_filter_level",
            PropertyInfo(VariantType::INT, "rendering/quality/filters/anisotropic_filter_level", PropertyHint::Range,
                    "1,16,1"));
    GLOBAL_DEF("rendering/quality/filters/use_nearest_mipmap_filter", false);

    GLOBAL_DEF("rendering/quality/skinning/software_skinning_fallback", true);
    GLOBAL_DEF("rendering/quality/skinning/force_software_skinning", false);

    GLOBAL_DEF_RST("rendering/2d/options/use_software_skinning", true);
    GLOBAL_DEF_RST("rendering/2d/options/ninepatch_mode", 1);
    ps->set_custom_property_info("rendering/2d/options/ninepatch_mode",
            PropertyInfo(VariantType::INT, "rendering/2d/options/ninepatch_mode", PropertyHint::Enum, "Fixed,Scaling"));
    GLOBAL_DEF_RST("rendering/2d/opengl/batching_send_null", 0);
    ps->set_custom_property_info("rendering/2d/opengl/batching_send_null",
            PropertyInfo(VariantType::INT, "rendering/2d/opengl/batching_send_null", PropertyHint::Enum,
                    "Default (On),Off,On"));
    GLOBAL_DEF_RST("rendering/2d/opengl/batching_stream", 0);
    ps->set_custom_property_info(
            "rendering/2d/opengl/batching_stream", PropertyInfo(VariantType::INT, "rendering/2d/opengl/batching_stream",
                                                           PropertyHint::Enum, "Default (Off),Off,On"));
    GLOBAL_DEF_RST("rendering/2d/opengl/legacy_orphan_buffers", 0);
    ps->set_custom_property_info("rendering/2d/opengl/legacy_orphan_buffers",
            PropertyInfo(VariantType::INT, "rendering/2d/opengl/legacy_orphan_buffers", PropertyHint::Enum,
                    "Default (On),Off,On"));
    GLOBAL_DEF_RST("rendering/2d/opengl/legacy_stream", 0);
    ps->set_custom_property_info(
            "rendering/2d/opengl/legacy_stream", PropertyInfo(VariantType::INT, "rendering/2d/opengl/legacy_stream",
                                                         PropertyHint::Enum, "Default (On),Off,On"));

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

    ps->set_custom_property_info("rendering/batching/parameters/max_join_item_commands",
            PropertyInfo(VariantType::INT, "rendering/batching/parameters/max_join_item_commands", PropertyHint::Range,
                    "0,65535"));
    ps->set_custom_property_info("rendering/batching/parameters/colored_vertex_format_threshold",
            PropertyInfo(VariantType::FLOAT, "rendering/batching/parameters/colored_vertex_format_threshold",
                    PropertyHint::Range, "0.0,1.0,0.01"));
    ps->set_custom_property_info("rendering/batching/parameters/batch_buffer_size",
            PropertyInfo(VariantType::INT, "rendering/batching/parameters/batch_buffer_size", PropertyHint::Range,
                    "1024,65535,1024"));
    ps->set_custom_property_info("rendering/batching/lights/scissor_area_threshold",
            PropertyInfo(VariantType::FLOAT, "rendering/batching/lights/scissor_area_threshold", PropertyHint::Range,
                    "0.0,1.0"));
    ps->set_custom_property_info("rendering/batching/lights/max_join_items",
            PropertyInfo(VariantType::INT, "rendering/batching/lights/max_join_items", PropertyHint::Range, "0,512"));
    ps->set_custom_property_info("rendering/batching/parameters/item_reordering_lookahead",
            PropertyInfo(VariantType::INT, "rendering/batching/parameters/item_reordering_lookahead",
                    PropertyHint::Range, "0,256"));
    ps->set_custom_property_info("rendering/batching/precision/uv_contract_amount",
            PropertyInfo(VariantType::INT, "rendering/batching/precision/uv_contract_amount", PropertyHint::Range,
                    "0,10000"));

    // Portal rendering settings
    GLOBAL_DEF("rendering/portals/pvs/use_simple_pvs", false);
    GLOBAL_DEF("rendering/portals/pvs/pvs_logging", false);
    GLOBAL_DEF("rendering/portals/gameplay/use_signals", true);
    GLOBAL_DEF("rendering/portals/optimize/remove_danglers", true);
    GLOBAL_DEF("rendering/portals/debug/logging", true);
    GLOBAL_DEF("rendering/portals/advanced/flip_imported_portals", false);

    // Occlusion culling
    GLOBAL_DEF("rendering/misc/occlusion_culling/max_active_spheres", 8);
    ps->set_custom_property_info("rendering/misc/occlusion_culling/max_active_spheres", PropertyInfo(VariantType::INT, "rendering/misc/occlusion_culling/max_active_spheres", PropertyHint::Range, "0,64"));
    GLOBAL_DEF("rendering/misc/occlusion_culling/max_active_polygons", 8);
    ps->set_custom_property_info("rendering/misc/occlusion_culling/max_active_polygons", PropertyInfo(VariantType::INT, "rendering/misc/occlusion_culling/max_active_polygons", PropertyHint::Range, "0,64"));
    // Async. compilation and caching
#ifdef DEBUG_ENABLED
//    if (!Engine::get_singleton()->is_editor_hint()) {
        force_shader_fallbacks = T_GLOBAL_GET<bool>("rendering/gles3/shaders/debug_shader_fallbacks");
//    }
#endif
    GLOBAL_DEF("rendering/gles3/shaders/shader_compilation_mode", 0);
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/gles3/shaders/shader_compilation_mode", PropertyInfo(VariantType::INT, "rendering/gles3/shaders/shader_compilation_mode", PropertyHint::Enum, "Synchronous,Asynchronous,Asynchronous + Cache"));
    GLOBAL_DEF("rendering/gles3/shaders/max_simultaneous_compiles", 2);
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/gles3/shaders/max_simultaneous_compiles", PropertyInfo(VariantType::INT, "rendering/gles3/shaders/max_simultaneous_compiles", PropertyHint::Range, "1,8,1"));
    GLOBAL_DEF("rendering/gles3/shaders/log_active_async_compiles_count", false);
    GLOBAL_DEF("rendering/gles3/shaders/shader_cache_size_mb", 512);
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/gles3/shaders/shader_cache_size_mb", PropertyInfo(VariantType::INT, "rendering/gles3/shaders/shader_cache_size_mb", PropertyHint::Range, "128,4096,128"));
}

RenderingServer::~RenderingServer() = default;
