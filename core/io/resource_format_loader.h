#pragma once

#include "core/reference.h"
#include "core/os/thread.h"
#include "core/string.h"
#include "core/forward_decls.h"
#include "core/hash_map.h"

template <class T>
struct Comparator;
class wrap_allocator;

class Resource;
using RES = Ref<Resource>;

class GODOT_EXPORT ResourceInteractiveLoader : public RefCounted {

    GDCLASS(ResourceInteractiveLoader, RefCounted)
    friend class ResourceLoader;
    String path_loading;
    Thread::ID path_loading_thread;

protected:
    static void _bind_methods();

public:
    virtual void set_local_path(StringView p_local_path) = 0;
    virtual Ref<Resource> get_resource() = 0;
    virtual Error poll() = 0;
    virtual int get_stage() const = 0;
    virtual int get_stage_count() const = 0;
    virtual void set_translation_remapped(bool p_remapped) = 0;
    virtual Error wait();

    ResourceInteractiveLoader() = default;
    ~ResourceInteractiveLoader() override;
};

class GODOT_EXPORT ResourceFormatLoader : public RefCounted {

    GDCLASS(ResourceFormatLoader, RefCounted)

protected:
    static void _bind_methods();

public:
    virtual Ref<ResourceInteractiveLoader> load_interactive(StringView p_path, StringView p_original_path = StringView(), Error *r_error = nullptr);
    virtual Ref<Resource> load(StringView p_path, StringView p_original_path = StringView(), Error *r_error = nullptr);
    virtual bool exists(StringView p_path) const;
    virtual void get_recognized_extensions(Vector<String> &p_extensions) const;
    virtual void get_recognized_extensions_for_type(StringView p_type, Vector<String> &p_extensions) const;
    virtual bool recognize_path(StringView p_path, StringView p_for_type = StringView()) const;
    virtual bool handles_type(StringView p_type) const;
    virtual String get_resource_type(StringView p_path) const;
    virtual void get_dependencies(StringView p_path, Vector<String> &p_dependencies, bool p_add_types = false);
    virtual Error rename_dependencies(StringView p_path, const HashMap<String, String> &p_map);
    virtual bool is_import_valid(StringView /*p_path*/) const { return true; }
    virtual bool is_imported(StringView /*p_path*/) const { return false; }
    virtual int get_import_order(StringView /*p_path*/) const { return 0; }
    virtual String get_import_group_file(StringView /*p_path*/) const { return String(); } //no group

    ~ResourceFormatLoader() override = default;
};
