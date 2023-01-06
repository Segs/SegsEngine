/*************************************************************************/
/*  object.cpp                                                           */
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

#include "object.h"

#include "object_private.h"
#include "core/class_db.h"
#include "core/core_string_names.h"
#include "core/hash_map.h"
#include "core/message_queue.h"
#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/node_path.h"
#include "core/object_db.h"
#include "core/object_rc.h"
#include "core/object_tooling.h"
#include "core/os/os.h"
#include "core/pool_vector.h"
#include "core/print_string.h"
#include "core/resource.h"
#include "core/script_language.h"
#include "core/string.h"
#include "core/string_formatter.h"
#include "core/translation.h"
#include "core/jlsignal/Signal.h"
#include "core/ecs_registry.h"
#include "EASTL/sort.h"

#include <EASTL/vector_map.h>
#include "entt/entity/registry.hpp"
#include "entt/meta/meta.hpp"
#include "entt/meta/factory.hpp"
#include "entt/meta/resolve.hpp"

ECS_Registry<GameEntity,true> game_object_registry;

// Not static to allow 'local' usage from specific cpp files.

#ifdef DEBUG_ENABLED

struct _ObjectDebugLock {

    Object *obj;

    explicit _ObjectDebugLock(Object *p_obj) {
        obj = p_obj;
        obj->private_data->_lock_index.ref();
    }
    ~_ObjectDebugLock() {
        obj->private_data->_lock_index.unref();
    }
};

#define OBJ_DEBUG_LOCK _ObjectDebugLock _debug_lock(this);

#else

#define OBJ_DEBUG_LOCK

#endif

PropertyInfo::operator Dictionary() const {

    Dictionary d;
    d["name"] = name;
    d["class_name"] = class_name;
    d["type"] = int(type);
    d["hint"] = hint;
    d["hint_string"] = hint_string;
    d["usage"] = usage;
    return d;
}

PropertyInfo PropertyInfo::from_dict(const Dictionary &p_dict) {

    PropertyInfo pi;

    if (p_dict.has("type")) {
        pi.type = p_dict["type"].as<VariantType>();
    }

    if (p_dict.has("name")) {
        pi.name = p_dict["name"].as<StringName>();
    }

    if (p_dict.has("class_name")) {
        pi.class_name = p_dict["class_name"].as<StringName>();
    }

    if (p_dict.has("hint")) {
        pi.hint = p_dict["hint"].as<PropertyHint>();
    }

    if (p_dict.has("hint_string")) {
        pi.hint_string = p_dict["hint_string"].as<String>();
    }

    if (p_dict.has("usage")) {
        pi.usage = p_dict["usage"].as<uint32_t>();
    }

    return pi;
}

Array convert_property_list(const Vector<PropertyInfo> *p_list) {

    Array va;
    for (const PropertyInfo & pi : *p_list) {

        va.push_back(Dictionary(pi));
    }

    return va;
}

Array convert_property_vector(Span<const PropertyInfo> p_list) {

    Array va;
    va.resize(p_list.size());
    int idx=0;
    for (const PropertyInfo &E : p_list) {

        va[idx++] = eastl::move(Dictionary(E));
    }

    return va;
}

MethodInfo::operator Dictionary() const {

    Dictionary d;
    d["name"] = name;
    d["args"] = convert_property_vector(arguments);
    Array da;
    for (const Variant &varg : default_arguments) {
        da.push_back(varg);
    }
    d["default_args"] = da;
    d["flags"] = flags;
    d["id"] = id;
    d["return"] = (Dictionary)return_val;
    return d;
}

MethodInfo MethodInfo::from_dict(const Dictionary &p_dict) {

    MethodInfo mi;

    if (p_dict.has("name"))
        mi.name = p_dict["name"].as<StringName>();
    Array args;
    if (p_dict.has("args")) {
        args = p_dict["args"].as<Array>();
    }

    for (int i = 0; i < args.size(); i++) {
        Dictionary d {args[i].as<Dictionary>()};
        mi.arguments.emplace_back(eastl::move(PropertyInfo::from_dict(d)));
    }
    Array defargs;
    if (p_dict.has("default_args")) {
        defargs = p_dict["default_args"].as<Array>();
    }
    for (int i = 0; i < defargs.size(); i++) {
        mi.default_arguments.push_back(defargs[i]);
    }

    if (p_dict.has("return")) {
        mi.return_val = PropertyInfo::from_dict(p_dict["return"].as<Dictionary>());
    }

    if (p_dict.has("flags"))
        mi.flags = p_dict["flags"].as<uint32_t>();

    return mi;
}

Object::Connection::operator Variant() const {

    //TODO: SEGS: note that this WILL NOT PRESERVE source and target if they are RefCounted types!
    Dictionary d;
    d["signal"] = (Variant)signal;
    d["callable"] = (Variant)callable;
    d["flags"] = flags;
    return d;
}

Object::Connection::Connection(const Variant &p_variant) {

    Dictionary d = p_variant.as<Dictionary>();
    if (d.has("signal")) {
        signal = (Signal)d["signal"];
    }
    if (d.has("callable")) {
        callable = (Callable)d["callable"];
    }
    if (d.has("flags")) {
        flags = (uint32_t)d["flags"];
    }
}

void Object::_predelete() {
    notification(NOTIFICATION_PREDELETE, true);
    _class_ptr = nullptr; //must restore so destructors can access class ptr correctly
}

void Object::_postinitialize() {
    _class_ptr = _get_class_namev();
    bool initialized = _initialize_classv();
    assert(!initialized); // we want all classes to be initialized before this.
    notification(NOTIFICATION_POSTINITIALIZE);
}

String Object::wrap_get_class() const {
    return String(get_class());
}

bool Object::wrap_is_class(StringView p_class) const {
    return is_class(p_class);
}

void Object::set(const StringName &p_name, const Variant &p_value, bool *r_valid) {

    Object_set_edited(this,true,false);

    if (script_instance) {

        if (script_instance->set(p_name, p_value)) {
            if (r_valid)
                *r_valid = true;
            return;
        }
    }

    //try built-in setgetter
    {
        if (ClassDB::set_property(this, p_name, p_value, r_valid)) {
            /*
            if (r_valid)
                *r_valid=true;
            */
            return;
        }
    }

    if (p_name == CoreStringNames::get_singleton()->_script) {
        set_script(p_value.as<RefPtr>());
        if (r_valid)
            *r_valid = true;
        return;
    }

    if (p_name == CoreStringNames::get_singleton()->_meta) {
        //set_meta(p_name,p_value);
        auto d = p_value.duplicate_t<Dictionary>();
        if(metadata) {
            if(d.empty()) {
                memdelete(metadata);
                metadata = nullptr;
            }
            else
                *metadata = eastl::move(d);
        }
        else if (!d.empty()) {
            metadata = memnew(Dictionary(eastl::move(d)));
        }
        if (r_valid)
            *r_valid = true;
        return;
    }

    //something inside the object... :|
    bool success = _setv(p_name, p_value);
    if (success) {
        if (r_valid) {
            *r_valid = true;
        }
        return;
    }

    {
        bool valid;
        setvar(p_name, p_value, &valid);
        if (valid) {
            if (r_valid) {
                *r_valid = true;
            }
            return;
        }
    }
    bool res = Object_set_fallback(this,p_name,p_value);
    if (r_valid)
        *r_valid = res;
}

Variant Object::get(const StringName &p_name, bool *r_valid) const {

    Variant ret;

    if (script_instance) {

        if (script_instance->get(p_name, ret)) {
            if (r_valid) {
                *r_valid = true;
            }
            return ret;
        }
    }

    //try built-in setgetter
    {
        if (ClassDB::get_property(const_cast<Object *>(this), p_name, ret)) {
            if (r_valid) {
                *r_valid = true;
            }
            return ret;
        }
    }

    if (p_name == CoreStringNames::get_singleton()->_script) {
        ret = Variant::from(get_script());
        if (r_valid) {
            *r_valid = true;
        }
        return ret;
    } else if (p_name == CoreStringNames::get_singleton()->_meta) {
        ret = metadata ? *metadata : Variant();
        if (r_valid) {
            *r_valid = true;
        }
        return ret;
    } else {
        //something inside the object... :|
        bool success = _getv(p_name, ret);
        if (success) {
            if (r_valid) {
                *r_valid = true;
            }
            return ret;
        }

        //if nothing else, use getvar
        {
            bool valid;
            ret = getvar(p_name, &valid);
            if (valid) {
                if (r_valid) {
                    *r_valid = true;
                }
                return ret;
            }
        }
        bool valid=false;
        ret = Object_get_fallback(this,p_name,valid);
        if (r_valid) {
            *r_valid = valid;
        }
    }
    return ret;
}

void Object::set_indexed(const Vector<StringName> &p_names, const Variant &p_value, bool *r_valid) {
    if (p_names.empty()) {
        if (r_valid) {
            *r_valid = false;
        }
        return;
    }
    if (p_names.size() == 1) {
        set(p_names[0], p_value, r_valid);
        return;
    }

    bool valid = false;
    if (!r_valid) {
        r_valid = &valid;
    }

    FixedVector<Variant,8,true> value_stack;

    value_stack.push_back(get(p_names[0], r_valid));

    if (!*r_valid) {
        value_stack.clear();
        return;
    }

    for (size_t i = 1; i < p_names.size() - 1; i++) {
        value_stack.emplace_back(value_stack.back().get_named(p_names[i], r_valid));

        if (!*r_valid) {
            value_stack.clear();
            return;
        }
    }

    value_stack.emplace_back(p_value); // p_names[p_names.size() - 1]

    for (int i = p_names.size() - 1; i > 0; i--) {
        Variant back = value_stack.back();
        value_stack.pop_back();
        value_stack.back().set_named(p_names[i], back, r_valid);

        if (!*r_valid) {
            value_stack.clear();
            return;
        }
    }

    set(p_names[0], value_stack.back(), r_valid);
    value_stack.pop_back();

    ERR_FAIL_COND(!value_stack.empty());
}

Variant Object::get_indexed(const Vector<StringName> &p_names, bool *r_valid) const {
    if (p_names.empty()) {
        if (r_valid) {
            *r_valid = false;
        }
        return Variant();
    }
    bool valid = false;

    Variant current_value = get(p_names[0], &valid);
    for (size_t i = 1; i < p_names.size(); i++) {
        current_value = current_value.get_named(p_names[i], &valid);

        if (!valid) {
            break;
        }
    }
    if (r_valid) {
        *r_valid = valid;
    }

    return current_value;
}

void Object::get_property_list(Vector<PropertyInfo> *p_list, bool p_reversed) const {

    if (script_instance && p_reversed) {
        p_list->push_back(PropertyInfo(VariantType::NIL, "Script Variables", PropertyHint::None, nullptr, PROPERTY_USAGE_CATEGORY));
        script_instance->get_property_list(p_list);
    }

    _get_property_listv(p_list, p_reversed);

    if (!is_class("Script")) { // can still be set, but this is for userfriendliness
        p_list->push_back(PropertyInfo(VariantType::OBJECT, "script", PropertyHint::ResourceType, "Script", PROPERTY_USAGE_DEFAULT));
    }
    if (metadata && !metadata->empty()) {
        p_list->push_back(PropertyInfo(VariantType::DICTIONARY, "__meta__", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
    }
    if (script_instance && !p_reversed) {
        p_list->push_back(PropertyInfo(VariantType::NIL, "Script Variables", PropertyHint::None, nullptr, PROPERTY_USAGE_CATEGORY));
        script_instance->get_property_list(p_list);
    }
}

void Object::get_method_list(Vector<MethodInfo> *p_list) const {

    ClassDB::get_method_list(get_class_name(), p_list);
    if (script_instance) {
        script_instance->get_method_list(p_list);
    }
}

Variant Object::_call_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    if (p_argcount < 1) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
        r_error.argument = 0;
        return Variant();
    }

    if (p_args[0]->get_type() != VariantType::STRING_NAME && p_args[0]->get_type() != VariantType::STRING) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
        r_error.argument = 0;
        r_error.expected = VariantType::STRING_NAME;
        return Variant();
    }

    StringName method = p_args[0]->as<StringName>();

    return call(method, &p_args[1], p_argcount - 1, r_error);
}

Variant Object::_call_deferred_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    if (p_argcount < 1) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
        r_error.argument = 0;
        return Variant();
    }

    if (p_args[0]->get_type() != VariantType::STRING_NAME && p_args[0]->get_type() != VariantType::STRING) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
        r_error.argument = 0;
        r_error.expected = VariantType::STRING_NAME;
        return Variant();
    }

    r_error.error = Callable::CallError::CALL_OK;

    StringName method = p_args[0]->as<StringName>();

    MessageQueue::get_singleton()->push_call(get_instance_id(), method, &p_args[1], p_argcount - 1, true);

    return Variant();
}

bool Object::has_method(const StringName &p_method) const {

    if (p_method == CoreStringNames::get_singleton()->_free) {
        return true;
    }

    if (script_instance && script_instance->has_method(p_method)) {
        return true;
    }

    MethodBind *method = ClassDB::get_method(get_class_name(), p_method);

    return method != nullptr;
}

Variant Object::getvar(const Variant &/*p_key*/, bool *r_valid) const {

    if (r_valid) {
        *r_valid = false;
    }
    return Variant();
}
void Object::setvar(const Variant &/*p_key*/, const Variant &/*p_value*/, bool *r_valid) {

    if (r_valid)
        *r_valid = false;
}

Variant Object::callv(const StringName &p_method, const Array &p_args) {
    const Variant **argptrs = nullptr;
    int argc=p_args.size();
    if (argc > 0) {
        argptrs = (const Variant **)alloca(sizeof(Variant *) * argc);
        for (int i = 0; i < argc; i++) {
            argptrs[i] = &p_args[i];
        }
    }

    Callable::CallError ce;
    Variant ret = call(p_method, argptrs, argc, ce);
    if (ce.error != Callable::CallError::CALL_OK) {
        ERR_FAIL_V_MSG(Variant(), "Error calling method from 'callv': " + Variant::get_call_error_text(this, p_method, argptrs, argc, ce) + ".");
    }
    return ret;
}

Variant Object::call_va(const StringName &p_name, VARIANT_ARG_DECLARE) {
    VARIANT_ARGPTRS

    int argc = 0;
    for (const Variant * i : argptr) {
        if (i->get_type() == VariantType::NIL) {
            break;
        }
        argc++;
    }

    Callable::CallError error;

    Variant ret = call(p_name, argptr, argc, error);
    return ret;
}
bool Object::free()
{
#ifdef DEBUG_ENABLED
    if (object_cast<RefCounted>(this)) {
        ERR_FAIL_V_MSG(false, "Can't 'free' a reference.");
    }

    if (private_data->_lock_index.get() > 1) {
        ERR_FAIL_V_MSG(false, "Object is locked and can't be freed.");
    }

#endif
    //must be here, must be before everything,
    memdelete(this);
    return true;

}
Variant Object::call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    r_error.error = Callable::CallError::CALL_OK;

    if (p_method == CoreStringNames::get_singleton()->_free) {
#ifdef DEBUG_ENABLED
        if (p_argcount != 0) {
            r_error.argument = 0;
            r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
            return Variant();
        }
#endif
        // free must be here, before anything, always ready
        if (!this->free()) {
            r_error.argument = 0;
            r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
            return Variant();
        }

        return Variant();
    }

    Variant ret;
    OBJ_DEBUG_LOCK
    if (script_instance) {
        ret = script_instance->call(p_method, p_args, p_argcount, r_error);
        //force jumptable
        switch (r_error.error) {

            case Callable::CallError::CALL_OK:
                return ret;
            case Callable::CallError::CALL_ERROR_INVALID_METHOD:
                break;
            case Callable::CallError::CALL_ERROR_INVALID_ARGUMENT:
            case Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS:
            case Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS:
                return ret;
            case Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL: {
            }
        }
    }

    MethodBind *method = ClassDB::get_method(get_class_name(), p_method);

    if (method) {

        ret = method->call(this, p_args, p_argcount, r_error);
    } else {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
    }

    return ret;
}

void Object::notification(int p_notification, bool p_reversed) {

    _notificationv(p_notification, p_reversed);

    if (script_instance) {
        script_instance->notification(p_notification);
    }
}

String Object::to_string() {
    if (script_instance) {
        bool valid;
        String ret = script_instance->to_string(&valid);
        if (valid) {
            return ret;
        }
    }
    return FormatVE("[%s:%zd]",get_class(),(uint64_t)entt::to_integral(get_instance_id()));
}

void Object::_changed_callback(Object * /*p_changed*/, const StringName & /*p_prop*/) {
}

void Object::property_list_changed_notify() {

    Object_change_notify(this);
}

jl::SignalObserver &Object::observer() {
    if(!observer_endpoint)
        observer_endpoint = memnew(jl::SignalObserver);
    return *observer_endpoint;
}

ObjectRC *Object::_use_rc() {

    // The RC object is lazily created the first time it's requested;
    // that way, there's no need to allocate and release it at all if this Object
    // is not being referred by any Variant at all.

    // Although when dealing with Objects from multiple threads some locking
    // mechanism should be used, this at least makes safe the case of first
    // assignment.

    ObjectRC *rc = nullptr;
    ObjectRC *const creating = reinterpret_cast<ObjectRC *>(1);
    if (unlikely(_rc.compare_exchange_strong(rc, creating, std::memory_order_acq_rel))) {
        // Not created yet
        rc = memnew(ObjectRC(this,this->get_instance_id()));
        _rc.store(rc, std::memory_order_release);
        return rc;
    }

    // Spin-wait until we know it's created (or just return if it's already created)
    for (;;) {
        if (likely(rc != creating)) {
            rc->increment();
            return rc;
        }
        rc = _rc.load(std::memory_order_acquire);
    }
}

//! @note some script languages can't control instance creation, so this function eases the process
void Object::set_script_and_instance(const RefPtr &p_script, ScriptInstance *p_instance) {

    //this function is not meant to be used in any of these ways
    ERR_FAIL_COND(p_script.is_null());
    ERR_FAIL_COND(!p_instance);
    ERR_FAIL_COND(script_instance != nullptr || !script.is_null());

    script = p_script;
    script_instance = p_instance;
}

void Object::set_script(const RefPtr &p_script) {

    if (script == p_script)
        return;

    memdelete(script_instance);
    script_instance = nullptr;

    script = p_script;
    Ref s(refFromRefPtr<Script>(script));

    if (s) {
        if (s->can_instance()) {
            OBJ_DEBUG_LOCK
            script_instance = s->instance_create(this);
        } else if (Engine::get_singleton()->is_editor_hint()) {
            OBJ_DEBUG_LOCK
            script_instance = s->placeholder_instance_create(this);
        }
    }

    Object_change_notify(this); //scripts may add variables, so refresh is desired
    emit_signal(CoreStringNames::get_singleton()->script_changed);
}

void Object::set_script_instance(ScriptInstance *p_instance) {

    if (script_instance == p_instance)
        return;

    memdelete(script_instance);

    script_instance = p_instance;

    if (p_instance)
        script = p_instance->get_script().get_ref_ptr();
    else
        script = RefPtr();
}

RefPtr Object::get_script() const {

    return script;
}

bool Object::has_meta(StringView p_name) const {

    return metadata && metadata->has(StringName(p_name));
}

void Object::set_meta(StringView p_name, const Variant &p_value) {
    StringName key(p_name);
    if (p_value.get_type() == VariantType::NIL) {
        if(!metadata)
            return;
        metadata->erase(key);
        if(metadata->empty()) {
            memdelete(metadata);
            metadata = nullptr;
        }

        return;
    }
    if(!metadata)
        metadata = memnew(Dictionary);
    (*metadata)[key] = p_value;
}

Variant Object::get_meta(StringView p_name) const {

    StringName key(p_name);
    ERR_FAIL_COND_V(!metadata || !metadata->has(key), Variant());
    return (*metadata)[key];
}

void Object::remove_meta(StringView p_name) {
    if(!metadata)
        return;
    metadata->erase(StringName(p_name));
    if(metadata->empty()) {
        memdelete(metadata);
        metadata = nullptr;
    }
}

Array Object::_get_property_list_bind() const {

    Vector<PropertyInfo> lpi;
    get_property_list(&lpi);
    return convert_property_list(&lpi);
}

Array Object::_get_method_list_bind() const {

    Vector<MethodInfo> ml;
    get_method_list(&ml);
    Array ret;

    for(MethodInfo &E : ml ) {

        Dictionary d = E;
        //va.push_back(d);
        ret.push_back(d);
    }

    return ret;
}

PoolStringArray Object::_get_meta_list_bind() const {

    PoolStringArray _metaret;
    if(!metadata)
        return _metaret;

    auto keys(metadata->get_key_list());
    for(const auto &E : keys ) {

        _metaret.push_back(E.asCString());
    }

    return _metaret;
}
void Object::get_meta_list(List<String> *p_list) const {
    if(!metadata)
        return;

    auto keys(metadata->get_key_list());
    for(const auto &E : keys ) {

        p_list->push_back(E.asCString());
    }
}

IObjectTooling *Object::get_tooling_interface() const
{
    return private_data->get_tooling();
}

struct _ObjectSignalDisconnectData {

    StringName signal;
    Callable callable;
};

void Object::do_emit_signal(const StringName &p_name, const Variant **p_args, int p_argcount) {

    if (_block_signals) {
        return ; //ERR_CANT_ACQUIRE_RESOURCE; //no emit, signals blocked
    }

    auto s = private_data->signal_map.find(p_name);
    if (s== private_data->signal_map.end()) {
#ifdef DEBUG_ENABLED
        bool signal_is_valid = ClassDB::has_signal(get_class_name(), p_name);
        //check in script
        ERR_FAIL_COND_MSG(!signal_is_valid && !script.is_null() && !refFromRefPtr<Script>(script)->has_script_signal(p_name),
                "Can't emit non-existing signal " + String("\"") + p_name + "\".");
#endif
        //not connected? just return
        return; // ERR_UNAVAILABLE;
    }

    FixedVector<_ObjectSignalDisconnectData,32> disconnect_data;

    //copy on write will ensure that disconnecting the signal or even deleting the object will not affect the signal calling.
    //this happens automatically and will not change the performance of calling.
    //awesome, isn't it?
    auto & slot_map = s->second.slot_map;

    ssize_t ssize = slot_map.size();

    OBJ_DEBUG_LOCK

    FixedVector<const Variant *,16,true> bind_mem; // upto 16 binds will not heap alloc here.

   // Error err = OK;

    for (int i = 0; i < ssize; i++) {

        const Connection &c = slot_map.at(i).second.conn;

        Object* target = c.callable.get_object();
        if (!target) {
            // Target might have been deleted during signal callback, this is expected and OK.
            continue;
        }

        const Variant **args = p_args;
        int argc = p_argcount;

        if (c.flags & ObjectNS::CONNECT_QUEUED) {
            MessageQueue::get_singleton()->push_callable(c.callable, args, argc, true);
        } else {
            Callable::CallError ce;
            _emitting = true;
            Variant ret;
            c.callable.call(args, argc, ret, ce);
            _emitting = false;

            if (ce.error != Callable::CallError::CALL_OK) {
#ifdef DEBUG_ENABLED
                if (c.flags & ObjectNS::CONNECT_PERSIST && Engine::get_singleton()->is_editor_hint() && (script.is_null() || !refFromRefPtr<Script>(script)->is_tool()))
                    continue;
#endif
                if (ce.error == Callable::CallError::CALL_ERROR_INVALID_METHOD && !ClassDB::class_exists(target->get_class_name())) {
                    // most likely object is not initialized yet, do not throw error.
                } else {
                    ERR_PRINT("Error calling from signal '" + String(p_name) + "' to callable: " + Variant::get_callable_error_text(c.callable, args, argc, ce) + ".");
                    //err = ERR_METHOD_NOT_FOUND;
                }
            }
        }

        bool disconnect = c.flags & ObjectNS::CONNECT_ONESHOT;
        disconnect &= Object_allow_disconnect(c.flags);
        if (disconnect) {

            _ObjectSignalDisconnectData dd;
            dd.signal = p_name;
            dd.callable = c.callable;
            disconnect_data.emplace_back(eastl::move(dd));
        }
    }
    for(const _ObjectSignalDisconnectData & dd : disconnect_data) {
        _disconnect(dd.signal, dd.callable);
    }
   // return err;
}

void Object::do_emit_signal(const StringName &p_name, VARIANT_ARG_DECLARE) {

    VARIANT_ARGPTRS

    int argc = 0;

    for (const Variant *i : argptr) {

        if (i->get_type() == VariantType::NIL)
            break;
        argc++;
    }

    do_emit_signal(p_name, argptr, argc);
}

Array Object::_get_signal_list() const {
    Vector<MethodInfo> signal_list;
    get_signal_list(&signal_list);

    Array ret;
    for (const MethodInfo &mi : signal_list) {

        ret.push_back(Dictionary(mi));
    }

    return ret;
}

Array Object::_get_signal_connection_list(StringName p_signal) const {

    Vector<Connection> conns;
    get_all_signal_connections(&conns);

    Array ret;

    for(Connection &c : conns ) {
        if (c.signal.get_name() == p_signal) {
            Dictionary rc;
            //TODO: SEGS: note that this WILL NOT PRESERVE source and target if they are Reference counted types!
            ret.emplace_back((Variant)c);
        }
    }

    return ret;
}

Array Object::_get_incoming_connections() const {

    Array ret;
    for (const Connection &cn : private_data->connections) {
        //TODO: SEGS: source will not be properly preserved if it inherits from Reference
        ret.emplace_back((Variant)cn);
    }

    return ret;
}
bool Object::has_signal(const StringName &p_name) const {
    if (!script.is_null()) {
        Ref<Script> scr = refFromRefPtr<Script>(script);
        if (scr && scr->has_script_signal(p_name)) {
            return true;
        }
    }

    if (ClassDB::has_signal(get_class_name(), p_name)) {
        return true;
    }

    return false;
}
void Object::get_signal_list(Vector<MethodInfo> *p_signals) const {

    if (!script.is_null()) {
        Ref<Script> scr = refFromRefPtr<Script>(script);
        if (scr) {
            scr->get_script_signal_list(p_signals);
        }
    }

    ClassDB::get_signal_list(get_class_name(), p_signals);
    //find maybe usersignals?

    for(auto & signal :  private_data->signal_map) {

        if (not signal.second.user.name.empty()) {
            //user signal
            p_signals->push_back(signal.second.user);
        }
    }
}

void Object::get_all_signal_connections(Vector<Connection> *p_connections) const {

    for (const auto & signal : private_data->signal_map) {

        const SignalData *s = &signal.second;

        for (const auto &entry : s->slot_map) {
            p_connections->emplace_back(entry.second.conn);
        }
    }
}

void Object::get_signal_connection_list(const StringName &p_signal, Vector<Connection> *p_connections) const {

    const auto s = private_data->signal_map.find(p_signal);
    if (s== private_data->signal_map.end()) {
        return; //nothing
    }

    for (const auto &entry : s->second.slot_map) {
        p_connections->emplace_back(entry.second.conn);
    }
}

int Object::get_persistent_signal_connection_count() const {

    int count = 0;

    for (const auto & signal : private_data->signal_map) {

        const SignalData *s = &signal.second;

        for (const auto &entry : s->slot_map) {
            count += (0!=(entry.second.conn.flags & ObjectNS::CONNECT_PERSIST));
        }
    }
    return count;
}

void Object::get_signals_connected_to_this(Vector<Connection> *p_connections) const {

    p_connections->insert(p_connections->end(), private_data->connections.begin(), private_data->connections.end());
}

Error Object::connectF(const StringName &p_signal,Object *tgt, eastl::function<void ()> p_to_object, uint32_t p_flags)
{
    return connect(p_signal,
            create_lambda_callable_function_pointer(
                    tgt->get_instance_id(), make_function(p_to_object)
#ifdef DEBUG_METHODS_ENABLED
                    ,__FUNCTION__, __LINE__
#endif
            ),
            p_flags);
}

bool Object::is_connected(const StringName& p_signal, const Callable& p_callable) const {
    ERR_FAIL_COND_V(p_callable.is_null(), false);

    auto s = private_data->signal_map.find(p_signal);

    if (s==private_data->signal_map.end()) {
        bool signal_is_valid = ClassDB::has_signal(get_class_name(), p_signal);
        if (signal_is_valid) {
            return false;
        }

        if (!script.is_null() && refFromRefPtr<Script>(script)->has_script_signal(p_signal)) {
            return false;
    }

        ERR_FAIL_V_MSG(false, "Nonexistent signal: " + p_signal + ".");
    }

    return s->second.slot_map.contains(p_callable);
}

bool Object::is_connected_any(const StringName &p_signal, GameEntity tgt)
{
    ERR_FAIL_COND_V(tgt==entt::null, false);
    const auto s = private_data->signal_map.find(p_signal);
    if (s== private_data->signal_map.end()) {
        bool signal_is_valid = ClassDB::has_signal(get_class_name(), p_signal);
        if (signal_is_valid) {
            return false;
        }

        if (!script.is_null() && refFromRefPtr<Script>(script)->has_script_signal(p_signal)) {
            return false;
        }

        ERR_FAIL_V_MSG(false, "Nonexistent signal: " + p_signal + ".");
    }
    for(const auto & entry : s->second.slot_map) {
        if(entry.first.get_object_id()==tgt) {
            return true;
        }
    }
    return false;
}

void Object::disconnect_all(const StringName& p_signal, GameEntity target_object) {
    ERR_FAIL_COND(target_object==entt::null);

    auto per_sig_data = private_data->signal_map.find(p_signal);
    ERR_FAIL_COND_MSG(per_sig_data == private_data->signal_map.end(), String(String::CtorSprintf(),"Nonexistent signal '%s' in %s.", p_signal.asCString(), to_string().c_str()));

    Object *tgt = object_for_entity(target_object);
    // find all connections from this signal to tgt
    bool any_erased=false;
    auto &slotmap(per_sig_data->second.slot_map);
    for(auto iter=slotmap.begin(); iter!=slotmap.end(); ) {
        auto & entr = *iter;
        if(entr.first.get_object_id() != target_object) {
            ++iter;
            continue;
        }
        SignalData::Slot* slot = &entr.second;
        slot->reference_count--; // by default is zero, if it was not referenced it will go below it
        if (slot->reference_count >= 0) {
            ++iter;
            continue;
        }
        tgt->private_data->connections.erase(slot->cE);
        iter = slotmap.erase(iter);
        any_erased = true;
        if (slotmap.empty() && ClassDB::has_signal(get_class_name(), p_signal)) {
            //not user signal, delete
            private_data->signal_map.erase(p_signal);
            break;
        }
    }
    ERR_FAIL_COND_MSG(!any_erased, "Signal '" + p_signal + "', is not connected to object: " + tgt->to_string() + ".");
}
void Object::disconnect(const StringName& p_signal, const Callable& p_callable) {
    _disconnect(p_signal, p_callable);
}

void Object::_disconnect(const StringName& p_signal, const Callable& p_callable, bool p_force) {
    ERR_FAIL_COND(p_callable.is_null());

    Object* target_object = p_callable.get_object();
    ERR_FAIL_COND(!target_object);

    auto per_sig_data = private_data->signal_map.find(p_signal);
    ERR_FAIL_COND_MSG(per_sig_data == private_data->signal_map.end(),
            String(String::CtorSprintf(), "Nonexistent signal '%s' in %s.", p_signal.asCString(), to_string().c_str()));

    if(!per_sig_data->second.slot_map.contains(p_callable)) {
        ERR_FAIL_MSG("Signal '" + p_signal + "', is not connected to callable: " + (String)p_callable + ".");
    }

    SignalData::Slot* slot = &per_sig_data->second.slot_map[p_callable];

    if (!p_force) {
        slot->reference_count--; // by default is zero, if it was not referenced it will go below it
        if (slot->reference_count > 0) {
            return;
        }
    }

    target_object->private_data->connections.erase(slot->cE);
    per_sig_data->second.slot_map.erase(p_callable);

    if (per_sig_data->second.slot_map.empty() && ClassDB::has_signal(get_class_name(), p_signal)) {
        //not user signal, delete
        private_data->signal_map.erase(p_signal);
    }
}

void Object::_set_bind(const StringName &p_set, const Variant &p_value) {

    set(p_set, p_value);
}

Variant Object::_get_bind(const StringName &p_name) const {

    return get(p_name);
}

void Object::_set_indexed_bind(const NodePath &p_name, const Variant &p_value) {

    set_indexed(p_name.get_as_property_path().get_subnames(), p_value);
}

Variant Object::_get_indexed_bind(const NodePath &p_name) const {

    return get_indexed(p_name.get_as_property_path().get_subnames());
}

bool Object::initialize_class() {

    static bool initialized = false;
    if (initialized)
        return false;
    game_object_registry.initialize();
    ClassDB::_add_class<Object,void>();
    ClassDB::_set_class_header(Object::get_class_static_name(),__FILE__);
    _bind_methods();
    initialized = true;
    return true;
}

StringName Object::tr(StringView p_message) const {

    if (!_can_translate || !TranslationServer::get_singleton())
        return StringName(p_message);

    return TranslationServer::get_singleton()->translate(p_message);
}

void Object::_clear_internal_resource_paths(const Variant &p_var) {

    switch (p_var.get_type()) {

        case VariantType::OBJECT: {

            RES r(refFromVariant<Resource>(p_var));
            if (not r)
                return;

            if (!StringUtils::begins_with(r->get_path(),"res://") || !StringUtils::contains(r->get_path(),"::"))
                return; //not an internal resource

            Object *object = p_var.as<Object *>();
            if (!object)
                return;

            r->set_path("");
            r->clear_internal_resource_paths();
        } break;
        case VariantType::ARRAY: {

            Array a = p_var.as<Array>();
            for (int i = 0; i < a.size(); i++) {
                _clear_internal_resource_paths(a[i]);
            }

        } break;
        case VariantType::DICTIONARY: {

            Dictionary d = p_var.as<Dictionary>();
            auto keys(d.get_key_list());

            for(StringName &E : keys ) {

                _clear_internal_resource_paths(E);
                _clear_internal_resource_paths(d[E]);
            }
        } break;
        default: {
        }
    }
}

void Object::clear_internal_resource_paths() {

    Vector<PropertyInfo> pinfo;

    get_property_list(&pinfo);

    for(PropertyInfo &E : pinfo ) {

        _clear_internal_resource_paths(get(E.name));
    }
}

void Object::_bind_methods() {
    SE_NAMESPACE(Godot);
    //    const auto &mo = Object::staticMetaObject;
    //    for(int enum_idx = 0; enum_idx < mo.enumeratorCount(); ++enum_idx) {
    //        const QMetaEnum &me(mo.enumerator(enum_idx));
    //        for(int i=0; i<me.keyCount(); ++i)
    //        {
    //            ClassDB::bind_integer_constant(get_class_static_name(), StaticCString(me.name(),true), StaticCString(me.key(i),true), me.value(i));
    //        }
    //    }
    //    for(int prop_idx = 0; prop_idx< mo.propertyCount(); ++prop_idx) {
    //        const QMetaProperty &prop(mo.property(prop_idx));

    //    }

    MethodBinder::bind_method(D_METHOD("get_class"), &Object::wrap_get_class);
    MethodBinder::bind_method(D_METHOD("is_class", {"class"}), &Object::wrap_is_class);
    MethodBinder::bind_method(D_METHOD("set", {"property", "value"}), &Object::_set_bind);
    MethodBinder::bind_method(D_METHOD("get", {"property"}), &Object::_get_bind);
    MethodBinder::bind_method(D_METHOD("set_indexed", {"property", "value"}), &Object::_set_indexed_bind);
    MethodBinder::bind_method(D_METHOD("get_indexed", {"property"}), &Object::_get_indexed_bind);
    MethodBinder::bind_method(D_METHOD("get_property_list"), &Object::_get_property_list_bind);
    MethodBinder::bind_method(D_METHOD("get_method_list"), &Object::_get_method_list_bind);
    MethodBinder::bind_method(D_METHOD("notification", {"what", "reversed"}), &Object::notification, {DEFVAL(false)});
    SE_BIND_METHOD(Object,to_string);
    SE_BIND_METHOD(Object,get_instance_id);

    SE_BIND_METHOD(Object,set_script);
    SE_BIND_METHOD(Object,get_script);

    SE_BIND_METHOD(Object,set_meta);
    SE_BIND_METHOD(Object,remove_meta);
    SE_BIND_METHOD(Object,get_meta);
    SE_BIND_METHOD(Object,has_meta);
    MethodBinder::bind_method(D_METHOD("get_meta_list"), &Object::_get_meta_list_bind);

    MethodBinder::bind_method(D_METHOD("emit_signal", {"signal","arg1","arg2","arg3","arg4","arg5"}), (void (Object::*)(const StringName &,VARIANT_ARG_DECLARE))&Object::do_emit_signal,{Variant(),Variant(),Variant(),Variant(),Variant()});

    {
        MethodInfo mi("call_deferred",PropertyInfo(VariantType::STRING_NAME, "method"));

        MethodBinder::bind_vararg_method("call_deferred", &Object::_call_deferred_bind, eastl::move(mi), null_variant_pvec, false);
    }

    SE_BIND_METHOD(Object,callv);

    SE_BIND_METHOD(Object,has_method);

    SE_BIND_METHOD(Object,has_signal);
    MethodBinder::bind_method(D_METHOD("get_signal_list"), &Object::_get_signal_list);
    MethodBinder::bind_method(D_METHOD("get_signal_connection_list", {"signal"}), &Object::_get_signal_connection_list);
    MethodBinder::bind_method(D_METHOD("get_incoming_connections"), &Object::_get_incoming_connections);

    MethodBinder::bind_method(D_METHOD("connect", {"signal", "callable", "flags"}), &Object::connect, {DEFVAL(0)});
    SE_BIND_METHOD(Object,disconnect);
    SE_BIND_METHOD(Object,is_connected);

    SE_BIND_METHOD(Object,set_block_signals);
    SE_BIND_METHOD(Object,is_blocking_signals);
    SE_BIND_METHOD(Object,property_list_changed_notify);

    SE_BIND_METHOD(Object,set_message_translation);
    SE_BIND_METHOD(Object,can_translate_messages);
    SE_BIND_METHOD(Object,tr);

    SE_BIND_METHOD(Object,is_queued_for_deletion);

    SE_BIND_METHOD(Object,free);

    ADD_SIGNAL(MethodInfo("script_changed"));

    BIND_VMETHOD(MethodInfo("_notification", PropertyInfo(VariantType::INT, "what")));
    BIND_VMETHOD(MethodInfo(VariantType::BOOL, "_set", PropertyInfo(VariantType::STRING, "property"), PropertyInfo(VariantType::NIL, "value")));

    Object_add_tooling_methods();

    BIND_VMETHOD(MethodInfo("_init"));
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_to_string"));

    BIND_CONSTANT(NOTIFICATION_POSTINITIALIZE);
    BIND_CONSTANT(NOTIFICATION_PREDELETE);

    ClassDB::add_namespace("ObjectNS","core/object.h");
    SE_NAMESPACE(ObjectNS);

    BIND_NS_ENUM_CONSTANT(ObjectNS,CONNECT_QUEUED);
    BIND_NS_ENUM_CONSTANT(ObjectNS,CONNECT_PERSIST);
    BIND_NS_ENUM_CONSTANT(ObjectNS,CONNECT_ONESHOT);
    BIND_NS_ENUM_CONSTANT(ObjectNS,CONNECT_REFERENCE_COUNTED);
    SE_END();

    SE_END();
}

void Object::call_deferred(const StringName &p_method, VARIANT_ARG_DECLARE) {

    MessageQueue::get_singleton()->push_call(get_instance_id(), p_method, VARIANT_ARG_PASS);
}
void Object::call_deferred(eastl::function<void()> func) {

    MessageQueue::get_singleton()->push_call(get_instance_id(), func);
}

void Object::set_block_signals(bool p_block) {

    _block_signals = p_block;
}

bool Object::is_blocking_signals() const {

    return _block_signals;
}

void Object::get_translatable_strings(List<String> *p_strings) const {

    Vector<PropertyInfo> plist;
    get_property_list(&plist);

    for(PropertyInfo &E : plist ) {

        if (!(E.usage & PROPERTY_USAGE_INTERNATIONALIZED))
            continue;

        String text = get(E.name).as<String>();

        if (text.empty())
            continue;

        p_strings->push_back(text);
    }
}

VariantType Object::get_static_property_type(const StringName &p_property, bool *r_valid) const {

    bool valid;
    VariantType t = ClassDB::get_property_type(get_class_name(), p_property, &valid);
    if (valid) {
        if (r_valid) {
            *r_valid = true;
        }
        return t;
    }

    if (get_script_instance()) {
        return get_script_instance()->get_property_type(p_property, r_valid);
    }
    if (r_valid)
        *r_valid = false;

    return VariantType::NIL;
}

VariantType Object::get_static_property_type_indexed(const Vector<StringName> &p_path, bool *r_valid) const {

    if (p_path.empty()) {
        if (r_valid) {
            *r_valid = false;
        }

        return VariantType::NIL;
    }

    bool valid = false;
    VariantType t = get_static_property_type(p_path[0], &valid);
    if (!valid) {
        if (r_valid)
            *r_valid = false;

        return VariantType::NIL;
    }

    Variant check = Variant::construct_default(t);

    for (size_t i = 1; i < p_path.size(); i++) {
        if (check.get_type() == VariantType::OBJECT || check.get_type() == VariantType::DICTIONARY || check.get_type() == VariantType::ARRAY) {
            // We cannot be sure about the type of properties this types can have
            if (r_valid)
                *r_valid = false;
            return VariantType::NIL;
        }

        check = check.get_named(p_path[i], &valid);

        if (!valid) {
            if (r_valid) {
                *r_valid = false;
            }
            return VariantType::NIL;
        }
    }

    if (r_valid) {
        *r_valid = true;
    }

    return check.get_type();
}

bool Object::is_queued_for_deletion() const {
    return _is_queued_for_deletion;
}

void *Object::get_script_instance_binding(int p_script_language_index) {
#ifdef DEBUG_ENABLED
    ERR_FAIL_INDEX_V(p_script_language_index, MAX_SCRIPT_INSTANCE_BINDINGS, nullptr);
#endif

    //it's up to the script language to make this thread safe, if the function is called twice due to threads being out of syncro
    //just return the same pointer.
    //if you want to put a big lock in the entire function and keep allocated pointers in a map or something, feel free to do it
    //as it should not really affect performance much (won't be called too often), as in far most caes the condition below will be false afterwards
    if(!_script_instance_bindings) {
        _script_instance_bindings = new ("script_instance_bindings") eastl::array<void *,MAX_SCRIPT_INSTANCE_BINDINGS>();
        _script_instance_bindings->fill(nullptr);
    }
    if (!(*_script_instance_bindings)[p_script_language_index]) {
        void *script_data = ScriptServer::get_language(p_script_language_index)->alloc_instance_binding_data(this);
        if (script_data) {
            instance_binding_count.increment();
            (*_script_instance_bindings)[p_script_language_index] = script_data;
        }
    }

    return (*_script_instance_bindings)[p_script_language_index];
}

bool Object::has_script_instance_binding(int p_script_language_index) {

    return _script_instance_bindings && (*_script_instance_bindings)[p_script_language_index] != nullptr;
}

void Object::set_script_instance_binding(int p_script_language_index, void *p_data) {
#ifdef DEBUG_ENABLED
    CRASH_COND(_script_instance_bindings && (*_script_instance_bindings)[p_script_language_index] != nullptr);
#endif
    if(!_script_instance_bindings) {
        _script_instance_bindings = new ("script_instance_bindings") eastl::array<void *,MAX_SCRIPT_INSTANCE_BINDINGS>();
        _script_instance_bindings->fill(nullptr);
    }
    (*_script_instance_bindings)[p_script_language_index] = p_data;
}
const auto autolink=entt::meta<ObjectLink>().type();

static void report_objects() {
    game_object_registry.lock_registry();
    HashMap<StringName, int> counts;
    game_object_registry.registry.each([&](GameEntity e) {
        counts[object_for_entity(e)->get_class_name()]++;
    });
    game_object_registry.unlock_registry();

    Vector<StringName> keys = counts.keys();
    eastl::sort(keys.begin(), keys.end(), WrapAlphaCompare());
    printf("Object counts =========================================\n");
    for (const StringName &k : keys) {
        if (counts[k]<5)
            continue;
        printf("%32s : %d\n", k.asCString(), counts[k]);
    }
    printf("done          =========================================\n");
}

Object::Object() : _block_signals(false)
                 , _can_translate(true)
                 , _emitting(false)
                 , _is_queued_for_deletion(false) {
    private_data = memnew_args_basic(ObjectPrivate,this);
    {
#if REPORT_INSTANCES
        if (game_object_registry.registry.size() == 10000)
            report_objects();
        if (game_object_registry.registry.size() == 50000)
            report_objects();
        if (game_object_registry.registry.size() == 100000)
            report_objects();
        if (game_object_registry.registry.size() == 400000)
            report_objects();
#endif
        if (game_object_registry.registry.size() > 1500000) {
            report_objects();
            abort();
        }
        game_object_registry.lock_registry();
        entity_id = game_object_registry.create();
        game_object_registry.registry.emplace<ObjectLink>(entity_id,this);
        game_object_registry.unlock_registry();
    }
    _rc.store(nullptr, std::memory_order_release);
}

Object::~Object() {

    ObjectRC *rc = _rc.load(std::memory_order_acquire);
    if (rc && rc->invalidate()) {
        memdelete(rc);
    }
    memdelete(observer_endpoint);

    memdelete(script_instance);
    script_instance = nullptr;

    if (_emitting) {
        //@todo this may need to actually reach the debugger prioritarily somehow because it may crash before
        ERR_PRINT("Object " + to_string() +
                  " was freed or unreferenced while a signal is being emitted from it. Try connecting to the signal "
                  "using "
                  "'CONNECT_DEFERRED' flag, or use queue_free() to free the object (if this object is a Node) to avoid "
                  "this "
                  "error and potential crashes.");
    }
    memdelete(private_data);
    private_data = nullptr;
    memdelete(metadata);
    metadata = nullptr;

    if (!ScriptServer::are_languages_finished() && _script_instance_bindings) {
    for (int i = 0; i < MAX_SCRIPT_INSTANCE_BINDINGS; i++) {
        if ((*_script_instance_bindings)[i]) {
            ScriptServer::get_language(i)->free_instance_binding_data((*_script_instance_bindings)[i]);
        }
    }
}
    {
        game_object_registry.lock_registry();
        game_object_registry.registry.destroy(entity_id);
        game_object_registry.unlock_registry();
    }
    entity_id = entt::null;
}

void predelete_handler(Object *p_object) {
    p_object->_predelete();
}

void postinitialize_handler(Object *p_object) {

    p_object->_postinitialize();
}

Object *object_for_entity(GameEntity ent)
{
    if(ent==entt::null) {
        return nullptr;
    }
    game_object_registry.lock_registry();
    if(!game_object_registry.valid(ent)) {
        game_object_registry.unlock_registry();
        return nullptr;
    }
    auto link = game_object_registry.registry.try_get<ObjectLink>(ent);
    game_object_registry.unlock_registry();
    if (link) {
        return link->object;
    }
    return nullptr;
}
