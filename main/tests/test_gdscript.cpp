/*************************************************************************/
/*  test_gdscript.cpp                                                    */
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

#include "test_gdscript.h"

#include "core/os/file_access.h"
#include "core/os/main_loop.h"
#include "core/string_formatter.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/print_string.h"
#include "core/se_string.h"
#include "core/string_utils.inl"

#ifdef GDSCRIPT_ENABLED

#include "modules/gdscript/gdscript.h"
#include "modules/gdscript/gdscript_compiler.h"
#include "modules/gdscript/gdscript_parser.h"
#include "modules/gdscript/gdscript_tokenizer.h"

namespace TestGDScript {

static void _print_indent(int p_ident, const se_string &p_text) {

    se_string txt;
    for (int i = 0; i < p_ident; i++) {
        txt += '\t';
    }

    print_line(txt + p_text);
}

static se_string _parser_extends(const GDScriptParser::ClassNode *p_class) {

    se_string txt("extends ");
    if (!p_class->extends_file.empty()) {
        txt += "\"" + se_string(p_class->extends_file) + "\"";
        if (!p_class->extends_class.empty())
            txt += se_string(".");
    }

    for (int i = 0; i < p_class->extends_class.size(); i++) {

        if (i != 0)
            txt += '.';

        txt += p_class->extends_class[i];
    }

    return txt;
}

static se_string _parser_expr(const GDScriptParser::Node *p_expr) {

    se_string txt;
    switch (p_expr->type) {

        case GDScriptParser::Node::TYPE_IDENTIFIER: {

            const GDScriptParser::IdentifierNode *id_node = static_cast<const GDScriptParser::IdentifierNode *>(p_expr);
            txt = id_node->name;
        } break;
        case GDScriptParser::Node::TYPE_CONSTANT: {
            const GDScriptParser::ConstantNode *c_node = static_cast<const GDScriptParser::ConstantNode *>(p_expr);
            if (c_node->value.get_type() == VariantType::STRING)
                txt = "\"" + se_string(c_node->value) + "\"";
            else
                txt = c_node->value.as<se_string>();

        } break;
        case GDScriptParser::Node::TYPE_SELF: {
            txt = "self";
        } break;
        case GDScriptParser::Node::TYPE_ARRAY: {
            const GDScriptParser::ArrayNode *arr_node = static_cast<const GDScriptParser::ArrayNode *>(p_expr);
            txt += "[";
            for (int i = 0; i < arr_node->elements.size(); i++) {

                if (i > 0)
                    txt += (", ");
                txt += _parser_expr(arr_node->elements[i]);
            }
            txt += ']';
        } break;
        case GDScriptParser::Node::TYPE_DICTIONARY: {
            const GDScriptParser::DictionaryNode *dict_node = static_cast<const GDScriptParser::DictionaryNode *>(p_expr);
            txt += '{';
            for (int i = 0; i < dict_node->elements.size(); i++) {

                if (i > 0)
                    txt += (", ");

                const GDScriptParser::DictionaryNode::Pair &p = dict_node->elements[i];
                txt += _parser_expr(p.key);
                txt += ':';
                txt += _parser_expr(p.value);
            }
            txt += '}';
        } break;
        case GDScriptParser::Node::TYPE_OPERATOR: {

            const GDScriptParser::OperatorNode *c_node = static_cast<const GDScriptParser::OperatorNode *>(p_expr);
            switch (c_node->op) {

                case GDScriptParser::OperatorNode::OP_PARENT_CALL:
                    txt += '.';
                    FALLTHROUGH;
                case GDScriptParser::OperatorNode::OP_CALL: {

                    ERR_FAIL_COND_V(c_node->arguments.empty(), se_string())
                    se_string func_name;
                    const GDScriptParser::Node *nfunc = c_node->arguments[0];
                    int arg_ofs = 0;
                    if (nfunc->type == GDScriptParser::Node::TYPE_BUILT_IN_FUNCTION) {

                        const GDScriptParser::BuiltInFunctionNode *bif_node = static_cast<const GDScriptParser::BuiltInFunctionNode *>(nfunc);
                        func_name = GDScriptFunctions::get_func_name(bif_node->function);
                        arg_ofs = 1;
                    } else if (nfunc->type == GDScriptParser::Node::TYPE_TYPE) {

                        const GDScriptParser::TypeNode *t_node = static_cast<const GDScriptParser::TypeNode *>(nfunc);
                        func_name = Variant::get_type_name(t_node->vtype);
                        arg_ofs = 1;
                    } else {

                        ERR_FAIL_COND_V(c_node->arguments.size() < 2, se_string())
                        nfunc = c_node->arguments[1];
                        ERR_FAIL_COND_V(nfunc->type != GDScriptParser::Node::TYPE_IDENTIFIER, se_string())

                        if (c_node->arguments[0]->type != GDScriptParser::Node::TYPE_SELF)
                            func_name = _parser_expr(c_node->arguments[0]) + ".";

                        func_name += _parser_expr(nfunc);
                        arg_ofs = 2;
                    }

                    txt += func_name + "(";

                    for (int i = arg_ofs; i < c_node->arguments.size(); i++) {

                        const GDScriptParser::Node *arg = c_node->arguments[i];
                        if (i > arg_ofs)
                            txt += ", ";
                        txt += _parser_expr(arg);
                    }

                    txt += ')';

                } break;
                case GDScriptParser::OperatorNode::OP_INDEX: {

                    ERR_FAIL_COND_V(c_node->arguments.size() != 2, se_string())

                    //index with []
                    txt = _parser_expr(c_node->arguments[0]) + "[" + _parser_expr(c_node->arguments[1]) + "]";

                } break;
                case GDScriptParser::OperatorNode::OP_INDEX_NAMED: {

                    ERR_FAIL_COND_V(c_node->arguments.size() != 2, se_string())

                    txt = _parser_expr(c_node->arguments[0]) + "." + _parser_expr(c_node->arguments[1]);

                } break;
                case GDScriptParser::OperatorNode::OP_NEG: {
                    txt = "-" + _parser_expr(c_node->arguments[0]);
                } break;
                case GDScriptParser::OperatorNode::OP_NOT: {
                    txt = "not " + _parser_expr(c_node->arguments[0]);
                } break;
                case GDScriptParser::OperatorNode::OP_BIT_INVERT: {
                    txt = "~" + _parser_expr(c_node->arguments[0]);
                } break;
                case GDScriptParser::OperatorNode::OP_IN: {
                    txt = _parser_expr(c_node->arguments[0]) + " in " + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_EQUAL: {
                    txt = _parser_expr(c_node->arguments[0]) + "==" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_NOT_EQUAL: {
                    txt = _parser_expr(c_node->arguments[0]) + "!=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_LESS: {
                    txt = _parser_expr(c_node->arguments[0]) + "<" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_LESS_EQUAL: {
                    txt = _parser_expr(c_node->arguments[0]) + "<=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_GREATER: {
                    txt = _parser_expr(c_node->arguments[0]) + ">" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_GREATER_EQUAL: {
                    txt = _parser_expr(c_node->arguments[0]) + ">=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_AND: {
                    txt = _parser_expr(c_node->arguments[0]) + " and " + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_OR: {
                    txt = _parser_expr(c_node->arguments[0]) + " or " + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ADD: {
                    txt = _parser_expr(c_node->arguments[0]) + "+" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_SUB: {
                    txt = _parser_expr(c_node->arguments[0]) + "-" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_MUL: {
                    txt = _parser_expr(c_node->arguments[0]) + "*" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_DIV: {
                    txt = _parser_expr(c_node->arguments[0]) + "/" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_MOD: {
                    txt = _parser_expr(c_node->arguments[0]) + "%" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_SHIFT_LEFT: {
                    txt = _parser_expr(c_node->arguments[0]) + "<<" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_SHIFT_RIGHT: {
                    txt = _parser_expr(c_node->arguments[0]) + ">>" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN: {
                    txt = _parser_expr(c_node->arguments[0]) + "=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN_ADD: {
                    txt = _parser_expr(c_node->arguments[0]) + "+=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN_SUB: {
                    txt = _parser_expr(c_node->arguments[0]) + "-=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN_MUL: {
                    txt = _parser_expr(c_node->arguments[0]) + "*=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN_DIV: {
                    txt = _parser_expr(c_node->arguments[0]) + "/=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN_MOD: {
                    txt = _parser_expr(c_node->arguments[0]) + "%=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN_SHIFT_LEFT: {
                    txt = _parser_expr(c_node->arguments[0]) + "<<=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN_SHIFT_RIGHT: {
                    txt = _parser_expr(c_node->arguments[0]) + ">>=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN_BIT_AND: {
                    txt = _parser_expr(c_node->arguments[0]) + "&=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN_BIT_OR: {
                    txt = _parser_expr(c_node->arguments[0]) + "|=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_ASSIGN_BIT_XOR: {
                    txt = _parser_expr(c_node->arguments[0]) + "^=" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_BIT_AND: {
                    txt = _parser_expr(c_node->arguments[0]) + "&" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_BIT_OR: {
                    txt = _parser_expr(c_node->arguments[0]) + "|" + _parser_expr(c_node->arguments[1]);
                } break;
                case GDScriptParser::OperatorNode::OP_BIT_XOR: {
                    txt = _parser_expr(c_node->arguments[0]) + "^" + _parser_expr(c_node->arguments[1]);
                } break;
                default: {
                }
            }

        } break;
        case GDScriptParser::Node::TYPE_CAST: {
            const GDScriptParser::CastNode *cast_node = static_cast<const GDScriptParser::CastNode *>(p_expr);
            txt = _parser_expr(cast_node->source_node) + " as " + cast_node->cast_type.to_string();

        } break;
        case GDScriptParser::Node::TYPE_NEWLINE: {

            //skippie
        } break;
        default: {

            ERR_FAIL_V_MSG(se_string(), "Parser bug at " + itos(p_expr->line) + ", invalid expression type: " + itos(p_expr->type))
        }
    }

    return txt;
}

static void _parser_show_block(const GDScriptParser::BlockNode *p_block, int p_indent) {

    for (size_t i = 0; i < p_block->statements.size(); i++) {

        const GDScriptParser::Node *statement = p_block->statements[i];

        switch (statement->type) {

            case GDScriptParser::Node::TYPE_CONTROL_FLOW: {

                const GDScriptParser::ControlFlowNode *cf_node = static_cast<const GDScriptParser::ControlFlowNode *>(statement);
                switch (cf_node->cf_type) {

                    case GDScriptParser::ControlFlowNode::CF_IF: {

                        ERR_FAIL_COND(cf_node->arguments.size() != 1)
                        se_string txt = FormatVE("if %s:",_parser_expr(cf_node->arguments[0]).c_str());
                        _print_indent(p_indent, txt);
                        ERR_FAIL_COND(!cf_node->body)
                        _parser_show_block(cf_node->body, p_indent + 1);
                        if (cf_node->body_else) {
                            _print_indent(p_indent, "else:");
                            _parser_show_block(cf_node->body_else, p_indent + 1);
                        }

                    } break;
                    case GDScriptParser::ControlFlowNode::CF_FOR: {
                        ERR_FAIL_COND(cf_node->arguments.size() != 2)
                        se_string txt;
                        txt = FormatVE("for %s in %s:",_parser_expr(cf_node->arguments[0]).c_str(),_parser_expr(cf_node->arguments[1]).c_str());
                        _print_indent(p_indent, txt);
                        ERR_FAIL_COND(!cf_node->body)
                        _parser_show_block(cf_node->body, p_indent + 1);

                    } break;
                    case GDScriptParser::ControlFlowNode::CF_WHILE: {

                        ERR_FAIL_COND(cf_node->arguments.size() != 1)
                        se_string txt;
                        txt += "while " + _parser_expr(cf_node->arguments[0]) + ':';
                        _print_indent(p_indent, txt);
                        ERR_FAIL_COND(!cf_node->body)
                        _parser_show_block(cf_node->body, p_indent + 1);

                    } break;
                    case GDScriptParser::ControlFlowNode::CF_MATCH: {
                        // FIXME: Implement
                    } break;
                    case GDScriptParser::ControlFlowNode::CF_CONTINUE: {

                        _print_indent(p_indent, "continue");
                    } break;
                    case GDScriptParser::ControlFlowNode::CF_BREAK: {

                        _print_indent(p_indent, ("break"));
                    } break;
                    case GDScriptParser::ControlFlowNode::CF_RETURN: {

                        if (!cf_node->arguments.empty())
                            _print_indent(p_indent, "return " + _parser_expr(cf_node->arguments[0]));
                        else
                            _print_indent(p_indent, ("return "));
                    } break;
                }

            } break;
            case GDScriptParser::Node::TYPE_LOCAL_VAR: {

                const GDScriptParser::LocalVarNode *lv_node = static_cast<const GDScriptParser::LocalVarNode *>(statement);
                _print_indent(p_indent, "var " + se_string(lv_node->name));
            } break;
            default: {
                //expression i guess
                _print_indent(p_indent, _parser_expr(statement));
            }
        }
    }
}

static void _parser_show_function(const GDScriptParser::FunctionNode *p_func, int p_indent, GDScriptParser::BlockNode *p_initializer = nullptr) {

    se_string txt;
    if (p_func->_static)
        txt = "static ";
    txt += "func ";
    if (p_func->name.empty()) // initializer
        txt += "[built-in-initializer]";
    else
        txt += p_func->name.asCString();
    txt += "(";

    for (int i = 0; i < p_func->arguments.size(); i++) {

        if (i != 0)
            txt += ", ";
        txt.append("var ");
        txt.append(p_func->arguments[i].asCString());
        if (i >= (p_func->arguments.size() - p_func->default_values.size())) {
            int defarg = i - (p_func->arguments.size() - p_func->default_values.size());
            txt += "=";
            txt +=  _parser_expr(p_func->default_values[defarg]);
        }
    }

    txt += ")";

    //todo constructor check!

    txt += ":";

    _print_indent(p_indent, txt);
    if (p_initializer)
        _parser_show_block(p_initializer, p_indent + 1);
    _parser_show_block(p_func->body, p_indent + 1);
}

static void _parser_show_class(const GDScriptParser::ClassNode *p_class, int p_indent, const Vector<se_string_view> &p_code) {

    if (p_indent == 0 && (!p_class->extends_file.empty() || !p_class->extends_class.empty())) {

        _print_indent(p_indent, _parser_extends(p_class));
        print_line("\n");
    }

    for (int i = 0; i < p_class->subclasses.size(); i++) {

        const GDScriptParser::ClassNode *subclass = p_class->subclasses[i];
        se_string line = se_string("class ") + subclass->name;
        if (!subclass->extends_file.empty() || !subclass->extends_class.empty())
            line += " " + _parser_extends(subclass);
        line += ':';
        _print_indent(p_indent, line);
        _parser_show_class(subclass, p_indent + 1, p_code);
        print_line("\n");
    }

    for (auto &E : p_class->constant_expressions) {
        const GDScriptParser::ClassNode::Constant &constant = E.second;
        _print_indent(p_indent, "const " + se_string(E.first) + "=" + _parser_expr(constant.expression));
    }

    for (int i = 0; i < p_class->variables.size(); i++) {

        const GDScriptParser::ClassNode::Member &m = p_class->variables[i];

        _print_indent(p_indent, "var " + se_string(m.identifier));
    }

    print_line("\n");

    for (int i = 0; i < p_class->static_functions.size(); i++) {

        _parser_show_function(p_class->static_functions[i], p_indent);
        print_line("\n");
    }

    for (int i = 0; i < p_class->functions.size(); i++) {

        if (String(p_class->functions[i]->name) == "_init") {
            _parser_show_function(p_class->functions[i], p_indent, p_class->initializer);
        } else
            _parser_show_function(p_class->functions[i], p_indent);
        print_line("\n");
    }
    //_parser_show_function(p_class->initializer,p_indent);
    print_line("\n");
}

static se_string _disassemble_addr(const Ref<GDScript> &p_script, const GDScriptFunction &func, int p_addr) {

    int addr = p_addr & GDScriptFunction::ADDR_MASK;

    switch (p_addr >> GDScriptFunction::ADDR_BITS) {

        case GDScriptFunction::ADDR_TYPE_SELF: {
            return "self";
        }
        case GDScriptFunction::ADDR_TYPE_CLASS: {
            return "class";
        }
        case GDScriptFunction::ADDR_TYPE_MEMBER: {

            return se_string("member(") + p_script->debug_get_member_by_index(addr).asCString() + ")";
        }
        case GDScriptFunction::ADDR_TYPE_CLASS_CONSTANT: {

            return se_string("class_const(") + func.get_global_name(addr).asCString() + ")";
        }
        case GDScriptFunction::ADDR_TYPE_LOCAL_CONSTANT: {

            Variant v = func.get_constant(addr);

            se_string txt(v);
            if (v.get_type() == VariantType::STRING || v.get_type() == VariantType::NODE_PATH)
                txt = "\"" + txt + "\"";
            return se_string("const(") + txt + ")";
        }
        case GDScriptFunction::ADDR_TYPE_STACK: {

            return "stack(" + to_string(addr) + ")";
        }
        case GDScriptFunction::ADDR_TYPE_STACK_VARIABLE: {

            return "var_stack(" + to_string(addr) + ")";
        }
        case GDScriptFunction::ADDR_TYPE_GLOBAL: {

            return se_string("global(") + func.get_global_name(addr).asCString() + ")";
        }
        case GDScriptFunction::ADDR_TYPE_NIL: {
            return "nil";
        }
    }

    return "<err>";
}

static void _disassemble_class(const Ref<GDScript> &p_class, const Vector<se_string_view> &p_code) {

    const Map<StringName, GDScriptFunction *> &mf = p_class->debug_get_member_functions();

    for (const auto &E : mf) {

        const GDScriptFunction &func = *E.second;
        const int *code = func.get_code();
        int codelen = func.get_code_size();
        se_string defargs;
        if (func.get_default_argument_count()) {
            defargs = "defarg at: ";
            for (int i = 0; i < func.get_default_argument_count(); i++) {

                if (i > 0)
                    defargs += ',';
                defargs += itos(func.get_default_argument_addr(i));
            }
            defargs += ' ';
        }
        print_line("== function " + se_string(func.get_name()) + "() :: stack size: " + itos(func.get_max_stack_size()) + " " + defargs + "==");

#define DADDR(m_ip) (_disassemble_addr(p_class, func, code[ip + m_ip]))

        for (int ip = 0; ip < codelen;) {

            int incr = 0;
            se_string txt = to_string(ip) + " ";

            switch (code[ip]) {

                case GDScriptFunction::OPCODE_OPERATOR: {

                    int op = code[ip + 1];
                    txt += " op ";

                    se_string opname = Variant::get_operator_name(Variant::Operator(op));

                    txt += DADDR(4);
                    txt += " = ";
                    txt += DADDR(2);
                    txt += " " + opname + " ";
                    txt += DADDR(3);
                    incr += 5;

                } break;
                case GDScriptFunction::OPCODE_SET: {

                    txt += "set ";
                    txt += DADDR(1);
                    txt += "[";
                    txt += DADDR(2);
                    txt += "]=";
                    txt += DADDR(3);
                    incr += 4;

                } break;
                case GDScriptFunction::OPCODE_GET: {

                    txt += " get ";
                    txt += DADDR(3);
                    txt += "=";
                    txt += DADDR(1);
                    txt += "[";
                    txt += DADDR(2);
                    txt += "]";
                    incr += 4;

                } break;
                case GDScriptFunction::OPCODE_SET_NAMED: {

                    txt += " set_named ";
                    txt += DADDR(1);
                    txt += "[\"";
                    txt += func.get_global_name(code[ip + 2]).asCString();
                    txt += "\"]=";
                    txt += DADDR(3);
                    incr += 4;

                } break;
                case GDScriptFunction::OPCODE_GET_NAMED: {

                    txt += " get_named ";
                    txt += DADDR(3);
                    txt += "=";
                    txt += DADDR(1);
                    txt += "[\"";
                    txt += func.get_global_name(code[ip + 2]).asCString();
                    txt += "\"]";
                    incr += 4;

                } break;
                case GDScriptFunction::OPCODE_SET_MEMBER: {

                    txt += " set_member ";
                    txt += "[\"";
                    txt += func.get_global_name(code[ip + 1]).asCString();
                    txt += "\"]=";
                    txt += DADDR(2);
                    incr += 3;

                } break;
                case GDScriptFunction::OPCODE_GET_MEMBER: {

                    txt += " get_member ";
                    txt += DADDR(2);
                    txt += "=";
                    txt += "[\"";
                    txt += func.get_global_name(code[ip + 1]).asCString();
                    txt += "\"]";
                    incr += 3;

                } break;
                case GDScriptFunction::OPCODE_ASSIGN: {

                    txt += " assign ";
                    txt += DADDR(1);
                    txt += "=";
                    txt += DADDR(2);
                    incr += 3;

                } break;
                case GDScriptFunction::OPCODE_ASSIGN_TRUE: {

                    txt += " assign ";
                    txt += DADDR(1);
                    txt += "= true";
                    incr += 2;

                } break;
                case GDScriptFunction::OPCODE_ASSIGN_FALSE: {

                    txt += " assign ";
                    txt += DADDR(1);
                    txt += "= false";
                    incr += 2;

                } break;
                case GDScriptFunction::OPCODE_ASSIGN_TYPED_BUILTIN: {

                    txt += " assign typed builtin (";
                    txt += Variant::get_type_name((VariantType)code[ip + 1]);
                    txt += ") ";
                    txt += DADDR(2);
                    txt += " = ";
                    txt += DADDR(3);
                    incr += 4;

                } break;
                case GDScriptFunction::OPCODE_ASSIGN_TYPED_NATIVE: {
                    Variant className = func.get_constant(code[ip + 1]);
                    GDScriptNativeClass *nc = object_cast<GDScriptNativeClass>(className.as<Object *>());

                    txt += " assign typed native (";
                    txt += nc->get_name().asCString();
                    txt += ") ";
                    txt += DADDR(2);
                    txt += " = ";
                    txt += DADDR(3);
                    incr += 4;

                } break;
                case GDScriptFunction::OPCODE_CAST_TO_SCRIPT: {

                    txt += " cast ";
                    txt += DADDR(3);
                    txt += "=";
                    txt += DADDR(1);
                    txt += " as ";
                    txt += DADDR(2);
                    incr += 4;

                } break;
                case GDScriptFunction::OPCODE_CONSTRUCT: {

                    VariantType t = VariantType(code[ip + 1]);
                    int argc = code[ip + 2];

                    txt += " construct ";
                    txt += DADDR(3 + argc);
                    txt += FormatVE(" = %s(",Variant::get_type_name(t));
                    for (int i = 0; i < argc; i++) {

                        if (i > 0)
                            txt += ", ";
                        txt += DADDR(i + 3);
                    }
                    txt += ")";

                    incr = 4 + argc;

                } break;
                case GDScriptFunction::OPCODE_CONSTRUCT_ARRAY: {

                    int argc = code[ip + 1];
                    txt += " make_array ";
                    txt += DADDR(2 + argc);
                    txt += " = [ ";

                    for (int i = 0; i < argc; i++) {
                        if (i > 0)
                            txt += ", ";
                        txt += DADDR(2 + i);
                    }

                    txt += "]";

                    incr += 3 + argc;

                } break;
                case GDScriptFunction::OPCODE_CONSTRUCT_DICTIONARY: {

                    int argc = code[ip + 1];
                    txt += " make_dict ";
                    txt += DADDR(2 + argc * 2);
                    txt += " = { ";

                    for (int i = 0; i < argc; i++) {
                        if (i > 0)
                            txt += ", ";
                        txt += DADDR(2 + i * 2 + 0);
                        txt += ":";
                        txt += DADDR(2 + i * 2 + 1);
                    }

                    txt += "}";

                    incr += 3 + argc * 2;

                } break;

                case GDScriptFunction::OPCODE_CALL:
                case GDScriptFunction::OPCODE_CALL_RETURN: {

                    bool ret = code[ip] == GDScriptFunction::OPCODE_CALL_RETURN;

                    if (ret)
                        txt += " call-ret ";
                    else
                        txt += " call ";

                    int argc = code[ip + 1];
                    if (ret) {
                        txt += DADDR(4 + argc) + "=";
                    }

                    txt += DADDR(2) + ".";
                    txt += func.get_global_name(code[ip + 3]).asCString();
                    txt += "(";

                    for (int i = 0; i < argc; i++) {
                        if (i > 0)
                            txt += ", ";
                        txt += DADDR(4 + i);
                    }
                    txt += ")";

                    incr = 5 + argc;

                } break;
                case GDScriptFunction::OPCODE_CALL_BUILT_IN: {

                    txt += " call-built-in ";

                    int argc = code[ip + 2];
                    txt += DADDR(3 + argc) + "=";

                    txt += GDScriptFunctions::get_func_name(GDScriptFunctions::Function(code[ip + 1]));
                    txt += "(";

                    for (int i = 0; i < argc; i++) {
                        if (i > 0)
                            txt += ", ";
                        txt += DADDR(3 + i);
                    }
                    txt += ")";

                    incr = 4 + argc;

                } break;
                case GDScriptFunction::OPCODE_CALL_SELF_BASE: {

                    txt += " call-self-base ";

                    int argc = code[ip + 2];
                    txt += DADDR(3 + argc) + "=";

                    txt += func.get_global_name(code[ip + 1]).asCString();
                    txt += "(";

                    for (int i = 0; i < argc; i++) {
                        if (i > 0)
                            txt += ", ";
                        txt += DADDR(3 + i);
                    }
                    txt += ")";

                    incr = 4 + argc;

                } break;
                case GDScriptFunction::OPCODE_YIELD: {

                    txt += " yield ";
                    incr = 1;

                } break;
                case GDScriptFunction::OPCODE_YIELD_SIGNAL: {

                    txt += " yield_signal ";
                    txt += DADDR(1);
                    txt += ",";
                    txt += DADDR(2);
                    incr = 3;
                } break;
                case GDScriptFunction::OPCODE_YIELD_RESUME: {

                    txt += " yield resume: ";
                    txt += DADDR(1);
                    incr = 2;
                } break;
                case GDScriptFunction::OPCODE_JUMP: {

                    txt += FormatVE(" jump %d",code[ip + 1]);

                    incr = 2;

                } break;
                case GDScriptFunction::OPCODE_JUMP_IF: {

                    txt += " jump-if ";
                    txt += DADDR(1);
                    txt += FormatVE(" to %d",code[ip + 2]);

                    incr = 3;
                } break;
                case GDScriptFunction::OPCODE_JUMP_IF_NOT: {

                    txt += " jump-if-not ";
                    txt += DADDR(1);
                    txt += FormatVE(" to %d",code[ip + 2]);

                    incr = 3;
                } break;
                case GDScriptFunction::OPCODE_JUMP_TO_DEF_ARGUMENT: {

                    txt += " jump-to-default-argument ";
                    incr = 1;
                } break;
                case GDScriptFunction::OPCODE_RETURN: {

                    txt += " return ";
                    txt += DADDR(1);

                    incr = 2;

                } break;
                case GDScriptFunction::OPCODE_ITERATE_BEGIN: {

                    txt += " for-init " + DADDR(4) + " in " + DADDR(2) + " counter " + DADDR(1) + " end " + to_string(code[ip + 3]);
                    incr += 5;

                } break;
                case GDScriptFunction::OPCODE_ITERATE: {

                    txt += " for-loop " + DADDR(4) + " in " + DADDR(2) + " counter " + DADDR(1) + " end " + to_string(code[ip + 3]);
                    incr += 5;

                } break;
                case GDScriptFunction::OPCODE_LINE: {

                    int line = code[ip + 1] - 1;
                    if (line >= 0 && line < p_code.size())
                        txt = "\n" + to_string(line + 1) + ": " + p_code[line] + "\n";
                    else
                        txt = "";
                    incr += 2;
                } break;
                case GDScriptFunction::OPCODE_END: {

                    txt += " end";
                    incr += 1;
                } break;
                case GDScriptFunction::OPCODE_ASSERT: {

                    txt += " assert ";
                    txt += DADDR(1);
                    incr += 2;

                } break;
            }

            if (incr == 0) {

                ERR_BREAK_MSG(true, "Unhandled opcode: " + itos(code[ip]));
            }

            ip += incr;
            if (!txt.empty())
                print_line(txt.c_str());
        }
    }
}

MainLoop *test(TestType p_type) {

    const ListPOD<se_string> &cmdlargs(OS::get_singleton()->get_cmdline_args());

    if (cmdlargs.empty()) {
        return nullptr;
    }

    const se_string &test(cmdlargs.back());
    if (!StringUtils::ends_with(test,".gd") && !StringUtils::ends_with(test,".gdc")) {
        print_line("This test expects a path to a GDScript file as its last parameter. Got: " + test);
        return nullptr;
    }

    FileAccess *fa = FileAccess::open(test, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!fa, nullptr, "Could not open file: " + test)

    Vector<uint8_t> buf;
    int flen = fa->get_len();
    buf.resize(fa->get_len() + 1);
    fa->get_buffer(buf.ptrw(), flen);
    buf.write[flen] = 0;

    se_string_view code((const char *)&buf[0]);

    Vector<se_string_view> lines;
    int last = 0;

    for (size_t i = 0; i <= code.length(); i++) {

        if (code[i] == '\n' || code[i] == 0) {

            lines.push_back(StringUtils::substr(code,last, i - last));
            last = i + 1;
        }
    }

    if (p_type == TEST_TOKENIZER) {

        GDScriptTokenizerText tk;
        tk.set_code(code);
        int line = -1;
        while (tk.get_token() != GDScriptTokenizer::TK_EOF) {

            se_string text;
            if (tk.get_token() == GDScriptTokenizer::TK_IDENTIFIER)
                text = se_string("'") + tk.get_token_identifier() + "' (identifier)";
            else if (tk.get_token() == GDScriptTokenizer::TK_CONSTANT) {
                const Variant &c = tk.get_token_constant();
                if (c.get_type() == VariantType::STRING)
                    text = "\"" + se_string(c) + "\"";
                else
                    text = se_string(c);

                text = text + " (" + Variant::get_type_name(c.get_type()) + " constant)";
            } else if (tk.get_token() == GDScriptTokenizer::TK_ERROR)
                text = "ERROR: " + tk.get_token_error();
            else if (tk.get_token() == GDScriptTokenizer::TK_NEWLINE)
                text = FormatVE("newline (%d) + indent: %d",tk.get_token_line(),tk.get_token_line_indent());
            else if (tk.get_token() == GDScriptTokenizer::TK_BUILT_IN_FUNC)
                text = "'" + se_string(GDScriptFunctions::get_func_name(tk.get_token_built_in_func())) + "' (built-in function)";
            else
                text = tk.get_token_name(tk.get_token());

            if (tk.get_token_line() != line) {
                int from = line + 1;
                line = tk.get_token_line();

                for (int i = from; i <= line; i++) {
                    int l = i - 1;
                    if (l >= 0 && l < lines.size()) {
                        print_line("\n" + ::to_string(i) + ": " + lines[l] + "\n");
                    }
                }
            }
            print_line("\t(" + ::to_string(tk.get_token_column()) + "): " + text);
            tk.advance();
        }
    }

    if (p_type == TEST_PARSER) {

        GDScriptParser parser;
        Error err = parser.parse(code);
        if (err) {
            print_line(FormatVE("Parse Error:\n%d:%d:%d:%s",parser.get_error_line(),parser.get_error_column(),parser.get_error().c_str()));
            memdelete(fa);
            return nullptr;
        }

        const GDScriptParser::Node *root = parser.get_parse_tree();
        ERR_FAIL_COND_V(root->type != GDScriptParser::Node::TYPE_CLASS, nullptr)
        const GDScriptParser::ClassNode *cnode = static_cast<const GDScriptParser::ClassNode *>(root);

        _parser_show_class(cnode, 0, lines);
    }

    if (p_type == TEST_COMPILER) {

        GDScriptParser parser;

        Error err = parser.parse(code);
        if (err) {
            print_line(FormatVE("Parse Error:\n%d:%d:%d:%s",parser.get_error_line(),parser.get_error_column(),parser.get_error().c_str()));
            memdelete(fa);
            return nullptr;
        }

        Ref<GDScript> gds(make_ref_counted<GDScript>());

        GDScriptCompiler gdc;
        err = gdc.compile(&parser, gds.get());
        if (err) {

            print_line(FormatVE("Compile Error:\n%d:%d:%d:%s",gdc.get_error_line(),gdc.get_error_column(),gdc.get_error().c_str()));
            return nullptr;
        }

        Ref<GDScript> current = gds;

        while (current) {

            print_line("** CLASS **");
            _disassemble_class(current, lines);

            current = current->get_base();
        }

    } else if (p_type == TEST_BYTECODE) {

        PODVector<uint8_t> buf2 = GDScriptTokenizerBuffer::parse_code_string(code);
        se_string dst = se_string(PathUtils::get_basename(test)) + ".gdc";
        FileAccess *fw = FileAccess::open(dst, FileAccess::WRITE);
        fw->store_buffer(buf2.data(), buf2.size());
        memdelete(fw);
    }

    memdelete(fa);

    return nullptr;
}
} // namespace TestGDScript

#else

namespace TestGDScript {

MainLoop *test(TestType p_type) {

    return NULL;
}
} // namespace TestGDScript

#endif
