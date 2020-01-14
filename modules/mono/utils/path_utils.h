/*************************************************************************/
/*  path_utils.h                                                         */
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

#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include "core/string_builder.h"
#include "core/ustring.h"

namespace path {

se_string join(se_string_view p_a, se_string_view p_b);
se_string join(se_string_view p_a, se_string_view p_b, se_string_view p_c);
se_string join(se_string_view p_a, se_string_view p_b, se_string_view p_c, se_string_view p_d);

se_string find_executable(const String &p_name);

/// Returns a normalized absolute path to the current working directory
se_string cwd();

/**
 * Obtains a normalized absolute path to p_path. Symbolic links are
 * not resolved. The path p_path might not exist in the file system.
 */
se_string abspath(se_string_view p_path);

/**
 * Obtains a normalized path to p_path with symbolic links resolved.
 * The resulting path might be either a relative or an absolute path.
 */
se_string realpath(se_string_view p_path);

se_string relative_to(se_string_view p_path, se_string_view p_relative_to);

} // namespace path

#endif // PATH_UTILS_H
