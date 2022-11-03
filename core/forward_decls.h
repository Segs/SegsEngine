#pragma once
#include "core/godot_export.h"
#include <stdint.h>
#include <stddef.h>

namespace eastl {
    template<typename T>
    struct hash;
    template <typename T>
    struct equal_to;

    template <typename T, typename Allocator>
    class vector;
    template <typename T, typename Allocator, unsigned kDequeSubarraySize>
    class deque;
    template <class T, class A>
    class list;
    template <class K,class CMP, class A>
    class set;
    template <typename Key, typename T, typename Compare, typename Allocator>
    class map;
    template<class T,class Allocator>
    class basic_string;
    template<class T>
    class basic_string_view;
    template <typename T, size_t Extent>
    class span;
    template <typename Allocator, typename Element, typename Container >
    class bitvector;
    template <class TKey, class TData, class HashFunc, class CompareFunc,typename Allocator, bool bCacheHashCode>
    class hash_map;
    template <typename T>
    struct less;
    template <typename T, size_t nodeCount, bool bEnableOverflow, typename OverflowAllocator>
    class fixed_vector;
}

template <class T>
class Ref;

template <class T>
class PoolVector;


class wrap_allocator;
class Memory;
using DefaultAllocator = Memory;

template<class T>
using Vector = eastl::vector<T,wrap_allocator>;
template<class T>
using Dequeue = eastl::deque<T,wrap_allocator,((sizeof(T) <= 4) ? 64 : ((sizeof(T) <= 8) ? 32 : ((sizeof(T) <= 16) ? 16 : ((sizeof(T) <= 32) ? 8 : 4))))>;

template<class T,int N,bool GROWING=false>
using FixedVector = eastl::fixed_vector<T,N,GROWING,wrap_allocator>;

template <typename T,size_t sz=size_t(-1)>
using Span = eastl::span<T,sz>;

using BitVector = eastl::bitvector<wrap_allocator,size_t,Vector<size_t>>;

template<class T>
using List = eastl::list<T,wrap_allocator>;
template <class T>
using Set = eastl::set<T, eastl::less<T>, wrap_allocator>;

using String = eastl::basic_string<char, wrap_allocator>;
using StringView = eastl::basic_string_view<char>;

template <class T>
struct Hasher;

using UIString = class QString;

