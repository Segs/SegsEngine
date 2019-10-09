#pragma once

namespace eastl {
    template <typename T, typename Allocator>
    class vector;
}

template <class T>
class Ref;

template <class T>
class PoolVector;

struct wrap_allocator;

template<class T>
using PODVector = eastl::vector<T,wrap_allocator>;
template <class T>
class Vector;
