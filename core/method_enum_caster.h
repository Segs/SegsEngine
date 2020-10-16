#pragma once

#include "core/type_info.h"
#define VARIANT_ENUM_CAST(m_enum) MAKE_ENUM_TYPE_INFO(m_enum)

#define VARIANT_NS_ENUM_CAST(ns,m_enum) TEMPL_MAKE_ENUM_TYPE_INFO(m_enum,ns::m_enum)
