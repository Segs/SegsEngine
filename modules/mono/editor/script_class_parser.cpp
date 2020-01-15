/*************************************************************************/
/*  script_class_parser.cpp                                              */
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

#include "script_class_parser.h"

#include "core/map.h"
#include "core/os/os.h"
#include "core/string_formatter.h"

#include "../utils/string_utils.h"

const char *ScriptClassParser::token_names[ScriptClassParser::TK_MAX] = {
    "[",
    "]",
    "{",
    "}",
    ".",
    ":",
    ",",
    "Symbol",
    "Identifier",
    "String",
    "Number",
    "<",
    ">",
    "EOF",
    "Error"
};

se_string ScriptClassParser::get_token_name(ScriptClassParser::Token p_token) {

    ERR_FAIL_INDEX_V(p_token, TK_MAX, "<error>")
    return token_names[p_token];
}

ScriptClassParser::Token ScriptClassParser::get_token() {

    while (true) {
        switch (code[idx]) {
            case '\n': {
                line++;
                idx++;
                break;
            }
            case 0: {
                return TK_EOF;
            } break;
            case '{': {
                idx++;
                return TK_CURLY_BRACKET_OPEN;
            }
            case '}': {
                idx++;
                return TK_CURLY_BRACKET_CLOSE;
            }
            case '[': {
                idx++;
                return TK_BRACKET_OPEN;
            }
            case ']': {
                idx++;
                return TK_BRACKET_CLOSE;
            }
            case '<': {
                idx++;
                return TK_OP_LESS;
            }
            case '>': {
                idx++;
                return TK_OP_GREATER;
            }
            case ':': {
                idx++;
                return TK_COLON;
            }
            case ',': {
                idx++;
                return TK_COMMA;
            }
            case '.': {
                idx++;
                return TK_PERIOD;
            }
            case '#': {
                //compiler directive
                while (code[idx] != '\n' && code[idx] != 0) {
                    idx++;
                }
                continue;
            } break;
            case '/': {
                switch (code[idx + 1]) {
                    case '*': { // block comment
                        idx += 2;
                        while (true) {
                            if (code[idx] == 0) {
                                error_str = "Unterminated comment";
                                error = true;
                                return TK_ERROR;
                            } else if (code[idx] == '*' && code[idx + 1] == '/') {
                                idx += 2;
                                break;
                            } else if (code[idx] == '\n') {
                                line++;
                            }

                            idx++;
                        }

                    } break;
                    case '/': { // line comment skip
                        while (code[idx] != '\n' && code[idx] != 0) {
                            idx++;
                        }

                    } break;
                    default: {
                        value = "/";
                        idx++;
                        return TK_SYMBOL;
                    }
                }

                continue; // a comment
            } break;
            case '\'':
            case '"': {
                bool verbatim = idx != 0 && code[idx - 1] == '@';

                CharType begin_str = code[idx];
                idx++;
                se_string tk_string;
                while (true) {
                    if (code[idx] == 0) {
                        error_str = "Unterminated String";
                        error = true;
                        return TK_ERROR;
                    } else if (code[idx] == begin_str) {
                        if (verbatim && code[idx + 1] == '"') { // '""' is verbatim string's '\"'
                            idx += 2; // skip next '"' as well
                            continue;
                        }

                        idx += 1;
                        break;
                    } else if (code[idx] == '\\' && !verbatim) {
                        //escaped characters...
                        idx++;
                        char next = code[idx];
                        if (next == 0) {
                            error_str = "Unterminated String";
                            error = true;
                            return TK_ERROR;
                        }
                        char res = 0;

                        switch (next) {
                            case 'b': res = 8; break;
                            case 't': res = 9; break;
                            case 'n': res = 10; break;
                            case 'f': res = 12; break;
                            case 'r':
                                res = 13;
                                break;
                            case '\"': res = '\"'; break;
                            case '\\':
                                res = '\\';
                                break;
                            default: {
                                res = next;
                            } break;
                        }

                        tk_string += res;

                    } else {
                        if (code[idx] == '\n')
                            line++;
                        tk_string += code[idx];
                    }
                    idx++;
                }

                value = tk_string;

                return TK_STRING;
            } break;
            default: {
                if (code[idx] <= 32) {
                    idx++;
                    break;
                }

                if ((code[idx] >= 33 && code[idx] <= 47) || (code[idx] >= 58 && code[idx] <= 63) || (code[idx] >= 91 && code[idx] <= 94) || code[idx] == 96 || (code[idx] >= 123 && code[idx] <= 127)) {
                    value = se_string({code[idx]});
                    idx++;
                    return TK_SYMBOL;
                }

                if (code[idx] == '-' || (code[idx] >= '0' && code[idx] <= '9')) {
                    //a number
                    char *rptr;
                    double number = StringUtils::to_double(&code[idx], &rptr);
                    idx += (rptr - &code[idx]);
                    value = number;
                    return TK_NUMBER;

                } else if ((code[idx] == '@' && code[idx + 1] != '"') || code[idx] == '_' || (code[idx] >= 'A' && code[idx] <= 'Z') || (code[idx] >= 'a' && code[idx] <= 'z') || code[idx] > 127) {
                    se_string id;

                    id += code[idx];
                    idx++;

                    while (code[idx] == '_' || (code[idx] >= 'A' && code[idx] <= 'Z') || (code[idx] >= 'a' && code[idx] <= 'z') || (code[idx] >= '0' && code[idx] <= '9') || code[idx] > 127) {
                        id += code[idx];
                        idx++;
                    }

                    value = id;
                    return TK_IDENTIFIER;
                } else if (code[idx] == '@' && code[idx + 1] == '"') {
                    // begin of verbatim string
                    idx++;
                } else {
                    error_str = "Unexpected character.";
                    error = true;
                    return TK_ERROR;
                }
            }
        }
    }
}

Error ScriptClassParser::_skip_generic_type_params() {

    Token tk;
    se_string value_str = value.as<se_string>();
    while (true) {
        tk = get_token();

        if (tk == TK_IDENTIFIER) {
            tk = get_token();
            // Type specifications can end with "?" to denote nullable types, such as IList<int?>
            if (tk == TK_SYMBOL) {
                tk = get_token();
                if (value_str != "?") {
                    error_str = "Expected " + get_token_name(TK_IDENTIFIER) + ", found unexpected symbol '" + value_str + "'";
                    error = true;
                    return ERR_PARSE_ERROR;
                }
                if (tk != TK_OP_GREATER && tk != TK_COMMA) {
                    error_str = "Nullable type symbol '?' is only allowed after an identifier, but found " + get_token_name(tk) + " next.";
                    error = true;
                    return ERR_PARSE_ERROR;
                }
            }

            if (tk == TK_PERIOD) {
                while (true) {
                    tk = get_token();

                    if (tk != TK_IDENTIFIER) {
                        error_str = "Expected " + get_token_name(TK_IDENTIFIER) + ", found: " + get_token_name(tk);
                        error = true;
                        return ERR_PARSE_ERROR;
                    }

                    tk = get_token();

                    if (tk != TK_PERIOD)
                        break;
                }
            }

            if (tk == TK_OP_LESS) {
                Error err = _skip_generic_type_params();
                if (err)
                    return err;
                continue;
            } else if (tk == TK_OP_GREATER) {
                return OK;
            } else if (tk != TK_COMMA) {
                error_str = "Unexpected token: " + get_token_name(tk);
                error = true;
                return ERR_PARSE_ERROR;
            }
        } else if (tk == TK_OP_LESS) {
            error_str = "Expected " + get_token_name(TK_IDENTIFIER) + ", found " + get_token_name(TK_OP_LESS);
            error = true;
            return ERR_PARSE_ERROR;
        } else if (tk == TK_OP_GREATER) {
            return OK;
        } else {
            error_str = "Unexpected token: " + get_token_name(tk);
            error = true;
            return ERR_PARSE_ERROR;
        }
    }
}

Error ScriptClassParser::_parse_type_full_name(se_string &r_full_name) {

    Token tk = get_token();

    if (tk != TK_IDENTIFIER) {
        error_str = "Expected " + get_token_name(TK_IDENTIFIER) + ", found: " + get_token_name(tk);
        error = true;
        return ERR_PARSE_ERROR;
    }

    r_full_name += se_string(value);

    if (code[idx] == '<') {
        idx++;

        // We don't mind if the base is generic, but we skip it any ways since this information is not needed
        Error err = _skip_generic_type_params();
        if (err)
            return err;
    }

    if (code[idx] != '.') // We only want to take the next token if it's a period
        return OK;

    tk = get_token();

    CRASH_COND(tk != TK_PERIOD); // Assertion

    r_full_name += ".";

    return _parse_type_full_name(r_full_name);
}

Error ScriptClassParser::_parse_class_base(PODVector<se_string> &r_base) {

    se_string name;

    Error err = _parse_type_full_name(name);
    if (err)
        return err;

    Token tk = get_token();

    if (tk == TK_COMMA) {
        err = _parse_class_base(r_base);
        if (err)
            return err;
    } else if (tk == TK_IDENTIFIER && se_string(value) == "where") {
        err = _parse_type_constraints();
        if (err) {
            return err;
        }

        // An open curly bracket was parsed by _parse_type_constraints, so we can exit
    } else if (tk == TK_CURLY_BRACKET_OPEN) {
        // we are finished when we hit the open curly bracket
    } else {
        error_str = "Unexpected token: " + get_token_name(tk);
        error = true;
        return ERR_PARSE_ERROR;
    }

    r_base.push_back(name);

    return OK;
}

Error ScriptClassParser::_parse_type_constraints() {
    Token tk = get_token();
    if (tk != TK_IDENTIFIER) {
        error_str = "Unexpected token: " + get_token_name(tk);
        error = true;
        return ERR_PARSE_ERROR;
    }

    tk = get_token();
    if (tk != TK_COLON) {
        error_str = "Unexpected token: " + get_token_name(tk);
        error = true;
        return ERR_PARSE_ERROR;
    }

    while (true) {
        tk = get_token();
        if (tk == TK_IDENTIFIER) {
            if (se_string(value) == "where") {
                return _parse_type_constraints();
            }

            tk = get_token();
            if (tk == TK_PERIOD) {
                while (true) {
                    tk = get_token();

                    if (tk != TK_IDENTIFIER) {
                        error_str = "Expected " + get_token_name(TK_IDENTIFIER) + ", found: " + get_token_name(tk);
                        error = true;
                        return ERR_PARSE_ERROR;
                    }

                    tk = get_token();

                    if (tk != TK_PERIOD)
                        break;
                }
            }
        }

        if (tk == TK_COMMA) {
            continue;
        } else if (tk == TK_IDENTIFIER && se_string(value) == "where") {
            return _parse_type_constraints();
        } else if (tk == TK_SYMBOL && se_string(value) == "(") {
            tk = get_token();
            if (tk != TK_SYMBOL || se_string(value) != ")") {
                error_str = "Unexpected token: " + get_token_name(tk);
                error = true;
                return ERR_PARSE_ERROR;
            }
        } else if (tk == TK_OP_LESS) {
            Error err = _skip_generic_type_params();
            if (err)
                return err;
        } else if (tk == TK_CURLY_BRACKET_OPEN) {
            return OK;
        } else {
            error_str = "Unexpected token: " + get_token_name(tk);
            error = true;
            return ERR_PARSE_ERROR;
        }
    }
}

Error ScriptClassParser::_parse_namespace_name(se_string &r_name, int &r_curly_stack) {

    Token tk = get_token();

    if (tk == TK_IDENTIFIER) {
        r_name += se_string(value);
    } else {
        error_str = "Unexpected token: " + get_token_name(tk);
        error = true;
        return ERR_PARSE_ERROR;
    }

    tk = get_token();

    if (tk == TK_PERIOD) {
        r_name += ".";
        return _parse_namespace_name(r_name, r_curly_stack);
    } else if (tk == TK_CURLY_BRACKET_OPEN) {
        r_curly_stack++;
        return OK;
    } else {
        error_str = "Unexpected token: " + get_token_name(tk);
        error = true;
        return ERR_PARSE_ERROR;
    }
}

Error ScriptClassParser::parse(const se_string &p_code) {

    code = p_code;
    idx = 0;
    line = 0;
    error_str.clear();
    error = false;
    value = Variant();
    classes.clear();

    Token tk = get_token();

    Map<int, NameDecl> name_stack;
    int curly_stack = 0;
    int type_curly_stack = 0;

    while (!error && tk != TK_EOF) {
        se_string identifier = value;
        if (tk == TK_IDENTIFIER && (identifier == "class" || identifier == "struct")) {
            bool is_class = identifier == "class";

            tk = get_token();

            if (tk == TK_IDENTIFIER) {
                se_string name = value;
                int at_level = curly_stack;

                ClassDecl class_decl;

                for (auto &E : name_stack) {
                    const NameDecl &name_decl = E.second;

                    if (name_decl.type == NameDecl::NAMESPACE_DECL) {
                        if (&E != &(*name_stack.begin()))
                            class_decl.namespace_ += ".";
                        class_decl.namespace_ += name_decl.name;
                    } else {
                        class_decl.name += name_decl.name + ".";
                    }
                }

                class_decl.name += name;
                class_decl.nested = type_curly_stack > 0;

                bool generic = false;

                while (true) {
                    tk = get_token();

                    if (tk == TK_COLON) {
                        Error err = _parse_class_base(class_decl.base);
                        if (err)
                            return err;

                        curly_stack++;
                        type_curly_stack++;

                        break;
                    } else if (tk == TK_CURLY_BRACKET_OPEN) {
                        curly_stack++;
                        type_curly_stack++;
                        break;
                    } else if (tk == TK_OP_LESS && !generic) {
                        generic = true;

                        Error err = _skip_generic_type_params();
                        if (err)
                            return err;
                    } else if (tk == TK_IDENTIFIER && String(value) == "where") {
                        Error err = _parse_type_constraints();
                        if (err) {
                            return err;
                        }

                        // An open curly bracket was parsed by _parse_type_constraints, so we can exit
                        curly_stack++;
                        type_curly_stack++;
                        break;
                    } else {
                        error_str = "Unexpected token: " + get_token_name(tk);
                        error = true;
                        return ERR_PARSE_ERROR;
                    }
                }

                NameDecl name_decl;
                name_decl.name = name;
                name_decl.type = is_class ? NameDecl::CLASS_DECL : NameDecl::STRUCT_DECL;
                name_stack[at_level] = name_decl;

                if (is_class) {
                    if (!generic) { // no generics, thanks
                        classes.push_back(class_decl);
                    } else if (OS::get_singleton()->is_stdout_verbose()) {
                        se_string full_name = class_decl.namespace_;
                        if (full_name.length())
                            full_name += ".";
                        full_name += class_decl.name;
                        OS::get_singleton()->print(FormatVE("Ignoring generic class declaration: %s\n", full_name.c_str()));
                    }
                }
            }
        } else if (tk == TK_IDENTIFIER && identifier == "namespace") {
            if (type_curly_stack > 0) {
                error_str = "Found namespace nested inside type.";
                error = true;
                return ERR_PARSE_ERROR;
            }

            se_string name;
            int at_level = curly_stack;

            Error err = _parse_namespace_name(name, curly_stack);
            if (err)
                return err;

            NameDecl name_decl;
            name_decl.name = name;
            name_decl.type = NameDecl::NAMESPACE_DECL;
            name_stack[at_level] = name_decl;
        } else if (tk == TK_CURLY_BRACKET_OPEN) {
            curly_stack++;
        } else if (tk == TK_CURLY_BRACKET_CLOSE) {
            curly_stack--;
            if (name_stack.contains(curly_stack)) {
                if (name_stack[curly_stack].type != NameDecl::NAMESPACE_DECL)
                    type_curly_stack--;
                name_stack.erase(curly_stack);
            }
        }

        tk = get_token();
    }

    if (!error && tk == TK_EOF && curly_stack > 0) {
        error_str = "Reached EOF with missing close curly brackets.";
        error = true;
    }

    if (error)
        return ERR_PARSE_ERROR;

    return OK;
}

Error ScriptClassParser::parse_file(se_string_view p_filepath) {

    se_string source;

    Error ferr = read_all_file_utf8(p_filepath, source);

    ERR_FAIL_COND_V_MSG(ferr != OK, ferr,
            ferr == ERR_INVALID_DATA ?
                    "File '" + p_filepath + "' contains invalid unicode (UTF-8), so it was not loaded."
                                            " Please ensure that scripts are saved in valid UTF-8 unicode." :
                    "Failed to read file: '" + p_filepath + "'.")

    return parse(source);
}



Vector<ScriptClassParser::ClassDecl> ScriptClassParser::get_classes() {
    return classes;
}
