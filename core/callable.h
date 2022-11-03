/*************************************************************************/
/*  callable.h                                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#define QT_NO_EMIT

#include "core/vector.h"
#include "core/engine_entities.h"
#include "core/string_name.h"
#include "core/error_list.h"
#include "entt/entity/entity.hpp"

class Object;
class Variant;
class CallableCustom;
class Array;
enum class VariantType : int8_t;
// This is an abstraction of things that can be called.
// It is used for signals and other cases where efficient calling of functions
// is required. It is designed for the standard case (object and method)
// but can be optimized or customized.

class GODOT_EXPORT Callable {
    //needs to be max 16 bytes in 64 bits
    StringName method;
    union {
        GameEntity object;
        CallableCustom *custom;
    };
    bool custom_method=false;

public:
    struct CallError {
        enum Error {
            CALL_OK,
            CALL_ERROR_INVALID_METHOD,
            CALL_ERROR_INVALID_ARGUMENT, // expected is variant type
            CALL_ERROR_TOO_MANY_ARGUMENTS, // expected is number of arguments
            CALL_ERROR_TOO_FEW_ARGUMENTS, // expected is number of arguments
            CALL_ERROR_INSTANCE_IS_NULL,
        };
        Error error;
        int argument;
            VariantType expected;
    };

    void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, CallError &r_call_error) const;
    //void call_deferred(const Variant **p_arguments, int p_argcount) const;

    _FORCE_INLINE_ bool is_null() const {
        return method.empty() && (custom_method ? custom == nullptr : object == entt::null);
    }
    _FORCE_INLINE_ bool is_valid() const {
        return !is_null();
    }
    _FORCE_INLINE_ bool is_custom() const {
        return method.empty() && custom_method && custom != nullptr;
    }
    _FORCE_INLINE_ bool is_standard() const {
        return !method.empty();
    }

    [[nodiscard]] Object *get_object() const;
    [[nodiscard]] GameEntity get_object_id() const;
    [[nodiscard]] StringName get_method() const;
    [[nodiscard]] CallableCustom *get_custom() const;

    uint32_t hash() const;
    explicit operator size_t() const // used by eastl hash containers
    {
        return hash();
    }

    bool operator==(const Callable &p_callable) const;
    bool operator!=(const Callable &p_callable) const;
    bool operator<(const Callable &p_callable) const;

    void operator=(const Callable &p_callable);

    operator String() const;

    Callable(const Object *p_object, const StringName &p_method);
    Callable(GameEntity p_object, const StringName &p_method);
    Callable(CallableCustom *p_custom);
    Callable(const Callable &p_callable);
    Callable() { object = entt::null; }
    ~Callable();
};

class GODOT_EXPORT CallableCustom {
    friend class Callable;
    SafeRefCount ref_count;
    bool referenced = false;

public:
    using CompareEqualFunc = bool (*)(const CallableCustom *p_a, const CallableCustom *p_b);
    using CompareLessFunc = bool (*)(const CallableCustom *p_a, const CallableCustom *p_b);

    //for every type that inherits, these must always be the same for this type
    virtual uint32_t hash() const = 0;
    virtual String get_as_text() const = 0;
    virtual CompareEqualFunc get_compare_equal_func() const = 0;
    virtual CompareLessFunc get_compare_less_func() const = 0;
    virtual GameEntity get_object() const = 0; //must always be able to provide an object
    virtual void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const = 0;
    CallableCustom();
    virtual ~CallableCustom() {}
};

// This is just a proxy object to object signals, its only
// allocated on demand by/for scripting languages so it can
// be put inside a Variant, but it is not
// used by the engine itself.
class GODOT_EXPORT Signal {
    StringName name;
    GameEntity object;

public:
    _FORCE_INLINE_ bool is_null() const {
        return object==entt::null && name.empty();
    }
    [[nodiscard]] Object *get_object() const;
    [[nodiscard]] GameEntity get_object_id() const;
    [[nodiscard]] StringName get_name() const;

    bool operator==(const Signal &p_signal) const {
        return object == p_signal.object && name == p_signal.name;
    }
    bool operator!=(const Signal &p_signal) const;
    bool operator<(const Signal &p_signal) const;

    operator String() const;

    //void emit_signal(const Variant **p_arguments, int p_argcount) const;
    Error connect(const Callable &p_callable, uint32_t p_flags = 0);
    void disconnect(const Callable &p_callable);
    [[nodiscard]] bool is_connected(const Callable &p_callable) const;

    [[nodiscard]] Array get_connections() const;
    Signal(const Object *p_object, const StringName &p_name);
    Signal(GameEntity p_object, const StringName& p_name) : name(p_name), object(p_object) {
    }
    Signal() = default;
};
