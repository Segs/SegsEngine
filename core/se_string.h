#pragma once

#include "core/os/memory.h"
#include "core/hashfuncs.h"
#include "EASTL/string.h"
#include "EASTL/fixed_string.h"

using se_string = eastl::basic_string<char,wrap_allocator>;

template<int node_count, bool bEnableOverflow = true>
using se_tmp_string = eastl::fixed_string<char,node_count,bEnableOverflow,wrap_allocator>;

using se_string_view = eastl::basic_string_view<char>;

constexpr char c_cursor_marker(-1); // invalid utf8 char to symbolize a cursor in a string
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::basic_string<char, wrap_allocator>;
extern const se_string null_se_string; // used to return 'null' string reference

template<>
struct Hasher<se_string> {
    _FORCE_INLINE_ uint32_t operator()(const se_string &s) const { return uint32_t(eastl::string_hash<se_string>()(s)); }
};
namespace StringUtils {

static inline uint32_t hash(se_string_view sv) {
    return uint32_t(eastl::hash<se_string_view>()(sv));
}
}
namespace eastl {
template <typename T> struct hash;
template <>
struct hash<se_string> {
    size_t operator()(const se_string &val) const { return eastl::string_hash<se_string>()(val); }
};
}

inline se_string to_string(int value)
{
    return se_string(se_string::CtorSprintf(), "%d", value);
}
inline se_string to_string(long value)
{
    return se_string(se_string::CtorSprintf(), "%ld", value);
}
inline se_string to_string(long long value)
{
    return se_string(se_string::CtorSprintf(), "%lld", value);
}
inline se_string to_string(unsigned value)
{
    return se_string(se_string::CtorSprintf(), "%u", value);
}
inline se_string to_string(unsigned long value)
{
    return se_string(se_string::CtorSprintf(), "%lu", value);
}
inline se_string to_string(unsigned long long value)
{
    return se_string(se_string::CtorSprintf(), "%llu", value);
}
inline se_string to_string(float value)
{
    return se_string(se_string::CtorSprintf(), "%f", value);
}
inline se_string to_string(double value)
{
    return se_string(se_string::CtorSprintf(), "%f", value);
}
inline se_string to_string(long double value)
{
    return se_string(se_string::CtorSprintf(), "%Lf", value);
}

