#pragma once

#include "core/os/memory.h"
#include "core/hashfuncs.h"
#include "EASTL/string.h"

using se_string = eastl::basic_string<char,wrap_allocator>;
using se_string_view = eastl::basic_string_view<char>;

extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::basic_string<char, wrap_allocator>;

template<>
struct Hasher<se_string> {
    _FORCE_INLINE_ uint32_t operator()(const se_string &s) const { return hash_djb2_buffer((const uint8_t *)s.c_str(), s.length()); }
};

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
