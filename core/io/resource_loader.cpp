/*************************************************************************/
/*  resource_loader.cpp                                                  */
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

#include "resource_loader.h"

#include "core/dictionary.h"
#include "core/hash_map.h"
#include "core/method_bind.h"
#include "core/os/file_access.h"
#include "core/os/mutex.h"
#include "core/print_string.h"
#include "core/property_info.h"
#include "core/resource/resource_manager.h"
#include "core/script_language.h"
#include "core/translation.h"

IMPL_GDCLASS(ResourceInteractiveLoader)
IMPL_GDCLASS(ResourceFormatLoader)

Error ResourceInteractiveLoader::wait() {
    Error err = poll();
    while (err == OK) {
        err = poll();
    }

    return err;
}

void ResourceInteractiveLoader::set_no_subresource_cache(bool p_no_subresource_cache) {
    no_subresource_cache = p_no_subresource_cache;
}

bool ResourceInteractiveLoader::get_no_subresource_cache() {
    return no_subresource_cache;
}

ResourceInteractiveLoader::~ResourceInteractiveLoader() {
    if (!path_loading.empty()) {
        gResourceManager().remove_from_loading_map_and_thread(path_loading, path_loading_thread);
    }
}

bool ResourceFormatLoader::recognize_path(StringView p_path, StringView p_for_type) const {
    StringView extension = PathUtils::get_extension(p_path);

    Vector<String> extensions;
    if (p_for_type.empty()) {
        get_recognized_extensions(extensions);
    } else {
        get_recognized_extensions_for_type(p_for_type, extensions);
    }

    for (const String &E : extensions) {
        if (StringUtils::compare(E, extension, StringUtils::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

bool ResourceFormatLoader::handles_type(StringView p_type) const {
    if (get_script_instance() && get_script_instance()->has_method("handles_type")) {
        // I guess custom loaders for custom resources should use "Resource"
        return get_script_instance()->call("handles_type", p_type).as<bool>();
    }

    return false;
}

String ResourceFormatLoader::get_resource_type(StringView p_path) const {
    if (get_script_instance() && get_script_instance()->has_method("get_resource_type")) {
        return get_script_instance()->call("get_resource_type", p_path).as<String>();
    }

    return String();
}

void ResourceFormatLoader::get_recognized_extensions_for_type(StringView p_type, Vector<String> &p_extensions) const {
    if (p_type.empty() || handles_type(p_type)) {
        get_recognized_extensions(p_extensions);
    }
}

void ResourceInteractiveLoader::_bind_methods() {
    BIND_METHOD(ResourceInteractiveLoader,get_resource);
    BIND_METHOD(ResourceInteractiveLoader,poll);
    BIND_METHOD(ResourceInteractiveLoader,wait);
    BIND_METHOD(ResourceInteractiveLoader,get_stage);
    BIND_METHOD(ResourceInteractiveLoader,get_stage_count);
    BIND_METHOD(ResourceInteractiveLoader, set_no_subresource_cache);
    BIND_METHOD(ResourceInteractiveLoader, get_no_subresource_cache);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "no_subresource_cache"), "set_no_subresource_cache", "get_no_subresource_cache");
}

void ResourceInteractiveLoaderDefault::set_translation_remapped(bool p_remapped) {
    resource->set_as_translation_remapped(p_remapped);
}
IMPL_GDCLASS(ResourceInteractiveLoaderDefault)

bool ResourceFormatLoader::exists(StringView p_path) const {
    return FileAccess::exists(p_path); // by default just check file
}

// Warning: Derived classes must override either `load` or `load_interactive`. The base code
// here can trigger an infinite recursion otherwise, since `load` calls `load_interactive`
// vice versa.
Ref<ResourceInteractiveLoader> ResourceFormatLoader::load_interactive(
        StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {
    // Warning: See previous note about the risk of infinite recursion.
    Ref<Resource> res = load(p_path, p_original_path, r_error, p_no_subresource_cache);
    if (not res) {
        return Ref<ResourceInteractiveLoader>();
    }

    Ref<ResourceInteractiveLoaderDefault> ril(make_ref_counted<ResourceInteractiveLoaderDefault>());
    ril->set_no_subresource_cache(p_no_subresource_cache);
    ril->resource = res;
    return ril;
}

void ResourceFormatLoader::get_recognized_extensions(Vector<String> &p_extensions) const {
    if (get_script_instance() && get_script_instance()->has_method("get_recognized_extensions")) {
        p_extensions = get_script_instance()->call("get_recognized_extensions").as<Vector<String>>();
    }
}

RES ResourceFormatLoader::load(StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {
    if (get_script_instance() && get_script_instance()->has_method("load")) {
        Variant res = get_script_instance()->call("load", p_path, p_original_path, p_no_subresource_cache);

        if (res.get_type() == VariantType::INT) { // Error code, abort.
            if (r_error) {
                *r_error = (Error)res.as<int64_t>();
            }
            return RES();
        } else {  // Success, pass on result.
            if (r_error) {
                *r_error = OK;
            }
            return refFromVariant<Resource>(res);
        }
    }

    // Warning: See previous note about the risk of infinite recursion.
    Ref<ResourceInteractiveLoader> ril = load_interactive(p_path, p_original_path, r_error, p_no_subresource_cache);
    if (not ril) {
        return RES();
    }
    ril->set_local_path(p_original_path);

    while (true) {
        Error err = ril->poll();

        if (err == ERR_FILE_EOF) {
            if (r_error) {
                *r_error = OK;
            }
            return ril->get_resource();
        }

        if (r_error) {
            *r_error = err;
        }

        ERR_FAIL_COND_V_MSG(err != OK, RES(), "Failed to load resource '" + String(p_path) + "'.");
    }
}

void ResourceFormatLoader::get_dependencies(StringView p_path, Vector<String> &p_dependencies, bool p_add_types) {
    if (get_script_instance() && get_script_instance()->has_method("get_dependencies")) {
        p_dependencies = get_script_instance()->call("get_dependencies", p_path, p_add_types).as<Vector<String>>();
    }
}

Error ResourceFormatLoader::rename_dependencies(StringView p_path, const HashMap<String, String> &p_map) {
    if (get_script_instance() && get_script_instance()->has_method("rename_dependencies")) {
        Dictionary deps_dict;
        for (const eastl::pair<const String, String> &E : p_map) {
            deps_dict[StringName(E.first)] = E.second;
        }

        int64_t res = get_script_instance()->call("rename_dependencies", deps_dict).as<int64_t>();
        return (Error)res;
    }

    return OK;
}

void ResourceFormatLoader::_bind_methods() {
    {
        MethodInfo info = MethodInfo(VariantType::NIL, "load", PropertyInfo(VariantType::STRING, "path"),
                PropertyInfo(VariantType::STRING, "original_path"), PropertyInfo(VariantType::BOOL, "no_subresource_cache"));
        info.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
        ClassDB::add_virtual_method(get_class_static_name(), info);
    }
    auto cname(get_class_static_name());
    ClassDB::add_virtual_method(cname, MethodInfo(VariantType::POOL_STRING_ARRAY, "get_recognized_extensions"));
    ClassDB::add_virtual_method(cname, MethodInfo(VariantType::BOOL, "handles_type", PropertyInfo(VariantType::STRING_NAME, "typename")));
    ClassDB::add_virtual_method(cname, MethodInfo(VariantType::STRING, "get_resource_type", PropertyInfo(VariantType::STRING, "path")));
    ClassDB::add_virtual_method(cname, MethodInfo("get_dependencies", PropertyInfo(VariantType::STRING, "path"), PropertyInfo(VariantType::STRING, "add_types")));
    ClassDB::add_virtual_method(cname, MethodInfo(VariantType::INT, "rename_dependencies", PropertyInfo(VariantType::STRING, "path"),
                                               PropertyInfo(VariantType::STRING, "renames")));
}

ResourceLoaderImport g_import_func;
