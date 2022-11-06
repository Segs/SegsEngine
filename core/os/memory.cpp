/*************************************************************************/
/*  memory.cpp                                                           */
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

#include "memory.h"

#include "core/safe_refcount.h"
#include "core/error_macros.h"
#include "core/external_profiler.h"

#include <cstdio>
#include <cstdlib>
#include <cassert>
#ifndef PAD_ALIGN
#define PAD_ALIGN 16 //must always be greater than this at much
#endif


#ifdef DEBUG_ENABLED
static SafeNumeric<uint64_t> mem_usage;
static SafeNumeric<uint64_t> max_usage;
#endif
#define CS_DEPTH 3
void *operator new(size_t p_size, const char *p_description) {

    return Memory::alloc(p_size, false);
}

void *operator new(size_t p_size, void *(*p_allocfunc)(size_t p_size)) {

    return p_allocfunc(p_size);
}

#ifdef _MSC_VER
void operator delete(void *p_mem, const char *p_description) {

    CRASH_NOW_MSG("Call to placement delete should not happen.");
}

void operator delete(void *p_mem, void *(*p_allocfunc)(size_t p_size)) {

    CRASH_NOW_MSG("Call to placement delete should not happen.");
}

void operator delete(void *p_mem, void *p_pointer, size_t check, const char *p_description) {

    CRASH_NOW_MSG("Call to placement delete should not happen.");
}
#endif

SafeNumeric<uint64_t> Memory::alloc_count {0};


void *Memory::alloc(size_t p_bytes, bool p_pad_align) {
#ifdef DEBUG_ENABLED
    bool prepad = true;
#else
    bool prepad = p_pad_align;
#endif

    void *mem = malloc(p_bytes + (prepad ? PAD_ALIGN : 0));

    assert(mem);

    alloc_count.increment();

    TRACE_ALLOC_S(mem, p_bytes + (prepad ? PAD_ALIGN : 0),CS_DEPTH);
    if (prepad) {
        uint64_t *s = (uint64_t *)mem;
        *s = p_bytes;

        uint8_t *s8 = (uint8_t *)mem;

#ifdef DEBUG_ENABLED
        uint64_t new_mem_usage = mem_usage.add(p_bytes);
        max_usage.exchange_if_greater(new_mem_usage);
#endif
        return s8 + PAD_ALIGN;
    } else {
        return mem;
    }
}

void *Memory::realloc(void *p_memory, size_t p_bytes, bool p_pad_align) {

    if (p_memory == nullptr) {
        return alloc(p_bytes, p_pad_align);
    }

    uint8_t *mem = (uint8_t *)p_memory;

#ifdef DEBUG_ENABLED
    bool prepad = true;
#else
    bool prepad = p_pad_align;
#endif

    if (prepad) {
        mem -= PAD_ALIGN;
        uint64_t *s = (uint64_t *)mem;

#ifdef DEBUG_ENABLED
        if (p_bytes > *s) {
            uint64_t new_mem_usage = mem_usage.add(p_bytes - *s);
            max_usage.exchange_if_greater(new_mem_usage);
        } else {
            mem_usage.sub(*s - p_bytes);
        }
#endif

        TRACE_FREE(mem);
        if (p_bytes == 0) {
            free(mem);
            return nullptr;
        } else {
            *s = p_bytes;
            void *prev_mem=mem;
            mem = (uint8_t *)::realloc(mem, p_bytes + PAD_ALIGN);
            TRACE_ALLOC_S(mem, p_bytes + PAD_ALIGN,CS_DEPTH); //
            assert(mem);
            if(unlikely(!mem)) {
                Memory::free(prev_mem);
                return nullptr;
            }

            s = (uint64_t *)mem;

            *s = p_bytes;

            return mem + PAD_ALIGN;
        }
    } else {

        TRACE_FREE(mem);
        void *prev_mem=mem;

        mem = (uint8_t *)::realloc(mem, p_bytes);
        TRACE_ALLOC_S(mem, p_bytes,CS_DEPTH);
        assert(mem != nullptr || p_bytes == 0);
        if(unlikely(mem == nullptr && p_bytes > 0)) {
            Memory::free(prev_mem);
            return nullptr;
        }

        return mem;
    }
}

void Memory::free(void *p_ptr, bool p_pad_align) {

    assert(p_ptr);
    if(unlikely(p_ptr == nullptr))
        return;

    uint8_t *mem = (uint8_t *)p_ptr;

#ifdef DEBUG_ENABLED
    bool prepad = true;
#else
    bool prepad = p_pad_align;
#endif

    alloc_count.decrement();

    if (prepad) {
        mem -= PAD_ALIGN;

#ifdef DEBUG_ENABLED
        uint64_t *s = (uint64_t *)mem;
        mem_usage.sub(*s);
#endif
    }
    ::free(mem);
    TRACE_FREE(mem);
}

uint64_t Memory::get_mem_available() {

    return ~0ULL;
}

uint64_t Memory::get_mem_usage() {
#ifdef DEBUG_ENABLED
    return max_usage.get();
#else
    return 0;
#endif
}

uint64_t Memory::get_mem_max_usage() {
#ifdef DEBUG_ENABLED
    return max_usage.get();
#else
    return 0;
#endif
}
//extern "C" const char* __asan_default_options() { return "detect_leaks=0"; }
//extern "C" int __lsan_is_turned_off() { return 1; }
