#include "core/se_string.h"

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::basic_string<char, wrap_allocator>;

const se_string null_se_string;

int Vsnprintf8(char* pDestination, size_t n, const char* pFormat, va_list arguments)
{
    #ifdef _MSC_VER
        return _vsnprintf(pDestination, n, pFormat, arguments);
    #else
        return vsnprintf(pDestination, n, pFormat, arguments);
    #endif
}
