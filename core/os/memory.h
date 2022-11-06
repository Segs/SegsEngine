#pragma once

/*************************************************************************/
/*  memory.h                                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/


#include "core/godot_export.h"
#include "core/safe_refcount.h"
//#include "core/external_profiler.h"

#include "EASTL/type_traits.h"
#include <stdint.h>
#include <cstddef>

class GODOT_EXPORT Memory {

    static SafeNumeric<uint64_t> alloc_count;
public:
    Memory() = delete;

    static void *alloc(size_t p_bytes, bool p_pad_align);
    static void *alloc(size_t p_bytes) { return alloc(p_bytes,false); }
    static void *realloc(void *p_memory, size_t p_bytes, bool p_pad_align = false);
    static void free(void *p_ptr, bool p_pad_align = false);

    static uint64_t get_mem_available();
    static uint64_t get_mem_usage();
    static uint64_t get_mem_max_usage();
};

using DefaultAllocator = Memory;
class wrap_allocator
{
public:
    constexpr explicit wrap_allocator(const char* /*pName*/ = "") noexcept {}
    constexpr wrap_allocator(const wrap_allocator& x) noexcept = default;
    constexpr wrap_allocator(const wrap_allocator& /*x*/, const char* /*pName*/) noexcept {}

    constexpr wrap_allocator& operator=(const wrap_allocator& x) noexcept = default;

    void* allocate(size_t n, int /*flags*/ = 0) {
        void *res=Memory::alloc(n, false);
        //TRACE_ALLOC_N(res,n,"wrap_alloc");
        return res;
    }
    void* allocate(size_t n, size_t /*alignment*/, size_t /*offset*/, int /*flags*/ = 0) {
        void *res=Memory::alloc(n, false);
        //TRACE_ALLOC_N(res,n,"wrap_alloc");
        return res;
    }
    void  deallocate(void* p, size_t /*n*/) {
        //TRACE_FREE_N(p,"wrap_alloc");
        Memory::free(p, false);
    }

    constexpr inline bool operator==(const wrap_allocator&)
    {
        return true; // All allocators are considered equal, as they merely use global new/delete.
    }

    constexpr inline bool operator!=(const wrap_allocator&)
    {
        return false; // All allocators are considered equal, as they merely use global new/delete.
    }
    constexpr const char* get_name() const noexcept { return "wrap godot allocator"; }
    constexpr void        set_name(const char* /*pName*/) {}
};


GODOT_EXPORT void *operator new(size_t p_size, const char *p_description); ///< operator new that takes a description and uses MemoryStaticPool
GODOT_EXPORT void *operator new(size_t p_size, void *(*p_allocfunc)(size_t p_size)); ///< operator new that takes a description and uses MemoryStaticPool
GODOT_EXPORT void *operator new(size_t p_size, void *p_pointer, size_t check, const char *p_description); ///< operator new that takes a description and uses a pointer to the preallocated memory

#ifdef _MSC_VER
// When compiling with VC++ 2017, the above declarations of placement new generate many irrelevant warnings (C4291).
// The purpose of the following definitions is to muffle these warnings, not to provide a usable implementation of placement delete.
GODOT_EXPORT void operator delete(void *p_mem, const char *p_description);
GODOT_EXPORT void operator delete(void *p_mem, void *(*p_allocfunc)(size_t p_size));
GODOT_EXPORT void operator delete(void *p_mem, void *p_pointer, size_t check, const char *p_description);
#endif

#define memalloc(m_size) Memory::alloc(m_size)
#define memrealloc(m_mem, m_size) Memory::realloc(m_mem, m_size)
#define memfree(m_mem) Memory::free(m_mem)

inline void postinitialize_handler(void *) {}

template <class T>
inline T *_post_initialize(T *p_obj) {
    postinitialize_handler(p_obj);
    return p_obj;
}
inline void *operator new(size_t /*p_size*/, void *p_pointer, size_t /*check*/, const char * /*p_description*/) {
    //void *failptr=0;
    //ERR_FAIL_COND_V( check < p_size , failptr) /** bug, or strange compiler, most likely */;

    return p_pointer;
}
#define memnew_basic(m_class) (new (#m_class) m_class)
#define memnew_args_basic(m_class,...) (new (#m_class) m_class(__VA_ARGS__))
#define memnew_allocator_basic(m_class, m_allocator) (new (m_allocator::alloc) m_class)
#define memnew_placement_basic(m_placement, m_class) (new (m_placement, sizeof(m_class), "") m_class)
#define memnew_placement_args_basic(m_placement, m_class,...) (new (m_placement, sizeof(m_class), "") m_class(__VA_ARGS__))

#define memnew(m_class) _post_initialize(new (#m_class) m_class)
#define memnew_args(m_class,...) _post_initialize(new (#m_class) m_class(__VA_ARGS__))
#define memnew_allocator(m_class, m_allocator) _post_initialize(new (m_allocator::alloc) m_class)
#define memnew_placement(m_placement, m_class) _post_initialize(new (m_placement, sizeof(m_class), "") m_class)

inline void predelete_handler(void *) {
}

template <class T>
void memdelete(T *p_class) {

    if(!p_class) // follow standard delete x; logic.
        return;
    predelete_handler(p_class);
    if constexpr(!eastl::is_trivially_destructible_v<T>)
        p_class->~T();

    Memory::free(p_class, false);
}

template <class T, class A>
void memdelete_allocator(T *p_class) {

    predelete_handler(p_class);

    if constexpr(!eastl::is_trivially_destructible_v<T>)
        p_class->~T();

    A::free(p_class);
}
struct GODOT_EXPORT wrap_deleter {
    constexpr wrap_deleter() noexcept = default;
    template<class T>
    void operator()(T *v)  const noexcept {
        memdelete<T>(v);
    }
};
#define memnew_arr(m_class, m_count) memnew_arr_template<m_class>(m_count)

template <typename T>
T *memnew_arr_template(size_t p_elements, const char *p_descr = "") {

    if (p_elements == 0)
        return nullptr;
    /** overloading operator new[] cannot be done , because it may not return the real allocated address (it may pad the
    'element count' before the actual array). Because of that, it must be done by hand. This is the same strategy used by
    std::vector, and the PoolVector class, so it should be safe.*/

    size_t len = sizeof(T) * p_elements;
    uint64_t *mem = (uint64_t *)Memory::alloc(len, true);

    *(mem - 1) = p_elements;

    if constexpr (!eastl::is_trivially_constructible_v<T>) {
        T *elems = (T *)mem;

        /* call operator new */
        for (size_t i = 0; i < p_elements; i++) {
            new (&elems[i], sizeof(T), p_descr) T;
        }
    }

    return (T *)mem;
}

///**
// * Wonders of having own array functions, you can actually check the length of
// * an allocated-with memnew_arr() array
// */

template <typename T>
void memdelete_arr(T *p_class) {

    uint64_t *ptr = (uint64_t *)p_class;

     if constexpr (!eastl::is_trivially_destructible_v<T>) {
        uint64_t elem_count = *(ptr - 1);

        for (uint64_t i = 0; i < elem_count; i++) {
            p_class[i].~T();
        }
    }

    Memory::free(ptr, true);
}

