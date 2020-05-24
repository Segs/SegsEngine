#pragma once

// Scoped allocators will most likely be used with placement new, e.g.:
//   Foo* pFoo = new( pFooAllocator->Alloc(sizeof(Foo)) ) Foo();
// To use placement new, we need to include the standard 'new' header.
#include <new>

namespace jl {

/**
 * An interface for very basic, stateful allocators. No array allocation.
 */
class ScopedAllocator
{
public:
    virtual ~ScopedAllocator() = default;
    virtual void *Alloc( size_t nBytes ) = 0;
    virtual void Free( void* pObject ) = 0;
};

} // namespace jl
