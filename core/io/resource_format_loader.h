#pragma once

#include "core/reference.h"
#include "core/os/thread.h"
#include "core/ustring.h"

namespace eastl {
template <typename Key, typename T, typename Compare, typename Allocator>
class map;
}
template <class T>
struct Comparator;
class wrap_allocator;

template <class K,class V>
using DefMap = eastl::map<K,V,Comparator<K>,wrap_allocator>;

class Resource;
using RES = Ref<Resource>;

class ResourceInteractiveLoader : public RefCounted {

    GDCLASS(ResourceInteractiveLoader, RefCounted)
    friend class ResourceLoader;
    String path_loading;
    Thread::ID path_loading_thread;

protected:
    static void _bind_methods();

public:
    virtual void set_local_path(const String &p_local_path) = 0;
    virtual Ref<Resource> get_resource() = 0;
    virtual Error poll() = 0;
    virtual int get_stage() const = 0;
    virtual int get_stage_count() const = 0;
    virtual void set_translation_remapped(bool p_remapped) = 0;
    virtual Error wait();

    ResourceInteractiveLoader() = default;
    ~ResourceInteractiveLoader() override;
};

class ResourceFormatLoader : public RefCounted {

    GDCLASS(ResourceFormatLoader, RefCounted)

protected:
    static void _bind_methods();

public:
    virtual Ref<ResourceInteractiveLoader> load_interactive(const String &p_path, const String &p_original_path = String::null_val, Error *r_error = nullptr);
    virtual Ref<Resource> load(const String &p_path, const String &p_original_path = String::null_val, Error *r_error = nullptr);
    virtual bool exists(const String &p_path) const;
    virtual void get_recognized_extensions(ListPOD<String> *p_extensions) const;
    virtual void get_recognized_extensions_for_type(const String &p_type, ListPOD<String> *p_extensions) const;
    virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const;
    virtual bool handles_type(const String &p_type) const;
    virtual String get_resource_type(const String &p_path) const;
    virtual void get_dependencies(const String &p_path, ListPOD<String> *p_dependencies, bool p_add_types = false);
    virtual Error rename_dependencies(const String &p_path, const DefMap<String, String> &p_map);
    virtual bool is_import_valid(const String & /*p_path*/) const { return true; }
    virtual bool is_imported(const String & /*p_path*/) const { return false; }
    virtual int get_import_order(const String & /*p_path*/) const { return 0; }
    virtual String get_import_group_file(const String & /*p_path*/) const { return String::null_val; } //no group

    ~ResourceFormatLoader() override = default;
};
