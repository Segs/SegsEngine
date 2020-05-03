/*************************************************************************/
/*  gdscript_tokenizer.cpp                                               */
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

#include "gdscript_tokenizer.h"

#include "core/io/marshalls.h"
#include "core/map.h"
#include "core/hash_map.h"
#include "core/variant.h"
#include "core/math/vector2.h"
#include "core/print_string.h"
#include "core/string.h"
#include "core/string_utils.h"

#include "gdscript_functions.h"
#include "EASTL/vector_map.h"

const char *GDScriptTokenizer::token_names[TK_MAX] = {
    "Empty",
    "Identifier",
    "Constant",
    "Self",
    "Built-In Type",
    "Built-In Func",
    "In",
    "'=='",
    "'!='",
    "'<'",
    "'<='",
    "'>'",
    "'>='",
    "'and'",
    "'or'",
    "'not'",
    "'+'",
    "'-'",
    "'*'",
    "'/'",
    "'%'",
    "'<<'",
    "'>>'",
    "'='",
    "'+='",
    "'-='",
    "'*='",
    "'/='",
    "'%='",
    "'<<='",
    "'>>='",
    "'&='",
    "'|='",
    "'^='",
    "'&'",
    "'|'",
    "'^'",
    "'~'",
    //"Plus Plus",
    //"Minus Minus",
    "if",
    "elif",
    "else",
    "for",
    "while",
    "break",
    "continue",
    "pass",
    "return",
    "match",
    "func",
    "class",
    "class_name",
    "extends",
    "is",
    "onready",
    "tool",
    "static",
    "export",
    "setget",
    "const",
    "var",
    "as",
    "void",
    "enum",
    "preload",
    "assert",
    "yield",
    "signal",
    "breakpoint",
    "rpc",
    "sync",
    "master",
    "puppet",
    "slave",
    "remotesync",
    "mastersync",
    "puppetsync",
    "'['",
    "']'",
    "'{'",
    "'}'",
    "'('",
    "')'",
    "','",
    "';'",
    "'.'",
    "'?'",
    "':'",
    "'$'",
    "'->'",
    "'\\n'",
    "PI",
    "TAU",
    "_",
    "INF",
    "NAN",
    "Error",
    "EOF",
    "Cursor"
};

struct _bit {
    VariantType type;
    const char *text;
};
//built in types

static const _bit _type_list[] = {
    //types
    { VariantType::BOOL, "bool" },
    { VariantType::INT, "int" },
    { VariantType::FLOAT, "float" },
    { VariantType::STRING, "String" },
    { VariantType::VECTOR2, "Vector2" },
    { VariantType::RECT2, "Rect2" },
    { VariantType::TRANSFORM2D, "Transform2D" },
    { VariantType::VECTOR3, "Vector3" },
    { VariantType::AABB, "AABB" },
    { VariantType::PLANE, "Plane" },
    { VariantType::QUAT, "Quat" },
    { VariantType::BASIS, "Basis" },
    { VariantType::TRANSFORM, "Transform" },
    { VariantType::COLOR, "Color" },
    { VariantType::_RID, "RID" },
    { VariantType::OBJECT, "Object" },
    { VariantType::NODE_PATH, "NodePath" },
    { VariantType::DICTIONARY, "Dictionary" },
    { VariantType::ARRAY, "Array" },
    { VariantType::POOL_BYTE_ARRAY, "PoolByteArray" },
    { VariantType::POOL_INT_ARRAY, "PoolIntArray" },
    { VariantType::POOL_REAL_ARRAY, "PoolRealArray" },
    { VariantType::POOL_STRING_ARRAY, "PoolStringArray" },
    { VariantType::POOL_VECTOR2_ARRAY, "PoolVector2Array" },
    { VariantType::POOL_VECTOR3_ARRAY, "PoolVector3Array" },
    { VariantType::POOL_COLOR_ARRAY, "PoolColorArray" },
    { VariantType::VARIANT_MAX, nullptr },
};

struct _kws {
    GDScriptTokenizer::Token token;
    const char *text;
};

static const _kws _keyword_list[] = {
    //ops
    { GDScriptTokenizer::TK_OP_IN, "in" },
    { GDScriptTokenizer::TK_OP_NOT, "not" },
    { GDScriptTokenizer::TK_OP_OR, "or" },
    { GDScriptTokenizer::TK_OP_AND, "and" },
    //func
    { GDScriptTokenizer::TK_PR_FUNCTION, "func" },
    { GDScriptTokenizer::TK_PR_CLASS, "class" },
    { GDScriptTokenizer::TK_PR_CLASS_NAME, "class_name" },
    { GDScriptTokenizer::TK_PR_EXTENDS, "extends" },
    { GDScriptTokenizer::TK_PR_IS, "is" },
    { GDScriptTokenizer::TK_PR_ONREADY, "onready" },
    { GDScriptTokenizer::TK_PR_TOOL, "tool" },
    { GDScriptTokenizer::TK_PR_STATIC, "static" },
    { GDScriptTokenizer::TK_PR_EXPORT, "export" },
    { GDScriptTokenizer::TK_PR_SETGET, "setget" },
    { GDScriptTokenizer::TK_PR_VAR, "var" },
    { GDScriptTokenizer::TK_PR_AS, "as" },
    { GDScriptTokenizer::TK_PR_VOID, "void" },
    { GDScriptTokenizer::TK_PR_PRELOAD, "preload" },
    { GDScriptTokenizer::TK_PR_ASSERT, "assert" },
    { GDScriptTokenizer::TK_PR_YIELD, "yield" },
    { GDScriptTokenizer::TK_PR_SIGNAL, "signal" },
    { GDScriptTokenizer::TK_PR_BREAKPOINT, "breakpoint" },
    { GDScriptTokenizer::TK_PR_REMOTE, "remote" },
    { GDScriptTokenizer::TK_PR_MASTER, "master" },
    { GDScriptTokenizer::TK_PR_SLAVE, "slave" },
    { GDScriptTokenizer::TK_PR_PUPPET, "puppet" },
    { GDScriptTokenizer::TK_PR_SYNC, "sync" },
    { GDScriptTokenizer::TK_PR_REMOTESYNC, "remotesync" },
    { GDScriptTokenizer::TK_PR_MASTERSYNC, "mastersync" },
    { GDScriptTokenizer::TK_PR_PUPPETSYNC, "puppetsync" },
    { GDScriptTokenizer::TK_PR_CONST, "const" },
    { GDScriptTokenizer::TK_PR_ENUM, "enum" },
    //controlflow
    { GDScriptTokenizer::TK_CF_IF, "if" },
    { GDScriptTokenizer::TK_CF_ELIF, "elif" },
    { GDScriptTokenizer::TK_CF_ELSE, "else" },
    { GDScriptTokenizer::TK_CF_FOR, "for" },
    { GDScriptTokenizer::TK_CF_WHILE, "while" },
    { GDScriptTokenizer::TK_CF_BREAK, "break" },
    { GDScriptTokenizer::TK_CF_CONTINUE, "continue" },
    { GDScriptTokenizer::TK_CF_RETURN, "return" },
    { GDScriptTokenizer::TK_CF_MATCH, "match" },
    { GDScriptTokenizer::TK_CF_PASS, "pass" },
    { GDScriptTokenizer::TK_SELF, "self" },
    { GDScriptTokenizer::TK_CONST_PI, "PI" },
    { GDScriptTokenizer::TK_CONST_TAU, "TAU" },
    { GDScriptTokenizer::TK_WILDCARD, "_" },
    { GDScriptTokenizer::TK_CONST_INF, "INF" },
    { GDScriptTokenizer::TK_CONST_NAN, "NAN" },
    { GDScriptTokenizer::TK_ERROR, nullptr }
};

const char *GDScriptTokenizer::get_token_name(Token p_token) {

    ERR_FAIL_INDEX_V(p_token, TK_MAX, "<error>");
    return token_names[p_token];
}

bool GDScriptTokenizer::is_token_literal(int p_offset, bool variable_safe) const {
    switch (get_token(p_offset)) {
        // Can always be literal:
        case TK_IDENTIFIER:

        case TK_PR_ONREADY:
        case TK_PR_TOOL:
        case TK_PR_STATIC:
        case TK_PR_EXPORT:
        case TK_PR_SETGET:
        case TK_PR_SIGNAL:
        case TK_PR_REMOTE:
        case TK_PR_MASTER:
        case TK_PR_PUPPET:
        case TK_PR_SYNC:
        case TK_PR_REMOTESYNC:
        case TK_PR_MASTERSYNC:
        case TK_PR_PUPPETSYNC:
            return true;

        // Literal for non-variables only:
        case TK_BUILT_IN_TYPE:
        case TK_BUILT_IN_FUNC:

        case TK_OP_IN:
            //case TK_OP_NOT:
            //case TK_OP_OR:
            //case TK_OP_AND:

        case TK_PR_CLASS:
        case TK_PR_CONST:
        case TK_PR_ENUM:
        case TK_PR_PRELOAD:
        case TK_PR_FUNCTION:
        case TK_PR_EXTENDS:
        case TK_PR_ASSERT:
        case TK_PR_YIELD:
        case TK_PR_VAR:

        case TK_CF_IF:
        case TK_CF_ELIF:
        case TK_CF_ELSE:
        case TK_CF_FOR:
        case TK_CF_WHILE:
        case TK_CF_BREAK:
        case TK_CF_CONTINUE:
        case TK_CF_RETURN:
        case TK_CF_MATCH:
        case TK_CF_PASS:
        case TK_SELF:
        case TK_CONST_PI:
        case TK_CONST_TAU:
        case TK_WILDCARD:
        case TK_CONST_INF:
        case TK_CONST_NAN:
        case TK_ERROR:
            return !variable_safe;

        case TK_CONSTANT: {
            switch (get_token_constant(p_offset).get_type()) {
                case VariantType::NIL:
                case VariantType::BOOL:
                    return true;
                default:
                    return false;
            }
        }
        default:
            return false;
    }
}

StringName GDScriptTokenizer::get_token_literal(int p_offset) const {
    Token token = get_token(p_offset);
    switch (token) {
        case TK_IDENTIFIER:
            return get_token_identifier(p_offset);
        case TK_BUILT_IN_TYPE: {
            VariantType type = get_token_type(p_offset);
            int idx = 0;

            while (_type_list[idx].text) {
                if (type == _type_list[idx].type) {
                    return StringName(_type_list[idx].text);
                }
                idx++;
            }
        } break; // Shouldn't get here, stuff happens
        case TK_BUILT_IN_FUNC:
            return StringName(GDScriptFunctions::get_func_name(get_token_built_in_func(p_offset)));
        case TK_CONSTANT: {
            const Variant value = get_token_constant(p_offset);

            switch (value.get_type()) {
                case VariantType::NIL:
                    return "null";
                case VariantType::BOOL:
                    return value ? StringName("true") : StringName("false");
                default: {
                }
            }
        }
        case TK_OP_AND:
        case TK_OP_OR:
            break; // Don't get into default, since they can be non-literal
        default: {
            int idx = 0;

            while (_keyword_list[idx].text) {
                if (token == _keyword_list[idx].token) {
                    return StringName(_keyword_list[idx].text);
                }
                idx++;
            }
        }
    }
    ERR_FAIL_V_MSG("", "Failed to get token literal.");
}

static bool _is_text_char(char c) {

    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool _is_number(char c) {

    return (c >= '0' && c <= '9');
}

static bool _is_hex(char c) {

    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool _is_bin(char c) {

    return (c == '0' || c == '1');
}

void GDScriptTokenizerText::_make_token(Token p_type) {

    TokenData &tk = tk_rb[tk_rb_pos];

    tk.type = p_type;
    tk.line = line;
    tk.col = column;

    tk_rb_pos = (tk_rb_pos + 1) % TK_RB_SIZE;
}
void GDScriptTokenizerText::_make_identifier(const StringName &p_identifier) {

    TokenData &tk = tk_rb[tk_rb_pos];

    tk.type = TK_IDENTIFIER;
    tk.identifier = p_identifier;
    tk.line = line;
    tk.col = column;

    tk_rb_pos = (tk_rb_pos + 1) % TK_RB_SIZE;
}

void GDScriptTokenizerText::_make_built_in_func(GDScriptFunctions::Function p_func) {

    TokenData &tk = tk_rb[tk_rb_pos];

    tk.type = TK_BUILT_IN_FUNC;
    tk.func = p_func;
    tk.line = line;
    tk.col = column;

    tk_rb_pos = (tk_rb_pos + 1) % TK_RB_SIZE;
}
void GDScriptTokenizerText::_make_constant(const Variant &p_constant) {

    TokenData &tk = tk_rb[tk_rb_pos];

    tk.type = TK_CONSTANT;
    tk.constant = p_constant;
    tk.line = line;
    tk.col = column;

    tk_rb_pos = (tk_rb_pos + 1) % TK_RB_SIZE;
}

void GDScriptTokenizerText::_make_type(const VariantType &p_type) {

    TokenData &tk = tk_rb[tk_rb_pos];

    tk.type = TK_BUILT_IN_TYPE;
    tk.vtype = p_type;
    tk.line = line;
    tk.col = column;

    tk_rb_pos = (tk_rb_pos + 1) % TK_RB_SIZE;
}

//void GDScriptTokenizerText::_make_error(const String &p_error) {

//    error_flag = true;
//    last_error = p_error;

//    TokenData &tk = tk_rb[tk_rb_pos];
//    tk.type = TK_ERROR;
//    tk.constant = p_error;
//    tk.line = line;
//    tk.col = column;
//    tk_rb_pos = (tk_rb_pos + 1) % TK_RB_SIZE;
//}
void GDScriptTokenizerText::_make_error(StringView p_error) {

    error_flag = true;
    last_error = p_error.data();

    TokenData &tk = tk_rb[tk_rb_pos];
    tk.type = TK_ERROR;
    tk.constant = p_error.data();
    tk.line = line;
    tk.col = column;
    tk_rb_pos = (tk_rb_pos + 1) % TK_RB_SIZE;
}

void GDScriptTokenizerText::_make_newline(int p_indentation, int p_tabs) {

    TokenData &tk = tk_rb[tk_rb_pos];
    tk.type = TK_NEWLINE;
    tk.constant = Vector2(p_indentation, p_tabs);
    tk.line = line;
    tk.col = column;
    tk_rb_pos = (tk_rb_pos + 1) % TK_RB_SIZE;
}


void GDScriptTokenizerText::_advance() {

    if (error_flag) {
        //parser broke
        _make_error(last_error);
        return;
    }

    if (code_pos >= len) {
        _make_token(TK_EOF);
        return;
    }
#define GETCHAR(m_ofs) ((m_ofs + code_pos) >= len ? char(0) : _code[m_ofs + code_pos])
#define INCPOS(m_amount)      \
    {                         \
        code_pos += m_amount; \
        column += m_amount;   \
    }
    while (true) {

        bool is_node_path = false;
        StringMode string_mode = STRING_DOUBLE_QUOTE;

        switch (GETCHAR(0)) {
            case 0:
                _make_token(TK_EOF);
                break;
            case '\\':
                INCPOS(1);
                if (GETCHAR(0) == '\r') {
                    INCPOS(1);
                }

                if (GETCHAR(0) != '\n') {
                    _make_error("Expected newline after '\\'.");
                    return;
                }

                INCPOS(1);
                line++;

                while (GETCHAR(0) == ' ' || GETCHAR(0) == '\t') {
                    INCPOS(1);
                }

                continue;
            case '\t':
            case '\r':
            case ' ':
                INCPOS(1);
                continue;
            case '#': { // line comment skip
#ifdef DEBUG_ENABLED
                String comment;
#endif // DEBUG_ENABLED
                while (GETCHAR(0) != '\n') {
#ifdef DEBUG_ENABLED
                    comment += GETCHAR(0);
#endif // DEBUG_ENABLED
                    code_pos++;
                    if (GETCHAR(0) == 0) { //end of file
                        //_make_error("Unterminated Comment");
                        _make_token(TK_EOF);
                        return;
                    }
                }
#ifdef DEBUG_ENABLED
                StringView comment_content = StringUtils::trim_prefix(StringUtils::trim_prefix(comment,"#")," ");
                if (StringUtils::begins_with(comment_content,"warning-ignore:")) {
                    StringView code = StringUtils::get_slice(comment_content,':', 1);
                    warning_skips.push_back(Pair<int, String>(line, StringUtils::to_lower(StringUtils::strip_edges(code))));
                } else if (StringUtils::begins_with(comment_content,"warning-ignore-all:")) {
                    StringView code = StringUtils::get_slice(comment_content,':', 1);
                    warning_global_skips.insert(StringUtils::to_lower(StringUtils::strip_edges(code)));
                } else if (StringUtils::strip_edges(comment_content) == StringView("warnings-disable")) {
                    ignore_warnings = true;
                }
#endif // DEBUG_ENABLED
                [[fallthrough]];
            }
        case '\n': {
            line++;
            INCPOS(1);
            bool used_spaces = false;
            int tabs = 0;
            column = 1;
            int i = 0;
            while (true) {
                if (GETCHAR(i) == ' ') {
                    i++;
                    used_spaces = true;
                } else if (GETCHAR(i) == '\t') {
                    if (used_spaces) {
                        _make_error("Spaces used before tabs on a line");
                        return;
                    }
                    i++;
                    tabs++;
                } else {
                    break; // not indentation anymore
                }
            }

            _make_newline(i, tabs);
            return;
        }
            case '/': {

                switch (GETCHAR(1)) {
                    case '=': { // diveq

                        _make_token(TK_OP_ASSIGN_DIV);
                        INCPOS(1);

                    } break;
                    default:
                        _make_token(TK_OP_DIV);
                }
            } break;
            case '=': {
                if (GETCHAR(1) == '=') {
                    _make_token(TK_OP_EQUAL);
                    INCPOS(1);

                } else
                    _make_token(TK_OP_ASSIGN);

            } break;
            case '<': {
                if (GETCHAR(1) == '=') {

                    _make_token(TK_OP_LESS_EQUAL);
                    INCPOS(1);
                } else if (GETCHAR(1) == '<') {
                    if (GETCHAR(2) == '=') {
                        _make_token(TK_OP_ASSIGN_SHIFT_LEFT);
                        INCPOS(1);
                    } else {
                        _make_token(TK_OP_SHIFT_LEFT);
                    }
                    INCPOS(1);
                } else
                    _make_token(TK_OP_LESS);

            } break;
            case '>': {
                if (GETCHAR(1) == '=') {
                    _make_token(TK_OP_GREATER_EQUAL);
                    INCPOS(1);
                } else if (GETCHAR(1) == '>') {
                    if (GETCHAR(2) == '=') {
                        _make_token(TK_OP_ASSIGN_SHIFT_RIGHT);
                        INCPOS(1);

                    } else {
                        _make_token(TK_OP_SHIFT_RIGHT);
                    }
                    INCPOS(1);
                } else {
                    _make_token(TK_OP_GREATER);
                }

            } break;
            case '!': {
                if (GETCHAR(1) == '=') {
                    _make_token(TK_OP_NOT_EQUAL);
                    INCPOS(1);
                } else {
                    _make_token(TK_OP_NOT);
                }

            } break;
            //case '"' //string - no strings in shader
            //case '\'' //string - no strings in shader
            case '{':
                _make_token(TK_CURLY_BRACKET_OPEN);
                break;
            case '}':
                _make_token(TK_CURLY_BRACKET_CLOSE);
                break;
            case '[':
                _make_token(TK_BRACKET_OPEN);
                break;
            case ']':
                _make_token(TK_BRACKET_CLOSE);
                break;
            case '(':
                _make_token(TK_PARENTHESIS_OPEN);
                break;
            case ')':
                _make_token(TK_PARENTHESIS_CLOSE);
                break;
            case ',':
                _make_token(TK_COMMA);
                break;
            case ';':
                _make_token(TK_SEMICOLON);
                break;
            case '?':
                _make_token(TK_QUESTION_MARK);
                break;
            case ':':
                _make_token(TK_COLON); //for methods maybe but now useless.
                break;
            case '$':
                _make_token(TK_DOLLAR); //for the get_node() shortener
                break;
            case '^': {
                if (GETCHAR(1) == '=') {
                    _make_token(TK_OP_ASSIGN_BIT_XOR);
                    INCPOS(1);
                } else {
                    _make_token(TK_OP_BIT_XOR);
                }

            } break;
            case '~':
                _make_token(TK_OP_BIT_INVERT);
                break;
            case '&': {
                if (GETCHAR(1) == '&') {

                    _make_token(TK_OP_AND);
                    INCPOS(1);
                } else if (GETCHAR(1) == '=') {
                    _make_token(TK_OP_ASSIGN_BIT_AND);
                    INCPOS(1);
                } else {
                    _make_token(TK_OP_BIT_AND);
                }
            } break;
            case '|': {
                if (GETCHAR(1) == '|') {

                    _make_token(TK_OP_OR);
                    INCPOS(1);
                } else if (GETCHAR(1) == '=') {
                    _make_token(TK_OP_ASSIGN_BIT_OR);
                    INCPOS(1);
                } else {
                    _make_token(TK_OP_BIT_OR);
                }
            } break;
            case '*': {

                if (GETCHAR(1) == '=') {
                    _make_token(TK_OP_ASSIGN_MUL);
                    INCPOS(1);
                } else {
                    _make_token(TK_OP_MUL);
                }
            } break;
            case '+': {

                if (GETCHAR(1) == '=') {
                    _make_token(TK_OP_ASSIGN_ADD);
                    INCPOS(1);
                    /*
                }  else if (GETCHAR(1)=='+') {
                    _make_token(TK_OP_PLUS_PLUS);
                    INCPOS(1);
                */
                } else {
                    _make_token(TK_OP_ADD);
                }

            } break;
            case '-': {

                if (GETCHAR(1) == '=') {
                    _make_token(TK_OP_ASSIGN_SUB);
                    INCPOS(1);
                } else if (GETCHAR(1) == '>') {
                    _make_token(TK_FORWARD_ARROW);
                    INCPOS(1);
                } else {
                    _make_token(TK_OP_SUB);
                }
            } break;
            case '%': {

                if (GETCHAR(1) == '=') {
                    _make_token(TK_OP_ASSIGN_MOD);
                    INCPOS(1);
                } else {
                    _make_token(TK_OP_MOD);
                }
            } break;
            case '@':
                if (char(GETCHAR(1)) != '"' && char(GETCHAR(1)) != '\'') {
                    _make_error("Unexpected '@'");
                    return;
                }
                INCPOS(1);
                is_node_path = true;
                [[fallthrough]];
            case '\'':
            case '"': {

                if (GETCHAR(0) == '\'')
                    string_mode = STRING_SINGLE_QUOTE;

                int i = 1;
                if (string_mode == STRING_DOUBLE_QUOTE && GETCHAR(i) == '"' && GETCHAR(i + 1) == '"') {
                    i += 2;
                    string_mode = STRING_MULTILINE;
                }

                String str;
                while (true) {
                    if (GETCHAR(i) == 0) {

                        _make_error("Unterminated String");
                        return;
                    } else if (string_mode == STRING_DOUBLE_QUOTE && char(GETCHAR(i)) == '"') {
                        break;
                    } else if (string_mode == STRING_SINGLE_QUOTE && char(GETCHAR(i)) == '\'') {
                        break;
                    } else if (string_mode == STRING_MULTILINE && char(GETCHAR(i)) == '\"' && char(GETCHAR(i + 1)) == '\"' && char(GETCHAR(i + 2)) == '\"') {
                        i += 2;
                        break;
                    } else if (string_mode != STRING_MULTILINE && char(GETCHAR(i)) == '\n') {
                        _make_error("Unexpected EOL at String.");
                        return;
                    } else if (GETCHAR(i) == c_cursor_marker) {
                        //string ends here, next will be TK
                        i--;
                        break;
                    } else if (GETCHAR(i) == '\\') {
                        //escaped characters...
                        i++;
                        char next = GETCHAR(i);
                        if (next == 0) {
                            _make_error("Unterminated String");
                            return;
                        }
                        char res = 0;

                        switch (next) {

                            case 'a': res = 7; break;
                            case 'b': res = 8; break;
                            case 't': res = 9; break;
                            case 'n': res = 10; break;
                            case 'v': res = 11; break;
                            case 'f': res = 12; break;
                            case 'r': res = 13; break;
                            case '\'': res = '\''; break;
                            case '\"': res = '\"'; break;
                            case '\\': res = '\\'; break;
                            case '/':
                                res = '/';
                                break; //wtf

                            case 'u': {
                                //hexnumbarh - oct is deprecated
                                i += 1;
                                uint16_t accval=0;
                                for (int j = 0; j < 4; j++) {
                                    char c = GETCHAR(i + j);
                                    if (c == 0) {
                                        _make_error("Unterminated String");
                                        return;
                                    }
                                    uint16_t v;
                                    if (c >= '0' && c <= '9') {
                                        v = c-'0';
                                    } else if (c >= 'a' && c <= 'f') {
                                        v = c - 'a';
                                        v += 10;
                                    } else if (c >= 'A' && c <= 'F') {
                                        v = c - 'A';
                                        v += 10;
                                    } else {
                                        _make_error("Malformed hex constant in string");
                                        return;
                                    }

                                    accval <<= 4;
                                    accval |= v;
                                }
                                res = accval;
                                i += 3;

                            } break;
                            default: {

                                _make_error("Invalid escape sequence");
                                return;
                            } break;
                        }

                        str += res;

                    } else {
                        if (GETCHAR(i) == '\n') {
                            line++;
                            column = 1;
                        }

                        str += GETCHAR(i);
                    }
                    i++;
                }
                INCPOS(i);

                if (is_node_path) {
                    _make_constant(NodePath(str));
                } else {
                    _make_constant(str);
                }

            } break;
            case c_cursor_marker: {
                _make_token(TK_CURSOR);
            } break;
            default: {

                if (_is_number(GETCHAR(0)) || (GETCHAR(0) == '.' && _is_number(GETCHAR(1)))) {
                    // parse number
                    bool period_found = false;
                    bool exponent_found = false;
                    bool hexa_found = false;
                    bool bin_found = false;
                    bool sign_found = false;

                    String str;
                    int i = 0;

                    while (true) {
                        if (GETCHAR(i) == '.') {
                            if (period_found || exponent_found) {
                                _make_error("Invalid numeric constant at '.'");
                                return;
                            } else if (bin_found) {
                                _make_error("Invalid binary constant at '.'");
                                return;
                            } else if (hexa_found) {
                                _make_error("Invalid hexadecimal constant at '.'");
                                return;
                            }
                            period_found = true;
                        } else if (GETCHAR(i) == 'x') {
                            if (hexa_found || bin_found || str.length() != 1 || !((i == 1 && str[0] == '0') || (i == 2 && str[1] == '0' && str[0] == '-'))) {
                                _make_error("Invalid numeric constant at 'x'");
                                return;
                            }
                            hexa_found = true;
                        } else if (hexa_found && _is_hex(GETCHAR(i))) {

                        } else if (!hexa_found && GETCHAR(i) == 'b') {
                            if (bin_found || str.length() != 1 || !((i == 1 && str[0] == '0') || (i == 2 && str[1] == '0' && str[0] == '-'))) {
                                _make_error("Invalid numeric constant at 'b'");
                                return;
                            }
                            bin_found = true;
                        }  else if (!hexa_found && GETCHAR(i) == 'e') {
                            if (exponent_found || bin_found) {
                                _make_error("Invalid numeric constant at 'e'");
                                return;
                            }
                            exponent_found = true;
                        } else if (_is_number(GETCHAR(i))) {
                            //all ok
                        } else if (hexa_found && _is_hex(GETCHAR(i))) {

                        } else if (bin_found && _is_bin(GETCHAR(i))) {

                        } else if ((GETCHAR(i) == '-' || GETCHAR(i) == '+') && exponent_found) {
                            if (sign_found) {
                                _make_error("Invalid numeric constant at '-'");
                                return;
                            }
                            sign_found = true;
                        } else if (GETCHAR(i) == '_') {
                            i++;
                            continue; // Included for readability, shouldn't be a part of the string
                        } else
                            break;

                        str += GETCHAR(i);
                        i++;
                    }

                    if (!(_is_number(str[str.length() - 1]) || (hexa_found && _is_hex(str[str.length() - 1])))) {
                        _make_error("Invalid numeric constant: " + str);
                        return;
                    }

                    INCPOS(i);
                    if (hexa_found) {
                        int64_t val = StringUtils::hex_to_int64(str);
                        _make_constant(val);
                    } else if (bin_found) {
                        int64_t val = StringUtils::bin_to_int64(str);
                        _make_constant(val);
                    } else if (period_found || exponent_found) {
                        double val = StringUtils::to_double(str);
                        _make_constant(val);
                    } else {
                        int64_t val = StringUtils::to_int64(str);
                        _make_constant(val);
                    }

                    return;
                }

                if (GETCHAR(0) == '.') {
                    //parse period
                    _make_token(TK_PERIOD);
                    break;
                }

                if (_is_text_char(GETCHAR(0))) {
                    // parse identifier
                    String str;
                    str += GETCHAR(0);

                    int i = 1;
                    while (_is_text_char(GETCHAR(i))) {
                        str += GETCHAR(i);
                        i++;
                    }

                    bool identifier = false;

                    if (str == "null") {
                        _make_constant(Variant());

                    } else if (str == "true") {
                        _make_constant(true);

                    } else if (str == "false") {
                        _make_constant(false);
                    } else {

                        bool found = false;

                        {

                            int idx = 0;

                            while (_type_list[idx].text) {

                                if (str == _type_list[idx].text) {
                                    _make_type(_type_list[idx].type);
                                    found = true;
                                    break;
                                }
                                idx++;
                            }
                        }

                        if (!found) {

                            //built in func?

                            for (int j = 0; j < GDScriptFunctions::FUNC_MAX; j++) {

                                if (str == GDScriptFunctions::get_func_name(GDScriptFunctions::Function(j))) {

                                    _make_built_in_func(GDScriptFunctions::Function(j));
                                    found = true;
                                    break;
                                }
                            }
                        }

                        if (!found) {
                            //keyword

                            int idx = 0;
                            found = false;

                            while (_keyword_list[idx].text) {

                                if (str == _keyword_list[idx].text) {
                                    _make_token(_keyword_list[idx].token);
                                    found = true;
                                    break;
                                }
                                idx++;
                            }
                        }

                        if (!found)
                            identifier = true;
                    }

                    if (identifier) {
                        _make_identifier(StringName(str));
                    }
                    INCPOS(str.length());
                    return;
                }

                _make_error("Unknown character");
                return;

            }
        }

        INCPOS(1);
        break;
    }
}

void GDScriptTokenizerText::set_code(StringView p_code) {

    code = p_code;
    len = p_code.length();
    if (len) {
        _code = code.c_str();
    } else {
        _code = nullptr;
    }
    code_pos = 0;
    line = 1; //it is stand-ar-ized that lines begin in 1 in code..
    column = 1; //the same holds for columns
    tk_rb_pos = 0;
    error_flag = false;
#ifdef DEBUG_ENABLED
    ignore_warnings = false;
#endif // DEBUG_ENABLED
    last_error = "";
    for (int i = 0; i < MAX_LOOKAHEAD + 1; i++)
        _advance();
}

GDScriptTokenizerText::Token GDScriptTokenizerText::get_token(int p_offset) const {
    ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, TK_ERROR);
    ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, TK_ERROR);

    int ofs = (TK_RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % TK_RB_SIZE;
    return tk_rb[ofs].type;
}

int GDScriptTokenizerText::get_token_line(int p_offset) const {
    ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, -1);
    ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, -1);

    int ofs = (TK_RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % TK_RB_SIZE;
    return tk_rb[ofs].line;
}

int GDScriptTokenizerText::get_token_column(int p_offset) const {
    ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, -1);
    ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, -1);

    int ofs = (TK_RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % TK_RB_SIZE;
    return tk_rb[ofs].col;
}

const Variant &GDScriptTokenizerText::get_token_constant(int p_offset) const {
    ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, tk_rb[0].constant);
    ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, tk_rb[0].constant);

    int ofs = (TK_RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % TK_RB_SIZE;
    ERR_FAIL_COND_V(tk_rb[ofs].type != TK_CONSTANT, tk_rb[0].constant);
    return tk_rb[ofs].constant;
}

StringName GDScriptTokenizerText::get_token_identifier(int p_offset) const {

    ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, StringName());
    ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, StringName());

    int ofs = (TK_RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % TK_RB_SIZE;
    ERR_FAIL_COND_V(tk_rb[ofs].type != TK_IDENTIFIER, StringName());
    return tk_rb[ofs].identifier;
}

GDScriptFunctions::Function GDScriptTokenizerText::get_token_built_in_func(int p_offset) const {

    ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, GDScriptFunctions::FUNC_MAX);
    ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, GDScriptFunctions::FUNC_MAX);

    int ofs = (TK_RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % TK_RB_SIZE;
    ERR_FAIL_COND_V(tk_rb[ofs].type != TK_BUILT_IN_FUNC, GDScriptFunctions::FUNC_MAX);
    return tk_rb[ofs].func;
}

VariantType GDScriptTokenizerText::get_token_type(int p_offset) const {

    ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, VariantType::NIL);
    ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, VariantType::NIL);

    int ofs = (TK_RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % TK_RB_SIZE;
    ERR_FAIL_COND_V(tk_rb[ofs].type != TK_BUILT_IN_TYPE, VariantType::NIL);
    return tk_rb[ofs].vtype;
}

int GDScriptTokenizerText::get_token_line_indent(int p_offset) const {

    ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, 0);
    ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, 0);

    int ofs = (TK_RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % TK_RB_SIZE;
    ERR_FAIL_COND_V(tk_rb[ofs].type != TK_NEWLINE, 0);
    return tk_rb[ofs].constant.operator Vector2().x;
}

int GDScriptTokenizerText::get_token_line_tab_indent(int p_offset) const {

    ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, 0);
    ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, 0);

    int ofs = (TK_RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % TK_RB_SIZE;
    ERR_FAIL_COND_V(tk_rb[ofs].type != TK_NEWLINE, 0);
    return tk_rb[ofs].constant.operator Vector2().y;
}

String GDScriptTokenizerText::get_token_error(int p_offset) const {

    ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, String());
    ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, String());

    int ofs = (TK_RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % TK_RB_SIZE;
    ERR_FAIL_COND_V(tk_rb[ofs].type != TK_ERROR, String());
    return tk_rb[ofs].constant;
}

void GDScriptTokenizerText::advance(int p_amount) {

    ERR_FAIL_COND(p_amount <= 0);
    for (int i = 0; i < p_amount; i++)
        _advance();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

#define BYTECODE_VERSION 13

struct TokenizerBufferPrivate {
    Vector<StringName> identifiers;
    Vector<Variant> constants;
    eastl::vector_map<uint32_t, uint32_t,eastl::less<uint32_t>,wrap_allocator> lines;
    Vector<uint32_t> tokens;
    Variant nil;
    int token=0;

    void advance(int p_amount) {
        ERR_FAIL_INDEX(p_amount + token, tokens.size());
        token += p_amount;
    }
};
#define D() ((TokenizerBufferPrivate *)m_private_data)

Error GDScriptTokenizerBuffer::set_code_buffer(const Vector<uint8_t> &p_buffer) {

    const uint8_t *buf = p_buffer.data();
    int total_len = p_buffer.size();
    ERR_FAIL_COND_V(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', ERR_INVALID_DATA);

    int version = decode_uint32(&buf[4]);
    ERR_FAIL_COND_V_MSG(version > BYTECODE_VERSION, ERR_INVALID_DATA, "Bytecode is too recent! Please use a newer engine version.");

    int identifier_count = decode_uint32(&buf[8]);
    int constant_count = decode_uint32(&buf[12]);
    int line_count = decode_uint32(&buf[16]);
    uint32_t token_count = decode_uint32(&buf[20]);

    const uint8_t *b = &buf[24];
    total_len -= 24;

    D()->identifiers.reserve(identifier_count);
    for (int i = 0; i < identifier_count; i++) {

        int len = decode_uint32(b);
        ERR_FAIL_COND_V(len > total_len, ERR_INVALID_DATA);
        b += 4;
        Vector<uint8_t> cs;
        cs.resize(len);
        for (int j = 0; j < len; j++) {
            cs[j] = b[j] ^ 0xb6;
        }

        cs[cs.size() - 1] = 0;
        String s((const char *)cs.data());
        b += len;
        total_len -= len + 4;
        D()->identifiers.emplace_back(s);
    }

    D()->constants.reserve(constant_count);
    for (int i = 0; i < constant_count; i++) {

        Variant v;
        int len;
        // An object cannot be constant, never decode objects
        Error err = decode_variant(v, b, total_len, &len, false);
        if (err)
            return err;
        b += len;
        total_len -= len;
        D()->constants.emplace_back(eastl::move(v));
    }

    ERR_FAIL_COND_V(line_count * 8 > total_len, ERR_INVALID_DATA);

    for (int i = 0; i < line_count; i++) {

        uint32_t token = decode_uint32(b);
        b += 4;
        uint32_t linecol = decode_uint32(b);
        b += 4;

        D()->lines[token] = linecol;
        total_len -= 8;
    }

    D()->tokens.resize(token_count);

    for (uint32_t i = 0; i < token_count; i++) {

        ERR_FAIL_COND_V(total_len < 1, ERR_INVALID_DATA);

        if ((*b) & TOKEN_BYTE_MASK) { //little endian always
            ERR_FAIL_COND_V(total_len < 4, ERR_INVALID_DATA);

            D()->tokens[i] = decode_uint32(b) & ~TOKEN_BYTE_MASK;
            b += 4;
        } else {
            D()->tokens[i] = *b;
            b += 1;
            total_len--;
        }
    }

    D()->token = 0;

    return OK;
}

Vector<uint8_t> GDScriptTokenizerBuffer::parse_code_string(StringView p_code) {

    Vector<uint8_t> buf;

    HashMap<StringName, int> identifier_map;
    HashMap<Variant, int, Hasher<Variant>, VariantComparator> constant_map;
    Map<uint32_t, int> line_map;
    Vector<uint32_t> token_array;

    GDScriptTokenizerText tt;
    tt.set_code(p_code);
    int line = -1;

    while (true) {

        if (tt.get_token_line() != line) {

            line = tt.get_token_line();
            line_map[line] = token_array.size();
        }

        uint32_t token = tt.get_token();
        switch (tt.get_token()) {

            case TK_IDENTIFIER: {
                StringName id = tt.get_token_identifier();
                if (!identifier_map.contains(id)) {
                    int idx = identifier_map.size();
                    identifier_map[id] = idx;
                }
                token |= identifier_map[id] << TOKEN_BITS;
            } break;
            case TK_CONSTANT: {

                const Variant &c = tt.get_token_constant();
                if (!constant_map.contains(c)) {
                    int idx = constant_map.size();
                    constant_map[c] = idx;
                }
                token |= constant_map[c] << TOKEN_BITS;
            } break;
            case TK_BUILT_IN_TYPE: {

                token |= int(tt.get_token_type()) << TOKEN_BITS;
            } break;
            case TK_BUILT_IN_FUNC: {

                token |= tt.get_token_built_in_func() << TOKEN_BITS;

            } break;
            case TK_NEWLINE: {

                token |= tt.get_token_line_indent() << TOKEN_BITS;
            } break;
            case TK_ERROR: {

                ERR_FAIL_V(Vector<uint8_t>());
            } break;
            default: {
            }
        }

        token_array.push_back(token);

        if (tt.get_token() == TK_EOF)
            break;
        tt.advance();
    }

    //reverse maps

    Map<int, StringName> rev_identifier_map;
    for (eastl::pair<const StringName,int> &E : identifier_map) {
        rev_identifier_map[E.second] = E.first;
    }

    Map<int, Variant> rev_constant_map;
    for (eastl::pair<const Variant,int> &E : constant_map) {
        rev_constant_map[E.second] = E.first;
    }

    Map<int, uint32_t> rev_line_map;
    for (eastl::pair<const uint32_t,int> &E : line_map) {
        rev_line_map[E.second] = E.first;
    }

    //save header
    buf.resize(24);
    buf[0] = 'G';
    buf[1] = 'D';
    buf[2] = 'S';
    buf[3] = 'C';
    encode_uint32(BYTECODE_VERSION, buf.data()+4);
    encode_uint32(identifier_map.size(), buf.data()+8);
    encode_uint32(constant_map.size(), buf.data()+12);
    encode_uint32(line_map.size(), buf.data()+16);
    encode_uint32(token_array.size(), buf.data()+20);

    //save identifiers

    for (eastl::pair<const int,StringName> &E : rev_identifier_map) {

        StringView cs(E.second);
        int len = cs.length() + 1;
        int extra = 4 - (len % 4);
        if (extra == 4)
            extra = 0;

        uint8_t ibuf[4];
        encode_uint32(len + extra, ibuf);
        for (int i = 0; i < 4; i++) {
            buf.push_back(ibuf[i]);
        }
        for (int i = 0; i < len; i++) {
            buf.push_back(cs[i] ^ 0xb6);
        }
        for (int i = 0; i < extra; i++) {
            buf.push_back(0 ^ 0xb6);
        }
    }

    for (eastl::pair<const int,Variant> &E : rev_constant_map) {

        int len;
        // Objects cannot be constant, never encode objects
        Error err = encode_variant(E.second, nullptr, len, false);
        ERR_FAIL_COND_V_MSG(err != OK, Vector<uint8_t>(), "Error when trying to encode Variant.");
        int pos = buf.size();
        buf.resize(pos + len);
        encode_variant(E.second, &buf[pos], len, false);
    }

    for (eastl::pair<const int,uint32_t> &E : rev_line_map) {

        uint8_t ibuf[8];
        encode_uint32(E.first, &ibuf[0]);
        encode_uint32(E.second, &ibuf[4]);
        for (int i = 0; i < 8; i++)
            buf.push_back(ibuf[i]);
    }

    for (size_t i = 0; i < token_array.size(); i++) {

        uint32_t token = token_array[i];

        if (token & ~TOKEN_MASK) {
            uint8_t buf4[4];
            encode_uint32(token_array[i] | TOKEN_BYTE_MASK, &buf4[0]);
            for (int j = 0; j < 4; j++) {
                buf.push_back(buf4[j]);
            }
        } else {
            buf.push_back(token);
        }
    }

    return buf;
}

GDScriptTokenizerBuffer::Token GDScriptTokenizerBuffer::get_token(int p_offset) const {

    int offset = D()->token + p_offset;

    if (offset < 0 || offset >= D()->tokens.size())
        return TK_EOF;

    return GDScriptTokenizerBuffer::Token(D()->tokens[offset] & TOKEN_MASK);
}

StringName GDScriptTokenizerBuffer::get_token_identifier(int p_offset) const {

    int offset = D()->token + p_offset;

    ERR_FAIL_INDEX_V(offset, D()->tokens.size(), StringName());
    uint32_t identifier = D()->tokens[offset] >> TOKEN_BITS;
    ERR_FAIL_UNSIGNED_INDEX_V(identifier, (uint32_t)D()->identifiers.size(), StringName());

    return D()->identifiers[identifier];
}

GDScriptFunctions::Function GDScriptTokenizerBuffer::get_token_built_in_func(int p_offset) const {

    int offset = D()->token + p_offset;
    ERR_FAIL_INDEX_V(offset, D()->tokens.size(), GDScriptFunctions::FUNC_MAX);
    return GDScriptFunctions::Function(D()->tokens[offset] >> TOKEN_BITS);
}

VariantType GDScriptTokenizerBuffer::get_token_type(int p_offset) const {

    int offset = D()->token + p_offset;
    ERR_FAIL_INDEX_V(offset, D()->tokens.size(), VariantType::NIL);

    return VariantType(D()->tokens[offset] >> TOKEN_BITS);
}

int GDScriptTokenizerBuffer::get_token_line(int p_offset) const {

    int offset = D()->token + p_offset;
    auto iter=D()->lines.lower_bound(offset);
    if(iter==D()->lines.begin()) {
        if(offset<int(iter->first))
            return -1;
    }
    uint32_t l;
    if(iter==D()->lines.end())
        l=D()->lines.back().second;
    else
        l=iter->second;

    return l & TOKEN_LINE_MASK;
}
int GDScriptTokenizerBuffer::get_token_column(int p_offset) const {

    int offset = D()->token + p_offset;
    auto iter=D()->lines.lower_bound(offset);
    if(iter==D()->lines.begin()) {
        if(offset<int(iter->first))
            return -1;
    }
    uint32_t l;
    if(iter==D()->lines.end())
        l=D()->lines.back().second;
    else
        l=iter->second;

    return l >> TOKEN_LINE_BITS;
}
int GDScriptTokenizerBuffer::get_token_line_indent(int p_offset) const {

    int offset = D()->token + p_offset;
    ERR_FAIL_INDEX_V(offset, D()->tokens.size(), 0);
    return D()->tokens[offset] >> TOKEN_BITS;
}
const Variant &GDScriptTokenizerBuffer::get_token_constant(int p_offset) const {

    int offset = D()->token + p_offset;
    ERR_FAIL_INDEX_V(offset, D()->tokens.size(), D()->nil);
    uint32_t constant = D()->tokens[offset] >> TOKEN_BITS;
    ERR_FAIL_UNSIGNED_INDEX_V(constant, (uint32_t)D()->constants.size(), D()->nil);
    return D()->constants[constant];
}
String GDScriptTokenizerBuffer::get_token_error(int p_offset) const {

    ERR_FAIL_V(String());
}

void GDScriptTokenizerBuffer::advance(int p_amount) {

    D()->advance(p_amount);
}
GDScriptTokenizerBuffer::GDScriptTokenizerBuffer() {
    m_private_data = memnew(TokenizerBufferPrivate);
}
GDScriptTokenizerBuffer::~GDScriptTokenizerBuffer() {
    memdelete(D());
    m_private_data = nullptr;
}
