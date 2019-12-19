/*************************************************************************/
/*  png_driver_common.cpp                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "png_driver_common.h"

#include "core/os/os.h"
#include "core/image_data.h"
#include "core/vector.h"

#include <png.h>
#include <cstring>

namespace PNGDriverCommon {

// Print any warnings.
// On error, set explain and return true.
// Call should be wrapped in ERR_FAIL_COND
static bool check_error(const png_image &image) {
    const png_uint_32 failed = PNG_IMAGE_FAILED(image);
    if (failed & PNG_IMAGE_ERROR) {
        return true;
    } else if (failed) {
#ifdef TOOLS_ENABLED
        // suppress this warning, to avoid log spam when opening assetlib
        const static char *const noisy = "iCCP: known incorrect sRGB profile";
        const Engine *const eng = Engine::get_singleton();
        if (eng && eng->is_editor_hint() && !strcmp(image.message, noisy)) {
            return false;
        }
#endif
        WARN_PRINT(image.message)
    }
    return false;
}

Error png_to_image(const uint8_t *p_source, size_t p_size, ImageData &p_image) {

    png_image png_img;
    memset(&png_img, 0, sizeof(png_img));
    png_img.version = PNG_IMAGE_VERSION;

    // fetch image properties
    int success = png_image_begin_read_from_memory(&png_img, p_source, p_size);
    ERR_FAIL_COND_V_MSG(check_error(png_img), ERR_FILE_CORRUPT, png_img.message);
    ERR_FAIL_COND_V(!success, ERR_FILE_CORRUPT)

    // flags to be masked out of input format to give target format
    const png_uint_32 format_mask = ~(
            // convert component order to RGBA
            PNG_FORMAT_FLAG_BGR | PNG_FORMAT_FLAG_AFIRST
            // convert 16 bit components to 8 bit
            | PNG_FORMAT_FLAG_LINEAR
            // convert indexed image to direct color
            | PNG_FORMAT_FLAG_COLORMAP);

    png_img.format &= format_mask;

    ImageData::Format dest_format;
    switch (png_img.format) {
        case PNG_FORMAT_GRAY:
            dest_format = ImageData::FORMAT_L8;
            break;
        case PNG_FORMAT_GA:
            dest_format = ImageData::FORMAT_LA8;
            break;
        case PNG_FORMAT_RGB:
            dest_format = ImageData::FORMAT_RGB8;
            break;
        case PNG_FORMAT_RGBA:
            dest_format = ImageData::FORMAT_RGBA8;
            break;
        default:
            png_image_free(&png_img); // only required when we return before finish_read
            ERR_PRINT("Unsupported png format.")
            return ERR_UNAVAILABLE;
    }

    const png_uint_32 stride = PNG_IMAGE_ROW_STRIDE(png_img);
    Error err = p_image.data.resize(PNG_IMAGE_BUFFER_SIZE(png_img, stride));
    if (err) {
        png_image_free(&png_img); // only required when we return before finish_read
        return err;
    }
    PoolVector<uint8_t>::Write writer = p_image.data.write();

    // read image data to buffer and release libpng resources
    success = png_image_finish_read(&png_img, nullptr, writer.ptr(), stride, nullptr);
    ERR_FAIL_COND_V_MSG(check_error(png_img), ERR_FILE_CORRUPT, png_img.message);
    ERR_FAIL_COND_V(!success, ERR_FILE_CORRUPT)

    p_image.width = png_img.width;
    p_image.height = png_img.height;
    p_image.mipmaps = false;
    p_image.format = dest_format;

    return OK;
}

Error image_to_png(const ImageData &source_image, PODVector<uint8_t> &p_buffer) {

    png_image png_img;
    memset(&png_img, 0, sizeof(png_img));
    png_img.version = PNG_IMAGE_VERSION;
    png_img.width = source_image.width;
    png_img.height = source_image.height;

    switch (source_image.format) {
        case ImageData::FORMAT_L8:
            png_img.format = PNG_FORMAT_GRAY;
            break;
        case ImageData::FORMAT_LA8:
            png_img.format = PNG_FORMAT_GA;
            break;
        case ImageData::FORMAT_RGB8:
            png_img.format = PNG_FORMAT_RGB;
            break;
        case ImageData::FORMAT_RGBA8:
            png_img.format = PNG_FORMAT_RGBA;
            break;
        default:
        return ERR_INVALID_DATA;
    }

    const PoolVector<uint8_t> &image_data = source_image.data;
    const PoolVector<uint8_t>::Read reader = image_data.read();

    // we may be passed a buffer with existing content we're expected to append to
    const int buffer_offset = p_buffer.size();

    const size_t png_size_estimate = PNG_IMAGE_PNG_SIZE_MAX(png_img);

    // try with estimated size
    size_t compressed_size = png_size_estimate;
    int success = 0;
    { // scope writer lifetime
        p_buffer.resize(buffer_offset + png_size_estimate);
        success = png_image_write_to_memory(&png_img, p_buffer.data() + buffer_offset,
                &compressed_size, 0, reader.ptr(), 0, nullptr);
        ERR_FAIL_COND_V_MSG(check_error(png_img), FAILED, png_img.message);
    }
    if (!success) {

        // buffer was big enough, must be some other error
        ERR_FAIL_COND_V(compressed_size <= png_size_estimate, FAILED)

        // write failed due to buffer size, resize and retry
        p_buffer.resize(buffer_offset + compressed_size);

        success = png_image_write_to_memory(&png_img, p_buffer.data()+buffer_offset,
                &compressed_size, 0, reader.ptr(), 0, nullptr);
        ERR_FAIL_COND_V_MSG(check_error(png_img), FAILED, png_img.message);
        ERR_FAIL_COND_V(!success, FAILED)
    }

    // trim buffer size to content
    p_buffer.resize(buffer_offset + compressed_size);
    p_buffer.shrink_to_fit();
    return OK;
}

} // namespace PNGDriverCommon
