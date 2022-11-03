/*************************************************************************/
/*  method_bind.h                                                        */
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

#include "core/method_ptrcall.h"
#include "core/method_info.h"
#include "core/method_bind_interface.h"
#include "core/method_enum_caster.h"
#include "core/type_info.h"
#include "core/string_utils.h"
#include "core/math/vector3.h"

#include "EASTL/type_traits.h"

namespace ObjectNS
{
enum ConnectFlags : uint8_t;
template<class T>
T* cast_to(::Object *f);
} // end of ObjectNS declarations


// Object enum casts must go here
VARIANT_NS_ENUM_CAST(ObjectNS,ConnectFlags);

template <typename T>
struct VariantObjectClassChecker {
    static bool check(const Variant &) {
        return true;
    }
};

template <typename T>
struct VariantObjectClassChecker<T *> {
    static bool check(const Variant &p_variant) {
        Object *obj = (Object * )p_variant;
        T *node = p_variant.as<T *>();
        return node || !obj;
    }
};

// some helpers

VARIANT_ENUM_CAST(Vector3::Axis);

VARIANT_ENUM_CAST(Error);
VARIANT_ENUM_CAST(Margin);
VARIANT_ENUM_CAST(Corner);
VARIANT_ENUM_CAST(Orientation);
VARIANT_ENUM_CAST(HAlign);
VARIANT_ENUM_CAST(VAlign);
VARIANT_ENUM_CAST(PropertyHint);
VARIANT_ENUM_CAST(PropertyUsageFlags);
VARIANT_ENUM_CAST(MethodFlags);
VARIANT_ENUM_CAST(VariantType);

VARIANT_ENUM_CAST(Variant::Operator);

template <class T>
MethodBind *create_vararg_method_bind(Variant (T::*p_method)(const Variant **, int, Callable::CallError &), MethodInfo &&p_info, bool p_return_nil_is_variant) {

    auto *a = memnew((MethodBindVarArg<Variant,T>));
    a->set_method(p_method);
    a->set_method_info(eastl::move(p_info),p_return_nil_is_variant);
    return a;
}
template <class T>
MethodBind *create_vararg_method_bind(void (T::*p_method)(const Variant **, int, Callable::CallError &), MethodInfo &&p_info, bool p_return_nil_is_variant) {

    auto *a = memnew((MethodBindVarArg<void,T>));
    a->set_method(p_method);
    a->set_method_info(eastl::move(p_info),p_return_nil_is_variant);
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
        if (idx == I - 1) { // if idx == count-1
            return functor.template doit<typename std::tuple_element<I - 1,TS>::type,I-1>();
        }
        if constexpr(I!=1) {
            return visit_impl<I - 1>::template visit<TS,F>(idx,functor);
        }

        return typename F::Result{};
    }
};

template <>
struct visit_impl<0>
{
    template <typename TS, typename F>
    static constexpr typename F::Result visit(int,const F & /*functor*/) {
        CRASH_COND(true);
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
            ssize_t def_idx = ssize_t(default_args.size()) - IDX - 1;
            if (def_idx < 0 || def_idx >= ssize_t(default_args.size()))
                return &Variant::null_variant;
            else
                return &default_args[IDX];
        }
        return provided_args[IDX];
    }
};
class MethodBindVABase : public MethodBind {
protected:
    MethodBindVABase(const char *classname,int argc,bool returns
                 #ifdef DEBUG_METHODS_ENABLED
                     ,bool is_const
                 #endif
                     ) {
        instance_class_name = classname;
        set_argument_count(argc);
#ifdef DEBUG_METHODS_ENABLED
        _set_const(is_const);
#endif
        _set_returns(returns);
    }
};

template<class T, class RESULT,typename ...Args>
class MethodBindVA final : public MethodBindVABase {

    using MethodConst = RESULT (T::*)(Args...) const;
    using MethodNonconst = RESULT (T::*)(Args...);

    using TFunction = std::conditional_t<
        std::is_const_v<T>,
        RESULT (T::*)(Args...) const,
        RESULT (T::*)(Args...)
    >;

protected:
    template <std::size_t... Is>
    RESULT converting_call(T *instance, const Variant **p_args, int p_arg_count, eastl::index_sequence<Is...>) {

        if constexpr (sizeof...(Args) == 0) {
            // TODO: SEGS: add assertion p_arg_count==0
            (void)p_arg_count;
            (void)p_args;
            return (instance->*method)();
        } else {
            ArgumentWrapper wrap{ p_args ? p_args : nullptr, p_arg_count, default_arguments };
            return (instance->*method)(
                    (eastl::decay_t<typename std::tuple_element<Is, Params>::type>)*visit_at_ce<ArgumentWrapper, Args...>(Is, wrap)...);
        }
    }
    using Params = std::tuple<Args...>;
    // MethodBind interface
public:
    TFunction method;
    constexpr static bool (*verifiers[sizeof...(Args)+1])(const Variant &) = { // +1 is here because vs2017 requires constexpr array of non-zero size
        VariantObjectClassChecker<Args>::check ...
    };
    constexpr static const TypePassBy s_pass_type[sizeof...(Args) + 1] = {
        GetTypeInfo<typename eastl::conditional<eastl::is_same_v<void,RESULT>, bool , RESULT>::type >::PASS_BY,
        GetTypeInfo<Args>::PASS_BY ...
    };
    constexpr static RawPropertyInfo arg_infos[sizeof...(Args) + 1] = {
        GetTypeInfo<RESULT>::get_class_info(),
        GetTypeInfo<Args>::get_class_info() ...
    };
#ifdef DEBUG_METHODS_ENABLED
    constexpr static const GodotTypeInfo::Metadata s_metadata[sizeof...(Args)+1] = {
        GetTypeInfo<typename eastl::conditional<eastl::is_same_v<void,RESULT>, bool , RESULT>::type >::METADATA,
        GetTypeInfo<typename eastl::decay<Args>::type>::METADATA ...
    };

    Span<const GodotTypeInfo::Metadata> do_get_argument_meta() const override {
        return s_metadata;
    }
#endif
    Span<const TypePassBy> do_get_argument_passby() const override {
        return s_pass_type;
    }
    PropertyInfo _gen_argument_type_info(int p_arg) const override {
        return arg_infos[p_arg+1];
    }

public:
    Variant do_call(Object* p_object,const Variant** p_args,int p_arg_count, Callable::CallError& r_error) override {

        T *instance=ObjectNS::cast_to<T>(p_object);
        r_error.error=Callable::CallError::CALL_OK;
#ifdef DEBUG_METHODS_ENABLED

        ERR_FAIL_COND_V(!instance,Variant());
        if(!checkArgs(p_args,p_arg_count,verifiers,sizeof...(Args),r_error)) {
            return Variant::null_variant;
        }

#endif
        constexpr auto seq = eastl::index_sequence_for<Args...>();

        if constexpr(!eastl::is_same_v<void,RESULT>) {
            return Variant::from(converting_call(instance,p_args,p_arg_count,seq));
        }
        else
            converting_call(instance,p_args,p_arg_count,seq);

        return Variant::null_variant;
    }

    MethodBindVA (TFunction f) :
        MethodBindVABase(T::get_class_static(),sizeof...(Args),!eastl::is_same_v<void,RESULT>
#ifdef DEBUG_METHODS_ENABLED
                         ,eastl::is_const_v<T>
#endif
                         ) {
        method = f; // casting method to a basic Object::method()
#ifdef DEBUG_METHODS_ENABLED
        VariantType *argt = memnew_arr(VariantType, sizeof...(Args) + 1);
        constexpr VariantType arg_types[sizeof...(Args)+1] = { // +1 is here because vs2017 requires constexpr array of non-zero size
            GetTypeInfo<Args>::VARIANT_TYPE...,
        };
        memcpy(argt+1,arg_types,(sizeof...(Args))*sizeof(VariantType));
        if constexpr (eastl::is_same_v<void,RESULT>)
            argt[0] = VariantType::NIL;
        else
            argt[0] = GetTypeInfo<RESULT>::VARIANT_TYPE;
        argument_types = argt;
#endif
    }
    ~MethodBindVA()=default;
};
