#include "core/string.h"
#include "core/vector.h"

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::basic_string<char, wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<String,wrap_allocator>;

GODOT_EXPORT const String null_string;
const Vector<String> null_string_pvec;

int Vsnprintf8(char* pDestination, size_t n, const char* pFormat, va_list arguments)
{
    #ifdef _MSC_VER
        return _vsnprintf(pDestination, n, pFormat, arguments);
    #else
        return vsnprintf(pDestination, n, pFormat, arguments);
    #endif
}
