/*************************************************************************/
/*  image_loader_hdr.cpp                                                 */
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

#include "image_loader_hdr.h"

#include "core/os/os.h"
#include "core/os/file_access.h"
#include "core/image_data.h"
#include "core/print_string.h"

Error ImageLoaderHDR::load_image(ImageData & p_image, FileAccess *f, LoadParams params) {

    String header = f->get_token();

    ERR_FAIL_COND_V_MSG(header != "#?RADIANCE" && header != "#?RGBE", ERR_FILE_UNRECOGNIZED, "Unsupported header information in HDR: " + header + ".")

    while (true) {
        String line = f->get_line();
        ERR_FAIL_COND_V(f->eof_reached(), ERR_FILE_UNRECOGNIZED)
        if (line.empty()) // empty line indicates end of header
            break;
        if (StringUtils::begins_with(line,"FORMAT=")) { // leave option to implement other commands
			ERR_FAIL_COND_V_CMSG(line != "FORMAT=32-bit_rle_rgbe", ERR_FILE_UNRECOGNIZED, "Only 32-bit_rle_rgbe is supported for HDR files.")
        } else if (!StringUtils::begins_with(line,"#")) { // not comment
            WARN_PRINTS("Ignoring unsupported header information in HDR: " + line + ".")
        }
    }

    String token = f->get_token();

    ERR_FAIL_COND_V(token != "-Y", ERR_FILE_CORRUPT)

    int height = StringUtils::to_int(f->get_token());

    token = f->get_token();

    ERR_FAIL_COND_V(token != "+X", ERR_FILE_CORRUPT)

    int width = StringUtils::to_int(f->get_line());

    p_image.data.resize(height * width * sizeof(uint32_t));

    {

        PoolVector<uint8_t>::Write w = p_image.data.write();

        uint8_t *ptr = (uint8_t *)w.ptr();

        if (width < 8 || width >= 32768) {
            // Read flat data

            f->get_buffer(ptr, width * height * 4);
        } else {
            // Read RLE-encoded data

            for (int j = 0; j < height; ++j) {
                int c1 = f->get_8();
                int c2 = f->get_8();
                int len = f->get_8();
                if (c1 != 2 || c2 != 2 || (len & 0x80)) {
                    // not run-length encoded, so we have to actually use THIS data as a decoded
                    // pixel (note this can't be a valid pixel--one of RGB must be >= 128)

                    ptr[(j * width) * 4 + 0] = uint8_t(c1);
                    ptr[(j * width) * 4 + 1] = uint8_t(c2);
                    ptr[(j * width) * 4 + 2] = uint8_t(len);
                    ptr[(j * width) * 4 + 3] = f->get_8();

                    f->get_buffer(&ptr[(j * width + 1) * 4], (width - 1) * 4);
                    continue;
                }
                len <<= 8;
                len |= f->get_8();

				ERR_FAIL_COND_V_CMSG(len != width, ERR_FILE_CORRUPT, "Invalid decoded scanline length, corrupt HDR.")

                for (int k = 0; k < 4; ++k) {
                    int i = 0;
                    while (i < width) {
                        int count = f->get_8();
                        if (count > 128) {
                            // Run
                            int value = f->get_8();
                            count -= 128;
                            for (int z = 0; z < count; ++z)
                                ptr[(j * width + i++) * 4 + k] = uint8_t(value);
                        } else {
                            // Dump
                            for (int z = 0; z < count; ++z)
                                ptr[(j * width + i++) * 4 + k] = f->get_8();
                        }
                    }
                }
            }
        }

        //convert
        for (int i = 0; i < width * height; i++) {

            float exp = pow(2.0f, ptr[3] - 128.0f);

            Color c(
                    ptr[0] * exp / 255.0f,
                    ptr[1] * exp / 255.0f,
                    ptr[2] * exp / 255.0f);

            if (params.p_force_linear) {
                c = c.to_linear();
            }

            *(uint32_t *)ptr = c.to_rgbe9995();
            ptr += 4;
        }
    }

    p_image.width = width;
    p_image.height= height;
    p_image.mipmaps= false;
    p_image.format = ImageData::FORMAT_RGBE9995;

    return OK;
}

void ImageLoaderHDR::get_recognized_extensions(Vector<String> *p_extensions) const {

	p_extensions->push_back(String("hdr"));
}

ImageLoaderHDR::ImageLoaderHDR() {
}
