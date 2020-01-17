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

#include "core/pool_vector.h"
#include "core/os/mutex.h"
#include "core/io/resource_importer.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/os/rw_lock.h"
#include "core/object_tooling.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/translation.h"
#include "core/script_language.h"
#include "core/variant_parser.h"
#include "core/method_bind.h"
#include "core/property_info.h"
#include "core/plugin_interfaces/ResourceLoaderInterface.h"

#include <utility>

namespace {
/**
 * @brief The ResourceFormatLoaderWrap class is meant as a wrapper for a plugin-based resource format loaders.
 */
class ResourceFormatLoaderWrap final : public ResourceFormatLoader {

protected:
    ResourceLoaderInterface *m_wrapped;
public:
    RES load(se_string_view p_path, se_string_view p_original_path = se_string_view (), Error *r_error = nullptr) override {
        return m_wrapped->load(p_path,p_original_path,r_error);
    }
    void get_recognized_extensions(PODVector<String> &p_extensions) const final {
        m_wrapped->get_recognized_extensions(p_extensions);
    }
    bool handles_type(se_string_view p_type) const final{
        return m_wrapped->handles_type(p_type);
    }
    String get_resource_type(se_string_view p_path) const final {
        return m_wrapped->get_resource_type(p_path);
    }

    ResourceFormatLoaderWrap(ResourceLoaderInterface *w ) : m_wrapped(w) {}
    ~ResourceFormatLoaderWrap() override = default;
    bool wrapped_same(const ResourceLoaderInterface *wrapped) const { return m_wrapped == wrapped;}
};

Ref<ResourceFormatLoader> createLoaderWrap(ResourceLoaderInterface *iface)
{
    //TODO: SEGS: verify that we don't create multiple wrappers for the same interface ?
    return make_ref_counted<ResourceFormatLoaderWrap>(iface);
}
} // end of anonymous namespace

Ref<ResourceFormatLoader> ResourceLoader::loader[ResourceLoader::MAX_LOADERS];

int ResourceLoader::loader_count = 0;

IMPL_GDCLASS(ResourceInteractiveLoader);
IMPL_GDCLASS(ResourceFormatLoader);

Error ResourceInteractiveLoader::wait() {

    Error err = poll();
    while (err == OK) {
        err = poll();
    }

    return err;
}

ResourceInteractiveLoader::~ResourceInteractiveLoader() {
    if (!path_loading.empty()) {
        ResourceLoader::_remove_from_loading_map_and_thread(path_loading, path_loading_thread);
    }
}

bool ResourceFormatLoader::recognize_path(se_string_view p_path, se_string_view p_for_type) const {

    se_string_view extension = PathUtils::get_extension(p_path);

    PODVector<String> extensions;
    if (p_for_type.empty()) {
        get_recognized_extensions(extensions);
    } else {
        get_recognized_extensions_for_type(p_for_type, extensions);
    }

    for(const String &E : extensions ) {

        if (StringUtils::compare(E,extension,StringUtils::CaseInsensitive) == 0)
            return true;
    }

    return false;
}

bool ResourceFormatLoader::handles_type(se_string_view p_type) const {

    if (get_script_instance() && get_script_instance()->has_method("handles_type")) {
        // I guess custom loaders for custom resources should use "Resource"
        return get_script_instance()->call("handles_type", p_type).as<bool>();
    }

    return false;
}

String ResourceFormatLoader::get_resource_type(se_string_view p_path) const {

    if (get_script_instance() && get_script_instance()->has_method("get_resource_type")) {
        return get_script_instance()->call("get_resource_type", p_path).as<String>();
    }

    return String();
}

void ResourceFormatLoader::get_recognized_extensions_for_type(se_string_view p_type, PODVector<String> &p_extensions) const {

    if (p_type.empty() || handles_type(p_type))
        get_recognized_extensions(p_extensions);
}

void ResourceLoader::get_recognized_extensions_for_type(se_string_view p_type, PODVector<String> &p_extensions) {

    for (int i = 0; i < loader_count; i++) {
        loader[i]->get_recognized_extensions_for_type(p_type, p_extensions);
    }
}

void ResourceInteractiveLoader::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("get_resource"), &ResourceInteractiveLoader::get_resource);
    MethodBinder::bind_method(D_METHOD("poll"), &ResourceInteractiveLoader::poll);
    MethodBinder::bind_method(D_METHOD("wait"), &ResourceInteractiveLoader::wait);
    MethodBinder::bind_method(D_METHOD("get_stage"), &ResourceInteractiveLoader::get_stage);
    MethodBinder::bind_method(D_METHOD("get_stage_count"), &ResourceInteractiveLoader::get_stage_count);
}

class ResourceInteractiveLoaderDefault : public ResourceInteractiveLoader {

    GDCLASS(ResourceInteractiveLoaderDefault, ResourceInteractiveLoader)

public:
    Ref<Resource> resource;

    void set_local_path(se_string_view /*p_local_path*/) override {
        /*scene->set_filename(p_local_path);*/
    }
    Ref<Resource> get_resource() override { return resource; }
    Error poll() override { return ERR_FILE_EOF; }
    int get_stage() const override { return 1; }
    int get_stage_count() const override { return 1; }
    void set_translation_remapped(bool p_remapped) override { resource->set_as_translation_remapped(p_remapped); }

    ResourceInteractiveLoaderDefault() = default;
};
IMPL_GDCLASS(ResourceInteractiveLoaderDefault)

Ref<ResourceInteractiveLoader> ResourceFormatLoader::load_interactive(se_string_view p_path, se_string_view p_original_path, Error *r_error) {

    //either this
    Ref<Resource> res = load(p_path, p_original_path, r_error);
    if (not res)
        return Ref<ResourceInteractiveLoader>();

    Ref<ResourceInteractiveLoaderDefault> ril(make_ref_counted<ResourceInteractiveLoaderDefault>());
    ril->resource = res;
    return ril;
}

bool ResourceFormatLoader::exists(se_string_view p_path) const {
    return FileAccess::exists(p_path); //by default just check file
}

void ResourceFormatLoader::get_recognized_extensions(PODVector<String> &p_extensions) const {

    if (get_script_instance() && get_script_instance()->has_method("get_recognized_extensions")) {
        p_extensions = get_script_instance()->call("get_recognized_extensions").as<PODVector<String>>();
    }
}

RES ResourceFormatLoader::load(se_string_view p_path, se_string_view p_original_path, Error *r_error) {

    if (get_script_instance() && get_script_instance()->has_method("load")) {
        Variant res = get_script_instance()->call("load", p_path, p_original_path);

        if (res.get_type() == VariantType::INT) {

            if (r_error)
                *r_error = (Error)res.as<int64_t>();

        } else {

            if (r_error)
                *r_error = OK;
            return refFromVariant<Resource>(res);
        }
    }

    //or this must be implemented
    Ref<ResourceInteractiveLoader> ril = load_interactive(p_path, p_original_path, r_error);
    if (not ril)
        return RES();
    ril->set_local_path(p_original_path);

    while (true) {

        Error err = ril->poll();

        if (err == ERR_FILE_EOF) {
            if (r_error)
                *r_error = OK;
            return ril->get_resource();
        }

        if (r_error)
            *r_error = err;

        ERR_FAIL_COND_V_MSG(err != OK, RES(), "Failed to load resource '" + String(p_path) + "'.")
    }
}

void ResourceFormatLoader::get_dependencies(se_string_view p_path, PODVector<String> &p_dependencies, bool p_add_types) {

    if (get_script_instance() && get_script_instance()->has_method("get_dependencies")) {
        p_dependencies = get_script_instance()->call("get_dependencies", p_path, p_add_types).as<PODVector<String>>();
    }
}

Error ResourceFormatLoader::rename_dependencies(se_string_view p_path, const Map<String, String> &p_map) {

    if (get_script_instance() && get_script_instance()->has_method("rename_dependencies")) {

        Dictionary deps_dict;
        for (const eastl::pair<const String,String> &E : p_map) {
            deps_dict[E.first] = E.second;
        }

        int64_t res = get_script_instance()->call("rename_dependencies", deps_dict);
        return (Error)res;
    }

    return OK;
}

void ResourceFormatLoader::_bind_methods() {

    {
        MethodInfo info = MethodInfo(VariantType::NIL, "load", PropertyInfo(VariantType::STRING, "path"), PropertyInfo(VariantType::STRING, "original_path"));
        info.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
        ClassDB::add_virtual_method(get_class_static_name(), info);
    }

    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::POOL_STRING_ARRAY, "get_recognized_extensions"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::BOOL, "handles_type", PropertyInfo(VariantType::STRING, "typename")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::STRING, "get_resource_type", PropertyInfo(VariantType::STRING, "path")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("get_dependencies", PropertyInfo(VariantType::STRING, "path"), PropertyInfo(VariantType::STRING, "add_types")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::INT, "rename_dependencies", PropertyInfo(VariantType::STRING, "path"), PropertyInfo(VariantType::STRING, "renames")));
}

///////////////////////////////////

RES ResourceLoader::_load(se_string_view p_path, se_string_view p_original_path, se_string_view p_type_hint, bool p_no_cache, Error *r_error) {

    bool found = false;

    // Try all loaders and pick the first match for the type hint
    for (int i = 0; i < loader_count; i++) {

        if (!loader[i]->recognize_path(p_path, p_type_hint)) {
            continue;
        }
        found = true;
        RES res(loader[i]->load(p_path, !p_original_path.empty() ? p_original_path : p_path, r_error));
        if (not res) {
            continue;
        }

        return res;
    }

    ERR_FAIL_COND_V_MSG(found, RES(), "Failed loading resource: " + String(p_path) + ".")
#ifdef TOOLS_ENABLED
    FileAccessRef file_check = FileAccess::create(FileAccess::ACCESS_RESOURCES);
    ERR_FAIL_COND_V_MSG(!file_check->file_exists(p_path), RES(), "Resource file not found: " + p_path + ".")
#endif
    ERR_FAIL_V_MSG(RES(), "No loader found for resource: " + String(p_path) + ".")
}

bool ResourceLoader::_add_to_loading_map(se_string_view p_path) {

    bool success;
    if (loading_map_mutex) {
        loading_map_mutex->lock();
    }

    LoadingMapKey key;
    key.path = p_path;
    key.thread = Thread::get_caller_id();

    if (loading_map.contains(key)) {
        success = false;
    } else {
        loading_map[key] = true;
        success = true;
    }

    if (loading_map_mutex) {
        loading_map_mutex->unlock();
    }

    return success;
}

void ResourceLoader::_remove_from_loading_map(se_string_view p_path) {
    if (loading_map_mutex) {
        loading_map_mutex->lock();
    }

    LoadingMapKey key;
    key.path = p_path;
    key.thread = Thread::get_caller_id();

    loading_map.erase(key);

    if (loading_map_mutex) {
        loading_map_mutex->unlock();
    }
}

void ResourceLoader::_remove_from_loading_map_and_thread(se_string_view p_path, Thread::ID p_thread) {
    if (loading_map_mutex) {
        loading_map_mutex->lock();
    }

    LoadingMapKey key;
    key.path = p_path;
    key.thread = p_thread;

    loading_map.erase(key);

    if (loading_map_mutex) {
        loading_map_mutex->unlock();
    }
}

RES ResourceLoader::load(se_string_view p_path, se_string_view p_type_hint, bool p_no_cache, Error *r_error) {

    if (r_error)
        *r_error = ERR_CANT_OPEN;

    String local_path;
    if (PathUtils::is_rel_path(p_path))
        local_path = String("res://") + p_path;
    else
        local_path = ProjectSettings::get_singleton()->localize_path(p_path);

    if (!p_no_cache) {

        {
            bool success = _add_to_loading_map(local_path);
            ERR_FAIL_COND_V_MSG(!success, RES(), "Resource: '" + local_path + "' is already being loaded. Cyclic reference?")
        }

        //lock first if possible
        if (ResourceCache::lock) {
            ResourceCache::lock->read_lock();
        }

        //get ptr
        Resource *rptr = ResourceCache::get_unguarded(local_path);

        if (rptr) {
            RES res(rptr);
            //it is possible this resource was just freed in a thread. If so, this referencing will not work and resource is considered not cached
            if (res) {
                //referencing is fine
                if (r_error)
                    *r_error = OK;
                if (ResourceCache::lock) {
                    ResourceCache::lock->read_unlock();
                }
                _remove_from_loading_map(local_path);
                return res;
            }
        }
        if (ResourceCache::lock) {
            ResourceCache::lock->read_unlock();
        }
    }

    bool xl_remapped = false;
    String path = _path_remap(local_path, &xl_remapped);

    if (path.empty()) {
        if (!p_no_cache) {
            _remove_from_loading_map(local_path);
        }
        ERR_FAIL_V_MSG(RES(), "Remapping '" + local_path + "' failed.")
    }

    print_verbose("Loading resource: " + path);
    RES res(_load(path, local_path, p_type_hint, p_no_cache, r_error));

    if (not res) {
        if (!p_no_cache) {
            _remove_from_loading_map(local_path);
        }
        return RES();
    }
    if (!p_no_cache)
        res->set_path(local_path);

    if (xl_remapped)
        res->set_as_translation_remapped(true);

    Object_set_edited(res.get(),false);

#ifdef TOOLS_ENABLED
    if (timestamp_on_load) {
        uint64_t mt = FileAccess::get_modified_time(path);
        //printf("mt %s: %lli\n",remapped_path.utf8().get_data(),mt);
        res->set_last_modified_time(mt);
    }
#endif

    if (!p_no_cache) {
        _remove_from_loading_map(local_path);
    }

    if (_loaded_callback) {
        _loaded_callback(res, p_path);
    }

    return res;
}

bool ResourceLoader::exists(se_string_view p_path, se_string_view p_type_hint) {

    String local_path;
    if (PathUtils::is_rel_path(p_path))
        local_path = String("res://") + p_path;
    else
        local_path = ProjectSettings::get_singleton()->localize_path(p_path);

    if (ResourceCache::has(local_path)) {

        return true; // If cached, it probably exists
    }

    bool xl_remapped = false;
    String path = _path_remap(local_path, &xl_remapped);

    // Try all loaders and pick the first match for the type hint
    for (int i = 0; i < loader_count; i++) {

        if (!loader[i]->recognize_path(path, p_type_hint)) {
            continue;
        }

        if (loader[i]->exists(path))
            return true;
    }

    return false;
}

Ref<ResourceInteractiveLoader> ResourceLoader::load_interactive(se_string_view p_path, se_string_view p_type_hint, bool p_no_cache, Error *r_error) {

    if (r_error)
        *r_error = ERR_CANT_OPEN;

    String local_path;
    if (PathUtils::is_rel_path(p_path))
        local_path = String("res://") + p_path;
    else
        local_path = ProjectSettings::get_singleton()->localize_path(p_path);

    if (!p_no_cache) {

        bool success = _add_to_loading_map(local_path);
        ERR_FAIL_COND_V_MSG(!success, Ref<ResourceInteractiveLoader>(), "Resource: '" + local_path + "' is already being loaded. Cyclic reference?")

        if (ResourceCache::has(local_path)) {

            print_verbose("Loading resource: " + local_path + " (cached)");
            Ref<Resource> res_cached(ResourceCache::get(local_path));
            Ref<ResourceInteractiveLoaderDefault> ril(make_ref_counted<ResourceInteractiveLoaderDefault>());

            ril->resource = res_cached;
            ril->path_loading = local_path;
            ril->path_loading_thread = Thread::get_caller_id();
            return ril;
        }
    }

    bool xl_remapped = false;
    String path = _path_remap(local_path, &xl_remapped);
    if (path.empty()) {
        if (!p_no_cache) {
            _remove_from_loading_map(local_path);
        }
        ERR_FAIL_V_MSG(Ref<ResourceInteractiveLoader>(), "Remapping '" + local_path + "' failed.")
    }

    print_verbose("Loading resource: " + path);

    bool found = false;
    for (int i = 0; i < loader_count; i++) {

        if (!loader[i]->recognize_path(path, p_type_hint))
            continue;
        found = true;
        Ref<ResourceInteractiveLoader> ril = loader[i]->load_interactive(path, local_path, r_error);
        if (not ril)
            continue;
        if (!p_no_cache) {
            ril->set_local_path(local_path);
            ril->path_loading = local_path;
            ril->path_loading_thread = Thread::get_caller_id();
        }

        if (xl_remapped)
            ril->set_translation_remapped(true);

        return ril;
    }

    if (!p_no_cache) {
        _remove_from_loading_map(local_path);
    }

    ERR_FAIL_COND_V_MSG(found, Ref<ResourceInteractiveLoader>(), "Failed loading resource: " + path + ".")

    ERR_FAIL_V_MSG(Ref<ResourceInteractiveLoader>(), "No loader found for resource: " + path + ".")
}

void ResourceLoader::add_resource_format_loader(const Ref<ResourceFormatLoader>& p_format_loader, bool p_at_front) {

    ERR_FAIL_COND(not p_format_loader)
    ERR_FAIL_COND(loader_count >= MAX_LOADERS)

    if (p_at_front) {
        for (int i = loader_count; i > 0; i--) {
            loader[i] = loader[i - 1];
        }
        loader[0] = p_format_loader;
        loader_count++;
    } else {
        loader[loader_count++] = p_format_loader;
    }
}

void ResourceLoader::add_resource_format_loader(ResourceLoaderInterface *p_format_loader, bool p_at_front)
{
    ERR_FAIL_COND(not p_format_loader)
    ERR_FAIL_COND(loader_count >= MAX_LOADERS)
#ifdef DEBUG_ENABLED
    for(int i=0; i<loader_count; ++i)
    {
        Ref<ResourceFormatLoaderWrap> fmt = dynamic_ref_cast<ResourceFormatLoaderWrap>(loader[i]);
        if(fmt) {
            ERR_FAIL_COND(fmt->wrapped_same(p_format_loader))
        }
    }
#endif
    add_resource_format_loader(createLoaderWrap(p_format_loader),p_at_front);
}
void ResourceLoader::remove_resource_format_loader(const ResourceLoaderInterface *p_format_loader) {

    if (unlikely(not p_format_loader)) {
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Null p_format_loader in remove_resource_format_loader.");
        return;
    }
    // Find loader
    int i = 0;
    for (; i < loader_count; ++i) {
        Ref<ResourceFormatLoaderWrap> fmt = dynamic_ref_cast<ResourceFormatLoaderWrap>(loader[i]);
        if (fmt && fmt->wrapped_same(p_format_loader))
            break;
    }

    ERR_FAIL_COND(i >= loader_count) // Not found

    // Shift next loaders up
    for (; i < loader_count - 1; ++i) {
        loader[i] = loader[i + 1];
    }
    loader[loader_count - 1].unref();
    --loader_count;
}
void ResourceLoader::remove_resource_format_loader(const Ref<ResourceFormatLoader>& p_format_loader) {

    if (unlikely(not p_format_loader)) {
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Null p_format_loader in remove_resource_format_loader.");
        return;
    }
    // Find loader
    int i = 0;
    for (; i < loader_count; ++i) {
        if (loader[i] == p_format_loader)
            break;
    }

    ERR_FAIL_COND(i >= loader_count) // Not found

    // Shift next loaders up
    for (; i < loader_count - 1; ++i) {
        loader[i] = loader[i + 1];
    }
    loader[loader_count - 1].unref();
    --loader_count;
}

int ResourceLoader::get_import_order(se_string_view p_path) {

    String path = _path_remap(p_path);

    String local_path;
    if (PathUtils::is_rel_path(path))
        local_path = "res://" + path;
    else
        local_path = ProjectSettings::get_singleton()->localize_path(path);

    for (int i = 0; i < loader_count; i++) {

        if (!loader[i]->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        return loader[i]->get_import_order(p_path);
    }

    return 0;
}

String ResourceLoader::get_import_group_file(se_string_view p_path) {
    String path = _path_remap(p_path);

    String local_path;
    if (PathUtils::is_rel_path(path))
        local_path = "res://" + path;
    else
        local_path = ProjectSettings::get_singleton()->localize_path(path);

    for (int i = 0; i < loader_count; i++) {

        if (!loader[i]->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        return loader[i]->get_import_group_file(p_path);
    }

    return String(); //not found
}

bool ResourceLoader::is_import_valid(se_string_view p_path) {

    String path = _path_remap(p_path);

    String local_path;
    if (PathUtils::is_rel_path(path))
        local_path = "res://" + path;
    else
        local_path = ProjectSettings::get_singleton()->localize_path(path);

    for (int i = 0; i < loader_count; i++) {

        if (!loader[i]->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        return loader[i]->is_import_valid(p_path);
    }

    return false; //not found
}

bool ResourceLoader::is_imported(se_string_view p_path) {

    String path = _path_remap(p_path);

    String local_path;
    if (PathUtils::is_rel_path(path))
        local_path = "res://" + path;
    else
        local_path = ProjectSettings::get_singleton()->localize_path(path);

    for (int i = 0; i < loader_count; i++) {

        if (!loader[i]->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        return loader[i]->is_imported(p_path);
    }

    return false; //not found
}

void ResourceLoader::get_dependencies(se_string_view p_path, PODVector<String> &p_dependencies, bool p_add_types) {

    String path = _path_remap(p_path);

    String local_path;
    if (PathUtils::is_rel_path(path))
        local_path = "res://" + path;
    else
        local_path = ProjectSettings::get_singleton()->localize_path(path);

    for (int i = 0; i < loader_count; i++) {

        if (!loader[i]->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        loader[i]->get_dependencies(local_path, p_dependencies, p_add_types);
    }
}

Error ResourceLoader::rename_dependencies(se_string_view p_path, const Map<String, String> &p_map) {

    String path = _path_remap(p_path);

    String local_path;
    if (PathUtils::is_rel_path(path))
        local_path = "res://" + path;
    else
        local_path = ProjectSettings::get_singleton()->localize_path(path);

    for (int i = 0; i < loader_count; i++) {

        if (!loader[i]->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        return loader[i]->rename_dependencies(local_path, p_map);
    }

    return OK; // ??
}

String ResourceLoader::get_resource_type(se_string_view p_path) {

    String local_path;
    if (PathUtils::is_rel_path(p_path))
        local_path = String("res://") + p_path;
    else
        local_path = ProjectSettings::get_singleton()->localize_path(p_path);

    for (int i = 0; i < loader_count; i++) {

        String result = loader[i]->get_resource_type(local_path);
        if (not result.empty())
            return result;
    }

    return String();
}

String ResourceLoader::_path_remap(se_string_view p_path, bool *r_translation_remapped) {
    using namespace StringUtils;
    String new_path(p_path);

    if (translation_remaps.contains(new_path)) {
        // translation_remaps has the following format:
        //   { "res://path.png": PoolStringArray( "res://path-ru.png:ru", "res://path-de.png:de" ) }

        // To find the path of the remapped resource, we extract the locale name after
        // the last ':' to match the project locale.
        // We also fall back in case of regional locales as done in TranslationServer::translate
        // (e.g. 'ru_RU' -> 'ru' if the former has no specific mapping).

        String locale = TranslationServer::get_singleton()->get_locale();
        ERR_FAIL_COND_V_MSG(locale.length() < 2, new_path, "Could not remap path '" + p_path + "' for translation as configured locale '" + locale + "' is invalid.")
        String lang(TranslationServer::get_language_code(locale));

        PODVector<String> &res_remaps = translation_remaps[new_path];
        bool near_match = false;

        for (size_t i = 0; i < res_remaps.size(); i++) {
            auto split = res_remaps[i].rfind(':');
            if (split == String::npos) {
                continue;
            }

            se_string_view l = strip_edges(right(res_remaps[i],split + 1));
            if (locale == l) { // Exact match.
                new_path = res_remaps[i].left(split);
                break;
            } else if (near_match) {
                continue; // Already found near match, keep going for potential exact match.
            }

            // No exact match (e.g. locale 'ru_RU' but remap is 'ru'), let's look further
            // for a near match (same language code, i.e. first 2 or 3 letters before
            // regional code, if included).
            if (lang == TranslationServer::get_language_code(l)) {
                // Language code matches, that's a near match. Keep looking for exact match.
                near_match = true;
                new_path = res_remaps[i].left(split);
                continue;
            }
        }

        if (r_translation_remapped) {
            *r_translation_remapped = true;
        }
    }

    if (path_remaps.contains(new_path)) {
        new_path = path_remaps[new_path];
    }

    if (new_path == p_path) { // Did not remap.
        // Try file remap.
        Error err;
        FileAccess *f = FileAccess::open(String(p_path) + ".remap", FileAccess::READ, &err);

        if (f) {

            VariantParser::Stream *stream=VariantParser::get_file_stream(f);

            String assign;
            Variant value;
            VariantParser::Tag next_tag;

            int lines = 0;
            String error_text;
            while (true) {

                assign = Variant().as<String>();
                next_tag.fields.clear();
                next_tag.name.clear();

                err = VariantParser::parse_tag_assign_eof(stream, lines, error_text, next_tag, assign, value, nullptr, true);
                if (err == ERR_FILE_EOF) {
                    break;
                } else if (err != OK) {
                    ERR_PRINT("Parse error: " + String(p_path) + ".remap:" + ::to_string(lines) + " error: " + error_text + ".")
                    break;
                }

                if (assign == "path") {
                    new_path = value.as<String>();
                    break;
                } else if (next_tag.name != "remap") {
                    break;
                }
            }
            VariantParser::release_stream(stream);
            memdelete(f);
        }
    }

    return new_path;
}

String ResourceLoader::import_remap(se_string_view p_path) {

    if (ResourceFormatImporter::get_singleton()->recognize_path(p_path)) {

        return ResourceFormatImporter::get_singleton()->get_internal_resource_path(p_path);
    }

    return String(p_path);
}

String ResourceLoader::path_remap(se_string_view p_path) {
    return _path_remap(p_path);
}

void ResourceLoader::reload_translation_remaps() {

    if (ResourceCache::lock) {
        ResourceCache::lock->read_lock();
    }

    List<Resource *> to_reload;
    SelfList<Resource> *E = remapped_list.first();

    while (E) {
        to_reload.push_back(E->self());
        E = E->next();
    }

    if (ResourceCache::lock) {
        ResourceCache::lock->read_unlock();
    }

    //now just make sure to not delete any of these resources while changing locale..
    while (to_reload.front()) {
        to_reload.front()->deref()->reload_from_file();
        to_reload.pop_front();
    }
}

void ResourceLoader::load_translation_remaps() {

    if (!ProjectSettings::get_singleton()->has_setting("locale/translation_remaps"))
        return;

    Dictionary remaps = ProjectSettings::get_singleton()->get("locale/translation_remaps");
    PODVector<Variant> keys(remaps.get_key_list());
    for(const Variant &E : keys ) {

        Array langs = remaps[E];
        PODVector<String> lang_remaps;
        lang_remaps.reserve(langs.size());
        for (int i = 0; i < langs.size(); i++) {
            lang_remaps.emplace_back(langs[i].as<String>());
        }

        translation_remaps[E.as<String>()] = lang_remaps;
    }
}

void ResourceLoader::clear_translation_remaps() {
    translation_remaps.clear();
}

void ResourceLoader::load_path_remaps() {

    if (!ProjectSettings::get_singleton()->has_setting("path_remap/remapped_paths"))
        return;

    PoolVector<String> remaps = ProjectSettings::get_singleton()->get("path_remap/remapped_paths");
    int rc = remaps.size();
    ERR_FAIL_COND(rc & 1) //must be even
    PoolVector<String>::Read r = remaps.read();

    for (int i = 0; i < rc; i += 2) {

        path_remaps[r[i]] = r[i+1];
    }
}

void ResourceLoader::clear_path_remaps() {

    path_remaps.clear();
}

void ResourceLoader::set_load_callback(ResourceLoadedCallback p_callback) {
    _loaded_callback = p_callback;
}

ResourceLoadedCallback ResourceLoader::_loaded_callback = nullptr;

Ref<ResourceFormatLoader> ResourceLoader::_find_custom_resource_format_loader(se_string_view path) {
    for (int i = 0; i < loader_count; ++i) {
        if (loader[i]->get_script_instance() && loader[i]->get_script_instance()->get_script()->get_path() == path) {
            return loader[i];
        }
    }
    return Ref<ResourceFormatLoader>();
}

bool ResourceLoader::add_custom_resource_format_loader(se_string_view script_path) {

    if (_find_custom_resource_format_loader(script_path))
        return false;

    Ref<Resource> res = ResourceLoader::load(script_path);
    ERR_FAIL_COND_V(not res, false)
    ERR_FAIL_COND_V(!res->is_class("Script"), false)

    Ref<Script> s(dynamic_ref_cast<Script>(res));
    StringName ibt = s->get_instance_base_type();
    bool valid_type = ClassDB::is_parent_class(ibt, "ResourceFormatLoader");
    ERR_FAIL_COND_V_MSG(!valid_type, false, "Script does not inherit a CustomResourceLoader: " + String(script_path) + ".")

    Object *obj = ClassDB::instance(ibt);

    ERR_FAIL_COND_V_MSG(obj == nullptr, false, "Cannot instance script as custom resource loader, expected 'ResourceFormatLoader' inheritance, got: " + String(ibt) + ".")

    auto *crl = object_cast<ResourceFormatLoader>(obj);
    crl->set_script(s.get_ref_ptr());
    ResourceLoader::add_resource_format_loader(Ref<ResourceFormatLoader>(crl));

    return true;
}

void ResourceLoader::remove_custom_resource_format_loader(se_string_view script_path) {

    Ref<ResourceFormatLoader> custom_loader = _find_custom_resource_format_loader(script_path);
    if (custom_loader)
        remove_resource_format_loader(custom_loader);
}

void ResourceLoader::add_custom_loaders() {
    // Custom loaders registration exploits global class names

    StringName custom_loader_base_class = ResourceFormatLoader::get_class_static_name();

    PODVector<StringName> global_classes;
    ScriptServer::get_global_class_list(&global_classes);

    for (size_t i=0, fin = global_classes.size(); i<fin; ++i) {

        StringName class_name = global_classes[i];
        StringName base_class = ScriptServer::get_global_class_native_base(class_name);

        if (base_class == custom_loader_base_class) {
            se_string_view path = ScriptServer::get_global_class_path(class_name);
            add_custom_resource_format_loader(path);
        }
    }
}

void ResourceLoader::remove_custom_loaders() {

    PODVector<Ref<ResourceFormatLoader> > custom_loaders;
    for (int i = 0; i < loader_count; ++i) {
        if (loader[i]->get_script_instance()) {
            custom_loaders.push_back(loader[i]);
        }
    }

    for (int i = 0; i < custom_loaders.size(); ++i) {
        remove_resource_format_loader(custom_loaders[i]);
    }
}

Mutex *ResourceLoader::loading_map_mutex = nullptr;
HashMap<LoadingMapKey, int,Hasher<LoadingMapKey> > ResourceLoader::loading_map;

void ResourceLoader::initialize() {

#ifndef NO_THREADS
    loading_map_mutex = memnew(Mutex);
#endif
}

void ResourceLoader::finalize() {
#ifndef NO_THREADS
    const LoadingMapKey *K = nullptr;
    while ((K = loading_map.next(K))) {
        ERR_PRINT("Exited while resource is being loaded: " + K->path)
    }
    loading_map.clear();
    memdelete(loading_map_mutex);
    loading_map_mutex = nullptr;
    for(auto &ldr : loader)
        ldr.reset();
#endif
}

ResourceLoadErrorNotify ResourceLoader::err_notify = nullptr;
void *ResourceLoader::err_notify_ud = nullptr;

DependencyErrorNotify ResourceLoader::dep_err_notify = nullptr;
void *ResourceLoader::dep_err_notify_ud = nullptr;

bool ResourceLoader::abort_on_missing_resource = true;
bool ResourceLoader::timestamp_on_load = false;

SelfList<Resource>::List ResourceLoader::remapped_list;
DefHashMap<String, PODVector<String> > ResourceLoader::translation_remaps;
DefHashMap<String, String> ResourceLoader::path_remaps;

ResourceLoaderImport ResourceLoader::import = nullptr;



