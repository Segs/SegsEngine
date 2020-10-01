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

#include "core/class_db.h"
#include "core/object_db.h"
#include "core/core_string_names.h"
#include "core/message_queue.h"
#include "core/os/os.h"
#include "core/object_rc.h"
#include "core/print_string.h"
#include "core/resource.h"
#include "core/script_language.h"
#include "core/string.h"
#include "core/string_formatter.h"
#include "core/translation.h"
#include "core/hash_map.h"
#include "core/pool_vector.h"
#include "core/vmap.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"

#include <EASTL/vector_map.h>

struct Object::SignalData  {
    struct Slot {
        int reference_count = 0;
        Connection conn;
        List<Connection>::iterator cE;
    };

    MethodInfo user;
    VMap<Callable, Slot> slot_map;
};

struct Object::ObjectPrivate {
    IObjectTooling *m_tooling;
    HashMap<StringName, SignalData> signal_map;
    List<Connection> connections;

#ifdef DEBUG_ENABLED
    SafeRefCount _lock_index;
#endif

    ObjectPrivate(Object *self)
    {
#ifdef DEBUG_ENABLED
        _lock_index.init(1);
#endif
        m_tooling = create_tooling_for(self);
    }
    ~ObjectPrivate() {
        //TODO: use range-based-for + signal_map.clear() afterwards ?
        while(!signal_map.empty()) {
            SignalData *s = &signal_map.begin()->second;

            //brute force disconnect for performance
            const auto *slot_list = s->slot_map.get_array();
            int slot_count = s->slot_map.size();
            for (int i = 0; i < slot_count; i++) {
                slot_list[i].value.conn.callable.get_object()->private_data->connections.erase(slot_list[i].value.cE);
            }

            signal_map.erase(signal_map.begin());
        }

        //signals from nodes that connect to this node
        while (connections.size()) {
            Connection c = connections.front();
            c.signal.get_object()->_disconnect(c.signal.get_name(), c.callable, true);
        }
        relase_tooling(m_tooling);
        m_tooling = nullptr;
    }
    IObjectTooling *get_tooling() {
        return m_tooling;
    }
};
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

    if (p_dict.has("type"))
        pi.type = p_dict["type"].as<VariantType>();

    if (p_dict.has("name"))
        pi.name = p_dict["name"].as<StringName>();

    if (p_dict.has("class_name"))
        pi.class_name = p_dict["class_name"].as<StringName>();

    if (p_dict.has("hint"))
        pi.hint = p_dict["hint"].as<PropertyHint>();

    if (p_dict.has("hint_string"))

        pi.hint_string = p_dict["hint_string"].as<String>();

    if (p_dict.has("usage"))
        pi.usage = p_dict["usage"].as<uint32_t>();

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
    for (const Variant &varg : default_arguments)
        da.push_back(varg);
    d["default_args"] = da;
    d["flags"] = flags;
    d["id"] = id;
    Dictionary r = return_val;
    d["return"] = r;
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
    d["binds"] = Variant::fromVector(Span<const Variant>(binds));
    return d;
}

bool Object::Connection::operator<(const Connection &p_conn) const noexcept {

    if (signal == p_conn.signal) {
        return callable < p_conn.callable;
    }
    else {
        return signal < p_conn.signal;
    }
}
Object::Connection::Connection(const Variant &p_variant) {

    Dictionary d = (Dictionary)p_variant;
    if (d.has("signal")) {
        signal = (Signal)d["signal"];
    }
    if (d.has("callable")) {
        callable = (Callable)d["callable"];
    }
    if (d.has("flags")) {
        flags = (uint32_t)d["flags"];
    }
    if (d.has("binds")) {
        binds = (Vector<Variant>)d["binds"];
    }
}

bool Object::_predelete() {

    _predelete_ok = 1;
    notification(NOTIFICATION_PREDELETE, true);
    if (_predelete_ok) {
        _class_ptr = nullptr; //must restore so destructors can access class ptr correctly
    }
    // TODO: SEGS: Only case where _predelete_ok is false here is if something constructed another Object on this one's
    // memory in the notification handler.
    return _predelete_ok;
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

    } else if (p_name == CoreStringNames::get_singleton()->_meta) {
        //set_meta(p_name,p_value);
        metadata = p_value.duplicate_t<Dictionary>();
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
        ret = refFromRefPtr<Script>(get_script());
        if (r_valid) {
            *r_valid = true;
        }
        return ret;
    } else if (p_name == CoreStringNames::get_singleton()->_meta) {
        ret = metadata;
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
        if (r_valid)
            *r_valid = valid;
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
    if (!r_valid) r_valid = &valid;

    Vector<Variant> value_stack;

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
        Object_add_tool_properties(p_list);
        p_list->push_back(PropertyInfo(VariantType::OBJECT, "script", PropertyHint::ResourceType, "Script", PROPERTY_USAGE_DEFAULT));
    }
    if (!metadata.empty()) {
        p_list->push_back(PropertyInfo(VariantType::DICTIONARY, "__meta__", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
    }
    if (script_instance && !p_reversed) {
        p_list->push_back(PropertyInfo(VariantType::NIL, "Script Variables", PropertyHint::None, nullptr, PROPERTY_USAGE_CATEGORY));
        script_instance->get_property_list(p_list);
    }
}

void Object::_validate_property(PropertyInfo & /*property*/) const {
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
    for (int i = 0; i < VARIANT_ARG_MAX; i++) {
        if (argptr[i]->get_type() == VariantType::NIL) {
            break;
        }
        argc++;
    }

    Callable::CallError error;

    Variant ret = call(p_name, argptr, argc, error);
    return ret;
}

Variant Object::call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    r_error.error = Callable::CallError::CALL_OK;

    if (p_method == CoreStringNames::get_singleton()->_free) {
//free must be here, before anything, always ready
#ifdef DEBUG_ENABLED
        if (p_argcount != 0) {
            r_error.argument = 0;
            r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
            return Variant();
        }
        if (object_cast<RefCounted>(this)) {
            r_error.argument = 0;
            r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
            ERR_FAIL_V_MSG(Variant(), "Can't 'free' a reference.");
        }

        if (private_data->_lock_index.get() > 1) {
            r_error.argument = 0;
            r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
            ERR_FAIL_V_MSG(Variant(), "Object is locked and can't be freed.");
        }

#endif
        //must be here, must be before everything,
        memdelete(this);
        r_error.error = Callable::CallError::CALL_OK;
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
        if (valid)
            return ret;
    }
    return FormatVE("[%s:%zd]",get_class(),get_instance_id());
}

void Object::_changed_callback(Object * /*p_changed*/, StringName /*p_prop*/) {
}

void Object::property_list_changed_notify() {

    Object_change_notify(this);
}

void Object::cancel_delete() {

    _predelete_ok = true;
}

#ifdef DEBUG_ENABLED
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
#endif

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

    if (script_instance) {
        memdelete(script_instance);
        script_instance = nullptr;
    }

    script = p_script;
    Ref<Script> s(refFromRefPtr<Script>(script));

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

    if (script_instance)
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

    return metadata.has(Variant(p_name));
}

void Object::set_meta(StringView p_name, const Variant &p_value) {
    Variant key(p_name);
    if (p_value.get_type() == VariantType::NIL) {
        metadata.erase(key);
        return;
    }

    metadata[key] = p_value;
}

Variant Object::get_meta(StringView p_name) const {

    Variant key(p_name);
    ERR_FAIL_COND_V(!metadata.has(key), Variant());
    return metadata[key];
}

void Object::remove_meta(StringView p_name) {
    metadata.erase(Variant(p_name));
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

    Vector<Variant> keys(metadata.get_key_list());
    for(const Variant &E : keys ) {

        _metaret.push_back(E.as<String>());
    }

    return _metaret;
}
void Object::get_meta_list(List<String> *p_list) const {

    Vector<Variant> keys(metadata.get_key_list());
    for(const Variant &E : keys ) {

        p_list->push_back(E.as<String>());
    }
}

IObjectTooling *Object::get_tooling_interface() const
{
    return private_data->get_tooling();
}

void Object::add_user_signal(MethodInfo &&p_signal) {

    ERR_FAIL_COND_MSG(p_signal.name.empty(), "Signal name cannot be empty.");
    ERR_FAIL_COND_MSG(ClassDB::has_signal(get_class_name(), p_signal.name), "User signal's name conflicts with a built-in signal of '" + String(get_class_name()) + "'.");
    ERR_FAIL_COND_MSG(private_data->signal_map.contains(p_signal.name), "Trying to add already existing signal '" + String(p_signal.name) + "'.");
    SignalData s;
    s.user = eastl::move(p_signal);
    private_data->signal_map[p_signal.name] = eastl::move(s);
}

bool Object::_has_user_signal(const StringName &p_name) const {

    if (!private_data->signal_map.contains(p_name))
        return false;
    return not private_data->signal_map[p_name].user.name.empty();
}

struct _ObjectSignalDisconnectData {

    StringName signal;
    Callable callable;
};

Variant Object::_emit_signal(const Variant** p_args, int p_argcount, Callable::CallError& r_error) {

    r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;

    ERR_FAIL_COND_V(p_argcount < 1, Variant());
    if (p_args[0]->get_type() != VariantType::STRING_NAME && p_args[0]->get_type() != VariantType::STRING) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
        r_error.argument = 0;
        r_error.expected = VariantType::STRING_NAME;
        ERR_FAIL_COND_V(p_args[0]->get_type() != VariantType::STRING_NAME && p_args[0]->get_type() != VariantType::STRING, Variant());
    }

    r_error.error = Callable::CallError::CALL_OK;

    StringName signal = (StringName)*p_args[0];

    const Variant** args = nullptr;

    int argc = p_argcount - 1;
    if (argc) {
        args = &p_args[1];
    }

    emit_signal(signal, args, argc);

    return Variant();
}

Error Object::emit_signal(const StringName &p_name, const Variant **p_args, int p_argcount) {

    if (_block_signals) {
        return ERR_CANT_ACQUIRE_RESOURCE; //no emit, signals blocked
    }

    auto s = private_data->signal_map.find(p_name);
    if (s== private_data->signal_map.end()) {
#ifdef DEBUG_ENABLED
        bool signal_is_valid = ClassDB::has_signal(get_class_name(), p_name);
        //check in script
        ERR_FAIL_COND_V_MSG(!signal_is_valid && !script.is_null() && !refFromRefPtr<Script>(script)->has_script_signal(p_name),
                ERR_UNAVAILABLE, "Can't emit non-existing signal " + String("\"") + p_name + "\".");
#endif
        //not connected? just return
        return ERR_UNAVAILABLE;
    }

    FixedVector<_ObjectSignalDisconnectData,32> disconnect_data;

    //copy on write will ensure that disconnecting the signal or even deleting the object will not affect the signal calling.
    //this happens automatically and will not change the performance of calling.
    //awesome, isn't it?
    VMap<Callable, SignalData::Slot> slot_map = s->second.slot_map;

    int ssize = slot_map.size();

    OBJ_DEBUG_LOCK

    FixedVector<const Variant *,16,true> bind_mem; // upto 16 binds will not heap alloc here.

    Error err = OK;

    for (int i = 0; i < ssize; i++) {

        const Connection &c = slot_map.getv(i).conn;

        Object* target = c.callable.get_object();
        if (!target) {
            // Target might have been deleted during signal callback, this is expected and OK.
            continue;
        }

        const Variant **args = p_args;
        int argc = p_argcount;

        if (!c.binds.empty()) {
            //handle binds
            bind_mem.resize(p_argcount + c.binds.size());

            for (int j = 0; j < p_argcount; j++) {
                bind_mem[j] = p_args[j];
            }
            for (size_t j = 0; j < c.binds.size(); j++) {
                bind_mem[p_argcount + j] = &c.binds[j];
            }

            args = (const Variant **)bind_mem.data();
            argc = bind_mem.size();
        }

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
                    err = ERR_METHOD_NOT_FOUND;
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
    return err;
}

Error Object::emit_signal(const StringName &p_name, VARIANT_ARG_DECLARE) {

    VARIANT_ARGPTRS

    int argc = 0;

    for (int i = 0; i < VARIANT_ARG_MAX; i++) {

        if (argptr[i]->get_type() == VariantType::NIL)
            break;
        argc++;
    }

    return emit_signal(p_name, argptr, argc);
}

void Object::_add_user_signal(const StringName &p_name, const Array &p_args) {

    // this version of add_user_signal is meant to be used from scripts or external apis
    // without access to ADD_SIGNAL in bind_methods
    // added events are per instance, as opposed to the other ones, which are global

    MethodInfo mi;
    mi.name = p_name;

    for (int i = 0; i < p_args.size(); i++) {

        Dictionary d = p_args[i].as<Dictionary>();
        PropertyInfo param;

        if (d.has("name"))
            param.name = d["name"].as<StringName>();
        if (d.has("type"))
            param.type = d["type"].as<VariantType>();

        mi.arguments.emplace_back(eastl::move(param));
    }

    add_user_signal(eastl::move(mi));
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

    List<Connection> conns;
    get_all_signal_connections(&conns);

    Array ret;

    for(Connection &c : conns ) {
        if (c.signal.get_name() == p_signal) {
            Dictionary rc;
            //TODO: SEGS: note that this WILL NOT PRESERVE source and target if they are Reference counted types!
            ret.emplace_back(c);
        }
    }

    return ret;
}

Array Object::_get_incoming_connections() const {

    Array ret;
    for (const Connection &cn : private_data->connections) {
        //TODO: SEGS: source will not be properly preserved if it inherits from Reference
        ret.emplace_back(cn);
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

    if (_has_user_signal(p_name)) {
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

void Object::get_all_signal_connections(List<Connection> *p_connections) const {

    for (const auto & signal : private_data->signal_map) {

        const SignalData *s = &signal.second;

        for (int i = 0; i < s->slot_map.size(); i++) {

            p_connections->push_back(s->slot_map.getv(i).conn);
        }
    }
}

void Object::get_signal_connection_list(const StringName &p_signal, List<Connection> *p_connections) const {

    const auto s = private_data->signal_map.find(p_signal);
    if (s== private_data->signal_map.end())
        return; //nothing

    for (int i = 0; i < s->second.slot_map.size(); i++)
        p_connections->push_back(s->second.slot_map.getv(i).conn);
}

int Object::get_persistent_signal_connection_count() const {

    int count = 0;

    for (const auto & signal : private_data->signal_map) {

        const SignalData *s = &signal.second;

        for (int i = 0; i < s->slot_map.size(); i++) {

            if (s->slot_map.getv(i).conn.flags & ObjectNS::CONNECT_PERSIST) {
                count += 1;
            }
        }
    }

    return count;
}

void Object::get_signals_connected_to_this(List<Connection> *p_connections) const {

    p_connections->insert(p_connections->end(), private_data->connections.begin(), private_data->connections.end());
}

Error Object::connect(const StringName& p_signal, const Callable& p_callable, const Vector<Variant>& p_binds, uint32_t p_flags) {
    ERR_FAIL_COND_V(p_callable.is_null(), ERR_INVALID_PARAMETER);

    Object* target_object = p_callable.get_object();
    ERR_FAIL_COND_V(!target_object, ERR_INVALID_PARAMETER);

    auto s = private_data->signal_map.find(p_signal);

    if (s==private_data->signal_map.end()) {
        bool signal_is_valid = ClassDB::has_signal(get_class_name(), p_signal);
        //check in script
        if (!signal_is_valid && !script.is_null()) {
            Ref<Script> script_ref(refFromRefPtr<Script>(script));
            if (script_ref->has_script_signal(p_signal)) {
                signal_is_valid = true;
            }
#ifdef TOOLS_ENABLED
            else {
                //allow connecting signals anyway if script is invalid, see issue #17070
                if (!script_ref->is_valid()) {
                    signal_is_valid = true;
                }
            }
#endif
        }

        ERR_FAIL_COND_V_MSG(!signal_is_valid, ERR_INVALID_PARAMETER,
                "In Object of type '" + String(get_class()) +
                "': Attempt to connect nonexistent signal '" + p_signal +
                "' to callable '" + (String)p_callable + "'.");

        private_data->signal_map[p_signal] = SignalData();
        s = private_data->signal_map.emplace(p_signal,SignalData()).first;
    }

    Callable target = p_callable;

    if (s->second.slot_map.has(target)) {
        if (p_flags & ObjectNS::CONNECT_REFERENCE_COUNTED) {
            s->second.slot_map[target].reference_count++;
            return OK;
        }
        else {
            ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Signal '" + p_signal + "' is already connected to given callable '" + (String)p_callable + "' in that object.");
        }
    }

    SignalData::Slot slot;

    Connection conn;
    conn.callable = target;
    conn.signal = ::Signal(this, p_signal);
    conn.flags = p_flags;
    conn.binds = p_binds;
    slot.conn = conn;
    auto &conns(target_object->private_data->connections);
    slot.cE = conns.emplace(conns.end(), eastl::move(conn));
    if (p_flags & ObjectNS::CONNECT_REFERENCE_COUNTED) {
        slot.reference_count = 1;
    }

    s->second.slot_map[target] = slot;

    return OK;
}

bool Object::is_connected(const StringName& p_signal, const Callable& p_callable) const {
    ERR_FAIL_COND_V(p_callable.is_null(), false);
    auto s = private_data->signal_map.find(p_signal);
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

    Callable target = p_callable;

    return s->second.slot_map.has(target);
}

void Object::disconnect(const StringName& p_signal, const Callable& p_callable) {
    _disconnect(p_signal, p_callable);
}

void Object::_disconnect(const StringName& p_signal, const Callable& p_callable, bool p_force) {
    ERR_FAIL_COND(p_callable.is_null());

    Object* target_object = p_callable.get_object();
    ERR_FAIL_COND(!target_object);

    auto s = private_data->signal_map.find(p_signal);
    ERR_FAIL_COND_MSG(s== private_data->signal_map.end(), vformat("Nonexistent signal '%s' in %s.", p_signal, to_string()));

    ERR_FAIL_COND_MSG(!s->second.slot_map.has(p_callable), "Disconnecting nonexistent signal '" + p_signal + "', callable: " + (String)p_callable + ".");

    SignalData::Slot* slot = &s->second.slot_map[p_callable];

    if (!p_force) {
        slot->reference_count--; // by default is zero, if it was not referenced it will go below it
        if (slot->reference_count >= 0) {
            return;
        }
    }

    target_object->private_data->connections.erase(slot->cE);
    s->second.slot_map.erase(p_callable);

    if (s->second.slot_map.empty() && ClassDB::has_signal(get_class_name(), p_signal)) {
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
    ClassDB::_add_class<Object,void>();
    ClassDB::_set_class_header(Object::get_class_static_name(),__FILE__);
    _bind_methods();
    initialized = true;
    return true;
}

StringName Object::tr(const StringName &p_message) const {

    if (!_can_translate || !TranslationServer::get_singleton())
        return p_message;

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
            Vector<Variant> keys(d.get_key_list());

            for(Variant &E : keys ) {

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
    MethodBinder::bind_method(D_METHOD("to_string"), &Object::to_string);
    MethodBinder::bind_method(D_METHOD("get_instance_id"), &Object::get_instance_id);

    MethodBinder::bind_method(D_METHOD("set_script", {"script"}), &Object::set_script);
    MethodBinder::bind_method(D_METHOD("get_script"), &Object::get_script);

    MethodBinder::bind_method(D_METHOD("set_meta", {"name", "value"}), &Object::set_meta);
    MethodBinder::bind_method(D_METHOD("remove_meta", {"name"}), &Object::remove_meta);
    MethodBinder::bind_method(D_METHOD("get_meta", {"name"}), &Object::get_meta);
    MethodBinder::bind_method(D_METHOD("has_meta", {"name"}), &Object::has_meta);
    MethodBinder::bind_method(D_METHOD("get_meta_list"), &Object::_get_meta_list_bind);

    MethodBinder::bind_method(D_METHOD("add_user_signal", {"signal", "arguments"}), &Object::_add_user_signal, {DEFVAL(Array())});
    MethodBinder::bind_method(D_METHOD("has_user_signal", {"signal"}), &Object::_has_user_signal);

    {
        MethodInfo mi("emit_signal",PropertyInfo(VariantType::STRING_NAME, "signal"));

        MethodBinder::bind_vararg_method("emit_signal", &Object::_emit_signal, eastl::move(mi), null_variant_pvec, false);
    }

    {
        MethodInfo mi("call",PropertyInfo(VariantType::STRING_NAME, "method"));

        MethodBinder::bind_vararg_method("call", &Object::_call_bind, eastl::move(mi));
    }

    {
        MethodInfo mi("call_deferred",PropertyInfo(VariantType::STRING_NAME, "method"));

        MethodBinder::bind_vararg_method("call_deferred", &Object::_call_deferred_bind, eastl::move(mi), null_variant_pvec, false);
    }

    MethodBinder::bind_method(D_METHOD("set_deferred", {"property", "value"}), &Object::set_deferred);

    MethodBinder::bind_method(D_METHOD("callv", {"method", "arg_array"}), &Object::callv);

    MethodBinder::bind_method(D_METHOD("has_method", {"method"}), &Object::has_method);

    MethodBinder::bind_method(D_METHOD("has_signal", {"signal"}), &Object::has_signal);
    MethodBinder::bind_method(D_METHOD("get_signal_list"), &Object::_get_signal_list);
    MethodBinder::bind_method(D_METHOD("get_signal_connection_list", {"signal"}), &Object::_get_signal_connection_list);
    MethodBinder::bind_method(D_METHOD("get_incoming_connections"), &Object::_get_incoming_connections);

    MethodBinder::bind_method(D_METHOD("connect", {"signal", "callable", "binds", "flags"}), &Object::connect, {DEFVAL(Array()), DEFVAL(0)});
    MethodBinder::bind_method(D_METHOD("disconnect", {"signal","callable" }), &Object::disconnect);
    MethodBinder::bind_method(D_METHOD("is_connected", {"signal", "callable" }), &Object::is_connected);

    MethodBinder::bind_method(D_METHOD("set_block_signals", {"enable"}), &Object::set_block_signals);
    MethodBinder::bind_method(D_METHOD("is_blocking_signals"), &Object::is_blocking_signals);
    MethodBinder::bind_method(D_METHOD("property_list_changed_notify"), &Object::property_list_changed_notify);

    MethodBinder::bind_method(D_METHOD("set_message_translation", {"enable"}), &Object::set_message_translation);
    MethodBinder::bind_method(D_METHOD("can_translate_messages"), &Object::can_translate_messages);
    MethodBinder::bind_method(D_METHOD("tr", {"message"}), &Object::tr);

    MethodBinder::bind_method(D_METHOD("is_queued_for_deletion"), &Object::is_queued_for_deletion);

    ClassDB::add_virtual_method(StaticCString("Object"), MethodInfo("free"), false);

    ADD_SIGNAL(MethodInfo("script_changed"));

    BIND_VMETHOD(MethodInfo("_notification", PropertyInfo(VariantType::INT, "what")));
    BIND_VMETHOD(MethodInfo(VariantType::BOOL, "_set", PropertyInfo(VariantType::STRING, "property"), PropertyInfo(VariantType::NIL, "value")));

    Object_add_tooling_methods();

    BIND_VMETHOD(MethodInfo("_init"));
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_to_string"));

    BIND_CONSTANT(NOTIFICATION_POSTINITIALIZE);
    BIND_CONSTANT(NOTIFICATION_PREDELETE);

    ClassDB::add_namespace("ObjectNS","core/object.h");

    BIND_NS_ENUM_CONSTANT(ObjectNS,CONNECT_QUEUED);
    BIND_NS_ENUM_CONSTANT(ObjectNS,CONNECT_PERSIST);
    BIND_NS_ENUM_CONSTANT(ObjectNS,CONNECT_ONESHOT);
    BIND_NS_ENUM_CONSTANT(ObjectNS,CONNECT_REFERENCE_COUNTED);
}

void Object::call_deferred(const StringName &p_method, VARIANT_ARG_DECLARE) {

    MessageQueue::get_singleton()->push_call(this, p_method, VARIANT_ARG_PASS);
}
void Object::call_deferred(eastl::function<void()> func) {

    MessageQueue::get_singleton()->push_call(this->get_instance_id(), func);
}

void Object::set_deferred(const StringName &p_property, const Variant &p_value) {
    MessageQueue::get_singleton()->push_set(this, p_property, p_value);
}

void Object::set_block_signals(bool p_block) {

    _block_signals = p_block;
}

bool Object::is_blocking_signals() const {

    return _block_signals;
}

void Object::get_translatable_strings(List<StringName> *p_strings) const {

    Vector<PropertyInfo> plist;
    get_property_list(&plist);

    for(PropertyInfo &E : plist ) {

        if (!(E.usage & PROPERTY_USAGE_INTERNATIONALIZED))
            continue;

        StringName text = get(E.name).as<StringName>();

        if (text.empty())
            continue;

        p_strings->push_back(text);
    }
}

VariantType Object::get_static_property_type(const StringName &p_property, bool *r_valid) const {

    bool valid;
    VariantType t = ClassDB::get_property_type(get_class_name(), p_property, &valid);
    if (valid) {
        if (r_valid)
            *r_valid = true;
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
        if (r_valid)
            *r_valid = false;

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
            if (r_valid)
                *r_valid = false;
            return VariantType::NIL;
        }
    }

    if (r_valid)
        *r_valid = true;

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

    if (!_script_instance_bindings[p_script_language_index]) {
        void *script_data = ScriptServer::get_language(p_script_language_index)->alloc_instance_binding_data(this);
        if (script_data) {
            atomic_increment(&instance_binding_count);
            _script_instance_bindings[p_script_language_index] = script_data;
        }
    }

    return _script_instance_bindings[p_script_language_index];
}

bool Object::has_script_instance_binding(int p_script_language_index) {

    return _script_instance_bindings[p_script_language_index] != nullptr;
}

void Object::set_script_instance_binding(int p_script_language_index, void *p_data) {
#ifdef DEBUG_ENABLED
    CRASH_COND(_script_instance_bindings[p_script_language_index] != nullptr);
#endif
    _script_instance_bindings[p_script_language_index] = p_data;
}
void Object::get_argument_options(const StringName & /*p_function*/, int /*p_idx*/, List<String> * /*r_options*/) const {
}


Object::Object() {
    private_data = memnew_args_basic(ObjectPrivate,this);
    _class_ptr = nullptr;
    _block_signals = false;
    _predelete_ok = 0;
    _instance_id = gObjectDB().add_instance(this);
    _can_translate = true;
    _is_queued_for_deletion = false;
    _emitting = false;
    instance_binding_count = 0;
    memset(_script_instance_bindings, 0, sizeof(void *) * MAX_SCRIPT_INSTANCE_BINDINGS);
    script_instance = nullptr;
#ifdef DEBUG_ENABLED
    _rc.store(nullptr, std::memory_order_release);
#endif
}

Object::~Object() {

#ifdef DEBUG_ENABLED
    ObjectRC *rc = _rc.load(std::memory_order_acquire);
    if (rc) {
        if (rc->invalidate()) {
            memdelete(rc);
        }
    }
#endif

    if (script_instance)
        memdelete(script_instance);
    script_instance = nullptr;

    if (_emitting) {
        //@todo this may need to actually reach the debugger prioritarily somehow because it may crash before
        ERR_PRINT("Object " + to_string() +
                  " was freed or unreferenced while a signal is being emitted from it. Try connecting to the signal using "
                  "'CONNECT_DEFERRED' flag, or use queue_free() to free the object (if this object is a Node) to avoid this "
                  "error and potential crashes.");
    }
    if(private_data)
        memdelete(private_data);

    gObjectDB().remove_instance(this);
    _instance_id = ObjectID(0ULL);
    _predelete_ok = 2;

    if (!ScriptServer::are_languages_finished()) {
        for (int i = 0; i < MAX_SCRIPT_INSTANCE_BINDINGS; i++) {
            if (_script_instance_bindings[i]) {
                ScriptServer::get_language(i)->free_instance_binding_data(_script_instance_bindings[i]);
            }
        }
    }
}

bool predelete_handler(Object *p_object) {

    return p_object->_predelete();
}

void postinitialize_handler(Object *p_object) {

    p_object->_postinitialize();
}
