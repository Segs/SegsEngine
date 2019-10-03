/*************************************************************************/
/*  resource.cpp                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "resource.h"

#include "core/class_db.h"
#include "core/core_string_names.h"
#include "core/hash_map.h"
#include "core/io/resource_loader.h"
#include "core/map.h"
#include "core/object_db.h"
#include "core/os/file_access.h"
#include "core/script_language.h"
#include "core/self_list.h"
#include "core/ustring.h"
//TODO: SEGS consider removing 'scene/main/node.h' include from core module
#include "scene/main/node.h" //only so casting works
#include "core/method_bind.h"

#include <cstdio>
namespace {
    DefHashMap<String, Resource *> cached_resources;

} // end of anonymous namespace

struct Resource::Data {
    Data(Resource *own) : remapped_list(own) {}
#ifdef TOOLS_ENABLED
    Map<String, int> id_for_path;
    String import_path;
#endif
    Set<ObjectID> owners;
    SelfList<Resource> remapped_list;
    String name;
    String path_cache;
    Node *local_scene = nullptr;
    int subindex=0;
    bool local_to_scene=false;

};


IMPL_GDCLASS(Resource)
RES_BASE_EXTENSION_IMPL(Resource,"res")

void Resource::emit_changed() {

    emit_signal(CoreStringNames::get_singleton()->changed);
}

void Resource::_resource_path_changed() {
}

void Resource::set_path(const String &p_path, bool p_take_over) {

    if (impl_data->path_cache == p_path)
        return;

    if (!impl_data->path_cache.empty()) {

        ResourceCache::lock->write_lock();
        cached_resources.erase(impl_data->path_cache);
        ResourceCache::lock->write_unlock();
    }

    impl_data->path_cache = "";

    ResourceCache::lock->read_lock();
    bool has_path = cached_resources.contains(p_path);
    ResourceCache::lock->read_unlock();

    if (has_path) {
        if (p_take_over) {

            ResourceCache::lock->write_lock();
            cached_resources.at(p_path)->set_name(String::null_val);
            ResourceCache::lock->write_unlock();
        } else {
            ResourceCache::lock->read_lock();
            bool exists = cached_resources.contains(p_path);
            ResourceCache::lock->read_unlock();

			ERR_FAIL_COND_MSG(exists, "Another resource is loaded from path '" + p_path + "' (possible cyclic resource inclusion).")
        }
    }
    impl_data->path_cache = p_path;

    if (!impl_data->path_cache.empty()) {

        ResourceCache::lock->write_lock();
        cached_resources[impl_data->path_cache] = this;
        ResourceCache::lock->write_unlock();
    }

    _change_notify("resource_path");
    _resource_path_changed();
}

String Resource::get_path() const {

    return impl_data->path_cache;
}

void Resource::set_subindex(int p_sub_index) {

    impl_data->subindex = p_sub_index;
}

int Resource::get_subindex() const {

    return impl_data->subindex;
}

void Resource::set_name(const String &p_name) {

    impl_data->name = p_name;
    _change_notify("resource_name");
}
String Resource::get_name() const {

    return impl_data->name;
}

bool Resource::editor_can_reload_from_file() {

    return true; //by default yes
}

void Resource::reload_from_file() {

    String path = get_path();
    if (!PathUtils::is_resource_file(path))
        return;

    Ref<Resource> s = ResourceLoader::load(ResourceLoader::path_remap(path), get_class(), true);

    if (not s)
        return;

    ListPOD<PropertyInfo> pi;
    s->get_property_list(&pi);

    for(PropertyInfo &E : pi ) {

        if (!(E.usage & PROPERTY_USAGE_STORAGE))
            continue;
        if (E.name == "resource_path")
            continue; //do not change path

        set(E.name, s->get(E.name));
    }
}

Ref<Resource> Resource::duplicate_for_local_scene(Node *p_for_scene, Map<Ref<Resource>, Ref<Resource> > &remap_cache) {

    ListPOD<PropertyInfo> plist;
    get_property_list(&plist);

    Resource *r = Object::cast_to<Resource>(ClassDB::instance(get_class_name()));
    ERR_FAIL_COND_V(!r, Ref<Resource>())

    r->impl_data->local_scene = p_for_scene;

    for(PropertyInfo &E : plist ) {

        if (!(E.usage & PROPERTY_USAGE_STORAGE))
            continue;
        Variant p = get(E.name);
        if (p.get_type() == VariantType::OBJECT) {

            RES sr(refFromVariant<Resource>(p));
            if (sr) {

                if (sr->is_local_to_scene()) {
                    if (remap_cache.contains(sr)) {
                        p = remap_cache[sr];
                    } else {

                        RES dupe(sr->duplicate_for_local_scene(p_for_scene, remap_cache));
                        p = dupe;
                        remap_cache[sr] = dupe;
                    }
                }
            }
        }

        r->set(E.name, p);
    }

    RES res(r);

    return res;
}

void Resource::configure_for_local_scene(Node *p_for_scene, Map<Ref<Resource>, Ref<Resource> > &remap_cache) {

    ListPOD<PropertyInfo> plist;
    get_property_list(&plist);

    impl_data->local_scene = p_for_scene;

    for(PropertyInfo &E : plist ) {

        if (!(E.usage & PROPERTY_USAGE_STORAGE))
            continue;
        Variant p = get(E.name);
        if (p.get_type() == VariantType::OBJECT) {

            RES sr(refFromVariant<Resource>(p));
            if (sr) {

                if (sr->is_local_to_scene()) {
                    if (!remap_cache.contains(sr)) {
                        sr->configure_for_local_scene(p_for_scene, remap_cache);
                        remap_cache[sr] = sr;
                    }
                }
            }
        }
    }
}

Ref<Resource> Resource::duplicate(bool p_subresources) const {

    ListPOD<PropertyInfo> plist;
    get_property_list(&plist);

    Resource *r = (Resource *)ClassDB::instance(get_class_name());
    ERR_FAIL_COND_V(!r, Ref<Resource>())

    for(PropertyInfo &E : plist ) {

        if (!(E.usage & PROPERTY_USAGE_STORAGE))
            continue;
        Variant p = get(E.name);

        if ((p.get_type() == VariantType::DICTIONARY || p.get_type() == VariantType::ARRAY)) {
            r->set(E.name, p.duplicate(p_subresources));
        } else if (p.get_type() == VariantType::OBJECT && (p_subresources || (E.usage & PROPERTY_USAGE_DO_NOT_SHARE_ON_DUPLICATE))) {

            RES sr(refFromVariant<Resource>(p));
            if (sr) {
                r->set(E.name, sr->duplicate(p_subresources));
            }
        } else {

            r->set(E.name, p);
        }
    }

    return Ref<Resource>(r);
}

void Resource::_set_path(const String &p_path) {

    set_path(p_path, false);
}

void Resource::_take_over_path(const String &p_path) {

    set_path(p_path, true);
}

RID Resource::get_rid() const {

    return RID();
}

void Resource::register_owner(Object *p_owner) {

    impl_data->owners.insert(p_owner->get_instance_id());
}

void Resource::unregister_owner(Object *p_owner) {

    impl_data->owners.erase(p_owner->get_instance_id());
}

void Resource::notify_change_to_owners() {

    for (const ObjectID E : impl_data->owners) {

        Object *obj = ObjectDB::get_instance(E);
        ERR_CONTINUE_MSG(!obj, "Object was deleted, while still owning a resource.") //wtf
        //TODO store string
        obj->call("resource_changed", RES(this));
    }
}

#ifdef TOOLS_ENABLED

uint32_t Resource::hash_edited_version() const {

    uint32_t hash = hash_djb2_one_32(get_edited_version());

    ListPOD<PropertyInfo> plist;
    get_property_list(&plist);

    for(PropertyInfo &E : plist ) {

        if (E.usage & PROPERTY_USAGE_STORAGE && E.type == VariantType::OBJECT && E.hint == PROPERTY_HINT_RESOURCE_TYPE) {
            RES res(refFromVariant<Resource>(get(E.name)));
            if (res) {
                hash = hash_djb2_one_32(res->hash_edited_version(), hash);
            }
        }
    }

    return hash;
}

void Resource::set_import_path(const String &p_path) { impl_data->import_path = p_path; }

String Resource::get_import_path() const { return impl_data->import_path; }

#endif

void Resource::set_local_to_scene(bool p_enable) {

    impl_data->local_to_scene = p_enable;
}

bool Resource::is_local_to_scene() const {

    return impl_data->local_to_scene;
}

Node *Resource::get_local_scene() const {

    if (impl_data->local_scene)
        return impl_data->local_scene;

    if (_get_local_scene_func) {
        return _get_local_scene_func();
    }

    return nullptr;
}

void Resource::setup_local_to_scene() {

    if (get_script_instance())
        get_script_instance()->call(StaticCString("_setup_local_to_scene"));
}

Node *(*Resource::_get_local_scene_func)() = nullptr;

void Resource::set_as_translation_remapped(bool p_remapped) {

    if (impl_data->remapped_list.in_list() == p_remapped)
        return;

    if (ResourceCache::lock) {
        ResourceCache::lock->write_lock();
    }

    if (p_remapped) {
        ResourceLoader::remapped_list.add(&impl_data->remapped_list);
    } else {
        ResourceLoader::remapped_list.remove(&impl_data->remapped_list);
    }

    if (ResourceCache::lock) {
        ResourceCache::lock->write_unlock();
    }
}

bool Resource::is_translation_remapped() const {

    return impl_data->remapped_list.in_list();
}

#ifdef TOOLS_ENABLED
//helps keep IDs same number when loading/saving scenes. -1 clears ID and it Returns -1 when no id stored
void Resource::set_id_for_path(const String &p_path, int p_id) {
    if (p_id == -1) {
        impl_data->id_for_path.erase(p_path);
    } else {
        impl_data->id_for_path[p_path] = p_id;
    }
}

int Resource::get_id_for_path(const String &p_path) const {

    if (impl_data->id_for_path.contains(p_path)) {
        return impl_data->id_for_path.at(p_path);
    } else {
        return -1;
    }
}
#endif
String Resource::_get_category_wrap() {
    return String(_get_category());
}

void Resource::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_path", {"path"}), &Resource::_set_path);
    MethodBinder::bind_method(D_METHOD("take_over_path", {"path"}), &Resource::_take_over_path);
    MethodBinder::bind_method(D_METHOD("get_path"), &Resource::get_path);
    MethodBinder::bind_method(D_METHOD("set_name", {"name"}), &Resource::set_name);
    MethodBinder::bind_method(D_METHOD("get_name"), &Resource::get_name);
    MethodBinder::bind_method(D_METHOD("get_rid"), &Resource::get_rid);
    MethodBinder::bind_method(D_METHOD("set_local_to_scene", {"enable"}), &Resource::set_local_to_scene);
    MethodBinder::bind_method(D_METHOD("is_local_to_scene"), &Resource::is_local_to_scene);
    MethodBinder::bind_method(D_METHOD("get_local_scene"), &Resource::get_local_scene);
    MethodBinder::bind_method(D_METHOD("setup_local_to_scene"), &Resource::setup_local_to_scene);

    MethodBinder::bind_method(D_METHOD("duplicate", {"subresources"}), &Resource::duplicate, {DEFVAL(false)});
    ADD_SIGNAL(MethodInfo("changed"));
    ADD_GROUP("Resource", "resource_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "resource_local_to_scene"), "set_local_to_scene", "is_local_to_scene");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "resource_path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_path", "get_path");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "resource_name"), "set_name", "get_name");

    BIND_VMETHOD(MethodInfo("_setup_local_to_scene"))
}

Resource::Resource() :
        impl_data(memnew_args_basic(Resource::Data,this)) {

#ifdef TOOLS_ENABLED
    last_modified_time = 0;
    import_last_modified_time = 0;
#endif

}

Resource::~Resource() {

    if (!impl_data->path_cache.empty()) {
        ResourceCache::lock->write_lock();
        cached_resources.erase(impl_data->path_cache);
        ResourceCache::lock->write_unlock();
    }
    if (!impl_data->owners.empty()) {
        WARN_PRINT("Resource is still owned.")
    }
    memdelete(impl_data);
    impl_data = nullptr;
}

RWLock *ResourceCache::lock = nullptr;

void ResourceCache::setup() {

    lock = RWLock::create();
}

void ResourceCache::clear() {
    if (!cached_resources.empty())
        ERR_PRINT("Resources Still in use at Exit!")

    cached_resources.clear();
    memdelete(lock);
}

void ResourceCache::reload_externals() {

    /*
    const String *K=NULL;
    while ((K=resources.next(K))) {
        resources[*K]->reload_external_data();
    }
    */
}

bool ResourceCache::has(const String &p_path) {

    lock->read_lock();
    bool b = cached_resources.contains(p_path);
    lock->read_unlock();

    return b;
}

Resource * ResourceCache::get_unguarded(const String &p_path) {
    return cached_resources.at(p_path,nullptr);
}

Resource *ResourceCache::get(const String &p_path) {

    lock->read_lock();

    Resource *res = cached_resources.at(p_path,nullptr);

    lock->read_unlock();

    return res;
}

void ResourceCache::get_cached_resources(List<Ref<Resource> > *p_resources) {

    lock->read_lock();
    for(eastl::pair<const String,Resource *> & e :cached_resources) {
        p_resources->push_back(Ref<Resource>(e.second));
    }
    lock->read_unlock();
}

int ResourceCache::get_cached_resource_count() {

    lock->read_lock();
    int rc = cached_resources.size();
    lock->read_unlock();

    return rc;
}

void ResourceCache::dump(const char *p_file, bool p_short) {
#ifdef DEBUG_ENABLED
    lock->read_lock();

    Map<String, int> type_count;

    FileAccess *f = nullptr;
    if (p_file) {
        f = FileAccess::open(p_file, FileAccess::WRITE);
		ERR_FAIL_COND_MSG(!f, "Cannot create file at path '" + String(p_file) + "'.")
    }

    for (eastl::pair<const String, Resource *> & e : cached_resources) {

        Resource *r = e.second;

        if (!type_count.contains(r->get_class())) {
            type_count[r->get_class()] = 0;
        }

        type_count[r->get_class()]++;

        if (!p_short) {
            if (f)
                f->store_line(String(r->get_class()) + ": " + r->get_path());
        }
    }

    for (const eastl::pair<const String,int> &E : type_count) {

        if (f)
            f->store_line(E.first + " count: " + itos(E.second));
    }
    if (f) {
        f->close();
        memdelete(f);
    }

    lock->read_unlock();

#endif
}
