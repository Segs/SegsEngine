/*************************************************************************/
/*  visual_script_nodes.h                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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
#include "core/method_info.h"

class VisualScriptFunction : public VisualScriptNode {

    GDCLASS(VisualScriptFunction,VisualScriptNode)


    struct Argument {
        StringName name;
        VariantType type;
        PropertyHint hint;
        String hint_string;
    };

    Vector<Argument> arguments;

    bool stack_less;
    int stack_size;
    MultiplayerAPI_RPCMode rpc_mode;
    bool sequenced;

protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(ListPOD<PropertyInfo> *p_list) const;

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_text() const override;
    String get_category() const override { return "flow_control"; }

    void add_argument(VariantType p_type, const StringName &p_name, int p_index = -1, const PropertyHint p_hint = PROPERTY_HINT_NONE, const String &p_hint_string = String(""));
    void set_argument_type(int p_argidx, VariantType p_type);
    VariantType get_argument_type(int p_argidx) const;
    void set_argument_name(int p_argidx, const StringName &p_name);
    StringName get_argument_name(int p_argidx) const;
    void remove_argument(int p_argidx);
    int get_argument_count() const;

    void set_stack_less(bool p_enable);
    bool is_stack_less() const;

    void set_sequenced(bool p_enable);
    bool is_sequenced() const;

    void set_stack_size(int p_size);
    int get_stack_size() const;

    void set_return_type_enabled(bool p_returns);
    bool is_return_type_enabled() const;

    void set_return_type(VariantType p_type);
    VariantType get_return_type() const;

    void set_rpc_mode(MultiplayerAPI_RPCMode p_mode);
    MultiplayerAPI_RPCMode get_rpc_mode() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptFunction();
};

class VisualScriptLists : public VisualScriptNode {

    GDCLASS(VisualScriptLists, VisualScriptNode)

    struct Port {
        StringName name;
        VariantType type;
    };

protected:
    Vector<Port> inputports;
    Vector<Port> outputports;

    enum {
        OUTPUT_EDITABLE = 0x0001,
        OUTPUT_NAME_EDITABLE = 0x0002,
        OUTPUT_TYPE_EDITABLE = 0x0004,
        INPUT_EDITABLE = 0x0008,
        INPUT_NAME_EDITABLE = 0x000F,
        INPUT_TYPE_EDITABLE = 0x0010,
    };

    int flags;

    bool sequenced;

    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(ListPOD<PropertyInfo> *p_list) const;

    static void _bind_methods();

public:
    virtual bool is_output_port_editable() const;
    virtual bool is_output_port_name_editable() const;
    virtual bool is_output_port_type_editable() const;

    virtual bool is_input_port_editable() const;
    virtual bool is_input_port_name_editable() const;
    virtual bool is_input_port_type_editable() const;

    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override = 0;
    String get_text() const override = 0;
    String get_category() const override = 0;

    void add_input_data_port(VariantType p_type, const StringName &p_name, int p_index = -1);
    void set_input_data_port_type(int p_idx, VariantType p_type);
    void set_input_data_port_name(int p_idx, const StringName &p_name);
    void remove_input_data_port(int p_argidx);

    void add_output_data_port(VariantType p_type, const StringName &p_name, int p_index = -1);
    void set_output_data_port_type(int p_idx, VariantType p_type);
    void set_output_data_port_name(int p_idx, const StringName &p_name);
    void remove_output_data_port(int p_argidx);

    void set_sequenced(bool p_enable);
    bool is_sequenced() const;

    VisualScriptLists();
};

class VisualScriptComposeArray : public VisualScriptLists {

    GDCLASS(VisualScriptComposeArray, VisualScriptLists)

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_text() const override;
    String get_category() const override { return "functions"; }

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptComposeArray();
};
class VisualScriptOperator : public VisualScriptNode {

    GDCLASS(VisualScriptOperator,VisualScriptNode)


    VariantType typed;
    Variant::Operator op;

protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "operators"; }

    void set_operator(Variant::Operator p_op);
    Variant::Operator get_operator() const;

    void set_typed(VariantType p_op);
    VariantType get_typed() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptOperator();
};

class VisualScriptSelect : public VisualScriptNode {

    GDCLASS(VisualScriptSelect,VisualScriptNode)


    VariantType typed;

protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_text() const override;
    String get_category() const override { return "operators"; }

    void set_typed(VariantType p_op);
    VariantType get_typed() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptSelect();
};

class VisualScriptVariableGet : public VisualScriptNode {

    GDCLASS(VisualScriptVariableGet,VisualScriptNode)


    StringName variable;

protected:
    void _validate_property(PropertyInfo &property) const override;
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "data"; }

    void set_variable(StringName p_variable);
    StringName get_variable() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptVariableGet();
};

class VisualScriptVariableSet : public VisualScriptNode {

    GDCLASS(VisualScriptVariableSet,VisualScriptNode)


    StringName variable;

protected:
    void _validate_property(PropertyInfo &property) const override;
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "data"; }

    void set_variable(StringName p_variable);
    StringName get_variable() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptVariableSet();
};

class VisualScriptConstant : public VisualScriptNode {

    GDCLASS(VisualScriptConstant,VisualScriptNode)


    VariantType type;
    Variant value;

protected:
    void _validate_property(PropertyInfo &property) const override;
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "constants"; }

    void set_constant_type(VariantType p_type);
    VariantType get_constant_type() const;

    void set_constant_value(Variant p_value);
    Variant get_constant_value() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptConstant();
};

class VisualScriptPreload : public VisualScriptNode {

    GDCLASS(VisualScriptPreload,VisualScriptNode)


    Ref<Resource> preload;

protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "data"; }

    void set_preload(const Ref<Resource> &p_preload);
    Ref<Resource> get_preload() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptPreload();
};

class VisualScriptIndexGet : public VisualScriptNode {

    GDCLASS(VisualScriptIndexGet,VisualScriptNode)


public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "operators"; }

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptIndexGet();
};

class VisualScriptIndexSet : public VisualScriptNode {

    GDCLASS(VisualScriptIndexSet,VisualScriptNode)


public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "operators"; }

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptIndexSet();
};

class VisualScriptGlobalConstant : public VisualScriptNode {

    GDCLASS(VisualScriptGlobalConstant,VisualScriptNode)


    int index;

    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "constants"; }

    void set_global_constant(int p_which);
    int get_global_constant();

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptGlobalConstant();
};

class VisualScriptClassConstant : public VisualScriptNode {

    GDCLASS(VisualScriptClassConstant,VisualScriptNode)


    StringName base_type;
    StringName name;

protected:
    static void _bind_methods();
    void _validate_property(PropertyInfo &property) const override;

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "constants"; }

    void set_class_constant(const StringName &p_which);
    StringName get_class_constant();

    void set_base_type(const StringName &p_which);
    StringName get_base_type();

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptClassConstant();
};

class VisualScriptBasicTypeConstant : public VisualScriptNode {

    GDCLASS(VisualScriptBasicTypeConstant,VisualScriptNode)


    VariantType type;
    StringName name;

protected:
    static void _bind_methods();
    void _validate_property(PropertyInfo &property) const override;

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_text() const override;
    String get_category() const override { return "constants"; }

    void set_basic_type_constant(const StringName &p_which);
    StringName get_basic_type_constant() const;

    void set_basic_type(VariantType p_which);
    VariantType get_basic_type() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptBasicTypeConstant();
};

class VisualScriptMathConstant : public VisualScriptNode {

    GDCLASS(VisualScriptMathConstant,VisualScriptNode)


public:
    enum MathConstant {
        MATH_CONSTANT_ONE,
        MATH_CONSTANT_PI,
        MATH_CONSTANT_HALF_PI,
        MATH_CONSTANT_TAU,
        MATH_CONSTANT_E,
        MATH_CONSTANT_SQRT2,
        MATH_CONSTANT_INF,
        MATH_CONSTANT_NAN,
        MATH_CONSTANT_MAX
    };

private:
    static const char *const_name[MATH_CONSTANT_MAX];
    static double const_value[MATH_CONSTANT_MAX];
    MathConstant constant;

protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "constants"; }

    void set_math_constant(MathConstant p_which);
    MathConstant get_math_constant();

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptMathConstant();
};


class VisualScriptEngineSingleton : public VisualScriptNode {

    GDCLASS(VisualScriptEngineSingleton,VisualScriptNode)


    String singleton;

    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "data"; }

    void set_singleton(const String &p_string);
    String get_singleton();

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    TypeGuess guess_output_type(TypeGuess *p_inputs, int p_output) const override;

    VisualScriptEngineSingleton();
};

class VisualScriptSceneNode : public VisualScriptNode {

    GDCLASS(VisualScriptSceneNode,VisualScriptNode)


    NodePath path;

protected:
    void _validate_property(PropertyInfo &property) const override;
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "data"; }

    void set_node_path(const NodePath &p_path);
    NodePath get_node_path();

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    TypeGuess guess_output_type(TypeGuess *p_inputs, int p_output) const override;

    VisualScriptSceneNode();
};

class VisualScriptSceneTree : public VisualScriptNode {

    GDCLASS(VisualScriptSceneTree,VisualScriptNode)


protected:
    void _validate_property(PropertyInfo &property) const override;
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "data"; }

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    TypeGuess guess_output_type(TypeGuess *p_inputs, int p_output) const override;

    VisualScriptSceneTree();
};

class VisualScriptResourcePath : public VisualScriptNode {

    GDCLASS(VisualScriptResourcePath,VisualScriptNode)


    String path;

protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "data"; }

    void set_resource_path(const String &p_path);
    String get_resource_path();

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptResourcePath();
};

class VisualScriptSelf : public VisualScriptNode {

    GDCLASS(VisualScriptSelf,VisualScriptNode)


protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override { return "data"; }

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    TypeGuess guess_output_type(TypeGuess *p_inputs, int p_output) const override;

    VisualScriptSelf();
};

class VisualScriptCustomNode : public VisualScriptNode {

    GDCLASS(VisualScriptCustomNode,VisualScriptNode)


protected:
    static void _bind_methods();

public:
    enum StartMode { //replicated for step
        START_MODE_BEGIN_SEQUENCE,
        START_MODE_CONTINUE_SEQUENCE,
        START_MODE_RESUME_YIELD
    };

    enum { //replicated for step
        STEP_SHIFT = 1 << 24,
        STEP_MASK = STEP_SHIFT - 1,
        STEP_PUSH_STACK_BIT = STEP_SHIFT, //push bit to stack
        STEP_GO_BACK_BIT = STEP_SHIFT << 1, //go back to previous node
        STEP_NO_ADVANCE_BIT = STEP_SHIFT << 2, //do not advance past this node
        STEP_EXIT_FUNCTION_BIT = STEP_SHIFT << 3, //return from function
        STEP_YIELD_BIT = STEP_SHIFT << 4, //yield (will find VisualScriptFunctionState state in first working memory)
    };

    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_text() const override;
    String get_category() const override;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    void _script_changed();

    VisualScriptCustomNode();
};


class VisualScriptSubCall : public VisualScriptNode {

    GDCLASS(VisualScriptSubCall,VisualScriptNode)


protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_text() const override;
    String get_category() const override;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptSubCall();
};

class VisualScriptComment : public VisualScriptNode {

    GDCLASS(VisualScriptComment,VisualScriptNode)


    String title;
    String description;
    Size2 size;

protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_text() const override;
    String get_category() const override;

    void set_title(const String &p_title);
    String get_title() const;

    void set_description(const String &p_description);
    String get_description() const;

    void set_size(const Size2 &p_size);
    Size2 get_size() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptComment();
};

class VisualScriptConstructor : public VisualScriptNode {

    GDCLASS(VisualScriptConstructor,VisualScriptNode)


    VariantType type;
    MethodInfo constructor;

protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override;

    void set_constructor_type(VariantType p_type);
    VariantType get_constructor_type() const;

    void set_constructor(const Dictionary &p_info);
    Dictionary get_constructor() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptConstructor();
};

class VisualScriptLocalVar : public VisualScriptNode {

    GDCLASS(VisualScriptLocalVar,VisualScriptNode)


    StringName name;
    VariantType type;

protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override;

    void set_var_name(const StringName &p_name);
    StringName get_var_name() const;

    void set_var_type(VariantType p_type);
    VariantType get_var_type() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptLocalVar();
};

class VisualScriptLocalVarSet : public VisualScriptNode {

    GDCLASS(VisualScriptLocalVarSet,VisualScriptNode)


    StringName name;
    VariantType type;

protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_text() const override;
    String get_category() const override;

    void set_var_name(const StringName &p_name);
    StringName get_var_name() const;

    void set_var_type(VariantType p_type);
    VariantType get_var_type() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptLocalVarSet();
};

class VisualScriptInputAction : public VisualScriptNode {

    GDCLASS(VisualScriptInputAction,VisualScriptNode)


public:
    enum Mode {
        MODE_PRESSED,
        MODE_RELEASED,
        MODE_JUST_PRESSED,
        MODE_JUST_RELEASED,
    };

    StringName name;
    Mode mode;

protected:
    void _validate_property(PropertyInfo &property) const override;

    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override;

    void set_action_name(const StringName &p_name);
    StringName get_action_name() const;

    void set_action_mode(Mode p_mode);
    Mode get_action_mode() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptInputAction();
};


class VisualScriptDeconstruct : public VisualScriptNode {

    GDCLASS(VisualScriptDeconstruct,VisualScriptNode)


    struct Element {
        StringName name;
        VariantType type;
    };

    Vector<Element> elements;

    void _update_elements();
    VariantType type;

    void _set_elem_cache(const Array &p_elements);
    Array _get_elem_cache() const;

    void _validate_property(PropertyInfo &property) const override;

protected:
    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    String get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    String get_caption() const override;
    String get_category() const override;

    void set_deconstruct_type(VariantType p_type);
    VariantType get_deconstruct_type() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptDeconstruct();
};

void register_visual_script_nodes();
void unregister_visual_script_nodes();
