/*************************************************************************/
/*  rasterizer_storage_gles3.h                                           */
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

#include "core/self_list.h"
#include "servers/rendering/rasterizer.h"
#include "servers/rendering/shader_language.h"
#include "shader_compiler_gles3.h"
#include "shader_gles3.h"

#include "gles3/shaders/blend_shape.glsl.gen.h"
#include "gles3/shaders/canvas.glsl.gen.h"
#include "gles3/shaders/copy.glsl.gen.h"
#include "gles3/shaders/cubemap_filter.glsl.gen.h"
#include "gles3/shaders/particles.glsl.gen.h"

#include "EASTL/deque.h"

class RasterizerCanvasGLES3;
class RasterizerSceneGLES3;

#define _TEXTURE_SRGB_DECODE_EXT 0x8A48
#define _DECODE_EXT 0x8A49
#define _SKIP_DECODE_EXT 0x8A4A

void glTexStorage2DCustom(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type);

class RasterizerStorageGLES3 : public RasterizerStorage {
public:
    RasterizerCanvasGLES3 *canvas;
    RasterizerSceneGLES3 *scene;
    static GLuint system_fbo; //on some devices, such as apple, screen is rendered to yet another fbo.

    enum RenderArchitecture {
        RENDER_ARCH_MOBILE,
        RENDER_ARCH_DESKTOP,
    };

    struct Config {

        bool shrink_textures_x2;
        bool use_fast_texture_filter;
        bool use_anisotropic_filter;

        bool s3tc_supported;
        bool latc_supported;
        bool rgtc_supported;
        bool bptc_supported;
        bool etc_supported;
        bool etc2_supported;
        bool pvrtc_supported;

        bool srgb_decode_supported;

        bool texture_float_linear_supported;
        bool framebuffer_float_supported;
        bool framebuffer_half_float_supported;

        bool use_rgba_2d_shadows;

        float anisotropic_level;

        int max_texture_image_units;
        int max_texture_size;

        bool generate_wireframes;

        bool use_texture_array_environment;

        Set<String> extensions;

        bool keep_original_textures;

        bool use_depth_prepass;
        bool force_vertex_shading;
    } config;

    mutable struct Shaders {

        CopyShaderGLES3 copy;

        ShaderCompilerGLES3 compiler;

        CubemapFilterShaderGLES3 cubemap_filter;

        BlendShapeShaderGLES3 blend_shapes;

        ParticlesShaderGLES3 particles;

        ShaderCompilerGLES3::IdentifierActions actions_canvas;
        ShaderCompilerGLES3::IdentifierActions actions_scene;
        ShaderCompilerGLES3::IdentifierActions actions_particles;
    } shaders;

    struct Resources {

        GLuint white_tex;
        GLuint black_tex;
        GLuint normal_tex;
        GLuint aniso_tex;

        GLuint white_tex_3d;
        GLuint white_tex_array;

        GLuint quadie;
        GLuint quadie_array;

        GLuint transform_feedback_buffers[2];
        GLuint transform_feedback_array;

    } resources;

    struct Info {

        uint64_t texture_mem;
        uint64_t vertex_mem;

        struct Render {
            uint32_t object_count;
            uint32_t draw_call_count;
            uint32_t material_switch_count;
            uint32_t surface_switch_count;
            uint32_t shader_rebind_count;
            uint32_t vertices_count;
            uint32_t _2d_item_count;
            uint32_t _2d_draw_call_count;

            void reset() {
                object_count = 0;
                draw_call_count = 0;
                material_switch_count = 0;
                surface_switch_count = 0;
                shader_rebind_count = 0;
                vertices_count = 0;
                _2d_item_count = 0;
                _2d_draw_call_count = 0;
            }
        } render, render_final, snap;

        Info() {

            texture_mem = 0;
            vertex_mem = 0;
            render.reset();
            render_final.reset();
        }

    } info;

    /////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////DATA///////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////

    struct Instantiable : public RID_Data {

        IntrusiveList<RasterizerScene::InstanceBase> instance_list;

        _FORCE_INLINE_ void instance_change_notify(bool p_aabb, bool p_materials) {

            IntrusiveListNode<RasterizerScene::InstanceBase> *instances = instance_list.first();
            while (instances) {

                instances->self()->base_changed(p_aabb, p_materials);
                instances = instances->next();
            }
        }

        _FORCE_INLINE_ void instance_remove_deps() {
            IntrusiveListNode<RasterizerScene::InstanceBase> *instances = instance_list.first();
            while (instances) {

                IntrusiveListNode<RasterizerScene::InstanceBase> *next = instances->next();
                instances->self()->base_removed();
                instances = next;
            }
        }

        Instantiable() {}
        ~Instantiable() override {
        }
    };

    struct GeometryOwner : public Instantiable {

        ~GeometryOwner() override {}
    };
    struct Geometry : Instantiable {

        enum Type {
            GEOMETRY_INVALID,
            GEOMETRY_SURFACE,
            GEOMETRY_IMMEDIATE,
            GEOMETRY_MULTISURFACE,
        };

        RID material;
        uint64_t last_pass=0;
        uint32_t index=0;
        Type type;

        virtual void material_changed_notify() {}
    };

    /////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////API////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////

    /* TEXTURE API */

    struct RenderTarget;

    struct Texture : public RID_Data {

        uint32_t flags=0; // put here to align next field to 8 bytes
        HashSet<Texture *> proxy_owners;
        Vector<Ref<Image> > images; //TODO: SEGS: consider using FixedVector here
        String path;

        RenderTarget *render_target = nullptr;
        Texture *proxy=nullptr;
        RenderingServer::TextureDetectCallback detect_3d = nullptr;
        void *detect_3d_ud = nullptr;

        RenderingServer::TextureDetectCallback detect_srgb = nullptr;
        void *detect_srgb_ud = nullptr;

        RenderingServer::TextureDetectCallback detect_normal = nullptr;
        void *detect_normal_ud = nullptr;

        int width=0, height=0, depth;
        int alloc_width, alloc_height, alloc_depth;
        Image::Format format=Image::FORMAT_L8;
        RS::TextureType type=RS::TEXTURE_TYPE_2D;

        GLenum target = GL_TEXTURE_2D;
        GLenum gl_format_cache;
        GLenum gl_internal_format_cache;
        GLenum gl_type_cache;
        int data_size=0; //original data size, useful for retrieving back
        int total_data_size=0;
        int mipmaps=0;
        GLuint tex_id=0;
        uint16_t stored_cube_sides=0;


        bool compressed=false;
        bool srgb=false;
        bool ignore_mipmaps=false;
        bool active=false;
        bool using_srgb=false;
        bool redraw_if_visible=false;
        Texture() {
        }

        _ALWAYS_INLINE_ Texture *get_ptr() {
            if (proxy) {
                return proxy; //->get_ptr(); only one level of indirection, else not inlining possible.
            } else {
                return this;
            }
        }

        ~Texture() override {

            if (tex_id != 0) {

                glDeleteTextures(1, &tex_id);
            }

            for (Texture * E : proxy_owners) {
                E->proxy = nullptr;
            }

            if (proxy) {
                proxy->proxy_owners.erase(this);
            }
        }
    };

    mutable RID_Owner<Texture> texture_owner;

    Ref<Image> _get_gl_image_and_format(const Ref<Image> &p_image, Image::Format p_format, uint32_t p_flags, Image::Format &r_real_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type, bool &r_compressed, bool &srgb, bool p_force_decompress) const;

    RID texture_create() override;
    void texture_allocate(RID p_texture, int p_width, int p_height, int p_depth_3d, Image::Format p_format, RS::TextureType p_type, uint32_t p_flags = RS::TEXTURE_FLAGS_DEFAULT) override;
    void texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) override;
    void texture_set_data_partial(RID p_texture, const Ref<Image> &p_image, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, int p_dst_mip, int p_layer = 0) override;
    Ref<Image> texture_get_data(RID p_texture, int p_layer = 0) const override;
    void texture_set_flags(RID p_texture, uint32_t p_flags) override;
    uint32_t texture_get_flags(RID p_texture) const override;
    Image::Format texture_get_format(RID p_texture) const override;
    RS::TextureType texture_get_type(RID p_texture) const override;
    uint32_t texture_get_texid(RID p_texture) const override;
    uint32_t texture_get_width(RID p_texture) const override;
    uint32_t texture_get_height(RID p_texture) const override;
    uint32_t texture_get_depth(RID p_texture) const override;
    void texture_set_size_override(RID p_texture, int p_width, int p_height, int p_depth) override;
    void texture_bind(RID p_texture, uint32_t p_texture_no) override;

    void texture_set_path(RID p_texture, StringView p_path) override;
    const String &texture_get_path(RID p_texture) const override;

    void texture_set_shrink_all_x2_on_set_data(bool p_enable) override;

    void texture_debug_usage(Vector<RenderingServer::TextureInfo> *r_info) override;

    RID texture_create_radiance_cubemap(RID p_source, int p_resolution = -1) const override;

    void textures_keep_original(bool p_enable) override;

    void texture_set_detect_3d_callback(RID p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) override;
    void texture_set_detect_srgb_callback(RID p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) override;
    void texture_set_detect_normal_callback(RID p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) override;

    void texture_set_proxy(RID p_texture, RID p_proxy) override;
    Size2 texture_size_with_proxy(RID p_texture) const override;

    void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) override;

    /* SKY API */

    struct Sky : public RID_Data {
        RID panorama;
        GLuint radiance;
        GLuint irradiance;
        int radiance_size;
    };

    mutable RID_Owner<Sky> sky_owner;

    RID sky_create() override;
    void sky_set_texture(RID p_sky, RID p_panorama, int p_radiance_size) override;

    /* SHADER API */

    struct Material;

    struct Shader : public RID_Data {

        uint32_t version;
        HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> uniforms;
        HashMap<StringName, RID> default_textures;
        Vector<uint32_t> ubo_offsets;
        Vector<ShaderLanguage::DataType> texture_types;
        Vector<ShaderLanguage::ShaderNode::Uniform::Hint> texture_hints;
        IntrusiveList<Material> materials;
        IntrusiveListNode<Shader> dirty_list;
        String code;
        String path;
        RID self;
        ShaderGLES3 *shader;

        RS::ShaderMode mode;
        uint32_t ubo_size;
        uint32_t texture_count;
        uint32_t custom_code_id;
        bool valid;


        struct CanvasItem {
            bool uses_screen_texture;
            bool uses_screen_uv;
            bool uses_time;
            enum BlendMode : int8_t {
                BLEND_MODE_MIX,
                BLEND_MODE_ADD,
                BLEND_MODE_SUB,
                BLEND_MODE_MUL,
                BLEND_MODE_PMALPHA,
                BLEND_MODE_DISABLED,
            };

            int blend_mode;

            enum LightMode : int8_t {
                LIGHT_MODE_NORMAL,
                LIGHT_MODE_UNSHADED,
                LIGHT_MODE_LIGHT_ONLY
            };

            int light_mode;


        } canvas_item;

        struct Node3D {

            enum BlendMode {
                BLEND_MODE_MIX,
                BLEND_MODE_ADD,
                BLEND_MODE_SUB,
                BLEND_MODE_MUL,
            };

            int blend_mode;

            enum DepthDrawMode {
                DEPTH_DRAW_OPAQUE,
                DEPTH_DRAW_ALWAYS,
                DEPTH_DRAW_NEVER,
                DEPTH_DRAW_ALPHA_PREPASS,
            };

            int depth_draw_mode;

            enum CullMode {
                CULL_MODE_FRONT,
                CULL_MODE_BACK,
                CULL_MODE_DISABLED,
            };

            int cull_mode;

            bool uses_alpha;
            bool uses_alpha_scissor;
            bool unshaded;
            bool no_depth_test;
            bool uses_vertex;
            bool uses_discard;
            bool uses_sss;
            bool uses_screen_texture;
            bool uses_depth_texture;
            bool uses_time;
            bool writes_modelview_or_projection;
            bool uses_vertex_lighting;
            bool uses_world_coordinates;

        } spatial;

        struct Particles {

        } particles;

        bool uses_vertex_time;
        bool uses_fragment_time;

        Shader() :
                dirty_list(this) {

            shader = nullptr;
            ubo_size = 0;
            valid = false;
            custom_code_id = 0;
            version = 1;
        }
    };

    mutable IntrusiveList<Shader> _shader_dirty_list;
    void _shader_make_dirty(Shader *p_shader);

    mutable RID_Owner<Shader> shader_owner;

    RID shader_create() override;

    void shader_set_code(RID p_shader, const String &p_code) override;
    String shader_get_code(RID p_shader) const override;
    void shader_get_param_list(RID p_shader, Vector<PropertyInfo> *p_param_list) const override;

    void shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture) override;
    RID shader_get_default_texture_param(RID p_shader, const StringName &p_name) const override;

    void shader_add_custom_define(RID p_shader, StringView p_define) override;
    void shader_get_custom_defines(RID p_shader, Vector<StringView> *p_defines) const override;
    void shader_remove_custom_define(RID p_shader, StringView p_define) override;


    void _update_shader(Shader *p_shader) const;

    void update_dirty_shaders();

    /* COMMON MATERIAL API */

    struct Material : public RID_Data {

        GLuint ubo_id=0;
        Shader *shader=nullptr;
        HashMap<StringName, Variant> params;
        HashMap<Geometry *, int> geometry_owners;
        HashMap<RasterizerScene::InstanceBase *, int> instance_owners;
        IntrusiveListNode<Material> list;
        IntrusiveListNode<Material> dirty_list;
        Vector<bool> texture_is_3d; //TODO: SEGS: consider using dynamic_bitvector here.
        Vector<RID> textures;
        RID next_pass;
        float line_width;
        uint32_t ubo_size=0;
        int render_priority=0;
        uint32_t index;
        uint64_t last_pass=0;


        bool can_cast_shadow_cache;
        bool is_animated_cache;

        Material() :
                list(this),
                dirty_list(this),
                line_width(1.0),
                can_cast_shadow_cache(false),
                is_animated_cache(false) {
        }
    };

    mutable IntrusiveList<Material> _material_dirty_list;
    void _material_make_dirty(Material *p_material) const;
    void _material_add_geometry(RID p_material, Geometry *p_geometry);
    void _material_remove_geometry(RID p_material, Geometry *p_geometry);

    mutable RID_Owner<Material> material_owner;

    RID material_create() override;

    void material_set_shader(RID p_material, RID p_shader) override;
    RID material_get_shader(RID p_material) const override;

    void material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) override;
    Variant material_get_param(RID p_material, const StringName &p_param) const override;
    Variant material_get_param_default(RID p_material, const StringName &p_param) const override;

    void material_set_line_width(RID p_material, float p_width) override;
    void material_set_next_pass(RID p_material, RID p_next_material) override;

    bool material_is_animated(RID p_material) override;
    bool material_casts_shadows(RID p_material) override;

    void material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) override;
    void material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) override;

    void material_set_render_priority(RID p_material, int priority) override;

    void _update_material(Material *material);

    void update_dirty_materials();

    /* MESH API */

    struct Mesh;
    struct Surface : public Geometry {

        struct Attrib {

            GLuint index;
            GLint size;
            uint32_t offset;
            GLenum type;
            GLsizei stride;
            bool enabled;
            bool integer;
            GLboolean normalized;
        };
        struct BlendShape {
            GLuint vertex_id;
            GLuint array_id;
        };

        Attrib attribs[RS::ARRAY_MAX];
        Vector<AABB> skeleton_bone_aabb;
        Vector<bool> skeleton_bone_used;
        Vector<BlendShape> blend_shapes;

        AABB aabb;
        Mesh *mesh;
        uint32_t format;

        GLuint array_id;
        GLuint instancing_array_id;
        GLuint vertex_id;
        GLuint index_id;

        GLuint index_wireframe_id;
        GLuint array_wireframe_id;
        GLuint instancing_array_wireframe_id;
        int index_wireframe_len;

        int array_len;
        int index_array_len;
        int max_bone;

        int array_byte_size;
        int index_array_byte_size;

        RS::PrimitiveType primitive;

        bool active;

        void material_changed_notify() override {
            mesh->instance_change_notify(false, true);
            mesh->update_multimeshes();
        }

        int total_data_size;

        Surface() :
                mesh(nullptr),
                format(0),
                array_id(0),
                vertex_id(0),
                index_id(0),
                index_wireframe_id(0),
                array_wireframe_id(0),
                instancing_array_wireframe_id(0),
                index_wireframe_len(0),
                array_len(0),
                index_array_len(0),
                array_byte_size(0),
                index_array_byte_size(0),
                primitive(RS::PRIMITIVE_POINTS),
                active(false),
                total_data_size(0) {
            type = GEOMETRY_SURFACE;
        }

        ~Surface() override {
        }
    };

    struct MultiMesh;

    struct Mesh : public GeometryOwner {

        bool active;
        Vector<Surface *> surfaces;
        int blend_shape_count;
        RS::BlendShapeMode blend_shape_mode;
        AABB custom_aabb;
        mutable uint64_t last_pass;
        IntrusiveList<MultiMesh> multimeshes;
        _FORCE_INLINE_ void update_multimeshes() {

            IntrusiveListNode<MultiMesh> *mm = multimeshes.first();
            while (mm) {
                mm->self()->instance_change_notify(false, true);
                mm = mm->next();
            }
        }

        Mesh() :
                active(false),
                blend_shape_count(0),
                blend_shape_mode(RS::BLEND_SHAPE_MODE_NORMALIZED),
                last_pass(0) {
        }
    };

    mutable RID_Owner<Mesh> mesh_owner;

    RID mesh_create() override;

    void mesh_add_surface(RID p_mesh, uint32_t p_format, RS::PrimitiveType p_primitive, Span<const uint8_t> p_array, int p_vertex_count, Span<const uint8_t> p_index_array, int p_index_count, const AABB &p_aabb, const
                          Vector<PoolVector<uint8_t>> &p_blend_shapes = Vector<PoolVector<uint8_t>>(), Span<const AABB> p_bone_aabbs = {}) override;

    void mesh_set_blend_shape_count(RID p_mesh, int p_amount) override;
    int mesh_get_blend_shape_count(RID p_mesh) const override;

    void mesh_set_blend_shape_mode(RID p_mesh, RS::BlendShapeMode p_mode) override;
    RS::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const override;

    void mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, Span<const uint8_t> p_data) override;

    void mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) override;
    RID mesh_surface_get_material(RID p_mesh, int p_surface) const override;

    int mesh_surface_get_array_len(RID p_mesh, int p_surface) const override;
    int mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const override;

    PoolVector<uint8_t> mesh_surface_get_array(RID p_mesh, int p_surface) const override;
    PoolVector<uint8_t> mesh_surface_get_index_array(RID p_mesh, int p_surface) const override;

    uint32_t mesh_surface_get_format(RID p_mesh, int p_surface) const override;
    RS::PrimitiveType mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const override;

    AABB mesh_surface_get_aabb(RID p_mesh, int p_surface) const override;
    Vector<Vector<uint8_t>> mesh_surface_get_blend_shapes(RID p_mesh, int p_surface) const override;
    const Vector<AABB> &mesh_surface_get_skeleton_aabb(RID p_mesh, int p_surface) const override;

    void mesh_remove_surface(RID p_mesh, int p_surface) override;
    int mesh_get_surface_count(RID p_mesh) const override;

    void mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) override;
    AABB mesh_get_custom_aabb(RID p_mesh) const override;

    AABB mesh_get_aabb(RID p_mesh, RID p_skeleton) const override;
    void mesh_clear(RID p_mesh) override;

    void mesh_render_blend_shapes(Surface *s, const float *p_weights);

    /* MULTIMESH API */

    struct MultiMesh : public GeometryOwner {
        RID mesh;
        int size;
        RS::MultimeshTransformFormat transform_format;
        RS::MultimeshColorFormat color_format;
        RS::MultimeshCustomDataFormat custom_data_format;
        PoolVector<float> data;
        AABB aabb;
        IntrusiveListNode<MultiMesh> update_list;
        IntrusiveListNode<MultiMesh> mesh_list;
        GLuint buffer;
        int visible_instances;

        int xform_floats;
        int color_floats;
        int custom_data_floats;

        bool dirty_aabb;
        bool dirty_data;

        MultiMesh() :
                size(0),
                transform_format(RS::MULTIMESH_TRANSFORM_2D),
                color_format(RS::MULTIMESH_COLOR_NONE),
                custom_data_format(RS::MULTIMESH_CUSTOM_DATA_NONE),
                update_list(this),
                mesh_list(this),
                buffer(0),
                visible_instances(-1),
                xform_floats(0),
                color_floats(0),
                custom_data_floats(0),
                dirty_aabb(true),
                dirty_data(true) {
        }
    };

    mutable RID_Owner<MultiMesh> multimesh_owner;

    IntrusiveList<MultiMesh> multimesh_update_list;

    void update_dirty_multimeshes();

    RID multimesh_create() override;

    void multimesh_allocate(RID p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, RS::MultimeshColorFormat p_color_format, RS::MultimeshCustomDataFormat p_data_format = RS::MULTIMESH_CUSTOM_DATA_NONE) override;
    int multimesh_get_instance_count(RID p_multimesh) const override;

    void multimesh_set_mesh(RID p_multimesh, RID p_mesh) override;
    void multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform) override;
    void multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) override;
    void multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) override;
    void multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_custom_data) override;

    RID multimesh_get_mesh(RID p_multimesh) const override;

    Transform multimesh_instance_get_transform(RID p_multimesh, int p_index) const override;
    Transform2D multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const override;
    Color multimesh_instance_get_color(RID p_multimesh, int p_index) const override;
    Color multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const override;

    void multimesh_set_as_bulk_array(RID p_multimesh, const PoolVector<float> &p_array) override;

    void multimesh_set_visible_instances(RID p_multimesh, int p_visible) override;
    int multimesh_get_visible_instances(RID p_multimesh) const override;

    AABB multimesh_get_aabb(RID p_multimesh) const override;

    /* IMMEDIATE API */

    struct Immediate : public Geometry {

        struct Chunk {
            Vector<Vector3> vertices;
            RID texture;
            Vector<Vector3> normals;
            Vector<Plane> tangents;
            Vector<Color> colors;
            Vector<Vector2> uvs;
            Vector<Vector2> uvs2;
            RS::PrimitiveType primitive;
        };

        eastl::deque<Chunk,wrap_allocator> chunks;
        AABB aabb;
        int mask;
        bool building;

        Immediate() {
            type = GEOMETRY_IMMEDIATE;
            building = false;
        }
    };

    Vector3 chunk_vertex;
    Vector3 chunk_normal;
    Plane chunk_tangent;
    Color chunk_color;
    Vector2 chunk_uv;
    Vector2 chunk_uv2;

    mutable RID_Owner<Immediate> immediate_owner;

    RID immediate_create() override;
    void immediate_begin(RID p_immediate, RS::PrimitiveType p_rimitive, RID p_texture = RID()) override;
    void immediate_vertex(RID p_immediate, const Vector3 &p_vertex) override;
    void immediate_normal(RID p_immediate, const Vector3 &p_normal) override;
    void immediate_tangent(RID p_immediate, const Plane &p_tangent) override;
    void immediate_color(RID p_immediate, const Color &p_color) override;
    void immediate_uv(RID p_immediate, const Vector2 &tex_uv) override;
    void immediate_uv2(RID p_immediate, const Vector2 &tex_uv) override;
    void immediate_end(RID p_immediate) override;
    void immediate_clear(RID p_immediate) override;
    void immediate_set_material(RID p_immediate, RID p_material) override;
    RID immediate_get_material(RID p_immediate) const override;
    AABB immediate_get_aabb(RID p_immediate) const override;

    /* SKELETON API */

    struct Skeleton : RID_Data {
        int size=0; // put first to align from parent members
        Set<RasterizerScene::InstanceBase *> instances; //instances using skeleton
        PoolVector<float> skel_texture;
        IntrusiveListNode<Skeleton> update_list;
        Transform2D base_transform_2d;
        GLuint texture=0;
        bool use_2d=false;
        Skeleton() : update_list(this) {
        }
    };

    mutable RID_Owner<Skeleton> skeleton_owner;

    IntrusiveList<Skeleton> skeleton_update_list;

    void update_dirty_skeletons();

    RID skeleton_create() override;
    void skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton = false) override;
    int skeleton_get_bone_count(RID p_skeleton) const override;
    void skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform) override;
    Transform skeleton_bone_get_transform(RID p_skeleton, int p_bone) const override;
    void skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) override;
    Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const override;
    void skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) override;

    /* Light3D API */

    struct Light3D : Instantiable {
        float param[RS::LIGHT_PARAM_MAX];
        Color color;
        Color shadow_color;
        RID projector;
        uint64_t version;
        uint32_t cull_mask;
        RS::LightType type;
        RS::LightOmniShadowMode omni_shadow_mode;
        RS::LightOmniShadowDetail omni_shadow_detail;
        RS::LightDirectionalShadowMode directional_shadow_mode;
        RS::LightDirectionalShadowDepthRangeMode directional_range_mode;
        bool shadow : 1;
        bool negative : 1;
        bool reverse_cull : 1;
        bool use_gi : 1;
        bool directional_blend_splits : 1;
    };

    mutable RID_Owner<Light3D> light_owner;

    RID light_create(RS::LightType p_type) override;

    void light_set_color(RID p_light, const Color &p_color) override;
    void light_set_param(RID p_light, RS::LightParam p_param, float p_value) override;
    void light_set_shadow(RID p_light, bool p_enabled) override;
    void light_set_shadow_color(RID p_light, const Color &p_color) override;
    void light_set_projector(RID p_light, RID p_texture) override;
    void light_set_negative(RID p_light, bool p_enable) override;
    void light_set_cull_mask(RID p_light, uint32_t p_mask) override;
    void light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) override;
    void light_set_use_gi(RID p_light, bool p_enabled) override;

    void light_omni_set_shadow_mode(RID p_light, RS::LightOmniShadowMode p_mode) override;
    void light_omni_set_shadow_detail(RID p_light, RS::LightOmniShadowDetail p_detail) override;

    void light_directional_set_shadow_mode(RID p_light, RS::LightDirectionalShadowMode p_mode) override;
    void light_directional_set_blend_splits(RID p_light, bool p_enable) override;
    bool light_directional_get_blend_splits(RID p_light) const override;

    RS::LightDirectionalShadowMode light_directional_get_shadow_mode(RID p_light) override;
    RS::LightOmniShadowMode light_omni_get_shadow_mode(RID p_light) override;

    void light_directional_set_shadow_depth_range_mode(RID p_light, RS::LightDirectionalShadowDepthRangeMode p_range_mode) override;
    RS::LightDirectionalShadowDepthRangeMode light_directional_get_shadow_depth_range_mode(RID p_light) const override;

    bool light_has_shadow(RID p_light) const override;

    RS::LightType light_get_type(RID p_light) const override;
    float light_get_param(RID p_light, RS::LightParam p_param) override;
    Color light_get_color(RID p_light) override;
    bool light_get_use_gi(RID p_light) override;

    AABB light_get_aabb(RID p_light) const override;
    uint64_t light_get_version(RID p_light) const override;

    /* PROBE API */

    struct ReflectionProbe : Instantiable {

        Color interior_ambient;
        Vector3 extents;
        Vector3 origin_offset;
        float intensity;
        float interior_ambient_energy;
        float interior_ambient_probe_contrib;
        float max_distance;
        uint32_t cull_mask;
        RS::ReflectionProbeUpdateMode update_mode;
        bool interior : 1;
        bool box_projection: 1;
        bool enable_shadows: 1;
    };

    mutable RID_Owner<ReflectionProbe> reflection_probe_owner;

    RID reflection_probe_create() override;

    void reflection_probe_set_update_mode(RID p_probe, RS::ReflectionProbeUpdateMode p_mode) override;
    void reflection_probe_set_intensity(RID p_probe, float p_intensity) override;
    void reflection_probe_set_interior_ambient(RID p_probe, const Color &p_ambient) override;
    void reflection_probe_set_interior_ambient_energy(RID p_probe, float p_energy) override;
    void reflection_probe_set_interior_ambient_probe_contribution(RID p_probe, float p_contrib) override;
    void reflection_probe_set_max_distance(RID p_probe, float p_distance) override;
    void reflection_probe_set_extents(RID p_probe, const Vector3 &p_extents) override;
    void reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) override;
    void reflection_probe_set_as_interior(RID p_probe, bool p_enable) override;
    void reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) override;
    void reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) override;
    void reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) override;
    void reflection_probe_set_resolution(RID p_probe, int p_resolution) override;

    AABB reflection_probe_get_aabb(RID p_probe) const override;
    RS::ReflectionProbeUpdateMode reflection_probe_get_update_mode(RID p_probe) const override;
    uint32_t reflection_probe_get_cull_mask(RID p_probe) const override;

    Vector3 reflection_probe_get_extents(RID p_probe) const override;
    Vector3 reflection_probe_get_origin_offset(RID p_probe) const override;
    float reflection_probe_get_origin_max_distance(RID p_probe) const override;
    bool reflection_probe_renders_shadows(RID p_probe) const override;

    /* GI PROBE API */

    struct GIProbe : public Instantiable {

        AABB bounds;
        Transform to_cell;
        PoolVector<int> dynamic_data;
        float cell_size;

        int dynamic_range;
        float energy;
        float bias;
        float normal_bias;
        float propagation;
        uint32_t version;

        bool interior;
        bool compress;

    };

    mutable RID_Owner<GIProbe> gi_probe_owner;

    RID gi_probe_create() override;

    void gi_probe_set_bounds(RID p_probe, const AABB &p_bounds) override;
    AABB gi_probe_get_bounds(RID p_probe) const override;

    void gi_probe_set_cell_size(RID p_probe, float p_size) override;
    float gi_probe_get_cell_size(RID p_probe) const override;

    void gi_probe_set_to_cell_xform(RID p_probe, const Transform &p_xform) override;
    Transform gi_probe_get_to_cell_xform(RID p_probe) const override;

    void gi_probe_set_dynamic_data(RID p_probe, const PoolVector<int> &p_data) override;
    PoolVector<int> gi_probe_get_dynamic_data(RID p_probe) const override;

    void gi_probe_set_dynamic_range(RID p_probe, int p_range) override;
    int gi_probe_get_dynamic_range(RID p_probe) const override;

    void gi_probe_set_energy(RID p_probe, float p_range) override;
    float gi_probe_get_energy(RID p_probe) const override;

    void gi_probe_set_bias(RID p_probe, float p_range) override;
    float gi_probe_get_bias(RID p_probe) const override;

    void gi_probe_set_normal_bias(RID p_probe, float p_range) override;
    float gi_probe_get_normal_bias(RID p_probe) const override;

    void gi_probe_set_propagation(RID p_probe, float p_range) override;
    float gi_probe_get_propagation(RID p_probe) const override;

    void gi_probe_set_interior(RID p_probe, bool p_enable) override;
    bool gi_probe_is_interior(RID p_probe) const override;

    void gi_probe_set_compress(RID p_probe, bool p_enable) override;
    bool gi_probe_is_compressed(RID p_probe) const override;

    uint32_t gi_probe_get_version(RID p_probe) override;

    struct GIProbeData : public RID_Data {

        int width;
        int height;
        int depth;
        int levels;
        GLuint tex_id;
        GIProbeCompression compression;

        GIProbeData() {
        }
    };

    mutable RID_Owner<GIProbeData> gi_probe_data_owner;

    GIProbeCompression gi_probe_get_dynamic_data_get_preferred_compression() const override;
    RID gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth, GIProbeCompression p_compression) override;
    void gi_probe_dynamic_data_update(RID p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data) override;

    /* LIGHTMAP CAPTURE */

    RID lightmap_capture_create() override;
    void lightmap_capture_set_bounds(RID p_capture, const AABB &p_bounds) override;
    AABB lightmap_capture_get_bounds(RID p_capture) const override;
    void lightmap_capture_set_octree(RID p_capture, const PoolVector<uint8_t> &p_octree) override;
    PoolVector<uint8_t> lightmap_capture_get_octree(RID p_capture) const override;
    void lightmap_capture_set_octree_cell_transform(RID p_capture, const Transform &p_xform) override;
    Transform lightmap_capture_get_octree_cell_transform(RID p_capture) const override;
    void lightmap_capture_set_octree_cell_subdiv(RID p_capture, int p_subdiv) override;
    int lightmap_capture_get_octree_cell_subdiv(RID p_capture) const override;

    void lightmap_capture_set_energy(RID p_capture, float p_energy) override;
    float lightmap_capture_get_energy(RID p_capture) const override;

    const PoolVector<LightmapCaptureOctree> *lightmap_capture_get_octree_ptr(RID p_capture) const override;

    struct LightmapCapture : public Instantiable {

        PoolVector<LightmapCaptureOctree> octree;
        AABB bounds;
        Transform cell_xform;
        int cell_subdiv=1;
        float energy=1.0f;
    };

    mutable RID_Owner<LightmapCapture> lightmap_capture_data_owner;

    /* PARTICLES */

    struct Particles : public GeometryOwner {

        Vector<RID> draw_passes;
        RID process_material;
        AABB custom_aabb;
        Transform emission_transform;
        IntrusiveListNode<Particles> particle_element;

        float inactive_time=0.0f;
        int amount=0;
        float lifetime=1.0f;
        float pre_process_time=0.0f;
        float explosiveness=0.0f;
        float randomness=0.0f;
        float phase;
        float prev_phase;
        uint64_t prev_ticks=0;
        uint32_t random_seed=0;

        uint32_t cycle_number=0;

        float speed_scale=1.0f;

        int fixed_fps=0;
        float frame_remainder=0;

        RS::ParticlesDrawOrder draw_order=RS::PARTICLES_DRAW_ORDER_INDEX;


        GLuint particle_buffers[2] = {0,0};
        GLuint particle_vaos[2];

        GLuint particle_buffer_histories[2];
        GLuint particle_vao_histories[2];
        bool particle_valid_histories[2];
        bool histories_enabled = false;

        bool inactive = true;
        bool emitting = false;
        bool one_shot = false;
        bool restart_request = false;
        bool use_local_coords = true;
        bool fractional_delta = false;
        bool clear = true;

        Particles() :
                custom_aabb(AABB(Vector3(-4, -4, -4), Vector3(8, 8, 8))),
                particle_element(this) {
            glGenBuffers(2, particle_buffers);
            glGenVertexArrays(2, particle_vaos);
        }

        ~Particles() override {

            glDeleteBuffers(2, particle_buffers);
            glDeleteVertexArrays(2, particle_vaos);
            if (histories_enabled) {
                glDeleteBuffers(2, particle_buffer_histories);
                glDeleteVertexArrays(2, particle_vao_histories);
            }
        }
    };

    IntrusiveList<Particles> particle_update_list;

    void update_particles();

    mutable RID_Owner<Particles> particles_owner;

    RID particles_create() override;

    void particles_set_emitting(RID p_particles, bool p_emitting) override;
    bool particles_get_emitting(RID p_particles) override;
    void particles_set_amount(RID p_particles, int p_amount) override;
    void particles_set_lifetime(RID p_particles, float p_lifetime) override;
    void particles_set_one_shot(RID p_particles, bool p_one_shot) override;
    void particles_set_pre_process_time(RID p_particles, float p_time) override;
    void particles_set_explosiveness_ratio(RID p_particles, float p_ratio) override;
    void particles_set_randomness_ratio(RID p_particles, float p_ratio) override;
    void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) override;
    void particles_set_speed_scale(RID p_particles, float p_scale) override;
    void particles_set_use_local_coordinates(RID p_particles, bool p_enable) override;
    void particles_set_process_material(RID p_particles, RID p_material) override;
    void particles_set_fixed_fps(RID p_particles, int p_fps) override;
    void particles_set_fractional_delta(RID p_particles, bool p_enable) override;
    void particles_restart(RID p_particles) override;

    void particles_set_draw_order(RID p_particles, RS::ParticlesDrawOrder p_order) override;

    void particles_set_draw_passes(RID p_particles, int p_passes) override;
    void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) override;

    void particles_request_process(RID p_particles) override;
    AABB particles_get_current_aabb(RID p_particles) override;
    AABB particles_get_aabb(RID p_particles) const override;

    virtual void _particles_update_histories(Particles *particles);

    void particles_set_emission_transform(RID p_particles, const Transform &p_transform) override;
    void _particles_process(Particles *p_particles, float p_delta);

    int particles_get_draw_passes(RID p_particles) const override;
    RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const override;

    bool particles_is_inactive(RID p_particles) const override;

    /* INSTANCE */

    void instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) override;
    void instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) override;

    void instance_add_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) override;
    void instance_remove_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) override;

    /* RENDER TARGET */

    struct RenderTarget : public RID_Data {

        GLuint fbo;
        GLuint color;
        GLuint depth;

        struct Buffers {

            bool active;
            bool effects_active;
            GLuint fbo;
            GLuint depth;
            GLuint specular;
            GLuint diffuse;
            GLuint normal_rough;
            GLuint sss;

            GLuint effect_fbo;
            GLuint effect;

        } buffers;

        struct Effects {

            struct MipMaps {

                struct Size {
                    GLuint fbo;
                    int width;
                    int height;
                };

                Vector<Size> sizes;
                GLuint color;
                int levels;

                MipMaps() :
                        color(0),
                        levels(0) {
                }
            };

            MipMaps mip_maps[2]; //first mipmap chain starts from full-screen
            //GLuint depth2; //depth for the second mipmap chain, in case of desiring upsampling

            struct SSAO {
                GLuint blur_fbo[2]; // blur fbo
                GLuint blur_red[2]; // 8 bits red buffer

                GLuint linear_depth;

                Vector<GLuint> depth_mipmap_fbos; //fbos for depth mipmapsla ver

                SSAO() :
                        linear_depth(0) {
                    blur_fbo[0] = 0;
                    blur_fbo[1] = 0;
                }
            } ssao;

            Effects() {}

        } effects;

        struct Exposure {
            GLuint fbo=0;
            GLuint color;

        } exposure;

        // External FBO to render our final result to (mostly used for ARVR)
        struct External {
            GLuint fbo;
            RID texture;

            External() :
                    fbo(0) {}
        } external;

        uint64_t last_exposure_tick;

        int width, height;

        bool flags[RENDER_TARGET_FLAG_MAX];

        bool used_in_frame;
        RS::ViewportMSAA msaa;

        RID texture;

        RenderTarget() :
                fbo(0),
                depth(0),
                last_exposure_tick(0),
                width(0),
                height(0),
                used_in_frame(false),
                msaa(RS::VIEWPORT_MSAA_DISABLED) {
            exposure.fbo = 0;
            buffers.fbo = 0;
            external.fbo = 0;
            for (int i = 0; i < RENDER_TARGET_FLAG_MAX; i++) {
                flags[i] = false;
            }
            flags[RENDER_TARGET_HDR] = true;
            buffers.active = false;
            buffers.effects_active = false;
        }
    };

    mutable RID_Owner<RenderTarget> render_target_owner;

    void _render_target_clear(RenderTarget *rt);
    void _render_target_allocate(RenderTarget *rt);

    RID render_target_create() override;
    void render_target_set_position(RID p_render_target, int p_x, int p_y) override;
    void render_target_set_size(RID p_render_target, int p_width, int p_height) override;
    RID render_target_get_texture(RID p_render_target) const override;
    void render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id) override;

    void render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) override;
    bool render_target_was_used(RID p_render_target) override;
    void render_target_clear_used(RID p_render_target) override;
    void render_target_set_msaa(RID p_render_target, RS::ViewportMSAA p_msaa) override;

    /* CANVAS SHADOW */

    struct CanvasLightShadow : public RID_Data {

        int size;
        int height;
        GLuint fbo;
        GLuint depth;
        GLuint distance; //for older devices
    };

    RID_Owner<CanvasLightShadow> canvas_light_shadow_owner;

    RID canvas_light_shadow_buffer_create(int p_width) override;

    /* LIGHT SHADOW MAPPING */

    struct CanvasOccluder : public RID_Data {

        GLuint array_id; // 0 means, unconfigured
        GLuint vertex_id; // 0 means, unconfigured
        GLuint index_id; // 0 means, unconfigured
        Vector<Vector2> lines;
        int len;
    };

    RID_Owner<CanvasOccluder> canvas_occluder_owner;

    RID canvas_light_occluder_create() override;
    void canvas_light_occluder_set_polylines(RID p_occluder, Span<const Vector2> p_lines) override;

    RS::InstanceType get_base_type(RID p_rid) const override;

    bool free(RID p_rid) override;

    struct Frame {

        RenderTarget *current_rt;

        bool clear_request;
        Color clear_request_color;
        float time[4];
        float delta;
        uint64_t count;

    } frame;

    void initialize();
    void finalize();

    bool has_os_feature(const StringName &p_feature) const override;

    void update_dirty_resources() override;

    void set_debug_generate_wireframes(bool p_generate) override;

    void render_info_begin_capture() override;
    void render_info_end_capture() override;
    int get_captured_render_info(RS::RenderInfo p_info) override;

    int get_render_info(RS::RenderInfo p_info) override;
    const char *get_video_adapter_name() const override;
    const char *get_video_adapter_vendor() const override;
    RasterizerStorageGLES3();
};
