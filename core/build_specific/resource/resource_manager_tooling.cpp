#include "core/resource/resource_manager_tooling.h"

#include "core/resource/resource_manager.h"

#ifdef TOOLS_ENABLED
#include "core/os/file_access.h"
#include "core/project_settings.h"
#include "core/resource/resource_tools.h"
#include "core/string.h"
#include "core/string_utils.h"

namespace ResourceManagerTooling {
struct Settings {
    bool timestamp_on_save = false;
    bool timestamp_on_load = false;
};
Settings g_settings;
} // namespace ResourceManagerTooling

namespace {
String normalized_resource_path(StringView path) {
    if (PathUtils::is_rel_path(path))
        return String("res://") + path;
    return ProjectSettings::get_singleton()->localize_path(path);
}

} // namespace

namespace ResourceManagerTooling {

void set_timestamp_on_save(bool p_timestamp) {
    g_settings.timestamp_on_save = p_timestamp;
}

bool get_timestamp_on_save() {
    return g_settings.timestamp_on_save;
}

void set_timestamp_on_load(bool p_timestamp) {
    g_settings.timestamp_on_load = p_timestamp;
}

bool get_timestamp_on_load() {
    return g_settings.timestamp_on_load;
}
} // namespace ResourceManagerTooling

Error ResourceManager::save(StringView p_path, const RES &p_resource, uint32_t p_flags) {
    auto res = save_impl(p_path, p_resource, p_flags);
    if (res == OK) {
        if (ResourceManagerTooling::g_settings.timestamp_on_save) {
            const uint64_t mt = FileAccess::get_modified_time(p_path);
            ResourceTooling::set_last_modified_time(p_resource.get(), mt);
        }
    }
    return res;
}

RES ResourceManager::load(StringView p_path, StringView p_type_hint, bool p_no_cache, Error *r_error) {
    RES res;
    bool non_cached = load_impl(res, p_path, p_type_hint, p_no_cache, r_error);

    if (!non_cached || !res || !ResourceManagerTooling::g_settings.timestamp_on_load) {
        return res;
    }
    String local_path = normalized_resource_path(p_path);
    String path = gResourceRemapper().path_remap(local_path);
    uint64_t mt = FileAccess::get_modified_time(path);
    // printf("mt %s: %lli\n",remapped_path.utf8().get_data(),mt);
    ResourceTooling::set_last_modified_time(res.get(), mt);
    return res;
}
#else
RES ResourceManager::load(StringView p_path, StringView p_type_hint, bool p_no_cache, Error *r_error) {
    RES res;
    bool non_cached = load_impl(res, p_path, p_type_hint, p_no_cache, r_error);
    return res;
}
Error ResourceManager::save(StringView p_path, const RES &p_resource, uint32_t p_flags) {
    return save_impl(p_path, p_resource, p_flags);
}
// TODO: report calls to tooling functions on non-tool platform ??
void set_timestamp_on_save(bool p_timestamp) {}
bool get_timestamp_on_save() {
    return false;
}
void set_timestamp_on_load(bool p_timestamp) {}
bool get_timestamp_on_load() {
    return false;
}

#endif
