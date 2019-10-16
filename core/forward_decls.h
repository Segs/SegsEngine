#pragma once
#include "core/godot_export.h"
#include <stdint.h>

namespace eastl {
    template <typename T, typename Allocator>
    class vector;
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
}

template <class T>
class Ref;

template <class T>
class PoolVector;

template <class T>
struct Comparator;

class wrap_allocator;
class DefaultAllocator;

template <class T>
struct Comparator;

template<class T>
using PODVector = eastl::vector<T,wrap_allocator>;
template <class T>
class Vector;

template <class T, class A>
class List;

template <class T>
using DefList = class List<T,DefaultAllocator>;
template<class T>
using ListPOD = eastl::list<T,wrap_allocator>;
template <class T>
using Set = eastl::set<T, Comparator<T>, wrap_allocator>;
template <class K,class V>
using DefMap = eastl::map<K,V,Comparator<K>,wrap_allocator>;

using se_string = eastl::basic_string<char, wrap_allocator>;
using se_string_view = eastl::basic_string_view<char>;

template <class T> struct Hasher;
template <typename T>
struct HashMapComparatorDefault;

template <class TKey, class TData, class Hasher,
        class Comparator, uint8_t MIN_HASH_TABLE_POWER, uint8_t RELATIONSHIP>
class HashMap;
template <class TKey, class TData, class Hasher = Hasher<TKey>,
        class Comparator = HashMapComparatorDefault<TKey> >
using BaseHashMap = HashMap<TKey,TData,Hasher,Comparator,3,8>;
