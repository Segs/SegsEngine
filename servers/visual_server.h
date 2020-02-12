/*************************************************************************/
/*  visual_server.h                                                      */
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
#include "core/se_string.h"
#include "core/variant.h"
#include "servers/visual_server_enums.h"

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
    explicit SurfaceArrays(Vector<Vector3> && positions) :
        m_position_data(eastl::move(positions),eastl::I_LIVE_DANGEROUSLY),
        m_vertices_2d(false)
    {

    }
    explicit SurfaceArrays(Vector<Vector2> && positions) :
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
        res.resize(VS::ARRAY_MAX);
        if(m_vertices_2d)
            res[VS::ARRAY_VERTEX] = Variant::from(positions2());
        else
            res[VS::ARRAY_VERTEX] = Variant::from(positions3());
        res[VS::ARRAY_NORMAL] = m_normals;
        res[VS::ARRAY_TANGENT] = m_tangents;
        res[VS::ARRAY_COLOR] = m_colors;
        res[VS::ARRAY_TEX_UV] = Variant::from(m_uv_1);
        res[VS::ARRAY_TEX_UV2] = Variant::from(m_uv_2);
        res[VS::ARRAY_BONES] = m_bones;
        res[VS::ARRAY_WEIGHTS] = m_weights;
        res[VS::ARRAY_INDEX] = m_indices;
        return res;
    }
    static SurfaceArrays fromArray(Array a) {
        if(a.empty())
            return SurfaceArrays();
        SurfaceArrays res;
        Variant dat=a[VS::ARRAY_VERTEX];
        if(dat.get_type()==VariantType::POOL_VECTOR2_ARRAY)
            res.m_position_data = Vector<float>(eastl::move(a[VS::ARRAY_VERTEX].as<Vector<Vector2>>()),eastl::I_LIVE_DANGEROUSLY);
        else if (dat.get_type()==VariantType::POOL_VECTOR3_ARRAY) {
            res.m_position_data = Vector<float>(eastl::move(a[VS::ARRAY_VERTEX].as<Vector<Vector3>>()),eastl::I_LIVE_DANGEROUSLY);
        }
        res.m_normals = a[VS::ARRAY_NORMAL].as<Vector<Vector3>>();
        res.m_tangents = a[VS::ARRAY_TANGENT].as<Vector<float>>();
        //res[VS::ARRAY_TANGENT] = m_normal_data;
        res.m_colors = a[VS::ARRAY_COLOR].as<Vector<Color>>();
        res.m_uv_1 = a[VS::ARRAY_TEX_UV].as<Vector<Vector2>>();
        res.m_uv_2 = a[VS::ARRAY_TEX_UV2].as<Vector<Vector2>>();
        res.m_bones = a[VS::ARRAY_BONES].as<Vector<int>>();
        res.m_weights = a[VS::ARRAY_WEIGHTS].as<Vector<float>>();
        res.m_indices = a[VS::ARRAY_INDEX].as<Vector<int>>();
        return res;
    }
    bool empty() const { return m_position_data.empty(); }
    bool check_sanity() const {
        auto expected= m_position_data.size();
        if(m_normals.size()!=expected && !m_normals.empty())
            return false;
        if (m_tangents.size() != expected && !m_tangents.empty())
            return false;
        if (m_colors.size() != expected && !m_colors.empty())
            return false;
        if (m_uv_1.size() != expected && !m_uv_1.empty())
            return false;
        if (m_uv_2.size() != expected && !m_uv_2.empty())
            return false;
        if (m_weights.size() != expected && !m_weights.empty())
            return false;
        if (m_bones.size() != expected && !m_bones.empty())
            return false;
        if (m_indices.size() != expected && !m_indices.empty())
            return false;
        return true;
    }
    uint32_t get_flags() const {
        uint32_t lformat=0;
        if (!m_position_data.empty()) {
            lformat |= VS::ARRAY_FORMAT_VERTEX;
        }
        if (!m_normals.empty()) {
            lformat |= VS::ARRAY_FORMAT_NORMAL;
        }
        if (!m_tangents.empty()) {
            lformat |= VS::ARRAY_FORMAT_TANGENT;
        }
        if (!m_colors.empty()) {
            lformat |= VS::ARRAY_FORMAT_COLOR;
        }
        if (!m_uv_1.empty()) {
            lformat |= VS::ARRAY_FORMAT_TEX_UV;
        }
        if (!m_uv_2.empty()) {
            lformat |= VS::ARRAY_FORMAT_TEX_UV2;
        }
        if (!m_bones.empty()) {
            lformat |= VS::ARRAY_FORMAT_BONES;
        }
        if (!m_weights.empty()) {
            lformat |= VS::ARRAY_FORMAT_WEIGHTS;
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
    SurfaceArrays(SurfaceArrays &&) = default;
    SurfaceArrays &operator=(SurfaceArrays &&) = default;
    // Move only type!
    SurfaceArrays(const SurfaceArrays &) = delete;
    SurfaceArrays & operator=(const SurfaceArrays &) = delete;
};

/*
    TODO: SEGS: Add function overrides that take ownership of passed buffers Span<> -> Vector<>&&
*/
class VisualServer : public Object {

    GDCLASS(VisualServer,Object)

    static VisualServer *singleton;

    int mm_policy;

    void _camera_set_orthogonal(RID p_camera, float p_size, float p_z_near, float p_z_far);
    void _canvas_item_add_style_box(RID p_item, const Rect2 &p_rect, const Rect2 &p_source, RID p_texture, const Vector<float> &p_margins, const Color &p_modulate = Color(1, 1, 1));
    SurfaceArrays _get_array_from_surface(uint32_t p_format, Span<const uint8_t> p_vertex_data, int p_vertex_len,
            Span<const uint8_t> p_index_data, int p_index_len) const;

protected:
    RID _make_test_cube();
    void _free_internal_rids();
    RID test_texture;
    RID white_texture;
    RID test_material;

    Error _surface_set_data(const SurfaceArrays &p_arrays, uint32_t p_format, uint32_t *p_offsets, uint32_t p_stride, Vector<uint8_t> &r_vertex_array, int p_vertex_array_len, Vector<uint8_t> &r_index_array, int p_index_array_len, AABB &r_aabb, Vector<AABB> &r_bone_aabb);

    static VisualServer *(*create_func)();
    static void _bind_methods();
public: // scripting glue helpers
    Array _mesh_surface_get_arrays(RID p_mesh, int p_surface) const;
    void _mesh_add_surface_from_arrays(RID p_mesh, VS::PrimitiveType p_primitive, const Array &p_arrays, const Array &p_blend_shapes = Array(), uint32_t p_compress_format = VS::ARRAY_COMPRESS_DEFAULT);
    Array _mesh_surface_get_blend_shape_arrays(RID p_mesh, int p_surface) const;

public:
    static VisualServer *get_singleton();
    static VisualServer *create();

    virtual RID texture_create() = 0;
    RID texture_create_from_image(const Ref<Image> &p_image, uint32_t p_flags = VS::TEXTURE_FLAGS_DEFAULT); // helper
    virtual void texture_allocate(RID p_texture,
            int p_width,
            int p_height,
            int p_depth_3d,
            Image::Format p_format,
            VS::TextureType p_type,
            uint32_t p_flags = VS::TEXTURE_FLAGS_DEFAULT) = 0;

    virtual void texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) = 0;
    virtual void texture_set_data_partial(RID p_texture,
            const Ref<Image> &p_image,
            int src_x, int src_y,
            int src_w, int src_h,
            int dst_x, int dst_y,
            int p_dst_mip,
            int p_layer = 0) = 0;

    virtual Ref<Image> texture_get_data(RID p_texture, int p_layer = 0) const = 0;
    virtual void texture_set_flags(RID p_texture, uint32_t p_flags) = 0;
    virtual uint32_t texture_get_flags(RID p_texture) const = 0;
    virtual Image::Format texture_get_format(RID p_texture) const = 0;
    virtual VS::TextureType texture_get_type(RID p_texture) const = 0;
    virtual uint32_t texture_get_texid(RID p_texture) const = 0;
    virtual uint32_t texture_get_width(RID p_texture) const = 0;
    virtual uint32_t texture_get_height(RID p_texture) const = 0;
    virtual uint32_t texture_get_depth(RID p_texture) const = 0;
    virtual void texture_set_size_override(RID p_texture, int p_width, int p_height, int p_depth_3d) = 0;
    virtual void texture_bind(RID p_texture, uint32_t p_texture_no) = 0;

    virtual void texture_set_path(RID p_texture, se_string_view p_path) = 0;
    virtual const String &texture_get_path(RID p_texture) const = 0;

    virtual void texture_set_shrink_all_x2_on_set_data(bool p_enable) = 0;

    using TextureDetectCallback = void (*)(void *);

    virtual void texture_set_detect_3d_callback(RID p_texture, TextureDetectCallback p_callback, void *p_userdata) = 0;
    virtual void texture_set_detect_srgb_callback(RID p_texture, TextureDetectCallback p_callback, void *p_userdata) = 0;
    virtual void texture_set_detect_normal_callback(RID p_texture, TextureDetectCallback p_callback, void *p_userdata) = 0;

    struct TextureInfo {
        RID texture;
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

    virtual void texture_set_proxy(RID p_proxy, RID p_base) = 0;
    virtual void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) = 0;

    /* SKY API */

    virtual RID sky_create() = 0;
    virtual void sky_set_texture(RID p_sky, RID p_cube_map, int p_radiance_size) = 0;

    /* SHADER API */

    virtual RID shader_create() = 0;

    virtual void shader_set_code(RID p_shader, const String &p_code) = 0;
    virtual String shader_get_code(RID p_shader) const = 0;
    virtual void shader_get_param_list(RID p_shader, Vector<PropertyInfo> *p_param_list) const = 0;
    Array _shader_get_param_list_bind(RID p_shader) const;

    virtual void shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture) = 0;
    virtual RID shader_get_default_texture_param(RID p_shader, const StringName &p_name) const = 0;

    /* COMMON MATERIAL API */
    virtual RID material_create() = 0;

    virtual void material_set_shader(RID p_shader_material, RID p_shader) = 0;
    virtual RID material_get_shader(RID p_shader_material) const = 0;

    virtual void material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) = 0;
    virtual Variant material_get_param(RID p_material, const StringName &p_param) const = 0;
    virtual Variant material_get_param_default(RID p_material, const StringName &p_param) const = 0;

    virtual void material_set_render_priority(RID p_material, int priority) = 0;

    virtual void material_set_line_width(RID p_material, float p_width) = 0;
    virtual void material_set_next_pass(RID p_material, RID p_next_material) = 0;

    /* MESH API */

    virtual RID mesh_create() = 0;

    virtual uint32_t mesh_surface_get_format_offset(uint32_t p_format, int p_vertex_len, int p_index_len, int p_array_index) const;
    virtual uint32_t mesh_surface_get_format_stride(uint32_t p_format, int p_vertex_len, int p_index_len) const;
    /// Returns stride
    virtual uint32_t mesh_surface_make_offsets_from_format(uint32_t p_format, int p_vertex_len, int p_index_len, uint32_t *r_offsets) const;
    virtual void mesh_add_surface_from_arrays(RID p_mesh, VS::PrimitiveType p_primitive, const SurfaceArrays &p_arrays, Vector<SurfaceArrays> &&p_blend_shapes = {}, uint32_t p_compress_format = VS::ARRAY_COMPRESS_DEFAULT);
    virtual void mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t> > &p_blend_shapes = Vector<PoolVector<uint8_t> >(), const PoolVector<AABB> &p_bone_aabbs = PoolVector<AABB>()) = 0;

    virtual void mesh_set_blend_shape_count(RID p_mesh, int p_amount) = 0;
    virtual int mesh_get_blend_shape_count(RID p_mesh) const = 0;

    virtual void mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode) = 0;
    virtual VS::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const = 0;

    virtual void mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data) = 0;

    virtual void mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) = 0;
    virtual RID mesh_surface_get_material(RID p_mesh, int p_surface) const = 0;

    virtual int mesh_surface_get_array_len(RID p_mesh, int p_surface) const = 0;
    virtual int mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const = 0;

    virtual PoolVector<uint8_t> mesh_surface_get_array(RID p_mesh, int p_surface) const = 0;
    virtual PoolVector<uint8_t> mesh_surface_get_index_array(RID p_mesh, int p_surface) const = 0;

    virtual SurfaceArrays mesh_surface_get_arrays(RID p_mesh, int p_surface) const;
    virtual Vector<SurfaceArrays> mesh_surface_get_blend_shape_arrays(RID p_mesh, int p_surface) const;

    virtual uint32_t mesh_surface_get_format(RID p_mesh, int p_surface) const = 0;
    virtual VS::PrimitiveType mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const = 0;

    virtual AABB mesh_surface_get_aabb(RID p_mesh, int p_surface) const = 0;
    virtual Vector<Vector<uint8_t>> mesh_surface_get_blend_shapes(RID p_mesh, int p_surface) const = 0;
    virtual const Vector<AABB> &mesh_surface_get_skeleton_aabb(RID p_mesh, int p_surface) const = 0;
    Array _mesh_surface_get_skeleton_aabb_bind(RID p_mesh, int p_surface) const;

    virtual void mesh_remove_surface(RID p_mesh, int p_index) = 0;
    virtual int mesh_get_surface_count(RID p_mesh) const = 0;

    virtual void mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) = 0;
    virtual AABB mesh_get_custom_aabb(RID p_mesh) const = 0;

    virtual void mesh_clear(RID p_mesh) = 0;

    /* MULTIMESH API */

    virtual RID multimesh_create() = 0;

    virtual void multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, VS::MultimeshColorFormat p_color_format, VS::MultimeshCustomDataFormat p_data_format = VS::MULTIMESH_CUSTOM_DATA_NONE) = 0;
    virtual int multimesh_get_instance_count(RID p_multimesh) const = 0;

    virtual void multimesh_set_mesh(RID p_multimesh, RID p_mesh) = 0;
    virtual void multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform) = 0;
    virtual void multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) = 0;
    virtual void multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) = 0;
    virtual void multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) = 0;

    virtual RID multimesh_get_mesh(RID p_multimesh) const = 0;
    virtual AABB multimesh_get_aabb(RID p_multimesh) const = 0;

    virtual Transform multimesh_instance_get_transform(RID p_multimesh, int p_index) const = 0;
    virtual Transform2D multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const = 0;
    virtual Color multimesh_instance_get_color(RID p_multimesh, int p_index) const = 0;
    virtual Color multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const = 0;

    virtual void multimesh_set_as_bulk_array(RID p_multimesh, const PoolVector<float> &p_array) = 0;

    virtual void multimesh_set_visible_instances(RID p_multimesh, int p_visible) = 0;
    virtual int multimesh_get_visible_instances(RID p_multimesh) const = 0;

    /* IMMEDIATE API */

    virtual RID immediate_create() = 0;
    virtual void immediate_begin(RID p_immediate, VS::PrimitiveType p_rimitive, RID p_texture = RID()) = 0;
    virtual void immediate_vertex(RID p_immediate, const Vector3 &p_vertex) = 0;
    virtual void immediate_vertex_2d(RID p_immediate, const Vector2 &p_vertex);
    virtual void immediate_normal(RID p_immediate, const Vector3 &p_normal) = 0;
    virtual void immediate_tangent(RID p_immediate, const Plane &p_tangent) = 0;
    virtual void immediate_color(RID p_immediate, const Color &p_color) = 0;
    virtual void immediate_uv(RID p_immediate, const Vector2 &tex_uv) = 0;
    virtual void immediate_uv2(RID p_immediate, const Vector2 &tex_uv) = 0;
    virtual void immediate_end(RID p_immediate) = 0;
    virtual void immediate_clear(RID p_immediate) = 0;
    virtual void immediate_set_material(RID p_immediate, RID p_material) = 0;
    virtual RID immediate_get_material(RID p_immediate) const = 0;

    /* SKELETON API */

    virtual RID skeleton_create() = 0;
    virtual void skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton = false) = 0;
    virtual int skeleton_get_bone_count(RID p_skeleton) const = 0;
    virtual void skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform) = 0;
    virtual Transform skeleton_bone_get_transform(RID p_skeleton, int p_bone) const = 0;
    virtual void skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) = 0;
    virtual Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const = 0;
    virtual void skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) = 0;

    /* Light API */

    virtual RID directional_light_create() = 0;
    virtual RID omni_light_create() = 0;
    virtual RID spot_light_create() = 0;

    virtual void light_set_color(RID p_light, const Color &p_color) = 0;
    virtual void light_set_param(RID p_light, VS::LightParam p_param, float p_value) = 0;
    virtual void light_set_shadow(RID p_light, bool p_enabled) = 0;
    virtual void light_set_shadow_color(RID p_light, const Color &p_color) = 0;
    virtual void light_set_projector(RID p_light, RID p_texture) = 0;
    virtual void light_set_negative(RID p_light, bool p_enable) = 0;
    virtual void light_set_cull_mask(RID p_light, uint32_t p_mask) = 0;
    virtual void light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) = 0;
    virtual void light_set_use_gi(RID p_light, bool p_enable) = 0;

    // omni light
    virtual void light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode) = 0;
    virtual void light_omni_set_shadow_detail(RID p_light, VS::LightOmniShadowDetail p_detail) = 0;
    virtual void light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode) = 0;
    virtual void light_directional_set_blend_splits(RID p_light, bool p_enable) = 0;
    virtual void light_directional_set_shadow_depth_range_mode(RID p_light, VS::LightDirectionalShadowDepthRangeMode p_range_mode) = 0;

    /* PROBE API */

    virtual RID reflection_probe_create() = 0;
    virtual void reflection_probe_set_update_mode(RID p_probe, VS::ReflectionProbeUpdateMode p_mode) = 0;
    virtual void reflection_probe_set_intensity(RID p_probe, float p_intensity) = 0;
    virtual void reflection_probe_set_interior_ambient(RID p_probe, const Color &p_color) = 0;
    virtual void reflection_probe_set_interior_ambient_energy(RID p_probe, float p_energy) = 0;
    virtual void reflection_probe_set_interior_ambient_probe_contribution(RID p_probe, float p_contrib) = 0;
    virtual void reflection_probe_set_max_distance(RID p_probe, float p_distance) = 0;
    virtual void reflection_probe_set_extents(RID p_probe, const Vector3 &p_extents) = 0;
    virtual void reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) = 0;
    virtual void reflection_probe_set_as_interior(RID p_probe, bool p_enable) = 0;
    virtual void reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) = 0;
    virtual void reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) = 0;
    virtual void reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) = 0;
    virtual void reflection_probe_set_resolution(RID p_probe, int p_resolution) = 0;

    /* GI PROBE API */

    virtual RID gi_probe_create() = 0;

    virtual void gi_probe_set_bounds(RID p_probe, const AABB &p_bounds) = 0;
    virtual AABB gi_probe_get_bounds(RID p_probe) const = 0;

    virtual void gi_probe_set_cell_size(RID p_probe, float p_range) = 0;
    virtual float gi_probe_get_cell_size(RID p_probe) const = 0;

    virtual void gi_probe_set_to_cell_xform(RID p_probe, const Transform &p_xform) = 0;
    virtual Transform gi_probe_get_to_cell_xform(RID p_probe) const = 0;

    virtual void gi_probe_set_dynamic_data(RID p_probe, const PoolVector<int> &p_data) = 0;
    virtual PoolVector<int> gi_probe_get_dynamic_data(RID p_probe) const = 0;

    virtual void gi_probe_set_dynamic_range(RID p_probe, int p_range) = 0;
    virtual int gi_probe_get_dynamic_range(RID p_probe) const = 0;

    virtual void gi_probe_set_energy(RID p_probe, float p_range) = 0;
    virtual float gi_probe_get_energy(RID p_probe) const = 0;

    virtual void gi_probe_set_bias(RID p_probe, float p_range) = 0;
    virtual float gi_probe_get_bias(RID p_probe) const = 0;

    virtual void gi_probe_set_normal_bias(RID p_probe, float p_range) = 0;
    virtual float gi_probe_get_normal_bias(RID p_probe) const = 0;

    virtual void gi_probe_set_propagation(RID p_probe, float p_range) = 0;
    virtual float gi_probe_get_propagation(RID p_probe) const = 0;

    virtual void gi_probe_set_interior(RID p_probe, bool p_enable) = 0;
    virtual bool gi_probe_is_interior(RID p_probe) const = 0;

    virtual void gi_probe_set_compress(RID p_probe, bool p_enable) = 0;
    virtual bool gi_probe_is_compressed(RID p_probe) const = 0;

    /* LIGHTMAP CAPTURE */

    virtual RID lightmap_capture_create() = 0;
    virtual void lightmap_capture_set_bounds(RID p_capture, const AABB &p_bounds) = 0;
    virtual AABB lightmap_capture_get_bounds(RID p_capture) const = 0;
    virtual void lightmap_capture_set_octree(RID p_capture, const PoolVector<uint8_t> &p_octree) = 0;
    virtual void lightmap_capture_set_octree_cell_transform(RID p_capture, const Transform &p_xform) = 0;
    virtual Transform lightmap_capture_get_octree_cell_transform(RID p_capture) const = 0;
    virtual void lightmap_capture_set_octree_cell_subdiv(RID p_capture, int p_subdiv) = 0;
    virtual int lightmap_capture_get_octree_cell_subdiv(RID p_capture) const = 0;
    virtual PoolVector<uint8_t> lightmap_capture_get_octree(RID p_capture) const = 0;
    virtual void lightmap_capture_set_energy(RID p_capture, float p_energy) = 0;
    virtual float lightmap_capture_get_energy(RID p_capture) const = 0;

    /* PARTICLES API */

    virtual RID particles_create() = 0;

    virtual void particles_set_emitting(RID p_particles, bool p_emitting) = 0;
    virtual bool particles_get_emitting(RID p_particles) = 0;
    virtual void particles_set_amount(RID p_particles, int p_amount) = 0;
    virtual void particles_set_lifetime(RID p_particles, float p_lifetime) = 0;
    virtual void particles_set_one_shot(RID p_particles, bool p_one_shot) = 0;
    virtual void particles_set_pre_process_time(RID p_particles, float p_time) = 0;
    virtual void particles_set_explosiveness_ratio(RID p_particles, float p_ratio) = 0;
    virtual void particles_set_randomness_ratio(RID p_particles, float p_ratio) = 0;
    virtual void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) = 0;
    virtual void particles_set_speed_scale(RID p_particles, float p_scale) = 0;
    virtual void particles_set_use_local_coordinates(RID p_particles, bool p_enable) = 0;
    virtual void particles_set_process_material(RID p_particles, RID p_material) = 0;
    virtual void particles_set_fixed_fps(RID p_particles, int p_fps) = 0;
    virtual void particles_set_fractional_delta(RID p_particles, bool p_enable) = 0;
    virtual bool particles_is_inactive(RID p_particles) = 0;
    virtual void particles_request_process(RID p_particles) = 0;
    virtual void particles_restart(RID p_particles) = 0;

    virtual void particles_set_draw_order(RID p_particles, VS::ParticlesDrawOrder p_order) = 0;

    virtual void particles_set_draw_passes(RID p_particles, int p_count) = 0;
    virtual void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) = 0;

    virtual AABB particles_get_current_aabb(RID p_particles) = 0;

    virtual void particles_set_emission_transform(RID p_particles, const Transform &p_transform) = 0; //this is only used for 2D, in 3D it's automatic

    /* CAMERA API */

    virtual RID camera_create() = 0;
    virtual void camera_set_perspective(RID p_camera, float p_fovy_degrees, float p_z_near, float p_z_far) = 0;
    virtual void camera_set_orthogonal(RID p_camera, float p_size, float p_z_near, float p_z_far) = 0;
    virtual void camera_set_frustum(RID p_camera, float p_size, Vector2 p_offset, float p_z_near, float p_z_far) = 0;
    virtual void camera_set_transform(RID p_camera, const Transform &p_transform) = 0;
    virtual void camera_set_cull_mask(RID p_camera, uint32_t p_layers) = 0;
    virtual void camera_set_environment(RID p_camera, RID p_env) = 0;
    virtual void camera_set_use_vertical_aspect(RID p_camera, bool p_enable) = 0;

    /*
    enum ParticlesCollisionMode {
        PARTICLES_COLLISION_NONE,
        PARTICLES_COLLISION_TEXTURE,
        PARTICLES_COLLISION_CUBEMAP,
    };

    virtual void particles_set_collision(RID p_particles,ParticlesCollisionMode p_mode,const Transform&, p_xform,const RID p_depth_tex,const RID p_normal_tex)=0;
*/
    /* VIEWPORT TARGET API */

    virtual RID viewport_create() = 0;

    virtual void viewport_set_use_arvr(RID p_viewport, bool p_use_arvr) = 0;
    virtual void viewport_set_size(RID p_viewport, int p_width, int p_height) = 0;
    virtual void viewport_set_active(RID p_viewport, bool p_active) = 0;
    virtual void viewport_set_parent_viewport(RID p_viewport, RID p_parent_viewport) = 0;

    virtual void viewport_attach_to_screen(RID p_viewport, const Rect2 &p_rect = Rect2(), int p_screen = 0) = 0;
    virtual void viewport_set_render_direct_to_screen(RID p_viewport, bool p_enable) = 0;
    virtual void viewport_detach(RID p_viewport) = 0;

    virtual void viewport_set_update_mode(RID p_viewport, VS::ViewportUpdateMode p_mode) = 0;
    virtual void viewport_set_vflip(RID p_viewport, bool p_enable) = 0;

    virtual void viewport_set_clear_mode(RID p_viewport, VS::ViewportClearMode p_clear_mode) = 0;

    virtual RID viewport_get_texture(RID p_viewport) const = 0;

    virtual void viewport_set_hide_scenario(RID p_viewport, bool p_hide) = 0;
    virtual void viewport_set_hide_canvas(RID p_viewport, bool p_hide) = 0;
    virtual void viewport_set_disable_environment(RID p_viewport, bool p_disable) = 0;
    virtual void viewport_set_disable_3d(RID p_viewport, bool p_disable) = 0;
    virtual void viewport_set_keep_3d_linear(RID p_viewport, bool p_disable) = 0;

    virtual void viewport_attach_camera(RID p_viewport, RID p_camera) = 0;
    virtual void viewport_set_scenario(RID p_viewport, RID p_scenario) = 0;
    virtual void viewport_attach_canvas(RID p_viewport, RID p_canvas) = 0;
    virtual void viewport_remove_canvas(RID p_viewport, RID p_canvas) = 0;
    virtual void viewport_set_canvas_transform(RID p_viewport, RID p_canvas, const Transform2D &p_offset) = 0;
    virtual void viewport_set_transparent_background(RID p_viewport, bool p_enabled) = 0;

    virtual void viewport_set_global_canvas_transform(RID p_viewport, const Transform2D &p_transform) = 0;
    virtual void viewport_set_canvas_stacking(RID p_viewport, RID p_canvas, int p_layer, int p_sublayer) = 0;

    virtual void viewport_set_shadow_atlas_size(RID p_viewport, int p_size) = 0;
    virtual void viewport_set_shadow_atlas_quadrant_subdivision(RID p_viewport, int p_quadrant, int p_subdiv) = 0;

    virtual void viewport_set_msaa(RID p_viewport, VS::ViewportMSAA p_msaa) = 0;

    virtual void viewport_set_hdr(RID p_viewport, bool p_enabled) = 0;
    virtual void viewport_set_usage(RID p_viewport, VS::ViewportUsage p_usage) = 0;

    virtual int viewport_get_render_info(RID p_viewport, VS::ViewportRenderInfo p_info) = 0;

    virtual void viewport_set_debug_draw(RID p_viewport, VS::ViewportDebugDraw p_draw) = 0;

    /* ENVIRONMENT API */

    virtual RID environment_create() = 0;

    virtual void environment_set_background(RID p_env, VS::EnvironmentBG p_bg) = 0;
    virtual void environment_set_sky(RID p_env, RID p_sky) = 0;
    virtual void environment_set_sky_custom_fov(RID p_env, float p_scale) = 0;
    virtual void environment_set_sky_orientation(RID p_env, const Basis &p_orientation) = 0;
    virtual void environment_set_bg_color(RID p_env, const Color &p_color) = 0;
    virtual void environment_set_bg_energy(RID p_env, float p_energy) = 0;
    virtual void environment_set_canvas_max_layer(RID p_env, int p_max_layer) = 0;
    virtual void environment_set_ambient_light(RID p_env, const Color &p_color, float p_energy = 1.0, float p_sky_contribution = 0.0) = 0;
    virtual void environment_set_camera_feed_id(RID p_env, int p_camera_feed_id) = 0;

    //set default SSAO options
    //set default SSR options
    //set default SSSSS options

    virtual void environment_set_dof_blur_near(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) = 0;
    virtual void environment_set_dof_blur_far(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) = 0;
    virtual void environment_set_glow(RID p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, VS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale) = 0;
    virtual void environment_set_tonemap(RID p_env, VS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_grey) = 0;
    virtual void environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RID p_ramp) = 0;
    virtual void environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_in, float p_fade_out, float p_depth_tolerance, bool p_roughness) = 0;
    virtual void environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, VS::EnvironmentSSAOQuality p_quality, VS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) = 0;
    virtual void environment_set_fog(RID p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) = 0;
    virtual void environment_set_fog_depth(RID p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) = 0;
    virtual void environment_set_fog_height(RID p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) = 0;

    /* SCENARIO API */

    virtual RID scenario_create() = 0;
    virtual void scenario_set_debug(RID p_scenario, VS::ScenarioDebugMode p_debug_mode) = 0;
    virtual void scenario_set_environment(RID p_scenario, RID p_environment) = 0;
    virtual void scenario_set_reflection_atlas_size(RID p_scenario, int p_size, int p_subdiv) = 0;
    virtual void scenario_set_fallback_environment(RID p_scenario, RID p_environment) = 0;

    /* INSTANCING API */

    virtual RID instance_create2(RID p_base, RID p_scenario);

    //virtual RID instance_create(RID p_base,RID p_scenario)=0;
    virtual RID instance_create() = 0;

    virtual void instance_set_base(RID p_instance, RID p_base) = 0; // from can be mesh, light, poly, area and portal so far.
    virtual void instance_set_scenario(RID p_instance, RID p_scenario) = 0; // from can be mesh, light, poly, area and portal so far.
    virtual void instance_set_layer_mask(RID p_instance, uint32_t p_mask) = 0;
    virtual void instance_set_transform(RID p_instance, const Transform &p_transform) = 0;
    virtual void instance_attach_object_instance_id(RID p_instance, ObjectID p_id) = 0;
    virtual void instance_set_blend_shape_weight(RID p_instance, int p_shape, float p_weight) = 0;
    virtual void instance_set_surface_material(RID p_instance, int p_surface, RID p_material) = 0;
    virtual void instance_set_visible(RID p_instance, bool p_visible) = 0;

    virtual void instance_set_use_lightmap(RID p_instance, RID p_lightmap_instance, RID p_lightmap) = 0;

    virtual void instance_set_custom_aabb(RID p_instance, AABB aabb) = 0;

    virtual void instance_attach_skeleton(RID p_instance, RID p_skeleton) = 0;
    virtual void instance_set_exterior(RID p_instance, bool p_enabled) = 0;

    virtual void instance_set_extra_visibility_margin(RID p_instance, real_t p_margin) = 0;

    // don't use these in a game!
    virtual Vector<ObjectID> instances_cull_aabb(const AABB &p_aabb, RID p_scenario = RID()) const = 0;
    virtual Vector<ObjectID> instances_cull_ray(const Vector3 &p_from, const Vector3 &p_to, RID p_scenario = RID()) const = 0;
    virtual Vector<ObjectID> instances_cull_convex(Span<const Plane> p_convex, RID p_scenario = RID()) const = 0;

    Array _instances_cull_aabb_bind(const AABB &p_aabb, RID p_scenario = RID()) const;
    Array _instances_cull_ray_bind(const Vector3 &p_from, const Vector3 &p_to, RID p_scenario = RID()) const;
    Array _instances_cull_convex_bind(const Array &p_convex, RID p_scenario = RID()) const;

    virtual void instance_geometry_set_flag(RID p_instance, VS::InstanceFlags p_flags, bool p_enabled) = 0;
    virtual void instance_geometry_set_cast_shadows_setting(RID p_instance, VS::ShadowCastingSetting p_shadow_casting_setting) = 0;
    virtual void instance_geometry_set_material_override(RID p_instance, RID p_material) = 0;

    virtual void instance_geometry_set_draw_range(RID p_instance, float p_min, float p_max, float p_min_margin, float p_max_margin) = 0;
    virtual void instance_geometry_set_as_instance_lod(RID p_instance, RID p_as_lod_of_instance) = 0;

    /* CANVAS (2D) */

    virtual RID canvas_create() = 0;
    virtual void canvas_set_item_mirroring(RID p_canvas, RID p_item, const Point2 &p_mirroring) = 0;
    virtual void canvas_set_modulate(RID p_canvas, const Color &p_color) = 0;
    virtual void canvas_set_parent(RID p_canvas, RID p_parent, float p_scale) = 0;

    virtual void canvas_set_disable_scale(bool p_disable) = 0;

    virtual RID canvas_item_create() = 0;
    virtual void canvas_item_set_parent(RID p_item, RID p_parent) = 0;

    virtual void canvas_item_set_visible(RID p_item, bool p_visible) = 0;
    virtual void canvas_item_set_light_mask(RID p_item, int p_mask) = 0;

    virtual void canvas_item_set_update_when_visible(RID p_item, bool p_update) = 0;

    virtual void canvas_item_set_transform(RID p_item, const Transform2D &p_transform) = 0;
    virtual void canvas_item_set_clip(RID p_item, bool p_clip) = 0;
    virtual void canvas_item_set_distance_field_mode(RID p_item, bool p_enable) = 0;
    virtual void canvas_item_set_custom_rect(RID p_item, bool p_custom_rect, const Rect2 &p_rect = Rect2()) = 0;
    virtual void canvas_item_set_modulate(RID p_item, const Color &p_color) = 0;
    virtual void canvas_item_set_self_modulate(RID p_item, const Color &p_color) = 0;

    virtual void canvas_item_set_draw_behind_parent(RID p_item, bool p_enable) = 0;

    virtual void canvas_item_add_line(RID p_item, const Point2 &p_from, const Point2 &p_to, const Color &p_color, float p_width = 1.0, bool p_antialiased = false) = 0;
    //TODO: SEGS: move p_points using `Vector<Point2> &&`, will need to consider scripting api?
    virtual void canvas_item_add_polyline(RID p_item, const Vector<Vector2> &p_points, const Vector<Color> &p_colors, float p_width = 1.0, bool p_antialiased = false) = 0;
    virtual void canvas_item_add_multiline(RID p_item, const Vector<Vector2> &p_points, const Vector<Color> &p_colors, float p_width = 1.0, bool p_antialiased = false) = 0;
    virtual void canvas_item_add_rect(RID p_item, const Rect2 &p_rect, const Color &p_color) = 0;
    virtual void canvas_item_add_circle(RID p_item, const Point2 &p_pos, float p_radius, const Color &p_color) = 0;
    virtual void canvas_item_add_texture_rect(RID p_item, const Rect2 &p_rect, RID p_texture, bool p_tile = false, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false, RID p_normal_map = RID()) = 0;
    virtual void canvas_item_add_texture_rect_region(RID p_item, const Rect2 &p_rect, RID p_texture, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false, RID p_normal_map = RID(), bool p_clip_uv = false) = 0;
    virtual void canvas_item_add_nine_patch(RID p_item, const Rect2 &p_rect, const Rect2 &p_source, RID p_texture, const Vector2 &p_topleft, const Vector2 &p_bottomright, VS::NinePatchAxisMode p_x_axis_mode = VS::NINE_PATCH_STRETCH, VS::NinePatchAxisMode p_y_axis_mode = VS::NINE_PATCH_STRETCH, bool p_draw_center = true, const Color &p_modulate = Color(1, 1, 1), RID p_normal_map = RID()) = 0;
    virtual void canvas_item_add_primitive(RID p_item, const Vector<Point2> &p_points, const PoolVector<Color> &p_colors, const PoolVector<Point2> &p_uvs, RID p_texture, float p_width = 1.0, RID p_normal_map = RID()) = 0;
    virtual void canvas_item_add_polygon(RID p_item, Span<const Point2> p_points, const PoolVector<Color> &p_colors, const PoolVector<Point2> &p_uvs = PoolVector<Point2>(), RID p_texture = RID(), RID p_normal_map = RID(), bool p_antialiased = false) = 0;
    virtual void canvas_item_add_triangle_array(RID p_item, Span<const int> p_indices, Span<const Point2> p_points, const PoolVector<Color> &p_colors, const PoolVector<Point2> &p_uvs = PoolVector<Point2>(), const PoolVector<int> &p_bones = PoolVector<int>(), const PoolVector<float> &p_weights = PoolVector<float>(), RID p_texture = RID(), int p_count = -1, RID p_normal_map = RID(), bool p_antialiased = false, bool p_antialiasing_use_indices = false) = 0;

    virtual void canvas_item_add_mesh(RID p_item, const RID &p_mesh, const Transform2D &p_transform = Transform2D(), const Color &p_modulate = Color(1, 1, 1), RID p_texture = RID(), RID p_normal_map = RID()) = 0;
    virtual void canvas_item_add_multimesh(RID p_item, RID p_mesh, RID p_texture = RID(), RID p_normal_map = RID()) = 0;
    virtual void canvas_item_add_particles(RID p_item, RID p_particles, RID p_texture, RID p_normal_map) = 0;
    virtual void canvas_item_add_set_transform(RID p_item, const Transform2D &p_transform) = 0;
    virtual void canvas_item_add_clip_ignore(RID p_item, bool p_ignore) = 0;
    virtual void canvas_item_set_sort_children_by_y(RID p_item, bool p_enable) = 0;
    virtual void canvas_item_set_z_index(RID p_item, int p_z) = 0;
    virtual void canvas_item_set_z_as_relative_to_parent(RID p_item, bool p_enable) = 0;
    virtual void canvas_item_set_copy_to_backbuffer(RID p_item, bool p_enable, const Rect2 &p_rect) = 0;

    virtual void canvas_item_attach_skeleton(RID p_item, RID p_skeleton) = 0;

    virtual void canvas_item_clear(RID p_item) = 0;
    virtual void canvas_item_set_draw_index(RID p_item, int p_index) = 0;

    virtual void canvas_item_set_material(RID p_item, RID p_material) = 0;

    virtual void canvas_item_set_use_parent_material(RID p_item, bool p_enable) = 0;

    virtual RID canvas_light_create() = 0;
    virtual void canvas_light_attach_to_canvas(RID p_light, RID p_canvas) = 0;
    virtual void canvas_light_set_enabled(RID p_light, bool p_enabled) = 0;
    virtual void canvas_light_set_scale(RID p_light, float p_scale) = 0;
    virtual void canvas_light_set_transform(RID p_light, const Transform2D &p_transform) = 0;
    virtual void canvas_light_set_texture(RID p_light, RID p_texture) = 0;
    virtual void canvas_light_set_texture_offset(RID p_light, const Vector2 &p_offset) = 0;
    virtual void canvas_light_set_color(RID p_light, const Color &p_color) = 0;
    virtual void canvas_light_set_height(RID p_light, float p_height) = 0;
    virtual void canvas_light_set_energy(RID p_light, float p_energy) = 0;
    virtual void canvas_light_set_z_range(RID p_light, int p_min_z, int p_max_z) = 0;
    virtual void canvas_light_set_layer_range(RID p_light, int p_min_layer, int p_max_layer) = 0;
    virtual void canvas_light_set_item_cull_mask(RID p_light, int p_mask) = 0;
    virtual void canvas_light_set_item_shadow_cull_mask(RID p_light, int p_mask) = 0;

    virtual void canvas_light_set_mode(RID p_light, VS::CanvasLightMode p_mode) = 0;
    virtual void canvas_light_set_shadow_enabled(RID p_light, bool p_enabled) = 0;
    virtual void canvas_light_set_shadow_buffer_size(RID p_light, int p_size) = 0;
    virtual void canvas_light_set_shadow_gradient_length(RID p_light, float p_length) = 0;
    virtual void canvas_light_set_shadow_filter(RID p_light, VS::CanvasLightShadowFilter p_filter) = 0;
    virtual void canvas_light_set_shadow_color(RID p_light, const Color &p_color) = 0;
    virtual void canvas_light_set_shadow_smooth(RID p_light, float p_smooth) = 0;

    virtual RID canvas_light_occluder_create() = 0;
    virtual void canvas_light_occluder_attach_to_canvas(RID p_occluder, RID p_canvas) = 0;
    virtual void canvas_light_occluder_set_enabled(RID p_occluder, bool p_enabled) = 0;
    virtual void canvas_light_occluder_set_polygon(RID p_occluder, RID p_polygon) = 0;
    virtual void canvas_light_occluder_set_transform(RID p_occluder, const Transform2D &p_xform) = 0;
    virtual void canvas_light_occluder_set_light_mask(RID p_occluder, int p_mask) = 0;

    virtual RID canvas_occluder_polygon_create() = 0;
    virtual void canvas_occluder_polygon_set_shape(RID p_occluder_polygon, Span<const Vector2> p_shape, bool p_closed) = 0;
    virtual void canvas_occluder_polygon_set_shape_as_lines(RID p_occluder_polygon, Span<const Vector2> p_shape) = 0;

    virtual void canvas_occluder_polygon_set_cull_mode(RID p_occluder_polygon, VS::CanvasOccluderPolygonCullMode p_mode) = 0;

    /* BLACK BARS */

    virtual void black_bars_set_margins(int p_left, int p_top, int p_right, int p_bottom) = 0;
    virtual void black_bars_set_images(RID p_left, RID p_top, RID p_right, RID p_bottom) = 0;

    /* FREE */

    virtual void free_rid(RID p_rid) = 0; ///< free RIDs associated with the visual server

    virtual void request_frame_drawn_callback(Object *p_where, const StringName &p_method, const Variant &p_userdata) = 0;

    /* EVENT QUEUING */

    virtual void draw(bool p_swap_buffers = true, double frame_step = 0.0) = 0;
    virtual void sync() = 0;
    virtual bool has_changed() const = 0;
    virtual void init() = 0;
    virtual void finish() = 0;

    /* STATUS INFORMATION */

    virtual int get_render_info(VS::RenderInfo p_info) = 0;
    virtual const char *get_video_adapter_name() const = 0;
    virtual const char *get_video_adapter_vendor() const = 0;

    /* Materials for 2D on 3D */

    /* TESTING */

    virtual RID get_test_cube() = 0;

    virtual RID get_test_texture();
    virtual RID get_white_texture();

    virtual RID make_sphere_mesh(int p_lats, int p_lons, float p_radius);

    virtual void mesh_add_surface_from_mesh_data(RID p_mesh, const Geometry::MeshData &p_mesh_data);
    virtual void mesh_add_surface_from_planes(RID p_mesh, const PoolVector<Plane> &p_planes);

    virtual void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) = 0;
    virtual void set_default_clear_color(const Color &p_color) = 0;

    virtual bool has_feature(VS::Features p_feature) const = 0;

    virtual bool has_os_feature(const StringName &p_feature) const = 0;

    virtual void set_debug_generate_wireframes(bool p_generate) = 0;

    virtual void call_set_use_vsync(bool p_enable) = 0;

    virtual bool is_low_end() const = 0;

    VisualServer();
    ~VisualServer() override;
};
