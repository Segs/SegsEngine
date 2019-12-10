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

#include "image_loader.h"

#include "core/image.h"
#include "core/print_string.h"
#include "core/plugin_interfaces/PluginDeclarations.h"
#include "plugins/plugin_registry_interface.h"
#include "core/string_utils.h"

namespace
{
struct ImagePluginResolver : public ResolverInterface
{
    bool new_plugin_detected(QObject * ob) override {
        bool res=false;
        auto image_loader_interface = qobject_cast<ImageFormatLoader *>(ob);
        if (image_loader_interface) {
            print_line(se_string("Adding image loader:")+ob->metaObject()->className());
            ImageLoader::add_image_format_loader(image_loader_interface);
            res=true;
        }
        return res;
    }
    void plugin_removed(QObject * ob)  override  {
        auto image_loader_interface = qobject_cast<ImageFormatLoader *>(ob);
        if(image_loader_interface) {
            print_line(se_string("Removing image loader:")+ob->metaObject()->className());
            ImageLoader::remove_image_format_loader(image_loader_interface);
        }
    }

};

bool loader_recognizes(const ImageFormatLoader *ldr,se_string_view p_extension) {

    PODVector<se_string> extensions;
    ldr->get_recognized_extensions(extensions);
    for (const se_string & e : extensions) {

        if (StringUtils::compare(e,p_extension,StringUtils::CaseInsensitive) == 0)
            return true;
    }

    return false;
}
}

void ImageLoader::register_plugin_resolver()
{
    static bool registered=false;
    if(!registered) {
        add_plugin_resolver(new ImagePluginResolver);
        registered = true;
    }
}

Error ImageLoader::load_image(se_string_view p_file, const Ref<Image> &p_image, FileAccess *p_custom, const LoadParams &params) {
    ERR_FAIL_COND_V_MSG(not p_image, ERR_INVALID_PARAMETER, "It's not a reference to a valid Image object.")

    register_plugin_resolver();

    FileAccess *f = p_custom;
    if (!f) {
        Error err;
        f = FileAccess::open(p_file, FileAccess::READ, &err);
        if (!f) {
            ERR_PRINT("Error opening file '" + se_string(p_file)+"'.")
            return err;
        }
    }

    se_string extension(PathUtils::get_extension(p_file));

    for (int i = 0; i < loader.size(); i++) {

        if (!loader_recognizes(loader[i],extension))
            continue;
        ImageData result_data;
        Error err = loader[i]->load_image(result_data, f, params);
        if (err != OK) {
            ERR_PRINT("Error loading image: " + se_string(p_file))
        }
        else
            p_image->create(std::move(result_data));
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

ImageData ImageLoader::load_image(se_string_view extension, const uint8_t *data, int sz, const LoadParams &params)
{
    register_plugin_resolver();

    ImageData result_data;
    bool loader_found=false;
    for (int i = 0; i < loader.size(); i++) {

        if (!loader_recognizes(loader[i],extension))
            continue;
        loader_found = true;
        Error err = loader[i]->load_image(result_data, data,sz, params);
        if (err != OK) {
            ERR_PRINT("Error loading image from memory")
        }
        else
            return result_data;
        if (err != ERR_FILE_UNRECOGNIZED) {
            CRASH_COND(err!=OK)
            return {};
        }

    }
    if(!loader_found)
        ERR_PRINT("No loader found for file with extension:"+se_string(extension))
    return result_data;
}

void ImageLoader::get_recognized_extensions(PODVector<se_string> &p_extensions) {
    register_plugin_resolver();

    for (int i = 0; i < loader.size(); i++) {

        loader[i]->get_recognized_extensions(p_extensions);
    }
}

ImageFormatLoader *ImageLoader::recognize(se_string_view p_extension) {
    register_plugin_resolver();

    for (int i = 0; i < loader.size(); i++) {

        if (loader_recognizes(loader[i],p_extension))
            return loader[i];
    }

    return nullptr;
}

Vector<ImageFormatLoader *> ImageLoader::loader;

void ImageLoader::add_image_format_loader(ImageFormatLoader *p_loader) {

    loader.push_back(p_loader);
}

void ImageLoader::remove_image_format_loader(ImageFormatLoader *p_loader) {

    loader.erase(p_loader);
}

const Vector<ImageFormatLoader *> &ImageLoader::get_image_format_loaders() {
    register_plugin_resolver();

    return loader;
}

void ImageLoader::cleanup() {

    while (!loader.empty()) {
        remove_image_format_loader(loader[0]);
    }
}

/////////////////

RES ResourceFormatLoaderImage::load(se_string_view p_path, se_string_view p_original_path, Error *r_error) {

    FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
    if (!f) {
        if (r_error) {
            *r_error = ERR_CANT_OPEN;
        }
        return RES();
    }

    uint8_t header[4] = { 0, 0, 0, 0 };
    f->get_buffer(header, 4);

    bool unrecognized = header[0] != 'G' || header[1] != 'D' || header[2] != 'I' || header[3] != 'M';
    if (unrecognized) {
        memdelete(f);
        if (r_error) {
            *r_error = ERR_FILE_UNRECOGNIZED;
        }
        ERR_FAIL_V(RES())
    }

    se_string extension = f->get_pascal_string();

    int idx = -1;

    for (int i = 0; i < ImageLoader::loader.size(); i++) {
        if (loader_recognizes(ImageLoader::loader[i],extension)) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        memdelete(f);
        if (r_error) {
            *r_error = ERR_FILE_UNRECOGNIZED;
        }
        ERR_FAIL_V(RES())
    }

    ImageData resdata;
    Error err = ImageLoader::loader[idx]->load_image(resdata, f);
    memdelete(f);

    if (err != OK) {
        if (r_error) {
            *r_error = err;
        }
        return RES();
    }

    if (r_error) {
        *r_error = OK;
    }

    Ref<Image> image(make_ref_counted<Image>());

    image->create(std::move(resdata));

    return image;
}

void ResourceFormatLoaderImage::get_recognized_extensions(PODVector<se_string> &p_extensions) const {

    p_extensions.push_back("image");
}

bool ResourceFormatLoaderImage::handles_type(se_string_view p_type) const {

    return p_type == se_string_view("Image");
}

se_string ResourceFormatLoaderImage::get_resource_type(se_string_view p_path) const {

    return StringUtils::to_lower(PathUtils::get_extension(p_path)) == "image" ? "Image" : se_string();
}
