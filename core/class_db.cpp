/*************************************************************************/
/*  class_db.cpp                                                         */
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

#include "class_db.h"

#include "object_tooling.h"
#include "EASTL/sort.h"
#include "core/engine.h"
#include "core/error_macros.h"
#include "core/hashfuncs.h"
#include "core/method_bind_interface.h"
#include "core/method_info.h"
#include "core/object.h"
#include "core/os/rw_lock.h"
#include "core/string_utils.h"
#include "core/version.h"
#include <cassert>

#define OBJTYPE_RLOCK RWLockRead _rw_lockr_(classdb_lock);
#define OBJTYPE_WLOCK RWLockWrite _rw_lockw_(classdb_lock);
static RWLock classdb_lock;

#ifdef DEBUG_METHODS_ENABLED

// MethodDefinition D_METHOD(StringName p_name) {

//    MethodDefinition md;
//    md.name = eastl::move(p_name);
//    return md;
//}

// MethodDefinition D_METHOD(StringName p_name, Vector<StringName> &&names) {

//    MethodDefinition md;
//    md.name = eastl::move(p_name);
//    md.args = eastl::move(names);
//    return md;
//}

#endif
// Non-locking variants of get_parent_class and is_parent_class.
static StringName _get_parent_class(const StringName &p_class) {
    auto ti = ClassDB::classes.find(p_class);
    ERR_FAIL_COND_V_MSG(ti==ClassDB::classes.end(), StringName(), "Cannot get class '" + String(p_class) + "'.");
    return ti->second.inherits;
}
static bool _is_parent_class(const StringName &p_class, const StringName &p_inherits) {

    StringName inherits = p_class;

    while (!inherits.empty()) {

        if (inherits == p_inherits) {

            return true;
        }
        inherits = _get_parent_class(inherits);
    }

    return false;
}

struct ClassInfoImpl {
    HashMap<StringName, MethodInfo> signal_map;
};
static HashMap<StringName, MethodInfo> &class_signal_map(ClassDB_ClassInfo &ci) {
    return ci.class_signal_map();
}
ClassDB_APIType ClassDB::current_api = API_CORE;

void ClassDB::set_current_api(ClassDB_APIType p_api) {
    current_api = p_api;
}

ClassDB_APIType ClassDB::get_current_api() {
    return current_api;
}

HashMap<StringName, ClassDB_ClassInfo> ClassDB::classes;
static HashMap<StringName, StringName> resource_base_extensions;
static HashMap<StringName, StringName> compat_classes;

struct NamespaceInfo {
    HashMap<StringName, ClassDB_ClassInfo> classes;
    Vector<NamespaceInfo *> nested_namespaces;
};
static HashMap<StringName, NamespaceInfo> namespaces;

ClassDB_ClassInfo::ClassDB_ClassInfo() = default;

ClassDB_ClassInfo::~ClassDB_ClassInfo() {
    for (auto &entry : method_map) {
        memdelete(entry.second);
    }
    method_map.clear();
}

bool ClassDB::is_parent_class(const StringName &p_class, const StringName &p_inherits) {
    RWLockRead _rw_lockr_(classdb_lock);

    StringName inherits = p_class;

    while (!inherits.empty()) {
        if (inherits == p_inherits) {
            return true;
        }
        inherits = _get_parent_class(inherits);
    }

    return false;
}
void ClassDB::get_class_list(Vector<StringName> *p_classes) {
    RWLockRead _rw_lockr_(classdb_lock);
    p_classes->reserve(p_classes->size() + classes.size());
    for (const auto &k : classes) {
        p_classes->emplace_back(k.first);
    }
    eastl::sort(p_classes->begin(), p_classes->end());
}

void ClassDB::get_inheriters_from_class(const StringName &p_class, Vector<StringName> *p_classes) {
    RWLockRead _rw_lockr_(classdb_lock);

    for (const auto &k : classes) {
        if (k.first != p_class && _is_parent_class(k.first, p_class)) {
            p_classes->push_back(k.first);
        }
    }
}

void ClassDB::get_direct_inheriters_from_class(const StringName &p_class, Vector<StringName> *p_classes) {
    RWLockRead _rw_lockr_(classdb_lock);

    for (const auto &k : classes) {
        if (k.first != p_class && _get_parent_class(k.first) == p_class) {
            p_classes->push_back(k.first);
        }
    }
}

StringName ClassDB::get_compatibility_remapped_class(const StringName &p_class) {
    if (classes.contains(p_class)) {
        return p_class;
    }

    if (compat_classes.contains(p_class)) {
        return compat_classes[p_class];
    }

    return p_class;
}

StringName ClassDB::get_parent_class_nocheck(const StringName &p_class) {
    RWLockRead _rw_lockr_(classdb_lock);
    const auto iter = classes.find(p_class);
    if (iter == classes.end()) {
        return StringName();
    }
    return iter->second.inherits;
}

StringName ClassDB::get_parent_class(const StringName &p_class) {
    RWLockRead _rw_lockr_(classdb_lock);

    return _get_parent_class(p_class);

}

ClassDB_APIType ClassDB::get_api_type(const StringName &p_class) {
    RWLockRead _rw_lockr_(classdb_lock);

    auto iter = classes.find(p_class);
    ERR_FAIL_COND_V_MSG(iter == classes.end(), API_NONE, "Cannot get class '" + String(p_class) + "'.");
    return iter->second.api;
}

uint64_t ClassDB::get_api_hash(ClassDB_APIType p_api) {
    using class_iter = HashMap<StringName, ClassDB_ClassInfo>::iterator;
    RWLockRead _rw_lockr_(classdb_lock);
#ifdef DEBUG_METHODS_ENABLED
    uint64_t hash = hash_djb2_one_64(Hasher<const char *>()(VERSION_FULL_CONFIG));
    // TODO: bunch of copies are made here, the containers should just hold pointers/const references to objects ?
    Vector<class_iter> entries;
    entries.reserve(classes.size());

    for (auto iter = classes.begin(), fin = classes.end(); iter != fin; ++iter) {
        entries.push_back(iter);
    }
    eastl::stable_sort(entries.begin(), entries.end(),
            [](class_iter a, class_iter b) -> bool { return StringName::AlphCompare(a->first, b->first); });
    // must be alphabetically sorted for hash to compute
    Vector<StringName> snames;

    for (const auto &iter : entries) {
        ClassDB_ClassInfo &t = iter->second;
        if (t.api != p_api || !t.exposed) {
            continue;
        }
        hash = hash_djb2_one_64(t.name.hash(), hash);
        hash = hash_djb2_one_64(t.inherits.hash(), hash);

        { // methods

            snames.clear();
            snames.reserve(t.method_map.size());

            for (const auto &v : t.method_map) {
                const char *name = v.first.asCString();

                ERR_CONTINUE(name == nullptr);

                if (name[0] == '_') {
                    continue; // Ignore non-virtual methods that start with an underscore
                }

                snames.push_back(v.first);
            }

            eastl::stable_sort(snames.begin(), snames.end(), StringName::AlphCompare);

            for (const StringName &sn : snames) {
                MethodBind *mb = t.method_map[sn];
                hash = hash_djb2_one_64(mb->get_name().hash(), hash);
                hash = hash_djb2_one_64(mb->get_argument_count(), hash);
                hash = hash_djb2_one_64(uint64_t(mb->get_argument_type(-1)), hash); // return

                for (int i = 0; i < mb->get_argument_count(); i++) {
                    const PropertyInfo info = mb->get_argument_info(i);
                    hash = hash_djb2_one_64(uint64_t(info.type), hash);
                    // all names are autogenerated arg+arg_idx
                    //hash = hash_djb2_one_64(StringUtils::hash(info.name), hash);
                    hash = hash_djb2_one_64(i, hash);
                    hash = hash_djb2_one_64((int)info.hint, hash);
                    hash = hash_djb2_one_64(StringUtils::hash(info.hint_string), hash);
                }

                hash = hash_djb2_one_64(mb->get_default_argument_count(), hash);

                for (int i = 0; i < mb->get_default_argument_count(); i++) {
                    // hash should not change, i hope for tis
                    Variant da = mb->get_default_argument(i);
                    hash = hash_djb2_one_64(da.hash(), hash);
                }

                hash = hash_djb2_one_64(mb->get_hint_flags(), hash);
            }
        }

        { // constants

            snames.clear();

            for (const auto &k : t.constant_map) {
                snames.emplace_back(k.first);
            }
            eastl::stable_sort(snames.begin(), snames.end(), StringName::AlphCompare);

            for (StringName &sn : snames) {
                hash = hash_djb2_one_64(sn.hash(), hash);
                hash = hash_djb2_one_64(t.constant_map[sn], hash);
            }
        }

        { // signals

            snames.clear();
            t.class_signal_map().keys_into(snames);
            eastl::stable_sort(snames.begin(), snames.end(), StringName::AlphCompare);

            for (StringName &sn : snames) {
                MethodInfo &mi = class_signal_map(t)[sn];
                hash = hash_djb2_one_64(sn.hash(), hash);
                for (const PropertyInfo &argument : mi.arguments) {
                    hash = hash_djb2_one_64(uint64_t(argument.type), hash);
                }
            }
        }

        { // properties

            snames.clear();

            t.property_setget.keys_into(snames);

            eastl::stable_sort(snames.begin(), snames.end(), StringName::AlphCompare);

            for (StringName &sn : snames) {
                ClassDB_PropertySetGet &psg(t.property_setget[sn]);

                hash = hash_djb2_one_64(sn.hash(), hash);
                hash = hash_djb2_one_64(psg.setter.hash(), hash);
                hash = hash_djb2_one_64(psg.getter.hash(), hash);
            }
        }

        // property list
        for (const PropertyInfo &pi : t.property_list) {
            hash = hash_djb2_one_64(StringUtils::hash(pi.name), hash);
            hash = hash_djb2_one_64(uint64_t(pi.type), hash);
            hash = hash_djb2_one_64((int)pi.hint, hash);
            hash = hash_djb2_one_64(StringUtils::hash(pi.hint_string), hash);
            hash = hash_djb2_one_64(pi.usage, hash);
        }
    }

    return hash;
#else
    return 0;
#endif
}

bool ClassDB::class_exists(const StringName &p_class) {
    OBJTYPE_RLOCK
    return classes.contains(p_class);
}

void ClassDB::add_compatibility_class(const StringName &p_class, const StringName &p_fallback) {
    OBJTYPE_WLOCK
    compat_classes[p_class] = p_fallback;
}

Object *ClassDB::instance(const StringName &p_class) {
    ClassDB_ClassInfo *ti;
    {
        RWLockRead _rw_lockr_(classdb_lock);
        auto iter = classes.find(p_class);
        if (iter == classes.end() || iter->second.disabled || !iter->second.creation_func) {
            if (compat_classes.contains(p_class)) {
                iter = classes.find(compat_classes[p_class]);
            }
        }
        ERR_FAIL_COND_V_MSG(iter == classes.end(), nullptr, "Cannot get class '" + String(p_class) + "'.");
        ti = &iter->second;
        ERR_FAIL_COND_V_MSG(ti->disabled, nullptr, "Class '" + String(p_class) + "' is disabled.");
        ERR_FAIL_COND_V_MSG(!ti->creation_func, nullptr, "Class '" + String(p_class) + "' or its base class cannot be instantiated.");
    }
    if(!Tooling::class_can_instance_cb(ti,p_class)) {
        return nullptr;
    }
    return ti->creation_func();
}

bool ClassDB::can_instance(const StringName &p_class) {
    RWLockRead _rw_lockr_(classdb_lock);

    auto iter = classes.find(p_class);
    ERR_FAIL_COND_V_MSG(iter == classes.end(), false, "Cannot get class '" + String(p_class) + "'.");
    ClassDB_ClassInfo *ti = &iter->second;
    if (!Tooling::class_can_instance_cb(ti, p_class)) {
        return false;
    }
    return (!ti->disabled && ti->creation_func != nullptr);
}

void ClassDB::_add_class2(const StringName &p_class, const StringName &p_inherits) {
    OBJTYPE_WLOCK;

    const StringName &name = p_class;

    ERR_FAIL_COND_MSG(classes.contains(name), "Class '" + String(p_class) + "' already exists.");

    ClassDB_ClassInfo &ti = classes[name];
    ti = ClassDB_ClassInfo();
    ti.name = name;
    ti.inherits = p_inherits;
    ti.api = current_api;

    if (!ti.inherits.empty()) {
        ERR_FAIL_COND(!classes.contains(ti.inherits)); // it MUST be registered.
        ti.inherits_ptr = &classes[ti.inherits];

    } else {
        ti.inherits_ptr = nullptr;
    }
}

void ClassDB::add_namespace(const StringName &ns, StringView header_file) {
    GLOBAL_LOCK_FUNCTION
            ERR_FAIL_COND(classes.find(ns)!=classes.end());
    ClassDB_ClassInfo &ti = classes[ns];
    ti = ClassDB_ClassInfo();
    ti.name = ns;
    ti.inherits = StringName();
    ti.api = current_api;
    ti.inherits_ptr=nullptr;
    ti.exposed = true;
    ti.is_namespace=true;
#ifdef DEBUG_METHODS_ENABLED
    ti.usage_header=header_file;
#endif
}

static MethodInfo info_from_bind(MethodBind *bind) {
    MethodInfo minfo;
    minfo.name = bind->get_name();
    minfo.id = bind->get_method_id();

    for (int i = 0; i < bind->get_argument_count(); i++) {
        // VariantType t=method->get_argument_type(i);

        minfo.arguments.emplace_back(eastl::move(bind->get_argument_info(i)));
    }

    minfo.return_val = bind->get_return_info();
    minfo.flags = bind->get_hint_flags();

    for (int i = 0; i < bind->get_argument_count(); i++) {
        if (bind->has_default_argument(i)) {
            minfo.default_arguments.push_back(bind->get_default_argument(i));
        }
    }
    return minfo;
}

void ClassDB::get_method_list(const StringName &p_class, Vector<MethodInfo> *p_methods, bool p_no_inheritance,
        bool p_exclude_from_properties) {
    RWLockRead _rw_lockr_(classdb_lock);

    const auto iter = classes.find(p_class);

    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    while (type) {
        if (type->disabled) {
            if (p_no_inheritance) {
                break;
            }

            type = type->inherits_ptr;
            continue;
        }

#ifdef DEBUG_METHODS_ENABLED
        p_methods->insert(p_methods->end(),type->virtual_methods.begin(),type->virtual_methods.end());

        for (const StringName &nm : type->method_order) {

            if (p_exclude_from_properties && type->methods_in_properties.contains(nm)) {
                continue;
            }

            MethodBind *method = type->method_map.at(nm);
            p_methods->push_back(info_from_bind(method));
        }

#else


        for (const auto &entry : type->method_map) {
            MethodBind *m = entry.second;
            p_methods->push_back(info_from_bind(entry.second));
        }

#endif

        if (p_no_inheritance) {
            break;
        }

        type = type->inherits_ptr;
    }
}

MethodBind *ClassDB::get_method(StringName p_class, StringName p_name) {
    RWLockRead _rw_lockr_(classdb_lock);

    auto iter = classes.find(p_class);

    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    while (type) {
        MethodBind *method = type->method_map.at(p_name, nullptr);
        if (method) {
            return method;
        }
        type = type->inherits_ptr;
    }
    return nullptr;
}

HashMap<StringName, MethodInfo> *ClassDB::get_signal_list(const StringName &p_class) {
    RWLockRead _rw_lockr_(classdb_lock);

    auto iter = classes.find(p_class);

    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    if (!type) {
        return nullptr;
    }
    return &type->signal_map;
}
void ClassDB::register_enum_type(
        const StringName &p_class, const StringName &p_enum, const StringName &p_underlying_type) {
    auto iter = classes.find(p_class);
    ERR_FAIL_COND(iter == classes.end());
    ClassDB_ClassInfo *type = &iter->second;

    if (type->enum_map.contains(p_enum)) {
        ERR_FAIL();
    }
    assert(!p_underlying_type.empty());
    type->enum_map[p_enum].underlying_type = p_underlying_type;
}

void ClassDB::bind_integer_constant(
        const StringName &p_class, const StringName &p_enum, const StringName &p_name, int p_constant) {
    OBJTYPE_WLOCK;

    auto iter = classes.find(p_class);

    ERR_FAIL_COND(iter == classes.end());

    ClassDB_ClassInfo *type = &iter->second;

    if (type->constant_map.contains(p_name)) {
        ERR_FAIL();
    }

    type->constant_map[p_name] = p_constant;

    StringView enum_name(p_enum);
    if (!p_enum.empty()) {
        if (StringUtils::contains(p_enum, '.')) {
            enum_name = StringUtils::get_slice(enum_name, '.', 1);
        }
        const StringName interned_enum_name(enum_name);

        ClassDB_EnumDescriptor &constants_list = type->enum_map[interned_enum_name];
        if (constants_list.underlying_type.empty()) {
            constants_list.underlying_type = "int32_t";
        }
        constants_list.enumerators.push_back(p_name);
    }

#ifdef DEBUG_METHODS_ENABLED
    type->constant_order.push_back(p_name);
#endif
}

void ClassDB::get_integer_constant_list(const StringName &p_class, Vector<String> *p_constants, bool p_no_inheritance) {
    RWLockRead _rw_lockr_(classdb_lock);

    auto iter = classes.find(p_class);

    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    while (type) {
#ifdef DEBUG_METHODS_ENABLED
        for (const StringName &name : type->constant_order) {
            p_constants->emplace_back(name.asCString());
        }
#else
        for (const auto &e : type->constant_map) {
            p_constants->emplace_back(e.first);
        }
#endif
        if (p_no_inheritance) {
            break;
        }

        type = type->inherits_ptr;
    }
}

int ClassDB::get_integer_constant(const StringName &p_class, const StringName &p_name, bool *p_success) {
    RWLockRead _rw_lockr_(classdb_lock);

    auto iter = classes.find(p_class);

    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    while (type) {
        auto iter = type->constant_map.find(p_name);
        if (iter != type->constant_map.end()) {
            if (p_success) {
                *p_success = true;
            }
            return iter->second;
        }

        type = type->inherits_ptr;
    }

    if (p_success) {
        *p_success = false;
    }

    return 0;
}

StringName ClassDB::get_integer_constant_enum(
        const StringName &p_class, const StringName &p_name, bool p_no_inheritance) {
    RWLockRead _rw_lockr_(classdb_lock);

    auto iter = classes.find(p_class);

    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    while (type) {
        for (const auto &entry : type->enum_map) {
            if (entry.second.enumerators.contains(p_name)) {
                return entry.first;
            }
        }

        if (p_no_inheritance) {
            break;
        }

        type = type->inherits_ptr;
    }

    return StringName();
}

//void ClassDB::get_enum_list(const StringName &p_class, Vector<StringName> *p_enums, bool p_no_inheritance) {
//    RWLockRead _rw_lockr_(lock);

//    auto iter = classes.find(p_class);

//    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
//    while (type) {
//        for (const auto &entry : type->enum_map) {
//            p_enums->emplace_back(entry.first);
//        }

//        if (p_no_inheritance) {
//            break;
//        }

//        type = type->inherits_ptr;
//    }
//}

//void ClassDB::get_enum_constants(
//        const StringName &p_class, const StringName &p_enum, List<StringName> *p_constants, bool p_no_inheritance) {
//    RWLockRead _rw_lockr_(lock);

//    auto iter = classes.find(p_class);

//    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
//    while (type) {
//        auto enum_iter = type->enum_map.find(p_enum);

//        if (enum_iter != type->enum_map.end()) {
//            for (const StringName &name : enum_iter->second.enumerators) {
//                p_constants->push_back(name);
//            }
//        }

//        if (p_no_inheritance) {
//            break;
//        }

//        type = type->inherits_ptr;
//    }
//}

void ClassDB::add_signal(StringName p_class, MethodInfo &&p_signal) {
    OBJTYPE_WLOCK;

    auto iter = classes.find(p_class);
    ERR_FAIL_COND(iter == classes.end());
    ClassDB_ClassInfo *type = &iter->second;

    const StringName &sname = p_signal.name;

#ifdef DEBUG_METHODS_ENABLED
    ClassDB_ClassInfo *check = type;
    while (check) {
        ERR_FAIL_COND_MSG(class_signal_map(*check).contains(sname),
                "Class '" + String(p_class) + "' already has signal '" + StringView(sname) + "'.");
        check = check->inherits_ptr;
    }
#endif
    type->class_signal_map()[sname] = eastl::move(p_signal);
}

void ClassDB::get_signal_list(StringName p_class, Vector<MethodInfo> *p_signals, bool p_no_inheritance) {
    RWLockRead _rw_lockr_(classdb_lock);

    auto iter = classes.find(p_class);
    ERR_FAIL_COND(iter == classes.end());
    ClassDB_ClassInfo *type = &iter->second;

    ClassDB_ClassInfo *check = type;

    while (check) {
        class_signal_map(*check).keys_into(*p_signals);

        if (p_no_inheritance) {
            return;
        }

        check = check->inherits_ptr;
    }
}

bool ClassDB::has_signal(StringName p_class, StringName p_signal) {
    RWLockRead _rw_lockr_(classdb_lock);
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        if (class_signal_map(*check).contains(p_signal)) {
            return true;
        }
        check = check->inherits_ptr;
    }

    return false;
}

bool ClassDB::get_signal(StringName p_class, StringName p_signal, MethodInfo *r_signal) {
    RWLockRead _rw_lockr_(classdb_lock);
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        if (class_signal_map(*check).contains(p_signal)) {
            if (r_signal) {
                *r_signal = class_signal_map(*check)[p_signal];
            }
            return true;
        }
        check = check->inherits_ptr;
    }

    return false;
}

void ClassDB::add_property_group(StringName p_class, const char *p_name, const char *p_prefix) {
    OBJTYPE_WLOCK
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ERR_FAIL_COND(!type);

    type->property_list.emplace_back(
            VariantType::NIL, StringName(p_name), PropertyHint::None, p_prefix, PROPERTY_USAGE_GROUP);
}

void ClassDB::add_property_array(StringName p_class, const char *p_name, int elem_count, const char *p_prefix) {
    OBJTYPE_WLOCK
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ERR_FAIL_COND(!type);

    type->property_list.emplace_back(StringName(p_name), elem_count, StringName(p_prefix));
}

void ClassDB::add_property(StringName p_class, const PropertyInfo &p_pinfo, const StringName &p_setter,
        const StringName &p_getter, int p_index) {
    classdb_lock.read_lock();
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    classdb_lock.read_unlock();

    ERR_FAIL_COND(!type);

    MethodBind *mb_set = nullptr;
    if (p_setter) {
        mb_set = get_method(p_class, p_setter);
#ifdef DEBUG_METHODS_ENABLED

        ERR_FAIL_COND_MSG(!mb_set,
                String("Invalid setter '") + p_class + "::" + p_setter + "' for property '" + p_pinfo.name + "'.");

        int exp_args = 1 + (p_index >= 0 ? 1 : 0);
        ERR_FAIL_COND_MSG(mb_set->get_argument_count() != exp_args, String("Invalid function for setter '") + p_class +
                                                                            "::" + p_setter + " for property '" +
                                                                            p_pinfo.name + "'.");
#endif
    }

    MethodBind *mb_get = nullptr;
    if (p_getter) {
        mb_get = get_method(p_class, p_getter);
#ifdef DEBUG_METHODS_ENABLED

        ERR_FAIL_COND_MSG(!mb_get,
                String("Invalid getter '") + p_class + "::" + p_getter + "' for property '" + p_pinfo.name + "'.");

        int exp_args = 0 + (p_index >= 0 ? 1 : 0);
        ERR_FAIL_COND_MSG(mb_get->get_argument_count() != exp_args, String("Invalid function for getter '") + p_class +
                                                                            "::" + p_getter + "' for property: '" +
                                                                            p_pinfo.name + "'.");
#endif
    }

#ifdef DEBUG_METHODS_ENABLED
    ERR_FAIL_COND_MSG(type->property_setget.contains(p_pinfo.name),
            String("Object '") + p_class + "' already has property '" + p_pinfo.name + "'.");
#endif

    OBJTYPE_WLOCK

    type->property_list.push_back(p_pinfo);
#ifdef DEBUG_METHODS_ENABLED
    if (mb_get) {
        type->methods_in_properties.insert(p_getter);
    }
    if (mb_set) {
        type->methods_in_properties.insert(p_setter);
    }
#endif
    ClassDB_PropertySetGet psg;
    psg.setter = p_setter;
    psg.getter = p_getter;
    psg._setptr = mb_set;
    psg._getptr = mb_get;
    psg.index = p_index;
    psg.type = p_pinfo.type;

    type->property_setget[p_pinfo.name] = psg;
}

void ClassDB::set_property_default_value(StringName p_class, const StringName &p_name, const Variant &p_default) {
    default_values[p_class][p_name] = p_default;
}

void ClassDB::get_property_list(
        StringName p_class, Vector<PropertyInfo> *p_list, bool p_no_inheritance, const Object *p_validator) {
    RWLockRead _rw_lockr_(classdb_lock);

    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        for (const PropertyInfo &pi : check->property_list) {
            if (p_validator) {
                PropertyInfo pimod = pi;
                p_validator->_validate_property(pimod);
                p_list->push_back(pimod);
            } else {
                p_list->push_back(pi);
            }
        }

        if (p_no_inheritance) {
            return;
        }
        check = check->inherits_ptr;
    }
}
bool ClassDB::set_property(Object *p_object, const StringName &p_property, const Variant &p_value, bool *r_valid) {
    ERR_FAIL_NULL_V(p_object, false);
    auto iter = classes.find(p_object->get_class_name());
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        auto iter = check->property_setget.find(p_property);
        if (iter != check->property_setget.end()) {
            const ClassDB_PropertySetGet &psg(iter->second);
            if (!psg.setter) {
                if (r_valid) {
                    *r_valid = false;
                }
                return true; // return true but do nothing
            }

            Callable::CallError ce;

            if (psg.index >= 0) {
                Variant index = psg.index;
                const Variant *arg[2] = { &index, &p_value };
                // p_object->call(psg.setter,arg,2,ce);
                if (psg._setptr) {
                    psg._setptr->call(p_object, arg, 2, ce);
                } else {
                    p_object->call(psg.setter, arg, 2, ce);
                }

            } else {
                const Variant *arg[1] = { &p_value };
                if (psg._setptr) {
                    psg._setptr->call(p_object, arg, 1, ce);
                } else {
                    p_object->call(psg.setter, arg, 1, ce);
                }
            }

            if (r_valid) {
                *r_valid = ce.error == Callable::CallError::CALL_OK;
            }

            return true;
        }

        check = check->inherits_ptr;
    }

    return false;
}
bool ClassDB::get_property(Object *p_object, const StringName &p_property, Variant &r_value) {
    ERR_FAIL_NULL_V(p_object, false);
    auto iter = classes.find(p_object->get_class_name());
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        auto iter2 = check->property_setget.find(p_property);
        if (iter2 != check->property_setget.end()) {
            const ClassDB_PropertySetGet &psg(iter2->second);
            if (!psg.getter) {
                return true; // return true but do nothing
            }

            if (psg.index >= 0) {
                Variant index = psg.index;
                const Variant *arg[1] = { &index };
                Callable::CallError ce;
                r_value = p_object->call(psg.getter, arg, 1, ce);

            } else {
                Callable::CallError ce;
                if (psg._getptr) {
                    r_value = psg._getptr->call(p_object, nullptr, 0, ce);
                } else {
                    r_value = p_object->call(psg.getter, nullptr, 0, ce);
                }
            }
            return true;
        }
        auto iter = check->constant_map.find(p_property);
        if (iter != check->constant_map.end()) {
            r_value = iter->second;
            return true;
        }

        check = check->inherits_ptr;
    }

    return false;
}

int ClassDB::get_property_index(const StringName &p_class, const StringName &p_property, bool *r_is_valid) {
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        auto iter2 = check->property_setget.find(p_property);
        if (iter2 != check->property_setget.end()) {
            const ClassDB_PropertySetGet &psg(iter2->second);
            if (r_is_valid) {
                *r_is_valid = true;
            }

            return psg.index;
        }

        check = check->inherits_ptr;
    }
    if (r_is_valid) {
        *r_is_valid = false;
    }

    return -1;
}

VariantType ClassDB::get_property_type(const StringName &p_class, const StringName &p_property, bool *r_is_valid) {
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        auto iter2 = check->property_setget.find(p_property);
        if (iter2 != check->property_setget.end()) {
            const ClassDB_PropertySetGet &psg(iter2->second);

            if (r_is_valid) {
                *r_is_valid = true;
            }

            return psg.type;
        }

        check = check->inherits_ptr;
    }
    if (r_is_valid) {
        *r_is_valid = false;
    }

    return VariantType::NIL;
}

StringName ClassDB::get_property_setter(StringName p_class, const StringName &p_property) {
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        auto iter2 = check->property_setget.find(p_property);
        if (iter2 != check->property_setget.end()) {
            const ClassDB_PropertySetGet &psg(iter2->second);

            return psg.setter;
        }

        check = check->inherits_ptr;
    }

    return StringName();
}

StringName ClassDB::get_property_getter(StringName p_class, const StringName &p_property) {
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        auto iter2 = check->property_setget.find(p_property);
        if (iter2 != check->property_setget.end()) {
            const ClassDB_PropertySetGet &psg(iter2->second);
            return psg.getter;
        }

        check = check->inherits_ptr;
    }

    return StringName();
}

bool ClassDB::has_property(const StringName &p_class, const StringName &p_property, bool p_no_inheritance) {
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        if (check->property_setget.contains(p_property)) {
            return true;
        }

        if (p_no_inheritance) {
            break;
        }
        check = check->inherits_ptr;
    }

    return false;
}

void ClassDB::set_method_flags(StringName p_class, StringName p_method, int p_flags) {
    OBJTYPE_WLOCK;
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    ERR_FAIL_COND(!check);
    ERR_FAIL_COND(!check->method_map.contains(p_method));
    check->method_map[p_method]->set_hint_flags(p_flags);
}

bool ClassDB::has_method(StringName p_class, StringName p_method, bool p_no_inheritance) {
    auto iter = classes.find(p_class);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    ClassDB_ClassInfo *check = type;
    while (check) {
        if (check->method_map.contains(p_method)) {
            return true;
        }
        if (p_no_inheritance) {
            return false;
        }
        check = check->inherits_ptr;
    }

    return false;
}

#ifdef DEBUG_METHODS_ENABLED
MethodBind *ClassDB::bind_methodfi(uint32_t p_flags, MethodBind *p_bind, const MethodDefinition &method_name,
        std::initializer_list<Variant> def_vals) {
    StringName mdname = method_name.name;
#else
MethodBind *ClassDB::bind_methodfi(
        uint32_t p_flags, MethodBind *p_bind, const char *method_name, std::initializer_list<Variant> def_vals) {
    StringName mdname = StaticCString(method_name, true);
#endif

    OBJTYPE_WLOCK;
    ERR_FAIL_COND_V(!p_bind, nullptr);
    p_bind->set_name(mdname);

    const char *instance_type = p_bind->get_instance_class();

#ifdef DEBUG_ENABLED

    ERR_FAIL_COND_V_MSG(has_method(StringName(instance_type), mdname), nullptr,
            "Class " + String(instance_type) + " already has a method " + String(mdname) + ".");
#endif

    auto iter = classes.find_as(instance_type);
    ClassDB_ClassInfo *type = iter != classes.end() ? &iter->second : nullptr;
    if (!type) {
        memdelete(p_bind);
        ERR_FAIL_V_MSG(nullptr, String("Couldn't bind method '") + mdname + "' for instance '" + instance_type + "'.");
    }

    if (type->method_map.contains(mdname)) {
        memdelete(p_bind);
        // overloading not supported
        ERR_FAIL_V_MSG(nullptr, String("Method already bound '") + instance_type + "::" + mdname + "'.");
    }

#ifdef DEBUG_METHODS_ENABLED

    if (method_name.parameterCount() > p_bind->get_argument_count()) {
        memdelete(p_bind);
        ERR_FAIL_V_MSG(nullptr, String("Method definition provides more arguments than the method actually has '") +
                                        instance_type + "::" + mdname + "'.");
    }

    // p_bind->set_argument_names(method_name.args);

    type->method_order.push_back(mdname);
#endif

    type->method_map[mdname] = p_bind;

    Vector<Variant> defvals;

    defvals.reserve(def_vals.size());
    for (size_t i = 0; i < def_vals.size(); i++) {
        defvals.emplace_back(def_vals.begin()[def_vals.size() - i - 1]);
    }

    p_bind->set_default_arguments(defvals);
    p_bind->set_hint_flags(p_flags);
    return p_bind;
}
#ifdef DEBUG_METHODS_ENABLED
void ClassDB::_set_class_header(const StringName &p_class, StringView header_file) {
    // 2 ways this functions is called:
    //  - during engine compilation -> all paths are under same prefix.
    //  - during external exe/plugin compilation -> paths are 'rooted' under the specific project
    constexpr StringView current_path = __FILE__;
    const int prefix_len = current_path.length() - strlen("core/class_db.cpp");
    const String hdr_path = PathUtils::from_native_path(header_file).replaced(".cpp", ".h");
    if(header_file.starts_with(hdr_path.substr(0,prefix_len))) {
        // chop the prefix, the bindings are compiled with correct include paths.
        classes[p_class].usage_header = String(hdr_path.substr(prefix_len));
    }
    else {
        classes[p_class].usage_header = hdr_path;
    }
}
#endif

void ClassDB::add_virtual_method(const StringName &p_class, const MethodInfo &p_method) { //, bool p_virtual
    ERR_FAIL_COND(!classes.contains(p_class));

#ifdef DEBUG_METHODS_ENABLED
    OBJTYPE_WLOCK

    MethodInfo mi = p_method;
        mi.flags |= METHOD_FLAG_VIRTUAL;
    classes[p_class].virtual_methods.emplace_back(mi);

#endif
}

void ClassDB::get_virtual_methods(const StringName &p_class, Vector<MethodInfo> *p_methods) {
    ERR_FAIL_COND(!classes.contains(p_class));

#ifdef DEBUG_METHODS_ENABLED

    const auto iter = classes.find(p_class);
    if(iter == classes.end()) {
            return;
        }
    for (const MethodInfo &mi : iter->second.virtual_methods) {
        p_methods->push_back(mi);
    }

#endif
}

void ClassDB::set_class_enabled(StringName p_class, bool p_enable) {
    OBJTYPE_WLOCK;

    ERR_FAIL_COND(!classes.contains(p_class));
    classes[p_class].disabled = !p_enable;
}

bool ClassDB::is_class_enabled(StringName p_class) {
    RWLockRead _rw_lockr_(classdb_lock);

    auto iter = classes.find(p_class);
    if (iter == classes.end() || !iter->second.creation_func) {
        if (compat_classes.contains(p_class)) {
            iter = classes.find(compat_classes[p_class]);
        }
    }

    ERR_FAIL_COND_V_MSG(iter == classes.end(), false, "Cannot get class '" + String(p_class) + "'.");
    return !iter->second.disabled;
}

bool ClassDB::is_class_exposed(StringName p_class) {
    RWLockRead _rw_lockr_(classdb_lock);

    const auto iter = classes.find(p_class);
    ERR_FAIL_COND_V_MSG(iter == classes.end(), false, "Cannot get class '" + String(p_class) + "'.");
    return iter->second.exposed;
}

StringName ClassDB::get_category(const StringName &p_node) {
    ERR_FAIL_COND_V(!classes.contains(p_node), StringName());
#ifdef DEBUG_ENABLED
    return classes[p_node].category;
#else
    return StringName();
#endif
}

void ClassDB::add_resource_base_extension(const StringName &p_extension, const StringName &p_class) {
    if (resource_base_extensions.contains(p_extension)) {
        return;
    }

    resource_base_extensions[p_extension] = p_class;
}

void ClassDB::get_resource_base_extensions(Vector<String> &p_extensions) {
    for (const auto &p : resource_base_extensions) {
        p_extensions.emplace_back(p.first.asCString());
    }
}

void ClassDB::get_extensions_for_type(const StringName &p_class, Vector<String> *p_extensions) {
    for (const auto &p : resource_base_extensions) {
        if (is_parent_class(p_class, p.second) || is_parent_class(p.second, p_class)) {
            p_extensions->emplace_back(p.first);
        }
    }
}

HashMap<StringName, HashMap<StringName, Variant>> ClassDB::default_values;
HashSet<StringName> ClassDB::default_values_cached;

Variant ClassDB::class_get_default_property_value(
        const StringName &p_class, const StringName &p_property, bool *r_valid) {
    if (!default_values_cached.contains(p_class)) {
        Object *c = nullptr;
        bool cleanup_c = false;

        if (Engine::get_singleton()->has_singleton(p_class)) {
            c = Engine::get_singleton()->get_named_singleton(p_class);
            cleanup_c = false;
        } else if (ClassDB::can_instance(p_class)) {
            c = ClassDB::instance(p_class);
            cleanup_c = true;
        }

        if (c) {
            Vector<PropertyInfo> plist;
            c->get_property_list(&plist);
            for (const PropertyInfo &pi : plist) {
                if (pi.usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR)) {
                    if (!default_values[p_class].contains(pi.name)) {
                        Variant v = c->get(pi.name);
                        default_values[p_class][pi.name] = v;
                    }
                }
            }

            if (cleanup_c) {
                memdelete(c);
            }
        }

        default_values_cached.insert(p_class);
    }

    if (!default_values.contains(p_class)) {
        if (r_valid != nullptr) {
            *r_valid = false;
        }
        return Variant();
    }

    if (!default_values[p_class].contains(p_property)) {
        if (r_valid != nullptr) {
            *r_valid = false;
        }
        return Variant();
    }

    if (r_valid != nullptr) {
        *r_valid = true;
    }
    return default_values[p_class][p_property];
}

void ClassDB::cleanup_defaults() {
    default_values.clear();
    default_values_cached.clear();
}

void ClassDB::cleanup() {
    // OBJTYPE_LOCK; hah not here
    classes.clear();
    resource_base_extensions.clear();
    compat_classes.clear();
}

//
bool ClassDB::can_bind(const StringName &classname, const StringName &p_name) {
    auto iter = classes.find(classname);
    if(iter == classes.end()) {
        return false;
    }

    auto type = &iter->second;
    // overloading not supported
    return !type->method_map.contains(p_name);
}
bool ClassDB::bind_helper(MethodBind *bind, const StringName &p_name) {
    bool can_bind_method = can_bind(StaticCString(bind->get_instance_class(), true),p_name);
    if (!can_bind_method) {
        memdelete(bind);
        ERR_FAIL_V_MSG(false,"can_bind_method==false");
    }
    auto iter = classes.find(StaticCString(bind->get_instance_class(), true));
    auto type = &iter->second;
    type->method_map[p_name] = bind;
#ifdef DEBUG_METHODS_ENABLED
    type->method_order.push_back(p_name);
#endif
    return true;
}
