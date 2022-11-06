/*************************************************************************/
/*  resource_saver.cpp                                                   */
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

#include "resource_saver.h"
#include "core/method_info.h"
#include "core/class_db.h"
#include "core/io/resource_loader.h"
#include "core/io/image_saver.h"
#include "core/plugin_interfaces/ImageLoaderInterface.h"
#include "core/os/file_access.h"
#include "core/project_settings.h"
#include "core/string_utils.h"
#include "core/script_language.h"
#include "core/object_tooling.h"

#include "scene/resources/texture.h"
#include "EASTL/deque.h"

IMPL_GDCLASS(ResourceFormatSaver)

enum {
    MAX_SAVERS = 64
};

//static eastl::deque<Ref<ResourceFormatSaver>> saver;

Error ResourceFormatSaver::save(StringView p_path, const Ref<Resource> &p_resource, uint32_t p_flags) {

    if (get_script_instance() && get_script_instance()->has_method("save")) {
        return (Error)get_script_instance()->call("save", p_path, p_resource, p_flags).as<int64_t>();
    }
    Ref<ImageTexture> texture = dynamic_ref_cast<ImageTexture>( p_resource );
    if(texture) {
        ERR_FAIL_COND_V_MSG(!texture->get_width(), ERR_INVALID_PARAMETER, "Can't save empty texture as PNG.");
        Ref<Image> img = texture->get_data();
        Ref<Image> source_image = prepareForPngStorage(img);
        return ImageSaver::save_image(p_path,source_image);
    }

    return ERR_METHOD_NOT_FOUND;
}

bool ResourceFormatSaver::recognize(const Ref<Resource> &p_resource) const {

    if (get_script_instance() && get_script_instance()->has_method("recognize")) {
        return get_script_instance()->call("recognize", p_resource).as<bool>();
    }
    return p_resource && p_resource->is_class("ImageTexture");
}

void ResourceFormatSaver::get_recognized_extensions(const Ref<Resource> &p_resource, Vector<String> &p_extensions) const {

    if (get_script_instance() && get_script_instance()->has_method("get_recognized_extensions")) {
        PoolVector<String> exts = get_script_instance()->call("get_recognized_extensions", p_resource).as<PoolVector<String>>();

        {
            PoolVector<String>::Read r = exts.read();
            for (int i = 0; i < exts.size(); ++i) {
                p_extensions.push_back(r[i]);
            }
        }
    }
    if (object_cast<ImageTexture>(p_resource.get())) {
        //TODO: use resource name here ?
        auto saver = ImageSaver::recognize("png");
        saver->get_saved_extensions(p_extensions);
    }
}

void ResourceFormatSaver::_bind_methods() {

    {
        PropertyInfo arg0 = PropertyInfo(VariantType::STRING, "path");
        PropertyInfo arg1 = PropertyInfo(VariantType::OBJECT, "resource", PropertyHint::ResourceType, "Resource");
        PropertyInfo arg2 = PropertyInfo(VariantType::INT, "flags");
        ClassDB::add_virtual_method(get_class_static_name(),
                MethodInfo(VariantType::INT, "save", eastl::move(arg0), eastl::move(arg1), eastl::move(arg2)));
    }

    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::POOL_STRING_ARRAY, "get_recognized_extensions", PropertyInfo(VariantType::OBJECT, "resource", PropertyHint::ResourceType, "Resource")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::BOOL, "recognize", PropertyInfo(VariantType::OBJECT, "resource", PropertyHint::ResourceType, "Resource")));
}



