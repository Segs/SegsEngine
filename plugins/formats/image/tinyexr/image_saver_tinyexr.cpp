/*************************************************************************/
/*  image_saver_tinyexr.cpp                                              */
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

#include "image_saver_tinyexr.h"
#include "core/math/math_funcs.h"
#include "core/image_data.h"
#include "core/string.h"
#include "core/os/file_access.h"
#include "core/print_string.h"

#include "zlib.h" // Should come before including tinyexr.

#include "thirdparty/tinyexr/tinyexr.h"

#include <cstdlib>


static bool is_supported_format(ImageData::Format p_format) {
    // This is checked before anything else.
    // Mostly uncompressed formats are considered.
    switch (p_format) {
        case ImageData::FORMAT_RF:
        case ImageData::FORMAT_RGF:
        case ImageData::FORMAT_RGBF:
        case ImageData::FORMAT_RGBAF:
        case ImageData::FORMAT_RH:
        case ImageData::FORMAT_RGH:
        case ImageData::FORMAT_RGBH:
        case ImageData::FORMAT_RGBAH:
        case ImageData::FORMAT_R8:
        case ImageData::FORMAT_RG8:
        case ImageData::FORMAT_RGB8:
        case ImageData::FORMAT_RGBA8:
            return true;
        default:
            return false;
    }
}

enum SrcPixelType {
    SRC_FLOAT,
    SRC_HALF,
    SRC_BYTE,
    SRC_UNSUPPORTED
};

static SrcPixelType get_source_pixel_type(ImageData::Format p_format) {
    switch (p_format) {
        case ImageData::FORMAT_RF:
        case ImageData::FORMAT_RGF:
        case ImageData::FORMAT_RGBF:
        case ImageData::FORMAT_RGBAF:
            return SRC_FLOAT;
        case ImageData::FORMAT_RH:
        case ImageData::FORMAT_RGH:
        case ImageData::FORMAT_RGBH:
        case ImageData::FORMAT_RGBAH:
            return SRC_HALF;
        case ImageData::FORMAT_R8:
        case ImageData::FORMAT_RG8:
        case ImageData::FORMAT_RGB8:
        case ImageData::FORMAT_RGBA8:
            return SRC_BYTE;
        default:
            return SRC_UNSUPPORTED;
    }
}

static int get_target_pixel_type(ImageData::Format p_format) {
    switch (p_format) {
        case ImageData::FORMAT_RF:
        case ImageData::FORMAT_RGF:
        case ImageData::FORMAT_RGBF:
        case ImageData::FORMAT_RGBAF:
            return TINYEXR_PIXELTYPE_FLOAT;
        case ImageData::FORMAT_RH:
        case ImageData::FORMAT_RGH:
        case ImageData::FORMAT_RGBH:
        case ImageData::FORMAT_RGBAH:
        // EXR doesn't support 8-bit channels so in that case we'll convert
        case ImageData::FORMAT_R8:
        case ImageData::FORMAT_RG8:
        case ImageData::FORMAT_RGB8:
        case ImageData::FORMAT_RGBA8:
            return TINYEXR_PIXELTYPE_HALF;
        default:
            return -1;
    }
}

static int get_pixel_type_size(int p_pixel_type) {
    switch (p_pixel_type) {
        case TINYEXR_PIXELTYPE_HALF:
            return 2;
        case TINYEXR_PIXELTYPE_FLOAT:
            return 4;
    }
    return -1;
}

static int get_channel_count(ImageData::Format p_format) {
    switch (p_format) {
        case ImageData::FORMAT_RF:
        case ImageData::FORMAT_RH:
        case ImageData::FORMAT_R8:
            return 1;
        case ImageData::FORMAT_RGF:
        case ImageData::FORMAT_RGH:
        case ImageData::FORMAT_RG8:
            return 2;
        case ImageData::FORMAT_RGBF:
        case ImageData::FORMAT_RGBH:
        case ImageData::FORMAT_RGB8:
            return 3;
        case ImageData::FORMAT_RGBAF:
        case ImageData::FORMAT_RGBAH:
        case ImageData::FORMAT_RGBA8:
            return 4;
        default:
            return -1;
    }
}

Error prepare_exr_save(EXRImage &image,EXRHeader &header, const ImageData &p_img, bool p_grayscale) {

    ImageData::Format format = p_img.format;

    if (!is_supported_format(format)) {
        // Format not supported
        //TODO: use OsInterface to print the message
        //print_error("Image format not supported for saving as EXR. Consider saving as PNG.");
        return ERR_UNAVAILABLE;
    }

    InitEXRHeader(&header);
    InitEXRImage(&image);

    constexpr int max_channels = 4;

    // Godot does not support more than 4 channels,
    // so we can preallocate header infos on the stack and use only the subset we need
    PoolVector<uint8_t> channels[max_channels];
    unsigned char *channels_ptrs[max_channels];
    EXRChannelInfo channel_infos[max_channels];
    int pixel_types[max_channels];
    int requested_pixel_types[max_channels] = { -1,-1,-1,-1 };

    // Gimp and Blender are a bit annoying so order of channels isn't straightforward.
    const int channel_mappings[4][4] = {
        { 0 }, // R
        { 1, 0 }, // GR
        { 2, 1, 0 }, // BGR
        { 3, 2, 1, 0 } // ABGR
    };

    int channel_count = get_channel_count(format);
    ERR_FAIL_COND_V(channel_count < 0, ERR_UNAVAILABLE);
    ERR_FAIL_COND_V(p_grayscale && channel_count != 1, ERR_INVALID_PARAMETER);

    int target_pixel_type = get_target_pixel_type(format);
    ERR_FAIL_COND_V(target_pixel_type < 0, ERR_UNAVAILABLE);
    int target_pixel_type_size = get_pixel_type_size(target_pixel_type);
    ERR_FAIL_COND_V(target_pixel_type_size < 0, ERR_UNAVAILABLE);
    SrcPixelType src_pixel_type = get_source_pixel_type(format);
    ERR_FAIL_COND_V(src_pixel_type == SRC_UNSUPPORTED, ERR_UNAVAILABLE);
    const int pixel_count = p_img.width * p_img.height;

    const int *channel_mapping = channel_mappings[channel_count - 1];

    {
        PoolVector<uint8_t> src_data = p_img.data;
        PoolVector<uint8_t>::Read src_r = src_data.read();

        for (int channel_index = 0; channel_index < channel_count; ++channel_index) {

            // De-interleave channels

            PoolVector<uint8_t> &dst = channels[channel_index];
            dst.resize(pixel_count * target_pixel_type_size);

            PoolVector<uint8_t>::Write dst_w = dst.write();

            if (src_pixel_type == SRC_FLOAT && target_pixel_type == TINYEXR_PIXELTYPE_FLOAT) {

                // Note: we don't save mipmaps
                CRASH_COND(src_data.size() < pixel_count * channel_count * target_pixel_type_size);

                const float *src_rp = (float *)src_r.ptr();
                float *dst_wp = (float *)dst_w.ptr();

                for (int i = 0; i < pixel_count; ++i) {
                    dst_wp[i] = src_rp[channel_index + i * channel_count];
                }

            } else if (src_pixel_type == SRC_HALF && target_pixel_type == TINYEXR_PIXELTYPE_HALF) {

                CRASH_COND(src_data.size() < pixel_count * channel_count * target_pixel_type_size);

                const uint16_t *src_rp = (uint16_t *)src_r.ptr();
                uint16_t *dst_wp = (uint16_t *)dst_w.ptr();

                for (int i = 0; i < pixel_count; ++i) {
                    dst_wp[i] = src_rp[channel_index + i * channel_count];
                }

            } else if (src_pixel_type == SRC_BYTE && target_pixel_type == TINYEXR_PIXELTYPE_HALF) {

                CRASH_COND(src_data.size() < pixel_count * channel_count);

                const uint8_t *src_rp = (uint8_t *)src_r.ptr();
                uint16_t *dst_wp = (uint16_t *)dst_w.ptr();

                for (int i = 0; i < pixel_count; ++i) {
                    dst_wp[i] = Math::make_half_float(src_rp[channel_index + i * channel_count] / 255.f);
                }

            } else {
                CRASH_NOW();
            }

            int remapped_index = channel_mapping[channel_index];

            channels_ptrs[remapped_index] = dst_w.ptr();

            // No conversion
            pixel_types[remapped_index] = target_pixel_type;
            requested_pixel_types[remapped_index] = target_pixel_type;

            // Write channel name
            if (p_grayscale) {
                channel_infos[remapped_index].name[0] = 'Y';
                channel_infos[remapped_index].name[1] = '\0';
            } else {
                const char *rgba = "RGBA";
                channel_infos[remapped_index].name[0] = rgba[channel_index];
                channel_infos[remapped_index].name[1] = '\0';
            }
        }
    }

    image.images = channels_ptrs;
    image.num_channels = channel_count;
    image.width = p_img.width;
    image.height = p_img.height;

    header.num_channels = image.num_channels;
    header.channels = channel_infos;
    header.pixel_types = pixel_types;
    header.requested_pixel_types = requested_pixel_types;
    header.compression_type = TINYEXR_COMPRESSIONTYPE_PIZ;

    return OK;
}
Error save_exr(StringView p_path, const ImageData &p_img, bool p_grayscale) {
    EXRImage image;
    EXRHeader header;
    auto res = prepare_exr_save(image,header,p_img,p_grayscale);
    if(res!=OK)
        return res;

    unsigned char *mem = nullptr;
    const char *err = nullptr;

    size_t bytes = SaveEXRImageToMemory(&image, &header, &mem, &err);

    if (bytes == 0) {
        print_error(String("Saving EXR failed. Error: ")+err);
        return ERR_FILE_CANT_WRITE;
    } else {
        FileAccessRef ref = FileAccess::open(p_path, FileAccess::WRITE);
        ERR_FAIL_COND_V(!ref, ERR_FILE_CANT_WRITE);
        ref->store_buffer(mem, bytes);
        free(mem);
    }

    return OK;
}
Error save_exr(Vector<uint8_t> &tgt, const ImageData &p_img, bool p_grayscale) {
    EXRImage image;
    EXRHeader header;
    auto res = prepare_exr_save(image,header,p_img,p_grayscale);
    if(res!=OK)
        return res;

    const char *err;
    uint8_t *exr_memory;
    int ret = SaveEXRImageToMemory(&image, &header, &exr_memory, &err);
    if (ret == 0) {
        //TODO: use OsInterface if the reporting is needed.
        //print_error(String("Saving EXR failed. Error: {0}").format(varray(err)));
        FreeEXRErrorMessage(err);
        return ERR_FILE_CANT_WRITE;
    }
    tgt.resize(ret);
    memcpy(tgt.data(),exr_memory,ret);
    free(exr_memory);
    return OK;
}
