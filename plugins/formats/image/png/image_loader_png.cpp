/*************************************************************************/
/*  image_loader_png.cpp                                                 */
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

#include "image_loader_png.h"

#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/image_data.h"
#include "core/print_string.h"
#include "png_driver_common.h"

#include <cstring>

Error ImageLoaderPNG::load_image(ImageData &p_image, FileAccess *f, LoadParams params) {

    const auto buffer_size = f->get_len();
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
    return PNGDriverCommon::png_to_image(reader.ptr(), buffer_size,params.p_force_linear, p_image);
}

Error ImageLoaderPNG::load_image(ImageData &p_image,const uint8_t *p_png, int p_size,LoadParams params) {

    Error err = PNGDriverCommon::png_to_image(p_png, p_size,params.p_force_linear, p_image);
    ERR_FAIL_COND_V(err, ERR_CANT_OPEN);

    return OK;
}
void ImageLoaderPNG::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.emplace_back("png");
}

ImageData ImageLoaderPNG::load_mem_png(const uint8_t *p_png, int p_size) {
    ImageData dat;
    // the value of p_force_linear does not matter since it only applies to 16 bit
    Error err = PNGDriverCommon::png_to_image(p_png, p_size,false, dat);
    ERR_FAIL_COND_V(err, ImageData());
    return dat;
}

Error ImageLoaderPNG::save_image(const ImageData &p_image, Vector<uint8_t> &tgt, SaveParams params)
{
    Error err = PNGDriverCommon::image_to_png(p_image, tgt);
    return err!=OK ? err : (tgt.empty() ? ERR_CANT_CREATE : OK);
}

Error ImageLoaderPNG::save_image(const ImageData &p_image, FileAccess *p_fileaccess, SaveParams params)
{
    Vector<uint8_t> tgt;
    Error err = PNGDriverCommon::image_to_png(p_image, tgt);
    if(err!=OK)
        return err;

    p_fileaccess->store_buffer(tgt.data(), tgt.size());
    if (p_fileaccess->get_error() != OK && p_fileaccess->get_error() != ERR_FILE_EOF) {
        return ERR_CANT_CREATE;
    }
    return OK;
}

bool ImageLoaderPNG::can_save(StringView extension)
{
    return extension==StringView("png");
}
