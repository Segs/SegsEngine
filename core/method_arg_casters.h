#pragma once
#include "core/typedefs.h"
#include "core/variant.h"
#include "core/reference.h"

#include "EASTL/type_traits.h"

//template <class T>
//struct VariantCaster {
//    static _FORCE_INLINE_ decltype(auto) cast(const Variant &p_variant) {
//        return (eastl::decay_t<T>)p_variant;
//    }
//};
//template <class T>
//struct VariantCaster<T *> {
//    static _FORCE_INLINE_ T *cast(const Variant &p_variant) {
//        return p_variant.as<T *>();
//    }
//};

//template <class T>
//struct VariantCaster<const Ref<T> &> {
//    static _FORCE_INLINE_ Ref<T> cast(const Variant &p_variant) {
//        return refFromVariant<T>(p_variant);
//    }
//};
//template <class T>
//struct VariantCaster<const Ref<T>> {
//    static _FORCE_INLINE_ Ref<T> cast(const Variant& p_variant) {
//        return refFromVariant<T>(p_variant);
//    }
//};
//template <class T>
//struct VariantCaster<Ref<T>> {

//    static _FORCE_INLINE_ Ref<T> cast(const Variant& p_variant) {
//        return refFromVariant<T>(p_variant);
//    }
//};

//#define _VC(m_idx) \
//    (VariantCaster<P##m_idx>::cast((m_idx - 1) >= p_arg_count ? get_default_argument(m_idx - 1) : *p_args[m_idx - 1]))

