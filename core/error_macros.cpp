/*************************************************************************/
/*  error_macros.cpp                                                     */
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

#include "error_macros.h"

#include "core/io/logger.h"
#include "os/os.h"
#include "core/string.h"
#include "core/string_utils.h"

static ErrorHandlerList *error_handler_list = nullptr;

void add_error_handler(ErrorHandlerList *p_handler)
{
    _global_lock();
    p_handler->next = error_handler_list;
    error_handler_list = p_handler;
    _global_unlock();
}

void remove_error_handler(ErrorHandlerList *p_handler)
{
    _global_lock();

    ErrorHandlerList *prev = nullptr;
    ErrorHandlerList *l = error_handler_list;

    while (l)
    {
        if (l == p_handler)
        {
            if (prev) {
                prev->next = l->next;
            } else {
                error_handler_list = l->next;
            }
            break;
        }
        prev = l;
        l = l->next;
    }

    _global_unlock();
}

void _err_print_error(const char *p_function, const char *p_file, int p_line, StringView p_error, StringView p_message,
                      ErrorHandlerType p_type)
{
    if (OS::get_singleton()) {
        OS::get_singleton()->print_error(p_function, p_file, p_line, p_error, p_message, (Logger::ErrorType)p_type);
    } else {
        // Fallback if errors happen before OS init or after it's destroyed.
        StringView err_details = (!p_message.empty()) ? p_message : p_error;
        fprintf(stderr, "ERROR: %.*s\n   at: %s (%s:%i)\n", (int)err_details.size(), err_details.data(), p_function, p_file, p_line);
    }

    _global_lock();
    ErrorHandlerList *l = error_handler_list;
    while (l)
    {
        l->errfunc(l->userdata, p_function, p_file, p_line, p_error, p_message, p_type);
        l = l->next;
    }

    _global_unlock();
}

void _err_print_index_error(const char *p_function, const char *p_file, int p_line, int64_t p_index, int64_t p_size,
                            StringView p_index_str, StringView p_size_str, StringView p_message, bool fatal)
{
    const String fstr(fatal ? "FATAL: " : "");
    const String err(
        fstr + "Index " + p_index_str + " = " + itos(p_index) + " is out of bounds (" + p_size_str + " = " +
        itos(p_size) + ").");

    _err_print_error(p_function, p_file, p_line, err, p_message);
}
void _err_flush_stdout() {
    fflush(stdout);
}
