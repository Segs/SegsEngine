/*************************************************************************/
/*  image_loader.cpp                                                     */
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

#include "image_saver.h"

#include "core/plugin_interfaces/ImageLoaderInterface.h"
#include "core/print_string.h"
#include "core/ustring.h"

#include <QPluginLoader>

Error ImageSaver::save_image(String p_file, const Ref<Image> &p_image, FileAccess *p_custom, float p_quality) {
    ERR_FAIL_COND_V(p_image.is_null(), ERR_INVALID_PARAMETER)

    FileAccess *f = p_custom;
    if (!f) {
        Error err;
        f = FileAccess::open(p_file, FileAccess::WRITE, &err);
        if (!f) {
            ERR_PRINTS("Error opening file: " + p_file)
            return err;
        }
    }

	String extension = PathUtils::get_extension(p_file);

    for (int i = 0; i < savers.size(); i++) {

        if (!savers[i]->can_save(extension))
            continue;
        ImageData result_data;
        Error err = savers[i]->save_image(result_data, f, {p_quality,false});
        if (err != OK) {
            ERR_PRINTS("Error saving image: " + p_file)
        }
        if (err != ERR_FILE_UNRECOGNIZED) {

            if (!p_custom)
                memdelete(f);
            CRASH_COND(err!=OK)
            return err;
        }
    }

    if (!p_custom)
        memdelete(f);

    return ERR_FILE_UNRECOGNIZED;
}

Error ImageSaver::save_image(String ext, const Ref<Image> & p_image, PoolVector<uint8_t> &tgt, float p_quality)
{
    ImageData result_data;

    for (int i = 0; i < savers.size(); i++) {

        if (!savers[i]->can_save(ext))
            continue;
        Error err = savers[i]->save_image(*p_image.ptr(), tgt, {p_quality,false});
        if (err != OK) {
            ERR_PRINTS("Error loading image from memory")
        }
        if (err != ERR_FILE_UNRECOGNIZED) {
            CRASH_COND(err!=OK)
            return err;
        }

    }
    return ERR_FILE_UNRECOGNIZED;
}

void ImageSaver::get_recognized_extensions(List<String> *p_extensions) {

    for (int i = 0; i < savers.size(); i++) {

        savers[i]->get_saved_extensions(p_extensions);
    }
}

ImageFormatSaver *ImageSaver::recognize(const String &p_extension) {

    for (int i = 0; i < savers.size(); i++) {

        if (savers[i]->can_save(p_extension))
            return savers[i];
    }

    return nullptr;
}

Vector<ImageFormatSaver *> ImageSaver::savers;

void ImageSaver::add_image_format_saver(ImageFormatSaver *p_loader) {

    savers.push_back(p_loader);
}

void ImageSaver::remove_image_format_saver(ImageFormatSaver *p_loader) {

    savers.erase(p_loader);
}

const Vector<ImageFormatSaver *> &ImageSaver::get_image_format_savers() {

    return savers;
}

void ImageSaver::cleanup() {

    while (!savers.empty()) {
        remove_image_format_saver(savers[0]);
    }
}
