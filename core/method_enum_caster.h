#pragma once

#include "core/type_info.h"
#define VARIANT_ENUM_CAST(m_enum)                                     \
    MAKE_ENUM_TYPE_INFO(m_enum)                                       \
    template <>                                                       \
    struct VariantCaster<m_enum> {                                    \
                                                                      \
        static m_enum cast(const Variant &p_variant) { \
            return (m_enum)p_variant.as<int>();                       \
        }                                                             \
    };

#define VARIANT_NS_ENUM_CAST(ns,m_enum)                               \
    TEMPL_MAKE_ENUM_TYPE_INFO(m_enum,ns::m_enum)                      \
    template <>                                                       \
    struct VariantCaster<ns::m_enum> {                                \
                                                                      \
        static ns::m_enum cast(const Variant &p_variant) { \
            return (ns::m_enum)p_variant.as<int>();                       \
        }                                                             \
    };
