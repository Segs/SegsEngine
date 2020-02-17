/*************************************************************************/
/*  resource_loader.h                                                    */
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

#include "core/os/thread.h"
#include "core/resource.h"
#include "core/hashfuncs.h"
#include "core/hash_map.h"
#include "core/self_list.h"
#include "core/se_string.h"
//#include "core/string_utils.inl"

namespace std {
class recursive_mutex;
}
using Mutex = std::recursive_mutex;
class ResourceFormatLoader;
class ResourceInteractiveLoader;
class ResourceLoaderInterface;

//used to track paths being loaded in a thread, avoids cyclic recursion
struct LoadingMapKey {
    String path;
    Thread::ID thread;
    bool operator==(const LoadingMapKey &p_key) const {
        return (thread == p_key.thread && path == p_key.path);
    }
};
template<>
struct Hasher<LoadingMapKey> {
    uint32_t operator()(const LoadingMapKey &p_key) const { return StringUtils::hash(p_key.path) + Hasher<Thread::ID>()(p_key.thread); }
};



using ResourceLoadErrorNotify = void (*)(void *, se_string_view);
using DependencyErrorNotify = void (*)(void *, se_string_view, se_string_view, se_string_view);
using ResourceLoaderImport = Error (*)(se_string_view);
using ResourceLoadedCallback = void (*)(RES, se_string_view );

class GODOT_EXPORT ResourceLoader {

    enum {
        MAX_LOADERS = 64
    };

    static Ref<ResourceFormatLoader> loader[MAX_LOADERS];
    static int loader_count;
    static bool timestamp_on_load;

    static void *err_notify_ud;
    static ResourceLoadErrorNotify err_notify;
    static void *dep_err_notify_ud;
    static DependencyErrorNotify dep_err_notify;
    static bool abort_on_missing_resource;
    static HashMap<String, Vector<String> > translation_remaps;
    static HashMap<String, String> path_remaps;

    static String _path_remap(se_string_view p_path, bool *r_translation_remapped = nullptr);
    friend class Resource;

    static SelfList<Resource>::List remapped_list;

    friend class ResourceFormatImporter;
    friend class ResourceInteractiveLoader;
    //internal load function
    static RES _load(se_string_view p_path, se_string_view p_original_path, se_string_view p_type_hint, bool p_no_cache, Error *r_error);

    static ResourceLoadedCallback _loaded_callback;

    static Ref<ResourceFormatLoader> _find_custom_resource_format_loader(se_string_view path);
    static Mutex *loading_map_mutex;

    static HashMap<LoadingMapKey, int, Hasher<LoadingMapKey>> loading_map;

    static bool _add_to_loading_map(se_string_view p_path);
    static void _remove_from_loading_map(se_string_view p_path);
    static void _remove_from_loading_map_and_thread(se_string_view p_path, Thread::ID p_thread);

public:
    static Ref<ResourceInteractiveLoader> load_interactive(se_string_view p_path, se_string_view p_type_hint = se_string_view(), bool p_no_cache = false, Error *r_error = nullptr);
    static RES load(se_string_view p_path, se_string_view p_type_hint = se_string_view(), bool p_no_cache = false, Error *r_error = nullptr);
    static bool exists(se_string_view p_path, se_string_view p_type_hint = se_string_view());

    static void get_recognized_extensions_for_type(se_string_view p_type, Vector<String> &p_extensions);
    static void add_resource_format_loader(const Ref<ResourceFormatLoader>& p_format_loader, bool p_at_front = false);
    static void add_resource_format_loader(ResourceLoaderInterface *, bool p_at_front = false);
    static void remove_resource_format_loader(const ResourceLoaderInterface *p_format_loader);
    static void remove_resource_format_loader(const Ref<ResourceFormatLoader>& p_format_loader);
    static String get_resource_type(se_string_view p_path);
    static void get_dependencies(se_string_view p_path, Vector<String> &p_dependencies, bool p_add_types = false);
    static Error rename_dependencies(se_string_view p_path, const HashMap<String, String> &p_map);
    static bool is_import_valid(se_string_view p_path);
    static String get_import_group_file(se_string_view p_path);
    static bool is_imported(se_string_view p_path);
    static int get_import_order(se_string_view p_path);

    static void set_timestamp_on_load(bool p_timestamp) { timestamp_on_load = p_timestamp; }
    static bool get_timestamp_on_load() { return timestamp_on_load; }

    static void notify_load_error(se_string_view p_err) {
        if (err_notify)
            err_notify(err_notify_ud, p_err);
    }
    static void set_error_notify_func(void *p_ud, ResourceLoadErrorNotify p_err_notify) {
        err_notify = p_err_notify;
        err_notify_ud = p_ud;
    }

    static void notify_dependency_error(se_string_view p_path, se_string_view p_dependency, se_string_view p_type) {
        if (dep_err_notify) dep_err_notify(dep_err_notify_ud, p_path, p_dependency, p_type);
    }
    static void set_dependency_error_notify_func(void *p_ud, DependencyErrorNotify p_err_notify) {
        dep_err_notify = p_err_notify;
        dep_err_notify_ud = p_ud;
    }

    static void set_abort_on_missing_resources(bool p_abort) { abort_on_missing_resource = p_abort; }
    static bool get_abort_on_missing_resources() { return abort_on_missing_resource; }

    static String path_remap(se_string_view p_path);
    static String import_remap(se_string_view p_path);

    static void load_path_remaps();
    static void clear_path_remaps();

    static void reload_translation_remaps();
    static void load_translation_remaps();
    static void clear_translation_remaps();

    static void set_load_callback(ResourceLoadedCallback p_callback);
    static ResourceLoaderImport import;

    static bool add_custom_resource_format_loader(se_string_view script_path);
    static void remove_custom_resource_format_loader(se_string_view script_path);
    static void add_custom_loaders();
    static void remove_custom_loaders();

    static void initialize();
    static void finalize();
};
