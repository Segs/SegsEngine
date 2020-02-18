/*
 * SEGS - Super Entity Game Server Engine
 * http://www.segs.dev/
 * Copyright (c) 2006 - 2020 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License. See LICENSE_SEGS.md for details.
 */
#pragma once

#include "core/os/memory.h"
#include "EASTL/unordered_set.h"
#include "EASTL/fixed_hash_set.h"

template<class T>
struct Hasher;

template <class TKey, class HashFunc = eastl::hash<TKey>, class CompareFunc = eastl::equal_to<TKey>>
using HashSet = eastl::unordered_set<TKey, HashFunc, CompareFunc, wrap_allocator>;

