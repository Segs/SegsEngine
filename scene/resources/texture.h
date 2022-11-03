/*************************************************************************/
/*  texture.h                                                            */
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
#include "core/math/rect2.h"
#include "core/os/rw_lock.h"
#include "core/resource.h"
#include "core/rid.h"
#include "scene/resources/gradient.h"
//
#include "servers/rendering_server_enums.h"

#include <EASTL/unique_ptr.h>

class GODOT_EXPORT Texture : public Resource {

    GDCLASS(Texture,Resource)

    OBJ_SAVE_TYPE(Texture) //children are all saved as Texture, so they can be exchanged
protected:
    static void _bind_methods();

public:
    enum Flags {
        FLAG_MIPMAPS = RS::TEXTURE_FLAG_MIPMAPS,
        FLAG_REPEAT = RS::TEXTURE_FLAG_REPEAT,
        FLAG_FILTER = RS::TEXTURE_FLAG_FILTER,
        FLAG_ANISOTROPIC_FILTER = RS::TEXTURE_FLAG_ANISOTROPIC_FILTER,
        FLAG_CONVERT_TO_LINEAR = RS::TEXTURE_FLAG_CONVERT_TO_LINEAR,
        FLAG_VIDEO_SURFACE = RS::TEXTURE_FLAG_USED_FOR_STREAMING,
        FLAGS_DEFAULT = FLAG_MIPMAPS | FLAG_REPEAT | FLAG_FILTER,
        FLAG_MIRRORED_REPEAT = RS::TEXTURE_FLAG_MIRRORED_REPEAT
    };

    virtual int get_width() const = 0;
    virtual int get_height() const = 0;
    virtual Size2 get_size() const;
    RenderingEntity get_rid() const override = 0;

    virtual bool is_pixel_opaque(int p_x, int p_y) const;

    virtual bool has_alpha() const = 0;

    virtual void set_flags(uint32_t p_flags) = 0;
    virtual uint32_t get_flags() const = 0;

    virtual void draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate = Color(1, 1, 1),
            bool p_transpose = false, const Ref<Texture> &p_normal_map = Ref<Texture>()) const;
    virtual void draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile = false,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>()) const;
    virtual void draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>(), bool p_clip_uv = true) const;
    virtual bool get_rect_region(const Rect2 &p_rect, const Rect2 &p_src_rect, Rect2 &r_rect, Rect2 &r_src_rect) const;

    virtual Ref<Image> get_data() const { return Ref<Image>(); }

    Texture();
};


class BitMap;

class GODOT_EXPORT ImageTexture : public Texture {

    GDCLASS(ImageTexture,Texture)

    RES_BASE_EXTENSION("tex")

public:
    enum Storage {
        STORAGE_RAW,
        STORAGE_COMPRESS_LOSSY,
        STORAGE_COMPRESS_LOSSLESS
    };

private:
    RenderingEntity texture;
    Image::Format format;
    uint32_t flags;
    int w, h;
    Storage storage;
    Size2 size_override;
    float lossy_storage_quality;
    mutable eastl::unique_ptr<BitMap> alpha_cache;
    bool image_stored;

protected:
    void reload_from_file() override;

    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(Vector<PropertyInfo> *p_list) const;

    void _reload_hook(RenderingEntity p_hook);
    void _resource_path_changed() override;
    static void _bind_methods();

    void _set_data(Dictionary p_data);

public:
    void create(int p_width, int p_height, Image::Format p_format, uint32_t p_flags = FLAGS_DEFAULT);
    void create_from_image(const Ref<Image> &p_image, uint32_t p_flags = FLAGS_DEFAULT);

    void set_flags(uint32_t p_flags) override;
    uint32_t get_flags() const override;
    Image::Format get_format() const;
    void set_data(const Ref<Image> &p_image);
    Ref<Image> get_data() const override;

    int get_width() const override;
    int get_height() const override;

    RenderingEntity get_rid() const override;

    bool has_alpha() const override;
    void draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate = Color(1, 1, 1),
            bool p_transpose = false, const Ref<Texture> &p_normal_map = Ref<Texture>()) const override;
    void draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile = false,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>()) const override;
    void draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>(), bool p_clip_uv = true) const override;
    void set_storage(Storage p_storage);
    Storage get_storage() const;

    bool is_pixel_opaque(int p_x, int p_y) const override;

    void set_lossy_storage_quality(float p_lossy_storage_quality);
    float get_lossy_storage_quality() const;

    void set_size_override(const Size2 &p_size);

    void set_path(StringView p_path, bool p_take_over = false) override;

    ImageTexture();
    ~ImageTexture() override;
};

class GODOT_EXPORT StreamTexture : public Texture {

    GDCLASS(StreamTexture,Texture)

public:

    enum FormatBits {
        FORMAT_MASK_IMAGE_FORMAT = (1 << 20) - 1,
        FORMAT_BIT_PNG = 1 << 20,
        FORMAT_BIT_WEBP = 1 << 21,
        FORMAT_BIT_STREAM = 1 << 22,
        FORMAT_BIT_HAS_MIPMAPS = 1 << 23,
        FORMAT_BIT_DETECT_3D = 1 << 24,
        FORMAT_BIT_DETECT_SRGB = 1 << 25,
        FORMAT_BIT_DETECT_NORMAL = 1 << 26,
    };

private:
    Error _load_data(StringView p_path, int &tw, int &th, int &tw_custom, int &th_custom, int &flags, Ref<Image> &image,
            int p_size_limit = 0);
    struct StreamTextureData;
    StreamTextureData *m_impl_data;

    void reload_from_file() override;

    static void _requested_3d(void *p_ud);
    static void _requested_srgb(void *p_ud);
    static void _requested_normal(void *p_ud);

protected:
    static void _bind_methods();
    void _validate_property(PropertyInfo &property) const override;

public:
    using TextureFormatRequestCallback = void (*)(StringName);

    static TextureFormatRequestCallback request_3d_callback;
    static TextureFormatRequestCallback request_srgb_callback;
    static TextureFormatRequestCallback request_normal_callback;

    uint32_t get_flags() const override;
    Image::Format get_format() const;
    Error load(StringView p_path);
    String get_load_path() const;

    int get_width() const override;
    int get_height() const override;
    RenderingEntity get_rid() const override;

    void set_path(StringView p_path, bool p_take_over) override;

    void draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate = Color(1, 1, 1),
            bool p_transpose = false, const Ref<Texture> &p_normal_map = Ref<Texture>()) const override;
    void draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile = false,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>()) const override;
    void draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>(), bool p_clip_uv = true) const override;

    bool has_alpha() const override;
    void set_flags(uint32_t p_flags) override;
    bool is_pixel_opaque(int p_x, int p_y) const override;

    Ref<Image> get_data() const override;

    StreamTexture();
    ~StreamTexture() override;
};

class GODOT_EXPORT AtlasTexture : public Texture {

    GDCLASS(AtlasTexture,Texture)

    RES_BASE_EXTENSION("atlastex")

protected:
    Ref<Texture> atlas;
    Rect2 region;
    Rect2 margin;
    bool filter_clip;

    static void _bind_methods();

public:
    int get_width() const override;
    int get_height() const override;
    RenderingEntity get_rid() const override;

    bool has_alpha() const override;

    void set_flags(uint32_t p_flags) override;
    uint32_t get_flags() const override;

    void set_atlas(const Ref<Texture> &p_atlas);
    Ref<Texture> get_atlas() const;

    void set_region(const Rect2 &p_region);
    Rect2 get_region() const;

    void set_margin(const Rect2 &p_margin);
    Rect2 get_margin() const;

    void set_filter_clip(const bool p_enable);
    bool has_filter_clip() const;

    Ref<Image> get_data() const override;

    void draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate = Color(1, 1, 1),
            bool p_transpose = false, const Ref<Texture> &p_normal_map = Ref<Texture>()) const override;
    void draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile = false,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>()) const override;
    void draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>(), bool p_clip_uv = true) const override;
    bool get_rect_region(const Rect2 &p_rect, const Rect2 &p_src_rect, Rect2 &r_rect, Rect2 &r_src_rect) const override;

    bool is_pixel_opaque(int p_x, int p_y) const override;

    AtlasTexture();
};

class Mesh;

class GODOT_EXPORT MeshTexture : public Texture {

    GDCLASS(MeshTexture,Texture)

    RES_BASE_EXTENSION("meshtex")

    Ref<Texture> base_texture;
    Ref<Mesh> mesh;
    Size2i size;

protected:
    static void _bind_methods();

public:
    int get_width() const override;
    int get_height() const override;
    RenderingEntity get_rid() const override;

    bool has_alpha() const override;

    void set_flags(uint32_t p_flags) override;
    uint32_t get_flags() const override;

    void set_mesh(const Ref<Mesh> &p_mesh);
    const Ref<Mesh> &get_mesh() const;

    void set_image_size(const Size2 &p_size);
    Size2 get_image_size() const;

    void set_base_texture(const Ref<Texture> &p_texture);
    const Ref<Texture> &get_base_texture() const;

    void draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate = Color(1, 1, 1),
            bool p_transpose = false, const Ref<Texture> &p_normal_map = Ref<Texture>()) const override;
    void draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile = false,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>()) const override;
    void draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>(), bool p_clip_uv = true) const override;
    bool get_rect_region(const Rect2 &p_rect, const Rect2 &p_src_rect, Rect2 &r_rect, Rect2 &r_src_rect) const override;

    bool is_pixel_opaque(int p_x, int p_y) const override;

    MeshTexture();
    ~MeshTexture() override;
};

class GODOT_EXPORT LargeTexture : public Texture {

    GDCLASS(LargeTexture,Texture)

    RES_BASE_EXTENSION("largetex")

protected:
    struct Piece {

        Point2 offset;
        Ref<Texture> texture;
    };

    Vector<Piece> pieces;
    Size2i size;
public:
    Array _get_data() const;
    void _set_data(const Array &p_array);
protected:
    static void _bind_methods();

public:
    int get_width() const override;
    int get_height() const override;
    RenderingEntity get_rid() const override;

    bool has_alpha() const override;

    void set_flags(uint32_t p_flags) override;
    uint32_t get_flags() const override;

    int add_piece(const Point2 &p_offset, const Ref<Texture> &p_texture);
    void set_piece_offset(int p_idx, const Point2 &p_offset);
    void set_piece_texture(int p_idx, const Ref<Texture> &p_texture);

    void set_size(const Size2 &p_size);
    void clear();

    int get_piece_count() const;
    Vector2 get_piece_offset(int p_idx) const;
    Ref<Texture> get_piece_texture(int p_idx) const;
    Ref<Image> to_image() const;

    void draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate = Color(1, 1, 1),
            bool p_transpose = false, const Ref<Texture> &p_normal_map = Ref<Texture>()) const override;
    void draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile = false,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>()) const override;
    void draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect,
            const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false,
            const Ref<Texture> &p_normal_map = Ref<Texture>(), bool p_clip_uv = true) const override;

    bool is_pixel_opaque(int p_x, int p_y) const override;

    LargeTexture();
};

class GODOT_EXPORT CubeMap : public Resource {

    GDCLASS(CubeMap,Resource)

    RES_BASE_EXTENSION("cubemap")

public:
    enum Storage {
        STORAGE_RAW,
        STORAGE_COMPRESS_LOSSY,
        STORAGE_COMPRESS_LOSSLESS
    };

    enum Side {

        SIDE_LEFT,
        SIDE_RIGHT,
        SIDE_BOTTOM,
        SIDE_TOP,
        SIDE_FRONT,
        SIDE_BACK
    };

    enum Flags {
        FLAG_MIPMAPS = RS::TEXTURE_FLAG_MIPMAPS,
        FLAG_REPEAT = RS::TEXTURE_FLAG_REPEAT,
        FLAG_FILTER = RS::TEXTURE_FLAG_FILTER,
        FLAGS_DEFAULT = FLAG_MIPMAPS | FLAG_REPEAT | FLAG_FILTER,
    };

private:
    bool valid[6];
    RenderingEntity cubemap;
    Image::Format format;
    uint32_t flags;
    int w, h;
    Storage storage;
    Size2 size_override;
    float lossy_storage_quality;

    _FORCE_INLINE_ bool _is_valid() const {
        for (int i = 0; i < 6; i++) {
            if (valid[i]) {
                return true;
            }
        }
        return false;
    }

protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(Vector<PropertyInfo> *p_list) const;

    static void _bind_methods();

public:
    void set_flags(uint32_t p_flags);
    uint32_t get_flags() const;
    void set_side(Side p_side, const Ref<Image> &p_image);
    Ref<Image> get_side(Side p_side) const;

    Image::Format get_format() const;
    int get_width() const;
    int get_height() const;

    RenderingEntity get_rid() const override;

    void set_storage(Storage p_storage);
    Storage get_storage() const;

    void set_lossy_storage_quality(float p_lossy_storage_quality);
    float get_lossy_storage_quality() const;

    void set_path(StringView p_path, bool p_take_over = false) override;

    CubeMap();
    ~CubeMap() override;
};


class GODOT_EXPORT TextureLayered : public Resource {

    GDCLASS(TextureLayered,Resource)

public:
    enum Flags {
        FLAG_MIPMAPS = RS::TEXTURE_FLAG_MIPMAPS,
        FLAG_REPEAT = RS::TEXTURE_FLAG_REPEAT,
        FLAG_FILTER = RS::TEXTURE_FLAG_FILTER,
        FLAG_ANISOTROPIC_FILTER = RS::TEXTURE_FLAG_ANISOTROPIC_FILTER,
        FLAG_CONVERT_TO_LINEAR = RS::TEXTURE_FLAG_CONVERT_TO_LINEAR,
        FLAGS_DEFAULT_TEXTURE_ARRAY = FLAG_MIPMAPS | FLAG_REPEAT | FLAG_FILTER,
        FLAGS_DEFAULT_TEXTURE_3D = FLAG_FILTER,
    };
    enum Compression {
        COMPRESSION_LOSSLESS,
        COMPRESSION_VRAM,
        COMPRESSION_UNCOMPRESSED
    };
private:
    RenderingEntity texture;
    Image::Format format;
    uint32_t flags;
    int width;
    int height;
    int depth;
    bool is_3d;

public:
    void _set_data(const Dictionary &p_data);
    Dictionary _get_data() const;

protected:
    static void _bind_methods();

public:
    void set_flags(uint32_t p_flags);
    uint32_t get_flags() const;

    Image::Format get_format() const;
    uint32_t get_width() const;
    uint32_t get_height() const;
    uint32_t get_depth() const;

    void create(uint32_t p_width, uint32_t p_height, uint32_t p_depth, Image::Format p_format,
            uint32_t p_flags = FLAGS_DEFAULT_TEXTURE_ARRAY);
    void set_layer_data(const Ref<Image> &p_image, int p_layer);
    Ref<Image> get_layer_data(int p_layer) const;
    void set_data_partial(const Ref<Image> &p_image, int p_x_ofs, int p_y_ofs, int p_z, int p_mipmap = 0);

    RenderingEntity get_rid() const override;
    void set_path(StringView p_path, bool p_take_over = false) override;

    TextureLayered(bool p_3d = false);
    ~TextureLayered() override;
};


class GODOT_EXPORT Texture3D : public TextureLayered {

    GDCLASS(Texture3D,TextureLayered)

protected:
    static void _bind_methods();
public:
    void create(uint32_t p_width, uint32_t p_height, uint32_t p_depth, Image::Format p_format,
            uint32_t p_flags = FLAGS_DEFAULT_TEXTURE_3D) {
        TextureLayered::create(p_width, p_height, p_depth, p_format, p_flags);
    }
    Texture3D() : TextureLayered(true) {}
};

class GODOT_EXPORT TextureArray : public TextureLayered {

    GDCLASS(TextureArray,TextureLayered)

protected:
    static void _bind_methods();
public:
    void create(uint32_t p_width, uint32_t p_height, uint32_t p_depth, Image::Format p_format,
            uint32_t p_flags = FLAGS_DEFAULT_TEXTURE_ARRAY) {
        TextureLayered::create(p_width, p_height, p_depth, p_format, p_flags);
    }

    TextureArray() : TextureLayered(false) {}
};

/*
    enum CubeMapSide {

        CUBEMAP_LEFT,
        CUBEMAP_RIGHT,
        CUBEMAP_BOTTOM,
        CUBEMAP_TOP,
        CUBEMAP_FRONT,
        CUBEMAP_BACK,
    };

*/
//VARIANT_ENUM_CAST( Texture::CubeMapSide );

class GODOT_EXPORT GradientTexture : public Texture {
    GDCLASS(GradientTexture,Texture)

public:
    struct Point {

        float offset;
        Color color;
        bool operator<(const Point &p_ponit) const { return offset < p_ponit.offset; }
    };

private:
    Ref<Gradient> gradient;
    bool update_pending;
    RenderingEntity texture;
    int width;
    bool use_hdr = false;

    void _queue_update();
    void _update();

protected:
    static void _bind_methods();

public:
    void set_gradient(const Ref<Gradient>& p_gradient);
    Ref<Gradient> get_gradient() const;

    void set_width(int p_width);
    int get_width() const override;
    void set_use_hdr(bool p_enabled);
    bool is_using_hdr() const;

    RenderingEntity get_rid() const override { return texture; }
    int get_height() const override { return 1; }
    bool has_alpha() const override { return true; }

    void set_flags(uint32_t /*p_flags*/) override {}
    uint32_t get_flags() const override { return FLAG_FILTER; }

    Ref<Image> get_data() const override;

    GradientTexture();
    ~GradientTexture() override;
};

class GODOT_EXPORT GradientTexture2D : public Texture {
    GDCLASS(GradientTexture2D, Texture);

public:
    enum Fill {
        FILL_LINEAR,
        FILL_RADIAL,
    };
    enum Repeat {
        REPEAT_NONE,
        REPEAT,
        REPEAT_MIRROR,
    };

private:
    Ref<Gradient> gradient;
    mutable RenderingEntity texture;

    int width = 64;
    int height = 64;

    uint32_t flags = FLAGS_DEFAULT;

    bool use_hdr = false;

    Vector2 fill_from;
    Vector2 fill_to = Vector2(1, 0);

    Fill fill = FILL_LINEAR;
    Repeat repeat = REPEAT_NONE;

    float _get_gradient_offset_at(int x, int y) const;

    bool update_pending = false;
    void _queue_update();
    void _update();

protected:
    static void _bind_methods();

public:
    void set_gradient(Ref<Gradient> p_gradient);
    Ref<Gradient> get_gradient() const;

    void set_width(int p_width);
    int get_width() const override;
    void set_height(int p_height);
    int get_height() const override;

    void set_flags(uint32_t p_flags) override;
    uint32_t get_flags() const override;

    void set_use_hdr(bool p_enabled);
    bool is_using_hdr() const;

    void set_fill(Fill p_fill);
    Fill get_fill() const;
    void set_fill_from(Vector2 p_fill_from);
    Vector2 get_fill_from() const;
    void set_fill_to(Vector2 p_fill_to);
    Vector2 get_fill_to() const;

    void set_repeat(Repeat p_repeat);
    Repeat get_repeat() const;

    RenderingEntity get_rid() const override;
    bool has_alpha() const override { return true; }
    virtual Ref<Image> get_image() const;

    GradientTexture2D();
    virtual ~GradientTexture2D();
};
class GODOT_EXPORT ProxyTexture : public Texture {
    GDCLASS(ProxyTexture,Texture)

private:
    RenderingEntity proxy;
    Ref<Texture> base;

protected:
    static void _bind_methods();

public:
    void set_base(const Ref<Texture> &p_texture);
    Ref<Texture> get_base() const;

    int get_width() const override;
    int get_height() const override;
    RenderingEntity get_rid() const override;

    bool has_alpha() const override;

    void set_flags(uint32_t p_flags) override;
    uint32_t get_flags() const override;

    ProxyTexture();
    ~ProxyTexture() override;
};

class GODOT_EXPORT AnimatedTexture : public Texture {
    GDCLASS(AnimatedTexture,Texture)

    //use readers writers lock for this, since its far more times read than written to
    RWLock rw_lock;
public:
    enum { MAX_FRAMES = 256 };

private:
    RenderingEntity proxy;

    struct Frame {

        Ref<Texture> texture;
        float delay_sec = 0;
    };

    Frame frames[MAX_FRAMES];
    int frame_count;
    int current_frame;
    float fps;
    float time;
    uint64_t prev_ticks;
    bool pause;
    bool oneshot;


    void _update_proxy();

protected:
    static void _bind_methods();
    void _validate_property(PropertyInfo &property) const override;

public:
    void set_frames(int p_frames);
    int get_frames() const;

    void set_current_frame(int p_frame);
    int get_current_frame() const;

    void set_pause(bool p_pause);
    bool get_pause() const;

    void set_oneshot(bool p_oneshot);
    bool get_oneshot() const;

    void set_frame_texture(int p_frame, const Ref<Texture> &p_texture);
    Ref<Texture> get_frame_texture(int p_frame) const;

    void set_frame_delay(int p_frame, float p_delay_sec);
    float get_frame_delay(int p_frame) const;

    void set_fps(float p_fps);
    float get_fps() const;

    int get_width() const override;
    int get_height() const override;
    RenderingEntity get_rid() const override;

    bool has_alpha() const override;

    void set_flags(uint32_t p_flags) override;
    uint32_t get_flags() const override;

    Ref<Image> get_data() const override;

    bool is_pixel_opaque(int p_x, int p_y) const override;

    AnimatedTexture();
    ~AnimatedTexture() override;
};

// External textures as defined by https://www.khronos.org/registry/OpenGL/extensions/OES/OES_EGL_image_external.txt
class GODOT_EXPORT ExternalTexture : public Texture {
    GDCLASS(ExternalTexture, Texture)

private:
    RenderingEntity texture;
    Size2 size;

protected:
    static void _bind_methods();

public:
    uint32_t get_external_texture_id();

    Size2 get_size() const override;
    void set_size(const Size2 &p_size);

    int get_width() const override;
    int get_height() const override;

    RenderingEntity get_rid() const override;
    bool has_alpha() const override;

    void set_flags(uint32_t p_flags) override;
    uint32_t get_flags() const override;

    ExternalTexture();
    ~ExternalTexture() override;
};
