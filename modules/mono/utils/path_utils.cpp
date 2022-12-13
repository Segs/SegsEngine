/*************************************************************************/
/*  path_utils.cpp                                                       */
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

#include "path_utils.h"

#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/string_utils.h"
#include "core/ustring.h"

#ifdef WINDOWS_ENABLED
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define ENV_PATH_SEP ";"
#else
#include <limits.h>
#include <unistd.h>

#define ENV_PATH_SEP ":"
#endif

#include <stdlib.h>

namespace path {

//String find_executable(StringView p_name) {
//#ifdef WINDOWS_ENABLED
//    String path_ext= OS::get_singleton()->get_environment("PATHEXT");
//    Vector<StringView> exts = StringUtils::split(path_ext,ENV_PATH_SEP, false);
//#endif
//    String path=OS::get_singleton()->get_environment("PATH");
//    Vector<StringView> env_path = StringUtils::split(path,ENV_PATH_SEP, false);

//    if (env_path.empty())
//        return String();

//    for (auto & env_p : env_path) {
//        String p = path::join(env_p, p_name);

//#ifdef WINDOWS_ENABLED
//        for (auto ext : exts) {
//            String p2 = p + StringUtils::to_lower(ext); // lowercase to reduce risk of case mismatch warning

//            if (FileAccess::exists(p2))
//                return p2;
//        }
//#else
//        if (FileAccess::exists(p))
//            return p;
//#endif
//    }

//    return String();
//}

String cwd() {
#ifdef WINDOWS_ENABLED
    const DWORD expected_size = ::GetCurrentDirectoryW(0, nullptr);
    eastl::vector<wchar_t> wbuffer;

    wbuffer.resize((int)expected_size);
    if (::GetCurrentDirectoryW(expected_size, wbuffer.data()) == 0)
        return ".";
    String buffer = StringUtils::utf8(UIString::fromWCharArray(wbuffer.data(),wbuffer.size()-1));
    return PathUtils::simplify_path(buffer);
#else
    char buffer[PATH_MAX];
    if (::getcwd(buffer, sizeof(buffer)) == nullptr)
        return ".";

    String result(buffer);

    return PathUtils::simplify_path(result);
#endif
}

String abspath(StringView p_path) {
    if (PathUtils::is_abs_path(p_path)) {
        return PathUtils::simplify_path(p_path);
    } else {
        return PathUtils::simplify_path(path::join(path::cwd(), p_path));
    }
}

String realpath(StringView p_path) {
#ifdef WINDOWS_ENABLED
    // Open file without read/write access
    UIString str(StringUtils::from_utf8(p_path));
    auto win_api_str = str.toStdWString();
    HANDLE hFile = ::CreateFileW(win_api_str.c_str(), 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
        return String(p_path);

    const DWORD expected_size = ::GetFinalPathNameByHandleW(hFile, nullptr, 0, FILE_NAME_NORMALIZED);

    if (expected_size == 0) {
        ::CloseHandle(hFile);
        return String(p_path);
    }

    eastl::vector<wchar_t> wbuffer;

    wbuffer.resize((int)expected_size);
    ::GetFinalPathNameByHandleW(hFile, wbuffer.data(), expected_size, FILE_NAME_NORMALIZED);

    ::CloseHandle(hFile);
    String buffer = StringUtils::utf8(UIString::fromWCharArray(wbuffer.data(), wbuffer.size()));
    return PathUtils::simplify_path(buffer);
#elif UNIX_ENABLED
    String res(p_path);
    char *resolved_path = ::realpath(res.data(), nullptr);

    if (!resolved_path)
        return res;
    res = resolved_path;
    ::free(resolved_path);

    return res;
#endif
}

String join(StringView p_a, StringView p_b) {
    if (p_a.empty())
        return String(p_b);

    const char a_last = p_a[p_a.length() - 1];
    if ((a_last == '/' || a_last == '\\') ||
            (!p_b.empty() && (p_b[0] == '/' || p_b[0] == '\\'))) {
        return String(p_a) + p_b;
    }

    return String(p_a) + "/" + p_b;
}

String join(StringView p_a, StringView p_b, StringView p_c) {
    return path::join(path::join(p_a, p_b), p_c);
}

String join(StringView p_a, StringView p_b, StringView p_c, StringView p_d) {
    return path::join(path::join(path::join(p_a, p_b), p_c), p_d);
}

String relative_to_impl(StringView p_path, StringView p_relative_to) {
    // This function assumes arguments are normalized and absolute paths

    if (p_path.starts_with(p_relative_to)) {
        return String(p_path.substr(p_relative_to.length() + 1));
    } else {
        StringView base_dir = PathUtils::get_base_dir(p_relative_to);

        if (base_dir.length() <= 2 && (base_dir.empty() || base_dir.ends_with(":")))
            return String(p_path);

        return PathUtils::plus_file("..",relative_to_impl(p_path, base_dir));
    }
}

#ifdef WINDOWS_ENABLED
String get_drive_letter(StringView p_norm_path) {
    auto idx = p_norm_path.find(":/");
    if (idx != String::npos && idx < p_norm_path.find("/"))
        return String(p_norm_path.substr(0, idx + 1));
    return String();
}
#endif

String relative_to(StringView p_path, StringView p_relative_to) {
    String relative_to_abs_norm = abspath(p_relative_to);
    String path_abs_norm = abspath(p_path);

#ifdef WINDOWS_ENABLED
    if (get_drive_letter(relative_to_abs_norm) != get_drive_letter(path_abs_norm)) {
        return path_abs_norm;
    }
#endif

    return relative_to_impl(path_abs_norm, relative_to_abs_norm);
}

} // namespace path
