#pragma once

#include "core/os/memory.h"

#include "EASTL/deque.h"

template<class T>
using Deque = eastl::deque<T,wrap_allocator>;
