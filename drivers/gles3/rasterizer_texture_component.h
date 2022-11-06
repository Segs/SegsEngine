#pragma once

#include "servers/rendering/render_entity_helpers.h"

#include "rasterizer_gl_unique_handle.h"
#include "core/hash_set.h"
#include "core/image.h"
#include "core/vector.h"
#include "servers/rendering_server.h"
#include "servers/rendering/render_entity_getter.h"

struct RasterizerTextureComponent {
    HashSet<RenderingEntity> proxy_owners;
    Vector<Ref<Image> > images; //TODO: SEGS: consider using FixedVector here
    String path;

    MoveOnlyEntityHandle render_target = entt::null; //RasterizerRenderTargetComponent *
    MoveOnlyEntityHandle proxy=entt::null; //RasterizerTextureComponent *
    MoveOnlyEntityHandle self;
    RenderingServer::TextureDetectCallback detect_3d = nullptr;
    void* detect_3d_ud = nullptr;

    RenderingServer::TextureDetectCallback detect_srgb = nullptr;
    void* detect_srgb_ud = nullptr;

    RenderingServer::TextureDetectCallback detect_normal = nullptr;
    void* detect_normal_ud = nullptr;

    int width = 0, height = 0, depth;
    int alloc_width, alloc_height, alloc_depth;
    ImageData::Format format=ImageData::FORMAT_L8;
    RS::TextureType type = RS::TEXTURE_TYPE_2D;

    GLenum target = GL_TEXTURE_2D;
    GLenum gl_format_cache;
    GLenum gl_internal_format_cache;
    GLenum gl_type_cache;
    int data_size = 0; //original data size, useful for retrieving back
    int total_data_size = 0;
    int mipmaps = 0;
    uint32_t flags = 0;
    GLTextureHandle tex_id;
    GLNonOwningHandle external_tex_id;

    GLuint get_texture_id() const {
        if(tex_id.is_initialized())
            return tex_id;
        return external_tex_id;
    }
    uint16_t stored_cube_sides = 0;


    bool compressed = false;
    bool srgb = false;
    bool ignore_mipmaps = false;
    bool active = false;
    bool using_srgb = false;
    bool redraw_if_visible = false;


    RasterizerTextureComponent *get_ptr() {
        return proxy!=entt::null ? get<RasterizerTextureComponent>(proxy) : this;
    }
    const RasterizerTextureComponent *get_ptr() const {
        return proxy!=entt::null ? get<RasterizerTextureComponent>(proxy) : this;
    }
    RenderingEntity get_self_or_proxy() {
        return proxy!=entt::null ? proxy : self;
    }

    // Non copyable.
    RasterizerTextureComponent(const RasterizerTextureComponent&) = delete;
    RasterizerTextureComponent& operator=(const RasterizerTextureComponent&) = delete;
    // But movable.
    RasterizerTextureComponent(RasterizerTextureComponent &&a);
    RasterizerTextureComponent & operator=(RasterizerTextureComponent && fr);

    RasterizerTextureComponent() {}
    ~RasterizerTextureComponent();
};

void texture_set_flags(RasterizerTextureComponent *texture, uint32_t p_flags, bool use_anisotropic, bool use_fast_texture_filter, int anisotropic_level, bool srgb_decode_supported);
