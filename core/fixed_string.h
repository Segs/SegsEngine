#pragma once

#include "core/os/memory.h"
#include "EASTL/fixed_string.h"

template<int node_count, bool bEnableOverflow = true>
using TmpString = eastl::fixed_string<char,node_count,bEnableOverflow,wrap_allocator>;