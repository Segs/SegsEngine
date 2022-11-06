/*************************************************************************/
/*  json.cpp                                                             */
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

#include "json.h"

#include "core/print_string.h"
#include "core/list.h"
#include "core/vector.h"
#include "core/dictionary.h"
#include "core/string_utils.inl"

#include "EASTL/set.h"
#include "EASTL/sort.h"

const char *JSON::tk_name[TK_MAX] = {
    "'{'",
    "'}'",
    "'['",
    "']'",
    "identifier",
    "string",
    "number",
    "':'",
    "','",
    "EOF",
};

static String _make_indent(StringView p_indent, int p_size) {

    String indent_text;
    indent_text.reserve(p_size*p_indent.size());
    if (!p_indent.empty()) {
        for (int i = 0; i < p_size; i++)
            indent_text.append(p_indent);
    }
    return indent_text;
}

String JSON::_print_var(
        const Variant &p_var, StringView p_indent, int p_cur_indent, bool p_sort_keys, Set<const void *> &p_markers) {

    String colon(":");
    String end_statement;

    if (!p_indent.empty()) {
        colon += " ";
        end_statement += "\n";
    }

    switch (p_var.get_type()) {

        case VariantType::NIL:
            return "null";
        case VariantType::BOOL:
            return p_var.as<bool>() ? "true" : "false";
        case VariantType::INT:
            return itos(p_var.as<int>());
        case VariantType::FLOAT:
            return rtos(p_var.as<float>());
        case VariantType::POOL_INT_ARRAY:
        case VariantType::POOL_FLOAT32_ARRAY:
        case VariantType::POOL_STRING_ARRAY:
        case VariantType::ARRAY: {

            String s("[");
            s += end_statement;
            Array a = p_var.as<Array>();
            ERR_FAIL_COND_V_MSG(p_markers.contains(a.id()), "\"[...]\"", "Converting circular structure to JSON.");
            p_markers.insert(a.id());
            for (int i = 0; i < a.size(); i++) {
                if (i > 0) {
                    s += ",";
                    s += end_statement;
                }
                s += _make_indent(p_indent, p_cur_indent + 1) +
                     _print_var(a[i], p_indent, p_cur_indent + 1, p_sort_keys, p_markers);
            }
            s += end_statement + _make_indent(p_indent, p_cur_indent) + "]";
            p_markers.erase(a.id());
            return s;
        }
        case VariantType::DICTIONARY: {

            String s("{");
            s += end_statement;
            Dictionary d = p_var.as<Dictionary>();
            ERR_FAIL_COND_V_MSG(p_markers.contains(d.id()), "\"{...}\"", "Converting circular structure to JSON.");
            p_markers.insert(d.id());

            auto keys(d.get_key_list());

            if (p_sort_keys)
                eastl::sort(keys.begin(),keys.end());

            for (const auto &E : keys) {

                if (&E != &keys.front()) {
                    s += ",";
                    s += end_statement;
                }
                s += _make_indent(p_indent, p_cur_indent + 1) +
                     _print_var(E, p_indent, p_cur_indent + 1, p_sort_keys, p_markers);
                s += colon;
                s += _print_var(d[E], p_indent, p_cur_indent + 1, p_sort_keys, p_markers);
            }

            s += end_statement + _make_indent(p_indent, p_cur_indent) + "}";
            p_markers.erase(d.id());
            return s;
        }
        default:
            return "\"" + StringUtils::json_escape(p_var.as<String>()) + "\"";
    }
}

String JSON::print(const Variant &p_var, StringView p_indent, bool p_sort_keys) {
    Set<const void *> markers; //TODO: consider using simpler allocator here
    return _print_var(p_var, p_indent, 0, p_sort_keys, markers);

}

Error JSON::_get_token(const CharType *p_str, int &index, int p_len, Token &r_token, int &line, String &r_err_str) {

    while (p_len > 0) {
        switch (p_str[index].toLatin1()) {

            case '\n': {

                line++;
                index++;
                break;
            }
            case 0: {
                r_token.type = TK_EOF;
                return OK;
            }
            case '{': {

                r_token.type = TK_CURLY_BRACKET_OPEN;
                index++;
                return OK;
            }
            case '}': {

                r_token.type = TK_CURLY_BRACKET_CLOSE;
                index++;
                return OK;
            }
            case '[': {

                r_token.type = TK_BRACKET_OPEN;
                index++;
                return OK;
            }
            case ']': {

                r_token.type = TK_BRACKET_CLOSE;
                index++;
                return OK;
            }
            case ':': {

                r_token.type = TK_COLON;
                index++;
                return OK;
            }
            case ',': {

                r_token.type = TK_COMMA;
                index++;
                return OK;
            }
            case '"': {

                index++;
                UIString str;
                while (true) {
                    if (p_str[index] == nullptr) {
                        r_err_str = "Unterminated String";
                        return ERR_PARSE_ERROR;
                    } else if (p_str[index] == '"') {
                        index++;
                        break;
                    } else if (p_str[index] == '\\') {
                        //escaped characters...
                        index++;
                        CharType next = p_str[index];
                        if (next == nullptr) {
                            r_err_str = "Unterminated String";
                            return ERR_PARSE_ERROR;
                        }
                        CharType res = 0;

                        switch (next.toLatin1()) {

                            case 'b': res = 8; break;
                            case 't': res = 9; break;
                            case 'n': res = 10; break;
                            case 'f': res = 12; break;
                            case 'r': res = 13; break;
                            case 'u': {
                                //hexnumbarh - oct is deprecated
                                uint16_t accval=0;
                                for (int j = 0; j < 4; j++) {
                                    CharType c = p_str[index + j + 1];
                                    if (c == nullptr) {
                                        r_err_str = "Unterminated String";
                                        return ERR_PARSE_ERROR;
                                    }
                                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {

                                        r_err_str = "Malformed hex constant in string";
                                        return ERR_PARSE_ERROR;
                                    }
                                    uint16_t v;
                                    if (c >= '0' && c <= '9') {
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

                                    accval <<= 4;
                                    accval |= v;
                                }
                                res = QChar(accval);
                                index += 4; //will add at the end anyway

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

                        str += res;

                    } else {
                        if (p_str[index] == '\n')
                            line++;
                        str += p_str[index];
                    }
                    index++;
                }

                r_token.type = TK_STRING;
                r_token.value = Variant(StringUtils::to_utf8(str));
                return OK;

            }
            default: {

                if (p_str[index] <= 32) {
                    index++;
                    break;
                }

                if (p_str[index] == '-' || (p_str[index] >= '0' && p_str[index] <= '9')) {
                    //a number
                    const CharType *rptr;
                    double number = StringUtils::to_double(&p_str[index], &rptr);
                    index += (rptr - &p_str[index]);
                    r_token.type = TK_NUMBER;
                    r_token.value = number;
                    return OK;

                } else if ((p_str[index] >= 'A' && p_str[index] <= 'Z') || (p_str[index] >= 'a' && p_str[index] <= 'z')) {

                    UIString id;

                    while ((p_str[index] >= 'A' && p_str[index] <= 'Z') || (p_str[index] >= 'a' && p_str[index] <= 'z')) {

                        id += p_str[index];
                        index++;
                    }

                    r_token.type = TK_IDENTIFIER;
                    r_token.value = Variant(StringUtils::to_utf8(id));
                    return OK;
                } else {
                    r_err_str = "Unexpected character.";
                    return ERR_PARSE_ERROR;
                }
            }
        }
    }

    return ERR_PARSE_ERROR;
}

Error JSON::_parse_value(Variant &value, Token &token, const CharType *p_str, int &index, int p_len, int &line, String &r_err_str) {

    if (token.type == TK_CURLY_BRACKET_OPEN) {

        Dictionary d;
        Error err = _parse_object(d, p_str, index, p_len, line, r_err_str);
        if (err)
            return err;
        value = d;
    } else if (token.type == TK_BRACKET_OPEN) {

        Array a;
        Error err = _parse_array(a, p_str, index, p_len, line, r_err_str);
        if (err)
            return err;
        value = a;

    } else if (token.type == TK_IDENTIFIER) {

        String id = token.value.as<String>();
        if (id == "true")
            value = true;
        else if (id == "false")
            value = false;
        else if (id == "null")
            value = Variant();
        else {
            r_err_str = "Expected 'true','false' or 'null', got '" + id + "'.";
            return ERR_PARSE_ERROR;
        }

    } else if (token.type == TK_NUMBER) {

        value = token.value;
    } else if (token.type == TK_STRING) {

        value = token.value;
    } else {
        r_err_str = "Expected value, got " + String(tk_name[token.type]) + ".";
        return ERR_PARSE_ERROR;
    }
    return OK;
}

Error JSON::_parse_array(Array &array, const CharType *p_str, int &index, int p_len, int &line, String &r_err_str) {

    Token token;
    bool need_comma = false;

    while (index < p_len) {

        Error err = _get_token(p_str, index, p_len, token, line, r_err_str);
        if (err != OK)
            return err;

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
        err = _parse_value(v, token, p_str, index, p_len, line, r_err_str);
        if (err)
            return err;

        array.push_back(v);
        need_comma = true;
    }

    r_err_str = "Expected ']'";
    return ERR_PARSE_ERROR;
}

Error JSON::_parse_object(Dictionary &object, const CharType *p_str, int &index, int p_len, int &line, String &r_err_str) {

    bool at_key = true;
    String key;
    Token token;
    bool need_comma = false;

    while (index < p_len) {

        if (at_key) {

            Error err = _get_token(p_str, index, p_len, token, line, r_err_str);
            if (err != OK)
                return err;

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

            if (token.type != TK_STRING) {

                r_err_str = "Expected key";
                return ERR_PARSE_ERROR;
            }

            key = token.value.as<String>();
            err = _get_token(p_str, index, p_len, token, line, r_err_str);
            if (err != OK)
                return err;
            if (token.type != TK_COLON) {

                r_err_str = "Expected ':'";
                return ERR_PARSE_ERROR;
            }
            at_key = false;
        } else {

            Error err = _get_token(p_str, index, p_len, token, line, r_err_str);
            if (err != OK)
                return err;

            Variant v;
            err = _parse_value(v, token, p_str, index, p_len, line, r_err_str);
            if (err)
                return err;
            object[StringName(key)] = Variant(v);
            need_comma = true;
            at_key = true;
        }
    }

    r_err_str = "Expected '}'";
    return ERR_PARSE_ERROR;
}

Error JSON::parse(const String &p_json, Variant &r_ret, String &r_err_str, int &r_err_line) {
    UIString enc(UIString::fromUtf8(p_json.c_str()));
    const CharType *str = enc.constData();
    int idx = 0;
    int len = p_json.length();
    Token token;
    r_err_line = 0;

    Error err = _get_token(str, idx, len, token, r_err_line, r_err_str);
    if (err)
        return err;

    err = _parse_value(r_ret, token, str, idx, len, r_err_line, r_err_str);
    // Check if EOF is reached
    // or it's a type of the next token.
    if (err == OK && idx < len) {
        err = _get_token(str, idx, len, token, r_err_line, r_err_str);

        if (err || token.type != TK_EOF) {
            r_err_str = "Expected 'EOF'";
            // Reset return value to empty `Variant`
            r_ret = Variant();
            return ERR_PARSE_ERROR;
        }
    }
    return err;
}
