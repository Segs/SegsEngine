#include "rasterizer_texture_component.h"

#include "rasterizer_canvas_gles3.h"
#include "rasterizer_storage_gles3.h"
#include "servers/rendering/render_entity_getter.h"


static void unregister_from_proxies(RasterizerTextureComponent *tex_component) {
    for (const auto ent : tex_component->proxy_owners) {
        auto *tex = get<RasterizerTextureComponent>(ent);
        tex->proxy = entt::null;
    }

    if (tex_component->proxy!=entt::null) {
        auto *our_proxy(get<RasterizerTextureComponent>(tex_component->proxy));
        our_proxy->proxy_owners.erase(tex_component->self);
    }
}

RasterizerTextureComponent::RasterizerTextureComponent(RasterizerTextureComponent &&fr)  {
    *this = eastl::move(fr);
}
#define MOVE_AND_RESET(v) eastl::move(v); v = {}

RasterizerTextureComponent &RasterizerTextureComponent::operator=(RasterizerTextureComponent &&f) {
    assert(this != &f);

    tex_id.release();
    external_tex_id = GLNonOwningHandle(0);

    unregister_from_proxies(this);
    // record used memory change.
    get_rasterizer_storage_info().texture_mem -= total_data_size;

    proxy_owners = MOVE_AND_RESET(f.proxy_owners);
    images = MOVE_AND_RESET(f.images);
    path = MOVE_AND_RESET(f.path);
    self = eastl::move(f.self);
    render_target = eastl::move(f.render_target);
    proxy = eastl::move(f.proxy);
    detect_3d = MOVE_AND_RESET(f.detect_3d);
    detect_3d_ud = MOVE_AND_RESET(f.detect_3d_ud);

    detect_srgb = MOVE_AND_RESET(f.detect_srgb);
    detect_srgb_ud = MOVE_AND_RESET(f.detect_srgb_ud);

    detect_normal = MOVE_AND_RESET(f.detect_normal);
    detect_normal_ud = MOVE_AND_RESET(f.detect_normal_ud);

    width = MOVE_AND_RESET(f.width);
    height = MOVE_AND_RESET(f.height);
    depth = MOVE_AND_RESET(f.depth);
    alloc_width = MOVE_AND_RESET(f.alloc_width);
    alloc_height = MOVE_AND_RESET(f.alloc_height);
    alloc_depth = MOVE_AND_RESET(f.alloc_depth);
    format = MOVE_AND_RESET(f.format);
    type = MOVE_AND_RESET(f.type);

    target = MOVE_AND_RESET(f.target);
    gl_format_cache = MOVE_AND_RESET(f.gl_format_cache);
    gl_internal_format_cache = MOVE_AND_RESET(f.gl_internal_format_cache);
    gl_type_cache = MOVE_AND_RESET(f.gl_type_cache);
    data_size = MOVE_AND_RESET(f.data_size);
    total_data_size = MOVE_AND_RESET(f.total_data_size);
    mipmaps = MOVE_AND_RESET(f.mipmaps);
    flags = f.flags;
    tex_id = eastl::move(f.tex_id);
    external_tex_id = f.external_tex_id.value;
    f.external_tex_id = {0};
    stored_cube_sides = MOVE_AND_RESET(f.stored_cube_sides);


    compressed = MOVE_AND_RESET(f.compressed);
    srgb = MOVE_AND_RESET(f.srgb);
    ignore_mipmaps = MOVE_AND_RESET(f.ignore_mipmaps);
    active = MOVE_AND_RESET(f.active);
    using_srgb = MOVE_AND_RESET(f.using_srgb);
    redraw_if_visible = MOVE_AND_RESET(f.redraw_if_visible);
    return *this;
}

RasterizerTextureComponent::~RasterizerTextureComponent() {
    tex_id.release();
    external_tex_id = GLNonOwningHandle(0);

    ::unregister_from_proxies(this);
    // record used memory change.
    get_rasterizer_storage_info().texture_mem -= total_data_size;
}
#undef MOVE_AND_RESET
static Ref<Image> _get_gl_image_and_format(const RasterizerStorageGLES3::Config &config,const Ref<Image>& p_image, Image::Format p_format, uint32_t p_flags, Image::Format& r_real_format, GLenum& r_gl_format, GLenum& r_gl_internal_format, GLenum& r_gl_type, bool& r_compressed, bool& r_srgb, bool p_force_decompress) {

    r_compressed = false;
    r_gl_format = 0;
    r_real_format = p_format;
    Ref<Image> image = p_image;
    r_srgb = false;

    bool need_decompress = false;

    switch (p_format) {

    case ImageData::FORMAT_L8: {
        r_gl_internal_format = GL_R8;
        r_gl_format = GL_RED;
        r_gl_type = GL_UNSIGNED_BYTE;
    } break;
    case ImageData::FORMAT_LA8: {
        r_gl_internal_format = GL_RG8;
        r_gl_format = GL_RG;
        r_gl_type = GL_UNSIGNED_BYTE;
    } break;
    case ImageData::FORMAT_R8: {

        r_gl_internal_format = GL_R8;
        r_gl_format = GL_RED;
        r_gl_type = GL_UNSIGNED_BYTE;

    } break;
    case ImageData::FORMAT_RG8: {

        r_gl_internal_format = GL_RG8;
        r_gl_format = GL_RG;
        r_gl_type = GL_UNSIGNED_BYTE;

    } break;
    case ImageData::FORMAT_RGB8: {

        r_gl_internal_format = (config.srgb_decode_supported || (p_flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? GL_SRGB8 : GL_RGB8;
        r_gl_format = GL_RGB;
        r_gl_type = GL_UNSIGNED_BYTE;
        r_srgb = true;

    } break;
    case ImageData::FORMAT_RGBA8: {

        r_gl_format = GL_RGBA;
        r_gl_internal_format = (config.srgb_decode_supported || (p_flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        r_gl_type = GL_UNSIGNED_BYTE;
        r_srgb = true;

    } break;
    case ImageData::FORMAT_RGBA4444: {

        r_gl_internal_format = GL_RGBA4;
        r_gl_format = GL_RGBA;
        r_gl_type = GL_UNSIGNED_SHORT_4_4_4_4;

    } break;
    case ImageData::FORMAT_RGB565: {

        r_gl_internal_format = GL_RGB5_A1;
        r_gl_format = GL_RGBA;
        r_gl_type = GL_UNSIGNED_SHORT_5_5_5_1;

    } break;
    case ImageData::FORMAT_RF: {

        r_gl_internal_format = GL_R32F;
        r_gl_format = GL_RED;
        r_gl_type = GL_FLOAT;

    } break;
    case ImageData::FORMAT_RGF: {

        r_gl_internal_format = GL_RG32F;
        r_gl_format = GL_RG;
        r_gl_type = GL_FLOAT;

    } break;
    case ImageData::FORMAT_RGBF: {

        r_gl_internal_format = GL_RGB32F;
        r_gl_format = GL_RGB;
        r_gl_type = GL_FLOAT;

    } break;
    case ImageData::FORMAT_RGBAF: {

        r_gl_internal_format = GL_RGBA32F;
        r_gl_format = GL_RGBA;
        r_gl_type = GL_FLOAT;

    } break;
    case ImageData::FORMAT_RH: {
        r_gl_internal_format = GL_R32F;
        r_gl_format = GL_RED;
        r_gl_type = GL_HALF_FLOAT;
    } break;
    case ImageData::FORMAT_RGH: {
        r_gl_internal_format = GL_RG32F;
        r_gl_format = GL_RG;
        r_gl_type = GL_HALF_FLOAT;

    } break;
    case ImageData::FORMAT_RGBH: {
        r_gl_internal_format = GL_RGB32F;
        r_gl_format = GL_RGB;
        r_gl_type = GL_HALF_FLOAT;

    } break;
    case ImageData::FORMAT_RGBAH: {
        r_gl_internal_format = GL_RGBA32F;
        r_gl_format = GL_RGBA;
        r_gl_type = GL_HALF_FLOAT;

    } break;
    case ImageData::FORMAT_RGBE9995: {
        r_gl_internal_format = GL_RGB9_E5;
        r_gl_format = GL_RGB;
        r_gl_type = GL_UNSIGNED_INT_5_9_9_9_REV;

    } break;
    case ImageData::FORMAT_DXT1: {

        if (config.s3tc_supported) {

            r_gl_internal_format = (config.srgb_decode_supported || (p_flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            r_gl_format = GL_RGBA;
            r_gl_type = GL_UNSIGNED_BYTE;
            r_compressed = true;
            r_srgb = true;

            } else {

            need_decompress = true;
        }

    } break;
    case ImageData::FORMAT_DXT3: {

        if (config.s3tc_supported) {

            r_gl_internal_format = (config.srgb_decode_supported || (p_flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
            r_gl_format = GL_RGBA;
            r_gl_type = GL_UNSIGNED_BYTE;
            r_compressed = true;
            r_srgb = true;

            } else {

            need_decompress = true;
        }

    } break;
    case ImageData::FORMAT_DXT5: {

        if (config.s3tc_supported) {

            r_gl_internal_format = (config.srgb_decode_supported || (p_flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            r_gl_format = GL_RGBA;
            r_gl_type = GL_UNSIGNED_BYTE;
            r_compressed = true;
            r_srgb = true;

        } else {

            need_decompress = true;
        }

    } break;
    case ImageData::FORMAT_RGTC_R: {

        if (config.rgtc_supported) {

            r_gl_internal_format = GL_COMPRESSED_RED_RGTC1;
            r_gl_format = GL_RGBA;
            r_gl_type = GL_UNSIGNED_BYTE;
            r_compressed = true;

        } else {

            need_decompress = true;
        }

    } break;
    case ImageData::FORMAT_RGTC_RG: {

        if (config.rgtc_supported) {

            r_gl_internal_format = GL_COMPRESSED_RG_RGTC2;
            r_gl_format = GL_RGBA;
            r_gl_type = GL_UNSIGNED_BYTE;
            r_compressed = true;
        } else {

            need_decompress = true;
        }

    } break;
    case ImageData::FORMAT_BPTC_RGBA: {

        if (config.bptc_supported) {

            r_gl_internal_format = (config.srgb_decode_supported || (p_flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM : GL_COMPRESSED_RGBA_BPTC_UNORM;
            r_gl_format = GL_RGBA;
            r_gl_type = GL_UNSIGNED_BYTE;
            r_compressed = true;
            r_srgb = true;

        } else {

            need_decompress = true;
        }
    } break;
    case ImageData::FORMAT_BPTC_RGBF: {

        if (config.bptc_supported) {

            r_gl_internal_format = GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
            r_gl_format = GL_RGB;
            r_gl_type = GL_FLOAT;
            r_compressed = true;
        } else {

            need_decompress = true;
        }
    } break;
    case ImageData::FORMAT_BPTC_RGBFU: {
        if (config.bptc_supported) {

            r_gl_internal_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
            r_gl_format = GL_RGB;
            r_gl_type = GL_FLOAT;
            r_compressed = true;
        } else {

            need_decompress = true;
        }
    } break;
    default: {

        ERR_FAIL_V(Ref<Image>());
    }
    }

    if (need_decompress || p_force_decompress) {

        if (image) {
            image = dynamic_ref_cast<Image>(image->duplicate());
            image->decompress();
            ERR_FAIL_COND_V(image->is_compressed(), image);
            image->convert(ImageData::FORMAT_RGBA8);
        }

        r_gl_format = GL_RGBA;
        r_gl_internal_format = (config.srgb_decode_supported || (p_flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        r_gl_type = GL_UNSIGNED_BYTE;
        r_compressed = false;
        r_real_format = ImageData::FORMAT_RGBA8;
        r_srgb = true;

        return image;
    }

    return image;
}

RenderingEntity RasterizerStorageGLES3::texture_create() {
    auto res = VSG::ecs->create();
    RasterizerTextureComponent& texture(VSG::ecs->registry.emplace<RasterizerTextureComponent>(res));
    texture.tex_id.create();
    texture.self = res;
    texture.active = false;
    texture.total_data_size = 0;

    return res;
}

void RasterizerStorageGLES3::texture_allocate(RenderingEntity p_texture, int p_width, int p_height, int p_depth_3d, Image::Format p_format, RS::TextureType p_type, uint32_t p_flags) {

    GLenum format;
    GLenum internal_format;
    GLenum type;

    bool compressed;
    bool srgb;

    if (p_flags & RS::TEXTURE_FLAG_USED_FOR_STREAMING) {
        p_flags &= ~RS::TEXTURE_FLAG_MIPMAPS; // no mipies for video
    }
    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND(!texture);
    texture->width = p_width;
    texture->height = p_height;
    texture->depth = p_depth_3d;
    texture->format = p_format;
    texture->flags = p_flags;
    texture->stored_cube_sides = 0;

    texture->type = p_type;

    switch (p_type) {
    case RS::TEXTURE_TYPE_2D: {
        texture->target = GL_TEXTURE_2D;
        texture->images.resize(1);
    } break;
    case RS::TEXTURE_TYPE_EXTERNAL: {
        texture->target = GL_TEXTURE_2D; //_GL_TEXTURE_EXTERNAL_OES;
        texture->images.resize(0);
    } break;
    case RS::TEXTURE_TYPE_CUBEMAP: {
        texture->target = GL_TEXTURE_CUBE_MAP;
        texture->images.resize(6);
    } break;
    case RS::TEXTURE_TYPE_2D_ARRAY: {
        texture->target = GL_TEXTURE_2D_ARRAY;
        texture->images.resize(p_depth_3d);
    } break;
    case RS::TEXTURE_TYPE_3D: {
        texture->target = GL_TEXTURE_3D;
        texture->images.resize(p_depth_3d);
    } break;
    }
    if (p_type != RS::TEXTURE_TYPE_EXTERNAL) {
        Image::Format real_format;
        _get_gl_image_and_format(config,Ref<Image>(), texture->format, texture->flags, real_format, format, internal_format, type,
            compressed, srgb, false);

        texture->alloc_width = texture->width;
        texture->alloc_height = texture->height;
        texture->alloc_depth = texture->depth;

        texture->gl_format_cache = format;
        texture->gl_type_cache = type;
        texture->gl_internal_format_cache = internal_format;
        texture->compressed = compressed;
        texture->srgb = srgb;
        texture->data_size = 0;
        texture->mipmaps = 1;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texture->target, texture->tex_id);

    if (p_type == RS::TEXTURE_TYPE_EXTERNAL) {
        glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else if (p_type == RS::TEXTURE_TYPE_3D || p_type == RS::TEXTURE_TYPE_2D_ARRAY) {

        int width = p_width;
        int height = p_height;
        int depth = p_depth_3d;

        int mipmaps = 0;

        while (width > 0 || height > 0 || (p_type == RS::TEXTURE_TYPE_3D && depth > 0)) {
            width = M_MAX(1, width);
            height = M_MAX(1, height);
            depth = M_MAX(1, depth);

            glTexImage3D(texture->target, mipmaps, internal_format, width, height, depth, 0, format, type, nullptr);

            width /= 2;
            height /= 2;

            if (p_type == RS::TEXTURE_TYPE_3D) {
                depth /= 2;
            }

            mipmaps++;

            if (!(p_flags & RS::TEXTURE_FLAG_MIPMAPS)) {
                break;
            }
        }

        glTexParameteri(texture->target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(texture->target, GL_TEXTURE_MAX_LEVEL, mipmaps - 1);

    } else if (p_flags & RS::TEXTURE_FLAG_USED_FOR_STREAMING) {
        //prealloc if video
        glTexImage2D(texture->target, 0, internal_format, p_width, p_height, 0, format, type, nullptr);
    }

    texture->active = true;
}

void RasterizerStorageGLES3::texture_set_data(RenderingEntity p_texture, const Ref<Image>& p_image, int p_layer) {

    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND(!texture);
    ERR_FAIL_COND(!texture->active);
    ERR_FAIL_COND(texture->render_target != entt::null);
    ERR_FAIL_COND(texture->format != p_image->get_format());
    ERR_FAIL_COND(not p_image);
    ERR_FAIL_COND(texture->type == RS::TEXTURE_TYPE_EXTERNAL);

    GLenum type;
    GLenum format;
    GLenum internal_format;
    bool compressed;
    bool srgb;
    if (config.keep_original_textures && !(texture->flags & RS::TEXTURE_FLAG_USED_FOR_STREAMING)) {
        texture->images[p_layer] = p_image;
    }

    Image::Format real_format;
    Ref<Image> img = _get_gl_image_and_format(config, p_image, p_image->get_format(), texture->flags, real_format, format, internal_format, type, compressed, srgb, false);

    if (config.shrink_textures_x2 && (p_image->has_mipmaps() || !p_image->is_compressed()) && !(texture->flags & RS::TEXTURE_FLAG_USED_FOR_STREAMING)) {

        texture->alloc_height = M_MAX(1, texture->alloc_height / 2);
        texture->alloc_width = M_MAX(1, texture->alloc_width / 2);

        if (texture->alloc_width == img->get_width() / 2 && texture->alloc_height == img->get_height() / 2) {

            img->shrink_x2();
        } else if (img->get_format() <= ImageData::FORMAT_RGBA8) {

            img->resize(texture->alloc_width, texture->alloc_height, Image::INTERPOLATE_BILINEAR);
        }
    }

    GLenum blit_target = GL_TEXTURE_2D;

    switch (texture->type) {
    case RS::TEXTURE_TYPE_2D:
    case RS::TEXTURE_TYPE_EXTERNAL: {
        blit_target = GL_TEXTURE_2D;
    } break;
    case RS::TEXTURE_TYPE_CUBEMAP: {
        ERR_FAIL_INDEX(p_layer, 6);
        blit_target = _cube_side_enum[p_layer];
    } break;
    case RS::TEXTURE_TYPE_2D_ARRAY: {
        blit_target = GL_TEXTURE_2D_ARRAY;
    } break;
    case RS::TEXTURE_TYPE_3D: {
        blit_target = GL_TEXTURE_3D;
    } break;
    }

    texture->data_size = img->get_data().size();
    PoolVector<uint8_t>::Read read = img->get_data().read();
    ERR_FAIL_COND(!read.ptr());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texture->target, texture->tex_id);

    texture->ignore_mipmaps = compressed && !img->has_mipmaps();

    if ((texture->flags & RS::TEXTURE_FLAG_MIPMAPS) && !texture->ignore_mipmaps) {
        if (texture->flags & RS::TEXTURE_FLAG_FILTER) {
            glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, config.use_fast_texture_filter ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
        } else {
            glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, config.use_fast_texture_filter ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR);
        }
    } else {
        if (texture->flags & RS::TEXTURE_FLAG_FILTER) {
            glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        } else {
            glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }
    }

    if (config.srgb_decode_supported && srgb) {

        if (texture->flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR) {

            glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _DECODE_EXT);
            texture->using_srgb = true;
        } else {
            glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _SKIP_DECODE_EXT);
            texture->using_srgb = false;
        }
    }

    if (texture->flags & RS::TEXTURE_FLAG_FILTER) {

        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Linear Filtering

    } else {

        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // raw Filtering
    }

    if (((texture->flags & RS::TEXTURE_FLAG_REPEAT) || (texture->flags & RS::TEXTURE_FLAG_MIRRORED_REPEAT)) && texture->target != GL_TEXTURE_CUBE_MAP) {

        if (texture->flags & RS::TEXTURE_FLAG_MIRRORED_REPEAT) {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        } else {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
    } else {

        //glTexParameterf( texture->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
        glTexParameterf(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    //set swizle for older format compatibility
    switch (texture->format) {

    case ImageData::FORMAT_L8: {
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_B, GL_RED);
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_A, GL_ONE);

    } break;
    case ImageData::FORMAT_LA8: {

        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_B, GL_RED);
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
    } break;
    default: {
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
        glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

    } break;
    }

    if (config.use_anisotropic_filter) {

        if (texture->flags & RS::TEXTURE_FLAG_ANISOTROPIC_FILTER) {

            glTexParameterf(texture->target, GL_TEXTURE_MAX_ANISOTROPY, config.anisotropic_level);
        } else {
            glTexParameterf(texture->target, GL_TEXTURE_MAX_ANISOTROPY, 1);
        }
    }

    int mipmaps = ((texture->flags & RS::TEXTURE_FLAG_MIPMAPS) && img->has_mipmaps()) ? img->get_mipmap_count() + 1 : 1;

    int w = img->get_width();
    int h = img->get_height();

    int tsize = 0;

    for (int i = 0; i < mipmaps; i++) {

        int size, ofs;
        img->get_mipmap_offset_and_size(i, ofs, size);

        if (texture->type == RS::TEXTURE_TYPE_2D || texture->type == RS::TEXTURE_TYPE_CUBEMAP) {

            if (texture->compressed) {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

                int bw = w;
                int bh = h;

                glCompressedTexImage2D(blit_target, i, internal_format, bw, bh, 0, size, &read[ofs]);

            } else {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                if (texture->flags & RS::TEXTURE_FLAG_USED_FOR_STREAMING) {
                    glTexSubImage2D(blit_target, i, 0, 0, w, h, format, type, &read[ofs]);
                } else {
                    glTexImage2D(blit_target, i, internal_format, w, h, 0, format, type, &read[ofs]);
                }
            }
        } else {
            if (texture->compressed) {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

                int bw = w;
                int bh = h;

                glCompressedTexSubImage3D(blit_target, i, 0, 0, p_layer, bw, bh, 1, internal_format, size, &read[ofs]);
            } else {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                glTexSubImage3D(blit_target, i, 0, 0, p_layer, w, h, 1, format, type, &read[ofs]);
            }
        }
        tsize += size;

        w = M_MAX(1, w >> 1);
        h = M_MAX(1, h >> 1);
    }

    // Handle array and 3D textures, as those set their data per layer.
    tsize *= M_MAX(texture->alloc_depth, 1);

    get_rasterizer_storage_info().texture_mem -= texture->total_data_size;
    texture->total_data_size = tsize;
    get_rasterizer_storage_info().texture_mem += texture->total_data_size;

    //printf("texture: %i x %i - size: %i - total: %i\n",texture->width,texture->height,tsize,_rinfo.texture_mem);

    texture->stored_cube_sides |= (1 << p_layer);

    if ((texture->type == RS::TEXTURE_TYPE_2D || texture->type == RS::TEXTURE_TYPE_CUBEMAP) && (texture->flags & RS::TEXTURE_FLAG_MIPMAPS) && mipmaps == 1 && !texture->ignore_mipmaps && (texture->type != RS::TEXTURE_TYPE_CUBEMAP || texture->stored_cube_sides == (1 << 6) - 1)) {
        //generate mipmaps if they were requested and the image does not contain them
        glGenerateMipmap(texture->target);
    } else if (mipmaps > 1) {
        glTexParameteri(texture->target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(texture->target, GL_TEXTURE_MAX_LEVEL, mipmaps - 1);
    } else {
        glTexParameteri(texture->target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(texture->target, GL_TEXTURE_MAX_LEVEL, 0);
    }

    texture->mipmaps = mipmaps;

    //texture_set_flags(p_texture,texture->flags);
}

// Uploads pixel data to a sub-region of a texture, for the specified mipmap.
// The texture pixels must have been allocated before, because most features seen in texture_set_data() make no sense in a partial update.
// TODO If we want this to be usable without pre-filling pixels with a full image, we have to call glTexImage2D() with null data.
void RasterizerStorageGLES3::texture_set_data_partial(RenderingEntity p_texture, const Ref<Image>& p_image, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, int p_dst_mip, int p_layer) {

    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND(!texture);
    ERR_FAIL_COND(!texture->active);
    ERR_FAIL_COND(texture->render_target != entt::null);
    ERR_FAIL_COND(texture->format != p_image->get_format());
    ERR_FAIL_COND(not p_image);
    ERR_FAIL_COND(src_w <= 0 || src_h <= 0);
    ERR_FAIL_COND(src_x < 0 || src_y < 0 || src_x + src_w > p_image->get_width() || src_y + src_h > p_image->get_height());
    ERR_FAIL_COND(dst_x < 0 || dst_y < 0 || dst_x + src_w > texture->alloc_width || dst_y + src_h > texture->alloc_height);
    ERR_FAIL_COND(p_dst_mip < 0 || p_dst_mip >= texture->mipmaps);
    ERR_FAIL_COND(texture->type == RS::TEXTURE_TYPE_EXTERNAL);

    GLenum type;
    GLenum format;
    GLenum internal_format;
    bool compressed;
    bool srgb;

    // Because OpenGL wants data as a dense array, we have to extract the sub-image if the source rect isn't the full image
    Ref<Image> p_sub_img = p_image;
    if (src_x > 0 || src_y > 0 || src_w != p_image->get_width() || src_h != p_image->get_height()) {
        p_sub_img = p_image->get_rect(Rect2(src_x, src_y, src_w, src_h));
    }

    Image::Format real_format;
    Ref<Image> img = _get_gl_image_and_format(config, p_sub_img, p_sub_img->get_format(), texture->flags, real_format, format, internal_format, type, compressed, srgb, false);

    GLenum blit_target = GL_TEXTURE_2D;

    switch (texture->type) {
    case RS::TEXTURE_TYPE_2D:
    case RS::TEXTURE_TYPE_EXTERNAL: {
        blit_target = GL_TEXTURE_2D;
    } break;
    case RS::TEXTURE_TYPE_CUBEMAP: {
        ERR_FAIL_INDEX(p_layer, 6);
        blit_target = _cube_side_enum[p_layer];
    } break;
    case RS::TEXTURE_TYPE_2D_ARRAY: {
        blit_target = GL_TEXTURE_2D_ARRAY;
    } break;
    case RS::TEXTURE_TYPE_3D: {
        blit_target = GL_TEXTURE_3D;
    } break;
    }

    PoolVector<uint8_t>::Read read = img->get_data().read();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texture->target, texture->tex_id);

    int src_data_size = img->get_data().size();
    int src_ofs = 0;

    if (texture->type == RS::TEXTURE_TYPE_2D || texture->type == RS::TEXTURE_TYPE_CUBEMAP) {
        if (texture->compressed) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glCompressedTexSubImage2D(blit_target, p_dst_mip, dst_x, dst_y, src_w, src_h, internal_format, src_data_size, &read[src_ofs]);

        } else {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            // `format` has to match the internal_format used when the texture was created
            glTexSubImage2D(blit_target, p_dst_mip, dst_x, dst_y, src_w, src_h, format, type, &read[src_ofs]);
        }
    } else {
        if (texture->compressed) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glCompressedTexSubImage3D(blit_target, p_dst_mip, dst_x, dst_y, p_layer, src_w, src_h, 1, format, src_data_size, &read[src_ofs]);
        } else {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            // `format` has to match the internal_format used when the texture was created
            glTexSubImage3D(blit_target, p_dst_mip, dst_x, dst_y, p_layer, src_w, src_h, 1, format, type, &read[src_ofs]);
        }
    }

    if (texture->flags & RS::TEXTURE_FLAG_FILTER) {

        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Linear Filtering

    } else {

        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // raw Filtering
    }
}

Ref<Image> RasterizerStorageGLES3::texture_get_data(RenderingEntity p_texture, int p_layer) const {

    auto *texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND_V(!texture, Ref<Image>());
    ERR_FAIL_COND_V(!texture->active, Ref<Image>());
    ERR_FAIL_COND_V(texture->data_size == 0 && texture->render_target == entt::null, Ref<Image>());

    if (texture->type == RS::TEXTURE_TYPE_CUBEMAP && p_layer < 6 && texture->images[p_layer]) {
        return texture->images[p_layer];
    }

    // 3D textures and 2D texture arrays need special treatment, as the glGetTexImage reads **the whole**
    // texture to host-memory. 3D textures and 2D texture arrays are potentially very big, so reading
    // everything just to throw everything but one layer away is A Bad Idea.
    //
    // Unfortunately, to solve this, the copy shader has to read the data out via a shader and store it
    // in a temporary framebuffer. The data from the framebuffer can then be read using glReadPixels.
    if (texture->type == RS::TEXTURE_TYPE_2D_ARRAY || texture->type == RS::TEXTURE_TYPE_3D) {
        // can't read a layer that doesn't exist
        ERR_FAIL_INDEX_V(p_layer, texture->alloc_depth, Ref<Image>());

        // get some information about the texture
        Image::Format real_format;
        GLenum gl_format;
        GLenum gl_internal_format;
        GLenum gl_type;

        bool compressed;
        bool srgb;

        _get_gl_image_and_format(config, Ref<Image>(),
            texture->format, texture->flags,
            real_format, gl_format,
            gl_internal_format, gl_type,
            compressed, srgb, false);

        PoolVector<uint8_t> data;

        // TODO need to decide between RgbaUnorm and RgbaFloat32 for output
        int data_size = Image::get_image_data_size(texture->alloc_width, texture->alloc_height, ImageData::FORMAT_RGBA8, false);

        data.resize(data_size * 2); // add some more memory at the end, just in case for buggy drivers
        PoolVector<uint8_t>::Write wb = data.write();

        // generate temporary resources
        GLuint tmp_fbo;
        glGenFramebuffers(1, &tmp_fbo);

        GLuint tmp_color_attachment;
        glGenTextures(1, &tmp_color_attachment);

        // now bring the OpenGL context into the correct state
        {
            glBindFramebuffer(GL_FRAMEBUFFER, tmp_fbo);

            // back color attachment with memory, then set properties
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tmp_color_attachment);
            // TODO support HDR properly
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->alloc_width, texture->alloc_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // use the color texture as color attachment for this render pass
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_color_attachment, 0);

            // more GL state, wheeeey
            glDepthMask(GL_FALSE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glDepthFunc(GL_LEQUAL);
            glColorMask(1, 1, 1, 1);

            // use volume tex for reading
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(texture->target, texture->tex_id);

            glViewport(0, 0, texture->alloc_width, texture->alloc_height);

            // set up copy shader for proper use
            shaders.copy.set_conditional(CopyShaderGLES3::LINEAR_TO_SRGB, !srgb);
            shaders.copy.set_conditional(CopyShaderGLES3::USE_TEXTURE3D, texture->type == RS::TEXTURE_TYPE_3D);
            shaders.copy.set_conditional(CopyShaderGLES3::USE_TEXTURE2DARRAY, texture->type == RS::TEXTURE_TYPE_2D_ARRAY);
            shaders.copy.bind();

            float layer;
            if (texture->type == RS::TEXTURE_TYPE_2D_ARRAY) {
                layer = (float)p_layer;
            } else {
                // calculate the normalized z coordinate for the layer
                layer = (float)p_layer / (float)texture->alloc_depth;

            }
            shaders.copy.set_uniform(CopyShaderGLES3::LAYER, layer);

            glBindVertexArray(resources.quadie_array);
        }

        // clear color attachment, then perform copy
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // read the image into the host buffer
        glReadPixels(0, 0, texture->alloc_width, texture->alloc_height, GL_RGBA, GL_UNSIGNED_BYTE, &wb[0]);

        // remove temp resources and unset some GL state
        {
            shaders.copy.set_conditional(CopyShaderGLES3::USE_TEXTURE3D, false);
            shaders.copy.set_conditional(CopyShaderGLES3::USE_TEXTURE2DARRAY, false);
            shaders.copy.set_conditional(CopyShaderGLES3::LINEAR_TO_SRGB, false);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glDeleteTextures(1, &tmp_color_attachment);
            glDeleteFramebuffers(1, &tmp_fbo);
        }

        wb.release();

        data.resize(data_size);

        Image* img = memnew(Image(texture->alloc_width, texture->alloc_height, false, ImageData::FORMAT_RGBA8, data));
        if (!texture->compressed) {
            img->convert(real_format);
        }

        return Ref<Image>(img, DoNotAddRef);
    }

    Image::Format real_format;
    GLenum gl_format;
    GLenum gl_internal_format;
    GLenum gl_type;
    bool compressed;
    bool srgb;
    _get_gl_image_and_format(config, Ref<Image>(), texture->format, texture->flags, real_format, gl_format,
            gl_internal_format, gl_type, compressed, srgb, false);

    PoolVector<uint8_t> data;

    int data_size = Image::get_image_data_size(texture->alloc_width, texture->alloc_height, real_format, texture->mipmaps > 1);

    data.resize(data_size * 2); //add some memory at the end, just in case for buggy drivers
    PoolVector<uint8_t>::Write wb = data.write();

    glActiveTexture(GL_TEXTURE0);

    glBindTexture(texture->target, texture->tex_id);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    for (int i = 0; i < texture->mipmaps; i++) {

        int ofs = Image::get_image_mipmap_offset(texture->alloc_width, texture->alloc_height, real_format, i);

        if (texture->compressed) {

            glPixelStorei(GL_PACK_ALIGNMENT, 4);
            glGetCompressedTexImage(texture->target, i, &wb[ofs]);

        } else {

            glPixelStorei(GL_PACK_ALIGNMENT, 1);

            glGetTexImage(texture->target, i, texture->gl_format_cache, texture->gl_type_cache, &wb[ofs]);
        }
    }

    Image::Format img_format;

    //convert special case RGB10_A2 to RGBA8 because it's not a supported image format
    if (texture->gl_internal_format_cache == GL_RGB10_A2) {

        img_format = ImageData::FORMAT_RGBA8;

        uint32_t* ptr = (uint32_t*)wb.ptr();
        uint32_t num_pixels = data_size / 4;

        for (uint32_t ofs = 0; ofs < num_pixels; ofs++) {
            uint32_t px = ptr[ofs];
            uint32_t a = px >> 30 & 0xFF;

            ptr[ofs] = (px >> 2 & 0xFF) |
                (px >> 12 & 0xFF) << 8 |
                (px >> 22 & 0xFF) << 16 |
                (a | a << 2 | a << 4 | a << 6) << 24;
        }
    } else {
        img_format = real_format;
    }

    wb.release();

    data.resize(data_size);

    return make_ref_counted<Image>(texture->alloc_width, texture->alloc_height, texture->mipmaps > 1, img_format, data);
}

void RasterizerStorageGLES3::texture_set_shrink_all_x2_on_set_data(bool p_enable) {

    config.shrink_textures_x2 = p_enable;
}

void RasterizerStorageGLES3::textures_keep_original(bool p_enable) {

    config.keep_original_textures = p_enable;
}
void rt_texture_set_flags(RasterizerTextureComponent *texture, uint32_t p_flags,bool use_anisotropic,bool use_fast_texture_filter,int anisotropic_level,bool srgb_decode_supported) {
    if (texture->render_target!=entt::null) {
        // only allow filter and repeat flags for render target (ie. viewport) textures
        p_flags &= (RS::TEXTURE_FLAG_FILTER | RS::TEXTURE_FLAG_REPEAT);
    }

    texture->flags = p_flags;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texture->target, texture->get_texture_id());

    //glTexParameterf( texture->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
    glTexParameterf(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (use_anisotropic) {
        glTexParameterf(texture->target, GL_TEXTURE_MAX_ANISOTROPY, 1);
    }

    if (texture->flags & RS::TEXTURE_FLAG_FILTER) {
        glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    if (srgb_decode_supported && texture->srgb) {

        glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _SKIP_DECODE_EXT);
        texture->using_srgb = false;
    }

    if (texture->flags & RS::TEXTURE_FLAG_FILTER) {

        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Linear Filtering

    } else {

        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // raw Filtering
    }
}

void texture_set_flags(RasterizerTextureComponent *texture, uint32_t p_flags,bool use_anisotropic,bool use_fast_texture_filter,int anisotropic_level,bool srgb_decode_supported) {
    if (texture->render_target != entt::null) {

        // only allow filter and repeat flags for render target (ie. viewport) textures
        p_flags &= (RS::TEXTURE_FLAG_FILTER | RS::TEXTURE_FLAG_REPEAT);
    }

    const bool had_mipmaps = texture->flags & RS::TEXTURE_FLAG_MIPMAPS;

    texture->flags = p_flags;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texture->target, texture->get_texture_id());

    int wrap_mode;
    if (((texture->flags & RS::TEXTURE_FLAG_REPEAT) || (texture->flags & RS::TEXTURE_FLAG_MIRRORED_REPEAT)) &&
        texture->target != GL_TEXTURE_CUBE_MAP) {

        if (texture->flags & RS::TEXTURE_FLAG_MIRRORED_REPEAT) {
            wrap_mode = GL_MIRRORED_REPEAT;
        } else {
            wrap_mode = GL_REPEAT;
        }
    } else {
        wrap_mode = GL_CLAMP_TO_EDGE;
        //glTexParameterf( texture->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
    }

    glTexParameterf(texture->target, GL_TEXTURE_WRAP_S, wrap_mode);
    glTexParameterf(texture->target, GL_TEXTURE_WRAP_T, wrap_mode);
    if (use_anisotropic) {

        int calculated_anisotropy_level = 1;
        if (texture->flags & RS::TEXTURE_FLAG_ANISOTROPIC_FILTER) {

            calculated_anisotropy_level = anisotropic_level;
        }
        glTexParameterf(texture->target, GL_TEXTURE_MAX_ANISOTROPY, calculated_anisotropy_level);
    }
    int min_filter_type;

    if ((texture->flags & RS::TEXTURE_FLAG_MIPMAPS) && !texture->ignore_mipmaps) {
        if (!had_mipmaps && texture->mipmaps == 1) {
            glGenerateMipmap(texture->target);
        }
        if (texture->flags & RS::TEXTURE_FLAG_FILTER) {
            min_filter_type = use_fast_texture_filter ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
        } else {
            min_filter_type = use_fast_texture_filter ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
        }

    } else {
        if (texture->flags & RS::TEXTURE_FLAG_FILTER) {
            min_filter_type = GL_LINEAR;
        } else {
            min_filter_type = GL_NEAREST;
        }
    }
    glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, min_filter_type);

    if (srgb_decode_supported && texture->srgb) {

        if (texture->flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR) {

            glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _DECODE_EXT);
            texture->using_srgb = true;
        } else {
            glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _SKIP_DECODE_EXT);
            texture->using_srgb = false;
        }
    }

    int mag_filter_type;
    if (texture->flags & RS::TEXTURE_FLAG_FILTER) {
        mag_filter_type = GL_LINEAR; // Linear Filtering
    }  else {
        mag_filter_type = GL_NEAREST; // raw Filtering
    }

    glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, mag_filter_type); // Linear Filtering
}

void RasterizerStorageGLES3::texture_set_flags(RenderingEntity p_texture, uint32_t p_flags) {
    auto *texture(get<RasterizerTextureComponent>(p_texture));

    ERR_FAIL_COND(!texture);
    ::texture_set_flags(texture, p_flags, config.use_anisotropic_filter, config.use_fast_texture_filter,
            config.anisotropic_level, config.srgb_decode_supported);
}

uint32_t RasterizerStorageGLES3::texture_get_flags(RenderingEntity p_texture) const {

    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND_V(!texture, 0);

    return texture->flags;
}
Image::Format RasterizerStorageGLES3::texture_get_format(RenderingEntity p_texture) const {

    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND_V(!texture, ImageData::FORMAT_L8);

    return texture->format;
}

RS::TextureType RasterizerStorageGLES3::texture_get_type(RenderingEntity p_texture) const {
    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND_V(!texture, RS::TEXTURE_TYPE_2D);

    return texture->type;
}
uint32_t RasterizerStorageGLES3::texture_get_texid(RenderingEntity p_texture) const {

    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND_V(!texture, 0);

    return texture->tex_id;
}
void RasterizerStorageGLES3::texture_bind(RenderingEntity p_texture, uint32_t p_texture_no) {

    RasterizerTextureComponent* texture = get<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND(!texture);

    glActiveTexture(GL_TEXTURE0 + p_texture_no);
    glBindTexture(texture->target, texture->tex_id);
}
uint32_t RasterizerStorageGLES3::texture_get_width(RenderingEntity p_texture) const {

    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND_V(!texture, 0);

    return texture->width;
}
uint32_t RasterizerStorageGLES3::texture_get_height(RenderingEntity p_texture) const {

    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND_V(!texture, 0);

    return texture->height;
}

uint32_t RasterizerStorageGLES3::texture_get_depth(RenderingEntity p_texture) const {

    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND_V(!texture, 0);

    return texture->depth;
}

void RasterizerStorageGLES3::texture_set_size_override(RenderingEntity p_texture, int p_width, int p_height, int p_depth) {

    auto *texture = getUnchecked<RasterizerTextureComponent>(p_texture);

    ERR_FAIL_COND(!texture);
    ERR_FAIL_COND(texture->render_target != entt::null);

    ERR_FAIL_COND(p_width <= 0 || p_width > 16384);
    ERR_FAIL_COND(p_height <= 0 || p_height > 16384);
    //real texture size is in alloc width and height
    texture->width = p_width;
    texture->height = p_height;
}

void RasterizerStorageGLES3::texture_set_path(RenderingEntity p_texture, StringView p_path) {
    auto *texture = getUnchecked<RasterizerTextureComponent>(p_texture);
    ERR_FAIL_COND(!texture);

    texture->path = p_path;
}

const String& RasterizerStorageGLES3::texture_get_path(RenderingEntity p_texture) const {

    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);
    ERR_FAIL_COND_V(!texture, null_string);
    return texture->path;
}
void RasterizerStorageGLES3::texture_debug_usage(Vector<RenderingServer::TextureInfo>* r_info) {

    const auto textures(VSG::ecs->registry.view<RasterizerTextureComponent>());

    textures.each([r_info](const RenderingEntity ent,const RasterizerTextureComponent &t) {
            RenderingServer::TextureInfo tinfo;
            tinfo.texture = ent;
            tinfo.path = t.path;
            tinfo.format = t.format;
            tinfo.width = t.alloc_width;
            tinfo.height = t.alloc_height;
            tinfo.depth = t.alloc_depth;
            tinfo.bytes = t.total_data_size;
            r_info->push_back(tinfo);
    });
}

void RasterizerStorageGLES3::texture_set_detect_3d_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void *p_userdata) {

    RasterizerTextureComponent* texture = getUnchecked<RasterizerTextureComponent>(p_texture);
    ERR_FAIL_COND(!texture);

    texture->detect_3d = p_callback;
    texture->detect_3d_ud = p_userdata;
}

void RasterizerStorageGLES3::texture_set_detect_srgb_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void* p_userdata) {
    auto* texture = getUnchecked<RasterizerTextureComponent>(p_texture);
    ERR_FAIL_COND(!texture);

    texture->detect_srgb = p_callback;
    texture->detect_srgb_ud = p_userdata;
}

void RasterizerStorageGLES3::texture_set_detect_normal_callback(RenderingEntity p_texture, RenderingServer::TextureDetectCallback p_callback, void* p_userdata) {
    auto* texture = getUnchecked<RasterizerTextureComponent>(p_texture);
    ERR_FAIL_COND(!texture);

    texture->detect_normal = p_callback;
    texture->detect_normal_ud = p_userdata;
}

RenderingEntity RasterizerStorageGLES3::texture_create_radiance_cubemap(RenderingEntity p_source, int p_resolution) const {

    auto* texture = getUnchecked<RasterizerTextureComponent>(p_source);
    ERR_FAIL_COND_V(!texture, entt::null);
    ERR_FAIL_COND_V(texture->type != RS::TEXTURE_TYPE_CUBEMAP, entt::null);

    const bool use_float = config.framebuffer_half_float_supported;

    if (p_resolution < 0) {
        p_resolution = texture->width;
    }

    glBindVertexArray(0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texture->target, texture->tex_id);

    if (config.srgb_decode_supported && texture->srgb && !texture->using_srgb) {

        glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _DECODE_EXT);
        texture->using_srgb = true;
#ifdef TOOLS_ENABLED
        if (!(texture->flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) {
            texture->flags |= RS::TEXTURE_FLAG_CONVERT_TO_LINEAR;
            //notify that texture must be set to linear beforehand, so it works in other platforms when exported
        }
#endif
    }

    glActiveTexture(GL_TEXTURE1);
    GLTextureHandle new_cubemap;
    new_cubemap.create();
    glBindTexture(GL_TEXTURE_CUBE_MAP, new_cubemap);

    GLuint tmp_fb;

    glGenFramebuffers(1, &tmp_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);

    int size = p_resolution;

    int lod = 0;

    shaders.cubemap_filter.bind();

    constexpr int mipmaps = 6;

    int mm_level = mipmaps;

    const GLenum internal_format = use_float ? GL_RGBA16F : GL_RGB10_A2;
    constexpr GLenum format = GL_RGBA;
    const GLenum type = use_float ? GL_HALF_FLOAT : GL_UNSIGNED_INT_2_10_10_10_REV;

    while (mm_level) {

        for (const GLenum side : _cube_side_enum) {
            glTexImage2D(side, lod, internal_format, size, size, 0, format, type, nullptr);
        }

        lod++;
        mm_level--;

        if (size > 1) {
            size >>= 1;
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, lod - 1);

    lod = 0;
    mm_level = mipmaps;

    size = p_resolution;

    shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, false);

    while (mm_level) {

        for (int i = 0; i < 6; i++) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _cube_side_enum[i], new_cubemap, lod);

            glViewport(0, 0, size, size);
            glBindVertexArray(resources.quadie_array);

            shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::FACE_ID, i);
            shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::ROUGHNESS, lod / float(mipmaps - 1));

            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glBindVertexArray(0);
#ifdef DEBUG_ENABLED
            const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            ERR_CONTINUE(status != GL_FRAMEBUFFER_COMPLETE);
#endif
        }

        if (size > 1) {
            size >>= 1;
        }
        lod++;
        mm_level--;
    }

    //restore ranges
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, lod - 1);

    glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
    glDeleteFramebuffers(1, &tmp_fb);


    const auto res = VSG::ecs->create();
    RasterizerTextureComponent& ctex(VSG::ecs->registry.emplace<RasterizerTextureComponent>(res));

    ctex.self = res;
    ctex.type = RS::TEXTURE_TYPE_CUBEMAP;
    ctex.flags = RS::TEXTURE_FLAG_MIPMAPS | RS::TEXTURE_FLAG_FILTER;
    ctex.width = p_resolution;
    ctex.height = p_resolution;
    ctex.alloc_width = p_resolution;
    ctex.alloc_height = p_resolution;
    ctex.format = use_float ? ImageData::FORMAT_RGBAH : ImageData::FORMAT_RGBA8;
    ctex.target = GL_TEXTURE_CUBE_MAP;
    ctex.gl_format_cache = format;
    ctex.gl_internal_format_cache = internal_format;
    ctex.gl_type_cache = type;
    ctex.data_size = 0;
    ctex.compressed = false;
    ctex.srgb = false;
    ctex.total_data_size = 0;
    ctex.ignore_mipmaps = false;
    ctex.mipmaps = mipmaps;
    ctex.active = true;
    ctex.tex_id = eastl::move(new_cubemap);
    ctex.stored_cube_sides = (1 << 6) - 1;
    ctex.render_target = entt::null;

    return res;
}

Size2 RasterizerStorageGLES3::texture_size_with_proxy(RenderingEntity p_texture) const {

    const auto *texture(get<RasterizerTextureComponent>(p_texture));
    ERR_FAIL_COND_V(!texture, Size2());
    const auto *tex_proxy = get<RasterizerTextureComponent>(texture->proxy);

    if (tex_proxy) {
        return Size2(tex_proxy->width, tex_proxy->height);
    }
    return Size2(texture->width, texture->height);
}

void RasterizerStorageGLES3::texture_set_proxy(RenderingEntity p_texture, RenderingEntity p_proxy) {

    auto* texture = getUnchecked<RasterizerTextureComponent>(p_texture);
    ERR_FAIL_COND(!texture);
    auto *tex_proxy = get<RasterizerTextureComponent>(texture->proxy);

    if (tex_proxy) {
        tex_proxy->proxy_owners.erase(p_texture);
        texture->proxy = entt::null;
    }

    RasterizerTextureComponent* proxy_texture = get<RasterizerTextureComponent>(p_proxy);
    if (proxy_texture) {
        ERR_FAIL_COND(proxy_texture == texture);
        proxy_texture->proxy_owners.insert(p_texture);
        texture->proxy = p_proxy;
    }
}

void RasterizerStorageGLES3::texture_set_force_redraw_if_visible(RenderingEntity p_texture, bool p_enable) {

    auto* texture = getUnchecked<RasterizerTextureComponent>(p_texture);
    ERR_FAIL_COND(!texture);
    texture->redraw_if_visible = p_enable;
}
