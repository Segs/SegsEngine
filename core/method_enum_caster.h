#pragma once

#include "core/type_info.h"
#ifdef PTRCALL_ENABLED

//TODO: SEGS: all enums are encoded as ints, try using underlying type helper here?

#define VARIANT_ENUM_CAST(m_enum)                                            \
    MAKE_ENUM_TYPE_INFO(m_enum)                                              \
    template <>                                                              \
    struct VariantCaster<m_enum> {                                           \
                                                                             \
        static _FORCE_INLINE_ m_enum cast(const Variant &p_variant) {        \
            return (m_enum)p_variant.operator int();                         \
        }                                                                    \
    };                                                                       \
    template <>                                                              \
    struct PtrToArg<m_enum> {                                                \
        _FORCE_INLINE_ static m_enum convert(const void *p_ptr) {            \
            return m_enum(*reinterpret_cast<const int *>(p_ptr));            \
        }                                                                    \
        _FORCE_INLINE_ static void encode(m_enum p_val, void *p_ptr) {       \
            *(int *)p_ptr = int(p_val);                                      \
        }                                                                    \
    };

#else

#define VARIANT_ENUM_CAST(m_enum)                                     \
    MAKE_ENUM_TYPE_INFO(m_enum)                                       \
    template <>                                                       \
    struct VariantCaster<m_enum> {                                    \
                                                                      \
        static _FORCE_INLINE_ m_enum cast(const Variant &p_variant) { \
            return (m_enum)p_variant.as<int>();                       \
        }                                                             \
    };

#endif
