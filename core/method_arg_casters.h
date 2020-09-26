#pragma once
#include "core/typedefs.h"
#include "core/variant.h"
#include "core/reference.h"

#include "EASTL/type_traits.h"

template <class T>
struct PtrToArg ;

template <class T>
struct VariantCaster {
    using ref = eastl::false_type;
    static _FORCE_INLINE_ T cast(const Variant &p_variant) {

        return p_variant.as<T>();
    }
};

template <class T>
struct VariantCaster<const Ref<T> &> {
    using ref = eastl::true_type;

    static _FORCE_INLINE_ Ref<T> cast(const Variant &p_variant) {
        return refFromVariant<T>(p_variant);
    }
};
template <class T>
struct VariantCaster<const Ref<T>> {
    using ref = eastl::true_type;

    static _FORCE_INLINE_ Ref<T> cast(const Variant& p_variant) {
        return refFromVariant<T>(p_variant);
    }
};

template <class T>
struct VariantCaster<const T &> {
    using ref = eastl::false_type;
    static _FORCE_INLINE_ T cast(const Variant& p_variant) {
        return p_variant.as<eastl::decay<T>::type>();
    }
};

template <>
struct VariantCaster<const Variant&> {

    static _FORCE_INLINE_ const Variant& cast(const Variant& p_variant) {

        return p_variant;
    }
};

//#define _VC(m_idx) \
//    (VariantCaster<P##m_idx>::cast((m_idx - 1) >= p_arg_count ? get_default_argument(m_idx - 1) : *p_args[m_idx - 1]))

