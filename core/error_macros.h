/*************************************************************************/
/*  error_macros.h                                                       */
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

#include "core/typedefs.h"
#include "core/forward_decls.h"
#include "EASTL/string_view.h"

/**
 * Error macros. Unlike exceptions and asserts, these macros try to maintain consistency and stability
 * inside the code. It is recommended to always return processable data, so in case of an error,
 * the engine can keep working well.
 * In most cases, bugs and/or invalid data are not fatal and should never allow a perfectly running application
 * to fail or crash.
 */


/** Function used by the error macros */

enum ErrorHandlerType {
    ERR_HANDLER_ERROR,
    ERR_HANDLER_WARNING,
    ERR_HANDLER_SCRIPT,
    ERR_HANDLER_SHADER,
};

struct ErrorHandlerList {
/**
 * Pointer to the error macro printing function. Reassign to any function to have errors printed
 */
    using ErrorHandlerFunc = void (*)(void *, se_string_view, se_string_view, int, se_string_view, se_string_view, ErrorHandlerType);

    ErrorHandlerFunc errfunc = nullptr;
    void *userdata = nullptr;
    ErrorHandlerList *next = nullptr;
};

GODOT_EXPORT void add_error_handler(ErrorHandlerList *p_handler);
GODOT_EXPORT void remove_error_handler(ErrorHandlerList *p_handler);
GODOT_EXPORT void _err_print_error(se_string_view p_function, se_string_view p_file, int p_line, se_string_view p_error, se_string_view p_message ={}, ErrorHandlerType p_type = ERR_HANDLER_ERROR);
GODOT_EXPORT void _err_print_index_error(se_string_view p_function, se_string_view p_file, int p_line, int64_t p_index, int64_t p_size, se_string_view p_index_str, se_string_view p_size_str, se_string_view p_message, bool fatal = false);


#ifndef _STR
#define _STR(m_x) #m_x
#define _MKSTR(m_x) _STR(m_x)
#endif

// Used to strip debug messages in release mode
#ifdef DEBUG_ENABLED
#define DEBUG_STR(m_msg) m_msg
#else
#define DEBUG_STR(m_msg) ""
#endif

#ifdef __GNUC__
//#define FUNCTION_STR __PRETTY_FUNCTION__ - too annoying
#define FUNCTION_STR __FUNCTION__
#else
#define FUNCTION_STR __FUNCTION__
#endif

// Don't use this directly; instead, use any of the CRASH_* macros
#ifdef _MSC_VER
#define GENERATE_TRAP                       \
    __debugbreak();                         \
    /* Avoid warning about control paths */ \
    for (;;) {                              \
    }
#else
#define GENERATE_TRAP __builtin_trap();
#endif

// (*): See https://stackoverflow.com/questions/257418/do-while-0-what-is-it-good-for
/**
 * If `m_index` is less than 0 or greater than or equal to `m_size`, prints a generic
 * error message and returns from the function. This macro should be preferred to
 * `ERR_FAIL_COND` for bounds checking.
 */
#define ERR_FAIL_INDEX(m_index, m_size)                                                                             \
    do {                                                                                                            \
        if (unlikely(decltype(m_size)(m_index) < 0 || decltype(m_size)(m_index) >= (m_size))) {                                                     \
            _err_print_index_error(FUNCTION_STR, __FILE__, __LINE__, m_index, int64_t(m_size), _STR(m_index), _STR(m_size),""); \
            return;                                                                                                 \
        }                                                                                                           \
    } while (0); // (*)

/**
 * If `m_index` is less than 0 or greater than or equal to `m_size`, prints a custom
 * error message and returns from the function. This macro should be preferred to
 * `ERR_FAIL_COND_MSG` for bounds checking.
 */
#define ERR_FAIL_INDEX_MSG(m_index, m_size, m_msg)                                                                         \
    do {                                                                                                                   \
        if (unlikely((m_index) < 0 || (m_index) >= (m_size))) {                                                            \
            _err_print_index_error(FUNCTION_STR, __FILE__, __LINE__, m_index, int64_t(m_size), _STR(m_index), _STR(m_size), DEBUG_STR(m_msg)); \
            return;                                                                                                        \
        }                                                                                                                  \
    } while (0); // (*)

/**
 * If `m_index` is less than 0 or greater than or equal to `m_size`,
 * prints a generic error message and returns the value specified in `m_retval`.
 * This macro should be preferred to `ERR_FAIL_COND_V` for bounds checking.
 */
#define ERR_FAIL_INDEX_V(m_index, m_size, m_retval)                                                                 \
    do {                                                                                                            \
        if (unlikely((m_index) < 0 || (m_index) >= (m_size))) {                                                     \
            _err_print_index_error(FUNCTION_STR, __FILE__, __LINE__, m_index, m_size, _STR(m_index), _STR(m_size),""); \
            return m_retval;                                                                                        \
        }                                                                                                           \
    } while (0)

/**
 * If `m_index` is less than 0 or greater than or equal to `m_size`,
 * prints a custom error message and returns the value specified in `m_retval`.
 * This macro should be preferred to `ERR_FAIL_COND_V_MSG` for bounds checking.
 */
#define ERR_FAIL_INDEX_V_MSG(m_index, m_size, m_retval, m_msg)                                                             \
    do {                                                                                                                   \
        if (unlikely((m_index) < 0 || (m_index) >= (m_size))) {                                                            \
            _err_print_index_error(FUNCTION_STR, __FILE__, __LINE__, m_index, m_size, _STR(m_index), _STR(m_size), DEBUG_STR(m_msg)); \
            return m_retval;                                                                                               \
        }                                                                                                                  \
    } while (0)

/**
 * If `m_index` is greater than or equal to `m_size`,
 * prints a generic error message and returns the value specified in `m_retval`.
 * This macro should be preferred to `ERR_FAIL_COND_V` for unsigned bounds checking.
 */
#define ERR_FAIL_UNSIGNED_INDEX_V(m_index, m_size, m_retval)                                                        \
    do {                                                                                                            \
        if (unlikely((m_index) >= (m_size))) {                                                                      \
            _err_print_index_error(FUNCTION_STR, __FILE__, __LINE__, m_index, m_size, _STR(m_index), _STR(m_size),""); \
            return m_retval;                                                                                        \
        }                                                                                                           \
    } while (0); // (*)

/**
 * If `m_index` is greater than or equal to `m_size`,
 * prints a custom error message and returns the value specified in `m_retval`.
 * This macro should be preferred to `ERR_FAIL_COND_V_MSG` for unsigned bounds checking.
 */
#define ERR_FAIL_UNSIGNED_INDEX_V_MSG(m_index, m_size, m_retval, m_msg)                                                    \
    do {                                                                                                                   \
        if (unlikely((m_index) >= (m_size))) {                                                                             \
            _err_print_index_error(FUNCTION_STR, __FILE__, __LINE__, m_index, m_size, _STR(m_index), _STR(m_size), DEBUG_STR(m_msg)); \
            return m_retval;                                                                                               \
        }                                                                                                                  \
    } while (0); // (*)

/**
 * If `m_index` is less than 0 or greater than or equal to `m_size`,
 * crashes the engine immediately with a generic error message.
 * Only use this if there's no sensible fallback (i.e. the error is unrecoverable).
 * This macro should be preferred to `CRASH_COND` for bounds checking.
 */
#define CRASH_BAD_INDEX(m_index, m_size)                                                                                      \
    do {                                                                                                                      \
        if (unlikely((m_index) < 0 || (m_index) >= (m_size))) {                                                               \
            _err_print_index_error(FUNCTION_STR, __FILE__, __LINE__, m_index, m_size, _STR(m_index), _STR(m_size), "", true); \
            GENERATE_TRAP                                                                                                     \
        }                                                                                                                     \
    } while (0)

/**
 * If `m_index` is less than 0 or greater than or equal to `m_size`,
 * crashes the engine immediately with a custom error message.
 * Only use this if there's no sensible fallback (i.e. the error is unrecoverable).
 * This macro should be preferred to `CRASH_COND` for bounds checking.
 */
#define CRASH_BAD_INDEX_MSG(m_index, m_size, m_msg)                                                                              \
    do {                                                                                                                         \
        if (unlikely((m_index) < 0 || (m_index) >= (m_size))) {                                                                  \
            _err_print_index_error(FUNCTION_STR, __FILE__, __LINE__, m_index, m_size, _STR(m_index), _STR(m_size), DEBUG_STR(m_msg), true); \
            GENERATE_TRAP                                                                                                        \
        }                                                                                                                        \
    } while (0)

/**
 * If `m_param` is `null`, prints a generic error message and returns from the function.
 */

#define ERR_FAIL_NULL(m_param)                                                                              \
    do {                                                                                                    \
        if (unlikely(!m_param)) {                                                                           \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Parameter ' \"" _STR(m_param) "\" ' is null."); \
            return;                                                                                         \
        }                                                                                                   \
    } while (0)

#define ERR_FAIL_NULL_MSG(m_param, m_msg)                                                                   \
do {                                                                                                        \
        if (unlikely(!m_param)) {                                                                           \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Parameter ' \"" _STR(m_param) "\" ' is null.",DEBUG_STR(m_msg)); \
            return;                                                                                         \
        }                                                                                                   \
    } while (0)

 /**
 * If `m_param` is `null`, prints a generic error message and returns the value specified in `m_retval`.
 */
#define ERR_FAIL_NULL_V(m_param, m_retval)                                                                  \
    do {                                                                                                    \
        if (unlikely(!m_param)) {                                                                           \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Parameter ' \"" _STR(m_param) "\" ' is null."); \
            return m_retval;                                                                                \
        }                                                                                                   \
    } while (0)

/**
 * If `m_param` is `null`, prints a custom error message and returns the value specified in `m_retval`.
 */
#define ERR_FAIL_NULL_V_MSG(m_param, m_retval, m_msg)                                                       \
    do {                                                                                                    \
        if (unlikely(!m_param)) {                                                                           \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Parameter ' \"" _STR(m_param) "\" ' is null.",DEBUG_STR(m_msg)); \
            return m_retval;                                                                                \
        }                                                                                                   \
    } while (0)

/**
 * If `m_cond` evaluates to `true`, prints a generic error message and returns from the function.
 */
#define ERR_FAIL_COND(m_cond)                                                                              \
    {                                                                                                      \
        if (unlikely(m_cond)) {                                                                            \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' \"" _STR(m_cond) "\" ' is true."); \
            return;                                                                                        \
        }                                                                                                  \
    }
/**
 * Prints a generic error message for given condition
 */
#define ERR_REPORT_COND(m_cond)\
    _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' \"" _STR(m_cond) "\" ' is true.");

/**
 * If `m_cond` evaluates to `true`, prints a custom error message and returns from the function.
 */
#define ERR_FAIL_COND_MSG(m_cond, m_msg)                                                                          \
    {                                                                                                             \
        if (unlikely(m_cond)) {                                                                                   \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' \"" _STR(m_cond) "\" ' is true.", DEBUG_STR(m_msg)); \
            return;                                                                                               \
        }                                                                                                         \
    }

/**
 * If `m_cond` evaluates to `true`, crashes the engine immediately with a generic error message.
 * Only use this if there's no sensible fallback (i.e. the error is unrecoverable).
 */
#define CRASH_COND(m_cond)                                                                                        \
    {                                                                                                             \
        if (unlikely(m_cond)) {                                                                                   \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "FATAL: Condition ' \"" _STR(m_cond) "\" ' is true."); \
            GENERATE_TRAP                                                                                         \
        }                                                                                                         \
    }

/**
 * If `m_cond` evaluates to `true`, crashes the engine immediately with a custom error message.
 * Only use this if there's no sensible fallback (i.e. the error is unrecoverable).
 */
#define CRASH_COND_MSG(m_cond, m_msg)                                                                                    \
    {                                                                                                                    \
        if (unlikely(m_cond)) {                                                                                          \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "FATAL: Condition ' \"" _STR(m_cond) "\" ' is true.", DEBUG_STR(m_msg)); \
            GENERATE_TRAP                                                                                                \
        }                                                                                                                \
    }

/**
 * If `m_cond` evaluates to `true`, prints a generic error message and returns the value specified in `m_retval`.
 */
#define ERR_FAIL_COND_V(m_cond, m_retval)                                                                                            \
    do {                                                                                                                         \
        if (unlikely(m_cond)) {                                                                                                      \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' \"" _STR(m_cond) "\" ' is true. returned: " _STR(m_retval)); \
            return m_retval;                                                                                                         \
        }                                                                                                                            \
    } while (0)
 /**
 * If `m_cond` evaluates to `true`, prints a custom error message and returns the value specified in `m_retval`.
 */
#define ERR_FAIL_COND_V_MSG(m_cond, m_retval, m_msg)                                                                                        \
    do {                                                                                                                                    \
        if (unlikely(m_cond)) {                                                                                                             \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' \"" _STR(m_cond) "\" ' is true. returned: " _STR(m_retval), DEBUG_STR(m_msg)); \
            return m_retval;                                                                                                                \
        }                                                                                                                                   \
    } while (0)

 /**
 * If `m_cond` evaluates to `true`, prints a custom error message and continues the loop the macro is located in.
 */
#define ERR_CONTINUE(m_cond)                                                                                             \
    {                                                                                                                    \
        if (unlikely(m_cond)) {                                                                                          \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' \"" _STR(m_cond) "\" ' is true. Continuing..:"); \
            continue;                                                                                                    \
        }                                                                                                                \
    }

/**
 * If `m_cond` evaluates to `true`, prints a custom error message and continues the loop the macro is located in.
 */
#define ERR_CONTINUE_MSG(m_cond, m_msg)                                                                                         \
    {                                                                                                                           \
        if (unlikely(m_cond)) {                                                                                                 \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' \"" _STR(m_cond) "\" ' is true. Continuing..:", DEBUG_STR(m_msg)); \
            continue;                                                                                                           \
        }                                                                                                                       \
    }

/**
 * If `m_cond` evaluates to `true`, prints a generic error message and breaks from the loop the macro is located in.
 */
#define ERR_BREAK(m_cond)                                                                                              \
    {                                                                                                                  \
        if (unlikely(m_cond)) {                                                                                        \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' \"" _STR(m_cond) "\" ' is true. Breaking..:"); \
            break;                                                                                                     \
        }                                                                                                              \
    }
/**
 * If `m_cond` evaluates to `true`, prints a custom error message and breaks from the loop the macro is located in.
 */
#define ERR_BREAK_MSG(m_cond, m_msg)                                                                                          \
    {                                                                                                                         \
        if (unlikely(m_cond)) {                                                                                               \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' \"" _STR(m_cond) "\" ' is true. Breaking..:", DEBUG_STR(m_msg)); \
            break;                                                                                                            \
        }                                                                                                                     \
    }

/**
 * Prints a generic error message and returns from the function.
 */
#define ERR_FAIL()                                                                     \
    {                                                                                  \
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Method/Function Failed."); \
        return;                                                                        \
    }

#define ERR_FAIL_MSG(m_msg)                                                                   \
    {                                                                                         \
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Method/Function Failed.", DEBUG_STR(m_msg)); \
        return;                                                                               \
    }

/** Print an error string and return with value
 */

#define ERR_FAIL_V(m_value)                                                                                       \
    {                                                                                                             \
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Method/Function Failed, returning: " __STR(m_value)); \
        return m_value;                                                                                           \
    }

#define ERR_FAIL_V_MSG(m_value, m_msg)                                                                                   \
    {                                                                                                                    \
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Method/Function Failed, returning: " __STR(m_value), DEBUG_STR(m_msg)); \
        return m_value;                                                                                                  \
    }
/** Use this one if there is no sensible fallback, that is, the error is unrecoverable.
 */

#define CRASH_NOW()                                                                           \
    {                                                                                         \
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "FATAL: Method/Function Failed."); \
        GENERATE_TRAP                                                                         \
    }

#define CRASH_NOW_MSG(m_msg)                                                                         \
    {                                                                                                \
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "FATAL: Method/Function Failed.", DEBUG_STR(m_msg)); \
        GENERATE_TRAP                                                                                \
    }


/** Print an error string.
 */

#define ERR_PRINT(m_string)                                           \
    {                                                                 \
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, m_string); \
    }
#define ERR_PRINTF(fmt,...)                                           \
    {                                                                 \
        _err_print_error(FUNCTION_STR, __FILE__, __LINE__, FormatVE(fmt,__VA_ARGS__)); \
    }

#define ERR_PRINT_ONCE(m_string)                                          \
    {                                                                     \
        static bool first_print = true;                                   \
        if (first_print) {                                                \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, m_string); \
            first_print = false;                                          \
        }                                                                 \
    }

/** Print a warning string.
 */

#define WARN_PRINT(m_string)                                                                   \
   do {                                                                                        \
         _err_print_error(FUNCTION_STR, __FILE__, __LINE__, m_string,{}, ERR_HANDLER_WARNING); \
   } while(0)

#define WARN_PRINT_ONCE(m_string)                                                              \
   do {                                                                                        \
        static bool first_print = true;                                                        \
        if (first_print) {                                                                     \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, m_string,{}, ERR_HANDLER_WARNING); \
            first_print = false;                                                               \
        }                                                                                      \
   } while(0)
 /**
 * Prints a custom deprecation warning message without returning.
 * This should be preferred to `WARN_PRINT` for deprecation warnings.
 */
#define WARN_DEPRECATED_MSG(m_msg)                                                                                                                               \
    {                                                                                                                                                            \
        static volatile bool warning_shown = false;                                                                                                              \
        if (!warning_shown) {                                                                                                                                    \
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "This method has been deprecated and will be removed in the future.", DEBUG_STR(m_msg), ERR_HANDLER_WARNING); \
            warning_shown = true;                                                                                                                                \
        }                                                                                                                                                        \
    }
