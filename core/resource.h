/*************************************************************************/
/*  resource.h                                                           */
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

#include "core/reference.h"
#include "core/os/rw_lock.h"
#include "core/engine_entities.h"

namespace eastl {
template <typename Key, typename T, typename Compare, typename Allocator>
class map;
}
class Node;
class wrap_allocator;
template <class K,class V>
using DefMap = eastl::map<K,V,eastl::less<K>,wrap_allocator>;

#define RES_BASE_EXTENSION_IMPL(m_class,m_ext)                                                                      \
void m_class::register_custom_data_to_otdb() {                                                                      \
    ClassDB::add_resource_base_extension(StringName(m_ext), get_class_static_name());                               \
}

#define RES_BASE_EXTENSION(m_ext)                                                                                   \
public:                                                                                                             \
    StringName get_base_extension() const override { return StringName(m_ext); }                                    \
                                                                                                                    \
    static void register_custom_data_to_otdb();                                                                     \
private:


class GODOT_EXPORT Resource : public RefCounted {

    GDCLASS(Resource,RefCounted)
    SE_CLASS()
//    Q_GADGET
//    Q_CLASSINFO("Category","Resources")
    SE_PROPERTY(bool resource_local_to_scene READ is_local_to_scene WRITE set_local_to_scene )
    OBJ_CATEGORY("Resources")
private:
    friend class ResBase;
    friend class ResourceCache;
    friend class SceneState;
    struct Data;
    Data *impl_data;


protected:
    void emit_changed();

    void notify_change_to_owners();

    virtual void _resource_path_changed();
    static void _bind_methods();
public:
    void _set_path(StringView p_path);
    void _take_over_path(StringView p_path);
//Q_SIGNALS:
    void changed();
public:
    virtual StringName get_base_extension() const { return StringName("res"); }
    virtual Ref<Resource> duplicate(bool p_subresources = false) const;
    virtual bool editor_can_reload_from_file();
    virtual void reload_from_file();
    virtual void set_path(StringView p_path, bool p_take_over = false);
    virtual void setup_local_to_scene();
    virtual RenderingEntity get_rid() const; // some resources may offer conversion to RID
    virtual RID get_phys_rid() const; // some resources may offer conversion to RID
    static void register_custom_data_to_otdb();

    static Node *(*_get_local_scene_func)(); //used by editor


    void register_owner(Object *p_owner);
    void unregister_owner(Object *p_owner);

    void set_name(StringView p_name);
    const String &get_name() const;

    const String &get_path() const;

    void set_subindex(int p_sub_index);
    int get_subindex() const;

    Ref<Resource> duplicate_for_local_scene(Node *p_for_scene, DefMap<Ref<Resource>, Ref<Resource> > &remap_cache);
    void configure_for_local_scene(Node *p_for_scene, DefMap<Ref<Resource>, Ref<Resource> > &remap_cache);

    SE_INVOCABLE void set_local_to_scene(bool p_enable);
    SE_INVOCABLE bool is_local_to_scene() const;

    Node *get_local_scene() const;


    void set_as_translation_remapped(bool p_remapped);
    bool is_translation_remapped() const;

#ifdef DEBUG_ENABLED
    /// Used in ObjectDB::cleanup() warning print
    const char *get_dbg_name() const override;
#endif

    Resource();
    ~Resource() override;
};

using RES = Ref<Resource>;

class GODOT_EXPORT ResourceCache {
    friend class Resource;
    friend class ResourceManager; //need the lock
    friend class ResourceRemapper; //need the lock
    friend void unregister_core_types();
    friend void register_core_types();

    static RWLock lock;
    static void clear();
    static Resource *get_unguarded(StringView p_path);
public:
    static void reload_externals();
    static bool has(StringView p_path);
    static Resource *get(StringView p_path);
    static void dump(StringView p_file = nullptr, bool p_short = false);
    static void get_cached_resources(Vector<Ref<Resource>> &p_resources);
    static int get_cached_resource_count();
};
