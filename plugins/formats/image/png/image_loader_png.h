/*************************************************************************/
/*  image_loader_png.h                                                   */
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

#include "core/plugin_interfaces/PluginDeclarations.h"

class ImageLoaderPNG : public QObject, public ImageFormatLoader, public ImageFormatSaver {
    Q_PLUGIN_METADATA(IID "org.godot.ImageLoaderPNG")
    Q_INTERFACES(ImageFormatLoader ImageFormatSaver)
    Q_OBJECT

private:
    static ImageData load_mem_png(const uint8_t *p_png, int p_size);

public:
    Error load_image(ImageData &p_image, FileAccess *f, LoadParams params={}) override;
    Error load_image(ImageData &p_image, const uint8_t *data, int size, LoadParams params={}) override;
    void get_recognized_extensions(PODVector<String> &p_extensions) const override;

    void get_saved_extensions(PODVector<String> &p_extensions) const override {
        this->get_recognized_extensions(p_extensions);
    }
    Error save_image(const ImageData &p_image, PODVector<uint8_t> &tgt, SaveParams params) override;
    Error save_image(const ImageData &p_image, FileAccess *p_fileaccess, SaveParams params) override;
    bool can_save(se_string_view extension) override;


    ImageLoaderPNG() = default;
};
