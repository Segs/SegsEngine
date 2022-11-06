/*************************************************************************/
/*  rendering_server.h                                                      */
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

#include "core/image.h"
#include "core/math/bsp_tree.h"
#include "core/math/geometry.h"
#include "core/math/transform_2d.h"
#include "core/object.h"
#include "core/rid.h"
#include "core/string.h"
#include "core/variant.h"
#include "core/os/thread.h"
#include "servers/rendering_server_enums.h"


//SEGS: In the future this is meant to replace passing Surface data in Array
class GODOT_EXPORT SurfaceArrays {
public:
    Vector<float> m_position_data;
    Vector<Vector3> m_normals;
    Vector<float> m_tangents;
    Vector<Color> m_colors;
    Vector<Vector2> m_uv_1;
    Vector<Vector2> m_uv_2;
    Vector<float> m_weights;
    Vector<int> m_bones;
    Vector<int> m_indices;
    bool m_vertices_2d;

    explicit SurfaceArrays(Vector<Vector3> && positions) noexcept :
        m_position_data(eastl::move(positions),eastl::I_LIVE_DANGEROUSLY),
        m_vertices_2d(false)
    {

    }
    explicit SurfaceArrays(Vector<Vector2> && positions) noexcept :
        m_position_data(eastl::move(positions),eastl::I_LIVE_DANGEROUSLY),
        m_vertices_2d(true)
    {

    }
    void set_positions(Vector<Vector2> &&from) {
        m_position_data = Vector<float>(eastl::move(from),eastl::I_LIVE_DANGEROUSLY);
        m_vertices_2d = true;
    }
    void set_positions(Vector<Vector3> &&from) {
        m_position_data = Vector<float>(eastl::move(from),eastl::I_LIVE_DANGEROUSLY);
        m_vertices_2d = false;
    }

    Span<const Vector2> positions2() const {
        ERR_FAIL_COND_V(m_vertices_2d==false,Span<const Vector2>());
        return Span<Vector2>((Vector2 *)m_position_data.data(), m_position_data.size()/2);
    }
    Span<const Vector3> positions3() const {
        ERR_FAIL_COND_V(m_vertices_2d==true,Span<const Vector3>());
        return Span<const Vector3>((Vector3 *)m_position_data.data(), m_position_data.size() / 3);
    }
    Span<Vector3> writeable_positions3() const {
        ERR_FAIL_COND_V(m_vertices_2d==true,Span<Vector3>());
        return Span<Vector3>((Vector3 *)m_position_data.data(), m_position_data.size() / 3);
    }
    explicit operator Array() const {
        Array res;
        res.resize(RS::ARRAY_MAX);
        if(m_vertices_2d)
            res[RS::ARRAY_VERTEX] = Variant::from(positions2());
        else
            res[RS::ARRAY_VERTEX] = Variant::from(positions3());
        res[RS::ARRAY_NORMAL] = m_normals;
        res[RS::ARRAY_TANGENT] = m_tangents;
        res[RS::ARRAY_COLOR] = m_colors;
        res[RS::ARRAY_TEX_UV] = Variant::from(m_uv_1);
        res[RS::ARRAY_TEX_UV2] = Variant::from(m_uv_2);
        res[RS::ARRAY_BONES] = m_bones;
        res[RS::ARRAY_WEIGHTS] = m_weights;
        res[RS::ARRAY_INDEX] = m_indices;
        return res;
    }
    static SurfaceArrays fromArray(Array a) {
        if(a.empty())
            return SurfaceArrays();
        SurfaceArrays res;
        Variant dat=a[RS::ARRAY_VERTEX];
        if(dat.get_type()==VariantType::POOL_VECTOR2_ARRAY)
            res.m_position_data = Vector<float>(eastl::move(a[RS::ARRAY_VERTEX].as<Vector<Vector2>>()),eastl::I_LIVE_DANGEROUSLY);
        else if (dat.get_type()==VariantType::POOL_VECTOR3_ARRAY) {
            res.m_position_data = Vector<float>(eastl::move(a[RS::ARRAY_VERTEX].as<Vector<Vector3>>()),eastl::I_LIVE_DANGEROUSLY);
        }
        res.m_normals = a[RS::ARRAY_NORMAL].as<Vector<Vector3>>();
        res.m_tangents = a[RS::ARRAY_TANGENT].as<Vector<float>>();
        //res[RS::ARRAY_TANGENT] = m_normal_data;
        res.m_colors = a[RS::ARRAY_COLOR].as<Vector<Color>>();
        res.m_uv_1 = a[RS::ARRAY_TEX_UV].as<Vector<Vector2>>();
        res.m_uv_2 = a[RS::ARRAY_TEX_UV2].as<Vector<Vector2>>();
        res.m_bones = a[RS::ARRAY_BONES].as<Vector<int>>();
        res.m_weights = a[RS::ARRAY_WEIGHTS].as<Vector<float>>();
        res.m_indices = a[RS::ARRAY_INDEX].as<Vector<int>>();
        return res;
    }
    bool empty() const { return m_position_data.empty(); }
    bool check_sanity() const {
        auto expected= m_position_data.size();
        if(!m_normals.empty() && m_normals.size() != expected)
            return false;
        if (!m_tangents.empty() && m_tangents.size() != expected)
            return false;
        if (!m_colors.empty() && m_colors.size() != expected)
            return false;
        if (!m_uv_1.empty() && m_uv_1.size() != expected)
            return false;
        if (!m_uv_2.empty() && m_uv_2.size() != expected)
            return false;
        if (!m_weights.empty() && m_weights.size() != expected)
            return false;
        if (!m_bones.empty() && m_bones.size() != expected)
            return false;
        if (!m_indices.empty() && m_indices.size() != expected)
            return false;
        return true;
    }
    uint32_t get_flags() const {
        uint32_t lformat=0;
        if (!m_position_data.empty()) {
            lformat |= RS::ARRAY_FORMAT_VERTEX;
        }
        if (!m_normals.empty()) {
            lformat |= RS::ARRAY_FORMAT_NORMAL;
        }
        if (!m_tangents.empty()) {
            lformat |= RS::ARRAY_FORMAT_TANGENT;
        }
        if (!m_colors.empty()) {
            lformat |= RS::ARRAY_FORMAT_COLOR;
        }
        if (!m_uv_1.empty()) {
            lformat |= RS::ARRAY_FORMAT_TEX_UV;
        }
        if (!m_uv_2.empty()) {
            lformat |= RS::ARRAY_FORMAT_TEX_UV2;
        }
        if (!m_bones.empty()) {
            lformat |= RS::ARRAY_FORMAT_BONES;
        }
        if (!m_weights.empty()) {
            lformat |= RS::ARRAY_FORMAT_WEIGHTS;
        }
        if(!m_indices.empty()) {
            lformat |= RS::ARRAY_FORMAT_INDEX;
        }
        return lformat;
    }
    SurfaceArrays clone() const {
        SurfaceArrays res;
        res.m_position_data = m_position_data;
        res.m_normals = m_normals;
        res.m_tangents = m_tangents;
        res.m_colors = m_colors;
        res.m_uv_1 = m_uv_1;
        res.m_uv_2 = m_uv_2;
        res.m_weights = m_weights;
        res.m_bones = m_bones;
        res.m_indices = m_indices;
        res.m_vertices_2d=m_vertices_2d;
        return res;
    }
    SurfaceArrays() noexcept = default;
    SurfaceArrays(SurfaceArrays &&) noexcept = default;
    SurfaceArrays &operator=(SurfaceArrays &&) noexcept = default;
    // Move only type!
    SurfaceArrays(const SurfaceArrays &) = delete;
    SurfaceArrays & operator=(const SurfaceArrays &) = delete;
};

class RenderingServerCallbacks;
/*
    TODO: SEGS: Add function overrides that take ownership of passed buffers Span<> -> Vector<>&&
*/
class GODOT_EXPORT RenderingServer : public Object {

    GDCLASS(RenderingServer,Object)

#ifdef DEBUG_ENABLED
    bool force_shader_fallbacks = false;
#endif

    void _camera_set_orthogonal(RenderingEntity p_camera, float p_size, float p_z_near, float p_z_far);
    void _canvas_item_add_style_box(RenderingEntity p_item, const Rect2 &p_rect, const Rect2 &p_source, RenderingEntity p_texture, const Vector<float> &p_margins, const Color &p_modulate = Color(1, 1, 1));

protected:

    Error _surface_set_data(const SurfaceArrays &p_arrays, uint32_t p_format, uint32_t *p_offsets, uint32_t *p_stride,
            Vector<uint8_t> &r_vertex_array, int p_vertex_array_len, Vector<uint8_t> &r_index_array,
            int p_index_array_len, AABB &r_aabb, Vector<AABB> &r_bone_aabb);

    static void _bind_methods();
public: // scripting glue helpers

    static Thread::ID server_thread;
    static RenderingServer* submission_thread_singleton; // gpu operation submission object
    static RenderingServer* queueing_thread_singleton; // other threads enqueue operations through this object.


    Array _mesh_surface_get_arrays(RenderingEntity p_mesh, int p_surface) const;
    void _mesh_add_surface_from_arrays(RenderingEntity p_mesh, RS::PrimitiveType p_primitive, const Array &p_arrays, const Array &p_blend_shapes = Array(), uint32_t p_compress_format = RS::ARRAY_COMPRESS_DEFAULT);
    Array _mesh_surface_get_blend_shape_arrays(RenderingEntity p_mesh, int p_surface) const;


    static RenderingServer *get_singleton()
    {
        return (Thread::get_caller_id()==server_thread) ? submission_thread_singleton : queueing_thread_singleton;
    }
    static void sync_thread();
    // TODO: this is here only because MethodBinder::bind_method does not support binding static/free functions
    void force_sync() { sync_thread(); }

    virtual void set_ent_debug_name(RenderingEntity p_texture, StringView p_name) const = 0;


    virtual RenderingEntity texture_create() = 0;
    RenderingEntity texture_create_from_image(const Ref<Image> &p_image, uint32_t p_flags = RS::TEXTURE_FLAGS_DEFAULT); // helper
    virtual void texture_allocate(RenderingEntity p_texture,
            int p_width,
            int p_height,
            int p_depth_3d,
            Image::Format p_format,
            RenderingServerEnums::TextureType p_type,
            uint32_t p_flags = RS::TEXTURE_FLAGS_DEFAULT) = 0;

    virtual void texture_set_data(RenderingEntity p_texture, const Ref<Image> &p_image, int p_layer = 0) = 0;
    virtual void texture_set_data_partial(RenderingEntity p_texture,
            const Ref<Image> &p_image,
            int src_x, int src_y,
            int src_w, int src_h,
            int dst_x, int dst_y,
            int p_dst_mip,
            int p_layer = 0) = 0;

    virtual Ref<Image> texture_get_data(RenderingEntity p_texture, int p_layer = 0) const = 0;
    virtual void texture_set_flags(RenderingEntity p_texture, uint32_t p_flags) = 0;
    virtual uint32_t texture_get_flags(RenderingEntity p_texture) const = 0;
    virtual Image::Format texture_get_format(RenderingEntity p_texture) const = 0;
    virtual RS::TextureType texture_get_type(RenderingEntity p_texture) const = 0;
    virtual uint32_t texture_get_texid(RenderingEntity p_texture) const = 0;
    virtual uint32_t texture_get_width(RenderingEntity p_texture) const = 0;
    virtual uint32_t texture_get_height(RenderingEntity p_texture) const = 0;
    virtual uint32_t texture_get_depth(RenderingEntity p_texture) const = 0;
    virtual void texture_set_size_override(RenderingEntity p_texture, int p_width, int p_height, int p_depth_3d) = 0;
    virtual void texture_bind(RenderingEntity p_texture, uint32_t p_texture_no) = 0;

    virtual void texture_set_path(RenderingEntity p_texture, StringView p_path) = 0;
    virtual const String &texture_get_path(RenderingEntity p_texture) const = 0;

    virtual void texture_set_shrink_all_x2_on_set_data(bool p_enable) = 0;

    using TextureDetectCallback = void (*)(void *);

    virtual void texture_set_detect_3d_callback(RenderingEntity p_texture, TextureDetectCallback p_callback, void *p_userdata) = 0;
    virtual void texture_set_detect_srgb_callback(RenderingEntity p_texture, TextureDetectCallback p_callback, void *p_userdata) = 0;
    virtual void texture_set_detect_normal_callback(RenderingEntity p_texture, TextureDetectCallback p_callback, void *p_userdata) = 0;

    struct TextureInfo {
        RenderingEntity texture;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        Image::Format format;
        int bytes;
        String path;
    };

    virtual void texture_debug_usage(Vector<TextureInfo> *r_info) = 0;
    Array _texture_debug_usage_bind();

    virtual void textures_keep_original(bool p_enable) = 0;

    virtual void texture_set_proxy(RenderingEntity p_proxy, RenderingEntity p_base) = 0;
    virtual void texture_set_force_redraw_if_visible(RenderingEntity p_texture, bool p_enable) = 0;

    /* SKY API */

    virtual RenderingEntity sky_create() = 0;
    virtual void sky_set_texture(RenderingEntity p_sky, RenderingEntity p_cube_map, int p_radiance_size) = 0;

    /* SHADER API */

    virtual RenderingEntity shader_create() = 0;

    virtual void shader_set_code(RenderingEntity p_shader, const String &p_code) = 0;
    virtual String shader_get_code(RenderingEntity p_shader) const = 0;
    virtual void shader_get_param_list(RenderingEntity p_shader, Vector<PropertyInfo> *p_param_list) const = 0;
    Array _shader_get_param_list_bind(RenderingEntity p_shader) const;

    virtual void shader_set_default_texture_param(
            RenderingEntity p_shader, const StringName &p_name, RenderingEntity p_texture) = 0;
    virtual RenderingEntity shader_get_default_texture_param(
            RenderingEntity p_shader, const StringName &p_name) const = 0;

    virtual void shader_add_custom_define(RenderingEntity p_shader, StringView p_define) = 0;
    virtual void shader_get_custom_defines(RenderingEntity p_shader, Vector<StringView> *p_defines) const = 0;
    virtual void shader_remove_custom_define(RenderingEntity p_shader, StringView p_define) = 0;

    virtual void set_shader_async_hidden_forbidden(bool p_forbidden) = 0;
    /* COMMON MATERIAL API */
    virtual RenderingEntity material_create() = 0;

    virtual void material_set_shader(RenderingEntity p_shader_material, RenderingEntity p_shader) = 0;
    virtual RenderingEntity material_get_shader(RenderingEntity p_shader_material) const = 0;

    virtual void material_set_param(RenderingEntity p_material, const StringName &p_param, const Variant &p_value) = 0;
    virtual Variant material_get_param(RenderingEntity p_material, const StringName &p_param) const = 0;
    virtual Variant material_get_param_default(RenderingEntity p_material, const StringName &p_param) const = 0;

    virtual void material_set_render_priority(RenderingEntity p_material, int priority) = 0;

    virtual void material_set_line_width(RenderingEntity p_material, float p_width) = 0;
    virtual void material_set_next_pass(RenderingEntity p_material, RenderingEntity p_next_material) = 0;

    /* MESH API */

    virtual RenderingEntity mesh_create() = 0;

    virtual uint32_t mesh_surface_get_format_offset(
            uint32_t p_format, int p_vertex_len, int p_index_len, int p_array_index) const;
    virtual uint32_t mesh_surface_get_format_stride(
            uint32_t p_format, int p_vertex_len, int p_index_len, int p_array_index) const;
    /// Returns stride
    virtual void mesh_surface_make_offsets_from_format(
            uint32_t p_format, int p_vertex_len, int p_index_len, uint32_t *r_offsets, uint32_t *r_strides) const;
    virtual void mesh_add_surface_from_arrays(RenderingEntity p_mesh, RS::PrimitiveType p_primitive,
            const SurfaceArrays &p_arrays, Vector<SurfaceArrays> &&p_blend_shapes = {},
            uint32_t p_compress_format = RS::ARRAY_COMPRESS_DEFAULT);
    virtual void mesh_add_surface(RenderingEntity p_mesh, uint32_t p_format, RS::PrimitiveType p_primitive,
            const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array,
            int p_index_count, const AABB &p_aabb,
            const Vector<PoolVector<uint8_t>> &p_blend_shapes = Vector<PoolVector<uint8_t>>(),
            const PoolVector<AABB> &p_bone_aabbs = PoolVector<AABB>()) = 0;

    virtual void mesh_set_blend_shape_count(RenderingEntity p_mesh, int p_amount) = 0;
    virtual int mesh_get_blend_shape_count(RenderingEntity p_mesh) const = 0;

    virtual void mesh_set_blend_shape_mode(RenderingEntity p_mesh, RS::BlendShapeMode p_mode) = 0;
    virtual RS::BlendShapeMode mesh_get_blend_shape_mode(RenderingEntity p_mesh) const = 0;

    virtual void mesh_surface_update_region(
            RenderingEntity p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data) = 0;

    virtual void mesh_surface_set_material(RenderingEntity p_mesh, int p_surface, RenderingEntity p_material) = 0;
    virtual RenderingEntity mesh_surface_get_material(RenderingEntity p_mesh, int p_surface) const = 0;

    virtual int mesh_surface_get_array_len(RenderingEntity p_mesh, int p_surface) const = 0;
    virtual int mesh_surface_get_array_index_len(RenderingEntity p_mesh, int p_surface) const = 0;

    virtual PoolVector<uint8_t> mesh_surface_get_array(RenderingEntity p_mesh, int p_surface) const = 0;
    virtual PoolVector<uint8_t> mesh_surface_get_index_array(RenderingEntity p_mesh, int p_surface) const = 0;

    virtual SurfaceArrays mesh_surface_get_arrays(RenderingEntity p_mesh, int p_surface) const;
    virtual Vector<SurfaceArrays> mesh_surface_get_blend_shape_arrays(RenderingEntity p_mesh, int p_surface) const;

    virtual uint32_t mesh_surface_get_format(RenderingEntity p_mesh, int p_surface) const = 0;
    virtual RS::PrimitiveType mesh_surface_get_primitive_type(RenderingEntity p_mesh, int p_surface) const = 0;

    virtual AABB mesh_surface_get_aabb(RenderingEntity p_mesh, int p_surface) const = 0;
    virtual Vector<Vector<uint8_t>> mesh_surface_get_blend_shapes(RenderingEntity p_mesh, int p_surface) const = 0;
    virtual const Vector<AABB> &mesh_surface_get_skeleton_aabb(RenderingEntity p_mesh, int p_surface) const = 0;
    Array _mesh_surface_get_skeleton_aabb_bind(RenderingEntity p_mesh, int p_surface) const;

    virtual void mesh_remove_surface(RenderingEntity p_mesh, int p_index) = 0;
    virtual int mesh_get_surface_count(RenderingEntity p_mesh) const = 0;

    virtual void mesh_set_custom_aabb(RenderingEntity p_mesh, const AABB &p_aabb) = 0;
    virtual AABB mesh_get_custom_aabb(RenderingEntity p_mesh) const = 0;

    virtual void mesh_clear(RenderingEntity p_mesh) = 0;

    /* MULTIMESH API */

    virtual RenderingEntity multimesh_create() = 0;

    virtual void multimesh_allocate(RenderingEntity p_multimesh, int p_instances,
            RS::MultimeshTransformFormat p_transform_format, RS::MultimeshColorFormat p_color_format,
            RS::MultimeshCustomDataFormat p_data_format = RS::MULTIMESH_CUSTOM_DATA_NONE) = 0;
    virtual int multimesh_get_instance_count(RenderingEntity p_multimesh) const = 0;

    virtual void multimesh_set_mesh(RenderingEntity p_multimesh, RenderingEntity p_mesh) = 0;
    virtual void multimesh_instance_set_transform(
            RenderingEntity p_multimesh, int p_index, const Transform &p_transform) = 0;
    virtual void multimesh_instance_set_transform_2d(
            RenderingEntity p_multimesh, int p_index, const Transform2D &p_transform) = 0;
    virtual void multimesh_instance_set_color(RenderingEntity p_multimesh, int p_index, const Color &p_color) = 0;
    virtual void multimesh_instance_set_custom_data(RenderingEntity p_multimesh, int p_index, const Color &p_color) = 0;

    virtual RenderingEntity multimesh_get_mesh(RenderingEntity p_multimesh) const = 0;
    virtual AABB multimesh_get_aabb(RenderingEntity p_multimesh) const = 0;

    virtual Transform multimesh_instance_get_transform(RenderingEntity p_multimesh, int p_index) const = 0;
    virtual Transform2D multimesh_instance_get_transform_2d(RenderingEntity p_multimesh, int p_index) const = 0;
    virtual Color multimesh_instance_get_color(RenderingEntity p_multimesh, int p_index) const = 0;
    virtual Color multimesh_instance_get_custom_data(RenderingEntity p_multimesh, int p_index) const = 0;

    virtual void multimesh_set_as_bulk_array(RenderingEntity p_multimesh, Span<const float> p_array) = 0;

    virtual void multimesh_set_visible_instances(RenderingEntity p_multimesh, int p_visible) = 0;
    virtual int multimesh_get_visible_instances(RenderingEntity p_multimesh) const = 0;

    /* IMMEDIATE API */

    virtual RenderingEntity immediate_create() = 0;
    virtual void immediate_begin(RenderingEntity p_immediate, RS::PrimitiveType p_rimitive, RenderingEntity p_texture) = 0;
    virtual void immediate_vertex(RenderingEntity p_immediate, const Vector3 &p_vertex) = 0;
    virtual void immediate_vertex_2d(RenderingEntity p_immediate, const Vector2 &p_vertex);
    virtual void immediate_normal(RenderingEntity p_immediate, const Vector3 &p_normal) = 0;
    virtual void immediate_tangent(RenderingEntity p_immediate, const Plane &p_tangent) = 0;
    virtual void immediate_color(RenderingEntity p_immediate, const Color &p_color) = 0;
    virtual void immediate_uv(RenderingEntity p_immediate, const Vector2 &tex_uv) = 0;
    virtual void immediate_uv2(RenderingEntity p_immediate, const Vector2 &tex_uv) = 0;
    virtual void immediate_end(RenderingEntity p_immediate) = 0;
    virtual void immediate_clear(RenderingEntity p_immediate) = 0;
    virtual void immediate_set_material(RenderingEntity p_immediate, RenderingEntity p_material) = 0;
    virtual RenderingEntity immediate_get_material(RenderingEntity p_immediate) const = 0;

    /* SKELETON API */

    virtual RenderingEntity skeleton_create() = 0;
    virtual void skeleton_allocate(RenderingEntity p_skeleton, int p_bones, bool p_2d_skeleton = false) = 0;
    virtual int skeleton_get_bone_count(RenderingEntity p_skeleton) const = 0;
    virtual void skeleton_bone_set_transform(RenderingEntity p_skeleton, int p_bone, const Transform &p_transform) = 0;
    virtual Transform skeleton_bone_get_transform(RenderingEntity p_skeleton, int p_bone) const = 0;
    virtual void skeleton_bone_set_transform_2d(
            RenderingEntity p_skeleton, int p_bone, const Transform2D &p_transform) = 0;
    virtual Transform2D skeleton_bone_get_transform_2d(RenderingEntity p_skeleton, int p_bone) const = 0;
    virtual void skeleton_set_base_transform_2d(RenderingEntity p_skeleton, const Transform2D &p_base_transform) = 0;

    /* Light API */

    virtual RenderingEntity directional_light_create() = 0;
    virtual RenderingEntity omni_light_create() = 0;
    virtual RenderingEntity spot_light_create() = 0;

    virtual void light_set_color(RenderingEntity p_light, const Color &p_color) = 0;
    virtual void light_set_param(RenderingEntity p_light, RS::LightParam p_param, float p_value) = 0;
    virtual void light_set_shadow(RenderingEntity p_light, bool p_enabled) = 0;
    virtual void light_set_shadow_color(RenderingEntity p_light, const Color &p_color) = 0;
    virtual void light_set_projector(RenderingEntity p_light, RenderingEntity p_texture) = 0;
    virtual void light_set_negative(RenderingEntity p_light, bool p_enable) = 0;
    virtual void light_set_cull_mask(RenderingEntity p_light, uint32_t p_mask) = 0;
    virtual void light_set_reverse_cull_face_mode(RenderingEntity p_light, bool p_enabled) = 0;
    virtual void light_set_use_gi(RenderingEntity p_light, bool p_enable) = 0;

    // bake mode
    virtual void light_set_bake_mode(RenderingEntity p_light, RS::LightBakeMode p_bake_mode) = 0;

    // omni light
    virtual void light_omni_set_shadow_mode(RenderingEntity p_light, RS::LightOmniShadowMode p_mode) = 0;
    virtual void light_omni_set_shadow_detail(RenderingEntity p_light, RS::LightOmniShadowDetail p_detail) = 0;
    virtual void light_directional_set_shadow_mode(RenderingEntity p_light, RS::LightDirectionalShadowMode p_mode) = 0;
    virtual void light_directional_set_blend_splits(RenderingEntity p_light, bool p_enable) = 0;
    virtual void light_directional_set_shadow_depth_range_mode(
            RenderingEntity p_light, RS::LightDirectionalShadowDepthRangeMode p_range_mode) = 0;

    /* PROBE API */

    virtual RenderingEntity reflection_probe_create() = 0;
    virtual void reflection_probe_set_update_mode(RenderingEntity p_probe, RS::ReflectionProbeUpdateMode p_mode) = 0;
    virtual void reflection_probe_set_intensity(RenderingEntity p_probe, float p_intensity) = 0;
    virtual void reflection_probe_set_interior_ambient(RenderingEntity p_probe, const Color &p_color) = 0;
    virtual void reflection_probe_set_interior_ambient_energy(RenderingEntity p_probe, float p_energy) = 0;
    virtual void reflection_probe_set_interior_ambient_probe_contribution(RenderingEntity p_probe, float p_contrib) = 0;
    virtual void reflection_probe_set_max_distance(RenderingEntity p_probe, float p_distance) = 0;
    virtual void reflection_probe_set_extents(RenderingEntity p_probe, const Vector3 &p_extents) = 0;
    virtual void reflection_probe_set_origin_offset(RenderingEntity p_probe, const Vector3 &p_offset) = 0;
    virtual void reflection_probe_set_as_interior(RenderingEntity p_probe, bool p_enable) = 0;
    virtual void reflection_probe_set_enable_box_projection(RenderingEntity p_probe, bool p_enable) = 0;
    virtual void reflection_probe_set_enable_shadows(RenderingEntity p_probe, bool p_enable) = 0;
    virtual void reflection_probe_set_cull_mask(RenderingEntity p_probe, uint32_t p_layers) = 0;
    virtual void reflection_probe_set_resolution(RenderingEntity p_probe, int p_resolution) = 0;

    /* GI PROBE API */

    virtual RenderingEntity gi_probe_create() = 0;

    virtual void gi_probe_set_bounds(RenderingEntity p_probe, const AABB &p_bounds) = 0;
    virtual AABB gi_probe_get_bounds(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_cell_size(RenderingEntity p_probe, float p_range) = 0;
    virtual float gi_probe_get_cell_size(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_to_cell_xform(RenderingEntity p_probe, const Transform &p_xform) = 0;
    virtual Transform gi_probe_get_to_cell_xform(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_dynamic_data(RenderingEntity p_probe, const PoolVector<int> &p_data) = 0;
    virtual PoolVector<int> gi_probe_get_dynamic_data(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_dynamic_range(RenderingEntity p_probe, int p_range) = 0;
    virtual int gi_probe_get_dynamic_range(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_energy(RenderingEntity p_probe, float p_range) = 0;
    virtual float gi_probe_get_energy(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_bias(RenderingEntity p_probe, float p_range) = 0;
    virtual float gi_probe_get_bias(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_normal_bias(RenderingEntity p_probe, float p_range) = 0;
    virtual float gi_probe_get_normal_bias(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_propagation(RenderingEntity p_probe, float p_range) = 0;
    virtual float gi_probe_get_propagation(RenderingEntity p_probe) const = 0;

    virtual void gi_probe_set_interior(RenderingEntity p_probe, bool p_enable) = 0;
    virtual bool gi_probe_is_interior(RenderingEntity p_probe) const = 0;

    /* LIGHTMAP CAPTURE */

    virtual RenderingEntity lightmap_capture_create() = 0;
    virtual void lightmap_capture_set_bounds(RenderingEntity p_capture, const AABB &p_bounds) = 0;
    virtual AABB lightmap_capture_get_bounds(RenderingEntity p_capture) const = 0;
    virtual void lightmap_capture_set_octree(RenderingEntity p_capture, const PoolVector<uint8_t> &p_octree) = 0;
    virtual void lightmap_capture_set_octree_cell_transform(RenderingEntity p_capture, const Transform &p_xform) = 0;
    virtual Transform lightmap_capture_get_octree_cell_transform(RenderingEntity p_capture) const = 0;
    virtual void lightmap_capture_set_octree_cell_subdiv(RenderingEntity p_capture, int p_subdiv) = 0;
    virtual int lightmap_capture_get_octree_cell_subdiv(RenderingEntity p_capture) const = 0;
    virtual PoolVector<uint8_t> lightmap_capture_get_octree(RenderingEntity p_capture) const = 0;
    virtual void lightmap_capture_set_energy(RenderingEntity p_capture, float p_energy) = 0;
    virtual float lightmap_capture_get_energy(RenderingEntity p_capture) const = 0;
    virtual void lightmap_capture_set_interior(RenderingEntity p_capture, bool p_interior) = 0;
    virtual bool lightmap_capture_is_interior(RenderingEntity p_capture) const = 0;

    /* PARTICLES API */

    virtual RenderingEntity particles_create() = 0;

    virtual void particles_set_emitting(RenderingEntity p_particles, bool p_emitting) = 0;
    virtual bool particles_get_emitting(RenderingEntity p_particles) = 0;
    virtual void particles_set_amount(RenderingEntity p_particles, int p_amount) = 0;
    virtual void particles_set_lifetime(RenderingEntity p_particles, float p_lifetime) = 0;
    virtual void particles_set_one_shot(RenderingEntity p_particles, bool p_one_shot) = 0;
    virtual void particles_set_pre_process_time(RenderingEntity p_particles, float p_time) = 0;
    virtual void particles_set_explosiveness_ratio(RenderingEntity p_particles, float p_ratio) = 0;
    virtual void particles_set_randomness_ratio(RenderingEntity p_particles, float p_ratio) = 0;
    virtual void particles_set_custom_aabb(RenderingEntity p_particles, const AABB &p_aabb) = 0;
    virtual void particles_set_speed_scale(RenderingEntity p_particles, float p_scale) = 0;
    virtual void particles_set_use_local_coordinates(RenderingEntity p_particles, bool p_enable) = 0;
    virtual void particles_set_process_material(RenderingEntity p_particles, RenderingEntity p_material) = 0;
    virtual void particles_set_fixed_fps(RenderingEntity p_particles, int p_fps) = 0;
    virtual void particles_set_fractional_delta(RenderingEntity p_particles, bool p_enable) = 0;
    virtual bool particles_is_inactive(RenderingEntity p_particles) = 0;
    virtual void particles_request_process(RenderingEntity p_particles) = 0;
    virtual void particles_restart(RenderingEntity p_particles) = 0;

    virtual void particles_set_draw_order(RenderingEntity p_particles, RS::ParticlesDrawOrder p_order) = 0;

    virtual void particles_set_draw_passes(RenderingEntity p_particles, int p_count) = 0;
    virtual void particles_set_draw_pass_mesh(RenderingEntity p_particles, int p_pass, RenderingEntity p_mesh) = 0;

    virtual AABB particles_get_current_aabb(RenderingEntity p_particles) = 0;

    virtual void particles_set_emission_transform(RenderingEntity p_particles,
            const Transform &p_transform) = 0; // this is only used for 2D, in 3D it's automatic

    /* CAMERA API */

    virtual RenderingEntity camera_create() = 0;
    virtual void camera_set_perspective(
            RenderingEntity p_camera, float p_fovy_degrees, float p_z_near, float p_z_far) = 0;
    virtual void camera_set_orthogonal(RenderingEntity p_camera, float p_size, float p_z_near, float p_z_far) = 0;
    virtual void camera_set_frustum(
            RenderingEntity p_camera, float p_size, Vector2 p_offset, float p_z_near, float p_z_far) = 0;
    virtual void camera_set_transform(RenderingEntity p_camera, const Transform &p_transform) = 0;
    virtual void camera_set_cull_mask(RenderingEntity p_camera, uint32_t p_layers) = 0;
    virtual void camera_set_environment(RenderingEntity p_camera, RenderingEntity p_env) = 0;
    virtual void camera_set_use_vertical_aspect(RenderingEntity p_camera, bool p_enable) = 0;

    /*
    enum ParticlesCollisionMode {
        PARTICLES_COLLISION_NONE,
        PARTICLES_COLLISION_TEXTURE,
        PARTICLES_COLLISION_CUBEMAP,
    };

    virtual void particles_set_collision(RenderingEntity p_particles,ParticlesCollisionMode p_mode,const Transform&, p_xform,const RenderingEntity p_depth_tex,const RenderingEntity p_normal_tex)=0;
*/
    /* VIEWPORT TARGET API */

    virtual RenderingEntity viewport_create() = 0;

    virtual void viewport_set_use_arvr(RenderingEntity p_viewport, bool p_use_arvr) = 0;
    virtual void viewport_set_size(RenderingEntity p_viewport, int p_width, int p_height) = 0;
    virtual void viewport_set_active(RenderingEntity p_viewport, bool p_active) = 0;
    virtual void viewport_set_parent_viewport(RenderingEntity p_viewport, RenderingEntity p_parent_viewport) = 0;

    virtual void viewport_attach_to_screen(RenderingEntity p_viewport, const Rect2 &p_rect = Rect2(), int p_screen = 0) = 0;
    virtual void viewport_detach(RenderingEntity p_viewport) = 0;

    virtual void viewport_set_update_mode(RenderingEntity p_viewport, RS::ViewportUpdateMode p_mode) = 0;
    virtual void viewport_set_vflip(RenderingEntity p_viewport, bool p_enable) = 0;

    virtual void viewport_set_clear_mode(RenderingEntity p_viewport, RS::ViewportClearMode p_clear_mode) = 0;

    virtual RenderingEntity viewport_get_texture(RenderingEntity p_viewport) const = 0;

    virtual void viewport_set_hide_scenario(RenderingEntity p_viewport, bool p_hide) = 0;
    virtual void viewport_set_hide_canvas(RenderingEntity p_viewport, bool p_hide) = 0;
    virtual void viewport_set_disable_environment(RenderingEntity p_viewport, bool p_disable) = 0;
    virtual void viewport_set_disable_3d(RenderingEntity p_viewport, bool p_disable) = 0;
    virtual void viewport_set_keep_3d_linear(RenderingEntity p_viewport, bool p_disable) = 0;

    virtual void viewport_attach_camera(RenderingEntity p_viewport, RenderingEntity p_camera) = 0;
    virtual void viewport_set_scenario(RenderingEntity p_viewport, RenderingEntity p_scenario) = 0;
    virtual void viewport_attach_canvas(RenderingEntity p_viewport, RenderingEntity p_canvas) = 0;
    virtual void viewport_remove_canvas(RenderingEntity p_viewport, RenderingEntity p_canvas) = 0;
    virtual void viewport_set_canvas_transform(RenderingEntity p_viewport, RenderingEntity p_canvas, const Transform2D &p_offset) = 0;
    virtual void viewport_set_transparent_background(RenderingEntity p_viewport, bool p_enabled) = 0;

    virtual void viewport_set_global_canvas_transform(RenderingEntity p_viewport, const Transform2D &p_transform) = 0;
    virtual void viewport_set_canvas_stacking(RenderingEntity p_viewport, RenderingEntity p_canvas, int p_layer, int p_sublayer) = 0;

    virtual void viewport_set_shadow_atlas_size(RenderingEntity p_viewport, int p_size) = 0;
    virtual void viewport_set_shadow_atlas_quadrant_subdivision(RenderingEntity p_viewport, int p_quadrant, int p_subdiv) = 0;

    virtual void viewport_set_msaa(RenderingEntity p_viewport, RS::ViewportMSAA p_msaa) = 0;
    virtual void viewport_set_use_fxaa(RenderingEntity p_viewport, bool p_fxaa) = 0;
    virtual void viewport_set_use_debanding(RenderingEntity p_viewport, bool p_debanding) = 0;
    virtual void viewport_set_sharpen_intensity(RenderingEntity p_viewport, float p_intensity) = 0;

    virtual void viewport_set_hdr(RenderingEntity p_viewport, bool p_enabled) = 0;
    virtual void viewport_set_use_32_bpc_depth(RenderingEntity p_viewport, bool p_enabled) = 0;
    virtual void viewport_set_usage(RenderingEntity p_viewport, RS::ViewportUsage p_usage) = 0;

    virtual uint64_t viewport_get_render_info(RenderingEntity p_viewport, RS::ViewportRenderInfo p_info) = 0;

    virtual void viewport_set_debug_draw(RenderingEntity p_viewport, RS::ViewportDebugDraw p_draw) = 0;

    /* ENVIRONMENT API */

    virtual RenderingEntity environment_create() = 0;

    virtual void environment_set_background(RenderingEntity p_env, RS::EnvironmentBG p_bg) = 0;
    virtual void environment_set_sky(RenderingEntity p_env, RenderingEntity p_sky) = 0;
    virtual void environment_set_sky_custom_fov(RenderingEntity p_env, float p_scale) = 0;
    virtual void environment_set_sky_orientation(RenderingEntity p_env, const Basis &p_orientation) = 0;
    virtual void environment_set_bg_color(RenderingEntity p_env, const Color &p_color) = 0;
    virtual void environment_set_bg_energy(RenderingEntity p_env, float p_energy) = 0;
    virtual void environment_set_canvas_max_layer(RenderingEntity p_env, int p_max_layer) = 0;
    virtual void environment_set_ambient_light(RenderingEntity p_env, const Color &p_color, float p_energy = 1.0, float p_sky_contribution = 0.0) = 0;
    virtual void environment_set_camera_feed_id(RenderingEntity p_env, int p_camera_feed_id) = 0;

    //set default SSAO options
    //set default SSR options
    //set default SSSSS options

    virtual void environment_set_dof_blur_near(RenderingEntity p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, RS::EnvironmentDOFBlurQuality p_quality) = 0;
    virtual void environment_set_dof_blur_far(RenderingEntity p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, RS::EnvironmentDOFBlurQuality p_quality) = 0;
    virtual void environment_set_glow(RenderingEntity p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, RS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale, bool p_high_quality) = 0;
    virtual void environment_set_tonemap(RenderingEntity p_env, RS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_grey) = 0;
    virtual void environment_set_adjustment(RenderingEntity p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RenderingEntity p_ramp) = 0;
    virtual void environment_set_ssr(RenderingEntity p_env, bool p_enable, int p_max_steps, float p_fade_in, float p_fade_out, float p_depth_tolerance, bool p_roughness) = 0;
    virtual void environment_set_ssao(RenderingEntity p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, RS::EnvironmentSSAOQuality p_quality, RS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) = 0;
    virtual void environment_set_fog(RenderingEntity p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) = 0;
    virtual void environment_set_fog_depth(RenderingEntity p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) = 0;
    virtual void environment_set_fog_height(RenderingEntity p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) = 0;

    /* SCENARIO API */

    virtual RenderingEntity scenario_create() = 0;
    virtual void scenario_set_debug(RenderingEntity p_scenario, RS::ScenarioDebugMode p_debug_mode) = 0;
    virtual void scenario_set_environment(RenderingEntity p_scenario, RenderingEntity p_environment) = 0;
    virtual void scenario_set_reflection_atlas_size(RenderingEntity p_scenario, int p_size, int p_subdiv) = 0;
    virtual void scenario_set_fallback_environment(RenderingEntity p_scenario, RenderingEntity p_environment) = 0;

    /* INSTANCING API */

    virtual RenderingEntity instance_create2(RenderingEntity p_base, RenderingEntity p_scenario);

    //virtual RenderingEntity instance_create(RenderingEntity p_base,RenderingEntity p_scenario)=0;
    virtual RenderingEntity instance_create() = 0;

    virtual void instance_set_base(RenderingEntity p_instance, RenderingEntity p_base) = 0; // from can be mesh, light, poly, area and portal so far.
    virtual void instance_set_scenario(RenderingEntity p_instance, RenderingEntity p_scenario) = 0; // from can be mesh, light, poly, area and portal so far.
    virtual void instance_set_layer_mask(RenderingEntity p_instance, uint32_t p_mask) = 0;
    virtual void instance_set_transform(RenderingEntity p_instance, const Transform &p_transform) = 0;
    virtual void instance_attach_object_instance_id(RenderingEntity p_instance, GameEntity p_id) = 0;
    virtual void instance_set_blend_shape_weight(RenderingEntity p_instance, int p_shape, float p_weight) = 0;
    virtual void instance_set_surface_material(RenderingEntity p_instance, int p_surface, RenderingEntity p_material) = 0;
    virtual void instance_set_visible(RenderingEntity p_instance, bool p_visible) = 0;

	virtual void instance_set_use_lightmap(RenderingEntity p_instance, RenderingEntity p_lightmap_instance, RenderingEntity p_lightmap, int p_lightmap_slice, const Rect2 &p_lightmap_uv_rect) = 0;

    virtual void instance_set_custom_aabb(RenderingEntity p_instance, AABB aabb) = 0;

    virtual void instance_attach_skeleton(RenderingEntity p_instance, RenderingEntity p_skeleton) = 0;

    virtual void instance_set_extra_visibility_margin(RenderingEntity p_instance, real_t p_margin) = 0;

    /* ROOMS AND PORTALS API */
    virtual void instance_set_portal_mode(RenderingEntity p_instance, RS::InstancePortalMode p_mode) = 0;
    /* OCCLUSION API */
    virtual RenderingEntity occluder_instance_create() = 0;
    virtual void occluder_instance_set_scenario(RenderingEntity p_occluder_instance, RenderingEntity p_scenario) = 0;
    virtual void occluder_instance_link_resource(RenderingEntity p_occluder_instance, RenderingEntity p_occluder_resource) = 0;
    virtual void occluder_instance_set_transform(RenderingEntity p_occluder_instance, const Transform &p_xform) = 0;
    virtual void occluder_instance_set_active(RenderingEntity p_occluder_instance, bool p_active) = 0;

    virtual RenderingEntity occluder_resource_create() = 0;
    virtual void occluder_resource_prepare(RenderingEntity p_occluder_resource, RS::OccluderType p_type) = 0;
    virtual void occluder_resource_spheres_update(RenderingEntity p_occluder_resource, const Vector<Plane> &p_spheres) = 0;
    virtual void occluder_resource_mesh_update(RenderingEntity p_occluder_resource, const OccluderMeshData &p_mesh_data) = 0;
 
   virtual void set_use_occlusion_culling(bool p_enable) = 0;
    virtual Geometry::MeshData occlusion_debug_get_current_polys(RenderingEntity p_scenario) const = 0;

    // callbacks are used to send messages back from the visual server to scene tree in thread friendly manner
    virtual void callbacks_register(RenderingServerCallbacks *p_callbacks) = 0;

    // don't use these in a game!
    virtual Vector<GameEntity> instances_cull_aabb(const AABB &p_aabb, RenderingEntity p_scenario) const = 0;
    virtual Vector<GameEntity> instances_cull_ray(const Vector3 &p_from, const Vector3 &p_to, RenderingEntity p_scenario) const = 0;
    virtual Vector<GameEntity> instances_cull_convex(Span<const Plane> p_convex, RenderingEntity p_scenario) const = 0;

    Array _instances_cull_aabb_bind(const AABB &p_aabb, RenderingEntity p_scenario) const;
    Array _instances_cull_ray_bind(const Vector3 &p_from, const Vector3 &p_to, RenderingEntity p_scenario) const;
    Array _instances_cull_convex_bind(const Array &p_convex, RenderingEntity p_scenario) const;

    virtual void instance_geometry_set_flag(RenderingEntity p_instance, RenderingServerEnums::InstanceFlags p_flags, bool p_enabled) = 0;
    virtual void instance_geometry_set_cast_shadows_setting(RenderingEntity p_instance, RS::ShadowCastingSetting p_shadow_casting_setting) = 0;
    virtual void instance_geometry_set_material_override(RenderingEntity p_instance, RenderingEntity p_material) = 0;
    virtual void instance_geometry_set_material_overlay(RenderingEntity p_instance, RenderingEntity p_material) = 0;

    virtual void instance_geometry_set_draw_range(RenderingEntity p_instance, float p_min, float p_max, float p_min_margin, float p_max_margin) = 0;
    virtual void instance_geometry_set_as_instance_lod(RenderingEntity p_instance, RenderingEntity p_as_lod_of_instance) = 0;

    /* CANVAS (2D) */

    virtual RenderingEntity canvas_create() = 0;
    virtual void canvas_set_item_mirroring(RenderingEntity p_canvas, RenderingEntity p_item, const Point2 &p_mirroring) = 0;
    virtual void canvas_set_modulate(RenderingEntity p_canvas, const Color &p_color) = 0;
    virtual void canvas_set_parent(RenderingEntity p_canvas, RenderingEntity p_parent, float p_scale) = 0;

    virtual void canvas_set_disable_scale(bool p_disable) = 0;

    virtual RenderingEntity canvas_item_create() = 0;
    virtual void canvas_item_set_parent(RenderingEntity p_item, RenderingEntity p_parent) = 0;

    virtual void canvas_item_set_visible(RenderingEntity p_item, bool p_visible) = 0;
    virtual void canvas_item_set_light_mask(RenderingEntity p_item, int p_mask) = 0;

    virtual void canvas_item_set_update_when_visible(RenderingEntity p_item, bool p_update) = 0;

    virtual void canvas_item_set_transform(RenderingEntity p_item, const Transform2D &p_transform) = 0;
    virtual void canvas_item_set_clip(RenderingEntity p_item, bool p_clip) = 0;
    virtual void canvas_item_set_distance_field_mode(RenderingEntity p_item, bool p_enable) = 0;
    virtual void canvas_item_set_custom_rect(RenderingEntity p_item, bool p_custom_rect, const Rect2 &p_rect = Rect2()) = 0;
    virtual void canvas_item_set_modulate(RenderingEntity p_item, const Color &p_color) = 0;
    virtual void canvas_item_set_self_modulate(RenderingEntity p_item, const Color &p_color) = 0;

    virtual void canvas_item_set_draw_behind_parent(RenderingEntity p_item, bool p_enable) = 0;

    virtual void canvas_item_add_line(RenderingEntity p_item, const Point2 &p_from, const Point2 &p_to, const Color &p_color, float p_width = 1.0, bool p_antialiased = false) = 0;
    //TODO: SEGS: move p_points using `Vector<Point2> &&`, will need to consider scripting api?
    virtual void canvas_item_add_polyline(RenderingEntity p_item, Span<const Vector2> p_points,
            Span<const Color> p_colors, float p_width = 1.0, bool p_antialiased = false) = 0;
    virtual void canvas_item_add_multiline(RenderingEntity p_item, Span<const Vector2> p_points,
            Span<const Color> p_colors, float p_width = 1.0, bool p_antialiased = false) = 0;
    virtual void canvas_item_add_rect(RenderingEntity p_item, const Rect2 &p_rect, const Color &p_color) = 0;
    virtual void canvas_item_add_circle(
            RenderingEntity p_item, const Point2 &p_pos, float p_radius, const Color &p_color) = 0;
    virtual void canvas_item_add_texture_rect(RenderingEntity p_item, const Rect2 &p_rect, RenderingEntity p_texture,
            bool p_tile = false, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            RenderingEntity p_normal_map = entt::null) = 0;
    virtual void canvas_item_add_texture_rect_region(RenderingEntity p_item, const Rect2 &p_rect,
            RenderingEntity p_texture, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1),
            bool p_transpose = false, RenderingEntity p_normal_map = entt::null, bool p_clip_uv = false) = 0;
    virtual void canvas_item_add_nine_patch(RenderingEntity p_item, const Rect2 &p_rect, const Rect2 &p_source,
            RenderingEntity p_texture, const Vector2 &p_topleft, const Vector2 &p_bottomright,
            RS::NinePatchAxisMode p_x_axis_mode = RS::NINE_PATCH_STRETCH,
            RS::NinePatchAxisMode p_y_axis_mode = RS::NINE_PATCH_STRETCH, bool p_draw_center = true,
            const Color &p_modulate = Color(1, 1, 1), RenderingEntity p_normal_map = entt::null) = 0;
    virtual void canvas_item_add_primitive(RenderingEntity p_item, Span<const Vector2> p_points,
            Span<const Color> p_colors, const PoolVector<Point2> &p_uvs, RenderingEntity p_texture,
            float p_width = 1.0, RenderingEntity p_normal_map = entt::null) = 0;
    virtual void canvas_item_add_polygon(RenderingEntity p_item, Span<const Point2> p_points,
            Span<const Color> p_colors, Span<const Point2> p_uvs = {},
            RenderingEntity p_texture = entt::null, RenderingEntity p_normal_map = entt::null,
            bool p_antialiased = false) = 0;
    virtual void canvas_item_add_triangle_array(RenderingEntity p_item, Span<const int> p_indices,
            Span<const Point2> p_points, Span<const Color> p_colors,
            Span<const Point2> p_uvs = {}, const PoolVector<int> &p_bones = PoolVector<int>(),
            const PoolVector<float> &p_weights = PoolVector<float>(), RenderingEntity p_texture = entt::null,
            int p_count = -1, RenderingEntity p_normal_map = entt::null, bool p_antialiased = false,
            bool p_antialiasing_use_indices = false) = 0;

    virtual void canvas_item_add_mesh(RenderingEntity p_item, RenderingEntity p_mesh, const Transform2D &p_transform = Transform2D(), const Color &p_modulate = Color(1, 1, 1), RenderingEntity p_texture = entt::null, RenderingEntity p_normal_map = entt::null) = 0;
    virtual void canvas_item_add_multimesh(RenderingEntity p_item, RenderingEntity p_mesh, RenderingEntity p_texture = entt::null, RenderingEntity p_normal_map = entt::null) = 0;
    virtual void canvas_item_add_particles(RenderingEntity p_item, RenderingEntity p_particles, RenderingEntity p_texture, RenderingEntity p_normal_map) = 0;
    virtual void canvas_item_add_set_transform(RenderingEntity p_item, const Transform2D &p_transform) = 0;
    virtual void canvas_item_add_clip_ignore(RenderingEntity p_item, bool p_ignore) = 0;
    virtual void canvas_item_set_sort_children_by_y(RenderingEntity p_item, bool p_enable) = 0;
    virtual void canvas_item_set_z_index(RenderingEntity p_item, int p_z) = 0;
    virtual void canvas_item_set_z_as_relative_to_parent(RenderingEntity p_item, bool p_enable) = 0;
    virtual void canvas_item_set_copy_to_backbuffer(RenderingEntity p_item, bool p_enable, const Rect2 &p_rect) = 0;

    virtual void canvas_item_attach_skeleton(RenderingEntity p_item, RenderingEntity p_skeleton) = 0;

    virtual void canvas_item_clear(RenderingEntity p_item) = 0;
    virtual void canvas_item_set_draw_index(RenderingEntity p_item, int p_index) = 0;

    virtual void canvas_item_set_material(RenderingEntity p_item, RenderingEntity p_material) = 0;

    virtual void canvas_item_set_use_parent_material(RenderingEntity p_item, bool p_enable) = 0;

    virtual RenderingEntity canvas_light_create() = 0;
    virtual void canvas_light_attach_to_canvas(RenderingEntity p_light, RenderingEntity p_canvas) = 0;
    virtual void canvas_light_set_enabled(RenderingEntity p_light, bool p_enabled) = 0;
    virtual void canvas_light_set_scale(RenderingEntity p_light, float p_scale) = 0;
    virtual void canvas_light_set_transform(RenderingEntity p_light, const Transform2D &p_transform) = 0;
    virtual void canvas_light_set_texture(RenderingEntity p_light, RenderingEntity p_texture) = 0;
    virtual void canvas_light_set_texture_offset(RenderingEntity p_light, const Vector2 &p_offset) = 0;
    virtual void canvas_light_set_color(RenderingEntity p_light, const Color &p_color) = 0;
    virtual void canvas_light_set_height(RenderingEntity p_light, float p_height) = 0;
    virtual void canvas_light_set_energy(RenderingEntity p_light, float p_energy) = 0;
    virtual void canvas_light_set_z_range(RenderingEntity p_light, int p_min_z, int p_max_z) = 0;
    virtual void canvas_light_set_layer_range(RenderingEntity p_light, int p_min_layer, int p_max_layer) = 0;
    virtual void canvas_light_set_item_cull_mask(RenderingEntity p_light, int p_mask) = 0;
    virtual void canvas_light_set_item_shadow_cull_mask(RenderingEntity p_light, int p_mask) = 0;

    virtual void canvas_light_set_mode(RenderingEntity p_light, RS::CanvasLightMode p_mode) = 0;
    virtual void canvas_light_set_shadow_enabled(RenderingEntity p_light, bool p_enabled) = 0;
    virtual void canvas_light_set_shadow_buffer_size(RenderingEntity p_light, int p_size) = 0;
    virtual void canvas_light_set_shadow_gradient_length(RenderingEntity p_light, float p_length) = 0;
    virtual void canvas_light_set_shadow_filter(RenderingEntity p_light, RS::CanvasLightShadowFilter p_filter) = 0;
    virtual void canvas_light_set_shadow_color(RenderingEntity p_light, const Color &p_color) = 0;
    virtual void canvas_light_set_shadow_smooth(RenderingEntity p_light, float p_smooth) = 0;

    virtual RenderingEntity canvas_light_occluder_create() = 0;
    virtual void canvas_light_occluder_attach_to_canvas(RenderingEntity p_occluder, RenderingEntity p_canvas) = 0;
    virtual void canvas_light_occluder_set_enabled(RenderingEntity p_occluder, bool p_enabled) = 0;
    virtual void canvas_light_occluder_set_polygon(RenderingEntity p_occluder, RenderingEntity p_polygon) = 0;
    virtual void canvas_light_occluder_set_transform(RenderingEntity p_occluder, const Transform2D &p_xform) = 0;
    virtual void canvas_light_occluder_set_light_mask(RenderingEntity p_occluder, int p_mask) = 0;

    virtual RenderingEntity canvas_occluder_polygon_create() = 0;
    virtual void canvas_occluder_polygon_set_shape(RenderingEntity p_occluder_polygon, Span<const Vector2> p_shape, bool p_closed) = 0;
    virtual void canvas_occluder_polygon_set_shape_as_lines(RenderingEntity p_occluder_polygon, Span<const Vector2> p_shape) = 0;

    virtual void canvas_occluder_polygon_set_cull_mode(RenderingEntity p_occluder_polygon, RS::CanvasOccluderPolygonCullMode p_mode) = 0;

    /* BLACK BARS */

    virtual void black_bars_set_margins(int p_left, int p_top, int p_right, int p_bottom) = 0;
    virtual void black_bars_set_images(RenderingEntity p_left, RenderingEntity p_top, RenderingEntity p_right, RenderingEntity p_bottom) = 0;

    /* FREE */

    virtual void free_rid(RenderingEntity p_rid) = 0; ///< free RIDs associated with the visual server

    virtual void request_frame_drawn_callback(Callable &&) = 0;

    /* EVENT QUEUING */

    virtual void draw(bool p_swap_buffers = true, double frame_step = 0.0) = 0;
    virtual bool has_changed(RS::ChangedPriority p_priority = RS::CHANGED_PRIORITY_ANY) const = 0;
    virtual void init() = 0;
    virtual void finish() = 0;
    virtual void tick() = 0;
    virtual void pre_draw(bool p_will_draw) = 0;

    /* STATUS INFORMATION */

    virtual uint64_t get_render_info(RS::RenderInfo p_info) = 0;
    virtual const char *get_video_adapter_name() const = 0;
    virtual const char *get_video_adapter_vendor() const = 0;

    virtual RenderingEntity make_sphere_mesh(int p_lats, int p_lons, float p_radius);

    virtual void mesh_add_surface_from_mesh_data(RenderingEntity p_mesh, const Geometry::MeshData &p_mesh_data);
    virtual void mesh_add_surface_from_planes(RenderingEntity p_mesh, const PoolVector<Plane> &p_planes);

    virtual void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) = 0;
    virtual void set_default_clear_color(const Color &p_color) = 0;
	virtual void set_shader_time_scale(float p_scale) = 0;

    virtual bool has_feature(RS::Features p_feature) const = 0;

    virtual bool has_os_feature(const StringName &p_feature) const = 0;

    virtual void set_debug_generate_wireframes(bool p_generate) = 0;

    virtual void call_set_use_vsync(bool p_enable) = 0;
#ifdef DEBUG_ENABLED
    bool is_force_shader_fallbacks_enabled() const;
    void set_force_shader_fallbacks_enabled(bool p_enabled);
#endif

    RenderingServer();
    ~RenderingServer() override;

    static Vector2 norm_to_oct(const Vector3 v);
    static Vector2 tangent_to_oct(const Vector3 v, const float sign, const bool high_precision);
    static Vector3 oct_to_norm(const Vector2 v);
    static Vector3 oct_to_tangent(const Vector2 v, float *out_sign);
};
