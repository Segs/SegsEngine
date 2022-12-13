/*************************************************************************/
/*  command_queue_mt.h                                                   */
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

#pragma once

#include "core/os/memory.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/typedefs.h"
#include "core/error_macros.h"

#include "EASTL/functional.h"

class GODOT_EXPORT CommandQueueMT {

    struct SyncSemaphore {

        Semaphore sem;
        bool in_use = false;
    };

    struct CommandBase {
        eastl::function<void()> callable;
        SyncSemaphore *sync_sem = nullptr;
        void call() {
            callable();
        }
        void post() {
            if(sync_sem)
                sync_sem->sem.post();
        }
    };

    enum {
        COMMAND_MEM_SIZE_KB = 256,
        COMMAND_MEM_SIZE = COMMAND_MEM_SIZE_KB * 1024,
        SYNC_SEMAPHORES = 8
    };

    uint8_t *command_mem = (uint8_t*)memalloc(COMMAND_MEM_SIZE);
    uint32_t read_ptr = 0;
    uint32_t write_ptr = 0;
    uint32_t dealloc_ptr = 0;
    SyncSemaphore sync_sems[SYNC_SEMAPHORES];
    Mutex mutex;
    Semaphore *sync = nullptr;

    CommandBase *allocate() {

        // alloc size is size+T+safeguard
        uint32_t alloc_size = ((sizeof(CommandBase) + 8 - 1) & ~(8 - 1)) + 8;

    tryagain:

        if (write_ptr < dealloc_ptr) {
            // behind dealloc_ptr, check that there is room
            if ((dealloc_ptr - write_ptr) <= alloc_size) {

                // There is no more room, try to deallocate something
                if (dealloc_one()) {
                    goto tryagain;
                }
                return nullptr;
            }
        } else {
            // ahead of dealloc_ptr, check that there is room

            if ((COMMAND_MEM_SIZE - write_ptr) < alloc_size + sizeof(uint32_t)) {
                // no room at the end, wrap down;

                if (dealloc_ptr == 0) { // don't want write_ptr to become dealloc_ptr

                    // There is no more room, try to deallocate something
                    if (dealloc_one()) {
                        goto tryagain;
                    }
                    return nullptr;
                }

                // if this happens, it's a bug
                ERR_FAIL_COND_V((COMMAND_MEM_SIZE - write_ptr) < 8, nullptr);
                // zero means, wrap to beginning

                uint32_t *p = (uint32_t *)&command_mem[write_ptr];
                *p = 0;
                write_ptr = 0;
                goto tryagain;
            }
        }
        // Allocate the size and the 'in use' bit.
        // First bit used to mark if command is still in use (1)
        // or if it has been destroyed and can be deallocated (0).
        uint32_t size = (sizeof(CommandBase) + 8 - 1) & ~(8 - 1);
        uint32_t *p = (uint32_t *)&command_mem[write_ptr];
        *p = (size << 1) | 1;
        write_ptr += 8;
        // allocate the command
        CommandBase *cmd = memnew_placement(&command_mem[write_ptr], CommandBase);
        write_ptr += size;
        return cmd;
    }

    CommandBase *allocate_and_lock() {

        lock();
        CommandBase *ret;

        while ((ret = allocate()) == nullptr) {

            unlock();
            // sleep a little until fetch happened and some room is made
            wait_for_flush();
            lock();
        }

        return ret;
    }

    bool flush_one(bool p_lock = true) {
        if (p_lock) {
            lock();
        }
    tryagain:

        // tried to read an empty queue
        if (read_ptr == write_ptr) {
            if (p_lock) {
                unlock();
            }
            return false;
        }

        uint32_t size_ptr = read_ptr;
        uint32_t size = *(uint32_t *)&command_mem[read_ptr] >> 1;

        if (size == 0) {
            //end of ringbuffer, wrap
            read_ptr = 0;
            goto tryagain;
        }

        read_ptr += 8;

        CommandBase *cmd = reinterpret_cast<CommandBase *>(&command_mem[read_ptr]);

        read_ptr += size;

        if (p_lock) unlock();
        cmd->call();
        if (p_lock) lock();

        cmd->post();
        cmd->~CommandBase();
        *(uint32_t *)&command_mem[size_ptr] &= ~1;

        if (p_lock) unlock();
        return true;
    }

    void lock() {
        mutex.lock();
    }

    void unlock() {
        mutex.unlock();
    }

    void wait_for_flush();
    SyncSemaphore *_alloc_sync_sem();
    bool dealloc_one();

public:

    void push(eastl::function<void()> func) {
        auto cmd = allocate_and_lock();
        cmd->callable = eastl::move(func);
        unlock();
        if (sync)
            sync->post();
    }

    void push_and_sync(eastl::function<void()> func) {
        SyncSemaphore *ss = _alloc_sync_sem();
        auto cmd = allocate_and_lock();
        cmd->callable = eastl::move(func);
        cmd->sync_sem = ss;
        unlock();
        if (sync)
            sync->post();
        ss->sem.wait();
        ss->in_use = false;
    }

    void wait_and_flush_one() {
        ERR_FAIL_COND(!sync);
        sync->wait();
        flush_one();
    }

    void flush_all() {

        //ERR_FAIL_COND();
        lock();
        while (flush_one(false)) {
        }
        unlock();
    }

    CommandQueueMT(bool p_sync);
    ~CommandQueueMT();
};
