/*************************************************************************/
/*  image_loader_tinyexr.cpp                                             */
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

#include "image_loader_tinyexr.h"
#include "image_saver_tinyexr.h"

#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/color.h"
#include "core/print_string.h"
#include "core/image_data.h"

#include "thirdparty/tinyexr/tinyexr.h"

Error ImageLoaderTinyEXR::load_image(ImageData &p_image, FileAccess *f, LoadParams params) {

    PoolVector<uint8_t> src_image;
    int src_image_len = f->get_len();
    ERR_FAIL_COND_V(src_image_len == 0, ERR_FILE_CORRUPT);
    src_image.resize(src_image_len);

    PoolVector<uint8_t>::Write w = src_image.write();

    f->get_buffer(&w[0], src_image_len);

    f->close();

    // Re-implementation of tinyexr's LoadEXRFromMemory using Godot types to store the Image data
    // and Godot's error codes.
    // When debugging after updating the thirdparty library, check that we're still in sync with
    // their API usage in LoadEXRFromMemory.

    EXRVersion exr_version;
    EXRImage exr_image;
    EXRHeader exr_header;
    const char *err = nullptr;

    InitEXRHeader(&exr_header);

    int ret = ParseEXRVersionFromMemory(&exr_version, w.ptr(), src_image_len);
    if (ret != TINYEXR_SUCCESS) {

        return ERR_FILE_CORRUPT;
    }

    ret = ParseEXRHeaderFromMemory(&exr_header, &exr_version, w.ptr(), src_image_len, &err);
    if (ret != TINYEXR_SUCCESS) {
        if (err) {
            ERR_PRINT(String(err));
        }
        return ERR_FILE_CORRUPT;
    }

    // Read HALF channel as FLOAT. (GH-13490)
    for (int i = 0; i < exr_header.num_channels; i++) {
        if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
            exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
        }
    }

    InitEXRImage(&exr_image);
    ret = LoadEXRImageFromMemory(&exr_image, &exr_header, w.ptr(), src_image_len, &err);
    if (ret != TINYEXR_SUCCESS) {
        if (err) {
            ERR_PRINT(String(err));
        }
        return ERR_FILE_CORRUPT;
    }

    // RGBA
    int idxR = -1;
    int idxG = -1;
    int idxB = -1;
    int idxA = -1;
    for (int c = 0; c < exr_header.num_channels; c++) {
        if (strcmp(exr_header.channels[c].name, "R") == 0) {
            idxR = c;
        } else if (strcmp(exr_header.channels[c].name, "G") == 0) {
            idxG = c;
        } else if (strcmp(exr_header.channels[c].name, "B") == 0) {
            idxB = c;
        } else if (strcmp(exr_header.channels[c].name, "A") == 0) {
            idxA = c;
        }
    }

    if (exr_header.num_channels == 1) {
        // Grayscale channel only.
        idxR = 0;
        idxG = 0;
        idxB = 0;
        idxA = 0;
    } else {
        // Assume RGB(A)
        if (idxR == -1) {
            ERR_PRINT("TinyEXR: R channel not found.");
            // @todo { free exr_image }
            return ERR_FILE_CORRUPT;
        }

        if (idxG == -1) {
            ERR_PRINT("TinyEXR: G channel not found.");
            // @todo { free exr_image }
            return ERR_FILE_CORRUPT;
        }

        if (idxB == -1) {
            ERR_PRINT("TinyEXR: B channel not found.");
            // @todo { free exr_image }
            return ERR_FILE_CORRUPT;
        }
    }

    // EXR image data loaded, now parse it into Godot-friendly image data

    ImageData::Format format;
    int output_channels = 0;

    if (idxA != -1) {

        p_image.data.resize(exr_image.width * exr_image.height * 8); //RGBA16
        format = ImageData::FORMAT_RGBAH;
        output_channels = 4;
    } else {

        p_image.data.resize(exr_image.width * exr_image.height * 6); //RGB16
        format = ImageData::FORMAT_RGBH;
        output_channels = 3;
    }

    EXRTile single_image_tile;
    int num_tiles;
    int tile_width = 0;
    int tile_height = 0;

    const EXRTile *exr_tiles;

    if (!exr_header.tiled) {
        single_image_tile.images = exr_image.images;
        single_image_tile.width = exr_image.width;
        single_image_tile.height = exr_image.height;
        single_image_tile.level_x = exr_image.width;
        single_image_tile.level_y = exr_image.height;
        single_image_tile.offset_x = 0;
        single_image_tile.offset_y = 0;

        exr_tiles = &single_image_tile;
        num_tiles = 1;
        tile_width = exr_image.width;
        tile_height = exr_image.height;
    } else {
        tile_width = exr_header.tile_size_x;
        tile_height = exr_header.tile_size_y;
        num_tiles = exr_image.num_tiles;
        exr_tiles = exr_image.tiles;
    }

    {
        PoolVector<uint8_t>::Write wd = p_image.data.write();
        uint16_t *iw = (uint16_t *)wd.ptr();

        // Assume `out_rgba` have enough memory allocated.
        for (int tile_index = 0; tile_index < num_tiles; tile_index++) {

            const EXRTile &tile = exr_tiles[tile_index];

            int tw = tile.width;
            int th = tile.height;

            const float *r_channel_start = reinterpret_cast<const float *>(tile.images[idxR]);
            const float *g_channel_start = reinterpret_cast<const float *>(tile.images[idxG]);
            const float *b_channel_start = reinterpret_cast<const float *>(tile.images[idxB]);
            const float *a_channel_start = nullptr;

            if (idxA != -1) {
                a_channel_start = reinterpret_cast<const float *>(tile.images[idxA]);
            }

            uint16_t *first_row_w = iw + (tile.offset_y * tile_height * exr_image.width + tile.offset_x * tile_width) * output_channels;

            for (int y = 0; y < th; y++) {
                const float *r_channel = r_channel_start + y * tile_width;
                const float *g_channel = g_channel_start + y * tile_width;
                const float *b_channel = b_channel_start + y * tile_width;
                const float *a_channel = nullptr;

                if (a_channel_start) {
                    a_channel = a_channel_start + y * tile_width;
                }

                uint16_t *row_w = first_row_w + (y * exr_image.width * output_channels);

                for (int x = 0; x < tw; x++) {

                    Color color(*r_channel++, *g_channel++, *b_channel++);

                    if (params.p_force_linear)
                        color = color.to_linear();

                    *row_w++ = Math::make_half_float(color.r);
                    *row_w++ = Math::make_half_float(color.g);
                    *row_w++ = Math::make_half_float(color.b);

                    if (idxA != -1) {
                        *row_w++ = Math::make_half_float(*a_channel++);
                    }
                }
            }
        }
    }

    p_image.width = exr_image.width;
    p_image.height= exr_image.height;
    p_image.mipmaps=false;
    p_image.format = format;

    w.release();

    FreeEXRHeader(&exr_header);
    FreeEXRImage(&exr_image);

    return OK;
}

void ImageLoaderTinyEXR::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.push_back("exr");
}

ImageLoaderTinyEXR::ImageLoaderTinyEXR() {
}

bool ImageLoaderTinyEXR::can_save(se_string_view extension)
{
    return se_string_view("exr")==extension;
}

Error ImageLoaderTinyEXR::save_image(const ImageData &p_image, Vector<uint8_t> &tgt, SaveParams params)
{
    auto err = save_exr(tgt,p_image,params.p_greyscale);
    return err;
}

Error ImageLoaderTinyEXR::save_image(const ImageData &p_image, FileAccess *p_fileaccess, SaveParams params)
{
    Vector<uint8_t> tgt;
    auto err = save_exr(tgt,p_image,params.p_greyscale);

    if(err!=OK)
        return err;
    p_fileaccess->store_buffer(tgt.data(), tgt.size());
    if (p_fileaccess->get_error() != OK && p_fileaccess->get_error() != ERR_FILE_EOF) {
        return ERR_CANT_CREATE;
    }
    return OK;
}
