/*************************************************************************/
/*  image_loader_webp.cpp                                                */
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

#include "image_loader_webp.h"

#include "core/image_data.h"
#include "core/os/os.h"
#include "core/print_string.h"

#include <stdlib.h>
#include <webp/decode.h>
#include <webp/encode.h>

static PoolVector<uint8_t> _webp_lossy_pack(const ImageData &p_image, float p_quality) {

    ERR_FAIL_COND_V(p_image.data.size()==0, PoolVector<uint8_t>())

    ERR_FAIL_COND_V(p_image.format!=ImageData::FORMAT_RGBA8 && p_image.format!=ImageData::FORMAT_RGB8, PoolVector<uint8_t>())

    Size2 s(p_image.width, p_image.height);
    const PoolVector<uint8_t> &data = p_image.data;
    PoolVector<uint8_t>::Read r = data.read();

    uint8_t *dst_buff = nullptr;
    size_t dst_size = 0;
    if (p_image.format == ImageData::FORMAT_RGB8) {

        dst_size = WebPEncodeRGB(r.ptr(), s.width, s.height, 3 * s.width, CLAMP(p_quality * 100.0, 0, 100.0), &dst_buff);
    } else {
        dst_size = WebPEncodeRGBA(r.ptr(), s.width, s.height, 4 * s.width, CLAMP(p_quality * 100.0, 0, 100.0), &dst_buff);
    }

    ERR_FAIL_COND_V(dst_size == 0, PoolVector<uint8_t>());
    PoolVector<uint8_t> dst;
    dst.resize(4 + dst_size);
    PoolVector<uint8_t>::Write w = dst.write();
    w[0] = 'W';
    w[1] = 'E';
    w[2] = 'B';
    w[3] = 'P';
    memcpy(&w[4], dst_buff, dst_size);
    free(dst_buff);
    w.release();
    return dst;
}

Error webp_load_image_from_buffer(ImageData &p_image, const uint8_t *p_buffer, int p_buffer_len) {

    WebPBitstreamFeatures features;
    if (WebPGetFeatures(p_buffer, p_buffer_len, &features) != VP8_STATUS_OK) {
        ERR_FAIL_V(ERR_FILE_CORRUPT)
    }

    int datasize = features.width * features.height * (features.has_alpha ? 4 : 3);
    p_image.data.resize(datasize);
    PoolVector<uint8_t>::Write dst_w = p_image.data.write();

    bool errdec = false;
    if (features.has_alpha) {
        errdec = WebPDecodeRGBAInto(p_buffer, p_buffer_len, dst_w.ptr(), datasize, 4 * features.width) == nullptr;
    } else {
        errdec = WebPDecodeRGBInto(p_buffer, p_buffer_len, dst_w.ptr(), datasize, 3 * features.width) == nullptr;
    }
    dst_w.release();

    ERR_FAIL_COND_V_MSG(errdec, ERR_FILE_CORRUPT, "Failed decoding WebP image.")

    p_image.width=features.width;
    p_image.height=features.height;
    p_image.mipmaps=0;
    p_image.format = features.has_alpha ? ImageData::FORMAT_RGBA8 : ImageData::FORMAT_RGB8;

    return OK;
}

Error ImageLoaderWEBP::load_image(ImageData &p_image, FileAccess *f, LoadParams params) {

    PoolVector<uint8_t> src_image;
    int src_image_len = f->get_len();
    ERR_FAIL_COND_V(src_image_len == 0, ERR_FILE_CORRUPT)
    src_image.resize(src_image_len);

    PoolVector<uint8_t>::Write w = src_image.write();

    f->get_buffer(&w[0], src_image_len);

    f->close();

    Error err = webp_load_image_from_buffer(p_image, w.ptr(), src_image_len);

    return err;
}

Error ImageLoaderWEBP::save_image(const ImageData &p_image, PoolVector<uint8_t> &tgt, SaveParams params)
{
    tgt = _webp_lossy_pack(p_image,params.p_quality);
    return tgt.size()==0 ? ERR_CANT_CREATE : OK;
}
Error ImageLoaderWEBP::save_image(const ImageData &p_image, FileAccess *p_fileaccess, SaveParams params)
{
    PoolVector<uint8_t> tgt = _webp_lossy_pack(p_image,params.p_quality);
    if(tgt.size()==0)
        return ERR_CANT_CREATE;
    auto reader = tgt.read();
    p_fileaccess->store_buffer(reader.ptr(), tgt.size());
    if (p_fileaccess->get_error() != OK && p_fileaccess->get_error() != ERR_FILE_EOF) {
        return ERR_CANT_CREATE;
    }
    return OK;
}

bool ImageLoaderWEBP::can_save(const String &extension)
{
    return "webp"==extension;
}

void ImageLoaderWEBP::get_recognized_extensions(List<String> *p_extensions) const {

    p_extensions->push_back("webp");
}

ImageLoaderWEBP::ImageLoaderWEBP() {
}
