/*************************************************************************/
/*  variant_parser.cpp                                                   */
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

#include "variant_parser.h"

#include "core/dictionary.h"
#include "core/class_db.h"
#include "core/color.h"
#include "core/fixed_string.h"
#include "core/io/resource_loader.h"
#include "core/list.h"
#include "core/map.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/face3.h"
#include "core/math/math_funcs.h"
#include "core/math/plane.h"
#include "core/math/quat.h"
#include "core/math/transform.h"
#include "core/math/transform_2d.h"
#include "core/math/vector3.h"
#include "core/node_path.h"
#include "core/input/input_event.h"
#include "core/os/keyboard.h"
#include "core/pool_vector.h"
#include "core/property_info.h"
#include "core/resource/resource_manager.h"
#include "core/string_utils.inl"
#include "core/ustring.h"
#include "core/variant.h"

#include "EASTL/sort.h"
#include <cmath>

struct StreamFile : public VariantParserStream {

    FileAccess *f;

    char get_char() override;
    bool is_utf8() const override;
    bool is_eof() const override;

    StreamFile(FileAccess *fl = nullptr) : f(fl) {}
};

struct StreamString : public VariantParserStream {

    String s;
    int pos=0;

    char get_char() override;
    bool is_utf8() const override;
    bool is_eof() const override;

    StreamString(const String &str) : s(str) {}
    StreamString(String &&str) noexcept : s(eastl::move(str)) {}
};

char StreamFile::get_char() {

    return f->get_8();
}

bool StreamFile::is_utf8() const {

    return true;
}
bool StreamFile::is_eof() const {

    return f->eof_reached();
}

char StreamString::get_char() {
    if (pos > s.length()) {
        return 0;
    } else if (pos == s.length()) {
        // You need to try to read again when you have reached the end for EOF to be reported,
        // so this works the same as files (like StreamFile does)
        pos++;
        return 0;
    } else {
        return s[pos++];
    }
}

bool StreamString::is_utf8() const {
    return false;
}
bool StreamString::is_eof() const {
    return pos > s.length();
}

/////////////////////////////////////////////////////////////////////////////////////////////////

const char *VariantParser::tk_name[TK_MAX] = {
    "'{'",
    "'}'",
    "'['",
    "']'",
    "'('",
    "')'",
    "identifier",
    "string",
    "string_name",
    "number",
    "color",
    "':'",
    "','",
    "'.'",
    "'='",
    "EOF",
    "ERROR"
};

static double stor_fix(const String &p_str) {
    if (p_str == "inf") {
        return Math_INF;
    } else if (p_str == "inf_neg") {
        return -Math_INF;
    } else if (p_str == "nan") {
        return Math_NAN;
    }
    return -1;
}
Error VariantParser::get_token(VariantParserStream *p_stream, Token &r_token, int &line, String &r_err_str) {
    TmpString<128> tmp_str_buf; // static variable to prevent constat alloc/dealloc

    while (true) {

        char cchar;
        if (p_stream->saved) {
            cchar = p_stream->saved;
            p_stream->saved = 0;
        } else {
            cchar = p_stream->get_char();
            if (p_stream->is_eof()) {
                r_token.type = TK_EOF;
                return OK;
            }
        }

        switch (cchar) {

            case '\n': {

                line++;
                break;
            }
            case 0: {
                r_token.type = TK_EOF;
                return OK;
            }
            case '{': {

                r_token.type = TK_CURLY_BRACKET_OPEN;
                return OK;
            }
            case '}': {

                r_token.type = TK_CURLY_BRACKET_CLOSE;
                return OK;
            }
            case '[': {

                r_token.type = TK_BRACKET_OPEN;
                return OK;
            }
            case ']': {

                r_token.type = TK_BRACKET_CLOSE;
                return OK;
            }
            case '(': {

                r_token.type = TK_PARENTHESIS_OPEN;
                return OK;
            }
            case ')': {

                r_token.type = TK_PARENTHESIS_CLOSE;
                return OK;
            }
            case ':': {

                r_token.type = TK_COLON;
                return OK;
            }
            case ';': {

                while (true) {
                    CharType ch = p_stream->get_char();
                    if (p_stream->is_eof()) {
                        r_token.type = TK_EOF;
                        return OK;
                    }
                    if (ch == '\n') {
                        line++;
                        break;
                    }
                }

                break;
            }
            case ',': {

                r_token.type = TK_COMMA;
                return OK;
            }
            case '.': {

                r_token.type = TK_PERIOD;
                return OK;
            }
            case '=': {

                r_token.type = TK_EQUAL;
                return OK;
            }
            case '#': {

                tmp_str_buf = "#";
                while (true) {
                    char ch = p_stream->get_char();
                    if (p_stream->is_eof()) {
                        r_token.type = TK_EOF;
                        return OK;
                    } else if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
                        tmp_str_buf.push_back(ch);

                    } else {
                        p_stream->saved = ch;
                        break;
                    }
                }

                r_token.value = Color::html(tmp_str_buf);
                r_token.type = TK_COLOR;
                return OK;
            }
            case '@': {
                cchar = p_stream->get_char();
                if (cchar != '"') {
                    r_err_str = "Expected '\"' after '@'";
                    r_token.type = TK_ERROR;
                    return ERR_PARSE_ERROR;
                }
                [[fallthrough]];
            }
            case '"': {

                String str;
                while (true) {

                    char ch = p_stream->get_char();

                    if (ch == 0) {
                        r_err_str = "Unterminated String";
                        r_token.type = TK_ERROR;
                        return ERR_PARSE_ERROR;
                    } else if (ch == '"') {
                        break;
                    } else if (ch == '\\') {
                        //escaped characters...
                        char next = p_stream->get_char();
                        if (next == 0) {
                            r_err_str = "Unterminated String";
                            r_token.type = TK_ERROR;
                            return ERR_PARSE_ERROR;
                        }
                        char res = 0;

                        switch (next) {

                            case 'b': res = 8; break;
                            case 't': res = 9; break;
                            case 'n': res = 10; break;
                            case 'f': res = 12; break;
                            case 'r': res = 13; break;
                            case 'u': {
                                //hexnumbarh - oct is deprecated
                                uint16_t code=0;
                                for (int j = 0; j < 4; j++) {
                                    CharType c = p_stream->get_char();
                                    if (c.isNull()) {
                                        r_err_str = "Unterminated String";
                                        r_token.type = TK_ERROR;
                                        return ERR_PARSE_ERROR;
                                    }
                                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {

                                        r_err_str = "Malformed hex constant in string";
                                        r_token.type = TK_ERROR;
                                        return ERR_PARSE_ERROR;
                                    }
                                    int v;
                                    if (c.isDigit()) {
                                        v = c.digitValue();
                                    } else if (c >= 'a' && c <= 'f') {
                                        v = c.toLatin1() - 'a';
                                        v += 10;
                                    } else if (c >= 'A' && c <= 'F') {
                                        v = c.toLatin1() - 'A';
                                        v += 10;
                                    } else {
                                        ERR_PRINT("BUG");
                                        v = 0;
                                    }

                                    code <<= 4;
                                    code |= v;
                                }
                                str += StringUtils::to_utf8(QChar(code));
                            } break;
                            //case '\"': res='\"'; break;
                            //case '\\': res='\\'; break;
                            //case '/': res='/'; break;
                            default: {
                                res = next;
                                //r_err_str="Invalid escape sequence";
                                //return ERR_PARSE_ERROR;
                            } break;
                        }
                        if(res)
                            str += res;

                    } else {
                        if (ch == '\n')
                            line++;
                        str += ch;
                    }
                }

                r_token.type = TK_STRING;
                r_token.value = str;
                return OK;

            }
            default: {

                if (cchar <= 32) {
                    break;
                }

                if (cchar == '-' || (cchar >= '0' && cchar <= '9')) {
                    //a number

                    tmp_str_buf="";
#define READING_SIGN 0
#define READING_INT 1
#define READING_DEC 2
#define READING_EXP 3
#define READING_DONE 4
                    int reading = READING_INT;

                    if (cchar == '-') {
                        tmp_str_buf += '-';
                        cchar = p_stream->get_char();
                    }

                    char c = cchar;
                    bool exp_sign = false;
                    bool exp_beg = false;
                    bool is_float = false;

                    while (true) {

                        switch (reading) {
                            case READING_INT: {

                                if (c >= '0' && c <= '9') {
                                    //pass
                                } else if (c == '.') {
                                    reading = READING_DEC;
                                    is_float = true;
                                } else if (c == 'e') {
                                    reading = READING_EXP;
                                    is_float = true;
                                } else {
                                    reading = READING_DONE;
                                }

                            } break;
                            case READING_DEC: {

                                if (c >= '0' && c <= '9') {

                                } else if (c == 'e') {
                                    reading = READING_EXP;
                                } else {
                                    reading = READING_DONE;
                                }

                            } break;
                            case READING_EXP: {

                                if (c >= '0' && c <= '9') {
                                    exp_beg = true;

                                } else if ((c == '-' || c == '+') && !exp_sign && !exp_beg) {
                                    exp_sign = true;

                                } else {
                                    reading = READING_DONE;
                                }
                            } break;
                        }

                        if (reading == READING_DONE)
                            break;
                        tmp_str_buf.push_back(c);
                        c = p_stream->get_char();
                    }

                    p_stream->saved = c;

                    r_token.type = TK_NUMBER;

                    if (is_float)
                        r_token.value = StringUtils::to_double(tmp_str_buf);
                    else
                        r_token.value = StringUtils::to_int(tmp_str_buf);
                    return OK;

                } else if ((cchar >= 'A' && cchar <= 'Z') || (cchar >= 'a' && cchar <= 'z') || cchar == '_') {

                    tmp_str_buf = "";
                    bool first = true;

                    while ((cchar >= 'A' && cchar <= 'Z') || (cchar >= 'a' && cchar <= 'z') || cchar == '_' || (!first && cchar >= '0' && cchar <= '9')) {

                        tmp_str_buf += cchar;
                        cchar = p_stream->get_char();
                        first = false;
                    }

                    p_stream->saved = cchar;

                    r_token.type = TK_IDENTIFIER;
                    r_token.value = Variant::from(StringView(tmp_str_buf));
                    return OK;
                } else {
                    r_err_str = "Unexpected character.";
                    r_token.type = TK_ERROR;
                    return ERR_PARSE_ERROR;
                }
            }
        }
    }

    r_token.type = TK_ERROR;
    return ERR_PARSE_ERROR;
}

template <class T>
Error VariantParser::_parse_construct(VariantParserStream *p_stream, Vector<T> &r_construct, int &line, String &r_err_str) {

    Token token;
    get_token(p_stream, token, line, r_err_str);
    if (token.type != TK_PARENTHESIS_OPEN) {
        r_err_str = "Expected '(' in constructor";
        return ERR_PARSE_ERROR;
    }

    bool first = true;
    while (true) {

        if (!first) {
            get_token(p_stream, token, line, r_err_str);
            if (token.type == TK_COMMA) {
                //do none
            } else if (token.type == TK_PARENTHESIS_CLOSE) {
                break;
            } else {
                r_err_str = "Expected ',' or ')' in constructor";
                return ERR_PARSE_ERROR;
            }
        }
        get_token(p_stream, token, line, r_err_str);

        if (first && token.type == TK_PARENTHESIS_CLOSE) {
            break;
        }
        if (token.type != TK_NUMBER) {
            bool valid = false;
            if (token.type == TK_IDENTIFIER) {
                double real = stor_fix(token.value.as<String>());
                if (real != -1) {
                    token.type = TK_NUMBER;
                    token.value = real;
                    valid = true;
                }
            }
            if (!valid) {
            r_err_str = "Expected float in constructor";
            return ERR_PARSE_ERROR;
        }
        }

        r_construct.push_back(token.value.as<T>());
        first = false;
    }

    return OK;
}

Error VariantParser::parse_value(Token &token, Variant &value, VariantParserStream *p_stream, int &line, String &r_err_str, ResourceParser *p_res_parser) {
    using namespace eastl; // for _sv suffix
    /*  {
        Error err = get_token(p_stream,token,line,r_err_str);
        if (err)
            return err;
    }*/

    if (token.type == TK_CURLY_BRACKET_OPEN) {

        Dictionary d;
        Error err = _parse_dictionary(d, p_stream, line, r_err_str, p_res_parser);
        if (err) {
            return err;
        }
        value = d;
        return OK;
    } else if (token.type == TK_BRACKET_OPEN) {

        Array a;
        Error err = _parse_array(a, p_stream, line, r_err_str, p_res_parser);
        if (err) {
            return err;
        }
        value = a;
        return OK;

    } else if (token.type == TK_IDENTIFIER) {

        String id = token.value.as<String>();
        if (id == "true"_sv)
            value = true;
        else if (id == "false"_sv)
            value = false;
        else if (id == "null"_sv || id == "nil"_sv)
            value = Variant();
        else if (id == "inf"_sv)
            value = Math_INF;
        else if (id == "inf_neg"_sv)
            value = -Math_INF;
        else if (id == "nan"_sv)
            value = Math_NAN;
        else if (id == "Vector2"_sv) {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err) {
                return err;
            }

            if (args.size() != 2) {
                r_err_str = "Expected 2 arguments for constructor";
                return ERR_PARSE_ERROR;
            }

            value = Vector2(args[0], args[1]);
            return OK;
        } else if (id == "Rect2") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err) {
                return err;
            }

            if (args.size() != 4) {
                r_err_str = "Expected 4 arguments for constructor";
                return ERR_PARSE_ERROR;
            }

            value = Rect2(args[0], args[1], args[2], args[3]);
            return OK;
        } else if (id == "Vector3") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err)
                return err;

            if (args.size() != 3) {
                r_err_str = "Expected 3 arguments for constructor";
            }

            value = Vector3(args[0], args[1], args[2]);
            return OK;
        } else if (id == "Transform2D" || id == "Matrix32") { //compatibility

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err) {
                return err;
            }

            if (args.size() != 6) {
                r_err_str = "Expected 6 arguments for constructor";
                return ERR_PARSE_ERROR;
            }
            Transform2D m;
            m[0] = Vector2(args[0], args[1]);
            m[1] = Vector2(args[2], args[3]);
            m[2] = Vector2(args[4], args[5]);
            value = m;
            return OK;
        } else if (id == "Plane") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err) {
                return err;
            }

            if (args.size() != 4) {
                r_err_str = "Expected 4 arguments for constructor";
                return ERR_PARSE_ERROR;
            }

            value = Plane(args[0], args[1], args[2], args[3]);
            return OK;
        } else if (id == "Quat") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err) {
                return err;
            }

            if (args.size() != 4) {
                r_err_str = "Expected 4 arguments for constructor";
                return ERR_PARSE_ERROR;
            }

            value = Quat(args[0], args[1], args[2], args[3]);
            return OK;

        } else if (id == "AABB" || id == "Rect3") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err) {
                return err;
            }

            if (args.size() != 6) {
                r_err_str = "Expected 6 arguments for constructor";
                return ERR_PARSE_ERROR;
            }

            value = AABB(Vector3(args[0], args[1], args[2]), Vector3(args[3], args[4], args[5]));
            return OK;

        } else if (id == "Basis" || id == "Matrix3") { //compatibility

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err) {
                return err;
            }

            if (args.size() != 9) {
                r_err_str = "Expected 9 arguments for constructor";
                return ERR_PARSE_ERROR;
            }

            value = Basis(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
            return OK;
        } else if (id == "Transform") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err) {
                return err;
            }

            if (args.size() != 12) {
                r_err_str = "Expected 12 arguments for constructor";
                return ERR_PARSE_ERROR;
            }

            value = Transform(Basis(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]), Vector3(args[9], args[10], args[11]));
            return OK;

        } else if (id == "Color") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err) {
                return err;
            }

            if (args.size() != 4) {
                r_err_str = "Expected 4 arguments for constructor";
                return ERR_PARSE_ERROR;
            }

            value = Color(args[0], args[1], args[2], args[3]);
            return OK;

        } else if (id == "NodePath") {

            get_token(p_stream, token, line, r_err_str);
            if (token.type != TK_PARENTHESIS_OPEN) {
                r_err_str = "Expected '('";
                return ERR_PARSE_ERROR;
            }

            get_token(p_stream, token, line, r_err_str);
            if (token.type != TK_STRING) {
                r_err_str = "Expected string as argument for NodePath()";
                return ERR_PARSE_ERROR;
            }

            value = NodePath(token.value.as<String>());

            get_token(p_stream, token, line, r_err_str);
            if (token.type != TK_PARENTHESIS_CLOSE) {
                r_err_str = "Expected ')'";
                return ERR_PARSE_ERROR;
            }

        } else if (id == "RID") {

            get_token(p_stream, token, line, r_err_str);
            if (token.type != TK_PARENTHESIS_OPEN) {
                r_err_str = "Expected '('";
                return ERR_PARSE_ERROR;
            }

            get_token(p_stream, token, line, r_err_str);
            if (token.type != TK_NUMBER) {
                r_err_str = "Expected number as argument";
                return ERR_PARSE_ERROR;
            }

            value = token.value;

            get_token(p_stream, token, line, r_err_str);
            if (token.type != TK_PARENTHESIS_CLOSE) {
                r_err_str = "Expected ')'";
                return ERR_PARSE_ERROR;
            }

            return OK;
        } else if (id == "Object") {

            get_token(p_stream, token, line, r_err_str);
            if (token.type != TK_PARENTHESIS_OPEN) {
                r_err_str = "Expected '('";
                return ERR_PARSE_ERROR;
            }

            get_token(p_stream, token, line, r_err_str);

            if (token.type != TK_IDENTIFIER) {
                r_err_str = "Expected identifier with type of object";
                return ERR_PARSE_ERROR;
            }

            StringName type = token.value.as<StringName>();

            Object *obj = ClassDB::instance(type);

            if (!obj) {
                r_err_str = String("Can't instance Object() of type: ") + type;
                return ERR_PARSE_ERROR;
            }

            REF ref = REF(object_cast<RefCounted>(obj));

            get_token(p_stream, token, line, r_err_str);
            if (token.type != TK_COMMA) {
                r_err_str = "Expected ',' after object type";
                return ERR_PARSE_ERROR;
            }

            bool at_key = true;
            String key;
            Token token2;
            bool need_comma = false;

            while (true) {

                if (p_stream->is_eof()) {
                    r_err_str = "Unexpected End of File while parsing Object()";
                    return ERR_FILE_CORRUPT;
                }

                if (at_key) {

                    Error err = get_token(p_stream, token2, line, r_err_str);
                    if (err != OK) {
                        return err;
                    }

                    if (token2.type == TK_PARENTHESIS_CLOSE) {
                        value = ref ? Variant(ref) : Variant(obj);
                        return OK;
                    }

                    if (need_comma) {

                        if (token2.type != TK_COMMA) {

                            r_err_str = "Expected '}' or ','";
                            return ERR_PARSE_ERROR;
                        } else {
                            need_comma = false;
                            continue;
                        }
                    }

                    if (token2.type != TK_STRING) {
                        r_err_str = "Expected property name as string";
                        return ERR_PARSE_ERROR;
                    }

                    key = token2.value.as<String>();

                    err = get_token(p_stream, token2, line, r_err_str);

                    if (err != OK)
                        return err;
                    if (token2.type != TK_COLON) {

                        r_err_str = "Expected ':'";
                        return ERR_PARSE_ERROR;
                    }
                    at_key = false;
                } else {

                    Error err = get_token(p_stream, token2, line, r_err_str);
                    if (err != OK)
                        return err;

                    Variant v;
                    err = parse_value(token2, v, p_stream, line, r_err_str, p_res_parser);
                    if (err)
                        return err;
                    obj->set(StringName(key), v);
                    need_comma = true;
                    at_key = true;
                }
            }

        } else if (id == "Resource" || id == "SubResource" || id == "ExtResource") {

            get_token(p_stream, token, line, r_err_str);
            if (token.type != TK_PARENTHESIS_OPEN) {
                r_err_str = "Expected '('";
                return ERR_PARSE_ERROR;
            }

            if (p_res_parser && id == "Resource" && p_res_parser->func) {

                RES res;
                Error err = p_res_parser->func(p_res_parser->userdata, p_stream, res, line, r_err_str);
                if (err) {
                    return err;
                }

                value = res;

                return OK;
            } else if (p_res_parser && id == "ExtResource" && p_res_parser->ext_func) {

                RES res;
                Error err = p_res_parser->ext_func(p_res_parser->userdata, p_stream, res, line, r_err_str);
                if (err) {
                    return err;
                }

                value = res;

                return OK;
            } else if (p_res_parser && id == "SubResource" && p_res_parser->sub_func) {

                RES res;
                Error err = p_res_parser->sub_func(p_res_parser->userdata, p_stream, res, line, r_err_str);
                if (err) {
                    return err;
                }

                value = res;

                return OK;
            } else {

                get_token(p_stream, token, line, r_err_str);
                if (token.type == TK_STRING) {
                    String path = token.value.as<String>();
                    RES res(gResourceManager().load(path));
                    if (not res) {
                        r_err_str = "Can't load resource at path: '" + path + "'.";
                        return ERR_PARSE_ERROR;
                    }

                    get_token(p_stream, token, line, r_err_str);
                    if (token.type != TK_PARENTHESIS_CLOSE) {
                        r_err_str = "Expected ')'";
                        return ERR_PARSE_ERROR;
                    }

                    value = res;
                    return OK;

                } else {
                    r_err_str = "Expected string as argument for Resource().";
                    return ERR_PARSE_ERROR;
                }
            }
        } else if (id == "PoolByteArray" || id == "ByteArray") {

            Vector<uint8_t> args;
            Error err = _parse_construct<uint8_t>(p_stream, args, line, r_err_str);
            if (err) {
                return err;
            }

            PoolVector<uint8_t> arr;
            {
                int len = args.size();
                arr.resize(len);
                PoolVector<uint8_t>::Write w = arr.write();
                for (int i = 0; i < len; i++) {
                    w[i] = args[i];
                }
            }

            value = arr;

            return OK;

        } else if (id == "PoolIntArray" || id == "IntArray") {

            Vector<int> args;
            Error err = _parse_construct<int>(p_stream, args, line, r_err_str);
            if (err)
                return err;

            PoolVector<int> arr;
            {
                int len = args.size();
                arr.resize(len);
                PoolVector<int>::Write w = arr.write();
                for (int i = 0; i < len; i++) {
                    w[i] = int(args[i]);
                }
            }

            value = arr;

            return OK;

        } else if (id == "PoolRealArray" || id == "FloatArray") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err)
                return err;

            PoolVector<float> arr;
            {
                int len = args.size();
                arr.resize(len);
                PoolVector<float>::Write w = arr.write();
                for (int i = 0; i < len; i++) {
                    w[i] = args[i];
                }
            }

            value = arr;

            return OK;
        } else if (id == "PoolStringArray" || id == "StringArray") {

            get_token(p_stream, token, line, r_err_str);
            if (token.type != TK_PARENTHESIS_OPEN) {
                r_err_str = "Expected '('";
                return ERR_PARSE_ERROR;
            }

            Vector<UIString> cs;

            bool first = true;
            while (true) {

                if (!first) {
                    get_token(p_stream, token, line, r_err_str);
                    if (token.type == TK_COMMA) {
                        //do none
                    } else if (token.type == TK_PARENTHESIS_CLOSE) {
                        break;
                    } else {
                        r_err_str = "Expected ',' or ')'";
                        return ERR_PARSE_ERROR;
                    }
                }
                get_token(p_stream, token, line, r_err_str);

                if (token.type == TK_PARENTHESIS_CLOSE) {
                    break;
                } else if (token.type != TK_STRING) {
                    r_err_str = "Expected string";
                    return ERR_PARSE_ERROR;
                }

                first = false;
                cs.push_back(token.value.as<UIString>());
            }

            PoolVector<UIString> arr;
            {
                int len = cs.size();
                arr.resize(len);
                PoolVector<UIString>::Write w = arr.write();
                for (int i = 0; i < len; i++) {
                    w[i] = cs[i];
                }
            }

            value = arr;

            return OK;

        } else if (id == StringView("PoolVector2Array") || id == "Vector2Array") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err)
                return err;

            PoolVector<Vector2> arr;
            {
                int len = args.size() / 2;
                arr.resize(len);
                PoolVector<Vector2>::Write w = arr.write();
                for (int i = 0; i < len; i++) {
                    w[i] = Vector2(args[i * 2 + 0], args[i * 2 + 1]);
                }
            }

            value = Variant(arr);

            return OK;

        } else if (id == "PoolVector3Array" || id == "Vector3Array") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err)
                return err;

            PoolVector<Vector3> arr;
            {
                int len = args.size() / 3;
                arr.resize(len);
                PoolVector<Vector3>::Write w = arr.write();
                for (int i = 0; i < len; i++) {
                    w[i] = Vector3(args[i * 3 + 0], args[i * 3 + 1], args[i * 3 + 2]);
                }
            }

            value = arr;

            return OK;

        } else if (id == "PoolColorArray" || id == "ColorArray") {

            Vector<float> args;
            Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
            if (err)
                return err;

            PoolVector<Color> arr;
            {
                int len = args.size() / 4;
                arr.resize(len);
                PoolVector<Color>::Write w = arr.write();
                for (int i = 0; i < len; i++) {
                    w[i] = Color(args[i * 4 + 0], args[i * 4 + 1], args[i * 4 + 2], args[i * 4 + 3]);
                }
            }

            value = arr;

            return OK;
        } else {
            r_err_str = "Unexpected identifier: '" + id + "'.";
            return ERR_PARSE_ERROR;
        }

        return OK;

    } else if (token.type == TK_NUMBER) {

        value = token.value;
        return OK;
    } else if (token.type == TK_STRING) {
        value = token.value;
        return OK;
    } else if (token.type == TK_STRING_NAME) {
        value = token.value;
        return OK;
    } else if (token.type == TK_COLOR) {

        value = token.value;
        return OK;
    } else {
        r_err_str = "Expected value, got " + String(tk_name[token.type]) + ".";
        return ERR_PARSE_ERROR;
    }
}

Error VariantParser::_parse_array(Array &array, VariantParserStream *p_stream, int &line, String &r_err_str, ResourceParser *p_res_parser) {

    Token token;
    bool need_comma = false;

    while (true) {

        if (p_stream->is_eof()) {
            r_err_str = "Unexpected End of File while parsing array";
            return ERR_FILE_CORRUPT;
        }

        Error err = get_token(p_stream, token, line, r_err_str);
        if (err != OK) {
            return err;
        }

        if (token.type == TK_BRACKET_CLOSE) {

            return OK;
        }

        if (need_comma) {

            if (token.type != TK_COMMA) {

                r_err_str = "Expected ','";
                return ERR_PARSE_ERROR;
            } else {
                need_comma = false;
                continue;
            }
        }

        Variant v;
        err = parse_value(token, v, p_stream, line, r_err_str, p_res_parser);
        if (err) {
            return err;
        }

        array.push_back(v);
        need_comma = true;
    }
}

Error VariantParser::_parse_dictionary(Dictionary &object, VariantParserStream *p_stream, int &line, String &r_err_str, ResourceParser *p_res_parser) {

    bool at_key = true;
    Variant key;
    Token token;
    bool need_comma = false;

    while (true) {

        if (p_stream->is_eof()) {
            r_err_str = "Unexpected End of File while parsing dictionary";
            return ERR_FILE_CORRUPT;
        }

        if (at_key) {

            Error err = get_token(p_stream, token, line, r_err_str);
            if (err != OK) {
                return err;
            }

            if (token.type == TK_CURLY_BRACKET_CLOSE) {

                return OK;
            }

            if (need_comma) {

                if (token.type != TK_COMMA) {

                    r_err_str = "Expected '}' or ','";
                    return ERR_PARSE_ERROR;
                } else {
                    need_comma = false;
                    continue;
                }
            }

            err = parse_value(token, key, p_stream, line, r_err_str, p_res_parser);

            if (err) {
                return err;
            }

            err = get_token(p_stream, token, line, r_err_str);

            if (err != OK) {
                return err;
            }
            if (token.type != TK_COLON) {

                r_err_str = "Expected ':'";
                return ERR_PARSE_ERROR;
            }
            at_key = false;
        } else {

            Error err = get_token(p_stream, token, line, r_err_str);
            if (err != OK) {
                return err;
            }

            Variant v;
            err = parse_value(token, v, p_stream, line, r_err_str, p_res_parser);
            if (err) {
                return err;
            }
            if(key.get_type()!=VariantType::STRING && key.get_type() != VariantType::STRING_NAME)
            {
                r_err_str = "Expected key to be a string";
                return ERR_PARSE_ERROR;
            }
            object[key.as<StringName>()] = v;
            need_comma = true;
            at_key = true;
        }
    }
}

Error VariantParser::_parse_tag(Token &token, VariantParserStream *p_stream, int &line, String &r_err_str, Tag &r_tag, ResourceParser *p_res_parser, bool p_simple_tag) {

    r_tag.fields.clear();

    if (token.type != TK_BRACKET_OPEN) {
        r_err_str = "Expected '['";
        return ERR_PARSE_ERROR;
    }

    if (p_simple_tag) {

        r_tag.name = "";
        r_tag.fields.clear();

        while (true) {

            char c = p_stream->get_char();
            if (p_stream->is_eof()) {
                r_err_str = "Unexpected EOF while parsing simple tag";
                return ERR_PARSE_ERROR;
            }
            if (c == ']')
                break;
            r_tag.name.push_back(c);
        }

        r_tag.name =StringUtils::strip_edges( r_tag.name);

        return OK;
    }

    get_token(p_stream, token, line, r_err_str);

    if (token.type != TK_IDENTIFIER) {
        r_err_str = "Expected identifier (tag name)";
        return ERR_PARSE_ERROR;
    }

    r_tag.name = token.value.as<String>();
    bool parsing_tag = true;

    while (true) {

        if (p_stream->is_eof()) {
            r_err_str = String("Unexpected End of File while parsing tag: " + r_tag.name);
            return ERR_FILE_CORRUPT;
        }

        get_token(p_stream, token, line, r_err_str);
        if (token.type == TK_BRACKET_CLOSE)
            break;

        if (parsing_tag && token.type == TK_PERIOD) {
            r_tag.name += '.'; //support tags such as [someprop.Android] for specific platforms
            get_token(p_stream, token, line, r_err_str);
        } else if (parsing_tag && token.type == TK_COLON) {
            r_tag.name += ':'; //support tags such as [someprop.Android] for specific platforms
            get_token(p_stream, token, line, r_err_str);
        } else {
            parsing_tag = false;
        }

        if (token.type != TK_IDENTIFIER) {
            r_err_str = "Expected Identifier";
            return ERR_PARSE_ERROR;
        }

        String id = token.value.as<String>();

        if (parsing_tag) {
            r_tag.name += id;
            continue;
        }

        get_token(p_stream, token, line, r_err_str);
        if (token.type != TK_EQUAL) {
            return ERR_PARSE_ERROR;
        }

        get_token(p_stream, token, line, r_err_str);
        Variant value;
        Error err = parse_value(token, value, p_stream, line, r_err_str, p_res_parser);
        if (err) {
            return err;
        }

        r_tag.fields[id] = value;
    }

    return OK;
}

Error VariantParser::parse_tag(VariantParserStream *p_stream, int &line, String &r_err_str, Tag &r_tag, ResourceParser *p_res_parser, bool p_simple_tag) {

    Token token;
    get_token(p_stream, token, line, r_err_str);

    if (token.type == TK_EOF) {
        return ERR_FILE_EOF;
    }

    if (token.type != TK_BRACKET_OPEN) {
        r_err_str = "Expected '['";
        return ERR_PARSE_ERROR;
    }

    return _parse_tag(token, p_stream, line, r_err_str, r_tag, p_res_parser, p_simple_tag);
}

Error VariantParser::parse_tag_assign_eof(VariantParserStream *p_stream, int &line, String &r_err_str, Tag &r_tag, String &r_assign, Variant &r_value, ResourceParser *p_res_parser, bool p_simple_tag) {

    //assign..
    r_assign.clear();
    String what;

    while (true) {

        char c;
        if (p_stream->saved) {
            c = p_stream->saved;
            p_stream->saved = 0;

        } else {
            c = p_stream->get_char();
        }

        if (p_stream->is_eof()) {
            return ERR_FILE_EOF;
        }

        if (c == ';') { //comment
            while (true) {
                CharType ch = p_stream->get_char();
                if (p_stream->is_eof()) {
                    return ERR_FILE_EOF;
                }
                if (ch == '\n') {
                    break;
                }
            }
            continue;
        }

        if (c == '[' && what.length() == 0) {
            //it's a tag!
            p_stream->saved = '['; //go back one

            Error err = parse_tag(p_stream, line, r_err_str, r_tag, p_res_parser, p_simple_tag);

            return err;
        }

        if (c > 32) {
            if (c == '"') { //quoted
                p_stream->saved = '"';
                Token tk;
                Error err = get_token(p_stream, tk, line, r_err_str);
                if (err) {
                    return err;
                }
                if (tk.type != TK_STRING) {
                    r_err_str = "Error reading quoted string";
                    return ERR_INVALID_DATA;
                }

                what = tk.value.as<String>();

            } else if (c != '=') {
                what.push_back(c);
            } else {
                r_assign = what;
                Token token;
                get_token(p_stream, token, line, r_err_str);
                Error err = parse_value(token, r_value, p_stream, line, r_err_str, p_res_parser);
                return err;
            }
        } else if (c == '\n') {
            line++;
        }
    }
}

Error VariantParser::parse(VariantParserStream *p_stream, Variant &r_ret, String &r_err_str, int &r_err_line, ResourceParser *p_res_parser) {

    Token token;
    Error err = get_token(p_stream, token, r_err_line, r_err_str);
    if (err) {
        return err;
    }

    if (token.type == TK_EOF) {
        return ERR_FILE_EOF;
    }

    return parse_value(token, r_ret, p_stream, r_err_line, r_err_str, p_res_parser);
}

VariantParserStream *VariantParser::get_file_stream(FileAccess *f)
{
    return memnew_args_basic(StreamFile,f);
}

VariantParserStream *VariantParser::get_string_stream(const String &f)
{
    return memnew_args_basic(StreamString,f);

}
VariantParserStream *VariantParser::get_string_stream(String &&f)
{
    return memnew_args_basic(StreamString,eastl::move(f));

}
void VariantParser::release_stream(VariantParserStream *s)
{
    memdelete(s);
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static String rtos_fix(double p_value) {
    if (p_value == 0.0) {
        return "0"; // avoid negative zero (-0) being written, which may annoy git, svn, etc. for changes when they
                    // don't exist.
    } else if (std::isnan(p_value)) {
        return "nan";
    } else if (std::isinf(p_value)) {
        if (p_value > 0) {
            return "inf";
        } else {
            return "inf_neg";
        }
    } else {
        return StringUtils::num_scientific(p_value);
}
}

Error VariantWriter::write(const Variant &p_variant, StoreStringFunc p_store_string_func, void *p_store_string_ud, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud) {

    switch (p_variant.get_type()) {

        case VariantType::NIL: {
            p_store_string_func(p_store_string_ud, ("null"));
        } break;
        case VariantType::BOOL: {

            p_store_string_func(p_store_string_ud, (p_variant.as<bool>() ? "true" : "false"));
        } break;
        case VariantType::INT: {

            p_store_string_func(p_store_string_ud, ::to_string(p_variant.as<int64_t>()));
        } break;
        case VariantType::FLOAT: {

            String s = rtos_fix(p_variant.as<float>());
            if (s != "inf" && s != "inf_neg" && s != "nan") {
                if (not StringUtils::contains(s,".") && not StringUtils::contains(s,"e"))
                    s += ".0";
                p_store_string_func(p_store_string_ud, s);
            }
        } break;
        case VariantType::STRING: {

            String str = p_variant.as<String>();

            str = "\"" + StringUtils::c_escape_multiline(str) + "\"";
            p_store_string_func(p_store_string_ud, str);
        } break;
        case VariantType::VECTOR2: {

            Vector2 v = p_variant.as<Vector2>();
            p_store_string_func(p_store_string_ud, "Vector2( " + rtos_fix(v.x) + ", " + rtos_fix(v.y) + " )");
        } break;
        case VariantType::RECT2: {

            Rect2 aabb = p_variant.as<Rect2>();
            p_store_string_func(p_store_string_ud, "Rect2( " + rtos_fix(aabb.position.x) + ", " + rtos_fix(aabb.position.y) + ", " + rtos_fix(aabb.size.x) + ", " + rtos_fix(aabb.size.y) + " )");

        } break;
        case VariantType::VECTOR3: {

            Vector3 v = p_variant.as<Vector3>();
            p_store_string_func(p_store_string_ud, "Vector3( " + rtos_fix(v.x) + ", " + rtos_fix(v.y) + ", " + rtos_fix(v.z) + " )");
        } break;
        case VariantType::PLANE: {

            Plane p = p_variant.as<Plane>();
            p_store_string_func(p_store_string_ud, "Plane( " + rtos_fix(p.normal.x) + ", " + rtos_fix(p.normal.y) + ", " + rtos_fix(p.normal.z) + ", " + rtos_fix(p.d) + " )");

        } break;
        case VariantType::AABB: {

            AABB aabb = p_variant.as<::AABB>();
            p_store_string_func(p_store_string_ud, "AABB( " + rtos_fix(aabb.position.x) + ", " + rtos_fix(aabb.position.y) + ", " + rtos_fix(aabb.position.z) + ", " + rtos_fix(aabb.size.x) + ", " + rtos_fix(aabb.size.y) + ", " + rtos_fix(aabb.size.z) + " )");

        } break;
        case VariantType::QUAT: {

            Quat quat = p_variant.as<Quat>();
            p_store_string_func(p_store_string_ud, "Quat( " + rtos_fix(quat.x) + ", " + rtos_fix(quat.y) + ", " + rtos_fix(quat.z) + ", " + rtos_fix(quat.w) + " )");

        } break;
        case VariantType::TRANSFORM2D: {

            String s("Transform2D( ");
            Transform2D m3 = p_variant.as<Transform2D>();
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {

                    if (i != 0 || j != 0)
                        s += ", ";
                    s += rtos_fix(m3.elements[i][j]);
                }
            }

            p_store_string_func(p_store_string_ud, s + " )");

        } break;
        case VariantType::BASIS: {

            String s("Basis( ");
            Basis m3 = p_variant.as<Basis>();
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {

                    if (i != 0 || j != 0)
                        s += ", ";
                    s += rtos_fix(m3.elements[i][j]);
                }
            }

            p_store_string_func(p_store_string_ud, s + " )");

        } break;
        case VariantType::TRANSFORM: {

            String s("Transform( ");
            Transform t = p_variant.as<Transform>();
            Basis &m3 = t.basis;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {

                    if (i != 0 || j != 0) {
                        s += ", ";
                    }
                    s += rtos_fix(m3.elements[i][j]);
                }
            }

            s = s + ", " + rtos_fix(t.origin.x) + ", " + rtos_fix(t.origin.y) + ", " + rtos_fix(t.origin.z);

            p_store_string_func(p_store_string_ud, s + " )");
        } break;

        // misc types
        case VariantType::COLOR: {

            Color c = p_variant.as<Color>();
            p_store_string_func(p_store_string_ud, "Color( " + rtos_fix(c.r) + ", " + rtos_fix(c.g) + ", " + rtos_fix(c.b) + ", " + rtos_fix(c.a) + " )");

        } break;
        case VariantType::STRING_NAME: {
            String str((StringName)p_variant);

            str = "@\"" + StringUtils::c_escape(str) + "\"";
            p_store_string_func(p_store_string_ud, str);

        } break;
        case VariantType::NODE_PATH: {

            String str = p_variant.as<String>();

            str = "NodePath(\"" + StringUtils::c_escape(str) + "\")";
            p_store_string_func(p_store_string_ud, str);

        } break;

        case VariantType::OBJECT: {

            Object *obj = p_variant.as<Object *>();

            if (!obj) {
                p_store_string_func(p_store_string_ud, "null");
                break; // don't save it
            }

            RES res(refFromVariant<Resource>(p_variant));
            if (res) {
                //is resource
                String res_text;

                //try external function
                if (p_encode_res_func) {

                    res_text = p_encode_res_func(p_encode_res_ud, res);
                }

                //try path because it's a file
                if (res_text.empty() && PathUtils::is_resource_file(res->get_path())) {

                    //external resource
                    String path = res->get_path();
                    res_text = "Resource( \"" + path + "\")";
                }

                //could come up with some sort of text
                if (!res_text.empty()) {
                    p_store_string_func(p_store_string_ud, res_text);
                    break;
                }
            }

            //store as generic object

            p_store_string_func(p_store_string_ud, "Object(" + String(obj->get_class()) + ",");

            Vector<PropertyInfo> props;
            obj->get_property_list(&props);
            bool first = true;
            for (const PropertyInfo & E : props) {

                if (E.usage & PROPERTY_USAGE_STORAGE || E.usage & PROPERTY_USAGE_SCRIPT_VARIABLE) {
                    //must be serialized

                    if (first) {
                        first = false;
                    } else {
                        p_store_string_func(p_store_string_ud, ",");
                    }

                    p_store_string_func(p_store_string_ud, String("\"") + E.name.asCString() + "\":");
                    write(obj->get(E.name), p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
                }
            }

            p_store_string_func(p_store_string_ud, ")\n");

        } break;

        case VariantType::DICTIONARY: {

            Dictionary dict = p_variant.as<Dictionary>();

            auto keys(dict.get_key_list());
            eastl::sort(keys.begin(),keys.end(), WrapAlphaCompare());

            p_store_string_func(p_store_string_ud, "{\n");
            int size = keys.size()-1;
            for(auto &E : keys ) {

                /*
                if (!_check_type(dict[E]))
                    continue;
                */
                write(E, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
                p_store_string_func(p_store_string_ud, (": "));
                write(dict[E], p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
                if (size!=0)
                    p_store_string_func(p_store_string_ud, (",\n"));
                size--;
            }

            p_store_string_func(p_store_string_ud, ("\n}"));

        } break;
        case VariantType::ARRAY: {

            p_store_string_func(p_store_string_ud, ("[ "));
            Array array = p_variant.as<Array>();
            int len = array.size();
            for (int i = 0; i < len; i++) {

                if (i > 0)
                    p_store_string_func(p_store_string_ud, (", "));
                write(array[i], p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
            }
            p_store_string_func(p_store_string_ud, (" ]"));

        } break;

        case VariantType::POOL_BYTE_ARRAY: {

            p_store_string_func(p_store_string_ud, ("PoolByteArray( "));
            PoolVector<uint8_t> data = p_variant.as<PoolVector<uint8_t>>();
            int len = data.size();
            PoolVector<uint8_t>::Read r = data.read();
            const uint8_t *ptr = r.ptr();
            for (int i = 0; i < len; i++) {

                if (i > 0)
                    p_store_string_func(p_store_string_ud, (", "));

                p_store_string_func(p_store_string_ud, ::to_string(ptr[i]));
            }

            p_store_string_func(p_store_string_ud, (" )"));

        } break;
        case VariantType::POOL_INT_ARRAY: {

            p_store_string_func(p_store_string_ud, String("PoolIntArray( "));
            PoolVector<int> data = p_variant.as<PoolVector<int>>();
            int len = data.size();
            PoolVector<int>::Read r = data.read();
            const int *ptr = r.ptr();

            for (int i = 0; i < len; i++) {

                if (i > 0)
                    p_store_string_func(p_store_string_ud, (", "));

                p_store_string_func(p_store_string_ud, ::to_string(ptr[i]));
            }

            p_store_string_func(p_store_string_ud, (" )"));

        } break;
        case VariantType::POOL_FLOAT32_ARRAY: {

            p_store_string_func(p_store_string_ud, ("PoolRealArray( "));
            PoolVector<real_t> data = p_variant.as<PoolVector<real_t>>();
            int len = data.size();
            PoolVector<real_t>::Read r = data.read();
            const real_t *ptr = r.ptr();

            for (int i = 0; i < len; i++) {

                if (i > 0)
                    p_store_string_func(p_store_string_ud, (", "));
                p_store_string_func(p_store_string_ud, rtos_fix(ptr[i]));
            }

            p_store_string_func(p_store_string_ud, (" )"));

        } break;
        case VariantType::POOL_STRING_ARRAY: {

            p_store_string_func(p_store_string_ud, ("PoolStringArray( "));
            PoolVector<String> data = p_variant.as<PoolVector<String>>();
            int len = data.size();
            PoolVector<String>::Read r = data.read();
            const String *ptr = r.ptr();
            String s;
            //write_string("\n");

            for (int i = 0; i < len; i++) {

                if (i > 0)
                    p_store_string_func(p_store_string_ud, (", "));
                String str = ptr[i];
                p_store_string_func(p_store_string_ud, String("\"") + StringUtils::c_escape(str) + "\"");
            }

            p_store_string_func(p_store_string_ud, (" )"));

        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {

            p_store_string_func(p_store_string_ud, ("PoolVector2Array( "));
            PoolVector<Vector2> data = p_variant.as<PoolVector<Vector2>>();
            int len = data.size();
            PoolVector<Vector2>::Read r = data.read();
            const Vector2 *ptr = r.ptr();

            for (int i = 0; i < len; i++) {

                if (i > 0)
                    p_store_string_func(p_store_string_ud, (", "));
                p_store_string_func(p_store_string_ud, rtos_fix(ptr[i].x) + ", " + rtos_fix(ptr[i].y));
            }

            p_store_string_func(p_store_string_ud, (" )"));

        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {

            p_store_string_func(p_store_string_ud, ("PoolVector3Array( "));
            PoolVector<Vector3> data = p_variant.as<PoolVector<Vector3>>();
            int len = data.size();
            PoolVector<Vector3>::Read r = data.read();
            const Vector3 *ptr = r.ptr();

            for (int i = 0; i < len; i++) {

                if (i > 0)
                    p_store_string_func(p_store_string_ud, (", "));
                p_store_string_func(p_store_string_ud, rtos_fix(ptr[i].x) + ", " + rtos_fix(ptr[i].y) + ", " + rtos_fix(ptr[i].z));
            }

            p_store_string_func(p_store_string_ud, (" )"));

        } break;
        case VariantType::POOL_COLOR_ARRAY: {

            p_store_string_func(p_store_string_ud, ("PoolColorArray( "));

            PoolVector<Color> data = p_variant.as<PoolVector<Color>>();
            int len = data.size();
            PoolVector<Color>::Read r = data.read();
            const Color *ptr = r.ptr();

            for (int i = 0; i < len; i++) {

                if (i > 0)
                    p_store_string_func(p_store_string_ud, (", "));

                p_store_string_func(p_store_string_ud, rtos_fix(ptr[i].r) + ", " + rtos_fix(ptr[i].g) + ", " + rtos_fix(ptr[i].b) + ", " + rtos_fix(ptr[i].a));
            }
            p_store_string_func(p_store_string_ud, (" )"));

        } break;
        default: {
        }
    }

    return OK;
}

static Error _write_to_str(void *ud, const String &p_string) {

    String *str = (String *)ud;
    (*str) += p_string;
    return OK;
}

Error VariantWriter::write_to_string(const Variant &p_variant, String &r_string, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud) {

    r_string.clear();

    return write(p_variant, _write_to_str, &r_string, p_encode_res_func, p_encode_res_ud);
}
