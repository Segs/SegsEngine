#pragma once

#include "core/io/resource_format_loader.h"
#include "core/io/resource_loader.h"
#include "core/resource.h"
#include "core/io/resource_saver.h"
#include "core/plugin_interfaces/ResourceLoaderInterface.h"

using ResourceSavedCallback = void (*)(const Ref<Resource>&, StringView);
class GODOT_EXPORT ResourceRemapper {
public:
    void set_as_translation_remapped(const Resource* r,bool p_remapped);
    bool is_translation_remapped(const Resource* resource);
    String import_remap(StringView p_path);
    String path_remap(StringView p_path);
    void remove_remap(const Resource *);
    void reload_translation_remaps();
    void load_translation_remaps();
    void clear_translation_remaps();
    void load_path_remaps();
    void clear_path_remaps();

};
extern GODOT_EXPORT ResourceRemapper& gResourceRemapper();

class GODOT_EXPORT ResourceManager {
    //TODO: SEGS: timestamp_on_save variable is only used when TOOLS_ENABLED is set.
    bool timestamp_on_save;
    bool timestamp_on_load;

    void* err_notify_ud = nullptr;
    ResourceLoadErrorNotify err_notify = nullptr;
    void* dep_err_notify_ud = nullptr;
    DependencyErrorNotify dep_err_notify = nullptr;
    bool abort_on_missing_resource = false;

public:
    ResourceManager();
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    enum SaverFlags {

        FLAG_RELATIVE_PATHS = 1,
        FLAG_BUNDLE_RESOURCES = 2,
        FLAG_CHANGE_PATH = 4,
        FLAG_OMIT_EDITOR_PROPERTIES = 8,
        FLAG_SAVE_BIG_ENDIAN = 16,
        FLAG_COMPRESS = 32,
        FLAG_REPLACE_SUBRESOURCE_PATHS = 64,
    };

    void set_timestamp_on_save(bool p_timestamp) { timestamp_on_save = p_timestamp; }
    bool get_timestamp_on_save() const { return timestamp_on_save; }

    void add_resource_format_saver(const Ref<ResourceFormatSaver> &p_format_saver, bool p_at_front=false);
    void remove_resource_format_saver(const Ref<ResourceFormatSaver> &p_format_saver);
    Ref<ResourceFormatSaver> _find_custom_resource_format_saver(StringView path);
    void remove_custom_savers();
    bool add_custom_resource_format_saver(StringView script_path);
    void remove_custom_resource_format_saver(StringView script_path);
    void add_custom_savers();
    Error save(StringView p_path, const RES &p_resource, uint32_t p_flags = 0);
    void set_save_callback(ResourceSavedCallback p_callback);
    void get_recognized_extensions(const RES &p_resource, Vector<String> &p_extensions);

    void set_timestamp_on_load(bool p_timestamp) { timestamp_on_load = p_timestamp; }
    bool get_timestamp_on_load() const { return timestamp_on_load; }
    bool add_custom_resource_format_loader(StringView script_path);
    void remove_custom_resource_format_loader(StringView script_path);
    void add_custom_loaders();
    void remove_custom_loaders();
    void get_recognized_extensions_for_type(StringView p_type, Vector<String> &p_extensions);

    void set_load_callback(ResourceLoadedCallback p_callback);

    Ref<ResourceInteractiveLoader> load_interactive(StringView p_path, StringView p_type_hint = StringView(), bool p_no_cache = false, Error* r_error = nullptr);
    RES load(StringView p_path, StringView p_type_hint = StringView(), bool p_no_cache = false, Error* r_error = nullptr);
    // TODO: Only used in ResourceFormatImporter::load, try to remove this from the public interface.
    RES load_internal(StringView p_path, StringView p_original_path, StringView p_type_hint = StringView(), bool p_no_cache = false, Error* r_error = nullptr);
    bool exists(StringView p_path, StringView p_type_hint = StringView());

    void add_resource_format_loader(const Ref<ResourceFormatLoader>& p_format_loader, bool p_at_front = false);
    void add_resource_format_loader(ResourceLoaderInterface*, bool p_at_front = false);
    void remove_resource_format_loader(const ResourceLoaderInterface* p_format_loader);
    void remove_resource_format_loader(const Ref<ResourceFormatLoader>& p_format_loader);
    String get_resource_type(StringView p_path);
    void get_dependencies(StringView p_path, Vector<String>& p_dependencies, bool p_add_types = false);
    Error rename_dependencies(StringView p_path, const HashMap<String, String>& p_map);
    bool is_import_valid(StringView p_path);
    String get_import_group_file(StringView p_path);
    bool is_imported(StringView p_path);
    int get_import_order(StringView p_path);

    void notify_load_error(StringView p_err) {
        if (err_notify)
            err_notify(err_notify_ud, p_err);
    }
    void set_error_notify_func(void* p_ud, ResourceLoadErrorNotify p_err_notify) {
        err_notify = p_err_notify;
        err_notify_ud = p_ud;
    }

    void notify_dependency_error(StringView p_path, StringView p_dependency, StringView p_type) {
        if (dep_err_notify)
            dep_err_notify(dep_err_notify_ud, p_path, p_dependency, p_type);
    }
    void set_dependency_error_notify_func(void* p_ud, DependencyErrorNotify p_err_notify) {
        dep_err_notify = p_err_notify;
        dep_err_notify_ud = p_ud;
    }

    void set_abort_on_missing_resources(bool p_abort) { abort_on_missing_resource = p_abort; }
    bool get_abort_on_missing_resources() const { return abort_on_missing_resource; }


    // TODO: Only used in ResourceInteractiveLoader::~ResourceInteractiveLoader, try to remove this from the public interface.
    void remove_from_loading_map_and_thread(StringView p_path, Thread::ID p_thread);

    void initialize();
    void finalize();
};
extern GODOT_EXPORT ResourceManager &  gResourceManager();
