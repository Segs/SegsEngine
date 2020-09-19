/*************************************************************************/
/*  string_utils.cpp                                                     */
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

#include "string_utils.h"

#include "core/os/file_access.h"
#include "core/string.h"
#include "core/array.h"

#include "EASTL/vector_set.h"
#include <stdio.h>
#include <stdlib.h>

namespace {

int sfind(const String &p_text, int p_from) {
    if (p_from < 0)
        return -1;

    int src_len = 2;
    int len = p_text.length();

    if (len == 0)
        return -1;

    const char *src = p_text.data();

    for (int i = p_from; i <= (len - src_len); i++) {
        bool found = true;

        for (int j = 0; j < src_len; j++) {
            int read_pos = i + j;

            ERR_FAIL_COND_V(read_pos >= len, -1);

            switch (j) {
                case 0:
                    found = src[read_pos] == '%';
                    break;
                case 1: {
                    char c = src[read_pos];
                    found = src[read_pos] == 's' || (c >= '0' && c <= '4');
                    break;
                }
                default:
                    found = false;
            }

            if (!found) {
                break;
            }
        }

        if (found)
            return i;
    }

    return -1;
}
} // namespace

String sformat(const String &p_text, const Variant &p1, const Variant &p2, const Variant &p3, const Variant &p4, const Variant &p5) {
    if (p_text.length() < 2)
        return p_text;

    Array args;

    if (p1.get_type() != VariantType::NIL) {
        args.push_back(p1);

        if (p2.get_type() != VariantType::NIL) {
            args.push_back(p2);

            if (p3.get_type() != VariantType::NIL) {
                args.push_back(p3);

                if (p4.get_type() != VariantType::NIL) {
                    args.push_back(p4);

                    if (p5.get_type() != VariantType::NIL) {
                        args.push_back(p5);
                    }
                }
            }
        }
    }

    String new_string;

    int findex = 0;
    int search_from = 0;
    int result = 0;

    while ((result = sfind(p_text, search_from)) >= 0) {
        char c = p_text[result + 1];

        int req_index = (c == 's' ? findex++ : c - '0');

        new_string += p_text.substr(search_from, result - search_from);
        new_string += args[req_index].as<String>();
        search_from = result + 2;
    }

    new_string += p_text.substr(search_from, p_text.length() - search_from);

    return new_string;
}

#ifdef TOOLS_ENABLED

bool is_csharp_keyword(StringView p_name) {
    using namespace eastl;
    static eastl::vector_set<StringView,eastl::less<StringView>,EASTLAllocatorType,eastl::fixed_vector<StringView,79,false>>
    keywords;
    static bool initialized=false;
    if(!initialized){
        constexpr const char *kwords[] = {
            "abstract" ,"as" ,"base" ,"bool" ,
            "break" ,"byte" ,"case" ,"catch" ,
            "char" ,"checked" ,"class" ,"const" ,
            "continue" ,"decimal" ,"default" ,"delegate" ,
            "do" ,"double" ,"else" ,"enum" ,
            "event" ,"explicit" ,"extern" ,"false" ,
            "finally" ,"fixed" ,"float" ,"for" ,
            "forech" ,"goto" ,"if" ,"implicit" ,
            "in" ,"int" ,"interface" ,"internal" ,
            "is" ,"lock" ,"long" ,"namespace" ,
            "new" ,"null" ,"object" ,"operator" ,
            "out" ,"override" ,"params" ,"private" ,
            "protected" ,"public" ,"readonly" ,"ref" ,
            "return" ,"sbyte" ,"sealed" ,"short" ,
            "sizeof" ,"stackalloc" ,"static" ,"string" ,
            "struct" ,"switch" ,"this" ,"throw" ,
            "true" ,"try" ,"typeof" ,"uint" ,"ulong" ,
            "unchecked" ,"unsafe" ,"ushort" ,"using" ,
            "value", // contextual kw
            "virtual" ,"volatile" ,"void" ,"while",
        };
        for(const char *c : kwords)
            keywords.emplace(c);
        initialized=true;
    }
    // Reserved keywords
    return keywords.contains(p_name);
}

String escape_csharp_keyword(StringView p_name) {
    return is_csharp_keyword(p_name) ? String("@") + p_name : String(p_name);
}
#endif

Error read_all_file_utf8(StringView p_path, String &r_content) {

    Error err;
    String res = FileAccess::get_file_as_string(p_path,&err);
    ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot open file '" + p_path + "'.");

    r_content = res;
    return OK;
}

// TODO: Move to variadic templates once we upgrade to C++11

String str_format(const char *p_format, ...) {
    va_list list;

    va_start(list, p_format);
    String res = str_format(p_format, list);
    va_end(list);

    return res;
}
// va_copy was defined in the C99, but not in C++ standards before C++11.
// When you compile C++ without --std=c++<XX> option, compilers still define
// va_copy, otherwise you have to use the internal version (__va_copy).
#if !defined(va_copy)
#if defined(__GNUC__)
#define va_copy(d, s) __va_copy((d), (s))
#else
#define va_copy(d, s) ((d) = (s))
#endif
#endif

#if defined(MINGW_ENABLED) || defined(_MSC_VER) && _MSC_VER < 1900
#define gd_vsnprintf(m_buffer, m_count, m_format, m_args_copy) vsnprintf_s(m_buffer, m_count, _TRUNCATE, m_format, m_args_copy)
#define gd_vscprintf(m_format, m_args_copy) _vscprintf(m_format, m_args_copy)
#else
#define gd_vsnprintf(m_buffer, m_count, m_format, m_args_copy) vsnprintf(m_buffer, m_count, m_format, m_args_copy)
#define gd_vscprintf(m_format, m_args_copy) vsnprintf(NULL, 0, p_format, m_args_copy)
#endif

String str_format(const char *p_format, va_list p_list) {
    char *buffer = str_format_new(p_format, p_list);

    String res(buffer);
    memdelete_arr(buffer);

    return res;
}

char *str_format_new(const char *p_format, ...) {
    va_list list;

    va_start(list, p_format);
    char *res = str_format_new(p_format, list);
    va_end(list);

    return res;
}

char *str_format_new(const char *p_format, va_list p_list) {
    va_list list;

    va_copy(list, p_list);
    int len = gd_vscprintf(p_format, list);
    va_end(list);

    len += 1; // for the trailing '/0'

    char *buffer(memnew_arr(char, len));

    va_copy(list, p_list);
    gd_vsnprintf(buffer, len, p_format, list);
    va_end(list);

    return buffer;
}
