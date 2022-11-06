#pragma once
#include "core/hash_map.h"
#include "core/vector.h"
#include "core/image.h"
#include "core/string.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering/render_entity_helpers.h"
#include "servers/rendering_server_enums.h"

struct RasterizerMaterialComponent;
struct RasterizerGLES3ShadersStorage;
class ShaderGLES3;

struct ShaderDirtyMarker {};

struct RasterizerShaderComponent {
    uint32_t version = 1;
    HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> uniforms;
    HashMap<StringName, RenderingEntity> default_textures;
    Vector<uint32_t> ubo_offsets;
    Vector<ShaderLanguage::DataType> texture_types;
    Vector<ShaderLanguage::ShaderNode::Uniform::Hint> texture_hints;
    // links to all materials using this shader
    Vector<RenderingEntity> materials; //RasterizerMaterialComponent
    String code;
    String path;
    MoveOnlyEntityHandle self;
    MoveOnlyPointer<ShaderGLES3> shader;

    RS::ShaderMode mode;
    uint32_t ubo_size=0;
    uint32_t texture_count;
    uint32_t custom_code_id=0;
    bool valid = false;

    struct CanvasItem {
        enum BlendMode : int8_t {
            BLEND_MODE_MIX,
            BLEND_MODE_ADD,
            BLEND_MODE_SUB,
            BLEND_MODE_MUL,
            BLEND_MODE_PMALPHA,
            BLEND_MODE_DISABLED,
        };
        enum LightMode : int8_t {
            LIGHT_MODE_NORMAL,
            LIGHT_MODE_UNSHADED,
            LIGHT_MODE_LIGHT_ONLY
        };
        // these flags are specifically for batching
        // some of the logic is thus in rasterizer_storage.cpp
        // we could alternatively set bitflags for each 'uses' and test on the fly
        // defined in RasterizerStorageCommon::BatchFlags
        uint32_t batch_flags;

        BlendMode blend_mode;
        LightMode light_mode;

        bool uses_screen_uv;
        bool uses_time;
        bool uses_screen_texture;
        bool uses_modulate;
        bool uses_color;
        bool uses_vertex;

        // all these should disable item joining if used in a custom shader
        bool uses_world_matrix;
        bool uses_extra_matrix;
        bool uses_projection_matrix;
        bool uses_instance_custom;
    } canvas_item;

    struct Node3D {
        enum BlendMode : int8_t {
            BLEND_MODE_MIX,
            BLEND_MODE_ADD,
            BLEND_MODE_SUB,
            BLEND_MODE_MUL,
        };
        enum DepthDrawMode : int8_t {
            DEPTH_DRAW_OPAQUE,
            DEPTH_DRAW_ALWAYS,
            DEPTH_DRAW_NEVER,
            DEPTH_DRAW_ALPHA_PREPASS,
        };
        enum CullMode : int8_t {
            CULL_MODE_FRONT,
            CULL_MODE_BACK,
            CULL_MODE_DISABLED,
        };

        int8_t blend_mode;
        int8_t depth_draw_mode;
        int8_t cull_mode;

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
        bool uses_tangent;
        bool uses_ensure_correct_normals;
        bool writes_modelview_or_projection;
        bool uses_vertex_lighting;
        bool uses_world_coordinates;

    } spatial;

    struct Particles {
    } particles;

    bool uses_vertex_time=false;
    bool uses_fragment_time=false;

    RasterizerShaderComponent(const RasterizerShaderComponent &) = delete;
    RasterizerShaderComponent &operator=(const RasterizerShaderComponent &) = delete;

    RasterizerShaderComponent(RasterizerShaderComponent &&f) noexcept {
        *this = eastl::move(f);
    }
    RasterizerShaderComponent &operator = (RasterizerShaderComponent &&);

    RasterizerShaderComponent()  {
    }
    ~RasterizerShaderComponent();
};
void _shader_make_dirty(const RasterizerShaderComponent *p_shader);
void _update_shader(RasterizerGLES3ShadersStorage &shaders, RasterizerShaderComponent *p_shader);
