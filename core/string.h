#pragma once

#include "core/reflection_macros.h"
#include "core/os/memory.h"
#include "core/hashfuncs.h"

#include "EASTL/string.h"

using String = eastl::basic_string<char,wrap_allocator>;

using StringView = eastl::basic_string_view<char>;
SE_OPAQUE_TYPE(String)
SE_OPAQUE_TYPE(StringView)
#ifdef GODOT_EXPORTS
// make a single instantiation of the template available for the engine build, but not to the projects using the engine
extern template class EXPORT_TEMPLATE_DECL eastl::basic_string<char, wrap_allocator>;
#endif
constexpr char c_cursor_marker(-1); // invalid utf8 char to symbolize a cursor in a string
GODOT_EXPORT extern const String null_string; // used to return 'null' string reference

template<>
struct Hasher<String> {
    _FORCE_INLINE_ uint32_t operator()(const String &s) const { return uint32_t(eastl::string_hash<String>()(s)); }
};
namespace StringUtils {

static inline uint32_t hash(StringView sv) {
    return uint32_t(eastl::hash<StringView>()(sv));
}
}
namespace eastl {
template <typename T> struct hash;
template <>
struct hash<String> {
    size_t operator()(const String &val) const { return eastl::string_hash<String>()(val); }
};
}

inline String to_string(int value)
{
    return String(String::CtorSprintf(), "%d", value);
}
inline String to_string(long value)
{
    return String(String::CtorSprintf(), "%ld", value);
}
inline String to_string(long long value)
{
    return String(String::CtorSprintf(), "%lld", value);
}
inline String to_string(unsigned value)
{
    return String(String::CtorSprintf(), "%u", value);
}
inline String to_string(unsigned long value)
{
    return String(String::CtorSprintf(), "%lu", value);
}
inline String to_string(unsigned long long value)
{
    return String(String::CtorSprintf(), "%llu", value);
}
inline String to_string(float value)
{
    return String(String::CtorSprintf(), "%f", value);
}
inline String to_string(double value)
{
    return String(String::CtorSprintf(), "%f", value);
}
inline String to_string(long double value)
{
    return String(String::CtorSprintf(), "%Lf", value);
}

