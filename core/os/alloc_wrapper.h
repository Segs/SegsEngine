#pragma once
#include "core/os/memory.h"

#include <limits>

template <class T>
class StdCowAlloc
{
public:
        using value_type = T;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        constexpr StdCowAlloc() = default;
        constexpr StdCowAlloc(StdCowAlloc&&) = default;
        constexpr StdCowAlloc(const StdCowAlloc&) = default;

        template<class U, class Alloc2> constexpr StdCowAlloc(const StdCowAlloc<U>&) { }
        template<class U, class Alloc2> constexpr bool operator==(const StdCowAlloc<U>&) const noexcept { return true; }
        template<class U, class Alloc2> constexpr bool operator!=(const StdCowAlloc<U>&) const noexcept { return false; }

        template<class U> class rebind { public: using other = StdCowAlloc<U>; };

        /** Allocate but don't initialize number elements of type T. */
        static T* allocate(const size_t num)
        {
                if (num == 0)
                        return nullptr;

                if (num > std::numeric_limits<size_t>::max() / sizeof(T))
                        return nullptr; // Error

                void* const pv = Memory::alloc_static(num * sizeof(T),true);
                if (!pv)
                        return nullptr; // Error

                return static_cast<T*>(pv);
        }

        /** Deallocate storage p of deleted elements. */
        static void deallocate(pointer p, size_type)
        {
                Memory::free_static(p,true);
        }

        static constexpr size_t max_size() { return std::numeric_limits<size_type>::max() / sizeof(T); }
        static constexpr void destroy(pointer p) { p->~T(); }

        template<class... Args>
        static void construct(pointer p, Args&&... args) { new(p) T(std::forward<Args>(args)...); }
};
