/*************************************************************************/
/*  image.h                                                              */
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

#include "core/color.h"
#include "core/image_data.h"
#include "core/resource.h"
#include "core/math/rect2.h"
#include "core/forward_decls.h"
#include "core/plugin_interfaces/load_params.h"

/**
 * @author Juan Linietsky <reduzio@gmail.com>
 *
 * Image storage class. This is used to store an image in user memory, as well as
 * providing some basic methods for image manipulation.
 * Images can be loaded from a file, or registered into the Render object as textures.
*/

class Image;
class ImageCodecInterface;

//this is used for compression
enum class ImageUsedChannels : int8_t {
    USED_CHANNELS_L,
    USED_CHANNELS_LA,
    USED_CHANNELS_R,
    USED_CHANNELS_RG,
    USED_CHANNELS_RGB,
    USED_CHANNELS_RGBA,
};
enum class ImageCompressSource : int8_t {
    COMPRESS_SOURCE_GENERIC=0,
    COMPRESS_SOURCE_SRGB,
    COMPRESS_SOURCE_NORMAL,
    COMPRESS_SOURCE_LAYERED,
    COMPRESS_SOURCE_MAX,
};
SE_ENUM(ImageCompressSource)

using SavePNGFunc = Error (*)(const UIString &, const Ref<Image> &);
using ImageMemLoadFunc = ImageData (*)(const uint8_t *, int);

using SaveEXRFunc = Error (*)(const UIString &, const Ref<Image> &, bool);

class GODOT_EXPORT Image : public Resource, private ImageData {
    GDCLASS(Image, Resource)
    SE_CLASS()

    SE_PROPERTY(Dictionary data READ _set_data WRITE _get_data USAGE STORAGE)
public:
    static Error save_png_func(StringView p_path, const Ref<Image> &p_img);
    static Error save_exr_func(StringView p_path, const Ref<Image> &p_img, bool p_grayscale);

    using Format = ImageData::Format;

    enum Interpolation {

        INTERPOLATE_NEAREST,
        INTERPOLATE_BILINEAR,
        INTERPOLATE_CUBIC,
        INTERPOLATE_TRILINEAR,
        INTERPOLATE_LANCZOS,
        /* INTERPOLATE_TRICUBIC, */
        /* INTERPOLATE GAUSS */
    };
    SE_ENUM(Interpolation)
    //some functions provided by something else
    static Error compress_image(Image *,CompressParams p);
    static Error decompress_image(Image *,CompressParams p);

    static Vector<uint8_t> lossy_packer(const Ref<Image> &p_image, float p_quality);
    static Ref<Image> webp_unpacker(const Vector<uint8_t> &p_buffer);
    static Vector<uint8_t> lossless_packer(const Ref<Image> &p_image);
    static Ref<Image> png_unpacker(const Vector<uint8_t> &p_buffer);
    static Vector<uint8_t> basis_universal_packer(const Ref<Image> &p_image, ImageUsedChannels p_channels);
    static Ref<Image> basis_universal_unpacker(const Vector<uint8_t> &p_buffer);

    PoolVector<uint8_t>::Write write_lock;

    Color _get_color_at_ofs(uint8_t *ptr, uint32_t ofs) const;
    void _set_color_at_ofs(uint8_t *ptr, uint32_t ofs, const Color &p_color);

protected:
    static void _bind_methods();

public:
    void _create_empty(int p_width, int p_height, bool p_use_mipmaps, Format p_format) {
        create(p_width, p_height, p_use_mipmaps, p_format);
    }

    void _create_from_data(int p_width, int p_height, bool p_use_mipmaps, Format p_format, const PoolVector<uint8_t> &p_data) {
        create(p_width, p_height, p_use_mipmaps, p_format, p_data);
    }

    void _set_data(const Dictionary &p_data);
    Dictionary _get_data() const;
protected:
    void _copy_internals_from(const Image &p_image) {
        format = p_image.format;
        width = p_image.width;
        height = p_image.height;
        mipmaps = p_image.mipmaps;
        data = p_image.data;
    }

    void _get_mipmap_offset_and_size(int p_mipmap, int &r_offset, int &r_width, int &r_height) const; //get where the mipmap begins in data

    bool _can_modify(Format p_format) const;

    _FORCE_INLINE_ void _put_pixelb(int p_x, int p_y, uint32_t p_pixelsize, uint8_t *p_data, const uint8_t *p_pixel);
    _FORCE_INLINE_ void _get_pixelb(int p_x, int p_y, uint32_t p_pixelsize, const uint8_t *p_data, uint8_t *p_pixel);


    Error _load_from_buffer(const PoolVector<uint8_t> &p_array, const char *ext);
    Error _load_from_buffer(const uint8_t *p_array,int size, const char *ext);

public:
    ImageData &img_data() { return *this; }
    int get_width() const; ///< Get image width
    int get_height() const; ///< Get image height
    Vector2 get_size() const;
    bool has_mipmaps() const { return mipmaps; }
    int get_mipmap_count() const;

    /**
     * Convert the image to another format, conversion only to raw byte format
     */
    void convert(Format p_new_format);

    /**
     * Get the current image format.
     */
    Format get_format() const;

    int get_mipmap_byte_size(int p_mipmap) const;
    int get_mipmap_offset(int p_mipmap) const; //get where the mipmap begins in data
    void get_mipmap_offset_and_size(int p_mipmap, int &r_ofs, int &r_size) const; //get where the mipmap begins in data
    void get_mipmap_offset_size_and_dimensions(int p_mipmap, int &r_ofs, int &r_size, int &w, int &h) const; //get where the mipmap begins in data

    /**
     * Resize the image, using the preferred interpolation method.
     */

    void resize_to_po2(bool p_square = false);
    void resize(int p_width, int p_height, Interpolation p_interpolation = INTERPOLATE_BILINEAR);
    void shrink_x2();
    void expand_x2_hq2x();
    bool is_size_po2() const;
    /**
     * Crop the image to a specific size, if larger, then the image is filled by black
     */
    void crop_from_point(int p_x, int p_y, int p_width, int p_height);
    void crop(int p_width, int p_height);

    void flip_x();
    void flip_y();

    /**
     * Generate a mipmap to an image (creates an image 1/4 the size, with averaging of 4->1)
     */
    Error generate_mipmaps(bool p_renormalize = false);
    enum RoughnessChannel {
        ROUGHNESS_CHANNEL_R,
        ROUGHNESS_CHANNEL_G,
        ROUGHNESS_CHANNEL_B,
        ROUGHNESS_CHANNEL_A,
        ROUGHNESS_CHANNEL_L,
    };

    Error generate_mipmap_roughness(RoughnessChannel p_roughness_channel, const Ref<Image> &p_normal_map);

    void clear_mipmaps();
    void normalize(); //for normal maps

    /**
     * Create a new image of a given size and format. Current image will be lost
     */
    void create(int p_width, int p_height, bool p_use_mipmaps, Format p_format);
    void create(int p_width, int p_height, bool p_use_mipmaps, Format p_format, const PoolVector<uint8_t> &p_data);
    void create(ImageData && src);

    /**
     * returns true when the image is empty (0,0) in size
     */
    bool is_empty() const;

    const PoolVector<uint8_t> &get_data() const { return data; }

    Error load(StringView p_path);
    Error save_png(StringView p_path) const;
    Error save_exr(StringView p_path, bool p_grayscale) const;

    /**
     * create an empty image
     */
    Image();
    /**
     * create an empty image of a specific size and format
     */
    Image(int p_width, int p_height, bool p_use_mipmaps, Format p_format);
    /**
     * import an image of a specific size and format from a pointer
     */
    Image(int p_width, int p_height, bool p_mipmaps, Format p_format, const PoolVector<uint8_t> &p_data);

    enum AlphaMode {
        ALPHA_NONE,
        ALPHA_BIT,
        ALPHA_BLEND
    };

    AlphaMode detect_alpha() const;
    bool is_invisible() const;

    static int get_format_pixel_size(Format p_format);
    static int get_format_pixel_rshift(Format p_format);
    static int get_format_block_size(Format p_format);
    static void get_format_min_pixel_size(Format p_format, int &r_w, int &r_h);

    static int get_image_data_size(int p_width, int p_height, Format p_format, bool p_mipmaps = false);
    static int get_image_required_mipmaps(int p_width, int p_height, Format p_format);
    static int get_image_mipmap_offset(int p_width, int p_height, Format p_format, int p_mipmap);

    Error compress(ImageCompressMode p_mode = COMPRESS_S3TC, ImageCompressSource p_source = ImageCompressSource::COMPRESS_SOURCE_GENERIC, float p_lossy_quality = 0.7);
    Error compress_from_channels(ImageCompressMode p_mode, ImageUsedChannels p_channels, float p_lossy_quality = 0.7);
    Error decompress();
    bool is_compressed() const;

    void fix_alpha_edges();
    void premultiply_alpha();
    void srgb_to_linear();
    void normalmap_to_xy();
    Ref<Image> rgbe_to_srgb();
    Ref<Image> get_image_from_mipmap(int p_mipamp) const;
    void bumpmap_to_normalmap(float bump_scale = 1.0);

    void blit_rect(const Ref<Image> &p_src, const Rect2 &p_src_rect, const Point2 &p_dest);
    void blit_rect_mask(const Ref<Image> &p_src, const Ref<Image> &p_mask, const Rect2 &p_src_rect, const Point2 &p_dest);
    void blend_rect(const Ref<Image> &p_src, const Rect2 &p_src_rect, const Point2 &p_dest);
    void blend_rect_mask(const Ref<Image> &p_src, const Ref<Image> &p_mask, const Rect2 &p_src_rect, const Point2 &p_dest);
    void fill(const Color &p_color);

    Rect2 get_used_rect() const;
    Ref<Image> get_rect(const Rect2 &p_area) const;

    static StringView get_format_name(Format p_format);

    Error load_png_from_buffer(const PoolVector<uint8_t> &p_array);
    Error load_jpg_from_buffer(const PoolVector<uint8_t> &p_array);
    Error load_webp_from_buffer(const PoolVector<uint8_t> &p_array);
    Error load_from_buffer(const uint8_t *p_array,int size, const char *ext);


    Ref<Resource> duplicate(bool p_subresources = false) const override;

    void lock();
    void unlock();

    ImageUsedChannels detect_used_channels(ImageCompressSource p_source = ImageCompressSource::COMPRESS_SOURCE_GENERIC);
    void optimize_channels();

    Color get_pixelv(const Point2 &p_src) const;
    Color get_pixel(int p_x, int p_y) const;
    void set_pixelv(const Point2 &p_dst, const Color &p_color);
    void set_pixel(int p_x, int p_y, const Color &p_color);

    void copy_internals_from(const Ref<Image> &p_image) {
        ERR_FAIL_COND_MSG(not p_image, "It's not a reference to a valid Image object.");
        format = p_image->format;
        width = p_image->width;
        height = p_image->height;
        mipmaps = p_image->mipmaps;
        data = p_image->data;
    }

    void convert_rg_to_ra_rgba8();
    void convert_ra_rgba8_to_rg();

    Image(const uint8_t *p_mem_png_jpg, int p_len = -1);
    Image(ImageData &&from) {
        format = from.format;
        width = from.width;
        height = from.height;
        mipmaps = from.mipmaps;
        data = eastl::move(from.data);
    }
    ~Image() override;
};
Ref<Image> prepareForPngStorage(const Ref<Image> &img);
