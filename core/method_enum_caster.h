#pragma once

#include "core/type_info.h"
#define VARIANT_ENUM_CAST(m_enum)                                     \
    MAKE_ENUM_TYPE_INFO(m_enum)                                       \
    template <>                                                       \
    struct VariantCaster<m_enum> {                                    \
                                                                      \
        static _FORCE_INLINE_ m_enum cast(const Variant &p_variant) { \
            return (m_enum)p_variant.as<int>();                       \
        }                                                             \
    };

