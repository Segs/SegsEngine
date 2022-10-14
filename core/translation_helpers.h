#pragma once

#include "core/forward_decls.h"
#include <EASTL/string_view.h>
class StringName;
// Tool translate (TTR and variants) for the editor UI,
// and doc translate for the class reference (DTR).
#ifdef TOOLS_ENABLED

// Gets parsed.
GODOT_EXPORT StringName TTR(StringView, StringView = "");
GODOT_EXPORT String TTRS(StringView, StringView = "");
GODOT_EXPORT StringName DTR(StringView);

// Use for C strings.
#define TTRC(m_value) (m_value)
// Use to avoid parsing (for use later with C strings).
#define TTRGET(m_value) TTR(m_value)

#else

#define TTR(m_value) (m_value)
#define DTR(m_value) (String())
#define TTRC(m_value) (m_value)
#define TTRGET(m_value) (m_value)
#define TTRS(m_value) (m_value)

#endif

// Runtime translate for the public node API.
GODOT_EXPORT StringName RTR(const char *);
GODOT_EXPORT String RTR_utf8(StringView sv);
