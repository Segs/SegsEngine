/*************************************************************************/
/*  method_bind.h                                                        */
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

#pragma once

#include "core/list.h"
#include "core/method_ptrcall.h"
#include "core/method_info.h"
#include "core/method_bind_interface.h"
#include "core/method_arg_casters.h"
#include "core/method_enum_caster.h"
#include "core/type_info.h"

#include <cstdio>
#include <type_traits>
#include <functional>

namespace ObjectNS
{
enum ConnectFlags : uint8_t;
template<class T>
T* cast_to(::Object *f);
} // end of ObjectNS declarations


// Object enum casts must go here
VARIANT_ENUM_CAST(ObjectNS::ConnectFlags);

template <typename T>
struct VariantObjectClassChecker {
    static _FORCE_INLINE_ bool check(const Variant &) {
        return true;
    }
};

template <>
struct VariantObjectClassChecker<Node *> {
    static _FORCE_INLINE_ bool check(const Variant &p_variant) {
        Object *obj = (Object *)p_variant;
        Node *node = (Node *)p_variant;
        return node || !obj;
    }
};

template <>
struct VariantObjectClassChecker<Control *> {
    static _FORCE_INLINE_ bool check(const Variant &p_variant) {
        Object *obj = p_variant;
        Control *control = p_variant;
        return control || !obj;
    }
};

// some helpers

VARIANT_ENUM_CAST(Vector3::Axis)

VARIANT_ENUM_CAST(Error)
VARIANT_ENUM_CAST(Margin)
VARIANT_ENUM_CAST(Corner)
VARIANT_ENUM_CAST(Orientation)
VARIANT_ENUM_CAST(HAlign)
VARIANT_ENUM_CAST(VAlign)
VARIANT_ENUM_CAST(PropertyHint)
VARIANT_ENUM_CAST(PropertyUsageFlags)
VARIANT_ENUM_CAST(MethodFlags)
VARIANT_ENUM_CAST(VariantType)

VARIANT_ENUM_CAST(Variant::Operator)

template <>
struct VariantCaster<char16_t> {
    static _FORCE_INLINE_ char16_t cast(const Variant &p_variant) {
        return (char16_t)p_variant.operator int();
    }
};
#ifdef PTRCALL_ENABLED
template <>
struct PtrToArg<QChar> {
    _FORCE_INLINE_ static QChar convert(const void *p_ptr) {
        return QChar(*reinterpret_cast<const uint32_t *>(p_ptr));
    }
    _FORCE_INLINE_ static void encode(QChar p_val, const void *p_ptr) {
        *(int *)p_ptr = p_val.unicode();
    }
};
#endif


template <class T>
MethodBind *create_vararg_method_bind(Variant (T::*p_method)(const Variant **, int, Variant::CallError &), const MethodInfo &p_info) {

    MethodBindVarArg<T> *a = memnew((MethodBindVarArg<T>));
    a->set_method(p_method);
    a->set_method_info(p_info);
    return a;
}

/*****************************************************************/
/// Warning - Lovecraftian horrors ahead
/*****************************************************************/

template <size_t I>
struct visit_impl
{
    template <typename TS, typename F>
    static constexpr typename F::Result visit(int idx,const F &functor)
    {
        if (idx == I - 1) // if idx == count-1
            return functor.template doit<typename std::tuple_element<I - 1,TS>::type,I-1>();
        if constexpr(I!=1)
            return visit_impl<I - 1>::template visit<TS,F>(idx,functor);

        return typename F::Result{};
    }
};

template <>
struct visit_impl<0>
{
    template <typename TS, typename F>
    static constexpr typename F::Result visit(int,const F & /*functor*/) {
        assert(false);
        return typename F::Result{};
    }
};
template <typename F,typename ...Args>
static constexpr typename F::Result visit_at_ce(int idx,F functor)
{
    return visit_impl<sizeof...(Args)>::template visit<std::tuple<Args...>,F>(idx,functor);
}
struct ArgumentWrapper {
    using Result = const Variant *;
    const Variant **provided_args=nullptr;
    const int p_arg_count=0;
    const Vector<Variant> &default_args={};

    template<class TS,int IDX>
    Result doit() const {
        if(IDX>=p_arg_count)
        {
            int def_idx = default_args.size() - IDX - 1;
            if (def_idx < 0 || def_idx >= default_args.size())
                return &Variant::null_variant;
            else
                return &default_args[IDX];
        }
        return provided_args[IDX];
    }
};
#ifdef DEBUG_METHODS_ENABLED

struct GetPropertyType {
    using Result = PropertyInfo;
    template<class TS,int IDX>
    Result static doit() noexcept {
        return GetTypeInfo<typename std::remove_cv<typename std::remove_reference<TS>::type>::type>::get_class_info();
    }
};
#endif
template<class T, class RESULT,typename ...Args>
class MethodBindVA final : public MethodBind {

    using MethodConst = RESULT (T::*)(Args...) const;
    using MethodNonconst = RESULT (T::*)(Args...);

    using TFunction = std::conditional_t<
        std::is_const_v<T>,
        RESULT (T::*)(Args...) const,
        RESULT (T::*)(Args...)
    >;

protected:
    template <std::size_t... Is>
    RESULT converting_call(T *instance, const Variant **p_args, int p_arg_count, std::index_sequence<Is...>) {

        if constexpr (sizeof...(Args) == 0) {
            // TODO: SEGS: add assertion p_arg_count==0
            (void)p_arg_count;
            (void)p_args;
            return std::invoke((TFunction)method, instance);
        } else {
            ArgumentWrapper wrap{ p_args ? p_args : nullptr, p_arg_count, default_arguments };
            return std::invoke((TFunction)method, instance,
                    VariantCaster<typename std::tuple_element<Is, Params>::type>::cast(
                            *visit_at_ce<ArgumentWrapper, Args...>(Is, wrap))...);
        }
    }
#ifdef PTRCALL_ENABLED
    template<std::size_t... Is>
    RESULT ptr_call(T *instance,const void** p_args, std::index_sequence<Is...>)
    {
        if constexpr(sizeof...(Args)==0)
        {
            (void)p_args;
            return std::invoke((TFunction)method,instance);
        }
        else
            return std::invoke((TFunction)method,instance,PtrToArg<typename std::tuple_element<Is,Params>::type>::convert(p_args[Is]) ...);
    }
#endif
    using Params = std::tuple<Args...>;
    // MethodBind interface
public:
    MethodNonconst method;
    constexpr static bool (*verifiers[sizeof...(Args)])(const Variant &) = {
        VariantObjectClassChecker<Args>::check ...
    };
#ifdef DEBUG_METHODS_ENABLED
    constexpr static GodotTypeInfo::Metadata s_metadata[sizeof...(Args)+1] = {
        GetTypeInfo<typename std::conditional<std::is_same_v<void,RESULT>, bool , RESULT>::type >::METADATA,
        GetTypeInfo<typename std::decay<Args>::type>::METADATA ...
    };
    GodotTypeInfo::Metadata do_get_argument_meta(int p_arg) const override {
        return s_metadata[p_arg+1];
    }
    PropertyInfo _gen_argument_type_info(int p_arg) const override {
        if(p_arg==-1) {
            if constexpr (!std::is_same_v<void,RESULT>) {
                return GetTypeInfo<RESULT>::get_class_info();
            }
            else
                return {};
        }
        if(p_arg<0 || size_t(p_arg)>= sizeof...(Args))
            return {};
        return visit_at_ce<GetPropertyType,Args...>(p_arg,GetPropertyType());
    }
#endif

public:
    Variant do_call(Object* p_object,const Variant** p_args,int p_arg_count, Variant::CallError& r_error) override {

        T *instance=ObjectNS::cast_to<T>(p_object);
        r_error.error=Variant::CallError::CALL_OK;
#ifdef DEBUG_METHODS_ENABLED

        ERR_FAIL_COND_V(!instance,Variant())
        if(!checkArgs(p_args,p_arg_count,verifiers,sizeof...(Args),r_error))
            return Variant::null_variant;

#endif
        auto seq = std::index_sequence_for<Args...>();
        static_assert (seq.size()==sizeof... (Args) );
        if constexpr(!std::is_same_v<void,RESULT>) {
            return Variant(converting_call(instance,p_args,p_arg_count,seq));
        }
        else
            converting_call(instance,p_args,p_arg_count,seq);

        return Variant::null_variant;
    }
#ifdef PTRCALL_ENABLED
    void ptrcall(Object*p_object,const void** p_args,void *r_ret) override {
        T *instance=ObjectNS::cast_to<T>(p_object);
        if constexpr(!std::is_same_v<void,RESULT>) {
            PtrToArg<RESULT>::encode( ptr_call(instance,p_args,std::index_sequence_for<Args...>{}) ,r_ret) ;
        }
        else
        {
            (void)r_ret;
            ptr_call(instance,p_args,std::index_sequence_for<Args...>{});
        }
    }
#endif
    MethodBindVA (TFunction f) {
        method = (MethodNonconst)f; // casting away const-ness of a method
        instance_class_name = T::get_class_static();
        set_argument_count(sizeof...(Args));
#ifdef DEBUG_METHODS_ENABLED
        _set_const(std::is_const_v<T>);

        VariantType *argt = memnew_arr(VariantType, sizeof...(Args) + 1);
        constexpr VariantType arg_types[sizeof...(Args)+1] = { // +1 is here because vs2017 requires constexpr array of non-zero size
            GetTypeInfo<Args>::VARIANT_TYPE...,
        };
        memcpy(argt+1,arg_types,(sizeof...(Args))*sizeof(VariantType));
        if constexpr (std::is_same_v<void,RESULT>)
            argt[0] = VariantType::NIL;
        else
            argt[0] = GetTypeInfo<RESULT>::VARIANT_TYPE;
        argument_types = argt;
#endif
        _set_returns(!std::is_same_v<void,RESULT>);

    }
};
