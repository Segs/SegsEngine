/*************************************************************************/
/*  pool_vector.cpp                                                      */
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

#include "pool_vector.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/os/mutex.h"

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) PoolVector<Vector2>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) PoolVector<Vector3>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) PoolVector<unsigned char>;


MemoryPool::Alloc *MemoryPool::allocs = nullptr;
MemoryPool::Alloc *MemoryPool::free_list = nullptr;
uint32_t MemoryPool::alloc_count = 0;
uint32_t MemoryPool::allocs_used = 0;
Mutex *MemoryPool::alloc_mutex = nullptr;

size_t MemoryPool::total_memory = 0;
size_t MemoryPool::max_memory = 0;

void MemoryPool::setup(uint32_t p_max_allocs) {

    allocs = memnew_arr(Alloc, p_max_allocs);
    alloc_count = p_max_allocs;
    allocs_used = 0;

    for (uint32_t i = 0; i < alloc_count - 1; i++) {

        allocs[i].free_list = &allocs[i + 1];
    }

    free_list = &allocs[0];

    alloc_mutex = memnew(Mutex);
}

void MemoryPool::cleanup() {

    memdelete_arr(allocs);
    memdelete(alloc_mutex);

    ERR_FAIL_COND_MSG(allocs_used > 0, "There are still MemoryPool allocs in use at exit!");
}

bool MemoryPool::do_alloc_block(MemoryPool::Alloc *&alloc)
{
    MemoryPool::alloc_mutex->lock();
    if (MemoryPool::allocs_used == MemoryPool::alloc_count) {
        MemoryPool::alloc_mutex->unlock();
        ERR_FAIL_V_MSG(false,"All memory pool allocations are in use, can't COW.");
    }

    MemoryPool::Alloc *old_alloc = alloc;

    //take one from the free list
    alloc = MemoryPool::free_list;
    MemoryPool::free_list = alloc->free_list;
    //increment the used counter
    MemoryPool::allocs_used++;

    //copy the alloc data
    alloc->size = old_alloc ? old_alloc->size : 0;
    alloc->refcount.init();
    alloc->lock = 0;

#ifdef DEBUG_ENABLED
    MemoryPool::total_memory += alloc->size;
    if (MemoryPool::total_memory > MemoryPool::max_memory) {
        MemoryPool::max_memory = MemoryPool::total_memory;
    }
#endif

    MemoryPool::alloc_mutex->unlock();
    alloc->mem = memalloc(alloc->size);
    return true;
}

void MemoryPool::releaseBlock(MemoryPool::Alloc *alloc) {
    memfree(alloc->mem);
    alloc->mem = nullptr;
    alloc->size = 0;

    alloc_mutex->lock();
    alloc->free_list = free_list;
    free_list = alloc;
    allocs_used--;
    alloc_mutex->unlock();
}

#ifdef DEBUG_ENABLED
void MemoryPool::doUpdate(int delta)
{
    alloc_mutex->lock();
    total_memory += delta;
    if (total_memory > max_memory) {
        max_memory = total_memory;
    }
    alloc_mutex->unlock();
}
#endif
