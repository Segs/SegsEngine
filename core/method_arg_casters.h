#pragma once
#include "core/typedefs.h"
#include "core/variant.h"

template <class T>
struct PtrToArg ;

template <class T>
struct VariantCaster {

    static _FORCE_INLINE_ T cast(const Variant &p_variant) {

        return p_variant.as<T>();
    }
};

template <class T>
struct VariantCaster<T &> {

    static _FORCE_INLINE_ T cast(const Variant &p_variant) {

        return p_variant.as<T>();
    }
};

template <class T>
struct VariantCaster<const T &> {

    static _FORCE_INLINE_ T cast(const Variant &p_variant) {

        return p_variant.as<T>();
    }
};

#define _VC(m_idx) \
    (VariantCaster<P##m_idx>::cast((m_idx - 1) >= p_arg_count ? get_default_argument(m_idx - 1) : *p_args[m_idx - 1]))

