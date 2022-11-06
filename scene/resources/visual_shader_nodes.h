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

class GODOT_EXPORT VisualShaderNodeScalarConstant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarConstant,VisualShaderNode)
    float constant = 0.0f;

protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_constant(float p_value);
    float get_constant() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeScalarConstant();
};

///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeBooleanConstant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeBooleanConstant,VisualShaderNode)

    bool constant = false;

protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_constant(bool p_value);
    bool get_constant() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeBooleanConstant();
};

///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeColorConstant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeColorConstant,VisualShaderNode)

    Color constant;

protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_constant(Color p_value);
    Color get_constant() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeColorConstant();
};

///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeVec3Constant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVec3Constant,VisualShaderNode)

    Vector3 constant;

protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_constant(Vector3 p_value);
    Vector3 get_constant() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeVec3Constant();
};

///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeTransformConstant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeTransformConstant,VisualShaderNode)

    Transform constant;

protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_constant(Transform p_value);
    Transform get_constant() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeTransformConstant();
};

///////////////////////////////////////
/// TEXTURES
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeTexture : public VisualShaderNode {
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
    Source source = SOURCE_TEXTURE;
    TextureType texture_type = TYPE_DATA;

protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    StringName get_input_port_default_hint(int p_port) const override;

    Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const override;
    String generate_global(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_source(Source p_source);
    Source get_source() const;

    void set_texture(Ref<Texture> p_value);
    Ref<Texture> get_texture() const;

    void set_texture_type(TextureType p_type);
    TextureType get_texture_type() const;

    Vector<StringName> get_editable_properties() const override;

    StringName get_warning(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type) const override;

    VisualShaderNodeTexture();
};


///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeCubeMap : public VisualShaderNode {
    GDCLASS(VisualShaderNodeCubeMap,VisualShaderNode)

    Ref<CubeMap> cube_map;

public:
    enum Source {
        SOURCE_TEXTURE,
        SOURCE_PORT,
        SOURCE_UNKNOWN
    };
    enum TextureType {
        TYPE_DATA,
        TYPE_COLOR,
        TYPE_NORMALMAP
    };

private:
    Source source = SOURCE_TEXTURE;
    TextureType texture_type = TYPE_DATA;

protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;
    StringName get_input_port_default_hint(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type,
            int p_id) const override;
    String generate_global(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_source(Source p_source);
    Source get_source() const;

    void set_cube_map(const Ref<CubeMap> &p_value);
    Ref<CubeMap> get_cube_map() const;

    void set_texture_type(TextureType p_type);
    TextureType get_texture_type() const;

    Vector<StringName> get_editable_properties() const override;
    StringName get_warning(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type) const override;

    VisualShaderNodeCubeMap();
};


///////////////////////////////////////
/// OPS
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeScalarOp : public VisualShaderNode {
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
    Operator op = OP_ADD;

    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_operator(Operator p_op);
    Operator get_operator() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeScalarOp();
};


class GODOT_EXPORT VisualShaderNodeVectorOp : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_operator(Operator p_op);
    Operator get_operator() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeVectorOp();
};


///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeColorOp : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_operator(Operator p_op);
    Operator get_operator() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeColorOp();
};


///////////////////////////////////////
/// TRANSFORM-TRANSFORM MULTIPLICATION
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeTransformMult : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_operator(Operator p_op);
    Operator get_operator() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeTransformMult();
};


///////////////////////////////////////
/// TRANSFORM-VECTOR MULTIPLICATION
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeTransformVecMult : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_operator(Operator p_op);
    Operator get_operator() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeTransformVecMult();
};


///////////////////////////////////////
/// SCALAR FUNC
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeScalarFunc : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeScalarFunc();
};


///////////////////////////////////////
/// VECTOR FUNC
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeVectorFunc : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeVectorFunc();
};

///////////////////////////////////////
/// COLOR FUNC
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeColorFunc : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeColorFunc();
};


///////////////////////////////////////
/// TRANSFORM FUNC
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeTransformFunc : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeDotProduct();
};

///////////////////////////////////////
/// LENGTH
///////////////////////////////////////

class VisualShaderNodeVectorLen : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorLen,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorLen();
};

///////////////////////////////////////
/// DETERMINANT
///////////////////////////////////////

class VisualShaderNodeDeterminant : public VisualShaderNode {
    GDCLASS(VisualShaderNodeDeterminant,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeDeterminant();
};

///////////////////////////////////////
/// CLAMP
///////////////////////////////////////

class VisualShaderNodeScalarClamp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarClamp,VisualShaderNode)

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeScalarClamp();
};

///////////////////////////////////////

class VisualShaderNodeVectorClamp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorClamp,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorClamp();
};

///////////////////////////////////////
/// DERIVATIVE FUNCTIONS
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeScalarDerivativeFunc : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeScalarDerivativeFunc();
};

///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeVectorDerivativeFunc : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeFaceForward();
};

///////////////////////////////////////
/// OUTER PRODUCT
///////////////////////////////////////

class VisualShaderNodeOuterProduct : public VisualShaderNode {
    GDCLASS(VisualShaderNodeOuterProduct,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeOuterProduct();
};

///////////////////////////////////////
/// STEP
///////////////////////////////////////

class VisualShaderNodeVectorScalarStep : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorScalarStep,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorScalarStep();
};

///////////////////////////////////////
/// SMOOTHSTEP
///////////////////////////////////////

class VisualShaderNodeScalarSmoothStep : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarSmoothStep,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeScalarSmoothStep();
};

///////////////////////////////////////

class VisualShaderNodeVectorSmoothStep : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorSmoothStep,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorSmoothStep();
};

///////////////////////////////////////

class VisualShaderNodeVectorScalarSmoothStep : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorScalarSmoothStep,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorScalarSmoothStep();
};

///////////////////////////////////////
/// DISTANCE
///////////////////////////////////////

class VisualShaderNodeVectorDistance : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorDistance,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorDistance();
};

///////////////////////////////////////
/// REFRACT
///////////////////////////////////////

class VisualShaderNodeVectorRefract : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorRefract,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorRefract();
};

///////////////////////////////////////
/// MIX
///////////////////////////////////////

class VisualShaderNodeScalarInterp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeScalarInterp,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeScalarInterp();
};

///////////////////////////////////////

class VisualShaderNodeVectorInterp : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorInterp,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorInterp();
};

///////////////////////////////////////

class VisualShaderNodeVectorScalarMix : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorScalarMix,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorScalarMix();
};

///////////////////////////////////////
/// COMPOSE
///////////////////////////////////////

class VisualShaderNodeVectorCompose : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorCompose,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorCompose();
};

///////////////////////////////////////

class VisualShaderNodeTransformCompose : public VisualShaderNode {
    GDCLASS(VisualShaderNodeTransformCompose,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeTransformCompose();
};

///////////////////////////////////////
/// DECOMPOSE
///////////////////////////////////////

class VisualShaderNodeVectorDecompose : public VisualShaderNode {
    GDCLASS(VisualShaderNodeVectorDecompose,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeVectorDecompose();
};

///////////////////////////////////////

class VisualShaderNodeTransformDecompose : public VisualShaderNode {
    GDCLASS(VisualShaderNodeTransformDecompose,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeTransformDecompose();
};

///////////////////////////////////////
/// UNIFORMS
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeScalarUniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeScalarUniform,VisualShaderNodeUniform)


public:
    enum Hint {
        HINT_NONE,
        HINT_RANGE,
        HINT_RANGE_STEP,
        HINT_MAX
    };

private:
    Hint hint = HINT_NONE;
    float hint_range_min=0.0f;
    float hint_range_max=1.0f;
    float hint_range_step=0.1f;
    float default_value=0.0f;
    bool default_value_enabled=false;
protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_hint(Hint p_hint);
    Hint get_hint() const;

    void set_min(float p_value);
    float get_min() const;

    void set_max(float p_value);
    float get_max() const;

    void set_step(float p_value);
    float get_step() const;

    void set_default_value_enabled(bool p_enabled);
    bool is_default_value_enabled() const;

    void set_default_value(float p_value);
    float get_default_value() const;
    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeScalarUniform();
};

///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeBooleanUniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeBooleanUniform,VisualShaderNodeUniform)

private:
    bool default_value_enabled=false;
    bool default_value=false;

protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override { return 0; }
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty
    void set_default_value_enabled(bool p_enabled);
    bool is_default_value_enabled() const;

    void set_default_value(bool p_value);
    bool get_default_value() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeBooleanUniform();
};

///////////////////////////////////////

class GODOT_EXPORT  VisualShaderNodeColorUniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeColorUniform,VisualShaderNodeUniform)

private:
    bool default_value_enabled=false;
    Color default_value = Color(1,1,1,1);

protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_default_value_enabled(bool p_enabled);
    bool is_default_value_enabled() const;

    void set_default_value(const Color &p_value);
    Color get_default_value() const;

    Vector<StringName> get_editable_properties() const override;
    VisualShaderNodeColorUniform();
};

///////////////////////////////////////

class GODOT_EXPORT  VisualShaderNodeVec3Uniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeVec3Uniform,VisualShaderNodeUniform)

private:
    bool default_value_enabled=false;
    Vector3 default_value;

protected:
    static void _bind_methods();

public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

	void set_default_value_enabled(bool p_enabled);
    bool is_default_value_enabled() const;

    void set_default_value(const Vector3 &p_value);
    Vector3 get_default_value() const;

    Vector<StringName> get_editable_properties() const override;
    VisualShaderNodeVec3Uniform();
};

///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeTransformUniform : public VisualShaderNodeUniform {
    GDCLASS(VisualShaderNodeTransformUniform,VisualShaderNodeUniform)
private:
    bool default_value_enabled = false;
    Transform default_value = Transform(1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0);

protected:
    static void _bind_methods();


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty
    void set_default_value_enabled(bool p_enabled);
    bool is_default_value_enabled() const;

    void set_default_value(const Transform &p_value);
    Transform get_default_value() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeTransformUniform();
};

///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeTextureUniform : public VisualShaderNodeUniform {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;
    StringName get_input_port_default_hint(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_global(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty
    bool is_code_generated() const override;
    bool is_show_prop_names() const override { return false; }

    Vector<StringName> get_editable_properties() const override;

    void set_texture_type(TextureType p_type);
    TextureType get_texture_type() const;

    void set_color_default(ColorDefault p_default);
    ColorDefault get_color_default() const;

    VisualShaderNodeTextureUniform();
};


///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeTextureUniformTriplanar : public VisualShaderNodeTextureUniform {
    GDCLASS(VisualShaderNodeTextureUniformTriplanar,VisualShaderNodeTextureUniform)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    StringName get_input_port_default_hint(int p_port) const override;

    String generate_global_per_node(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_global_per_func(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeTextureUniformTriplanar();
};

///////////////////////////////////////

class VisualShaderNodeCubeMapUniform : public VisualShaderNodeTextureUniform {
    GDCLASS(VisualShaderNodeCubeMapUniform,VisualShaderNodeTextureUniform)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    StringName get_input_port_default_hint(int p_port) const override;
    String generate_global(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;
    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    VisualShaderNodeCubeMapUniform();
};

///////////////////////////////////////
/// IF
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeIf : public VisualShaderNode {
    GDCLASS(VisualShaderNodeIf,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

    VisualShaderNodeIf();
};

///////////////////////////////////////
/// SWITCH
///////////////////////////////////////

class VisualShaderNodeSwitch : public VisualShaderNode {
    GDCLASS(VisualShaderNodeSwitch,VisualShaderNode)


public:
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

    VisualShaderNodeSwitch();
};

class VisualShaderNodeScalarSwitch : public VisualShaderNodeSwitch {
    GDCLASS(VisualShaderNodeScalarSwitch, VisualShaderNodeSwitch)

public:
    StringView get_caption() const override;

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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;
    StringName get_input_port_default_hint(int p_port) const override;
    bool is_generate_input_var(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

    VisualShaderNodeFresnel();
};

///////////////////////////////////////
/// Is
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeIs : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_function(Function p_func);
    Function get_function() const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeIs();
};

///////////////////////////////////////
/// Compare
///////////////////////////////////////

class GODOT_EXPORT VisualShaderNodeCompare : public VisualShaderNode {
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
    StringView get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    String generate_code(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    void set_comparison_type(ComparisonType p_type);
    ComparisonType get_comparison_type() const;

    void set_function(Function p_func);
    Function get_function() const;

    void set_condition(Condition p_cond);
    Condition get_condition() const;

    Vector<StringName> get_editable_properties() const override;
    StringName get_warning(RenderingServerEnums::ShaderMode p_mode, VisualShader::Type p_type) const override;

    VisualShaderNodeCompare();
};
