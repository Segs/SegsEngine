#pragma once

#include "core/forward_decls.h"

using UIString = class QString;
namespace StringUtils {
    [[nodiscard]] String to_utf8(const UIString &s);
}
//tool translate
#ifdef TOOLS_ENABLED

//gets parsed
StringName TTR(StringView);
#define TTR_utf8(v) StringUtils::to_utf8(TTR(v))

//use for C strings
#define TTRC(m_value) (m_value)
//use to avoid parsing (for use later with C strings)
#define TTRGET(m_value) TTR(m_value)

#else

#define TTR(m_value) (m_value)
#define TTRC(m_value) (m_value)
#define TTRGET(m_value) (m_value)

#endif

//tool or regular translate
StringName RTR(const char *);
String RTR_utf8(StringView sv);
