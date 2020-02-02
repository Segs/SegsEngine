/*************************************************************************/
/*  visual_shader_nodes.h                                                */
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

#include "scene/resources/visual_shader.h"
#include "core/math/transform.h"

///////////////////////////////////////
/// CONSTANTS
///////////////////////////////////////

class VisualShaderNodeScalarConstant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarConstant,VisualShaderNode)
    float constant;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_constant(float p_value);
    float get_constant() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeScalarConstant();
};

///////////////////////////////////////

class VisualShaderNodeBooleanConstant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeBooleanConstant,VisualShaderNode)

    bool constant;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_constant(bool p_value);
    bool get_constant() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeBooleanConstant();
};

///////////////////////////////////////

class VisualShaderNodeColorConstant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeColorConstant,VisualShaderNode)

    Color constant;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_constant(Color p_value);
    Color get_constant() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeColorConstant();
};

///////////////////////////////////////

class VisualShaderNodeVec3Constant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVec3Constant,VisualShaderNode)

    Vector3 constant;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_constant(Vector3 p_value);
    Vector3 get_constant() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeVec3Constant();
};

///////////////////////////////////////

class VisualShaderNodeTransformConstant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeTransformConstant,VisualShaderNode)

    Transform constant;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_constant(Transform p_value);
    Transform get_constant() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeTransformConstant();
};

///////////////////////////////////////
/// TEXTURES
///////////////////////////////////////

class VisualShaderNodeTexture : public VisualShaderNode {
    GDCLASS(VisualShaderNodeTexture,VisualShaderNode)

    Ref<Texture> texture;

public:
    enum Source {
        SOURCE_TEXTURE,
        SOURCE_SCREEN,
        SOURCE_2D_TEXTURE,
        SOURCE_2D_NORMAL,
        SOURCE_DEPTH,
        SOURCE_PORT,
    };

    enum TextureType {
        TYPE_DATA,
        TYPE_COLOR,
        TYPE_NORMALMAP
    };

private:
    Source source;
    TextureType texture_type;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    StringName get_input_port_default_hint(int p_port) const override;

    Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const override;
    String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_source(Source p_source);
    Source get_source() const;

    void set_texture(Ref<Texture> p_value);
    Ref<Texture> get_texture() const;

    void set_texture_type(TextureType p_type);
    TextureType get_texture_type() const;

    Vector<StringName> get_editable_properties() const override;

    StringName get_warning(ShaderMode p_mode, VisualShader::Type p_type) const override;

    VisualShaderNodeTexture();
};


///////////////////////////////////////

class VisualShaderNodeCubeMap : public VisualShaderNode {
    GDCLASS(VisualShaderNodeCubeMap,VisualShaderNode)

    Ref<CubeMap> cube_map;

public:
    enum Source {
        SOURCE_TEXTURE,
        SOURCE_PORT
    };
    enum TextureType {
        TYPE_DATA,
        TYPE_COLOR,
        TYPE_NORMALMAP
    };

private:
    Source source;
    TextureType texture_type;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;
    StringName get_input_port_default_hint(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const override;
    String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_source(Source p_source);
    Source get_source() const;

    void set_cube_map(Ref<CubeMap> p_value);
    Ref<CubeMap> get_cube_map() const;

    void set_texture_type(TextureType p_type);
    TextureType get_texture_type() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeCubeMap();
};


///////////////////////////////////////
/// OPS
///////////////////////////////////////

class VisualShaderNodeScalarOp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarOp,VisualShaderNode)


public:
    enum Operator {
        OP_ADD,
        OP_SUB,
        OP_MUL,
        OP_DIV,
        OP_MOD,
        OP_POW,
        OP_MAX,
        OP_MIN,
        OP_ATAN2,
        OP_STEP
    };

protected:
    Operator op;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_operator(Operator p_op);
    Operator get_operator() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeScalarOp();
};


class VisualShaderNodeVectorOp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorOp,VisualShaderNode)


public:
    enum Operator {
        OP_ADD,
        OP_SUB,
        OP_MUL,
        OP_DIV,
        OP_MOD,
        OP_POW,
        OP_MAX,
        OP_MIN,
        OP_CROSS,
        OP_ATAN2,
        OP_REFLECT,
        OP_STEP
    };

protected:
    Operator op;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_operator(Operator p_op);
    Operator get_operator() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeVectorOp();
};


///////////////////////////////////////

class VisualShaderNodeColorOp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeColorOp,VisualShaderNode)


public:
    enum Operator {
        OP_SCREEN,
        OP_DIFFERENCE,
        OP_DARKEN,
        OP_LIGHTEN,
        OP_OVERLAY,
        OP_DODGE,
        OP_BURN,
        OP_SOFT_LIGHT,
        OP_HARD_LIGHT
    };

protected:
    Operator op;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_operator(Operator p_op);
    Operator get_operator() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeColorOp();
};


///////////////////////////////////////
/// TRANSFORM-TRANSFORM MULTIPLICATION
///////////////////////////////////////

class VisualShaderNodeTransformMult : public VisualShaderNode {
    GDCLASS(VisualShaderNodeTransformMult,VisualShaderNode)


public:
    enum Operator {
        OP_AxB,
        OP_BxA,
        OP_AxB_COMP,
        OP_BxA_COMP
    };

protected:
    Operator op;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_operator(Operator p_op);
    Operator get_operator() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeTransformMult();
};


///////////////////////////////////////
/// TRANSFORM-VECTOR MULTIPLICATION
///////////////////////////////////////

class VisualShaderNodeTransformVecMult : public VisualShaderNode {
    GDCLASS(VisualShaderNodeTransformVecMult,VisualShaderNode)

public:
    enum Operator {
        OP_AxB,
        OP_BxA,
        OP_3x3_AxB,
        OP_3x3_BxA,
    };

protected:
    Operator op;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_operator(Operator p_op);
    Operator get_operator() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeTransformVecMult();
};


///////////////////////////////////////
/// SCALAR FUNC
///////////////////////////////////////

class VisualShaderNodeScalarFunc : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarFunc,VisualShaderNode)

public:
    enum Function {
        FUNC_SIN,
        FUNC_COS,
        FUNC_TAN,
        FUNC_ASIN,
        FUNC_ACOS,
        FUNC_ATAN,
        FUNC_SINH,
        FUNC_COSH,
        FUNC_TANH,
        FUNC_LOG,
        FUNC_EXP,
        FUNC_SQRT,
        FUNC_ABS,
        FUNC_SIGN,
        FUNC_FLOOR,
        FUNC_ROUND,
        FUNC_CEIL,
        FUNC_FRAC,
        FUNC_SATURATE,
        FUNC_NEGATE,
        FUNC_ACOSH,
        FUNC_ASINH,
        FUNC_ATANH,
        FUNC_DEGREES,
        FUNC_EXP2,
        FUNC_INVERSE_SQRT,
        FUNC_LOG2,
        FUNC_RADIANS,
        FUNC_RECIPROCAL,
        FUNC_ROUNDEVEN,
        FUNC_TRUNC,
        FUNC_ONEMINUS
    };

protected:
    Function func;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeScalarFunc();
};


///////////////////////////////////////
/// VECTOR FUNC
///////////////////////////////////////

class VisualShaderNodeVectorFunc : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorFunc,VisualShaderNode)

public:
    enum Function {
        FUNC_NORMALIZE,
        FUNC_SATURATE,
        FUNC_NEGATE,
        FUNC_RECIPROCAL,
        FUNC_RGB2HSV,
        FUNC_HSV2RGB,
        FUNC_ABS,
        FUNC_ACOS,
        FUNC_ACOSH,
        FUNC_ASIN,
        FUNC_ASINH,
        FUNC_ATAN,
        FUNC_ATANH,
        FUNC_CEIL,
        FUNC_COS,
        FUNC_COSH,
        FUNC_DEGREES,
        FUNC_EXP,
        FUNC_EXP2,
        FUNC_FLOOR,
        FUNC_FRAC,
        FUNC_INVERSE_SQRT,
        FUNC_LOG,
        FUNC_LOG2,
        FUNC_RADIANS,
        FUNC_ROUND,
        FUNC_ROUNDEVEN,
        FUNC_SIGN,
        FUNC_SIN,
        FUNC_SINH,
        FUNC_SQRT,
        FUNC_TAN,
        FUNC_TANH,
        FUNC_TRUNC,
        FUNC_ONEMINUS
    };

protected:
    Function func;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeVectorFunc();
};

///////////////////////////////////////
/// COLOR FUNC
///////////////////////////////////////

class VisualShaderNodeColorFunc : public VisualShaderNode {
    GDCLASS(VisualShaderNodeColorFunc,VisualShaderNode)

public:
    enum Function {
        FUNC_GRAYSCALE,
        FUNC_SEPIA
    };

protected:
    Function func;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeColorFunc();
};


///////////////////////////////////////
/// TRANSFORM FUNC
///////////////////////////////////////

class VisualShaderNodeTransformFunc : public VisualShaderNode {
    GDCLASS(VisualShaderNodeTransformFunc,VisualShaderNode)

public:
    enum Function {
        FUNC_INVERSE,
        FUNC_TRANSPOSE
    };

protected:
    Function func;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeTransformFunc();
};


///////////////////////////////////////
/// DOT
///////////////////////////////////////

class VisualShaderNodeDotProduct : public VisualShaderNode {
    GDCLASS(VisualShaderNodeDotProduct,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeDotProduct();
};

///////////////////////////////////////
/// LENGTH
///////////////////////////////////////

class VisualShaderNodeVectorLen : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorLen,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorLen();
};

///////////////////////////////////////
/// DETERMINANT
///////////////////////////////////////

class VisualShaderNodeDeterminant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeDeterminant,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeDeterminant();
};

///////////////////////////////////////
/// CLAMP
///////////////////////////////////////

class VisualShaderNodeScalarClamp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarClamp,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeScalarClamp();
};

///////////////////////////////////////

class VisualShaderNodeVectorClamp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorClamp,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorClamp();
};

///////////////////////////////////////
/// DERIVATIVE FUNCTIONS
///////////////////////////////////////

class VisualShaderNodeScalarDerivativeFunc : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarDerivativeFunc,VisualShaderNode)


public:
    enum Function {
        FUNC_SUM,
        FUNC_X,
        FUNC_Y
    };

protected:
    Function func;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeScalarDerivativeFunc();
};

///////////////////////////////////////

class VisualShaderNodeVectorDerivativeFunc : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorDerivativeFunc,VisualShaderNode)


public:
    enum Function {
        FUNC_SUM,
        FUNC_X,
        FUNC_Y
    };

protected:
    Function func;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeVectorDerivativeFunc();
};


///////////////////////////////////////
/// FACEFORWARD
///////////////////////////////////////

class VisualShaderNodeFaceForward : public VisualShaderNode {
    GDCLASS(VisualShaderNodeFaceForward,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeFaceForward();
};

///////////////////////////////////////
/// OUTER PRODUCT
///////////////////////////////////////

class VisualShaderNodeOuterProduct : public VisualShaderNode {
    GDCLASS(VisualShaderNodeOuterProduct,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeOuterProduct();
};

///////////////////////////////////////
/// STEP
///////////////////////////////////////

class VisualShaderNodeVectorScalarStep : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorScalarStep,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorScalarStep();
};

///////////////////////////////////////
/// SMOOTHSTEP
///////////////////////////////////////

class VisualShaderNodeScalarSmoothStep : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarSmoothStep,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeScalarSmoothStep();
};

///////////////////////////////////////

class VisualShaderNodeVectorSmoothStep : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorSmoothStep,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorSmoothStep();
};

///////////////////////////////////////

class VisualShaderNodeVectorScalarSmoothStep : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorScalarSmoothStep,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorScalarSmoothStep();
};

///////////////////////////////////////
/// DISTANCE
///////////////////////////////////////

class VisualShaderNodeVectorDistance : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorDistance,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorDistance();
};

///////////////////////////////////////
/// REFRACT
///////////////////////////////////////

class VisualShaderNodeVectorRefract : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorRefract,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorRefract();
};

///////////////////////////////////////
/// MIX
///////////////////////////////////////

class VisualShaderNodeScalarInterp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarInterp,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeScalarInterp();
};

///////////////////////////////////////

class VisualShaderNodeVectorInterp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorInterp,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorInterp();
};

///////////////////////////////////////

class VisualShaderNodeVectorScalarMix : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorScalarMix,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorScalarMix();
};

///////////////////////////////////////
/// COMPOSE
///////////////////////////////////////

class VisualShaderNodeVectorCompose : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorCompose,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorCompose();
};

///////////////////////////////////////

class VisualShaderNodeTransformCompose : public VisualShaderNode {
    GDCLASS(VisualShaderNodeTransformCompose,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeTransformCompose();
};

///////////////////////////////////////
/// DECOMPOSE
///////////////////////////////////////

class VisualShaderNodeVectorDecompose : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorDecompose,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorDecompose();
};

///////////////////////////////////////

class VisualShaderNodeTransformDecompose : public VisualShaderNode {
    GDCLASS(VisualShaderNodeTransformDecompose,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeTransformDecompose();
};

///////////////////////////////////////
/// UNIFORMS
///////////////////////////////////////

class VisualShaderNodeScalarUniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeScalarUniform,VisualShaderNodeUniform)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeScalarUniform();
};

///////////////////////////////////////

class VisualShaderNodeBooleanUniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeBooleanUniform,VisualShaderNodeUniform)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeBooleanUniform();
};

///////////////////////////////////////

class VisualShaderNodeColorUniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeColorUniform,VisualShaderNodeUniform)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeColorUniform();
};

///////////////////////////////////////

class VisualShaderNodeVec3Uniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeVec3Uniform,VisualShaderNodeUniform)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVec3Uniform();
};

///////////////////////////////////////

class VisualShaderNodeTransformUniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeTransformUniform,VisualShaderNodeUniform)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeTransformUniform();
};

///////////////////////////////////////

class VisualShaderNodeTextureUniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeTextureUniform,VisualShaderNodeUniform)


public:
    enum TextureType {
        TYPE_DATA,
        TYPE_COLOR,
        TYPE_NORMALMAP,
        TYPE_ANISO,
    };

    enum ColorDefault {
        COLOR_DEFAULT_WHITE,
        COLOR_DEFAULT_BLACK
    };

protected:
    TextureType texture_type;
    ColorDefault color_default;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;
    StringName get_input_port_default_hint(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    Vector<StringName> get_editable_properties() const override;

    void set_texture_type(TextureType p_type);
    TextureType get_texture_type() const;

    void set_color_default(ColorDefault p_default);
    ColorDefault get_color_default() const;

    VisualShaderNodeTextureUniform();
};


///////////////////////////////////////

class VisualShaderNodeTextureUniformTriplanar : public VisualShaderNodeTextureUniform {
    GDCLASS(VisualShaderNodeTextureUniformTriplanar,VisualShaderNodeTextureUniform)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    StringName get_input_port_default_hint(int p_port) const override;

    String generate_global_per_node(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_global_per_func(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeTextureUniformTriplanar();
};

///////////////////////////////////////

class VisualShaderNodeCubeMapUniform : public VisualShaderNodeTextureUniform {
    GDCLASS(VisualShaderNodeCubeMapUniform,VisualShaderNodeTextureUniform)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    StringName get_input_port_default_hint(int p_port) const override;
    String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeCubeMapUniform();
};

///////////////////////////////////////
/// IF
///////////////////////////////////////

class VisualShaderNodeIf : public VisualShaderNode {
    GDCLASS(VisualShaderNodeIf,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

    VisualShaderNodeIf();
};

///////////////////////////////////////
/// SWITCH
///////////////////////////////////////

class VisualShaderNodeSwitch : public VisualShaderNode {
    GDCLASS(VisualShaderNodeSwitch,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

    VisualShaderNodeSwitch();
};

class VisualShaderNodeScalarSwitch : public VisualShaderNodeSwitch {
    GDCLASS(VisualShaderNodeScalarSwitch, VisualShaderNodeSwitch);

public:
    se_string_view get_caption() const override;

    PortType get_input_port_type(int p_port) const override;
    PortType get_output_port_type(int p_port) const override;

    VisualShaderNodeScalarSwitch();
};
///////////////////////////////////////
/// FRESNEL
///////////////////////////////////////

class VisualShaderNodeFresnel : public VisualShaderNode {
    GDCLASS(VisualShaderNodeFresnel,VisualShaderNode)


public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;
    StringName get_input_port_default_hint(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

    VisualShaderNodeFresnel();
};

///////////////////////////////////////
/// Is
///////////////////////////////////////

class VisualShaderNodeIs : public VisualShaderNode {
    GDCLASS(VisualShaderNodeIs,VisualShaderNode)


public:
    enum Function {
        FUNC_IS_INF,
        FUNC_IS_NAN,
    };

protected:
    Function func;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeIs();
};

///////////////////////////////////////
/// Compare
///////////////////////////////////////

class VisualShaderNodeCompare : public VisualShaderNode {
    GDCLASS(VisualShaderNodeCompare,VisualShaderNode)


public:
    enum ComparisonType {
        CTYPE_SCALAR,
        CTYPE_VECTOR,
        CTYPE_BOOLEAN,
        CTYPE_TRANSFORM
    };

    enum Function {
        FUNC_EQUAL,
        FUNC_NOT_EQUAL,
        FUNC_GREATER_THAN,
        FUNC_GREATER_THAN_EQUAL,
        FUNC_LESS_THAN,
        FUNC_LESS_THAN_EQUAL,
    };

    enum Condition {
        COND_ALL,
        COND_ANY,
    };

protected:
    ComparisonType ctype;
    Function func;
    Condition condition;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_comparison_type(ComparisonType p_type);
    ComparisonType get_comparison_type() const;

    void set_function(Function p_func);
    Function get_function() const;

    void set_condition(Condition p_cond);
    Condition get_condition() const;

    Vector<StringName> get_editable_properties() const override;
    StringName get_warning(ShaderMode p_mode, VisualShader::Type p_type) const override;

    VisualShaderNodeCompare();
};
