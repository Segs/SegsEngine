/*************************************************************************/
/*  semaphore_windows.cpp                                                */
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

#include "semaphore_windows.h"

#if defined(WINDOWS_ENABLED)

#include "core/os/memory.h"
#include "core/error_macros.h"

Error SemaphoreWindows::wait() {

    WaitForSingleObjectEx(semaphore, INFINITE, false);
    return OK;
}
Error SemaphoreWindows::post() {

    ReleaseSemaphore(semaphore, 1, nullptr);
    return OK;
}
int SemaphoreWindows::get() const {
    long previous;
    switch (WaitForSingleObjectEx(semaphore, 0, false)) {
        case WAIT_OBJECT_0: {
            ERR_FAIL_COND_V(!ReleaseSemaphore(semaphore, 1, &previous), -1);
            return previous + 1;
        }
        case WAIT_TIMEOUT: {
            return 0;
        }
        default: {
        }
    }

    ERR_FAIL_V(-1);
}

SemaphoreOld *SemaphoreWindows::create_semaphore_windows() {

    return memnew(SemaphoreWindows);
}

void SemaphoreWindows::make_default() {

    create_func = create_semaphore_windows;
}

SemaphoreWindows::SemaphoreWindows() {

#ifdef UWP_ENABLED
    semaphore = CreateSemaphoreEx(
            nullptr,
            0,
            0xFFFFFFF, //wathever
            nullptr,
            0,
            SEMAPHORE_ALL_ACCESS);
#else
    semaphore = CreateSemaphore(
            nullptr,
            0,
            0xFFFFFFF, //wathever
            nullptr);
#endif
}

SemaphoreWindows::~SemaphoreWindows() {

    CloseHandle(semaphore);
}

#endif
