/*************************************************************************/
/*  visual_script_expression.h                                           */
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

#include "visual_script.h"
#include "visual_script_builtin_funcs.h"

class VisualScriptExpression : public VisualScriptNode {

    GDCLASS(VisualScriptExpression,VisualScriptNode)
    friend class VisualScriptNodeInstanceExpression;

    struct Input {

        VariantType type;
        String name;

        Input() { type = VariantType::NIL; }
    };

    PODVector<Input> inputs;
    VariantType output_type;

    String expression;

    bool sequenced;
    size_t str_ofs;
    bool expression_dirty;

    bool _compile_expression();

    enum TokenType {
        TK_CURLY_BRACKET_OPEN,
        TK_CURLY_BRACKET_CLOSE,
        TK_BRACKET_OPEN,
        TK_BRACKET_CLOSE,
        TK_PARENTHESIS_OPEN,
        TK_PARENTHESIS_CLOSE,
        TK_IDENTIFIER,
        TK_BUILTIN_FUNC,
        TK_SELF,
        TK_CONSTANT,
        TK_BASIC_TYPE,
        TK_COLON,
        TK_COMMA,
        TK_PERIOD,
        TK_OP_IN,
        TK_OP_EQUAL,
        TK_OP_NOT_EQUAL,
        TK_OP_LESS,
        TK_OP_LESS_EQUAL,
        TK_OP_GREATER,
        TK_OP_GREATER_EQUAL,
        TK_OP_AND,
        TK_OP_OR,
        TK_OP_NOT,
        TK_OP_ADD,
        TK_OP_SUB,
        TK_OP_MUL,
        TK_OP_DIV,
        TK_OP_MOD,
        TK_OP_SHIFT_LEFT,
        TK_OP_SHIFT_RIGHT,
        TK_OP_BIT_AND,
        TK_OP_BIT_OR,
        TK_OP_BIT_XOR,
        TK_OP_BIT_INVERT,
        TK_EOF,
        TK_ERROR,
        TK_MAX
    };

    static const char *token_name[TK_MAX];
    struct Token {

        TokenType type;
        Variant value;
    };

    void _set_error(const char *p_err) {
        if (error_set)
            return;
        error_str = p_err;
        error_set = true;
    }

    Error _get_token(Token &r_token);

    String error_str;
    bool error_set;

    struct ENode {

        enum Type {
            TYPE_INPUT,
            TYPE_CONSTANT,
            TYPE_SELF,
            TYPE_OPERATOR,
            TYPE_INDEX,
            TYPE_NAMED_INDEX,
            TYPE_ARRAY,
            TYPE_DICTIONARY,
            TYPE_CONSTRUCTOR,
            TYPE_BUILTIN_FUNC,
            TYPE_CALL
        };

        ENode *next;

        Type type;

        ENode() { next = nullptr; }
        virtual ~ENode() {
            if (next) {
                memdelete(next);
            }
        }
    };

    struct Expression {

        bool is_op;
        union {
            Variant::Operator op;
            ENode *node;
        };
    };

    ENode *_parse_expression();

    struct InputNode : public ENode {

        int index;
        InputNode() {
            type = TYPE_INPUT;
        }
    };

    struct ConstantNode : public ENode {

        Variant value;
        ConstantNode() {
            type = TYPE_CONSTANT;
        }
    };

    struct OperatorNode : public ENode {

        Variant::Operator op;

        ENode *nodes[2];

        OperatorNode() {
            type = TYPE_OPERATOR;
        }
    };

    struct SelfNode : public ENode {

        SelfNode() {
            type = TYPE_SELF;
        }
    };

    struct IndexNode : public ENode {
        ENode *base;
        ENode *index;

        IndexNode() {
            type = TYPE_INDEX;
        }
    };

    struct NamedIndexNode : public ENode {
        ENode *base;
        StringName name;

        NamedIndexNode() {
            type = TYPE_NAMED_INDEX;
        }
    };

    struct ConstructorNode : public ENode {
        VariantType data_type;
        PODVector<ENode *> arguments;

        ConstructorNode() {
            type = TYPE_CONSTRUCTOR;
        }
    };

    struct CallNode : public ENode {
        ENode *base;
        StringName method;
        PODVector<ENode *> arguments;

        CallNode() {
            type = TYPE_CALL;
        }
    };

    struct ArrayNode : public ENode {
        PODVector<ENode *> array;
        ArrayNode() {
            type = TYPE_ARRAY;
        }
    };

    struct DictionaryNode : public ENode {
        PODVector<ENode *> dict;
        DictionaryNode() {
            type = TYPE_DICTIONARY;
        }
    };

    struct BuiltinFuncNode : public ENode {
        VisualScriptBuiltinFunc::BuiltinFunc func;
        PODVector<ENode *> arguments;
        BuiltinFuncNode() {
            type = TYPE_BUILTIN_FUNC;
        }
    };

    template <class T>
    T *alloc_node() {
        T *node = memnew(T);
        node->next = nodes;
        nodes = node;
        return node;
    }

    ENode *root;
    ENode *nodes;

protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(PODVector<PropertyInfo> *p_list) const;

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    se_string_view get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    se_string_view get_caption() const override;
    String get_text() const override;
    const char *get_category() const override { return "operators"; }

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptExpression();
    ~VisualScriptExpression() override;
};

void register_visual_script_expression_node();
