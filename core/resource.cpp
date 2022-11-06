/*************************************************************************/
/*  resource.cpp                                                         */
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

#include "resource.h"
#include "rid.h"

#include "core/class_db.h"
#include "core/core_string_names.h"
#include "core/hash_map.h"
#include "core/fixed_string.h"
#include "core/map.h"
#include "core/os/file_access.h"
#include "core/script_language.h"
#include "core/object_tooling.h"
//#include "core/ustring.h"
//TODO: SEGS consider removing 'scene/main/node.h' include from core module
#include "scene/main/node.h" //only so casting works
#include "core/method_bind.h"
#include <QMetaProperty>

#include "resource/resource_manager.h"

namespace {
    HashMap<String, Resource *> cached_resources;

} // end of anonymous namespace

struct Resource::Data {
    Data() = default;
    HashSet<GameEntity> owners;
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

void Resource::set_path(StringView p_path, bool p_take_over) {

    if (impl_data->path_cache == p_path) {
        return;
    }

    if (!impl_data->path_cache.empty()) {
        RWLockWrite write_guard(ResourceCache::lock);
        cached_resources.erase(impl_data->path_cache);
    }
    HashMap<String, Resource*>::iterator lociter;
    bool has_path;
    impl_data->path_cache.clear();
    {
        RWLockRead read_guard(ResourceCache::lock);
        lociter = cached_resources.find_as(p_path);
        has_path = cached_resources.end() != lociter;
    }

    if (has_path) {
        if (p_take_over) {

            RWLockWrite write_guard(ResourceCache::lock);
            //TODO: can `lociter` really change between this and previous call ?
            lociter = cached_resources.find_as(p_path);
            if(lociter!=cached_resources.end()) {
                lociter->second->set_name("");
            }
        } else {
            RWLockRead read_guard(ResourceCache::lock);
            bool exists = cached_resources.find_as(p_path)!=cached_resources.end();

            ERR_FAIL_COND_MSG(exists, "Another resource is loaded from path '" + String(p_path) + "' (possible cyclic resource inclusion).");
        }
    }
    impl_data->path_cache = p_path;

    if (!impl_data->path_cache.empty()) {
        RWLockWrite guard(ResourceCache::lock);

        cached_resources[impl_data->path_cache] = this;
    }

    Object_change_notify(this,"resource_path");
    _resource_path_changed();
}

//! \note a few places are using the returned reference to construct StringViews, if this function no longer returns a long-living string, change the callers
const String &Resource::get_path() const {

    return impl_data->path_cache;
}

void Resource::set_subindex(int p_sub_index) {

    impl_data->subindex = p_sub_index;
}

int Resource::get_subindex() const {

    return impl_data->subindex;
}

void Resource::set_name(StringView p_name) {

    impl_data->name = p_name;
    Object_change_notify(this,"resource_name");
}
const String &Resource::get_name() const {

    return impl_data->name;
}

bool Resource::editor_can_reload_from_file() {

    return true; //by default yes
}

void Resource::reload_from_file() {

    String path = get_path();
    if (!PathUtils::is_resource_file(path)) {
        return;
    }

    Ref<Resource> s = gResourceManager().load(gResourceRemapper().path_remap(path), get_class(), true);

    if (not s) {
        return;
    }

    Vector<PropertyInfo> pi;
    s->get_property_list(&pi);

    for(PropertyInfo &E : pi ) {

        if (!(E.usage & PROPERTY_USAGE_STORAGE))
            continue;
        if (E.name == StringView("resource_path"))
            continue; //do not change path

        set(E.name, s->get(E.name));
    }
}

Ref<Resource> Resource::duplicate_for_local_scene(Node *p_for_scene, Map<Ref<Resource>, Ref<Resource> > &remap_cache) {

    Vector<PropertyInfo> plist;
    get_property_list(&plist);

    Resource *r = object_cast<Resource>(ClassDB::instance(get_class_name()));
    ERR_FAIL_COND_V(!r, Ref<Resource>());

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

    RES res(r,DoNotAddRef);

    return res;
}

void Resource::configure_for_local_scene(Node *p_for_scene, Map<Ref<Resource>, Ref<Resource> > &remap_cache) {

    Vector<PropertyInfo> plist;
    get_property_list(&plist);

    impl_data->local_scene = p_for_scene;

    for(PropertyInfo &E : plist ) {

        if (!(E.usage & PROPERTY_USAGE_STORAGE))
            continue;
        Variant p = get(E.name);
        if (p.get_type() != VariantType::OBJECT)
            continue;

        RES sr(refFromVariant<Resource>(p));
        if (!sr || !sr->is_local_to_scene() || remap_cache.contains(sr))
            continue;

        sr->configure_for_local_scene(p_for_scene, remap_cache);
        remap_cache[sr] = sr;
    }
}

Ref<Resource> Resource::duplicate(bool p_subresources) const {

    Vector<PropertyInfo> plist;
    get_property_list(&plist);

    Resource *r = (Resource *)ClassDB::instance(get_class_name());
    ERR_FAIL_COND_V(!r, Ref<Resource>());

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

    return Ref<Resource>(r,DoNotAddRef);
}

void Resource::_set_path(StringView p_path) {

    set_path(p_path, false);
}

void Resource::_take_over_path(StringView p_path) {

    set_path(p_path, true);
}

RenderingEntity Resource::get_rid() const {
    return entt::null;
}

RID Resource::get_phys_rid() const
{
    return RID();
}

void Resource::register_owner(Object *p_owner) {

    impl_data->owners.insert(p_owner->get_instance_id());
}

void Resource::unregister_owner(Object *p_owner) {

    impl_data->owners.erase(p_owner->get_instance_id());
}

void Resource::notify_change_to_owners() {

    for (GameEntity E : impl_data->owners) {

        Object *obj = object_for_entity(E);
        ERR_CONTINUE_MSG(!obj, "Object was deleted, while still owning a resource."); //wtf
        //TODO store string
        obj->call_va("resource_changed", RES(this));
    }
}


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

    gResourceRemapper().set_as_translation_remapped(this, p_remapped);
}

bool Resource::is_translation_remapped() const {
    return gResourceRemapper().is_translation_remapped(this);
}

#ifdef DEBUG_ENABLED
const char *Resource::get_dbg_name() const {
    static TmpString<2048,false> s_data;
    s_data.assign(get_name().c_str());
    s_data.append(" Path: ");
    s_data.append(get_path().c_str());
    return s_data.c_str();
}
#endif
VariantType fromQVariantType(QVariant::Type t) {
    switch(t) {
    case QVariant::Invalid:
        return VariantType::NIL;
    case QVariant::Bool:
        return VariantType::BOOL;
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
        return VariantType::INT;
    case QVariant::Double:
        return VariantType::FLOAT;
    case QVariant::Char:
        return VariantType::INT;
    default:
        ERR_FAIL_V_MSG(VariantType::NIL,"No known variant->qvariant conversion");
    }
}
//TODO: this is here only because Object is not an QObject yet
void Resource::changed() {}

void Resource::_bind_methods() {
    SE_BIND_METHOD_WRAPPER(Resource,set_path,_set_path);
    SE_BIND_METHOD_WRAPPER(Resource,take_over_path,_take_over_path);
    SE_BIND_METHOD(Resource,get_path);
    SE_BIND_METHOD(Resource,set_name);
    SE_BIND_METHOD(Resource,get_name);
    SE_BIND_METHOD(Resource,get_rid);
    SE_BIND_METHOD(Resource,get_phys_rid);
    SE_BIND_METHOD(Resource,set_local_to_scene);
    SE_BIND_METHOD(Resource,is_local_to_scene);
    SE_BIND_METHOD(Resource,get_local_scene);
    SE_BIND_METHOD(Resource,setup_local_to_scene);

    SE_BIND_METHOD_WITH_DEFAULTS(Resource,duplicate,DEFVAL(false));

//    const auto &mo = Resource::staticMetaObject;
//    for(int i=0; i<mo.methodCount(); ++i) {
//        const auto &method(mo.method(i));
//        if(method.methodType()==QMetaMethod::Signal) {
//            const char *z=method.typeName();
//            printf("%s",z);
//            ADD_SIGNAL(MethodInfo(method.name().constData()));
//        }
//    }
    ADD_SIGNAL(MethodInfo("changed"));
    ADD_GROUP("Resource", "resource_");
    //    for(int enum_idx = 0; enum_idx < mo.enumeratorCount(); ++enum_idx) {
    //        const QMetaEnum &me(mo.enumerator(enum_idx));
    //        for(int i=0; i<me.keyCount(); ++i)
    //        {
    //            ClassDB::bind_integer_constant(get_class_static_name(), StaticCString(me.name(),true), StaticCString(me.key(i),true), me.value(i));
    //        }
    //    }
//    for(int prop_idx = 0; prop_idx< mo.propertyCount(); ++prop_idx) {
//        const QMetaProperty &prop(mo.property(prop_idx));
//        //PropertyInfo pi(fromQVariantType(prop.type()), prop.name(), "set_local_to_scene", "is_local_to_scene");
//    }
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "resource_local_to_scene"), "set_local_to_scene", "is_local_to_scene");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "resource_path", PropertyHint::None, "", PROPERTY_USAGE_EDITOR), "set_path", "get_path");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "resource_name"), "set_name", "get_name");

    BIND_VMETHOD(MethodInfo("_setup_local_to_scene"));
}

Resource::Resource() :
        impl_data(memnew(Resource::Data)) {
}

Resource::~Resource() {

    if (!impl_data->path_cache.empty()) {
        RWLockWrite wr_guard(ResourceCache::lock);
        cached_resources.erase(impl_data->path_cache);
    }
    gResourceRemapper().remove_remap(this);

    if (!impl_data->owners.empty()) {
        WARN_PRINT("Resource is still owned.");
    }
    memdelete(impl_data);
    impl_data = nullptr;
}

RWLock ResourceCache::lock;

void ResourceCache::clear() {
    if (!cached_resources.empty()) {
        ERR_PRINT("Resources Still in use at Exit!");
    }

    cached_resources.clear();
}

void ResourceCache::reload_externals() {

    /*
    const String *K=nullptr;
    while ((K=resources.next(K))) {
        resources[*K]->reload_external_data();
    }
    */
}

bool ResourceCache::has(StringView p_path) {


    lock.read_lock();
    bool b = cached_resources.find_as(p_path)!=cached_resources.end();
    lock.read_unlock();

    return b;
}

Resource * ResourceCache::get_unguarded(StringView p_path) {
    return cached_resources.at(String(p_path),nullptr);
}

Resource *ResourceCache::get(StringView p_path) {

    RWLockRead guard(lock);

    Resource *res = cached_resources.at(String(p_path),nullptr);

    return res;
}

void ResourceCache::get_cached_resources(Vector<Ref<Resource>> &p_resources) {

    RWLockRead guard(lock);
    p_resources.reserve(cached_resources.size());
    for(eastl::pair<const String,Resource *> & e : cached_resources) {
        p_resources.emplace_back(e.second);
    }
}

int ResourceCache::get_cached_resource_count() {

    RWLockRead guard(lock);

    int rc = cached_resources.size();

    return rc;
}

void ResourceCache::dump(StringView p_file, bool p_short) {
#ifdef DEBUG_ENABLED
    RWLockRead guard(lock);

    Map<String, int> type_count;

    FileAccess *f = nullptr;
    if (not p_file.empty()) {
        f = FileAccess::open(p_file, FileAccess::WRITE);
        ERR_FAIL_COND_MSG(!f, "Cannot create file at path '" + String(p_file) + "'.");
    }

    for (eastl::pair<const String, Resource *> & e : cached_resources) {

        Resource *r = e.second;

        if (!type_count.contains(r->get_class())) {
            type_count[r->get_class()] = 0;
        }

        type_count[r->get_class()]++;

        if (!p_short) {
            if (f) {
                f->store_line(String(r->get_class()) + ": " + r->get_path());
            }
        }
    }

    for (const eastl::pair<const String,int> &E : type_count) {

        if (f) {
            f->store_line(E.first + " count: " + itos(E.second));
        }
    }
    if (f) {
        f->close();
        memdelete(f);
    }

#else
    WARN_PRINT("ResourceCache::dump only with in debug builds.");
#endif
}
