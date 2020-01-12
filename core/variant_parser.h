/*************************************************************************/
/*  variant_parser.h                                                     */
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

#include "core/os/file_access.h"
#include "core/resource.h"
#include "core/variant.h"
#include "core/se_string.h"
#include "core/map.h"

using String = class QString;

class VariantParser {
public:
    struct Stream;

    using ParseResourceFunc = Error (*)(void *, Stream *, Ref<Resource> &, int &, se_string &);

    struct ResourceParser {

        void *userdata;
        ParseResourceFunc func;
        ParseResourceFunc ext_func;
        ParseResourceFunc sub_func;
    };

    enum TokenType {
        TK_CURLY_BRACKET_OPEN,
        TK_CURLY_BRACKET_CLOSE,
        TK_BRACKET_OPEN,
        TK_BRACKET_CLOSE,
        TK_PARENTHESIS_OPEN,
        TK_PARENTHESIS_CLOSE,
        TK_IDENTIFIER,
        TK_STRING,
        TK_NUMBER,
        TK_COLOR,
        TK_COLON,
        TK_COMMA,
        TK_PERIOD,
        TK_EQUAL,
        TK_EOF,
        TK_ERROR,
        TK_MAX
    };

    enum Expecting {

        EXPECT_OBJECT,
        EXPECT_OBJECT_KEY,
        EXPECT_COLON,
        EXPECT_OBJECT_VALUE,
    };

    struct Token {

        TokenType type;
        Variant value;
    };

    struct Tag {

        se_string name;
        Map<se_string, Variant> fields;
    };

private:
    static const char *tk_name[TK_MAX];

    template <class T>
    static Error _parse_construct(Stream *p_stream, PODVector<T> &r_construct, int &line, se_string &r_err_str);

    static Error _parse_dictionary(Dictionary &object, Stream *p_stream, int &line, se_string &r_err_str, ResourceParser *p_res_parser = nullptr);
    static Error _parse_array(Array &array, Stream *p_stream, int &line, se_string &r_err_str, ResourceParser *p_res_parser = nullptr);
    static Error _parse_tag(Token &token, Stream *p_stream, int &line, se_string &r_err_str, Tag &r_tag, ResourceParser *p_res_parser = nullptr, bool p_simple_tag = false);

public:
    static Error parse_tag(Stream *p_stream, int &line, se_string &r_err_str, Tag &r_tag, ResourceParser *p_res_parser = nullptr, bool p_simple_tag = false);
    static Error parse_tag_assign_eof(Stream *p_stream, int &line, se_string &r_err_str, Tag &r_tag, se_string &r_assign, Variant &r_value, ResourceParser *p_res_parser = nullptr, bool p_simple_tag = false);

    static Error parse_value(Token &token, Variant &value, Stream *p_stream, int &line, se_string &r_err_str, ResourceParser *p_res_parser = nullptr);
    static Error get_token(Stream *p_stream, Token &r_token, int &line, se_string &r_err_str);
    static Error parse(Stream *p_stream, Variant &r_ret, se_string &r_err_str, int &r_err_line, ResourceParser *p_res_parser = nullptr);

    static Stream *get_file_stream(FileAccess *f);
    static Stream *get_string_stream(const se_string &f);
    static void release_stream(Stream *s);
};

class VariantWriter {
public:
    using StoreStringFunc = Error (*)(void *, const se_string &);
    using EncodeResourceFunc = se_string (*)(void *, const RES &);

    static Error write(const Variant &p_variant, StoreStringFunc p_store_string_func, void *p_store_string_ud, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud);
    static Error write_to_string(const Variant &p_variant, se_string &r_string, EncodeResourceFunc p_encode_res_func = nullptr, void *p_encode_res_ud = nullptr);
};
