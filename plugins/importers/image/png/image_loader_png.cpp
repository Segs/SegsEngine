/*************************************************************************/
/*  image_loader_png.cpp                                                 */
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

#include "image_loader_png.h"

#include "core/os/os.h"
#include "core/image_data.h"
#include "core/print_string.h"
#include "png_driver_common.h"

#include <string.h>

Error ImageLoaderPNG::load_image(ImageData &p_image, FileAccess *f, LoadParams params) {

    const size_t buffer_size = f->get_len();
    PoolVector<uint8_t> file_buffer;
    Error err = file_buffer.resize(buffer_size);
    if (err) {
        f->close();
        return err;
    }
    {
        PoolVector<uint8_t>::Write writer = file_buffer.write();
        f->get_buffer(writer.ptr(), buffer_size);
        f->close();
    }
    PoolVector<uint8_t>::Read reader = file_buffer.read();
    return PNGDriverCommon::png_to_image(reader.ptr(), buffer_size, p_image);
}

Error ImageLoaderPNG::load_image(ImageData &p_image,const uint8_t *p_png, int p_size,LoadParams params) {

    Error err = PNGDriverCommon::png_to_image(p_png, p_size, p_image);
    ERR_FAIL_COND_V(err, ERR_CANT_OPEN)

    return OK;
}
void ImageLoaderPNG::get_recognized_extensions(List<String> *p_extensions) const {

    p_extensions->push_back(String("png"));
}

ImageData ImageLoaderPNG::load_mem_png(const uint8_t *p_png, int p_size) {
    ImageData dat;
    Error err = PNGDriverCommon::png_to_image(p_png, p_size, dat);
    ERR_FAIL_COND_V(err, ImageData())
    return dat;
}

ImageData ImageLoaderPNG::lossless_unpack_png(const PoolVector<uint8_t> &p_data) {

    const int len = p_data.size();
    ERR_FAIL_COND_V(len < 4, ImageData())
    PoolVector<uint8_t>::Read r = p_data.read();
    ERR_FAIL_COND_V(r[0] != 'P' || r[1] != 'N' || r[2] != 'G' || r[3] != ' ', ImageData())
    return load_mem_png(&r[4], len - 4);
}

PoolVector<uint8_t> ImageLoaderPNG::lossless_pack_png(const ImageData &p_image) {

    PoolVector<uint8_t> out_buffer;

    // add Godot's own "PNG " prefix
    if (out_buffer.resize(4) != OK) {
        ERR_FAIL_V(PoolVector<uint8_t>())
    }

    // scope for writer lifetime
    {
        // must be closed before call to image_to_png
        PoolVector<uint8_t>::Write writer = out_buffer.write();
        memcpy(writer.ptr(), "PNG ", 4);
    }

    Error err = PNGDriverCommon::image_to_png(p_image, out_buffer);
    if (err) {
        ERR_REPORT_COND("Can't convert image to PNG.")
        return PoolVector<uint8_t>();
    }

    return out_buffer;
}


Error ImageLoaderPNG::save_image(const ImageData &p_image, PoolVector<uint8_t> &tgt, SaveParams params)
{
    tgt = lossless_pack_png(p_image);
    return tgt.size()==0 ? ERR_CANT_CREATE : OK;
}

Error ImageLoaderPNG::save_image(const ImageData &p_image, FileAccess *p_fileaccess, SaveParams params)
{
    PoolVector<uint8_t> tgt = lossless_pack_png(p_image);
    if(tgt.size()==0)
        return ERR_CANT_CREATE;
    auto reader = tgt.read();
    p_fileaccess->store_buffer(reader.ptr(), tgt.size());
    if (p_fileaccess->get_error() != OK && p_fileaccess->get_error() != ERR_FILE_EOF) {
        return ERR_CANT_CREATE;
    }
    return OK;
}

bool ImageLoaderPNG::can_save(const String &extension)
{
    return "png"==extension;
}

ImageLoaderPNG::ImageLoaderPNG() {
}
