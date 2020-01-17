/*************************************************************************/
/*  image_loader.h                                                       */
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

//#include "core/image.h"
#include "core/io/resource_format_loader.h"
#include "core/list.h"
#include "core/vector.h"
#include "core/os/file_access.h"

#include "core/plugin_interfaces/load_params.h"


class ImageFormatLoader;
struct LoadParams;
template <class T>
class Ref;
template <class T>
class PoolVector;
class Image;
struct ImageData;

//TODO: SEGS - convert ImageLoader to singelton, so we can have a single initialization point at which to register plugin resolver
class GODOT_EXPORT ImageLoader {

    static PODVector<ImageFormatLoader *> loader;
    friend class ResourceFormatLoaderImage;

protected:
public:
    static void register_plugin_resolver();
    static Error load_image(se_string_view p_file, const Ref<Image> &p_image, FileAccess *p_custom = nullptr, const LoadParams &params={});
    static ImageData load_image(se_string_view ext, const uint8_t *data, int sz, const LoadParams &params={});
    static void get_recognized_extensions(PODVector<String> &p_extensions);
    static ImageFormatLoader *recognize(se_string_view p_extension);

    static void add_image_format_loader(ImageFormatLoader *p_loader);
    static void remove_image_format_loader(ImageFormatLoader *p_loader);

    static const PODVector<ImageFormatLoader *> &get_image_format_loaders();

    static void cleanup();
};

class ResourceFormatLoaderImage : public ResourceFormatLoader {
public:
    RES load(se_string_view p_path, se_string_view p_original_path = se_string_view(), Error *r_error = nullptr) override;
    void get_recognized_extensions(PODVector<String> &p_extensions) const override;
    bool handles_type(se_string_view p_type) const override;
    String get_resource_type(se_string_view p_path) const override;
};
