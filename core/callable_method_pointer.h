/*************************************************************************/
/*  callable_method_pointer.h                                            */
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

#include "core/callable.h"
#include "core/hashfuncs.h"
#include "core/object.h"
#include "core/object_db.h"
#include "core/string.h"
#include "core/string_utils.h"
#include "core/type_info.h"

#include <stdlib.h>
#include <cassert>

struct FunctorCallable : CallableCustom
{
private:
    const uint32_t h;
    const char *m_filename;
    int m_line;
public:
    ObjectID m_holder;
    eastl::function<void()> m_func;

    FunctorCallable(ObjectID holder, eastl::function<void()> f,const char *func=nullptr,int line=0) :
        // dangerous code
        h(hash_djb2_buffer((const uint8_t *)&f,sizeof(eastl::function<void()>))),
        m_filename(func), m_line(line),m_holder(holder), m_func(eastl::move(f))
    {
    }

    uint32_t hash() const override
    {
        return ((uint64_t)m_holder) ^ h;
    }

    String get_as_text() const override
    {
        if(m_filename)
            return String(String::CtorSprintf(),"<%s:%d>",m_filename,m_line);
        else
            return "<LAMBDA>";
    }

    CompareEqualFunc get_compare_equal_func() const override
    {
        return +[](const CallableCustom *a, const CallableCustom *b)-> bool { return a == b; };
    }

    CompareLessFunc get_compare_less_func() const override
    {
        return +[](const CallableCustom *a, const CallableCustom *b)-> bool { return a < b; };
    }

    ObjectID get_object() const override
    {
        return m_holder;
    }

    void call(const Variant **p_arguments, int p_argcount,
              Variant &r_return_value,
              Callable::CallError &r_call_error) const override
    {
        assert(p_argcount==0);
        r_call_error = {};
        if (!m_func)
        {
            r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
        }
        else
            m_func();
    }
};

class CallableCustomMethodPointerBase : public CallableCustom {
    uint32_t *comp_ptr;
    uint32_t comp_size;
    uint32_t h;
#ifdef DEBUG_METHODS_ENABLED
    const char *text = "";
#endif
    static bool compare_equal(const CallableCustom *p_a, const CallableCustom *p_b);
    static bool compare_less(const CallableCustom *p_a, const CallableCustom *p_b);

protected:
    void _setup(uint32_t *p_base_ptr, uint32_t p_ptr_size);

public:
#ifdef DEBUG_METHODS_ENABLED
    void set_text(const char *p_text) {
        text = p_text;
    }
    virtual String get_as_text() const {
        return text;
    }
#else
    virtual String get_as_text() const {
        return String();
    }
#endif
    CompareEqualFunc get_compare_equal_func() const final {
        return compare_equal;
    }
    CompareLessFunc get_compare_less_func() const final  {
        return compare_less;
    }

    virtual uint32_t hash() const;
};

#ifdef DEBUG_METHODS_ENABLED

template <class T>
struct VariantCasterAndValidate {
    static _FORCE_INLINE_ T cast(const Variant **p_args, uint32_t p_arg_idx, Callable::CallError &r_error) {
        VariantType argtype = GetTypeInfo<T>::VARIANT_TYPE;
        if (!Variant::can_convert_strict(p_args[p_arg_idx]->get_type(), argtype)) {
            r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
            r_error.argument = p_arg_idx;
            r_error.expected = argtype;
        }

        return (T)(*p_args[p_arg_idx]);
    }
};

template <class T>
struct VariantCasterAndValidate<T &> {
    static _FORCE_INLINE_ T cast(const Variant **p_args, uint32_t p_arg_idx, Callable::CallError &r_error) {
        VariantType argtype = GetTypeInfo<T>::VARIANT_TYPE;
        if (!Variant::can_convert_strict(p_args[p_arg_idx]->get_type(), argtype)) {
            r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
            r_error.argument = p_arg_idx;
            r_error.expected = argtype;
        }

        return(T)(*p_args[p_arg_idx]);
    }
};

template <class T>
struct VariantCasterAndValidate<const T &> {
    static _FORCE_INLINE_ T cast(const Variant **p_args, uint32_t p_arg_idx, Callable::CallError &r_error) {
        VariantType argtype = GetTypeInfo<T>::VARIANT_TYPE;
        if (!Variant::can_convert_strict(p_args[p_arg_idx]->get_type(), argtype)) {
            r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
            r_error.argument = p_arg_idx;
            r_error.expected = argtype;
        }

        return (T)(*p_args[p_arg_idx]);
    }
};

#endif // DEBUG_METHODS_ENABLED

// GCC 8 raises "parameter 'p_args' set but not used" here, probably using a
// template version that does not have arguments and thus sees it unused, but
// obviously the template can be used for functions with and without them, and
// the optimizer will get rid of it anyway.
#if defined(DEBUG_METHODS_ENABLED) && defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#endif

template <class T, class... P, size_t... Is>
void call_with_variant_args_helper(T *p_instance, void (T::*p_method)(P...), const Variant **p_args, Callable::CallError &r_error, eastl::index_sequence<Is...>) {
      r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
      (p_instance->*p_method)(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
      (p_instance->*p_method)(VariantCaster<P>::cast(*p_args[Is])...);
#endif
}

#if defined(DEBUG_METHODS_ENABLED) && defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

template <class T, class... P>
void call_with_variant_args(T *p_instance, void (T::*p_method)(P...), const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
#ifdef DEBUG_METHODS_ENABLED
    if ((size_t)p_argcount > sizeof...(P)) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
        r_error.argument = sizeof...(P);
        return;
    }

    if ((size_t)p_argcount < sizeof...(P)) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
        r_error.argument = sizeof...(P);
        return;
    }
#endif
    call_with_variant_args_helper<T, P...>(p_instance, p_method, p_args, r_error, eastl::make_index_sequence<sizeof...(P)>{});
}

template <class T, class... P>
class CallableCustomMethodPointer : public CallableCustomMethodPointerBase {
    struct Data {
        T *instance;
#ifdef DEBUG_ENABLED
        uint64_t object_id;
#endif
        void (T::*method)(P...);
    } data;

public:
    virtual ObjectID get_object() const {
#ifdef DEBUG_ENABLED
        if (ObjectDB::get_instance(ObjectID(data.object_id)) == nullptr) {
            return ObjectID();
        }
#endif
        return data.instance->get_instance_id();
    }

    virtual void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
#ifdef DEBUG_ENABLED
        ERR_FAIL_COND_MSG(ObjectDB::get_instance(ObjectID(data.object_id)) == nullptr, "Invalid Object id '" + StringUtils::num_uint64(data.object_id) + "', can't call method.");
#endif
        call_with_variant_args(data.instance, data.method, p_arguments, p_argcount, r_call_error);
    }

    CallableCustomMethodPointer(T *p_instance, void (T::*p_method)(P...)) {
        memset(&data,0, sizeof(Data)); // Clear beforehand, may have padding bytes.
        data.instance = p_instance;
#ifdef DEBUG_ENABLED
        data.object_id = p_instance->get_instance_id();
#endif
        data.method = p_method;
        _setup((uint32_t *)&data, sizeof(Data));
    }
};

template <class T, class... P>
Callable create_custom_callable_function_pointer(T *p_instance,
#ifdef DEBUG_METHODS_ENABLED
        const char *p_func_text,
#endif
        void (T::*p_method)(P...)) {

    typedef CallableCustomMethodPointer<T, P...> CCMP; // Messes with memnew otherwise.
    CCMP *ccmp = memnew(CCMP(p_instance, p_method));
#ifdef DEBUG_METHODS_ENABLED
    ccmp->set_text(p_func_text + 1); // Try to get rid of the ampersand.
#endif
    return Callable(ccmp);
}

// VERSION WITH RETURN

// GCC 8 raises "parameter 'p_args' set but not used" here, probably using a
// template version that does not have arguments and thus sees it unused, but
// obviously the template can be used for functions with and without them, and
// the optimizer will get rid of it anyway.
#if defined(DEBUG_METHODS_ENABLED) && defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#endif

template <class T, class R, class... P, size_t... Is>
void call_with_variant_args_ret_helper(T *p_instance, R (T::*p_method)(P...), const Variant **p_args, Variant &r_ret, Callable::CallError &r_error, eastl::index_sequence<Is...>) {
    r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
    r_ret = (p_instance->*p_method)(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
    (p_instance->*p_method)(VariantCaster<P>::cast(*p_args[Is])...);
#endif
}

#if defined(DEBUG_METHODS_ENABLED) && defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

template <class T, class R, class... P>
void call_with_variant_args_ret(T *p_instance, R (T::*p_method)(P...), const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
#ifdef DEBUG_METHODS_ENABLED
    if ((size_t)p_argcount > sizeof...(P)) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
        r_error.argument = sizeof...(P);
        return;
    }

    if ((size_t)p_argcount < sizeof...(P)) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
        r_error.argument = sizeof...(P);
        return;
    }
#endif
    call_with_variant_args_ret_helper<T, R, P...>(p_instance, p_method, p_args, r_ret, r_error, eastl::make_index_sequence<sizeof...(P)>{});
}

template <class T, class R, class... P>
class CallableCustomMethodPointerRet : public CallableCustomMethodPointerBase {
    struct Data {
        T *instance;
#ifdef DEBUG_ENABLED
        uint64_t object_id;
#endif
            R(T::*method)
            (P...);
    } data;

public:
    virtual ObjectID get_object() const {
#ifdef DEBUG_ENABLED
        if (ObjectDB::get_instance(ObjectID(data.object_id)) == nullptr) {
            return ObjectID();
        }
#endif
        return data.instance->get_instance_id();
    }

    virtual void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
#ifdef DEBUG_ENABLED
        ERR_FAIL_COND_MSG(ObjectDB::get_instance(ObjectID(data.object_id)) == nullptr, "Invalid Object id '" + StringUtils::num_uint64(data.object_id) + "', can't call method.");
#endif
        call_with_variant_args_ret(data.instance, data.method, p_arguments, p_argcount, r_return_value, r_call_error);
    }

    CallableCustomMethodPointerRet(T *p_instance, R (T::*p_method)(P...)) {
        memset(&data,0, sizeof(Data)); // Clear beforehand, may have padding bytes.
        data.instance = p_instance;
#ifdef DEBUG_ENABLED
        data.object_id = p_instance->get_instance_id();
#endif
        data.method = p_method;
        _setup((uint32_t *)&data, sizeof(Data));
    }
};

template <class T, class R, class... P>
Callable create_custom_callable_function_pointer(T *p_instance,
#ifdef DEBUG_METHODS_ENABLED
        const char *p_func_text,
#endif
        R (T::*p_method)(P...)) {

    typedef CallableCustomMethodPointerRet<T, R, P...> CCMP; // Messes with memnew otherwise.
    CCMP *ccmp = memnew(CCMP(p_instance, p_method));
#ifdef DEBUG_METHODS_ENABLED
    ccmp->set_text(p_func_text + 1); // Try to get rid of the ampersand.
#endif
    return Callable(ccmp);
}

#ifdef DEBUG_METHODS_ENABLED
#define callable_mp(I, M) create_custom_callable_function_pointer(I, #M, M)
#define callable_gen(I, M) Callable(memnew_args(FunctorCallable,I->get_instance_id(),M,__FILE__,__LINE__))
#else
#define callable_mp(I, M) create_custom_callable_function_pointer(I, M)
#define callable_gen(I, M) Callable(memnew_args(FunctorCallable,I->get_instance_id(),M))
#endif
