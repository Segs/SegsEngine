#pragma once
#include "core/typedefs.h"

struct ReflectionData;
class DocData;
enum class ReflectionSource {
    Core,
    Editor
};
#if defined(DEBUG_METHODS_ENABLED) && defined(TOOLS_ENABLED)

void _initialize_reflection_data(ReflectionData& rd,ReflectionSource src);

#endif
