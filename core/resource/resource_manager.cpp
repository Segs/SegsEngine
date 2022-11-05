#include "resource_manager.h"

#include "core/io/resource_saver.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_format_loader.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "core/project_settings.h"
#include "core/object_tooling.h"
#include "core/os/file_access.h"
#include "core/script_language.h"
#include "core/class_db.h"
#include "core/os/mutex.h"
#include "core/os/rw_lock.h"
#include "core/io/resource_importer.h"
#include "core/plugin_interfaces/ResourceLoaderInterface.h"
#include "core/print_string.h"
#include "core/variant_parser.h"
#include "core/translation.h"
#include "core/pool_vector.h"
#include "core/dictionary.h"

#include "EASTL/deque.h"
/// Note: resource manager private data is using default 'new'/'delete'
namespace {
//used to track paths being loaded in a thread, avoids cyclic recursion
struct LoadingMapKey {
    String path;
    Thread::ID thread;
    bool operator==(const LoadingMapKey &p_key) const {
        return (thread == p_key.thread && path == p_key.path);
    }
    operator size_t() { // used by eastl::hash as a default
        return StringUtils::hash(path) + std::hash<Thread::ID>()(thread);
    }
};

struct ResourceManagerPriv {
    Mutex loading_map_mutex;
    HashMap<LoadingMapKey, int > loading_map;
    eastl::deque<Ref<ResourceFormatSaver>> s_savers;
    eastl::deque<Ref<ResourceFormatLoader>> s_loaders;
    ResourceSavedCallback save_callback = nullptr;

    Ref<ResourceFormatLoader> _find_custom_resource_format_loader(StringView path) {
        for (const auto & ldr : s_loaders) {
            if (ldr->get_script_instance() && ldr->get_script_instance()->get_script()->get_path() == path) {
                return ldr;
            }
        }
        return Ref<ResourceFormatLoader>();
    }
    bool _add_to_loading_map(StringView p_path) {

        bool success;
        MutexLock guard(loading_map_mutex);

        LoadingMapKey key;
        key.path = p_path;
        key.thread = Thread::get_caller_id();

        if (loading_map.contains(key)) {
            success = false;
        }
        else {
            loading_map[key] = true;
            success = true;
        }

        return success;
    }

    void _remove_from_loading_map(StringView p_path) {
        MutexLock guard(loading_map_mutex);

        LoadingMapKey key;
        key.path = p_path;
        key.thread = Thread::get_caller_id();

        loading_map.erase(key);

    }

    void _remove_from_loading_map_and_thread(StringView p_path, Thread::ID p_thread) {
        MutexLock guard(loading_map_mutex);

        LoadingMapKey key;
        key.path = p_path;
        key.thread = p_thread;

        loading_map.erase(key);

    }
    RES _load(StringView p_path, StringView p_original_path, StringView p_type_hint, bool p_no_cache, Error* r_error) {

        bool found = false;

        // Try all loaders and pick the first match for the type hint
        for (size_t i = 0; i < s_loaders.size(); i++) {

            if (!s_loaders[i]->recognize_path(p_path, p_type_hint)) {
                continue;
            }
            found = true;
            RES res(s_loaders[i]->load(p_path, !p_original_path.empty() ? p_original_path : p_path, r_error));
            if (not res) {
                continue;
            }

            return res;
        }
        ERR_FAIL_COND_V_MSG(found, RES(),
                    String("Failed loading resource: ") +p_path+". Make sure resources have been imported by opening the project in the editor at least once.");

        if(!Tooling::check_resource_manager_load(p_path)) {
            ERR_FAIL_V_MSG(RES(), "Resource file not found: " + p_path + ".");
        }
        ERR_FAIL_V_MSG(RES(), "No loader found for resource: " + p_path + ".");
    }

};
#define D() ((ResourceManagerPriv *)m_priv)

ResourceManager s_resource_manager;
ResourceRemapper s_resource_remapper;

HashSet<const Resource*> remapped_list;
HashMap<String, Vector<String> > translation_remaps;
HashMap<String, String> path_remaps;
ResourceLoadedCallback _loaded_callback;

/**
 * @brief The ResourceFormatLoaderWrap class is meant as a wrapper for a plugin-based resource format loaders.
 */
class ResourceFormatLoaderWrap final : public ResourceFormatLoader {

protected:
    ResourceLoaderInterface* m_wrapped;
public:
    RES load(StringView p_path, StringView p_original_path = StringView(), Error *r_error = nullptr, bool p_no_subresource_cache = false) override {
        return m_wrapped->load(p_path, p_original_path, r_error, p_no_subresource_cache);
    }
    void get_recognized_extensions(Vector<String>& p_extensions) const final {
        m_wrapped->get_recognized_extensions(p_extensions);
    }
    bool handles_type(StringView p_type) const final {
        return m_wrapped->handles_type(p_type);
    }
    String get_resource_type(StringView p_path) const final {
        return m_wrapped->get_resource_type(p_path);
    }

    explicit ResourceFormatLoaderWrap(ResourceLoaderInterface* w) : m_wrapped(w) {}
    ~ResourceFormatLoaderWrap() override = default;
    bool wrapped_same(const ResourceLoaderInterface* wrapped) const { return m_wrapped == wrapped; }
};

Ref<ResourceFormatLoader> createLoaderWrap(ResourceLoaderInterface* iface)
{
    //TODO: SEGS: verify that we don't create multiple wrappers for the same interface ?
    return make_ref_counted<ResourceFormatLoaderWrap>(iface);
}


String _path_remap(StringView p_path, bool* r_translation_remapped=nullptr) {
    using namespace StringUtils;
    String new_path(p_path);

    if (translation_remaps.contains(new_path)) {
        // translation_remaps has the following format:
        //   { "res://path.png": { "res://path-ru.png:ru", "res://path-de.png:de" } }

        // To find the path of the remapped resource, we extract the locale name after
        // the last ':' to match the project locale.
        // An extra remap may still be necessary afterwards due to the text -> binary converter on export.

        String locale = TranslationServer::get_singleton()->get_locale();
        ERR_FAIL_COND_V_MSG(locale.length() < 2, new_path, "Could not remap path '" + p_path + "' for translation as configured locale '" + locale + "' is invalid.");

        Vector<String>& res_remaps = translation_remaps[new_path];

		int best_score = 0;
        for (int i = 0; i < res_remaps.size(); i++) {
            int split = res_remaps[i].rfind(":");
            if (split == -1) {
                continue;
            }

            String l(strip_edges(right(res_remaps[i],split + 1)));
            int score = TranslationServer::get_singleton()->compare_locales(locale, l);
            if (score > 0 && score >= best_score) {
                new_path = res_remaps[i].left(split);
                best_score = score;
                if (score == 10) {
                    break; // Exact match, skip the rest.
            }
            }
        }

        if (r_translation_remapped) {
            *r_translation_remapped = true;
        }
    }

    if (path_remaps.contains(new_path)) {
        new_path = path_remaps[new_path];
    } else {
        // Try file remap.
        Error err;
        FileAccessRef f = FileAccess::open(new_path + ".remap", FileAccess::READ, &err);

        if (!f)
            return new_path;

        VariantParserStream* stream = VariantParser::get_file_stream(f);

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
            }
            else if (err != OK) {
                ERR_PRINT("Parse error: " + String(p_path) + ".remap:" + ::to_string(lines) + " error: " + error_text + ".");
                break;
            }

            if (assign == "path") {
                new_path = value.as<String>();
                break;
            }
            else if (next_tag.name != "remap") {
                break;
            }
        }
        VariantParser::release_stream(stream);
    }

    return new_path;
}

String normalized_resource_path(StringView path) {
    if (PathUtils::is_rel_path(path))
        return String("res://") + path;
    return ProjectSettings::get_singleton()->localize_path(path);

}
} // end of anonymous namespace


ResourceManager::ResourceManager()
{
    m_priv = new ResourceManagerPriv;
}

ResourceManager::~ResourceManager()
{
    if(D())
        finalize();
    queued_save_updates.clear();
}

void ResourceManager::add_resource_format_saver(const Ref<ResourceFormatSaver>& p_format_saver, bool p_at_front) {

    ERR_FAIL_COND_MSG(not p_format_saver, "It's not a reference to a valid ResourceFormatSaver object.");

    if (p_at_front) {
        D()->s_savers.push_front(p_format_saver);
    }
    else {
        D()->s_savers.push_back(p_format_saver);
    }
}

Error ResourceManager::save_impl(StringView p_path, const RES& p_resource, uint32_t p_flags) {

    const StringView extension = PathUtils::get_extension(p_path);
    Error err = ERR_FILE_UNRECOGNIZED;

    for (const Ref<ResourceFormatSaver>& s : D()->s_savers) {

        if (!s->recognize(p_resource))
            continue;

        Vector<String> extensions;
        bool recognized = false;
        s->get_recognized_extensions(p_resource, extensions);

        for (auto& ext : extensions) {

            if (StringUtils::compare(ext, extension, StringUtils::CaseInsensitive) == 0)
                recognized = true;
        }

        if (!recognized)
            continue;

        const String& old_path(p_resource->get_path());

        String local_path = ProjectSettings::get_singleton()->localize_path(p_path);

        if (p_flags & FLAG_CHANGE_PATH) {
            p_resource->set_path(local_path);
        }

        err = s->save(p_path, p_resource, p_flags);

        if (err != OK) {
            continue;
        }

        Object_set_edited(p_resource.get(), false);
        if (p_flags & FLAG_CHANGE_PATH) {
            p_resource->set_path(old_path);
        }

        if (D()->save_callback && StringUtils::begins_with(p_path, "res://")) {
            if (pause_save_callback) {
                queued_save_updates.emplace_back(QueuedCallbackCall{ p_resource, String(p_path) });
            } else {
                D()->save_callback(p_resource, p_path);
            }
        }

        return OK;
    }

    return err;
}

void ResourceManager::set_save_callback(ResourceSavedCallback p_callback) {

    D()->save_callback = p_callback;
}

void ResourceManager::set_save_callback_pause(bool v) {
    if (v==pause_save_callback) {
        return;
    }
    pause_save_callback = v;
    if (!D()->save_callback) {
        queued_save_updates.clear();
        return;
    }
    if (pause_save_callback==false) {
        for(const auto &path_and_resource : queued_save_updates) {
            if (StringUtils::begins_with(path_and_resource.path, "res://"))
                D()->save_callback(path_and_resource.res, path_and_resource.path);
        }
        queued_save_updates.clear();
    }
}

void ResourceManager::get_recognized_extensions(const RES& p_resource, Vector<String>& p_extensions) {

    for (const Ref<ResourceFormatSaver>& s : D()->s_savers) {

        s->get_recognized_extensions(p_resource, p_extensions);
    }
}

void ResourceManager::remove_resource_format_saver(const Ref<ResourceFormatSaver>& p_format_saver) {

    ERR_FAIL_COND_MSG(not p_format_saver, "It's not a reference to a valid ResourceFormatSaver object.");
    // Find saver
    auto iter = eastl::find(D()->s_savers.begin(), D()->s_savers.end(), p_format_saver);
    ERR_FAIL_COND(iter == D()->s_savers.end()); // Not found

    D()->s_savers.erase(iter);
}

Ref<ResourceFormatSaver> ResourceManager::_find_custom_resource_format_saver(StringView path) {
    for (const Ref<ResourceFormatSaver>& s : D()->s_savers) {
        if (s->get_script_instance() && s->get_script_instance()->get_script()->get_path() == path) {
            return s;
        }
    }
    return Ref<ResourceFormatSaver>();
}

void ResourceManager::remove_custom_savers() {

    Vector<Ref<ResourceFormatSaver> > custom_savers;
    for (const Ref<ResourceFormatSaver>& s : D()->s_savers) {
        if (s->get_script_instance()) {
            custom_savers.push_back(s);
        }
    }

    for (const Ref<ResourceFormatSaver>& saver : custom_savers) {
        remove_resource_format_saver(saver);
    }
}

bool ResourceManager::add_custom_resource_format_saver(StringView script_path) {

    if (_find_custom_resource_format_saver(script_path))
        return false;

    Ref<Resource> res = load(script_path);
    ERR_FAIL_COND_V(not res, false);
    ERR_FAIL_COND_V(!res->is_class("Script"), false);

    Ref<Script> s = dynamic_ref_cast<Script>(res);
    StringName ibt = s->get_instance_base_type();
    bool valid_type = ClassDB::is_parent_class(ibt, "ResourceFormatSaver");
    ERR_FAIL_COND_V_MSG(!valid_type, false, "Script does not inherit a CustomResourceSaver: " + String(script_path) + ".");

    Object* obj = ClassDB::instance(ibt);

    ERR_FAIL_COND_V_MSG(obj == nullptr, false, "Cannot instance script as custom resource saver, expected 'ResourceFormatSaver' inheritance, got: " + String(ibt) + ".");

    Ref<ResourceFormatSaver> crl(object_cast<ResourceFormatSaver>(obj),DoNotAddRef);
    crl->set_script(s.get_ref_ptr());
    add_resource_format_saver(crl);

    return true;
}

void ResourceManager::remove_custom_resource_format_saver(StringView script_path) {

    Ref<ResourceFormatSaver> custom_saver = _find_custom_resource_format_saver(script_path);
    if (custom_saver)
        remove_resource_format_saver(custom_saver);
}

void ResourceManager::add_custom_savers() {
    // Custom resource savers exploits global class names

    StringName custom_saver_base_class(ResourceFormatSaver::get_class_static_name());

    Vector<StringName> global_classes;
    ScriptServer::get_global_class_list(&global_classes);

    for (const StringName& class_name : global_classes) {

        StringName base_class = ScriptServer::get_global_class_native_base(class_name);

        if (base_class == custom_saver_base_class) {
            StringView path = ScriptServer::get_global_class_path(class_name);
            add_custom_resource_format_saver(path);
        }
    }
}


bool ResourceManager::add_custom_resource_format_loader(StringView script_path) {

    if (D()->_find_custom_resource_format_loader(script_path))
        return false;

    Ref<Resource> res = ResourceManager::load(script_path);
    ERR_FAIL_COND_V(not res, false);
    ERR_FAIL_COND_V(!res->is_class("Script"), false);

    Ref<Script> s(dynamic_ref_cast<Script>(res));
    StringName ibt = s->get_instance_base_type();
    bool valid_type = ClassDB::is_parent_class(ibt, "ResourceFormatLoader");
    ERR_FAIL_COND_V_MSG(!valid_type, false, "Script does not inherit a CustomResourceLoader: " + String(script_path) + ".");

    Object* obj = ClassDB::instance(ibt);

    ERR_FAIL_COND_V_MSG(obj == nullptr, false, "Cannot instance script as custom resource loader, expected 'ResourceFormatLoader' inheritance, got: " + String(ibt) + ".");

    Ref<ResourceFormatLoader> crl(object_cast<ResourceFormatLoader>(obj), DoNotAddRef);
    crl->set_script(s.get_ref_ptr());
    add_resource_format_loader(crl);

    return true;
}

void ResourceManager::remove_custom_resource_format_loader(StringView script_path) {

    Ref<ResourceFormatLoader> custom_loader = D()->_find_custom_resource_format_loader(script_path);
    if (custom_loader)
        remove_resource_format_loader(custom_loader);
}

void ResourceManager::add_custom_loaders() {
    // Custom loaders registration exploits global class names

    StringName custom_loader_base_class = ResourceFormatLoader::get_class_static_name();

    Vector<StringName> global_classes;
    ScriptServer::get_global_class_list(&global_classes);

    for (size_t i = 0, fin = global_classes.size(); i < fin; ++i) {

        StringName class_name = global_classes[i];
        StringName base_class = ScriptServer::get_global_class_native_base(class_name);

        if (base_class == custom_loader_base_class) {
            StringView path = ScriptServer::get_global_class_path(class_name);
            add_custom_resource_format_loader(path);
        }
    }
}

void ResourceManager::remove_custom_loaders() {

    Vector<Ref<ResourceFormatLoader> > custom_loaders;
    for (auto &ldr : D()->s_loaders) {
        if (ldr->get_script_instance()) {
            custom_loaders.push_back(ldr);
        }
    }

    for (const Ref<ResourceFormatLoader> & ldr : custom_loaders) {
        remove_resource_format_loader(ldr);
    }
}
void ResourceManager::get_recognized_extensions_for_type(StringView p_type, Vector<String>& p_extensions) {

    for (const auto & v : D()->s_loaders) {
        v->get_recognized_extensions_for_type(p_type, p_extensions);
    }
}
/**
 * @returns true if the resource was not in cache and an attempt was made to load it.
 */
bool ResourceManager::load_impl(RES &p_res, StringView p_path, StringView p_type_hint, bool p_no_cache, Error *r_error) {

    if (r_error)
        *r_error = ERR_CANT_OPEN;

    String local_path=normalized_resource_path(p_path);

    if (!p_no_cache) {

        {
            bool success = D()->_add_to_loading_map(local_path);
            ERR_FAIL_COND_V_MSG(!success, RES(), "Resource: '" + local_path + "' ");
        }

        //lock first if possible
        ResourceCache::lock.read_lock();

        //get ptr
        Resource* rptr = ResourceCache::get_unguarded(local_path);

        if (rptr) {
            p_res = RES(rptr);
            // it is possible this resource was just freed in a thread. If so, this referencing will not work and
            // resource is considered not cached
            if (p_res) {
                //referencing is fine
                if (r_error)
                    *r_error = OK;
                ResourceCache::lock.read_unlock();
                D()->_remove_from_loading_map(local_path);
                return false;
            }
        }
        ResourceCache::lock.read_unlock();
    }

    bool xl_remapped = false;
    const String path = _path_remap(local_path, &xl_remapped);

    if (path.empty()) {
        if (!p_no_cache) {
            D()->_remove_from_loading_map(local_path);
        }
        ERR_FAIL_V_MSG(false, "Remapping '" + local_path + "' failed.");
    }

    print_verbose("Loading resource: " + path);
    RES res(D()->_load(path, local_path, p_type_hint, p_no_cache, r_error));

    if (not res) {
        if (!p_no_cache) {
            D()->_remove_from_loading_map(local_path);
        }
        print_verbose("Failed loading resource: " + path);
        return false;
    }
    if (!p_no_cache)
        res->set_path(local_path);

    if (xl_remapped)
        res->set_as_translation_remapped(true);

    Object_set_edited(res.get(), false);


    if (!p_no_cache) {
        D()->_remove_from_loading_map(local_path);
    }

    if (_loaded_callback) {
        _loaded_callback(res, p_path);
    }
    p_res = res;
    return res;
}

RES ResourceManager::load_internal(StringView p_path, StringView p_original_path, StringView p_type_hint, bool p_no_cache, Error* r_error)
{
    return D()->_load(p_path, p_original_path, p_type_hint, p_no_cache, r_error);
}

bool ResourceManager::exists(StringView p_path, StringView p_type_hint) {

    const String local_path=normalized_resource_path(p_path);

    if (ResourceCache::has(local_path)) {

        return true; // If cached, it probably exists
    }

    bool xl_remapped = false;
    String path = _path_remap(local_path, &xl_remapped);

    // Try all loaders and pick the first match for the type hint
    for (const auto & v : D()->s_loaders) {

        if (!v->recognize_path(path, p_type_hint)) {
            continue;
        }

        if (v->exists(path))
            return true;
    }

    return false;
}

Ref<ResourceInteractiveLoader> ResourceManager::load_interactive(StringView p_path, StringView p_type_hint, bool p_no_cache, Error* r_error) {

    if (r_error)
        *r_error = ERR_CANT_OPEN;

    String local_path = normalized_resource_path(p_path);

    if (!p_no_cache) {

        bool success = D()->_add_to_loading_map(local_path);
        ERR_FAIL_COND_V_MSG(!success, Ref<ResourceInteractiveLoader>(), "Resource: '" + local_path + "' is already being loaded. Cyclic reference?");

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
            D()->_remove_from_loading_map(local_path);
        }
        ERR_FAIL_V_MSG(Ref<ResourceInteractiveLoader>(), "Remapping '" + local_path + "' failed.");
    }

    print_verbose("Loading resource: " + path);

    bool found = false;
    for (const auto & v : D()->s_loaders) {

        if (!v->recognize_path(path, p_type_hint))
            continue;
        found = true;
        Ref<ResourceInteractiveLoader> ril = v->load_interactive(path, local_path, r_error);
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
        D()->_remove_from_loading_map(local_path);
    }

    ERR_FAIL_COND_V_MSG(found, Ref<ResourceInteractiveLoader>(), "Failed loading resource: " + path + ".");

    ERR_FAIL_V_MSG(Ref<ResourceInteractiveLoader>(), "No loader found for resource: " + path + ".");
}

void ResourceManager::add_resource_format_loader(const Ref<ResourceFormatLoader>& p_format_loader, bool p_at_front) {

    ERR_FAIL_COND(not p_format_loader);

    if (p_at_front) {
        D()->s_loaders.push_front(p_format_loader);
    }
    else {
        D()->s_loaders.push_back(p_format_loader);
    }
}

void ResourceManager::add_resource_format_loader(ResourceLoaderInterface* p_format_loader, bool p_at_front)
{
    ERR_FAIL_COND(not p_format_loader);
#ifdef DEBUG_ENABLED
    for (auto & s_loader : D()->s_loaders)
    {
        Ref<ResourceFormatLoaderWrap> fmt = dynamic_ref_cast<ResourceFormatLoaderWrap>(s_loader);
        if (fmt) {
            ERR_FAIL_COND(fmt->wrapped_same(p_format_loader));
        }
    }
#endif
    add_resource_format_loader(createLoaderWrap(p_format_loader), p_at_front);
}
void ResourceManager::remove_resource_format_loader(const ResourceLoaderInterface* p_format_loader) {

    if (unlikely(not p_format_loader)) {
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Null p_format_loader in remove_resource_format_loader.",{});
        return;
    }
    ERR_FAIL_COND_MSG(D() == nullptr, "ResourceManager was already destructed");
    eastl::erase_if(D()->s_loaders,[p_format_loader](const Ref<ResourceFormatLoader> &v)->bool
    {
            Ref<ResourceFormatLoaderWrap> fmt = dynamic_ref_cast<ResourceFormatLoaderWrap>(v);
            return (fmt && fmt->wrapped_same(p_format_loader));

    });

}
void ResourceManager::remove_resource_format_loader(const Ref<ResourceFormatLoader>& p_format_loader) {

    if (unlikely(not p_format_loader)) {
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Null p_format_loader in remove_resource_format_loader.",{});
        return;
    }
    ERR_FAIL_COND_MSG(D()==nullptr,"ResourceManager was already destructed");
    eastl::erase_if(D()->s_loaders, [p_format_loader](const Ref<ResourceFormatLoader>& v)->bool {
            return (v == p_format_loader);
    });
}

int ResourceManager::get_import_order(StringView p_path) {

    const String path = _path_remap(p_path);

    const String local_path=normalized_resource_path(path);

    for (auto & s_loader : D()->s_loaders) {

        if (!s_loader->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        return s_loader->get_import_order(p_path);
    }

    return 0;
}

void ResourceManager::remove_from_loading_map_and_thread(StringView p_path, Thread::ID p_thread) {
    D()->_remove_from_loading_map_and_thread(p_path,p_thread);
}

String ResourceManager::get_import_group_file(StringView p_path) {
    String path = _path_remap(p_path);

    String local_path=normalized_resource_path(path);

    for (auto & s_loader : D()->s_loaders) {

        if (!s_loader->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        return s_loader->get_import_group_file(p_path);
    }

    return String(); //not found
}

bool ResourceManager::is_import_valid(StringView p_path) {

    String path = _path_remap(p_path);

    String local_path = normalized_resource_path(path);

    for (auto & s_loader : D()->s_loaders) {

        if (!s_loader->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        return s_loader->is_import_valid(p_path);
    }

    return false; //not found
}

bool ResourceManager::is_imported(StringView p_path) {

    String path = _path_remap(p_path);

    String local_path = normalized_resource_path(path);

    for (auto & s_loader : D()->s_loaders) {

        if (!s_loader->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        return s_loader->is_imported(p_path);
    }

    return false; //not found
}

void ResourceManager::get_dependencies(StringView p_path, Vector<String>& p_dependencies, bool p_add_types) {

    String path = _path_remap(p_path);

    String local_path = normalized_resource_path(path);

    for (auto & s_loader : D()->s_loaders) {

        if (!s_loader->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        s_loader->get_dependencies(local_path, p_dependencies, p_add_types);
    }
}

Error ResourceManager::rename_dependencies(StringView p_path, const HashMap<String, String>& p_map) {

    String path = _path_remap(p_path);

    String local_path = normalized_resource_path(path);

    for (auto & s_loader : D()->s_loaders) {

        if (!s_loader->recognize_path(local_path))
            continue;
        /*
        if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
            continue;
        */

        return s_loader->rename_dependencies(local_path, p_map);
    }

    return OK; // ??
}

String ResourceManager::get_resource_type(StringView p_path) {

    String local_path=normalized_resource_path(p_path);

    for (auto & s_loader : D()->s_loaders) {

        String result = s_loader->get_resource_type(local_path);
        if (not result.empty())
            return result;
    }

    return String();
}


String ResourceRemapper::import_remap(StringView p_path) {

    if (ResourceFormatImporter::get_singleton()->recognize_path(p_path)) {

        return ResourceFormatImporter::get_singleton()->get_internal_resource_path(p_path);
    }

    return String(p_path);
}

String ResourceRemapper::path_remap(StringView p_path) {
    return _path_remap(p_path);
}

void ResourceRemapper::remove_remap(const Resource *r) {
    remapped_list.erase(r);
}

void ResourceRemapper::reload_translation_remaps() {
    Vector<const Resource*> to_reload;
    {
        RWLockRead read_lock(ResourceCache::lock);
        to_reload.assign(remapped_list.begin(), remapped_list.end());
    }


    //now just make sure to not delete any of these resources while changing locale..
    for (const Resource* r : to_reload) {
        const_cast<Resource* >(r)->reload_from_file();
    }
}

void ResourceRemapper::load_translation_remaps() {

    if (!ProjectSettings::get_singleton()->has_setting("locale/translation_remaps"))
        return;

    Dictionary remaps = ProjectSettings::get_singleton()->getT<Dictionary>("locale/translation_remaps");
    auto keys(remaps.get_key_list());
    for (const auto & E : keys) {

        Array langs = remaps[E].as<Array>();
        Vector<String> lang_remaps;
        lang_remaps.reserve(langs.size());
        for (int i = 0; i < langs.size(); i++) {
            lang_remaps.emplace_back(langs[i].as<String>());
        }

        translation_remaps[E.asCString()] = lang_remaps;
    }
}

void ResourceRemapper::clear_translation_remaps() {
    translation_remaps.clear();
    remapped_list.clear();
}

void ResourceRemapper::load_path_remaps() {

    if (!ProjectSettings::get_singleton()->has_setting("path_remap/remapped_paths"))
        return;

    PoolVector<String> remaps = ProjectSettings::get_singleton()->getT<PoolVector<String>>("path_remap/remapped_paths");
    int rc = remaps.size();
    ERR_FAIL_COND(rc & 1); //must be even
    PoolVector<String>::Read r = remaps.read();

    for (int i = 0; i < rc; i += 2) {

        path_remaps[r[i]] = r[i + 1];
    }
}

void ResourceRemapper::clear_path_remaps() {

    path_remaps.clear();
}

void ResourceManager::set_load_callback(ResourceLoadedCallback p_callback) {
    _loaded_callback = p_callback;
}



void ResourceManager::initialize() {

}

void ResourceManager::finalize()
{
    for (const auto& e : D()->loading_map) {
        ERR_PRINT("Exited while resource is being loaded: " + e.first.path);
    }
    delete D();
    m_priv=nullptr;
}
ResourceManager& gResourceManager() {
    return s_resource_manager;
}


void ResourceRemapper::set_as_translation_remapped(const Resource* r, bool p_remapped) {

    if (remapped_list.contains(r) == p_remapped)
        return;

    RWLockWrite write_locker(ResourceCache::lock);

    if (p_remapped) {
        remapped_list.insert(r);
    }
    else {
        remapped_list.erase(r);
    }

}

bool ResourceRemapper::is_translation_remapped(const Resource *resource) {
    return remapped_list.contains(resource);
}
ResourceRemapper& gResourceRemapper() {
    return s_resource_remapper;
}

#undef D
