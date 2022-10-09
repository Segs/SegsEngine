/*************************************************************************/
/*  image_loader_webp.cpp                                                */
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

#include "image_loader_webp.h"

#include "core/image.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/project_settings.h"

#include <cstdlib>
#include <webp/decode.h>
#include <webp/encode.h>

static Vector<uint8_t> _webp_lossy_pack(const ImageData &p_image, float p_quality) {

    ERR_FAIL_COND_V(p_image.data.size()==0, Vector<uint8_t>());

    ERR_FAIL_COND_V(p_image.format!=ImageData::FORMAT_RGBA8 && p_image.format!=ImageData::FORMAT_RGB8, Vector<uint8_t>());

    Size2 s(p_image.width, p_image.height);
    const PoolVector<uint8_t> &data = p_image.data;
    PoolVector<uint8_t>::Read r = data.read();

    uint8_t *dst_buff = nullptr;
    size_t dst_size = 0;
    if (p_image.format == ImageData::FORMAT_RGB8) {

        dst_size = WebPEncodeRGB(r.ptr(), s.width, s.height, 3 * s.width, CLAMP(p_quality * 100.0f, 0.0f, 100.0f), &dst_buff);
    } else {
        dst_size = WebPEncodeRGBA(r.ptr(), s.width, s.height, 4 * s.width, CLAMP(p_quality * 100.0f, 0.0f, 100.0f), &dst_buff);
    }

    ERR_FAIL_COND_V(dst_size == 0, Vector<uint8_t>());
    Vector<uint8_t> dst;
    dst.resize(4 + dst_size);
    dst[0] = 'W';
    dst[1] = 'E';
    dst[2] = 'B';
    dst[3] = 'P';
    memcpy(dst.data()+4, dst_buff, dst_size);
    WebPFree(dst_buff);
    return dst;
}

static Vector<uint8_t> _webp_lossless_pack(const ImageData &p_image) {
    ERR_FAIL_COND_V(p_image.data.empty(), {});

    int compression_level = ProjectSettings::get_singleton()->getT<int>("rendering/misc/lossless_compression/webp_compression_level");
    compression_level = CLAMP(compression_level, 0, 9);

    Image img(eastl::move(ImageData(p_image)));

    if (img.detect_alpha()) {
        img.convert(ImageData::FORMAT_RGBA8);
    } else {
        img.convert(ImageData::FORMAT_RGB8);
    }

    Size2 s(img.get_width(), img.get_height());
    PoolVector<uint8_t> data = img.get_data();
    PoolVector<uint8_t>::Read r = data.read();

    // we need to use the more complex API in order to access the 'exact' flag...

    WebPConfig config;
    WebPPicture pic;
    if (!WebPConfigInit(&config) || !WebPConfigLosslessPreset(&config, compression_level) || !WebPPictureInit(&pic)) {
        ERR_FAIL_V({});
    }

    WebPMemoryWriter wrt;
    config.exact = 1;
    pic.use_argb = 1;
    pic.width = s.width;
    pic.height = s.height;
    pic.writer = WebPMemoryWrite;
    pic.custom_ptr = &wrt;
    WebPMemoryWriterInit(&wrt);

    bool success_import = false;
    if (img.get_format() == ImageData::FORMAT_RGB8) {
        success_import = WebPPictureImportRGB(&pic, r.ptr(), 3 * s.width);
    } else {
        success_import = WebPPictureImportRGBA(&pic, r.ptr(), 4 * s.width);
    }
    bool success_encode = false;
    if (success_import) {
        success_encode = WebPEncode(&config, &pic);
    }
    WebPPictureFree(&pic);

    if (!success_encode) {
        WebPMemoryWriterClear(&wrt);
        ERR_FAIL_V_MSG({}, "WebP packing failed.");
    }

    // copy from wrt
    Vector<uint8_t> dst;
    dst.resize(4 + wrt.size);
    dst[0] = 'W';
    dst[1] = 'E';
    dst[2] = 'B';
    dst[3] = 'P';
    memcpy(dst.data()+4, wrt.mem, wrt.size);
    WebPMemoryWriterClear(&wrt);
    return dst;
}

Error webp_load_image_from_buffer(ImageData &p_image, const uint8_t *p_buffer, int p_buffer_len) {

    WebPBitstreamFeatures features;
    if (WebPGetFeatures(p_buffer, p_buffer_len, &features) != VP8_STATUS_OK) {
        ERR_FAIL_V(ERR_FILE_CORRUPT);
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

    ERR_FAIL_COND_V_MSG(errdec, ERR_FILE_CORRUPT, "Failed decoding WebP image.");

    p_image.width=features.width;
    p_image.height=features.height;
    p_image.mipmaps=false;
    p_image.format = features.has_alpha ? ImageData::FORMAT_RGBA8 : ImageData::FORMAT_RGB8;

    return OK;
}

Error ImageLoaderWEBP::load_image(ImageData &p_image, FileAccess *f, LoadParams params) {

    PoolVector<uint8_t> src_image;
    uint64_t src_image_len = f->get_len();
    ERR_FAIL_COND_V(src_image_len == 0, ERR_FILE_CORRUPT);
    src_image.resize(src_image_len);

    PoolVector<uint8_t>::Write w = src_image.write();

    f->get_buffer(&w[0], src_image_len);

    f->close();

    Error err = webp_load_image_from_buffer(p_image, w.ptr(), src_image_len);

    return err;
}

Error ImageLoaderWEBP::save_image(const ImageData &p_image, Vector<uint8_t> &tgt, SaveParams params)
{
    if(params.p_lossless)
        tgt = _webp_lossless_pack(p_image);
    else
    tgt = _webp_lossy_pack(p_image,params.p_quality);
    return tgt.size()==0 ? ERR_CANT_CREATE : OK;
}
Error ImageLoaderWEBP::save_image(const ImageData &p_image, FileAccess *p_fileaccess, SaveParams params)
{
    Vector<uint8_t> tgt;
    if(params.p_lossless) {
        tgt = _webp_lossless_pack(p_image);
    }
    else {
        tgt = _webp_lossy_pack(p_image,params.p_quality);
    }
    if(tgt.empty()) {
        return ERR_CANT_CREATE;
    }
    p_fileaccess->store_buffer(tgt.data(), tgt.size());
    if (p_fileaccess->get_error() != OK && p_fileaccess->get_error() != ERR_FILE_EOF) {
        return ERR_CANT_CREATE;
    }
    return OK;
}

bool ImageLoaderWEBP::can_save(StringView extension)
{
    return StringView("webp")==extension;
}

void ImageLoaderWEBP::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.emplace_back("webp");
}

ImageLoaderWEBP::ImageLoaderWEBP() {
}
