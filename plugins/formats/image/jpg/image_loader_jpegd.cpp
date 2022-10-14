/*************************************************************************/
/*  image_loader_jpegd.cpp                                               */
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

#include "image_loader_jpegd.h"

#include "core/image_data.h"
#include "core/print_string.h"
#include "core/os/file_access.h"
#include "core/ustring.h"

#include <jpgd.h>
#include <cstring>

Error jpeg_load_image_from_buffer(ImageData &p_image, const uint8_t *p_buffer, int p_buffer_len) {

    jpgd::jpeg_decoder_mem_stream mem_stream(p_buffer, p_buffer_len);

    jpgd::jpeg_decoder decoder(&mem_stream);

    if (decoder.get_error_code() != jpgd::JPGD_SUCCESS) {
        return ERR_CANT_OPEN;
    }

    const int image_width = decoder.get_width();
    const int image_height = decoder.get_height();
    const int comps = decoder.get_num_components();
    if (comps != 1 && comps != 3)
        return ERR_FILE_CORRUPT;

    if (decoder.begin_decoding() != jpgd::JPGD_SUCCESS)
        return ERR_FILE_CORRUPT;

    const int dst_bpl = image_width * comps;

    p_image.data.resize(dst_bpl * image_height);

    PoolVector<uint8_t>::Write dw = p_image.data.write();

    jpgd::uint8 *pImage_data = (jpgd::uint8 *)dw.ptr();

    for (int y = 0; y < image_height; y++) {
        const jpgd::uint8 *pScan_line;
        jpgd::uint scan_line_len;
        if (decoder.decode((const void **)&pScan_line, &scan_line_len) != jpgd::JPGD_SUCCESS) {
            return ERR_FILE_CORRUPT;
        }

        jpgd::uint8 *pDst = pImage_data + y * dst_bpl;

        if (comps == 1) {
            memcpy(pDst, pScan_line, dst_bpl);
        } else {
            // For images with more than 1 channel pScan_line will always point to a buffer
            // containing 32-bit RGBA pixels. Alpha is always 255 and we ignore it.
            for (int x = 0; x < image_width; x++) {
                pDst[0] = pScan_line[x * 4 + 0];
                pDst[1] = pScan_line[x * 4 + 1];
                pDst[2] = pScan_line[x * 4 + 2];
                pDst += 3;
            }
        }
    }

    //all good

    ImageData::Format fmt;
    if (comps == 1)
        fmt = ImageData::FORMAT_L8;
    else
        fmt = ImageData::FORMAT_RGB8;

    dw.release();
    p_image.width= image_width;
    p_image.height = image_height;
    p_image.mipmaps = false;
    p_image.format = fmt;

    return OK;
}

Error ImageLoaderJPG::load_image(ImageData &p_image, FileAccess *f, LoadParams params) {

    PoolVector<uint8_t> src_image;
    uint64_t src_image_len = f->get_len();
    ERR_FAIL_COND_V(src_image_len == 0, ERR_FILE_CORRUPT);
    src_image.resize(src_image_len);

    PoolVector<uint8_t>::Write w = src_image.write();

    f->get_buffer(&w[0], src_image_len);

    f->close();

    Error err = jpeg_load_image_from_buffer(p_image, w.ptr(), src_image_len);

    return err;
}

void ImageLoaderJPG::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.push_back("jpg");
    p_extensions.push_back("jpeg");
}

ImageLoaderJPG::ImageLoaderJPG() {
}
