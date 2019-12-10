#pragma once

#include "core/reference.h"
#include "core/os/thread.h"
#include "core/se_string.h"

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
    se_string path_loading;
    Thread::ID path_loading_thread;

protected:
    static void _bind_methods();

public:
    virtual void set_local_path(se_string_view p_local_path) = 0;
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
    virtual Ref<ResourceInteractiveLoader> load_interactive(se_string_view p_path, se_string_view p_original_path = se_string_view(), Error *r_error = nullptr);
    virtual Ref<Resource> load(se_string_view p_path, se_string_view p_original_path = se_string_view(), Error *r_error = nullptr);
    virtual bool exists(se_string_view p_path) const;
    virtual void get_recognized_extensions(PODVector<se_string> &p_extensions) const;
    virtual void get_recognized_extensions_for_type(se_string_view p_type, PODVector<se_string> &p_extensions) const;
    virtual bool recognize_path(se_string_view p_path, se_string_view p_for_type = se_string_view()) const;
    virtual bool handles_type(se_string_view p_type) const;
    virtual se_string get_resource_type(se_string_view p_path) const;
    virtual void get_dependencies(se_string_view p_path, ListPOD<se_string> *p_dependencies, bool p_add_types = false);
    virtual Error rename_dependencies(se_string_view p_path, const DefMap<se_string, se_string> &p_map);
    virtual bool is_import_valid(se_string_view /*p_path*/) const { return true; }
    virtual bool is_imported(se_string_view /*p_path*/) const { return false; }
    virtual int get_import_order(se_string_view /*p_path*/) const { return 0; }
    virtual se_string get_import_group_file(se_string_view /*p_path*/) const { return se_string(); } //no group

    ~ResourceFormatLoader() override = default;
};
